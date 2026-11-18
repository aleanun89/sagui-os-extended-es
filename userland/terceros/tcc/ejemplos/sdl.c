/* SPDX-License-Identifier: GPL-3.0-only */
/* sdl.c - un programa con la SDL 2 de SaguiOS, compilado con TCC (D384).
 *   tcc sdl.c -o sdl -lSDL2 && ./sdl [segundos]
 * Un cuadrado rebota por la ventana; termina al cerrarla, con Esc o pasados los segundos. */
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

int main(int argc, char **argv)
{
    int segundos = argc > 1 ? atoi(argv[1]) : 0;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *v = SDL_CreateWindow("SDL con TCC", 0, 0, 320, 240, 0);
    SDL_Renderer *r = v ? SDL_CreateRenderer(v, -1, 0) : NULL;
    if (!r) {
        printf("ventana: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Log("sdl TCC: ventana abierta");
    int x = 10, y = 20, dx = 3, dy = 2, cuadros = 0, seguir = 1;
    Uint32 inicio = SDL_GetTicks();
    while (seguir) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
            if (ev.type == SDL_QUIT || (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE))
                seguir = 0;
        if (segundos && SDL_GetTicks() - inicio >= (Uint32)segundos * 1000)
            seguir = 0;
        x += dx; y += dy;
        if (x < 0 || x > 320 - 40) dx = -dx;
        if (y < 0 || y > 240 - 40) dy = -dy;
        SDL_SetRenderDrawColor(r, 20, 40, 90, 255);
        SDL_RenderClear(r);
        SDL_Rect c = { x, y, 40, 40 };
        SDL_SetRenderDrawColor(r, 250, 200, 40, 255);
        SDL_RenderFillRect(r, &c);
        SDL_RenderPresent(r);
        SDL_Delay(16);
        cuadros++;
    }
    SDL_Log("sdl TCC: %d cuadros", cuadros);
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(v);
    SDL_Quit();
    return 0;
}
