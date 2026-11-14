// SPDX-License-Identifier: GPL-2.0-or-later
/* pdf_ccitt.c - el filtro CCITTFaxDecode del PDF (mejora 059, D138).
 *
 * Los PDF escaneados en blanco y negro guardan cada pagina como una imagen
 * de 1 bit comprimida como un fax: grupo 3 (T.4, una dimension o mixto) o
 * grupo 4 (T.6, dos dimensiones). Sin este filtro las paginas escaneadas
 * salian en blanco.
 *
 * Cada fila se codifica como las posiciones donde cambia el color
 * ("elementos de cambio"). En una dimension son longitudes de tramos
 * blancos y negros alternos, con codigos de Huffman fijos (tablas de la
 * recomendacion T.4). En dos dimensiones cada cambio se describe respecto
 * de la fila de arriba: vertical (el mismo sitio +-3), paso (saltar dos
 * cambios de arriba) u horizontal (dos tramos en una dimension).
 *
 * La salida es la imagen de 1 bit por pixel, filas alineadas a byte, con
 * negro = 0 (o 1 con /BlackIs1), que es lo que espera el resto del motor. */

#include "pdf_ccitt.h"

/* ---------------------------------------------------------- los bits */

typedef struct {
    const uint8_t *d;
    int n;
    int pos;        /* en bits */
} cc_bits_t;

static int cc_bit(cc_bits_t *b)
{
    if (b->pos >= b->n * 8) return -1;
    int v = (b->d[b->pos >> 3] >> (7 - (b->pos & 7))) & 1;
    b->pos++;
    return v;
}

/* Los 'len' bits siguientes sin consumirlos (con ceros al final). */
static int cc_ver(const cc_bits_t *b, int len)
{
    int v = 0;
    for (int i = 0; i < len; i++) {
        int p = b->pos + i;
        int bit = (p < b->n * 8) ? (b->d[p >> 3] >> (7 - (p & 7))) & 1 : 0;
        v = (v << 1) | bit;
    }
    return v;
}

static void cc_alinhar(cc_bits_t *b)
{
    b->pos = (b->pos + 7) & ~7;
}

/* ---------------------------------------------------------- las tablas
 *
 * Cada codigo: su longitud en bits, los bits y el tramo que vale. Los de
 * 64 en adelante son "de relleno" (makeup) y van seguidos de uno final. */

typedef struct {
    uint8_t  len;
    uint16_t bits;
    int16_t  valor;
} cc_cod_t;

/* Los codigos de relleno largos (1792 a 2560), comunes a blanco y negro. */
#define CC_EXT \
    {11, 0x008, 1792}, {11, 0x00C, 1856}, {11, 0x00D, 1920}, {12, 0x012, 1984}, \
    {12, 0x013, 2048}, {12, 0x014, 2112}, {12, 0x015, 2176}, {12, 0x016, 2240}, \
    {12, 0x017, 2304}, {12, 0x01C, 2368}, {12, 0x01D, 2432}, {12, 0x01E, 2496}, \
    {12, 0x01F, 2560}

static const cc_cod_t cc_branco[] = {
    {8, 0x35, 0}, {6, 0x07, 1}, {4, 0x7, 2}, {4, 0x8, 3}, {4, 0xB, 4}, {4, 0xC, 5},
    {4, 0xE, 6}, {4, 0xF, 7}, {5, 0x13, 8}, {5, 0x14, 9}, {5, 0x07, 10}, {5, 0x08, 11},
    {6, 0x08, 12}, {6, 0x03, 13}, {6, 0x34, 14}, {6, 0x35, 15}, {6, 0x2A, 16},
    {6, 0x2B, 17}, {7, 0x27, 18}, {7, 0x0C, 19}, {7, 0x08, 20}, {7, 0x17, 21},
    {7, 0x03, 22}, {7, 0x04, 23}, {7, 0x28, 24}, {7, 0x2B, 25}, {7, 0x13, 26},
    {7, 0x24, 27}, {7, 0x18, 28}, {8, 0x02, 29}, {8, 0x03, 30}, {8, 0x1A, 31},
    {8, 0x1B, 32}, {8, 0x12, 33}, {8, 0x13, 34}, {8, 0x14, 35}, {8, 0x15, 36},
    {8, 0x16, 37}, {8, 0x17, 38}, {8, 0x28, 39}, {8, 0x29, 40}, {8, 0x2A, 41},
    {8, 0x2B, 42}, {8, 0x2C, 43}, {8, 0x2D, 44}, {8, 0x04, 45}, {8, 0x05, 46},
    {8, 0x0A, 47}, {8, 0x0B, 48}, {8, 0x52, 49}, {8, 0x53, 50}, {8, 0x54, 51},
    {8, 0x55, 52}, {8, 0x24, 53}, {8, 0x25, 54}, {8, 0x58, 55}, {8, 0x59, 56},
    {8, 0x5A, 57}, {8, 0x5B, 58}, {8, 0x4A, 59}, {8, 0x4B, 60}, {8, 0x32, 61},
    {8, 0x33, 62}, {8, 0x34, 63},
    {5, 0x1B, 64}, {5, 0x12, 128}, {6, 0x17, 192}, {7, 0x37, 256}, {8, 0x36, 320},
    {8, 0x37, 384}, {8, 0x64, 448}, {8, 0x65, 512}, {8, 0x68, 576}, {8, 0x67, 640},
    {9, 0xCC, 704}, {9, 0xCD, 768}, {9, 0xD2, 832}, {9, 0xD3, 896}, {9, 0xD4, 960},
    {9, 0xD5, 1024}, {9, 0xD6, 1088}, {9, 0xD7, 1152}, {9, 0xD8, 1216},
    {9, 0xD9, 1280}, {9, 0xDA, 1344}, {9, 0xDB, 1408}, {9, 0x98, 1472},
    {9, 0x99, 1536}, {9, 0x9A, 1600}, {6, 0x18, 1664}, {9, 0x9B, 1728},
    CC_EXT
};

