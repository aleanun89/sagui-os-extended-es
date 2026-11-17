// SPDX-License-Identifier: GPL-3.0-or-later
/* jb_modelo.c - el árbol de bloques, el proyecto y su fichero (texto legible dentro de un ZIP) (D377). */
#include "jb.h"
#include "compressao.h"

#include "../programas/jabuti_ops.h"

proy_t jb_p;

/* ============================================================ números en punto fijo */

bool jb_num_parse(const char *s, int32_t *v)
{
    int i = 0, neg = 0;
    while (s[i] == ' ') i++;
    if (s[i] == '-') { neg = 1; i++; } else if (s[i] == '+') i++;
    if (!(s[i] >= '0' && s[i] <= '9') && s[i] != '.') return false;
    int64_t e = 0;
    bool dig = false;
    while (s[i] >= '0' && s[i] <= '9') { e = e * 10 + (s[i] - '0'); dig = true; if (e > 2000000) e = 2000000; i++; }
    int64_t f = 0, mul = 100;
    if (s[i] == '.' || s[i] == ',') {
        i++;
        while (s[i] >= '0' && s[i] <= '9') { dig = true; if (mul > 0) { f += (s[i] - '0') * mul; mul /= 10; } i++; }
    }
    while (s[i] == ' ') i++;
    if (!dig || s[i]) return false;
    int64_t r = e * U + f;
    *v = (int32_t)(neg ? -r : r);
    return true;
}

void jb_num_texto(int32_t v, char *s, int max)
{
    char b[24];
    int n = 0;
    bool neg = v < 0;
    uint32_t a = neg ? (uint32_t)(-(int64_t)v) : (uint32_t)v;
    uint32_t ent = a / U, fr = a % U;
    if (neg && (ent || fr)) b[n++] = '-';
    char d[12];
    int k = 0;
    do { d[k++] = (char)('0' + ent % 10); ent /= 10; } while (ent);
    while (k) b[n++] = d[--k];
    if (fr) {
        b[n++] = '.';
        char f[4] = { (char)('0' + fr / 100), (char)('0' + fr / 10 % 10), (char)('0' + fr % 10), 0 };
        int l = 3;
        while (l > 0 && f[l - 1] == '0') l--;
        for (int i = 0; i < l; i++) b[n++] = f[i];
    }
    b[n] = 0;
    copia_texto(s, b, (size_t)max);
}

/* ============================================================ bloques */

static int cuenta_ranuras(const char *r)
{
    int n = 0;
    for (; *r; r++) if (*r == '#' || *r == '$' || *r == '^' || *r == '@') n++;
    return n;
}
int jb_nranuras(int op) { return cuenta_ranuras(jb_ops[op].rotulo); }
char jb_tipo_ranura(int op, int i)
{
    for (const char *r = jb_ops[op].rotulo; *r; r++)
        if (*r == '#' || *r == '$' || *r == '^' || *r == '@') { if (i-- == 0) return *r == '^' ? '?' : *r; }
    return 0;
}

int jb_op_buscar(const char *clave)
{
    for (int i = 0; i < O_FIN; i++) if (jb_ops[i].clave && compara_texto(jb_ops[i].clave, clave) == 0) return i;
    return -1;
}

blk_t *jb_blk_nuevo(int op)
{
    blk_t *b = aloca_zerado(sizeof *b);
    if (!b) return NULL;
    b->op = (uint8_t)op;
    int n = jb_nranuras(op);
    for (int i = 0; i < n && i < 3; i++) if (jb_ops[op].def[i]) copia_texto(b->a[i].t, jb_ops[op].def[i], sizeof b->a[i].t);
    return b;
}

blk_t *jb_blk_copiar(const blk_t *b, bool con_sig)
{
    blk_t *r = NULL, **fin = &r;
    for (; b; b = con_sig ? b->sig : NULL) {
        blk_t *c = aloca(sizeof *c);
        if (!c) break;
        *c = *b;
        c->sig = NULL;
        for (int i = 0; i < 3; i++) if (b->a[i].rep) c->a[i].rep = jb_blk_copiar(b->a[i].rep, false);
        c->c1 = b->c1 ? jb_blk_copiar(b->c1, true) : NULL;
        c->c2 = b->c2 ? jb_blk_copiar(b->c2, true) : NULL;
        *fin = c; fin = &c->sig;
    }
    return r;
}

