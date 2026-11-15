// SPDX-License-Identifier: GPL-2.0-or-later
/* ohci.c - driver del controlador USB 1.1 OHCI (AMD, NVIDIA, SiS, ALi, Apple...) (D267).
 *
 * Escrito desde la especificación pública "OpenHCI, Open Host Controller
 * Interface Specification for USB, rel. 1.0a" (Compaq, Microsoft, National
 * Semiconductor), sin código de otros sistemas.
 *
 * El OHCI se programa por registros en memoria (BAR 0). El controlador lee de
 * la memoria un área común (HCCA: tabla de 32 listas periódicas y la cola de
 * terminados) y dos listas de "endpoint descriptors" (ED): la de control y la
 * de bulk. Cada ED tiene una cola de "transfer descriptors" (TD) entre HeadP y
 * TailP: está vacía cuando los dos son iguales (el TD de cola es un "ficticio").
 * Diseño, como en el UHCI y el EHCI de este proyecto:
 *   - una ED de trabajo en la lista de control y otra en la de bulk, siempre
 *     enlazadas pero con el bit "saltar" (K) puesto cuando no hacen nada; una
 *     transferencia las rellena, quita K, avisa (CLF/BLF), espera los TD y
 *     vuelve a poner K y a esperar un cuadro;
 *   - la alternancia de datos (DATA0/DATA1) va explícita en cada TD, no en la ED;
 *   - un TD de datos con "redondeo" (R) acepta paquetes cortos y la cola sigue;
 *   - los TD terminan con un código de condición (CC; 0xF = aún no tocado), que
 *     se consulta sin interrupciones, cediendo la CPU cuando se puede; el
 *     tiempo se mide con el TSC (prazo.h);
 *   - los endpoints de interrupción (HID) cuelgan de las 32 entradas de la
 *     tabla periódica, todas a la misma cadena: se visitan en cada cuadro.
 * Traspaso BIOS -> sistema: si HcControl.IR está a 1, un controlador de SMM la
 * tiene: se pide la propiedad con OwnershipChangeRequest y se espera a que
 * suelte IR. Después, reinicio del controlador (HCR) y puesta en marcha. */
#include "usb.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "paginacao.h"
#include "portas.h"
#include "prazo.h"
#include "processo.h"
#include "texto.h"
#include "trava.h"

/* Registros (desplazamiento / 4) */
#define HC_REVISION   0
#define HC_CONTROL    1
#define HC_CMDSTATUS  2
#define HC_INTSTATUS  3
#define HC_INTENABLE  4
#define HC_INTDISABLE 5
#define HC_HCCA       6
#define HC_CTLHEAD    8
#define HC_CTLCUR     9
#define HC_BULKHEAD   10
#define HC_BULKCUR    11
#define HC_FMINTERVAL 13
#define HC_PERIODICSTART 16
#define HC_LSTHRESHOLD 17
#define HC_RHDESCA    18
#define HC_RHDESCB    19
#define HC_RHSTATUS   20
#define HC_RHPORTA    21

#define CTL_PLE   0x04u
#define CTL_CLE   0x10u
#define CTL_BLE   0x20u
#define CTL_HCFS  0xC0u
#define CTL_OPER  0x80u
#define CTL_IR    0x100u
#define CMD_HCR   0x1u
#define CMD_CLF   0x2u
#define CMD_BLF   0x4u
#define CMD_OCR   0x8u
#define INT_WDH   0x2u

/* Estado de puerto (escribir 1 en el bit actúa: ver especificación 7.4.4) */
#define PS_CCS   (1u << 0)
#define PS_PES   (1u << 1)
#define PS_PRS   (1u << 4)
#define PS_PPS   (1u << 8)
#define PS_LSDA  (1u << 9)
#define PS_CSC   (1u << 16)
#define PS_PESC  (1u << 17)
#define PS_PRSC  (1u << 20)

