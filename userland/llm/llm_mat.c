// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_mat.c - las funciones matemáticas del motor (D380).
 *
 * En SaguiOS no hay libm en la libsagui: se usan las instrucciones de la x87 (fsqrt,
 * fsin, fcos, f2xm1, fyl2x, fscale), que tienen todas las CPU desde el 486. En el PC
 * (LLM_ANFITRIAO) se usa la libm, para comparar con llama.cpp. */
#include "llm.h"

#if defined(LLM_ANFITRIAO) && !defined(LLM_MAT_X87)   /* LLM_MAT_X87: la x87 también en el PC */
#include <math.h>
float llm_expf(float x) { return expf(x); }
float llm_sqrtf(float x) { return sqrtf(x); }
float llm_logf(float x) { return logf(x); }
float llm_powf(float b, float e) { return powf(b, e); }
void  llm_sincosf(float x, float *s, float *c) { *s = sinf(x); *c = cosf(x); }
float llm_fabsf(float x) { return fabsf(x); }
#else

float llm_fabsf(float x) { return x < 0 ? -x : x; }

float llm_sqrtf(float x)
{
    long double r;
    __asm__("fsqrt" : "=t"(r) : "0"((long double)x));
    return (float)r;
}

/* 2^x = 2^int * 2^frac, con f2xm1 para la parte en [-1, 1]. */
static long double exp2l_x87(long double x)
{
    long double i, f, r;
    __asm__("frndint" : "=t"(i) : "0"(x));
    f = x - i;
    __asm__("f2xm1" : "=t"(r) : "0"(f));
    r += 1.0L;
    __asm__("fscale" : "=t"(r) : "0"(r), "u"(i));
    return r;
}

float llm_expf(float x)
{
    if (x < -104.0f) return 0.0f;
    if (x > 88.7f) return 3.4e38f;
    return (float)exp2l_x87((long double)x * 1.44269504088896340736L);
}

static long double log2l_x87(long double x)
{
    long double r;
    __asm__("fyl2x" : "=t"(r) : "0"(x), "u"(1.0L) : "st(1)");
    return r;
}

float llm_logf(float x)
{
    if (x <= 0) return -3.4e38f;
    return (float)(log2l_x87(x) * 0.69314718055994530942L);
}

float llm_powf(float b, float e)
{
    if (b <= 0) return 0.0f;
    return (float)exp2l_x87((long double)e * log2l_x87(b));
}

/* fsin y fcos valen para |x| < 2^63; los ángulos de RoPE son mucho menores. */
void llm_sincosf(float x, float *s, float *c)
{
    long double sn, cs;
    __asm__("fsincos" : "=t"(cs), "=u"(sn) : "0"((long double)x));
    *s = (float)sn;
    *c = (float)cs;
}
#endif
