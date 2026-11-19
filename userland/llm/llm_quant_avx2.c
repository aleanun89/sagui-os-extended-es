// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_quant_avx2.c - productos punto con AVX2 (D381): lo mismo que llm_quant_ssse3.c con
 * registros de 256 bits. vpmaddubsw multiplica bytes sin signo (los pesos de 4 o 6 bits) por
 * bytes con signo (la entrada Q8) y suma por pares a 16 bits; vpmaddwd aplica la escala de
 * cada grupo de 16 sumando a 32 bits. Cada instrucción hace el trabajo de dos de la ruta SSSE3.
 *
 * Mismos resultados que las rutas SSSE3/SSE2, bit a bit: los enteros de cada bloque son los
 * mismos (los pares no saturan, ver llm_quant_ssse3.c) y la cuenta de float de cada bloque se
 * hace igual, con la x87 (-mfpmath=387), en el mismo orden. Por eso no se usa FMA: cambiaría
 * los redondeos de la suma de bloques y los tokens dejarían de coincidir con los del PC (h388).
 *
 * Solo se compila con -mavx2 y solo se llama si la CPU tiene AVX2 Y el kernel guarda los YMM
 * (cpu_tem_avx2: CR4.OSXSAVE y XCR0, D381); lo elige llm_quant.c. GCC pone vzeroupper al salir
 * de cada función, así el SSE clásico que venga después no paga la transición. */
#include "llm.h"
#define _MM_MALLOC_H_INCLUDED
#include <immintrin.h>

/* Suma de los 8 enteros de 32 bits. */
static inline int soma_i32(__m256i v)
{
    __m128i s = _mm_add_epi32(_mm256_castsi256_si128(v), _mm256_extracti128_si256(v, 1));
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(1, 0, 3, 2)));
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1)));
    return _mm_cvtsi128_si32(s);
}

/* 32 pesos sin signo · 32 bytes de entrada; "esc" lleva la escala de cada mitad de 16
 * (en la mitad baja del registro, la de los 16 primeros): 8 sumas de 32 bits. */
static inline __m256i dot32(__m256i u, const int8_t *q8, __m256i esc)
{
    return _mm256_madd_epi16(_mm256_maddubs_epi16(u, _mm256_loadu_si256((const __m256i *)q8)), esc);
}

/* pshufb que repite el byte 2p en los 8 primeros y el 2p+1 en los 8 siguientes (p = 0..7). */
static const uint8_t mascara_par[8][16] __attribute__((aligned(16))) = {
#define PAR(p) { 2*p, 2*p, 2*p, 2*p, 2*p, 2*p, 2*p, 2*p, 2*p+1, 2*p+1, 2*p+1, 2*p+1, 2*p+1, 2*p+1, 2*p+1, 2*p+1 }
    PAR(0), PAR(1), PAR(2), PAR(3), PAR(4), PAR(5), PAR(6), PAR(7),
#undef PAR
};

/* De 16 escalas de 8 bits con signo ("sc16", Q6_K), el vector de 16 escalas de 16 bits para
 * dot32 del par p: sc[2p] para los 16 primeros valores y sc[2p+1] para los 16 siguientes. */
static inline __m256i escalas_par(__m128i sc16, int p)
{
    return _mm256_cvtepi8_epi16(_mm_shuffle_epi8(sc16, _mm_load_si128((const __m128i *)mascara_par[p])));
}

float llm_dot_q8_0_avx2(const void *w, const void *x, int n)
{
    const bloco_q8_0 *a = w, *b = x;
    const __m256i um = _mm256_set1_epi16(1);
    float s = 0;
    for (int i = 0; i < n / QK8_0; i++, a++, b++) {
        __m256i va = _mm256_loadu_si256((const __m256i *)a->qs);
        __m256i vb = _mm256_loadu_si256((const __m256i *)b->qs);
        /* |a| · (b con el signo de a) = a · b, como en la ruta SSSE3 */
        __m256i p = _mm256_maddubs_epi16(_mm256_abs_epi8(va), _mm256_sign_epi8(vb, va));
        s += (float)soma_i32(_mm256_madd_epi16(p, um)) * (llm_f16_f32(a->d) * llm_f16_f32(b->d));
    }
    return s;
}

