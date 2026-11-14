// SPDX-License-Identifier: GPL-2.0-or-later
/* sed_logica.h - el motor de sed (D232): compila un guion y lo aplica línea a línea.
 *
 * Es lo básico de sed: direcciones (N, $, /regex/, N,M, N,/re/, /re/,/re/, ! para negar) y las órdenes
 * s, d, p, q, =, a, i, c. En "s": delimitador libre, banderas g, i/I, p y un número N; en el reemplazo, &,
 * \n, \t y \&. No hay referencias \1 (el motor de expresiones regulares no tiene subexpresiones, D210).
 * Se separa del programa para probarlo en el PC (tests/comandos_unidad.c) sin QEMU. */
#ifndef SED_LOGICA_H
#define SED_LOGICA_H
#include "sagui.h"

#define SED_MAX_CMD  48
#define SED_MAX_RE   24
#define SED_LINEA    8192
#define SED_POZA     4096

enum { SED_DIR_NADA = 0, SED_DIR_LINEA, SED_DIR_ULTIMA, SED_DIR_RE };

typedef struct { int tipo; int n; } sed_dir_t;

typedef struct {
    sed_dir_t a1, a2;
    bool nega, en_rango;
    char cmd;
    int  re;            /* s: índice de la expresión */
    int  repl;          /* s: posición del reemplazo en la poza; a/i/c: del texto */
    bool g, p;
    int  num;           /* s: aplica desde la coincidencia N (por defecto 1) */
} sed_cmd_t;

typedef struct {
    sed_cmd_t cmds[SED_MAX_CMD];
    int ncmds;
    er_t res[SED_MAX_RE];
    int nres;
    char poza[SED_POZA];
    int npoza;
    bool ext;           /* -E */
    bool silencio;      /* -n */
    /* estado de la ejecución */
    int  nlinea;
    bool salir;
    int  codigo_salida;
} sed_t;

typedef int (*sed_saida_fn)(void *ctx, const char *s, int n);
/* Entrega la siguiente línea (sin el salto) en buf; devuelve su largo o -1 al terminar. */
typedef int (*sed_entrada_fn)(void *ctx, char *buf, int max);

static void sed_iniciar(sed_t *s, bool ext, bool silencio)
{
    memset(s, 0, sizeof *s);
    s->ext = ext;
    s->silencio = silencio;
}

static void sed_erro(char *e, int max, const char *m, int c)
{
    if (c) formatar_texto(e, (size_t)max, "%s (\"%c\")", m, c); else copia_texto(e, m, (size_t)max);
}

static int sed_poza(sed_t *s, const char *t, int n)
{
    if (s->npoza + n + 1 > SED_POZA) return -1;
    int p = s->npoza;
    memcpy(s->poza + p, t, (size_t)n);
    s->poza[p + n] = 0;
    s->npoza += n + 1;
    return p;
}

static int sed_num(const char **p)
{
    int v = 0;
    while (**p >= '0' && **p <= '9') { v = v * 10 + (**p - '0'); (*p)++; }
    return v;
}

/* Lee un texto hasta el delimitador d (sin pasarlo); \d es el propio d; deja las demás \x como están. */
static int sed_leer_hasta(const char **p, char d, char *dst, int max)
{
    int n = 0;
    const char *q = *p;
    while (*q && *q != d) {
        if (*q == '\\' && q[1]) {
            if (q[1] == d) { if (n + 1 < max) dst[n++] = d; q += 2; continue; }
            if (q[1] == 'n' && d != 'n') { if (n + 1 < max) dst[n++] = '\n'; q += 2; continue; }
            if (n + 2 < max) { dst[n++] = q[0]; dst[n++] = q[1]; }
            q += 2;
            continue;
        }
        if (n + 1 < max) dst[n++] = *q;
        q++;
    }
    if (*q != d) return -1;
    dst[n] = 0;
    *p = q + 1;
    return n;
}

