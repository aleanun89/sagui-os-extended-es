// SPDX-License-Identifier: GPL-3.0-or-later
// PlanoMini, © 2026 Alejandro Anuncibay: prototipo original (C++/SDL) del mantenedor; versión nativa de SaguiOS.
/* pm_geo.c - geometría y entidades de PlanoMini (D312): piezas (segmentos y arcos), intersecciones,
 * dibujo reducido a primitivas, transformaciones, selección, pinzamientos y referencia a objetos. */
#include "pm.h"

pm_ent_t pm_ent[PM_ENT_MAX];
int pm_n;
pm_capa_t pm_capa[PM_CAPAS_MAX];
int pm_ncapa, pm_capa_act;
int32_t pm_color_act = COL_CAPA;
int pm_ltipo_act = LT_CAPA;
double pm_cota_alt = 2.5, pm_cota_flecha = 2.5;
int pm_cota_dec = 2;
int pm_rejilla = 1, pm_orto, pm_forzc, pm_polar, pm_refent = 1, pm_refmask = REF_FIN | REF_MEDIO | REF_CENTRO | REF_INTER | REF_PERP | REF_CUAD;
double pm_paso_rej = 1.0, pm_polar_ang = 45.0;
double pm_vcx, pm_vcy, pm_ve = 20.0;
int pm_vw = 800, pm_vh = 500;
char pm_erro[120];
const char *const pm_ltipo_nombre[LT_N] = { "Continua", "Trazos", "Puntos", "Trazo y punto" };

static int fallo(const char *t) { copia_texto(pm_erro, t, sizeof pm_erro); return -1; }

/* ============================================================ utilidades */

int pm_fmt(double v, int dec, char *s, int max)
{
    if (max < 2) return 0;
    if (!gm_fin(v)) { copia_texto(s, "?", (size_t)max); return 1; }
    if (dec < 0) dec = 0;
    if (dec > 9) dec = 9;
    double p = 1;
    for (int i = 0; i < dec; i++) p *= 10;
    bool neg = v < 0;
    if (neg) v = -v;
    if (v > 1e14) v = 1e14;
    double r = gm_floor(v * p + 0.5), ip = gm_floor(r / p), fp = r - ip * p;
    if (r == 0) neg = false;
    char tmp[48];
    int n = 0;
    char fr[12];
    int nf = 0;
    for (int i = 0; i < dec; i++) { fr[nf++] = (char)('0' + (int)gm_fmod(fp, 10)); fp = gm_floor(fp / 10); }
    while (nf > 0 && fr[0] == '0') { memmove(fr, fr + 1, (size_t)(nf - 1)); nf--; }   /* los ceros finales están al principio (orden inverso) */
    char en[24];
    int ne = 0;
    if (ip == 0) en[ne++] = '0';
    while (ip > 0) { en[ne++] = (char)('0' + (int)gm_fmod(ip, 10)); ip = gm_floor(ip / 10); }
    if (neg) tmp[n++] = '-';
    while (ne) tmp[n++] = en[--ne];
    if (nf) { tmp[n++] = '.'; while (nf) tmp[n++] = fr[--nf]; }
    tmp[n] = 0;
    copia_texto(s, tmp, (size_t)max);
    return n;
}

double pm_atof(const char *s, const char **fim)
{
    const char *q = s;
    while (*q == ' ' || *q == '\t') q++;
    bool neg = false;
    if (*q == '-') { neg = true; q++; } else if (*q == '+') q++;
    double v = 0;
    bool alguno = false;
    while (*q >= '0' && *q <= '9') { v = v * 10 + (*q - '0'); q++; alguno = true; }
    if (*q == '.') {
        q++;
        double f = 0.1;
        while (*q >= '0' && *q <= '9') { v += (*q - '0') * f; f /= 10; q++; alguno = true; }
    }
    if (!alguno) { if (fim) *fim = s; return 0; }
    if ((*q == 'e' || *q == 'E') && (q[1] == '-' || q[1] == '+' || (q[1] >= '0' && q[1] <= '9'))) {
        const char *e = q + 1;
        bool en = false;
        if (*e == '-') { en = true; e++; } else if (*e == '+') e++;
        if (*e >= '0' && *e <= '9') {
            int ex = 0;
            while (*e >= '0' && *e <= '9') { ex = ex * 10 + (*e - '0'); if (ex > 400) ex = 400; e++; }
            double m = 1;
            for (int i = 0; i < ex; i++) m *= 10;
            v = en ? v / m : v * m;
            q = e;
        }
    }
    if (fim) *fim = q;
    return neg ? -v : v;
}

double pm_angnorm(double a)
{
    if (!gm_fin(a)) return 0;
    a = gm_fmod(a, 2 * PM_PI);
    if (a < 0) a += 2 * PM_PI;
    if (a >= 2 * PM_PI) a = 0;
    return a;
}
double pm_ang(double dx, double dy) { return pm_angnorm(gm_atan2(dy, dx)); }
bool pm_ang_en(double a, double a0, double a1)
{
    double d = pm_angnorm(a - a0);
    if (d > 2 * PM_PI - 1e-9) d = 0;
    return d <= (a1 - a0) + 1e-9;
}
double pm_dist(double x0, double y0, double x1, double y1) { return gm_hypot(x1 - x0, y1 - y0); }

/* ============================================================ piezas */

static void pz_seg(pm_pz_t *p, double x0, double y0, double x1, double y1)
{
    memset(p, 0, sizeof *p);
    p->x0 = x0; p->y0 = y0; p->x1 = x1; p->y1 = y1;
}
static void pz_arco(pm_pz_t *p, double cx, double cy, double r, double a0, double sweep)
{
    memset(p, 0, sizeof *p);
    p->arco = 1; p->cx = cx; p->cy = cy; p->r = r;
    p->a0 = pm_angnorm(a0);
    if (sweep < 1e-12) sweep = 2 * PM_PI;
    p->a1 = p->a0 + sweep;
    p->x0 = cx + r * gm_cos(p->a0); p->y0 = cy + r * gm_sin(p->a0);
    p->x1 = cx + r * gm_cos(p->a1); p->y1 = cy + r * gm_sin(p->a1);
}

