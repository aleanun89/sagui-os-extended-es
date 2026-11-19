// SPDX-License-Identifier: GPL-2.0-or-later
/* uhci.c - driver del controlador USB 1.1 UHCI (Intel PIIX/ICH, VIA...) (D267).
 *
 * Escrito desde la especificación pública "Universal Host Controller Interface
 * (UHCI) Design Guide, rev. 1.1" de Intel, sin código de otros sistemas.
 *
 * El UHCI se programa por puertos de E/S (BAR 4) y lee de memoria una lista de
 * 1024 cuadros (uno por milisegundo). Cada cuadro apunta a una cadena de
 * cabezas de cola (QH) y de descriptores de transferencia (TD) de 32 bytes.
 * Diseño, en la línea del EHCI de este proyecto (simple y previsible):
 *   - la lista de cuadros: uno de cada 8 apunta a la QH "int" (la cabeza de los
 *     endpoints de interrupción: el teclado y el ratón se consultan cada 8 ms)
 *     y todos acaban en la QH "trab" (la de las transferencias de control y
 *     bulk, una por vez, protegida por un cerrojo);
 *   - los TD de una transferencia van encadenados "en profundidad" (bit Vf):
 *     el controlador los recorre todos en el mismo cuadro y no uno por cuadro;
 *   - paquete corto (SPD): la cola se detiene y el programa decide (en el
 *     control, pasa a la etapa de estado; en el bulk, termina);
 *   - sin interrupciones del controlador: se consulta el bit "activo" de los
 *     TD, cediendo la CPU entre consulta y consulta cuando se puede. El tiempo se
 *     mide con el TSC (prazo.h), que anda con las interrupciones apagadas;
 *   - los datos pasan por un buffer de 8 KB de memoria física contigua.
 * Traspaso BIOS -> sistema: la capacidad USBLEGSUP (registro 0xC0 del espacio
 * PCI) apaga las SMI con las que la BIOS emula un teclado PS/2 a partir del
 * teclado USB; antes se detiene el controlador y se le da un reinicio global. */
#include "usb.h"
#include "isoent.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "paginacao.h"
#include "portas.h"
#include "prazo.h"
#include "processo.h"
#include "texto.h"
#include "trava.h"

/* Registros (desplazamiento desde la base de E/S) */
#define R_CMD    0x00
#define R_STS    0x02
#define R_INTR   0x04
#define R_FRNUM  0x06
#define R_FRBASE 0x08
#define R_SOF    0x0C
#define R_PORTA  0x10

#define CMD_RS      0x0001
#define CMD_HCRESET 0x0002
#define CMD_GRESET  0x0004
#define CMD_CF      0x0040
#define CMD_MAXP    0x0080
#define STS_HALTED  0x0020

/* PORTSC */
#define P_CCS   0x0001
#define P_CSC   0x0002
#define P_PE    0x0004
#define P_PEC   0x0008
#define P_LSDA  0x0100
#define P_PR    0x0200
#define P_SUSP  0x1000

/* TD: palabra de control y estado */
#define TD_ATIVO  (1u << 23)
#define TD_PAROU  (1u << 22)
#define TD_ERROS  ((1u << 22) | (1u << 21) | (1u << 20) | (1u << 18) | (1u << 17))   /* sin el NAK (bit 19) */
#define TD_LS     (1u << 26)
#define TD_SPD    (1u << 29)
#define TD_CERR3  (3u << 27)
#define PID_SETUP 0x2Du
#define PID_ENTRADA 0x69u
#define PID_SAIDA 0xE1u

#define L_TERMINA 1u
#define L_QH      2u
#define L_PROFUNDO 4u

#define MAX_TD 256
#define BUFFER_BYTES 8192
#define MAX_INT 4

typedef struct { volatile uint32_t link, ctrl, token, buf; uint32_t reservado[4]; } __attribute__((aligned(32))) utd_t;
typedef struct { volatile uint32_t link, elem; uint32_t reservado[2]; } __attribute__((aligned(16))) uqh_t;
_Static_assert(sizeof(utd_t) == 32, "TD del UHCI: 32 bytes");
_Static_assert(sizeof(uqh_t) == 16, "QH del UHCI: 16 bytes");

static uhci_t ctls[USB_MAX_CONTROLADORES];
static int qtd_ctls;
static uint8_t *buffer;    /* compartido: las transferencias van de una en una */
static trava_t trava_uhci;

/* Estructuras de la página del controlador: QH int (0), QH trab (16), paquete
 * de setup (64) y, desde 4096, 256 TD. */
static uqh_t *qh_int(uhci_t *c)  { return (uqh_t *)c->pagina; }
static uqh_t *qh_trab(uhci_t *c) { return (uqh_t *)(c->pagina + 16); }
static uint8_t *setup_pacote(uhci_t *c) { return (uint8_t *)(c->pagina + 64); }
static utd_t *tds(uhci_t *c) { return (utd_t *)(c->pagina + 4096); }

int uhci_quantidade(void) { return qtd_ctls; }
uhci_t *uhci_controlador(int i) { return i >= 0 && i < qtd_ctls ? &ctls[i] : NULL; }

static inline uint16_t le16(uhci_t *c, int r) { return porta_le16((uint16_t)(c->io + r)); }
static inline void esc16(uhci_t *c, int r, uint16_t v) { porta_escreve16((uint16_t)(c->io + r), v); }

static void pausa_ms(uint32_t ms)
{
    prazo_t p = prazo_ms(ms);
    while (!prazo_vencido(&p)) ;
}

/* ------------------------------------------------------------ inicialización */

