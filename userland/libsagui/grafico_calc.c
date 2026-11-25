// SPDX-License-Identifier: GPL-3.0-only
/* grafico_calc.c - la parte de cálculo del componente de gráficos (D469): escalas con marcas «bonitas»,
 * formato de números, apilado, interpolación y curvas de suavizado, geometría de barras, histograma y
 * colisiones. Todo son funciones puras sobre números: no usan la libc ni el resto de la libsagui, así se
 * compila igual en el PC (tests/grafico_unidad.c) y en SaguiOS (con la x87, ver el Makefile: libsagui no
 * usa SSE por omisión).
 *
 * Sin libm: seno y coseno se calculan con una serie de Taylor tras reducir el ángulo. */
#include "grafico.h"

#define PI  3.14159265358979323846
#define PI2 6.28318530717958647692

double gr_abs(double x) { return x < 0 ? -x : x; }

double gr_floor(double x)
{
    if (x > 4e18 || x < -4e18) return x;
    long long i = (long long)x;
    double d = (double)i;
    return d > x ? d - 1 : d;
}

double gr_ceil(double x)
{
    double f = gr_floor(x);
    return f < x ? f + 1 : f;
}

double gr_raiz(double x)
{
    if (x <= 0) return 0;
    double r = x > 1 ? x / 2 : 1;
    for (int i = 0; i < 40; i++) r = 0.5 * (r + x / r);
    return r;
}

double gr_p10(int e)
{
    double r = 1;
    if (e >= 0) while (e-- > 0) r *= 10;
    else while (e++ < 0) r /= 10;
    return r;
}

double gr_seno(double a)
{
    a -= PI2 * gr_floor(a / PI2 + 0.5);          /* [-pi, pi] */
    if (a > PI / 2) a = PI - a;
    else if (a < -PI / 2) a = -PI - a;
    double a2 = a * a, t = a, s = a;
    for (int k = 1; k <= 7; k++) { t *= -a2 / ((2 * k) * (2 * k + 1)); s += t; }
    return s;
}

double gr_coseno(double a) { return gr_seno(a + PI / 2); }

void gr_dir(double a, double *dx, double *dy)
{
    *dx = gr_seno(a);
    *dy = -gr_coseno(a);
}

double gr_lerp(double a, double b, double t) { return a + (b - a) * t; }

double gr_ease(int curva, double t)
{
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    switch (curva) {
    case GR_EASE_SALIDA: { double u = 1 - t; return 1 - u * u * u; }
    case GR_EASE_ENTRADA: return t * t * t;
    case GR_EASE_AMBAS: return t < 0.5 ? 4 * t * t * t : 1 - (-2 * t + 2) * (-2 * t + 2) * (-2 * t + 2) / 2;
    default: return t;
    }
}

/* ------------------------------------------------------------ escalas */

/* El menor paso «bonito» (1, 2, 5 por una potencia de 10) que sea >= x. */
static double paso_bonito(double x)
{
    int e = 0;
    double m = x;
    while (m >= 10) { m /= 10; e++; }
    while (m < 1) { m *= 10; e--; }
    double f = m <= 1.0000001 ? 1 : m <= 2.0000001 ? 2 : m <= 5.0000001 ? 5 : 10;
    return f * gr_p10(e);
}

static double paso_siguiente(double p)
{
    int e = 0;
    double m = p;
    while (m >= 10) { m /= 10; e++; }
    while (m < 1) { m *= 10; e--; }
    double f = m < 1.5 ? 2 : m < 3.5 ? 5 : 10;
    return f * gr_p10(e);
}

void gr_escala(double lo, double hi, int max_marcas, int cero, gr_escala_t *e)
{
    if (hi < lo) { double t = lo; lo = hi; hi = t; }
    if (cero) { if (lo > 0) lo = 0; if (hi < 0) hi = 0; }
    if (hi - lo < 1e-12 * (gr_abs(hi) + 1e-300)) {
        double d = lo == 0 ? 1 : gr_abs(lo) * 0.1;
        lo -= d; hi += d;
        if (cero && lo < 0 && hi > 0 && d == 1) lo = 0;
    }
    if (max_marcas < 3) max_marcas = 3;   /* un rango que cruza el cero necesita al menos tres marcas */
    double paso = paso_bonito((hi - lo) / (max_marcas - 1));
    double mn, mx;
    for (int it = 0; it < 4; it++) {
        mn = gr_floor(lo / paso + 1e-9) * paso;
        mx = gr_ceil(hi / paso - 1e-9) * paso;
        int n = (int)((mx - mn) / paso + 0.5) + 1;
        if (n <= max_marcas) break;
        paso = paso_siguiente(paso);
    }
    mn = gr_floor(lo / paso + 1e-9) * paso;
    mx = gr_ceil(hi / paso - 1e-9) * paso;
    if (mx <= mn) mx = mn + paso;
    e->min = mn;
    e->max = mx;
    e->paso = paso;
    e->n = (int)((mx - mn) / paso + 0.5) + 1;
}

