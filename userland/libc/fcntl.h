/* fcntl.h - open() y sus banderas (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_FCNTL_H
#define NS_FCNTL_H

#include <sys/types.h>
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_ACCMODE   0x0003
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_NOCTTY    0x0100
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
#define O_NONBLOCK  0x0800
#define O_CLOEXEC   0x80000
#define O_BINARY    0
#define F_GETFL 3
#define F_SETFL 4
#define F_GETFD 1
#define F_SETFD 2
#define FD_CLOEXEC 1
#define AT_FDCWD (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR 0x200
int open(const char *caminho, int flags, ...);
int openat(int pasta, const char *caminho, int flags, ...);
int creat(const char *caminho, mode_t modo);
int fcntl(int fd, int op, ...);
#endif
