/* netinet/in.h - direcciones IPv4 e IPv6 (tipos) (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_NETINET_IN_H
#define NS_NETINET_IN_H

#include <stdint.h>
#include <sys/socket.h>
typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;
struct in_addr { in_addr_t s_addr; };
struct in6_addr { unsigned char s6_addr[16]; };
struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t   sin_port;
    struct in_addr sin_addr;
    unsigned char sin_zero[8];
};
struct sockaddr_in6 {
    sa_family_t sin6_family;
    in_port_t   sin6_port;
    uint32_t    sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t    sin6_scope_id;
};
#define INADDR_ANY ((in_addr_t)0)
#define INADDR_NONE ((in_addr_t)0xffffffff)
#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46
uint32_t htonl(uint32_t v);
uint16_t htons(uint16_t v);
uint32_t ntohl(uint32_t v);
uint16_t ntohs(uint16_t v);
#endif
