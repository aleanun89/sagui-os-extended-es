/* openal_sagui.c - la OpenAL 1.1 del SaguiOS: un mezclador de software (D230).
 *
 * Lo que hay detrás de al.h y alc.h (ver allí qué hay y qué no). Código
 * nuestro; de OpenAL solo está la interfaz. El modelo es el de OpenAL Soft
 * en lo que los programas notan:
 *
 *   - una fuente toca su búfer (AL_BUFFER) o su cola de búferes
 *     (alSourceQueueBuffers) del primero al último; al acabar la cola se
 *     para (AL_STOPPED) y todos sus búferes cuentan como procesados;
 *   - AL_BUFFERS_PROCESSED son los búferes de la cola que ya sonaron
 *     enteros; alSourceUnqueueBuffers solo quita esos (si no, AL_INVALID_VALUE);
 *   - alSourcePlay sobre una fuente parada vuelve al principio de la cola;
 *     alSourceStop la para y da por procesada la cola entera;
 *   - alBufferData copia los datos (el programa puede reutilizar los suyos).
 *
 * Una diferencia, a propósito: una fuente de streaming que se queda sin
 * datos (la cola entera ya sonó) no pasa en seguida a AL_STOPPED, sino que
 * sigue AL_PLAYING «con hambre» hasta que el programa pregunta dos veces su
 * estado sin haberle dado más búferes, con al menos FOME_MS entre la
 * primera pregunta y la última (varias seguidas en el mismo instante no
 * cuentan: el programa pudo preguntar por otro motivo). Aquí la mezcla avanza solo cuando el
 * programa llama (no hay hilos), y en una pausa larga del programa (cargar
 * una misión) la cola se acaba aunque el programa la vaya a rellenar: los
 * que, como Vanilla Conquer, solo rellenan una fuente que está AL_PLAYING
 * se quedarían mudos para siempre. Quien espera a que el sonido acabe ve el
 * AL_STOPPED una consulta más tarde.
 *
 * La mezcla: las fuentes que suenan se suman en un acumulador de 32 bits,
 * estéreo, a la frecuencia de salida (22050 Hz; cada fuente se remuestrea
 * con interpolación lineal y su AL_PITCH), se aplican la ganancia de la
 * fuente y la del oyente y se satura a 16 bits. Sin hilos: la mezcla avanza
 * en sagui_al_bombear(), que llaman las funciones que consultan el estado
 * (alGetSource*, alSourceUnqueueBuffers...) y la SDL 2 del SaguiOS en su
 * bucle. Cada vez se deja por delante, en el flujo de sonido del sistema,
 * AL_ADIANTO_MS de sonido: un búfer cuenta como procesado cuando ya está
 * mezclado, que es como mucho ese tiempo antes de oírse.
 *
 * Sin tarjeta de sonido el dispositivo se abre igual y mezcla al ritmo del
 * reloj (sin escribir), para que los programas que esperan a que acabe un
 * sonido no se queden esperando.
 */
#include "al.h"
#include "alc.h"

#include "sagui.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define AL_FREQ_SAIDA   22050
#define AL_ADIANTO_MS   80     /* lo que se deja mezclado por delante */
#define AL_BLOCO        256    /* cuadros por vuelta del mezclador (~12 ms) */
#define MAX_FONTES      64
#define MAX_BUFERES     1024
#define MAX_FILA        64     /* búferes en la cola de una fuente */
#define FOME_MS         50     /* lo mínimo que una fuente sigue «con hambre» */

typedef struct {
    int usado;
    int16_t *dados;      /* siempre en 16 bits con signo, entrelazado */
    int canais;          /* 1 o 2 */
    int freq;
    int quadros;
    int bits_orig;       /* para AL_BITS/AL_SIZE */
    int refs;            /* fuentes que lo tienen en su cola */
} bufer_t;

typedef struct {
    int usado;
    ALenum estado;
    float ganho, tom;
    int laco;
    ALenum tipo;              /* AL_UNDETERMINED, AL_STATIC o AL_STREAMING */
    ALuint fila[MAX_FILA];
    int n_fila;
    int atual;                /* índice en la cola del que suena */
    uint32_t pos;             /* posición en el búfer actual, 16.16 cuadros */
    int fome;                 /* streaming sin datos (ver arriba) */
    int consultas_fome;       /* veces que se preguntó el estado con hambre */
    unsigned fome_desde;      /* tempo_ms() de la primera pregunta con hambre */
} fonte_t;

