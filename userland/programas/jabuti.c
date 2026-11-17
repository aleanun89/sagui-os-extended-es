// SPDX-License-Identifier: GPL-3.0-or-later
/* jabuti [proyecto.jbt] - programación con bloques para niños (D377, mejora 137).
 *
 * Una aplicación ORIGINAL de SaguiOS, inspirada en la idea de los lenguajes de bloques para aprender
 * (no es un porte de ninguno: ni su código, ni su gato, ni su nombre, ni sus gráficos). Tres zonas:
 * la paleta de bloques por categorías, el área de programa (se arrastran y se encajan los bloques) y el
 * escenario con los personajes. El programa de cada personaje corre en un intérprete propio (userland/jabuti/).
 *
 * Modos sin ventana (pruebas):
 *   jabuti --ejecutar proyecto.jbt [--cuadros N] [--tecla NOMBRE@CUADRO] [--respuesta TEXTO] [--guardar salida.jbt]
 *   jabuti --abrir proyecto.jbt [--guardar salida.jbt] [--texto]       (valida; --texto lo escribe legible) */
#include "sagui.h"
#include "gui.h"
#include "imagem.h"
#include "jb.h"

extern const unsigned char fonte_sagui[256][16];
extern const char *const jb_nombre_cat[NCAT];
extern const uint32_t jb_color_cat[NCAT];

/* ============================================================ dibujo con recorte */

static jan_t *j;
static int cx0, cy0, cx1, cy1;                      /* recorte actual */

static void recorte(int x, int y, int w, int h) { cx0 = x; cy0 = y; cx1 = x + w; cy1 = y + h; if (cx0 < 0) cx0 = 0; if (cy0 < 0) cy0 = 0; if (cx1 > j->larg) cx1 = j->larg; if (cy1 > j->alt) cy1 = j->alt; }
static void rc(int x, int y, int w, int h, uint32_t c)
{
    int x1 = x + w, y1 = y + h;
    if (x < cx0) x = cx0;
    if (y < cy0) y = cy0;
    if (x1 > cx1) x1 = cx1;
    if (y1 > cy1) y1 = cy1;
    for (int yy = y; yy < y1; yy++) { uint32_t *p = j->px + yy * j->larg; for (int xx = x; xx < x1; xx++) p[xx] = c; }
}
static void pt(int x, int y, uint32_t c) { if (x >= cx0 && y >= cy0 && x < cx1 && y < cy1) j->px[y * j->larg + x] = c; }
static void marco(int x, int y, int w, int h, uint32_t c) { rc(x, y, w, 1, c); rc(x, y + h - 1, w, 1, c); rc(x, y, 1, h, c); rc(x + w - 1, y, 1, h, c); }
static uint32_t oscuro(uint32_t c) { return (((c >> 16) & 255) * 3 / 4) << 16 | (((c >> 8) & 255) * 3 / 4) << 8 | ((c & 255) * 3 / 4); }
static uint32_t claro(uint32_t c) { return ((255 - ((255 - ((c >> 16) & 255)) * 2 / 3)) << 16) | ((255 - ((255 - ((c >> 8) & 255)) * 2 / 3)) << 8) | (255 - ((255 - (c & 255)) * 2 / 3)); }
static void texto(int x, int y, const char *t, uint32_t c, int fs)
{
    for (; *t; t++, x += 8 * fs) {
        if (x + 8 * fs <= cx0 || x >= cx1) continue;
        const unsigned char *g = fonte_sagui[(uint8_t)*t];
        for (int l = 0; l < 16; l++)
            for (int k = 0; k < 8; k++)
                if (g[l] & (0x80 >> k)) { if (fs == 1) pt(x + k, y + l, c); else rc(x + k * 2, y + l * 2, 2, 2, c); }
    }
}
static int tw(const char *t, int fs) { return (int)tam_texto(t) * 8 * fs; }
/* formas: 0 rectángulo, 1 redondeada (números), 2 hexágono (condiciones) */
static void forma(int x, int y, int w, int h, uint32_t c, int tipo)
{
    int r = h / 2;
    for (int row = 0; row < h; row++) {
        int ins = 0;
        if (tipo == 1) { int d = row < r ? r - row : row - r + 1; ins = r - jb_raiz((r * r - (d - 1) * (d - 1)) > 0 ? r * r - (d - 1) * (d - 1) : 0); if (ins < 0) ins = 0; }
        else if (tipo == 2) ins = row < r ? r - row : row - r + 1 - 1;
        if (tipo == 2 && ins > r) ins = r;
        rc(x + ins, y + row, w - 2 * ins, 1, c);
    }
}
static void forma_borde(int x, int y, int w, int h, uint32_t c, uint32_t borde, int tipo)
{
    forma(x, y, w, h, borde, tipo);
    forma(x + 1, y + 1, w - 2, h - 2, c, tipo);
}

/* ============================================================ métricas por zoom */

typedef struct { int fs, rh, sh, pad, gap, ind, foot, hat, minb; } met_t;
static const met_t MET[3] = {
    { 1, 26, 20, 8, 5, 14, 12, 10, 14 },
    { 1, 32, 24, 10, 6, 18, 14, 12, 18 },
    { 2, 52, 40, 14, 10, 24, 20, 18, 26 },
};
static const int ZF[3] = { 81, 100, 162 };    /* escala de las posiciones de los guiones */
static const met_t *M = &MET[1];
static int dir_visible(int d) { d %= 360000; if (d > 180000) d -= 360000; if (d <= -180000) d += 360000; return d; }
#define EB (M->fs == 2 ? 36 : 22)         /* alto de la barra del «si no» */
static int zoom = 1;

/* ============================================================ estado */

enum { MODO_PROG, MODO_TRAJES, MODO_FONDOS };
static int modo = MODO_PROG;
static int cat_sel = C_MOV;
static int cur;                                     /* personaje elegido */
static char ruta[200];
static bool modificado;
static bool corriendo;
static bool a_pantalla_completa;
static int esc_pref = 1;                            /* 0..3: 160, 240, 360, 480 */
static const int ESC_TAM[4] = { 160, 240, 360, 480 };
static int pal_scroll, area_sx, area_sy;

/* geometría */
static int W, H;
#define TOPH 46
#define TABH 28
#define SBH 22
#define CATW 88
#define PALW 236
static int ax, ay, aw, ah;                          /* área de programa */
static int px_, py_, pw_, ph_;                      /* paleta */
static int ex, ey, evl, eva;                        /* escenario (vista) */
static int spx, spy, spw, sph;                      /* panel de personajes */
static int edx, edy, edw, edh;                      /* editor de trajes/fondos */

/* ============================================================ maquetación y dibujo de bloques */

typedef struct { int x, y, w, h; uint8_t t; blk_t *b; int i; int g; } hit_t;
enum { H_BLK, H_SLOT, H_MENU, H_BTN, H_DROP };
#define MAXHIT 1600
static hit_t hits_a[MAXHIT], hits_p[MAXHIT];
static int nha, nhp;
static hit_t *hits;
static int *nh;
typedef struct { int x, y; uint8_t t; blk_t *b; int g; } conn_t;
enum { CN_SIG, CN_C1, CN_C2, CN_TOP };
static conn_t conns[MAXHIT];
static int ncn;
static bool grabando_conns;

static void hit(int x, int y, int w, int h, int t, blk_t *b, int i, int g)
{
    if (*nh >= MAXHIT) return;
    hits[(*nh)++] = (hit_t){ x, y, w, h, (uint8_t)t, b, i, g };
}
static void conn(int x, int y, int t, blk_t *b, int g)
{
    if (!grabando_conns || ncn >= MAXHIT) return;
    conns[ncn++] = (conn_t){ x, y, (uint8_t)t, b, g };
}

static const char *traducir_dato(const char *s) { return idioma_traducir_sl(s); }
/* el texto que se ve en un menú desplegable para el valor guardado */
static const char *texto_menu(int menu, const char *v, char *buf, int max)
{
    (void)menu;
    if (compara_texto(v, "raton") == 0) return idioma_traducir("puntero del ratón");
    if (compara_texto(v, "azar") == 0) return idioma_traducir("posición al azar");
    if (compara_texto(v, "borde") == 0) return idioma_traducir("borde");
    switch (menu) {
    case M_TECLA: case M_MENSAJE: case M_COLOR: case M_DETENER: case M_SONIDO: case M_ROT: return traducir_dato(v);
    default: copia_texto(buf, v, (size_t)max); return buf;
    }
}

static const char *rotulo_de(int op) { return idioma_traducir(jb_ops[op].rotulo); }

static struct { bool on; blk_t *b; int i; char orig[40]; bool cambio; } ed_;
typedef struct { int w, h; } dim_t;
static void medir(blk_t *b);

static dim_t ranura_dim(blk_t *b, int i, char tipo)
{
    dim_t d;
    int fs = M->fs, cw = 8 * fs;
    if (b->a[i].rep) { medir(b->a[i].rep); d.w = b->a[i].rep->w; d.h = b->a[i].rep->h; return d; }
    int sp = fs == 2 ? 10 : 6;
    char buf[48];
    const char *t = b->a[i].t;
    if (tipo == '@') {
        const char *s = b->op == O_VAR ? b->a[i].t : texto_menu(jb_ops[b->op].menu, t, buf, sizeof buf);
        d.w = tw(s, fs) + 2 * sp + 12 * fs; d.h = M->sh;
        return d;
    }
    int n = (int)tam_texto(t) + (ed_.on && ed_.b == b && ed_.i == i ? 1 : 0);
    int minimo = tipo == '?' ? 4 : tipo == '#' ? 2 : 3;
    d.w = (n > minimo ? n : minimo) * cw + 2 * sp;
    d.h = M->sh;
    return d;
}

/* tamaño de un bloque (con sus bloques internos y reporteros); los siguientes los suma quien lo apila */
static void medir_pila(blk_t *b, int *w, int *h)
{
    *w = 0; *h = 0;
    for (; b; b = b->sig) { medir(b); if (b->w > *w) *w = b->w; *h += b->h; }
}
static void medir(blk_t *b)
{
    const op_t *o = &jb_ops[b->op];
    int fs = M->fs, cw = 8 * fs;
    const char *r = rotulo_de(b->op);
    int x = M->pad, hmax = M->rh;
    int si = 0;
    char piece[64];
    int np = 0;
    for (const char *c = r;; c++) {
        bool slot = *c == '#' || *c == '$' || *c == '^' || *c == '@';
        if (slot || !*c) {
            piece[np] = 0;
            int a = 0, z = np;
            while (a < z && piece[a] == ' ') a++;
            while (z > a && piece[z - 1] == ' ') z--;
            if (z > a) x += (z - a) * cw + M->gap;
            np = 0;
            if (!*c) break;
            dim_t d = ranura_dim(b, si, *c == '^' ? '?' : *c);
            x += d.w + M->gap;
            if (d.h + 8 > hmax) hmax = d.h + 8;
            si++;
        } else if (np < (int)sizeof piece - 1) piece[np++] = *c;
    }
    x += M->pad - M->gap;
    if (o->tipo == K_REP || o->tipo == K_BOOL) {
        b->wr = (int16_t)x; b->hr = (int16_t)(hmax - 4);
        b->w = b->wr; b->h = b->hr;
        return;
    }
    b->wr = (int16_t)x; b->hr = (int16_t)hmax;
    if (o->tipo == K_C || o->tipo == K_CAP) {
        int w1, h1, w2 = 0, h2 = 0;
        medir_pila(b->c1, &w1, &h1);
        if (h1 < M->minb) h1 = M->minb;
        b->hb1 = (int16_t)h1;
        int w = x, h = hmax + h1 + M->foot;
        if (M->ind + w1 > w) w = M->ind + w1;
        if (b->op == O_SI_NO) {
            medir_pila(b->c2, &w2, &h2);
            if (h2 < M->minb) h2 = M->minb;
            b->hb2 = (int16_t)h2;
            h += EB + h2;
            if (M->ind + w2 > w) w = M->ind + w2;
        }
        if (o->tipo == K_CAP && b->op != O_SIEMPRE) { b->w = (int16_t)x; b->h = b->hr; return; }   /* «detener» no tiene cuerpo */
        b->w = (int16_t)w; b->h = (int16_t)h;
        return;
    }
    b->w = (int16_t)x; b->h = b->hr;
}

static void dibujar_pila(blk_t *b, int x, int y, int g, bool pal);

static uint32_t color_de(const blk_t *b) { return jb_color_cat[jb_ops[b->op].cat]; }

/* dibuja la fila de rótulo de b en (x, y) con la altura h */
static void fila(blk_t *b, int x, int y, int h, int g, bool pal, bool soy_rep)
{
    uint32_t col = color_de(b);
    int fs = M->fs, cw = 8 * fs;
    const char *r = rotulo_de(b->op);
    int cxp = x + M->pad;
    int si = 0;
    char piece[64];
    int np = 0;
    for (const char *c = r;; c++) {
        bool slot = *c == '#' || *c == '$' || *c == '^' || *c == '@';
        if (slot || !*c) {
            piece[np] = 0;
            int a = 0, z = np;
            while (a < z && piece[a] == ' ') a++;
            while (z > a && piece[z - 1] == ' ') z--;
            if (z > a) {
                char tmp[64];
                int n = z - a;
                memcpy(tmp, piece + a, (size_t)n); tmp[n] = 0;
                texto(cxp, y + (h - 16 * fs) / 2, tmp, 0xFFFFFF, fs);
                cxp += n * cw + M->gap;
            }
            np = 0;
            if (!*c) break;
            dim_t d = ranura_dim(b, si, *c == '^' ? '?' : *c);
            int sy = y + (h - d.h) / 2;
            if (b->a[si].rep) {
                blk_t *q = b->a[si].rep;
                int qy = y + (h - q->h) / 2;
                int tipo = jb_ops[q->op].tipo;
                uint32_t qc = color_de(q);
                forma_borde(cxp, qy, q->w, q->h, qc, oscuro(qc), tipo == K_BOOL ? 2 : 1);
                hit(cxp, qy, q->w, q->h, H_DROP, b, si, g);     /* antes que lo de dentro: lo de dentro manda */
                hit(cxp, qy, q->w, q->h, H_BLK, q, -1, g);
                fila(q, cxp, qy, q->h, g, pal, true);
            } else if (*c == '@') {
                char buf[48];
                const char *s = b->op == O_VAR ? b->a[si].t : texto_menu(jb_ops[b->op].menu, b->a[si].t, buf, sizeof buf);
                if (b->op == O_VAR && soy_rep) { /* la variable es toda ella un reportero: sin desplegable */
                    texto(cxp, sy + (d.h - 16 * fs) / 2, s, 0xFFFFFF, fs);
                } else {
                    forma_borde(cxp, sy, d.w, d.h, claro(col) == col ? col : oscuro(col), oscuro(col), 0);
                    rc(cxp + 1, sy + 1, d.w - 2, d.h - 2, oscuro(col));
                    texto(cxp + (fs == 2 ? 10 : 6), sy + (d.h - 16 * fs) / 2, s, 0xFFFFFF, fs);
                    g_flecha(j, cxp + d.w - 10 * fs, sy + d.h / 2 - 2, 1, 0xFFFFFF);
                    hit(cxp, sy, d.w, d.h, H_MENU, b, si, g);
                }
            } else if (*c == '^') {
                forma_borde(cxp, sy, d.w, d.h, oscuro(col), oscuro(oscuro(col)), 2);
                hit(cxp, sy, d.w, d.h, H_DROP, b, si, g);
            } else {
                bool editando = ed_.on && ed_.b == b && ed_.i == si;
                forma_borde(cxp, sy, d.w, d.h, 0xFFFFFF, editando ? 0x2A6FD8 : oscuro(col), *c == '#' ? 1 : 0);
                if (editando) { char tmp[48]; formatar_texto(tmp, sizeof tmp, "%s_", b->a[si].t); texto(cxp + (fs == 2 ? 10 : 6), sy + (d.h - 16 * fs) / 2, tmp, 0x1E2328, fs); }
                else texto(cxp + (fs == 2 ? 10 : 6), sy + (d.h - 16 * fs) / 2, b->a[si].t, 0x1E2328, fs);
                hit(cxp, sy, d.w, d.h, H_SLOT, b, si, g);
                hit(cxp, sy, d.w, d.h, H_DROP, b, si, g);
            }
            cxp += d.w + M->gap;
            si++;
        } else if (np < (int)sizeof piece - 1) piece[np++] = *c;
    }
}

