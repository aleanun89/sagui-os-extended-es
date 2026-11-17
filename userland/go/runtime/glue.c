/* SPDX-License-Identifier: GPL-3.0-only */
/* glue.c - capa C entre el runtime de TinyGo y SaguiOS (D364).
 *
 * El código que genera TinyGo (con el objetivo userland/go/alvos/saguios.json)
 * se enlaza con la libsagui y la libc común de SaguiOS y con este fichero. Aquí
 * están:
 *   - el punto de entrada (principal -> main de TinyGo);
 *   - lo que el runtime de TinyGo para «Linux con libc» espera de ella y la libc
 *     de SaguiOS no tiene (señales, futex, getrandom, reloj de 64 bits);
 *   - la reserva del montón del recolector (SaguiOS no tiene paginación a
 *     demanda dentro de lo reservado: se reserva y se asignan las páginas a
 *     medida que el recolector crece);
 *   - las funciones sg_* que usa el paquete syscall de SaguiOS
 *     (userland/go/syscall/), con tipos de tamaño fijo (la libc usa off_t de
 *     32 bits y Go necesita 64) y las carpetas como descriptores.
 * Todas las sg_* devuelven -1 y dejan el código en errno, como POSIX. */
#include "sagui.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef EOVERFLOW
#define EOVERFLOW 75   /* el de Linux, que es el que conoce el paquete syscall de Go */
#endif

/* ------------------------------------------------------------- arranque */

extern int main(int argc, char **argv);   /* el de TinyGo (runtime_unix.go) */

int principal(int argc, char **argv)
{
    return main(argc, argv);
}

/* ------------------------------------------------------------- señales (no hay) */

int tinygo_caught_signal;
void tinygo_register_fatal_signals(void) {}
void tinygo_signal_enable(uint32_t s) { (void)s; }

/* ------------------------------------------------------------- futex y reloj */

/* El planificador de tareas de TinyGo no usa hilos del sistema: la única
 * espera es dormir (sleepTicks). Sin señales que la interrumpan, dormir el
 * tiempo pedido es correcto. */
void tinygo_futex_wait_timeout(uint32_t *addr, uint32_t cmp, uint64_t ns)
{
    if (*addr != cmp) return;
    uint64_t ms = (ns + 999999u) / 1000000u;
    if (ms > 3600000u) ms = 3600000u;
    dormir((unsigned)ms);
}

void tinygo_futex_wait(uint32_t *addr, uint32_t cmp)
{
    while (*addr == cmp) hilo_ceder();
}

void tinygo_futex_wake(uint32_t *addr) { (void)addr; }
void tinygo_futex_wake_all(uint32_t *addr) { (void)addr; }

struct timespec64 { int64_t sec; int64_t nsec; };

/* clock_gettime con tiempo de 64 bits (lo que pide runtime_unix.go en 32 bits). */
int __clock_gettime64(int clk, struct timespec64 *ts)
{
    struct timespec t;
    if (clock_gettime(clk == 4 ? CLOCK_MONOTONIC : clk, &t) < 0) return -1;
    ts->sec = t.tv_sec;
    ts->nsec = t.tv_nsec;
    return 0;
}

int getrandom(void *buf, size_t n, unsigned flags)
{
    (void)flags;
    size_t hecho = 0;
    while (hecho < n) {
        size_t t = n - hecho > 4096 ? 4096 : n - hecho;
        int r = bytes_aleatorios((char *)buf + hecho, (unsigned)t);
        if (r <= 0) { errno = EIO; return -1; }
        hecho += (size_t)r;
    }
    return (int)n;
}

/* ------------------------------------------------------------- montón del GC */

/* Reserva direcciones para el montón sin asignar páginas. */
void *sg_heap_reservar(uint32_t bytes)
{
    bytes = (bytes + 4095u) & ~4095u;
    int r = memoria_paginas(MEM_RESERVAR, 0, bytes);
    if (r < 0 && r > -100) return MAP_FAILED;
    return (void *)r;
}

/* Asigna (a cero) las páginas de un trozo ya reservado; 0 = bien. */
int sg_heap_asignar(void *dir, uint32_t bytes)
{
    uintptr_t ini = (uintptr_t)dir & ~(uintptr_t)4095;
    uintptr_t fin = ((uintptr_t)dir + bytes + 4095u) & ~(uintptr_t)4095;
    return memoria_paginas(MEM_ASIGNAR, (void *)ini, (unsigned)(fin - ini)) < 0 ? -1 : 0;
}

