// SPDX-License-Identifier: GPL-2.0-or-later
/* ecc.c - curvas P-256 e P-384 (FIPS 186-4, D.1.2.3 e D.1.2.4; SEC1): ECDSA
 * (só verificação) e ECDH. D060.
 *
 * Coordenadas jacobianas (X, Y, Z) = (X/Z^2, Y/Z^3) na forma de Montgomery:
 * nenhuma divisão até o fim. Fórmulas da "Explicit-Formulas Database"
 * (dbl-2001-b com a = -3 e add-2007-bl), deduzidas das definições de
 * adição de pontos.
 *
 * O ECDH usa a chave privada: escada de Montgomery com troca por máscara e
 * o escalar completado (k + n ou k + 2n) para ter sempre o bit mais alto no
 * mesmo lugar, então o número de passos e as operações não dependem de k.
 * A verificação ECDSA (u1*G + u2*Q) só lida com valores públicos e usa o
 * método de Shamir, de tempo variável. */
#include "bignum.h"

#define EC_MAX 12   /* membros: 384 bits */

typedef struct {
    int      id;
    unsigned tam;          /* bytes por coordenada */
    int      n;            /* membros */
    mont_t   p, ordem;
    uint32_t b[EC_MAX];    /* forma de Montgomery (de p) */
    uint32_t gx[EC_MAX], gy[EC_MAX];
    uint8_t  p_menos_2[48], n_menos_2[48];
    uint8_t  n_bytes[48];
    bool     pronta;
} curva_t;

typedef struct { uint32_t x[EC_MAX], y[EC_MAX], z[EC_MAX]; } ponto_t;

static const uint8_t P256_P[] = {
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff };
static const uint8_t P256_N[] = {
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xbc,0xe6,0xfa,0xad,0xa7,0x17,0x9e,0x84,0xf3,0xb9,0xca,0xc2,0xfc,0x63,0x25,0x51 };
static const uint8_t P256_B[] = {
    0x5a,0xc6,0x35,0xd8,0xaa,0x3a,0x93,0xe7,0xb3,0xeb,0xbd,0x55,0x76,0x98,0x86,0xbc,
    0x65,0x1d,0x06,0xb0,0xcc,0x53,0xb0,0xf6,0x3b,0xce,0x3c,0x3e,0x27,0xd2,0x60,0x4b };
static const uint8_t P256_GX[] = {
    0x6b,0x17,0xd1,0xf2,0xe1,0x2c,0x42,0x47,0xf8,0xbc,0xe6,0xe5,0x63,0xa4,0x40,0xf2,
    0x77,0x03,0x7d,0x81,0x2d,0xeb,0x33,0xa0,0xf4,0xa1,0x39,0x45,0xd8,0x98,0xc2,0x96 };
static const uint8_t P256_GY[] = {
    0x4f,0xe3,0x42,0xe2,0xfe,0x1a,0x7f,0x9b,0x8e,0xe7,0xeb,0x4a,0x7c,0x0f,0x9e,0x16,
    0x2b,0xce,0x33,0x57,0x6b,0x31,0x5e,0xce,0xcb,0xb6,0x40,0x68,0x37,0xbf,0x51,0xf5 };

static const uint8_t P384_P[] = {
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff };
static const uint8_t P384_N[] = {
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xc7,0x63,0x4d,0x81,0xf4,0x37,0x2d,0xdf,
    0x58,0x1a,0x0d,0xb2,0x48,0xb0,0xa7,0x7a,0xec,0xec,0x19,0x6a,0xcc,0xc5,0x29,0x73 };
static const uint8_t P384_B[] = {
    0xb3,0x31,0x2f,0xa7,0xe2,0x3e,0xe7,0xe4,0x98,0x8e,0x05,0x6b,0xe3,0xf8,0x2d,0x19,
    0x18,0x1d,0x9c,0x6e,0xfe,0x81,0x41,0x12,0x03,0x14,0x08,0x8f,0x50,0x13,0x87,0x5a,
    0xc6,0x56,0x39,0x8d,0x8a,0x2e,0xd1,0x9d,0x2a,0x85,0xc8,0xed,0xd3,0xec,0x2a,0xef };
