// SPDX-License-Identifier: GPL-2.0-or-later
/* ventana_mueve.c - una ventana y un rectángulo que se mueve con las flechas.
 *
 * Ejemplo del anexo «La biblioteca SDL en SaguiOS». Compilar dentro de SaguiOS:
 *   tcc ventana_mueve.c -o ventana_mueve -lSDL2
 * Flechas: mover. Escape o cerrar la ventana: salir.
 * El título va sin acento: TCC deja las cadenas en UTF-8 y la ventana del sistema las dibuja como Latin-1.
 */
#include <SDL2/SDL.h>

int main(void)
{
    int x = 150, y = 90, fin = 0;
    SDL_Window *ventana;
    SDL_Renderer *dibujo;

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 1;
    ventana = SDL_CreateWindow("Rectangulo", 0, 0, 320, 200, 0);
    dibujo = SDL_CreateRenderer(ventana, -1, 0);

    while (!fin) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                fin = 1;
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                case SDLK_LEFT:   x -= 4; break;
                case SDLK_RIGHT:  x += 4; break;
                case SDLK_UP:     y -= 4; break;
                case SDLK_DOWN:   y += 4; break;
                case SDLK_ESCAPE: fin = 1; break;
                }
            }
        }
        /* Fondo negro y un rectángulo amarillo en la posición actual. */
        SDL_SetRenderDrawColor(dibujo, 0, 0, 0, 255);
        SDL_RenderClear(dibujo);
        SDL_SetRenderDrawColor(dibujo, 255, 200, 0, 255);
        {
            SDL_Rect r = { x, y, 40, 30 };
            SDL_RenderFillRect(dibujo, &r);
        }
        SDL_RenderPresent(dibujo);
        SDL_Delay(16);   /* unos 60 cuadros por segundo */
    }
    SDL_DestroyRenderer(dibujo);
    SDL_DestroyWindow(ventana);
    SDL_Quit();
    return 0;
}
