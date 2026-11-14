// SPDX-License-Identifier: GPL-2.0-or-later
/* audio_mp3.c - decodificador MP3 (MPEG-1 y MPEG-2 Layer III), mejora 053 fase 2 (D099).
 *
 * Escrito a partir de la norma ISO/IEC 11172-3 (y 13818-3 para 16, 22,05 y 24
 * kHz). Las tablas numéricas (Huffman, ventana de síntesis, cosenos) vienen de
 * audio_mp3_tabelas.c, generado por tools/gerar_tabelas_mp3.py.
 *
 * Coma fija de enteros, sin FPU ni SSE (los programas de usuario no guardan
 * estado de FPU por defecto y así corre igual en cualquier Pentium): la
 * representación interna es Q24 (1,0 = 2^24) en int32; los productos con
 * coeficientes Q30 se suman en 64 bits (un imul de 32x32 -> 64 por producto).
 *
 * Etapas de cada cuadro (cada una con 1 o 2 gránulos de 576 muestras):
 *   cabecera y side info -> reservorio de bits (main_data_begin) ->
 *   factores de escala -> Huffman (pares y cuádruplas) -> recuantización ->
 *   estéreo M/S y de intensidad -> reordenación de los bloques cortos ->
 *   reducción de alias -> IMDCT con ventanas (larga, inicio, fin, corta,
 *   mixta) y solapamiento -> inversión de frecuencia -> banco polifásico.
 *
 * Etiquetas: ID3v2 (2.2, 2.3 y 2.4) al principio y ID3v1 al final. Duración: de la
 * cabecera Xing/Info (VBR) o, sin ella, del tamaño del archivo. Si hay la
 * etiqueta LAME, se descartan el retardo del codificador y el relleno
 * final (reproducción sin silencio en los empalmes, como ffmpeg). */
#include "audio_interno.h"
#include "audio_mp3_tabelas.h"

#define RES_MAX 511                      /* el main_data_begin tiene 9 bits */
#define QUADRO_MAX 1500                  /* mayor cuadro: 1441 bytes */

typedef struct {
    uint16_t p23, big, gg, sc;
    uint8_t  ws, bt, mixed, tsel[3], sbg[3], r0, r1, preflag, escala, c1tab;
    uint8_t  sfl[23];
    uint8_t  sfs[13][3];
    uint8_t  liml[23];                   /* mayor is_pos válido por banda (estéreo de intensidad MPEG-2) */
    uint8_t  lims[13][3];
} gr_t;

struct mp3_estado {
    /* lo que dice la cabecera (fijo en el flujo) */
    int      versao, taxa_idx, canais, modo, ext;
    uint32_t taxa, amostras_quadro;
    const uint16_t *bl;                  /* bandas largas (23 límites) */
    const uint16_t *bc;                  /* bandas cortas (14 límites) */
    bool     travado;                    /* ya se encontró el primer cuadro */
    /* recorte (sin silencio en los empalmes) */
    uint32_t pos_audio, pular_ini;       /* dónde empieza el sonido y el recorte inicial (para volver al principio) */
    uint32_t tolerar;                    /* cuadros que pueden fallar tras un salto (el reservorio está vacío) */
    uint32_t ruins_salvos;               /* quadros_ruins antes del salto: la sincronía de después no cuenta */
    bool     restaurar_ruins;
    uint32_t pular;                      /* cuadros de PCM a descartar del principio */
    uint32_t limite;                     /* máximo de cuadros a entregar (0 = sin límite) */
    /* reservorio */
    uint8_t  md[RES_MAX + QUADRO_MAX + 16];
    uint32_t res_n;
    /* gránulos */
    gr_t     gr[2][2];
    uint8_t  scfsi[2][4];
    int32_t  xr[2][576];
    int16_t  isv[576];
    int32_t  sobra[2][576];
    int32_t  v[2][1024];
    uint32_t voff[2];
    int32_t  xs[3][192];                 /* bloques cortos reordenados */
    int16_t  pcm[1152 * 2];
    uint32_t pcm_n, pcm_i;
    uint8_t  quadro[QUADRO_MAX];
    bool     fim, truncado;
    uint32_t sem_cabecalho;              /* bytes saltados buscando sincronía desde el último cuadro bueno */
    /* bits del reservorio */
    uint32_t bp, bfim;                   /* posición y fin en bits */
    bool     estouro;
};

static mp3_t g_mp3;

/* ------------------------------------------------------------ tablas pequeñas */

static const uint16_t bandas_longas[6][23] = {
    { 0, 4, 8, 12, 16, 20, 24, 30, 36, 44, 52, 62, 74, 90, 110, 134, 162, 196, 238, 288, 342, 418, 576 },   /* MPEG-1 44,1 */
    { 0, 4, 8, 12, 16, 20, 24, 30, 36, 42, 50, 60, 72, 88, 106, 128, 156, 190, 230, 276, 330, 384, 576 },   /* 48 */
    { 0, 4, 8, 12, 16, 20, 24, 30, 36, 44, 54, 66, 82, 102, 126, 156, 194, 240, 296, 364, 448, 550, 576 },  /* 32 */
    { 0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576 },  /* MPEG-2 22,05 */
    { 0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 114, 136, 162, 194, 232, 278, 332, 394, 464, 540, 576 },  /* 24 */
    { 0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576 },  /* 16 */
};
static const uint16_t bandas_curtas[6][14] = {
    { 0, 4, 8, 12, 16, 22, 30, 40, 52, 66, 84, 106, 136, 192 },
    { 0, 4, 8, 12, 16, 22, 28, 38, 50, 64, 80, 100, 126, 192 },
    { 0, 4, 8, 12, 16, 22, 30, 42, 58, 78, 104, 138, 180, 192 },
    { 0, 4, 8, 12, 18, 24, 32, 42, 56, 74, 100, 132, 174, 192 },
    { 0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 136, 180, 192 },
    { 0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 134, 174, 192 },
};
static const uint8_t pretab[22] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0 };
static const uint8_t slen1_tab[16] = { 0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4 };
static const uint8_t slen2_tab[16] = { 0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3 };
/* MPEG-2: número de bandas en cada una de las 4 particiones, por tabla (6) y tipo de bloque (largo, corto, mixto) */
static const uint8_t nsfb_lsf[6][3][4] = {
    { { 6, 5, 5, 5 }, { 9, 9, 9, 9 }, { 6, 9, 9, 9 } },
    { { 6, 5, 7, 3 }, { 9, 9, 12, 6 }, { 6, 9, 12, 6 } },
    { { 11, 10, 0, 0 }, { 18, 18, 0, 0 }, { 15, 18, 0, 0 } },
    { { 7, 7, 7, 0 }, { 12, 12, 12, 0 }, { 6, 15, 12, 0 } },
    { { 6, 6, 6, 3 }, { 12, 9, 9, 6 }, { 6, 12, 9, 6 } },
    { { 8, 8, 5, 0 }, { 15, 12, 9, 0 }, { 6, 18, 9, 0 } },
};
static const uint16_t tabela_taxas[2][3] = { { 44100, 48000, 32000 }, { 22050, 24000, 16000 } };
static const uint16_t tabela_kbps[2][15] = {
    { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 },
    { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 },
};

