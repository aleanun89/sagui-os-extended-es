// SPDX-License-Identifier: GPL-3.0-only
/* terminal_vt.h - el emulador de pantalla del Terminal (D320).
 *
 * Un subconjunto de xterm/VT220 suficiente para las aplicaciones de pantalla completa (vim, less, htop,
 * nano, mc, tmux) que llegan por ssh (D317). Es lógica pura, sin pantalla ni procesos: se compila en el PC con
 * -DTERMINAL_ANFITRION y se prueba sin QEMU (tests/terminal_vt_unidad.c). terminal.c le pone la rejilla y
 * recibe lo que necesita por callbacks (historial, líneas sucias, respuestas, campana, título).
 *
 * Lo que entiende (tabla completa en docs/terminal.md):
 *   - controles: BEL BS HT LF VT FF CR SO SI; ESC 7 8 D E M c = > ( ) #8;
 *   - CSI: A B C D E F G H I Z @ P L M S T X b d e f g r s u n c t h l m, con «?» y «>»;
 *   - regiones de desplazamiento (DECSTBM), modo origen, salto de línea automático, pantalla alternativa
 *     (47, 1047, 1048, 1049), cursor visible, DECCKM, teclado numérico de aplicación, pegado entre corchetes;
 *   - el juego de caracteres de dibujo de líneas DEC (ESC ( 0).
 *
 * Los modos «xterm» (el cliente ssh lo enciende con CSI ? 7700 h) cambian tres cosas para quien habla con
 * un terminal de verdad: LF no vuelve a la columna 0, BS solo mueve el cursor y las teclas salen como en un
 * xterm. Sin él, el Terminal se comporta como siempre con el sagsh (D231): LF = CR+LF y BS borra. */
#ifndef TERMINAL_VT_H
#define TERMINAL_VT_H
#include "terminal_logica.h"

#ifdef TERMINAL_ANFITRION
#include <stdlib.h>
#define VT_ALOCAR(n) calloc(1, (n))
#else
#define VT_ALOCAR(n) aloca_zerado(n)
#endif

#define VT_STRIDE TL_COLS_MAX

typedef struct vt_s vt_t;
typedef struct { int col, lin; uint16_t at; bool pend, origem; int gl, cs0, cs1; } vt_sv_t;
struct vt_s {
    char     *g;                 /* rejilla: TL_LINS_MAX x VT_STRIDE letras */
    uint16_t *a;                 /* y sus atributos */
    int       cols, lins;        /* tamaño visible */
    int       col, lin;
    bool      pend;              /* salto de línea pendiente (se escribió en la última columna) */
    uint16_t  at;                /* atributo con el que se escribe */
    tl_esc_t  esc;
    int       top, bot;          /* región de desplazamiento (DECSTBM), base 0 */
    bool      xterm;             /* habla con un xterm (ssh): LF sin CR, BS sin borrar, teclas xterm */
    bool      lnm;               /* LF también vuelve a la columna 0 (el modo de siempre del sagsh) */
    bool      origem, autowrap, ins, cur_oculto, app_cur, app_kp, colar_cor;
    int       gl, cs[2];         /* juego activo (SO/SI) y juegos G0/G1: 0 ASCII, 1 dibujo DEC */
    vt_sv_t   sv, sv_alt;        /* DECSC / DECRC, y 1048 / 1049 */
    bool      alt;               /* pantalla alternativa en uso */
    char     *g2;                /* la otra pantalla (se reserva al primer uso) */
    uint16_t *a2;
    unsigned  ultimo;            /* última letra impresa (REP) */
    /* callbacks */
    void *u;
    void (*hist)(void *u, int linha);              /* la línea sube y sale por arriba: guardarla */
    void (*suja)(void *u, int l0, int l1);         /* líneas a redibujar */
    void (*resp)(void *u, const char *b, int n);   /* respuesta para el programa */
    void (*sino)(void *u);
    void (*osc)(void *u);                          /* OSC completa en esc.osc */
    void (*limpar_hist)(void *u);                  /* ED 3 */
};

