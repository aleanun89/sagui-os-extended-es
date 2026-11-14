// SPDX-License-Identifier: GPL-2.0-or-later
/* hid.c - teclados y ratones USB (clase HID) por el xHCI y por el EHCI (D214).
 *
 * En un PC moderno sin emulación PS/2 en la BIOS, el teclado y el ratón USB
 * son la única forma de manejar el sistema. Aquí:
 *
 *   - TECLADO: "boot protocol" (SET_PROTOCOL 0): cada informe son 8 bytes
 *     (modificadores, reservado y hasta 6 teclas pulsadas, en códigos de uso
 *     HID). Se compara con el informe anterior y cada tecla que se pulsa o se
 *     suelta se traduce al código del teclado PS/2 (set 1, con el prefijo E0
 *     de las extendidas) y entra por teclado_byte(): la misma cola, las mismas
 *     distribuciones (ES, ABNT2), las teclas muertas, los atajos y el mapa de
 *     teclas sostenidas de los juegos. El teclado USB no repite solo (SET_IDLE
 *     0: solo informa de cambios): la repetición (500 ms y 30 por segundo) la
 *     hace esta tarea. Las luces (Bloq Mayús, Bloq Num) se le mandan con
 *     SET_REPORT cuando cambian;
 *   - RATÓN y TABLETA: se lee el descriptor de informe y se buscan X, Y, la
 *     rueda y los botones (posiciones en bits, relativo o absoluto); así
 *     funcionan los ratones con rueda en el protocolo normal y la tableta del
 *     QEMU (posición absoluta). Si el descriptor no se entiende y el ratón es
 *     "boot", se pasa al protocolo boot (botones, X, Y y quizá la rueda). Los
 *     movimientos entran por mouse_usb() en la misma cola que el ratón PS/2.
 * El informe llega por el endpoint de interrupción de entrada: en el xHCI
 * queda una lectura armada en su anillo; en el EHCI, una QH en la lista
 * periódica. Una tarea del kernel ("usb-hid") los mira cada tic (10 ms) o al
 * llegar la IRQ del xHCI. */
#include "usb.h"
#include "console.h"
#include "cpu.h"
#include "mouse.h"
#include "processo.h"
#include "relogio.h"
#include "teclado.h"
#include "texto.h"
#include "trava.h"
#include "xhci.h"

#define HID_MAX 6
#define TECLADO 1
#define PONTEIRO 2     /* ratón o tableta por el descriptor de informe */
#define RATO_BOOT 3

#define REPETIR_ATRASO (TIQUES_POR_SEGUNDO / 2)     /* 500 ms */
#define REPETIR_CADA   3                            /* 30 ms */

typedef struct {
    bool     presente, absoluto, sinal;
    uint16_t bit;
    uint8_t  tam;
    int32_t  min, max;
} campo_t;

typedef struct {
    bool     usado;
    usb_disp_t *d;
    uint8_t  tipo, interface, ep;
    uint16_t tam;
    int      ehci_h;
    uint32_t relatorios, erros, erros_seguidos;
    /* teclado */
    uint8_t  anterior[8];
    uint8_t  repetir;              /* código de uso que se repite (0 = ninguno) */
    uint32_t repetir_em, visto_em;
    uint8_t  leds;
    /* ratón por el descriptor de informe */
    bool     com_id;
    uint8_t  id;
    campo_t  x, y, roda;
    uint16_t bit_botoes;
    uint8_t  n_botoes;
} hid_t;

static hid_t hids[HID_MAX];
static trava_t trava_hid;
static const char canal = 0;
static bool tarefa_criada;
static uint32_t teclados, ratos;

/* ------------------------------------------------------------ teclado: uso HID -> PS/2 (set 1) */

