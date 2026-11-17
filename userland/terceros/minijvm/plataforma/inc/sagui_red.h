/* sagui_red.h - lo que le falta a la libc común para los sockets de miniJVM (jni_io.c) (D376).
 * Se incluye con -include en todos los ficheros de miniJVM. Código propio; implementado en
 * minijvm_sustitutos.c sobre la API de red de la libsagui (criar_socket, conectar...). */
#ifndef SAGUI_RED_H
#define SAGUI_RED_H
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#endif
#ifndef SOL_SOCKET
#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define SO_SNDBUF 7
#define SO_RCVBUF 8
#define SO_KEEPALIVE 9
#define SO_LINGER 13
#define SO_RCVTIMEO 20
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#endif
struct linger { int l_onoff; int l_linger; };
int setsockopt(int fd, int nivel, int opcion, const void *v, socklen_t n);
int getsockopt(int fd, int nivel, int opcion, void *v, socklen_t *n);
int getpeername(int fd, struct sockaddr *a, socklen_t *n);
int getsockname(int fd, struct sockaddr *a, socklen_t *n);
int madvise(void *dir, size_t n, int consejo);
const char *gai_strerror(int codigo);
#endif