/* ------------------------------------------------------------ Huffman: árboles binarios */

/* Un nodo es un par de hijos. Hijo: 0 = inválido; >= 0x8000 = hoja (valor & 0x7FFF; para pares
 * es x * 16 + y, para cuádruplas el índice 0-15); si no, el índice del nodo siguiente. La raíz de cada
 * tabla es el nodo indicado en raiz[]. */
#define NOS_MAX 2200
static uint16_t arvore[NOS_MAX][2];
static uint16_t raiz[34];
static uint16_t nos_usados;
static bool arvores_prontas;

static void montar_arvores(void)
{
    if (arvores_prontas) return;
    nos_usados = 1;   /* el nodo 0 nunca es hijo: sirve de marca de inválido */
    for (int t = 0; t < 34; t++) {
        const mp3_huf_t *h = &mp3_huf[t];
        if (!h->com) continue;
        unsigned n = t >= 32 ? 16 : (unsigned)h->xlen * h->xlen;
        if (!n) continue;
        int igual = -1;   /* las tablas 16-23 y 24-31 reparten los códigos: un solo árbol */
        for (int u = 0; u < t; u++)
            if (mp3_huf[u].cod == h->cod && raiz[u]) igual = u;
        if (igual >= 0) { raiz[t] = raiz[igual]; continue; }
        raiz[t] = nos_usados++;
        for (unsigned s = 0; s < n; s++) {
            unsigned len = h->com[s];
            if (!len) continue;
            unsigned no = raiz[t];
            for (unsigned b = 0; b < len; b++) {
                unsigned bit = (h->cod[s] >> (len - 1 - b)) & 1;
                if (b == len - 1) {
                    unsigned val = t >= 32 ? s : (s / h->xlen) * 16 + s % h->xlen;
                    arvore[no][bit] = (uint16_t)(0x8000 | val);
                } else {
                    if (!arvore[no][bit]) arvore[no][bit] = nos_usados++;
                    no = arvore[no][bit];
                }
            }
        }
    }
    arvores_prontas = true;
}

/* ------------------------------------------------------------ matrices de la IMDCT con la ventana ya aplicada */

static int32_t mat36[3][36 * 18];   /* por tipo de ventana: normal, inicio, fin */
static int32_t mat12[12 * 6];
static bool matrizes_prontas;

static void montar_matrizes(void)
{
    if (matrizes_prontas) return;
    for (int t = 0; t < 3; t++)
        for (int i = 0; i < 36; i++)
            for (int k = 0; k < 18; k++)
                mat36[t][i * 18 + k] = (int32_t)(((int64_t)mp3_imdct36[i * 18 + k] * mp3_janela_longa[t * 36 + i] + (1 << 29)) >> 30);
    for (int i = 0; i < 12; i++)
        for (int k = 0; k < 6; k++)
            mat12[i * 6 + k] = (int32_t)(((int64_t)mp3_imdct12[i * 6 + k] * mp3_janela_curta[i] + (1 << 29)) >> 30);
    matrizes_prontas = true;
}

/* ------------------------------------------------------------ lectura de bits del reservorio */

static inline uint32_t bit1(mp3_t *m)
{
    if (m->bp >= m->bfim) { m->estouro = true; return 0; }
    uint32_t b = (m->md[m->bp >> 3] >> (7 - (m->bp & 7))) & 1;
    m->bp++;
    return b;
}

static inline uint32_t bitsn(mp3_t *m, unsigned n)
{
    if (!n) return 0;
    if (m->bp + n > m->bfim) { m->estouro = true; m->bp = m->bfim; return 0; }
    uint32_t pos = m->bp >> 3, desl = m->bp & 7;
    uint32_t v = (uint32_t)m->md[pos] << 24 | m->md[pos + 1] << 16 | m->md[pos + 2] << 8 | m->md[pos + 3];
    /* los 4 bytes dan al menos 25 bits útiles; n <= 13 aquí, o 16 */
    v = (v << desl) >> (32 - n);
    m->bp += n;
    return v;
}

/* ------------------------------------------------------------ cabecera */

typedef struct {
    int versao, camada_ok, taxa_idx, kbps, canais, modo, ext, crc, preench;
    uint32_t taxa, tam, lado;
} cab_t;

static bool analisar_cabecalho(const uint8_t *p, cab_t *c)
{
    if (p[0] != 0xFF || (p[1] & 0xE0) != 0xE0) return false;
    int ver = (p[1] >> 3) & 3;
    if (ver == 1 || ver == 0) return false;   /* reservado; MPEG-2.5 no soportado */
    if (((p[1] >> 1) & 3) != 1) return false;  /* solo Layer III */
    int bi = p[2] >> 4, si = (p[2] >> 2) & 3;
    if (bi == 0 || bi == 15 || si == 3) return false;
    c->versao = ver == 3 ? 1 : 2;
    c->crc = !(p[1] & 1);
    c->kbps = tabela_kbps[c->versao - 1][bi];
    c->taxa = tabela_taxas[c->versao - 1][si];
    c->taxa_idx = (c->versao - 1) * 3 + si;
    c->preench = (p[2] >> 1) & 1;
    c->modo = p[3] >> 6;
    c->ext = (p[3] >> 4) & 3;
    c->canais = c->modo == 3 ? 1 : 2;
    c->tam = (c->versao == 1 ? 144u : 72u) * c->kbps * 1000 / c->taxa + c->preench;
    c->lado = c->versao == 1 ? (c->canais == 1 ? 17 : 32) : (c->canais == 1 ? 9 : 17);
    return c->tam >= 4 + c->lado + (c->crc ? 2u : 0u);
}

/* ------------------------------------------------------------ side info y factores de escala */

