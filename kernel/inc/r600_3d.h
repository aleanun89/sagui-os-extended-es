// SPDX-License-Identifier: GPL-2.0-or-later
/* r600_3d.h - paquetes PM4, sombreadores y valores de inicio del motor 3D de las
 * Radeon R6xx/R7xx (D284). Lógica pura, sin registros ni memoria de vídeo: la usa el
 * driver (kernel/dispositivos/gpu3d_r600.c) y se compila también en el PC para la
 * prueba de unidad (tests/r600_3d_unidad.c, con -DR600_ANFITRION), que compara los
 * bytes con una segunda codificación independiente.
 *
 * Referencias (solo números de registro, formato de los paquetes y de las
 * instrucciones, y el orden de las secuencias; el código es propio, docs/terceros.md):
 *   - xf86-video-ati (MIT): r600_reg*.h, r600_shader.h/.c, r6xx_accel.c, r600_exa.c;
 *   - Mesa (MIT): src/gallium/drivers/r600/r600d.h y r600_state.c;
 *   - radeon de Linux 6.6 (ficheros con cabecera MIT): r600.c, rv770.c, r600d.h, rv770d.h;
 *   - r600_cp.c de FreeBSD 9.3 (MIT).
 *
 * El procesador de órdenes (CP) lee palabras de 32 bits de un anillo. Un paquete
 * de tipo 3: cabecera 0xC0000000 | (n - 1) << 16 | orden << 8, y n palabras.
 * Los registros se escriben con SET_CONFIG_REG (0x8000-0xAC00), SET_CONTEXT_REG
 * (0x28000-0x29000), SET_ALU_CONST, SET_RESOURCE (0x38000...) y SET_SAMPLER
 * (0x3C000...): la primera palabra es (registro - base) / 4. */
#ifndef SAGUI_R600_3D_H
#define SAGUI_R600_3D_H

#ifdef R600_ANFITRION
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#else
#include "tipos.h"
#endif

/* Órdenes PM4 (tipo 3). */
#define R63D_IT_NOP             0x10
#define R63D_IT_START_3D        0x24   /* solo R6xx */
#define R63D_IT_CONTEXT_CONTROL 0x28
#define R63D_IT_INDEX_TYPE      0x2A
#define R63D_IT_DRAW_INDEX_AUTO 0x2D
#define R63D_IT_NUM_INSTANCES   0x2F
#define R63D_IT_CP_DMA          0x41   /* D406: copia de memoria por el CP (r600_copy_cpdma) */
#define R63D_IT_SURFACE_SYNC    0x43
#define R63D_IT_ME_INITIALIZE   0x44
#define R63D_IT_EVENT_WRITE     0x46
#define R63D_IT_SET_CONFIG_REG  0x68
#define R63D_IT_SET_CONTEXT_REG 0x69
#define R63D_IT_SET_ALU_CONST   0x6A
#define R63D_IT_SET_RESOURCE    0x6D
#define R63D_IT_SET_SAMPLER     0x6E
#define R63D_IT_SET_CTL_CONST   0x6F
#define R63D_IT_SURFACE_BASE_UPDATE 0x73   /* solo RV6xx */
#define R63D_PKT3(op, n)        (0xC0000000u | ((((uint32_t)(n) - 1u) & 0x3FFFu) << 16) | ((uint32_t)(op) << 8))

