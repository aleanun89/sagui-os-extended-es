// SPDX-License-Identifier: GPL-2.0-or-later
/* audio_flac.c - decodificador FLAC, mejora 053 fase 3 (D098), escrito
 * a partir de la especificación del formato (RFC 9639).
 *
 * Flujo: "fLaC", bloques de metadatos (STREAMINFO, VORBIS_COMMENT; el resto se
 * salta) y cuadros. Cada cuadro tiene una cabecera (sincronía, tamaño del bloque,
 * tasa, asignación de los canales, profundidad, número, CRC-8), un subcuadro por
 * canal (constante, verbatim, predictor fijo de orden 0-4 o LPC de orden 1-32,
 * con residuo codificado en Rice por particiones) y un CRC-16. La decorrelación
 * estéreo (izquierda/lado, lado/derecha, medio/lado) se deshace al final.
 *
 * Salida: 16 bits. Los archivos de 24 bits pierden los 8 bits bajos (truncados,
 * como hace ffmpeg al convertir a s16); de 8 bits, ganan 8 bits cero. */
#include "audio_interno.h"

static flac_t g_flac;
static fluxo_t *F;
static flac_t *S;
static uint16_t tab_crc16[256];
static uint8_t tab_crc8[256];
static bool tabelas_prontas;

static void preparar_crc(void)
{
    if (tabelas_prontas) return;
    for (int i = 0; i < 256; i++) {
        uint16_t c16 = (uint16_t)(i << 8);
        uint8_t c8 = (uint8_t)i;
        for (int k = 0; k < 8; k++) {
            c16 = (c16 & 0x8000) ? (uint16_t)((c16 << 1) ^ 0x8005) : (uint16_t)(c16 << 1);
            c8 = (c8 & 0x80) ? (uint8_t)((c8 << 1) ^ 0x07) : (uint8_t)(c8 << 1);
        }
        tab_crc16[i] = c16;
        tab_crc8[i] = c8;
    }
    tabelas_prontas = true;
}

/* ---- lector de bits ---- */

static inline uint32_t proximo_byte(void)
{
    int b = fl_byte(F);
    if (b < 0) { S->erro_fim = true; return 0; }
    S->crc16 = (uint16_t)((S->crc16 << 8) ^ tab_crc16[(S->crc16 >> 8) ^ b]);
    S->crc8 = tab_crc8[S->crc8 ^ b];
    return (uint32_t)b;
}

static uint32_t bits(unsigned n)
{
    if (n > 16) {
        uint32_t h = bits(n - 16);
        return h << 16 | bits(16);
    }
    while (S->nacc < n) {
        S->acc = S->acc << 8 | proximo_byte();
        S->nacc += 8;
    }
    S->nacc -= n;
    return (S->acc >> S->nacc) & ((1u << n) - 1);
}

static inline int32_t bits_sinal(unsigned n)
{
    if (!n) return 0;
    uint32_t v = bits(n);
    return (int32_t)(v << (32 - n)) >> (32 - n);
}

/* Número de ceros antes de un 1 (código unario); acotado para no dar vueltas con un archivo cortado. */
static inline uint32_t unario(void)
{
    uint32_t q = 0;
    for (;;) {
        if (!S->nacc) {
            S->acc = proximo_byte();
            S->nacc = 8;
            if (S->erro_fim) return q;
        }
        uint32_t resto = S->acc & ((1u << S->nacc) - 1);
        if (resto) {
            unsigned lz = 0;
            while (!(resto & (1u << (S->nacc - 1 - lz)))) lz++;
            q += lz;
            S->nacc -= lz + 1;
            return q;
        }
        q += S->nacc;
        S->nacc = 0;
        if (q > (1u << 22)) { S->erro_fim = true; return q; }
    }
}

/* ---- subcuadros ---- */

