// SPDX-License-Identifier: GPL-2.0-or-later
/* mandelbrot.c - el conjunto de Mandelbrot en ASCII, con double.
 * Compila y ejecuta con:
 *     cc /ejemplos/mandelbrot.c -o /tmp/mandelbrot
 *     /tmp/mandelbrot            (200 iteraciones por punto)
 *     /tmp/mandelbrot 1000
 */
#include <stdio.h>
#include <stdlib.h>

#define ANCHO 79
#define ALTO 23

int main(int argc, char **argv)
{
    int max = argc > 1 ? atoi(argv[1]) : 200;
    const char *paleta = " .,:;-=+*#%";
    long total = 0;
    for (int fila = 0; fila < ALTO; fila++) {
        char linea[ANCHO + 1];
        double ci = 1.2 - 2.4 * fila / (ALTO - 1);
        for (int col = 0; col < ANCHO; col++) {
            double cr = -2.1 + 2.7 * col / (ANCHO - 1);
            double zr = 0, zi = 0;
            int n = 0;
            while (n < max && zr * zr + zi * zi <= 4.0) {
                double t = zr * zr - zi * zi + cr;
                zi = 2 * zr * zi + ci;
                zr = t;
                n++;
            }
            total += n;
            linea[col] = n == max ? '@' : paleta[n % 11];
        }
        linea[ANCHO] = 0;
        puts(linea);
    }
    printf("Mandelbrot %dx%d, %d iteraciones como maximo, %ld en total\n", ANCHO, ALTO, max, total);
    return 0;
}
