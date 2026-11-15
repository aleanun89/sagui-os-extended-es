// SPDX-License-Identifier: GPL-3.0-or-later
// PlanoMini, © 2026 Alejandro Anuncibay: prototipo original (C++/SDL) del mantenedor; versión nativa de SaguiOS.
/* pm_io.c - ficheros de PlanoMini (D312): .pmini (v1 del prototipo y v2 con capas, polilíneas, textos, cotas y
 * sombreados) y DXF ASCII (R12 al escribir; R12 y 2000 al leer: LINE, CIRCLE, ARC, TEXT/MTEXT, POINT,
 * LWPOLYLINE, POLYLINE/VERTEX, SOLID, DIMENSION con su bloque explotado, capas con color y tipo de línea).
 * DXF es un formato de intercambio documentado públicamente; aquí no se usa nada de ningún programa concreto. */
#include "pm.h"

/* ============================================================ búfer de texto */
typedef struct { char *b; int n, cap; bool mal; } sb_t;
static void sb_add(sb_t *s, const char *t)
{
    int l = (int)tam_texto(t);
    if (s->mal) return;
    if (s->n + l + 1 > s->cap) {
        int nc = s->cap ? s->cap * 2 : 8192;
        while (nc < s->n + l + 1) nc *= 2;
        char *nb = s->b ? realoca(s->b, (size_t)nc) : aloca((size_t)nc);
        if (!nb) { s->mal = true; return; }
        s->b = nb; s->cap = nc;
    }
    memcpy(s->b + s->n, t, (size_t)l + 1);
    s->n += l;
}
static void sb_num(sb_t *s, double v, int dec) { char t[48]; pm_fmt(v, dec, t, sizeof t); sb_add(s, t); }
static void sb_int(sb_t *s, int v) { char t[16]; formatar_texto(t, sizeof t, "%d", v); sb_add(s, t); }

const char *const pm_ltipo_nombre_dxf[LT_N] = { "CONTINUOUS", "DASHED", "DOT", "DASHDOT" };

/* ============================================================ colores ACI */
static uint32_t hsv(double h, double s, double v)
{
    double c_ = v * s, hh = h / 60.0, x = c_ * (1 - gm_abs(gm_fmod(hh, 2.0) - 1)), r = 0, g = 0, b = 0;
    int k = (int)hh;
    if (k == 0) { r = c_; g = x; } else if (k == 1) { r = x; g = c_; } else if (k == 2) { g = c_; b = x; }
    else if (k == 3) { g = x; b = c_; } else if (k == 4) { r = x; b = c_; } else { r = c_; b = x; }
    double m = v - c_;
    return ((uint32_t)((r + m) * 255 + 0.5) << 16) | ((uint32_t)((g + m) * 255 + 0.5) << 8) | (uint32_t)((b + m) * 255 + 0.5);
}
uint32_t pm_aci_color(int a)
{
    static const uint32_t base[10] = { 0xFFFFFF, 0xFF0000, 0xFFFF00, 0x00FF00, 0x00FFFF, 0x0000FF, 0xFF00FF, 0xFFFFFF, 0x808080, 0xC0C0C0 };
    static const uint32_t gris[6] = { 0x333333, 0x505050, 0x696969, 0x828282, 0xBEBEBE, 0xFFFFFF };
    if (a >= 1 && a <= 9) return base[a];
    if (a >= 250 && a <= 255) return gris[a - 250];
    if (a >= 10 && a <= 249) {
        int h = (a - 10) / 10, k = (a - 10) % 10;
        return hsv(h * 15.0, 1.0 - 0.25 * (k / 2), (k % 2 == 0) ? 1.0 : 0.65);
    }
    return 0xFFFFFF;
}
int pm_color_aci(uint32_t rgb)
{
    int mejor = 7;
    long dm = 0x7FFFFFFF;
    for (int a = 1; a <= 255; a++) {
        uint32_t q = pm_aci_color(a);
        long dr = (long)((rgb >> 16) & 255) - (long)((q >> 16) & 255), dg = (long)((rgb >> 8) & 255) - (long)((q >> 8) & 255), db = (long)(rgb & 255) - (long)(q & 255);
        long d = dr * dr + dg * dg + db * db;
        if (d < dm) { dm = d; mejor = a; }
    }
    return mejor;
}

/* ============================================================ .pmini */

