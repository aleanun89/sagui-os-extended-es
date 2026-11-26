// SPDX-License-Identifier: GPL-3.0-only
/* sgl_tgsi.h - traductor de la RI de SaguiGL 2.0 a TGSI en texto (D480): lo que recibe virglrenderer. */
#ifndef SAGUI_SGL_TGSI_H
#define SAGUI_SGL_TGSI_H
#include "sgl_ri.h"

/* Traduce un sombreador (p->tipo 0 vértices, 1 fragmentos) a TGSI en texto.
 *  gen[r]: índice GENERIC de la salida r (vértices) o de la entrada r (fragmentos); -1 = sin emparejar. Lo da el
 *          enlace: a la entrada k-ésima de fragmentos le corresponde la salida mapa_vary de vértices, y las dos
 *          llevan GENERIC[k]. NULL: orden de aparición.
 * Devuelve la longitud del texto (sin contar el 0 final) o -1 con el motivo en «erro» (por ejemplo
 * gl_PointCoord, que solo existe al dibujar puntos y los dibuja la CPU). Sin memoria dinámica. */
int sgl_tgsi_gerar(const ri_prog_t *p, const int *gen, char *saida, int max, char *erro, int max_erro);

#endif
