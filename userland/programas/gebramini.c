// SPDX-License-Identifier: GPL-3.0-or-later
// GebraMini, © 2026 Alejandro Anuncibay: prototipo original (C++/SDL) del mantenedor; esta versión nativa
// para SaguiOS está escrita desde cero a partir de sus ideas. Independiente de GeoGebra: nada de su código,
// de sus iconos ni de sus nombres de marca (solo la inspiración de la idea).
/* gebramini [archivo.gbm] - geometría dinámica y funciones de SaguiOS (D309, mejora 128).
 *
 * Un plano con ejes, cuadrícula, zoom y desplazamiento; herramientas para construir puntos, rectas,
 * circunferencias, polígonos, ángulos y medidas; una vista algebraica a la izquierda con el valor de cada
 * objeto; y una barra de entrada (A = (1, 2), f(x) = sin(x) + x^2/4, Recta(A, B), Raiz(f)...). Los objetos
 * dependen unos de otros (el motor está en userland/gebramini/): al arrastrar un punto libre, todo lo que
 * sale de él se recalcula.
 *
 * Ficheros: .gbm (texto legible), exportar a PNG y a PDF vectorial. Modos sin ventana, para las pruebas:
 *   gebramini --abrir fichero.gbm [--pdf s.pdf] [--png s.png] [--guardar s.gbm]
 *   gebramini --orden "orden" ["orden" ...] [--pdf ...] [--png ...] [--guardar ...]    (muestra cada objeto) */
#include "sagui.h"
#include "gui.h"
#include "pdf_escribir.h"
#include "gbm.h"
#include "gbm_mat.h"
#include "icones.h"

extern const unsigned char fonte_sagui[256][16];

/* ============================================================ trama y destinos de dibujo */

typedef struct alvo alvo_t;
struct alvo {
    void (*linha)(alvo_t *, double x0, double y0, double x1, double y1, double w, uint32_t cor);
    void (*disco)(alvo_t *, double x, double y, double r, uint32_t cor);
    void (*preenche)(alvo_t *, const double *xs, const double *ys, int n, uint32_t cor);
    void (*texto)(alvo_t *, int x, int y, const char *t, uint32_t cor);
    /* trama */
    uint32_t *px;
    int larg, alt, ox, oy;                 /* tamaño del búfer y esquina de la vista en él */
    int cx0, cy0, cx1, cy1;                /* recorte (en el búfer) */
    /* PDF */
    pdfw_t *pdf;
    double k;                              /* twips por píxel de la vista */
    int mx, my;                            /* margen (twips) */
};

static void mezcla(alvo_t *r, int x, int y, uint32_t cor, int a)
{
    if (x < r->cx0 || y < r->cy0 || x >= r->cx1 || y >= r->cy1 || a <= 0) return;
    uint32_t *p = &r->px[y * r->larg + x];
    if (a >= 255) { *p = cor; return; }
    uint32_t d = *p;
    int ra = (int)((cor >> 16) & 255), ga = (int)((cor >> 8) & 255), ba = (int)(cor & 255);
    int rb = (int)((d >> 16) & 255), gb = (int)((d >> 8) & 255), bb = (int)(d & 255);
    *p = (uint32_t)((((rb * (255 - a) + ra * a) / 255) << 16) | (((gb * (255 - a) + ga * a) / 255) << 8) | ((bb * (255 - a) + ba * a) / 255));
}

static inline double mn(double a, double b) { return a < b ? a : b; }
static inline double mx_(double a, double b) { return a > b ? a : b; }

/* Línea con grosor w y bordes suavizados: cada píxel cubre según su distancia al segmento. */
static void r_linha(alvo_t *r, double x0, double y0, double x1, double y1, double w, uint32_t cor)
{
    x0 += r->ox; y0 += r->oy; x1 += r->ox; y1 += r->oy;
    double dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy, half = w * 0.5;
    if (!gm_fin(x0) || !gm_fin(y0) || !gm_fin(x1) || !gm_fin(y1)) return;
    double mnx = mn(x0, x1) - half - 1, mxx = mx_(x0, x1) + half + 1, mny = mn(y0, y1) - half - 1, mxy = mx_(y0, y1) + half + 1;
    if (mxx < r->cx0 || mnx >= r->cx1 || mxy < r->cy0 || mny >= r->cy1) return;
    if (l2 < 1e-9) {
        int ix = (int)gm_floor(x0), iy = (int)gm_floor(y0);
        int rr = (int)half + 1;
        for (int yy = iy - rr; yy <= iy + rr; yy++)
            for (int xx = ix - rr; xx <= ix + rr; xx++) {
                double d = gm_hypot(xx + 0.5 - x0, yy + 0.5 - y0), c = half + 0.5 - d;
                mezcla(r, xx, yy, cor, c <= 0 ? 0 : c >= 1 ? 255 : (int)(c * 255));
            }
        return;
    }
    double len = gm_sqrt(l2);
    if (gm_abs(dx) >= gm_abs(dy)) {
        int xa = (int)gm_floor(mnx), xb = (int)gm_ceil(mxx);
        if (xa < r->cx0) xa = r->cx0;
        if (xb > r->cx1) xb = r->cx1;
        double h = (half + 1) * len / gm_abs(dx);
        for (int x = xa; x < xb; x++) {
            double xc = x + 0.5, t0 = (xc - x0) / dx, yl = y0 + t0 * dy;
            int ya = (int)gm_floor(yl - h), yb = (int)gm_ceil(yl + h);
            if (ya < r->cy0) ya = r->cy0;
            if (yb > r->cy1) yb = r->cy1;
            for (int y = ya; y < yb; y++) {
                double yc = y + 0.5, t = ((xc - x0) * dx + (yc - y0) * dy) / l2;
                t = t < 0 ? 0 : t > 1 ? 1 : t;
                double ex = xc - (x0 + t * dx), ey = yc - (y0 + t * dy);
                double d = gm_sqrt(ex * ex + ey * ey), c = half + 0.5 - d;
                if (c > 0) mezcla(r, x, y, cor, c >= 1 ? 255 : (int)(c * 255));
            }
        }
    } else {
        int ya = (int)gm_floor(mny), yb = (int)gm_ceil(mxy);
        if (ya < r->cy0) ya = r->cy0;
        if (yb > r->cy1) yb = r->cy1;
        double h = (half + 1) * len / gm_abs(dy);
        for (int y = ya; y < yb; y++) {
            double yc = y + 0.5, t0 = (yc - y0) / dy, xl = x0 + t0 * dx;
            int xa = (int)gm_floor(xl - h), xb = (int)gm_ceil(xl + h);
            if (xa < r->cx0) xa = r->cx0;
            if (xb > r->cx1) xb = r->cx1;
            for (int x = xa; x < xb; x++) {
                double xc = x + 0.5, t = ((xc - x0) * dx + (yc - y0) * dy) / l2;
                t = t < 0 ? 0 : t > 1 ? 1 : t;
                double ex = xc - (x0 + t * dx), ey = yc - (y0 + t * dy);
                double d = gm_sqrt(ex * ex + ey * ey), c = half + 0.5 - d;
                if (c > 0) mezcla(r, x, y, cor, c >= 1 ? 255 : (int)(c * 255));
            }
        }
    }
}

static uint32_t oscurece(uint32_t c)
{
    return (((c >> 16) & 255) * 6 / 10) << 16 | (((c >> 8) & 255) * 6 / 10) << 8 | ((c & 255) * 6 / 10);
}

static void r_disco(alvo_t *r, double cx, double cy, double rad, uint32_t cor)
{
    cx += r->ox; cy += r->oy;
    int ia = (int)gm_floor(cx - rad - 2), ib = (int)gm_ceil(cx + rad + 2), ja = (int)gm_floor(cy - rad - 2), jb = (int)gm_ceil(cy + rad + 2);
    uint32_t borde = oscurece(cor);
    for (int y = ja; y < jb; y++)
        for (int x = ia; x < ib; x++) {
            double d = gm_hypot(x + 0.5 - cx, y + 0.5 - cy);
            double c_out = rad + 0.5 - d;
            if (c_out <= 0) continue;
            double c_in = rad - 0.8 + 0.5 - d;       /* el borde oscuro es el anillo de 1 píxel */
            mezcla(r, x, y, borde, c_out >= 1 ? 255 : (int)(c_out * 255));
            if (c_in > 0) mezcla(r, x, y, cor, c_in >= 1 ? 255 : (int)(c_in * 255));
        }
}

static void r_preenche(alvo_t *r, const double *xs, const double *ys, int n, uint32_t cor)
{
    if (n < 3) return;
    double ymin = 1e30, ymax = -1e30;
    for (int i = 0; i < n; i++) { if (ys[i] < ymin) ymin = ys[i]; if (ys[i] > ymax) ymax = ys[i]; }
    int ya = (int)gm_floor(ymin + r->oy), yb = (int)gm_ceil(ymax + r->oy);
    if (ya < r->cy0) ya = r->cy0;
    if (yb > r->cy1) yb = r->cy1;
    for (int y = ya; y < yb; y++) {
        double yc = y + 0.5 - r->oy, xi[130];
        int k = 0;
        for (int i = 0; i < n && k < 128; i++) {
            int j = (i + 1) % n;
            double y0 = ys[i], y1 = ys[j];
            if ((y0 <= yc && y1 > yc) || (y1 <= yc && y0 > yc)) xi[k++] = xs[i] + (yc - y0) / (y1 - y0) * (xs[j] - xs[i]);
        }
        for (int a = 1; a < k; a++) { double v = xi[a]; int b = a - 1; while (b >= 0 && xi[b] > v) { xi[b + 1] = xi[b]; b--; } xi[b + 1] = v; }
        for (int a = 0; a + 1 < k; a += 2) {
            int xa = (int)gm_floor(xi[a] + r->ox + 0.5), xb = (int)gm_floor(xi[a + 1] + r->ox + 0.5);
            for (int x = xa; x < xb; x++) mezcla(r, x, y, cor, 56);
        }
    }
}

static void r_texto(alvo_t *r, int x, int y, const char *t, uint32_t cor)
{
    x += r->ox; y += r->oy;
    for (; *t; t++, x += 8) {
        const unsigned char *g = fonte_sagui[(uint8_t)*t];
        for (int l = 0; l < 16; l++)
            for (int c = 0; c < 8; c++)
                if (g[l] & (0x80 >> c)) mezcla(r, x + c, y + l, cor, 255);
    }
}

/* ---- PDF (vectorial): líneas, rellenos y texto; las circunferencias y las curvas son polilíneas finas ---- */

static int tw(alvo_t *a, double v) { return (int)(v * a->k + 0.5); }
static void p_linha(alvo_t *a, double x0, double y0, double x1, double y1, double w, uint32_t cor)
{
    if (!gm_fin(x0) || !gm_fin(y0) || !gm_fin(x1) || !gm_fin(y1)) return;
    int e = tw(a, w);
    pdfw_linha(a->pdf, a->mx + tw(a, x0), a->my + tw(a, y0), a->mx + tw(a, x1), a->my + tw(a, y1), e < 6 ? 6 : e, cor);
}
static void p_disco(alvo_t *a, double cx, double cy, double rad, uint32_t cor)
{
    /* un disco = rectángulos horizontales finos (el escritor no tiene círculos) */
    uint32_t borde = oscurece(cor);
    for (int pasada = 0; pasada < 2; pasada++) {
        double r = pasada ? rad - 1 : rad;
        for (double dy = -r; dy <= r; dy += 0.7) {
            double h = gm_sqrt(mx_(0, r * r - dy * dy));
            pdfw_retangulo(a->pdf, a->mx + tw(a, cx - h), a->my + tw(a, cy + dy), tw(a, 2 * h) + 1, tw(a, 0.7) + 2, pasada ? cor : borde);
        }
    }
}
static uint32_t claro(uint32_t c, int a)   /* mezcla con blanco (el PDF no tiene aquí transparencias) */
{
    int r = (int)((c >> 16) & 255), g = (int)((c >> 8) & 255), b = (int)(c & 255);
    r = (r * a + 255 * (255 - a)) / 255; g = (g * a + 255 * (255 - a)) / 255; b = (b * a + 255 * (255 - a)) / 255;
    return (uint32_t)((r << 16) | (g << 8) | b);
}
static void p_preenche(alvo_t *a, const double *xs, const double *ys, int n, uint32_t cor)
{
    double ymin = 1e30, ymax = -1e30;
    for (int i = 0; i < n; i++) { if (ys[i] < ymin) ymin = ys[i]; if (ys[i] > ymax) ymax = ys[i]; }
    uint32_t c = claro(cor, 56);
    for (double y = ymin + 0.5; y < ymax; y += 1.0) {
        double xi[130];
        int k = 0;
        for (int i = 0; i < n && k < 128; i++) {
            int j = (i + 1) % n;
            if ((ys[i] <= y && ys[j] > y) || (ys[j] <= y && ys[i] > y)) xi[k++] = xs[i] + (y - ys[i]) / (ys[j] - ys[i]) * (xs[j] - xs[i]);
        }
        for (int s = 1; s < k; s++) { double v = xi[s]; int b = s - 1; while (b >= 0 && xi[b] > v) { xi[b + 1] = xi[b]; b--; } xi[b + 1] = v; }
        for (int s = 0; s + 1 < k; s += 2)
            pdfw_retangulo(a->pdf, a->mx + tw(a, xi[s]), a->my + tw(a, y - 0.5), tw(a, xi[s + 1] - xi[s]), tw(a, 1.0) + 2, c);
    }
}
static void p_texto(alvo_t *a, int x, int y, const char *t, uint32_t cor)
{
    /* La fuente de pantalla mide 8 x 16 px: el cuerpo de la letra es 13 px y la línea base cae a 12 px del borde. */
    int cuerpo = (int)(13 * a->k);
    pdfw_texto(a->pdf, a->mx + tw(a, x), a->my + tw(a, y + 12), PDFW_SANS, 0, cuerpo, cor, t, (int)tam_texto(t), 0);
}

/* ============================================================ vista y escena */

#define COR_FUNDO_VISTA 0xFFFFFF

static double ox_, oy_, esc = 40.0;      /* origen del plano (px dentro de la vista) y píxeles por unidad */
static int vw = 600, vh = 400;
static bool ver_grade = true, ver_ejes = true;
static int sel = -1;                     /* objeto elegido */

static double w2x(double x) { return ox_ + x * esc; }
static double w2y(double y) { return oy_ - y * esc; }
static double s2x(double px) { return (px - ox_) / esc; }
static double s2y(double py) { return (oy_ - py) / esc; }

