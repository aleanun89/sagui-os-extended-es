// SPDX-License-Identifier: GPL-2.0-or-later
/* Entrada del usuario: escribe en el cuadro Entrada (abajo) y pulsa Intro.
 * (Se lee una linea con fgets y se interpreta con sscanf.) */
#include <stdio.h>

int main(void)
{
    char linea[80], nombre[40];
    int edad;

    printf("Como te llamas? ");
    fflush(stdout);
    if (!fgets(linea, sizeof linea, stdin) || sscanf(linea, "%39s", nombre) != 1) return 1;
    printf("Que edad tienes? ");
    fflush(stdout);
    if (!fgets(linea, sizeof linea, stdin) || sscanf(linea, "%d", &edad) != 1) return 1;
    printf("Hola %s, el ano que viene tendras %d.\n", nombre, edad + 1);
    return 0;
}
