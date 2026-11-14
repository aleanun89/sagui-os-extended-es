/* sys/times.h - tiempos del proceso (D210).
 *
 * Parte de la libc común (userland/libc). El SaguiOS no cuenta el tiempo de
 * CPU de cada proceso: times() da como tiempo de usuario el real desde que
 * empezó el programa, en centésimas (CLK_TCK). La función, en tempo.c. */
#ifndef NS_SYS_TIMES_H
#define NS_SYS_TIMES_H

#include <sys/types.h>
struct tms { clock_t tms_utime, tms_stime, tms_cutime, tms_cstime; };
#define CLK_TCK 100
clock_t times(struct tms *t);
#endif
