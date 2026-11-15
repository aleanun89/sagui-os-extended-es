// SPDX-License-Identifier: GPL-3.0-only
/* rede.c - a pilha de rede: Ethernet, ARP, IPv4, ICMP (ping), UDP, DHCP e
 * DNS. O TCP fica em tcp.c e os sockets em socket.c. Ver D046.
 *
 * Base: a pilha do PR #3 do repositório original (M4R4G0N), estendida com
 * interrupções, cache ARP com validade e fila, DHCP com renovação e
 * configuração estática, cache de DNS, ping com tempo em microssegundos,
 * UDP com soma de verificação e sockets.
 *
 * Tudo roda com as interrupções desligadas: na interrupção da placa, na
 * tarefa da rede (que também consulta a placa a cada tique, para o caso de
 * a IRQ não funcionar) e dentro das chamadas de sistema (que já entram com
 * elas desligadas). Não há duas partes mexendo no mesmo estado ao mesmo
 * tempo, e não precisa de travas.
 *
 * Os endereços IP ficam como uint32_t na ordem "humana" (10.0.2.15 =
 * 0x0A00020F); a conversão para a ordem da rede é feita byte a byte, com
 * be16/be32/poe16/poe32, na hora de ler ou montar um pacote.
 *
 * Robustez: todo comprimento que vem de um pacote é conferido contra o que
 * de fato chegou antes de ser usado; nenhum laço depende de um campo do
 * pacote sem um limite. */
#include "irq_rotas.h"
#include "rede.h"
#include "abi.h"
#include "console.h"
#include "cpu.h"
#include "info_hardware.h"
#include "memoria.h"
#include "processo.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"
#include "aleatorio.h"
#include "bloqueo.h"
#include "wifi.h"

#define ETH_IP   0x0800
#define ETH_ARP  0x0806

#define PORTA_DHCP_SERVIDOR 67
#define PORTA_DHCP_CLIENTE  68
#define PORTA_DNS           53

static placa_t placa;
static bool tem_placa;          /* placa com driver ligado */
static bool so_detectada;       /* placa conhecida mas sem driver ativo (RTL810x) */
static bool usa_irq;
static uint32_t irq_alheias;    /* interrupções na linha que não eram da placa, no último segundo */
static uint32_t meu_ip, mascara, roteador, servidor_dns;
static uint32_t metodo = REDE_SEM_IP;

/* D079: eth0 desligada a pedido (REDE_DESATIVAR): a placa continua ligada,
 * mas a pilha não manda nada e joga fora o que chegar. Vale para qualquer
 * driver, porque é feito aqui e não na placa. */
static bool desativada;

/* D215: la wlan0 (pincho Wi-Fi). Mientras está conectada es la interfaz
 * activa («placa») y la eth0 espera aquí, aparcada. */
static placa_t *wlan;
static placa_t placa_eth;
static bool eth_tinha, wifi_ativa;

/* Loopback (D079): os pacotes para 127.0.0.0/8 ou para o próprio endereço
 * esperam nesta fila e entram de novo pela tarefa da rede, como se tivessem
 * chegado. Assim nenhuma chegada roda dentro de um envio (o TCP responde a
 * um segmento mandando outro). Fila cheia: o pacote se perde e o TCP
 * reenvia, como numa rede de verdade. */
#define LO_FILA 32
#define LO_IP   0x7F000001u
static struct { uint32_t n; uint8_t p[ETH_MAX - 14]; } lo_fila[LO_FILA];
static uint32_t lo_ini, lo_qtd;
static uint32_t lo_pacotes, lo_bytes, lo_descartados;
static uint8_t lo_atual[ETH_MAX - 14];
static const char canal_rede = 0;     /* a tarefa da rede dorme aqui */
static bool tarefa_ligada;

/* Tráfego da eth0 por segundo (INFO_TRAFEGO): uma subtração por segundo na
 * tarefa da rede, para o gráfico do Administrador já abrir com o último minuto. */
static uint32_t trafego_rx[TRAFEGO_AMOSTRAS], trafego_tx[TRAFEGO_AMOSTRAS], trafego_qtd;
static uint32_t trafego_rx_ant, trafego_tx_ant;

static uint8_t quadro[ETH_MAX];       /* o pacote sendo montado para envio */
static uint8_t chegada[ETH_MAX + 2];  /* o quadro que acabou de chegar */
static const uint8_t difusao[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* D091: por que a pilha descartou cada quadro recebido (o total continua
 * em placa.descartados; o que não tem motivo aqui, como a fila UDP cheia
 * em socket.c, aparece como "otros"). Os primeiros vão para o log. */
enum { DESC_MULTICAST, DESC_MAC_ALHEIA, DESC_IPV6, DESC_VLAN, DESC_TIPO, DESC_FRAGMENTO, DESC_FALSO_127,
       DESC_SEM_ARP, DESC_DESATIVADA, DESC_MOTIVOS };
static const char *const nome_descarte[DESC_MOTIVOS] = {
    "multicast", "MAC ajena", "IPv6", "VLAN", "otro tipo", "IP fragmentado", "origen 127 falso",
    "sin respuesta ARP", "interfaz desactivada",
};
static uint32_t descartes[DESC_MOTIVOS];
static uint16_t descarte_ultimo_tipo;
#define DESC_GUARDADOS 4
#define DESC_LOG_MAX   16
static struct { uint8_t motivo, destino[6]; uint16_t tipo, n; } descarte_quadros[DESC_GUARDADOS];
static uint32_t descartes_registrados;

/* q = o quadro Ethernet inteiro (NULL quando o descarte não vem de um quadro). */
static void descartar(int motivo, const uint8_t *q, uint32_t n)
{
    placa.descartados++;
    descartes[motivo]++;
    if (!q || descartes_registrados >= DESC_LOG_MAX) return;
    uint16_t tipo = be16(q + 12);
    if (descartes_registrados < DESC_GUARDADOS) {
        descarte_quadros[descartes_registrados].motivo = (uint8_t)motivo;
        memcpy(descarte_quadros[descartes_registrados].destino, q, 6);
        descarte_quadros[descartes_registrados].tipo = tipo;
        descarte_quadros[descartes_registrados].n = (uint16_t)n;
    }
    descartes_registrados++;
    klog("[REDE] descarte %u (%s): destino %02x:%02x:%02x:%02x:%02x:%02x origen %02x:%02x:%02x:%02x:%02x:%02x tipo %04x, %u bytes\n",
         descartes_registrados, nome_descarte[motivo], q[0], q[1], q[2], q[3], q[4], q[5], q[6], q[7], q[8], q[9],
         q[10], q[11], tipo, n);
}

placa_t *rede_placa(void) { return tem_placa ? &placa : NULL; }
bool rede_pronta(void) { return tem_placa && meu_ip; }
uint32_t rede_meu_ip(void) { return meu_ip; }
uint32_t rede_origem_para(uint32_t destino) { return (destino >> 24) == 127 ? LO_IP : meu_ip; }
bool rede_eh_local(uint32_t ip) { return (ip >> 24) == 127 || (meu_ip && ip == meu_ip); }
uint8_t *rede_carga(void) { return quadro + 14 + 20; }
static uint32_t agora(void) { return relogio_tiques(); }
static bool passou(uint32_t t) { return (int32_t)(agora() - t) >= 0; }

uint32_t soma_parcial(uint32_t s, const uint8_t *p, uint32_t n)
{
    for (; n > 1; p += 2, n -= 2) s += (uint32_t)(p[0] << 8 | p[1]);
    if (n) s += (uint32_t)p[0] << 8;
    return s;
}

uint16_t soma_final(uint32_t s)
{
    while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
    return (uint16_t)~s;
}

/* Soma do TCP/UDP: o "pseudo-cabeçalho" com os IPs vem antes do segmento. */
uint16_t soma_pseudo(uint32_t origem, uint32_t destino, uint8_t proto, const uint8_t *p, uint32_t n)
{
    uint8_t ps[12];
    poe32(ps, origem);
    poe32(ps + 4, destino);
    ps[8] = 0; ps[9] = proto;
    poe16(ps + 10, (uint16_t)n);
    return soma_final(soma_parcial(soma_parcial(0, ps, 12), p, n));
}

/* ------------------------------------------------------------ tempo fino (ping) */

static inline uint64_t ler_tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t div64(uint64_t n, uint32_t d)
{
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= 1ull << i; }
    }
    return q;
}

uint32_t rede_tsc_khz(void) { return hw_tsc_khz(); }

/* ------------------------------------------------------------ Ethernet */

static bool quadro_enviar(const uint8_t *q, uint32_t n)
{
    if (!tem_placa || desativada) return false;
    if (!placa.enviar(q, n)) { placa.tx_erros++; return false; }
    placa.tx_pacotes++;
    placa.tx_bytes += n;
    return true;
}

