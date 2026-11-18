/* SPDX-License-Identifier: GPL-3.0-only */
/* limites.c - un error típico: escribir fuera de un array (D384).
 *   tcc limites.c -o limites && ./limites      sin -b: escribe en memoria ajena y sigue como si nada
 *   tcc -b limites.c -o limites && ./limites   con -b: se para y dice el fichero y la línea */
#include <stdio.h>

int puntos[10];

int main(void)
{
    int i;
    for (i = 0; i <= 10; i++)   /* el error: <= en vez de < */
        puntos[i] = i * i;
    printf("puntos[9] = %d\n", puntos[9]);
    return 0;
}
