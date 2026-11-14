// SPDX-License-Identifier: GPL-2.0-or-later
/* p256.c - la curva NIST P-256 para el SAE de WPA3 (D220). Ver p256.h.
 *
 * Escrito a partir de FIPS 186-4 (D.1.2.3, los parámetros), la multiplicación
 * de Montgomery (CIOS) y las fórmulas de la «Explicit-Formulas Database»
 * (dbl-2001-b con a = -3 y add-2007-bl), como userland/libtls/ecc.c (D060), y
 * de IEEE 802.11-2020 (12.4.4.2.3) y RFC 9380 (6.6.2) para el mapa SSWU.
 *
 * Los números son 8 miembros de 32 bits, del menos significativo al más. La
 * suma, la resta y la multiplicación modulares no tienen ramas que dependan de
 * los datos; la multiplicación de un punto es una escalera de Montgomery
 * (misma secuencia de operaciones para cualquier escalar) y solo los casos
 * especiales de la suma (el infinito, un punto consigo mismo) usan ramas, que
 * con escalares aleatorios no salen nunca salvo en los primeros pasos. */
#include "p256.h"

#define N 8

typedef struct {
    uint32_t m[N];      /* módulo */
    uint32_t r2[N];     /* R^2 mod m (R = 2^256) */
    uint32_t um[N];     /* R mod m: el 1 en la forma de Montgomery */
    uint32_t m0inv;     /* -m^-1 mod 2^32 */
} mod_t;

static const uint8_t P_BYTES[32] = {
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff };
static const uint8_t N_BYTES[32] = {
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xbc,0xe6,0xfa,0xad,0xa7,0x17,0x9e,0x84,0xf3,0xb9,0xca,0xc2,0xfc,0x63,0x25,0x51 };
static const uint8_t B_BYTES[32] = {
    0x5a,0xc6,0x35,0xd8,0xaa,0x3a,0x93,0xe7,0xb3,0xeb,0xbd,0x55,0x76,0x98,0x86,0xbc,
    0x65,0x1d,0x06,0xb0,0xcc,0x53,0xb0,0xf6,0x3b,0xce,0x3c,0x3e,0x27,0xd2,0x60,0x4b };
static const uint8_t GX_BYTES[32] = {
    0x6b,0x17,0xd1,0xf2,0xe1,0x2c,0x42,0x47,0xf8,0xbc,0xe6,0xe5,0x63,0xa4,0x40,0xf2,
    0x77,0x03,0x7d,0x81,0x2d,0xeb,0x33,0xa0,0xf4,0xa1,0x39,0x45,0xd8,0x98,0xc2,0x96 };
static const uint8_t GY_BYTES[32] = {
    0x4f,0xe3,0x42,0xe2,0xfe,0x1a,0x7f,0x9b,0x8e,0xe7,0xeb,0x4a,0x7c,0x0f,0x9e,0x16,
    0x2b,0xce,0x33,0x57,0x6b,0x31,0x5e,0xce,0xcb,0xb6,0x40,0x68,0x37,0xbf,0x51,0xf5 };

static mod_t P, ORD;
static uint32_t B_M[N], GX_M[N], GY_M[N];   /* b y el generador, en la forma de Montgomery */
static uint8_t P_MENOS_2[32], P_MAIS_1_4[32], P_MENOS_1_2[32];
static bool pronto;

/* ------------------------------------------------------------ enteros de 256 bits */

static void de_bytes(uint32_t r[N], const uint8_t b[32])
{
    for (int i = 0; i < N; i++)
        r[i] = (uint32_t)b[31 - 4 * i] | (uint32_t)b[30 - 4 * i] << 8 | (uint32_t)b[29 - 4 * i] << 16 |
               (uint32_t)b[28 - 4 * i] << 24;
}

static void para_bytes(uint8_t b[32], const uint32_t a[N])
{
    for (int i = 0; i < N; i++) {
        b[31 - 4 * i] = (uint8_t)a[i];
        b[30 - 4 * i] = (uint8_t)(a[i] >> 8);
        b[29 - 4 * i] = (uint8_t)(a[i] >> 16);
        b[28 - 4 * i] = (uint8_t)(a[i] >> 24);
    }
}