static void eth_enviar(const uint8_t *destino, uint16_t tipo, uint8_t *q, uint32_t n)
{
    memcpy(q, destino, 6);
    memcpy(q + 6, placa.mac, 6);
    poe16(q + 12, tipo);
    quadro_enviar(q, 14 + n);
}

/* ------------------------------------------------------------ ARP
 * Cache de 32 entradas. Uma entrada "completa" vale 10 minutos; uma
 * "incompleta" (pergunta feita, sem resposta) repete a pergunta a cada
 * segundo, três vezes, e então desiste e descarta o que esperava. Até 4
 * pacotes esperam a resposta (quase sempre é o do roteador, e só uma vez). */

#define ARP_MAX       32
#define ARP_VALIDADE  (600 * TIQUES_POR_SEGUNDO)
#define ARP_PENDENTES 4
enum { ARP_LIVRE, ARP_INCOMPLETA, ARP_COMPLETA };
static struct { uint32_t ip; uint8_t mac[6]; uint8_t estado, tentativas; uint32_t quando; } arp[ARP_MAX];
static struct { uint32_t ip, n; uint8_t q[ETH_MAX]; } pendentes[ARP_PENDENTES];

static int arp_indice(uint32_t ip)
{
    for (int i = 0; i < ARP_MAX; i++)
        if (arp[i].estado != ARP_LIVRE && arp[i].ip == ip) return i;
    return -1;
}

static int arp_vaga(void)
{
    int k = 0;
    for (int i = 0; i < ARP_MAX; i++) {
        if (arp[i].estado == ARP_LIVRE) return i;
        if ((int32_t)(arp[i].quando - arp[k].quando) < 0) k = i;   /* a mais antiga dá lugar */
    }
    return k;
}

static void arp_enviar(uint16_t operacao, const uint8_t *mac_alvo, uint32_t ip_alvo)
{
    uint8_t q[14 + 28];
    uint8_t *a = q + 14;
    poe16(a, 1);                 /* Ethernet */
    poe16(a + 2, ETH_IP);
    a[4] = 6; a[5] = 4;
    poe16(a + 6, operacao);      /* 1 pergunta, 2 resposta */
    memcpy(a + 8, placa.mac, 6);
    poe32(a + 14, meu_ip);
    if (operacao == 1) memset(a + 18, 0, 6); else memcpy(a + 18, mac_alvo, 6);
    poe32(a + 24, ip_alvo);
    eth_enviar(operacao == 1 ? difusao : mac_alvo, ETH_ARP, q, 28);
}

static void arp_guardar(uint32_t ip, const uint8_t *mac)
{
    int k = arp_indice(ip);
    if (k < 0) k = arp_vaga();
    arp[k].ip = ip;
    memcpy(arp[k].mac, mac, 6);
    arp[k].estado = ARP_COMPLETA;
    arp[k].quando = agora();
    for (int i = 0; i < ARP_PENDENTES; i++)
        if (pendentes[i].n && pendentes[i].ip == ip) {
            memcpy(pendentes[i].q, mac, 6);
            quadro_enviar(pendentes[i].q, pendentes[i].n);
            pendentes[i].n = 0;
        }
}

static void arp_chegou(const uint8_t *a, uint32_t n)
{
    if (n < 28 || be16(a) != 1 || be16(a + 2) != ETH_IP || a[4] != 6 || a[5] != 4) return;
    uint32_t ip_origem = be32(a + 14), ip_alvo = be32(a + 24);
    uint16_t op = be16(a + 6);
    if (ip_origem == 0 || ip_origem == IP_TODOS || (a[8] & 1)) return;   /* sondas e MAC de grupo: ignora */
    if (meu_ip && ip_origem == meu_ip) {
        klog("[REDE] ARP: outro computador usa o nosso IP\n");
        return;
    }
    if (!meu_ip || ip_alvo != meu_ip) {
        if (arp_indice(ip_origem) >= 0) arp_guardar(ip_origem, a + 8);   /* atualiza quem já conhecemos */
        return;
    }
    arp_guardar(ip_origem, a + 8);
    if (op == 1) arp_enviar(2, a + 8, ip_origem);
}

static void arp_tique(void)
{
    for (int i = 0; i < ARP_MAX; i++) {
        if (arp[i].estado == ARP_COMPLETA && passou(arp[i].quando + ARP_VALIDADE)) arp[i].estado = ARP_LIVRE;
        if (arp[i].estado != ARP_INCOMPLETA || !passou(arp[i].quando + TIQUES_POR_SEGUNDO)) continue;
        if (++arp[i].tentativas >= 3) {
            arp[i].estado = ARP_LIVRE;   /* ninguém respondeu: descarta o que esperava */
            for (int k = 0; k < ARP_PENDENTES; k++)
                if (pendentes[k].n && pendentes[k].ip == arp[i].ip) { pendentes[k].n = 0; descartar(DESC_SEM_ARP, NULL, 0); }
            continue;
        }
        arp[i].quando = agora();
        arp_enviar(1, NULL, arp[i].ip);
    }
}

/* ------------------------------------------------------------ IPv4 */

static bool mesma_rede(uint32_t ip) { return ((ip ^ meu_ip) & mascara) == 0; }

/* O conteúdo (n bytes) já está em rede_carga(). */
static bool lo_enfileirar(const uint8_t *pacote, uint32_t n)
{
    if (lo_qtd == LO_FILA) { lo_descartados++; return false; }
    uint32_t k = (lo_ini + lo_qtd) % LO_FILA;
    memcpy(lo_fila[k].p, pacote, n);
    lo_fila[k].n = n;
    lo_qtd++;
    lo_pacotes++;
    lo_bytes += n;
    acordar(&canal_rede);
    return true;
}

static void ip_chegou(const uint8_t *h, uint32_t n, bool do_lo);

static void lo_processar(void)
{
    for (int k = 0; k < LO_FILA && lo_qtd; k++) {
        uint32_t n = lo_fila[lo_ini].n;
        memcpy(lo_atual, lo_fila[lo_ini].p, n);   /* a vaga fica livre para as respostas */
        lo_ini = (lo_ini + 1) % LO_FILA;
        lo_qtd--;
        ip_chegou(lo_atual, n, true);
    }
}

bool ip_enviar(uint32_t destino, uint8_t protocolo, uint32_t n)
{
    static uint16_t ident;
    bool local = rede_eh_local(destino);
    if ((!tem_placa && !local) || n > ETH_MAX - 34) return false;
    uint8_t *h = quadro + 14;
    h[0] = 0x45; h[1] = 0;
    poe16(h + 2, (uint16_t)(20 + n));
    poe16(h + 4, ident++);
    poe16(h + 6, 0x4000);        /* não fragmentar: o TCP nunca passa do MSS */
    h[8] = 64; h[9] = protocolo;
    poe16(h + 10, 0);
    poe32(h + 12, rede_origem_para(destino));
    poe32(h + 16, destino);
    poe16(h + 10, soma_final(soma_parcial(0, h, 20)));
    if (local) return lo_enfileirar(h, 20 + n);

    if (destino == IP_TODOS || (meu_ip && mascara && destino == (meu_ip | ~mascara))) {
        eth_enviar(difusao, ETH_IP, quadro, 20 + n);
        return true;
    }
    if (!meu_ip) return false;
    uint32_t salto = mesma_rede(destino) ? destino : roteador;
    if (!salto) return false;    /* fora da rede e sem roteador */
    int k = arp_indice(salto);
    if (k >= 0 && arp[k].estado == ARP_COMPLETA) {
        eth_enviar(arp[k].mac, ETH_IP, quadro, 20 + n);
        return true;
    }
    /* Ainda não sabemos o MAC: guarda o pacote e pergunta. */
    int v = (int)(ident % ARP_PENDENTES);   /* fila cheia: um qualquer dá lugar (o TCP reenvia) */
    for (int i = 0; i < ARP_PENDENTES; i++)
        if (!pendentes[i].n) { v = i; break; }
    memcpy(quadro + 6, placa.mac, 6);
    poe16(quadro + 12, ETH_IP);
    pendentes[v].ip = salto;
    pendentes[v].n = 14 + 20 + n;
    memcpy(pendentes[v].q, quadro, pendentes[v].n);
    if (k < 0) {
        k = arp_vaga();
        arp[k].ip = salto;
        arp[k].estado = ARP_INCOMPLETA;
        arp[k].tentativas = 0;
        arp[k].quando = agora();
        arp_enviar(1, NULL, salto);
    }
    return true;
}

/* ------------------------------------------------------------ esperas
 * Ping, DNS e DHCP seguem o padrão das outras chamadas que bloqueiam (D010):
 * a primeira execução manda o pedido e bloqueia com prazo; a chamada roda
 * de novo ao acordar e então vê se a resposta chegou. Cada processo tem no
 * máximo uma espera de cada tipo; a de um processo que morreu é reciclada. */

