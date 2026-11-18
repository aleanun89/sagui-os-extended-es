/* sdl2_mixer.c - SDL2_mixer del SaguiOS (D374), una capa reutilizable para los ports con sonido.
 *
 * Mezcla por software en la llamada de vuelta de la SDL 2 del sistema (que la atiende desde
 * SDL_PollEvent, SDL_Delay y SDL_RenderPresent: no hay hilos). El dispositivo es siempre PCM de
 * 16 bits con signo; los efectos (Mix_Chunk) se convierten al abrirlos a la frecuencia y los canales
 * del dispositivo (WAV propio y Ogg Vorbis por stb_vorbis), así que mezclar es solo sumar. La música
 * se decodifica en flujo (Ogg por stb_vorbis; MP3, FLAC, WAV y MIDI por los decodificadores de la
 * libsagui, de uno en uno) y se remuestrea con interpolación lineal. Anota en el registro del sistema
 * «[MIXER]» al abrir el audio y al reproducir el primer efecto y la primera música. */
#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "../terceros/stbvorbis/stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY

#include "SDL_mixer.h"
#include "audio_formato.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>



/* ------------------------------------------------------------------ estado */
typedef struct {
    Mix_Chunk *c;
    Uint32 pos;          /* bytes ya mezclados */
    int veces;           /* repeticiones que faltan (-1 = sin fin) */
    int vol;             /* 0..128 */
    int pausado;
    int activo;
    int pan_i, pan_d;      /* panorama 0..255 por lado (D383, Mix_SetPanning) */
    long limite;         /* cuadros que quedan hasta parar (-1 = sin límite) */
    long fade_total, fade_resta;   /* fundido de salida en cuadros */
} canal_t;

struct _Mix_Music {
    Mix_MusicType tipo;
    unsigned char *datos;   /* el fichero entero (Ogg) */
    size_t n;
    char ruta[160];         /* para MP3/FLAC/WAV/MIDI */
    int copia;              /* ruta es una copia propia en /tmp (MIDI): se borra al liberar (D383) */
};

static int abierto, dev_freq, dev_ch, dev_bloque;
static canal_t *canales;
static int ncanales;
static void (*al_acabar_canal)(int);
static void (*al_acabar_musica)(void);
static int log_efecto, log_musica;
static int32_t *acum;

/* La música que suena. */
static struct {
    Mix_Music *m;
    stb_vorbis *ogg;
    audio_arquivo_t *arq;
    int src_ch;
    long src_rate;
    int vol;
    int veces;
    int pausada;
    int activa;
    long fade_total, fade_resta;
    /* remuestreo en flujo */
    int16_t buf[2 * 1024 + 4];
    int n;                    /* cuadros válidos en buf */
    unsigned long pos;        /* posición en 16.16 */
    int fin;                  /* el origen se acabó */
} mus;

/* ------------------------------------------------------------------ conversión */
static void (*gancho_musica)(void *, Uint8 *, int);
static void *gancho_datos;
static struct { Mix_EffectFunc_t f; void *datos; } post[4];
static int n_post;

static int16_t clip16(int32_t v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : (int16_t)v; }

/* PCM de origen (cuadros intercalados, sch canales, srate) -> PCM del dispositivo (malloc). */
static int16_t *a_dispositivo(const int16_t *src, long n, int sch, long srate, long *nsal)
{
    if (n <= 0 || srate <= 0) return NULL;
    long m = (long)((long long)n * dev_freq / srate);
    if (m < 1) m = 1;
    int16_t *o = malloc((size_t)m * dev_ch * 2);
    if (!o) return NULL;
    for (long i = 0; i < m; i++) {
        long long p = (long long)i * srate * 256 / dev_freq;   /* 24.8 */
        long a = (long)(p >> 8);
        int f = (int)(p & 255);
        long b = a + 1 < n ? a + 1 : a;
        if (a >= n) a = b = n - 1;
        int32_t l[2];
        for (int c = 0; c < 2; c++) {
            int cc = c < sch ? c : 0;
            int32_t x = src[a * sch + cc], y = src[b * sch + cc];
            l[c] = x + (((y - x) * f) >> 8);
        }
        if (sch == 1) l[1] = l[0];
        if (dev_ch == 1) o[i] = clip16((l[0] + l[1]) / 2);
        else { o[i * 2] = clip16(l[0]); o[i * 2 + 1] = clip16(l[1]); }
    }
    *nsal = m;
    return o;
}

