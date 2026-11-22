// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d_v3.h - gpu3d v3 (D457): lo que comparten la capa, los drivers y gpu3d_v3.c (la transformación por la
 * CPU). Solo necesita los tipos de abi.h y uint8_t/uint32_t ya definidos: lo incluye gpu3d.h en el kernel y,
 * en el PC, las pruebas que enlazan gpu3d_v3.c sin las cabeceras del kernel (tests/sgl/es11_gpu_falsa.c). */
#ifndef SAGUI_GPU3D_V3_H
#define SAGUI_GPU3D_V3_H

/* D457: un dibujo v3 para el driver. */
typedef struct {
    const gpu3d_estado2_t *e;      /* la parte de los píxeles: la de v2 */
    const gpu3d_transf_t  *t;
    const gpu3d_luzes_t   *luz;    /* NULL sin G3D3_LUZ */
    gpu3d_desenho3_t       d;
    uint32_t               max_vert;   /* el mayor número de vértice que el dibujo puede leer (con índices, el mayor del búfer) */
    uint32_t               bytes_indices;   /* lo subido al búfer de índices (0 sin índices) */
} g3d_desenho3_t;

/* ------------------------------------------------------------ gpu3d v3 por la CPU (gpu3d_v3.c, D457)
 * La transformación de v3 hecha con enteros (coma flotante IEEE de 32 bits por software, redondeo al más
 * próximo): lee los búferes, transforma, calcula la niebla, recorta, proyecta, descarta caras y entrega
 * triángulos v2. La usan el driver simulado (su v3 es esto + su v2) y la autoprueba de la capa, que dibuja
 * la misma escena por el desenhar3 del driver y por aquí + desenhar2, y compara. No toca hardware ni usa
 * nada del kernel: se compila también en el PC (tests/g3d_v3_unidad.c). */
typedef struct { const uint8_t *p; uint32_t bytes; } g3d_v3_mem_t;
typedef struct {
    g3d_v3_mem_t (*mem)(void *ctx, int vaga);       /* el contenido de un búfer (p NULL: no está) */
    int   (*saida)(void *ctx, const gpu3d_estado2_t *e, const gpu3d_vert2_t *v, int n);   /* n múltiplo de 3 */
    void  *ctx;
    gpu3d_vert2_t *lote;                            /* sitio de trabajo */
    int    max;                                     /* vértices que caben en lote (múltiplo de 3, >= 24) */
} g3d_v3_cpu_t;
/* G3D_OK, G3D_NAO (pide algo que esta versión no hace: luces, niebla exponencial) o lo que devuelva saida. */
int g3d_v3_cpu(const g3d_desenho3_t *d, const g3d_v3_cpu_t *c);
/* ¿Sabe g3d_v3_cpu hacer esa transformación? (lo que anuncia quien la usa) */
#define G3D_V3_CPU_BITS3 (G3D_C3_V3 | G3D_C3_NEBLINA | G3D_C3_TEX_MAT | G3D_C3_INDICES | G3D_C3_TIRAS | G3D_C3_COR_FLOAT)
/* La coma flotante por software (bits IEEE-754 de 32 bits; los desnormalizados valen 0). */
uint32_t g3d_fl_mul(uint32_t a, uint32_t b);
uint32_t g3d_fl_soma(uint32_t a, uint32_t b);
uint32_t g3d_fl_div(uint32_t a, uint32_t b);
uint32_t g3d_fl_raiz(uint32_t a);
static inline uint32_t g3d_fl_sub(uint32_t a, uint32_t b) { return g3d_fl_soma(a, b ^ 0x80000000u); }

#endif
