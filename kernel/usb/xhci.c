// SPDX-License-Identifier: GPL-2.0-or-later
/* xhci.c - driver del controlador USB 3.0 (xHCI) (D155, D214).
 *
 * El xHCI no se parece al EHCI: el sistema no describe las transferencias en
 * listas que el controlador recorre, sino que deja TRB (Transfer Request
 * Blocks, 16 bytes) en anillos y "toca la campana" (doorbell):
 *
 *   - el anillo de ÓRDENES: habilitar una ranura, dar dirección, configurar
 *     endpoints, parar o reiniciar un endpoint...;
 *   - el anillo de EVENTOS (uno, descrito por la tabla ERST): el controlador
 *     escribe aquí el fin de cada orden y de cada transferencia y los cambios
 *     de los puertos;
 *   - un anillo de TRANSFERENCIA por endpoint de cada dispositivo.
 * El bit de ciclo de cada TRB dice de quién es: el productor lo invierte en
 * cada vuelta al anillo (el TRB de enlace del final lleva el "toggle cycle").
 *
 * Cada dispositivo ocupa una RANURA (slot) con dos contextos: el de salida
 * (lo que el controlador sabe del dispositivo, apuntado por la DCBAA) y el de
 * entrada (lo que el sistema le pide en Address Device, Configure Endpoint o
 * Evaluate Context). Los contextos miden 32 o 64 bytes (HCCPARAMS1.CSZ).
 *
 * Diseño, como el del EHCI (D095), simple y previsible:
 *   - una transferencia por vez en todo el sistema (un cerrojo), con un buffer
 *     de 128 KB en memoria física contigua por el que pasan los datos;
 *   - cada TD (transferencia) es de un solo TRB "Normal" (los de bulk se
 *     parten en trozos que no cruzan una frontera de 64 KB y se mandan uno
 *     detrás de otro) o, en las de control, Setup + Data + Status;
 *   - el evento de fin se reconoce por la dirección del TRB: si cae dentro del
 *     TD en curso es suyo; si no, es viejo y se ignora;
 *   - la espera mira el anillo de eventos con un plazo medido por el TSC; con
 *     el planificador en marcha cede la CPU y, si la IRQ del controlador llega
 *     (INTx, con la autoprueba de la D211), duerme hasta ella;
 *   - un endpoint parado por un error (STALL, error de transacción) se
 *     recupera con Reset Endpoint + Set TR Dequeue Pointer; una transferencia
 *     que no termina a tiempo se cancela con Stop Endpoint;
 *   - todo por debajo de 4 GB: la parte alta de cada dirección es 0 (el kernel
 *     ve la memoria física 1:1, D008).
 * Antes de nada se pide el controlador a la BIOS (capacidad extendida "USB
 * Legacy Support"), que lo usaba para el teclado y el pendrive de arranque. */
#include "xhci.h"
#include "isoent.h"
#include "console.h"
#include "cpu.h"
#include "irq_rotas.h"
#include "memoria.h"
#include "paginacao.h"
#include "pci.h"
#include "portas.h"
#include "prazo.h"
#include "processo.h"
#include "relogio.h"
#include "texto.h"
#include "trava.h"

/* Registros de capacidad. */
#define CAP_CAPLENGTH  0x00
#define CAP_HCIVERSION 0x02
#define CAP_HCSPARAMS1 0x04
#define CAP_HCSPARAMS2 0x08
#define CAP_HCCPARAMS1 0x10
#define CAP_DBOFF      0x14
#define CAP_RTSOFF     0x18

/* Registros operacionales (desde CAPLENGTH). */
#define OP_USBCMD   0x00
#define OP_USBSTS   0x04
#define OP_PAGESIZE 0x08
#define OP_DNCTRL   0x14
#define OP_CRCR     0x18
#define OP_DCBAAP   0x30
#define OP_CONFIG   0x38
#define OP_PORTSC   0x400   /* + 0x10 por puerto */

#define CMD_RS    (1u << 0)
#define CMD_HCRST (1u << 1)
#define CMD_INTE  (1u << 2)
#define CMD_HSEE  (1u << 3)
#define STS_HCH   (1u << 0)
#define STS_HSE   (1u << 2)
#define STS_EINT  (1u << 3)
#define STS_PCD   (1u << 4)
#define STS_CNR   (1u << 11)
#define STS_HCE   (1u << 12)

/* Interruptor 0 (desde RTSOFF). */
#define RT_IMAN   0x20
#define RT_IMOD   0x24
#define RT_ERSTSZ 0x28
#define RT_ERSTBA 0x30
#define RT_ERDP   0x38
#define IMAN_IP   (1u << 0)
#define IMAN_IE   (1u << 1)
#define ERDP_EHB  (1u << 3)

/* PORTSC */
#define P_CCS   (1u << 0)
#define P_PED   (1u << 1)
#define P_OCA   (1u << 3)
#define P_PR    (1u << 4)
#define P_PLS   (0xFu << 5)
#define P_PP    (1u << 9)
#define P_VEL   (0xFu << 10)
#define P_PIC   (3u << 14)
#define P_LWS   (1u << 16)
#define P_CSC   (1u << 17)
#define P_PEC   (1u << 18)
#define P_WRC   (1u << 19)
#define P_OCC   (1u << 20)
#define P_PRC   (1u << 21)
#define P_PLC   (1u << 22)
#define P_CEC   (1u << 23)
#define P_CAS   (1u << 24)
#define P_WAKE  (7u << 25)
#define P_DR    (1u << 30)
#define P_WPR   (1u << 31)
#define P_MUDANCAS (P_CSC | P_PEC | P_WRC | P_OCC | P_PRC | P_PLC | P_CEC)
/* Lo que se puede reescribir tal cual sin cambiar nada (como el "neutral" de Linux):
 * nunca PED (escribir 1 deshabilita el puerto) ni los bits de cambio (escribir 1 los borra). */
#define P_NEUTRO (P_CCS | P_OCA | P_PLS | P_PP | P_VEL | P_PIC | P_CAS | P_WAKE | P_DR)

/* Tipos de TRB. */
#define TRB_NORMAL     1
#define TRB_SETUP      2
#define TRB_DATA       3
#define TRB_ISOCH      5
#define TRB_STATUS     4
#define TRB_LINK       6
#define TRB_NOOP_TR    8
#define TRB_EN_SLOT    9
#define TRB_DIS_SLOT   10
#define TRB_ADDR_DEV   11
#define TRB_CONF_EP    12
#define TRB_EVAL_CTX   13
#define TRB_RESET_EP   14
#define TRB_STOP_EP    15
#define TRB_SET_DEQ    16
#define TRB_NOOP_CMD   23
#define EV_TRANSFER    32
#define EV_COMANDO     33
#define EV_PORTA       34
#define EV_CONTROLADOR 37

#define TRB_CICLO (1u << 0)
#define TRB_TC    (1u << 1)    /* enlace: invierte el ciclo */
#define TRB_ISP   (1u << 2)    /* evento también si el paquete viene corto */
#define TRB_CH    (1u << 4)
#define TRB_IOC   (1u << 5)
#define TRB_IDT   (1u << 6)    /* los datos (el setup) van dentro del TRB */
#define TRB_DIR_IN (1u << 16)

/* Códigos de compleción. */
#define CC_OK     1
#define CC_STALL  6
#define CC_CURTO  13

#define NTRB          256u            /* TRB por anillo (una página) */
#define XHCI_MAX_SLOTS 16
#define BUFFER_BYTES  131072u
#define CTRL_MAX      4096u
#define HID_BUF_DESLOC 0xC00u        /* en la página del contexto de entrada, tras los 33 contextos */
#define HID_BUF_TAM   0x100u
#define ESPERAS_SEM_IRQ 6

typedef struct { uint32_t p0, p1, status, controle; } trb_t;

typedef struct {
    volatile trb_t *t;
    uint32_t fis;
    uint32_t enq;
    uint32_t ciclo;
} anel_t;

typedef struct {
    anel_t   anel;
    bool     ativo;
    volatile bool pendente, feito, curto;
    volatile uint8_t codigo;
    volatile uint32_t resto, resto_dados;
    uint32_t td_ini, td_fim;
    /* HID: lectura de interrupción armada en la página de entrada */
    bool     int_armado, int_erro;
    bool     int_grande;          /* D215: buffer propio (recepción continua del Wi-Fi), sin el límite de HID */
    uint16_t int_tam;
    uint32_t int_buf;
    struct xfila *fila;           /* D238: cola de transferencias (NULL = la manera de siempre) */
    struct xiso  *iso;            /* D275: flujo isócrono (NULL = no es de ese tipo) */
} xep_t;

typedef struct {
    bool     usado;
    uint8_t  id;
    uint32_t saida, entrada;       /* páginas de los contextos */
    usb_disp_t *disp;
    uint8_t  n_buf;                /* buffers de HID usados en la página de entrada */
    xep_t    ep[32];               /* por DCI (1 = endpoint 0) */
} xslot_t;

/* D238: cola de transferencias bulk en vuelo a la vez (el pincho Wi-Fi). Cada
 * una es un TD de un solo TRB sobre su propio buffer (del driver, físico, sin
 * cruzar 64 KB). Las de un endpoint terminan en orden: basta con mirar la más
 * antigua. De entrada, todas armadas siempre: al leer una se vuelve a armar al
 * final del anillo. De salida, se ponen al enviar y se recogen al terminar. Un
 * endpoint con cola no usa td_executar ni la lectura de HID. */
#define XFILA_MAX     8
#define XISO_MAX_TOTAL (XHCI_MAX_CTL * USB_ISO_MAX)
#define XFILAS        4
#define CC_PARADO     26              /* "Stopped": el TD lo cortó un Stop Endpoint */
#define CC_PARADO_TAM 27

typedef struct xfila {
    bool     usado;
    bool     entrada;
    uint8_t  n;                       /* transferencias de la cola */
    uint8_t  prox;                    /* la más antigua en el anillo */
    uint8_t  emvoo;                   /* cuántas hay en el anillo sin recoger */
    uint8_t  dci;
    xslot_t *slot;
    uint32_t tam;                     /* bytes de cada buffer */
    uint32_t buf[XFILA_MAX];          /* físicos */
    uint32_t trb[XFILA_MAX];          /* el TRB de cada una en el anillo */
    volatile bool     feito[XFILA_MAX];
    volatile uint8_t  cod[XFILA_MAX];
    volatile uint32_t resto[XFILA_MAX];
    volatile bool     erro;           /* terminó una con error: el endpoint se recupera con el cerrojo */
    uint32_t completadas, erros, perdidas;
} xfila_t;

struct xhci {
    pci_disp_t pci;
    volatile uint8_t *cap, *op, *rt;
    volatile uint32_t *db;
    uint32_t barra, tam_barra;
    uint16_t versao;
    int      portas, slots_max, slots_en, n_intr, n_scratch;
    uint32_t tam_ctx;
    bool     ac64, ppc;
    uint32_t dcbaa, scratch;
    anel_t   cmd;
    volatile trb_t *ev;
    uint32_t ev_fis, ev_deq, ev_ciclo, erst;
    volatile bool cmd_feito;
    volatile uint8_t cmd_codigo, cmd_slot;
    uint32_t cmd_trb;
    uint8_t  protocolo[XHCI_MAX_PORTAS];
    int      resultado[XHCI_MAX_PORTAS];
    int      bios;                 /* 0 = no lo usaba, 1 = lo soltó, -1 = forzado */
    bool     ok, morto;
    uint8_t  irq;
    bool     irq_usavel, intx;
    int      esperas_sem_irq;
    volatile uint32_t irqs;
    volatile bool prova;
    volatile uint32_t provas;
    uint32_t soltos, erros, transferencias;
    xslot_t  slots[XHCI_MAX_SLOTS + 1];
};

static xhci_t ctls[XHCI_MAX_CTL];
static int qtd_ctls;
static bool assincrono;
static uint8_t *buffer;
static const void *canal_hid;
static const void *canal_usb;    /* D227: la tarea de conexión en caliente */
static const void *canal_fila;   /* D238: la tarea que lee las colas (el Wi-Fi) */
static xfila_t filas[XFILAS];
static void recuperar_trava(void);
static trava_t trava_xhci = { .ao_roubar = recuperar_trava };