static void dibujar_bloque(blk_t *b, int x, int y, int g, bool pal)
{
    const op_t *o = &jb_ops[b->op];
    uint32_t col = color_de(b), bor = oscuro(col);
    if (o->tipo == K_REP || o->tipo == K_BOOL) {
        forma_borde(x, y, b->w, b->h, col, bor, o->tipo == K_BOOL ? 2 : 1);
        hit(x, y, b->w, b->h, H_BLK, b, -1, g);
        fila(b, x, y, b->h, g, pal, true);
        return;
    }
    int hr = b->hr;
    bool es_c = o->tipo == K_C || (o->tipo == K_CAP && b->op == O_SIEMPRE);
    int ancho = b->w;
    if (o->tipo == K_HAT) {
        /* la cabeza del sombrero: un casquete redondeado encima de la fila */
        int hc = M->hat;
        for (int k = 0; k < hc; k++) { int ins = (hc - k - 1) * 3 / 2; rc(x + ins, y - hc + k, ancho * 3 / 5 - ins, 1, col); }
    }
    rc(x, y, ancho, hr, col);
    rc(x, y, ancho, 1, claro(col)); rc(x, y, 1, hr, claro(col));
    rc(x, y + hr - 1, ancho, 1, bor); rc(x + ancho - 1, y, 1, hr, bor);
    if (o->tipo != K_HAT) { rc(x + 10, y, 20, 3, bor); }      /* la muesca de arriba */
    hit(x, y, ancho, hr, H_BLK, b, -1, g);
    fila(b, x, y, hr, g, pal, false);
    if (es_c) {
        int ind = M->ind;
        rc(x, y + hr, ind, b->hb1, col); rc(x, y + hr, 1, b->hb1, claro(col)); rc(x + ind - 1, y + hr, 1, b->hb1, bor);
        hit(x, y + hr, ind, b->hb1, H_BLK, b, -1, g);
        conn(x + ind, y + hr, CN_C1, b, g);
        if (b->c1) dibujar_pila(b->c1, x + ind, y + hr, g, pal);
        int yf = y + hr + b->hb1;
        if (b->op == O_SI_NO) {
            rc(x, yf, ancho, M->foot, col); rc(x, yf + M->foot - 1, ancho, 1, bor); rc(x, yf, 1, M->foot, claro(col));
            int eb = EB;
            rc(x, yf, ancho, eb, col); rc(x, yf, ancho, 1, bor);
            texto(x + M->pad, yf + (eb - 16) / 2, _("si no"), 0xFFFFFF, 1);
            hit(x, yf, ancho, eb, H_BLK, b, -1, g);
            rc(x, yf + eb, ind, b->hb2, col); rc(x, yf + eb, 1, b->hb2, claro(col)); rc(x + ind - 1, yf + eb, 1, b->hb2, bor);
            hit(x, yf + eb, ind, b->hb2, H_BLK, b, -1, g);
            conn(x + ind, yf + eb, CN_C2, b, g);
            if (b->c2) dibujar_pila(b->c2, x + ind, yf + eb, g, pal);
            yf += eb + b->hb2;
        }
        rc(x, yf, ancho, M->foot, col); rc(x, yf, 1, M->foot, claro(col)); rc(x, yf + M->foot - 1, ancho, 1, bor); rc(x + ancho - 1, yf, 1, M->foot, bor);
        rc(x + 10, yf + M->foot, 20, 3, col);
        hit(x, yf, ancho, M->foot, H_BLK, b, -1, g);
        return;
    }
    if (o->tipo != K_CAP) rc(x + 10, y + hr, 20, 3, col);        /* la pestaña de abajo */
}

static void dibujar_pila(blk_t *b, int x, int y, int g, bool pal)
{
    for (; b; b = b->sig) {
        dibujar_bloque(b, x, y, g, pal);
        if (jb_ops[b->op].tipo != K_CAP) conn(x, y + b->h, CN_SIG, b, g);
        y += b->h;
    }
}

/* ============================================================ edición de una ranura y menús */

static void edicion_terminar(bool aceptar)
{
    if (!ed_.on) return;
    if (!aceptar) copia_texto(ed_.b->a[ed_.i].t, ed_.orig, sizeof ed_.b->a[ed_.i].t);
    if (!aceptar || !ed_.cambio) { if (!ed_.cambio) jb_hist_descartar(); }
    else { modificado = true; registrar("jabuti: editado %s %d = %s\n", jb_ops[ed_.b->op].clave, ed_.i, ed_.b->a[ed_.i].t); }
    ed_.on = false;
}

static pers_t *pers_cur(void) { return cur >= 0 && cur < jb_p.npers ? jb_p.pers[cur] : NULL; }

/* ---- bloques de la paleta ---- */
#define MAXPAL 40
static blk_t *pal_blks[MAXPAL];
static int npal, pal_alto;
enum { B_VAR_NUEVA = 1, B_LISTA_NUEVA, B_VAR_BORRAR };

static void ajustar_defaults(blk_t *b)
{
    pers_t *p = pers_cur();
    int n = jb_nranuras(b->op);
    for (int i = 0; i < n && i < 3; i++) {
        char t = jb_tipo_ranura(b->op, i);
        if (t == '$' && b->a[i].t[0]) copia_texto(b->a[i].t, idioma_traducir_sl(b->a[i].t), sizeof b->a[i].t);
        if (t != '@' || b->a[i].t[0]) continue;
        switch (jb_ops[b->op].menu) {
        case M_TRAJE: if (p && p->ntrajes) copia_texto(b->a[i].t, p->trajes[p->traje]->nombre, sizeof b->a[i].t); break;
        case M_FONDO: if (jb_p.nfondos) copia_texto(b->a[i].t, jb_p.fondos[jb_p.fondo]->nombre, sizeof b->a[i].t); break;
        case M_VAR: if (jb_p.nvars) copia_texto(b->a[i].t, jb_p.vars[0].nombre, sizeof b->a[i].t); break;
        case M_LISTA: if (jb_p.nlistas) copia_texto(b->a[i].t, jb_p.listas[0].nombre, sizeof b->a[i].t); break;
        default: break;
        }
    }
}

static void pal_reconstruir(void)
{
    for (int i = 0; i < npal; i++) jb_blk_liberar(pal_blks[i]);
    npal = 0;
    for (int op = 0; op < O_FIN && npal < MAXPAL; op++) {
        if (jb_ops[op].cat != cat_sel) continue;
        if (op == O_VAR) {
            for (int v = 0; v < jb_p.nvars && npal < MAXPAL; v++) {
                blk_t *b = jb_blk_nuevo(O_VAR);
                if (!b) continue;
                copia_texto(b->a[0].t, jb_p.vars[v].nombre, sizeof b->a[0].t);
                pal_blks[npal++] = b;
            }
            continue;
        }
        blk_t *b = jb_blk_nuevo(op);
        if (!b) continue;
        ajustar_defaults(b);
        pal_blks[npal++] = b;
    }
}

static void boton3d(int x, int y, int w, int h, const char *t, bool hundido, uint32_t fondo)
{
    rc(x, y, w, h, hundido ? oscuro(fondo) : fondo);
    rc(x, y, w, 1, claro(fondo)); rc(x, y, 1, h, claro(fondo));
    rc(x, y + h - 1, w, 1, oscuro(fondo)); rc(x + w - 1, y, 1, h, oscuro(fondo));
    marco(x - 1, y - 1, w + 2, h + 2, 0x5A6470);
    int tx = x + (w - tw(t, 1)) / 2;
    texto(tx, y + (h - 16) / 2 + (hundido ? 1 : 0), t, fondo == 0xE2E6EA || fondo == 0xFFFFFF || fondo == 0xF4F4F0 ? 0x1E2328 : 0xFFFFFF, 1);
}

static void dibujar_paleta(void)
{
    if (modo != MODO_PROG) return;
    recorte(px_, py_, pw_, ph_);
    rc(px_, py_, pw_, ph_, 0xFFFFFF);
    rc(px_ + pw_ - 1, py_, 1, ph_, 0xB8C0C8);
    M = &MET[zoom > 1 ? 1 : zoom];
    hits = hits_p; nh = &nhp; nhp = 0;
    grabando_conns = false;
    int y = py_ + 8 - pal_scroll;
    const int x = px_ + 10;
    if (cat_sel == C_VARS) {
        boton3d(x, y, pw_ - 24, 28, _("Crear una variable"), false, 0xE2E6EA);
        hit(x, y, pw_ - 24, 28, H_BTN, NULL, B_VAR_NUEVA, 0);
        y += 34;
        boton3d(x, y, pw_ - 24, 28, _("Crear una lista"), false, 0xE2E6EA);
        hit(x, y, pw_ - 24, 28, H_BTN, NULL, B_LISTA_NUEVA, 0);
        y += 34;
    }
    for (int i = 0; i < npal; i++) {
        blk_t *b = pal_blks[i];
        medir(b);
        int hat = jb_ops[b->op].tipo == K_HAT ? M->hat : 0;
        y += hat;
        dibujar_bloque(b, x, y, -1, true);
        y += b->h + 10;
    }
    pal_alto = y + pal_scroll - py_ + 8;
    recorte(0, 0, W, H);
}

/* ---- el área de programa ---- */

static hit_t hits_x[MAXHIT];
static int nhx;
static void guion_pos(const guion_t *g, int *x, int *y)
{
    *x = ax + g->x * ZF[zoom] / 100 - area_sx;
    *y = ay + g->y * ZF[zoom] / 100 - area_sy;
}

static struct {
    bool on, rep, nuevo, hay_snap;
    blk_t *b;
    int ox, oy, x, y, w, h;
    conn_t snap;
    hit_t drop;
} dr;

static void dibujar_area(void)
{
    if (modo != MODO_PROG) return;
    recorte(ax, ay, aw, ah);
    rc(ax, ay, aw, ah, 0xF7F8F4);
    for (int yy = ay - area_sy % 24; yy < ay + ah; yy += 24)
        for (int xx = ax - area_sx % 24; xx < ax + aw; xx += 24) pt(xx, yy, 0xE0E4E0);
    M = &MET[zoom];
    hits = hits_a; nh = &nha; nha = 0; ncn = 0;
    grabando_conns = true;
    pers_t *p = pers_cur();
    if (p) {
        for (int g = 0; g < p->nguiones; g++) {
            int x, y;
            guion_pos(&p->guiones[g], &x, &y);
            int w, h;
            medir_pila(p->guiones[g].top, &w, &h);
            if (!p->guiones[g].top) continue;
            if (x > ax + aw || y > ay + ah || x + w < ax || y + h < ay - 40) {
                /* fuera de la vista: no se dibuja (se registran igual las conexiones de arriba) */
                continue;
            }
            if (jb_ops[p->guiones[g].top->op].tipo != K_HAT) conn(x, y, CN_TOP, p->guiones[g].top, g);
            dibujar_pila(p->guiones[g].top, x, y, g, false);
        }
        if (p->nguiones == 0 && !dr.on) {
            const char *t = _("Arrastra bloques aquí");
            texto(ax + (aw - tw(t, 1)) / 2, ay + ah / 2 - 8, t, 0x8A939C, 1);
        }
    }
    recorte(0, 0, W, H);
}

/* ---- quitar un bloque del árbol (para arrastrarlo) ---- */
static bool quitar_rep(blk_t *r, blk_t *obj);
static bool quitar(blk_t **enlace, blk_t *obj)
{
    for (; *enlace; enlace = &(*enlace)->sig) {
        blk_t *q = *enlace;
        if (q == obj) { *enlace = NULL; return true; }
        if (quitar(&q->c1, obj) || quitar(&q->c2, obj)) return true;
        if (quitar_rep(q, obj)) return true;
    }
    return false;
}
static bool quitar_rep(blk_t *q, blk_t *obj)
{
    for (int i = 0; i < 3; i++) if (q->a[i].rep) {
        if (q->a[i].rep == obj) { q->a[i].rep = NULL; return true; }
        if (quitar_rep(q->a[i].rep, obj)) return true;
    }
    return false;
}
static bool desprender(pers_t *p, blk_t *obj)
{
    for (int g = 0; g < p->nguiones; g++) {
        if (p->guiones[g].top == obj) {
            for (int k = g; k + 1 < p->nguiones; k++) p->guiones[k] = p->guiones[k + 1];
            p->nguiones--;
            return true;
        }
        if (quitar(&p->guiones[g].top, obj)) return true;
    }
    return false;
}

/* ---- el arrastre ---- */
static int dist2(int x0, int y0, int x1, int y1) { return (x0 - x1) * (x0 - x1) + (y0 - y1) * (y0 - y1); }

static bool buscar_snap(void)
{
    dr.hay_snap = false;
    pers_t *p = pers_cur();
    if (!p || !dr.b) return false;
    if (dr.rep) {
        int mx = dr.x + dr.ox, my = dr.y + dr.oy;
        int tipo = jb_ops[dr.b->op].tipo;
        for (int i = nha - 1; i >= 0; i--) {
            hit_t *h = &hits_a[i];
            if (h->t != H_DROP) continue;
            if (mx < h->x - 4 || mx > h->x + h->w + 4 || my < h->y - 4 || my > h->y + h->h + 4) continue;
            char st = jb_tipo_ranura(h->b->op, h->i);
            if (tipo == K_BOOL ? st != '?' : !(st == '#' || st == '$')) continue;
            dr.drop = *h; dr.hay_snap = true;
            return true;
        }
        return false;
    }
    blk_t *ultimo = jb_ultimo(dr.b);
    bool hat = jb_ops[dr.b->op].tipo == K_HAT, cap = jb_ops[ultimo->op].tipo == K_CAP && !(ultimo->op == O_SIEMPRE && false);
    int mejor = 40 * 40;
    for (int i = 0; i < ncn; i++) {
        conn_t *c = &conns[i];
        int d;
        if (c->t == CN_TOP) {
            if (cap || jb_ops[c->b->op].tipo == K_HAT) continue;
            d = dist2(dr.x, dr.y + dr.h, c->x, c->y);
        } else {
            if (hat) continue;
            if (cap) {
                blk_t *sig = c->t == CN_SIG ? c->b->sig : c->t == CN_C1 ? c->b->c1 : c->b->c2;
                if (sig) continue;
            }
            d = dist2(dr.x, dr.y, c->x, c->y);
        }
        if (d < mejor) { mejor = d; dr.snap = *c; dr.hay_snap = true; }
    }
    return dr.hay_snap;
}

