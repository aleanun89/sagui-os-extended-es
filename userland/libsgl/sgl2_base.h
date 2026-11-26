// SPDX-License-Identifier: GPL-3.0-only
/* sgl2_base.h - lo que SaguiGL 2.0 (D475) toma del sistema: memoria y textos. Las mismas funciones en el
 * SaguiOS (libsagui) y en el anfitrión (libc, para las pruebas del PC con -DSGL_ANFITRIAO). */
#ifndef SAGUI_SGL2_BASE_H
#define SAGUI_SGL2_BASE_H

#ifdef SGL_ANFITRIAO
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static inline void *aloca(size_t n) { return malloc(n ? n : 1); }
static inline void *aloca_zerado(size_t n) { return calloc(1, n ? n : 1); }
static inline void libera(void *p) { free(p); }
static inline size_t tam_texto(const char *s) { return strlen(s); }
static inline int compara_texto(const char *a, const char *b) { return strcmp(a, b); }
static inline int compara_texto_n(const char *a, const char *b, size_t n) { return strncmp(a, b, n); }
static inline char *copia_texto(char *d, const char *s, size_t max)
{
    if (!max) return d;
    size_t i = 0;
    for (; i + 1 < max && s[i]; i++) d[i] = s[i];
    d[i] = 0;
    return d;
}
#define formatar_texto snprintf
#else
#include "sagui.h"
#endif

#endif
