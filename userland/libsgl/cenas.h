// SPDX-License-Identifier: GPL-2.0-or-later
/* cenas.h - escenas fijas de SaguiGL, compartidas por sgldemo y por la
 * prueba de píxeles del anfitrión (tests/h77_sgl.py). */
#ifndef SAGUI_SGL_CENAS_H
#define SAGUI_SGL_CENAS_H
#include "sgl.h"

#define CENA_L 160
#define CENA_A 120

/* Cubo de colores girado `graus` sobre el eje (1,1,0) (con Z y cull). */
void cena_cubo(float graus);
/* Igual, para un viewport de l x a (el demo usa 320x240; la proporción es 4:3). */
void cena_cubo_tam(float graus, int l, int a);
/* Triángulo Gouraud (con un segundo plano y), sin Z. */
void cena_triangulo(void);
/* Dos triángulos que se cruzan en Z + líneas y abanico. */
void cena_z_cruzado(void);
/* Nombre -> escena; devuelve 0 si no existe. */
int cena_por_nome(const char *nome);

#endif
