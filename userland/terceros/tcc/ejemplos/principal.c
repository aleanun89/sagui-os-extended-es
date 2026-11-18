/* SPDX-License-Identifier: GPL-3.0-only */
/* principal.c - ejemplo de programa de varios ficheros con TCC (D384).
 *   tcc -o calculos principal.c calculos.c
 * o por partes, como haría un Makefile:
 *   tcc -c calculos.c && tcc -c principal.c && tcc -o calculos principal.o calculos.o */
#include <stdio.h>
#include <stdlib.h>
#include "calculos.h"

int main(int argc, char **argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 20;
    long long f = factorial(n);
    printf("factorial(%d) = %lld\n", n, f);
    printf("factorial(%d) / 1000003 = %lld (resto %lld)\n", n, f / 1000003, f % 1000003);
    printf("raiz(2) = %.6f\n", raiz(2.0));
    printf("primos hasta 1000: %d\n", cuenta_primos(1000));
    return 0;
}
