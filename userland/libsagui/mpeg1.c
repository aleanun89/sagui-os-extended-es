// SPDX-License-Identifier: GPL-2.0-or-later
/* mpeg1.c - el demultiplexador MPEG-PS y el decodificador de vídeo MPEG-1
 * (mejora 048, fase 2, D137).
 *
 * Un fichero .mpg es un "program stream" MPEG (ISO/IEC 11172-1): una serie de
 * paquetes, cada uno con un código de inicio (00 00 01 XX), que transportan el
 * flujo elemental de vídeo (MPEG-1, ISO/IEC 11172-2) y el de audio.
 *
 * El programa de vídeo (userland/libsagui/video.c) es el que habla con el
 * usuario; este módulo hace el trabajo sucio:
 *
 *  1. el demultiplexador lee los paquetes del programa y junta el flujo de vídeo;
 *  2. el decodificador lee los cuadros de ese flujo y devuelve cada uno en píxeles.
 *
 * Las normas (que llevan más de treinta años y son públicas) mandan así:
 *
 *  - la secuencia (00 00 01 B3) dice el tamaño del vídeo, los cuadros por
 *    segundo y las matrices de cuantización;
 *  - el cuadro (00 00 01 00) dice el tipo (I, P o B) y el tiempo;
 *  - cada slice (00 00 01 01..AF) trae los macrobloques: el tipo, los vectores
 *    de movimiento, el patrón de bloques y los coeficientes de la DCT (con los
 *    códigos de Huffman de la norma);
 *  - los macrobloques intra son sólo DCT; los inter son la predicción (el cuadro
 *    anterior, y en el caso de los B también el siguiente) más el residuo.
 *
 * Los cuadros B se decodifican en el orden del flujo, pero se muestran en otro:
 * por eso se guardan sus bytes hasta tener la referencia siguiente, y entonces
 * se decodifican y se entregan por el orden del tiempo.
 *
 * Todo en enteros: la libsagui se compila sin coma flotante. */
#include "mpeg1_interno.h"
#ifdef SAGUI_HOST
#include <stdio.h>
#define DEP(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEP(...)
#endif

/* El tamaño máximo de un paquete PES (el campo de longitud son 16 bits). */
#define M1_PACOTE_MAX 65536
/* Cuántos cuadros B pueden estar esperando la referencia siguiente. */
#define M1_PENDENTES_MAX 8

/* --------------------------------------------------------------- tablas */

/* Los códigos de la norma. Las tablas de Huffman del MPEG-1 (la tabla B-1 de
 * las direcciones de macrobloque, la B-2 de los tipos, la B-3 de los patrones de
 * bloques, la B-4 de los vectores de movimiento, la B-5 del DC y la B-6 de los
 * coeficientes AC) están aquí tal cual las publica la norma. */

/* Las tablas de códigos de la norma (las tablas B-1 a B-6). El índice de cada
 * tabla es el propio valor (el incremento, el patrón de bloques, el valor del
 * vector, el tamaño del DC, el tipo de macrobloque). */

static const uint8_t m1_ac_bits[113] = {
    2, 4, 5, 7, 8, 8, 10, 12, 12, 12, 12, 13,
    13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 3, 6, 8, 10, 12, 13, 13, 15,
    15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 4, 7,
    10, 12, 13, 5, 8, 12, 13, 5, 10, 12, 6, 10,
    13, 6, 12, 16, 6, 12, 7, 12, 7, 13, 8, 13,
    8, 16, 8, 16, 8, 16, 10, 16, 10, 16, 10, 16,
    12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 16, 16,
    16, 16, 16, 6, 2,
};
static const uint16_t m1_ac_codes[113] = {
    0x3, 0x4, 0x5, 0x6, 0x26, 0x21, 0xa, 0x1d, 0x18, 0x13, 0x10, 0x1a,
    0x19, 0x18, 0x17, 0x1f, 0x1e, 0x1d, 0x1c, 0x1b, 0x1a, 0x19, 0x18, 0x17,
    0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x18, 0x17, 0x16, 0x15, 0x14,
    0x13, 0x12, 0x11, 0x10, 0x3, 0x6, 0x25, 0xc, 0x1b, 0x16, 0x15, 0x1f,
    0x1e, 0x1d, 0x1c, 0x1b, 0x1a, 0x19, 0x13, 0x12, 0x11, 0x10, 0x5, 0x4,
    0xb, 0x14, 0x14, 0x7, 0x24, 0x1c, 0x13, 0x6, 0xf, 0x12, 0x7, 0x9,
    0x12, 0x5, 0x1e, 0x14, 0x4, 0x15, 0x7, 0x11, 0x5, 0x11, 0x27, 0x10,
    0x23, 0x1a, 0x22, 0x19, 0x20, 0x18, 0xe, 0x17, 0xd, 0x16, 0x8, 0x15,
    0x1f, 0x1a, 0x19, 0x17, 0x16, 0x1f, 0x1e, 0x1d, 0x1c, 0x1b, 0x1f, 0x1e,
    0x1d, 0x1c, 0x1b, 0x1, 0x2,
};
static const uint8_t m1_incr_bits[36] = {
    1, 3, 3, 4, 4, 5, 5, 7, 7, 8, 8, 8,
    8, 8, 8, 10, 10, 10, 10, 10, 10, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 8,
};
static const uint16_t m1_incr_codes[36] = {
    0x1, 0x3, 0x2, 0x3, 0x2, 0x3, 0x2, 0x7, 0x6, 0xb, 0xa, 0x9,
    0x8, 0x7, 0x6, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x23, 0x22, 0x21,
    0x20, 0x1f, 0x1e, 0x1d, 0x1c, 0x1b, 0x1a, 0x19, 0x18, 0x8, 0xf, 0x0,
};
static const uint8_t m1_pat_bits[64] = {
    9, 5, 5, 6, 4, 7, 7, 8, 4, 7, 7, 8,
    5, 8, 8, 8, 4, 7, 7, 8, 5, 8, 8, 8,
    6, 8, 8, 9, 5, 8, 8, 9, 4, 7, 7, 8,
    6, 8, 8, 9, 5, 8, 8, 8, 5, 8, 8, 9,
    5, 8, 8, 8, 5, 8, 8, 9, 5, 8, 8, 9,
    3, 5, 5, 6,
};
static const uint16_t m1_pat_codes[64] = {
    0x1, 0xb, 0x9, 0xd, 0xd, 0x17, 0x13, 0x1f, 0xc, 0x16, 0x12, 0x1e,
    0x13, 0x1b, 0x17, 0x13, 0xb, 0x15, 0x11, 0x1d, 0x11, 0x19, 0x15, 0x11,
    0xf, 0xf, 0xd, 0x3, 0xf, 0xb, 0x7, 0x7, 0xa, 0x14, 0x10, 0x1c,
    0xe, 0xe, 0xc, 0x2, 0x10, 0x18, 0x14, 0x10, 0xe, 0xa, 0x6, 0x6,
    0x12, 0x1a, 0x16, 0x12, 0xd, 0x9, 0x5, 0x5, 0xc, 0x8, 0x4, 0x4,
    0x7, 0xa, 0x8, 0xc,
};
static const uint8_t m1_mv_bits[17] = {
    1, 2, 3, 4, 6, 7, 7, 7, 9, 9, 9, 10,
    10, 10, 10, 10, 10,
};
static const uint16_t m1_mv_codes[17] = {
    0x1, 0x1, 0x1, 0x1, 0x3, 0x5, 0x4, 0x3, 0xb, 0xa, 0x9, 0x11,
    0x10, 0xf, 0xe, 0xd, 0xc,
};
static const uint8_t m1_dcl_bits[12] = {
    3, 2, 2, 3, 3, 4, 5, 6, 7, 8, 9, 9,
};
static const uint16_t m1_dcl_codes[12] = {
    0x4, 0x0, 0x1, 0x5, 0x6, 0xe, 0x1e, 0x3e, 0x7e, 0xfe, 0x1fe, 0x1ff,
};
static const uint8_t m1_dcc_bits[12] = {
    2, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10,
};
static const uint16_t m1_dcc_codes[12] = {
    0x0, 0x1, 0x2, 0x6, 0xe, 0x1e, 0x3e, 0x7e, 0xfe, 0x1fe, 0x3fe, 0x3ff,
};
static const uint8_t m1_pt_bits[7] = {
    5, 2, 3, 1, 6, 5, 5,
};
static const uint16_t m1_pt_codes[7] = {
    0x3, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2,
};
static const uint8_t m1_bt_bits[11] = {
    5, 3, 3, 4, 4, 2, 2, 6, 6, 6, 5,
};
static const uint16_t m1_bt_codes[11] = {
    0x3, 0x2, 0x3, 0x2, 0x3, 0x2, 0x3, 0x1, 0x2, 0x3, 0x2,
};
static const uint8_t m1_ac_run[111] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3,
    3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6, 7, 7, 8, 8,
    9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};
static const uint8_t m1_ac_level[111] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
    33, 34, 35, 36, 37, 38, 39, 40, 1, 2, 3, 4, 5, 6, 7, 8,
    9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 1, 2, 3, 4, 5, 1,
    2, 3, 4, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 1, 2,
    1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

