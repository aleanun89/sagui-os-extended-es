// SPDX-License-Identifier: GPL-2.0-or-later
/* pdf_cripto.c - o cifrado dos PDF protegidos (melhoria 059, D138).
 *
 * Este modulo e autocontido: nao toca no userland/libtls (o AES de la so
 * cifra, porque o GCM usa CTR e nao precisa do inverso) nem no
 * kernel/lib/sha256.c. Tudo aqui foi escrito a partir das normas: RFC 1321
 * para o MD5, FIPS 197 para o AES, FIPS 180-4 para os SHA e o apartado 7.6
 * da ISO 32000-1 para o gestor de seguranca (e o 7.6.4 da ISO 32000-2
 * para as revisoes 5 e 6).
 *
 * O que tem dentro:
 *
 *   - MD5 (RFC 1321), RC4 e AES-128/256 (FIPS 197) no modo CBC.
 *   - O gestor de seguranca "standard" da ISO 32000-1, apartado 7.6:
 *     algoritmo 1 (chave por objecto), algoritmo 2 (chave de arquivo),
 *     algoritmo 3 (valor O), algoritmo 4 (senha de utilizador) e
 *     algoritmo 5 (senha de proprietario); mais as revisoes 5 e 6
 *     (V=5), que usam o "hash 2.B" da norma.
 *
 * A S-box do AES e CALCULADA (o inverso em GF(2^8) mais a transformacao
 * afim, tal como no userland/libtls/aes.c) e a sua inversa sai da propria
 * permutacao: nenhuma das duas esta escrita a mao. As constantes do MD5 e
 * dos SHA sao as do algoritmo, publicas, e podem estar escritas.
 *
 * Nao ha coma flutuante aqui: e tudo inteiro. */
#include "pdf_cripto.h"

/* ==================================================== MD5 (RFC 1321) ==== */

/* Os coeficientes K[i] = floor(2^32 * |sin(i + 1)|) e as deslocacoes de
 * cada passo, tal como na RFC 1321. Sao constantes publicas do algoritmo. */
