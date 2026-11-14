// SPDX-License-Identifier: GPL-2.0-or-later
/* chacha.c - ChaCha20, Poly1305 e o AEAD ChaCha20-Poly1305 (RFC 8439). D060.
 *
 * Só somas, XORs e rotações de 32 bits (ChaCha20) e multiplicações de
 * números de 26 bits (Poly1305): tempo constante sem esforço, e mais rápido
 * que o AES sem tabelas numa CPU sem AES-NI. Por isso o cliente TLS oferece
 * esta suíte primeiro. */
#include "cripto.h"

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void poe_le32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

#define ROTE(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define QR(a, b, c, d) \
    a += b; d ^= a; d = ROTE(d, 16); \
    c += d; b ^= c; b = ROTE(b, 12); \
    a += b; d ^= a; d = ROTE(d, 8);  \
    c += d; b ^= c; b = ROTE(b, 7);

void chacha20_bloco(const uint8_t chave[32], uint32_t contador, const uint8_t nonce[12], uint8_t saida[64])
{
    uint32_t e[16], x[16];
    e[0] = 0x61707865; e[1] = 0x3320646e; e[2] = 0x79622d32; e[3] = 0x6b206574;   /* "expand 32-byte k" */
    for (int i = 0; i < 8; i++) e[4 + i] = le32(chave + 4 * i);
    e[12] = contador;
    for (int i = 0; i < 3; i++) e[13 + i] = le32(nonce + 4 * i);
    memcpy(x, e, sizeof e);
    for (int i = 0; i < 10; i++) {
        QR(x[0], x[4], x[8], x[12]) QR(x[1], x[5], x[9], x[13])
        QR(x[2], x[6], x[10], x[14]) QR(x[3], x[7], x[11], x[15])
        QR(x[0], x[5], x[10], x[15]) QR(x[1], x[6], x[11], x[12])
        QR(x[2], x[7], x[8], x[13]) QR(x[3], x[4], x[9], x[14])
    }
    for (int i = 0; i < 16; i++) poe_le32(saida + 4 * i, x[i] + e[i]);
    cripto_apagar(x, sizeof x);
    cripto_apagar(e, sizeof e);
}

void chacha20_xor(const uint8_t chave[32], uint32_t contador, const uint8_t nonce[12],
                  const uint8_t *entrada, uint8_t *saida, unsigned n)
{
    uint8_t ks[64];
    while (n) {
        chacha20_bloco(chave, contador++, nonce, ks);
        unsigned k = n < 64 ? n : 64;
        for (unsigned i = 0; i < k; i++) saida[i] = entrada[i] ^ ks[i];
        entrada += k;
        saida += k;
        n -= k;
    }
    cripto_apagar(ks, sizeof ks);
}

/* ------------------------------------------------------------ Poly1305 */

/* Acumulador e r em 5 "membros" de 26 bits: os produtos cabem em 64 bits. */
typedef struct {
    uint32_t r[5], s[4];   /* s = pad (segunda metade da chave) */
    uint32_t h[5];
    uint8_t  resto[16];
    unsigned nresto;
} poly_t;

static void poly_iniciar(poly_t *p, const uint8_t k[32])
{
    /* r com os bits "presos" da seção 2.5 (r &= 0x0ffffffc0ffffffc0ffffffc0fffffff) */
    p->r[0] = le32(k + 0) & 0x3ffffff;
    p->r[1] = (le32(k + 3) >> 2) & 0x3ffff03;
    p->r[2] = (le32(k + 6) >> 4) & 0x3ffc0ff;
    p->r[3] = (le32(k + 9) >> 6) & 0x3f03fff;
    p->r[4] = (le32(k + 12) >> 8) & 0x00fffff;
    for (int i = 0; i < 4; i++) p->s[i] = le32(k + 16 + 4 * i);
    for (int i = 0; i < 5; i++) p->h[i] = 0;
    p->nresto = 0;
}

