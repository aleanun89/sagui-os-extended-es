// SPDX-License-Identifier: GPL-3.0-or-later
// GebraMini, © 2026 Alejandro Anuncibay: prototipo original (C++/SDL) del mantenedor; esta versión nativa
// para SaguiOS está escrita desde cero a partir de sus ideas. Independiente de GeoGebra.
/* gbm_motor.c - objetos geométricos y grafo de dependencias de GebraMini (D309).
 *
 * Cada objeto guarda de qué depende (los puntos o los objetos de los que sale: "pai") y/o una
 * expresión (el árbol de gbm_expr.c, que a su vez hace referencia a otros objetos por su índice).
 * El recálculo es perezoso y con memoria: calc(i) calcula primero lo que i necesita, así que da
 * igual el orden en que estén creados, y un ciclo (A depende de B y B de A) se detecta al vuelo.
 *
 * Contenido de v[] según el tipo:
 *   puntos            v0, v1 = x, y
 *   rectas, segmentos... v0, v1 = P0 y v2, v3 = P1 (la recta pasa por los dos; el vector va de P0 a P1)
 *   circunferencias   v0, v1 = centro y v2 = radio
 *   polígonos         v0 = área y v1 = perímetro
 *   números           v0 (el ángulo, en radianes)
 */
#include "gbm.h"
#include "gbm_mat.h"

gbm_obj_t gbm_obj[GBM_OBJ_MAX];
int gbm_n;
int gbm_decimais = 2;

static bool ciclo;
static const char *nome_pedido;      /* nombre que reclama el próximo objeto creado ("A = ...") */

#define LIM_X 30.0                   /* intervalo donde se buscan raíces y cortes de funciones */

/* ------------------------------------------------------------ clasificación */

bool gbm_eh_ponto(int t) { return t >= G_PTO_LIVRE && t <= G_PTO_EXTR; }
bool gbm_eh_reta(int t) { return t >= G_RETA && t <= G_BISSETRIZ; }
bool gbm_eh_circ(int t) { return t >= G_CIRC_PP && t <= G_CIRC_3; }
bool gbm_eh_poligono(int t) { return t == G_POLIGONO || t == G_POLREG; }
bool gbm_eh_num(int t) { return t >= G_ANGULO && t <= G_SLIDER; }

static gbm_obj_t *O(int i) { return &gbm_obj[i]; }
static bool valido(int i) { return i >= 0 && i < gbm_n; }

static int limite_reta(int t)   /* 0 recta, 1 semirrecta, 2 segmento */
{
    return t == G_SEMIRRETA ? 1 : (t == G_SEGMENTO || t == G_VETOR) ? 2 : 0;
}

/* ------------------------------------------------------------ nombres */

static const char *const COMANDOS[] = {
    "Punto", "PuntoEn", "PuntoMedio", "Interseca", "Recta", "Semirrecta", "Segmento", "Vector", "Circunferencia",
    "Perpendicular", "Paralela", "Mediatriz", "Bisectriz", "Poligono", "Angulo", "Distancia", "Longitud", "Area",
    "Texto", "Deslizador", "Derivada", "Raiz", "Extremo",
};
#define NCOM ((int)(sizeof COMANDOS / sizeof COMANDOS[0]))

static int nrm(int c)
{
    unsigned char u = (unsigned char)c;
    if (u >= 'A' && u <= 'Z') return u + 32;
    if (u >= 0xC0 && u <= 0xC5) return 'a';
    if (u >= 0xE0 && u <= 0xE5) return 'a';
    if (u >= 0xC8 && u <= 0xCB) return 'e';
    if (u >= 0xE8 && u <= 0xEB) return 'e';
    if (u >= 0xCC && u <= 0xCF) return 'i';
    if (u >= 0xEC && u <= 0xEF) return 'i';
    if (u >= 0xD2 && u <= 0xD6) return 'o';
    if (u >= 0xF2 && u <= 0xF6) return 'o';
    if (u >= 0xD9 && u <= 0xDC) return 'u';
    if (u >= 0xF9 && u <= 0xFC) return 'u';
    if (u == 0xD1 || u == 0xF1) return 'n';
    return u;
}
static bool igual_n(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) if (nrm(*a) != nrm(*b)) return false;
    return *a == 0 && *b == 0;
}
static bool prefijo_n(const char *p, const char *t)   /* ¿p es el comienzo de t, sin distinguir tildes ni mayúsculas? */
{
    for (; *p && *t; p++, t++) if (nrm(*p) != nrm(*t)) return false;
    return *p == 0;
}

int gbm_comandos(const char **lista, int max)
{
    static const char *const bonitos[] = {
        "Punto", "PuntoEn", "PuntoMedio", "Interseca", "Recta", "Semirrecta", "Segmento", "Vector", "Circunferencia",
        "Perpendicular", "Paralela", "Mediatriz", "Bisectriz", "Pol\xED" "gono", "\xC1ngulo", "Distancia", "Longitud", "\xC1rea",
        "Texto", "Deslizador", "Derivada", "Ra\xED" "z", "Extremo",
    };
    int n = 0;
    for (; n < NCOM && n < max; n++) lista[n] = bonitos[n];
    return n;
}

static int comando_id(const char *nome)
{
    for (int i = 0; i < NCOM; i++) if (igual_n(nome, COMANDOS[i])) return i;
    if (igual_n(nome, "Punto")) return 0;
    if (igual_n(nome, "Interseccion")) return 3;
    if (igual_n(nome, "Circulo")) return 8;
    if (igual_n(nome, "Raices")) return 21;
    if (igual_n(nome, "Extremos")) return 22;
    if (igual_n(nome, "Midpoint")) return 2;
    return -1;
}

static bool nome_funcao_ou_const(const char *n)
{
    if (compara_texto(n, "x") == 0 || compara_texto(n, "y") == 0 || compara_texto(n, "e") == 0 || compara_texto(n, "pi") == 0) return true;
    static const char *const r[] = { "sin", "sen", "cos", "tan", "tg", "asin", "acos", "atan", "sinh", "cosh", "tanh",
        "exp", "ln", "log", "log10", "log2", "sqrt", "cbrt", "abs", "floor", "ceil", "round", "sign", "atan2", "min", "max", "mod",
        "hypot", "pow", "abscisa", "ordenada" };
    for (unsigned i = 0; i < sizeof r / sizeof r[0]; i++) if (igual_n(n, r[i])) return true;
    return false;
}

static bool letra(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || ((unsigned char)c >= 0xC0 && (unsigned char)c != 0xD7 && (unsigned char)c != 0xF7); }
static bool digito(int c) { return c >= '0' && c <= '9'; }

static bool nome_valido(const char *n)
{
    if (!n[0] || !letra(n[0])) return false;
    int len = 0;
    for (const char *p = n; *p; p++, len++) if (!letra(*p) && !digito(*p)) return false;
    if (len >= GBM_NOME) return false;
    if (nome_funcao_ou_const(n) || comando_id(n) >= 0) return false;
    return true;
}

int gbm_buscar(const char *nome)
{
    for (int i = 0; i < gbm_n; i++) if (compara_texto(gbm_obj[i].nome, nome) == 0) return i;
    return -1;
}
const char *gbm_nome(int i) { return valido(i) ? gbm_obj[i].nome : "?"; }

bool gbm_renomear(int i, const char *novo)
{
    if (!valido(i) || !nome_valido(novo)) return false;
    int j = gbm_buscar(novo);
    if (j >= 0 && j != i) return false;
    copia_texto(gbm_obj[i].nome, novo, GBM_NOME);
    return true;
}

static void nome_livre(int tipo, char *s)
{
    char t[GBM_NOME];
    if (gbm_eh_ponto(tipo)) {
        for (int ronda = 0; ronda < 40; ronda++)
            for (char c = 'A'; c <= 'Z'; c++) {
                if (ronda) formatar_texto(t, sizeof t, "%c%d", c, ronda); else { t[0] = c; t[1] = 0; }
                if (gbm_buscar(t) < 0) { copia_texto(s, t, GBM_NOME); return; }
            }
    } else if (tipo == G_FUNCAO) {
        static const char fs[] = "fgh";
        for (int ronda = 0; ronda < 40; ronda++)
            for (int k = 0; k < 3; k++) {
                if (ronda) formatar_texto(t, sizeof t, "%c%d", fs[k], ronda); else { t[0] = fs[k]; t[1] = 0; }
                if (gbm_buscar(t) < 0) { copia_texto(s, t, GBM_NOME); return; }
            }
    } else if (tipo == G_ANGULO) {
        static const char *const gr[] = { "alfa", "beta", "gama", "delta", "epsilon", "zeta", "eta", "theta" };
        for (int ronda = 0; ronda < 40; ronda++)
            for (int k = 0; k < 8; k++) {
                if (ronda) formatar_texto(t, sizeof t, "%s%d", gr[k], ronda); else copia_texto(t, gr[k], sizeof t);
                if (gbm_buscar(t) < 0) { copia_texto(s, t, GBM_NOME); return; }
            }
    } else {
        static const char az[] = "abcdijklmnopqrstuvwz";
        for (int ronda = 0; ronda < 40; ronda++)
            for (int k = 0; az[k]; k++) {
                if (ronda) formatar_texto(t, sizeof t, "%c%d", az[k], ronda); else { t[0] = az[k]; t[1] = 0; }
                if (gbm_buscar(t) < 0) { copia_texto(s, t, GBM_NOME); return; }
            }
    }
    formatar_texto(s, GBM_NOME, "obj%d", gbm_n);
}

/* ------------------------------------------------------------ creación */

int gbm_cor_padrao(int tipo, int ordem)
{
    static const uint32_t fun[] = { 0xCC2222, 0x228822, 0x7722AA, 0xBB6600, 0x0088AA };
    if (tipo == G_PTO_LIVRE || tipo == G_PTO_EM) return 0x2255CC;
    if (gbm_eh_ponto(tipo)) return 0x444444;
    if (tipo == G_FUNCAO) return (int)fun[ordem % 5];
    if (gbm_eh_poligono(tipo)) return 0x8A4B00;
    if (tipo == G_ANGULO) return 0x007744;
    if (tipo == G_TEXTO || gbm_eh_num(tipo)) return 0x222222;
    if (gbm_eh_circ(tipo)) return 0x1F3F7F;
    return 0x222222;
}

