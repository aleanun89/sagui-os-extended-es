/* sys/select.h - select() (D184; sobre poll desde la D401).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_SYS_SELECT_H
#define NS_SYS_SELECT_H

#include <sys/time.h>
#define FD_SETSIZE 128   /* D477: igual que MAX_ARQUIVOS del kernel */
typedef struct { unsigned int bits[FD_SETSIZE / 32]; } fd_set;
#define FD_ZERO(s)  do { int _i; for (_i = 0; _i < FD_SETSIZE / 32; _i++) (s)->bits[_i] = 0; } while (0)
#define FD_SET(fd, s)   ((s)->bits[(fd) / 32] |= (1u << ((fd) % 32)))
#define FD_CLR(fd, s)   ((s)->bits[(fd) / 32] &= ~(1u << ((fd) % 32)))
#define FD_ISSET(fd, s) (((s)->bits[(fd) / 32] >> ((fd) % 32)) & 1u)
/* D401: sobre poll() (sinais.c): ficheros, tuberías, terminales y sockets, con plazo. */
int select(int n, fd_set *l, fd_set *e, fd_set *x, struct timeval *prazo);
#endif