static int sed_dir(sed_t *s, const char **p, sed_dir_t *d, char *e, int emax)
{
    d->tipo = SED_DIR_NADA;
    if (**p >= '0' && **p <= '9') { d->tipo = SED_DIR_LINEA; d->n = sed_num(p); return 0; }
    if (**p == '$') { d->tipo = SED_DIR_ULTIMA; (*p)++; return 0; }
    if (**p == '/' || **p == '\\') {
        char delim = '/';
        if (**p == '\\') { (*p)++; delim = **p; if (!delim) { sed_erro(e, emax, "dirección incompleta", 0); return -1; } }
        (*p)++;
        char t[512];
        if (sed_leer_hasta(p, delim, t, sizeof t) < 0) { sed_erro(e, emax, "falta el cierre de la expresión de la dirección", 0); return -1; }
        bool ign = false;
        if (**p == 'I') { ign = true; (*p)++; }
        if (s->nres >= SED_MAX_RE) { sed_erro(e, emax, "demasiadas expresiones regulares", 0); return -1; }
        if (er_compilar(&s->res[s->nres], t, s->ext, ign) != 0) {
            formatar_texto(e, (size_t)emax, "expresión no válida: %s", s->res[s->nres].erro);
            return -1;
        }
        d->tipo = SED_DIR_RE;
        d->n = s->nres++;
        return 0;
    }
    return 0;
}

static void sed_saltar_esp(const char **p) { while (**p == ' ' || **p == '\t') (*p)++; }

/* Añade las órdenes de un guion. Devuelve 0 o -1 con el mensaje en e. */
static int sed_compilar(sed_t *s, const char *g, char *e, int emax)
{
    const char *p = g;
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == ';') p++;
        if (!*p) return 0;
        if (s->ncmds >= SED_MAX_CMD) { sed_erro(e, emax, "demasiadas órdenes", 0); return -1; }
        sed_cmd_t *c = &s->cmds[s->ncmds];
        memset(c, 0, sizeof *c);
        if (sed_dir(s, &p, &c->a1, e, emax) < 0) return -1;
        if (c->a1.tipo != SED_DIR_NADA) {
            sed_saltar_esp(&p);
            if (*p == ',') {
                p++;
                sed_saltar_esp(&p);
                if (sed_dir(s, &p, &c->a2, e, emax) < 0) return -1;
                if (c->a2.tipo == SED_DIR_NADA) { sed_erro(e, emax, "falta la segunda dirección", 0); return -1; }
            }
        }
        sed_saltar_esp(&p);
        while (*p == '!') { c->nega = !c->nega; p++; sed_saltar_esp(&p); }
        c->cmd = *p;
        if (!c->cmd) { sed_erro(e, emax, "falta la orden", 0); return -1; }
        p++;
        switch (c->cmd) {
        case 'd': case 'p': case '=': case 'q': break;
        case 'a': case 'i': case 'c': {
            if (*p == '\\') { p++; if (*p == '\n') p++; }
            while (*p == ' ' || *p == '\t') p++;
            const char *f = p;
            while (*f && *f != '\n') f++;
            c->repl = sed_poza(s, p, (int)(f - p));
            if (c->repl < 0) { sed_erro(e, emax, "guion demasiado largo", 0); return -1; }
            p = f;
            break;
        }
        case 's': {
            char delim = *p;
            if (!delim || delim == '\n' || delim == '\\') { sed_erro(e, emax, "s: falta el delimitador", 0); return -1; }
            p++;
            char pat[512], rep[512];
            if (sed_leer_hasta(&p, delim, pat, sizeof pat) < 0 || sed_leer_hasta(&p, delim, rep, sizeof rep) < 0) {
                sed_erro(e, emax, "s: falta un delimitador (s/viejo/nuevo/)", 0);
                return -1;
            }
            c->num = 1;
            bool ign = false;
            for (;; p++) {
                if (*p == 'g') c->g = true;
                else if (*p == 'p') c->p = true;
                else if (*p == 'i' || *p == 'I') ign = true;
                else if (*p >= '0' && *p <= '9') { c->num = sed_num(&p); p--; if (c->num < 1) { sed_erro(e, emax, "s: el número de coincidencia debe ser >= 1", 0); return -1; } }
                else break;
            }
            for (const char *q = rep; *q; q++)
                if (*q == '\\' && q[1] >= '1' && q[1] <= '9') {
                    sed_erro(e, emax, "s: las referencias \\1 no están disponibles (el motor no tiene subexpresiones)", 0);
                    return -1;
                }
            if (s->nres >= SED_MAX_RE) { sed_erro(e, emax, "demasiadas expresiones regulares", 0); return -1; }
            if (er_compilar(&s->res[s->nres], pat, s->ext, ign) != 0) {
                formatar_texto(e, (size_t)emax, "expresión no válida: %s", s->res[s->nres].erro);
                return -1;
            }
            c->re = s->nres++;
            c->repl = sed_poza(s, rep, (int)tam_texto(rep));
            if (c->repl < 0) { sed_erro(e, emax, "guion demasiado largo", 0); return -1; }
            break;
        }
        default:
            sed_erro(e, emax, "orden desconocida", (unsigned char)c->cmd);
            return -1;
        }
        sed_saltar_esp(&p);
        if (*p && *p != ';' && *p != '\n') { sed_erro(e, emax, "sobran caracteres tras la orden", (unsigned char)*p); return -1; }
        s->ncmds++;
    }
}

