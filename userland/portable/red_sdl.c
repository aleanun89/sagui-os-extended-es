// SPDX-License-Identifier: GPL-2.0-or-later
/* red_sdl.c - los sockets de SaguiOS sobre los del PC (D459, VisualPythonS).
 *
 * El modulo socket del MicroPython de SaguiOS (rede_sagui.c) habla con la libsagui: criar_socket, conectar,
 * escutar, aceitar_conexao, enviar_para, receber_de, opcao_socket, tempo_limite y resolver. Aqui son los sockets
 * del sistema (IPv4, TCP y UDP); los descriptores son los reales, de modo que ler/escrever/fechar de
 * sistema_sdl.c valen en ellos. Los errores se traducen a los ERRO_* de SaguiOS (ERRO_TEMPO, ERRO_RECUSADA...).
 * Solo lo enlaza el python portable (las aplicaciones de ventanas no usan la red). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "sagui.h"

#define NFD 4096
static unsigned limite_ms[NFD];   /* 0 = sin limite */

static int erro_de(int e)
{
    switch (e) {
    case ETIMEDOUT: case EAGAIN: return ERRO_TEMPO;
    case ECONNREFUSED: return ERRO_RECUSADA;
    case ECONNRESET: case ECONNABORTED: return ERRO_CONEXAO_CAIU;
    case ENETUNREACH: case EHOSTUNREACH: case ENETDOWN: return ERRO_SEM_REDE;
    case EADDRINUSE: return ERRO_OCUPADO;
    case EPIPE: return ERRO_PIPE_QUEBRADO;
    case EBADF: case ENOTSOCK: return ERRO_DESCRITOR;
    case EACCES: case EPERM: return ERRO_ACESSO;
    default: return ERRO_INVALIDO;
    }
}

static void deixar_sigpipe(void)
{
    static int hecho;
    if (!hecho) { hecho = 1; signal(SIGPIPE, SIG_IGN); }
}

int criar_socket(int tipo)
{
    deixar_sigpipe();
    int fd = socket(AF_INET, tipo == SOCK_UDP ? SOCK_DGRAM : SOCK_STREAM, 0);
    if (fd < 0) return erro_de(errno);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (fd < NFD) limite_ms[fd] = 0;
    if (tipo != SOCK_UDP) { int v = 1; setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &v, sizeof v); }
    return fd;
}

static void poner_limite(int fd, unsigned ms)
{
    struct timeval tv = { ms / 1000, (ms % 1000) * 1000 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
}

int tempo_limite(int fd, unsigned ms)
{
    if (fd < 0) return ERRO_DESCRITOR;
    if (fd < NFD) limite_ms[fd] = ms;
    poner_limite(fd, ms);
    return 0;
}

int conectar(int fd, uint32_t ip, int porta)
{
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(ip);
    a.sin_port = htons((unsigned short)porta);
    unsigned ms = fd >= 0 && fd < NFD ? limite_ms[fd] : 0;
    if (!ms) {
        int r;
        do r = connect(fd, (struct sockaddr *)&a, sizeof a); while (r < 0 && errno == EINTR);
        return r < 0 ? erro_de(errno) : 0;
    }
    int fl = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    int r = connect(fd, (struct sockaddr *)&a, sizeof a);
    if (r < 0 && errno == EINPROGRESS) {
        struct pollfd p = { fd, POLLOUT, 0 };
        int n = poll(&p, 1, (int)ms);
        if (n == 0) { fcntl(fd, F_SETFL, fl); return ERRO_TEMPO; }
        int e = 0;
        socklen_t l = sizeof e;
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &e, &l);
        fcntl(fd, F_SETFL, fl);
        return e ? erro_de(e) : 0;
    }
    fcntl(fd, F_SETFL, fl);
    return r < 0 ? erro_de(errno) : 0;
}