#define VT_GR(v, l, c) ((v)->g[(l) * VT_STRIDE + (c)])
#define VT_AT(v, l, c) ((v)->a[(l) * VT_STRIDE + (c)])

static TL_USADA int vt_lim(int x, int a, int b) { return x < a ? a : x > b ? b : x; }

static TL_USADA void vt_sujar(vt_t *v, int l0, int l1) { if (v->suja) v->suja(v->u, l0, l1); }

static TL_USADA void vt_responder(vt_t *v, const char *s)
{
    if (v->resp) v->resp(v->u, s, (int)tl_len(s));
}

/* Borra c0..c1-1 de la línea l. Borra con el fondo actual (bce: xterm-256color lo anuncia). */
static TL_USADA void vt_apagar(vt_t *v, int l, int c0, int c1)
{
    if (c0 < 0) c0 = 0;
    if (c1 > VT_STRIDE) c1 = VT_STRIDE;
    if (c1 <= c0 || l < 0 || l >= TL_LINS_MAX) return;
    uint16_t at = (uint16_t)(v->at & TL_AT_FONDO);
    memset(&VT_GR(v, l, c0), ' ', (size_t)(c1 - c0));
    for (int c = c0; c < c1; c++) VT_AT(v, l, c) = at;
}

static TL_USADA void vt_apagar_tudo(vt_t *v)
{
    for (int l = 0; l < v->lins; l++) vt_apagar(v, l, 0, VT_STRIDE);
    vt_sujar(v, 0, v->lins - 1);
}

/* Desplaza las líneas t..b: n > 0 sube el contenido (entran líneas vacías por abajo), n < 0 lo baja. */
static TL_USADA void vt_rolar(vt_t *v, int t, int b, int n)
{
    int h = b - t + 1, k = n < 0 ? -n : n;
    if (h <= 0 || k <= 0) return;
    if (k > h) k = h;
    if (n > 0) {
        if (t == 0 && !v->alt && v->hist) for (int i = 0; i < k; i++) v->hist(v->u, i);
        if (h > k) {
            memmove(&VT_GR(v, t, 0), &VT_GR(v, t + k, 0), (size_t)(h - k) * VT_STRIDE);
            memmove(&VT_AT(v, t, 0), &VT_AT(v, t + k, 0), (size_t)(h - k) * VT_STRIDE * 2);
        }
        for (int l = b - k + 1; l <= b; l++) vt_apagar(v, l, 0, VT_STRIDE);
    } else {
        if (h > k) {
            memmove(&VT_GR(v, t + k, 0), &VT_GR(v, t, 0), (size_t)(h - k) * VT_STRIDE);
            memmove(&VT_AT(v, t + k, 0), &VT_AT(v, t, 0), (size_t)(h - k) * VT_STRIDE * 2);
        }
        for (int l = t; l < t + k; l++) vt_apagar(v, l, 0, VT_STRIDE);
    }
    vt_sujar(v, t, b);
}

static TL_USADA void vt_lf(vt_t *v)     /* IND */
{
    if (v->lin == v->bot) vt_rolar(v, v->top, v->bot, 1);
    else if (v->lin < v->lins - 1) v->lin++;
}

static TL_USADA void vt_ri(vt_t *v)
{
    if (v->lin == v->top) vt_rolar(v, v->top, v->bot, -1);
    else if (v->lin > 0) v->lin--;
}

static TL_USADA void vt_ir(vt_t *v, int l, int c)   /* mueve el cursor (base 0), respetando el modo origen */
{
    if (v->origem) l += v->top;
    v->lin = vt_lim(l, v->origem ? v->top : 0, v->origem ? v->bot : v->lins - 1);
    v->col = vt_lim(c, 0, v->cols - 1);
    v->pend = false;
}

static TL_USADA void vt_guardar(vt_t *v, bool alt)
{
    vt_sv_t *d = alt ? &v->sv_alt : &v->sv;
    d->col = v->col; d->lin = v->lin; d->at = v->at; d->pend = v->pend; d->origem = v->origem;
    d->gl = v->gl; d->cs0 = v->cs[0]; d->cs1 = v->cs[1];
}