static bool sed_casa_dir(sed_t *s, const sed_dir_t *d, const char *ps, int n, bool ultima)
{
    switch (d->tipo) {
    case SED_DIR_LINEA: return s->nlinea == d->n;
    case SED_DIR_ULTIMA: return ultima;
    case SED_DIR_RE: { int a, b; return er_buscar(&s->res[d->n], ps, n, 0, &a, &b); }
    }
    return true;
}

/* ¿Se aplica la orden a esta línea? Lleva el estado de los rangos. Fin del rango: *fin_rango. */
static bool sed_aplica(sed_t *s, sed_cmd_t *c, const char *ps, int n, bool ultima, bool *fin_rango)
{
    bool r;
    *fin_rango = true;
    if (c->a1.tipo == SED_DIR_NADA) r = true;
    else if (c->a2.tipo == SED_DIR_NADA) r = sed_casa_dir(s, &c->a1, ps, n, ultima);
    else if (c->en_rango) {
        r = true;
        bool fin = false;
        if (c->a2.tipo == SED_DIR_LINEA) fin = s->nlinea >= c->a2.n;
        else fin = sed_casa_dir(s, &c->a2, ps, n, ultima);
        if (fin) c->en_rango = false;
        *fin_rango = fin;
    } else if (sed_casa_dir(s, &c->a1, ps, n, ultima)) {
        r = true;
        if (c->a2.tipo == SED_DIR_LINEA && c->a2.n <= s->nlinea) { /* una sola línea */ }
        else if (c->a2.tipo == SED_DIR_ULTIMA && ultima) { }
        else { c->en_rango = true; *fin_rango = false; }
    } else r = false;
    return c->nega ? !r : r;
}