static void ler_side_info(mp3_t *m, const uint8_t *s, int ngr, int canais, int versao, uint32_t *mdb)
{
    /* el side info se lee con el mismo lector de bits, en un búfer temporal */
    uint32_t bp = 0;
#define SB(n) ({ uint32_t _v = 0; for (unsigned _i = 0; _i < (n); _i++) { _v = _v << 1 | ((s[bp >> 3] >> (7 - (bp & 7))) & 1); bp++; } _v; })
    *mdb = SB(versao == 1 ? 9 : 8);
    bp += versao == 1 ? (canais == 1 ? 5 : 3) : (canais == 1 ? 1 : 2);
    if (versao == 1)
        for (int c = 0; c < canais; c++)
            for (int g = 0; g < 4; g++) m->scfsi[c][g] = (uint8_t)SB(1);
    for (int g = 0; g < ngr; g++)
        for (int c = 0; c < canais; c++) {
            gr_t *r = &m->gr[g][c];
            r->p23 = (uint16_t)SB(12);
            r->big = (uint16_t)SB(9);
            r->gg = (uint16_t)SB(8);
            r->sc = (uint16_t)SB(versao == 1 ? 4 : 9);
            r->ws = (uint8_t)SB(1);
            r->bt = 0;
            r->mixed = 0;
            r->sbg[0] = r->sbg[1] = r->sbg[2] = 0;
            if (r->ws) {
                r->bt = (uint8_t)SB(2);
                r->mixed = (uint8_t)SB(1);
                r->tsel[0] = (uint8_t)SB(5);
                r->tsel[1] = (uint8_t)SB(5);
                r->tsel[2] = 0;
                for (int w = 0; w < 3; w++) r->sbg[w] = (uint8_t)SB(3);
                r->r0 = r->bt == 2 ? 8 : 7;   /* valores implícitos (usados solo para calcular las regiones) */
                r->r1 = 36;
            } else {
                for (int k = 0; k < 3; k++) r->tsel[k] = (uint8_t)SB(5);
                r->r0 = (uint8_t)SB(4);
                r->r1 = (uint8_t)SB(3);
            }
            r->preflag = versao == 1 ? (uint8_t)SB(1) : 0;
            r->escala = (uint8_t)SB(1);
            r->c1tab = (uint8_t)SB(1);
            if (r->big > 288) r->big = 288;
        }
#undef SB
}

/* Lee los factores de escala de un gránulo/canal (parte 2). */
static void ler_fatores(mp3_t *m, int g, int c, int versao, bool intens_dir)
{
    gr_t *r = &m->gr[g][c];
    bool curto = r->ws && r->bt == 2;
    memset(r->sfl, 0, sizeof r->sfl);
    memset(r->sfs, 0, sizeof r->sfs);
    for (int i = 0; i < 23; i++) r->liml[i] = 255;
    for (int i = 0; i < 13; i++) r->lims[i][0] = r->lims[i][1] = r->lims[i][2] = 255;
    if (versao == 1) {
        unsigned s1 = slen1_tab[r->sc], s2 = slen2_tab[r->sc];
        if (curto) {
            int nlong = r->mixed ? 8 : 0;
            for (int i = 0; i < nlong; i++) r->sfl[i] = (uint8_t)bitsn(m, s1);
            for (int i = r->mixed ? 3 : 0; i < 12; i++)
                for (int w = 0; w < 3; w++) r->sfs[i][w] = (uint8_t)bitsn(m, i < 6 ? s1 : s2);
        } else {
            static const uint8_t ini[5] = { 0, 6, 11, 16, 21 };
            for (int grp = 0; grp < 4; grp++) {
                if (g == 1 && m->scfsi[c][grp]) {
                    for (int i = ini[grp]; i < ini[grp + 1]; i++) r->sfl[i] = m->gr[0][c].sfl[i];
                } else {
                    unsigned n = grp < 2 ? s1 : s2;
                    for (int i = ini[grp]; i < ini[grp + 1]; i++) r->sfl[i] = (uint8_t)bitsn(m, n);
                }
            }
        }
        return;
    }
    /* MPEG-2 (LSF) */
    unsigned sc = r->sc, slen[4], tab;
    if (!intens_dir) {
        if (sc < 400) { slen[0] = (sc >> 4) / 5; slen[1] = (sc >> 4) % 5; slen[2] = (sc & 15) >> 2; slen[3] = sc & 3; tab = 0; }
        else if (sc < 500) { sc -= 400; slen[0] = (sc >> 2) / 5; slen[1] = (sc >> 2) % 5; slen[2] = sc & 3; slen[3] = 0; tab = 1; }
        else { sc -= 500; slen[0] = sc / 3; slen[1] = sc % 3; slen[2] = slen[3] = 0; tab = 2; r->preflag = 1; }
    } else {
        sc >>= 1;
        if (sc < 180) { slen[0] = sc / 36; slen[1] = (sc % 36) / 6; slen[2] = (sc % 36) % 6; slen[3] = 0; tab = 3; }
        else if (sc < 244) { sc -= 180; slen[0] = (sc & 63) >> 4; slen[1] = (sc & 15) >> 2; slen[2] = sc & 3; slen[3] = 0; tab = 4; }
        else { sc -= 244; slen[0] = sc / 3; slen[1] = sc % 3; slen[2] = slen[3] = 0; tab = 5; }
    }
    const uint8_t *nb = nsfb_lsf[tab][curto ? (r->mixed ? 2 : 1) : 0];
    int sfb = 0, w = 0;   /* posición en el vector de salida: largas por banda, cortas por (banda, ventana) */
    bool no_misto_longo = curto && r->mixed;
    int longas_restantes = no_misto_longo ? 6 : 0;
    int sfb_curta = no_misto_longo ? 3 : 0;
    for (int p = 0; p < 4; p++) {
        for (int k = 0; k < nb[p]; k++) {
            unsigned v = slen[p] ? bitsn(m, slen[p]) : 0;
            uint8_t lim = (uint8_t)((1u << slen[p]) - 1);
            if (!curto) {
                if (sfb < 22) { r->sfl[sfb] = (uint8_t)v; r->liml[sfb] = lim; }
                sfb++;
            } else if (longas_restantes > 0) {
                r->sfl[sfb] = (uint8_t)v;
                r->liml[sfb] = lim;
                sfb++;
                longas_restantes--;
            } else {
                if (sfb_curta < 12) { r->sfs[sfb_curta][w] = (uint8_t)v; r->lims[sfb_curta][w] = lim; }
                if (++w == 3) { w = 0; sfb_curta++; }
            }
        }
    }
}

/* ------------------------------------------------------------ Huffman y recuantización */

/* Decodifica un valor de un árbol; -1 si el código es inválido. */
static inline int huff(mp3_t *m, unsigned t)
{
    unsigned no = raiz[t];
    for (;;) {
        unsigned f = arvore[no][bit1(m)];
        if (f >= 0x8000) return (int)(f & 0x7FFF);
        if (!f || m->estouro) return -1;
        no = f;
    }
}