char *pm_serializar_ex(int *n, bool con_vista)
{
    sb_t s = { NULL, 0, 0, false };
    sb_add(&s, "PLANOMINI 2\n");
    sb_add(&s, "ESTILO "); sb_num(&s, pm_cota_alt, 6); sb_add(&s, " "); sb_num(&s, pm_cota_flecha, 6); sb_add(&s, " "); sb_int(&s, pm_cota_dec); sb_add(&s, "\n");
    if (con_vista) { sb_add(&s, "VISTA "); sb_num(&s, pm_vcx, 6); sb_add(&s, " "); sb_num(&s, pm_vcy, 6); sb_add(&s, " "); sb_num(&s, pm_ve, 6); sb_add(&s, "\n"); }
    for (int i = 0; i < pm_ncapa; i++) {
        char t[80];
        formatar_texto(t, sizeof t, "CAPA \"%s\" %x %d %d %d\n", pm_capa[i].nome, (unsigned)pm_capa[i].color, pm_capa[i].ltipo, pm_capa[i].visible, pm_capa[i].bloq);
        sb_add(&s, t);
    }
    sb_add(&s, "ACTUAL "); sb_int(&s, pm_capa_act); sb_add(&s, "\n");
    for (int i = 0; i < pm_n; i++) {
        const pm_ent_t *e = &pm_ent[i];
        char t[4];
        t[1] = 0;
        if (e->tipo == E_POLI) t[0] = 'W';
        else if (e->tipo == E_PUNTO) t[0] = 'P';
        else if (e->tipo == E_LINEA) t[0] = 'L';
        else if (e->tipo == E_CIRCULO) t[0] = 'C';
        else if (e->tipo == E_ARCO) t[0] = 'A';
        else if (e->tipo == E_TEXTO) t[0] = 'T';
        else if (e->tipo == E_COTA) t[0] = 'D';
        else if (e->tipo == E_SOMBRA) t[0] = 'S';
        else continue;
        sb_add(&s, t); sb_add(&s, " "); sb_int(&s, e->capa); sb_add(&s, " "); sb_int(&s, e->color); sb_add(&s, " "); sb_int(&s, e->ltipo);
        switch (e->tipo) {
        case E_PUNTO: sb_add(&s, " "); sb_num(&s, e->p[0], 9); sb_add(&s, " "); sb_num(&s, e->p[1], 9); break;
        case E_LINEA: for (int k = 0; k < 4; k++) { sb_add(&s, " "); sb_num(&s, e->p[k], 9); } break;
        case E_CIRCULO: for (int k = 0; k < 2; k++) { sb_add(&s, " "); sb_num(&s, e->p[k], 9); } sb_add(&s, " "); sb_num(&s, e->r, 9); break;
        case E_ARCO: for (int k = 0; k < 2; k++) { sb_add(&s, " "); sb_num(&s, e->p[k], 9); } sb_add(&s, " "); sb_num(&s, e->r, 9); sb_add(&s, " "); sb_num(&s, pm_angnorm(e->a0), 9); sb_add(&s, " "); sb_num(&s, pm_angnorm(e->a1), 9); break;
        case E_POLI: case E_SOMBRA:
            if (e->tipo == E_SOMBRA) { sb_add(&s, " "); sb_int(&s, e->sub); sb_add(&s, " "); sb_num(&s, e->a0, 9); sb_add(&s, " "); sb_num(&s, e->r, 9); }
            else { sb_add(&s, " "); sb_int(&s, e->cerrada); }
            sb_add(&s, " "); sb_int(&s, e->nv);
            for (int k = 0; k < e->nv; k++) { sb_add(&s, " "); sb_num(&s, e->v[k].x, 9); sb_add(&s, " "); sb_num(&s, e->v[k].y, 9); sb_add(&s, " "); sb_num(&s, e->v[k].b, 9); }
            break;
        case E_TEXTO:
            for (int k = 0; k < 2; k++) { sb_add(&s, " "); sb_num(&s, e->p[k], 9); }
            sb_add(&s, " "); sb_num(&s, e->alt, 9); sb_add(&s, " "); sb_num(&s, e->giro, 9); sb_add(&s, " \""); sb_add(&s, e->txt); sb_add(&s, "\"");
            break;
        case E_COTA:
            sb_add(&s, " "); sb_int(&s, e->sub); sb_add(&s, " "); sb_num(&s, e->alt, 9); sb_add(&s, " "); sb_num(&s, e->r, 9); sb_add(&s, " "); sb_num(&s, e->a0, 9);
            for (int k = 0; k < 8; k++) { sb_add(&s, " "); sb_num(&s, e->p[k], 9); }
            sb_add(&s, " \""); sb_add(&s, e->txt); sb_add(&s, "\"");
            break;
        }
        sb_add(&s, "\n");
    }
    if (n) *n = s.n;
    if (s.mal) { if (s.b) libera(s.b); return NULL; }
    return s.b;
}
char *pm_serializar(int *n) { return pm_serializar_ex(n, true); }

/* Lectura de campos de una línea */
static const char *salta(const char *p) { while (*p == ' ' || *p == '\t') p++; return p; }
static double lee(const char **p) { const char *e; double v = pm_atof(salta(*p), &e); *p = e; return v; }
static bool lee_txt(const char **p, char *d, int max)   /* "texto" */
{
    *p = salta(*p);
    if (**p != '"') return false;
    (*p)++;
    int n = 0;
    while (**p && **p != '"' && n < max - 1) d[n++] = *(*p)++;
    d[n] = 0;
    if (**p == '"') (*p)++;
    return true;
}
static uint32_t lee_hex(const char **p)
{
    *p = salta(*p);
    uint32_t v = 0;
    while ((**p >= '0' && **p <= '9') || (**p >= 'a' && **p <= 'f') || (**p >= 'A' && **p <= 'F')) {
        char c = **p;
        v = v * 16 + (uint32_t)(c <= '9' ? c - '0' : (c | 32) - 'a' + 10);
        (*p)++;
    }
    return v;
}