void jb_blk_liberar(blk_t *b)
{
    while (b) {
        blk_t *s = b->sig;
        for (int i = 0; i < 3; i++) if (b->a[i].rep) jb_blk_liberar(b->a[i].rep);
        jb_blk_liberar(b->c1);
        jb_blk_liberar(b->c2);
        libera(b);
        b = s;
    }
}

blk_t *jb_ultimo(blk_t *b) { while (b && b->sig) b = b->sig; return b; }

/* ============================================================ proyecto */

traje_t *jb_traje_nuevo(const char *nombre)
{
    traje_t *t = aloca_zerado(sizeof *t);
    if (t) copia_texto(t->nombre, nombre, sizeof t->nombre);
    return t;
}

void jb_pers_recalcular(pers_t *p)
{
    p->sx0 = TRAJE_L; p->sy0 = TRAJE_L; p->sx1 = -1; p->sy1 = -1;
    if (p->traje < 0 || p->traje >= p->ntrajes) { p->sx0 = 0; p->sy0 = 0; p->sx1 = TRAJE_L - 1; p->sy1 = TRAJE_L - 1; return; }
    const traje_t *t = p->trajes[p->traje];
    for (int y = 0; y < TRAJE_L; y++)
        for (int x = 0; x < TRAJE_L; x++)
            if (t->px[y * TRAJE_L + x]) {
                if (x < p->sx0) p->sx0 = x;
                if (x > p->sx1) p->sx1 = x;
                if (y < p->sy0) p->sy0 = y;
                if (y > p->sy1) p->sy1 = y;
            }
    if (p->sx1 < 0) { p->sx0 = 12; p->sy0 = 12; p->sx1 = 19; p->sy1 = 19; }
}

pers_t *jb_pers_nuevo(const char *nombre)
{
    if (jb_p.npers >= MAX_PERS) return NULL;
    pers_t *p = aloca_zerado(sizeof *p);
    if (!p) return NULL;
    copia_texto(p->nombre, nombre, sizeof p->nombre);
    p->dir = 90 * U; p->tam = 100; p->visible = true; p->lapiz_color = 8; p->lapiz_grosor = 2;
    jb_p.pers[jb_p.npers++] = p;
    return p;
}

pers_t *jb_pers_biblioteca(int i)
{
    pers_t *p = jb_pers_nuevo(_(jb_biblioteca_titulo(i)));
    if (!p) return NULL;
    copia_texto(p->biblio, jb_biblioteca_nombre(i), sizeof p->biblio);
    for (int k = 0; k < jb_biblioteca_trajes(i) && k < MAX_TRAJES; k++) {
        traje_t *t = aloca(sizeof *t);
        if (!t) break;
        jb_biblioteca_traje(i, k, t);
        p->trajes[p->ntrajes++] = t;
    }
    jb_pers_recalcular(p);
    return p;
}

void jb_guion_borrar(pers_t *p, int i)
{
    if (i < 0 || i >= p->nguiones) return;
    jb_blk_liberar(p->guiones[i].top);
    for (int k = i; k + 1 < p->nguiones; k++) p->guiones[k] = p->guiones[k + 1];
    p->nguiones--;
}

void jb_pers_liberar(pers_t *p)
{
    if (!p) return;
    for (int i = 0; i < p->ntrajes; i++) libera(p->trajes[i]);
    for (int i = 0; i < p->nguiones; i++) jb_blk_liberar(p->guiones[i].top);
    libera(p);
}

void jb_pers_borrar(int i)
{
    if (i < 0 || i >= jb_p.npers) return;
    jb_pers_liberar(jb_p.pers[i]);
    for (int k = i; k + 1 < jb_p.npers; k++) jb_p.pers[k] = jb_p.pers[k + 1];
    jb_p.npers--;
}

void jb_proy_vaciar(void)
{
    for (int i = 0; i < jb_p.npers; i++) jb_pers_liberar(jb_p.pers[i]);
    for (int i = 0; i < jb_p.nfondos; i++) libera(jb_p.fondos[i]);
    memset(&jb_p, 0, sizeof jb_p);
}

static void fondo_por_defecto(void)
{
    fondo_t *f = aloca(sizeof *f);
    if (!f) return;
    jb_fondo_biblioteca(0, f);
    jb_p.fondos[0] = f; jb_p.nfondos = 1; jb_p.fondo = 0;
}

void jb_proy_nuevo(void)
{
    jb_proy_vaciar();
    fondo_por_defecto();
    pers_t *p = jb_pers_biblioteca(0);
    (void)p;
}