#define E0 0x100
static const uint16_t uso_ps2[0x88] = {
    [0x04] = 0x1E, [0x05] = 0x30, [0x06] = 0x2E, [0x07] = 0x20, [0x08] = 0x12, [0x09] = 0x21, [0x0A] = 0x22,
    [0x0B] = 0x23, [0x0C] = 0x17, [0x0D] = 0x24, [0x0E] = 0x25, [0x0F] = 0x26, [0x10] = 0x32, [0x11] = 0x31,
    [0x12] = 0x18, [0x13] = 0x19, [0x14] = 0x10, [0x15] = 0x13, [0x16] = 0x1F, [0x17] = 0x14, [0x18] = 0x16,
    [0x19] = 0x2F, [0x1A] = 0x11, [0x1B] = 0x2D, [0x1C] = 0x15, [0x1D] = 0x2C,
    [0x1E] = 0x02, [0x1F] = 0x03, [0x20] = 0x04, [0x21] = 0x05, [0x22] = 0x06, [0x23] = 0x07, [0x24] = 0x08,
    [0x25] = 0x09, [0x26] = 0x0A, [0x27] = 0x0B,
    [0x28] = 0x1C, [0x29] = 0x01, [0x2A] = 0x0E, [0x2B] = 0x0F, [0x2C] = 0x39, [0x2D] = 0x0C, [0x2E] = 0x0D,
    [0x2F] = 0x1A, [0x30] = 0x1B, [0x31] = 0x2B, [0x32] = 0x2B, [0x33] = 0x27, [0x34] = 0x28, [0x35] = 0x29,
    [0x36] = 0x33, [0x37] = 0x34, [0x38] = 0x35, [0x39] = 0x3A,
    [0x3A] = 0x3B, [0x3B] = 0x3C, [0x3C] = 0x3D, [0x3D] = 0x3E, [0x3E] = 0x3F, [0x3F] = 0x40, [0x40] = 0x41,
    [0x41] = 0x42, [0x42] = 0x43, [0x43] = 0x44, [0x44] = 0x57, [0x45] = 0x58,
    [0x46] = E0 | 0x37, [0x47] = 0x46,
    [0x49] = E0 | 0x52, [0x4A] = E0 | 0x47, [0x4B] = E0 | 0x49, [0x4C] = E0 | 0x53, [0x4D] = E0 | 0x4F,
    [0x4E] = E0 | 0x51, [0x4F] = E0 | 0x4D, [0x50] = E0 | 0x4B, [0x51] = E0 | 0x50, [0x52] = E0 | 0x48,
    [0x53] = 0x45, [0x54] = E0 | 0x35, [0x55] = 0x37, [0x56] = 0x4A, [0x57] = 0x4E, [0x58] = E0 | 0x1C,
    [0x59] = 0x4F, [0x5A] = 0x50, [0x5B] = 0x51, [0x5C] = 0x4B, [0x5D] = 0x4C, [0x5E] = 0x4D, [0x5F] = 0x47,
    [0x60] = 0x48, [0x61] = 0x49, [0x62] = 0x52, [0x63] = 0x53,
    [0x64] = 0x56,                 /* la tecla < > del teclado español (junto al Mayús izquierdo) */
    [0x65] = E0 | 0x5D,
    [0x85] = 0x7E,                 /* ABNT2: el punto del teclado numérico */
    [0x87] = 0x73,                 /* ABNT2: la tecla / ? */
};
/* Modificadores, bit a bit: Ctrl, Mayús, Alt y Windows de la izquierda; los mismos de la derecha. */
static const uint16_t mod_ps2[8] = { 0x1D, 0x2A, 0x38, E0 | 0x5B, E0 | 0x1D, 0x36, E0 | 0x38, E0 | 0x5C };

static void ps2(uint16_t c, bool solta)
{
    if (!c) return;
    bool i0 = int_salva_desliga();
    if (c & E0) teclado_byte(0xE0);
    teclado_byte((uint8_t)((c & 0x7F) | (solta ? 0x80 : 0)));
    int_restaura(i0);
}

