// SPDX-License-Identifier: GPL-3.0-or-later
/* convertir-audio - el Conversor de audio (D339).
 *
 * Con dos o más nombres es una orden:
 *     convertir-audio entrada.wav salida.mp3 [--kbps 192] [--nivel 5] [--taxa 22050] [--canales 1] [--bits 8] [-y]
 *   (el formato de salida sale de la extensión: .mp3, .flac o .wav; con varias entradas, la última es la carpeta
 *    de destino y el formato se pide con --formato mp3|flac|wav).
 * Sin nombres, o con uno solo (Archivos: «Abrir con»), abre la ventana: lista de archivos, formato y calidad,
 * frecuencia, canales, carpeta de destino, barra de progreso y Cancelar.
 *
 * Entrada: todo lo que decodifica la libsagui (WAV, WAV IMA ADPCM, MP3, FLAC, MIDI; audio_formato.h).
 * Salida:
 *   MP3   shine (userland/terceros/shine/, LGPL-2.0 o posterior), tasa de bits constante de 64 a 320 kbps. Solo si
 *         se compiló con CONVERSOR_SHINE (no entra en la variante sin GPL);
 *   FLAC  codificador propio (audio_conversor.c), sin pérdida, niveles de compresión 0 a 8;
 *   WAV   PCM de 8 o 16 bits.
 * La frecuencia (44,1; 22,05 y 11,025 kHz) y los canales se cambian con enteros (audio_conversor.c).
 * La conversión va por bloques entre evento y evento, así que Cancelar responde al instante y no hace falta un
 * hilo (los decodificadores solo admiten un archivo abierto a la vez). */
#include "sagui.h"
#include "gui.h"
#include "audio_formato.h"
#include "audio_conversor.h"
#ifdef CONVERSOR_SHINE
#include "layer3.h"
#endif

#define FMT_MP3  0
#define FMT_FLAC 1
#define FMT_WAV16 2
#define FMT_WAV8 3

#define MAX_ARQ 64
#define PEDIDO 4096          /* cuadros que se piden al decodificador cada vez */
#define SINK_TAM 32768

typedef struct { int fmt, kbps, nivel, taxa, canales; bool sobrescribir; } opc_t;   /* taxa y canales: 0 = los del original */

/* ------------------------------------------------------------ utilidades */

static const int KBPS[] = { 64, 96, 128, 160, 192, 256, 320 };
#define N_KBPS ((int)(sizeof KBPS / sizeof KBPS[0]))

static const char *nome_curto(const char *c)
{
    const char *b = acha_ultimo_char(c, '/');
    return b ? b + 1 : c;
}

static bool extensao_audio(const char *c)
{
    unsigned n = (unsigned)tam_texto(c), p = n;
    while (p && c[p - 1] != '.' && c[p - 1] != '/') p--;
    if (!p || c[p - 1] != '.') return false;
    char e[5] = { 0 };
    for (unsigned k = 0; k < 4 && p + k < n; k++) e[k] = (char)(c[p + k] | 0x20);
    return compara_texto(e, "wav") == 0 || compara_texto(e, "mp3") == 0 || compara_texto(e, "flac") == 0 ||
           compara_texto(e, "mid") == 0 || compara_texto(e, "midi") == 0 || compara_texto(e, "kar") == 0;
}

static const char *ext_de(int fmt) { return fmt == FMT_MP3 ? "mp3" : fmt == FMT_FLAC ? "flac" : "wav"; }

/* Nombre del destino: carpeta + nombre del origen sin extensión + la extensión del formato. */
static void destino_de(char *d, size_t max, const char *pasta, const char *origem, int fmt)
{
    char nome[120];
    copia_texto(nome, nome_curto(origem), sizeof nome);
    char *p = acha_ultimo_char(nome, '.');
    if (p && p != nome) *p = 0;
    junta_texto(nome, ".", sizeof nome);
    junta_texto(nome, ext_de(fmt), sizeof nome);
    caminho_juntar(d, max, pasta, nome);
}

static bool existe(const char *c)
{
    info_arquivo_t i;
    return info_arquivo(c, &i) >= 0;
}

/* Las frecuencias y tasas que admite el MP3 (MPEG-1: 32/44,1/48 kHz, 32 a 320; MPEG-2: 16/22,05/24 kHz, 8 a 160;
 * MPEG-2.5: 8/11,025/12 kHz, 8 a 64). Ajusta la tasa pedida a la más cercana permitida. */
#ifdef CONVERSOR_SHINE
static int kbps_valido(int taxa, int kbps)
{
    int max = taxa >= 32000 ? 320 : taxa >= 16000 ? 160 : 64;
    if (kbps > max) kbps = max;
    return kbps;
}
#endif

#ifdef CONVERSOR_SHINE
static bool taxa_mp3_ok(uint32_t t)
{
    return t == 44100 || t == 48000 || t == 32000 || t == 22050 || t == 24000 || t == 16000 || t == 11025 || t == 12000 || t == 8000;
}
#endif

/* ------------------------------------------------------------ el trabajo de una conversión */

typedef struct {
    audio_arquivo_t *arq;
    const audio_info_t *inf;
    int fmt;
    uint32_t taxa_out, can_out;
    cv_rem_t rem;
    cv_wav_t wav;
    cv_flac_t *flac;
#ifdef CONVERSOR_SHINE
    shine_t shine;
    int pasada;                  /* cuadros por canal que pide shine cada vez */
    uint8_t fr_h[4];             /* seguimiento de las tramas que salen (mp3_emitir) */
    unsigned fr_nh, fr_rest;
    int16_t *mp3_buf;
    unsigned mp3_n;
#endif
    int fd;
    uint8_t *sink;
    unsigned sink_n;
    int16_t *entrada, *saida;
    unsigned saida_cap;
    uint32_t total_q, leidos_q;  /* cuadros del origen (total 0 = desconocido) */
    char destino[256];
    char erro[160];
} trab_t;

static int sink_vaciar(trab_t *t)
{
    unsigned off = 0;
    while (off < t->sink_n) {
        int r = escrever(t->fd, t->sink + off, t->sink_n - off);
        if (r <= 0) return -1;
        off += (unsigned)r;
    }
    t->sink_n = 0;
    return 0;
}

