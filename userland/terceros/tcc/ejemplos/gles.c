/* SPDX-License-Identifier: GPL-3.0-only */
/* gles.c - OpenGL ES 1.1 en una ventana de la SDL 2, compilado con TCC (D425).
 *   tcc gles.c -o gles -lSDL2 -lGLESv1_CM -lEGL && ./gles [segundos]
 * Un cubo de colores girando, con Z; termina al cerrar la ventana, con Esc o pasados los segundos.
 * Solo usa las cabeceras estándar <GLES/gl.h> y <SDL2/SDL.h> (SDL_GL_CreateContext va por EGL). */
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <GLES/gl.h>

static const GLfloat v[] = { -1,-1, 1,  1,-1, 1,  1, 1, 1, -1, 1, 1,   1,-1,-1, -1,-1,-1, -1, 1,-1,  1, 1,-1,
                             -1,-1,-1, -1,-1, 1, -1, 1, 1, -1, 1,-1,   1,-1, 1,  1,-1,-1,  1, 1,-1,  1, 1, 1,
                             -1, 1, 1,  1, 1, 1,  1, 1,-1, -1, 1,-1,  -1,-1,-1,  1,-1,-1,  1,-1, 1, -1,-1, 1 };

int main(int argc, char **argv)
{
    int segundos = argc > 1 ? atoi(argv[1]) : 0;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) { printf("SDL_Init: %s\n", SDL_GetError()); return 1; }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_Window *w = SDL_CreateWindow("OpenGL ES con TCC", 0, 0, 320, 240, SDL_WINDOW_OPENGL);
    SDL_GLContext c = w ? SDL_GL_CreateContext(w) : NULL;
    if (!c) { printf("contexto: %s\n", SDL_GetError()); return 1; }
    printf("gles: %s, %s\n", (const char *)glGetString(GL_VERSION), (const char *)glGetString(GL_RENDERER));
    int l, a;
    SDL_GL_GetDrawableSize(w, &l, &a);
    glViewport(0, 0, l, a);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustumf(-0.8f, 0.8f, -0.6f, 0.6f, 1.0f, 20.0f);
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, v);
    Uint32 inicio = SDL_GetTicks();
    int seguir = 1, cuadros = 0;
    while (seguir) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
            if (ev.type == SDL_QUIT || (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE)) seguir = 0;
        if (segundos && SDL_GetTicks() - inicio >= (Uint32)segundos * 1000) seguir = 0;
        glClearColor(0.1f, 0.1f, 0.2f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        glTranslatef(0, 0, -4);
        glRotatef((float)(SDL_GetTicks() - inicio) * 0.05f, 1, 1, 0);
        for (int f = 0; f < 6; f++) {
            glColor4f(f & 1 ? 1.0f : 0.3f, f & 2 ? 1.0f : 0.3f, f & 4 ? 1.0f : 0.3f, 1);
            glDrawArrays(GL_TRIANGLE_FAN, f * 4, 4);
        }
        SDL_GL_SwapWindow(w);
        cuadros++;
    }
    printf("gles: %d cuadros\n", cuadros);
    SDL_GL_DeleteContext(c);
    SDL_DestroyWindow(w);
    SDL_Quit();
    return 0;
}
