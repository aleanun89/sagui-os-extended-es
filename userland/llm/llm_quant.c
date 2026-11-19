// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_quant.c - bloques cuantizados de GGUF (D380): decuantizar filas, cuantizar la
 * entrada y los productos punto en C puro (la ruta de las CPU sin SSE2).
 *
 * Los formatos son los de ggml (los define el fichero, no el código): una fila es una
 * sucesión de bloques; cada bloque guarda una escala (y a veces un mínimo) en f16 y los
 * valores en pocos bits.
 *   Q4_0  32 valores: d, 16 bytes con dos valores de 4 bits; v = d * (q - 8).
 *   Q8_0  32 valores: d, 32 bytes con signo; v = d * q.
 *   Q4_K  256 valores en 8 subbloques de 32: d, dmin, 12 bytes con la escala y el mínimo
 *         de cada subbloque (6 bits cada uno) y 128 bytes de valores de 4 bits;
 *         v = d * esc * q - dmin * min.
 *   Q6_K  256 valores en 16 subbloques de 16: 128 bytes con los 4 bits bajos, 64 con
 *         los 2 altos, 16 escalas de 8 bits con signo y d; v = d * esc * (q - 32).
 * La entrada se cuantiza a Q8_K (bloques de 256: d en float, 256 valores con signo y la
 * suma de cada grupo de 16) para las filas K y a Q8_0 para las de 32: así el producto
 * es casi todo con enteros, como en llama.cpp, y la ruta SSE2 da los mismos enteros. */
#include "llm.h"

float llm_f16_tab[65536];

float llm_f16_f32_calc(uint16_t h)
{
    uint32_t s = (uint32_t)(h & 0x8000) << 16, e = (h >> 10) & 0x1F, m = h & 0x3FF, b;
    if (e == 0) {
        if (m == 0) b = s;
        else {   /* subnormal: se normaliza */
            e = 127 - 15 + 1;
            while (!(m & 0x400)) { m <<= 1; e--; }
            m &= 0x3FF;
            b = s | e << 23 | m << 13;
        }
    } else if (e == 31) b = s | 0x7F800000u | m << 13;
    else b = s | (e + 127 - 15) << 23 | m << 13;
    float f;
    llm_memcpy(&f, &b, 4);
    return f;
}

uint16_t llm_f32_f16(float f)
{
    uint32_t b;
    llm_memcpy(&b, &f, 4);
    uint32_t s = (b >> 16) & 0x8000;
    int e = (int)((b >> 23) & 0xFF) - 127 + 15;
    uint32_t m = b & 0x7FFFFF;
    if (((b >> 23) & 0xFF) == 0xFF) return (uint16_t)(s | 0x7C00 | (m ? 0x200 : 0));
    if (e >= 31) return (uint16_t)(s | 0x7C00);
    if (e <= 0) {
        if (e < -10) return (uint16_t)s;
        m |= 0x800000;
        uint32_t desl = (uint32_t)(14 - e);
        uint32_t r = m >> desl, resto = m & ((1u << desl) - 1), meio = 1u << (desl - 1);
        if (resto > meio || (resto == meio && (r & 1))) r++;
        return (uint16_t)(s | r);
    }
    uint32_t r = (uint32_t)e << 10 | m >> 13, resto = m & 0x1FFF;
    if (resto > 0x1000 || (resto == 0x1000 && (r & 1))) r++;   /* al par más cercano */
    return (uint16_t)(s | r);
}

static int redondear(float v) { return v >= 0 ? (int)(v + 0.5f) : -(int)(-v + 0.5f); }

