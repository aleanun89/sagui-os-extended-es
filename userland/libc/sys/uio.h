/* sys/uio.h - readv y writev (D477: las pide Valkey para enviar respuestas en varios trozos).
 *
 * Parte de la libc común (userland/libc, D210). Se resuelven con read y write en un bucle (posix.c):
 * en un socket o tubería, si un trozo se queda corto se devuelve lo escrito hasta ahí. */
#ifndef SAGUI_SYS_UIO_H
#define SAGUI_SYS_UIO_H
#include <sys/types.h>
struct iovec {
    void  *iov_base;
    size_t iov_len;
};
#define IOV_MAX 1024
ssize_t readv(int fd, const struct iovec *v, int n);
ssize_t writev(int fd, const struct iovec *v, int n);
#endif
