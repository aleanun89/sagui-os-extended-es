/* sys/time.h - gettimeofday y struct timeval (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_SYS_TIME_H
#define NS_SYS_TIME_H

#include <sys/types.h>
#include <time.h>
struct timeval {
    time_t      tv_sec;
    suseconds_t tv_usec;
};
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
int gettimeofday(struct timeval *tv, void *tz);
int utimes(const char *caminho, const struct timeval tv[2]);   /* D341: [0] acceso, [1] modificación */
int futimes(int fd, const struct timeval tv[2]);

#define timerclear(t) ((t)->tv_sec = (t)->tv_usec = 0)
#define timerisset(t) ((t)->tv_sec || (t)->tv_usec)
#define timercmp(a, b, CMP) (((a)->tv_sec == (b)->tv_sec) ? \
    ((a)->tv_usec CMP (b)->tv_usec) : ((a)->tv_sec CMP (b)->tv_sec))
#define timeradd(a, b, r) do { (r)->tv_sec = (a)->tv_sec + (b)->tv_sec; \
    (r)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
    if ((r)->tv_usec >= 1000000) { (r)->tv_sec++; (r)->tv_usec -= 1000000; } } while (0)
#define timersub(a, b, r) do { (r)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
    (r)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
    if ((r)->tv_usec < 0) { (r)->tv_sec--; (r)->tv_usec += 1000000; } } while (0)
/* D477: setitimer solo para ITIMER_REAL, sobre alarm (segundos enteros hacia arriba; servidores.c). */
#define ITIMER_REAL 0
struct itimerval { struct timeval it_interval, it_value; };
int setitimer(int cual, const struct itimerval *nuevo, struct itimerval *antiguo);
/* Como la glibc: fd_set y select() vienen también con sys/time.h. */
#include <sys/select.h>
#endif
