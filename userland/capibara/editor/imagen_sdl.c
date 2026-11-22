/* SPDX-License-Identifier: MIT */
/* imagen_sdl.c - decodifica PNG con transparencia por la capa de imágenes de la SDL de SaguiOS (la misma que usa el motor). */
#include "raster.h"
#include <SDL.h>
#include <SDL_image.h>

uint32_t *ed_decodificar(const unsigned char *d, size_t n, int *w, int *h) {
	SDL_Surface *s = IMG_Load_Mem(d, (int)n);
	if (!s) return NULL;
	uint32_t *px = malloc((size_t)s->w * (size_t)s->h * 4);
	if (px) for (int y = 0; y < s->h; ++y) memcpy(px + (size_t)y * (size_t)s->w, (char *)s->pixels + (size_t)y * (size_t)s->pitch, (size_t)s->w * 4);
	*w = s->w; *h = s->h; SDL_FreeSurface(s);
	return px;
}
