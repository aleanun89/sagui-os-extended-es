// SPDX-License-Identifier: GPL-2.0-or-later
/* prova.c - el puente del QuickJS y el DOM en JavaScript, en el PC (D488).
 *
 * Un «motor» de juguete (un árbol de nodos con un lector de HTML mínimo) hace
 * de navegador: recibe las operaciones del DOM por los ganchos del puente, como
 * el Cipó se las pasa al motor de verdad. Así se comprueba, sin QEMU, que la
 * copia del árbol que tiene el JavaScript y la del motor son siempre la misma,
 * y el comportamiento de los eventos, los selectores y el bucle de eventos.
 *
 *   prova página.html script.js [script2.js...]
 *
 * Escribe en la salida lo que los scripts escriben con console.log, y al final
 * «arvore-ok» o «arvore-DIFERENTE» (las dos copias del árbol). */
#include "sagui.h"
#include "qjs_bridge.h"

#define MAXN 60000
typedef struct { char tag[24]; int pai, prim, ult, prox, ant; char *txt; char *an[16], *av[16]; int na; int vivo; } no_t;
static no_t N[MAXN];
static int nn;
static int ops_inserir, ops_html, ops_caixa;
static int rol_x, rol_y;

static int novo(const char *tag)
{
    if (nn >= MAXN) return -1;
    no_t *n = &N[nn];
    memset(n, 0, sizeof *n);
    snprintf(n->tag, sizeof n->tag, "%s", tag);
    n->pai = n->prim = n->ult = n->prox = n->ant = -1;
    n->vivo = 1;
    return nn++;
}
static void desligar(int k)
{
    no_t *n = &N[k];
    if (n->pai < 0) return;
    no_t *p = &N[n->pai];
    if (n->ant >= 0) N[n->ant].prox = n->prox; else p->prim = n->prox;
    if (n->prox >= 0) N[n->prox].ant = n->ant; else p->ult = n->ant;
    n->pai = n->ant = n->prox = -1;
}
static int dentro(int a, int b) { for (; a >= 0; a = N[a].pai) if (a == b) return 1; return 0; }
static int meter(int p, int k, int antes)
{
    if (p < 0 || p >= nn || k < 0 || k >= nn || dentro(p, k)) return 0;
    if (antes >= 0 && N[antes].pai != p) { printf("ERRO: inserir antes de %d que nao e filho de %d\n", antes, p); return 0; }
    desligar(k);
    N[k].pai = p;
    if (antes >= 0) {
        N[k].prox = antes; N[k].ant = N[antes].ant;
        if (N[antes].ant >= 0) N[N[antes].ant].prox = k; else N[p].prim = k;
        N[antes].ant = k;
    } else {
        N[k].ant = N[p].ult;
        if (N[p].ult >= 0) N[N[p].ult].prox = k; else N[p].prim = k;
        N[p].ult = k;
    }
    return 1;
}
static void atr(int k, const char *nome, const char *v)
{
    no_t *n = &N[k];
    int i;
    for (i = 0; i < n->na; i++) if (!strcmp(n->an[i], nome)) break;
    if (!v) { if (i < n->na) { free(n->an[i]); free(n->av[i]); for (; i + 1 < n->na; i++) { n->an[i] = n->an[i + 1]; n->av[i] = n->av[i + 1]; } n->na--; } return; }
    if (i < n->na) { free(n->av[i]); n->av[i] = strdup(v); return; }
    if (n->na < 16) { n->an[n->na] = strdup(nome); n->av[n->na] = strdup(v); n->na++; }
}

