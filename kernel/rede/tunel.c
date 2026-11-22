// SPDX-License-Identifier: GPL-3.0-only
/* tunel.c - la interfaz del túnel, wg0 (D363).
 *
 * El WireGuard de SaguiOS está repartido como el wireguard-go con un TUN: aquí,
 * en el kernel, solo hay una interfaz virtual con su dirección, sus rutas, su
 * MTU y su DNS; el protocolo (claves, saludo, cifrado) corre en un demonio de
 * usuario (userland/programas/wg.c) que abre esta interfaz como un socket
 * SOCK_TUN. Lo que la pila manda por wg0 espera en la fila «saida» hasta que el
 * demonio lo lee; lo que el demonio escribe (ya descifrado y comprobado) espera
 * en la fila «entrada» y la tarea de la red lo mete en la pila como si hubiera
 * llegado por un cable.
 *
 * Seguridad: las claves no entran nunca al kernel. El kernel no se fía del
 * demonio más que de un cable: cada paquete que escribe pasa por las mismas
 * comprobaciones de ip_chegou (cabecera, suma, longitudes).
 *
 * Solo un túnel a la vez. Si el demonio muere, al cerrarse su socket la
 * interfaz desaparece con sus rutas: la red física sigue como estaba.
 *
 * Concurrencia: como el resto de la red, todo con las interrupciones
 * desligadas (llamadas al sistema y tarea de la red). */
#include "rede.h"
#include "abi.h"
#include "console.h"
#include "memoria.h"
#include "processo.h"
#include "sistema.h"
#include "texto.h"

#define FILA_MAX  64          /* paquetes por sentido */
#define FILA_BYTES (128 * 1024)

typedef struct pacote_tunel {
    struct pacote_tunel *prox;
    uint32_t n;
    uint8_t  d[];
} pacote_tunel_t;

typedef struct {
    pacote_tunel_t *primeiro, *ultimo;
    uint32_t qtd, bytes;
} fila_t;

static struct {
    bool           ativo;
    bool           desligando;
    const void    *dono;          /* el socket del demonio */
    int            pid;
    uint32_t       uid;
    config_tunel_t cfg;
    fila_t         saida, entrada;
    uint32_t       rx_pacotes, tx_pacotes, rx_bytes, tx_bytes, descartados;
    info_wg_demonio_t wg;
} t;

static bool fila_por(fila_t *f, const uint8_t *d, uint32_t n)
{
    if (f->qtd >= FILA_MAX || f->bytes + n > FILA_BYTES) return false;
    pacote_tunel_t *p = kmalloc(sizeof *p + n);
    if (!p) return false;
    p->prox = NULL;
    p->n = n;
    memcpy(p->d, d, n);
    if (f->ultimo) f->ultimo->prox = p; else f->primeiro = p;
    f->ultimo = p;
    f->qtd++;
    f->bytes += n;
    return true;
}

static pacote_tunel_t *fila_tirar(fila_t *f)
{
    pacote_tunel_t *p = f->primeiro;
    if (!p) return NULL;
    f->primeiro = p->prox;
    if (!f->primeiro) f->ultimo = NULL;
    f->qtd--;
    f->bytes -= p->n;
    return p;
}

static void fila_limpar(fila_t *f)
{
    pacote_tunel_t *p;
    while ((p = fila_tirar(f))) kfree(p);
}

/* ------------------------------------------------------------ lo que usa la pila (rede.c) */

bool tunel_ativo(void) { return t.ativo; }
uint32_t tunel_ip(void) { return t.ativo ? t.cfg.ip : 0; }
uint32_t tunel_dns(void) { return t.ativo ? t.cfg.dns : 0; }
uint32_t tunel_mtu(void) { return t.cfg.mtu; }

static bool casa(uint32_t ip, uint32_t rede, uint32_t bits)
{
    uint32_t m = bits ? 0xFFFFFFFFu << (32 - bits) : 0;
    return ((ip ^ rede) & m) == 0;
}

static uint32_t bits_mascara(uint32_t m)
{
    uint32_t b = 0;
    while (b < 32 && (m & (0x80000000u >> b))) b++;
    return b;
}

/* La ruta más larga del túnel que lleva a "destino": sus bits, o -1 si no hay. La red de
 * la propia dirección (Address 10.0.0.2/24) cuenta como una ruta más (como en Linux). */
int tunel_casamento(uint32_t destino)
{
    if (!t.ativo) return -1;
    int melhor = -1;
    uint32_t b = bits_mascara(t.cfg.mascara);
    if (t.cfg.mascara && casa(destino, t.cfg.ip, b)) melhor = (int)b;
    for (uint32_t i = 0; i < t.cfg.nrotas; i++)
        if ((int)t.cfg.rotas[i].bits > melhor && casa(destino, t.cfg.rotas[i].ip, t.cfg.rotas[i].bits))
            melhor = (int)t.cfg.rotas[i].bits;
    return melhor;
}

/* ip_chegou rechazó un paquete que el demonio escribió (origen fuera de las rutas del túnel,
 * destino que no es la dirección del túnel, DHCP): se cuenta y los primeros van al registro. */
