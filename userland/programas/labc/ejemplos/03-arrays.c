// SPDX-License-Identifier: GPL-2.0-or-later
/* Arrays: guardar varios valores y recorrerlos. */
#include <stdio.h>

int main(void)
{
    int notas[6] = { 7, 4, 9, 5, 10, 6 };
    int suma = 0, mayor = notas[0], i;

    for (i = 0; i < 6; i++) {
        suma += notas[i];
        if (notas[i] > mayor) mayor = notas[i];
    }
    printf("suma = %d, media = %.2f, mayor = %d\n", suma, suma / 6.0, mayor);

    /* ordenar de menor a mayor (burbuja) */
    for (i = 0; i < 6; i++)
        for (int k = 0; k < 5 - i; k++)
            if (notas[k] > notas[k + 1]) {
                int t = notas[k]; notas[k] = notas[k + 1]; notas[k + 1] = t;
            }
    for (i = 0; i < 6; i++) printf("%d ", notas[i]);
    printf("\n");

    char palabra[] = "SaguiOS";
    printf("%s tiene %d letras\n", palabra, (int)sizeof palabra - 1);
    return 0;
}
