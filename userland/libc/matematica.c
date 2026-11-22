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

/* D458 (FFmpeg): lo que faltaba de C99 para que un port con libm completa no use sus sustitutos. Son débiles: algunos ports
 * (mGBA...) traen la suya y esa gana en el enlace. */
#define FRACA __attribute__((weak))
FRACA double exp2(double x) { return pow(2.0, x); }
FRACA float exp2f(float x) { return (float)exp2(x); }
FRACA float log2f(float x) { return (float)log2(x); }
FRACA float log10f(float x) { return (float)log10(x); }
FRACA float cbrtf(float x) { return (float)cbrt(x); }
FRACA float asinf(float x) { return (float)asin(x); }
FRACA float acosf(float x) { return (float)acos(x); }
FRACA long lrintf(float x) { return (long)rint(x); }
FRACA long long llrint(double x) { return (long long)rint(x); }
FRACA long long llrintf(float x) { return (long long)rint(x); }
FRACA long long llround(double x) { return (long long)round(x); }
FRACA long long llroundf(float x) { return (long long)round(x); }

/* j1: función de Bessel de primera especie y orden 1 (la usa libswscale para su filtro Kaiser). Serie de potencias
 * hasta |x| = 12 (error ~1e-13) y desarrollo asintótico de Hankel más allá (error ~1e-9): suficiente para
 * calcular ventanas de filtros; no es la j1 de precisión completa de la libm de musl. */
FRACA double j1(double x)
{
    double a = fabs(x), r;
    if (a < 12.0) {
        double t = a / 2.0, q = t * t, s = t;   /* término k: (-1)^k (x/2)^(2k+1) / (k! (k+1)!) */
        for (int k = 1; k < 60; k++) {
            t *= -q / ((double)k * (k + 1));
            s += t;
            if (fabs(t) < 1e-18 * fabs(s)) break;
        }
        r = s;
    } else {
        /* J1(x) ~ sqrt(2/(pi x)) (P cos(x - 3pi/4) - Q sin(x - 3pi/4)) */
        double mu = 4.0, p = 1.0, qq = 0.0, t = 1.0;
        for (int k = 1; k < 12; k++) {
            t *= (mu - (2.0 * k - 1) * (2.0 * k - 1)) / (k * 8.0 * a);
            if (fabs(t) < 1e-17) break;
            if (k & 1) qq += (((k - 1) / 2) & 1) ? -t : t;
            else       p  += ((k / 2) & 1) ? -t : t;
        }
        double w = a - 2.356194490192345;   /* x - 3 pi / 4 */
        r = sqrt(0.6366197723675814 / a) * (p * cos(w) - qq * sin(w));
    }
    return x < 0 ? -r : r;
}

/* sincos: seno y coseno juntos (gcc convierte un sin y un cos del mismo argumento en esta llamada). */
FRACA void sincos(double x, double *s, double *c) { *s = sin(x); *c = cos(x); }
FRACA void sincosf(float x, float *s, float *c) { *s = (float)sin(x); *c = (float)cos(x); }
