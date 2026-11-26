/* netinet/tcp.h - opciones de socket TCP (D477). setsockopt las acepta y las ignora (sockets.c). */
#ifndef SAGUI_NETINET_TCP_H
#define SAGUI_NETINET_TCP_H
#define TCP_NODELAY   1
#define TCP_MAXSEG    2
#define TCP_KEEPIDLE  4
#define TCP_KEEPINTVL 5
#define TCP_KEEPCNT   6
#endif