/* Decodifica los valores enteros de un gránulo en m->isv; devuelve cuántos (hasta donde hay no ceros) o -1. */
static int huffman_granulo(mp3_t *m, const gr_t *g, uint32_t fim_bits)
{
    int16_t *iv = m->isv;
    memset(iv, 0, sizeof m->isv);
    int nbig = g->big * 2;
    int b1, b2;
    if (g->ws) {
        b1 = g->bt == 2 ? 36 : m->bl[8];
        b2 = 576;
    } else {
        int i1 = g->r0 + 1, i2 = g->r0 + g->r1 + 2;
        b1 = m->bl[i1 > 22 ? 22 : i1];
        b2 = m->bl[i2 > 22 ? 22 : i2];
    }
    int reg[4] = { 0, b1, b2, 576 };
    int i = 0;
    for (int r = 0; r < 3; r++) {
        int fim = reg[r + 1] < nbig ? reg[r + 1] : nbig;
        unsigned t = g->tsel[r];
        if (i >= fim) continue;
        if (t == 0) { i = fim; continue; }
        if (t == 4 || t == 14 || t > 31 || !mp3_huf[t].xlen) return -1;
        unsigned lb = mp3_huf[t].linbits;
        for (; i < fim; i += 2) {
            int v = huff(m, t);
            if (v < 0) return -1;
            int x = v >> 4, y = v & 15;
            if (x == 15 && lb) x += (int)bitsn(m, lb);
            if (x && bit1(m)) x = -x;
            if (y == 15 && lb) y += (int)bitsn(m, lb);
            if (y && bit1(m)) y = -y;
            iv[i] = (int16_t)x;
            iv[i + 1] = (int16_t)y;
        }
        if (m->estouro) return -1;
    }
    /* cuádruplas */
    unsigned tq = 32 + g->c1tab;
    while (i + 4 <= 576 && m->bp < fim_bits) {
        int v = huff(m, tq);
        if (v < 0) { if (m->estouro) break; return -1; }
        /* el bit 3 del índice es el primer valor (v), el bit 0 el último (y) */
        int q[4] = { (v >> 3) & 1, (v >> 2) & 1, (v >> 1) & 1, v & 1 };
        uint32_t guarda = m->bp;
        for (int k = 0; k < 4; k++)
            if (q[k] && bit1(m)) q[k] = -q[k];
        if (m->bp > fim_bits) { m->bp = guarda; break; }
        for (int k = 0; k < 4; k++) iv[i + k] = (int16_t)q[k];
        i += 4;
    }
    int n = i;
    while (n > 0 && !iv[n - 1]) n--;
    return n;
}

static inline int32_t escalar(uint32_t mant, int8_t e43, uint32_t g4, int shiftq, bool neg)
{
    /* valor Q24 = mant * 2^e43 * (g4 / 2^30) * 2^shiftq * 2^24 */
    uint64_t p = (uint64_t)mant * g4;
    int R = 6 - e43 - shiftq;
    uint64_t v;
    if (R >= 64) return 0;
    if (R > 0) v = (p + (1ull << (R - 1))) >> R;
    else if (-R > 31 || p > (0x7FFFFFFFull >> -R)) v = 0x7FFFFFFF;
    else v = p << -R;
    if (v > 0x7FFFFFFF) v = 0x7FFFFFFF;
    return neg ? -(int32_t)v : (int32_t)v;
}

static inline int32_t requant(int v, uint32_t g4, int shiftq)
{
    if (!v) return 0;
    bool neg = v < 0;
    unsigned a = (unsigned)(neg ? -v : v);
    if (a > 8206) a = 8206;
    return escalar(mp3_pot43_mant[a], mp3_pot43_exp[a], g4, shiftq, neg);
}

/* Pasa los enteros de m->isv a xr (Q24), con la ganancia de cada banda. */
static void requantizar(mp3_t *m, const gr_t *g, int nlin, int32_t *xr)
{
    memset(xr, 0, 576 * sizeof *xr);
    int base = (int)g->gg - 210;
    int mult = 2 + 2 * g->escala;
    bool curto = g->ws && g->bt == 2;
    if (!curto) {
        for (int sfb = 0; sfb < 22; sfb++) {
            int a = m->bl[sfb], b = m->bl[sfb + 1];
            if (a >= nlin) break;
            int q = base - mult * (g->sfl[sfb] + (g->preflag ? pretab[sfb] : 0));
            uint32_t g4 = mp3_ganho4[q & 3];
            int sh = q >> 2;
            if (b > nlin) b = nlin;
            for (int i = a; i < b; i++) xr[i] = requant(m->isv[i], g4, sh);
        }
        return;
    }
    int ini_curta = 0;
    if (g->mixed) {
        int nl = m->bl == bandas_longas[0] || m->bl == bandas_longas[1] || m->bl == bandas_longas[2] ? 8 : 6;
        for (int sfb = 0; sfb < nl; sfb++) {
            int a = m->bl[sfb], b = m->bl[sfb + 1];
            if (a >= nlin) break;
            int q = base - mult * (g->sfl[sfb] + (g->preflag ? pretab[sfb] : 0));
            uint32_t g4 = mp3_ganho4[q & 3];
            int sh = q >> 2;
            if (b > nlin) b = nlin;
            for (int i = a; i < b; i++) xr[i] = requant(m->isv[i], g4, sh);
        }
        ini_curta = 3;
    }
    for (int sfb = ini_curta; sfb < 13; sfb++) {
        int a = m->bc[sfb], larg = m->bc[sfb + 1] - a;
        for (int w = 0; w < 3; w++) {
            int ini = 3 * a + w * larg;
            if (ini >= nlin) continue;
            int q = base - 8 * g->sbg[w] - mult * g->sfs[sfb][w];
            uint32_t g4 = mp3_ganho4[q & 3];
            int sh = q >> 2;
            for (int i = ini; i < ini + larg && i < nlin; i++) xr[i] = requant(m->isv[i], g4, sh);
        }
    }
}

/* ------------------------------------------------------------ estéreo */

