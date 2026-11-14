// SPDX-License-Identifier: GPL-2.0-or-later
/* wpa_cifra.c - primitivas del Wi-Fi para WEP, TKIP, el descriptor 1 de
 * EAPOL-Key, las AKM con SHA-256 (SAE, PSK-SHA256), el CCMP por software y
 * BIP (D220, v7.3.2). Ver wpa_cifra.h.
 *
 * Escrito a partir de las normas: MD5 (RFC 1321), HMAC (RFC 2104), RC4 (el
 * algoritmo público, comprobado con RFC 6229), CRC-32 (IEEE 802.3), Michael,
 * la mezcla de TKIP, WEP, CCMP y BIP de IEEE 802.11-2020 (12.3, 12.5), el KDF
 * de la 802.11 (12.7.1.6.2), HKDF (RFC 5869) y AES-CMAC (RFC 4493). Las
 * pruebas del anfitrión (tests/h215_wifi.py) lo comprueban con los vectores
 * de esas normas.
 *
 * Nada de esto es camino caliente de una tarjeta rápida: el pincho es b/g y
 * el TKIP y el WEP son para redes antiguas. Se prefiere lo sencillo y claro. */
#include "wpa_cifra.h"
#include "sha256.h"

/* ------------------------------------------------------------ utilidades */

static void copiar(void *d, const void *o, uint32_t n)
{
    uint8_t *a = d;
    const uint8_t *b = o;
    while (n--) *a++ = *b++;
}

/* Copia hacia atrás: para abrir hueco dentro del mismo buffer. */
static void mover(uint8_t *d, const uint8_t *o, uint32_t n)
{
    if (d <= o) { while (n--) *d++ = *o++; return; }
    while (n--) d[n] = o[n];
}

static void zerar(void *d, uint32_t n)
{
    uint8_t *a = d;
    while (n--) *a++ = 0;
}

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void poe_le32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

#define ROTE(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define ROTD(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

/* ------------------------------------------------------------ MD5 (RFC 1321, 3.4) */

static const uint32_t MD5_T[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};
static const uint8_t MD5_R[16] = { 7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21 };

static void md5_comprimir(uint32_t h[4], const uint8_t *b)
{
    uint32_t x[16];
    for (int i = 0; i < 16; i++) x[i] = le32(b + 4 * i);
    uint32_t a = h[0], bb = h[1], c = h[2], d = h[3];
    for (int i = 0; i < 64; i++) {
        uint32_t f;
        int g, r = i / 16;
        if (r == 0) { f = (bb & c) | (~bb & d); g = i; }
        else if (r == 1) { f = (d & bb) | (~d & c); g = (5 * i + 1) % 16; }
        else if (r == 2) { f = bb ^ c ^ d; g = (3 * i + 5) % 16; }
        else { f = c ^ (bb | ~d); g = (7 * i) % 16; }
        uint32_t t = a + f + MD5_T[i] + x[g];
        int s = MD5_R[4 * r + i % 4];
        a = d; d = c; c = bb;
        bb = bb + ROTE(t, s);
    }
    h[0] += a; h[1] += bb; h[2] += c; h[3] += d;
}

void md5_iniciar(md5_t *c)
{
    c->h[0] = 0x67452301u; c->h[1] = 0xefcdab89u; c->h[2] = 0x98badcfeu; c->h[3] = 0x10325476u;
    c->usados = 0;
    c->total = 0;
}

void md5_somar(md5_t *c, const void *dados, uint32_t n)
{
    const uint8_t *p = dados;
    c->total += n;
    while (n) {
        uint32_t k = 64 - c->usados;
        if (k > n) k = n;
        copiar(c->bloco + c->usados, p, k);
        c->usados += k;
        p += k;
        n -= k;
        if (c->usados == 64) { md5_comprimir(c->h, c->bloco); c->usados = 0; }
    }
}

void md5_fim(md5_t *c, uint8_t saida[MD5_TAM])
{
    uint64_t bits = c->total * 8;
    uint8_t um = 0x80, zero = 0;
    md5_somar(c, &um, 1);
    while (c->usados != 56) md5_somar(c, &zero, 1);
    uint8_t t[8];
    for (int i = 0; i < 8; i++) t[i] = (uint8_t)(bits >> (8 * i));   /* longitud en little-endian */
    md5_somar(c, t, 8);
    for (int i = 0; i < 4; i++) poe_le32(saida + 4 * i, c->h[i]);
}