static void poly_blocos(poly_t *p, const uint8_t *m, unsigned n, uint32_t bit_alto)
{
    const uint32_t r0 = p->r[0], r1 = p->r[1], r2 = p->r[2], r3 = p->r[3], r4 = p->r[4];
    const uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;   /* 2^130 = 5 (mod p) */
    uint32_t h0 = p->h[0], h1 = p->h[1], h2 = p->h[2], h3 = p->h[3], h4 = p->h[4];
    while (n >= 16) {
        h0 += le32(m) & 0x3ffffff;
        h1 += (le32(m + 3) >> 2) & 0x3ffffff;
        h2 += (le32(m + 6) >> 4) & 0x3ffffff;
        h3 += (le32(m + 9) >> 6) & 0x3ffffff;
        h4 += (le32(m + 12) >> 8) | bit_alto;
        uint64_t d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 + (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
        uint64_t d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 + (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
        uint64_t d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 + (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
        uint64_t d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 + (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
        uint64_t d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 + (uint64_t)h3 * r1 + (uint64_t)h4 * r0;
        uint32_t c;
        c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffff; d1 += c;
        c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffff; d2 += c;
        c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffff; d3 += c;
        c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffff; d4 += c;
        c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffff;
        h0 += c * 5;
        c = h0 >> 26; h0 &= 0x3ffffff; h1 += c;
        m += 16;
        n -= 16;
    }
    p->h[0] = h0; p->h[1] = h1; p->h[2] = h2; p->h[3] = h3; p->h[4] = h4;
}

static void poly_somar(poly_t *p, const uint8_t *m, unsigned n)
{
    if (p->nresto) {
        unsigned k = 16 - p->nresto;
        if (k > n) k = n;
        memcpy(p->resto + p->nresto, m, k);
        p->nresto += k;
        m += k;
        n -= k;
        if (p->nresto < 16) return;
        poly_blocos(p, p->resto, 16, 1u << 24);
        p->nresto = 0;
    }
    unsigned inteiros = n & ~15u;
    poly_blocos(p, m, inteiros, 1u << 24);
    m += inteiros;
    n -= inteiros;
    memcpy(p->resto, m, n);
    p->nresto = n;
}

static void poly_fim(poly_t *p, uint8_t tag[16])
{
    if (p->nresto) {   /* último bloco curto: 0x01 depois dos dados e sem o bit 2^128 */
        p->resto[p->nresto] = 1;
        for (unsigned i = p->nresto + 1; i < 16; i++) p->resto[i] = 0;
        poly_blocos(p, p->resto, 16, 0);
    }
    uint32_t h0 = p->h[0], h1 = p->h[1], h2 = p->h[2], h3 = p->h[3], h4 = p->h[4], c;
    c = h1 >> 26; h1 &= 0x3ffffff; h2 += c;
    c = h2 >> 26; h2 &= 0x3ffffff; h3 += c;
    c = h3 >> 26; h3 &= 0x3ffffff; h4 += c;
    c = h4 >> 26; h4 &= 0x3ffffff; h0 += c * 5;
    c = h0 >> 26; h0 &= 0x3ffffff; h1 += c;
    /* g = h + 5 - 2^130: se não ficou negativo, h >= p e o resultado é g */
    uint32_t g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
    uint32_t g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
    uint32_t g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
    uint32_t g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
    uint32_t g4 = h4 + c - (1u << 26);
    uint32_t usa_g = (g4 >> 31) - 1;   /* tudo 1 se g4 >= 0 */
    h0 = (h0 & ~usa_g) | (g0 & usa_g);
    h1 = (h1 & ~usa_g) | (g1 & usa_g);
    h2 = (h2 & ~usa_g) | (g2 & usa_g);
    h3 = (h3 & ~usa_g) | (g3 & usa_g);
    h4 = (h4 & ~usa_g) | (g4 & usa_g & 0x3ffffff);
    /* para 128 bits e soma o pad (mod 2^128) */
    uint32_t w0 = h0 | (h1 << 26), w1 = (h1 >> 6) | (h2 << 20), w2 = (h2 >> 12) | (h3 << 14), w3 = (h3 >> 18) | (h4 << 8);
    uint64_t f;
    f = (uint64_t)w0 + p->s[0];             poe_le32(tag, (uint32_t)f);
    f = (uint64_t)w1 + p->s[1] + (f >> 32); poe_le32(tag + 4, (uint32_t)f);
    f = (uint64_t)w2 + p->s[2] + (f >> 32); poe_le32(tag + 8, (uint32_t)f);
    f = (uint64_t)w3 + p->s[3] + (f >> 32); poe_le32(tag + 12, (uint32_t)f);
    cripto_apagar(p, sizeof *p);
}

void poly1305(const uint8_t chave[32], const uint8_t *msg, unsigned n, uint8_t tag[16])
{
    poly_t p;
    poly_iniciar(&p, chave);
    poly_somar(&p, msg, n);
    poly_fim(&p, tag);
}

/* ------------------------------------------------------------ AEAD (2.8) */

static void aead_tag(const uint8_t chave[32], const uint8_t nonce[12], const uint8_t *aad, unsigned naad,
                     const uint8_t *cifrado, unsigned n, uint8_t tag[16])
{
    uint8_t otk[64], zeros[16], tam[16];
    chacha20_bloco(chave, 0, nonce, otk);
    memset(zeros, 0, 16);
    poly_t p;
    poly_iniciar(&p, otk);
    poly_somar(&p, aad, naad);
    poly_somar(&p, zeros, (16 - naad % 16) % 16);
    poly_somar(&p, cifrado, n);
    poly_somar(&p, zeros, (16 - n % 16) % 16);
    poe_le32(tam, naad); poe_le32(tam + 4, 0);
    poe_le32(tam + 8, n); poe_le32(tam + 12, 0);
    poly_somar(&p, tam, 16);
    poly_fim(&p, tag);
    cripto_apagar(otk, sizeof otk);
}

void chachapoly_cifrar(const uint8_t chave[32], const uint8_t nonce[12], const uint8_t *aad, unsigned naad,
                       const uint8_t *entrada, uint8_t *saida, unsigned n, uint8_t tag[16])
{
    chacha20_xor(chave, 1, nonce, entrada, saida, n);
    aead_tag(chave, nonce, aad, naad, saida, n, tag);
}

bool chachapoly_decifrar(const uint8_t chave[32], const uint8_t nonce[12], const uint8_t *aad, unsigned naad,
                         const uint8_t *entrada, uint8_t *saida, unsigned n, const uint8_t tag[16])
{
    uint8_t t[16];
    aead_tag(chave, nonce, aad, naad, entrada, n, t);
    if (!cripto_igual(t, tag, 16)) return false;
    chacha20_xor(chave, 1, nonce, entrada, saida, n);
    return true;
}
