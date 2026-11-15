// SPDX-License-Identifier: GPL-3.0-or-later
// GebraMini, © 2026 Alejandro Anuncibay: prototipo original (C++/SDL) del mantenedor; esta versión nativa
// para SaguiOS está escrita desde cero a partir de sus ideas. Independiente de GeoGebra.
/* gbm_expr.c - expresiones de GebraMini (D309): analizador, evaluador, derivada simbólica, escritor y
 * búsqueda numérica de ceros. Los números son dobles de la FPU; un valor puede ser un número o un
 * punto (a, b), de modo que «B = A + (3, 0)» es una expresión como las demás. */
#include "gbm.h"
#include "gbm_mat.h"

char gbm_erro[96];

static gbm_nodo_t nodos[GBM_NODOS];
static int n_nodos;

void gbm_nodos_reiniciar(void) { n_nodos = 0; }
int  gbm_nodos_usados(void) { return n_nodos; }

static void erro(const char *m)
{
    int i = 0;
    for (; m[i] && i < (int)sizeof gbm_erro - 1; i++) gbm_erro[i] = m[i];
    gbm_erro[i] = 0;
}

static int nodo(int tipo, int fn, int a, int b, double num)
{
    if (n_nodos >= GBM_NODOS) { erro("Expresión demasiado grande"); return -1; }
    nodos[n_nodos] = (gbm_nodo_t){ (uint8_t)tipo, (uint8_t)fn, (int16_t)a, (int16_t)b, num };
    return n_nodos++;
}

static bool eh_num(int n, double *v) { if (n >= 0 && nodos[n].tipo == N_NUM) { if (v) *v = nodos[n].num; return true; } return false; }

/* Constructores que simplifican (0, 1 y constantes), para que las derivadas salgan limpias. */
static int mk_num(double v) { return nodo(N_NUM, 0, 0, 0, v); }
static int mk_neg(int a)
{
    double v;
    if (a < 0) return -1;
    if (eh_num(a, &v)) return mk_num(-v);
    if (nodos[a].tipo == N_NEG) return nodos[a].a;
    return nodo(N_NEG, 0, a, 0, 0);
}
static int mk_add(int a, int b)
{
    double x, y;
    if (a < 0 || b < 0) return -1;
    bool na = eh_num(a, &x), nb = eh_num(b, &y);
    if (na && nb) return mk_num(x + y);
    if (na && x == 0) return b;
    if (nb && y == 0) return a;
    return nodo(N_ADD, 0, a, b, 0);
}
static int mk_sub(int a, int b)
{
    double x, y;
    if (a < 0 || b < 0) return -1;
    bool na = eh_num(a, &x), nb = eh_num(b, &y);
    if (na && nb) return mk_num(x - y);
    if (nb && y == 0) return a;
    if (na && x == 0) return mk_neg(b);
    return nodo(N_SUB, 0, a, b, 0);
}
static int mk_mul(int a, int b)
{
    double x, y;
    if (a < 0 || b < 0) return -1;
    bool na = eh_num(a, &x), nb = eh_num(b, &y);
    if (na && nb) return mk_num(x * y);
    if ((na && x == 0) || (nb && y == 0)) return mk_num(0);
    if (na && x == 1) return b;
    if (nb && y == 1) return a;
    if (na && x == -1) return mk_neg(b);
    if (nb && y == -1) return mk_neg(a);
    /* 3 * (2 * x) = 6 * x, (2 * x) * 4 = 8 * x */
    if (na && nodos[b].tipo == N_MUL && nodos[nodos[b].a].tipo == N_NUM) return mk_mul(mk_num(x * nodos[nodos[b].a].num), nodos[b].b);
    if (nb && nodos[a].tipo == N_MUL && nodos[nodos[a].a].tipo == N_NUM) return mk_mul(mk_num(y * nodos[nodos[a].a].num), nodos[a].b);
    return nodo(N_MUL, 0, a, b, 0);
}
static int mk_div(int a, int b)
{
    double x, y;
    if (a < 0 || b < 0) return -1;
    bool na = eh_num(a, &x), nb = eh_num(b, &y);
    if (na && nb && y != 0) return mk_num(x / y);
    if (na && x == 0) return mk_num(0);
    if (nb && y == 1) return a;
    if (nb && y != 0 && nodos[a].tipo == N_MUL && nodos[nodos[a].a].tipo == N_NUM) {   /* (2 * x) / 4 = 0.5 * x, si es exacto */
        double p = nodos[nodos[a].a].num, q = p / y;
        if (q * y == p) return mk_mul(mk_num(q), nodos[a].b);
    }
    return nodo(N_DIV, 0, a, b, 0);
}
static int mk_pow(int a, int b)
{
    double x, y;
    if (a < 0 || b < 0) return -1;
    bool na = eh_num(a, &x), nb = eh_num(b, &y);
    if (nb && y == 1) return a;
    if (nb && y == 0) return mk_num(1);
    if (na && nb) return mk_num(gm_pow(x, y));
    return nodo(N_POW, 0, a, b, 0);
}
static int mk_f1(int f, int a) { return a < 0 ? -1 : nodo(N_F1, f, a, 0, 0); }