enum { ESP_PING = 1, ESP_DNS, ESP_DHCP };
#define ESPERAS 16
typedef struct {
    int      pid, tipo;
    uint16_t id, porta;
    uint32_t prazo, reenviar;
    uint32_t ip;                  /* ping: destino; DNS: resposta */
    uint32_t servidor, porta_srv; /* DNS */
    uint32_t ttl, tentativas;
    uint64_t tsc_ini, tsc_fim;
    uint32_t tique_ini, tique_fim;
    int      resultado;           /* 1 = chegou; <0 = erro */
    char     nome[256];
} espera_t;
static espera_t esperas[ESPERAS];

static espera_t *espera_minha(int tipo)
{
    for (int i = 0; i < ESPERAS; i++)
        if (esperas[i].tipo == tipo && esperas[i].pid == processo_atual()->pid) return &esperas[i];
    return NULL;
}

static espera_t *espera_nova(int tipo, uint32_t prazo_ms)
{
    static uint16_t proximo_id = 0x5A00;
    for (int i = 0; i < ESPERAS; i++) {
        espera_t *e = &esperas[i];
        if (e->tipo && processo_por_pid(e->pid)) continue;   /* em uso (a de um processo que morreu fica livre) */
        memset(e, 0, sizeof *e);
        e->pid = processo_atual()->pid;
        e->tipo = tipo;
        e->id = (uint16_t)(++proximo_id ^ aleatorio_u32());   /* ID do DNS imprevisível (D061) */
        e->prazo = agora() + (prazo_ms * TIQUES_POR_SEGUNDO + 999) / 1000;
        return e;
    }
    return NULL;
}

/* Bloqueia até a resposta ou o prazo; devolve SIS_REPETIR, ou o resultado. */
static int32_t espera_continuar(espera_t *e, int32_t ok)
{
    if (e->resultado || passou(e->prazo)) {
        int32_t r = e->resultado > 0 ? ok : e->resultado ? e->resultado : ERRO_TEMPO;
        e->tipo = 0;
        processo_atual()->acordar_em = 0;
        return r;
    }
    uint32_t ate = e->reenviar && (int32_t)(e->reenviar - e->prazo) < 0 ? e->reenviar : e->prazo;
    bloquear(e);
    processo_atual()->acordar_em = ate;
    return SIS_REPETIR;
}

/* ------------------------------------------------------------ ICMP */

static void eco_enviar(espera_t *e, uint16_t seq, uint32_t dados)
{
    uint8_t *c = rede_carga();
    c[0] = 8; c[1] = 0;
    poe16(c + 2, 0);
    poe16(c + 4, e->id);
    poe16(c + 6, seq);
    for (uint32_t i = 0; i < dados; i++) c[8 + i] = (uint8_t)(0x10 + i);
    poe16(c + 2, soma_final(soma_parcial(0, c, 8 + dados)));
    e->tique_ini = agora();
    e->tsc_ini = hw_tsc_khz() ? ler_tsc() : 0;
    if (!ip_enviar(e->ip, IP_ICMP, 8 + dados)) e->resultado = ERRO_INALCANCAVEL;
}

static void icmp_chegou(uint32_t origem, uint32_t destino, uint8_t ttl, const uint8_t *c, uint32_t n)
{
    if (n < 8 || soma_final(soma_parcial(0, c, n)) != 0) return;
    if (c[0] == 8 && c[1] == 0 && (destino == meu_ip || (destino >> 24) == 127) && n <= ETH_MAX - 34) {   /* alguém pingou o SaguiOS */
        uint8_t *r = rede_carga();
        memmove(r, c, n);
        r[0] = 0;
        poe16(r + 2, 0);
        poe16(r + 2, soma_final(soma_parcial(0, r, n)));
        ip_enviar(origem, IP_ICMP, n);
    } else if (c[0] == 0) {
        for (int i = 0; i < ESPERAS; i++) {
            espera_t *e = &esperas[i];
            if (e->tipo == ESP_PING && e->id == be16(c + 4) && e->ip == origem && !e->resultado) {
                e->tsc_fim = e->tsc_ini ? ler_tsc() : 0;
                e->tique_fim = agora();
                e->ttl = ttl;
                e->resultado = 1;
                acordar(e);
            }
        }
    }
}

/* SIS_PING (ip, &info_ping_t, prazo_ms) */
int32_t sis_ping(uint32_t ip, uint32_t pinfo, uint32_t prazo_ms, uint32_t d, uint32_t e_)
{
    (void)d; (void)e_;
    info_ping_t *info = (info_ping_t *)pinfo;
    if (!usuario_ok(info, sizeof *info, true)) return ERRO_ENDERECO;
    espera_t *e = espera_minha(ESP_PING);
    if (!e) {
        if (!rede_pronta()) return ERRO_SEM_REDE;
        if (ip == 0 || ip == IP_TODOS) return ERRO_INVALIDO;
        uint32_t dados = info->tamanho ? info->tamanho : 56;
        if (dados > 1400) return ERRO_INVALIDO;
        if (prazo_ms < 100 || prazo_ms > 30000) prazo_ms = 2000;
        e = espera_nova(ESP_PING, prazo_ms);
        if (!e) return ERRO_LIMITE;
        e->ip = ip;
        eco_enviar(e, (uint16_t)info->seq, dados);
    }
    int32_t r = espera_continuar(e, 0);
    if (r == 0) {
        uint32_t khz = hw_tsc_khz();
        if (khz && e->tsc_fim > e->tsc_ini) info->us = (uint32_t)div64((e->tsc_fim - e->tsc_ini) * 1000u, khz);
        else info->us = (e->tique_fim - e->tique_ini) * (1000000 / TIQUES_POR_SEGUNDO);
        info->ttl = e->ttl;
    }
    return r;
}

/* ------------------------------------------------------------ UDP */

/* Os dados já estão em rede_carga() + 8. */
bool udp_enviar(uint32_t destino, uint16_t porta_origem, uint16_t porta_destino, uint32_t n)
{
    uint8_t *u = rede_carga();
    if (n > ETH_MAX - 34 - 8) return false;
    poe16(u, porta_origem);
    poe16(u + 2, porta_destino);
    poe16(u + 4, (uint16_t)(8 + n));
    poe16(u + 6, 0);
    uint16_t s = soma_pseudo(rede_origem_para(destino), destino, IP_UDP, u, 8 + n);
    poe16(u + 6, s ? s : 0xFFFF);   /* 0 quer dizer "sem soma" no UDP */
    return ip_enviar(destino, IP_UDP, 8 + n);
}

/* ------------------------------------------------------------ DHCP */

enum { DHCP_PARADO, DHCP_DESCOBRINDO, DHCP_PEDINDO, DHCP_PRONTO, DHCP_RENOVANDO };
static int dhcp_estado;
static uint32_t dhcp_xid, dhcp_oferta, servidor_dhcp, dhcp_proxima, dhcp_tentativas;
static uint32_t dhcp_inicio_concessao, dhcp_concessao;   /* tiques; concessão em segundos */

bool rede_dhcp_terminou(void) { return dhcp_estado == DHCP_PRONTO || metodo == REDE_ESTATICO; }

static uint8_t *dhcp_montar(uint8_t tipo)
{
    uint8_t *b = rede_carga() + 8;
    memset(b, 0, 240);
    b[0] = 1; b[1] = 1; b[2] = 6;             /* pedido, Ethernet, MAC de 6 bytes */
    poe32(b + 4, dhcp_xid);
    if (dhcp_estado == DHCP_RENOVANDO) poe32(b + 12, meu_ip);   /* ciaddr */
    else poe16(b + 10, 0x8000);               /* responda por difusão: ainda não temos IP */
    memcpy(b + 28, placa.mac, 6);
    poe32(b + 236, 0x63825363);               /* "magic cookie" */
    uint8_t *o = b + 240;
    *o++ = 53; *o++ = 1; *o++ = tipo;
    *o++ = 12; *o++ = 7; memcpy(o, "saguios", 7); o += 7;   /* nome do computador */
    return o;
}

static void dhcp_enviar(uint8_t *fim)
{
    *fim++ = 55; *fim++ = 3; *fim++ = 1; *fim++ = 3; *fim++ = 6;   /* queremos máscara, roteador e DNS */
    *fim++ = 255;
    uint32_t n = (uint32_t)(fim - (rede_carga() + 8));
    if (n < 300) { memset(fim, 0, 300 - n); n = 300; }   /* alguns servidores ignoram pedidos curtos */
    udp_enviar(IP_TODOS, PORTA_DHCP_CLIENTE, PORTA_DHCP_SERVIDOR, n);
}