static int sink_escribir(void *ctx, const void *d, unsigned n)
{
    trab_t *t = ctx;
    const uint8_t *p = d;
    while (n) {
        unsigned m = SINK_TAM - t->sink_n < n ? SINK_TAM - t->sink_n : n;
        memcpy(t->sink + t->sink_n, p, m);
        t->sink_n += m; p += m; n -= m;
        if (t->sink_n == SINK_TAM && sink_vaciar(t) < 0) return -1;
    }
    return 0;
}

static int sink_parchear(void *ctx, unsigned desde, const void *d, unsigned n)
{
    trab_t *t = ctx;
    if (sink_vaciar(t) < 0) return -1;
    if (posicionar(t->fd, (int)desde, POS_INICIO) < 0) return -1;
    if (escrever(t->fd, d, n) != (int)n) return -1;
    return posicionar(t->fd, 0, POS_FIM) < 0 ? -1 : 0;
}

static void trab_liberar(trab_t *t)
{
    if (t->arq) { audio_arquivo_fechar(t->arq); t->arq = NULL; }
    if (t->fd >= 0) { fechar(t->fd); t->fd = -1; }
#ifdef CONVERSOR_SHINE
    if (t->shine) { shine_close(t->shine); t->shine = NULL; }
    if (t->mp3_buf) { libera(t->mp3_buf); t->mp3_buf = NULL; }
#endif
    if (t->flac) { libera(t->flac); t->flac = NULL; }
    if (t->sink) { libera(t->sink); t->sink = NULL; }
    if (t->entrada) { libera(t->entrada); t->entrada = NULL; }
    if (t->saida) { libera(t->saida); t->saida = NULL; }
}

/* Abre origen y destino y prepara el codificador. 0 = bien; si no, t->erro explica. */
static int trab_abrir(trab_t *t, const char *origem, const char *destino, const opc_t *o)
{
    memset(t, 0, sizeof *t);
    t->fd = -1;
    int erro;
    t->arq = audio_arquivo_abrir(origem, &erro);
    if (!t->arq) {
        formatar_texto(t->erro, sizeof t->erro, _("%s: %s"), nome_curto(origem), audio_erro_texto(erro));
        return -1;
    }
    t->inf = audio_arquivo_info(t->arq);
    t->fmt = o->fmt;
    t->taxa_out = o->taxa ? (uint32_t)o->taxa : t->inf->taxa;
    t->can_out = o->canales ? (uint32_t)o->canales : t->inf->canais;
    if (t->can_out < 1 || t->can_out > 2) t->can_out = 2;
    int kbps = o->kbps;
    (void)kbps;
    if (t->fmt == FMT_MP3) {
#ifdef CONVERSOR_SHINE
        if (!taxa_mp3_ok(t->taxa_out)) {   /* MP3 solo admite unas frecuencias: la más cercana de las habituales */
            t->taxa_out = t->taxa_out >= 38000 ? 44100 : t->taxa_out >= 19000 ? 22050 : 11025;
        }
        kbps = kbps_valido((int)t->taxa_out, kbps);
#else
        copia_texto(t->erro, _("Esta versión no incluye el codificador MP3"), sizeof t->erro);
        trab_liberar(t);
        return -1;
#endif
    }
    t->total_q = t->inf->quadros ? t->inf->quadros : (uint32_t)((uint64_t)t->inf->duracao_ms * t->inf->taxa / 1000);
    copia_texto(t->destino, destino, sizeof t->destino);
    t->fd = abrir(destino, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    t->sink = aloca(SINK_TAM);
    t->entrada = aloca(PEDIDO * 2 * sizeof(int16_t));
    cv_rem_iniciar(&t->rem, t->inf->taxa, t->inf->canais, t->taxa_out, t->can_out);
    t->saida_cap = cv_rem_max(&t->rem, PEDIDO);
    t->saida = aloca((size_t)t->saida_cap * 2 * sizeof(int16_t));
    if (t->fd < 0 || !t->sink || !t->entrada || !t->saida) {
        formatar_texto(t->erro, sizeof t->erro, _("No se puede crear %s"), destino);
        trab_liberar(t);
        return -1;
    }
    cv_salida_t s = { sink_escribir, sink_parchear, t };
    int r = 0;
    if (t->fmt == FMT_FLAC) {
        t->flac = cv_flac_iniciar(s, t->taxa_out, t->can_out, o->nivel);
        if (!t->flac) r = -1;
    } else if (t->fmt == FMT_WAV16 || t->fmt == FMT_WAV8) {
        r = cv_wav_iniciar(&t->wav, s, t->taxa_out, t->can_out, t->fmt == FMT_WAV8 ? 8 : 16);
#ifdef CONVERSOR_SHINE
    } else {
        shine_config_t c;
        memset(&c, 0, sizeof c);
        shine_set_config_mpeg_defaults(&c.mpeg);
        c.mpeg.bitr = kbps;
        c.mpeg.mode = t->can_out == 1 ? MONO : STEREO;
        c.wave.channels = t->can_out == 1 ? PCM_MONO : PCM_STEREO;
        c.wave.samplerate = (int)t->taxa_out;
        t->shine = shine_initialise(&c);
        if (!t->shine) r = -1;
        else {
            t->pasada = shine_samples_per_pass(t->shine);
            t->mp3_buf = aloca((size_t)t->pasada * 2 * sizeof(int16_t));
            if (!t->mp3_buf) r = -1;
        }
#endif
    }
    if (r < 0) {
        copia_texto(t->erro, _("No se pudo iniciar el codificador"), sizeof t->erro);
        trab_liberar(t);
        apagar(destino);
        return -1;
    }
    return 0;
}

#ifdef CONVERSOR_SHINE
/* Lo que devuelve shine no coincide con las tramas MP3 (da los bytes que lleva escritos). La última trama sale a
 * veces un byte corta (su cabecera declara el relleno y el último byte no se escribe): se sigue el flujo trama a
 * trama y, al acabar, se completa con ceros; si no, los decodificadores estrictos (el propio) dan «archivo
 * cortado» al final. */
static unsigned mp3_longitud(const uint8_t h[4])
{
    static const short BR1[16] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 };
    static const short BR2[16] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 };
    static const int SR[3] = { 44100, 48000, 32000 };
    if (h[0] != 0xFF || (h[1] & 0xE0) != 0xE0) return 0;
    int ver = (h[1] >> 3) & 3, ix = (h[2] >> 2) & 3;   /* ver: 3 = MPEG-1, 2 = MPEG-2, 0 = MPEG-2.5 */
    if (ix == 3 || ver == 1) return 0;
    unsigned br = (unsigned)((ver == 3 ? BR1 : BR2)[h[2] >> 4]) * 1000, sr = (unsigned)SR[ix] >> (ver == 3 ? 0 : ver == 2 ? 1 : 2);
    return sr && br ? (ver == 3 ? 144u : 72u) * br / sr + ((h[2] >> 1) & 1) : 0;
}

