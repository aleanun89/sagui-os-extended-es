// SPDX-License-Identifier: GPL-3.0-or-later
// PlanoMini, © 2026 Alejandro Anuncibay: prototipo original (C++/SDL) del mantenedor; esta versión nativa
// para SaguiOS está escrita desde cero a partir de sus ideas. Inspirado en los CAD de órdenes clásicos,
// pero independiente: nada de código, iconos, marcas ni formatos propietarios de Autodesk (DXF es un formato
// de intercambio documentado públicamente).
/* planomini [archivo.pmini|.dxf] - mini CAD 2D de SaguiOS (D312, mejora 129).
 *
 * Interfaz Win98 al estilo de los CAD clásicos: menús, barras de dibujo y modificación, barra de propiedades
 * (capa, color, tipo de línea), área de dibujo con cursor en cruz, línea de órdenes con historial y barra
 * de estado con REJILLA (F7), ORTO (F8), FORZC (F9), POLAR (F10) y REFENT (F3). El motor (órdenes,
 * geometría, ficheros) está en userland/planomini/ y se prueba en el anfitrión.
 *
 * Modos sin ventana, para las pruebas:
 *   planomini --abrir f.(pmini|dxf) [--orden "o1" "o2"...] [--guardar f.pmini] [--dxf f.dxf]
 *             [--pdf f.pdf] [--escala N] [--papel A3] [--png f.png] [--listar] */
#include "sagui.h"
#include "gui.h"
#include "pdf_escribir.h"
#include "pm.h"
#include "icones.h"

extern const unsigned char fonte_sagui[256][16];

/* ============================================================ destinos de dibujo (trama y PDF) */

typedef struct alvo alvo_t;
struct alvo {
    bool pdf;
    uint32_t *px;                          /* trama */
    int larg, alt, ox, oy, cx0, cy0, cx1, cy1;
    pdfw_t *w;                             /* PDF */
    double ccx0, ccy0, ccx1, ccy1;         /* recorte del PDF (twips) */
    double s, tx, ty;                      /* del plano al destino: X = tx + x*s; Y = ty - y*s */
    double pxmm;                           /* unidades del destino por milímetro de papel (para los trazos) */
    double grosor;                         /* ancho de línea en unidades del destino */
    bool fondo_claro;
    double tol;                            /* tolerancia al aplanar arcos (unidades del plano) */
};

static inline double mn(double a, double b) { return a < b ? a : b; }
static inline double mx_(double a, double b) { return a > b ? a : b; }

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

/* Línea con grosor w y bordes suavizados (cobertura por distancia al segmento). */
static void r_linea(alvo_t *r, double x0, double y0, double x1, double y1, double w, uint32_t cor)
{
    x0 += r->ox; y0 += r->oy; x1 += r->ox; y1 += r->oy;
    if (!gm_fin(x0) || !gm_fin(y0) || !gm_fin(x1) || !gm_fin(y1)) return;
    double dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy, half = w * 0.5;
    if (mx_(x0, x1) + half + 1 < r->cx0 || mn(x0, x1) - half - 1 >= r->cx1 || mx_(y0, y1) + half + 1 < r->cy0 || mn(y0, y1) - half - 1 >= r->cy1) return;
    double len = gm_sqrt(l2);
    if (l2 < 1e-9) {
        int ix = (int)gm_floor(x0), iy = (int)gm_floor(y0), rr = (int)half + 1;
        for (int yy = iy - rr; yy <= iy + rr; yy++)
            for (int xx = ix - rr; xx <= ix + rr; xx++) {
                double c = half + 0.5 - gm_hypot(xx + 0.5 - x0, yy + 0.5 - y0);
                mezcla(r, xx, yy, cor, c <= 0 ? 0 : c >= 1 ? 255 : (int)(c * 255));
            }
        return;
    }
    bool horiz = gm_abs(dx) >= gm_abs(dy);
    int a0 = (int)gm_floor(horiz ? mn(x0, x1) - half - 1 : mn(y0, y1) - half - 1), a1 = (int)gm_ceil(horiz ? mx_(x0, x1) + half + 1 : mx_(y0, y1) + half + 1);
    int lo = horiz ? r->cx0 : r->cy0, hi = horiz ? r->cx1 : r->cy1;
    if (a0 < lo) a0 = lo;
    if (a1 > hi) a1 = hi;
    double h = (half + 1) * len / (horiz ? gm_abs(dx) : gm_abs(dy));
    for (int a = a0; a < a1; a++) {
        double ac = a + 0.5, t0 = horiz ? (ac - x0) / dx : (ac - y0) / dy, bl = horiz ? y0 + t0 * dy : x0 + t0 * dx;
        int b0 = (int)gm_floor(bl - h), b1 = (int)gm_ceil(bl + h);
        int blo = horiz ? r->cy0 : r->cx0, bhi = horiz ? r->cy1 : r->cx1;
        if (b0 < blo) b0 = blo;
        if (b1 > bhi) b1 = bhi;
        for (int b = b0; b < b1; b++) {
            double xc = horiz ? ac : b + 0.5, yc = horiz ? b + 0.5 : ac;
            double t = ((xc - x0) * dx + (yc - y0) * dy) / l2;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            double ex = xc - (x0 + t * dx), ey = yc - (y0 + t * dy), c = half + 0.5 - gm_sqrt(ex * ex + ey * ey);
            if (c > 0) { if (horiz) mezcla(r, a, b, cor, c >= 1 ? 255 : (int)(c * 255)); else mezcla(r, b, a, cor, c >= 1 ? 255 : (int)(c * 255)); }
        }
    }
}

static void r_poli(alvo_t *r, const double *xs, const double *ys, int n, uint32_t cor, int alfa)
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
            if ((ys[i] <= yc && ys[j] > yc) || (ys[j] <= yc && ys[i] > yc)) xi[k++] = xs[i] + (yc - ys[i]) / (ys[j] - ys[i]) * (xs[j] - xs[i]);
        }
        for (int a = 1; a < k; a++) { double v = xi[a]; int b = a - 1; while (b >= 0 && xi[b] > v) { xi[b + 1] = xi[b]; b--; } xi[b + 1] = v; }
        for (int a = 0; a + 1 < k; a += 2)
            for (int x = (int)gm_floor(xi[a] + r->ox + 0.5); x < (int)gm_floor(xi[a + 1] + r->ox + 0.5); x++) mezcla(r, x, y, cor, alfa);
    }
}

/* Texto con la fuente de mapa de bits, escalada y girada (correspondencia inversa de cada píxel). */
static void r_texto(alvo_t *r, double x, double y, double alt, double giro, const char *t, int cen, uint32_t cor)
{
    int n = (int)tam_texto(t);
    if (!n) return;
    if (alt < 7) alt = 7;
    double sw = 0.67 * alt / 8.0, sh = alt * 1.3333 / 16.0, adv = 8 * sw, ancho = adv * n;
    double c = gm_cos(giro), s = gm_sin(giro);
    x += r->ox; y += r->oy;
    if (cen) { x -= ancho / 2 * c; y += ancho / 2 * s; }
    /* caja en pantalla */
    double ex[4], ey[4], us[4] = { 0, ancho, ancho, 0 }, vs[4] = { -4 * sh, -4 * sh, 12 * sh, 12 * sh };
    double bx0 = 1e30, by0 = 1e30, bx1 = -1e30, by1 = -1e30;
    for (int i = 0; i < 4; i++) {
        ex[i] = x + us[i] * c - vs[i] * s; ey[i] = y - us[i] * s - vs[i] * c;
        bx0 = mn(bx0, ex[i]); bx1 = mx_(bx1, ex[i]); by0 = mn(by0, ey[i]); by1 = mx_(by1, ey[i]);
    }
    int ya = (int)gm_floor(by0), yb = (int)gm_ceil(by1), xa = (int)gm_floor(bx0), xb = (int)gm_ceil(bx1);
    if (ya < r->cy0) ya = r->cy0;
    if (yb > r->cy1) yb = r->cy1;
    if (xa < r->cx0) xa = r->cx0;
    if (xb > r->cx1) xb = r->cx1;
    for (int py = ya; py < yb; py++)
        for (int px = xa; px < xb; px++) {
            double dx = px + 0.5 - x, dy = py + 0.5 - y;
            double u = dx * c - dy * s, v = -dx * s - dy * c;
            if (u < 0 || u >= ancho) continue;
            int ci = (int)(u / adv);
            if (ci >= n) continue;
            int col = (int)((u - ci * adv) / sw), row = 11 - (int)gm_floor(v / sh);
            if (col < 0 || col > 7 || row < 0 || row > 15) continue;
            if (fonte_sagui[(uint8_t)t[ci]][row] & (0x80 >> col)) mezcla(r, px, py, cor, 255);
        }
}

/* ---- PDF: líneas recortadas, rellenos por franjas y texto ---- */

static bool recorta(double *x0, double *y0, double *x1, double *y1, double xa, double ya, double xb, double yb)
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

static void p_linea(alvo_t *a, double x0, double y0, double x1, double y1, double w, uint32_t cor)
{
    if (!gm_fin(x0) || !gm_fin(y0) || !gm_fin(x1) || !gm_fin(y1)) return;
    if (!recorta(&x0, &y0, &x1, &y1, a->ccx0, a->ccy0, a->ccx1, a->ccy1)) return;
    int e = (int)(w + 0.5);
    pdfw_linha(a->w, (int)(x0 + 0.5), (int)(y0 + 0.5), (int)(x1 + 0.5), (int)(y1 + 0.5), e < 4 ? 4 : e, cor);
}
static void p_poli(alvo_t *a, const double *xs, const double *ys, int n, uint32_t cor)
{
    if (n < 3) return;
    double ymin = 1e30, ymax = -1e30;
    for (int i = 0; i < n; i++) { if (ys[i] < ymin) ymin = ys[i]; if (ys[i] > ymax) ymax = ys[i]; }
    double paso = 6;
    if ((ymax - ymin) / paso > 1500) paso = (ymax - ymin) / 1500;
    for (double y = ymin + paso / 2; y < ymax; y += paso) {
        double xi[130];
        int k = 0;
        for (int i = 0; i < n && k < 128; i++) {
            int j = (i + 1) % n;
            if ((ys[i] <= y && ys[j] > y) || (ys[j] <= y && ys[i] > y)) xi[k++] = xs[i] + (y - ys[i]) / (ys[j] - ys[i]) * (xs[j] - xs[i]);
        }
        for (int s = 1; s < k; s++) { double v = xi[s]; int b = s - 1; while (b >= 0 && xi[b] > v) { xi[b + 1] = xi[b]; b--; } xi[b + 1] = v; }
        for (int s = 0; s + 1 < k; s += 2) pdfw_retangulo(a->w, (int)xi[s], (int)(y - paso / 2), (int)(xi[s + 1] - xi[s]) + 1, (int)paso + 2, cor);
    }
}
static void p_texto(alvo_t *a, double x, double y, double alt, double giro, const char *t, int cen, uint32_t cor)
{
    int n = (int)tam_texto(t);
    if (!n) return;
    int tam = (int)(alt / 0.72);
    if (tam < 40) tam = 40;
    double c = gm_cos(giro), s = gm_sin(giro), ancho = pdfw_largura_texto(PDFW_SANS, 0, tam, t, n);
    if (cen) { x -= ancho / 2 * c; y += ancho / 2 * s; }
    if (gm_abs(giro) < 1e-6) { pdfw_texto(a->w, (int)x, (int)y, PDFW_SANS, 0, tam, cor, t, n, 0); return; }
    double pos = 0;                         /* girado: carácter a carácter sobre la línea base inclinada */
    for (int i = 0; i < n; i++) {
        char ch[2] = { t[i], 0 };
        pdfw_texto(a->w, (int)(x + pos * c), (int)(y - pos * s), PDFW_SANS, 0, tam, cor, ch, 1, 0);
        pos += pdfw_largura_texto(PDFW_SANS, 0, tam, ch, 1);
    }
}

/* ---- primitivas comunes ---- */

static void a_linea(alvo_t *a, double x0, double y0, double x1, double y1, double w, uint32_t cor)
{
    if (a->pdf) p_linea(a, x0, y0, x1, y1, w, cor); else r_linea(a, x0, y0, x1, y1, w, cor);
}

/* Patrones de trazo en milímetros de papel: pares (trazo, hueco). */
static const double PAT[LT_N][4] = { { 0, 0, 0, 0 }, { 3.5, 1.8, 0, 0 }, { 0.3, 1.4, 0, 0 }, { 5.0, 1.5, 0.3, 1.5 } };

static void trazo(alvo_t *a, double x0, double y0, double x1, double y1, uint32_t cor, int lt, double *fase)
{
    double w = a->grosor;
    if (lt <= 0 || lt >= LT_N) { a_linea(a, x0, y0, x1, y1, w, cor); return; }
    double on1 = PAT[lt][0] * a->pxmm, off1 = PAT[lt][1] * a->pxmm, on2 = PAT[lt][2] * a->pxmm, off2 = PAT[lt][3] * a->pxmm;
    double per = on1 + off1 + on2 + off2, dx = x1 - x0, dy = y1 - y0, l = gm_hypot(dx, dy);
    if (l < 1e-9 || per < 1e-6) return;
    double pos = 0, f = *fase;
    int guardia = 0;
    while (pos < l && guardia++ < 20000) {
        double q = gm_fmod(f + pos, per), seg_fin, ini1 = 0, fin1 = on1, ini2 = on1 + off1, fin2 = on1 + off1 + on2;
        bool dentro = false;
        if (q < fin1) { dentro = true; seg_fin = fin1; }
        else if (on2 > 0 && q >= ini2 && q < fin2) { dentro = true; seg_fin = fin2; }
        else if (q < ini2) seg_fin = ini2;
        else if (on2 > 0 && q < fin2 + off2) seg_fin = per;
        else seg_fin = per;
        (void)ini1;
        double largo = seg_fin - q;
        if (largo < 1e-6) largo = 1e-6;
        double fin = mn(l, pos + largo);
        if (dentro) a_linea(a, x0 + dx * pos / l, y0 + dy * pos / l, x0 + dx * fin / l, y0 + dy * fin / l, w, cor);
        pos = fin;
    }
    *fase = gm_fmod(f + l, per);
}

typedef struct { alvo_t *a; uint32_t cor; int lt; double fase; } cx_t;

