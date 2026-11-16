// SPDX-License-Identifier: GPL-3.0-or-later
/* audio_conversor.c - cambio de canales y frecuencia, WAV y FLAC de salida (D339). Ver audio_conversor.h.
 *
 * El FLAC sigue la especificación (RFC 9639): cabecera «fLaC», STREAMINFO, y tramas con cabecera
 * (CRC-8), una subtrama por canal (constante, verbatim o fija de orden 0 a 4 con residuo Rice) y CRC-16.
 * No calcula el MD5 de STREAMINFO (va a ceros = «desconocido», que todos los decodificadores admiten).
 * Todo entero: el mismo código corre en el anfitrión para las pruebas (-DAUDIO_HOST). */
#include "audio_conversor.h"

/* ------------------------------------------------------------ canales y frecuencia */

void cv_rem_iniciar(cv_rem_t *r, uint32_t taxa_in, uint32_t can_in, uint32_t taxa_out, uint32_t can_out)
{
    memset(r, 0, sizeof *r);
    r->taxa_in = taxa_in; r->taxa_out = taxa_out; r->can_in = can_in; r->can_out = can_out;
    r->paso = (uint32_t)(((uint64_t)taxa_in << 16) / taxa_out);
    r->bajando = taxa_in > taxa_out;
    r->primeiro = true;
    r->falta = r->paso;
}

unsigned cv_rem_max(const cv_rem_t *r, unsigned n)
{
    return (unsigned)((uint64_t)n * r->taxa_out / r->taxa_in) + 3;
}

static inline int16_t corta16(int32_t v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : (int16_t)v; }

unsigned cv_rem_procesar(cv_rem_t *r, const int16_t *in, unsigned n, int16_t *out)
{
    unsigned no = 0, co = r->can_out;
    for (unsigned i = 0; i < n; i++) {
        int32_t s[2];
        if (r->can_in == 2) {
            if (co == 2) { s[0] = in[2 * i]; s[1] = in[2 * i + 1]; }
            else { s[0] = ((int32_t)in[2 * i] + in[2 * i + 1]) >> 1; s[1] = s[0]; }
        } else { s[0] = s[1] = in[i]; }
        if (r->taxa_in == r->taxa_out) {
            for (unsigned c = 0; c < co; c++) out[no * co + c] = (int16_t)s[c];
            no++;
        } else if (!r->bajando) {
            /* subida: entre la muestra anterior y esta, en línea recta, cada «paso» */
            if (r->primeiro) { r->ant[0] = s[0]; r->ant[1] = s[1]; r->primeiro = false; r->fase = 0; }
            while (r->fase < 65536) {
                for (unsigned c = 0; c < co; c++) {
                    int64_t d = (int64_t)(s[c] - r->ant[c]) * r->fase;
                    out[no * co + c] = corta16(r->ant[c] + (int32_t)(d >> 16));
                }
                no++;
                r->fase += r->paso;
            }
            r->fase -= 65536;
            r->ant[0] = s[0]; r->ant[1] = s[1];
        } else {
            /* bajada: cada salida es el promedio de «paso» cuadros de entrada (con fracciones) */
            uint32_t w = 65536;
            while (w) {
                uint32_t t = w < r->falta ? w : r->falta;
                for (unsigned c = 0; c < co; c++) r->acc[c] += (int64_t)s[c] * t;
                w -= t;
                r->falta -= t;
                if (!r->falta) {
                    for (unsigned c = 0; c < co; c++) {
                        int64_t a = r->acc[c];
                        a = (a + (a >= 0 ? (int64_t)(r->paso >> 1) : -(int64_t)(r->paso >> 1))) / (int64_t)r->paso;
                        out[no * co + c] = corta16((int32_t)a);
                        r->acc[c] = 0;
                    }
                    no++;
                    r->falta = r->paso;
                }
            }
        }
    }
    return no;
}

/* ------------------------------------------------------------ WAV */

static void le32(uint8_t *d, uint32_t v) { d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); d[2] = (uint8_t)(v >> 16); d[3] = (uint8_t)(v >> 24); }
static void le16(uint8_t *d, uint32_t v) { d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); }