/* ---- el lector de HTML de juguete ---- */
static int vazio(const char *t) { return !strcmp(t, "br") || !strcmp(t, "img") || !strcmp(t, "input") || !strcmp(t, "hr") || !strcmp(t, "meta") || !strcmp(t, "link"); }
static void ent(char *s)
{
    char *o = s;
    while (*s) {
        if (!strncmp(s, "&amp;", 5)) { *o++ = '&'; s += 5; }
        else if (!strncmp(s, "&lt;", 4)) { *o++ = '<'; s += 4; }
        else if (!strncmp(s, "&gt;", 4)) { *o++ = '>'; s += 4; }
        else if (!strncmp(s, "&quot;", 6)) { *o++ = '"'; s += 6; }
        else *o++ = *s++;
    }
    *o = 0;
}
static void ler(const char *h, int raiz)
{
    int pilha[256], np = 0;
    pilha[np++] = raiz;
    while (*h) {
        if (*h != '<') {
            const char *f = strchr(h, '<');
            if (!f) f = h + strlen(h);
            int so = 1;
            for (const char *q = h; q < f; q++) if (*q != ' ' && *q != '\n' && *q != '\t') so = 0;
            if (!so) {
                int t = novo("#texto");
                N[t].txt = strndup(h, (size_t)(f - h)); ent(N[t].txt);
                meter(pilha[np - 1], t, -1);
            }
            h = f;
            continue;
        }
        if (!strncmp(h, "<!--", 4)) { const char *f = strstr(h, "-->"); h = f ? f + 3 : h + strlen(h); continue; }
        if (h[1] == '!') { while (*h && *h != '>') h++; if (*h) h++; continue; }
        if (h[1] == '/') {
            char t[24]; int k = 0; h += 2;
            while (*h && *h != '>' && k < 23) t[k++] = (char)(*h >= 'A' && *h <= 'Z' ? *h + 32 : *h), h++;
            t[k] = 0;
            while (*h && *h != '>') h++;
            if (*h) h++;
            for (int q = np - 1; q > 0; q--) if (!strcmp(N[pilha[q]].tag, t)) { np = q; break; }
            continue;
        }
        h++;
        char t[24]; int k = 0;
        while (*h && *h != ' ' && *h != '>' && *h != '/' && k < 23) t[k++] = (char)(*h >= 'A' && *h <= 'Z' ? *h + 32 : *h), h++;
        t[k] = 0;
        int e = novo(t);
        meter(pilha[np - 1], e, -1);
        while (*h && *h != '>') {
            while (*h == ' ' || *h == '/' || *h == '\n') h++;
            if (*h == '>' || !*h) break;
            char nm[64], v[512]; int a = 0, b = 0;
            while (*h && *h != '=' && *h != ' ' && *h != '>' && a < 63) nm[a++] = *h++;
            nm[a] = 0;
            v[0] = 0;
            if (*h == '=') {
                h++;
                char q = (*h == '"' || *h == '\'') ? *h++ : 0;
                while (*h && (q ? *h != q : (*h != ' ' && *h != '>')) && b < 511) v[b++] = *h++;
                v[b] = 0;
                if (q && *h) h++;
            }
            ent(v);
            atr(e, nm, v);
        }
        if (*h) h++;
        if (!vazio(t) && np < 255) pilha[np++] = e;
        if (!strcmp(t, "script") || !strcmp(t, "style")) {
            char fim[32]; snprintf(fim, sizeof fim, "</%s", t);
            const char *f = strstr(h, fim);
            if (!f) f = h + strlen(h);
            if (f > h) { int x = novo("#texto"); N[x].txt = strndup(h, (size_t)(f - h)); meter(e, x, -1); }
            h = f;
        }
    }
}

/* ---- exportar (el formato de qjs_bridge.h) ---- */
static char *buf; static int bn, bc;
static void por(const char *s)
{
    int n = (int)strlen(s);
    if (bn + n + 1 > bc) { bc = (bn + n + 1) * 2; buf = realloc(buf, (size_t)bc); }
    memcpy(buf + bn, s, (size_t)n + 1); bn += n;
}
static void campo(const char *s)
{
    char t[3] = { 0 };
    for (; s && *s; s++) {
        if (*s == '\\') por("\\\\"); else if (*s == '\t') por("\\t"); else if (*s == '\n') por("\\n"); else if (*s == '\r') por("\\r");
        else { t[0] = *s; por(t); }
    }
}
static void exp_no(int k, int pai)
{
    char b[64];
    no_t *n = &N[k];
    snprintf(b, sizeof b, "N\t%d\t%d\t", k, pai); por(b);
    if (!strcmp(n->tag, "#texto")) { por("#texto\t\t\t0\t"); campo(n->txt); por("\n"); return; }
    const char *id = "", *cl = "";
    for (int i = 0; i < n->na; i++) { if (!strcmp(n->an[i], "id")) id = n->av[i]; if (!strcmp(n->an[i], "class")) cl = n->av[i]; }
    campo(n->tag); por("\t"); campo(id); por("\t"); campo(cl);
    snprintf(b, sizeof b, "\t%d\t\n", n->na); por(b);
    for (int i = 0; i < n->na; i++) { por("A\t"); campo(n->an[i]); por("\t"); campo(n->av[i]); por("\n"); }
}
static void exp_filhos(int k) { for (int c = N[k].prim; c >= 0; c = N[c].prox) { exp_no(c, k); exp_filhos(c); } }
static int achar(int k, const char *tag) { if (!strcmp(N[k].tag, tag)) return k; for (int c = N[k].prim; c >= 0; c = N[c].prox) { int r = achar(c, tag); if (r >= 0) return r; } return -1; }