/* ED y TD */
typedef struct { volatile uint32_t flags, tail, head, next; } __attribute__((aligned(16))) oed_t;
typedef struct { volatile uint32_t flags, cbp, next, be; } __attribute__((aligned(16))) otd_t;
_Static_assert(sizeof(oed_t) == 16 && sizeof(otd_t) == 16, "ED y TD del OHCI: 16 bytes");

#define ED_SALTAR (1u << 14)
#define DP_SETUP  0u
#define DP_SAIDA  1u
#define DP_ENTRADA 2u
#define CC_NAO_TOCADO 0xFu
#define CC_STALL      4u

#define MAX_TD 220
#define BUFFER_BYTES 8192
#define MAX_INT 4

/* Página del controlador: HCCA (0), ED "int" (256), ED control (272), ED bulk
 * (288), paquete de setup (304) y TD desde 512. */
static ohci_t ctls[USB_MAX_CONTROLADORES];
static int qtd_ctls;
static uint8_t *buffer;
static trava_t trava_ohci;

static oed_t *ed_int(ohci_t *c)   { return (oed_t *)(c->pagina + 256); }
static oed_t *ed_ctl(ohci_t *c)   { return (oed_t *)(c->pagina + 272); }
static oed_t *ed_bulk(ohci_t *c)  { return (oed_t *)(c->pagina + 288); }
static uint8_t *setup_pacote(ohci_t *c) { return (uint8_t *)(c->pagina + 304); }
static otd_t *tds(ohci_t *c)      { return (otd_t *)(c->pagina + 512); }

int ohci_quantidade(void) { return qtd_ctls; }
ohci_t *ohci_controlador(int i) { return i >= 0 && i < qtd_ctls ? &ctls[i] : NULL; }

static inline uint32_t le(ohci_t *c, int r) { return c->r[r]; }
static inline void esc(ohci_t *c, int r, uint32_t v) { c->r[r] = v; }

static void pausa_ms(uint32_t ms)
{
    prazo_t p = prazo_ms(ms);
    while (!prazo_vencido(&p)) ;
}

/* ------------------------------------------------------------ inicialización */

