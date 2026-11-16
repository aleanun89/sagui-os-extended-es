// SPDX-License-Identifier: GPL-2.0-or-later
/* gcn_dma.h - lo propio de las Radeon GCN para el 2D por la GPU (D360): la
 * lista exacta de modelos (GCN 1.0 a 5), los paquetes del motor DMA de las
 * Southern Islands (GCN 1.0), la comprobación del controlador de pantalla
 * (DCE 6) y el relleno de rectángulos con DMA_PACKET_CONSTANT_FILL.
 *
 * Es lógica pura, sin registros ni memoria de vídeo: la usa el driver
 * (kernel/dispositivos/acel_gcn.c) y se compila también en el PC para la
 * prueba de unidad (tests/gcn_dma_unidad.c, con -DR600_ANFITRION).
 *
 * Fichero aparte (y no dentro de evergreen_dma.c) a propósito: la rama de las
 * Evergreen/NI (v8.5.2-amd-v83) toca evergreen_dma.c y acel_r600.c; esto solo
 * LLAMA a r600_dma_pacotes.c (el reparto de rectángulos en copias lineales) y
 * no cambia nada de lo de R6xx/R7xx/Evergreen.
 *
 * Qué cambia en el DMA de las Southern Islands respecto de Cayman (sid.h,
 * si_dma.c de radeon y de amdgpu, Linux 6.6):
 *   - la cabecera es DMA_PACKET(cmd, b, t, s, n): orden en 31:28, «b» en 26,
 *     «t» en 23, «s» en 22 y la cuenta en 19:0;
 *   - la copia lineal va con b = 1 y la cuenta en BYTES (no en palabras):
 *     cabecera, destino, origen, bits 39:32 del destino, bits 39:32 del origen
 *     (5 palabras; amdgpu parte en trozos de 0xFFFF8 bytes);
 *   - existe DMA_PACKET_CONSTANT_FILL (orden 0xD), que amdgpu usa en SI para
 *     borrar memoria: cabecera con la cuenta en palabras, destino, valor,
 *     bits altos del destino << 16 (4 palabras; trozos de 0xFFFF8 bytes);
 *   - escribir, valla y NOP son los de siempre (r600_dma_ring_test y
 *     evergreen_dma_fence_ring_emit valen para SI en radeon);
 *   - el anillo se enciende con cayman_dma_resume (radeon) / si_dma_start
 *     (amdgpu): sin DMA_MODE y con CMD_VMID_FORCE en DMA_IB_CNTL;
 *   - el HDP se vacía con HDP_MEM_COHERENCY_FLUSH_CNTL y su caché de lectura se
 *     invalida con HDP_DEBUG0 (si_flush_hdp / si_invalidate_hdp de amdgpu);
 *   - la pantalla (DCE 6) tiene los registros de Evergreen (EVERGREEN_GRPH_*,
 *     los mismos desplazamientos por controlador: si.c usa crtc_offsets[] de
 *     Evergreen); Oland tiene 2 controladores, Tahiti/Pitcairn/Cape Verde 6 y
 *     Hainan ninguno (no está en la lista: no puede tener la pantalla).
 *
 * GCN 1.1 en adelante (CIK, VI/Polaris, Vega) llevan el SDMA, con microcódigo:
 * están en la lista solo para decir en el log qué tarjeta es y por qué copia
 * la CPU (gcn_por_que_nao); el driver no toca ningún registro suyo (D360).
 *
 * Referencia de los números: sid.h, si_dma.c, ni_dma.c, evergreen_reg.h,
 * radeon_asic.c de radeon y si_dma.c, sid.h, si.c de amdgpu (Linux 6.6, todos
 * con la licencia MIT/X11 de AMD), drm_pciids.h y amdgpu_drv.c (las listas de
 * modelos); el código es propio (docs/terceros.md). */
#ifndef SAGUI_GCN_DMA_H
#define SAGUI_GCN_DMA_H

#ifdef R600_ANFITRION
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#else
#include "tipos.h"
#endif
#include "r600_dma.h"

/* Generaciones para la tabla de la capa común (8 y 9 son Evergreen y Cayman, D286). */
#define GCN_GEN_SI     10    /* GCN 1.0: Tahiti, Pitcairn, Cape Verde, Oland (DMA sin microcódigo) */
#define GCN_GEN_CIK    11    /* GCN 1.1: Bonaire, Hawaii (SDMA con microcódigo cargado por el driver) */
#define GCN_GEN_VI     12    /* GCN 1.2-4: Tonga, Fiji, Polaris 10/11/12, Vega M (microcódigo por el SMU) */
#define GCN_GEN_GFX9   13    /* GCN 5: Vega 10/12/20 (firmware por el PSP) */

