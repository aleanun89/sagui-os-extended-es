// SPDX-License-Identifier: GPL-2.0-or-later
/* inflar.h - descompresión DEFLATE y CRC-32 en el núcleo (v8.5.4.2, D379). Ver kernel/lib/inflar.c. */
#ifndef SAGUI_INFLAR_H
#define SAGUI_INFLAR_H
#include "tipos.h"

#define INFLAR_FORMATO  -100   /* datos corruptos */
#define INFLAR_CHEIO    -101   /* no cabe en la salida */
#define INFLAR_TRUNCADO -103   /* el flujo se acaba antes de tiempo */

/* Las tablas de Huffman (unos 3,3 KB): una por lector que pueda estar descomprimiendo a la vez. */
typedef struct { uint16_t hc[16 + 288 + 512], hd[16 + 288 + 512]; } inflar_ctx_t;

/* Descomprime un flujo DEFLATE crudo; devuelve los bytes escritos o un INFLAR_*. */
int inflar_bloco(inflar_ctx_t *ctx, const uint8_t *ent, int n_ent, uint8_t *saida, int max_saida);
uint32_t inflar_crc32(uint32_t crc, const void *dados, uint32_t n);

#endif