int xhci_quantidade(void) { return qtd_ctls; }
xhci_t *xhci_controlador(int i) { return i >= 0 && i < qtd_ctls ? &ctls[i] : NULL; }
int xhci_portas(xhci_t *c) { return c->portas; }
void xhci_avisar_hid(const void *canal) { canal_hid = canal; }
void xhci_avisar_usb(const void *canal) { canal_usb = canal; }
void xhci_avisar_fila(const void *canal) { canal_fila = canal; }

/* ------------------------------------------------------------ registros y tiempo */

static inline uint32_t le_op(xhci_t *c, uint32_t r) { return *(volatile uint32_t *)(c->op + r); }
static inline void escreve_op(xhci_t *c, uint32_t r, uint32_t v) { *(volatile uint32_t *)(c->op + r) = v; }
static inline uint32_t le_rt(xhci_t *c, uint32_t r) { return *(volatile uint32_t *)(c->rt + r); }
static inline void escreve_rt(xhci_t *c, uint32_t r, uint32_t v) { *(volatile uint32_t *)(c->rt + r) = v; }
static inline void escreve64_op(xhci_t *c, uint32_t r, uint32_t v)
{
    escreve_op(c, r, v);
    escreve_op(c, r + 4, 0);   /* todo por debajo de 4 GB */
}
static inline void escreve64_rt(xhci_t *c, uint32_t r, uint32_t v)
{
    escreve_rt(c, r, v);
    escreve_rt(c, r + 4, 0);
}
static inline uint32_t le_porta(xhci_t *c, int p) { return le_op(c, OP_PORTSC + 0x10u * (uint32_t)p); }
static inline void escreve_porta(xhci_t *c, int p, uint32_t v) { escreve_op(c, OP_PORTSC + 0x10u * (uint32_t)p, v); }