static bool iniciar(uhci_t *c)
{
    uint32_t bar = pci_le32(&c->pci, 0x20);
    if (!(bar & 1)) return false;                        /* tiene que ser de E/S */
    c->io = (uint16_t)(bar & 0xFFE0u);
    if (!c->io) return false;
    /* Comando PCI: espacio de E/S + maestro del bus (DMA). */
    pci_escreve32(&c->pci, 0x04, pci_le32(&c->pci, 0x04) | 0x5);

    /* Traspaso de la BIOS: se detiene el controlador, se borran los avisos y las
     * SMI de USBLEGSUP (16 bits en 0xC0; el PIRQ queda encendido) y se reinicia. */
    uint16_t cmd = le16(c, R_CMD);
    if (cmd & CMD_RS) c->bios = 1;
    esc16(c, R_INTR, 0);
    esc16(c, R_CMD, (uint16_t)(cmd & ~CMD_RS));
    for (int i = 0; i < 100 && !(le16(c, R_STS) & STS_HALTED); i++) pausa_ms(1);
    uint32_t leg = pci_le32(&c->pci, 0xC0);
    pci_escreve32(&c->pci, 0xC0, (leg & 0xFFFF0000u) | 0xAF00u);
    esc16(c, R_CMD, CMD_GRESET);                         /* reinicio global: 50 ms */
    pausa_ms(50);
    esc16(c, R_CMD, 0);
    pausa_ms(10);
    esc16(c, R_CMD, CMD_HCRESET);
    for (int i = 0; i < 100 && (le16(c, R_CMD) & CMD_HCRESET); i++) pausa_ms(1);
    if (le16(c, R_CMD) & CMD_HCRESET) {
        /* D328: el HCRESET debe borrarse solo (UHCI 1.1, 2.1.1), pero el PIIX4 emulado por 86Box lo deja
         * puesto. Se baja a mano y se sigue si el controlador responde (parado y con los puertos en pie). */
        uint16_t cmd_ant = le16(c, R_CMD), sts_ant = le16(c, R_STS);
        esc16(c, R_CMD, 0);
        pausa_ms(10);
        if ((le16(c, R_CMD) & CMD_HCRESET) || le16(c, R_CMD) == 0xFFFF || !(le16(c, R_PORTA) & 0x80)) {
            klog("[UHCI] el reinicio no terminó (CMD %04x, STS %04x, E/S %x)\n", cmd_ant, sts_ant, c->io);
            return false;
        }
        klog("[UHCI] el HCRESET no se borra solo (CMD %04x, STS %04x): se baja a mano\n", cmd_ant, sts_ant);
    }

    c->quadros = memfisica_alocar();
    c->pagina = memfisica_alocar_seguidas(1 + MAX_TD * 32 / PAGINA);
    if (!c->quadros || !c->pagina) return false;
    memset((void *)c->quadros, 0, PAGINA);
    memset((void *)c->pagina, 0, PAGINA * (1 + MAX_TD * 32 / PAGINA));
    uqh_t *qi = qh_int(c), *qt = qh_trab(c);
    qt->link = L_TERMINA;
    qt->elem = L_TERMINA;
    qi->link = (uint32_t)qt | L_QH;
    qi->elem = L_TERMINA;
    volatile uint32_t *lista = (volatile uint32_t *)c->quadros;
    for (int i = 0; i < 1024; i++) lista[i] = (uint32_t)((i & 7) == 0 ? qi : qt) | L_QH;

    esc16(c, R_STS, 0x3F);
    esc16(c, R_FRNUM, 0);
    porta_escreve32((uint16_t)(c->io + R_FRBASE), c->quadros);
    porta_escreve8((uint16_t)(c->io + R_SOF), 0x40);
    esc16(c, R_CMD, CMD_RS | CMD_CF | CMD_MAXP);
    for (int i = 0; i < 100 && (le16(c, R_STS) & STS_HALTED); i++) pausa_ms(1);

    /* Puertos: cada registro de estado tiene el bit 7 siempre a 1. */
    c->portas = 0;
    for (int p = 0; p < 8; p++) {
        uint16_t v = le16(c, R_PORTA + 2 * p);
        if (!(v & 0x80) || v == 0xFFFF) break;
        c->portas++;
    }
    if (c->portas < 2) c->portas = 2;
    for (int p = 0; p < USB_MAX_PORTAS; p++) c->resultado[p] = -1;
    c->proximo_endereco = 1;
    pausa_ms(100);                                       /* conexiones estables */
    c->ok = !(le16(c, R_STS) & STS_HALTED);
    klog("[UHCI] controlador %02x:%02x.%x (%04x:%04x) en E/S %x: %d puertos, BIOS %s, %s\n",
         c->pci.barramento, c->pci.dispositivo, c->pci.funcao, c->pci.fabricante, c->pci.modelo,
         c->io, c->portas, c->bios ? "lo usaba" : "no lo usaba", c->ok ? "en marcha" : "PARADO");
    return c->ok;
}

int uhci_iniciar_todos(void)
{
    pci_disp_t achados[USB_MAX_CONTROLADORES];
    int n = pci_procurar(0x0C, 0x03, 0x00, achados, USB_MAX_CONTROLADORES);   /* USB, UHCI */
    if (n == 0) return 0;
    if (!buffer) {
        uint32_t b = memfisica_alocar_seguidas(BUFFER_BYTES / PAGINA);
        if (!b) return 0;
        buffer = (uint8_t *)b;
    }
    bool i0 = int_salva_desliga();
    for (int i = 0; i < n && qtd_ctls < USB_MAX_CONTROLADORES; i++) {
        uhci_t *c = &ctls[qtd_ctls];
        memset(c, 0, sizeof *c);
        c->pci = achados[i];
        if (iniciar(c)) qtd_ctls++;
    }
    int_restaura(i0);
    return qtd_ctls;
}