static unsigned rd16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static unsigned rd32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24); }

/* WAV PCM de 8, 16, 24 o 32 bits -> int16 intercalado (malloc). */
static int16_t *wav_decodificar(const unsigned char *d, size_t n, int *ch, long *rate, long *frames)
{
    if (n < 12 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) return NULL;
    size_t p = 12;
    unsigned fmt = 0, bits = 0, canales = 0, tasa = 0;
    const unsigned char *datos = NULL;
    size_t nd = 0;
    while (p + 8 <= n) {
        unsigned l = rd32(d + p + 4);
        const unsigned char *c = d + p + 8;
        if (!memcmp(d + p, "fmt ", 4) && l >= 16 && p + 8 + 16 <= n) {
            fmt = rd16(c); canales = rd16(c + 2); tasa = rd32(c + 4); bits = rd16(c + 14);
            if (fmt == 0xFFFE && l >= 26) fmt = rd16(c + 24);
        } else if (!memcmp(d + p, "data", 4)) {
            datos = c;
            nd = l > n - (p + 8) ? n - (p + 8) : l;
            break;
        }
        p += 8 + l + (l & 1);
    }
    if (fmt != 1 || !datos || (canales != 1 && canales != 2) || !(bits == 8 || bits == 16 || bits == 24 || bits == 32)) return NULL;
    long q = (long)(nd / (canales * bits / 8));
    int16_t *o = malloc((size_t)q * canales * 2 + 2);
    if (!o) return NULL;
    for (long i = 0; i < q * (long)canales; i++) {
        const unsigned char *s = datos + (size_t)i * (bits / 8);
        o[i] = bits == 8 ? (int16_t)((s[0] - 128) << 8) : bits == 16 ? (int16_t)rd16(s) : bits == 24 ? (int16_t)rd16(s + 1) : (int16_t)rd16(s + 2);
    }
    *ch = (int)canales; *rate = tasa; *frames = q;
    return o;
}