static bool residuo(int32_t *x, unsigned n, unsigned ordem)
{
    unsigned metodo = bits(2);
    if (metodo > 1) return false;
    unsigned porder = bits(4), nparts = 1u << porder, bp = metodo ? 5 : 4;
    if (n % nparts || (n >> porder) < ordem) return false;
    unsigned por = n >> porder, k = ordem;
    for (unsigned p = 0; p < nparts; p++) {
        unsigned param = bits(bp), cnt = por - (p ? 0 : ordem);
        if (param == (1u << bp) - 1) {
            unsigned nb = bits(5);
            for (unsigned i = 0; i < cnt; i++) x[k++] = bits_sinal(nb);
        } else {
            for (unsigned i = 0; i < cnt; i++) {
                uint32_t q = unario();
                uint32_t u = q << param | (param ? bits(param) : 0);
                x[k++] = (int32_t)(u >> 1) ^ -(int32_t)(u & 1);
            }
        }
        if (S->erro_fim) return false;
    }
    return true;
}

static bool subquadro(int32_t *x, unsigned n, unsigned bps)
{
    if (bits(1)) return false;
    unsigned tipo = bits(6), perdidos = 0;
    if (bits(1)) perdidos = unario() + 1;
    if (perdidos >= bps) return false;
    bps -= perdidos;
    if (tipo == 0) {
        int32_t v = bits_sinal(bps);
        for (unsigned i = 0; i < n; i++) x[i] = v;
    } else if (tipo == 1) {
        for (unsigned i = 0; i < n; i++) x[i] = bits_sinal(bps);
    } else if (tipo >= 8 && tipo <= 12) {
        unsigned o = tipo - 8;
        if (o > n) return false;
        for (unsigned i = 0; i < o; i++) x[i] = bits_sinal(bps);
        if (!residuo(x, n, o)) return false;
        switch (o) {
        case 1: for (unsigned i = 1; i < n; i++) x[i] += x[i - 1]; break;
        case 2: for (unsigned i = 2; i < n; i++) x[i] += 2 * x[i - 1] - x[i - 2]; break;
        case 3: for (unsigned i = 3; i < n; i++) x[i] += 3 * x[i - 1] - 3 * x[i - 2] + x[i - 3]; break;
        case 4: for (unsigned i = 4; i < n; i++) x[i] += 4 * x[i - 1] - 6 * x[i - 2] + 4 * x[i - 3] - x[i - 4]; break;
        }
    } else if (tipo >= 32) {
        unsigned o = (tipo & 31) + 1;
        if (o > n) return false;
        for (unsigned i = 0; i < o; i++) x[i] = bits_sinal(bps);
        unsigned prec = bits(4) + 1;
        if (prec == 16) return false;
        int shift = bits_sinal(5);
        if (shift < 0) return false;
        int32_t c[32];
        uint64_t somaabs = 0;
        for (unsigned j = 0; j < o; j++) {
            c[j] = bits_sinal(prec);
            somaabs += (uint32_t)(c[j] < 0 ? -c[j] : c[j]);
        }
        if (!residuo(x, n, o)) return false;
        /* la suma cabe en 32 bits si |muestra| * suma|c| < 2^31 (muestras de bps bits y residuo sumado) */
        bool curto = (somaabs << (bps - 1)) < (1u << 30);
        if (curto) {
            for (unsigned i = o; i < n; i++) {
                int32_t s = 0;
                for (unsigned j = 0; j < o; j++) s += c[j] * x[i - 1 - j];
                x[i] += s >> shift;
            }
        } else {
            for (unsigned i = o; i < n; i++) {
                int64_t s = 0;
                for (unsigned j = 0; j < o; j++) s += (int64_t)c[j] * x[i - 1 - j];
                x[i] += (int32_t)(s >> shift);
            }
        }
    } else
        return false;
    if (perdidos)
        for (unsigned i = 0; i < n; i++) x[i] = (int32_t)((uint32_t)x[i] << perdidos);
    return !S->erro_fim;
}

/* ---- cuadro ---- */

static const uint8_t bits_codigo[8] = { 0, 8, 12, 0, 16, 20, 24, 32 };