/* ------------------------------------------------------------ puertos */

uint32_t uhci_porta_estado(uhci_t *c, int p) { return le16(c, R_PORTA + 2 * p); }
bool uhci_porta_conectada(uhci_t *c, int p) { return (uhci_porta_estado(c, p) & P_CCS) != 0; }

bool uhci_porta_mudou(uhci_t *c, int p)
{
    uint16_t v = le16(c, R_PORTA + 2 * p);
    if (!(v & P_CSC)) return false;
    esc16(c, R_PORTA + 2 * p, (uint16_t)((v & (P_PE | P_SUSP)) | P_CSC));   /* se borra escribiendo 1 */
    return true;
}

/* Reinicia el puerto (USB 2.0, 7.1.7.5: 50 ms en la raíz) y lo habilita; devuelve
 * la velocidad del dispositivo, leída DESPUÉS del reset (D389: como hace el hub de Linux; antes
 * del reset el bit LSDA puede ser el de una conexión anterior). */
int uhci_porta_resetar(uhci_t *c, int p)
{
    int reg = R_PORTA + 2 * p;
    uint16_t v = le16(c, reg);
    c->porta_antes = v;
    c->porta_depois = 0;
    if (!(v & P_CCS)) return USB_ERRO;
    int vel = (v & P_LSDA) ? USB_VEL_BAIXA : USB_VEL_PLENA;
    esc16(c, reg, P_PR);
    usb_pausa_ms(NULL, 50);
    esc16(c, reg, 0);
    usb_pausa_ms(NULL, 10);
    for (int i = 0; i < 50; i++) {                       /* habilitar: PE a 1 y borrar los cambios */
        esc16(c, reg, P_PE | P_CSC | P_PEC);
        usb_pausa_ms(NULL, 2);
        v = le16(c, reg);
        if (!(v & P_CCS)) return USB_ERRO;
        if (v & P_PE) break;
    }
    c->porta_depois = v;
    if (!(v & P_PE)) { klog("[UHCI] puerto %d: no se habilitó tras el reset (estado %x)\n", p, v); return USB_ERRO; }
    vel = (v & P_LSDA) ? USB_VEL_BAIXA : USB_VEL_PLENA;
    esc16(c, reg, (uint16_t)(P_PE | P_CSC | P_PEC));
    usb_pausa_ms(NULL, 10);                              /* recuperación tras el reset (TRSTRCY, USB 2.0 7.1.7.5) */
    return vel;
}

/* Los bits de error de la palabra de estado de un TD (UHCI 1.1, 3.2.2), para el registro. */
int uhci_erro_texto(uint32_t f, char *s, int max)
{
    return snformatar(s, (size_t)max, "%08x%s%s%s%s%s%s", f, (f & (1u << 22)) ? " stall" : "", (f & (1u << 21)) ? " buffer" : "",
                      (f & (1u << 20)) ? " babble" : "", (f & (1u << 19)) ? " nak" : "", (f & (1u << 18)) ? " crc/tiempo" : "",
                      (f & (1u << 17)) ? " bitstuff" : "");
}

/* ------------------------------------------------------------ transferencias */

static void td_preparar(utd_t *t, uint32_t pid, uint32_t endereco, uint32_t ep, uint32_t alterna,
                        uint32_t tam, uint32_t buf, bool lenta, bool curto)
{
    t->link = L_TERMINA;
    t->ctrl = TD_ATIVO | TD_CERR3 | (lenta ? TD_LS : 0) | (curto ? TD_SPD : 0);
    t->token = pid | (endereco << 8) | (ep << 15) | (alterna << 19) | ((tam ? tam - 1 : 0x7FFu) << 21);
    t->buf = tam ? buf : 0;
}

static void encadear(utd_t *t, int n)
{
    for (int i = 0; i < n - 1; i++) t[i].link = (uint32_t)&t[i + 1] | L_PROFUNDO;
    t[n - 1].link = L_TERMINA;
}

static uint32_t td_pedido(const utd_t *t) { return ((t->token >> 21) + 1) & 0x7FF; }
static uint32_t td_feito(const utd_t *t) { return ((t->ctrl & 0x7FF) + 1) & 0x7FF; }

/* Arranca la cola en t[ini] y espera a que t[ini..fim] terminen. 0 = bien;
 * *curto = índice del TD que recibió un paquete corto (la cola se detiene ahí)
 * o -1; si no, USB_ERRO_PAROU / USB_ERRO / USB_ERRO_TEMPO (y la cola se vacía). */
static int rodar(uhci_t *c, utd_t *t, int ini, int fim, uint32_t ms, int *curto)
{
    uqh_t *q = qh_trab(c);
    *curto = -1;
    q->elem = (uint32_t)&t[ini];
    prazo_t p = prazo_ms(ms);
    int r = 0;
    for (;;) {
        bool tudo = true;
        for (int i = ini; i <= fim; i++) {
            uint32_t f = t[i].ctrl;
            if (f & TD_ATIVO) { tudo = false; break; }
            if (f & TD_ERROS) { r = ((f & TD_ERROS) == TD_PAROU) ? USB_ERRO_PAROU : USB_ERRO; c->ultimo_td = f; break; }
            if ((f & TD_SPD) && td_feito(&t[i]) < td_pedido(&t[i])) { *curto = i; break; }
        }
        if (r || *curto >= 0 || tudo) break;
        if (prazo_vencido(&p)) {
            r = USB_ERRO_TEMPO;
            for (int i = ini; i <= fim; i++) if (t[i].ctrl & TD_ATIVO) { c->ultimo_td = t[i].ctrl; break; }   /* NAK sin fin */
            break;
        }
        if (ehci_pode_dormir()) ceder();
    }
    if (r || *curto >= 0) {
        q->elem = L_TERMINA;                             /* parar la cola y esperar a que el controlador la suelte */
        pausa_ms(2);                                     /* los TD que no llegaron a correr se rehacen en la siguiente transferencia */
    }
    return r;
}