int jb_var_buscar(const char *n)
{
    for (int i = 0; i < jb_p.nvars; i++) if (compara_texto(jb_p.vars[i].nombre, n) == 0) return i;
    return -1;
}
int jb_var_crear(const char *n)
{
    int i = jb_var_buscar(n);
    if (i >= 0) return i;
    if (jb_p.nvars >= MAX_VARS) return -1;
    var_t *v = &jb_p.vars[jb_p.nvars];
    memset(v, 0, sizeof *v);
    copia_texto(v->nombre, n, sizeof v->nombre);
    v->mostrar = true;
    return jb_p.nvars++;
}
int jb_lista_buscar(const char *n)
{
    for (int i = 0; i < jb_p.nlistas; i++) if (compara_texto(jb_p.listas[i].nombre, n) == 0) return i;
    return -1;
}
int jb_lista_crear(const char *n)
{
    int i = jb_lista_buscar(n);
    if (i >= 0) return i;
    if (jb_p.nlistas >= MAX_LISTAS) return -1;
    memset(&jb_p.listas[jb_p.nlistas], 0, sizeof(bqlista_t));
    copia_texto(jb_p.listas[jb_p.nlistas].nombre, n, 24);
    return jb_p.nlistas++;
}

/* ============================================================ escritura del texto */

typedef struct { char *d; unsigned n, cap; } sal_t;
static void sal_c(sal_t *s, const char *t, unsigned n)
{
    if (s->n + n + 1 > s->cap) {
        unsigned nc = s->cap ? s->cap * 2 : 4096;
        while (nc < s->n + n + 1) nc *= 2;
        char *x = realoca(s->d, nc);
        if (!x) return;
        s->d = x; s->cap = nc;
    }
    memcpy(s->d + s->n, t, n);
    s->n += n;
    s->d[s->n] = 0;
}
static void sal_t_(sal_t *s, const char *t) { sal_c(s, t, (unsigned)tam_texto(t)); }
static void sal_cadena(sal_t *s, const char *t)
{
    sal_c(s, "\"", 1);
    for (; *t; t++) {
        if (*t == '"' || *t == '\\') sal_c(s, "\\", 1);
        if (*t == '\n') { sal_c(s, "\\n", 2); continue; }
        sal_c(s, t, 1);
    }
    sal_c(s, "\"", 1);
}
static void sal_sangria(sal_t *s, int n) { for (int i = 0; i < n; i++) sal_c(s, " ", 1); }

static void escribir_expr(sal_t *s, const blk_t *b);
static void escribir_args(sal_t *s, const blk_t *b)
{
    int n = jb_nranuras(b->op);
    for (int i = 0; i < n; i++) {
        sal_c(s, " ", 1);
        if (b->a[i].rep) { escribir_expr(s, b->a[i].rep); continue; }
        int32_t v;
        if (jb_tipo_ranura(b->op, i) == '#' && b->a[i].t[0] && jb_num_parse(b->a[i].t, &v) && !acha_char(b->a[i].t, ' ')) sal_t_(s, b->a[i].t);
        else sal_cadena(s, b->a[i].t);
    }
}
static void escribir_expr(sal_t *s, const blk_t *b)
{
    sal_c(s, "(", 1);
    sal_t_(s, jb_ops[b->op].clave);
    escribir_args(s, b);
    sal_c(s, ")", 1);
}
static void escribir_pila(sal_t *s, const blk_t *b, int sg)
{
    for (; b; b = b->sig) {
        sal_sangria(s, sg);
        sal_t_(s, jb_ops[b->op].clave);
        escribir_args(s, b);
        sal_c(s, "\n", 1);
        if (jb_ops[b->op].tipo == K_C || jb_ops[b->op].tipo == K_CAP) {
            if (b->c1 || b->op == O_SI_NO) escribir_pila(s, b->c1, sg + 2);
            if (b->op == O_SI_NO) {
                sal_sangria(s, sg); sal_t_(s, "sino\n");
                escribir_pila(s, b->c2, sg + 2);
            }
        }
    }
}

static void escribir_rejilla(sal_t *s, const uint8_t *px, int l, int a, int sg)
{
    for (int y = 0; y < a; y++) {
        sal_sangria(s, sg);
        for (int x = 0; x < l; x++) sal_c(s, &jb_alfabeto[px[y * l + x] & 31], 1);
        sal_c(s, "\n", 1);
    }
}

static void sal_num(sal_t *s, int32_t v) { char b[24]; jb_num_texto(v, b, sizeof b); sal_t_(s, b); }