/* Los últimos dos índices de la tabla AC: la fuga (escape) y el fin del bloque. */
#define M1_AC_FUGA 111
#define M1_AC_FIM  112

/* Y en la tabla de las direcciones: la fuga, el relleno y el fin del slice. */
#define M1_INCR_FUGA 33
#define M1_INCR_ENCHOF 34
#define M1_INCR_FIM  35

/* Las marcas de los tipos de macrobloque. */
#define M1_INTRA 0x01    /* só com DCT */
#define M1_QUANT 0x02    /* muda o quantizador */
#define M1_FOR   0x04    /* vetor para a frente */
#define M1_BACK  0x08    /* vetor para tras */
#define M1_PAT   0x10    /* há resíduo */
#define M1_ZERO  0x20    /* vetor para a frente, mas zero */

static const uint8_t m1_pt_val[7] = {
    M1_INTRA,
    M1_FOR | M1_PAT | M1_ZERO,
    M1_FOR,
    M1_FOR | M1_PAT,
    M1_QUANT | M1_INTRA,
    M1_QUANT | M1_FOR | M1_PAT | M1_ZERO,
    M1_QUANT | M1_FOR | M1_PAT,
};
static const uint8_t m1_bt_val[11] = {
    M1_INTRA,
    M1_BACK,
    M1_BACK | M1_PAT,
    M1_FOR,
    M1_FOR | M1_PAT,
    M1_FOR | M1_BACK,
    M1_FOR | M1_BACK | M1_PAT,
    M1_QUANT | M1_INTRA,
    M1_QUANT | M1_BACK | M1_PAT,
    M1_QUANT | M1_FOR | M1_PAT,
    M1_QUANT | M1_FOR | M1_BACK | M1_PAT,
};

/* La matriz de cuantización del luma por defecto (la norma, tabla 4). */
static const uint8_t m1_mat_intra_padrao[64] = {
     8, 16, 19, 22, 26, 27, 29, 34,
    16, 16, 22, 24, 27, 29, 34, 37,
    19, 22, 26, 27, 29, 34, 34, 38,
    22, 22, 26, 27, 29, 34, 37, 40,
    22, 26, 27, 29, 32, 35, 40, 48,
    26, 27, 29, 32, 35, 40, 48, 58,
    26, 27, 29, 34, 38, 46, 56, 69,
    27, 29, 35, 38, 46, 56, 69, 83,
};

/* --------------------------------------------------------------- bits */

/* El lector de bits: los bits del flujo, el primero el más significativo. */
typedef struct {
    const uint8_t *d;
    int n;        /* bytes */
    int pos;      /* o byte seguinte */
    int nbits;    /* os bits que estão no buffer */
    uint32_t buf; /* os bits, alinhados à esquerda (o primeiro é o bit 31) */
} m1_bits_t;

static void m1_bits_inicia(m1_bits_t *b, const uint8_t *d, int n)
{
    b->d = d; b->n = n; b->pos = 0; b->nbits = 0; b->buf = 0;
}

/* Deja en el búfer al menos 25 bits (o los que haya). */
static void m1_bits_cheio(m1_bits_t *b)
{
    while (b->nbits <= 24) {
        uint32_t byte = (b->pos < b->n) ? b->d[b->pos] : 0;
        b->pos++;
        b->buf |= byte << (24 - b->nbits);
        b->nbits += 8;
    }
}

/* n bits (n <= 24). */
static uint32_t m1_bits_pega(m1_bits_t *b, int n)
{
    if (n <= 0) return 0;
    m1_bits_cheio(b);
    uint32_t v = b->buf >> (32 - n);
    b->buf <<= n;
    b->nbits -= n;
    return v;
}

/* Mira n bits sin sacarlos. */
static uint32_t m1_bits_vista(m1_bits_t *b, int n)
{
    m1_bits_cheio(b);
    return b->buf >> (32 - n);
}

static void m1_bits_pula(m1_bits_t *b, int n) { m1_bits_pega(b, n); }

/* Cuántos bits se han leído ya. */
static int m1_bits_pos(m1_bits_t *b)
{
    return (b->pos - b->nbits / 8) * 8 - (b->nbits % 8);
}

/* Alinea en el byte siguiente y devuelve el índice de ese byte. */
static int m1_bits_alinha(m1_bits_t *b)
{
    int p = m1_bits_pos(b);
    if (p & 7) m1_bits_pega(b, 8 - (p & 7));
    return m1_bits_pos(b) >> 3;
}

/* --------------------------------------------------------------- vlc */

/* Una tabla de códigos de longitud variable. Los códigos cortos (hasta 8
 * bits) caben en una tabla directa (256 entradas); los largos, que son raros,
 * se buscan a mano. */
typedef struct {
    uint8_t  raiz[256];    /* 0 = não há código; senão o valor + 1 */
    uint8_t  compr[256];   /* o comprimento do código */
    uint8_t  lon_bits[128];
    uint16_t lon_codes[128];
    uint8_t  lon_vals[128];
    int      nlon;
} m1_vlc_t;

static void m1_vlc_prepara(m1_vlc_t *t, const uint8_t *bits, const uint16_t *codes,
                           const uint8_t *vals, int n)
{
    memset(t->raiz, 0, sizeof t->raiz);
    memset(t->compr, 0, sizeof t->compr);
    t->nlon = 0;
    for (int i = 0; i < n; i++) {
        int len = bits[i];
        uint32_t code = codes[i];
        if (len <= 8) {
            uint32_t base = code << (8 - len);
            uint32_t cnt = 1u << (8 - len);
            for (uint32_t s = 0; s < cnt; s++) {
                t->raiz[base | s] = (uint8_t)(vals[i] + 1);
                t->compr[base | s] = (uint8_t)len;
            }
        } else {
            if (t->nlon >= (int)(sizeof t->lon_bits)) break;
            t->lon_bits[t->nlon] = (uint8_t)len;
            t->lon_codes[t->nlon] = (uint16_t)code;
            t->lon_vals[t->nlon] = vals[i];
            t->nlon++;
        }
    }
}

/* Un símbolo de la tabla (-1 si el código no existe). */
static int m1_vlc(m1_bits_t *b, const m1_vlc_t *t)
{
    uint32_t v = m1_bits_vista(b, 16);
    uint8_t p = (uint8_t)(v >> 8);
    if (t->raiz[p]) {
        int len = t->compr[p];
        m1_bits_pula(b, len);
        return (int)t->raiz[p] - 1;
    }
    for (int i = 0; i < t->nlon; i++) {
        int len = t->lon_bits[i];
        if ((v >> (16 - len)) == t->lon_codes[i]) {
            m1_bits_pula(b, len);
            return t->lon_vals[i];
        }
    }
    return -1;
}

/* --------------------------------------------------------------- planos */

/* Un cuadro en YUV 4:2:0 (la Y en todo el tamaño, el croma a la mitad). Los
 * márgenes (hasta el macrobloque) se dejan al final, para que la compensación de
 * movimiento pueda leer un píxel más. */
typedef struct {
    uint8_t *y, *cb, *cr;
    int larg, alt;      /* o tamanho visível */
    int stride;         /* a largura do Y (com a margem) */
    int cstride;        /* a largura do croma */
    int calt;
} m1_plano_t;

static void m1_plano_liberta(m1_plano_t *p)
{
    if (p->y) libera(p->y);
    if (p->cb) libera(p->cb);
    if (p->cr) libera(p->cr);
    memset(p, 0, sizeof *p);
}

static bool m1_plano_cria(m1_plano_t *p, int mb_larg, int mb_alt, int larg, int alt)
{
    memset(p, 0, sizeof *p);
    p->larg = larg;
    p->alt = alt;
    p->stride = mb_larg * 16;
    p->cstride = mb_larg * 8;
    p->calt = mb_alt * 8;
    p->y = aloca_zerado((size_t)p->stride * (mb_alt * 16));
    p->cb = aloca_zerado((size_t)p->cstride * p->calt);
    p->cr = aloca_zerado((size_t)p->cstride * p->calt);
    return p->y && p->cb && p->cr;
}

/* --------------------------------------------------------------- el módulo */

/* Un cuadro B esperando la referencia siguiente. */
typedef struct {
    uint8_t *dados;     /* os bytes do quadro (cabeçalho e slices) */
    int      tam;
    int      tr;        /* o tempo (a ordem de mostra) */
    int      tipo;      /* o tipo do quadro (1, 2 ou 3) */
} m1_pendente_t;

/* El lector del archivo, con un búfer para no leer byte a byte. */
typedef struct {
    int      fd;
    uint8_t *buf;
    int      cap, n, pos;
    bool     fim;
} m1_arq_t;

struct mpeg1 {
    int fd;

    /* la secuencia */
    int      larg, alt;
    int      mb_larg, mb_alt;
    int      fps;
    uint16_t mat_intra[64];
    uint16_t mat_inter[64];
    int      f_code[2];      /* os códigos dos vetores (frente e tras) */

