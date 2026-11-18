// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_quant_ssse3.c - productos punto con SSSE3 (D380): pmaddubsw multiplica bytes sin signo
 * (los pesos de 4 o 6 bits) por bytes con signo (la entrada Q8) y suma por pares a 16 bits, y
 * pmaddwd aplica la escala de cada subbloque sumando a 32 bits. La mitad de instrucciones que
 * la ruta SSE2 (que tiene que pasar todo a 16 bits antes).
 *
 * Solo se compila con -mssse3 y solo se llama si CPUID lo dice (llm_quant.c). Da los mismos
 * enteros que la ruta en C: los pares no saturan (63 × 128 × 2 < 32767 en Q6_K; en Q8_0 los
 * valores van de -127 a 127, como los cuantiza ggml y este motor). */
#include "llm.h"
#define _MM_MALLOC_H_INCLUDED
#include <tmmintrin.h>

static inline int soma_i32(__m128i v)
{
    v = _mm_add_epi32(v, _mm_shuffle_epi32(v, _MM_SHUFFLE(1, 0, 3, 2)));
    v = _mm_add_epi32(v, _mm_shuffle_epi32(v, _MM_SHUFFLE(2, 3, 0, 1)));
    return _mm_cvtsi128_si32(v);
}

/* 16 pesos sin signo · 16 bytes de entrada, con una escala: 4 sumas de 32 bits. */
static inline __m128i dot16(__m128i u, const int8_t *q8, __m128i esc)
{
    return _mm_madd_epi16(_mm_maddubs_epi16(u, _mm_loadu_si128((const __m128i *)q8)), esc);
}

float llm_dot_q8_0_ssse3(const void *w, const void *x, int n)
{
    const bloco_q8_0 *a = w, *b = x;
    const __m128i um = _mm_set1_epi16(1);
    float s = 0;
    for (int i = 0; i < n / QK8_0; i++, a++, b++) {
        __m128i acc = _mm_setzero_si128();
        for (int k = 0; k < 2; k++) {
            __m128i va = _mm_loadu_si128((const __m128i *)(a->qs + 16 * k));
            __m128i vb = _mm_loadu_si128((const __m128i *)(b->qs + 16 * k));
            /* |a| · (b con el signo de a) = a · b */
            acc = _mm_add_epi32(acc, _mm_madd_epi16(_mm_maddubs_epi16(_mm_abs_epi8(va), _mm_sign_epi8(vb, va)), um));
        }
        s += (float)soma_i32(acc) * (llm_f16_f32(a->d) * llm_f16_f32(b->d));
    }
    return s;
}

float llm_dot_q4_0_ssse3(const void *w, const void *x, int n)
{
    const bloco_q4_0 *a = w;
    const bloco_q8_0 *b = x;
    const __m128i m4 = _mm_set1_epi8(0xF), um = _mm_set1_epi16(1), uns = _mm_set1_epi8(1);
    float s = 0;
    for (int i = 0; i < n / QK8_0; i++, a++, b++) {
        __m128i q = _mm_loadu_si128((const __m128i *)a->qs);
        __m128i lo = _mm_and_si128(q, m4), hi = _mm_and_si128(_mm_srli_epi16(q, 4), m4);
        __m128i b0 = _mm_loadu_si128((const __m128i *)b->qs), b1 = _mm_loadu_si128((const __m128i *)(b->qs + 16));
        __m128i p = _mm_add_epi16(_mm_maddubs_epi16(lo, b0), _mm_maddubs_epi16(hi, b1));
        __m128i sb = _mm_add_epi16(_mm_maddubs_epi16(uns, b0), _mm_maddubs_epi16(uns, b1));   /* la suma de la entrada */
        int si = soma_i32(_mm_madd_epi16(p, um)) - 8 * soma_i32(_mm_madd_epi16(sb, um));
        s += (float)si * (llm_f16_f32(a->d) * llm_f16_f32(b->d));
    }
    return s;
}

static inline void escala_min_k4(int j, const uint8_t *q, int *d, int *m)
{
    if (j < 4) { *d = q[j] & 63; *m = q[j + 4] & 63; }
    else {
        *d = (q[j + 4] & 0xF) | ((q[j - 4] >> 6) << 4);
        *m = (q[j + 4] >> 4) | ((q[j] >> 6) << 4);
    }
}

