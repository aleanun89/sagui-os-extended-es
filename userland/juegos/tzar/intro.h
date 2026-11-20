/* SPDX-License-Identifier: GPL-3.0-or-later */
/* intro.h - vídeo de introducción de Tzar (intro.MPG) con el decodificador MPEG-1 de la libsagui (D417, fase 7). */
#ifndef TZAR_INTRO_H
#define TZAR_INTRO_H

#include "som.h"

typedef struct TzIntro TzIntro;
/* Abre <raiz>/intro.MPG (sin distinguir mayúsculas). NULL si no está o esta compilación no trae el decodificador (se salta la
 * introducción, sin error); el motivo queda en tz_erro(). */
TzIntro *tz_intro_abrir(const char *raiz);
void tz_intro_tamanho(const TzIntro *v, int *w, int *h, int *fps);
/* Decodifica el cuadro siguiente a 0xAARRGGBB (opaco) en px (w*h) y entrega su audio al sonido (si no es NULL). 0 = hay cuadro, 1 = fin
 * del vídeo, -1 = cuadro dañado (se puede seguir llamando). */
int tz_intro_quadro(TzIntro *v, uint32_t *px, TzSom *som);
int tz_intro_cuadros(const TzIntro *v);
void tz_intro_fechar(TzIntro *v);

#endif
