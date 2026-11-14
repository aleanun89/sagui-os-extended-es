// SPDX-License-Identifier: GPL-2.0-or-later
/* ed25519.c - firmas Ed25519 (RFC 8032, sección 5.1). D250.
 *
 * Sobre la aritmética de campo del X25519 (campo25519.h) y el SHA-512 de
 * hash.c. La curva es la de Edwards torcida -x^2 + y^2 = 1 + d x^2 y^2; los
 * puntos van en coordenadas extendidas (X, Y, Z, T), con x = X/Z, y = Y/Z y
 * xy = T/Z, y la suma es la fórmula unificada (vale también para doblar), sin
 * casos especiales.
 *
 * Tiempo constante: la multiplicación por el escalar recorre los 256 bits
 * haciendo siempre una suma y un doblado, y elige con intercambios por
 * máscara; la reducción módulo L tampoco tiene saltos que dependan de los
 * datos. La verificación solo maneja datos públicos. */
#include "cripto.h"
#include "campo25519.h"

typedef fe ponto[4];

static const fe fe_um = { 1 };
static const fe ed_d = { 0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141, 0x0a4d, 0x0070,
                         0xe898, 0x7779, 0x4079, 0x8cc7, 0xfe73, 0x2b6f, 0x6cee, 0x5203 };
static const fe ed_2d = { 0xf159, 0x26b2, 0x9b94, 0xebd6, 0xb156, 0x8283, 0x149a, 0x00e0,
                          0xd130, 0xeef3, 0x80f2, 0x198e, 0xfce7, 0x56df, 0xd9dc, 0x2406 };
static const fe base_x = { 0xd51a, 0x8f25, 0x2d60, 0xc956, 0xa7b2, 0x9525, 0xc760, 0x692c,
                           0xdc5c, 0xfdd6, 0xe231, 0xc0a4, 0x53fe, 0xcd6e, 0x36d3, 0x2169 };
static const fe base_y = { 0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666,
                           0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666 };
/* raíz cuadrada de -1 */
static const fe raiz_m1 = { 0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f, 0x1806, 0x2f43,
                            0xd7a7, 0x3dfb, 0x0099, 0x2b4d, 0xdf0b, 0x4fc1, 0x2480, 0x2b83 };
/* el orden del grupo: L = 2^252 + 27742317777372353535851937790883648493 */
static const uint8_t ordem[32] = { 0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7, 0xa2,
                                   0xde, 0xf9, 0xde, 0x14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10 };

static void fe_ler256(fe r, const uint8_t b[32])
{
    for (int i = 0; i < 16; i++) r[i] = (int64_t)b[2 * i] | (int64_t)b[2 * i + 1] << 8;
    r[15] &= 0x7fff;
}

static int fe_paridade(const fe a)
{
    uint8_t d[32];
    fe_gravar(d, a);
    return d[0] & 1;
}

static bool fe_igual(const fe a, const fe b)
{
    uint8_t c[32], d[32];
    fe_gravar(c, a);
    fe_gravar(d, b);
    return cripto_igual(c, d, 32);
}

/* p = p + q (fórmula unificada, "add-2008-hwcd-3") */
static void ponto_somar(ponto p, ponto q)
{
    fe a, b, c, d, t, e, f, g, h;
    fe_sub(a, p[1], p[0]);
    fe_sub(t, q[1], q[0]);
    fe_mul(a, a, t);
    fe_soma(b, p[0], p[1]);
    fe_soma(t, q[0], q[1]);
    fe_mul(b, b, t);
    fe_mul(c, p[3], q[3]);
    fe_mul(c, c, ed_2d);
    fe_mul(d, p[2], q[2]);
    fe_soma(d, d, d);
    fe_sub(e, b, a);
    fe_sub(f, d, c);
    fe_soma(g, d, c);
    fe_soma(h, b, a);
    fe_mul(p[0], e, f);
    fe_mul(p[1], h, g);
    fe_mul(p[2], g, f);
    fe_mul(p[3], e, h);
}

