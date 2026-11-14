// SPDX-License-Identifier: GPL-2.0-or-later
/* audio_formato.c - abrir archivos de sonido y entregar PCM de 16 bits (mejora 053, D098).
 *
 * Aquí viven: el lector con búfer sobre el archivo, la detección del formato
 * por el contenido, el WAV (PCM de 8/16/24/32 bits y la cabecera del IMA ADPCM) y
 * la interfaz común. El MP3, el FLAC y el ADPCM están en audio_mp3.c,
 * audio_flac.c y audio_adpcm.c. */
#include "audio_interno.h"
#include "audio_midi.h"

static audio_arquivo_t g_arq;

/* ------------------------------------------------------------ lector con búfer */

void fl_iniciar(fluxo_t *f, int fd)
{
    f->fd = fd;
    f->ini = f->fim = 0;
    f->pos = 0;
    f->fim_arq = false;
    int t = posicionar(fd, 0, POS_FIM);
    f->tam = t > 0 ? (uint32_t)t : 0;
    posicionar(fd, 0, POS_INICIO);
}

/* Llena el búfer hasta tener n bytes seguidos (o llegar al fin del archivo). */
uint32_t fl_garantir(fluxo_t *f, uint32_t n)
{
    if (n > FL_TAM) n = FL_TAM;
    if (f->fim - f->ini >= n) return f->fim - f->ini;
    if (f->ini) {
        memmove(f->buf, f->buf + f->ini, f->fim - f->ini);
        f->fim -= f->ini;
        f->ini = 0;
    }
    while (f->fim < n && !f->fim_arq) {
        uint32_t quer = FL_TAM - f->fim;
        if (quer > 4096) quer = 4096;   /* trozos pequeños: el pendrive lee con las interrupciones desactivadas (D089) */
        int r = ler(f->fd, f->buf + f->fim, quer);
        if (r <= 0) f->fim_arq = true;
        else f->fim += (uint32_t)r;
    }
    return f->fim - f->ini;
}

uint32_t fl_ler(fluxo_t *f, void *dst, uint32_t n)
{
    uint8_t *d = dst;
    uint32_t total = 0;
    while (n) {
        uint32_t t = fl_garantir(f, n > FL_TAM ? FL_TAM : n);
        if (!t) break;
        if (t > n) t = n;
        memcpy(d, f->buf + f->ini, t);
        fl_consumir(f, t);
        d += t;
        n -= t;
        total += t;
    }
    return total;
}

bool fl_posicionar(fluxo_t *f, uint32_t pos)
{
    if (pos >= f->pos && pos - f->pos <= f->fim - f->ini) {
        uint32_t d = pos - f->pos;
        fl_consumir(f, d);
        return true;
    }
    if (posicionar(f->fd, (int)pos, POS_INICIO) < 0) return false;
    f->ini = f->fim = 0;
    f->pos = pos;
    f->fim_arq = false;
    return true;
}

bool fl_pular(fluxo_t *f, uint32_t n)
{
    return fl_posicionar(f, f->pos + n);
}

int fl_byte(fluxo_t *f)
{
    if (f->ini >= f->fim && !fl_garantir(f, 1)) return -1;
    int b = f->buf[f->ini];
    fl_consumir(f, 1);
    return b;
}

/* ------------------------------------------------------------ utilidades */

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t le16(const uint8_t *p) { return p[0] | p[1] << 8; }

void audio_copiar_etiqueta(char *dst, unsigned max, const char *src, unsigned n)
{
    unsigned i = 0;
    for (; i < n && i + 1 < max && src[i]; i++) dst[i] = src[i];
    while (i && dst[i - 1] == ' ') i--;   /* el ID3v1 completa con espacios */
    dst[i] = 0;
}

void audio_etiqueta_utf8(char *dst, unsigned max, const uint8_t *s, unsigned n)
{
    unsigned o = 0;
    for (unsigned i = 0; i < n && s[i] && o + 1 < max;) {
        uint32_t c = s[i];
        unsigned extra = 0;
        if (c >= 0xF0) { c &= 7; extra = 3; }
        else if (c >= 0xE0) { c &= 15; extra = 2; }
        else if (c >= 0xC0) { c &= 31; extra = 1; }
        i++;
        while (extra-- && i < n && (s[i] & 0xC0) == 0x80) c = c << 6 | (s[i++] & 63);
        dst[o++] = c < 256 ? (char)c : '?';
    }
    while (o && dst[o - 1] == ' ') o--;
    dst[o] = 0;
}