static void wav_cabecera(const cv_wav_t *w, uint8_t h[44])
{
    uint32_t bq = w->canais * w->bits / 8;
    memcpy(h, "RIFF", 4); le32(h + 4, 36 + w->bytes);
    memcpy(h + 8, "WAVEfmt ", 8); le32(h + 16, 16);
    le16(h + 20, 1); le16(h + 22, w->canais); le32(h + 24, w->taxa); le32(h + 28, w->taxa * bq);
    le16(h + 32, bq); le16(h + 34, w->bits);
    memcpy(h + 36, "data", 4); le32(h + 40, w->bytes);
}

int cv_wav_iniciar(cv_wav_t *w, cv_salida_t s, uint32_t taxa, uint32_t canais, uint32_t bits)
{
    if ((bits != 8 && bits != 16) || canais < 1 || canais > 2 || !taxa) return -1;
    memset(w, 0, sizeof *w);
    w->s = s; w->taxa = taxa; w->canais = canais; w->bits = bits;
    uint8_t h[44];
    wav_cabecera(w, h);
    return s.escribir(s.ctx, h, 44);
}

int cv_wav_escrever(cv_wav_t *w, const int16_t *pcm, unsigned quadros)
{
    unsigned n = quadros * w->canais;
    if (w->bits == 16) {
        static uint8_t b[8192];
        for (unsigned i = 0; i < n;) {
            unsigned m = n - i < 4096 ? n - i : 4096;
            for (unsigned k = 0; k < m; k++) le16(b + 2 * k, (uint16_t)pcm[i + k]);
            if (w->s.escribir(w->s.ctx, b, 2 * m) < 0) return -1;
            w->bytes += 2 * m;
            i += m;
        }
    } else {
        static uint8_t b[4096];
        for (unsigned i = 0; i < n;) {
            unsigned m = n - i < 4096 ? n - i : 4096;
            for (unsigned k = 0; k < m; k++) {
                int v = (pcm[i + k] + 128) >> 8;   /* redondeo al más cercano */
                b[k] = (uint8_t)((v > 127 ? 127 : v) + 128);
            }
            if (w->s.escribir(w->s.ctx, b, m) < 0) return -1;
            w->bytes += m;
            i += m;
        }
    }
    return 0;
}

int cv_wav_fechar(cv_wav_t *w)
{
    uint8_t h[44];
    wav_cabecera(w, h);
    return w->s.parchear(w->s.ctx, 0, h, 44);
}

/* ------------------------------------------------------------ FLAC */

#define FL_MAX_BS 4096
#define FL_MAX_ORDEM 4

struct cv_flac {
    cv_salida_t s;
    uint32_t taxa, canais;
    int nivel;
    unsigned bs;                 /* tamaño de bloque */
    unsigned max_ordem_part;
    unsigned min_ordem, max_ordem;   /* predictores fijos a probar */
    int estereo;                 /* 0 independiente, 1 solo medio/lado, 2 los cuatro modos */
    unsigned n;                  /* cuadros acumulados en el bloque */
    int32_t ch[2][FL_MAX_BS];
    int32_t aux[2][FL_MAX_BS];   /* medio y lado */
    int32_t res[FL_MAX_BS];
    uint8_t quadro[FL_MAX_BS * 2 * 3 + 256];
    unsigned qbytes;             /* bytes ya completos en quadro */
    uint64_t acc; unsigned nacc; /* acumulador de bits */
    uint32_t num_quadro;
    uint64_t total;              /* cuadros por canal escritos */
    uint32_t min_q, max_q;       /* tamaños de trama */
};

static uint8_t crc8_t[256];
static uint16_t crc16_t[256];
static bool crcs_listos;

static void crcs_iniciar(void)
{
    if (crcs_listos) return;
    for (unsigned i = 0; i < 256; i++) {
        unsigned c = i;
        for (int k = 0; k < 8; k++) c = c & 0x80 ? (c << 1) ^ 0x07 : c << 1;
        crc8_t[i] = (uint8_t)c;
        c = i << 8;
        for (int k = 0; k < 8; k++) c = c & 0x8000 ? (c << 1) ^ 0x8005 : c << 1;
        crc16_t[i] = (uint16_t)c;
    }
    crcs_listos = true;
}

