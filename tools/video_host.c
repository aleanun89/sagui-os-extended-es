// SPDX-License-Identifier: GPL-2.0-or-later
/* video_host.c - pegamento para compilar el decodificador de vídeo de la libsagui
 * (userland/libsagui/video.c) en el anfitrión, con -DSAGUI_HOST: los tipos y las
 * pocas llamadas de archivo y memoria que la biblioteca usa. Solo sirve para las
 * pruebas (h100) y la depuración; en el SaguiOS de verdad, estas funciones están
 * en la libsagui y en el kernel. */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "imagem.h"

int abrir(const char *c, int modo)
{
    (void)modo;
    int fd = open(c, O_RDONLY);
    return fd < 0 ? -2 : fd;
}

int ler(int fd, void *b, unsigned n) { return (int)read(fd, b, n); }
int fechar(int fd) { return close(fd); }

int posicionar(int fd, int d, int o)
{
    off_t r = lseek(fd, (off_t)d, o == 0 ? SEEK_SET : o == 1 ? SEEK_CUR : SEEK_END);
    return r < 0 ? -1 : (int)r;
}

void *aloca(size_t n) { return malloc(n ? n : 1); }
void *aloca_zerado(size_t n) { return calloc(n ? n : 1, 1); }
void libera(void *p) { free(p); }
void *realoca(void *p, size_t n) { return realloc(p, n ? n : 1); }

/* La imagen en memoria: los mismos píxeles que en el SaguiOS (0x00RRGGBB). */
bool imagem_criar(imagem_t *img, int larg, int alt, uint32_t cor)
{
    img->larg = img->alt = 0;
    img->px = NULL;
    img->formato[0] = 0;
    if (larg <= 0 || alt <= 0) return false;
    img->px = malloc((size_t)larg * (size_t)alt * 4);
    if (!img->px) return false;
    img->larg = larg;
    img->alt = alt;
    for (int i = 0; i < larg * alt; i++) img->px[i] = cor;
    return true;
}

void imagem_liberar(imagem_t *img)
{
    free(img->px);
    img->px = NULL;
    img->larg = img->alt = 0;
}

/* Los dos textos que el decodificador usa para los errores. */
const char *nome_erro(int erro) { (void)erro; return "erro"; }
char *acha_ultimo_char(const char *s, int c)
{
    const char *ultimo = NULL;
    for (; *s; s++) if (*s == (char)c) ultimo = s;
    return (char *)ultimo;
}
