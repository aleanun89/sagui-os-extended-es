// SPDX-License-Identifier: GPL-2.0-or-later
/* radeon2d_ordens.h - las escrituras de registros de una orden del motor 2D de las Radeon R100-R500
 * (D195, D387, D391), sin efectos: las mismas, en el mismo orden, van por MMIO (acel_radeon.c) o, con
 * el CP en marcha, por el anillo como PACKET0 (gpu3d_r500.c).
 *
 * Por qué dos caminos (Amilo, X1400, 2026-11-19, D387): con el 3D encendido (CP en marcha) y el volcado
 * por MMIO, tras miles de cuadros el motor queda ocupado para siempre (RBBM_STATUS 90010140: GUI_ACTIVE,
 * RBBM_HIBUSY, CP_CMDSTRM_BUSY y HIRQ_ON_RBB, con todos los bloques del 2D y del 3D parados). El driver
 * radeon de Linux (r100_copy_blit, r300_fence_ring_emit) y xf86-video-ati (radeon_exa_funcs.c, compilado
 * dos veces: ACCEL_MMIO sin CP y ACCEL_CP con él) nunca mezclan las dos cosas: con el CP en marcha todo
 * el 2D va por el anillo, con los mismos registros (OUT_ACCEL_REG = PACKET0) y, tras cada orden, el
 * vaciado de la caché de destino del 2D y WAIT_UNTIL (RADEONFlush2D / RADEONDone2D).
 *
 * Solo lógica (sin MMIO): se prueba en el PC (tests/r500_3d_unidad.c, h287 unidad). */
#ifndef RADEON2D_ORDENS_H
#define RADEON2D_ORDENS_H

#include "tipos.h"

#define RA2D_SRC_PITCH_OFFSET     0x1428
#define RA2D_DST_PITCH_OFFSET     0x142C
#define RA2D_SRC_Y_X              0x1434
#define RA2D_DST_Y_X              0x1438
#define RA2D_DST_HEIGHT_WIDTH     0x143C
#define RA2D_DP_GUI_MASTER_CNTL   0x146C
#define RA2D_DP_BRUSH_FRGD_CLR    0x147C
#define RA2D_DP_CNTL              0x16C0
#define RA2D_DP_WRITE_MASK        0x16CC
#define RA2D_DSTCACHE_CTLSTAT     0x1714      /* R300 en adelante (la caché de destino del 2D) */
#define   RA2D_DC_FLUSH_ALL       0xFu
#define RA2D_WAIT_UNTIL           0x1720
#define   RA2D_WAIT_DMA_GUI_IDLE  (1u << 9)
#define   RA2D_WAIT_2D_IDLECLEAN  (1u << 16)

#define RA2D_GMC_SRC_PITCH_OFFSET (1u << 0)
#define RA2D_GMC_DST_PITCH_OFFSET (1u << 1)
#define RA2D_GMC_BRUSH_SOLIDO     (13u << 4)
#define RA2D_GMC_BRUSH_NADA       (15u << 4)
#define RA2D_GMC_DST_32BPP        (6u << 8)
#define RA2D_GMC_SRC_COR          (3u << 12)
#define RA2D_ROP3_S               0x00CC0000u
#define RA2D_ROP3_P               0x00F00000u
#define RA2D_ROP3_SP              0x00C00000u   /* D391: origen AND patrón (patrón 0x00FFFFFF: quita el alfa) */
#define RA2D_GMC_SRC_MEMORIA      (2u << 24)
#define RA2D_GMC_SEM_COMPARAR     (1u << 28)
#define RA2D_DIR_X_ESQ            0x1u          /* DP_CNTL: de izquierda a derecha */
#define RA2D_DIR_Y_CIMA           0x2u          /* DP_CNTL: de arriba abajo */

#define RA2D_MAX_ESCRITAS 12
typedef struct { uint32_t reg, val; } ra2d_escrita_t;
typedef struct { ra2d_escrita_t e[RA2D_MAX_ESCRITAS]; int n; } ra2d_ordem_t;

static inline void ra2d_por(ra2d_ordem_t *o, uint32_t reg, uint32_t val)
{
    if (o->n < RA2D_MAX_ESCRITAS) { o->e[o->n].reg = reg; o->e[o->n].val = val; }
    o->n++;   /* más de RA2D_MAX_ESCRITAS: error interno, que ra2d_ok ve */
}

static inline bool ra2d_ok(const ra2d_ordem_t *o) { return o->n > 0 && o->n <= RA2D_MAX_ESCRITAS; }

