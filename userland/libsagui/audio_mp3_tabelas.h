// SPDX-License-Identifier: GPL-2.0-or-later
/* audio_mp3_tabelas.h - tablas del decodificador MP3 (generadas por tools/gerar_tabelas_mp3.py). */
#ifndef AUDIO_MP3_TABELAS_H
#define AUDIO_MP3_TABELAS_H
#include "audio_formato.h"

/* Una tabla de Huffman de la norma: xlen = lado de la matriz (0 = tabla vacía o de las cuádruplas),
 * linbits = bits extra del valor 15; el código del par (x, y) es cod[x * xlen + y] con
 * longitud com[x * xlen + y] (0 = sin código). */
typedef struct { uint8_t xlen, linbits; const uint16_t *cod; const uint8_t *com; } mp3_huf_t;

extern const mp3_huf_t mp3_huf[34];
extern const int32_t mp3_janela_d[512];       /* ventana de la síntesis, múltiplos de 2^-16 */
extern const uint32_t mp3_pot43_mant[8207];   /* x^(4/3) = mant * 2^exp, mant normalizada en 32 bits */
extern const int8_t mp3_pot43_exp[8207];
extern const uint32_t mp3_ganho4[4];          /* 2^(r/4), Q30 */
extern const int32_t mp3_imdct36[36 * 18];    /* Q30 */
extern const int32_t mp3_imdct12[12 * 6];
extern const int32_t mp3_janela_longa[3 * 36];   /* normal, inicio, fin; Q30 */
extern const int32_t mp3_janela_curta[12];
extern const int32_t mp3_alias_cs[8], mp3_alias_ca[8];
extern const int32_t mp3_is_esq[7], mp3_is_dir[7];
extern const int32_t mp3_is2_q[32];
extern const int32_t mp3_raiz_meio;
extern const int32_t mp3_sintese_n[64 * 32];

#endif