static int mp3_emitir(trab_t *t, const unsigned char *d, unsigned w)
{
    if (sink_escribir(t, d, w) < 0) return -1;
    while (w) {
        if (t->fr_rest) {
            unsigned m = w < t->fr_rest ? w : t->fr_rest;
            t->fr_rest -= m; d += m; w -= m;
        } else {
            while (w && t->fr_nh < 4) t->fr_h[t->fr_nh++] = *d++, w--;
            if (t->fr_nh == 4) {
                unsigned len = mp3_longitud(t->fr_h);
                t->fr_rest = len > 4 ? len - 4 : 0;
                t->fr_nh = 0;
                if (!len) return 0;   /* no es una cabecera: no se sigue */
            }
        }
    }
    return 0;
}

static int mp3_completar(trab_t *t)
{
    static const uint8_t ceros[4];
    if (t->fr_rest && t->fr_rest < 4) { unsigned r = t->fr_rest; t->fr_rest = 0; return sink_escribir(t, ceros, r); }
    return 0;
}
#endif

#ifdef CONVERSOR_SHINE
/* Entrega a shine los cuadros de salida, de «pasada» en «pasada». */
static int mp3_alimentar(trab_t *t, const int16_t *pcm, unsigned n)
{
    unsigned c = t->can_out;
    while (n) {
        unsigned falta = (unsigned)t->pasada - t->mp3_n, m = n < falta ? n : falta;
        memcpy(t->mp3_buf + t->mp3_n * c, pcm, m * c * sizeof(int16_t));
        t->mp3_n += m; pcm += m * c; n -= m;
        if (t->mp3_n == (unsigned)t->pasada) {
            int w = 0;
            unsigned char *d = shine_encode_buffer_interleaved(t->shine, t->mp3_buf, &w);
            if (d && w > 0 && mp3_emitir(t, d, (unsigned)w) < 0) return -1;
            t->mp3_n = 0;
        }
    }
    return 0;
}
#endif

/* Un bloque: 1 = queda más, 0 = terminó bien, <0 = error (t->erro). */
static int trab_paso(trab_t *t)
{
    int n = audio_arquivo_ler(t->arq, t->entrada, PEDIDO);
    if (n < 0) {
        formatar_texto(t->erro, sizeof t->erro, _("%s: %s"), nome_curto(t->destino), audio_erro_texto(n));
        return -1;
    }
    if (n == 0) {
        int r = 0;
#ifdef CONVERSOR_SHINE
        if (t->fmt == FMT_MP3) {
            if (t->mp3_n) {   /* el último trozo, completado con silencio */
                memset(t->mp3_buf + t->mp3_n * t->can_out, 0, ((unsigned)t->pasada - t->mp3_n) * t->can_out * sizeof(int16_t));
                t->mp3_n = (unsigned)t->pasada;
                int w = 0;
                unsigned char *d = shine_encode_buffer_interleaved(t->shine, t->mp3_buf, &w);
                if (d && w > 0 && mp3_emitir(t, d, (unsigned)w) < 0) r = -1;
            }
            int w = 0;
            unsigned char *d = shine_flush(t->shine, &w);
            if (d && w > 0 && mp3_emitir(t, d, (unsigned)w) < 0) r = -1;
            if (r == 0 && mp3_completar(t) < 0) r = -1;
            if (r == 0 && sink_vaciar(t) < 0) r = -1;
        } else
#endif
        if (t->fmt == FMT_FLAC) { r = cv_flac_fechar(t->flac); t->flac = NULL; if (r == 0 && sink_vaciar(t) < 0) r = -1; }
        else { r = cv_wav_fechar(&t->wav); if (r == 0 && sink_vaciar(t) < 0) r = -1; }
        if (r < 0) { copia_texto(t->erro, _("Error al escribir el archivo (¿disco lleno?)"), sizeof t->erro); return -1; }
        return 0;
    }
    t->leidos_q += (uint32_t)n;
    unsigned m = cv_rem_procesar(&t->rem, t->entrada, (unsigned)n, t->saida);
    int r = 0;
    if (t->fmt == FMT_FLAC) r = cv_flac_escrever(t->flac, t->saida, m);
    else if (t->fmt == FMT_MP3) {
#ifdef CONVERSOR_SHINE
        r = mp3_alimentar(t, t->saida, m);
#endif
    } else r = cv_wav_escrever(&t->wav, t->saida, m);
    if (r < 0) { copia_texto(t->erro, _("Error al escribir el archivo (¿disco lleno?)"), sizeof t->erro); return -1; }
    return 1;
}

/* Cierra el trabajo; con borrar, quita el archivo a medias. */
static void trab_cerrar(trab_t *t, bool borrar)
{
    char d[256];
    copia_texto(d, t->destino, sizeof d);
    if (t->flac) { libera(t->flac); t->flac = NULL; }   /* cancelado: no se completa */
    trab_liberar(t);
    if (borrar) apagar(d);
}

static int pct(const trab_t *t)
{
    if (!t->total_q) return 0;
    uint64_t p = (uint64_t)t->leidos_q * 100 / t->total_q;
    return p > 100 ? 100 : (int)p;
}