void pm_bulge_arco(double x0, double y0, double x1, double y1, double b, pm_pz_t *p)
{
    double dx = x1 - x0, dy = y1 - y0, d = gm_hypot(dx, dy);
    if (d < PM_EPS || gm_abs(b) < 1e-12) { pz_seg(p, x0, y0, x1, y1); return; }
    double th = 4 * gm_atan(b);
    double h = d / (2 * gm_tan(th / 2)), r = gm_abs(d / (2 * gm_sin(th / 2)));
    double cx = (x0 + x1) / 2 + (-dy / d) * h, cy = (y0 + y1) / 2 + (dx / d) * h;
    double as = pm_ang(x0 - cx, y0 - cy), ae = pm_ang(x1 - cx, y1 - cy);
    if (b > 0) pz_arco(p, cx, cy, r, as, gm_abs(th));
    else { pz_arco(p, cx, cy, r, ae, gm_abs(th)); p->inv = 1; }
}

double pm_bulge_de(double as, double ae, bool horario)
{
    double sw = horario ? pm_angnorm(as - ae) : pm_angnorm(ae - as);
    double b = gm_tan(sw / 4);
    return horario ? -b : b;
}

int pm_piezas(const pm_ent_t *e, pm_pz_t *pz, int max)
{
    int n = 0;
    if (max < 1) return 0;
    switch (e->tipo) {
    case E_LINEA: pz_seg(&pz[0], e->p[0], e->p[1], e->p[2], e->p[3]); n = 1; break;
    case E_CIRCULO: pz_arco(&pz[0], e->p[0], e->p[1], e->r, 0, 2 * PM_PI); n = 1; break;
    case E_ARCO: pz_arco(&pz[0], e->p[0], e->p[1], e->r, e->a0, pm_angnorm(e->a1 - e->a0)); n = 1; break;
    case E_POLI:
        for (int i = 0; i + 1 < e->nv + (e->cerrada ? 1 : 0) && n < max; i++) {
            const pm_vert_t *a = &e->v[i], *b = &e->v[(i + 1) % e->nv];
            if (e->nv < 2) break;
            pm_bulge_arco(a->x, a->y, b->x, b->y, a->b, &pz[n++]);
        }
        break;
    }
    return n;
}

/* ------------------------------------------------------------ intersecciones */

static int inter_ss(const pm_pz_t *a, const pm_pz_t *b, bool ea, bool eb, double (*o)[2], int max)
{
    double d1x = a->x1 - a->x0, d1y = a->y1 - a->y0, d2x = b->x1 - b->x0, d2y = b->y1 - b->y0;
    double den = d1x * d2y - d1y * d2x, l1 = gm_hypot(d1x, d1y), l2 = gm_hypot(d2x, d2y);
    if (max < 1 || l1 < PM_EPS || l2 < PM_EPS || gm_abs(den) < 1e-12 * l1 * l2) return 0;
    double ex = b->x0 - a->x0, ey = b->y0 - a->y0;
    double t = (ex * d2y - ey * d2x) / den, u = (ex * d1y - ey * d1x) / den;
    double e1 = 1e-9, e2 = 1e-9;
    if (!ea && (t < -e1 || t > 1 + e1)) return 0;
    if (!eb && (u < -e2 || u > 1 + e2)) return 0;
    o[0][0] = a->x0 + t * d1x; o[0][1] = a->y0 + t * d1y;
    return 1;
}

static int inter_sa(const pm_pz_t *s, const pm_pz_t *a, bool es, bool ea, double (*o)[2], int max)
{
    double dx = s->x1 - s->x0, dy = s->y1 - s->y0, fx = s->x0 - a->cx, fy = s->y0 - a->cy;
    double A = dx * dx + dy * dy, B = 2 * (dx * fx + dy * fy), C = fx * fx + fy * fy - a->r * a->r;
    if (A < PM_EPS) return 0;
    double disc = B * B - 4 * A * C, tol = 1e-12 * (B * B + 4 * A * gm_abs(C) + 1e-30);
    if (disc < -tol) return 0;
    if (disc < 0) disc = 0;
    double sq = gm_sqrt(disc), ts[2] = { (-B - sq) / (2 * A), (-B + sq) / (2 * A) };
    int nt = sq < 1e-9 * gm_sqrt(A) ? 1 : 2, n = 0;
    for (int i = 0; i < nt && n < max; i++) {
        double t = ts[i];
        if (!es && (t < -1e-9 || t > 1 + 1e-9)) continue;
        double x = s->x0 + t * dx, y = s->y0 + t * dy;
        if (!ea && !pm_ang_en(pm_ang(x - a->cx, y - a->cy), a->a0, a->a1)) continue;
        o[n][0] = x; o[n][1] = y; n++;
    }
    return n;
}

static int inter_aa(const pm_pz_t *a, const pm_pz_t *b, bool ea, bool eb, double (*o)[2], int max)
{
    double dx = b->cx - a->cx, dy = b->cy - a->cy, d = gm_hypot(dx, dy);
    if (d < PM_EPS) return 0;
    if (d > a->r + b->r + 1e-9 || d < gm_abs(a->r - b->r) - 1e-9) return 0;
    double aa = (a->r * a->r - b->r * b->r + d * d) / (2 * d), h2 = a->r * a->r - aa * aa, h = h2 > 0 ? gm_sqrt(h2) : 0;
    double bx = a->cx + aa * dx / d, by = a->cy + aa * dy / d;
    int n = 0, nc = h < 1e-9 * (a->r + 1) ? 1 : 2;
    for (int i = 0; i < nc && n < max; i++) {
        double sg = i ? -1 : 1, x = bx + sg * h * (-dy) / d, y = by + sg * h * dx / d;
        if (!ea && !pm_ang_en(pm_ang(x - a->cx, y - a->cy), a->a0, a->a1)) continue;
        if (!eb && !pm_ang_en(pm_ang(x - b->cx, y - b->cy), b->a0, b->a1)) continue;
        o[n][0] = x; o[n][1] = y; n++;
    }
    return n;
}