static const uint8_t P384_GX[] = {
    0xaa,0x87,0xca,0x22,0xbe,0x8b,0x05,0x37,0x8e,0xb1,0xc7,0x1e,0xf3,0x20,0xad,0x74,
    0x6e,0x1d,0x3b,0x62,0x8b,0xa7,0x9b,0x98,0x59,0xf7,0x41,0xe0,0x82,0x54,0x2a,0x38,
    0x55,0x02,0xf2,0x5d,0xbf,0x55,0x29,0x6c,0x3a,0x54,0x5e,0x38,0x72,0x76,0x0a,0xb7 };
static const uint8_t P384_GY[] = {
    0x36,0x17,0xde,0x4a,0x96,0x26,0x2c,0x6f,0x5d,0x9e,0x98,0xbf,0x92,0x92,0xdc,0x29,
    0xf8,0xf4,0x1d,0xbd,0x28,0x9a,0x14,0x7c,0xe9,0xda,0x31,0x13,0xb5,0xf0,0xb8,0xc0,
    0x0a,0x60,0xb1,0xce,0x1d,0x7e,0x81,0x9d,0x7a,0x43,0x1d,0x7c,0x90,0xea,0x0e,0x5f };

static curva_t curvas[2];

/* x - 2 em bytes big-endian (x ímpar e > 2: não há empresta-um além do último byte) */
static void menos_dois(uint8_t *d, const uint8_t *x, unsigned n)
{
    memcpy(d, x, n);
    unsigned i = n - 1;
    unsigned v = d[i];
    if (v >= 2) { d[i] = (uint8_t)(v - 2); return; }
    d[i] = (uint8_t)(v + 256 - 2);
    while (i-- > 0) { if (d[i]--) break; }
}

static const curva_t *curva(int id)
{
    int k = id == CURVA_P256 ? 0 : id == CURVA_P384 ? 1 : -1;
    if (k < 0) return NULL;
    curva_t *c = &curvas[k];
    if (c->pronta) return c;
    const uint8_t *p = k ? P384_P : P256_P, *n = k ? P384_N : P256_N, *b = k ? P384_B : P256_B;
    const uint8_t *gx = k ? P384_GX : P256_GX, *gy = k ? P384_GY : P256_GY;
    c->id = id;
    c->tam = k ? 48 : 32;
    c->n = (int)c->tam / 4;
    mont_preparar(&c->p, p, c->tam);
    mont_preparar(&c->ordem, n, c->tam);
    uint32_t t[EC_MAX];
    bn_de_bytes(t, c->n, b, c->tam);  mont_para(&c->p, c->b, t);
    bn_de_bytes(t, c->n, gx, c->tam); mont_para(&c->p, c->gx, t);
    bn_de_bytes(t, c->n, gy, c->tam); mont_para(&c->p, c->gy, t);
    menos_dois(c->p_menos_2, p, c->tam);
    menos_dois(c->n_menos_2, n, c->tam);
    memcpy(c->n_bytes, n, c->tam);
    c->pronta = true;
    return c;
}

unsigned ec_tam(int id)
{
    return id == CURVA_P256 ? 32 : id == CURVA_P384 ? 48 : 0;
}

/* ------------------------------------------------------------ campo */

#define MUL(r, a, b) mont_mul(&c->p, r, a, b)
#define SOMA(r, a, b) mod_somar(&c->p, r, a, b)
#define SUB(r, a, b) mod_subtrair(&c->p, r, a, b)

static void copiar(const curva_t *c, uint32_t *r, const uint32_t *a) { memcpy(r, a, (unsigned)c->n * 4); }

/* (X, Y, Z) -> 2 (X, Y, Z); a = -3 */
static void dobrar(const curva_t *c, ponto_t *r, const ponto_t *p)
{
    uint32_t delta[EC_MAX], gama[EC_MAX], beta[EC_MAX], alfa[EC_MAX], t1[EC_MAX], t2[EC_MAX];
    MUL(delta, p->z, p->z);
    MUL(gama, p->y, p->y);
    MUL(beta, p->x, gama);
    SUB(t1, p->x, delta);
    SOMA(t2, p->x, delta);
    MUL(alfa, t1, t2);
    SOMA(t1, alfa, alfa);
    SOMA(alfa, t1, alfa);                 /* alfa = 3 (X - delta)(X + delta) */
    /* Z3 = (Y + Z)^2 - gama - delta (antes de mexer em r: r pode ser p) */
    SOMA(t1, p->y, p->z);
    MUL(t1, t1, t1);
    SUB(t1, t1, gama);
    SUB(r->z, t1, delta);
    /* X3 = alfa^2 - 8 beta */
    SOMA(beta, beta, beta);
    SOMA(beta, beta, beta);               /* 4 beta */
    MUL(t1, alfa, alfa);
    SUB(t1, t1, beta);
    SUB(r->x, t1, beta);
    /* Y3 = alfa (4 beta - X3) - 8 gama^2 */
    SUB(t1, beta, r->x);
    MUL(t1, alfa, t1);
    MUL(t2, gama, gama);
    SOMA(t2, t2, t2);
    SOMA(t2, t2, t2);
    SOMA(t2, t2, t2);
    SUB(r->y, t1, t2);
}