static void copiar(uint32_t r[N], const uint32_t a[N]) { for (int i = 0; i < N; i++) r[i] = a[i]; }
static void zerar(uint32_t r[N]) { for (int i = 0; i < N; i++) r[i] = 0; }

static bool e_zero(const uint32_t a[N])
{
    uint32_t o = 0;
    for (int i = 0; i < N; i++) o |= a[i];
    return o == 0;
}

static bool iguais(const uint32_t a[N], const uint32_t b[N])
{
    uint32_t o = 0;
    for (int i = 0; i < N; i++) o |= a[i] ^ b[i];
    return o == 0;
}

static uint32_t somar(uint32_t r[N], const uint32_t a[N], const uint32_t b[N])
{
    uint64_t c = 0;
    for (int i = 0; i < N; i++) { c += (uint64_t)a[i] + b[i]; r[i] = (uint32_t)c; c >>= 32; }
    return (uint32_t)c;
}

static uint32_t subtrair(uint32_t r[N], const uint32_t a[N], const uint32_t b[N])
{
    uint32_t emp = 0;
    for (int i = 0; i < N; i++) {
        uint64_t d = (uint64_t)a[i] - b[i] - emp;
        r[i] = (uint32_t)d;
        emp = (uint32_t)(d >> 63);
    }
    return emp;
}

/* r = mascara ? a : b */
static void escolher(uint32_t r[N], uint32_t mascara, const uint32_t a[N], const uint32_t b[N])
{
    for (int i = 0; i < N; i++) r[i] = (a[i] & mascara) | (b[i] & ~mascara);
}

static int comparar(const uint32_t a[N], const uint32_t b[N])
{
    for (int i = N - 1; i >= 0; i--)
        if (a[i] != b[i]) return a[i] > b[i] ? 1 : -1;
    return 0;
}

/* ------------------------------------------------------------ aritmética modular */

static void mod_somar(const mod_t *c, uint32_t r[N], const uint32_t a[N], const uint32_t b[N])
{
    uint32_t s[N], d[N];
    uint32_t vai = somar(s, a, b);
    uint32_t emp = subtrair(d, s, c->m);
    escolher(r, (uint32_t)-(int32_t)(vai | (emp ^ 1)), d, s);
}

static void mod_subtrair(const mod_t *c, uint32_t r[N], const uint32_t a[N], const uint32_t b[N])
{
    uint32_t s[N], d[N];
    uint32_t emp = subtrair(s, a, b);
    somar(d, s, c->m);
    escolher(r, (uint32_t)-(int32_t)emp, d, s);
}

/* Montgomery (CIOS): r = a·b·R^-1 mod m; a, b < m; r puede ser a o b. */
static void mont_mul(const mod_t *c, uint32_t r[N], const uint32_t a[N], const uint32_t b[N])
{
    uint32_t t[N + 2];
    for (int i = 0; i < N + 2; i++) t[i] = 0;
    for (int i = 0; i < N; i++) {
        uint64_t s = 0;
        for (int j = 0; j < N; j++) {
            s = (uint64_t)t[j] + (uint64_t)a[j] * b[i] + (s >> 32);
            t[j] = (uint32_t)s;
        }
        s = (uint64_t)t[N] + (s >> 32);
        t[N] = (uint32_t)s;
        t[N + 1] = (uint32_t)(s >> 32);
        uint32_t mm = t[0] * c->m0inv;
        s = (uint64_t)t[0] + (uint64_t)mm * c->m[0];
        for (int j = 1; j < N; j++) {
            s = (uint64_t)t[j] + (uint64_t)mm * c->m[j] + (s >> 32);
            t[j - 1] = (uint32_t)s;
        }
        s = (uint64_t)t[N] + (s >> 32);
        t[N - 1] = (uint32_t)s;
        t[N] = t[N + 1] + (uint32_t)(s >> 32);
    }
    uint32_t d[N];
    uint32_t emp = subtrair(d, t, c->m);
    /* t < 2m: se resta m si t >= m (sin empresta-uno, o con el bit 256 encendido) */
    escolher(r, (uint32_t)-(int32_t)(t[N] | (emp ^ 1)), d, t);
}

static void mont_para(const mod_t *c, uint32_t r[N], const uint32_t a[N]) { mont_mul(c, r, a, c->r2); }