static void b_poner(cv_flac_t *f, uint32_t v, unsigned bits)   /* bits <= 32 */
{
    if (!bits) return;
    f->acc = (f->acc << bits) | (v & (bits == 32 ? 0xFFFFFFFFu : ((1u << bits) - 1)));
    f->nacc += bits;
    while (f->nacc >= 8) {
        f->nacc -= 8;
        f->quadro[f->qbytes++] = (uint8_t)(f->acc >> f->nacc);
    }
}

static void b_sinal(cv_flac_t *f, int32_t v, unsigned bits) { b_poner(f, (uint32_t)v, bits); }

static void b_unario(cv_flac_t *f, uint32_t q)   /* q ceros y un uno */
{
    while (q >= 24) { b_poner(f, 0, 24); q -= 24; }
    b_poner(f, 1, q + 1);
}

static unsigned log2_piso(uint32_t v) { unsigned r = 0; while (v >>= 1) r++; return r; }

/* Residuo de un predictor fijo de orden o sobre x[0..n). Las primeras o posiciones no cuentan. */
static void residuo_fijo(const int32_t *x, unsigned n, unsigned o, int32_t *r)
{
    switch (o) {
    case 0: for (unsigned i = 0; i < n; i++) r[i] = x[i]; break;
    case 1: for (unsigned i = 1; i < n; i++) r[i] = x[i] - x[i - 1]; break;
    case 2: for (unsigned i = 2; i < n; i++) r[i] = x[i] - 2 * x[i - 1] + x[i - 2]; break;
    case 3: for (unsigned i = 3; i < n; i++) r[i] = x[i] - 3 * x[i - 1] + 3 * x[i - 2] - x[i - 3]; break;
    default: for (unsigned i = 4; i < n; i++) r[i] = x[i] - 4 * x[i - 1] + 6 * x[i - 2] - 4 * x[i - 3] + x[i - 4]; break;
    }
}

static inline uint32_t dobra(int32_t v) { return ((uint32_t)v << 1) ^ (uint32_t)(v >> 31); }

/* Bits aproximados de una partición de «cnt» valores doblados con suma «soma» y parámetro k. */
static uint32_t rice_bits(uint64_t soma, unsigned cnt, unsigned k)
{
    return cnt * (k + 1) + (uint32_t)(soma >> k);
}

static unsigned rice_k(uint64_t soma, unsigned cnt, unsigned kmax)
{
    if (!cnt) return 0;
    uint64_t media = (soma + cnt / 2) / cnt;
    unsigned k = media ? log2_piso((uint32_t)(media > 0xFFFFFFFFu ? 0xFFFFFFFFu : media)) : 0;
    if (k > kmax) k = kmax;
    unsigned k2 = k < kmax ? k + 1 : k;
    return rice_bits(soma, cnt, k2) < rice_bits(soma, cnt, k) ? k2 : k;
}

/* Mejor partición Rice del residuo r[o..n): devuelve los bits totales y su orden en *po. */
static uint32_t mejor_particion(const int32_t *r, unsigned n, unsigned o, unsigned max_po, unsigned *po, bool *rice2)
{
    static uint64_t soma[1u << 8];   /* partición más fina: 2^8 */
    uint32_t mejor = 0xFFFFFFFFu;
    unsigned lim = max_po > 8 ? 8 : max_po;
    while (lim > 0 && ((n % (1u << lim)) || (n >> lim) <= o)) lim--;
    /* sumas de la partición más fina */
    unsigned np = 1u << lim, tam = n >> lim;
    for (unsigned p = 0; p < np; p++) {
        unsigned ini = p * tam, fin = ini + tam;
        if (p == 0) ini = o;
        uint64_t s = 0;
        for (unsigned i = ini; i < fin; i++) s += dobra(r[i]);
        soma[p] = s;
    }
    for (int pord = (int)lim; pord >= 0; pord--) {
        unsigned parts = 1u << pord, t = n >> pord;
        uint32_t bits = 0;
        bool r2 = false;
        for (unsigned p = 0; p < parts; p++) {
            unsigned cnt = p == 0 ? t - o : t;
            unsigned k = rice_k(soma[p], cnt, 30);
            if (k > 14) r2 = true;
            bits += rice_bits(soma[p], cnt, k) + 4;
        }
        if (r2) bits += parts;   /* un bit más por parámetro (5 en vez de 4) */
        if (bits < mejor) { mejor = bits; *po = (unsigned)pord; *rice2 = r2; }
        if (pord > 0) for (unsigned p = 0; p < parts / 2; p++) soma[p] = soma[2 * p] + soma[2 * p + 1];
    }
    return mejor;
}

