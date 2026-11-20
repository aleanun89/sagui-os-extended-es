// SPDX-License-Identifier: GPL-2.0-or-later
/* Punteros: la direccion de una variable y como se usa. */
#include <stdio.h>
#include <stdlib.h>

void intercambiar(int *a, int *b)
{
    int t = *a;
    *a = *b;
    *b = t;
}

int main(void)
{
    int x = 10, y = 20;
    int *p = &x;
    printf("x vale %d y p apunta a x: *p = %d\n", x, *p);
    *p = 99;
    printf("tras *p = 99, x vale %d\n", x);

    intercambiar(&x, &y);
    printf("tras intercambiar: x = %d, y = %d\n", x, y);

    int *lista = malloc(5 * sizeof *lista);      /* memoria dinamica */
    for (int i = 0; i < 5; i++) lista[i] = i * i;
    for (int i = 0; i < 5; i++) printf("lista[%d] = %d\n", i, *(lista + i));
    free(lista);
    return 0;
}