/* ------------------------------------------------------------------ dispositivo */
static void mezclar_callback(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    int frames = len / (2 * dev_ch);
    int total = frames * dev_ch;
    int16_t *out = (int16_t *)stream;
    if (!acum) { memset(stream, 0, (size_t)len); return; }
    memset(acum, 0, (size_t)total * sizeof *acum);

    /* efectos */
    int acabados[64], nac = 0;
    for (int k = 0; k < ncanales; k++) {
        canal_t *c = &canales[k];
        if (!c->activo || c->pausado) continue;
        int vol = c->vol * c->c->volume / 128;
        int fr_hechos = 0;
        while (fr_hechos < frames && c->activo) {
            Uint32 quedan = c->c->alen - c->pos;
            int bps = 2 * dev_ch;
            int f = (int)(quedan / bps);
            if (f > frames - fr_hechos) f = frames - fr_hechos;
            if (c->limite >= 0 && f > c->limite) f = (int)c->limite;
            const int16_t *s = (const int16_t *)(c->c->abuf + c->pos);
            for (int i = 0; i < f * dev_ch; i++) {
                int v = vol;
                if (c->fade_total > 0) v = (int)((long long)vol * c->fade_resta / c->fade_total);
                int sv = s[i] * v / 128;
                if (dev_ch == 2 && (c->pan_i | c->pan_d) && (c->pan_i != 255 || c->pan_d != 255))
                    sv = sv * ((i & 1) ? c->pan_d : c->pan_i) / 255;
                acum[fr_hechos * dev_ch + i] += sv;
                if (c->fade_total > 0 && (i % dev_ch) == dev_ch - 1 && c->fade_resta > 0) c->fade_resta--;
            }
            c->pos += (Uint32)(f * bps);
            fr_hechos += f;
            if (c->limite >= 0) { c->limite -= f; if (c->limite <= 0) c->activo = 0; }
            if (c->fade_total > 0 && c->fade_resta <= 0) c->activo = 0;
            if (c->activo && c->pos + bps > c->c->alen) {   /* fin del efecto */
                if (c->veces == 0) c->activo = 0;
                else { if (c->veces > 0) c->veces--; c->pos = 0; }
            }
            if (f == 0) break;
        }
        if (!c->activo && nac < 64) acabados[nac++] = k;
    }

    /* música */
    if (mus.activa && !mus.pausada && (mus.ogg || mus.arq)) {
        int hecho = 0;
        long paso = (long)((long long)mus.src_rate * 65536 / dev_freq);
        while (hecho < frames) {
            long i = (long)(mus.pos >> 16);
            if (i + 1 >= mus.n) {
                if (mus.fin) { mus.activa = 0; break; }
                int resto = 0;
                if (mus.n > 0) { mus.buf[0 * mus.src_ch] = mus.buf[(mus.n - 1) * mus.src_ch];
                                 if (mus.src_ch == 2) mus.buf[1] = mus.buf[(mus.n - 1) * 2 + 1];
                                 resto = 1; mus.pos -= (unsigned long)(mus.n - 1) << 16; }
                int got;
                int cap = 1024 - resto;
                if (mus.ogg) {
                    got = stb_vorbis_get_samples_short_interleaved(mus.ogg, mus.src_ch, mus.buf + resto * mus.src_ch, cap * mus.src_ch);
                } else {
                    got = audio_arquivo_ler(mus.arq, mus.buf + resto * mus.src_ch, (unsigned)cap);
                    if (got < 0) got = 0;
                }
                if (got <= 0) {
                    if (mus.veces != 0 && mus.n > 1) {   /* repetir */
                        if (mus.veces > 0) mus.veces--;
                        if (mus.ogg) stb_vorbis_seek_start(mus.ogg); else audio_arquivo_buscar(mus.arq, 0);
                        mus.n = 0; mus.pos = 0;
                        got = mus.ogg ? stb_vorbis_get_samples_short_interleaved(mus.ogg, mus.src_ch, mus.buf, 1024 * mus.src_ch)
                                      : audio_arquivo_ler(mus.arq, mus.buf, 1024);
                        if (got <= 0) { mus.fin = 1; mus.activa = 0; break; }
                        mus.n = got;
                        continue;
                    }
                    mus.fin = 1; mus.activa = 0; break;
                }
                mus.n = resto + got;
                continue;
            }
            int f = (int)((mus.pos >> 8) & 255);
            int32_t l[2];
            for (int c = 0; c < 2; c++) {
                int cc = c < mus.src_ch ? c : 0;
                int32_t x = mus.buf[i * mus.src_ch + cc], y = mus.buf[(i + 1) * mus.src_ch + cc];
                l[c] = x + (((y - x) * f) >> 8);
            }
            int vol = mus.vol;
            if (mus.fade_total > 0) { vol = (int)((long long)vol * mus.fade_resta / mus.fade_total); if (mus.fade_resta > 0) mus.fade_resta--; else mus.activa = 0; }
            if (dev_ch == 1) acum[hecho] += (l[0] + l[1]) / 2 * vol / 128;
            else { acum[hecho * 2] += l[0] * vol / 128; acum[hecho * 2 + 1] += l[1] * vol / 128; }
            mus.pos += (unsigned long)paso;
            hecho++;
            if (!mus.activa) break;
        }
        if (!mus.activa && al_acabar_musica) al_acabar_musica();
    }

    if (gancho_musica) {   /* Mix_HookMusic: la música la genera el programa (D383) */
        static int16_t gm[8192];
        int nb = total * 2 < (int)sizeof gm ? total * 2 : (int)sizeof gm;
        memset(gm, 0, sizeof gm);
        gancho_musica(gancho_datos, (Uint8 *)gm, nb);
        for (int i = 0; i < nb / 2; i++) acum[i] += gm[i];
    }
    for (int i = 0; i < total; i++) out[i] = clip16(acum[i]);
    for (int e = 0; e < n_post; e++) post[e].f(MIX_CHANNEL_POST, out, total * 2, post[e].datos);
    for (int k = 0; k < nac; k++) if (al_acabar_canal) al_acabar_canal(acabados[k]);
}

/* ------------------------------------------------------------------ API: audio */
int Mix_Init(int flags) { return flags & (MIX_INIT_OGG | MIX_INIT_MP3 | MIX_INIT_FLAC | MIX_INIT_MID); }
void Mix_Quit(void) { }