int pm_pz_inter(const pm_pz_t *a, const pm_pz_t *b, bool ea, bool eb, double (*o)[2], int max)
{
    if (!a->arco && !b->arco) return inter_ss(a, b, ea, eb, o, max);
    if (!a->arco && b->arco) return inter_sa(a, b, ea, eb, o, max);
    if (a->arco && !b->arco) return inter_sa(b, a, eb, ea, o, max);
    return inter_aa(a, b, ea, eb, o, max);
}

double pm_pz_dist(const pm_pz_t *p, double x, double y, double *qx, double *qy)
{
    double ax, ay;
    if (!p->arco) {
        double dx = p->x1 - p->x0, dy = p->y1 - p->y0, l2 = dx * dx + dy * dy;
        double t = l2 < PM_EPS ? 0 : ((x - p->x0) * dx + (y - p->y0) * dy) / l2;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        ax = p->x0 + t * dx; ay = p->y0 + t * dy;
    } else {
        double a = pm_ang(x - p->cx, y - p->cy);
        if (pm_ang_en(a, p->a0, p->a1) && gm_hypot(x - p->cx, y - p->cy) > 1e-12) { ax = p->cx + p->r * gm_cos(a); ay = p->cy + p->r * gm_sin(a); }
        else {
            double d0 = gm_hypot(x - p->x0, y - p->y0), d1 = gm_hypot(x - p->x1, y - p->y1);
            if (d0 <= d1) { ax = p->x0; ay = p->y0; } else { ax = p->x1; ay = p->y1; }
        }
    }
    if (qx) *qx = ax;
    if (qy) *qy = ay;
    return gm_hypot(x - ax, y - ay);
}

double pm_pz_param(const pm_pz_t *p, double x, double y)
{
    if (p->arco) return pm_ang(x - p->cx, y - p->cy);
    double dx = p->x1 - p->x0, dy = p->y1 - p->y0, l2 = dx * dx + dy * dy;
    return l2 < PM_EPS ? 0 : ((x - p->x0) * dx + (y - p->y0) * dy) / l2;
}

/* ============================================================ dibujo: toda entidad se reduce a primitivas */

double pm_txt_ancho(double alt, const char *t) { return 0.67 * alt * (double)tam_texto(t); }

static void emit_arco(const pm_sink_t *s, double cx, double cy, double r, double a0, double sweep)
{
    double tol = s->tol > 0 ? s->tol : 0.01;
    int n = 8;
    if (r > tol) {
        double dphi = 2 * gm_acos(1 - tol / r);
        n = dphi > 1e-9 ? (int)gm_ceil(sweep / dphi) : 400;
    }
    int minimo = (int)gm_ceil(sweep / (PM_PI / 12));
    if (n < minimo) n = minimo;
    if (n < 2) n = 2;
    if (n > 400) n = 400;
    double px = cx + r * gm_cos(a0), py = cy + r * gm_sin(a0);
    for (int i = 1; i <= n; i++) {
        double a = a0 + sweep * i / n, x = cx + r * gm_cos(a), y = cy + r * gm_sin(a);
        s->seg(s->c, px, py, x, y);
        px = x; py = y;
    }
}

static void emit_pieza(const pm_sink_t *s, const pm_pz_t *p)
{
    if (!p->arco) s->seg(s->c, p->x0, p->y0, p->x1, p->y1);
    else emit_arco(s, p->cx, p->cy, p->r, p->a0, p->a1 - p->a0);
}

int pm_aplanar(const pm_ent_t *e, double *xs, double *ys, int max)
{
    pm_pz_t pz[PM_VERT_MAX + 2];
    int np = pm_piezas(e, pz, PM_VERT_MAX + 2), n = 0;
    if (e->tipo == E_SOMBRA && e->nv >= 2) {    /* el contorno de una sombra es una polilínea cerrada */
        pm_ent_t t = *e;
        t.tipo = E_POLI; t.cerrada = 1;
        np = pm_piezas(&t, pz, PM_VERT_MAX + 2);
    }
    for (int i = 0; i < np; i++) {
        const pm_pz_t *p = &pz[i];
        if (!p->arco) { if (n < max) { xs[n] = p->x0; ys[n] = p->y0; n++; } continue; }
        int k = (int)gm_ceil((p->a1 - p->a0) / (PM_PI / 32));
        if (k < 1) k = 1;
        for (int q = 0; q < k && n < max; q++) {
            double a = p->inv ? p->a1 - (p->a1 - p->a0) * q / k : p->a0 + (p->a1 - p->a0) * q / k;
            xs[n] = p->cx + p->r * gm_cos(a); ys[n] = p->cy + p->r * gm_sin(a); n++;
        }
    }
    /* las piezas se recorren en el orden del contorno: si un arco va al revés, su último punto es el primero del siguiente */
    return n;
}

static void flecha(const pm_sink_t *s, double tx, double ty, double dx, double dy, double len)
{
    double w = len / 6;
    s->tri(s->c, tx, ty, tx + dx * len - dy * w, ty + dy * len + dx * w, tx + dx * len + dy * w, ty + dy * len - dx * w);
}

/* Texto sobre una línea de cota: legible (giro en (-90, 90]) y encima de la línea. */
static void texto_sobre(const pm_sink_t *s, double mx, double my, double dx, double dy, double alt, const char *t)
{
    double phi = gm_atan2(dy, dx);
    if (phi > PM_PI / 2 + 1e-9) phi -= PM_PI;
    if (phi <= -PM_PI / 2 + 1e-9) phi += PM_PI;
    s->txt(s->c, mx - gm_sin(phi) * 0.45 * alt, my + gm_cos(phi) * 0.45 * alt, alt, phi, t, 1);
}

