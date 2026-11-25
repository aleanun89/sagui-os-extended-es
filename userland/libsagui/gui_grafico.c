// SPDX-License-Identifier: GPL-3.0-only
/* gui_grafico.c - componente de gráficos de la libsagui (v8.5.6, D469; API en userland/inc/grafico.h).
 *
 * El gráfico es un objeto con estado (tipo, series, categorías, opciones, animación, ratón) que se dibuja
 * entero sobre un búfer de píxeles 0x00RRGGBB (el de una jan_t, el de una sagui.Imagen de MicroPython o uno
 * temporal para exportar). No toca el kernel ni las ventanas: quien lo usa le pasa el tiempo
 * (grafico_avanzar), los eventos del ratón (grafico_mover, grafico_pulsar) y el búfer (grafico_dibujar).
 *
 * Los colores salen del tema del sistema (mod_tema: fondo, texto, bordes) y, para las series, de una
 * paleta categórica propia con variante oscura (gr_color_serie). El texto usa la fuente de mapa de bits
 * del sistema (8x16, Latin-1). Los bordes de líneas, discos y sectores se suavizan con cobertura
 * analítica por píxel (sin muestreo múltiple).
 *
 * Animación: dos progresos independientes, p_ent (entrada: las barras crecen, las líneas se dibujan de
 * izquierda a derecha, la tarta se abre) y p_cam (cambio de datos: cada valor y la escala pasan del valor
 * que se veía al nuevo). Ambos avanzan por el reloj que pasa quien llama; nada duerme ni bloquea.
 *
 * Esta unidad usa coma flotante: se compila con la x87 (ver el Makefile), como el ecualizador. */
#include "gui.h"
#include "grafico.h"

extern const unsigned char fonte_sagui[256][16];

#define SUB       8                       /* subdivisiones por tramo de una curva suave */
#define HOT_MAX   1024
#define SCR_N     (GR_MAX_PUNTOS * SUB + 16)
#define SCR_TOT   (3 * GR_MAX_PUNTOS + 5 * SCR_N)   /* px, py, qy, ox, oy, bx, by, base */
#define LET_L     8
#define LET_A     16
#define MARGEN    8

typedef struct { double *y, *x, *t, *y0; int n, cap; char nombre[GR_MAX_ETQ]; } gr_serie_t;
typedef struct { int x, y, w, h; } gr_rec_t;
typedef struct { unsigned char k; short s, i; double a, b, c, d, e, f; } gr_hot_t;   /* k: 0 rect(a..d) 1 círculo(a,b,c) 2 sector(a..f) */

struct grafico {
    int tipo;
    char titulo[64], etq_x[32], etq_y[32];
    gr_serie_t s[GR_MAX_SERIES];
    int ns;
    char cat[GR_MAX_PUNTOS][GR_MAX_ETQ];
    int ncat;
    unsigned oculto;
    unsigned col_item[GR_MAX_LEY];
    int ley, rejilla, animado, apilado, suave, puntos, duracion, valores, hueco, bins, ventana, cero, ease, fondo;
    int rango_auto;
    double rmin, rmax;
    gr_fmt_t fmt;
    /* animación */
    unsigned ahora, t_ent0, t_cam0;
    int ent_pend, cam_pend, cam_activa;
    double p_ent, p_cam;
    double esc_a_min, esc_a_max;       /* escala que se veía al empezar el último cambio */
    double esc_v_min, esc_v_max;       /* escala del último dibujo */
    int esc_v_ok, esc_a_ok;            /* esc_a_ok: hay una escala de la que partir (ya se dibujó antes del cambio) */
    double desp;                       /* desplazamiento pendiente de una ventana deslizante (en tramos) */
    int sucio;
    /* ratón y resaltado */
    int hs, hi, mx, my, dentro;
    char tip[256];
    gr_rec_t ley_r[GR_MAX_LEY];
    int ley_n;
    gr_hot_t hot[HOT_MAX];
    int nhot;
    /* histograma (último dibujo) */
    int hb_n, hb_cnt[64];
    double hb_lo, hb_paso;
    /* memoria de trabajo (se pide al primer dibujo) */
    double *scr;                       /* SCR_TOT: puntos de pantalla y polilíneas densas */
    double *tmp;                       /* 3 * GR_MAX_SERIES * GR_MAX_PUNTOS: valores, bases y cimas */
};

/* ------------------------------------------------------------ contexto de dibujo */

typedef struct {
    unsigned *px;
    int w, h;
    int cx0, cy0, cx1, cy1;            /* recorte */
    unsigned fondo, texto, fraco, rej, eje, tipf, tipt;
    int oscuro;
} cx_t;

static inline double mn(double a, double b) { return a < b ? a : b; }
static inline double mx(double a, double b) { return a > b ? a : b; }
static inline double fabs_(double a) { return a < 0 ? -a : a; }
static inline double sq(double a) { double r; __asm__("fsqrt" : "=t"(r) : "0"(a)); return r; }   /* x87: sin libm */
static inline double c01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void scopy(char *d, const char *s, int max) { int i = 0; while (s && s[i] && i < max - 1) { d[i] = s[i]; i++; } d[i] = 0; }
static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

static inline void mezcla(cx_t *c, int x, int y, unsigned col, unsigned a)
{
    if (x < c->cx0 || x >= c->cx1 || y < c->cy0 || y >= c->cy1 || a == 0) return;
    unsigned *p = &c->px[y * c->w + x];
    if (a >= 255) { *p = col & 0xFFFFFF; return; }
    unsigned d = *p, ia = 255 - a;
    unsigned r = (((d >> 16) & 255) * ia + ((col >> 16) & 255) * a + 127) / 255;
    unsigned g = (((d >> 8) & 255) * ia + ((col >> 8) & 255) * a + 127) / 255;
    unsigned b = ((d & 255) * ia + (col & 255) * a + 127) / 255;
    *p = (r << 16) | (g << 8) | b;
}

static inline unsigned alfa_de(double cov, unsigned alfa)
{
    if (cov <= 0) return 0;
    if (cov >= 1) return alfa;
    return (unsigned)(cov * alfa + 0.5);
}

static void rect(cx_t *c, int x, int y, int w, int h, unsigned col, unsigned a)
{
    int x0 = imax(x, c->cx0), y0 = imax(y, c->cy0), x1 = imin(x + w, c->cx1), y1 = imin(y + h, c->cy1);
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++) mezcla(c, i, j, col, a);
}

/* Rectángulo con bordes de media cobertura (barras que crecen con posición no entera). */
static void rect_f(cx_t *c, double x, double y, double w, double h, unsigned col, unsigned a)
{
    if (w <= 0 || h <= 0) return;
    double x1 = x + w, y1 = y + h;
    int ix0 = (int)gr_floor(x), ix1 = (int)gr_ceil(x1), iy0 = (int)gr_floor(y), iy1 = (int)gr_ceil(y1);
    ix0 = imax(ix0, c->cx0); iy0 = imax(iy0, c->cy0); ix1 = imin(ix1, c->cx1); iy1 = imin(iy1, c->cy1);
    for (int j = iy0; j < iy1; j++) {
        double cy = mn(y1, j + 1) - mx(y, j);
        for (int i = ix0; i < ix1; i++) {
            double cxv = mn(x1, i + 1) - mx(x, i);
            mezcla(c, i, j, col, alfa_de(cxv * cy, a));
        }
    }
}

static void marco(cx_t *c, int x, int y, int w, int h, unsigned col)
{
    rect(c, x, y, w, 1, col, 255);
    rect(c, x, y + h - 1, w, 1, col, 255);
    rect(c, x, y, 1, h, col, 255);
    rect(c, x + w - 1, y, 1, h, col, 255);
}

static void disco(cx_t *c, double cx, double cy, double r, unsigned col, unsigned a)
{
    int x0 = imax((int)gr_floor(cx - r - 1), c->cx0), x1 = imin((int)gr_ceil(cx + r + 1), c->cx1);
    int y0 = imax((int)gr_floor(cy - r - 1), c->cy0), y1 = imin((int)gr_ceil(cy + r + 1), c->cy1);
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
            mezcla(c, x, y, col, alfa_de(r + 0.5 - sq(dx * dx + dy * dy), a));
        }
}

static void aro(cx_t *c, double cx, double cy, double r, double ancho, unsigned col, unsigned a)
{
    double hw = ancho / 2, ext = r + hw + 1;
    int x0 = imax((int)gr_floor(cx - ext), c->cx0), x1 = imin((int)gr_ceil(cx + ext), c->cx1);
    int y0 = imax((int)gr_floor(cy - ext), c->cy0), y1 = imin((int)gr_ceil(cy + ext), c->cy1);
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
            mezcla(c, x, y, col, alfa_de(hw + 0.5 - fabs_(sq(dx * dx + dy * dy) - r), a));
        }
}

