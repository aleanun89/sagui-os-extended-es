// SPDX-License-Identifier: GPL-3.0-or-later
/* arara_modelo.h - el documento de Arara (D465): páginas, figuras, conectores, geometría de los
 * conectores, historial de deshacer y formato de texto `.arara` (JSON sencillo).
 *
 * Todo en enteros (la libsagui no usa coma flotante, D076). Las coordenadas del mundo van en
 * [-MUNDO_MAX, MUNDO_MAX] para que los productos de las comprobaciones de acierto no se desborden.
 * El texto del documento es Latin-1 (como el resto del sistema); el archivo `.arara` es UTF-8. */
#ifndef ARARA_MODELO_H
#define ARARA_MODELO_H

#define MUNDO_MAX 15000
#define MAX_FIG   300
#define MAX_CON   300
#define MAX_PAG   8
#define TXT_FIG   96

/* ------------------------------------------------------------ tipos de figura */

enum {
    T_RECT, T_REDOND, T_ELIPSE, T_ROMBO, T_TRIANGULO, T_TEXTO,                        /* básicas */
    T_INICIO, T_PROCESO, T_DECISION, T_ES, T_DOCUMENTO, T_BD,                          /* diagrama de flujo */
    T_PC, T_SERVIDOR, T_ROUTER, T_NUBE, T_IMPRESORA,                                   /* red */
    T_PERSONA,                                                                         /* organigrama */
    T_N
};
enum { CAT_BASICAS, CAT_FLUJO, CAT_RED, CAT_ORGANI, CAT_N };

typedef struct {
    const char *clave;       /* nombre en el archivo .arara */
    const char *nombre;      /* nombre en la interfaz (se traduce con _()) */
    int cat, l, a;           /* paleta y tamaño por defecto (múltiplos de la rejilla de 16) */
    uint32_t relleno, linea; /* colores por defecto 0xAARRGGBB */
} tipo_info_t;

static const tipo_info_t TIPOS[T_N] = {
    { "rect",       N_("Rectángulo"),          CAT_BASICAS, 112,  64, 0xFFDCEBFA, 0xFF2F5F8F },
    { "redond",     N_("Rectángulo redondeado"), CAT_BASICAS, 112, 64, 0xFFE3F2E1, 0xFF3F7F3F },
    { "elipse",     N_("Elipse"),              CAT_BASICAS, 112,  64, 0xFFFDE8D0, 0xFFB5651D },
    { "rombo",      N_("Rombo"),               CAT_BASICAS, 112,  80, 0xFFFFF2B8, 0xFF9A7B00 },
    { "triangulo",  N_("Triángulo"),           CAT_BASICAS,  96,  80, 0xFFF3DDF3, 0xFF8A3F8A },
    { "texto",      N_("Texto"),               CAT_BASICAS, 112,  32, 0x00FFFFFF, 0x00000000 },
    { "inicio",     N_("Inicio / fin"),        CAT_FLUJO,   112,  48, 0xFFD5F0DD, 0xFF2E7D4B },
    { "proceso",    N_("Proceso"),             CAT_FLUJO,   112,  64, 0xFFDCEBFA, 0xFF2F5F8F },
    { "decision",   N_("Decisión"),            CAT_FLUJO,   128,  80, 0xFFFFF2B8, 0xFF9A7B00 },
    { "es",         N_("Entrada / salida"),    CAT_FLUJO,   128,  48, 0xFFE8E0F5, 0xFF5B3F9A },
    { "documento",  N_("Documento"),           CAT_FLUJO,   112,  64, 0xFFFFFFFF, 0xFF555555 },
    { "bd",         N_("Base de datos"),       CAT_FLUJO,    80,  96, 0xFFD6E4F0, 0xFF35566F },
    { "pc",         N_("PC"),                  CAT_RED,      80,  80, 0xFFC9D6E2, 0xFF3E4C59 },
    { "servidor",   N_("Servidor"),            CAT_RED,      80,  96, 0xFFC9D6E2, 0xFF3E4C59 },
    { "router",     N_("Router"),              CAT_RED,      96,  64, 0xFFBFD9F2, 0xFF2F5F8F },
    { "nube",       N_("Nube"),                CAT_RED,     112,  80, 0xFFE6F1FB, 0xFF5B8DB8 },
    { "impresora",  N_("Impresora"),           CAT_RED,      80,  80, 0xFFD9D9D9, 0xFF444444 },
    { "persona",    N_("Persona"),             CAT_ORGANI,  176,  64, 0xFFFFFFFF, 0xFF2F5F8F },
};
static const char *const CATEGORIAS[CAT_N] = { N_("Básicas"), N_("Diagrama de flujo"), N_("Red"), N_("Organigrama") };

static bool tipo_es_red(int t) { return t >= T_PC && t <= T_IMPRESORA; }

/* ------------------------------------------------------------ figuras, conectores y páginas */

typedef struct {
    int id, tipo, x, y, l, a, grosor;
    uint32_t relleno, linea;
    char texto[TXT_FIG];
    bool sel;                 /* no se guarda */
} fig_t;

