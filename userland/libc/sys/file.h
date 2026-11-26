/* sys/file.h - flock (D477). SaguiOS no tiene bloqueos de ficheros entre procesos: flock siempre
 * tiene éxito (posix.c), como un sistema de un solo usuario; sirve para que los servidores que
 * protegen su fichero de datos arranquen. */
#ifndef SAGUI_SYS_FILE_H
#define SAGUI_SYS_FILE_H
#include <fcntl.h>
#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8
int flock(int fd, int op);
#endif