static TL_USADA void vt_restaurar(vt_t *v, bool alt)
{
    vt_sv_t *d = alt ? &v->sv_alt : &v->sv;
    v->col = vt_lim(d->col, 0, v->cols - 1); v->lin = vt_lim(d->lin, 0, v->lins - 1);
    v->at = d->at; v->pend = d->pend; v->origem = d->origem;
    v->gl = d->gl; v->cs[0] = d->cs0; v->cs[1] = d->cs1;
}

/* RIS / DECSTR. */
static TL_USADA void vt_reset(vt_t *v, bool duro)
{
    v->at = 0; v->top = 0; v->bot = v->lins - 1; v->origem = false; v->autowrap = true; v->ins = false;
    v->cur_oculto = false; v->app_cur = false; v->app_kp = false; v->colar_cor = false;
    v->gl = 0; v->cs[0] = v->cs[1] = 0; v->pend = false;
    memset(&v->sv, 0, sizeof v->sv);
    if (duro) { v->col = v->lin = 0; vt_apagar_tudo(v); }
}

static TL_USADA void vt_iniciar(vt_t *v)
{
    v->lnm = true;
    vt_reset(v, false);
}

/* Cambia de pantalla (47, 1047, 1049). */
static TL_USADA void vt_pantalha(vt_t *v, bool alt, bool salvar, bool limpar)
{
    if (alt == v->alt) return;
    if (alt) {
        if (!v->g2) {
            v->g2 = VT_ALOCAR((size_t)VT_STRIDE * TL_LINS_MAX);
            v->a2 = VT_ALOCAR((size_t)VT_STRIDE * TL_LINS_MAX * 2);
            if (!v->g2 || !v->a2) {
#ifdef TERMINAL_ANFITRION
                free(v->g2); free(v->a2);
#else
                if (v->g2) libera(v->g2);
                if (v->a2) libera(v->a2);
#endif
                v->g2 = NULL; v->a2 = NULL;
                return;
            }
            memset(v->g2, ' ', (size_t)VT_STRIDE * TL_LINS_MAX);
        }
        if (salvar) vt_guardar(v, true);
    }
    char *tg = v->g; v->g = v->g2; v->g2 = tg;
    uint16_t *ta = v->a; v->a = v->a2; v->a2 = ta;
    v->alt = alt;
    if (alt && limpar) vt_apagar_tudo(v);
    if (!alt && salvar) vt_restaurar(v, true);
    vt_sujar(v, 0, v->lins - 1);
}

/* Un carácter imprimible. */
static TL_USADA void vt_imprimir(vt_t *v, unsigned c)
{
    uint16_t at = v->at;
    if (v->cs[v->gl] == 1 && c >= 0x5F && c <= 0x7E) {
        if (c == 0x5F) c = ' ';
        else at |= TL_AT_DEC;
    }
    if (v->pend) {
        v->pend = false;
        if (v->autowrap) { v->col = 0; vt_lf(v); }
    }
    if (v->ins && v->col < v->cols - 1) {
        int resto = v->cols - 1 - v->col;
        memmove(&VT_GR(v, v->lin, v->col + 1), &VT_GR(v, v->lin, v->col), (size_t)resto);
        memmove(&VT_AT(v, v->lin, v->col + 1), &VT_AT(v, v->lin, v->col), (size_t)resto * 2);
    }
    VT_GR(v, v->lin, v->col) = (char)c;
    VT_AT(v, v->lin, v->col) = at;
    v->ultimo = c;
    if (v->col >= v->cols - 1) { if (v->autowrap) v->pend = true; }
    else v->col++;
    vt_sujar(v, v->lin, v->lin);
}

/* ------------------------------------------------------------ modos */

