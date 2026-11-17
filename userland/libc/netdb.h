/* netdb.h - resolución de nombres (solo los tipos y las declaraciones) (D230).
 *
 * Parte de la libc común (userland/libc, D210). Como sys/socket.h (D184),
 * declara lo de BSD para que compile el código que lo incluye; los programas
 * del SaguiOS resuelven con resolver_nome() de la libsagui (conectar_tcp).
 * Las funciones no existen: quien las llame no enlaza (carencia anotada). */
#ifndef SAGUI_NETDB_H
#define SAGUI_NETDB_H

#include <sys/socket.h>
#include <netinet/in.h>

struct hostent {
    char  *h_name;
    char **h_aliases;
    int    h_addrtype;
    int    h_length;
    char **h_addr_list;
};
#define h_addr h_addr_list[0]

struct addrinfo {
    int    ai_flags;
    int    ai_family;
    int    ai_socktype;
    int    ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr;
    char  *ai_canonname;
    struct addrinfo *ai_next;
};
#define AI_PASSIVE     0x01
#define AI_CANONNAME   0x02
#define AI_NUMERICHOST 0x04
#define AI_NUMERICSERV 0x400
#define AI_ADDRCONFIG  0x20
#define NI_NUMERICHOST 1
#define NI_NUMERICSERV 2
#define NI_NAMEREQD    8
#define NI_MAXHOST 1025
#define NI_MAXSERV 32
#define EAI_SERVICE (-8)
#define EAI_FAIL       (-4)
#define EAI_NONAME     (-2)

struct hostent *gethostbyname(const char *nome);
struct hostent *gethostbyaddr(const void *dir, socklen_t n, int tipo);
int  getaddrinfo(const char *nome, const char *servico, const struct addrinfo *pistas, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);
const char *gai_strerror(int erro);
int  getnameinfo(const struct sockaddr *a, socklen_t an, char *host, socklen_t hl, char *serv, socklen_t sl, int flags);
#endif
