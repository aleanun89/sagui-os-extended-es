// SPDX-License-Identifier: GPL-2.0-or-later
/* sse2.c - copias y rellenos con SSE2 (D042).
 *
 * Único archivo del kernel compilado con -msse2 (regla propia en el Makefile).
 * GCC puede usar registros XMM en CUALQUIER función de aquí, por eso:
 *   - en el kernel, todo aquí solo corre entre fpu_kernel_inicio() y
 *     fpu_kernel_fim(), que guardan los XMM del proceso dueño y apagan el
 *     CR0.TS (sin eso, el primer XMM daría #NM en el anillo 0 = pánico);
 *   - en libsagui (el mismo archivo se compila de nuevo para los programas),
 *     solo después de que CPUID diga que hay SSE2.
 * Sin cabeceras del sistema: los tipos vectoriales y las instrucciones vienen de las
 * extensiones de GCC (<emmintrin.h> trae <stdlib.h>, que no existe aquí).
 *
 * Todas las funciones hacen lo mismo: alinean el destino a un múltiplo de 16
 * bytes con copias pequeñas, luego avanzan 64 bytes por vuelta (cuatro
 * registros) y terminan el resto con copias pequeñas. Leer fuera del
 * alineamiento (movdqu) es barato; escribir alineado (movdqa/movntdq) no divide
 * líneas de caché. */
#include "sse2.h"

typedef int v4si __attribute__((vector_size(16), may_alias));
typedef int v4si_u __attribute__((vector_size(16), aligned(1), may_alias));   /* sin alineamiento */
typedef unsigned v4su __attribute__((vector_size(16), may_alias));
typedef long long v2di __attribute__((vector_size(16), may_alias));
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned int u32a __attribute__((may_alias, aligned(1)));

#define ALINH(p) ((u32)(p) & 15)

static inline v4si ler(const void *p) { return *(const v4si_u *)p; }
static inline void gravar(void *p, v4si v) { *(v4si *)p = v; }
static inline void gravar_nt(void *p, v4si v) { __builtin_ia32_movntdq((v2di *)p, (v2di)v); }

/* Copia pequeña (hasta 15 bytes, o el rabo): de 4 en 4 y luego byte a byte. */
static inline void pouco(u8 *d, const u8 *o, u32 n)
{
    while (n >= 4) { *(u32a *)d = *(const u32a *)o; d += 4; o += 4; n -= 4; }
    while (n--) *d++ = *o++;
}

void sse2_copiar(void *dst, const void *src, unsigned n)
{
    u8 *d = dst;
    const u8 *o = src;
    if (n >= 16 && ALINH(d)) {
        u32 k = 16 - ALINH(d);
        pouco(d, o, k);
        d += k; o += k; n -= k;
    }
    for (; n >= 64; n -= 64, d += 64, o += 64) {
        v4si a = ler(o), b = ler(o + 16), c = ler(o + 32), e = ler(o + 48);
        gravar(d, a); gravar(d + 16, b); gravar(d + 32, c); gravar(d + 48, e);
    }
    for (; n >= 16; n -= 16, d += 16, o += 16) gravar(d, ler(o));
    pouco(d, o, n);
}

void sse2_copiar_nt(void *dst, const void *src, unsigned n)
{
    u8 *d = dst;
    const u8 *o = src;
    if (n >= 16 && ALINH(d)) {
        u32 k = 16 - ALINH(d);
        pouco(d, o, k);
        d += k; o += k; n -= k;
    }
    for (; n >= 64; n -= 64, d += 64, o += 64) {
        v4si a = ler(o), b = ler(o + 16), c = ler(o + 32), e = ler(o + 48);
        gravar_nt(d, a); gravar_nt(d + 16, b); gravar_nt(d + 32, c); gravar_nt(d + 48, e);
    }
    for (; n >= 16; n -= 16, d += 16, o += 16) gravar_nt(d, ler(o));
    __builtin_ia32_sfence();   /* las escrituras no temporales se ven antes de seguir */
    pouco(d, o, n);
}

/* Con el destino después del origen y solapado: cada bloque se lee antes de
 * cualquier escritura que pueda cubrirlo (las escrituras van a direcciones
 * más altas que los bloques todavía no leídos). */