/* Decodifica el siguiente cuadro. 1 = ok, 0 = fin limpio, <0 = error. */
static int quadro(audio_arquivo_t *a)
{
    (void)a;
    /* sincronía: 0xFF y después 111110xx; salta basura (relleno) hasta encontrarla */
    int b0 = fl_byte(F);
    if (b0 < 0) return 0;
    S->crc16 = 0;
    S->crc8 = 0;
    S->nacc = 0;
    S->erro_fim = false;
    uint32_t pulados = 0;
    int b1;
    for (;;) {
        if (b0 == 0xFF) {
            b1 = fl_byte(F);
            if (b1 < 0) return AUDIO_ERRO_TRUNCADO;
            if ((b1 & 0xFE) == 0xF8) break;
            b0 = b1;
        } else
            b0 = fl_byte(F);
        if (b0 < 0) return AUDIO_ERRO_TRUNCADO;
        if (++pulados > 65536) return AUDIO_ERRO_CORROMPIDO;
    }
    S->crc16 = (uint16_t)(tab_crc16[b0]);   /* CRC-16 de 0xFF desde el valor inicial 0 */
    S->crc16 = (uint16_t)((S->crc16 << 8) ^ tab_crc16[(S->crc16 >> 8) ^ b1]);
    S->crc8 = tab_crc8[tab_crc8[b0] ^ b1];
    uint32_t h2 = proximo_byte(), h3 = proximo_byte();
    unsigned codbloco = h2 >> 4, codtaxa = h2 & 15, atrib = h3 >> 4, codbits = (h3 >> 1) & 7;
    if (h3 & 1) return AUDIO_ERRO_CORROMPIDO;
    /* número del cuadro/muestra: UTF-8 extendido (hasta 7 bytes) */
    uint32_t u = proximo_byte();
    unsigned extra = 0;
    if (u >= 0xFE) return AUDIO_ERRO_CORROMPIDO;
    if (u >= 0xFC) extra = 5;
    else if (u >= 0xF8) extra = 4;
    else if (u >= 0xF0) extra = 3;
    else if (u >= 0xE0) extra = 2;
    else if (u >= 0xC0) extra = 1;
    else if (u >= 0x80) return AUDIO_ERRO_CORROMPIDO;
    uint32_t numero = extra ? u & (0x3Fu >> extra) : u;
    while (extra--) {
        uint32_t c = proximo_byte();
        if ((c & 0xC0) != 0x80) return AUDIO_ERRO_CORROMPIDO;
        numero = numero << 6 | (c & 0x3F);
    }
    unsigned bs;
    if (codbloco == 0) return AUDIO_ERRO_CORROMPIDO;
    else if (codbloco == 1) bs = 192;
    else if (codbloco <= 5) bs = 576u << (codbloco - 2);
    else if (codbloco == 6) bs = proximo_byte() + 1;
    else if (codbloco == 7) { bs = proximo_byte() << 8; bs |= proximo_byte(); bs += 1; }
    else bs = 256u << (codbloco - 8);
    if (codtaxa == 12) proximo_byte();
    else if (codtaxa == 13 || codtaxa == 14) { proximo_byte(); proximo_byte(); }
    else if (codtaxa == 15) return AUDIO_ERRO_CORROMPIDO;
    proximo_byte();   /* el CRC-8: con él incluido, el CRC actual tiene que dar 0 */
    if (S->erro_fim) return AUDIO_ERRO_TRUNCADO;
    if (S->crc8 != 0) return AUDIO_ERRO_CORROMPIDO;
    if (bs > FLAC_BLOCO_MAX) return AUDIO_ERRO_NAO_SUPORTADO;
    unsigned bps = codbits ? bits_codigo[codbits] : S->bits;
    if (!bps || bps > 24) return codbits == 3 || !bps ? AUDIO_ERRO_CORROMPIDO : AUDIO_ERRO_NAO_SUPORTADO;
    unsigned nc = atrib < 8 ? atrib + 1 : 2;
    if (atrib > 10) return AUDIO_ERRO_CORROMPIDO;
    if (nc != S->canais_arq) return AUDIO_ERRO_CORROMPIDO;
    for (unsigned c = 0; c < nc; c++) {
        unsigned bc = bps;
        if ((atrib == 8 && c == 1) || (atrib == 9 && c == 0) || (atrib == 10 && c == 1)) bc++;
        if (!subquadro(S->can[c], bs, bc)) return S->erro_fim ? AUDIO_ERRO_TRUNCADO : AUDIO_ERRO_CORROMPIDO;
    }
    S->nacc = 0;   /* alinea al byte */
    proximo_byte();
    proximo_byte();
    if (S->erro_fim) return AUDIO_ERRO_TRUNCADO;
    if (S->crc16 != 0) return AUDIO_ERRO_CORROMPIDO;
    /* deshace la decorrelación */
    if (nc == 2 && atrib >= 8) {
        int32_t *l = S->can[0], *r = S->can[1];
        for (unsigned i = 0; i < bs; i++) {
            int32_t x = l[i], y = r[i];
            if (atrib == 8) r[i] = x - y;                 /* izquierda, lado */
            else if (atrib == 9) l[i] = x + y;            /* lado, derecha */
            else {                                        /* medio, lado */
                int32_t m = (int32_t)((uint32_t)x << 1) | (y & 1);
                l[i] = (m + y) >> 1;
                r[i] = (m - y) >> 1;
            }
        }
    }
    /* a 16 bits */
    for (unsigned c = 0; c < nc; c++) {
        int32_t *x = S->can[c];
        if (bps > 16) { unsigned s = bps - 16; for (unsigned i = 0; i < bs; i++) x[i] >>= s; }
        else if (bps < 16) { unsigned s = 16 - bps; for (unsigned i = 0; i < bs; i++) x[i] <<= s; }
    }
    S->n = bs;
    S->i = 0;
    S->amostra = (b1 & 1) ? numero : numero * S->bloco_max;   /* número de cuadro (tamaño fijo) o de muestra */
    return 1;
}

