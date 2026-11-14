// SPDX-License-Identifier: GPL-2.0-or-later
/* bucle.c - benchmark entero del generador (D108): criba, mezcla con
 * divisiones y desplazamientos, y producto de matrices. Imprime sumas de
 * comprobacion (iguales con cualquier optimizacion) y el tiempo en ms. */
#ifndef __SAGUIOS__
#include <stdio.h>   /* con GCC en el ordenador de desarrollo */
int time_ms(void) { return 0; }
#endif
#ifdef __LINUX__
int time_ms(void) { return 0; }   /* solo para probar fuera de SaguiOS */
#endif
#define N 60000
#define REP 400
char criba[N + 1];
int ma[32][32], mb[32][32], mc[32][32];
int tabla[64];

int hash(int x)
{
    x = x * 31 + (x >> 3);
    x = x ^ (x << 5);
    x = x % 1000003;
    if (x < 0) x = -x;
    return x / 7 + x % 7;
}

int main(void)
{
    int t0 = time_ms();
    int i, j, k, s = 0, n = 0, r;
    for (r = 0; r < REP; r++) {
        for (i = 0; i <= N; i++) criba[i] = 1;
        for (i = 2; i * i <= N; i++)
            if (criba[i])
                for (j = i * i; j <= N; j += i) criba[j] = 0;
        n = 0;
        for (i = 2; i <= N; i++) if (criba[i]) n++;
    }
    printf("primos=%d\n", n);
    for (r = 0; r < REP * 2; r++)
        for (i = 0; i < 20000; i++) {
            s += hash(i + r) & 1023;
            tabla[i & 63] += s % 17 + (i / 8);
        }
    printf("hash=%d t=%d\n", s, tabla[5] + tabla[63]);
    for (i = 0; i < 32; i++) for (j = 0; j < 32; j++) { ma[i][j] = i + j; mb[i][j] = i - j; }
    for (r = 0; r < REP; r++) {
        for (i = 0; i < 32; i++)
            for (j = 0; j < 32; j++) {
                int a = 0;
                for (k = 0; k < 32; k++) a += ma[i][k] * mb[k][j];
                mc[i][j] = a;
            }
        s += mc[r & 31][r & 31] + mc[31][0];
    }
    printf("matriz=%d\n", s);
    printf("ms=%d\n", time_ms() - t0);
    return 0;
}
