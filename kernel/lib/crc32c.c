// SPDX-License-Identifier: GPL-2.0-or-later
/* crc32c.c - CRC32c (polinomio 0x1EDC6F41, en orden reflejado 0x82F63B78) y CRC16
 * (polinomio 0x8005, reflejado 0xA001). Tablas de 256 entradas calculadas la primera vez.
 * Ver crc32c.h y D326. */
#include "crc32c.h"

static uint32_t tabela32[256];
static uint16_t tabela16[256];
static bool pronta;

static void preparar(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        uint16_t d = (uint16_t)i;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? (c >> 1) ^ 0x82F63B78u : c >> 1;
            d = (uint16_t)((d & 1) ? (d >> 1) ^ 0xA001u : d >> 1);
        }
        tabela32[i] = c;
        tabela16[i] = d;
    }
    pronta = true;
}

uint32_t crc32c(uint32_t crc, const void *p, uint32_t n)
{
    if (!pronta) preparar();
    const uint8_t *b = p;
    while (n--) crc = tabela32[(crc ^ *b++) & 0xFF] ^ (crc >> 8);
    return crc;
}

uint16_t crc16_ext4(uint16_t crc, const void *p, uint32_t n)
{
    if (!pronta) preparar();
    const uint8_t *b = p;
    while (n--) crc = (uint16_t)(tabela16[(crc ^ *b++) & 0xFF] ^ (crc >> 8));
    return crc;
}