static void ponto_cswap(ponto p, ponto q, int64_t bit)
{
    for (int i = 0; i < 4; i++) fe_cswap(p[i], q[i], bit);
}

/* Punto comprimido: la y con el signo de la x en el bit 255. */
static void ponto_gravar(uint8_t r[32], ponto p)
{
    fe tx, ty, zi;
    fe_inverso(zi, p[2]);
    fe_mul(tx, p[0], zi);
    fe_mul(ty, p[1], zi);
    fe_gravar(r, ty);
    r[31] ^= (uint8_t)(fe_paridade(tx) << 7);
}

/* p = s * q; estropea q. Los mismos pasos para cualquier escalar. */
static void ponto_mul(ponto p, ponto q, const uint8_t s[32])
{
    for (int i = 0; i < 16; i++) p[0][i] = p[1][i] = p[2][i] = p[3][i] = 0;
    p[1][0] = 1;
    p[2][0] = 1;
    for (int i = 255; i >= 0; i--) {
        int64_t bit = (s[i >> 3] >> (i & 7)) & 1;
        ponto_cswap(p, q, bit);
        ponto_somar(q, p);
        ponto_somar(p, p);
        ponto_cswap(p, q, bit);
    }
}

static void ponto_mul_base(ponto p, const uint8_t s[32])
{
    ponto q;
    fe_copiar(q[0], base_x);
    fe_copiar(q[1], base_y);
    fe_copiar(q[2], fe_um);
    fe_mul(q[3], base_x, base_y);
    ponto_mul(p, q, s);
}

/* x[0..63] (un número de hasta 512 bits, un byte por miembro) módulo L -> r. */
static void reduzir_l(uint8_t r[32], int64_t x[64])
{
    int64_t vai;
    for (int i = 63; i >= 32; i--) {
        vai = 0;
        int j;
        for (j = i - 32; j < i - 12; j++) {
            x[j] += vai - 16 * x[i] * (int64_t)ordem[j - (i - 32)];
            vai = (x[j] + 128) >> 8;
            x[j] -= vai * 256;
        }
        x[j] += vai;
        x[i] = 0;
    }
    vai = 0;
    for (int j = 0; j < 32; j++) {
        x[j] += vai - (x[31] >> 4) * (int64_t)ordem[j];
        vai = x[j] >> 8;
        x[j] &= 255;
    }
    for (int j = 0; j < 32; j++) x[j] -= vai * (int64_t)ordem[j];
    for (int i = 0; i < 32; i++) {
        x[i + 1] += x[i] >> 8;
        r[i] = (uint8_t)(x[i] & 255);
    }
}

static void reduzir_hash(uint8_t r[32], const uint8_t h[64])
{
    int64_t x[64];
    for (int i = 0; i < 64; i++) x[i] = h[i];
    reduzir_l(r, x);
    cripto_apagar(x, sizeof x);
}

/* SHA-512(semilla): la mitad baja "sujeta" es el escalar secreto; la alta, el prefijo. */
static void expandir(uint8_t d[64], const uint8_t semente[32])
{
    sha512_t c;
    sha512_iniciar(&c);
    sha512_somar(&c, semente, 32);
    sha512_fim(&c, d);
    d[0] &= 248;
    d[31] &= 127;
    d[31] |= 64;
}

void ed25519_publica(uint8_t publica[32], const uint8_t semente[32])
{
    uint8_t d[64];
    ponto p;
    expandir(d, semente);
    ponto_mul_base(p, d);
    ponto_gravar(publica, p);
    cripto_apagar(d, sizeof d);
    cripto_apagar(p, sizeof p);
}