static bool contem(const uint8_t *r, uint8_t u)
{
    for (int i = 2; i < 8; i++) if (r[i] == u) return true;
    return false;
}

static void teclado_relatorio(hid_t *h, const uint8_t *r, int n)
{
    uint8_t novo[8] = { 0 };
    memcpy(novo, r, (uint32_t)(n > 8 ? 8 : n));
    if (novo[2] == 1 && novo[3] == 1) return;   /* demasiadas teclas a la vez (ErrorRollOver) */
    uint8_t mudou = novo[0] ^ h->anterior[0];
    for (int b = 0; b < 8; b++)
        if (mudou & (1u << b)) ps2(mod_ps2[b], !(novo[0] & (1u << b)));
    for (int i = 2; i < 8; i++) {
        uint8_t u = h->anterior[i];
        if (u > 3 && !contem(novo, u)) {
            if (u < sizeof uso_ps2 / sizeof uso_ps2[0]) ps2(uso_ps2[u], true);
            if (u == h->repetir) h->repetir = 0;
        }
    }
    for (int i = 2; i < 8; i++) {
        uint8_t u = novo[i];
        if (u > 3 && !contem(h->anterior, u) && u < sizeof uso_ps2 / sizeof uso_ps2[0] && uso_ps2[u]) {
            ps2(uso_ps2[u], false);
            /* Se repite la última que se pulsa, menos Bloq Mayús/Num/Despl. */
            if (u != 0x39 && u != 0x53 && u != 0x47) {
                h->repetir = u;
                h->repetir_em = relogio_tiques() + REPETIR_ATRASO;
            }
        }
    }
    memcpy(h->anterior, novo, 8);
}

static void teclado_soltar_tudo(hid_t *h)
{
    uint8_t vazio[8] = { 0 };
    teclado_relatorio(h, vazio, 8);
    h->repetir = 0;
}

/* ------------------------------------------------------------ descriptor de informe */

static int32_t ler_bits(const uint8_t *r, int n, uint16_t bit, uint8_t tam, bool sinal)
{
    uint32_t v = 0;
    for (int i = 0; i < tam && i < 32; i++) {
        uint32_t b = bit + (uint32_t)i;
        if ((int)(b >> 3) >= n) break;
        if (r[b >> 3] & (1u << (b & 7))) v |= 1u << i;
    }
    if (sinal && tam < 32 && (v & (1u << (tam - 1)))) v |= ~0u << tam;
    return (int32_t)v;
}

/* Busca X, Y, la rueda y los botones del primer informe de entrada que los
 * tenga. Solo lo necesario para un ratón o una tableta. */