/* Familias con valores propios. 0-3 son R7xx; 4-7 son R6xx (D357). */
#define R63D_RV770  0
#define R63D_RV730  1
#define R63D_RV740  2
#define R63D_RV710  3
#define R63D_R600   4     /* HD 2900: sin mezcla por superficie */
#define R63D_RV610  5     /* HD 2400 y la RV620 (HD 3400): mismos valores en Linux y Mesa; sin caché de vértices */
#define R63D_RV630  6     /* HD 2600 */
#define R63D_RV670  7     /* HD 3800 */
#define R63D_RV635  8     /* HD 3600: la RV630 salvo DB_DEBUG y SQ_MS_FIFO_SIZES */
#define R63D_N_FAMILIAS 9
#define R63D_E_R7XX(f)          ((f) >= R63D_RV770 && (f) <= R63D_RV710)
#define R63D_E_R6XX(f)          ((f) >= R63D_R600 && (f) <= R63D_RV635)
/* Sin caché de vértices (SQ_CONFIG sin VC_ENABLE; los vértices se sincronizan por la de texturas). */
#define R63D_SEM_CACHE_VTX(f)   ((f) == R63D_RV710 || (f) == R63D_RV610)
/* Las RV6xx (no la R600 ni las R7xx) necesitan SURFACE_BASE_UPDATE tras cambiar la base
 * del color o del Z (rodeo de su hardware; xf86-video-ati y Mesa lo emiten). */
#define R63D_PRECISA_SBU(f)     ((f) >= R63D_RV610 && (f) <= R63D_RV635)

/* Registros que el driver lee o escribe por MMIO (por debajo de 64 KB). */
#define R6_GRBM_STATUS          0x8010
#define   R6_GUI_ACTIVE         (1u << 31)
#define R6_GRBM_STATUS2         0x8014
#define R6_GRBM_SOFT_RESET      0x8020
#define   R6_SOFT_RESET_CP      1u
#define R6_WAIT_UNTIL           0x8040
#define R6_SCRATCH_REG0         0x8500   /* +4*n; el 1 para la prueba de la cola, el 2 para las vallas */
#define R6_SCRATCH_UMSK         0x8540
#define R6_CP_SEM_WAIT_TIMER    0x85BC
#define R6_CP_ME_CNTL           0x86D8
#define   R6_CP_ME_HALT         (1u << 28)
#define   R6_CP_PFP_HALT        (1u << 26)
#define R6_CP_RB_RPTR           0x8700
#define R6_CP_RB_WPTR_DELAY     0x8704
#define R6_CP_STAT              0x8680
#define R6_CP_RB_BASE           0xC100
#define R6_CP_RB_CNTL           0xC104
#define   R6_RB_NO_UPDATE       (1u << 27)
#define   R6_RB_RPTR_WR_ENA     (1u << 31)
#define R6_CP_RB_RPTR_WR        0xC108
#define R6_CP_RB_RPTR_ADDR      0xC10C
#define R6_CP_RB_RPTR_ADDR_HI   0xC110
#define R6_CP_RB_WPTR           0xC114
#define R6_CP_INT_CNTL          0xC124
#define R6_CP_PFP_UCODE_ADDR    0xC150
#define R6_CP_PFP_UCODE_DATA    0xC154
#define R6_CP_ME_RAM_RADDR      0xC158
#define R6_CP_ME_RAM_WADDR      0xC15C
#define R6_CP_ME_RAM_DATA       0xC160
#define R6_CP_DEBUG             0xC1FC
#define R7_PFP_UCODE_PALAVRAS   848      /* RV7xx_pfp.bin: 3392 bytes */
#define R7_ME_UCODE_PALAVRAS    1360     /* RV7xx_me.bin: 5440 bytes */
#define R6_PFP_UCODE_PALAVRAS   576      /* R600/RV6xx_pfp.bin: 2304 bytes (R600_PFP_UCODE_SIZE) */
#define R6_ME_UCODE_PALAVRAS    5376     /* R600/RV6xx_me.bin: 21504 bytes (R600_PM4_UCODE_SIZE * 3) */