void sse2_copiar_tras(void *dst, const void *src, unsigned n)
{
    u8 *d = (u8 *)dst + n;
    const u8 *o = (const u8 *)src + n;
    while (n && ALINH(d)) { *--d = *--o; n--; }
    for (; n >= 64; n -= 64) {
        d -= 64; o -= 64;
        v4si e = ler(o + 48), c = ler(o + 32), b = ler(o + 16), a = ler(o);
        gravar(d + 48, e); gravar(d + 32, c); gravar(d + 16, b); gravar(d, a);
    }
    for (; n >= 16; n -= 16) { d -= 16; o -= 16; gravar(d, ler(o)); }
    while (n--) *--d = *--o;
}

void sse2_memset(void *dst, int v, unsigned n)
{
    u8 *d = dst;
    u8 b = (u8)v;
    while (n && ALINH(d)) { *d++ = b; n--; }
    u32 v4 = b * 0x01010101u;
    v4si x = { (int)v4, (int)v4, (int)v4, (int)v4 };
    for (; n >= 64; n -= 64, d += 64) { gravar(d, x); gravar(d + 16, x); gravar(d + 32, x); gravar(d + 48, x); }
    for (; n >= 16; n -= 16, d += 16) gravar(d, x);
    while (n--) *d++ = b;
}

void sse2_preencher32(unsigned *d, unsigned v, unsigned n)
{
    while (n && ALINH(d)) { *d++ = v; n--; }
    v4si x = { (int)v, (int)v, (int)v, (int)v };
    for (; n >= 16; n -= 16, d += 16) { gravar(d, x); gravar(d + 4, x); gravar(d + 8, x); gravar(d + 12, x); }
    for (; n >= 4; n -= 4, d += 4) gravar(d, x);
    while (n--) *d++ = v;
}