double pm_cota_valor(const pm_ent_t *e)
{
    switch (e->sub) {
    case C_LIN_H: return gm_abs(e->p[2] - e->p[0]);
    case C_LIN_V: return gm_abs(e->p[3] - e->p[1]);
    case C_ALINEADA: return gm_hypot(e->p[2] - e->p[0], e->p[3] - e->p[1]);
    case C_RADIO: return e->r;
    case C_DIAM: return 2 * e->r;
    case C_ANGULAR: {
        double as = pm_ang(e->p[2] - e->p[0], e->p[3] - e->p[1]), ae = pm_ang(e->p[4] - e->p[0], e->p[5] - e->p[1]);
        double al = pm_ang(e->p[6] - e->p[0], e->p[7] - e->p[1]), sw = pm_angnorm(ae - as);
        if (!pm_ang_en(al, as, as + sw)) sw = pm_angnorm(as - ae);
        return sw * 180.0 / PM_PI;
    }
    }
    return 0;
}

void pm_cota_texto(const pm_ent_t *e, char *s, int max)
{
    char v[32];
    pm_fmt(pm_cota_valor(e), pm_cota_dec, v, sizeof v);
    char med[40];
    med[0] = 0;
    if (e->sub == C_RADIO) { med[0] = 'R'; med[1] = 0; }
    else if (e->sub == C_DIAM) { med[0] = (char)0xD8; med[1] = 0; }
    int k = (int)tam_texto(med);
    copia_texto(med + k, v, sizeof med - (size_t)k);
    if (e->sub == C_ANGULAR) { k = (int)tam_texto(med); med[k] = (char)0xB0; med[k + 1] = 0; }
    if (!e->txt[0]) { copia_texto(s, med, (size_t)max); return; }
    int n = 0;
    for (const char *q = e->txt; *q && n + 1 < max; q++) {
        if (q[0] == '<' && q[1] == '>') {
            for (const char *m = med; *m && n + 1 < max; m++) s[n++] = *m;
            q++;
        } else s[n++] = *q;
    }
    s[n] = 0;
}

static void cota_lineal(const pm_ent_t *e, const pm_sink_t *s)
{
    double ax = e->p[0], ay = e->p[1], bx = e->p[2], by = e->p[3], lx = e->p[4], ly = e->p[5];
    double alt = e->alt > 0 ? e->alt : pm_cota_alt, fl = pm_cota_flecha > 0 ? pm_cota_flecha : alt;
    double ux, uy;                /* dirección de la línea de cota */
    double nx, ny;                /* de las referencias hacia la línea de cota */
    if (e->sub == C_LIN_H) { ux = bx >= ax ? 1 : -1; uy = 0; nx = 0; ny = 1; }
    else if (e->sub == C_LIN_V) { ux = 0; uy = by >= ay ? 1 : -1; nx = 1; ny = 0; }
    else {
        double d = gm_hypot(bx - ax, by - ay);
        if (d < PM_EPS) return;
        ux = (bx - ax) / d; uy = (by - ay) / d; nx = -uy; ny = ux;
    }
    double off = (lx - ax) * nx + (ly - ay) * ny;       /* distancia de la línea de cota a la primera referencia */
    double offb = (lx - bx) * nx + (ly - by) * ny;
    /* extremos de la línea de cota */
    double p1x = ax + nx * off, p1y = ay + ny * off, p2x = bx + nx * offb, p2y = by + ny * offb;
    double g = alt * 0.3, ov = alt * 0.5;
    double sa = off >= 0 ? 1 : -1, sb = offb >= 0 ? 1 : -1;
    s->seg(s->c, ax + nx * sa * g, ay + ny * sa * g, p1x + nx * sa * ov, p1y + ny * sa * ov);
    s->seg(s->c, bx + nx * sb * g, by + ny * sb * g, p2x + nx * sb * ov, p2y + ny * sb * ov);
    double L = gm_hypot(p2x - p1x, p2y - p1y);
    if (L < PM_EPS) return;
    double dx = (p2x - p1x) / L, dy = (p2y - p1y) / L;
    if (L >= 2 * fl) {
        s->seg(s->c, p1x, p1y, p2x, p2y);
        flecha(s, p1x, p1y, dx, dy, fl);
        flecha(s, p2x, p2y, -dx, -dy, fl);
    } else {                                    /* cota corta: flechas por fuera */
        s->seg(s->c, p1x - dx * fl, p1y - dy * fl, p2x + dx * fl, p2y + dy * fl);
        flecha(s, p1x, p1y, -dx, -dy, fl);
        flecha(s, p2x, p2y, dx, dy, fl);
    }
    char t[80];
    pm_cota_texto(e, t, sizeof t);
    texto_sobre(s, (p1x + p2x) / 2, (p1y + p2y) / 2, dx, dy, alt, t);
}

static void cota_radial(const pm_ent_t *e, const pm_sink_t *s)
{
    double cx = e->p[0], cy = e->p[1], r = e->r;
    double alt = e->alt > 0 ? e->alt : pm_cota_alt, fl = pm_cota_flecha > 0 ? pm_cota_flecha : alt;
    double ux = gm_cos(e->a0), uy = gm_sin(e->a0), px = cx + ux * r, py = cy + uy * r;
    char t[80];
    pm_cota_texto(e, t, sizeof t);
    double w = pm_txt_ancho(alt, t);
    if (e->sub == C_DIAM) { s->seg(s->c, cx - ux * r, cy - uy * r, px, py); flecha(s, cx - ux * r, cy - uy * r, ux, uy, fl); }
    else s->seg(s->c, cx, cy, px, py);
    flecha(s, px, py, -ux, -uy, fl);
    double d = fl * 0.5 + w * 0.5 * gm_abs(ux) + alt * 0.7 * gm_abs(uy) + alt * 0.3;
    s->txt(s->c, px + ux * d, py + uy * d - alt * 0.5, alt, 0, t, 1);
}

