// SPDX-License-Identifier: GPL-2.0-or-later
/* blake2s.c - BLAKE2s (RFC 7693), con y sin clave, y su entrada en la tabla de
 * hashes (HASH_BLAKE2S), para que el HMAC y el HKDF de hash.c sirvan también con
 * él. Es el hash del WireGuard (D363): HASH, MAC (BLAKE2s con clave y 16 bytes de
 * salida), HMAC y el KDF del Noise.
 *
 * Escrito a partir del RFC 7693 (secciones 2 y 3); las pruebas con los vectores
 * del RFC y los de la referencia de BLAKE2 están en tests/h365_wireguard.py (en el
 * anfitrión) y en userland/testes/pruebacripto.c.
 *
 * Solo sumas, XOR y rotaciones de 32 bits sobre datos: tiempo constante sin
 * esfuerzo. */
#include "cripto.h"

static const uint32_t IV[8] = {
    0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A, 0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19,
};

static const uint8_t SIGMA[10][16] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
    { 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
    { 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
    { 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
    { 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
    { 12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
    { 13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
    { 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
    { 10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0 },
};

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
#define ROTD(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define G(a, b, c, d, x, y) \
    v[a] += v[b] + (x); v[d] = ROTD(v[d] ^ v[a], 16); \
    v[c] += v[d];       v[b] = ROTD(v[b] ^ v[c], 12); \
    v[a] += v[b] + (y); v[d] = ROTD(v[d] ^ v[a], 8);  \
    v[c] += v[d];       v[b] = ROTD(v[b] ^ v[c], 7);

/* La función F del RFC 7693 (3.2). */
static void comprimir(blake2s_t *c, const uint8_t bloco[64], bool ultimo)
{
    uint32_t v[16], m[16];
    for (int i = 0; i < 16; i++) m[i] = le32(bloco + 4 * i);
    for (int i = 0; i < 8; i++) { v[i] = c->h[i]; v[i + 8] = IV[i]; }
    v[12] ^= c->t[0];
    v[13] ^= c->t[1];
    if (ultimo) v[14] = ~v[14];
    for (int r = 0; r < 10; r++) {
        const uint8_t *s = SIGMA[r];
        G(0, 4, 8, 12, m[s[0]], m[s[1]])
        G(1, 5, 9, 13, m[s[2]], m[s[3]])
        G(2, 6, 10, 14, m[s[4]], m[s[5]])
        G(3, 7, 11, 15, m[s[6]], m[s[7]])
        G(0, 5, 10, 15, m[s[8]], m[s[9]])
        G(1, 6, 11, 12, m[s[10]], m[s[11]])
        G(2, 7, 8, 13, m[s[12]], m[s[13]])
        G(3, 4, 9, 14, m[s[14]], m[s[15]])
    }
    for (int i = 0; i < 8; i++) c->h[i] ^= v[i] ^ v[i + 8];
    cripto_apagar(v, sizeof v);
    cripto_apagar(m, sizeof m);
}

void blake2s_iniciar_chave(blake2s_t *c, unsigned tam, const void *chave, unsigned nchave)
{
    if (tam < 1 || tam > 32) tam = 32;
    if (nchave > 32) nchave = 32;
    for (int i = 0; i < 8; i++) c->h[i] = IV[i];
    c->h[0] ^= 0x01010000u ^ ((uint32_t)nchave << 8) ^ tam;   /* bloque de parámetros: fanout 1, profundidad 1 */
    c->t[0] = c->t[1] = 0;
    c->usados = 0;
    c->tam = tam;
    memset(c->bloco, 0, sizeof c->bloco);
    if (nchave) {   /* la clave, rellenada con ceros, es el primer bloque */
        memcpy(c->bloco, chave, nchave);
        c->usados = 64;
    }
}

void blake2s_iniciar(blake2s_t *c, unsigned tam) { blake2s_iniciar_chave(c, tam, NULL, 0); }

void blake2s_somar(blake2s_t *c, const void *dados, unsigned n)
{
    const uint8_t *p = dados;
    while (n) {
        if (c->usados == 64) {   /* el bloque lleno solo se comprime cuando llega más: el último va con la marca final */
            c->t[0] += 64;
            if (c->t[0] < 64) c->t[1]++;
            comprimir(c, c->bloco, false);
            c->usados = 0;
        }
        unsigned k = 64 - c->usados;
        if (k > n) k = n;
        memcpy(c->bloco + c->usados, p, k);
        c->usados += k;
        p += k;
        n -= k;
    }
}

void blake2s_fim(blake2s_t *c, uint8_t *saida)
{
    c->t[0] += c->usados;
    if (c->t[0] < c->usados) c->t[1]++;
    memset(c->bloco + c->usados, 0, 64 - c->usados);
    comprimir(c, c->bloco, true);
    for (unsigned i = 0; i < c->tam; i++) saida[i] = (uint8_t)(c->h[i / 4] >> (8 * (i % 4)));
    cripto_apagar(c, sizeof *c);
}

void blake2s(uint8_t *saida, unsigned tam, const void *chave, unsigned nchave, const void *dados, unsigned n)
{
    blake2s_t c;
    blake2s_iniciar_chave(&c, tam, chave, nchave);
    blake2s_somar(&c, dados, n);
    blake2s_fim(&c, saida);
}

/* La tabla de hashes: BLAKE2s-256 sin clave, bloque de 64 bytes (el HMAC del WireGuard). */
static void ib2s(hash_ctx_t *c) { blake2s_iniciar(&c->b2s, 32); }
static void sb2s(hash_ctx_t *c, const void *d, unsigned n) { blake2s_somar(&c->b2s, d, n); }
static void fb2s(hash_ctx_t *c, uint8_t *o) { blake2s_fim(&c->b2s, o); }
const hash_alg_t HASH_BLAKE2S = { "BLAKE2s", 32, 64, ib2s, sb2s, fb2s };