/* ------------------------------------------------------------ línea de órdenes */

static void uso(void)
{
    imprimir(_("Uso: convertir-audio ENTRADA... SALIDA [opciones]\n"));
    imprimir(_("  SALIDA con extensión .mp3, .flac o .wav (con varias entradas, una carpeta y --formato).\n"));
    imprimir(_("  --kbps N      MP3: tasa de bits constante, 64 a 320 (por omisión, 128)\n"));
    imprimir(_("  --nivel N     FLAC: nivel de compresión, 0 a 8 (por omisión, 5)\n"));
    imprimir(_("  --taxa HZ     frecuencia de salida: 44100, 22050 u 11025 (por omisión, la del original)\n"));
    imprimir(_("  --canales N   1 (mono) o 2 (estéreo) (por omisión, los del original)\n"));
    imprimir(_("  --bits N      WAV: 8 o 16 (por omisión, 16)\n"));
    imprimir(_("  --formato F   mp3, flac o wav, si SALIDA es una carpeta\n"));
    imprimir(_("  -y            sobrescribe sin preguntar\n"));
    imprimir(_("Sin nombres abre la ventana del Conversor de audio.\n"));
}

static int fmt_de_ext(const char *c)
{
    size_t n = tam_texto(c);
    const char *e = n > 4 && c[n - 4] == '.' ? c + n - 3 : n > 5 && c[n - 5] == '.' ? c + n - 4 : NULL;
    if (!e) return -1;
    if (compara_texto(e, "mp3") == 0) return FMT_MP3;
    if (compara_texto(e, "flac") == 0) return FMT_FLAC;
    if (compara_texto(e, "wav") == 0) return FMT_WAV16;
    return -1;
}

static int convertir_uno(const char *origem, const char *destino, const opc_t *o, bool verboso)
{
    static trab_t t;
    if (trab_abrir(&t, origem, destino, o) < 0) { imprimir_em(2, "convertir-audio: %s\n", t.erro); return 1; }
    uint32_t t0 = tempo_ms();
    int r;
    while ((r = trab_paso(&t)) > 0) {}
    uint32_t dt = tempo_ms() - t0;
    if (r < 0) {
        imprimir_em(2, "convertir-audio: %s\n", t.erro);
        trab_cerrar(&t, true);
        return 1;
    }
    uint32_t dur = t.total_q ? (uint32_t)((uint64_t)t.total_q * 1000 / t.inf->taxa) : 0;
    char sai[64];
    formatar_texto(sai, sizeof sai, "%u Hz, %u canales", t.taxa_out, t.can_out);
    uint32_t leidos = t.leidos_q, inf_taxa = t.inf->taxa;
    trab_cerrar(&t, false);
    if (verboso) imprimir(_("%s -> %s (%s): %u ms de audio en %u ms\n"), nome_curto(origem), nome_curto(destino), sai, (unsigned)((uint64_t)leidos * 1000 / inf_taxa), dt);
    registrar("convertir-audio: %s -> %s: %u cuadros, audio %u ms, tiempo %u ms\n", origem, destino, leidos, dur, dt);
    return 0;
}

static int linea_ordenes(int argc, char **argv)
{
    opc_t o = { -1, 128, 5, 0, 0, false };
    const char *ent[MAX_ARQ];
    int nent = 0, bits = 16;
    const char *forzado = NULL;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (compara_texto(a, "-y") == 0) o.sobrescribir = true;
        else if (compara_texto(a, "--ayuda") == 0 || compara_texto(a, "-h") == 0 || compara_texto(a, "--help") == 0) { uso(); return 0; }
        else if (i + 1 < argc && compara_texto(a, "--kbps") == 0) o.kbps = texto_para_int(argv[++i]);
        else if (i + 1 < argc && compara_texto(a, "--nivel") == 0) o.nivel = texto_para_int(argv[++i]);
        else if (i + 1 < argc && (compara_texto(a, "--taxa") == 0 || compara_texto(a, "--frecuencia") == 0)) o.taxa = texto_para_int(argv[++i]);
        else if (i + 1 < argc && compara_texto(a, "--canales") == 0) o.canales = texto_para_int(argv[++i]);
        else if (i + 1 < argc && compara_texto(a, "--bits") == 0) bits = texto_para_int(argv[++i]);
        else if (i + 1 < argc && compara_texto(a, "--formato") == 0) forzado = argv[++i];
        else if (a[0] == '-' && a[1]) { imprimir_em(2, "convertir-audio: opción desconocida %s\n", a); return 2; }
        else if (nent < MAX_ARQ) ent[nent++] = a;
    }
    if (nent < 2) { uso(); return 2; }
    const char *saida = ent[--nent];
    if (o.kbps < 64 || o.kbps > 320) { imprimir_em(2, "convertir-audio: --kbps va de 64 a 320\n"); return 2; }
    if (o.nivel < 0 || o.nivel > CV_FLAC_NIVEL_MAX) { imprimir_em(2, "convertir-audio: --nivel va de 0 a 8\n"); return 2; }
    if (o.canales < 0 || o.canales > 2) { imprimir_em(2, "convertir-audio: --canales es 1 o 2\n"); return 2; }
    if (bits != 8 && bits != 16) { imprimir_em(2, "convertir-audio: --bits es 8 o 16\n"); return 2; }
    if (o.taxa && (o.taxa < 8000 || o.taxa > 192000)) { imprimir_em(2, "convertir-audio: --taxa fuera de rango\n"); return 2; }
    info_arquivo_t inf;
    bool pasta = info_arquivo(saida, &inf) >= 0 && inf.tipo == TIPO_PASTA;
    int fmt = pasta || forzado ? -1 : fmt_de_ext(saida);
    if (forzado) fmt = compara_texto(forzado, "mp3") == 0 ? FMT_MP3 : compara_texto(forzado, "flac") == 0 ? FMT_FLAC : compara_texto(forzado, "wav") == 0 ? FMT_WAV16 : -1;
    if (fmt < 0) { imprimir_em(2, "convertir-audio: el formato de salida no está claro (usa .mp3, .flac, .wav o --formato)\n"); return 2; }
    if (fmt == FMT_WAV16 && bits == 8) fmt = FMT_WAV8;
    o.fmt = fmt;
    if (nent > 1 && !pasta) { imprimir_em(2, "convertir-audio: con varias entradas, la salida debe ser una carpeta\n"); return 2; }
    int malos = 0;
    for (int i = 0; i < nent; i++) {
        char d[256];
        if (pasta) destino_de(d, sizeof d, saida, ent[i], fmt);
        else copia_texto(d, saida, sizeof d);
        if (compara_texto(d, ent[i]) == 0) { imprimir_em(2, "convertir-audio: %s: la salida es el propio archivo de entrada\n", ent[i]); malos++; continue; }
        if (existe(d) && !o.sobrescribir) { imprimir_em(2, "convertir-audio: %s ya existe (-y para sobrescribir)\n", d); malos++; continue; }
        if (convertir_uno(ent[i], d, &o, true)) malos++;
    }
    return malos ? 1 : 0;
}