void llm_quant_q8_K(const float *x, bloco_q8_K *y, int n)
{
    for (int b = 0; b < n / QK_K; b++, x += QK_K, y++) {
        float amax = 0, max = 0;
        for (int j = 0; j < QK_K; j++) {
            float a = llm_fabsf(x[j]);
            if (a > amax) { amax = a; max = x[j]; }
        }
        if (amax == 0) {
            y->d = 0;
            llm_memset(y->qs, 0, sizeof y->qs);
            llm_memset(y->bsums, 0, sizeof y->bsums);
            continue;
        }
        const float iscale = -128.0f / max;
        for (int j = 0; j < QK_K; j++) {
            int v = redondear(iscale * x[j]);
            y->qs[j] = (int8_t)(v > 127 ? 127 : v < -128 ? -128 : v);
        }
        for (int j = 0; j < QK_K / 16; j++) {
            int s = 0;
            for (int i = 0; i < 16; i++) s += y->qs[j * 16 + i];
            y->bsums[j] = (int16_t)s;
        }
        y->d = 1.0f / iscale;
    }
}

void llm_quant_q8_0(const float *x, bloco_q8_0 *y, int n)
{
    for (int b = 0; b < n / QK8_0; b++, x += QK8_0, y++) {
        float amax = 0;
        for (int j = 0; j < QK8_0; j++) { float a = llm_fabsf(x[j]); if (a > amax) amax = a; }
        const float d = amax / 127.0f, id = d ? 1.0f / d : 0.0f;
        y->d = llm_f32_f16(d);
        for (int j = 0; j < QK8_0; j++) y->qs[j] = (int8_t)redondear(x[j] * id);
    }
}

/* Escala y mínimo del subbloque j de un Q4_K (6 bits cada uno, repartidos en 12 bytes). */
static inline void escala_min_k4(int j, const uint8_t *q, int *d, int *m)
{
    if (j < 4) { *d = q[j] & 63; *m = q[j + 4] & 63; }
    else {
        *d = (q[j + 4] & 0xF) | ((q[j - 4] >> 6) << 4);
        *m = (q[j + 4] >> 4) | ((q[j] >> 6) << 4);
    }
}

void llm_decuant_linha(uint32_t tipo, const void *src, float *y, int n)
{
    switch (tipo) {
    case T_F32: llm_memcpy(y, src, (size_t)n * 4); return;
    case T_F16: { const uint16_t *h = src; for (int i = 0; i < n; i++) y[i] = llm_f16_f32(h[i]); return; }
    case T_Q8_0: {
        const bloco_q8_0 *b = src;
        for (int i = 0; i < n / QK8_0; i++, b++) {
            float d = llm_f16_f32(b->d);
            for (int j = 0; j < QK8_0; j++) *y++ = d * b->qs[j];
        }
        return;
    }
    case T_Q4_0: {
        const bloco_q4_0 *b = src;
        for (int i = 0; i < n / QK8_0; i++, b++) {
            float d = llm_f16_f32(b->d);
            for (int j = 0; j < 16; j++) {
                y[j] = d * ((b->qs[j] & 0xF) - 8);
                y[j + 16] = d * ((b->qs[j] >> 4) - 8);
            }
            y += 32;
        }
        return;
    }
    case T_Q4_K: {
        const bloco_q4_K *b = src;
        for (int i = 0; i < n / QK_K; i++, b++) {
            const float d = llm_f16_f32(b->d), dmin = llm_f16_f32(b->dmin);
            const uint8_t *q = b->qs;
            for (int j = 0; j < QK_K; j += 64) {
                int sc, m;
                escala_min_k4(j / 32, b->scales, &sc, &m);
                const float d1 = d * sc, m1 = dmin * m;
                escala_min_k4(j / 32 + 1, b->scales, &sc, &m);
                const float d2 = d * sc, m2 = dmin * m;
                for (int l = 0; l < 32; l++) *y++ = d1 * (q[l] & 0xF) - m1;
                for (int l = 0; l < 32; l++) *y++ = d2 * (q[l] >> 4) - m2;
                q += 32;
            }
        }
        return;
    }
    case T_Q6_K: {
        const bloco_q6_K *b = src;
        for (int i = 0; i < n / QK_K; i++, b++) {
            const float d = llm_f16_f32(b->d);
            const uint8_t *ql = b->ql, *qh = b->qh;
            const int8_t *sc = b->scales;
            for (int h = 0; h < QK_K; h += 128) {
                for (int l = 0; l < 32; l++) {
                    int is = l / 16;
                    int q1 = (int)((ql[l] & 0xF) | (((qh[l] >> 0) & 3) << 4)) - 32;
                    int q2 = (int)((ql[l + 32] & 0xF) | (((qh[l] >> 2) & 3) << 4)) - 32;
                    int q3 = (int)((ql[l] >> 4) | (((qh[l] >> 4) & 3) << 4)) - 32;
                    int q4 = (int)((ql[l + 32] >> 4) | (((qh[l] >> 6) & 3) << 4)) - 32;
                    y[l] = d * sc[is] * q1;
                    y[l + 32] = d * sc[is + 2] * q2;
                    y[l + 64] = d * sc[is + 4] * q3;
                    y[l + 96] = d * sc[is + 6] * q4;
                }
                y += 128; ql += 64; qh += 32; sc += 8;
            }
        }
        return;
    }
    default:
        llm_memset(y, 0, (size_t)n * 4);
    }
}

