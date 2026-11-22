// SPDX-License-Identifier: GPL-2.0-or-later
/* sdl2audioprova.c - /apps/bin/sdl2audio: prueba del audio de la SDL 2 del sistema (D393, hilo de audio; prueba h393).
 *
 *   sdl2audio MODO [segundos] [bloqueo_ms]
 *     callback  un seno de 440 Hz (44100 Hz, estéreo, S16) por la llamada de vuelta; cada segundo el hilo principal se
 *               queda ocupado `bloqueo_ms` (400 por defecto) sin llamar a la SDL, como un cuadro atascado
 *     cola      lo mismo por SDL_QueueAudio, con un adelanto de ~700 ms en la cola
 *     formato   F32 mono a 22050 Hz sin dejar cambiar el formato (como Brumacanto): un seno de 880 Hz por la cola;
 *               el .wav tiene que sonar a 880 Hz y a la mitad de la amplitud, no a ruido
 *     cerrojo   el hilo principal toma SDL_LockAudio y toca dos contadores que la llamada de vuelta compara: no deben
 *               verse nunca distintos
 *     pausa     SDL_PauseAudio(1) calla la llamada de vuelta y SDL_PauseAudio(0) la devuelve
 *   Con SDL_SAGUI_AUDIO_HILO=0 el mismo programa corre con el audio movido por el hilo principal (el modo de antes).
 * Escribe por el registro una línea «SDLAUDIO modo=... clave=valor ...» al acabar. */
#include "sagui.h"
#include "SDL.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TASA 44100

static volatile int llamadas, tid_llamada, tid_distinto, difs, pausado_visto;
static volatile unsigned gran_hueco_llamadas;
static unsigned ult_llamada;
static volatile int a_cerrojo, b_cerrojo;
static unsigned fase;   /* fase del seno en muestras */

static void llamada(void *ud, Uint8 *flujo, int n)
{
    (void)ud;
    Sint16 *s = (Sint16 *)flujo;
    int cuadros = n / 4;
    for (int i = 0; i < cuadros; i++) {
        float v = 0.5f * sinf(6.2831853f * 440.0f * (float)(fase++ % TASA) / TASA);
        s[2 * i] = s[2 * i + 1] = (Sint16)(v * 32767.0f);
    }
    unsigned ahora = SDL_GetTicks();
    if (ult_llamada && ahora - ult_llamada > gran_hueco_llamadas) gran_hueco_llamadas = ahora - ult_llamada;
    ult_llamada = ahora;
    if (!tid_llamada) tid_llamada = (int)SDL_ThreadID();
    if (llamadas > 0 && a_cerrojo != b_cerrojo) difs++;   /* la llamada de vuelta vio el cerrojo a medias */
    if (pausado_visto) pausado_visto++;
    llamadas++;
}

/* Un trozo de la cola: n cuadros del seno en el formato pedido */
static unsigned fase_cola;
static void trozo_cola(int formato_f32, int cuadros)
{
    static Sint16 s16[8192 * 2];
    static float f32[8192];
    if (cuadros > 8192) cuadros = 8192;
    if (formato_f32) {
        for (int i = 0; i < cuadros; i++) f32[i] = 0.5f * sinf(6.2831853f * 880.0f * (float)(fase_cola++ % 22050) / 22050.0f);
    } else {
        for (int i = 0; i < cuadros; i++) {
            float v = 0.5f * sinf(6.2831853f * 440.0f * (float)(fase_cola++ % TASA) / TASA);
            s16[2 * i] = s16[2 * i + 1] = (Sint16)(v * 32767.0f);
        }
    }
    SDL_QueueAudio(1, formato_f32 ? (void *)f32 : (void *)s16, (Uint32)cuadros * (4u));
}

static int hilo_activo(void)
{
    const char *e = getenv("SDL_SAGUI_AUDIO_HILO");
    return e && *e ? e[0] != '0' : 1;
}

static void ocupado(unsigned ms)
{
    unsigned t0 = SDL_GetTicks();
    while (SDL_GetTicks() - t0 < ms) { }
}