/* ------------------------------------------------------------ ventana */

#define LARG 640
#define ALT 520
#define MARGEM 12
#define COL_OPC 128
#define ROD 22

enum { ES_PARADO, ES_CONVIRTIENDO };

typedef struct { int x, y, n; const char *rot[9]; int w[9]; int sel; bool ativo; } seg_t;

static jan_t *j;
static char rutas[MAX_ARQ][200];
static char nomes[MAX_ARQ][64];
static int qtd;
static lista_t lista;
static caixa_t pasta_c;
static char aviso[200];
static int estado = ES_PARADO;
static char ult_dir[200] = "/home";

static seg_t sg_fmt, sg_cal, sg_taxa, sg_can;
static int cal_kbps = 2, cal_nivel = 5;   /* índice en KBPS y nivel FLAC */

static botao_t bt_arq[] = {
    { 12, 10, 148, 26, N_("Añadir archivo..."), false },
    { 168, 10, 148, 26, N_("Añadir carpeta..."), false },
    { 324, 10, 80, 26, N_("Quitar"), false },
    { 412, 10, 80, 26, N_("Vaciar"), false },
};
static botao_t bt_dest = { 0, 0, 100, 24, N_("Examinar..."), false };
static botao_t bt_conv = { 12, 0, 130, 28, N_("Convertir"), false };
static botao_t bt_canc = { 150, 0, 130, 28, N_("Cancelar"), false };

static progresso_t *prog_arq, *prog_tot;
static trab_t trab;
static int cola_i, cola_n;        /* archivo que se está convirtiendo y cuántos en la tanda */
static opc_t opc_act;
static bool omitir_existentes;    /* el usuario dijo «No» a sobrescribir */
static bool cancelado;
static int hechos, fallos, omitidos;
static uint32_t t_ini, t_ult;

static void seg_iniciar(seg_t *s, int y, int n, const char *const *rot, int sel)
{
    s->x = COL_OPC; s->y = y; s->n = n; s->sel = sel; s->ativo = true;
    for (int i = 0; i < n; i++) { s->rot[i] = rot[i]; s->w[i] = (int)tam_texto(rot[i]) * LETRA_L + 18; }
}

static void seg_desenhar(const seg_t *s)
{
    int x = s->x;
    for (int i = 0; i < s->n; i++) {
        bool on = i == s->sel;
        uint32_t fundo = !s->ativo ? COR_FUNDO : on ? COR_SELECAO : COR_BOTAO;
        uint32_t cor = !s->ativo ? COR_TEXTO_FRACO : on ? COR_BRANCO : COR_TEXTO;
        g_retangulo(j, x, s->y, s->w[i], 24, fundo);
        g_moldura(j, x, s->y, s->w[i], 24, COR_BORDA);
        g_texto(j, x + 9, s->y + 4, s->rot[i], cor);
        x += s->w[i] - 1;
    }
}

/* true si el clic cambió la selección. */
static bool seg_clic(seg_t *s, const evento_t *e)
{
    if (!s->ativo || e->tipo != EV_CLIQUE || !(e->botoes & 1) || e->y < s->y || e->y >= s->y + 24) return false;
    int x = s->x;
    for (int i = 0; i < s->n; i++) {
        if (e->x >= x && e->x < x + s->w[i]) { bool mudou = s->sel != i; s->sel = i; return mudou; }
        x += s->w[i] - 1;
    }
    return false;
}

static bool adicionar_arquivo(const char *c)
{
    if (qtd >= MAX_ARQ) { copia_texto(aviso, _("Lista llena"), sizeof aviso); return false; }
    for (int i = 0; i < qtd; i++) if (compara_texto(rutas[i], c) == 0) return false;
    int erro;
    audio_arquivo_t *a = audio_arquivo_abrir(c, &erro);   /* solo para comprobar que se sabe leer */
    if (!a) {
        formatar_texto(aviso, sizeof aviso, _("%s: %s"), nome_curto(c), audio_erro_texto(erro));
        return false;
    }
    const audio_info_t *inf = audio_arquivo_info(a);
    char t[64];
    uint32_t s = inf->duracao_ms / 1000;
    formatar_texto(t, sizeof t, "%s [%s %u:%02u]", nome_curto(c), inf->canais == 2 ? "st" : "mono", s / 60, s % 60);
    audio_arquivo_fechar(a);
    copia_texto(rutas[qtd], c, sizeof rutas[0]);
    copia_texto(nomes[qtd], t, sizeof nomes[0]);
    qtd++;
    lista.itens = nomes;
    lista.qtd = qtd;
    return true;
}

static int adicionar_pasta(const char *c)
{
    static char ns[MAX_ARQ][64];
    int n = 0, add = 0;
    entrada_pasta_t e;
    for (int k = 0; listar(c, k, &e) > 0 && n < MAX_ARQ; k++) {
        if (e.tipo == TIPO_PASTA || !extensao_audio(e.nome)) continue;
        int p = n++;
        while (p > 0 && compara_texto(ns[p - 1], e.nome) > 0) { copia_texto(ns[p], ns[p - 1], 64); p--; }
        copia_texto(ns[p], e.nome, 64);
    }
    for (int k = 0; k < n; k++) {
        char r[200];
        caminho_juntar(r, sizeof r, c, ns[k]);
        if (adicionar_arquivo(r)) add++;
    }
    if (!add && !aviso[0]) formatar_texto(aviso, sizeof aviso, _("No hay archivos de sonido en %s"), c);
    return add;
}