static bool analisar_relatorio(hid_t *h, const uint8_t *d, int n)
{
    uint32_t pagina = 0, tam = 0, cont = 0, id = 0;
    int32_t lmin = 0, lmax = 0;
    uint32_t usos[16], n_usos = 0, uso_min = 0, uso_max = 0;
    uint16_t desloc = 0;
    bool achou_id = false;
    for (int i = 0; i < n;) {
        uint8_t p = d[i++];
        if (p == 0xFE) { if (i + 1 < n) i += 2 + d[i]; continue; }   /* item largo */
        int t = p & 3; t = t == 3 ? 4 : t;
        if (i + t > n) break;
        uint32_t v = 0;
        for (int k = 0; k < t; k++) v |= (uint32_t)d[i + k] << (8 * k);
        int32_t sv = t == 1 ? (int8_t)v : t == 2 ? (int16_t)v : (int32_t)v;
        i += t;
        uint8_t tipo = (p >> 2) & 3, tag = p >> 4;
        if (tipo == 1) {            /* global */
            if (tag == 0) pagina = v;
            else if (tag == 1) lmin = sv;
            else if (tag == 2) lmax = (lmin >= 0 && t < 4 && sv < 0) ? (int32_t)v : sv;
            else if (tag == 7) tam = v;
            else if (tag == 8) {
                if (h->x.presente) break;   /* ya está todo en el informe anterior */
                /* Otro informe: lo que se viera en el anterior (sin X) no vale. */
                h->n_botoes = 0;
                h->y.presente = h->roda.presente = false;
                id = v; desloc = 0; achou_id = true;
            }
            else if (tag == 9) cont = v;
        } else if (tipo == 2) {     /* local */
            if (tag == 0 && n_usos < 16) usos[n_usos++] = (t == 4) ? v : (pagina << 16) | v;
            else if (tag == 1) uso_min = (t == 4) ? v : (pagina << 16) | v;
            else if (tag == 2) uso_max = (t == 4) ? v : (pagina << 16) | v;
        } else if (tipo == 0) {     /* principal */
            if (tag == 8) {         /* Input */
                bool constante = v & 1, relativo = (v >> 2) & 1;
                for (uint32_t k = 0; k < cont && !constante; k++) {
                    uint32_t u = k < n_usos ? usos[k] : n_usos ? usos[n_usos - 1] :
                                 uso_min ? uso_min + k : 0;
                    if (uso_max && u > uso_max) u = uso_max;
                    campo_t c = { true, !relativo, lmin < 0, (uint16_t)(desloc + k * tam), (uint8_t)tam, lmin, lmax };
                    if (u == 0x10030 && !h->x.presente) { h->x = c; h->id = (uint8_t)id; h->com_id = achou_id; }
                    else if (u == 0x10031 && !h->y.presente) h->y = c;
                    else if (u == 0x10038 && !h->roda.presente) h->roda = c;
                    else if ((u >> 16) == 9 && !h->n_botoes && tam == 1) {
                        h->bit_botoes = (uint16_t)(desloc + k);
                        h->n_botoes = (uint8_t)(cont - k > 5 ? 5 : cont - k);
                    }
                }
                desloc = (uint16_t)(desloc + cont * tam);
            }
            n_usos = 0; uso_min = uso_max = 0;
        }
    }
    return h->x.presente && h->y.presente;
}

static void ponteiro_relatorio(hid_t *h, const uint8_t *r, int n)
{
    if (h->com_id) {
        if (n < 1 || r[0] != h->id) return;
        r++; n--;
    }
    uint8_t botoes = 0;
    for (int i = 0; i < h->n_botoes && i < 3; i++)
        if (ler_bits(r, n, (uint16_t)(h->bit_botoes + i), 1, false)) botoes |= (uint8_t)(1u << i);
    int dz = h->roda.presente ? -ler_bits(r, n, h->roda.bit, h->roda.tam, h->roda.sinal) : 0;
    int32_t x = ler_bits(r, n, h->x.bit, h->x.tam, h->x.sinal), y = ler_bits(r, n, h->y.bit, h->y.tam, h->y.sinal);
    bool i0 = int_salva_desliga();
    if (h->x.absoluto && h->x.max > h->x.min)
        mouse_usb_absoluto((uint32_t)(x - h->x.min), (uint32_t)(h->x.max - h->x.min),
                           (uint32_t)(y - h->y.min), (uint32_t)(h->y.max - h->y.min), dz, botoes);
    else mouse_usb(x, y, dz, botoes);
    int_restaura(i0);
}

static void rato_boot(const uint8_t *r, int n)
{
    if (n < 3) return;
    int dz = n >= 4 ? -(int8_t)r[3] : 0;
    bool i0 = int_salva_desliga();
    mouse_usb((int8_t)r[1], (int8_t)r[2], dz, r[0] & 7);
    int_restaura(i0);
}

/* ------------------------------------------------------------ transporte */

static int ler(hid_t *h, uint8_t *b, int max)
{
    if (h->d->tipo_ctl == USB_XHCI) return xhci_int_ler(h->d, h->ep, b, max);
    return ehci_int_ler(h->ehci_h, b, max);
}