int principal(int argc, char **argv)
{
    const char *modo = argc > 1 ? argv[1] : "callback";
    int seg = argc > 2 ? atoi(argv[2]) : 8;
    int bloqueo = argc > 3 ? atoi(argv[3]) : 400;
    sagui_sdl2_nome = "SDL2AUDIO";
    if (SDL_Init(SDL_INIT_AUDIO) < 0) { printf("sdl2audio: SDL_Init falla\n"); return 1; }
    SDL_AudioSpec q, o;
    SDL_zero(q);
    int cola = !strcmp(modo, "cola"), f32 = !strcmp(modo, "formato");
    q.freq = f32 ? 22050 : TASA;
    q.format = f32 ? AUDIO_F32SYS : AUDIO_S16SYS;
    q.channels = f32 ? 1 : 2;
    q.samples = 1024;
    q.callback = (cola || f32) ? NULL : llamada;
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &q, f32 ? NULL : &o, 0);
    if (!dev) { printf("sdl2audio: SDL_OpenAudioDevice: %s\n", SDL_GetError()); return 1; }
    int tid_main = (int)SDL_ThreadID();
    unsigned t0 = SDL_GetTicks();
    int bloqueos = 0, pausas_ok = 0;
    SDL_PauseAudioDevice(dev, 0);

    if (cola || f32) {
        /* mantener unos 700 ms en la cola; el bloqueo del principal se cubre con ese adelanto */
        int cuadros_s = f32 ? 22050 : TASA, ancho = 4;
        unsigned objetivo = (unsigned)cuadros_s * 7 / 10 * ancho;
        unsigned ult_bloqueo = SDL_GetTicks();
        while (SDL_GetTicks() - t0 < (unsigned)seg * 1000u) {
            while (SDL_GetQueuedAudioSize(dev) < objetivo) trozo_cola(f32, 1024);
            if (SDL_GetTicks() - ult_bloqueo >= 1000) { ocupado((unsigned)bloqueo); bloqueos++; ult_bloqueo = SDL_GetTicks(); }
            SDL_Delay(10);
        }
        unsigned resto = SDL_GetQueuedAudioSize(dev);
        SDL_ClearQueuedAudio(dev);
        printf("SDLAUDIO modo=%s hilo=%d bloqueos=%d cola_al_final=%u cola_tras_limpiar=%u\n", modo, hilo_activo(), bloqueos,
               resto, SDL_GetQueuedAudioSize(dev));
    } else if (!strcmp(modo, "cerrojo")) {
        while (SDL_GetTicks() - t0 < (unsigned)seg * 1000u) {
            SDL_LockAudioDevice(dev);
            a_cerrojo++;
            ocupado(3);   /* con el cerrojo tomado: la llamada de vuelta no puede entrar a mitad */
            b_cerrojo++;
            SDL_UnlockAudioDevice(dev);
            SDL_Delay(2);
        }
    } else if (!strcmp(modo, "pausa")) {
        SDL_Delay(1000);
        SDL_PauseAudioDevice(dev, 1);
        SDL_Delay(100);
        int antes = llamadas;
        SDL_Delay(500);
        pausas_ok = llamadas == antes;   /* callado: ni una llamada en 500 ms */
        SDL_PauseAudioDevice(dev, 0);
        SDL_Delay(500);
        pausas_ok = pausas_ok && llamadas > antes;   /* y vuelve */
    } else {
        unsigned ult_bloqueo = SDL_GetTicks();
        while (SDL_GetTicks() - t0 < (unsigned)seg * 1000u) {
            if (SDL_GetTicks() - ult_bloqueo >= 1000) { ocupado((unsigned)bloqueo); bloqueos++; ult_bloqueo = SDL_GetTicks(); }
            SDL_Delay(10);
        }
    }
    SDL_CloseAudioDevice(dev);
    if (!cola && !f32)
        printf("SDLAUDIO modo=%s hilo=%d llamadas=%d en_otro_hilo=%d huecoLlamadas=%u bloqueos=%d difs=%d pausa_ok=%d\n", modo,
               hilo_activo(), llamadas,
               tid_llamada != tid_main, gran_hueco_llamadas, bloqueos, difs, pausas_ok);
    printf("SDLAUDIO fin\n");
    SDL_Quit();
    return 0;
}
