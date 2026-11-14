/* limits.h - límites de los tipos (i386) y de las rutas (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_LIMITS_H
#define NS_LIMITS_H

#define CHAR_BIT   8
#define SCHAR_MIN  (-128)
#define SCHAR_MAX  127
#define UCHAR_MAX  255
#define CHAR_MIN   SCHAR_MIN
#define CHAR_MAX   SCHAR_MAX
#define SHRT_MIN   (-32768)
#define SHRT_MAX   32767
#define USHRT_MAX  65535
#define INT_MIN    (-2147483647 - 1)
#define INT_MAX    2147483647
#define UINT_MAX   4294967295U
#define LONG_MIN   (-2147483647L - 1)
#define LONG_MAX   2147483647L
#define ULONG_MAX  4294967295UL
#define LLONG_MAX  9223372036854775807LL
#define LLONG_MIN  (-LLONG_MAX - 1)
#define ULLONG_MAX 18446744073709551615ULL
#define SSIZE_MAX  INT_MAX
/* Rutas: el VFS del SaguiOS no fija un máximo global; los nombres de una
 * entrada de carpeta son de NOME_MAX (64) bytes. */
#define PATH_MAX   1024
#define NAME_MAX   63
#define IOV_MAX    16
#endif