    /* Los cuadros.
     *
     * En el MPEG-1 el orden en el flujo es I, P, B, B (los cuadros B vienen
     * después del P que los precede en la imagen), pero el orden de muestra es
     * I, B, B, P. Por eso hay dos papeles: ref_a es la referencia más reciente
     * (la de delante, para los cuadros B y para el siguiente P) y ref_b es la
     * anterior (la de detrás, para los cuadros B). La referencia de ref_b se
     * muestra cuando llega la siguiente. */
    m1_plano_t ref_a;        /* a referência mais recente */
    m1_plano_t ref_b;        /* a referência anterior */
    m1_plano_t trabalho;     /* onde se descodifica um quadro B */
    bool       ref_a_pronta; /* ref_a já está descodificada */
    bool       ref_b_pronta; /* ref_b já está descodificada */
    int        tr_a;         /* o tempo de ref_a */
    int        tr_b;         /* o tempo de ref_b */
    int        tr_ref;       /* o tempo da referência à espera de ser mostrada */
    bool       mostrar_ref;  /* há uma referência à espera de ser mostrada */
    bool       b_mostrar;    /* há um quadro B descodificado à espera */
    m1_plano_t *a_mostrar;   /* o plano dessa referência */
    m1_pendente_t fila[M1_PENDENTES_MAX];  /* os quadros à espera, por ordem */
    int        nfila;        /* quantos há na fila */

    /* el flujo elemental acumulado */
    uint8_t *es;
    int      es_cap, es_n;
    int      es_scan;        /* onde se continua a procurar códigos */
    int      es_pic;         /* onde começa o quadro atual (-1 = nenhum) */

    /* el archivo */
    m1_arq_t arq;
    uint8_t *pacote;         /* o pacote PES que se está a ler */
    int      pacote_cap;

    /* las tablas de códigos */
    m1_vlc_t vlc_ac, vlc_incr, vlc_pat, vlc_mv, vlc_dcl, vlc_dcc, vlc_pt, vlc_bt;
    bool     tabelas_prontas;

    /* el resto */
    bool     tem_audio;      /* o programa traz audio */
    int      lidos;          /* os quadros lidos do arquivo */
};

/* --------------------------------------------------------------- archivo */

static bool m1_arq_inicia(m1_arq_t *a, int fd)
{
    a->fd = fd;
    a->cap = 8192;
    a->buf = aloca(a->cap);
    a->n = a->pos = 0;
    a->fim = false;
    return a->buf != NULL;
}

static void m1_arq_liberta(m1_arq_t *a)
{
    if (a->buf) libera(a->buf);
    memset(a, 0, sizeof *a);
}

/* Un byte (-1 al final del archivo). */
static int m1_arq_byte(m1_arq_t *a)
{
    if (a->pos >= a->n) {
        if (a->fim) return -1;
        int r = ler(a->fd, a->buf, (unsigned)a->cap);
        if (r <= 0) { a->fim = true; return -1; }
        a->n = r;
        a->pos = 0;
    }
    return a->buf[a->pos++];
}

/* n bytes (devuelve cuántos leyó). */
static int m1_arq_le(m1_arq_t *a, uint8_t *dst, int n)
{
    int lidos = 0;
    while (lidos < n) {
        int c = m1_arq_byte(a);
        if (c < 0) break;
        dst[lidos++] = (uint8_t)c;
    }
    return lidos;
}

/* Salta n bytes. */
static void m1_arq_pula(m1_arq_t *a, int n)
{
    while (n > 0) {
        int c = m1_arq_byte(a);
        if (c < 0) return;
        n--;
    }
}

/* Busca el siguiente código de inicio (00 00 01) y devuelve el byte que lo
 * sigue (-1 si se acabó el archivo). */
static int m1_arq_start(m1_arq_t *a)
{
    int zeros = 0;
    for (;;) {
        int c = m1_arq_byte(a);
        if (c < 0) return -1;
        if (c == 0) { zeros++; continue; }
        if (c == 1 && zeros >= 2) {
            zeros = 0;
            return m1_arq_byte(a);
        }
        zeros = 0;
    }
}

/* --------------------------------------------------------------- la secuencia */

/* Lee la cabecera de la secuencia (00 00 01 B3): el tamaño, los cuadros por
 * segundo y las matrices de cuantización. */
static void m1_le_sequencia(mpeg1_t *m, const uint8_t *d, int n)
{
    if (n < 8) return;
    m1_bits_t b;
    m1_bits_inicia(&b, d, n);
    int larg = (int)m1_bits_pega(&b, 12);
    int alt = (int)m1_bits_pega(&b, 12);
    m1_bits_pega(&b, 4);            /* a proporção */
    int cod_fps = (int)m1_bits_pega(&b, 4);
    m1_bits_pega(&b, 18);           /* a taxa de bits */
    m1_bits_pega(&b, 1);            /* o bit de marca */
    m1_bits_pega(&b, 10);           /* o tamanho do buffer */
    m1_bits_pega(&b, 1);            /* os parâmetros restringidos */

    if (larg <= 0 || alt <= 0) return;
    m->larg = larg;
    m->alt = alt;
    m->mb_larg = (larg + 15) / 16;
    m->mb_alt = (alt + 15) / 16;
    static const int fps_tab[16] = {
        0, 24, 24, 25, 30, 30, 50, 60, 60, 0, 0, 0, 0, 0, 0, 0
    };
    if (cod_fps > 0 && cod_fps < 16 && fps_tab[cod_fps]) m->fps = fps_tab[cod_fps];

    /* Las matrices: la del luma y la de entre macrobloques. Vienen en zigzag. */
    if (m1_bits_pega(&b, 1)) {
        for (int i = 0; i < 64; i++) {
            int v = (int)m1_bits_pega(&b, 8);
            if (!v) v = 1;
            if (i == 0) v = 8;      /* o DC do intra é sempre 8 */
            m->mat_intra[imagem_zigzag[i]] = (uint16_t)v;
        }
    } else {
        for (int i = 0; i < 64; i++) m->mat_intra[i] = m1_mat_intra_padrao[i];
    }
    if (m1_bits_pega(&b, 1)) {
        for (int i = 0; i < 64; i++) {
            int v = (int)m1_bits_pega(&b, 8);
            if (!v) v = 1;
            m->mat_inter[imagem_zigzag[i]] = (uint16_t)v;
        }
    } else {
        for (int i = 0; i < 64; i++) m->mat_inter[i] = 16;
    }
}

/* --------------------------------------------------------------- decodificar */

/* El estado de un cuadro a decodificar. */
typedef struct {
    mpeg1_t *m;
    m1_bits_t b;
    int tipo;                /* 1 = I, 2 = P, 3 = B */
    int f_code[2];           /* o código dos vetores (0 = frente, 1 = tras) */
    bool full_pel[2];
    int qscale;
    int last_dc[3];
    int last_mv[2][2];       /* [direcao][0 = x, 1 = y] */
    int mb_x, mb_y;
    int ult_dir;            /* as direcoes do ultimo macrobloco (1 = frente, 2 = tras) */
    m1_plano_t *dest;                    /* onde se escreve */
    const m1_plano_t *ref_frente;        /* a referência anterior */
    const m1_plano_t *ref_tras;          /* a referência seguinte */
} m1_dec_t;

/* El residuo de un macrobloque: los seis bloques de 8x8. */
typedef struct {
    int32_t bloco[6][64];
} m1_residuo_t;

/* Lee el vector de movimiento (la norma, 7.6.3). */
static int m1_vetor(m1_dec_t *d, int dir, int comp)
{
    int cod = m1_vlc(&d->b, &d->m->vlc_mv);
    if (cod < 0) return 0;
    if (cod == 0) {
        /* Igual que antes (el ffmpeg también dobla el valor en el modo de píxel
         * entero, pero el predictor se queda en unidades codificadas). */
        int v = d->last_mv[dir][comp];
        if (d->full_pel[dir]) v *= 2;
        return v;
    }
    int sinal = (int)m1_bits_pega(&d->b, 1);
    int fcode = d->f_code[dir];
    int val = cod;
    int shift = fcode - 1;
    if (shift > 0) {
        val = (val - 1) << shift;
        val |= (int)m1_bits_pega(&d->b, shift);
        val++;
    }
    if (sinal) val = -val;
    val += d->last_mv[dir][comp];
    int bits = 5 + shift;             /* o valor cabe em tantos bits */
    val = (val << (32 - bits)) >> (32 - bits);
    d->last_mv[dir][comp] = val;
    if (d->full_pel[dir]) val *= 2;
    return val;
}