static const cc_cod_t cc_preto[] = {
    {10, 0x37, 0}, {3, 0x2, 1}, {2, 0x3, 2}, {2, 0x2, 3}, {3, 0x3, 4}, {4, 0x3, 5},
    {4, 0x2, 6}, {5, 0x3, 7}, {6, 0x5, 8}, {6, 0x4, 9}, {7, 0x4, 10}, {7, 0x5, 11},
    {7, 0x7, 12}, {8, 0x04, 13}, {8, 0x07, 14}, {9, 0x18, 15}, {10, 0x17, 16},
    {10, 0x18, 17}, {10, 0x08, 18}, {11, 0x67, 19}, {11, 0x68, 20}, {11, 0x6C, 21},
    {11, 0x37, 22}, {11, 0x28, 23}, {11, 0x17, 24}, {11, 0x18, 25}, {12, 0xCA, 26},
    {12, 0xCB, 27}, {12, 0xCC, 28}, {12, 0xCD, 29}, {12, 0x68, 30}, {12, 0x69, 31},
    {12, 0x6A, 32}, {12, 0x6B, 33}, {12, 0xD2, 34}, {12, 0xD3, 35}, {12, 0xD4, 36},
    {12, 0xD5, 37}, {12, 0xD6, 38}, {12, 0xD7, 39}, {12, 0x6C, 40}, {12, 0x6D, 41},
    {12, 0xDA, 42}, {12, 0xDB, 43}, {12, 0x54, 44}, {12, 0x55, 45}, {12, 0x56, 46},
    {12, 0x57, 47}, {12, 0x64, 48}, {12, 0x65, 49}, {12, 0x52, 50}, {12, 0x53, 51},
    {12, 0x24, 52}, {12, 0x37, 53}, {12, 0x38, 54}, {12, 0x27, 55}, {12, 0x28, 56},
    {12, 0x58, 57}, {12, 0x59, 58}, {12, 0x2B, 59}, {12, 0x2C, 60}, {12, 0x5A, 61},
    {12, 0x66, 62}, {12, 0x67, 63},
    {10, 0x0F, 64}, {12, 0xC8, 128}, {12, 0xC9, 192}, {12, 0x5B, 256}, {12, 0x33, 320},
    {12, 0x34, 384}, {12, 0x35, 448}, {13, 0x6C, 512}, {13, 0x6D, 576}, {13, 0x4A, 640},
    {13, 0x4B, 704}, {13, 0x4C, 768}, {13, 0x4D, 832}, {13, 0x72, 896}, {13, 0x73, 960},
    {13, 0x74, 1024}, {13, 0x75, 1088}, {13, 0x76, 1152}, {13, 0x77, 1216},
    {13, 0x52, 1280}, {13, 0x53, 1344}, {13, 0x54, 1408}, {13, 0x55, 1472},
    {13, 0x5A, 1536}, {13, 0x5B, 1600}, {13, 0x64, 1664}, {13, 0x65, 1728},
    CC_EXT
};

#define N_BRANCO ((int)(sizeof(cc_branco) / sizeof(cc_branco[0])))
#define N_PRETO  ((int)(sizeof(cc_preto) / sizeof(cc_preto[0])))