enum { C_RECTO, C_ANGULO };
typedef struct {
    int id, tipo;
    int de, pde, a, pa;        /* figura (id, 0 = suelto) y punto de conexión 0 N, 1 E, 2 S, 3 O */
    int x0, y0, x1, y1;        /* extremos sueltos */
    bool flecha_ini, flecha_fin;
    uint32_t linea;
    int grosor;
    bool sel;
} con_t;

typedef struct {
    char nombre[32];
    fig_t *fig; int nfig;
    con_t *con; int ncon;
    int sig_id;
    int vzoom, vox, voy;       /* vista de esa página */
} pag_t;

static pag_t pag[MAX_PAG];
static int npag, cur;

static pag_t *P(void) { return &pag[cur]; }

static int limitar(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
static int mundo(int v) { return limitar(v, -MUNDO_MAX, MUNDO_MAX); }
static int fdivi(int a, int b) { int q = a / b; if ((a % b) < 0) q--; return q; }
static unsigned raiz_u(unsigned n)
{
    unsigned r = 0, b = 1u << 30;
    while (b > n) b >>= 2;
    while (b) {
        if (n >= r + b) { n -= r + b; r = (r >> 1) + b; } else r >>= 1;
        b >>= 2;
    }
    return r;
}

static bool pag_iniciar(pag_t *p, const char *nombre)
{
    memset(p, 0, sizeof *p);
    p->fig = aloca_zerado(sizeof(fig_t) * MAX_FIG);
    p->con = aloca_zerado(sizeof(con_t) * MAX_CON);
    if (!p->fig || !p->con) { libera(p->fig); libera(p->con); p->fig = NULL; p->con = NULL; return false; }
    copia_texto(p->nombre, nombre, sizeof p->nombre);
    p->sig_id = 1;
    p->vzoom = 1000; p->vox = 40; p->voy = 40;
    return true;
}
static void pag_liberar(pag_t *p)
{
    libera(p->fig); libera(p->con);
    memset(p, 0, sizeof *p);
}
static void doc_vaciar(void)
{
    for (int i = 0; i < npag; i++) pag_liberar(&pag[i]);
    npag = 0; cur = 0;
}
static bool doc_nuevo(void)
{
    doc_vaciar();
    if (!pag_iniciar(&pag[0], _("Página 1"))) return false;
    npag = 1;
    return true;
}

static int fig_idx(const pag_t *p, int id)
{
    for (int i = 0; i < p->nfig; i++) if (p->fig[i].id == id) return i;
    return -1;
}
static int con_idx(const pag_t *p, int id)
{
    for (int i = 0; i < p->ncon; i++) if (p->con[i].id == id) return i;
    return -1;
}

/* Una figura nueva con los valores por defecto de su tipo, al frente. Devuelve su índice o -1. */
static int fig_nueva(pag_t *p, int tipo, int x, int y)
{
    if (p->nfig >= MAX_FIG) return -1;
    fig_t *f = &p->fig[p->nfig];
    memset(f, 0, sizeof *f);
    f->id = p->sig_id++;
    f->tipo = tipo; f->l = TIPOS[tipo].l; f->a = TIPOS[tipo].a;
    f->x = mundo(x); f->y = mundo(y);
    f->relleno = TIPOS[tipo].relleno; f->linea = TIPOS[tipo].linea; f->grosor = tipo == T_TEXTO ? 1 : 2;
    return p->nfig++;
}

/* ------------------------------------------------------------ puntos de conexión y rutas */

/* Punto de conexión pt (0 N, 1 E, 2 S, 3 O) de una figura; dir = hacia dónde sale (0 arriba, 1 derecha, 2 abajo, 3 izquierda). */
static void fig_punto(const fig_t *f, int pt, int *x, int *y, int *dir)
{
    int cx = f->x + f->l / 2, cy = f->y + f->a / 2;
    int ia = tipo_es_red(f->tipo) ? f->a - 18 : f->a;     /* las figuras de red llevan su rótulo debajo */
    switch (pt & 3) {
    case 0: *x = cx; *y = f->y; break;
    case 1: *x = f->x + f->l; *y = tipo_es_red(f->tipo) ? f->y + ia / 2 : cy; if (f->tipo == T_TRIANGULO) *x = f->x + f->l * 3 / 4; break;
    case 2: *x = cx; *y = f->y + f->a; break;
    default: *x = f->x; *y = tipo_es_red(f->tipo) ? f->y + ia / 2 : cy; if (f->tipo == T_TRIANGULO) *x = f->x + f->l / 4; break;
    }
    if (dir) *dir = pt & 3;
}

static int dir_hacia(int dx, int dy)
{
    if (dx < 0 ? -dx > (dy < 0 ? -dy : dy) : dx > (dy < 0 ? -dy : dy)) return dx > 0 ? 1 : 3;
    return dy > 0 ? 2 : 0;
}

/* Extremos del conector: posición y dirección de salida de cada uno. */
static void con_extremos(const pag_t *p, const con_t *c, int *x0, int *y0, int *d0, int *x1, int *y1, int *d1)
{
    int i0 = c->de ? fig_idx(p, c->de) : -1, i1 = c->a ? fig_idx(p, c->a) : -1;
    if (i0 >= 0) fig_punto(&p->fig[i0], c->pde, x0, y0, d0); else { *x0 = c->x0; *y0 = c->y0; *d0 = -1; }
    if (i1 >= 0) fig_punto(&p->fig[i1], c->pa, x1, y1, d1); else { *x1 = c->x1; *y1 = c->y1; *d1 = -1; }
    if (*d0 < 0) *d0 = dir_hacia(*x1 - *x0, *y1 - *y0);
    if (*d1 < 0) *d1 = dir_hacia(*x0 - *x1, *y0 - *y1);
}

#define RUTA_MAX 8
#define TOPE_RUTA 16       /* tramo mínimo que sale de una figura antes de girar */

static void dir_vec(int d, int *dx, int *dy)
{
    *dx = d == 1 ? 1 : d == 3 ? -1 : 0;
    *dy = d == 2 ? 1 : d == 0 ? -1 : 0;
}

/* La poligonal del conector (hasta RUTA_MAX puntos). Recto: dos puntos. En ángulo recto: tramos horizontales y verticales. */
static int con_ruta(const pag_t *p, const con_t *c, int pts[][2])
{
    int x0, y0, d0, x1, y1, d1;
    con_extremos(p, c, &x0, &y0, &d0, &x1, &y1, &d1);
    int n = 0;
    pts[n][0] = x0; pts[n][1] = y0; n++;
    if (c->tipo == C_RECTO) { pts[n][0] = x1; pts[n][1] = y1; n++; return n; }
    int ax, ay, bx, by, dx, dy;
    dir_vec(d0, &dx, &dy); ax = x0 + dx * TOPE_RUTA; ay = y0 + dy * TOPE_RUTA;
    dir_vec(d1, &dx, &dy); bx = x1 + dx * TOPE_RUTA; by = y1 + dy * TOPE_RUTA;
    bool h0 = d0 == 1 || d0 == 3, h1 = d1 == 1 || d1 == 3;
    /* si el destino queda por delante de la salida no hace falta el tramo corto */
    if (h0 && h1) {
        if ((d0 == 1 && x1 >= x0 + 2 * TOPE_RUTA && d1 == 3) || (d0 == 3 && x1 <= x0 - 2 * TOPE_RUTA && d1 == 1)) { ax = x0; bx = x1; }
        int mx = (ax + bx) / 2;
        if (ax == x0 && bx == x1) {
            pts[n][0] = mx; pts[n][1] = y0; n++;
            pts[n][0] = mx; pts[n][1] = y1; n++;
        } else {
            pts[n][0] = ax; pts[n][1] = ay; n++;
            pts[n][0] = mx; pts[n][1] = ay; n++;
            pts[n][0] = mx; pts[n][1] = by; n++;
            pts[n][0] = bx; pts[n][1] = by; n++;
        }
    } else if (!h0 && !h1) {
        if ((d0 == 2 && y1 >= y0 + 2 * TOPE_RUTA && d1 == 0) || (d0 == 0 && y1 <= y0 - 2 * TOPE_RUTA && d1 == 2)) { ay = y0; by = y1; }
        int my = (ay + by) / 2;
        if (ay == y0 && by == y1) {
            pts[n][0] = x0; pts[n][1] = my; n++;
            pts[n][0] = x1; pts[n][1] = my; n++;
        } else {
            pts[n][0] = ax; pts[n][1] = ay; n++;
            pts[n][0] = ax; pts[n][1] = my; n++;
            pts[n][0] = bx; pts[n][1] = my; n++;
            pts[n][0] = bx; pts[n][1] = by; n++;
        }
    } else if (h0) {                                /* sale en horizontal y entra en vertical: una esquina */
        bool ok = (d0 == 1 ? x1 > x0 : x1 < x0) && (d1 == 0 ? y0 < y1 : y0 > y1);
        if (ok) { pts[n][0] = x1; pts[n][1] = y0; n++; }
        else {
            pts[n][0] = ax; pts[n][1] = ay; n++;
            pts[n][0] = ax; pts[n][1] = by; n++;
            pts[n][0] = bx; pts[n][1] = by; n++;
        }
    } else {
        bool ok = (d0 == 2 ? y1 > y0 : y1 < y0) && (d1 == 3 ? x0 > x1 : x0 < x1);
        if (ok) { pts[n][0] = x0; pts[n][1] = y1; n++; }
        else {
            pts[n][0] = ax; pts[n][1] = ay; n++;
            pts[n][0] = bx; pts[n][1] = ay; n++;
            pts[n][0] = bx; pts[n][1] = by; n++;
        }
    }
    pts[n][0] = x1; pts[n][1] = y1; n++;
    /* quita puntos repetidos y los que quedan en línea recta */
    int m = 1;
    for (int i = 1; i < n; i++) {
        if (pts[i][0] == pts[m - 1][0] && pts[i][1] == pts[m - 1][1]) continue;
        if (m >= 2) {
            int ux = pts[m - 1][0] - pts[m - 2][0], uy = pts[m - 1][1] - pts[m - 2][1], vx = pts[i][0] - pts[m - 1][0], vy = pts[i][1] - pts[m - 1][1];
            if ((ux == 0 && vx == 0 && (uy > 0) == (vy > 0)) || (uy == 0 && vy == 0 && (ux > 0) == (vx > 0))) { pts[m - 1][0] = pts[i][0]; pts[m - 1][1] = pts[i][1]; continue; }
        }
        pts[m][0] = pts[i][0]; pts[m][1] = pts[i][1]; m++;
    }
    return m < 2 ? 2 : m;
}

/* Punta de flecha en el extremo (px, py) de un tramo que viene de (qx, qy): triángulo de tres puntos. */
static void flecha_puntos(int qx, int qy, int px, int py, int grosor, int *xy)
{
    int dx = px - qx, dy = py - qy;
    unsigned l2 = (unsigned)(dx * dx) + (unsigned)(dy * dy);
    int len = (int)raiz_u(l2);
    if (len < 1) len = 1;
    int lg = 9 + grosor * 2, an = 4 + grosor;
    int ux = dx * 1000 / len, uy = dy * 1000 / len;          /* unitario x1000 */
    int bx = px - ux * lg / 1000, by = py - uy * lg / 1000;
    xy[0] = px; xy[1] = py;
    xy[2] = bx - uy * an / 1000; xy[3] = by + ux * an / 1000;
    xy[4] = bx + uy * an / 1000; xy[5] = by - ux * an / 1000;
}

/* ------------------------------------------------------------ aciertos */

static bool fig_acierta(const fig_t *f, int wx, int wy)
{
    if (wx < f->x || wy < f->y || wx > f->x + f->l || wy > f->y + f->a) return false;
    int rx = f->l / 2, ry = f->a / 2;
    if (rx < 1) rx = 1;
    if (ry < 1) ry = 1;
    int dx = wx - (f->x + rx), dy = wy - (f->y + ry);
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    switch (f->tipo) {
    case T_ELIPSE: {
        int u = dx * 1000 / rx, v = dy * 1000 / ry;
        return u * u + v * v <= 1000000;
    }
    case T_ROMBO: case T_DECISION: return dx * ry + dy * rx <= rx * ry;
    case T_TRIANGULO: return dx * f->a <= rx * (wy - f->y);
    default: return true;
    }
}

/* Distancia mínima (en unidades del mundo) del punto a un segmento, o -1 si supera tope. */
static bool segmento_cerca(int x0, int y0, int x1, int y1, int px, int py, int tope)
{
    int dx = x1 - x0, dy = y1 - y0;
    int cx = px - x0, cy = py - y0;
    if (dx == 0 && dy == 0) { unsigned d2 = (unsigned)(cx * cx) + (unsigned)(cy * cy); return d2 <= (unsigned)(tope * tope); }
    unsigned len = raiz_u((unsigned)(dx * dx) + (unsigned)(dy * dy));
    if (len < 1) len = 1;
    int dot = cx * dx + cy * dy;
    if (dot < -(int)(tope * len) || dot > (int)(len * len) + (int)(tope * len)) return false;
    int cruz = cx * dy - cy * dx;
    if (cruz < 0) cruz = -cruz;
    return cruz <= (int)(tope * len);
}

static bool con_acierta(const pag_t *p, const con_t *c, int wx, int wy, int tope)
{
    int pts[RUTA_MAX][2];
    int n = con_ruta(p, c, pts);
    for (int i = 0; i + 1 < n; i++) if (segmento_cerca(pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], wx, wy, tope)) return true;
    return false;
}