static double passo_bonito(double alvo)
{
    double m = 1.0;
    while (m * 10 <= alvo) m *= 10;
    while (m > alvo) m /= 10;
    if (m * 1 >= alvo) return m;
    if (m * 2 >= alvo) return m * 2;
    if (m * 5 >= alvo) return m * 5;
    return m * 10;
}

/* Recorta un segmento a la vista (Liang-Barsky). */
static bool recorta_vista(double *x0, double *y0, double *x1, double *y1, double xa, double ya, double xb, double yb)
{
    double t0 = 0, t1 = 1, dx = *x1 - *x0, dy = *y1 - *y0;
    double p[4] = { -dx, dx, -dy, dy }, q[4] = { *x0 - xa, xb - *x0, *y0 - ya, yb - *y0 };
    for (int i = 0; i < 4; i++) {
        if (p[i] == 0) { if (q[i] < 0) return false; }
        else {
            double r = q[i] / p[i];
            if (p[i] < 0) { if (r > t1) return false; if (r > t0) t0 = r; }
            else { if (r < t0) return false; if (r < t1) t1 = r; }
        }
    }
    double ax = *x0, ay = *y0;
    *x0 = ax + t0 * dx; *y0 = ay + t0 * dy; *x1 = ax + t1 * dx; *y1 = ay + t1 * dy;
    return true;
}

/* Trazo con estilo: continuo, discontinuo (1) o punteado (2). */
static double fase_trazo;
static void trazo(alvo_t *a, double x0, double y0, double x1, double y1, double w, uint32_t cor, int estilo)
{
    if (estilo == 0) { a->linha(a, x0, y0, x1, y1, w, cor); return; }
    double on = estilo == 1 ? 9 : 1.5, off = estilo == 1 ? 6 : 4.5 + w, per = on + off;
    double dx = x1 - x0, dy = y1 - y0, l = gm_sqrt(dx * dx + dy * dy);
    if (l < 1e-9) return;
    double pos = 0;
    while (pos < l) {
        double f = gm_fmod(fase_trazo + pos, per);
        double fin = f < on ? mn(l, pos + (on - f)) : mn(l, pos + (per - f));
        if (f < on) a->linha(a, x0 + dx * pos / l, y0 + dy * pos / l, x0 + dx * fin / l, y0 + dy * fin / l, w, cor);
        pos = fin;
        if (fin <= pos && pos < l) pos += 0.01;
    }
    fase_trazo = gm_fmod(fase_trazo + l, per);
}

static void numero_eje(char *s, int max, double v, double passo)
{
    int dec = 0;
    double p = passo;
    while (p < 0.99 && dec < 8) { p *= 10; dec++; }
    gbm_formatar(v, dec, s, max);
}

static void grade_ejes(alvo_t *a)
{
    double passo = passo_bonito(72.0 / esc), menor = passo / 5.0;
    double x_ini = s2x(0), x_fim = s2x(vw), y_fim = s2y(0), y_ini = s2y(vh);
    char t[32];
    if (ver_grade) {
        if (menor * esc >= 8) {
            for (double x = gm_ceil(x_ini / menor) * menor; x <= x_fim; x += menor) { double px = gm_floor(w2x(x)) + 0.5; a->linha(a, px, 0, px, vh, 1, 0xF0F0F0); }
            for (double y = gm_ceil(y_ini / menor) * menor; y <= y_fim; y += menor) { double py = gm_floor(w2y(y)) + 0.5; a->linha(a, 0, py, vw, py, 1, 0xF0F0F0); }
        }
        for (double x = gm_ceil(x_ini / passo) * passo; x <= x_fim; x += passo) { double px = gm_floor(w2x(x)) + 0.5; a->linha(a, px, 0, px, vh, 1, 0xD4D4D4); }
        for (double y = gm_ceil(y_ini / passo) * passo; y <= y_fim; y += passo) { double py = gm_floor(w2y(y)) + 0.5; a->linha(a, 0, py, vw, py, 1, 0xD4D4D4); }
    }
    if (!ver_ejes) return;
    double ax = gm_floor(w2x(0)) + 0.5, ay = gm_floor(w2y(0)) + 0.5;
    if (ay >= 0 && ay <= vh) a->linha(a, 0, ay, vw, ay, 1.4, 0x404040);
    if (ax >= 0 && ax <= vw) a->linha(a, ax, 0, ax, vh, 1.4, 0x404040);
    /* marcas y números */
    double ly = ay < 4 ? 4 : ay > vh - 20 ? vh - 20 : ay;
    double lx = ax < 4 ? 4 : ax > vw - 8 ? vw - 8 : ax;
    for (double x = gm_ceil(x_ini / passo) * passo; x <= x_fim; x += passo) {
        double px = w2x(x);
        if (gm_abs(x) < passo * 1e-6) continue;
        if (ay >= 0 && ay <= vh) a->linha(a, gm_floor(px) + 0.5, ay - 3, gm_floor(px) + 0.5, ay + 3, 1, 0x404040);
        numero_eje(t, sizeof t, x, passo);
        a->texto(a, (int)(px - tam_texto(t) * 4), (int)ly + 4, t, 0x404040);
    }
    for (double y = gm_ceil(y_ini / passo) * passo; y <= y_fim; y += passo) {
        double py = w2y(y);
        if (gm_abs(y) < passo * 1e-6) continue;
        if (ax >= 0 && ax <= vw) a->linha(a, ax - 3, gm_floor(py) + 0.5, ax + 3, gm_floor(py) + 0.5, 1, 0x404040);
        numero_eje(t, sizeof t, y, passo);
        int len = (int)tam_texto(t) * 8;
        int tx = (int)lx - len - 6;
        if (ax < len + 10) tx = (int)lx + 6;
        a->texto(a, tx, (int)(py - 8), t, 0x404040);
    }
    if (ax >= 0 && ax <= vw && ay >= 0 && ay <= vh) a->texto(a, (int)ax - 12, (int)ay + 3, "0", 0x404040);
}

static uint32_t cor_obj(const gbm_obj_t *o) { return o->cor; }

/* Punto de la pantalla de un objeto, para colocar su etiqueta. */
static bool ancla_rotulo(int i, double *sx, double *sy)
{
    gbm_obj_t *o = &gbm_obj[i];
    if (!o->ok) return false;
    if (gbm_eh_ponto(o->tipo)) { *sx = w2x(o->v[0]) + 7; *sy = w2y(o->v[1]) - 20; return true; }
    if (gbm_eh_reta(o->tipo)) {
        double x0 = w2x(o->v[0]), y0 = w2y(o->v[1]), x1 = w2x(o->v[2]), y1 = w2y(o->v[3]);
        if (o->tipo == G_SEGMENTO || o->tipo == G_VETOR) { *sx = (x0 + x1) / 2 + 4; *sy = (y0 + y1) / 2 - 18; return true; }
        double l = gm_hypot(x1 - x0, y1 - y0);
        if (l < 1e-9) return false;
        double ux = (x1 - x0) / l, uy = (y1 - y0) / l, a0x = x0 - ux * 1e6, a0y = y0 - uy * 1e6, a1x = x0 + ux * 1e6, a1y = y0 + uy * 1e6;
        if (o->tipo == G_SEMIRRETA) { a0x = x0; a0y = y0; }
        if (!recorta_vista(&a0x, &a0y, &a1x, &a1y, 8, 8, vw - 8, vh - 8)) return false;
        *sx = a0x + (a1x - a0x) * 0.7 + 4; *sy = a0y + (a1y - a0y) * 0.7 - 18;
        return true;
    }
    if (gbm_eh_circ(o->tipo)) { *sx = w2x(o->v[0] + o->v[2] * 0.7071) + 4; *sy = w2y(o->v[1] + o->v[2] * 0.7071) - 18; return true; }
    if (o->tipo == G_DIST) {
        double x0, y0, x1, y1;
        if (!gbm_ponto_xy(o->pai[0], &x0, &y0)) return false;
        if (o->k == 1) { *sx = w2x(x0) + 6; *sy = w2y(y0) + 4; return true; }
        if (!gbm_ponto_xy(o->pai[1], &x1, &y1)) return false;
        *sx = w2x((x0 + x1) / 2) + 4; *sy = w2y((y0 + y1) / 2) + 2;
        return true;
    }
    if (o->tipo == G_ANGULO) {
        double x, y;
        if (!gbm_ponto_xy(o->pai[1], &x, &y)) return false;
        *sx = w2x(x) + 20; *sy = w2y(y) - 30;
        return true;
    }
    if (gbm_eh_poligono(o->tipo)) {
        double xs[64], ys[64];
        int n = gbm_vertices(i, xs, ys, 64);
        if (!n) return false;
        double cx = 0, cy = 0;
        for (int k = 0; k < n; k++) { cx += xs[k]; cy += ys[k]; }
        *sx = w2x(cx / n) - 8; *sy = w2y(cy / n) - 8;
        return true;
    }
    if (o->tipo == G_AREA) {
        double xs[64], ys[64];
        int n = gbm_vertices(o->pai[0], xs, ys, 64);
        if (n) { double cx = 0, cy = 0; for (int k = 0; k < n; k++) { cx += xs[k]; cy += ys[k]; } *sx = w2x(cx / n) - 16; *sy = w2y(cy / n) + 6; return true; }
        double x, y;
        if (gbm_obj[o->pai[0]].ok) { x = gbm_obj[o->pai[0]].v[0]; y = gbm_obj[o->pai[0]].v[1]; *sx = w2x(x) - 16; *sy = w2y(y) + 6; return true; }
    }
    return false;
}

/* La gráfica de y = f(x): muestreo adaptativo; la línea se corta donde la función no existe o salta. */
typedef struct { alvo_t *a; int obj; double w; uint32_t cor; int estilo; bool ult_ok; double ult_x, ult_y; } graf_t;

static double fy(int obj, double xw) { double v = gbm_valor_funcao(obj, xw); return gm_fin(v) ? w2y(v) : GM_NAN; }

static void graf_seg(graf_t *g, double ax, double ay, double bx, double by)
{
    double lim = 1e6;
    if (ay > lim) ay = lim; else if (ay < -lim) ay = -lim;
    if (by > lim) by = lim; else if (by < -lim) by = -lim;
    double x0 = w2x(ax), x1 = w2x(bx);
    if (!recorta_vista(&x0, &ay, &x1, &by, -4, -4, vw + 4, vh + 4)) return;
    trazo(g->a, x0, ay, x1, by, g->w, g->cor, g->estilo);
}

static void graf_rec(graf_t *g, double xa, double ya, double xb, double yb, int prof)
{
    bool oa = !gm_nan(ya), ob = !gm_nan(yb);
    if (!oa && !ob && prof > 3) return;
    double xm = (xa + xb) / 2, ym = fy(g->obj, xm);
    bool om = !gm_nan(ym);
    if (!oa || !ob || !om) {
        if (prof >= 10) { if (oa && om) graf_seg(g, xa, ya, xm, ym); else if (om && ob) graf_seg(g, xm, ym, xb, yb); return; }
        graf_rec(g, xa, ya, xm, ym, prof + 1);
        graf_rec(g, xm, ym, xb, yb, prof + 1);
        return;
    }
    double dev = gm_abs(ym - (ya + yb) / 2);
    if (prof < 9 && (dev > 0.6 || (gm_abs(yb - ya) > 40 && prof < 7))) {
        graf_rec(g, xa, ya, xm, ym, prof + 1);
        graf_rec(g, xm, ym, xb, yb, prof + 1);
        return;
    }
    if (gm_abs(yb - ya) > 3.0 * vh && prof >= 9) return;      /* salto de una asíntota */
    graf_seg(g, xa, ya, xb, yb);
}

static void desenhar_funcao(alvo_t *a, int i, double w, uint32_t cor, int estilo)
{
    graf_t g = { a, i, w, cor, estilo, false, 0, 0 };
    fase_trazo = 0;
    double paso = 4.0 / esc, x = s2x(-4), xf = s2x(vw + 4);
    double yp = fy(i, x);
    for (double xn = x + paso; xn < xf + paso; xn += paso) {
        double yn = fy(i, xn);
        graf_rec(&g, x, yp, xn, yn, 0);
        x = xn; yp = yn;
    }
}

static void desenhar_circ(alvo_t *a, double cx, double cy, double r, double w, uint32_t cor, int estilo)
{
    double rp = r * esc, px = w2x(cx), py = w2y(cy);
    if (rp > 1e6 || rp < 0.01) return;
    int n = (int)(rp * 0.9);
    n = n < 24 ? 24 : n > 720 ? 720 : n;
    double ang0 = 0;
    fase_trazo = 0;
    double xa = px + rp, ya = py;
    for (int k = 1; k <= n; k++) {
        double th = ang0 + 2 * GM_PI * k / n, xb = px + rp * gm_cos(th), yb = py - rp * gm_sin(th);
        double x0 = xa, y0 = ya, x1 = xb, y1 = yb;
        if (recorta_vista(&x0, &y0, &x1, &y1, -6, -6, vw + 6, vh + 6)) trazo(a, x0, y0, x1, y1, w, cor, estilo);
        xa = xb; ya = yb;
    }
}

static void seta(alvo_t *a, double x0, double y0, double x1, double y1, double w, uint32_t cor)
{
    double dx = x1 - x0, dy = y1 - y0, l = gm_hypot(dx, dy);
    if (l < 3) return;
    dx /= l; dy /= l;
    double c = 11.0, s = 4.5;
    a->linha(a, x1, y1, x1 - dx * c + dy * s, y1 - dy * c - dx * s, w, cor);
    a->linha(a, x1, y1, x1 - dx * c - dy * s, y1 - dy * c + dx * s, w, cor);
}

