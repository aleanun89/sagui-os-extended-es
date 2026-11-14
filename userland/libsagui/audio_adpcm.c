// SPDX-License-Identifier: GPL-2.0-or-later
/* audio_adpcm.c - IMA ADPCM en WAV (formato 0x11), mejora 053 fase 1 (D098).
 *
 * El algoritmo IMA/DVI: cada muestra de 4 bits (un signo y tres de módulo)
 * mueve un predictor de 16 bits por un paso que sale de una tabla de 89
 * valores; el índice del paso sube o baja según el módulo.
 *
 * Bloque del WAV (nBlockAlign bytes): por canal, 4 bytes de cabecera (predictor
 * de 16 bits, índice del paso, un byte cero) y después los nibbles en grupos
 * de 4 bytes por canal (8 muestras), los canales intercalados de grupo en
 * grupo; el nibble bajo va primero. La primera muestra de cada canal es el
 * propio predictor de la cabecera: un bloque tiene (bloque - 4*canales)*8/(4*canales)+1
 * muestras por canal. */
#include "audio_interno.h"

static adpcm_t g_adpcm;

static const int8_t tab_indice[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };
static const uint16_t tab_passo[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73,
    80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494,
    544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499,
    2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

typedef struct { int pred, indice; } canal_t;

static inline int16_t expandir(canal_t *c, int nib)
{
    int passo = tab_passo[c->indice];
    int d = ((2 * (nib & 7) + 1) * passo) >> 3;   /* (módulo + 1/2) * paso / 4 */
    int p = c->pred + ((nib & 8) ? -d : d);
    if (p > 32767) p = 32767;
    if (p < -32768) p = -32768;
    c->pred = p;
    int i = c->indice + tab_indice[nib];
    c->indice = i < 0 ? 0 : i > 88 ? 88 : i;
    return (int16_t)p;
}

int adpcm_abrir(audio_arquivo_t *a, uint32_t canais, uint32_t taxa, uint32_t bloco, uint32_t amostras, uint32_t dados)
{
    adpcm_t *d = &g_adpcm;
    (void)taxa;
    if (bloco < 4 * canais + 4 * canais || bloco > ADPCM_BLOCO_MAX || (bloco - 4 * canais) % (4 * canais)) return AUDIO_ERRO_NAO_SUPORTADO;
    uint32_t spb = (bloco - 4 * canais) * 8 / (4 * canais) + 1;
    (void)amostras;   /* la cabecera lo repite; vale la cuenta por el tamaño del bloque */
    memset(d, 0, sizeof *d);
    d->bloco = bloco;
    d->amostras_bloco = spb;
    d->canais = canais;
    d->restam = dados;
    d->total = dados;
    a->adpcm = d;
    uint32_t cheios = dados / bloco, resto = dados % bloco;
    uint32_t q = cheios * spb;
    if (resto >= 4 * canais + 4 * canais) q += (resto - 4 * canais) / (4 * canais) * 8 + 1;
    a->info.quadros = q;
    a->info.kbps = bloco * 8 * taxa / spb / 1000   /* cabe en 32 bits: bloque <= 4096, tasa <= 96000 */;
    return 0;
}

/* Decodifica el siguiente bloque; devuelve 0 si terminó, <0 si está cortado. */
static int proximo_bloco(audio_arquivo_t *a)
{
    adpcm_t *d = a->adpcm;
    uint32_t canais = d->canais;
    if (d->restam < 8 * canais) return 0;
    uint32_t tam = d->restam < d->bloco ? d->restam : d->bloco;
    if (fl_ler(&a->fl, d->dados, tam) != tam) { d->restam = 0; return AUDIO_ERRO_TRUNCADO; }
    d->restam -= tam;
    tam -= (tam - 4 * canais) % (4 * canais);   /* solo grupos enteros */
    uint32_t grupos = (tam - 4 * canais) / (4 * canais);
    uint32_t n = grupos * 8 + 1;
    canal_t c[2];
    for (uint32_t k = 0; k < canais; k++) {
        const uint8_t *h = d->dados + 4 * k;
        c[k].pred = (int16_t)(h[0] | h[1] << 8);
        c[k].indice = h[2];
        if (c[k].indice > 88) return AUDIO_ERRO_CORROMPIDO;
        d->pcm[k] = (int16_t)c[k].pred;
    }
    for (uint32_t g = 0; g < grupos; g++) {
        for (uint32_t k = 0; k < canais; k++) {
            const uint8_t *p = d->dados + 4 * canais + (g * canais + k) * 4;
            int16_t *o = d->pcm + (1 + g * 8) * canais + k;
            for (int m = 0; m < 4; m++) {
                o[(2 * m) * canais] = expandir(&c[k], p[m] & 15);
                o[(2 * m + 1) * canais] = expandir(&c[k], p[m] >> 4);
            }
        }
    }
    d->pcm_n = n;
    d->pcm_i = 0;
    return 1;
}

/* Salto: al bloque que contiene el cuadro y, dentro de él, hasta el cuadro exacto. */
int adpcm_buscar(audio_arquivo_t *a, uint32_t quadro)
{
    adpcm_t *d = a->adpcm;
    uint32_t bl = quadro / d->amostras_bloco, bytes = bl * d->bloco;
    if (bytes > d->total) bytes = d->total;
    d->restam = d->total - bytes;
    d->pcm_n = d->pcm_i = 0;
    if (!fl_posicionar(&a->fl, a->inicio_dados + bytes)) return AUDIO_ERRO_TRUNCADO;
    uint32_t dentro = quadro - bl * d->amostras_bloco;
    if (dentro && d->restam) {
        int r = proximo_bloco(a);
        if (r < 0) return r;
        d->pcm_i = dentro < d->pcm_n ? dentro : d->pcm_n;
    }
    return 0;
}

int adpcm_ler(audio_arquivo_t *a, int16_t *pcm, unsigned n)
{
    adpcm_t *d = a->adpcm;
    unsigned feito = 0;
    while (feito < n) {
        if (d->pcm_i >= d->pcm_n) {
            if (a->pend) return feito ? (int)feito : a->pend;
            int r = proximo_bloco(a);
            if (r <= 0) {
                if (r < 0) a->pend = r;
                return feito ? (int)feito : r;
            }
        }
        unsigned q = d->pcm_n - d->pcm_i;
        if (q > n - feito) q = n - feito;
        memcpy(pcm + feito * d->canais, d->pcm + d->pcm_i * d->canais, q * d->canais * 2);
        d->pcm_i += q;
        feito += q;
    }
    return (int)feito;
}