/* ------------------------------------------------------------ texto de las figuras: líneas que caben en cpl letras */

#define LIN_MAX 12
#define LIN_LONG 96
static int texto_lineas(const char *t, int cpl, char lin[][LIN_LONG])
{
    int n = 0;
    if (cpl < 1) cpl = 1;
    if (cpl > LIN_LONG - 1) cpl = LIN_LONG - 1;
    while (*t && n < LIN_MAX) {
        const char *fin = t;
        while (*fin && *fin != '\n') fin++;
        const char *q = t;
        if (fin == t) { lin[n][0] = 0; n++; }
        while (q < fin && n < LIN_MAX) {
            int resto = (int)(fin - q);
            int cuanto = resto <= cpl ? resto : cpl;
            if (resto > cpl) {                      /* corta en el último espacio que quepa */
                int e = -1;
                for (int k = cuanto; k > 0; k--) if (q[k] == ' ') { e = k; break; }
                if (e > 0) cuanto = e;
            }
            int cp = cuanto;
            memcpy(lin[n], q, (size_t)cp);
            lin[n][cp] = 0;
            n++;
            q += cuanto;
            while (q < fin && *q == ' ' && resto > cpl) q++;
        }
        t = *fin ? fin + 1 : fin;
    }
    return n;
}

/* ------------------------------------------------------------ búfer de texto que crece */

