// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_visao_referencia.c - la referencia de llama.cpp (libmtmd) para el proyector de imágenes de
 * LFM3-VL (D380, fase 2). Solo para el PC, como tools/llm_referencia.c: se enlaza con la libmtmd
 * que se compila en ~/.local/saguitools/llama.cpp (fuera de git; llama.cpp es MIT).
 *
 *   llm_visao_referencia MODELO MMPROJ IMAGEN.ppm TOKENS_MAX VOLCADO
 *
 * Codifica la imagen (PPM P6) y vuelca los vectores (float32, una fila de n_embd por vector);
 * escribe «VECTORES: n de n_embd». tools/llm_comparar.py --visao lo compara con `llm-pc`. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llama.h"
#include "mtmd.h"

static void calado(enum ggml_log_level n, const char *t, void *u) { (void)n; (void)t; (void)u; }

static unsigned char *ler_ppm(const char *c, int *w, int *h)
{
    FILE *f = fopen(c, "rb");
    if (!f) return NULL;
    int mx;
    if (fscanf(f, "P6 %d %d %d", w, h, &mx) != 3 || mx != 255) { fclose(f); return NULL; }
    fgetc(f);
    unsigned char *d = malloc((size_t)*w * *h * 3);
    if (d && fread(d, 3, (size_t)*w * *h, f) != (size_t)*w * *h) { free(d); d = NULL; }
    fclose(f);
    return d;
}

int main(int argc, char **argv)
{
    if (argc < 6) { fprintf(stderr, "uso: %s modelo mmproj imagen.ppm tokens_max volcado\n", argv[0]); return 2; }
    llama_log_set(calado, NULL);
    llama_backend_init();
    struct llama_model_params mp = llama_model_default_params();
    struct llama_model *m = llama_model_load_from_file(argv[1], mp);
    if (!m) { fprintf(stderr, "no se pudo cargar el modelo\n"); return 1; }
    struct mtmd_context_params p = mtmd_context_params_default();
    p.use_gpu = false;
    p.n_threads = 4;
    p.warmup = false;
    p.print_timings = false;
    p.image_max_tokens = atoi(argv[4]);
    mtmd_context *ctx = mtmd_init_from_file(argv[2], m, p);
    if (!ctx) { fprintf(stderr, "no se pudo cargar el mmproj\n"); return 1; }
    int w, h;
    unsigned char *rgb = ler_ppm(argv[3], &w, &h);
    if (!rgb) { fprintf(stderr, "no se pudo leer la imagen\n"); return 1; }
    mtmd_bitmap *bm = mtmd_bitmap_init((uint32_t)w, (uint32_t)h, rgb);
    mtmd_input_chunks *ch = mtmd_input_chunks_init();
    char prompt[64];
    snprintf(prompt, sizeof prompt, "%s", mtmd_default_marker());
    mtmd_input_text t = { prompt, strlen(prompt), false, true };
    const mtmd_bitmap *bms[1] = { bm };
    if (mtmd_tokenize(ctx, ch, &t, bms, 1)) { fprintf(stderr, "mtmd_tokenize falló\n"); return 1; }
    int n_embd = llama_model_n_embd(m);
    for (size_t i = 0; i < mtmd_input_chunks_size(ch); i++) {
        const mtmd_input_chunk *c = mtmd_input_chunks_get(ch, i);
        if (mtmd_input_chunk_get_type(c) != MTMD_INPUT_CHUNK_TYPE_IMAGE) continue;
        if (mtmd_encode_chunk(ctx, c)) { fprintf(stderr, "mtmd_encode_chunk falló\n"); return 1; }
        size_t n = mtmd_input_chunk_get_n_tokens(c);
        float *e = mtmd_get_output_embd(ctx);
        FILE *f = fopen(argv[5], "wb");
        if (f) { fwrite(e, 4, n * (size_t)n_embd, f); fclose(f); }
        printf("VECTORES: %zu de %d\n", n, n_embd);
    }
    mtmd_input_chunks_free(ch);
    mtmd_bitmap_free(bm);
    mtmd_free(ctx);
    free(rgb);
    llama_model_free(m);
    return 0;
}