/* Un bloque intra: el DC (con la predicción) y los coeficientes AC. */
static bool m1_bloco_intra(m1_dec_t *d, int indice, int32_t *coef)
{
    int comp = (indice <= 3) ? 0 : (indice - 3);
    int tam = m1_vlc(&d->b, comp == 0 ? &d->m->vlc_dcl : &d->m->vlc_dcc);
    if (tam < 0) return false;
    int diff = 0;
    if (tam > 0) {
        int v = (int)m1_bits_pega(&d->b, tam);
        /* El MPEG-1 codifica la diferencia del DC de una manera particular: si
         * el bit más alto es uno, la diferencia es la misma; si es cero, es
         * negativa y vale el complemento (la norma, 7.2.2.1). */
        if (v < (1 << (tam - 1))) diff = v - ((1 << tam) - 1);
        else diff = v;
    }
    d->last_dc[comp] += diff;
    memset(coef, 0, 64 * sizeof(int32_t));
    coef[0] = d->last_dc[comp] * d->m->mat_intra[0];

    int pos = 0;
    for (;;) {
        int idx = m1_vlc(&d->b, &d->m->vlc_ac);
        if (idx < 0) return false;
        if (idx == M1_AC_FIM) break;
        int run, nivel;
        if (idx == M1_AC_FUGA) {
            run = (int)m1_bits_pega(&d->b, 6) + 1;
            /* El nivel de la fuga es un valor de ocho bits con signo. El valor
             * -128 es la señal de que hay más ocho bits (la norma, 7.2.2.2). */
            int v = (int)m1_bits_pega(&d->b, 8);
            if (v >= 128) v -= 256;
            if (v == -128) v = (int)m1_bits_pega(&d->b, 8) - 256;
            else if (v == 0) v = (int)m1_bits_pega(&d->b, 8);
            nivel = v;
        } else {
            run = m1_ac_run[idx] + 1;
            nivel = m1_ac_level[idx];
            if (m1_bits_pega(&d->b, 1)) nivel = -nivel;
        }
        pos += run;
        if (pos > 63) return false;
        int j = imagem_zigzag[pos];
        int l = nivel;
        if (l < 0) l = -l;
        l = (l * d->qscale * d->m->mat_intra[j]) >> 4;
        l = (l - 1) | 1;                 /* a norma: o valor tem de ser ímpar */
        if (nivel < 0) l = -l;
        coef[j] = l;
    }
    return true;
}

/* Un bloque inter (el residuo). */
static bool m1_bloco_inter(m1_dec_t *d, int32_t *coef)
{
    memset(coef, 0, 64 * sizeof(int32_t));
    int pos = -1;

    /* El primer coeficiente tiene una forma especial: un bit de código y el
     * bit del signo (es el caso más común: el nivel 1). */
    if (m1_bits_vista(&d->b, 1)) {
        m1_bits_pula(&d->b, 1);
        int sinal = (int)m1_bits_pega(&d->b, 1);
        int l = (3 * d->qscale * d->m->mat_inter[0]) >> 5;
        l = (l - 1) | 1;
        if (sinal) l = -l;
        coef[0] = l;
        pos = 0;
    }

    for (;;) {
        int idx = m1_vlc(&d->b, &d->m->vlc_ac);
        if (idx < 0) return false;
        if (idx == M1_AC_FIM) break;
        int run, nivel;
        if (idx == M1_AC_FUGA) {
            run = (int)m1_bits_pega(&d->b, 6) + 1;
            /* El nivel de la fuga es un valor de ocho bits con signo. El valor
             * -128 es la señal de que hay más ocho bits (la norma, 7.2.2.2). */
            int v = (int)m1_bits_pega(&d->b, 8);
            if (v >= 128) v -= 256;
            if (v == -128) v = (int)m1_bits_pega(&d->b, 8) - 256;
            else if (v == 0) v = (int)m1_bits_pega(&d->b, 8);
            nivel = v;
        } else {
            run = m1_ac_run[idx] + 1;
            nivel = m1_ac_level[idx];
            if (m1_bits_pega(&d->b, 1)) nivel = -nivel;
        }
        pos += run;
        if (pos > 63) return false;
        int j = imagem_zigzag[pos];
        int l = nivel;
        bool neg = l < 0;
        if (neg) l = -l;
        l = ((l * 2 + 1) * d->qscale * d->m->mat_inter[j]) >> 5;
        l = (l - 1) | 1;
        if (neg) l = -l;
        coef[j] = l;
    }
    return true;
}

/* La compensación de movimiento: copia (o interpola a medio píxel) desde la
 * referencia. */
static void m1_mc_luma(m1_dec_t *d, const m1_plano_t *ref, int mvx, int mvy, bool media)
{
    /* El vector está en medios de píxel: la parte entera es el desplazamiento, y el
     * bit par es la mitad de píxel (la norma, 7.6.3). */
    int sx = d->mb_x * 16 + (mvx >> 1);
    int sy = d->mb_y * 16 + (mvy >> 1);
    int hx = mvx & 1, hy = mvy & 1;
    int larg = ref->larg, alt = ref->alt;

    uint8_t *dst = d->dest->y + d->mb_y * 16 * d->dest->stride + d->mb_x * 16;
    for (int y = 0; y < 16; y++) {
        uint8_t *o = dst + y * d->dest->stride;
        /* Fuera de la imagen se repite el último píxel (la norma, 7.6.3). */
        int ly = sy + y, ly1 = ly + hy;
        if (ly < 0) ly = 0;
        if (ly > alt - 1) ly = alt - 1;
        if (ly1 < 0) ly1 = 0;
        if (ly1 > alt - 1) ly1 = alt - 1;
        const uint8_t *s = ref->y + (size_t)ly * ref->stride;
        const uint8_t *s1 = ref->y + (size_t)ly1 * ref->stride;
        for (int x = 0; x < 16; x++) {
            int lx = sx + x, lx1 = lx + hx;
            if (lx < 0) lx = 0;
            if (lx > larg - 1) lx = larg - 1;
            if (lx1 < 0) lx1 = 0;
            if (lx1 > larg - 1) lx1 = larg - 1;
            int v;
            if (!hx && !hy) v = s[lx];
            else if (hx && !hy) v = (s[lx] + s[lx1] + 1) >> 1;
            else if (!hx && hy) v = (s[lx] + s1[lx] + 1) >> 1;
            else v = (s[lx] + s[lx1] + s1[lx] + s1[lx1] + 2) >> 2;
            /* En los cuadros B la predicción es la media de las dos referencias. */
            if (media) v = (o[x] + v + 1) >> 1;
            o[x] = (uint8_t)v;
        }
    }
}

/* Lo mismo, para el croma (que va a media velocidad). */
static void m1_mc_croma(m1_dec_t *d, const m1_plano_t *ref, const uint8_t *fonte,
                        int mvx, int mvy, uint8_t *plano, bool media)
{
    /* El croma va a media velocidad: el vector del luma dividido por dos. */
    int mx = mvx / 2, my = mvy / 2;
    int hx = mx & 1, hy = my & 1;
    int sx = d->mb_x * 8 + (mx >> 1);
    int sy = d->mb_y * 8 + (my >> 1);
    int larg = (ref->larg + 1) / 2, alt = (ref->alt + 1) / 2;

    uint8_t *dst = plano + d->mb_y * 8 * d->dest->cstride + d->mb_x * 8;
    for (int y = 0; y < 8; y++) {
        uint8_t *o = dst + y * d->dest->cstride;
        int ly = sy + y, ly1 = ly + hy;
        if (ly < 0) ly = 0;
        if (ly > alt - 1) ly = alt - 1;
        if (ly1 < 0) ly1 = 0;
        if (ly1 > alt - 1) ly1 = alt - 1;
        const uint8_t *s = fonte + (size_t)ly * ref->cstride;
        const uint8_t *s1 = fonte + (size_t)ly1 * ref->cstride;
        for (int x = 0; x < 8; x++) {
            int lx = sx + x, lx1 = lx + hx;
            if (lx < 0) lx = 0;
            if (lx > larg - 1) lx = larg - 1;
            if (lx1 < 0) lx1 = 0;
            if (lx1 > larg - 1) lx1 = larg - 1;
            int v;
            if (!hx && !hy) v = s[lx];
            else if (hx && !hy) v = (s[lx] + s[lx1] + 1) >> 1;
            else if (!hx && hy) v = (s[lx] + s1[lx] + 1) >> 1;
            else v = (s[lx] + s[lx1] + s1[lx] + s1[lx1] + 2) >> 2;
            if (media) v = (o[x] + v + 1) >> 1;
            o[x] = (uint8_t)v;
        }
    }
}

/* La predicción del macrobloque: desde la referencia de delante (y la de
 * detrás, en los cuadros B, en que se hace la media de las dos). */
static void m1_predicao(m1_dec_t *d, const int mv_f[2], const int mv_b[2],
                        bool tem_f, bool tem_b)
{
    /* La media con la referencia de detrás sólo ocurre cuando hay también
     * predicción de delante: el ffmpeg escribe la primera y luego promedia
     * con la segunda (la norma, 7.6.7). */
    bool media = tem_f && d->ref_frente != NULL;
    if (tem_f && d->ref_frente) {
        m1_mc_luma(d, d->ref_frente, mv_f[0], mv_f[1], false);
        m1_mc_croma(d, d->ref_frente, d->ref_frente->cb, mv_f[0], mv_f[1], d->dest->cb, false);
        m1_mc_croma(d, d->ref_frente, d->ref_frente->cr, mv_f[0], mv_f[1], d->dest->cr, false);
    }
    if (tem_b && d->ref_tras) {
        m1_mc_luma(d, d->ref_tras, mv_b[0], mv_b[1], media);
        m1_mc_croma(d, d->ref_tras, d->ref_tras->cb, mv_b[0], mv_b[1], d->dest->cb, media);
        m1_mc_croma(d, d->ref_tras, d->ref_tras->cr, mv_b[0], mv_b[1], d->dest->cr, media);
    }
}

/* Escribe los bloques de un macrobloque (la IDCT y, si es inter, la suma del
 * residuo). */