static void def_prop(gbm_obj_t *o)
{
    o->visivel = 1;
    o->estilo = 0;
    o->grosor = gbm_eh_ponto(o->tipo) ? 3 : 2;
    o->rotulo = gbm_eh_ponto(o->tipo) || o->tipo == G_ANGULO || o->tipo == G_DIST || o->tipo == G_AREA || o->tipo == G_NUM_EXPR;
    if (o->tipo == G_TEXTO || o->tipo == G_SLIDER) o->rotulo = 0;
    int ord = 0;
    for (int i = 0; i < gbm_n; i++) if (gbm_obj[i].tipo == G_FUNCAO) ord++;
    o->cor = (uint32_t)gbm_cor_padrao(o->tipo, ord > 0 ? ord - 1 : 0);
}

static int nova(int tipo)
{
    if (gbm_n >= GBM_OBJ_MAX) { copia_texto(gbm_erro, "Demasiados objetos", sizeof gbm_erro); return -1; }
    gbm_obj_t *o = &gbm_obj[gbm_n];
    memset(o, 0, sizeof *o);
    o->tipo = (uint8_t)tipo;
    o->raiz = -1;
    if (nome_pedido) { copia_texto(o->nome, nome_pedido, GBM_NOME); nome_pedido = NULL; }
    else nome_livre(tipo, o->nome);
    gbm_n++;
    def_prop(o);
    return gbm_n - 1;
}

void gbm_limpar(void)
{
    gbm_n = 0;
    gbm_nodos_reiniciar();
    nome_pedido = NULL;
}

/* ------------------------------------------------------------ cálculo */

static void calc(int i);

static bool ptc(int i, double *x, double *y)
{
    if (!valido(i)) return false;
    calc(i);
    gbm_obj_t *o = O(i);
    if (!o->ok || !gbm_eh_ponto(o->tipo)) return false;
    *x = o->v[0]; *y = o->v[1];
    return true;
}

bool gbm_ponto_xy(int i, double *x, double *y) { return ptc(i, x, y); }

bool gbm_ref_valor(int idx, gbm_val_t *v)
{
    if (!valido(idx)) return false;
    calc(idx);
    gbm_obj_t *o = O(idx);
    if (!o->ok) return false;
    if (gbm_eh_ponto(o->tipo)) { *v = (gbm_val_t){ o->v[0], o->v[1], 1 }; return true; }
    if (o->tipo == G_VETOR) { *v = (gbm_val_t){ o->v[2] - o->v[0], o->v[3] - o->v[1], 1 }; return true; }
    if (gbm_eh_num(o->tipo)) { *v = (gbm_val_t){ o->v[0], 0, 0 }; return true; }
    return false;
}

double gbm_valor_funcao(int i, double x)
{
    if (!valido(i) || gbm_obj[i].tipo != G_FUNCAO) return GM_NAN;
    calc(i);
    if (!gbm_obj[i].ok) return GM_NAN;
    gbm_val_t v = gbm_avaliar(gbm_obj[i].raiz, x);
    return v.vec ? GM_NAN : v.a;
}

/* ---- primitivas para los cortes ---- */
enum { P_RETA, P_SEMI, P_SEG, P_CIRC, P_FUNC };
typedef struct { int k; double x0, y0, x1, y1, r; int fobj; } prim_t;

static int vert_raw(gbm_obj_t *o, double *xs, double *ys, int max)
{
    if (o->tipo == G_POLIGONO) {
        int n = 0;
        for (int k = 0; k < o->npai && n < max; k++) if (ptc(o->pai[k], &xs[n], &ys[n])) n++;
        return n;
    }
    if (o->tipo == G_POLREG) {
        double ax, ay, bx, by;
        if (!ptc(o->pai[0], &ax, &ay) || !ptc(o->pai[1], &bx, &by)) return 0;
        int n = o->k < 3 ? 3 : o->k > 64 ? 64 : o->k;
        if (n > max) n = max;
        double ex = bx - ax, ey = by - ay, c = gm_cos(2 * GM_PI / o->k), s = gm_sin(2 * GM_PI / o->k), x = ax, y = ay;
        for (int k = 0; k < n; k++) {
            xs[k] = x; ys[k] = y;
            x += ex; y += ey;
            double nx = ex * c - ey * s, ny = ex * s + ey * c;
            ex = nx; ey = ny;
        }
        return n;
    }
    return 0;
}

int gbm_vertices(int i, double *xs, double *ys, int max)
{
    if (!valido(i)) return 0;
    calc(i);
    if (!O(i)->ok) return 0;
    return vert_raw(O(i), xs, ys, max);
}

static int prims(int i, prim_t *p, int max)
{
    if (!valido(i)) return 0;
    gbm_obj_t *o = O(i);
    calc(i);
    if (!o->ok) return 0;
    if (gbm_eh_reta(o->tipo) && max >= 1) {
        p[0] = (prim_t){ limite_reta(o->tipo) == 0 ? P_RETA : limite_reta(o->tipo) == 1 ? P_SEMI : P_SEG, o->v[0], o->v[1], o->v[2], o->v[3], 0, -1 };
        return 1;
    }
    if (gbm_eh_circ(o->tipo) && max >= 1) { p[0] = (prim_t){ P_CIRC, o->v[0], o->v[1], 0, 0, o->v[2], -1 }; return 1; }
    if (o->tipo == G_FUNCAO && max >= 1) { p[0] = (prim_t){ P_FUNC, 0, 0, 0, 0, 0, i }; return 1; }
    if (gbm_eh_poligono(o->tipo)) {
        double xs[64], ys[64];
        int n = gbm_vertices(i, xs, ys, 64), q = 0;
        for (int k = 0; k < n && q < max; k++) p[q++] = (prim_t){ P_SEG, xs[k], ys[k], xs[(k + 1) % n], ys[(k + 1) % n], 0, -1 };
        return q;
    }
    return 0;
}

static bool dentro_limite(int k, double t)
{
    const double e = 1e-9;
    if (k == P_SEMI) return t >= -e;
    if (k == P_SEG) return t >= -e && t <= 1 + e;
    return true;
}

typedef struct { prim_t f; prim_t o; } corte_ctx_t;
static double fn_corte(void *c, double x)
{
    corte_ctx_t *cc = c;
    double fx = gbm_valor_funcao(cc->f.fobj, x);
    if (cc->o.k == P_FUNC) return fx - gbm_valor_funcao(cc->o.fobj, x);
    if (cc->o.k == P_CIRC) { double dx = x - cc->o.x0, dy = fx - cc->o.y0; return dx * dx + dy * dy - cc->o.r * cc->o.r; }
    double dx = cc->o.x1 - cc->o.x0, dy = cc->o.y1 - cc->o.y0;
    return fx - (cc->o.y0 + dy / dx * (x - cc->o.x0));
}

/* Puntos de corte de dos primitivas. */
static int corta(prim_t a, prim_t b, double *xs, double *ys, int max)
{
    int n = 0;
    if (a.k > b.k) { prim_t t = a; a = b; b = t; }
    if (a.k <= P_SEG && b.k <= P_SEG) {
        double d1x = a.x1 - a.x0, d1y = a.y1 - a.y0, d2x = b.x1 - b.x0, d2y = b.y1 - b.y0;
        double den = d1x * d2y - d1y * d2x;
        double ln = gm_sqrt(d1x * d1x + d1y * d1y) * gm_sqrt(d2x * d2x + d2y * d2y);
        if (ln == 0 || gm_abs(den) < 1e-12 * ln) return 0;
        double wx = b.x0 - a.x0, wy = b.y0 - a.y0;
        double t = (wx * d2y - wy * d2x) / den, s = (wx * d1y - wy * d1x) / den;
        if (dentro_limite(a.k, t) && dentro_limite(b.k, s) && n < max) { xs[n] = a.x0 + t * d1x; ys[n] = a.y0 + t * d1y; n++; }
        return n;
    }
    if (a.k <= P_SEG && b.k == P_CIRC) {
        double dx = a.x1 - a.x0, dy = a.y1 - a.y0, fx = a.x0 - b.x0, fy = a.y0 - b.y0;
        double A = dx * dx + dy * dy, B = 2 * (fx * dx + fy * dy), C = fx * fx + fy * fy - b.r * b.r;
        if (A == 0) return 0;
        double disc = B * B - 4 * A * C;
        if (disc < 0) { if (disc > -1e-9 * (A * b.r * b.r + 1e-12)) disc = 0; else return 0; }
        double sq = gm_sqrt(disc);
        double t1 = (-B - sq) / (2 * A), t2 = (-B + sq) / (2 * A);
        if (dentro_limite(a.k, t1) && n < max) { xs[n] = a.x0 + t1 * dx; ys[n] = a.y0 + t1 * dy; n++; }
        if (sq > 0 && dentro_limite(a.k, t2) && n < max) { xs[n] = a.x0 + t2 * dx; ys[n] = a.y0 + t2 * dy; n++; }
        return n;
    }
    if (a.k == P_CIRC && b.k == P_CIRC) {
        double dx = b.x0 - a.x0, dy = b.y0 - a.y0, d = gm_sqrt(dx * dx + dy * dy);
        if (d == 0 || d > a.r + b.r + 1e-9 || d < gm_abs(a.r - b.r) - 1e-9) return 0;
        double l = (a.r * a.r - b.r * b.r + d * d) / (2 * d);
        double h2 = a.r * a.r - l * l, h = h2 > 0 ? gm_sqrt(h2) : 0;
        double mx = a.x0 + l * dx / d, my = a.y0 + l * dy / d;
        if (n < max) { xs[n] = mx - h * dy / d; ys[n] = my + h * dx / d; n++; }
        if (h > 0 && n < max) { xs[n] = mx + h * dy / d; ys[n] = my - h * dx / d; n++; }
        return n;
    }
    if (b.k == P_FUNC) {   /* a: recta, circunferencia o función */
        corte_ctx_t c = { b, a };
        double r[24];
        if (a.k <= P_SEG && a.x1 == a.x0) {          /* recta vertical */
            double fy = gbm_valor_funcao(b.fobj, a.x0);
            double t = a.y1 != a.y0 ? (fy - a.y0) / (a.y1 - a.y0) : 0;
            if (gm_fin(fy) && dentro_limite(a.k, t) && n < max) { xs[n] = a.x0; ys[n] = fy; n++; }
            return n;
        }
        int q = gbm_zeros_f(fn_corte, &c, -LIM_X, LIM_X, r, 24);
        for (int k = 0; k < q && n < max; k++) {
            double y = gbm_valor_funcao(b.fobj, r[k]);
            if (!gm_fin(y)) continue;
            if (a.k <= P_SEG) {
                double t = (r[k] - a.x0) / (a.x1 - a.x0);
                if (!dentro_limite(a.k, t)) continue;
            }
            xs[n] = r[k]; ys[n] = y; n++;
        }
        return n;
    }
    return 0;
}