static void dibujar_arrastre(void)
{
    if (!dr.on) return;
    recorte(px_, ay, ax + aw - px_, ah);
    M = &MET[zoom];
    hits = hits_x; nh = &nhx; nhx = 0; grabando_conns = false;
    if (dr.hay_snap) {
        if (dr.rep) { marco(dr.drop.x - 2, dr.drop.y - 2, dr.drop.w + 4, dr.drop.h + 4, 0xFFFFFF); marco(dr.drop.x - 3, dr.drop.y - 3, dr.drop.w + 6, dr.drop.h + 6, 0x1E2328); }
        else {
            int sx_ = dr.snap.x, sy_ = dr.snap.y;
            if (dr.snap.t == CN_TOP) rc(sx_, sy_ - 3, 60, 6, 0xFFFFFF);
            else rc(sx_, sy_ - 3, 60, 6, 0xFFFFFF);
            marco(sx_ - 1, sy_ - 4, 62, 8, 0x1E2328);
        }
    }
    if (dr.rep) {
        uint32_t qc = color_de(dr.b);
        forma_borde(dr.x, dr.y, dr.b->w, dr.b->h, qc, oscuro(qc), jb_ops[dr.b->op].tipo == K_BOOL ? 2 : 1);
        fila(dr.b, dr.x, dr.y, dr.b->h, -1, false, true);
    } else dibujar_pila(dr.b, dr.x, dr.y, -1, false);
    recorte(0, 0, W, H);
}

/* ============================================================ barra, pestañas, categorías, escenario, personajes */

typedef struct { int x, y, w, h; const char *id; } boton_t;
enum { BT_BANDERA, BT_PARAR, BT_NUEVO, BT_ABRIR, BT_GUARDAR, BT_DESHACER, BT_REHACER, BT_EJEMPLOS, BT_ZMENOS, BT_ZMAS, BT_AYUDA, BT_PANTALLA, BT_TAMANO, BT_PERSONAJE, BT_N };
static boton_t b_[BT_N];
static char estado_txt[200];

static void dizer(const char *t) { copia_texto(estado_txt, t, sizeof estado_txt); }

static void calcular_layout(void)
{
    W = j->larg; H = j->alt;
    if (a_pantalla_completa) {
        int vl = W < (H - SBH) * 4 / 3 ? W : (H - SBH) * 4 / 3;
        evl = vl & ~3; eva = evl * 3 / 4;
        ex = (W - evl) / 2; ey = (H - SBH - eva) / 2;
        return;
    }
    int disponible = W - CATW - PALW - 250 - 12;
    int k = esc_pref;
    while (k > 0 && ESC_TAM[k] > disponible) k--;
    if (W < 700) k = 0;
    evl = ESC_TAM[k]; eva = evl * 3 / 4;
    ex = W - evl - 8; ey = TOPH + 6;
    spx = ex; spy = ey + eva + 6; spw = evl; sph = H - SBH - spy - 4;
    px_ = CATW; py_ = TOPH + TABH; pw_ = PALW; ph_ = H - SBH - py_;
    ax = px_ + pw_; ay = py_; aw = ex - 8 - ax; ah = ph_;
    edx = 0; edy = TOPH + TABH; edw = ex - 8; edh = H - SBH - edy;
    /* barra de herramientas */
    static const char *const ids[BT_PANTALLA] = { "bandera", "parar", "nuevo", "abrir", "guardar", "deshacer", "rehacer", "ejemplos", "zoom-", "zoom+", "ayuda" };
    const char *rot[BT_PANTALLA] = { "", "", _("Nuevo"), _("Abrir"), _("Guardar"), _("Deshacer"), _("Rehacer"), _("Ejemplos"), "A-", "A+", _("Ayuda") };
    int x = 6;
    for (int i = 0; i < BT_PANTALLA; i++) {
        int w = i < 2 ? 44 : (int)tam_texto(rot[i]) * 8 + 18;
        if (i == BT_NUEVO || i == BT_DESHACER || i == BT_EJEMPLOS || i == BT_ZMENOS) x += 10;
        if (i == BT_AYUDA) x += 10;
        b_[i] = (boton_t){ x, 6, w, 34, ids[i] };
        x += w + 5;
    }
    b_[BT_PANTALLA] = (boton_t){ spx, spy, spw / 2 - 2, 26, "pantalla" };
    b_[BT_TAMANO] = (boton_t){ spx + spw / 2 + 2, spy, spw / 2 - 2, 26, "tamano" };
    b_[BT_PERSONAJE] = (boton_t){ spx, spy + 30, spw, 20, "personaje" };
}

static void titulo_actualizar(void)
{
    char t[260];
    const char *n = ruta[0] ? nome_base(ruta) : _("Sin título");
    formatar_texto(t, sizeof t, "%s%s - Jabuti", n, modificado ? " *" : "");
    titulo_janela(j->id, t);
}

static void dibujar_barra(void)
{
    recorte(0, 0, W, TOPH);
    rc(0, 0, W, TOPH, 0xDDE3E8);
    rc(0, TOPH - 1, W, 1, 0xB0BAC4);
    const char *rot[BT_PANTALLA] = { "", "", _("Nuevo"), _("Abrir"), _("Guardar"), _("Deshacer"), _("Rehacer"), _("Ejemplos"), "A-", "A+", _("Ayuda") };
    for (int i = 0; i < BT_PANTALLA; i++) {
        boton_t *b = &b_[i];
        if (i == BT_BANDERA) {
            boton3d(b->x, b->y, b->w, b->h, "", false, 0xE2E6EA);
            rc(b->x + 14, b->y + 6, 3, 22, 0x6E553C);
            rc(b->x + 17, b->y + 7, 16, 11, 0x1AAE55); marco(b->x + 17, b->y + 7, 16, 11, 0x0B7A3E);
        } else if (i == BT_PARAR) {
            boton3d(b->x, b->y, b->w, b->h, "", false, 0xE2E6EA);
            rc(b->x + 12, b->y + 7, 20, 20, 0xD83A30); marco(b->x + 12, b->y + 7, 20, 20, 0x7A1218);
        } else {
            bool des = (i == BT_DESHACER && !jb_hist_hay_deshacer()) || (i == BT_REHACER && !jb_hist_hay_rehacer());
            boton3d(b->x, b->y, b->w, b->h, rot[i], false, 0xE2E6EA);
            if (des) { for (int yy = b->y; yy < b->y + b->h; yy += 2) rc(b->x, yy, b->w, 1, 0xE2E6EA); }
        }
    }
    if (corriendo) rc(b_[BT_BANDERA].x - 1, b_[BT_BANDERA].y + b_[BT_BANDERA].h + 1, b_[BT_BANDERA].w + 2, 3, 0x1AAE55);
    recorte(0, 0, W, H);
}

static void dibujar_pestanas(void)
{
    recorte(0, TOPH, W, TABH);
    rc(0, TOPH, W, TABH, 0xE8ECEF);
    const char *nom[3] = { _("Programa"), _("Trajes"), _("Fondos") };
    int x = 6;
    for (int i = 0; i < 3; i++) {
        int w = (int)tam_texto(nom[i]) * 8 + 28;
        rc(x, TOPH + 3, w, TABH - 3, i == modo ? 0xFFFFFF : 0xD5DBE0);
        marco(x, TOPH + 3, w, TABH - 3, 0xB0BAC4);
        if (i == modo) rc(x + 1, TOPH + TABH - 1, w - 2, 1, 0xFFFFFF);
        texto(x + 14, TOPH + 7, nom[i], 0x1E2328, 1);
        x += w + 3;
    }
    recorte(0, 0, W, H);
}
static int pestana_en(int mx, int my)
{
    if (my < TOPH || my >= TOPH + TABH) return -1;
    const char *nom[3] = { _("Programa"), _("Trajes"), _("Fondos") };
    int x = 6;
    for (int i = 0; i < 3; i++) {
        int w = (int)tam_texto(nom[i]) * 8 + 28;
        if (mx >= x && mx < x + w) return i;
        x += w + 3;
    }
    return -1;
}

static void dibujar_categorias(void)
{
    if (modo != MODO_PROG) return;
    recorte(0, TOPH + TABH, CATW, H - TOPH - TABH - SBH);
    rc(0, TOPH + TABH, CATW, H - TOPH - TABH - SBH, 0xF0F2F4);
    rc(CATW - 1, TOPH + TABH, 1, H - TOPH - TABH - SBH, 0xB8C0C8);
    for (int i = 0; i < NCAT; i++) {
        int y = TOPH + TABH + 4 + i * 42;
        if (i == cat_sel) rc(2, y - 2, CATW - 5, 40, 0xFFFFFF);
        if (i == cat_sel) marco(2, y - 2, CATW - 5, 40, jb_color_cat[i]);
        rc(8, y + 2, 22, 22, jb_color_cat[i]);
        marco(8, y + 2, 22, 22, oscuro(jb_color_cat[i]));
        texto(8, y + 22, _(jb_nombre_cat[i]), 0x1E2328, 1);
    }
    recorte(0, 0, W, H);
}
static int cat_en(int mx, int my)
{
    if (modo != MODO_PROG || mx >= CATW) return -1;
    for (int i = 0; i < NCAT; i++) { int y = TOPH + TABH + 4 + i * 42; if (my >= y - 2 && my < y + 38) return i; }
    return -1;
}

static void dibujar_escenario(void)
{
    recorte(ex - 2, ey - 2, evl + 4, eva + 4);
    marco(ex - 1, ey - 1, evl + 2, eva + 2, 0x5A6470);
    jb_escena_dibujar(j->px + ey * j->larg + ex, j->larg, evl, eva);
    recorte(0, 0, W, H);
}

static void dibujar_personajes(void)
{
    if (a_pantalla_completa) return;
    recorte(spx - 2, spy, spw + 4, sph + 4);
    rc(spx - 2, spy, spw + 4, sph + 4, 0xF4F4F0);
    boton3d(b_[BT_PANTALLA].x, b_[BT_PANTALLA].y, b_[BT_PANTALLA].w, 26, _("Pantalla"), false, 0xE2E6EA);
    boton3d(b_[BT_TAMANO].x, b_[BT_TAMANO].y, b_[BT_TAMANO].w, 26, _("Tamaño"), false, 0xE2E6EA);
    pers_t *p = pers_cur();
    char t[120];
    formatar_texto(t, sizeof t, "%s: %s", _("Personaje"), p ? p->nombre : "-");
    texto(spx, spy + 32, t, 0x1E2328, 1);
    if (p) {
        char a[24], b[24], c[24];
        jb_num_texto(p->x, a, sizeof a); jb_num_texto(p->y, b, sizeof b); jb_num_texto(dir_visible(p->dir), c, sizeof c);
        formatar_texto(t, sizeof t, "x:%s y:%s %s:%s", a, b, _("dir"), c);
        texto(spx, spy + 50, t, 0x6A737C, 1);
    }
    int y0 = spy + 72, cw = 52, por = spw / cw;
    if (por < 1) por = 1;
    for (int i = 0; i <= jb_p.npers; i++) {
        int x = spx + (i % por) * cw, y = y0 + (i / por) * cw;
        if (y + 48 > spy + sph + 4) break;
        if (i == jb_p.npers) {
            rc(x, y, 48, 48, 0xE2E6EA); marco(x, y, 48, 48, 0x8A939C);
            rc(x + 22, y + 10, 4, 28, 0x1AAE55); rc(x + 10, y + 22, 28, 4, 0x1AAE55);
            continue;
        }
        pers_t *q = jb_p.pers[i];
        rc(x, y, 48, 48, i == cur ? 0xCFE6FF : 0xFFFFFF);
        marco(x, y, 48, 48, i == cur ? 0x2A6FD8 : 0x8A939C);
        if (i == cur) marco(x + 1, y + 1, 46, 46, 0x2A6FD8);
        if (q->ntrajes) jb_escena_traje(j->px, j->larg, j->larg, j->alt, q->trajes[q->traje], x + 8, y + 4, 32);
    }
    recorte(0, 0, W, H);
}

/* ============================================================ editor de trajes y fondos */

static struct { int herr; int color; int sel; bool pintando; int cell, gx, gy, tx, ty; } te = { 0, 8, 0, false, 10, 0, 0, 0, 0 };
enum { HE_LAPIZ, HE_BORRADOR, HE_RELLENO };
enum { EB_NUEVO = 1, EB_DUP, EB_BORRAR, EB_VOLTEAR, EB_LIMPIAR, EB_IMPORTAR, EB_BIBLIO, EB_NOMBRE, EB_H0, EB_H1, EB_H2 };

static uint8_t *ed_pix(int *gw, int *gh, char **nombre)
{
    if (modo == MODO_TRAJES) {
        pers_t *p = pers_cur();
        if (!p || !p->ntrajes) return NULL;
        if (te.sel < 0 || te.sel >= p->ntrajes) te.sel = 0;
        *gw = TRAJE_L; *gh = TRAJE_L; if (nombre) *nombre = p->trajes[te.sel]->nombre;
        return p->trajes[te.sel]->px;
    }
    if (!jb_p.nfondos) return NULL;
    if (te.sel < 0 || te.sel >= jb_p.nfondos) te.sel = 0;
    *gw = FONDO_L; *gh = FONDO_A; if (nombre) *nombre = jb_p.fondos[te.sel]->nombre;
    return jb_p.fondos[te.sel]->px;
}
static int ed_cuenta(void) { return modo == MODO_TRAJES ? (pers_cur() ? pers_cur()->ntrajes : 0) : jb_p.nfondos; }

static void ed_layout(void)
{
    int gw = modo == MODO_TRAJES ? TRAJE_L : FONDO_L, gh = modo == MODO_TRAJES ? TRAJE_L : FONDO_A;
    int disp_w = edw - 170 - 16, disp_h = edh - 64 - 8;
    int c = disp_w / gw;
    if (disp_h / gh < c) c = disp_h / gh;
    if (c > 12) c = 12;
    if (c < 4) c = 4;
    te.cell = c;
    te.gx = edx + 8; te.gy = edy + 60;
    te.tx = te.gx + gw * c + 14; te.ty = te.gy;
}

static void ed_recalcular(void)
{
    pers_t *p = pers_cur();
    if (modo == MODO_TRAJES && p) jb_pers_recalcular(p);
    jb_sucio = true;
}

