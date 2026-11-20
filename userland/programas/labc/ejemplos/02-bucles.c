// SPDX-License-Identifier: GPL-2.0-or-later
/* Bucles: for, while y condiciones. */
#include <stdio.h>

int main(void)
{
    int i;
    printf("Tabla del 7:\n");
    for (i = 1; i <= 10; i++)
        printf("7 x %2d = %2d\n", i, 7 * i);

    int n = 27, pasos = 0;
    while (n != 1) {                 /* conjetura de Collatz */
        n = (n % 2 == 0) ? n / 2 : 3 * n + 1;
        pasos++;
    }
    printf("27 llega a 1 en %d pasos\n", pasos);

    for (i = 1; i <= 15; i++) {
        if (i % 15 == 0) printf("FizzBuzz ");
        else if (i % 3 == 0) printf("Fizz ");
        else if (i % 5 == 0) printf("Buzz ");
        else printf("%d ", i);
    }
    printf("\n");
    return 0;
}