/* ------------------------------------------------------------ analizador */

static const char *P;          /* posición en el texto */
static bool con_x;
static int  prof_ana;

static void salta(void) { while (*P == ' ' || *P == '\t') P++; }
static bool eh_letra(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || ((unsigned char)c >= 0xC0 && (unsigned char)c != 0xD7 && (unsigned char)c != 0xF7); }
static bool eh_dig(int c) { return c >= '0' && c <= '9'; }

/* Minúscula sin tilde (Latin-1): «Raíz» y «raiz» son lo mismo. */
static int norm(int c)
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
static bool igual_norm(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) if (norm(*a) != norm(*b)) return false;
    return *a == 0 && *b == 0;
}

double gbm_texto_num(const char *s, const char **fim)
{
    double v = 0;
    const char *q = s;
    while (eh_dig(*q)) v = v * 10 + (*q++ - '0');
    if (*q == '.') {
        q++;
        double f = 0.1;
        while (eh_dig(*q)) { v += (*q++ - '0') * f; f *= 0.1; }
        /* Los decimales acumulados así pierden precisión: se reconstruyen como entero / potencia de 10. */
        const char *d = s;
        while (eh_dig(*d)) d++;
        d++;
        double ent = 0, den = 1;
        int cuantos = 0;
        for (const char *t = s; t < q; t++) {
            if (*t == '.') continue;
            if (t > d - 1 && cuantos < 17) { ent = ent * 10 + (*t - '0'); den *= 10; cuantos++; }
        }
        double parte = 0;
        for (const char *t = s; *t != '.'; t++) parte = parte * 10 + (*t - '0');
        v = parte + (den > 1 ? ent / den : 0);
    }
    if ((*q == 'e' || *q == 'E') && (eh_dig(q[1]) || ((q[1] == '-' || q[1] == '+') && eh_dig(q[2])))) {
        q++;
        int sg = 1, ex = 0;
        if (*q == '-') { sg = -1; q++; } else if (*q == '+') q++;
        while (eh_dig(*q)) { ex = ex * 10 + (*q++ - '0'); if (ex > 400) ex = 400; }
        v *= gm_pow(10.0, sg * ex);
    }
    if (fim) *fim = q;
    return v;
}

typedef struct { const char *nome; uint8_t f; uint8_t dos; } fn_t;
static const fn_t FUNCOES[] = {
    { "sin", F_SIN, 0 }, { "sen", F_SIN, 0 }, { "cos", F_COS, 0 }, { "tan", F_TAN, 0 }, { "tg", F_TAN, 0 },
    { "asin", F_ASIN, 0 }, { "arcsin", F_ASIN, 0 }, { "arcsen", F_ASIN, 0 }, { "acos", F_ACOS, 0 }, { "arccos", F_ACOS, 0 },
    { "atan", F_ATAN, 0 }, { "arctan", F_ATAN, 0 }, { "arctg", F_ATAN, 0 },
    { "sinh", F_SINH, 0 }, { "cosh", F_COSH, 0 }, { "tanh", F_TANH, 0 },
    { "exp", F_EXP, 0 }, { "ln", F_LN, 0 }, { "log10", F_LOG10, 0 }, { "log2", F_LOG2, 0 },
    { "sqrt", F_SQRT, 0 }, { "raizcuad", F_SQRT, 0 }, { "cbrt", F_CBRT, 0 },
    { "abs", F_ABS, 0 }, { "floor", F_FLOOR, 0 }, { "piso", F_FLOOR, 0 }, { "ceil", F_CEIL, 0 }, { "techo", F_CEIL, 0 },
    { "round", F_ROUND, 0 }, { "redondear", F_ROUND, 0 }, { "sign", F_SIGN, 0 }, { "signo", F_SIGN, 0 },
    { "abscisa", F_ABSCISA, 0 }, { "ordenada", F_ORDENADA, 0 },
    { "atan2", F2_ATAN2, 1 }, { "min", F2_MIN, 1 }, { "max", F2_MAX, 1 }, { "mod", F2_MOD, 1 },
    { "hypot", F2_HYPOT, 1 }, { "pow", F2_POW, 1 },
};
#define NFUNCOES ((int)(sizeof FUNCOES / sizeof FUNCOES[0]))
/* «log» es de base 10 con un argumento y «log(base, x)» con dos. */

static int parse_expr(void);