/* Segmento con grosor w y extremos redondos. */
static void linea(cx_t *c, double x0, double y0, double x1, double y1, double w, unsigned col, unsigned a)
{
    double dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy, hw = w / 2, r = hw + 1.5;
    if (l2 < 1e-9) { disco(c, x0, y0, hw, col, a); return; }
    int pend = fabs_(dy) > fabs_(dx);          /* casi vertical: se recorren filas */
    double p0 = pend ? y0 : x0, p1 = pend ? y1 : x1, q0 = pend ? x0 : y0, q1 = pend ? x1 : y1;
    double dp = p1 - p0, dq = q1 - q0;
    int pa = (int)gr_floor(mn(p0, p1) - r), pb = (int)gr_ceil(mx(p0, p1) + r);
    int lim_p0 = pend ? c->cy0 : c->cx0, lim_p1 = pend ? c->cy1 : c->cx1;
    int lim_q0 = pend ? c->cx0 : c->cy0, lim_q1 = pend ? c->cx1 : c->cy1;
    if (pa < lim_p0) pa = lim_p0;
    if (pb > lim_p1) pb = lim_p1;
    for (int P = pa; P < pb; P++) {
        double pc = P + 0.5, ta = c01((pc - 0.5 - p0) / dp), tb = c01((pc + 0.5 - p0) / dp);
        double qa = q0 + dq * ta, qb = q0 + dq * tb;
        int qs = (int)gr_floor(mn(qa, qb) - r), qe = (int)gr_ceil(mx(qa, qb) + r);
        if (qs < lim_q0) qs = lim_q0;
        if (qe > lim_q1) qe = lim_q1;
        for (int Q = qs; Q < qe; Q++) {
            double px_ = pend ? Q + 0.5 : pc, py_ = pend ? pc : Q + 0.5;
            double t = c01(((px_ - x0) * dx + (py_ - y0) * dy) / l2);
            double ex = px_ - (x0 + t * dx), ey = py_ - (y0 + t * dy);
            unsigned al = alfa_de(hw + 0.5 - sq(ex * ex + ey * ey), a);
            if (al) mezcla(c, pend ? Q : P, pend ? P : Q, col, al);
        }
    }
}

/* Porción de corona entre los radios r0 y r1 y los ángulos a0..a1 (0 = las 12, sentido horario). */
static void sector(cx_t *c, double cx, double cy, double r0, double r1, double a0, double a1, unsigned col, unsigned a)
{
    double span = a1 - a0;
    if (span <= 0 || r1 <= 0) return;
    double d0x, d0y, d1x, d1y;
    gr_dir(a0, &d0x, &d0y);
    gr_dir(a1, &d1x, &d1y);
    int x0 = imax((int)gr_floor(cx - r1 - 1), c->cx0), x1 = imin((int)gr_ceil(cx + r1 + 1), c->cx1);
    int y0 = imax((int)gr_floor(cy - r1 - 1), c->cy0), y1 = imin((int)gr_ceil(cy + r1 + 1), c->cy1);
    int entero = span >= 6.283185307179586 - 1e-9;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            double dx = x + 0.5 - cx, dy = y + 0.5 - cy, r = sq(dx * dx + dy * dy);
            double cr = c01(r1 + 0.5 - r);
            if (r0 > 0) cr *= c01(r - r0 + 0.5);
            if (cr <= 0) continue;
            double ca = 1;
            if (!entero) {
                double s0 = d0x * dy - d0y * dx, s1 = d1x * dy - d1y * dx;    /* >0 después de a0; <0 antes de a1 */
                if (span <= 3.141592653589793) ca = c01(s0 + 0.5) * c01(-s1 + 0.5);
                else ca = 1 - c01(s1 + 0.5) * c01(-s0 + 0.5);
            }
            mezcla(c, x, y, col, alfa_de(cr * ca, a));
        }
}

/* Relleno entre dos polilíneas con las mismas x (crecientes): columna a columna. */
static void area_entre(cx_t *c, const double *xs, const double *yt, const double *yb, int n, unsigned col, unsigned a)
{
    if (n < 2) return;
    int j = 0;
    int xa = imax((int)gr_ceil(xs[0] - 0.5), c->cx0), xb = imin((int)gr_floor(xs[n - 1] + 0.5), c->cx1 - 1);
    for (int X = xa; X <= xb; X++) {
        double xc = X + 0.5;
        if (xc < xs[0]) xc = xs[0];
        if (xc > xs[n - 1]) xc = xs[n - 1];
        while (j < n - 2 && xs[j + 1] < xc) j++;
        double dx = xs[j + 1] - xs[j];
        double t = dx > 1e-9 ? c01((xc - xs[j]) / dx) : 0;
        double a1 = yt[j] + (yt[j + 1] - yt[j]) * t, b1 = yb[j] + (yb[j + 1] - yb[j]) * t;
        if (a1 > b1) { double q = a1; a1 = b1; b1 = q; }
        int ys = imax((int)gr_floor(a1), c->cy0), ye = imin((int)gr_ceil(b1), c->cy1);
        for (int Y = ys; Y < ye; Y++) {
            double cov = mn(b1, Y + 1) - mx(a1, Y);
            mezcla(c, X, Y, col, alfa_de(cov, a));
        }
    }
}

static void texto(cx_t *c, int x, int y, const char *s, unsigned col)
{
    for (; *s; s++, x += LET_L) {
        if (x + LET_L <= c->cx0 || x >= c->cx1) continue;
        const unsigned char *g = fonte_sagui[(unsigned char)*s];
        for (int l = 0; l < LET_A; l++) {
            unsigned char b = g[l];
            if (!b) continue;
            for (int k = 0; k < 8; k++)
                if (b & (0x80 >> k)) mezcla(c, x + k, y + l, col, 255);
        }
    }
}

static void texto_max(cx_t *c, int x, int y, const char *s, int maxw, unsigned col)
{
    char b[96];
    int n = imin(slen(s), imin(maxw / LET_L, 95));
    for (int i = 0; i < n; i++) b[i] = s[i];
    b[n] = 0;
    texto(c, x, y, b, col);
}

static unsigned mezclar_col(unsigned a, unsigned b, unsigned t) { return mod_mezclar(a, b, t); }

/* ------------------------------------------------------------ ciclo de vida y datos */

static void defecto(grafico_t *g)
{
    g->tipo = GR_LINEAS;
    g->ley = GR_LEY_ABAJO; g->rejilla = 1; g->animado = 1; g->apilado = 0; g->suave = 0; g->puntos = 2;
    g->duracion = 450; g->valores = 0; g->hueco = 55; g->bins = 0; g->ventana = 0; g->cero = 1; g->ease = GR_EASE_SALIDA; g->fondo = 1;
    g->rango_auto = 1;
    gr_fmt_parse("auto", &g->fmt);
    g->ent_pend = 1; g->p_ent = 0; g->p_cam = 1;
    g->hs = g->hi = -1;
    g->sucio = 1;
}

grafico_t *grafico_nuevo(void)
{
    grafico_t *g = aloca_zerado(sizeof *g);
    if (g) defecto(g);
    return g;
}

static void serie_liberar(gr_serie_t *s)
{
    if (s->y) libera(s->y);
    if (s->x) libera(s->x);
    if (s->t) libera(s->t);
    if (s->y0) libera(s->y0);
    s->y = s->x = s->t = s->y0 = 0;
    s->n = s->cap = 0;
}

void grafico_liberar(grafico_t *g)
{
    if (!g) return;
    for (int i = 0; i < g->ns; i++) serie_liberar(&g->s[i]);
    if (g->scr) libera(g->scr);
    if (g->tmp) libera(g->tmp);
    libera(g);
}

void grafico_tipo(grafico_t *g, int tipo)
{
    if (tipo < 0 || tipo >= GR_NTIPOS || tipo == g->tipo) return;
    g->tipo = tipo;
    g->oculto = 0;
    g->hs = g->hi = -1;
    grafico_reiniciar(g);
}
int  grafico_tipo_actual(const grafico_t *g) { return g->tipo; }
void grafico_titulo(grafico_t *g, const char *t) { scopy(g->titulo, t, sizeof g->titulo); g->sucio = 1; }
void grafico_etiquetas_ejes(grafico_t *g, const char *x, const char *y) { scopy(g->etq_x, x, sizeof g->etq_x); scopy(g->etq_y, y, sizeof g->etq_y); g->sucio = 1; }
void grafico_formato(grafico_t *g, const char *spec) { char d = g->fmt.dec, m = g->fmt.mil; gr_fmt_parse(spec, &g->fmt); g->fmt.dec = d; g->fmt.mil = m; g->sucio = 1; }
void grafico_separadores(grafico_t *g, char dec, char mil) { g->fmt.dec = dec; g->fmt.mil = mil; g->sucio = 1; }
void grafico_rango(grafico_t *g, int automatico, double min, double max) { g->rango_auto = automatico != 0; g->rmin = min; g->rmax = max; g->sucio = 1; }

void grafico_opcion(grafico_t *g, int o, double v)
{
    int i = (int)(v < 0 ? v - 0.5 : v + 0.5);
    switch (o) {
    case GR_O_LEYENDA: g->ley = i < 0 || i > GR_LEY_IZQUIERDA ? GR_LEY_ABAJO : i; break;
    case GR_O_REJILLA: g->rejilla = i != 0; break;
    case GR_O_ANIMADO: g->animado = i != 0; if (!g->animado) { g->p_ent = g->p_cam = 1; g->ent_pend = g->cam_pend = g->cam_activa = 0; } break;
    case GR_O_APILADO: g->apilado = i != 0; break;
    case GR_O_SUAVE: g->suave = i != 0; break;
    case GR_O_PUNTOS: g->puntos = i < 0 ? 2 : i != 0; break;
    case GR_O_DURACION: g->duracion = i < 1 ? 1 : i; break;
    case GR_O_VALORES: g->valores = i != 0; break;
    case GR_O_HUECO: g->hueco = i < 0 ? 0 : i > 90 ? 90 : i; break;
    case GR_O_BINS: g->bins = i < 0 ? 0 : i > 64 ? 64 : i; break;
    case GR_O_VENTANA: g->ventana = i < 0 ? 0 : i > GR_MAX_PUNTOS ? GR_MAX_PUNTOS : i; break;
    case GR_O_CERO: g->cero = i != 0; break;
    case GR_O_EASE: g->ease = i < 0 || i > GR_EASE_AMBAS ? GR_EASE_SALIDA : i; break;
    case GR_O_FONDO: g->fondo = i != 0; break;
    default: return;
    }
    g->sucio = 1;
}

