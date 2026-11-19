// SPDX-License-Identifier: GPL-2.0-or-later
/* sgl_obj.h - cargador de modelos Wavefront OBJ (+ MTL básico) para SaguiGL (D404).
 *
 * Del OBJ: v, vt, vn, f (polígonos de 3 o más vértices, con v, v/vt, v//vn o
 * v/vt/vn e índices negativos), usemtl y mtllib; lo demás (o, g, s, l...) se
 * ignora. Del MTL: newmtl, Kd, Ks, Ns, d y map_Kd. Sin normales en el fichero,
 * se calculan por cara. El análisis trabaja sobre un texto en memoria (igual
 * en el anfitrión, para las pruebas, y en el SaguiOS); sgl_obj_cargar() lee
 * los ficheros y, en el SaguiOS, las texturas (BMP, PNG, JPEG, PPM... con la
 * libsagui). Si el OBJ no trae mtllib y hay una imagen <nombre>.png (o .bmp,
 * .jpg, .ppm) al lado, se usa como textura de todo el modelo. */
#ifndef SAGUI_SGL_OBJ_H
#define SAGUI_SGL_OBJ_H
#include "sgl.h"

#define SGL_OBJ_MAX_MAT 32

typedef struct {
    char   nome[32];
    float  kd[3], ks[3], ns, d;      /* difuso, especular, brillo (0..128), opacidad */
    char   mapa[96];                 /* map_Kd (relativo al .mtl) */
    GLuint tex;                      /* textura de GL (0 = ninguna) */
} sgl_obj_mat_t;

typedef struct {
    int    n_vert;                   /* vértices de triángulos (3 por triángulo) */
    float *pos, *nor, *uv;           /* n_vert * 3, * 3, * 2 */
    int    tem_uv;
    int    n_grupos;
    struct { int mat, ini, n; } grupo[SGL_OBJ_MAX_MAT * 2];   /* tramos con el mismo material */
    int    n_mats;
    sgl_obj_mat_t mat[SGL_OBJ_MAX_MAT];
    char   mtllib[96];
    float  min[3], max[3];           /* caja del modelo */
} sgl_obj_t;

/* 0 = bien; < 0 = error (-1 sin memoria, -2 sin triángulos, -3 índice malo). */
int  sgl_obj_ler(const char *texto, int n, sgl_obj_t *m);
/* Añade los materiales de un MTL (los que ya nombró el OBJ toman sus valores). */
int  sgl_obj_ler_mtl(const char *texto, int n, sgl_obj_t *m);
/* Lee el .obj, su .mtl y sus texturas (en el contexto actual). 0 = bien. */
int  sgl_obj_cargar(const char *caminho, sgl_obj_t *m);
/* Dibuja con el estado de ahora (matrices, luces, niebla): por cada material,
 * glMaterial con su Kd/Ks/Ns y su textura si la tiene. Centrado en el origen
 * y escalado para que su lado mayor mida `tamanho` (0 = tal cual). */
void sgl_obj_desenhar(const sgl_obj_t *m, float tamanho);
void sgl_obj_liberar(sgl_obj_t *m);

#endif
