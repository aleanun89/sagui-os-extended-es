// SPDX-License-Identifier: GPL-2.0-or-later
/* paisaje.h - escena realista y de carga de SaguiGL (D404): terreno con relieve,
 * hierba, roca, arena y agua, árboles, rocas, casas, nubes, niebla, dos luces y
 * un avión que vuela; todo generado por código (texturas incluidas). La cámara
 * se mueve con el número de cuadro: con el mismo cuadro sale el mismo píxel
 * (la comparten sgldemo y la prueba del anfitrión, tests/h405_saguigl.py). */
#ifndef SAGUI_PAISAJE_H
#define SAGUI_PAISAJE_H
#include "sgl.h"
#include "sgl_obj.h"

#define PAIS_MAX_MB 112      /* --texturas: como mucho (4 texturas de 256x256 por MB) */

typedef struct {
    int mip;                 /* 1: mipmaps y trilineal; 0: solo bilineal (lo que hace hoy la GPU) */
    int texturas_mb;         /* MB de texturas de carga distintas en cada cuadro (0 = ninguna) */
    int detalle;             /* celdas del terreno por lado (16..128; 0 = 64) */
    const sgl_obj_t *aviao;  /* modelo del avión (NULL = uno hecho por código) */
    float giro_aviao;        /* grados sobre Y para orientar el modelo (su morro hacia +x) */
    int camara;              /* 0: vuelta al valle; 1: detrás del avión (escena «avion») */
} pais_opcoes_t;

/* Crea las texturas en el contexto actual y la geometría. 0 = bien. */
int  paisaje_iniciar(const pais_opcoes_t *o);
/* Dibuja el cuadro `i` en un viewport de l x a. */
void paisaje_cuadro(int i, int l, int a);
/* Suelta las texturas (del contexto actual). */
void paisaje_fim(void);
/* Cuántas texturas y KB (sin mipmaps) de carga hay. */
int  paisaje_texturas_carga(void);

#endif