static void mont_de(const mod_t *c, uint32_t r[N], const uint32_t a[N])
{
    uint32_t um[N];
    zerar(um);
    um[0] = 1;
    mont_mul(c, r, a, um);
}

/* r = a^e en la forma de Montgomery (e público, 32 bytes big-endian). */
static void mont_potencia(const mod_t *c, uint32_t r[N], const uint32_t a[N], const uint8_t e[32])
{
    uint32_t x[N], b[N];
    copiar(x, c->um);
    copiar(b, a);
    for (int i = 0; i < 256; i++) {
        mont_mul(c, x, x, x);
        if ((e[i / 8] >> (7 - i % 8)) & 1) mont_mul(c, x, x, b);
    }
    copiar(r, x);
}

static void mod_preparar(mod_t *c, const uint8_t m[32])
{
    de_bytes(c->m, m);
    /* -m^-1 mod 2^32 por Newton: cada paso dobla los bits correctos */
    uint32_t inv = 1;
    for (int i = 0; i < 5; i++) inv *= 2 - c->m[0] * inv;
    c->m0inv = (uint32_t)-inv;
    /* R mod m y R^2 mod m doblando: 1 -> 2^256 -> 2^512 */
    uint32_t x[N];
    zerar(x);
    x[0] = 1;
    for (int i = 0; i < 512; i++) {
        mod_somar(c, x, x, x);
        if (i == 255) copiar(c->um, x);
    }
    copiar(c->r2, x);
}

/* x - k en bytes big-endian (sin empréstito más allá del primer byte). */
static void bytes_menos(uint8_t d[32], const uint8_t x[32], unsigned k)
{
    unsigned emp = k;
    for (int i = 31; i >= 0; i--) {
        int v = (int)x[i] - (int)emp;
        emp = v < 0;
        d[i] = (uint8_t)(v < 0 ? v + 256 : v);
    }
}

/* x >> s (s < 8) en bytes big-endian. */
static void bytes_dir(uint8_t d[32], const uint8_t x[32], int s)
{
    for (int i = 31; i >= 0; i--) d[i] = (uint8_t)((x[i] >> s) | (i ? x[i - 1] << (8 - s) : 0));
}

static void preparar(void)
{
    if (pronto) return;
    mod_preparar(&P, P_BYTES);
    mod_preparar(&ORD, N_BYTES);
    uint32_t t[N];
    de_bytes(t, B_BYTES);  mont_para(&P, B_M, t);
    de_bytes(t, GX_BYTES); mont_para(&P, GX_M, t);
    de_bytes(t, GY_BYTES); mont_para(&P, GY_M, t);
    bytes_menos(P_MENOS_2, P_BYTES, 2);
    /* p = 3 (mod 4): (p + 1) / 4 = (p >> 2) + 1, y (p - 1) / 2 = p >> 1 */
    uint8_t q[32];
    bytes_dir(q, P_BYTES, 2);
    for (int i = 31; i >= 0; i--) if (++q[i]) break;
    for (int i = 0; i < 32; i++) P_MAIS_1_4[i] = q[i];
    bytes_dir(P_MENOS_1_2, P_BYTES, 1);
    pronto = true;
}

#define FMUL(r, a, b) mont_mul(&P, r, a, b)
#define FSOMA(r, a, b) mod_somar(&P, r, a, b)
#define FSUB(r, a, b) mod_subtrair(&P, r, a, b)

static void finv(uint32_t r[N], const uint32_t a[N]) { mont_potencia(&P, r, a, P_MENOS_2); }

/* y^2 = x^3 - 3x + b (todo en la forma de Montgomery) */
static void lado_direito(uint32_t r[N], const uint32_t x[N])
{
    uint32_t t[N], x3[N];
    FMUL(t, x, x);
    FMUL(x3, t, x);
    FSOMA(t, x, x);
    FSOMA(t, t, x);
    FSUB(x3, x3, t);
    FSOMA(r, x3, B_M);
}

/* ¿a es un cuadrado módulo p? (Euler: a^((p-1)/2) = 1; el 0 también vale) */
static bool quadrado(const uint32_t a[N])
{
    uint32_t e[N];
    mont_potencia(&P, e, a, P_MENOS_1_2);
    return iguais(e, P.um) || e_zero(a);
}

/* ------------------------------------------------------------ puntos */