static TL_USADA void vt_modo_priv(vt_t *v, int p, bool on)
{
    switch (p) {
    case 1: v->app_cur = on; break;
    case 6: v->origem = on; vt_ir(v, 0, 0); break;
    case 7: v->autowrap = on; if (!on) v->pend = false; break;
    case 25: v->cur_oculto = !on; break;
    case 47: vt_pantalha(v, on, false, false); break;
    case 1047: vt_pantalha(v, on, false, on); break;
    case 1048: if (on) vt_guardar(v, true); else vt_restaurar(v, true); break;
    case 1049: vt_pantalha(v, on, true, true); break;
    case 2004: v->colar_cor = on; break;
    case 7700:   /* SaguiOS: hay un xterm al otro lado (lo enciende ssh, lo apaga al salir y cada prompt del sagsh) */
        v->xterm = on; v->lnm = !on;
        if (!on) { v->app_cur = false; v->app_kp = false; v->colar_cor = false; vt_pantalha(v, false, false, false); }
        break;
    default: break;   /* 5, 12, 69, 1000-1006, 1015, 2026...: se aceptan sin efecto */
    }
}

/* ------------------------------------------------------------ CSI */

static TL_USADA void vt_csi(vt_t *v)
{
    const tl_esc_t *e = &v->esc;
    int n1 = tl_par(e, 0, 1);
    char r[48];
    if (e->inter) {
        if (e->inter == '!' && e->final == 'p') vt_reset(v, false);   /* DECSTR */
        return;
    }
    if (e->marca == '?') {
        if (e->final == 'h' || e->final == 'l')
            for (int i = 0; i < (e->n ? e->n : 1) && i < TL_PAR; i++) vt_modo_priv(v, e->par[i], e->final == 'h');
        else if (e->final == 'n' && e->n == 1 && e->par[0] == 6) {
            tl_formato(r, sizeof r, "\033[?%d;%dR", v->lin - (v->origem ? v->top : 0) + 1, v->col + 1);
            vt_responder(v, r);
        }
        return;
    }
    if (e->marca == '>') {
        if (e->final == 'c') vt_responder(v, "\033[>0;10;0c");   /* DA secundario */
        return;
    }
    if (e->marca) return;
    int reg_t = v->top, reg_b = v->bot;
    switch (e->final) {
    case 'A': { int lim = v->lin >= reg_t ? reg_t : 0; v->lin = vt_lim(v->lin - n1, lim, v->lins - 1); v->pend = false; break; }
    case 'B': case 'e': { int lim = v->lin <= reg_b ? reg_b : v->lins - 1; v->lin = vt_lim(v->lin + n1, 0, lim); v->pend = false; break; }
    case 'C': case 'a': v->col = vt_lim(v->col + n1, 0, v->cols - 1); v->pend = false; break;
    case 'D': v->col = vt_lim(v->col - n1, 0, v->cols - 1); v->pend = false; break;
    case 'E': v->lin = vt_lim(v->lin + n1, 0, v->lin <= reg_b ? reg_b : v->lins - 1); v->col = 0; v->pend = false; break;
    case 'F': v->lin = vt_lim(v->lin - n1, v->lin >= reg_t ? reg_t : 0, v->lins - 1); v->col = 0; v->pend = false; break;
    case 'G': case '`': v->col = vt_lim(n1 - 1, 0, v->cols - 1); v->pend = false; break;
    case 'd': vt_ir(v, n1 - 1, v->col); break;
    case 'H': case 'f': vt_ir(v, tl_par(e, 0, 1) - 1, tl_par(e, 1, 1) - 1); break;
    case 'I': v->col = vt_lim((v->col & ~7) + 8 * n1, 0, v->cols - 1); v->pend = false; break;
    case 'Z': v->col = vt_lim(((v->col - 1) & ~7) - 8 * (n1 - 1), 0, v->cols - 1); v->pend = false; break;
    case 'J': {
        int m = e->n ? e->par[0] : 0;
        if (m == 0) {
            vt_apagar(v, v->lin, v->col, VT_STRIDE);
            for (int l = v->lin + 1; l < v->lins; l++) vt_apagar(v, l, 0, VT_STRIDE);
            vt_sujar(v, v->lin, v->lins - 1);
        } else if (m == 1) {
            for (int l = 0; l < v->lin; l++) vt_apagar(v, l, 0, VT_STRIDE);
            vt_apagar(v, v->lin, 0, v->col + 1);
            vt_sujar(v, 0, v->lin);
        } else if (m == 2) vt_apagar_tudo(v);
        else if (m == 3 && v->limpar_hist) v->limpar_hist(v->u);
        break;
    }
    case 'K': {
        int m = e->n ? e->par[0] : 0;
        if (m == 1) vt_apagar(v, v->lin, 0, v->col + 1);
        else if (m == 2) vt_apagar(v, v->lin, 0, VT_STRIDE);
        else vt_apagar(v, v->lin, v->col, VT_STRIDE);
        vt_sujar(v, v->lin, v->lin);
        break;
    }
    case 'X': vt_apagar(v, v->lin, v->col, v->col + n1); vt_sujar(v, v->lin, v->lin); break;
    case '@': case 'P': {
        int n = vt_lim(n1, 1, v->cols - v->col), resto = v->cols - v->col - n;
        if (e->final == '@') {
            memmove(&VT_GR(v, v->lin, v->col + n), &VT_GR(v, v->lin, v->col), (size_t)resto);
            memmove(&VT_AT(v, v->lin, v->col + n), &VT_AT(v, v->lin, v->col), (size_t)resto * 2);
            vt_apagar(v, v->lin, v->col, v->col + n);
        } else {
            memmove(&VT_GR(v, v->lin, v->col), &VT_GR(v, v->lin, v->col + n), (size_t)resto);
            memmove(&VT_AT(v, v->lin, v->col), &VT_AT(v, v->lin, v->col + n), (size_t)resto * 2);
            vt_apagar(v, v->lin, v->cols - n, v->cols);
        }
        v->pend = false;
        vt_sujar(v, v->lin, v->lin);
        break;
    }
    case 'L': case 'M':   /* IL / DL: dentro de la región */
        if (v->lin >= reg_t && v->lin <= reg_b) {
            vt_rolar(v, v->lin, reg_b, e->final == 'M' ? n1 : -n1);
            v->col = 0; v->pend = false;
        }
        break;
    case 'S': vt_rolar(v, reg_t, reg_b, n1); break;
    case 'T': if (e->n <= 1) vt_rolar(v, reg_t, reg_b, -n1); break;
    case 'b':   /* REP */
        if (v->ultimo) for (int i = 0; i < vt_lim(n1, 1, 4000); i++) vt_imprimir(v, v->ultimo);
        break;
    case 'r': {   /* DECSTBM */
        int t = tl_par(e, 0, 1) - 1, b = tl_par(e, 1, v->lins) - 1;
        if (b > v->lins - 1) b = v->lins - 1;
        if (t < 0) t = 0;
        if (t < b) { v->top = t; v->bot = b; }
        else { v->top = 0; v->bot = v->lins - 1; }
        vt_ir(v, 0, 0);
        break;
    }
    case 's': vt_guardar(v, false); break;
    case 'u': vt_restaurar(v, false); break;
    case 'm': v->at = tl_sgr_todo(v->at, e); break;
    case 'h': case 'l':
        for (int i = 0; i < (e->n ? e->n : 1) && i < TL_PAR; i++) {
            if (e->par[i] == 4) v->ins = e->final == 'h';
            else if (e->par[i] == 20) v->lnm = e->final == 'h';
        }
        break;
    case 'g': break;   /* TBC: las tabulaciones son fijas, cada 8 */
    case 'n':
        if (e->n == 1 && e->par[0] == 5) vt_responder(v, "\033[0n");
        else if (e->n == 1 && e->par[0] == 6) {
            tl_formato(r, sizeof r, "\033[%d;%dR", v->lin - (v->origem ? v->top : 0) + 1, v->col + 1);
            vt_responder(v, r);
        }
        break;
    case 'c': if (!e->n || e->par[0] == 0) vt_responder(v, "\033[?62;22c"); break;   /* DA primario: VT220 con color */
    case 't':   /* D317: ESC [ 18 t = «¿cuántas filas y columnas?» (lo pregunta el cliente ssh) */
        if (e->n == 1 && e->par[0] == 18) {
            tl_formato(r, sizeof r, "\033[8;%d;%dt", v->lins, v->cols);
            vt_responder(v, r);
        }
        break;
    default: break;
    }
}

