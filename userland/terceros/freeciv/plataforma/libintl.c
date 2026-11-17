/* libintl.c - gettext mínimo sobre catálogos .mo para Freeciv (D374).
 *
 * Solo lo que Freeciv usa: gettext/dgettext/ngettext y bindtextdomain/textdomain. El catálogo se
 * lee entero a memoria la primera vez que se pide un texto del dominio y se busca por bisección
 * en la tabla ordenada del .mo (clave «contexto\4texto» para los textos con contexto; los
 * plurales se guardan como «singular\0plural»). Las fórmulas de plural («Plural-Forms») las
 * evalúa un analizador pequeño de expresiones de C. Sin idioma o sin catálogo, devuelve el
 * texto original (inglés). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <libintl.h>

#define DOM_MAX 8

typedef struct {
    char nombre[32];
    char dir[128];
    unsigned char *mo;       /* NULL = sin catálogo (o aún no leído) */
    int  leido;
    unsigned n;              /* mensajes */
    unsigned off_o, off_t;
    int  nplurales;
    char formula[160];
    char idioma[16];         /* para el que se leyó */
} dom_t;

static dom_t doms[DOM_MAX];
static int ndoms;
static char dom_defecto[32] = "messages";

static dom_t *dom_buscar(const char *nombre, int crear)
{
    for (int i = 0; i < ndoms; i++)
        if (!strcmp(doms[i].nombre, nombre)) return &doms[i];
    if (!crear || ndoms >= DOM_MAX) return NULL;
    dom_t *d = &doms[ndoms++];
    memset(d, 0, sizeof *d);
    strncpy(d->nombre, nombre, sizeof d->nombre - 1);
    return d;
}

static unsigned rd32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24); }

/* El idioma pedido: LANGUAGE (primer elemento) o LANG, sin «.codificación» ni «@variante». */
static void idioma_pedido(char *dst, int max)
{
    const char *s = getenv("LANGUAGE");
    if (!s || !*s) s = getenv("LC_ALL");
    if (!s || !*s) s = getenv("LC_MESSAGES");
    if (!s || !*s) s = getenv("LANG");
    dst[0] = 0;
    if (!s) return;
    int i = 0;
    while (s[i] && s[i] != ':' && s[i] != '.' && s[i] != '@' && i < max - 1) { dst[i] = s[i]; i++; }
    dst[i] = 0;
    if (!strcmp(dst, "C") || !strcmp(dst, "POSIX")) dst[0] = 0;
}

static void formula_leer(dom_t *d)
{
    d->nplurales = 2;
    strcpy(d->formula, "n!=1");
    /* El mensaje vacío ("") lleva la cabecera, que es la primera de la tabla ordenada. */
    if (!d->n) return;
    const unsigned char *t = d->mo + d->off_t;
    unsigned l = rd32(t), o = rd32(t + 4);
    (void)l;
    const char *h = (const char *)d->mo + o;
    const char *p = strstr(h, "Plural-Forms:");
    if (!p) return;
    const char *np = strstr(p, "nplurals=");
    if (np) d->nplurales = atoi(np + 9);
    const char *pl = strstr(p, "plural=");
    if (pl) {
        pl += 7;
        int i = 0;
        while (pl[i] && pl[i] != ';' && pl[i] != '\n' && i < (int)sizeof d->formula - 1) { d->formula[i] = pl[i]; i++; }
        d->formula[i] = 0;
    }
}

