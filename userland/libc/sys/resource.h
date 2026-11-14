/* sys/resource.h - uso de recursos (D210).
 *
 * Parte de la libc común (userland/libc). El SaguiOS no cuenta el tiempo de
 * CPU de cada proceso: getrusage da como tiempo de usuario el real desde que
 * empezó el programa (como el port de GNU Prolog, D189). En tempo.c. */
#ifndef NS_SYS_RESOURCE_H
#define NS_SYS_RESOURCE_H

#include <sys/time.h>
#define RUSAGE_SELF 0
#define RUSAGE_CHILDREN (-1)
struct rusage { struct timeval ru_utime, ru_stime; long ru_maxrss; };
int getrusage(int quem, struct rusage *r);
#endif