static int parse_primario(void)
{
    salta();
    if (++prof_ana > 60) { erro("Expresión demasiado anidada"); return -1; }
    int r = -1;
    if (eh_dig(*P) || (*P == '.' && eh_dig(P[1]))) {
        const char *fim;
        double v = gbm_texto_num(P, &fim);
        P = fim;
        r = mk_num(v);
    } else if (*P == '(') {
        P++;
        int a = parse_expr();
        if (a < 0) return -1;
        salta();
        if (*P == ',') {
            P++;
            int b = parse_expr();
            if (b < 0) return -1;
            salta();
            if (*P != ')') { erro("Falta cerrar el paréntesis"); return -1; }
            P++;
            r = nodo(N_PAR, 0, a, b, 0);
        } else {
            if (*P != ')') { erro("Falta cerrar el paréntesis"); return -1; }
            P++;
            r = a;
        }
    } else if (eh_letra(*P)) {
        char nome[GBM_NOME + 8];
        int k = 0;
        while ((eh_letra(*P) || eh_dig(*P)) && k < (int)sizeof nome - 1) nome[k++] = *P++;
        nome[k] = 0;
        bool x_var = con_x && nome[0] == 'x' && nome[1] == 0;
        if (x_var) {
            r = nodo(N_X, 0, 0, 0, 0);
        } else {
            const char *q = P;
            while (*q == ' ') q++;
            bool llamada = *q == '(';
            int obj = gbm_buscar(nome);
            if (llamada && !(obj >= 0 && gbm_obj[obj].tipo != G_FUNCAO && false)) {
                int fi = -1;
                if (!(obj >= 0 && gbm_obj[obj].tipo == G_FUNCAO)) {
                    for (int i = 0; i < NFUNCOES; i++) if (igual_norm(nome, FUNCOES[i].nome)) { fi = i; break; }
                    if (fi < 0 && igual_norm(nome, "log")) fi = -2;
                }
                if (obj >= 0 && gbm_obj[obj].tipo == G_FUNCAO) {
                    P = q + 1;
                    int a = parse_expr();
                    if (a < 0) return -1;
                    salta();
                    if (*P != ')') { erro("Falta cerrar el paréntesis"); return -1; }
                    P++;
                    r = nodo(N_LLAMA, 0, obj, a, 0);
                } else if (fi != -1) {
                    P = q + 1;
                    int a = parse_expr();
                    if (a < 0) return -1;
                    salta();
                    int b = -1;
                    if (*P == ',') {
                        P++;
                        b = parse_expr();
                        if (b < 0) return -1;
                        salta();
                    }
                    if (*P != ')') { erro("Falta cerrar el paréntesis"); return -1; }
                    P++;
                    if (fi == -2) r = b < 0 ? mk_f1(F_LOG10, a) : nodo(N_F2, F2_LOG, a, b, 0);
                    else if (FUNCOES[fi].dos) {
                        if (b < 0) { erro("Esa función necesita dos argumentos"); return -1; }
                        r = nodo(N_F2, FUNCOES[fi].f, a, b, 0);
                    } else {
                        if (b >= 0) { erro("Esa función solo lleva un argumento"); return -1; }
                        r = mk_f1(FUNCOES[fi].f, a);
                    }
                } else if (obj >= 0) {
                    r = nodo(N_REF, 0, obj, 0, 0);   /* «A(…)» sin ser función: producto implícito, lo resuelve el término */
                } else {
                    char m[96];
                    formatar_texto(m, sizeof m, "Función o nombre desconocido: %s", nome);
                    erro(m);
                    return -1;
                }
            } else if (obj >= 0) {
                r = nodo(N_REF, 0, obj, 0, 0);
            } else if (compara_texto(nome, "pi") == 0) {
                r = mk_num(GM_PI);
            } else if (compara_texto(nome, "e") == 0) {
                r = mk_num(GM_E);
            } else {
                char m[96];
                if (nome[0] == 'x' && nome[1] == 0) erro("«x» solo se puede usar dentro de una función");
                else { formatar_texto(m, sizeof m, "Nombre desconocido: %s", nome); erro(m); }
                return -1;
            }
        }
    } else {
        if (!*P) erro("La expresión está incompleta");
        else { char m[96]; formatar_texto(m, sizeof m, "Carácter inesperado: %c", *P); erro(m); }
        return -1;
    }
    if (r < 0) return -1;
    /* ² y ³ postfijos */
    for (;;) {
        if ((unsigned char)*P == 0xB2) { P++; r = mk_pow(r, mk_num(2)); }
        else if ((unsigned char)*P == 0xB3) { P++; r = mk_pow(r, mk_num(3)); }
        else break;
    }
    prof_ana--;
    return r;
}

static int parse_unario(void);

static int parse_potencia(void)
{
    int a = parse_primario();
    if (a < 0) return -1;
    salta();
    if (*P == '^') {
        P++;
        int b = parse_unario();      /* asociativa por la derecha: 2^3^2 = 2^9 */
        if (b < 0) return -1;
        return mk_pow(a, b);
    }
    return a;
}

static int parse_unario(void)
{
    salta();
    if (*P == '-') { P++; int a = parse_unario(); return a < 0 ? -1 : mk_neg(a); }
    if (*P == '+') { P++; return parse_unario(); }
    return parse_potencia();
}

static bool empieza_primario(void)
{
    return eh_letra(*P) || *P == '(' || eh_dig(*P) || (*P == '.' && eh_dig(P[1]));
}

static int parse_termino(void)
{
    int a = parse_unario();
    if (a < 0) return -1;
    for (;;) {
        salta();
        if (*P == '*' || (unsigned char)*P == 0xD7) { P++; int b = parse_unario(); if (b < 0) return -1; a = mk_mul(a, b); }
        else if (*P == '/' || (unsigned char)*P == 0xF7) { P++; int b = parse_unario(); if (b < 0) return -1; a = mk_div(a, b); }
        else if (empieza_primario() && !eh_dig(*P)) {   /* 2x, 3(x+1), (x+1)(x-1): producto implícito */
            int b = parse_potencia();
            if (b < 0) return -1;
            a = mk_mul(a, b);
        } else break;
    }
    return a;
}