static void k_seg(void *c, double x0, double y0, double x1, double y1)
{
    cx_t *k = c;
    alvo_t *a = k->a;
    double X0 = a->tx + x0 * a->s, Y0 = a->ty - y0 * a->s, X1 = a->tx + x1 * a->s, Y1 = a->ty - y1 * a->s;
    if (x0 == x1 && y0 == y1) {                 /* un punto: una cruz pequeña */
        double d = 2.5 * (a->pdf ? a->pxmm * 0.4 : 1.0);
        a_linea(a, X0 - d, Y0, X0 + d, Y0, a->grosor, k->cor);
        a_linea(a, X0, Y0 - d, X0, Y0 + d, a->grosor, k->cor);
        return;
    }
    if (!a->pdf) {                               /* recorte previo para no trocear líneas enormes */
        double ox = X0, oy = Y0, cx0 = a->cx0 - a->ox - 3, cy0 = a->cy0 - a->oy - 3, cx1 = a->cx1 - a->ox + 3, cy1 = a->cy1 - a->oy + 3;
        if (!recorta(&X0, &Y0, &X1, &Y1, cx0, cy0, cx1, cy1)) return;
        k->fase += gm_hypot(X0 - ox, Y0 - oy);
    }
    trazo(a, X0, Y0, X1, Y1, k->cor, k->lt, &k->fase);
}
static void k_tri(void *c, double x0, double y0, double x1, double y1, double x2, double y2)
{
    cx_t *k = c;
    double xs[3] = { k->a->tx + x0 * k->a->s, k->a->tx + x1 * k->a->s, k->a->tx + x2 * k->a->s };
    double ys[3] = { k->a->ty - y0 * k->a->s, k->a->ty - y1 * k->a->s, k->a->ty - y2 * k->a->s };
    if (k->a->pdf) p_poli(k->a, xs, ys, 3, k->cor); else r_poli(k->a, xs, ys, 3, k->cor, 255);
}
static void k_poli(void *c, const double *px, const double *py, int n)
{
    cx_t *k = c;
    double xs[800], ys[800];
    if (n > 800) n = 800;
    for (int i = 0; i < n; i++) { xs[i] = k->a->tx + px[i] * k->a->s; ys[i] = k->a->ty - py[i] * k->a->s; }
    if (k->a->pdf) p_poli(k->a, xs, ys, n, k->cor); else r_poli(k->a, xs, ys, n, k->cor, 255);
}
static void k_txt(void *c, double x, double y, double alt, double giro, const char *t, int cen)
{
    cx_t *k = c;
    double X = k->a->tx + x * k->a->s, Y = k->a->ty - y * k->a->s;
    if (k->a->pdf) p_texto(k->a, X, Y, alt * k->a->s, giro, t, cen, k->cor); else r_texto(k->a, X, Y, alt * k->a->s, giro, t, cen, k->cor);
}

static uint32_t color_vista(const alvo_t *a, uint32_t c)
{
    if (a->fondo_claro && c == 0xFFFFFF) return 0x000000;
    if (!a->fondo_claro && c == 0x000000) return 0xFFFFFF;
    return c;
}

static void dibujar_entidad(alvo_t *a, const pm_ent_t *e, uint32_t cor, int lt, double tol)
{
    cx_t k = { a, cor, lt, 0 };
    pm_sink_t s = { k_seg, k_tri, k_poli, k_txt, &k, tol };
    pm_dibujar_ent(e, &s);
}

/* ============================================================ estado de la ventana */

#define MENU_A 20
#define FERR_A 28
#define TOP (MENU_A + 2 * FERR_A)
#define CMD_A 78
#define ESTADO_A 22
#define COL_FONDO_OSC 0x14181C
#define COL_FONDO_CLARO 0xF8F8F4

static jan_t *j;
static char caminho[200];
static bool sair_pedido, fondo_claro;
static int vx, vy;
static uint32_t *cache;
static int cache_cap;
static bool cache_sucia = true;
static barra_menu_t menu;
static barra_ferr_t barra1, barra2;
static barra_item_t itens1[40], itens2[40];
static int n1, n2, gap_capa, gap_color, gap_lt;
static combo_t c_capa, c_color, c_lt;
static const char *nom_capas[PM_CAPAS_MAX];
static const char *const nom_colores[8] = { "PorCapa", "Rojo", "Amarillo", "Verde", "Cian", "Azul", "Magenta", "Blanco" };
static const char *const nom_ltipos[5] = { "PorCapa", "Continua", "Trazos", "Puntos", "Trazo y punto" };
static caixa_t entrada;
static char ord_hist[40][128];
static int ord_n, ord_pos;
static int mouse_x = -1, mouse_y = -1;
static double cur_wx, cur_wy;             /* cursor en el plano, ya ajustado */
static pm_ref_t cur_ref;
static char ult_titulo[200];
static int tab_idx = -1;
static char tab_base[40];

enum { D_NADA, D_PAN, D_VENTANA, D_GRIP };
static int arrastre;
static int arr_sx, arr_sy;                     /* origen del arrastre, en píxeles de la ventana */
static double arr_vcx, arr_vcy;
static bool arr_movio, arr_shift;
static int grip_ent = -1, grip_idx = -1;
static pm_ent_t grip_orig;
static evento_t adiado;
static bool tem_adiado;

static const char *const TIPO_NOMBRE[] = { "?", "PUNTO", "LINEA", "CIRCULO", "ARCO", "POLILINEA", "TEXTO", "COTA", "SOMBREADO" };
static const char *const REF_NOMBRE[] = { "", "Punto final", "Punto medio", "", "Centro", "", "", "", "Intersección", "", "", "", "", "", "", "", "Perpendicular", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "Tangente" };

/* ============================================================ escena */

static void alvo_pantalla(alvo_t *a, uint32_t *px, int larg, int alt, int ox, int oy, int cx0, int cy0, int cx1, int cy1)
{
    memset(a, 0, sizeof *a);
    a->px = px; a->larg = larg; a->alt = alt; a->ox = ox; a->oy = oy; a->cx0 = cx0; a->cy0 = cy0; a->cx1 = cx1; a->cy1 = cy1;
    a->s = pm_ve; a->tx = pm_vw / 2.0 - pm_vcx * pm_ve; a->ty = pm_vh / 2.0 + pm_vcy * pm_ve;
    a->pxmm = 3.78; a->grosor = 1.0; a->fondo_claro = fondo_claro; a->tol = 0.6 / pm_ve;
}

static void rejilla_y_ejes(alvo_t *a)
{
    uint32_t pt = fondo_claro ? 0xC8C8C4 : 0x3A424A, pt2 = fondo_claro ? 0xA0A09C : 0x5A646E;
    if (pm_rejilla) {
        double paso = pm_paso_rej > 0 ? pm_paso_rej : 1;
        while (paso * pm_ve < 9) paso *= (paso * 5 * pm_ve >= 9 && paso * 2 * pm_ve < 9) ? 5 : 10;
        double x0 = pm_s2wx(0), x1 = pm_s2wx(pm_vw), y1 = pm_s2wy(0), y0 = pm_s2wy(pm_vh);
        int cuenta = 0;
        for (double x = gm_ceil(x0 / paso) * paso; x <= x1 && cuenta < 400; x += paso, cuenta++) {
            int sx = (int)(pm_w2sx(x) + 0.5);
            bool mayor = gm_abs(gm_fmod(gm_round(x / paso), 5.0)) < 0.5;
            for (double y = gm_ceil(y0 / paso) * paso; y <= y1; y += paso) {
                int sy = (int)(pm_w2sy(y) + 0.5);
                mezcla(a, sx + a->ox, sy + a->oy, (mayor && gm_abs(gm_fmod(gm_round(y / paso), 5.0)) < 0.5) ? pt2 : pt, 255);
            }
        }
    }
    /* ejes en el origen */
    double ox = pm_w2sx(0), oy = pm_w2sy(0);
    if (ox >= 0 && ox < pm_vw && oy >= 0 && oy < pm_vh) {
        r_linea(a, ox, oy, ox + 44, oy, 1.3, 0xD04040);
        r_linea(a, ox, oy, ox, oy - 44, 1.3, 0x40B040);
        r_texto(a, ox + 48, oy + 4, 9, 0, "X", 0, 0xD04040);
        r_texto(a, ox + 5, oy - 56, 9, 0, "Y", 0, 0x40B040);
    }
}

static void escena(alvo_t *a, bool pantalla)
{
    for (int i = 0; i < pm_n; i++) {
        const pm_ent_t *e = &pm_ent[i];
        if (!pm_ent_visible(e)) continue;
        uint32_t cor = color_vista(a, pm_ent_color(e));
        int lt = pm_ent_ltipo(e);
        if (pantalla && e->sel) { cor = fondo_claro ? 0x1060E0 : 0x60B0FF; lt = LT_TRAZOS; a->grosor = 1.5; }
        else a->grosor = pantalla ? 1.1 : 0.25 * a->pxmm;
        dibujar_entidad(a, e, cor, lt, a->tol);
    }
}

static void regenerar_cache(void)
{
    int n = pm_vw * pm_vh;
    if (n > cache_cap) {
        uint32_t *nc = cache ? realoca(cache, (size_t)n * 4) : aloca((size_t)n * 4);
        if (!nc) return;
        cache = nc; cache_cap = n;
    }
    uint32_t bg = fondo_claro ? COL_FONDO_CLARO : COL_FONDO_OSC;
    for (int i = 0; i < n; i++) cache[i] = bg;
    alvo_t a;
    alvo_pantalla(&a, cache, pm_vw, pm_vh, 0, 0, 0, 0, pm_vw, pm_vh);
    rejilla_y_ejes(&a);
    escena(&a, true);
}

/* ---- ajustes del cursor ---- */

static void cursor_actualizar(int sx, int sy)
{
    double wx = pm_s2wx(sx), wy = pm_s2wy(sy);
    double ap = 9.0 / pm_ve;
    pm_ref_t r;
    pm_ref_siempre = arrastre == D_GRIP;
    pm_ref_cursor(wx, wy, ap, &cur_wx, &cur_wy, &r);
    pm_ref_siempre = false;
    cur_ref = r;
}

static void marcador(alvo_t *a, int tipo, double x, double y)
{
    uint32_t c = 0xE8D000;
    double X = a->tx + x * a->s, Y = a->ty - y * a->s;
    switch (tipo) {
    case REF_FIN: r_linea(a, X - 5, Y - 5, X + 5, Y - 5, 1.6, c); r_linea(a, X + 5, Y - 5, X + 5, Y + 5, 1.6, c); r_linea(a, X + 5, Y + 5, X - 5, Y + 5, 1.6, c); r_linea(a, X - 5, Y + 5, X - 5, Y - 5, 1.6, c); break;
    case REF_MEDIO: r_linea(a, X - 5, Y + 4, X + 5, Y + 4, 1.6, c); r_linea(a, X - 5, Y + 4, X, Y - 5, 1.6, c); r_linea(a, X + 5, Y + 4, X, Y - 5, 1.6, c); break;
    case REF_CENTRO: for (int k = 0; k < 16; k++) { double a0 = k * PM_PI / 8, a1 = (k + 1) * PM_PI / 8; r_linea(a, X + 5 * gm_cos(a0), Y + 5 * gm_sin(a0), X + 5 * gm_cos(a1), Y + 5 * gm_sin(a1), 1.6, c); } break;
    case REF_INTER: r_linea(a, X - 5, Y - 5, X + 5, Y + 5, 1.8, c); r_linea(a, X - 5, Y + 5, X + 5, Y - 5, 1.8, c); break;
    case REF_PERP: r_linea(a, X - 5, Y + 5, X + 5, Y + 5, 1.6, c); r_linea(a, X - 5, Y + 5, X - 5, Y - 5, 1.6, c); r_linea(a, X - 5, Y, X, Y, 1.4, c); r_linea(a, X, Y, X, Y + 5, 1.4, c); break;
    case REF_TANG: for (int k = 0; k < 16; k++) { double a0 = k * PM_PI / 8, a1 = (k + 1) * PM_PI / 8; r_linea(a, X + 4 * gm_cos(a0), Y - 1 + 4 * gm_sin(a0), X + 4 * gm_cos(a1), Y - 1 + 4 * gm_sin(a1), 1.4, c); } r_linea(a, X - 6, Y - 6, X + 6, Y - 6, 1.6, c); break;
    case REF_CUAD: r_linea(a, X, Y - 6, X + 6, Y, 1.6, c); r_linea(a, X + 6, Y, X, Y + 6, 1.6, c); r_linea(a, X, Y + 6, X - 6, Y, 1.6, c); r_linea(a, X - 6, Y, X, Y - 6, 1.6, c); break;
    default: r_linea(a, X - 5, Y - 5, X + 5, Y + 5, 1.6, c); r_linea(a, X - 5, Y + 5, X + 5, Y - 5, 1.6, c); r_linea(a, X - 5, Y - 5, X + 5, Y - 5, 1.6, c); r_linea(a, X - 5, Y + 5, X + 5, Y + 5, 1.6, c); break;
    }
    const char *n = tipo < 40 ? REF_NOMBRE[tipo] : "";
    if (!n[0] && tipo == REF_CERCA) n = "Cercano";
    if (n[0]) r_texto(a, X + 9, Y - 12, 9, 0, n, 0, c);
}

static int grips_de_seleccion(double (*out)[2], int *ent, int *idx, int max)
{
    int n = 0;
    for (int i = 0; i < pm_n && n < max; i++) {
        if (!pm_ent[i].sel) continue;
        double g[PM_VERT_MAX + 8][2];
        int k = pm_grips(&pm_ent[i], g, PM_VERT_MAX + 8);
        for (int q = 0; q < k && n < max; q++) { out[n][0] = g[q][0]; out[n][1] = g[q][1]; ent[n] = i; idx[n] = q; n++; }
    }
    return n;
}

static void overlay(void)
{
    alvo_t a;
    alvo_pantalla(&a, j->px, j->larg, j->alt, vx, vy, vx, vy, vx + pm_vw, vy + pm_vh);
    uint32_t cruz = fondo_claro ? 0x606060 : 0xB8B8B8;
    /* pinzamientos */
    if (!pm_activa()) {
        static double g[260][2];
        static int ge[260], gi[260];
        int n = grips_de_seleccion(g, ge, gi, 260);
        for (int i = 0; i < n; i++) {
            double X = a.tx + g[i][0] * a.s, Y = a.ty - g[i][1] * a.s;
            bool caliente = arrastre == D_GRIP && ge[i] == grip_ent && gi[i] == grip_idx;
            double xs[4] = { X - 3.5, X + 3.5, X + 3.5, X - 3.5 }, ys[4] = { Y - 3.5, Y - 3.5, Y + 3.5, Y + 3.5 };
            r_poli(&a, xs, ys, 4, caliente ? 0xE03030 : 0x2A6FD8, 255);
            r_linea(&a, X - 3.5, Y - 3.5, X + 3.5, Y - 3.5, 1, 0xFFFFFF); r_linea(&a, X + 3.5, Y - 3.5, X + 3.5, Y + 3.5, 1, 0xFFFFFF);
            r_linea(&a, X + 3.5, Y + 3.5, X - 3.5, Y + 3.5, 1, 0xFFFFFF); r_linea(&a, X - 3.5, Y + 3.5, X - 3.5, Y - 3.5, 1, 0xFFFFFF);
        }
    }
    /* vista previa elástica */
    if (pm_activa() && mouse_x >= vx && mouse_y >= vy) {
        pm_ent_t gh[24];
        int ng = pm_fantasma(cur_wx, cur_wy, gh, 24);
        for (int i = 0; i < ng; i++) {
            a.grosor = 1.1;
            dibujar_entidad(&a, &gh[i], fondo_claro ? 0x2070D0 : 0x70B0FF, LT_TRAZOS, 0.6 / pm_ve);
            pm_ent_liberar(&gh[i]);
        }
        double bx, by;
        if (ng == 0 && pm_base_ghost(&bx, &by)) {
            double fase = 0;
            a.grosor = 1.0;
            trazo(&a, a.tx + bx * a.s, a.ty - by * a.s, a.tx + cur_wx * a.s, a.ty - cur_wy * a.s, fondo_claro ? 0x7080A0 : 0x8090B0, LT_TRAZOS, &fase);
        }
    }
    /* ventana de selección */
    if (arrastre == D_VENTANA) {
        int x0 = arr_sx - vx, y0 = arr_sy - vy, x1 = mouse_x - vx, y1 = mouse_y - vy;
        bool cruce = x1 < x0;
        uint32_t cor = cruce ? 0x40C060 : 0x4090F0;
        double xs[4] = { mn(x0, x1), mx_(x0, x1), mx_(x0, x1), mn(x0, x1) }, ys[4] = { mn(y0, y1), mn(y0, y1), mx_(y0, y1), mx_(y0, y1) };
        r_poli(&a, xs, ys, 4, cor, 40);
        double fase = 0;
        a.grosor = 1.0;
        for (int i = 0; i < 4; i++) trazo(&a, xs[i], ys[i], xs[(i + 1) % 4], ys[(i + 1) % 4], cor, cruce ? LT_TRAZOS : LT_CONT, &fase);
    }
    /* cursor en cruz */
    if (mouse_x >= vx && mouse_x < vx + pm_vw && mouse_y >= vy && mouse_y < vy + pm_vh && arrastre != D_PAN) {
        double X = a.tx + cur_wx * a.s, Y = a.ty - cur_wy * a.s;
        bool punto = pm_activa() && (pm_esp == ESP_PUNTO || pm_esp == ESP_NUM);
        if (punto || !pm_activa()) {
            r_linea(&a, 0, Y, pm_vw, Y, 1.0, cruz);
            r_linea(&a, X, 0, X, pm_vh, 1.0, cruz);
        }
        if (!punto) {                    /* casilla de selección */
            double xs[4] = { X - 4, X + 4, X + 4, X - 4 }, ys[4] = { Y - 4, Y - 4, Y + 4, Y + 4 };
            for (int i = 0; i < 4; i++) r_linea(&a, xs[i], ys[i], xs[(i + 1) % 4], ys[(i + 1) % 4], 1.0, cruz);
        }
        if (cur_ref.tipo) marcador(&a, cur_ref.tipo, cur_ref.x, cur_ref.y);
    }
}