static void dir_de(const char *ruta, char *d, size_t max)
{
    copia_texto(d, ruta, max);
    char *p = acha_ultimo_char(d, '/');
    if (p && p != d) *p = 0;
    else if (p) p[1] = 0;
}

/* Adapta las filas de calidad al formato elegido. */
static void rehacer_calidad(void)
{
    static char rot_k[N_KBPS][8], rot_n[CV_FLAC_NIVEL_MAX + 1][4];
    static const char *pk[8], *pn[CV_FLAC_NIVEL_MAX + 1], *pv[1];
    int f = sg_fmt.sel;
    if (f == FMT_MP3) {
        for (int i = 0; i < N_KBPS; i++) { formatar_texto(rot_k[i], sizeof rot_k[i], "%d", KBPS[i]); pk[i] = rot_k[i]; }
        seg_iniciar(&sg_cal, sg_cal.y, N_KBPS, pk, cal_kbps);
    } else if (f == FMT_FLAC) {
        for (int i = 0; i <= CV_FLAC_NIVEL_MAX; i++) { formatar_texto(rot_n[i], sizeof rot_n[i], "%d", i); pn[i] = rot_n[i]; }
        seg_iniciar(&sg_cal, sg_cal.y, CV_FLAC_NIVEL_MAX + 1, pn, cal_nivel);
    } else {
        pv[0] = "-";
        seg_iniciar(&sg_cal, sg_cal.y, 1, pv, 0);
        sg_cal.ativo = false;
    }
}

static const char *rotulo_calidad(void)
{
    return sg_fmt.sel == FMT_MP3 ? _("Tasa (kbps):") : sg_fmt.sel == FMT_FLAC ? _("Compresión:") : _("Calidad:");
}

static void opcoes_atuais(opc_t *o)
{
    memset(o, 0, sizeof *o);
    o->fmt = sg_fmt.sel;
    o->kbps = KBPS[cal_kbps];
    o->nivel = cal_nivel;
    o->taxa = sg_taxa.sel == 1 ? 44100 : sg_taxa.sel == 2 ? 22050 : sg_taxa.sel == 3 ? 11025 : 0;
    o->canales = sg_can.sel;   /* 0 original, 1 mono, 2 estéreo */
    o->sobrescribir = true;
}

static void desenhar_tudo(void)
{
    int w = j->larg;
    g_retangulo(j, 0, 0, w, j->alt, COR_FUNDO);
    g_retangulo(j, 0, 44, w, 1, COR_BORDA);
    for (int i = 0; i < 4; i++) botao_desenhar(j, &bt_arq[i]);
    lista.x = MARGEM; lista.y = 52; lista.l = w - 2 * MARGEM; lista.a = 132;
    if (qtd) lista_desenhar(j, &lista);
    else {
        g_retangulo(j, lista.x, lista.y, lista.l, lista.a, COR_BRANCO);
        g_moldura(j, lista.x, lista.y, lista.l, lista.a, COR_BORDA);
        g_texto(j, lista.x + 8, lista.y + 8, _("Añade los archivos de sonido que quieres convertir."), COR_TEXTO_FRACO);
    }
    int y = 196;
    rotulo(j, MARGEM, y + 4, _("Formato:"), COR_TEXTO);
    seg_desenhar(&sg_fmt);
    rotulo(j, MARGEM, y + 36, rotulo_calidad(), COR_TEXTO);
    seg_desenhar(&sg_cal);
    rotulo(j, MARGEM, y + 68, _("Frecuencia:"), COR_TEXTO);
    seg_desenhar(&sg_taxa);
    rotulo(j, MARGEM, y + 100, _("Canales:"), COR_TEXTO);
    seg_desenhar(&sg_can);
    int yd = y + 140;
    rotulo(j, MARGEM, yd + 4, _("Destino:"), COR_TEXTO);
    pasta_c.x = COL_OPC; pasta_c.y = yd; pasta_c.l = w - COL_OPC - 120 - MARGEM;
    caixa_desenhar(j, &pasta_c);
    bt_dest.x = w - 112 - MARGEM + 12; bt_dest.y = yd - 2;
    botao_desenhar(j, &bt_dest);
    int yb = yd + 36;
    bt_conv.y = yb; bt_canc.y = yb;
    botao_desenhar(j, &bt_conv);
    botao_desenhar(j, &bt_canc);
    rotulo(j, MARGEM, yb + 40, estado == ES_CONVIRTIENDO ? _("Archivo actual") : _("Archivo"), COR_TEXTO_FRACO);
    rotulo(j, MARGEM, yb + 74, _("Total"), COR_TEXTO_FRACO);
    if (prog_arq) progresso_desenhar(j, prog_arq);
    if (prog_tot) progresso_desenhar(j, prog_tot);
    int yr = j->alt - ROD;
    g_retangulo(j, 0, yr, w, ROD, 0xE6E8EA);
    g_retangulo(j, 0, yr, w, 1, COR_BORDA);
    char t[220];
    if (aviso[0]) copia_texto(t, aviso, sizeof t);
    else if (sg_fmt.sel == FMT_MP3 && sg_taxa.sel == 3) copia_texto(t, _("MP3 a 11,025 kHz es MPEG-2.5: muchos reproductores no lo leen"), sizeof t);
    else if (sg_fmt.sel == FMT_MP3 && sg_taxa.sel >= 2 && cal_kbps > 3) copia_texto(t, _("A esa frecuencia el MP3 admite como mucho 160 kbps"), sizeof t);
    else formatar_texto(t, sizeof t, _("%d archivos en la lista"), qtd);
    g_texto_n(j, 8, yr + 3, t, (w - 16) / LETRA_L, aviso[0] ? 0x9A1B1B : COR_TEXTO);
    jan_mostrar(j);
}