typedef struct { unsigned tipo; unsigned ordem; unsigned po; bool rice2; uint32_t bits; } plan_t;   /* tipo: 0 const, 1 verbatim, 2 fija */

/* Elige cómo codificar una subtrama de «bps» bits por muestra. */
static plan_t planear(cv_flac_t *f, const int32_t *x, unsigned n, unsigned bps)
{
    plan_t p = { 1, 0, 0, false, 8 + n * bps };   /* verbatim */
    bool igual = true;
    for (unsigned i = 1; i < n; i++) if (x[i] != x[0]) { igual = false; break; }
    if (igual) { p.tipo = 0; p.bits = 8 + bps; return p; }
    for (unsigned o = f->min_ordem; o <= f->max_ordem && o < n; o++) {
        unsigned po = 0; bool r2 = false;
        residuo_fijo(x, n, o, f->res);
        uint32_t b = 8 + o * bps + 2 + 4 + mejor_particion(f->res, n, o, f->max_ordem_part, &po, &r2);
        if (b < p.bits) { p.tipo = 2; p.ordem = o; p.po = po; p.rice2 = r2; p.bits = b; }
    }
    return p;
}

static void escribir_subquadro(cv_flac_t *f, const int32_t *x, unsigned n, unsigned bps, const plan_t *p)
{
    if (p->tipo == 0) { b_poner(f, 0, 8); b_sinal(f, x[0], bps); return; }
    if (p->tipo == 1) {
        b_poner(f, 1 << 1, 8);
        for (unsigned i = 0; i < n; i++) b_sinal(f, x[i], bps);
        return;
    }
    unsigned o = p->ordem;
    b_poner(f, (8u + o) << 1, 8);
    for (unsigned i = 0; i < o; i++) b_sinal(f, x[i], bps);
    residuo_fijo(x, n, o, f->res);
    b_poner(f, p->rice2 ? 1 : 0, 2);
    b_poner(f, p->po, 4);
    unsigned parts = 1u << p->po, t = n >> p->po, pb = p->rice2 ? 5 : 4;
    for (unsigned q = 0; q < parts; q++) {
        unsigned ini = q * t, fin = ini + t;
        if (q == 0) ini = o;
        uint64_t s = 0;
        for (unsigned i = ini; i < fin; i++) s += dobra(f->res[i]);
        unsigned k = rice_k(s, fin - ini, p->rice2 ? 30 : 14);
        b_poner(f, k, pb);
        for (unsigned i = ini; i < fin; i++) {
            uint32_t u = dobra(f->res[i]);
            b_unario(f, u >> k);
            b_poner(f, u & ((1u << k) - 1), k);
        }
    }
}