static bufer_t buferes[MAX_BUFERES + 1];   /* el 0 es AL_NONE */
static fonte_t fontes[MAX_FONTES + 1];
static ALenum erro_al = AL_NO_ERROR;
static float ganho_ouvinte = 1.0f;

struct ALCdevice { int fd; int freq; int aberto; unsigned ultimo_ms; uint64_t quadros_mudos; };
struct ALCcontext { ALCdevice *d; };
static ALCdevice dispositivo;
static ALCcontext contexto;
static ALCcontext *contexto_atual = NULL;
static ALCenum erro_alc = ALC_NO_ERROR;

static void erro(ALenum e)
{
    if (erro_al == AL_NO_ERROR) erro_al = e;
}

ALenum alGetError(void)
{
    ALenum e = erro_al;
    erro_al = AL_NO_ERROR;
    return e;
}

const ALchar *alGetString(ALenum p)
{
    switch (p) {
    case AL_VENDOR: return "SaguiOS";
    case AL_VERSION: return "1.1 SaguiOS";
    case AL_RENDERER: return "mezclador de software del SaguiOS";
    case AL_EXTENSIONS: return "";
    case AL_NO_ERROR: return "No Error";
    case AL_INVALID_NAME: return "Invalid Name";
    case AL_INVALID_ENUM: return "Invalid Enum";
    case AL_INVALID_VALUE: return "Invalid Value";
    case AL_INVALID_OPERATION: return "Invalid Operation";
    case AL_OUT_OF_MEMORY: return "Out of Memory";
    default: erro(AL_INVALID_ENUM); return NULL;
    }
}
ALboolean alIsExtensionPresent(const ALchar *nome) { (void)nome; return AL_FALSE; }
void alDistanceModel(ALenum m) { (void)m; }

/* ------------------------------------------------------------------ búferes */

static bufer_t *bufer(ALuint id)
{
    if (id == 0 || id > MAX_BUFERES || !buferes[id].usado) return NULL;
    return &buferes[id];
}

void alGenBuffers(ALsizei n, ALuint *ids)
{
    if (n < 0 || (n && !ids)) { erro(AL_INVALID_VALUE); return; }
    int feitos = 0;
    for (ALuint i = 1; i <= MAX_BUFERES && feitos < n; i++) {
        if (buferes[i].usado) continue;
        memset(&buferes[i], 0, sizeof buferes[i]);
        buferes[i].usado = 1;
        buferes[i].canais = 1;
        buferes[i].freq = AL_FREQ_SAIDA;
        buferes[i].bits_orig = 16;
        ids[feitos++] = i;
    }
    if (feitos < n) {   /* no hay sitio: se deshace todo, como OpenAL */
        for (int k = 0; k < feitos; k++) buferes[ids[k]].usado = 0;
        erro(AL_OUT_OF_MEMORY);
    }
}

void alDeleteBuffers(ALsizei n, const ALuint *ids)
{
    if (n < 0) { erro(AL_INVALID_VALUE); return; }
    for (int i = 0; i < n; i++) {
        if (ids[i] == 0) continue;
        bufer_t *b = bufer(ids[i]);
        if (!b) { erro(AL_INVALID_NAME); return; }
        if (b->refs > 0) { erro(AL_INVALID_OPERATION); return; }
    }
    for (int i = 0; i < n; i++) {
        bufer_t *b = bufer(ids[i]);
        if (!b) continue;
        free(b->dados);
        memset(b, 0, sizeof *b);
    }
}

ALboolean alIsBuffer(ALuint id) { return (id == 0 || bufer(id)) ? AL_TRUE : AL_FALSE; }