void sse2_ampliar2x(unsigned *d, unsigned *d2, const unsigned *o, unsigned n)
{
    /* Cuatro píxeles a b c e pasan a a a b b | c c e e (punpckldq/punpckhdq
     * del registro con él mismo). */
    if (!ALINH(d) && (!d2 || !ALINH(d2))) {
        for (; n >= 4; n -= 4, d += 8, o += 4) {
            v4si p = ler(o);
            v4si b = __builtin_ia32_punpckldq128(p, p), a = __builtin_ia32_punpckhdq128(p, p);
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

void sse2_escurecer(unsigned *d, unsigned n, unsigned desloc, unsigned mascara)
{
    /* psrld + pand en cuatro píxeles por vez: lo mismo que (p >> desloc) & mascara. */
    while (n && ALINH(d)) { *d = (*d >> desloc) & mascara; d++; n--; }
    v4si m = { (int)mascara, (int)mascara, (int)mascara, (int)mascara };
    for (; n >= 4; n -= 4, d += 4) {
        v4si p = *(v4si *)d;
        *(v4si *)d = (v4si)((v4su)p >> desloc) & m;
    }
    while (n--) { *d = (*d >> desloc) & mascara; d++; }
}

/* 0x00RRGGBB -> RGB565 (cinco bits del rojo, seis del verde, cinco del
 * azul) o RGB555 (cinco de cada), ocho píxeles por vuelta (D078): tres
 * psrld + pand + por en cuatro píxeles de 32 bits, y el packssdw une dos
 * mitades en ocho de 16 bits. El packssdw satura con signo, así que antes cada
 * valor se convierte en un entero de 16 bits con signo (pslld 16 y psrad 16): los bits
 * pasan intactos. d en píxeles de 16 bits, sin exigencia de alineamiento. */
typedef short v8hi __attribute__((vector_size(16), may_alias));
typedef short v8hi_u __attribute__((vector_size(16), aligned(1), may_alias));
typedef unsigned short u16a __attribute__((may_alias, aligned(1)));

static inline u32 um_16(u32 c, int r555)
{
    return r555 ? ((c >> 9) & 0x7C00) | ((c >> 6) & 0x03E0) | ((c >> 3) & 0x1F)
                : ((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x1F);
}

void sse2_converter16(unsigned short *d, const unsigned *o, unsigned n, int r555)
{
    const u32 mr = r555 ? 0x7C00 : 0xF800, mg = r555 ? 0x03E0 : 0x07E0;
    const unsigned dr = r555 ? 9 : 8, dg = r555 ? 6 : 5;
    v4su vr = { mr, mr, mr, mr }, vg = { mg, mg, mg, mg }, vb = { 0x1F, 0x1F, 0x1F, 0x1F };
    for (; n >= 8; n -= 8, d += 8, o += 8) {
        v4su a = (v4su)ler(o), b = (v4su)ler(o + 4);
        v4su ra = ((a >> dr) & vr) | ((a >> dg) & vg) | ((a >> 3) & vb);
        v4su rb = ((b >> dr) & vr) | ((b >> dg) & vg) | ((b >> 3) & vb);
        v4si sa = ((v4si)(ra << 16)) >> 16, sb = ((v4si)(rb << 16)) >> 16;
        *(v8hi_u *)d = (v8hi)__builtin_ia32_packssdw128(sa, sb);
    }
    while (n--) *(u16a *)d++ = (unsigned short)um_16(*o++, r555);
}

/* Intercambia el canal rojo y el azul (0xAARRGGBB <-> 0xAABBGGRR): (p & verde) | rojo
 * desplazado | azul desplazado, cuatro píxeles por vuelta (D208). En SSSE3 hay una
 * versión de una sola instrucción (ssse3_trocar_rb32). */
void sse2_trocar_rb32(unsigned *d, const unsigned *o, unsigned n)
{
    const v4su mv = { 0xFF00FF00u, 0xFF00FF00u, 0xFF00FF00u, 0xFF00FF00u }, mb = { 0xFF, 0xFF, 0xFF, 0xFF };
    for (; n >= 4; n -= 4, d += 4, o += 4) {
        v4su p = (v4su)ler(o);
        v4su r = (p & mv) | ((p >> 16) & mb) | ((p & mb) << 16);
        *(v4si_u *)d = (v4si)r;
    }
    for (; n; n--, d++, o++) {
        u32 c = *o;
        *d = (c & 0xFF00FF00u) | (c >> 16 & 0xFF) | (c & 0xFF) << 16;
    }
}

/* Mezcla constante: d = (o * a + d * (256 - a)) >> 8 en cada canal (a de 0 a 256).
 * Cada byte se ensancha a 16 bits con punpck, se multiplica con pmullw, se suma, se
 * desplaza y se vuelve a empaquetar con packuswb: cuatro píxeles por vuelta. */
typedef char v16qi __attribute__((vector_size(16), may_alias));
typedef unsigned short v8hu __attribute__((vector_size(16), may_alias));

void sse2_misturar32(unsigned *d, const unsigned *o, unsigned n, unsigned a)
{
    const v16qi zero = { 0 };
    const unsigned b = 256 - a;
    const v8hu va = { a, a, a, a, a, a, a, a }, vb = { b, b, b, b, b, b, b, b };
    for (; n >= 4; n -= 4, d += 4, o += 4) {
        v16qi p = (v16qi)ler(o), q = (v16qi)ler(d);
        v8hu pl = (v8hu)__builtin_ia32_punpcklbw128(p, zero), ph = (v8hu)__builtin_ia32_punpckhbw128(p, zero);
        v8hu ql = (v8hu)__builtin_ia32_punpcklbw128(q, zero), qh = (v8hu)__builtin_ia32_punpckhbw128(q, zero);
        v8hu rl = pl * va + ql * vb, rh = ph * va + qh * vb;
        rl = (v8hu)__builtin_ia32_psrlwi128((v8hi)rl, 8);
        rh = (v8hu)__builtin_ia32_psrlwi128((v8hi)rh, 8);
        *(v4si_u *)d = (v4si)__builtin_ia32_packuswb128((v8hi)rl,
                                                         (v8hi)rh);
    }
    for (; n; n--, d++, o++) {
        u32 p = *o, q = *d, r = 0;
        for (int s = 0; s < 32; s += 8) r |= ((((p >> s) & 255) * a + ((q >> s) & 255) * b) >> 8) << s;
        *d = r;
    }
}
