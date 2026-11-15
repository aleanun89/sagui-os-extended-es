// SPDX-License-Identifier: GPL-3.0-or-later
// GebraMini, © 2026 Alejandro Anuncibay: prototipo original (C++/SDL) del mantenedor; esta versión nativa
// para SaguiOS está escrita desde cero a partir de sus ideas. Independiente de GeoGebra.
/* gbm_mat.h - funciones matemáticas de GebraMini sobre la FPU x87 (D309).
 *
 * El programa no usa libm: cada función es una o dos instrucciones x87 (fsin, fpatan, fyl2x,
 * f2xm1...) con la reducción de argumentos que hace falta. Sirve igual en SaguiOS (donde se compila
 * con -mfpmath=387) y en el anfitrión x86-64 (las pruebas de unidad comparan con la libm). */
#ifndef GBM_MAT_H
#define GBM_MAT_H

#define GM_PI   3.14159265358979323846
#define GM_E    2.71828182845904523536
#define GM_NAN  (__builtin_nan(""))
#define GM_INF  (__builtin_inf())

static inline int gm_nan(double x) { return x != x; }
static inline int gm_fin(double x) { return x == x && x - x == 0.0; }
static inline double gm_abs(double x) { return x < 0 ? -x : x; }

static inline double gm_sqrt(double x)
{
    if (x < 0) return GM_NAN;
    double r;
    __asm__("fsqrt" : "=t"(r) : "0"(x));
    return r;
}

/* Resto de la división (con el signo de x), como fmod. */
static inline double gm_fmod(double x, double y)
{
    if (y == 0.0 || !gm_fin(x) || gm_nan(y)) return GM_NAN;
    double r;
    __asm__("1: fprem; fnstsw %%ax; sahf; jp 1b" : "=t"(r) : "0"(x), "u"(y) : "ax", "cc");
    return r;
}

static inline double gm_floor(double x)
{
    if (!gm_fin(x) || gm_abs(x) >= 4.0e15) return x;
    double t = (double)(long long)x;
    return t > x ? t - 1.0 : t;
}
static inline double gm_ceil(double x) { return -gm_floor(-x); }
static inline double gm_round(double x) { return gm_floor(x + 0.5); }

/* Argumento reducido a [-2 pi, 2 pi] para que fsin/fcos no pierdan precisión con valores grandes. */
static inline double gm_red(double x)
{
    if (gm_abs(x) > 6.5) return gm_fmod(x, 2.0 * GM_PI);
    return x;
}
static inline double gm_sin(double x)
{
    if (!gm_fin(x)) return GM_NAN;
    double r, a = gm_red(x);
    __asm__("fsin" : "=t"(r) : "0"(a));
    return r;
}
static inline double gm_cos(double x)
{
    if (!gm_fin(x)) return GM_NAN;
    double r, a = gm_red(x);
    __asm__("fcos" : "=t"(r) : "0"(a));
    return r;
}
static inline double gm_tan(double x)
{
    if (!gm_fin(x)) return GM_NAN;
    double uno, r, a = gm_red(x);
    __asm__("fptan" : "=t"(uno), "=u"(r) : "0"(a));
    (void)uno;
    return r;
}
/* atan2(y, x) con la convención de siempre. */
static inline double gm_atan2(double y, double x)
{
    double r;
    __asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
    return r;
}
static inline double gm_atan(double x) { return gm_atan2(x, 1.0); }
static inline double gm_asin(double x)
{
    if (x < -1.0 || x > 1.0) return GM_NAN;
    return gm_atan2(x, gm_sqrt((1.0 - x) * (1.0 + x)));
}
static inline double gm_acos(double x)
{
    if (x < -1.0 || x > 1.0) return GM_NAN;
    return gm_atan2(gm_sqrt((1.0 - x) * (1.0 + x)), x);
}

/* 2^t */
static inline double gm_exp2(double t)
{
    if (gm_nan(t)) return t;
    if (t > 1023.9) return GM_INF;
    if (t < -1074.0) return 0.0;
    double n, f, m, r;
    __asm__("frndint" : "=t"(n) : "0"(t));
    f = t - n;
    __asm__("f2xm1" : "=t"(m) : "0"(f));
    m += 1.0;
    __asm__("fscale" : "=t"(r) : "0"(m), "u"(n));
    return r;
}
static inline double gm_exp(double x) { return gm_exp2(x * 1.44269504088896340736); }
static inline double gm_log2(double x)
{
    if (gm_nan(x) || x < 0) return GM_NAN;
    if (x == 0) return -GM_INF;
    double r;
    __asm__("fld1; fxch; fyl2x" : "=t"(r) : "0"(x) : "st(1)");
    return r;
}
static inline double gm_ln(double x) { return gm_log2(x) * 0.69314718055994530942; }
static inline double gm_log10(double x) { return gm_log2(x) * 0.30102999566398119521; }

static inline double gm_pow(double x, double y)
{
    if (y == 0.0) return 1.0;
    if (gm_nan(x) || gm_nan(y)) return GM_NAN;
    if (gm_abs(y) <= 1024.0 && y == gm_floor(y)) {      /* exponente entero: también vale con base negativa */
        long n = (long)gm_abs(y);
        double b = x, r = 1.0;
        while (n) { if (n & 1) r *= b; b *= b; n >>= 1; }
        return y < 0 ? 1.0 / r : r;
    }
    if (x == 0.0) return y > 0 ? 0.0 : GM_INF;
    if (x < 0) return GM_NAN;
    double t;
    double l = gm_log2(x);
    t = y * l;
    return gm_exp2(t);
}
static inline double gm_cbrt(double x)
{
    if (x == 0.0) return 0.0;
    double r = gm_pow(gm_abs(x), 1.0 / 3.0);
    return x < 0 ? -r : r;
}
static inline double gm_sinh(double x) { double e = gm_exp(x); return (e - 1.0 / e) * 0.5; }
static inline double gm_cosh(double x) { double e = gm_exp(x); return (e + 1.0 / e) * 0.5; }
static inline double gm_tanh(double x)
{
    if (x > 20.0) return 1.0;
    if (x < -20.0) return -1.0;
    double e = gm_exp(2.0 * x);
    return (e - 1.0) / (e + 1.0);
}
static inline double gm_hypot(double a, double b) { return gm_sqrt(a * a + b * b); }

#endif