/* Lo que rv7_inicio_motor necesita leer antes (por MMIO). */
#define R7_MC_ARB_RAMCFG              0x2760
#define R7_CC_GC_SHADER_PIPE_CONFIG   0x8950
#define R7_CC_RB_BACKEND_DISABLE      0x98F4
#define R7_TA_CNTL_AUX                0x9508
#define R7_SX_DEBUG_1                 0x9058
#define R7_SMX_DC_CTL0                0xA020
#define R7_DB_DEBUG3                  0x98B0
#define R7_DB_DEBUG4                  0x9B8C
#define R7_SQ_CONFIG                  0x8C00
/* Lo que r6_inicio_motor lee (R6xx; los demás números son los mismos que en R7xx). */
#define R6_RAMCFG                     0x2408
#define R6_SX_DEBUG_1                 0x9054
#define R6_SQ_MS_FIFO_SIZES           0x8CF0
typedef struct {
    uint32_t mc_arb_ramcfg, cc_gc_shader_pipe_config, cc_rb_backend_disable;
    uint32_t ta_cntl_aux, sx_debug_1, smx_dc_ctl0, db_debug3, db_debug4, sq_config;
    uint32_t sq_ms_fifo_sizes;   /* D357: solo lo usa el inicio de las R6xx */
} r7_leituras_t;   /* en R6xx: mc_arb_ramcfg es RAMCFG (0x2408) y sx_debug_1 el 0x9054; smx/db_debug3/4 no se leen */
typedef struct { uint32_t reg, valor; } r6_par_t;
#define R7_INICIO_MAX 64

/* Las escrituras por MMIO que dejan el motor 3D de una R7xx con los valores por
 * defecto (lo de rv770_gpu_init que cae por debajo de 64 KB; lo demás va por la
 * cola en r63d_estado_base). No toca la pantalla (DCP), el HDP ni el DMA.
 * Devuelve cuántos pares, o -1 si no caben o la familia no es de las R7xx. */
int r7_inicio_motor(int familia, const r7_leituras_t *l, r6_par_t *p, int max, uint32_t *max_contextos);
/* Lo mismo para las R6xx (lo de r600_gpu_init de Linux por debajo de 64 KB, D357). Tampoco toca
 * el HDP (ni sus superficies), DCP/HDP/DMA_TILING_CONFIG ni el árbitro de memoria (ARB_*). */
int r6_inicio_motor(int familia, const r7_leituras_t *l, r6_par_t *p, int max, uint32_t *max_contextos);
/* La que toque según la familia. */
int r63d_inicio_motor(int familia, const r7_leituras_t *l, r6_par_t *p, int max, uint32_t *max_contextos);

/* Un lote de palabras para la cola. Si no cabe, «cheio» queda a true y no se escribe más. */
typedef struct { uint32_t *p; int n, max; bool cheio; } r6_lote_t;
void r63d_lote(r6_lote_t *l, uint32_t *p, int max);
void r63d_p(r6_lote_t *l, uint32_t v);
void r63d_reg(r6_lote_t *l, uint32_t reg, uint32_t valor);
void r63d_regs(r6_lote_t *l, uint32_t reg, const uint32_t *valores, int n);

/* Los 32 bits IEEE 754 de un entero (el kernel no usa coma flotante). */
uint32_t r63d_float(int v);
#define R63D_F_0     0x00000000u
#define R63D_F_1     0x3F800000u

void r63d_me_initialize(r6_lote_t *l, bool r7xx, uint32_t max_contextos);   /* 7 palabras */
void r63d_teste_cola(r6_lote_t *l, uint32_t scratch, uint32_t valor);       /* 3 palabras */
/* El estado base del contexto (una vez tras iniciar el CP): recursos del secuenciador,
 * recorte desactivado, vértices ya en pantalla (x, y, z, 1/w), interpolación con
 * perspectiva, tres parámetros (color, especular y textura), sin plantilla ni MSAA. */
void r63d_estado_base(r6_lote_t *l, int familia, bool r7xx, uint32_t qd_pipes, uint32_t mc_fs);
/* Superficie de color (lineal, ARGB8888) y Z (mosaico 1D, 16 o 24 bits) y los recortes.
 * «sbu»: emitir SURFACE_BASE_UPDATE tras las bases (R63D_PRECISA_SBU, solo RV6xx). */