static bool iniciar(ohci_t *c)
{
    uint32_t bar = pci_le32(&c->pci, 0x10);
    if ((bar & 1) || !(bar & ~0xFu)) return false;       /* tiene que ser de memoria */
    pci_escreve32(&c->pci, 0x04, pci_le32(&c->pci, 0x04) | 0x6);
    c->r = (volatile uint32_t *)janela_mapear(bar & ~0xFu, 4096);
    if (!c->r) return false;
    if ((le(c, HC_REVISION) & 0xFF) != 0x10) { klog("[OHCI] revisión %x no soportada\n", le(c, HC_REVISION) & 0xFF); return false; }
    c->pagina = memfisica_alocar();
    if (!c->pagina) return false;
    memset((void *)c->pagina, 0, PAGINA);

    /* Traspaso de la BIOS (OHCI 5.1.1.3). */
    uint32_t ctl = le(c, HC_CONTROL);
    if (ctl & CTL_IR) {
        esc(c, HC_CMDSTATUS, CMD_OCR);
        for (int i = 0; i < 500 && (le(c, HC_CONTROL) & CTL_IR); i++) pausa_ms(1);
        c->bios = (le(c, HC_CONTROL) & CTL_IR) ? -1 : 1;
        klog("[OHCI] BIOS %s el controlador\n", c->bios < 0 ? "NO soltó" : "soltó");
    } else if (ctl & CTL_HCFS) {
        c->bios = 2;
    }
    esc(c, HC_INTDISABLE, 0xC000007Fu);                  /* sin interrupciones: el driver consulta */
    uint32_t fi = le(c, HC_FMINTERVAL);
    esc(c, HC_CMDSTATUS, CMD_HCR);
    bool reinicio = false;
    for (int i = 0; i < 100 && !reinicio; i++) {
        reinicio = !(le(c, HC_CMDSTATUS) & CMD_HCR);
        if (!reinicio) pausa_ms(1);
    }
    if (!reinicio) { klog("[OHCI] el reinicio no terminó\n"); return false; }

    /* Dos milisegundos para ponerlo en marcha: todo preparado antes. */
    c->proximo_endereco = 1;
    oed_t *ei = ed_int(c), *ec = ed_ctl(c), *eb = ed_bulk(c);
    ei->flags = ED_SALTAR;
    ec->flags = ED_SALTAR;
    eb->flags = ED_SALTAR;
    volatile uint32_t *hcca = (volatile uint32_t *)c->pagina;
    for (int i = 0; i < 32; i++) hcca[i] = (uint32_t)ei;
    uint32_t cuadro = fi & 0x3FFF;
    if (!cuadro) cuadro = 0x2EDF;
    uint32_t novo = cuadro | ((((cuadro - 210) * 6) / 7) << 16) | (~fi & 0x80000000u);   /* FIT alterna */
    esc(c, HC_FMINTERVAL, novo);
    esc(c, HC_PERIODICSTART, cuadro * 9 / 10);
    esc(c, HC_LSTHRESHOLD, 0x628);
    esc(c, HC_HCCA, c->pagina);
    esc(c, HC_CTLHEAD, (uint32_t)ec);
    esc(c, HC_CTLCUR, 0);
    esc(c, HC_BULKHEAD, (uint32_t)eb);
    esc(c, HC_BULKCUR, 0);
    esc(c, HC_INTSTATUS, 0xFFFFFFFFu);
    esc(c, HC_CONTROL, CTL_OPER | CTL_PLE | CTL_CLE | CTL_BLE);
    if ((le(c, HC_CONTROL) & CTL_HCFS) != CTL_OPER) { klog("[OHCI] no pasó a operacional\n"); return false; }

    /* Puertos y energía. */
    uint32_t a = le(c, HC_RHDESCA);
    c->portas = (int)(a & 0xFF);
    if (c->portas > 15) c->portas = 15;
    esc(c, HC_RHDESCA, a & ~((1u << 8) | (1u << 11)));   /* energía de todos a la vez, sin control de sobrecorriente por puerto */
    esc(c, HC_RHSTATUS, 1u << 16);                        /* LPSC: encender la energía global */
    for (int p = 0; p < c->portas; p++) esc(c, HC_RHPORTA + p, PS_PPS);
    uint32_t pot = ((a >> 24) & 0xFF) * 2;
    pausa_ms(pot < 20 ? 20 : pot);
    for (int p = 0; p < USB_MAX_PORTAS; p++) c->resultado[p] = -1;
    pausa_ms(100);                                        /* conexiones estables */
    c->ok = true;
    klog("[OHCI] controlador %02x:%02x.%x (%04x:%04x) en %x: %d puertos, %s\n",
         c->pci.barramento, c->pci.dispositivo, c->pci.funcao, c->pci.fabricante, c->pci.modelo,
         bar & ~0xFu, c->portas, "en marcha");
    return true;
}

int ohci_iniciar_todos(void)
{
    pci_disp_t achados[USB_MAX_CONTROLADORES];
    int n = pci_procurar(0x0C, 0x03, 0x10, achados, USB_MAX_CONTROLADORES);   /* USB, OHCI */
    if (n == 0) return 0;
    if (!buffer) {
        uint32_t b = memfisica_alocar_seguidas(BUFFER_BYTES / PAGINA);
        if (!b) return 0;
        buffer = (uint8_t *)b;
    }
    bool i0 = int_salva_desliga();
    for (int i = 0; i < n && qtd_ctls < USB_MAX_CONTROLADORES; i++) {
        ohci_t *c = &ctls[qtd_ctls];
        memset(c, 0, sizeof *c);
        c->pci = achados[i];
        if (iniciar(c)) qtd_ctls++;
    }
    int_restaura(i0);
    return qtd_ctls;
}

/* ------------------------------------------------------------ puertos */

uint32_t ohci_porta_estado(ohci_t *c, int p) { return le(c, HC_RHPORTA + p); }
bool ohci_porta_conectada(ohci_t *c, int p) { return (ohci_porta_estado(c, p) & PS_CCS) != 0; }