float llm_dot_q4_0_avx2(const void *w, const void *x, int n)
{
    const bloco_q4_0 *a = w;
    const bloco_q8_0 *b = x;
    const __m128i m4 = _mm_set1_epi8(0xF);
    const __m256i um = _mm256_set1_epi16(1), uns = _mm256_set1_epi8(1);
    float s = 0;
    for (int i = 0; i < n / QK8_0; i++, a++, b++) {
        /* los 16 bytes: nibble bajo = elementos 0-15, alto = 16-31 */
        __m128i q = _mm_loadu_si128((const __m128i *)a->qs);
        __m256i v = _mm256_inserti128_si256(_mm256_castsi128_si256(_mm_and_si128(q, m4)),
                                            _mm_and_si128(_mm_srli_epi16(q, 4), m4), 1);
        __m256i vb = _mm256_loadu_si256((const __m256i *)b->qs);
        __m256i p = _mm256_madd_epi16(_mm256_maddubs_epi16(v, vb), um);
        __m256i sb = _mm256_madd_epi16(_mm256_maddubs_epi16(uns, vb), um);   /* la suma de la entrada */
        int si = soma_i32(p) - 8 * soma_i32(sb);
        s += (float)si * (llm_f16_f32(a->d) * llm_f16_f32(b->d));
    }
    return s;
}

/* Las 8 escalas y los 8 mínimos de 6 bits de un superbloque Q4_K (12 bytes empaquetados) en los
 * 8 bytes bajos de "sc" y de "mn", como hace ggml. Mismo resultado que escala_min_k4 de la ruta en
 * C (j < 4: los 6 bits bajos de q[j] y q[j+4]; j >= 4: 4 bits de q[j+4] y los 2 altos de q[j-4] o
 * q[j]). Se pasan a los registros con vmovd/vpinsrd y no por la pila: en 32 bits, guardar dos
 * palabras de 4 bytes y leerlas como una de 8 hace fallar el reenvío de la escritura (medido:
 * la ruta AVX2 quedaba por debajo de la SSSE3). */
static inline void escalas_q4_K(const uint8_t *q, __m128i *sc, __m128i *mn)
{
    uint32_t u[4];
    __builtin_memcpy(u, q, 12);
    const uint32_t k1 = 0x3f3f3f3f, k2 = 0x0f0f0f0f, k3 = 0x03030303;
    uint32_t s0 = u[0] & k1;                                       /* escalas 0-3 */
    uint32_t s1 = (u[2] & k2) | (((u[0] >> 6) & k3) << 4);         /* escalas 4-7 */
    uint32_t m0 = u[1] & k1;                                       /* mínimos 0-3 */
    uint32_t m1 = ((u[2] >> 4) & k2) | (((u[1] >> 6) & k3) << 4);  /* mínimos 4-7 */
    *sc = _mm_insert_epi32(_mm_cvtsi32_si128((int)s0), (int)s1, 1);
    *mn = _mm_insert_epi32(_mm_cvtsi32_si128((int)m0), (int)m1, 1);
}

/* Suma de los 4 enteros de 32 bits. */
static inline int soma_i32_128(__m128i s)
{
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(1, 0, 3, 2)));
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1)));
    return _mm_cvtsi128_si32(s);
}

