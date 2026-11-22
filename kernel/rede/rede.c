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
#include "tunel.h"

#define ETH_IP   0x0800
#define ETH_ARP  0x0806

#define PORTA_DHCP_SERVIDOR 67
#define PORTA_DHCP_CLIENTE  68
#define PORTA_DNS           53

/* D445: varias interfaces a la vez (eth0, eth1, wlan0), cada una con su tarjeta, su dirección,
 * su DHCP y sus entradas ARP. Antes había una sola interfaz activa: la Wi-Fi conectada aparcaba la
 * eth0 y el Ethernet USB solo se usaba sin tarjeta PCI.
 *
 * Rutas: no hay tabla aparte; salen de las interfaces (como en Linux con una dirección por
 * interfaz):
 *   - cada interfaz con dirección da la ruta de su red local (ip & mascara, sin puerta de enlace);
 *   - cada interfaz con puerta de enlace da una ruta por defecto con su métrica (cable 100,
 *     Wi-Fi 600, se cambia con REDE_METRICA); gana la de métrica menor entre las que tienen
 *     enlace y no están desactivadas; si ninguna lo tiene, la de métrica menor igualmente (con
 *     una sola tarjeta nada cambia: sin cable se intenta como antes);
 *   - un paquete cuyo origen es la dirección de una interfaz que puede llevarlo (red local o
 *     puerta de enlace) sale por ella: las conexiones TCP siguen por la interfaz por la que
 *     empezaron aunque cambie la ruta por defecto, y las respuestas salen por donde entró la
 *     pregunta (dos tarjetas en la misma red, o una conexión entrante por la que no es la ruta
 *     por defecto).
 * La «interfaz principal» (la de la ruta por defecto o, si no hay, la primera con dirección) es la
 * que ven las llamadas sin nombre de interfaz (REDE_CONSULTAR, INFO_REDE, la bandeja), y su DNS es
 * el del sistema. Con una sola tarjeta, todo es como antes.
 *
 * Los drivers siguen siendo de una tarjeta cada uno (sus funciones no llevan contexto): de dos
 * tarjetas iguales (dos e1000) solo se usa la primera. */
#define REDE_IFS 4
enum { IF_LIVRE, IF_PCI, IF_USB, IF_WIFI };
#define METRICA_CABO 100
#define METRICA_WIFI 600
typedef struct {
    uint8_t  tipo;                /* IF_*; IF_LIVRE = hueco sin usar */
    char     nome[8];             /* "eth0", "eth1", "wlan0" */
    placa_t  placa;
    bool     ligada;              /* driver encendido: manda y recibe (el antiguo tem_placa) */
    bool     so_detectada;        /* tarjeta conocida sin driver activo (r8169/alc experimental) */
    bool   (*procurar_experimental)(placa_t *p, bool ativar);   /* D200: el de la detectada */
    bool     usa_irq, sonda;
    /* D079: desactivada a pedido (REDE_DESATIVAR): la tarjeta sigue encendida, pero la pila no
     * manda nada y tira lo que llegue. Vale para cualquier driver, porque se hace aquí. */
    bool     desativada;
    bool     enlace;              /* el último cabo(), leído en la tarea de la red a cada vuelta */
    uint32_t metrica;             /* de la ruta por defecto: menor = preferida */
    uint32_t ip, mascara, roteador, dns, metodo;
    int      dhcp_estado;
    uint32_t dhcp_xid, dhcp_oferta, servidor_dhcp, dhcp_proxima, dhcp_tentativas;
    uint32_t dhcp_inicio_concessao, dhcp_concessao;   /* tiques; concesión en segundos */
} iface_t;
static iface_t ifs[REDE_IFS];
/* La interfaz del cuadro que se está tratando (NULL = vino del lo o del túnel). La pila no es
 * reentrante (todo va con las interrupciones apagadas), así que basta una variable. */
static iface_t *rx_if;
static uint32_t irq_alheias;    /* interrupções na linha que não eram de nenhuma placa, no último segundo */
static uint16_t linhas_nossas;  /* líneas de IRQ con ao_irq (varias tarjetas pueden compartir una) */
/* Contadores de todas las tarjetas juntas (INFO_TRAFEGO y el registro de D437): no bajan
 * cuando una interfaz desaparece. */
static uint32_t total_rx, total_tx;

/* D215: la wlan0 (pincho Wi-Fi o la AR9485). Registrada = tiene hueco en ifs, sin conectar
 * (ligada = false); conectada = ligada y con su DHCP, a la vez que la eth0 (D445). */
static placa_t *wlan;

static bool usavel(const iface_t *f) { return f->ligada && f->enlace && !f->desativada; }
/* ¿a es mejor que b para la ruta por defecto? Primero las que tienen enlace, luego la métrica. */
static bool melhor(const iface_t *a, const iface_t *b)
{
    if (!b) return true;
    if (usavel(a) != usavel(b)) return usavel(a);
    if (a->metrica != b->metrica) return a->metrica < b->metrica;
    return a < b;
}
/* La de la ruta por defecto (con dirección y puerta de enlace), o NULL. */
static iface_t *if_padrao(void)
{
    iface_t *m = NULL;
    for (int i = 0; i < REDE_IFS; i++)
        if (ifs[i].ligada && ifs[i].ip && ifs[i].roteador && melhor(&ifs[i], m)) m = &ifs[i];
    return m;
}
/* La principal: la de la ruta por defecto; si no, la mejor con dirección; si no, la mejor
 * encendida; si no, una detectada sin driver. NULL = ninguna tarjeta. */
static iface_t *if_principal(void)
{
    iface_t *m = if_padrao();
    if (m) return m;
    for (int i = 0; i < REDE_IFS; i++) if (ifs[i].ligada && ifs[i].ip && melhor(&ifs[i], m)) m = &ifs[i];
    if (m) return m;
    for (int i = 0; i < REDE_IFS; i++) if (ifs[i].ligada && melhor(&ifs[i], m)) m = &ifs[i];
    if (m) return m;
    for (int i = 0; i < REDE_IFS; i++) if (ifs[i].so_detectada) return &ifs[i];
    return NULL;
}
static bool alguma_ligada(void)
{
    for (int i = 0; i < REDE_IFS; i++) if (ifs[i].ligada) return true;
    return false;
}
static iface_t *if_do_ip(uint32_t ip)
{
    if (!ip) return NULL;
    for (int i = 0; i < REDE_IFS; i++) if (ifs[i].ligada && ifs[i].ip == ip) return &ifs[i];
    return NULL;
}
/* ¿destino está en la red local de f? */
static bool na_rede(const iface_t *f, uint32_t destino)
{
    return f->ligada && f->ip && ((destino ^ f->ip) & f->mascara) == 0;
}
static bool difusao_de(const iface_t *f, uint32_t destino)
{
    return f && f->ip && f->mascara && destino == (f->ip | ~f->mascara);
}
static iface_t *if_livre(void)
{
    for (int i = 0; i < REDE_IFS; i++)
        if (ifs[i].tipo == IF_LIVRE) { memset(&ifs[i], 0, sizeof ifs[i]); return &ifs[i]; }
    return NULL;
}
/* Ocupa un hueco para una tarjeta cableada (PCI o USB) con el primer ethN libre. */
static iface_t *if_nova_eth(uint8_t tipo)
{
    for (int n = 0; n < 10; n++) {
        char nome[8];
        snformatar(nome, sizeof nome, "eth%d", n);
        bool usado = false;
        for (int i = 0; i < REDE_IFS; i++) if (ifs[i].tipo != IF_LIVRE && txt_compara(ifs[i].nome, nome) == 0) usado = true;
        if (usado) continue;
        iface_t *f = if_livre();
        if (!f) return NULL;
        f->tipo = tipo;
        txt_copia_n(f->nome, nome, sizeof f->nome);
        f->metrica = METRICA_CABO;
        f->metodo = REDE_SEM_IP;
        return f;
    }
    return NULL;
}

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

