// SPDX-License-Identifier: GPL-2.0-or-later
/* openal_unidad.c - prueba de unidad en el PC de la OpenAL del SaguiOS (D230, h230).
 *
 *   gcc -O2 -Wall -Wextra -I userland/libopenal -I userland/inc -o openal_unidad tests/openal_unidad.c
 *
 * Compila el mezclador (userland/libopenal/openal_sagui.c) con un flujo de
 * sonido y un reloj falsos (el flujo «toca» a 22050 Hz según el reloj) y
 * comprueba: un búfer estático suena entero y la fuente se para; el
 * remuestreo (11025 Hz da el doble de cuadros); la ganancia; la cola de
 * streaming como la usa Vanilla Conquer (dos búferes que se rellenan cuando
 * se procesan) suena entera y en orden; una pausa larga del programa no deja
 * la fuente parada con datos por tocar (AL_PLAYING «con hambre») y, sin más
 * datos, pasa a AL_STOPPED a la segunda consulta; alSourceUnqueueBuffers de
 * más da AL_INVALID_VALUE; alSourceStop da la cola entera por procesada.
 * Escribe "todo bien" y sale con 0 si pasa todo. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

/* ---- el SaguiOS de mentira ---- */
#define SAGUI_H
static unsigned agora_ms = 1000;
static int16_t *saida;              /* lo que se escribió en el flujo (estéreo) */
static size_t saida_n, saida_cap;   /* en muestras (2 por cuadro) */
static double tocados = 0;          /* cuadros que el flujo ya «tocó» */
#define FD_FALSO 7

static unsigned tempo_ms(void) { return agora_ms; }
static int audio_abrir(int taxa, int canais, int bits) { (void)taxa; (void)canais; (void)bits; return FD_FALSO; }
static int audio_fechar(int fd, int esperar) { (void)fd; (void)esperar; return 0; }
static void avancar(unsigned ms)
{
    agora_ms += ms;
    tocados += ms * 22.05;
    if (tocados > saida_n / 2) tocados = (double)(saida_n / 2);   /* se vació: no toca de más */
}
static int audio_pendente(int fd) { (void)fd; return (int)((saida_n / 2 - tocados) / 22.05); }
static int audio_livre(int fd) { (void)fd; return 65536; }
static int audio_escrever(int fd, const void *buf, unsigned n)
{
    (void)fd;
    size_t k = n / 2;
    if (saida_n + k > saida_cap) {
        saida_cap = (saida_n + k) * 2;
        saida = realloc(saida, saida_cap * sizeof(int16_t));
    }
    memcpy(saida + saida_n, buf, n);
    saida_n += k;
    return (int)n;
}
static void registrar(const char *fmt, ...) { (void)fmt; }

#include "../userland/libopenal/openal_sagui.c"

static int fallos, total;
#define COMPROBAR(c, ...) do { total++; if (!(c)) { fallos++; printf("FALLO %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void limpar_saida(void) { saida_n = 0; tocados = 0; }

/* Deja correr el tiempo llamando al mezclador (como la SDL en su bucle). */
static void correr(unsigned ms)
{
    for (unsigned t = 0; t < ms; t += 10) {
        avancar(10);
        sagui_al_bombear();
    }
}

static size_t cuadros_com_som(void)
{
    size_t n = 0;
    for (size_t i = 0; i < saida_n / 2; i++)
        if (saida[2 * i] || saida[2 * i + 1]) n++;
    return n;
}

static void prueba_estatico(void)
{
    ALuint b, s;
    alGenBuffers(1, &b);
    alGenSources(1, &s);
    COMPROBAR(alGetError() == AL_NO_ERROR, "crear un búfer y una fuente");
    /* 1 s de 8 bits mono a 11025 Hz, un valor fijo (no cero) */
    static uint8_t d[11025];
    memset(d, 128 + 64, sizeof d);
    alBufferData(b, AL_FORMAT_MONO8, d, sizeof d, 11025);
    alSourcei(s, AL_BUFFER, (ALint)b);
    limpar_saida();
    alSourcePlay(s);
    ALint st;
    alGetSourcei(s, AL_SOURCE_STATE, &st);
    COMPROBAR(st == AL_PLAYING, "tras alSourcePlay, AL_PLAYING (%x)", st);
    correr(1500);
    alGetSourcei(s, AL_SOURCE_STATE, &st);
    COMPROBAR(st == AL_STOPPED, "acabado el búfer estático, AL_STOPPED (%x)", st);
    size_t n = cuadros_com_som();
    COMPROBAR(n >= 22040 && n <= 22060, "11025 cuadros a 11025 Hz dan ~22050 a 22050 Hz (%zu)", n);
    /* 64 << 8 = 16384, centrado */
    int v = saida[2 * 100];
    COMPROBAR(v > 16000 && v < 16800 && saida[2 * 100 + 1] == v, "la muestra (64 sobre 128) da ~16384 en los dos canales (%d)", v);
    /* ganancia 0,5 */
    alSourcef(s, AL_GAIN, 0.5f);
    limpar_saida();
    alSourcePlay(s);
    correr(300);
    v = saida[2 * 100];
    COMPROBAR(v > 8000 && v < 8400, "con AL_GAIN 0,5, la mitad (%d)", v);
    alSourceStop(s);
    alGetSourcei(s, AL_SOURCE_STATE, &st);
    COMPROBAR(st == AL_STOPPED, "alSourceStop para la fuente");
    alDeleteBuffers(1, &b);
    COMPROBAR(alGetError() == AL_INVALID_OPERATION, "borrar un búfer en uso: AL_INVALID_OPERATION");
    alDeleteSources(1, &s);
    alDeleteBuffers(1, &b);
    COMPROBAR(alGetError() == AL_NO_ERROR, "borrar la fuente y luego el búfer");
}

