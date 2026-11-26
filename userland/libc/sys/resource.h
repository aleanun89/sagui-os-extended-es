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
/* D477: getrlimit/setrlimit (servidores.c). El único límite real es el de descriptores abiertos por proceso
 * (128, MAX_ARQUIVOS del kernel); setrlimit solo acepta lo que ya hay. */
typedef unsigned long rlim_t;
struct rlimit { rlim_t rlim_cur, rlim_max; };
#define RLIM_INFINITY (~0UL)
#define RLIMIT_CPU    0
#define RLIMIT_FSIZE  1
#define RLIMIT_DATA   2
#define RLIMIT_CORE   4
#define RLIMIT_RSS    5
#define RLIMIT_NPROC  6
#define RLIMIT_MEMLOCK 8
#define RLIMIT_STACK  3
#define RLIMIT_NOFILE 7
#define RLIMIT_AS     9
int getrlimit(int recurso, struct rlimit *l);
int setrlimit(int recurso, const struct rlimit *l);
#endif
