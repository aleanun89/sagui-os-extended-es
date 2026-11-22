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
#include "abi.h"     /* gpu3d v2 (D426): gpu3d_estado2_t y sus G3D_* */

/* Órdenes PM4 (tipo 3). */
#define R63D_IT_NOP             0x10
#define R63D_IT_START_3D        0x24   /* solo R6xx */
#define R63D_IT_CONTEXT_CONTROL 0x28
#define R63D_IT_INDEX_TYPE      0x2A
#define R63D_IT_DRAW_INDEX      0x2B   /* D457: índices desde la memoria */
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
/* D432: con el ARRAY_MODE del color dado (0 lineal general, 1 lineal alineada): la pantalla como destino. */
void r63d_alvo_modo(r6_lote_t *l, uint32_t mc_cor, uint32_t mc_z, int passo, int alt_buf, int larg, int alt, int zbits, bool sbu,
                    uint32_t modo);
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


/* ============================================================ gpu3d v2 (D426) */

/* Vértices v2: gpu3d_vert2_t tal cual en el búfer de vértices (88 bytes). El sombreador de vértices v2 lee
 * posición, color, especular y los cuatro juegos (s, t, r, q) y exporta POS0 y PARAM0-5 (color, especular,
 * juegos 0-3); el de píxeles los recibe en R0-R5. */
#define R63D_VERT2_BYTES 88
#define R63D_VS2_MAX     48
int r63d_vs2(uint32_t *p);
/* SPI: cuántos parámetros pasan del de vértices al de píxeles (3 los de v1, 6 los de v2). */
void r63d_spi(r6_lote_t *l, int nparams);
/* Sombreadores de un dibujo v2: el de vértices v2 y uno de píxeles generado (gprs registros, «bytes» de largo
 * para el vaciado de la caché de sombreadores). */
void r63d_programas2(r6_lote_t *l, uint32_t mc_vs, uint32_t mc_ps, int gprs_ps, uint32_t bytes_ps);
/* Recurso y muestreador de la unidad u (0-3). fmt: G3D_FMT_*; passo en texels (alineado: r63d_passo_tex). */
void r63d_textura2(r6_lote_t *l, int u, uint32_t mc, int larg, int alt, int passo, uint32_t fmt, const gpu3d_unidade_t *un,
                   uint32_t bytes);
/* Paso de una textura v2 en texels: ARGB como el destino (lineal alineada); DXT en 1D en mosaico (ver
 * r63d_bc_desloc), múltiplo de 8 bloques (32 texels): r600_get_array_mode_alignment de Linux (r600_cs.c,
 * ARRAY_1D_TILED_THIN1: max(8, grupo / (8 * bytes del bloque)), que con grupos de 256 o 512 bytes es 8). */
static inline int r63d_passo_tex(int larg, uint32_t fmt, int alin)
{
    return fmt == G3D_FMT_ARGB8888 ? r63d_passo(larg, alin) : r63d_passo(larg, 32);
}
/* D426, DXT: las R6xx/R7xx no leen bien las texturas comprimidas en lineal (probado en la HD 4500: cada texel sale de
 * otro sitio; Mesa no las deja nunca en lineal, r600_texture.c: «Compressed textures ... must always be tiled»).
 * Van en ARRAY_1D_TILED_THIN1: micro-mosaicos de 8x8 bloques seguidos por filas (passo_blocos / 8 por fila) y,
 * dentro, el orden «displayable» de los elementos de 64 bits (BC1: x0 y0 x1 x2 y1 y2) o de 128 (BC2/BC3:
 * y0 x0 x1 x2 y1 y2), el de AddrLib (EgBasedLib::ComputePixelIndexWithinMicroTile) y el medido en la tarjeta.
 * Desplazamiento en bytes del bloque (bx, by); bs = 8 o 16 bytes por bloque. */