/* Un codigo de la tabla: el valor, o -1 si no hay ninguno que encaje. */
static int cc_codigo(cc_bits_t *b, const cc_cod_t *t, int n)
{
    for (int len = 2; len <= 13; len++) {
        int v = cc_ver(b, len);
        for (int i = 0; i < n; i++) {
            if (t[i].len == len && t[i].bits == v) {
                b->pos += len;
                return t[i].valor;
            }
        }
    }
    return -1;
}

/* Un tramo entero: los de relleno (64 o mas) y el final (0 a 63). */
static int cc_tramo(cc_bits_t *b, bool preto)
{
    int total = 0;
    for (int k = 0; k < 64; k++) {
        int v = preto ? cc_codigo(b, cc_preto, N_PRETO) : cc_codigo(b, cc_branco, N_BRANCO);
        if (v < 0) return -1;
        total += v;
        if (v < 64) return total;
    }
    return -1;
}

/* Los modos de dos dimensiones. */
enum { M_PASSO, M_HORIZ, M_V0, M_VR1, M_VR2, M_VR3, M_VL1, M_VL2, M_VL3, M_EOL, M_ERRO };

static int cc_modo(cc_bits_t *b)
{
    if (cc_ver(b, 1) == 1) { b->pos += 1; return M_V0; }
    int v3 = cc_ver(b, 3);
    if (v3 == 3) { b->pos += 3; return M_VR1; }
    if (v3 == 2) { b->pos += 3; return M_VL1; }
    if (v3 == 1) { b->pos += 3; return M_HORIZ; }
    if (cc_ver(b, 4) == 1) { b->pos += 4; return M_PASSO; }
    int v6 = cc_ver(b, 6);
    if (v6 == 3) { b->pos += 6; return M_VR2; }
    if (v6 == 2) { b->pos += 6; return M_VL2; }
    int v7 = cc_ver(b, 7);
    if (v7 == 3) { b->pos += 7; return M_VR3; }
    if (v7 == 2) { b->pos += 7; return M_VL3; }
    if (cc_ver(b, 12) == 1) { b->pos += 12; return M_EOL; }
    return M_ERRO;
}

/* Salta un EOL (000000000001) si lo hay delante (con ceros de relleno). */
static bool cc_pular_eol(cc_bits_t *b)
{
    int p = b->pos;
    int zeros = 0;
    while (p < b->n * 8 && zeros < 64) {
        int bit = (b->d[p >> 3] >> (7 - (p & 7))) & 1;
        p++;
        if (bit) {
            if (zeros >= 11) { b->pos = p; return true; }
            return false;
        }
        zeros++;
    }
    return false;
}

/* ---------------------------------------------------------- una fila */

/* Pone un cambio en la lista (las posiciones van creciendo). */
static void cc_poe(int *mud, int *n, int max, int x, int colunas)
{
    if (x > colunas) x = colunas;
    if (x < 0) x = 0;
    if (*n > 0 && x < mud[*n - 1]) x = mud[*n - 1];
    if (*n < max) mud[(*n)++] = x;
}

/* Una fila en una dimension: tramos blanco/negro alternos. */
static bool cc_fila_1d(cc_bits_t *b, int colunas, int *mud, int *nm, int max)
{
    int x = 0;
    bool preto = false;
    *nm = 0;
    while (x < colunas) {
        int t = cc_tramo(b, preto);
        if (t < 0) return false;
        x += t;
        cc_poe(mud, nm, max, x, colunas);
        preto = !preto;
    }
    return true;
}

/* Una fila en dos dimensiones respecto de la de arriba ('ref', con sus
 * cambios y dos centinelas de 'colunas' al final). */
static bool cc_fila_2d(cc_bits_t *b, int colunas, const int *ref, int nref,
                       int *mud, int *nm, int max)
{
    int a0 = -1;
    bool preto = false;
    *nm = 0;
    while (a0 < colunas) {
        /* b1: el primer cambio de arriba a la derecha de a0 que pasa al
         * color contrario del actual (los pares van de blanco a negro). */
        int i = 0;
        while (i < nref && (ref[i] <= a0 || (i & 1) != (preto ? 1 : 0))) i++;
        int b1 = (i < nref) ? ref[i] : colunas;
        int b2 = (i + 1 < nref) ? ref[i + 1] : colunas;
        int m = cc_modo(b);
        int a1;
        switch (m) {
        case M_PASSO:
            a0 = b2;
            break;
        case M_HORIZ: {
            int ini = a0 < 0 ? 0 : a0;
            int t1 = cc_tramo(b, preto);
            int t2 = cc_tramo(b, !preto);
            if (t1 < 0 || t2 < 0) return false;
            cc_poe(mud, nm, max, ini + t1, colunas);
            cc_poe(mud, nm, max, ini + t1 + t2, colunas);
            a0 = ini + t1 + t2;
            break;
        }
        case M_V0: case M_VR1: case M_VR2: case M_VR3:
        case M_VL1: case M_VL2: case M_VL3: {
            static const int desl[] = { 0, 0, 0, 1, 2, 3, -1, -2, -3 };
            a1 = b1 + desl[m];
            if (a1 < 0) a1 = 0;
            if (a1 > colunas) a1 = colunas;
            cc_poe(mud, nm, max, a1, colunas);
            a0 = a1;
            preto = !preto;
            break;
        }
        default:
            return false;
        }
    }
    return true;
}