static int cargar_v1(const char *p)
{
    int n = 0;
    while (*p) {
        while (*p == '\n' || *p == '\r' || *p == ' ') p++;
        char t = *p;
        if (!t) break;
        const char *fin = p;
        while (*fin && *fin != '\n') fin++;
        const char *q = p + 1;
        double v[5];
        for (int i = 0; i < 5; i++) v[i] = lee(&q);
        pm_ent_t e;
        bool ok = true;
        if (t == 'L') { pm_ent_nueva(&e, E_LINEA); e.p[0] = v[0]; e.p[1] = v[1]; e.p[2] = v[2]; e.p[3] = v[3]; }
        else if (t == 'C') { pm_ent_nueva(&e, E_CIRCULO); e.p[0] = v[0]; e.p[1] = v[1]; e.r = gm_abs(v[4]); if (e.r < PM_EPS) ok = false; }
        else if (t == 'R') { pm_ent_nueva(&e, E_POLI); pm_rect_poli(&e, v[0], v[1], v[2], v[3]); }
        else if (t == 'P') { pm_ent_nueva(&e, E_PUNTO); e.p[0] = v[0]; e.p[1] = v[1]; }
        else ok = false;
        if (ok) { if (pm_anadir(&e) >= 0) n++; pm_ent_liberar(&e); }
        p = fin;
    }
    return n;
}

int pm_cargar_ex(const char *texto, bool con_vista)
{
    pm_erro[0] = 0;
    const char *p = salta(texto);
    int ver = 0;
    if (!(p[0] == 'P' && p[1] == 'L' && p[2] == 'A' && p[3] == 'N' && p[4] == 'O' && p[5] == 'M' && p[6] == 'I' && p[7] == 'N' && p[8] == 'I')) {
        copia_texto(pm_erro, "No es un archivo PlanoMini (falta la cabecera PLANOMINI)", sizeof pm_erro);
        return -1;
    }
    p += 9;
    ver = (int)lee(&p);
    if (ver < 1 || ver > 2) { copia_texto(pm_erro, "Versión de PlanoMini no soportada", sizeof pm_erro); return -1; }
    pm_limpiar();
    while (*p && *p != '\n') p++;
    if (ver == 1) { cargar_v1(p); if (con_vista) pm_zoom_ext(); return 0; }
    int malas = 0;
    while (*p) {
        while (*p == '\n' || *p == '\r' || *p == ' ') p++;
        if (!*p) break;
        const char *fin = p;
        while (*fin && *fin != '\n') fin++;
        char lin[1100];
        int ln = (int)(fin - p);
        if (ln > (int)sizeof lin - 1) ln = (int)sizeof lin - 1;
        memcpy(lin, p, (size_t)ln);
        lin[ln] = 0;
        p = fin;
        const char *q = lin;
        if (lin[0] == 'E' && lin[1] == 'S') { q = lin + 6; pm_cota_alt = lee(&q); pm_cota_flecha = lee(&q); pm_cota_dec = (int)lee(&q); continue; }
        if (lin[0] == 'V' && lin[1] == 'I') { q = lin + 5; double a = lee(&q), b = lee(&q), c = lee(&q); if (con_vista && c > 0) { pm_vcx = a; pm_vcy = b; pm_ve = c; } continue; }
        if (lin[0] == 'C' && lin[1] == 'A') {
            q = lin + 4;
            char nom[32];
            if (!lee_txt(&q, nom, sizeof nom)) { malas++; continue; }
            int i = pm_capa_nueva(nom);
            if (i < 0) { malas++; continue; }
            pm_capa[i].color = lee_hex(&q); pm_capa[i].ltipo = (uint8_t)lee(&q); pm_capa[i].visible = lee(&q) != 0; pm_capa[i].bloq = lee(&q) != 0;
            if (pm_capa[i].ltipo >= LT_N) pm_capa[i].ltipo = 0;
            continue;
        }
        if (lin[0] == 'A' && lin[1] == 'C') { q = lin + 6; int a = (int)lee(&q); if (a >= 0 && a < pm_ncapa) pm_capa_act = a; continue; }
        char t = lin[0];
        q = lin + 1;
        pm_ent_t e;
        int tipo = t == 'L' ? E_LINEA : t == 'C' ? E_CIRCULO : t == 'A' ? E_ARCO : t == 'P' ? E_PUNTO : t == 'W' ? E_POLI : t == 'T' ? E_TEXTO : t == 'D' ? E_COTA : t == 'S' ? E_SOMBRA : 0;
        if (!tipo) { malas++; continue; }
        pm_ent_nueva(&e, tipo);
        int capa = (int)lee(&q);
        e.capa = (uint8_t)(capa >= 0 && capa < pm_ncapa ? capa : 0);
        e.color = (int32_t)lee(&q);
        double lt = lee(&q);
        e.ltipo = lt >= 0 && lt < LT_N ? (uint8_t)lt : LT_CAPA;
        switch (tipo) {
        case E_PUNTO: e.p[0] = lee(&q); e.p[1] = lee(&q); break;
        case E_LINEA: for (int k = 0; k < 4; k++) e.p[k] = lee(&q); break;
        case E_CIRCULO: e.p[0] = lee(&q); e.p[1] = lee(&q); e.r = lee(&q); break;
        case E_ARCO: e.p[0] = lee(&q); e.p[1] = lee(&q); e.r = lee(&q); e.a0 = lee(&q); e.a1 = lee(&q); break;
        case E_POLI: case E_SOMBRA: {
            if (tipo == E_SOMBRA) { e.sub = (uint8_t)lee(&q); e.a0 = lee(&q); e.r = lee(&q); e.cerrada = 1; } else e.cerrada = lee(&q) != 0;
            int nv = (int)lee(&q);
            for (int k = 0; k < nv && k < PM_VERT_MAX; k++) { double x = lee(&q), y = lee(&q), b = lee(&q); pm_poli_vert(&e, x, y, b); }
            break;
        }
        case E_TEXTO:
            e.p[0] = lee(&q); e.p[1] = lee(&q); e.alt = lee(&q); e.giro = lee(&q);
            lee_txt(&q, e.txt, sizeof e.txt);
            break;
        case E_COTA:
            e.sub = (uint8_t)lee(&q); e.alt = lee(&q); e.r = lee(&q); e.a0 = lee(&q);
            for (int k = 0; k < 8; k++) e.p[k] = lee(&q);
            lee_txt(&q, e.txt, sizeof e.txt);
            break;
        }
        if (pm_anadir(&e) < 0) malas++;
        pm_ent_liberar(&e);
    }
    if (con_vista) { double a, b, c, d; if (pm_n && !(pm_ve > 0 && pm_ve < 1e7)) pm_zoom_ext(); (void)a; (void)b; (void)c; (void)d; }
    return 0;
}
int pm_cargar(const char *texto) { return pm_cargar_ex(texto, true); }