/* r = p + q (add-2007-bl). Devolve false se H = 0 (pontos iguais ou opostos):
 * o chamador decide (na escada isso só acontece com probabilidade desprezível). */
static bool somar_pontos(const curva_t *c, ponto_t *r, const ponto_t *p, const ponto_t *q, bool *rr_zero)
{
    uint32_t z1z1[EC_MAX], z2z2[EC_MAX], u1[EC_MAX], u2[EC_MAX], s1[EC_MAX], s2[EC_MAX];
    uint32_t h[EC_MAX], i[EC_MAX], j[EC_MAX], rr[EC_MAX], v[EC_MAX], t[EC_MAX];
    MUL(z1z1, p->z, p->z);
    MUL(z2z2, q->z, q->z);
    MUL(u1, p->x, z2z2);
    MUL(u2, q->x, z1z1);
    MUL(s1, p->y, q->z);
    MUL(s1, s1, z2z2);
    MUL(s2, q->y, p->z);
    MUL(s2, s2, z1z1);
    SUB(h, u2, u1);
    SUB(rr, s2, s1);
    SOMA(rr, rr, rr);
    bool h_zero = bn_zero(h, c->n);
    if (rr_zero) *rr_zero = bn_zero(rr, c->n);
    SOMA(i, h, h);
    MUL(i, i, i);
    MUL(j, h, i);
    MUL(v, u1, i);
    /* Z3 = ((Z1 + Z2)^2 - Z1Z1 - Z2Z2) H */
    SOMA(t, p->z, q->z);
    MUL(t, t, t);
    SUB(t, t, z1z1);
    SUB(t, t, z2z2);
    MUL(r->z, t, h);
    /* X3 = r^2 - J - 2V */
    MUL(t, rr, rr);
    SUB(t, t, j);
    SUB(t, t, v);
    SUB(r->x, t, v);
    /* Y3 = r (V - X3) - 2 S1 J */
    SUB(t, v, r->x);
    MUL(t, rr, t);
    MUL(s1, s1, j);
    SOMA(s1, s1, s1);
    SUB(r->y, t, s1);
    return !h_zero;
}

static void inverso_campo(const curva_t *c, uint32_t *r, const uint32_t *a)
{
    mont_potencia(&c->p, r, a, c->p_menos_2, c->tam);
}

/* Jacobiano -> afim, na forma normal (fora de Montgomery). */
static void para_afim(const curva_t *c, const ponto_t *p, uint32_t *x, uint32_t *y)
{
    uint32_t zi[EC_MAX], zi2[EC_MAX], t[EC_MAX];
    inverso_campo(c, zi, p->z);
    MUL(zi2, zi, zi);
    MUL(t, p->x, zi2);
    mont_de(&c->p, x, t);
    MUL(zi2, zi2, zi);
    MUL(t, p->y, zi2);
    mont_de(&c->p, y, t);
}

/* Lê 0x04 || x || y e confere que o ponto está na curva: y^2 = x^3 - 3x + b.
 * Sem isso, um servidor malicioso poderia mandar um ponto de outra curva
 * (de ordem pequena) e descobrir a chave privada aos pedaços. */
