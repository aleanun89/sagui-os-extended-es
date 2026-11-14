// SPDX-License-Identifier: GPL-2.0-or-later
/* cc_flotante.c - prueba de float/double del cc (tests/b7_cc_flotante.py).
 * La salida debe ser igual a la del mismo programa compilado con GCC. */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

typedef struct { double x, y; } vec;
struct cuerpo { char nombre[8]; double masa; float carga; int n; };
typedef double (*fn)(double);

double garr[3] = {1, 2.5, -3e2}, gsuma = 1.0 / 4 + 2 * 3 - 0.5;
float gf[] = {1.5f, -2, 3e-1f};
vec gv = {1.5, -2.25};

double media(int n, ...)
{
    va_list ap;
    va_start(ap, n);
    double s = 0;
    for (int i = 0; i < n; i++) s += va_arg(ap, double);
    va_end(ap);
    return s / n;
}
double cuadrado(double x) { return x * x; }
float mitad(float x) { return x / 2; }
double aplica(fn f, double x) { return f(x); }
double mezcla(char c, float f, double d, short s, int i) { return c + f + d + s + i; }
double norma(vec *v) { return sqrt(v->x * v->x + v->y * v->y); }
double fact(int n) { return n <= 1 ? 1 : n * fact(n - 1); }
double profundo(double a, double b, double c, double d)
{
    return a - (b - (c - (d - (a / (b + cuadrado(c - (d * (a - b))))))));
}
double sin_return(double x) { if (x > 0) return x; return -1; }

int main(void)
{
    /* aritmetica y conversiones */
    double a = 1.5, b = -2.25;
    int i = 7;
    unsigned u = 3000000000u;
    printf("A %g %g %g %g\n", a + b, a - b, a * b, a / b);
    printf("B %d %d %d %u %d\n", (int)2.99, (int)-2.99, (int)(a * 10), (unsigned)4e9, (int)(i / 2.0 * 10));
    printf("C %.1f %.1f %g\n", (double)u, (double)i, (double)'A');
    double x = 5;
    x++; ++x; x--; x *= 3; x -= 1; x /= 2;
    i += 2.9;
    printf("D %g %d\n", x, i);
    /* comparaciones y NaN */
    double nan = NAN, inf = INFINITY;
    printf("E %d%d%d%d%d%d %d%d%d %d%d\n", nan < 1, nan <= 1, nan > 1, nan >= 1, nan == nan, nan != nan,
           !nan, !0.0, 0.0 || nan, a < b, a > b);
    printf("F %f %e %g %d %d\n", inf, -inf, nan, isnan(nan), isinf(-inf) != 0);
    /* printf */
    printf("G [%f] [%.0f] [%.3f] [%10.2f] [%-10.1f] [%+.2f] [%08.3f]\n", M_PI, 2.5, 1.0005, -3.14159, 2.25, 1.0, -3.14159);
    printf("H [%e] [%.2e] [%E] [%g] [%g] [%g] [%.3g] [%#g]\n", 12345.678, 0.000123, 1e100, 100000.0, 1e6, 1e-5, 2.0 / 3, 1.0);
    char buf[64];
    snprintf(buf, sizeof buf, "%.4f|%8.3e|%G", M_E, 6.02e23, 1e-10);
    printf("I %s\n", buf);
    printf("J %.17g %.17g %.15g\n", 0.1, 1.0 / 3, 2.675);
    /* math.h */
    printf("K %.10f %.10f %.10f %.10f\n", sqrt(2), sin(M_PI / 6), cos(M_PI / 3), tan(M_PI / 4));
    printf("L %.10f %.10f %.10f %.10f\n", atan2(1, 1), atan2(-1, -2), asin(0.5), acos(-0.5));
    printf("M %.10f %.10f %.10f %.10f\n", exp(1), log(10), log10(2000), pow(2, 0.5));
    printf("N %g %g %g %g %g %g\n", pow(2, 10), pow(-2, 3), pow(10, -2), floor(-2.5), ceil(-2.5), round(2.5));
    printf("O %g %g %g %g %g\n", trunc(-2.7), fmod(7.5, 2), fabs(-3.25), round(-0.5), fmod(-7.5, 2));
    printf("P %g %g %g %g\n", atof("3.25"), strtod("-1.5e3x", 0), atof("  .5"), atof("1e-3"));
    /* structs, vectores, punteros */
    struct cuerpo cs[3];
    for (int k = 0; k < 3; k++) {
        cs[k].masa = k * 1.5 + 0.25;
        cs[k].carga = k * 0.5f;
        cs[k].n = k;
    }
    struct cuerpo *p = &cs[2];
    p->masa *= 2;
    double total = 0;
    for (int k = 0; k < 3; k++) total += cs[k].masa * cs[k].n + cs[k].carga;
    printf("Q %g %g %g\n", total, p->masa, cs[1].carga);
    float fv[5];
    float fs = 0;
    for (int k = 0; k < 5; k++) { fv[k] = k * 1.25f; fs += fv[k]; }
    double *pd = garr;
    pd++;
    printf("R %g %g %g %g %g\n", fs, fv[3], *pd, pd[1], gsuma);
    printf("S %g %g %g %g\n", gf[0], gf[1], gv.x, gv.y);
    vec v = {3, 4}, w;
    w = v;
    w.x *= 2;
    printf("T %g %g %g\n", norma(&v), w.x, w.y);
    /* funciones */
    fn f = cuadrado;
    printf("U %g %g %g %g\n", f(3), aplica(sqrt, 16), mitad(5), mezcla(1, 2.5f, 3.25, -4, 10));
    printf("V %g %g %.0f\n", media(3, 1.0, 2.0, 4.5), media(2, 1.5f, 2.5), fact(20));
    printf("W %.12g %g\n", profundo(1.5, 2.25, -3.5, 0.75), sin_return(-2));
    double t = i > 3 ? 1 : 2.5;
    double s = 0;
    for (double y = 0; y < 1; y += 0.125) s += y;
    printf("X %g %g\n", t, s);
    printf("Y %d %d %d\n", (int)sizeof(double), (int)sizeof(float), (int)sizeof 1.0f);
    return 0;
}
