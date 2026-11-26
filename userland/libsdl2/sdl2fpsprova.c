// SPDX-License-Identifier: GPL-2.0-or-later
/* sdl2fpsprova.c - /apps/bin/sdl2fps: prueba del limitador de FPS de la SDL 2 (D476, prueba h476).
 *
 *   sdl2fps [segundos] [fps_pista]
 *     segundos   duración de la medida (4 por defecto)
 *     fps_pista  si se da y no es «-», se pone con SDL_SetHint(SDL_SAGUI_FPS_MAX) antes de crear la ventana; sin él vale
 *                la variable de entorno SDL_SAGUI_FPS_MAX (o ninguna: ilimitado)
 * Presenta con SDL_RenderPresent y renderizador por software (SDL_GL_SwapWindow lleva el mismo limitador; se prueba con Brumacanto).
 * Cuenta los cuadros presentados y mide el uso de CPU del sistema con INFO_NUCLEOS (tiques y tiques ociosos). Escribe
 * «SDLFPS cuadros=N ms=T fps10=F cpu=P» (fps10: fps por diez; cpu: % de tiques no ociosos en esa medida). */
#include "sagui.h"
#include "SDL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void cpu_leer(unsigned *tq, unsigned *oc)
{
    info_nucleo_t n[8];
    int c = info_sistema(INFO_NUCLEOS, n, 8);
    *tq = *oc = 0;
    if (c > 8) c = 8;
    for (int i = 0; i < c; i++) { *tq += n[i].tiques; *oc += n[i].tiques_ociosos; }
}

int principal(int argc, char **argv)
{
    int seg = argc > 1 ? atoi(argv[1]) : 4;
    const char *pista = argc > 2 && strcmp(argv[2], "-") ? argv[2] : NULL;
    if (seg < 1) seg = 1;
    if (pista) SDL_SetHint(SDL_HINT_SAGUI_FPS_MAX, pista);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDLFPS error SDL_Init\n"); return 1; }
    SDL_Window *w = SDL_CreateWindow("fps", 100, 100, 160, 120, 0);
    SDL_Renderer *r = NULL;
    if (!w) { fprintf(stderr, "SDLFPS error ventana\n"); return 1; }
    r = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE);
    if (!r) { fprintf(stderr, "SDLFPS error renderizador\n"); return 1; }
    unsigned t0 = SDL_GetTicks(), tq0, oc0, tq1, oc1;
    int n = 0;
    SDL_Delay(50);   /* que el ritmo se asiente antes de medir */
    cpu_leer(&tq0, &oc0);
    t0 = SDL_GetTicks();
    while (SDL_GetTicks() - t0 < (unsigned)seg * 1000u) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {}
        SDL_SetRenderDrawColor(r, (Uint8)(n * 3), 40, 90, 255);
        SDL_RenderClear(r);
        SDL_RenderPresent(r);
        n++;
    }
    unsigned ms = SDL_GetTicks() - t0;
    cpu_leer(&tq1, &oc1);
    unsigned dt = tq1 - tq0, di = oc1 - oc0;
    int cpu = dt ? (int)((dt > di ? dt - di : 0) * 100u / dt) : -1;
    fprintf(stderr, "SDLFPS cuadros=%d ms=%u fps10=%u cpu=%d\n", n, ms, ms ? n * 10000u / ms : 0, cpu);
    return 0;
}
