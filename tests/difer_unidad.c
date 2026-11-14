// SPDX-License-Identifier: GPL-2.0-or-later
/* difer_unidad.c - prueba de unidad en el PC de la diferencia por líneas (D240, h240).
 *
 *   gcc -O2 -Wall -Wextra -DDIFER_PC -Iuserland/inc -o difer_unidad tests/difer_unidad.c
 *
 * Compila userland/libsagui/difer.c y, con textos al azar y casos fijos, comprueba
 * que el guion es válido (reconstruye A y B), que las líneas IGUAL son iguales y que
 * su número es el de la secuencia común más larga (calculada aparte por programación
 * dinámica). Escribe "todo bien" y sale con 0 si pasa todo. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../userland/libsagui/difer.c"

static int fallos;
#define VER(c, ...) do { if (!(c)) { fallos++; printf("FALLO: " __VA_ARGS__); printf("\n"); } } while (0)

static int lcs(const difer_linha_t *a, int na, const difer_linha_t *b, int nb)
{
    int *t = calloc((size_t)(na + 1) * (nb + 1), sizeof(int));
    for (int i = 1; i <= na; i++)
        for (int j = 1; j <= nb; j++) {
            int x = igual(&a[i - 1], &b[j - 1]) ? t[(i - 1) * (nb + 1) + j - 1] + 1 : 0;
            int y = t[(i - 1) * (nb + 1) + j], z = t[i * (nb + 1) + j - 1];
            if (y > x) x = y;
            if (z > x) x = z;
            t[i * (nb + 1) + j] = x;
        }
    int r = t[na * (nb + 1) + nb];
    free(t);
    return r;
}

static void comprobar(const difer_linha_t *a, int na, const difer_linha_t *b, int nb, const char *quien)
{
    int max = na + nb + 4;
    difer_op_t *ops = malloc((size_t)max * sizeof *ops);
    int n = difer_linhas(a, na, b, nb, ops, max);
    VER(n >= 0, "%s: devolvió %d", quien, n);
    if (n < 0) { free(ops); return; }
    int ia = 0, ib = 0, iguais = 0;
    for (int k = 0; k < n; k++) {
        difer_op_t *o = &ops[k];
        if (o->tipo == DIFER_IGUAL) {
            VER(o->ia == ia && o->ib == ib, "%s: IGUAL fuera de sitio", quien);
            VER(ia < na && ib < nb && igual(&a[ia], &b[ib]), "%s: IGUAL con líneas distintas", quien);
            ia++; ib++; iguais++;
        } else if (o->tipo == DIFER_QUITA) {
            VER(o->ia == ia, "%s: QUITA fuera de sitio (%d != %d)", quien, o->ia, ia);
            ia++;
        } else {
            VER(o->ib == ib && o->ia == ia, "%s: ANADE fuera de sitio", quien);
            ib++;
        }
    }
    VER(ia == na && ib == nb, "%s: no consume todo (%d/%d, %d/%d)", quien, ia, na, ib, nb);
    VER(iguais == lcs(a, na, b, nb), "%s: no es mínima (%d iguales, LCS %d)", quien, iguais, lcs(a, na, b, nb));
    free(ops);
}

static void texto_vs(const char *x, const char *y, const char *quien)
{
    difer_linha_t a[64], b[64];
    int na = difer_partir(x, (int)strlen(x), a, 64), nb = difer_partir(y, (int)strlen(y), b, 64);
    comprobar(a, na, b, nb, quien);
}

int main(void)
{
    difer_linha_t l[8];
    VER(difer_partir("", 0, l, 8) == 0, "vacío");
    VER(difer_partir("a\n", 2, l, 8) == 1, "una línea con \\n");
    VER(difer_partir("a\n\nb", 4, l, 8) == 3 && l[1].n == 0, "línea vacía en medio");
    VER(difer_partir("a\nb", 3, l, 8) == 2 && l[1].n == 1, "sin \\n final");

    texto_vs("", "", "vacíos");
    texto_vs("a\nb\nc\n", "a\nb\nc\n", "iguales");
    texto_vs("", "a\nb\n", "solo añade");
    texto_vs("a\nb\n", "", "solo quita");
    texto_vs("a\nb\nc\n", "a\nX\nc\n", "cambia una");
    texto_vs("a\nb\nc\nd\ne\n", "b\nc\nd\n", "quita los extremos");
    texto_vs("x\ny\n", "a\nb\nc\n", "nada en común");
    texto_vs("a\nb\na\nb\na\n", "b\na\nb\na\nb\n", "alternados");
    texto_vs("int f(void)\n{\n    return 1;\n}\n", "int f(void)\n{\n    int x = 2;\n    return x;\n}\n", "función");

    /* Al azar: alfabeto pequeño para que haya muchas líneas repetidas. */
    srand(240);
    for (int t = 0; t < 3000; t++) {
        char x[256], y[256];
        int nx = rand() % 20, ny = rand() % 20, p = 0;
        for (int i = 0; i < nx; i++) { x[p++] = (char)('a' + rand() % 4); x[p++] = '\n'; }
        x[p] = 0; p = 0;
        for (int i = 0; i < ny; i++) { y[p++] = (char)('a' + rand() % 4); y[p++] = '\n'; }
        y[p] = 0;
        char q[32];
        snprintf(q, sizeof q, "azar %d", t);
        texto_vs(x, y, q);
    }

    /* Muchas líneas: el prefijo y el sufijo comunes no cuentan para el límite. */
    {
        enum { N = 5000 };
        static difer_linha_t a[N], b[N];
        static char ln[N][8];
        for (int i = 0; i < N; i++) { snprintf(ln[i], 8, "%d", i); a[i].p = b[i].p = ln[i]; a[i].n = b[i].n = (int)strlen(ln[i]); }
        b[2500].p = "cambiada"; b[2500].n = 8;
        static difer_op_t ops[N + 8];
        int n = difer_linhas(a, N, b, N, ops, N + 8);
        VER(n == N + 1, "5000 líneas con una cambiada: %d operaciones", n);
        /* Demasiadas diferencias: -1, sin caerse. */
        static difer_linha_t c[N];
        static char ld[N][8];
        for (int i = 0; i < N; i++) { snprintf(ld[i], 8, "z%d", i); c[i].p = ld[i]; c[i].n = (int)strlen(ld[i]); }
        VER(difer_linhas(a, N, c, N, ops, N + 8) == -1, "demasiadas diferencias devuelve -1");
        /* Poco sitio en ops. */
        VER(difer_linhas(a, 10, b, 10, ops, 3) == -1, "ops pequeño devuelve -1");
    }
    if (fallos) { printf("%d fallos\n", fallos); return 1; }
    printf("todo bien\n");
    return 0;
}