static void m1_escreve_blocos(m1_dec_t *d, const m1_residuo_t *res, bool intra)
{
    int32_t pix[64];
    for (int i = 0; i < 6; i++) {
        imagem_idct(res->bloco[i], pix, 8);
        uint8_t *o;
        int passo;
        if (i < 4) {
            o = d->dest->y + (d->mb_y * 16 + (i / 2) * 8) * d->dest->stride +
                d->mb_x * 16 + (i % 2) * 8;
            passo = d->dest->stride;
        } else {
            o = (i == 4 ? d->dest->cb : d->dest->cr) +
                d->mb_y * 8 * d->dest->cstride + d->mb_x * 8;
            passo = d->dest->cstride;
        }
        int tam = (i < 4) ? 8 : 8;
        for (int y = 0; y < tam; y++) {
            for (int x = 0; x < tam; x++) {
                int v = pix[y * 8 + x];
                if (!intra) v += o[y * passo + x];
                if (v < 0) v = 0; else if (v > 255) v = 255;
                o[y * passo + x] = (uint8_t)v;
            }
        }
    }
}

/* Decodifica un macrobloque. */
static bool m1_macrobloco(m1_dec_t *d)
{
    int tipo;
    if (d->tipo == 1) {
        /* En un cuadro I: "1" es intra y "01" es intra con cambio de cuantizador. */
        if (m1_bits_pega(&d->b, 1) == 0) {
            if (m1_bits_pega(&d->b, 1) == 0) return false;
            tipo = M1_INTRA | M1_QUANT;
        } else tipo = M1_INTRA;
    } else if (d->tipo == 2) {
        int v = m1_vlc(&d->b, &d->m->vlc_pt);
        if (v < 0) return false;
        tipo = m1_pt_val[v];
    } else {
        int v = m1_vlc(&d->b, &d->m->vlc_bt);
        if (v < 0) return false;
        tipo = m1_bt_val[v];
    }

    m1_residuo_t res;
    if (tipo & M1_INTRA) {
        /* Los vectores de movimiento a cero y el DC como está. */
        d->last_mv[0][0] = d->last_mv[0][1] = 0;
        d->last_mv[1][0] = d->last_mv[1][1] = 0;
        if (tipo & M1_QUANT) {
            int q = (int)m1_bits_pega(&d->b, 5);
            if (q > 31) return false;
            d->qscale = q << 1;
        }
        for (int i = 0; i < 6; i++)
            if (!m1_bloco_intra(d, i, res.bloco[i])) return false;
        m1_escreve_blocos(d, &res, true);
        return true;
    }

    /* En un macrobloque inter el DC vuelve a 128 (la predicción del DC sólo se
     * usa en los macrobloques intra). */
    d->last_dc[0] = d->last_dc[1] = d->last_dc[2] = 128;
    if (tipo & M1_QUANT) {
        int q = (int)m1_bits_pega(&d->b, 5);
        if (q > 31) return false;
        d->qscale = q << 1;
    }

    int mv_f[2] = { 0, 0 }, mv_b[2] = { 0, 0 };
    bool tem_f = (tipo & M1_FOR) != 0;
    bool tem_b = (tipo & M1_BACK) != 0;
    d->ult_dir = (tem_f ? 1 : 0) | (tem_b ? 2 : 0);
    if (tipo & M1_ZERO) {
        d->last_mv[0][0] = d->last_mv[0][1] = 0;
        mv_f[0] = mv_f[1] = 0;
        tem_f = true;
        tem_b = false;
    } else {
        if (tem_f) {
            mv_f[0] = m1_vetor(d, 0, 0);
            mv_f[1] = m1_vetor(d, 0, 1);
        }
        if (tem_b) {
            mv_b[0] = m1_vetor(d, 1, 0);
            mv_b[1] = m1_vetor(d, 1, 1);
        }
    }

    int cbp = 0;
    if (tipo & M1_PAT) {
        cbp = m1_vlc(&d->b, &d->m->vlc_pat);
        if (cbp < 0) return false;
    }

    /* La predicción y el residuo. */
    m1_predicao(d, mv_f, mv_b, tem_f, tem_b);
    memset(&res, 0, sizeof res);
    /* El patrón de bloques codificados tiene un bit por bloque: los cuatro del
     * luma y los dos del croma (la norma, 7.2.2.3). */
    for (int i = 0; i < 6; i++) {
        if (cbp & 32) {
            if (!m1_bloco_inter(d, res.bloco[i])) return false;
        }
        cbp += cbp;
    }
    m1_escreve_blocos(d, &res, false);
    return true;
}

/* Un macrobloque saltado (sin datos): se queda la predicción, el residuo es cero. */
static void m1_macrobloco_saltado(m1_dec_t *d)
{
    /* Un macrobloque saltado y previsto con el vector del macrobloque anterior
     * (la norma, 7.6.3). Con el modo de píxel entero el vector es el doble. */
    int ef = d->full_pel[0] ? 2 : 1;
    int eb = d->full_pel[1] ? 2 : 1;
    if (d->tipo == 2) {
        d->last_mv[0][0] = d->last_mv[0][1] = 0;
        int mv_f[2] = { 0, 0 };
        m1_predicao(d, mv_f, NULL, true, false);
    } else {
        /* En los cuadros B el macrobloque saltado hereda también la dirección del
         * macrobloque anterior (la norma, 7.6.6: el ffmpeg también). */
        int mv_f[2] = { d->last_mv[0][0] * ef, d->last_mv[0][1] * ef };
        int mv_b[2] = { d->last_mv[1][0] * eb, d->last_mv[1][1] * eb };
        bool tem_f = (d->ult_dir & 1) != 0;
        bool tem_b = (d->ult_dir & 2) != 0;
        if (!tem_f && !tem_b) { tem_f = true; tem_b = true; }
        m1_predicao(d, mv_f, mv_b, tem_f, tem_b);
    }
}

/* Decodifica un slice. En el MPEG-1 la posición vertical viene del propio
 * código de inicio (00 00 01 01..AF): el código menos uno es la línea del
 * macrobloque. La cabecera del slice es sólo el cuantizador y los bits extra. */
static void m1_slice(m1_dec_t *d, const uint8_t *dados, int n, int mb_y)
{
    m1_bits_inicia(&d->b, dados, n);
    d->qscale = (int)m1_bits_pega(&d->b, 5);
    d->qscale <<= 1;
    /* El MPEG-1 dobla el cuantizador (el código de 5 bits es un valor de 1 a 31
     * que la norma ya usa multiplicado por dos). */
    /* Los bits extra: mientras el bit sea uno, se leen más ocho. El bit que
     * acaba el ciclo también cuenta (la norma, 6.2.5). */
    while (m1_bits_pega(&d->b, 1)) m1_bits_pega(&d->b, 8);

    /* Al final de cada slice los predictores vuelven al principio (la norma, 7.4.3
     * y 7.6.3: el DC y los vectores de movimiento se reinician). */
    d->last_dc[0] = d->last_dc[1] = d->last_dc[2] = 128;
    d->last_mv[0][0] = d->last_mv[0][1] = 0;
    d->last_mv[1][0] = d->last_mv[1][1] = 0;

    d->mb_y = mb_y;

    /* El primer macrobloque del slice: el incremento da la columna donde
     * empieza (la norma, 7.2.2.3). No se salta ningún macrobloque. */
    d->mb_x = 0;
    for (;;) {
        if (m1_bits_pos(&d->b) >= n * 8) return;
        int c = m1_vlc(&d->b, &d->m->vlc_incr);
        if (c < 0) return;
        if (c == M1_INCR_FUGA) { d->mb_x += 33; continue; }
        if (c == M1_INCR_ENCHOF || c == M1_INCR_FIM) continue;
        d->mb_x += c;
        break;
    }

    /* El primer macrobloque del slice. */
    if (!m1_macrobloco(d)) return;
    d->mb_x++;
    if (d->mb_x >= d->m->mb_larg) { d->mb_x = 0; d->mb_y++; }

    /* A partir de aquí cada macrobloque va precedido del incremento de la
     * dirección, que dice cuántos se saltan. */
    int pulo = -1;
    while (d->mb_y < d->m->mb_alt) {
        if (pulo < 0) {
            pulo = 0;
            for (;;) {
                /* Los datos del slice se acabaron: se acabó el slice. */
                if (m1_bits_pos(&d->b) >= n * 8) return;
                int c = m1_vlc(&d->b, &d->m->vlc_incr);
                if (c < 0) return;
                if (c == M1_INCR_FUGA) { pulo += 33; continue; }
                /* El relleno y el fin del slice no paran la decodificación: una
                 * secuencia de ceros dentro de los datos puede parecer un fin
                 * de slice. El slice acaba cuando se acaban los datos. */
                if (c == M1_INCR_ENCHOF || c == M1_INCR_FIM) continue;
                pulo += c;
                break;
            }
        }
        if (pulo > 0) {
            pulo--;
            m1_macrobloco_saltado(d);
        } else {
            if (!m1_macrobloco(d)) return;
            pulo = -1;
        }
        d->mb_x++;
        if (d->mb_x >= d->m->mb_larg) { d->mb_x = 0; d->mb_y++; }
    }
}