/* ============================================================ iconos 16x16 */

#define R(x_, y_, w_, h_, c_) g_retangulo(jj, x + (x_), y + (y_), (w_), (h_), (c_))
#define L(x0, y0, x1, y1, c_) g_linha(jj, x + (x0), y + (y0), x + (x1), y + (y1), (c_))
#define NEG 0x000000
#define AZUL 0x2255CC
#define ROJO 0xCC2222
#define GRIS 0x808080
#define VERDE 0x228822

static void circ_i(jan_t *jj, int x, int y, int cx, int cy, int r, uint32_t c)
{
    for (int k = 0; k < 32; k++) {
        int px = cx + (int)(r * gm_cos(k * PM_PI / 16) + (gm_cos(k * PM_PI / 16) >= 0 ? 0.5 : -0.5));
        int py = cy + (int)(r * gm_sin(k * PM_PI / 16) + (gm_sin(k * PM_PI / 16) >= 0 ? 0.5 : -0.5));
        g_retangulo(jj, x + px, y + py, 1, 1, c);
    }
}
static void arco_i(jan_t *jj, int x, int y, int cx, int cy, int r, int g0, int g1, uint32_t c)
{
    for (int g = g0; g <= g1; g += 6) {
        double a = g * PM_PI / 180;
        g_retangulo(jj, x + cx + (int)(r * gm_cos(a) + 0.5 * (gm_cos(a) >= 0 ? 1 : -1)), y + cy - (int)(r * gm_sin(a) + 0.5 * (gm_sin(a) >= 0 ? 1 : -1)), 1, 1, c);
    }
}
static void punta(jan_t *jj, int x, int y, int px, int py, int dx, int dy, uint32_t c)   /* flechita con la punta en (px, py) hacia (dx, dy) */
{
    g_linha(jj, x + px, y + py, x + px + dx * 3 - dy * 2, y + py + dy * 3 + dx * 2, c);
    g_linha(jj, x + px, y + py, x + px + dx * 3 + dy * 2, y + py + dy * 3 - dx * 2, c);
}

static void ic_linea(jan_t *jj, int x, int y, bool a) { (void)a; L(2, 13, 13, 2, NEG); R(1, 12, 3, 3, AZUL); R(12, 1, 3, 3, AZUL); }
static void ic_plinea(jan_t *jj, int x, int y, bool a) { (void)a; L(1, 12, 5, 4, NEG); L(5, 4, 10, 11, NEG); L(10, 11, 14, 3, NEG); R(0, 11, 3, 3, AZUL); R(13, 2, 3, 3, AZUL); }
static void ic_rect(jan_t *jj, int x, int y, bool a) { (void)a; R(2, 3, 12, 1, NEG); R(2, 12, 12, 1, NEG); R(2, 3, 1, 10, NEG); R(13, 3, 1, 10, NEG); }
static void ic_poligono(jan_t *jj, int x, int y, bool a) { (void)a; L(4, 2, 11, 2, NEG); L(11, 2, 14, 8, NEG); L(14, 8, 11, 14, NEG); L(11, 14, 4, 14, NEG); L(4, 14, 1, 8, NEG); L(1, 8, 4, 2, NEG); }
static void ic_circulo(jan_t *jj, int x, int y, bool a) { (void)a; circ_i(jj, x, y, 8, 8, 6, NEG); R(7, 7, 2, 2, AZUL); }
static void ic_arco(jan_t *jj, int x, int y, bool a) { (void)a; arco_i(jj, x, y, 3, 13, 11, 0, 90, NEG); R(1, 12, 3, 3, AZUL); R(13, 2, 3, 3, AZUL); }
static void ic_punto(jan_t *jj, int x, int y, bool a) { (void)a; R(7, 3, 2, 10, NEG); R(3, 7, 10, 2, NEG); }
static void ic_texto(jan_t *jj, int x, int y, bool a) { (void)a; L(3, 13, 8, 2, NEG); L(8, 2, 13, 13, NEG); R(5, 9, 7, 1, NEG); }
static void ic_sombra(jan_t *jj, int x, int y, bool a) { (void)a; R(2, 2, 12, 1, NEG); R(2, 13, 12, 1, NEG); R(2, 2, 1, 12, NEG); R(13, 2, 1, 12, NEG); for (int k = 4; k <= 12; k += 3) L(3, k + 1, k + 1, 3, GRIS); L(8, 13, 13, 8, GRIS); }
static void ic_cota(jan_t *jj, int x, int y, bool a) { (void)a; R(2, 2, 1, 12, GRIS); R(13, 2, 1, 12, GRIS); L(2, 9, 13, 9, NEG); punta(jj, x, y, 2, 9, 1, 0, NEG); punta(jj, x, y, 13, 9, -1, 0, NEG); R(5, 4, 5, 2, NEG); }
static void ic_cotaali(jan_t *jj, int x, int y, bool a) { (void)a; L(2, 13, 2, 8, GRIS); L(12, 6, 12, 1, GRIS); L(1, 9, 12, 2, NEG); punta(jj, x, y, 1, 9, 1, -1, NEG); punta(jj, x, y, 12, 2, -1, 1, NEG); }
static void ic_cotarad(jan_t *jj, int x, int y, bool a) { (void)a; circ_i(jj, x, y, 7, 9, 5, GRIS); L(7, 9, 13, 3, NEG); punta(jj, x, y, 12, 4, -1, 1, NEG); R(2, 1, 1, 1, NEG); R(2, 1, 4, 1, NEG); R(2, 1, 1, 4, NEG); }
static void ic_cotadiam(jan_t *jj, int x, int y, bool a) { (void)a; circ_i(jj, x, y, 8, 8, 6, GRIS); L(3, 12, 13, 4, NEG); punta(jj, x, y, 3, 12, 1, -1, NEG); punta(jj, x, y, 13, 4, -1, 1, NEG); }
static void ic_cotaang(jan_t *jj, int x, int y, bool a) { (void)a; L(2, 14, 14, 14, GRIS); L(2, 14, 12, 3, GRIS); arco_i(jj, x, y, 2, 14, 8, 0, 48, NEG); R(13, 6, 1, 1, NEG); }
static void ic_borrar(jan_t *jj, int x, int y, bool a) { (void)a; for (int k = 0; k < 3; k++) { L(3 + k, 3, 12 + k - 2, 12, ROJO); L(12 - k, 3, 3 + 2 - k, 12, ROJO); } }
static void ic_mover(jan_t *jj, int x, int y, bool a) { (void)a; L(8, 1, 8, 14, NEG); L(1, 8, 14, 8, NEG); punta(jj, x, y, 8, 1, 0, 1, NEG); punta(jj, x, y, 8, 14, 0, -1, NEG); punta(jj, x, y, 1, 8, 1, 0, NEG); punta(jj, x, y, 14, 8, -1, 0, NEG); }
static void ic_copiar(jan_t *jj, int x, int y, bool a) { (void)a; R(2, 2, 8, 1, NEG); R(2, 9, 8, 1, NEG); R(2, 2, 1, 8, NEG); R(9, 2, 1, 8, NEG); R(6, 6, 8, 1, AZUL); R(6, 13, 8, 1, AZUL); R(6, 6, 1, 8, AZUL); R(13, 6, 1, 8, AZUL); }
static void ic_girar(jan_t *jj, int x, int y, bool a) { (void)a; arco_i(jj, x, y, 8, 8, 5, 30, 330, NEG); punta(jj, x, y, 13, 9, 0, 1, ROJO); R(7, 7, 2, 2, AZUL); }
static void ic_escala(jan_t *jj, int x, int y, bool a) { (void)a; R(2, 8, 6, 1, NEG); R(2, 13, 6, 1, NEG); R(2, 8, 1, 6, NEG); R(7, 8, 1, 6, NEG); R(4, 2, 9, 1, GRIS); R(12, 2, 1, 7, GRIS); L(8, 8, 13, 3, ROJO); }
static void ic_simetria(jan_t *jj, int x, int y, bool a) { (void)a; for (int k = 0; k < 14; k += 3) R(8, 1 + k, 1, 2, GRIS); L(2, 4, 6, 4, NEG); L(2, 4, 2, 12, NEG); L(2, 12, 6, 12, NEG); L(14, 4, 10, 4, AZUL); L(14, 4, 14, 12, AZUL); L(14, 12, 10, 12, AZUL); }
static void ic_equidist(jan_t *jj, int x, int y, bool a) { (void)a; arco_i(jj, x, y, 1, 14, 11, 0, 90, NEG); arco_i(jj, x, y, 1, 14, 7, 0, 90, AZUL); }
static void ic_recorta(jan_t *jj, int x, int y, bool a) { (void)a; L(1, 8, 14, 8, GRIS); L(8, 1, 8, 6, NEG); L(8, 10, 8, 14, NEG); L(3, 2, 6, 5, ROJO); L(6, 2, 3, 5, ROJO); }
static void ic_alarga(jan_t *jj, int x, int y, bool a) { (void)a; R(13, 2, 1, 12, GRIS); L(1, 8, 9, 8, NEG); L(9, 8, 12, 8, AZUL); punta(jj, x, y, 12, 8, 1, 0, AZUL); }
static void ic_empalme(jan_t *jj, int x, int y, bool a) { (void)a; L(2, 14, 2, 7, NEG); L(9, 2, 14, 2, NEG); arco_i(jj, x, y, 9, 7, 7, 90, 180, ROJO); }
static void ic_chaflan(jan_t *jj, int x, int y, bool a) { (void)a; L(2, 14, 2, 6, NEG); L(10, 2, 14, 2, NEG); L(2, 6, 10, 2, ROJO); }
static void ic_explota(jan_t *jj, int x, int y, bool a) { (void)a; R(1, 1, 6, 1, NEG); R(9, 1, 6, 1, NEG); R(1, 14, 6, 1, NEG); R(9, 14, 6, 1, NEG); R(0, 3, 1, 4, NEG); R(15, 3, 1, 4, NEG); R(0, 9, 1, 4, NEG); R(15, 9, 1, 4, NEG); R(7, 7, 2, 2, ROJO); }
static void ic_matriz(jan_t *jj, int x, int y, bool a) { (void)a; for (int i = 0; i < 2; i++) for (int k = 0; k < 2; k++) { R(2 + k * 7, 2 + i * 7, 5, 5, AZUL); R(3 + k * 7, 3 + i * 7, 3, 3, 0xC0D0F0); } }
static void ic_igualaprop(jan_t *jj, int x, int y, bool a) { (void)a; R(2, 2, 5, 5, ROJO); R(9, 9, 5, 5, NEG); L(7, 4, 11, 4, GRIS); L(11, 4, 11, 8, GRIS); punta(jj, x, y, 11, 8, 0, 1, GRIS); }
static void ic_zext(jan_t *jj, int x, int y, bool a) { (void)a; R(2, 2, 12, 1, NEG); R(2, 13, 12, 1, NEG); R(2, 2, 1, 12, NEG); R(13, 2, 1, 12, NEG); L(5, 5, 11, 11, AZUL); L(11, 5, 5, 11, AZUL); }
static void ic_zmas(jan_t *jj, int x, int y, bool a) { (void)a; circ_i(jj, x, y, 6, 6, 4, NEG); L(9, 9, 14, 14, NEG); R(4, 6, 5, 1, ROJO); R(6, 4, 1, 5, ROJO); }
static void ic_zmenos(jan_t *jj, int x, int y, bool a) { (void)a; circ_i(jj, x, y, 6, 6, 4, NEG); L(9, 9, 14, 14, NEG); R(4, 6, 5, 1, ROJO); }
static void ic_zvent(jan_t *jj, int x, int y, bool a) { (void)a; R(1, 1, 9, 1, GRIS); R(1, 8, 9, 1, GRIS); R(1, 1, 1, 8, GRIS); R(9, 1, 1, 8, GRIS); circ_i(jj, x, y, 10, 10, 3, NEG); L(12, 12, 15, 15, NEG); }
static void ic_capas(jan_t *jj, int x, int y, bool a) { (void)a; R(2, 9, 12, 3, GRIS); R(2, 5, 12, 3, AZUL); R(2, 1, 12, 3, ROJO); }
static void ic_props(jan_t *jj, int x, int y, bool a) { (void)a; R(2, 2, 12, 12, NEG); R(3, 3, 10, 10, W98_LUZ); R(5, 5, 6, 1, NEG); R(5, 8, 6, 1, NEG); R(5, 11, 4, 1, NEG); }

typedef void (*fn_icono)(jan_t *, int, int, bool);

/* ============================================================ acciones y menús */

enum {
    A_NUEVO = 1, A_ABRIR, A_GUARDAR, A_GUARDAR_COMO, A_ABRIR_DXF, A_GUARDAR_DXF, A_PDF, A_PNG, A_SALIR,
    A_DESHACER, A_REHACER, A_SELTODO, A_BORRAR_SEL, A_PROPS, A_ZOOM_EXT, A_ZOOM_MAS, A_ZOOM_MENOS, A_ZOOM_VENT, A_ZOOM_PREV,
    A_REGEN, A_FONDO_OSC, A_FONDO_CLARO, A_CAPAS, A_EJ1, A_EJ2, A_EJ3, A_AYUDA, A_ORDENES, A_ACERCA, A_ESTILO_COTA, A_REJ, A_ORTO, A_FORZC, A_POLAR, A_REF,
    A_CMD = 100
};
static const char *const CMDS[] = {
    "LINEA", "PLINEA", "RECTANG", "POLIGONO", "CIRCULO", "ARCO", "PUNTO", "TEXTO", "SOMBREADO",
    "COTALIN", "COTAALI", "COTARAD", "COTADIAM", "COTAANG",
    "BORRAR", "MOVER", "COPIAR", "GIRAR", "ESCALA", "SIMETRIA", "EQUIDIST", "RECORTA", "ALARGA", "EMPALME", "CHAFLAN", "EXPLOTA", "MATRIZ", "IGUALAPROP",
};
static const fn_icono ICONOS_CMD[] = {
    ic_linea, ic_plinea, ic_rect, ic_poligono, ic_circulo, ic_arco, ic_punto, ic_texto, ic_sombra,
    ic_cota, ic_cotaali, ic_cotarad, ic_cotadiam, ic_cotaang,
    ic_borrar, ic_mover, ic_copiar, ic_girar, ic_escala, ic_simetria, ic_equidist, ic_recorta, ic_alarga, ic_empalme, ic_chaflan, ic_explota, ic_matriz, ic_igualaprop,
};
#define N_CMDS ((int)(sizeof CMDS / sizeof CMDS[0]))