static inline uint32_t r63d_bc_desloc(uint32_t bx, uint32_t by, uint32_t passo_blocos, uint32_t bs)
{
    uint32_t x = bx & 7u, y = by & 7u, i;
    if (bs == 8) i = (x & 1u) | (y & 1u) << 1 | (x >> 1) << 2 | (y >> 1) << 4;
    else i = (y & 1u) | (x & 1u) << 1 | (x >> 1) << 2 | (y >> 1) << 4;
    uint32_t mosaico = (by >> 3) * (passo_blocos >> 3) + (bx >> 3);
    return (mosaico * 64u + i) * bs;
}
/* Z, esténcil, prueba alfa, máscaras, mezcla u operación lógica y tijera de un dibujo v2 (todo cada vez).
 * estencil: el destino tiene esténcil (Z de 24 + 8). */
void r63d_estado2(r6_lote_t *l, const gpu3d_estado2_t *e, int larg, int alt, bool estencil, bool r600_puro);
/* Lo que el estado de v1 no vuelve a poner tras un dibujo v2. */
void r63d_estado_v1(r6_lote_t *l);
/* n vértices de «passo» bytes en mc_vb. */
void r63d_desenhar2(r6_lote_t *l, uint32_t mc_vb, int n, uint32_t passo, bool sem_cache_vertices);
/* Los 32 bits del flotante b / 255 (0 <= b <= 255). */
uint32_t r63d_float_byte(uint32_t b);

/* El sombreador de píxeles de un estado v2 (combinación de hasta 4 unidades, especular y niebla).
 * Registros: R0 color, R1 especular, R2-R5 juegos de coordenadas, R6-R9 el texel de cada unidad, R10-R11 el
 * resultado de la unidad anterior (alternando), R12-R16 operandos y pasos intermedios, R17 la salida y R18 la
 * constante de la unidad. Las constantes (GL_TEXTURE_ENV_COLOR, la niebla) van como literales de la
 * instrucción: el programa depende de sus valores (y la caché del driver también). */
#define R63D_PS2_MAX   1024   /* palabras (4 KB en la VRAM) */
#define R63D_PS2_GPRS  19
typedef struct {
    uint32_t p[R63D_PS2_MAX];
    int      n;                /* palabras */
    int      grupos, clausulas;
    bool     erro;             /* no cabe: el dibujo va por software */
} r63d_ps2_t;
void r63d_ps2_gerar(r63d_ps2_t *ps, const gpu3d_estado2_t *e, bool r7xx);


/* ============================================================ gpu3d v3 (D457, «v3 en R6xx/R7xx»)
 *
 * La GPU lee los vértices de los búferes residentes y los transforma con un sombreador de vértices generado para la
 * transformación fija de ES 1.1: posición por la MVP, niebla lineal (distancia al ojo por la modelo-vista), matriz de
 * textura por unidad, color de bytes, de floats o constante. El recorte, la división por w, la vista y el descarte de
 * caras los hace el rasterizador (PA_CL_CLIP_CNTL, PA_CL_VTE_CNTL, PA_CL_VPORT_*, PA_SU_SC_MODE_CNTL). Las salidas son
 * las del sombreador de vértices de v2 (PARAM0 color, PARAM1 especular con el factor de niebla en el alfa, PARAM2-5 los
 * cuatro juegos de coordenadas), así el programa de píxeles de v2 (r63d_ps2_gerar) sirve tal cual.
 * Las constantes van al «constant file» del sombreador de vértices (SET_ALU_CONST desde la 256, con SQ_CONFIG.DX9_CONSTS
 * que ya pone el estado base), como el sombreador de vértices de xf86-video-ati (r600_shader.c, r600_exa.c). */
#define R63D_VS3_MAX     512     /* palabras de un sombreador v3 (2 KB en la VRAM) */
#define R63D_VS3_GPRS    14      /* R0 índice, R1 posición, R2 color, R3-R6 juegos leídos, R7 recorte, R8 ojo, R9 especular, R10-R13 juegos transformados */
#define R63D_VS3_NCONST  30      /* constantes: 0-3 MVP (filas), 4-6 modelo-vista (filas 0-2), 7 niebla (a, b, 0, 0), 8 color de la niebla, 9 color
                                  * constante, 10-13 coordenadas constantes, 14-29 matrices de textura (4 filas por unidad) */