double grafico_opcion_leer(const grafico_t *g, int o)
{
    switch (o) {
    case GR_O_LEYENDA: return g->ley;
    case GR_O_REJILLA: return g->rejilla;
    case GR_O_ANIMADO: return g->animado;
    case GR_O_APILADO: return g->apilado;
    case GR_O_SUAVE: return g->suave;
    case GR_O_PUNTOS: return g->puntos;
    case GR_O_DURACION: return g->duracion;
    case GR_O_VALORES: return g->valores;
    case GR_O_HUECO: return g->hueco;
    case GR_O_BINS: return g->bins;
    case GR_O_VENTANA: return g->ventana;
    case GR_O_CERO: return g->cero;
    case GR_O_EASE: return g->ease;
    case GR_O_FONDO: return g->fondo;
    }
    return 0;
}

void grafico_categorias_limpiar(grafico_t *g) { g->ncat = 0; g->sucio = 1; }

int grafico_categoria_agregar(grafico_t *g, const char *n)
{
    if (g->ncat >= GR_MAX_PUNTOS) return -1;
    scopy(g->cat[g->ncat], n, GR_MAX_ETQ);
    g->sucio = 1;
    return g->ncat++;
}
int grafico_n_categorias(const grafico_t *g) { return g->ncat; }

int grafico_agregar_serie(grafico_t *g, const char *nombre, unsigned color)
{
    if (g->ns >= GR_MAX_SERIES) return -1;
    gr_serie_t *s = &g->s[g->ns];
    scopy(s->nombre, nombre, sizeof s->nombre);
    g->col_item[g->ns] = color & 0xFFFFFF;
    g->sucio = 1;
    return g->ns++;
}

void grafico_quitar_series(grafico_t *g)
{
    for (int i = 0; i < g->ns; i++) serie_liberar(&g->s[i]);
    g->ns = 0;
    g->oculto = 0;
    for (int i = 0; i < GR_MAX_LEY; i++) g->col_item[i] = 0;
    g->hs = g->hi = -1;
    g->sucio = 1;
}

int grafico_n_series(const grafico_t *g) { return g->ns; }
int grafico_serie_color(const grafico_t *g, int s, unsigned color)
{
    if (s < 0 || s >= GR_MAX_LEY) return -1;
    ((grafico_t *)g)->col_item[s] = color & 0xFFFFFF;
    ((grafico_t *)g)->sucio = 1;
    return 0;
}
int grafico_serie_nombre(grafico_t *g, int s, const char *nombre)
{
    if (s < 0 || s >= g->ns) return -1;
    scopy(g->s[s].nombre, nombre, GR_MAX_ETQ);
    g->sucio = 1;
    return 0;
}

static int serie_reservar(gr_serie_t *s, int n)
{
    if (n <= s->cap) return 0;
    int cap = s->cap ? s->cap : 16;
    while (cap < n) cap *= 2;
    if (cap > GR_MAX_PUNTOS) cap = GR_MAX_PUNTOS;
    if (n > cap) return -1;
    double **v[4] = { &s->y, &s->x, &s->t, &s->y0 };
    for (int k = 0; k < 4; k++) {
        double *nuevo = aloca_zerado((size_t)cap * sizeof(double));
        if (!nuevo) return -1;
        if (*v[k]) { for (int i = 0; i < s->n; i++) nuevo[i] = (*v[k])[i]; libera(*v[k]); }
        *v[k] = nuevo;
    }
    s->cap = cap;
    return 0;
}

/* Valor que se ve ahora de un punto (entre el anterior y el nuevo según p_cam). */
static double vd(const gr_serie_t *s, int i, double p) { return s->y0[i] + (s->y[i] - s->y0[i]) * p; }

/* Antes de cambiar datos: lo que se ve pasa a ser el origen de la interpolación. */
static void iniciar_cambio(grafico_t *g)
{
    if (!g->animado) return;
    for (int k = 0; k < g->ns; k++) {
        gr_serie_t *s = &g->s[k];
        for (int i = 0; i < s->n; i++) s->y0[i] = vd(s, i, g->p_cam);
    }
    g->esc_a_min = g->esc_v_min; g->esc_a_max = g->esc_v_max; g->esc_a_ok = g->esc_v_ok;
    g->cam_pend = 1;
    g->p_cam = 0;
    g->cam_activa = 1;
}

int grafico_datos(grafico_t *g, int k, const double *y, int n)
{
    if (k < 0 || k >= g->ns || n < 0) return -1;
    if (n > GR_MAX_PUNTOS) n = GR_MAX_PUNTOS;
    gr_serie_t *s = &g->s[k];
    int antes = s->n;
    iniciar_cambio(g);
    if (n && serie_reservar(s, n) < 0) return -1;
    for (int i = 0; i < n; i++) {
        s->y0[i] = (g->animado && i < antes && antes) ? s->y0[i] : (antes ? 0 : y[i]);
        s->y[i] = y[i];
        s->x[i] = i;
        s->t[i] = 0;
    }
    s->n = n;
    if (!g->animado) for (int i = 0; i < n; i++) s->y0[i] = s->y[i];
    g->hs = g->hi = -1;
    g->sucio = 1;
    return 0;
}

int grafico_datos_xy(grafico_t *g, int k, const double *x, const double *y, const double *tam, int n)
{
    if (grafico_datos(g, k, y, n) < 0) return -1;
    gr_serie_t *s = &g->s[k];
    for (int i = 0; i < s->n; i++) { s->x[i] = x ? x[i] : i; s->t[i] = tam ? tam[i] : 0; }
    return 0;
}

int grafico_agregar_punto_xy(grafico_t *g, int k, double x, double y, double tam)
{
    if (k < 0 || k >= g->ns) return -1;
    gr_serie_t *s = &g->s[k];
    int lim = g->ventana > 0 ? g->ventana : GR_MAX_PUNTOS;
    iniciar_cambio(g);
    if (s->n >= lim) {                                   /* ventana deslizante: se pierde el más antiguo */
        int d = s->n - lim + 1;
        for (int i = 0; i + d < s->n; i++) { s->y[i] = s->y[i + d]; s->x[i] = s->x[i + d]; s->t[i] = s->t[i + d]; s->y0[i] = s->y0[i + d]; }
        s->n -= d;
        if (g->animado) { g->desp += d; if (g->desp > 3) g->desp = 3; }
    }
    if (serie_reservar(s, s->n + 1) < 0) return -1;
    double prev = s->n ? s->y[s->n - 1] : y;
    s->y[s->n] = y;
    s->y0[s->n] = g->animado ? prev : y;
    s->x[s->n] = x;
    s->t[s->n] = tam;
    s->n++;
    if (!g->animado) for (int i = 0; i < s->n; i++) s->y0[i] = s->y[i];
    g->hs = g->hi = -1;
    g->sucio = 1;
    return 0;
}

int grafico_agregar_punto(grafico_t *g, int k, double y)
{
    if (k < 0 || k >= g->ns) return -1;
    gr_serie_t *s = &g->s[k];
    /* x continúa la cuenta de la serie (para dispersión en vivo) */
    return grafico_agregar_punto_xy(g, k, s->n ? s->x[s->n - 1] + 1 : 0, y, 0);
}

int grafico_n_puntos(const grafico_t *g, int s) { return s >= 0 && s < g->ns ? g->s[s].n : 0; }
double grafico_valor(const grafico_t *g, int s, int i) { return s >= 0 && s < g->ns && i >= 0 && i < g->s[s].n ? g->s[s].y[i] : 0; }

void grafico_limpiar(grafico_t *g)
{
    grafico_quitar_series(g);
    g->ncat = 0;
    g->desp = 0;
    grafico_reiniciar(g);
}

void grafico_ocultar(grafico_t *g, int item, int oc)
{
    if (item < 0 || item >= 32) return;
    iniciar_cambio(g);
    if (oc) g->oculto |= 1u << item; else g->oculto &= ~(1u << item);
    g->sucio = 1;
}
int grafico_oculto(const grafico_t *g, int item) { return item >= 0 && item < 32 && (g->oculto >> item & 1); }

void grafico_reiniciar(grafico_t *g)
{
    g->ent_pend = 1;
    g->p_ent = g->animado ? 0 : 1;
    g->sucio = 1;
}

/* ------------------------------------------------------------ animación */

int grafico_avanzar(grafico_t *g, unsigned ahora)
{
    g->ahora = ahora;
    if (!g->animado) {
        if (g->p_ent < 1 || g->p_cam < 1) g->sucio = 1;
        g->p_ent = g->p_cam = 1; g->ent_pend = g->cam_pend = g->cam_activa = 0; g->desp = 0;
        return 0;
    }
    if (g->ent_pend) { g->t_ent0 = ahora; g->ent_pend = 0; g->p_ent = 0; g->sucio = 1; }
    if (g->cam_pend) { g->t_cam0 = ahora; g->cam_pend = 0; g->p_cam = 0; g->cam_activa = 1; }
    int activo = 0;
    if (g->p_ent < 1) {
        double t = (double)(ahora - g->t_ent0) / g->duracion;
        g->p_ent = t >= 1 ? 1 : gr_ease(g->ease, t);
        g->sucio = 1;
        activo = g->p_ent < 1;
    }
    if (g->cam_activa) {
        double t = (double)(ahora - g->t_cam0) / g->duracion;
        if (t >= 1) {
            g->p_cam = 1; g->cam_activa = 0; g->desp = 0;
            for (int k = 0; k < g->ns; k++)
                for (int i = 0; i < g->s[k].n; i++) g->s[k].y0[i] = g->s[k].y[i];
        } else { g->p_cam = gr_ease(g->ease, t); activo = 1; }
        g->sucio = 1;
    }
    return activo;
}

int grafico_sucio(const grafico_t *g) { return g->sucio; }