void alBufferData(ALuint id, ALenum formato, const ALvoid *dados, ALsizei tamanho, ALsizei freq)
{
    bufer_t *b = bufer(id);
    if (!b) { erro(AL_INVALID_NAME); return; }
    if (b->refs > 0) { erro(AL_INVALID_OPERATION); return; }
    if (tamanho < 0 || freq <= 0 || (tamanho && !dados)) { erro(AL_INVALID_VALUE); return; }
    int canais, bits;
    switch (formato) {
    case AL_FORMAT_MONO8: canais = 1; bits = 8; break;
    case AL_FORMAT_MONO16: canais = 1; bits = 16; break;
    case AL_FORMAT_STEREO8: canais = 2; bits = 8; break;
    case AL_FORMAT_STEREO16: canais = 2; bits = 16; break;
    default: erro(AL_INVALID_ENUM); return;
    }
    int quadros = tamanho / (canais * bits / 8);
    int16_t *d = quadros ? malloc((size_t)quadros * (size_t)canais * sizeof(int16_t)) : NULL;
    if (quadros && !d) { erro(AL_OUT_OF_MEMORY); return; }
    int n = quadros * canais;
    if (bits == 8) {
        const uint8_t *o = dados;
        for (int i = 0; i < n; i++) d[i] = (int16_t)((o[i] - 128) << 8);
    } else if (n) {
        memcpy(d, dados, (size_t)n * sizeof(int16_t));
    }
    free(b->dados);
    b->dados = d;
    b->canais = canais;
    b->freq = freq;
    b->quadros = quadros;
    b->bits_orig = bits;
}

void alGetBufferi(ALuint id, ALenum p, ALint *v)
{
    bufer_t *b = bufer(id);
    if (!b) { erro(AL_INVALID_NAME); return; }
    if (!v) { erro(AL_INVALID_VALUE); return; }
    switch (p) {
    case AL_FREQUENCY: *v = b->freq; break;
    case AL_BITS: *v = b->bits_orig; break;
    case AL_CHANNELS: *v = b->canais; break;
    case AL_SIZE: *v = b->quadros * b->canais * b->bits_orig / 8; break;
    default: erro(AL_INVALID_ENUM); break;
    }
}

/* ------------------------------------------------------------------ fuentes */

static fonte_t *fonte(ALuint id)
{
    if (id == 0 || id > MAX_FONTES || !fontes[id].usado) return NULL;
    return &fontes[id];
}

static void soltar_fila(fonte_t *f)
{
    for (int i = 0; i < f->n_fila; i++) {
        bufer_t *b = bufer(f->fila[i]);
        if (b && b->refs > 0) b->refs--;
    }
    f->n_fila = 0;
    f->atual = 0;
    f->pos = 0;
}

void alGenSources(ALsizei n, ALuint *ids)
{
    if (n < 0 || (n && !ids)) { erro(AL_INVALID_VALUE); return; }
    int feitos = 0;
    for (ALuint i = 1; i <= MAX_FONTES && feitos < n; i++) {
        if (fontes[i].usado) continue;
        memset(&fontes[i], 0, sizeof fontes[i]);
        fontes[i].usado = 1;
        fontes[i].estado = AL_INITIAL;
        fontes[i].ganho = 1.0f;
        fontes[i].tom = 1.0f;
        fontes[i].tipo = AL_UNDETERMINED;
        ids[feitos++] = i;
    }
    if (feitos < n) {
        for (int k = 0; k < feitos; k++) fontes[ids[k]].usado = 0;
        erro(AL_OUT_OF_MEMORY);
    }
}

void alDeleteSources(ALsizei n, const ALuint *ids)
{
    if (n < 0) { erro(AL_INVALID_VALUE); return; }
    for (int i = 0; i < n; i++)
        if (!fonte(ids[i])) { erro(AL_INVALID_NAME); return; }
    for (int i = 0; i < n; i++) {
        fonte_t *f = fonte(ids[i]);
        if (!f) continue;
        soltar_fila(f);
        f->usado = 0;
    }
}

ALboolean alIsSource(ALuint id) { return fonte(id) ? AL_TRUE : AL_FALSE; }