/* Estéreo de intensidad y M/S sobre el par de canales de un gránulo. */
static void estereo(mp3_t *m, int g, int ext, int versao)
{
    bool ms = ext & 2, is = ext & 1;
    int32_t *l = m->xr[0], *r = m->xr[1];
    const gr_t *gl = &m->gr[g][0], *gr = &m->gr[g][1];
    bool curto = gr->ws && gr->bt == 2;
    /* marca de las líneas (por banda) tratadas por intensidad: 1 = intensidad */
    uint8_t ib[576];
    memset(ib, 0, sizeof ib);
    if (is) {
        int escala2 = gr->sc & 1;   /* MPEG-2: intensity_scale */
        int nl = (m->bl == bandas_longas[0] || m->bl == bandas_longas[1] || m->bl == bandas_longas[2]) ? 8 : 6;
        (void)gl;
        if (!curto) {
            int ult = 575;
            while (ult >= 0 && !r[ult]) ult--;
            for (int sfb = 0; sfb < 22; sfb++) {
                int a = m->bl[sfb], b = m->bl[sfb + 1];
                if (a <= ult) continue;       /* la banda tiene algo en el canal derecho: no es de intensidad */
                int pos = gr->sfl[sfb < 21 ? sfb : 20];
                int lim = versao == 1 ? 7 : gr->liml[sfb < 21 ? sfb : 20];
                if (pos >= lim) continue;     /* posición ilegal: sigue como estéreo normal */
                for (int i = a; i < b; i++) {
                    int32_t x = l[i];
                    if (versao == 1) {
                        l[i] = (int32_t)(((int64_t)x * mp3_is_esq[pos]) >> 30);
                        r[i] = (int32_t)(((int64_t)x * mp3_is_dir[pos]) >> 30);
                    } else {
                        int n = (pos + 1) >> 1;
                        int idx = escala2 ? 2 * n : n;
                        if (idx > 31) idx = 31;
                        int32_t f = mp3_is2_q[idx];
                        if (pos == 0) r[i] = x;
                        else if (pos & 1) { l[i] = (int32_t)(((int64_t)x * f) >> 30); r[i] = x; }
                        else { l[i] = x; r[i] = (int32_t)(((int64_t)x * f) >> 30); }
                    }
                    ib[i] = 1;
                }
            }
        } else {
            int ini = gr->mixed ? 3 : 0;
            for (int w = 0; w < 3; w++) {
                int ult = -1;   /* última línea no nula del canal derecho en esta ventana (índice en la ventana) */
                for (int sfb = ini; sfb < 13; sfb++) {
                    int a = m->bc[sfb], larg = m->bc[sfb + 1] - a;
                    for (int j = 0; j < larg; j++)
                        if (r[3 * a + w * larg + j]) ult = a + j;
                }
                for (int sfb = ini; sfb < 13; sfb++) {
                    int a = m->bc[sfb], larg = m->bc[sfb + 1] - a;
                    if (a <= ult) continue;
                    int pos = gr->sfs[sfb < 12 ? sfb : 11][w];
                    int lim = versao == 1 ? 7 : gr->lims[sfb < 12 ? sfb : 11][w];
                    if (pos >= lim) continue;
                    for (int j = 0; j < larg; j++) {
                        int i = 3 * a + w * larg + j;
                        int32_t x = l[i];
                        if (versao == 1) {
                            l[i] = (int32_t)(((int64_t)x * mp3_is_esq[pos]) >> 30);
                            r[i] = (int32_t)(((int64_t)x * mp3_is_dir[pos]) >> 30);
                        } else {
                            int n = (pos + 1) >> 1;
                            int idx = escala2 ? 2 * n : n;
                            if (idx > 31) idx = 31;
                            int32_t f = mp3_is2_q[idx];
                            if (pos == 0) r[i] = x;
                            else if (pos & 1) { l[i] = (int32_t)(((int64_t)x * f) >> 30); r[i] = x; }
                            else { l[i] = x; r[i] = (int32_t)(((int64_t)x * f) >> 30); }
                        }
                        ib[i] = 1;
                    }
                }
            }
        }
        (void)nl;
    }
    if (ms) {
        for (int i = 0; i < 576; i++) {
            if (ib[i]) continue;
            int32_t a = l[i], b = r[i];
            l[i] = (int32_t)(((int64_t)a + b) * mp3_raiz_meio >> 30);
            r[i] = (int32_t)(((int64_t)a - b) * mp3_raiz_meio >> 30);
        }
    }
}

/* ------------------------------------------------------------ alias, IMDCT, síntesis */

static void reduzir_alias(int32_t *xr, int limite_sb)
{
    for (int sb = 1; sb < limite_sb; sb++) {
        int32_t *p = xr + sb * 18;
        for (int i = 0; i < 8; i++) {
            int32_t a = p[-1 - i], b = p[i];
            p[-1 - i] = (int32_t)(((int64_t)a * mp3_alias_cs[i] - (int64_t)b * mp3_alias_ca[i]) >> 30);
            p[i] = (int32_t)(((int64_t)b * mp3_alias_cs[i] + (int64_t)a * mp3_alias_ca[i]) >> 30);
        }
    }
}

static inline int32_t sat32(int64_t v)
{
    return v > 0x7FFFFFFF ? 0x7FFFFFFF : v < -0x7FFFFFFFLL ? -0x7FFFFFFF : (int32_t)v;
}

/* IMDCT de un canal de un gránulo: rellena s[t][sb] (18 x 32), ya con el solapamiento y la inversión de frecuencia. */
static void imdct_canal(mp3_t *m, int ch, const gr_t *g, int nlin, int32_t s[18][32])
{
    int32_t *xr = m->xr[ch];
    bool curto = g->ws && g->bt == 2;
    int nl_misto = (m->bl == bandas_longas[0] || m->bl == bandas_longas[1] || m->bl == bandas_longas[2]) ? 8 : 6;
    (void)nl_misto;
    if (curto) {
        /* reordena: xs[ventana][frecuencia] a partir del orden del Huffman (banda, ventana, línea) */
        int ini = g->mixed ? 3 : 0;
        for (int sfb = ini; sfb < 13; sfb++) {
            int a = m->bc[sfb], larg = m->bc[sfb + 1] - a;
            for (int w = 0; w < 3; w++)
                for (int j = 0; j < larg; j++) m->xs[w][a + j] = xr[3 * a + w * larg + j];
        }
        if (g->mixed) {
            for (int w = 0; w < 3; w++)
                for (int j = 0; j < 12; j++) m->xs[w][j] = 0;
        }
    } else
        reduzir_alias(xr, 32);
    if (g->mixed && curto) reduzir_alias(xr, 2);   /* bloque mixto: alias solo entre las dos subbandas largas */
    int sbmax = (nlin + 17) / 18 + 1;   /* después de ella las entradas son cero (el alias avanza una subbanda) */
    if (sbmax > 32) sbmax = 32;
    for (int sb = 0; sb < 32; sb++) {
        int32_t z[36];
        int32_t *so = m->sobra[ch] + sb * 18;
        bool longo = !curto || (g->mixed && sb < 2);
        bool zero = sb >= sbmax && !curto;
        if (!zero && !longo) {
            /* tres ventanas de 12 puntos */
            memset(z, 0, sizeof z);
            for (int w = 0; w < 3; w++) {
                const int32_t *x = &m->xs[w][sb * 6];
                int64_t a0[12];
                for (int i = 0; i < 12; i++) {
                    int64_t acc = 0;
                    for (int k = 0; k < 6; k++) acc += (int64_t)x[k] * mat12[i * 6 + k];
                    a0[i] = acc >> 30;
                }
                for (int i = 0; i < 12; i++) z[6 + 6 * w + i] += sat32(a0[i]);
            }
        } else if (!zero) {
            const int32_t *x = xr + sb * 18;
            int kmax = 17;
            while (kmax >= 0 && !x[kmax]) kmax--;
            if (kmax < 0) {
                memset(z, 0, sizeof z);
            } else {
                int tipo = g->ws ? (curto ? 0 : (g->bt == 1 ? 1 : 2)) : 0;
                const int32_t *mt = mat36[tipo];
                for (int i = 0; i < 36; i++) {
                    int64_t acc = 0;
                    const int32_t *c = mt + i * 18;
                    for (int k = 0; k <= kmax; k++) acc += (int64_t)x[k] * c[k];
                    z[i] = sat32(acc >> 30);
                }
            }
        } else
            memset(z, 0, sizeof z);
        for (int t = 0; t < 18; t++) {
            int32_t o = z[t] + so[t];
            if ((sb & 1) && (t & 1)) o = -o;
            s[t][sb] = o;
            so[t] = z[18 + t];
        }
    }
}