typedef struct { char *p; int n, cap; bool err; } buf_t;
static void buf_c(buf_t *b, const char *s, int n)
{
    if (b->err) return;
    if (b->n + n + 1 > b->cap) {
        int nc = b->cap ? b->cap * 2 : 4096;
        while (nc < b->n + n + 1) nc *= 2;
        char *np = realoca(b->p, (size_t)nc);
        if (!np) { b->err = true; return; }
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->n, s, (size_t)n);
    b->n += n;
    b->p[b->n] = 0;
}
static void buf_s(buf_t *b, const char *s) { buf_c(b, s, (int)tam_texto(s)); }
static void buf_f(buf_t *b, const char *fmt, ...)
{
    char t[512];
    va_list ap;
    va_start(ap, fmt);
    int n = formatar_lista(t, sizeof t, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof t - 1) n = (int)sizeof t - 1;
    if (n > 0) buf_c(b, t, n);
}

/* Latin-1 -> UTF-8 con las escapadas que pide JSON (json = true) o XML (json = false). */
static void buf_cadena(buf_t *b, const char *s, bool json)
{
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (json) {
            if (c == '"') buf_s(b, "\\\"");
            else if (c == '\\') buf_s(b, "\\\\");
            else if (c == '\n') buf_s(b, "\\n");
            else if (c < 32) buf_f(b, "\\u%04x", c);
            else if (c < 128) buf_c(b, (const char *)&c, 1);
            else { char u[2] = { (char)(0xC0 | (c >> 6)), (char)(0x80 | (c & 0x3F)) }; buf_c(b, u, 2); }
        } else {
            if (c == '&') buf_s(b, "&amp;");
            else if (c == '<') buf_s(b, "&lt;");
            else if (c == '>') buf_s(b, "&gt;");
            else if (c == '"') buf_s(b, "&quot;");
            else if (c < 32) buf_s(b, " ");
            else if (c < 128) buf_c(b, (const char *)&c, 1);
            else { char u[2] = { (char)(0xC0 | (c >> 6)), (char)(0x80 | (c & 0x3F)) }; buf_c(b, u, 2); }
        }
    }
}