void alSourcef(ALuint id, ALenum p, ALfloat v)
{
    fonte_t *f = fonte(id);
    if (!f) { erro(AL_INVALID_NAME); return; }
    switch (p) {
    case AL_GAIN: if (v < 0) { erro(AL_INVALID_VALUE); return; } f->ganho = v; break;
    case AL_PITCH: if (v <= 0) { erro(AL_INVALID_VALUE); return; } f->tom = v; break;
    case AL_MIN_GAIN: case AL_MAX_GAIN: case AL_REFERENCE_DISTANCE: case AL_ROLLOFF_FACTOR:
    case AL_MAX_DISTANCE: case AL_CONE_INNER_ANGLE: case AL_CONE_OUTER_ANGLE: case AL_CONE_OUTER_GAIN:
        break;   /* del sonido 3D: se aceptan y no se usan */
    case AL_SEC_OFFSET: case AL_SAMPLE_OFFSET: case AL_BYTE_OFFSET:
        break;
    default: erro(AL_INVALID_ENUM); break;
    }
}
void alSource3f(ALuint id, ALenum p, ALfloat a, ALfloat b, ALfloat c)
{
    (void)a; (void)b; (void)c;
    if (!fonte(id)) { erro(AL_INVALID_NAME); return; }
    if (p != AL_POSITION && p != AL_VELOCITY && p != AL_DIRECTION) erro(AL_INVALID_ENUM);
}
void alSourcefv(ALuint id, ALenum p, const ALfloat *v)
{
    if (!v) { erro(AL_INVALID_VALUE); return; }
    if (p == AL_POSITION || p == AL_VELOCITY || p == AL_DIRECTION) alSource3f(id, p, v[0], v[1], v[2]);
    else alSourcef(id, p, v[0]);
}
void alSource3i(ALuint id, ALenum p, ALint a, ALint b, ALint c) { alSource3f(id, p, (float)a, (float)b, (float)c); }

void alSourcei(ALuint id, ALenum p, ALint v)
{
    fonte_t *f = fonte(id);
    if (!f) { erro(AL_INVALID_NAME); return; }
    switch (p) {
    case AL_LOOPING: f->laco = v != 0; break;
    case AL_SOURCE_RELATIVE: break;
    case AL_BUFFER:
        if (f->estado == AL_PLAYING || f->estado == AL_PAUSED) { erro(AL_INVALID_OPERATION); return; }
        if (v != 0 && !bufer((ALuint)v)) { erro(AL_INVALID_VALUE); return; }
        soltar_fila(f);
        if (v) {
            f->fila[0] = (ALuint)v;
            f->n_fila = 1;
            buferes[v].refs++;
            f->tipo = AL_STATIC;
        } else {
            f->tipo = AL_UNDETERMINED;
        }
        f->estado = AL_INITIAL;
        break;
    case AL_GAIN: case AL_PITCH: alSourcef(id, p, (float)v); break;
    default: erro(AL_INVALID_ENUM); break;
    }
}

/* Los búferes de la cola que ya sonaron enteros. */
static int processados(const fonte_t *f)
{
    if (f->tipo == AL_STATIC || f->laco) return 0;
    if (f->estado == AL_STOPPED) return f->n_fila;
    if (f->estado == AL_INITIAL) return 0;
    return f->atual;
}

void alGetSourcei(ALuint id, ALenum p, ALint *v)
{
    sagui_al_bombear();
    fonte_t *f = fonte(id);
    if (!f) { erro(AL_INVALID_NAME); return; }
    if (!v) { erro(AL_INVALID_VALUE); return; }
    switch (p) {
    case AL_SOURCE_STATE:
        if (f->fome && ++f->consultas_fome == 1) f->fome_desde = tempo_ms();
        if (f->fome && f->consultas_fome >= 2 && tempo_ms() - f->fome_desde >= FOME_MS) {
            /* no le dieron más: ahora sí, parada */
            f->fome = 0;
            f->estado = AL_STOPPED;
        }
        *v = f->estado;
        break;
    case AL_BUFFERS_QUEUED: *v = f->n_fila; break;
    case AL_BUFFERS_PROCESSED: *v = processados(f); break;
    case AL_LOOPING: *v = f->laco; break;
    case AL_BUFFER: *v = (f->tipo == AL_STATIC && f->n_fila) ? (ALint)f->fila[0] : 0; break;
    case AL_SOURCE_TYPE: *v = f->tipo; break;
    case AL_SAMPLE_OFFSET: *v = (f->estado == AL_PLAYING || f->estado == AL_PAUSED) ? (ALint)(f->pos >> 16) : 0; break;
    default: erro(AL_INVALID_ENUM); break;
    }
}
void alGetSourcef(ALuint id, ALenum p, ALfloat *v)
{
    fonte_t *f = fonte(id);
    if (!f) { erro(AL_INVALID_NAME); return; }
    if (!v) { erro(AL_INVALID_VALUE); return; }
    switch (p) {
    case AL_GAIN: *v = f->ganho; break;
    case AL_PITCH: *v = f->tom; break;
    default: { ALint i = 0; alGetSourcei(id, p, &i); *v = (float)i; break; }
    }
}

