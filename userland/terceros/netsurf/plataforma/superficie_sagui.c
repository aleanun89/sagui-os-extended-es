// SPDX-License-Identifier: GPL-2.0-or-later
/* superficie_sagui.c - la superficie "sagui" de libnsfb (D184): NetSurf
 * dibuja directamente en los píxeles de una ventana del SaguiOS.
 *
 * libnsfb (la biblioteca de framebuffer del frontend framebuffer de NetSurf)
 * tiene superficies para SDL, X11, VNC, Wayland y el framebuffer de Linux.
 * Esta es la del SaguiOS, sobre la API de ventanas (criar_janela,
 * esperar_evento, atualizar_janela). Es el mismo papel que hace
 * plataforma/sdl_sagui.c para el OpenTyrian (D156).
 *
 *   - píxeles: la ventana es 0x00RRGGBB por píxel, que es el formato
 *     XRGB8888 de libnsfb; el área de la ventana es el propio framebuffer
 *     (sin copia). update() publica el rectángulo cambiado.
 *   - ratón: EV_MOVER/EV_CLIQUE/EV_SOLTOU/EV_RODA a movimientos absolutos y
 *     teclas MOUSE_1 (izquierdo), MOUSE_3 (derecho), MOUSE_4/5 (rueda).
 *   - teclado: el SaguiOS da el carácter ya traducido (Latin-1) y no avisa
 *     de cuándo se suelta una tecla; libnsfb trabaja con códigos de tecla de
 *     un teclado de EE. UU. y Mayúsculas/Control como teclas aparte. Cada
 *     pulsación se convierte en la secuencia de libnsfb (Mayús abajo, tecla
 *     abajo, tecla arriba, Mayús arriba). Carencia: los caracteres que no
 *     están en ese teclado (ñ, á, ç...) no se pueden escribir en NetSurf.
 *   - el puntero lo dibuja el SaguiOS: el cursor por software de libnsfb no
 *     se pinta (claim/update no lo tocan).
 *
 * Es código nuestro; el registro usa el NSFB_SURFACE_DEF de libnsfb (un
 * constructor, que plataforma/principal_ns.c llama, ver netsurf.ld).
 */
#include "sagui.h"

#include <stdbool.h>
#include <stdint.h>

#include "libnsfb.h"
#include "libnsfb_event.h"
#include "libnsfb_plot.h"
#include "nsfb.h"
#include "plot.h"
#include "surface.h"

#define FILA_MAX 16

typedef struct {
    int id;                      /* la ventana */
    uint32_t *px;
    nsfb_event_t fila[FILA_MAX]; /* eventos de libnsfb pendientes */
    int ini, n;
    bool botao_esq, botao_dir;
} sagui_priv_t;

/* Título de la ventana (lo cambia el frontend con la URL, ver principal_ns.c). */
const char *ns_sagui_titulo = "NetSurf";
/* El id de la ventana, para quien lo necesite (la prueba, el título). */
int ns_sagui_janela = -1;

static void empurrar(sagui_priv_t *p, enum nsfb_event_type_e tipo, int valor)
{
    if (p->n >= FILA_MAX) return;
    nsfb_event_t *e = &p->fila[(p->ini + p->n++) % FILA_MAX];
    e->type = tipo;
    e->value.keycode = (enum nsfb_key_code_e)valor;
}

static void empurrar_mover(sagui_priv_t *p, int x, int y)
{
    if (p->n >= FILA_MAX) return;
    nsfb_event_t *e = &p->fila[(p->ini + p->n++) % FILA_MAX];
    e->type = NSFB_EVENT_MOVE_ABSOLUTE;
    e->value.vector.x = x;
    e->value.vector.y = y;
    e->value.vector.z = 0;
}

static void empurrar_tecla(sagui_priv_t *p, int codigo, bool shift, bool ctrl)
{
    if (ctrl) empurrar(p, NSFB_EVENT_KEY_DOWN, NSFB_KEY_LCTRL);
    if (shift) empurrar(p, NSFB_EVENT_KEY_DOWN, NSFB_KEY_LSHIFT);
    empurrar(p, NSFB_EVENT_KEY_DOWN, codigo);
    empurrar(p, NSFB_EVENT_KEY_UP, codigo);
    if (shift) empurrar(p, NSFB_EVENT_KEY_UP, NSFB_KEY_LSHIFT);
    if (ctrl) empurrar(p, NSFB_EVENT_KEY_UP, NSFB_KEY_LCTRL);
}

/* Los caracteres que el teclado de EE. UU. de fbtk (fbtk/event.c) da con
 * Mayúsculas, y la tecla que hay que pulsar con ella. */
static const struct { char c; char tecla; } com_shift[] = {
    { '%', '5' }, { '^', '6' }, { '{', '[' }, { '}', ']' }, { '|', '\\' },
    { '~', '#' }, { '_', '-' },
};

