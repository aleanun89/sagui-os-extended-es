/* padroes.c - fnmatch (patrones de shell), basename y dirname (D230).
 *
 * Parte de la libc común (userland/libc, D210). Las pidió Vanilla Conquer
 * (busca los ficheros sin distinguir mayúsculas con fnmatch y FNM_CASEFOLD),
 * pero son las de POSIX, para cualquier port. Código propio.
 */
#include "fnmatch.h"
#include "libgen.h"

#include <stddef.h>

/* Minúscula de un carácter en ASCII y en Latin-1 (À..Þ salvo ×). */
static int minuscula(int c)
{
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
    return c;
}

static int iguais(int a, int b, int opcoes)
{
    if (opcoes & FNM_CASEFOLD) return minuscula(a) == minuscula(b);
    return a == b;
}

/* Un [...] en p (después del «[»). Devuelve el puntero tras el «]» y en
 * *casou si c entra; NULL si el corchete no se cierra (entonces «[» es un
 * carácter normal, como en POSIX). */
static const unsigned char *conjunto(const unsigned char *p, int c, int opcoes, int *casou)
{
    int negar = 0, ok = 0;
    if (*p == '!' || *p == '^') { negar = 1; p++; }
    const unsigned char *ini = p;
    while (*p && (*p != ']' || p == ini)) {
        int a = *p++;
        if (a == '\\' && !(opcoes & FNM_NOESCAPE) && *p) a = *p++;
        if (*p == '-' && p[1] && p[1] != ']') {
            int b = p[1];
            p += 2;
            if (b == '\\' && !(opcoes & FNM_NOESCAPE) && *p) b = *p++;
            if (opcoes & FNM_CASEFOLD) {
                int m = minuscula(c);
                int M = (c >= 'a' && c <= 'z') ? c - 32 : c;
                if ((m >= a && m <= b) || (M >= a && M <= b) || (c >= a && c <= b)) ok = 1;
            } else if (c >= a && c <= b) {
                ok = 1;
            }
        } else if (iguais(a, c, opcoes)) {
            ok = 1;
        }
    }
    if (*p != ']') return NULL;
    *casou = ok ^ negar;
    return p + 1;
}

static int casar(const unsigned char *p, const unsigned char *n, const unsigned char *inicio, int opcoes)
{
    for (;;) {
        int c = *p;
        /* Un punto al principio (del nombre, o de un componente con
         * FNM_PATHNAME) solo casa con un punto escrito. */
        int ponto_inicial = (opcoes & FNM_PERIOD) && *n == '.' &&
                            (n == inicio || ((opcoes & FNM_PATHNAME) && n[-1] == '/'));
        switch (c) {
        case 0:
            if (*n == 0) return 0;
            if ((opcoes & FNM_LEADING_DIR) && *n == '/') return 0;
            return FNM_NOMATCH;
        case '?':
            if (*n == 0) return FNM_NOMATCH;
            if (*n == '/' && (opcoes & FNM_PATHNAME)) return FNM_NOMATCH;
            if (ponto_inicial) return FNM_NOMATCH;
            p++; n++;
            break;
        case '*': {
            while (*p == '*') p++;
            if (ponto_inicial) return FNM_NOMATCH;
            if (*p == 0) {
                /* El resto entero, salvo que haya otra «/» con FNM_PATHNAME. */
                if (opcoes & FNM_PATHNAME) {
                    for (const unsigned char *k = n; *k; k++)
                        if (*k == '/') return (opcoes & FNM_LEADING_DIR) ? 0 : FNM_NOMATCH;
                }
                return 0;
            }
            for (const unsigned char *k = n;; k++) {
                if (casar(p, k, inicio, opcoes & ~FNM_PERIOD) == 0) return 0;
                if (*k == 0) return FNM_NOMATCH;
                if (*k == '/' && (opcoes & FNM_PATHNAME)) return FNM_NOMATCH;
            }
        }
        case '[': {
            if (*n == 0) return FNM_NOMATCH;
            if (*n == '/' && (opcoes & FNM_PATHNAME)) return FNM_NOMATCH;
            if (ponto_inicial) return FNM_NOMATCH;
            int casou = 0;
            const unsigned char *q = conjunto(p + 1, *n, opcoes, &casou);
            if (!q) {   /* «[» sin cerrar: un carácter más */
                if (*n != '[') return FNM_NOMATCH;
                p++; n++;
                break;
            }
            if (!casou) return FNM_NOMATCH;
            p = q; n++;
            break;
        }
        case '\\':
            if (!(opcoes & FNM_NOESCAPE) && p[1]) { p++; c = *p; }
            /* sigue como un carácter normal */
            /* fallthrough */
        default:
            if (!iguais(c, *n, opcoes)) return FNM_NOMATCH;
            p++; n++;
            break;
        }
    }
}

int fnmatch(const char *padrao, const char *nome, int opcoes)
{
    if (!padrao || !nome) return FNM_NOMATCH;
    return casar((const unsigned char *)padrao, (const unsigned char *)nome, (const unsigned char *)nome, opcoes);
}

/* basename: «/a/b/» -> «b», «/» -> «/», «» o NULL -> «.». */
char *basename(char *caminho)
{
    static char ponto[] = ".", barra[] = "/";
    if (!caminho || !*caminho) return ponto;
    char *fim = caminho;
    while (*fim) fim++;
    while (fim > caminho + 1 && fim[-1] == '/') *--fim = 0;   /* barras del final */
    if (fim == caminho + 1 && *caminho == '/') return barra;
    char *p = fim;
    while (p > caminho && p[-1] != '/') p--;
    return p;
}

/* dirname: «/a/b» -> «/a», «b» -> «.», «/» -> «/». */
char *dirname(char *caminho)
{
    static char ponto[] = ".", barra[] = "/";
    if (!caminho || !*caminho) return ponto;
    char *fim = caminho;
    while (*fim) fim++;
    while (fim > caminho + 1 && fim[-1] == '/') fim--;        /* barras del final */
    while (fim > caminho && fim[-1] != '/') fim--;            /* el último nombre */
    if (fim == caminho) return ponto;
    while (fim > caminho + 1 && fim[-1] == '/') fim--;        /* las barras que lo separan */
    if (fim == caminho + 1 && *caminho == '/') return barra;
    *fim = 0;
    return caminho;
}