/* Una rebanada de tiempo: 32 muestras de subbanda -> 32 muestras de PCM. */
static void sintese(mp3_t *m, int ch, const int32_t *sub, int16_t *saida, int passo)
{
    int32_t *v = m->v[ch];
    uint32_t off = (m->voff[ch] - 64) & 1023;
    m->voff[ch] = off;
    for (int i = 0; i < 64; i++) {
        const int32_t *n = mp3_sintese_n + i * 32;
        int64_t acc = 0;
        for (int k = 0; k < 32; k++) acc += (int64_t)n[k] * sub[k];
        v[(off + i) & 1023] = sat32(acc >> 30);
    }
    for (int j = 0; j < 32; j++) {
        int64_t acc = 0;
        for (int i = 0; i < 8; i++) {
            acc += (int64_t)mp3_janela_d[64 * i + j] * v[(off + 128 * i + j) & 1023];
            acc += (int64_t)mp3_janela_d[64 * i + 32 + j] * v[(off + 128 * i + 96 + j) & 1023];
        }
        acc = (acc + (1 << 24)) >> 25;
        saida[j * passo] = (int16_t)(acc > 32767 ? 32767 : acc < -32768 ? -32768 : acc);
    }
}

/* ------------------------------------------------------------ un cuadro */

/* Decodifica el cuadro en m->quadro (con cabecera c) a m->pcm. Devuelve false si el cuadro es inutilizable. */
static bool decodificar_quadro(mp3_t *m, const cab_t *c)
{
    int ngr = c->versao == 1 ? 2 : 1, nc = c->canais;
    uint32_t ini = 4 + (c->crc ? 2 : 0);
    uint32_t mdb;
    ler_side_info(m, m->quadro + ini, ngr, nc, c->versao, &mdb);
    uint32_t dados = c->tam - ini - c->lado;
    /* reservorio: añade los datos principales de este cuadro a los que sobraron */
    memcpy(m->md + m->res_n, m->quadro + ini + c->lado, dados);
    memset(m->md + m->res_n + dados, 0, 8);
    uint32_t total = m->res_n + dados;
    bool ok = mdb <= m->res_n;
    uint32_t pos0 = ok ? m->res_n - mdb : 0;
    m->bfim = total * 8;
    m->bp = pos0 * 8;
    m->estouro = false;
    uint32_t amostras = c->versao == 1 ? 1152 : 576;
    memset(m->pcm, 0, amostras * nc * sizeof(int16_t));
    if (ok) {
        static int32_t sub[18][32];
        for (int g = 0; g < ngr && ok; g++) {
            int nlin[2];
            for (int ch = 0; ch < nc; ch++) {
                gr_t *r = &m->gr[g][ch];
                uint32_t ini_bits = m->bp, fim_bits = ini_bits + r->p23;
                if (fim_bits > m->bfim) { ok = false; break; }
                if (r->ws && r->bt == 0) { ok = false; break; }
                ler_fatores(m, g, ch, c->versao, c->versao == 2 && (c->ext & 1) && c->modo == 1 && ch == 1);
                if (m->bp > fim_bits || m->estouro) { ok = false; break; }
                int n = huffman_granulo(m, r, fim_bits);
                if (n < 0) { ok = false; break; }
                nlin[ch] = n;
                requantizar(m, r, n, m->xr[ch]);
                m->bp = fim_bits;
            }
            if (!ok) break;
            if (nc == 2 && c->modo == 1 && c->ext) {
                estereo(m, g, c->ext, c->versao);
                nlin[0] = nlin[1] = nlin[0] > nlin[1] ? nlin[0] : nlin[1];
                if (c->ext & 1) nlin[0] = nlin[1] = 576;
            }
            for (int ch = 0; ch < nc; ch++) {
                imdct_canal(m, ch, &m->gr[g][ch], nlin[ch], sub);
                for (int t = 0; t < 18; t++)
                    sintese(m, ch, sub[t], m->pcm + (g * 576 + t * 32) * nc + ch, nc);
            }
        }
    }
    /* el reservorio se queda con los últimos RES_MAX bytes */
    if (total > RES_MAX) {
        memmove(m->md, m->md + total - RES_MAX, RES_MAX);
        total = RES_MAX;
    }
    m->res_n = total;
    m->pcm_n = amostras;
    m->pcm_i = 0;
    if (!ok) {
        memset(m->pcm, 0, amostras * nc * sizeof(int16_t));
        return false;
    }
    return true;
}

/* ------------------------------------------------------------ etiquetas y cabecera Xing */

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static uint32_t sync_safe(const uint8_t *p) { return (uint32_t)(p[0] & 127) << 21 | (p[1] & 127) << 14 | (p[2] & 127) << 7 | (p[3] & 127); }