static int parse_expr(void)
{
    int a = parse_termino();
    if (a < 0) return -1;
    for (;;) {
        salta();
        if (*P == '+') { P++; int b = parse_termino(); if (b < 0) return -1; a = mk_add(a, b); }
        else if (*P == '-') { P++; int b = parse_termino(); if (b < 0) return -1; a = mk_sub(a, b); }
        else break;
    }
    return a;
}

int gbm_analizar(const char *texto, bool x_ok)
{
    P = texto;
    con_x = x_ok;
    prof_ana = 0;
    gbm_erro[0] = 0;
    salta();
    if (!*P) { erro("La expresión está vacía"); return -1; }
    int r = parse_expr();
    if (r < 0) { if (!gbm_erro[0]) erro("Expresión no válida"); return -1; }
    salta();
    if (*P) {
        char m[96];
        formatar_texto(m, sizeof m, "Sobra algo al final: %s", P);
        erro(m);
        return -1;
    }
    return r;
}

/* ------------------------------------------------------------ evaluación */

static gbm_val_t num(double v) { return (gbm_val_t){ v, 0, 0 }; }
static gbm_val_t vec(double a, double b) { return (gbm_val_t){ a, b, 1 }; }
static gbm_val_t mal(void) { return (gbm_val_t){ GM_NAN, GM_NAN, 0 }; }

static double f1(int f, double u)
{
    switch (f) {
    case F_SIN: return gm_sin(u);
    case F_COS: return gm_cos(u);
    case F_TAN: return gm_tan(u);
    case F_ASIN: return gm_asin(u);
    case F_ACOS: return gm_acos(u);
    case F_ATAN: return gm_atan(u);
    case F_SINH: return gm_sinh(u);
    case F_COSH: return gm_cosh(u);
    case F_TANH: return gm_tanh(u);
    case F_EXP: return gm_exp(u);
    case F_LN: return gm_ln(u);
    case F_LOG10: return gm_log10(u);
    case F_LOG2: return gm_log2(u);
    case F_SQRT: return gm_sqrt(u);
    case F_CBRT: return gm_cbrt(u);
    case F_ABS: return gm_abs(u);
    case F_FLOOR: return gm_floor(u);
    case F_CEIL: return gm_ceil(u);
    case F_ROUND: return gm_round(u);
    case F_SIGN: return u > 0 ? 1.0 : u < 0 ? -1.0 : 0.0;
    }
    return GM_NAN;
}

static int prof_ev;

static gbm_val_t ev(int n, double x)
{
    if (n < 0 || prof_ev > 200) return mal();
    const gbm_nodo_t *d = &nodos[n];
    switch (d->tipo) {
    case N_NUM: return num(d->num);
    case N_X: return num(x);
    case N_REF: { gbm_val_t v; if (!gbm_ref_valor(d->a, &v)) return mal(); return v; }
    case N_PAR: {
        prof_ev++;
        gbm_val_t a = ev(d->a, x), b = ev(d->b, x);
        prof_ev--;
        if (a.vec || b.vec) return mal();
        return vec(a.a, b.a);
    }
    case N_NEG: { prof_ev++; gbm_val_t a = ev(d->a, x); prof_ev--; a.a = -a.a; a.b = -a.b; return a; }
    case N_ADD: case N_SUB: case N_MUL: case N_DIV: case N_POW: {
        prof_ev++;
        gbm_val_t a = ev(d->a, x), b = ev(d->b, x);
        prof_ev--;
        switch (d->tipo) {
        case N_ADD: if (a.vec != b.vec) return mal(); return (gbm_val_t){ a.a + b.a, a.b + b.b, a.vec };
        case N_SUB: if (a.vec != b.vec) return mal(); return (gbm_val_t){ a.a - b.a, a.b - b.b, a.vec };
        case N_MUL:
            if (!a.vec && !b.vec) return num(a.a * b.a);
            if (a.vec && b.vec) return num(a.a * b.a + a.b * b.b);
            if (a.vec) return vec(a.a * b.a, a.b * b.a);
            return vec(b.a * a.a, b.b * a.a);
        case N_DIV:
            if (b.vec) return mal();
            if (a.vec) return vec(a.a / b.a, a.b / b.a);
            return num(a.a / b.a);
        default:
            if (a.vec || b.vec) return mal();
            return num(gm_pow(a.a, b.a));
        }
    }
    case N_F1: {
        prof_ev++;
        gbm_val_t a = ev(d->a, x);
        prof_ev--;
        if (d->fn == F_ABSCISA) return a.vec ? num(a.a) : mal();
        if (d->fn == F_ORDENADA) return a.vec ? num(a.b) : mal();
        if (a.vec) return d->fn == F_ABS ? num(gm_hypot(a.a, a.b)) : mal();
        return num(f1(d->fn, a.a));
    }
    case N_F2: {
        prof_ev++;
        gbm_val_t a = ev(d->a, x), b = ev(d->b, x);
        prof_ev--;
        if (a.vec || b.vec) return mal();
        switch (d->fn) {
        case F2_ATAN2: return num(gm_atan2(a.a, b.a));
        case F2_MIN: return num(a.a < b.a ? a.a : b.a);
        case F2_MAX: return num(a.a > b.a ? a.a : b.a);
        case F2_MOD: return num(b.a == 0 ? GM_NAN : a.a - b.a * gm_floor(a.a / b.a));
        case F2_HYPOT: return num(gm_hypot(a.a, b.a));
        case F2_POW: return num(gm_pow(a.a, b.a));
        case F2_LOG: return num(gm_ln(b.a) / gm_ln(a.a));
        }
        return mal();
    }
    case N_LLAMA: {
        prof_ev++;
        gbm_val_t a = ev(d->b, x);
        prof_ev--;
        if (a.vec) return mal();
        return num(gbm_valor_funcao(d->a, a.a));
    }
    case N_DNUM: {
        prof_ev++;
        double h = 1e-3 * (1.0 + gm_abs(x));
        gbm_val_t f2 = ev(d->a, x + 2 * h), f1v = ev(d->a, x + h), g1 = ev(d->a, x - h), g2 = ev(d->a, x - 2 * h);
        prof_ev--;
        if (f1v.vec) return mal();
        return num((-f2.a + 8 * f1v.a - 8 * g1.a + g2.a) / (12 * h));
    }
    }
    return mal();
}