/* mmap para quien lo pide desde Go (syscall.Mmap): anónimo o de fichero. */
void *sg_mmap(uint32_t bytes, int fd, uint32_t desloc)
{
    void *p = mmap(NULL, bytes, PROT_READ | PROT_WRITE, fd < 0 ? (MAP_PRIVATE | MAP_ANONYMOUS) : MAP_PRIVATE,
                   fd, (off_t)desloc);
    return p == MAP_FAILED ? NULL : p;
}

int sg_munmap(void *p, uint32_t bytes) { return munmap(p, bytes); }

/* ------------------------------------------------------------- carpetas como descriptores */

/* SaguiOS no abre carpetas como ficheros: os.Open("/") necesita un número que
 * ReadDirent y Close entiendan. Se dan desde 3000. */
#define DIR_BASE 3000
#define DIR_MAX  32
static struct { int usado; int indice; char caminho[PATH_MAX]; } pastas[DIR_MAX];

static int es_pasta(const char *caminho)
{
    info_arquivo_t inf;
    return info_arquivo(caminho, &inf) == 0 && inf.tipo == TIPO_PASTA;
}

static int pasta_de(int fd)
{
    int i = fd - DIR_BASE;
    return (i >= 0 && i < DIR_MAX && pastas[i].usado) ? i : -1;
}

int sg_open(const char *caminho, int flags, unsigned modo)
{
    (void)modo;
    if ((flags & O_ACCMODE) == O_RDONLY && es_pasta(caminho)) {
        for (int i = 0; i < DIR_MAX; i++)
            if (!pastas[i].usado) {
                pastas[i].usado = 1;
                pastas[i].indice = 0;
                strlcpy(pastas[i].caminho, caminho, sizeof pastas[i].caminho);
                return DIR_BASE + i;
            }
        errno = EMFILE;
        return -1;
    }
    return open(caminho, flags);
}

int sg_close(int fd)
{
    int i = pasta_de(fd);
    if (i >= 0) { pastas[i].usado = 0; return 0; }
    return close(fd);
}

/* ---- UTF-8 <-> Latin-1 solo en terminales ----
 * Go trabaja en UTF-8 y el terminal de SaguiOS es ISO-8859-1. En un descriptor de
 * terminal (TIPO_TERMINAL) la salida se pasa de UTF-8 a Latin-1 (lo que no cabe, «?») y la
 * entrada de Latin-1 a UTF-8. Ficheros y tuberías pasan byte a byte. Las secuencias UTF-8
 * partidas entre dos write se guardan hasta el siguiente. */
#define TERM_FDS 8
static struct { uint8_t seq[4]; int tiene, necesita; } sal_term[TERM_FDS];   /* UTF-8 incompleto */
static struct { uint8_t resto[2]; int nresto; } ent_term[TERM_FDS];            /* UTF-8 por entregar */

static int es_terminal(int fd)
{
    return fd >= 0 && fd < TERM_FDS && tipo_descritor(fd) == TIPO_TERMINAL;
}

/* UTF-8 -> Latin-1 con una pequeña máquina de estados por descriptor (así una secuencia partida
 * entre dos write se completa con el siguiente). */
static int escribir_terminal(int fd, const uint8_t *buf, uint32_t n)
{
    uint8_t salida[256];
    uint32_t ns = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint8_t b = buf[i], c = 0;
        int emitir = 0;
        if (sal_term[fd].tiene) {
            if ((b & 0xC0) == 0x80) {
                sal_term[fd].seq[sal_term[fd].tiene++] = b;
                if (sal_term[fd].tiene < sal_term[fd].necesita) continue;
                if (sal_term[fd].necesita == 2) {
                    uint32_t cp = ((sal_term[fd].seq[0] & 0x1Fu) << 6) | (sal_term[fd].seq[1] & 0x3Fu);
                    c = cp < 0x100 ? (uint8_t)cp : '?';
                } else c = '?';
                sal_term[fd].tiene = 0;
                emitir = 1;
            } else {                       /* secuencia rota: un «?» y este byte se trata de nuevo */
                salida[ns++] = '?';
                sal_term[fd].tiene = 0;
            }
        }
        if (!emitir) {
            if (b < 0x80) c = b;
            else if (b >= 0xC0 && b < 0xF8) {
                sal_term[fd].seq[0] = b;
                sal_term[fd].tiene = 1;
                sal_term[fd].necesita = b < 0xE0 ? 2 : b < 0xF0 ? 3 : 4;
                continue;
            } else c = '?';
        }
        salida[ns++] = c;
        if (ns >= sizeof salida - 2) {
            if (write(fd, salida, ns) < 0) return -1;
            ns = 0;
        }
    }
    if (ns && write(fd, salida, ns) < 0) return -1;
    return (int)n;
}