static void desenhar_obj(alvo_t *a, int i, bool realce, int pasada)
{
    gbm_obj_t *o = &gbm_obj[i];
    if (!o->visivel || !o->ok) return;
    uint32_t cor = cor_obj(o);
    double w = o->grosor * 0.9 + 0.6 + (realce ? 1.6 : 0);
    int t = o->tipo;
    if (pasada == 0) {
        if (gbm_eh_poligono(t)) {
            double xs[64], ys[64], px[64], py[64];
            int n = gbm_vertices(i, xs, ys, 64);
            for (int k = 0; k < n; k++) { px[k] = w2x(xs[k]); py[k] = w2y(ys[k]); }
            a->preenche(a, px, py, n, cor);
        }
        return;
    }
    if (pasada == 1) {
        if (gbm_eh_reta(t)) {
            double x0 = w2x(o->v[0]), y0 = w2y(o->v[1]), x1 = w2x(o->v[2]), y1 = w2y(o->v[3]);
            double l = gm_hypot(x1 - x0, y1 - y0);
            if (l < 1e-9) return;
            double ux = (x1 - x0) / l, uy = (y1 - y0) / l;
            double sx0 = x0, sy0 = y0, sx1 = x1, sy1 = y1;
            if (t == G_RETA || t == G_PERP || t == G_PARAL || t == G_MEDIATRIZ || t == G_BISSETRIZ) { sx0 = x0 - ux * 1e6; sy0 = y0 - uy * 1e6; sx1 = x0 + ux * 1e6; sy1 = y0 + uy * 1e6; }
            else if (t == G_SEMIRRETA) { sx1 = x0 + ux * 1e6; sy1 = y0 + uy * 1e6; }
            fase_trazo = 0;
            double cx0 = sx0, cy0 = sy0, cx1 = sx1, cy1 = sy1;
            if (recorta_vista(&cx0, &cy0, &cx1, &cy1, -4, -4, vw + 4, vh + 4)) {
                trazo(a, cx0, cy0, cx1, cy1, w, cor, o->estilo);
                if (t == G_VETOR) seta(a, x0, y0, x1, y1, w, cor);
            }
        } else if (gbm_eh_circ(t)) {
            desenhar_circ(a, o->v[0], o->v[1], o->v[2], w, cor, o->estilo);
        } else if (gbm_eh_poligono(t)) {
            double xs[64], ys[64];
            int n = gbm_vertices(i, xs, ys, 64);
            fase_trazo = 0;
            for (int k = 0; k < n; k++) {
                int m = (k + 1) % n;
                trazo(a, w2x(xs[k]), w2y(ys[k]), w2x(xs[m]), w2y(ys[m]), w, cor, o->estilo);
            }
        } else if (t == G_FUNCAO) {
            desenhar_funcao(a, i, w, cor, o->estilo);
        } else if (t == G_ANGULO) {
            double ax, ay, bx, by, cx, cy;
            if (gbm_ponto_xy(o->pai[0], &ax, &ay) && gbm_ponto_xy(o->pai[1], &bx, &by) && gbm_ponto_xy(o->pai[2], &cx, &cy)) {
                double a0 = gm_atan2(ay - by, ax - bx), px = w2x(bx), py = w2y(by), r = 26;
                int n = (int)(o->v[0] / GM_PI * 24) + 3;
                double xa = px + r * gm_cos(a0), ya = py - r * gm_sin(a0);
                for (int k = 1; k <= n; k++) {
                    double th = a0 + o->v[0] * k / n, xb = px + r * gm_cos(th), yb = py - r * gm_sin(th);
                    a->linha(a, xa, ya, xb, yb, w * 0.8, cor);
                    xa = xb; ya = yb;
                }
            }
        } else if (t == G_TEXTO) {
            a->texto(a, (int)w2x(o->px), (int)w2y(o->py) - 8, o->texto, cor);
        }
        return;
    }
    /* pasada 2: puntos y etiquetas */
    if (gbm_eh_ponto(t)) a->disco(a, w2x(o->v[0]), w2y(o->v[1]), 1.5 + o->grosor + (realce ? 1.5 : 0), cor);
    if (o->rotulo && t != G_TEXTO) {
        double sx, sy;
        if (ancla_rotulo(i, &sx, &sy)) {
            char s[160];
            if (gbm_eh_ponto(t) || gbm_eh_reta(t) || gbm_eh_circ(t) || gbm_eh_poligono(t) || t == G_FUNCAO) copia_texto(s, o->nome, sizeof s);
            else gbm_descrever(i, s, sizeof s);
            a->texto(a, (int)sx, (int)sy, s, cor);
        }
    }
}

static void escena(alvo_t *a, bool para_pantalla)
{
    grade_ejes(a);
    for (int pasada = 0; pasada < 3; pasada++) {
        for (int i = 0; i < gbm_n; i++) desenhar_obj(a, i, para_pantalla && i == sel, pasada);
        if (pasada == 1 && para_pantalla) { /* el realce de lo elegido ya va en el grosor */ }
    }
}

/* ============================================================ estado del programa */

enum {
    T_MOVER = 1, T_PUNTO, T_SEGMENTO, T_RECTA, T_SEMIRRECTA, T_VECTOR, T_CIRC_PP, T_CIRC_R, T_PMEDIO, T_INTERSECA, T_PERP, T_PARAL,
    T_MEDIATRIZ, T_BISECTRIZ, T_POLIGONO, T_POLREG, T_ANGULO, T_DIST, T_AREA, T_TEXTO, T_SLIDER, T_BORRAR, T_N
};
enum {
    A_NUEVO = 100, A_ABRIR, A_GUARDAR, A_GUARDAR_COMO, A_PNG, A_PDF, A_SALIR, A_DESHACER, A_REHACER, A_BORRAR, A_PROPIEDADES,
    A_VERTODO, A_RESTABLECER, A_GRADE, A_EJES, A_ALGEBRA, A_DEC0, A_DEC1, A_DEC2, A_DEC3, A_DEC4, A_DEC5,
    A_EJ1, A_EJ2, A_EJ3, A_EJ4, A_ORDENES, A_ACERCA, A_ZOOM_MAS, A_ZOOM_MENOS,
};

#define MENU_A 20
#define FERR_Y 20
#define FERR_A 28
#define TOP (MENU_A + FERR_A)
#define ENTRADA_A 30
#define ESTADO_A 22

static jan_t *j;
static char caminho[200];
static bool modificado;
static int ferr = T_MOVER;
static int algebra_l = 250;
static bool algebra_vis = true;
static int alg_scroll;
static int vx, vy;                      /* esquina de la vista en la ventana */
static barra_menu_t menu;
static barra_ferr_t barra;
static barra_item_t itens[48];
static int n_itens;
static barra_estado_t estado;
static caixa_t entrada;
static char ult_titulo[160];
static char msg[128];
static bool msg_erro;
static bool sair_pedido;

/* historial para deshacer: texto de la construcción tras cada cambio */
#define HIST_MAX 40
static char *hist[HIST_MAX];
static int hist_n, hist_pos;
static char tmp_ser[131072];

static const char *const nomes_tool[T_N] = {
    "", "Mover", "Punto", "Segmento", "Recta", "Semirrecta", "Vector", "Circunferencia (centro y punto)", "Circunferencia (centro y radio)",
    "Punto medio", "Intersección", "Perpendicular", "Paralela", "Mediatriz", "Bisectriz", "Polígono", "Polígono regular", "Ángulo",
    "Distancia", "Área", "Texto", "Deslizador", "Borrar",
};
static const char *const ayuda_tool[T_N] = {
    "", "Arrastra puntos, deslizadores y la vista; doble clic = propiedades", "Clic en el plano (o sobre un objeto) para poner un punto",
    "Elige dos puntos", "Elige dos puntos", "Elige dos puntos: origen y dirección", "Elige dos puntos: origen y punta", "Elige el centro y un punto de la circunferencia",
    "Elige el centro; después se pide el radio", "Elige dos puntos o un segmento", "Elige dos objetos", "Elige un punto y una recta", "Elige un punto y una recta",
    "Elige dos puntos o un segmento", "Elige tres puntos (el vértice en medio)", "Elige los vértices; clic en el primero para cerrar", "Elige dos puntos; después el número de lados",
    "Elige tres puntos (el vértice en medio)", "Elige dos puntos, un segmento o un punto y una recta", "Elige un polígono o una circunferencia", "Clic donde irá el texto",
    "Clic en el plano para crear un deslizador", "Clic sobre el objeto que quieres borrar",
};

static int pend[GBM_PAI_MAX + 2], npend;          /* objetos ya elegidos en la herramienta actual */
static int arrastra_obj = -1;                       /* qué se arrastra */
enum { D_NADA, D_PAN, D_OBJ, D_SLIDER, D_TRASLADA, D_DIVISOR };
static int modo_arrastre;
static double arr_x0, arr_y0;                       /* el plano bajo el cursor al empezar */
static int arr_sx, arr_sy;
static double arr_ox, arr_oy;
static bool arr_movio;
static evento_t adiado;
static bool tem_adiado;
static int mouse_x = -1, mouse_y = -1;

/* ---- mensajes ---- */
static void dizer(const char *t, bool erro) { copia_texto(msg, t, sizeof msg); msg_erro = erro; }

/* ---- guardado y deshacer ---- */

static void vista_arr(double *v) { v[0] = s2x(vw / 2.0); v[1] = s2y(vh / 2.0); v[2] = esc; }

static void estado_guardar(void)
{
    gbm_serializar(tmp_ser, sizeof tmp_ser, NULL);
    if (hist_n && hist_pos >= 0 && compara_texto(hist[hist_pos], tmp_ser) == 0) return;
    for (int i = hist_pos + 1; i < hist_n; i++) { libera(hist[i]); hist[i] = NULL; }
    hist_n = hist_pos + 1;
    if (hist_n >= HIST_MAX) { libera(hist[0]); memmove(hist, hist + 1, sizeof(char *) * (HIST_MAX - 1)); hist_n--; }
    size_t n = tam_texto(tmp_ser) + 1;
    char *c = aloca(n);
    if (!c) return;
    memcpy(c, tmp_ser, n);
    hist[hist_n++] = c;
    hist_pos = hist_n - 1;
    modificado = hist_n > 1 || modificado;
}

static void historial_reiniciar(void)
{
    for (int i = 0; i < hist_n; i++) { libera(hist[i]); hist[i] = NULL; }
    hist_n = 0; hist_pos = -1;
    estado_guardar();
    modificado = false;
}

static void titulo(void)
{
    char t[260];
    const char *b = caminho[0] ? acha_ultimo_char(caminho, '/') : NULL;
    formatar_texto(t, sizeof t, "%s - GebraMini%s", caminho[0] ? (b ? b + 1 : caminho) : "Sin título", modificado ? " *" : "");
    if (compara_texto(t, ult_titulo) == 0) return;
    copia_texto(ult_titulo, t, sizeof ult_titulo);
    titulo_janela(j->id, t);
    registrar("gebramini: titulo %s\n", t);
}

static void vista_ver_todo(void);

/* ---- dibujo de la ventana ---- */

static void calcular_geometria(void)
{
    vx = algebra_vis ? algebra_l + 4 : 0;
    vy = TOP;
    vw = j->larg - vx;
    vh = j->alt - TOP - ENTRADA_A - ESTADO_A;
    if (vw < 50) vw = 50;
    if (vh < 50) vh = 50;
}

static void layout(void)
{
    calcular_geometria();
    barra_menu_iniciar(&menu, 0, 0, j->larg);
    barra_ferr_iniciar(&barra, 0, FERR_Y, j->larg, itens, n_itens);
    static const int larg[2] = { 130, 100 };
    barra_estado_iniciar(&estado, 0, j->alt - ESTADO_A, j->larg, 3, larg);
    entrada.x = 74; entrada.y = j->alt - ESTADO_A - ENTRADA_A + 3; entrada.l = j->larg - 74 - 10;
}

static void estado_atualizar(void)
{
    char t[64], a[24], b[24];
    if (mouse_x >= vx && mouse_x < vx + vw && mouse_y >= vy && mouse_y < vy + vh) {
        gbm_formatar(s2x(mouse_x - vx), 3, a, sizeof a);
        gbm_formatar(s2y(mouse_y - vy), 3, b, sizeof b);
        formatar_texto(t, sizeof t, "(%s, %s)", a, b);
    } else t[0] = 0;
    barra_estado_texto(&estado, 0, t);
    formatar_texto(t, sizeof t, "%d objetos", gbm_n);
    barra_estado_texto(&estado, 1, t);
    barra_estado_texto(&estado, 2, msg);
    barra_estado_desenhar(j, &estado);
}

/* Deslizadores sobre la vista (en píxeles de la ventana). */
#define SL_L 150
static bool slider_rect(int i, int *x, int *y)
{
    int k = 0;
    for (int m = 0; m < gbm_n; m++) {
        if (gbm_obj[m].tipo != G_SLIDER || !gbm_obj[m].visivel) continue;
        if (m == i) { *x = vx + 14; *y = vy + 10 + k * 38; return true; }
        k++;
    }
    return false;
}

static double slider_valor(gbm_obj_t *o, double f)
{
    double v = o->min + f * (o->max - o->min);
    if (o->passo > 0) v = o->min + gm_round((v - o->min) / o->passo) * o->passo;
    if (v < o->min) v = o->min;
    if (v > o->max) v = o->max;
    return v;
}

static void desenhar_sliders(void)
{
    for (int i = 0; i < gbm_n; i++) {
        gbm_obj_t *o = &gbm_obj[i];
        int x, y;
        if (o->tipo != G_SLIDER || !slider_rect(i, &x, &y)) continue;
        char s[96];
        gbm_descrever(i, s, sizeof s);
        g_retangulo(j, x - 4, y - 2, SL_L + 24, 34, 0xF8F8F4);
        g_moldura(j, x - 4, y - 2, SL_L + 24, 34, i == sel ? W98_AZUL : 0xC8C8C0);
        g_texto(j, x, y, s, o->cor);
        g_retangulo(j, x, y + 22, SL_L, 3, 0xB0B0B0);
        double f = (o->v[0] - o->min) / (o->max - o->min);
        int kx = x + (int)(f * SL_L);
        g_retangulo(j, kx - 5, y + 17, 11, 13, o->cor);
        g_moldura(j, kx - 5, y + 17, 11, 13, 0x303030);
    }
}

static void desenhar_vista(void)
{
    alvo_t a;
    memset(&a, 0, sizeof a);
    a.linha = r_linha; a.disco = r_disco; a.preenche = r_preenche; a.texto = r_texto;
    a.px = j->px; a.larg = j->larg; a.alt = j->alt; a.ox = vx; a.oy = vy;
    a.cx0 = vx; a.cy0 = vy; a.cx1 = vx + vw; a.cy1 = vy + vh;
    g_retangulo(j, vx, vy, vw, vh, COR_FUNDO_VISTA);
    escena(&a, true);
    desenhar_sliders();
    g_moldura(j, vx, vy, vw, vh, 0x808080);
}

