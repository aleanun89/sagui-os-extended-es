// SPDX-License-Identifier: GPL-3.0-only
/* socket.c - sockets: a rede vista pelos programas. Ver D046.
 *
 * Um socket é um descritor de arquivo comum (tipo A_SOCKET em arquivos.c):
 * ler, escrever, fechar e disponivel funcionam nele. As chamadas novas
 * (SIS_SOCKET a SIS_OPCAO_SOCKET, 80-89) criam, conectam, escutam etc.
 *
 * Esperas: seguem o padrão das chamadas reiniciáveis (D010). Quem teria de
 * esperar bloqueia num "canal" (a conexão TCP, ou o próprio socket UDP) e a
 * chamada roda de novo quando a rede acorda esse canal. O tempo limite
 * (OPC_TEMPO_LIMITE) vira um prazo guardado no socket: a primeira espera o
 * marca, a que acorda depois dele devolve ERRO_TEMPO, e qualquer operação
 * que termina o apaga.
 *
 * Ctrl+C e kill: o processo é encerrado, arquivos_fechar_todos fecha o
 * descritor e socket_fechar solta a conexão (que manda o FIN, ou RST se
 * ainda havia dados por ler, e some sozinha). */
#include "rede.h"
#include "abi.h"
#include "console.h"
#include "memoria.h"
#include "processo.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"
#include "tunel.h"

#define UDP_FILA_MAX   32
#define UDP_BYTES_MAX  (64 * 1024)
#define UDP_DADOS_MAX  (ETH_MAX - 14 - 20 - 8)

typedef struct datagrama {
    struct datagrama *prox;
    uint32_t ip;
    uint16_t porta;
    uint32_t n;
    uint8_t  dados[];
} datagrama_t;

struct socket {
    int       tipo;              /* SOCK_TCP, SOCK_UDP, SOCK_TUN (D363) */
    bool      fisica;            /* UDP, OPC_FISICA (D363): sale siempre por la interfaz física */
    int       pid;
    uint32_t  tempo_ms;          /* 0 = sem limite */
    uint32_t  prazo;             /* tique em que a espera atual desiste (0 = nenhuma) */
    bool      conectando, escutando;
    bool      nao_bloquear;      /* D401: O_NONBLOCK: lo que tendría que esperar da ERRO_TENTAR */
    tcp_t    *tcp;               /* conexão ou escuta */
    /* UDP */
    uint16_t  porta_local;
    uint32_t  ip_destino;
    uint16_t  porta_destino;
    datagrama_t *primeiro, *ultimo;
    uint32_t  qtd, bytes;
    struct socket *prox;         /* lista dos sockets UDP */
};

static socket_t *udps;

static uint32_t agora(void) { return relogio_tiques(); }

static int32_t esperar(socket_t *s, const void *canal)
{
    if (s->nao_bloquear) return ERRO_TENTAR;   /* D401 */
    if (s->tempo_ms) {
        if (!s->prazo) s->prazo = agora() + (s->tempo_ms * TIQUES_POR_SEGUNDO + 999) / 1000;
        else if ((int32_t)(agora() - s->prazo) >= 0) { s->prazo = 0; return ERRO_TEMPO; }
        processo_atual()->acordar_em = s->prazo;
    }
    bloquear(canal);
    return SIS_REPETIR;
}

static int32_t pronto(socket_t *s, int32_t r)
{
    if (r != SIS_REPETIR) s->prazo = 0;
    return r;
}

/* ------------------------------------------------------------ UDP */

static bool udp_porta_usada(uint16_t p)
{
    for (socket_t *s = udps; s; s = s->prox)
        if (s->porta_local == p) return true;
    return false;
}

static uint16_t udp_porta_livre(void)
{
    static uint16_t proxima;
    for (int t = 0; t < 10000; t++) {
        uint16_t p = (uint16_t)(40000 + (proxima++ + agora()) % 10000);   /* 40000-49999; o DNS usa 50000+ */
        if (!udp_porta_usada(p)) return p;
    }
    return 0;
}

static int32_t udp_ligar(socket_t *s)
{
    if (s->porta_local) return 0;
    s->porta_local = udp_porta_livre();
    return s->porta_local ? 0 : ERRO_LIMITE;
}

