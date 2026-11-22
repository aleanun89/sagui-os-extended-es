// SPDX-License-Identifier: GPL-2.0-or-later
/* tono.c - un tono de 440 Hz generado en la llamada de vuelta del audio.
 *
 * Ejemplo del anexo «La biblioteca SDL en SaguiOS». SDL_PauseAudio(0) arranca el hilo de audio
 * de la SDL del sistema (D393) y la llamada de vuelta corre en ese hilo, con el cerrojo del
 * dispositivo tomado: por eso el programa usa SDL_LockAudio para tocar lo que comparte con ella
 * (aquí, el contador de llamadas). Al final escribe cuántas veces la llamaron y si lo hizo otro hilo.
 * Compilar dentro de SaguiOS:  tcc tono.c -o tono -lSDL2
 */
#include <stdio.h>
#include <math.h>
#include <SDL2/SDL.h>

static double fase = 0.0;
static int llamadas = 0;
static SDL_threadID hilo_audio = 0;

static void llamada(void *usuario, Uint8 *flujo, int largo)
{
    Sint16 *muestras = (Sint16 *)flujo;
    int n = largo / 2, i;
    (void)usuario;
    if (llamadas == 0)
        hilo_audio = SDL_ThreadID();   /* el hilo que nos llama */
    llamadas++;
    for (i = 0; i < n; i++) {
        muestras[i] = (Sint16)(sin(fase) * 6000);
        fase += 2.0 * M_PI * 440.0 / 22050.0;
    }
}

int main(void)
{
    SDL_AudioSpec quiere, tiene;
    int fin = 0, n;
    SDL_threadID otro;
    Uint32 inicio;

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) != 0)
        return 1;
    SDL_memset(&quiere, 0, sizeof quiere);
    quiere.freq = 22050;
    quiere.format = AUDIO_S16SYS;
    quiere.channels = 1;
    quiere.samples = 512;
    quiere.callback = llamada;
    if (SDL_OpenAudio(&quiere, &tiene) != 0)
        return 1;

    SDL_LockAudio();              /* la llamada de vuelta corre en otro hilo: no a la vez */
    fase = 0.0;
    SDL_UnlockAudio();
    SDL_PauseAudio(0);            /* empieza a sonar */

    /* Dos segundos. El hilo principal solo atiende eventos: el audio no depende de el. */
    inicio = SDL_GetTicks();
    while (!fin && SDL_GetTicks() - inicio < 2000) {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT)
                fin = 1;
        SDL_Delay(10);
    }
    SDL_PauseAudio(1);
    SDL_LockAudio();
    n = llamadas;
    otro = hilo_audio;
    SDL_UnlockAudio();
    printf("tono: %d llamadas de vuelta; en otro hilo que el principal: %s\n", n, otro != SDL_ThreadID() ? "si" : "no");
    SDL_CloseAudio();
    SDL_Quit();
    return 0;
}
