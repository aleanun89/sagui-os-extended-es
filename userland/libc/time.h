/* time.h - hora y fecha (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_TIME_H
#define NS_TIME_H

#include <stddef.h>
#include <sys/types.h>
#define CLOCKS_PER_SEC 1000
#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1
typedef int clockid_t;
struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
    long tm_gmtoff;
    const char *tm_zone;
};
struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};
time_t     time(time_t *t);
clock_t    clock(void);
double     difftime(time_t a, time_t b);
struct tm *localtime(const time_t *t);
struct tm *localtime_r(const time_t *t, struct tm *r);
struct tm *gmtime(const time_t *t);
struct tm *gmtime_r(const time_t *t, struct tm *r);
time_t     mktime(struct tm *t);
time_t     timegm(struct tm *t);
char      *strptime(const char *s, const char *fmt, struct tm *t);
size_t     strftime(char *s, size_t max, const char *fmt, const struct tm *t);
char      *asctime(const struct tm *t);
char      *ctime(const time_t *t);
int        clock_gettime(clockid_t relogio, struct timespec *ts);
int        nanosleep(const struct timespec *pedido, struct timespec *resto);
void       tzset(void);
#endif