bool ohci_porta_mudou(ohci_t *c, int p)
{
    uint32_t v = le(c, HC_RHPORTA + p);
    if (!(v & PS_CSC)) return false;
    esc(c, HC_RHPORTA + p, PS_CSC);
    return true;
}

int ohci_porta_resetar(ohci_t *c, int p)
{
    int reg = HC_RHPORTA + p;
    uint32_t v = le(c, reg);
    if (!(v & PS_CCS)) return USB_ERRO;
    int vel = (v & PS_LSDA) ? USB_VEL_BAIXA : USB_VEL_PLENA;
    esc(c, reg, PS_PRS);                                  /* el controlador mantiene el reset 10 ms y avisa con PRSC */
    for (int i = 0; i < 100 && !(le(c, reg) & PS_PRSC); i++) usb_pausa_ms(NULL, 2);
    esc(c, reg, PS_PRSC);
    v = le(c, reg);
    if (!(v & PS_CCS)) return USB_ERRO;
    if (!(v & PS_PES)) {
        esc(c, reg, PS_PES);
        usb_pausa_ms(NULL, 2);
        v = le(c, reg);
    }
    if (!(v & PS_PES)) { klog("[OHCI] puerto %d: no se habilitó tras el reset (estado %x)\n", p, v); return USB_ERRO; }
    esc(c, reg, PS_CSC | PS_PESC);
    usb_pausa_ms(NULL, 10);                               /* recuperación tras el reset */
    return vel;
}

/* ------------------------------------------------------------ transferencias */

static void td_preparar(otd_t *t, uint32_t dp, uint32_t alterna, uint32_t tam, uint32_t buf)
{
    t->flags = (1u << 18) | (dp << 19) | (7u << 21) | ((2u | alterna) << 24) | (CC_NAO_TOCADO << 28);
    t->cbp = tam ? buf : 0;
    t->be = tam ? buf + tam - 1 : 0;
    t->next = 0;
}

static uint32_t td_cc(const otd_t *t) { return t->flags >> 28; }

/* Bytes recibidos o enviados por un TD terminado: lo que no quedó por transferir (CBP) se resta. */
static uint32_t td_feito(const otd_t *t, uint32_t tam, uint32_t inicio)
{
    if (!tam) return 0;
    return t->cbp ? t->cbp - inicio : tam;
}

/* Cuelga t[0..n-1] (n TD, más t[n] como TD de cola) en la ED y espera a que
 * terminen. Devuelve 0, USB_ERRO_PAROU, USB_ERRO o USB_ERRO_TEMPO; en
 * *completos, cuántos TD terminaron bien. La ED vuelve a "saltar". */
static int executar(ohci_t *c, oed_t *ed, uint32_t flags, bool bulk, otd_t *t, int n, uint32_t ms, int *completos)
{
    for (int i = 0; i < n; i++) t[i].next = (uint32_t)&t[i + 1];
    memset((void *)&t[n], 0, sizeof t[n]);
    ed->flags = flags | ED_SALTAR;
    ed->tail = (uint32_t)&t[n];
    ed->head = (uint32_t)&t[0];
    ed->flags = flags;
    esc(c, HC_CMDSTATUS, bulk ? CMD_BLF : CMD_CLF);
    prazo_t p = prazo_ms(ms);
    int r = 0;
    *completos = 0;
    for (;;) {
        esc(c, HC_INTSTATUS, INT_WDH);                    /* la cola de terminados no se usa: se confirma sin más */
        int i;
        for (i = 0; i < n; i++) {
            uint32_t cc = td_cc(&t[i]);
            if (cc == CC_NAO_TOCADO) break;
            if (cc != 0) { r = cc == CC_STALL ? USB_ERRO_PAROU : USB_ERRO; break; }
        }
        *completos = i;
        if (r || i == n) break;
        if (prazo_vencido(&p)) { r = USB_ERRO_TEMPO; break; }
        if (ehci_pode_dormir()) ceder();
    }
    ed->flags = flags | ED_SALTAR;
    pausa_ms(2);                                          /* el controlador ya dejó la ED */
    ed->head = ed->tail;                                  /* vacía y sin "detenida" */
    esc(c, HC_INTSTATUS, INT_WDH);
    return r;
}

