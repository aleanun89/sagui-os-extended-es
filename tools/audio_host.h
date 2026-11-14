// SPDX-License-Identifier: GPL-2.0-or-later
/* audio_host.h - pegamento para compilar los decodificadores de audio de la libsagui
 * (userland/libsagui/audio_*.c) en el anfitrión, con -DAUDIO_HOST: da los tipos
 * y las pocas llamadas de archivo que ofrece la libsagui (abrir, leer,
 * posicionar, cerrar). Solo sirve para pruebas y depuración rápida. */
#ifndef AUDIO_HOST_H
#define AUDIO_HOST_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

static inline void *aloca(size_t n) { return malloc(n); }
static inline void *aloca_zerado(size_t n) { return calloc(1, n); }
static inline void libera(void *p) { free(p); }

#define ABRIR_LER 1
#define POS_INICIO 0
#define POS_ATUAL 1
#define POS_FIM 2
#define ERRO_NAO_EXISTE (-2)
#define ERRO_SEM_MEMORIA (-4)

static inline int abrir(const char *c, int modo) { (void)modo; int fd = open(c, O_RDONLY); return fd < 0 ? ERRO_NAO_EXISTE : fd; }
static inline int ler(int fd, void *b, unsigned n) { return (int)read(fd, b, n); }
static inline int fechar(int fd) { return close(fd); }
static inline int posicionar(int fd, int d, int o)
{
    off_t r = lseek(fd, d, o == POS_INICIO ? SEEK_SET : o == POS_ATUAL ? SEEK_CUR : SEEK_END);
    return r < 0 ? -1 : (int)r;
}
#endif