/* Escribe la fila (de sus cambios) en bits: negro = 'tinta'. */
static void cc_pintar(uint8_t *linha, int colunas, const int *mud, int nm, int tinta)
{
    int bytes = (colunas + 7) / 8;
    /* Primero todo "blanco" (lo contrario de la tinta). */
    for (int i = 0; i < bytes; i++) linha[i] = tinta ? 0x00 : 0xFF;
    int x = 0;
    for (int k = 0; k < nm; k++) {
        int fim = mud[k];
        if (k & 1) {
            /* de x a fim, negro */
            for (int p = x; p < fim && p < colunas; p++) {
                if (tinta) linha[p >> 3] |= (uint8_t)(0x80 >> (p & 7));
                else linha[p >> 3] &= (uint8_t)~(0x80 >> (p & 7));
            }
        }
        x = fim;
    }
}

/* ---------------------------------------------------------- el filtro */

bool pdf_ccitt_decodificar(const uint8_t *ent, int n, const pdf_ccitt_par_t *par,
                           uint8_t **saida, int *tam)
{
    int colunas = par->colunas > 0 ? par->colunas : 1728;
    if (colunas > 65536) return false;
    int filas_max = par->filas > 0 ? par->filas : 0;
    int bytes = (colunas + 7) / 8;
    int max = colunas + 4;
    int tinta = par->preto_1 ? 1 : 0;

    int *ref = aloca((size_t)(max + 2) * sizeof(int));
    int *mud = aloca((size_t)(max + 2) * sizeof(int));
    int cap_filas = filas_max > 0 ? filas_max : 256;
    uint8_t *out = aloca((size_t)cap_filas * bytes);
    if (!ref || !mud || !out) {
        if (ref) libera(ref);
        if (mud) libera(mud);
        if (out) libera(out);
        return false;
    }
    /* La fila imaginaria de arriba de la primera: toda blanca. */
    int nref = 0;
    ref[nref++] = colunas;
    ref[nref++] = colunas;

    cc_bits_t b = { ent, n, 0 };
    int filas = 0;
    while (filas_max == 0 || filas < filas_max) {
        if (b.pos >= n * 8) break;
        if (par->alinhar) cc_alinhar(&b);
        bool dim2 = (par->k < 0);
        if (par->k >= 0) {
            /* Grupo 3: puede haber un EOL delante; con K > 0, un bit dice
             * si la fila va en una (1) o en dos (0) dimensiones. */
            cc_pular_eol(&b);
            if (par->alinhar) cc_alinhar(&b);
            if (par->k > 0) {
                int t = cc_bit(&b);
                if (t < 0) break;
                dim2 = (t == 0);
            }
        } else if (cc_ver(&b, 12) == 1) {
            /* EOFB (dos EOL seguidos): fin de los datos. */
            if (cc_ver(&b, 24) == 0x001001) break;
            b.pos += 12;
        }
        int nm = 0;
        bool ok = dim2 ? cc_fila_2d(&b, colunas, ref, nref, mud, &nm, max)
                       : cc_fila_1d(&b, colunas, mud, &nm, max);
        if (!ok) break;           /* datos rotos: se queda lo leido */
        if (filas >= cap_filas) {
            int nova = cap_filas * 2;
            uint8_t *o2 = realoca(out, (size_t)nova * bytes);
            if (!o2) break;
            out = o2;
            cap_filas = nova;
        }
        cc_pintar(out + (size_t)filas * bytes, colunas, mud, nm, tinta);
        filas++;
        /* Esta fila es la de referencia de la siguiente. */
        for (int i = 0; i < nm; i++) ref[i] = mud[i];
        nref = nm;
        ref[nref++] = colunas;
        ref[nref++] = colunas;
    }
    libera(ref);
    libera(mud);
    if (filas == 0) { libera(out); return false; }
    /* Si faltan filas (datos cortados), el resto en blanco. */
    if (filas_max > 0 && filas < filas_max) {
        for (int y = filas; y < filas_max; y++)
            for (int i = 0; i < bytes; i++) out[(size_t)y * bytes + i] = tinta ? 0x00 : 0xFF;
        filas = filas_max;
    }
    *saida = out;
    *tam = filas * bytes;
    return true;
}
