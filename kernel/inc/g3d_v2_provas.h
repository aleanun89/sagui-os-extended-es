// SPDX-License-Identifier: GPL-2.0-or-later
/* g3d_v2_provas.h - pruebas de las capacidades gpu3d v2 de un driver (D426).
 *
 * Un driver v2 no debe anunciar lo que no ha visto funcionar (docs/gpu3d.md, «gpu3d v2», regla 1).
 * Esto dibuja, con las propias funciones del driver, un caso pequeño de cada capacidad en un destino
 * de 64x32, lee los píxeles y devuelve solo los bits de las que salen bien. Cada fallo deja en el log
 * «[GPU3D] <driver> v2: prueba N (<nombre>) MAL: ...» con el píxel esperado y el obtenido.
 *
 * Se llama desde capacidades() del driver (tras iniciar y la autoprueba v1). Usa los huecos de textura
 * 28 a 31 (los suelta al terminar) y deja el destino de la prueba: la capa crea el suyo en cada ABRIR. */
#ifndef SAGUI_G3D_V2_PROVAS_H
#define SAGUI_G3D_V2_PROVAS_H
#include "gpu3d.h"

typedef struct {
    int  (*alvo)(int larg, int alt, int zbits);
    int  (*limpar)(uint32_t mascara, uint32_t cor);
    int  (*textura)(int vaga, int larg, int alt, const uint32_t *argb);
    int  (*textura2)(const gpu3d_textura2_t *t);
    int  (*desenhar2)(const gpu3d_estado2_t *e, const gpu3d_vert2_t *v, int n, g3d_vala_t *vala);
    int  (*limpar2)(const gpu3d_limpar2_t *l);
    int  (*ler)(uint32_t *dest, int passo_px);
    int  (*destino)(int vaga, int zbits, int estencil);   /* puede ser NULL */
} g3d_v2_ops_t;

/* candidatos: los G3D_CAP_* que el driver cree tener; *unidades y *formatos: los que cree tener, y a la
 * vuelta los que pasaron. Devuelve los bits que pasaron (0 si ni el dibujo v2 más sencillo sale). */
uint32_t g3d_v2_provar(const g3d_v2_ops_t *ops, const char *nome, uint32_t candidatos, uint32_t *unidades, uint32_t *formatos);

#endif