int flac_ler(audio_arquivo_t *a, int16_t *pcm, unsigned n)
{
    unsigned feito = 0, nc = S->canais_arq;
    while (feito < n) {
        if (S->i >= S->n) {
            if (a->pend) return feito ? (int)feito : a->pend;
            if (a->info.quadros && a->entregues + feito >= a->info.quadros) break;   /* lo que viene después del último cuadro no es audio */
            int r = quadro(a);
            if (r <= 0) {
                S->n = S->i = 0;
                if (r < 0) a->pend = r;
                return feito ? (int)feito : r;
            }
        }
        unsigned q = S->n - S->i;
        if (q > n - feito) q = n - feito;
        if (a->info.quadros && a->entregues + feito + q > a->info.quadros) q = a->info.quadros - a->entregues - feito;
        for (unsigned i = 0; i < q; i++)
            for (unsigned c = 0; c < nc; c++) {
                int32_t v = S->can[c][S->i + i];
                pcm[(feito + i) * nc + c] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
            }
        S->i += q;
        feito += q;
    }
    return (int)feito;
}

/* Salto (D115): por la proporción de bytes; busca la sincronía del siguiente cuadro (con su CRC-8). */
int flac_buscar(audio_arquivo_t *a, uint32_t q)
{
    uint32_t total = a->info.quadros, ini = a->inicio_dados;
    uint32_t bytes = F->tam > ini ? F->tam - ini : 0;
    uint32_t pos = q == 0 || !total ? ini : ini + audio_proporcao(bytes, q, total);
    if (!fl_posicionar(F, pos)) return AUDIO_ERRO_TRUNCADO;
    S->n = S->i = 0;
    S->erro_fim = false;
    if (q == 0) return 0;
    for (int tent = 0; tent < 200; tent++) {
        int r = quadro(a);
        if (r == 1) { a->entregues = S->amostra; return 0; }   /* la posición real del cuadro */
        if (r == 0) { S->n = S->i = 0; return 0; }   /* final: lo siguiente es el fin del archivo */
    }
    return AUDIO_ERRO_CORROMPIDO;
}

/* ---- apertura: metadatos ---- */

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static bool igual_sem_caixa(const uint8_t *s, unsigned n, const char *nome)
{
    for (unsigned i = 0; nome[i]; i++) {
        if (i >= n) return false;
        uint8_t c = s[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c != (uint8_t)nome[i]) return false;
    }
    return true;
}

