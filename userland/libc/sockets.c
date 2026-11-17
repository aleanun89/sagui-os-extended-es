/* sockets.c - sockets BSD, select y getaddrinfo de la libc común, sobre las llamadas de red
 * del SaguiOS (criar_socket, conectar, escutar, aceitar_conexao, disponivel, esperar_rede) (D374).
 *
 * Nació con el port de Freeciv (cliente y servidor en procesos distintos, por TCP en 127.0.0.1).
 * Solo IPv4, TCP y UDP. El SaguiOS no tiene sockets «no bloqueantes»: se emulan mirando
 * disponivel(fd) antes de leer o aceptar, y un plazo de un tic (tempo_limite) al escribir.
 * select() mira disponivel() de cada descriptor y, si ninguno está listo, duerme con
 * esperar_rede() hasta que la red se mueva o venza el plazo (sin consumir CPU). */
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include "sagui.h"

#define SOCK_MAX 256

static struct {
    unsigned char usado;
    unsigned char tipo;      /* SOCK_TCP / SOCK_UDP */
    unsigned char nb;        /* no bloqueante */
    unsigned char escucha;
    unsigned short porta;    /* la de bind() */
} tabla[SOCK_MAX];

static int falha_s(int r)
{
    errno = __ns_errno_de(r);
    if (r == ERRO_TEMPO) errno = ETIMEDOUT;
    return -1;
}

static int es_socket(int fd) { return fd >= 0 && fd < SOCK_MAX && tabla[fd].usado; }

int socket(int dominio, int tipo, int protocolo)
{
    (void)protocolo;
    if (dominio != AF_INET) { errno = EAFNOSUPPORT; return -1; }
    int t = tipo & 0xff;
    if (t != SOCK_STREAM && t != SOCK_DGRAM) { errno = EPROTONOSUPPORT; return -1; }
    int fd = criar_socket(t == SOCK_STREAM ? SOCK_TCP : SOCK_UDP);
    if (fd < 0) return falha_s(fd);
    if (fd < SOCK_MAX) {
        memset(&tabla[fd], 0, sizeof tabla[fd]);
        tabla[fd].usado = 1;
        tabla[fd].tipo = (unsigned char)(t == SOCK_STREAM ? SOCK_TCP : SOCK_UDP);
        tabla[fd].nb = (tipo & SOCK_NONBLOCK) ? 1 : 0;
    }
    return fd;
}

/* El resto de la libc (close) libera el descriptor; aquí solo se olvida su estado. */
void __sock_olvidar(int fd) { if (fd >= 0 && fd < SOCK_MAX) tabla[fd].usado = 0; }

int __sock_fcntl(int fd, int op, int arg)
{
    if (!es_socket(fd)) return -2;
    if (op == F_GETFL) return O_RDWR | (tabla[fd].nb ? O_NONBLOCK : 0);
    if (op == F_SETFL) { tabla[fd].nb = (arg & O_NONBLOCK) ? 1 : 0; return 0; }
    return 0;
}

int bind(int fd, const struct sockaddr *a, socklen_t n)
{
    if (!es_socket(fd)) { errno = ENOTSOCK; return -1; }
    if (!a || n < sizeof(struct sockaddr_in)) { errno = EINVAL; return -1; }
    const struct sockaddr_in *s = (const struct sockaddr_in *)a;
    tabla[fd].porta = ntohs(s->sin_port);
    if (tabla[fd].tipo == SOCK_UDP && tabla[fd].porta) {
        int r = escutar(fd, tabla[fd].porta, 0);
        if (r < 0) { if (r == ERRO_OCUPADO) { errno = EADDRINUSE; return -1; } return falha_s(r); }
    }
    return 0;
}

int listen(int fd, int fila)
{
    if (!es_socket(fd)) { errno = ENOTSOCK; return -1; }
    if (tabla[fd].tipo != SOCK_TCP) { errno = EOPNOTSUPP; return -1; }
    int r = escutar(fd, tabla[fd].porta, fila > 0 ? fila : 1);
    if (r < 0) { if (r == ERRO_OCUPADO) { errno = EADDRINUSE; return -1; } return falha_s(r); }
    tabla[fd].escucha = 1;
    return 0;
}

