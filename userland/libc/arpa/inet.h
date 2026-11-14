/* arpa/inet.h - inet_aton, inet_pton y compañía (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_ARPA_INET_H
#define NS_ARPA_INET_H

#include <netinet/in.h>
int inet_aton(const char *s, struct in_addr *a);
int inet_pton(int familia, const char *s, void *dst);
const char *inet_ntop(int familia, const void *src, char *dst, socklen_t n);
in_addr_t inet_addr(const char *s);
#endif