void r63d_alvo(r6_lote_t *l, uint32_t mc_cor, uint32_t mc_z, int passo, int alt_buf, int larg, int alt, int zbits, bool sbu);
/* Sombreadores en VRAM (direcciones múltiplo de 256): el de vértices y el de píxeles. */
void r63d_programas(r6_lote_t *l, uint32_t mc_vs, uint32_t mc_ps, bool com_textura);
void r63d_textura(r6_lote_t *l, uint32_t mc, int larg, int alt, int passo, bool borde_s, bool borde_t, bool linear);
/* Z, mezcla y recorte, con los números de los registros (z_func: 1 <, 3 <=, 7 siempre; factores:
 * 0 cero, 1 uno, 4 alfa, 5 uno menos alfa). «r600_puro»: la R600 original no tiene mezcla por superficie. */
void r63d_estado(r6_lote_t *l, bool z_teste, bool z_escrita, uint32_t z_func, bool mezcla, uint32_t src, uint32_t dst, bool cor_escrita,
                 int rx, int ry, int rl, int ra, bool r600_puro);
/* n vértices de 28 bytes en mc_vb. prim: 4 triángulos, 1 líneas. */
void r63d_desenhar(r6_lote_t *l, uint32_t mc_vb, int n, int prim, bool sem_cache_vertices);
/* Vacía las cachés de color y Z, espera al motor y escribe «valor» en un registro de apuntes. */
void r63d_valla(r6_lote_t *l, uint32_t mc_cor, uint32_t bytes_cor, uint32_t mc_z, uint32_t bytes_z, uint32_t scratch, uint32_t valor);
/* D406: copia «bytes» de «orig» a «dest» (direcciones del MC; «dest» puede ser la ventana de la GART) con
 * CP_DMA, como r600_copy_cpdma de Linux 6.6: antes, esperar al 3D parado y vaciar la caché del color del
 * rango de origen; trozos de 0x1FFFFF bytes como mucho, el último con CP_SYNC, y al final esperar a que
 * el CP_DMA termine (WAIT_UNTIL bit 8). Devuelve false si «bytes» es 0. */
bool r63d_cp_dma(r6_lote_t *l, uint32_t orig, uint32_t dest, uint32_t bytes);
#define R63D_CP_DMA_MAX 0x1FFFFFu

/* Los sombreadores, escritos a mano (ISA de la R600). Devuelven cuántas palabras (<= R63D_SH_MAX).
 *   vértices:  R1 = posición (4 flotantes), R2 = color y R3 = especular (4 bytes -> RGBA 0..1),
 *              R4 = (u, v, 0, 1); exporta POS0 = R1, PARAM0 = R2, PARAM1 = R3, PARAM2 = R4;
 *   color:     entran R0 = color, R1 = especular; sale sat(R0 + R1);
 *   textura:   además R2 = uv; R2 = muestra(textura 0); sale sat(sat(R0 * R2) + R1). */
#define R63D_SH_MAX 32
int r63d_vs(uint32_t *p, bool r7xx);
int r63d_ps_cor(uint32_t *p, bool r7xx);
int r63d_ps_tex(uint32_t *p, bool r7xx);

#define R63D_VERT_BYTES 32   /* gpu3d_vert_t: x, y, z, 1/w, color, especular, u, v */

/* D402: alineación del paso (en píxeles de 32 bits) de una superficie lineal alineada
 * (ARRAY_LINEAR_ALIGNED: destino de color y texturas), según el GB_TILING_CONFIG que quedó
 * en el motor: max(64, tamaño del grupo / 4 bytes), como r600_get_array_mode_alignment de
 * Linux 6.6 (r600_cs.c). GROUP_SIZE (bit 6) es 256 o 512 bytes; con 512 (el HP dv7, RV710)
 * el motor redondea el paso a 128 píxeles, y una textura con paso 64 se leía con las filas
 * de dos en dos (prueba 4 de la autoprueba, 2026-11-19). */
static inline int r63d_alinhamento_passo(uint32_t gb_tiling_config)
{
    uint32_t grupo = (gb_tiling_config >> 6) & 1u ? 512u : 256u;
    return grupo / 4u > 64u ? (int)(grupo / 4u) : 64;
}
static inline int r63d_passo(int larg, int alin) { return (larg + alin - 1) / alin * alin; }

#endif