static uint32_t ed_flags(const usb_disp_t *d, uint32_t ep, uint32_t mps)
{
    return (uint32_t)d->endereco | (ep << 7) | (d->velocidade == USB_VEL_BAIXA ? 1u << 13 : 0) | (mps << 16);
}

int ohci_controle(usb_disp_t *d, const void *setup, void *dados, uint16_t tam, bool entrada)
{
    ohci_t *c = d->octl;
    uint32_t mps0 = d->mps0 ? d->mps0 : 8;
    if (d->velocidade == USB_VEL_BAIXA && mps0 > 8) mps0 = 8;
    if (tam > BUFFER_BYTES) return USB_ERRO;
    trava_pegar(&trava_ohci);
    bool i0 = int_salva_desliga();
    otd_t *t = tds(c);
    memcpy(setup_pacote(c), setup, 8);
    if (!entrada && tam) memcpy(buffer, dados, tam);
    int n = 0;
    td_preparar(&t[n++], DP_SETUP, 0, 8, (uint32_t)setup_pacote(c));
    if (tam) td_preparar(&t[n++], entrada ? DP_ENTRADA : DP_SAIDA, 1, tam, (uint32_t)buffer);
    td_preparar(&t[n++], (tam && entrada) ? DP_SAIDA : DP_ENTRADA, 1, 0, 0);
    int completos;
    int r = executar(c, ed_ctl(c), ed_flags(d, 0, mps0), false, t, n, 1000, &completos);
    int feito = 0;
    if (r == 0 && tam) {
        feito = (int)td_feito(&t[1], tam, (uint32_t)buffer);
        if (entrada) memcpy(dados, buffer, (uint32_t)feito);
    }
    int_restaura(i0);
    trava_soltar(&trava_ohci);
    return r < 0 ? r : feito;
}

/* Un trozo de bulk: un solo TD de hasta 8 KB (dos páginas como mucho, el buffer está alineado). */
int ohci_bulk(usb_disp_t *d, uint8_t ep, uint16_t mps, bool entrada, uint8_t *alterna, void *dados, uint32_t tam)
{
    ohci_t *c = d->octl;
    if (!mps) return USB_ERRO;
    trava_pegar(&trava_ohci);
    bool i0 = int_salva_desliga();
    otd_t *t = tds(c);
    uint32_t total = 0;
    int r = 0;
    bool primeiro = true;
    while (primeiro || total < tam) {
        primeiro = false;
        uint32_t parte = tam - total > BUFFER_BYTES ? BUFFER_BYTES : tam - total;
        if (!entrada && parte) memcpy(buffer, (uint8_t *)dados + total, parte);
        td_preparar(&t[0], entrada ? DP_ENTRADA : DP_SAIDA, *alterna, parte, (uint32_t)buffer);
        int completos;
        r = executar(c, ed_bulk(c), ed_flags(d, ep, mps), true, t, 1, 5000, &completos);
        uint32_t feito = completos ? td_feito(&t[0], parte, (uint32_t)buffer) : 0;
        /* La alternancia cambia con cada paquete enviado (uno de tamaño cero cuenta). */
        uint32_t pacotes = parte ? (feito + mps - 1) / mps : 1;
        if (feito == 0 && completos) pacotes = 1;
        if (r == 0 || r == USB_ERRO_PAROU) *alterna ^= (uint8_t)(pacotes & 1);
        if (r < 0) break;
        if (entrada && feito) memcpy((uint8_t *)dados + total, buffer, feito);
        total += feito;
        if (feito < parte) break;
    }
    int_restaura(i0);
    trava_soltar(&trava_ohci);
    return r < 0 ? r : (int)total;
}

/* ------------------------------------------------------------ interrupción (HID) */