/* ============================================================ DXF: escritura (R12) */

static void g(sb_t *s, int code, const char *v) { sb_int(s, code); sb_add(s, "\n"); sb_add(s, v); sb_add(s, "\n"); }
static void gn(sb_t *s, int code, double v) { sb_int(s, code); sb_add(s, "\n"); sb_num(s, v, 9); sb_add(s, "\n"); }
static void gi(sb_t *s, int code, int v) { sb_int(s, code); sb_add(s, "\n"); sb_int(s, v); sb_add(s, "\n"); }

static void dxf_texto(char *d, const char *t, int max)     /* Ø y ° con los códigos %%c y %%d */
{
    int n = 0;
    for (; *t && n + 4 < max; t++) {
        unsigned char u = (unsigned char)*t;
        if (u == 0xD8) { d[n++] = '%'; d[n++] = '%'; d[n++] = 'c'; }
        else if (u == 0xB0) { d[n++] = '%'; d[n++] = '%'; d[n++] = 'd'; }
        else if (u == 0xB1) { d[n++] = '%'; d[n++] = '%'; d[n++] = 'p'; }
        else d[n++] = *t;
    }
    d[n] = 0;
}

typedef struct { sb_t *s; const pm_ent_t *e; } dw_t;
static void dxf_comun(sb_t *s, const pm_ent_t *e, const char *tipo)
{
    g(s, 0, tipo);
    g(s, 8, pm_capa[e->capa < pm_ncapa ? e->capa : 0].nome);
    if (e->color >= 0) gi(s, 62, pm_color_aci((uint32_t)e->color));
    if (e->ltipo != LT_CAPA && e->ltipo < LT_N) g(s, 6, pm_ltipo_nombre_dxf[e->ltipo]);
}
static void dw_seg(void *c, double x0, double y0, double x1, double y1)
{
    dw_t *w = c;
    if (gm_hypot(x1 - x0, y1 - y0) < 1e-12) return;
    dxf_comun(w->s, w->e, "LINE");
    gn(w->s, 10, x0); gn(w->s, 20, y0); gn(w->s, 30, 0); gn(w->s, 11, x1); gn(w->s, 21, y1); gn(w->s, 31, 0);
}
static void solid(dw_t *w, double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3)
{
    dxf_comun(w->s, w->e, "SOLID");
    gn(w->s, 10, x0); gn(w->s, 20, y0); gn(w->s, 30, 0); gn(w->s, 11, x1); gn(w->s, 21, y1); gn(w->s, 31, 0);
    gn(w->s, 12, x2); gn(w->s, 22, y2); gn(w->s, 32, 0); gn(w->s, 13, x3); gn(w->s, 23, y3); gn(w->s, 33, 0);
}
static void dw_tri(void *c, double a, double b, double d, double e, double f, double h) { solid(c, a, b, d, e, f, h, f, h); }
static void dw_poli(void *c, const double *xs, const double *ys, int n)    /* relleno sólido: franjas horizontales de SOLID */
{
    double ymin = 1e300, ymax = -1e300;
    for (int i = 0; i < n; i++) { if (ys[i] < ymin) ymin = ys[i]; if (ys[i] > ymax) ymax = ys[i]; }
    double h = (ymax - ymin) / 60.0;
    if (h < 1e-9) return;
    for (int k = 0; k < 60; k++) {
        double y = ymin + (k + 0.5) * h, y0 = ymin + k * h, y1 = y0 + h * 1.02, xi[64];
        int m = 0;
        for (int i = 0; i < n && m < 62; i++) {
            int j = (i + 1) % n;
            if ((ys[i] <= y && ys[j] > y) || (ys[j] <= y && ys[i] > y)) xi[m++] = xs[i] + (y - ys[i]) / (ys[j] - ys[i]) * (xs[j] - xs[i]);
        }
        for (int a = 1; a < m; a++) { double v = xi[a]; int b = a - 1; while (b >= 0 && xi[b] > v) { xi[b + 1] = xi[b]; b--; } xi[b + 1] = v; }
        for (int a = 0; a + 1 < m; a += 2) solid(c, xi[a], y0, xi[a + 1], y0, xi[a], y1, xi[a + 1], y1);
    }
}
static void dw_txt(void *c, double x, double y, double alt, double giro, const char *t, int cen)
{
    dw_t *w = c;
    double wd = pm_txt_ancho(alt, t), px = cen ? x - wd / 2 * gm_cos(giro) : x, py = cen ? y - wd / 2 * gm_sin(giro) : y;
    char b[200];
    dxf_comun(w->s, w->e, "TEXT");
    gn(w->s, 10, px); gn(w->s, 20, py); gn(w->s, 30, 0); gn(w->s, 40, alt);
    dxf_texto(b, t, sizeof b);
    g(w->s, 1, b);
    if (giro != 0) gn(w->s, 50, giro * 180.0 / PM_PI);
}

