// SPDX-License-Identifier: GPL-2.0-or-later
/* teste-jpeg.c - prova do decodificador de JPEG no anfitrión (melhora 047).
 *
 * Decodifica um arquivo JPEG e escreve a imagem num PPM, para comparar com o
 * decodificador de referência (djpeg, do libjpeg-turbo). Só serve para
 * development: dentro do SaguiOS quem lê JPEG é a libsagui.
 *
 *   gcc -O2 -Wall -I tools/host-jpeg -I userland/inc \
 *       tools/teste-jpeg.c userland/libsagui/imagem_jpeg.c -o build/teste-jpeg
 *   ./build/teste-jpeg entrada.jpg saida.ppm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imagem.h"

/* A libsagui usa o heap do sistema; aqui é malloc. */
bool imagem_criar(imagem_t *img, int larg, int alt, uint32_t cor)
{
    img->px = NULL;
    img->larg = img->alt = 0;
    img->formato[0] = 0;
    if (larg <= 0 || alt <= 0) return false;
    img->px = malloc((size_t)larg * (size_t)alt * sizeof(uint32_t));
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

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "uso: %s entrada.jpg saida.ppm\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "nao abre %s\n", argv[1]); return 2; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)n);
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "nao le %s\n", argv[1]);
        fclose(f);
        return 2;
    }
    fclose(f);

    imagem_t img;
    int r = imagem_ler_jpeg(buf, (int)n, &img);
    if (r < 0) {
        fprintf(stderr, "erro %d ao decodificar %s\n", r, argv[1]);
        free(buf);
        return 1;
    }
    printf("%d %d %s\n", img.larg, img.alt, img.formato);

    FILE *o = fopen(argv[2], "wb");
    if (!o) { fprintf(stderr, "nao cria %s\n", argv[2]); return 2; }
    fprintf(o, "P6\n%d %d\n255\n", img.larg, img.alt);
    for (int i = 0; i < img.larg * img.alt; i++) {
        uint32_t c = img.px[i];
        fputc((c >> 16) & 0xFF, o);
        fputc((c >> 8) & 0xFF, o);
        fputc(c & 0xFF, o);
    }
    fclose(o);
    imagem_liberar(&img);
    free(buf);
    return 0;
}