/* ---- los ganchos ---- */
static void g_log(const char *t) { printf("%s\n", t); }
static void g_titulo(const char *t) { printf("[titulo] %s\n", t); }
static void g_navegar(const char *u) { printf("[navegar] %s\n", u); }
static void g_atributo(int k, const char *nome, const char *v) { if (k >= 0 && k < nn) atr(k, nome, v); }
static int g_criar(const char *tag, const char *txt) { int k = novo(tag); if (k >= 0 && txt) N[k].txt = strdup(txt); return k; }
static int g_inserir(int p, int k, int a) { ops_inserir++; return meter(p, k, a); }
static int g_tirar(int k) { if (k < 0 || k >= nn) return 0; desligar(k); return 1; }
static int g_dado(int k, const char *t) { if (k < 0 || k >= nn) return 0; free(N[k].txt); N[k].txt = strdup(t); return 1; }
static char *g_html(int k, const char *h, int *n)
{
    ops_html++;
    if (k < 0 || k >= nn) return NULL;
    while (N[k].prim >= 0) desligar(N[k].prim);
    ler(h, k);
    bn = 0; if (buf) buf[0] = 0;
    por("D\t-1\t-1\n");
    exp_filhos(k);
    char *r = malloc((size_t)bn + 1);
    memcpy(r, buf, (size_t)bn + 1);
    *n = bn;
    return r;
}
/* cajas de juguete: cada elemento, 10 px por debajo del anterior en orden de documento */
static int orden(int k) { int c = 0; for (int a = k; a >= 0; a = N[a].pai) c++; return c; }
static int g_caixa(int k, int *o)
{
    ops_caixa++;
    memset(o, 0, 16 * sizeof(int));
    if (k < 0 || k >= nn) return 0;
    o[0] = 8 * orden(k); o[1] = 10 * k; o[2] = 100; o[3] = 10; o[12] = 100; o[13] = 10; o[14] = 1;
    return 1;
}
static int g_estilo(int k, const char *prop, char *o, int max) { (void)k; if (!strcmp(prop, "display")) { snprintf(o, (size_t)max, "block"); return 1; } return 0; }
static int g_em(int x, int y) { (void)x; int k = (y + rol_y) / 10; return k >= 0 && k < nn ? k : -1; }
static void g_janela(int *o) { o[0] = 800; o[1] = 600; o[2] = rol_x; o[3] = rol_y; o[4] = 800; o[5] = 3000; }
static void g_rolar(int x, int y) { rol_x = x; rol_y = y; printf("[rolar] %d %d\n", x, y); }
static void g_historia(const char *u, int s) { printf("[historia] %s %s\n", s ? "replace" : "push", u); }
static void g_ir(int d) { printf("[ir] %d\n", d); }

static char *ler_arquivo(const char *c, int *n)
{
    FILE *f = fopen(c, "rb");
    if (!f) { perror(c); exit(2); }
    fseek(f, 0, SEEK_END); long t = ftell(f); fseek(f, 0, SEEK_SET);
    char *d = malloc((size_t)t + 1);
    if (fread(d, 1, (size_t)t, f) != (size_t)t) exit(2);
    d[t] = 0; fclose(f);
    *n = (int)t;
    return d;
}