/* Sustituye en ps (largo *n, capacidad max). Devuelve true si cambió algo. */
static bool sed_sustituir(sed_t *s, sed_cmd_t *c, char *ps, int *n, int max)
{
    static char sal[SED_LINEA];
    const char *rep = s->poza + c->repl;
    int ns = 0, desde = 0, cuenta = 0;
    bool hecho = false;
    while (desde <= *n) {
        int a, b;
        if (!er_buscar(&s->res[c->re], ps, *n, desde, &a, &b)) break;
        cuenta++;
        /* copia lo que hay entre medias */
        for (int i = desde; i < a && ns < SED_LINEA - 1; i++) sal[ns++] = ps[i];
        if (cuenta >= c->num && (c->g || cuenta == c->num)) {
            for (const char *q = rep; *q && ns < SED_LINEA - 1; q++) {
                if (*q == '&') { for (int i = a; i < b && ns < SED_LINEA - 1; i++) sal[ns++] = ps[i]; }
                else if (*q == '\\' && q[1]) {
                    q++;
                    sal[ns++] = *q == 'n' ? '\n' : *q == 't' ? '\t' : *q;
                } else sal[ns++] = *q;
            }
            hecho = true;
            if (!c->g) { for (int i = b; i < *n && ns < SED_LINEA - 1; i++) sal[ns++] = ps[i]; desde = *n + 1; break; }
        } else {
            for (int i = a; i < b && ns < SED_LINEA - 1; i++) sal[ns++] = ps[i];
        }
        if (b == a) {   /* coincidencia vacía: se copia un carácter y se sigue */
            if (a < *n && ns < SED_LINEA - 1) sal[ns++] = ps[a];
            desde = a + 1;
        } else desde = b;
    }
    if (!hecho) return false;
    if (desde <= *n) for (int i = desde; i < *n && ns < SED_LINEA - 1; i++) sal[ns++] = ps[i];
    if (ns > max - 1) ns = max - 1;
    memcpy(ps, sal, (size_t)ns);
    ps[ns] = 0;
    *n = ns;
    return true;
}

static int sed_emitir_linea(sed_saida_fn out, void *ctx, const char *t, int n)
{
    if (out(ctx, t, n) < 0) return -1;
    return out(ctx, "\n", 1);
}

/* Procesa todo el flujo. Devuelve 0, o -1 si la salida se cerró. */
static int sed_executar(sed_t *s, sed_entrada_fn in, void *ictx, sed_saida_fn out, void *octx)
{
    static char ps[SED_LINEA], sig[SED_LINEA];
    int n = in(ictx, ps, sizeof ps), nsig;
    s->nlinea = 0;
    if (n < 0) return 0;
    nsig = in(ictx, sig, sizeof sig);
    while (n >= 0) {
        bool ultima = nsig < 0, suprime = false;
        int nanex = 0, quedan_anexos[8];
        s->nlinea++;
        for (int i = 0; i < s->ncmds && !suprime; i++) {
            sed_cmd_t *c = &s->cmds[i];
            bool fin;
            if (!sed_aplica(s, c, ps, n, ultima, &fin)) continue;
            switch (c->cmd) {
            case 'd': suprime = true; break;
            case 'p': if (sed_emitir_linea(out, octx, ps, n) < 0) return -1; break;
            case '=': {
                char t[16];
                int k = formatar_texto(t, sizeof t, "%d", s->nlinea);
                if (sed_emitir_linea(out, octx, t, k) < 0) return -1;
                break;
            }
            case 's':
                if (sed_sustituir(s, c, ps, &n, SED_LINEA) && c->p && sed_emitir_linea(out, octx, ps, n) < 0) return -1;
                break;
            case 'i': if (sed_emitir_linea(out, octx, s->poza + c->repl, (int)tam_texto(s->poza + c->repl)) < 0) return -1; break;
            case 'a': if (nanex < 8) quedan_anexos[nanex++] = c->repl; break;
            case 'c':
                if (fin && sed_emitir_linea(out, octx, s->poza + c->repl, (int)tam_texto(s->poza + c->repl)) < 0) return -1;
                suprime = true;
                break;
            case 'q':
                if (!s->silencio && sed_emitir_linea(out, octx, ps, n) < 0) return -1;
                suprime = true;
                s->salir = true;
                break;
            }
        }
        if (!suprime && !s->silencio && sed_emitir_linea(out, octx, ps, n) < 0) return -1;
        for (int k = 0; k < nanex; k++)
            if (sed_emitir_linea(out, octx, s->poza + quedan_anexos[k], (int)tam_texto(s->poza + quedan_anexos[k])) < 0) return -1;
        if (s->salir) return 0;
        if (ultima) break;
        memcpy(ps, sig, (size_t)nsig + 1);
        n = nsig;
        nsig = in(ictx, sig, sizeof sig);
    }
    return 0;
}

#endif