char *pm_dxf_escribir(int *n)
{
    sb_t s = { NULL, 0, 0, false };
    g(&s, 0, "SECTION"); g(&s, 2, "HEADER");
    g(&s, 9, "$ACADVER"); g(&s, 1, "AC1009");
    g(&s, 9, "$DWGCODEPAGE"); g(&s, 3, "ANSI_1252");
    g(&s, 0, "ENDSEC");
    g(&s, 0, "SECTION"); g(&s, 2, "TABLES");
    g(&s, 0, "TABLE"); g(&s, 2, "LTYPE"); gi(&s, 70, 4);
    static const char *const desc[4] = { "Solid line", "Dashed", "Dot", "Dash dot" };
    static const double pat[4][5] = { { 0 }, { 2, 0.75, 0.5, -0.25 }, { 2, 0.25, 0.0, -0.25 }, { 4, 1.0, 0.5, -0.25, 0.0 } };
    for (int i = 0; i < 4; i++) {
        g(&s, 0, "LTYPE"); g(&s, 2, pm_ltipo_nombre_dxf[i]); gi(&s, 70, 0); g(&s, 3, desc[i]); gi(&s, 72, 65);
        gi(&s, 73, (int)pat[i][0]);
        gn(&s, 40, i == 0 ? 0 : i == 1 ? 0.75 : i == 2 ? 0.25 : 1.0);
        if (i == 1) { gn(&s, 49, 0.5); gn(&s, 49, -0.25); }
        if (i == 2) { gn(&s, 49, 0.0); gn(&s, 49, -0.25); }
        if (i == 3) { gn(&s, 49, 0.5); gn(&s, 49, -0.25); gn(&s, 49, 0.0); gn(&s, 49, -0.25); }
    }
    (void)pat;
    g(&s, 0, "ENDTAB");
    g(&s, 0, "TABLE"); g(&s, 2, "LAYER"); gi(&s, 70, pm_ncapa);
    for (int i = 0; i < pm_ncapa; i++) {
        g(&s, 0, "LAYER"); g(&s, 2, pm_capa[i].nome);
        gi(&s, 70, pm_capa[i].bloq ? 4 : 0);
        int aci = pm_color_aci(pm_capa[i].color);
        gi(&s, 62, pm_capa[i].visible ? aci : -aci);
        g(&s, 6, pm_ltipo_nombre_dxf[pm_capa[i].ltipo < LT_N ? pm_capa[i].ltipo : 0]);
    }
    g(&s, 0, "ENDTAB");
    g(&s, 0, "TABLE"); g(&s, 2, "STYLE"); gi(&s, 70, 1);
    g(&s, 0, "STYLE"); g(&s, 2, "STANDARD"); gi(&s, 70, 0); gn(&s, 40, 0); gn(&s, 41, 1); gn(&s, 50, 0); gi(&s, 71, 0); gn(&s, 42, 0.2); g(&s, 3, "txt"); g(&s, 4, "");
    g(&s, 0, "ENDTAB");
    g(&s, 0, "ENDSEC");
    g(&s, 0, "SECTION"); g(&s, 2, "ENTITIES");
    for (int i = 0; i < pm_n; i++) {
        const pm_ent_t *e = &pm_ent[i];
        switch (e->tipo) {
        case E_PUNTO: dxf_comun(&s, e, "POINT"); gn(&s, 10, e->p[0]); gn(&s, 20, e->p[1]); gn(&s, 30, 0); break;
        case E_LINEA: dxf_comun(&s, e, "LINE"); gn(&s, 10, e->p[0]); gn(&s, 20, e->p[1]); gn(&s, 30, 0); gn(&s, 11, e->p[2]); gn(&s, 21, e->p[3]); gn(&s, 31, 0); break;
        case E_CIRCULO: dxf_comun(&s, e, "CIRCLE"); gn(&s, 10, e->p[0]); gn(&s, 20, e->p[1]); gn(&s, 30, 0); gn(&s, 40, e->r); break;
        case E_ARCO: dxf_comun(&s, e, "ARC"); gn(&s, 10, e->p[0]); gn(&s, 20, e->p[1]); gn(&s, 30, 0); gn(&s, 40, e->r); gn(&s, 50, pm_angnorm(e->a0) * 180 / PM_PI); gn(&s, 51, pm_angnorm(e->a1) * 180 / PM_PI); break;
        case E_POLI:
            dxf_comun(&s, e, "POLYLINE"); gi(&s, 66, 1); gn(&s, 10, 0); gn(&s, 20, 0); gn(&s, 30, 0); gi(&s, 70, e->cerrada ? 1 : 0);
            for (int k = 0; k < e->nv; k++) {
                g(&s, 0, "VERTEX"); g(&s, 8, pm_capa[e->capa < pm_ncapa ? e->capa : 0].nome);
                gn(&s, 10, e->v[k].x); gn(&s, 20, e->v[k].y); gn(&s, 30, 0);
                if (e->v[k].b != 0) gn(&s, 42, e->v[k].b);
            }
            g(&s, 0, "SEQEND"); g(&s, 8, pm_capa[e->capa < pm_ncapa ? e->capa : 0].nome);
            break;
        case E_TEXTO: {
            char b[200];
            dxf_comun(&s, e, "TEXT"); gn(&s, 10, e->p[0]); gn(&s, 20, e->p[1]); gn(&s, 30, 0); gn(&s, 40, e->alt);
            dxf_texto(b, e->txt, sizeof b);
            g(&s, 1, b);
            if (e->giro != 0) gn(&s, 50, e->giro * 180 / PM_PI);
            break;
        }
        case E_COTA: case E_SOMBRA: {                  /* explotadas: líneas, flechas sólidas y texto */
            dw_t w = { &s, e };
            if (e->tipo == E_SOMBRA && e->sub == 1 && (e->nv == 3 || e->nv == 4)) {      /* un triángulo o un cuadrilátero rectos: un SOLID */
                bool recto = true;
                for (int k = 0; k < e->nv; k++) if (e->v[k].b != 0) recto = false;
                if (recto) {
                    if (e->nv == 3) solid(&w, e->v[0].x, e->v[0].y, e->v[1].x, e->v[1].y, e->v[2].x, e->v[2].y, e->v[2].x, e->v[2].y);
                    else solid(&w, e->v[0].x, e->v[0].y, e->v[1].x, e->v[1].y, e->v[3].x, e->v[3].y, e->v[2].x, e->v[2].y);
                    break;
                }
            }
            pm_sink_t k = { dw_seg, dw_tri, dw_poli, dw_txt, &w, 0.01 };
            pm_dibujar_ent(e, &k);
            break;
        }
        }
    }
    g(&s, 0, "ENDSEC");
    g(&s, 0, "EOF");
    if (n) *n = s.n;
    if (s.mal) { if (s.b) libera(s.b); return NULL; }
    return s.b;
}