/* ------------------------------------------------------------ formato .arara (JSON) */

static void json_color(buf_t *b, uint32_t c)
{
    if ((c >> 24) == 0xFF) buf_f(b, "\"#%06x\"", c & 0xFFFFFF);
    else buf_f(b, "\"#%08x\"", c);
}

/* Escribe el documento. pagina < 0: todas las páginas; si no, solo la pedida (sirve para copiar). */
static void arara_serializar(buf_t *b)
{
    buf_s(b, "{\n  \"arara\": 1,\n  \"paginas\": [\n");
    for (int k = 0; k < npag; k++) {
        const pag_t *p = &pag[k];
        buf_s(b, "    {\n      \"nombre\": \""); buf_cadena(b, p->nombre, true); buf_s(b, "\",\n      \"figuras\": [\n");
        for (int i = 0; i < p->nfig; i++) {
            const fig_t *f = &p->fig[i];
            buf_f(b, "        { \"id\": %d, \"tipo\": \"%s\", \"x\": %d, \"y\": %d, \"l\": %d, \"a\": %d, \"relleno\": ", f->id, TIPOS[f->tipo].clave, f->x, f->y, f->l, f->a);
            json_color(b, f->relleno);
            buf_s(b, ", \"linea\": "); json_color(b, f->linea);
            buf_f(b, ", \"grosor\": %d, \"texto\": \"", f->grosor);
            buf_cadena(b, f->texto, true);
            buf_s(b, i + 1 < p->nfig ? "\" },\n" : "\" }\n");
        }
        buf_s(b, "      ],\n      \"conectores\": [\n");
        for (int i = 0; i < p->ncon; i++) {
            const con_t *c = &p->con[i];
            buf_f(b, "        { \"id\": %d, \"tipo\": \"%s\"", c->id, c->tipo == C_ANGULO ? "angulo" : "recto");
            if (c->de) buf_f(b, ", \"de\": %d, \"pto_de\": %d", c->de, c->pde); else buf_f(b, ", \"x0\": %d, \"y0\": %d", c->x0, c->y0);
            if (c->a) buf_f(b, ", \"a\": %d, \"pto_a\": %d", c->a, c->pa); else buf_f(b, ", \"x1\": %d, \"y1\": %d", c->x1, c->y1);
            buf_f(b, ", \"flecha_ini\": %s, \"flecha_fin\": %s, \"linea\": ", c->flecha_ini ? "true" : "false", c->flecha_fin ? "true" : "false");
            json_color(b, c->linea);
            buf_f(b, ", \"grosor\": %d }%s\n", c->grosor, i + 1 < p->ncon ? "," : "");
        }
        buf_f(b, "      ]\n    }%s\n", k + 1 < npag ? "," : "");
    }
    buf_s(b, "  ]\n}\n");
}

