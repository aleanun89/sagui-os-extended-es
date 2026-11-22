/* SPDX-License-Identifier: GPL-3.0-only */
/* make_sagui.h - adaptación para compilar GNU make con la libc de SaguiOS (se incluye con -include).
 * SaguiOS no tiene bloqueos de ficheros con fcntl: --output-sync (-O) funciona sin ellos, porque la
 * salida de cada receta se junta en un fichero temporal y se vuelca de una vez (osync_acquire/release
 * no hacen nada). */
#ifndef MAKE_SAGUI_H
#define MAKE_SAGUI_H
#include <sys/types.h>
#include <fcntl.h>
struct flock { short l_type, l_whence; off_t l_start, l_len; pid_t l_pid; };
#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2
#define F_SETLKW 7
#define fcntl_bloqueo(fd, op, fl) (0)
/* GCC 14 y 15: declarar una función sin prototipo es un error (D464). compat.c las implementa. */
#include <stddef.h>
void *mempcpy(void *d, const void *s, size_t n);
char *getlogin(void);
#endif
