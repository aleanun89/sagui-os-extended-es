// SPDX-License-Identifier: GPL-2.0-or-later
/* inflar.c - descompresión DEFLATE (RFC 1951) en el núcleo (v8.5.4.2, D379: la raíz comprimida del CD).
 *
 * Es el inflate propio de userland/libsagui/compressao.c (D054, D103) traído al núcleo tal cual, con
 * dos cambios: las tablas de Huffman van en un contexto que da quien llama (en userland eran estáticas;
 * aquí puede haber más de un lector) y los errores son los INFLAR_* de inflar.h. Si se corrige algo
 * aquí, corregirlo también allí. */
#include "inflar.h"
#include "texto.h"

/* ------------------------------------------------------------ tablas comunes */

static const uint16_t base_comp[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const uint8_t extra_comp[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const uint16_t base_dist[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                                        8193, 12289, 16385, 24577 };
static const uint8_t extra_dist[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                                        7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
static const uint8_t ordem_cl[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

/* ------------------------------------------------------------ inflate (RFC 1951) */

#define RAPIDO 9   /* bits de la tabla de búsqueda directa */

typedef struct {
    const uint8_t *ent;
    int n, pos;
    uint32_t bits;   /* bits ya leídos y aún sin usar (los de abajo primero) */
    int qtd;
    bool estourou;
} bits_t;

typedef struct {
    uint16_t contagem[16];   /* cuántos códigos de cada longitud */
    uint16_t simbolo[288];   /* símbolos en orden de código */
    uint16_t rapida[1 << RAPIDO];   /* (longitud << 12 | símbolo) por los próximos RAPIDO bits; 0 = código largo */
} huff_t;

_Static_assert(sizeof(huff_t) == sizeof(((inflar_ctx_t *)0)->hc), "inflar_ctx_t: el tamaño de huff_t");

static inline void encher(bits_t *b, int n)
{
    while (b->qtd < n) {
        if (b->pos >= b->n) return;
        b->bits |= (uint32_t)b->ent[b->pos++] << b->qtd;
        b->qtd += 8;
    }
}

static int ler_bits(bits_t *b, int n)
{
    if (n == 0) return 0;
    encher(b, n);
    if (b->qtd < n) { b->estourou = true; return 0; }
    int v = (int)(b->bits & ((1u << n) - 1));
    b->bits >>= n;
    b->qtd -= n;
    return v;
}

static uint32_t inverter(uint32_t v, int n)
{
    uint32_t r = 0;
    while (n--) { r = r << 1 | (v & 1); v >>= 1; }
    return r;
}

/* Construye la tabla a partir de las longitudes. Códigos canónicos: los de la misma
 * longitud son consecutivos, en el orden de los símbolos. La tabla rápida cubre los
 * códigos de hasta RAPIDO bits (los demás se decodifican bit a bit). */
static int construir(huff_t *h, const uint8_t *comp, int n)
{
    uint16_t desloc[16];
    uint32_t prox[16];
    for (int i = 0; i < 16; i++) h->contagem[i] = 0;
    for (int s = 0; s < n; s++) h->contagem[comp[s]]++;
    h->contagem[0] = 0;
    int sobra = 1;
    for (int len = 1; len < 16; len++) {
        sobra <<= 1;
        sobra -= h->contagem[len];
        if (sobra < 0) return -1;   /* demasiados códigos para la longitud */
    }
    desloc[1] = 0;
    for (int len = 1; len < 15; len++) desloc[len + 1] = (uint16_t)(desloc[len] + h->contagem[len]);
    for (int s = 0; s < n; s++)
        if (comp[s]) h->simbolo[desloc[comp[s]]++] = (uint16_t)s;
    memset(h->rapida, 0, sizeof h->rapida);
    uint32_t cod = 0;
    for (int len = 1; len < 16; len++) { cod = (cod + h->contagem[len - 1]) << 1; prox[len] = cod; }
    for (int s = 0; s < n; s++) {
        int len = comp[s];
        if (!len || len > RAPIDO) continue;
        uint32_t c = prox[len]++;
        uint32_t r = inverter(c, len);
        for (uint32_t k = r; k < (1u << RAPIDO); k += 1u << len)
            h->rapida[k] = (uint16_t)(len << 12 | s);
    }
    return 0;
}

/* Decodifica un símbolo: primero la tabla rápida; si no, bit a bit: en cada longitud
 * sus códigos ocupan el intervalo [primero, primero + cuenta). */
static int decodificar(bits_t *b, const huff_t *h)
{
    encher(b, RAPIDO);
    uint16_t r = h->rapida[b->bits & ((1u << RAPIDO) - 1)];
    if (r) {
        int len = r >> 12;
        if (len > b->qtd) { b->estourou = true; return -1; }
        b->bits >>= len;
        b->qtd -= len;
        return r & 0xFFF;
    }
    int codigo = 0, primeiro = 0, indice = 0;
    for (int len = 1; len < 16; len++) {
        codigo |= ler_bits(b, 1);
        if (b->estourou) return -1;
        int c = h->contagem[len];
        if (codigo - primeiro < c) return h->simbolo[indice + codigo - primeiro];
        indice += c;
        primeiro = (primeiro + c) << 1;
        codigo <<= 1;
    }
    return -1;
}

static int bloco(bits_t *b, const huff_t *hc, const huff_t *hd, uint8_t *s, int *k, int max)
{
    for (;;) {
        int sim = decodificar(b, hc);
        if (sim < 0) return b->estourou ? INFLAR_TRUNCADO : INFLAR_FORMATO;
        if (sim < 256) {
            if (*k >= max) return INFLAR_CHEIO;
            s[(*k)++] = (uint8_t)sim;
        } else if (sim == 256) return 0;
        else {
            sim -= 257;
            if (sim >= 29) return INFLAR_FORMATO;
            int len = base_comp[sim] + ler_bits(b, extra_comp[sim]);
            int ds = decodificar(b, hd);
            if (ds < 0 || ds >= 30) return b->estourou ? INFLAR_TRUNCADO : INFLAR_FORMATO;
            int dist = base_dist[ds] + ler_bits(b, extra_dist[ds]);
            if (b->estourou) return INFLAR_TRUNCADO;
            if (dist > *k) return INFLAR_FORMATO;
            if (*k + len > max) return INFLAR_CHEIO;
            if (dist >= len) { memcpy(s + *k, s + *k - dist, (size_t)len); *k += len; }
            else for (int i = 0; i < len; i++, (*k)++) s[*k] = s[*k - dist];
        }
    }
}

/* Descomprime un flujo DEFLATE crudo entero (sin cabecera zlib ni gzip). */
int inflar_bloco(inflar_ctx_t *ctx, const uint8_t *ent, int n_ent, uint8_t *saida, int max_saida)
{
    huff_t *hc = (huff_t *)ctx->hc, *hd = (huff_t *)ctx->hd;
    bits_t b = { ent, n_ent, 0, 0, 0, false };
    uint8_t comp[320];
    int k = 0, ultimo, r;
    do {
        ultimo = ler_bits(&b, 1);
        int tipo = ler_bits(&b, 2);
        if (b.estourou) return INFLAR_TRUNCADO;
        if (tipo == 0) {   /* guardado sin comprimir */
            /* v8.5.4.2: los bytes enteros que ya estaban en el acumulador se devuelven a la entrada (antes se
             * perdían y un bloque guardado tras otro con bits de sobra no se leía bien); solo se tiran los bits
             * sueltos hasta el siguiente byte. */
            b.pos -= b.qtd / 8; b.bits = 0; b.qtd = 0;
            if (b.pos + 4 > b.n) return INFLAR_TRUNCADO;
            int len = ent[b.pos] | ent[b.pos + 1] << 8, nlen = ent[b.pos + 2] | ent[b.pos + 3] << 8;
            b.pos += 4;
            if ((len ^ 0xFFFF) != nlen) return INFLAR_FORMATO;
            if (b.pos + len > b.n) return INFLAR_TRUNCADO;
            if (k + len > max_saida) return INFLAR_CHEIO;
            memcpy(saida + k, ent + b.pos, (size_t)len);
            b.pos += len;
            k += len;
            continue;
        }
        if (tipo == 1) {   /* códigos fijos */
            int s = 0;
            for (; s < 144; s++) comp[s] = 8;
            for (; s < 256; s++) comp[s] = 9;
            for (; s < 280; s++) comp[s] = 7;
            for (; s < 288; s++) comp[s] = 8;
            construir(hc, comp, 288);
            for (s = 0; s < 30; s++) comp[s] = 5;
            construir(hd, comp, 30);
        } else if (tipo == 2) {   /* códigos dinámicos, descritos en el propio bloque */
            int nlit = ler_bits(&b, 5) + 257, ndist = ler_bits(&b, 5) + 1, ncod = ler_bits(&b, 4) + 4;
            if (nlit > 286 || ndist > 30) return INFLAR_FORMATO;
            for (int i = 0; i < 19; i++) comp[ordem_cl[i]] = 0;
            for (int i = 0; i < ncod; i++) comp[ordem_cl[i]] = (uint8_t)ler_bits(&b, 3);
            if (b.estourou) return INFLAR_TRUNCADO;
            if (construir(hc, comp, 19) < 0) return INFLAR_FORMATO;
            int i = 0;
            while (i < nlit + ndist) {
                int sim = decodificar(&b, hc), rep, val = 0;
                if (sim < 0) return b.estourou ? INFLAR_TRUNCADO : INFLAR_FORMATO;
                if (sim < 16) { comp[i++] = (uint8_t)sim; continue; }
                if (sim == 16) { if (!i) return INFLAR_FORMATO; val = comp[i - 1]; rep = 3 + ler_bits(&b, 2); }
                else if (sim == 17) rep = 3 + ler_bits(&b, 3);
                else rep = 11 + ler_bits(&b, 7);
                if (i + rep > nlit + ndist) return INFLAR_FORMATO;
                while (rep--) comp[i++] = (uint8_t)val;
            }
            if (b.estourou) return INFLAR_TRUNCADO;
            if (comp[256] == 0) return INFLAR_FORMATO;
            if (construir(hc, comp, nlit) < 0 || construir(hd, comp + nlit, ndist) < 0) return INFLAR_FORMATO;
        } else return INFLAR_FORMATO;
        r = bloco(&b, hc, hd, saida, &k, max_saida);
        if (r < 0) return r;
    } while (!ultimo);
    return k;
}

/* ------------------------------------------------------------ CRC-32 (el de zlib y gzip) */

static uint32_t tab_crc[256];
static bool tab_crc_pronta;

uint32_t inflar_crc32(uint32_t crc, const void *dados, uint32_t n)
{
    if (!tab_crc_pronta) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            tab_crc[i] = c;
        }
        tab_crc_pronta = true;
    }
    const uint8_t *p = dados;
    crc = ~crc;
    while (n--) crc = tab_crc[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;
}
