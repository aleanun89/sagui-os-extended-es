/* matematica.c - lo de math.h que no está en la libm del MicroPython
 * (libm_dbl, musl, que da la doble precisión) para NetSurf (D184): las
 * funciones de precisión simple y algunas dobles más. */
#include "math.h"

/* round: la mitad, lejos del cero. Sin «floor(x + 0.5)», que para
 * 0.49999999999999994 da 1 (la suma redondea); el -0 se queda en -0. */
double round(double x)
{
    if (!isfinite(x)) return x;
    double r = trunc(x);
    if (fabs(x - r) >= 0.5) r += x < 0 ? -1.0 : 1.0;
    return copysign(r, x);
}
long lround(double x) { return (long)round(x); }
long lrint(double x) { return (long)rint(x); }
/* fmin/fmax: un NaN se ignora (C99). */
double fmin(double a, double b) { if (a != a) return b; if (b != b) return a; return a < b ? a : b; }
double fmax(double a, double b) { if (a != a) return b; if (b != b) return a; return a > b ? a : b; }

/* hypot con escala: sqrt(a*a + b*b) se desbordaría con 1e200. Un infinito
 * gana a un NaN (C99). */
double hypot(double x, double y)
{
    double a = fabs(x), b = fabs(y);
    if (isinf(a) || isinf(b)) return INFINITY;
    if (a != a || b != b) return a + b;
    if (a < b) { double t = a; a = b; b = t; }
    if (a == 0) return 0;
    double r = b / a;
    return a * sqrt(1 + r * r);
}

/* cbrt: la raíz cúbica (la libm del MicroPython no la trae). Empieza por pow
 * y corrige con Newton: y -= (y^3 - x) / (3 y^2). Viene del porte del
 * QuickJS (qjs_port.c, D149). */
double cbrt(double x)
{
    if (x == 0 || !isfinite(x)) return x;
    double s = x < 0 ? -1.0 : 1.0;
    double a = fabs(x);
    double y = pow(a, 1.0 / 3.0);
    for (int i = 0; i < 3; i++) {
        double y2 = y * y;
        if (y2 < 1e-300) break;
        y -= (y2 * y - a) / (3 * y2);
    }
    return s * y;
}

float sqrtf(float x) { return (float)sqrt(x); }
float fabsf(float x) { return __builtin_fabsf(x); }
float floorf(float x) { return (float)floor(x); }
float ceilf(float x) { return (float)ceil(x); }
float roundf(float x) { return (float)round(x); }
float truncf(float x) { return (float)trunc(x); }
float sinf(float x) { return (float)sin(x); }
float cosf(float x) { return (float)cos(x); }
float tanf(float x) { return (float)tan(x); }
float atanf(float x) { return (float)atan(x); }
float atan2f(float y, float x) { return (float)atan2(y, x); }
float powf(float x, float y) { return (float)pow(x, y); }
float fmodf(float x, float y) { return (float)fmod(x, y); }
float fminf(float a, float b) { return a < b ? a : b; }
float fmaxf(float a, float b) { return a > b ? a : b; }
float expf(float x) { return (float)exp(x); }
float logf(float x) { return (float)log(x); }
long lroundf(float x) { return lround(x); }