static void etiqueta_texto(char *dst, unsigned max, const uint8_t *d, uint32_t n)
{
    if (n < 2) return;
    switch (d[0]) {
    case 0: audio_copiar_etiqueta(dst, max, (const char *)d + 1, n - 1); break;
    case 1: audio_etiqueta_utf16(dst, max, d + 1, n - 1, false); break;
    case 2: audio_etiqueta_utf16(dst, max, d + 1, n - 1, true); break;
    default: audio_etiqueta_utf8(dst, max, d + 1, n - 1); break;
    }
}

/* ID3v2 al principio del archivo; deja el lector después de la etiqueta. */
static void ler_id3v2(audio_arquivo_t *a)
{
    fluxo_t *f = &a->fl;
    uint8_t h[10];
    if (fl_garantir(f, 10) < 10 || memcmp(fl_ptr(f), "ID3", 3) != 0) return;
    memcpy(h, fl_ptr(f), 10);
    uint8_t ver = h[3], flags = h[5];
    uint32_t tam = sync_safe(h + 6), fim = 10 + tam + ((flags & 0x10) ? 10 : 0);
    if (ver < 2 || ver > 4 || (h[6] | h[7] | h[8] | h[9]) & 0x80) { fl_pular(f, 0); return; }
    uint32_t base = f->pos;
    fl_pular(f, 10);
    uint32_t resto = tam;
    if ((flags & 0x40) && ver >= 3) {   /* cabecera extendida */
        uint8_t e[4];
        if (fl_ler(f, e, 4) != 4) return;
        uint32_t t = ver == 4 ? sync_safe(e) - 4 : be32(e);
        if (t > resto) return;
        fl_pular(f, t);
        resto -= t + 4;
    }
    unsigned hl = ver == 2 ? 6 : 10;
    static uint8_t buf[1024];
    while (resto > hl) {
        uint8_t fh[10];
        if (fl_ler(f, fh, hl) != hl || fh[0] == 0) break;
        uint32_t t = ver == 2 ? (uint32_t)(fh[3] << 16 | fh[4] << 8 | fh[5]) : ver == 4 ? sync_safe(fh + 4) : be32(fh + 4);
        resto -= hl;
        if (t > resto) break;
        char *dst = NULL;
        if (ver == 2) {
            if (!memcmp(fh, "TT2", 3)) dst = a->info.titulo;
            else if (!memcmp(fh, "TP1", 3)) dst = a->info.artista;
            else if (!memcmp(fh, "TAL", 3)) dst = a->info.album;
        } else {
            if (!memcmp(fh, "TIT2", 4)) dst = a->info.titulo;
            else if (!memcmp(fh, "TPE1", 4)) dst = a->info.artista;
            else if (!memcmp(fh, "TALB", 4)) dst = a->info.album;
        }
        if (dst && t <= sizeof buf) {
            if (fl_ler(f, buf, t) != t) break;
            etiqueta_texto(dst, 96, buf, t);
        } else if (!fl_pular(f, t))
            break;
        resto -= t;
    }
    fl_posicionar(f, base + fim);
}

static void ler_id3v1(audio_arquivo_t *a)
{
    fluxo_t *f = &a->fl;
    if (f->tam < 128) return;
    uint32_t guarda = f->pos;
    uint8_t t[128];
    if (!fl_posicionar(f, f->tam - 128)) return;
    if (fl_ler(f, t, 128) == 128 && !memcmp(t, "TAG", 3)) {
        if (!a->info.titulo[0]) audio_copiar_etiqueta(a->info.titulo, 96, (const char *)t + 3, 30);
        if (!a->info.artista[0]) audio_copiar_etiqueta(a->info.artista, 96, (const char *)t + 33, 30);
        if (!a->info.album[0]) audio_copiar_etiqueta(a->info.album, 96, (const char *)t + 63, 30);
    }
    fl_posicionar(f, guarda);
}

/* ------------------------------------------------------------ sincronía y apertura */

/* Busca el siguiente cuadro válido; lo deja en m->quadro y lo consume. 1 = encontrado, 0 = fin, -1 = cortado. */
static int proximo_quadro(audio_arquivo_t *a, cab_t *c)
{
    mp3_t *m = a->mp3;
    fluxo_t *f = &a->fl;
    for (;;) {
        uint32_t n = fl_garantir(f, 4);
        if (n < 4) return 0;
        const uint8_t *p = fl_ptr(f);
        bool bom = analisar_cabecalho(p, c);
        if (bom && m->travado && (c->versao != m->versao || c->taxa_idx != m->taxa_idx || c->canais != m->canais)) bom = false;
        if (bom) {
            n = fl_garantir(f, c->tam + 4);
            if (n < c->tam) {
                if (!m->travado) return 0;
                m->truncado = true;
                return -1;
            }
            p = fl_ptr(f);
            cab_t seg;
            /* confirma con la cabecera del cuadro siguiente (salvo al final del archivo) */
            if (n >= c->tam + 4 && !analisar_cabecalho(p + c->tam, &seg)) bom = false;
        }
        if (!bom) {
            fl_consumir(f, 1);
            if (m->travado) a->info.quadros_ruins += m->sem_cabecalho == 0;
            m->sem_cabecalho++;
            if (m->sem_cabecalho > (1u << 20) && !m->travado) return 0;
            continue;
        }
        memcpy(m->quadro, p, c->tam);
        fl_consumir(f, c->tam);
        m->sem_cabecalho = 0;
        return 1;
    }
}

