/* SPDX-License-Identifier: GPL-3.0-only */
/* calculos.c - la mitad «biblioteca» del ejemplo de varios ficheros (D384). */
#include <math.h>
#include "calculos.h"

long long factorial(int n)
{
    long long r = 1;
    while (n > 1)
        r *= n--;
    return r;
}

double raiz(double x)
{
    return sqrt(x);
}

int cuenta_primos(int hasta)
{
    int n = 0;
    for (int i = 2; i <= hasta; i++) {
        int primo = 1;
        for (int d = 2; d * d <= i; d++)
            if (i % d == 0) { primo = 0; break; }
        n += primo;
    }
    return n;
}
