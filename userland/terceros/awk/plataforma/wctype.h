/* SPDX-License-Identifier: GPL-3.0-only */
/* wctype.h mínimo para awk (D440): la libc de SaguiOS no tiene caracteres anchos y su MB_CUR_MAX es 1, así que
 * awk trabaja por bytes (Latin-1) y estas funciones no llegan a ejecutarse con texto real. */
#ifndef AWK_WCTYPE_H
#define AWK_WCTYPE_H
#include <stddef.h>
#include <ctype.h>
typedef unsigned int wint_t;
static inline wint_t towupper(wint_t c) { return c < 256 ? (wint_t)toupper((int)c) : c; }
static inline wint_t towlower(wint_t c) { return c < 256 ? (wint_t)tolower((int)c) : c; }
int mbtowc(wchar_t *pwc, const char *s, size_t n);
int wctomb(char *s, wchar_t wc);
#endif
