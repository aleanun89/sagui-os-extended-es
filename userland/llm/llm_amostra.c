// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_amostra.c - elegir el siguiente token (D380): penalización de repetición,
 * temperatura, top-k, top-p y un generador con semilla (xorshift64*), para que la misma
 * semilla dé el mismo texto en SaguiOS y en el PC. Con temperatura 0, el más probable. */
#include "llm_programa.h"

void llm_amostra_iniciar(llm_amostra_t *a, uint32_t semente)
{
    a->temperatura = 0.7f;
    a->top_k = 40;
    a->top_p = 0.95f;
    a->penalizacao = 1.1f;
    a->janela_pen = 64;
    a->estado = 0x9E3779B97F4A7C15ull ^ semente;
    if (!a->estado) a->estado = 1;
}

static uint32_t proximo(llm_amostra_t *a)
{
    a->estado ^= a->estado >> 12;
    a->estado ^= a->estado << 25;
    a->estado ^= a->estado >> 27;
    return (uint32_t)((a->estado * 0x2545F4914F6CDD1Dull) >> 32);
}

int llm_argmax(const float *v, int n)
{
    int m = 0;
    for (int i = 1; i < n; i++) if (v[i] > v[m]) m = i;
    return m;
}

typedef struct { float p; int id; } cand_t;

int llm_amostrar(llm_amostra_t *a, float *logits, int n, const int *ultimos, int n_ultimos)
{
    /* penalización (como llama.cpp: divide los positivos, multiplica los negativos) */
    if (a->penalizacao != 1.0f && ultimos) {
        int ini = n_ultimos > a->janela_pen ? n_ultimos - a->janela_pen : 0;
        for (int i = ini; i < n_ultimos; i++) {
            int t = ultimos[i];
            if (t < 0 || t >= n) continue;
            bool visto = false;
            for (int j = ini; j < i; j++) if (ultimos[j] == t) { visto = true; break; }
            if (visto) continue;
            logits[t] = logits[t] > 0 ? logits[t] / a->penalizacao : logits[t] * a->penalizacao;
        }
    }
    if (a->temperatura <= 0) return llm_argmax(logits, n);
    int k = a->top_k > 0 && a->top_k < n ? a->top_k : (n < 256 ? n : 256);
    cand_t c[256];
    if (k > 256) k = 256;
    int nc = 0;
    /* los k mayores, por inserción en una lista ordenada (k es pequeño) */
    for (int i = 0; i < n; i++) {
        float v = logits[i];
        if (nc == k && v <= c[nc - 1].p) continue;
        int j = nc < k ? nc++ : nc - 1;
        while (j > 0 && c[j - 1].p < v) { c[j] = c[j - 1]; j--; }
        c[j].p = v; c[j].id = i;
    }
    float maxv = c[0].p, soma = 0;
    for (int i = 0; i < nc; i++) { c[i].p = llm_expf((c[i].p - maxv) / a->temperatura); soma += c[i].p; }
    float acum = 0;
    int corte = nc;
    for (int i = 0; i < nc; i++) {
        c[i].p /= soma;
        acum += c[i].p;
        if (acum >= a->top_p) { corte = i + 1; break; }
    }
    float tot = 0;
    for (int i = 0; i < corte; i++) tot += c[i].p;
    float r = (float)(proximo(a) >> 8) / 16777216.0f * tot;
    for (int i = 0; i < corte; i++) {
        r -= c[i].p;
        if (r < 0) return c[i].id;
    }
    return c[corte - 1].id;
}

/* Solo la semilla (deja la temperatura y lo demás como estén). */
void llm_amostra_iniciar_semente(llm_amostra_t *a, uint32_t semente)
{
    a->estado = 0x9E3779B97F4A7C15ull ^ semente;
    if (!a->estado) a->estado = 1;
}