static const uint32_t md5_k[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static const uint8_t md5_s[64] = {
    7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
};

static uint32_t md5_rodar(uint32_t x, int n)
{
    return (uint32_t)((x << n) | (x >> (32 - n)));
}

/* Um bloco de 64 bytes do MD5. */
static void md5_bloco(uint32_t h[4], const uint8_t b[64])
{
    uint32_t m[16];
    uint32_t a = h[0], b1 = h[1], c = h[2], d = h[3];
    for (int i = 0; i < 16; i++)
        m[i] = (uint32_t)b[4 * i] | ((uint32_t)b[4 * i + 1] << 8) |
               ((uint32_t)b[4 * i + 2] << 16) | ((uint32_t)b[4 * i + 3] << 24);
    for (int i = 0; i < 64; i++) {
        uint32_t f;
        int g;
        if (i < 16) {
            f = (b1 & c) | (~b1 & d);
            g = i;
        } else if (i < 32) {
            f = (d & b1) | (~d & c);
            g = (5 * i + 1) & 15;
        } else if (i < 48) {
            f = b1 ^ c ^ d;
            g = (3 * i + 5) & 15;
        } else {
            f = c ^ (b1 | ~d);
            g = (7 * i) & 15;
        }
        uint32_t tmp = d;
        d = c;
        c = b1;
        b1 += md5_rodar(a + f + md5_k[i] + m[g], md5_s[i]);
        a = tmp;
    }
    h[0] += a;
    h[1] += b1;
    h[2] += c;
    h[3] += d;
}

/* O MD5 dos dados. Se "com_tamanho" for falso, o bloco e processado sem o
 * campo do comprimento (a variante "MD5 de bloco" da norma 32000-1). */
static void md5_calcular(const uint8_t *dados, int n, uint8_t *saida, bool com_tamanho)
{
    uint32_t h[4] = { 0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u };
    uint8_t bloco[64];
    int i = 0;
    if (n < 0) n = 0;
    for (; i + 64 <= n; i += 64)
        md5_bloco(h, dados + i);
    int resto = n - i;
    memset(bloco, 0, sizeof bloco);
    if (resto > 0)
        memcpy(bloco, dados + i, (size_t)resto);
    bloco[resto] = 0x80;
    if (resto >= 56) {
        md5_bloco(h, bloco);
        memset(bloco, 0, sizeof bloco);
    }
    if (com_tamanho) {
        uint64_t bits = (uint64_t)n * 8;
        for (int j = 0; j < 8; j++)
            bloco[56 + j] = (uint8_t)(bits >> (8 * j));
    }
    md5_bloco(h, bloco);
    for (int j = 0; j < 4; j++) {
        saida[4 * j]     = (uint8_t)h[j];
        saida[4 * j + 1] = (uint8_t)(h[j] >> 8);
        saida[4 * j + 2] = (uint8_t)(h[j] >> 16);
        saida[4 * j + 3] = (uint8_t)(h[j] >> 24);
    }
}

void pdf_md5(const uint8_t *dados, int n, uint8_t *saida)
{
    md5_calcular(dados, n, saida, true);
}

void pdf_md5_bloco(const uint8_t *dados, int n, uint8_t *saida)
{
    md5_calcular(dados, n, saida, false);
}

/* ======================================================== RC4 ========== */

/* O RC4: KSA (o arranjo da permutacao) e PRGA (a keystream). Como e um
 * cifrado de fluxo, cifrar e descifrar sao a mesma coisa. */
void pdf_rc4(const uint8_t *chave, int nchave, const uint8_t *ent, int n, uint8_t *saida)
{
    uint8_t s[256];
    int i, j = 0;
    if (n <= 0)
        return;
    if (nchave <= 0) {
        memset(saida, 0, (size_t)n);
        return;
    }
    for (i = 0; i < 256; i++)
        s[i] = (uint8_t)i;
    for (i = 0; i < 256; i++) {
        uint8_t t;
        j = (j + s[i] + chave[i % nchave]) & 0xff;
        t = s[i];
        s[i] = s[j];
        s[j] = t;
    }
    /* A geracao do fluxo começa com i = j = 0. */
    i = 0;
    j = 0;
    for (int k = 0; k < n; k++) {
        uint8_t t;
        i = (i + 1) & 0xff;
        j = (j + s[i]) & 0xff;
        t = s[i];
        s[i] = s[j];
        s[j] = t;
        saida[k] = (uint8_t)(ent[k] ^ s[(s[i] + s[j]) & 0xff]);
    }
}

/* ======================================================== AES ========== */

/* A S-box e a sua inversa, calculadas (nunca escritas a mao). A S-box sai
 * do inverso multiplicativo em GF(2^8) (x^254) seguido da transformacao
 * afim; a inversa sai da propria permutacao, que e bijectiva. */
static uint8_t aes_s[256];
static uint8_t aes_si[256];
static bool aes_pronto = false;

/* r = a * b em GF(2^8), com o polinomio x^8 + x^4 + x^3 + x + 1. */
static uint8_t gf_mul8(uint8_t a, uint8_t b)
{
    uint8_t r = 0;
    while (b) {
        if (b & 1)
            r ^= a;
        bool alto = (a & 0x80) != 0;
        a = (uint8_t)(a << 1);
        if (alto)
            a ^= 0x1B;
        b = (uint8_t)(b >> 1);
    }
    return r;
}

/* O inverso multiplicativo: x^254 (e 0 fica em 0). */
static uint8_t gf_inverso(uint8_t x)
{
    uint8_t r = 1, base = x;
    int e = 254;
    if (!x)
        return 0;
    while (e) {
        if (e & 1)
            r = gf_mul8(r, base);
        base = gf_mul8(base, base);
        e >>= 1;
    }
    return r;
}

/* A rotacao para a esquerda, dentro do byte. */
static uint8_t rot8(uint8_t x, int n)
{
    return (uint8_t)((x << n) | (x >> (8 - n)));
}

static void aes_preparar(void)
{
    if (aes_pronto)
        return;
    for (int x = 0; x < 256; x++) {
        uint8_t s = gf_inverso((uint8_t)x);
        aes_s[x] = (uint8_t)(s ^ rot8(s, 1) ^ rot8(s, 2) ^ rot8(s, 3) ^ rot8(s, 4) ^ 0x63);
    }
    for (int x = 0; x < 256; x++)
        aes_si[aes_s[x]] = (uint8_t)x;
    aes_pronto = true;
}

/* O xtime: o dobro em GF(2^8). */
static uint8_t xtime(uint8_t b)
{
    return (uint8_t)((b << 1) ^ (0x1B & (uint8_t) - (b >> 7)));
}

/* Expande a chave nas chaves de ronda. Nk = 4 (AES-128) ou 8 (AES-256). */
static bool aes_expandir(const uint8_t *chave, int nchave, uint8_t rk[240], int *rodadas)
{
    int nk, total, i;
    uint8_t rcon = 1;
    if (nchave != 16 && nchave != 32)
        return false;
    nk = nchave / 4;
    *rodadas = nk + 6;
    total = 4 * (*rodadas + 1);
    memcpy(rk, chave, (size_t)nchave);
    for (i = nk; i < total; i++) {
        uint8_t t[4];
        memcpy(t, rk + 4 * (i - 1), 4);
        if (i % nk == 0) {
            uint8_t tmp = t[0];
            t[0] = (uint8_t)(aes_s[t[1]] ^ rcon);
            t[1] = aes_s[t[2]];
            t[2] = aes_s[t[3]];
            t[3] = aes_s[tmp];
            rcon = xtime(rcon);
        } else if (nk > 6 && i % nk == 4) {
            for (int j = 0; j < 4; j++)
                t[j] = aes_s[t[j]];
        }
        for (int j = 0; j < 4; j++)
            rk[4 * i + j] = (uint8_t)(rk[4 * (i - nk) + j] ^ t[j]);
    }
    return true;
}

/* O estado em colunas: e[4 * c + l] e a linha l da coluna c. */
static void shift_rows(uint8_t *e)
{
    uint8_t t[16];
    for (int c = 0; c < 4; c++)
        for (int l = 0; l < 4; l++)
            t[4 * c + l] = e[4 * ((c + l) & 3) + l];
    memcpy(e, t, 16);
}

static void inv_shift_rows(uint8_t *e)
{
    uint8_t t[16];
    for (int c = 0; c < 4; c++)
        for (int l = 0; l < 4; l++)
            t[4 * c + l] = e[4 * ((c - l + 4) & 3) + l];
    memcpy(e, t, 16);
}

static void mix_columns(uint8_t *e)
{
    for (int c = 0; c < 4; c++) {
        uint8_t *p = e + 4 * c;
        uint8_t a0 = p[0], a1 = p[1], a2 = p[2], a3 = p[3];
        uint8_t todos = (uint8_t)(a0 ^ a1 ^ a2 ^ a3);
        p[0] ^= todos ^ xtime((uint8_t)(a0 ^ a1));
        p[1] ^= todos ^ xtime((uint8_t)(a1 ^ a2));
        p[2] ^= todos ^ xtime((uint8_t)(a2 ^ a3));
        p[3] ^= todos ^ xtime((uint8_t)(a3 ^ a0));
    }
}

static void inv_mix_columns(uint8_t *e)
{
    for (int c = 0; c < 4; c++) {
        uint8_t *p = e + 4 * c;
        uint8_t a0 = p[0], a1 = p[1], a2 = p[2], a3 = p[3];
        p[0] = (uint8_t)(gf_mul8(0x0E, a0) ^ gf_mul8(0x0B, a1) ^ gf_mul8(0x0D, a2) ^ gf_mul8(0x09, a3));
        p[1] = (uint8_t)(gf_mul8(0x09, a0) ^ gf_mul8(0x0E, a1) ^ gf_mul8(0x0B, a2) ^ gf_mul8(0x0D, a3));
        p[2] = (uint8_t)(gf_mul8(0x0D, a0) ^ gf_mul8(0x09, a1) ^ gf_mul8(0x0E, a2) ^ gf_mul8(0x0B, a3));
        p[3] = (uint8_t)(gf_mul8(0x0B, a0) ^ gf_mul8(0x0D, a1) ^ gf_mul8(0x09, a2) ^ gf_mul8(0x0E, a3));
    }
}

static void aes_cifrar_bloco(const uint8_t *rk, int nr, const uint8_t ent[16], uint8_t saida[16])
{
    uint8_t e[16];
    memcpy(e, ent, 16);
    for (int i = 0; i < 16; i++)
        e[i] ^= rk[i];
    for (int r = 1; r <= nr; r++) {
        for (int i = 0; i < 16; i++)
            e[i] = aes_s[e[i]];
        shift_rows(e);
        if (r != nr)
            mix_columns(e);
        for (int i = 0; i < 16; i++)
            e[i] ^= rk[16 * r + i];
    }
    memcpy(saida, e, 16);
}

/* O cipher inverso: as operacoes na ordem contraria e o AddRoundKey antes
 * do InvMixColumns (as mesmas chaves de ronda da cifragem). */
static void aes_descifrar_bloco(const uint8_t *rk, int nr, const uint8_t ent[16], uint8_t saida[16])
{
    uint8_t e[16];
    memcpy(e, ent, 16);
    for (int i = 0; i < 16; i++)
        e[i] ^= rk[16 * nr + i];
    for (int r = nr - 1; r >= 1; r--) {
        inv_shift_rows(e);
        for (int i = 0; i < 16; i++)
            e[i] = aes_si[e[i]];
        for (int i = 0; i < 16; i++)
            e[i] ^= rk[16 * r + i];
        inv_mix_columns(e);
    }
    inv_shift_rows(e);
    for (int i = 0; i < 16; i++)
        e[i] = aes_si[e[i]];
    for (int i = 0; i < 16; i++)
        e[i] ^= rk[i];
    memcpy(saida, e, 16);
}

/* O modo CBC, sem relleno. O buffer de saida pode ser o mesmo que o da
 * entrada. Devolve o numero de bytes escritos, ou um erro. */
static int aes_cbc_modo(const uint8_t *chave, int nchave, const uint8_t iv[16],
                        const uint8_t *ent, int n, uint8_t *saida, bool cifrar)
{
    uint8_t rk[240];
    uint8_t cif[16], pl[16], anterior[16];
    int nr, blocos;
    if (nchave != 16 && nchave != 32)
        return PDF_CRIPTO_SOPORTE;
    if (n < 0 || (n % 16) != 0)
        return PDF_CRIPTO_MAL;
    aes_preparar();
    if (!aes_expandir(chave, nchave, rk, &nr))
        return PDF_CRIPTO_SOPORTE;
    memcpy(anterior, iv, 16);
    blocos = n / 16;
    for (int b = 0; b < blocos; b++) {
        memcpy(cif, ent + 16 * b, 16);
        if (cifrar) {
            for (int i = 0; i < 16; i++)
                pl[i] = (uint8_t)(cif[i] ^ anterior[i]);
            aes_cifrar_bloco(rk, nr, pl, cif);
            /* Na cifragem o que sai e o bloco cifrado. */
            memcpy(saida + 16 * b, cif, 16);
        } else {
            aes_descifrar_bloco(rk, nr, cif, pl);
            for (int i = 0; i < 16; i++)
                pl[i] ^= anterior[i];
            memcpy(saida + 16 * b, pl, 16);
        }
        memcpy(anterior, cif, 16);
    }
    return n;
}

int pdf_aes_cbc_descifrar(const uint8_t *chave, int nchave, const uint8_t iv[16],
                           const uint8_t *ent, int n, uint8_t *saida)
{
    int r = aes_cbc_modo(chave, nchave, iv, ent, n, saida, false);
    int pad;
    if (r < 0)
        return r;
    if (n < 16)
        return PDF_CRIPTO_MAL;
    /* O relleno e PKCS#5: o ultimo byte diz quantos ha. */
    pad = saida[n - 1];
    if (pad < 1 || pad > 16)
        return PDF_CRIPTO_MAL;
    for (int i = 0; i < pad; i++)
        if (saida[n - 1 - i] != (uint8_t)pad)
            return PDF_CRIPTO_MAL;
    return n - pad;
}

/* ==================================================== SHA (FIPS 180-4) == */

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static const uint64_t sha512_k[80] = {
    0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc,
    0x3956c25bf348b538, 0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118,
    0xd807aa98a3030242, 0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
    0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 0xc19bf174cf692694,
    0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
    0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
    0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4,
    0xc6e00bf33da88fc2, 0xd5a79147930aa725, 0x06ca6351e003826f, 0x142929670a0e6e70,
    0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
    0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
    0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30,
    0xd192e819d6ef5218, 0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
    0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8,
    0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3,
    0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
    0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b,
    0xca273eceea26619c, 0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178,
    0x06f067aa72176fba, 0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
    0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc, 0x431d67c49c100d4c,
    0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817,
};

static uint32_t sha256_rodar(uint32_t x, int n)
{
    return (uint32_t)((x >> n) | (x << (32 - n)));
}

static uint64_t sha512_rodar(uint64_t x, int n)
{
    return (x >> n) | (x << (64 - n));
}

static void sha256_bloco(uint32_t h[8], const uint8_t b[64])
{
    uint32_t w[64];
    uint32_t a, b1, c, d, e, f, g, hh;
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)b[4 * i] << 24) | ((uint32_t)b[4 * i + 1] << 16) |
               ((uint32_t)b[4 * i + 2] << 8) | (uint32_t)b[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = sha256_rodar(w[i - 15], 7) ^ sha256_rodar(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = sha256_rodar(w[i - 2], 17) ^ sha256_rodar(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = h[0]; b1 = h[1]; c = h[2]; d = h[3];
    e = h[4]; f = h[5]; g = h[6]; hh = h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t s1 = sha256_rodar(e, 6) ^ sha256_rodar(e, 11) ^ sha256_rodar(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = hh + s1 + ch + sha256_k[i] + w[i];
        uint32_t s0 = sha256_rodar(a, 2) ^ sha256_rodar(a, 13) ^ sha256_rodar(a, 22);
        uint32_t maj = (a & b1) ^ (a & c) ^ (b1 & c);
        uint32_t t2 = s0 + maj;
        hh = g; g = f; f = e;
        e = d + t1;
        d = c; c = b1; b1 = a;
        a = t1 + t2;
    }
    h[0] += a; h[1] += b1; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

static void sha512_bloco(uint64_t h[8], const uint8_t b[128])
{
    uint64_t w[80];
    uint64_t a, b1, c, d, e, f, g, hh;
    for (int i = 0; i < 16; i++) {
        w[i] = 0;
        for (int j = 0; j < 8; j++)
            w[i] = (w[i] << 8) | b[8 * i + j];
    }
    for (int i = 16; i < 80; i++) {
        uint64_t s0 = sha512_rodar(w[i - 15], 1) ^ sha512_rodar(w[i - 15], 8) ^ (w[i - 15] >> 7);
        uint64_t s1 = sha512_rodar(w[i - 2], 19) ^ sha512_rodar(w[i - 2], 61) ^ (w[i - 2] >> 6);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = h[0]; b1 = h[1]; c = h[2]; d = h[3];
    e = h[4]; f = h[5]; g = h[6]; hh = h[7];
    for (int i = 0; i < 80; i++) {
        uint64_t s1 = sha512_rodar(e, 14) ^ sha512_rodar(e, 18) ^ sha512_rodar(e, 41);
        uint64_t ch = (e & f) ^ (~e & g);
        uint64_t t1 = hh + s1 + ch + sha512_k[i] + w[i];
        uint64_t s0 = sha512_rodar(a, 28) ^ sha512_rodar(a, 34) ^ sha512_rodar(a, 39);
        uint64_t maj = (a & b1) ^ (a & c) ^ (b1 & c);
        uint64_t t2 = s0 + maj;
        hh = g; g = f; f = e;
        e = d + t1;
        d = c; c = b1; b1 = a;
        a = t1 + t2;
    }
    h[0] += a; h[1] += b1; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

static void sha256_calcular(const uint8_t *dados, int n, uint8_t *saida)
{
    uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    uint8_t bloco[64];
    int i = 0;
    if (n < 0)
        n = 0;
    for (; i + 64 <= n; i += 64)
        sha256_bloco(h, dados + i);
    int resto = n - i;
    memset(bloco, 0, sizeof bloco);
    if (resto > 0)
        memcpy(bloco, dados + i, (size_t)resto);
    bloco[resto] = 0x80;
    if (resto >= 56) {
        sha256_bloco(h, bloco);
        memset(bloco, 0, sizeof bloco);
    }
    uint64_t bits = (uint64_t)n * 8;
    for (int j = 0; j < 8; j++)
        bloco[63 - j] = (uint8_t)(bits >> (8 * j));
    sha256_bloco(h, bloco);
    for (int j = 0; j < 8; j++) {
        saida[4 * j]     = (uint8_t)(h[j] >> 24);
        saida[4 * j + 1] = (uint8_t)(h[j] >> 16);
        saida[4 * j + 2] = (uint8_t)(h[j] >> 8);
        saida[4 * j + 3] = (uint8_t)h[j];
    }
}

static void sha512_calcular(const uint8_t *dados, int n, uint8_t *saida)
{
    uint64_t h[8] = { 0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
                      0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
                      0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
                      0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL };
    uint8_t bloco[128];
    int i = 0;
    if (n < 0)
        n = 0;
    for (; i + 128 <= n; i += 128)
        sha512_bloco(h, dados + i);
    int resto = n - i;
    memset(bloco, 0, sizeof bloco);
    if (resto > 0)
        memcpy(bloco, dados + i, (size_t)resto);
    bloco[resto] = 0x80;
    if (resto >= 112) {
        sha512_bloco(h, bloco);
        memset(bloco, 0, sizeof bloco);
    }
    uint64_t bits = (uint64_t)n * 8;
    /* O comprimento e de 128 bits; so os 64 bits de baixo interessam aqui. */
    for (int j = 0; j < 8; j++)
        bloco[127 - j] = (uint8_t)(bits >> (8 * j));
    sha512_bloco(h, bloco);
    for (int j = 0; j < 8; j++)
        for (int k = 0; k < 8; k++)
            saida[8 * j + k] = (uint8_t)(h[j] >> (56 - 8 * k));
}

/* O SHA-384: o mesmo que o SHA-512 com outro valor inicial e cortando. */
static void sha384_calcular(const uint8_t *dados, int n, uint8_t *saida)
{
    uint64_t h[8] = { 0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL,
                      0x9159015a3070dd17ULL, 0x152fecd8f70e5939ULL,
                      0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL,
                      0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL };
    uint8_t bloco[128];
    int i = 0;
    if (n < 0)
        n = 0;
    for (; i + 128 <= n; i += 128)
        sha512_bloco(h, dados + i);
    int resto = n - i;
    memset(bloco, 0, sizeof bloco);
    if (resto > 0)
        memcpy(bloco, dados + i, (size_t)resto);
    bloco[resto] = 0x80;
    if (resto >= 112) {
        sha512_bloco(h, bloco);
        memset(bloco, 0, sizeof bloco);
    }
    uint64_t bits = (uint64_t)n * 8;
    for (int j = 0; j < 8; j++)
        bloco[127 - j] = (uint8_t)(bits >> (8 * j));
    sha512_bloco(h, bloco);
    for (int j = 0; j < 6; j++)
        for (int k = 0; k < 8; k++)
            saida[8 * j + k] = (uint8_t)(h[j] >> (56 - 8 * k));
}

/* ================================ o gestor de seguranca (32000-1, 7.6) = */

/* O padding de 32 bytes da norma. */
static const uint8_t senha_pad[32] = {
    0x28, 0xBF, 0x4E, 0x5E, 0x4E, 0x75, 0x8A, 0x41,
    0x64, 0x00, 0x4E, 0x56, 0xFF, 0xFA, 0x01, 0x08,
    0x2E, 0x2E, 0x00, 0xB6, 0xD0, 0x68, 0x3E, 0x80,
    0x2F, 0x0C, 0xA9, 0xFE, 0x64, 0x53, 0x69, 0x7A
};

/* Prepara a senha para os hashes. Nas revisoes 5 e 6 a senha vai em UTF-8 e
 * cortada a 127 bytes (ISO 32000-2, algoritmo 2.A). O SASLprep do "Adobe
 * Extension Level 3" nao se aplica: a norma ISO 32000-2 deixou-o de fora e usa
 * so o UTF-8. Nas revisoes antigas os bytes vao tal e qual (Latin-1, que e como
 * a norma guarda a senha) e cortados a 32. */
static int senha_preparar(const char *senha, int r, uint8_t *saida)
{
    int n = 0;
    int limite = (r >= 5) ? 127 : 32;
    if (!senha)
        return 0;
    for (const char *p = senha; *p && n < limite; p++)
        saida[n++] = (unsigned char)*p;
    return n;
}

/* Poe a senha em 32 bytes: a senha e o padding da norma. */
static void senha_32(const uint8_t *senha, int n, uint8_t *saida)
{
    int k = n > 32 ? 32 : n;
    memset(saida, 0, 32);
    if (k > 0)
        memcpy(saida, senha, (size_t)k);
    for (int i = k; i < 32; i++)
        saida[i] = senha_pad[i - k];
}

/* As 50 voltas do MD5 das revisoes 3 e seguintes: hasheia 50 vezes os
 * primeiros "nbytes" do resultado anterior. */
static void md5_voltas(uint8_t h[16], int nbytes)
{
    uint8_t novo[16];
    for (int i = 0; i < 50; i++) {
        pdf_md5(h, nbytes, novo);
        memcpy(h, novo, 16);
    }
}

/* Algoritmo 2: a chave de arquivo a partir da senha de utilizador.
 * MD5 da senha (com 32 bytes) + O + P + o identificador do ficheiro. */
static void chave_arquivo(const pdf_cripto_t *c, const uint8_t *senha, int nsenha,
                           const uint8_t *o, int n_o, int p, const uint8_t *id, int n_id,
                           uint8_t *chave, int nbytes)
{
    uint8_t senha32[32];
    uint8_t buf[192];
    uint8_t hash[16];
    int n = 0;
    senha_32(senha, nsenha, senha32);
    memcpy(buf, senha32, 32);
    n = 32;
    for (int i = 0; i < n_o && i < 32; i++)
        buf[n++] = o[i];
    uint32_t pp = (uint32_t)p;
    /* O /P entram so com os tres bytes de baixo, do mais baixo para o mais
     * alto (a norma 32000-1, algoritmo 2, passo 4). */
    buf[n++] = (uint8_t)pp;
    buf[n++] = (uint8_t)(pp >> 8);
    buf[n++] = (uint8_t)(pp >> 16);
    for (int i = 0; i < n_id && n < (int)sizeof buf - 4; i++)
        buf[n++] = id[i];
    if (c->r >= 4 && !c->cifrar_meta) {
        buf[n++] = 0xFF;
        buf[n++] = 0xFF;
        buf[n++] = 0xFF;
        buf[n++] = 0xFF;
    }
    pdf_md5(buf, n, hash);
    if (c->r >= 3)
        md5_voltas(hash, nbytes);
    memcpy(chave, hash, (size_t)nbytes);
}

/* O /U que a norma calcula para a senha de utilizador (algoritmo 4). */
static void valor_u(const pdf_cripto_t *c, const uint8_t *chave, int nbytes,
                    const uint8_t *id, int n_id, uint8_t *u)
{
    if (c->r == 2) {
        memcpy(u, senha_pad, 32);
        pdf_rc4(chave, nbytes, u, 32, u);
    } else {
        uint8_t buf[96];
        int n = 0;
        memcpy(buf, senha_pad, 32);
        n = 32;
        for (int i = 0; i < n_id && n < (int)sizeof buf; i++)
            buf[n++] = id[i];
        pdf_md5(buf, n, u);
        /* Vinte voltas de RC4, com a chave mudada (chave ^ i). */
        for (int i = 0; i < 20; i++) {
            uint8_t k[16];
            for (int j = 0; j < nbytes; j++)
                k[j] = (uint8_t)(chave[j] ^ i);
            pdf_rc4(k, nbytes, u, 16, u);
        }
    }
}

/* Confere a senha de utilizador (algoritmo 4): deriva a chave de arquivo e
 * compara o /U calculado com o do ficheiro. Em R=2 comparam-se os 32
 * bytes; em R>=3, os 16 primeiros (os resto e arbitrario). */
static bool senha_utilizador(const pdf_cripto_t *c, const uint8_t *senha, int nsenha,
                              const uint8_t *o, int n_o, int p, const uint8_t *id, int n_id,
                              const uint8_t *u, int n_u, uint8_t *chave, int nbytes)
{
    uint8_t u_calculado[32];
    if (n_u < 16)
        return false;
    chave_arquivo(c, senha, nsenha, o, n_o, p, id, n_id, chave, nbytes);
    valor_u(c, chave, nbytes, id, n_id, u_calculado);
    if (c->r == 2) {
        int n = n_u < 32 ? n_u : 32;
        return memcmp(u, u_calculado, (size_t)n) == 0;
    }
    return memcmp(u, u_calculado, 16) == 0;
}

/* Confere a senha de proprietario (algoritmo 5): tira do /O a senha de
 * utilizador e depois confere essa. */
static bool senha_proprietario(const pdf_cripto_t *c, const uint8_t *senha, int nsenha,
                                const uint8_t *o, int n_o, int p, const uint8_t *id, int n_id,
                                const uint8_t *u, int n_u, uint8_t *chave, int nbytes)
{
    uint8_t senha32[32];
    uint8_t hash[16];
    uint8_t utilizador[32];
    if (n_o < 32)
        return false;
    senha_32(senha, nsenha, senha32);
    pdf_md5(senha32, 32, hash);
    if (c->r >= 3)
        md5_voltas(hash, nbytes);
    memcpy(utilizador, o, 32);
    /* O RC4 ao contrario: as chaves vao de (chave ^ 19) ate (chave ^ 0). */
    int voltas = (c->r >= 3) ? 20 : 1;
    for (int i = 0; i < voltas; i++) {
        int x = (c->r >= 3) ? (voltas - 1 - i) : 0;
        uint8_t k[16];
        for (int j = 0; j < nbytes; j++)
            k[j] = (uint8_t)(hash[j] ^ x);
        pdf_rc4(k, nbytes, utilizador, 32, utilizador);
    }
    return senha_utilizador(c, utilizador, 32, o, n_o, p, id, n_id, u, n_u, chave, nbytes);
}

/* O "hash 2.B" da ISO 32000-2. Na revisao 5 e o SHA-256 da senha com o
 * sal; na revisao 6, alem do SHA-256 inicial, ha o ciclo de 64 voltas (ou
 * mais) da norma: cada volta cifra (senha + K + dados) repetido 64 vezes
 * com AES-128-CBC (chave K[0..15], IV K[16..31]) e o resultado E e
 * hasheado com SHA-256, SHA-384 ou SHA-512, conforme a soma dos 16
 * primeiros bytes de E modulo 3. */
static void hash_2b(int r, const uint8_t *senha, int nsenha, const uint8_t *sal, int nsal,
                    const uint8_t *dados, int ndados, uint8_t *saida)
{
    uint8_t k[64];
    uint8_t inicio[192];
    uint8_t *k1 = NULL, *e = NULL;
    int nk = 32;
    int n = 0;
    memcpy(inicio, senha, (size_t)nsenha);
    n = nsenha;
    memcpy(inicio + n, sal, (size_t)nsal);
    n += nsal;
    if (ndados > 0) {
        memcpy(inicio + n, dados, (size_t)ndados);
        n += ndados;
    }
    sha256_calcular(inicio, n, k);
    if (r < 6) {
        memcpy(saida, k, 32);
        return;
    }
    /* Revisao 6: o ciclo. A sequencia que se cifra e (senha + K + dados) e o
     * comprimento de K muda (32, 48 ou 64 bytes) conforme o hash da volta
     * anterior, por isso o comprimento se recalcula a cada volta. */
    int comp_max = nsenha + 64 + ndados;
    k1 = aloca((size_t)comp_max * 64);
    e = aloca((size_t)comp_max * 64);
    if (!k1 || !e) {
        if (k1)
            libera(k1);
        if (e)
            libera(e);
        memset(saida, 0, 32);
        return;
    }
    aes_preparar();
    int voltas = 0;
    for (;;) {
        int comp = nsenha + nk + ndados;
        memcpy(k1, senha, (size_t)nsenha);
        memcpy(k1 + nsenha, k, (size_t)nk);
        if (ndados > 0)
            memcpy(k1 + nsenha + nk, dados, (size_t)ndados);
        for (int i = 1; i < 64; i++)
            memcpy(k1 + i * comp, k1, (size_t)comp);
        if (aes_cbc_modo(k, 16, k + 16, k1, comp * 64, e, true) < 0)
            break;
        int soma = 0;
        for (int i = 0; i < 16; i++)
            soma += e[i];
        switch (soma % 3) {
            case 0:
                sha256_calcular(e, comp * 64, k);
                nk = 32;
                break;
            case 1:
                sha384_calcular(e, comp * 64, k);
                nk = 48;
                break;
            default:
                sha512_calcular(e, comp * 64, k);
                nk = 64;
                break;
        }
        voltas++;
        if (voltas >= 64 && e[comp * 64 - 1] <= (uint8_t)(voltas - 32))
            break;
    }
    memcpy(saida, k, 32);
    libera(k1);
    libera(e);
}

/* A senha de utilizador ou de proprietario confere? (revisoes 5 e 6) */
static bool senha_v5(const pdf_cripto_t *c, const uint8_t *senha, int nsenha,
                     const uint8_t *o, int n_o, const uint8_t *u, int n_u)
{
    uint8_t hash[32];
    if (n_u >= 48) {
        hash_2b(c->r, senha, nsenha, u + 32, 8, NULL, 0, hash);
        if (memcmp(hash, u, 32) == 0)
            return true;
    }
    if (n_o >= 48 && n_u >= 48) {
        hash_2b(c->r, senha, nsenha, o + 32, 8, u, 48, hash);
        if (memcmp(hash, o, 32) == 0)
            return true;
    }
    return false;
}

/* Recupera a chave de arquivo nas revisoes 5 e 6: o hash 2.B da senha com
 * o sal da chave dá a chave intermedia, que descifra o /UE (ou o /OE). */
static bool chave_arquivo_v5(const pdf_cripto_t *c, const uint8_t *senha, int nsenha,
                             const uint8_t *o, int n_o, const uint8_t *u, int n_u,
                             const uint8_t *oe, int n_oe, const uint8_t *ue, int n_ue,
                             uint8_t *chave)
{
    uint8_t hash[32];
    uint8_t intermedio[32];
    const uint8_t *sal = NULL;
    const uint8_t *dados = NULL;
    const uint8_t *cifrado = NULL;
    int ndados = 0, ncifrado = 0;
    if (n_u < 48)
        return false;
    /* Senha de utilizador: sal em U[32..39], chave cifrada em /UE. */
    hash_2b(c->r, senha, nsenha, u + 32, 8, NULL, 0, hash);
    if (memcmp(hash, u, 32) == 0 && n_ue >= 32) {
        sal = u + 40;
        cifrado = ue;
        ncifrado = 32;
    } else {
        /* Senha de proprietario: sal em O[32..39], dados em U[0..47]. */
        if (n_o < 48)
            return false;
        hash_2b(c->r, senha, nsenha, o + 32, 8, u, 48, hash);
        if (memcmp(hash, o, 32) != 0 || n_oe < 32)
            return false;
        sal = o + 40;
        dados = u;
        ndados = 48;
        cifrado = oe;
        ncifrado = 32;
    }
    hash_2b(c->r, senha, nsenha, sal, 8, dados, ndados, intermedio);
    uint8_t iv[16];
    memset(iv, 0, 16);
    if (aes_cbc_modo(intermedio, 32, iv, cifrado, ncifrado, chave, false) < 0)
        return false;
    return true;
}

/* ============================== a interface publica ===================== */

/* Algoritmo 1: a chave de um objecto. Para o RC4 e o AES-128 sai do MD5 da
 * chave de arquivo com o numero e a geracao do objecto (e o "sAlT" no
 * AES); no V=5 (AES-256) a chave do objecto e a propria chave de arquivo. */
void pdf_cripto_chave_objeto(const pdf_cripto_t *c, int num, int gen, uint8_t *saida, int *n)
{
    uint8_t buf[64];
    uint8_t hash[16];
    int m = 0, k;
    if (c->metodo == 3) {
        int nch = c->nchave > 32 ? 32 : c->nchave;
        memcpy(saida, c->chave, (size_t)nch);
        *n = nch;
        return;
    }
    memcpy(buf, c->chave, (size_t)c->nchave);
    m = c->nchave;
    buf[m++] = (uint8_t)(num & 0xff);
    buf[m++] = (uint8_t)((num >> 8) & 0xff);
    buf[m++] = (uint8_t)((num >> 16) & 0xff);
    buf[m++] = (uint8_t)(gen & 0xff);
    buf[m++] = (uint8_t)((gen >> 8) & 0xff);
    if (c->metodo == 2) {
        buf[m++] = 0x73;   /* "sAlT" */
        buf[m++] = 0x41;
        buf[m++] = 0x6C;
        buf[m++] = 0x54;
    }
    pdf_md5(buf, m, hash);
    k = (c->metodo == 2) ? 16 : c->nchave + 5;
    if (k > 16)
        k = 16;
    memcpy(saida, hash, (size_t)k);
    *n = k;
}

/* Descifra os dados de um objecto (uma cadeia ou o conteudo de um stream).
 * Devolve o numero de bytes que ficam validos no buffer, ou um erro. */
int pdf_cripto_descifrar(const pdf_cripto_t *c, uint8_t *dados, int n, int num, int gen)
{
    uint8_t chave[32];
    int nc = 0;
    if (!c || !dados)
        return PDF_CRIPTO_MAL;
    if (c->metodo == 0)
        return n;
    if (n < 0)
        return PDF_CRIPTO_MAL;
    pdf_cripto_chave_objeto(c, num, gen, chave, &nc);
    if (c->metodo == 1) {
        pdf_rc4(chave, nc, dados, n, dados);
        return n;
    }
    if (c->metodo == 2 || c->metodo == 3) {
        uint8_t iv[16], cif[16], pl[16], anterior[16];
        uint8_t rk[240];
        int nr, blocos;
        if (n < 16)
            return PDF_CRIPTO_MAL;
        if (nc != 16 && nc != 32)
            return PDF_CRIPTO_SOPORTE;
        aes_preparar();
        if (!aes_expandir(chave, nc, rk, &nr))
            return PDF_CRIPTO_SOPORTE;
        memcpy(iv, dados, 16);
        memcpy(anterior, iv, 16);
        blocos = (n - 16) / 16;
        for (int b = 0; b < blocos; b++) {
            memcpy(cif, dados + 16 + 16 * b, 16);
            aes_descifrar_bloco(rk, nr, cif, pl);
            for (int i = 0; i < 16; i++)
                pl[i] ^= anterior[i];
            memcpy(dados + 16 * b, pl, 16);
            memcpy(anterior, cif, 16);
        }
        /* O relleno PKCS#5: o ultimo byte diz quantos ha. */
        int total = 16 * blocos;
        if (total < 16)
            return PDF_CRIPTO_MAL;
        int pad = dados[total - 1];
        if (pad < 1 || pad > 16)
            return PDF_CRIPTO_MAL;
        for (int i = 0; i < pad; i++)
            if (dados[total - 1 - i] != (uint8_t)pad)
                return PDF_CRIPTO_MAL;
        return total - pad;
    }
    return PDF_CRIPTO_SOPORTE;
}

/* Prepara o cifrado de um documento. */
int pdf_cripto_preparar(pdf_cripto_t *c, const char *contrasena,
                        const uint8_t *o, int n_o,
                        const uint8_t *u, int n_u,
                        int p, int v, int r, int length,
                        const uint8_t *id, int n_id,
                        bool cifrar_meta)
{
    uint8_t senha[128];
    uint8_t chave[32];
    int nsenha, nbytes, bits;
    if (!c)
        return PDF_CRIPTO_MAL;
    memset(c, 0, sizeof *c);
    c->versao = v;
    c->r = r;
    c->cifrar_meta = cifrar_meta;
    if (r < 2 || r > 6)
        return PDF_CRIPTO_SOPORTE;
    if (v == 1 || v == 2)
        c->metodo = 1;    /* RC4 */
    else if (v == 4)
        c->metodo = 2;    /* AES-128 */
    else if (v == 5)
        c->metodo = 3;    /* AES-256 */
    else
        return PDF_CRIPTO_SOPORTE;
    nsenha = senha_preparar(contrasena, r, senha);
    if (v == 5) {
        /* No V=5 a chave de arquivo vai cifrada no /OE e no /UE, que estao
         * fora desta interface: usa-se a funcao especifica. Aqui apenas se
         * diz se a senha e a certa. */
        if (!senha_v5(c, senha, nsenha, o, n_o, u, n_u))
            return PDF_CRIPTO_MAL;
        return PDF_CRIPTO_SOPORTE;
    }
    bits = length;
    if (bits <= 0)
        bits = (r == 2) ? 40 : 128;
    if (v == 1)
        bits = 40;
    nbytes = bits / 8;
    if (nbytes < 5)
        nbytes = 5;
    if (nbytes > 16)
        nbytes = 16;
    if (senha_utilizador(c, senha, nsenha, o, n_o, p, id, n_id, u, n_u, chave, nbytes) ||
        senha_proprietario(c, senha, nsenha, o, n_o, p, id, n_id, u, n_u, chave, nbytes)) {
        memcpy(c->chave, chave, (size_t)nbytes);
        c->nchave = nbytes;
        return PDF_CRIPTO_OK;
    }
    return PDF_CRIPTO_MAL;
}

/* O mesmo que pdf_cripto_preparar, mas para o V=5 (revisoes 5 e 6), onde a
 * chave de arquivo esta cifrada no /OE e no /UE e por isso sao precisos. */
int pdf_cripto_preparar_v5(pdf_cripto_t *c, const char *contrasena,
                           const uint8_t *o, int n_o,
                           const uint8_t *u, int n_u,
                           const uint8_t *oe, int n_oe,
                           const uint8_t *ue, int n_ue,
                           int p, int v, int r, bool cifrar_meta)
{
    uint8_t senha[128];
    uint8_t chave[32];
    int nsenha;
    (void)p;   /* as permissoes (/P) nao entram na derivacao da chave no V=5 */
    if (!c)
        return PDF_CRIPTO_MAL;
    memset(c, 0, sizeof *c);
    c->versao = v;
    c->r = r;
    c->cifrar_meta = cifrar_meta;
    if (v != 5 || (r != 5 && r != 6))
        return PDF_CRIPTO_SOPORTE;
    c->metodo = 3;    /* AES-256 */
    nsenha = senha_preparar(contrasena, r, senha);
    if (!chave_arquivo_v5(c, senha, nsenha, o, n_o, u, n_u, oe, n_oe, ue, n_ue, chave))
        return PDF_CRIPTO_MAL;
    memcpy(c->chave, chave, 32);
    c->nchave = 32;
    return PDF_CRIPTO_OK;
}