/* Decodifica un cuadro entero (a partir de sus bytes). */
static bool m1_descodifica(mpeg1_t *m, int tipo, const uint8_t *d, int n,
                            m1_plano_t *dest, const m1_plano_t *ref_frente,
                            const m1_plano_t *ref_tras)
{
    if (n < 8) return false;
    m1_dec_t d0;
    memset(&d0, 0, sizeof d0);
    d0.m = m;
    d0.tipo = tipo;
    d0.dest = dest;
    d0.ref_frente = ref_frente;
    d0.ref_tras = ref_tras;
    d0.f_code[0] = 1;
    d0.f_code[1] = 1;
    d0.last_dc[0] = d0.last_dc[1] = d0.last_dc[2] = 128;

    /* La cabecera del cuadro (la norma, 6.2.4). */
    m1_bits_inicia(&d0.b, d + 4, n - 4);
    m1_bits_pega(&d0.b, 10);              /* o tempo */
    int tipo2 = (int)m1_bits_pega(&d0.b, 3);
    m1_bits_pega(&d0.b, 16);              /* o atraso do buffer */
    if (tipo2 == 2 || tipo2 == 3) {
        d0.full_pel[0] = m1_bits_pega(&d0.b, 1) != 0;
        int f = (int)m1_bits_pega(&d0.b, 3);
        d0.f_code[0] = f ? f : 1;
    }
    if (tipo2 == 3) {
        d0.full_pel[1] = m1_bits_pega(&d0.b, 1) != 0;
        int f = (int)m1_bits_pega(&d0.b, 3);
        d0.f_code[1] = f ? f : 1;
    }
    while (m1_bits_pega(&d0.b, 1)) m1_bits_pega(&d0.b, 8);   /* os bits extra */
    if (tipo2 != tipo) return false;

    /* Los slices: cada uno empieza con su código de inicio. Después de la
     * cabecera del cuadro el flujo está alineado en el byte (el codificador lo
     * completa con ceros), pero se busca el código de inicio por seguridad. */
    int p = 4 + m1_bits_alinha(&d0.b);
    while (p + 4 <= n) {
        if (!(d[p] == 0 && d[p + 1] == 0 && d[p + 2] == 1)) {
            /* No es un código de inicio: se busca el siguiente. */
            p++;
            continue;
        }
        int id = d[p + 3];
        if (id < 1 || id > 0xAF) break;      /* não é um slice: fim do quadro */
        /* El slice va hasta el próximo código de inicio. */
        int fim = p + 4;
        while (fim + 3 <= n && !(d[fim] == 0 && d[fim + 1] == 0 && d[fim + 2] == 1)) fim++;
        if (fim + 3 > n) fim = n;   /* o último slice: vai até ao fim do quadro */
        m1_slice(&d0, d + p + 4, fim - p - 4, id - 1);
        p = fim;
    }
    return true;
}

/* --------------------------------------------------------------- el flujo */

/* Termina un cuadro (declarado aquí porque el flujo lo llama). */
static void m1_fim_quadro(mpeg1_t *m, int ini, int fim);

/* Junta más bytes al flujo elemental y busca los cuadros. */
static void m1_acumula(mpeg1_t *m, const uint8_t *d, int n)
{
    if (m->es_n + n > m->es_cap) {
        int nova = m->es_cap ? m->es_cap * 2 : 65536;
        while (nova < m->es_n + n) nova *= 2;
        uint8_t *p = realoca(m->es, (size_t)nova);
        if (!p) return;
        m->es = p;
        m->es_cap = nova;
    }
    memcpy(m->es + m->es_n, d, (size_t)n);
    m->es_n += n;

    /* Busca los códigos de inicio. */
    int p = m->es_scan;
    while (p + 3 < m->es_n) {
        if (m->es[p] == 0 && m->es[p + 1] == 0 && m->es[p + 2] == 1) {
            int id = m->es[p + 3];
            if (id == 0x00) {
                /* Un cuadro nuevo: el anterior acaba aquí. */
                if (m->es_pic >= 0 && p > m->es_pic) {
                    m1_fim_quadro(m, m->es_pic, p);
                }
                m->es_pic = p;
                p += 4;
                m->es_scan = p;
                continue;
            }
            if (id == 0xB3) {
                /* La cabecera de la secuencia (8 bytes y hasta dos matrices de 64): si llega partida entre dos paquetes del PS se
                 * espera a tenerla entera (antes se leía a medias y se perdían las matrices o el tamaño). */
                if (m->es_n - p - 4 < 8 + 128) { m->es_scan = p; return; }
                m1_le_sequencia(m, m->es + p + 4, m->es_n - p - 4);
                p += 4;
                m->es_scan = p;
                continue;
            }
            p += 4;
            m->es_scan = p;
            continue;
        }
        p++;
    }
    m->es_scan = p;

    /* Tira lo que ya se usó (el cuadro actual se queda al principio). */
    if (m->es_pic > 0) {
        int resto = m->es_n - m->es_pic;
        if (resto > 0) memmove(m->es, m->es + m->es_pic, (size_t)resto);
        m->es_n = resto;
        m->es_scan -= m->es_pic;
        m->es_pic = 0;
    }
}



/* Termina un cuadro: o es un B (se queda esperando la referencia siguiente) o
 * es una referencia (se decodifica ya). */
static void m1_fim_quadro(mpeg1_t *m, int ini, int fim)
{
    const uint8_t *d = m->es + ini;
    int n = fim - ini;
    if (n < 8) return;

    /* La cabecera del cuadro: el tipo y el tiempo. */
    m1_bits_t b;
    m1_bits_inicia(&b, d + 4, n - 4);
    int tr = (int)m1_bits_pega(&b, 10);
    int tipo = (int)m1_bits_pega(&b, 3);
    if (tipo < 1 || tipo > 3) return;

    /* El cuadro va a la cola, por orden de llegada en el flujo. El orden de
     * muestra es otro: en el MPEG-1 el flujo trae I, P, B, B y la imagen es
     * I, B, B, P. Por eso la cola se procesa con calma (véase mpeg1_quadro). */
    if (m->nfila >= M1_PENDENTES_MAX) {
        /* No hay espacio: se tira el más antiguo (y mejor un cuadro menos que
         * atascarse). */
        libera(m->fila[0].dados);
        for (int i = 1; i < m->nfila; i++) m->fila[i - 1] = m->fila[i];
        m->nfila--;
    }
    uint8_t *copia = aloca((size_t)n);
    if (!copia) return;
    memcpy(copia, d, (size_t)n);
    m->fila[m->nfila].dados = copia;
    m->fila[m->nfila].tam = n;
    m->fila[m->nfila].tr = tr;
    m->fila[m->nfila].tipo = tipo;
    m->nfila++;

}

/* --------------------------------------------------------------- el demultiplexador */

/* Lee un paquete PES de vídeo y junta su carga al flujo. */
static void m1_pacote_video(mpeg1_t *m, int len)
{
    int cab = 0;
    int c;
    /* El relleno: 0xFF hasta el primer byte que no lo es. */
    do {
        c = m1_arq_byte(&m->arq);
        if (c < 0) return;
        cab++;
    } while (c == 0xFF);

    if ((c & 0xC0) == 0x40) {
        /* La escala y el tamaño del búfer. */
        m1_arq_byte(&m->arq);
        m1_arq_byte(&m->arq);
        cab += 2;
        c = m1_arq_byte(&m->arq);
        cab++;
        if (c < 0) return;
    }
    if ((c & 0xE0) == 0x20) {
        /* El tiempo de presentación (4 bytes) y, a veces, el de decodificación. */
        m1_arq_pula(&m->arq, 4);
        cab += 4;
        if (c & 0x10) {
            m1_arq_pula(&m->arq, 5);
            cab += 5;
        }
    } else if ((c & 0xC0) == 0x80) {
        /* La cabecera del MPEG-2: dos bytes y la longitud. */
        int flags = m1_arq_byte(&m->arq);
        int tam_cab = m1_arq_byte(&m->arq);
        cab += 2;
        if (tam_cab < 0) return;
        m1_arq_pula(&m->arq, tam_cab);
        cab += tam_cab;
        (void)flags;
    } else if (c != 0x0F) {
        return;
    }

    int carga = len - cab;
    if (carga <= 0) return;
    if (carga > m->pacote_cap) {
        int nova = carga * 2;
        uint8_t *p = realoca(m->pacote, (size_t)nova);
        if (!p) return;
        m->pacote = p;
        m->pacote_cap = nova;
    }
    int lidos = m1_arq_le(&m->arq, m->pacote, carga);
    if (lidos > 0) m1_acumula(m, m->pacote, lidos);
}

/* Cierra el último cuadro del flujo: sólo aparece un nuevo código de inicio de
 * cuadro cuando viene otro cuadro, y el último se queda sin cerrar. */
static bool m1_fecha_quadro(mpeg1_t *m)
{
    if (m->es_pic >= 0 && m->es_n > m->es_pic) {
        m1_fim_quadro(m, m->es_pic, m->es_n);
        m->es_pic = -1;
        return true;
    }
    return false;
}

