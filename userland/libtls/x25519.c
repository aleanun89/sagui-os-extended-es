// SPDX-License-Identifier: GPL-2.0-or-later
/* x25519.c - Diffie-Hellman na Curve25519 (RFC 7748, seção 5). D060.
 *
 * Elementos de GF(2^255 - 19) em 16 membros de 16 bits guardados em int64:
 * os produtos (até 2^34) e as somas de 16 deles cabem com folga, e não é
 * preciso propagar o "vai um" a cada soma. 2^256 = 38 (mod p) dobra a
 * metade alta do produto sobre a baixa.
 *
 * A escada de Montgomery faz as mesmas operações para qualquer bit do
 * escalar: a troca de x2/x3 é por máscara (cswap), sem desvio. */
#include "cripto.h"
#include "campo25519.h"

void x25519(uint8_t saida[32], const uint8_t escalar[32], const uint8_t ponto[32])
{
    uint8_t k[32];
    memcpy(k, escalar, 32);
    k[0] &= 248;     /* "decodeScalar25519" */
    k[31] &= 127;
    k[31] |= 64;
    fe x1, x2, z2, x3, z3, a, aa, b, bb, e, c, d, da, cb, t;
    static const fe a24 = { 0xdb41, 1 };   /* 121665 */
    fe_ler(x1, ponto);
    for (int i = 0; i < 16; i++) { x2[i] = z2[i] = z3[i] = 0; x3[i] = x1[i]; }
    x2[0] = 1;
    z3[0] = 1;
    int64_t troca = 0;
    for (int i = 254; i >= 0; i--) {
        int64_t bit = (k[i >> 3] >> (i & 7)) & 1;
        troca ^= bit;
        fe_cswap(x2, x3, troca);
        fe_cswap(z2, z3, troca);
        troca = bit;
        fe_soma(a, x2, z2);
        fe_quad(aa, a);
        fe_sub(b, x2, z2);
        fe_quad(bb, b);
        fe_sub(e, aa, bb);
        fe_soma(c, x3, z3);
        fe_sub(d, x3, z3);
        fe_mul(da, d, a);
        fe_mul(cb, c, b);
        fe_soma(t, da, cb);
        fe_quad(x3, t);
        fe_sub(t, da, cb);
        fe_quad(t, t);
        fe_mul(z3, x1, t);
        fe_mul(x2, aa, bb);
        fe_mul(t, a24, e);
        fe_soma(t, aa, t);
        fe_mul(z2, e, t);
    }
    fe_cswap(x2, x3, troca);
    fe_cswap(z2, z3, troca);
    fe_inverso(z2, z2);
    fe_mul(x2, x2, z2);
    fe_gravar(saida, x2);
    cripto_apagar(k, sizeof k);
    cripto_apagar(x2, sizeof x2);
    cripto_apagar(z2, sizeof z2);
    cripto_apagar(x3, sizeof x3);
    cripto_apagar(z3, sizeof z3);
}

void x25519_base(uint8_t saida[32], const uint8_t escalar[32])
{
    static const uint8_t nove[32] = { 9 };
    x25519(saida, escalar, nove);
}
