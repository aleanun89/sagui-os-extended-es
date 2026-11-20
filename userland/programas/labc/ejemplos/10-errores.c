// SPDX-License-Identifier: GPL-2.0-or-later
/* Este programa tiene errores de compilacion a proposito. Pulsa F5 y haz clic en el error
 * de la lista: el editor salta a su linea. Corrigelo (falta un ;), pulsa F5 otra vez y veras
 * el siguiente error (una variable mal escrita). */
#include <stdio.h>

int main(void)
{
    int total = 0
    for (int i = 0; i < 5; i++)
        total += i;
    printf("total = %d\n", totl);
    return 0;
}