void alSourcePlay(ALuint id)
{
    fonte_t *f = fonte(id);
    if (!f) { erro(AL_INVALID_NAME); return; }
    f->fome = 0;
    if (f->estado == AL_PAUSED) { f->estado = AL_PLAYING; return; }
    if (f->estado == AL_PLAYING) {   /* otra vez Play: vuelve a empezar */
        f->atual = 0;
        f->pos = 0;
        return;
    }
    f->atual = 0;
    f->pos = 0;
    f->estado = f->n_fila ? AL_PLAYING : AL_STOPPED;
}
void alSourceStop(ALuint id)
{
    fonte_t *f = fonte(id);
    if (!f) { erro(AL_INVALID_NAME); return; }
    f->fome = 0;
    if (f->estado != AL_INITIAL) f->estado = AL_STOPPED;
    f->atual = f->n_fila;
    f->pos = 0;
}
void alSourcePause(ALuint id)
{
    fonte_t *f = fonte(id);
    if (!f) { erro(AL_INVALID_NAME); return; }
    if (f->estado == AL_PLAYING) f->estado = AL_PAUSED;
}
void alSourceRewind(ALuint id)
{
    fonte_t *f = fonte(id);
    if (!f) { erro(AL_INVALID_NAME); return; }
    f->fome = 0;
    f->estado = AL_INITIAL;
    f->atual = 0;
    f->pos = 0;
}
void alSourcePlayv(ALsizei n, const ALuint *ids) { for (int i = 0; i < n; i++) alSourcePlay(ids[i]); }
void alSourceStopv(ALsizei n, const ALuint *ids) { for (int i = 0; i < n; i++) alSourceStop(ids[i]); }
void alSourcePausev(ALsizei n, const ALuint *ids) { for (int i = 0; i < n; i++) alSourcePause(ids[i]); }

void alSourceQueueBuffers(ALuint id, ALsizei n, const ALuint *ids)
{
    fonte_t *f = fonte(id);
    if (!f) { erro(AL_INVALID_NAME); return; }
    if (f->tipo == AL_STATIC) { erro(AL_INVALID_OPERATION); return; }
    if (n < 0 || f->n_fila + n > MAX_FILA) { erro(AL_INVALID_VALUE); return; }
    for (int i = 0; i < n; i++)
        if (ids[i] && !bufer(ids[i])) { erro(AL_INVALID_NAME); return; }
    for (int i = 0; i < n; i++) {
        f->fila[f->n_fila++] = ids[i];
        if (ids[i]) buferes[ids[i]].refs++;
    }
    f->tipo = AL_STREAMING;
    /* Con hambre, sigue sonando con lo nuevo. Una fuente ya parada sigue
     * parada: el programa vuelve a llamar a alSourcePlay (como en OpenAL). */
    if (n > 0) {
        f->fome = 0;
        f->consultas_fome = 0;
    }
}

void alSourceUnqueueBuffers(ALuint id, ALsizei n, ALuint *ids)
{
    sagui_al_bombear();
    fonte_t *f = fonte(id);
    if (!f) { erro(AL_INVALID_NAME); return; }
    if (n < 0 || n > processados(f) || f->tipo != AL_STREAMING) { if (n) erro(AL_INVALID_VALUE); return; }
    for (int i = 0; i < n; i++) {
        ALuint b = f->fila[i];
        if (ids) ids[i] = b;
        if (bufer(b) && buferes[b].refs > 0) buferes[b].refs--;
    }
    memmove(f->fila, f->fila + n, (size_t)(f->n_fila - n) * sizeof(ALuint));
    f->n_fila -= n;
    if (f->estado == AL_STOPPED) f->atual = f->n_fila;
    else f->atual -= n;
    if (f->atual < 0) f->atual = 0;
}

void alListenerf(ALenum p, ALfloat v)
{
    if (p == AL_GAIN) { if (v < 0) erro(AL_INVALID_VALUE); else ganho_ouvinte = v; }
}
void alListener3f(ALenum p, ALfloat a, ALfloat b, ALfloat c) { (void)p; (void)a; (void)b; (void)c; }
void alListenerfv(ALenum p, const ALfloat *v) { if (p == AL_GAIN && v) alListenerf(p, v[0]); }
void alGetListenerf(ALenum p, ALfloat *v)
{
    if (!v) { erro(AL_INVALID_VALUE); return; }
    if (p == AL_GAIN) *v = ganho_ouvinte;
    else erro(AL_INVALID_ENUM);
}

