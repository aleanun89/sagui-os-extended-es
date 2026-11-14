// SPDX-License-Identifier: GPL-2.0-or-later
/* audio_decodificar.c - decodifica un archivo de sonido con los decodificadores
 * de la libsagui, en el anfitrión: PCM de 16 bits intercalado en la salida estándar y la
 * descripción del archivo en el error estándar. Sirve a las pruebas (h75) y a la depuración.
 *   audio_decodificar archivo [ms] > saida.pcm      (con ms: salta antes de decodificar, D115) */
#include <stdio.h>
#include <stdlib.h>
#include "audio_formato.h"

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "uso: %s arquivo\n", argv[0]); return 2; }
    int erro;
    audio_arquivo_t *a = audio_arquivo_abrir(argv[1], &erro);
    if (!a) { fprintf(stderr, "erro %d: %s\n", erro, audio_erro_texto(erro)); return 1; }
    const audio_info_t *i = audio_arquivo_info(a);
    fprintf(stderr, "%s %u Hz %u canais %u quadros %u ms %u kbps%s titulo='%s' artista='%s' album='%s'\n", i->nome_formato,
            i->taxa, i->canais, i->quadros, i->duracao_ms, i->kbps, i->vbr ? " VBR" : "", i->titulo, i->artista, i->album);
    if (argc > 2) {
        int r = audio_arquivo_buscar(a, (uint32_t)atoi(argv[2]));
        fprintf(stderr, "salto a %s ms: %d, posicion %u cuadros\n", argv[2], r, audio_arquivo_posicao(a));
    }
    static int16_t pcm[4096 * 2];
    long total = 0;
    int n, rc = 0;
    while ((n = audio_arquivo_ler(a, pcm, 4096)) > 0) {
        fwrite(pcm, 2 * i->canais, n, stdout);
        total += n;
    }
    if (n < 0) { fprintf(stderr, "erro %d: %s (depois de %ld quadros)\n", n, audio_erro_texto(n), total); rc = 1; }
    fprintf(stderr, "decodificados %ld quadros, %u ruins\n", total, audio_arquivo_info(a)->quadros_ruins);
    audio_arquivo_fechar(a);
    return rc;
}
