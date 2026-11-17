/* net_sockets.h - sustituto (D376) de la parte de red de mbedtls que usa miniJVM (jni_io.c): sockets
 * TCP/UDP sobre la libc de SaguiOS. Implementado en minijvm_sustitutos.c. Código propio. */
#ifndef SAGUI_NET_SOCKETS_H
#define SAGUI_NET_SOCKETS_H
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

#define MBEDTLS_NET_PROTO_TCP 0
#define MBEDTLS_NET_PROTO_UDP 1
#define MBEDTLS_ERR_SSL_WANT_READ -0x6900
#define MBEDTLS_ERR_SSL_WANT_WRITE -0x6880
#define MBEDTLS_ERR_SSL_TIMEOUT -0x6800

typedef struct { int fd; } mbedtls_net_context;
void mbedtls_net_init(mbedtls_net_context *ctx);
int mbedtls_net_connect(mbedtls_net_context *ctx, const char *host, const char *port, int proto);
int mbedtls_net_bind(mbedtls_net_context *ctx, const char *host, const char *port, int proto);
int mbedtls_net_accept(mbedtls_net_context *bind_ctx, mbedtls_net_context *client_ctx, void *client_ip, size_t buf_size, size_t *ip_len);
int mbedtls_net_set_block(mbedtls_net_context *ctx);
int mbedtls_net_set_nonblock(mbedtls_net_context *ctx);
void mbedtls_net_usleep(unsigned long usec);
int mbedtls_net_recv(void *ctx, unsigned char *buf, size_t len);
int mbedtls_net_send(void *ctx, const unsigned char *buf, size_t len);
int mbedtls_net_recv_timeout(void *ctx, unsigned char *buf, size_t len, unsigned timeout);
void mbedtls_net_free(mbedtls_net_context *ctx);
#endif