void tunel_descarte(uint32_t origem, uint32_t destino)
{
    t.descartados++;
    if (t.descartados <= 8)
        klog("[TUNEL] %s: descartado %u.%u.%u.%u -> %u.%u.%u.%u (origen o destino no permitidos)\n", t.cfg.nome,
             origem >> 24, (origem >> 16) & 255, (origem >> 8) & 255, origem & 255,
             destino >> 24, (destino >> 16) & 255, (destino >> 8) & 255, destino & 255);
}

/* La pila manda un paquete IPv4 entero (cabecera incluida) por wg0. */
bool tunel_enviar(const uint8_t *pacote, uint32_t n)
{
    if (!t.ativo || t.desligando || n > t.cfg.mtu) { t.descartados++; return false; }
    if (!fila_por(&t.saida, pacote, n)) { t.descartados++; return false; }   /* fila llena: se pierde (el TCP reenvía) */
    t.tx_pacotes++;
    t.tx_bytes += n;
    acordar(&t);
    rede_atividade();
    return true;
}

/* La tarea de la red mete en la pila lo que escribió el demonio. */
void tunel_processar(void)
{
    for (int k = 0; k < FILA_MAX && t.ativo; k++) {
        pacote_tunel_t *p = fila_tirar(&t.entrada);
        if (!p) break;
        rede_tunel_chegou(p->d, p->n);
        kfree(p);
    }
}

void tunel_info_rede(info_rede_t *v)
{
    memset(v, 0, sizeof *v);
    txt_copia_n(v->nome, t.cfg.nome, sizeof v->nome);
    txt_copia_n(v->driver, "wireguard", sizeof v->driver);
    txt_copia_n(v->descricao, "Túnel WireGuard (VPN)", sizeof v->descricao);
    v->ativa = 1;
    v->cabo = t.wg.npares && t.wg.pares[0].ultimo_hs ? 1 : 0;   /* «enlace»: hubo saludo */
    v->irq = 0xFF;
    v->ip = t.cfg.ip;
    v->mascara = t.cfg.mascara;
    v->dns = t.cfg.dns;
    v->metodo = REDE_ESTATICO;
    v->rx_pacotes = t.rx_pacotes; v->tx_pacotes = t.tx_pacotes;
    v->rx_bytes = t.rx_bytes; v->tx_bytes = t.tx_bytes;
    v->descartados = t.descartados;
}

int32_t tunel_info(uint32_t ptr, uint32_t n)
{
    static info_wg_t v;
    uint32_t tam = n && n < sizeof v ? n : sizeof v;
    if (!usuario_ok((void *)ptr, tam, true)) return ERRO_ENDERECO;
    memset(&v, 0, sizeof v);
    if (t.ativo) {
        v.ativo = 1;
        v.pid = (uint32_t)t.pid;
        v.uid = t.uid;
        v.desligando = t.desligando;
        v.config = t.cfg;
        v.rx_pacotes = t.rx_pacotes; v.tx_pacotes = t.tx_pacotes;
        v.rx_bytes = t.rx_bytes; v.tx_bytes = t.tx_bytes;
        v.descartados = t.descartados;
        v.wg = t.wg;
    }
    memcpy((void *)ptr, &v, tam);
    return t.ativo ? 1 : 0;
}

/* Para la bandeja (barra.c): ¿activo? y ¿hubo saludo? */
bool tunel_bandeja(bool *conectado, char *nome, uint32_t max)
{
    if (!t.ativo) return false;
    *conectado = t.wg.npares && t.wg.pares[0].ultimo_hs;
    txt_copia_n(nome, t.cfg.nome, max);
    return true;
}

/* REDE_TUNEL_DESLIGAR: el demonio lo ve en disponivel() y sale (borrando sus claves). */
int32_t tunel_pedir_desligar(void)
{
    if (!t.ativo) return ERRO_NAO_EXISTE;
    t.desligando = true;
    acordar(&t);
    rede_atividade();
    klog("[TUNEL] %s: apagado pedido por pid %d\n", t.cfg.nome, processo_atual()->pid);
    return 0;
}

/* ------------------------------------------------------------ el socket del demonio (socket.c) */

int32_t tunel_abrir(const void *dono)
{
    if (t.ativo) return ERRO_OCUPADO;
    memset(&t, 0, sizeof t);
    t.ativo = true;
    t.dono = dono;
    t.pid = processo_recursos()->pid;
    t.uid = processo_lider(processo_atual())->uid;
    txt_copia_n(t.cfg.nome, "wg0", sizeof t.cfg.nome);
    t.cfg.mtu = 1420;
    klog("[TUNEL] wg0 creada por pid %d\n", t.pid);
    return 0;
}

bool tunel_dono(const void *s) { return t.ativo && t.dono == s; }