static void ler_comentarios(audio_info_t *info, const uint8_t *p, uint32_t n)
{
    if (n < 8) return;
    uint32_t v = le32(p);
    if (v > n - 8) return;
    uint32_t o = 4 + v, cnt = le32(p + o);
    o += 4;
    for (uint32_t k = 0; k < cnt && o + 4 <= n; k++) {
        uint32_t len = le32(p + o);
        o += 4;
        if (len > n - o) break;
        const uint8_t *s = p + o;
        if (igual_sem_caixa(s, len, "TITLE=")) audio_etiqueta_utf8(info->titulo, sizeof info->titulo, s + 6, len - 6);
        else if (igual_sem_caixa(s, len, "ARTIST=")) audio_etiqueta_utf8(info->artista, sizeof info->artista, s + 7, len - 7);
        else if (igual_sem_caixa(s, len, "ALBUM=")) audio_etiqueta_utf8(info->album, sizeof info->album, s + 6, len - 6);
        o += len;
    }
}

int flac_abrir(audio_arquivo_t *a)
{
    preparar_crc();
    F = &a->fl;
    S = &g_flac;
    memset(S, 0, sizeof *S);
    a->flac = S;
    uint8_t m[4];
    if (fl_ler(F, m, 4) != 4) return AUDIO_ERRO_TRUNCADO;
    bool tem_info = false, ultimo = false;
    static uint8_t bloco[8192];
    while (!ultimo) {
        if (fl_ler(F, m, 4) != 4) return AUDIO_ERRO_TRUNCADO;
        ultimo = m[0] >> 7;
        unsigned tipo = m[0] & 127;
        uint32_t len = m[1] << 16 | m[2] << 8 | m[3];
        if (tipo == 0) {
            if (len < 34 || len > sizeof bloco) return AUDIO_ERRO_CORROMPIDO;
            if (fl_ler(F, bloco, len) != len) return AUDIO_ERRO_TRUNCADO;
            const uint8_t *s = bloco;
            S->bloco_max = s[2] << 8 | s[3];
            uint32_t taxa = (uint32_t)s[10] << 12 | s[11] << 4 | s[12] >> 4;
            S->canais_arq = ((s[12] >> 1) & 7) + 1;
            S->bits = (((s[12] & 1) << 4) | s[13] >> 4) + 1;
            /* 36 bits de total de muestras: los 4 altos solo valen para más de 4G muestras (no se espera) */
            uint32_t total = (uint32_t)s[14] << 24 | s[15] << 16 | s[16] << 8 | s[17];
            if (s[13] & 15) total = 0;
            if (S->canais_arq > FLAC_CAN_MAX) return AUDIO_ERRO_NAO_SUPORTADO;
            if (S->bits < 4 || S->bits > 24) return AUDIO_ERRO_NAO_SUPORTADO;
            if (S->bloco_max > FLAC_BLOCO_MAX || taxa < 1) return AUDIO_ERRO_NAO_SUPORTADO;
            a->info.taxa = taxa;
            a->info.canais = S->canais_arq;
            a->info.bits_origem = S->bits;
            a->info.quadros = total;
            S->restam = total;
            tem_info = true;
        } else if (tipo == 4 && len <= sizeof bloco) {
            if (fl_ler(F, bloco, len) != len) return AUDIO_ERRO_TRUNCADO;
            ler_comentarios(&a->info, bloco, len);
        } else if (tipo == 127) {
            return AUDIO_ERRO_CORROMPIDO;
        } else if (!fl_pular(F, len))
            return AUDIO_ERRO_TRUNCADO;
        if (!tem_info) return AUDIO_ERRO_CORROMPIDO;   /* el STREAMINFO tiene que ser el primero */
    }
    a->fmt = AUDIO_FMT_FLAC;
    a->info.formato = AUDIO_FMT_FLAC;
    a->info.nome_formato = "FLAC";
    a->info.sem_perdas = true;
    a->inicio_dados = F->pos;
    if (a->info.quadros && a->info.taxa) {
        uint32_t q = a->info.quadros, t = a->info.taxa;
        a->info.duracao_ms = q / t * 1000 + q % t * 1000 / t;
        if (a->info.duracao_ms && F->tam > F->pos) {
            uint32_t bytes = F->tam - F->pos;
            a->info.kbps = bytes / a->info.duracao_ms * 8 + bytes % a->info.duracao_ms * 8 / a->info.duracao_ms;
        }
    }
    S->n = S->i = 0;
    return 0;
}
