/* sys/socket.h - sockets BSD (solo los tipos: NetSurf sin libcurl no abre sockets) (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_SYS_SOCKET_H
#define NS_SYS_SOCKET_H

#include <sys/types.h>
typedef unsigned int socklen_t;
typedef unsigned short sa_family_t;
#define AF_UNSPEC 0
#define AF_INET   2
#define AF_INET6 10
#define PF_INET  AF_INET
#define PF_INET6 AF_INET6
#define SOCK_STREAM 1
#define SOCK_DGRAM  2
struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};
struct sockaddr_storage {
    sa_family_t ss_family;
    char __dados[126];
};
#endif