static bool ler_ponto(const curva_t *c, const uint8_t *b, unsigned nb, ponto_t *p)
{
    if (nb != 1 + 2 * c->tam || b[0] != 4) return false;
    uint32_t x[EC_MAX], y[EC_MAX], t[EC_MAX], u[EC_MAX];
    bn_de_bytes(x, c->n, b + 1, c->tam);
    bn_de_bytes(y, c->n, b + 1 + c->tam, c->tam);
    if (bn_comparar(x, c->p.m, c->n) >= 0 || bn_comparar(y, c->p.m, c->n) >= 0) return false;
    mont_para(&c->p, p->x, x);
    mont_para(&c->p, p->y, y);
    copiar(c, p->z, c->p.um);
    MUL(t, p->y, p->y);                 /* y^2 */
    MUL(u, p->x, p->x);
    MUL(u, u, p->x);                    /* x^3 */
    SUB(u, u, p->x);
    SUB(u, u, p->x);
    SUB(u, u, p->x);                    /* x^3 - 3x */
    SOMA(u, u, c->b);
    return bn_comparar(t, u, c->n) == 0;
}

static void gravar_ponto(const curva_t *c, const ponto_t *p, uint8_t *b)
{
    uint32_t x[EC_MAX], y[EC_MAX];
    para_afim(c, p, x, y);
    b[0] = 4;
    bn_para_bytes(b + 1, c->tam, x, c->n);
    bn_para_bytes(b + 1 + c->tam, c->tam, y, c->n);
}

/* ------------------------------------------------------------ escada (ECDH) */

static void trocar_pontos(const curva_t *c, ponto_t *a, ponto_t *b, uint32_t bit)
{
    uint32_t m = (uint32_t)-(int32_t)bit;
    for (int i = 0; i < c->n; i++) {
        uint32_t t;
        t = m & (a->x[i] ^ b->x[i]); a->x[i] ^= t; b->x[i] ^= t;
        t = m & (a->y[i] ^ b->y[i]); a->y[i] ^= t; b->y[i] ^= t;
        t = m & (a->z[i] ^ b->z[i]); a->z[i] ^= t; b->z[i] ^= t;
    }
}

/* r = k * p, k < n e k != 0 (bytes big-endian com c->tam bytes). Tempo constante em k. */
static void multiplicar(const curva_t *c, ponto_t *r, const ponto_t *p, const uint8_t *k)
{
    int n = c->n;
    uint32_t kk[EC_MAX + 1], k1[EC_MAX + 1], k2[EC_MAX + 1], nn[EC_MAX + 1];
    bn_de_bytes(kk, n + 1, k, c->tam);
    memcpy(nn, c->ordem.m, (unsigned)n * 4);
    nn[n] = 0;
    bn_somar(k1, kk, nn, n + 1);   /* k + n */
    bn_somar(k2, k1, nn, n + 1);   /* k + 2n */
    /* Usa k + n se ele já tem o bit 32n; senão k + 2n (que tem). */
    uint32_t tem = k1[n] & 1;
    uint32_t m = (uint32_t)-(int32_t)tem;
    for (int i = 0; i <= n; i++) kk[i] = (k1[i] & m) | (k2[i] & ~m);
    ponto_t r0 = *p, r1;
    dobrar(c, &r1, p);
    for (int i = 32 * n - 1; i >= 0; i--) {
        uint32_t bit = (kk[i / 32] >> (i % 32)) & 1;
        trocar_pontos(c, &r0, &r1, bit);
        somar_pontos(c, &r1, &r0, &r1, NULL);
        dobrar(c, &r0, &r0);
        trocar_pontos(c, &r0, &r1, bit);
    }
    *r = r0;
    cripto_apagar(kk, sizeof kk);
    cripto_apagar(k1, sizeof k1);
    cripto_apagar(k2, sizeof k2);
    cripto_apagar(&r1, sizeof r1);
}

static bool escalar_valido(const curva_t *c, const uint8_t *k)
{
    uint32_t x[EC_MAX];
    bn_de_bytes(x, c->n, k, c->tam);
    return !bn_zero(x, c->n) && bn_comparar(x, c->ordem.m, c->n) < 0;
}

bool ec_multiplicar_base(int id, const uint8_t *escalar, uint8_t *publica)
{
    const curva_t *c = curva(id);
    if (!c || !escalar_valido(c, escalar)) return false;
    ponto_t g, r;
    copiar(c, g.x, c->gx);
    copiar(c, g.y, c->gy);
    copiar(c, g.z, c->p.um);
    multiplicar(c, &r, &g, escalar);
    gravar_ponto(c, &r, publica);
    return true;
}

bool ecdh_gerar(int id, uint8_t *privada, uint8_t *publica)
{
    const curva_t *c = curva(id);
    if (!c) return false;
    do cripto_aleatorio(privada, c->tam);
    while (!escalar_valido(c, privada));
    return ec_multiplicar_base(id, privada, publica);
}