int gbm_interseccoes(int a, int b, double *xs, double *ys, int max)
{
    prim_t pa[64], pb[64];
    int na = prims(a, pa, 64), nb = prims(b, pb, 64), n = 0;
    for (int i = 0; i < na && n < max; i++)
        for (int k = 0; k < nb && n < max; k++) n += corta(pa[i], pb[k], xs + n, ys + n, max - n);
    return n;
}

/* ---- raíces y extremos: las listas de un objeto función ---- */
static double fn_obj(void *c, double x) { return gbm_valor_funcao((int)(long)c, x); }
static double fn_deriv_obj(void *c, double x)
{
    double h = 1e-4 * (1.0 + gm_abs(x));
    return (gbm_valor_funcao((int)(long)c, x + h) - gbm_valor_funcao((int)(long)c, x - h)) / (2 * h);
}

static void calc_pontos_de_objeto(gbm_obj_t *o, int fi, bool extremo)
{
    double r[32];
    int q = gbm_zeros_f(extremo ? fn_deriv_obj : fn_obj, (void *)(long)fi, -LIM_X, LIM_X, r, 32);
    if (o->k < q) {
        double y = gbm_valor_funcao(fi, r[o->k]);
        if (gm_fin(y)) { o->v[0] = r[o->k]; o->v[1] = extremo ? y : 0.0; o->ok = 1; }
    }
}

static void calc_em(gbm_obj_t *o)
{
    int pi = o->pai[0];
    if (!valido(pi)) return;
    calc(pi);
    gbm_obj_t *p = O(pi);
    if (!p->ok) return;
    if (gbm_eh_reta(p->tipo)) {
        double t = o->param;
        int lim = limite_reta(p->tipo);
        if (lim == 2) t = t < 0 ? 0 : t > 1 ? 1 : t;
        else if (lim == 1 && t < 0) t = 0;
        o->v[0] = p->v[0] + t * (p->v[2] - p->v[0]);
        o->v[1] = p->v[1] + t * (p->v[3] - p->v[1]);
        o->ok = 1;
    } else if (gbm_eh_circ(p->tipo)) {
        o->v[0] = p->v[0] + p->v[2] * gm_cos(o->param);
        o->v[1] = p->v[1] + p->v[2] * gm_sin(o->param);
        o->ok = 1;
    } else if (p->tipo == G_FUNCAO) {
        double y = gbm_valor_funcao(pi, o->param);
        if (gm_fin(y)) { o->v[0] = o->param; o->v[1] = y; o->ok = 1; }
    } else if (gbm_eh_poligono(p->tipo)) {
        double xs[64], ys[64];
        int n = gbm_vertices(pi, xs, ys, 64);
        if (n < 2) return;
        double s = o->param;
        s = s - n * gm_floor(s / n);
        int e = (int)s;
        if (e >= n) e = n - 1;
        double f = s - e;
        o->v[0] = xs[e] + f * (xs[(e + 1) % n] - xs[e]);
        o->v[1] = ys[e] + f * (ys[(e + 1) % n] - ys[e]);
        o->ok = 1;
    }
}