void ed25519_assinar(uint8_t assin[64], const void *msg, unsigned n, const uint8_t semente[32], const uint8_t publica[32])
{
    uint8_t d[64], h[64], r[32], k[32];
    int64_t x[64];
    ponto p;
    sha512_t c;
    expandir(d, semente);
    /* r = SHA-512(prefijo || mensaje) mod L;  R = r * B */
    sha512_iniciar(&c);
    sha512_somar(&c, d + 32, 32);
    sha512_somar(&c, msg, n);
    sha512_fim(&c, h);
    reduzir_hash(r, h);
    ponto_mul_base(p, r);
    ponto_gravar(assin, p);
    /* k = SHA-512(R || A || mensaje) mod L;  S = r + k * a mod L */
    sha512_iniciar(&c);
    sha512_somar(&c, assin, 32);
    sha512_somar(&c, publica, 32);
    sha512_somar(&c, msg, n);
    sha512_fim(&c, h);
    reduzir_hash(k, h);
    for (int i = 0; i < 64; i++) x[i] = 0;
    for (int i = 0; i < 32; i++) x[i] = r[i];
    for (int i = 0; i < 32; i++)
        for (int j = 0; j < 32; j++) x[i + j] += (int64_t)k[i] * (int64_t)d[j];
    reduzir_l(assin + 32, x);
    cripto_apagar(d, sizeof d);
    cripto_apagar(h, sizeof h);
    cripto_apagar(r, sizeof r);
    cripto_apagar(x, sizeof x);
    cripto_apagar(p, sizeof p);
    cripto_apagar(&c, sizeof c);
}

/* z^((p-5)/8) = z^(2^252 - 3) */
static void fe_pot2523(fe r, const fe z)
{
    fe c;
    fe_copiar(c, z);
    for (int i = 250; i >= 0; i--) {
        fe_quad(c, c);
        if (i != 1) fe_mul(c, c, z);
    }
    fe_copiar(r, c);
}

/* Descomprime un punto y lo deja negado (-A), que es lo que usa la
 * verificación. false = no es un punto de la curva. */
static bool ponto_ler_negado(ponto r, const uint8_t b[32])
{
    fe t, chk, num, den, den2, den4, den6;
    fe_copiar(r[2], fe_um);
    fe_ler256(r[1], b);
    fe_quad(num, r[1]);
    fe_mul(den, num, ed_d);
    fe_sub(num, num, r[2]);     /* y^2 - 1 */
    fe_soma(den, r[2], den);    /* d y^2 + 1 */
    fe_quad(den2, den);
    fe_quad(den4, den2);
    fe_mul(den6, den4, den2);
    fe_mul(t, den6, num);
    fe_mul(t, t, den);
    fe_pot2523(t, t);
    fe_mul(t, t, num);
    fe_mul(t, t, den);
    fe_mul(t, t, den);
    fe_mul(r[0], t, den);       /* candidata a x */
    fe_quad(chk, r[0]);
    fe_mul(chk, chk, den);
    if (!fe_igual(chk, num)) fe_mul(r[0], r[0], raiz_m1);
    fe_quad(chk, r[0]);
    fe_mul(chk, chk, den);
    if (!fe_igual(chk, num)) return false;
    if (fe_paridade(r[0]) == (b[31] >> 7)) {
        fe zero = { 0 };
        fe_sub(r[0], zero, r[0]);
    }
    fe_mul(r[3], r[0], r[1]);
    return true;
}

bool ed25519_verificar(const uint8_t assin[64], const void *msg, unsigned n, const uint8_t publica[32])
{
    uint8_t h[64], k[32], t[32];
    ponto p, q, a;
    sha512_t c;
    /* S tiene que ser menor que L (si no, la firma sería maleable) */
    int menor = 0, igual = 1;
    for (int i = 31; i >= 0; i--) {
        if (igual && assin[32 + i] < ordem[i]) { menor = 1; igual = 0; }
        else if (igual && assin[32 + i] > ordem[i]) igual = 0;
    }
    if (!menor) return false;
    if (!ponto_ler_negado(a, publica)) return false;
    sha512_iniciar(&c);
    sha512_somar(&c, assin, 32);
    sha512_somar(&c, publica, 32);
    sha512_somar(&c, msg, n);
    sha512_fim(&c, h);
    reduzir_hash(k, h);
    ponto_mul(p, a, k);                 /* k * (-A) */
    ponto_mul_base(q, assin + 32);      /* S * B */
    ponto_somar(p, q);
    ponto_gravar(t, p);
    return cripto_igual(t, assin, 32);  /* == R */
}
