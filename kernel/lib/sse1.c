// SPDX-License-Identifier: GPL-2.0-or-later
/* sse1.c - copias y rellenos con SSE solo, sin SSE2 (D208).
 *
 * Para el Pentium III, el Athlon XP y afines: tienen SSE (movups, movaps,
 * movntps, prefetch) pero no SSE2, así que no hay instrucciones enteras de 128
 * bits y todo pasa por registros XMM tratados como cuatro floats. Los movimientos de
 * memoria no interpretan los bits: se copian enteros arbitrarios sin tocarlos.
 *
 * Mismas reglas que sse2.c: compilado con -msse -mno-sse2 (regla en el
 * Makefile), en el kernel solo corre entre fpu_kernel_inicio() y
 * fpu_kernel_fim(), y en los programas solo si CPUID dice que hay SSE.
 * Con prefetch hacia delante en las copias grandes (prefetcht0 / prefetchnta). */
#include "sse2.h"

typedef float v4sf __attribute__((vector_size(16), may_alias));
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned int u32a __attribute__((may_alias, aligned(1)));

#define ALINH(p) ((u32)(p) & 15)

static inline v4sf ler(const void *p) { return __builtin_ia32_loadups((const float *)p); }   /* movups explícito: sin él GCC parte la carga en movlps+movhps */
static inline void gravar(void *p, v4sf v) { *(v4sf *)p = v; }
static inline void gravar_nt(void *p, v4sf v) { __builtin_ia32_movntps((float *)p, v); }

static inline void pouco(u8 *d, const u8 *o, u32 n)
{
    while (n >= 4) { *(u32a *)d = *(const u32a *)o; d += 4; o += 4; n -= 4; }
    while (n--) *d++ = *o++;
}

/* Cuatro copias de v en un registro. Se carga con movups: la pila del kernel no está alineada a
 * 16 bytes y un movaps desde un temporal de la pila daría #GP. */
static inline v4sf quatro(u32 v)
{
    u32 t[4] = { v, v, v, v };
    return __builtin_ia32_loadups((const float *)t);
}

void sse1_copiar(void *dst, const void *src, unsigned n)
{
    u8 *d = dst;
    const u8 *o = src;
    if (n >= 16 && ALINH(d)) {
        u32 k = 16 - ALINH(d);
        pouco(d, o, k);
        d += k; o += k; n -= k;
    }
    for (; n >= 64; n -= 64, d += 64, o += 64) {
        __builtin_prefetch(o + 320, 0, 3);
        v4sf a = ler(o), b = ler(o + 16), c = ler(o + 32), e = ler(o + 48);
        gravar(d, a); gravar(d + 16, b); gravar(d + 32, c); gravar(d + 48, e);
    }
    for (; n >= 16; n -= 16, d += 16, o += 16) gravar(d, ler(o));
    pouco(d, o, n);
}

void sse1_copiar_nt(void *dst, const void *src, unsigned n)
{
    u8 *d = dst;
    const u8 *o = src;
    if (n >= 16 && ALINH(d)) {
        u32 k = 16 - ALINH(d);
        pouco(d, o, k);
        d += k; o += k; n -= k;
    }
    for (; n >= 64; n -= 64, d += 64, o += 64) {
        __builtin_prefetch(o + 320, 0, 0);   /* prefetchnta: no ensucia la caché con lo que se lee una vez */
        v4sf a = ler(o), b = ler(o + 16), c = ler(o + 32), e = ler(o + 48);
        gravar_nt(d, a); gravar_nt(d + 16, b); gravar_nt(d + 32, c); gravar_nt(d + 48, e);
    }
    for (; n >= 16; n -= 16, d += 16, o += 16) gravar_nt(d, ler(o));
    __builtin_ia32_sfence();
    pouco(d, o, n);
}

void sse1_copiar_tras(void *dst, const void *src, unsigned n)
{
    u8 *d = (u8 *)dst + n;
    const u8 *o = (const u8 *)src + n;
    while (n && ALINH(d)) { *--d = *--o; n--; }
    for (; n >= 64; n -= 64) {
        d -= 64; o -= 64;
        v4sf e = ler(o + 48), c = ler(o + 32), b = ler(o + 16), a = ler(o);
        gravar(d + 48, e); gravar(d + 32, c); gravar(d + 16, b); gravar(d, a);
    }
    for (; n >= 16; n -= 16) { d -= 16; o -= 16; gravar(d, ler(o)); }
    while (n--) *--d = *--o;
}

void sse1_memset(void *dst, int v, unsigned n)
{
    u8 *d = dst;
    u8 b = (u8)v;
    while (n && ALINH(d)) { *d++ = b; n--; }
    v4sf x = quatro(b * 0x01010101u);
    for (; n >= 64; n -= 64, d += 64) { gravar(d, x); gravar(d + 16, x); gravar(d + 32, x); gravar(d + 48, x); }
    for (; n >= 16; n -= 16, d += 16) gravar(d, x);
    while (n--) *d++ = b;
}

void sse1_preencher32(unsigned *d, unsigned v, unsigned n)
{
    while (n && ALINH(d)) { *d++ = v; n--; }
    v4sf x = quatro(v);
    for (; n >= 16; n -= 16, d += 16) { gravar(d, x); gravar(d + 4, x); gravar(d + 8, x); gravar(d + 12, x); }
    for (; n >= 4; n -= 4, d += 4) gravar(d, x);
    while (n--) *d++ = v;
}

void sse1_ampliar2x(unsigned *d, unsigned *d2, const unsigned *o, unsigned n)
{
    /* a b c e -> a a b b | c c e e con unpcklps/unpckhps del registro consigo mismo. */
    if (!ALINH(d) && (!d2 || !ALINH(d2))) {
        for (; n >= 4; n -= 4, d += 8, o += 4) {
            v4sf p = ler(o);
            v4sf b = __builtin_ia32_unpcklps(p, p), a = __builtin_ia32_unpckhps(p, p);
            gravar(d, b);
            gravar(d + 4, a);
            if (d2) { gravar(d2, b); gravar(d2 + 4, a); d2 += 8; }
        }
    }
    for (; n; n--, o++) {
        d[0] = d[1] = *o;
        d += 2;
        if (d2) { d2[0] = d2[1] = *o; d2 += 2; }
    }
}
