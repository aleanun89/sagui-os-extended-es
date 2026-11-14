// SPDX-License-Identifier: GPL-2.0-or-later
/* bignum.c - aritmética de Montgomery e a verificação RSA (RFC 8017). D060.
 *
 * Só o necessário para VERIFICAR: o expoente público é pequeno (65537) e
 * nada aqui é segredo, mas a multiplicação e as somas modulares são de tempo
 * constante porque as curvas (ECDHE) também as usam com a chave privada. */
#include "bignum.h"

void bn_de_bytes(uint32_t *r, int n, const uint8_t *b, unsigned nb)
{
    for (int i = 0; i < n; i++) r[i] = 0;
    for (unsigned i = 0; i < nb && i < 4u * (unsigned)n; i++)
        r[i / 4] |= (uint32_t)b[nb - 1 - i] << (8 * (i % 4));
}

void bn_para_bytes(uint8_t *b, unsigned nb, const uint32_t *a, int n)
{
    for (unsigned i = 0; i < nb; i++)
        b[nb - 1 - i] = i / 4 < (unsigned)n ? (uint8_t)(a[i / 4] >> (8 * (i % 4))) : 0;
}

int bn_comparar(const uint32_t *a, const uint32_t *b, int n)
{
    for (int i = n - 1; i >= 0; i--)
        if (a[i] != b[i]) return a[i] > b[i] ? 1 : -1;
    return 0;
}

bool bn_zero(const uint32_t *a, int n)
{
    uint32_t o = 0;
    for (int i = 0; i < n; i++) o |= a[i];
    return o == 0;
}

uint32_t bn_somar(uint32_t *r, const uint32_t *a, const uint32_t *b, int n)
{
    uint64_t c = 0;
    for (int i = 0; i < n; i++) {
        c += (uint64_t)a[i] + b[i];
        r[i] = (uint32_t)c;
        c >>= 32;
    }
    return (uint32_t)c;
}

uint32_t bn_subtrair(uint32_t *r, const uint32_t *a, const uint32_t *b, int n)
{
    uint32_t emp = 0;
    for (int i = 0; i < n; i++) {
        uint64_t d = (uint64_t)a[i] - b[i] - emp;
        r[i] = (uint32_t)d;
        emp = (uint32_t)(d >> 63);
    }
    return emp;
}

/* r = mascara ? a : b (mascara = 0 ou 0xFFFFFFFF) */
static void escolher(uint32_t *r, uint32_t mascara, const uint32_t *a, const uint32_t *b, int n)
{
    for (int i = 0; i < n; i++) r[i] = (a[i] & mascara) | (b[i] & ~mascara);
}

void mod_somar(const mont_t *c, uint32_t *r, const uint32_t *a, const uint32_t *b)
{
    uint32_t s[BN_MAX], d[BN_MAX];
    uint32_t vai = bn_somar(s, a, b, c->n);
    uint32_t emp = bn_subtrair(d, s, c->m, c->n);
    /* usa s - m se houve vai-um ou se s >= m (não emprestou) */
    uint32_t usa_d = (uint32_t)-(int32_t)(vai | (emp ^ 1));
    escolher(r, usa_d, d, s, c->n);
}

void mod_subtrair(const mont_t *c, uint32_t *r, const uint32_t *a, const uint32_t *b)
{
    uint32_t s[BN_MAX], d[BN_MAX];
    uint32_t emp = bn_subtrair(s, a, b, c->n);
    bn_somar(d, s, c->m, c->n);
    escolher(r, (uint32_t)-(int32_t)emp, d, s, c->n);
}

/* CIOS (Koç, Acar e Kaliski, 1996): multiplica e reduz membro a membro. */
void mont_mul(const mont_t *c, uint32_t *r, const uint32_t *a, const uint32_t *b)
{
    int n = c->n;
    uint32_t t[BN_MAX + 2];
    for (int i = 0; i < n + 2; i++) t[i] = 0;
    for (int i = 0; i < n; i++) {
        uint64_t v = 0;
        uint32_t bi = b[i];
        for (int j = 0; j < n; j++) {
            v = (uint64_t)t[j] + (uint64_t)a[j] * bi + (v >> 32);
            t[j] = (uint32_t)v;
        }
        v = (uint64_t)t[n] + (v >> 32);
        t[n] = (uint32_t)v;
        t[n + 1] = (uint32_t)(v >> 32);
        uint32_t q = t[0] * c->m0inv;
        v = (uint64_t)t[0] + (uint64_t)q * c->m[0];
        for (int j = 1; j < n; j++) {
            v = (uint64_t)t[j] + (uint64_t)q * c->m[j] + (v >> 32);
            t[j - 1] = (uint32_t)v;
        }
        v = (uint64_t)t[n] + (v >> 32);
        t[n - 1] = (uint32_t)v;
        t[n] = t[n + 1] + (uint32_t)(v >> 32);
    }
    uint32_t d[BN_MAX];
    uint32_t emp = bn_subtrair(d, t, c->m, n);
    uint32_t usa_d = (uint32_t)-(int32_t)(t[n] | (emp ^ 1));
    escolher(r, usa_d, d, t, n);
}