void udp_chegou_socket(uint32_t origem, uint32_t destino, uint16_t po, uint16_t pd, const uint8_t *d, uint32_t n)
{
    (void)destino;
    for (socket_t *s = udps; s; s = s->prox) {
        if (s->porta_local != pd) continue;
        if (s->ip_destino && (s->ip_destino != origem || s->porta_destino != po)) return;   /* "conectado" a outro */
        if (s->qtd >= UDP_FILA_MAX || s->bytes + n > UDP_BYTES_MAX) { placa_t *p = rede_placa(); if (p) p->descartados++; return; }
        datagrama_t *g = kmalloc(sizeof *g + n);
        if (!g) return;
        g->prox = NULL;
        g->ip = origem;
        g->porta = po;
        g->n = n;
        memcpy(g->dados, d, n);
        if (s->ultimo) s->ultimo->prox = g; else s->primeiro = g;
        s->ultimo = g;
        s->qtd++;
        s->bytes += n;
        acordar(s);
        rede_atividade();
        return;
    }
}

static int32_t udp_receber(socket_t *s, char *buf, uint32_t n, endereco_rede_t *de)
{
    datagrama_t *g = s->primeiro;
    if (!g) return esperar(s, s);
    uint32_t k = g->n < n ? g->n : n;   /* o resto de um datagrama maior que o buffer se perde */
    memcpy(buf, g->dados, k);
    if (de) { de->ip = g->ip; de->porta = g->porta; }
    s->primeiro = g->prox;
    if (!s->primeiro) s->ultimo = NULL;
    s->qtd--;
    s->bytes -= g->n;
    kfree(g);
    return (int32_t)k;
}

static int32_t udp_mandar(socket_t *s, const char *buf, uint32_t n, uint32_t ip, uint32_t porta)
{
    if (!rede_pronta() && (ip >> 24) != 127) return ERRO_SEM_REDE;
    if (n > UDP_DADOS_MAX) return ERRO_INVALIDO;
    if (!ip || !porta || porta > 65535) return ERRO_INVALIDO;
    int32_t r = udp_ligar(s);
    if (r < 0) return r;
    memcpy(rede_carga() + 8, buf, n);
    rede_forcar_fisica = s->fisica;
    bool ok = udp_enviar(ip, s->porta_local, (uint16_t)porta, n);
    rede_forcar_fisica = false;
    if (!ok) return ERRO_INALCANCAVEL;
    return (int32_t)n;
}

int udp_listar(void *v, int max)
{
    info_conexao_t *o = v;
    int k = 0;
    for (socket_t *s = udps; s && k < max; s = s->prox) {
        if (!s->porta_local) continue;
        o[k].tipo = SOCK_UDP;
        o[k].estado = CON_UDP;
        o[k].ip_local = rede_meu_ip();
        o[k].porta_local = s->porta_local;
        o[k].ip_remoto = s->ip_destino;
        o[k].porta_remota = s->porta_destino;
        o[k].na_fila_rx = s->bytes;
        o[k].na_fila_tx = 0;
        o[k].pid = s->pid;
        k++;
    }
    return k;
}

/* ------------------------------------------------------------ operações de arquivo */

int32_t socket_ler(socket_t *s, char *buf, uint32_t n)
{
    if (s->tipo == SOCK_UDP) return pronto(s, udp_receber(s, buf, n, NULL));
    if (s->tipo == SOCK_TUN) {   /* D363: el siguiente paquete que la pila manda por el túnel */
        int32_t r = tunel_ler(s, buf, n);
        if (r == SIS_REPETIR) r = esperar(s, tunel_canal());
        return pronto(s, r);
    }
    if (!s->tcp || s->escutando || s->conectando) return ERRO_INVALIDO;
    int32_t r = tcp_ler(s->tcp, buf, n);
    if (r == SIS_REPETIR) r = esperar(s, s->tcp);
    return pronto(s, r);
}

int32_t socket_escrever(socket_t *s, const char *buf, uint32_t n)
{
    if (s->tipo == SOCK_TUN) return tunel_escrever(s, buf, n);
    if (s->tipo == SOCK_UDP) {
        if (!s->ip_destino) return ERRO_INVALIDO;   /* sem destino: use enviar_para */
        return udp_mandar(s, buf, n, s->ip_destino, s->porta_destino);
    }
    if (!s->tcp || s->escutando || s->conectando) return ERRO_INVALIDO;
    int32_t r = tcp_escrever(s->tcp, buf, n);
    if (r == SIS_REPETIR) r = esperar(s, s->tcp);
    return pronto(s, r);
}

int32_t socket_disponivel(socket_t *s)
{
    if (s->tipo == SOCK_UDP) return s->primeiro ? (int32_t)s->primeiro->n : 0;
    if (s->tipo == SOCK_TUN) return tunel_disponivel(s);
    if (!s->tcp || s->conectando) return 0;
    return tcp_disponivel(s->tcp);
}