static void tratar(hid_t *h, const uint8_t *b, int n)
{
    h->relatorios++;
    if (h->relatorios == 1)
        klog("[HID] primer informe del %s USB (%d bytes)\n", h->tipo == TECLADO ? "teclado" : "raton", n);
    if (h->tipo == TECLADO) teclado_relatorio(h, b, n);
    else if (h->tipo == PONTEIRO) ponteiro_relatorio(h, b, n);
    else rato_boot(b, n);
}

static void atualizar_leds(hid_t *h)
{
    uint8_t l = teclado_leds_usb();
    if (l == h->leds) return;
    h->leds = l;
    usb_controle(h->d, 0x21, 0x09, 0x0200, h->interface, &l, 1);   /* SET_REPORT (saída) */
}

static void tarefa_hid(void *arg)
{
    (void)arg;
    for (;;) {
        trava_pegar(&trava_hid);
        bool algum = false;
        uint32_t agora = relogio_tiques();
        for (int i = 0; i < HID_MAX; i++) {
            hid_t *h = &hids[i];
            if (!h->usado) continue;
            algum = true;
            uint8_t b[64];
            for (int k = 0; k < 8; k++) {
                int n = ler(h, b, sizeof b);
                if (n < 0) {
                    h->erros++;
                    if (++h->erros_seguidos == 20) klog("[HID] el dispositivo %s no responde\n", h->d->produto);
                    break;
                }
                h->visto_em = agora;   /* el estado de las teclas está al día */
                if (n == 0) break;
                h->erros_seguidos = 0;
                tratar(h, b, n);
            }
            if (h->tipo == TECLADO) {
                /* Solo se repite si el teclado se acaba de mirar: si no, la
                 * tecla puede estar ya suelta y el informe aún sin leer. */
                if (h->repetir && agora - h->visto_em <= 2 && (int32_t)(agora - h->repetir_em) >= 0) {
                    ps2(uso_ps2[h->repetir], false);
                    h->repetir_em = agora + REPETIR_CADA;
                }
                atualizar_leds(h);
            }
        }
        trava_soltar(&trava_hid);
        int_desliga();
        if (algum) processo_atual()->acordar_em = relogio_tiques() + 1;
        bloquear(&canal);
        ceder();
        int_liga();
    }
}

/* ------------------------------------------------------------ conexión */