bool p256_infinito(const p256_ponto_t *p) { return e_zero(p->z); }

void p256_gerador(p256_ponto_t *p)
{
    preparar();
    copiar(p->x, GX_M);
    copiar(p->y, GY_M);
    copiar(p->z, P.um);
}

static void dobrar(p256_ponto_t *r, const p256_ponto_t *p)
{
    uint32_t delta[N], gama[N], beta[N], alfa[N], t1[N], t2[N];
    FMUL(delta, p->z, p->z);
    FMUL(gama, p->y, p->y);
    FMUL(beta, p->x, gama);
    FSUB(t1, p->x, delta);
    FSOMA(t2, p->x, delta);
    FMUL(alfa, t1, t2);
    FSOMA(t1, alfa, alfa);
    FSOMA(alfa, t1, alfa);               /* alfa = 3 (X - delta)(X + delta) */
    FSOMA(t1, p->y, p->z);
    FMUL(t1, t1, t1);
    FSUB(t1, t1, gama);
    FSUB(r->z, t1, delta);               /* Z3 = (Y + Z)^2 - gama - delta */
    FSOMA(beta, beta, beta);
    FSOMA(beta, beta, beta);             /* 4 beta */
    FMUL(t1, alfa, alfa);
    FSUB(t1, t1, beta);
    FSUB(r->x, t1, beta);                /* X3 = alfa^2 - 8 beta */
    FSUB(t1, beta, r->x);
    FMUL(t1, alfa, t1);
    FMUL(t2, gama, gama);
    FSOMA(t2, t2, t2);
    FSOMA(t2, t2, t2);
    FSOMA(t2, t2, t2);                   /* 8 gama^2 */
    FSUB(r->y, t1, t2);                  /* Y3 = alfa (4 beta - X3) - 8 gama^2 */
}

void p256_somar(p256_ponto_t *r, const p256_ponto_t *a, const p256_ponto_t *b)
{
    preparar();
    if (p256_infinito(a)) { *r = *b; return; }
    if (p256_infinito(b)) { *r = *a; return; }
    uint32_t z1z1[N], z2z2[N], u1[N], u2[N], s1[N], s2[N], h[N], i[N], j[N], rr[N], v[N], t[N];
    FMUL(z1z1, a->z, a->z);
    FMUL(z2z2, b->z, b->z);
    FMUL(u1, a->x, z2z2);
    FMUL(u2, b->x, z1z1);
    FMUL(t, b->z, z2z2);
    FMUL(s1, a->y, t);
    FMUL(t, a->z, z1z1);
    FMUL(s2, b->y, t);
    FSUB(h, u2, u1);
    FSUB(rr, s2, s1);
    if (e_zero(h)) {
        if (e_zero(rr)) { dobrar(r, a); return; }    /* el mismo punto */
        zerar(r->x); zerar(r->y); zerar(r->z);       /* a = -b: el infinito */
        r->x[0] = 1; r->y[0] = 1;
        return;
    }
    FSOMA(rr, rr, rr);                   /* r = 2 (S2 - S1) */
    FSOMA(i, h, h);
    FMUL(i, i, i);                       /* I = (2H)^2 */
    FMUL(j, h, i);                       /* J = H·I */
    FMUL(v, u1, i);                      /* V = U1·I */
    p256_ponto_t o;
    FMUL(t, rr, rr);
    FSUB(t, t, j);
    FSUB(t, t, v);
    FSUB(o.x, t, v);                     /* X3 = r^2 - J - 2V */
    FSUB(t, v, o.x);
    FMUL(t, rr, t);
    FMUL(s1, s1, j);
    FSOMA(s1, s1, s1);
    FSUB(o.y, t, s1);                    /* Y3 = r (V - X3) - 2 S1 J */
    FSOMA(t, a->z, b->z);
    FMUL(t, t, t);
    FSUB(t, t, z1z1);
    FSUB(t, t, z2z2);
    FMUL(o.z, t, h);                     /* Z3 = ((Z1 + Z2)^2 - Z1Z1 - Z2Z2) H */
    *r = o;
}

void p256_negar(p256_ponto_t *r, const p256_ponto_t *a)
{
    uint32_t zero[N];
    preparar();
    zerar(zero);
    copiar(r->x, a->x);
    copiar(r->z, a->z);
    FSUB(r->y, zero, a->y);
}