/* Barrera completa: lo escrito en la memoria (los TRB) antes de la campana. */
static inline void barreira(void) { __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory"); }

void xhci_esperar_ms(uint32_t ms)
{
    prazo_t p = prazo_ms(ms);
    while (!prazo_vencido(&p)) ;
}

/* Espera a que (registro & mascara) == valor, con plazo. */
static bool esperar_op(xhci_t *c, uint32_t reg, uint32_t mascara, uint32_t valor, uint32_t ms)
{
    prazo_t p = prazo_ms(ms);
    for (;;) {
        if ((le_op(c, reg) & mascara) == valor) return true;
        if (prazo_vencido(&p)) return (le_op(c, reg) & mascara) == valor;
    }
}

static const char *nome_codigo(uint32_t cod)
{
    switch (cod) {
    case 1: return "exito";
    case 2: return "error de buffer de datos";
    case 3: return "babble";
    case 4: return "error de transaccion USB";
    case 5: return "TRB invalido";
    case 6: return "STALL";
    case 7: return "sin recursos";
    case 8: return "sin ancho de banda";
    case 9: return "sin ranuras libres";
    case 11: return "ranura no habilitada";
    case 12: return "endpoint no habilitado";
    case 13: return "paquete corto";
    case 17: return "parametro invalido";
    case 19: return "estado del contexto";
    case 21: return "anillo de eventos lleno";
    case 22: return "dispositivo incompatible";
    case 24: return "anillo de ordenes parado";
    case 25: return "orden abortada";
    case 26: return "parado";
    case 27: return "parado (longitud invalida)";
    case 36: return "error de transaccion partida";
    default: return "?";
    }
}

const char *usb_nome_velocidade(int v)
{
    switch (v) {
    case USB_VEL_PLENA: return "12 Mb/s (USB 1.1)";
    case USB_VEL_BAIXA: return "1,5 Mb/s (USB 1.1)";
    case USB_VEL_ALTA: return "480 Mb/s (USB 2.0)";
    case USB_VEL_SUPER: return "5 Gb/s (USB 3.0)";
    case USB_VEL_SUPERP: return "10 Gb/s (USB 3.1)";
    default: return "?";
    }
}

/* ------------------------------------------------------------ anillos */

static bool anel_novo(anel_t *a)
{
    uint32_t p = memfisica_alocar();
    if (!p) return false;
    memset((void *)p, 0, PAGINA);
    a->t = (volatile trb_t *)p;
    a->fis = p;
    a->enq = 0;
    a->ciclo = 1;
    a->t[NTRB - 1].p0 = p;               /* el TRB de enlace vuelve al principio */
    a->t[NTRB - 1].p1 = 0;
    a->t[NTRB - 1].status = 0;
    a->t[NTRB - 1].controle = (TRB_LINK << 10) | TRB_TC;   /* el ciclo se pone al pasar por él */
    return true;
}

static void anel_avancar(anel_t *a)
{
    if (++a->enq == NTRB - 1) {
        a->t[NTRB - 1].controle = (TRB_LINK << 10) | TRB_TC | a->ciclo;
        a->ciclo ^= 1;
        a->enq = 0;
    }
}

/* Pone un TD de n TRB seguidos (sin cruzar el enlace: si no cabe, el resto
 * del anillo se rellena con TRB vacíos). El bit de ciclo del primero se
 * escribe al final: el controlador no ve un TD a medias. */
static void anel_td(anel_t *a, const trb_t *v, uint32_t n, uint32_t tipo_vazio, uint32_t *ini, uint32_t *fim)
{
    if (a->enq + n > NTRB - 1)
        while (a->enq != 0) {
            volatile trb_t *t = &a->t[a->enq];
            t->p0 = 0; t->p1 = 0; t->status = 0;
            t->controle = (tipo_vazio << 10) | a->ciclo;
            anel_avancar(a);
        }
    uint32_t prim = a->enq, ciclo0 = a->ciclo;
    for (uint32_t i = 0; i < n; i++) {
        volatile trb_t *t = &a->t[a->enq];
        t->p0 = v[i].p0;
        t->p1 = v[i].p1;
        t->status = v[i].status;
        if (i) t->controle = v[i].controle | a->ciclo;
        *fim = a->fis + a->enq * 16u;
        anel_avancar(a);
    }
    barreira();
    a->t[prim].controle = v[0].controle | ciclo0;
    barreira();
    *ini = a->fis + prim * 16u;
}

/* ------------------------------------------------------------ eventos */

static xslot_t *slot_por_id(xhci_t *c, uint32_t id)
{
    if (id == 0 || id > (uint32_t)c->slots_en || !c->slots[id].usado) return NULL;
    return &c->slots[id];
}

/* D238: el fin de una transferencia de una cola. Solo cuenta si es de una de
 * las que están en el anillo (las cortadas por un Stop Endpoint, no). */
static bool fila_evento(xfila_t *f, uint32_t ptr, uint32_t cod, uint32_t resto)
{
    if (cod == CC_PARADO || cod == CC_PARADO_TAM) return false;
    for (uint32_t k = 0; k < f->emvoo; k++) {
        uint32_t i = (f->prox + k) % f->n;
        if (f->trb[i] != ptr || f->feito[i]) continue;
        f->resto[i] = resto;
        f->cod[i] = (uint8_t)cod;
        f->feito[i] = true;
        if (cod != CC_OK && cod != CC_CURTO) f->erro = true;
        return true;
    }
    return false;
}

static void iso_evento(struct xiso *is, uint32_t ptr, uint32_t cod, uint32_t resto);

/* Consume todo lo que haya en el anillo de eventos y anota el resultado en el
 * endpoint o en la orden a la que pertenece. */
static void processar(xhci_t *c)
{
    bool algum = false;
    for (uint32_t n = 0; n < NTRB; n++) {
        volatile trb_t *e = &c->ev[c->ev_deq];
        uint32_t ctl = e->controle;
        if ((ctl & 1) != c->ev_ciclo) break;
        uint32_t tipo = (ctl >> 10) & 0x3F, cod = e->status >> 24, ptr = e->p0;
        if (tipo == EV_TRANSFER) {
            xslot_t *s = slot_por_id(c, ctl >> 24);
            uint32_t dci = (ctl >> 16) & 0x1F;
            xep_t *ep = s ? &s->ep[dci] : NULL;
            uint32_t resto = e->status & 0xFFFFFF;
            if (ep && ep->iso) {
                iso_evento(ep->iso, ptr, cod, resto);   /* D275 */
            } else if (ep && ep->fila) {
                if ((ctl & (1u << 2)) || !fila_evento(ep->fila, ptr, cod, resto)) c->soltos++;
            } else if (!ep || !ep->pendente || ep->feito || (ctl & (1u << 2)) || ptr < ep->td_ini || ptr > ep->td_fim) {
                c->soltos++;    /* de un TD viejo (o cancelado) */
            } else if (cod == CC_CURTO && ptr != ep->td_fim) {
                ep->curto = true;          /* datos cortos en una de control: falta el estado */
                ep->resto_dados = resto;
            } else {
                if (cod == CC_CURTO) { ep->curto = true; ep->resto_dados = resto; }
                ep->codigo = (uint8_t)cod;
                ep->resto = resto;
                ep->feito = true;
            }
        } else if (tipo == EV_COMANDO) {
            if (ptr == c->cmd_trb) {
                c->cmd_codigo = (uint8_t)cod;
                c->cmd_slot = (uint8_t)(ctl >> 24);
                c->cmd_feito = true;
            }
        } else if (tipo == EV_CONTROLADOR) {
            klog("[XHCI] evento del controlador: %s (%u)\n", nome_codigo(cod), cod);
        }
        /* EV_PORTA: la tarea USB mira los puertos por su cuenta (bits de cambio);
         * D227: se la despierta para que no espere a su próxima vuelta. */
        else if (tipo == EV_PORTA && canal_usb) acordar(canal_usb);
        algum = true;
        if (++c->ev_deq == NTRB) { c->ev_deq = 0; c->ev_ciclo ^= 1; }
    }
    if (algum) escreve64_rt(c, RT_ERDP, (c->ev_fis + c->ev_deq * 16u) | ERDP_EHB);
}

void xhci_processar_todos(void)
{
    if (!trava_tentar(&trava_xhci)) return;
    bool i0 = int_salva_desliga();
    for (int i = 0; i < qtd_ctls; i++)
        if (ctls[i].ok) processar(&ctls[i]);
    int_restaura(i0);
    trava_soltar(&trava_xhci);
}

static bool pode_dormir(void)
{
    return assincrono && processo_atual() && !contexto_irq();
}

/* Espera a que *feito se encienda (lo enciende processar), como mucho ms. */
static bool esperar(xhci_t *c, volatile bool *feito, uint32_t ms)
{
    prazo_t p = prazo_ms(ms);
    bool dormir = pode_dormir(), por_irq = dormir && c->irq_usavel, dormiu = false;
    uint32_t irqs0 = c->irqs;
    for (;;) {
        processar(c);
        if (*feito) break;
        if (le_op(c, OP_USBSTS) & (STS_HCH | STS_HSE | STS_HCE)) {
            if (!c->morto) klog("[XHCI] el controlador se ha parado (USBSTS %x)\n", le_op(c, OP_USBSTS));
            c->morto = true;
            return false;
        }
        if (prazo_vencido(&p)) { processar(c); break; }
        if (dormir) {
            if (por_irq) {
                bloquear(c);
                processo_atual()->acordar_em = relogio_tiques() + 1;
            }
            ceder();
            processo_atual()->acordar_em = 0;
            dormiu = true;
        }
    }
    if (por_irq && dormiu && *feito) {
        /* Como en el EHCI: si la espera durmió varias veces seguidas y la IRQ
         * no llegó nunca, la línea no llega y se pasa a ceder la CPU sin dormir. */
        if (c->irqs != irqs0) c->esperas_sem_irq = 0;
        else if (++c->esperas_sem_irq >= ESPERAS_SEM_IRQ) {
            c->irq_usavel = false;
            klog("[XHCI] la IRQ %u no llega: se espera cediendo la CPU\n", c->irq);
        }
    }
    return *feito;
}

/* ------------------------------------------------------------ órdenes */

static int comando(xhci_t *c, uint32_t p0, uint32_t ctl, uint32_t ms)
{
    if (c->morto) return USB_ERRO;
    trb_t v = { p0, 0, 0, ctl };
    uint32_t ini, fim;
    c->cmd_feito = false;
    anel_td(&c->cmd, &v, 1, TRB_NOOP_CMD, &ini, &fim);
    c->cmd_trb = ini;
    barreira();
    c->db[0] = 0;
    if (!esperar(c, &c->cmd_feito, ms)) {
        klog("[XHCI] la orden %u no ha terminado en %u ms\n", (ctl >> 10) & 0x3F, ms);
        return USB_ERRO_TEMPO;
    }
    if (c->cmd_codigo != CC_OK) {
        klog("[XHCI] orden %u (ranura %u): %s (%u)\n", (ctl >> 10) & 0x3F, ctl >> 24,
             nome_codigo(c->cmd_codigo), c->cmd_codigo);
        return USB_ERRO;
    }
    return 0;
}

/* ------------------------------------------------------------ contextos */

static volatile uint32_t *ctx_saida(xhci_t *c, xslot_t *s, uint32_t i)
{
    return (volatile uint32_t *)(s->saida + i * c->tam_ctx);
}

static volatile uint32_t *ctx_entrada(xhci_t *c, xslot_t *s, uint32_t i)   /* 0 = control, 1 = ranura, 1+dci */
{
    return (volatile uint32_t *)(s->entrada + i * c->tam_ctx);
}

static void entrada_limpar(xhci_t *c, xslot_t *s)
{
    memset((void *)s->entrada, 0, 33 * c->tam_ctx);
}

static xslot_t *slot_de(usb_disp_t *d)
{
    if (!d || d->tipo_ctl != USB_XHCI || !d->xctl) return NULL;
    xslot_t *s = slot_por_id(d->xctl, d->slot);
    return s && s->disp == d ? s : NULL;
}

/* Un endpoint parado por un error vuelve a funcionar: Reset Endpoint (si
 * está "halted") y Set TR Dequeue Pointer al final de lo que hay en su anillo. */
static void recuperar_ep(xhci_t *c, xslot_t *s, uint32_t dci)
{
    xep_t *e = &s->ep[dci];
    uint32_t estado = ctx_saida(c, s, dci)[0] & 7;
    if (estado == 2)   /* halted */
        comando(c, 0, (TRB_RESET_EP << 10) | ((uint32_t)s->id << 24) | (dci << 16), 1000);
    estado = ctx_saida(c, s, dci)[0] & 7;
    if (estado == 3)   /* stopped */
        comando(c, (e->anel.fis + e->anel.enq * 16u) | e->anel.ciclo,
                (TRB_SET_DEQ << 10) | ((uint32_t)s->id << 24) | (dci << 16), 1000);
    e->pendente = false;
    e->int_armado = false;
}

static void cancelar_ep(xhci_t *c, xslot_t *s, uint32_t dci)
{
    uint32_t estado = ctx_saida(c, s, dci)[0] & 7;
    if (estado == 1)   /* running */
        comando(c, 0, (TRB_STOP_EP << 10) | ((uint32_t)s->id << 24) | (dci << 16), 1000);
    recuperar_ep(c, s, dci);
}

/* Ejecuta un TD en el endpoint dci y espera su fin. 0 o USB_ERRO_*. */
static int td_executar(xhci_t *c, xslot_t *s, uint32_t dci, const trb_t *v, uint32_t n, uint32_t ms)
{
    xep_t *e = &s->ep[dci];
    if (!e->ativo || c->morto) return USB_ERRO;
    e->feito = false;
    e->curto = false;
    e->resto = e->resto_dados = 0;
    e->codigo = 0;
    anel_td(&e->anel, v, n, TRB_NOOP_TR, &e->td_ini, &e->td_fim);
    e->pendente = true;
    barreira();
    c->db[s->id] = dci;
    c->transferencias++;
    if (!esperar(c, &e->feito, ms)) {
        e->pendente = false;
        if (!c->morto) cancelar_ep(c, s, dci);
        return USB_ERRO_TEMPO;
    }
    e->pendente = false;
    if (e->codigo == CC_OK || e->codigo == CC_CURTO) return 0;
    c->erros++;
    if (e->codigo != CC_STALL || dci != 1)   /* el STALL del endpoint 0 es una respuesta normal */
        klog("[XHCI] ranura %u endpoint %u: %s (%u)\n", s->id, dci, nome_codigo(e->codigo), e->codigo);
    int r = e->codigo == CC_STALL ? USB_ERRO_PAROU : USB_ERRO;
    recuperar_ep(c, s, dci);
    return r;
}

/* ------------------------------------------------------------ dispositivos */

static void iso_soltar(struct xiso *is);

static void liberar_slot(xhci_t *c, xslot_t *s)
{
    for (int i = 1; i < 32; i++) {
        if (s->ep[i].fila) { s->ep[i].fila->usado = false; s->ep[i].fila = NULL; }   /* D238 */
        if (s->ep[i].iso) { iso_soltar(s->ep[i].iso); s->ep[i].iso = NULL; }          /* D275 */
        if (s->ep[i].ativo) { memfisica_liberar(s->ep[i].anel.fis); s->ep[i].ativo = false; }
    }
    if (s->saida) memfisica_liberar(s->saida);
    if (s->entrada) memfisica_liberar(s->entrada);
    ((volatile uint32_t *)c->dcbaa)[s->id * 2] = 0;
    memset(s, 0, sizeof *s);
}

static uint16_t mps0_inicial(uint8_t vel)
{
    switch (vel) {
    case USB_VEL_BAIXA: return 8;
    case USB_VEL_PLENA: return 8;    /* se lee b[7] y se corrige con Evaluate Context */
    case USB_VEL_ALTA: return 64;
    default: return 512;
    }
}

int xhci_endereco(usb_disp_t *d)
{
    xhci_t *c = d->xctl;
    if (!c || !c->ok || c->morto) return USB_ERRO;
    trava_pegar(&trava_xhci);
    bool i0 = int_salva_desliga();
    int r = comando(c, 0, TRB_EN_SLOT << 10, 1000);
    uint32_t id = c->cmd_slot;
    if (r == 0 && (id == 0 || id > (uint32_t)c->slots_en)) {
        klog("[XHCI] el controlador dio la ranura %u (maximo %d)\n", id, c->slots_en);
        r = USB_ERRO;
    }
    xslot_t *s = NULL;
    if (r == 0) {
        s = &c->slots[id];
        memset(s, 0, sizeof *s);
        s->id = (uint8_t)id;
        s->saida = memfisica_alocar();
        s->entrada = memfisica_alocar();
        if (!s->saida || !s->entrada || !anel_novo(&s->ep[1].anel)) {
            klog("[XHCI] falta memoria para la ranura %u\n", id);
            r = USB_ERRO;
        } else {
            s->usado = true;
            s->disp = d;
            s->ep[1].ativo = true;
            memset((void *)s->saida, 0, PAGINA);
            memset((void *)s->entrada, 0, PAGINA);
            ((volatile uint32_t *)c->dcbaa)[id * 2] = s->saida;
            ((volatile uint32_t *)c->dcbaa)[id * 2 + 1] = 0;
            entrada_limpar(c, s);
            uint16_t mps = mps0_inicial(d->velocidade);
            volatile uint32_t *ctl = ctx_entrada(c, s, 0), *sl = ctx_entrada(c, s, 1), *ep0 = ctx_entrada(c, s, 2);
            ctl[1] = 3;                                                   /* añadir A0 (ranura) y A1 (endpoint 0) */
            sl[0] = (d->rota & 0xFFFFF) | ((uint32_t)d->velocidade << 20) | (1u << 27);
            sl[1] = (uint32_t)(d->porta_raiz + 1) << 16;
            if (d->tt_slot) sl[2] = d->tt_slot | ((uint32_t)d->tt_porta << 8);
            ep0[1] = (3u << 1) | (4u << 3) | ((uint32_t)mps << 16);      /* 3 reintentos, control */
            ep0[2] = s->ep[1].anel.fis | 1;                               /* DCS = 1 */
            ep0[3] = 0;
            ep0[4] = 8;                                                   /* longitud media de TRB */
            r = comando(c, s->entrada, (TRB_ADDR_DEV << 10) | (id << 24), 3000);
            if (r == 0) {
                d->slot = (uint8_t)id;
                d->endereco = (uint8_t)(ctx_saida(c, s, 0)[3] & 0xFF);
                d->mps0 = mps > 255 ? 255 : (uint8_t)mps;   /* usb.c lo vuelve a leer del descriptor */
            }
        }
        if (r != 0) {
            if (s->usado || s->id) comando(c, 0, (TRB_DIS_SLOT << 10) | (id << 24), 1000);
            liberar_slot(c, s);
            s = NULL;
        }
    }
    int_restaura(i0);
    trava_soltar(&trava_xhci);
    return r;
}

int xhci_mps0(usb_disp_t *d, uint16_t mps)
{
    trava_pegar(&trava_xhci);
    bool i0 = int_salva_desliga();
    int r = USB_ERRO;
    xslot_t *s = slot_de(d);
    if (s) {
        xhci_t *c = d->xctl;
        entrada_limpar(c, s);
        ctx_entrada(c, s, 0)[1] = 2;   /* A1: endpoint 0 */
        volatile uint32_t *ep0 = ctx_entrada(c, s, 2);
        ep0[1] = (3u << 1) | (4u << 3) | ((uint32_t)mps << 16);
        r = comando(c, s->entrada, (TRB_EVAL_CTX << 10) | ((uint32_t)s->id << 24), 1000);
    }
    int_restaura(i0);
    trava_soltar(&trava_xhci);
    return r;
}

/* Intervalo del contexto (2^n microcuadros de 125 us) a partir del bInterval. */
static uint32_t intervalo_xhci(uint8_t vel, uint8_t b)
{
    if (vel == USB_VEL_BAIXA || vel == USB_VEL_PLENA) {
        uint32_t ms8 = (uint32_t)(b ? b : 1) * 8, n = 0;
        while ((2u << n) <= ms8) n++;    /* parte entera del log2 */
        return n < 3 ? 3 : n > 10 ? 10 : n;
    }
    uint32_t n = b ? b - 1u : 0;
    return n > 15 ? 15 : n;
}

static bool ep_preparar(xhci_t *c, xslot_t *s, uint32_t dci, uint32_t tipo, uint16_t mps, uint8_t burst,
                        uint32_t intervalo, uint32_t *max_dci)
{
    xep_t *e = &s->ep[dci];
    if (!e->ativo) {
        if (!anel_novo(&e->anel)) return false;
        e->ativo = true;
    } else {
        /* Reconfigurar: anillo nuevo desde el principio (el viejo se reutiliza vacío). */
        memset((void *)e->anel.t, 0, (NTRB - 1) * 16);
        e->anel.enq = 0;
        e->anel.ciclo = 1;
        e->anel.t[NTRB - 1].controle = (TRB_LINK << 10) | TRB_TC;
    }
    e->pendente = e->int_armado = e->int_erro = false;
    if (e->fila) { e->fila->usado = false; e->fila = NULL; }   /* D238: anillo nuevo, cola vacía */
    ctx_entrada(c, s, 0)[1] |= 1u << dci;
    volatile uint32_t *x = ctx_entrada(c, s, 1 + dci);
    uint32_t esit = (uint32_t)mps * (burst + 1u);
    x[0] = intervalo << 16;
    x[1] = (3u << 1) | (tipo << 3) | ((uint32_t)burst << 8) | ((uint32_t)mps << 16);
    x[2] = e->anel.fis | 1;
    x[3] = 0;
    x[4] = tipo == 7 ? (esit | (esit << 16)) : 3072;   /* interrupción: la carga por intervalo */
    if (dci > *max_dci) *max_dci = dci;
    return true;
}

int xhci_configurar(usb_disp_t *d)
{
    trava_pegar(&trava_xhci);
    bool i0 = int_salva_desliga();
    int r = USB_ERRO;
    xslot_t *s = slot_de(d);
    if (s) {
        xhci_t *c = d->xctl;
        entrada_limpar(c, s);
        ctx_entrada(c, s, 0)[1] = 1;   /* A0: la ranura (las entradas de contexto cambian) */
        uint32_t max_dci = (ctx_saida(c, s, 0)[0] >> 27) & 0x1F;
        if (max_dci < 1) max_dci = 1;
        bool ok = true;
        if (d->ep_entrada)
            ok = ok && ep_preparar(c, s, d->ep_entrada * 2u + 1, 6, d->mps_entrada, d->burst_entrada, 0, &max_dci);
        if (d->ep_saida)
            ok = ok && ep_preparar(c, s, d->ep_saida * 2u, 2, d->mps_saida, d->burst_saida, 0, &max_dci);
        for (int i = 0; i < d->n_hid; i++)
            ok = ok && ep_preparar(c, s, d->hid[i].ep * 2u + 1, 7, d->hid[i].mps, 0,
                                   intervalo_xhci(d->velocidade, d->hid[i].intervalo), &max_dci);
        if (d->ep_int)   /* D279: eventos HCI de un controlador Bluetooth */
            ok = ok && ep_preparar(c, s, d->ep_int * 2u + 1, 7, d->mps_int, 0, intervalo_xhci(d->velocidade, d->int_intervalo), &max_dci);
        volatile uint32_t *sl = ctx_entrada(c, s, 1), *so = ctx_saida(c, s, 0);
        sl[0] = (so[0] & 0x07FFFFFFu) | (max_dci << 27);
        sl[1] = so[1];
        sl[2] = so[2];
        if (ok && (ctx_entrada(c, s, 0)[1] & ~1u))
            r = comando(c, s->entrada, (TRB_CONF_EP << 10) | ((uint32_t)s->id << 24), 2000);
        else r = ok ? 0 : USB_ERRO;
    }
    int_restaura(i0);
    trava_soltar(&trava_xhci);
    return r;
}

int xhci_hub(usb_disp_t *d, uint8_t portas, uint8_t ttt, bool mtt)
{
    trava_pegar(&trava_xhci);
    bool i0 = int_salva_desliga();
    int r = USB_ERRO;
    xslot_t *s = slot_de(d);
    if (s) {
        xhci_t *c = d->xctl;
        entrada_limpar(c, s);
        ctx_entrada(c, s, 0)[1] = 1;
        volatile uint32_t *sl = ctx_entrada(c, s, 1), *so = ctx_saida(c, s, 0);
        sl[0] = so[0] | (1u << 26) | (mtt ? (1u << 25) : 0);
        sl[1] = (so[1] & 0x00FFFFFFu) | ((uint32_t)portas << 24);
        sl[2] = so[2] | (d->velocidade == USB_VEL_ALTA ? ((uint32_t)(ttt & 3) << 16) : 0);
        r = comando(c, s->entrada, (TRB_CONF_EP << 10) | ((uint32_t)s->id << 24), 2000);
    }
    int_restaura(i0);
    trava_soltar(&trava_xhci);
    return r;
}

void xhci_liberar(usb_disp_t *d)
{
    trava_pegar(&trava_xhci);
    bool i0 = int_salva_desliga();
    xslot_t *s = slot_de(d);
    if (s) {
        xhci_t *c = d->xctl;
        comando(c, 0, (TRB_DIS_SLOT << 10) | ((uint32_t)s->id << 24), 1000);
        liberar_slot(c, s);
    }
    d->slot = 0;
    int_restaura(i0);
    trava_soltar(&trava_xhci);
}

/* ------------------------------------------------------------ transferencias */

int xhci_controle(usb_disp_t *d, const uint8_t setup[8], void *dados, uint16_t tam, bool entrada)
{
    if (tam > CTRL_MAX) return USB_ERRO;
    trava_pegar(&trava_xhci);
    bool i0 = int_salva_desliga();
    int r = USB_ERRO;
    xslot_t *s = slot_de(d);
    if (s) {
        trb_t v[3];
        uint32_t n = 0, trt = tam == 0 ? 0 : entrada ? 3 : 2;
        v[n++] = (trb_t){ (uint32_t)setup[0] | (uint32_t)setup[1] << 8 | (uint32_t)setup[2] << 16 | (uint32_t)setup[3] << 24,
                          (uint32_t)setup[4] | (uint32_t)setup[5] << 8 | (uint32_t)setup[6] << 16 | (uint32_t)setup[7] << 24,
                          8, (TRB_SETUP << 10) | TRB_IDT | (trt << 16) };
        if (tam) {
            if (!entrada) memcpy(buffer, dados, tam);
            v[n++] = (trb_t){ (uint32_t)buffer, 0, tam, (TRB_DATA << 10) | TRB_ISP | (entrada ? TRB_DIR_IN : 0) };
        }
        v[n++] = (trb_t){ 0, 0, 0, (TRB_STATUS << 10) | TRB_IOC | ((tam && entrada) ? 0 : TRB_DIR_IN) };
        r = td_executar(d->xctl, s, 1, v, n, 2000);
        if (r == 0) {
            xep_t *e = &s->ep[1];
            uint32_t resto = e->curto ? e->resto_dados : 0;
            int feito = resto > tam ? 0 : (int)(tam - resto);
            if (entrada && feito) memcpy(dados, buffer, (uint32_t)feito);
            r = feito;
        }
    }
    int_restaura(i0);
    trava_soltar(&trava_xhci);
    return r;
}

int xhci_bulk(usb_disp_t *d, bool entrada, void *dados, uint32_t tam)
{
    if (tam > BUFFER_BYTES) return USB_ERRO;
    trava_pegar(&trava_xhci);
    bool i0 = int_salva_desliga();
    int r = USB_ERRO;
    xslot_t *s = slot_de(d);
    uint8_t ep = entrada ? d->ep_entrada : d->ep_saida;
    if (s && ep) {
        uint32_t dci = ep * 2u + (entrada ? 1 : 0);
        if (!entrada) memcpy(buffer, dados, tam);
        uint32_t feito = 0;
        r = 0;
        do {
            uint32_t end = (uint32_t)buffer + feito;
            uint32_t parte = tam - feito, ate64k = 0x10000u - (end & 0xFFFFu);
            if (parte > ate64k) parte = ate64k;
            trb_t v = { end, 0, parte, (TRB_NORMAL << 10) | TRB_IOC | TRB_ISP };
            r = td_executar(d->xctl, s, dci, &v, 1, 5000);
            if (r < 0) break;
            xep_t *e = &s->ep[dci];
            uint32_t resto = e->curto ? e->resto_dados : e->resto;
            uint32_t veio = resto > parte ? 0 : parte - resto;
            feito += veio;
            if (veio < parte) break;   /* paquete corto: la transferencia termina aquí */
        } while (feito < tam);
        if (r == 0) {
            if (entrada && feito) memcpy(dados, buffer, feito);
            r = (int)feito;
        }
    }
    int_restaura(i0);
    trava_soltar(&trava_xhci);
    return r;
}

/* Lectura de interrupción armada: queda en el anillo hasta que el dispositivo
 * mande algo (el teclado al pulsar una tecla). La consulta no espera. */
static int int_armar_sem_trava(xhci_t *c, xslot_t *s, uint32_t dci, uint16_t tam)
{
    xep_t *e = &s->ep[dci];
    if (!e->ativo || c->morto) return USB_ERRO;
    if (!e->int_buf) {
        if (s->n_buf >= (PAGINA - HID_BUF_DESLOC) / HID_BUF_TAM) return USB_ERRO;
        e->int_buf = s->entrada + HID_BUF_DESLOC + HID_BUF_TAM * s->n_buf++;
    }
    if (tam > HID_BUF_TAM && !e->int_grande) tam = HID_BUF_TAM;
    e->int_tam = tam;
    e->feito = e->curto = false;
    e->resto = e->resto_dados = 0;
    trb_t v = { e->int_buf, 0, tam, (TRB_NORMAL << 10) | TRB_IOC | TRB_ISP };
    anel_td(&e->anel, &v, 1, TRB_NOOP_TR, &e->td_ini, &e->td_fim);
    e->pendente = true;
    e->int_armado = true;
    barreira();
    c->db[s->id] = dci;
    return 0;
}

int xhci_int_armar(usb_disp_t *d, uint8_t ep, uint16_t tam)
{
    trava_pegar(&trava_xhci);
    bool i0 = int_salva_desliga();
    xslot_t *s = slot_de(d);
    int r = s ? int_armar_sem_trava(d->xctl, s, ep * 2u + 1, tam) : USB_ERRO;
    int_restaura(i0);
    trava_soltar(&trava_xhci);
    return r;
}

/* D215: recepción continua por el endpoint bulk de entrada (el pincho Wi-Fi):
 * la misma lectura armada que la de HID, sobre el buffer que da el driver
 * (físico, <= 16 KB y sin cruzar un límite de 64 KB). Se consulta con
 * xhci_int_ler, que la vuelve a armar. */
int xhci_rx_abrir(usb_disp_t *d, uint32_t buffer_fis, uint16_t tam)
{
    if (!d->ep_entrada || !buffer_fis || ((buffer_fis & 0xFFFFu) + tam) > 0x10000u) return USB_ERRO;
    trava_pegar(&trava_xhci);
    bool i0 = int_salva_desliga();
    xslot_t *s = slot_de(d);
    int r = USB_ERRO;
    if (s) {
        xep_t *e = &s->ep[d->ep_entrada * 2u + 1];
        if (e->ativo && e->int_grande && e->int_buf == buffer_fis) r = 0;   /* ya estaba armada (se reabre) */
        else if (e->ativo && !e->pendente) {
            e->int_buf = buffer_fis;
            e->int_grande = true;
            r = int_armar_sem_trava(d->xctl, s, d->ep_entrada * 2u + 1, tam);
        }
    }
    int_restaura(i0);
    trava_soltar(&trava_xhci);
    return r < 0 ? r : 0;
}

/* Sin el cerrojo: la tarea de HID no puede esperar a que acabe la copia de un
 * pendrive (el teclado se quedaría "pulsado" y la repetición escribiría de más).
 * Es seguro porque todo corre en la CPU 0 con las interrupciones desactivadas
 * y quien tiene el cerrojo solo lo suelta dentro de esperar(), con los anillos
 * en orden: aquí solo se consumen eventos (que quedan anotados en su endpoint)
 * y se arma otra lectura en el anillo de ESTE endpoint. Lo que necesita una
 * orden (recuperar el endpoint tras un error) espera a tener el cerrojo. */
int xhci_int_ler(usb_disp_t *d, uint8_t ep, uint8_t *dados, int max)
{
    bool i0 = int_salva_desliga();
    int r = USB_ERRO;
    xslot_t *s = slot_de(d);
    if (s) {
        xhci_t *c = d->xctl;
        uint32_t dci = ep * 2u + 1;
        xep_t *e = &s->ep[dci];
        processar(c);
        if (e->int_erro) {
            r = 0;
            if (trava_tentar(&trava_xhci)) {
                recuperar_ep(c, s, dci);
                e->int_erro = false;
                trava_soltar(&trava_xhci);
            }
        } else if (!e->int_armado) r = int_armar_sem_trava(c, s, dci, e->int_tam ? e->int_tam : 8);
        else if (!e->feito) r = 0;
        else {
            e->pendente = false;
            e->int_armado = false;
            if (e->codigo == CC_OK || e->codigo == CC_CURTO) {
                uint32_t resto = e->curto ? e->resto_dados : e->resto;
                int n = resto > e->int_tam ? 0 : (int)(e->int_tam - resto);
                if (n > max) n = max;
                memcpy(dados, (const void *)e->int_buf, (uint32_t)n);
                r = n;
                int_armar_sem_trava(c, s, dci, e->int_tam);
            } else {
                c->erros++;
                klog("[XHCI] ranura %u endpoint %u (interrupcion): %s (%u)\n", s->id, dci,
                     nome_codigo(e->codigo), e->codigo);
                e->int_erro = true;
                r = USB_ERRO;
            }
        }
    }
    int_restaura(i0);
    return r;
}

/* ------------------------------------------------------------ transferencias isócronas (D275) */

/* Un flujo isócrono por endpoint (el altavoz USB): un TD de un solo TRB "Isoch"
 * por intervalo de servicio, con SIA (empezar en cuanto se pueda, como hace Linux por
 * omisión): tras un hueco (el kernel se quedó sin atender el flujo) el siguiente TD
 * sale en el primer intervalo libre sin cuenta de cuadros. Los datos son un anillo
 * físico del driver; cada TRB apunta a un trozo (nunca cruza el final del anillo ni
 * una frontera de 64 KB). Solo cada ISO_IOC_CADA TRB pide evento (sin interrupción:
 * BEI); como los eventos de un endpoint llegan en orden, el evento de un TRB completa
 * todos los anteriores: así se cuenta lo completado (pos_feito) sin 1000 eventos por
 * segundo. Un hueco se anuncia con "Ring Underrun" (14). Todo lo hace el driver con las
 * interrupciones apagadas y sin el cerrojo (como el HID): aquí solo se escriben TRB, se
 * toca la campana y se consumen eventos. */
#define ISO_IOC_CADA   8
#define ISO_EM_VOO_MAX 200u
#define CC_SUBDESB     14      /* Ring Underrun */
#define CC_PERDIDO     23      /* Missed Service Error */
#define TRB_ISO_SIA    (1u << 31)
#define TRB_BEI        (1u << 9)

struct xiso {
    bool     usado, ligado;
    xhci_t  *c;
    xslot_t *s;
    uint8_t  dci;
    uint32_t anel_fis, anel_tam, bytes_s, mps;
    uint8_t  tam_quadro;
    uint32_t pos_fila;       /* desplazamiento del próximo trozo del anillo de datos */
    uint32_t pos_feito;      /* bytes completados desde ligar */
    uint32_t acc;
    uint32_t vel;            /* índice del TRB más antiguo sin completar */
    uint32_t em_voo;         /* TRB en el anillo sin completar (los que se saben) */
    uint32_t desde_ioc;
    uint16_t len[NTRB];
    bool     entrada;        /* D277: flujo de entrada (cámara): un TRB por paquete, con un evento cada uno */
    uint8_t  mult;           /* paquetes por intervalo de servicio (1 a 3) */
    uint32_t ivl_us;         /* microsegundos entre paquetes */
    uint32_t cola;           /* TRB que se mantienen en el anillo */
    isoent_t re;
    uint32_t paquetes, completados, subdesb, erros;
};
static struct xiso isos[XISO_MAX_TOTAL];

static void iso_soltar(struct xiso *is) { is->usado = false; is->ligado = false; }

static void iso_evento(struct xiso *is, uint32_t ptr, uint32_t cod, uint32_t resto)
{
    if (!is->usado) return;
    if (cod == CC_SUBDESB) { is->subdesb++; return; }       /* hueco: no completa nada */
    if (cod == CC_PARADO || cod == CC_PARADO_TAM) return;
    anel_t *a = &is->s->ep[is->dci].anel;
    if (ptr < a->fis || ptr >= a->fis + (NTRB - 1) * 16u) return;
    uint32_t idx = (ptr - a->fis) / 16u;
    uint32_t dist = (idx + (NTRB - 1) - is->vel) % (NTRB - 1);
    if (dist >= is->em_voo) return;                         /* evento de un TRB ya contado */
    if (cod != CC_OK && cod != CC_CURTO) is->erros++;
    for (uint32_t k = 0; k <= dist; k++) {
        if (is->entrada) {
            /* el último TRB de la tanda trae el resultado; los anteriores (sin evento) se dan por perdidos */
            bool ultimo = k == dist, ok = ultimo && (cod == CC_OK || cod == CC_CURTO);
            uint32_t n = is->len[is->vel];
            isoent_completar(&is->re, ok && resto <= n ? n - resto : 0, !ok);
            is->len[is->vel] = 0;
            is->completados++;
            if (++is->vel == NTRB - 1) is->vel = 0;
            continue;
        }
        is->pos_feito += is->len[is->vel];
        is->len[is->vel] = 0;
        is->completados++;
        if (++is->vel == NTRB - 1) is->vel = 0;
    }
    is->em_voo -= dist + 1;
}

/* Pone TRB hasta tener USB_ISO_COLA_MS milisegundos en el anillo. */
static void iso_encher(struct xiso *is)
{
    xhci_t *c = is->c;
    if (!is->ligado || c->morto) return;
    anel_t *a = &is->s->ep[is->dci].anel;
    bool algum = false;
    while (is->em_voo < (is->entrada ? is->cola : USB_ISO_COLA_MS) && is->em_voo < ISO_EM_VOO_MAX) {
        if (is->entrada) {
            if (!isoent_livres(&is->re)) break;                  /* el consumidor va atrasado: no se pide más */
            uint32_t off = isoent_postar(&is->re), n = is->mps;  /* mps ya incluye el multiplicador */
            uint32_t npk = (n + is->mps / is->mult - 1) / (is->mps / is->mult);
            uint32_t idx = a->enq;
            trb_t v = { is->anel_fis + off, 0, n, (TRB_ISOCH << 10) | TRB_ISO_SIA | TRB_IOC | TRB_BEI | (is->mult > 1 ? (npk - 1) << 12 : 0) };
            uint32_t ini, fim;
            anel_td(a, &v, 1, TRB_NOOP_TR, &ini, &fim);
            is->len[idx] = (uint16_t)n;
            is->em_voo++;
            is->paquetes++;
            algum = true;
            continue;
        }
        uint32_t n = usb_iso_calcular(&is->acc, is->bytes_s, is->tam_quadro, 1000, is->mps);
        uint32_t fis = is->anel_fis + is->pos_fila;
        uint32_t lim = is->anel_tam - is->pos_fila, a64 = 0x10000u - (fis & 0xFFFFu);
        if (lim > a64) lim = a64;
        if (n > lim) n = lim - lim % is->tam_quadro;
        uint32_t idx = a->enq;
        is->desde_ioc++;
        bool ioc = is->desde_ioc >= ISO_IOC_CADA || is->em_voo + 1 >= USB_ISO_COLA_MS;
        if (ioc) is->desde_ioc = 0;
        trb_t v = { fis, 0, n, (TRB_ISOCH << 10) | TRB_ISO_SIA | (ioc ? (TRB_IOC | TRB_BEI) : 0) };
        uint32_t ini, fim;
        anel_td(a, &v, 1, TRB_NOOP_TR, &ini, &fim);
        is->len[idx] = (uint16_t)n;
        is->em_voo++;
        is->paquetes++;
        is->pos_fila += n;
        if (is->pos_fila >= is->anel_tam) is->pos_fila -= is->anel_tam;
        algum = true;
    }
    if (algum) {
        barreira();
        c->db[is->s->id] = is->dci;
    }
}

int xhci_iso_abrir(usb_disp_t *d, const usb_iso_cfg_t *cfg)
{
    uint32_t mult = cfg->mult ? cfg->mult : 1;
    if (!cfg->mps || !cfg->anel_fis || mult > 3) return USB_ERRO;
    if (cfg->entrada ? (cfg->anel_tam % (cfg->mps * mult) != 0 || cfg->anel_tam / (cfg->mps * mult) > ISOENT_CASILLAS_MAX)
                     : (!cfg->tam_quadro || cfg->anel_tam % cfg->tam_quadro || mult > 1)) return USB_ERRO;
    trava_pegar(&trava_xhci);
    bool i0 = int_salva_desliga();
    int r = USB_ERRO;
    xslot_t *s = slot_de(d);
    struct xiso *is = NULL;
    for (int i = 0; i < XISO_MAX_TOTAL && s; i++) if (!isos[i].usado) { is = &isos[i]; r = i; break; }
    if (s && is) {
        xhci_t *c = d->xctl;
        uint32_t dci = cfg->ep * 2u + (cfg->entrada ? 1 : 0);
        xep_t *e = &s->ep[dci];
        r = USB_ERRO;
        bool ok = true;
        if (!e->ativo) { ok = anel_novo(&e->anel); e->ativo = ok; }
        else {
            memset((void *)e->anel.t, 0, (NTRB - 1) * 16);
            e->anel.enq = 0;
            e->anel.ciclo = 1;
            e->anel.t[NTRB - 1].controle = (TRB_LINK << 10) | TRB_TC;
        }
        if (ok) {
            e->pendente = e->int_armado = e->int_erro = false;
            if (e->fila) { e->fila->usado = false; e->fila = NULL; }
            entrada_limpar(c, s);
            ctx_entrada(c, s, 0)[1] = 1u << dci | 1;        /* A0 (ranura) y la A del endpoint */
            uint32_t max_dci = (ctx_saida(c, s, 0)[0] >> 27) & 0x1F;
            if (dci > max_dci) max_dci = dci;
            volatile uint32_t *sl = ctx_entrada(c, s, 1), *so = ctx_saida(c, s, 0);
            sl[0] = (so[0] & 0x07FFFFFFu) | (max_dci << 27);
            sl[1] = so[1];
            sl[2] = so[2];
            /* Intervalo en unidades de 125 us: 2^(bInterval-1) ms en plena velocidad, 2^(bInterval-1) microcuadros en alta */
            uint32_t b = cfg->intervalo ? cfg->intervalo : 1, iv = b - 1;
            if (d->velocidade <= USB_VEL_PLENA) iv += 3;
            if (iv > 15) iv = 15;
            volatile uint32_t *x = ctx_entrada(c, s, 1 + dci);
            x[0] = iv << 16;
            x[1] = (cfg->entrada ? 5u : 1u) << 3 | ((mult - 1) << 8) | ((uint32_t)cfg->mps << 16);   /* CErr 0; ráfaga = paquetes por microcuadro - 1 */
            x[2] = e->anel.fis | 1;
            x[3] = 0;
            x[4] = cfg->mps * mult | ((cfg->mps * mult) << 16);
            if (comando(c, s->entrada, (TRB_CONF_EP << 10) | ((uint32_t)s->id << 24), 2000) == 0) {
                memset(is, 0, sizeof *is);
                is->usado = true;
                is->c = c;
                is->s = s;
                is->dci = (uint8_t)dci;
                is->anel_fis = cfg->anel_fis;
                is->anel_tam = cfg->anel_tam;
                is->bytes_s = cfg->bytes_s;
                is->mps = cfg->mps * mult;          /* tamaño de la transacción completa (todas las ráfagas) */
                is->tam_quadro = cfg->tam_quadro;
                is->entrada = cfg->entrada;
                is->mult = (uint8_t)mult;
                is->ivl_us = 125u << iv;
                is->cola = USB_ISO_COLA_MS * 1000u / is->ivl_us;
                if (is->entrada && !isoent_iniciar(&is->re, cfg->anel_tam, cfg->mps * mult)) { is->usado = false; r = USB_ERRO; }
                else {
                e->iso = is;
                r = (int)(is - isos);
                }
            }
        }
    }
    int_restaura(i0);
    trava_soltar(&trava_xhci);
    return r;
}

static struct xiso *iso_de(usb_disp_t *d, int h)
{
    if (h < 0 || h >= XISO_MAX_TOTAL || !isos[h].usado) return NULL;
    xslot_t *s = slot_de(d);
    return s && isos[h].s == s ? &isos[h] : NULL;
}

void xhci_iso_ligar(usb_disp_t *d, int h)
{
    bool i0 = int_salva_desliga();
    struct xiso *is = iso_de(d, h);
    if (is) {
        processar(is->c);
        /* Lo que quedaba en el anillo sale con longitud cero para la cuenta */
        for (uint32_t k = 0, i = is->vel; k < is->em_voo; k++, i = (i + 1) % (NTRB - 1)) is->len[i] = 0;
        is->pos_fila = is->pos_feito = is->acc = is->desde_ioc = 0;
        if (is->entrada) isoent_reiniciar(&is->re);
        is->ligado = true;
        iso_encher(is);
    }
    int_restaura(i0);
}

void xhci_iso_parar(usb_disp_t *d, int h)
{
    bool i0 = int_salva_desliga();
    struct xiso *is = iso_de(d, h);
    if (is) is->ligado = false;
    int_restaura(i0);
}

uint32_t xhci_iso_posicao(usb_disp_t *d, int h)
{
    bool i0 = int_salva_desliga();
    struct xiso *is = iso_de(d, h);
    uint32_t r = 0;
    if (is) {
        if (!is->c->morto) processar(is->c);
        iso_encher(is);
        r = is->pos_feito;
    }
    int_restaura(i0);
    return r;
}

bool xhci_iso_ler(usb_disp_t *d, int h, uint32_t *off, uint32_t *len, bool *erro)
{
    bool i0 = int_salva_desliga();
    struct xiso *is = iso_de(d, h);
    bool r = is && is->entrada && isoent_ler(&is->re, off, len, erro);
    int_restaura(i0);
    return r;
}

bool xhci_iso_info(usb_disp_t *d, int h, usb_iso_info_t *i)
{
    struct xiso *is = iso_de(d, h);
    if (!is) return false;
    i->paquetes = is->paquetes;
    i->completados = is->completados;
    i->subdesbordes = is->subdesb;
    i->errores = is->erros;
    i->ancho = is->mps;
    return true;
}

/* Cierra el flujo y quita el endpoint del controlador (Configure Endpoint con el
 * bit de baja) si el dispositivo sigue ahí. */
void xhci_iso_fechar(usb_disp_t *d, int h)
{
    trava_pegar(&trava_xhci);
    bool i0 = int_salva_desliga();
    struct xiso *is = h >= 0 && h < XISO_MAX_TOTAL && isos[h].usado ? &isos[h] : NULL;
    if (is) {
        is->ligado = false;
        xslot_t *s = slot_de(d);
        if (s && is->s == s) {
            xhci_t *c = d->xctl;
            cancelar_ep(c, s, is->dci);
            s->ep[is->dci].iso = NULL;
            entrada_limpar(c, s);
            ctx_entrada(c, s, 0)[0] = 1u << is->dci;      /* D: baja del endpoint */
            ctx_entrada(c, s, 0)[1] = 1;
            volatile uint32_t *sl = ctx_entrada(c, s, 1), *so = ctx_saida(c, s, 0);
            sl[0] = so[0];
            sl[1] = so[1];
            sl[2] = so[2];
            comando(c, s->entrada, (TRB_CONF_EP << 10) | ((uint32_t)s->id << 24), 2000);
        }
        is->usado = false;
    }
    int_restaura(i0);
    trava_soltar(&trava_xhci);
}

/* ------------------------------------------------------------ colas de transferencias (D238) */

/* Pone la transferencia i de la cola al final del anillo de su endpoint y toca
 * la campana. Sin el cerrojo, como la lectura de HID: con las interrupciones
 * desactivadas y solo en el anillo de ESTE endpoint (ver xhci_int_ler). */
static void fila_por(xhci_t *c, xfila_t *f, uint32_t i, uint32_t n)
{
    xep_t *e = &f->slot->ep[f->dci];
    trb_t v = { f->buf[i], 0, n, (TRB_NORMAL << 10) | TRB_IOC | (f->entrada ? TRB_ISP : 0) };
    uint32_t ini, fim;
    f->feito[i] = false;
    f->cod[i] = 0;
    f->resto[i] = 0;
    anel_td(&e->anel, &v, 1, TRB_NOOP_TR, &ini, &fim);
    f->trb[i] = ini;
    barreira();
    c->db[f->slot->id] = f->dci;
}

static xfila_t *fila_de(usb_disp_t *d, bool entrada)
{
    xslot_t *s = slot_de(d);
    uint8_t ep = entrada ? d->ep_entrada : d->ep_saida;
    if (!s || !ep) return NULL;
    xfila_t *f = s->ep[ep * 2u + (entrada ? 1 : 0)].fila;
    return f && f->usado && f->entrada == entrada ? f : NULL;
}

/* Una transferencia terminó con error (el endpoint se para): Stop Endpoint si
 * sigue en marcha, Reset Endpoint y Set TR Dequeue al final del anillo (las que
 * quedaban se abandonan); las de entrada se vuelven a armar todas. Necesita el
 * cerrojo (son órdenes): si está ocupado, se intenta en la próxima consulta. */
static bool fila_recuperar(xhci_t *c, xfila_t *f)
{
    if (!trava_tentar(&trava_xhci)) return false;
    f->erros++;
    klog("[XHCI] cola %s de la ranura %u: %s (%u); se recupera el endpoint\n", f->entrada ? "de entrada" : "de salida",
         f->slot->id, nome_codigo(f->cod[f->prox]), f->cod[f->prox]);
    cancelar_ep(c, f->slot, f->dci);
    if (f->entrada) {
        f->prox = 0;
        f->emvoo = 0;
        for (uint32_t i = 0; i < f->n; i++) { fila_por(c, f, i, f->tam); f->emvoo++; }
    } else {
        f->perdidas += f->emvoo;
        f->emvoo = 0;
        f->prox = 0;
    }
    f->erro = false;
    trava_soltar(&trava_xhci);
    return true;
}

/* Si la más antigua no ha terminado pero una posterior sí, su evento se perdió
 * (terminan en orden): se da por terminada sin datos. */
static bool fila_perdeu_evento(xfila_t *f)
{
    for (uint32_t k = 1; k < f->emvoo; k++)
        if (f->feito[(f->prox + k) % f->n]) return true;
    return false;
}

/* Abre una cola de n transferencias de tam bytes sobre los buffers base,
 * base + tam, ... (físicos, del que llama, para siempre: el controlador puede
 * escribir en los de entrada hasta que se cierre la cola o se retire el
 * dispositivo). Ninguno puede cruzar un límite de 64 KB. 0 o USB_ERRO. */
int xhci_fila_abrir(usb_disp_t *d, bool entrada, uint32_t base, uint32_t tam, int n)
{
    if (n < 1 || n > XFILA_MAX || !base || !tam || tam > 0x10000u) return USB_ERRO;
    for (int i = 0; i < n; i++) {
        uint32_t b = base + (uint32_t)i * tam;
        if ((b & 0xFFFFu) + tam > 0x10000u) return USB_ERRO;
    }
    trava_pegar(&trava_xhci);
    bool i0 = int_salva_desliga();
    int r = USB_ERRO;
    xslot_t *s = slot_de(d);
    uint8_t ep = entrada ? d->ep_entrada : d->ep_saida;
    xfila_t *f = NULL;
    for (int k = 0; k < XFILAS && !f; k++) if (!filas[k].usado) f = &filas[k];
    if (s && ep && f && !d->xctl->morto) {
        uint32_t dci = ep * 2u + (entrada ? 1 : 0);
        xep_t *e = &s->ep[dci];
        if (e->ativo && !e->fila && !e->pendente && !e->int_armado) {
            memset(f, 0, sizeof *f);
            f->usado = true;
            f->entrada = entrada;
            f->n = (uint8_t)n;
            f->dci = (uint8_t)dci;
            f->slot = s;
            f->tam = tam;
            for (int i = 0; i < n; i++) f->buf[i] = base + (uint32_t)i * tam;
            e->fila = f;
            if (entrada)
                for (int i = 0; i < n; i++) { fila_por(d->xctl, f, (uint32_t)i, tam); f->emvoo++; }
            r = 0;
        }
    }
    int_restaura(i0);
    trava_soltar(&trava_xhci);
    return r;
}

/* La más antigua de la cola de entrada, si ya terminó: se copia y se vuelve
 * a armar. >0 bytes, 0 nada todavía, <0 error (el endpoint se recupera). */
int xhci_fila_ler(usb_disp_t *d, uint8_t *dados, int max)
{
    bool i0 = int_salva_desliga();
    int r = USB_ERRO;
    xfila_t *f = fila_de(d, true);
    if (f) {
        xhci_t *c = d->xctl;
        processar(c);
        uint32_t i = f->prox;
        r = 0;
        if (!f->emvoo) {
            /* Todas cortadas por una recuperación a medias: se vuelven a armar. */
            if (f->erro) fila_recuperar(c, f);
        } else if (f->feito[i] && f->cod[i] != CC_OK && f->cod[i] != CC_CURTO) {
            r = USB_ERRO;
            fila_recuperar(c, f);
        } else if (f->feito[i] || fila_perdeu_evento(f)) {
            if (f->feito[i]) {
                uint32_t resto = f->resto[i];
                int n = resto > f->tam ? 0 : (int)(f->tam - resto);
                if (n > max) n = max;
                if (n > 0) memcpy(dados, (const void *)f->buf[i], (uint32_t)n);
                r = n;
                f->completadas++;
            } else f->perdidas++;
            f->prox = (uint8_t)((i + 1) % f->n);
            f->emvoo--;
            fila_por(c, f, i, f->tam);
            f->emvoo++;
        }
    }
    int_restaura(i0);
    return r;
}

/* Recoge las de salida que ya terminaron (en orden). */
static void fila_recolher(xhci_t *c, xfila_t *f)
{
    processar(c);
    while (f->emvoo) {
        uint32_t i = f->prox;
        if (!f->feito[i]) {
            if (!fila_perdeu_evento(f)) break;
            f->perdidas++;
        } else if (f->cod[i] != CC_OK && f->cod[i] != CC_CURTO) {
            fila_recuperar(c, f);
            break;
        } else f->completadas++;
        f->prox = (uint8_t)((i + 1) % f->n);
        f->emvoo--;
    }
}

/* Pone n bytes en la cola de salida sin esperar a que se manden. n (>0) si
 * se puso; 0 si la cola está llena (las anteriores aún no han salido); <0 error. */
int xhci_fila_enviar(usb_disp_t *d, const void *dados, uint32_t n)
{
    bool i0 = int_salva_desliga();
    int r = USB_ERRO;
    xfila_t *f = fila_de(d, false);
    if (f && n && n <= f->tam && !d->xctl->morto) {
        xhci_t *c = d->xctl;
        fila_recolher(c, f);
        if (f->emvoo >= f->n) r = 0;
        else {
            uint32_t i = (f->prox + f->emvoo) % f->n;
            memcpy((void *)f->buf[i], dados, n);
            fila_por(c, f, i, n);
            f->emvoo++;
            r = (int)n;
        }
    }
    int_restaura(i0);
    return r;
}

/* Contadores de una cola: en vuelo (en el anillo y sin terminar; sin mirar
 * los eventos nuevos), terminadas bien y perdidas. false = no hay cola. */
bool xhci_fila_info(usb_disp_t *d, bool entrada, uint32_t *emvoo, uint32_t *completadas, uint32_t *perdidas)
{
    bool i0 = int_salva_desliga();
    xfila_t *f = fila_de(d, entrada);
    if (f) {
        /* Solo lee la memoria (se llama también desde «wifi estado» y la bandeja). */
        uint32_t v = 0;
        for (uint32_t k = 0; k < f->emvoo; k++) if (!f->feito[(f->prox + k) % f->n]) v++;
        *emvoo = v;
        *completadas = f->completadas;
        *perdidas = f->perdidas;
    }
    int_restaura(i0);
    return f != NULL;
}

/* Cierra la cola: Stop Endpoint y Set TR Dequeue (lo que quedaba en el anillo
 * se abandona); el endpoint queda libre para la manera de siempre. */
void xhci_fila_fechar(usb_disp_t *d, bool entrada)
{
    trava_pegar(&trava_xhci);
    bool i0 = int_salva_desliga();
    xfila_t *f = fila_de(d, entrada);
    if (f) {
        xep_t *e = &f->slot->ep[f->dci];
        if (!d->xctl->morto) cancelar_ep(d->xctl, f->slot, f->dci);
        e->fila = NULL;
        f->usado = false;
    }
    int_restaura(i0);
    trava_soltar(&trava_xhci);
}

/* El dueño del cerrojo murió a mitad de una transferencia (Ctrl+C durante una
 * copia): sus TD pueden seguir en los anillos. Se cancelan. */
static void recuperar_trava(void)
{
    for (int i = 0; i < qtd_ctls; i++) {
        xhci_t *c = &ctls[i];
        if (!c->ok || c->morto) continue;
        for (int id = 1; id <= c->slots_en; id++) {
            xslot_t *s = &c->slots[id];
            if (!s->usado) continue;
            for (uint32_t dci = 1; dci < 32; dci++)
                if (s->ep[dci].ativo && s->ep[dci].pendente && !s->ep[dci].int_armado) cancelar_ep(c, s, dci);
        }
    }
}

/* ------------------------------------------------------------ puertos */

uint32_t xhci_porta_estado(xhci_t *c, int p) { return p >= 0 && p < c->portas ? le_porta(c, p) : 0; }
bool xhci_porta_conectada(xhci_t *c, int p) { return (xhci_porta_estado(c, p) & P_CCS) != 0; }
int xhci_porta_protocolo(xhci_t *c, int p) { return p >= 0 && p < XHCI_MAX_PORTAS ? c->protocolo[p] : 0; }
void xhci_porta_resultado(xhci_t *c, int p, int r) { if (p >= 0 && p < XHCI_MAX_PORTAS) c->resultado[p] = r; }

bool xhci_porta_mudou(xhci_t *c, int p)
{
    uint32_t v = le_porta(c, p);
    if (!(v & P_MUDANCAS)) return false;
    escreve_porta(c, p, (v & P_NEUTRO) | (v & P_MUDANCAS));   /* escribe 1 para limpiar */
    return (v & P_CSC) != 0;
}

int xhci_porta_resetar(xhci_t *c, int p)
{
    uint32_t v = le_porta(c, p);
    if (!(v & P_CCS)) return USB_ERRO;
    bool usb3 = c->protocolo[p] == 3;
    if (!(usb3 && (v & P_PED))) {
        /* USB 2.0: el reset lo habilita. USB 3.0: el enlace ya se entrenó solo; si
         * no se habilitó, un reset (y, si tampoco, un "warm reset"). */
        for (int tentativa = 0; tentativa < (usb3 ? 2 : 1); tentativa++) {
            uint32_t bit = tentativa ? P_WPR : P_PR, fim = tentativa ? P_WRC : P_PRC;
            escreve_porta(c, p, (le_porta(c, p) & P_NEUTRO) | bit);
            /* D227: en la tarea "usb" la espera duerme (usb_pausa_ms), no congela el sistema. */
            prazo_t pz = prazo_ms(500);
            while (!(le_porta(c, p) & fim) && !prazo_vencido(&pz)) usb_pausa_ms(NULL, 1);
            v = le_porta(c, p);
            escreve_porta(c, p, (v & P_NEUTRO) | P_PRC | P_WRC | P_PEC);
            usb_pausa_ms(NULL, 10);   /* recuperación tras el reset (USB 2.0, 7.1.7.5) */
            if (le_porta(c, p) & P_PED) break;
        }
        v = le_porta(c, p);
        if (!(v & P_PED)) {
            klog("[XHCI] puerto %d: no se habilito despues del reset (PORTSC %x)\n", p + 1, v);
            return USB_ERRO;
        }
    }
    int vel = (int)((v & P_VEL) >> 10);
    return vel ? vel : USB_ERRO;
}

/* ------------------------------------------------------------ interrupciones */

/* Línea PCI (quizá compartida): confirma el aviso y despierta a quien espera. */
static bool atender(void)
{
    bool meu = false;
    for (int i = 0; i < qtd_ctls; i++) {
        xhci_t *c = &ctls[i];
        if (!c->ok || !c->intx) continue;
        uint32_t st = le_op(c, OP_USBSTS);
        if (st == 0xFFFFFFFFu || !(st & STS_EINT)) continue;
        escreve_op(c, OP_USBSTS, STS_EINT | (st & STS_PCD));   /* solo esos bits (los demás también se borran con 1) */
        if ((st & STS_PCD) && canal_usb) acordar(canal_usb);   /* D227: un puerto ha cambiado */
        escreve_rt(c, RT_IMAN, le_rt(c, RT_IMAN) | IMAN_IP | IMAN_IE);
        c->irqs++;
        if (c->prova) c->provas++;
        acordar(c);
        if (canal_hid) acordar(canal_hid);
        if (canal_fila) acordar(canal_fila);   /* D238: el Wi-Fi lee sus colas sin esperar al tic */
        meu = true;
    }
    return meu;
}

/* D211: la autoprueba de la IRQ con una orden No-Op: su evento de fin pide
 * interrupción. Con el cerrojo del xHCI tomado. */
static bool sonda_provocar(void *ctx)
{
    xhci_t *c = ctx;
    if (!c->ok || c->morto || !trava_tentar(&trava_xhci)) return false;
    bool i0 = int_salva_desliga();
    processar(c);
    c->prova = true;
    trb_t v = { 0, 0, 0, TRB_NOOP_CMD << 10 };
    uint32_t ini, fim;
    c->cmd_feito = false;
    anel_td(&c->cmd, &v, 1, TRB_NOOP_CMD, &ini, &fim);
    c->cmd_trb = ini;
    barreira();
    c->db[0] = 0;
    int_restaura(i0);
    return true;
}

static uint32_t sonda_contador(void *ctx) { return ((xhci_t *)ctx)->provas; }

static void sonda_terminar(void *ctx, bool chegou)
{
    xhci_t *c = ctx;
    bool i0 = int_salva_desliga();
    esperar(c, &c->cmd_feito, 100);
    c->prova = false;
    if (chegou && c->irq) { c->irq_usavel = true; c->esperas_sem_irq = 0; }
    int_restaura(i0);
    trava_soltar(&trava_xhci);
}

/* Se usa la línea INTx: si la BIOS dejó encendido el MSI o el MSI-X, el
 * controlador no la usaría y la IRQ no llegaría nunca. Se apagan los dos. */
static void apagar_msi(xhci_t *c)
{
    if (!(pci_le32(&c->pci, 0x04) & (1u << 20))) return;   /* sin lista de capacidades */
    uint8_t p = (uint8_t)(pci_le32(&c->pci, 0x34) & 0xFC);
    for (int volta = 0; p >= 0x40 && volta < 48; volta++) {
        uint32_t v = pci_le32(&c->pci, p);
        uint8_t id = (uint8_t)v;
        if (id == 0x05 && (v & (1u << 16))) {
            pci_escreve32(&c->pci, p, v & ~(1u << 16));
            klog("[XHCI] MSI apagado (se usa la linea INTx)\n");
        } else if (id == 0x11 && (v & (1u << 31))) {
            pci_escreve32(&c->pci, p, v & ~(1u << 31));
            klog("[XHCI] MSI-X apagado (se usa la linea INTx)\n");
        }
        p = (uint8_t)((v >> 8) & 0xFC);
    }
}

/* D450: al apagar o reiniciar, con las interrupciones cerradas para siempre: cada controlador
 * se para (Run/Stop = 0, sin interrupciones) y se espera el HCHalted como mucho 20 ms (la
 * especificación pide 16 microcuadros: 2 ms). Como el xhci_shutdown de Linux: algunos
 * firmwares se cuelgan al arrancar de nuevo si el xHCI sigue escribiendo en la RAM. */
int xhci_parar_todos(void)
{
    int paradas = 0;
    for (int i = 0; i < qtd_ctls; i++) {
        xhci_t *c = &ctls[i];
        if (!c->ok) continue;
        escreve_op(c, OP_USBCMD, le_op(c, OP_USBCMD) & ~(CMD_RS | CMD_INTE));
        if (esperar_op(c, OP_USBSTS, STS_HCH, STS_HCH, 20)) paradas++;
        else klog("[XHCI] %d: no se ha parado en 20 ms (USBSTS %x): se sigue\n", i, le_op(c, OP_USBSTS));
        c->morto = true;
    }
    return paradas;
}

void xhci_modo_assincrono(bool ligado)
{
    if (ligado) {
        for (int i = 0; i < qtd_ctls; i++) {
            xhci_t *c = &ctls[i];
            if (!c->ok) continue;
            uint32_t l = pci_le32(&c->pci, 0x3C) & 0xFF;
            c->irq = (l >= 1 && l < 16 && l != 2) ? (uint8_t)l : 0;
            c->irq_usavel = c->irq != 0;
            c->esperas_sem_irq = 0;
            if (c->irq) {
                /* INTx (el MSI/MSI-X no se usa: necesitaría el APIC local
                 * encendido en todas las imágenes). El bit "interrupt disable"
                 * del comando PCI se apaga por si la BIOS lo dejó puesto. */
                apagar_msi(c);
                pci_escreve32(&c->pci, 0x04, pci_le32(&c->pci, 0x04) & ~(1u << 10));
                c->intx = true;
                irq_compartilhar(c->irq, atender);
                bool i0 = int_salva_desliga();
                escreve_rt(c, RT_IMAN, IMAN_IP | IMAN_IE);
                escreve_op(c, OP_USBCMD, le_op(c, OP_USBCMD) | CMD_INTE);
                int_restaura(i0);
                irq_sonda_t s = { "xHCI", c->pci, c->irq, sonda_provocar, sonda_contador, sonda_terminar, c };
                irq_sonda_registrar(&s);
            }
            if (c->irq) klog("[XHCI] %d: espera por interrupcion (IRQ %u, INTx)\n", i, c->irq);
            else klog("[XHCI] %d: espera cediendo la CPU (sin IRQ)\n", i);
        }
    }
    assincrono = ligado;
}

/* ------------------------------------------------------------ inicialización */

/* Tamaño de la barra 0 (escribiendo unos, con la decodificación apagada un instante). */
static uint32_t tamanho_barra(xhci_t *c, bool par64)
{
    uint32_t cmd = pci_le32(&c->pci, 0x04);
    bool i0 = int_salva_desliga();
    pci_escreve32(&c->pci, 0x04, cmd & ~0x3u);
    uint32_t orig = pci_le32(&c->pci, 0x10), orig_alta = par64 ? pci_le32(&c->pci, 0x14) : 0;
    pci_escreve32(&c->pci, 0x10, 0xFFFFFFFFu);
    uint32_t m = pci_le32(&c->pci, 0x10) & ~0xFu;
    pci_escreve32(&c->pci, 0x10, orig);
    if (par64) pci_escreve32(&c->pci, 0x14, orig_alta);
    pci_escreve32(&c->pci, 0x04, cmd);
    int_restaura(i0);
    uint32_t t = m ? (~m + 1) : 0;
    return t;
}

/* Capacidades extendidas: el traspaso de la BIOS (id 1) y los protocolos de
 * cada puerto (id 2, "Supported Protocol": USB 2.0 o USB 3.x). */
static void capacidades_extendidas(xhci_t *c, bool so_bios)
{
    uint32_t hcc = *(volatile uint32_t *)(c->cap + CAP_HCCPARAMS1);
    uint32_t desl = (hcc >> 16) << 2;
    for (int volta = 0; desl && desl + 16 <= c->tam_barra && volta < 64; volta++) {
        volatile uint32_t *x = (volatile uint32_t *)(c->cap + desl);
        uint32_t v = x[0], id = v & 0xFF;
        if (id == 1 && so_bios) {
            if (v & (1u << 16)) {
                x[0] = v | (1u << 24);   /* "el sistema lo quiere" */
                prazo_t p = prazo_ms(1000);
                while ((x[0] & (1u << 16)) && !prazo_vencido(&p)) ;
                if (x[0] & (1u << 16)) {
                    /* Como Linux: si la BIOS no lo suelta en 1 s, se le quita. */
                    x[0] = (x[0] & ~(1u << 16)) | (1u << 24);
                    c->bios = -1;
                } else c->bios = 1;
                klog("[XHCI] la BIOS %s el controlador\n", c->bios > 0 ? "ha soltado" : "NO soltaba (forzado)");
            }
            /* USBLEGCTLSTS: sin SMI (se apagan los permisos y se borran los avisos). */
            x[1] = (x[1] & ((7u << 1) | (0xFFu << 5) | (7u << 17))) | (7u << 29);
        } else if (id == 2 && !so_bios) {
            uint32_t maior = v >> 24, ini = x[2] & 0xFF, n = (x[2] >> 8) & 0xFF;
            for (uint32_t p = ini; p && p < ini + n && p <= (uint32_t)c->portas; p++)
                if (p - 1 < XHCI_MAX_PORTAS) c->protocolo[p - 1] = (uint8_t)maior;
            klog("[XHCI] protocolo USB %u.%u: puertos %u a %u\n", maior, (v >> 16) & 0xFF, ini, ini + n - 1);
        }
        uint32_t prox = (v >> 8) & 0xFF;
        if (!prox) break;
        desl += prox << 2;
    }
}

/* Los chipsets de Intel de las series 7 a 9 (Panther Point, Lynx Point, Wildcat
 * Point) tienen EHCI y xHCI con los mismos conectores, y la BIOS puede dejarlos
 * en el EHCI (los USB 3.0 entonces ni se encienden). Como Linux, se pasan al
 * xHCI todos los que el chipset deja pasar (USB3PRM -> USB3_PSSEN y
 * USB2PRM -> XUSB2PR); el EHCI se queda con lo que no se pueda mover. */
static void intel_portas_para_xhci(xhci_t *c)
{
    static const uint16_t ids[] = { 0x1E31, 0x8C31, 0x9C31, 0x9CB1, 0x8CB1, 0x8D31 };
    if (c->pci.fabricante != 0x8086) return;
    bool achou = false;
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) if (ids[i] == c->pci.modelo) achou = true;
    if (!achou) return;
    uint32_t usb3 = pci_le32(&c->pci, 0xDC) & 0xFFFFu;
    pci_escreve32(&c->pci, 0xD8, usb3);
    uint32_t usb2 = pci_le32(&c->pci, 0xD4) & 0xFFFFu;
    pci_escreve32(&c->pci, 0xD0, usb2);
    klog("[XHCI] Intel: puertos pasados del EHCI al xHCI (USB 3.0 %x, USB 2.0 %x)\n",
         pci_le32(&c->pci, 0xD8) & 0xFFFFu, pci_le32(&c->pci, 0xD0) & 0xFFFFu);
}