/* ============================================================ DXF: lectura */

typedef struct { int code; const char *val; } par_t;
typedef struct { const char *nombre; int ini, fin; } bloque_t;

static par_t *P_;
static int NP_;
static bloque_t BL_[64];
static int NBL_;
static int omit_;

static int ltipo_dxf(const char *n)
{
    if (!n[0]) return LT_CAPA;
    char u[32];
    int k = 0;
    for (; n[k] && k < 31; k++) u[k] = (n[k] >= 'a' && n[k] <= 'z') ? (char)(n[k] - 32) : n[k];
    u[k] = 0;
    if (u[0] == 'B' && u[1] == 'Y' && u[2] == 'L') return LT_CAPA;
    bool dash = false, dot = false;
    for (int i = 0; u[i]; i++) {
        if (u[i] == 'D' && u[i + 1] == 'A' && u[i + 2] == 'S' && u[i + 3] == 'H') dash = true;
        if (u[i] == 'D' && u[i + 1] == 'O' && u[i + 2] == 'T') dot = true;
        if (u[i] == 'H' && u[i + 1] == 'I' && u[i + 2] == 'D') dash = true;
        if (u[i] == 'C' && u[i + 1] == 'E' && u[i + 2] == 'N') dash = true;
    }
    if (dash && dot) return LT_TRAZOPUNTO;
    if (dash) return LT_TRAZOS;
    if (dot) return LT_PUNTOS;
    return LT_CONT;
}

static void dxf_texto_a_latin1(char *d, const char *t, int max)
{
    int n = 0;
    for (; *t && n + 1 < max; t++) {
        if (t[0] == '%' && t[1] == '%' && n + 1 < max) {
            char c = t[2];
            if (c == 'c' || c == 'C') { d[n++] = (char)0xD8; t += 2; continue; }
            if (c == 'd' || c == 'D') { d[n++] = (char)0xB0; t += 2; continue; }
            if (c == 'p' || c == 'P') { d[n++] = (char)0xB1; t += 2; continue; }
        }
        if (t[0] == '\\' && (t[1] == 'P' || t[1] == 'p')) { d[n++] = ' '; t++; continue; }
        d[n++] = *t;
    }
    d[n] = 0;
}

typedef struct { int capa; int32_t color; int lt; double v[50]; bool tiene[50]; char txt[160]; char bloque[40]; int flags; } dxfent_t;

/* Lee los pares de una entidad desde i (que apunta a su "0") hasta el siguiente "0". */
static int leer_entidad(int i, dxfent_t *d)
{
    memset(d, 0, sizeof *d);
    d->color = COL_CAPA; d->lt = LT_CAPA; d->capa = 0;
    int j = i + 1;
    for (; j < NP_ && P_[j].code != 0; j++) {
        int c_ = P_[j].code;
        const char *v = P_[j].val;
        if (c_ == 8) { int k = pm_capa_nueva(v); d->capa = k < 0 ? 0 : k; }
        else if (c_ == 62) { int a = (int)pm_atof(v, NULL); d->color = (a >= 1 && a <= 255) ? (int32_t)pm_aci_color(a) : COL_CAPA; }
        else if (c_ == 420) d->color = (int32_t)((long)pm_atof(v, NULL) & 0xFFFFFF);
        else if (c_ == 6) d->lt = ltipo_dxf(v);
        else if (c_ == 1 || c_ == 3) dxf_texto_a_latin1(d->txt, v, sizeof d->txt);
        else if (c_ == 2) copia_texto(d->bloque, v, sizeof d->bloque);
        else if (c_ == 70) d->flags = (int)pm_atof(v, NULL);
        else if (c_ >= 10 && c_ < 60) { d->v[c_ - 10] = pm_atof(v, NULL); d->tiene[c_ - 10] = true; }
    }
    return j;
}