/* D401: O_NONBLOCK (SIS_DESCRITOR). */
void socket_nao_bloquear(socket_t *s, bool sim)
{
    s->nao_bloquear = sim;
    s->prazo = 0;
}

/* D401: lo que poll ve en un socket (SONDA_*). */
uint32_t socket_sondar(socket_t *s)
{
    if (s->tipo == SOCK_UDP) return (s->primeiro ? SONDA_LER : 0) | SONDA_ESCREVER;
    if (s->tipo == SOCK_TUN) return (tunel_disponivel(s) > 0 ? SONDA_LER : 0) | SONDA_ESCREVER;
    if (!s->tcp) return s->conectando ? (SONDA_ERRO | SONDA_FECHADO | SONDA_ESCREVER) : 0;
    if (s->escutando) return tcp_disponivel(s->tcp) > 0 ? SONDA_LER : 0;
    int32_t sit = tcp_situacao(s->tcp);
    if (sit == 1) return 0;                                      /* conectando */
    if (sit < 0) return SONDA_ERRO | SONDA_FECHADO | SONDA_ESCREVER;
    if (s->conectando) return SONDA_ESCREVER;                    /* conectada: falta recoger el resultado */
    int32_t d = tcp_disponivel(s->tcp);
    if (d > 0) return SONDA_LER | SONDA_ESCREVER;
    if (d == ERRO_PIPE_QUEBRADO) return SONDA_LER | SONDA_FECHADO | SONDA_ESCREVER;
    if (d < 0) return SONDA_ERRO | SONDA_FECHADO;
    return SONDA_ESCREVER;
}

void socket_fechar(socket_t *s)
{
    if (s->tcp) tcp_soltar(s->tcp);
    if (s->tipo == SOCK_TUN) tunel_fechar(s);   /* D363: la interfaz, sus rutas y su DNS se van con el demonio */
    if (s->tipo == SOCK_UDP) {
        for (socket_t **p = &udps; *p; p = &(*p)->prox)
            if (*p == s) { *p = s->prox; break; }
        while (s->primeiro) { datagrama_t *g = s->primeiro; s->primeiro = g->prox; kfree(g); }
    }
    acordar(s);
    kfree(s);
}

/* ------------------------------------------------------------ chamadas */

static socket_t *pegar(uint32_t fd) { return arquivos_socket(fd); }

static socket_t *socket_novo(int tipo)
{
    socket_t *s = kcalloc(sizeof *s);
    if (!s) return NULL;
    s->tipo = tipo;
    s->pid = processo_atual()->pid;
    if (tipo == SOCK_UDP) { s->prox = udps; udps = s; }
    return s;
}

/* SIS_SOCKET (tipo) -> fd */
int32_t sis_socket(uint32_t tipo, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    if (tipo != SOCK_TCP && tipo != SOCK_UDP && tipo != SOCK_TUN) return ERRO_INVALIDO;
    socket_t *s = socket_novo((int)tipo);
    if (!s) return ERRO_SEM_MEMORIA;
    if (tipo == SOCK_TUN) {   /* D363: el permiso (solo root, D372) ya lo miró permisos.c */
        int32_t r = tunel_abrir(s);
        if (r < 0) { kfree(s); return r; }
    }
    int fd = arquivos_novo_socket(s);
    if (fd < 0) socket_fechar(s);
    return fd;
}

/* SIS_CONECTAR (fd, ip, porta) */
int32_t sis_conectar(uint32_t fd, uint32_t ip, uint32_t porta, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    socket_t *s = pegar(fd);
    if (!s) return ERRO_DESCRITOR;
    if (s->tipo == SOCK_TUN) return ERRO_INVALIDO;
    if (porta == 0 || porta > 65535 || ip == 0 || ip == IP_TODOS) return ERRO_INVALIDO;
    if (s->tipo == SOCK_UDP) {
        int32_t r = udp_ligar(s);
        if (r < 0) return r;
        s->ip_destino = ip;
        s->porta_destino = (uint16_t)porta;
        return 0;
    }
    if (!s->conectando) {
        if (s->tcp) return ERRO_JA_EXISTE;
        if (!rede_pronta() && (ip >> 24) != 127) return ERRO_SEM_REDE;   /* 127.0.0.1 funciona sem placa (D079) */
        int32_t erro = 0;
        s->tcp = tcp_abrir(ip, (uint16_t)porta, &erro);
        if (!s->tcp) return erro;
        s->conectando = true;
        /* D401: no bloqueante: vuelve ya; poll da POLLOUT al acabar y OPC_ERRO_CONEXAO el resultado. */
        if (s->nao_bloquear && tcp_situacao(s->tcp) == 1) return ERRO_EM_CURSO;
    } else if (s->nao_bloquear && tcp_situacao(s->tcp) == 1) {
        return ERRO_JA_EM_CURSO;
    }
    int32_t r = tcp_situacao(s->tcp);
    if (r == 1) {
        r = esperar(s, s->tcp);
        if (r == SIS_REPETIR) return r;
    }
    s->conectando = false;
    s->prazo = 0;
    if (r < 0) { tcp_soltar(s->tcp); s->tcp = NULL; }
    return r;
}