char *jb_escribir_texto(unsigned *n)
{
    sal_t s = { 0 };
    sal_t_(&s, "JABUTI 1\n");
    sal_t_(&s, "# Proyecto de «Jabuti» (SaguiOS). Formato en docs/jabuti.md\n");
    char b[48];
    formatar_texto(b, sizeof b, "fondo %d\n", jb_p.fondo); sal_t_(&s, b);
    for (int i = 0; i < jb_p.nfondos; i++) {
        sal_t_(&s, "f "); sal_cadena(&s, jb_p.fondos[i]->nombre); sal_t_(&s, " pintado\n");
        escribir_rejilla(&s, jb_p.fondos[i]->px, FONDO_L, FONDO_A, 2);
    }
    for (int i = 0; i < jb_p.nvars; i++) {
        var_t *v = &jb_p.vars[i];
        sal_t_(&s, "variable "); sal_cadena(&s, v->nombre); sal_c(&s, " ", 1);
        if (v->es_t) sal_cadena(&s, v->t); else sal_num(&s, v->n);
        sal_t_(&s, v->mostrar ? " 1\n" : " 0\n");
    }
    for (int i = 0; i < jb_p.nlistas; i++) {
        sal_t_(&s, "lista "); sal_cadena(&s, jb_p.listas[i].nombre);
        for (int k = 0; k < jb_p.listas[i].n; k++) { sal_c(&s, " ", 1); sal_cadena(&s, jb_p.listas[i].v[k]); }
        sal_c(&s, "\n", 1);
    }
    for (int i = 0; i < jb_p.npers; i++) {
        pers_t *p = jb_p.pers[i];
        sal_t_(&s, "personaje "); sal_cadena(&s, p->nombre); sal_c(&s, "\n", 1);
        sal_t_(&s, "  pos "); sal_num(&s, p->x); sal_c(&s, " ", 1); sal_num(&s, p->y); sal_c(&s, " ", 1); sal_num(&s, p->dir);
        formatar_texto(b, sizeof b, " %d %d %d %d\n", p->tam, p->visible ? 1 : 0, p->giro, p->traje); sal_t_(&s, b);
        for (int k = 0; k < p->ntrajes; k++) {
            sal_t_(&s, "  traje "); sal_cadena(&s, p->trajes[k]->nombre); sal_t_(&s, " pintado\n");
            escribir_rejilla(&s, p->trajes[k]->px, TRAJE_L, TRAJE_L, 4);
        }
        for (int k = 0; k < p->nguiones; k++) {
            formatar_texto(b, sizeof b, "  guion %d %d\n", p->guiones[k].x, p->guiones[k].y); sal_t_(&s, b);
            escribir_pila(&s, p->guiones[k].top, 4);
        }
    }
    if (n) *n = s.n;
    return s.d;
}

void jb_guion_texto(const blk_t *b, char *out, int max)
{
    sal_t s = { 0 };
    for (; b; b = b->sig) {
        sal_t_(&s, jb_ops[b->op].clave);
        escribir_args(&s, b);
        if (jb_ops[b->op].tipo == K_C || jb_ops[b->op].tipo == K_CAP) {
            sal_t_(&s, " {"); 
            char sub[200]; jb_guion_texto(b->c1, sub, sizeof sub); sal_t_(&s, sub);
            if (b->op == O_SI_NO) { sal_t_(&s, "} sino {"); jb_guion_texto(b->c2, sub, sizeof sub); sal_t_(&s, sub); }
            sal_t_(&s, "}");
        }
        if (b->sig) sal_t_(&s, "; ");
    }
    copia_texto(out, s.d ? s.d : "", (size_t)max);
    libera(s.d);
}

/* ============================================================ lectura del texto */

typedef struct { int tipo; char t[40]; } tok_t;   /* 0 fin, 1 palabra, 2 cadena, 3 «(», 4 «)» */
typedef struct { const char *p; bool err; } lex_t;

