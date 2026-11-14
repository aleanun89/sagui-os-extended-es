// SPDX-License-Identifier: GPL-2.0-or-later
/* resumos_logica.h - MD5 (RFC 1321) y SHA-1 (RFC 3174) para md5sum y sha1sum (D232).
 * Sirven para comprobar descargas y archivos, no como protección: ambos están rotos para firmas. */
#ifndef RESUMOS_LOGICA_H
#define RESUMOS_LOGICA_H
#include "sagui.h"

typedef struct { uint32_t h[5]; uint64_t total; uint8_t buf[64]; int n; } resumo_t;

#define ROL32(x, c) (((x) << (c)) | ((x) >> (32 - (c))))

static void md5r_bloque(uint32_t h[4], const uint8_t *p)
{
    static const uint8_t r[64] = { 7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22, 5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
                                   4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23, 6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21 };
    static const uint32_t k[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391 };
    uint32_t w[16], a = h[0], b = h[1], c = h[2], d = h[3];
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4*i] | (uint32_t)p[4*i+1] << 8 | (uint32_t)p[4*i+2] << 16 | (uint32_t)p[4*i+3] << 24;
    for (int i = 0; i < 64; i++) {
        uint32_t f; int g;
        if (i < 16) { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
        else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) & 15; }
        else { f = c ^ (b | ~d); g = (7 * i) & 15; }
        uint32_t t = d;
        d = c; c = b;
        b = b + ROL32(a + f + k[i] + w[g], r[i]);
        a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
}

static void sha1r_bloque(uint32_t h[5], const uint8_t *p)
{
    uint32_t w[80], a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4*i] << 24 | (uint32_t)p[4*i+1] << 16 | (uint32_t)p[4*i+2] << 8 | p[4*i+3];
    for (int i = 16; i < 80; i++) w[i] = ROL32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6; }
        uint32_t t = ROL32(a, 5) + f + e + k + w[i];
        e = d; d = c; c = ROL32(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

/* sha: true = SHA-1, false = MD5 */
static void resumo_iniciar(resumo_t *c, bool sha)
{
    memset(c, 0, sizeof *c);
    (void)sha;   /* los cuatro primeros valores iniciales son los mismos en MD5 y SHA-1 */
    c->h[0] = 0x67452301u;
    c->h[1] = 0xEFCDAB89u;
    c->h[2] = 0x98BADCFEu;
    c->h[3] = 0x10325476u;
    c->h[4] = 0xC3D2E1F0u;
}

static void resumo_somar(resumo_t *c, bool sha, const void *dados, uint32_t n)
{
    const uint8_t *p = dados;
    c->total += n;
    while (n) {
        uint32_t k = 64 - (uint32_t)c->n;
        if (k > n) k = n;
        memcpy(c->buf + c->n, p, k);
        c->n += (int)k; p += k; n -= k;
        if (c->n == 64) { if (sha) sha1r_bloque(c->h, c->buf); else md5r_bloque(c->h, c->buf); c->n = 0; }
    }
}

/* Cierra y escribe el resumen en hexadecimal (33 bytes para MD5, 41 para SHA-1). */
static void resumo_fim(resumo_t *c, bool sha, char *hex)
{
    uint64_t bits = c->total * 8;
    uint8_t pad[72];
    int relleno = (c->n < 56) ? 56 - c->n : 120 - c->n;
    memset(pad, 0, sizeof pad);
    pad[0] = 0x80;
    for (int i = 0; i < 8; i++) pad[relleno + i] = (uint8_t)(sha ? bits >> (56 - 8 * i) : bits >> (8 * i));
    resumo_somar(c, sha, pad, (uint32_t)relleno + 8);
    int nh = sha ? 5 : 4;
    for (int i = 0; i < nh; i++)
        for (int j = 0; j < 4; j++) {
            uint8_t b = (uint8_t)(sha ? c->h[i] >> (24 - 8 * j) : c->h[i] >> (8 * j));
            static const char hx[] = "0123456789abcdef";
            hex[8 * i + 2 * j] = hx[b >> 4];
            hex[8 * i + 2 * j + 1] = hx[b & 15];
        }
    hex[8 * nh] = 0;
}

static void md5_hex(const void *d, uint32_t n, char *hex) { resumo_t c; resumo_iniciar(&c, false); resumo_somar(&c, false, d, n); resumo_fim(&c, false, hex); }
static void sha1_hex(const void *d, uint32_t n, char *hex) { resumo_t c; resumo_iniciar(&c, true); resumo_somar(&c, true, d, n); resumo_fim(&c, true, hex); }

#endif