static void cota_angular(const pm_ent_t *e, const pm_sink_t *s)
{
    double cx = e->p[0], cy = e->p[1];
    double alt = e->alt > 0 ? e->alt : pm_cota_alt, fl = pm_cota_flecha > 0 ? pm_cota_flecha : alt;
    double R = gm_hypot(e->p[6] - cx, e->p[7] - cy);
    if (R < PM_EPS) return;
    double as = pm_ang(e->p[2] - cx, e->p[3] - cy), ae = pm_ang(e->p[4] - cx, e->p[5] - cy);
    double al = pm_ang(e->p[6] - cx, e->p[7] - cy), sw = pm_angnorm(ae - as);
    if (!pm_ang_en(al, as, as + sw)) { double t = as; as = ae; ae = t; sw = pm_angnorm(ae - as); }
    emit_arco(s, cx, cy, R, as, sw);
    double sx = cx + R * gm_cos(as), sy = cy + R * gm_sin(as), ex = cx + R * gm_cos(ae), ey = cy + R * gm_sin(ae);
    flecha(s, sx, sy, -gm_sin(as), gm_cos(as), fl);
    flecha(s, ex, ey, gm_sin(ae), -gm_cos(ae), fl);
    double g = alt * 0.3;
    double da = gm_hypot(e->p[2] - cx, e->p[3] - cy), db = gm_hypot(e->p[4] - cx, e->p[5] - cy);
    if (R > da + g) s->seg(s->c, cx + (da + g) * gm_cos(as), cy + (da + g) * gm_sin(as), cx + (R + alt * 0.4) * gm_cos(as), cy + (R + alt * 0.4) * gm_sin(as));
    if (R > db + g) s->seg(s->c, cx + (db + g) * gm_cos(ae), cy + (db + g) * gm_sin(ae), cx + (R + alt * 0.4) * gm_cos(ae), cy + (R + alt * 0.4) * gm_sin(ae));
    char t[80];
    pm_cota_texto(e, t, sizeof t);
    double am = as + sw / 2, w = pm_txt_ancho(alt, t), d = R + alt * 0.6 + w * 0.5 * gm_abs(gm_cos(am)) + alt * 0.5 * gm_abs(gm_sin(am));
    s->txt(s->c, cx + d * gm_cos(am), cy + d * gm_sin(am) - alt * 0.5, alt, 0, t, 1);
}

/* Sombreado: líneas paralelas (ángulo a0, espaciado r) recortadas por el contorno. */
static void sombra_lineas(const pm_ent_t *e, const pm_sink_t *s)
{
    static double xs[800], ys[800];
    int n = pm_aplanar(e, xs, ys, 800);
    if (n < 3) return;
    double esp = e->r > 0 ? e->r : 1.0, ca = gm_cos(e->a0), sa = gm_sin(e->a0);
    double ymin = 1e300, ymax = -1e300;
    static double rx[800], ry[800];
    for (int i = 0; i < n; i++) {
        rx[i] = xs[i] * ca + ys[i] * sa; ry[i] = -xs[i] * sa + ys[i] * ca;
        if (ry[i] < ymin) ymin = ry[i];
        if (ry[i] > ymax) ymax = ry[i];
    }
    double y = gm_ceil(ymin / esp) * esp + esp * 1e-6;
    int lineas = 0;
    for (; y < ymax && lineas < 3000; y += esp, lineas++) {
        double xi[64];
        int k = 0;
        for (int i = 0; i < n && k < 62; i++) {
            int j = (i + 1) % n;
            if ((ry[i] <= y && ry[j] > y) || (ry[j] <= y && ry[i] > y)) xi[k++] = rx[i] + (y - ry[i]) / (ry[j] - ry[i]) * (rx[j] - rx[i]);
        }
        for (int a = 1; a < k; a++) { double v = xi[a]; int b = a - 1; while (b >= 0 && xi[b] > v) { xi[b + 1] = xi[b]; b--; } xi[b + 1] = v; }
        for (int a = 0; a + 1 < k; a += 2)
            s->seg(s->c, xi[a] * ca - y * sa, xi[a] * sa + y * ca, xi[a + 1] * ca - y * sa, xi[a + 1] * sa + y * ca);
    }
}

void pm_dibujar_ent(const pm_ent_t *e, const pm_sink_t *s)
{
    pm_pz_t pz[PM_VERT_MAX + 2];
    switch (e->tipo) {
    case E_PUNTO: s->seg(s->c, e->p[0], e->p[1], e->p[0], e->p[1]); break;
    case E_LINEA: s->seg(s->c, e->p[0], e->p[1], e->p[2], e->p[3]); break;
    case E_CIRCULO: emit_arco(s, e->p[0], e->p[1], e->r, 0, 2 * PM_PI); break;
    case E_ARCO: emit_arco(s, e->p[0], e->p[1], e->r, pm_angnorm(e->a0), pm_angnorm(e->a1 - e->a0) < 1e-12 ? 2 * PM_PI : pm_angnorm(e->a1 - e->a0)); break;
    case E_POLI: { int n = pm_piezas(e, pz, PM_VERT_MAX + 2); for (int i = 0; i < n; i++) emit_pieza(s, &pz[i]); break; }
    case E_TEXTO: s->txt(s->c, e->p[0], e->p[1], e->alt, e->giro, e->txt, 0); break;
    case E_COTA:
        if (e->sub == C_RADIO || e->sub == C_DIAM) cota_radial(e, s);
        else if (e->sub == C_ANGULAR) cota_angular(e, s);
        else cota_lineal(e, s);
        break;
    case E_SOMBRA:
        if (e->sub == 1) {
            static double xs[800], ys[800];
            int n = pm_aplanar(e, xs, ys, 800);
            if (n >= 3 && s->poli) s->poli(s->c, xs, ys, n);
        } else sombra_lineas(e, s);
        break;
    }
}

/* ------------------------------------------------------------ cajas y distancias (por las primitivas) */