static void llenar_dir(struct sockaddr *a, socklen_t *n, unsigned ip, unsigned porta)
{
    struct sockaddr_in s;
    memset(&s, 0, sizeof s);
    s.sin_family = AF_INET;
    s.sin_port = htons((uint16_t)porta);
    s.sin_addr.s_addr = htonl(ip);
    if (a && n) {
        socklen_t c = *n < sizeof s ? *n : (socklen_t)sizeof s;
        memcpy(a, &s, c);
        *n = sizeof s;
    }
}

int accept(int fd, struct sockaddr *a, socklen_t *n)
{
    if (!es_socket(fd)) { errno = ENOTSOCK; return -1; }
    if (tabla[fd].nb) {
        int d = disponivel(fd);
        if (d == 0) { errno = EAGAIN; return -1; }
    }
    endereco_rede_t de;
    memset(&de, 0, sizeof de);
    int nfd = aceitar_conexao(fd, &de);
    if (nfd < 0) return falha_s(nfd);
    if (nfd < SOCK_MAX) {
        memset(&tabla[nfd], 0, sizeof tabla[nfd]);
        tabla[nfd].usado = 1;
        tabla[nfd].tipo = SOCK_TCP;
    }
    llenar_dir(a, n, de.ip, de.porta);
    return nfd;
}

int connect(int fd, const struct sockaddr *a, socklen_t n)
{
    if (!es_socket(fd)) { errno = ENOTSOCK; return -1; }
    if (!a || n < sizeof(struct sockaddr_in)) { errno = EINVAL; return -1; }
    const struct sockaddr_in *s = (const struct sockaddr_in *)a;
    int r = conectar(fd, ntohl(s->sin_addr.s_addr), ntohs(s->sin_port));
    if (r < 0) {
        if (r == ERRO_RECUSADA) errno = ECONNREFUSED;
        else if (r == ERRO_INALCANCAVEL || r == ERRO_SEM_REDE) errno = ENETUNREACH;
        else return falha_s(r);
        return -1;
    }
    return 0;
}

ssize_t send(int fd, const void *buf, size_t n, int flags)
{
    (void)flags;
    if (!es_socket(fd)) { errno = ENOTSOCK; return -1; }
    if (n == 0) return 0;
    if (tabla[fd].nb) tempo_limite(fd, 10);
    int r = escrever(fd, buf, (unsigned)n);
    if (tabla[fd].nb) tempo_limite(fd, 0);
    if (r == ERRO_TEMPO && tabla[fd].nb) { errno = EAGAIN; return -1; }
    if (r == ERRO_PIPE_QUEBRADO) { errno = EPIPE; return -1; }
    return r < 0 ? falha_s(r) : r;
}

ssize_t recv(int fd, void *buf, size_t n, int flags)
{
    (void)flags;
    if (!es_socket(fd)) { errno = ENOTSOCK; return -1; }
    if (tabla[fd].nb || (flags & MSG_DONTWAIT)) {
        int d = disponivel(fd);
        if (d == ERRO_PIPE_QUEBRADO) return 0;     /* el otro lado cerró: fin */
        if (d <= 0) { errno = EAGAIN; return -1; }
        if ((size_t)d < n) n = (size_t)d;
    }
    int r = ler(fd, buf, (unsigned)n);
    if (r == ERRO_PIPE_QUEBRADO) return 0;
    if (r == ERRO_CONEXAO_CAIU) { errno = ECONNRESET; return -1; }
    return r < 0 ? falha_s(r) : r;
}

ssize_t sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *a, socklen_t an)
{
    (void)flags;
    if (!es_socket(fd)) { errno = ENOTSOCK; return -1; }
    if (!a || an < sizeof(struct sockaddr_in)) return send(fd, buf, n, flags);
    const struct sockaddr_in *s = (const struct sockaddr_in *)a;
    int r = enviar_para(fd, buf, (unsigned)n, ntohl(s->sin_addr.s_addr), ntohs(s->sin_port));
    return r < 0 ? falha_s(r) : r;
}

ssize_t recvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr *a, socklen_t *an)
{
    (void)flags;
    if (!es_socket(fd)) { errno = ENOTSOCK; return -1; }
    if (tabla[fd].nb || (flags & MSG_DONTWAIT)) {
        int d = disponivel(fd);
        if (d <= 0 && d != ERRO_PIPE_QUEBRADO) { errno = EAGAIN; return -1; }
    }
    endereco_rede_t de;
    memset(&de, 0, sizeof de);
    int r = receber_de(fd, buf, (unsigned)n, &de);
    if (r < 0) return falha_s(r);
    llenar_dir(a, an, de.ip, de.porta);
    return r;
}