void mont_para(const mont_t *c, uint32_t *r, const uint32_t *a) { mont_mul(c, r, a, c->r2); }

void mont_de(const mont_t *c, uint32_t *r, const uint32_t *a)
{
    uint32_t um[BN_MAX];
    for (int i = 0; i < c->n; i++) um[i] = 0;
    um[0] = 1;
    mont_mul(c, r, a, um);
}

void mont_potencia(const mont_t *c, uint32_t *r, const uint32_t *a, const uint8_t *e, unsigned ne)
{
    uint32_t x[BN_MAX];
    memcpy(x, c->um, (unsigned)c->n * 4);
    for (unsigned i = 0; i < ne; i++)
        for (int b = 7; b >= 0; b--) {
            mont_mul(c, x, x, x);
            if ((e[i] >> b) & 1) mont_mul(c, x, x, a);
        }
    memcpy(r, x, (unsigned)c->n * 4);
}

bool mont_preparar(mont_t *c, const uint8_t *modulo, unsigned nb)
{
    while (nb && !*modulo) { modulo++; nb--; }
    if (!nb || nb > 4 * (BN_MAX - 1) || !(modulo[nb - 1] & 1)) return false;
    int n = (int)(nb + 3) / 4;
    c->n = n;
    bn_de_bytes(c->m, n, modulo, nb);
    /* -m^-1 mod 2^32 por Newton: cada passo dobra os bits certos */
    uint32_t inv = 1;
    for (int i = 0; i < 5; i++) inv *= 2 - c->m[0] * inv;
    c->m0inv = (uint32_t)-inv;
    /* R mod m e R^2 mod m dobrando 1 (32n e 64n vezes) com subtração */
    uint32_t x[BN_MAX];
    for (int i = 0; i < n; i++) x[i] = 0;
    x[0] = 1;
    for (int k = 0; k < 64 * n; k++) {
        uint32_t alto = x[n - 1] >> 31;
        for (int i = n - 1; i > 0; i--) x[i] = x[i] << 1 | x[i - 1] >> 31;
        x[0] <<= 1;
        if (alto || bn_comparar(x, c->m, n) >= 0) bn_subtrair(x, x, c->m, n);
        if (k == 32 * n - 1) memcpy(c->um, x, (unsigned)n * 4);
    }
    memcpy(c->r2, x, (unsigned)n * 4);
    return true;
}

/* ------------------------------------------------------------ RSA */

/* s^e mod n em bytes (k = tamanho do módulo). Falha se s >= n. */
static bool rsa_publica(const uint8_t *n, unsigned nn, const uint8_t *e, unsigned ne,
                        const uint8_t *assin, unsigned na, uint8_t *em, unsigned *k)
{
    static mont_t c;   /* 1,5 KB: fora da pilha */
    while (nn && !*n) { n++; nn--; }
    if (nn < 128 || nn > RSA_MAX_BYTES || ne == 0 || ne > 8) return false;   /* 1024 a 4096 bits */
    if (na != nn) return false;   /* a assinatura tem exatamente o tamanho do módulo (8.2.2) */
    if (!mont_preparar(&c, n, nn)) return false;
    uint32_t s[BN_MAX], x[BN_MAX];
    bn_de_bytes(s, c.n, assin, na);
    if (bn_comparar(s, c.m, c.n) >= 0) return false;
    mont_para(&c, x, s);
    mont_potencia(&c, x, x, e, ne);
    mont_de(&c, x, x);
    bn_para_bytes(em, nn, x, c.n);
    *k = nn;
    return true;
}

/* DigestInfo DER de cada hash (RFC 8017, 9.2, nota 1) */
static const uint8_t DI_SHA256[] = { 0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20 };
static const uint8_t DI_SHA384[] = { 0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02, 0x05, 0x00, 0x04, 0x30 };
static const uint8_t DI_SHA512[] = { 0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05, 0x00, 0x04, 0x40 };

