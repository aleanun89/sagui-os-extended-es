// SPDX-License-Identifier: GPL-2.0-or-later
/* bc_logica.h - la calculadora bc sencilla (D232): enteros y decimales de hasta 6 cifras (punto fijo de 64 bits).
 *
 * Expresiones con + - * / % ^, paréntesis, variables de una letra (a-z), scale=N (0 a 6), sqrt(x) y quit.
 * Como en bc, la división y la raíz dan tantos decimales como "scale" (por defecto 0), la suma conserva los
 * de los operandos y el producto los de ambos (hasta max(scale, operandos)). Los números caben hasta ~9e12. */
#ifndef BC_LOGICA_H
#define BC_LOGICA_H
#include "sagui.h"

#define BC_S 1000000LL   /* 6 decimales internos */

typedef struct { int64_t v; int sc; } bn_t;

typedef struct {
    bn_t var[26];
    int scale;
    const char *p;
    char err[64];
    bool fallo;
} bc_t;

static void bc_iniciar(bc_t *b) { memset(b, 0, sizeof *b); }

static void bc_falla(bc_t *b, const char *m) { if (!b->fallo) copia_texto(b->err, m, sizeof b->err); b->fallo = true; }

static int64_t bc_pot10(int n) { int64_t r = 1; while (n-- > 0) r *= 10; return r; }

static bn_t bc_trunc(bn_t a, int sc)
{
    if (sc < a.sc) {
        int64_t m = bc_pot10(6 - sc);
        a.v -= a.v % m;
        a.sc = sc;
    }
    return a;
}

static int bc_max(int a, int b) { return a > b ? a : b; }
static int bc_min(int a, int b) { return a < b ? a : b; }

/* a * b / S sin desbordar: se separan parte entera y fraccionaria de cada uno */
static int64_t bc_mulfix(int64_t a, int64_t b)
{
    int sg = 1;
    if (a < 0) { a = -a; sg = -sg; }
    if (b < 0) { b = -b; sg = -sg; }
    int64_t ai = a / BC_S, af = a % BC_S, bi = b / BC_S, bf = b % BC_S;
    int64_t r = ai * bi * BC_S + ai * bf + af * bi + af * bf / BC_S;
    return sg * r;
}

/* a / b en punto fijo: parte entera y 6 cifras por división larga */
static int64_t bc_divfix(int64_t a, int64_t b)
{
    int sg = 1;
    if (a < 0) { a = -a; sg = -sg; }
    if (b < 0) { b = -b; sg = -sg; }
    int64_t q = a / b, r = a % b, res = q * BC_S;
    int64_t unidad = BC_S / 10;
    for (int i = 0; i < 6; i++) {
        r *= 10;
        res += (r / b) * unidad;
        r %= b;
        unidad /= 10;
    }
    return sg * res;
}

static void bc_espacios(bc_t *b) { while (*b->p == ' ' || *b->p == '\t' || *b->p == '\r') b->p++; }

static bn_t bc_exp(bc_t *b);

