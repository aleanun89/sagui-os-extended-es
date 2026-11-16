// SPDX-License-Identifier: GPL-3.0-or-later
/* conversor_audio_host.c - el conversor de audio (decodificadores + audio_conversor.c) en el anfitrión,
 * para las pruebas rápidas (h341). Uso: conversor_audio_host entrada saida.{wav|flac} [taxa] [canales] [bits|nivel] */
#ifndef AUDIO_HOST
#define AUDIO_HOST
#endif
#include <stdio.h>
#include "audio_formato.h"
#include "audio_conversor.h"

static int esc(void *c, const void *d, unsigned n) { return fwrite(d, 1, n, (FILE *)c) == n ? 0 : -1; }
static int par(void *c, unsigned desde, const void *d, unsigned n)
{
    FILE *f = c;
    long p = ftell(f);
    fseek(f, desde, SEEK_SET);
    int r = fwrite(d, 1, n, f) == n ? 0 : -1;
    fseek(f, p, SEEK_SET);
    return r;
}

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    int erro;
    audio_arquivo_t *a = audio_arquivo_abrir(argv[1], &erro);
    if (!a) { fprintf(stderr, "erro %d\n", erro); return 1; }
    const audio_info_t *i = audio_arquivo_info(a);
    uint32_t taxa = argc > 3 ? (uint32_t)atoi(argv[3]) : i->taxa, can = argc > 4 ? (uint32_t)atoi(argv[4]) : i->canais;
    int opc = argc > 5 ? atoi(argv[5]) : 16;
    int flac = strstr(argv[2], ".flac") != NULL;
    FILE *fo = fopen(argv[2], "w+b");
    cv_salida_t s = { esc, par, fo };
    cv_wav_t w; cv_flac_t *f = NULL;
    if (flac) f = cv_flac_iniciar(s, taxa, can, opc); else cv_wav_iniciar(&w, s, taxa, can, (uint32_t)opc);
    cv_rem_t r;
    cv_rem_iniciar(&r, i->taxa, i->canais, taxa, can);
    static int16_t in[4096 * 2], out[(4096 * 8 + 8) * 2];
    int n;
    while ((n = audio_arquivo_ler(a, in, 4096)) > 0) {
        unsigned m = cv_rem_procesar(&r, in, (unsigned)n, out);
        if (flac) cv_flac_escrever(f, out, m); else cv_wav_escrever(&w, out, m);
    }
    if (flac) cv_flac_fechar(f); else cv_wav_fechar(&w);
    fclose(fo);
    audio_arquivo_fechar(a);
    return 0;
}
