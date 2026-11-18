/* SPDX-License-Identifier: GPL-3.0-only */
/* liberada.c - otro error típico: usar memoria después de liberarla (D384).
 *   tcc -b liberada.c -o liberada && ./liberada   con -b se para y dice el fichero y la línea */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    char *nombre = malloc(16);
    strcpy(nombre, "aldeano");
    free(nombre);
    printf("primera letra: %c\n", nombre[0]);   /* el error: nombre ya se liberó */
    return 0;
}