/* Como SoundImp_* de Vanilla Conquer (common/soundio_openal.cpp): dos búferes;
 * se rellenan los procesados mientras la fuente suena. */
#define TROZO 2048   /* muestras de 16 bits mono por búfer */
static int16_t fonte_dados[TROZO * 20];
static int fonte_pos;

static int livres(ALuint s, int *nao_usados)
{
    ALint st, p = 0;
    alGetSourcei(s, AL_SOURCE_STATE, &st);
    if (st == AL_PLAYING) alGetSourcei(s, AL_BUFFERS_PROCESSED, &p);
    return *nao_usados + p;
}

static void encher(ALuint s, ALuint *bufs, int *nao_usados)
{
    ALuint b;
    if (*nao_usados) {
        b = bufs[2 - *nao_usados];
        (*nao_usados)--;
    } else {
        alSourceUnqueueBuffers(s, 1, &b);
    }
    alBufferData(b, AL_FORMAT_MONO16, fonte_dados + fonte_pos, TROZO * 2, 22050);
    alSourceQueueBuffers(s, 1, &b);
    fonte_pos += TROZO;
}

static void prueba_streaming(void)
{
    for (int i = 0; i < TROZO * 20; i++) fonte_dados[i] = (int16_t)(1 + i % 1000);   /* rampa, nunca cero */
    ALuint s, bufs[2];
    alGenSources(1, &s);
    alGenBuffers(2, bufs);
    int nao_usados = 2;
    fonte_pos = 0;
    limpar_saida();
    encher(s, bufs, &nao_usados);
    encher(s, bufs, &nao_usados);
    alSourcePlay(s);
    int pausa_feita = 0;
    /* El bucle del juego: un cuadro cada 16 ms; a la mitad, una pausa de 600 ms
     * sin atender el sonido (cargar algo), pero con la SDL bombeando. */
    for (int quadro = 0; quadro < 400 && fonte_pos < TROZO * 20; quadro++) {
        avancar(16);
        sagui_al_bombear();
        if (!pausa_feita && fonte_pos >= TROZO * 10) {
            correr(600);
            pausa_feita = 1;
            ALint st;
            alGetSourcei(s, AL_SOURCE_STATE, &st);
            COMPROBAR(st == AL_PLAYING, "tras la pausa, la fuente sin datos sigue AL_PLAYING «con hambre» (%x)", st);
        }
        int k = livres(s, &nao_usados);
        while (k-- > 0 && fonte_pos < TROZO * 20) encher(s, bufs, &nao_usados);
    }
    COMPROBAR(alGetError() == AL_NO_ERROR, "sin errores de OpenAL en el streaming");
    correr(1000);
    /* Todo lo que se dio, en orden, sin huecos dentro de cada trozo: se
     * reconstruye la señal quitando los silencios (la pausa). */
    size_t j = 0, malos = 0;
    for (size_t i = 0; i < saida_n / 2 && j < (size_t)TROZO * 20; i++) {
        int16_t v = saida[2 * i];
        if (!v) continue;
        if (v != fonte_dados[j]) malos++;
        j++;
    }
    COMPROBAR(j == (size_t)TROZO * 20, "sonaron todas las muestras (%zu de %d)", j, TROZO * 20);
    COMPROBAR(malos == 0, "en orden y sin cambios (%zu distintas)", malos);
    /* Sin más datos: AL_STOPPED a la segunda consulta del estado (pasado FOME_MS). */
    ALint st1, st2;
    alGetSourcei(s, AL_SOURCE_STATE, &st1);
    avancar(60);
    alGetSourcei(s, AL_SOURCE_STATE, &st2);
    COMPROBAR(st2 == AL_STOPPED, "sin más datos, AL_STOPPED (consultas: %x, %x)", st1, st2);
    ALint p;
    alGetSourcei(s, AL_BUFFERS_PROCESSED, &p);
    COMPROBAR(p == 2, "parada, los dos búferes cuentan como procesados (%d)", p);
    ALuint tres[3];
    alSourceUnqueueBuffers(s, 3, tres);
    COMPROBAR(alGetError() == AL_INVALID_VALUE, "quitar más búferes de los procesados: AL_INVALID_VALUE");
    alSourceUnqueueBuffers(s, 2, tres);
    alGetSourcei(s, AL_BUFFERS_QUEUED, &p);
    COMPROBAR(alGetError() == AL_NO_ERROR && p == 0, "quitados los dos, la cola queda vacía (%d)", p);
    alDeleteSources(1, &s);
    alDeleteBuffers(2, bufs);
}

static void prueba_nomes(void)
{
    COMPROBAR(alIsSource(0) == AL_FALSE, "alIsSource(0) es falso (Vanilla Conquer lo pregunta antes de crear)");
    COMPROBAR(alIsSource(9999) == AL_FALSE, "alIsSource de un nombre que no existe es falso");
    ALint v;
    alGetSourcei(12345, AL_SOURCE_STATE, &v);
    COMPROBAR(alGetError() == AL_INVALID_NAME, "una fuente que no existe: AL_INVALID_NAME");
}

int main(void)
{
    ALCdevice *d = alcOpenDevice(NULL);
    ALCcontext *c = alcCreateContext(d, NULL);
    COMPROBAR(d && c && alcMakeContextCurrent(c), "abrir el dispositivo y el contexto");
    prueba_nomes();
    prueba_estatico();
    prueba_streaming();
    alcMakeContextCurrent(NULL);
    alcDestroyContext(c);
    alcCloseDevice(d);
    printf("%d comprobaciones, %d fallos\n", total, fallos);
    if (!fallos) printf("todo bien\n");
    return fallos ? 1 : 0;
}
