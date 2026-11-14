// SPDX-License-Identifier: GPL-2.0-or-later
/* base.h - lo mínimo que el núcleo de lite-monkey necesita del C.
 *
 * El núcleo (nucleo/) no incluye nada del sistema operativo: en el PC usa la
 * libc del compilador; en SaguiOS (LMK_SAGUI), la libsagui, que no es una
 * libc y tiene otros nombres. Todo lo demás (memoria, archivos, red, reloj)
 * lo da la capa de plataforma (plataforma.h). */
#ifndef LMK_BASE_H
#define LMK_BASE_H

#ifdef LMK_SAGUI
#include "sagui.h"
#define strlen(s)         tam_texto(s)
#define strcmp(a, b)      compara_texto(a, b)
#define strncmp(a, b, n)  compara_texto_n(a, b, n)
#define lmk_fmt           formatar_texto
#define INT64_MAX         0x7FFFFFFFFFFFFFFFLL
#define INT64_MIN         (-INT64_MAX - 1)
#define INT32_MAX         0x7FFFFFFF
#define INT32_MIN         (-INT32_MAX - 1)
#define UINT32_MAX        0xFFFFFFFFu
static inline void *lmk_memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    for (size_t i = 0; i < n; i++)
        if (p[i] == (unsigned char)c) return (void *)(p + i);
    return NULL;
}
#define memchr lmk_memchr
#else
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#define lmk_fmt           snprintf
#endif

/* Memoria: la de la plataforma (en SaguiOS, un asignador por clases de
 * tamaño, porque el de la libsagui busca bloque a bloque). */
void *lmk_aloca(size_t n);
void *lmk_realoca(void *p, size_t n);
void  lmk_libera(void *p);

#define LMK_MIN(a, b) ((a) < (b) ? (a) : (b))
#define LMK_MAX(a, b) ((a) > (b) ? (a) : (b))

static inline uint32_t lmk_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static inline void lmk_pon32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static inline uint64_t lmk_le64(const uint8_t *p) { return (uint64_t)lmk_le32(p) | (uint64_t)lmk_le32(p + 4) << 32; }
static inline void lmk_pon64(uint8_t *p, uint64_t v) { lmk_pon32(p, (uint32_t)v); lmk_pon32(p + 4, (uint32_t)(v >> 32)); }
static inline double lmk_ledouble(const uint8_t *p) { uint64_t u = lmk_le64(p); double d; memcpy(&d, &u, 8); return d; }

#endif