static void dom_cargar(dom_t *d)
{
    char lang[16], ruta[300];
    if (d->leido) {
        char act[16];
        idioma_pedido(act, sizeof act);
        if (!strcmp(act, d->idioma)) return;       /* mismo idioma: nada que hacer */
        free(d->mo); d->mo = NULL; d->n = 0; d->leido = 0;
    }
    d->leido = 1;
    idioma_pedido(lang, sizeof lang);
    strcpy(d->idioma, lang);
    if (!lang[0] || !d->dir[0]) return;
    for (int intento = 0; intento < 2; intento++) {
        char l2[16];
        strcpy(l2, lang);
        if (intento == 1) { char *g = strchr(l2, '_'); if (!g) return; *g = 0; }
        snprintf(ruta, sizeof ruta, "%s/%s/LC_MESSAGES/%s.mo", d->dir, l2, d->nombre);
        int fd = open(ruta, O_RDONLY);
        if (fd < 0) continue;
        off_t tam = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        if (tam < 28) { close(fd); continue; }
        unsigned char *b = malloc((size_t)tam);
        if (!b) { close(fd); return; }
        ssize_t got = 0;
        while (got < tam) {
            ssize_t r = read(fd, b + got, (size_t)(tam - got));
            if (r <= 0) break;
            got += r;
        }
        close(fd);
        if (got != tam || rd32(b) != 0x950412de) { free(b); continue; }
        unsigned n = rd32(b + 8), oo = rd32(b + 12), ot = rd32(b + 16);
        if ((unsigned long)oo + 8ul * n > (unsigned long)tam || (unsigned long)ot + 8ul * n > (unsigned long)tam) { free(b); continue; }
        d->mo = b; d->n = n; d->off_o = oo; d->off_t = ot;
        formula_leer(d);
        return;
    }
}

/* ---- evaluador de la fórmula de plural ---- */
typedef struct { const char *s; unsigned long n; } ev_t;
static long ev_expr(ev_t *e);
static void ev_sp(ev_t *e) { while (*e->s == ' ') e->s++; }
static long ev_prim(ev_t *e)
{
    ev_sp(e);
    if (*e->s == '(') { e->s++; long v = ev_expr(e); ev_sp(e); if (*e->s == ')') e->s++; return v; }
    if (*e->s == 'n') { e->s++; return (long)e->n; }
    if (*e->s == '!') { e->s++; return !ev_prim(e); }
    long v = 0;
    while (*e->s >= '0' && *e->s <= '9') v = v * 10 + (*e->s++ - '0');
    return v;
}
static long ev_mul(ev_t *e)
{
    long a = ev_prim(e);
    for (;;) {
        ev_sp(e);
        char c = *e->s;
        if (c != '*' && c != '/' && c != '%') return a;
        e->s++;
        long b = ev_prim(e);
        a = c == '*' ? a * b : !b ? 0 : c == '/' ? a / b : a % b;
    }
}
static long ev_sum(ev_t *e)
{
    long a = ev_mul(e);
    for (;;) {
        ev_sp(e);
        char c = *e->s;
        if (c != '+' && c != '-') return a;
        e->s++;
        long b = ev_mul(e);
        a = c == '+' ? a + b : a - b;
    }
}
static long ev_rel(ev_t *e)
{
    long a = ev_sum(e);
    for (;;) {
        ev_sp(e);
        if (e->s[0] == '<' && e->s[1] == '=') { e->s += 2; a = a <= ev_sum(e); }
        else if (e->s[0] == '>' && e->s[1] == '=') { e->s += 2; a = a >= ev_sum(e); }
        else if (e->s[0] == '<') { e->s++; a = a < ev_sum(e); }
        else if (e->s[0] == '>') { e->s++; a = a > ev_sum(e); }
        else return a;
    }
}
static long ev_eq(ev_t *e)
{
    long a = ev_rel(e);
    for (;;) {
        ev_sp(e);
        if (e->s[0] == '=' && e->s[1] == '=') { e->s += 2; a = a == ev_rel(e); }
        else if (e->s[0] == '!' && e->s[1] == '=') { e->s += 2; a = a != ev_rel(e); }
        else return a;
    }
}
static long ev_and(ev_t *e)
{
    long a = ev_eq(e);
    for (;;) {
        ev_sp(e);
        if (e->s[0] == '&' && e->s[1] == '&') { e->s += 2; long b = ev_eq(e); a = a && b; }
        else return a;
    }
}
static long ev_or(ev_t *e)
{
    long a = ev_and(e);
    for (;;) {
        ev_sp(e);
        if (e->s[0] == '|' && e->s[1] == '|') { e->s += 2; long b = ev_and(e); a = a || b; }
        else return a;
    }
}
static long ev_expr(ev_t *e)
{
    long c = ev_or(e);
    ev_sp(e);
    if (*e->s == '?') {
        e->s++;
        long a = ev_expr(e);
        ev_sp(e);
        if (*e->s == ':') e->s++;
        long b = ev_expr(e);
        return c ? a : b;
    }
    return c;
}

