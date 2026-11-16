// SPDX-License-Identifier: GPL-3.0-only
/* compressor.h - compresión rápida de una página de 4 KB para la swap (D352).
 *
 * LZ77 sin entropía, al estilo de LZ4 (formato propio, no compatible): secuencias de
 * «token, literales, distancia, longitud». Pensado para la swap comprimida: rápido,
 * sin memoria dinámica (una tabla de 8 KB en la BSS) y con un descompresor que
 * comprueba cada límite (un trozo corrupto da error, nunca escribe fuera). */
#ifndef SAGUI_COMPRESSOR_H
#define SAGUI_COMPRESSOR_H
#include "tipos.h"

#define COMP_PAGINA 4096u

/* Comprime 4096 bytes en "saida". Devuelve los bytes escritos, o 0 si no caben en
 * "max" (la página no comprime lo bastante). No reentrante (tabla estática): el
 * kernel lo usa con el cerrojo general y las interrupciones apagadas. */
uint32_t comp_pagina(const uint8_t *entrada, uint8_t *saida, uint32_t max);

/* Descomprime "n" bytes en exactamente 4096. false si el trozo no es válido. */
bool     descomp_pagina(const uint8_t *entrada, uint32_t n, uint8_t *saida);

/* Suma de comprobación de 16 bits de una página (estilo Fletcher sobre palabras). */
uint16_t comp_soma(const uint8_t *pagina);

#endif