static bool lenta(const usb_disp_t *d) { return d->velocidade == USB_VEL_BAIXA; }

int uhci_controle(usb_disp_t *d, const void *setup, void *dados, uint16_t tam, bool entrada)
{
    uhci_t *c = d->uctl;
    uint32_t mps0 = d->mps0 ? d->mps0 : 8;
    if (lenta(d) && mps0 > 8) mps0 = 8;
    uint32_t ntd = 2 + (tam + mps0 - 1) / mps0;
    if (tam > BUFFER_BYTES || ntd > MAX_TD) return USB_ERRO;
    trava_pegar(&trava_uhci);
    bool i0 = int_salva_desliga();
    utd_t *t = tds(c);
    memcpy(setup_pacote(c), setup, 8);
    if (!entrada && tam) memcpy(buffer, dados, tam);
    int n = 0;
    bool ls = lenta(d);
    td_preparar(&t[n++], PID_SETUP, d->endereco, 0, 0, 8, (uint32_t)setup_pacote(c), ls, false);
    uint32_t alterna = 1;
    for (uint32_t feito = 0; feito < tam;) {
        uint32_t parte = tam - feito > mps0 ? mps0 : tam - feito;
        td_preparar(&t[n++], entrada ? PID_ENTRADA : PID_SAIDA, d->endereco, 0, alterna, parte,
                    (uint32_t)buffer + feito, ls, entrada);
        alterna ^= 1;
        feito += parte;
    }
    int estado = n;
    /* Estado: siempre DATA1, en dirección contraria a los datos (o IN si no hay datos). */
    td_preparar(&t[n++], (tam && entrada) ? PID_SAIDA : PID_ENTRADA, d->endereco, 0, 1, 0, 0, ls, false);
    encadear(t, n);
    int curto;
    int r = rodar(c, t, 0, n - 1, 1000, &curto);
    if (r == 0 && curto >= 0) {                          /* dato más corto de lo pedido: va al estado */
        td_preparar(&t[estado], (tam && entrada) ? PID_SAIDA : PID_ENTRADA, d->endereco, 0, 1, 0, 0, ls, false);
        t[estado].link = L_TERMINA;
        r = rodar(c, t, estado, estado, 1000, &curto);
    }
    int feito = 0;
    if (r == 0 && tam) {
        for (int i = 1; i < estado; i++) {
            feito += (int)td_feito(&t[i]);
            if (td_feito(&t[i]) < td_pedido(&t[i])) break;
        }
        if (entrada) memcpy(dados, buffer, (uint32_t)feito);
    }
    int_restaura(i0);
    trava_soltar(&trava_uhci);
    return r < 0 ? r : feito;
}

/* Un trozo de transferencia bulk (<= buffer, <= MAX_TD paquetes). Devuelve bytes o error. */
static int bulk_parte(usb_disp_t *d, uint8_t ep, uint32_t mps, bool entrada, uint8_t *alterna, uint32_t tam, bool *fim)
{
    uhci_t *c = d->uctl;
    utd_t *t = tds(c);
    bool ls = lenta(d);
    int n = 0;
    uint32_t tog = *alterna, feito = 0;
    do {
        uint32_t parte = tam - feito > mps ? mps : tam - feito;
        td_preparar(&t[n++], entrada ? PID_ENTRADA : PID_SAIDA, d->endereco, ep, tog, parte,
                    (uint32_t)buffer + feito, ls, entrada);
        tog ^= 1;
        feito += parte;
    } while (feito < tam);
    encadear(t, n);
    int curto;
    int r = rodar(c, t, 0, n - 1, 5000, &curto);
    int completos = 0;
    uint32_t bytes = 0;
    for (int i = 0; i < n; i++) {
        uint32_t f = t[i].ctrl;
        if ((f & TD_ATIVO) || (f & TD_ERROS)) break;
        bytes += td_feito(&t[i]);
        completos++;
        if (entrada && td_feito(&t[i]) < td_pedido(&t[i])) break;
    }
    *fim = r == 0 && (completos < n);
    if (r == 0 || r == USB_ERRO_PAROU) *alterna ^= (uint8_t)(completos & 1);
    return r < 0 ? r : (int)bytes;
}

int uhci_bulk(usb_disp_t *d, uint8_t ep, uint16_t mps, bool entrada, uint8_t *alterna, void *dados, uint32_t tam)
{
    if (!mps) return USB_ERRO;
    uint32_t max = (uint32_t)(MAX_TD - 2) * mps;
    if (max > BUFFER_BYTES) max = BUFFER_BYTES;
    max -= max % mps;
    trava_pegar(&trava_uhci);
    bool i0 = int_salva_desliga();
    uint32_t total = 0;
    int r = 0;
    bool primeiro = true;
    while (primeiro || total < tam) {
        primeiro = false;
        uint32_t parte = tam - total > max ? max : tam - total;
        if (!entrada && parte) memcpy(buffer, (uint8_t *)dados + total, parte);
        bool curto = false;
        r = bulk_parte(d, ep, mps, entrada, alterna, parte, &curto);
        if (r < 0) break;
        if (entrada && r) memcpy((uint8_t *)dados + total, buffer, (uint32_t)r);
        total += (uint32_t)r;
        if ((uint32_t)r < parte || curto) break;
    }
    int_restaura(i0);
    trava_soltar(&trava_uhci);
    return r < 0 ? r : (int)total;
}

