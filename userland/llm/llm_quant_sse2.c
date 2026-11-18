// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_quant_sse2.c - productos punto de los bloques cuantizados con SSE2 (D380).
 *
 * Este fichero es el único del motor que se compila con -msse2: el GCC puede usar XMM en
 * cualquier función de aquí, y solo se llaman si CPUID dice que hay SSE2 (llm_quant.c los
 * elige). El kernel guarda los registros XMM con FXSAVE; AVX no se usa.
 *
 * Las cuentas son las mismas que en llm_quant.c y dan los mismos enteros: los valores de
 * 4 o 6 bits y los de la entrada (Q8) se pasan a 16 bits, se multiplican y suman por pares
 * (pmaddwd) y la escala de cada subbloque se aplica antes, sobre la entrada (cabe en 16
 * bits: 127 × 63 en Q4_K, 128 × 127 en Q6_K). */
#include "llm.h"
/* Como en la libsgl: sin _mm_malloc, la cabecera no pide la libc del anfitrión. */
#define _MM_MALLOC_H_INCLUDED
#include <emmintrin.h>

static inline int soma_i32(__m128i v)
{
    v = _mm_add_epi32(v, _mm_shuffle_epi32(v, _MM_SHUFFLE(1, 0, 3, 2)));
    v = _mm_add_epi32(v, _mm_shuffle_epi32(v, _MM_SHUFFLE(2, 3, 0, 1)));
    return _mm_cvtsi128_si32(v);
}

/* Bytes con signo -> dos vectores de 16 bits (extensión de signo). */
static inline void s8_s16(__m128i x, __m128i *lo, __m128i *hi)
{
    *lo = _mm_srai_epi16(_mm_unpacklo_epi8(x, x), 8);
    *hi = _mm_srai_epi16(_mm_unpackhi_epi8(x, x), 8);
}

/* Bytes sin signo (< 128) por la entrada ya multiplicada por la escala (16 bits): u (bytes) · e (2 × 8 de 16 bits). */
static inline __m128i dot_u8_e16(__m128i u, __m128i elo, __m128i ehi)
{
    const __m128i z = _mm_setzero_si128();
    return _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi8(u, z), elo), _mm_madd_epi16(_mm_unpackhi_epi8(u, z), ehi));
}

float llm_dot_q8_0_sse2(const void *w, const void *x, int n)
{
    const bloco_q8_0 *a = w, *b = x;
    float s = 0;
    for (int i = 0; i < n / QK8_0; i++, a++, b++) {
        __m128i acc = _mm_setzero_si128();
        for (int k = 0; k < 2; k++) {
            __m128i va = _mm_loadu_si128((const __m128i *)(a->qs + 16 * k));
            __m128i vb = _mm_loadu_si128((const __m128i *)(b->qs + 16 * k));
            __m128i alo, ahi, blo, bhi;
            s8_s16(va, &alo, &ahi); s8_s16(vb, &blo, &bhi);
            acc = _mm_add_epi32(acc, _mm_add_epi32(_mm_madd_epi16(alo, blo), _mm_madd_epi16(ahi, bhi)));
        }
        s += (float)soma_i32(acc) * (llm_f16_f32(a->d) * llm_f16_f32(b->d));
    }
    return s;
}

float llm_dot_q4_0_sse2(const void *w, const void *x, int n)
{
    const bloco_q4_0 *a = w;
    const bloco_q8_0 *b = x;
    const __m128i m4 = _mm_set1_epi8(0xF), oito = _mm_set1_epi8(8);
    float s = 0;
    for (int i = 0; i < n / QK8_0; i++, a++, b++) {
        __m128i q = _mm_loadu_si128((const __m128i *)a->qs);
        __m128i lo = _mm_sub_epi8(_mm_and_si128(q, m4), oito);                     /* -8..7 */
        __m128i hi = _mm_sub_epi8(_mm_and_si128(_mm_srli_epi16(q, 4), m4), oito);
        __m128i b0 = _mm_loadu_si128((const __m128i *)b->qs), b1 = _mm_loadu_si128((const __m128i *)(b->qs + 16));
        __m128i l0, l1, h0, h1, x0, x1, y0, y1;
        s8_s16(lo, &l0, &l1); s8_s16(hi, &h0, &h1); s8_s16(b0, &x0, &x1); s8_s16(b1, &y0, &y1);
        __m128i acc = _mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(l0, x0), _mm_madd_epi16(l1, x1)),
                                    _mm_add_epi32(_mm_madd_epi16(h0, y0), _mm_madd_epi16(h1, y1)));
        s += (float)soma_i32(acc) * (llm_f16_f32(a->d) * llm_f16_f32(b->d));
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