/* ------------------------------------------------------------ etiquetas y datos derivados */

static void fnum(const grafico_t *g, double v, char *b, int max) { gr_fmt_num(&g->fmt, v, b, max); }

static void etq_cat(const grafico_t *g, int i, char *b, int max)
{
    if (i < g->ncat && g->cat[i][0]) scopy(b, g->cat[i], max);
    else { gr_fmt_t f; gr_fmt_parse("0", &f); gr_fmt_num(&f, i + 1, b, max); }
}

static int n_cat(const grafico_t *g)
{
    int n = 0;
    for (int k = 0; k < g->ns; k++) if (g->s[k].n > n) n = g->s[k].n;
    return n;
}

static unsigned col_item(const grafico_t *g, int i, int oscuro)
{
    return i >= 0 && i < GR_MAX_LEY && g->col_item[i] ? g->col_item[i] : gr_color_serie(i, oscuro);
}

static int es_pie(const grafico_t *g) { return g->tipo == GR_TARTA || g->tipo == GR_ANILLO; }
static int visible(const grafico_t *g, int k) { return !(g->oculto >> k & 1); }

/* Elementos de la leyenda: series, o porciones en la tarta; ninguna en velas. */
static int ley_items(const grafico_t *g, char nom[GR_MAX_LEY][GR_MAX_ETQ])
{
    int n = 0;
    if (g->tipo == GR_VELAS) return 0;
    if (es_pie(g)) {
        int m = g->ns ? g->s[0].n : 0;
        for (int i = 0; i < m && n < GR_MAX_LEY; i++, n++) etq_cat(g, i, nom[n], GR_MAX_ETQ);
    } else if (g->tipo == GR_HISTOGRAMA) {
        if (g->ns) { scopy(nom[0], g->s[0].nombre[0] ? g->s[0].nombre : "Frecuencia", GR_MAX_ETQ); n = 1; }
    } else {
        for (int k = 0; k < g->ns && n < GR_MAX_LEY; k++, n++) scopy(nom[n], g->s[k].nombre[0] ? g->s[k].nombre : "Serie", GR_MAX_ETQ);
    }
    return n;
}

static double *tmp_(grafico_t *g)
{
    if (!g->tmp) g->tmp = aloca_zerado(sizeof(double) * 3 * GR_MAX_SERIES * GR_MAX_PUNTOS);
    return g->tmp;
}

static void hot_add(grafico_t *g, int k, int s, int i, double a, double b, double c, double d, double e, double f)
{
    if (g->nhot >= HOT_MAX) return;
    gr_hot_t *h = &g->hot[g->nhot++];
    h->k = (unsigned char)k; h->s = (short)s; h->i = (short)i;
    h->a = a; h->b = b; h->c = c; h->d = d; h->e = e; h->f = f;
}

static int es_hover(const grafico_t *g, int s, int i) { return g->hs == s && g->hi == i; }

/* ------------------------------------------------------------ rango y escala */

typedef struct { double min, max, t0, paso; int nt; } esc_t;   /* rango mostrado y marcas t0 + k*paso */

/* Rango de los datos de destino. *cero = 1 si el eje debe incluir el cero. */
static int rango_datos(grafico_t *g, double *lo, double *hi, int *cero)
{
    double mi = 1e300, ma = -1e300;
    int ncat = n_cat(g);
    *cero = 1;
    if (g->tipo == GR_HISTOGRAMA) { *lo = 0; *hi = 1; return 1; }
    if (g->tipo == GR_VELAS) {
        *cero = 0;
        if (g->ns >= 4)
            for (int i = 0; i < g->s[1].n && i < g->s[2].n; i++) { ma = mx(ma, g->s[1].y[i]); mi = mn(mi, g->s[2].y[i]); }
    } else if ((g->tipo == GR_BARRAS || g->tipo == GR_BARRAS_H || g->tipo == GR_AREA) && g->apilado && ncat) {
        const double *ys[GR_MAX_SERIES];
        int np[GR_MAX_SERIES];
        double *buf = tmp_(g);
        if (!buf) return 0;
        for (int k = 0; k < g->ns; k++) { ys[k] = g->s[k].y; np[k] = g->s[k].n; }
        gr_apilar(ys, np, g->ns, g->oculto, ncat, buf, buf + GR_MAX_SERIES * GR_MAX_PUNTOS);
        for (int k = 0; k < g->ns; k++)
            for (int i = 0; i < ncat; i++) {
                double b = buf[k * ncat + i], t = buf[GR_MAX_SERIES * GR_MAX_PUNTOS + k * ncat + i];
                mi = mn(mi, mn(b, t)); ma = mx(ma, mx(b, t));
            }
    } else {
        for (int k = 0; k < g->ns; k++) {
            if (!visible(g, k)) continue;
            for (int i = 0; i < g->s[k].n; i++) { mi = mn(mi, g->s[k].y[i]); ma = mx(ma, g->s[k].y[i]); }
        }
        if (g->tipo == GR_LINEAS) *cero = g->cero;
        if (g->tipo == GR_DISPERSION) *cero = 0;
    }
    if (mi > ma) { mi = 0; ma = 1; }
    *lo = mi; *hi = ma;
    return 1;
}

static void escala_valores(grafico_t *g, int maxm, esc_t *e)
{
    double lo, hi;
    int cero;
    gr_escala_t s;
    if (g->tipo == GR_HISTOGRAMA) {
        int mxc = 1;
        for (int j = 0; j < g->hb_n; j++) if (g->hb_cnt[j] > mxc) mxc = g->hb_cnt[j];
        lo = 0; hi = mxc; cero = 1;
    } else rango_datos(g, &lo, &hi, &cero);
    if (!g->rango_auto) { lo = g->rmin; hi = g->rmax; }
    gr_escala(lo, hi, maxm, g->rango_auto ? cero : 0, &s);
    e->min = s.min; e->max = s.max; e->paso = s.paso;
    if (!g->rango_auto && g->rmax > g->rmin) { e->min = g->rmin; e->max = g->rmax; }
    e->t0 = gr_ceil(e->min / e->paso - 1e-9) * e->paso;
    e->nt = (int)gr_floor((e->max - e->t0) / e->paso + 1e-9) + 1;
    if (e->nt < 1) e->nt = 1;
    /* durante un cambio de datos el rango pasa del que se veía al nuevo; las marcas son las de destino */
    if (g->esc_a_ok && g->p_cam < 1) {
        double a = g->esc_a_min, b = g->esc_a_max;
        e->min = gr_lerp(a, e->min, g->p_cam);
        e->max = gr_lerp(b, e->max, g->p_cam);
    }
    if (e->max - e->min < 1e-12) e->max = e->min + 1;
}

/* ------------------------------------------------------------ dibujo */

typedef struct { gr_rec_t area; gr_rec_t tit; gr_rec_t ley; int ley_h; } lay_t;

static void rellenar_tema(cx_t *c)
{
    esq_asegurar();
    c->oscuro = sagui_esq.escuro ? 1 : 0;
    c->fondo = mod_tema.superficie;
    c->texto = mod_tema.texto;
    c->fraco = mod_tema.texto_fraco;
    c->rej = mezclar_col(mod_tema.superficie, mod_tema.texto, c->oscuro ? 34 : 28);
    c->eje = mod_tema.borde_forte;
    c->tipf = mod_tema.dica_fundo;
    c->tipt = mod_tema.dica_texto;
}

/* Reparte el sitio: título, leyenda y el área del dibujo (sin las marcas de los ejes). */
static void disponer(grafico_t *g, cx_t *c, lay_t *L)
{
    char nom[GR_MAX_LEY][GR_MAX_ETQ];
    int n = ley_items(g, nom);
    int x0 = MARGEN, y0 = MARGEN, x1 = c->w - MARGEN, y1 = c->h - MARGEN;
    g->ley_n = 0;
    if (g->titulo[0]) { L->tit.x = x0; L->tit.y = y0; L->tit.w = x1 - x0; L->tit.h = LET_A + 6; y0 += LET_A + 8; }
    else L->tit.h = 0;
    int pos = g->ley;
    if (n < 1) pos = GR_LEY_NINGUNA;
    if (pos == GR_LEY_ABAJO || pos == GR_LEY_ARRIBA) {
        int fila_h = 20, x = x0, fila = 0, ancho = x1 - x0;
        for (int i = 0; i < n; i++) {
            int w = 14 + slen(nom[i]) * LET_L + 14;
            if (x > x0 && x + w > x0 + ancho) { x = x0; fila++; }
            g->ley_r[i].x = x; g->ley_r[i].y = fila * fila_h; g->ley_r[i].w = w; g->ley_r[i].h = fila_h;
            x += w;
        }
        int total_h = (fila + 1) * fila_h + 2, base = pos == GR_LEY_ABAJO ? y1 - total_h : y0;
        /* centra cada fila */
        for (int f = 0; f <= fila; f++) {
            int a = -1, b = 0;
            for (int i = 0; i < n; i++) if (g->ley_r[i].y == f * fila_h) { if (a < 0) a = g->ley_r[i].x; b = g->ley_r[i].x + g->ley_r[i].w; }
            int d = a < 0 ? 0 : (ancho - (b - a)) / 2;
            for (int i = 0; i < n; i++) if (g->ley_r[i].y == f * fila_h) g->ley_r[i].x += d;
        }
        for (int i = 0; i < n; i++) g->ley_r[i].y += base;
        if (pos == GR_LEY_ABAJO) y1 -= total_h + 4; else y0 += total_h + 4;
    } else if (pos == GR_LEY_DERECHA || pos == GR_LEY_IZQUIERDA) {
        int maxl = 0;
        for (int i = 0; i < n; i++) maxl = imax(maxl, slen(nom[i]));
        int w = imin(14 + maxl * LET_L + 10, (x1 - x0) * 2 / 5);
        int alto = n * 20, ys = y0 + ((y1 - y0) - alto) / 2;
        if (ys < y0) ys = y0;
        int xs = pos == GR_LEY_DERECHA ? x1 - w : x0;
        for (int i = 0; i < n; i++) { g->ley_r[i].x = xs; g->ley_r[i].y = ys + i * 20; g->ley_r[i].w = w; g->ley_r[i].h = 20; }
        if (pos == GR_LEY_DERECHA) x1 -= w + 8; else x0 += w + 8;
    }
    g->ley_n = pos == GR_LEY_NINGUNA ? 0 : n;
    L->area.x = x0; L->area.y = y0; L->area.w = x1 - x0; L->area.h = y1 - y0;
}