static bn_t bc_primario(bc_t *b)
{
    bn_t r = { 0, 0 };
    bc_espacios(b);
    char c = *b->p;
    if (c == '-') { b->p++; r = bc_primario(b); r.v = -r.v; return r; }
    if (c == '+') { b->p++; return bc_primario(b); }
    if (c == '(') {
        b->p++;
        r = bc_exp(b);
        bc_espacios(b);
        if (*b->p != ')') { bc_falla(b, "falta un \")\""); return r; }
        b->p++;
        return r;
    }
    if ((c >= '0' && c <= '9') || c == '.') {
        int64_t ent = 0, frac = 0;
        int nd = 0;
        while (*b->p >= '0' && *b->p <= '9') { ent = ent * 10 + (*b->p - '0'); b->p++; if (ent > 9000000000000LL) { bc_falla(b, "número demasiado grande"); return r; } }
        if (*b->p == '.') {
            b->p++;
            int64_t m = BC_S / 10;
            while (*b->p >= '0' && *b->p <= '9') {
                if (nd < 6) { frac += (*b->p - '0') * m; m /= 10; nd++; }
                b->p++;
            }
        }
        r.v = ent * BC_S + frac;
        r.sc = nd;
        return r;
    }
    if (c >= 'a' && c <= 'z') {
        if (comeca_com(b->p, "sqrt") && !(b->p[4] >= 'a' && b->p[4] <= 'z')) {
            b->p += 4;
            bc_espacios(b);
            if (*b->p != '(') { bc_falla(b, "expresión no válida"); return r; }
            b->p++;
            bn_t a = bc_exp(b);
            bc_espacios(b);
            if (*b->p != ')') { bc_falla(b, "falta un \")\""); return r; }
            b->p++;
            if (a.v < 0) { bc_falla(b, "raíz de un número negativo"); return r; }
            if (a.v > 18000000000000LL) { bc_falla(b, "sqrt: número demasiado grande"); return r; }
            uint64_t x = (uint64_t)a.v * (uint64_t)BC_S, lo = 0, hi = 4294967295ULL;
            while (lo < hi) {   /* raíz entera por bisección: mayor m con m*m <= x */
                uint64_t m = lo + (hi - lo + 1) / 2;
                if (m * m <= x) lo = m; else hi = m - 1;
            }
            r.v = (int64_t)lo;
            r.sc = 6;
            return bc_trunc(r, bc_max(b->scale, a.sc));
        }
        if (comeca_com(b->p, "scale") && !(b->p[5] >= 'a' && b->p[5] <= 'z')) { b->p += 5; r.v = b->scale * BC_S; return r; }
        b->p++;
        if (*b->p >= 'a' && *b->p <= 'z') { bc_falla(b, "variable desconocida (solo una letra)"); return r; }
        return b->var[c - 'a'];
    }
    bc_falla(b, "expresión no válida");
    return r;
}

static bn_t bc_pot(bc_t *b)
{
    bn_t base = bc_primario(b);
    bc_espacios(b);
    if (*b->p != '^') return base;
    b->p++;
    bn_t e = bc_pot(b);   /* asociativa por la derecha */
    if (b->fallo) return base;
    int64_t n = e.v / BC_S;
    bool neg = n < 0;
    if (neg) n = -n;
    if (n > 100000) { bc_falla(b, "exponente demasiado grande"); return base; }
    bn_t r = { BC_S, 0 };
    bn_t m = base;
    int64_t k = n;
    while (k) { if (k & 1) r.v = bc_mulfix(r.v, m.v); m.v = bc_mulfix(m.v, m.v); k >>= 1; }
    r.sc = bc_min((int)(base.sc * n > 6 ? 6 : base.sc * n), bc_max(b->scale, base.sc));
    r = bc_trunc((bn_t){ r.v, 6 }, r.sc);
    if (neg) {
        if (r.v == 0) { bc_falla(b, "división entre cero"); return r; }
        r.v = bc_divfix(BC_S, r.v);
        r.sc = b->scale;
        r = bc_trunc((bn_t){ r.v, 6 }, r.sc);
    }
    return r;
}

static bn_t bc_mul(bc_t *b)
{
    bn_t a = bc_pot(b);
    for (;;) {
        bc_espacios(b);
        char op = *b->p;
        if (op != '*' && op != '/' && op != '%') return a;
        b->p++;
        bn_t c = bc_pot(b);
        if (b->fallo) return a;
        if (op == '*') {
            a.v = bc_mulfix(a.v, c.v);
            a.sc = bc_min(a.sc + c.sc, bc_max(b->scale, bc_max(a.sc, c.sc)));
            a = bc_trunc((bn_t){ a.v, 6 }, a.sc);
        } else {
            if (c.v == 0) { bc_falla(b, "división entre cero"); return a; }
            if (op == '/') {
                a.v = bc_divfix(a.v, c.v);
                a = bc_trunc((bn_t){ a.v, 6 }, b->scale);
            } else {
                bn_t q = { bc_divfix(a.v, c.v), 6 };
                q = bc_trunc(q, b->scale);
                a.v -= bc_mulfix(q.v, c.v);
                a.sc = bc_max(a.sc, c.sc + b->scale > 6 ? 6 : c.sc + b->scale);
            }
        }
    }
}

