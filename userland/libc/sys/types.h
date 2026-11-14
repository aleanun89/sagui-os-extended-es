/* sys/types.h - tipos de POSIX (i386, time_t de 32 bits) (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_SYS_TYPES_H
#define NS_SYS_TYPES_H

#include <stddef.h>
#include <stdint.h>
typedef int           ssize_t;
typedef long          off_t;
typedef long          time_t;
typedef long          clock_t;
typedef long          suseconds_t;
typedef unsigned int  mode_t;
typedef int           pid_t;
typedef unsigned int  uid_t;
typedef unsigned int  gid_t;
typedef unsigned int  ino_t;
typedef unsigned int  dev_t;
typedef unsigned int  nlink_t;
typedef long          blksize_t;
typedef long          blkcnt_t;
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int  u_int;
typedef unsigned long u_long;
#endif
