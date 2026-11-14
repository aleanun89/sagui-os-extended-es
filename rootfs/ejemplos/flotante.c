// SPDX-License-Identifier: GPL-2.0-or-later
/* flotante.c - numeros con coma flotante (float y double) en el cc.
 * Compila y ejecuta con:
 *     cc /ejemplos/flotante.c -o /tmp/flotante
 *     /tmp/flotante
 */
#include <stdio.h>
#include <math.h>

struct planeta { char *nombre; double radio_km; double masa_kg; };

struct planeta planetas[] = {
    {"Mercurio", 2439.7, 3.3011e23},
    {"Tierra", 6371.0, 5.9722e24},
    {"Marte", 3389.5, 6.4171e23},
    {"Jupiter", 69911, 1.8982e27},
};

/* Raiz cuadrada por el metodo de Newton. */
double raiz(double x)
{
    double r = x > 1 ? x / 2 : 1;
    for (int i = 0; i < 30; i++) r = (r + x / r) / 2;
    return r;
}

int main(void)
{
    /* pi con dos series */
    double leibniz = 0, signo = 1;
    for (int k = 0; k < 100000; k++) {
        leibniz += signo / (2 * k + 1);
        signo = -signo;
    }
    leibniz *= 4;
    double nilakantha = 3;
    signo = 1;
    for (int k = 2; k <= 400; k += 2) {
        nilakantha += signo * 4 / ((double)k * (k + 1) * (k + 2));
        signo = -signo;
    }
    printf("pi (Leibniz, 100000 terminos) = %.10f\n", leibniz);
    printf("pi (Nilakantha, 200 terminos) = %.10f\n", nilakantha);
    printf("pi (M_PI)                     = %.10f\n", M_PI);
    printf("pi (4*atan(1))                = %.15g\n", 4 * atan(1));

    printf("\nRaices: Newton y sqrt()\n");
    for (int n = 2; n <= 10; n += 2)
        printf("  raiz(%2d) = %.12f   sqrt(%2d) = %.12f\n", n, raiz(n), n, sqrt(n));

    printf("\nTrigonometria (grados)\n");
    printf("  %6s %10s %10s %10s\n", "ang", "sin", "cos", "tan");
    for (int g = 0; g <= 90; g += 15) {
        double r = g * M_PI / 180;
        if (g == 90) printf("  %6d %10.6f %10.6f %10s\n", g, sin(r), cos(r), "inf");
        else printf("  %6d %10.6f %10.6f %10.6f\n", g, sin(r), cos(r), tan(r));
    }

    printf("\nPlanetas: gravedad en la superficie\n");
    double G = 6.674e-11;
    for (int i = 0; i < 4; i++) {
        struct planeta *p = &planetas[i];
        double r = p->radio_km * 1000;
        double gs = G * p->masa_kg / (r * r);
        printf("  %-9s g = %6.2f m/s2  masa = %.3e kg\n", p->nombre, gs, p->masa_kg);
    }

    printf("\nfloat frente a double: 0.1 sumado 1000 veces\n");
    float f = 0;
    double d = 0;
    for (int i = 0; i < 1000; i++) { f += 0.1f; d += 0.1; }
    printf("  float  = %.10f\n  double = %.10f\n", f, d);
    printf("\nexp(1) = %.12g  log(10) = %.12g  pow(2, 0.5) = %.12g\n", exp(1), log(10), pow(2, 0.5));
    printf("%%e: %e  %%g: %g  %%g: %g\n", 6.02214076e23, 0.0001234, 1e-5);
    return 0;
}