/* Cambia a y b si bit = 1, sin ramas. */
static void trocar(p256_ponto_t *a, p256_ponto_t *b, uint32_t bit)
{
    uint32_t m = (uint32_t)-(int32_t)bit;
    uint32_t *x = (uint32_t *)a, *y = (uint32_t *)b;
    for (unsigned i = 0; i < sizeof *a / 4; i++) {
        uint32_t t = (x[i] ^ y[i]) & m;
        x[i] ^= t;
        y[i] ^= t;
    }
}

void p256_mult(p256_ponto_t *r, const p256_ponto_t *a, const uint8_t k[P256_TAM])
{
    preparar();
    p256_ponto_t r0, r1;
    zerar(r0.x); zerar(r0.y); zerar(r0.z);
    r0.x[0] = r0.y[0] = 1;
    r1 = *a;
    for (int i = 0; i < 256; i++) {
        uint32_t bit = (k[i / 8] >> (7 - i % 8)) & 1;
        trocar(&r0, &r1, bit);
        p256_somar(&r1, &r0, &r1);
        dobrar(&r0, &r0);
        trocar(&r0, &r1, bit);
    }
    *r = r0;
    uint32_t *z = (uint32_t *)&r1;
    for (unsigned i = 0; i < sizeof r1 / 4; i++) z[i] = 0;
}

bool p256_ler(p256_ponto_t *p, const uint8_t xy[64])
{
    preparar();
    uint32_t x[N], y[N], t[N], y2[N];
    de_bytes(x, xy);
    de_bytes(y, xy + 32);
    if (comparar(x, P.m) >= 0 || comparar(y, P.m) >= 0) return false;
    mont_para(&P, p->x, x);
    mont_para(&P, p->y, y);
    copiar(p->z, P.um);
    lado_direito(t, p->x);
    FMUL(y2, p->y, p->y);
    return iguais(t, y2);
}

bool p256_gravar(const p256_ponto_t *p, uint8_t xy[64])
{
    preparar();
    if (p256_infinito(p)) return false;
    uint32_t zi[N], zi2[N], t[N];
    finv(zi, p->z);
    FMUL(zi2, zi, zi);
    FMUL(t, p->x, zi2);
    mont_de(&P, t, t);
    para_bytes(xy, t);
    FMUL(zi2, zi2, zi);
    FMUL(t, p->y, zi2);
    mont_de(&P, t, t);
    para_bytes(xy + 32, t);
    return true;
}

/* ------------------------------------------------------------ escalares */

bool p256_escalar_valido(const uint8_t k[32])
{
    preparar();
    uint32_t a[N], um[N];
    de_bytes(a, k);
    zerar(um);
    um[0] = 1;
    return comparar(a, um) > 0 && comparar(a, ORD.m) < 0;
}

void p256_somar_n(uint8_t r[32], const uint8_t a[32], const uint8_t b[32])
{
    preparar();
    uint32_t x[N], y[N];
    de_bytes(x, a);
    de_bytes(y, b);
    mod_somar(&ORD, x, x, y);
    para_bytes(r, x);
}

void p256_mod_n1_mais1(uint8_t r[32], const uint8_t v[32])
{
    preparar();
    uint32_t x[N], n1[N], d[N], um[N];
    de_bytes(x, v);
    zerar(um);
    um[0] = 1;
    subtrair(n1, ORD.m, um);
    uint32_t emp = subtrair(d, x, n1);            /* v < 2^256 < 2 (n - 1): basta una resta */
    escolher(x, (uint32_t)-(int32_t)(emp ^ 1), d, x);
    somar(x, x, um);
    para_bytes(r, x);
}

bool p256_menor_que_p(const uint8_t x[32])
{
    preparar();
    uint32_t a[N];
    de_bytes(a, x);
    return comparar(a, P.m) < 0;
}

/* ------------------------------------------------------------ SAE */

/* Raíz cuadrada (p = 3 mod 4): a^((p+1)/4). */
static void fraiz(uint32_t r[N], const uint32_t a[N]) { mont_potencia(&P, r, a, P_MAIS_1_4); }

/* Paridad de un elemento en la forma de Montgomery (la de su valor normal). */
static int paridade(const uint32_t a[N])
{
    uint32_t t[N];
    mont_de(&P, t, a);
    return (int)(t[0] & 1);
}