static void item(barra_item_t *v, int *n, int id, fn_icono ic, const char *txt, int larg)
{
    barra_item_t it = { id, ic, txt, false, false, false, false, larg };
    v[(*n)++] = it;
}
static void itens_iniciar(void)
{
    n1 = n2 = 0;
    item(itens1, &n1, A_NUEVO, ic98_novo, NULL, 0); item(itens1, &n1, A_ABRIR, ic98_abrir, NULL, 0); item(itens1, &n1, A_GUARDAR, ic98_guardar, NULL, 0); item(itens1, &n1, A_PDF, ic98_pdf, NULL, 0);
    item(itens1, &n1, 0, NULL, NULL, 0);
    item(itens1, &n1, A_DESHACER, ic98_desfazer, NULL, 0); item(itens1, &n1, A_REHACER, NULL, "Re", 0);
    item(itens1, &n1, 0, NULL, NULL, 0);
    item(itens1, &n1, A_ZOOM_EXT, ic_zext, NULL, 0); item(itens1, &n1, A_ZOOM_VENT, ic_zvent, NULL, 0); item(itens1, &n1, A_ZOOM_MAS, ic_zmas, NULL, 0); item(itens1, &n1, A_ZOOM_MENOS, ic_zmenos, NULL, 0);
    item(itens1, &n1, 0, NULL, NULL, 0);
    item(itens1, &n1, A_CAPAS, ic_capas, NULL, 0); item(itens1, &n1, A_PROPS, ic_props, NULL, 0);
    item(itens1, &n1, 0, NULL, NULL, 0);
    gap_capa = n1; item(itens1, &n1, -1, NULL, NULL, 186);
    gap_color = n1; item(itens1, &n1, -2, NULL, NULL, 106);
    gap_lt = n1; item(itens1, &n1, -3, NULL, NULL, 130);
    for (int i = 0; i < N_CMDS; i++) {
        item(itens2, &n2, A_CMD + i, ICONOS_CMD[i], NULL, 0);
        if (i == 8 || i == 13) item(itens2, &n2, 0, NULL, NULL, 0);
    }
}

static const char *const menu_nomes[8] = { "Archivo", "Edición", "Ver", "Dibujo", "Cotas", "Modificar", "Ejemplos", "Ayuda" };
static const char *const menu_textos[8] = {
    "Nuevo\tCtrl+N\nAbrir...\tCtrl+O\n-\nGuardar\tCtrl+S\nGuardar como...\n-\nAbrir DXF...\nGuardar como DXF...\n-\nExportar a PDF...\tCtrl+P\nExportar a PNG...\n-\nSalir",
    "Deshacer\tCtrl+Z\nRehacer\tCtrl+Y\n-\nSeleccionar todo\tCtrl+A\nBorrar selección\tSupr\nPropiedades...",
    "Zoom extensión\nZoom ventana\nZoom previo\nAcercar\nAlejar\nRegenerar\n-\nFondo oscuro\nFondo claro\n-\nRejilla\tF7\nOrto\tF8\nForzcursor\tF9\nRastreo polar\tF10\nReferencia a objetos\tF3",
    "Línea\nPolilínea\nRectángulo\nPolígono\nCírculo\nArco\nPunto\nTexto\nSombreado",
    "Cota lineal\nCota alineada\nCota de radio\nCota de diámetro\nCota angular\n-\nEstilo de cotas...",
    "Borrar\nMover\nCopiar\nGirar\nEscala\nSimetría\nEquidistancia\nRecortar\nAlargar\nEmpalme\nChaflán\nExplotar\nMatriz\nIgualar propiedades\n-\nAdministrador de capas...",
    "Planta de una habitación con cotas\nPieza mecánica con empalmes\nPlano en DXF",
    "Órdenes disponibles\nAcerca de PlanoMini",
};
static const int menu_acoes[8][20] = {
    { A_NUEVO, A_ABRIR, 0, A_GUARDAR, A_GUARDAR_COMO, 0, A_ABRIR_DXF, A_GUARDAR_DXF, 0, A_PDF, A_PNG, 0, A_SALIR },
    { A_DESHACER, A_REHACER, 0, A_SELTODO, A_BORRAR_SEL, A_PROPS },
    { A_ZOOM_EXT, A_ZOOM_VENT, A_ZOOM_PREV, A_ZOOM_MAS, A_ZOOM_MENOS, A_REGEN, 0, A_FONDO_OSC, A_FONDO_CLARO, 0, A_REJ, A_ORTO, A_FORZC, A_POLAR, A_REF },
    { A_CMD + 0, A_CMD + 1, A_CMD + 2, A_CMD + 3, A_CMD + 4, A_CMD + 5, A_CMD + 6, A_CMD + 7, A_CMD + 8 },
    { A_CMD + 9, A_CMD + 10, A_CMD + 11, A_CMD + 12, A_CMD + 13, 0, A_ESTILO_COTA },
    { A_CMD + 14, A_CMD + 15, A_CMD + 16, A_CMD + 17, A_CMD + 18, A_CMD + 19, A_CMD + 20, A_CMD + 21, A_CMD + 22, A_CMD + 23, A_CMD + 24, A_CMD + 25, A_CMD + 26, A_CMD + 27, 0, A_CAPAS },
    { A_EJ1, A_EJ2, A_EJ3 },
    { A_ORDENES, A_ACERCA },
};
static const char *const EJEMPLOS[3] = { "habitacion.pmini", "pieza.pmini", "plano.dxf" };

#define FILTROS_PM "Dibujo PlanoMini (*.pmini)|*.pmini|Todos los archivos (*.*)|*"
#define FILTROS_DXF "Intercambio DXF (*.dxf)|*.dxf|Todos los archivos (*.*)|*"
#define FILTROS_ABRIR "Dibujos (*.pmini;*.dxf)|*.pmini;*.dxf|PlanoMini (*.pmini)|*.pmini|DXF (*.dxf)|*.dxf|Todos los archivos (*.*)|*"
#define FILTROS_PNG "Imagen PNG (*.png)|*.png"
#define FILTROS_PDF "Documento PDF (*.pdf)|*.pdf"

/* ============================================================ ficheros y exportación */

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
static bool tiene_ext(const char *c, const char *ext)     /* sin distinguir mayúsculas */
{
    int l = (int)tam_texto(c), e = (int)tam_texto(ext);
    if (l < e) return false;
    for (int i = 0; i < e; i++) { char a = c[l - e + i], b = ext[i]; if (a >= 'A' && a <= 'Z') a = (char)(a + 32); if (a != b) return false; }
    return true;
}

static void vista_inicial(void) { pm_vcx = 150; pm_vcy = 85; pm_ve = 2.5; }
static void sincronizar_combos(void);
static void titulo_ventana(void);

static bool abrir_dibujo(const char *c)
{
    int n;
    char *b = ler_arquivo(c, &n);
    if (!b) { copia_texto(pm_erro, "No se puede leer el archivo", sizeof pm_erro); return false; }
    int rc;
    bool dxf = tiene_ext(c, ".dxf");
    int omit = 0;
    if (dxf) rc = pm_dxf_leer(b, &omit);
    else {
        rc = pm_cargar(b);
        if (rc == 0) {          /* sin línea VISTA (v1 del prototipo): se ajusta a la extensión */
            bool v = false;
            for (const char *q = b; *q; q++) if ((q == b || q[-1] == '\n') && q[0] == 'V' && q[1] == 'I' && q[2] == 'S') v = true;
            if (!v) pm_zoom_ext();
        }
    }
    libera(b);
    if (rc != 0) return false;
    pm_historial_reiniciar();
    if (dxf) { caminho[0] = 0; if (omit) { char m[120]; formatar_texto(m, sizeof m, "DXF abierto; %d entidades no soportadas se han omitido", omit); copia_texto(pm_msg, m, sizeof pm_msg); } }
    else copia_texto(caminho, c, sizeof caminho);
    registrar("planomini: abierto %s (%d entidades%s)\n", c, pm_n, dxf ? ", DXF" : "");
    return true;
}
static bool gravar_pmini(const char *c)
{
    int n;
    char *t = pm_serializar(&n);
    if (!t) return false;
    bool ok = escrever_arquivo(c, t, n);
    libera(t);
    return ok;
}
static bool gravar_dxf(const char *c)
{
    int n;
    char *t = pm_dxf_escribir(&n);
    if (!t) return false;
    bool ok = escrever_arquivo(c, t, n);
    libera(t);
    if (ok) registrar("planomini: dxf exportado %s (%d bytes)\n", c, n);
    return ok;
}

static bool extension_dibujo(double *x0, double *y0, double *x1, double *y1)
{
    double a = 1e300, b = 1e300, c = -1e300, d = -1e300;
    for (int i = 0; i < pm_n; i++) {
        double p, q, r, s;
        if (!pm_ent_visible(&pm_ent[i]) || !pm_ent_bbox(&pm_ent[i], &p, &q, &r, &s)) continue;
        a = mn(a, p); b = mn(b, q); c = mx_(c, r); d = mx_(d, s);
    }
    if (a > c) return false;
    *x0 = a; *y0 = b; *x1 = c; *y1 = d;
    return true;
}

static int exportar_png(const char *ruta)
{
    double x0, y0, x1, y1;
    if (!extension_dibujo(&x0, &y0, &x1, &y1)) return -1;
    double bw = mx_(x1 - x0, 1e-6), bh = mx_(y1 - y0, 1e-6);
    int W = 1000, H = (int)(1000 * bh / bw) + 60;
    if (H > 700) { H = 700; W = (int)(700 * bw / bh) + 60; if (W < 200) W = 200; }
    if (H < 200) H = 200;
    imagem_t img;
    if (!imagem_criar(&img, W, H, 0xFFFFFF)) return -1;
    alvo_t a;
    memset(&a, 0, sizeof a);
    a.px = img.px; a.larg = W; a.alt = H; a.cx1 = W; a.cy1 = H;
    a.s = mn((W - 40) / bw, (H - 40) / bh);
    a.tx = W / 2.0 - (x0 + x1) / 2 * a.s; a.ty = H / 2.0 + (y0 + y1) / 2 * a.s;
    a.pxmm = 3.78; a.fondo_claro = true; a.tol = 0.5 / a.s;
    escena(&a, false);
    uint8_t *d;
    int n;
    int r = imagem_codificar_png(&img, &d, &n);
    imagem_liberar(&img);
    if (r != 0) return -1;
    bool ok = escrever_arquivo(ruta, d, n);
    libera(d);
    if (ok) registrar("planomini: png exportado %s (%dx%d, %d bytes)\n", ruta, W, H, n);
    return ok ? 0 : -1;
}

#define MM 56.6929
static int exportar_pdf(const char *ruta, int papel, int escala, const char *titulo_)
{
    double x0, y0, x1, y1;
    if (!extension_dibujo(&x0, &y0, &x1, &y1)) return -1;
    double bw = x1 - x0, bh = y1 - y0;
    int pw = papel == 1 ? 23811 : 16838, ph = papel == 1 ? 16838 : 11906;
    double m = 10 * MM, caj = 28 * MM, ax0 = m + 3 * MM, ay0 = m + 3 * MM, ax1 = pw - m - 3 * MM, ay1 = ph - m - caj - 3 * MM;
    double aw_mm = (ax1 - ax0) / MM, ah_mm = (ay1 - ay0) / MM;
    if (escala <= 0) {
        static const int NS[] = { 1, 2, 5, 10, 20, 25, 50, 75, 100, 125, 150, 200, 250, 500, 1000, 2000, 5000, 10000 };
        escala = 10000;
        for (unsigned i = 0; i < sizeof NS / sizeof NS[0]; i++) if (bw / NS[i] <= aw_mm * 0.98 && bh / NS[i] <= ah_mm * 0.98) { escala = NS[i]; break; }
    }
    pdfw_t *w = pdfw_criar(papel == 1 ? PDFW_A4 : PDFW_A4);
    if (!w) return -1;
    pdfw_tamanho(w, pw, ph);
    pdfw_titulo(w, titulo_ && titulo_[0] ? titulo_ : "Plano");
    pdfw_pagina(w);
    int fino = (int)(0.35 * MM);
    /* marco y cajetín */
    pdfw_linha(w, (int)m, (int)m, pw - (int)m, (int)m, fino, 0);
    pdfw_linha(w, pw - (int)m, (int)m, pw - (int)m, ph - (int)m, fino, 0);
    pdfw_linha(w, pw - (int)m, ph - (int)m, (int)m, ph - (int)m, fino, 0);
    pdfw_linha(w, (int)m, ph - (int)m, (int)m, (int)m, fino, 0);
    int cy = ph - (int)m - (int)caj, cdiv = pw - (int)m - (int)(70 * MM);
    pdfw_linha(w, (int)m, cy, pw - (int)m, cy, fino, 0);
    pdfw_linha(w, cdiv, cy, cdiv, ph - (int)m, fino, 0);
    pdfw_linha(w, cdiv, cy + (int)(caj / 2), pw - (int)m, cy + (int)(caj / 2), fino / 2, 0);
    char t[160];
    const char *tit = titulo_ && titulo_[0] ? titulo_ : (caminho[0] ? (acha_ultimo_char(caminho, '/') ? acha_ultimo_char(caminho, '/') + 1 : caminho) : "Plano sin título");
    pdfw_texto(w, (int)m + 300, cy + (int)(12 * MM), PDFW_SANS, PDFW_NEGRITA, (int)(6 * MM), 0, tit, (int)tam_texto(tit), 0);
    pdfw_texto(w, (int)m + 300, cy + (int)(22 * MM), PDFW_SANS, 0, (int)(3.2 * MM), 0x404040, "Dibujo 2D - unidades: milímetros", 32, 0);
    formatar_texto(t, sizeof t, "Escala 1:%d", escala);
    pdfw_texto(w, cdiv + 300, cy + (int)(11 * MM), PDFW_SANS, PDFW_NEGRITA, (int)(4.2 * MM), 0, t, (int)tam_texto(t), 0);
    formatar_texto(t, sizeof t, "Hoja 1 de 1   %s", papel == 1 ? "A3" : "A4");
    pdfw_texto(w, cdiv + 300, cy + (int)(21 * MM), PDFW_SANS, 0, (int)(3.4 * MM), 0, t, (int)tam_texto(t), 0);
    pdfw_texto(w, cdiv + 300, cy + (int)(27 * MM), PDFW_SANS, PDFW_CURSIVA, (int)(2.6 * MM), 0x606060, "PlanoMini - SaguiOS", 19, 0);
    /* el dibujo */
    alvo_t a;
    memset(&a, 0, sizeof a);
    a.pdf = true; a.w = w;
    a.s = MM / escala;
    a.tx = (ax0 + ax1) / 2 - (x0 + x1) / 2 * a.s; a.ty = (ay0 + ay1) / 2 + (y0 + y1) / 2 * a.s;
    a.pxmm = MM; a.fondo_claro = true; a.tol = 0.04 * escala;
    a.ccx0 = ax0; a.ccy0 = ay0; a.ccx1 = ax1; a.ccy1 = ay1;
    escena(&a, false);
    uint8_t *d;
    int n;
    bool ok = pdfw_finalizar(w, &d, &n);
    pdfw_liberar(w);
    if (!ok) return -1;
    ok = escrever_arquivo(ruta, d, n);
    libera(d);
    if (ok) registrar("planomini: pdf exportado %s (%d bytes, escala 1:%d, %s)\n", ruta, n, escala, papel == 1 ? "A3" : "A4");
    return ok ? 0 : -1;
}