static bool alocar_pagina_zero(uint32_t *p)
{
    *p = memfisica_alocar();
    if (!*p) return false;
    memset((void *)*p, 0, PAGINA);
    return true;
}

static bool iniciar(xhci_t *c)
{
    for (int p = 0; p < XHCI_MAX_PORTAS; p++) c->resultado[p] = -1;
    uint32_t b0 = pci_le32(&c->pci, 0x10);
    if (b0 & 1) { klog("[XHCI] %04x:%04x: la barra 0 no es de memoria\n", c->pci.fabricante, c->pci.modelo); return false; }
    bool par64 = (b0 & 6) == 4;
    if (par64 && pci_le32(&c->pci, 0x14)) {
        klog("[XHCI] %04x:%04x: los registros estan por encima de 4 GB\n", c->pci.fabricante, c->pci.modelo);
        return false;
    }
    c->barra = b0 & ~0xFu;
    c->tam_barra = tamanho_barra(c, par64);
    if (!c->barra || c->tam_barra < 0x1000) { klog("[XHCI] barra no utilizable\n"); return false; }
    if (c->tam_barra > 0x100000) c->tam_barra = 0x100000;
    /* PCI: memoria y maestro del bus (el DMA de los anillos). */
    pci_escreve32(&c->pci, 0x04, pci_le32(&c->pci, 0x04) | 0x6);
    uint32_t v = janela_mapear_tipo(c->barra, c->tam_barra, PG_PCD);
    if (!v) { klog("[XHCI] no se ha podido mapear la barra\n"); return false; }
    c->cap = (volatile uint8_t *)v;

    uint32_t cap0 = *(volatile uint32_t *)(c->cap + CAP_CAPLENGTH);
    uint32_t caplen = cap0 & 0xFF;
    c->versao = (uint16_t)(cap0 >> 16);
    uint32_t hcs1 = *(volatile uint32_t *)(c->cap + CAP_HCSPARAMS1);
    uint32_t hcs2 = *(volatile uint32_t *)(c->cap + CAP_HCSPARAMS2);
    uint32_t hcc1 = *(volatile uint32_t *)(c->cap + CAP_HCCPARAMS1);
    uint32_t dboff = *(volatile uint32_t *)(c->cap + CAP_DBOFF) & ~3u;
    uint32_t rtsoff = *(volatile uint32_t *)(c->cap + CAP_RTSOFF) & ~0x1Fu;
    c->slots_max = (int)(hcs1 & 0xFF);
    c->n_intr = (int)((hcs1 >> 8) & 0x7FF);
    c->portas = (int)((hcs1 >> 24) & 0xFF);
    c->n_scratch = (int)((((hcs2 >> 21) & 0x1F) << 5) | ((hcs2 >> 27) & 0x1F));
    c->ac64 = hcc1 & 1;
    c->ppc = (hcc1 >> 3) & 1;
    c->tam_ctx = (hcc1 & (1u << 2)) ? 64 : 32;
    if (caplen < 0x20 || dboff >= c->tam_barra || rtsoff >= c->tam_barra || c->portas == 0) {
        klog("[XHCI] registros de capacidad absurdos (CAPLENGTH %x, DBOFF %x, RTSOFF %x)\n", caplen, dboff, rtsoff);
        return false;
    }
    c->op = c->cap + caplen;
    c->rt = c->cap + rtsoff;
    c->db = (volatile uint32_t *)(c->cap + dboff);
    if (c->portas > XHCI_MAX_PORTAS) {
        klog("[XHCI] %d puertos: solo se usan los %d primeros\n", c->portas, XHCI_MAX_PORTAS);
        c->portas = XHCI_MAX_PORTAS;
    }
    c->slots_en = c->slots_max < XHCI_MAX_SLOTS ? c->slots_max : XHCI_MAX_SLOTS;

    capacidades_extendidas(c, true);   /* BIOS -> sistema */

    /* Parar y reiniciar. */
    escreve_op(c, OP_USBCMD, le_op(c, OP_USBCMD) & ~(CMD_RS | CMD_INTE));
    if (!esperar_op(c, OP_USBSTS, STS_HCH, STS_HCH, 100)) klog("[XHCI] no se ha parado (USBSTS %x)\n", le_op(c, OP_USBSTS));
    escreve_op(c, OP_USBCMD, CMD_HCRST);
    xhci_esperar_ms(1);   /* algunos Intel no aceptan lecturas justo después (Linux) */
    if (!esperar_op(c, OP_USBCMD, CMD_HCRST, 0, 1000) || !esperar_op(c, OP_USBSTS, STS_CNR, 0, 1000)) {
        klog("[XHCI] el reinicio no ha terminado (USBCMD %x, USBSTS %x)\n", le_op(c, OP_USBCMD), le_op(c, OP_USBSTS));
        return false;
    }
    if (!(le_op(c, OP_PAGESIZE) & 1))
        klog("[XHCI] aviso: paginas de %x (se usan de 4 KB)\n", le_op(c, OP_PAGESIZE));

    capacidades_extendidas(c, false);   /* protocolos de los puertos */
    intel_portas_para_xhci(c);

    /* Memoria: DCBAA, scratchpad, anillos de órdenes y eventos, ERST. */
    if (!alocar_pagina_zero(&c->dcbaa) || !anel_novo(&c->cmd) || !alocar_pagina_zero(&c->ev_fis) ||
        !alocar_pagina_zero(&c->erst)) {
        klog("[XHCI] falta memoria para los anillos\n");
        return false;
    }
    if (c->n_scratch) {
        uint32_t paginas_arr = ((uint32_t)c->n_scratch * 8 + PAGINA - 1) / PAGINA;
        c->scratch = memfisica_alocar_seguidas(paginas_arr);
        if (!c->scratch) { klog("[XHCI] falta memoria para el scratchpad\n"); return false; }
        memset((void *)c->scratch, 0, paginas_arr * PAGINA);
        volatile uint32_t *arr = (volatile uint32_t *)c->scratch;
        for (int i = 0; i < c->n_scratch; i++) {
            uint32_t pg;
            if (!alocar_pagina_zero(&pg)) { klog("[XHCI] falta memoria para el scratchpad\n"); return false; }
            arr[i * 2] = pg;
            arr[i * 2 + 1] = 0;
        }
        ((volatile uint32_t *)c->dcbaa)[0] = c->scratch;
    }
    c->ev = (volatile trb_t *)c->ev_fis;
    c->ev_deq = 0;
    c->ev_ciclo = 1;
    volatile uint32_t *erst = (volatile uint32_t *)c->erst;
    erst[0] = c->ev_fis;
    erst[1] = 0;
    erst[2] = NTRB;
    erst[3] = 0;

    escreve_op(c, OP_CONFIG, (le_op(c, OP_CONFIG) & ~0xFFu) | (uint32_t)c->slots_en);
    escreve64_op(c, OP_DCBAAP, c->dcbaa);
    escreve64_op(c, OP_CRCR, c->cmd.fis | 1);   /* RCS = 1 */
    escreve_op(c, OP_DNCTRL, 0);
    escreve_rt(c, RT_ERSTSZ, 1);
    escreve64_rt(c, RT_ERDP, c->ev_fis);
    escreve64_rt(c, RT_ERSTBA, c->erst);
    escreve_rt(c, RT_IMOD, 160);           /* 40 us entre interrupciones */
    escreve_rt(c, RT_IMAN, IMAN_IP);       /* sin interrupciones hasta xhci_modo_assincrono */
    escreve_op(c, OP_USBCMD, CMD_RS);
    if (!esperar_op(c, OP_USBSTS, STS_HCH, 0, 100)) {
        klog("[XHCI] no ha arrancado (USBSTS %x)\n", le_op(c, OP_USBSTS));
        return false;
    }
    c->ok = true;

    /* Una orden No-Op comprueba los anillos de órdenes y eventos. */
    int r = comando(c, 0, TRB_NOOP_CMD << 10, 500);
    if (r < 0) {
        klog("[XHCI] el anillo de ordenes no responde\n");
        c->ok = false;
        escreve_op(c, OP_USBCMD, 0);
        return false;
    }

    /* Encender los puertos (si el controlador lo controla) y esperar a que se estabilicen. */
    if (c->ppc)
        for (int p = 0; p < c->portas; p++) {
            uint32_t ps = le_porta(c, p);
            if (!(ps & P_PP)) escreve_porta(c, p, (ps & P_NEUTRO) | P_PP);
        }
    xhci_esperar_ms(c->ppc ? 120 : 20);

    int n2 = 0, n3 = 0;
    for (int p = 0; p < c->portas; p++) { if (c->protocolo[p] == 3) n3++; else if (c->protocolo[p] == 2) n2++; }
    klog("[XHCI] controlador %02x:%02x.%x (%04x:%04x) version %x.%02x em %x (%u KB): %d puertos (%d USB 2.0, %d USB 3.x), "
         "%d ranuras, contextos de %u bytes, %d paginas de scratchpad, %s\n",
         c->pci.barramento, c->pci.dispositivo, c->pci.funcao, c->pci.fabricante, c->pci.modelo,
         c->versao >> 8, c->versao & 0xFF, c->barra, c->tam_barra / 1024, c->portas, n2, n3, c->slots_max,
         c->tam_ctx, c->n_scratch, "en marcha");
    return true;
}

