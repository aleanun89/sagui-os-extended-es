// SPDX-License-Identifier: GPL-2.0-or-later
/* expr_logica.h - el evaluador de expr (D232): | & = != < <= > >= + - * / % : y length, substr, index, match.
 * Cada operando es un argumento aparte (como en Linux, así que * y los paréntesis se escriben \* \( \)). */
#ifndef EXPR_LOGICA_H
#define EXPR_LOGICA_H
#include "sagui.h"

typedef struct { char s[256]; } ev_t;

typedef struct {
    int n, i;
    char **a;
    char *err; int emax;
    bool fallo;
} ex_t;

static ev_t ex_o(ex_t *x);

static bool ex_num(const char *s, long *v)
{
    long r = 0;
    int sg = 1;
    if (*s == '-') { sg = -1; s++; }
    if (!*s) return false;
    for (; *s; s++) { if (*s < '0' || *s > '9') return false; r = r * 10 + (*s - '0'); }
    *v = r * sg;
    return true;
}

static void ex_falla(ex_t *x, const char *m)
{
    if (!x->fallo) copia_texto(x->err, m, (size_t)x->emax);
    x->fallo = true;
}

static bool ex_verdad(const ev_t *e)
{
    long v;
    if (!e->s[0]) return false;
    if (ex_num(e->s, &v)) return v != 0;
    return true;
}

static ev_t ex_de_num(long v) { ev_t e; formatar_texto(e.s, sizeof e.s, "%ld", v); return e; }
static ev_t ex_de_txt(const char *t) { ev_t e; copia_texto(e.s, t, sizeof e.s); return e; }

static const char *ex_ver(ex_t *x) { return x->i < x->n ? x->a[x->i] : NULL; }

static ev_t ex_primario(ex_t *x)
{
    const char *t = ex_ver(x);
    if (!t) { ex_falla(x, "expresión sintácticamente incorrecta"); return ex_de_txt(""); }
    if (compara_texto(t, "(") == 0) {
        x->i++;
        ev_t v = ex_o(x);
        const char *c = ex_ver(x);
        if (!c || compara_texto(c, ")") != 0) { ex_falla(x, "falta un \")\""); return v; }
        x->i++;
        return v;
    }
    if (compara_texto(t, "length") == 0 && x->i + 1 < x->n) {
        x->i++;
        ev_t v = ex_primario(x);
        return ex_de_num((long)tam_texto(v.s));
    }
    if (compara_texto(t, "substr") == 0 && x->i + 3 < x->n) {
        x->i++;
        ev_t s = ex_primario(x), p = ex_primario(x), l = ex_primario(x);
        long pv, lv;
        if (!ex_num(p.s, &pv) || !ex_num(l.s, &lv)) { ex_falla(x, "argumento no numérico"); return ex_de_txt(""); }
        long n = (long)tam_texto(s.s);
        if (pv < 1 || lv < 1 || pv > n) return ex_de_txt("");
        if (pv - 1 + lv > n) lv = n - (pv - 1);
        ev_t r;
        memcpy(r.s, s.s + pv - 1, (size_t)lv);
        r.s[lv] = 0;
        return r;
    }
    if (compara_texto(t, "index") == 0 && x->i + 2 < x->n) {
        x->i++;
        ev_t s = ex_primario(x), c = ex_primario(x);
        for (int k = 0; s.s[k]; k++)
            if (acha_char(c.s, (unsigned char)s.s[k])) return ex_de_num(k + 1);
        return ex_de_num(0);
    }
    if (compara_texto(t, "match") == 0 && x->i + 2 < x->n) {
        /* match S RE equivale a S : RE; se reescribe la lectura en el nivel de ':' */
        x->i++;
        ev_t s = ex_primario(x), re = ex_primario(x);
        static er_t er;
        if (er_compilar(&er, re.s, false, false) != 0) { ex_falla(x, "expresión regular no válida"); return ex_de_txt(""); }
        int a, b;
        if (er_buscar(&er, s.s, (int)tam_texto(s.s), 0, &a, &b) && a == 0) return ex_de_num(b);
        return ex_de_num(0);
    }
    x->i++;
    return ex_de_txt(t);
}

