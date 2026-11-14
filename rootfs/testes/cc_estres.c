// SPDX-License-Identifier: GPL-2.0-or-later
/* cc_estres.c - calculo largo con muchos registros de coma flotante vivos
 * a la vez (XMM1..XMM6 o la pila x87), para probar que SaguiOS guarda el
 * estado de la FPU de cada proceso (tests/b7_cc_flotante.py).
 *     cc_estres <semilla> <vueltas> */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    int semilla = argc > 1 ? atoi(argv[1]) : 1;
    int vueltas = argc > 2 ? atoi(argv[2]) : 100000;
    double a = semilla * 0.5, b = 1.25 + semilla * 0.01, c = -0.75, d = 2.5, e = 0.125;
    double acc = 0;
    for (int i = 0; i < vueltas; i++) {
        double r = a * (b + (c * (d - (e * (a + (b * (c - d)))))));
        acc = acc * 0.999 + r * 1e-3;
        a = a * 0.9999 + 0.0001 * (i % 7);
        e = e + 1e-6;
    }
    printf("estres %d: %.15g\n", semilla, acc);
    return 0;
}