int mp3_abrir(audio_arquivo_t *a)
{
    mp3_t *m = &g_mp3;
    montar_arvores();
    montar_matrizes();
    memset(m, 0, sizeof *m);
    a->mp3 = m;
    fluxo_t *f = &a->fl;
    ler_id3v2(a);
    a->inicio_dados = f->pos;
    ler_id3v1(a);
    /* primer cuadro */
    cab_t c;
    int r = proximo_quadro(a, &c);
    if (r <= 0) { a->inicio_dados = f->pos ? a->inicio_dados : 0; return AUDIO_ERRO_FORMATO; }
    m->travado = true;
    m->versao = c.versao;
    m->taxa_idx = c.taxa_idx;
    m->taxa = c.taxa;
    m->canais = c.canais;
    m->modo = c.modo;
    m->amostras_quadro = c.versao == 1 ? 1152 : 576;
    m->bl = bandas_longas[c.taxa_idx];
    m->bc = bandas_curtas[c.taxa_idx];
    a->info.taxa = c.taxa;
    a->info.canais = c.canais;
    a->info.formato = AUDIO_FMT_MP3;
    a->info.nome_formato = c.versao == 1 ? "MP3 (MPEG-1 Layer III)" : "MP3 (MPEG-2 Layer III)";
    a->info.bits_origem = 0;
    a->fmt = AUDIO_FMT_MP3;
    uint32_t pos_primeiro = f->pos - c.tam;
    /* ¿cabecera Xing/Info en el primer cuadro? */
    uint32_t off = 4 + (c.crc ? 2 : 0) + c.lado;
    const uint8_t *x = m->quadro + off;
    bool xing = off + 8 <= c.tam && (!memcmp(x, "Xing", 4) || !memcmp(x, "Info", 4));
    uint32_t total_quadros = 0, bytes_audio = 0;
    uint32_t atraso = 0, preench = 0;
    bool lame = false;
    if (xing) {
        a->info.vbr = !memcmp(x, "Xing", 4);
        uint32_t flags = be32(x + 4), p = off + 8;
        if ((flags & 1) && p + 4 <= c.tam) { total_quadros = be32(m->quadro + p); p += 4; }
        if ((flags & 2) && p + 4 <= c.tam) { bytes_audio = be32(m->quadro + p); p += 4; }
        if (flags & 4) p += 100;
        if (flags & 8) p += 4;
        if (p + 24 <= c.tam && (!memcmp(m->quadro + p, "LAME", 4) || !memcmp(m->quadro + p, "Lavf", 4) ||
                                 !memcmp(m->quadro + p, "Lavc", 4) || !memcmp(m->quadro + p, "GOGO", 4))) {
            uint32_t v = (uint32_t)m->quadro[p + 21] << 16 | m->quadro[p + 22] << 8 | m->quadro[p + 23];
            atraso = v >> 12;
            preench = v & 4095;
            lame = atraso < 4096 && !(atraso == 0 && preench == 0 && false);
        }
        /* el cuadro del Xing no lleva sonido: se sigue con el siguiente */
    } else {
        /* el primer cuadro lleva sonido: vuelve a él */
        fl_posicionar(f, pos_primeiro);
    }
    m->pos_audio = f->pos;
    uint32_t aq = m->amostras_quadro;
    if (xing && total_quadros) {
        uint64_t amostras = (uint64_t)total_quadros * aq;
        if (lame && amostras > (uint64_t)atraso + preench) {
            m->pular = atraso + 528 + 1;
            m->limite = (uint32_t)(amostras - atraso - preench);
        } else
            m->limite = (uint32_t)amostras;
        a->info.quadros = m->limite;
    } else {
        uint32_t util = f->tam > pos_primeiro ? f->tam - pos_primeiro : 0;
        if (f->tam >= 128 && a->info.quadros == 0) {
            /* descontar el ID3v1 del final */
            uint32_t guarda = f->pos;
            uint8_t t3[3];
            if (fl_posicionar(f, f->tam - 128) && fl_ler(f, t3, 3) == 3 && !memcmp(t3, "TAG", 3)) util -= util > 128 ? 128 : util;
            fl_posicionar(f, guarda);
        }
        if (c.tam) a->info.quadros = (util / c.tam) * aq;
        if (xing) a->info.quadros = a->info.quadros > aq ? a->info.quadros - aq : 0;
    }
    a->info.duracao_ms = a->info.quadros / c.taxa * 1000 + a->info.quadros % c.taxa * 1000 / c.taxa;
    if (a->info.vbr && a->info.duracao_ms) {
        uint32_t b = bytes_audio ? bytes_audio : (f->tam > pos_primeiro ? f->tam - pos_primeiro : 0);
        a->info.kbps = b / a->info.duracao_ms * 8 + b % a->info.duracao_ms * 8 / a->info.duracao_ms;
    } else
        a->info.kbps = (uint32_t)c.kbps;
    a->inicio_dados = pos_primeiro;
    m->pular_ini = m->pular;
    return 0;
}

/* Salto (D115): al cuadro más cercano por la proporción de bytes (exacto en CBR); el reservorio
 * y el solapamiento de la síntesis empiezan de cero, y los primeros cuadros pueden fallar sin contar. */
int mp3_buscar(audio_arquivo_t *a, uint32_t q)
{
    mp3_t *m = a->mp3;
    fluxo_t *f = &a->fl;
    uint32_t total = a->info.quadros, ini = m->pos_audio;
    uint32_t bytes = f->tam > ini ? f->tam - ini : 0;
    uint32_t pos = q == 0 || !total ? ini : ini + audio_proporcao(bytes, q, total);
    if (!fl_posicionar(f, pos)) return AUDIO_ERRO_TRUNCADO;
    m->fim = m->truncado = false;
    m->pcm_n = m->pcm_i = 0;
    m->res_n = 0;
    memset(m->sobra, 0, sizeof m->sobra);
    memset(m->v, 0, sizeof m->v);
    m->voff[0] = m->voff[1] = 0;
    m->sem_cabecalho = 0;
    m->pular = q == 0 ? m->pular_ini : 0;
    m->tolerar = q == 0 ? 0 : 2;
    m->ruins_salvos = a->info.quadros_ruins;
    m->restaurar_ruins = true;
    return 0;
}

int mp3_ler(audio_arquivo_t *a, int16_t *pcm, unsigned n)
{
    mp3_t *m = a->mp3;
    unsigned nc = m->canais, feito = 0;
    while (feito < n) {
        if (m->pcm_i >= m->pcm_n) {
            if (m->fim) break;
            if (m->limite && a->entregues + feito >= m->limite) { m->fim = true; break; }
            cab_t c;
            int r = proximo_quadro(a, &c);
            if (r == 0) { m->fim = true; break; }
            if (r < 0) {
                m->fim = true;
                return feito ? (int)feito : AUDIO_ERRO_TRUNCADO;
            }
            if (m->restaurar_ruins) { a->info.quadros_ruins = m->ruins_salvos; m->restaurar_ruins = false; }
            /* el cuadro del Xing/Info (sin sonido) ya se trató al abrir: aquí solo cuadros con sonido */
            if (!decodificar_quadro(m, &c)) {
                if (m->tolerar) m->tolerar--;
                else a->info.quadros_ruins++;
            } else m->tolerar = 0;
            if (m->pular) {
                uint32_t d = m->pular < m->pcm_n ? m->pular : m->pcm_n;
                m->pcm_i = d;
                m->pular -= d;
            }
            continue;
        }
        unsigned q = m->pcm_n - m->pcm_i;
        if (q > n - feito) q = n - feito;
        if (m->limite && a->entregues + feito + q > m->limite) q = m->limite - a->entregues - feito;
        memcpy(pcm + feito * nc, m->pcm + m->pcm_i * nc, q * nc * sizeof(int16_t));
        m->pcm_i += q;
        feito += q;
        if (!q) break;
    }
    if (!feito && m->truncado) return AUDIO_ERRO_TRUNCADO;
    return (int)feito;
}