bool rsa_verificar_pkcs1(const uint8_t *n, unsigned nn, const uint8_t *e, unsigned ne,
                         const hash_alg_t *h, const uint8_t *hash, const uint8_t *assin, unsigned na)
{
    static uint8_t em[RSA_MAX_BYTES];
    unsigned k;
    if (!rsa_publica(n, nn, e, ne, assin, na, em, &k)) return false;
    const uint8_t *di = h == &HASH_SHA256 ? DI_SHA256 : h == &HASH_SHA384 ? DI_SHA384 : h == &HASH_SHA512 ? DI_SHA512 : NULL;
    if (!di) return false;
    unsigned t = sizeof DI_SHA256 + h->tam;
    if (k < t + 11) return false;
    /* EM = 0x00 || 0x01 || PS (0xFF...) || 0x00 || T: monta o esperado e compara tudo (9.2) */
    unsigned ps = k - t - 3;
    if (em[0] != 0 || em[1] != 1 || em[2 + ps] != 0) return false;
    for (unsigned i = 0; i < ps; i++)
        if (em[2 + i] != 0xFF) return false;
    return memcmp(em + 3 + ps, di, sizeof DI_SHA256) == 0 && memcmp(em + 3 + ps + sizeof DI_SHA256, hash, h->tam) == 0;
}

/* MGF1 (B.2.1): máscara = Hash(semente || 0) || Hash(semente || 1) ... */
static void mgf1_xor(const hash_alg_t *h, const uint8_t *semente, unsigned ns, uint8_t *d, unsigned n)
{
    uint8_t t[HASH_MAX];
    for (uint32_t cont = 0; n; cont++) {
        hash_ctx_t c;
        uint8_t cb[4] = { (uint8_t)(cont >> 24), (uint8_t)(cont >> 16), (uint8_t)(cont >> 8), (uint8_t)cont };
        h->iniciar(&c);
        h->somar(&c, semente, ns);
        h->somar(&c, cb, 4);
        h->fim(&c, t);
        unsigned k = n < h->tam ? n : h->tam;
        for (unsigned i = 0; i < k; i++) d[i] ^= t[i];
        d += k;
        n -= k;
    }
}

bool rsa_verificar_pss(const uint8_t *n, unsigned nn, const uint8_t *e, unsigned ne,
                       const hash_alg_t *h, const uint8_t *hash, const uint8_t *assin, unsigned na, int sal)
{
    static uint8_t em[RSA_MAX_BYTES];
    unsigned k;
    if (!rsa_publica(n, nn, e, ne, assin, na, em, &k)) return false;
    while (nn && !*n) { n++; nn--; }
    /* modBits: bits do módulo; emBits = modBits - 1 (9.1.2 e 8.1.2) */
    unsigned bits = 8 * nn;
    for (uint8_t b = n[0]; !(b & 0x80); b <<= 1) bits--;
    unsigned em_bits = bits - 1, em_len = (em_bits + 7) / 8;
    uint8_t *m = em + (k - em_len);
    if (k > em_len && em[0] != 0) return false;   /* o byte que sobra do inteiro tem de ser 0 */
    unsigned hl = h->tam;
    if (em_len < hl + 2 || m[em_len - 1] != 0xbc) return false;
    unsigned ndb = em_len - hl - 1;
    uint8_t *db = m, *hh = m + ndb;
    uint8_t mascara_alta = (uint8_t)(0xFF >> (8 * em_len - em_bits));
    if (db[0] & ~mascara_alta) return false;
    mgf1_xor(h, hh, hl, db, ndb);
    db[0] &= mascara_alta;
    unsigned i = 0;
    while (i < ndb && db[i] == 0) i++;
    if (i == ndb || db[i] != 1) return false;
    unsigned ns = ndb - i - 1;
    if (sal >= 0 && ns != (unsigned)sal) return false;
    /* M' = 8 zeros || mHash || sal; H' = Hash(M') */
    uint8_t zeros[8] = { 0 }, h2[HASH_MAX];
    hash_ctx_t c;
    h->iniciar(&c);
    h->somar(&c, zeros, 8);
    h->somar(&c, hash, hl);
    h->somar(&c, db + i + 1, ns);
    h->fim(&c, h2);
    return memcmp(h2, hh, hl) == 0;
}