/* ------------------------------------------------------------------ la mezcla */

/* Suma una fuente a acc[2 * quadros]. Avanza su posición y su cola. */
static void misturar_fonte(fonte_t *f, int32_t *acc, int quadros)
{
    int g = (int)(f->ganho * ganho_ouvinte * 256.0f);   /* ganancia en 8.8 */
    if (g > 4 * 256) g = 4 * 256;
    int k = 0;
    while (k < quadros && f->estado == AL_PLAYING) {
        if (f->atual >= f->n_fila) {
            if (f->laco && f->n_fila) { f->atual = 0; f->pos = 0; continue; }
            if (f->tipo == AL_STREAMING) {   /* con hambre: espera más búferes */
                if (!f->fome) { f->fome = 1; f->consultas_fome = 0; }
                break;
            }
            f->estado = AL_STOPPED;
            break;
        }
        bufer_t *b = bufer(f->fila[f->atual]);
        if (!b || !b->quadros) { f->atual++; f->pos = 0; continue; }
        /* El paso: frecuencia del búfer / la de salida, por el tono, en 16.16. */
        uint32_t passo = (uint32_t)((double)b->freq * f->tom * 65536.0 / AL_FREQ_SAIDA);
        if (!passo) passo = 1;
        const int16_t *d = b->dados;
        int c = b->canais;
        uint32_t fim = (uint32_t)b->quadros << 16;
        while (k < quadros && f->pos < fim) {
            int i = (int)(f->pos >> 16);
            int fr = (int)(f->pos & 0xFFFF);
            int i2 = i + 1 < b->quadros ? i + 1 : i;
            int l, r;
            if (c == 1) {
                l = d[i] + (((d[i2] - d[i]) * fr) >> 16);
                r = l;
            } else {
                l = d[2 * i] + (((d[2 * i2] - d[2 * i]) * fr) >> 16);
                r = d[2 * i + 1] + (((d[2 * i2 + 1] - d[2 * i + 1]) * fr) >> 16);
            }
            acc[2 * k] += (l * g) >> 8;
            acc[2 * k + 1] += (r * g) >> 8;
            k++;
            f->pos += passo;
        }
        if (f->pos >= fim) {   /* este búfer ya sonó entero: el siguiente */
            f->pos -= fim;
            f->atual++;
            if (f->tipo == AL_STATIC) {
                if (f->laco) { f->atual = 0; continue; }
                f->estado = AL_STOPPED;
                f->atual = 0;
                break;
            }
        }
    }
}

