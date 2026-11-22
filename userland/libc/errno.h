/* errno.h - errno y sus códigos (los de Linux) (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_ERRNO_H
#define NS_ERRNO_H

/* Un único errno para todo el proceso: NetSurf no usa hilos. Con hilos
 * haría falta uno por hilo (carencia anotada). */
extern int errno;
#define EPERM    1
#define ENOENT   2
#define ESRCH    3
#define EINTR    4
#define EIO      5
#define ENXIO    6
#define E2BIG    7
#define ENOEXEC  8
#define EBADF    9
#define ECHILD  10
#define EAGAIN  11
#define EWOULDBLOCK EAGAIN
#define ENOMEM  12
#define EACCES  13
#define EFAULT  14
#define EBUSY   16
#define EEXIST  17
#define EXDEV   18
#define ENODEV  19
#define ENOTDIR 20
#define EISDIR  21
#define EINVAL  22
#define ENFILE  23
#define EMFILE  24
#define ENOTTY  25
#define EFBIG   27
#define ENOSPC  28
#define ESPIPE  29
#define EROFS   30
#define EMLINK  31
#define EPIPE   32
#define EDOM    33
#define ERANGE  34
#define EDEADLK 35
#define ENAMETOOLONG 36
#define ENOLCK  37
#define ENOSYS  38
#define ENOTEMPTY 39
#define ELOOP   40
#define ENODATA 61
#define ETIME   62
#define EPROTO  71
#define EBADMSG 74
#define EOVERFLOW 75
#define EILSEQ  84
#define ENOTSUP 95
#define EOPNOTSUPP ENOTSUP
#define EAFNOSUPPORT 97
#define ECONNRESET 104
#define ETIMEDOUT 110
#define ECONNREFUSED 111
#define EHOSTUNREACH 113
#define EINPROGRESS 115
#define ENOTSOCK 88
#define EMSGSIZE 90
#define ENOPROTOOPT 92
#define EPROTONOSUPPORT 93
#define EADDRINUSE 98
#define EADDRNOTAVAIL 99
#define ENETUNREACH 101
#define ECONNABORTED 103
#define ENOBUFS 105
#define EISCONN 106
#define ENOTCONN 107
#define EALREADY 114
#define ECANCELED 125
/* Pasa un código de error del SaguiOS (ERRO_*, negativo) a uno de POSIX. */
int __ns_errno_de(int erro_sagui);
#endif