static tok_t lex_sig(lex_t *l)
{
    tok_t k = { 0, "" };
    while (*l->p == ' ' || *l->p == '\t') l->p++;
    if (!*l->p || *l->p == '#') return k;
    if (*l->p == '(') { k.tipo = 3; l->p++; return k; }
    if (*l->p == ')') { k.tipo = 4; l->p++; return k; }
    int n = 0;
    if (*l->p == '"') {
        k.tipo = 2; l->p++;
        while (*l->p && *l->p != '"') {
            char c = *l->p++;
            if (c == '\\' && *l->p) { c = *l->p++; if (c == 'n') c = '\n'; }
            if (n < (int)sizeof k.t - 1) k.t[n++] = c;
        }
        if (*l->p == '"') l->p++; else l->err = true;
    } else {
        k.tipo = 1;
        while (*l->p && *l->p != ' ' && *l->p != '\t' && *l->p != '(' && *l->p != ')') { if (n < (int)sizeof k.t - 1) k.t[n++] = *l->p; l->p++; }
    }
    k.t[n] = 0;
    return k;
}

static blk_t *leer_bloque(lex_t *l, const char *op_nombre, bool reportero, char *err)
{
    int op = jb_op_buscar(op_nombre);
    if (op < 0) { formatar_texto(err, 160, "bloque desconocido «%s»", op_nombre); return NULL; }
    (void)reportero;
    blk_t *b = jb_blk_nuevo(op);
    if (!b) return NULL;
    int n = jb_nranuras(op);
    for (int i = 0; i < n; i++) {
        tok_t k = lex_sig(l);
        if (k.tipo == 3) {
            tok_t o = lex_sig(l);
            if (o.tipo != 1) { formatar_texto(err, 160, "falta el nombre del bloque tras «(»"); jb_blk_liberar(b); return NULL; }
            b->a[i].rep = leer_bloque(l, o.t, true, err);
            if (!b->a[i].rep) { jb_blk_liberar(b); return NULL; }
            tok_t c = lex_sig(l);
            if (c.tipo != 4) { formatar_texto(err, 160, "falta «)»"); jb_blk_liberar(b); return NULL; }
        } else if (k.tipo == 1 || k.tipo == 2) {
            copia_texto(b->a[i].t, k.t, sizeof b->a[i].t);
        } else if (k.tipo == 0) {
            /* argumento que falta: se queda el valor por defecto */
        } else { formatar_texto(err, 160, "argumento inesperado en «%s»", op_nombre); jb_blk_liberar(b); return NULL; }
    }
    return b;
}

typedef struct { int indent; blk_t *owner; int arm; blk_t *last; guion_t *g; } ctx_t;

static int sangria_de(const char *l) { int n = 0; while (l[n] == ' ') n++; return n; }

