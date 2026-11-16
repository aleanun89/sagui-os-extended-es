/* utime.h - utime() (D341): fija las fechas de acceso y de modificación.
 *
 * Parte de la libc común (userland/libc, D210); la función está en posix.c. */
#ifndef NS_UTIME_H
#define NS_UTIME_H

#include <sys/types.h>
#include <time.h>
struct utimbuf {
    time_t actime;    /* acceso */
    time_t modtime;   /* modificación */
};
int utime(const char *caminho, const struct utimbuf *t);   /* NULL = ahora */
#endif