typedef struct { double x0, y0, x1, y1; bool vacio; } caja_t;
static void caja_pt(caja_t *c, double x, double y)
{
    if (c->vacio) { c->x0 = c->x1 = x; c->y0 = c->y1 = y; c->vacio = false; return; }
    if (x < c->x0) c->x0 = x;
    if (x > c->x1) c->x1 = x;
    if (y < c->y0) c->y0 = y;
    if (y > c->y1) c->y1 = y;
}
static void bb_seg(void *c, double x0, double y0, double x1, double y1) { caja_pt(c, x0, y0); caja_pt(c, x1, y1); }
static void bb_tri(void *c, double a, double b, double d, double e, double f, double g) { caja_pt(c, a, b); caja_pt(c, d, e); caja_pt(c, f, g); }
static void bb_poli(void *c, const double *xs, const double *ys, int n) { for (int i = 0; i < n; i++) caja_pt(c, xs[i], ys[i]); }
static void txt_esquinas(double x, double y, double alt, double giro, const char *t, int cen, double (*q)[2])
{
    double w = pm_txt_ancho(alt, t), c = gm_cos(giro), s = gm_sin(giro), x0 = cen ? -w / 2 : 0, x1 = x0 + w, y0 = -alt * 0.25, y1 = alt * 1.05;
    double lx[4] = { x0, x1, x1, x0 }, ly[4] = { y0, y0, y1, y1 };
    for (int i = 0; i < 4; i++) { q[i][0] = x + lx[i] * c - ly[i] * s; q[i][1] = y + lx[i] * s + ly[i] * c; }
}
static void bb_txt(void *c, double x, double y, double alt, double giro, const char *t, int cen)
{
    double q[4][2];
    txt_esquinas(x, y, alt, giro, t, cen, q);
    for (int i = 0; i < 4; i++) caja_pt(c, q[i][0], q[i][1]);
}

bool pm_ent_bbox(const pm_ent_t *e, double *x0, double *y0, double *x1, double *y1)
{
    caja_t c = { 0, 0, 0, 0, true };
    pm_sink_t s = { bb_seg, bb_tri, bb_poli, bb_txt, &c, 0.002 };
    if (e->tipo == E_SOMBRA && e->sub == 0) { for (int i = 0; i < e->nv; i++) caja_pt(&c, e->v[i].x, e->v[i].y); }
    else pm_dibujar_ent(e, &s);
    if (c.vacio) return false;
    *x0 = c.x0; *y0 = c.y0; *x1 = c.x1; *y1 = c.y1;
    return true;
}

typedef struct { double x, y, d; } dctx_t;
static double txt_dist(double px, double py, double x, double y, double alt, double giro, const char *t, int cen)
{
    double w = pm_txt_ancho(alt, t), c = gm_cos(giro), s = gm_sin(giro), dx = px - x, dy = py - y;
    double lx = dx * c + dy * s, ly = -dx * s + dy * c, x0 = cen ? -w / 2 : 0, x1 = x0 + w, y0 = -alt * 0.25, y1 = alt * 1.05;
    double ex = lx < x0 ? x0 - lx : lx > x1 ? lx - x1 : 0, ey = ly < y0 ? y0 - ly : ly > y1 ? ly - y1 : 0;
    return gm_hypot(ex, ey);
}
static void d_seg(void *c, double x0, double y0, double x1, double y1)
{
    dctx_t *d = c;
    pm_pz_t p;
    memset(&p, 0, sizeof p);
    p.x0 = x0; p.y0 = y0; p.x1 = x1; p.y1 = y1;
    double v = pm_pz_dist(&p, d->x, d->y, NULL, NULL);
    if (v < d->d) d->d = v;
}
static void d_tri(void *c, double a, double b, double e, double f, double g, double h) { (void)c; (void)a; (void)b; (void)e; (void)f; (void)g; (void)h; }
static void d_txt(void *c, double x, double y, double alt, double giro, const char *t, int cen)
{
    dctx_t *d = c;
    double v = txt_dist(d->x, d->y, x, y, alt, giro, t, cen);
    if (v < d->d) d->d = v;
}

double pm_dist_ent(const pm_ent_t *e, double x, double y)
{
    if (e->tipo == E_PUNTO) return gm_hypot(x - e->p[0], y - e->p[1]);
    if (e->tipo == E_LINEA || e->tipo == E_CIRCULO || e->tipo == E_ARCO || e->tipo == E_POLI) {
        pm_pz_t pz[PM_VERT_MAX + 2];
        int n = pm_piezas(e, pz, PM_VERT_MAX + 2);
        double m = 1e300;
        for (int i = 0; i < n; i++) {
            double d = pm_pz_dist(&pz[i], x, y, NULL, NULL);
            if (d < m) m = d;
        }
        return m;
    }
    dctx_t d = { x, y, 1e300 };
    pm_sink_t s = { d_seg, d_tri, NULL, d_txt, &d, 0.01 };
    pm_dibujar_ent(e, &s);
    return d.d;
}

static bool seg_caja(double x0, double y0, double x1, double y1, double a, double b, double c, double d)
{
    double t0 = 0, t1 = 1, dx = x1 - x0, dy = y1 - y0;
    double p[4] = { -dx, dx, -dy, dy }, q[4] = { x0 - a, c - x0, y0 - b, d - y0 };
    for (int i = 0; i < 4; i++) {
        if (p[i] == 0) { if (q[i] < 0) return false; }
        else {
            double r = q[i] / p[i];
            if (p[i] < 0) { if (r > t1) return false; if (r > t0) t0 = r; }
            else { if (r < t0) return false; if (r < t1) t1 = r; }
        }
    }
    return true;
}
typedef struct { double a, b, c, d; bool toca; } wctx_t;
static void w_seg(void *c, double x0, double y0, double x1, double y1) { wctx_t *w = c; if (!w->toca && seg_caja(x0, y0, x1, y1, w->a, w->b, w->c, w->d)) w->toca = true; }
static void w_tri(void *c, double a, double b, double d, double e, double f, double g) { w_seg(c, a, b, d, e); w_seg(c, d, e, f, g); }
static void w_txt(void *c, double x, double y, double alt, double giro, const char *t, int cen)
{
    double q[4][2];
    txt_esquinas(x, y, alt, giro, t, cen, q);
    for (int i = 0; i < 4; i++) w_seg(c, q[i][0], q[i][1], q[(i + 1) % 4][0], q[(i + 1) % 4][1]);
}

bool pm_ent_en_ventana(const pm_ent_t *e, double x0, double y0, double x1, double y1, bool cruce)
{
    double bx0, by0, bx1, by1;
    if (!pm_ent_bbox(e, &bx0, &by0, &bx1, &by1)) return false;
    if (!cruce) return bx0 >= x0 - 1e-9 && bx1 <= x1 + 1e-9 && by0 >= y0 - 1e-9 && by1 <= y1 + 1e-9;
    if (bx1 < x0 || bx0 > x1 || by1 < y0 || by0 > y1) return false;
    wctx_t w = { x0, y0, x1, y1, false };
    pm_sink_t s = { w_seg, w_tri, NULL, w_txt, &w, 0.01 };
    pm_dibujar_ent(e, &s);
    return w.toca;
}

