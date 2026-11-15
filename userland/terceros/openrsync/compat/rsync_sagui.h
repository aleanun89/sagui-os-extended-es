/* SPDX-License-Identifier: GPL-3.0-only
 * rsync_sagui.h - adaptación de openrsync a la libc de SaguiOS (D314).
 *
 * Se incluye a la fuerza (-include) en todos los .c de openrsync. Aquí van las
 * funciones de OpenBSD/Linux que la libc común no tiene (pledge, unveil, poll,
 * arc4random, las funciones "...at" con descriptor de carpeta, fechas...) y que
 * se implementan en compat_sagui.c. Es código propio de SaguiOS, no de openrsync.
 */
#ifndef RSYNC_SAGUI_H
#define RSYNC_SAGUI_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <endian.h>

/* Lo que OpenBSD usa para acotar el proceso: aquí no existe, no hace nada. */
static inline int pledge(const char *a, const char *b) { (void)a; (void)b; return 0; }
static inline int unveil(const char *a, const char *b) { (void)a; (void)b; return 0; }

/* El orden de bytes en la red de rsync es little endian, como el i386. */
#define htole32(x) ((uint32_t)(x))
#define le32toh(x) ((uint32_t)(x))
#define htole64(x) ((uint64_t)(x))
#define le64toh(x) ((uint64_t)(x))

#ifndef S_ISSOCK
#define S_ISSOCK(m) (((m) & S_IFMT) == 0140000)
#endif
#ifndef S_ISBLK
#define S_ISBLK(m)  (((m) & S_IFMT) == 0060000)
#endif

#define INFTIM (-1)
#ifndef __BEGIN_DECLS
#define __BEGIN_DECLS
#define __END_DECLS
#endif

uint32_t arc4random(void);
void    *reallocarray(void *p, size_t n, size_t t);
void    *recallocarray(void *p, size_t viejo, size_t nuevo, size_t t);
const char *getprogname(void);

/* Descriptor de carpeta "raíz": SaguiOS no abre carpetas como ficheros. El
 * receptor registra su carpeta de destino con rs_abrir_raiz() y las funciones
 * rs_*at() resuelven las rutas relativas a ella. */
#define RS_RAIZFD 900
int rs_abrir_raiz(const char *caminho);

int rs_copiar_modo(const char *de, const char *para);   /* el modo de "de" a "para" (D282) */
void rs_modos_exactos(int si);   /* -p: los modos de los ficheros nuevos son los exactos del origen */
int rs_openat(int pasta, const char *caminho, int flags, ...);
int rs_fstatat(int pasta, const char *caminho, struct stat *st, int flags);
int rs_unlinkat(int pasta, const char *caminho, int flags);
int rs_renameat(int p1, const char *de, int p2, const char *para);
int rs_mkdirat(int pasta, const char *caminho, mode_t modo);
int rs_symlinkat(const char *alvo, int pasta, const char *caminho);
ssize_t rs_readlinkat(int pasta, const char *caminho, char *buf, size_t n);
int rs_fchmodat(int pasta, const char *caminho, mode_t modo, int flags);
int rs_utimensat(int pasta, const char *caminho, const struct timespec ts[2], int flags);
int futimens(int fd, const struct timespec ts[2]);
int fchown(int fd, uid_t uid, gid_t gid);

#define openat rs_openat
#define fstatat rs_fstatat
#define unlinkat rs_unlinkat
#define renameat rs_renameat
#define mkdirat rs_mkdirat
#define symlinkat rs_symlinkat
#define readlinkat rs_readlinkat
#define fchmodat rs_fchmodat
#define utimensat rs_utimensat

/* D314 (cliente sobre ssh): el cliente habla con "ssh" por dos tuberías de 512 bytes del kernel.
 * Para no interbloquearse (los dos lados bloqueados escribiendo), la lectura y la escritura de
 * esas dos tuberías pasan por rs_read/rs_write: escribir solo lo que cabe, vaciando lo que llega
 * a un búfer propio. Con cualquier otro descriptor son read/write de siempre. */
ssize_t rs_read(int fd, void *buf, size_t n);
ssize_t rs_write(int fd, const void *buf, size_t n);
void rs_cliente_tuberias(int de_ssh, int a_ssh, int a_ssh_leitura);
#define read rs_read
#define write rs_write

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
#ifndef EPERM
#define EPERM 1
#endif

#endif