/* ------------------------------------------------------------ interrupción (HID) */

/* Cada endpoint abierto tiene una página: QH (0), TD (32) y buffer (64). La QH
 * cuelga detrás de la cabeza "int" y se visita cada 8 ms; el TD queda inactivo
 * cuando el dispositivo manda su informe, y uhci_int_ler lo lee y lo arma de nuevo. */
typedef struct {
    bool     usado;
    uhci_t  *c;
    uint32_t pagina;
    uint16_t tam;
    uint8_t  alterna, endereco, ep, lenta;
} uhci_int_t;

static uhci_int_t ints[MAX_INT];

static void int_armar(uhci_int_t *e)
{
    uqh_t *q = (uqh_t *)e->pagina;
    utd_t *t = (utd_t *)(e->pagina + 32);
    td_preparar(t, PID_ENTRADA, e->endereco, e->ep, e->alterna, e->tam, e->pagina + 64, e->lenta, false);
    q->elem = (uint32_t)t;
}

int uhci_int_abrir(usb_disp_t *d, uint8_t ep, uint16_t mps)
{
    uhci_t *c = d->uctl;
    if (!c || !mps) return USB_ERRO;
    bool i0 = int_salva_desliga();
    int h = -1;
    for (int i = 0; i < MAX_INT; i++) if (!ints[i].usado) { h = i; break; }
    uint32_t pg = h >= 0 ? memfisica_alocar() : 0;
    if (!pg) { int_restaura(i0); return USB_ERRO; }
    memset((void *)pg, 0, PAGINA);
    uhci_int_t *e = &ints[h];
    e->usado = true;
    e->c = c;
    e->pagina = pg;
    e->tam = mps > 64 ? 64 : mps;
    e->alterna = 0;
    e->endereco = d->endereco;
    e->ep = ep;
    e->lenta = lenta(d);
    uqh_t *q = (uqh_t *)pg;
    int_armar(e);
    q->link = qh_int(c)->link;
    qh_int(c)->link = pg | L_QH;                         /* detrás de la cabeza: el controlador lo ve entero o nada */
    int_restaura(i0);
    return h;
}

int uhci_int_ler(int h, uint8_t *dados, int max)
{
    if (h < 0 || h >= MAX_INT || !ints[h].usado) return USB_ERRO;
    uhci_int_t *e = &ints[h];
    utd_t *t = (utd_t *)(e->pagina + 32);
    uint32_t f = t->ctrl;
    if (f & TD_ATIVO) return 0;
    bool i0 = int_salva_desliga();
    int r;
    if (f & TD_ERROS) {
        r = ((f & TD_ERROS) == TD_PAROU) ? USB_ERRO_PAROU : USB_ERRO;
        e->alterna = 0;
    } else {
        int n = (int)td_feito(t);
        if (n > max) n = max;
        memcpy(dados, (const void *)(e->pagina + 64), (uint32_t)n);
        e->alterna ^= 1;
        r = n;
    }
    int_armar(e);
    int_restaura(i0);
    return r;
}

void uhci_int_fechar(int h)
{
    if (h < 0 || h >= MAX_INT || !ints[h].usado) return;
    uhci_int_t *e = &ints[h];
    bool i0 = int_salva_desliga();
    uqh_t *q = (uqh_t *)e->pagina;
    uqh_t *p = qh_int(e->c);
    while ((p->link & ~0xFu) != (uint32_t)q && (p->link & L_QH) && !(p->link & L_TERMINA)) p = (uqh_t *)(p->link & ~0xFu);
    if ((p->link & ~0xFu) == (uint32_t)q) p->link = q->link;
    pausa_ms(3);                                         /* el controlador ya no la está leyendo */
    memfisica_liberar(e->pagina);
    e->usado = false;
    int_restaura(i0);
}

/* ------------------------------------------------------------ recepción bulk continua (D389) */

/* Para los datos ACL de un controlador Bluetooth por UHCI (antes no había: «solo se puede buscar»).
 * Una página por flujo: QH (0), dos juegos de RX_TDS TD (64 y 64 + 32*RX_TDS) y sus buffers de un
 * paquete (desde 2048). La QH cuelga, como las de interrupción, detrás de la cabeza «int» (cada 8 ms);
 * los TD van encadenados en profundidad, así que en una visita entran varios paquetes seguidos hasta
 * el primer NAK (16 x 64 bytes cada 8 ms: unos 128 KB/s, de sobra para HID por Bluetooth).
 * Cada TD lleva SPD: un paquete corto detiene la cola sobre ese TD (que queda inactivo). Al leer, se
 * recogen en orden los TD acabados y, cuando el juego se acaba (paquete corto, error o los RX_TDS
 * hechos), se arma el OTRO juego y se apunta la QH a él de una sola escritura: el controlador nunca ve
 * un TD a medio escribir. Lo que se devuelve es un flujo de bytes: quien lo usa reconstruye sus
 * paquetes (el Bluetooth, por la longitud de la cabecera ACL). */
#define RX_TDS   16
#define MAX_RX   2

typedef struct {
    bool     usado;
    uhci_t  *c;
    uint32_t pagina;
    uint16_t mps;
    uint8_t  alterna, endereco, ep, lenta;
    int      conj, lidos;
    uint32_t paquetes, erros;
} uhci_rx_t;

static uhci_rx_t rxs[MAX_RX];

