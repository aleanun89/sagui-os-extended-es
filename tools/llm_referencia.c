// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_referencia.c - la referencia de llama.cpp para comparar el motor `llm` (D380).
 * Solo para el PC: se enlaza con la libllama que tools/llm_comparar.py compila en
 * ~/.local/saguitools/llama.cpp (fuera de git; llama.cpp es MIT y no va en SaguiOS).
 *
 *   llm_referencia MODELO CONTEXTO "id,id,..." N VOLCADO [q8]
 *
 * Pasa los tokens de uno en uno (así hay logits en cada posición), genera N con el más
 * probable, escribe «GERADOS: ...» y vuelca los logits (float32, una fila por posición)
 * en VOLCADO. Con «q8», la caché K/V en Q8_0 (como el motor). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llama.h"

static void calado(enum ggml_log_level n, const char *t, void *u) { (void)n; (void)t; (void)u; }

int main(int argc, char **argv)
{
    if (argc < 6) { fprintf(stderr, "uso: %s modelo contexto ids n volcado [q8]\n", argv[0]); return 2; }
    llama_log_set(calado, NULL);
    llama_backend_init();
    struct llama_model_params mp = llama_model_default_params();
    struct llama_model *m = llama_model_load_from_file(argv[1], mp);
    if (!m) { fprintf(stderr, "no se pudo cargar el modelo\n"); return 1; }
    struct llama_context_params cp = llama_context_default_params();
    cp.n_ctx = (uint32_t)atoi(argv[2]);
    cp.n_batch = cp.n_ubatch = 512;
    cp.n_threads = cp.n_threads_batch = 4;
    if (argc > 6 && !strcmp(argv[6], "q8")) {
        cp.type_k = GGML_TYPE_Q8_0;
        cp.type_v = GGML_TYPE_Q8_0;
        cp.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;
    }
    struct llama_context *c = llama_init_from_model(m, cp);
    if (!c) { fprintf(stderr, "no se pudo crear el contexto\n"); return 1; }
    const struct llama_vocab *v = llama_model_get_vocab(m);
    int nv = llama_vocab_n_tokens(v);
    int toks[8192], nt = 0;
    for (char *p = argv[3]; *p && nt < 8192;) {
        toks[nt++] = (int)strtol(p, &p, 10);
        if (*p == ',') p++;
    }
    int n = atoi(argv[4]);
    FILE *f = fopen(argv[5], "wb");
    printf("GERADOS:");
    int ult = 0;
    for (int i = 0; i < nt + n - 1; i++) {
        int t = i < nt ? toks[i] : ult;
        struct llama_batch b = llama_batch_get_one(&t, 1);
        if (llama_decode(c, b)) { fprintf(stderr, "llama_decode falló\n"); return 1; }
        float *lg = llama_get_logits_ith(c, -1);
        fwrite(lg, 4, (size_t)nv, f);
        if (i >= nt - 1) {
            int mx = 0;
            for (int k = 1; k < nv; k++) if (lg[k] > lg[mx]) mx = k;
            ult = mx;
            printf(" %d", mx);
        }
    }
    printf("\n");
    fclose(f);
    llama_free(c);
    llama_model_free(m);
    return 0;
}