bool p256_de_x(p256_ponto_t *p, const uint8_t xb[32], int par)
{
    preparar();
    uint32_t x[N], v[N], y[N];
    de_bytes(x, xb);
    if (comparar(x, P.m) >= 0) return false;
    mont_para(&P, p->x, x);
    lado_direito(v, p->x);
    if (!quadrado(v)) return false;
    fraiz(y, v);
    uint32_t zero[N], ny[N];
    zerar(zero);
    FSUB(ny, zero, y);
    escolher(p->y, (uint32_t)-(int32_t)(paridade(y) == (par & 1)), y, ny);
    copiar(p->z, P.um);
    return true;
}

/* valor (big-endian, hasta 64 bytes) mod p, en la forma normal:
 * valor = alto·2^256 + bajo -> alto·R mod p (Montgomery con R^2) + bajo mod p. */
static void reduzir_p(uint32_t r[N], const uint8_t *v, uint32_t nv)
{
    uint8_t alto[32], baixo[32];
    for (int i = 0; i < 32; i++) { alto[i] = 0; baixo[i] = 0; }
    uint32_t nb = nv < 32 ? nv : 32;
    for (uint32_t i = 0; i < nb; i++) baixo[32 - nb + i] = v[nv - nb + i];
    for (uint32_t i = 0; i + nb < nv; i++) alto[32 - (nv - nb) + i] = v[i];
    uint32_t a[N], b[N], d[N];
    de_bytes(a, alto);
    de_bytes(b, baixo);
    mont_mul(&P, a, a, P.r2);                   /* con a < 2^256 y R^2 < p, el resultado ya es < 2p y se reduce */
    uint32_t emp = subtrair(d, b, P.m);
    escolher(b, (uint32_t)-(int32_t)(emp ^ 1), d, b);
    mod_somar(&P, r, a, b);
}

static void pequeno(uint32_t r[N], uint32_t v)
{
    uint32_t t[N];
    zerar(t);
    t[0] = v;
    mont_para(&P, r, t);
}

void p256_sswu(p256_ponto_t *p, const uint8_t *valor, uint32_t nvalor)
{
    preparar();
    uint32_t u[N], un[N], zero[N], z[N], a[N], t[N], u2[N], m[N], tinv[N], x1[N], x2[N], gx1[N], gx2[N], y[N], ny[N];
    zerar(zero);
    reduzir_p(un, valor, nvalor);
    mont_para(&P, u, un);
    pequeno(t, 10);
    FSUB(z, zero, t);                            /* z = -10 */
    pequeno(t, 3);
    FSUB(a, zero, t);                            /* a = -3 */
    /* m = z^2 u^4 + z u^2 */
    FMUL(u2, u, u);
    FMUL(t, z, u2);                              /* z u^2 */
    FMUL(m, t, t);
    FSOMA(m, m, t);
    bool m_zero = e_zero(m);
    finv(tinv, m);                               /* 0^(p-2) = 0 */
    /* x1 = m == 0 ? b / (z a) : (-b / a)(1 + t) */
    uint32_t c1[N], c2[N], inv[N];
    FMUL(t, z, a);
    finv(inv, t);
    FMUL(c1, B_M, inv);                          /* b / (z a) */
    finv(inv, a);
    FMUL(c2, B_M, inv);
    FSUB(c2, zero, c2);                          /* -b / a */
    FSOMA(t, P.um, tinv);
    FMUL(c2, c2, t);
    escolher(x1, (uint32_t)-(int32_t)m_zero, c1, c2);
    lado_direito(gx1, x1);
    FMUL(t, z, u2);
    FMUL(x2, t, x1);                             /* x2 = z u^2 x1 */
    lado_direito(gx2, x2);
    bool l = quadrado(gx1);
    uint32_t v[N];
    escolher(v, (uint32_t)-(int32_t)l, gx1, gx2);
    escolher(p->x, (uint32_t)-(int32_t)l, x1, x2);
    fraiz(y, v);
    FSUB(ny, zero, y);
    escolher(p->y, (uint32_t)-(int32_t)((int)(un[0] & 1) == paridade(y)), y, ny);
    copiar(p->z, P.um);
}
