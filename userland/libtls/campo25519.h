// SPDX-License-Identifier: GPL-2.0-or-later
/* campo25519.h - aritmética en GF(2^255 - 19), compartida por X25519 (D060)
 * y Ed25519 (D250).
 *
 * Elementos en 16 miembros de 16 bits guardados en int64: los productos
 * (hasta 2^34) y las sumas de 16 de ellos caben con holgura, y no hace falta
 * propagar el acarreo en cada suma. 2^256 = 38 (mod p) dobla la mitad alta
 * del producto sobre la baja. Nada de aquí tiene saltos ni índices que
 * dependan de los datos. */
#ifndef SAGUI_CAMPO25519_H
#define SAGUI_CAMPO25519_H
#include "cripto.h"

#define FE_UNUSED __attribute__((unused))

typedef int64_t fe[16];

FE_UNUSED static void fe_copiar(fe r, const fe a) { for (int i = 0; i < 16; i++) r[i] = a[i]; }

FE_UNUSED static void fe_vai_um(fe o)
{
    for (int i = 0; i < 16; i++) {
        int64_t c = o[i] >> 16;          /* deslocamento aritmético: piso, também para negativos */
        o[i] -= c * 65536;
        if (i < 15) o[i + 1] += c;
        else o[0] += 38 * c;             /* o que passa de 2^256 volta multiplicado por 38 */
    }
}

FE_UNUSED static void fe_soma(fe r, const fe a, const fe b) { for (int i = 0; i < 16; i++) r[i] = a[i] + b[i]; }
FE_UNUSED static void fe_sub(fe r, const fe a, const fe b) { for (int i = 0; i < 16; i++) r[i] = a[i] - b[i]; }

FE_UNUSED static void fe_mul(fe r, const fe a, const fe b)
{
    int64_t t[31];
    for (int i = 0; i < 31; i++) t[i] = 0;
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 16; j++) t[i + j] += a[i] * b[j];
    for (int i = 0; i < 15; i++) t[i] += 38 * t[i + 16];
    for (int i = 0; i < 16; i++) r[i] = t[i];
    fe_vai_um(r);
    fe_vai_um(r);
}

FE_UNUSED static void fe_quad(fe r, const fe a) { fe_mul(r, a, a); }

/* Troca a e b se bit = 1, sem desvio. */
FE_UNUSED static void fe_cswap(fe a, fe b, int64_t bit)
{
    int64_t m = -bit;
    for (int i = 0; i < 16; i++) {
        int64_t t = m & (a[i] ^ b[i]);
        a[i] ^= t;
        b[i] ^= t;
    }
}

/* z^(p-2) = z^-1. O expoente p - 2 = 2^255 - 21 é público: todos os bits
 * ligados menos o 2 e o 4. */
FE_UNUSED static void fe_inverso(fe r, const fe z)
{
    fe c;
    fe_copiar(c, z);
    for (int i = 253; i >= 0; i--) {
        fe_quad(c, c);
        if (i != 2 && i != 4) fe_mul(c, c, z);
    }
    fe_copiar(r, c);
}

FE_UNUSED static void fe_ler(fe r, const uint8_t b[32])
{
    for (int i = 0; i < 16; i++) r[i] = (int64_t)b[2 * i] | (int64_t)b[2 * i + 1] << 8;
    r[15] &= 0x7fff;   /* o bit 255 é ignorado (seção 5) */
}

/* Forma canônica (0 <= x < p) e bytes little-endian. */
FE_UNUSED static void fe_gravar(uint8_t s[32], const fe a)
{
    fe t, m;
    fe_copiar(t, a);
    fe_vai_um(t);
    fe_vai_um(t);
    fe_vai_um(t);
    for (int k = 0; k < 2; k++) {   /* subtrai p até duas vezes, escolhendo por máscara */
        m[0] = t[0] - 0xffed;
        for (int i = 1; i < 15; i++) {
            m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
            m[i - 1] &= 0xffff;
        }
        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        int64_t emprestou = (m[15] >> 16) & 1;
        m[14] &= 0xffff;
        fe_cswap(t, m, 1 - emprestou);
    }
    for (int i = 0; i < 16; i++) {
        s[2 * i] = (uint8_t)t[i];
        s[2 * i + 1] = (uint8_t)(t[i] >> 8);
    }
}

#endif