static void dibujar_editor(void)
{
    if (modo == MODO_PROG) return;
    recorte(edx, edy, edw, edh);
    rc(edx, edy, edw, edh, 0xF4F4F0);
    ed_layout();
    int gw, gh;
    char *nombre = NULL;
    uint8_t *px = ed_pix(&gw, &gh, &nombre);
    int n = ed_cuenta();
    /* la lista de trajes o fondos */
    for (int i = 0; i <= n && i < 9; i++) {
        int x = edx + 8 + i * 52, y = edy + 6;
        if (i == n) {
            if ((modo == MODO_TRAJES && n >= MAX_TRAJES) || (modo == MODO_FONDOS && n >= MAX_FONDOS)) break;
            rc(x, y, 48, 48, 0xE2E6EA); marco(x, y, 48, 48, 0x8A939C);
            rc(x + 22, y + 10, 4, 28, 0x1AAE55); rc(x + 10, y + 22, 28, 4, 0x1AAE55);
            break;
        }
        rc(x, y, 48, 48, i == te.sel ? 0xCFE6FF : 0xFFFFFF);
        marco(x, y, 48, 48, i == te.sel ? 0x2A6FD8 : 0x8A939C);
        if (modo == MODO_TRAJES) jb_escena_traje(j->px, j->larg, j->larg, j->alt, pers_cur()->trajes[i], x + 8, y + 8, 32);
        else for (int yy = 0; yy < 36; yy++) for (int xx = 0; xx < 48; xx++) { uint8_t c = jb_p.fondos[i]->px[yy * FONDO_L + xx]; if (c) pt(x + xx, y + 6 + yy, jb_paleta[c]); }
    }
    if (!px) { texto(edx + 20, edy + 80, _("No hay nada que pintar."), 0x6A737C, 1); recorte(0, 0, W, H); return; }
    /* la rejilla */
    for (int y = 0; y < gh; y++)
        for (int x = 0; x < gw; x++) {
            uint8_t c = px[y * gw + x];
            uint32_t col = c ? jb_paleta[c] : (((x + y) & 1) ? 0xE8E8E8 : 0xFFFFFF);
            rc(te.gx + x * te.cell, te.gy + y * te.cell, te.cell, te.cell, col);
            if (te.cell >= 8) { rc(te.gx + x * te.cell, te.gy + y * te.cell, te.cell, 1, 0xD0D4D8); rc(te.gx + x * te.cell, te.gy + y * te.cell, 1, te.cell, 0xD0D4D8); }
        }
    marco(te.gx - 1, te.gy - 1, gw * te.cell + 2, gh * te.cell + 2, 0x5A6470);
    /* herramientas */
    const char *her[3] = { _("Lápiz"), _("Goma"), _("Cubo") };
    int x = te.tx, y = te.ty;
    for (int i = 0; i < 3; i++) { boton3d(x + i * 52 - (i == 2 ? 0 : 0), y, 50, 26, her[i], te.herr == i, te.herr == i ? 0xBFD8F5 : 0xE2E6EA); }
    y += 32;
    for (int i = 1; i < 32; i++) {
        int sx_ = x + ((i - 1) % 8) * 19, sy_ = y + ((i - 1) / 8) * 19;
        rc(sx_, sy_, 17, 17, jb_paleta[i]);
        marco(sx_ - 1, sy_ - 1, 19, 19, i == te.color ? 0x000000 : 0x8A939C);
        if (i == te.color) marco(sx_, sy_, 17, 17, 0xFFFFFF);
    }
    y += 4 * 19 + 8;
    const char *bt[8] = { _("Nuevo"), _("Duplicar"), _("Borrar"), _("Voltear"), _("Limpiar"), _("Importar PNG"), _("Biblioteca"), _("Nombre") };
    for (int i = 0; i < 8; i++) { boton3d(x, y, 154, 24, bt[i], false, 0xE2E6EA); y += 28; }
    char t[80];
    formatar_texto(t, sizeof t, "%s", nombre ? nombre : "");
    texto(te.gx, te.gy + gh * te.cell + 6, t, 0x1E2328, 1);
    recorte(0, 0, W, H);
}

static void ed_pintar(int gx_, int gy_, int gw, int gh, uint8_t *px)
{
    if (gx_ < 0 || gy_ < 0 || gx_ >= gw || gy_ >= gh) return;
    uint8_t c = te.herr == HE_BORRADOR ? 0 : (uint8_t)te.color;
    px[gy_ * gw + gx_] = c;
}
static void ed_rellenar(int x, int y, int gw, int gh, uint8_t *px)
{
    uint8_t viejo = px[y * gw + x], nuevo = (uint8_t)te.color;
    if (viejo == nuevo) return;
    static short pila[FONDO_L * FONDO_A * 2];
    int sp = 0;
    pila[sp++] = (short)x; pila[sp++] = (short)y;
    while (sp > 0 && sp < (int)(sizeof pila / sizeof pila[0]) - 8) {
        int yy = pila[--sp], xx = pila[--sp];
        if (xx < 0 || yy < 0 || xx >= gw || yy >= gh || px[yy * gw + xx] != viejo) continue;
        px[yy * gw + xx] = nuevo;
        pila[sp++] = (short)(xx + 1); pila[sp++] = (short)yy; pila[sp++] = (short)(xx - 1); pila[sp++] = (short)yy;
        pila[sp++] = (short)xx; pila[sp++] = (short)(yy + 1); pila[sp++] = (short)xx; pila[sp++] = (short)(yy - 1);
    }
}

/* el color de paleta más parecido a un RGB */
static uint8_t color_cercano(int r, int g, int b)
{
    int mejor = 1, md = 1 << 30;
    for (int i = 1; i < 32; i++) {
        int pr = (jb_paleta[i] >> 16) & 255, pg = (jb_paleta[i] >> 8) & 255, pb = jb_paleta[i] & 255;
        int d = (pr - r) * (pr - r) + (pg - g) * (pg - g) + (pb - b) * (pb - b);
        if (d < md) { md = d; mejor = i; }
    }
    return (uint8_t)mejor;
}

/* convierte una imagen a un rejilla de paleta; los trajes pierden el color liso del borde (se vuelve transparente) */
static void importar_en(uint8_t *dst, int gw, int gh, const imagem_t *img, bool traje)
{
    int sc = img->larg * 256 / gw > img->alt * 256 / gh ? img->larg * 256 / gw : img->alt * 256 / gh;
    int dw = traje ? img->larg * 256 / sc : gw, dh = traje ? img->alt * 256 / sc : gh;
    if (!traje) sc = 0;
    int offx = (gw - dw) / 2, offy = (gh - dh) / 2;
    static uint32_t medio[FONDO_L * FONDO_A];
    static bool fuera[FONDO_L * FONDO_A];
    for (int y = 0; y < gh; y++)
        for (int x = 0; x < gw; x++) {
            fuera[y * gw + x] = false;
            if (x < offx || y < offy || x >= offx + dw || y >= offy + dh) { fuera[y * gw + x] = true; medio[y * gw + x] = 0; continue; }
            int sx0, sy0, sx1, sy1;
            if (traje) { sx0 = (x - offx) * sc / 256; sx1 = (x - offx + 1) * sc / 256; sy0 = (y - offy) * sc / 256; sy1 = (y - offy + 1) * sc / 256; }
            else { sx0 = x * img->larg / gw; sx1 = (x + 1) * img->larg / gw; sy0 = y * img->alt / gh; sy1 = (y + 1) * img->alt / gh; }
            if (sx1 <= sx0) sx1 = sx0 + 1;
            if (sy1 <= sy0) sy1 = sy0 + 1;
            if (sx1 > img->larg) sx1 = img->larg;
            if (sy1 > img->alt) sy1 = img->alt;
            int pasox = (sx1 - sx0 + 3) / 4, pasoy = (sy1 - sy0 + 3) / 4;
            if (pasox < 1) pasox = 1;
            if (pasoy < 1) pasoy = 1;
            long r = 0, g = 0, b = 0, n = 0;
            for (int yy = sy0; yy < sy1; yy += pasoy)
                for (int xx = sx0; xx < sx1; xx += pasox) { uint32_t c = img->px[yy * img->larg + xx]; r += (c >> 16) & 255; g += (c >> 8) & 255; b += c & 255; n++; }
            if (!n) n = 1;
            medio[y * gw + x] = (uint32_t)(((r / n) << 16) | ((g / n) << 8) | (b / n));
        }
    for (int y = 0; y < gh; y++) for (int x = 0; x < gw; x++) { uint32_t c = medio[y * gw + x]; dst[y * gw + x] = fuera[y * gw + x] ? 0 : color_cercano((int)(c >> 16) & 255, (int)(c >> 8) & 255, (int)c & 255); }
    if (traje) {
        /* el fondo liso: lo que se parece a la esquina y se alcanza desde los bordes */
        uint32_t c0 = medio[offy * gw + offx];
        int r0 = (c0 >> 16) & 255, g0 = (c0 >> 8) & 255, b0 = c0 & 255;
        static short pila[TRAJE_L * TRAJE_L * 4];
        static bool vis[TRAJE_L * TRAJE_L];
        memset(vis, 0, sizeof vis);
        int sp = 0;
        for (int i = 0; i < gw; i++) { pila[sp++] = (short)i; pila[sp++] = 0; pila[sp++] = (short)i; pila[sp++] = (short)(gh - 1); pila[sp++] = 0; pila[sp++] = (short)i; pila[sp++] = (short)(gw - 1); pila[sp++] = (short)i; }
        while (sp > 0 && sp < (int)(sizeof pila / sizeof pila[0]) - 8) {
            int y = pila[--sp], x = pila[--sp];
            if (x < 0 || y < 0 || x >= gw || y >= gh || vis[y * gw + x]) continue;
            uint32_t c = medio[y * gw + x];
            int dr_ = (int)((c >> 16) & 255) - r0, dg = (int)((c >> 8) & 255) - g0, db = (int)(c & 255) - b0;
            if (!fuera[y * gw + x] && dr_ * dr_ + dg * dg + db * db > 40 * 40 * 3) continue;
            vis[y * gw + x] = true;
            dst[y * gw + x] = 0;
            pila[sp++] = (short)(x + 1); pila[sp++] = (short)y; pila[sp++] = (short)(x - 1); pila[sp++] = (short)y;
            pila[sp++] = (short)x; pila[sp++] = (short)(y + 1); pila[sp++] = (short)x; pila[sp++] = (short)(y - 1);
        }
    }
}

static bool importar_imagen(void)
{
    char c[200] = "/home";
    if (!dialogo_abrir(_("Importar una imagen"), _("Imágenes (*.png *.jpg *.bmp)|*.png;*.jpg;*.jpeg;*.bmp;*.gif|Todos los archivos (*.*)|*"), c, c, sizeof c) || !c[0]) return false;
    imagem_t img;
    int r = imagem_ler(c, &img);
    if (r != 0) { mensagem("Jabuti", _("No he podido leer esa imagen. Prueba con un PNG, JPG o BMP.")); return false; }
    jb_hist_tomar();
    int n = ed_cuenta();
    if (modo == MODO_TRAJES) {
        pers_t *p = pers_cur();
        if (!p || n >= MAX_TRAJES) { imagem_liberar(&img); mensagem("Jabuti", _("Ya hay demasiados trajes.")); return false; }
        traje_t *t = jb_traje_nuevo(nome_base(c));
        if (!t) { imagem_liberar(&img); return false; }
        char *pt_ = acha_ultimo_char(t->nombre, '.'); if (pt_) *pt_ = 0;
        importar_en(t->px, TRAJE_L, TRAJE_L, &img, true);
        p->trajes[p->ntrajes] = t; te.sel = p->ntrajes++; p->traje = te.sel;
    } else {
        if (n >= MAX_FONDOS) { imagem_liberar(&img); mensagem("Jabuti", _("Ya hay demasiados fondos.")); return false; }
        fondo_t *f = aloca_zerado(sizeof *f);
        if (!f) { imagem_liberar(&img); return false; }
        copia_texto(f->nombre, nome_base(c), sizeof f->nombre);
        char *pt_ = acha_ultimo_char(f->nombre, '.'); if (pt_) *pt_ = 0;
        importar_en(f->px, FONDO_L, FONDO_A, &img, false);
        jb_p.fondos[jb_p.nfondos] = f; te.sel = jb_p.nfondos++; jb_p.fondo = te.sel;
    }
    imagem_liberar(&img);
    modificado = true; ed_recalcular();
    registrar("jabuti: imagen importada %s\n", c);
    return true;
}

static void ed_nuevo(void)
{
    jb_hist_tomar();
    if (modo == MODO_TRAJES) {
        pers_t *p = pers_cur();
        if (!p || p->ntrajes >= MAX_TRAJES) return;
        char nm[24]; formatar_texto(nm, sizeof nm, "%s%d", _("traje"), p->ntrajes + 1);
        traje_t *t = jb_traje_nuevo(nm);
        if (!t) return;
        p->trajes[p->ntrajes] = t; te.sel = p->ntrajes++; p->traje = te.sel;
    } else {
        if (jb_p.nfondos >= MAX_FONDOS) return;
        fondo_t *f = aloca_zerado(sizeof *f);
        if (!f) return;
        formatar_texto(f->nombre, sizeof f->nombre, "%s%d", _("fondo"), jb_p.nfondos + 1);
        memset(f->px, 6, sizeof f->px);
        jb_p.fondos[jb_p.nfondos] = f; te.sel = jb_p.nfondos++; jb_p.fondo = te.sel;
    }
    modificado = true; ed_recalcular();
}
static void ed_duplicar(void)
{
    jb_hist_tomar();
    if (modo == MODO_TRAJES) {
        pers_t *p = pers_cur();
        if (!p || !p->ntrajes || p->ntrajes >= MAX_TRAJES) return;
        traje_t *t = aloca(sizeof *t);
        if (!t) return;
        *t = *p->trajes[te.sel];
        p->trajes[p->ntrajes] = t; te.sel = p->ntrajes++;
    } else {
        if (!jb_p.nfondos || jb_p.nfondos >= MAX_FONDOS) return;
        fondo_t *f = aloca(sizeof *f);
        if (!f) return;
        *f = *jb_p.fondos[te.sel];
        jb_p.fondos[jb_p.nfondos] = f; te.sel = jb_p.nfondos++;
    }
    modificado = true; ed_recalcular();
}
static void ed_borrar(void)
{
    if (modo == MODO_TRAJES) {
        pers_t *p = pers_cur();
        if (!p || p->ntrajes <= 1) { mensagem("Jabuti", _("Un personaje necesita al menos un traje.")); return; }
        jb_hist_tomar();
        libera(p->trajes[te.sel]);
        for (int k = te.sel; k + 1 < p->ntrajes; k++) p->trajes[k] = p->trajes[k + 1];
        p->ntrajes--;
        if (te.sel >= p->ntrajes) te.sel = p->ntrajes - 1;
        if (p->traje >= p->ntrajes) p->traje = p->ntrajes - 1;
    } else {
        if (jb_p.nfondos <= 1) { mensagem("Jabuti", _("El escenario necesita al menos un fondo.")); return; }
        jb_hist_tomar();
        libera(jb_p.fondos[te.sel]);
        for (int k = te.sel; k + 1 < jb_p.nfondos; k++) jb_p.fondos[k] = jb_p.fondos[k + 1];
        jb_p.nfondos--;
        if (te.sel >= jb_p.nfondos) te.sel = jb_p.nfondos - 1;
        if (jb_p.fondo >= jb_p.nfondos) jb_p.fondo = jb_p.nfondos - 1;
    }
    modificado = true; ed_recalcular();
}

static void ed_accion(int a)
{
    int gw, gh;
    char *nombre = NULL;
    uint8_t *px = ed_pix(&gw, &gh, &nombre);
    switch (a) {
    case EB_NUEVO: ed_nuevo(); break;
    case EB_DUP: ed_duplicar(); break;
    case EB_BORRAR: ed_borrar(); break;
    case EB_VOLTEAR:
        if (!px) break;
        jb_hist_tomar();
        for (int y = 0; y < gh; y++) for (int x = 0; x < gw / 2; x++) { uint8_t t = px[y * gw + x]; px[y * gw + x] = px[y * gw + gw - 1 - x]; px[y * gw + gw - 1 - x] = t; }
        modificado = true; ed_recalcular();
        break;
    case EB_LIMPIAR:
        if (!px) break;
        jb_hist_tomar();
        memset(px, modo == MODO_FONDOS ? 6 : 0, (size_t)gw * gh);
        modificado = true; ed_recalcular();
        break;
    case EB_IMPORTAR: importar_imagen(); break;
    case EB_NOMBRE: {
        if (!nombre) break;
        char n[24];
        copia_texto(n, nombre, sizeof n);
        if (pedir_texto(_("Nombre"), _("¿Cómo se llama?"), n, sizeof n) && n[0]) { jb_hist_tomar(); copia_texto(nombre, n, 24); modificado = true; }
        break;
    }
    case EB_BIBLIO: {
        /* un menú con la biblioteca; la elección llega como EV_MENU (código 6) */
        char m[512] = "";
        if (modo == MODO_TRAJES) for (int i = 0; i < jb_biblioteca_n(); i++) { if (i) junta_texto(m, "\n", sizeof m); junta_texto(m, _(jb_biblioteca_titulo(i)), sizeof m); }
        else for (int i = 0; i < jb_fondos_n(); i++) { if (i) junta_texto(m, "\n", sizeof m); junta_texto(m, _(jb_fondo_titulo(i)), sizeof m); }
        menu_contextual(j->id, te.tx, te.ty + 32 + 4 * 19 + 8 + 28 * 6, m, 6);
        break;
    }
    default: break;
    }
}

