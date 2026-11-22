/* SPDX-License-Identifier: MIT */
/* raster.h - dibuja en un búfer de píxeles los cuadriláteros que recoge el motor (D460): así el lienzo del editor usa el
 * dibujo del propio motor (nodos, cámara, casillas) sin la SDL. */
#ifndef CAPI_RASTER_H
#define CAPI_RASTER_H
#include "capi.h"
/* Dibuja g_quads (ya con la cámara aplicada) ordenados por z sobre buf (w x h, 0x00RRGGBB). */
void rast_dibujar(uint32_t *buf, int w, int h);
/* Decodificador de imágenes con transparencia (la capa de imágenes de la SDL de SaguiOS). */
uint32_t *ed_decodificar(const unsigned char *d, size_t n, int *w, int *h);
#endif