int Mix_AllocateChannels(int n)
{
    if (n < 0) return ncanales;
    if (n == ncanales) return n;
    canal_t *nc = realloc(canales, (size_t)(n ? n : 1) * sizeof *nc);
    if (!nc) return ncanales;
    for (int i = ncanales; i < n; i++) memset(&nc[i], 0, sizeof nc[i]);
    for (int i = 0; i < n; i++) if (i >= ncanales) { nc[i].vol = MIX_MAX_VOLUME; nc[i].pan_i = nc[i].pan_d = 255; }
    canales = nc;
    ncanales = n;
    return n;
}

int Mix_OpenAudio(int frecuencia, Uint16 formato, int canales_, int tam_bloque)
{
    (void)formato;
    if (abierto) { abierto++; return 0; }
    SDL_AudioSpec pedido, obtenido;
    memset(&pedido, 0, sizeof pedido);
    pedido.freq = frecuencia > 0 ? frecuencia : MIX_DEFAULT_FREQUENCY;
    pedido.format = AUDIO_S16SYS;
    pedido.channels = canales_ == 1 ? 1 : 2;
    pedido.samples = (Uint16)(tam_bloque >= 256 && tam_bloque <= 16384 ? tam_bloque : 1024);
    pedido.callback = mezclar_callback;
    if (SDL_OpenAudio(&pedido, &obtenido) < 0) return -1;
    dev_freq = obtenido.freq;
    dev_ch = obtenido.channels == 1 ? 1 : 2;
    dev_bloque = obtenido.samples;
    acum = malloc((size_t)dev_bloque * 4 * dev_ch * sizeof *acum);
    if (!acum) { SDL_CloseAudio(); return -1; }
    if (!ncanales) Mix_AllocateChannels(MIX_CHANNELS);
    memset(&mus, 0, sizeof mus);
    mus.vol = MIX_MAX_VOLUME;
    abierto = 1;
    SDL_PauseAudio(0);
    registrar("[MIXER] audio abierto: %d Hz, %d canales, bloque de %d cuadros, %d canales de efectos\n",
              dev_freq, dev_ch, dev_bloque, ncanales);
    return 0;
}

void Mix_CloseAudio(void)
{
    if (!abierto) return;
    if (--abierto > 0) return;
    Mix_HaltChannel(-1);
    Mix_HaltMusic();
    n_post = 0;   /* los efectos post-mix se registran de nuevo al reabrir */
    gancho_musica = NULL;
    SDL_CloseAudio();
    free(acum); acum = NULL;
}

int Mix_QuerySpec(int *f, Uint16 *fmt, int *c)
{
    if (!abierto) return 0;
    if (f) *f = dev_freq;
    if (fmt) *fmt = AUDIO_S16SYS;
    if (c) *c = dev_ch;
    return 1;
}

/* ------------------------------------------------------------------ efectos */
Mix_Chunk *Mix_LoadWAV_RW(SDL_RWops *src, int liberar)
{
    if (!src) return NULL;
    size_t n = 0;
    unsigned char *d = SDL_LoadFile_RW(src, &n, liberar);
    if (!d || n < 12) { free(d); SDL_SetError("Mixer: no se puede leer el sonido"); return NULL; }
    if (!abierto) { free(d); SDL_SetError("Mixer: el audio no está abierto"); return NULL; }
    int16_t *pcm = NULL;
    int ch = 0;
    long rate = 0, frames = 0;
    if (!memcmp(d, "RIFF", 4)) pcm = wav_decodificar(d, n, &ch, &rate, &frames);
    else if (!memcmp(d, "OggS", 4)) {
        int c, r;
        short *s = NULL;
        int fr = stb_vorbis_decode_memory(d, (int)n, &c, &r, &s);
        if (fr > 0 && s) { pcm = s; ch = c; rate = r; frames = fr; }
    }
    free(d);
    if (!pcm) { SDL_SetError("Mixer: formato de sonido no admitido"); return NULL; }
    long nf = 0;
    int16_t *o = a_dispositivo(pcm, frames, ch > 2 ? 2 : ch, rate, &nf);
    free(pcm);
    if (!o) { SDL_SetError("Mixer: sin memoria"); return NULL; }
    Mix_Chunk *c = calloc(1, sizeof *c);
    if (!c) { free(o); return NULL; }
    c->allocated = 1; c->abuf = (Uint8 *)o; c->alen = (Uint32)(nf * dev_ch * 2); c->volume = MIX_MAX_VOLUME;
    return c;
}