/* ------------------------------------------------------------ productos punto en C */

float llm_dot_q8_0_c(const void *w, const void *x, int n)
{
    const bloco_q8_0 *a = w, *b = x;
    float s = 0;
    for (int i = 0; i < n / QK8_0; i++, a++, b++) {
        int si = 0;
        for (int j = 0; j < QK8_0; j++) si += a->qs[j] * b->qs[j];
        s += (float)si * (llm_f16_f32(a->d) * llm_f16_f32(b->d));
    }
    return s;
}

float llm_dot_q4_0_c(const void *w, const void *x, int n)
{
    const bloco_q4_0 *a = w;
    const bloco_q8_0 *b = x;
    float s = 0;
    for (int i = 0; i < n / QK8_0; i++, a++, b++) {
        int si = 0;
        for (int j = 0; j < 16; j++)
            si += ((a->qs[j] & 0xF) - 8) * b->qs[j] + ((a->qs[j] >> 4) - 8) * b->qs[j + 16];
        s += (float)si * (llm_f16_f32(a->d) * llm_f16_f32(b->d));
    }
    return s;
}

float llm_dot_q4_K_c(const void *w, const void *x, int n)
{
    const bloco_q4_K *a = w;
    const bloco_q8_K *b = x;
    float s = 0;
    for (int i = 0; i < n / QK_K; i++, a++, b++) {
        const uint8_t *q4 = a->qs;
        const int8_t *q8 = b->qs;
        int sumi = 0, summs = 0;
        for (int j = 0; j < 4; j++) {
            int sc1, m1, sc2, m2, s1 = 0, s2 = 0;
            escala_min_k4(2 * j, a->scales, &sc1, &m1);
            escala_min_k4(2 * j + 1, a->scales, &sc2, &m2);
            for (int l = 0; l < 32; l++) { s1 += (q4[l] & 0xF) * q8[l]; s2 += (q4[l] >> 4) * q8[l + 32]; }
            sumi += sc1 * s1 + sc2 * s2;
            summs += m1 * (b->bsums[4 * j] + b->bsums[4 * j + 1]) + m2 * (b->bsums[4 * j + 2] + b->bsums[4 * j + 3]);
            q4 += 32; q8 += 64;
        }
        s += b->d * (llm_f16_f32(a->d) * (float)sumi - llm_f16_f32(a->dmin) * (float)summs);
    }
    return s;
}