int xhci_iniciar_todos(void)
{
    /* Bus de serie, USB, interfaz 0x30: el xHCI. */
    pci_disp_t achados[8];
    int n = pci_procurar_classe(0x0C, 0x03, achados, 8);
    bool algum = false;
    for (int i = 0; i < n; i++) if (achados[i].interface == 0x30) algum = true;
    if (!algum) return 0;
    if (!buffer) {
        uint32_t b = memfisica_alocar_seguidas(BUFFER_BYTES / PAGINA);
        if (!b) { klog("[XHCI] falta memoria para el buffer\n"); return 0; }
        buffer = (uint8_t *)b;
    }
    bool i0 = int_salva_desliga();
    for (int i = 0; i < n && qtd_ctls < XHCI_MAX_CTL; i++) {
        if (achados[i].interface != 0x30) continue;
        xhci_t *c = &ctls[qtd_ctls];
        memset(c, 0, sizeof *c);
        c->pci = achados[i];
        if (iniciar(c)) qtd_ctls++;
        else klog("[XHCI] controlador %02x:%02x.%x (%04x:%04x) sin usar\n", achados[i].barramento,
                  achados[i].dispositivo, achados[i].funcao, achados[i].fabricante, achados[i].modelo);
    }
    int_restaura(i0);
    return qtd_ctls;
}