static void misturar(int16_t *saida, int quadros)
{
    static int32_t acc[2 * AL_BLOCO];
    memset(acc, 0, (size_t)quadros * 2 * sizeof(int32_t));
    for (int i = 1; i <= MAX_FONTES; i++)
        if (fontes[i].usado && fontes[i].estado == AL_PLAYING) misturar_fonte(&fontes[i], acc, quadros);
    for (int i = 0; i < 2 * quadros; i++) {
        int32_t v = acc[i];
        saida[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
}

static int alguma_tocando(void)
{
    for (int i = 1; i <= MAX_FONTES; i++)
        if (fontes[i].usado && fontes[i].estado == AL_PLAYING &&
            (!fontes[i].fome || fontes[i].atual < fontes[i].n_fila))
            return 1;
    return 0;
}

void sagui_al_bombear(void)
{
    ALCdevice *d = &dispositivo;
    if (!d->aberto || !contexto_atual) return;
    static int16_t bloco[2 * AL_BLOCO];
    if (d->fd < 0) {
        /* Sin tarjeta: se «toca» al ritmo del reloj, sin escribir. */
        unsigned agora = tempo_ms();
        unsigned ms = agora - d->ultimo_ms;
        if (ms > 1000) ms = 1000;
        int quadros = (int)((uint64_t)ms * AL_FREQ_SAIDA / 1000);
        if (quadros < AL_BLOCO) return;
        d->ultimo_ms = agora;
        while (quadros > 0) {
            int q = quadros > AL_BLOCO ? AL_BLOCO : quadros;
            misturar(bloco, q);
            quadros -= q;
        }
        return;
    }
    /* Con tarjeta: se rellena hasta AL_ADIANTO_MS por delante. */
    for (int vezes = 0; vezes < 16; vezes++) {
        if (audio_pendente(d->fd) >= AL_ADIANTO_MS) return;
        if (audio_livre(d->fd) < (int)sizeof bloco) return;
        if (!alguma_tocando()) {
            /* Nada que tocar: no se escribe silencio (el flujo se vacía y
             * el siguiente sonido empieza sin retraso). */
            return;
        }
        misturar(bloco, AL_BLOCO);
        audio_escrever(d->fd, bloco, sizeof bloco);
    }
}

/* ------------------------------------------------------------------ dispositivo y contexto */

ALCdevice *alcOpenDevice(const ALCchar *nome)
{
    (void)nome;
    if (dispositivo.aberto) { erro_alc = ALC_INVALID_DEVICE; return NULL; }
    dispositivo.fd = audio_abrir(AL_FREQ_SAIDA, 2, 16);
    if (dispositivo.fd < 0) dispositivo.fd = -1;
    dispositivo.freq = AL_FREQ_SAIDA;
    dispositivo.aberto = 1;
    dispositivo.ultimo_ms = tempo_ms();
    registrar("[OPENAL] dispositivo abierto: %d Hz, estereo, 16 bits (%s)", AL_FREQ_SAIDA,
              dispositivo.fd >= 0 ? "con tarjeta" : "mudo");
    return &dispositivo;
}

ALCboolean alcCloseDevice(ALCdevice *d)
{
    if (d != &dispositivo || !d->aberto) { erro_alc = ALC_INVALID_DEVICE; return ALC_FALSE; }
    if (d->fd >= 0) audio_fechar(d->fd, 0);
    d->fd = -1;
    d->aberto = 0;
    return ALC_TRUE;
}

ALCcontext *alcCreateContext(ALCdevice *d, const ALCint *atributos)
{
    (void)atributos;
    if (d != &dispositivo || !d->aberto) { erro_alc = ALC_INVALID_DEVICE; return NULL; }
    contexto.d = d;
    return &contexto;
}
void alcDestroyContext(ALCcontext *c)
{
    if (c != &contexto) { erro_alc = ALC_INVALID_CONTEXT; return; }
    if (contexto_atual == c) contexto_atual = NULL;
    for (int i = 1; i <= MAX_FONTES; i++)
        if (fontes[i].usado) { soltar_fila(&fontes[i]); fontes[i].usado = 0; }
}
ALCboolean alcMakeContextCurrent(ALCcontext *c)
{
    if (c && c != &contexto) { erro_alc = ALC_INVALID_CONTEXT; return ALC_FALSE; }
    contexto_atual = c;
    if (c) dispositivo.ultimo_ms = tempo_ms();
    return ALC_TRUE;
}
ALCcontext *alcGetCurrentContext(void) { return contexto_atual; }
ALCdevice *alcGetContextsDevice(ALCcontext *c) { return c ? c->d : NULL; }
/* Como en OpenAL Soft: suspender solo aplaza los cambios; la mezcla sigue. */
void alcProcessContext(ALCcontext *c) { (void)c; }
void alcSuspendContext(ALCcontext *c) { (void)c; }

ALCenum alcGetError(ALCdevice *d)
{
    (void)d;
    ALCenum e = erro_alc;
    erro_alc = ALC_NO_ERROR;
    return e;
}
const ALCchar *alcGetString(ALCdevice *d, ALCenum p)
{
    (void)d;
    switch (p) {
    case ALC_DEFAULT_DEVICE_SPECIFIER: case ALC_DEVICE_SPECIFIER: return "SaguiOS";
    case ALC_EXTENSIONS: return "";
    default: return "";
    }
}
void alcGetIntegerv(ALCdevice *d, ALCenum p, ALCsizei n, ALCint *v)
{
    (void)d;
    if (!v || n < 1) return;
    switch (p) {
    case ALC_FREQUENCY: *v = AL_FREQ_SAIDA; break;
    case ALC_MAJOR_VERSION: *v = 1; break;
    case ALC_MINOR_VERSION: *v = 1; break;
    case ALC_MONO_SOURCES: *v = MAX_FONTES; break;
    case ALC_STEREO_SOURCES: *v = MAX_FONTES; break;
    default: *v = 0; break;
    }
}
ALCboolean alcIsExtensionPresent(ALCdevice *d, const ALCchar *nome) { (void)d; (void)nome; return ALC_FALSE; }