/* Tráfego da rede por segundo (INFO_TRAFEGO; D445: de todas las tarjetas juntas): uma subtração
 * por segundo na tarefa da rede, para o gráfico do Administrador já abrir com o último minuto. */
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
static iface_t *if_principal(void);
static void descartar(int motivo, const uint8_t *q, uint32_t n)
{
    iface_t *f = rx_if ? rx_if : if_principal();
    if (f) f->placa.descartados++;
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

/* D445: la tarjeta de la interfaz principal (NULL = ninguna encendida). */
placa_t *rede_placa(void) { iface_t *f = if_principal(); return f && f->ligada ? &f->placa : NULL; }
bool rede_pronta(void)
{
    for (int i = 0; i < REDE_IFS; i++) if (ifs[i].ligada && ifs[i].ip) return true;
    return false;
}
uint32_t rede_meu_ip(void) { iface_t *f = if_principal(); return f && f->ligada ? f->ip : 0; }
bool rede_ip_de_interface(uint32_t ip) { return if_do_ip(ip) != NULL; }
uint8_t *rede_carga(void) { return quadro + 14 + 20; }
static uint32_t agora(void) { return relogio_tiques(); }
static bool passou(uint32_t t) { return (int32_t)(agora() - t) >= 0; }

/* D363: el túnel (wg0, tunel.c). Un destino va por él si su ruta más larga en el túnel
 * es al menos tan larga como la red local de la interfaz física: con AllowedIPs =
 * 0.0.0.0/0 todo va por el túnel salvo la red local (como wg-quick con su regla
 * «suppress_prefixlength 0»). El demonio del túnel marca su socket (OPC_FISICA) y
 * sus datagramas salen siempre por la física: rede_forcar_fisica vale mientras se
 * manda uno. La difusión y el multicast nunca van por el túnel. */
bool rede_forcar_fisica;
static uint32_t bits_de(uint32_t m) { uint32_t b = 0; while (b < 32 && (m & (0x80000000u >> b))) b++; return b; }
static bool pelo_tunel(uint32_t destino)
{
    if (rede_forcar_fisica || destino == IP_TODOS || (destino >> 28) >= 14 || (destino >> 24) == 127) return false;
    int lt = tunel_casamento(destino);
    if (lt < 0) return false;
    int lf = -1;   /* D445: la red local más larga entre las interfaces que la contienen */
    for (int i = 0; i < REDE_IFS; i++)
        if (na_rede(&ifs[i], destino) && ifs[i].mascara && (int)bits_de(ifs[i].mascara) > lf) lf = (int)bits_de(ifs[i].mascara);
    return lt >= lf;
}
static bool eh_meu(uint32_t ip) { return ip && (if_do_ip(ip) || ip == tunel_ip()); }

/* D445: por qué interfaz sale un paquete a destino y a quién se le da (salto: el propio destino
 * en la red local, la puerta de enlace fuera de ella; la difusión, a todos). origem != 0: la
 * dirección de origen que llevará; si es de una interfaz que puede llevarlo, sale por ella.
 * NULL = sin ruta. */
static iface_t *rota(uint32_t destino, uint32_t origem, uint32_t *salto)
{
    iface_t *o = if_do_ip(origem);
    *salto = destino;
    if (destino == IP_TODOS) {   /* la difusión general: por la del origen o por la principal */
        if (o) return o;
        iface_t *p = if_principal();
        return p && p->ligada ? p : NULL;
    }
    for (int i = 0; i < REDE_IFS; i++)   /* la difusión de una red local */
        if (ifs[i].ligada && difusao_de(&ifs[i], destino) && (!o || o == &ifs[i])) return &ifs[i];
    if (o && usavel(o)) {
        if (na_rede(o, destino)) return o;
        if (o->roteador) { *salto = o->roteador; return o; }
    }
    iface_t *m = NULL;   /* la red local más larga; a igualdad, la mejor */
    for (int i = 0; i < REDE_IFS; i++) {
        if (!na_rede(&ifs[i], destino)) continue;
        if (!m || ifs[i].mascara > m->mascara || (ifs[i].mascara == m->mascara && melhor(&ifs[i], m))) m = &ifs[i];
    }
    if (m) return m;
    m = if_padrao();
    if (m) *salto = m->roteador;
    return m;
}

uint32_t rede_origem_para(uint32_t destino)
{
    if ((destino >> 24) == 127) return LO_IP;
    if (pelo_tunel(destino)) return tunel_ip();
    if (if_do_ip(destino)) return destino;   /* a uno mismo: la propia dirección */
    uint32_t salto;
    iface_t *f = rota(destino, 0, &salto);
    if (f) return f->ip;
    return rede_meu_ip();
}
bool rede_eh_local(uint32_t ip) { return (ip >> 24) == 127 || eh_meu(ip); }
/* El MSS del TCP según por dónde sale: 1460 por Ethernet, MTU - 40 por el túnel (1380 con 1420). */
uint32_t rede_mss_para(uint32_t destino)
{
    if (pelo_tunel(destino) && tunel_mtu() > 40 + 536) return tunel_mtu() - 40;
    return 1460;
}
/* D445: el DNS del túnel, el de la interfaz principal o, si no tiene, el de otra cualquiera. */
static uint32_t dns_atual(void)
{
    uint32_t d = tunel_dns();
    if (d) return d;
    iface_t *p = if_principal();
    if (p && p->ligada && p->dns) return p->dns;
    for (int i = 0; i < REDE_IFS; i++) if (ifs[i].ligada && ifs[i].ip && ifs[i].dns) return ifs[i].dns;
    return 0;
}

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

/* D437: el último ip_enviar no salió porque la tarjeta (o la cola del lo o del túnel) estaba
 * llena. ip_enviar devuelve true en ese caso (la trama «salió» hacia el cable, como siempre);
 * el TCP lo pregunta para no dar el segmento por enviado y esperar a que haya hueco. */
static bool envio_recusado;
bool rede_envio_recusado(void) { return envio_recusado; }

static bool quadro_enviar(iface_t *f, const uint8_t *q, uint32_t n)
{
    if (!f || !f->ligada || f->desativada) return false;
    if (!f->placa.enviar(q, n)) { f->placa.tx_erros++; envio_recusado = true; return false; }
    f->placa.tx_pacotes++;
    f->placa.tx_bytes += n;
    total_tx += n;
    return true;
}

static void eth_enviar(iface_t *f, const uint8_t *destino, uint16_t tipo, uint8_t *q, uint32_t n)
{
    memcpy(q, destino, 6);
    memcpy(q + 6, f->placa.mac, 6);
    poe16(q + 12, tipo);
    quadro_enviar(f, q, 14 + n);
}

/* ------------------------------------------------------------ ARP
 * D445: cada entrada (y cada paquete en espera) es de una interfaz: la misma IP puede estar
 * en dos redes y una respuesta solo vale para la tarjeta por la que llegó.
 * Cache de 32 entradas. Uma entrada "completa" vale 10 minutos; uma
 * "incompleta" (pergunta feita, sem resposta) repete a pergunta a cada
 * segundo, três vezes, e então desiste e descarta o que esperava. Até 4
 * pacotes esperam a resposta (quase sempre é o do roteador, e só uma vez). */

#define ARP_MAX       32
#define ARP_VALIDADE  (600 * TIQUES_POR_SEGUNDO)
#define ARP_PENDENTES 4
enum { ARP_LIVRE, ARP_INCOMPLETA, ARP_COMPLETA };
static struct { uint32_t ip; uint8_t mac[6]; uint8_t estado, tentativas; uint32_t quando; iface_t *f; } arp[ARP_MAX];
static struct { uint32_t ip, n; iface_t *f; uint8_t q[ETH_MAX]; } pendentes[ARP_PENDENTES];

static int arp_indice(iface_t *f, uint32_t ip)
{
    for (int i = 0; i < ARP_MAX; i++)
        if (arp[i].estado != ARP_LIVRE && arp[i].ip == ip && arp[i].f == f) return i;
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

static void arp_enviar(iface_t *f, uint16_t operacao, const uint8_t *mac_alvo, uint32_t ip_alvo)
{
    uint8_t q[14 + 28];
    uint8_t *a = q + 14;
    poe16(a, 1);                 /* Ethernet */
    poe16(a + 2, ETH_IP);
    a[4] = 6; a[5] = 4;
    poe16(a + 6, operacao);      /* 1 pergunta, 2 resposta */
    memcpy(a + 8, f->placa.mac, 6);
    poe32(a + 14, f->ip);
    if (operacao == 1) memset(a + 18, 0, 6); else memcpy(a + 18, mac_alvo, 6);
    poe32(a + 24, ip_alvo);
    eth_enviar(f, operacao == 1 ? difusao : mac_alvo, ETH_ARP, q, 28);
}

static void arp_guardar(iface_t *f, uint32_t ip, const uint8_t *mac)
{
    int k = arp_indice(f, ip);
    if (k < 0) k = arp_vaga();
    arp[k].ip = ip;
    arp[k].f = f;
    memcpy(arp[k].mac, mac, 6);
    arp[k].estado = ARP_COMPLETA;
    arp[k].quando = agora();
    for (int i = 0; i < ARP_PENDENTES; i++)
        if (pendentes[i].n && pendentes[i].ip == ip && pendentes[i].f == f) {
            memcpy(pendentes[i].q, mac, 6);
            quadro_enviar(f, pendentes[i].q, pendentes[i].n);
            pendentes[i].n = 0;
        }
}

/* Pregunta ya por el MAC de ip (el router tras el DHCP), sin esperar a tener un paquete. */
static void arp_perguntar(iface_t *f, uint32_t ip)
{
    int k = arp_indice(f, ip);
    if (k < 0) {
        k = arp_vaga();
        arp[k].ip = ip; arp[k].f = f; arp[k].estado = ARP_INCOMPLETA; arp[k].tentativas = 0; arp[k].quando = agora();
    }
    arp_enviar(f, 1, NULL, ip);
}

/* D445: solo se contesta por la dirección de la interfaz por la que llegó la pregunta (cada IP
 * con su MAC, también con dos tarjetas en la misma red). */
static void arp_chegou(iface_t *f, const uint8_t *a, uint32_t n)
{
    if (n < 28 || be16(a) != 1 || be16(a + 2) != ETH_IP || a[4] != 6 || a[5] != 4) return;
    uint32_t ip_origem = be32(a + 14), ip_alvo = be32(a + 24);
    uint16_t op = be16(a + 6);
    if (ip_origem == 0 || ip_origem == IP_TODOS || (a[8] & 1)) return;   /* sondas e MAC de grupo: ignora */
    if (f->ip && ip_origem == f->ip) {
        klog("[REDE] ARP: outro computador usa o nosso IP\n");
        return;
    }
    if (!f->ip || ip_alvo != f->ip) {
        if (arp_indice(f, ip_origem) >= 0) arp_guardar(f, ip_origem, a + 8);   /* atualiza quem já conhecemos */
        return;
    }
    arp_guardar(f, ip_origem, a + 8);
    if (op == 1) arp_enviar(f, 2, a + 8, ip_origem);
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
        arp_enviar(arp[i].f, 1, NULL, arp[i].ip);
    }
}

/* Olvida lo de una interfaz (se apagó, cambió de dirección o se retiró). */
static void arp_esquecer(iface_t *f)
{
    for (int i = 0; i < ARP_MAX; i++) if (arp[i].f == f) arp[i].estado = ARP_LIVRE;
    for (int i = 0; i < ARP_PENDENTES; i++) if (pendentes[i].f == f) pendentes[i].n = 0;
}

/* ------------------------------------------------------------ IPv4 */

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

/* De dónde viene un paquete: del cable, del bucle local o del túnel (D363). */
enum { DE_CABO, DE_LO, DE_TUNEL };
static void ip_chegou(const uint8_t *h, uint32_t n, int de);

static void lo_processar(void)
{
    for (int k = 0; k < LO_FILA && lo_qtd; k++) {
        uint32_t n = lo_fila[lo_ini].n;
        memcpy(lo_atual, lo_fila[lo_ini].p, n);   /* a vaga fica livre para as respostas */
        lo_ini = (lo_ini + 1) % LO_FILA;
        lo_qtd--;
        ip_chegou(lo_atual, n, DE_LO);
    }
}

/* D445: forcado = la interfaz por la que tiene que salir (el DHCP de cada una); origem = la
 * dirección de origen (0 = la de la ruta, rede_origem_para). */
static bool ip_enviar_por(iface_t *forcado, uint32_t origem, uint32_t destino, uint8_t protocolo, uint32_t n)
{
    static uint16_t ident;
    envio_recusado = false;
    bool local = !forcado && rede_eh_local(destino);
    bool tunel = !forcado && !local && pelo_tunel(destino);
    if (n > ETH_MAX - 34) return false;
    if (!origem && !forcado) origem = rede_origem_para(destino);
    iface_t *f = forcado;
    uint32_t salto = destino;
    if (!local && !tunel && !f) f = rota(destino, origem, &salto);
    if (!local && !tunel && !f) return false;
    uint8_t *h = quadro + 14;
    h[0] = 0x45; h[1] = 0;
    poe16(h + 2, (uint16_t)(20 + n));
    poe16(h + 4, ident++);
    poe16(h + 6, 0x4000);        /* não fragmentar: o TCP nunca passa do MSS */
    h[8] = 64; h[9] = protocolo;
    poe16(h + 10, 0);
    poe32(h + 12, origem);
    poe32(h + 16, destino);
    poe16(h + 10, soma_final(soma_parcial(0, h, 20)));
    if (local) return lo_enfileirar(h, 20 + n);
    if (tunel) return tunel_enviar(h, 20 + n);   /* D363: el demonio lo cifra y lo manda */

    if (destino == IP_TODOS || difusao_de(f, destino)) {
        eth_enviar(f, difusao, ETH_IP, quadro, 20 + n);
        return true;
    }
    if (!f->ip || !salto) return false;    /* sem endereço, ou fora da rede e sem roteador */
    int k = arp_indice(f, salto);
    if (k >= 0 && arp[k].estado == ARP_COMPLETA) {
        eth_enviar(f, arp[k].mac, ETH_IP, quadro, 20 + n);
        return true;
    }
    /* Ainda não sabemos o MAC: guarda o pacote e pergunta. */
    int v = (int)(ident % ARP_PENDENTES);   /* fila cheia: um qualquer dá lugar (o TCP reenvia) */
    for (int i = 0; i < ARP_PENDENTES; i++)
        if (!pendentes[i].n) { v = i; break; }
    memcpy(quadro + 6, f->placa.mac, 6);
    poe16(quadro + 12, ETH_IP);
    pendentes[v].ip = salto;
    pendentes[v].f = f;
    pendentes[v].n = 14 + 20 + n;
    memcpy(pendentes[v].q, quadro, pendentes[v].n);
    if (k < 0) {
        k = arp_vaga();
        arp[k].ip = salto;
        arp[k].f = f;
        arp[k].estado = ARP_INCOMPLETA;
        arp[k].tentativas = 0;
        arp[k].quando = agora();
        arp_enviar(f, 1, NULL, salto);
    }
    return true;
}

bool ip_enviar(uint32_t destino, uint8_t protocolo, uint32_t n) { return ip_enviar_por(NULL, 0, destino, protocolo, n); }
/* D445: con el origen fijado (el TCP con la dirección de su conexión, la respuesta a un ping). */
bool ip_enviar_de(uint32_t origem, uint32_t destino, uint8_t protocolo, uint32_t n)
{
    return ip_enviar_por(NULL, origem, destino, protocolo, n);
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
    if (c[0] == 8 && c[1] == 0 && (eh_meu(destino) || (destino >> 24) == 127) && n <= ETH_MAX - 34) {   /* alguém pingou o SaguiOS */
        uint8_t *r = rede_carga();
        memmove(r, c, n);
        r[0] = 0;
        poe16(r + 2, 0);
        poe16(r + 2, soma_final(soma_parcial(0, r, n)));
        /* D445: la respuesta sale con la dirección a la que preguntaron (y por su interfaz) */
        ip_enviar_de((destino >> 24) == 127 || !eh_meu(destino) ? 0 : destino, origem, IP_ICMP, n);
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

/* Os dados já estão em rede_carga() + 8. D445: f != NULL = sale por esa interfaz con su
 * dirección (el DHCP); NULL = por la ruta. */
static bool udp_enviar_por(iface_t *f, uint32_t destino, uint16_t porta_origem, uint16_t porta_destino, uint32_t n)
{
    uint8_t *u = rede_carga();
    if (n > ETH_MAX - 34 - 8) return false;
    uint32_t origem = f ? f->ip : rede_origem_para(destino);
    poe16(u, porta_origem);
    poe16(u + 2, porta_destino);
    poe16(u + 4, (uint16_t)(8 + n));
    poe16(u + 6, 0);
    uint16_t s = soma_pseudo(origem, destino, IP_UDP, u, 8 + n);
    poe16(u + 6, s ? s : 0xFFFF);   /* 0 quer dizer "sem soma" no UDP */
    return ip_enviar_por(f, origem, destino, IP_UDP, 8 + n);
}

bool udp_enviar(uint32_t destino, uint16_t porta_origem, uint16_t porta_destino, uint32_t n)
{
    return udp_enviar_por(NULL, destino, porta_origem, porta_destino, n);
}

/* ------------------------------------------------------------ DHCP
 * D445: uno por interfaz (su estado, su xid y su concesión van en iface_t). */

enum { DHCP_PARADO, DHCP_DESCOBRINDO, DHCP_PEDINDO, DHCP_PRONTO, DHCP_RENOVANDO };

/* D445: el arranque espera a todas las interfaces con cable (como mucho 2 s, principal.c). */
bool rede_dhcp_terminou(void)
{
    for (int i = 0; i < REDE_IFS; i++) {
        iface_t *f = &ifs[i];
        if (!f->ligada || f->desativada || !f->enlace) continue;
        if (f->dhcp_estado != DHCP_PRONTO && f->metodo != REDE_ESTATICO) return false;
    }
    return true;
}

static uint8_t *dhcp_montar(iface_t *f, uint8_t tipo)
{
    uint8_t *b = rede_carga() + 8;
    memset(b, 0, 240);
    b[0] = 1; b[1] = 1; b[2] = 6;             /* pedido, Ethernet, MAC de 6 bytes */
    poe32(b + 4, f->dhcp_xid);
    if (f->dhcp_estado == DHCP_RENOVANDO) poe32(b + 12, f->ip);   /* ciaddr */
    else poe16(b + 10, 0x8000);               /* responda por difusão: ainda não temos IP */
    memcpy(b + 28, f->placa.mac, 6);
    poe32(b + 236, 0x63825363);               /* "magic cookie" */
    uint8_t *o = b + 240;
    *o++ = 53; *o++ = 1; *o++ = tipo;
    *o++ = 12; *o++ = 7; memcpy(o, "saguios", 7); o += 7;   /* nome do computador */
    return o;
}

static void dhcp_enviar(iface_t *f, uint8_t *fim)
{
    *fim++ = 55; *fim++ = 3; *fim++ = 1; *fim++ = 3; *fim++ = 6;   /* queremos máscara, roteador e DNS */
    *fim++ = 255;
    uint32_t n = (uint32_t)(fim - (rede_carga() + 8));
    if (n < 300) { memset(fim, 0, 300 - n); n = 300; }   /* alguns servidores ignoram pedidos curtos */
    udp_enviar_por(f, IP_TODOS, PORTA_DHCP_CLIENTE, PORTA_DHCP_SERVIDOR, n);
}

static void dhcp_descobrir(iface_t *f)
{
    f->dhcp_xid = 0x5A600000u ^ (agora() * 2654435761u) ^ ((uint32_t)f->placa.mac[5] << 8) ^ ((uint32_t)f->placa.mac[4] << 16);
    f->dhcp_estado = DHCP_DESCOBRINDO;
    uint32_t espera = 2u << (f->dhcp_tentativas < 3 ? f->dhcp_tentativas : 3);   /* 2, 4, 8, 16 s */
    f->dhcp_tentativas++;
    f->dhcp_proxima = agora() + espera * TIQUES_POR_SEGUNDO;
    dhcp_enviar(f, dhcp_montar(f, 1));
}

static void dhcp_pedir(iface_t *f)
{
    uint8_t *o = dhcp_montar(f, 3);
    if (f->dhcp_estado == DHCP_PEDINDO) {
        *o++ = 50; *o++ = 4; poe32(o, f->dhcp_oferta); o += 4;
        *o++ = 54; *o++ = 4; poe32(o, f->servidor_dhcp); o += 4;
    }
    dhcp_enviar(f, o);
}

/* Renueva ya (o pide de cero, si no tenía dirección). */
static void dhcp_renovar_ja(iface_t *f)
{
    f->dhcp_tentativas = 0;
    if (f->dhcp_estado == DHCP_PRONTO || f->dhcp_estado == DHCP_RENOVANDO) {
        f->dhcp_estado = DHCP_RENOVANDO;
        f->dhcp_xid++;
        f->dhcp_proxima = agora() + 2 * TIQUES_POR_SEGUNDO;
        dhcp_pedir(f);
    } else {
        f->dhcp_estado = DHCP_PARADO;
        if (f->placa.cabo()) dhcp_descobrir(f);
    }
}

/* Las esperas del DHCP (REDE_DHCP) guardan en «ip» el número de la interfaz. */
static void avisar_dhcp(iface_t *f, int resultado)
{
    for (int i = 0; i < ESPERAS; i++)
        if (esperas[i].tipo == ESP_DHCP && !esperas[i].resultado && esperas[i].ip == (uint32_t)(f - ifs)) {
            esperas[i].resultado = resultado;
            acordar(&esperas[i]);
        }
}

static void ip_texto(char *s, int max, uint32_t ip)
{
    snformatar(s, (size_t)max, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
}

/* Prefijo de los mensajes del registro: nada con una sola interfaz (como antes), "eth1: " con varias. */
static const char *pref_if(const iface_t *f, char *s, int max)
{
    int n = 0;
    for (int i = 0; i < REDE_IFS; i++) if (ifs[i].ligada) n++;
    if (n <= 1) s[0] = 0;
    else snformatar(s, (size_t)max, "%s: ", f->nome);
    return s;
}

static void dhcp_chegou(iface_t *f, const uint8_t *b, uint32_t n)
{
    if (!f || n < 240 || b[0] != 2 || be32(b + 4) != f->dhcp_xid || be32(b + 236) != 0x63825363) return;
    if (memcmp(b + 28, f->placa.mac, 6) != 0) return;
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
    char px[12];
    if (tipo == 2 && f->dhcp_estado == DHCP_DESCOBRINDO) {       /* oferta: pede esse endereço */
        if (oferta == 0 || oferta == IP_TODOS || (oferta >> 24) == 127 || (oferta >> 28) >= 14) return;
        f->dhcp_oferta = oferta;
        f->servidor_dhcp = srv;
        f->dhcp_estado = DHCP_PEDINDO;
        f->dhcp_tentativas = 0;
        f->dhcp_proxima = agora() + 2 * TIQUES_POR_SEGUNDO;
        dhcp_pedir(f);
    } else if (tipo == 5 && (f->dhcp_estado == DHCP_PEDINDO || f->dhcp_estado == DHCP_RENOVANDO)) {   /* confirmado */
        if (oferta == 0 || oferta == IP_TODOS) return;
        bool novo = f->ip != oferta;
        if (novo && f->ip) arp_esquecer(f);
        f->ip = oferta;
        f->mascara = msk ? msk : 0xFFFFFF00u;
        f->roteador = rot;
        f->dns = dns ? dns : rot;
        if (srv) f->servidor_dhcp = srv;
        f->dhcp_concessao = conc ? conc : 3600;
        if (f->dhcp_concessao < 60) f->dhcp_concessao = 60;
        if (f->dhcp_concessao > 7 * 24 * 3600) f->dhcp_concessao = 7 * 24 * 3600;
        f->dhcp_inicio_concessao = agora();
        f->dhcp_estado = DHCP_PRONTO;
        f->dhcp_tentativas = 0;
        f->metodo = REDE_POR_DHCP;
        if (novo) {
            char a[16], r[16], d[16];
            ip_texto(a, sizeof a, f->ip); ip_texto(r, sizeof r, f->roteador); ip_texto(d, sizeof d, f->dns);
            klog("[REDE] %sDHCP: ip %s roteador %s dns %s concessao %u s\n", pref_if(f, px, sizeof px), a, r, d,
                 f->dhcp_concessao);
            arp_enviar(f, 1, NULL, f->ip);              /* anuncia o endereço (ARP gratuito) */
            if (f->roteador) arp_perguntar(f, f->roteador);   /* já descobre o MAC do roteador */
        }
        else klog("[REDE] %sDHCP: concessao renovada (%u s)\n", pref_if(f, px, sizeof px), f->dhcp_concessao);
        avisar_dhcp(f, 1);
    } else if (tipo == 6 && f->dhcp_estado != DHCP_PRONTO) {      /* recusado: começa de novo */
        klog("[REDE] %sDHCP: o servidor recusou (NAK)\n", pref_if(f, px, sizeof px));
        if (f->dhcp_estado == DHCP_RENOVANDO) { f->ip = 0; f->metodo = REDE_SEM_IP; arp_esquecer(f); }
        f->dhcp_estado = DHCP_PARADO;
    }
}

static void dhcp_tique(iface_t *f)
{
    if (!f->ligada || f->desativada || f->metodo == REDE_ESTATICO) return;
    uint32_t passado = (agora() - f->dhcp_inicio_concessao) / TIQUES_POR_SEGUNDO;
    char px[12];
    switch (f->dhcp_estado) {
    case DHCP_PRONTO:
        if (passado >= f->dhcp_concessao / 2) {     /* metade da concessão: renova com o mesmo servidor */
            f->dhcp_estado = DHCP_RENOVANDO;
            f->dhcp_xid++;
            f->dhcp_proxima = agora() + 10 * TIQUES_POR_SEGUNDO;
            dhcp_pedir(f);
        }
        break;
    case DHCP_RENOVANDO:
        if (passado >= f->dhcp_concessao) {          /* acabou: perde o endereço e começa do zero */
            klog("[REDE] %sDHCP: a concessao acabou sem renovacao\n", pref_if(f, px, sizeof px));
            f->ip = 0;
            f->metodo = REDE_SEM_IP;
            f->dhcp_estado = DHCP_PARADO;
            arp_esquecer(f);
        } else if (passou(f->dhcp_proxima)) {
            f->dhcp_proxima = agora() + 10 * TIQUES_POR_SEGUNDO;
            dhcp_pedir(f);
        }
        break;
    case DHCP_PEDINDO:
        if (passou(f->dhcp_proxima)) {
            if (++f->dhcp_tentativas >= 3) { f->dhcp_tentativas = 0; dhcp_descobrir(f); }
            else { f->dhcp_proxima = agora() + 2 * TIQUES_POR_SEGUNDO; dhcp_pedir(f); }
        }
        break;
    default:
        if (!f->enlace) break;
        if (f->dhcp_estado == DHCP_PARADO || passou(f->dhcp_proxima)) dhcp_descobrir(f);
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
        if (!servidor) servidor = dns_atual();
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
    bool por_padrao = e->servidor == dns_atual() && e->porta_srv == PORTA_DNS;
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
    if (be16(u + 6) && soma_pseudo(origem, destino, IP_UDP, u, tam) != 0) { if (rx_if) rx_if->placa.rx_erros++; return; }
    uint16_t po = be16(u), pd = be16(u + 2);
    if (pd == PORTA_DHCP_CLIENTE && po == PORTA_DHCP_SERVIDOR) { dhcp_chegou(rx_if, u + 8, tam - 8); return; }
    /* D445: la difusión vale la de la red de la interfaz por la que llegó */
    if ((destino >> 24) != 127 && !eh_meu(destino) &&
        (!rx_if || !rx_if->ip || (destino != IP_TODOS && !difusao_de(rx_if, destino))))
        return;
    if (pd >= 50000 && pd < 50000 + 0x2000) {
        for (int i = 0; i < ESPERAS; i++)
            if (esperas[i].tipo == ESP_DNS && esperas[i].porta == pd) { dns_chegou(origem, pd, u + 8, tam - 8); return; }
    }
    udp_chegou_socket(origem, destino, po, pd, u + 8, tam - 8);
}

static void ip_chegou(const uint8_t *h, uint32_t n, int de)
{
    bool do_lo = de != DE_CABO;   /* sin trama Ethernet: el registro de descartes no la enseña */
    if (n < 20 || (h[0] >> 4) != 4) return;
    uint32_t cab = (h[0] & 15) * 4u, total = be16(h + 2);
    if (cab < 20 || total < cab || total > n || soma_final(soma_parcial(0, h, cab)) != 0) { if (rx_if) rx_if->placa.rx_erros++; return; }
    /* Pedaços de pacotes fragmentados são descartados: o TCP anuncia MSS
     * 1460 e ninguém precisa fragmentar para falar com o SaguiOS. */
    if (be16(h + 6) & 0x3FFF) { descartar(DESC_FRAGMENTO, do_lo ? NULL : chegada, n + 14); return; }
    uint32_t origem = be32(h + 12), destino = be32(h + 16);
    bool para_lo = (destino >> 24) == 127;
    /* 127.x vindo do cabo é falsificado (RFC 1122, "martian"): fora. */
    if (de != DE_LO && (para_lo || (origem >> 24) == 127)) { descartar(DESC_FALSO_127, do_lo ? NULL : chegada, n + 14); return; }
    /* D445: rx_if = la interfaz por la que llegó (NULL desde el lo o el túnel) */
    uint32_t meu_ip = rx_if ? rx_if->ip : rede_meu_ip();
    bool difusao_ip = destino == IP_TODOS || (rx_if ? difusao_de(rx_if, destino) : false);
    /* D363, revisión de seguridad: el demonio del túnel puede ser cualquier programa de la sesión
     * local (no hace falta root), así que lo que escribe se trata como lo que llega de un cable
     * ajeno y se filtra en las dos direcciones:
     *   - por el túnel solo entra lo que va a la dirección del túnel y viene de un origen que
     *     respondemos por el túnel (filtro de camino inverso estricto, el AllowedIPs del kernel):
     *     nada que se haga pasar por la red local, por el servidor DNS o DHCP de la física;
     *   - por el cable no entra nada para la dirección del túnel: un vecino de la red local no
     *     puede meterse en las conexiones de la VPN falsificando el origen de un par. */
    if (de == DE_TUNEL && (difusao_ip || destino != tunel_ip() || !pelo_tunel(origem))) { tunel_descarte(origem, destino); return; }
    if (de == DE_CABO && tunel_ativo() && destino == tunel_ip() && !if_do_ip(destino)) return;
    if ((meu_ip || de == DE_TUNEL) && !eh_meu(destino) && !difusao_ip && !para_lo) return;
    if (origem == IP_TODOS || (origem >> 28) == 14) return;   /* origem de difusão ou multicast: inválida */
    const uint8_t *c = h + cab;
    uint32_t t = total - cab;
    if (h[9] == IP_ICMP && (meu_ip || do_lo)) icmp_chegou(origem, destino, h[8], c, t);
    else if (h[9] == IP_UDP) {
        /* D363: el DHCP solo por la física (por el túnel cambiaría la dirección y la puerta de enlace) */
        if (de == DE_TUNEL && (t < 4 || be16(c + 2) == PORTA_DHCP_CLIENTE)) { tunel_descarte(origem, destino); return; }
        udp_chegou(origem, destino, c, t);
    }
    else if (h[9] == IP_TCP && (eh_meu(destino) || para_lo)) tcp_receber(origem, destino, c, t);
}

/* D437: se vacía la tarjeta en cada vuelta, con un tope (un anillo) para no tener el sistema
 * parado; si se llega al tope, la tarea de la red vuelve enseguida (cediendo antes el turno)
 * en vez de esperar al tic siguiente. Estadística para «[REDE] rx ...» (cada 10 s con tráfico).
 * D445: una vez por cada interfaz encendida. */
#define RX_POR_VOLTA   64
#define VOLTAS_SEGUIDAS 4
static uint32_t est_voltas, est_quadros, est_max_volta, est_topes;

static int processar_chegadas(iface_t *f)
{
    if (!f->ligada) return 0;
    placa_t *p = &f->placa;
    rx_if = f;
    int k;
    for (k = 0; k < RX_POR_VOLTA; k++) {
        int n = p->receber(chegada);
        if (n == 0) break;
        if (n < 14 || n > ETH_MAX) { p->rx_erros++; continue; }
        if (f->desativada) { descartar(DESC_DESATIVADA, NULL, 0); continue; }   /* desligada: esvazia o anel e joga fora */
        p->rx_pacotes++;
        p->rx_bytes += (uint32_t)n;
        total_rx += (uint32_t)n;
        /* Só quadros para o nosso MAC ou de difusão (a placa já filtra, mas
         * confere; o multicast a placa deixa passar e morre aqui). */
        if (memcmp(chegada, p->mac, 6) != 0 && memcmp(chegada, difusao, 6) != 0) {
            descartar((chegada[0] & 1) ? DESC_MULTICAST : DESC_MAC_ALHEIA, chegada, (uint32_t)n);
            continue;
        }
        uint16_t tipo = be16(chegada + 12);
        if (tipo == ETH_ARP) arp_chegou(f, chegada + 14, (uint32_t)n - 14);
        else if (tipo == ETH_IP) ip_chegou(chegada + 14, (uint32_t)n - 14, DE_CABO);
        else {
            descarte_ultimo_tipo = tipo;
            descartar(tipo == 0x86DD ? DESC_IPV6 : (tipo == 0x8100 || tipo == 0x88A8) ? DESC_VLAN : DESC_TIPO,
                      chegada, (uint32_t)n);
        }
    }
    rx_if = NULL;
    if (k) {
        est_voltas++;
        est_quadros += (uint32_t)k;
        if ((uint32_t)k > est_max_volta) est_max_volta = (uint32_t)k;
        if (k == RX_POR_VOLTA) est_topes++;
    }
    return k;
}

/* ------------------------------------------------------------ interrupção e tarefa */

/* D445: una sola función para todas las líneas de las tarjetas: pregunta a cada tarjeta con IRQ
 * si la causa es suya (dos tarjetas pueden compartir línea, y leer la causa de la que no es la
 * de esta línea solo adelanta su trabajo). */
static quadro_t *ao_irq(quadro_t *q)
{
    bool alguma = false, mais = false;
    for (int i = 0; i < REDE_IFS; i++) {
        iface_t *f = &ifs[i];
        if (!f->ligada || !f->usa_irq || !f->placa.atender || !f->placa.atender()) continue;
        alguma = true;
        f->placa.irqs++;
        if (processar_chegadas(f) == RX_POR_VOLTA) mais = true;
    }
    if (!alguma) {
        /* Linha compartilhada com algo que ninguém atende: se virar uma
         * tempestade, desiste da IRQ e fica só na sondagem. */
        /* D338: la que reconoció otro atendedor de la línea (el xHCI, el sonido) no cuenta: con un
         * pendrive en un xHCI que comparte la IRQ, 5000 órdenes USB dejaban la línea enmascarada
         * y el USB sin interrupciones. */
        if (!irq_atendida_por_outro() && ++irq_alheias > 5000 && linhas_nossas) {
            for (int l = 0; l < 16; l++)
                if (linhas_nossas & (1u << l)) {
                    irq_desabilitar((uint8_t)l);
                    klog("[REDE] IRQ %d sem causa da placa: passando a sondagem\n", l);
                }
            linhas_nossas = 0;
            for (int i = 0; i < REDE_IFS; i++) ifs[i].usa_irq = false;
        }
        return q;
    }
    if (mais) acordar(&canal_rede);   /* D437: queda más: la tarea sigue */
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
    trafego_rx[trafego_qtd] = total_rx - trafego_rx_ant;
    trafego_tx[trafego_qtd] = total_tx - trafego_tx_ant;
    trafego_qtd++;
    trafego_rx_ant = total_rx;
    trafego_tx_ant = total_tx;
}

/* D437: cada 10 s con tráfico, una línea en el registro para medir en una máquina real (dmesg). */
static void registrar_trafego(void)
{
    static uint32_t marca_rx, marca_tx, marca_voltas, marca_quadros, marca_topes, segundos;
    if (++segundos < 10) return;
    segundos = 0;
    uint32_t drx = total_rx - marca_rx, dtx = total_tx - marca_tx;
    bool sondeo = false;
    for (int i = 0; i < REDE_IFS; i++) if (ifs[i].ligada && !ifs[i].usa_irq) sondeo = true;
    if (drx + dtx >= 64 * 1024 && alguma_ligada()) {
        uint32_t v = est_voltas - marca_voltas, q = est_quadros - marca_quadros;
        klog("[REDE] rx %u KB/s, tx %u KB/s; %u tramas en %u vueltas (media %u, hasta %u por vuelta; tope %u: %u veces)%s\n",
             drx / 10240, dtx / 10240, q, v, v ? q / v : 0, est_max_volta, RX_POR_VOLTA, est_topes - marca_topes,
             sondeo ? ", por sondeo" : "");
        est_max_volta = 0;
    }
    marca_rx = total_rx; marca_tx = total_tx;
    marca_voltas = est_voltas; marca_quadros = est_quadros; marca_topes = est_topes;
}

/* D445: el enlace de cada interfaz (para elegir la ruta por defecto sin leer la tarjeta en cada
 * paquete) y los cambios de la ruta por defecto, al registro (solo con más de una interfaz: con
 * una, el registro es el de siempre). */
static void atualizar_enlaces(void)
{
    static iface_t *padrao_ant;
    static uint32_t roteador_ant;
    static int n_ant;
    char px[12];
    for (int i = 0; i < REDE_IFS; i++) {
        iface_t *f = &ifs[i];
        if (!f->ligada) continue;
        bool e = f->placa.cabo();
        if (e != f->enlace && pref_if(f, px, sizeof px)[0])
            klog("[REDE] %s: enlace %s\n", f->nome, e ? "activo" : "caído");
        f->enlace = e;
    }
    iface_t *p = if_padrao();
    int n = 0;
    for (int i = 0; i < REDE_IFS; i++) if (ifs[i].ligada) n++;
    if (p != padrao_ant || (p && p->roteador != roteador_ant)) {
        if (n > 1 || n_ant > 1) {   /* con una sola interfaz (antes y ahora), el registro de siempre */
            char r[16];
            if (p) { ip_texto(r, sizeof r, p->roteador); klog("[REDE] ruta por defecto: %s (puerta %s)\n", p->nome, r); }
            else klog("[REDE] sin ruta por defecto\n");
        }
        if (padrao_ant && p != padrao_ant) rede_dns_limpar();   /* otra red: otro DNS */
        padrao_ant = p;
        roteador_ant = p ? p->roteador : 0;
    }
    n_ant = n;
}

static void tarefa_rede(void *arg)
{
    (void)arg;
    uint32_t proximo_segundo = agora() + TIQUES_POR_SEGUNDO;
    int seguidas = 0;
    for (;;) {
        int_desliga();
        atualizar_enlaces();
        int chegaram = 0;
        for (int i = 0; i < REDE_IFS; i++)
            if (processar_chegadas(&ifs[i]) == RX_POR_VOLTA) chegaram = RX_POR_VOLTA;
        lo_processar();
        tunel_processar();
        arp_tique();
        for (int i = 0; i < REDE_IFS; i++) dhcp_tique(&ifs[i]);
        tcp_tique();
        if (passou(proximo_segundo)) {
            irq_alheias = 0;
            proximo_segundo = agora() + TIQUES_POR_SEGUNDO;
            amostrar_trafego();
            registrar_trafego();
        }
        if (chegaram == RX_POR_VOLTA && seguidas < VOLTAS_SEGUIDAS) {
            /* D437: la tarjeta tenía más: otra vuelta en cuanto los demás hayan tenido su turno. */
            seguidas++;
            ceder();
            int_liga();
            continue;
        }
        seguidas = 0;
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

/* D433: Ethernet USB (kernel/rede/ure.c). El USB arranca antes que la pila: su placa se guarda aquí y
 * se activa al llegar a rede_iniciar() (detrás de las PCI, para que la eth0 sea la integrada) o en el
 * momento si la pila ya está en marcha. D445: es una interfaz más (eth0 sin tarjeta PCI, si no eth1),
 * a la vez que la PCI y la Wi-Fi; usb_if = su hueco (NULL = sin adaptador en la pila). */
static placa_t placa_usb;
static bool usb_tem, rede_arrancou;
static iface_t *usb_if;
static void ativar_usb(void);

/* D211: la autoprueba de la IRQ (irq_rotas.c) cuenta las interrupciones
 * de la tarjeta que su driver reconoció (ctx = su interfaz). */
static bool sonda_provocar(void *ctx)
{
    iface_t *f = ctx;
    return f->ligada && f->usa_irq && f->placa.provocar_irq && f->placa.provocar_irq();
}
static uint32_t sonda_contador(void *ctx) { return ((iface_t *)ctx)->placa.irqs; }
static void sonda_terminar(void *ctx, bool chegou) { (void)ctx; (void)chegou; }

static void ligar_placa(iface_t *f)
{
    f->ligada = true;
    f->so_detectada = false;
    f->usa_irq = false;
    int irq = f->placa.irq;
    if (irq > 0 && irq < 16 && irq != 2) {
        if (linhas_nossas & (1u << irq)) f->usa_irq = true;   /* D445: la línea ya es de otra tarjeta nuestra */
        else if (!irq_ocupada((uint8_t)irq)) {
            irq_registrar((uint8_t)irq, ao_irq);
            linhas_nossas |= (uint16_t)(1u << irq);
            f->usa_irq = true;
        }
    }
    if (f->usa_irq && f->placa.provocar_irq && !f->sonda) {
        irq_sonda_t s = { f->placa.nome, f->placa.pci, (uint8_t)irq, sonda_provocar, sonda_contador,
                          sonda_terminar, f };
        irq_sonda_registrar(&s);
        f->sonda = true;
    }
    f->enlace = f->placa.cabo();
    bool eth0 = txt_compara(f->nome, "eth0") == 0;
    klog("[REDE] placa %s (%s), %s%s%s\n", f->placa.nome, f->placa.descricao,
         f->usa_irq ? "com interrupcao" : "por sondagem a cada tique", eth0 ? "" : ", ", eth0 ? "" : f->nome);
    ligar_tarefa();
}

/* Una tarjeta PCI encontrada: su hueco (eth0, eth1...) y el driver en marcha. */
static void adicionar_pci(const placa_t *p)
{
    iface_t *f = if_nova_eth(IF_PCI);
    if (!f) { klog("[REDE] %s: sin hueco para otra interfaz (hay %d)\n", p->descricao, REDE_IFS); return; }
    f->placa = *p;
    ligar_placa(f);
}

static void reiniciar_ip(iface_t *f)
{
    f->ip = 0; f->mascara = 0; f->roteador = 0; f->dns = 0;
    f->metodo = REDE_SEM_IP;
    f->dhcp_estado = DHCP_PARADO;
    f->dhcp_tentativas = 0;
    arp_esquecer(f);
    rede_dns_limpar();
}

/* La interfaz desaparece (adaptador USB o pincho Wi-Fi retirados). */
static void iface_remover(iface_t *f)
{
    f->ligada = false;
    reiniciar_ip(f);
    if (rx_if == f) rx_if = NULL;
    if (usb_if == f) usb_if = NULL;
    memset(f, 0, sizeof *f);   /* tipo = IF_LIVRE */
}

void rede_iniciar(void)
{
    bool i0 = int_salva_desliga();
    placa_t placa;
    bool so_usb = false;
#ifdef SAGUI_URE_PREFERIR
    /* D433, solo para probar: con un Ethernet USB Realtek enchufado al arrancar, la red PCI (y la ISA) no se
     * buscan, para que el USB sea la eth0 en un equipo con red integrada (el Amilo). Se compila con
     * «make URE_PREFERIR=1»; sin adaptador, todo sigue igual. D445: ya no hace falta para usar los dos
     * (la PCI es la eth0 y el USB la eth1); sirve para probar el USB solo. */
    so_usb = ure_usb_visto();
    if (so_usb) klog("[REDE] URE_PREFERIR: hay un Ethernet USB Realtek; no se buscan tarjetas PCI ni ISA\n");
#endif
    if (!so_usb) {
        /* D445: todas las tarjetas que haya (una por driver), cada una con su interfaz. */
        memset(&placa, 0, sizeof placa);
        if (e1000_procurar(&placa)) adicionar_pci(&placa);
        memset(&placa, 0, sizeof placa);
        if (rtl8139_procurar(&placa)) adicionar_pci(&placa);
        memset(&placa, 0, sizeof placa);
        if (ne2000_procurar(&placa)) adicionar_pci(&placa);
        /* D200: el driver experimental de la tarjeta detectada (r8169 o alc), para
         * encenderlo a pedido con REDE_EXPERIMENTAL. */
        bool (*procurar_experimental)(placa_t *p, bool ativar) = NULL;
        memset(&placa, 0, sizeof placa);
        if (r8169_procurar(&placa, false)) procurar_experimental = r8169_procurar;
        else {
            memset(&placa, 0, sizeof placa);
            if (alc_procurar(&placa, false)) procurar_experimental = alc_procurar;   /* D200: Atheros AR813x/AR815x */
        }
        iface_t *f = procurar_experimental ? if_nova_eth(IF_PCI) : NULL;
        if (f) {
            f->procurar_experimental = procurar_experimental;
            if (!placa.experimental) {
                /* D091: revisão já testada em hardware real: liga direto. */
                memset(&placa, 0, sizeof placa);
                if (f->procurar_experimental(&placa, true)) { f->placa = placa; ligar_placa(f); }
                else memset(f, 0, sizeof *f);
            } else {
                f->placa = placa;
                f->so_detectada = true;
                klog("[REDE] %s detectada: driver experimental desligado (ifconfig %s experimental)\n", placa.descricao, f->nome);
            }
        }
    }
    rede_arrancou = true;
    ativar_usb();                        /* D433: el Ethernet USB, si lo hay (detrás de las PCI) */
    bool cabeada = false;
    for (int i = 0; i < REDE_IFS; i++) if (ifs[i].tipo == IF_PCI || ifs[i].tipo == IF_USB) cabeada = true;
    if (!cabeada && !so_usb) {   /* D329: último recurso, el sondeo de puertos de la NE2000 ISA */
        memset(&placa, 0, sizeof placa);
        if (ne2000_isa_procurar(&placa)) adicionar_pci(&placa);
    }
    ligar_tarefa();   /* mesmo sem placa: o loopback (127.0.0.1) precisa dela */
    wifi_pci_iniciar();   /* D280: Wi-Fi integrada Atheros AR9485 (PCIe), si la hay */
    int_restaura(i0);
}

/* A linha da tela de boot (sem o "[ ok ]"): la de la interfaz principal y, con más, cuántas. */
void rede_linha_boot(char *s, int max, bool *aviso)
{
    char ip[16];
    *aviso = false;
    iface_t *f = if_principal();
    if (f && f->so_detectada) {
        snformatar(s, (size_t)max, "Red: %s %04x:%04x sin activar (ifconfig %s experimental)",
                   f->placa.pci.fabricante == 0x1969 ? "Atheros" : "Realtek", f->placa.pci.fabricante, f->placa.pci.modelo,
                   f->nome);
        *aviso = true;
        return;
    }
    if (!f || !f->ligada) { s[0] = 0; return; }
    placa_t *p = &f->placa;
    int k = snformatar(s, (size_t)max, "Red: %s %02x:%02x:%02x:%02x:%02x:%02x, ", p->nome, p->mac[0], p->mac[1],
                       p->mac[2], p->mac[3], p->mac[4], p->mac[5]);
    if (f->ip) {
        ip_texto(ip, sizeof ip, f->ip);
        k += snformatar(s + k, (size_t)(max - k), "%s (%s)", ip, f->metodo == REDE_ESTATICO ? "estática" : "DHCP");
    } else if (!p->cabo()) {
        k += snformatar(s + k, (size_t)(max - k), "sin cable");
        *aviso = true;
    } else {
        k += snformatar(s + k, (size_t)(max - k), "pidiendo IP por DHCP...");
    }
    int outras = 0;
    for (int i = 0; i < REDE_IFS; i++) if (ifs[i].ligada && &ifs[i] != f) outras++;
    if (outras && k < max) snformatar(s + k, (size_t)(max - k), "; +%d (ifconfig)", outras);
}

/* ------------------------------------------------------------ configuração */

/* D091: as linhas de diagnóstico do "ip": motivos dos descartes da pilha,
 * os primeiros quadros descartados e o que o driver tiver a dizer. Só para
 * o driver experimental, para o "ip" das placas que funcionam não mudar. */
static void diagnostico_preencher(iface_t *f, char *s, uint32_t max)
{
    s[0] = 0;
    if (!f->ligada || !f->placa.experimental) return;
    uint32_t k = 0, soma = 0;
#define POE(...) do { if (k < max) { int _r = snformatar(s + k, max - k, __VA_ARGS__); k += _r > 0 ? (uint32_t)_r : 0; } } while (0)
    POE("Descartes en la pila:");
    for (int i = 0; i < DESC_MOTIVOS; i++) {
        soma += descartes[i];
        if (descartes[i]) POE(" %s %u%s", nome_descarte[i], descartes[i], i == DESC_TIPO ? "" : ",");
        if (i == DESC_TIPO && descartes[i]) POE(" (último %04x),", descarte_ultimo_tipo);
    }
    POE(" otros %u\n", f->placa.descartados >= soma ? f->placa.descartados - soma : 0);
    for (uint32_t i = 0; i < descartes_registrados && i < DESC_GUARDADOS; i++)
        POE("  descarte %u: %s, destino %02x:%02x:%02x:%02x:%02x:%02x tipo %04x, %u bytes\n", i + 1,
            nome_descarte[descarte_quadros[i].motivo], descarte_quadros[i].destino[0], descarte_quadros[i].destino[1],
            descarte_quadros[i].destino[2], descarte_quadros[i].destino[3], descarte_quadros[i].destino[4],
            descarte_quadros[i].destino[5], descarte_quadros[i].tipo, descarte_quadros[i].n);
    if (f->placa.diagnostico && k < max) f->placa.diagnostico(s + k, max - k);
#undef POE
}

/* f NULL = sin tarjeta: «eth0» vacía, como antes. */
static void info_preencher(iface_t *f, info_rede_t *v, bool com_diagnostico)
{
    memset(v, 0, sizeof *v);
    if (!f) { txt_copia_n(v->nome, "eth0", sizeof v->nome); v->irq = 0xFF; return; }
    placa_t *p = &f->placa;
    txt_copia_n(v->nome, f->nome, sizeof v->nome);
    txt_copia_n(v->driver, p->nome ? p->nome : "", sizeof v->driver);
    txt_copia_n(v->descricao, p->descricao ? p->descricao : "", sizeof v->descricao);
    memcpy(v->mac, p->mac, 6);
    /* la wlan0 sin conectar: el pincho está (ativa) pero no tiene dirección */
    v->ativa = f->ligada || f->tipo == IF_WIFI;
    v->cabo = (f->ligada || f->tipo == IF_WIFI) && p->cabo && p->cabo();
    v->experimental = p->experimental;
    v->irq = f->usa_irq ? (uint8_t)p->irq : 0xFF;
    v->fabricante = p->pci.fabricante;
    v->modelo = p->pci.modelo;
    v->barramento = p->pci.barramento;
    v->dispositivo = p->pci.dispositivo;
    v->funcao = p->pci.funcao;
    v->dhcp_estado = (uint8_t)f->dhcp_estado;
    v->ip = f->ip; v->mascara = f->mascara; v->roteador = f->roteador; v->dns = f->dns;
    v->metodo = f->metodo;
    v->servidor_dhcp = f->servidor_dhcp;
    if (f->metodo == REDE_POR_DHCP) {
        uint32_t passado = (agora() - f->dhcp_inicio_concessao) / TIQUES_POR_SEGUNDO;
        v->concessao_s = passado < f->dhcp_concessao ? f->dhcp_concessao - passado : 0;
    }
    v->rx_pacotes = p->rx_pacotes; v->tx_pacotes = p->tx_pacotes;
    v->rx_bytes = p->rx_bytes;     v->tx_bytes = p->tx_bytes;
    v->rx_erros = p->rx_erros;     v->tx_erros = p->tx_erros;
    v->descartados = p->descartados; v->irqs = p->irqs;
    v->desativada = f->desativada;
    v->pode_desativar = f->ligada;
    v->metrica = f->metrica;
    v->rota_padrao = f == if_padrao();
    v->usb = f->tipo == IF_USB || f->tipo == IF_WIFI;
    v->wifi = f->tipo == IF_WIFI;
    if (com_diagnostico) diagnostico_preencher(f, v->diagnostico, sizeof v->diagnostico);
}

/* D363: un paquete que el demonio del túnel descifró (tunel_processar, en la tarea de la red). */
void rede_tunel_chegou(const uint8_t *p, uint32_t n)
{
    static uint8_t copia[1500];   /* ip_chegou puede responder (y montar) en "quadro", nunca aquí */
    if (n > sizeof copia) return;
    memcpy(copia, p, n);
    ip_chegou(copia, n, DE_TUNEL);
}

void rede_dns_limpar(void)
{
    for (int i = 0; i < DNS_CACHE; i++) dns_cache[i].usado = false;
}

/* D238: lo llama wifi.c al dejar tramas en su cola de llegada. */
void rede_acordar(void)
{
    acordar(&canal_rede);
}

static iface_t *if_wifi(void)
{
    for (int i = 0; i < REDE_IFS; i++) if (ifs[i].tipo == IF_WIFI) return &ifs[i];
    return NULL;
}

/* La placa nueva de la wlan0 conservando los contadores de la interfaz. */
static void placa_trocar(iface_t *f, const placa_t *p)
{
    placa_t ant = f->placa;
    f->placa = *p;
    f->placa.rx_pacotes = ant.rx_pacotes; f->placa.tx_pacotes = ant.tx_pacotes;
    f->placa.rx_bytes = ant.rx_bytes;     f->placa.tx_bytes = ant.tx_bytes;
    f->placa.rx_erros = ant.rx_erros;     f->placa.tx_erros = ant.tx_erros;
    f->placa.descartados = ant.descartados; f->placa.irqs = ant.irqs;
}

/* D215/D445: la wlan0 tiene su hueco desde que el pincho (o la AR9485) está listo; p NULL = se retiró. */
void rede_wifi_registrar(placa_t *p)
{
    bool i0 = int_salva_desliga();
    iface_t *f = if_wifi();
    if (!p) {
        if (f) {
            if (f->ligada) klog("[REDE] wlan0 desconectada\n");
            iface_remover(f);
        }
    } else {
        if (!f) {
            f = if_livre();
            if (f) {
                f->tipo = IF_WIFI;
                txt_copia_n(f->nome, "wlan0", sizeof f->nome);
                f->metrica = METRICA_WIFI;
                f->metodo = REDE_SEM_IP;
            } else klog("[REDE] wlan0: sin hueco para otra interfaz (hay %d)\n", REDE_IFS);
        }
        if (f && !f->ligada) placa_trocar(f, p);
    }
    wlan = p;
    int_restaura(i0);
}

/* D445: conectada, la wlan0 se enciende a la vez que las demás (antes aparcaba la eth0). */
void rede_wifi_conectada(bool sim)
{
    bool i0 = int_salva_desliga();
    iface_t *f = if_wifi();
    if (sim && f && wlan && !f->ligada) {
        placa_trocar(f, wlan);
        f->desativada = false;
        reiniciar_ip(f);   /* el DHCP empieza solo en el próximo tique (la wlan0 tiene «cable») */
        f->ligada = true;
        f->usa_irq = false;
        f->enlace = f->placa.cabo();
        bool outra = false;
        for (int i = 0; i < REDE_IFS; i++) if (ifs[i].ligada && &ifs[i] != f) outra = true;
        klog("[REDE] wlan0 activa%s\n", outra ? " (junto con la red por cable)" : "");
    } else if (!sim && f && f->ligada) {
        f->ligada = false;
        reiniciar_ip(f);
        klog("[REDE] wlan0 desconectada%s\n", alguma_ligada() ? ": sigue la red por cable" : "");
    }
    int_restaura(i0);
    acordar(&canal_rede);
}

/* D433/D445: la placa USB como interfaz propia: eth0 sin tarjeta PCI, eth1 detrás de ella. */
static void ativar_usb(void)
{
    if (!usb_tem || usb_if) return;
    iface_t *f = if_nova_eth(IF_USB);
    if (!f) { klog("[REDE] %s (USB) sin usar: no queda hueco para otra interfaz\n", placa_usb.descricao); return; }
    f->placa = placa_usb;
    reiniciar_ip(f);
    f->ligada = true;
    f->usa_irq = false;               /* sin IRQ: sondeo a cada tic */
    f->enlace = f->placa.cabo();
    usb_if = f;
    bool eth0 = txt_compara(f->nome, "eth0") == 0;
    klog("[REDE] placa %s (%s), por sondeo (USB)%s%s\n", f->placa.nome, f->placa.descricao, eth0 ? "" : ", ",
         eth0 ? "" : f->nome);
}

void rede_usb_eth_registrar(placa_t *p)
{
    bool i0 = int_salva_desliga();
    if (!p) {
        if (usb_if) {
            iface_remover(usb_if);    /* la ruta por defecto pasa a otra interfaz, si la hay */
            klog("[REDE] Ethernet USB retirada\n");
        }
        usb_tem = false;
    } else {
        placa_usb = *p;
        usb_tem = true;
        if (rede_arrancou) ativar_usb();
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

/* D445: las interfaces en el orden en que se listan: eth0, eth1... y luego la wlan0. */
static int ifs_em_ordem(iface_t **v)
{
    int k = 0;
    for (int n = 0; n < 10; n++) {
        char nome[8];
        snformatar(nome, sizeof nome, "eth%d", n);
        for (int i = 0; i < REDE_IFS; i++)
            if (ifs[i].tipo != IF_LIVRE && ifs[i].tipo != IF_WIFI && txt_compara(ifs[i].nome, nome) == 0) v[k++] = &ifs[i];
    }
    for (int i = 0; i < REDE_IFS; i++) if (ifs[i].tipo == IF_WIFI) v[k++] = &ifs[i];
    return k;
}

int32_t rede_info(uint32_t tipo, uint32_t ptr, uint32_t n)
{
    if (tipo == INFO_REDE) {
        /* D079: info_rede_t cresceu no fim; quem foi compilado antes pede
         * menos bytes (n = o sizeof dele) e recebe só a parte antiga.
         * D445: es la interfaz principal (la de la ruta por defecto). */
        uint32_t tam = (n && n < sizeof(info_rede_t)) ? n : sizeof(info_rede_t);
        if (!usuario_ok((void *)ptr, tam, true)) return ERRO_ENDERECO;
        static info_rede_t v;   /* 3,2 KB: fora da pilha do kernel */
        iface_t *f = if_principal();
        info_preencher(f, &v, true);
        memcpy((void *)ptr, &v, tam);
        return f != NULL;
    }
    if (tipo == INFO_INTERFACES) {
        info_rede_t *v = (info_rede_t *)ptr;
        if (n > REDE_IFS + 2) n = REDE_IFS + 2;
        if (!usuario_ok(v, n * sizeof *v, true)) return ERRO_ENDERECO;
        iface_t *ord[REDE_IFS];
        int q = ifs_em_ordem(ord);
        uint32_t k = 0;
        for (int i = 0; i < q && k < n; i++) info_preencher(ord[i], &v[k++], true);
        if (k < n && tunel_ativo()) tunel_info_rede(&v[k++]);   /* D363 */
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
        iface_t *ord[REDE_IFS];
        int q = ifs_em_ordem(ord);
        uint32_t k = 0;
        for (int i = 0; i < ARP_MAX && k < n; i++) {
            if (arp[i].estado == ARP_LIVRE) continue;
            v[k].ip = arp[i].ip;
            memcpy(v[k].mac, arp[i].mac, 6);
            v[k].completa = arp[i].estado == ARP_COMPLETA;
            v[k].interface = 0;   /* D445: la posición de su interfaz en INFO_INTERFACES */
            for (int j = 0; j < q; j++) if (ord[j] == arp[i].f) v[k].interface = (unsigned char)j;
            v[k].idade_s = (agora() - arp[i].quando) / TIQUES_POR_SEGUNDO;
            k++;
        }
        return (int32_t)k;
    }
    return ERRO_INVALIDO;
}

/* D445: la interfaz por su nombre. «eth0» con una sola interfaz es esa, se llame como se llame
 * (la wlan0 sola, como antes, cuando era la única activa). */
static iface_t *if_por_nome(const char *nome)
{
    int n = 0;
    iface_t *unica = NULL;
    for (int i = 0; i < REDE_IFS; i++) {
        if (ifs[i].tipo == IF_LIVRE) continue;
        if (txt_compara(ifs[i].nome, nome) == 0) return &ifs[i];
        n++;
        unica = &ifs[i];
    }
    if (n == 1 && txt_compara(nome, "eth0") == 0) return unica;
    return NULL;
}

/* SIS_CONFIG_REDE (op, &config_rede_t, interfaz). D445: el tercer argumento es el nombre de la
 * interfaz ("eth1") o 0 = la principal (lo de siempre, para quien no lo pasa). */
int32_t sis_config_rede(uint32_t op, uint32_t ptr, uint32_t c, uint32_t d, uint32_t e_)
{
    (void)d; (void)e_;
    config_rede_t *cfg = (config_rede_t *)ptr;
    bool sem_ponteiro = op == REDE_DHCP || op == REDE_EXPERIMENTAL || op == REDE_RENOVAR || op == REDE_DESATIVAR ||
                        op == REDE_ATIVAR || op == REDE_FECHAR || op == REDE_AJUSTAR_DRIVER || op == REDE_TUNEL_DESLIGAR ||
                        op == REDE_METRICA;
    if (!sem_ponteiro && !usuario_ok(cfg, sizeof *cfg, op == REDE_CONSULTAR)) return ERRO_ENDERECO;
    iface_t *f;
    bool com_nome = c != 0;
    if (com_nome) {
        char nome[16];
        int r = usuario_texto((const char *)c, nome, sizeof nome);
        if (r < 0) return r;
        f = if_por_nome(nome);
        if (!f) return ERRO_NAO_EXISTE;
    } else f = if_principal();
    char px[12];
    switch (op) {
    case REDE_CONSULTAR:
        if (!f) { cfg->ip = cfg->mascara = cfg->roteador = cfg->dns = 0; cfg->metodo = REDE_SEM_IP; return 0; }
        cfg->ip = f->ip; cfg->mascara = f->mascara; cfg->roteador = f->roteador; cfg->dns = f->dns;
        cfg->metodo = f->metodo;
        return 0;
    case REDE_FIXAR: {
        if (!f || !f->ligada) return ERRO_SEM_REDE;
        uint32_t ip = cfg->ip, m = cfg->mascara;
        if (ip == 0 || ip == IP_TODOS || (ip >> 28) >= 14 || (ip >> 24) == 127) return ERRO_INVALIDO;
        if (m == 0 || (~m & (~m + 1)) != 0) return ERRO_INVALIDO;   /* máscara: uns seguidos de zeros */
        if (cfg->roteador && ((cfg->roteador ^ ip) & m)) return ERRO_INVALIDO;   /* roteador fora da rede */
        iface_t *outra = if_do_ip(ip);
        if (outra && outra != f) return ERRO_JA_EXISTE;   /* D445: la misma dirección en dos interfaces */
        f->ip = ip; f->mascara = m; f->roteador = cfg->roteador;
        if (cfg->dns) f->dns = cfg->dns;
        f->metodo = REDE_ESTATICO;
        f->dhcp_estado = DHCP_PARADO;
        arp_esquecer(f);
        arp_enviar(f, 1, NULL, f->ip);   /* ARP gratuito */
        klog("[REDE] %sendereco estatico configurado\n", pref_if(f, px, sizeof px));
        return 0;
    }
    case REDE_DNS:
        if (!cfg->dns || cfg->dns == IP_TODOS) return ERRO_INVALIDO;
        if (!f) return ERRO_SEM_REDE;
        f->dns = cfg->dns;
        rede_dns_limpar();
        return 0;
    case REDE_DHCP: {
        espera_t *e = espera_minha(ESP_DHCP);
        if (!e) {
            if (!f || !f->ligada) return ERRO_SEM_REDE;
            e = espera_nova(ESP_DHCP, 10000);
            if (!e) return ERRO_LIMITE;
            e->ip = (uint32_t)(f - ifs);   /* avisar_dhcp despierta solo a los de esta interfaz */
            if (f->metodo == REDE_ESTATICO) { f->ip = 0; f->metodo = REDE_SEM_IP; arp_esquecer(f); }
            dhcp_renovar_ja(f);
        }
        return espera_continuar(e, 0);
    }
    case REDE_RENOVAR:
        /* D079: o "Renovar DHCP" do Administrador de tareas não pode ficar
         * 10 s parado esperando como o comando dhcp: só dispara. */
        if (!f || !f->ligada) return ERRO_SEM_REDE;
        if (f->desativada) return ERRO_OCUPADO;
        if (f->metodo == REDE_ESTATICO) { f->ip = 0; f->metodo = REDE_SEM_IP; arp_esquecer(f); }
        dhcp_renovar_ja(f);
        klog("[REDE] %sDHCP: renovacao pedida\n", pref_if(f, px, sizeof px));
        return 0;
    case REDE_DESATIVAR:
    case REDE_ATIVAR:
        if (!f || !f->ligada) return ERRO_SEM_REDE;
        if (f->desativada == (op == REDE_DESATIVAR)) return 0;
        f->desativada = op == REDE_DESATIVAR;
        klog("[REDE] %s %s\n", f->nome, f->desativada ? "desativada" : "ativada");
        if (!f->desativada && f->metodo == REDE_POR_DHCP) {   /* volta: confirma o endereço com o servidor */
            f->dhcp_estado = DHCP_RENOVANDO;
            f->dhcp_xid++;
            f->dhcp_proxima = agora() + 2 * TIQUES_POR_SEGUNDO;
            dhcp_pedir(f);
        }
        acordar(&canal_rede);   /* la ruta por defecto se vuelve a mirar ya */
        return 0;
    case REDE_METRICA:
        /* D445: la métrica de la ruta por defecto de la interfaz (en el lugar del puntero). */
        if (!f) return ERRO_SEM_REDE;
        if (ptr > 9999) return ERRO_INVALIDO;
        f->metrica = ptr;
        klog("[REDE] %s: metrica %u\n", f->nome, f->metrica);
        acordar(&canal_rede);
        return 0;
    case REDE_FECHAR:
        return tcp_abortar(ptr);
    case REDE_TUNEL_DESLIGAR:
        return tunel_pedir_desligar();
    case REDE_AJUSTAR_DRIVER: {
        /* D124: ajustes en caliente del driver experimental, para probar
         * variantes en el hardware real sin recompilar. */
        if (!usuario_ok((void *)ptr, sizeof(ajuste_driver_t), false)) return ERRO_ENDERECO;
        if (!f || !f->ligada) return ERRO_SEM_REDE;
        if (!f->placa.ajustar) return ERRO_INVALIDO;
        ajuste_driver_t a = *(const ajuste_driver_t *)ptr;
        bool i = int_salva_desliga();
        int32_t r = f->placa.ajustar(&a);
        int_restaura(i);
        klog("[REDE] ajuste do driver (mascara %x): %d\n", a.mascara, r);
        return r;
    }
    case REDE_EXPERIMENTAL: {
        /* sin nombre: la detectada (con una sola tarjeta, la de siempre) */
        if (!com_nome)
            for (int i = 0; i < REDE_IFS; i++) if (ifs[i].so_detectada) { f = &ifs[i]; break; }
        if (!f) return ERRO_NAO_EXISTE;
        if (f->ligada) return f->placa.experimental ? 0 : ERRO_JA_EXISTE;
        if (!f->so_detectada || !f->procurar_experimental) return ERRO_NAO_EXISTE;
        placa_t placa;
        memset(&placa, 0, sizeof placa);
        if (!f->procurar_experimental(&placa, true)) return ERRO_SEM_MEMORIA;
        f->placa = placa;
        ligar_placa(f);
        return 0;
    }
    }
    return ERRO_INVALIDO;
}

/* Para la bandeja de la barra (D114): el mismo resumen de INFO_REDE sin pasar por la syscall. */
bool rede_bandeja(info_rede_t *v)
{
    iface_t *f = if_principal();
    info_preencher(f, v, false);   /* D124: sin el diagnóstico (lee todos los búferes del r8169) */
    return f != NULL;
}

void rede_renovar_dhcp(void) { sis_config_rede(REDE_RENOVAR, 0, 0, 0, 0); }
