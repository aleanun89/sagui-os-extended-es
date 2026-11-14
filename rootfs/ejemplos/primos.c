// SPDX-License-Identifier: GPL-2.0-or-later
/* primos.c - criba de Eratostenes (ejemplo de vectores y bucles).
 *     cc /ejemplos/primos.c -o /tmp/primos && /tmp/primos 100
 */
#include <stdio.h>
#include <stdlib.h>

#define MAX 100000
char compuesto[MAX];

int main(int argc, char **argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 100;
    if (n >= MAX) n = MAX - 1;
    int cuantos = 0;
    for (int i = 2; i <= n; i++) {
        if (compuesto[i]) continue;
        cuantos++;
        printf("%d ", i);
        for (int j = i * 2; j <= n; j += i) compuesto[j] = 1;
    }
    printf("\n%d primos hasta %d\n", cuantos, n);
    return 0;
}