/* Argumentos de EXPORTAR pdf: «archivo [1:N | N] [A3|A4] [título]». */
static int exportar_pdf_args(const char *arg)
{
    char ruta[200], tok[80];
    const char *p = arg;
    int papel = 0, escala = 0, n = 0;
    while (*p == ' ') p++;
    while (*p && *p != ' ' && n < 199) ruta[n++] = *p++;
    ruta[n] = 0;
    if (!ruta[0]) return -2;
    char titulo_[100];
    titulo_[0] = 0;
    for (;;) {
        while (*p == ' ') p++;
        if (!*p) break;
        int k = 0;
        while (*p && *p != ' ' && k < 79) tok[k++] = *p++;
        tok[k] = 0;
        const char *q = tok;
        if (tok[0] == '1' && tok[1] == ':') q = tok + 2;
        if ((tok[0] == 'A' || tok[0] == 'a') && tok[1] == '3') papel = 1;
        else if ((tok[0] == 'A' || tok[0] == 'a') && tok[1] == '4') papel = 0;
        else if (*q >= '0' && *q <= '9') escala = texto_para_int(q);
        else { copia_texto(titulo_, tok, sizeof titulo_); for (; *p == ' '; p++) ; if (*p) { int l = (int)tam_texto(titulo_); titulo_[l] = ' '; copia_texto(titulo_ + l + 1, p, sizeof titulo_ - (size_t)l - 1); } break; }
    }
    return exportar_pdf(ruta, papel, escala, titulo_);
}

static void describir(int i, char *s, int max)
{
    const pm_ent_t *e = &pm_ent[i];
    char f[6][24];
    pm_fmt(e->p[0], 3, f[0], 24); pm_fmt(e->p[1], 3, f[1], 24); pm_fmt(e->p[2], 3, f[2], 24); pm_fmt(e->p[3], 3, f[3], 24); pm_fmt(e->r, 3, f[4], 24);
    char cab[96];
    formatar_texto(cab, sizeof cab, "ent %d %s capa=%s col=%x lt=%d", i, TIPO_NOMBRE[e->tipo < 9 ? e->tipo : 0], pm_capa[e->capa < pm_ncapa ? e->capa : 0].nome, (unsigned)pm_ent_color(e), pm_ent_ltipo(e));
    switch (e->tipo) {
    case E_PUNTO: formatar_texto(s, max, "%s p=(%s,%s)", cab, f[0], f[1]); break;
    case E_LINEA: formatar_texto(s, max, "%s p=(%s,%s)-(%s,%s)", cab, f[0], f[1], f[2], f[3]); break;
    case E_CIRCULO: formatar_texto(s, max, "%s c=(%s,%s) r=%s", cab, f[0], f[1], f[4]); break;
    case E_ARCO: { char a0[24], a1[24]; pm_fmt(pm_angnorm(e->a0) * 180 / PM_PI, 2, a0, 24); pm_fmt(pm_angnorm(e->a1) * 180 / PM_PI, 2, a1, 24); formatar_texto(s, max, "%s c=(%s,%s) r=%s a=(%s,%s)", cab, f[0], f[1], f[4], a0, a1); break; }
    case E_POLI: formatar_texto(s, max, "%s nv=%d cerrada=%d", cab, e->nv, e->cerrada); break;
    case E_TEXTO: formatar_texto(s, max, "%s p=(%s,%s) txt=\"%s\"", cab, f[0], f[1], e->txt); break;
    case E_COTA: { char t[60]; pm_cota_texto(e, t, sizeof t); formatar_texto(s, max, "%s sub=%d txt=\"%s\"", cab, e->sub, t); break; }
    default: formatar_texto(s, max, "%s sub=%d nv=%d", cab, e->sub, e->nv); break;
    }
}

static int ult_n_log = -1;
static void registrar_estado(bool todo)
{
    char s[240];
    if (pm_n != ult_n_log || todo) {
        registrar("planomini: n=%d\n", pm_n);
        ult_n_log = pm_n;
    }
    if (todo) {
        int d = pm_n > 14 ? pm_n - 14 : 0;
        for (int i = d; i < pm_n; i++) { describir(i, s, sizeof s); registrar("planomini: %s\n", s); }
    }
}

/* ============================================================ panel de órdenes, estado y dibujo de la ventana */

static void geometria(void)
{
    vx = 0; vy = TOP;
    pm_vw = j->larg;
    pm_vh = j->alt - TOP - CMD_A - ESTADO_A;
    if (pm_vw < 100) pm_vw = 100;
    if (pm_vh < 60) pm_vh = 60;
}

static const char *const ST_NOM[5] = { "REJILLA", "ORTO", "FORZC", "POLAR", "REFENT" };
static int *st_val(int i) { return i == 0 ? &pm_rejilla : i == 1 ? &pm_orto : i == 2 ? &pm_forzc : i == 3 ? &pm_polar : &pm_refent; }
#define ST_X0 196
#define ST_L 64

static void dibujar_estado(void)
{
    int y = j->alt - ESTADO_A;
    g_retangulo(j, 0, y, j->larg, ESTADO_A, W98_FACE);
    g_retangulo(j, 0, y, j->larg, 1, W98_SOMBRA);
    char t[80], a[24], b[24];
    if (mouse_x >= vx && mouse_x < vx + pm_vw && mouse_y >= vy && mouse_y < vy + pm_vh) {
        pm_fmt(cur_wx, 3, a, sizeof a); pm_fmt(cur_wy, 3, b, sizeof b);
        formatar_texto(t, sizeof t, "%s, %s", a, b);
    } else t[0] = 0;
    g_bisel(j, 3, y + 2, 186, 18, true);
    g_texto(j, 8, y + 4, t, W98_NEGRO);
    for (int i = 0; i < 5; i++) {
        int x = ST_X0 + i * (ST_L + 2);
        bool on = *st_val(i) != 0;
        g_bisel(j, x, y + 2, ST_L, 18, on);
        g_texto(j, x + (ST_L - (int)tam_texto(ST_NOM[i]) * 8) / 2 + (on ? 1 : 0), y + 4 + (on ? 1 : 0), ST_NOM[i], on ? W98_NEGRO : 0x505050);
    }
    int mx = ST_X0 + 5 * (ST_L + 2) + 8, cols = (j->larg - mx - 6) / 8;
    char m[160];
    copia_texto(m, pm_msg, sizeof m);
    if ((int)tam_texto(m) > cols && cols > 3) { m[cols - 2] = '.'; m[cols - 1] = '.'; m[cols] = 0; }
    g_texto(j, mx, y + 4, m, pm_msg_err ? 0xB00000 : W98_NEGRO);
}

static void sugerencias_dibujar(void)
{
    if (pm_activa() || !entrada.texto[0]) return;
    char sug[6][20];
    int n = pm_sugerir(entrada.texto, sug, 6);
    if (n <= 0 || (n == 1 && compara_texto(sug[0], entrada.texto) == 0)) return;
    int h = n * 16 + 4, y = j->alt - ESTADO_A - 26 - h - 2;
    g_retangulo(j, 20, y, 112, h, 0xFFFFE0);
    g_moldura(j, 20, y, 112, h, W98_NEGRO);
    for (int i = 0; i < n; i++) g_texto(j, 26, y + 2 + i * 16, sug[i], W98_NEGRO);
}

static void dibujar_panel(void)
{
    int y0 = j->alt - ESTADO_A - CMD_A;
    g_retangulo(j, 0, y0, j->larg, CMD_A, W98_FACE);
    g_retangulo(j, 0, y0, j->larg, 1, W98_SOMBRA);
    int cols = (j->larg - 16) / 8;
    for (int k = 0; k < 2; k++) {
        int idx = pm_hist_n - 2 + k;
        if (idx < 0) continue;
        char l[130];
        copia_texto(l, pm_hist_linea(idx), sizeof l);
        if ((int)tam_texto(l) > cols && cols > 3) { l[cols - 2] = '.'; l[cols - 1] = '.'; l[cols] = 0; }
        g_texto(j, 8, y0 + 3 + k * 16, l, (k == 1 && pm_msg_err) ? 0xB00000 : 0x303030);
    }
    char pr[200];
    copia_texto(pr, pm_activa() ? pm_prompt : "Escribe una orden (AYUDA muestra la lista):", sizeof pr);
    if ((int)tam_texto(pr) > cols && cols > 3) { pr[cols - 2] = '.'; pr[cols - 1] = '.'; pr[cols] = 0; }
    g_texto(j, 8, y0 + 35, pr, W98_NEGRO);
    g_texto(j, 8, y0 + 53, ">", W98_NEGRO);
    caixa_desenhar(j, &entrada);
}

static void pintar_vista(void)
{
    if (cache_sucia) { regenerar_cache(); cache_sucia = false; }
    if (cache) for (int y = 0; y < pm_vh; y++) memcpy(&j->px[(vy + y) * j->larg + vx], &cache[y * pm_vw], (size_t)pm_vw * 4);
    overlay();
    g_moldura(j, vx, vy, pm_vw, pm_vh, 0x808080);
}
static void mostrar_vista(void) { pintar_vista(); jan_mostrar_area(j, vx, vy, pm_vw, pm_vh); }

static void titulo_ventana(void)
{
    char t[260];
    const char *b = caminho[0] ? acha_ultimo_char(caminho, '/') : NULL;
    formatar_texto(t, sizeof t, "%s - PlanoMini%s", caminho[0] ? (b ? b + 1 : caminho) : "Sin título", pm_modificado ? " *" : "");
    if (compara_texto(t, ult_titulo) == 0) return;
    copia_texto(ult_titulo, t, sizeof ult_titulo);
    titulo_janela(j->id, t);
    registrar("planomini: titulo %s\n", t);
}

static void sincronizar_combos(void)
{
    for (int i = 0; i < pm_ncapa; i++) nom_capas[i] = pm_capa[i].nome;
    int capa = pm_capa_act, color = pm_color_act < 0 ? 0 : 0, lt = pm_ltipo_act == LT_CAPA ? 0 : pm_ltipo_act + 1;
    int32_t col = pm_color_act;
    int unica = -1;
    if (!pm_activa() && pm_sel_contar() == 1) for (int i = 0; i < pm_n; i++) if (pm_ent[i].sel) unica = i;
    if (unica >= 0) { capa = pm_ent[unica].capa; col = pm_ent[unica].color; lt = pm_ent[unica].ltipo == LT_CAPA ? 0 : pm_ent[unica].ltipo + 1; }
    for (int a = 1; a <= 7; a++) if (col >= 0 && (uint32_t)col == pm_aci_color(a)) color = a;
    c_capa.itens = nom_capas; c_capa.qtd = pm_ncapa; c_capa.sel = capa;
    c_color.sel = color; c_lt.sel = lt;
    combo_definir(&c_capa, pm_capa[capa < pm_ncapa ? capa : 0].nome);
    combo_definir(&c_color, nom_colores[color]);
    combo_definir(&c_lt, nom_ltipos[lt]);
}

static void layout(void)
{
    geometria();
    barra_menu_iniciar(&menu, 0, 0, j->larg);
    barra_ferr_iniciar(&barra1, 0, MENU_A, j->larg, itens1, n1);
    barra_ferr_iniciar(&barra2, 0, MENU_A + FERR_A, j->larg, itens2, n2);
    combo_iniciar(&c_capa, barra_ferr_x(&barra1, gap_capa), MENU_A + 3, 180, nom_capas, pm_ncapa, false, 7201);
    combo_iniciar(&c_color, barra_ferr_x(&barra1, gap_color), MENU_A + 3, 100, nom_colores, 8, false, 7202);
    combo_iniciar(&c_lt, barra_ferr_x(&barra1, gap_lt), MENU_A + 3, 124, nom_ltipos, 5, false, 7203);
    gui_revisar_ignorar(&c_capa); gui_revisar_ignorar(&c_color); gui_revisar_ignorar(&c_lt);
    entrada.x = 22; entrada.y = j->alt - ESTADO_A - 26; entrada.l = j->larg - 32;
    sincronizar_combos();
    cache_sucia = true;
}

static void redibujar_todo(void)
{
    sincronizar_combos();
    g_retangulo(j, 0, 0, j->larg, j->alt, W98_FACE);
    barra_menu_desenhar(j, &menu);
    barra_ferr_desenhar(j, &barra1);
    barra_ferr_desenhar(j, &barra2);
    combo_desenhar(j, &c_capa); combo_desenhar(j, &c_color); combo_desenhar(j, &c_lt);
    pintar_vista();
    dibujar_panel();
    sugerencias_dibujar();
    dibujar_estado();
    titulo_ventana();
    jan_mostrar(j);
}
static void redibujar_panel(void)
{
    dibujar_panel();
    sugerencias_dibujar();
    dibujar_estado();
    jan_mostrar_area(j, 0, j->alt - ESTADO_A - CMD_A, j->larg, CMD_A + ESTADO_A);
}

/* ============================================================ diálogos */

static uint32_t color_de_indice(int i) { return i <= 0 ? (uint32_t)COL_CAPA : pm_aci_color(i); }