typedef struct {
    uint16_t    id;        /* modelo PCI (fabricante 0x1002) */
    uint8_t     gen;       /* GCN_GEN_* */
    uint8_t     crtcs;     /* controladores de pantalla del chip (SI: 2 o 6; el resto, 0: no se leen) */
    const char *nome;      /* la familia primero: "Pitcairn", "Polaris 10"... */
} gcn_modelo_t;

const gcn_modelo_t *gcn_modelo(uint16_t id);      /* NULL si no está en la lista exacta */
int                 gcn_n_modelos(void);
const gcn_modelo_t *gcn_modelo_n(int i);
/* Por qué no hay 2D por la GPU en esa generación (texto corto para el motivo de
 * infopc; NULL en SI). */
const char         *gcn_por_que_nao(int gen);
const char         *gcn_nome_geracao(int gen);   /* "GCN 1.0 (Southern Islands)"... */

/* Paquetes del DMA de SI. Las direcciones son del controlador de memoria de la
 * GPU, de 40 bits: los 32 bajos y el byte alto (bits 39:32) por separado; el
 * driver comprueba que todo lo que toca tiene el mismo byte alto. */
#define SI_DMA_ESCREVER     0x2u
#define SI_DMA_COPIAR       0x3u
#define SI_DMA_VALLA        0x6u
#define SI_DMA_ENCHER       0xDu      /* DMA_PACKET_CONSTANT_FILL */
#define SI_DMA_NADA         0xFu
#define SI_DMA_CABECA(op, b, n) ((((uint32_t)(op) & 0xFu) << 28) | (((uint32_t)(b) & 1u) << 26) | ((uint32_t)(n) & 0xFFFFFu))
#define SI_DMA_NOP          SI_DMA_CABECA(SI_DMA_NADA, 0, 0)
#define SI_DMA_MAX_BYTES    0xFFFF8u  /* por paquete, como amdgpu (copy_max_bytes / fill_max_bytes) */
#define SI_PKT_COPIAR_DW    5
#define SI_PKT_ENCHER_DW    4
int si_pkt_copiar(uint32_t *p, uint32_t dst, uint32_t src, uint32_t bytes, uint8_t alto);    /* 5, o 0 si no vale */
int si_pkt_encher(uint32_t *p, uint32_t dst, uint32_t valor, uint32_t bytes, uint8_t alto);  /* 4, o 0 si no vale */
int si_pkt_escrever(uint32_t *p, uint32_t dst, uint32_t valor, uint8_t alto);                /* 4 */
int si_pkt_valla(uint32_t *p, uint32_t dst, uint32_t valor, uint8_t alto);                   /* 4 */

/* Controladores de pantalla (DCE 6 = los registros de Evergreen). */
#define GCN_MAX_CRTC        6
uint32_t gcn_crtc_desloc(int n);

typedef struct {
    uint32_t crtc_control;    /* bit 0: encendido */
    uint32_t grph_enable;     /* bit 0 */
    uint32_t grph_control;    /* 1:0 profundidad (2 = 32 bits), 10:8 formato (0 = ARGB8888), 22:20 mosaico (0-1 lineal) */
    uint32_t superficie;      /* bits 31:8 de la dirección en el MC */
    uint32_t superficie_alta; /* bits 39:32 */
    uint32_t pitch;           /* en píxeles */
    uint32_t off_x, off_y;
} gcn_crtc_t;

/* ¿Muestra ese controlador, tal cual, la pantalla que está en mc_tela (40 bits)
 * con ese pitch (en píxeles)? Encendido, 32 bits ARGB8888, lineal, sin
 * desplazamiento, superficie alineada a 256 bytes e igual a mc_tela. */
bool gcn_crtc_mostra(const gcn_crtc_t *c, uint64_t mc_tela, uint32_t pitch_px);

/* CONFIG_MEMSIZE (en MB, como en Evergreen) a bytes; 0 si no dice nada creíble
 * (0, todo unos o más de 64 GB). */
uint64_t gcn_memsize_bytes(uint32_t reg);

/* Relleno de un rectángulo de la pantalla con CONSTANT_FILL: un paquete por
 * fila, o uno por bloque de filas si el rectángulo ocupa todo el pitch (las
 * filas son contiguas). Las filas no se solapan: no hacen falta barreras. */
typedef struct {
    void *ctx;
    bool (*encher)(void *ctx, uint32_t dst, uint32_t bytes, uint32_t cor);   /* bytes <= SI_DMA_MAX_BYTES, múltiplo de 4 */
} gcn_saida_encher_t;
bool gcn_preencher_ret(const r600_geo_t *g, const gcn_saida_encher_t *s, int x, int y, int l, int a, uint32_t cor);

#endif