bool ecdh_segredo(int id, const uint8_t *privada, const uint8_t *outro, unsigned np, uint8_t *segredo)
{
    const curva_t *c = curva(id);
    ponto_t q, r;
    if (!c || !escalar_valido(c, privada) || !ler_ponto(c, outro, np, &q)) return false;
    multiplicar(c, &r, &q, privada);
    if (bn_zero(r.z, c->n)) return false;
    uint32_t x[EC_MAX], y[EC_MAX];
    para_afim(c, &r, x, y);
    bn_para_bytes(segredo, c->tam, x, c->n);
    cripto_apagar(&r, sizeof r);
    return true;
}

/* ------------------------------------------------------------ ECDSA */

/* Soma completa (pontos no infinito = Z zero, iguais, opostos): para a verificação. */
static void somar_completo(const curva_t *c, ponto_t *r, const ponto_t *p, const ponto_t *q)
{
    if (bn_zero(p->z, c->n)) { *r = *q; return; }
    if (bn_zero(q->z, c->n)) { *r = *p; return; }
    bool rr_zero;
    ponto_t t;
    if (somar_pontos(c, &t, p, q, &rr_zero)) { *r = t; return; }
    if (rr_zero) dobrar(c, r, p);
    else memset(r, 0, sizeof *r);   /* P + (-P) = infinito */
}

bool ecdsa_verificar(int id, const uint8_t *pub, unsigned np, const uint8_t *hash, unsigned nh,
                     const uint8_t *rb, unsigned nr, const uint8_t *sb, unsigned ns)
{
    const curva_t *c = curva(id);
    if (!c) return false;
    int n = c->n;
    ponto_t q, g;
    if (!ler_ponto(c, pub, np, &q)) return false;
    while (nr && !*rb) { rb++; nr--; }
    while (ns && !*sb) { sb++; ns--; }
    if (nr > c->tam || ns > c->tam) return false;
    uint32_t r[EC_MAX], s[EC_MAX], e[EC_MAX], w[EC_MAX], u1[EC_MAX], u2[EC_MAX];
    bn_de_bytes(r, n, rb, nr);
    bn_de_bytes(s, n, sb, ns);
    if (bn_zero(r, n) || bn_zero(s, n) || bn_comparar(r, c->ordem.m, n) >= 0 || bn_comparar(s, c->ordem.m, n) >= 0)
        return false;
    /* e = os bits mais à esquerda do hash, tantos quanto os da ordem */
    bn_de_bytes(e, n, hash, nh > c->tam ? c->tam : nh);
    if (bn_comparar(e, c->ordem.m, n) >= 0) bn_subtrair(e, e, c->ordem.m, n);
    /* w = s^-1 (Fermat: s^(n-2)); u1 = e w, u2 = r w (mod n) */
    mont_para(&c->ordem, w, s);
    mont_potencia(&c->ordem, w, w, c->n_menos_2, c->tam);
    mont_mul(&c->ordem, u1, e, w);
    mont_mul(&c->ordem, u2, r, w);
    /* Shamir: percorre os bits de u1 e u2 juntos, somando G, Q ou G + Q */
    copiar(c, g.x, c->gx);
    copiar(c, g.y, c->gy);
    copiar(c, g.z, c->p.um);
    ponto_t gq, x;
    somar_completo(c, &gq, &g, &q);
    memset(&x, 0, sizeof x);
    for (int i = 32 * n - 1; i >= 0; i--) {
        if (!bn_zero(x.z, n)) dobrar(c, &x, &x);
        int b1 = (u1[i / 32] >> (i % 32)) & 1, b2 = (u2[i / 32] >> (i % 32)) & 1;
        if (b1 && b2) somar_completo(c, &x, &x, &gq);
        else if (b1) somar_completo(c, &x, &x, &g);
        else if (b2) somar_completo(c, &x, &x, &q);
    }
    if (bn_zero(x.z, n)) return false;
    uint32_t ax[EC_MAX], ay[EC_MAX];
    para_afim(c, &x, ax, ay);
    if (bn_comparar(ax, c->ordem.m, n) >= 0) bn_subtrair(ax, ax, c->ordem.m, n);
    return bn_comparar(ax, r, n) == 0;
}
