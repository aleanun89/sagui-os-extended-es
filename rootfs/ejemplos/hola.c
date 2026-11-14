// SPDX-License-Identifier: GPL-2.0-or-later
/* hola.c - ejemplo del compilador de C de SaguiOS.
 * Compila y ejecuta con:
 *     cc /ejemplos/hola.c -o /tmp/hola
 *     /tmp/hola Tito
 */
#include <stdio.h>

struct sagui { char *nombre; int bananas; };

int factorial(int n)
{
    return n <= 1 ? 1 : n * factorial(n - 1);
}

int main(int argc, char **argv)
{
    struct sagui s;
    s.nombre = argc > 1 ? argv[1] : "mundo";
    s.bananas = 3;
    printf("Hola, %s! Compilado dentro de SaguiOS.\n", s.nombre);
    for (int i = 1; i <= 5; i++)
        printf("%d! = %d\n", i, factorial(i));
    printf("%s tiene %d bananas\n", s.nombre, s.bananas);
    return 0;
}