gbm_val_t gbm_avaliar(int raiz, double x)
{
    prof_ev = 0;
    return ev(raiz, x);
}

double gbm_derivada_num(int raiz, double x)
{
    int n = nodo(N_DNUM, 0, raiz, 0, 0);
    gbm_val_t v = n < 0 ? mal() : gbm_avaliar(n, x);
    if (n == n_nodos - 1) n_nodos--;
    return v.a;
}

/* ------------------------------------------------------------ análisis del árbol */

static void refs_rec(int n, int *saida, int *q, int max, int prof)
{
    if (n < 0 || prof > 400) return;
    const gbm_nodo_t *d = &nodos[n];
    switch (d->tipo) {
    case N_REF: {
        for (int i = 0; i < *q; i++) if (saida[i] == d->a) return;
        if (*q < max) saida[(*q)++] = d->a;
        return;
    }
    case N_LLAMA: {
        bool ya = false;
        for (int i = 0; i < *q; i++) if (saida[i] == d->a) ya = true;
        if (!ya && *q < max) saida[(*q)++] = d->a;
        refs_rec(d->b, saida, q, max, prof + 1);
        return;
    }
    case N_NUM: case N_X: return;
    case N_NEG: case N_F1: case N_DNUM: refs_rec(d->a, saida, q, max, prof + 1); return;
    default: refs_rec(d->a, saida, q, max, prof + 1); refs_rec(d->b, saida, q, max, prof + 1);
    }
}
int gbm_refs(int raiz, int *saida, int max) { int q = 0; refs_rec(raiz, saida, &q, max, 0); return q; }

static bool usa_x_rec(int n, int prof)
{
    if (n < 0 || prof > 400) return false;
    const gbm_nodo_t *d = &nodos[n];
    switch (d->tipo) {
    case N_X: return true;
    case N_NUM: case N_REF: return false;
    case N_NEG: case N_F1: case N_DNUM: return usa_x_rec(d->a, prof + 1);
    case N_LLAMA: return usa_x_rec(d->b, prof + 1);
    default: return usa_x_rec(d->a, prof + 1) || usa_x_rec(d->b, prof + 1);
    }
}
bool gbm_usa_x(int raiz) { return usa_x_rec(raiz, 0); }

bool gbm_constante(int raiz)
{
    int r[1];
    return !gbm_usa_x(raiz) && gbm_refs(raiz, r, 1) == 0;
}

/* Copia con sustitución de x (sust >= 0) por el árbol "sust". */
static int copia_rec(int n, int sust, int prof)
{
    if (n < 0 || prof > 400) return -1;
    const gbm_nodo_t d = nodos[n];
    switch (d.tipo) {
    case N_NUM: return sust == -2 ? mk_num(d.num) : n;
    case N_X: return sust >= 0 ? sust : n;
    case N_REF: return n;
    case N_NEG: case N_F1: case N_DNUM: { int a = copia_rec(d.a, sust, prof + 1); return a < 0 ? -1 : nodo(d.tipo, d.fn, a, 0, 0); }
    case N_LLAMA: { int b = copia_rec(d.b, sust, prof + 1); return b < 0 ? -1 : nodo(N_LLAMA, 0, d.a, b, 0); }
    default: {
        int a = copia_rec(d.a, sust, prof + 1), b = copia_rec(d.b, sust, prof + 1);
        return a < 0 || b < 0 ? -1 : nodo(d.tipo, d.fn, a, b, 0);
    }
    }
}
int gbm_copiar_arbol(int raiz) { return copia_rec(raiz, -2, 0); }

/* ------------------------------------------------------------ derivada simbólica */

static int deriv(int n, int prof);