float llm_dot_q4_K_avx2(const void *w, const void *x, int n)
{
    const bloco_q4_K *a = w;
    const bloco_q8_K *b = x;
    const __m256i m4 = _mm256_set1_epi8(0xF);
    float s = 0;
    for (int i = 0; i < n / QK_K; i++, a++, b++) {
        const uint8_t *q4 = a->qs;
        const int8_t *q8 = b->qs;
        __m128i sc16, mn8;
        escalas_q4_K(a->scales, &sc16, &mn8);
        /* summs = sum de mn[k] · (bsums[2k] + bsums[2k+1]): las sumas de pares con phaddw (como
         * mucho 2 · 16 · 128, cabe en 16 bits) y los productos con pmaddwd. */
        __m128i bs = _mm_hadd_epi16(_mm_loadu_si128((const __m128i *)b->bsums),
                                    _mm_loadu_si128((const __m128i *)(b->bsums + 8)));
        __m128i mins = _mm_cvtepu8_epi16(mn8);
        int summs = soma_i32_128(_mm_madd_epi16(mins, bs));
        __m256i acc = _mm256_setzero_si256();
#pragma GCC unroll 4
        for (int j = 0; j < 4; j++) {
            /* 32 bytes de pesos: los nibbles bajos van con q8[0..31] (escala sc[2j]), los altos con
             * q8[32..63] (sc[2j+1]) */
            __m256i q = _mm256_loadu_si256((const __m256i *)q4);
            /* aquí los 32 pesos de cada mitad llevan una sola escala: sc[2j] repetida 16 veces */
            const __m256i e1 = _mm256_cvtepu8_epi16(_mm_shuffle_epi8(sc16, _mm_set1_epi8((char)(2 * j))));
            const __m256i e2 = _mm256_cvtepu8_epi16(_mm_shuffle_epi8(sc16, _mm_set1_epi8((char)(2 * j + 1))));
            acc = _mm256_add_epi32(acc, dot32(_mm256_and_si256(q, m4), q8, e1));
            acc = _mm256_add_epi32(acc, dot32(_mm256_and_si256(_mm256_srli_epi16(q, 4), m4), q8 + 32, e2));
            q4 += 32; q8 += 64;
        }
        s += b->d * (llm_f16_f32(a->d) * (float)soma_i32(acc) - llm_f16_f32(a->dmin) * (float)summs);
    }
    return s;
}

float llm_dot_q6_K_avx2(const void *w, const void *x, int n)
{
    const bloco_q6_K *a = w;
    const bloco_q8_K *b = x;
    const __m256i m4 = _mm256_set1_epi8(0xF), m2 = _mm256_set1_epi8(3);
    float s = 0;
    for (int i = 0; i < n / QK_K; i++, a++, b++) {
        const uint8_t *ql = a->ql, *qh = a->qh;
        const int8_t *sc = a->scales, *q8 = b->qs;
        __m256i acc = _mm256_setzero_si256();
        /* corr = sum de sc[g] · bsums[g] (16 productos de 16 bits con pmaddwd) */
        const __m128i sc16 = _mm_loadu_si128((const __m128i *)sc);   /* las 16 escalas */
        __m256i pc = _mm256_madd_epi16(_mm256_cvtepi8_epi16(sc16), _mm256_loadu_si256((const __m256i *)b->bsums));
        int corr = soma_i32(pc);
        for (int h = 0; h < 2; h++) {
            /* 128 valores: 4 grupos de 32 (v1..v4), cada uno con dos escalas de 16 */
            __m256i l0 = _mm256_loadu_si256((const __m256i *)ql);
            __m256i l1 = _mm256_loadu_si256((const __m256i *)(ql + 32));
            __m256i hh = _mm256_loadu_si256((const __m256i *)qh);
            __m256i v1 = _mm256_or_si256(_mm256_and_si256(l0, m4), _mm256_slli_epi16(_mm256_and_si256(hh, m2), 4));
            __m256i v2 = _mm256_or_si256(_mm256_and_si256(l1, m4),
                                         _mm256_slli_epi16(_mm256_and_si256(_mm256_srli_epi16(hh, 2), m2), 4));
            __m256i v3 = _mm256_or_si256(_mm256_and_si256(_mm256_srli_epi16(l0, 4), m4),
                                         _mm256_slli_epi16(_mm256_and_si256(_mm256_srli_epi16(hh, 4), m2), 4));
            __m256i v4 = _mm256_or_si256(_mm256_and_si256(_mm256_srli_epi16(l1, 4), m4),
                                         _mm256_slli_epi16(_mm256_and_si256(_mm256_srli_epi16(hh, 6), m2), 4));
            acc = _mm256_add_epi32(acc, dot32(v1, q8, escalas_par(sc16, 4 * h)));
            acc = _mm256_add_epi32(acc, dot32(v2, q8 + 32, escalas_par(sc16, 4 * h + 1)));
            acc = _mm256_add_epi32(acc, dot32(v3, q8 + 64, escalas_par(sc16, 4 * h + 2)));
            acc = _mm256_add_epi32(acc, dot32(v4, q8 + 96, escalas_par(sc16, 4 * h + 3)));
            ql += 64; qh += 32; q8 += 128;
        }
        s += b->d * llm_f16_f32(a->d) * (float)(soma_i32(acc) - 32 * corr);
    }
    return s;
}