int escutar(int fd, int porta, int fila)
{
    int v = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof v);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons((unsigned short)porta);
    if (bind(fd, (struct sockaddr *)&a, sizeof a) < 0) return erro_de(errno);
    int tipo = 0;
    socklen_t l = sizeof tipo;
    getsockopt(fd, SOL_SOCKET, SO_TYPE, &tipo, &l);
    if (tipo == SOCK_STREAM && listen(fd, fila > 0 ? fila : 5) < 0) return erro_de(errno);
    return 0;
}

int aceitar_conexao(int fd, endereco_rede_t *de)
{
    struct sockaddr_in a;
    socklen_t l = sizeof a;
    int n;
    do n = accept(fd, (struct sockaddr *)&a, &l); while (n < 0 && errno == EINTR);
    if (n < 0) return erro_de(errno);
    fcntl(n, F_SETFD, FD_CLOEXEC);
    if (n < NFD) limite_ms[n] = 0;
    if (de) { de->ip = ntohl(a.sin_addr.s_addr); de->porta = ntohs(a.sin_port); }
    return n;
}

int enviar_para(int fd, const void *buf, unsigned n, uint32_t ip, int porta)
{
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(ip);
    a.sin_port = htons((unsigned short)porta);
    ssize_t r = sendto(fd, buf, n, MSG_NOSIGNAL, (struct sockaddr *)&a, sizeof a);
    return r < 0 ? erro_de(errno) : (int)r;
}

int receber_de(int fd, void *buf, unsigned n, endereco_rede_t *de)
{
    struct sockaddr_in a;
    socklen_t l = sizeof a;
    ssize_t r = recvfrom(fd, buf, n, 0, (struct sockaddr *)&a, &l);
    if (r < 0) return erro_de(errno);
    if (de) { de->ip = ntohl(a.sin_addr.s_addr); de->porta = ntohs(a.sin_port); }
    return (int)r;
}

int opcao_socket(int fd, int opcao, unsigned valor)
{
    switch (opcao) {
    case OPC_TEMPO_LIMITE: return tempo_limite(fd, valor);
    case OPC_FECHAR_ENVIO: return shutdown(fd, SHUT_WR) == 0 ? 0 : erro_de(errno);
    case OPC_ERRO_CONEXAO: {
        int e = 0;
        socklen_t l = sizeof e;
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &e, &l);
        return e ? erro_de(e) : 0;
    }
    default: return ERRO_NAO_SUPORTADO;
    }
}

/* OPC_ENDERECOS: en 64 bits el puntero no cabe en el `unsigned` de opcao_socket; rede_sagui.c llama a esta. */
int portavel_enderecos_socket(int fd, enderecos_socket_t *e)
{
    struct sockaddr_in a;
    socklen_t l = sizeof a;
    memset(e, 0, sizeof *e);
    if (getsockname(fd, (struct sockaddr *)&a, &l) == 0) { e->ip_local = ntohl(a.sin_addr.s_addr); e->porta_local = ntohs(a.sin_port); }
    l = sizeof a;
    if (getpeername(fd, (struct sockaddr *)&a, &l) == 0) { e->ip_remoto = ntohl(a.sin_addr.s_addr); e->porta_remota = ntohs(a.sin_port); }
    return 0;
}

int resolver(const char *nome, uint32_t *ip)
{
    struct in_addr ia;
    if (inet_pton(AF_INET, nome, &ia) == 1) { *ip = ntohl(ia.s_addr); return 0; }
    struct addrinfo h, *r = NULL;
    memset(&h, 0, sizeof h);
    h.ai_family = AF_INET;
    h.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(nome, NULL, &h, &r) != 0 || !r) return ERRO_NAO_EXISTE;
    *ip = ntohl(((struct sockaddr_in *)r->ai_addr)->sin_addr.s_addr);
    freeaddrinfo(r);
    return 0;
}

int config_rede(int op, config_rede_t *c) { (void)op; (void)c; return ERRO_NAO_SUPORTADO; }