static int emitir_quadro(cv_flac_t *f, unsigned n)
{
    unsigned bps = 16;
    plan_t pl[2];
    int modo = 0;   /* 0..3 independiente, 8..10 los de estéreo */
    const int32_t *sub[2];
    unsigned bits_sub[2] = { bps, bps };
    if (f->canais == 1) {
        sub[0] = f->ch[0];
        pl[0] = planear(f, sub[0], n, bps);
    } else {
        int32_t *l = f->ch[0], *r = f->ch[1], *m = f->aux[0], *s = f->aux[1];
        plan_t pL = planear(f, l, n, bps), pR = planear(f, r, n, bps);
        uint32_t mejor = pL.bits + pR.bits;
        pl[0] = pL; pl[1] = pR; sub[0] = l; sub[1] = r;
        if (f->estereo) {
            for (unsigned i = 0; i < n; i++) { m[i] = (l[i] + r[i]) >> 1; s[i] = l[i] - r[i]; }
            plan_t pS = planear(f, s, n, bps + 1), pM = planear(f, m, n, bps);
            if (pM.bits + pS.bits < mejor) { mejor = pM.bits + pS.bits; modo = 10; pl[0] = pM; pl[1] = pS; sub[0] = m; sub[1] = s; bits_sub[0] = bps; bits_sub[1] = bps + 1; }
            if (f->estereo > 1) {
                if (pL.bits + pS.bits < mejor) { mejor = pL.bits + pS.bits; modo = 8; pl[0] = pL; pl[1] = pS; sub[0] = l; sub[1] = s; bits_sub[0] = bps; bits_sub[1] = bps + 1; }
                if (pS.bits + pR.bits < mejor) { mejor = pS.bits + pR.bits; modo = 9; pl[0] = pS; pl[1] = pR; sub[0] = s; sub[1] = r; bits_sub[0] = bps + 1; bits_sub[1] = bps; }
            }
        }
        if (modo == 0 && f->canais == 2) modo = 1;
    }
    unsigned asign = f->canais == 1 ? 0 : modo == 1 ? 1 : modo;   /* 0: mono, 1: dos canales, 8-10 */
    f->qbytes = 0; f->acc = 0; f->nacc = 0;
    /* cabecera */
    b_poner(f, 0xFFF8, 16);   /* sincronía, tamaño fijo */
    unsigned bcod = n == 1152 ? 3 : n == 4096 ? 12 : n <= 256 ? 6 : 7;
    b_poner(f, bcod, 4);
    b_poner(f, 0, 4);         /* frecuencia: la de STREAMINFO */
    b_poner(f, asign, 4);
    b_poner(f, 4, 3);         /* 16 bits */
    b_poner(f, 0, 1);
    uint32_t nq = f->num_quadro;   /* número de trama, UTF-8 */
    if (nq < 0x80) b_poner(f, nq, 8);
    else if (nq < 0x800) { b_poner(f, 0xC0 | (nq >> 6), 8); b_poner(f, 0x80 | (nq & 63), 8); }
    else if (nq < 0x10000) { b_poner(f, 0xE0 | (nq >> 12), 8); b_poner(f, 0x80 | ((nq >> 6) & 63), 8); b_poner(f, 0x80 | (nq & 63), 8); }
    else if (nq < 0x200000) { b_poner(f, 0xF0 | (nq >> 18), 8); b_poner(f, 0x80 | ((nq >> 12) & 63), 8); b_poner(f, 0x80 | ((nq >> 6) & 63), 8); b_poner(f, 0x80 | (nq & 63), 8); }
    else { b_poner(f, 0xF8 | (nq >> 24), 8); b_poner(f, 0x80 | ((nq >> 18) & 63), 8); b_poner(f, 0x80 | ((nq >> 12) & 63), 8); b_poner(f, 0x80 | ((nq >> 6) & 63), 8); b_poner(f, 0x80 | (nq & 63), 8); }
    if (bcod == 6) b_poner(f, n - 1, 8);
    else if (bcod == 7) b_poner(f, n - 1, 16);
    uint8_t c8 = 0;
    for (unsigned i = 0; i < f->qbytes; i++) c8 = crc8_t[c8 ^ f->quadro[i]];
    b_poner(f, c8, 8);
    for (unsigned c = 0; c < f->canais; c++) escribir_subquadro(f, sub[c], n, bits_sub[c], &pl[c]);
    if (f->nacc) b_poner(f, 0, 8 - f->nacc);   /* a byte */
    uint16_t c16 = 0;
    for (unsigned i = 0; i < f->qbytes; i++) c16 = (uint16_t)((c16 << 8) ^ crc16_t[(c16 >> 8) ^ f->quadro[i]]);
    b_poner(f, c16, 16);
    f->num_quadro++;
    f->total += n;
    if (f->qbytes < f->min_q) f->min_q = f->qbytes;
    if (f->qbytes > f->max_q) f->max_q = f->qbytes;
    return f->s.escribir(f->s.ctx, f->quadro, f->qbytes);
}