/* Relleno de un color (ra_preencher): 7 escrituras. */
static inline void ra2d_preencher(ra2d_ordem_t *o, uint32_t dst_po, int x, int y, int l, int a, uint32_t cor)
{
    o->n = 0;
    ra2d_por(o, RA2D_DP_GUI_MASTER_CNTL, RA2D_GMC_DST_PITCH_OFFSET | RA2D_GMC_BRUSH_SOLIDO | RA2D_GMC_DST_32BPP |
                                         RA2D_GMC_SRC_COR | RA2D_ROP3_P | RA2D_GMC_SEM_COMPARAR);
    ra2d_por(o, RA2D_DP_BRUSH_FRGD_CLR, cor);
    ra2d_por(o, RA2D_DP_WRITE_MASK, 0xFFFFFFFFu);
    ra2d_por(o, RA2D_DP_CNTL, RA2D_DIR_X_ESQ | RA2D_DIR_Y_CIMA);
    ra2d_por(o, RA2D_DST_PITCH_OFFSET, dst_po);
    ra2d_por(o, RA2D_DST_Y_X, ((uint32_t)y << 16) | (uint32_t)x);
    ra2d_por(o, RA2D_DST_HEIGHT_WIDTH, ((uint32_t)a << 16) | (uint32_t)l);
}

/* Copia de memoria a memoria (ra_copiar, las franjas del volcado y las de la vuelta). Las coordenadas son
 * las que se escriben (en una copia hacia atrás, ya la última columna o fila). «mascara» (D391): patrón
 * sólido 0x00FFFFFF y ROP origen AND patrón; «com_brush»: escribir DP_BRUSH_FRGD_CLR aunque no haya
 * máscara (como hacía gv_retornar). 8 escrituras, o 9 con el color del patrón. */
static inline void ra2d_copiar(ra2d_ordem_t *o, uint32_t src_po, uint32_t dst_po, uint32_t dir, int sx, int sy, int dx, int dy,
                               int l, int a, bool mascara, bool com_brush)
{
    o->n = 0;
    ra2d_por(o, RA2D_DP_GUI_MASTER_CNTL, RA2D_GMC_SRC_PITCH_OFFSET | RA2D_GMC_DST_PITCH_OFFSET |
                                         (mascara ? RA2D_GMC_BRUSH_SOLIDO : RA2D_GMC_BRUSH_NADA) | RA2D_GMC_DST_32BPP |
                                         RA2D_GMC_SRC_COR | (mascara ? RA2D_ROP3_SP : RA2D_ROP3_S) | RA2D_GMC_SRC_MEMORIA |
                                         RA2D_GMC_SEM_COMPARAR);
    if (mascara || com_brush) ra2d_por(o, RA2D_DP_BRUSH_FRGD_CLR, 0x00FFFFFFu);
    ra2d_por(o, RA2D_DP_WRITE_MASK, 0xFFFFFFFFu);
    ra2d_por(o, RA2D_DP_CNTL, dir);
    ra2d_por(o, RA2D_DST_PITCH_OFFSET, dst_po);
    ra2d_por(o, RA2D_SRC_PITCH_OFFSET, src_po);
    ra2d_por(o, RA2D_SRC_Y_X, ((uint32_t)sy << 16) | (uint32_t)sx);
    ra2d_por(o, RA2D_DST_Y_X, ((uint32_t)dy << 16) | (uint32_t)dx);
    ra2d_por(o, RA2D_DST_HEIGHT_WIDTH, ((uint32_t)a << 16) | (uint32_t)l);
}

/* Lo que va detrás de cada orden por el CP (RADEONFlush2D de xf86-video-ati, y el final de r100_copy_blit
 * de Linux): vaciar la caché de destino del 2D y que el CP no siga hasta que el 2D esté parado y limpio. */
static inline void ra2d_fim_cp(ra2d_ordem_t *o)
{
    ra2d_por(o, RA2D_DSTCACHE_CTLSTAT, RA2D_DC_FLUSH_ALL);
    ra2d_por(o, RA2D_WAIT_UNTIL, RA2D_WAIT_2D_IDLECLEAN | RA2D_WAIT_DMA_GUI_IDLE);
}

/* ¿Puede ir esta escritura por el anillo? Solo los registros del 2D de arriba (ni pantalla, ni relojes,
 * ni memoria, ni el propio CP). */
static inline bool ra2d_reg_permitido(uint32_t r)
{
    return (r & 3) == 0 && r >= 0x1400 && r < 0x1800;
}

/* Las palabras para el anillo: un PACKET0 (un registro) por escritura. Devuelve cuántas (0 si no cabe o
 * si hay un registro que no es del 2D). */
static inline uint32_t ra2d_pacotes(const ra2d_ordem_t *o, uint32_t *p, uint32_t max)
{
    if (!ra2d_ok(o) || (uint32_t)o->n * 2u > max) return 0;
    for (int i = 0; i < o->n; i++) {
        if (!ra2d_reg_permitido(o->e[i].reg)) return 0;
        p[2 * i] = o->e[i].reg >> 2;          /* PACKET0, un registro (cuenta 0) */
        p[2 * i + 1] = o->e[i].val;
    }
    return (uint32_t)o->n * 2u;
}

#endif
