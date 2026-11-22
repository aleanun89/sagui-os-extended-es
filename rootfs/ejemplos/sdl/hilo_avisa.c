// SPDX-License-Identifier: GPL-2.0-or-later
/* hilo_avisa.c - un hilo de trabajo avisa al principal con una condición.
 *
 * Ejemplo del anexo «La biblioteca SDL en SaguiOS». El hilo de trabajo no llama a vídeo,
 * eventos ni audio (D393): solo calcula y avisa.
 * Compilar dentro de SaguiOS:  tcc hilo_avisa.c -o hilo_avisa -lSDL2
 */
#include <stdio.h>
#include <SDL2/SDL.h>

typedef struct {
    SDL_mutex *cerrojo;
    SDL_cond *aviso;
    int listo;
    long long resultado;       /* long long: en 32 bits un long no cabe en la suma */
} Estado;

static int trabajo(void *datos)
{
    Estado *e = datos;
    long long suma = 0;
    long long i;

    for (i = 1; i <= 1000000; i++)
        suma += i;
    SDL_LockMutex(e->cerrojo);
    e->resultado = suma;
    e->listo = 1;
    SDL_CondSignal(e->aviso);      /* despierta al principal */
    SDL_UnlockMutex(e->cerrojo);
    return 0;
}

int main(void)
{
    Estado e;
    SDL_Thread *hilo;

    e.cerrojo = SDL_CreateMutex();
    e.aviso = SDL_CreateCond();
    e.listo = 0;
    e.resultado = 0;
    hilo = SDL_CreateThread(trabajo, "trabajo", &e);

    SDL_LockMutex(e.cerrojo);
    while (!e.listo)               /* siempre en bucle: la espera puede despertar sin aviso */
        SDL_CondWait(e.aviso, e.cerrojo);
    SDL_UnlockMutex(e.cerrojo);

    printf("suma de 1 a 1000000 = %lld\n", e.resultado);
    SDL_WaitThread(hilo, NULL);
    return 0;
}
