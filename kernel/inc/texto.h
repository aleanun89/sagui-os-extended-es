// SPDX-License-Identifier: GPL-3.0-only
/* texto.h - funciones de memoria y de texto escritas para el kernel. */
#ifndef SAGUI_TEXTO_H
#define SAGUI_TEXTO_H
#include "tipos.h"

/* El GCC puede generar llamadas a memcpy/memset/memmove/memcmp por cuenta propia
 * (copia de estructuras, por ejemplo), así que esos cuatro nombres tienen que existir. */
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int v, size_t n);
int   memcmp(const void *a, const void *b, size_t n);

size_t txt_tamanho(const char *s);
int    txt_compara(const char *a, const char *b);
int    txt_compara_n(const char *a, const char *b, size_t n);
char  *txt_copia_n(char *dst, const char *src, size_t n);
bool   txt_comeca_com(const char *s, const char *prefixo);

#endif
