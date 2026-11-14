/* inttypes.h - formatos de printf para los tipos de stdint.h (i386) (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_INTTYPES_H
#define NS_INTTYPES_H

#include <stdint.h>
#define PRId8  "d"
#define PRIi8  "i"
#define PRIu8  "u"
#define PRIx8  "x"
#define PRId16 "d"
#define PRIi16 "i"
#define PRIu16 "u"
#define PRIx16 "x"
#define PRId32 "d"
#define PRIi32 "i"
#define PRIu32 "u"
#define PRIx32 "x"
#define PRIX32 "X"
#define PRIo32 "o"
#define PRId64 "lld"
#define PRIi64 "lli"
#define PRIu64 "llu"
#define PRIx64 "llx"
#define PRIX64 "llX"
#define PRIdPTR "d"
#define PRIiPTR "i"
#define PRIuPTR "u"
#define PRIxPTR "x"
#define PRIdMAX "lld"
#define PRIuMAX "llu"
#define PRIxMAX "llx"
#define SCNd32 "d"
#define SCNu32 "u"
#define SCNx32 "x"
#define SCNd64 "lld"
#define SCNu64 "llu"
#define SCNx64 "llx"
#define SCNdPTR "d"
#define SCNuPTR "u"
typedef struct { long long quot, rem; } imaxdiv_t;
intmax_t  strtoimax(const char *s, char **fim, int base);
uintmax_t strtoumax(const char *s, char **fim, int base);
#endif
