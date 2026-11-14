/* math.h mínimo para o MicroPython e a libm_dbl (D038). As classificações
 * (isnan, isinf...) usam as funções embutidas do GCC, que viram poucas
 * instruções SSE2; as funções vêm de libm_dbl/ e de libm_sagui.c. */
#ifndef SAGUI_MATH_H
#define SAGUI_MATH_H

/* FLT_EVAL_METHOD = 0 com -mfpmath=sse: as contas são no próprio tipo. */
typedef float float_t;
typedef double double_t;

#define NAN        __builtin_nan("")
#define INFINITY   __builtin_inf()
#define HUGE_VAL   __builtin_huge_val()
#define HUGE_VALF  __builtin_huge_valf()

#define FP_NAN       0
#define FP_INFINITE  1
#define FP_ZERO      2
#define FP_SUBNORMAL 3
#define FP_NORMAL    4

#define fpclassify(x) __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x)
#define isnan(x)      __builtin_isnan(x)
#define isinf(x)      __builtin_isinf(x)
#define isfinite(x)   __builtin_isfinite(x)
#define isnormal(x)   __builtin_isnormal(x)
#define signbit(x)    __builtin_signbit(x)

#define M_E        2.7182818284590452354
#define M_LOG2E    1.4426950408889634074
#define M_LOG10E   0.43429448190325182765
#define M_LN2      0.69314718055994530942
#define M_LN10     2.30258509299404568402
#define M_PI       3.14159265358979323846
#define M_PI_2     1.57079632679489661923
#define M_PI_4     0.78539816339744830962
#define M_SQRT2    1.41421356237309504880

double sqrt(double);
double fabs(double);
double nan(const char *);
double floor(double), ceil(double), trunc(double), rint(double), nearbyint(double);
double fmod(double, double), modf(double, double *), copysign(double, double);
double frexp(double, int *), ldexp(double, int), scalbn(double, int);
double sin(double), cos(double), tan(double);
double asin(double), acos(double), atan(double), atan2(double, double);
double sinh(double), cosh(double), tanh(double);
double asinh(double), acosh(double), atanh(double);
double exp(double), expm1(double), log(double), log1p(double), log2(double), log10(double);
double pow(double, double);
double erf(double), erfc(double), tgamma(double), lgamma(double);
#endif