/* Lee más del archivo: devuelve false al final. */
static bool m1_le_mais(mpeg1_t *m)
{
    for (;;) {
        int id = m1_arq_start(&m->arq);
        if (id < 0) return m1_fecha_quadro(m);
        if (id == 0xB9) return m1_fecha_quadro(m);   /* o fim do programa */
        if (id == 0xBA) {
            /* La cabecera del paquete: 12 bytes en el MPEG-1, 14 en el MPEG-2 (y
             * luego el relleno, que son 0xFF). */
            int b = m1_arq_byte(&m->arq);
            if (b < 0) return false;
            if ((b & 0xC0) == 0x40) {
                /* MPEG-2: 10 bytes fijos después del código (SCR, velocidad) y, en los 3 bits bajos del último, cuántos bytes
                 * de relleno siguen. Antes se leía hasta el primer byte que no fuese 0xFF: sin relleno eso se comía el primer 0x00
                 * del código siguiente y se perdía el paquete (con ello, la mayor parte de los cuadros del MPEG-PS de MPEG-2). */
                m1_arq_pula(&m->arq, 8);
                int ult = m1_arq_byte(&m->arq);
                if (ult < 0) return false;
                m1_arq_pula(&m->arq, ult & 7);
            } else {
                /* MPEG-1: 12 bytes en total (8 después del código). */
                m1_arq_pula(&m->arq, 7);
            }
            continue;
        }
        if (id == 0xBB || id == 0xB8 || id == 0xB2) {
            /* La cabecera del sistema, del grupo o del usuario: tiene
             * longitud. */
            int lo = m1_arq_byte(&m->arq);
            int hi = m1_arq_byte(&m->arq);
            if (lo < 0 || hi < 0) return false;
            m1_arq_pula(&m->arq, (lo << 8) | hi);
            continue;
        }
        if ((id & 0xF0) == 0xE0) {
            int lo = m1_arq_byte(&m->arq);
            int hi = m1_arq_byte(&m->arq);
            if (lo < 0 || hi < 0) return false;
            m1_pacote_video(m, (lo << 8) | hi);
            /* Si ya hay cuadros listos (un B con referencia de detrás o una
             * referencia decodificada), basta: quien llama va a buscarlos. */
            if (m->mostrar_ref || m->b_mostrar || m->nfila > 0) return true;
            continue;
        }
        if ((id & 0xE0) == 0xC0) {
            /* El audio: por ahora sólo se cuenta que existe. */
            int lo = m1_arq_byte(&m->arq);
            int hi = m1_arq_byte(&m->arq);
            if (lo < 0 || hi < 0) return false;
            m->tem_audio = true;
            m1_arq_pula(&m->arq, (lo << 8) | hi);
            continue;
        }
        /* Cualquier otra cosa: se busca el siguiente código. */
    }
}

/* --------------------------------------------------------------- la API */

/* Prepara las tablas de códigos. */
static bool m1_prepara_tabelas(mpeg1_t *m)
{
    if (m->tabelas_prontas) return true;
    static uint8_t ids[113], incr[36], pat[64], mv[17], dcl[12], dcc[12], pt[7], bt[11];
    for (int i = 0; i < 113; i++) ids[i] = (uint8_t)i;
    for (int i = 0; i < 36; i++) incr[i] = (uint8_t)i;
    for (int i = 0; i < 64; i++) pat[i] = (uint8_t)i;
    for (int i = 0; i < 17; i++) mv[i] = (uint8_t)i;
    for (int i = 0; i < 12; i++) { dcl[i] = (uint8_t)i; dcc[i] = (uint8_t)i; }
    for (int i = 0; i < 7; i++) pt[i] = (uint8_t)i;
    for (int i = 0; i < 11; i++) bt[i] = (uint8_t)i;

    m1_vlc_prepara(&m->vlc_ac, m1_ac_bits, m1_ac_codes, ids, 113);
    m1_vlc_prepara(&m->vlc_incr, m1_incr_bits, m1_incr_codes, incr, 36);
    m1_vlc_prepara(&m->vlc_pat, m1_pat_bits, m1_pat_codes, pat, 64);
    m1_vlc_prepara(&m->vlc_mv, m1_mv_bits, m1_mv_codes, mv, 17);
    m1_vlc_prepara(&m->vlc_dcl, m1_dcl_bits, m1_dcl_codes, dcl, 12);
    m1_vlc_prepara(&m->vlc_dcc, m1_dcc_bits, m1_dcc_codes, dcc, 12);
    m1_vlc_prepara(&m->vlc_pt, m1_pt_bits, m1_pt_codes, pt, 7);
    m1_vlc_prepara(&m->vlc_bt, m1_bt_bits, m1_bt_codes, bt, 11);
    m->tabelas_prontas = true;
    return true;
}

/* Recorre el archivo buscando los cuadros (para saber cuántos hay). */
static int m1_prescan(mpeg1_t *m)
{
    m1_arq_t arq;
    if (!m1_arq_inicia(&arq, m->fd)) return 0;
    int quadros = 0;
    uint8_t *junta = NULL;
    int junta_n = 0, junta_cap = 0;
    for (;;) {
        int id = m1_arq_start(&arq);
        if (id < 0) break;
        if (id == 0xB9) break;
        if (id == 0xBA) {
            int b = m1_arq_byte(&arq);
            if (b < 0) break;
            if ((b & 0xC0) == 0x40) {
                m1_arq_pula(&arq, 8);
                int ult = m1_arq_byte(&arq);
                if (ult < 0) goto fim;
                m1_arq_pula(&arq, ult & 7);
            } else m1_arq_pula(&arq, 7);
            continue;
        }
        if (id == 0xBB || id == 0xB8 || id == 0xB2) {
            int lo = m1_arq_byte(&arq);
            int hi = m1_arq_byte(&arq);
            if (lo < 0 || hi < 0) break;
            m1_arq_pula(&arq, (lo << 8) | hi);
            continue;
        }
        if ((id & 0xF0) == 0xE0) {
            int lo = m1_arq_byte(&arq);
            int hi = m1_arq_byte(&arq);
            if (lo < 0 || hi < 0) break;
            int len = (lo << 8) | hi;
            /* La cabecera del PES y la carga: se cuentan los códigos de inicio. */
            int cab = 0, c;
            do {
                c = m1_arq_byte(&arq);
                if (c < 0) goto fim;
                cab++;
            } while (c == 0xFF);
            if ((c & 0xC0) == 0x40) {
                m1_arq_byte(&arq); m1_arq_byte(&arq);
                cab += 2;
                c = m1_arq_byte(&arq);
                cab++;
                if (c < 0) goto fim;
            }
            if ((c & 0xE0) == 0x20) {
                m1_arq_pula(&arq, 4);
                cab += 4;
                if (c & 0x10) { m1_arq_pula(&arq, 5); cab += 5; }
            } else if ((c & 0xC0) == 0x80) {
                m1_arq_byte(&arq);
                int tam = m1_arq_byte(&arq);
                cab += 2;
                if (tam < 0) goto fim;
                m1_arq_pula(&arq, tam);
                cab += tam;
            } else if (c != 0x0F) goto fim;
            int carga = len - cab;
            if (carga <= 0) continue;
            if (carga > m->pacote_cap) {
                uint8_t *p = realoca(m->pacote, (size_t)carga);
                if (!p) goto fim;
                m->pacote = p;
                m->pacote_cap = carga;
            }
            int lidos = m1_arq_le(&arq, m->pacote, carga);
            /* Se cuentan los códigos sobre la cola del paquete anterior más este: un código o una cabecera de secuencia pueden
             * quedar partidos entre dos paquetes. */
            if (junta_n + lidos > junta_cap) {
                int nc = junta_n + lidos + 256;
                uint8_t *nj = realoca(junta, (size_t)nc);
                if (!nj) goto fim;
                junta = nj; junta_cap = nc;
            }
            memcpy(junta + junta_n, m->pacote, (size_t)lidos);
            junta_n += lidos;
            int i = 0;
            for (; i + 3 < junta_n; i++) {
                if (junta[i] == 0 && junta[i + 1] == 0 && junta[i + 2] == 1) {
                    int c2 = junta[i + 3];
                    if (c2 == 0x00) quadros++;
                    else if (c2 == 0xB3) {
                        if (junta_n - i - 4 < 8 + 128) break;   /* la cabecera sigue en el paquete siguiente */
                        m1_le_sequencia(m, junta + i + 4, junta_n - i - 4);
                        i += 3;
                    }
                }
            }
            /* se queda lo que aún no se pudo juzgar: desde i, o los 3 últimos bytes */
            if (junta_n - i > 0) memmove(junta, junta + i, (size_t)(junta_n - i));
            junta_n -= i;
            continue;
        }
        if ((id & 0xE0) == 0xC0) {
            int lo = m1_arq_byte(&arq);
            int hi = m1_arq_byte(&arq);
            if (lo < 0 || hi < 0) break;
            m->tem_audio = true;
            m1_arq_pula(&arq, (lo << 8) | hi);
            continue;
        }
    }
fim:
    /* lo que quedó sin juzgar al final: la última cabecera de secuencia, aunque corta */
    for (int i = 0; i + 3 < junta_n; i++)
        if (junta[i] == 0 && junta[i + 1] == 0 && junta[i + 2] == 1) {
            if (junta[i + 3] == 0x00) quadros++;
            else if (junta[i + 3] == 0xB3) m1_le_sequencia(m, junta + i + 4, junta_n - i - 4);
        }
    libera(junta);
    m1_arq_liberta(&arq);
    return quadros;
}