float llm_dot_q4_K_ssse3(const void *w, const void *x, int n)
{
    const bloco_q4_K *a = w;
    const bloco_q8_K *b = x;
    const __m128i m4 = _mm_set1_epi8(0xF);
    float s = 0;
    for (int i = 0; i < n / QK_K; i++, a++, b++) {
        const uint8_t *q4 = a->qs;
        const int8_t *q8 = b->qs;
        __m128i acc = _mm_setzero_si128();
        int summs = 0;
        for (int j = 0; j < 4; j++) {
            int sc1, m1, sc2, m2;
            escala_min_k4(2 * j, a->scales, &sc1, &m1);
            escala_min_k4(2 * j + 1, a->scales, &sc2, &m2);
            summs += m1 * (b->bsums[4 * j] + b->bsums[4 * j + 1]) + m2 * (b->bsums[4 * j + 2] + b->bsums[4 * j + 3]);
            const __m128i e1 = _mm_set1_epi16((short)sc1), e2 = _mm_set1_epi16((short)sc2);
            for (int k = 0; k < 2; k++) {
                __m128i q = _mm_loadu_si128((const __m128i *)(q4 + 16 * k));
                acc = _mm_add_epi32(acc, dot16(_mm_and_si128(q, m4), q8 + 16 * k, e1));
                acc = _mm_add_epi32(acc, dot16(_mm_and_si128(_mm_srli_epi16(q, 4), m4), q8 + 32 + 16 * k, e2));
            }
            q4 += 32; q8 += 64;
        }
        s += b->d * (llm_f16_f32(a->d) * (float)soma_i32(acc) - llm_f16_f32(a->dmin) * (float)summs);
    }
    return s;
}

float llm_dot_q6_K_ssse3(const void *w, const void *x, int n)
{
    const bloco_q6_K *a = w;
    const bloco_q8_K *b = x;
    const __m128i m4 = _mm_set1_epi8(0xF), m2 = _mm_set1_epi8(3);
    float s = 0;
    for (int i = 0; i < n / QK_K; i++, a++, b++) {
        const uint8_t *ql = a->ql, *qh = a->qh;
        const int8_t *sc = a->scales, *q8 = b->qs;
        __m128i acc = _mm_setzero_si128();
        int corr = 0;
        for (int g = 0; g < 16; g++) corr += sc[g] * b->bsums[g];
        for (int h = 0; h < 2; h++) {
            for (int k = 0; k < 2; k++) {
                __m128i l0 = _mm_loadu_si128((const __m128i *)(ql + 16 * k));
                __m128i l1 = _mm_loadu_si128((const __m128i *)(ql + 32 + 16 * k));
                __m128i hh = _mm_loadu_si128((const __m128i *)(qh + 16 * k));
                __m128i v1 = _mm_or_si128(_mm_and_si128(l0, m4), _mm_slli_epi16(_mm_and_si128(hh, m2), 4));
                __m128i v2 = _mm_or_si128(_mm_and_si128(l1, m4), _mm_slli_epi16(_mm_and_si128(_mm_srli_epi16(hh, 2), m2), 4));
                __m128i v3 = _mm_or_si128(_mm_and_si128(_mm_srli_epi16(l0, 4), m4), _mm_slli_epi16(_mm_and_si128(_mm_srli_epi16(hh, 4), m2), 4));
                __m128i v4 = _mm_or_si128(_mm_and_si128(_mm_srli_epi16(l1, 4), m4), _mm_slli_epi16(_mm_and_si128(_mm_srli_epi16(hh, 6), m2), 4));
                acc = _mm_add_epi32(acc, dot16(v1, q8 + 16 * k, _mm_set1_epi16(sc[k])));
                acc = _mm_add_epi32(acc, dot16(v2, q8 + 32 + 16 * k, _mm_set1_epi16(sc[k + 2])));
                acc = _mm_add_epi32(acc, dot16(v3, q8 + 64 + 16 * k, _mm_set1_epi16(sc[k + 4])));
                acc = _mm_add_epi32(acc, dot16(v4, q8 + 96 + 16 * k, _mm_set1_epi16(sc[k + 6])));
            }
            ql += 64; qh += 32; sc += 8; q8 += 128;
        }
        s += b->d * llm_f16_f32(a->d) * (float)(soma_i32(acc) - 32 * corr);
    }
    return s;
}