static void dibujar_leyenda(grafico_t *g, cx_t *c)
{
    char nom[GR_MAX_LEY][GR_MAX_ETQ];
    int n = ley_items(g, nom);
    for (int i = 0; i < g->ley_n && i < n; i++) {
        gr_rec_t *r = &g->ley_r[i];
        int oc = !visible(g, i);
        unsigned col = col_item(g, i, c->oscuro);
        int cy = r->y + r->h / 2;
        if (oc) { rect(c, r->x, cy - 5, 10, 10, c->fraco, 255); rect(c, r->x + 1, cy - 4, 8, 8, c->fondo, 255); }
        else rect(c, r->x, cy - 5, 10, 10, col, 255);
        unsigned tc = oc ? c->fraco : c->texto;
        texto_max(c, r->x + 14, cy - LET_A / 2, nom[i], r->w - 14, tc);
        if (oc) rect(c, r->x + 14, cy, imin(slen(nom[i]) * LET_L, r->w - 14), 1, tc, 255);
    }
}

static void dibujar_titulo(grafico_t *g, cx_t *c, const lay_t *L)
{
    if (!g->titulo[0]) return;
    int w = slen(g->titulo) * LET_L, x = L->tit.x + (L->tit.w - w) / 2;
    if (x < L->tit.x) x = L->tit.x;
    texto_max(c, x, L->tit.y, g->titulo, L->tit.w, c->texto);
    texto_max(c, x + 1, L->tit.y, g->titulo, L->tit.w, c->texto);   /* negrita barata */
}

static void sin_datos(cx_t *c, const gr_rec_t *a)
{
    const char *t = "Sin datos";
    texto(c, a->x + (a->w - slen(t) * LET_L) / 2, a->y + (a->h - LET_A) / 2, t, c->fraco);
}

/* Polilínea densa de una serie (curva suave si procede): devuelve el número de puntos. */
static int densa(const grafico_t *g, const double *px, const double *py, int n, double *ox, double *oy)
{
    int m = 0;
    if (!g->suave || n < 3 || n > GR_MAX_PUNTOS) {
        for (int i = 0; i < n; i++) { ox[m] = px[i]; oy[m] = py[i]; m++; }
        return m;
    }
    for (int i = 0; i < n - 1; i++) {
        double p0 = py[i > 0 ? i - 1 : 0], p3 = py[i + 2 < n ? i + 2 : n - 1];
        for (int k = 0; k < SUB; k++) {
            double t = (double)k / SUB;
            ox[m] = px[i] + (px[i + 1] - px[i]) * t;
            oy[m] = gr_catmull(p0, py[i], py[i + 1], p3, t);
            m++;
        }
    }
    ox[m] = px[n - 1]; oy[m] = py[n - 1]; m++;
    return m;
}

static unsigned resaltar(unsigned col, int oscuro) { return mezclar_col(col, oscuro ? 0xFFFFFF : 0xFFFFFF, 70); }

/* ---------------- gráficos con ejes cartesianos (líneas, área, barras, histograma, velas, dispersión) */

static void ejes_texto_y(grafico_t *g, cx_t *c, const esc_t *e, int max_ancho, int *ancho_etq)
{
    char b[48];
    int w = 0;
    (void)c;
    for (int k = 0; k < e->nt; k++) { fnum(g, e->t0 + k * e->paso, b, sizeof b); w = imax(w, slen(b)); }
    (void)max_ancho;
    *ancho_etq = w * LET_L;
}

