/* sys/ioctl.h - ioctl (solo la declaración) (D230).
 *
 * Parte de la libc común (userland/libc, D210). El SaguiOS no tiene ioctl:
 * se declara para que compile el código de sockets que lo incluye; quien la
 * llame no enlaza (carencia anotada). */
#ifndef SAGUI_SYS_IOCTL_H
#define SAGUI_SYS_IOCTL_H
#define FIONBIO  0x5421
#define FIONREAD 0x541B
int ioctl(int fd, unsigned long pedido, ...);
#endif