void audio_etiqueta_utf16(char *dst, unsigned max, const uint8_t *s, unsigned n, bool be)
{
    unsigned o = 0, i = 0;
    if (n >= 2 && ((s[0] == 0xFF && s[1] == 0xFE) || (s[0] == 0xFE && s[1] == 0xFF))) {
        be = s[0] == 0xFE;
        i = 2;
    }
    for (; i + 1 < n && o + 1 < max; i += 2) {
        uint32_t c = be ? (uint32_t)(s[i] << 8 | s[i + 1]) : (uint32_t)(s[i + 1] << 8 | s[i]);
        if (!c) break;
        dst[o++] = c < 256 ? (char)c : '?';
    }
    dst[o] = 0;
}

const char *audio_erro_texto(int erro)
{
    switch (erro) {
    case AUDIO_ERRO_FORMATO: return "no es un archivo de sonido conocido (WAV, MP3, FLAC o MIDI)";
    case AUDIO_ERRO_NAO_SUPORTADO: return "variante del formato no soportada";
    case AUDIO_ERRO_CORROMPIDO: return "archivo dañado";
    case AUDIO_ERRO_TRUNCADO: return "archivo cortado (termina antes de lo esperado)";
    case AUDIO_ERRO_OCUPADO: return "ya hay un archivo de sonido abierto";
    case ERRO_NAO_EXISTE: return "el archivo no existe";
    case ERRO_SEM_MEMORIA: return "sin memoria";
    default: return "error al leer el archivo";
    }
}

/* Los 4 bytes de cabecera de un cuadro MP3 Layer III válido (sin mirar más allá). */
static bool cabecalho_mp3(const uint8_t *p)
{
    if (p[0] != 0xFF || (p[1] & 0xE0) != 0xE0) return false;
    int versao = (p[1] >> 3) & 3, camada = (p[1] >> 1) & 3;
    if (versao == 1 || camada != 1) return false;
    if ((p[2] >> 4) == 15 || (p[2] >> 4) == 0 || ((p[2] >> 2) & 3) == 3) return false;
    return true;
}

int audio_detectar(const uint8_t *b, unsigned n)
{
    if (n >= 12 && memcmp(b, "RIFF", 4) == 0 && memcmp(b + 8, "WAVE", 4) == 0) return AUDIO_FMT_WAV;
    if (n >= 4 && memcmp(b, "fLaC", 4) == 0) return AUDIO_FMT_FLAC;
    if (n >= 14 && memcmp(b, "MThd", 4) == 0) return AUDIO_FMT_MIDI;
    if (n >= 10 && memcmp(b, "ID3", 3) == 0) return AUDIO_FMT_MP3;   /* al abrir se confirma (puede ser un FLAC con ID3) */
    if (n >= 4 && cabecalho_mp3(b)) return AUDIO_FMT_MP3;
    return AUDIO_FMT_NENHUM;
}

/* ------------------------------------------------------------ WAV */

static void info_duracao(audio_info_t *i)
{
    if (i->quadros && i->taxa)
        i->duracao_ms = i->quadros / i->taxa * 1000 + i->quadros % i->taxa * 1000 / i->taxa;
}