static void comandos(qjs_ponte_t *p, const char *s);

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "uso: prova pagina.html script.js...\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    int n;
    char *html = ler_arquivo(argv[1], &n);
    int raiz = novo("#doc");
    ler(html, raiz);
    bn = 0;
    int corpo = achar(raiz, "body"), doc = achar(raiz, "html");
    char b[64]; snprintf(b, sizeof b, "D\t%d\t%d\n", corpo, doc); por(b);
    exp_no(raiz, -1); exp_filhos(raiz);
    qjs_ganchos_t g = { .titulo = g_titulo, .log = g_log, .navegar = g_navegar, .atributo = g_atributo,
        .dom_criar = g_criar, .dom_inserir = g_inserir, .dom_tirar = g_tirar, .dom_dado = g_dado, .dom_html = g_html,
        .caixa = g_caixa, .estilo = g_estilo, .elemento_em = g_em, .janela = g_janela, .rolar = g_rolar,
        .historia = g_historia, .ir = g_ir };
    qjs_ponte_t *p = qjs_criar(&g);
    if (!p) { printf("sem ponte\n"); return 1; }
    qjs_definir_url(p, "http://exemplo.local/app/index.html");
    char *texto = malloc((size_t)bn + 1); memcpy(texto, buf, (size_t)bn + 1);
    qjs_definir_dom(p, texto, bn);
    for (int i = 2; i < argc; i++) {
        int m;
        char *src = ler_arquivo(argv[i], &m);
        /* «//! orden» al principio de una línea: lo que hace la persona (ver comandos()) */
        if (qjs_executar(p, src, m, argv[i]) != 0) printf("[erro no script %s]\n", argv[i]);
        comandos(p, src);
        free(src);
    }
    qjs_carga(p);
    /* el bucle de eventos: hasta que no quede nada (o 3 s) */
    unsigned t0 = tempo_ms();
    while (tempo_ms() - t0 < 3000) {
        int e = qjs_espera(p);
        if (e < 0) break;
        qjs_tique(p, tempo_ms());
        if (e > 0) { struct timespec ts = { 0, 1000000 }; nanosleep(&ts, NULL); }
    }
    /* las dos copias del árbol: la del JavaScript y la del «motor» */
    static char js[1 << 20];
    static const char cmp[] = "(function(){function f(n){var s=n._id+'<';for(var i=0;i<n._filhos.length;i++){var c=n._filhos[i];if(c._id>=0)s+=f(c)+',';}return s+'>';}return f(document);})()";
    qjs_executar_texto(p, cmp, (int)sizeof cmp - 1, "<cmp>", js, sizeof js);
    bn = 0;
    struct { int k; } pilha;
    (void)pilha;
    /* el mismo recorrido en C */
    char *c = malloc(1 << 20); int cn = 0;
    int st[4096], it[4096], sp = 0;
    (void)it;
    st[sp++] = raiz;
    /* recursivo, más simple */
    {
        void rec(int k) {
            cn += sprintf(c + cn, "%d<", k);
            for (int q = N[k].prim; q >= 0; q = N[q].prox) { rec(q); cn += sprintf(c + cn, ","); }
            cn += sprintf(c + cn, ">");
        }
        rec(raiz);
    }
    printf("%s\n", strcmp(js, c) == 0 ? "arvore-ok" : "arvore-DIFERENTE");
    if (strcmp(js, c)) printf("js: %.400s\nc:  %.400s\n", js, c);
    printf("ops: inserir=%d html=%d caixa=%d\n", ops_inserir, ops_html, ops_caixa);
    qjs_destruir(p);
    return 0;
}

/* Las líneas «//! tipo id {json}» del script: eventos que manda el navegador
 * después de correr el script (id = -1: el que tiene el foco). */
static void comandos(qjs_ponte_t *p, const char *s)
{
    for (const char *l = s; l && *l; l = strchr(l, '\n') ? strchr(l, '\n') + 1 : NULL) {
        if (strncmp(l, "//! ", 4)) continue;
        char tipo[32], json[512] = "{}"; int id = -1;
        const char *q = l + 4;
        char alvo[128] = "";
        if (sscanf(q, "%31s %127s", tipo, alvo) < 1) continue;
        if (alvo[0] == '#' || alvo[0] == '.' || alvo[0] == '[') {     /* un selector: el id del elemento */
            char cod[256], r[32];
            snprintf(cod, sizeof cod, "(function(){var e=document.querySelector('%s');return e?e._id:-1;})()", alvo);
            qjs_executar_texto(p, cod, (int)strlen(cod), "<sel>", r, sizeof r);
            id = atoi(r);
        } else id = atoi(alvo);
        const char *j = strchr(q, '{');
        const char *f = strchr(q, '\n');
        if (j && (!f || j < f)) { int k = 0; while (j[k] && j[k] != '\n' && k < 511) { json[k] = j[k]; k++; } json[k] = 0; }
        if (!strcmp(tipo, "tique")) {    /* esperar id ms corriendo el bucle */
            unsigned t0 = tempo_ms();
            while (tempo_ms() - t0 < (unsigned)id) { qjs_tique(p, tempo_ms()); struct timespec ts = { 0, 1000000 }; nanosleep(&ts, NULL); }
            continue;
        }
        int r = qjs_despachar(p, id, tipo, json);
        printf("[evento %s %d] %s\n", tipo, id, r == 1 ? "cancelado" : r == 0 ? "ok" : "sem-js");
    }
}