static int dnum(int n) { return nodo(N_DNUM, 0, n, 0, 0); }

static int deriv(int n, int prof)
{
    if (n < 0 || prof > 120) return -1;
    const gbm_nodo_t d = nodos[n];
    switch (d.tipo) {
    case N_NUM: case N_REF: return mk_num(0);
    case N_X: return mk_num(1);
    case N_NEG: return mk_neg(deriv(d.a, prof + 1));
    case N_ADD: return mk_add(deriv(d.a, prof + 1), deriv(d.b, prof + 1));
    case N_SUB: return mk_sub(deriv(d.a, prof + 1), deriv(d.b, prof + 1));
    case N_MUL: return mk_add(mk_mul(deriv(d.a, prof + 1), d.b), mk_mul(d.a, deriv(d.b, prof + 1)));
    case N_DIV: {
        int num_ = mk_sub(mk_mul(deriv(d.a, prof + 1), d.b), mk_mul(d.a, deriv(d.b, prof + 1)));
        return mk_div(num_, mk_pow(d.b, mk_num(2)));
    }
    case N_POW: {
        bool bx = usa_x_rec(d.a, 0), ex = usa_x_rec(d.b, 0);
        if (!ex) {
            int du = deriv(d.a, prof + 1);
            return mk_mul(mk_mul(d.b, mk_pow(d.a, mk_sub(d.b, mk_num(1)))), du);
        }
        if (!bx) return mk_mul(mk_mul(n, mk_f1(F_LN, d.a)), deriv(d.b, prof + 1));
        /* u^v (v' ln u + v u' / u) */
        int t1 = mk_mul(deriv(d.b, prof + 1), mk_f1(F_LN, d.a));
        int t2 = mk_div(mk_mul(d.b, deriv(d.a, prof + 1)), d.a);
        return mk_mul(n, mk_add(t1, t2));
    }
    case N_F1: {
        int u = d.a, du = deriv(u, prof + 1);
        if (du < 0) return -1;
        double z;
        if (eh_num(du, &z) && z == 0) return du;
        int g = -1;
        switch (d.fn) {
        case F_SIN: g = mk_f1(F_COS, u); break;
        case F_COS: g = mk_neg(mk_f1(F_SIN, u)); break;
        case F_TAN: g = mk_div(mk_num(1), mk_pow(mk_f1(F_COS, u), mk_num(2))); break;
        case F_ASIN: g = mk_div(mk_num(1), mk_f1(F_SQRT, mk_sub(mk_num(1), mk_pow(u, mk_num(2))))); break;
        case F_ACOS: g = mk_neg(mk_div(mk_num(1), mk_f1(F_SQRT, mk_sub(mk_num(1), mk_pow(u, mk_num(2)))))); break;
        case F_ATAN: g = mk_div(mk_num(1), mk_add(mk_num(1), mk_pow(u, mk_num(2)))); break;
        case F_SINH: g = mk_f1(F_COSH, u); break;
        case F_COSH: g = mk_f1(F_SINH, u); break;
        case F_TANH: g = mk_div(mk_num(1), mk_pow(mk_f1(F_COSH, u), mk_num(2))); break;
        case F_EXP: g = n; break;
        case F_LN: g = mk_div(mk_num(1), u); break;
        case F_LOG10: g = mk_div(mk_num(1), mk_mul(u, mk_num(2.30258509299404568402))); break;
        case F_LOG2: g = mk_div(mk_num(1), mk_mul(u, mk_num(0.69314718055994530942))); break;
        case F_SQRT: g = mk_div(mk_num(1), mk_mul(mk_num(2), n)); break;
        case F_CBRT: g = mk_div(mk_num(1), mk_mul(mk_num(3), mk_pow(n, mk_num(2)))); break;
        case F_ABS: g = mk_f1(F_SIGN, u); break;
        case F_FLOOR: case F_CEIL: case F_ROUND: case F_SIGN: return mk_num(0);
        default: return dnum(n);
        }
        return mk_mul(g, du);
    }
    case N_F2:
        if (d.fn == F2_POW) { int p = mk_pow(d.a, d.b); return deriv(p, prof + 1); }
        if (d.fn == F2_LOG) {
            double b;
            if (eh_num(d.a, &b)) { int du = deriv(d.b, prof + 1); return mk_div(du, mk_mul(d.b, mk_num(gm_ln(b)))); }
        }
        return dnum(n);
    case N_PAR: return nodo(N_PAR, 0, deriv(d.a, prof + 1), deriv(d.b, prof + 1), 0);
    case N_LLAMA: {
        /* La función llamada se copia con su x sustituida por el argumento y se deriva (regla de la cadena incluida). */
        int cuerpo = gbm_obj[d.a].raiz;
        if (cuerpo < 0) return -1;
        int c = copia_rec(cuerpo, d.b, 0);
        return deriv(c, prof + 1);
    }
    }
    return dnum(n);
}

int gbm_derivada(int raiz) { return deriv(raiz, 0); }

/* ------------------------------------------------------------ escritura */

