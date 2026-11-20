// SPDX-License-Identifier: GPL-2.0-or-later
/* Un error clasico: escribir fuera del array. Prueba dos veces:
 * 1) sin la casilla "Comprobar limites (-b)": no se nota nada (pero esta mal).
 * 2) con ella marcada: se para en la linea del fallo. */
#include <stdio.h>

int main(void)
{
    int puntos[5];
    for (int i = 0; i <= 5; i++) {      /* i llega a 5: fuera del array */
        puntos[i] = i * i;
        printf("puntos[%d] = %d\n", i, puntos[i]);
    }
    printf("fin\n");
    return 0;
}