/* ------------------------------------------------------------ formato de números */

void gr_fmt_parse(const char *spec, gr_fmt_t *f)
{
    f->decimales = -1; f->miles = 0; f->pct = 0; f->dec = ','; f->mil = '.';
    f->prefijo[0] = 0; f->sufijo[0] = 0;
    if (!spec) return;
    const char *p = spec;
    /* «auto» a secas */
    if (p[0] == 'a' && p[1] == 'u' && p[2] == 't' && p[3] == 'o' && !p[4]) return;
    int n = 0;
    while (*p && *p != '0' && *p != '#' && *p != ',' && *p != '.') {
        if (n < 7) f->prefijo[n++] = *p;
        p++;
    }
    f->prefijo[n] = 0;
    int vio = 0, punto = 0, dec = 0;
    while (*p == '0' || *p == '#' || *p == ',' || *p == '.') {
        vio = 1;
        if (*p == ',') f->miles = 1;
        else if (*p == '.') punto = 1;
        else if (punto) dec++;
        p++;
    }
    if (vio) f->decimales = dec;
    n = 0;
    while (*p) {
        if (*p == '%') f->pct = 1;
        if (n < 7) f->sufijo[n++] = *p;
        p++;
    }
    f->sufijo[n] = 0;
    if (!vio) {                    /* sin patrón numérico («%», « kg»): todo es sufijo */
        for (n = 0; f->prefijo[n]; n++) { f->sufijo[n] = f->prefijo[n]; if (f->prefijo[n] == '%') f->pct = 1; }
        f->sufijo[n] = 0;
        f->prefijo[0] = 0;
    }
}

static void pon(char *b, int max, int *n, char c) { if (*n < max - 1) b[(*n)++] = c; }

int gr_fmt_num(const gr_fmt_t *f, double v, char *buf, int max)
{
    int n = 0;
    double a = v;
    if (f->pct) a *= 100;
    int dec = f->decimales;
    double aa = gr_abs(a);
    if (dec < 0) {                 /* automático: los decimales necesarios, hasta 3 */
        for (dec = 0; dec < 3; dec++) {
            double q = aa * gr_p10(dec);
            double d = q - gr_floor(q + 0.5);
            if (gr_abs(d) < 1e-7 * (q > 1 ? q : 1)) break;
        }
    }
    double p = gr_p10(dec);
    if (f->decimales < 0 && gr_floor(aa * p + 0.5) < 1) { dec = 0; p = 1; }   /* ni con tres decimales se ve: «0» */
    double S = gr_floor(aa * p + 0.5);
    double ip = gr_floor(S / p);
    double fr = S - ip * p;
    int neg = v < 0 && S > 0;
    for (const char *s = f->prefijo; *s; s++) pon(buf, max, &n, *s);
    if (neg) pon(buf, max, &n, '-');
    char t[40];
    int k = 0;
    if (ip < 1) t[k++] = '0';
    int dig = 0;
    while (ip >= 1 && k < 36) {
        double q = gr_floor(ip / 10);
        int d = (int)(ip - q * 10 + 0.5);
        if (f->miles && dig && dig % 3 == 0) t[k++] = f->mil;
        t[k++] = (char)('0' + d);
        dig++;
        ip = q;
    }
    while (k > 0) pon(buf, max, &n, t[--k]);
    if (dec > 0) {
        pon(buf, max, &n, f->dec);
        char d[8];
        for (int i = dec - 1; i >= 0; i--) {
            double q = gr_floor(fr / 10);
            d[i < 8 ? i : 7] = (char)('0' + (int)(fr - q * 10 + 0.5));
            fr = q;
        }
        for (int i = 0; i < dec && i < 8; i++) pon(buf, max, &n, d[i]);
    }
    for (const char *s = f->sufijo; *s; s++) pon(buf, max, &n, *s);
    if (max > 0) buf[n] = 0;
    return n;
}

/* ------------------------------------------------------------ apilado, barras, histograma */

