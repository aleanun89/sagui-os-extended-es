/* math.h - funciones matemáticas (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_MATH_H
#define NS_MATH_H

typedef float float_t;
typedef double double_t;
#define NAN        __builtin_nanf("")
#define INFINITY   __builtin_inff()
#define HUGE_VAL   __builtin_huge_val()
#define HUGE_VALF  __builtin_huge_valf()
#define M_E        2.7182818284590452354
#define M_LOG2E    1.4426950408889634074
#define M_LOG10E   0.43429448190325182765
#define M_LN2      0.69314718055994530942
#define M_LN10     2.30258509299404568402
#define M_PI       3.14159265358979323846
#define M_PI_2     1.57079632679489661923
#define M_PI_4     0.78539816339744830962
#define M_SQRT2    1.41421356237309504880
#define isnan(x)      __builtin_isnan(x)
#define isinf(x)      __builtin_isinf(x)
#define isfinite(x)   __builtin_isfinite(x)
#define isnormal(x)   __builtin_isnormal(x)
#define signbit(x)    __builtin_signbit(x)
#define fpclassify(x) __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x)
#define FP_NAN       0
#define FP_INFINITE  1
#define FP_ZERO      2
#define FP_SUBNORMAL 3
#define FP_NORMAL    4
/* Doble precisión: la libm del MicroPython (musl, variante x87). */
double sqrt(double);
double fabs(double);
double floor(double), ceil(double), trunc(double), rint(double), nearbyint(double), round(double);
double fmod(double, double), modf(double, double *), copysign(double, double);
double frexp(double, int *), ldexp(double, int), scalbn(double, int);
double sin(double), cos(double), tan(double);
double asin(double), acos(double), atan(double), atan2(double, double);
double sinh(double), cosh(double), tanh(double);
double asinh(double), acosh(double), atanh(double);
double exp(double), expm1(double), log(double), log1p(double), log2(double), log10(double);
double pow(double, double);
double erf(double), erfc(double), tgamma(double), lgamma(double);
double nan(const char *);
double fmin(double, double), fmax(double, double), hypot(double, double), cbrt(double);
long   lround(double);
long   lrint(double);
/* Precisión simple: libc/matematica.c, a partir de las dobles. */
float sqrtf(float), fabsf(float), floorf(float), ceilf(float), roundf(float), truncf(float);
float sinf(float), cosf(float), tanf(float), atanf(float), atan2f(float, float);
float powf(float, float), fmodf(float, float), fminf(float, float), fmaxf(float, float);
float expf(float), logf(float);
long  lroundf(float);
#endif