/* ============================================================ proyectos, menús y acciones */

#define FILTROS_BLQ "Proyecto de Jabuti (*.jbt;*.blq)|*.jbt;*.blq|Texto legible (*.txt)|*.txt|Todos los archivos (*.*)|*"
static char ejemplos_dir[] = "/ejemplos/jabuti";

static void pintar_todo(void);

static void proyecto_cambiado(void)
{
    if (cur >= jb_p.npers) cur = jb_p.npers - 1;
    if (cur < 0) cur = 0;
    ed_.on = false;                /* los bloques de antes ya no existen: nada que cerrar */
    dr.on = false;
    pal_reconstruir();
    jb_sucio = true;
}

static bool confirmar_perder(void)
{
    if (!modificado) return true;
    return confirmar("Jabuti", _("Hay cambios sin guardar. ¿Seguro que quieres seguir sin guardarlos?"));
}

static void accion_nuevo(void)
{
    if (!confirmar_perder()) return;
    jb_motor_parar();
    jb_proy_nuevo();
    jb_hist_reiniciar();
    jb_motor_iniciar();
    jb_lapiz_borrar();
    ruta[0] = 0; modificado = false; cur = 0; area_sx = area_sy = 0;
    proyecto_cambiado();
    registrar("jabuti: nuevo proyecto\n");
}

static void abrir_ruta(const char *c)
{
    jb_motor_parar();
    if (!jb_abrir(c)) {
        char m[300];
        formatar_texto(m, sizeof m, "%s\n%s", _("No he podido abrir ese proyecto. Puede que no sea de Jabuti o que esté dañado."), jb_p.error);
        mensagem("Jabuti", m);
        registrar("jabuti: error al abrir %s: %s\n", c, jb_p.error);
        jb_proy_nuevo();
        return;
    }
    jb_hist_reiniciar();
    jb_motor_iniciar();
    jb_lapiz_borrar();
    copia_texto(ruta, c, sizeof ruta);
    modificado = false; cur = 0; area_sx = area_sy = 0; te.sel = 0;
    proyecto_cambiado();
    registrar("jabuti: abierto %s (%d personajes)\n", ruta, jb_p.npers);
}

static void accion_abrir(void)
{
    if (!confirmar_perder()) return;
    char c[200];
    copia_texto(c, ruta[0] ? ruta : "/home/", sizeof c);
    if (dialogo_abrir(_("Abrir un proyecto"), FILTROS_BLQ, c, c, sizeof c) && c[0]) abrir_ruta(c);
}