static void montar_progreso(void)
{
    if (prog_arq) progresso_liberar(prog_arq);
    if (prog_tot) progresso_liberar(prog_tot);
    int yb = 196 + 140 + 36;
    prog_arq = progresso_criar(MARGEM, yb + 58, j->larg - 2 * MARGEM, 16, false);
    prog_tot = progresso_criar(MARGEM, yb + 92, j->larg - 2 * MARGEM, 16, false);
    progresso_texto(prog_arq, true);
    progresso_texto(prog_tot, true);
}

/* ---- la tanda ---- */

static void tanda_fin(void)
{
    estado = ES_PARADO;
    uint32_t dt = tempo_ms() - t_ini;
    if (cancelado) formatar_texto(aviso, sizeof aviso, _("Cancelado: %d convertidos"), hechos);
    else if (fallos || omitidos) formatar_texto(aviso, sizeof aviso, _("Hecho: %d convertidos, %d con error, %d omitidos"), hechos, fallos, omitidos);
    else formatar_texto(aviso, sizeof aviso, _("Hecho: %d archivos convertidos en %u s"), hechos, dt / 1000);
    registrar("convertir-audio: tanda terminada: %d bien, %d fallos, %d omitidos, %u ms\n", hechos, fallos, omitidos, dt);
    progresso_definir(prog_arq, cancelado ? pct(&trab) : 100);
    desenhar_tudo();
}

static void tanda_siguiente(void);

static void abrir_siguiente(void)
{
    while (cola_i < cola_n) {
        char d[256];
        destino_de(d, sizeof d, pasta_c.texto, rutas[cola_i], opc_act.fmt);
        if (compara_texto(d, rutas[cola_i]) == 0) {
            formatar_texto(aviso, sizeof aviso, _("%s: el destino es el propio archivo de origen"), nome_curto(rutas[cola_i]));
            fallos++; cola_i++;
            continue;
        }
        if (existe(d) && omitir_existentes) { omitidos++; cola_i++; continue; }
        if (trab_abrir(&trab, rutas[cola_i], d, &opc_act) < 0) {
            copia_texto(aviso, trab.erro, sizeof aviso);
            fallos++; cola_i++;
            continue;
        }
        registrar("convertir-audio: %s -> %s\n", rutas[cola_i], d);
        return;
    }
    tanda_fin();
}

static void tanda_siguiente(void)
{
    cola_i++;
    abrir_siguiente();
}

static void empezar(void)
{
    aviso[0] = 0;
    if (!qtd) { copia_texto(aviso, _("No hay archivos que convertir"), sizeof aviso); return; }
    if (!pasta_c.texto[0]) { copia_texto(aviso, _("Elige la carpeta de destino"), sizeof aviso); return; }
    info_arquivo_t i;
    if (info_arquivo(pasta_c.texto, &i) < 0 || i.tipo != TIPO_PASTA) {
        if (!confirmar(_("Carpeta de destino"), _("La carpeta de destino no existe. ¿Crearla?")) || criar_pasta(pasta_c.texto) < 0) {
            copia_texto(aviso, _("No se puede usar esa carpeta de destino"), sizeof aviso);
            return;
        }
    }
    opcoes_atuais(&opc_act);
    int ya = 0;
    for (int k = 0; k < qtd; k++) {
        char d[256];
        destino_de(d, sizeof d, pasta_c.texto, rutas[k], opc_act.fmt);
        if (existe(d)) ya++;
    }
    omitir_existentes = false;
    if (ya) {   /* aviso si el destino existe */
        char p[200];
        formatar_texto(p, sizeof p, ya == 1 ? _("Ya existe %d archivo en el destino.\n¿Sobrescribirlo? (No: se omite)") : _("Ya existen %d archivos en el destino.\n¿Sobrescribirlos? (No: se omiten)"), ya);
        omitir_existentes = !confirmar(_("El destino existe"), p);
    }
    cola_i = 0; cola_n = qtd;
    hechos = fallos = omitidos = 0;
    cancelado = false;
    estado = ES_CONVIRTIENDO;
    t_ini = tempo_ms();
    progresso_definir(prog_arq, 0);
    progresso_definir(prog_tot, 0);
    abrir_siguiente();
}

static void cancelar(void)
{
    if (estado != ES_CONVIRTIENDO) return;
    cancelado = true;
    trab_cerrar(&trab, true);
    tanda_fin();
}

/* Un bloque de la conversión; true si hay que redibujar. */
static bool avanzar(void)
{
    if (estado != ES_CONVIRTIENDO) return false;
    int r = trab_paso(&trab);
    if (r > 0) {
        uint32_t ahora = tempo_ms();
        if (ahora - t_ult >= 80) {
            t_ult = ahora;
            progresso_definir(prog_arq, pct(&trab));
            progresso_definir(prog_tot, (cola_i * 100 + pct(&trab)) / cola_n);
            if (prog_arq) { retangulo_t rc; progresso_desenhar(j, prog_arq); progresso_rect(prog_arq, &rc); atualizar_janela(j->id, &rc); }
            if (prog_tot) { retangulo_t rc; progresso_desenhar(j, prog_tot); progresso_rect(prog_tot, &rc); atualizar_janela(j->id, &rc); }
        }
        return false;
    }
    if (r < 0) {
        copia_texto(aviso, trab.erro, sizeof aviso);
        trab_cerrar(&trab, true);
        fallos++;
    } else {
        trab_cerrar(&trab, false);
        hechos++;
    }
    progresso_definir(prog_tot, ((cola_i + 1) * 100) / cola_n);
    tanda_siguiente();
    if (estado == ES_CONVIRTIENDO) progresso_definir(prog_arq, 0);
    return true;
}

/* ---- eventos ---- */

