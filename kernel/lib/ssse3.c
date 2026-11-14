// SPDX-License-Identifier: GPL-2.0-or-later
/* ssse3.c - reordenar canales de píxeles con pshufb (D208).
 *
 * pshufb baraja los 16 bytes de un registro según una máscara: cambiar el
 * orden de los canales (BGRA <-> RGBA) de cuatro píxeles es una sola instrucción,
 * donde SSE2 necesita dos desplazamientos, dos máscaras y dos por. Compilado con
 * -mssse3; mismas reglas que sse2.c (en el kernel, entre fpu_kernel_inicio/fim; en
 * los programas, solo si CPUID dice que hay SSSE3). */
#include "sse2.h"

typedef char v16qi __attribute__((vector_size(16), may_alias));
typedef char v16qi_u __attribute__((vector_size(16), aligned(1), may_alias));
typedef unsigned u32;

void ssse3_trocar_rb32(unsigned *d, const unsigned *o, unsigned n)
{
    /* En cada píxel de 4 bytes: byte 0 <-> byte 2 (0xAARRGGBB <-> 0xAABBGGRR). */
    static const v16qi mascara = { 2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15 };
    for (; n >= 4; n -= 4, d += 4, o += 4) {
        v16qi p = *(const v16qi_u *)o;
        *(v16qi_u *)d = __builtin_ia32_pshufb128(p, mascara);
    }
    for (; n; n--, d++, o++) {
        u32 c = *o;
        *d = (c & 0xFF00FF00u) | (c >> 16 & 0xFF) | (c & 0xFF) << 16;
    }
}