#define R63D_VS3_BUFS    6       /* lecturas como mucho: posición, color y cuatro juegos (recursos 160-165 del sombreador de vértices) */
#define R63D_ALU_CONST_VS 0x31000u   /* SQ_ALU_CONSTANT0_0 + 256 * 16: la constante 0 del sombreador de vértices */
#define R63D_V3_NEB_NADA      0
#define R63D_V3_NEB_ESPEC     1  /* el factor en el alfa del especular (un ESTADO2 con G3D2_NEBLINA) */
#define R63D_V3_NEB_MEZCLADA  2  /* ya mezclada: color.rgb * f, especular.rgb = color de la niebla * (1 - f) */
/* Lo que decide el programa (no los valores: esos son constantes). Comparable con memcmp (relleno a 0). */
typedef struct {
    uint8_t pos_fmt;             /* G3D_AT_FLOAT2/3/4 */
    uint8_t cor_fmt;             /* G3D_AT_NADA (constante), G3D_AT_UBYTE4N o G3D_AT_FLOAT4 */
    uint8_t tc_fmt[4];           /* por unidad < unidades: G3D_AT_NADA (constante) o FLOAT2/3/4 */
    uint8_t unidades;            /* juegos con valor; los demás salen (0, 0, 0, 1) */
    uint8_t neblina;             /* R63D_V3_NEB_* */
    uint8_t tex_mat;             /* bit u: la unidad u pasa por su matriz */
    uint8_t reservado[3];
} r63d_vs3_desc_t;
typedef struct {
    uint32_t p[R63D_VS3_MAX];
    int      n;                  /* palabras */
    int      grupos, clausulas_alu, lecturas;
    uint8_t  atr[R63D_VS3_BUFS]; /* qué lee el recurso 160 + k: 0 posición, 1 color, 2 + u juego u */
    bool     erro;               /* no cabe o una regla de los puertos no se cumple: el dibujo va por la CPU */
} r63d_vs3_t;
void r63d_vs3_gerar(r63d_vs3_t *vs, const r63d_vs3_desc_t *d, bool r7xx);
/* Las constantes de una transformación para ese programa (R63D_VS3_NCONST x 4 palabras, bits de float). */
void r63d_vs3_constantes(uint32_t c[R63D_VS3_NCONST * 4], const gpu3d_transf_t *t);
void r63d_vs3_consts_emitir(r6_lote_t *l, const uint32_t c[R63D_VS3_NCONST * 4]);
/* El rasterizador para v3: recorte (espacio de GL), vista y w de verdad. Y lo de v1/v2 otra vez (sin recorte, vértices
 * ya en la pantalla, sin descarte). */
void r63d_v3_entrar(r6_lote_t *l);
void r63d_v3_vista(r6_lote_t *l, const gpu3d_transf_t *t);
void r63d_v3_sair(r6_lote_t *l);
/* PA_SU_SC_MODE_CNTL para G3D_DESC_*: la cara de delante es la de las agujas del reloj en la pantalla (FACE = 1, la
 * del estado base) y G3D_DESC_POS (área > 0 con la y hacia abajo: en el sentido de las agujas) quita esa. «inv»
 * (diagnóstico) la otra convención. */
uint32_t r63d_v3_descarte(uint8_t descarte, bool inv);
/* Sombreadores de un dibujo v3: el de vértices generado (vs_bytes de largo) y uno de píxeles de v2. */
void r63d_programas3(r6_lote_t *l, uint32_t mc_vs, uint32_t vs_bytes, uint32_t mc_ps, int gprs_ps, uint32_t bytes_ps);
/* Los recursos de lectura de vértices (k = 0..nb-1: el 160 + k) y el dibujo: «n» vértices (sin índices) o «n» índices
 * de 16 bits desde mc_idx. prim: G3D_PRIM_*. */
typedef struct { uint32_t mc, bytes, passo; } r63d_vbuf_t;
void r63d_desenhar3(r6_lote_t *l, const r63d_vbuf_t *b, int nb, int prim, uint32_t n, bool com_indices, uint32_t mc_idx);
/* SURFACE_SYNC de la caché de vértices (o de la de texturas en las que no la tienen) sobre un rango. */
void r63d_sync_vertices(r6_lote_t *l, uint32_t mc, uint32_t bytes, bool sem_cache_vertices);

#endif