static void dhcp_descobrir(void)
{
    dhcp_xid = 0x5A600000u ^ (agora() * 2654435761u) ^ ((uint32_t)placa.mac[5] << 8);
    dhcp_estado = DHCP_DESCOBRINDO;
    uint32_t espera = 2u << (dhcp_tentativas < 3 ? dhcp_tentativas : 3);   /* 2, 4, 8, 16 s */
    dhcp_tentativas++;
    dhcp_proxima = agora() + espera * TIQUES_POR_SEGUNDO;
    dhcp_enviar(dhcp_montar(1));
}

static void dhcp_pedir(void)
{
    uint8_t *o = dhcp_montar(3);
    if (dhcp_estado == DHCP_PEDINDO) {
        *o++ = 50; *o++ = 4; poe32(o, dhcp_oferta); o += 4;
        *o++ = 54; *o++ = 4; poe32(o, servidor_dhcp); o += 4;
    }
    dhcp_enviar(o);
}

static void avisar_dhcp(int resultado)
{
    for (int i = 0; i < ESPERAS; i++)
        if (esperas[i].tipo == ESP_DHCP && !esperas[i].resultado) {
            esperas[i].resultado = resultado;
            acordar(&esperas[i]);
        }
}

static void ip_texto(char *s, int max, uint32_t ip)
{
    snformatar(s, (size_t)max, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
}

static void dhcp_chegou(const uint8_t *b, uint32_t n)
{
    if (n < 240 || b[0] != 2 || be32(b + 4) != dhcp_xid || be32(b + 236) != 0x63825363) return;
    if (memcmp(b + 28, placa.mac, 6) != 0) return;
    uint8_t tipo = 0;
    uint32_t msk = 0, rot = 0, dns = 0, srv = 0, conc = 0;
    for (uint32_t i = 240; i < n && b[i] != 255;) {
        if (b[i] == 0) { i++; continue; }
        if (i + 1 >= n || i + 2 + b[i + 1] > n) break;
        const uint8_t *v = b + i + 2;
        uint8_t t = b[i + 1];
        if (b[i] == 53 && t >= 1) tipo = v[0];
        if (b[i] == 1 && t >= 4) msk = be32(v);
        if (b[i] == 3 && t >= 4) rot = be32(v);
        if (b[i] == 6 && t >= 4) dns = be32(v);
        if (b[i] == 54 && t >= 4) srv = be32(v);
        if (b[i] == 51 && t >= 4) conc = be32(v);
        i += 2u + t;
    }
    uint32_t oferta = be32(b + 16);
    if (tipo == 2 && dhcp_estado == DHCP_DESCOBRINDO) {       /* oferta: pede esse endereço */
        if (oferta == 0 || oferta == IP_TODOS || (oferta >> 24) == 127 || (oferta >> 28) >= 14) return;
        dhcp_oferta = oferta;
        servidor_dhcp = srv;
        dhcp_estado = DHCP_PEDINDO;
        dhcp_tentativas = 0;
        dhcp_proxima = agora() + 2 * TIQUES_POR_SEGUNDO;
        dhcp_pedir();
    } else if (tipo == 5 && (dhcp_estado == DHCP_PEDINDO || dhcp_estado == DHCP_RENOVANDO)) {   /* confirmado */
        if (oferta == 0 || oferta == IP_TODOS) return;
        bool novo = meu_ip != oferta;
        meu_ip = oferta;
        mascara = msk ? msk : 0xFFFFFF00u;
        roteador = rot;
        servidor_dns = dns ? dns : rot;
        if (srv) servidor_dhcp = srv;
        dhcp_concessao = conc ? conc : 3600;
        if (dhcp_concessao < 60) dhcp_concessao = 60;
        if (dhcp_concessao > 7 * 24 * 3600) dhcp_concessao = 7 * 24 * 3600;
        dhcp_inicio_concessao = agora();
        dhcp_estado = DHCP_PRONTO;
        dhcp_tentativas = 0;
        metodo = REDE_POR_DHCP;
        if (novo) {
            char a[16], r[16], d[16];
            ip_texto(a, sizeof a, meu_ip); ip_texto(r, sizeof r, roteador); ip_texto(d, sizeof d, servidor_dns);
            klog("[REDE] DHCP: ip %s roteador %s dns %s concessao %u s\n", a, r, d, dhcp_concessao);
            arp_enviar(1, NULL, meu_ip);              /* anuncia o endereço (ARP gratuito) */
            if (roteador) {                           /* já descobre o MAC do roteador */
                int k = arp_indice(roteador);
                if (k < 0) {
                    k = arp_vaga();
                    arp[k].ip = roteador; arp[k].estado = ARP_INCOMPLETA; arp[k].tentativas = 0; arp[k].quando = agora();
                }
                arp_enviar(1, NULL, roteador);
            }
        }
        else klog("[REDE] DHCP: concessao renovada (%u s)\n", dhcp_concessao);
        avisar_dhcp(1);
    } else if (tipo == 6 && dhcp_estado != DHCP_PRONTO) {      /* recusado: começa de novo */
        klog("[REDE] DHCP: o servidor recusou (NAK)\n");
        if (dhcp_estado == DHCP_RENOVANDO) { meu_ip = 0; metodo = REDE_SEM_IP; }
        dhcp_estado = DHCP_PARADO;
    }
}

static void dhcp_tique(void)
{
    if (!tem_placa || desativada || metodo == REDE_ESTATICO) return;
    uint32_t passado = (agora() - dhcp_inicio_concessao) / TIQUES_POR_SEGUNDO;
    switch (dhcp_estado) {
    case DHCP_PRONTO:
        if (passado >= dhcp_concessao / 2) {     /* metade da concessão: renova com o mesmo servidor */
            dhcp_estado = DHCP_RENOVANDO;
            dhcp_xid++;
            dhcp_proxima = agora() + 10 * TIQUES_POR_SEGUNDO;
            dhcp_pedir();
        }
        break;
    case DHCP_RENOVANDO:
        if (passado >= dhcp_concessao) {          /* acabou: perde o endereço e começa do zero */
            klog("[REDE] DHCP: a concessao acabou sem renovacao\n");
            meu_ip = 0;
            metodo = REDE_SEM_IP;
            dhcp_estado = DHCP_PARADO;
        } else if (passou(dhcp_proxima)) {
            dhcp_proxima = agora() + 10 * TIQUES_POR_SEGUNDO;
            dhcp_pedir();
        }
        break;
    case DHCP_PEDINDO:
        if (passou(dhcp_proxima)) {
            if (++dhcp_tentativas >= 3) { dhcp_tentativas = 0; dhcp_descobrir(); }
            else { dhcp_proxima = agora() + 2 * TIQUES_POR_SEGUNDO; dhcp_pedir(); }
        }
        break;
    default:
        if (!placa.cabo()) break;
        if (dhcp_estado == DHCP_PARADO || passou(dhcp_proxima)) dhcp_descobrir();
        break;
    }
}

/* ------------------------------------------------------------ DNS
 * Perguntas do tipo A por UDP ao servidor do DHCP (ou outro, a pedido).
 * Cache de 16 nomes, cada um pelo tempo que o servidor disser (TTL),
 * entre 10 s e 1 hora. */

#define DNS_CACHE 16
static struct { char nome[64]; uint32_t ip, expira; bool usado; } dns_cache[DNS_CACHE];

static bool nome_valido(const char *s)
{
    int rotulo = 0, total = 0;
    for (; *s; s++, total++) {
        char c = *s;
        if (c == '.') { if (rotulo == 0) return false; rotulo = 0; continue; }
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'))
            return false;
        if (++rotulo > 63) return false;
    }
    return total > 0 && total <= 253;
}

static void minusculas(char *s) { for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s + 32); }

static bool dns_no_cache(const char *nome, uint32_t *ip)
{
    for (int i = 0; i < DNS_CACHE; i++)
        if (dns_cache[i].usado && txt_compara(dns_cache[i].nome, nome) == 0) {
            if (passou(dns_cache[i].expira)) { dns_cache[i].usado = false; return false; }
            *ip = dns_cache[i].ip;
            return true;
        }
    return false;
}

static void dns_guardar(const char *nome, uint32_t ip, uint32_t ttl)
{
    if (txt_tamanho(nome) >= sizeof dns_cache[0].nome) return;
    if (ttl < 10) ttl = 10;
    if (ttl > 3600) ttl = 3600;
    int k = 0;
    for (int i = 0; i < DNS_CACHE; i++) {
        if (dns_cache[i].usado && txt_compara(dns_cache[i].nome, nome) == 0) { k = i; break; }
        if (!dns_cache[i].usado) { k = i; break; }
        if ((int32_t)(dns_cache[i].expira - dns_cache[k].expira) < 0) k = i;
    }
    txt_copia_n(dns_cache[k].nome, nome, sizeof dns_cache[k].nome);
    dns_cache[k].ip = ip;
    dns_cache[k].expira = agora() + ttl * TIQUES_POR_SEGUNDO;
    dns_cache[k].usado = true;
}