int getsockname(int fd, struct sockaddr *a, socklen_t *n)
{
    if (!es_socket(fd)) { errno = ENOTSOCK; return -1; }
    enderecos_socket_t e;
    memset(&e, 0, sizeof e);
    int r = opcao_socket(fd, OPC_ENDERECOS, (unsigned)&e);
    if (r < 0) { e.ip_local = 0; e.porta_local = tabla[fd].porta; }
    llenar_dir(a, n, e.ip_local, e.porta_local ? e.porta_local : tabla[fd].porta);
    return 0;
}

int getpeername(int fd, struct sockaddr *a, socklen_t *n)
{
    if (!es_socket(fd)) { errno = ENOTSOCK; return -1; }
    enderecos_socket_t e;
    memset(&e, 0, sizeof e);
    int r = opcao_socket(fd, OPC_ENDERECOS, (unsigned)&e);
    if (r < 0 || !e.porta_remota) { errno = ENOTCONN; return -1; }
    llenar_dir(a, n, e.ip_remoto, e.porta_remota);
    return 0;
}

int setsockopt(int fd, int nivel, int opcion, const void *v, socklen_t n)
{
    (void)nivel; (void)opcion; (void)v; (void)n;
    if (!es_socket(fd)) { errno = ENOTSOCK; return -1; }
    return 0;
}

int getsockopt(int fd, int nivel, int opcion, void *v, socklen_t *n)
{
    if (!es_socket(fd)) { errno = ENOTSOCK; return -1; }
    if (nivel == SOL_SOCKET && opcion == SO_ERROR && v && n && *n >= sizeof(int)) {
        *(int *)v = 0;
        *n = sizeof(int);
        return 0;
    }
    if (v && n && *n >= sizeof(int)) { *(int *)v = 0; *n = sizeof(int); }
    return 0;
}

int shutdown(int fd, int como)
{
    if (!es_socket(fd)) { errno = ENOTSOCK; return -1; }
    if (como == SHUT_WR || como == SHUT_RDWR) opcao_socket(fd, OPC_FECHAR_ENVIO, 0);
    return 0;
}

/* ---------------------------------------------------------------- select */

static int listo_lectura(int fd)
{
    int d = disponivel(fd);
    if (d > 0 || d == ERRO_PIPE_QUEBRADO) return 1;      /* datos, conexión pendiente o fin */
    return 0;
}

int select(int n, fd_set *l, fd_set *e, fd_set *x, struct timeval *prazo)
{
    unsigned ms = prazo ? (unsigned)(prazo->tv_sec * 1000 + (prazo->tv_usec + 999) / 1000) : 0xFFFFFFFFu;
    unsigned t0 = tempo_ms();
    unsigned visto = (unsigned)esperar_rede(0, 0);
    fd_set l0, e0;
    if (l) l0 = *l; else FD_ZERO(&l0);
    if (e) e0 = *e; else FD_ZERO(&e0);
    if (n > FD_SETSIZE) n = FD_SETSIZE;
    for (;;) {
        int listos = 0;
        fd_set lr, er;
        FD_ZERO(&lr);
        FD_ZERO(&er);
        for (int fd = 0; fd < n; fd++) {
            if (FD_ISSET(fd, &l0) && listo_lectura(fd)) { FD_SET(fd, &lr); listos++; }
            if (FD_ISSET(fd, &e0)) { FD_SET(fd, &er); listos++; }   /* siempre se puede escribir (o se bloquea) */
        }
        unsigned pasado = tempo_ms() - t0;
        if (listos || (prazo && pasado >= ms)) {
            if (l) *l = lr;
            if (e) *e = er;
            if (x) FD_ZERO(x);
            return listos;
        }
        unsigned resta = prazo ? ms - pasado : 100;
        if (resta > 100) resta = 100;                      /* despertar de vez en cuando: stdin y archivos no avisan */
        visto = (unsigned)esperar_rede(visto, resta ? resta : 1);
    }
}

/* ---------------------------------------------------------------- nombres */