static bool dialogo_props(int idx)
{
    pm_ent_t *e = &pm_ent[idx];
    char etiq[8][20];
    double val[8];
    int nc = pm_props_campos(e, etiq, val);
    bool tiene_txt = e->tipo == E_TEXTO || e->tipo == E_COTA;
    int filas = (nc + 1) / 2, alt = 24 + 3 * 30 + filas * 30 + (tiene_txt ? 30 : 0) + 56;
    jan_t *d = jan_abrir("Propiedades", 480, alt);
    if (!d) return false;
    jan_estilo(d, JAN_DIALOGO, 0, 0);
    combo_t cc, ck, cl;
    for (int i = 0; i < pm_ncapa; i++) nom_capas[i] = pm_capa[i].nome;
    combo_iniciar(&cc, 140, 10, 200, nom_capas, pm_ncapa, false, 9101);
    combo_iniciar(&ck, 140, 40, 130, nom_colores, 8, false, 9102);
    combo_iniciar(&cl, 140, 70, 150, nom_ltipos, 5, false, 9103);
    cc.sel = e->capa; combo_definir(&cc, pm_capa[e->capa].nome);
    int col = 0;
    for (int a = 1; a <= 7; a++) if (e->color >= 0 && (uint32_t)e->color == pm_aci_color(a)) col = a;
    ck.sel = col; combo_definir(&ck, nom_colores[col]);
    cl.sel = e->ltipo == LT_CAPA ? 0 : e->ltipo + 1; combo_definir(&cl, nom_ltipos[cl.sel]);
    caixa_t cajas[8], txt;
    memset(cajas, 0, sizeof cajas); memset(&txt, 0, sizeof txt);
    int y0 = 104;
    for (int i = 0; i < nc; i++) {
        char b[40];
        pm_fmt(val[i], 6, b, sizeof b);
        cajas[i].x = 100 + (i % 2) * 190; cajas[i].y = y0 + (i / 2) * 30; cajas[i].l = 84;
        caixa_definir(&cajas[i], b);
    }
    if (nc) cajas[0].foco = true;
    int yt = y0 + filas * 30;
    if (tiene_txt) { txt.x = 140; txt.y = yt; txt.l = 320; caixa_definir(&txt, e->txt); if (!nc) txt.foco = true; }
    botao_t ok = { 270, alt - 40, 90, 26, "Aceptar", false }, cancel = { 370, alt - 40, 90, 26, "Cancelar", false };
    bool aceptar = false, redib = true;
    evento_t ev;
    for (;;) {
        if (redib) {
            g_retangulo(d, 0, 0, d->larg, d->alt, W98_FACE);
            rotulo(d, 14, 14, "Capa:", W98_NEGRO); rotulo(d, 14, 44, "Color:", W98_NEGRO); rotulo(d, 14, 74, "Tipo de línea:", W98_NEGRO);
            rotulo(d, 14, 100, TIPO_NOMBRE[e->tipo < 9 ? e->tipo : 0], 0x606060);
            for (int i = 0; i < nc; i++) { rotulo(d, 14 + (i % 2) * 190 + (i % 2 ? 0 : 0), y0 + (i / 2) * 30 + 4, etiq[i], W98_NEGRO); caixa_desenhar(d, &cajas[i]); }
            if (tiene_txt) { rotulo(d, 14, yt + 4, e->tipo == E_TEXTO ? "Texto:" : "Texto fijo:", W98_NEGRO); caixa_desenhar(d, &txt); }
            boton98(d, &ok); boton98(d, &cancel);
            combo_desenhar(d, &cc); combo_desenhar(d, &ck); combo_desenhar(d, &cl);
            jan_mostrar(d);
            redib = false;
        }
        if (proximo_evento(d->id, &ev, true) != 1) continue;
        if (ev.tipo == EV_FECHAR) break;
        if (ev.tipo == EV_TECLA && ev.tecla == TECLA_ESC) break;
        if (ev.tipo == EV_MENU) {
            combo_tratar(d, &cc, &ev); combo_tratar(d, &ck, &ev); combo_tratar(d, &cl, &ev);
            redib = true;
            continue;
        }
        if (boton98_tratar(d, &ok, &ev)) { aceptar = true; break; }
        if (boton98_tratar(d, &cancel, &ev)) break;
        if (ev.tipo == EV_CLIQUE) { combo_tratar(d, &cc, &ev); combo_tratar(d, &ck, &ev); combo_tratar(d, &cl, &ev); redib = true; }
        if (ev.tipo == EV_TECLA && ev.tecla == '\n') { aceptar = true; break; }
        if (ev.tipo == EV_TECLA && ev.tecla == '\t') {
            int total = nc + (tiene_txt ? 1 : 0), k = 0;
            caixa_t *lista[9];
            for (int i = 0; i < nc; i++) lista[i] = &cajas[i];
            if (tiene_txt) lista[nc] = &txt;
            for (int i = 0; i < total; i++) if (lista[i]->foco) k = i;
            if (total) { lista[k]->foco = false; lista[(k + 1) % total]->foco = true; }
            redib = true;
            continue;
        }
        for (int i = 0; i < nc; i++) caixa_tratar(d, &cajas[i], &ev);
        if (tiene_txt) caixa_tratar(d, &txt, &ev);
        if (ev.tipo == EV_CLIQUE || ev.tipo == EV_TECLA) redib = true;
    }
    jan_fechar(d);
    if (!aceptar) return false;
    double nv[8];
    for (int i = 0; i < nc; i++) { const char *f; nv[i] = pm_atof(cajas[i].texto, &f); if (f == cajas[i].texto) nv[i] = val[i]; }
    pm_props_aplicar(e, nv);
    if (tiene_txt) copia_texto(e->txt, txt.texto, sizeof e->txt);
    bool varios = pm_sel_contar() > 1 && e->sel;
    for (int i = 0; i < pm_n; i++) {
        if (i != idx && !(varios && pm_ent[i].sel)) continue;
        pm_ent[i].capa = (uint8_t)cc.sel;
        pm_ent[i].color = (int32_t)color_de_indice(ck.sel);
        pm_ent[i].ltipo = cl.sel == 0 ? LT_CAPA : (uint8_t)(cl.sel - 1);
    }
    pm_historial_guardar();
    return true;
}

static void borrar_capa(int k)
{
    for (int i = 0; i < pm_n; i++) { if (pm_ent[i].capa == k) pm_ent[i].capa = 0; else if (pm_ent[i].capa > k) pm_ent[i].capa--; }
    for (int i = k; i + 1 < pm_ncapa; i++) pm_capa[i] = pm_capa[i + 1];
    pm_ncapa--;
    if (pm_capa_act == k) pm_capa_act = 0; else if (pm_capa_act > k) pm_capa_act--;
}

static void dialogo_capas(void)
{
    jan_t *d = jan_abrir("Administrador de capas", 540, 400);
    if (!d) return;
    jan_estilo(d, JAN_DIALOGO, 0, 0);
    botao_t b_nueva = { 10, 360, 80, 26, "Nueva...", false }, b_borrar = { 98, 360, 80, 26, "Borrar", false }, b_actual = { 186, 360, 100, 26, "Actual", false }, b_cerrar = { 440, 360, 90, 26, "Cerrar", false };
    int sel = pm_capa_act, scroll = 0;
    bool redib = true;
    evento_t ev;
#define FILAS 12
    for (;;) {
        if (redib) {
            g_retangulo(d, 0, 0, d->larg, d->alt, W98_FACE);
            g_retangulo(d, 10, 8, 520, 20, W98_FACE);
            g_texto(d, 16, 10, "Nombre", W98_NEGRO); g_texto(d, 200, 10, "Color", W98_NEGRO); g_texto(d, 270, 10, "Tipo de línea", W98_NEGRO); g_texto(d, 410, 10, "Visible", W98_NEGRO); g_texto(d, 475, 10, "Bloq.", W98_NEGRO);
            g_retangulo(d, 10, 30, 520, FILAS * 24 + 4, W98_LUZ);
            g_moldura(d, 10, 30, 520, FILAS * 24 + 4, W98_SOMBRA);
            for (int r = 0; r < FILAS && scroll + r < pm_ncapa; r++) {
                int i = scroll + r, y = 32 + r * 24;
                if (i == sel) g_retangulo(d, 11, y, 518, 24, 0xC8D8F8);
                g_texto(d, 16, y + 4, pm_capa[i].nome, W98_NEGRO);
                if (i == pm_capa_act) g_texto(d, 178, y + 4, "*", ROJO);
                g_retangulo(d, 202, y + 4, 40, 16, pm_capa[i].color);
                g_moldura(d, 202, y + 4, 40, 16, W98_NEGRO);
                g_texto(d, 270, y + 4, pm_ltipo_nombre[pm_capa[i].ltipo < LT_N ? pm_capa[i].ltipo : 0], W98_NEGRO);
                g_texto(d, 424, y + 4, pm_capa[i].visible ? "Sí" : "No", pm_capa[i].visible ? W98_NEGRO : ROJO);
                g_texto(d, 484, y + 4, pm_capa[i].bloq ? "Sí" : "No", W98_NEGRO);
            }
            g_texto(d, 14, 330, "Clic en color, tipo, visible o bloq. para cambiarlos; doble clic en el nombre para renombrar", 0x606060);
            boton98(d, &b_nueva); boton98(d, &b_borrar); boton98(d, &b_actual); boton98(d, &b_cerrar);
            jan_mostrar(d);
            redib = false;
        }
        if (proximo_evento(d->id, &ev, true) != 1) continue;
        if (ev.tipo == EV_FECHAR) break;
        if (ev.tipo == EV_TECLA && ev.tecla == TECLA_ESC) break;
        if (boton98_tratar(d, &b_cerrar, &ev)) break;
        if (boton98_tratar(d, &b_nueva, &ev)) {
            char n[32];
            n[0] = 0;
            if (pedir_texto("Nueva capa", "Nombre de la capa:", n, sizeof n) && n[0]) { int k = pm_capa_nueva(n); if (k >= 0) sel = k; else mensagem("Capas", "No se puede crear esa capa."); }
            redib = true;
            continue;
        }
        if (boton98_tratar(d, &b_borrar, &ev)) {
            int usada = 0;
            for (int i = 0; i < pm_n; i++) if (pm_ent[i].capa == sel) usada++;
            if (sel == 0) mensagem("Capas", "La capa 0 no se puede borrar.");
            else if (sel == pm_capa_act) mensagem("Capas", "No se puede borrar la capa actual.");
            else if (usada && !confirmar("Capas", "La capa tiene objetos; pasarán a la capa 0.\n¿Borrarla?")) { }
            else { borrar_capa(sel); sel = pm_capa_act; }
            redib = true;
            continue;
        }
        if (boton98_tratar(d, &b_actual, &ev)) { if (pm_capa[sel].visible) pm_capa_act = sel; else mensagem("Capas", "La capa está apagada."); redib = true; continue; }
        if (ev.tipo == EV_RODA) { scroll += (int)ev.cliques > 0 ? 1 : -1; if (scroll < 0) scroll = 0; if (scroll > pm_ncapa - FILAS) scroll = pm_ncapa > FILAS ? pm_ncapa - FILAS : 0; redib = true; continue; }
        if (ev.tipo == EV_CLIQUE && (ev.botoes & 1) && ev.x >= 10 && ev.x < 530 && ev.y >= 32 && ev.y < 32 + FILAS * 24) {
            int i = scroll + (ev.y - 32) / 24;
            if (i >= pm_ncapa) continue;
            sel = i;
            if (ev.x >= 200 && ev.x < 246) { int a = pm_color_aci(pm_capa[i].color); a = a >= 7 ? 1 : a + 1; pm_capa[i].color = pm_aci_color(a); }
            else if (ev.x >= 262 && ev.x < 400) pm_capa[i].ltipo = (uint8_t)((pm_capa[i].ltipo + 1) % LT_N);
            else if (ev.x >= 410 && ev.x < 470) { if (i == pm_capa_act && pm_capa[i].visible) mensagem("Capas", "No se puede apagar la capa actual."); else pm_capa[i].visible = !pm_capa[i].visible; }
            else if (ev.x >= 474) pm_capa[i].bloq = !pm_capa[i].bloq;
            else if (ev.cliques >= 2) {
                char n[32];
                copia_texto(n, pm_capa[i].nome, sizeof n);
                if (i > 0 && pedir_texto("Renombrar capa", "Nuevo nombre:", n, sizeof n) && n[0] && pm_capa_buscar(n) < 0) copia_texto(pm_capa[i].nome, n, sizeof pm_capa[i].nome);
            }
            redib = true;
        }
    }
#undef FILAS
    jan_fechar(d);
    pm_historial_guardar();
}

static const int ESCALAS[12] = { 0, 1, 2, 5, 10, 20, 25, 50, 75, 100, 200, 500 };
static const char *const nom_escalas[12] = { "Ajustar", "1:1", "1:2", "1:5", "1:10", "1:20", "1:25", "1:50", "1:75", "1:100", "1:200", "1:500" };
static const char *const nom_papeles[2] = { "A4 apaisado", "A3 apaisado" };

static bool dialogo_pdf(int *papel, int *escala, char *tit, int max)
{
    jan_t *d = jan_abrir("Exportar a PDF", 420, 210);
    if (!d) return false;
    jan_estilo(d, JAN_DIALOGO, 0, 0);
    combo_t cp, ce;
    combo_iniciar(&cp, 120, 12, 160, nom_papeles, 2, false, 9201);
    combo_iniciar(&ce, 120, 44, 120, nom_escalas, 12, false, 9202);
    ce.sel = 0; combo_definir(&ce, nom_escalas[0]);
    cp.sel = 0; combo_definir(&cp, nom_papeles[0]);
    caixa_t ct;
    memset(&ct, 0, sizeof ct);
    ct.x = 120; ct.y = 82; ct.l = 280; ct.foco = true;
    caixa_definir(&ct, tit);
    botao_t ok = { 220, 170, 90, 26, "Aceptar", false }, cancel = { 320, 170, 90, 26, "Cancelar", false };
    bool aceptar = false, redib = true;
    evento_t ev;
    for (;;) {
        if (redib) {
            g_retangulo(d, 0, 0, d->larg, d->alt, W98_FACE);
            rotulo(d, 14, 16, "Papel:", W98_NEGRO); rotulo(d, 14, 48, "Escala:", W98_NEGRO); rotulo(d, 14, 86, "Título:", W98_NEGRO);
            rotulo(d, 14, 118, "El cajetín lleva el título, la escala y la hoja.", 0x606060);
            rotulo(d, 14, 138, "Las unidades del dibujo son milímetros.", 0x606060);
            caixa_desenhar(d, &ct);
            boton98(d, &ok); boton98(d, &cancel);
            combo_desenhar(d, &cp); combo_desenhar(d, &ce);
            jan_mostrar(d);
            redib = false;
        }
        if (proximo_evento(d->id, &ev, true) != 1) continue;
        if (ev.tipo == EV_FECHAR || (ev.tipo == EV_TECLA && ev.tecla == TECLA_ESC)) break;
        if (ev.tipo == EV_MENU) { combo_tratar(d, &cp, &ev); combo_tratar(d, &ce, &ev); redib = true; continue; }
        if (boton98_tratar(d, &ok, &ev)) { aceptar = true; break; }
        if (boton98_tratar(d, &cancel, &ev)) break;
        if (ev.tipo == EV_CLIQUE) { combo_tratar(d, &cp, &ev); combo_tratar(d, &ce, &ev); redib = true; }
        if (ev.tipo == EV_TECLA && ev.tecla == '\n') { aceptar = true; break; }
        caixa_tratar(d, &ct, &ev);
        if (ev.tipo == EV_CLIQUE || ev.tipo == EV_TECLA) redib = true;
    }
    jan_fechar(d);
    if (!aceptar) return false;
    *papel = cp.sel; *escala = ESCALAS[ce.sel];
    copia_texto(tit, ct.texto, max);
    return true;
}

static void ayuda_ordenes(void)
{
    mensagem("Órdenes de PlanoMini",
             "Dibujo: LINEA(L) PLINEA(PL) RECTANG(REC) POLIGONO(POL)\n"
             "  CIRCULO(C) ARCO(A) PUNTO(PO) TEXTO(T) SOMBREADO(H)\n"
             "Cotas: COTALIN COTAHOR COTAVER COTAALI COTARAD COTADIAM\n"
             "  COTAANG COTAESTILO\n"
             "Modificar: MOVER(M) COPIAR(CO) GIRAR(RO) ESCALA(SC)\n"
             "  SIMETRIA(MI) EQUIDIST(O) RECORTA(TR) ALARGA(EX)\n"
             "  EMPALME(F) CHAFLAN(CHA) BORRAR(E) EXPLOTA(X) MATRIZ(AR)\n"
             "  IGUALAPROP(MA)\n"
             "Vista: ZOOM(Z) PAN REGEN   Ayudas: REJILLA ORTO FORZC\n"
             "  POLAR REFENT   Capas: CAPA COLOR TLINEA\n"
             "Archivo: GUARDAR ABRIR NUEVO EXPORTAR IMPORTAR SALIR\n"
             "También en inglés (LINE, CIRCLE, MOVE, TRIM, OFFSET...).\n"
             "Puntos: x,y   @dx,dy   @d<ángulo   (solo d con ORTO o POLAR)\n"
             "Intro repite la orden; Esc cancela; F7 F8 F9 F10 F3 = ayudas.");
}

/* ============================================================ acciones */

static bool pode_descartar(void)
{
    if (!pm_modificado) return true;
    return confirmar("PlanoMini", "El dibujo tiene cambios sin guardar.\n¿Quieres descartarlos?");
}

static void nuevo(void)
{
    pm_cancelar();
    pm_limpiar();
    vista_inicial();
    caminho[0] = 0;
    pm_historial_reiniciar();
    cache_sucia = true;
    registrar("planomini: nuevo\n");
}