static void traduzir_tecla(sagui_priv_t *p, const evento_t *e)
{
    unsigned t = e->tecla;
    bool ctrl = (e->modificadores & 2) != 0;
    switch (t) {
    case TECLA_CIMA:     empurrar_tecla(p, NSFB_KEY_UP, false, ctrl); return;
    case TECLA_BAIXO:    empurrar_tecla(p, NSFB_KEY_DOWN, false, ctrl); return;
    case TECLA_ESQUERDA: empurrar_tecla(p, NSFB_KEY_LEFT, false, ctrl); return;
    case TECLA_DIREITA:  empurrar_tecla(p, NSFB_KEY_RIGHT, false, ctrl); return;
    case TECLA_HOME:     empurrar_tecla(p, NSFB_KEY_HOME, false, ctrl); return;
    case TECLA_END:      empurrar_tecla(p, NSFB_KEY_END, false, ctrl); return;
    case TECLA_PGUP:     empurrar_tecla(p, NSFB_KEY_PAGEUP, false, ctrl); return;
    case TECLA_PGDN:     empurrar_tecla(p, NSFB_KEY_PAGEDOWN, false, ctrl); return;
    case TECLA_INSERT:   empurrar_tecla(p, NSFB_KEY_INSERT, false, ctrl); return;
    case TECLA_DELETE:   empurrar_tecla(p, NSFB_KEY_DELETE, false, ctrl); return;
    case TECLA_ESC:      empurrar_tecla(p, NSFB_KEY_ESCAPE, false, false); return;
    default: break;
    }
    if (t >= 0x100) return;   /* F1..F12 y demás: NetSurf framebuffer no las usa */
    /* El 8, el 9 y el 10 son Retroceso, Tabulador e Intro... salvo con
     * Control, que son Ctrl+H, Ctrl+I y Ctrl+J (como en la D156). */
    if (ctrl && t >= 1 && t <= 26) { empurrar_tecla(p, 'a' + (int)t - 1, false, true); return; }
    if (t == 10 || t == 13) { empurrar_tecla(p, NSFB_KEY_RETURN, false, false); return; }
    if (t == 8) { empurrar_tecla(p, NSFB_KEY_BACKSPACE, false, false); return; }
    if (t == 9) { empurrar_tecla(p, NSFB_KEY_TAB, (e->modificadores & 1) != 0, false); return; }
    if (t == 27) { empurrar_tecla(p, NSFB_KEY_ESCAPE, false, false); return; }
    if (t >= 'A' && t <= 'Z') { empurrar_tecla(p, (int)t + 32, true, ctrl); return; }
    if ((t >= 'a' && t <= 'z') || (t >= '0' && t <= '9')) { empurrar_tecla(p, (int)t, false, ctrl); return; }
    for (unsigned i = 0; i < sizeof com_shift / sizeof com_shift[0]; i++)
        if ((unsigned char)com_shift[i].c == t) { empurrar_tecla(p, com_shift[i].tecla, true, ctrl); return; }
    if (t >= 32 && t < 127) { empurrar_tecla(p, (int)t, false, ctrl); return; }
    /* Latin-1 por encima de 127: libnsfb no tiene cómo. */
}

static void traduzir(sagui_priv_t *p, const evento_t *e)
{
    switch (e->tipo) {
    case EV_MOVER:
        empurrar_mover(p, e->x, e->y);
        break;
    case EV_CLIQUE:
        empurrar_mover(p, e->x, e->y);
        if (e->botoes & 2) { p->botao_dir = true; empurrar(p, NSFB_EVENT_KEY_DOWN, NSFB_KEY_MOUSE_3); }
        else { p->botao_esq = true; empurrar(p, NSFB_EVENT_KEY_DOWN, NSFB_KEY_MOUSE_1); }
        break;
    case EV_SOLTOU:
        empurrar_mover(p, e->x, e->y);
        if (p->botao_esq) { p->botao_esq = false; empurrar(p, NSFB_EVENT_KEY_UP, NSFB_KEY_MOUSE_1); }
        if (p->botao_dir) { p->botao_dir = false; empurrar(p, NSFB_EVENT_KEY_UP, NSFB_KEY_MOUSE_3); }
        break;
    case EV_RODA: {
        int n = (int)e->cliques;
        int tecla = n < 0 ? NSFB_KEY_MOUSE_4 : NSFB_KEY_MOUSE_5;
        if (n < 0) n = -n;
        empurrar_mover(p, e->x, e->y);
        for (int i = 0; i < n && i < 3; i++) {
            empurrar(p, NSFB_EVENT_KEY_DOWN, tecla);
            empurrar(p, NSFB_EVENT_KEY_UP, tecla);
        }
        break;
    }
    case EV_TECLA:
        traduzir_tecla(p, e);
        break;
    case EV_FECHAR:
        empurrar(p, NSFB_EVENT_CONTROL, NSFB_CONTROL_QUIT);
        break;
    case EV_REDIMENSIONAR:
        if (p->n < FILA_MAX) {
            nsfb_event_t *r = &p->fila[(p->ini + p->n++) % FILA_MAX];
            r->type = NSFB_EVENT_RESIZE;
            r->value.resize.w = e->x;
            r->value.resize.h = e->y;
        }
        break;
    default:
        break;
    }
}