static void conectar_interface(usb_disp_t *d, usb_hid_if_t *f)
{
    hid_t h;
    memset(&h, 0, sizeof h);
    h.d = d;
    h.interface = f->interface;
    h.ep = f->ep;
    h.tam = f->mps > 64 ? 64 : f->mps;
    h.ehci_h = -1;
    if (f->subclasse == 1 && f->protocolo == 1) {
        h.tipo = TECLADO;
        usb_controle(d, 0x21, 0x0B, 0, f->interface, NULL, 0);   /* SET_PROTOCOL: boot */
        usb_controle(d, 0x21, 0x0A, 0, f->interface, NULL, 0);   /* SET_IDLE: solo los cambios */
    } else {
        static uint8_t desc[1024];
        uint16_t n = f->tam_relatorio ? f->tam_relatorio : 256;
        if (n > sizeof desc) n = sizeof desc;
        int r = usb_controle(d, 0x81, 6, 0x2200, f->interface, desc, n);
        if (r > 0 && analisar_relatorio(&h, desc, r)) {
            h.tipo = PONTEIRO;
            if (f->subclasse == 1) usb_controle(d, 0x21, 0x0B, 1, f->interface, NULL, 0);   /* protocolo normal */
            usb_controle(d, 0x21, 0x0A, 0, f->interface, NULL, 0);
        } else if (f->subclasse == 1 && f->protocolo == 2) {
            h.tipo = RATO_BOOT;
            usb_controle(d, 0x21, 0x0B, 0, f->interface, NULL, 0);
        } else {
            klog("[HID] interfaz %u de %s: no es teclado ni raton (%u/%u)\n", f->interface, d->produto,
                 f->subclasse, f->protocolo);
            return;
        }
    }
    if (d->tipo_ctl == USB_XHCI) {
        if (xhci_int_armar(d, h.ep, h.tam) < 0) { klog("[HID] no se ha podido armar el endpoint %u\n", h.ep); return; }
    } else {
        h.ehci_h = ehci_int_abrir(d, h.ep, h.tam);
        if (h.ehci_h < 0) { klog("[HID] no hay sitio en la lista periodica del EHCI\n"); return; }
    }
    h.leds = 0xFF;   /* manda las luces en la primera vuelta */
    h.usado = true;
    trava_pegar(&trava_hid);
    int k = -1;
    for (int i = 0; i < HID_MAX; i++) if (!hids[i].usado) { k = i; break; }
    if (k >= 0) hids[k] = h;
    trava_soltar(&trava_hid);
    if (k < 0) {
        if (h.ehci_h >= 0) ehci_int_fechar(h.ehci_h);
        klog("[HID] demasiados dispositivos HID\n");
        return;
    }
    if (h.tipo == TECLADO) teclados++;
    else { ratos++; mouse_usb_presente(h.tipo == PONTEIRO ? h.roda.presente : true); }
    if (h.tipo == PONTEIRO)
        klog("[HID] %s USB: %s %s (interfaz %u, endpoint %u, %s, X %u bits, rueda %s, %u botones) por %s\n",
             h.x.absoluto ? "tableta" : "raton", d->fabricante, d->produto, h.interface, h.ep,
             h.x.absoluto ? "posicion absoluta" : "relativo", h.x.tam, h.roda.presente ? "si" : "no", h.n_botoes,
             d->tipo_ctl == USB_XHCI ? "xHCI" : "EHCI");
    else
        klog("[HID] %s USB: %s %s (interfaz %u, endpoint %u, protocolo boot) por %s\n",
             h.tipo == TECLADO ? "teclado" : "raton", d->fabricante, d->produto, h.interface, h.ep,
             d->tipo_ctl == USB_XHCI ? "xHCI" : "EHCI");
    if (!tarefa_criada) {
        tarefa_criada = true;
        xhci_avisar_hid(&canal);
        tarefa_kernel("usb-hid", tarefa_hid, NULL);
    }
    acordar(&canal);
}

void hid_conectar(usb_disp_t *d)
{
    for (int i = 0; i < d->n_hid; i++)
        if (d->hid[i].ep) conectar_interface(d, &d->hid[i]);
}

void hid_desconectar(usb_disp_t *d)
{
    trava_pegar(&trava_hid);
    for (int i = 0; i < HID_MAX; i++) {
        hid_t *h = &hids[i];
        if (!h->usado || h->d != d) continue;
        if (h->tipo == TECLADO) { teclado_soltar_tudo(h); teclados--; }
        else ratos--;
        if (h->ehci_h >= 0) ehci_int_fechar(h->ehci_h);
        h->usado = false;
        klog("[HID] %s USB desconectado\n", h->tipo == TECLADO ? "teclado" : "raton");
    }
    trava_soltar(&trava_hid);
}

int hid_relatorio(char *s, int max)
{
    int k = 0;
    for (int i = 0; i < HID_MAX && k < max; i++) {
        hid_t *h = &hids[i];
        if (!h->usado) continue;
        k += snformatar(s + k, (size_t)(max - k), "HID: %s USB %s %s por %s, %u informes, %u errores\n",
                        h->tipo == TECLADO ? "teclado" : h->tipo == PONTEIRO && h->x.absoluto ? "tableta" : "ratón",
                        h->d->fabricante, h->d->produto, h->d->tipo_ctl == USB_XHCI ? "xHCI" : "EHCI",
                        h->relatorios, h->erros);
    }
    return k;
}
