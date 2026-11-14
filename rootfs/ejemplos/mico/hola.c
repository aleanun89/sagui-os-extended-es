// SPDX-License-Identifier: GPL-2.0-or-later
/* hola.c - un programa pequeño para probar Mico.
 * F5 lo compila con cc y lo ejecuta en el terminal de abajo. */
#include <stdio.h>

#define VECES 3

int cuadrado(int n)
{
    return n * n;
}

int main(void)
{
    char *nombre = "Mico";
    for (int i = 1; i <= VECES; i++)
        printf("%d al cuadrado = %d\n", i, cuadrado(i));
    printf("Hola desde %s\n", nombre);
    return 0;
}
