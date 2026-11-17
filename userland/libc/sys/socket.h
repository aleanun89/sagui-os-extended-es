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
#define SOCK_NONBLOCK 04000
#define SOCK_CLOEXEC  02000000
#define SOL_SOCKET  1
#define SO_REUSEADDR 2
#define SO_ERROR     4
#define SO_BROADCAST 6
#define SO_SNDBUF    7
#define SO_RCVBUF    8
#define SO_KEEPALIVE 9
#define SO_LINGER    13
#define SO_RCVTIMEO  20
#define SO_SNDTIMEO  21
#define SOMAXCONN    128
#define MSG_PEEK     2
#define MSG_DONTWAIT 0x40
#define MSG_NOSIGNAL 0x4000
#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2
struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};
struct sockaddr_storage {
    sa_family_t ss_family;
    char __dados[126];
};
/* D374: BSD sockets sobre las llamadas de red del SaguiOS (libc/sockets.c): solo IPv4, TCP y UDP;
 * «no bloqueante» se emula con disponivel(). */
int socket(int dominio, int tipo, int protocolo);
int bind(int fd, const struct sockaddr *a, socklen_t n);
int listen(int fd, int fila);
int accept(int fd, struct sockaddr *a, socklen_t *n);
int connect(int fd, const struct sockaddr *a, socklen_t n);
ssize_t send(int fd, const void *buf, size_t n, int flags);
ssize_t recv(int fd, void *buf, size_t n, int flags);
ssize_t sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *a, socklen_t an);
ssize_t recvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr *a, socklen_t *an);
int getsockname(int fd, struct sockaddr *a, socklen_t *n);
int getpeername(int fd, struct sockaddr *a, socklen_t *n);
int setsockopt(int fd, int nivel, int opcion, const void *v, socklen_t n);
int getsockopt(int fd, int nivel, int opcion, void *v, socklen_t *n);
int shutdown(int fd, int como);
#endif
