/* iconv.h - iconv mínimo (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_ICONV_H
#define NS_ICONV_H

#include <stddef.h>
typedef void *iconv_t;
/* Conversión entre UTF-8, ISO-8859-1 (Latin-1), Windows-1252, US-ASCII,
 * UTF-16 y UTF-32 (y sus variantes LE/BE); con //TRANSLIT o //IGNORE al
 * final del nombre de destino. Lo demás falla con EINVAL. */
iconv_t iconv_open(const char *para, const char *de);
size_t  iconv(iconv_t cd, char **ent, size_t *nent, char **sai, size_t *nsai);
int     iconv_close(iconv_t cd);
#endif