Mix_Chunk *Mix_QuickLoad_RAW(Uint8 *mem, Uint32 n)
{
    Mix_Chunk *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->abuf = mem; c->alen = n; c->volume = MIX_MAX_VOLUME;
    return c;
}

void Mix_FreeChunk(Mix_Chunk *c)
{
    if (!c) return;
    for (int i = 0; i < ncanales; i++) if (canales[i].activo && canales[i].c == c) canales[i].activo = 0;
    if (c->allocated) free(c->abuf);
    free(c);
}

static int canal_libre(void)
{
    for (int i = 0; i < ncanales; i++) if (!canales[i].activo) return i;
    return -1;
}

int Mix_PlayChannelTimed(int canal, Mix_Chunk *c, int veces, int ms)
{
    if (!abierto || !c) { if (!c) SDL_SetError("Mixer: sonido nulo"); return -1; }
    if (canal < 0) canal = canal_libre();
    if (canal < 0 || canal >= ncanales) return -1;
    canal_t *k = &canales[canal];
    k->c = c; k->pos = 0; k->veces = veces < 0 ? -1 : veces > 0 ? veces - 1 : 0;
    k->activo = 1; k->pausado = 0; k->fade_total = k->fade_resta = 0;
    k->limite = ms < 0 ? -1 : (long)((long long)ms * dev_freq / 1000);
    if (!log_efecto) { log_efecto = 1; registrar("[MIXER] primer efecto reproducido en el canal %d (%u bytes)\n", canal, c->alen); }
    return canal;
}

int Mix_FadeInChannelTimed(int canal, Mix_Chunk *c, int veces, int ms_fundido, int ms)
{
    (void)ms_fundido;
    return Mix_PlayChannelTimed(canal, c, veces, ms);
}

int Mix_Volume(int canal, int v)
{
    if (canal < 0) {
        int ant = ncanales ? canales[0].vol : MIX_MAX_VOLUME;
        if (v >= 0) for (int i = 0; i < ncanales; i++) canales[i].vol = v > 128 ? 128 : v;
        return ant;
    }
    if (canal >= ncanales) return 0;
    int ant = canales[canal].vol;
    if (v >= 0) canales[canal].vol = v > 128 ? 128 : v;
    return ant;
}

int Mix_VolumeChunk(Mix_Chunk *c, int v)
{
    if (!c) return -1;
    int ant = c->volume;
    if (v >= 0) c->volume = (Uint8)(v > 128 ? 128 : v);
    return ant;
}

int Mix_HaltChannel(int canal)
{
    for (int i = 0; i < ncanales; i++) {
        if (canal >= 0 && i != canal) continue;
        if (canales[i].activo) { canales[i].activo = 0; if (al_acabar_canal) al_acabar_canal(i); }
    }
    return 0;
}
int Mix_ExpireChannel(int canal, int ms)
{
    long l = ms < 0 ? -1 : (long)((long long)ms * dev_freq / 1000);
    for (int i = 0; i < ncanales; i++) if ((canal < 0 || i == canal) && canales[i].activo) canales[i].limite = l;
    return 0;
}
int Mix_FadeOutChannel(int canal, int ms)
{
    long f = (long)((long long)ms * dev_freq / 1000);
    int n = 0;
    for (int i = 0; i < ncanales; i++)
        if ((canal < 0 || i == canal) && canales[i].activo) { canales[i].fade_total = canales[i].fade_resta = f > 0 ? f : 1; n++; }
    return n;
}
void Mix_Pause(int canal) { for (int i = 0; i < ncanales; i++) if (canal < 0 || i == canal) canales[i].pausado = 1; }
void Mix_Resume(int canal) { for (int i = 0; i < ncanales; i++) if (canal < 0 || i == canal) canales[i].pausado = 0; }
int Mix_Paused(int canal)
{
    int n = 0;
    for (int i = 0; i < ncanales; i++) if ((canal < 0 || i == canal) && canales[i].activo && canales[i].pausado) n++;
    return n;
}
int Mix_Playing(int canal)
{
    int n = 0;
    for (int i = 0; i < ncanales; i++) if ((canal < 0 || i == canal) && canales[i].activo) n++;
    return n;
}
void Mix_ChannelFinished(void (*f)(int)) { al_acabar_canal = f; }
Mix_Chunk *Mix_GetChunk(int canal) { return canal >= 0 && canal < ncanales ? canales[canal].c : NULL; }
int Mix_ReserveChannels(int n) { (void)n; return 0; }

