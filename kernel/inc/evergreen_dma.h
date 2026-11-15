// SPDX-License-Identifier: GPL-2.0-or-later
/* evergreen_dma.h - lo propio de las Radeon Evergreen (HD 5000) y Northern
 * Islands (HD 6000) para el 2D por el motor DMA (D286): la lista exacta de
 * modelos, el paquete de copia y la comprobación del controlador de pantalla.
 *
 * Es lógica pura, sin registros ni memoria de vídeo: la usa el driver
 * (kernel/dispositivos/acel_r600.c, que ya lleva las R6xx/R7xx, D223) y se
 * compila también en el PC para la prueba de unidad
 * (tests/evergreen_dma_unidad.c, con -DR600_ANFITRION).
 *
 * Qué cambia respecto de las R7xx:
 *   - la cabecera de los paquetes lleva la cuenta en 20 bits (no 16) y un
 *     subcódigo en los bits 27:20 (0 = copia lineal de palabras); la copia son
 *     5 palabras como en las R7xx; escribir, valla y NOP son iguales (Linux usa
 *     las mismas r600_dma_ring_test y la misma valla en las dos);
 *   - los controladores de pantalla son otros (hasta 6, registros
 *     EVERGREEN_GRPH_* / EVERGREEN_CRTC_CONTROL con un desplazamiento por
 *     controlador) y la superficie tiene su registro de bits altos;
 *   - CONFIG_MEMSIZE va en MB (no en bytes);
 *   - el HDP se vacía con HDP_MEM_COHERENCY_FLUSH_CNTL (no con HDP_DEBUG1);
 *   - Cayman (HD 6900) tiene dos motores DMA (se usa solo el primero), no tiene
 *     DMA_MODE y su DMA_IB_CNTL lleva CMD_VMID_FORCE.
 * El anillo se enciende igual (r600_dma_resume vale para Evergreen, Barts,
 * Turks y Caicos; cayman_dma_resume para Cayman). El motor DMA no lleva
 * microcódigo en ninguna.
 *
 * Referencia de los números: evergreend.h, evergreen_reg.h, nid.h, r600_dma.c,
 * evergreen_dma.c y ni_dma.c del driver radeon de Linux 6.6 (cabeceras MIT/X11
 * de AMD) y drm_pciids.h (SPDX MIT); el código es propio (docs/terceros.md). */
#ifndef SAGUI_EVERGREEN_DMA_H
#define SAGUI_EVERGREEN_DMA_H

#ifdef R600_ANFITRION
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#else
#include "tipos.h"
#endif

/* Generaciones para la tabla de la capa común (6 y 7 son R6xx y R7xx). */
#define EVG_GEN_EVERGREEN   8      /* Cedar, Redwood, Juniper, Cypress, Hemlock, Barts, Turks, Caicos */
#define EVG_GEN_CAYMAN      9      /* Cayman (HD 6900): dos motores DMA */

typedef struct {
    uint16_t    id;        /* modelo PCI (fabricante 0x1002) */
    uint8_t     gen;       /* EVG_GEN_* */
    uint8_t     crtcs;     /* controladores de pantalla del chip (4 o 6) */
    const char *nome;
} evg_modelo_t;

const evg_modelo_t *evg_modelo(uint16_t id);      /* NULL si no está en la lista exacta */
int                 evg_n_modelos(void);
const evg_modelo_t *evg_modelo_n(int i);

/* Paquete de copia lineal: cabecera (orden 3, subcódigo 0, n palabras), destino,
 * origen, bits 39:32 del destino, bits 39:32 del origen (aquí siempre 0). */
#define EVG_DMA_COPIAR      0x3u
#define EVG_DMA_CABECA(op, sub, n) ((((uint32_t)(op) & 0xFu) << 28) | (((uint32_t)(sub) & 0xFFu) << 20) | ((uint32_t)(n) & 0xFFFFFu))
#define EVG_DMA_MAX_DW      0xFFFFFu
#define EVG_PKT_COPIAR_DW   5
int evg_pkt_copiar(uint32_t *p, uint32_t dst, uint32_t src, uint32_t ndw);   /* 5, o 0 si ndw no cabe */

/* Desplazamiento de los registros del controlador de pantalla n (0..5). */
#define EVG_MAX_CRTC        6
uint32_t evg_crtc_desloc(int n);

/* Lo leído de un controlador de pantalla. */
typedef struct {
    uint32_t crtc_control;   /* bit 0: encendido */
    uint32_t grph_enable;    /* bit 0 */
    uint32_t grph_control;   /* 1:0 profundidad (2 = 32 bits), 10:8 formato (0 = ARGB8888), 22:20 mosaico (0-1 lineal) */
    uint32_t superficie;     /* dirección en el MC (bits 31:8) */
    uint32_t superficie_alta;/* bits 39:32 */
    uint32_t pitch;          /* en píxeles */
    uint32_t off_x, off_y;
} evg_crtc_t;

/* ¿Muestra ese controlador, tal cual, el framebuffer que está en mc_tela con
 * ese pitch (en píxeles)? Encendido, 32 bits ARGB8888, lineal, sin
 * desplazamiento y por debajo de 4 GB. */
bool evg_crtc_mostra(const evg_crtc_t *c, uint32_t mc_tela, uint32_t pitch_px);

/* CONFIG_MEMSIZE (en MB) a bytes; 0 si no dice nada creíble (0, todo unos o 4 GB o más). */
uint32_t evg_memsize_bytes(uint32_t reg);

#endif