static void aplicar_props(pm_ent_t *e, const dxfent_t *d) { e->capa = (uint8_t)d->capa; e->color = d->color; e->ltipo = (uint8_t)d->lt; }
static int num_(const dxfent_t *d, int code) { return d->tiene[code - 10] ? 1 : 0; }

static void leer_rango(int ini, int fin);

static int leer_una(int i)      /* entidad en i; devuelve el índice siguiente */
{
    dxfent_t d;
    const char *tipo = P_[i].val;
    int j = leer_entidad(i, &d);
    pm_ent_t e;
#define IS(s) (compara_texto(tipo, s) == 0)
    if (IS("LINE")) {
        pm_ent_nueva(&e, E_LINEA); e.p[0] = d.v[0]; e.p[1] = d.v[10]; e.p[2] = d.v[1]; e.p[3] = d.v[11];
        aplicar_props(&e, &d); pm_anadir(&e);
    } else if (IS("CIRCLE")) {
        pm_ent_nueva(&e, E_CIRCULO); e.p[0] = d.v[0]; e.p[1] = d.v[10]; e.r = d.v[30];
        aplicar_props(&e, &d); if (e.r > 0) pm_anadir(&e);
    } else if (IS("ARC")) {
        pm_ent_nueva(&e, E_ARCO); e.p[0] = d.v[0]; e.p[1] = d.v[10]; e.r = d.v[30];
        e.a0 = pm_angnorm(d.v[40] * PM_PI / 180); e.a1 = pm_angnorm(d.v[41] * PM_PI / 180);
        aplicar_props(&e, &d); if (e.r > 0) pm_anadir(&e);
    } else if (IS("POINT")) {
        pm_ent_nueva(&e, E_PUNTO); e.p[0] = d.v[0]; e.p[1] = d.v[10];
        aplicar_props(&e, &d); pm_anadir(&e);
    } else if (IS("TEXT") || IS("MTEXT")) {
        pm_ent_nueva(&e, E_TEXTO);
        double x = d.v[0], y = d.v[10];
        e.p[0] = x; e.p[1] = y; e.alt = d.v[30] > 0 ? d.v[30] : 2.5; e.giro = d.v[40] * PM_PI / 180;
        if (IS("MTEXT")) { e.giro = num_(&d, 50) ? d.v[40] * PM_PI / 180 : 0; }
        copia_texto(e.txt, d.txt, sizeof e.txt);
        aplicar_props(&e, &d);
        if (e.txt[0]) pm_anadir(&e);
    } else if (IS("LWPOLYLINE")) {
        pm_ent_nueva(&e, E_POLI);
        for (int k = i + 1; k < j; k++) {
            if (P_[k].code == 10) {
                double x = pm_atof(P_[k].val, NULL), y = 0, b = 0;
                if (k + 1 < j && P_[k + 1].code == 20) y = pm_atof(P_[k + 1].val, NULL);
                int m = k + 2;
                if (m < j && P_[m].code == 42) b = pm_atof(P_[m].val, NULL);
                pm_poli_vert(&e, x, y, b);
            }
        }
        e.cerrada = (d.flags & 1) ? 1 : 0;
        aplicar_props(&e, &d);
        if (e.nv >= 2) pm_anadir(&e);
        pm_ent_liberar(&e);
    } else if (IS("POLYLINE")) {
        pm_ent_nueva(&e, E_POLI);
        e.cerrada = (d.flags & 1) ? 1 : 0;
        aplicar_props(&e, &d);
        while (j < NP_ && P_[j].code == 0 && compara_texto(P_[j].val, "VERTEX") == 0) {
            dxfent_t v;
            int jj = leer_entidad(j, &v);
            pm_poli_vert(&e, v.v[0], v.v[10], v.tiene[32] ? v.v[32] : 0);
            j = jj;
        }
        if (j < NP_ && P_[j].code == 0 && compara_texto(P_[j].val, "SEQEND") == 0) { dxfent_t sq; j = leer_entidad(j, &sq); }
        if (e.nv >= 2) pm_anadir(&e);
        pm_ent_liberar(&e);
    } else if (IS("SOLID") || IS("TRACE")) {
        pm_ent_nueva(&e, E_SOMBRA);
        e.sub = 1; e.cerrada = 1;
        pm_poli_vert(&e, d.v[0], d.v[10], 0); pm_poli_vert(&e, d.v[1], d.v[11], 0);
        if (d.v[3] != d.v[2] || d.v[13] != d.v[12]) pm_poli_vert(&e, d.v[3], d.v[13], 0);
        pm_poli_vert(&e, d.v[2], d.v[12], 0);
        aplicar_props(&e, &d);
        pm_anadir(&e);
        pm_ent_liberar(&e);
    } else if (IS("DIMENSION")) {
        int b = -1;
        for (int k = 0; k < NBL_; k++) if (compara_texto(BL_[k].nombre, d.bloque) == 0) b = k;
        if (b >= 0 && d.bloque[0]) leer_rango(BL_[b].ini, BL_[b].fin);
        else omit_++;
    } else if (IS("VERTEX") || IS("SEQEND") || IS("ENDBLK") || IS("BLOCK") || IS("ATTDEF") || IS("ATTRIB")) {
    } else omit_++;
#undef IS
    return j;
}