/* ------------------------------------------------------------------ música */
static void musica_cerrar(void)
{
    if (mus.ogg) stb_vorbis_close(mus.ogg);
    if (mus.arq) audio_arquivo_fechar(mus.arq);
    mus.ogg = NULL; mus.arq = NULL; mus.m = NULL; mus.activa = 0; mus.n = 0; mus.pos = 0;
}

Mix_Music *Mix_LoadMUS_RW(SDL_RWops *src, int liberar)
{
    if (!src) return NULL;
    size_t n = 0;
    unsigned char *d = SDL_LoadFile_RW(src, &n, liberar);
    if (!d || n < 4) { free(d); SDL_SetError("Mixer: no se puede leer la música"); return NULL; }
    if (memcmp(d, "OggS", 4)) { free(d); SDL_SetError("Mixer: desde memoria solo se admite Ogg Vorbis"); return NULL; }
    Mix_Music *m = calloc(1, sizeof *m);
    if (!m) { free(d); return NULL; }
    m->tipo = MUS_OGG; m->datos = d; m->n = n;
    return m;
}

Mix_Music *Mix_LoadMUS(const char *ruta)
{
    FILE *f = fopen(ruta, "rb");
    if (!f) { SDL_SetError("Mixer: no se puede abrir %s", ruta); return NULL; }
    unsigned char cab[16];
    size_t k = fread(cab, 1, sizeof cab, f);
    if (k >= 4 && !memcmp(cab, "OggS", 4)) {
        fclose(f);
        return Mix_LoadMUS_RW(SDL_RWFromFile(ruta, "rb"), 1);
    }
    fclose(f);
    int fmt = audio_detectar(cab, (unsigned)k);
    if (fmt == AUDIO_FMT_NENHUM) { SDL_SetError("Mixer: formato de música no admitido: %s", ruta); return NULL; }
    Mix_Music *m = calloc(1, sizeof *m);
    if (!m) return NULL;
    m->tipo = fmt == AUDIO_FMT_MP3 ? MUS_MP3 : fmt == AUDIO_FMT_FLAC ? MUS_FLAC : fmt == AUDIO_FMT_MIDI ? MUS_MID : MUS_WAV;
    strncpy(m->ruta, ruta, sizeof m->ruta - 1);
    if (m->tipo == MUS_MID) {
        /* Los programas (Chocolate Doom) borran el MIDI temporal justo después de cargarlo: se guarda una copia
         * propia, que se borra con Mix_FreeMusic (D383). */
        char c[64];
        strcpy(c, "/tmp/mixmus-XXXXXX");   /* mkstemp: nombre único y nuevo (nunca un fichero ajeno) */
        int fd = mkstemp(c);
        FILE *o = fopen(ruta, "rb");
        int ok = fd >= 0 && o;
        if (ok) {
            char b[1024];
            size_t k;
            while (ok && (k = fread(b, 1, sizeof b, o)) > 0)
                if (write(fd, b, k) != (ssize_t)k) ok = 0;
        }
        if (o) fclose(o);
        if (fd >= 0 && close(fd) != 0) ok = 0;
        if (ok) {
            strncpy(m->ruta, c, sizeof m->ruta - 1);
            m->copia = 1;
        } else if (fd >= 0) {
            remove(c);   /* sin copia: se sigue con la ruta original (si el programa no la borra, suena igual) */
        }
    }
    return m;
}

void Mix_FreeMusic(Mix_Music *m)
{
    if (!m) return;
    if (mus.m == m) musica_cerrar();
    if (m->copia) remove(m->ruta);
    free(m->datos);
    free(m);
}
Mix_MusicType Mix_GetMusicType(const Mix_Music *m) { return m ? m->tipo : MUS_NONE; }

int Mix_FadeInMusic(Mix_Music *m, int veces, int ms)
{
    (void)ms;
    return Mix_PlayMusic(m, veces);
}

