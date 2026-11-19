// SPDX-License-Identifier: GPL-2.0-or-later
/* brilho_conv.c - niveles de brillo de los portátiles (D403). Sin hardware: ver brilho_conv.h.
 *
 * Lo que se ve en las BIOS reales: el _BCL con los dos primeros valores
 * (cargador y batería) metidos también en la lista, niveles repetidos o al
 * revés, listas en 0-100 (lo normal), en 0-255 o en 0-15; un _BQC que da el
 * índice en vez del valor, o 0 antes del primer _BCM. Ante la duda: «no lo sé»
 * (-1) y el driver se queda con lo último que puso él. */
#include "brilho_conv.h"

#define VALOR_MAX 0xFFFFu

int brilho_niveis(const uint64_t *bcl, int n, uint32_t *niv, int max)
{
    if (!bcl || n <= 0 || !niv || max <= 0) return 0;
    int ini = n > 2 ? 2 : 0;   /* los dos primeros son los de cargador y batería */
    int k = 0;
    for (int i = ini; i < n; i++) {
        if (bcl[i] > VALOR_MAX) continue;
        uint32_t v = (uint32_t)bcl[i];
        /* inserción ordenada sin repetidos */
        int p = 0;
        while (p < k && niv[p] < v) p++;
        if (p < k && niv[p] == v) continue;
        if (k == max) {
            if (p == k) continue;   /* lleno: el mayor se pierde */
            k--;
        }
        for (int j = k; j > p; j--) niv[j] = niv[j - 1];
        niv[p] = v;
        k++;
    }
    return k;
}

int brilho_indice(const uint32_t *niv, int n, uint32_t valor)
{
    if (n <= 0) return -1;
    int melhor = 0;
    uint32_t dmin = 0xFFFFFFFFu;
    for (int i = 0; i < n; i++) {
        uint32_t d = niv[i] > valor ? niv[i] - valor : valor - niv[i];
        if (d < dmin) { dmin = d; melhor = i; }
    }
    return melhor;
}

int brilho_bqc(const uint32_t *niv, int n, uint64_t bqc, bool *e_indice)
{
    if (e_indice) *e_indice = false;
    if (n <= 0 || bqc > VALOR_MAX) return -1;
    uint32_t v = (uint32_t)bqc;
    for (int i = 0; i < n; i++)
        if (niv[i] == v) return i;
    /* No es un valor de la lista: algunas BIOS dan el índice. Solo si cabe y
     * los valores no son ya 0..n-1 (entonces habría coincidido arriba). */
    if (v < (uint32_t)n) {
        if (e_indice) *e_indice = true;
        return (int)v;
    }
    return -1;
}

int brilho_porcentagem(const uint32_t *niv, int n, int indice)
{
    if (n <= 0 || indice < 0 || indice >= n) return -1;
    uint32_t maior = niv[n - 1];
    if (maior == 0) return 0;
    if (maior <= 100) return (int)niv[indice];
    return (int)((niv[indice] * 100u + maior / 2) / maior);
}

int brilho_indice_de_pct(const uint32_t *niv, int n, int pct)
{
    if (n <= 0) return -1;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    int melhor = 0, dmin = 1000;
    for (int i = 0; i < n; i++) {
        int p = brilho_porcentagem(niv, n, i);
        int d = p > pct ? p - pct : pct - p;
        if (d < dmin) { dmin = d; melhor = i; }
    }
    return melhor;
}

int brilho_passo(int n, int indice, int passo)
{
    if (n <= 0) return -1;
    if (indice < 0) indice = passo > 0 ? -1 : n;   /* sin nivel conocido: desde fuera de la lista */
    long r = (long)indice + passo;
    if (r < 0) r = 0;
    if (r >= n) r = n - 1;
    return (int)r;
}

int brilho_pct_linear(int nivel, int total)
{
    if (total <= 1 || nivel < 0) return -1;
    if (nivel >= total) nivel = total - 1;
    return (nivel * 100 + (total - 1) / 2) / (total - 1);
}

int brilho_nivel_linear(int pct, int total)
{
    if (total <= 1) return 0;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return (pct * (total - 1) + 50) / 100;
}