/* SIS_ESCUTAR (fd, porta, fila) */
int32_t sis_escutar(uint32_t fd, uint32_t porta, uint32_t fila, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    socket_t *s = pegar(fd);
    if (!s) return ERRO_DESCRITOR;
    if (s->tipo == SOCK_TUN) return ERRO_INVALIDO;
    if (porta == 0 || porta > 65535) return ERRO_INVALIDO;
    if (s->tipo == SOCK_UDP) {
        if (s->porta_local) return ERRO_JA_EXISTE;
        if (udp_porta_usada((uint16_t)porta)) return ERRO_OCUPADO;
        s->porta_local = (uint16_t)porta;
        return 0;
    }
    if (s->tcp) return ERRO_JA_EXISTE;
    int32_t erro = 0;
    s->tcp = tcp_nova_escuta((uint16_t)porta, (int)fila, &erro);
    if (!s->tcp) return erro;
    s->escutando = true;
    return 0;
}

/* SIS_ACEITAR (fd, &endereco_rede_t ou 0) -> fd novo */
int32_t sis_aceitar(uint32_t fd, uint32_t pend, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    socket_t *s = pegar(fd);
    if (!s) return ERRO_DESCRITOR;
    if (!s->escutando) return ERRO_INVALIDO;
    endereco_rede_t *de = (endereco_rede_t *)pend;
    if (de && !usuario_ok(de, sizeof *de, true)) return ERRO_ENDERECO;
    tcp_t *nova = tcp_tirar_da_fila(s->tcp);
    if (!nova) return pronto(s, esperar(s, s->tcp));
    s->prazo = 0;
    socket_t *ns = socket_novo(SOCK_TCP);
    if (!ns) { tcp_soltar(nova); return ERRO_SEM_MEMORIA; }
    ns->tcp = nova;
    int nfd = arquivos_novo_socket(ns);
    if (nfd < 0) { socket_fechar(ns); return nfd; }
    if (de) {
        uint32_t il, ir; uint16_t pl, pr;
        tcp_enderecos(nova, &il, &pl, &ir, &pr);
        de->ip = ir;
        de->porta = pr;
    }
    return nfd;
}

/* SIS_ENVIAR_PARA (fd, buf, n, ip, porta) */
int32_t sis_enviar_para(uint32_t fd, uint32_t buf, uint32_t n, uint32_t ip, uint32_t porta)
{
    socket_t *s = pegar(fd);
    if (!s) return ERRO_DESCRITOR;
    if (!usuario_ok((void *)buf, n, false)) return ERRO_ENDERECO;
    if (s->tipo != SOCK_UDP) return socket_escrever(s, (const char *)buf, n);
    return udp_mandar(s, (const char *)buf, n, ip, porta);
}

/* SIS_RECEBER_DE (fd, buf, n, &endereco_rede_t ou 0) */
int32_t sis_receber_de(uint32_t fd, uint32_t buf, uint32_t n, uint32_t pend, uint32_t e)
{
    (void)e;
    socket_t *s = pegar(fd);
    if (!s) return ERRO_DESCRITOR;
    if (!usuario_ok((void *)buf, n, true)) return ERRO_ENDERECO;
    endereco_rede_t *de = (endereco_rede_t *)pend;
    if (de && !usuario_ok(de, sizeof *de, true)) return ERRO_ENDERECO;
    if (s->tipo == SOCK_UDP) {
        if (!s->porta_local) return ERRO_INVALIDO;   /* precisa de escutar, conectar ou enviar_para antes */
        return pronto(s, udp_receber(s, (char *)buf, n, de));
    }
    int32_t r = socket_ler(s, (char *)buf, n);
    if (r >= 0 && de && s->tcp) {
        uint32_t il, ir; uint16_t pl, pr;
        tcp_enderecos(s->tcp, &il, &pl, &ir, &pr);
        de->ip = ir;
        de->porta = pr;
    }
    return r;
}