static void dibujar_cartesiano(grafico_t *g, cx_t *c, lay_t *L)
{
    int ncat = n_cat(g);
    gr_rec_t a = L->area;
    char b[64];
    if (g->tipo == GR_HISTOGRAMA && g->ns) {
        g->hb_n = gr_bins(g->s[0].y, g->s[0].n, g->bins, &g->hb_lo, &g->hb_paso, g->hb_cnt, 64);
        ncat = g->hb_n;
    }
    if (ncat < 1 || g->ns < 1) { sin_datos(c, &a); return; }
    if (g->tipo == GR_VELAS && g->ns < 4) { sin_datos(c, &a); return; }
    int disp = g->tipo == GR_DISPERSION;

    /* escala de valores (eje vertical) */
    esc_t ev;
    int maxm = imax(3, imin(9, (a.h - 40) / 34));
    escala_valores(g, maxm, &ev);
    int wy;
    ejes_texto_y(g, c, &ev, 0, &wy);
    /* escala x numérica en la dispersión */
    esc_t ex = { 0, 1, 0, 1, 2 };
    if (disp) {
        double lo = 1e300, hi = -1e300;
        for (int k = 0; k < g->ns; k++) {
            if (!visible(g, k)) continue;
            for (int i = 0; i < g->s[k].n; i++) { lo = mn(lo, g->s[k].x[i]); hi = mx(hi, g->s[k].x[i]); }
        }
        if (lo > hi) { lo = 0; hi = 1; }
        gr_escala_t s;
        gr_escala(lo, hi, imax(3, imin(9, (a.w - 60) / 70)), 0, &s);
        ex.min = s.min; ex.max = s.max; ex.paso = s.paso; ex.t0 = s.min; ex.nt = s.n;
    }
    int ley_x = g->etq_x[0] ? LET_A + 2 : 0, ley_y = g->etq_y[0] ? LET_A + 2 : 0;
    int xl = LET_A + 8;
    /* área interior del trazado */
    gr_rec_t p;
    p.x = a.x + wy + 10; p.y = a.y + 6 + ley_y; p.w = a.w - (p.x - a.x) - 8; p.h = a.h - 6 - ley_y - xl - ley_x;
    if (p.w < 20 || p.h < 20) { sin_datos(c, &a); return; }
    double sw = p.w, sh = p.h;
    #define YPIX(v) (p.y + sh - ((v) - ev.min) / (ev.max - ev.min) * sh)
    #define XNUM(v) (p.x + ((v) - ex.min) / (ex.max - ex.min) * sw)

    /* rejilla y marcas del eje y */
    for (int k = 0; k < ev.nt; k++) {
        double v = ev.t0 + k * ev.paso, yy = YPIX(v);
        if (yy < p.y - 0.5 || yy > p.y + p.h + 0.5) continue;
        if (g->rejilla) rect(c, p.x, (int)(yy + 0.5), p.w, 1, c->rej, 255);
        fnum(g, v, b, sizeof b);
        texto(c, p.x - 6 - slen(b) * LET_L, (int)(yy + 0.5) - LET_A / 2, b, c->fraco);
    }
    if (disp) {
        for (int k = 0; k < ex.nt; k++) {
            double v = ex.t0 + k * ex.paso, xx = XNUM(v);
            if (g->rejilla) rect(c, (int)(xx + 0.5), p.y, 1, p.h, c->rej, 255);
            fnum(g, v, b, sizeof b);
            texto(c, (int)(xx + 0.5) - slen(b) * LET_L / 2, p.y + p.h + 4, b, c->fraco);
        }
    }
    /* línea de base del eje x (el cero si lo hay, si no el borde inferior) */
    double ycero = ev.min <= 0 && ev.max >= 0 ? YPIX(0) : p.y + p.h;
    rect(c, p.x, (int)(ycero + 0.5), p.w, 1, c->eje, 255);
    rect(c, p.x, p.y, 1, p.h, c->eje, 255);
    if (g->etq_y[0]) texto_max(c, a.x, a.y, g->etq_y, a.w, c->fraco);
    if (g->etq_x[0]) texto(c, p.x + (p.w - slen(g->etq_x) * LET_L) / 2, a.y + a.h - LET_A, g->etq_x, c->fraco);

    int nvis = 0;
    for (int k = 0; k < g->ns; k++) if (visible(g, k)) nvis++;

    /* etiquetas de categorías (eje x) */
    double slot = p.w / (double)ncat;
    int centradas = g->tipo == GR_BARRAS || g->tipo == GR_HISTOGRAMA || g->tipo == GR_VELAS;
    double lx0 = p.x + 10, lw = p.w - 20;
    #define XCAT(i) (centradas ? p.x + ((i) + 0.5) * slot : (ncat > 1 ? lx0 + (i) * lw / (ncat - 1) : p.x + p.w / 2.0))
    if (!disp) {
        int maxl = 1;
        char tmpl[64];
        for (int i = 0; i < ncat; i++) {
            if (g->tipo == GR_HISTOGRAMA) { fnum(g, g->hb_lo + (i + 0.5) * g->hb_paso, tmpl, sizeof tmpl); }
            else etq_cat(g, i, tmpl, sizeof tmpl);
            maxl = imax(maxl, slen(tmpl));
        }
        double paso_px = ncat > 1 && !centradas ? lw / (ncat - 1) : slot;
        int salto = (int)gr_ceil((maxl * LET_L + 8) / paso_px);
        if (salto < 1) salto = 1;
        for (int i = 0; i < ncat; i += salto) {
            if (g->tipo == GR_HISTOGRAMA) fnum(g, g->hb_lo + (i + 0.5) * g->hb_paso, tmpl, sizeof tmpl);
            else etq_cat(g, i, tmpl, sizeof tmpl);
            int w = slen(tmpl) * LET_L, xx = (int)XCAT(i) - w / 2;
            if (xx < a.x) xx = a.x;
            if (xx + w > a.x + a.w) xx = a.x + a.w - w;
            texto(c, xx, p.y + p.h + 4, tmpl, c->fraco);
        }
    }

    /* ---- series ---- */
    cx_t cc = *c;
    cc.cx0 = imax(c->cx0, p.x - 1); cc.cx1 = imin(c->cx1, p.x + p.w + 1);
    cc.cy0 = imax(c->cy0, p.y - 2); cc.cy1 = imin(c->cy1, p.y + p.h + 2);
    double pe = g->p_ent, pc = g->p_cam;
    double *scr = g->scr;
    double *dv = tmp_(g), *bas = dv + GR_MAX_SERIES * GR_MAX_PUNTOS, *cim = bas + GR_MAX_SERIES * GR_MAX_PUNTOS;
    if (!scr || !dv) return;

    if (g->tipo == GR_BARRAS || g->tipo == GR_HISTOGRAMA) {
        int k_vis = 0;
        const double *ys[GR_MAX_SERIES];
        int np[GR_MAX_SERIES];
        if (g->tipo == GR_HISTOGRAMA) {
            for (int i = 0; i < ncat; i++) {
                double x, w, h = g->hb_cnt[i] * pe, y1 = YPIX(h);
                gr_barra_geom(p.x, p.w, ncat, i, 1, 0, 0, 0.04, &x, &w);
                unsigned col = col_item(g, 0, c->oscuro);
                int hv = es_hover(g, 0, i);
                rect_f(&cc, x, y1, w, ycero - y1, hv ? resaltar(col, c->oscuro) : col, 255);
                hot_add(g, 0, 0, i, x, p.y, w, p.h, 0, 0);
                if (g->valores && g->hb_cnt[i] > 0 && pe >= 1) {
                    gr_fmt_t f0; gr_fmt_parse("0", &f0); gr_fmt_num(&f0, g->hb_cnt[i], b, sizeof b);
                    texto(&cc, (int)(x + w / 2) - slen(b) * LET_L / 2, (int)y1 - LET_A - 1, b, c->texto);
                }
            }
        } else if (g->apilado) {
            for (int k = 0; k < g->ns; k++) {
                ys[k] = dv + k * ncat; np[k] = g->s[k].n;
                for (int i = 0; i < ncat; i++) dv[k * ncat + i] = i < g->s[k].n ? vd(&g->s[k], i, pc) * pe : 0;
            }
            gr_apilar(ys, np, g->ns, g->oculto, ncat, bas, cim);
            for (int k = 0; k < g->ns; k++) {
                if (!visible(g, k)) continue;
                for (int i = 0; i < g->s[k].n; i++) {
                    double x, w;
                    gr_barra_geom(p.x, p.w, ncat, i, 1, 0, 1, g->hueco < 10 ? 0.25 : 0.3, &x, &w);
                    double yb = YPIX(bas[k * ncat + i]), yt = YPIX(cim[k * ncat + i]);
                    unsigned col = col_item(g, k, c->oscuro);
                    rect_f(&cc, x, mn(yb, yt), w, fabs_(yb - yt), es_hover(g, k, i) ? resaltar(col, c->oscuro) : col, 255);
                    if (fabs_(yb - yt) > 1.5) rect_f(&cc, x, mn(yb, yt), w, 1, c->fondo, 90);
                    hot_add(g, 0, k, i, x, mn(yb, yt), w, fabs_(yb - yt) < 3 ? 3 : fabs_(yb - yt), 0, 0);
                }
            }
        } else {
            for (int k = 0; k < g->ns; k++) {
                if (!visible(g, k)) continue;
                for (int i = 0; i < g->s[k].n; i++) {
                    double x, w, v = vd(&g->s[k], i, pc) * pe;
                    gr_barra_geom(p.x, p.w, ncat, i, nvis, k_vis, 0, 0.25, &x, &w);
                    double yv = YPIX(v), alt = fabs_(yv - ycero);
                    unsigned col = col_item(g, k, c->oscuro);
                    rect_f(&cc, x, mn(yv, ycero), w, alt, es_hover(g, k, i) ? resaltar(col, c->oscuro) : col, 255);
                    hot_add(g, 0, k, i, x, mn(yv, ycero), w, alt < 3 ? 3 : alt, 0, 0);
                    if (g->valores && pe >= 1 && w >= 24) {
                        fnum(g, g->s[k].y[i], b, sizeof b);
                        int tw = slen(b) * LET_L;
                        if (tw <= w + 12) texto(&cc, (int)(x + w / 2) - tw / 2, v >= 0 ? (int)yv - LET_A - 1 : (int)yv + 2, b, c->texto);
                    }
                }
                k_vis++;
            }
        }
    } else if (g->tipo == GR_LINEAS || g->tipo == GR_AREA) {
        /* revelado de izquierda a derecha al aparecer */
        cc.cx1 = imin(cc.cx1, p.x - 1 + (int)(pe * (p.w + 2)) + 1);
        double desp_px = g->desp * (ncat > 1 ? lw / (ncat - 1) : 0) * (1 - pc);
        const double *ys[GR_MAX_SERIES];
        int np[GR_MAX_SERIES];
        if (g->tipo == GR_AREA && g->apilado) {
            for (int k = 0; k < g->ns; k++) {
                ys[k] = dv + k * ncat; np[k] = g->s[k].n;
                for (int i = 0; i < ncat; i++) dv[k * ncat + i] = i < g->s[k].n ? vd(&g->s[k], i, pc) : 0;
            }
            gr_apilar(ys, np, g->ns, g->oculto, ncat, bas, cim);
        }
        for (int k = 0; k < g->ns; k++) {
            if (!visible(g, k)) continue;
            int n = g->s[k].n;
            if (n < 1) continue;
            double *px = scr, *py = px + GR_MAX_PUNTOS, *qy = py + GR_MAX_PUNTOS, *ox = qy + GR_MAX_PUNTOS, *oy = ox + SCR_N;
            double *bx = oy + SCR_N, *by = bx + SCR_N, *base = by + SCR_N;
            int apil = g->tipo == GR_AREA && g->apilado;
            for (int i = 0; i < n; i++) {
                px[i] = XCAT(i) + desp_px;
                py[i] = YPIX(apil ? cim[k * ncat + i] : vd(&g->s[k], i, pc));
                qy[i] = apil ? YPIX(bas[k * ncat + i]) : ycero;
            }
            unsigned col = col_item(g, k, c->oscuro);
            int m = densa(g, px, py, n, ox, oy);
            if (g->tipo == GR_AREA && m >= 2) {
                int mb = densa(g, px, qy, n, bx, by);
                for (int i = 0; i < m; i++) base[i] = mb == m ? by[i] : ycero;
                area_entre(&cc, ox, oy, base, m, col, c->oscuro ? 110 : 95);
            }
            for (int i = 0; i + 1 < m; i++) linea(&cc, ox[i], oy[i], ox[i + 1], oy[i + 1], 2.2, col, 255);
            int marcas = g->puntos == 2 ? ncat <= 30 : g->puntos;
            for (int i = 0; i < n; i++) {
                double xx = px[i], yy = py[i];
                int hv = es_hover(g, k, i);
                if (marcas || hv) {
                    disco(&cc, xx, yy, hv ? 6 : 4.5, c->fondo, 255);
                    disco(&cc, xx, yy, hv ? 4.5 : 3, hv ? resaltar(col, c->oscuro) : col, 255);
                }
                hot_add(g, 1, k, i, xx, yy, 8, 0, 0, 0);
                if (g->valores && pe >= 1) { fnum(g, g->s[k].y[i], b, sizeof b); texto(&cc, (int)xx - slen(b) * LET_L / 2, (int)yy - LET_A - 6, b, c->texto); }
            }
        }
    } else if (g->tipo == GR_VELAS) {
        cc.cx1 = imin(cc.cx1, p.x - 1 + (int)(pe * (p.w + 2)) + 1);
        for (int i = 0; i < g->s[0].n && i < g->s[1].n && i < g->s[2].n && i < g->s[3].n; i++) {
            double o = vd(&g->s[0], i, pc), h = vd(&g->s[1], i, pc), l = vd(&g->s[2], i, pc), cl = vd(&g->s[3], i, pc);
            double xc = XCAT(i), bw = mn(slot * 0.6, 22);
            unsigned col = cl >= o ? gr_color_serie(2, c->oscuro) : gr_color_serie(3, c->oscuro);
            if (es_hover(g, 0, i)) col = resaltar(col, c->oscuro);
            double yh = YPIX(h), yl = YPIX(l), yo = YPIX(o), ycl = YPIX(cl);
            rect_f(&cc, xc - 0.75, yh, 1.5, yl - yh, col, 255);
            rect_f(&cc, xc - bw / 2, mn(yo, ycl), bw, mx(fabs_(yo - ycl), 1.5), col, 255);
            hot_add(g, 0, 0, i, xc - slot / 2, p.y, slot, p.h, 0, 0);
        }
    } else if (disp) {
        for (int k = 0; k < g->ns; k++) {
            if (!visible(g, k)) continue;
            double tmax = 0;
            for (int i = 0; i < g->s[k].n; i++) tmax = mx(tmax, g->s[k].t[i]);
            unsigned col = col_item(g, k, c->oscuro);
            for (int i = 0; i < g->s[k].n; i++) {
                double xx = XNUM(g->s[k].x[i]), yy = YPIX(vd(&g->s[k], i, pc));
                double r = tmax > 0 ? 3 + 9 * sq(g->s[k].t[i] / tmax) : 5;
                int hv = es_hover(g, k, i);
                disco(&cc, xx, yy, (hv ? r + 2 : r) * pe, hv ? resaltar(col, c->oscuro) : col, 200);
                aro(&cc, xx, yy, (hv ? r + 2 : r) * pe, 1, col, 255);
                hot_add(g, 1, k, i, xx, yy, mx(r, 6), 0, 0, 0);
            }
        }
    }
    #undef YPIX
    #undef XNUM
    #undef XCAT
    g->esc_v_min = ev.min; g->esc_v_max = ev.max; g->esc_v_ok = 1;
}