float llm_dot_q6_K_c(const void *w, const void *x, int n)
{
    const bloco_q6_K *a = w;
    const bloco_q8_K *b = x;
    float s = 0;
    for (int i = 0; i < n / QK_K; i++, a++, b++) {
        const uint8_t *ql = a->ql, *qh = a->qh;
        const int8_t *sc = a->scales, *q8 = b->qs;
        int sumi = 0;
        for (int h = 0; h < 2; h++) {
            int s16[8] = { 0 };
            for (int l = 0; l < 32; l++) {
                int is = l / 16;
                int q1 = (int)((ql[l] & 0xF) | (((qh[l] >> 0) & 3) << 4)) - 32;
                int q2 = (int)((ql[l + 32] & 0xF) | (((qh[l] >> 2) & 3) << 4)) - 32;
                int q3 = (int)((ql[l] >> 4) | (((qh[l] >> 4) & 3) << 4)) - 32;
                int q4 = (int)((ql[l + 32] >> 4) | (((qh[l] >> 6) & 3) << 4)) - 32;
                s16[is + 0] += q1 * q8[l];
                s16[is + 2] += q2 * q8[l + 32];
                s16[is + 4] += q3 * q8[l + 64];
                s16[is + 6] += q4 * q8[l + 96];
            }
            for (int g = 0; g < 8; g++) sumi += sc[g] * s16[g];
            ql += 64; qh += 32; sc += 8; q8 += 128;
        }
        s += b->d * llm_f16_f32(a->d) * (float)sumi;
    }
    return s;
}

/* ------------------------------------------------------------ elección de la ruta */
/* Las rutas, de la mejor a la peor; cada una solo si la CPU la tiene. AVX2 (D381) solo si,
 * además, el kernel guarda los YMM (llm_cpu_avx2 lo mira con XGETBV). */
typedef struct { const char *nome; bool (*tem)(void); llm_dot_t q4_K, q6_K, q8_0, q4_0; } rota_t;
static bool sempre(void) { return true; }
static const rota_t rotas[] = {
    { "AVX2", llm_cpu_avx2, llm_dot_q4_K_avx2, llm_dot_q6_K_avx2, llm_dot_q8_0_avx2, llm_dot_q4_0_avx2 },
    { "SSSE3", llm_cpu_ssse3, llm_dot_q4_K_ssse3, llm_dot_q6_K_ssse3, llm_dot_q8_0_ssse3, llm_dot_q4_0_ssse3 },
    { "SSE2", llm_cpu_sse2, llm_dot_q4_K_sse2, llm_dot_q6_K_sse2, llm_dot_q8_0_sse2, llm_dot_q4_0_sse2 },
    { "C (x87)", sempre, llm_dot_q4_K_c, llm_dot_q6_K_c, llm_dot_q8_0_c, llm_dot_q4_0_c },
};
#define N_ROTAS ((int)(sizeof rotas / sizeof rotas[0]))
llm_dot_t llm_dot_q4_K = llm_dot_q4_K_c, llm_dot_q6_K = llm_dot_q6_K_c, llm_dot_q8_0 = llm_dot_q8_0_c,
          llm_dot_q4_0 = llm_dot_q4_0_c;
static const char *rota = "C (x87)";

/* nome NULL: la mejor que tenga la CPU; si no, esa ("c", "sse2", "ssse3", "avx2"). false si no la tiene. */
bool llm_quant_escolher(const char *nome)
{
    if (llm_f16_tab[0x3C00] != 1.0f)
        for (uint32_t h = 0; h < 65536; h++) llm_f16_tab[h] = llm_f16_f32_calc((uint16_t)h);
    for (int i = 0; i < N_ROTAS; i++) {
        const rota_t *r = &rotas[i];
        if (nome) {
            const char *a = nome, *b = r->nome;
            while (*a && (*a == *b || *a == *b + 32)) { a++; b++; }
            if (*a || (*b && *b != ' ')) continue;
        }
        if (!r->tem()) { if (nome) return false; continue; }   /* sin nombre: la siguiente */
        llm_dot_q4_K = r->q4_K; llm_dot_q6_K = r->q6_K; llm_dot_q8_0 = r->q8_0; llm_dot_q4_0 = r->q4_0;
        rota = r->nome;
        return true;
    }
    return false;
}

const char *llm_quant_rota(void) { return rota; }
