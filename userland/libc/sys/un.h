/* sys/un.h - direcciones de sockets de dominio Unix (D477).
 *
 * Solo el tipo: SaguiOS no tiene sockets AF_UNIX (socket(AF_UNIX, ...) da EAFNOSUPPORT), pero los
 * servidores que los ofrecen como opción (Valkey: «unixsocket») necesitan compilar. */
#ifndef SAGUI_SYS_UN_H
#define SAGUI_SYS_UN_H
#include <sys/socket.h>
#define AF_UNIX  1
#define AF_LOCAL AF_UNIX
#define PF_UNIX  AF_UNIX
#define PF_LOCAL AF_UNIX
struct sockaddr_un {
    sa_family_t sun_family;
    char        sun_path[108];
};
#endif