struct hostent *gethostbyname(const char *nome)
{
    static struct hostent h;
    static char *lista[2];
    static unsigned char ip[4];
    static char *alias[1];
    uint32_t v;
    if (!nome || resolver(nome, &v) != 0) return NULL;
    ip[0] = (unsigned char)(v >> 24); ip[1] = (unsigned char)(v >> 16); ip[2] = (unsigned char)(v >> 8); ip[3] = (unsigned char)v;
    lista[0] = (char *)ip; lista[1] = NULL; alias[0] = NULL;
    h.h_name = (char *)nome; h.h_aliases = alias; h.h_addrtype = AF_INET; h.h_length = 4; h.h_addr_list = lista;
    return &h;
}

/* Sin DNS inverso en el SaguiOS: siempre «no encontrado». */
struct hostent *gethostbyaddr(const void *dir, socklen_t n, int tipo) { (void)dir; (void)n; (void)tipo; return NULL; }

char *inet_ntoa(struct in_addr a)
{
    static char b[16];
    ip_para_texto(ntohl(a.s_addr), b);
    return b;
}

const char *gai_strerror(int erro)
{
    switch (erro) {
    case EAI_NONAME: return "nombre o servicio desconocido";
    case EAI_SERVICE: return "servicio no admitido";
    default: return "error al resolver el nombre";
    }
}

int getaddrinfo(const char *nome, const char *servico, const struct addrinfo *pistas, struct addrinfo **res)
{
    uint32_t ip = 0;
    int puerto = 0;
    int tipo = pistas ? pistas->ai_socktype : 0;
    int flags = pistas ? pistas->ai_flags : 0;
    if (pistas && pistas->ai_family != AF_UNSPEC && pistas->ai_family != AF_INET) return EAI_NONAME;
    if (servico && *servico) {
        char *fin;
        puerto = (int)strtol(servico, &fin, 10);
        if (*fin) return EAI_SERVICE;
    }
    if (!nome) ip = (flags & AI_PASSIVE) ? 0 : 0x7f000001;
    else if (!strcmp(nome, "localhost")) ip = 0x7f000001;
    else if (!texto_para_ip(nome, &ip)) {
        if ((flags & AI_NUMERICHOST) || resolver(nome, &ip) != 0) return EAI_NONAME;
    }
    int tipos[2], nt = 0;
    if (tipo == 0 || tipo == SOCK_STREAM) tipos[nt++] = SOCK_STREAM;
    if (tipo == 0 || tipo == SOCK_DGRAM) tipos[nt++] = SOCK_DGRAM;
    struct addrinfo *primero = NULL, **ult = &primero;
    for (int i = 0; i < nt; i++) {
        struct addrinfo *ai = calloc(1, sizeof *ai + sizeof(struct sockaddr_in));
        if (!ai) { freeaddrinfo(primero); return EAI_FAIL; }
        struct sockaddr_in *s = (struct sockaddr_in *)(ai + 1);
        s->sin_family = AF_INET;
        s->sin_port = htons((uint16_t)puerto);
        s->sin_addr.s_addr = htonl(ip);
        ai->ai_family = AF_INET;
        ai->ai_socktype = tipos[i];
        ai->ai_protocol = tipos[i] == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP;
        ai->ai_addrlen = sizeof *s;
        ai->ai_addr = (struct sockaddr *)s;
        *ult = ai;
        ult = &ai->ai_next;
    }
    *res = primero;
    return 0;
}

void freeaddrinfo(struct addrinfo *r)
{
    while (r) { struct addrinfo *s = r->ai_next; free(r->ai_canonname); free(r); r = s; }
}

int getnameinfo(const struct sockaddr *a, socklen_t an, char *host, socklen_t hl, char *serv, socklen_t sl, int flags)
{
    (void)flags;
    if (!a || an < sizeof(struct sockaddr_in) || a->sa_family != AF_INET) return EAI_NONAME;
    const struct sockaddr_in *s = (const struct sockaddr_in *)a;
    if (host && hl) {
        char b[16];
        ip_para_texto(ntohl(s->sin_addr.s_addr), b);
        snprintf(host, hl, "%s", b);
    }
    if (serv && sl) snprintf(serv, sl, "%u", (unsigned)ntohs(s->sin_port));
    return 0;
}

/* fchmod: el SaguiOS no cambia permisos por descriptor (D374, Freeciv); se dice que va bien. */
int fchmod(int fd, mode_t modo) { (void)fd; (void)modo; return 0; }