/* ============================================================ documento */

bool pm_ent_visible(const pm_ent_t *e) { return e->capa < pm_ncapa && pm_capa[e->capa].visible; }
bool pm_ent_editable(const pm_ent_t *e) { return pm_ent_visible(e) && !pm_capa[e->capa].bloq; }
uint32_t pm_ent_color(const pm_ent_t *e) { return e->color < 0 ? (e->capa < pm_ncapa ? pm_capa[e->capa].color : 0xFFFFFF) : (uint32_t)e->color; }
int pm_ent_ltipo(const pm_ent_t *e) { return e->ltipo == LT_CAPA ? (e->capa < pm_ncapa ? pm_capa[e->capa].ltipo : 0) : e->ltipo; }

void pm_ent_liberar(pm_ent_t *e) { if (e->v) libera(e->v); e->v = NULL; e->nv = 0; }
void pm_ent_clonar(pm_ent_t *d, const pm_ent_t *s)
{
    *d = *s;
    d->v = NULL;
    if (s->nv > 0 && s->v) {
        d->v = aloca((size_t)s->nv * sizeof(pm_vert_t));
        if (d->v) memcpy(d->v, s->v, (size_t)s->nv * sizeof(pm_vert_t)); else d->nv = 0;
    }
}
void pm_ent_nueva(pm_ent_t *e, int tipo)
{
    memset(e, 0, sizeof *e);
    e->tipo = (uint8_t)tipo; e->color = COL_CAPA; e->ltipo = LT_CAPA; e->alt = pm_cota_alt;
}
int pm_poli_vert(pm_ent_t *e, double x, double y, double b)
{
    if (e->nv >= PM_VERT_MAX) return -1;
    pm_vert_t *n = e->v ? realoca(e->v, (size_t)(e->nv + 1) * sizeof(pm_vert_t)) : aloca(sizeof(pm_vert_t));
    if (!n) return -1;
    e->v = n;
    e->v[e->nv].x = x; e->v[e->nv].y = y; e->v[e->nv].b = b;
    return e->nv++;
}
void pm_rect_poli(pm_ent_t *e, double x0, double y0, double x1, double y1)
{
    pm_poli_vert(e, x0, y0, 0); pm_poli_vert(e, x1, y0, 0); pm_poli_vert(e, x1, y1, 0); pm_poli_vert(e, x0, y1, 0);
    e->cerrada = 1;
}

void pm_limpiar(void)
{
    for (int i = 0; i < pm_n; i++) pm_ent_liberar(&pm_ent[i]);
    pm_n = 0;
    memset(pm_capa, 0, sizeof pm_capa);
    copia_texto(pm_capa[0].nome, "0", sizeof pm_capa[0].nome);
    pm_capa[0].color = 0xFFFFFF; pm_capa[0].visible = 1;
    pm_ncapa = 1; pm_capa_act = 0;
    pm_color_act = COL_CAPA; pm_ltipo_act = LT_CAPA;
}

int pm_anadir(const pm_ent_t *e)
{
    if (pm_n >= PM_ENT_MAX) { fallo("Demasiadas entidades"); return -1; }
    pm_ent_t *d = &pm_ent[pm_n];
    *d = *e;
    if (e->v && e->nv) { pm_ent_t t; pm_ent_clonar(&t, e); d->v = t.v; d->nv = t.nv; }
    d->sel = 0;
    return pm_n++;
}
/* Entidad nueva con las propiedades actuales: la usan las órdenes de dibujo. */
static int anadir_actual(pm_ent_t *e)
{
    e->capa = (uint8_t)pm_capa_act; e->color = pm_color_act; e->ltipo = (uint8_t)pm_ltipo_act;
    int r = pm_anadir(e);
    pm_ent_liberar(e);
    return r;
}
int pm_anadir_actual(pm_ent_t *e) { return anadir_actual(e); }

void pm_borrar(int i)
{
    if (i < 0 || i >= pm_n) return;
    pm_ent_liberar(&pm_ent[i]);
    memmove(&pm_ent[i], &pm_ent[i + 1], (size_t)(pm_n - i - 1) * sizeof(pm_ent_t));
    pm_n--;
    memset(&pm_ent[pm_n], 0, sizeof(pm_ent_t));
}

static bool igual_ci(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'a' && x <= 'z') x = (char)(x - 32);
        if (y >= 'a' && y <= 'z') y = (char)(y - 32);
        if (x != y) return false;
    }
    return *a == *b;
}
int pm_capa_buscar(const char *nome) { for (int i = 0; i < pm_ncapa; i++) if (igual_ci(pm_capa[i].nome, nome)) return i; return -1; }
int pm_capa_nueva(const char *nome)
{
    int i = pm_capa_buscar(nome);
    if (i >= 0) return i;
    if (pm_ncapa >= PM_CAPAS_MAX || !nome[0]) return -1;
    memset(&pm_capa[pm_ncapa], 0, sizeof pm_capa[0]);
    copia_texto(pm_capa[pm_ncapa].nome, nome, sizeof pm_capa[0].nome);
    pm_capa[pm_ncapa].color = 0xFFFFFF; pm_capa[pm_ncapa].visible = 1;
    return pm_ncapa++;
}

/* ============================================================ vista */