static int prec(int n)
{
    const gbm_nodo_t *d = &nodos[n];
    switch (d->tipo) {
    case N_ADD: case N_SUB: return 1;
    case N_MUL: case N_DIV: return 2;
    case N_NEG: return 3;
    case N_NUM: return d->num < 0 ? 3 : 6;
    case N_POW: return 4;
    default: return 6;
    }
}

typedef struct { char *s; int n, max; bool bonito; } sal_t;
static void put(sal_t *o, const char *t) { for (; *t; t++) if (o->n < o->max - 1) o->s[o->n++] = *t; }
static void putc_(sal_t *o, char c) { if (o->n < o->max - 1) o->s[o->n++] = c; }

static void escreve(sal_t *o, int n, int min, int prof);

static void arg(sal_t *o, int n, int min, int prof)
{
    if (prec(n) < min) { putc_(o, '('); escreve(o, n, 0, prof + 1); putc_(o, ')'); }
    else escreve(o, n, min, prof + 1);
}

static const char *nome_f1(int f)
{
    static const char *const t[] = { "sin", "cos", "tan", "asin", "acos", "atan", "sinh", "cosh", "tanh", "exp", "ln", "log10", "log2",
                                     "sqrt", "cbrt", "abs", "floor", "ceil", "round", "sign", "abscisa", "ordenada" };
    return f < F_N1 ? t[f] : "?";
}
static const char *nome_f2(int f)
{
    static const char *const t[] = { "atan2", "min", "max", "mod", "hypot", "pow", "log" };
    return f < F_N2 ? t[f] : "?";
}

static void escreve(sal_t *o, int n, int min, int prof)
{
    (void)min;
    if (n < 0 || prof > 200) { put(o, "?"); return; }
    const gbm_nodo_t *d = &nodos[n];
    char b[40];
    switch (d->tipo) {
    case N_NUM: gbm_formatar(d->num, 10, b, sizeof b); put(o, b); break;
    case N_X: putc_(o, 'x'); break;
    case N_REF: put(o, gbm_nome(d->a)); break;
    case N_PAR: putc_(o, '('); escreve(o, d->a, 0, prof + 1); put(o, ", "); escreve(o, d->b, 0, prof + 1); putc_(o, ')'); break;
    case N_NEG: putc_(o, '-'); arg(o, d->a, 4, prof); break;
    case N_ADD:
        arg(o, d->a, 1, prof);
        if (nodos[d->b].tipo == N_NEG) { put(o, " - "); arg(o, nodos[d->b].a, 2, prof); }
        else { put(o, " + "); arg(o, d->b, 1, prof); }
        break;
    case N_SUB:
        arg(o, d->a, 1, prof);
        if (nodos[d->b].tipo == N_NEG) { put(o, " + "); arg(o, nodos[d->b].a, 2, prof); }
        else { put(o, " - "); arg(o, d->b, 2, prof); }
        break;
    case N_MUL: {
        arg(o, d->a, 2, prof);
        bool junto = o->bonito && nodos[d->a].tipo == N_NUM && nodos[d->a].num >= 0 &&
                     (nodos[d->b].tipo == N_X || nodos[d->b].tipo == N_F1 || nodos[d->b].tipo == N_REF ||
                      (nodos[d->b].tipo == N_POW && nodos[nodos[d->b].a].tipo == N_X));
        put(o, junto ? "" : " * ");
        arg(o, d->b, 4, prof);
        break;
    }
    case N_DIV: arg(o, d->a, 2, prof); put(o, " / "); arg(o, d->b, 4, prof); break;
    case N_POW:
        arg(o, d->a, 6, prof);
        if (o->bonito && nodos[d->b].tipo == N_NUM && (nodos[d->b].num == 2.0 || nodos[d->b].num == 3.0)) putc_(o, nodos[d->b].num == 2.0 ? (char)0xB2 : (char)0xB3);
        else { putc_(o, '^'); arg(o, d->b, 4, prof); }
        break;
    case N_F1: put(o, nome_f1(d->fn)); putc_(o, '('); escreve(o, d->a, 0, prof + 1); putc_(o, ')'); break;
    case N_F2: put(o, nome_f2(d->fn)); putc_(o, '('); escreve(o, d->a, 0, prof + 1); put(o, ", "); escreve(o, d->b, 0, prof + 1); putc_(o, ')'); break;
    case N_LLAMA: put(o, gbm_nome(d->a)); putc_(o, '('); escreve(o, d->b, 0, prof + 1); putc_(o, ')'); break;
    case N_DNUM: put(o, "derivada("); escreve(o, d->a, 0, prof + 1); putc_(o, ')'); break;
    }
}

int gbm_escrever(int raiz, char *s, int max, bool bonito)
{
    sal_t o = { s, 0, max, bonito };
    escreve(&o, raiz, 0, 0);
    s[o.n] = 0;
    return o.n;
}