static int sagui_defaults(nsfb_t *nsfb)
{
    nsfb->width = 800;
    nsfb->height = 600;
    nsfb->format = NSFB_FMT_XRGB8888;
    nsfb->bpp = 32;
    select_plotters(nsfb);
    return 0;
}

static int sagui_initialise(nsfb_t *nsfb)
{
    if (nsfb->surface_priv) return -1;
    if (nsfb->bpp != 32) return -1;
    sagui_priv_t *p = aloca_zerado(sizeof *p);
    if (!p) return -1;
    int l = nsfb->width, a = nsfb->height;
    if (l > JANELA_LARG_MAX) l = JANELA_LARG_MAX;
    if (a > JANELA_ALT_MAX) a = JANELA_ALT_MAX;
    p->id = criar_janela(ns_sagui_titulo, l, a, &p->px);
    if (p->id < 0) { libera(p); return -1; }
    estilo_janela(p->id, JAN_REDIMENSIONAVEL, 320, 240);
    ns_sagui_janela = p->id;
    nsfb->width = l;
    nsfb->height = a;
    nsfb->format = NSFB_FMT_XRGB8888;
    select_plotters(nsfb);
    nsfb->surface_priv = p;
    nsfb->ptr = (uint8_t *)p->px;
    nsfb->linelen = l * 4;
    return 0;
}

static int sagui_set_geometry(nsfb_t *nsfb, int width, int height, enum nsfb_format_e format)
{
    sagui_priv_t *p = nsfb->surface_priv;
    if (width > JANELA_LARG_MAX) width = JANELA_LARG_MAX;
    if (height > JANELA_ALT_MAX) height = JANELA_ALT_MAX;
    if (width > 0) nsfb->width = width;
    if (height > 0) nsfb->height = height;
    if (format != NSFB_FMT_ANY && format != NSFB_FMT_XRGB8888) return -1;
    nsfb->format = NSFB_FMT_XRGB8888;
    select_plotters(nsfb);
    if (p) {
        if (redimensionar_janela(p->id, nsfb->width, nsfb->height, &p->px) < 0) return -1;
        nsfb->ptr = (uint8_t *)p->px;
    }
    nsfb->linelen = nsfb->width * 4;
    return 0;
}

static int sagui_finalise(nsfb_t *nsfb)
{
    sagui_priv_t *p = nsfb->surface_priv;
    if (p) {
        fechar_janela(p->id);
        ns_sagui_janela = -1;
        libera(p);
        nsfb->surface_priv = NULL;
        nsfb->ptr = NULL;
    }
    return 0;
}

static bool sagui_input(nsfb_t *nsfb, nsfb_event_t *event, int timeout)
{
    sagui_priv_t *p = nsfb->surface_priv;
    if (!p) return false;
    if (p->n == 0) {
        evento_t e;
        /* Primero lo que ya está en la cola, sin esperar; luego, si no hay
         * nada, hasta el plazo (-1 = sin plazo). */
        while (p->n == 0 && proximo_evento(p->id, &e, false) == 1) traduzir(p, &e);
        if (p->n == 0 && timeout != 0) {
            if (esperar_evento(p->id, &e, timeout < 0 ? -1 : timeout) == 1) traduzir(p, &e);
        }
    }
    if (p->n == 0) {
        if (timeout == 0) return false;
        event->type = NSFB_EVENT_CONTROL;
        event->value.controlcode = NSFB_CONTROL_TIMEOUT;
        return true;
    }
    *event = p->fila[p->ini];
    p->ini = (p->ini + 1) % FILA_MAX;
    p->n--;
    return true;
}

static int sagui_claim(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    (void)nsfb; (void)box;
    return 0;
}

static int sagui_update(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    sagui_priv_t *p = nsfb->surface_priv;
    if (!p) return -1;
    retangulo_t r = { box->x0, box->y0, box->x1 - box->x0, box->y1 - box->y0 };
    if (r.largura <= 0 || r.altura <= 0) return 0;
    atualizar_janela(p->id, &r);
    return 0;
}

static int sagui_cursor(nsfb_t *nsfb, struct nsfb_cursor_s *cursor)
{
    /* El puntero es el del SaguiOS. */
    (void)nsfb; (void)cursor;
    return 0;
}

const nsfb_surface_rtns_t sagui_rtns = {
    .defaults = sagui_defaults,
    .initialise = sagui_initialise,
    .finalise = sagui_finalise,
    .input = sagui_input,
    .claim = sagui_claim,
    .update = sagui_update,
    .cursor = sagui_cursor,
    .geometry = sagui_set_geometry,
};

/* El tipo NSFB_SURFACE_SDL (la SDL no se compila): el nombre "sagui" es el
 * que se elige con -f y el que el frontend toma por defecto (el registrado
 * con el tipo más bajo). La superficie "ram" de libnsfb también se compila:
 * el frontend la usa para los mapas de bits de las imágenes. */
NSFB_SURFACE_DEF(sagui, NSFB_SURFACE_SDL, &sagui_rtns)