/* ---------------- barras horizontales */

static void dibujar_barras_h(grafico_t *g, cx_t *c, lay_t *L)
{
    int ncat = n_cat(g);
    gr_rec_t a = L->area;
    char b[64];
    if (ncat < 1 || g->ns < 1) { sin_datos(c, &a); return; }
    esc_t ev;
    escala_valores(g, imax(3, imin(9, (a.w - 80) / 60)), &ev);
    int wl = 0;
    for (int i = 0; i < ncat; i++) { etq_cat(g, i, b, sizeof b); wl = imax(wl, slen(b)); }
    wl = imin(wl, 14) * LET_L;
    int ley_y = g->etq_y[0] ? LET_A + 2 : 0, ley_x = g->etq_x[0] ? LET_A + 2 : 0;
    gr_rec_t p;
    p.x = a.x + wl + 12; p.y = a.y + 6 + ley_y; p.w = a.w - wl - 12 - 14; p.h = a.h - 6 - ley_y - LET_A - 8 - ley_x;
    if (p.w < 20 || p.h < 20) { sin_datos(c, &a); return; }
    #define XPIX(v) (p.x + ((v) - ev.min) / (ev.max - ev.min) * p.w)
    for (int k = 0; k < ev.nt; k++) {
        double v = ev.t0 + k * ev.paso, xx = XPIX(v);
        if (xx < p.x - 0.5 || xx > p.x + p.w + 0.5) continue;
        if (g->rejilla) rect(c, (int)(xx + 0.5), p.y, 1, p.h, c->rej, 255);
        fnum(g, v, b, sizeof b);
        texto(c, (int)(xx + 0.5) - slen(b) * LET_L / 2, p.y + p.h + 4, b, c->fraco);
    }
    double xcero = ev.min <= 0 && ev.max >= 0 ? XPIX(0) : p.x;
    rect(c, (int)(xcero + 0.5), p.y, 1, p.h, c->eje, 255);
    rect(c, p.x, p.y + p.h, p.w, 1, c->eje, 255);
    if (g->etq_x[0]) texto(c, p.x + (p.w - slen(g->etq_x) * LET_L) / 2, a.y + a.h - LET_A, g->etq_x, c->fraco);
    if (g->etq_y[0]) texto_max(c, a.x, a.y, g->etq_y, a.w, c->fraco);
    double slot = p.h / (double)ncat;
    int salto = (int)gr_ceil((LET_A + 2) / slot);
    if (salto < 1) salto = 1;
    for (int i = 0; i < ncat; i += salto) {
        etq_cat(g, i, b, sizeof b);
        texto_max(c, a.x + wl - imin(slen(b), 14) * LET_L, (int)(p.y + (i + 0.5) * slot) - LET_A / 2, b, wl, c->fraco);
    }
    cx_t cc = *c;
    cc.cx0 = imax(c->cx0, p.x - 1); cc.cx1 = imin(c->cx1, p.x + p.w + 2);
    cc.cy0 = imax(c->cy0, p.y - 1); cc.cy1 = imin(c->cy1, p.y + p.h + 1);
    int nvis = 0;
    for (int k = 0; k < g->ns; k++) if (visible(g, k)) nvis++;
    double pe = g->p_ent, pc = g->p_cam;
    double *dv = tmp_(g);
    if (!dv) return;
    double *bas = dv + GR_MAX_SERIES * GR_MAX_PUNTOS, *cim = bas + GR_MAX_SERIES * GR_MAX_PUNTOS;
    const double *ys[GR_MAX_SERIES];
    int np[GR_MAX_SERIES];
    if (g->apilado) {
        for (int k = 0; k < g->ns; k++) {
            ys[k] = dv + k * ncat; np[k] = g->s[k].n;
            for (int i = 0; i < ncat; i++) dv[k * ncat + i] = i < g->s[k].n ? vd(&g->s[k], i, pc) * pe : 0;
        }
        gr_apilar(ys, np, g->ns, g->oculto, ncat, bas, cim);
    }
    int kv = 0;
    for (int k = 0; k < g->ns; k++) {
        if (!visible(g, k)) continue;
        unsigned col = col_item(g, k, c->oscuro);
        for (int i = 0; i < g->s[k].n; i++) {
            double y, h;
            gr_barra_geom(p.y, p.h, ncat, i, nvis, kv, g->apilado, 0.25, &y, &h);
            double x0, x1;
            if (g->apilado) { x0 = XPIX(bas[k * ncat + i]); x1 = XPIX(cim[k * ncat + i]); }
            else { x0 = xcero; x1 = XPIX(vd(&g->s[k], i, pc) * pe); }
            rect_f(&cc, mn(x0, x1), y, fabs_(x1 - x0), h, es_hover(g, k, i) ? resaltar(col, c->oscuro) : col, 255);
            hot_add(g, 0, k, i, mn(x0, x1), y, mx(fabs_(x1 - x0), 3), h, 0, 0);
            if (g->valores && pe >= 1 && !g->apilado) {
                fnum(g, g->s[k].y[i], b, sizeof b);
                texto(&cc, (int)mx(x0, x1) + 4, (int)(y + h / 2) - LET_A / 2, b, c->texto);
            }
        }
        kv++;
    }
    #undef XPIX
    g->esc_v_min = ev.min; g->esc_v_max = ev.max; g->esc_v_ok = 1;
}

/* ---------------- tarta y anillo */

static void dibujar_tarta(grafico_t *g, cx_t *c, lay_t *L)
{
    gr_rec_t a = L->area;
    if (g->ns < 1 || g->s[0].n < 1) { sin_datos(c, &a); return; }
    gr_serie_t *s = &g->s[0];
    int n = imin(s->n, 32);
    double tot = 0;
    for (int i = 0; i < n; i++) if (visible(g, i)) { double v = vd(s, i, g->p_cam); if (v > 0) tot += v; }
    if (tot <= 0) { sin_datos(c, &a); return; }
    double cx = a.x + a.w / 2.0, cy = a.y + a.h / 2.0, R = mn(a.w, a.h) / 2.0 - 8;
    if (R < 12) { sin_datos(c, &a); return; }
    double r0 = g->tipo == GR_ANILLO ? R * g->hueco / 100.0 : 0;
    double pe = g->p_ent, ini = 0;
    char b[64];
    for (int i = 0; i < n; i++) {
        if (!visible(g, i)) continue;
        double v = vd(s, i, g->p_cam);
        if (v <= 0) continue;
        double ang = v / tot * 6.283185307179586 * pe, a0 = ini, a1 = ini + ang;
        ini = a1;
        double am = (a0 + a1) / 2, dx, dy;
        gr_dir(am, &dx, &dy);
        int hv = es_hover(g, 0, i);
        double ox = hv ? dx * 6 : 0, oy = hv ? dy * 6 : 0;
        unsigned col = col_item(g, i, c->oscuro);
        sector(c, cx + ox, cy + oy, r0, R, a0, a1, hv ? resaltar(col, c->oscuro) : col, 255);
        if (ang < 6.28) {      /* separadores del color del fondo */
            double e0x, e0y, e1x, e1y;
            gr_dir(a0, &e0x, &e0y); gr_dir(a1, &e1x, &e1y);
            linea(c, cx + ox + e0x * r0, cy + oy + e0y * r0, cx + ox + e0x * (R + 1), cy + oy + e0y * (R + 1), 2, c->fondo, 255);
            linea(c, cx + ox + e1x * r0, cy + oy + e1y * r0, cx + ox + e1x * (R + 1), cy + oy + e1y * (R + 1), 2, c->fondo, 255);
        }
        hot_add(g, 2, 0, i, cx + ox, cy + oy, r0, R, v / tot * 6.283185307179586 * pe, a0);   /* e = ángulo, f = inicio */
        if (g->valores && pe >= 1 && ang > 0.35) {
            gr_fmt_t f; gr_fmt_parse("0%", &f);
            gr_fmt_num(&f, v / tot, b, sizeof b);
            double rm = (r0 + R) / 2;
            int tw = slen(b) * LET_L;
            texto(c, (int)(cx + ox + dx * rm) - tw / 2, (int)(cy + oy + dy * rm) - LET_A / 2, b, 0xFFFFFF);
        }
    }
    if (g->tipo == GR_ANILLO && pe >= 1) {       /* el total en el centro */
        fnum(g, tot, b, sizeof b);
        texto(c, (int)cx - slen(b) * LET_L / 2, (int)cy - LET_A + 2, b, c->texto);
        texto(c, (int)cx - 5 * LET_L / 2, (int)cy + 2, "Total", c->fraco);
    }
    g->esc_v_ok = 0;
}

/* ---------------- tooltip y texto del tooltip */