static int wav_abrir(audio_arquivo_t *a)
{
    fluxo_t *f = &a->fl;
    uint8_t cab[12], ch[8], fmt[40];
    if (fl_ler(f, cab, 12) != 12) return AUDIO_ERRO_TRUNCADO;
    uint32_t tipo = 0, canais = 0, taxa = 0, bits = 0, bloco = 0, spb = 0;
    bool tem_fmt = false;
    for (;;) {
        if (fl_ler(f, ch, 8) != 8) return tem_fmt ? AUDIO_ERRO_TRUNCADO : AUDIO_ERRO_FORMATO;
        uint32_t tam = le32(ch + 4);
        if (memcmp(ch, "fmt ", 4) == 0 && tam >= 16) {
            uint32_t n = tam > sizeof fmt ? sizeof fmt : tam;
            if (fl_ler(f, fmt, n) != n) return AUDIO_ERRO_TRUNCADO;
            if (!fl_pular(f, tam - n + (tam & 1))) return AUDIO_ERRO_TRUNCADO;
            tipo = le16(fmt);
            if (tipo == 0xFFFE && n >= 26) tipo = le16(fmt + 24);   /* extensible: el subformato */
            canais = le16(fmt + 2);
            taxa = le32(fmt + 4);
            bloco = le16(fmt + 12);
            bits = le16(fmt + 14);
            if (n >= 20) spb = le16(fmt + 18);
            tem_fmt = true;
        } else if (memcmp(ch, "data", 4) == 0) {
            uint32_t dados = tam;
            if (!tem_fmt) return AUDIO_ERRO_FORMATO;
            if (f->tam && f->pos + dados > f->tam) dados = f->tam - f->pos;   /* tamaño de un archivo cortado o de grabación en curso */
            if (canais < 1 || canais > 2 || taxa < 4000 || taxa > 96000) return AUDIO_ERRO_NAO_SUPORTADO;
            a->inicio_dados = f->pos;
            a->info.taxa = taxa;
            a->info.canais = canais;
            a->info.sem_perdas = true;
            if (tipo == 1) {
                if (bits != 8 && bits != 16 && bits != 24 && bits != 32) return AUDIO_ERRO_NAO_SUPORTADO;
                a->fmt = AUDIO_FMT_WAV;
                a->info.formato = AUDIO_FMT_WAV;
                a->info.nome_formato = "WAV PCM";
                a->info.bits_origem = bits;
                a->pcm_bits = bits;
                a->pcm_bq = canais * bits / 8;
                a->pcm_restam = dados / a->pcm_bq * a->pcm_bq;
                a->info.quadros = a->pcm_restam / a->pcm_bq;
                a->info.kbps = taxa * canais * bits / 1000;
            } else if (tipo == 0x11) {
                if (bits != 4) return AUDIO_ERRO_NAO_SUPORTADO;
                a->fmt = AUDIO_FMT_ADPCM;
                a->info.formato = AUDIO_FMT_ADPCM;
                a->info.nome_formato = "WAV IMA ADPCM";
                a->info.bits_origem = 4;
                a->info.sem_perdas = false;
                int r = adpcm_abrir(a, canais, taxa, bloco, spb, dados);
                if (r < 0) return r;
            } else
                return AUDIO_ERRO_NAO_SUPORTADO;
            info_duracao(&a->info);
            return 0;
        } else if (!fl_pular(f, tam + (tam & 1)))
            return AUDIO_ERRO_TRUNCADO;
    }
}

static int pcm_ler(audio_arquivo_t *a, int16_t *pcm, unsigned n)
{
    uint8_t tmp[4096];
    unsigned bq = a->pcm_bq, feito = 0;
    while (feito < n && a->pcm_restam) {
        unsigned q = sizeof tmp / bq;
        if (q > n - feito) q = n - feito;
        if (q * bq > a->pcm_restam) q = a->pcm_restam / bq;
        uint32_t got = fl_ler(&a->fl, tmp, q * bq);
        q = got / bq;
        if (!q) { a->pcm_restam = 0; return feito ? (int)feito : AUDIO_ERRO_TRUNCADO; }
        unsigned amostras = q * a->info.canais, by = a->pcm_bits / 8;
        for (unsigned k = 0; k < amostras; k++) {
            const uint8_t *p = tmp + k * by;
            int v;
            if (by == 1) v = ((int)p[0] - 128) << 8;
            else if (by == 2) v = (int16_t)(p[0] | p[1] << 8);
            else if (by == 3) v = (int16_t)(p[1] | p[2] << 8);   /* trunca a 16 bits (como el FLAC de 24) */
            else v = (int16_t)(p[2] | p[3] << 8);
            pcm[feito * a->info.canais + k] = (int16_t)v;
        }
        a->pcm_restam -= q * bq;
        feito += q;
    }
    return (int)feito;
}

/* ------------------------------------------------------------ MIDI */

/* El MIDI lo lee y sintetiza audio_midi.c; aquí solo se rellenan los datos comunes. */
static int midi_abrir_arq(audio_arquivo_t *a, const char *caminho)
{
    fechar(a->fd);
    a->fd = -1;
    int erro;
    a->midi = midi_abrir(caminho, &erro);
    if (!a->midi) return erro < 0 ? erro : AUDIO_ERRO_CORROMPIDO;
    const midi_info_t *m = midi_info(a->midi);
    a->fmt = AUDIO_FMT_MIDI;
    a->info.formato = AUDIO_FMT_MIDI;
    a->info.nome_formato = m->karaoke ? "MIDI karaoke" : m->formato ? "MIDI (formato 1)" : "MIDI (formato 0)";
    a->info.taxa = MIDI_TAXA;
    a->info.canais = 2;
    a->info.bits_origem = 0;
    a->info.duracao_ms = m->duracao_ms;
    a->info.quadros = m->duracao_ms / 1000 * MIDI_TAXA + m->duracao_ms % 1000 * MIDI_TAXA / 1000;
    a->info.sem_perdas = false;
    audio_copiar_etiqueta(a->info.titulo, sizeof a->info.titulo, m->titulo, sizeof m->titulo);
    audio_copiar_etiqueta(a->info.artista, sizeof a->info.artista, m->autor[0] ? m->autor : m->copyright, sizeof m->autor);
    return 0;
}

/* ------------------------------------------------------------ interfaz común */

