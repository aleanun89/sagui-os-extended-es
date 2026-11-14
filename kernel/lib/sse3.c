// SPDX-License-Identifier: GPL-2.0-or-later
/* sse3.c - copias con SSE3: lddqu para las lecturas desalineadas (D208).
 *
 * En los Pentium 4 de Prescott y los Core, lddqu carga 16 bytes que cruzan una
 * línea de caché sin la penalización de movdqu. Las escrituras siguen alineadas
 * (movdqa o movntdq). Compilado con -msse3; mismas reglas que sse2.c (en el
 * kernel, solo entre fpu_kernel_inicio/fim; en los programas, solo con CPUID). */
#include "sse2.h"

typedef int v4si __attribute__((vector_size(16), may_alias));
typedef char v16qi __attribute__((vector_size(16), may_alias));
typedef long long v2di __attribute__((vector_size(16), may_alias));
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned int u32a __attribute__((may_alias, aligned(1)));

#define ALINH(p) ((u32)(p) & 15)

static inline v4si ler(const void *p) { return (v4si)__builtin_ia32_lddqu((const char *)p); }
static inline void gravar(void *p, v4si v) { *(v4si *)p = v; }
static inline void gravar_nt(void *p, v4si v) { __builtin_ia32_movntdq((v2di *)p, (v2di)v); }

static inline void pouco(u8 *d, const u8 *o, u32 n)
{
    while (n >= 4) { *(u32a *)d = *(const u32a *)o; d += 4; o += 4; n -= 4; }
    while (n--) *d++ = *o++;
}

void sse3_copiar(void *dst, const void *src, unsigned n)
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
        v4si a = ler(o), b = ler(o + 16), c = ler(o + 32), e = ler(o + 48);
        gravar(d, a); gravar(d + 16, b); gravar(d + 32, c); gravar(d + 48, e);
    }
    for (; n >= 16; n -= 16, d += 16, o += 16) gravar(d, ler(o));
    pouco(d, o, n);
}

void sse3_copiar_nt(void *dst, const void *src, unsigned n)
{
    u8 *d = dst;
    const u8 *o = src;
    if (n >= 16 && ALINH(d)) {
        u32 k = 16 - ALINH(d);
        pouco(d, o, k);
        d += k; o += k; n -= k;
    }
    for (; n >= 64; n -= 64, d += 64, o += 64) {
        __builtin_prefetch(o + 320, 0, 0);
        v4si a = ler(o), b = ler(o + 16), c = ler(o + 32), e = ler(o + 48);
        gravar_nt(d, a); gravar_nt(d + 16, b); gravar_nt(d + 32, c); gravar_nt(d + 48, e);
    }
    for (; n >= 16; n -= 16, d += 16, o += 16) gravar_nt(d, ler(o));
    __builtin_ia32_sfence();
    pouco(d, o, n);
}