static double prev_v[8][3];
static int n_prev;
void pm_vista_guardar(void)
{
    if (n_prev == 8) { memmove(prev_v[0], prev_v[1], sizeof(double) * 3 * 7); n_prev--; }
    prev_v[n_prev][0] = pm_vcx; prev_v[n_prev][1] = pm_vcy; prev_v[n_prev][2] = pm_ve;
    n_prev++;
}
void pm_zoom_prev(void)
{
    if (!n_prev) return;
    n_prev--;
    pm_vcx = prev_v[n_prev][0]; pm_vcy = prev_v[n_prev][1]; pm_ve = prev_v[n_prev][2];
}
void pm_zoom_vent(double x0, double y0, double x1, double y1)
{
    double w = gm_abs(x1 - x0), h = gm_abs(y1 - y0);
    if (w < 1e-9) w = 1e-9;
    if (h < 1e-9) h = 1e-9;
    pm_vista_guardar();
    double e1 = pm_vw / w, e2 = pm_vh / h;
    pm_ve = e1 < e2 ? e1 : e2;
    pm_vcx = (x0 + x1) / 2; pm_vcy = (y0 + y1) / 2;
}
void pm_zoom_ext(void)
{
    double a = 1e300, b = 1e300, c = -1e300, d = -1e300;
    for (int i = 0; i < pm_n; i++) {
        double x0, y0, x1, y1;
        if (!pm_ent_visible(&pm_ent[i]) || !pm_ent_bbox(&pm_ent[i], &x0, &y0, &x1, &y1)) continue;
        if (x0 < a) a = x0;
        if (y0 < b) b = y0;
        if (x1 > c) c = x1;
        if (y1 > d) d = y1;
    }
    if (a > c) { pm_vista_guardar(); pm_vcx = 0; pm_vcy = 0; pm_ve = 20; return; }
    double mx = (c - a) * 0.05 + 0.5, my = (d - b) * 0.05 + 0.5;
    pm_zoom_vent(a - mx, b - my, c + mx, d + my);
}
void pm_zoom_factor(double f, double sx, double sy)
{
    if (f < 1e-6 || f > 1e6) return;
    double wx = pm_s2wx(sx), wy = pm_s2wy(sy);
    double ne = pm_ve * f;
    if (ne < 1e-4 || ne > 1e7) return;
    pm_ve = ne;
    pm_vcx = wx - (sx - pm_vw / 2.0) / pm_ve;
    pm_vcy = wy + (sy - pm_vh / 2.0) / pm_ve;
}

/* ============================================================ transformaciones */

void pm_mat_traslacion(double *m, double dx, double dy) { m[0] = 1; m[1] = 0; m[2] = dx; m[3] = 0; m[4] = 1; m[5] = dy; }
void pm_mat_giro(double *m, double cx, double cy, double a)
{
    double c = gm_cos(a), s = gm_sin(a);
    m[0] = c; m[1] = -s; m[2] = cx - c * cx + s * cy; m[3] = s; m[4] = c; m[5] = cy - s * cx - c * cy;
}
void pm_mat_escala(double *m, double cx, double cy, double f) { m[0] = f; m[1] = 0; m[2] = cx - f * cx; m[3] = 0; m[4] = f; m[5] = cy - f * cy; }
void pm_mat_simetria(double *m, double x0, double y0, double x1, double y1)
{
    double dx = x1 - x0, dy = y1 - y0, l = gm_hypot(dx, dy);
    if (l < PM_EPS) { pm_mat_traslacion(m, 0, 0); return; }
    dx /= l; dy /= l;
    double a = 2 * dx * dx - 1, b = 2 * dx * dy, d = 2 * dy * dy - 1;
    m[0] = a; m[1] = b; m[3] = b; m[4] = d;
    m[2] = x0 - a * x0 - b * y0; m[5] = y0 - b * x0 - d * y0;
}

static void mp(const double *m, double *x, double *y) { double a = *x, b = *y; *x = m[0] * a + m[1] * b + m[2]; *y = m[3] * a + m[4] * b + m[5]; }

void pm_ent_transformar(pm_ent_t *e, const double *m)
{
    double det = m[0] * m[4] - m[1] * m[3], s = gm_sqrt(gm_abs(det)), rot = gm_atan2(m[3], m[0]);
    bool esp = det < 0;
    switch (e->tipo) {
    case E_PUNTO: mp(m, &e->p[0], &e->p[1]); break;
    case E_LINEA: mp(m, &e->p[0], &e->p[1]); mp(m, &e->p[2], &e->p[3]); break;
    case E_CIRCULO: mp(m, &e->p[0], &e->p[1]); e->r *= s; break;
    case E_ARCO: {
        double sw = pm_angnorm(e->a1 - e->a0);
        if (sw < 1e-12) sw = 2 * PM_PI;
        double sx = e->p[0] + e->r * gm_cos(e->a0), sy = e->p[1] + e->r * gm_sin(e->a0);
        double ex = e->p[0] + e->r * gm_cos(e->a0 + sw), ey = e->p[1] + e->r * gm_sin(e->a0 + sw);
        mp(m, &e->p[0], &e->p[1]); mp(m, &sx, &sy); mp(m, &ex, &ey);
        e->r *= s;
        double as = pm_ang(sx - e->p[0], sy - e->p[1]), ae = pm_ang(ex - e->p[0], ey - e->p[1]);
        if (esp) { e->a0 = ae; e->a1 = as; } else { e->a0 = as; e->a1 = ae; }
        break;
    }
    case E_POLI: case E_SOMBRA:
        for (int i = 0; i < e->nv; i++) { mp(m, &e->v[i].x, &e->v[i].y); if (esp) e->v[i].b = -e->v[i].b; }
        if (e->tipo == E_SOMBRA) e->a0 += rot * (esp ? -1 : 1);
        break;
    case E_TEXTO: mp(m, &e->p[0], &e->p[1]); e->alt *= s; e->giro += rot; break;
    case E_COTA:
        e->alt *= s;
        if (e->sub == C_RADIO || e->sub == C_DIAM) {
            double ux = gm_cos(e->a0), uy = gm_sin(e->a0), vx = m[0] * ux + m[1] * uy, vy = m[3] * ux + m[4] * uy;
            mp(m, &e->p[0], &e->p[1]); e->r *= s; e->a0 = pm_ang(vx, vy);
        } else {
            int np = e->sub == C_ANGULAR ? 4 : 3;
            for (int i = 0; i < np; i++) mp(m, &e->p[2 * i], &e->p[2 * i + 1]);
            if (e->sub == C_ANGULAR && esp) { double t0 = e->p[2], t1 = e->p[3]; e->p[2] = e->p[4]; e->p[3] = e->p[5]; e->p[4] = t0; e->p[5] = t1; }
        }
        break;
    }
}