static utd_t *rx_td(uhci_rx_t *e, int conj) { return (utd_t *)(e->pagina + 64 + (uint32_t)conj * 32u * RX_TDS); }
static uint32_t rx_buf(uhci_rx_t *e, int conj, int i) { return e->pagina + 2048 + ((uint32_t)conj * RX_TDS + (uint32_t)i) * 64u; }

static void rx_armar(uhci_rx_t *e, int conj)
{
    utd_t *t = rx_td(e, conj);
    uint32_t tog = e->alterna;
    for (int i = 0; i < RX_TDS; i++) {
        td_preparar(&t[i], PID_ENTRADA, e->endereco, e->ep, tog, e->mps, rx_buf(e, conj, i), e->lenta, true);
        tog ^= 1;
    }
    encadear(t, RX_TDS);
    e->conj = conj;
    e->lidos = 0;
    ((uqh_t *)e->pagina)->elem = (uint32_t)t;   /* una escritura: el juego viejo ya no se visita */
}

int uhci_rx_abrir(usb_disp_t *d, uint8_t ep, uint16_t mps)
{
    uhci_t *c = d->uctl;
    if (!c || !mps || mps > 64) return USB_ERRO;
    bool i0 = int_salva_desliga();
    int h = -1;
    for (int i = 0; i < MAX_RX; i++) if (!rxs[i].usado) { h = i; break; }
    uint32_t pg = h >= 0 ? memfisica_alocar() : 0;
    if (!pg) { int_restaura(i0); return USB_ERRO; }
    memset((void *)pg, 0, PAGINA);
    uhci_rx_t *e = &rxs[h];
    memset(e, 0, sizeof *e);
    e->usado = true;
    e->c = c;
    e->pagina = pg;
    e->mps = mps;
    e->alterna = d->alterna_entrada;
    e->endereco = d->endereco;
    e->ep = ep;
    e->lenta = lenta(d);
    uqh_t *q = (uqh_t *)pg;
    rx_armar(e, 0);
    q->link = qh_int(c)->link;
    qh_int(c)->link = pg | L_QH;
    int_restaura(i0);
    return h;
}

/* Lo recibido desde la última lectura (hasta max bytes; nunca parte un paquete), 0 si nada o <0. */
int uhci_rx_ler(int h, uint8_t *dados, int max)
{
    if (h < 0 || h >= MAX_RX || !rxs[h].usado) return USB_ERRO;
    uhci_rx_t *e = &rxs[h];
    bool i0 = int_salva_desliga();
    utd_t *t = rx_td(e, e->conj);
    int n = 0, r = 0;
    bool reamar = false;
    while (e->lidos < RX_TDS) {
        int i = e->lidos;
        uint32_t f = t[i].ctrl;
        if (f & TD_ATIVO) break;
        if (f & TD_ERROS) {
            e->erros++;
            r = ((f & TD_ERROS) == TD_PAROU) ? USB_ERRO_PAROU : USB_ERRO;
            reamar = true;   /* el TD con error no cambió la alternancia */
            break;
        }
        int k = (int)td_feito(&t[i]);
        if (k > e->mps) k = e->mps;
        if (n + k > max) break;   /* no cabe: se queda para la próxima lectura */
        memcpy(dados + n, (const void *)rx_buf(e, e->conj, i), (uint32_t)k);
        n += k;
        e->paquetes++;
        e->alterna ^= 1;
        e->lidos++;
        if ((uint32_t)k < td_pedido(&t[i])) { reamar = true; break; }   /* corto: la cola se detuvo aquí */
    }
    if (e->lidos >= RX_TDS) reamar = true;
    if (reamar) rx_armar(e, e->conj ^ 1);
    int_restaura(i0);
    return n ? n : r;
}

void uhci_rx_fechar(int h)
{
    if (h < 0 || h >= MAX_RX || !rxs[h].usado) return;
    uhci_rx_t *e = &rxs[h];
    bool i0 = int_salva_desliga();
    uqh_t *q = (uqh_t *)e->pagina;
    uqh_t *p = qh_int(e->c);
    while ((p->link & ~0xFu) != (uint32_t)q && (p->link & L_QH) && !(p->link & L_TERMINA)) p = (uqh_t *)(p->link & ~0xFu);
    if ((p->link & ~0xFu) == (uint32_t)q) p->link = q->link;
    pausa_ms(3);
    memfisica_liberar(e->pagina);
    e->usado = false;
    int_restaura(i0);
}

bool uhci_rx_info(int h, uint32_t *paquetes, uint32_t *erros)
{
    if (h < 0 || h >= MAX_RX || !rxs[h].usado) return false;
    *paquetes = rxs[h].paquetes;
    *erros = rxs[h].erros;
    return true;
}

/* ------------------------------------------------------------ transferencias isócronas (D275) */

/* Un flujo isócrono es un anillo de 64 TD (uno por milisegundo) que cuelgan de la lista de
 * cuadros: la entrada f de la lista (f mod 64 = k) apunta al TD k y este, a lo que había
 * (la QH int o la de trabajo). El TD lleva IOS (isócrono) y un paquete de salida de hasta
 * 1023 bytes. Como en el EHCI, el tiempo es el contador de cuadros del controlador
 * (FRNUM): se arma el TD del cuadro f unos 32 ms antes; un TD cuyo cuadro ya pasó está
 * completo (y se desactiva para que no se repita 64 ms más tarde). Un TD isócrono no
 * se reintenta ni tiene alternancia de datos. */
#define UISO_N       64
#define TD_IOS       (1u << 25)
#define UISO_ANCHO_MAX 1280u     /* bytes isócronos por cuadro (por debajo del 90 % de 1500) */