void tunel_fechar(const void *dono)
{
    if (!t.ativo || t.dono != dono) return;
    fila_limpar(&t.saida);
    fila_limpar(&t.entrada);
    klog("[TUNEL] %s cerrada (rx %u paquetes, tx %u)\n", t.cfg.nome, t.rx_pacotes, t.tx_pacotes);
    bool tinha_dns = t.cfg.dns != 0;
    memset(&t, 0, sizeof t);
    if (tinha_dns) rede_dns_limpar();   /* las respuestas del DNS del túnel ya no valen */
    acordar(&t);
    rede_atividade();
}

int32_t tunel_disponivel(const void *dono)
{
    if (!t.ativo || t.dono != dono) return ERRO_INVALIDO;
    if (t.desligando) return ERRO_PIPE_QUEBRADO;
    return t.saida.primeiro ? (int32_t)t.saida.primeiro->n : 0;
}

/* Lee el siguiente paquete para el demonio; SIS_REPETIR si no hay (canal: &t). */
int32_t tunel_ler(const void *dono, char *buf, uint32_t n)
{
    if (!t.ativo || t.dono != dono) return ERRO_INVALIDO;
    if (t.desligando) return ERRO_PIPE_QUEBRADO;
    pacote_tunel_t *p = fila_tirar(&t.saida);
    if (!p) return SIS_REPETIR;
    uint32_t k = p->n < n ? p->n : n;
    memcpy(buf, p->d, k);
    kfree(p);
    return (int32_t)k;
}

const void *tunel_canal(void) { return &t; }

/* El demonio entrega un paquete descifrado. Solo se mira lo mínimo (IPv4 y el tamaño):
 * ip_chegou lo comprueba todo, como lo que llega del cable. */
int32_t tunel_escrever(const void *dono, const char *buf, uint32_t n)
{
    if (!t.ativo || t.dono != dono) return ERRO_INVALIDO;
    if (n < 20 || n > 1500 || ((const uint8_t *)buf)[0] >> 4 != 4) { t.descartados++; return ERRO_INVALIDO; }
    if (!fila_por(&t.entrada, (const uint8_t *)buf, n)) { t.descartados++; return (int32_t)n; }   /* llena: se pierde */
    t.rx_pacotes++;
    t.rx_bytes += n;
    rede_acordar();
    return (int32_t)n;
}

int32_t tunel_configurar(const void *dono, uint32_t ptr)
{
    if (!t.ativo || t.dono != dono) return ERRO_INVALIDO;
    if (!usuario_ok((void *)ptr, sizeof(config_tunel_t), false)) return ERRO_ENDERECO;
    config_tunel_t c = *(const config_tunel_t *)ptr;
    c.nome[sizeof c.nome - 1] = 0;
    if (!c.nome[0]) txt_copia_n(c.nome, "wg0", sizeof c.nome);
    if (c.mtu == 0) c.mtu = 1420;
    if (c.mtu < 576 || c.mtu > 1500) return ERRO_INVALIDO;
    uint32_t ip = c.ip, m = c.mascara;
    if (ip == 0 || ip == IP_TODOS || (ip >> 28) >= 14 || (ip >> 24) == 127) return ERRO_INVALIDO;
    if (rede_ip_de_interface(ip)) return ERRO_INVALIDO;   /* la de la física: el filtro de ip_chegou ya no distinguiría */
    if (m == 0 || (~m & (~m + 1)) != 0) return ERRO_INVALIDO;   /* unos seguidos de ceros */
    if (c.nrotas > TUNEL_ROTAS_MAX) return ERRO_INVALIDO;
    for (uint32_t i = 0; i < c.nrotas; i++) {
        if (c.rotas[i].bits > 32) return ERRO_INVALIDO;
        uint32_t mm = c.rotas[i].bits ? 0xFFFFFFFFu << (32 - c.rotas[i].bits) : 0;
        c.rotas[i].ip &= mm;
    }
    if (c.dns == IP_TODOS) return ERRO_INVALIDO;
    bool dns_mudou = c.dns != t.cfg.dns;
    t.cfg = c;
    if (dns_mudou) rede_dns_limpar();
    klog("[TUNEL] %s: %u.%u.%u.%u/%u, MTU %u, %u rutas%s\n", c.nome, ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255,
         ip & 255, bits_mascara(m), c.mtu, c.nrotas, c.dns ? ", DNS propio" : "");
    return 0;
}

int32_t tunel_publicar(const void *dono, uint32_t ptr)
{
    if (!t.ativo || t.dono != dono) return ERRO_INVALIDO;
    if (!usuario_ok((void *)ptr, sizeof(info_wg_demonio_t), false)) return ERRO_ENDERECO;
    t.wg = *(const info_wg_demonio_t *)ptr;
    t.wg.publica[sizeof t.wg.publica - 1] = 0;
    if (t.wg.npares > WG_INFO_PARES) t.wg.npares = WG_INFO_PARES;
    for (uint32_t i = 0; i < WG_INFO_PARES; i++) {
        t.wg.pares[i].publica[sizeof t.wg.pares[i].publica - 1] = 0;
        if (t.wg.pares[i].nips > 8) t.wg.pares[i].nips = 8;
    }
    return 0;
}