/* ------------------------------------------------------------ ESC */

static TL_USADA void vt_esc(vt_t *v)
{
    const tl_esc_t *e = &v->esc;
    if (e->inter == '(' || e->inter == ')') {
        v->cs[e->inter == '(' ? 0 : 1] = e->final == '0' ? 1 : 0;
        return;
    }
    if (e->inter == '#') {
        if (e->final == '8') {   /* DECALN */
            for (int l = 0; l < v->lins; l++) { memset(&VT_GR(v, l, 0), 'E', (size_t)v->cols); for (int c = 0; c < v->cols; c++) VT_AT(v, l, c) = 0; }
            vt_sujar(v, 0, v->lins - 1);
        }
        return;
    }
    if (e->inter) return;
    switch (e->final) {
    case '7': vt_guardar(v, false); break;
    case '8': vt_restaurar(v, false); break;
    case 'D': vt_lf(v); break;
    case 'E': vt_lf(v); v->col = 0; v->pend = false; break;
    case 'M': vt_ri(v); break;
    case '=': v->app_kp = true; break;
    case '>': v->app_kp = false; break;
    case 'c': vt_reset(v, true); break;
    default: break;
    }
}

/* ------------------------------------------------------------ un byte de la salida del programa */

static TL_USADA void vt_byte(vt_t *v, unsigned char c)
{
    int r = tl_esc_alimentar(&v->esc, c);
    if (r == TL_ESC_COMER) return;
    if (r == TL_ESC_CSI) { vt_csi(v); return; }
    if (r == TL_ESC_ESC) { vt_esc(v); return; }
    if (r == TL_ESC_OSC) { if (v->osc) v->osc(v->u); return; }
    switch (c) {
    case 7: if (v->sino) v->sino(v->u); return;
    case '\n': case 11: case 12:
        vt_lf(v);
        if (v->lnm) v->col = 0;
        v->pend = false;
        vt_sujar(v, v->lin, v->lin);
        return;
    case '\r': v->col = 0; v->pend = false; return;
    case '\t': v->col = vt_lim((v->col & ~7) + 8, 0, v->cols - 1); v->pend = false; return;
    case '\b':
        v->pend = false;
        if (v->col > 0) {
            v->col--;
            if (!v->xterm) { VT_GR(v, v->lin, v->col) = ' '; VT_AT(v, v->lin, v->col) = 0; vt_sujar(v, v->lin, v->lin); }
        }
        return;
    case 14: v->gl = 1; return;
    case 15: v->gl = 0; return;
    default: break;
    }
    if (c >= 0x20 && c != 0x7F) vt_imprimir(v, c);
}