int sg_read(int fd, void *buf, uint32_t n)
{
    if (pasta_de(fd) >= 0) { errno = EISDIR; return -1; }
    if (!es_terminal(fd) || n == 0) return (int)read(fd, buf, n);
    uint8_t *d = buf;
    uint32_t hecho = 0;
    while (ent_term[fd].nresto && hecho < n) {   /* lo que quedó del carácter anterior */
        d[hecho++] = ent_term[fd].resto[0];
        ent_term[fd].resto[0] = ent_term[fd].resto[1];
        ent_term[fd].nresto--;
    }
    if (hecho) return (int)hecho;
    uint8_t tmp[512];
    uint32_t pide = n / 2 ? n / 2 : 1;
    if (pide > sizeof tmp) pide = sizeof tmp;
    int r = (int)read(fd, tmp, pide);
    if (r <= 0) return r;
    for (int i = 0; i < r; i++) {
        if (tmp[i] < 0x80) {
            if (hecho < n) d[hecho++] = tmp[i]; else ent_term[fd].resto[ent_term[fd].nresto++] = tmp[i];
        } else {
            uint8_t a = (uint8_t)(0xC0 | (tmp[i] >> 6)), b = (uint8_t)(0x80 | (tmp[i] & 0x3F));
            if (hecho + 2 <= n) { d[hecho++] = a; d[hecho++] = b; }
            else { d[hecho++] = a; ent_term[fd].resto[ent_term[fd].nresto++] = b; }
        }
    }
    return (int)hecho;
}

int sg_write(int fd, const void *buf, uint32_t n)
{
    if (pasta_de(fd) >= 0) { errno = EISDIR; return -1; }
    if (es_terminal(fd)) return escribir_terminal(fd, buf, n);
    return (int)write(fd, buf, n);
}

int64_t sg_lseek(int fd, int64_t desloc, int origem)
{
    if (pasta_de(fd) >= 0) {
        if (origem == 0 && desloc == 0) { pastas[pasta_de(fd)].indice = 0; return 0; }
        errno = ESPIPE;
        return -1;
    }
    if (desloc > 0x7FFFFFFFLL || desloc < -0x7FFFFFFFLL) { errno = EOVERFLOW; return -1; }
    off_t r = lseek(fd, (off_t)desloc, origem);
    return r;
}

int sg_pread(int fd, void *buf, uint32_t n, int64_t desloc)
{
    if (desloc > 0x7FFFFFFFLL) { errno = EOVERFLOW; return -1; }
    off_t antes = lseek(fd, 0, SEEK_CUR);
    if (antes < 0) return -1;
    if (lseek(fd, (off_t)desloc, SEEK_SET) < 0) return -1;
    int r = (int)read(fd, buf, n);
    lseek(fd, antes, SEEK_SET);
    return r;
}

int sg_pwrite(int fd, const void *buf, uint32_t n, int64_t desloc)
{
    if (desloc > 0x7FFFFFFFLL) { errno = EOVERFLOW; return -1; }
    off_t antes = lseek(fd, 0, SEEK_CUR);
    if (antes < 0) return -1;
    if (lseek(fd, (off_t)desloc, SEEK_SET) < 0) return -1;
    int r = (int)write(fd, buf, n);
    lseek(fd, antes, SEEK_SET);
    return r;
}

int sg_fsync(int fd)
{
    if (pasta_de(fd) >= 0) return 0;
    return fsync(fd);
}