static void calc(int i)
{
    gbm_obj_t *o = O(i);
    if (o->estado == 2) return;
    if (o->estado == 1) { ciclo = true; return; }
    o->estado = 1;
    double ax = 0, ay = 0, bx = 0, by = 0, cx = 0, cy = 0;
    switch (o->tipo) {
    case G_PTO_LIVRE: o->ok = gm_fin(o->v[0]) && gm_fin(o->v[1]); break;
    case G_PTO_EXPR: {
        o->ok = 0;
        gbm_val_t v = gbm_avaliar(o->raiz, 0);
        if (v.vec && gm_fin(v.a) && gm_fin(v.b)) { o->v[0] = v.a; o->v[1] = v.b; o->ok = 1; }
        break;
    }
    case G_PTO_MEDIO:
        o->ok = ptc(o->pai[0], &ax, &ay) && ptc(o->pai[1], &bx, &by);
        if (o->ok) { o->v[0] = (ax + bx) / 2; o->v[1] = (ay + by) / 2; }
        break;
    case G_PTO_INTER: {
        o->ok = 0;
        double xs[40], ys[40];
        calc(o->pai[0]); calc(o->pai[1]);
        int n = gbm_interseccoes(o->pai[0], o->pai[1], xs, ys, 40);
        if (o->k < n) { o->v[0] = xs[o->k]; o->v[1] = ys[o->k]; o->ok = 1; }
        break;
    }
    case G_PTO_EM: o->ok = 0; calc_em(o); break;
    case G_PTO_RAIZ: case G_PTO_EXTR:
        o->ok = 0;
        calc(o->pai[0]);
        if (valido(o->pai[0]) && O(o->pai[0])->ok) calc_pontos_de_objeto(o, o->pai[0], o->tipo == G_PTO_EXTR);
        break;
    case G_RETA: case G_SEMIRRETA: case G_SEGMENTO: case G_VETOR:
        o->ok = ptc(o->pai[0], &ax, &ay) && ptc(o->pai[1], &bx, &by);
        if (o->ok) {
            o->v[0] = ax; o->v[1] = ay; o->v[2] = bx; o->v[3] = by;
            if ((o->tipo == G_RETA || o->tipo == G_SEMIRRETA) && ax == bx && ay == by) o->ok = 0;
        }
        break;
    case G_PERP: case G_PARAL: {
        o->ok = 0;
        if (!ptc(o->pai[0], &ax, &ay)) break;
        int r = o->pai[1];
        if (!valido(r)) break;
        calc(r);
        gbm_obj_t *ro = O(r);
        if (!ro->ok || !gbm_eh_reta(ro->tipo)) break;
        double dx = ro->v[2] - ro->v[0], dy = ro->v[3] - ro->v[1];
        if (dx == 0 && dy == 0) break;
        if (o->tipo == G_PERP) { double t = dx; dx = -dy; dy = t; }
        o->v[0] = ax; o->v[1] = ay; o->v[2] = ax + dx; o->v[3] = ay + dy;
        o->ok = 1;
        break;
    }
    case G_MEDIATRIZ:
        o->ok = ptc(o->pai[0], &ax, &ay) && ptc(o->pai[1], &bx, &by) && !(ax == bx && ay == by);
        if (o->ok) {
            double mx = (ax + bx) / 2, my = (ay + by) / 2;
            o->v[0] = mx; o->v[1] = my; o->v[2] = mx - (by - ay); o->v[3] = my + (bx - ax);
        }
        break;
    case G_BISSETRIZ:
        o->ok = ptc(o->pai[0], &ax, &ay) && ptc(o->pai[1], &bx, &by) && ptc(o->pai[2], &cx, &cy);
        if (o->ok) {
            double ux = ax - bx, uy = ay - by, wx = cx - bx, wy = cy - by;
            double lu = gm_hypot(ux, uy), lw = gm_hypot(wx, wy);
            if (lu == 0 || lw == 0) { o->ok = 0; break; }
            ux /= lu; uy /= lu; wx /= lw; wy /= lw;
            double dx = ux + wx, dy = uy + wy;
            if (gm_hypot(dx, dy) < 1e-12) { dx = -uy; dy = ux; }
            o->v[0] = bx; o->v[1] = by; o->v[2] = bx + dx; o->v[3] = by + dy;
        }
        break;
    case G_CIRC_PP:
        o->ok = ptc(o->pai[0], &ax, &ay) && ptc(o->pai[1], &bx, &by);
        if (o->ok) { o->v[0] = ax; o->v[1] = ay; o->v[2] = gm_hypot(bx - ax, by - ay); }
        break;
    case G_CIRC_R: {
        o->ok = 0;
        if (!ptc(o->pai[0], &ax, &ay)) break;
        gbm_val_t r = gbm_avaliar(o->raiz, 0);
        if (!r.vec && gm_fin(r.a) && r.a >= 0) { o->v[0] = ax; o->v[1] = ay; o->v[2] = r.a; o->ok = 1; }
        break;
    }
    case G_CIRC_3:
        o->ok = ptc(o->pai[0], &ax, &ay) && ptc(o->pai[1], &bx, &by) && ptc(o->pai[2], &cx, &cy);
        if (o->ok) {
            double d = 2 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
            if (gm_abs(d) < 1e-12) { o->ok = 0; break; }
            double a2 = ax * ax + ay * ay, b2 = bx * bx + by * by, c2 = cx * cx + cy * cy;
            double ux = (a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / d;
            double uy = (a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / d;
            o->v[0] = ux; o->v[1] = uy; o->v[2] = gm_hypot(ax - ux, ay - uy);
        }
        break;
    case G_POLIGONO: case G_POLREG: {
        double xs[64], ys[64];
        int n;
        o->ok = 0;
        if (o->tipo == G_POLIGONO) {
            bool bien = true;
            for (int k = 0; k < o->npai; k++) if (!ptc(o->pai[k], &xs[k], &ys[k])) bien = false;
            if (!bien) break;
            n = o->npai;
            o->ok = 1;
        } else {
            if (!ptc(o->pai[0], &ax, &ay) || !ptc(o->pai[1], &bx, &by)) break;
            o->ok = 1;
            n = vert_raw(o, xs, ys, 64);
            if (n == 0) { o->ok = 0; break; }
        }
        double s = 0, per = 0;
        for (int k = 0; k < n; k++) {
            int m = (k + 1) % n;
            s += xs[k] * ys[m] - xs[m] * ys[k];
            per += gm_hypot(xs[m] - xs[k], ys[m] - ys[k]);
        }
        o->v[0] = gm_abs(s) / 2; o->v[1] = per;
        break;
    }
    case G_ANGULO:
        o->ok = ptc(o->pai[0], &ax, &ay) && ptc(o->pai[1], &bx, &by) && ptc(o->pai[2], &cx, &cy);
        if (o->ok) {
            double ux = ax - bx, uy = ay - by, wx = cx - bx, wy = cy - by;
            if ((ux == 0 && uy == 0) || (wx == 0 && wy == 0)) { o->ok = 0; break; }
            double t = gm_atan2(ux * wy - uy * wx, ux * wx + uy * wy);
            if (t < 0) t += 2 * GM_PI;
            o->v[0] = t;
        }
        break;
    case G_DIST:
        o->ok = 0;
        if (o->k == 1) {   /* punto y recta */
            if (!ptc(o->pai[0], &ax, &ay) || !valido(o->pai[1])) break;
            calc(o->pai[1]);
            gbm_obj_t *r = O(o->pai[1]);
            if (!r->ok || !gbm_eh_reta(r->tipo)) break;
            double dx = r->v[2] - r->v[0], dy = r->v[3] - r->v[1], l = gm_hypot(dx, dy);
            if (l == 0) break;
            o->v[0] = gm_abs((ax - r->v[0]) * dy - (ay - r->v[1]) * dx) / l;
            o->ok = 1;
        } else if (ptc(o->pai[0], &ax, &ay) && ptc(o->pai[1], &bx, &by)) { o->v[0] = gm_hypot(bx - ax, by - ay); o->ok = 1; }
        break;
    case G_AREA:
        o->ok = 0;
        if (valido(o->pai[0])) {
            calc(o->pai[0]);
            gbm_obj_t *p = O(o->pai[0]);
            if (p->ok && gbm_eh_poligono(p->tipo)) { o->v[0] = p->v[0]; o->ok = 1; }
            else if (p->ok && gbm_eh_circ(p->tipo)) { o->v[0] = GM_PI * p->v[2] * p->v[2]; o->ok = 1; }
        }
        break;
    case G_NUM_EXPR: {
        gbm_val_t v = gbm_avaliar(o->raiz, 0);
        o->ok = !v.vec && gm_fin(v.a);
        if (o->ok) o->v[0] = v.a;
        break;
    }
    case G_SLIDER: o->ok = gm_fin(o->v[0]); break;
    case G_TEXTO: o->ok = 1; break;
    case G_FUNCAO: o->ok = o->raiz >= 0; break;
    }
    o->estado = 2;
}

void gbm_recalcular(void)
{
    ciclo = false;
    for (int i = 0; i < gbm_n; i++) gbm_obj[i].estado = 0;
    for (int i = 0; i < gbm_n; i++) calc(i);
}

/* ------------------------------------------------------------ dependencias */

static int deps(int i, int *s, int max)
{
    gbm_obj_t *o = O(i);
    int n = 0;
    for (int k = 0; k < o->npai && n < max; k++) s[n++] = o->pai[k];
    if (o->raiz >= 0 && n < max) {
        int r[GBM_PAI_MAX + 8];
        int q = gbm_refs(o->raiz, r, GBM_PAI_MAX + 8);
        for (int k = 0; k < q && n < max; k++) {
            bool ya = false;
            for (int m = 0; m < n; m++) if (s[m] == r[k]) ya = true;
            if (!ya) s[n++] = r[k];
        }
    }
    return n;
}

bool gbm_depende(int i, int de)
{
    if (!valido(i) || !valido(de)) return false;
    unsigned char vis[GBM_OBJ_MAX] = { 0 };
    int pila[GBM_OBJ_MAX], sp = 0;
    pila[sp++] = i;
    vis[i] = 1;
    while (sp) {
        int x = pila[--sp], d[GBM_PAI_MAX * 2];
        int n = deps(x, d, GBM_PAI_MAX * 2);
        for (int k = 0; k < n; k++) {
            if (d[k] == de) return true;
            if (valido(d[k]) && !vis[d[k]]) { vis[d[k]] = 1; pila[sp++] = d[k]; }
        }
    }
    return false;
}

int gbm_dependentes(int i, int *saida, int max)
{
    int n = 0;
    for (int j = 0; j < gbm_n && n < max; j++) if (j != i && gbm_depende(j, i)) saida[n++] = j;
    return n;
}

/* ------------------------------------------------------------ texto de un objeto */

static void num(char *s, int max, double v) { gbm_formatar(v, gbm_decimais, s, max); }

static void termo(char *s, int max, double a, const char *var, bool primero)
{
    char n[32];
    double m = gm_abs(a);
    int l = (int)tam_texto(s);
    if (primero) { if (a < 0) { s[l++] = '-'; s[l] = 0; } }
    else { copia_texto(s + l, a < 0 ? " - " : " + ", (size_t)(max - l)); }
    if (m != 1.0 || !var[0]) { num(n, sizeof n, m); copia_texto(s + tam_texto(s), n, (size_t)max - tam_texto(s)); }
    copia_texto(s + tam_texto(s), var, (size_t)max - tam_texto(s));
}

static void eq_reta(gbm_obj_t *o, char *s, int max)
{
    double dx = o->v[2] - o->v[0], dy = o->v[3] - o->v[1];
    double A = dy, B = -dx, C = dy * o->v[0] - dx * o->v[1];
    double lim = 1e-9 * (gm_abs(A) + gm_abs(B) + 1e-30);
    if (A < -lim || (gm_abs(A) <= lim && B < 0)) { A = -A; B = -B; C = -C; }
    s[0] = 0;
    char n[32];
    bool primero = true;
    if (gm_abs(A) > lim) { termo(s, max, A, "x", true); primero = false; }
    if (gm_abs(B) > lim) termo(s, max, B, "y", primero);
    copia_texto(s + tam_texto(s), " = ", (size_t)max - tam_texto(s));
    num(n, sizeof n, C);
    copia_texto(s + tam_texto(s), n, (size_t)max - tam_texto(s));
}

static void eq_circ(gbm_obj_t *o, char *s, int max)
{
    char a[40], b[40], r[40];
    double cx = o->v[0], cy = o->v[1];
    s[0] = 0;
    for (int e = 0; e < 2; e++) {
        double c = e ? cy : cx;
        const char *v = e ? "y" : "x";
        num(a, sizeof a, gm_abs(c));
        if (gm_abs(c) < 0.5 * gm_abs(gm_pow(10.0, -gbm_decimais))) formatar_texto(b, sizeof b, "%s\xB2", v);
        else formatar_texto(b, sizeof b, "(%s %c %s)\xB2", v, c < 0 ? '+' : '-', a);
        copia_texto(s + tam_texto(s), b, (size_t)max - tam_texto(s));
        if (!e) copia_texto(s + tam_texto(s), " + ", (size_t)max - tam_texto(s));
    }
    num(r, sizeof r, o->v[2] * o->v[2]);
    copia_texto(s + tam_texto(s), " = ", (size_t)max - tam_texto(s));
    copia_texto(s + tam_texto(s), r, (size_t)max - tam_texto(s));
}

int gbm_descrever(int i, char *s, int max)
{
    if (!valido(i)) { s[0] = 0; return 0; }
    gbm_obj_t *o = O(i);
    calc(i);
    char a[48], b[48], c[160];
    if (!o->ok && o->tipo != G_FUNCAO) { formatar_texto(s, (size_t)max, "%s = ?", o->nome); return (int)tam_texto(s); }
    switch (o->tipo) {
    case G_PTO_LIVRE: case G_PTO_EXPR: case G_PTO_MEDIO: case G_PTO_INTER: case G_PTO_EM: case G_PTO_RAIZ: case G_PTO_EXTR:
        num(a, sizeof a, o->v[0]); num(b, sizeof b, o->v[1]);
        formatar_texto(s, (size_t)max, "%s = (%s, %s)", o->nome, a, b);
        break;
    case G_VETOR:
        num(a, sizeof a, o->v[2] - o->v[0]); num(b, sizeof b, o->v[3] - o->v[1]);
        formatar_texto(s, (size_t)max, "%s = (%s, %s)", o->nome, a, b);
        break;
    case G_SEGMENTO:
        num(a, sizeof a, gm_hypot(o->v[2] - o->v[0], o->v[3] - o->v[1]));
        formatar_texto(s, (size_t)max, "%s = %s", o->nome, a);
        break;
    case G_RETA: case G_SEMIRRETA: case G_PERP: case G_PARAL: case G_MEDIATRIZ: case G_BISSETRIZ:
        eq_reta(o, c, sizeof c);
        formatar_texto(s, (size_t)max, "%s: %s", o->nome, c);
        break;
    case G_CIRC_PP: case G_CIRC_R: case G_CIRC_3:
        eq_circ(o, c, sizeof c);
        formatar_texto(s, (size_t)max, "%s: %s", o->nome, c);
        break;
    case G_POLIGONO: case G_POLREG:
        num(a, sizeof a, o->v[0]);
        formatar_texto(s, (size_t)max, "%s = %s", o->nome, a);
        break;
    case G_ANGULO:
        gbm_formatar(o->v[0] * 180.0 / GM_PI, gbm_decimais, a, sizeof a);
        formatar_texto(s, (size_t)max, "%s = %s\xB0", o->nome, a);
        break;
    case G_DIST: case G_AREA: case G_NUM_EXPR: case G_SLIDER:
        num(a, sizeof a, o->v[0]);
        formatar_texto(s, (size_t)max, "%s = %s", o->nome, a);
        break;
    case G_TEXTO: formatar_texto(s, (size_t)max, "%s = \"%s\"", o->nome, o->texto); break;
    case G_FUNCAO: {
        char e[600];
        gbm_escrever(o->raiz, e, sizeof e, true);
        formatar_texto(s, (size_t)max, "%s(x) = %s", o->nome, e);
        break;
    }
    default: s[0] = 0;
    }
    return (int)tam_texto(s);
}

/* ------------------------------------------------------------ comandos */

int gbm_serializar_sem(char *s, int max, const unsigned char *omitir);
static void erro(const char *m) { copia_texto(gbm_erro, m, sizeof gbm_erro); }
static void erro_n(const char *m, const char *n) { formatar_texto(gbm_erro, sizeof gbm_erro, "%s: %s", m, n); }

static char *recorta(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    int n = (int)tam_texto(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = 0;
    return s;
}

#define MAX_ARGS 28
typedef struct { char *txt; int obj; } arg_t;

/* Parte "a, b, (c, d)" por las comas de nivel cero (fuera de paréntesis y de comillas). */
static int separa(char *s, arg_t *a, int max)
{
    int n = 0, prof = 0;
    bool com = false;
    char *ini = s;
    for (char *p = s;; p++) {
        if (*p == '"') com = !com;
        if (!com) {
            if (*p == '(') prof++;
            else if (*p == ')') prof--;
        }
        if ((*p == ',' && prof == 0 && !com) || *p == 0) {
            bool fin = *p == 0;
            *p = 0;
            if (n < max) { a[n].txt = recorta(ini); a[n].obj = -1; n++; }
            if (fin) break;
            ini = p + 1;
        }
    }
    if (n == 1 && !a[0].txt[0]) return 0;
    for (int k = 0; k < n; k++) {
        const char *t = a[k].txt;
        if (!letra(t[0])) continue;
        const char *q = t;
        while (letra(*q) || digito(*q)) q++;
        if (*q == 0) a[k].obj = gbm_buscar(t);
    }
    return n;
}

static bool eh_ponto_i(int i) { return valido(i) && gbm_eh_ponto(O(i)->tipo); }

/* Un punto a partir de un argumento: un punto existente o una expresión (x, y). */
static int arg_ponto(arg_t *a)
{
    if (a->obj >= 0) {
        if (eh_ponto_i(a->obj)) return a->obj;
        erro_n("Se esperaba un punto", a->txt);
        return -1;
    }
    int r = gbm_analizar(a->txt, false);
    if (r < 0) return -1;
    gbm_val_t v = gbm_avaliar(r, 0);
    if (!v.vec) { erro_n("Se esperaba un punto", a->txt); return -1; }
    char *nm = NULL;
    (void)nm;
    int i = nova(gbm_constante(r) ? G_PTO_LIVRE : G_PTO_EXPR);
    if (i < 0) return -1;
    if (O(i)->tipo == G_PTO_LIVRE) { O(i)->v[0] = v.a; O(i)->v[1] = v.b; }
    else O(i)->raiz = r;
    return i;
}

/* Un número: expresión (con referencias a deslizadores, por ejemplo). */
static int arg_num(arg_t *a)
{
    int r = gbm_analizar(a->txt, false);
    if (r < 0) return -1;
    gbm_val_t v = gbm_avaliar(r, 0);
    if (v.vec || !gm_fin(v.a)) { erro_n("Se esperaba un número", a->txt); return -1; }
    return r;
}

static bool arg_cte(arg_t *a, double *v)
{
    int r = gbm_analizar(a->txt, false);
    if (r < 0 || !gbm_constante(r)) return false;
    gbm_val_t x = gbm_avaliar(r, 0);
    if (x.vec) return false;
    *v = x.a;
    return true;
}

/* Un objeto función: uno existente o una expresión con x (se crea). */
static int arg_funcao(arg_t *a)
{
    if (a->obj >= 0) {
        if (O(a->obj)->tipo == G_FUNCAO) return a->obj;
        erro_n("Se esperaba una función", a->txt);
        return -1;
    }
    int r = gbm_analizar(a->txt, true);
    if (r < 0) return -1;
    if (!gbm_usa_x(r)) { erro_n("Se esperaba una función", a->txt); return -1; }
    int i = nova(G_FUNCAO);
    if (i < 0) return -1;
    O(i)->raiz = r;
    return i;
}

static int nuevo_con_pais(int tipo, int n, int p0, int p1, int p2)
{
    int i = nova(tipo);
    if (i < 0) return -1;
    O(i)->npai = n;
    O(i)->pai[0] = (int16_t)p0; O(i)->pai[1] = (int16_t)p1; O(i)->pai[2] = (int16_t)p2;
    return i;
}

/* Extremos de un segmento o vector, para que «Mediatriz(s)» y «PuntoMedio(s)» valgan con un segmento. */
static bool extremos_de(arg_t *a, int *p, int *q)
{
    if (a->obj >= 0 && (O(a->obj)->tipo == G_SEGMENTO || O(a->obj)->tipo == G_VETOR)) { *p = O(a->obj)->pai[0]; *q = O(a->obj)->pai[1]; return true; }
    return false;
}

static bool vale_para_reta(int i) { return valido(i) && gbm_eh_reta(O(i)->tipo); }

static int cmd_executar(int cmd, arg_t *a, int na, int *ultimo)
{
    int i = -1, p, q, r, s;
    double v;
    switch (cmd) {
    case 0: case 1: {   /* Punto(obj) / PuntoEn(obj, t) / Punto(x, y) */
        if (na == 2 && cmd == 0 && a[0].obj < 0) {
            double x, y;
            if (!arg_cte(&a[0], &x) || !arg_cte(&a[1], &y)) { erro("Punto(x, y) necesita dos números"); return -1; }
            i = nova(G_PTO_LIVRE);
            if (i >= 0) { O(i)->v[0] = x; O(i)->v[1] = y; }
            break;
        }
        if (na < 1 || a[0].obj < 0) { erro("Punto(objeto): falta el objeto"); return -1; }
        int t = O(a[0].obj)->tipo;
        if (!(gbm_eh_reta(t) || gbm_eh_circ(t) || t == G_FUNCAO || gbm_eh_poligono(t))) { erro("En ese objeto no se puede poner un punto"); return -1; }
        double par = 0.5;
        if (na >= 2 && !arg_cte(&a[1], &par)) { erro("El parámetro tiene que ser un número"); return -1; }
        if (na < 2 && gbm_eh_circ(t)) par = 0.0;
        if (na < 2 && t == G_FUNCAO) par = 0.0;
        i = nuevo_con_pais(G_PTO_EM, 1, a[0].obj, 0, 0);
        if (i >= 0) O(i)->param = par;
        break;
    }
    case 2:   /* PuntoMedio */
        if (na == 1 && extremos_de(&a[0], &p, &q)) { i = nuevo_con_pais(G_PTO_MEDIO, 2, p, q, 0); break; }
        if (na != 2) { erro("PuntoMedio(A, B) o PuntoMedio(segmento)"); return -1; }
        if ((p = arg_ponto(&a[0])) < 0 || (q = arg_ponto(&a[1])) < 0) return -1;
        i = nuevo_con_pais(G_PTO_MEDIO, 2, p, q, 0);
        break;
    case 3: {   /* Interseca(a, b[, k]) */
        if (na < 2 || a[0].obj < 0 || a[1].obj < 0) {
            if (na < 2) { erro("Interseca(a, b)"); return -1; }
        }
        p = a[0].obj >= 0 ? a[0].obj : arg_funcao(&a[0]);
        q = a[1].obj >= 0 ? a[1].obj : arg_funcao(&a[1]);
        if (p < 0 || q < 0) return -1;
        double xs[40], ys[40];
        gbm_recalcular();
        int n = gbm_interseccoes(p, q, xs, ys, 40);
        double kk = 0;
        if (na >= 3) {
            if (!arg_cte(&a[2], &kk) || kk < 1) { erro("El tercer dato es el número del corte (1, 2...)"); return -1; }
            i = nuevo_con_pais(G_PTO_INTER, 2, p, q, 0);
            if (i >= 0) O(i)->k = (int)kk - 1;
            break;
        }
        if (n == 0) { erro("Los objetos no se cortan"); return -1; }
        if (n > 8) n = 8;
        for (int k = 0; k < n; k++) {
            i = nuevo_con_pais(G_PTO_INTER, 2, p, q, 0);
            if (i < 0) return -1;
            O(i)->k = k;
            if (ultimo) *ultimo = i;
        }
        return 0;
    }
    case 4: case 5: case 6: case 7: {   /* Recta, Semirrecta, Segmento, Vector */
        if (na != 2) { erro("Hacen falta dos puntos"); return -1; }
        if ((p = arg_ponto(&a[0])) < 0 || (q = arg_ponto(&a[1])) < 0) return -1;
        i = nuevo_con_pais(cmd == 4 ? G_RETA : cmd == 5 ? G_SEMIRRETA : cmd == 6 ? G_SEGMENTO : G_VETOR, 2, p, q, 0);
        break;
    }
    case 8: {   /* Circunferencia(A, B) / (A, r) / (A, B, C) */
        if (na == 3) {
            if ((p = arg_ponto(&a[0])) < 0 || (q = arg_ponto(&a[1])) < 0 || (r = arg_ponto(&a[2])) < 0) return -1;
            i = nuevo_con_pais(G_CIRC_3, 3, p, q, r);
            break;
        }
        if (na != 2) { erro("Circunferencia(centro, punto), (centro, radio) o (A, B, C)"); return -1; }
        if ((p = arg_ponto(&a[0])) < 0) return -1;
        if (a[1].obj >= 0 && eh_ponto_i(a[1].obj)) { i = nuevo_con_pais(G_CIRC_PP, 2, p, a[1].obj, 0); break; }
        int ex = arg_num(&a[1]);
        if (ex < 0) return -1;
        i = nuevo_con_pais(G_CIRC_R, 1, p, 0, 0);
        if (i >= 0) O(i)->raiz = ex;
        break;
    }
    case 9: case 10: {   /* Perpendicular(P, r) / Paralela(P, r), en cualquier orden */
        if (na != 2) { erro("Hace falta un punto y una recta"); return -1; }
        if (vale_para_reta(a[0].obj) && !eh_ponto_i(a[0].obj)) { arg_t t = a[0]; a[0] = a[1]; a[1] = t; }
        if ((p = arg_ponto(&a[0])) < 0) return -1;
        if (!vale_para_reta(a[1].obj)) { erro("El segundo dato tiene que ser una recta"); return -1; }
        i = nuevo_con_pais(cmd == 9 ? G_PERP : G_PARAL, 2, p, a[1].obj, 0);
        break;
    }
    case 11:   /* Mediatriz */
        if (na == 1 && extremos_de(&a[0], &p, &q)) { i = nuevo_con_pais(G_MEDIATRIZ, 2, p, q, 0); break; }
        if (na != 2) { erro("Mediatriz(A, B) o Mediatriz(segmento)"); return -1; }
        if ((p = arg_ponto(&a[0])) < 0 || (q = arg_ponto(&a[1])) < 0) return -1;
        i = nuevo_con_pais(G_MEDIATRIZ, 2, p, q, 0);
        break;
    case 12:   /* Bisectriz(A, B, C): del ángulo en B */
        if (na != 3) { erro("Bisectriz(A, B, C): el vértice es B"); return -1; }
        if ((p = arg_ponto(&a[0])) < 0 || (q = arg_ponto(&a[1])) < 0 || (r = arg_ponto(&a[2])) < 0) return -1;
        i = nuevo_con_pais(G_BISSETRIZ, 3, p, q, r);
        break;
    case 13: {   /* Poligono(A, B, C...) / Poligono(A, B, n) */
        if (na < 3) { erro("Un polígono necesita al menos tres puntos"); return -1; }
        if (na == 3 && a[2].obj < 0 && arg_cte(&a[2], &v) && v >= 3 && v <= 64 && v == gm_floor(v)) {
            if ((p = arg_ponto(&a[0])) < 0 || (q = arg_ponto(&a[1])) < 0) return -1;
            i = nuevo_con_pais(G_POLREG, 2, p, q, 0);
            if (i >= 0) O(i)->k = (int)v;
            break;
        }
        if (na > GBM_PAI_MAX) { erro("Demasiados vértices"); return -1; }
        int ps[GBM_PAI_MAX];
        for (int k = 0; k < na; k++) if ((ps[k] = arg_ponto(&a[k])) < 0) return -1;
        i = nova(G_POLIGONO);
        if (i < 0) return -1;
        O(i)->npai = na;
        for (int k = 0; k < na; k++) O(i)->pai[k] = (int16_t)ps[k];
        break;
    }
    case 14:   /* Angulo(A, B, C) */
        if (na != 3) { erro("Angulo(A, B, C): el vértice es B"); return -1; }
        if ((p = arg_ponto(&a[0])) < 0 || (q = arg_ponto(&a[1])) < 0 || (r = arg_ponto(&a[2])) < 0) return -1;
        i = nuevo_con_pais(G_ANGULO, 3, p, q, r);
        break;
    case 15: case 16:   /* Distancia(A, B) / (P, recta) / Longitud(segmento) */
        if (na == 1 && extremos_de(&a[0], &p, &q)) { i = nuevo_con_pais(G_DIST, 2, p, q, 0); break; }
        if (na != 2) { erro("Distancia(A, B)"); return -1; }
        if (vale_para_reta(a[1].obj) && !eh_ponto_i(a[1].obj)) {
            if ((p = arg_ponto(&a[0])) < 0) return -1;
            i = nuevo_con_pais(G_DIST, 2, p, a[1].obj, 0);
            if (i >= 0) O(i)->k = 1;
            break;
        }
        if ((p = arg_ponto(&a[0])) < 0 || (q = arg_ponto(&a[1])) < 0) return -1;
        i = nuevo_con_pais(G_DIST, 2, p, q, 0);
        break;
    case 17:   /* Area(polígono o circunferencia) */
        if (na != 1 || a[0].obj < 0 || !(gbm_eh_poligono(O(a[0].obj)->tipo) || gbm_eh_circ(O(a[0].obj)->tipo))) { erro("Area(polígono o circunferencia)"); return -1; }
        i = nuevo_con_pais(G_AREA, 1, a[0].obj, 0, 0);
        break;
    case 18: {   /* Texto("...", x, y) */
        if (na < 1 || a[0].txt[0] != '"') { erro("Texto(\"mensaje\", x, y)"); return -1; }
        double x = 0, y = 0;
        if (na >= 3 && (!arg_cte(&a[1], &x) || !arg_cte(&a[2], &y))) { erro("La posición tiene que ser un par de números"); return -1; }
        i = nova(G_TEXTO);
        if (i < 0) return -1;
        char *t = a[0].txt + 1;
        int n = (int)tam_texto(t);
        if (n > 0 && t[n - 1] == '"') t[n - 1] = 0;
        copia_texto(O(i)->texto, t, sizeof O(i)->texto);
        O(i)->px = x; O(i)->py = y;
        break;
    }
    case 19: {   /* Deslizador(min, max, paso, valor) */
        double mn = -5, mx = 5, st = 0.1, val = 1;
        if (na >= 1 && !arg_cte(&a[0], &mn)) { erro("Los límites tienen que ser números"); return -1; }
        if (na >= 2 && !arg_cte(&a[1], &mx)) { erro("Los límites tienen que ser números"); return -1; }
        if (na >= 3 && !arg_cte(&a[2], &st)) { erro("El paso tiene que ser un número"); return -1; }
        val = mn < 0 && mx > 0 ? 0 : mn;
        if (na >= 4 && !arg_cte(&a[3], &val)) { erro("El valor tiene que ser un número"); return -1; }
        if (!(mx > mn) || st < 0) { erro("Límites no válidos"); return -1; }
        i = nova(G_SLIDER);
        if (i < 0) return -1;
        O(i)->min = mn; O(i)->max = mx; O(i)->passo = st; O(i)->v[0] = val;
        O(i)->px = 20; O(i)->py = 0;
        break;
    }
    case 20: {   /* Derivada(f[, n]) */
        if (na < 1) { erro("Derivada(f)"); return -1; }
        double orden = 1;
        if (na >= 2 && (!arg_cte(&a[1], &orden) || orden < 1 || orden > 6)) { erro("El orden tiene que ser de 1 a 6"); return -1; }
        int f = a[0].obj >= 0 ? a[0].obj : -1;
        int raiz;
        if (f >= 0) {
            if (O(f)->tipo != G_FUNCAO) { erro("Se esperaba una función"); return -1; }
            raiz = O(f)->raiz;
        } else {
            raiz = gbm_analizar(a[0].txt, true);
            if (raiz < 0) return -1;
            if (!gbm_usa_x(raiz)) { erro("Se esperaba una función"); return -1; }
        }
        for (int k = 0; k < (int)orden; k++) { raiz = gbm_derivada(raiz); if (raiz < 0) { erro("No se puede derivar"); return -1; } }
        i = nova(G_FUNCAO);
        if (i < 0) return -1;
        O(i)->raiz = raiz;
        break;
    }
    case 21: case 22: {   /* Raiz(f[, k]) / Extremo(f[, k]) */
        if (na < 1) { erro("Raiz(f)"); return -1; }
        int f = arg_funcao(&a[0]);
        if (f < 0) return -1;
        int tipo = cmd == 21 ? G_PTO_RAIZ : G_PTO_EXTR;
        if (na >= 2) {
            double kk;
            if (!arg_cte(&a[1], &kk) || kk < 1) { erro("El segundo dato es el número de la raíz (1, 2...)"); return -1; }
            i = nuevo_con_pais(tipo, 1, f, 0, 0);
            if (i >= 0) O(i)->k = (int)kk - 1;
            break;
        }
        gbm_recalcular();
        double rs[32];
        int n = gbm_zeros_f(cmd == 21 ? fn_obj : fn_deriv_obj, (void *)(long)f, -LIM_X, LIM_X, rs, 32);
        if (n == 0) { erro(cmd == 21 ? "La función no tiene raíces entre -30 y 30" : "La función no tiene extremos entre -30 y 30"); return -1; }
        if (n > 10) n = 10;
        for (int k = 0; k < n; k++) {
            i = nuevo_con_pais(tipo, 1, f, 0, 0);
            if (i < 0) return -1;
            O(i)->k = k;
            if (ultimo) *ultimo = i;
        }
        return 0;
    }
    default: erro("Orden no implementada"); return -1;
    }
    (void)s;
    if (i < 0) return -1;
    if (ultimo) *ultimo = i;
    return 0;
}

/* ------------------------------------------------------------ interpretación de una línea */

static bool cierra_al_final(const char *s)   /* el paréntesis que abre en s[0] cierra en el último carácter */
{
    int prof = 0;
    bool com = false;
    for (const char *p = s; *p; p++) {
        if (*p == '"') com = !com;
        if (com) continue;
        if (*p == '(') prof++;
        else if (*p == ')') { prof--; if (prof == 0) return p[1] == 0; }
    }
    return false;
}

static int ya_existe_redefinir(int ex, int raiz, bool tiene_x, int *ultimo)
{
    gbm_obj_t copia = *O(ex);
    gbm_obj_t *o = O(ex);
    gbm_val_t v = gbm_avaliar(raiz, 0.5);
    if (o->tipo == G_SLIDER || o->tipo == G_PTO_LIVRE) {
        if (!gbm_constante(raiz)) { erro_n("Ese objeto es libre; para ligarlo a otro, bórralo antes", o->nome); return -1; }
        if (o->tipo == G_SLIDER && !v.vec) {
            o->v[0] = v.a;
            if (v.a < o->min) o->min = v.a;
            if (v.a > o->max) o->max = v.a;
            gbm_recalcular();
            if (ultimo) *ultimo = ex;
            return 0;
        }
        if (o->tipo == G_PTO_LIVRE && v.vec) { o->v[0] = v.a; o->v[1] = v.b; gbm_recalcular(); if (ultimo) *ultimo = ex; return 0; }
        erro_n("No coincide el tipo de", o->nome);
        return -1;
    }
    if (o->tipo == G_FUNCAO && tiene_x && !v.vec) o->raiz = raiz;
    else if (o->tipo == G_NUM_EXPR && !tiene_x && !v.vec) o->raiz = raiz;
    else if (o->tipo == G_PTO_EXPR && !tiene_x && v.vec) o->raiz = raiz;
    else { erro_n("Ya existe un objeto con ese nombre", o->nome); return -1; }
    gbm_recalcular();
    if (ciclo) { *o = copia; gbm_recalcular(); erro("Esa definición sería circular"); return -1; }
    if (ultimo) *ultimo = ex;
    return 0;
}

int gbm_orden(const char *texto, int *ultimo)
{
    char buf[1100];
    copia_texto(buf, texto, sizeof buf);
    char *s = recorta(buf);
    gbm_erro[0] = 0;
    if (!*s) { erro("No has escrito nada"); return -1; }
    if (gbm_nodos_usados() > 11000) {   /* el cajón de nodos se llena: se vuelve a cargar la construcción para compactarlo */
        static char tmp[131072];
        gbm_serializar(tmp, sizeof tmp, NULL);
        gbm_limpar();
        gbm_carregar(tmp, NULL);
    }
    char nome[GBM_NOME + 4] = "";
    bool con_nome = false, def_funcao = false;
    char *rhs = s;
    if (letra(*s)) {
        char *p = s;
        while (letra(*p) || digito(*p)) p++;
        char *q = p;
        while (*q == ' ') q++;
        int ln = (int)(p - s);
        if (*q == '=' && q[1] != '=' && ln < GBM_NOME) {
            memcpy(nome, s, (size_t)ln); nome[ln] = 0;
            con_nome = true;
            rhs = q + 1;
        } else if (*q == '(' && ln < GBM_NOME) {   /* f(x) = ... */
            char *r = q + 1;
            while (*r == ' ') r++;
            if (letra(*r)) {
                char *r2 = r;
                while (letra(*r2) || digito(*r2)) r2++;
                char *r3 = r2;
                while (*r3 == ' ') r3++;
                if (*r3 == ')') {
                    char *r4 = r3 + 1;
                    while (*r4 == ' ') r4++;
                    if (*r4 == '=' && r4[1] != '=' && r2 - r == 1 && *r == 'x') {
                        memcpy(nome, s, (size_t)ln); nome[ln] = 0;
                        con_nome = true; def_funcao = true;
                        rhs = r4 + 1;
                    }
                }
            }
        }
    }
    rhs = recorta(rhs);
    if (!*rhs) { erro("Falta la definición"); return -1; }
    bool y_igual = con_nome && nome[0] == 'y' && nome[1] == 0;
    if (con_nome && !y_igual) {
        if (!nome_valido(nome)) {
            if (comando_id(nome) >= 0 || nome_funcao_ou_const(nome)) erro_n("Ese nombre está reservado", nome);
            else erro_n("Nombre no válido", nome);
            return -1;
        }
    }
    int antes = gbm_n;
    /* ¿orden con nombre? */
    if (letra(*rhs)) {
        char *p = rhs;
        while (letra(*p) || digito(*p)) p++;
        char *q = p;
        while (*q == ' ') q++;
        if (*q == '(') {
            char cn[GBM_NOME + 4];
            int ln = (int)(p - rhs);
            if (ln < (int)sizeof cn) {
                memcpy(cn, rhs, (size_t)ln); cn[ln] = 0;
                int cmd = comando_id(cn);
                if (cmd >= 0 && cierra_al_final(q)) {
                    char *interior = q + 1;
                    int n = (int)tam_texto(interior);
                    interior[n - 1] = 0;
                    arg_t a[MAX_ARGS];
                    int na = separa(interior, a, MAX_ARGS);
                    for (int k = 0; k < na; k++) {   /* una orden dentro de otra: Perpendicular(P, Recta(A, B)) */
                        if (a[k].obj >= 0 || !letra(a[k].txt[0])) continue;
                        char *t = a[k].txt;
                        while (letra(*t) || digito(*t)) t++;
                        char *t2 = t;
                        while (*t2 == ' ') t2++;
                        if (*t2 != '(') continue;
                        char cn2[GBM_NOME + 4];
                        int l2 = (int)(t - a[k].txt);
                        if (l2 >= (int)sizeof cn2) continue;
                        memcpy(cn2, a[k].txt, (size_t)l2); cn2[l2] = 0;
                        if (comando_id(cn2) < 0 || !cierra_al_final(t2)) continue;
                        int u = -1;
                        if (gbm_orden(a[k].txt, &u) != 0) { gbm_n = antes; return -1; }
                        a[k].obj = u;
                    }
                    if (con_nome && !y_igual && gbm_buscar(nome) >= 0) { erro_n("Ya existe un objeto con ese nombre", nome); return -1; }
                    int ult = -1;
                    int r = cmd_executar(cmd, a, na, &ult);
                    if (r < 0) { gbm_n = antes; return -1; }
                    if (ult < 0) ult = gbm_n - 1;
                    if (ultimo) *ultimo = ult;
                    if (con_nome && !y_igual && ult >= antes) {
                        /* el nombre pedido es del objeto principal; si un punto auxiliar de esta misma orden lo había tomado, cede */
                        int j = gbm_buscar(nome);
                        if (j >= 0 && j != ult) { copia_texto(O(j)->nome, "~tmp", GBM_NOME); copia_texto(O(ult)->nome, nome, GBM_NOME); nome_livre(O(j)->tipo, O(j)->nome); }
                        else copia_texto(O(ult)->nome, nome, GBM_NOME);
                    }
                    gbm_recalcular();
                    if (ciclo) { gbm_n = antes; erro("Esa construcción sería circular"); return -1; }
                    return 0;
                }
            }
        }
    }
    /* expresión */
    int raiz = gbm_analizar(rhs, true);
    if (raiz < 0) return -1;
    bool tx = gbm_usa_x(raiz);
    gbm_val_t v = gbm_avaliar(raiz, 0.5);
    int ex = (con_nome && !y_igual) ? gbm_buscar(nome) : -1;
    if (def_funcao && !tx) { /* f(x) = 3: constante, pero función */ }
    if (v.vec && tx) { erro("Una función de x tiene que dar un número"); return -1; }
    if (ex >= 0) return ya_existe_redefinir(ex, raiz, tx || def_funcao, ultimo);
    if (con_nome && !y_igual) nome_pedido = nome;
    int i;
    if (tx || def_funcao || y_igual) {
        i = nova(G_FUNCAO);
        if (i >= 0) O(i)->raiz = raiz;
    } else if (v.vec) {
        if (gbm_constante(raiz)) { i = nova(G_PTO_LIVRE); if (i >= 0) { O(i)->v[0] = v.a; O(i)->v[1] = v.b; } }
        else { i = nova(G_PTO_EXPR); if (i >= 0) O(i)->raiz = raiz; }
    } else {
        if (!gm_fin(v.a) && gbm_constante(raiz)) { erro("Ese valor no está definido"); nome_pedido = NULL; return -1; }
        if (gbm_constante(raiz)) {
            i = nova(G_SLIDER);
            if (i >= 0) {
                gbm_obj_t *o = O(i);
                o->v[0] = v.a;
                double m = gm_abs(v.a) <= 5 ? 5 : gm_ceil(gm_abs(v.a) * 2);
                o->min = -m; o->max = m; o->passo = 0.1;
                o->px = 20; o->py = 0;
            }
        } else { i = nova(G_NUM_EXPR); if (i >= 0) O(i)->raiz = raiz; }
    }
    nome_pedido = NULL;
    if (i < 0) { gbm_n = antes; return -1; }
    gbm_recalcular();
    if (ciclo) { gbm_n = antes; erro("Esa definición sería circular"); return -1; }
    if (ultimo) *ultimo = i;
    return 0;
}

bool gbm_apagar(int i)
{
    if (!valido(i)) return false;
    static unsigned char marca[GBM_OBJ_MAX];
    memset(marca, 0, sizeof marca);
    marca[i] = 1;
    for (int j = 0; j < gbm_n; j++) if (j != i && gbm_depende(j, i)) marca[j] = 1;
    static char tmp[131072];
    gbm_serializar_sem(tmp, sizeof tmp, marca);
    gbm_limpar();
    gbm_carregar(tmp, NULL);
    return true;
}

/* ------------------------------------------------------------ arrastre */

bool gbm_livre(int i)
{
    if (!valido(i)) return false;
    int t = O(i)->tipo;
    return t == G_PTO_LIVRE || t == G_PTO_EM || t == G_TEXTO;
}

void gbm_arrastar(int i, double wx, double wy)
{
    if (!valido(i)) return;
    gbm_obj_t *o = O(i);
    if (o->tipo == G_PTO_LIVRE) { o->v[0] = wx; o->v[1] = wy; }
    else if (o->tipo == G_TEXTO) { o->px = wx; o->py = wy; }
    else if (o->tipo == G_PTO_EM) {
        int pi = o->pai[0];
        if (!valido(pi)) return;
        calc(pi);
        gbm_obj_t *p = O(pi);
        if (!p->ok) return;
        if (gbm_eh_reta(p->tipo)) {
            double dx = p->v[2] - p->v[0], dy = p->v[3] - p->v[1], l2 = dx * dx + dy * dy;
            if (l2 == 0) return;
            double t = ((wx - p->v[0]) * dx + (wy - p->v[1]) * dy) / l2;
            int lim = limite_reta(p->tipo);
            if (lim == 2) t = t < 0 ? 0 : t > 1 ? 1 : t;
            else if (lim == 1 && t < 0) t = 0;
            o->param = t;
        } else if (gbm_eh_circ(p->tipo)) o->param = gm_atan2(wy - p->v[1], wx - p->v[0]);
        else if (p->tipo == G_FUNCAO) o->param = wx;
        else if (gbm_eh_poligono(p->tipo)) {
            double xs[64], ys[64];
            int n = gbm_vertices(pi, xs, ys, 64);
            double mejor = 1e300, ps = 0;
            for (int e = 0; e < n; e++) {
                int m = (e + 1) % n;
                double dx = xs[m] - xs[e], dy = ys[m] - ys[e], l2 = dx * dx + dy * dy;
                double t = l2 > 0 ? ((wx - xs[e]) * dx + (wy - ys[e]) * dy) / l2 : 0;
                t = t < 0 ? 0 : t > 1 ? 1 : t;
                double qx = xs[e] + t * dx - wx, qy = ys[e] + t * dy - wy, d = qx * qx + qy * qy;
                if (d < mejor) { mejor = d; ps = e + t; }
            }
            o->param = ps;
        }
    }
    gbm_recalcular();
}

/* ------------------------------------------------------------ ficheros .gbm */

typedef struct { char *s; int n, max; } esc_t;
static void e_put(esc_t *e, const char *t) { for (; *t; t++) if (e->n < e->max - 1) e->s[e->n++] = *t; e->s[e->n] = 0; }
static void e_num(esc_t *e, double v) { char b[48]; gbm_formatar(v, 10, b, sizeof b); e_put(e, b); }

static void escreve_obj(esc_t *e, int i)
{
    gbm_obj_t *o = O(i);
    char b[1100];
    switch (o->tipo) {
    case G_PTO_LIVRE: e_put(e, o->nome); e_put(e, " = ("); e_num(e, o->v[0]); e_put(e, ", "); e_num(e, o->v[1]); e_put(e, ")"); break;
    case G_PTO_EXPR: e_put(e, o->nome); e_put(e, " = "); gbm_escrever(o->raiz, b, sizeof b, false); e_put(e, b); break;
    case G_PTO_MEDIO: e_put(e, o->nome); e_put(e, " = PuntoMedio("); e_put(e, gbm_nome(o->pai[0])); e_put(e, ", "); e_put(e, gbm_nome(o->pai[1])); e_put(e, ")"); break;
    case G_PTO_INTER:
        e_put(e, o->nome); e_put(e, " = Interseca("); e_put(e, gbm_nome(o->pai[0])); e_put(e, ", "); e_put(e, gbm_nome(o->pai[1]));
        formatar_texto(b, sizeof b, ", %d)", o->k + 1); e_put(e, b);
        break;
    case G_PTO_EM: e_put(e, o->nome); e_put(e, " = PuntoEn("); e_put(e, gbm_nome(o->pai[0])); e_put(e, ", "); e_num(e, o->param); e_put(e, ")"); break;
    case G_PTO_RAIZ: case G_PTO_EXTR:
        e_put(e, o->nome); e_put(e, o->tipo == G_PTO_RAIZ ? " = Raiz(" : " = Extremo("); e_put(e, gbm_nome(o->pai[0]));
        formatar_texto(b, sizeof b, ", %d)", o->k + 1); e_put(e, b);
        break;
    case G_RETA: case G_SEMIRRETA: case G_SEGMENTO: case G_VETOR: case G_MEDIATRIZ: case G_PERP: case G_PARAL: case G_CIRC_PP: case G_CIRC_3:
    case G_BISSETRIZ: case G_ANGULO: case G_POLIGONO: {
        static const char *const nm[G_TIPOS] = { [G_RETA] = "Recta", [G_SEMIRRETA] = "Semirrecta", [G_SEGMENTO] = "Segmento", [G_VETOR] = "Vector",
            [G_MEDIATRIZ] = "Mediatriz", [G_PERP] = "Perpendicular", [G_PARAL] = "Paralela", [G_CIRC_PP] = "Circunferencia",
            [G_CIRC_3] = "Circunferencia", [G_BISSETRIZ] = "Bisectriz", [G_ANGULO] = "Angulo", [G_POLIGONO] = "Poligono" };
        e_put(e, o->nome); e_put(e, " = "); e_put(e, nm[o->tipo]); e_put(e, "(");
        for (int k = 0; k < o->npai; k++) { if (k) e_put(e, ", "); e_put(e, gbm_nome(o->pai[k])); }
        e_put(e, ")");
        break;
    }
    case G_CIRC_R: e_put(e, o->nome); e_put(e, " = Circunferencia("); e_put(e, gbm_nome(o->pai[0])); e_put(e, ", "); gbm_escrever(o->raiz, b, sizeof b, false); e_put(e, b); e_put(e, ")"); break;
    case G_POLREG:
        e_put(e, o->nome); e_put(e, " = Poligono("); e_put(e, gbm_nome(o->pai[0])); e_put(e, ", "); e_put(e, gbm_nome(o->pai[1]));
        formatar_texto(b, sizeof b, ", %d)", o->k); e_put(e, b);
        break;
    case G_DIST: e_put(e, o->nome); e_put(e, " = Distancia("); e_put(e, gbm_nome(o->pai[0])); e_put(e, ", "); e_put(e, gbm_nome(o->pai[1])); e_put(e, ")"); break;
    case G_AREA: e_put(e, o->nome); e_put(e, " = Area("); e_put(e, gbm_nome(o->pai[0])); e_put(e, ")"); break;
    case G_NUM_EXPR: e_put(e, o->nome); e_put(e, " = "); gbm_escrever(o->raiz, b, sizeof b, false); e_put(e, b); break;
    case G_SLIDER:
        e_put(e, o->nome); e_put(e, " = Deslizador("); e_num(e, o->min); e_put(e, ", "); e_num(e, o->max); e_put(e, ", "); e_num(e, o->passo);
        e_put(e, ", "); e_num(e, o->v[0]); e_put(e, ")");
        break;
    case G_TEXTO:
        e_put(e, o->nome); e_put(e, " = Texto(\""); e_put(e, o->texto); e_put(e, "\", "); e_num(e, o->px); e_put(e, ", "); e_num(e, o->py); e_put(e, ")");
        break;
    case G_FUNCAO: e_put(e, o->nome); e_put(e, "(x) = "); gbm_escrever(o->raiz, b, sizeof b, false); e_put(e, b); break;
    }
    e_put(e, "\n");
    /* propiedades que no son las de por defecto */
    gbm_obj_t d;
    memset(&d, 0, sizeof d);
    d.tipo = o->tipo;
    def_prop(&d);
    if (o->tipo == G_FUNCAO) d.cor = o->cor;   /* el color de una función rota según el orden de creación */
    bool dif = o->cor != d.cor || o->grosor != d.grosor || o->estilo != d.estilo || o->rotulo != d.rotulo || o->visivel != d.visivel;
    if (dif) {
        e_put(e, "propiedades "); e_put(e, o->nome); e_put(e, ":");
        formatar_texto(b, sizeof b, " color=%06x grosor=%d estilo=%d rotulo=%d visible=%d", (unsigned)o->cor, o->grosor, o->estilo, o->rotulo, o->visivel);
        e_put(e, b);
        e_put(e, "\n");
    }
    if (o->tipo == G_SLIDER && (o->px != 20 || o->py != 0)) {
        e_put(e, "posicion "); e_put(e, o->nome); e_put(e, ": "); e_num(e, o->px); e_put(e, " "); e_num(e, o->py); e_put(e, "\n");
    }
}

static void emite(esc_t *e, int i, unsigned char *hecho, const unsigned char *omitir, int prof)
{
    if (hecho[i] || (omitir && omitir[i]) || prof > 450) return;
    hecho[i] = 1;
    int d[GBM_PAI_MAX * 2];
    int n = deps(i, d, GBM_PAI_MAX * 2);
    for (int k = 0; k < n; k++) if (valido(d[k])) emite(e, d[k], hecho, omitir, prof + 1);
    escreve_obj(e, i);
}

int gbm_serializar_sem(char *s, int max, const unsigned char *omitir)
{
    esc_t e = { s, 0, max };
    s[0] = 0;
    e_put(&e, "# GebraMini 1\n");
    unsigned char hecho[GBM_OBJ_MAX] = { 0 };
    for (int i = 0; i < gbm_n; i++) emite(&e, i, hecho, omitir, 0);
    return e.n;
}

int gbm_serializar(char *s, int max, const double *vista)
{
    esc_t e = { s, 0, max };
    s[0] = 0;
    e_put(&e, "# GebraMini 1\n");
    if (vista) {
        e_put(&e, "vista "); e_num(&e, vista[0]); e_put(&e, " "); e_num(&e, vista[1]); e_put(&e, " "); e_num(&e, vista[2]); e_put(&e, "\n");
    }
    unsigned char hecho[GBM_OBJ_MAX] = { 0 };
    for (int i = 0; i < gbm_n; i++) emite(&e, i, hecho, NULL, 0);
    return e.n;
}

static void propiedades(char *s)   /* "A: color=FF0000 grosor=3 ..." */
{
    char *dp = acha_char(s, ':');
    if (!dp) return;
    *dp = 0;
    int i = gbm_buscar(recorta(s));
    if (i < 0) return;
    char *p = dp + 1;
    gbm_obj_t *o = O(i);
    for (;;) {
        while (*p == ' ') p++;
        if (!*p) break;
        char *k = p;
        while (*p && *p != '=' && *p != ' ') p++;
        if (*p != '=') break;
        *p++ = 0;
        char *v = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
        unsigned val = 0;
        if (compara_texto(k, "color") == 0) {
            for (char *c = v; *c; c++) val = val * 16 + (unsigned)(*c >= 'a' ? *c - 'a' + 10 : *c >= 'A' ? *c - 'A' + 10 : *c - '0');
            o->cor = val & 0xFFFFFF;
        } else {
            for (char *c = v; *c >= '0' && *c <= '9'; c++) val = val * 10 + (unsigned)(*c - '0');
            if (compara_texto(k, "grosor") == 0) o->grosor = (uint8_t)(val < 1 ? 1 : val > 9 ? 9 : val);
            else if (compara_texto(k, "estilo") == 0) o->estilo = (uint8_t)(val > 2 ? 0 : val);
            else if (compara_texto(k, "rotulo") == 0) o->rotulo = val != 0;
            else if (compara_texto(k, "visible") == 0) o->visivel = val != 0;
        }
    }
}

int gbm_carregar(const char *texto, double *vista)
{
    int linha = 0;
    gbm_limpar();
    const char *p = texto;
    while (*p) {
        char l[1100];
        int n = 0;
        while (*p && *p != '\n' && n < (int)sizeof l - 1) l[n++] = *p++;
        if (*p == '\n') p++;
        else while (*p && *p != '\n') p++;
        l[n] = 0;
        linha++;
        char *s = recorta(l);
        if (!*s || *s == '#') continue;
        if (prefijo_n("vista", s) && (s[5] == ' ')) {
            if (vista) {
                const char *q = s + 5;
                for (int k = 0; k < 3; k++) {
                    while (*q == ' ') q++;
                    bool neg = *q == '-';
                    if (neg) q++;
                    double v = gbm_texto_num(q, &q);
                    vista[k] = neg ? -v : v;
                }
            }
            continue;
        }
        if (prefijo_n("propiedades", s) && s[11] == ' ') { propiedades(s + 12); continue; }
        if (prefijo_n("posicion", s) && s[8] == ' ') {
            char *dp = acha_char(s + 9, ':');
            if (dp) {
                *dp = 0;
                int i = gbm_buscar(recorta(s + 9));
                const char *q = dp + 1;
                while (*q == ' ') q++;
                bool n1 = *q == '-'; if (n1) q++;
                double x = gbm_texto_num(q, &q);
                while (*q == ' ') q++;
                bool n2 = *q == '-'; if (n2) q++;
                double y = gbm_texto_num(q, &q);
                if (i >= 0) { O(i)->px = n1 ? -x : x; O(i)->py = n2 ? -y : y; }
            }
            continue;
        }
        if (gbm_orden(s, NULL) != 0) {
            char m[96];
            formatar_texto(m, sizeof m, "Línea %d: %s", linha, gbm_erro);
            copia_texto(gbm_erro, m, sizeof gbm_erro);
            return -1;
        }
    }
    gbm_recalcular();
    return 0;
}