/* SIS_ESPERAR_REDE (visto, ms), D117: un servidor con muchos sockets (el
 * litemonkey) mira todos con disponivel y, si ninguno tiene nada, duerme aquí
 * hasta que la red entregue algo a cualquier socket TCP o UDP (o pase el
 * plazo). El contador evita perder un evento que llegue entre mirar y dormir:
 * si ya cambió desde "visto", vuelve en el acto. Devuelve el contador (31 bits). */
static uint32_t eventos_rede;
static const char canal_atividade = 0;

void rede_atividade(void)
{
    eventos_rede = (eventos_rede + 1) & 0x7FFFFFFFu;
    acordar(&canal_atividade);
}

int32_t sis_esperar_rede(uint32_t visto, uint32_t ms, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    if (eventos_rede != visto || ms == 0) return (int32_t)eventos_rede;
    if (ms > 60000) ms = 60000;
    processo_atual()->acordar_em = agora() + (ms * TIQUES_POR_SEGUNDO + 999) / 1000;
    bloquear(&canal_atividade);
    return (int32_t)eventos_rede;   /* resultado listo: no se repite al despertar */
}

/* SIS_OPCAO_SOCKET (fd, opcao, valor) */
int32_t sis_opcao_socket(uint32_t fd, uint32_t opcao, uint32_t valor, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    socket_t *s = pegar(fd);
    if (!s) return ERRO_DESCRITOR;
    switch (opcao) {
    case OPC_TEMPO_LIMITE:
        if (valor > 3600000) return ERRO_INVALIDO;
        s->tempo_ms = valor;
        s->prazo = 0;
        return 0;
    case OPC_FECHAR_ENVIO:
        if (s->tipo != SOCK_TCP || !s->tcp || s->escutando || s->conectando) return ERRO_INVALIDO;
        tcp_fechar_envio(s->tcp);
        return 0;
    case OPC_ENDERECOS: {
        enderecos_socket_t *v = (enderecos_socket_t *)valor;
        if (!usuario_ok(v, sizeof *v, true)) return ERRO_ENDERECO;
        if (s->tipo == SOCK_UDP) {
            v->ip_local = rede_meu_ip(); v->porta_local = s->porta_local;
            v->ip_remoto = s->ip_destino; v->porta_remota = s->porta_destino;
            return 0;
        }
        if (!s->tcp) { memset(v, 0, sizeof *v); return 0; }
        uint32_t il, ir; uint16_t pl, pr;
        tcp_enderecos(s->tcp, &il, &pl, &ir, &pr);
        v->ip_local = il; v->porta_local = pl; v->ip_remoto = ir; v->porta_remota = pr;
        return 0;
    }
    case OPC_ESTADO:
        if (s->tipo == SOCK_UDP) return CON_UDP;
        return s->tcp ? (int32_t)tcp_estado(s->tcp) : CON_FECHADA;
    case OPC_FISICA:
        /* D363: saltarse las rutas del túnel es cosa de su demonio: si no, cualquier programa podría
         * sacar tráfico por fuera de la VPN. D372: el demonio es root, así que basta con exigir root
         * (antes valía también el dueño del túnel, que podía ser un programa de la sesión local). */
        if (s->tipo != SOCK_UDP) return ERRO_INVALIDO;
        if (processo_recursos()->uid != 0) return ERRO_ACESSO;
        s->fisica = valor != 0;
        return 0;
    case OPC_TUN_CONFIG:
        if (s->tipo != SOCK_TUN) return ERRO_INVALIDO;
        /* D372: dirección, rutas y DNS, solo root, aunque el descriptor haya acabado en otras manos
         * (un demonio que soltase la elevación después de abrirlo no puede cambiar las rutas). */
        if (processo_recursos()->uid != 0) return ERRO_ACESSO;
        return tunel_configurar(s, valor);
    case OPC_TUN_ESTADO:
        if (s->tipo != SOCK_TUN) return ERRO_INVALIDO;
        return tunel_publicar(s, valor);
    case OPC_ERRO_CONEXAO: {   /* D401: SO_ERROR de un connect no bloqueante */
        if (s->tipo != SOCK_TCP) return 0;
        if (!s->tcp) return s->conectando ? ERRO_RECUSADA : 0;
        int32_t r = tcp_situacao(s->tcp);
        if (r == 1) return 1;
        if (s->conectando) {
            s->conectando = false;
            if (r < 0) { tcp_soltar(s->tcp); s->tcp = NULL; }
        }
        return r;
    }
    }
    return ERRO_INVALIDO;
}