/* Una página por endpoint: ED (0), dos TD (16 y 32) que se alternan como TD de
 * cola y buffer (64). */
typedef struct {
    bool     usado;
    ohci_t  *c;
    uint32_t pagina;
    uint16_t tam;
    uint8_t  alterna, cola, vivo;   /* cola = índice del TD de cola; vivo = el último armado */
} ohci_int_t;

static ohci_int_t ints[MAX_INT];

static otd_t *int_td(const ohci_int_t *e, int i) { return (otd_t *)(e->pagina + 16 + 16u * (uint32_t)i); }

static void int_armar(ohci_int_t *e)
{
    oed_t *ed = (oed_t *)e->pagina;
    otd_t *y = int_td(e, e->cola), *z = int_td(e, e->cola ^ 1);
    ed->head = (uint32_t)y;                               /* vacía y sin "detenida" (si un error la paró) */
    memset((void *)z, 0, sizeof *z);
    td_preparar(y, DP_ENTRADA, e->alterna, e->tam, e->pagina + 64);
    y->next = (uint32_t)z;
    e->vivo = e->cola;
    e->cola ^= 1;
    ed->tail = (uint32_t)z;                               /* ahora el controlador ve un TD */
}

int ohci_int_abrir(usb_disp_t *d, uint8_t ep, uint16_t mps)
{
    ohci_t *c = d->octl;
    if (!c || !mps) return USB_ERRO;
    bool i0 = int_salva_desliga();
    int h = -1;
    for (int i = 0; i < MAX_INT; i++) if (!ints[i].usado) { h = i; break; }
    uint32_t pg = h >= 0 ? memfisica_alocar() : 0;
    if (!pg) { int_restaura(i0); return USB_ERRO; }
    memset((void *)pg, 0, PAGINA);
    ohci_int_t *e = &ints[h];
    e->usado = true;
    e->c = c;
    e->pagina = pg;
    e->tam = mps > 64 ? 64 : mps;
    e->alterna = 0;
    e->cola = 0;
    oed_t *ed = (oed_t *)pg;
    ed->flags = ed_flags(d, ep, mps);
    ed->tail = ed->head = (uint32_t)int_td(e, 0);
    int_armar(e);
    ed->next = ed_int(c)->next;
    ed_int(c)->next = pg;                                 /* detrás de la cabeza ficticia */
    int_restaura(i0);
    return h;
}

int ohci_int_ler(int h, uint8_t *dados, int max)
{
    if (h < 0 || h >= MAX_INT || !ints[h].usado) return USB_ERRO;
    ohci_int_t *e = &ints[h];
    otd_t *t = int_td(e, e->vivo);
    uint32_t cc = td_cc(t);
    esc(e->c, HC_INTSTATUS, INT_WDH);
    if (cc == CC_NAO_TOCADO) return 0;
    bool i0 = int_salva_desliga();
    int r;
    if (cc != 0) {
        r = cc == CC_STALL ? USB_ERRO_PAROU : USB_ERRO;
        e->alterna = 0;
    } else {
        int n = (int)td_feito(t, e->tam, e->pagina + 64);
        if (n > max) n = max;
        memcpy(dados, (const void *)(e->pagina + 64), (uint32_t)n);
        e->alterna ^= 1;
        r = n;
    }
    int_armar(e);
    int_restaura(i0);
    return r;
}

void ohci_int_fechar(int h)
{
    if (h < 0 || h >= MAX_INT || !ints[h].usado) return;
    ohci_int_t *e = &ints[h];
    bool i0 = int_salva_desliga();
    oed_t *ed = (oed_t *)e->pagina;
    ed->flags |= ED_SALTAR;
    oed_t *p = ed_int(e->c);
    while (p->next && p->next != (uint32_t)ed) p = (oed_t *)p->next;
    if (p->next == (uint32_t)ed) p->next = ed->next;
    pausa_ms(3);                                          /* el controlador ya no la está leyendo */
    memfisica_liberar(e->pagina);
    e->usado = false;
    int_restaura(i0);
}