bool jb_leer_texto(const char *t, unsigned n)
{
    jb_proy_vaciar();
    jb_p.error[0] = 0;
    char err[160] = "";
    int lin = 0;
    const char *p = t, *fin = t + n;
    pers_t *cp = NULL;
    traje_t *ct = NULL; int ct_fila = 0;
    fondo_t *cf = NULL; int cf_fila = 0;
    ctx_t cx[16]; int ncx = 0;
    bool pend_sino = false; blk_t *sino_de = NULL;
    int g_indent = 0;
    bool primera = true;
    while (p < fin && *p) {
        char linea[400];
        int m = 0;
        while (p < fin && *p && *p != '\n') { if (*p != '\r' && m < (int)sizeof linea - 1) linea[m++] = *p; p++; }
        if (p < fin && *p == '\n') p++;
        linea[m] = 0;
        lin++;
        int ind = sangria_de(linea);
        const char *cu = linea + ind;
        if (!*cu || *cu == '#') continue;
        if (primera) {
            primera = false;
            if (compara_texto_n(cu, "JABUTI", 6) != 0 && compara_texto_n(cu, "BLOQUES", 7) != 0) { formatar_texto(err, sizeof err, "no es un proyecto de Jabuti"); goto malo; }
            continue;
        }
        /* filas de píxeles de un traje o un fondo */
        if (ct && ct_fila < TRAJE_L && ind >= 2 && (cu[0] == '.' || (cu[0] >= '0' && cu[0] <= '9') || (cu[0] >= 'a' && cu[0] <= 'u'))
            && (int)tam_texto(cu) == TRAJE_L) {
            for (int x = 0; x < TRAJE_L; x++) {
                const char *q = acha_char(jb_alfabeto, cu[x]);
                ct->px[ct_fila * TRAJE_L + x] = q ? (uint8_t)(q - jb_alfabeto) : 0;
            }
            ct_fila++;
            continue;
        }
        if (cf && cf_fila < FONDO_A && (int)tam_texto(cu) == FONDO_L && (cu[0] == '.' || (cu[0] >= '0' && cu[0] <= '9') || (cu[0] >= 'a' && cu[0] <= 'u'))) {
            for (int x = 0; x < FONDO_L; x++) {
                const char *q = acha_char(jb_alfabeto, cu[x]);
                cf->px[cf_fila * FONDO_L + x] = q ? (uint8_t)(q - jb_alfabeto) : 0;
            }
            cf_fila++;
            continue;
        }
        /* bloques de un guion */
        if (ncx > 0 && ind > g_indent) {
            lex_t l = { cu, false };
            tok_t o = lex_sig(&l);
            if (o.tipo != 1) { formatar_texto(err, sizeof err, "se esperaba un bloque"); goto malo; }
            while (ncx > 1 && cx[ncx - 1].indent > ind) ncx--;
            ctx_t *c = &cx[ncx - 1];
            if (compara_texto(o.t, "sino") == 0) {
                if (c->indent != ind || !c->last || c->last->op != O_SI_NO) { formatar_texto(err, sizeof err, "«sino» sin «si-no» delante"); goto malo; }
                sino_de = c->last; pend_sino = true;
                continue;
            }
            blk_t *b = leer_bloque(&l, o.t, false, err);
            if (!b) goto malo;
            if (c->indent < 0) c->indent = ind;
            if (ind > c->indent) {
                /* sangría mayor: se abre el cuerpo del bloque C anterior */
                blk_t *own = pend_sino ? sino_de : c->last;
                int arm = pend_sino ? 2 : 1;
                if (!own || (jb_ops[own->op].tipo != K_C && jb_ops[own->op].tipo != K_CAP)) { jb_blk_liberar(b); formatar_texto(err, sizeof err, "sangría inesperada"); goto malo; }
                if (ncx >= 16) { jb_blk_liberar(b); formatar_texto(err, sizeof err, "bloques demasiado anidados"); goto malo; }
                cx[ncx++] = (ctx_t){ ind, own, arm, NULL, c->g };
                c = &cx[ncx - 1];
            } else if (ind < c->indent) { jb_blk_liberar(b); formatar_texto(err, sizeof err, "sangría incoherente"); goto malo; }
            pend_sino = false;
            if (c->last) c->last->sig = b;
            else if (!c->owner) c->g->top = b;
            else if (c->arm == 1) c->owner->c1 = b; else c->owner->c2 = b;
            c->last = b;
            continue;
        }
        /* declaraciones */
        ncx = 0; pend_sino = false;
        lex_t l = { cu, false };
        tok_t k = lex_sig(&l);
        if (k.tipo != 1) { formatar_texto(err, sizeof err, "línea no válida"); goto malo; }
        if (compara_texto(k.t, "fondo") == 0) {
            tok_t v = lex_sig(&l);
            jb_p.fondo = v.tipo ? (int)(v.t[0] - '0') : 0;
        } else if (compara_texto(k.t, "f") == 0) {
            tok_t nm = lex_sig(&l), md = lex_sig(&l);
            if (jb_p.nfondos >= MAX_FONDOS) continue;
            fondo_t *f = aloca_zerado(sizeof *f);
            if (!f) goto malo;
            copia_texto(f->nombre, nm.t, sizeof f->nombre);
            if (compara_texto(md.t, "biblioteca") == 0) {
                tok_t cl = lex_sig(&l);
                int i = jb_buscar_fondo(cl.t);
                if (i < 0) { libera(f); formatar_texto(err, sizeof err, "fondo de la biblioteca desconocido «%s»", cl.t); goto malo; }
                jb_fondo_biblioteca(i, f);
                copia_texto(f->nombre, nm.t, sizeof f->nombre);
                cf = NULL;
            } else { cf = f; cf_fila = 0; }
            jb_p.fondos[jb_p.nfondos++] = f;
            ct = NULL;
        } else if (compara_texto(k.t, "variable") == 0) {
            tok_t nm = lex_sig(&l), v = lex_sig(&l), mo = lex_sig(&l);
            int i = jb_var_crear(nm.t);
            if (i >= 0) {
                int32_t x;
                if (v.tipo == 1 && jb_num_parse(v.t, &x)) { jb_p.vars[i].n = x; jb_p.vars[i].es_t = false; }
                else { copia_texto(jb_p.vars[i].t, v.t, sizeof jb_p.vars[i].t); jb_p.vars[i].es_t = true; }
                jb_p.vars[i].mostrar = mo.tipo ? mo.t[0] == '1' : true;
            }
        } else if (compara_texto(k.t, "lista") == 0) {
            tok_t nm = lex_sig(&l);
            int i = jb_lista_crear(nm.t);
            for (;;) {
                tok_t e = lex_sig(&l);
                if (!e.tipo) break;
                if (i >= 0 && jb_p.listas[i].n < MAX_ELEM) copia_texto(jb_p.listas[i].v[jb_p.listas[i].n++], e.t, 24);
            }
        } else if (compara_texto(k.t, "personaje") == 0) {
            tok_t nm = lex_sig(&l), md = lex_sig(&l);
            cp = jb_pers_nuevo(nm.t);
            if (!cp) { formatar_texto(err, sizeof err, "demasiados personajes"); goto malo; }
            ct = NULL; cf = NULL;
            if (compara_texto(md.t, "biblioteca") == 0) {
                tok_t cl = lex_sig(&l);
                int i = jb_buscar_biblioteca(cl.t);
                if (i < 0) { formatar_texto(err, sizeof err, "personaje de la biblioteca desconocido «%s»", cl.t); goto malo; }
                copia_texto(cp->biblio, cl.t, sizeof cp->biblio);
                for (int q = 0; q < jb_biblioteca_trajes(i) && q < MAX_TRAJES; q++) {
                    traje_t *tt = aloca(sizeof *tt);
                    if (!tt) goto malo;
                    jb_biblioteca_traje(i, q, tt);
                    cp->trajes[cp->ntrajes++] = tt;
                }
                jb_pers_recalcular(cp);
            }
        } else if (compara_texto(k.t, "pos") == 0 && cp) {
            int32_t v[7] = { 0, 0, 90 * U, 100 * U, U, 0, 0 };
            for (int i = 0; i < 7; i++) { tok_t x = lex_sig(&l); if (!x.tipo) break; int32_t q; if (jb_num_parse(x.t, &q)) v[i] = q; }
            cp->x = v[0]; cp->y = v[1]; cp->dir = v[2];
            cp->tam = v[3] / U; if (cp->tam < 1) cp->tam = 1;
            cp->visible = v[4] != 0;
            cp->giro = v[5] / U; cp->traje = v[6] / U;
        } else if (compara_texto(k.t, "traje") == 0 && cp) {
            tok_t nm = lex_sig(&l), md = lex_sig(&l);
            if (cp->ntrajes >= MAX_TRAJES) continue;
            traje_t *tt = jb_traje_nuevo(nm.t);
            if (!tt) goto malo;
            cp->trajes[cp->ntrajes++] = tt;
            if (compara_texto(md.t, "biblioteca") == 0) {
                tok_t cl = lex_sig(&l), ki = lex_sig(&l);
                int i = jb_buscar_biblioteca(cl.t);
                int q = ki.tipo ? ki.t[0] - '0' : 0;
                if (i >= 0 && q >= 0 && q < jb_biblioteca_trajes(i)) { jb_biblioteca_traje(i, q, tt); copia_texto(tt->nombre, nm.t, sizeof tt->nombre); }
                ct = NULL;
            } else { ct = tt; ct_fila = 0; }
            cf = NULL;
        } else if (compara_texto(k.t, "guion") == 0 && cp) {
            tok_t gx = lex_sig(&l), gy = lex_sig(&l);
            if (cp->nguiones >= MAX_GUIONES) continue;
            guion_t *g = &cp->guiones[cp->nguiones++];
            memset(g, 0, sizeof *g);
            int32_t xx = 0, yy = 0;
            jb_num_parse(gx.t, &xx); jb_num_parse(gy.t, &yy);
            g->x = xx / U; g->y = yy / U;
            cx[0] = (ctx_t){ -1, NULL, 0, NULL, g };
            ncx = 1; g_indent = ind;
            ct = NULL; cf = NULL;
        } else {
            formatar_texto(err, sizeof err, "no entiendo «%s»", k.t);
            goto malo;
        }
        continue;
    malo:
        formatar_texto(jb_p.error, sizeof jb_p.error, "línea %d: %s", lin, err);
        return false;
    }
    if (jb_p.nfondos == 0) fondo_por_defecto();
    if (jb_p.fondo < 0 || jb_p.fondo >= jb_p.nfondos) jb_p.fondo = 0;
    for (int i = 0; i < jb_p.npers; i++) {
        pers_t *q = jb_p.pers[i];
        if (q->traje >= q->ntrajes) q->traje = 0;
        jb_pers_recalcular(q);
    }
    return true;
}