#define LINHA_A 20
static void desenhar_algebra(void)
{
    if (!algebra_vis) return;
    int h = j->alt - TOP - ENTRADA_A - ESTADO_A;
    g_retangulo(j, 0, TOP, algebra_l, h, W98_LUZ);
    g_retangulo(j, 0, TOP, algebra_l, 18, W98_FACE);
    g_texto(j, 6, TOP + 1, "Vista algebraica", W98_NEGRO);
    g_retangulo(j, algebra_l, TOP, 4, h, W98_FACE);
    int y = TOP + 20 - alg_scroll * LINHA_A;
    int cols = (algebra_l - 30) / 8;
    for (int i = 0; i < gbm_n; i++, y += LINHA_A) {
        if (y < TOP + 18 || y + LINHA_A > TOP + h) continue;
        gbm_obj_t *o = &gbm_obj[i];
        if (i == sel) g_retangulo(j, 1, y, algebra_l - 2, LINHA_A - 1, W98_AZUL);
        /* viñeta de visibilidad: círculo lleno = visible */
        uint32_t c = o->cor;
        g_moldura(j, 6, y + 4, 12, 12, 0x606060);
        if (o->visivel) g_retangulo(j, 8, y + 6, 8, 8, c);
        char s[160];
        gbm_descrever(i, s, sizeof s);
        int n = (int)tam_texto(s);
        if (n > cols) { s[cols - 2] = '.'; s[cols - 1] = '.'; s[cols] = 0; }
        g_texto(j, 24, y + 2, s, i == sel ? W98_LUZ : (o->ok ? W98_NEGRO : 0x909090));
    }
}

static void desenhar_entrada(void)
{
    int y = j->alt - ESTADO_A - ENTRADA_A;
    g_retangulo(j, 0, y, j->larg, ENTRADA_A, W98_FACE);
    g_retangulo(j, 0, y, j->larg, 1, W98_SOMBRA);
    g_texto(j, 8, y + 7, "Entrada:", W98_NEGRO);
    caixa_desenhar(j, &entrada);
}

static void redesenhar_tudo(void)
{
    g_retangulo(j, 0, 0, j->larg, j->alt, W98_FACE);
    barra_menu_desenhar(j, &menu);
    barra_ferr_desenhar(j, &barra);
    desenhar_algebra();
    desenhar_vista();
    desenhar_entrada();
    estado_atualizar();
    titulo();
    jan_mostrar(j);
}

static void redesenhar_vista(void)
{
    desenhar_vista();
    jan_mostrar_area(j, vx, vy, vw, vh);
}

static void redesenhar_vista_algebra(void)
{
    desenhar_algebra();
    desenhar_vista();
    estado_atualizar();
    titulo();
    jan_mostrar_area(j, 0, TOP, j->larg, vh);
    jan_mostrar_area(j, 0, j->alt - ESTADO_A, j->larg, ESTADO_A);
}

/* ============================================================ iconos de las herramientas (16x16, estilo Win98) */

#define R(x_, y_, w_, h_, c_) g_retangulo(j, x + (x_), y + (y_), (w_), (h_), (c_))
#define L(x0, y0, x1, y1, c_) g_linha(j, x + (x0), y + (y0), x + (x1), y + (y1), (c_))
#define NEG 0x000000
#define AZUL 0x2255CC
#define ROJO 0xCC2222
#define GRIS 0x808080

static void pto(jan_t *jj, int x, int y, int cx, int cy, uint32_t c)
{
    jan_t *j = jj;
    R(cx - 1, cy - 2, 3, 5, NEG); R(cx - 2, cy - 1, 5, 3, NEG);
    R(cx - 1, cy - 1, 3, 3, c);
}
static void circulo(jan_t *jj, int x, int y, int cx, int cy, int r, uint32_t c)
{
    jan_t *j = jj;
    int px = r, py = 0, e = 1 - r;
    while (px >= py) {
        R(cx + px, cy + py, 1, 1, c); R(cx - px, cy + py, 1, 1, c); R(cx + px, cy - py, 1, 1, c); R(cx - px, cy - py, 1, 1, c);
        R(cx + py, cy + px, 1, 1, c); R(cx - py, cy + px, 1, 1, c); R(cx + py, cy - px, 1, 1, c); R(cx - py, cy - px, 1, 1, c);
        py++;
        if (e < 0) e += 2 * py + 1; else { px--; e += 2 * (py - px) + 1; }
    }
}

static void ic_mover(jan_t *j, int x, int y, bool a)
{
    (void)a;
    for (int k = 0; k < 9; k++) { R(4, 1 + k, k + 1, 1, NEG); if (k) R(5, 1 + k, k - 1 > 0 ? k - 1 : 0, 1, W98_LUZ); }
    R(4, 1, 1, 11, NEG); R(7, 9, 3, 5, NEG); R(8, 9, 1, 5, W98_LUZ);
}
static void ic_punto(jan_t *j, int x, int y, bool a) { (void)a; pto(j, x, y, 8, 8, AZUL); R(9, 3, 4, 1, GRIS); R(10, 2, 2, 3, GRIS); }
static void ic_segmento(jan_t *j, int x, int y, bool a) { (void)a; L(3, 12, 12, 3, NEG); pto(j, x, y, 3, 12, AZUL); pto(j, x, y, 12, 3, AZUL); }
static void ic_recta(jan_t *j, int x, int y, bool a) { (void)a; L(0, 14, 15, 1, NEG); L(0, 15, 15, 2, NEG); pto(j, x, y, 4, 11, AZUL); pto(j, x, y, 11, 5, AZUL); }
static void ic_semirrecta(jan_t *j, int x, int y, bool a) { (void)a; L(3, 12, 15, 4, NEG); L(3, 13, 15, 5, NEG); pto(j, x, y, 3, 12, AZUL); pto(j, x, y, 9, 8, AZUL); }
static void ic_vector(jan_t *j, int x, int y, bool a) { (void)a; L(2, 13, 12, 3, NEG); L(3, 13, 13, 3, NEG); L(12, 3, 7, 3, NEG); L(12, 3, 12, 8, NEG); L(13, 2, 13, 9, NEG); }
static void ic_circ_pp(jan_t *j, int x, int y, bool a) { (void)a; circulo(j, x, y, 8, 8, 6, NEG); pto(j, x, y, 8, 8, AZUL); pto(j, x, y, 14, 8, AZUL); }
static void ic_circ_r(jan_t *j, int x, int y, bool a) { (void)a; circulo(j, x, y, 8, 8, 6, NEG); pto(j, x, y, 8, 8, AZUL); L(8, 8, 13, 5, ROJO); }
static void ic_pmedio(jan_t *j, int x, int y, bool a) { (void)a; L(2, 13, 14, 3, NEG); pto(j, x, y, 2, 13, AZUL); pto(j, x, y, 14, 3, AZUL); pto(j, x, y, 8, 8, ROJO); }
static void ic_interseca(jan_t *j, int x, int y, bool a) { (void)a; L(1, 3, 14, 13, NEG); L(1, 13, 14, 3, NEG); pto(j, x, y, 8, 8, ROJO); }
static void ic_perp(jan_t *j, int x, int y, bool a) { (void)a; L(1, 12, 14, 12, NEG); L(7, 1, 7, 14, NEG); R(7, 9, 3, 1, ROJO); R(9, 9, 1, 3, ROJO); pto(j, x, y, 7, 5, AZUL); }
static void ic_paral(jan_t *j, int x, int y, bool a) { (void)a; L(1, 11, 11, 2, NEG); L(4, 14, 14, 5, NEG); pto(j, x, y, 7, 11, AZUL); }
static void ic_mediatriz(jan_t *j, int x, int y, bool a) { (void)a; L(1, 12, 14, 12, NEG); pto(j, x, y, 2, 12, AZUL); pto(j, x, y, 13, 12, AZUL); for (int k = 1; k < 14; k += 4) R(8, k, 1, 2, ROJO); }
static void ic_bisectriz(jan_t *j, int x, int y, bool a) { (void)a; L(2, 13, 14, 13, NEG); L(2, 13, 12, 2, NEG); L(2, 13, 13, 6, ROJO); L(2, 12, 13, 5, ROJO); pto(j, x, y, 2, 13, AZUL); }
static void ic_poligono(jan_t *j, int x, int y, bool a) { (void)a; for (int k = 0; k < 6; k++) R(4 + k, 5 + k, 8 - k, 1, 0xF0E0C0); L(2, 13, 8, 2, NEG); L(8, 2, 14, 13, NEG); L(2, 13, 14, 13, NEG); pto(j, x, y, 2, 13, AZUL); pto(j, x, y, 8, 2, AZUL); pto(j, x, y, 14, 13, AZUL); }
static void ic_polreg(jan_t *j, int x, int y, bool a) { (void)a; L(4, 2, 11, 2, NEG); L(11, 2, 14, 8, NEG); L(14, 8, 11, 14, NEG); L(11, 14, 4, 14, NEG); L(4, 14, 1, 8, NEG); L(1, 8, 4, 2, NEG); R(6, 6, 4, 4, 0xF0E0C0); }
static void ic_angulo(jan_t *j, int x, int y, bool a) { (void)a; L(2, 13, 14, 13, NEG); L(2, 13, 12, 3, NEG); R(6, 10, 1, 3, ROJO); R(7, 9, 1, 1, ROJO); R(8, 9, 2, 1, ROJO); R(9, 10, 1, 3, ROJO); pto(j, x, y, 2, 13, AZUL); }
static void ic_dist(jan_t *j, int x, int y, bool a) { (void)a; L(2, 6, 13, 6, NEG); L(2, 6, 4, 4, NEG); L(2, 6, 4, 8, NEG); L(13, 6, 11, 4, NEG); L(13, 6, 11, 8, NEG); R(1, 10, 1, 4, NEG); R(14, 10, 1, 4, NEG); L(1, 12, 14, 12, ROJO); }
static void ic_area(jan_t *j, int x, int y, bool a) { (void)a; for (int k = 0; k < 11; k++) { R(2 + (k < 6 ? 0 : 0), 2 + k, 12, 1, k % 3 == 0 ? NEG : 0xF0E0C0); } R(2, 2, 1, 11, NEG); R(13, 2, 1, 11, NEG); R(2, 12, 12, 1, NEG); }
static void ic_texto(jan_t *j, int x, int y, bool a) { (void)a; R(3, 2, 10, 2, NEG); R(7, 2, 2, 12, NEG); R(5, 13, 6, 1, NEG); }
static void ic_slider(jan_t *j, int x, int y, bool a) { (void)a; R(1, 7, 14, 2, GRIS); R(1, 7, 7, 2, AZUL); R(6, 4, 5, 8, NEG); R(7, 5, 3, 6, W98_FACE); }
static void ic_borrar(jan_t *j, int x, int y, bool a) { (void)a; for (int k = 0; k < 4; k++) { L(3 + k, 3, 12 + k - 3, 12, ROJO); L(12 - k, 3, 3 + 3 - k + 0, 12, ROJO); } }
static void ic_vertodo(jan_t *j, int x, int y, bool a) { (void)a; R(2, 2, 12, 12, W98_NEGRO); R(3, 3, 10, 10, W98_LUZ); L(4, 4, 11, 11, AZUL); L(4, 11, 11, 4, AZUL); pto(j, x, y, 8, 8, ROJO); }
static void ic_zmas(jan_t *j, int x, int y, bool a) { (void)a; circulo(j, x, y, 6, 6, 4, NEG); L(9, 9, 14, 14, NEG); L(10, 9, 15, 14, NEG); R(4, 6, 5, 1, ROJO); R(6, 4, 1, 5, ROJO); }
static void ic_zmenos(jan_t *j, int x, int y, bool a) { (void)a; circulo(j, x, y, 6, 6, 4, NEG); L(9, 9, 14, 14, NEG); L(10, 9, 15, 14, NEG); R(4, 6, 5, 1, ROJO); }

typedef void (*fn_icono)(jan_t *, int, int, bool);
static const fn_icono ICONOS[T_N] = {
    NULL, ic_mover, ic_punto, ic_segmento, ic_recta, ic_semirrecta, ic_vector, ic_circ_pp, ic_circ_r, ic_pmedio, ic_interseca, ic_perp, ic_paral,
    ic_mediatriz, ic_bisectriz, ic_poligono, ic_polreg, ic_angulo, ic_dist, ic_area, ic_texto, ic_slider, ic_borrar,
};

static void itens_iniciar(void)
{
    n_itens = 0;
    for (int t = T_MOVER; t < T_N; t++) {
        barra_item_t it = { t, ICONOS[t], NULL, false, true, t == ferr, false, 0 };
        itens[n_itens++] = it;
        if (t == T_MOVER || t == T_CIRC_R || t == T_BISECTRIZ || t == T_AREA) { barra_item_t s = { 0, NULL, NULL, false, false, false, false, 0 }; itens[n_itens++] = s; }
    }
    barra_item_t s = { 0, NULL, NULL, false, false, false, false, 0 };
    itens[n_itens++] = s;
    barra_item_t a1 = { A_DESHACER, ic98_desfazer, NULL, false, false, false, false, 0 };
    itens[n_itens++] = a1;
    barra_item_t a2 = { A_REHACER, NULL, "Re", false, false, false, false, 0 };
    itens[n_itens++] = a2;
    barra_item_t a3 = { A_VERTODO, ic_vertodo, NULL, false, false, false, false, 0 };
    itens[n_itens++] = a3;
    barra_item_t a4 = { A_ZOOM_MAS, ic_zmas, NULL, false, false, false, false, 0 };
    itens[n_itens++] = a4;
    barra_item_t a5 = { A_ZOOM_MENOS, ic_zmenos, NULL, false, false, false, false, 0 };
    itens[n_itens++] = a5;
}

/* ============================================================ menús */

static const char *const menu_nomes[5] = { "Archivo", "Edición", "Ver", "Ejemplos", "Ayuda" };
static const char *const menu_textos[5] = {
    "Nuevo\tCtrl+N\nAbrir...\tCtrl+O\n-\nGuardar\tCtrl+S\nGuardar como...\n-\nExportar a PNG...\nExportar a PDF...\tCtrl+P\n-\nSalir",
    "Deshacer\tCtrl+Z\nRehacer\tCtrl+Y\n-\nBorrar el objeto elegido\tSupr\nPropiedades...",
    "Ver todo\nRestablecer la vista\n-\nCuadrícula\nEjes\nVista algebraica\n-\nDecimales: 0\nDecimales: 1\nDecimales: 2\nDecimales: 3\nDecimales: 4\nDecimales: 5",
    "Triángulo y sus rectas notables\nCircunferencia por tres puntos\nParábola con deslizadores\nTeorema de Tales",
    "Órdenes disponibles...\nAcerca de GebraMini",
};
static const int menu_acoes[5][16] = {
    { A_NUEVO, A_ABRIR, 0, A_GUARDAR, A_GUARDAR_COMO, 0, A_PNG, A_PDF, 0, A_SALIR },
    { A_DESHACER, A_REHACER, 0, A_BORRAR, A_PROPIEDADES },
    { A_VERTODO, A_RESTABLECER, 0, A_GRADE, A_EJES, A_ALGEBRA, 0, A_DEC0, A_DEC1, A_DEC2, A_DEC3, A_DEC4, A_DEC5 },
    { A_EJ1, A_EJ2, A_EJ3, A_EJ4 },
    { A_ORDENES, A_ACERCA },
};
static const char *const EJEMPLOS[4] = { "triangulo.gbm", "circunferencia-3-puntos.gbm", "parabola.gbm", "tales.gbm" };