static void error_dlg(const char *titulo_, const char *m)
{
    char t[300];
    formatar_texto(t, sizeof t, "%s%s%s", m, pm_erro[0] ? "\n" : "", pm_erro);
    mensagem(titulo_, t);
}

static bool guardar_como(void)
{
    char c[200];
    copia_texto(c, caminho[0] ? caminho : "/home/dibujo.pmini", sizeof c);
    if (!dialogo_guardar("Guardar dibujo", FILTROS_PM, c, c, sizeof c) || !c[0]) return false;
    if (!gravar_pmini(c)) { mensagem("PlanoMini", "No se ha podido guardar el archivo."); return false; }
    copia_texto(caminho, c, sizeof caminho);
    pm_modificado = false;
    registrar("planomini: guardado %s\n", caminho);
    return true;
}
static bool guardar(void)
{
    if (!caminho[0]) return guardar_como();
    if (!gravar_pmini(caminho)) { mensagem("PlanoMini", "No se ha podido guardar el archivo."); return false; }
    pm_modificado = false;
    registrar("planomini: guardado %s\n", caminho);
    return true;
}
static void abrir_ruta(const char *c)
{
    if (!abrir_dibujo(c)) { error_dlg("PlanoMini", "No se ha podido abrir el archivo."); registrar("planomini: error al abrir %s: %s\n", c, pm_erro); return; }
    cache_sucia = true;
    registrar_estado(true);
}

static void acao(int a);

/* Peticiones que salen de la línea de órdenes (GUARDAR, ABRIR, EXPORTAR...). */
static void procesar_acciones(void)
{
    while (pm_accion) {
        int a = pm_accion;
        char arg[200];
        copia_texto(arg, pm_accion_arg, sizeof arg);
        pm_accion = 0;
        switch (a) {
        case PMA_ABRIR: if (arg[0]) { if (pode_descartar()) abrir_ruta(arg); } else acao(A_ABRIR); break;
        case PMA_GUARDAR:
            if (arg[0]) { if (gravar_pmini(arg)) { copia_texto(caminho, arg, sizeof caminho); pm_modificado = false; registrar("planomini: guardado %s\n", arg); } else mensagem("PlanoMini", "No se ha podido guardar el archivo."); }
            else guardar();
            break;
        case PMA_NUEVO: acao(A_NUEVO); break;
        case PMA_SALIR: if (pode_descartar()) sair_pedido = true; break;
        case PMA_CAPAS: acao(A_CAPAS); break;
        case PMA_PROPS: acao(A_PROPS); break;
        case PMA_PDF: if (arg[0]) { if (exportar_pdf_args(arg) != 0) mensagem("PlanoMini", "No se ha podido exportar el PDF."); } else acao(A_PDF); break;
        case PMA_PNG: if (arg[0]) { if (exportar_png(arg) != 0) mensagem("PlanoMini", "No se ha podido exportar la imagen."); } else acao(A_PNG); break;
        case PMA_DXF: if (arg[0]) { if (!gravar_dxf(arg)) mensagem("PlanoMini", "No se ha podido guardar el DXF."); } else acao(A_GUARDAR_DXF); break;
        case PMA_IMPDXF: if (arg[0]) { if (pode_descartar()) abrir_ruta(arg); } else acao(A_ABRIR_DXF); break;
        case PMA_REDIBUJAR: cache_sucia = true; break;
        default: break;
        }
    }
}

static void orden(const char *t)
{
    pm_orden(t);
    procesar_acciones();
}

static void acao(int a)
{
    if (a >= A_CMD && a < A_CMD + N_CMDS) {
        pm_iniciar_orden(CMDS[a - A_CMD]);
        procesar_acciones();
        registrar("planomini: orden %s\n", CMDS[a - A_CMD]);
        goto fin;
    }
    switch (a) {
    case A_NUEVO: if (pode_descartar()) nuevo(); break;
    case A_ABRIR: {
        if (!pode_descartar()) break;
        char c[200];
        copia_texto(c, caminho[0] ? caminho : "/ejemplos/planomini", sizeof c);
        if (dialogo_abrir("Abrir dibujo", FILTROS_ABRIR, c, c, sizeof c) && c[0]) abrir_ruta(c);
        break;
    }
    case A_GUARDAR: guardar(); break;
    case A_GUARDAR_COMO: guardar_como(); break;
    case A_ABRIR_DXF: {
        if (!pode_descartar()) break;
        char c[200];
        copia_texto(c, "/home", sizeof c);
        if (dialogo_abrir("Abrir DXF", FILTROS_DXF, c, c, sizeof c) && c[0]) abrir_ruta(c);
        break;
    }
    case A_GUARDAR_DXF: {
        char c[200];
        copia_texto(c, "/home/dibujo.dxf", sizeof c);
        if (!dialogo_guardar("Guardar como DXF", FILTROS_DXF, c, c, sizeof c) || !c[0]) break;
        if (!gravar_dxf(c)) mensagem("PlanoMini", "No se ha podido guardar el DXF."); else copia_texto(pm_msg, "DXF guardado", sizeof pm_msg);
        break;
    }
    case A_PNG: {
        char c[200];
        copia_texto(c, "/home/dibujo.png", sizeof c);
        if (!dialogo_guardar("Exportar a PNG", FILTROS_PNG, c, c, sizeof c) || !c[0]) break;
        if (exportar_png(c) != 0) mensagem("PlanoMini", "No se ha podido exportar la imagen (¿hay algo que dibujar?)."); else copia_texto(pm_msg, "Imagen PNG exportada", sizeof pm_msg);
        break;
    }
    case A_PDF: {
        int papel = 0, escala = 0;
        char tit[80], c[200];
        tit[0] = 0;
        if (caminho[0]) { const char *b = acha_ultimo_char(caminho, '/'); copia_texto(tit, b ? b + 1 : caminho, sizeof tit); char *p = acha_ultimo_char(tit, '.'); if (p) *p = 0; }
        if (!dialogo_pdf(&papel, &escala, tit, sizeof tit)) break;
        copia_texto(c, "/home/dibujo.pdf", sizeof c);
        if (!dialogo_guardar("Exportar a PDF", FILTROS_PDF, c, c, sizeof c) || !c[0]) break;
        if (exportar_pdf(c, papel, escala, tit) != 0) mensagem("PlanoMini", "No se ha podido exportar el PDF (¿hay algo que dibujar?)."); else copia_texto(pm_msg, "PDF exportado", sizeof pm_msg);
        break;
    }
    case A_SALIR: if (pode_descartar()) sair_pedido = true; break;
    case A_DESHACER: pm_cancelar(); if (pm_deshacer()) { copia_texto(pm_msg, "Deshacer", sizeof pm_msg); pm_msg_err = false; } cache_sucia = true; registrar("planomini: deshacer (%d entidades)\n", pm_n); break;
    case A_REHACER: pm_cancelar(); if (pm_rehacer()) { copia_texto(pm_msg, "Rehacer", sizeof pm_msg); pm_msg_err = false; } cache_sucia = true; registrar("planomini: rehacer (%d entidades)\n", pm_n); break;
    case A_SELTODO: pm_cancelar(); pm_sel_todo(true); break;
    case A_BORRAR_SEL: if (pm_sel_contar()) { pm_cancelar(); pm_iniciar_orden("BORRAR"); } break;
    case A_PROPS: {
        int unica = -1;
        for (int i = 0; i < pm_n; i++) if (pm_ent[i].sel) { unica = i; break; }
        if (unica < 0) { copia_texto(pm_msg, "Elige un objeto para ver sus propiedades", sizeof pm_msg); pm_msg_err = false; break; }
        if (dialogo_props(unica)) registrar("planomini: propiedades ent %d\n", unica);
        break;
    }
    case A_ZOOM_EXT: pm_zoom_ext(); break;
    case A_ZOOM_VENT: pm_cancelar(); pm_iniciar_orden("ZOOM"); pm_orden("Ventana"); break;
    case A_ZOOM_PREV: pm_zoom_prev(); break;
    case A_ZOOM_MAS: pm_vista_guardar(); pm_zoom_factor(1.25, pm_vw / 2.0, pm_vh / 2.0); break;
    case A_ZOOM_MENOS: pm_vista_guardar(); pm_zoom_factor(0.8, pm_vw / 2.0, pm_vh / 2.0); break;
    case A_REGEN: break;
    case A_FONDO_OSC: fondo_claro = false; break;
    case A_FONDO_CLARO: fondo_claro = true; break;
    case A_CAPAS: pm_cancelar(); dialogo_capas(); break;
    case A_EJ1: case A_EJ2: case A_EJ3: {
        if (!pode_descartar()) break;
        char c[120];
        formatar_texto(c, sizeof c, "/ejemplos/planomini/%s", EJEMPLOS[a - A_EJ1]);
        abrir_ruta(c);
        break;
    }
    case A_ORDENES: ayuda_ordenes(); break;
    case A_ACERCA:
        mensagem("Acerca de PlanoMini",
                 "PlanoMini - mini CAD 2D de SaguiOS\n"
                 "© 2026 Alejandro Anuncibay, GPL-3.0 o posterior.\n"
                 "Nace del prototipo PlanoMini del autor (C++/SDL);\n"
                 "esta versión nativa es una reescritura.\n"
                 "Inspirado en los CAD de órdenes clásicos; independiente:\n"
                 "sin código, iconos ni formatos propietarios de terceros.\n"
                 "Lee y escribe DXF ASCII (formato de intercambio).");
        break;
    case A_ESTILO_COTA: pm_iniciar_orden("COTAESTILO"); break;
    case A_REJ: orden("REJILLA"); break;
    case A_ORTO: orden("ORTO"); break;
    case A_FORZC: orden("FORZC"); break;
    case A_POLAR: orden("POLAR"); break;
    case A_REF: orden("REFENT"); break;
    }
fin:
    procesar_acciones();
    cache_sucia = true;
    registrar_estado(false);
    layout();
    redibujar_todo();
}

/* ============================================================ eventos de la vista */

static int ctrl_(const evento_t *e) { return (e->modificadores & 2) && !(e->modificadores & 4); }

static void tras_orden(const char *txt)
{
    if (txt) registrar("planomini: orden %s\n", txt);
    procesar_acciones();
    cache_sucia = true;
    registrar_estado(true);
}

static bool hay_grip_en(int sx, int sy, int *ent, int *idx)
{
    static double g[260][2];
    static int ge[260], gi[260];
    int n = grips_de_seleccion(g, ge, gi, 260);
    double mejor = 49;
    bool hay = false;
    for (int i = 0; i < n; i++) {
        double dx = pm_w2sx(g[i][0]) - sx, dy = pm_w2sy(g[i][1]) - sy, d2 = dx * dx + dy * dy;
        if (d2 <= mejor) { mejor = d2; *ent = ge[i]; *idx = gi[i]; hay = true; }
    }
    return hay;
}

static void clic_vista(const evento_t *e)
{
    int sx = e->x - vx, sy = e->y - vy;
    mouse_x = e->x; mouse_y = e->y;
    cursor_actualizar(sx, sy);
    bool shift = (e->modificadores & 1) != 0;
    if (e->botoes & 2) {
        arrastre = D_PAN; arr_sx = e->x; arr_sy = e->y; arr_vcx = pm_vcx; arr_vcy = pm_vcy; arr_movio = false;
        return;
    }
    if (!(e->botoes & 1)) return;
    double rx = pm_s2wx(sx), ry = pm_s2wy(sy);
    pm_quitar = shift;
    if (pm_activa()) {
        if (pm_esp == ESP_PUNTO || pm_esp == ESP_NUM) { pm_punto(cur_wx, cur_wy); tras_orden(NULL); }
        else if (pm_esp == ESP_SEL) { arrastre = D_VENTANA; arr_sx = e->x; arr_sy = e->y; arr_movio = false; arr_shift = shift; }
        else if (pm_esp == ESP_OBJ) { pm_punto(rx, ry); tras_orden(NULL); }
        pm_quitar = false;
        return;
    }
    int g_e, g_i;
    if (pm_sel_contar() && hay_grip_en(sx, sy, &g_e, &g_i)) {
        arrastre = D_GRIP; grip_ent = g_e; grip_idx = g_i;
        pm_ent_clonar(&grip_orig, &pm_ent[g_e]);
        arr_movio = false;
        pm_quitar = false;
        return;
    }
    int i = pm_entidad_en(rx, ry, 6.0 / pm_ve, -1);
    if (i >= 0) {
        if (e->cliques >= 2) {
            pm_sel_todo(false); pm_ent[i].sel = 1;
            cache_sucia = true;
            acao(A_PROPS);
            pm_quitar = false;
            return;
        }
        pm_ent[i].sel = shift ? 0 : 1;
        registrar("planomini: sel %d (%d seleccionados)\n", i, pm_sel_contar());
        cache_sucia = true;
    } else {
        arrastre = D_VENTANA; arr_sx = e->x; arr_sy = e->y; arr_movio = false; arr_shift = shift;
    }
    pm_quitar = false;
}

static void soltar_vista(const evento_t *e)
{
    (void)e;
    int modo = arrastre;
    arrastre = D_NADA;
    if (modo == D_PAN) {
        if (!arr_movio) { pm_enter(); tras_orden(NULL); }
        return;
    }
    if (modo == D_GRIP) {
        pm_ent_liberar(&grip_orig);
        pm_historial_guardar();
        registrar("planomini: grip ent %d\n", grip_ent);
        registrar_estado(true);
        grip_ent = -1;
        cache_sucia = true;
        return;
    }
    if (modo == D_VENTANA) {
        int dx = mouse_x - arr_sx, dy = mouse_y - arr_sy;
        pm_quitar = arr_shift;
        if (dx * dx + dy * dy > 16) {
            double x0 = pm_s2wx(arr_sx - vx), y0 = pm_s2wy(arr_sy - vy), x1 = pm_s2wx(mouse_x - vx), y1 = pm_s2wy(mouse_y - vy);
            if (pm_activa() && pm_esp != ESP_SEL) { pm_quitar = false; return; }
            pm_ventana(x0, y0, x1, y1);
            registrar("planomini: ventana %s (%d seleccionados)\n", x1 < x0 ? "captura" : "normal", pm_sel_contar());
        } else if (pm_activa()) {
            pm_punto(pm_s2wx(mouse_x - vx), pm_s2wy(mouse_y - vy));
            tras_orden(NULL);
        } else if (!arr_shift) { pm_sel_todo(false); }
        pm_quitar = false;
        cache_sucia = true;
    }
}

static void mover_vista(const evento_t *e)
{
    mouse_x = e->x; mouse_y = e->y;
    int sx = e->x - vx, sy = e->y - vy;
    if (arrastre == D_PAN) {
        int dx = e->x - arr_sx, dy = e->y - arr_sy;
        if (dx * dx + dy * dy > 9) arr_movio = true;
        pm_vcx = arr_vcx - dx / pm_ve; pm_vcy = arr_vcy + dy / pm_ve;
        cache_sucia = true;
        return;
    }
    if (arrastre == D_VENTANA) {
        int dx = e->x - arr_sx, dy = e->y - arr_sy;
        if (dx * dx + dy * dy > 16) arr_movio = true;
        cursor_actualizar(sx, sy);
        return;
    }
    cursor_actualizar(sx, sy);
    if (arrastre == D_GRIP && grip_ent >= 0 && grip_ent < pm_n) {
        arr_movio = true;
        pm_ent_liberar(&pm_ent[grip_ent]);
        pm_ent_clonar(&pm_ent[grip_ent], &grip_orig);
        pm_grip_mover(grip_ent, grip_idx, cur_wx, cur_wy);
        cache_sucia = true;
    }
}