audio_arquivo_t *audio_arquivo_abrir(const char *caminho, int *erro)
{
    int dummy;
    if (!erro) erro = &dummy;
    audio_arquivo_t *a = &g_arq;
    if (a->em_uso) { *erro = AUDIO_ERRO_OCUPADO; return NULL; }
    memset(a, 0, sizeof *a);
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) { *erro = fd; return NULL; }
    a->fd = fd;
    fl_iniciar(&a->fl, fd);
    a->em_uso = true;
    uint32_t t = fl_garantir(&a->fl, 16);
    int fmt = audio_detectar(fl_ptr(&a->fl), t);
    int r = AUDIO_ERRO_FORMATO;
    if (fmt == AUDIO_FMT_WAV) r = wav_abrir(a);
    else if (fmt == AUDIO_FMT_FLAC) r = flac_abrir(a);
    else if (fmt == AUDIO_FMT_MIDI) r = midi_abrir_arq(a, caminho);
    else if (fmt == AUDIO_FMT_MP3) {
        r = mp3_abrir(a);
        if (r == AUDIO_ERRO_FORMATO && a->inicio_dados) {
            /* ID3v2 y después un FLAC (ocurre con etiquetadores) */
            fl_posicionar(&a->fl, a->inicio_dados);
            fl_garantir(&a->fl, 4);
            if (audio_detectar(fl_ptr(&a->fl), 4) == AUDIO_FMT_FLAC) r = flac_abrir(a);
        }
    }
    if (r < 0) {
        audio_arquivo_fechar(a);
        *erro = r;
        return NULL;
    }
    *erro = 0;
    return a;
}

const audio_info_t *audio_arquivo_info(const audio_arquivo_t *a)
{
    return &a->info;
}

int audio_arquivo_ler(audio_arquivo_t *a, int16_t *pcm, unsigned quadros)
{
    if (!a || !a->em_uso || !quadros) return 0;
    int r;
    switch (a->fmt) {
    case AUDIO_FMT_WAV: r = pcm_ler(a, pcm, quadros); break;
    case AUDIO_FMT_ADPCM: r = adpcm_ler(a, pcm, quadros); break;
    case AUDIO_FMT_FLAC: r = flac_ler(a, pcm, quadros); break;
    case AUDIO_FMT_MP3: r = mp3_ler(a, pcm, quadros); break;
    case AUDIO_FMT_MIDI: r = midi_ler(a->midi, pcm, quadros); break;
    default: r = AUDIO_ERRO_FORMATO;
    }
    if (r > 0) a->entregues += (uint32_t)r;
    return r;
}

uint32_t audio_proporcao(uint32_t n, uint32_t q, uint32_t total)
{
    if (!total || q >= total) return n;
    uint32_t f = (q >> 8) * 1024 / ((total >> 8) | 1);   /* fracción en 1/1024 */
    if (f > 1024) f = 1024;
    return (n >> 10) * f + (n & 1023) * f / 1024;
}

uint32_t audio_arquivo_posicao(const audio_arquivo_t *a)
{
    return a ? a->entregues : 0;
}

int audio_arquivo_buscar(audio_arquivo_t *a, uint32_t ms)
{
    if (!a || !a->em_uso) return AUDIO_ERRO_FORMATO;
    uint32_t taxa = a->info.taxa, total = a->info.quadros;
    uint32_t q = ms / 1000 * taxa + ms % 1000 * taxa / 1000;
    if (total && q > total) q = total;
    if (a->fmt == AUDIO_FMT_MIDI) {
        a->entregues = ms / 1000 * taxa + ms % 1000 * taxa / 1000;
        return midi_buscar(a->midi, ms);
    }
    a->pend = 0;
    a->entregues = q;   /* FLAC lo corrige con la posición real del cuadro */
    int r;
    switch (a->fmt) {
    case AUDIO_FMT_WAV:
        a->pcm_restam = (total - q) * a->pcm_bq;
        r = fl_posicionar(&a->fl, a->inicio_dados + q * a->pcm_bq) ? 0 : AUDIO_ERRO_TRUNCADO;
        break;
    case AUDIO_FMT_ADPCM: r = adpcm_buscar(a, q); break;
    case AUDIO_FMT_FLAC: r = flac_buscar(a, q); break;
    case AUDIO_FMT_MP3: r = mp3_buscar(a, q); break;
    default: r = AUDIO_ERRO_FORMATO;
    }
    return r;
}

void audio_arquivo_fechar(audio_arquivo_t *a)
{
    if (!a || !a->em_uso) return;
    if (a->midi) { midi_fechar(a->midi); a->midi = NULL; }
    if (a->fd >= 0) fechar(a->fd);
    a->em_uso = false;
}