#define FILTROS_GBM "Construcción GebraMini (*.gbm)|*.gbm|Todos los archivos (*.*)|*"
#define FILTROS_PNG "Imagen PNG (*.png)|*.png"
#define FILTROS_PDF "Documento PDF (*.pdf)|*.pdf"

/* ============================================================ ficheros */

static char *ler_arquivo(const char *c, int *n)
{
    int fd = abrir(c, ABRIR_LER);
    if (fd < 0) return NULL;
    int cap = 65536, tam = 0, r;
    char *b = aloca((size_t)cap);
    while (b && (r = ler(fd, b + tam, (unsigned)(cap - tam - 1))) > 0) {
        tam += r;
        if (tam >= cap - 1) { char *nb = realoca(b, (size_t)cap * 2); if (!nb) break; b = nb; cap *= 2; }
    }
    fechar(fd);
    if (b) b[tam] = 0;
    *n = tam;
    return b;
}

static bool escrever_arquivo(const char *c, const void *d, int n)
{
    int fd = abrir(c, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return false;
    int r = escrever_tudo(fd, d, (unsigned)n);
    fechar(fd);
    return r == n;
}

static bool abrir_gbm(const char *c)
{
    int n;
    char *b = ler_arquivo(c, &n);
    if (!b) return false;
    double v[3] = { 0, 0, 0 };
    int r = gbm_carregar(b, v);
    libera(b);
    if (r != 0) return false;
    if (v[2] > 0) { esc = v[2]; ox_ = vw / 2.0 - v[0] * esc; oy_ = vh / 2.0 + v[1] * esc; }
    else vista_ver_todo();
    return true;
}

static bool gravar_gbm(const char *c)
{
    double v[3];
    vista_arr(v);
    int n = gbm_serializar(tmp_ser, sizeof tmp_ser, v);
    return escrever_arquivo(c, tmp_ser, n);
}

/* Dibuja la vista actual en una imagen (PNG) y la codifica. */
static int exportar_png(const char *c, int larg, int alt)
{
    imagem_t img;
    if (!imagem_criar(&img, larg, alt, COR_FUNDO_VISTA)) return -1;
    alvo_t a;
    memset(&a, 0, sizeof a);
    a.linha = r_linha; a.disco = r_disco; a.preenche = r_preenche; a.texto = r_texto;
    a.px = img.px; a.larg = larg; a.alt = alt; a.ox = 0; a.oy = 0; a.cx0 = 0; a.cy0 = 0; a.cx1 = larg; a.cy1 = alt;
    int ovw = vw, ovh = vh;
    vw = larg; vh = alt;
    escena(&a, false);
    vw = ovw; vh = ovh;
    uint8_t *d;
    int n;
    int r = imagem_codificar_png(&img, &d, &n);
    imagem_liberar(&img);
    if (r != 0) return -1;
    bool ok = escrever_arquivo(c, d, n);
    libera(d);
    if (ok) registrar("gebramini: png exportado %s (%dx%d, %d bytes)\n", c, larg, alt, n);
    return ok ? 0 : -1;
}

static int exportar_pdf(const char *c)
{
    pdfw_t *w = pdfw_criar(PDFW_A4);
    if (!w) return -1;
    pdfw_tamanho(w, 16838, 11906);                       /* A4 apaisado */
    pdfw_titulo(w, "Construcción de GebraMini");
    pdfw_pagina(w);
    int margen = 700;
    double k1 = (16838 - 2.0 * margen) / vw, k2 = (11906 - 2.0 * margen) / vh;
    alvo_t a;
    memset(&a, 0, sizeof a);
    a.linha = p_linha; a.disco = p_disco; a.preenche = p_preenche; a.texto = p_texto;
    a.pdf = w; a.k = k1 < k2 ? k1 : k2; a.mx = margen; a.my = margen;
    /* marco de la vista */
    pdfw_linha(w, margen, margen, margen + tw(&a, vw), margen, 10, 0x808080);
    pdfw_linha(w, margen, margen + tw(&a, vh), margen + tw(&a, vw), margen + tw(&a, vh), 10, 0x808080);
    pdfw_linha(w, margen, margen, margen, margen + tw(&a, vh), 10, 0x808080);
    pdfw_linha(w, margen + tw(&a, vw), margen, margen + tw(&a, vw), margen + tw(&a, vh), 10, 0x808080);
    escena(&a, false);
    uint8_t *d;
    int n;
    bool ok = pdfw_finalizar(w, &d, &n);
    pdfw_liberar(w);
    if (!ok) return -1;
    ok = escrever_arquivo(c, d, n);
    libera(d);
    if (ok) registrar("gebramini: pdf exportado %s (%d bytes)\n", c, n);
    return ok ? 0 : -1;
}

/* ============================================================ vista */

static void vista_restablecer(void) { esc = 40; ox_ = vw / 2.0; oy_ = vh / 2.0; }

static void vista_ver_todo(void)
{
    double x0 = 1e30, x1 = -1e30, y0 = 1e30, y1 = -1e30;
    int n = 0;
    for (int i = 0; i < gbm_n; i++) {
        gbm_obj_t *o = &gbm_obj[i];
        if (!o->ok || !o->visivel) continue;
        if (gbm_eh_ponto(o->tipo)) { x0 = mn(x0, o->v[0]); x1 = mx_(x1, o->v[0]); y0 = mn(y0, o->v[1]); y1 = mx_(y1, o->v[1]); n++; }
        else if (gbm_eh_circ(o->tipo)) { x0 = mn(x0, o->v[0] - o->v[2]); x1 = mx_(x1, o->v[0] + o->v[2]); y0 = mn(y0, o->v[1] - o->v[2]); y1 = mx_(y1, o->v[1] + o->v[2]); n++; }
        else if (o->tipo == G_SEGMENTO || o->tipo == G_VETOR) { x0 = mn(x0, mn(o->v[0], o->v[2])); x1 = mx_(x1, mx_(o->v[0], o->v[2])); y0 = mn(y0, mn(o->v[1], o->v[3])); y1 = mx_(y1, mx_(o->v[1], o->v[3])); n++; }
    }
    if (!n) { vista_restablecer(); return; }
    double an = mx_(x1 - x0, 1e-6), al = mx_(y1 - y0, 1e-6);
    double e1 = (vw - 100.0) / an, e2 = (vh - 100.0) / al;
    esc = mn(e1, e2);
    if (esc > 200) esc = 200;
    if (esc < 0.001) esc = 0.001;
    ox_ = vw / 2.0 - (x0 + x1) / 2 * esc;
    oy_ = vh / 2.0 + (y0 + y1) / 2 * esc;
}

static void zoom_en(double fx, double fy_, double factor)
{
    double nx = s2x(fx), ny = s2y(fy_);
    double ne = esc * factor;
    if (ne < 0.001) ne = 0.001;
    if (ne > 100000) ne = 100000;
    esc = ne;
    ox_ = fx - nx * esc;
    oy_ = fy_ + ny * esc;
}

/* ============================================================ elegir objetos con el ratón */

static double dist_seg(double px, double py, double x0, double y0, double x1, double y1, int limite)
{
    double dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy;
    double t = l2 > 0 ? ((px - x0) * dx + (py - y0) * dy) / l2 : 0;
    if (limite == 2) t = t < 0 ? 0 : t > 1 ? 1 : t;
    else if (limite == 1 && t < 0) t = 0;
    double qx = x0 + t * dx - px, qy = y0 + t * dy - py;
    return gm_sqrt(qx * qx + qy * qy);
}

/* Distancia en píxeles del punto (sx, sy) (de la vista) al objeto; 1e9 = no se puede elegir. */
static double dist_obj(int i, double sx, double sy)
{
    gbm_obj_t *o = &gbm_obj[i];
    if (!o->visivel || !o->ok) return 1e9;
    int t = o->tipo;
    if (gbm_eh_ponto(t)) return gm_hypot(sx - w2x(o->v[0]), sy - w2y(o->v[1])) - 3;
    if (gbm_eh_reta(t)) {
        int lim = t == G_SEMIRRETA ? 1 : (t == G_SEGMENTO || t == G_VETOR) ? 2 : 0;
        return dist_seg(sx, sy, w2x(o->v[0]), w2y(o->v[1]), w2x(o->v[2]), w2y(o->v[3]), lim);
    }
    if (gbm_eh_circ(t)) return gm_abs(gm_hypot(sx - w2x(o->v[0]), sy - w2y(o->v[1])) - o->v[2] * esc);
    if (t == G_FUNCAO) {
        double best = 1e9, xa = s2x(sx - 2), ya = fy(i, xa);
        for (int k = 1; k <= 4; k++) {
            double xb = s2x(sx - 2 + k), yb = fy(i, xb);
            if (!gm_nan(ya) && !gm_nan(yb)) { double d = dist_seg(sx, sy, w2x(xa), ya, w2x(xb), yb, 2); if (d < best) best = d; }
            xa = xb; ya = yb;
        }
        return best;
    }
    if (gbm_eh_poligono(t)) {
        double xs[64], ys[64], best = 1e9;
        int n = gbm_vertices(i, xs, ys, 64);
        bool dentro = false;
        for (int k = 0; k < n; k++) {
            int m = (k + 1) % n;
            double d = dist_seg(sx, sy, w2x(xs[k]), w2y(ys[k]), w2x(xs[m]), w2y(ys[m]), 2);
            if (d < best) best = d;
            double y0 = w2y(ys[k]), y1 = w2y(ys[m]);
            if ((y0 <= sy && y1 > sy) || (y1 <= sy && y0 > sy)) {
                double xc = w2x(xs[k]) + (sy - y0) / (y1 - y0) * (w2x(xs[m]) - w2x(xs[k]));
                if (xc > sx) dentro = !dentro;
            }
        }
        if (dentro && best > 4) best = 4.5;
        return best;
    }
    if (t == G_TEXTO) {
        double px = w2x(o->px), py = w2y(o->py) - 8;
        double w = tam_texto(o->texto) * 8.0;
        if (sx >= px && sx <= px + w && sy >= py && sy <= py + 16) return 0;
    }
    return 1e9;
}

/* El objeto más cercano; "tipos" filtra (máscara por tipo; 0 = todos). */
static int objeto_en(double sx, double sy, bool solo_puntos, bool sin_puntos)
{
    int mejor = -1;
    double dm = 6.0;
    for (int i = gbm_n - 1; i >= 0; i--) {
        bool es_pto = gbm_eh_ponto(gbm_obj[i].tipo);
        if (solo_puntos && !es_pto) continue;
        if (sin_puntos && es_pto) continue;
        double d = dist_obj(i, sx, sy);
        if (d < dm) { dm = d; mejor = i; }
    }
    return mejor;
}

/* ============================================================ acciones de construcción */

static char ultimo_descr[160];

static void registrar_nuevos(int desde)
{
    char s[200];
    for (int i = desde; i < gbm_n; i++) { gbm_descrever(i, s, sizeof s); registrar("gebramini: obj %s\n", s); }
}

/* Ejecuta una orden y deja el estado listo (deshacer, mensajes, registro). */
static bool ordenar(const char *t)
{
    int antes = gbm_n, ult = -1;
    if (gbm_orden(t, &ult) != 0) {
        char m[140];
        formatar_texto(m, sizeof m, "Error: %s", gbm_erro);
        dizer(m, true);
        registrar("gebramini: error %s (%s)\n", gbm_erro, t);
        return false;
    }
    gbm_recalcular();
    estado_guardar();
    if (ult >= 0) { gbm_descrever(ult, ultimo_descr, sizeof ultimo_descr); dizer(ultimo_descr, false); }
    else dizer("Hecho", false);
    registrar("gebramini: orden %s\n", t);
    registrar_nuevos(antes);
    if (gbm_n == antes && ult >= 0) { char s[200]; gbm_descrever(ult, s, sizeof s); registrar("gebramini: obj %s\n", s); }
    if (ult >= 0 && gbm_obj[ult].visivel) sel = ult;
    return true;
}

static void numero_texto(double v, char *s, int max) { gbm_formatar(v, 10, s, max); }

/* El punto que corresponde a un clic: uno existente, uno sobre un objeto o uno libre nuevo. Devuelve -1 si falla. */
static int punto_de_clic(int sx, int sy)
{
    int p = objeto_en(sx, sy, true, false);
    if (p >= 0) return p;
    double wx = s2x(sx), wy = s2y(sy);
    int o = objeto_en(sx, sy, false, true);
    char c[120], a[32], b[32];
    int antes = gbm_n;
    if (o >= 0 && gbm_obj[o].tipo != G_TEXTO && gbm_obj[o].tipo != G_SLIDER) {
        int tt = gbm_obj[o].tipo;
        if (gbm_eh_reta(tt) || gbm_eh_circ(tt) || tt == G_FUNCAO || gbm_eh_poligono(tt)) {
            formatar_texto(c, sizeof c, "PuntoEn(%s, 0)", gbm_obj[o].nome);
            if (!ordenar(c)) return -1;
            int n = gbm_n - 1;
            gbm_arrastar(n, wx, wy);
            estado_guardar();
            char s[200];
            gbm_descrever(n, s, sizeof s);
            registrar("gebramini: obj %s\n", s);
            return n;
        }
    }
    numero_texto(wx, a, sizeof a); numero_texto(wy, b, sizeof b);
    formatar_texto(c, sizeof c, "Punto(%s, %s)", a, b);
    if (!ordenar(c)) return -1;
    return gbm_n > antes ? gbm_n - 1 : -1;
}

static void limpiar_pend(void) { npend = 0; }

static void finalizar_orden(const char *c)
{
    ordenar(c);
    limpiar_pend();
}

static const char *nomb(int i) { return gbm_obj[i].nome; }

static void clic_herramienta(int sx, int sy)
{
    char c[300];
    int p;
    switch (ferr) {
    case T_PUNTO: punto_de_clic(sx, sy); break;
    case T_SEGMENTO: case T_RECTA: case T_SEMIRRECTA: case T_VECTOR: case T_CIRC_PP: case T_MEDIATRIZ: case T_PMEDIO: case T_DIST: {
        static const char *const cmd[T_N] = { [T_SEGMENTO] = "Segmento", [T_RECTA] = "Recta", [T_SEMIRRECTA] = "Semirrecta", [T_VECTOR] = "Vector",
            [T_CIRC_PP] = "Circunferencia", [T_MEDIATRIZ] = "Mediatriz", [T_PMEDIO] = "PuntoMedio", [T_DIST] = "Distancia" };
        if (npend == 0 && (ferr == T_MEDIATRIZ || ferr == T_PMEDIO || ferr == T_DIST)) {
            int s = objeto_en(sx, sy, false, true);
            if (s >= 0 && (gbm_obj[s].tipo == G_SEGMENTO || gbm_obj[s].tipo == G_VETOR) && objeto_en(sx, sy, true, false) < 0) {
                formatar_texto(c, sizeof c, "%s(%s)", cmd[ferr], nomb(s));
                finalizar_orden(c);
                return;
            }
        }
        if (npend == 1 && ferr == T_DIST) {   /* ¿punto y recta? */
            int r = objeto_en(sx, sy, false, true);
            if (r >= 0 && gbm_eh_reta(gbm_obj[r].tipo) && objeto_en(sx, sy, true, false) < 0) {
                formatar_texto(c, sizeof c, "Distancia(%s, %s)", nomb(pend[0]), nomb(r));
                finalizar_orden(c);
                return;
            }
        }
        p = punto_de_clic(sx, sy);
        if (p < 0) return;
        if (npend == 1 && pend[0] == p) return;
        pend[npend++] = p;
        if (npend == 2) {
            formatar_texto(c, sizeof c, "%s(%s, %s)", cmd[ferr], nomb(pend[0]), nomb(pend[1]));
            finalizar_orden(c);
        }
        break;
    }
    case T_CIRC_R: {
        p = punto_de_clic(sx, sy);
        if (p < 0) return;
        char r[32] = "1";
        if (!pedir_texto("Circunferencia", "Radio (un número o una expresión):", r, sizeof r)) { limpiar_pend(); return; }
        formatar_texto(c, sizeof c, "Circunferencia(%s, %s)", nomb(p), r);
        finalizar_orden(c);
        break;
    }
    case T_INTERSECA: {
        int o = objeto_en(sx, sy, false, true);
        if (o < 0) { dizer("Elige un objeto (no un punto)", true); return; }
        if (npend == 1 && pend[0] == o) return;
        pend[npend++] = o;
        if (npend == 2) { formatar_texto(c, sizeof c, "Interseca(%s, %s)", nomb(pend[0]), nomb(pend[1])); finalizar_orden(c); }
        break;
    }
    case T_PERP: case T_PARAL: {
        int pt = objeto_en(sx, sy, true, false);
        int o = pt >= 0 ? pt : objeto_en(sx, sy, false, true);
        if (o < 0 && npend == 0 && ferr != T_INTERSECA) { pt = punto_de_clic(sx, sy); o = pt; }
        if (o < 0) return;
        bool es_p = gbm_eh_ponto(gbm_obj[o].tipo);
        if (!es_p && !gbm_eh_reta(gbm_obj[o].tipo)) { dizer("Hace falta un punto y una recta", true); return; }
        if (npend == 1 && gbm_eh_ponto(gbm_obj[pend[0]].tipo) == es_p) { dizer("Hace falta un punto y una recta", true); return; }
        pend[npend++] = o;
        if (npend == 2) {
            int pp = gbm_eh_ponto(gbm_obj[pend[0]].tipo) ? pend[0] : pend[1], rr = pp == pend[0] ? pend[1] : pend[0];
            formatar_texto(c, sizeof c, "%s(%s, %s)", ferr == T_PERP ? "Perpendicular" : "Paralela", nomb(pp), nomb(rr));
            finalizar_orden(c);
        }
        break;
    }
    case T_BISECTRIZ: case T_ANGULO: {
        p = punto_de_clic(sx, sy);
        if (p < 0) return;
        pend[npend++] = p;
        if (npend == 3) {
            formatar_texto(c, sizeof c, "%s(%s, %s, %s)", ferr == T_ANGULO ? "Angulo" : "Bisectriz", nomb(pend[0]), nomb(pend[1]), nomb(pend[2]));
            finalizar_orden(c);
        }
        break;
    }
    case T_POLIGONO: {
        p = punto_de_clic(sx, sy);
        if (p < 0) return;
        if (npend >= 3 && p == pend[0]) {
            int n = formatar_texto(c, sizeof c, "Poligono(");
            for (int k = 0; k < npend; k++) n += formatar_texto(c + n, sizeof c - (size_t)n, "%s%s", k ? ", " : "", nomb(pend[k]));
            copia_texto(c + n, ")", sizeof c - (size_t)n);
            finalizar_orden(c);
            return;
        }
        for (int k = 0; k < npend; k++) if (pend[k] == p) return;
        if (npend < GBM_PAI_MAX) pend[npend++] = p;
        break;
    }
    case T_POLREG: {
        p = punto_de_clic(sx, sy);
        if (p < 0) return;
        if (npend == 1 && pend[0] == p) return;
        pend[npend++] = p;
        if (npend == 2) {
            char n[16] = "5";
            if (!pedir_texto("Polígono regular", "Número de vértices (3 a 64):", n, sizeof n)) { limpiar_pend(); return; }
            formatar_texto(c, sizeof c, "Poligono(%s, %s, %s)", nomb(pend[0]), nomb(pend[1]), n);
            finalizar_orden(c);
        }
        break;
    }
    case T_AREA: {
        int o = objeto_en(sx, sy, false, true);
        if (o < 0 || !(gbm_eh_poligono(gbm_obj[o].tipo) || gbm_eh_circ(gbm_obj[o].tipo))) { dizer("Elige un polígono o una circunferencia", true); return; }
        formatar_texto(c, sizeof c, "Area(%s)", nomb(o));
        finalizar_orden(c);
        break;
    }
    case T_TEXTO: {
        char t[80] = "";
        if (!pedir_texto("Texto", "Escribe el texto:", t, sizeof t) || !t[0]) return;
        for (char *q = t; *q; q++) if (*q == '"') *q = '\'';
        char a[32], b[32];
        numero_texto(s2x(sx), a, sizeof a); numero_texto(s2y(sy), b, sizeof b);
        formatar_texto(c, sizeof c, "Texto(\"%s\", %s, %s)", t, a, b);
        ordenar(c);
        break;
    }
    case T_SLIDER: ordenar("Deslizador(-5, 5, 0.1, 1)"); break;
    case T_BORRAR: {
        int o = objeto_en(sx, sy, true, false);
        if (o < 0) o = objeto_en(sx, sy, false, true);
        if (o < 0) return;
        char n[GBM_NOME];
        copia_texto(n, gbm_obj[o].nome, sizeof n);
        gbm_apagar(o);
        gbm_recalcular();
        estado_guardar();
        sel = -1;
        char m[80];
        formatar_texto(m, sizeof m, "Borrado %s (y lo que dependía de él)", n);
        dizer(m, false);
        registrar("gebramini: borrado %s\n", n);
        break;
    }
    }
}

static void tool_poner(int t)
{
    ferr = t;
    limpiar_pend();
    for (int i = 0; i < n_itens; i++) if (itens[i].id >= T_MOVER && itens[i].id < T_N) itens[i].marcado = itens[i].id == t;
    dizer(ayuda_tool[t], false);
    registrar("gebramini: herramienta %s\n", nomes_tool[t]);
}

/* ============================================================ propiedades */

static const uint32_t PALETA[12] = { 0x222222, 0xCC2222, 0xE07000, 0xC8A000, 0x228822, 0x00A0A0, 0x2255CC, 0x6A2AB0, 0xC02090, 0x8A4B00, 0x808080, 0x000000 };

static bool dialogo_propriedades(int i)
{
    gbm_obj_t o = gbm_obj[i];     /* copia: solo se aplica al aceptar */
    int t = o.tipo;
    bool tiene_texto = t == G_TEXTO, es_slider = t == G_SLIDER;
    int alt = 300 + (tiene_texto ? 34 : 0) + (es_slider ? 34 : 0);
    jan_t *d = jan_abrir("Propiedades", 400, alt);
    if (!d) return false;
    jan_estilo(d, JAN_DIALOGO, 0, 0);
    caixa_t nome, extra[3];
    memset(&nome, 0, sizeof nome); memset(extra, 0, sizeof extra);
    nome.x = 90; nome.y = 12; nome.l = 290; caixa_definir(&nome, o.nome); nome.foco = true;
    int y_extra = 44;
    if (tiene_texto) { extra[0].x = 90; extra[0].y = y_extra; extra[0].l = 290; caixa_definir(&extra[0], o.texto); y_extra += 34; }
    if (es_slider) {
        char b[32];
        for (int k = 0; k < 3; k++) { extra[k].x = 90 + k * 100; extra[k].y = y_extra; extra[k].l = 88; }
        numero_texto(o.min, b, sizeof b); caixa_definir(&extra[0], b);
        numero_texto(o.max, b, sizeof b); caixa_definir(&extra[1], b);
        numero_texto(o.passo, b, sizeof b); caixa_definir(&extra[2], b);
        y_extra += 34;
    }
    int y_cor = y_extra + 4;
    casilla_t c_rot = { 20, y_cor + 108, o.rotulo }, c_vis = { 200, y_cor + 108, o.visivel };
    botao_t b_mas = { 90, y_cor + 66, 26, 24, "+", false }, b_menos = { 124, y_cor + 66, 26, 24, "-", false };
    botao_t ok = { 200, alt - 44, 90, 26, "Aceptar", false }, cancel = { 300, alt - 44, 90, 26, "Cancelar", false };
    static const char *const estilos[3] = { "Continuo", "Discontinuo", "Punteado" };
    bool aceptar = false, redib = true;
    evento_t e;
    for (;;) {
        if (redib) {
            g_retangulo(d, 0, 0, d->larg, d->alt, W98_FACE);
            rotulo(d, 14, 16, "Nombre:", W98_NEGRO);
            caixa_desenhar(d, &nome);
            int yy = 48;
            if (tiene_texto) { rotulo(d, 14, yy, "Texto:", W98_NEGRO); caixa_desenhar(d, &extra[0]); yy += 34; }
            if (es_slider) {
                rotulo(d, 14, yy - 2, "Mín/Máx/Paso:", W98_NEGRO);
                for (int k = 0; k < 3; k++) caixa_desenhar(d, &extra[k]);
                yy += 34;
            }
            rotulo(d, 14, y_cor + 4, "Color:", W98_NEGRO);
            for (int k = 0; k < 12; k++) {
                int x = 90 + (k % 6) * 40, y = y_cor + (k / 6) * 30;
                g_retangulo(d, x, y, 32, 24, PALETA[k]);
                g_moldura(d, x - 1, y - 1, 34, 26, o.cor == PALETA[k] ? 0xFFFFFF : W98_SOMBRA);
                if (o.cor == PALETA[k]) g_moldura(d, x - 2, y - 2, 36, 28, W98_NEGRO);
            }
            rotulo(d, 14, y_cor + 70, "Grosor:", W98_NEGRO);
            boton98(d, &b_mas); boton98(d, &b_menos);
            char g[8];
            formatar_texto(g, sizeof g, "%d", o.grosor);
            rotulo(d, 160, y_cor + 70, g, W98_NEGRO);
            for (int k = 0; k < 3; k++) {
                int x = 200 + (k % 3) * 0, y = y_cor + 62 + k * 0;
                (void)x; (void)y;
            }
            rotulo(d, 200, y_cor + 70, estilos[o.estilo], W98_NEGRO);
            botao_t b_est = { 300, y_cor + 66, 80, 24, "Estilo", false };
            boton98(d, &b_est);
            casilla98(d, &c_rot, "Etiqueta"); casilla98(d, &c_vis, "Visible");
            boton98(d, &ok); boton98(d, &cancel);
            jan_mostrar(d);
            redib = false;
        }
        if (proximo_evento(d->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) break;
        if (boton98_tratar(d, &ok, &e)) { aceptar = true; break; }
        if (boton98_tratar(d, &cancel, &e)) break;
        if (boton98_tratar(d, &b_mas, &e)) { if (o.grosor < 9) o.grosor++; redib = true; continue; }
        if (boton98_tratar(d, &b_menos, &e)) { if (o.grosor > 1) o.grosor--; redib = true; continue; }
        botao_t b_est = { 300, y_cor + 66, 80, 24, "Estilo", false };
        if (boton98_tratar(d, &b_est, &e)) { o.estilo = (uint8_t)((o.estilo + 1) % 3); redib = true; continue; }
        if (casilla98_tratar(d, &c_rot, "Etiqueta", &e)) { o.rotulo = c_rot.marcada; redib = true; continue; }
        if (casilla98_tratar(d, &c_vis, "Visible", &e)) { o.visivel = c_vis.marcada; redib = true; continue; }
        if (e.tipo == EV_CLIQUE && (e.botoes & 1)) {
            for (int k = 0; k < 12; k++) {
                int x = 90 + (k % 6) * 40, y = y_cor + (k / 6) * 30;
                if (e.x >= x && e.x < x + 32 && e.y >= y && e.y < y + 24) { o.cor = PALETA[k]; redib = true; }
            }
        }
        if (e.tipo == EV_TECLA && e.tecla == '\n') { aceptar = true; break; }
        if (e.tipo == EV_TECLA && e.tecla == '\t') {
            caixa_t *lista[4] = { &nome, &extra[0], &extra[1], &extra[2] };
            int cuantas = tiene_texto ? 2 : es_slider ? 4 : 1, k = 0;
            for (int q = 0; q < cuantas; q++) if (lista[q]->foco) k = q;
            lista[k]->foco = false; lista[(k + 1) % cuantas]->foco = true;
            redib = true;
            continue;
        }
        bool antes_e = false;
        (void)antes_e;
        caixa_tratar(d, &nome, &e);
        if (tiene_texto) caixa_tratar(d, &extra[0], &e);
        if (es_slider) for (int k = 0; k < 3; k++) caixa_tratar(d, &extra[k], &e);
        if (e.tipo == EV_CLIQUE) redib = true;
    }
    jan_fechar(d);
    if (!aceptar) return false;
    gbm_obj_t *p = &gbm_obj[i];
    p->cor = o.cor; p->grosor = o.grosor; p->estilo = o.estilo; p->rotulo = o.rotulo; p->visivel = o.visivel;
    if (compara_texto(nome.texto, p->nome) != 0) {
        if (!gbm_renomear(i, nome.texto)) mensagem("GebraMini", "Ese nombre no es válido o ya está en uso.");
    }
    if (tiene_texto) { for (char *q = extra[0].texto; *q; q++) if (*q == '"') *q = '\''; copia_texto(p->texto, extra[0].texto, sizeof p->texto); }
    if (es_slider) {
        const char *fim;
        double mn_ = gbm_texto_num(extra[0].texto[0] == '-' ? extra[0].texto + 1 : extra[0].texto, &fim); if (extra[0].texto[0] == '-') mn_ = -mn_;
        double mx2 = gbm_texto_num(extra[1].texto[0] == '-' ? extra[1].texto + 1 : extra[1].texto, &fim); if (extra[1].texto[0] == '-') mx2 = -mx2;
        double st = gbm_texto_num(extra[2].texto, &fim);
        if (mx2 > mn_) { p->min = mn_; p->max = mx2; p->passo = st; if (p->v[0] < mn_) p->v[0] = mn_; if (p->v[0] > mx2) p->v[0] = mx2; }
    }
    return true;
}

/* ============================================================ acciones */

static void lista_ordenes(void)
{
    mensagem("Órdenes de GebraMini",
             "Punto(x, y)   PuntoEn(objeto, t)   PuntoMedio(A, B)\n"
             "Recta(A, B)   Semirrecta(A, B)   Segmento(A, B)   Vector(A, B)\n"
             "Circunferencia(centro, punto | radio)   Circunferencia(A, B, C)\n"
             "Perpendicular(P, r)   Paralela(P, r)   Mediatriz(A, B)\n"
             "Bisectriz(A, B, C)   Poligono(A, B, C...)   Poligono(A, B, n)\n"
             "Angulo(A, B, C)   Distancia(A, B)   Area(polígono)\n"
             "Interseca(a, b)   Raiz(f)   Extremo(f)   Derivada(f)\n"
             "Texto(\"hola\", x, y)   Deslizador(min, max, paso, valor)\n\n"
             "Y también:  A = (1, 2)   B = A + (3, 0)   f(x) = sin(x) + x^2/4   a = 2");
}

static bool pode_descartar(void)
{
    if (!modificado) return true;
    return confirmar("GebraMini", "La construcción tiene cambios sin guardar.\n¿Quieres descartarlos?");
}

static void nuevo(void)
{
    gbm_limpar();
    caminho[0] = 0;
    sel = -1;
    limpiar_pend();
    vista_restablecer();
    historial_reiniciar();
    registrar("gebramini: nuevo\n");
}

static bool guardar_como(void)
{
    char c[200];
    copia_texto(c, caminho[0] ? caminho : "/home/construccion.gbm", sizeof c);
    if (!dialogo_guardar("Guardar construcción", FILTROS_GBM, c, c, sizeof c) || !c[0]) return false;
    if (!gravar_gbm(c)) { mensagem("GebraMini", "No se ha podido guardar el archivo."); return false; }
    copia_texto(caminho, c, sizeof caminho);
    modificado = false;
    registrar("gebramini: guardado %s\n", caminho);
    return true;
}

static bool guardar(void)
{
    if (!caminho[0]) return guardar_como();
    if (!gravar_gbm(caminho)) { mensagem("GebraMini", "No se ha podido guardar el archivo."); return false; }
    modificado = false;
    registrar("gebramini: guardado %s\n", caminho);
    return true;
}

static void abrir_ruta(const char *c)
{
    if (!abrir_gbm(c)) {
        char m[300];
        formatar_texto(m, sizeof m, "No se ha podido abrir el archivo.\n%s", gbm_erro[0] ? gbm_erro : "");
        mensagem("GebraMini", m);
        registrar("gebramini: error al abrir %s: %s\n", c, gbm_erro);
        return;
    }
    copia_texto(caminho, c, sizeof caminho);
    sel = -1;
    limpiar_pend();
    historial_reiniciar();
    registrar("gebramini: abierto %s (%d objetos)\n", caminho, gbm_n);
    registrar_nuevos(0);
}

static void deshacer(int paso)
{
    int np = hist_pos + paso;
    if (np < 0 || np >= hist_n) return;
    hist_pos = np;
    double v[3];
    gbm_carregar(hist[hist_pos], v);
    gbm_recalcular();
    sel = -1;
    limpiar_pend();
    modificado = true;
    registrar("gebramini: %s (%d objetos)\n", paso < 0 ? "deshacer" : "rehacer", gbm_n);
    registrar_nuevos(0);
}

static void acao(int a)
{
    switch (a) {
    case A_NUEVO: if (pode_descartar()) nuevo(); break;
    case A_ABRIR: {
        if (!pode_descartar()) break;
        char c[200];
        copia_texto(c, caminho[0] ? caminho : "/ejemplos/gebramini", sizeof c);
        if (dialogo_abrir("Abrir construcción", FILTROS_GBM, c, c, sizeof c) && c[0]) abrir_ruta(c);
        break;
    }
    case A_GUARDAR: guardar(); break;
    case A_GUARDAR_COMO: guardar_como(); break;
    case A_PNG: {
        char c[200];
        copia_texto(c, "/home/construccion.png", sizeof c);
        if (!dialogo_guardar("Exportar a PNG", FILTROS_PNG, c, c, sizeof c) || !c[0]) break;
        if (exportar_png(c, vw, vh) != 0) mensagem("GebraMini", "No se ha podido exportar la imagen.");
        else dizer("Imagen PNG exportada", false);
        break;
    }
    case A_PDF: {
        char c[200];
        copia_texto(c, "/home/construccion.pdf", sizeof c);
        if (!dialogo_guardar("Exportar a PDF", FILTROS_PDF, c, c, sizeof c) || !c[0]) break;
        if (exportar_pdf(c) != 0) mensagem("GebraMini", "No se ha podido exportar el PDF.");
        else dizer("PDF exportado", false);
        break;
    }
    case A_SALIR: sair_pedido = true; break;
    case A_DESHACER: deshacer(-1); break;
    case A_REHACER: deshacer(1); break;
    case A_BORRAR:
        if (sel >= 0) {
            char n[GBM_NOME];
            copia_texto(n, gbm_obj[sel].nome, sizeof n);
            gbm_apagar(sel); gbm_recalcular(); estado_guardar(); sel = -1;
            registrar("gebramini: borrado %s\n", n);
        }
        break;
    case A_PROPIEDADES:
        if (sel >= 0 && dialogo_propriedades(sel)) { estado_guardar(); registrar("gebramini: propiedades %s\n", gbm_obj[sel].nome); }
        break;
    case A_VERTODO: vista_ver_todo(); break;
    case A_RESTABLECER: vista_restablecer(); break;
    case A_GRADE: ver_grade = !ver_grade; break;
    case A_EJES: ver_ejes = !ver_ejes; break;
    case A_ALGEBRA: algebra_vis = !algebra_vis; layout(); break;
    case A_DEC0: case A_DEC1: case A_DEC2: case A_DEC3: case A_DEC4: case A_DEC5: gbm_decimais = a - A_DEC0; break;
    case A_EJ1: case A_EJ2: case A_EJ3: case A_EJ4: {
        if (!pode_descartar()) break;
        char c[120];
        formatar_texto(c, sizeof c, "/ejemplos/gebramini/%s", EJEMPLOS[a - A_EJ1]);
        abrir_ruta(c);
        break;
    }
    case A_ORDENES: lista_ordenes(); break;
    case A_ACERCA:
        mensagem("Acerca de GebraMini",
                 "GebraMini - geometría dinámica y funciones\n"
                 "© 2026 Alejandro Anuncibay, GPL-3.0 o posterior.\n"
                 "Nace del prototipo GebraMini del autor (C++/SDL);\n"
                 "esta versión nativa de SaguiOS es una reescritura.\n"
                 "Inspirado en la idea de los programas de geometría\n"
                 "dinámica; sin código de terceros.");
        break;
    case A_ZOOM_MAS: zoom_en(vw / 2.0, vh / 2.0, 1.25); break;
    case A_ZOOM_MENOS: zoom_en(vw / 2.0, vh / 2.0, 0.8); break;
    }
    if (a >= T_MOVER && a < T_N) tool_poner(a);
    layout();
    redesenhar_tudo();
}

/* ============================================================ entrada con autocompletado e historial */

#define HIST_ORD 16
static char ord_hist[HIST_ORD][128];
static int ord_n, ord_pos;
static char comp_pref[32];
static int comp_idx = -1;

static bool prefijo_ci(const char *p, const char *t)
{
    for (; *p && *t; p++, t++) {
        int a = (unsigned char)*p, b = (unsigned char)*t;
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (b >= 0xC0 && b <= 0xC5) b = 'a'; else if (b >= 0xC8 && b <= 0xCB) b = 'e'; else if (b == 0xED) b = 'i';
        if (b >= 0xE0 && b <= 0xE5) b = 'a'; else if (b >= 0xE8 && b <= 0xEB) b = 'e';
        if (a != b) return false;
    }
    return *p == 0;
}

/* Hay algo como «Seg» al final del cursor: se completa con la siguiente orden que empiece así. */
static bool completar(void)
{
    int cur = entrada.cursor, ini = cur;
    while (ini > 0 && ((entrada.texto[ini - 1] >= 'a' && entrada.texto[ini - 1] <= 'z') || (entrada.texto[ini - 1] >= 'A' && entrada.texto[ini - 1] <= 'Z') || (unsigned char)entrada.texto[ini - 1] >= 0xC0)) ini--;
    if (ini == cur && comp_idx < 0) return false;
    char pref[32];
    if (comp_idx < 0) {
        int n = cur - ini;
        if (n <= 0 || n >= (int)sizeof pref) return false;
        memcpy(pref, entrada.texto + ini, (size_t)n); pref[n] = 0;
        copia_texto(comp_pref, pref, sizeof comp_pref);
    } else copia_texto(pref, comp_pref, sizeof pref);
    const char *lista[40];
    int total = gbm_comandos(lista, 40), hallado = -1, vistos = 0;
    for (int k = 0; k < total; k++) {
        if (!prefijo_ci(pref, lista[k])) continue;
        if (vistos == comp_idx + 1 || (comp_idx < 0 && vistos == 0)) { hallado = k; break; }
        vistos++;
    }
    if (hallado < 0) {
        if (comp_idx < 0) return false;
        comp_idx = -1;
        return completar();
    }
    if (comp_idx >= 0) {
        /* quitar lo ya completado: del inicio de la palabra al cursor */
        int fin = cur;
        memmove(entrada.texto + ini, entrada.texto + fin, tam_texto(entrada.texto + fin) + 1);
        entrada.cursor = ini;
        cur = ini;
    } else {
        memmove(entrada.texto + ini, entrada.texto + cur, tam_texto(entrada.texto + cur) + 1);
        entrada.cursor = ini; cur = ini;
    }
    char rest[160];
    formatar_texto(rest, sizeof rest, "%s(", lista[hallado]);
    int n = (int)tam_texto(rest), t = (int)tam_texto(entrada.texto);
    if (t + n >= (int)sizeof entrada.texto - 1) return false;
    memmove(entrada.texto + cur + n, entrada.texto + cur, (size_t)(t - cur + 1));
    memcpy(entrada.texto + cur, rest, (size_t)n);
    entrada.cursor = cur + n;
    comp_idx = vistos;
    return true;
}

static void sugerencias(void)
{
    int cur = entrada.cursor, ini = cur;
    while (ini > 0 && ((entrada.texto[ini - 1] >= 'a' && entrada.texto[ini - 1] <= 'z') || (entrada.texto[ini - 1] >= 'A' && entrada.texto[ini - 1] <= 'Z') || (unsigned char)entrada.texto[ini - 1] >= 0xC0)) ini--;
    if (cur - ini < 2 || cur - ini >= 30) return;
    char pref[32], s[120] = "";
    memcpy(pref, entrada.texto + ini, (size_t)(cur - ini)); pref[cur - ini] = 0;
    const char *lista[40];
    int total = gbm_comandos(lista, 40), n = 0;
    for (int k = 0; k < total; k++)
        if (prefijo_ci(pref, lista[k])) {
            if (n) copia_texto(s + tam_texto(s), ", ", sizeof s - tam_texto(s));
            copia_texto(s + tam_texto(s), lista[k], sizeof s - tam_texto(s));
            n++;
        }
    if (n) { char m[140]; formatar_texto(m, sizeof m, "Órdenes: %s   (Tab completa)", s); dizer(m, false); }
}

static void ejecutar_entrada(void)
{
    char t[sizeof entrada.texto];
    copia_texto(t, entrada.texto, sizeof t);
    int n = (int)tam_texto(t);
    while (n > 0 && t[n - 1] == ' ') t[--n] = 0;
    if (!t[0]) return;
    bool ok = ordenar(t);
    if (ok) {
        if (ord_n < HIST_ORD) copia_texto(ord_hist[ord_n++], t, sizeof ord_hist[0]);
        else { memmove(ord_hist[0], ord_hist[1], sizeof ord_hist[0] * (HIST_ORD - 1)); copia_texto(ord_hist[HIST_ORD - 1], t, sizeof ord_hist[0]); }
        ord_pos = ord_n;
        entrada.texto[0] = 0; entrada.cursor = 0;
    }
}

/* ============================================================ eventos del plano */

static bool slider_clic(int ex, int ey)
{
    for (int i = 0; i < gbm_n; i++) {
        gbm_obj_t *o = &gbm_obj[i];
        int x, y;
        if (o->tipo != G_SLIDER || !slider_rect(i, &x, &y)) continue;
        if (ex >= x - 8 && ex <= x + SL_L + 8 && ey >= y + 12 && ey <= y + 32) {
            arrastra_obj = i;
            modo_arrastre = D_SLIDER;
            sel = i;
            double f = (ex - x) / (double)SL_L;
            f = f < 0 ? 0 : f > 1 ? 1 : f;
            o->v[0] = slider_valor(o, f);
            gbm_recalcular();
            return true;
        }
    }
    return false;
}

static bool pais_libres(int i)    /* ¿se puede trasladar el objeto moviendo sus puntos libres? */
{
    gbm_obj_t *o = &gbm_obj[i];
    if (!(gbm_eh_reta(o->tipo) && (o->tipo == G_RETA || o->tipo == G_SEMIRRETA || o->tipo == G_SEGMENTO || o->tipo == G_VETOR)) && !gbm_eh_circ(o->tipo) && !gbm_eh_poligono(o->tipo)) return false;
    if (o->npai == 0) return false;
    for (int k = 0; k < o->npai; k++) if (gbm_obj[o->pai[k]].tipo != G_PTO_LIVRE) return false;
    return true;
}

static void clic_mover(int ex, int ey, bool doble)
{
    int sx = ex - vx, sy = ey - vy;
    if (slider_clic(ex, ey)) return;
    int o = objeto_en(sx, sy, true, false);
    if (o < 0) o = objeto_en(sx, sy, false, true);
    if (o >= 0 && doble) { sel = o; acao(A_PROPIEDADES); return; }
    sel = o;
    arr_sx = ex; arr_sy = ey; arr_ox = ox_; arr_oy = oy_; arr_movio = false;
    if (o >= 0 && (gbm_livre(o) || pais_libres(o))) {
        arrastra_obj = o;
        arr_x0 = s2x(sx); arr_y0 = s2y(sy);
        modo_arrastre = gbm_livre(o) ? D_OBJ : D_TRASLADA;
    } else if (o < 0 || gbm_obj[o].tipo == G_SLIDER) {
        modo_arrastre = D_PAN;
    } else {
        modo_arrastre = D_PAN;     /* un objeto que no se arrastra: la vista sí */
    }
}

static void mover_arrastre(int ex, int ey)
{
    int sx = ex - vx, sy = ey - vy;
    if (modo_arrastre == D_PAN) {
        ox_ = arr_ox + (ex - arr_sx); oy_ = arr_oy + (ey - arr_sy);
        arr_movio = true;
    } else if (modo_arrastre == D_OBJ) {
        gbm_arrastar(arrastra_obj, s2x(sx), s2y(sy));
        arr_movio = true;
    } else if (modo_arrastre == D_TRASLADA) {
        double dx = s2x(sx) - arr_x0, dy = s2y(sy) - arr_y0;
        gbm_obj_t *o = &gbm_obj[arrastra_obj];
        for (int k = 0; k < o->npai; k++) { gbm_obj[o->pai[k]].v[0] += dx; gbm_obj[o->pai[k]].v[1] += dy; }
        arr_x0 += dx; arr_y0 += dy;
        gbm_recalcular();
        arr_movio = true;
    } else if (modo_arrastre == D_SLIDER) {
        gbm_obj_t *o = &gbm_obj[arrastra_obj];
        int x, y;
        if (slider_rect(arrastra_obj, &x, &y)) {
            double f = (ex - x) / (double)SL_L;
            f = f < 0 ? 0 : f > 1 ? 1 : f;
            o->v[0] = slider_valor(o, f);
            gbm_recalcular();
            arr_movio = true;
        }
    } else if (modo_arrastre == D_DIVISOR) {
        algebra_l = ex < 120 ? 120 : ex > j->larg - 200 ? j->larg - 200 : ex;
        arr_movio = true;
    }
}

static void fin_arrastre(void)
{
    int modo = modo_arrastre;
    modo_arrastre = D_NADA;
    if (modo == D_OBJ || modo == D_TRASLADA || modo == D_SLIDER) {
        if (arr_movio) {
            estado_guardar();
            int ob = arrastra_obj;
            char s[200];
            gbm_descrever(ob, s, sizeof s);
            registrar("gebramini: arrastrado %s\n", s);
            for (int i = 0; i < gbm_n; i++) if (i != ob && gbm_depende(i, ob)) { gbm_descrever(i, s, sizeof s); registrar("gebramini: obj %s\n", s); }
        }
    }
    arrastra_obj = -1;
}

static void clic_algebra(int ex, int ey, bool doble)
{
    int fila = (ey - TOP - 20) / LINHA_A + alg_scroll;
    if (ey < TOP + 20 || fila < 0 || fila >= gbm_n) return;
    if (ex < 22) {
        gbm_obj[fila].visivel = !gbm_obj[fila].visivel;
        estado_guardar();
        registrar("gebramini: %s %s\n", gbm_obj[fila].visivel ? "mostrado" : "oculto", gbm_obj[fila].nome);
        return;
    }
    sel = fila;
    if (doble) acao(A_PROPIEDADES);
}

/* ============================================================ modos sin ventana */

static int modo_sin_ventana(int argc, char **argv)
{
    const char *pdf = NULL, *png = NULL, *gbm = NULL, *abrir_f = NULL;
    char *ordenes[40];
    int n_ord = 0;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--pdf") == 0 && i + 1 < argc) pdf = argv[++i];
        else if (compara_texto(argv[i], "--png") == 0 && i + 1 < argc) png = argv[++i];
        else if (compara_texto(argv[i], "--guardar") == 0 && i + 1 < argc) gbm = argv[++i];
        else if (compara_texto(argv[i], "--abrir") == 0 && i + 1 < argc) abrir_f = argv[++i];
        else if (compara_texto(argv[i], "--orden") == 0) {
            while (i + 1 < argc && argv[i + 1][0] != '-' && n_ord < 40) ordenes[n_ord++] = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] == '-' && argv[i + 1][1] != '-' && n_ord < 40) ordenes[n_ord++] = argv[++i];   /* una orden que empieza por «-» */
        }
    }
    vw = 800; vh = 520;
    vista_restablecer();
    gbm_limpar();
    if (abrir_f) {
        if (!abrir_gbm(abrir_f)) { escrever_texto(SAIDA, "gebramini: error "); escrever_texto(SAIDA, gbm_erro); escrever_texto(SAIDA, "\n"); return 1; }
        vista_ver_todo();
    }
    for (int i = 0; i < n_ord; i++) {
        if (gbm_orden(ordenes[i], NULL) != 0) {
            escrever_texto(SAIDA, "gebramini: error "); escrever_texto(SAIDA, gbm_erro); escrever_texto(SAIDA, "\n");
            return 1;
        }
        gbm_recalcular();
    }
    char s[220];
    for (int i = 0; i < gbm_n; i++) {
        gbm_descrever(i, s, sizeof s);
        escrever_texto(SAIDA, "obj ");
        escrever_texto(SAIDA, s);
        escrever_texto(SAIDA, "\n");
    }
    if (n_ord && !abrir_f) vista_ver_todo();
    int rc = 0;
    if (gbm && !gravar_gbm(gbm)) { escrever_texto(SAIDA, "gebramini: no se pudo guardar\n"); rc = 1; }
    if (png && exportar_png(png, vw, vh) != 0) { escrever_texto(SAIDA, "gebramini: no se pudo exportar el PNG\n"); rc = 1; }
    if (pdf && exportar_pdf(pdf) != 0) { escrever_texto(SAIDA, "gebramini: no se pudo exportar el PDF\n"); rc = 1; }
    formatar_texto(s, sizeof s, "gebramini: %d objetos\n", gbm_n);
    escrever_texto(SAIDA, s);
    return rc;
}