/* ---- línea de órdenes ---- */

static void completar_tab(void)
{
    char sug[8][20];
    char base[40];
    if (tab_idx < 0) { copia_texto(tab_base, entrada.texto, sizeof tab_base); }
    copia_texto(base, tab_base, sizeof base);
    int n = pm_sugerir(base, sug, 8);
    if (n <= 0) return;
    tab_idx = (tab_idx + 1) % n;
    caixa_definir(&entrada, sug[tab_idx]);
    entrada.cursor = (int)tam_texto(entrada.texto);
}

static void ejecutar_linea(void)
{
    char l[130];
    copia_texto(l, entrada.texto, sizeof l);
    caixa_definir(&entrada, "");
    entrada.cursor = 0;
    tab_idx = -1;
    if (l[0]) {
        if (ord_n == 40) { memmove(ord_hist[0], ord_hist[1], sizeof ord_hist[0] * 39); ord_n--; }
        copia_texto(ord_hist[ord_n++], l, sizeof ord_hist[0]);
    }
    ord_pos = ord_n;
    pm_quitar = false;
    orden(l);
    tras_orden(l[0] ? l : "<Intro>");
}

static void tecla(const evento_t *e)
{
    unsigned t = e->tecla;
    if (ctrl_(e)) {
        switch (t) {
        case 14: acao(A_NUEVO); return;
        case 15: acao(A_ABRIR); return;
        case 19: acao(A_GUARDAR); return;
        case 16: acao(A_PDF); return;
        case 26: acao(A_DESHACER); return;
        case 25: acao(A_REHACER); return;
        case 1: acao(A_SELTODO); return;
        default: break;
        }
    }
    if (t == TECLA_ESC) {
        if (arrastre == D_GRIP && grip_ent >= 0) { pm_ent_liberar(&pm_ent[grip_ent]); pm_ent_clonar(&pm_ent[grip_ent], &grip_orig); pm_ent_liberar(&grip_orig); grip_ent = -1; }
        arrastre = D_NADA;
        caixa_definir(&entrada, ""); entrada.cursor = 0; tab_idx = -1;
        pm_cancelar();
        tras_orden("<Esc>");
        redibujar_todo();
        return;
    }
    if (t >= 0x10A && t <= 0x113) {       /* F1..F10 */
        int f = (int)t - 0x109;
        if (f == 1) { acao(A_ORDENES); return; }
        if (f == 2) { pm_iniciar_orden("LINEA"); tras_orden("LINEA"); redibujar_todo(); return; }
        if (f == 3) { acao(A_REF); return; }
        if (f == 4) { pm_iniciar_orden("RECTANG"); tras_orden("RECTANG"); redibujar_todo(); return; }
        if (f == 7) { acao(A_REJ); return; }
        if (f == 8) { acao(A_ORTO); return; }
        if (f == 9) { acao(A_FORZC); return; }
        if (f == 10) { acao(A_POLAR); return; }
        return;
    }
    if (t == '\t') { completar_tab(); redibujar_panel(); return; }
    if (t == TECLA_CIMA || t == TECLA_BAIXO) {
        if (ord_n) {
            ord_pos += t == TECLA_CIMA ? -1 : 1;
            if (ord_pos < 0) ord_pos = 0;
            if (ord_pos >= ord_n) { ord_pos = ord_n; caixa_definir(&entrada, ""); entrada.cursor = 0; }
            else { caixa_definir(&entrada, ord_hist[ord_pos]); entrada.cursor = (int)tam_texto(entrada.texto); }
            redibujar_panel();
        }
        return;
    }
    if (t == TECLA_DELETE && !entrada.texto[0]) { acao(A_BORRAR_SEL); return; }
    if (t == ' ' && !entrada.texto[0] && pm_esp != ESP_TEXTO) {              /* espacio = Intro */
        pm_enter();
        tras_orden("<Intro>");
        redibujar_todo();
        return;
    }
    tab_idx = -1;
    if (caixa_tratar(j, &entrada, e)) { ejecutar_linea(); redibujar_todo(); }
    else redibujar_panel();
}

/* ============================================================ modo sin ventana (pruebas) */

static int modo_sin_ventana(int argc, char **argv)
{
    const char *pdf = NULL, *png = NULL, *gua = NULL, *dxf = NULL, *abrir_f = NULL, *papel = NULL;
    int escala = 0;
    bool listar = false;
    char *ordenes[60];
    int n_ord = 0;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--pdf") == 0 && i + 1 < argc) pdf = argv[++i];
        else if (compara_texto(argv[i], "--png") == 0 && i + 1 < argc) png = argv[++i];
        else if (compara_texto(argv[i], "--guardar") == 0 && i + 1 < argc) gua = argv[++i];
        else if (compara_texto(argv[i], "--dxf") == 0 && i + 1 < argc) dxf = argv[++i];
        else if (compara_texto(argv[i], "--abrir") == 0 && i + 1 < argc) abrir_f = argv[++i];
        else if (compara_texto(argv[i], "--escala") == 0 && i + 1 < argc) escala = texto_para_int(argv[++i]);
        else if (compara_texto(argv[i], "--papel") == 0 && i + 1 < argc) papel = argv[++i];
        else if (compara_texto(argv[i], "--listar") == 0) listar = true;
        else if (compara_texto(argv[i], "--orden") == 0) {
            while (i + 1 < argc && (argv[i + 1][0] != '-' || (argv[i + 1][1] >= '0' && argv[i + 1][1] <= '9') || argv[i + 1][1] == 0) && n_ord < 60) ordenes[n_ord++] = argv[++i];
            while (i + 1 < argc && argv[i + 1][0] == '-' && argv[i + 1][1] != '-' && n_ord < 60) ordenes[n_ord++] = argv[++i];   /* -CAPA y similares */
        }
    }
    pm_limpiar();
    vista_inicial();
    pm_vw = 800; pm_vh = 500;
    pm_historial_reiniciar();
    if (abrir_f && !abrir_dibujo(abrir_f)) { escrever_texto(SAIDA, "planomini: error "); escrever_texto(SAIDA, pm_erro); escrever_texto(SAIDA, "\n"); return 1; }
    for (int i = 0; i < n_ord; i++) {
        if (compara_texto(ordenes[i], "<Intro>") == 0) pm_enter(); else pm_orden(ordenes[i]);
        char s[220];
        formatar_texto(s, sizeof s, "planomini: orden %s -> %s%s\n", ordenes[i], pm_msg, pm_msg_err ? " [error]" : "");
        escrever_texto(SAIDA, s);
        while (pm_accion) {
            int a = pm_accion;
            char arg[200];
            copia_texto(arg, pm_accion_arg, sizeof arg);
            pm_accion = 0;
            if (a == PMA_GUARDAR && arg[0]) { if (gravar_pmini(arg)) escrever_texto(SAIDA, "planomini: guardado\n"); }
            else if (a == PMA_DXF && arg[0]) { gravar_dxf(arg); }
            else if (a == PMA_PDF && arg[0]) { if (exportar_pdf_args(arg) != 0) escrever_texto(SAIDA, "planomini: no se pudo exportar el PDF\n"); }
            else if (a == PMA_PNG && arg[0]) { if (exportar_png(arg) != 0) escrever_texto(SAIDA, "planomini: no se pudo exportar el PNG\n"); }
            else if ((a == PMA_ABRIR || a == PMA_IMPDXF) && arg[0]) abrir_dibujo(arg);
        }
    }
    if (pm_activa()) pm_cancelar();
    char s[260];
    if (listar || n_ord || abrir_f) for (int i = 0; i < pm_n; i++) { describir(i, s, sizeof s); escrever_texto(SAIDA, s); escrever_texto(SAIDA, "\n"); }
    int rc = 0;
    if (gua && !gravar_pmini(gua)) { escrever_texto(SAIDA, "planomini: no se pudo guardar\n"); rc = 1; }
    if (dxf && !gravar_dxf(dxf)) { escrever_texto(SAIDA, "planomini: no se pudo guardar el DXF\n"); rc = 1; }
    if (png && exportar_png(png) != 0) { escrever_texto(SAIDA, "planomini: no se pudo exportar el PNG\n"); rc = 1; }
    if (pdf && exportar_pdf(pdf, papel && (papel[0] == 'A' || papel[0] == 'a') && papel[1] == '3' ? 1 : 0, escala, NULL) != 0) { escrever_texto(SAIDA, "planomini: no se pudo exportar el PDF\n"); rc = 1; }
    formatar_texto(s, sizeof s, "planomini: %d entidades, %d capas\n", pm_n, pm_ncapa);
    escrever_texto(SAIDA, s);
    dormir(300);                 /* que la salida se vacíe antes del aviso de fin de proceso (las pruebas leen la serie) */
    return rc;
}

/* ============================================================ principal */

int principal(int argc, char **argv)
{
    if (argc >= 2 && argv[1][0] == '-' && argv[1][1] == '-' ) return modo_sin_ventana(argc, argv);

    j = jan_abrir("Sin título - PlanoMini", 960, 640);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK, 700, 460);
    pm_limpiar();
    vista_inicial();
    itens_iniciar();
    for (int i = 0; i < 8; i++) { menu.nomes[i] = menu_nomes[i]; menu.itens[i] = menu_textos[i]; }
    menu.n = 8;
    memset(&entrada, 0, sizeof entrada);
    entrada.foco = true;
    geometria();
    layout();
    if (argc >= 2) {
        if (!abrir_dibujo(argv[1])) { error_dlg("PlanoMini", "No se ha podido abrir el archivo."); pm_historial_reiniciar(); }
    } else pm_historial_reiniciar();
    cache_sucia = true;
    redibujar_todo();
    registrar("planomini: pronto\n");
    for (int i = 0; i < n1; i++) if (itens1[i].id > 0) registrar("planomini: boton %d %d 1\n", itens1[i].id, barra_ferr_x(&barra1, i));
    for (int i = 0; i < n2; i++) if (itens2[i].id > 0) registrar("planomini: boton %d %d 2\n", itens2[i].id, barra_ferr_x(&barra2, i));
    registrar("planomini: vista area=%d,%d,%d,%d status=%d\n", vx, vy, pm_vw, pm_vh, j->alt - ESTADO_A);
    registrar("planomini: vista cx=%d cy=%d e100=%d\n", (int)(pm_vcx * 100), (int)(pm_vcy * 100), (int)(pm_ve * 100));
    registrar_estado(true);

    evento_t e;
    while (!sair_pedido) {
        if (tem_adiado) { e = adiado; tem_adiado = false; }
        else if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) { if (pode_descartar()) break; continue; }
        if (jan_redimensionar(j, &e)) { layout(); redibujar_todo(); continue; }
        if (e.tipo == EV_MENU) {
            int m = barra_menu_tratar(j, &menu, &e);
            if (m >= 0) { int a = menu_acoes[m / 100][m % 100]; if (a) acao(a); continue; }
            int r = combo_tratar(j, &c_capa, &e);
            if (r == COMBO_SELECIONOU) {
                if (!pm_activa() && pm_sel_contar()) { for (int i = 0; i < pm_n; i++) if (pm_ent[i].sel) pm_ent[i].capa = (uint8_t)c_capa.sel; pm_historial_guardar(); }
                else if (pm_capa[c_capa.sel].visible) pm_capa_act = c_capa.sel;
                cache_sucia = true; redibujar_todo(); continue;
            }
            r = combo_tratar(j, &c_color, &e);
            if (r == COMBO_SELECIONOU) {
                int32_t col = (int32_t)color_de_indice(c_color.sel);
                if (!pm_activa() && pm_sel_contar()) { for (int i = 0; i < pm_n; i++) if (pm_ent[i].sel) pm_ent[i].color = col; pm_historial_guardar(); } else pm_color_act = col;
                cache_sucia = true; redibujar_todo(); continue;
            }
            r = combo_tratar(j, &c_lt, &e);
            if (r == COMBO_SELECIONOU) {
                int lt = c_lt.sel == 0 ? LT_CAPA : c_lt.sel - 1;
                if (!pm_activa() && pm_sel_contar()) { for (int i = 0; i < pm_n; i++) if (pm_ent[i].sel) pm_ent[i].ltipo = (uint8_t)lt; pm_historial_guardar(); } else pm_ltipo_act = lt;
                cache_sucia = true; redibujar_todo(); continue;
            }
            continue;
        }
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && e.y < MENU_A) { barra_menu_tratar(j, &menu, &e); continue; }
        if (e.tipo == EV_CLIQUE && e.y >= MENU_A && e.y < MENU_A + FERR_A) { combo_tratar(j, &c_capa, &e); combo_tratar(j, &c_color, &e); combo_tratar(j, &c_lt, &e); }
        {
            int id = barra_ferr_tratar(j, &barra1, &e);
            if (id <= 0) id = barra_ferr_tratar(j, &barra2, &e);
            if (id > 0) { acao(id); continue; }
            if (e.tipo == EV_CLIQUE && e.y >= MENU_A && e.y < TOP) continue;
        }
        if (e.tipo == EV_MOVER) {
            if (arrastre != D_NADA) {
                while (proximo_evento(j->id, &adiado, false) == 1) {
                    if (adiado.tipo == EV_MOVER) e = adiado; else { tem_adiado = true; break; }
                }
            }
            if (e.y >= vy && e.y < vy + pm_vh && e.x >= vx && e.x < vx + pm_vw) {
                mover_vista(&e);
                mostrar_vista();
                char t[60], a[24], b[24];
                pm_fmt(cur_wx, 3, a, sizeof a); pm_fmt(cur_wy, 3, b, sizeof b);
                formatar_texto(t, sizeof t, "%s, %s", a, b);
                dibujar_estado();
                jan_mostrar_area(j, 0, j->alt - ESTADO_A, j->larg, ESTADO_A);
            } else if (arrastre != D_NADA) { mover_vista(&e); mostrar_vista(); }
            continue;
        }
        if (e.tipo == EV_SOLTOU) {
            if (arrastre != D_NADA) { soltar_vista(&e); redibujar_todo(); }
            continue;
        }
        if (e.tipo == EV_RODA) {
            int n = (int)e.cliques;
            if (e.x >= vx && e.y >= vy && e.y < vy + pm_vh) {
                double f = 1.0;
                for (int k = 0; k < (n < 0 ? -n : n); k++) f *= n < 0 ? 1.2 : 1.0 / 1.2;
                pm_zoom_factor(f, e.x - vx, e.y - vy);
                cache_sucia = true;
                cursor_actualizar(e.x - vx, e.y - vy);
                mouse_x = e.x; mouse_y = e.y;
                mostrar_vista();
            }
            continue;
        }
        if (e.tipo == EV_CLIQUE) {
            if (e.y >= j->alt - ESTADO_A) {                       /* botones de la barra de estado */
                if (e.botoes & 1) {
                    for (int i = 0; i < 5; i++) {
                        int x = ST_X0 + i * (ST_L + 2);
                        if (e.x >= x && e.x < x + ST_L) { static const int ids[5] = { A_REJ, A_ORTO, A_FORZC, A_POLAR, A_REF }; acao(ids[i]); }
                    }
                }
                continue;
            }
            if (e.y >= vy && e.y < vy + pm_vh && e.x >= vx) { clic_vista(&e); redibujar_todo(); continue; }
            if (e.y >= j->alt - ESTADO_A - CMD_A) { entrada.foco = true; caixa_tratar(j, &entrada, &e); redibujar_panel(); }
            continue;
        }
        if (e.tipo == EV_TECLA) tecla(&e);
    }
    jan_fechar(j);
    registrar("planomini: fechado\n");
    return 0;
}