static void dns_perguntar(espera_t *e)
{
    uint8_t *m = rede_carga() + 8;
    poe16(m, e->id);
    poe16(m + 2, 0x0100);        /* pergunta recursiva */
    poe16(m + 4, 1);
    poe16(m + 6, 0); poe16(m + 8, 0); poe16(m + 10, 0);
    uint8_t *p = m + 12;
    for (const char *s = e->nome; *s;) {     /* "exemplo.com" -> 7 exemplo 3 com 0 */
        const char *ponto = s;
        while (*ponto && *ponto != '.') ponto++;
        uint32_t t = (uint32_t)(ponto - s);
        *p++ = (uint8_t)t;
        memcpy(p, s, t);
        p += t;
        s = *ponto ? ponto + 1 : ponto;
    }
    *p++ = 0;
    poe16(p, 1); poe16(p + 2, 1);            /* tipo A (IPv4), classe IN */
    p += 4;
    if (!udp_enviar(e->servidor, e->porta, (uint16_t)e->porta_srv, (uint32_t)(p - m))) e->resultado = ERRO_INALCANCAVEL;
    e->tentativas++;
    e->reenviar = agora() + TIQUES_POR_SEGUNDO * e->tentativas;   /* 1 s, 2 s... */
}

/* Pula um nome comprimido ou não; devolve a posição depois dele (ou n). */
static uint32_t dns_pular_nome(const uint8_t *m, uint32_t i, uint32_t n)
{
    for (int voltas = 0; i < n && voltas < 128; voltas++) {
        if (m[i] == 0) return i + 1;
        if ((m[i] & 0xC0) == 0xC0) return i + 2 <= n ? i + 2 : n;   /* ponteiro: o nome termina aqui */
        if (m[i] & 0xC0) return n;                                   /* formato reservado: inválido */
        i += 1u + m[i];
    }
    return n;
}

static void dns_chegou(uint32_t origem, uint16_t porta, const uint8_t *m, uint32_t n)
{
    if (n < 12) return;
    espera_t *e = NULL;
    for (int i = 0; i < ESPERAS; i++)
        if (esperas[i].tipo == ESP_DNS && esperas[i].id == be16(m) && esperas[i].porta == porta &&
            esperas[i].servidor == origem) e = &esperas[i];
    if (!e || e->resultado || !(m[2] & 0x80)) return;
    if ((m[3] & 0x0F) != 0) { e->resultado = ERRO_NAO_EXISTE; acordar(e); return; }   /* NXDOMAIN e outros */
    uint32_t i = 12;
    for (uint16_t q = be16(m + 4); q && i < n; q--) i = dns_pular_nome(m, i, n) + 4;
    for (uint16_t a = be16(m + 6); a && i < n; a--) {
        i = dns_pular_nome(m, i, n);
        if (i + 10 > n) break;
        uint16_t tipo = be16(m + i), classe = be16(m + i + 2), t = be16(m + i + 8);
        uint32_t ttl = be32(m + i + 4);
        i += 10;
        if (i + t > n) break;
        if (tipo == 1 && classe == 1 && t == 4) {
            e->ip = be32(m + i);
            e->ttl = ttl;
            e->resultado = 1;
            break;
        }
        i += t;                      /* CNAME e outros: pula */
    }
    if (!e->resultado) e->resultado = ERRO_NAO_EXISTE;   /* o nome não existe ou não tem IPv4 */
    acordar(e);
}

/* SIS_RESOLVER (nome, &ip, servidor ou 0, porta ou 0). Com servidor 0 usa o
 * do DHCP e a cache; com outro servidor pergunta sempre (é o nslookup). */
int32_t sis_resolver(uint32_t pnome, uint32_t pip, uint32_t servidor, uint32_t porta, uint32_t e_)
{
    (void)e_;
    if (!usuario_ok((void *)pip, 4, true)) return ERRO_ENDERECO;
    espera_t *e = espera_minha(ESP_DNS);
    if (!e) {
        char nome[256];
        int r = usuario_texto((const char *)pnome, nome, sizeof nome);
        if (r < 0) return r;
        if (!nome_valido(nome)) return ERRO_INVALIDO;
        if (r && nome[r - 1] == '.') nome[r - 1] = 0;   /* "exemplo.com." */
        minusculas(nome);
        /* D218: el bloqueo de publicidad (si está activo) contesta 0.0.0.0 sin
         * preguntar a nadie, también cuando se pide otro servidor (nslookup). */
        if (bloqueo_filtrar(nome)) { *(uint32_t *)pip = 0; return 0; }
        uint32_t ip;
        if (!servidor && dns_no_cache(nome, &ip)) { *(uint32_t *)pip = ip; return 0; }
        if (!rede_pronta()) return ERRO_SEM_REDE;
        if (!servidor) servidor = servidor_dns;
        if (!servidor) return ERRO_INALCANCAVEL;
        if (porta == 0 || porta > 65535) porta = PORTA_DNS;
        e = espera_nova(ESP_DNS, 5000);
        if (!e) return ERRO_LIMITE;
        txt_copia_n(e->nome, nome, sizeof e->nome);
        e->servidor = servidor;
        e->porta_srv = porta;
        e->porta = (uint16_t)(50000 + (e->id & 0x1FFF));
        dns_perguntar(e);
    } else if (!e->resultado && passou(e->reenviar) && e->tentativas < 4) {
        dns_perguntar(e);            /* UDP pode se perder: pergunta de novo */
    }
    uint32_t ip = e->ip, ttl = e->ttl;
    bool por_padrao = e->servidor == servidor_dns && e->porta_srv == PORTA_DNS;
    char nome[64];
    txt_copia_n(nome, e->nome, sizeof nome);
    int32_t r = espera_continuar(e, 0);
    if (r == 0) {
        *(uint32_t *)pip = ip;
        if (por_padrao) dns_guardar(nome, ip, ttl);
    }
    return r;
}

/* ------------------------------------------------------------ chegada */

static void udp_chegou(uint32_t origem, uint32_t destino, const uint8_t *u, uint32_t n)
{
    if (n < 8) return;
    uint32_t tam = be16(u + 4);
    if (tam < 8 || tam > n) return;
    if (be16(u + 6) && soma_pseudo(origem, destino, IP_UDP, u, tam) != 0) { placa.rx_erros++; return; }
    uint16_t po = be16(u), pd = be16(u + 2);
    if (pd == PORTA_DHCP_CLIENTE && po == PORTA_DHCP_SERVIDOR) { dhcp_chegou(u + 8, tam - 8); return; }
    if ((destino >> 24) != 127 && (!meu_ip || (destino != meu_ip && destino != IP_TODOS && destino != (meu_ip | ~mascara))))
        return;
    if (pd >= 50000 && pd < 50000 + 0x2000) {
        for (int i = 0; i < ESPERAS; i++)
            if (esperas[i].tipo == ESP_DNS && esperas[i].porta == pd) { dns_chegou(origem, pd, u + 8, tam - 8); return; }
    }
    udp_chegou_socket(origem, destino, po, pd, u + 8, tam - 8);
}

static void ip_chegou(const uint8_t *h, uint32_t n, bool do_lo)
{
    if (n < 20 || (h[0] >> 4) != 4) return;
    uint32_t cab = (h[0] & 15) * 4u, total = be16(h + 2);
    if (cab < 20 || total < cab || total > n || soma_final(soma_parcial(0, h, cab)) != 0) { placa.rx_erros++; return; }
    /* Pedaços de pacotes fragmentados são descartados: o TCP anuncia MSS
     * 1460 e ninguém precisa fragmentar para falar com o SaguiOS. */
    if (be16(h + 6) & 0x3FFF) { descartar(DESC_FRAGMENTO, do_lo ? NULL : chegada, n + 14); return; }
    uint32_t origem = be32(h + 12), destino = be32(h + 16);
    bool para_lo = (destino >> 24) == 127;
    /* 127.x vindo do cabo é falsificado (RFC 1122, "martian"): fora. */
    if (!do_lo && (para_lo || (origem >> 24) == 127)) { descartar(DESC_FALSO_127, chegada, n + 14); return; }
    bool difusao_ip = destino == IP_TODOS || (meu_ip && destino == (meu_ip | ~mascara));
    if (meu_ip && destino != meu_ip && !difusao_ip && !para_lo) return;
    if (origem == IP_TODOS || (origem >> 28) == 14) return;   /* origem de difusão ou multicast: inválida */
    const uint8_t *c = h + cab;
    uint32_t t = total - cab;
    if (h[9] == IP_ICMP && (meu_ip || do_lo)) icmp_chegou(origem, destino, h[8], c, t);
    else if (h[9] == IP_UDP) udp_chegou(origem, destino, c, t);
    else if (h[9] == IP_TCP && ((meu_ip && destino == meu_ip) || para_lo)) tcp_receber(origem, destino, c, t);
}