void gr_apilar(const double *const *ys, const int *np, int ns, unsigned oculto, int n, double *base, double *cima)
{
    for (int i = 0; i < n; i++) {
        double pos = 0, neg = 0;
        for (int s = 0; s < ns; s++) {
            double v = (ys[s] && i < np[s]) ? ys[s][i] : 0;
            double *b = &base[s * n + i], *c = &cima[s * n + i];
            if (s < 32 && (oculto >> s & 1)) { *b = *c = pos; continue; }
            if (v >= 0) { *b = pos; pos += v; *c = pos; }
            else { *b = neg; neg += v; *c = neg; }
        }
    }
}

void gr_barra_geom(double ini, double total, int n, int i, int nvis, int k, int apilado, double hueco, double *x, double *w)
{
    if (n < 1) n = 1;
    double slot = total / n;
    if (hueco < 0) hueco = 0;
    if (hueco > 0.9) hueco = 0.9;
    double inner = slot * (1 - hueco);
    double marg = (slot - inner) / 2;
    if (apilado || nvis <= 1) { *x = ini + i * slot + marg; *w = inner; return; }
    double bw = inner / nvis;
    *x = ini + i * slot + marg + k * bw;
    *w = bw;
}

int gr_bins(const double *v, int n, int nb, double *lo, double *paso, int *cuentas, int max_bins)
{
    if (n < 1) { *lo = 0; *paso = 1; return 0; }
    double mn = v[0], mx = v[0];
    for (int i = 1; i < n; i++) { if (v[i] < mn) mn = v[i]; if (v[i] > mx) mx = v[i]; }
    if (nb <= 0) { nb = 1; for (int m = 1; m < n; m *= 2) nb++; }   /* Sturges: 1 + log2(n) */
    if (nb > max_bins) nb = max_bins;
    if (nb < 1) nb = 1;
    double p = (mx - mn) / nb;
    if (p <= 0) { p = 1; mn -= 0.5 * nb; }
    for (int j = 0; j < nb; j++) cuentas[j] = 0;
    for (int i = 0; i < n; i++) {
        int j = (int)gr_floor((v[i] - mn) / p);
        if (j >= nb) j = nb - 1;
        if (j < 0) j = 0;
        cuentas[j]++;
    }
    *lo = mn;
    *paso = p;
    return nb;
}

double gr_catmull(double p0, double p1, double p2, double p3, double t)
{
    double t2 = t * t, t3 = t2 * t;
    return 0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3);
}

/* ------------------------------------------------------------ colisiones */

int gr_en_rect(double x, double y, double rx, double ry, double rw, double rh)
{
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

int gr_en_circulo(double x, double y, double cx, double cy, double r)
{
    double dx = x - cx, dy = y - cy;
    return dx * dx + dy * dy <= r * r;
}

int gr_en_sector(double x, double y, double cx, double cy, double r0, double r1, double a0, double a1)
{
    double dx = x - cx, dy = y - cy;
    double d2 = dx * dx + dy * dy;
    if (d2 < r0 * r0 || d2 > r1 * r1) return 0;
    double span = a1 - a0;
    if (span <= 0) return 0;
    if (span >= PI2 - 1e-9) return 1;
    double x0, y0, x1, y1;
    gr_dir(a0, &x0, &y0);
    gr_dir(a1, &x1, &y1);
    double c0 = x0 * dy - y0 * dx;   /* >= 0: el punto está después de a0 (en sentido horario) */
    double c1 = x1 * dy - y1 * dx;   /* <= 0: antes de a1 */
    if (span <= PI) return c0 >= 0 && c1 <= 0;
    return !(c1 >= 0 && c0 <= 0);
}

/* ------------------------------------------------------------ paleta */

/* Categórica, pensada para distinguirse también con daltonismo (azul, naranja, verde, rojo teja, violeta...). */
static const unsigned pal_clara[12] = {
    0x2A6FD8, 0xE5832B, 0x1E9E6B, 0xC8453C, 0x8E5BC4, 0x2AA3B5, 0xB8A100, 0xD6609A, 0x6B7A88, 0x7A9E2A, 0x3F4FBF, 0xA0522D
};
static const unsigned pal_oscura[12] = {
    0x6EA0FF, 0xFFA85C, 0x4FD1A0, 0xFF7A70, 0xB794F0, 0x58CDE0, 0xE6D24A, 0xFF8FC0, 0xA3B1BF, 0xA8D45A, 0x8C9BFF, 0xD98A60
};

unsigned gr_color_serie(int i, int oscuro)
{
    if (i < 0) i = 0;
    return (oscuro ? pal_oscura : pal_clara)[i % 12];
}
