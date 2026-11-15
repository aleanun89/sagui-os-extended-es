// SPDX-License-Identifier: GPL-2.0-or-later
/* crc32c.h - CRC32c (Castagnoli) y CRC16 (el de las sumas de grupo del ext4 sin
 * metadata_csum). Ver D326.
 *
 * Las dos funciones son "crudas", como las usan el ext4 y el JBD2: el valor inicial lo da
 * quien llama (normalmente ~0) y el resultado no se invierte al final. Así una suma se
 * puede continuar sobre varios trozos: crc32c(crc32c(~0, a, na), b, nb). */
#ifndef SAGUI_CRC32C_H
#define SAGUI_CRC32C_H
#include "tipos.h"

uint32_t crc32c(uint32_t crc, const void *p, uint32_t n);
uint16_t crc16_ext4(uint16_t crc, const void *p, uint32_t n);

#endif