static void processar_chegadas(void)
{
    if (!tem_placa) return;
    for (int k = 0; k < 64; k++) {     /* no máximo um anel por vez, para não segurar o sistema */
        int n = placa.receber(chegada);
        if (n == 0) break;
        if (n < 14 || n > ETH_MAX) { placa.rx_erros++; continue; }
        if (desativada) { descartar(DESC_DESATIVADA, NULL, 0); continue; }   /* desligada: esvazia o anel e joga fora */
        placa.rx_pacotes++;
        placa.rx_bytes += (uint32_t)n;
        /* Só quadros para o nosso MAC ou de difusão (a placa já filtra, mas
         * confere; o multicast a placa deixa passar e morre aqui). */
        if (memcmp(chegada, placa.mac, 6) != 0 && memcmp(chegada, difusao, 6) != 0) {
            descartar((chegada[0] & 1) ? DESC_MULTICAST : DESC_MAC_ALHEIA, chegada, (uint32_t)n);
            continue;
        }
        uint16_t tipo = be16(chegada + 12);
        if (tipo == ETH_ARP) arp_chegou(chegada + 14, (uint32_t)n - 14);
        else if (tipo == ETH_IP) ip_chegou(chegada + 14, (uint32_t)n - 14, false);
        else {
            descarte_ultimo_tipo = tipo;
            descartar(tipo == 0x86DD ? DESC_IPV6 : (tipo == 0x8100 || tipo == 0x88A8) ? DESC_VLAN : DESC_TIPO,
                      chegada, (uint32_t)n);
        }
    }
}

/* ------------------------------------------------------------ interrupção e tarefa */

static quadro_t *ao_irq(quadro_t *q)
{
    /* D215: con la wlan0 activa, la IRQ es de la eth0 aparcada: se atiende
     * (para que la línea no se quede pedida) y lo que trajo se descarta. */
    placa_t *p = wifi_ativa ? &placa_eth : &placa;
    if (!p->atender()) {
        /* Linha compartilhada com algo que ninguém atende: se virar uma
         * tempestade, desiste da IRQ e fica só na sondagem. */
        if (++irq_alheias > 5000 && usa_irq) {
            irq_desabilitar((uint8_t)p->irq);
            usa_irq = false;
            klog("[REDE] IRQ %d sem causa da placa: passando a sondagem\n", p->irq);
        }
        return q;
    }
    p->irqs++;
    if (wifi_ativa) {
        static uint8_t lixo[ETH_MAX + 2];
        for (int k = 0; k < 64 && placa_eth.receber(lixo) != 0; k++) { }
        return q;
    }
    processar_chegadas();
    if (processo_atual() && processo_atual()->pid == 0) return escalonar(q);   /* o ocioso cede logo a quem acordou */
    return q;
}

static void amostrar_trafego(void)
{
    if (trafego_qtd == TRAFEGO_AMOSTRAS) {
        memmove(trafego_rx, trafego_rx + 1, (TRAFEGO_AMOSTRAS - 1) * sizeof trafego_rx[0]);
        memmove(trafego_tx, trafego_tx + 1, (TRAFEGO_AMOSTRAS - 1) * sizeof trafego_tx[0]);
        trafego_qtd--;
    }
    trafego_rx[trafego_qtd] = placa.rx_bytes - trafego_rx_ant;
    trafego_tx[trafego_qtd] = placa.tx_bytes - trafego_tx_ant;
    trafego_qtd++;
    trafego_rx_ant = placa.rx_bytes;
    trafego_tx_ant = placa.tx_bytes;
}

static void tarefa_rede(void *arg)
{
    (void)arg;
    uint32_t proximo_segundo = agora() + TIQUES_POR_SEGUNDO;
    for (;;) {
        int_desliga();
        processar_chegadas();
        lo_processar();
        arp_tique();
        dhcp_tique();
        tcp_tique();
        if (passou(proximo_segundo)) {
            irq_alheias = 0;
            proximo_segundo = agora() + TIQUES_POR_SEGUNDO;
            amostrar_trafego();
        }
        processo_atual()->acordar_em = agora() + 1;
        bloquear(&canal_rede);
        ceder();
        int_liga();
    }
}

static void ligar_tarefa(void)
{
    if (!tarefa_ligada) { tarefa_ligada = true; tarefa_kernel("rede", tarefa_rede, NULL); }
}

/* D200: el driver experimental de la tarjeta detectada (r8169 o alc), para
 * encenderlo a pedido con REDE_EXPERIMENTAL. */
static bool (*procurar_experimental)(placa_t *p, bool ativar);

/* D211: la autoprueba de la IRQ (irq_rotas.c) cuenta las interrupciones
 * de la tarjeta que su driver reconoció. */
static bool sonda_provocar(void *ctx) { (void)ctx; return usa_irq && placa.provocar_irq && placa.provocar_irq(); }
static uint32_t sonda_contador(void *ctx) { (void)ctx; return placa.irqs; }
static void sonda_terminar(void *ctx, bool chegou) { (void)ctx; (void)chegou; }

static void ligar_placa(void)
{
    tem_placa = true;
    so_detectada = false;
    usa_irq = false;
    if (placa.irq > 0 && placa.irq < 16 && placa.irq != 2 && !irq_ocupada((uint8_t)placa.irq)) {
        irq_registrar((uint8_t)placa.irq, ao_irq);
        usa_irq = true;
        static bool sonda;
        if (placa.provocar_irq && !sonda) {
            irq_sonda_t s = { placa.nome, placa.pci, (uint8_t)placa.irq, sonda_provocar, sonda_contador,
                              sonda_terminar, NULL };
            irq_sonda_registrar(&s);
            sonda = true;
        }
    }
    klog("[REDE] placa %s (%s), %s\n", placa.nome, placa.descricao,
         usa_irq ? "com interrupcao" : "por sondagem a cada tique");
    ligar_tarefa();
}

void rede_iniciar(void)
{
    bool i0 = int_salva_desliga();
    memset(&placa, 0, sizeof placa);
    if (e1000_procurar(&placa) || (memset(&placa, 0, sizeof placa), rtl8139_procurar(&placa))) {
        ligar_placa();
    } else {
        memset(&placa, 0, sizeof placa);
        bool achou = r8169_procurar(&placa, false);
        if (achou) procurar_experimental = r8169_procurar;
        else {
            memset(&placa, 0, sizeof placa);
            achou = alc_procurar(&placa, false);   /* D200: Atheros AR813x/AR815x */
            if (achou) procurar_experimental = alc_procurar;
        }
        if (achou && !placa.experimental) {
            /* D091: revisão já testada em hardware real: liga direto. */
            memset(&placa, 0, sizeof placa);
            if (procurar_experimental(&placa, true)) ligar_placa();
        } else if (achou) {
            so_detectada = true;
            klog("[REDE] %s detectada: driver experimental desligado (ifconfig eth0 experimental)\n", placa.descricao);
        }
    }
    ligar_tarefa();   /* mesmo sem placa: o loopback (127.0.0.1) precisa dela */
    wifi_pci_iniciar();   /* D280: Wi-Fi integrada Atheros AR9485 (PCIe), si la hay */
    int_restaura(i0);
}

/* A linha da tela de boot (sem o "[ ok ]"). */
void rede_linha_boot(char *s, int max, bool *aviso)
{
    char ip[16];
    *aviso = false;
    if (so_detectada) {
        snformatar(s, (size_t)max, "Red: %s %04x:%04x sin activar (ifconfig eth0 experimental)",
                   placa.pci.fabricante == 0x1969 ? "Atheros" : "Realtek", placa.pci.fabricante, placa.pci.modelo);
        *aviso = true;
        return;
    }
    if (!tem_placa) { s[0] = 0; return; }
    int k = snformatar(s, (size_t)max, "Red: %s %02x:%02x:%02x:%02x:%02x:%02x, ", placa.nome, placa.mac[0], placa.mac[1],
                       placa.mac[2], placa.mac[3], placa.mac[4], placa.mac[5]);
    if (meu_ip) {
        ip_texto(ip, sizeof ip, meu_ip);
        snformatar(s + k, (size_t)(max - k), "%s (%s)", ip, metodo == REDE_ESTATICO ? "estática" : "DHCP");
    } else if (!placa.cabo()) {
        snformatar(s + k, (size_t)(max - k), "sin cable");
        *aviso = true;
    } else {
        snformatar(s + k, (size_t)(max - k), "pidiendo IP por DHCP...");
    }
}

/* ------------------------------------------------------------ configuração */

/* D091: as linhas de diagnóstico do "ip": motivos dos descartes da pilha,
 * os primeiros quadros descartados e o que o driver tiver a dizer. Só para
 * o driver experimental, para o "ip" das placas que funcionam não mudar. */