/* ------------------------------------------------------------ informe */

int xhci_relatorio(char *s, int max)
{
    int k = 0;
    if (!qtd_ctls) return 0;
    k += snformatar(s + k, (size_t)(max - k), "Controladores USB 3.0 (xHCI): %d\n", qtd_ctls);
    for (int i = 0; i < qtd_ctls && k < max; i++) {
        xhci_t *c = &ctls[i];
        static const char *bios[] = { "no lo soltaba (forzado)", "no lo usaba", "lo soltó" };
        k += snformatar(s + k, (size_t)(max - k),
                        "xHCI %d: PCI %02x:%02x.%x, %04x:%04x, versión %x.%02x, %d puertos, %d ranuras, contextos de %u bytes, BIOS %s%s\n",
                        i, c->pci.barramento, c->pci.dispositivo, c->pci.funcao, c->pci.fabricante, c->pci.modelo,
                        c->versao >> 8, c->versao & 0xFF, c->portas, c->slots_max, c->tam_ctx, bios[c->bios + 1],
                        c->morto ? ", PARADO" : "");
        k += snformatar(s + k, (size_t)(max - k), "  espera de transferencias: %s (IRQ %u, %u interrupciones); %u transferencias, %u errores\n",
                        c->irq_usavel ? "durmiendo hasta la IRQ" : c->irq ? "cediendo la CPU (la IRQ no llega)" : "cediendo la CPU (sin IRQ)",
                        c->irq, c->irqs, c->transferencias, c->erros);
        for (int p = 0; p < c->portas && k < max; p++) {
            uint32_t v = le_porta(c, p);
            if (!(v & P_CCS)) continue;
            k += snformatar(s + k, (size_t)(max - k), "  puerto %d (USB %d): dispositivo conectado, %s, %s (PORTSC %x)\n",
                            p + 1, c->protocolo[p] ? c->protocolo[p] : 2, usb_nome_velocidade((int)((v & P_VEL) >> 10)),
                            usb_nome_passo(c->resultado[p]), v);
        }
    }
    return k;
}
