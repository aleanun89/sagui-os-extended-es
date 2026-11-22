/* SPDX-License-Identifier: MIT */
/* dibujo_sdl.h - envío de los cuadriláteros de dibujo.c al renderizador de la SDL 2. */
#ifndef CAPI_DIBUJO_SDL_H
#define CAPI_DIBUJO_SDL_H
#include <SDL.h>
int dibsdl_iniciar(SDL_Renderer *r, int acelerado);
/* ordena por z, agrupa por textura y manda por lotes; la vista se escala (esc) y se desplaza (ox, oy) al lienzo */
void dibsdl_enviar(float esc, float ox, float oy);
void dibsdl_soltar(void);
extern long dibsdl_lotes, dibsdl_triangulos, dibsdl_copias;
extern int dibsdl_geometria;      /* 1: todo por SDL_RenderGeometry (GPU); 0: copias de textura (software) */
#endif