static void diagnostico_preencher(char *s, uint32_t max)
{
    s[0] = 0;
    if (!tem_placa || !placa.experimental) return;
    uint32_t k = 0, soma = 0;
#define POE(...) do { if (k < max) { int _r = snformatar(s + k, max - k, __VA_ARGS__); k += _r > 0 ? (uint32_t)_r : 0; } } while (0)
    POE("Descartes en la pila:");
    for (int i = 0; i < DESC_MOTIVOS; i++) {
        soma += descartes[i];
        if (descartes[i]) POE(" %s %u%s", nome_descarte[i], descartes[i], i == DESC_TIPO ? "" : ",");
        if (i == DESC_TIPO && descartes[i]) POE(" (último %04x),", descarte_ultimo_tipo);
    }
    POE(" otros %u\n", placa.descartados >= soma ? placa.descartados - soma : 0);
    for (uint32_t i = 0; i < descartes_registrados && i < DESC_GUARDADOS; i++)
        POE("  descarte %u: %s, destino %02x:%02x:%02x:%02x:%02x:%02x tipo %04x, %u bytes\n", i + 1,
            nome_descarte[descarte_quadros[i].motivo], descarte_quadros[i].destino[0], descarte_quadros[i].destino[1],
            descarte_quadros[i].destino[2], descarte_quadros[i].destino[3], descarte_quadros[i].destino[4],
            descarte_quadros[i].destino[5], descarte_quadros[i].tipo, descarte_quadros[i].n);
    if (placa.diagnostico && k < max) placa.diagnostico(s + k, max - k);
#undef POE
}

static void info_preencher(info_rede_t *v, bool com_diagnostico)
{
    memset(v, 0, sizeof *v);
    txt_copia_n(v->nome, wifi_ativa ? "wlan0" : "eth0", sizeof v->nome);
    txt_copia_n(v->driver, placa.nome ? placa.nome : "", sizeof v->driver);
    txt_copia_n(v->descricao, placa.descricao ? placa.descricao : "", sizeof v->descricao);
    memcpy(v->mac, placa.mac, 6);
    v->ativa = tem_placa;
    v->cabo = tem_placa && placa.cabo();
    v->experimental = placa.experimental;
    v->irq = usa_irq ? (uint8_t)placa.irq : 0xFF;
    v->fabricante = placa.pci.fabricante;
    v->modelo = placa.pci.modelo;
    v->barramento = placa.pci.barramento;
    v->dispositivo = placa.pci.dispositivo;
    v->funcao = placa.pci.funcao;
    v->dhcp_estado = (uint8_t)dhcp_estado;
    v->ip = meu_ip; v->mascara = mascara; v->roteador = roteador; v->dns = servidor_dns;
    v->metodo = metodo;
    v->servidor_dhcp = servidor_dhcp;
    if (metodo == REDE_POR_DHCP) {
        uint32_t passado = (agora() - dhcp_inicio_concessao) / TIQUES_POR_SEGUNDO;
        v->concessao_s = passado < dhcp_concessao ? dhcp_concessao - passado : 0;
    }
    v->rx_pacotes = placa.rx_pacotes; v->tx_pacotes = placa.tx_pacotes;
    v->rx_bytes = placa.rx_bytes;     v->tx_bytes = placa.tx_bytes;
    v->rx_erros = placa.rx_erros;     v->tx_erros = placa.tx_erros;
    v->descartados = placa.descartados; v->irqs = placa.irqs;
    v->desativada = desativada;
    v->pode_desativar = tem_placa;
    if (com_diagnostico) diagnostico_preencher(v->diagnostico, sizeof v->diagnostico);
}

/* D215: la interfaz que no es la activa (la eth0 aparcada o la wlan0 sin
 * conectar): solo la tarjeta y sus contadores, sin dirección. */
static void info_inativa(info_rede_t *v, const placa_t *p, const char *nome)
{
    memset(v, 0, sizeof *v);
    txt_copia_n(v->nome, nome, sizeof v->nome);
    txt_copia_n(v->driver, p->nome ? p->nome : "", sizeof v->driver);
    txt_copia_n(v->descricao, p->descricao ? p->descricao : "", sizeof v->descricao);
    memcpy(v->mac, p->mac, 6);
    v->ativa = 1;
    v->cabo = p->cabo && p->cabo();
    v->experimental = p->experimental;
    v->irq = 0xFF;
    v->fabricante = p->pci.fabricante;
    v->modelo = p->pci.modelo;
    v->rx_pacotes = p->rx_pacotes; v->tx_pacotes = p->tx_pacotes;
    v->rx_bytes = p->rx_bytes;     v->tx_bytes = p->tx_bytes;
    v->rx_erros = p->rx_erros;     v->tx_erros = p->tx_erros;
    v->descartados = p->descartados;
}

static void reiniciar_ip(void)
{
    meu_ip = 0; mascara = 0; roteador = 0; servidor_dns = 0;
    metodo = REDE_SEM_IP;
    dhcp_estado = DHCP_PARADO;
    dhcp_tentativas = 0;
    for (int i = 0; i < ARP_MAX; i++) arp[i].estado = ARP_LIVRE;
    for (int i = 0; i < ARP_PENDENTES; i++) pendentes[i].n = 0;
    for (int i = 0; i < DNS_CACHE; i++) dns_cache[i].usado = false;
}

/* D238: lo llama wifi.c al dejar tramas en su cola de llegada. */
void rede_acordar(void)
{
    acordar(&canal_rede);
}

void rede_wifi_registrar(placa_t *p)
{
    bool i0 = int_salva_desliga();
    if (!p && wifi_ativa) rede_wifi_conectada(false);
    wlan = p;
    int_restaura(i0);
}

void rede_wifi_conectada(bool sim)
{
    bool i0 = int_salva_desliga();
    if (sim && wlan && !wifi_ativa) {
        placa_eth = placa;
        eth_tinha = tem_placa;
        placa = *wlan;
        tem_placa = true;
        wifi_ativa = true;
        desativada = false;
        reiniciar_ip();   /* el DHCP empieza solo en el próximo tique (la wlan0 tiene «cable») */
        klog("[REDE] wlan0 activa%s\n", eth_tinha ? " (eth0 aparcada)" : "");
    } else if (!sim && wifi_ativa) {
        if (wlan) *wlan = placa;   /* los contadores vuelven a la wlan0 */
        placa = placa_eth;
        tem_placa = eth_tinha;
        wifi_ativa = false;
        reiniciar_ip();
        klog("[REDE] wlan0 desconectada%s\n", tem_placa ? ": vuelve la eth0" : "");
    }
    int_restaura(i0);
    acordar(&canal_rede);
}

/* "lo": só contadores (cada pacote conta uma vez como enviado e outra como recebido). */
static void info_lo(info_rede_t *v)
{
    memset(v, 0, sizeof *v);
    txt_copia_n(v->nome, "lo", sizeof v->nome);
    txt_copia_n(v->driver, "loopback", sizeof v->driver);
    txt_copia_n(v->descricao, "Interfaz de bucle local", sizeof v->descricao);
    v->ativa = 1;
    v->cabo = 1;
    v->irq = 0xFF;
    v->ip = LO_IP;
    v->mascara = 0xFF000000u;
    v->metodo = REDE_ESTATICO;
    v->rx_pacotes = v->tx_pacotes = lo_pacotes;
    v->rx_bytes = v->tx_bytes = lo_bytes;
    v->descartados = lo_descartados;
    v->loopback = 1;
}

