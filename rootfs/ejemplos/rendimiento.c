// SPDX-License-Identifier: GPL-2.0-or-later
/* rendimiento.c - mide el mismo calculo (Mandelbrot 79x23, sin dibujar)
 * con double, con float y con enteros en punto fijo (12 bits de fraccion).
 * Compila y ejecuta con:
 *     cc /ejemplos/rendimiento.c -o /tmp/rendimiento
 *     /tmp/rendimiento [repeticiones]
 */
#include <stdio.h>
#include <stdlib.h>

#if defined(__SAGUIOS__) && !defined(__LINUX__)
#define AHORA() time_ms()
#elif defined(__SAGUIOS__)
/* cc -linux (pruebas en el ordenador de desarrollo): gettimeofday */
int AHORA(void) { int tv[2]; __syscall(78, (int)tv, 0, 0); return tv[0] % 100000 * 1000 + tv[1] / 1000; }
#else
#include <time.h>
#define AHORA() ((int)(clock() / (CLOCKS_PER_SEC / 1000)))
#endif

#define ANCHO 79
#define ALTO 23
#define MAX 200

int con_double(void)
{
    int total = 0;
    for (int fila = 0; fila < ALTO; fila++) {
        double ci = 1.2 - 2.4 * fila / (ALTO - 1);
        for (int col = 0; col < ANCHO; col++) {
            double cr = -2.1 + 2.7 * col / (ANCHO - 1);
            double zr = 0, zi = 0;
            int n = 0;
            while (n < MAX && zr * zr + zi * zi <= 4.0) {
                double t = zr * zr - zi * zi + cr;
                zi = 2 * zr * zi + ci;
                zr = t;
                n++;
            }
            total += n;
        }
    }
    return total;
}

int con_float(void)
{
    int total = 0;
    for (int fila = 0; fila < ALTO; fila++) {
        float ci = 1.2f - 2.4f * fila / (ALTO - 1);
        for (int col = 0; col < ANCHO; col++) {
            float cr = -2.1f + 2.7f * col / (ANCHO - 1);
            float zr = 0, zi = 0;
            int n = 0;
            while (n < MAX && zr * zr + zi * zi <= 4.0f) {
                float t = zr * zr - zi * zi + cr;
                zi = 2 * zr * zi + ci;
                zr = t;
                n++;
            }
            total += n;
        }
    }
    return total;
}

/* Punto fijo: x vale x/4096. Con |z| <= 2 antes de cada paso, los
 * productos caben en 32 bits. */
#define UNO 4096
int con_enteros(void)
{
    int total = 0;
    for (int fila = 0; fila < ALTO; fila++) {
        int ci = (12 * UNO) / 10 - (24 * UNO / 10) * fila / (ALTO - 1);
        for (int col = 0; col < ANCHO; col++) {
            int cr = -(21 * UNO) / 10 + (27 * UNO / 10) * col / (ANCHO - 1);
            int zr = 0, zi = 0;
            int n = 0;
            while (n < MAX) {
                int zr2 = (zr * zr) >> 12, zi2 = (zi * zi) >> 12;
                if (zr2 + zi2 > 4 * UNO) break;
                zi = ((2 * zr * zi) >> 12) + ci;
                zr = zr2 - zi2 + cr;
                n++;
            }
            total += n;
        }
    }
    return total;
}

int main(int argc, char **argv)
{
    int rep = argc > 1 ? atoi(argv[1]) : 3;
    int t0 = AHORA(), a = 0, b = 0, c = 0;
    for (int i = 0; i < rep; i++) a = con_double();
    int t1 = AHORA();
    for (int i = 0; i < rep; i++) b = con_float();
    int t2 = AHORA();
    for (int i = 0; i < rep; i++) c = con_enteros();
    int t3 = AHORA();
    printf("double:      %d iteraciones, %d ms\n", a, t1 - t0);
    printf("float:       %d iteraciones, %d ms\n", b, t2 - t1);
    printf("punto fijo:  %d iteraciones, %d ms\n", c, t3 - t2);
    return 0;
}