int sg_ftruncate(int fd, int64_t n)
{
    if (n > 0x7FFFFFFFLL) { errno = EFBIG; return -1; }
    return ftruncate(fd, (off_t)n);
}

int sg_truncate(const char *caminho, int64_t n)
{
    int fd = open(caminho, O_WRONLY);
    if (fd < 0) return -1;
    int r = sg_ftruncate(fd, n);
    close(fd);
    return r;
}

/* ------------------------------------------------------------- stat */

/* El mismo diseño que syscall.Stat_t (userland/go/syscall/sagui_tipos.go). */
typedef struct {
    uint64_t dev, ino;
    uint32_t mode, nlink, uid, gid;
    uint64_t rdev;
    int64_t  size;
    int32_t  blksize;
    int64_t  blocks;
    int64_t  at_s, at_ns, mt_s, mt_ns, ct_s, ct_ns;
} stat_go_t;

_Static_assert(sizeof(stat_go_t) == 108, "stat_go_t debe medir lo mismo que syscall.Stat_t");

static void copiar_stat(stat_go_t *d, const struct stat *s)
{
    memset(d, 0, sizeof *d);
    d->dev = s->st_dev;
    d->ino = s->st_ino;
    d->mode = s->st_mode;
    d->nlink = s->st_nlink;
    d->uid = s->st_uid;
    d->gid = s->st_gid;
    d->rdev = s->st_rdev;
    d->size = s->st_size;
    d->blksize = (int32_t)s->st_blksize;
    d->blocks = s->st_blocks;
    d->at_s = s->st_atime;
    d->mt_s = s->st_mtime;
    d->ct_s = s->st_ctime;
}

int sg_stat(const char *caminho, stat_go_t *d)
{
    struct stat s;
    if (stat(caminho, &s) < 0) return -1;
    copiar_stat(d, &s);
    return 0;
}

int sg_fstat(int fd, stat_go_t *d)
{
    int i = pasta_de(fd);
    if (i >= 0) return sg_stat(pastas[i].caminho, d);
    struct stat s;
    if (fstat(fd, &s) < 0) return -1;
    copiar_stat(d, &s);
    return 0;
}

/* ------------------------------------------------------------- carpetas: getdents */

/* Escribe registros linux_dirent64 (los que lee os.ReadDir): ino u64, off i64,
 * reclen u16, tipo u8, nombre con cero, alineado a 8. */
int sg_getdents(int fd, uint8_t *buf, uint32_t n)
{
    int i = pasta_de(fd);
    if (i < 0) { errno = ENOTDIR; return -1; }
    uint32_t usado = 0;
    for (;;) {
        entrada_pasta_t e;
        if (listar(pastas[i].caminho, pastas[i].indice, &e) != 1) break;
        uint32_t nlen = (uint32_t)strlen(e.nome);
        uint32_t reclen = (19 + nlen + 1 + 7) & ~7u;
        if (usado + reclen > n) {
            if (usado == 0) { errno = EINVAL; return -1; }
            break;
        }
        uint8_t *r = buf + usado;
        memset(r, 0, reclen);
        uint64_t ino = (uint64_t)pastas[i].indice + 1;
        int64_t off = (int64_t)pastas[i].indice + 1;
        memcpy(r, &ino, 8);
        memcpy(r + 8, &off, 8);
        r[16] = (uint8_t)(reclen & 255);
        r[17] = (uint8_t)(reclen >> 8);
        r[18] = e.tipo == TIPO_PASTA ? 4 : 8;   /* DT_DIR o DT_REG */
        memcpy(r + 19, e.nome, nlen);
        usado += reclen;
        pastas[i].indice++;
    }
    return (int)usado;
}

/* ------------------------------------------------------------- nombres */

int sg_mkdir(const char *c, unsigned modo) { return mkdir(c, modo); }
int sg_rmdir(const char *c) { return rmdir(c); }
int sg_unlink(const char *c) { return unlink(c); }
int sg_rename(const char *a, const char *b) { return rename(a, b); }
int sg_chdir(const char *c) { return chdir(c); }
int sg_getcwd(char *buf, uint32_t n) { return getcwd(buf, n) ? 0 : -1; }
int sg_getpid(void) { return getpid(); }

/* ------------------------------------------------------------- libc que pide runtime/os_linux.go */

int *__errno_location(void) { return &errno; }