void md5(const void *dados, uint32_t n, uint8_t saida[MD5_TAM])
{
    md5_t c;
    md5_iniciar(&c);
    md5_somar(&c, dados, n);
    md5_fim(&c, saida);
}

void hmac_md5(const uint8_t *chave, uint32_t nchave, const uint8_t *dados, uint32_t n, uint8_t saida[MD5_TAM])
{
    uint8_t k[64], pad[64], interno[MD5_TAM];
    zerar(k, sizeof k);
    if (nchave > 64) md5(chave, nchave, k);
    else copiar(k, chave, nchave);
    md5_t c;
    for (int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x36;
    md5_iniciar(&c);
    md5_somar(&c, pad, 64);
    md5_somar(&c, dados, n);
    md5_fim(&c, interno);
    for (int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x5C;
    md5_iniciar(&c);
    md5_somar(&c, pad, 64);
    md5_somar(&c, interno, MD5_TAM);
    md5_fim(&c, saida);
    wpa_apagar(k, sizeof k);
    wpa_apagar(pad, sizeof pad);
    wpa_apagar(&c, sizeof c);
}

/* ------------------------------------------------------------ HMAC-SHA256, KDF y HKDF */

void hmac_sha256_partes(const uint8_t *chave, uint32_t nchave, int partes, const uint8_t *const *p,
                        const uint32_t *n, uint8_t saida[SHA256_SAIDA])
{
    uint8_t k[64], pad[64], interno[SHA256_SAIDA];
    zerar(k, sizeof k);
    if (nchave > 64) sha256(chave, nchave, k);
    else copiar(k, chave, nchave);
    sha256_t c;
    for (int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x36;
    sha256_iniciar(&c);
    sha256_somar(&c, pad, 64);
    for (int i = 0; i < partes; i++) sha256_somar(&c, p[i], n[i]);
    sha256_fim(&c, interno);
    for (int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x5C;
    sha256_iniciar(&c);
    sha256_somar(&c, pad, 64);
    sha256_somar(&c, interno, SHA256_SAIDA);
    sha256_fim(&c, saida);
    wpa_apagar(k, sizeof k);
    wpa_apagar(pad, sizeof pad);
    wpa_apagar(interno, sizeof interno);
    wpa_apagar(&c, sizeof c);
}

void hmac_sha256(const uint8_t *chave, uint32_t nchave, const uint8_t *dados, uint32_t n, uint8_t saida[SHA256_SAIDA])
{
    const uint8_t *p[1] = { dados };
    uint32_t t[1] = { n };
    hmac_sha256_partes(chave, nchave, 1, p, t, saida);
}

void kdf_sha256(const uint8_t *chave, uint32_t nchave, const char *rotulo, const uint8_t *contexto,
                uint32_t ncontexto, uint8_t *saida, uint32_t nsaida)
{
    uint32_t nr = 0;
    while (rotulo[nr]) nr++;
    uint32_t bits = nsaida * 8;
    uint8_t comp[2] = { (uint8_t)bits, (uint8_t)(bits >> 8) };
    for (uint16_t i = 1; nsaida; i++) {
        uint8_t ind[2] = { (uint8_t)i, (uint8_t)(i >> 8) };
        const uint8_t *p[4] = { ind, (const uint8_t *)rotulo, contexto, comp };
        uint32_t n[4] = { 2, nr, ncontexto, 2 };
        uint8_t h[SHA256_SAIDA];
        hmac_sha256_partes(chave, nchave, 4, p, n, h);
        uint32_t k = nsaida < SHA256_SAIDA ? nsaida : SHA256_SAIDA;
        copiar(saida, h, k);
        saida += k;
        nsaida -= k;
        wpa_apagar(h, sizeof h);
    }
}

void hkdf_extrair(const uint8_t *sal, uint32_t nsal, const uint8_t *ikm, uint32_t nikm, uint8_t prk[SHA256_SAIDA])
{
    static const uint8_t zeros[SHA256_SAIDA];
    if (!nsal) { sal = zeros; nsal = SHA256_SAIDA; }
    hmac_sha256(sal, nsal, ikm, nikm, prk);
}

void hkdf_expandir(const uint8_t prk[SHA256_SAIDA], const uint8_t *info, uint32_t ninfo, uint8_t *saida, uint32_t n)
{
    uint8_t t[SHA256_SAIDA];
    uint32_t nt = 0;
    for (uint8_t i = 1; n; i++) {
        const uint8_t *p[3] = { t, info, &i };
        uint32_t m[3] = { nt, ninfo, 1 };
        hmac_sha256_partes(prk, SHA256_SAIDA, 3, p, m, t);
        nt = SHA256_SAIDA;
        uint32_t k = n < SHA256_SAIDA ? n : SHA256_SAIDA;
        copiar(saida, t, k);
        saida += k;
        n -= k;
    }
    wpa_apagar(t, sizeof t);
}

/* ------------------------------------------------------------ RC4 */

void rc4_chave(rc4_t *r, const uint8_t *chave, uint32_t n)
{
    for (int i = 0; i < 256; i++) r->s[i] = (uint8_t)i;
    uint8_t j = 0;
    for (int i = 0; i < 256; i++) {
        j = (uint8_t)(j + r->s[i] + chave[i % n]);
        uint8_t t = r->s[i]; r->s[i] = r->s[j]; r->s[j] = t;
    }
    r->i = r->j = 0;
}

void rc4_aplicar(rc4_t *r, uint8_t *d, uint32_t n)
{
    uint8_t i = r->i, j = r->j;
    for (uint32_t k = 0; k < n; k++) {
        i++;
        j = (uint8_t)(j + r->s[i]);
        uint8_t t = r->s[i]; r->s[i] = r->s[j]; r->s[j] = t;
        d[k] ^= r->s[(uint8_t)(r->s[i] + r->s[j])];
    }
    r->i = i;
    r->j = j;
}

void rc4_pular(rc4_t *r, uint32_t n)
{
    uint8_t lixo[64];
    while (n) {
        uint32_t k = n < sizeof lixo ? n : sizeof lixo;
        zerar(lixo, k);
        rc4_aplicar(r, lixo, k);
        n -= k;
    }
}

uint32_t wpa_crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(c & 1));
    }
    return ~c;
}

/* ------------------------------------------------------------ Michael (12.5.2.3) */

static void michael_bloco(uint32_t *l, uint32_t *r)
{
    *r ^= ROTE(*l, 17);
    *l += *r;
    *r ^= ((*l & 0xff00ff00u) >> 8) | ((*l & 0x00ff00ffu) << 8);
    *l += *r;
    *r ^= ROTE(*l, 3);
    *l += *r;
    *r ^= ROTD(*l, 2);
    *l += *r;
}

/* Michael de varias partes seguidas (la pseudocabecera y los datos). */
static void michael_partes(const uint8_t chave[8], int partes, const uint8_t *const *p, const uint32_t *n,
                           uint8_t mic[8])
{
    uint32_t l = le32(chave), r = le32(chave + 4);
    uint8_t w[4];
    uint32_t k = 0;
    for (int i = 0; i < partes; i++)
        for (uint32_t j = 0; j < n[i]; j++) {
            w[k++] = p[i][j];
            if (k == 4) { l ^= le32(w); michael_bloco(&l, &r); k = 0; }
        }
    /* relleno: 0x5a y de 4 a 7 ceros hasta un múltiplo de 4 */
    w[k++] = 0x5a;
    while (k < 4) w[k++] = 0;
    l ^= le32(w);
    michael_bloco(&l, &r);
    michael_bloco(&l, &r);   /* la palabra de ceros: l ^= 0 */
    poe_le32(mic, l);
    poe_le32(mic + 4, r);
}

void michael(const uint8_t chave[8], const uint8_t *msg, uint32_t n, uint8_t mic[8])
{
    const uint8_t *p[1] = { msg };
    uint32_t t[1] = { n };
    michael_partes(chave, 1, p, t, mic);
}

/* ------------------------------------------------------------ mezcla de clave de TKIP (12.5.2.5) */

/* S(v) = Sbox[Lo8(v)] ^ Swap16(Sbox[Hi8(v)]); Sbox[i] = 2·S[i] << 8 | 3·S[i]
 * (S = la caja S del AES, en GF(2^8)). */
static uint16_t tkip_s(uint16_t v)
{
    uint8_t a = aes_caixa_s((uint8_t)v), b = aes_caixa_s((uint8_t)(v >> 8));
    uint8_t a2 = (uint8_t)((a << 1) ^ ((a & 0x80) ? 0x1B : 0)), b2 = (uint8_t)((b << 1) ^ ((b & 0x80) ? 0x1B : 0));
    uint16_t sa = (uint16_t)(a2 << 8 | (uint8_t)(a2 ^ a));
    uint16_t sb = (uint16_t)((uint8_t)(b2 ^ b) << 8 | b2);   /* ya con los bytes cambiados */
    return (uint16_t)(sa ^ sb);
}

#define MK16(hi, lo) ((uint16_t)((hi) << 8 | (lo)))
#define ROTD1(v) ((uint16_t)(((v) >> 1) | ((v) << 15)))

void tkip_fase1(uint16_t p1[5], const uint8_t tk[16], const uint8_t ta[6], uint32_t iv32)
{
    p1[0] = (uint16_t)iv32;
    p1[1] = (uint16_t)(iv32 >> 16);
    p1[2] = MK16(ta[1], ta[0]);
    p1[3] = MK16(ta[3], ta[2]);
    p1[4] = MK16(ta[5], ta[4]);
    for (int i = 0; i < 8; i++) {
        int j = 2 * (i & 1);
        p1[0] = (uint16_t)(p1[0] + tkip_s(p1[4] ^ MK16(tk[1 + j], tk[0 + j])));
        p1[1] = (uint16_t)(p1[1] + tkip_s(p1[0] ^ MK16(tk[5 + j], tk[4 + j])));
        p1[2] = (uint16_t)(p1[2] + tkip_s(p1[1] ^ MK16(tk[9 + j], tk[8 + j])));
        p1[3] = (uint16_t)(p1[3] + tkip_s(p1[2] ^ MK16(tk[13 + j], tk[12 + j])));
        p1[4] = (uint16_t)(p1[4] + tkip_s(p1[3] ^ MK16(tk[1 + j], tk[0 + j])) + i);
    }
}

void tkip_fase2(uint8_t rc4[16], const uint16_t p1[5], const uint8_t tk[16], uint16_t iv16)
{
    uint16_t p[6];
    for (int i = 0; i < 5; i++) p[i] = p1[i];
    p[5] = (uint16_t)(p1[4] + iv16);
    p[0] = (uint16_t)(p[0] + tkip_s(p[5] ^ MK16(tk[1], tk[0])));
    p[1] = (uint16_t)(p[1] + tkip_s(p[0] ^ MK16(tk[3], tk[2])));
    p[2] = (uint16_t)(p[2] + tkip_s(p[1] ^ MK16(tk[5], tk[4])));
    p[3] = (uint16_t)(p[3] + tkip_s(p[2] ^ MK16(tk[7], tk[6])));
    p[4] = (uint16_t)(p[4] + tkip_s(p[3] ^ MK16(tk[9], tk[8])));
    p[5] = (uint16_t)(p[5] + tkip_s(p[4] ^ MK16(tk[11], tk[10])));
    p[0] = (uint16_t)(p[0] + ROTD1((uint16_t)(p[5] ^ MK16(tk[13], tk[12]))));
    p[1] = (uint16_t)(p[1] + ROTD1((uint16_t)(p[0] ^ MK16(tk[15], tk[14]))));
    p[2] = (uint16_t)(p[2] + ROTD1(p[1]));
    p[3] = (uint16_t)(p[3] + ROTD1(p[2]));
    p[4] = (uint16_t)(p[4] + ROTD1(p[3]));
    p[5] = (uint16_t)(p[5] + ROTD1(p[4]));
    rc4[0] = (uint8_t)(iv16 >> 8);
    rc4[1] = (uint8_t)(((iv16 >> 8) | 0x20) & 0x7F);
    rc4[2] = (uint8_t)iv16;
    rc4[3] = (uint8_t)(((uint16_t)(p[5] ^ MK16(tk[1], tk[0]))) >> 1);
    for (int i = 0; i < 6; i++) { rc4[4 + 2 * i] = (uint8_t)p[i]; rc4[5 + 2 * i] = (uint8_t)(p[i] >> 8); }
    wpa_apagar(p, sizeof p);
}

/* ------------------------------------------------------------ tramas 802.11 */

/* Las direcciones de destino y origen de una trama de datos (12.5.2.3, figura
 * 12-14: la pseudocabecera de Michael) según To DS / From DS. */
static void da_sa(const uint8_t *q, const uint8_t **da, const uint8_t **sa)
{
    switch (q[1] & 3) {
    case 0: *da = q + 4; *sa = q + 10; break;
    case 1: *da = q + 16; *sa = q + 10; break;
    case 2: *da = q + 4; *sa = q + 16; break;
    default: *da = q + 16; *sa = q + 24; break;
    }
}

static uint8_t prioridade(const uint8_t *q, uint32_t cab)
{
    if ((q[0] & 0x0C) == 0x08 && (q[0] & 0x80)) {   /* datos QoS: el TID */
        uint32_t qc = ((q[1] & 3) == 3) ? 30 : 24;
        if (qc < cab) return q[qc] & 0x0F;
    }
    return 0;
}

static void tkip_mic(const uint8_t *q, uint32_t cab, const uint8_t *dados, uint32_t n, const uint8_t chave[8],
                     uint8_t mic[8])
{
    const uint8_t *da, *sa;
    da_sa(q, &da, &sa);
    uint8_t prio[4] = { prioridade(q, cab), 0, 0, 0 };
    const uint8_t *p[4] = { da, sa, prio, dados };
    uint32_t t[4] = { 6, 6, 4, n };
    michael_partes(chave, 4, p, t, mic);
}

uint64_t tkip_tsc(const uint8_t *iv)
{
    return (uint64_t)iv[2] | (uint64_t)iv[0] << 8 | (uint64_t)iv[4] << 16 | (uint64_t)iv[5] << 24 |
           (uint64_t)iv[6] << 32 | (uint64_t)iv[7] << 40;
}

uint64_t ccmp_pn(const uint8_t *iv)
{
    return (uint64_t)iv[0] | (uint64_t)iv[1] << 8 | (uint64_t)iv[4] << 16 | (uint64_t)iv[5] << 24 |
           (uint64_t)iv[6] << 32 | (uint64_t)iv[7] << 40;
}

static void tkip_chave_rc4(const uint8_t *q, const uint8_t tk[16], uint64_t tsc, uint8_t rc4[16])
{
    uint16_t p1[5];
    tkip_fase1(p1, tk, q + 10, (uint32_t)(tsc >> 16));   /* TA = la dirección 2 */
    tkip_fase2(rc4, p1, tk, (uint16_t)tsc);
    wpa_apagar(p1, sizeof p1);
}

uint32_t tkip_cifrar(uint8_t *q, uint32_t cab, uint32_t n, const uint8_t tk[16], const uint8_t chave_mic[8],
                     uint64_t tsc, uint8_t keyid)
{
    uint32_t nd = n - cab;
    uint8_t *d = q + cab + 8;
    mover(d, q + cab, nd);                     /* hueco para el IV y el IV extendido */
    tkip_mic(q, cab, d, nd, chave_mic, d + nd);
    poe_le32(d + nd + 8, wpa_crc32(d, nd + 8));
    uint8_t *iv = q + cab;
    iv[0] = (uint8_t)(tsc >> 8);
    iv[1] = (uint8_t)(((tsc >> 8) | 0x20) & 0x7F);
    iv[2] = (uint8_t)tsc;
    iv[3] = (uint8_t)(keyid << 6 | 0x20);      /* ExtIV */
    for (int i = 0; i < 4; i++) iv[4 + i] = (uint8_t)(tsc >> (16 + 8 * i));
    uint8_t k[16];
    rc4_t r;
    tkip_chave_rc4(q, tk, tsc, k);
    rc4_chave(&r, k, 16);
    rc4_aplicar(&r, d, nd + 12);
    wpa_apagar(k, sizeof k);
    wpa_apagar(&r, sizeof r);
    q[1] |= 0x40;
    return n + 20;
}

int tkip_decifrar(uint8_t *q, uint32_t cab, uint32_t n, const uint8_t tk[16], const uint8_t chave_mic[8],
                  uint32_t *nclaro)
{
    *nclaro = 0;
    if (n < cab + 8 + 12 || !(q[cab + 3] & 0x20)) return CIFRA_ERRO;
    uint64_t tsc = tkip_tsc(q + cab);
    uint8_t *d = q + cab + 8;
    uint32_t nd = n - cab - 8;                 /* datos + MIC + ICV */
    uint8_t k[16];
    rc4_t r;
    tkip_chave_rc4(q, tk, tsc, k);
    rc4_chave(&r, k, 16);
    rc4_aplicar(&r, d, nd);
    wpa_apagar(k, sizeof k);
    wpa_apagar(&r, sizeof r);
    if (wpa_crc32(d, nd - 4) != le32(d + nd - 4)) return CIFRA_ERRO_ICV;
    uint8_t mic[8];
    tkip_mic(q, cab, d, nd - 12, chave_mic, mic);
    if (!wpa_igual(mic, d + nd - 12, 8)) return CIFRA_ERRO_MIC;
    *nclaro = nd - 12;
    return CIFRA_OK;
}

uint32_t wep_cifrar(uint8_t *q, uint32_t cab, uint32_t n, const uint8_t *chave, uint32_t nchave,
                    uint32_t iv, uint8_t keyid)
{
    uint32_t nd = n - cab;
    uint8_t *d = q + cab + 4;
    mover(d, q + cab, nd);
    poe_le32(d + nd, wpa_crc32(d, nd));
    q[cab] = (uint8_t)iv;
    q[cab + 1] = (uint8_t)(iv >> 8);
    q[cab + 2] = (uint8_t)(iv >> 16);
    q[cab + 3] = (uint8_t)(keyid << 6);
    uint8_t k[16];
    copiar(k, q + cab, 3);
    copiar(k + 3, chave, nchave);
    rc4_t r;
    rc4_chave(&r, k, 3 + nchave);
    rc4_aplicar(&r, d, nd + 4);
    wpa_apagar(k, sizeof k);
    wpa_apagar(&r, sizeof r);
    q[1] |= 0x40;
    return n + 8;
}

int wep_decifrar(uint8_t *q, uint32_t cab, uint32_t n, const uint8_t *chave, uint32_t nchave, uint32_t *nclaro)
{
    *nclaro = 0;
    if (n < cab + 4 + 4 || (nchave != 5 && nchave != 13)) return CIFRA_ERRO;
    uint8_t k[16];
    copiar(k, q + cab, 3);
    copiar(k + 3, chave, nchave);
    uint8_t *d = q + cab + 4;
    uint32_t nd = n - cab - 4;
    rc4_t r;
    rc4_chave(&r, k, 3 + nchave);
    rc4_aplicar(&r, d, nd);
    wpa_apagar(k, sizeof k);
    wpa_apagar(&r, sizeof r);
    if (wpa_crc32(d, nd - 4) != le32(d + nd - 4)) return CIFRA_ERRO_ICV;
    *nclaro = nd - 4;
    return CIFRA_OK;
}

/* ------------------------------------------------------------ AES-CMAC (RFC 4493) */

static void dobro_gf128(uint8_t b[16])
{
    uint8_t c = b[0] & 0x80;
    for (int i = 0; i < 15; i++) b[i] = (uint8_t)(b[i] << 1 | b[i + 1] >> 7);
    b[15] = (uint8_t)(b[15] << 1);
    if (c) b[15] ^= 0x87;
}

void aes_cmac(const uint8_t chave[16], const uint8_t *m, uint32_t n, uint8_t t[16])
{
    aes128_t a;
    aes128_chave(&a, chave);
    uint8_t k1[16], k2[16], x[16];
    zerar(k1, 16);
    aes128_cifrar(&a, k1, k1);
    dobro_gf128(k1);
    copiar(k2, k1, 16);
    dobro_gf128(k2);
    zerar(x, 16);
    uint32_t blocos = n ? (n + 15) / 16 : 1;
    for (uint32_t b = 0; b + 1 < blocos; b++) {
        for (int i = 0; i < 16; i++) x[i] ^= m[16 * b + i];
        aes128_cifrar(&a, x, x);
    }
    uint32_t resto = n - 16 * (blocos - 1);
    const uint8_t *u = m + 16 * (blocos - 1);
    for (uint32_t i = 0; i < 16; i++) {
        uint8_t v = i < resto ? u[i] : (i == resto ? 0x80 : 0);
        x[i] ^= v ^ (resto == 16 ? k1[i] : k2[i]);
    }
    aes128_cifrar(&a, x, t);
    wpa_apagar(&a, sizeof a);
    wpa_apagar(k1, 16);
    wpa_apagar(k2, 16);
}

/* ------------------------------------------------------------ CCMP-128 (12.5.3) */

/* AAD y nonce de una MPDU (12.5.3.3.3 y 12.5.3.3.4). Devuelve el tamaño de la AAD. */
static uint32_t ccmp_aad(const uint8_t *q, uint32_t cab, uint64_t pn, uint8_t aad[32], uint8_t nonce[13])
{
    bool dados = (q[0] & 0x0C) == 0x08, gestao = (q[0] & 0x0C) == 0x00;
    bool qos = dados && (q[0] & 0x80);
    bool a4 = (q[1] & 3) == 3;
    uint32_t k = 0;
    aad[k++] = dados ? (uint8_t)(q[0] & 0x8F) : q[0];        /* subtipo de datos a cero */
    aad[k++] = (uint8_t)((q[1] & ~(0x08 | 0x10 | 0x20 | (qos ? 0x80 : 0))) | 0x40);
    copiar(aad + k, q + 4, 18);                               /* A1, A2, A3 */
    k += 18;
    aad[k++] = q[22] & 0x0F;                                  /* solo el número de fragmento */
    aad[k++] = 0;
    if (a4) { copiar(aad + k, q + 24, 6); k += 6; }
    uint8_t prio = 0;
    if (qos) {
        uint32_t qc = a4 ? 30 : 24;
        prio = q[qc] & 0x0F;
        if (qc + 2 <= cab) { aad[k++] = prio; aad[k++] = 0; }
    }
    nonce[0] = (uint8_t)(prio | (gestao ? 0x10 : 0));
    copiar(nonce + 1, q + 10, 6);
    for (int i = 0; i < 6; i++) nonce[7 + i] = (uint8_t)(pn >> (40 - 8 * i));
    return k;
}

/* CCM (RFC 3610) con M = 8 y L = 2: CBC-MAC y cifrado en modo contador. */
static void ccm(const aes128_t *a, const uint8_t nonce[13], const uint8_t *aad, uint32_t naad, uint8_t *d,
                uint32_t n, bool cifrar, uint8_t mic[8])
{
    uint8_t x[16], b[16], s[16];
    b[0] = 0x59;
    copiar(b + 1, nonce, 13);
    b[14] = (uint8_t)(n >> 8);
    b[15] = (uint8_t)n;
    aes128_cifrar(a, b, x);
    /* la AAD con su longitud delante, en bloques de 16 */
    uint8_t blk[16];
    uint32_t pos = 0, i = 0;
    zerar(blk, 16);
    blk[0] = (uint8_t)(naad >> 8);
    blk[1] = (uint8_t)naad;
    i = 2;
    while (pos < naad) {
        blk[i++] = aad[pos++];
        if (i == 16) { for (int j = 0; j < 16; j++) x[j] ^= blk[j]; aes128_cifrar(a, x, x); zerar(blk, 16); i = 0; }
    }
    if (i) { for (int j = 0; j < 16; j++) x[j] ^= blk[j]; aes128_cifrar(a, x, x); }
    /* contador: A_i = 0x01 | nonce | i */
    uint8_t ctr[16];
    ctr[0] = 0x01;
    copiar(ctr + 1, nonce, 13);
    for (uint32_t off = 0, c = 1; off < n; off += 16, c++) {
        uint32_t k = n - off < 16 ? n - off : 16;
        ctr[14] = (uint8_t)(c >> 8);
        ctr[15] = (uint8_t)c;
        aes128_cifrar(a, ctr, s);
        if (!cifrar) for (uint32_t j = 0; j < k; j++) d[off + j] ^= s[j];
        for (uint32_t j = 0; j < k; j++) x[j] ^= d[off + j];
        aes128_cifrar(a, x, x);
        if (cifrar) for (uint32_t j = 0; j < k; j++) d[off + j] ^= s[j];
    }
    ctr[14] = ctr[15] = 0;
    aes128_cifrar(a, ctr, s);
    for (int j = 0; j < 8; j++) mic[j] = x[j] ^ s[j];
    wpa_apagar(x, sizeof x);
    wpa_apagar(s, sizeof s);
}

uint32_t ccmp_cifrar(uint8_t *q, uint32_t cab, uint32_t n, const uint8_t tk[16], uint64_t pn, uint8_t keyid)
{
    uint32_t nd = n - cab;
    uint8_t *d = q + cab + 8;
    mover(d, q + cab, nd);
    uint8_t *iv = q + cab;
    iv[0] = (uint8_t)pn;
    iv[1] = (uint8_t)(pn >> 8);
    iv[2] = 0;
    iv[3] = (uint8_t)(keyid << 6 | 0x20);
    for (int i = 0; i < 4; i++) iv[4 + i] = (uint8_t)(pn >> (16 + 8 * i));
    q[1] |= 0x40;
    uint8_t aad[32], nonce[13];
    uint32_t naad = ccmp_aad(q, cab, pn, aad, nonce);
    aes128_t a;
    aes128_chave(&a, tk);
    ccm(&a, nonce, aad, naad, d, nd, true, d + nd);
    wpa_apagar(&a, sizeof a);
    return n + 16;
}

int ccmp_decifrar(uint8_t *q, uint32_t cab, uint32_t n, const uint8_t tk[16], uint64_t *pn, uint32_t *nclaro)
{
    *nclaro = 0;
    if (n < cab + 16 || !(q[cab + 3] & 0x20)) return CIFRA_ERRO;
    *pn = ccmp_pn(q + cab);
    uint8_t aad[32], nonce[13], mic[8];
    uint32_t naad = ccmp_aad(q, cab, *pn, aad, nonce);
    uint8_t *d = q + cab + 8;
    uint32_t nd = n - cab - 16;
    aes128_t a;
    aes128_chave(&a, tk);
    ccm(&a, nonce, aad, naad, d, nd, false, mic);
    wpa_apagar(&a, sizeof a);
    if (!wpa_igual(mic, d + nd, 8)) {
        wpa_apagar(d, nd);
        return CIFRA_ERRO_ICV;
    }
    *nclaro = nd;
    return CIFRA_OK;
}

/* ------------------------------------------------------------ BIP-CMAC-128 (12.5.4) */

static void bip_mic(const uint8_t *q, uint32_t n, const uint8_t igtk[16], uint8_t mic[8])
{
    static uint8_t m[20 + 2400];
    if (n > 2400) n = 2400;
    m[0] = q[0];
    m[1] = (uint8_t)(q[1] & ~(0x08 | 0x10 | 0x20));   /* sin reintento, ahorro ni «más datos» */
    copiar(m + 2, q + 4, 18);
    copiar(m + 20, q + 24, n - 24);
    zerar(m + 20 + (n - 24) - 8, 8);                  /* el MIC del MMIE, a cero */
    uint8_t t[16];
    aes_cmac(igtk, m, 20 + (n - 24), t);
    copiar(mic, t, 8);
}

uint32_t bip_assinar(uint8_t *q, uint32_t n, const uint8_t igtk[16], uint16_t keyid, uint64_t ipn)
{
    uint8_t *e = q + n;
    e[0] = 76;
    e[1] = 16;
    e[2] = (uint8_t)keyid;
    e[3] = (uint8_t)(keyid >> 8);
    for (int i = 0; i < 6; i++) e[4 + i] = (uint8_t)(ipn >> (8 * i));
    zerar(e + 10, 8);
    n += BIP_MMIE_TAM;
    bip_mic(q, n, igtk, e + 10);
    return n;
}

bool bip_verificar(const uint8_t *q, uint32_t n, const uint8_t igtk[16], uint16_t keyid, uint64_t *ipn)
{
    if (n < 24 + BIP_MMIE_TAM) return false;
    const uint8_t *e = q + n - BIP_MMIE_TAM;
    if (e[0] != 76 || e[1] != 16 || (uint16_t)(e[2] | e[3] << 8) != keyid) return false;
    *ipn = 0;
    for (int i = 0; i < 6; i++) *ipn |= (uint64_t)e[4 + i] << (8 * i);
    uint8_t mic[8];
    bip_mic(q, n, igtk, mic);
    return wpa_igual(mic, e + 10, 8);
}