/* .txt: el programa como texto legible, sin comprimir (se puede abrir de nuevo con Jabuti) */
static bool es_txt(const char *c) { unsigned n = (unsigned)tam_texto(c); return n > 4 && compara_texto(c + n - 4, ".txt") == 0; }
static bool guardar_texto(const char *c)
{
    unsigned n = 0;
    char *t = jb_escribir_texto(&n);
    if (!t) return false;
    int fd = abrir(c, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    bool ok = fd >= 0 && escrever_tudo(fd, t, n) >= 0;
    if (fd >= 0) fechar(fd);
    libera(t);
    return ok;
}

static bool guardar_en(const char *c)
{
    if (es_txt(c)) {
        if (!guardar_texto(c)) { mensagem("Jabuti", _("No he podido guardar el proyecto. Comprueba que hay sitio y que puedes escribir ahí.")); return false; }
        registrar("jabuti: texto guardado %s\n", c);
        dizer(_("Guardado como texto legible."));
        return true;
    }
    if (!jb_guardar(c)) { mensagem("Jabuti", _("No he podido guardar el proyecto. Comprueba que hay sitio y que puedes escribir ahí.")); return false; }
    copia_texto(ruta, c, sizeof ruta);
    modificado = false;
    registrar("jabuti: guardado %s\n", ruta);
    return true;
}
static bool accion_guardar_como(void)
{
    char c[200];
    copia_texto(c, ruta[0] ? ruta : "/home/proyecto.jbt", sizeof c);
    if (!dialogo_guardar(_("Guardar el proyecto"), FILTROS_BLQ, c, c, sizeof c) || !c[0]) return false;
    unsigned n = (unsigned)tam_texto(c);
    if (n < 4 || (compara_texto(c + n - 4, ".jbt") != 0 && compara_texto(c + n - 4, ".txt") != 0)) junta_texto(c, ".jbt", sizeof c);
    return guardar_en(c);
}
static bool accion_guardar(void) { return ruta[0] ? guardar_en(ruta) : accion_guardar_como(); }

static void accion_deshacer(bool rehacer)
{
    edicion_terminar(true);
    jb_motor_parar();
    bool ok = rehacer ? jb_hist_rehacer() : jb_hist_deshacer();
    if (ok) { modificado = true; registrar(rehacer ? "jabuti: rehacer\n" : "jabuti: deshacer\n"); }
    jb_motor_iniciar();
    proyecto_cambiado();
}

static void accion_ayuda(void)
{
    char *argv[3] = { "markdown", "/ayuda/jabuti.md", NULL };
    int pid = executar("/bin/markdown", argv);
    registrar("jabuti: ayuda (pid %d)\n", pid);
}

/* ---- menús desplegables de las ranuras ---- */
static struct { blk_t *b; int i, menu, n, otra; char vals[48][32]; } mp;
static struct { blk_t *b; int g; } cm;
static int sp_menu;
static char ej_nombres[24][64];
static int n_ej;

static char mtext[1800];
static void m_add(const char *clave, const char *vista)
{
    if (mp.n >= 46) return;
    if (mp.n) junta_texto(mtext, "\n", sizeof mtext);
    junta_texto(mtext, vista, sizeof mtext);
    copia_texto(mp.vals[mp.n++], clave, 32);
}
static void m_sep(void) { if (mp.n >= 46) return; if (mp.n) junta_texto(mtext, "\n", sizeof mtext); junta_texto(mtext, "-", sizeof mtext); mp.vals[mp.n++][0] = 0; }

static void abrir_menu_valor(blk_t *b, int i, int x, int y)
{
    int menu = jb_ops[b->op].menu;
    pers_t *p = pers_cur();
    mp.b = b; mp.i = i; mp.menu = menu; mp.n = 0; mp.otra = -1; mtext[0] = 0;
    char buf[48];
    switch (menu) {
    case M_TECLA: {
        static const char *const k[] = { "espacio", "flecha arriba", "flecha abajo", "flecha izquierda", "flecha derecha", "Intro", "cualquiera" };
        for (int q = 0; q < 7; q++) m_add(k[q], idioma_traducir(k[q]));
        m_sep();
        mp.otra = mp.n;
        m_add("", _("Otra tecla..."));
        break;
    }
    case M_MENSAJE: for (int q = 1; q <= MAX_MSG; q++) { char m[16]; formatar_texto(m, sizeof m, "mensaje%d", q); m_add(m, idioma_traducir(m)); } break;
    case M_OBJETIVO: case M_DISTANCIA: case M_TOCANDO:
        if (menu == M_TOCANDO) m_add("borde", idioma_traducir("borde"));
        m_add("raton", idioma_traducir("puntero del ratón"));
        if (menu == M_OBJETIVO) m_add("azar", idioma_traducir("posición al azar"));
        for (int q = 0; q < jb_p.npers; q++) if (jb_p.pers[q] != p || menu == M_OBJETIVO) m_add(jb_p.pers[q]->nombre, jb_p.pers[q]->nombre);
        break;
    case M_ROT: m_add("todo", idioma_traducir("todo")); m_add("izquierda-derecha", idioma_traducir("izquierda-derecha")); m_add("ninguno", idioma_traducir("ninguno")); break;
    case M_TRAJE: if (p) for (int q = 0; q < p->ntrajes; q++) m_add(p->trajes[q]->nombre, p->trajes[q]->nombre); break;
    case M_FONDO: for (int q = 0; q < jb_p.nfondos; q++) m_add(jb_p.fondos[q]->nombre, jb_p.fondos[q]->nombre); break;
    case M_COLOR: for (int q = 0; q < jb_colores_n(); q++) m_add(jb_color_nombre_n(q), idioma_traducir(jb_color_nombre_n(q))); break;
    case M_SONIDO: { static const char *const s[] = { "pitido", "salto", "moneda", "bocina", "aplauso" }; for (int q = 0; q < 5; q++) m_add(s[q], idioma_traducir(s[q])); break; }
    case M_VAR: for (int q = 0; q < jb_p.nvars; q++) m_add(jb_p.vars[q].nombre, jb_p.vars[q].nombre); break;
    case M_LISTA: for (int q = 0; q < jb_p.nlistas; q++) m_add(jb_p.listas[q].nombre, jb_p.listas[q].nombre); break;
    case M_DETENER: m_add("todo", idioma_traducir("todo")); m_add("este guion", idioma_traducir("este guion")); break;
    default: return;
    }
    (void)buf;
    if (mp.n == 0) { dizer(_("Todavía no hay nada que elegir aquí (crea primero una variable o una lista).")); return; }
    menu_contextual(j->id, x, y, mtext, 1);
}

static void aplicar_valor(int idx)
{
    if (idx < 0 || idx >= mp.n || !mp.b) return;
    char v[40];
    copia_texto(v, mp.vals[idx], sizeof v);
    if (idx == mp.otra) {
        char t[8] = "";
        if (!pedir_texto(_("Otra tecla"), _("Escribe la letra o el número de la tecla:"), t, sizeof t) || !t[0]) return;
        char c = t[0];
        if (c >= 'A' && c <= 'Z') c += 32;
        v[0] = c; v[1] = 0;
    }
    if (!v[0]) return;
    jb_hist_tomar();
    copia_texto(mp.b->a[mp.i].t, v, sizeof mp.b->a[mp.i].t);
    modificado = true;
    registrar("jabuti: valor %s %d = %s\n", jb_ops[mp.b->op].clave, mp.i, v);
}

static blk_t **enlace_de(blk_t **enlace, blk_t *obj)
{
    for (; *enlace; enlace = &(*enlace)->sig) {
        if (*enlace == obj) return enlace;
        blk_t **r = enlace_de(&(*enlace)->c1, obj);
        if (r) return r;
        r = enlace_de(&(*enlace)->c2, obj);
        if (r) return r;
    }
    return NULL;
}
/* quita solo ese bloque (los de debajo se quedan unidos) */
static void borrar_bloque(blk_t *b)
{
    pers_t *p = pers_cur();
    if (!p) return;
    jb_hist_tomar();
    bool es_rep = jb_ops[b->op].tipo == K_REP || jb_ops[b->op].tipo == K_BOOL;
    if (es_rep) { if (desprender(p, b)) jb_blk_liberar(b); modificado = true; return; }
    for (int g = 0; g < p->nguiones; g++) {
        blk_t **e = p->guiones[g].top == b ? &p->guiones[g].top : enlace_de(&p->guiones[g].top, b);
        if (!e) continue;
        *e = b->sig;
        b->sig = NULL;
        jb_blk_liberar(b);
        if (!p->guiones[g].top) jb_guion_borrar(p, g);
        modificado = true;
        return;
    }
}
static void duplicar_bloque(blk_t *b, int g)
{
    pers_t *p = pers_cur();
    if (!p || g < 0 || g >= p->nguiones || p->nguiones >= MAX_GUIONES) return;
    jb_hist_tomar();
    blk_t *c = jb_blk_copiar(b, true);
    if (!c) return;
    p->guiones[p->nguiones] = (guion_t){ c, p->guiones[g].x + 30, p->guiones[g].y + 30 };
    p->nguiones++;
    modificado = true;
}

static void menu_bloque(blk_t *b, int g, int x, int y)
{
    cm.b = b; cm.g = g;
    bool es_rep = jb_ops[b->op].tipo == K_REP || jb_ops[b->op].tipo == K_BOOL;
    char m[200];
    if (es_rep) formatar_texto(m, sizeof m, "%s", _("Borrar"));
    else formatar_texto(m, sizeof m, "%s\n%s", _("Duplicar"), _("Borrar"));
    menu_contextual(j->id, x, y, m, 2);
}

static void abrir_menu_ejemplos(int x, int y)
{
    n_ej = 0;
    char m[1600] = "";
    entrada_pasta_t e;
    for (int i = 0; n_ej < 24 && listar(ejemplos_dir, i, &e) == 1; i++) {
        unsigned n = (unsigned)tam_texto(e.nome);
        if (n < 5 || compara_texto(e.nome + n - 4, ".jbt") != 0) continue;
        copia_texto(ej_nombres[n_ej], e.nome, sizeof ej_nombres[0]);
        char t[64];
        copia_texto(t, e.nome, sizeof t);
        t[n - 4] = 0;
        if (n_ej) junta_texto(m, "\n", sizeof m);
        junta_texto(m, t, sizeof m);
        n_ej++;
    }
    if (!n_ej) { dizer(_("No encuentro los ejemplos.")); return; }
    menu_contextual(j->id, x, y, m, 4);
}

static void abrir_menu_personajes(int x, int y)
{
    char m[900] = "";
    for (int i = 0; i < jb_biblioteca_n(); i++) { if (i) junta_texto(m, "\n", sizeof m); junta_texto(m, _(jb_biblioteca_titulo(i)), sizeof m); }
    junta_texto(m, "\n-\n", sizeof m);
    junta_texto(m, _("Pintar uno nuevo"), sizeof m);
    junta_texto(m, "\n", sizeof m);
    junta_texto(m, _("Importar una imagen..."), sizeof m);
    menu_contextual(j->id, x, y, m, 3);
}

static void personaje_nuevo_pintado(const char *nombre_traje_img)
{
    (void)nombre_traje_img;
    jb_hist_tomar();
    char nm[24]; formatar_texto(nm, sizeof nm, "%s%d", _("Personaje"), jb_p.npers + 1);
    pers_t *p = jb_pers_nuevo(nm);
    if (!p) { mensagem("Jabuti", _("Ya hay demasiados personajes.")); jb_hist_descartar(); return; }
    traje_t *t = jb_traje_nuevo("traje1");
    if (t) { p->trajes[p->ntrajes++] = t; }
    jb_pers_recalcular(p);
    cur = jb_p.npers - 1;
    modificado = true;
    proyecto_cambiado();
    modo = MODO_TRAJES; te.sel = 0;
}

static void accion_menu_personajes(int idx)
{
    int nb = jb_biblioteca_n();
    if (idx < 0) return;
    if (idx < nb) {
        jb_hist_tomar();
        pers_t *p = jb_pers_biblioteca(idx);
        if (!p) { mensagem("Jabuti", _("Ya hay demasiados personajes.")); jb_hist_descartar(); return; }
        p->x = ((int)(jb_p.npers % 5) - 2) * 60 * U; p->y = ((jb_p.npers / 5) % 3 - 1) * 50 * U;
        cur = jb_p.npers - 1; modificado = true; proyecto_cambiado();
    } else if (idx == nb + 1) personaje_nuevo_pintado(NULL);
    else if (idx == nb + 2) {
        personaje_nuevo_pintado(NULL);
        if (!importar_imagen()) { /* sin imagen: se queda con el traje vacío */ }
    }
}

static void crear_variable(bool lista)
{
    char n[24] = "";
    if (!pedir_texto(lista ? _("Nueva lista") : _("Nueva variable"), lista ? _("Nombre de la lista:") : _("Nombre de la variable:"), n, sizeof n) || !n[0]) return;
    jb_hist_tomar();
    int i = lista ? jb_lista_crear(n) : jb_var_crear(n);
    if (i < 0) { mensagem("Jabuti", _("Ya hay demasiadas.")); jb_hist_descartar(); return; }
    modificado = true;
    pal_reconstruir();
    registrar("jabuti: %s creada %s\n", lista ? "lista" : "variable", n);
}

/* ============================================================ pintar y registrar */

static void dibujar_estado(void)
{
    recorte(0, H - SBH, W, SBH);
    rc(0, H - SBH, W, SBH, 0xDDE3E8);
    rc(0, H - SBH, W, 1, 0xB0BAC4);
    const char *t = jb_aviso[0] ? jb_aviso : estado_txt;
    uint32_t col = jb_aviso[0] ? 0xA04000 : 0x1E2328;
    texto(8, H - SBH + 3, t, col, 1);
    recorte(0, 0, W, H);
}

static void pintar_todo(void)
{
    calcular_layout();
    recorte(0, 0, W, H);
    if (a_pantalla_completa) {
        rc(0, 0, W, H, 0x101418);
        dibujar_escenario();
        dibujar_estado();
        jan_mostrar(j);
        return;
    }
    rc(0, 0, W, H, 0xF4F4F0);
    dibujar_barra();
    dibujar_pestanas();
    dibujar_categorias();
    dibujar_paleta();
    dibujar_area();
    dibujar_editor();
    dibujar_escenario();
    dibujar_personajes();
    dibujar_estado();
    jan_mostrar(j);
}

static void repintar_arrastre(void)
{
    dibujar_paleta();
    dibujar_area();
    dibujar_arrastre();
    jan_mostrar_area(j, px_, ay, ax + aw - px_, ah);
}
static void repintar_programa(void)
{
    dibujar_area();
    jan_mostrar_area(j, ax, ay, aw, ah);
}
static void repintar_escenario(void)
{
    dibujar_escenario();
    jan_mostrar_area(j, ex - 2, ey - 2, evl + 4, eva + 4);
    static int cuenta;
    if (!a_pantalla_completa && ++cuenta >= 8) { cuenta = 0; dibujar_personajes(); jan_mostrar_area(j, spx - 2, spy, spw + 4, sph + 4); }
}
static void repintar_estado(void) { dibujar_estado(); jan_mostrar_area(j, 0, H - SBH, W, SBH); }

static void log_bloques(void)
{
    const blk_t *vistos[64];
    int nv = 0;
    for (int i = 0; i < nha; i++) {
        hit_t *h = &hits_a[i];
        if (h->t != H_BLK) continue;
        bool visto = false;
        for (int k = 0; k < nv; k++) if (vistos[k] == h->b) visto = true;
        if (visto || nv >= 64) continue;
        vistos[nv++] = h->b;
        registrar("jabuti: bloque %s x=%d y=%d w=%d h=%d\n", jb_ops[h->b->op].clave, h->x, h->y, h->b->w, h->b->h);
    }
    for (int i = 0; i < nha; i++) {
        hit_t *h = &hits_a[i];
        if (h->t == H_SLOT) registrar("jabuti: ranura %s %d x=%d y=%d w=%d h=%d\n", jb_ops[h->b->op].clave, h->i, h->x, h->y, h->w, h->h);
        if (h->t == H_MENU) registrar("jabuti: menu %s %d x=%d y=%d w=%d h=%d\n", jb_ops[h->b->op].clave, h->i, h->x, h->y, h->w, h->h);
    }
    {
        pers_t *p = pers_cur();
        for (int g = 0; p && g < p->nguiones; g++) {
            char t[400];
            jb_guion_texto(p->guiones[g].top, t, sizeof t);
            registrar("jabuti: guion %d: %s\n", g, t);
        }
    }
    for (int i = 0; i < ncn; i++) {
        static const char *const nombres[] = { "sig", "c1", "c2", "top" };
        registrar("jabuti: conexion %s %s x=%d y=%d\n", nombres[conns[i].t], jb_ops[conns[i].b->op].clave, conns[i].x, conns[i].y);
    }
}

static void log_paleta(void)
{
    for (int i = 0; i < nhp; i++) {
        hit_t *h = &hits_p[i];
        if (h->t == H_BLK) registrar("jabuti: paleta %s x=%d y=%d w=%d h=%d\n", jb_ops[h->b->op].clave, h->x, h->y, h->b->w, h->b->h);
        else if (h->t == H_BTN) registrar("jabuti: paleta-boton %d x=%d y=%d w=%d h=%d\n", h->i, h->x, h->y, h->w, h->h);
    }
}

static void log_geometria(void)
{
    registrar("jabuti: ventana %dx%d\n", W, H);
    registrar("jabuti: escenario x=%d y=%d w=%d h=%d\n", ex, ey, evl, eva);
    registrar("jabuti: area x=%d y=%d w=%d h=%d\n", ax, ay, aw, ah);
    registrar("jabuti: panel x=%d y=%d w=%d h=%d\n", spx, spy, spw, sph);
    for (int i = 0; i < BT_N; i++) registrar("jabuti: boton %s x=%d y=%d w=%d h=%d\n", b_[i].id, b_[i].x, b_[i].y, b_[i].w, b_[i].h);
    for (int i = 0; i < NCAT; i++) registrar("jabuti: categoria %d x=%d y=%d w=%d h=%d\n", i, 0, TOPH + TABH + 4 + i * 42 - 2, CATW, 40);
    log_paleta();
}

static void log_estado(void)
{
    for (int i = 0; i < jb_p.npers; i++) {
        pers_t *p = jb_p.pers[i];
        char x[24], y[24], d[24];
        jb_num_texto(p->x, x, sizeof x); jb_num_texto(p->y, y, sizeof y); jb_num_texto(dir_visible(p->dir), d, sizeof d);
        registrar("jabuti: personaje %s x=%s y=%s dir=%s tam=%d vis=%d traje=%d decir=%s\n", p->nombre, x, y, d, p->tam, p->visible ? 1 : 0, p->traje, p->burbuja);
    }
    for (int i = 0; i < jb_p.nvars; i++) {
        char v[48];
        if (jb_p.vars[i].es_t) copia_texto(v, jb_p.vars[i].t, sizeof v); else jb_num_texto(jb_p.vars[i].n, v, sizeof v);
        registrar("jabuti: variable %s = %s\n", jb_p.vars[i].nombre, v);
    }
}

/* ============================================================ eventos */

static struct { int t; hit_t h; int x0, y0; bool moved; int sx, sy; } pr;
enum { P_NADA, P_BLOQUE, P_RANURA, P_MENU, P_PAN, P_PALETA, P_PINTAR };
static int ult_mx, ult_my;

static bool en_rect(int mx, int my, int x, int y, int w, int h) { return mx >= x && my >= y && mx < x + w && my < y + h; }

static void raton_logico(int mx, int my)
{
    jb_raton_x = (mx - ex) * ESC_L / evl - ESC_L / 2;
    jb_raton_y = ESC_A / 2 - (my - ey) * ESC_A / eva;
}

static bool hit_origen(blk_t *b, int *x, int *y)
{
    for (int i = 0; i < nha; i++) if (hits_a[i].t == H_BLK && hits_a[i].b == b) { *x = hits_a[i].x; *y = hits_a[i].y; return true; }
    return false;
}

static void iniciar_arrastre_area(void)
{
    pers_t *p = pers_cur();
    blk_t *b = pr.h.b;
    int ox_, oy_;
    if (!p || !hit_origen(b, &ox_, &oy_)) { pr.t = P_NADA; return; }
    jb_hist_tomar();
    if (!desprender(p, b)) { jb_hist_descartar(); pr.t = P_NADA; return; }
    M = &MET[zoom];
    dr.on = true; dr.b = b; dr.nuevo = false; dr.hay_snap = false;
    int tipo = jb_ops[b->op].tipo;
    dr.rep = tipo == K_REP || tipo == K_BOOL;
    if (dr.rep) { medir(b); dr.w = b->w; dr.h = b->h; } else medir_pila(b, &dr.w, &dr.h);
    dr.ox = pr.x0 - ox_; dr.oy = pr.y0 - oy_;
    dr.x = ult_mx - dr.ox; dr.y = ult_my - dr.oy;
    pr.t = P_NADA;
    modificado = true;
    dibujar_area();       /* sin el bloque: refresca las conexiones */
}

static void iniciar_arrastre_paleta(void)
{
    blk_t *root = pr.h.b;
    blk_t *c = jb_blk_copiar(root, false);
    if (!c) { pr.t = P_NADA; return; }
    jb_hist_tomar();
    M = &MET[zoom];
    dr.on = true; dr.b = c; dr.nuevo = true; dr.hay_snap = false;
    int tipo = jb_ops[c->op].tipo;
    dr.rep = tipo == K_REP || tipo == K_BOOL;
    if (dr.rep) { medir(c); dr.w = c->w; dr.h = c->h; } else medir_pila(c, &dr.w, &dr.h);
    dr.ox = 14; dr.oy = dr.h < 24 ? dr.h / 2 : 12;
    dr.x = ult_mx - dr.ox; dr.y = ult_my - dr.oy;
    pr.t = P_NADA;
    dibujar_area();
}

static void soltar_arrastre(int mx)
{
    pers_t *p = pers_cur();
    dr.on = false;
    blk_t *b = dr.b;
    dr.b = NULL;
    if (!p || mx < ax) {          /* sobre la paleta: se tira */
        jb_blk_liberar(b);
        if (dr.nuevo) jb_hist_descartar(); else modificado = true;
        dizer(dr.nuevo ? "" : _("Bloque borrado (Ctrl+Z lo recupera)."));
        return;
    }
    if (dr.rep) {
        if (dr.hay_snap) {
            blk_t *viejo = dr.drop.b->a[dr.drop.i].rep;
            dr.drop.b->a[dr.drop.i].rep = b;
            if (viejo) jb_blk_liberar(viejo);
        } else {
            jb_blk_liberar(b);
            if (dr.nuevo) jb_hist_descartar();
            dizer(_("Ese bloque redondo va dentro de un hueco de otro bloque."));
        }
        modificado = true;
        return;
    }
    int gx = (dr.x - ax + area_sx) * 100 / ZF[zoom], gy = (dr.y - ay + area_sy) * 100 / ZF[zoom];
    if (gy < 20) gy = 20;
    blk_t *ultimo = jb_ultimo(b);
    if (dr.hay_snap) {
        conn_t *c = &dr.snap;
        if (c->t == CN_SIG) { ultimo->sig = c->b->sig; c->b->sig = b; }
        else if (c->t == CN_C1) { ultimo->sig = c->b->c1; c->b->c1 = b; }
        else if (c->t == CN_C2) { ultimo->sig = c->b->c2; c->b->c2 = b; }
        else if (c->t == CN_TOP) {
            for (int g = 0; g < p->nguiones; g++) if (p->guiones[g].top == c->b) { ultimo->sig = c->b; p->guiones[g].top = b; p->guiones[g].x = gx; p->guiones[g].y = gy; break; }
        }
    } else {
        if (p->nguiones >= MAX_GUIONES) { jb_blk_liberar(b); dizer(_("Ya hay demasiados grupos de bloques.")); if (dr.nuevo) jb_hist_descartar(); return; }
        p->guiones[p->nguiones++] = (guion_t){ b, gx, gy };
    }
    modificado = true;
    dizer("");
}

static void seleccionar_personaje(int i)
{
    if (i < 0 || i >= jb_p.npers) return;
    edicion_terminar(true);
    cur = i; area_sx = area_sy = 0; te.sel = jb_p.pers[i]->traje;
    pal_reconstruir();
}

static void clic_escenario(int mx, int my)
{
    raton_logico(mx, my);
    jb_raton_pulsado = true;
    if (jb_hay_pregunta) return;
    jb_motor_clic(jb_raton_x, jb_raton_y);
}

static void clic_editor(int mx, int my)
{
    int gw, gh;
    uint8_t *px = ed_pix(&gw, &gh, NULL);
    int n = ed_cuenta();
    /* lista */
    for (int i = 0; i <= n && i < 9; i++) {
        int x = edx + 8 + i * 52, y = edy + 6;
        if (!en_rect(mx, my, x, y, 48, 48)) continue;
        if (i == n) ed_accion(EB_NUEVO);
        else { te.sel = i; if (modo == MODO_TRAJES) { pers_cur()->traje = i; jb_pers_recalcular(pers_cur()); } else jb_p.fondo = i; jb_sucio = true; }
        pintar_todo();
        return;
    }
    if (px && en_rect(mx, my, te.gx, te.gy, gw * te.cell, gh * te.cell)) {
        int cx_ = (mx - te.gx) / te.cell, cy_ = (my - te.gy) / te.cell;
        jb_hist_tomar();
        if (te.herr == HE_RELLENO) ed_rellenar(cx_, cy_, gw, gh, px);
        else ed_pintar(cx_, cy_, gw, gh, px);
        modificado = true; pr.t = P_PINTAR; ed_recalcular();
        pintar_todo();
        return;
    }
    int x = te.tx, y = te.ty;
    for (int i = 0; i < 3; i++) if (en_rect(mx, my, x + i * 52, y, 50, 26)) { te.herr = i; pintar_todo(); return; }
    y += 32;
    for (int i = 1; i < 32; i++) {
        int sx_ = x + ((i - 1) % 8) * 19, sy_ = y + ((i - 1) / 8) * 19;
        if (en_rect(mx, my, sx_, sy_, 17, 17)) { te.color = i; if (te.herr == HE_BORRADOR) te.herr = HE_LAPIZ; pintar_todo(); return; }
    }
    y += 4 * 19 + 8;
    for (int i = 0; i < 8; i++) {
        if (en_rect(mx, my, x, y, 154, 24)) {
            static const int acc[8] = { EB_NUEVO, EB_DUP, EB_BORRAR, EB_VOLTEAR, EB_LIMPIAR, EB_IMPORTAR, EB_BIBLIO, EB_NOMBRE };
            ed_accion(acc[i]);
            pintar_todo();
            return;
        }
        y += 28;
    }
}

static bool cerrar_ok(void) { return confirmar_perder(); }

static void accion_boton(int i, int mx, int my)
{
    (void)mx; (void)my;
    switch (i) {
    case BT_BANDERA:
        edicion_terminar(true);
        jb_motor_bandera();
        corriendo = true;
        dizer(_("En marcha. Pulsa el botón rojo para parar."));
        registrar("jabuti: bandera\n");
        break;
    case BT_PARAR:
        jb_motor_parar(); corriendo = false;
        dizer(_("Parado."));
        registrar("jabuti: parado\n");
        log_estado();
        break;
    case BT_NUEVO: accion_nuevo(); break;
    case BT_ABRIR: accion_abrir(); break;
    case BT_GUARDAR: accion_guardar(); break;
    case BT_DESHACER: if (jb_hist_hay_deshacer()) accion_deshacer(false); break;
    case BT_REHACER: if (jb_hist_hay_rehacer()) accion_deshacer(true); break;
    case BT_EJEMPLOS: abrir_menu_ejemplos(b_[i].x, b_[i].y + b_[i].h); break;
    case BT_ZMENOS: if (zoom > 0) { zoom--; registrar("jabuti: zoom %d\n", zoom); } break;
    case BT_ZMAS: if (zoom < 2) { zoom++; registrar("jabuti: zoom %d\n", zoom); } break;
    case BT_AYUDA: accion_ayuda(); break;
    case BT_PANTALLA: jan_tela_cheia(j, true); break;
    case BT_TAMANO: esc_pref = (esc_pref + 1) % 4; registrar("jabuti: tamano escenario %d\n", ESC_TAM[esc_pref]); break;
    default: break;
    }
}

static void tratar_clic(int mx, int my, unsigned botones, unsigned cliques)
{
    ult_mx = mx; ult_my = my;
    pr.t = P_NADA; pr.moved = false; pr.x0 = mx; pr.y0 = my;
    if (ed_.on) {
        bool dentro = false;
        for (int i = nha - 1; i >= 0; i--) if (hits_a[i].t == H_SLOT && hits_a[i].b == ed_.b && hits_a[i].i == ed_.i && en_rect(mx, my, hits_a[i].x, hits_a[i].y, hits_a[i].w, hits_a[i].h)) dentro = true;
        if (!dentro) { edicion_terminar(true); repintar_programa(); }
    }
    if (a_pantalla_completa) { if (en_rect(mx, my, ex, ey, evl, eva)) clic_escenario(mx, my); return; }
    /* barra y botones del escenario */
    for (int i = 0; i < BT_N; i++) {
        if (i == BT_PERSONAJE) continue;
        if (en_rect(mx, my, b_[i].x, b_[i].y, b_[i].w, b_[i].h)) {
            if (botones & 2) return;
            accion_boton(i, mx, my);
            pintar_todo();
            return;
        }
    }
    if (en_rect(mx, my, b_[BT_PERSONAJE].x, b_[BT_PERSONAJE].y, b_[BT_PERSONAJE].w, b_[BT_PERSONAJE].h) && pers_cur()) {
        char n[24];
        copia_texto(n, pers_cur()->nombre, sizeof n);
        if (pedir_texto(_("Nombre"), _("¿Cómo se llama este personaje?"), n, sizeof n) && n[0]) { jb_hist_tomar(); copia_texto(pers_cur()->nombre, n, 24); modificado = true; }
        pintar_todo();
        return;
    }
    int pe = pestana_en(mx, my);
    if (pe >= 0) { edicion_terminar(true); modo = pe; te.sel = pe == MODO_TRAJES && pers_cur() ? pers_cur()->traje : jb_p.fondo; pintar_todo(); if (pe == MODO_PROG) log_paleta(); return; }
    int ca = cat_en(mx, my);
    if (ca >= 0) { cat_sel = ca; pal_scroll = 0; pal_reconstruir(); pintar_todo(); log_paleta(); registrar("jabuti: categoria elegida %d\n", ca); return; }
    if (en_rect(mx, my, ex, ey, evl, eva)) { clic_escenario(mx, my); return; }
    /* panel de personajes */
    if (en_rect(mx, my, spx, spy + 72, spw, sph - 72)) {
        int cw = 52, por = spw / cw;
        if (por < 1) por = 1;
        for (int i = 0; i <= jb_p.npers; i++) {
            int x = spx + (i % por) * cw, y = spy + 72 + (i / por) * cw;
            if (!en_rect(mx, my, x, y, 48, 48)) continue;
            if (i == jb_p.npers) { if (!(botones & 2)) abrir_menu_personajes(x, y + 48); }
            else if (botones & 2) {
                sp_menu = i;
                char m[100];
                formatar_texto(m, sizeof m, "%s\n%s\n%s", _("Cambiar el nombre"), _("Duplicar"), _("Borrar"));
                menu_contextual(j->id, x, y + 48, m, 5);
            } else seleccionar_personaje(i);
            pintar_todo();
            return;
        }
        return;
    }
    if (modo != MODO_PROG) {
        if (en_rect(mx, my, edx, edy, edw, edh) && !(botones & 2)) clic_editor(mx, my);
        return;
    }
    /* la paleta */
    if (en_rect(mx, my, px_, py_, pw_, ph_)) {
        for (int i = nhp - 1; i >= 0; i--) {
            hit_t *h = &hits_p[i];
            if (h->t == H_DROP || !en_rect(mx, my, h->x, h->y, h->w, h->h)) continue;
            if (h->t == H_BTN) { crear_variable(h->i == B_LISTA_NUEVA); pintar_todo(); log_paleta(); return; }
            if (botones & 2) return;
            pr.t = P_PALETA; pr.h = *h;
            return;
        }
        return;
    }
    /* el área de programa */
    if (en_rect(mx, my, ax, ay, aw, ah)) {
        for (int i = nha - 1; i >= 0; i--) {
            hit_t *h = &hits_a[i];
            if (h->t == H_DROP || !en_rect(mx, my, h->x, h->y, h->w, h->h)) continue;
            if (botones & 2) { if (h->t == H_BLK) menu_bloque(h->b, h->g, mx, my); return; }
            pr.h = *h;
            pr.t = h->t == H_SLOT ? P_RANURA : h->t == H_MENU ? P_MENU : P_BLOQUE;
            return;
        }
        if (!(botones & 2)) { pr.t = P_PAN; pr.sx = area_sx; pr.sy = area_sy; }
        else {
            sp_menu = -1;
            char m[100];
            formatar_texto(m, sizeof m, "%s", _("Ordenar los grupos de bloques"));
            menu_contextual(j->id, mx, my, m, 7);
        }
    }
    (void)cliques;
}

static void tratar_soltar(int mx, int my);
static void tratar_mover(int mx, int my, unsigned botones)
{
    ult_mx = mx; ult_my = my;
    if (en_rect(mx, my, ex, ey, evl, eva)) raton_logico(mx, my);
    if (!(botones & 1)) {
        if (dr.on) { tratar_soltar(mx, my); }      /* se soltó fuera de la ventana y no llegó el aviso */
        return;
    }
    if (dr.on) {
        dr.x = mx - dr.ox; dr.y = my - dr.oy;
        buscar_snap();
        repintar_arrastre();
        return;
    }
    int dx = mx - pr.x0, dy = my - pr.y0;
    if (pr.t == P_PAN) {
        area_sx = pr.sx - dx; area_sy = pr.sy - dy;
        if (area_sx < -200) area_sx = -200;
        if (area_sy < -100) area_sy = -100;
        repintar_programa();
    } else if (pr.t == P_BLOQUE || pr.t == P_PALETA) {
        if (!pr.moved && dx * dx + dy * dy > 25) {
            pr.moved = true;
            if (pr.t == P_BLOQUE) iniciar_arrastre_area(); else iniciar_arrastre_paleta();
            if (dr.on) { dr.x = mx - dr.ox; dr.y = my - dr.oy; buscar_snap(); repintar_arrastre(); }
        }
    } else if (pr.t == P_PINTAR) {
        int gw, gh;
        uint8_t *px = ed_pix(&gw, &gh, NULL);
        if (px && en_rect(mx, my, te.gx, te.gy, gw * te.cell, gh * te.cell) && te.herr != HE_RELLENO) {
            ed_pintar((mx - te.gx) / te.cell, (my - te.gy) / te.cell, gw, gh, px);
            ed_recalcular();
            dibujar_editor(); dibujar_escenario();
            jan_mostrar_area(j, edx, edy, edw, edh);
            jan_mostrar_area(j, ex - 2, ey - 2, evl + 4, eva + 4);
        }
    }
}

static void tratar_soltar(int mx, int my)
{
    jb_raton_pulsado = false;
    if (dr.on) {
        soltar_arrastre(mx);
        pr.t = P_NADA;
        pal_reconstruir();
        pintar_todo();
        log_bloques();
        titulo_actualizar();
        return;
    }
    int t = pr.t;
    pr.t = P_NADA;
    if (pr.moved) return;
    pers_t *p = pers_cur();
    if (t == P_RANURA) {
        ed_.on = true; ed_.b = pr.h.b; ed_.i = pr.h.i; ed_.cambio = false;
        copia_texto(ed_.orig, ed_.b->a[ed_.i].t, sizeof ed_.orig);
        jb_hist_tomar();
        repintar_programa();
    } else if (t == P_MENU) {
        abrir_menu_valor(pr.h.b, pr.h.i, pr.h.x, pr.h.y + pr.h.h);
    } else if (t == P_BLOQUE || t == P_PALETA) {
        blk_t *b = pr.h.b;
        int tipo = jb_ops[b->op].tipo;
        if (p && tipo != K_REP && tipo != K_BOOL && tipo != K_HAT) {
            jb_motor_ejecutar(p, b);
            corriendo = true;
        } else if (p && tipo == K_HAT) {
            jb_motor_ejecutar(p, b);
            corriendo = true;
        }
    }
    (void)my;
}

static void tratar_tecla(unsigned t, unsigned mod)
{
    if (ed_.on) {
        char *s = ed_.b->a[ed_.i].t;
        int n = (int)tam_texto(s);
        char tipo = jb_tipo_ranura(ed_.b->op, ed_.i);
        if (t == '\n' || t == '\r' || t == '\t') { edicion_terminar(true); repintar_programa(); return; }
        if (t == TECLA_ESC) { edicion_terminar(false); repintar_programa(); return; }
        if (t == 8 || t == 127) { if (n > 0) { s[n - 1] = 0; ed_.cambio = true; } }
        else if (t >= 32 && t < 256 && t != 127 && n < 36) {
            if (tipo == '#' && !((t >= '0' && t <= '9') || t == '.' || t == ',' || t == '-')) return;
            s[n] = (char)(t == ',' && tipo == '#' ? '.' : t); s[n + 1] = 0; ed_.cambio = true;
        } else return;
        repintar_programa();
        return;
    }
    if (jb_hay_pregunta) {
        int n = (int)tam_texto(jb_respuesta);
        if (t == '\n' || t == '\r') jb_responder();
        else if (t == 8 || t == 127) { if (n > 0) jb_respuesta[n - 1] = 0; }
        else if (t >= 32 && t < 256 && n < 60) { jb_respuesta[n] = (char)t; jb_respuesta[n + 1] = 0; }
        jb_sucio = true;
        return;
    }
    if (mod & 2) {      /* Ctrl */
        switch (t) {
        case 14: accion_nuevo(); pintar_todo(); return;
        case 15: accion_abrir(); pintar_todo(); return;
        case 19: if (mod & 1) accion_guardar_como(); else accion_guardar(); pintar_todo(); return;
        case 26: if (jb_hist_hay_deshacer()) accion_deshacer(false); pintar_todo(); return;
        case 25: if (jb_hist_hay_rehacer()) accion_deshacer(true); pintar_todo(); return;
        default: break;
        }
    }
    if (t == TECLA_F11) { jan_tela_cheia(j, !a_pantalla_completa); return; }
    if (t == 0x13A /* F1 */ ) { accion_ayuda(); return; }
    jb_motor_tecla((int)t);
    if (jb_motor_activo()) corriendo = true;
}

static void tratar_menu(int idx, int codigo)
{
    pers_t *p = pers_cur();
    switch (codigo) {
    case 1: aplicar_valor(idx); break;
    case 2:
        if (idx == 0 && jb_ops[cm.b->op].tipo != K_REP && jb_ops[cm.b->op].tipo != K_BOOL) duplicar_bloque(cm.b, cm.g);
        else if ((idx == 1) || (idx == 0 && (jb_ops[cm.b->op].tipo == K_REP || jb_ops[cm.b->op].tipo == K_BOOL))) borrar_bloque(cm.b);
        break;
    case 3: accion_menu_personajes(idx); break;
    case 4:
        if (idx >= 0 && idx < n_ej && confirmar_perder()) { char c[200]; caminho_juntar(c, sizeof c, ejemplos_dir, ej_nombres[idx]); abrir_ruta(c); modo = MODO_PROG; }
        break;
    case 5:
        if (sp_menu < 0 || sp_menu >= jb_p.npers) break;
        if (idx == 0) {
            char n[24];
            copia_texto(n, jb_p.pers[sp_menu]->nombre, sizeof n);
            if (pedir_texto(_("Nombre"), _("¿Cómo se llama este personaje?"), n, sizeof n) && n[0]) { jb_hist_tomar(); copia_texto(jb_p.pers[sp_menu]->nombre, n, 24); modificado = true; }
        } else if (idx == 1 && jb_p.npers < MAX_PERS) {
            /* duplicar: se pasa por el texto del proyecto para copiar todo */
            jb_hist_tomar();
            pers_t *o = jb_p.pers[sp_menu];
            pers_t *c = jb_pers_nuevo(o->nombre);
            if (c) {
                char nm[24]; formatar_texto(nm, sizeof nm, "%.18s2", o->nombre); copia_texto(c->nombre, nm, sizeof c->nombre);
                c->x = o->x + 20 * U; c->y = o->y - 20 * U; c->dir = o->dir; c->tam = o->tam; c->visible = o->visible; c->giro = o->giro; c->traje = o->traje;
                for (int k = 0; k < o->ntrajes; k++) { traje_t *t = aloca(sizeof *t); if (t) { *t = *o->trajes[k]; c->trajes[c->ntrajes++] = t; } }
                for (int k = 0; k < o->nguiones; k++) { c->guiones[c->nguiones++] = (guion_t){ jb_blk_copiar(o->guiones[k].top, true), o->guiones[k].x, o->guiones[k].y }; }
                jb_pers_recalcular(c);
                cur = jb_p.npers - 1;
                modificado = true;
            }
        } else if (idx == 2) {
            if (jb_p.npers <= 1) { mensagem("Jabuti", _("Tiene que quedar al menos un personaje.")); break; }
            jb_hist_tomar();
            jb_motor_parar();
            jb_pers_borrar(sp_menu);
            if (cur >= jb_p.npers) cur = jb_p.npers - 1;
            modificado = true;
        }
        proyecto_cambiado();
        break;
    case 6:
        if (idx < 0) break;
        jb_hist_tomar();
        if (modo == MODO_TRAJES && p && p->ntrajes < MAX_TRAJES && idx < jb_biblioteca_n()) {
            traje_t *t = aloca(sizeof *t);
            if (t) { jb_biblioteca_traje(idx, 0, t); p->trajes[p->ntrajes] = t; te.sel = p->ntrajes++; p->traje = te.sel; }
        } else if (modo == MODO_FONDOS && jb_p.nfondos < MAX_FONDOS && idx < jb_fondos_n()) {
            fondo_t *f = aloca(sizeof *f);
            if (f) { jb_fondo_biblioteca(idx, f); jb_p.fondos[jb_p.nfondos] = f; te.sel = jb_p.nfondos++; jb_p.fondo = te.sel; }
        } else { jb_hist_descartar(); mensagem("Jabuti", _("Ya hay demasiados.")); break; }
        modificado = true; ed_recalcular();
        break;
    case 7:
        if (idx == 0 && p) {
            jb_hist_tomar();
            int y = 24;
            for (int g = 0; g < p->nguiones; g++) { int w, h; M = &MET[1]; medir_pila(p->guiones[g].top, &w, &h); p->guiones[g].x = 24; p->guiones[g].y = y + 12; y += h + 36; }
            area_sx = area_sy = 0;
            modificado = true;
        }
        break;
    default: break;
    }
    pintar_todo();
    titulo_actualizar();
    if (codigo == 4 || codigo == 3 || codigo == 5) log_bloques();
}

/* ============================================================ sin ventana (pruebas) */

/* mide lo que cuesta un cuadro en esta máquina: el intérprete y el dibujo del escenario a tres tamaños */
static int medir_rendimiento(const char *fich)
{
    jb_proy_vaciar();
    if (fich) { if (!jb_abrir(fich)) { imprimir("jabuti: error al abrir %s: %s\n", fich, jb_p.error); return 1; } }
    else {
        /* seis personajes, cada uno con un «por siempre» que lo mueve, lo gira y lo hace rebotar */
        static char t[2400];
        copia_texto(t, "JABUTI 1\nfondo 1\nf \"Blanco\" biblioteca blanco\nf \"Cielo\" biblioteca cielo\n", sizeof t);
        static const char *const cl[6] = { "sagui", "pelota", "cohete", "estrella", "manzana", "pez" };
        for (int i = 0; i < 6; i++) {
            char l[300];
            formatar_texto(l, sizeof l, "personaje \"P%d\" biblioteca %s\n  pos %d %d %d 100 1 0 0\n  guion 10 10\n    bandera\n    siempre\n      mover 4\n      girar-der 7\n      rebotar\n",
                           i, cl[i], (i - 3) * 60, (i % 3 - 1) * 50, 30 + i * 50);
            junta_texto(t, l, sizeof t);
        }
        if (!jb_leer_texto(t, (unsigned)tam_texto(t))) { imprimir("jabuti: error en la prueba: %s\n", jb_p.error); return 1; }
    }
    static uint32_t buf[480 * 360];
    static const int tam[3] = { 160, 240, 480 };
    unsigned t0;
    int n = 400;
    jb_motor_iniciar();
    jb_motor_bandera();
    t0 = tempo_ms();
    for (int c = 1; c <= n * 3; c++) jb_motor_cuadro((unsigned)c * 33);   /* 1200 cuadros de intérprete */
    unsigned dt = tempo_ms() - t0;
    imprimir("jabuti: medida intérprete %d cuadros en %u ms (%u us por cuadro, %d hilos, %d personajes)\n", n * 3, dt, dt * 1000 / (unsigned)(n * 3), jb_motor_hilos(), jb_p.npers);
    for (int k = 0; k < 3; k++) {
        int vl = tam[k], va = vl * 3 / 4;
        t0 = tempo_ms();
        for (int c = 0; c < n; c++) jb_escena_dibujar(buf, vl, vl, va);
        dt = tempo_ms() - t0;
        imprimir("jabuti: medida escenario %dx%d con %d personajes: %u us por cuadro\n", vl, va, jb_p.npers, dt * 1000 / (unsigned)n);
    }
    jb_motor_parar();
    imprimir("jabuti: medida ok\n");
    return 0;
}

static int nombre_a_tecla(const char *n)
{
    if (compara_texto(n, "espacio") == 0) return ' ';
    if (compara_texto(n, "flecha arriba") == 0 || compara_texto(n, "arriba") == 0) return TECLA_CIMA;
    if (compara_texto(n, "flecha abajo") == 0 || compara_texto(n, "abajo") == 0) return TECLA_BAIXO;
    if (compara_texto(n, "flecha izquierda") == 0 || compara_texto(n, "izquierda") == 0) return TECLA_ESQUERDA;
    if (compara_texto(n, "flecha derecha") == 0 || compara_texto(n, "derecha") == 0) return TECLA_DIREITA;
    if (compara_texto(n, "Intro") == 0) return '\n';
    return n[0];
}

static int sin_ventana(int argc, char **argv)
{
    const char *fich = NULL, *guardar = NULL, *resp = NULL, *importar = NULL;
    int cuadros = 90;
    bool ejecutar = false, texto_ = false, medir_ = false;
    struct { int cuadro; char nombre[24]; int x, y; bool clic; } evs[16];
    int nev = 0;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--ejecutar") == 0 && i + 1 < argc) { fich = argv[++i]; ejecutar = true; }
        else if (compara_texto(argv[i], "--abrir") == 0 && i + 1 < argc) fich = argv[++i];
        else if (compara_texto(argv[i], "--cuadros") == 0 && i + 1 < argc) { int32_t v; cuadros = jb_num_parse(argv[++i], &v) ? v / U : 90; }
        else if (compara_texto(argv[i], "--guardar") == 0 && i + 1 < argc) guardar = argv[++i];
        else if (compara_texto(argv[i], "--respuesta") == 0 && i + 1 < argc) resp = argv[++i];
        else if (compara_texto(argv[i], "--texto") == 0) texto_ = true;
        else if (compara_texto(argv[i], "--importar") == 0 && i + 1 < argc) importar = argv[++i];
        else if (compara_texto(argv[i], "--medir") == 0) medir_ = true;
        else if ((compara_texto(argv[i], "--tecla") == 0 || compara_texto(argv[i], "--clic") == 0) && i + 1 < argc && nev < 16) {
            bool clic = compara_texto(argv[i], "--clic") == 0;
            const char *s = argv[++i];
            const char *at = acha_ultimo_char(s, '@');
            evs[nev].clic = clic; evs[nev].x = evs[nev].y = 0; evs[nev].cuadro = 1;
            char nom[24];
            int n = at ? (int)(at - s) : (int)tam_texto(s);
            if (n > 23) n = 23;
            memcpy(nom, s, (size_t)n); nom[n] = 0;
            if (at) { int32_t v; if (jb_num_parse(at + 1, &v)) evs[nev].cuadro = v / U; }
            if (clic) { const char *co = acha_char(nom, ','); if (co) { int32_t a, b; char t1[12]; copia_texto(t1, nom, (size_t)(co - nom + 1)); if (jb_num_parse(t1, &a) && jb_num_parse(co + 1, &b)) { evs[nev].x = a / U; evs[nev].y = b / U; } } }
            else if (compara_texto(nom, "arriba") == 0 || compara_texto(nom, "abajo") == 0 || compara_texto(nom, "izquierda") == 0 || compara_texto(nom, "derecha") == 0)
                formatar_texto(evs[nev].nombre, sizeof evs[nev].nombre, "flecha %s", nom);
            else copia_texto(evs[nev].nombre, nom, sizeof evs[nev].nombre);
            nev++;
        }
    }
    if (medir_) return medir_rendimiento(fich);
    if (!fich && importar) { jb_proy_nuevo(); }
    else if (!fich) { imprimir("uso: jabuti --ejecutar proyecto.jbt [--cuadros N] [--tecla NOMBRE@CUADRO] [--clic X,Y@CUADRO] [--respuesta TEXTO] [--guardar salida.jbt]\n"); return 1; }
    if (fich) {
        jb_proy_vaciar();
        if (!jb_abrir(fich)) { imprimir("jabuti: error al abrir %s: %s\n", fich, jb_p.error); return 1; }
    }
    if (importar) {
        imagem_t img;
        int r = imagem_ler(importar, &img);
        if (r != 0) { imprimir("jabuti: no he podido leer la imagen %s: %s\n", importar, imagem_erro(r)); return 1; }
        traje_t *t = jb_traje_nuevo("importado");
        fondo_t *f = aloca_zerado(sizeof *f);
        pers_t *p = jb_pers_nuevo("Importado");
        if (!t || !f || !p || jb_p.nfondos >= MAX_FONDOS) { imprimir("jabuti: sin memoria\n"); return 1; }
        importar_en(t->px, TRAJE_L, TRAJE_L, &img, true);
        importar_en(f->px, FONDO_L, FONDO_A, &img, false);
        copia_texto(f->nombre, "importado", sizeof f->nombre);
        p->trajes[p->ntrajes++] = t; jb_pers_recalcular(p);
        jb_p.fondos[jb_p.nfondos++] = f;
        int opacos = 0, colores = 0;
        bool usado[32] = { false };
        for (int i = 0; i < TRAJE_L * TRAJE_L; i++) if (t->px[i]) { opacos++; if (!usado[t->px[i]]) { usado[t->px[i]] = true; colores++; } }
        imprimir("jabuti: importada %s (%dx%d): traje con %d píxeles opacos de %d colores, fondo de %dx%d\n", importar, img.larg, img.alt, opacos, colores, FONDO_L, FONDO_A);
        imagem_liberar(&img);
    }
    int ng = 0;
    for (int i = 0; i < jb_p.npers; i++) ng += jb_p.pers[i]->nguiones;
    imprimir("jabuti: abierto %s (%d personajes, %d guiones, %d fondos)\n", fich ? fich : "(nuevo)", jb_p.npers, ng, jb_p.nfondos);
    if (texto_) { unsigned n; char *t = jb_escribir_texto(&n); if (t) { escrever_tudo(SAIDA, t, n); libera(t); } }
    if (ejecutar) {
        jb_motor_iniciar();
        jb_auto_resp = resp;
        jb_motor_bandera();
        int usados = 0;
        for (int c = 1; c <= cuadros; c++) {
            for (int k = 0; k < nev; k++) if (evs[k].cuadro == c) {
                if (evs[k].clic) jb_motor_clic(evs[k].x, evs[k].y);
                else { jb_tecla_virtual(evs[k].nombre, 600); jb_motor_tecla(nombre_a_tecla(evs[k].nombre)); }
            }
            jb_motor_cuadro((unsigned)c * 33);
            usados = c;
            if (!jb_motor_activo() && c > 2 && nev == 0) break;
        }
        imprimir("jabuti: cuadros %d hilos %d\n", usados, jb_motor_hilos());
        for (int i = 0; i < jb_p.npers; i++) {
            pers_t *p = jb_p.pers[i];
            char x[24], y[24], d[24];
            jb_num_texto(p->x, x, sizeof x); jb_num_texto(p->y, y, sizeof y); jb_num_texto(dir_visible(p->dir), d, sizeof d);
            imprimir("jabuti: personaje %s x=%s y=%s dir=%s tam=%d vis=%d traje=%d decir=%s\n", p->nombre, x, y, d, p->tam, p->visible ? 1 : 0, p->traje, p->burbuja);
        }
        for (int i = 0; i < jb_p.nvars; i++) {
            char v[48];
            if (jb_p.vars[i].es_t) copia_texto(v, jb_p.vars[i].t, sizeof v); else jb_num_texto(jb_p.vars[i].n, v, sizeof v);
            imprimir("jabuti: variable %s = %s\n", jb_p.vars[i].nombre, v);
        }
        for (int i = 0; i < jb_p.nlistas; i++) imprimir("jabuti: lista %s (%d elementos)\n", jb_p.listas[i].nombre, jb_p.listas[i].n);
        if (jb_aviso[0]) imprimir("jabuti: aviso %s\n", jb_aviso);
        jb_motor_parar();
    }
    if (guardar) { if (jb_guardar(guardar)) imprimir("jabuti: guardado %s\n", guardar); else { imprimir("jabuti: no se pudo guardar %s\n", guardar); return 1; } }
    imprimir(ejecutar ? "jabuti: ejecucion ok\n" : "jabuti: abrir ok\n");
    return 0;
}