/* ============================================================ ficheros */

static const char LEEME[] =
    "Proyecto de «Jabuti» de SaguiOS.\n"
    "El programa está en proyecto.txt, en texto legible (Latin-1). Descripción del formato: docs/jabuti.md.\n"
    "Este archivo es un ZIP corriente: se puede abrir con cualquier programa de ZIP.\n";

bool jb_guardar(const char *ruta)
{
    unsigned n = 0;
    char *t = jb_escribir_texto(&n);
    if (!t) return false;
    zip_escritor_t z;
    memset(&z, 0, sizeof z);
    bool ok = zip_adicionar(&z, "proyecto.txt", (const uint8_t *)t, n, 0, 6, false, 0644) == 0
           && zip_adicionar(&z, "leeme.txt", (const uint8_t *)LEEME, (uint32_t)(sizeof LEEME - 1), 0, 6, false, 0644) == 0
           && zip_concluir(&z) == 0;
    if (ok) {
        int fd = abrir(ruta, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (fd < 0) ok = false;
        else {
            ok = escrever_tudo(fd, z.b.d, (unsigned)z.b.n) >= 0;
            fechar(fd);
        }
    }
    zip_escritor_liberar(&z);
    libera(t);
    return ok;
}

bool jb_abrir(const char *ruta)
{
    int fd = abrir(ruta, ABRIR_LER);
    if (fd < 0) { formatar_texto(jb_p.error, sizeof jb_p.error, "no se encuentra el archivo"); return false; }
    unsigned n = 0;
    char *d = ler_todo_fd(fd, &n);
    fechar(fd);
    if (!d) { formatar_texto(jb_p.error, sizeof jb_p.error, "no se puede leer el archivo"); return false; }
    bool ok;
    if (n > 4 && d[0] == 'P' && d[1] == 'K') {
        zip_leitor_t z;
        ok = false;
        if (zip_abrir(&z, (const uint8_t *)d, (int)n) == 0) {
            for (int i = 0; i < z.cnt; i++)
                if (compara_texto(z.e[i].nome, "proyecto.txt") == 0) {
                    uint8_t *x = NULL;
                    if (zip_extrair(&z, i, &x) >= 0 && x) { ok = jb_leer_texto((const char *)x, z.e[i].tu); libera(x); }
                    break;
                }
            if (!ok && !jb_p.error[0]) formatar_texto(jb_p.error, sizeof jb_p.error, "el ZIP no contiene proyecto.txt");
            zip_leitor_liberar(&z);
        } else formatar_texto(jb_p.error, sizeof jb_p.error, "el archivo ZIP está dañado");
    } else ok = jb_leer_texto(d, n);
    libera(d);
    return ok;
}

/* ============================================================ deshacer */

#define HIST_MAX 40
static char *hist[HIST_MAX]; static int hist_n;
static char *rehacer[HIST_MAX]; static int rehacer_n;

static void liberar_lista(char **l, int *n) { for (int i = 0; i < *n; i++) libera(l[i]); *n = 0; }
void jb_hist_reiniciar(void) { liberar_lista(hist, &hist_n); liberar_lista(rehacer, &rehacer_n); }
void jb_hist_tomar(void)
{
    char *s = jb_escribir_texto(NULL);
    if (!s) return;
    if (hist_n == HIST_MAX) { libera(hist[0]); for (int i = 1; i < HIST_MAX; i++) hist[i - 1] = hist[i]; hist_n--; }
    hist[hist_n++] = s;
    liberar_lista(rehacer, &rehacer_n);
}
void jb_hist_descartar(void) { if (hist_n) libera(hist[--hist_n]); }
bool jb_hist_hay_deshacer(void) { return hist_n > 0; }
bool jb_hist_hay_rehacer(void) { return rehacer_n > 0; }
static bool restaurar(char *s) { bool ok = jb_leer_texto(s, (unsigned)tam_texto(s)); libera(s); return ok; }
bool jb_hist_deshacer(void)
{
    if (!hist_n) return false;
    char *ahora = jb_escribir_texto(NULL);
    if (ahora && rehacer_n < HIST_MAX) rehacer[rehacer_n++] = ahora; else libera(ahora);
    return restaurar(hist[--hist_n]);
}
bool jb_hist_rehacer(void)
{
    if (!rehacer_n) return false;
    char *ahora = jb_escribir_texto(NULL);
    if (ahora && hist_n < HIST_MAX) hist[hist_n++] = ahora; else libera(ahora);
    return restaurar(rehacer[--rehacer_n]);
}