int32_t rede_info(uint32_t tipo, uint32_t ptr, uint32_t n)
{
    if (tipo == INFO_REDE) {
        /* D079: info_rede_t cresceu no fim; quem foi compilado antes pede
         * menos bytes (n = o sizeof dele) e recebe só a parte antiga. */
        uint32_t tam = (n && n < sizeof(info_rede_t)) ? n : sizeof(info_rede_t);
        if (!usuario_ok((void *)ptr, tam, true)) return ERRO_ENDERECO;
        static info_rede_t v;   /* 3,2 KB: fora da pilha do kernel */
        info_preencher(&v, true);
        memcpy((void *)ptr, &v, tam);
        return tem_placa || so_detectada;
    }
    if (tipo == INFO_INTERFACES) {
        info_rede_t *v = (info_rede_t *)ptr;
        if (n > 4) n = 4;
        if (!usuario_ok(v, n * sizeof *v, true)) return ERRO_ENDERECO;
        uint32_t k = 0;
        if (k < n && (tem_placa || so_detectada)) info_preencher(&v[k++], true);
        if (k < n && wifi_ativa && eth_tinha) info_inativa(&v[k++], &placa_eth, "eth0");
        if (k < n && !wifi_ativa && wlan) info_inativa(&v[k++], wlan, "wlan0");
        if (k < n) info_lo(&v[k++]);
        return (int32_t)k;
    }
    if (tipo == INFO_SOCKETS) {
        if (n > 64) n = 64;
        if (!usuario_ok((void *)ptr, n * sizeof(info_socket_t), true)) return ERRO_ENDERECO;
        info_socket_t *v = (info_socket_t *)ptr;
        int k = tcp_listar_detalhado(v, (int)n);
        info_conexao_t u[16];
        int nu = udp_listar(u, (int)n - k < 16 ? (int)n - k : 16);
        for (int i = 0; i < nu; i++) {
            memset(&v[k], 0, sizeof v[k]);
            v[k].c = u[i];
            v[k++].estado_tcp = TCP_FECHADA;
        }
        return k;
    }
    if (tipo == INFO_TRAFEGO) {
        info_trafego_t *v = (info_trafego_t *)ptr;
        if (!usuario_ok(v, sizeof *v, true)) return ERRO_ENDERECO;
        memset(v, 0, sizeof *v);
        v->amostras = trafego_qtd;
        for (uint32_t i = 0; i < trafego_qtd; i++) {   /* alinhadas à direita: a mais nova no fim */
            v->rx[TRAFEGO_AMOSTRAS - trafego_qtd + i] = trafego_rx[i];
            v->tx[TRAFEGO_AMOSTRAS - trafego_qtd + i] = trafego_tx[i];
        }
        return 0;
    }
    if (tipo == INFO_CONEXOES) {
        if (n > 64) n = 64;
        if (!usuario_ok((void *)ptr, n * sizeof(info_conexao_t), true)) return ERRO_ENDERECO;
        int k = tcp_listar((void *)ptr, (int)n);
        k += udp_listar((info_conexao_t *)ptr + k, (int)n - k);
        return k;
    }
    if (tipo == INFO_ARP) {
        info_arp_t *v = (info_arp_t *)ptr;
        if (n > ARP_MAX) n = ARP_MAX;
        if (!usuario_ok(v, n * sizeof *v, true)) return ERRO_ENDERECO;
        uint32_t k = 0;
        for (int i = 0; i < ARP_MAX && k < n; i++) {
            if (arp[i].estado == ARP_LIVRE) continue;
            v[k].ip = arp[i].ip;
            memcpy(v[k].mac, arp[i].mac, 6);
            v[k].completa = arp[i].estado == ARP_COMPLETA;
            v[k].reservado = 0;
            v[k].idade_s = (agora() - arp[i].quando) / TIQUES_POR_SEGUNDO;
            k++;
        }
        return (int32_t)k;
    }
    return ERRO_INVALIDO;
}

/* SIS_CONFIG_REDE (op, &config_rede_t) */
int32_t sis_config_rede(uint32_t op, uint32_t ptr, uint32_t c, uint32_t d, uint32_t e_)
{
    (void)c; (void)d; (void)e_;
    config_rede_t *cfg = (config_rede_t *)ptr;
    bool sem_ponteiro = op == REDE_DHCP || op == REDE_EXPERIMENTAL || op == REDE_RENOVAR || op == REDE_DESATIVAR ||
                        op == REDE_ATIVAR || op == REDE_FECHAR || op == REDE_AJUSTAR_DRIVER;
    if (!sem_ponteiro && !usuario_ok(cfg, sizeof *cfg, op == REDE_CONSULTAR)) return ERRO_ENDERECO;
    switch (op) {
    case REDE_CONSULTAR:
        cfg->ip = meu_ip; cfg->mascara = mascara; cfg->roteador = roteador; cfg->dns = servidor_dns;
        cfg->metodo = metodo;
        return 0;
    case REDE_FIXAR: {
        if (!tem_placa) return ERRO_SEM_REDE;
        uint32_t ip = cfg->ip, m = cfg->mascara;
        if (ip == 0 || ip == IP_TODOS || (ip >> 28) >= 14 || (ip >> 24) == 127) return ERRO_INVALIDO;
        if (m == 0 || (~m & (~m + 1)) != 0) return ERRO_INVALIDO;   /* máscara: uns seguidos de zeros */
        if (cfg->roteador && ((cfg->roteador ^ ip) & m)) return ERRO_INVALIDO;   /* roteador fora da rede */
        meu_ip = ip; mascara = m; roteador = cfg->roteador;
        if (cfg->dns) servidor_dns = cfg->dns;
        metodo = REDE_ESTATICO;
        dhcp_estado = DHCP_PARADO;
        for (int i = 0; i < ARP_MAX; i++) arp[i].estado = ARP_LIVRE;
        arp_enviar(1, NULL, meu_ip);   /* ARP gratuito */
        klog("[REDE] endereco estatico configurado\n");
        return 0;
    }
    case REDE_DNS:
        if (!cfg->dns || cfg->dns == IP_TODOS) return ERRO_INVALIDO;
        servidor_dns = cfg->dns;
        for (int i = 0; i < DNS_CACHE; i++) dns_cache[i].usado = false;
        return 0;
    case REDE_DHCP: {
        espera_t *e = espera_minha(ESP_DHCP);
        if (!e) {
            if (!tem_placa) return ERRO_SEM_REDE;
            e = espera_nova(ESP_DHCP, 10000);
            if (!e) return ERRO_LIMITE;
            if (metodo == REDE_ESTATICO) { meu_ip = 0; metodo = REDE_SEM_IP; }
            dhcp_tentativas = 0;
            if (dhcp_estado == DHCP_PRONTO || dhcp_estado == DHCP_RENOVANDO) {   /* renova já */
                dhcp_estado = DHCP_RENOVANDO;
                dhcp_xid++;
                dhcp_proxima = agora() + 2 * TIQUES_POR_SEGUNDO;
                dhcp_pedir();
            } else {
                dhcp_estado = DHCP_PARADO;
                if (placa.cabo()) dhcp_descobrir();
            }
        }
        return espera_continuar(e, 0);
    }
    case REDE_RENOVAR:
        /* D079: o "Renovar DHCP" do Administrador de tareas não pode ficar
         * 10 s parado esperando como o comando dhcp: só dispara. */
        if (!tem_placa) return ERRO_SEM_REDE;
        if (desativada) return ERRO_OCUPADO;
        if (metodo == REDE_ESTATICO) { meu_ip = 0; metodo = REDE_SEM_IP; }
        dhcp_tentativas = 0;
        if (dhcp_estado == DHCP_PRONTO || dhcp_estado == DHCP_RENOVANDO) {
            dhcp_estado = DHCP_RENOVANDO;
            dhcp_xid++;
            dhcp_proxima = agora() + 2 * TIQUES_POR_SEGUNDO;
            dhcp_pedir();
        } else {
            dhcp_estado = DHCP_PARADO;
            if (placa.cabo()) dhcp_descobrir();
        }
        klog("[REDE] DHCP: renovacao pedida\n");
        return 0;
    case REDE_DESATIVAR:
    case REDE_ATIVAR:
        if (!tem_placa) return ERRO_SEM_REDE;
        if (desativada == (op == REDE_DESATIVAR)) return 0;
        desativada = op == REDE_DESATIVAR;
        klog("[REDE] eth0 %s\n", desativada ? "desativada" : "ativada");
        if (!desativada && metodo == REDE_POR_DHCP) {   /* volta: confirma o endereço com o servidor */
            dhcp_estado = DHCP_RENOVANDO;
            dhcp_xid++;
            dhcp_proxima = agora() + 2 * TIQUES_POR_SEGUNDO;
            dhcp_pedir();
        }
        return 0;
    case REDE_FECHAR:
        return tcp_abortar(ptr);
    case REDE_AJUSTAR_DRIVER: {
        /* D124: ajustes en caliente del driver experimental, para probar
         * variantes en el hardware real sin recompilar. */
        if (!usuario_ok((void *)ptr, sizeof(ajuste_driver_t), false)) return ERRO_ENDERECO;
        if (!tem_placa) return ERRO_SEM_REDE;
        if (!placa.ajustar) return ERRO_INVALIDO;
        ajuste_driver_t a = *(const ajuste_driver_t *)ptr;
        bool i = int_salva_desliga();
        int32_t r = placa.ajustar(&a);
        int_restaura(i);
        klog("[REDE] ajuste do driver (mascara %x): %d\n", a.mascara, r);
        return r;
    }
    case REDE_EXPERIMENTAL:
        if (tem_placa) return placa.experimental ? 0 : ERRO_JA_EXISTE;
        if (!so_detectada || !procurar_experimental) return ERRO_NAO_EXISTE;
        memset(&placa, 0, sizeof placa);
        if (!procurar_experimental(&placa, true)) return ERRO_SEM_MEMORIA;
        ligar_placa();
        return 0;
    }
    return ERRO_INVALIDO;
}

/* Para la bandeja de la barra (D114): el mismo resumen de INFO_REDE sin pasar por la syscall. */
bool rede_bandeja(info_rede_t *v)
{
    info_preencher(v, false);   /* D124: sin el diagnóstico (lee todos los búferes del r8169) */
    return tem_placa || so_detectada;
}

void rede_renovar_dhcp(void) { sis_config_rede(REDE_RENOVAR, 0, 0, 0, 0); }