typedef struct {
    bool     usado, ligado;
    uhci_t  *c;
    uint32_t tds;                  /* físico de los 64 TD */
    uint8_t  dev, ep;
    uint16_t mps;
    uint32_t anel_fis, anel_tam, bytes_s;
    uint8_t  tam_quadro;
    uint32_t pos_fila, pos_feito, acc;
    uint32_t agora, ultimo_f, prox, vel;
    uint16_t bytes[UISO_N];
    bool     entrada;              /* D277: flujo de entrada (cámara) */
    uint8_t  arm[UISO_N];          /* entrada: el TD lleva un paquete puesto */
    isoent_t re;                   /* entrada: casillas y paquetes recibidos */
    uint32_t paquetes, completados, subdesb, erros;
} uhci_iso_t;

static uhci_iso_t isoc[USB_ISO_MAX * USB_MAX_CONTROLADORES];
static uint32_t uhci_ancho_iso[USB_MAX_CONTROLADORES];

static utd_t *utd_iso(uhci_iso_t *e, uint32_t k) { return (utd_t *)(e->tds + 32u * k); }

static void uiso_relogio(uhci_iso_t *e)
{
    uint32_t f = le16(e->c, R_FRNUM) & 0x7FF;
    e->agora += (f - e->ultimo_f) & 0x7FF;
    e->ultimo_f = f;
}

static void uiso_completar(uhci_iso_t *e)
{
    while (e->vel != e->prox && (int32_t)(e->agora - e->vel) > 0) {
        uint32_t k = e->vel % UISO_N;
        utd_t *t = utd_iso(e, k);
        if (e->entrada) {
            if (e->arm[k]) {
                uint32_t c = t->ctrl;
                bool mal = (c & TD_ATIVO) || (c & TD_ERROS);
                if (c & TD_ATIVO) e->subdesb++;
                else if (c & TD_ERROS) e->erros++;
                isoent_completar(&e->re, mal ? 0 : ((c + 1) & 0x7FFu), mal);   /* ActLen codificado como n-1 */
                e->arm[k] = 0;
            }
            t->ctrl = TD_IOS;
            e->completados++;
            e->vel++;
            continue;
        }
        if (t->ctrl & TD_ATIVO) e->subdesb++;                 /* el controlador no llegó a ejecutarlo */
        else if (t->ctrl & TD_ERROS) e->erros++;
        t->ctrl = TD_IOS;                                     /* inactivo */
        e->pos_feito += e->bytes[k];
        e->bytes[k] = 0;
        e->completados++;
        e->vel++;
    }
}

static void uiso_armar(uhci_iso_t *e, uint32_t f)
{
    uint32_t k = f % UISO_N;
    utd_t *t = utd_iso(e, k);
    if (e->entrada) {
        if (!isoent_livres(&e->re)) { t->ctrl = TD_IOS; e->paquetes++; return; }   /* el consumidor va atrasado */
        uint32_t off = isoent_postar(&e->re);
        t->token = ((uint32_t)(e->mps - 1) << 21) | ((uint32_t)e->ep << 15) | ((uint32_t)e->dev << 8) | PID_ENTRADA;
        t->buf = e->anel_fis + off;
        __asm__ volatile("" ::: "memory");
        t->ctrl = TD_ATIVO | TD_IOS;
        e->arm[k] = 1;
        e->paquetes++;
        return;
    }
    uint32_t n = usb_iso_calcular(&e->acc, e->bytes_s, e->tam_quadro, 1000, e->mps);
    uint32_t lim = e->anel_tam - e->pos_fila;
    if (n > lim) n = lim - lim % e->tam_quadro;
    t->token = ((n ? n - 1 : 0x7FFu) << 21) | ((uint32_t)e->ep << 15) | ((uint32_t)e->dev << 8) | PID_SAIDA;
    t->buf = e->anel_fis + e->pos_fila;
    __asm__ volatile("" ::: "memory");
    t->ctrl = TD_ATIVO | TD_IOS;
    e->bytes[k] = (uint16_t)n;
    e->pos_fila += n;
    if (e->pos_fila >= e->anel_tam) e->pos_fila -= e->anel_tam;
    e->paquetes++;
}

static void uiso_encher(uhci_iso_t *e)
{
    uiso_relogio(e);
    uiso_completar(e);
    if (!e->ligado) return;
    if ((int32_t)(e->prox - e->agora) < 2) {
        if (e->prox != e->vel) e->subdesb++;                  /* nos quedamos atrás: se salta al futuro */
        while (e->vel != e->prox) {
            uint32_t k = e->vel % UISO_N;
            utd_iso(e, k)->ctrl = TD_IOS;
            if (e->entrada && e->arm[k]) { isoent_completar(&e->re, 0, true); e->arm[k] = 0; }
            e->pos_feito += e->bytes[k];
            e->bytes[k] = 0;
            e->vel++;
        }
        e->prox = e->vel = e->agora + 3;
    }
    while ((int32_t)(e->prox - e->agora) < (int32_t)USB_ISO_COLA_MS && e->prox - e->vel < UISO_N - 4) {
        uiso_armar(e, e->prox);
        e->prox++;
    }
}