/* El tamaño cambia a nc x nl: si el cursor quedaría fuera, el texto sube (con historial en la pantalla normal). */
static TL_USADA void vt_redimensionar(vt_t *v, int nc, int nl)
{
    if (v->lin >= nl) {
        int d = v->lin - nl + 1;
        if (!v->alt && v->hist) for (int k = 0; k < d; k++) v->hist(v->u, k);
        memmove(&VT_GR(v, 0, 0), &VT_GR(v, d, 0), (size_t)(TL_LINS_MAX - d) * VT_STRIDE);
        memmove(&VT_AT(v, 0, 0), &VT_AT(v, d, 0), (size_t)(TL_LINS_MAX - d) * VT_STRIDE * 2);
        memset(&VT_GR(v, TL_LINS_MAX - d, 0), ' ', (size_t)d * VT_STRIDE);
        memset(&VT_AT(v, TL_LINS_MAX - d, 0), 0, (size_t)d * VT_STRIDE * 2);
        v->lin -= d;
    }
    for (int l = nl; l < TL_LINS_MAX; l++) {
        memset(&VT_GR(v, l, 0), ' ', VT_STRIDE); memset(&VT_AT(v, l, 0), 0, VT_STRIDE * 2);
        if (v->g2) { memset(&v->g2[l * VT_STRIDE], ' ', VT_STRIDE); memset(&v->a2[l * VT_STRIDE], 0, VT_STRIDE * 2); }
    }
    v->cols = nc; v->lins = nl;
    if (v->col >= nc) v->col = nc - 1;
    v->top = 0; v->bot = nl - 1; v->pend = false;
}

#endif
