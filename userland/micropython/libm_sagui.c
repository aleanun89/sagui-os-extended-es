/* libm_sagui.c - las funciones de math.h que no vienen de libm_dbl (D038).
 *
 * La raíz cuadrada usa la instrucción SQRTSD de SSE2 (FSQRT na variante x87,
 * D082), que ya da el resultado redondeado correctamente (IEEE 754) y es
 * mucho más rápida que el método bit a bit de sqrt.c de FreeBSD. */
#include <math.h>
#include <stdint.h>

double sqrt(double x)
{
    double r;
#ifdef SAGUI_X87
    /* Variante sin SSE2 (D082): FSQRT. Con la precisión de x87 en 53 bits
     * (inicio_python.c) el resultado es el mismo que SQRTSD. */
    __asm__("fsqrt" : "=t"(r) : "0"(x));
#else
    __asm__("sqrtsd %1, %0" : "=x"(r) : "x"(x));
#endif
    return r;
}

double fabs(double x)
{
    return __builtin_fabs(x);   /* un ANDPD que borra el bit de signo */
}

double copysign(double x, double y)
{
    return __builtin_copysign(x, y);   /* signo de y, módulo de x */
}

double nan(const char *s)
{
    (void)s;
    return NAN;
}

/* log2(x) = e + log(m) / ln 2, con x = m * 2^e y m en [0.5, 1). Separar el
 * exponente deja las potencias de 2 exactas: log2(8) da 3.0 y no
 * 2.9999999999999996. */
double log2(double x)
{
    if (!(x > 0) || isinf(x)) return log(x) / M_LN2;   /* 0, negativo, inf y nan */
    int e;
    double m = frexp(x, &e);
    if (m == 0.5) return e - 1;
    return e + log(m) / M_LN2;
}