int Mix_PlayMusic(Mix_Music *m, int veces)
{
    if (!abierto || !m) return -1;
    musica_cerrar();
    if (m->tipo == MUS_OGG) {
        int err = 0;
        mus.ogg = stb_vorbis_open_memory(m->datos, (int)m->n, &err, NULL);
        if (!mus.ogg) { SDL_SetError("Mixer: Ogg Vorbis no válido (%d)", err); return -1; }
        stb_vorbis_info vi = stb_vorbis_get_info(mus.ogg);
        mus.src_ch = vi.channels > 2 ? 2 : vi.channels;
        mus.src_rate = (long)vi.sample_rate;
        if (vi.channels > 2) { stb_vorbis_close(mus.ogg); mus.ogg = NULL; SDL_SetError("Mixer: Ogg con más de 2 canales"); return -1; }
    } else {
        int erro = 0;
        mus.arq = audio_arquivo_abrir(m->ruta, &erro);
        if (!mus.arq) { SDL_SetError("Mixer: %s", audio_erro_texto(erro)); return -1; }
        const audio_info_t *i = audio_arquivo_info(mus.arq);
        mus.src_ch = i->canais == 1 ? 1 : 2;
        mus.src_rate = (long)i->taxa;
    }
    mus.m = m; mus.veces = veces < 0 ? -1 : veces > 0 ? veces - 1 : 0;
    mus.activa = 1; mus.pausada = 0; mus.fin = 0; mus.n = 0; mus.pos = 0; mus.fade_total = mus.fade_resta = 0;
    if (!log_musica) { log_musica = 1; registrar("[MIXER] música reproducida: %d Hz, %d canales de origen\n", (int)mus.src_rate, mus.src_ch); }
    return 0;
}

int Mix_VolumeMusic(int v)
{
    int ant = mus.vol;
    if (v >= 0) mus.vol = v > 128 ? 128 : v;
    return ant;
}
int Mix_HaltMusic(void) { int era = mus.activa; musica_cerrar(); if (era && al_acabar_musica) al_acabar_musica(); return 0; }
int Mix_FadeOutMusic(int ms)
{
    if (!mus.activa) return 0;
    long f = (long)((long long)ms * dev_freq / 1000);
    mus.fade_total = mus.fade_resta = f > 0 ? f : 1;
    return 1;
}
void Mix_PauseMusic(void) { mus.pausada = 1; }
void Mix_ResumeMusic(void) { mus.pausada = 0; }
void Mix_RewindMusic(void)
{
    if (mus.ogg) stb_vorbis_seek_start(mus.ogg);
    else if (mus.arq) audio_arquivo_buscar(mus.arq, 0);
    mus.n = 0; mus.pos = 0; mus.fin = 0;
}
int Mix_PausedMusic(void) { return mus.pausada; }
int Mix_PlayingMusic(void) { return mus.activa; }
Mix_Fading Mix_FadingMusic(void) { return mus.fade_total > 0 && mus.activa ? MIX_FADING_OUT : MIX_NO_FADING; }
void Mix_HookMusicFinished(void (*f)(void)) { al_acabar_musica = f; }

/* ---- añadidos para Chocolate Doom (D383) ---- */
int Mix_OpenAudioDevice(int frecuencia, Uint16 formato, int canales_, int tam_bloque, const char *dispositivo, int cambios)
{
    (void)dispositivo; (void)cambios;
    return Mix_OpenAudio(frecuencia, formato, canales_, tam_bloque);
}

int Mix_SetPanning(int canal, Uint8 izq, Uint8 der)
{
    if (canal < 0 || canal >= ncanales) return 0;
    canales[canal].pan_i = izq; canales[canal].pan_d = der;
    if (!izq && !der) canales[canal].pan_i = canales[canal].pan_d = 255;
    return 1;
}

void Mix_HookMusic(void (*f)(void *, Uint8 *, int), void *datos) { gancho_musica = f; gancho_datos = datos; }

int Mix_RegisterEffect(int canal, Mix_EffectFunc_t f, Mix_EffectDone_t fin, void *datos)
{
    (void)fin;
    if (canal != MIX_CHANNEL_POST || n_post >= 4) return 0;
    post[n_post].f = f; post[n_post].datos = datos; n_post++;
    return 1;
}

int Mix_SetMusicCMD(const char *cmd) { (void)cmd; return 0; }
int Mix_SetMusicPosition(double segundos) { (void)segundos; return -1; }
