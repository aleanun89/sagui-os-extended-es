// SPDX-License-Identifier: GPL-3.0-only
/* util.c - textos y listas que crecen, arena del árbol y mensajes. */
#include "sagsh.h"

void *sh_aloca(size_t n)
{
    void *p = aloca(n ? n : 1);
    if (!p) {
        escrever_texto(ERROS, "sagsh: sin memoria\n");
        sair(2);
    }
    return p;
}

void *sh_realoca(void *p, size_t n)
{
    void *q = realoca(p, n ? n : 1);
    if (!q) {
        escrever_texto(ERROS, "sagsh: sin memoria\n");
        sair(2);
    }
    return q;
}

char *sh_dup_n(const char *s, int n)
{
    char *d = sh_aloca((size_t)n + 1);
    memcpy(d, s, (size_t)n);
    d[n] = 0;
    return d;
}

char *sh_dup(const char *s) { return sh_dup_n(s ? s : "", (int)tam_texto(s ? s : "")); }

static void tx_garantir(texto_t *t, int mais)
{
    if (t->n + mais + 1 <= t->cap) return;
    int cap = t->cap ? t->cap : 32;
    while (cap < t->n + mais + 1) cap *= 2;
    t->s = sh_realoca(t->s, (size_t)cap);
    t->cap = cap;
}

void tx_car(texto_t *t, char c)
{
    tx_garantir(t, 1);
    t->s[t->n++] = c;
    t->s[t->n] = 0;
}

void tx_junta_n(texto_t *t, const char *s, int n)
{
    if (n <= 0) { tx_garantir(t, 0); t->s[t->n] = 0; return; }
    tx_garantir(t, n);
    memcpy(t->s + t->n, s, (size_t)n);
    t->n += n;
    t->s[t->n] = 0;
}

void tx_junta(texto_t *t, const char *s) { tx_junta_n(t, s, (int)tam_texto(s)); }

void tx_formatar(texto_t *t, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(buf, sizeof buf, fmt, ap);
    va_end(ap);
    tx_junta(t, buf);
}

char *tx_texto(texto_t *t)
{
    tx_garantir(t, 0);
    t->s[t->n] = 0;
    return t->s;
}

void tx_liberar(texto_t *t)
{
    libera(t->s);
    t->s = NULL;
    t->n = t->cap = 0;
}

void lista_poe(lista_t *l, char *s)
{
    if (l->n + 2 > l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->v = sh_realoca(l->v, (size_t)l->cap * sizeof(char *));
    }
    l->v[l->n++] = s;
    l->v[l->n] = NULL;
}

void lista_liberar(lista_t *l)
{
    for (int i = 0; i < l->n; i++) libera(l->v[i]);
    libera(l->v);
    l->v = NULL;
    l->n = l->cap = 0;
}

static bool letra(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static bool digito(char c) { return c >= '0' && c <= '9'; }

bool eh_nome_n(const char *s, int n)
{
    if (n <= 0 || !letra(s[0])) return false;
    for (int i = 1; i < n; i++)
        if (!letra(s[i]) && !digito(s[i])) return false;
    return true;
}

bool eh_nome(const char *s) { return eh_nome_n(s, (int)tam_texto(s)); }

bool eh_numero(const char *s)
{
    if (*s == '-' || *s == '+') s++;
    if (!*s) return false;
    for (; *s; s++)
        if (!digito(*s)) return false;
    return true;
}

int para_int(const char *s) { return texto_para_int(s); }

void aspas_simples(texto_t *t, const char *s)
{
    tx_car(t, '\'');
    for (; *s; s++) {
        if (*s == '\'') tx_junta(t, "'\\''");
        else tx_car(t, *s);
    }
    tx_car(t, '\'');
}

int erro(const char *fmt, ...)
{
    char buf[400];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(buf, sizeof buf, fmt, ap);
    va_end(ap);
    texto_t t = {0};
    tx_junta(&t, "sagsh: ");
    if (sh.script && !sh.interativo) tx_formatar(&t, "%s: línea %d: ", sh.script, sh.linha);
    tx_junta(&t, buf);
    tx_car(&t, '\n');
    escrever_tudo(ERROS, t.s, (unsigned)t.n);
    tx_liberar(&t);
    return 1;
}

/* Escribe todo; si la salida es un pipe sin lector y el shell está ejecutando
 * una etapa de pipe dentro de sí mismo, la etapa termina (como SIGPIPE). */
int escrever_saida(int fd, const char *s, int n)
{
    int r = escrever_tudo(fd, s, (unsigned)n);
    if (r == ERRO_PIPE_QUEBRADO && sh.em_estagio && sh.fluxo == F_NORMAL) {
        sh.fluxo = F_PIPE;
        sh.status = 141;
    }
    return r;
}

/* Ordena textos (inserción: las listas son pequeñas). */
void ordenar(char **v, int n)
{
    for (int i = 1; i < n; i++) {
        char *x = v[i];
        int j = i - 1;
        while (j >= 0 && compara_texto(v[j], x) > 0) { v[j + 1] = v[j]; j--; }
        v[j + 1] = x;
    }
}

/* ------------------------------------------------------------ arena */

arena_t *arena_nova(void)
{
    arena_t *a = sh_aloca(sizeof *a);
    a->blocos = NULL;
    a->fixa = 0;
    return a;
}

void *arena_aloca(arena_t *a, size_t n)
{
    n = (n + 7) & ~7u;
    bloco_arena_t *b = a->blocos;
    if (!b || b->usado + (int)n > b->tam) {
        int tam = n > 4000 ? (int)n : 4000;
        b = sh_aloca(sizeof(bloco_arena_t) + (size_t)tam + 8);
        b->tam = tam;
        b->usado = 0;
        b->prox = a->blocos;
        a->blocos = b;
    }
    char *base = (char *)(((size_t)(b + 1) + 7) & ~(size_t)7);
    void *p = base + b->usado;
    b->usado += (int)n;
    memset(p, 0, n);
    return p;
}

char *arena_dup_n(arena_t *a, const char *s, int n)
{
    char *d = arena_aloca(a, (size_t)n + 1);
    memcpy(d, s, (size_t)n);
    d[n] = 0;
    return d;
}

void arena_liberar(arena_t *a)
{
    if (!a || a->fixa) return;
    bloco_arena_t *b = a->blocos;
    while (b) {
        bloco_arena_t *p = b->prox;
        libera(b);
        b = p;
    }
    libera(a);
}
