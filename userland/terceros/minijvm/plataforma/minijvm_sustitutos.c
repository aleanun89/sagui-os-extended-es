// SPDX-License-Identifier: GPL-3.0-only
/* minijvm_sustitutos.c - sockets para miniJVM sobre la libc de SaguiOS (D376). Sustituye a la parte
 * de mbedtls (net_sockets) y al cliente TLS que usa jni_io.c; TLS desde Java: no soportado aún. */
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sagui.h>
#include "sagui_red.h"
#include "utils/https/mbedtls/include/mbedtls/net_sockets.h"
#include "utils/https/ssl_client.h"

void mbedtls_net_init(mbedtls_net_context *ctx) { ctx->fd = -1; }

int mbedtls_net_connect(mbedtls_net_context *ctx, const char *host, const char *port, int proto)
{
    uint32_t ip;
    if (resolver(host, &ip) != 0) return -1;
    int fd = criar_socket(proto == MBEDTLS_NET_PROTO_UDP ? SOCK_UDP : SOCK_TCP);
    if (fd < 0) return -1;
    if (conectar(fd, ip, atoi(port)) < 0) { fechar(fd); return -1; }
    ctx->fd = fd;
    return 0;
}
int mbedtls_net_bind(mbedtls_net_context *ctx, const char *host, const char *port, int proto)
{
    (void)host;
    int fd = criar_socket(proto == MBEDTLS_NET_PROTO_UDP ? SOCK_UDP : SOCK_TCP);
    if (fd < 0) return -1;
    if (escutar(fd, atoi(port), 16) < 0) { fechar(fd); return -1; }
    ctx->fd = fd;
    return 0;
}
/* Sin sockets no bloqueantes en el kernel: se emulan mirando disponivel() antes de leer/aceptar. */
static unsigned char g_nobloq[1024];
int mbedtls_net_accept(mbedtls_net_context *b, mbedtls_net_context *c, void *ip, size_t n, size_t *ipl)
{
    endereco_rede_t de;
    (void)ip; (void)n;
    if (b->fd >= 0 && b->fd < 1024 && g_nobloq[b->fd]) {
        tempo_limite(b->fd, 1);
    }
    int fd = aceitar_conexao(b->fd, &de);
    if (fd < 0) return MBEDTLS_ERR_SSL_WANT_READ;
    c->fd = fd;
    if (ipl) *ipl = 0;
    return 0;
}
int mbedtls_net_set_block(mbedtls_net_context *ctx)
{
    if (ctx->fd >= 0 && ctx->fd < 1024) g_nobloq[ctx->fd] = 0;
    tempo_limite(ctx->fd, 0);
    return 0;
}
int mbedtls_net_set_nonblock(mbedtls_net_context *ctx)
{
    if (ctx->fd >= 0 && ctx->fd < 1024) g_nobloq[ctx->fd] = 1;
    return 0;
}
void mbedtls_net_usleep(unsigned long us) { usleep((unsigned)us); }
int mbedtls_net_recv(void *c, unsigned char *buf, size_t len)
{
    int fd = ((mbedtls_net_context *)c)->fd;
    if (fd >= 0 && fd < 1024 && g_nobloq[fd]) tempo_limite(fd, 1);
    int r = ler(fd, buf, (unsigned)len);
    if (r < 0 && fd >= 0 && fd < 1024 && g_nobloq[fd]) return MBEDTLS_ERR_SSL_WANT_READ;
    return r;
}
int mbedtls_net_send(void *c, const unsigned char *buf, size_t len)
{
    return escrever(((mbedtls_net_context *)c)->fd, buf, (unsigned)len);
}
int mbedtls_net_recv_timeout(void *c, unsigned char *buf, size_t len, unsigned ms)
{
    int fd = ((mbedtls_net_context *)c)->fd;
    tempo_limite(fd, ms);
    int r = ler(fd, buf, (unsigned)len);
    return r < 0 ? MBEDTLS_ERR_SSL_TIMEOUT : r;
}
void mbedtls_net_free(mbedtls_net_context *ctx)
{
    if (ctx->fd >= 0) fechar(ctx->fd);
    ctx->fd = -1;
}

/* TLS desde Java: no soportado. */
int sslc_init(SSLC_Entry *e) { e->fd = -1; return -1; }
int sslc_wrap(SSLC_Entry *e, int fd, const char *host) { (void)e; (void)fd; (void)host; return -1; }
int sslc_connect(SSLC_Entry *e, const char *host, const char *port) { (void)e; (void)host; (void)port; return -1; }
int sslc_close(SSLC_Entry *e) { (void)e; return 0; }
int sslc_read(SSLC_Entry *e, void *buf, int len) { (void)e; (void)buf; (void)len; return -1; }
int sslc_write(SSLC_Entry *e, const void *buf, int len) { (void)e; (void)buf; (void)len; return -1; }

/* Opciones de socket, nombres de socket y getaddrinfo: los da la libc comun (libc/sockets.c, D374). */

/* madvise: sin efecto (el GC de miniJVM solo lo usa para devolver páginas). */
int madvise(void *dir, size_t n, int consejo) { (void)dir; (void)n; (void)consejo; return 0; }