int uhci_iso_abrir(usb_disp_t *d, const usb_iso_cfg_t *cfg)
{
    uhci_t *c = d->uctl;
    if (!c || !cfg->mps || cfg->mps > 1023) return USB_ERRO;
    if (cfg->entrada ? (cfg->anel_tam % cfg->mps != 0 || cfg->anel_tam / cfg->mps > ISOENT_CASILLAS_MAX)
                     : (!cfg->tam_quadro || cfg->anel_tam % cfg->tam_quadro)) return USB_ERRO;
    if (d->velocidade == USB_VEL_BAIXA) return USB_ERRO;   /* la baja velocidad no tiene isócrono */
    int ci = (int)(c - ctls);
    bool i0 = int_salva_desliga();
    int h = -1, r = USB_ERRO;
    for (int i = 0; i < USB_ISO_MAX * USB_MAX_CONTROLADORES; i++) if (!isoc[i].usado) { h = i; break; }
    if (h >= 0 && uhci_ancho_iso[ci] + cfg->mps <= UISO_ANCHO_MAX) {
        uint32_t pg = memfisica_alocar();
        if (pg) {
            uhci_iso_t *e = &isoc[h];
            memset(e, 0, sizeof *e);
            memset((void *)pg, 0, PAGINA);
            e->usado = true;
            e->c = c;
            e->tds = pg;
            e->dev = d->endereco;
            e->ep = cfg->ep;
            e->mps = cfg->mps;
            e->anel_fis = cfg->anel_fis;
            e->anel_tam = cfg->anel_tam;
            e->bytes_s = cfg->bytes_s;
            e->tam_quadro = cfg->tam_quadro;
            e->entrada = cfg->entrada;
            if (e->entrada && !isoent_iniciar(&e->re, cfg->anel_tam, cfg->mps)) { memfisica_liberar(pg); e->usado = false; int_restaura(i0); return USB_ERRO; }
            e->ultimo_f = le16(c, R_FRNUM) & 0x7FF;
            e->agora = e->prox = e->vel = e->ultimo_f;
            uhci_ancho_iso[ci] += cfg->mps;
            volatile uint32_t *lista = (volatile uint32_t *)c->quadros;
            for (uint32_t k = 0; k < UISO_N; k++) {
                utd_t *t = utd_iso(e, k);
                t->link = (uint32_t)((k & 7) == 0 ? qh_int(c) : qh_trab(c)) | L_QH;
                t->ctrl = TD_IOS;
            }
            for (uint32_t f = 0; f < 1024; f++) lista[f] = (uint32_t)utd_iso(e, f % UISO_N);   /* TD: sin el bit de QH */
            r = h;
        }
    } else if (h >= 0)
        klog("[UHCI] isocrono: sin ancho de banda (%u + %u > %u bytes por cuadro)\n", uhci_ancho_iso[ci], cfg->mps, UISO_ANCHO_MAX);
    int_restaura(i0);
    return r;
}

static uhci_iso_t *uiso_h(usb_disp_t *d, int h)
{
    if (h < 0 || h >= USB_ISO_MAX * USB_MAX_CONTROLADORES || !isoc[h].usado || isoc[h].c != d->uctl) return NULL;
    return &isoc[h];
}

void uhci_iso_ligar(usb_disp_t *d, int h)
{
    bool i0 = int_salva_desliga();
    uhci_iso_t *e = uiso_h(d, h);
    if (e) {
        uiso_relogio(e);
        uiso_completar(e);
        while (e->vel != e->prox) { e->bytes[e->vel % UISO_N] = 0; e->vel++; }
        for (uint32_t k = 0; k < UISO_N; k++) { utd_iso(e, k)->ctrl = TD_IOS; e->arm[k] = 0; }
        if (e->entrada) isoent_reiniciar(&e->re);
        e->pos_fila = e->pos_feito = e->acc = 0;
        e->prox = e->vel = e->agora + 3;
        e->ligado = true;
        uiso_encher(e);
    }
    int_restaura(i0);
}

void uhci_iso_parar(usb_disp_t *d, int h)
{
    bool i0 = int_salva_desliga();
    uhci_iso_t *e = uiso_h(d, h);
    if (e) e->ligado = false;
    int_restaura(i0);
}

uint32_t uhci_iso_posicao(usb_disp_t *d, int h)
{
    bool i0 = int_salva_desliga();
    uhci_iso_t *e = uiso_h(d, h);
    uint32_t r = 0;
    if (e) { uiso_encher(e); r = e->pos_feito; }
    int_restaura(i0);
    return r;
}

bool uhci_iso_ler(usb_disp_t *d, int h, uint32_t *off, uint32_t *len, bool *erro)
{
    bool i0 = int_salva_desliga();
    uhci_iso_t *e = uiso_h(d, h);
    bool r = e && e->entrada && isoent_ler(&e->re, off, len, erro);
    int_restaura(i0);
    return r;
}

bool uhci_iso_info(usb_disp_t *d, int h, usb_iso_info_t *i)
{
    uhci_iso_t *e = uiso_h(d, h);
    if (!e) return false;
    i->paquetes = e->paquetes;
    i->completados = e->completados;
    i->subdesbordes = e->subdesb;
    i->errores = e->erros;
    i->ancho = e->mps;
    return true;
}

void uhci_iso_fechar(usb_disp_t *d, int h)
{
    bool i0 = int_salva_desliga();
    uhci_iso_t *e = h >= 0 && h < USB_ISO_MAX * USB_MAX_CONTROLADORES && isoc[h].usado ? &isoc[h] : NULL;
    if (e) {
        uhci_t *c = e->c;
        e->ligado = false;
        volatile uint32_t *lista = (volatile uint32_t *)c->quadros;
        for (uint32_t f = 0; f < 1024; f++) lista[f] = (uint32_t)((f & 7) == 0 ? qh_int(c) : qh_trab(c)) | L_QH;
        pausa_ms(3);                                          /* el controlador ya no los lee */
        memfisica_liberar(e->tds);
        int ci = (int)(c - ctls);
        if (uhci_ancho_iso[ci] >= e->mps) uhci_ancho_iso[ci] -= e->mps;
        e->usado = false;
    }
    (void)d;
    int_restaura(i0);
}