mpeg1_t *mpeg1_abrir(const char *caminho, mpeg1_info_t *info, int *erro)
{
    int e = 0;
    if (erro) *erro = 0;
    mpeg1_t *m = aloca_zerado(sizeof(mpeg1_t));
    if (!m) { if (erro) *erro = VIDEO_ERRO_MEMORIA; return NULL; }
    m->fd = abrir(caminho, ABRIR_LER);
    if (m->fd < 0) {
        e = m->fd;
        libera(m);
        if (erro) *erro = e;
        return NULL;
    }
    m->fps = 30;
    m->es_pic = -1;          /* ainda não começou nenhum quadro */
    m1_prepara_tabelas(m);
    if (!m1_arq_inicia(&m->arq, m->fd)) {
        e = VIDEO_ERRO_MEMORIA;
        goto falha;
    }
    m1_arq_liberta(&m->arq);

    /* La primera vuelta: cuántos cuadros hay y de qué tamaño. */
    int quadros = m1_prescan(m);
    if (quadros <= 0 || m->larg <= 0) {
        e = VIDEO_ERRO_FORMATO;
        goto falha;
    }
    if (!m1_plano_cria(&m->ref_a, m->mb_larg, m->mb_alt, m->larg, m->alt) ||
        !m1_plano_cria(&m->ref_b, m->mb_larg, m->mb_alt, m->larg, m->alt) ||
        !m1_plano_cria(&m->trabalho, m->mb_larg, m->mb_alt, m->larg, m->alt)) {
        e = VIDEO_ERRO_MEMORIA;
        goto falha;
    }
    if (posicionar(m->fd, 0, POS_INICIO) < 0) { e = VIDEO_ERRO_CORROMPIDO; goto falha; }
    if (!m1_arq_inicia(&m->arq, m->fd)) { e = VIDEO_ERRO_MEMORIA; goto falha; }

    if (info) {
        info->larg = m->larg;
        info->alt = m->alt;
        info->fps = m->fps;
        info->quadros = quadros;
        info->tem_audio = m->tem_audio;
        info->taxa_audio = 0;
        info->canais_audio = 0;
    }
    return m;

falha:
    mpeg1_fechar(m);
    if (erro) *erro = e;
    return NULL;
}

/* Convierte YCbCr en RGB con el alcance limitado (el del "estudio"), que es el
 * que usa el MPEG-1: la luma va de 16 a 235 y el croma de 16 a 240. La matriz es
 * la de la BT.601, la misma que usa el ffmpeg para el MPEG-1. Las constantes son
 * el valor real por 65536. */
#define M1_CR 104597        /* 1.596027 */
#define M1_CB 132201        /* 2.017232 */
#define M1_CBG 25681        /* 0.391762 */
#define M1_CRG 53282        /* 0.812968 */
#define M1_Y 76310          /* 1.164383 */

static uint32_t m1_ycbcr(int32_t y, int32_t cb, int32_t cr)
{
    int32_t cb2 = cb - 128, cr2 = cr - 128;
    int32_t yy = ((y - 16) * M1_Y + 32768) >> 16;
    int32_t r = yy + ((cr2 * M1_CR + 32768) >> 16);
    int32_t g = yy - ((cb2 * M1_CBG + cr2 * M1_CRG + 32768) >> 16);
    int32_t b = yy + ((cb2 * M1_CB + 32768) >> 16);
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    return (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
}

/* Convierte el cuadro de YUV a RGB (el croma se repite a medio píxel, como
 * hace el ffmpeg en el MPEG-1). */
static bool m1_para_rgb(mpeg1_t *m, const m1_plano_t *q, imagem_t *img)
{
    if (!imagem_criar(img, m->larg, m->alt, 0)) return false;
    for (int y = 0; y < m->alt; y++) {
        uint32_t *o = img->px + (size_t)y * img->larg;
        const uint8_t *ly = q->y + y * q->stride;
        const uint8_t *lcb = q->cb + (y / 2) * q->cstride;
        const uint8_t *lcr = q->cr + (y / 2) * q->cstride;
        for (int x = 0; x < m->larg; x++)
            o[x] = m1_ycbcr(ly[x], lcb[x / 2], lcr[x / 2]);
    }
    return true;
}

/* Rellena un plano con el color gris: es la referencia de detrás que se usa
 * cuando se acaba el archivo y no hay más cuadros (es lo que hacen los
 * decodificadores). */
static void m1_plano_cinza(m1_plano_t *p)
{
    if (p->y) memset(p->y, 128, (size_t)p->stride * (p->calt * 2));
    if (p->cb) memset(p->cb, 128, (size_t)p->cstride * p->calt);
    if (p->cr) memset(p->cr, 128, (size_t)p->cstride * p->calt);
}

int mpeg1_quadro(mpeg1_t *m, imagem_t *img)
{
    if (!m || !img) return VIDEO_ERRO_FORMATO;
    for (;;) {
        /* Un cuadro B ya decodificado: se muestra. */
        if (m->b_mostrar) {
            m->b_mostrar = false;
            if (!m1_para_rgb(m, &m->trabalho, img)) return VIDEO_ERRO_MEMORIA;
            return 0;
        }
        /* La referencia que espera a ser mostrada (y la que estaba antes de la
         * última, que se muestra después de los cuadros B que quedaron en medio). */
        if (m->mostrar_ref) {
            m->mostrar_ref = false;
            if (!m1_para_rgb(m, m->a_mostrar, img)) return VIDEO_ERRO_MEMORIA;
            return 0;
        }
        if (m->nfila > 0) {
            m1_pendente_t *pe = &m->fila[0];
            if (pe->tipo == 3) {
                /* Un cuadro B: necesita la referencia de delante (la más
                 * reciente) y la de detrás (la anterior). */
                if (m->ref_a_pronta && m->ref_b_pronta) {
                    /* En el MPEG-1 el vector "de delante" apunta al cuadro
                     * ANTERIOR (el último I o P decodificado) y el "de detrás"
                     * al siguiente (la norma y el ffmpeg también). */
                    bool ok = m1_descodifica(m, 3, pe->dados, pe->tam, &m->trabalho,
                                             &m->ref_b, &m->ref_a);
                    /* Saca lo pendiente de la cola. */
                    libera(pe->dados);
                    for (int i = 1; i < m->nfila; i++) m->fila[i - 1] = m->fila[i];
                    m->nfila--;
                    if (!ok) return VIDEO_ERRO_CORROMPIDO;
                    m->b_mostrar = true;
                    continue;
                }
            } else {
                /* Una referencia (I o P): se decodifica en el plano de ref_b (la
                 * de detrás es ref_a) y luego se cambian los papeles de las dos
                 * referencias: la nueva pasa a ser la de delante y la anterior se
                 * queda esperando a ser mostrada. */
                int tr = pe->tr;
                int tipo = pe->tipo;
                bool ok = m1_descodifica(m, tipo, pe->dados, pe->tam,
                                         &m->ref_b, &m->ref_a, NULL);
                /* Saca lo pendiente de la cola. */
                libera(pe->dados);
                for (int i = 1; i < m->nfila; i++) m->fila[i - 1] = m->fila[i];
                m->nfila--;
                if (!ok) return VIDEO_ERRO_CORROMPIDO;
                bool antiga_a = m->ref_a_pronta;
                m1_plano_t tmp = m->ref_a;
                m->ref_a = m->ref_b;
                m->ref_b = tmp;
                m->ref_a_pronta = true;
                m->ref_b_pronta = antiga_a;
                m->tr_b = m->tr_a;
                if (antiga_a) {
                    m->mostrar_ref = true;
                    m->a_mostrar = &m->ref_b;
                    m->tr_ref = m->tr_a;
                }
                m->tr_a = tr;
                continue;
            }
        }
        if (!m1_le_mais(m)) {
            /* Fin del archivo. Los cuadros B que se quedaron sin referencia de
             * detrás usan un plano gris. */
            if (m->nfila > 0 && m->fila[0].tipo == 3) {
                if (!m->ref_b_pronta) {
                    m1_plano_cinza(&m->ref_b);
                    m->ref_b_pronta = true;
                    continue;
                }
                if (m->ref_a_pronta) continue;   /* descodifica-se a seguir */
            }
            /* Y la última referencia, que nunca llegó a mostrarse. */
            if (m->ref_a_pronta && !m->mostrar_ref && m->nfila == 0) {
                m->ref_a_pronta = false;
                m->ref_b_pronta = false;
                if (!m1_para_rgb(m, &m->ref_a, img)) return VIDEO_ERRO_MEMORIA;
                return 0;
            }
            return VIDEO_ERRO_FIM;
        }
    }

}

void mpeg1_fechar(mpeg1_t *m)
{
    if (!m) return;
    if (m->fd >= 0) fechar(m->fd);
    m1_plano_liberta(&m->ref_a);
    m1_plano_liberta(&m->ref_b);
    m1_plano_liberta(&m->trabalho);
    for (int i = 0; i < m->nfila; i++) libera(m->fila[i].dados);
    if (m->es) libera(m->es);
    if (m->pacote) libera(m->pacote);
    m1_arq_liberta(&m->arq);
    libera(m);
}
