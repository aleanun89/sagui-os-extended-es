/* sys/mman.h - mmap emulado (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_SYS_MMAN_H
#define NS_SYS_MMAN_H

#include <sys/types.h>
#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4
#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20
#define MAP_ANON      MAP_ANONYMOUS
#define MAP_FAILED ((void *)-1)
/* D209: los anónimos son páginas propias que munmap devuelve al sistema; los
 * de fichero, una copia privada leída al mapear (MAP_SHARED con escritura no). */
void *mmap(void *dir, size_t n, int prot, int flags, int fd, off_t desloc);
int   munmap(void *dir, size_t n);
#endif