static bn_t bc_suma(bc_t *b)
{
    bn_t a = bc_mul(b);
    for (;;) {
        bc_espacios(b);
        char op = *b->p;
        if (op != '+' && op != '-') return a;
        b->p++;
        bn_t c = bc_mul(b);
        a.v = op == '+' ? a.v + c.v : a.v - c.v;
        a.sc = bc_max(a.sc, c.sc);
    }
}

static bn_t bc_exp(bc_t *b) { return bc_suma(b); }

static void bc_formatar(bn_t a, char *s, int max)
{
    int n = 0;
    int64_t v = a.v;
    if (v == 0) { copia_texto(s, "0", (size_t)max); return; }   /* bc escribe 0 a secas */
    if (v < 0) { v = -v; s[n++] = '-'; }
    int64_t ent = v / BC_S, frac = v % BC_S;
    if (ent >= 1000000000) n += formatar_texto(s + n, (size_t)(max - n), "%d%09d", (int)(ent / 1000000000), (int)(ent % 1000000000));
    else if (ent != 0 || a.sc == 0) n += formatar_texto(s + n, (size_t)(max - n), "%d", (int)ent);
    if (a.sc > 0) {
        char f[8];
        formatar_texto(f, sizeof f, "%06d", (int)frac);
        f[a.sc] = 0;
        formatar_texto(s + n, (size_t)(max - n), ".%s", f);
    }
}

/* Una línea (varias instrucciones separadas por ;). Devuelve 1 si hay resultado que escribir (en out, separados
 * por saltos de línea), 0 si no, -1 si hay un error (mensaje en out) y 2 si se pidió salir. */
static int bc_linea(bc_t *b, const char *linea, char *out, int max)
{
    int nsal = 0, devuelto = 0;
    out[0] = 0;
    const char *p = linea;
    while (*p) {
        const char *ini = p;
        while (*p && *p != ';') p++;
        char inst[256];
        int n = (int)(p - ini);
        if (n > 255) n = 255;
        memcpy(inst, ini, (size_t)n);
        inst[n] = 0;
        if (*p == ';') p++;
        const char *q = inst;
        while (*q == ' ' || *q == '\t') q++;
        if (!*q) continue;
        if (compara_texto(q, "quit") == 0 || compara_texto(q, "halt") == 0) return 2;
        b->fallo = false;
        b->err[0] = 0;
        /* asignación: letra o "scale", un "=" que no es "==" */
        const char *e = q;
        while (*e >= 'a' && *e <= 'z') e++;
        const char *f = e;
        while (*f == ' ') f++;
        if (e > q && *f == '=' && f[1] != '=') {
            bool es_scale = (e - q == 5) && comeca_com(q, "scale");
            if (!es_scale && e - q != 1) { copia_texto(out, "variable no válida (solo una letra o scale)", (size_t)max); return -1; }
            b->p = f + 1;
            bn_t v = bc_exp(b);
            bc_espacios(b);
            if (!b->fallo && *b->p) bc_falla(b, "expresión no válida");
            if (b->fallo) { copia_texto(out, b->err, (size_t)max); return -1; }
            if (es_scale) {
                int s = (int)(v.v / BC_S);
                if (s < 0 || s > 6) { copia_texto(out, "scale: solo de 0 a 6", (size_t)max); return -1; }
                b->scale = s;
            } else b->var[*q - 'a'] = v;
            continue;
        }
        b->p = q;
        bn_t v = bc_exp(b);
        bc_espacios(b);
        if (!b->fallo && *b->p) bc_falla(b, "expresión no válida");
        if (b->fallo) { copia_texto(out, b->err, (size_t)max); return -1; }
        char t[64];
        bc_formatar(v, t, sizeof t);
        int l = (int)tam_texto(t);
        if (nsal + l + 2 < max) {
            if (nsal) out[nsal++] = '\n';
            memcpy(out + nsal, t, (size_t)l);
            nsal += l;
            out[nsal] = 0;
        }
        devuelto = 1;
    }
    return devuelto;
}

#endif