/* --- lector: un árbol pequeño en memoria --- */
enum { J_NULO, J_BOOL, J_INT, J_CAD, J_OBJ, J_ARR };
typedef struct { int tipo; int v; int hijo, sig; int clave; } jn_t;     /* v: valor, o desplazamiento en el pozo de texto */
typedef struct { jn_t *n; int nn, cap; char *pozo; int np, cpozo; const char *s; int i, prof; bool err; } json_t;

static int j_nodo(json_t *j, int tipo, int v)
{
    if (j->nn >= j->cap) {
        int nc = j->cap ? j->cap * 2 : 256;
        jn_t *nn = realoca(j->n, sizeof(jn_t) * (size_t)nc);
        if (!nn) { j->err = true; return 0; }
        j->n = nn; j->cap = nc;
    }
    jn_t *q = &j->n[j->nn];
    q->tipo = tipo; q->v = v; q->hijo = -1; q->sig = -1; q->clave = -1;
    return j->nn++;
}
static int j_pozo(json_t *j, const char *t, int n)    /* guarda n bytes y un cero; devuelve el desplazamiento */
{
    if (j->np + n + 1 > j->cpozo) {
        int nc = j->cpozo ? j->cpozo * 2 : 4096;
        while (nc < j->np + n + 1) nc *= 2;
        char *np = realoca(j->pozo, (size_t)nc);
        if (!np) { j->err = true; return 0; }
        j->pozo = np; j->cpozo = nc;
    }
    memcpy(j->pozo + j->np, t, (size_t)n);
    j->pozo[j->np + n] = 0;
    int r = j->np;
    j->np += n + 1;
    return r;
}
static void j_blancos(json_t *j) { while (j->s[j->i] == ' ' || j->s[j->i] == '\n' || j->s[j->i] == '\r' || j->s[j->i] == '\t') j->i++; }