static void leer_rango(int ini, int fin)
{
    int i = ini;
    while (i < fin && i < NP_) {
        if (P_[i].code != 0) { i++; continue; }
        i = leer_una(i);
    }
}

int pm_dxf_leer(const char *texto, int *omitidas)
{
    pm_erro[0] = 0;
    int len = (int)tam_texto(texto);
    char *buf = aloca((size_t)len + 2);
    if (!buf) { copia_texto(pm_erro, "Sin memoria", sizeof pm_erro); return -1; }
    memcpy(buf, texto, (size_t)len + 1);
    int cap = 4096;
    P_ = aloca((size_t)cap * sizeof(par_t));
    NP_ = 0; NBL_ = 0; omit_ = 0;
    if (!P_) { libera(buf); return -1; }
    char *p = buf;
    while (*p) {
        char *l1 = p;
        while (*p && *p != '\n') p++;
        char *e1 = p;
        if (*p) p++;
        if (!*p && e1 == l1) break;
        char *l2 = p;
        while (*p && *p != '\n') p++;
        char *e2 = p;
        if (*p) p++;
        while (e1 > l1 && (e1[-1] == '\r' || e1[-1] == ' ')) e1--;
        while (e2 > l2 && (e2[-1] == '\r' || e2[-1] == ' ')) e2--;
        *e1 = 0; *e2 = 0;
        while (*l1 == ' ') l1++;
        while (*l2 == ' ' && l2 < e2) l2++;
        if (NP_ >= cap) {
            cap *= 2;
            par_t *n = realoca(P_, (size_t)cap * sizeof(par_t));
            if (!n) break;
            P_ = n;
        }
        P_[NP_].code = (int)pm_atof(l1, NULL);
        P_[NP_].val = l2;
        NP_++;
    }
    /* comprobación: tiene que haber una sección ENTITIES */
    int ent_ini = -1, ent_fin = -1;
    int sec = 0;       /* 1 TABLES, 2 BLOCKS, 3 ENTITIES */
    for (int i = 0; i < NP_; i++) {
        if (P_[i].code == 0 && compara_texto(P_[i].val, "SECTION") == 0 && i + 1 < NP_ && P_[i + 1].code == 2) {
            const char *n = P_[i + 1].val;
            sec = compara_texto(n, "TABLES") == 0 ? 1 : compara_texto(n, "BLOCKS") == 0 ? 2 : compara_texto(n, "ENTITIES") == 0 ? 3 : 0;
            if (sec == 3) ent_ini = i + 2;
        } else if (P_[i].code == 0 && compara_texto(P_[i].val, "ENDSEC") == 0) {
            if (sec == 3 && ent_ini >= 0 && ent_fin < 0) ent_fin = i;
            sec = 0;
        } else if (sec == 2 && P_[i].code == 0 && compara_texto(P_[i].val, "BLOCK") == 0 && NBL_ < 64) {
            const char *nombre = "";
            int j = i + 1;
            for (; j < NP_ && P_[j].code != 0; j++) if (P_[j].code == 2) nombre = P_[j].val;
            BL_[NBL_].nombre = nombre; BL_[NBL_].ini = j;
            int k = j;
            while (k < NP_ && !(P_[k].code == 0 && compara_texto(P_[k].val, "ENDBLK") == 0)) k++;
            BL_[NBL_].fin = k;
            NBL_++;
        }
    }
    if (ent_ini < 0) { copia_texto(pm_erro, "El DXF no tiene sección ENTITIES", sizeof pm_erro); libera(P_); libera(buf); return -1; }
    if (ent_fin < 0) ent_fin = NP_;
    pm_limpiar();
    /* capas */
    sec = 0;
    for (int i = 0; i < NP_; i++) {
        if (P_[i].code == 0 && compara_texto(P_[i].val, "SECTION") == 0 && i + 1 < NP_ && P_[i + 1].code == 2) sec = compara_texto(P_[i + 1].val, "TABLES") == 0 ? 1 : 0;
        else if (P_[i].code == 0 && compara_texto(P_[i].val, "ENDSEC") == 0) sec = 0;
        else if (sec == 1 && P_[i].code == 0 && compara_texto(P_[i].val, "LAYER") == 0) {
            const char *nombre = "";
            int aci = 7, fl = 0, lt = LT_CONT;
            for (int j = i + 1; j < NP_ && P_[j].code != 0; j++) {
                if (P_[j].code == 2) nombre = P_[j].val;
                else if (P_[j].code == 62) aci = (int)pm_atof(P_[j].val, NULL);
                else if (P_[j].code == 70) fl = (int)pm_atof(P_[j].val, NULL);
                else if (P_[j].code == 6) lt = ltipo_dxf(P_[j].val);
            }
            int k = pm_capa_nueva(nombre);
            if (k >= 0) {
                pm_capa[k].color = pm_aci_color(aci < 0 ? -aci : aci == 0 ? 7 : aci);
                pm_capa[k].visible = aci >= 0 && !(fl & 1);
                pm_capa[k].bloq = (fl & 4) ? 1 : 0;
                pm_capa[k].ltipo = (uint8_t)(lt == LT_CAPA ? LT_CONT : lt);
            }
        }
    }
    leer_rango(ent_ini, ent_fin);
    if (omitidas) *omitidas = omit_;
    libera(P_); P_ = NULL;
    libera(buf);
    pm_zoom_ext();
    return 0;
}
