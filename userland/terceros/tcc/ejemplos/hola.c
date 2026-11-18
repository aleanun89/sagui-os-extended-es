/* SPDX-License-Identifier: GPL-3.0-only */
/* hola.c - el primer programa con TCC en SaguiOS (D384).
 *   tcc hola.c -o hola && ./hola
 *   tcc -run hola.c            (compila en memoria y lo ejecuta, sin fichero) */
#include <stdio.h>

int main(int argc, char **argv)
{
    printf("Hola desde TCC (%d argumentos)\n", argc);
    for (int i = 1; i < argc; i++)
        printf("  argumento %d: %s\n", i, argv[i]);
    return 0;
}