/* ============================================================ principal */

int principal(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
        if (compara_texto(argv[i], "--ejecutar") == 0 || compara_texto(argv[i], "--abrir") == 0 || compara_texto(argv[i], "--importar") == 0 || compara_texto(argv[i], "--medir") == 0) return sin_ventana(argc, argv);
    j = jan_abrir("Jabuti", 780, 520);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK, 640, 470);
    jb_log = true;
    jb_proy_nuevo();
    jb_motor_iniciar();
    if (argc >= 2 && argv[1][0] != '-') abrir_ruta(argv[1]);
    pal_reconstruir();
    dizer(_("Arrastra bloques de la paleta al área de programa y pulsa la bandera verde."));
    pintar_todo();
    titulo_actualizar();
    log_geometria();
    log_bloques();
    log_estado();
    registrar("jabuti: pronto\n");

    unsigned proximo = tempo_ms() + 33;
    bool estaba_activo = false;
    evento_t e;
    for (;;) {
        bool activo = jb_motor_activo();
        int espera = -1;
        if (activo || corriendo || jb_hay_pregunta) { int d = (int)(proximo - tempo_ms()); espera = d < 0 ? 0 : d; }
        int r = esperar_evento(j->id, &e, espera);
        if (r > 0) {
            if (e.tipo == EV_FECHAR) { if (cerrar_ok()) break; continue; }
            if (e.tipo == EV_REDIMENSIONAR) {
                if (jan_redimensionar(j, &e)) { a_pantalla_completa = e.tecla == JAN_ESTADO_TELA_CHEIA; pintar_todo(); registrar("jabuti: pantalla completa %d (%dx%d)\n", a_pantalla_completa ? 1 : 0, j->larg, j->alt); if (!a_pantalla_completa) log_geometria(); }
                continue;
            }
            if (e.tipo == EV_MENU) { tratar_menu(e.x, (int)e.tecla); continue; }
            if (e.tipo == EV_CLIQUE) tratar_clic(e.x, e.y, e.botoes, e.cliques);
            else if (e.tipo == EV_MOVER) tratar_mover(e.x, e.y, e.botoes);
            else if (e.tipo == EV_SOLTOU) tratar_soltar(e.x, e.y);
            else if (e.tipo == EV_RODA) {
                int n = (int)e.cliques;
                if (modo == MODO_PROG && en_rect(e.x, e.y, px_, py_, pw_, ph_)) {
                    pal_scroll -= n * 40;
                    int max = pal_alto - ph_;
                    if (pal_scroll > max) pal_scroll = max;
                    if (pal_scroll < 0) pal_scroll = 0;
                    dibujar_paleta(); jan_mostrar_area(j, px_, py_, pw_, ph_);
                } else if (modo == MODO_PROG && en_rect(e.x, e.y, ax, ay, aw, ah)) {
                    area_sy -= n * 40;
                    if (area_sy < -100) area_sy = -100;
                    repintar_programa();
                }
            } else if (e.tipo == EV_TECLA) tratar_tecla(e.tecla, e.modificadores);
            if (nha == 0 && modo == MODO_PROG) { /* aún sin guiones: nada */ }
            if (activo || jb_sucio) { /* se repinta con el cuadro */ }
        }
        unsigned t = tempo_ms();
        if ((jb_motor_activo() || corriendo || jb_hay_pregunta) && t >= proximo) {
            proximo = t + 33;
            teclas_seguradas(jb_teclas_agarradas);
            jb_motor_cuadro(t);
            if (jb_sucio && !dr.on) { jb_sucio = false; if (a_pantalla_completa) { dibujar_escenario(); jan_mostrar(j); } else repintar_escenario(); }
            activo = jb_motor_activo();
            if (estaba_activo && !activo && !jb_hay_pregunta) {
                corriendo = false;
                registrar("jabuti: fin\n");
                log_estado();
                dizer(_("Terminado."));
                if (!a_pantalla_completa) { dibujar_barra(); jan_mostrar_area(j, 0, 0, W, TOPH); repintar_estado(); dibujar_personajes(); jan_mostrar_area(j, spx - 2, spy, spw + 4, sph + 4); }
            }
            estaba_activo = activo;
            if (jb_aviso[0] && !a_pantalla_completa) repintar_estado();
        } else if (jb_sucio && !dr.on) {
            jb_sucio = false;
            if (a_pantalla_completa) { dibujar_escenario(); jan_mostrar(j); } else repintar_escenario();
        }
    }
    jb_motor_parar();
    jan_fechar(j);
    registrar("jabuti: fechado\n");
    return 0;
}
