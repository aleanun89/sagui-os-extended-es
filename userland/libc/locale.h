/* locale.h - locale: solo el "C" (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_LOCALE_H
#define NS_LOCALE_H

#include <stddef.h>
#define LC_ALL      0
#define LC_COLLATE  1
#define LC_CTYPE    2
#define LC_MONETARY 3
#define LC_NUMERIC  4
#define LC_TIME     5
#define LC_MESSAGES 6
struct lconv {
    char *decimal_point;
    char *thousands_sep;
    char *grouping;
};
/* Solo existe el locale "C" (carencia anotada). */
char *setlocale(int categoria, const char *locale);
struct lconv *localeconv(void);
#endif