static int j_hex(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

/* Cadena JSON (UTF-8) -> Latin-1; devuelve el desplazamiento en el pozo, o -1. */
static int j_cadena(json_t *j)
{
    if (j->s[j->i] != '"') { j->err = true; return -1; }
    j->i++;
    char tmp[1024];
    int n = 0;
    while (j->s[j->i] && j->s[j->i] != '"') {
        unsigned char c = (unsigned char)j->s[j->i++];
        int ch = c;
        if (c == '\\') {
            char e = j->s[j->i++];
            if (e == 'n') ch = '\n'; else if (e == 't') ch = '\t'; else if (e == 'r') ch = '\r';
            else if (e == 'u') {
                int v = 0;
                for (int k = 0; k < 4; k++) { int h = j_hex(j->s[j->i]); if (h < 0) { j->err = true; return -1; } v = v * 16 + h; j->i++; }
                ch = v <= 0xFF ? v : '?';
            } else ch = (unsigned char)e;
        } else if (c >= 0xC0 && c < 0xE0 && ((unsigned char)j->s[j->i] & 0xC0) == 0x80) {
            ch = ((c & 0x1F) << 6) | ((unsigned char)j->s[j->i++] & 0x3F);
            if (ch > 0xFF) ch = '?';
        } else if (c >= 0xE0) {                          /* otros caracteres Unicode: no caben en Latin-1 */
            int extra = c >= 0xF0 ? 3 : 2;
            while (extra-- > 0 && ((unsigned char)j->s[j->i] & 0xC0) == 0x80) j->i++;
            ch = '?';
        }
        if (n < (int)sizeof tmp - 1) tmp[n++] = (char)ch;
    }
    if (j->s[j->i] != '"') { j->err = true; return -1; }
    j->i++;
    return j_pozo(j, tmp, n);
}

static int j_valor(json_t *j)
{
    if (j->err || j->prof > 12) { j->err = true; return 0; }
    j_blancos(j);
    char c = j->s[j->i];
    if (c == '"') { int o = j_cadena(j); return j_nodo(j, J_CAD, o); }
    if (c == '{' || c == '[') {
        bool obj = c == '{';
        char cierre = obj ? '}' : ']';
        int me = j_nodo(j, obj ? J_OBJ : J_ARR, 0);
        j->i++; j->prof++;
        int ult = -1;
        for (;;) {
            j_blancos(j);
            if (j->s[j->i] == cierre) { j->i++; break; }
            if (!j->s[j->i]) { j->err = true; break; }
            int clave = -1;
            if (obj) {
                clave = j_cadena(j);
                j_blancos(j);
                if (j->s[j->i] != ':') { j->err = true; break; }
                j->i++;
            }
            int h = j_valor(j);
            if (j->err) break;
            j->n[h].clave = clave;
            if (ult < 0) j->n[me].hijo = h; else j->n[ult].sig = h;
            ult = h;
            j_blancos(j);
            if (j->s[j->i] == ',') j->i++;
            else if (j->s[j->i] != cierre) { j->err = true; break; }
        }
        j->prof--;
        return me;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
        int neg = 0, v = 0;
        if (c == '-') { neg = 1; j->i++; }
        while (j->s[j->i] >= '0' && j->s[j->i] <= '9') { if (v < 100000000) v = v * 10 + (j->s[j->i] - '0'); j->i++; }
        if (j->s[j->i] == '.') { j->i++; while (j->s[j->i] >= '0' && j->s[j->i] <= '9') j->i++; }
        if (j->s[j->i] == 'e' || j->s[j->i] == 'E') { j->i++; if (j->s[j->i] == '+' || j->s[j->i] == '-') j->i++; while (j->s[j->i] >= '0' && j->s[j->i] <= '9') j->i++; }
        return j_nodo(j, J_INT, neg ? -v : v);
    }
    if (comeca_com(j->s + j->i, "true")) { j->i += 4; return j_nodo(j, J_BOOL, 1); }
    if (comeca_com(j->s + j->i, "false")) { j->i += 5; return j_nodo(j, J_BOOL, 0); }
    if (comeca_com(j->s + j->i, "null")) { j->i += 4; return j_nodo(j, J_NULO, 0); }
    j->err = true;
    return 0;
}

static int j_buscar(const json_t *j, int obj, const char *clave)
{
    if (obj < 0 || j->n[obj].tipo != J_OBJ) return -1;
    for (int h = j->n[obj].hijo; h >= 0; h = j->n[h].sig) if (j->n[h].clave >= 0 && compara_texto(j->pozo + j->n[h].clave, clave) == 0) return h;
    return -1;
}
static int j_int(const json_t *j, int obj, const char *clave, int defecto)
{
    int h = j_buscar(j, obj, clave);
    return h >= 0 && (j->n[h].tipo == J_INT || j->n[h].tipo == J_BOOL) ? j->n[h].v : defecto;
}
static const char *j_cad(const json_t *j, int obj, const char *clave, const char *defecto)
{
    int h = j_buscar(j, obj, clave);
    return h >= 0 && j->n[h].tipo == J_CAD ? j->pozo + j->n[h].v : defecto;
}
static uint32_t j_color(const json_t *j, int obj, const char *clave, uint32_t defecto)
{
    const char *s = j_cad(j, obj, clave, NULL);
    if (!s || s[0] != '#') return defecto;
    int n = (int)tam_texto(s) - 1;
    uint32_t v = 0;
    for (int i = 1; s[i]; i++) { int h = j_hex(s[i]); if (h < 0) return defecto; v = v * 16 + (uint32_t)h; }
    if (n == 6) return 0xFF000000u | v;
    if (n == 8) return v;
    return defecto;
}
static int tipo_por_clave(const char *c)
{
    for (int i = 0; i < T_N; i++) if (compara_texto(TIPOS[i].clave, c) == 0) return i;
    return -1;
}

/* Carga un documento desde texto. 0 = bien; en caso de error el documento anterior queda como estaba. */
static int arara_cargar(const char *texto)
{
    json_t j;
    memset(&j, 0, sizeof j);
    j.s = texto;
    int raiz = j_valor(&j);
    int rc = 0;
    int pags = j.err ? -1 : j_buscar(&j, raiz, "paginas");
    if (j.err || pags < 0 || j.n[pags].tipo != J_ARR || j_int(&j, raiz, "arara", 0) != 1) rc = -1;
    pag_t nuevo[MAX_PAG];
    int nn = 0;
    if (rc == 0) {
        for (int h = j.n[pags].hijo; h >= 0 && nn < MAX_PAG; h = j.n[h].sig) {
            if (!pag_iniciar(&nuevo[nn], j_cad(&j, h, "nombre", "Página"))) { rc = -2; break; }
            pag_t *p = &nuevo[nn++];
            int figs = j_buscar(&j, h, "figuras"), cons = j_buscar(&j, h, "conectores");
            for (int q = figs >= 0 ? j.n[figs].hijo : -1; q >= 0 && p->nfig < MAX_FIG; q = j.n[q].sig) {
                int t = tipo_por_clave(j_cad(&j, q, "tipo", ""));
                if (t < 0) continue;
                fig_t *f = &p->fig[p->nfig++];
                memset(f, 0, sizeof *f);
                f->id = j_int(&j, q, "id", p->sig_id);
                f->tipo = t;
                f->x = mundo(j_int(&j, q, "x", 0)); f->y = mundo(j_int(&j, q, "y", 0));
                f->l = limitar(j_int(&j, q, "l", TIPOS[t].l), 8, 4000); f->a = limitar(j_int(&j, q, "a", TIPOS[t].a), 8, 4000);
                f->relleno = j_color(&j, q, "relleno", TIPOS[t].relleno);
                f->linea = j_color(&j, q, "linea", TIPOS[t].linea);
                f->grosor = limitar(j_int(&j, q, "grosor", 2), 1, 12);
                copia_texto(f->texto, j_cad(&j, q, "texto", ""), sizeof f->texto);
                if (f->id >= p->sig_id) p->sig_id = f->id + 1;
            }
            for (int q = cons >= 0 ? j.n[cons].hijo : -1; q >= 0 && p->ncon < MAX_CON; q = j.n[q].sig) {
                con_t *c = &p->con[p->ncon++];
                memset(c, 0, sizeof *c);
                c->id = j_int(&j, q, "id", p->sig_id);
                c->tipo = compara_texto(j_cad(&j, q, "tipo", "recto"), "angulo") == 0 ? C_ANGULO : C_RECTO;
                c->de = j_int(&j, q, "de", 0); c->pde = j_int(&j, q, "pto_de", 0) & 3;
                c->a = j_int(&j, q, "a", 0); c->pa = j_int(&j, q, "pto_a", 0) & 3;
                if (c->de && fig_idx(p, c->de) < 0) c->de = 0;
                if (c->a && fig_idx(p, c->a) < 0) c->a = 0;
                c->x0 = mundo(j_int(&j, q, "x0", 0)); c->y0 = mundo(j_int(&j, q, "y0", 0));
                c->x1 = mundo(j_int(&j, q, "x1", 0)); c->y1 = mundo(j_int(&j, q, "y1", 0));
                c->flecha_ini = j_int(&j, q, "flecha_ini", 0) != 0; c->flecha_fin = j_int(&j, q, "flecha_fin", 0) != 0;
                c->linea = j_color(&j, q, "linea", 0xFF2F4F6F);
                c->grosor = limitar(j_int(&j, q, "grosor", 2), 1, 12);
                if (c->id >= p->sig_id) p->sig_id = c->id + 1;
            }
        }
        if (nn == 0) rc = -1;
    }
    libera(j.n); libera(j.pozo);
    if (rc != 0) { for (int i = 0; i < nn; i++) pag_liberar(&nuevo[i]); return rc; }
    doc_vaciar();
    for (int i = 0; i < nn; i++) pag[i] = nuevo[i];
    npag = nn; cur = 0;
    return 0;
}

/* ------------------------------------------------------------ historial: copias del documento */

typedef struct snap {
    int ref, npag, cur;
    struct { char nombre[32]; int nfig, ncon, sig_id; fig_t *fig; con_t *con; } p[MAX_PAG];
} snap_t;

static snap_t *snap_tomar(void)
{
    snap_t *s = aloca_zerado(sizeof *s);
    if (!s) return NULL;
    s->ref = 1; s->npag = npag; s->cur = cur;
    for (int i = 0; i < npag; i++) {
        copia_texto(s->p[i].nombre, pag[i].nombre, sizeof s->p[i].nombre);
        s->p[i].nfig = pag[i].nfig; s->p[i].ncon = pag[i].ncon; s->p[i].sig_id = pag[i].sig_id;
        s->p[i].fig = aloca(sizeof(fig_t) * (size_t)(pag[i].nfig + 1));
        s->p[i].con = aloca(sizeof(con_t) * (size_t)(pag[i].ncon + 1));
        if (!s->p[i].fig || !s->p[i].con) {
            for (int k = 0; k <= i; k++) { libera(s->p[k].fig); libera(s->p[k].con); }
            libera(s);
            return NULL;
        }
        memcpy(s->p[i].fig, pag[i].fig, sizeof(fig_t) * (size_t)pag[i].nfig);
        memcpy(s->p[i].con, pag[i].con, sizeof(con_t) * (size_t)pag[i].ncon);
    }
    return s;
}
static snap_t *snap_ref(snap_t *s) { if (s) s->ref++; return s; }
static void snap_soltar(snap_t *s)
{
    if (!s || --s->ref > 0) return;
    for (int i = 0; i < s->npag; i++) { libera(s->p[i].fig); libera(s->p[i].con); }
    libera(s);
}
/* Deja el documento como estaba en la copia (las vistas de las páginas que siguen existiendo se conservan). */
static void snap_aplicar(const snap_t *s)
{
    for (int i = 0; i < s->npag; i++) {
        if (i >= npag && !pag_iniciar(&pag[i], s->p[i].nombre)) return;
        pag_t *p = &pag[i];
        copia_texto(p->nombre, s->p[i].nombre, sizeof p->nombre);
        p->nfig = s->p[i].nfig; p->ncon = s->p[i].ncon; p->sig_id = s->p[i].sig_id;
        memcpy(p->fig, s->p[i].fig, sizeof(fig_t) * (size_t)p->nfig);
        memcpy(p->con, s->p[i].con, sizeof(con_t) * (size_t)p->ncon);
        for (int k = 0; k < p->nfig; k++) p->fig[k].sel = false;
        for (int k = 0; k < p->ncon; k++) p->con[k].sel = false;
    }
    for (int i = s->npag; i < npag; i++) pag_liberar(&pag[i]);
    npag = s->npag;
    if (cur >= npag) cur = npag - 1;
}

#endif