static void streaminfo(const cv_flac_t *f, uint8_t d[34])
{
    memset(d, 0, 34);
    d[0] = (uint8_t)(f->bs >> 8); d[1] = (uint8_t)f->bs;       /* tamaño mínimo y máximo de bloque */
    d[2] = (uint8_t)(f->bs >> 8); d[3] = (uint8_t)f->bs;
    uint32_t mn = f->min_q == 0xFFFFFFFFu ? 0 : f->min_q;
    d[4] = (uint8_t)(mn >> 16); d[5] = (uint8_t)(mn >> 8); d[6] = (uint8_t)mn;
    d[7] = (uint8_t)(f->max_q >> 16); d[8] = (uint8_t)(f->max_q >> 8); d[9] = (uint8_t)f->max_q;
    uint64_t v = ((uint64_t)f->taxa << 44) | ((uint64_t)(f->canais - 1) << 41) | ((uint64_t)15 << 36) | (f->total & 0xFFFFFFFFFull);
    for (int i = 0; i < 8; i++) d[10 + i] = (uint8_t)(v >> (56 - 8 * i));
}

cv_flac_t *cv_flac_iniciar(cv_salida_t s, uint32_t taxa, uint32_t canais, int nivel)
{
    if (canais < 1 || canais > 2 || taxa < 1 || taxa > 655350 || nivel < 0 || nivel > CV_FLAC_NIVEL_MAX) return NULL;
    cv_flac_t *f = aloca_zerado(sizeof *f);
    if (!f) return NULL;
    crcs_iniciar();
    f->s = s; f->taxa = taxa; f->canais = canais; f->nivel = nivel;
    static const struct { uint16_t bs; uint8_t omin, omax, part, est; } T[CV_FLAC_NIVEL_MAX + 1] = {
        { 1152, 1, 2, 3, 0 }, { 1152, 0, 4, 4, 1 }, { 1152, 0, 4, 5, 2 },
        { 4096, 1, 3, 5, 0 }, { 4096, 0, 4, 6, 1 }, { 4096, 0, 4, 6, 2 },
        { 4096, 0, 4, 7, 2 }, { 4096, 0, 4, 8, 2 }, { 4096, 0, 4, 8, 2 } };
    f->bs = T[nivel].bs; f->min_ordem = T[nivel].omin; f->max_ordem = T[nivel].omax;
    f->max_ordem_part = T[nivel].part; f->estereo = canais == 2 ? T[nivel].est : 0;
    f->min_q = 0xFFFFFFFFu;
    /* «fLaC» + bloque STREAMINFO (último bloque de metadatos, tipo 0, 34 bytes); se parchea al final */
    uint8_t h[42] = { 'f', 'L', 'a', 'C', 0x80, 0, 0, 34 };
    streaminfo(f, h + 8);
    if (s.escribir(s.ctx, h, 42) < 0) { libera(f); return NULL; }
    return f;
}

int cv_flac_escrever(cv_flac_t *f, const int16_t *pcm, unsigned quadros)
{
    for (unsigned i = 0; i < quadros; i++) {
        for (unsigned c = 0; c < f->canais; c++) f->ch[c][f->n] = pcm[i * f->canais + c];
        if (++f->n == f->bs) {
            if (emitir_quadro(f, f->n) < 0) return -1;
            f->n = 0;
        }
    }
    return 0;
}

int cv_flac_fechar(cv_flac_t *f)
{
    int r = 0;
    if (f->n && emitir_quadro(f, f->n) < 0) r = -1;
    uint8_t d[34];
    streaminfo(f, d);
    if (r == 0 && f->s.parchear(f->s.ctx, 8, d, 34) < 0) r = -1;
    libera(f);
    return r;
}