static ev_t ex_dospuntos(ex_t *x)
{
    ev_t a = ex_primario(x);
    for (;;) {
        const char *t = ex_ver(x);
        if (!t || compara_texto(t, ":") != 0) return a;
        x->i++;
        ev_t re = ex_primario(x);
        static er_t er;
        if (er_compilar(&er, re.s, false, false) != 0) { ex_falla(x, "expresión regular no válida"); return ex_de_txt(""); }
        int ini, fim;
        if (er_buscar(&er, a.s, (int)tam_texto(a.s), 0, &ini, &fim) && ini == 0) a = ex_de_num(fim);
        else a = ex_de_num(0);
    }
}

static ev_t ex_mul(ex_t *x)
{
    ev_t a = ex_dospuntos(x);
    for (;;) {
        const char *t = ex_ver(x);
        if (!t || (compara_texto(t, "*") && compara_texto(t, "/") && compara_texto(t, "%"))) return a;
        char op = t[0];
        x->i++;
        ev_t b = ex_dospuntos(x);
        long u, v;
        if (!ex_num(a.s, &u) || !ex_num(b.s, &v)) { ex_falla(x, "argumento no numérico"); return ex_de_txt(""); }
        if (op != '*' && v == 0) { ex_falla(x, "división entre cero"); return ex_de_txt(""); }
        a = ex_de_num(op == '*' ? u * v : op == '/' ? u / v : u % v);
    }
}

static ev_t ex_suma(ex_t *x)
{
    ev_t a = ex_mul(x);
    for (;;) {
        const char *t = ex_ver(x);
        if (!t || (compara_texto(t, "+") && compara_texto(t, "-"))) return a;
        char op = t[0];
        x->i++;
        ev_t b = ex_mul(x);
        long u, v;
        if (!ex_num(a.s, &u) || !ex_num(b.s, &v)) { ex_falla(x, "argumento no numérico"); return ex_de_txt(""); }
        a = ex_de_num(op == '+' ? u + v : u - v);
    }
}

static ev_t ex_cmp(ex_t *x)
{
    ev_t a = ex_suma(x);
    for (;;) {
        const char *t = ex_ver(x);
        if (!t) return a;
        int op = 0;
        if (!compara_texto(t, "=") || !compara_texto(t, "==")) op = 1;
        else if (!compara_texto(t, "!=")) op = 2;
        else if (!compara_texto(t, "<")) op = 3;
        else if (!compara_texto(t, "<=")) op = 4;
        else if (!compara_texto(t, ">")) op = 5;
        else if (!compara_texto(t, ">=")) op = 6;
        if (!op) return a;
        x->i++;
        ev_t b = ex_suma(x);
        long u, v;
        int c;
        if (ex_num(a.s, &u) && ex_num(b.s, &v)) c = u < v ? -1 : u > v;
        else { c = compara_texto(a.s, b.s); c = c < 0 ? -1 : c > 0; }
        bool r = op == 1 ? c == 0 : op == 2 ? c != 0 : op == 3 ? c < 0 : op == 4 ? c <= 0 : op == 5 ? c > 0 : c >= 0;
        a = ex_de_num(r);
    }
}

static ev_t ex_y(ex_t *x)
{
    ev_t a = ex_cmp(x);
    for (;;) {
        const char *t = ex_ver(x);
        if (!t || compara_texto(t, "&") != 0) return a;
        x->i++;
        ev_t b = ex_cmp(x);
        a = (ex_verdad(&a) && ex_verdad(&b)) ? a : ex_de_num(0);
    }
}

static ev_t ex_o(ex_t *x)
{
    ev_t a = ex_y(x);
    for (;;) {
        const char *t = ex_ver(x);
        if (!t || compara_texto(t, "|") != 0) return a;
        x->i++;
        ev_t b = ex_y(x);
        a = ex_verdad(&a) ? a : b;
    }
}

/* Devuelve 0 si el resultado no es vacío ni 0, 1 si lo es, 2 si hay un error (mensaje en err). */
static int expr_avaliar(int n, char **a, char *out, int max, char *err, int emax)
{
    ex_t x = { n, 0, a, err, emax, false };
    if (n < 1) { copia_texto(err, "falta una expresión", (size_t)emax); return 2; }
    ev_t v = ex_o(&x);
    if (!x.fallo && x.i < n) ex_falla(&x, "expresión sintácticamente incorrecta");
    if (x.fallo) return 2;
    copia_texto(out, v.s, (size_t)max);
    return ex_verdad(&v) ? 0 : 1;
}

#endif