static void acao_arquivo(int i)
{
    aviso[0] = 0;
    if (i == 0) {
        char c[200];
        copia_texto(c, ult_dir, sizeof c);
        if (dialogo_abrir(_("Añadir archivo de sonido"), "Sonido (*.wav;*.mp3;*.flac;*.mid;*.midi;*.kar)|*.wav;*.mp3;*.flac;*.mid;*.midi;*.kar|Todos los archivos (*.*)|*", c, c, sizeof c) && c[0]) {
            dir_de(c, ult_dir, sizeof ult_dir);
            if (adicionar_arquivo(c) && !pasta_c.texto[0]) caixa_definir(&pasta_c, ult_dir);
        }
    } else if (i == 1) {
        char c[200];
        copia_texto(c, ult_dir, sizeof c);
        if (dialogo_carpeta(_("Añadir carpeta"), c, c, sizeof c) && c[0]) {
            copia_texto(ult_dir, c, sizeof ult_dir);
            adicionar_pasta(c);
            if (!pasta_c.texto[0]) caixa_definir(&pasta_c, c);
        }
    } else if (i == 2) {
        if (lista.sel >= 0 && lista.sel < qtd) {
            for (int k = lista.sel; k + 1 < qtd; k++) { copia_texto(rutas[k], rutas[k + 1], sizeof rutas[0]); copia_texto(nomes[k], nomes[k + 1], sizeof nomes[0]); }
            qtd--;
            lista.qtd = qtd;
            if (lista.sel >= qtd) lista.sel = qtd - 1;
        }
    } else {
        qtd = 0; lista.qtd = 0; lista.sel = -1; lista.topo = 0;
    }
}

static bool tratar(evento_t *e)
{
    if (e->tipo == EV_FECHAR) return false;
    if (estado == ES_CONVIRTIENDO) {
        if (botao_tratar(j, &bt_canc, e)) { cancelar(); desenhar_tudo(); }
        else if (e->tipo == EV_TECLA && e->tecla == TECLA_ESC) { cancelar(); desenhar_tudo(); }
        return true;
    }
    bool dibuja = false;
    for (int i = 0; i < 4; i++) if (botao_tratar(j, &bt_arq[i], e)) { acao_arquivo(i); dibuja = true; }
    if (botao_tratar(j, &bt_dest, e)) {
        char c[200];
        copia_texto(c, pasta_c.texto[0] ? pasta_c.texto : ult_dir, sizeof c);
        if (dialogo_carpeta(_("Carpeta de destino"), c, c, sizeof c) && c[0]) caixa_definir(&pasta_c, c);
        dibuja = true;
    }
    if (botao_tratar(j, &bt_conv, e)) { empezar(); dibuja = true; }
    if (seg_clic(&sg_fmt, e)) { aviso[0] = 0; rehacer_calidad(); dibuja = true; }
    if (seg_clic(&sg_cal, e)) { if (sg_fmt.sel == FMT_MP3) cal_kbps = sg_cal.sel; else cal_nivel = sg_cal.sel; dibuja = true; }
    if (seg_clic(&sg_taxa, e)) dibuja = true;
    if (seg_clic(&sg_can, e)) dibuja = true;
    if (caixa_tratar(j, &pasta_c, e)) { empezar(); dibuja = true; }
    else if (e->tipo == EV_TECLA || e->tipo == EV_CLIQUE) dibuja = true;
    int r = lista_tratar(j, &lista, e);
    if (r != LISTA_NADA) dibuja = true;
    if (dibuja) desenhar_tudo();
    return true;
}

static int janela(int argc, char **argv)
{
    j = jan_abrir(_("Conversor de audio"), LARG, ALT);
    if (!j) return 1;
    lista.sel = -1;
    lista.icone = NULL;
    static const char *const r_fmt[] = { "MP3", "FLAC", "WAV 16", "WAV 8" };
    static const char *const r_taxa[] = { N_("Original"), "44,1 kHz", "22,05 kHz", "11,025 kHz" };
    static const char *const r_can[] = { N_("Original"), N_("Mono"), N_("Estéreo") };
    static const char *p_taxa[4], *p_can[3];
    int fmt0 = 0;
#ifndef CONVERSOR_SHINE
    fmt0 = FMT_FLAC;
#endif
    seg_iniciar(&sg_fmt, 196, 4, r_fmt, fmt0);
    sg_cal.y = 228;
    rehacer_calidad();
    for (int i = 0; i < 4; i++) p_taxa[i] = _(r_taxa[i]);
    for (int i = 0; i < 3; i++) p_can[i] = _(r_can[i]);
    seg_iniciar(&sg_taxa, 260, 4, p_taxa, 0);
    seg_iniciar(&sg_can, 292, 3, p_can, 0);
    pasta_c.texto[0] = 0;
    pasta_c.foco = false;
    caixa_definir(&pasta_c, "");
    montar_progreso();
    for (int i = 1; i < argc; i++) {
        info_arquivo_t inf;
        if (info_arquivo(argv[i], &inf) < 0) { formatar_texto(aviso, sizeof aviso, _("%s: no existe"), argv[i]); continue; }
        if (inf.tipo == TIPO_PASTA) { adicionar_pasta(argv[i]); copia_texto(ult_dir, argv[i], sizeof ult_dir); }
        else { adicionar_arquivo(argv[i]); dir_de(argv[i], ult_dir, sizeof ult_dir); }
    }
    caixa_definir(&pasta_c, ult_dir);
    if (qtd) lista.sel = 0;
    lista.itens = nomes;
    lista.qtd = qtd;
    desenhar_tudo();
    registrar("convertir-audio: ventana lista (%d archivos)\n", qtd);
    evento_t e;
    bool seguir = true;
    while (seguir) {
        if (estado == ES_CONVIRTIENDO) {
            while (seguir && proximo_evento(j->id, &e, false) == 1) seguir = tratar(&e);
            if (!seguir) break;
            if (avanzar()) desenhar_tudo();
        } else if (esperar_evento(j->id, &e, -1) == 1) {
            seguir = tratar(&e);
            while (seguir && proximo_evento(j->id, &e, false) == 1) seguir = tratar(&e);
        }
    }
    if (estado == ES_CONVIRTIENDO) trab_cerrar(&trab, true);
    jan_fechar(j);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc >= 3) return linea_ordenes(argc, argv);
    if (argc == 2 && (compara_texto(argv[1], "--ayuda") == 0 || compara_texto(argv[1], "-h") == 0)) { uso(); return 0; }
    return janela(argc, argv);
}