/* ============================================================ principal */

int principal(int argc, char **argv)
{
    if (argc >= 2 && (compara_texto(argv[1], "--orden") == 0 || compara_texto(argv[1], "--abrir") == 0 || compara_texto(argv[1], "--pdf") == 0))
        return modo_sin_ventana(argc, argv);

    j = jan_abrir("Sin título - GebraMini", 960, 640);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK, 640, 420);
    itens_iniciar();
    for (int i = 0; i < 5; i++) { menu.nomes[i] = menu_nomes[i]; menu.itens[i] = menu_textos[i]; }
    menu.n = 5;
    memset(&entrada, 0, sizeof entrada);
    entrada.foco = true;
    calcular_geometria();
    vista_restablecer();
    layout();
    gbm_limpar();
    if (argc >= 2) {
        if (abrir_gbm(argv[1])) { copia_texto(caminho, argv[1], sizeof caminho); registrar("gebramini: abierto %s (%d objetos)\n", caminho, gbm_n); registrar_nuevos(0); }
        else mensagem("GebraMini", "No se ha podido abrir el archivo.");
    }
    historial_reiniciar();
    dizer(ayuda_tool[ferr], false);
    redesenhar_tudo();
    registrar("gebramini: pronto\n");
    for (int i = 0; i < n_itens; i++) if (itens[i].id > 0) registrar("gebramini: boton %d %d\n", itens[i].id, barra_ferr_x(&barra, i));
    registrar("gebramini: vista origen=(%d,%d) escala=%d tam=%dx%d\n", vx + (int)ox_, vy + (int)oy_, (int)esc, vw, vh);

    evento_t e;
    while (!sair_pedido) {
        if (tem_adiado) { e = adiado; tem_adiado = false; }
        else if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) { if (pode_descartar()) break; continue; }
        if (jan_redimensionar(j, &e)) { layout(); redesenhar_tudo(); continue; }
        /* menú de ventana y barra de herramientas */
        if (e.tipo == EV_MENU) {
            int m = barra_menu_tratar(j, &menu, &e);
            if (m >= 0) { int a = menu_acoes[m / 100][m % 100]; if (a) acao(a); }
            continue;
        }
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && e.y < MENU_A) { barra_menu_tratar(j, &menu, &e); continue; }
        {
            int id = barra_ferr_tratar(j, &barra, &e);
            if (id > 0) { acao(id); continue; }
        }
        /* movimiento: arrastrar y coordenadas */
        if (e.tipo == EV_MOVER) {
            if (modo_arrastre != D_NADA && (e.botoes & 3)) {
                while (proximo_evento(j->id, &adiado, false) == 1) {   /* se juntan los movimientos que se acumularon */
                    if (adiado.tipo == EV_MOVER) e = adiado; else { tem_adiado = true; break; }
                }
                mover_arrastre(e.x, e.y);
                if (modo_arrastre == D_DIVISOR) { layout(); redesenhar_tudo(); } else redesenhar_vista_algebra();
                mouse_x = e.x; mouse_y = e.y;
                continue;
            }
            mouse_x = e.x; mouse_y = e.y;
            estado_atualizar();
            jan_mostrar_area(j, 0, j->alt - ESTADO_A, j->larg, ESTADO_A);
            continue;
        }
        if (e.tipo == EV_SOLTOU) {
            if (modo_arrastre != D_NADA) { fin_arrastre(); redesenhar_tudo(); }
            continue;
        }
        if (e.tipo == EV_RODA) {
            int n = (int)e.cliques;
            if (algebra_vis && e.x < algebra_l) {
                alg_scroll += n > 0 ? 1 : -1;
                if (alg_scroll < 0) alg_scroll = 0;
                if (alg_scroll > gbm_n - 1) alg_scroll = gbm_n > 0 ? gbm_n - 1 : 0;
                redesenhar_vista_algebra();
            } else if (e.x >= vx && e.y >= vy && e.y < vy + vh) {
                double f = 1.0;
                for (int k = 0; k < (n < 0 ? -n : n); k++) f *= n < 0 ? 1.15 : 1.0 / 1.15;
                zoom_en(e.x - vx, e.y - vy, f);
                redesenhar_vista();
            }
            continue;
        }
        if (e.tipo == EV_CLIQUE) {
            mouse_x = e.x; mouse_y = e.y;
            bool izq = e.botoes & 1, der = e.botoes & 2;
            bool entra = e.y >= j->alt - ESTADO_A - ENTRADA_A && e.y < j->alt - ESTADO_A;
            if (entra) {
                entrada.foco = true;
                caixa_tratar(j, &entrada, &e);
                continue;
            }
            if (algebra_vis && izq && e.x >= algebra_l && e.x < algebra_l + 4 && e.y >= TOP && e.y < TOP + vh) {
                modo_arrastre = D_DIVISOR;
                continue;
            }
            if (algebra_vis && e.x < algebra_l && e.y >= TOP && e.y < TOP + vh) {
                if (izq) { clic_algebra(e.x, e.y, e.cliques >= 2); redesenhar_tudo(); }
                continue;
            }
            if (e.x >= vx && e.y >= vy && e.y < vy + vh) {
                if (der) {
                    modo_arrastre = D_PAN; arr_sx = e.x; arr_sy = e.y; arr_ox = ox_; arr_oy = oy_; arr_movio = false;
                    continue;
                }
                if (izq) {
                    if (ferr == T_MOVER) { clic_mover(e.x, e.y, e.cliques >= 2); }
                    else {
                        if (!slider_clic(e.x, e.y)) clic_herramienta(e.x - vx, e.y - vy);
                        else modo_arrastre = D_SLIDER;
                    }
                    redesenhar_tudo();
                }
            }
            continue;
        }
        if (e.tipo == EV_TECLA) {
            unsigned t = e.tecla;
            if ((e.modificadores & 2) && !(e.modificadores & 4)) {
                switch (t) {
                case 14: acao(A_NUEVO); continue;
                case 15: acao(A_ABRIR); continue;
                case 19: acao(A_GUARDAR); continue;
                case 16: acao(A_PDF); continue;
                case 26: acao(A_DESHACER); continue;
                case 25: acao(A_REHACER); continue;
                default: break;
                }
            }
            if (t == TECLA_ESC) {
                if (npend) { limpiar_pend(); dizer("Selección cancelada", false); }
                else if (entrada.texto[0]) { entrada.texto[0] = 0; entrada.cursor = 0; }
                else if (ferr != T_MOVER) tool_poner(T_MOVER);
                else sel = -1;
                redesenhar_tudo();
                continue;
            }
            if (t == '\t') { if (completar()) { caixa_desenhar(j, &entrada); jan_mostrar_area(j, 0, entrada.y - 4, j->larg, ENTRADA_A); } continue; }
            if (t == TECLA_CIMA || t == TECLA_BAIXO) {
                if (ord_n) {
                    ord_pos += t == TECLA_CIMA ? -1 : 1;
                    if (ord_pos < 0) ord_pos = 0;
                    if (ord_pos >= ord_n) { ord_pos = ord_n; entrada.texto[0] = 0; entrada.cursor = 0; }
                    else { caixa_definir(&entrada, ord_hist[ord_pos]); entrada.cursor = (int)tam_texto(entrada.texto); }
                    caixa_desenhar(j, &entrada);
                    jan_mostrar_area(j, 0, entrada.y - 4, j->larg, ENTRADA_A);
                }
                continue;
            }
            if (t == TECLA_DELETE && !entrada.texto[0] && sel >= 0) { acao(A_BORRAR); continue; }
            comp_idx = -1;
            if (caixa_tratar(j, &entrada, &e)) {
                ejecutar_entrada();
                redesenhar_tudo();
            } else {
                sugerencias();
                jan_mostrar_area(j, 0, entrada.y - 4, j->larg, ENTRADA_A);
                estado_atualizar();
                jan_mostrar_area(j, 0, j->alt - ESTADO_A, j->larg, ESTADO_A);
            }
        }
    }
    jan_fechar(j);
    registrar("gebramini: fechado\n");
    return 0;
}