static void armar_tooltip(grafico_t *g)
{
    char b[64], v[64], t[GR_MAX_ETQ + 8];
    g->tip[0] = 0;
    if (g->hs < 0 || g->hi < 0 || g->hs >= g->ns) return;
    gr_serie_t *s = &g->s[g->hs];
    int i = g->hi;
    char *o = g->tip;
    int max = sizeof g->tip;
    #define ADD(txt) do { const char *q_ = (txt); while (*q_ && o < g->tip + max - 1) *o++ = *q_++; *o = 0; } while (0)
    if (es_pie(g)) {
        if (i >= s->n) return;
        double tot = 0;
        for (int k = 0; k < s->n; k++) if (visible(g, k) && s->y[k] > 0) tot += s->y[k];
        etq_cat(g, i, t, sizeof t);
        ADD(t); ADD("\n");
        fnum(g, s->y[i], v, sizeof v); ADD(v);
        if (tot > 0) { gr_fmt_t f; gr_fmt_parse("0.0%", &f); gr_fmt_num(&f, s->y[i] / tot, b, sizeof b); ADD(" ("); ADD(b); ADD(")"); }
    } else if (g->tipo == GR_HISTOGRAMA) {
        if (i >= g->hb_n) return;
        fnum(g, g->hb_lo + i * g->hb_paso, v, sizeof v); ADD("["); ADD(v); ADD("; ");
        fnum(g, g->hb_lo + (i + 1) * g->hb_paso, v, sizeof v); ADD(v); ADD(")\nFrecuencia: ");
        gr_fmt_t f; gr_fmt_parse("0", &f); gr_fmt_num(&f, g->hb_cnt[i], v, sizeof v); ADD(v);
    } else if (g->tipo == GR_VELAS) {
        if (g->ns < 4 || i >= s->n) return;
        etq_cat(g, i, t, sizeof t); ADD(t);
        static const char *rot[4] = { "\nApertura: ", "\nMáximo: ", "\nMínimo: ", "\nCierre: " };
        for (int k = 0; k < 4; k++) { fnum(g, i < g->s[k].n ? g->s[k].y[i] : 0, v, sizeof v); ADD(rot[k]); ADD(v); }
    } else if (g->tipo == GR_DISPERSION) {
        if (i >= s->n) return;
        ADD(s->nombre[0] ? s->nombre : "Serie"); ADD("\nx: ");
        fnum(g, s->x[i], v, sizeof v); ADD(v); ADD("  y: ");
        fnum(g, s->y[i], v, sizeof v); ADD(v);
        if (s->t[i] > 0) { ADD("  tam: "); fnum(g, s->t[i], v, sizeof v); ADD(v); }
    } else {
        if (i >= s->n) return;
        ADD(s->nombre[0] ? s->nombre : "Serie"); ADD("\n");
        etq_cat(g, i, t, sizeof t); ADD(t); ADD(": ");
        fnum(g, s->y[i], v, sizeof v); ADD(v);
    }
    #undef ADD
}

static void dibujar_tooltip(grafico_t *g, cx_t *c)
{
    if (!g->tip[0] || !g->dentro) return;
    char lin[5][64];
    int n = 0, w = 0, k = 0;
    for (const char *q = g->tip; ; q++) {
        if (*q == '\n' || !*q) { lin[n][k] = 0; w = imax(w, slen(lin[n])); n++; k = 0; if (!*q || n == 5) break; }
        else if (k < 63) lin[n][k++] = *q;
    }
    int bw = w * LET_L + 16, bh = n * LET_A + 10;
    int x = g->mx + 14, y = g->my + 16;
    if (x + bw > c->w - 2) x = g->mx - bw - 8;
    if (y + bh > c->h - 2) y = g->my - bh - 8;
    if (x < 2) x = 2;
    if (y < 2) y = 2;
    rect(c, x + 2, y + 2, bw, bh, 0x000000, 40);              /* sombra */
    rect(c, x + 1, y, bw - 2, bh, c->tipf, 255);
    rect(c, x, y + 1, bw, bh - 2, c->tipf, 255);
    for (int i = 0; i < n; i++) texto(c, x + 8, y + 5 + i * LET_A, lin[i], c->tipt);
}

/* ---------------- dibujo completo */

static void dibujar_en(grafico_t *g, cx_t *c, int con_raton)
{
    lay_t L;
    memset(&L, 0, sizeof L);
    if (g->fondo) rect(c, 0, 0, c->w, c->h, c->fondo, 255);
    g->nhot = 0;
    if (!g->scr) g->scr = aloca_zerado(sizeof(double) * SCR_TOT);
    disponer(g, c, &L);
    if (g->ns && es_pie(g)) dibujar_tarta(g, c, &L);
    else if (g->tipo == GR_BARRAS_H) dibujar_barras_h(g, c, &L);
    else dibujar_cartesiano(g, c, &L);
    dibujar_leyenda(g, c);
    dibujar_titulo(g, c, &L);
    if (con_raton) dibujar_tooltip(g, c);
}

void grafico_dibujar(grafico_t *g, unsigned *px, int larg, int alt)
{
    cx_t c;
    memset(&c, 0, sizeof c);
    c.px = px; c.w = larg; c.h = alt; c.cx0 = 0; c.cy0 = 0; c.cx1 = larg; c.cy1 = alt;
    rellenar_tema(&c);
    dibujar_en(g, &c, 1);
    g->sucio = 0;
}

/* ---------------- ratón */

static int buscar(const grafico_t *g, int x, int y, int *s, int *i)
{
    for (int k = g->nhot - 1; k >= 0; k--) {
        const gr_hot_t *h = &g->hot[k];
        int dentro = 0;
        if (h->k == 0) dentro = gr_en_rect(x, y, h->a, h->b, h->c, h->d);
        else if (h->k == 1) dentro = gr_en_circulo(x, y, h->a, h->b, h->c);
        else dentro = gr_en_sector(x, y, h->a, h->b, h->c, h->d, h->f, h->f + h->e);
        if (dentro) { *s = h->s; *i = h->i; return 1; }
    }
    return 0;
}

static int leyenda_en(const grafico_t *g, int x, int y)
{
    for (int i = 0; i < g->ley_n; i++)
        if (gr_en_rect(x, y, g->ley_r[i].x, g->ley_r[i].y, g->ley_r[i].w, g->ley_r[i].h)) return i;
    return -1;
}

int grafico_mover(grafico_t *g, int x, int y)
{
    int s = -1, i = -1, cambio = 0;
    int dentro_antes = g->dentro;
    g->dentro = 1;
    if (leyenda_en(g, x, y) < 0) buscar(g, x, y, &s, &i);
    if (s != g->hs || i != g->hi) { g->hs = s; g->hi = i; cambio = 1; armar_tooltip(g); }
    if (g->tip[0] && (x != g->mx || y != g->my)) cambio = 1;
    g->mx = x; g->my = y;
    if (!dentro_antes && g->tip[0]) cambio = 1;
    if (cambio) g->sucio = 1;
    return cambio;
}

void grafico_salir(grafico_t *g)
{
    if (g->hs >= 0 || g->dentro) g->sucio = 1;
    g->hs = g->hi = -1;
    g->dentro = 0;
    g->tip[0] = 0;
}

int grafico_pulsar(grafico_t *g, int x, int y, grafico_ev_t *ev)
{
    int it = leyenda_en(g, x, y);
    if (it >= 0) {
        grafico_ocultar(g, it, visible(g, it));
        g->hs = g->hi = -1; g->tip[0] = 0;
        if (ev) { ev->serie = es_pie(g) ? 0 : it; ev->indice = it; ev->valor = 0; ev->leyenda = 1; }
        return 2;
    }
    int s, i;
    if (!buscar(g, x, y, &s, &i)) return 0;
    if (ev) {
        ev->serie = s; ev->indice = i; ev->leyenda = 0;
        if (g->tipo == GR_VELAS) ev->valor = g->ns > 3 && i < g->s[3].n ? g->s[3].y[i] : 0;
        else if (g->tipo == GR_HISTOGRAMA) ev->valor = i < g->hb_n ? g->hb_cnt[i] : 0;
        else ev->valor = grafico_valor(g, s, i);
    }
    return 1;
}

int grafico_resaltado(const grafico_t *g, int *serie, int *indice)
{
    if (g->hs < 0) return 0;
    if (serie) *serie = g->hs;
    if (indice) *indice = g->hi;
    return 1;
}

const char *grafico_tooltip(const grafico_t *g) { return g->tip; }

/* ------------------------------------------------------------ exportar */

int grafico_exportar(grafico_t *g, const char *ruta, int larg, int alt)
{
    if (larg < 32) larg = 32;
    if (alt < 32) alt = 32;
    imagem_t img;
    if (!imagem_criar(&img, larg, alt, 0xFFFFFF)) return IMG_ERRO_MEMORIA;
    double pe = g->p_ent, pc = g->p_cam, ev0 = g->esc_v_min, ev1 = g->esc_v_max;
    int hs = g->hs, hi = g->hi, dentro = g->dentro;
    g->p_ent = g->p_cam = 1;
    g->hs = g->hi = -1; g->dentro = 0;
    cx_t c;
    memset(&c, 0, sizeof c);
    c.px = img.px; c.w = larg; c.h = alt; c.cx1 = larg; c.cy1 = alt;
    rellenar_tema(&c);
    dibujar_en(g, &c, 0);
    int n = slen(ruta);
    int bmp = n > 4 && (ruta[n - 1] == 'p' || ruta[n - 1] == 'P') && (ruta[n - 2] == 'm' || ruta[n - 2] == 'M');
    int r = bmp ? imagem_gravar_bmp(ruta, &img) : imagem_gravar_png(ruta, &img);
    imagem_liberar(&img);
    g->p_ent = pe; g->p_cam = pc; g->esc_v_min = ev0; g->esc_v_max = ev1;
    g->hs = hs; g->hi = hi; g->dentro = dentro;
    g->nhot = 0;           /* la lista de zonas era la del tamaño exportado: se rehace al dibujar de nuevo */
    g->sucio = 1;
    return r;
}