/* 16 bytes de entrada (Q8) multiplicados por una escala: dos vectores de 16 bits. */
static inline void entrada_escalada(const int8_t *q8, int esc, __m128i *lo, __m128i *hi)
{
    __m128i e = _mm_set1_epi16((short)esc);
    s8_s16(_mm_loadu_si128((const __m128i *)q8), lo, hi);
    *lo = _mm_mullo_epi16(*lo, e);
    *hi = _mm_mullo_epi16(*hi, e);
}

float llm_dot_q4_K_sse2(const void *w, const void *x, int n)
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
            for (int k = 0; k < 2; k++) {
                __m128i q = _mm_loadu_si128((const __m128i *)(q4 + 16 * k));
                __m128i lo = _mm_and_si128(q, m4), hi = _mm_and_si128(_mm_srli_epi16(q, 4), m4);
                __m128i e0, e1;
                entrada_escalada(q8 + 16 * k, sc1, &e0, &e1);
                acc = _mm_add_epi32(acc, dot_u8_e16(lo, e0, e1));
                entrada_escalada(q8 + 32 + 16 * k, sc2, &e0, &e1);
                acc = _mm_add_epi32(acc, dot_u8_e16(hi, e0, e1));
            }
            q4 += 32; q8 += 64;
        }
        s += b->d * (llm_f16_f32(a->d) * (float)soma_i32(acc) - llm_f16_f32(a->dmin) * (float)summs);
    }
    return s;
}

float llm_dot_q6_K_sse2(const void *w, const void *x, int n)
{
    const bloco_q6_K *a = w;
    const bloco_q8_K *b = x;
    const __m128i m4 = _mm_set1_epi8(0xF), m2 = _mm_set1_epi8(3);
    float s = 0;
    for (int i = 0; i < n / QK_K; i++, a++, b++) {
        const uint8_t *ql = a->ql, *qh = a->qh;
        const int8_t *sc = a->scales, *q8 = b->qs;
        __m128i acc = _mm_setzero_si128();
        int corr = 0;   /* los valores van de 0 a 63: se resta 32 × la suma de la entrada */
        for (int g = 0; g < 16; g++) corr += sc[g] * b->bsums[g];
        for (int h = 0; h < 2; h++) {
            for (int k = 0; k < 2; k++) {   /* l = 16k .. 16k+15 */
                __m128i l0 = _mm_loadu_si128((const __m128i *)(ql + 16 * k));
                __m128i l1 = _mm_loadu_si128((const __m128i *)(ql + 32 + 16 * k));
                __m128i hh = _mm_loadu_si128((const __m128i *)(qh + 16 * k));
                __m128i v1 = _mm_or_si128(_mm_and_si128(l0, m4), _mm_slli_epi16(_mm_and_si128(hh, m2), 4));
                __m128i v2 = _mm_or_si128(_mm_and_si128(l1, m4), _mm_slli_epi16(_mm_and_si128(_mm_srli_epi16(hh, 2), m2), 4));
                __m128i v3 = _mm_or_si128(_mm_and_si128(_mm_srli_epi16(l0, 4), m4), _mm_slli_epi16(_mm_and_si128(_mm_srli_epi16(hh, 4), m2), 4));
                __m128i v4 = _mm_or_si128(_mm_and_si128(_mm_srli_epi16(l1, 4), m4), _mm_slli_epi16(_mm_and_si128(_mm_srli_epi16(hh, 6), m2), 4));
                __m128i e0, e1;
                entrada_escalada(q8 + 16 * k, sc[k], &e0, &e1);
                acc = _mm_add_epi32(acc, dot_u8_e16(v1, e0, e1));
                entrada_escalada(q8 + 32 + 16 * k, sc[k + 2], &e0, &e1);
                acc = _mm_add_epi32(acc, dot_u8_e16(v2, e0, e1));
                entrada_escalada(q8 + 64 + 16 * k, sc[k + 4], &e0, &e1);
                acc = _mm_add_epi32(acc, dot_u8_e16(v3, e0, e1));
                entrada_escalada(q8 + 96 + 16 * k, sc[k + 6], &e0, &e1);
                acc = _mm_add_epi32(acc, dot_u8_e16(v4, e0, e1));
            }
            ql += 64; qh += 32; sc += 8; q8 += 128;
        }
        s += b->d * llm_f16_f32(a->d) * (float)(soma_i32(acc) - 32 * corr);
    }
    return s;
}

