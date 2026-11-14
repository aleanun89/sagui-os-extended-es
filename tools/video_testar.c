// SPDX-License-Identifier: GPL-2.0-or-later
/* video_testar.c - decodifica un vídeo con el decodificador de la libsagui en el
 * anfitrión: dice qué formato tiene, cuánto dura, y escribe los cuadros
 * decodificados en crudo (RGB24) o sus sumas por cuadro. Sirve a las pruebas
 * (h100) y a la depuración.
 *
 *   video_testar [--saida arquivo.raw] [--desde n] [--quadros n] video.avi
 *
 * El archivo crudo lleva una cabecera de texto ("SAGUIV1 <ancho> <alto>\n") y
 * después los cuadros, uno detrás de otro, con los bytes RGB en el orden en que
 * los pinta la ventana (fila a fila, de arriba abajo). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "video.h"

int main(int argc, char **argv)
{
    const char *saida = NULL;
    int desde = 0, quantos = -1, i = 1;
    while (i < argc && argv[i][0] == '-' && argv[i][1] == '-') {
        if (!strcmp(argv[i], "--saida") && i + 1 < argc) { saida = argv[++i]; i++; }
        else if (!strcmp(argv[i], "--desde") && i + 1 < argc) { desde = atoi(argv[++i]); i++; }
        else if (!strcmp(argv[i], "--quadros") && i + 1 < argc) { quantos = atoi(argv[++i]); i++; }
        else { fprintf(stderr, "opción desconocida: %s\n", argv[i]); return 2; }
    }
    if (i >= argc) { fprintf(stderr, "uso: %s [--saida arq.raw] [--desde n] [--quadros n] video\n", argv[0]); return 2; }

    int erro = 0;
    video_t *v = video_abrir(argv[i], &erro);
    if (!v) { fprintf(stderr, "erro %d: %s\n", erro, video_erro_texto(erro)); return 1; }
    const video_info_t *inf = video_info(v);
    fprintf(stderr, "%s %dx%d %d fps %d cuadros %u ms audio=%s %u Hz %d canais\n", inf->nome_formato, inf->larg, inf->alt,
            inf->fps, inf->quadros, inf->duracao_ms, inf->tem_audio ? "si" : "no", inf->taxa_audio, inf->canais_audio);

    FILE *f = NULL;
    if (saida) {
        f = fopen(saida, "wb");
        if (!f) { fprintf(stderr, "no se puede escribir %s\n", saida); return 1; }
        fprintf(f, "SAGUIV1 %d %d\n", inf->larg, inf->alt);
    }

    imagem_t img = { 0 };
    int n = 0, feitos = 0;
    while (video_quadro(v, &img) == 0) {
        if (n >= desde && (quantos < 0 || feitos < quantos)) {
            uint32_t sr = 0, sg = 0, sb = 0;
            for (int p = 0; p < img.larg * img.alt; p++) {
                sr += (img.px[p] >> 16) & 255;
                sg += (img.px[p] >> 8) & 255;
                sb += img.px[p] & 255;
            }
            printf("cuadro %d %dx%d %u %u %u\n", n, img.larg, img.alt, sr, sg, sb);
            if (f) {
                for (int p = 0; p < img.larg * img.alt; p++) {
                    fputc((img.px[p] >> 16) & 255, f);
                    fputc((img.px[p] >> 8) & 255, f);
                    fputc(img.px[p] & 255, f);
                }
            }
            feitos++;
        }
        n++;
    }
    if (f) fclose(f);
    fprintf(stderr, "%d cuadros\n", n);
    video_fechar(v);
    if (img.px) imagem_liberar(&img);
    return 0;
}