/* Número con "dec" decimales como mucho y sin ceros al final. */
int gbm_formatar(double v, int dec, char *s, int max)
{
    char t[48];
    int k = 0;
    if (gm_nan(v)) { copia_texto(s, "?", (size_t)max); return 1; }
    if (v - v != 0.0) { copia_texto(s, v < 0 ? "-inf" : "inf", (size_t)max); return (int)tam_texto(s); }
    bool neg = v < 0;
    if (neg) v = -v;
    if (v != 0.0 && (v >= 1e15 || v < 1e-9)) {
        /* notación científica: m.mmme±x */
        int ex = 0;
        while (v >= 10.0) { v /= 10.0; ex++; }
        while (v < 1.0) { v *= 10.0; ex--; }
        char m[32];
        gbm_formatar(v, dec < 9 ? dec : 9, m, sizeof m);
        if (m[0] == '1' && m[1] == '0' && m[2] == 0) { m[1] = 0; ex++; }   /* 9.99999 redondeado a 10 */
        formatar_texto(s, (size_t)max, "%s%se%d", neg ? "-" : "", m, ex);
        return (int)tam_texto(s);
    }
    if (dec > 12) dec = 12;
    double esc = 1.0;
    for (int i = 0; i < dec; i++) esc *= 10.0;
    while (dec > 0 && v * esc >= 9.0e18) { dec--; esc /= 10.0; }
    unsigned long long q = (unsigned long long)(v * esc + 0.5);
    unsigned long long ent = q, frac = 0, pot = 1;
    for (int i = 0; i < dec; i++) pot *= 10;
    if (dec) { ent = q / pot; frac = q % pot; }
    if (q == 0) neg = false;
    char d[24];
    int nd = 0;
    do { d[nd++] = (char)('0' + (int)(ent % 10)); ent /= 10; } while (ent);
    if (neg) t[k++] = '-';
    while (nd) t[k++] = d[--nd];
    if (dec && frac) {
        char f[16];
        for (int i = dec - 1; i >= 0; i--) { f[i] = (char)('0' + (int)(frac % 10)); frac /= 10; }
        int ult = dec - 1;
        while (ult >= 0 && f[ult] == '0') ult--;
        t[k++] = '.';
        for (int i = 0; i <= ult; i++) t[k++] = f[i];
    }
    t[k] = 0;
    copia_texto(s, t, (size_t)max);
    return k;
}

/* ------------------------------------------------------------ ceros */

int gbm_zeros_f(gbm_fn_t fn, void *ctx, double a, double b, double *r, int max)
{
    enum { N = 3000 };
    double h = (b - a) / N, xp = a, fp = fn(ctx, a), fpp = GM_NAN, xpp = a;
    int q = 0;
    for (int i = 1; i <= N && q < max; i++) {
        double x = a + h * i, f = fn(ctx, x);
        bool ok0 = gm_fin(fp), ok1 = gm_fin(f);
        if (ok0 && ok1 && fp == 0.0 && q < max) {
            if (q == 0 || gm_abs(r[q - 1] - xp) > 1e-9) r[q++] = xp;
        } else if (ok0 && ok1 && f != 0.0 && ((fp < 0 && f > 0) || (fp > 0 && f < 0))) {
            double lo = xp, hi = x, flo = fp;
            for (int k = 0; k < 100; k++) {
                double m = 0.5 * (lo + hi), fm = fn(ctx, m);
                if (!gm_fin(fm)) break;
                if (fm == 0.0) { lo = hi = m; break; }
                if ((fm < 0) == (flo < 0)) { lo = m; flo = fm; } else hi = m;
            }
            double rx = 0.5 * (lo + hi), fr = fn(ctx, rx);
            double escala = (gm_abs(fp) > gm_abs(f) ? gm_abs(fp) : gm_abs(f));
            if (gm_fin(fr) && gm_abs(fr) <= 1e-6 * escala + 1e-9) r[q++] = rx;
        } else if (ok0 && ok1 && gm_fin(fpp) && fp != 0.0 && f != 0.0 && fpp != 0.0 &&
                   ((fpp > 0) == (fp > 0)) && ((fp > 0) == (f > 0)) && gm_abs(fp) <= gm_abs(fpp) && gm_abs(fp) <= gm_abs(f)) {
            /* mínimo local de |f| en el sitio anterior: ¿raíz doble (f toca el eje sin cortarlo)? */
            double lo = xpp, hi = x;
            for (int k = 0; k < 120; k++) {
                double m1 = lo + (hi - lo) / 3, m2 = hi - (hi - lo) / 3;
                if (gm_abs(fn(ctx, m1)) < gm_abs(fn(ctx, m2))) hi = m2; else lo = m1;
            }
            double rx = 0.5 * (lo + hi), fr = fn(ctx, rx);
            if (gm_fin(fr) && gm_abs(fr) <= 1e-12 * (1.0 + gm_abs(fp)) && (q == 0 || gm_abs(r[q - 1] - rx) > 1e-6)) r[q++] = rx;
        }
        xpp = xp; fpp = fp;
        xp = x; fp = f;
    }
    if (q < max && gm_fin(fp) && fp == 0.0 && (q == 0 || gm_abs(r[q - 1] - xp) > 1e-9)) r[q++] = xp;
    return q;
}

static double fn_arvore(void *c, double x) { gbm_val_t v = gbm_avaliar((int)(long)c, x); return v.vec ? GM_NAN : v.a; }
int gbm_zeros(int raiz, double a, double b, double *r, int max) { return gbm_zeros_f(fn_arvore, (void *)(long)raiz, a, b, r, max); }