/* Busca la clave (con su largo, que puede incluir un \0 interno de plural); devuelve la traducción o NULL. */
static const char *buscar(dom_t *d, const char *clave, size_t kl)
{
    if (!d->mo || !d->n) return NULL;
    unsigned lo = 0, hi = d->n;
    while (lo < hi) {
        unsigned m = (lo + hi) / 2;
        const unsigned char *e = d->mo + d->off_o + 8 * m;
        unsigned l = rd32(e), o = rd32(e + 4);
        size_t cmp_n = l < kl ? l : kl;
        int c = memcmp(d->mo + o, clave, cmp_n);
        if (!c) c = l < kl ? -1 : l > kl ? 1 : 0;
        if (!c) {
            const unsigned char *t = d->mo + d->off_t + 8 * m;
            return (const char *)d->mo + rd32(t + 4);
        }
        if (c < 0) lo = m + 1; else hi = m;
    }
    return NULL;
}

char *dcgettext(const char *dominio, const char *msgid, int cat)
{
    (void)cat;
    if (!msgid) return NULL;
    dom_t *d = dom_buscar(dominio ? dominio : dom_defecto, 0);
    if (!d) return (char *)msgid;
    dom_cargar(d);
    const char *t = buscar(d, msgid, strlen(msgid));
    return (char *)(t && *t ? t : msgid);
}
char *dgettext(const char *dominio, const char *msgid) { return dcgettext(dominio, msgid, 0); }
char *gettext(const char *msgid) { return dcgettext(NULL, msgid, 0); }

char *dngettext(const char *dominio, const char *msgid, const char *plural, unsigned long n)
{
    dom_t *d = dom_buscar(dominio ? dominio : dom_defecto, 0);
    if (d) {
        dom_cargar(d);
        if (d->mo) {
            char clave[512];
            size_t a = strlen(msgid), b = strlen(plural);
            if (a + b + 1 < sizeof clave) {
                memcpy(clave, msgid, a);
                clave[a] = 0;
                memcpy(clave + a + 1, plural, b);
                const char *t = buscar(d, clave, a + 1 + b);
                if (t) {
                    ev_t e = { d->formula, n };
                    long idx = ev_expr(&e);
                    if (idx < 0 || idx >= d->nplurales) idx = 0;
                    while (idx-- > 0) t += strlen(t) + 1;
                    if (*t) return (char *)t;
                }
            }
        }
    }
    return (char *)(n == 1 ? msgid : plural);
}
char *ngettext(const char *msgid, const char *plural, unsigned long n) { return dngettext(NULL, msgid, plural, n); }

char *textdomain(const char *dominio)
{
    if (dominio) { strncpy(dom_defecto, dominio, sizeof dom_defecto - 1); dom_defecto[sizeof dom_defecto - 1] = 0; }
    return dom_defecto;
}

char *bindtextdomain(const char *dominio, const char *dir)
{
    dom_t *d = dom_buscar(dominio, 1);
    if (!d) return NULL;
    if (dir) {
        strncpy(d->dir, dir, sizeof d->dir - 1);
        free(d->mo); d->mo = NULL; d->n = 0; d->leido = 0;
    }
    return d->dir;
}

char *bind_textdomain_codeset(const char *dominio, const char *codigo)
{
    (void)dominio;
    return (char *)codigo;   /* los .mo ya están en UTF-8, que es lo que Freeciv usa dentro */
}
