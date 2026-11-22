// SPDX-License-Identifier: GPL-2.0-or-later
/* evergreen_3d.h - paquetes PM4, sombreadores y valores de inicio del motor 3D de las
 * Radeon Evergreen (HD 5000: Cedar, Redwood, Juniper, Cypress, Hemlock) y Northern
 * Islands sin Cayman (HD 6000 y HD 7500M/7600M: Barts, Turks, Caicos) (D427). Lógica
 * pura, sin registros ni memoria de vídeo: la usa el driver
 * (kernel/dispositivos/gpu3d_evergreen.c) y se compila también en el PC para la prueba
 * de unidad (tests/evergreen_3d_unidad.c, con -DR600_ANFITRION), que compara los bytes
 * con una segunda codificación independiente.
 *
 * Qué cambia respecto de las R6xx/R7xx (r600_3d.h), y por eso es otro fichero:
 *   - recursos en 0x30000 (8 palabras cada uno; los de vértices del VS desde el 176) y
 *     no hay SET_ALU_CONST (las constantes van en búferes: aquí no se usan);
 *   - el secuenciador (SQ) tiene GPR, hilos y pila también para HS y LS, en otros
 *     registros (0x8C18...), y LDS;
 *   - el color (CB_COLOR0_*) y el Z (DB_Z_INFO, bases de lectura y escritura) son otros
 *     registros; la mezcla se enciende en CB_BLEND0_CONTROL (bit 30) y no en
 *     CB_COLOR_CONTROL; el Z tiene que ir en mosaico (1D aquí);
 *   - el sombreador de píxeles interpola él mismo los parámetros (INTERP_XY/ZW con las
 *     baricéntricas que el SPI deja en R0); el CF tiene otra codificación (contador de
 *     6 bits, instrucción en los bits 29:22; EXPORT 0x53 y EXPORT_DONE 0x54);
 *   - la palabra 4 del recurso de textura y el muestreador cambian de sitio algunos campos;
 *   - las direcciones del MC son de 40 bits (D408): todos los registros de base llevan
 *     los bits 39:8 y el recurso de vértices los bits 39:32 en su palabra 2.
 * Cayman (HD 6900) NO está: VLIW4, otro CF (sin END_OF_PROGRAM), otro inicio del CP.
 *
 * Referencias (solo números de registro, formato de los paquetes y de las
 * instrucciones, y el orden de las secuencias; el código es propio, docs/terceros.md):
 *   - xf86-video-ati (MIT): evergreen_accel.c, evergreen_shader.c/.h, evergreen_reg*.h;
 *   - Mesa 23.3 (MIT): src/gallium/drivers/r600/evergreend.h, eg_sq.h, evergreen_state.c;
 *   - radeon de Linux 6.6 (ficheros con cabecera MIT): evergreen.c (evergreen_gpu_init,
 *     evergreen_cp_load_microcode, evergreen_cp_resume, evergreen_cp_start),
 *     evergreend.h, evergreen_reg.h, evergreen_blit_shaders.h (estado por defecto). */
#ifndef SAGUI_EVERGREEN_3D_H
#define SAGUI_EVERGREEN_3D_H

#include "r600_3d.h"     /* r6_lote_t, r63d_lote, r63d_p, r63d_float, r63d_me_initialize: iguales en Evergreen */

/* Órdenes PM4 que solo usa Evergreen (las demás, R63D_IT_* de r600_3d.h, tienen el mismo número). */
#define EG3D_IT_CLEAR_STATE      0x12
#define EG3D_IT_PREAMBLE_CNTL    0x4A
#define   EG3D_PREAMBLE_INICIO   (2u << 28)
#define   EG3D_PREAMBLE_FIM      (3u << 28)

/* Familias (índice de las tablas). */
#define EG3D_CEDAR    0
#define EG3D_REDWOOD  1
#define EG3D_JUNIPER  2
#define EG3D_CYPRESS  3     /* también Hemlock (HD 5970): mismos valores y microcódigo */
#define EG3D_BARTS    4     /* Northern Islands («BTC»): Barts, Turks, Caicos */
#define EG3D_TURKS    5
#define EG3D_CAICOS   6
#define EG3D_N_FAMILIAS 7
#define EG3D_E_NI(f)  ((f) >= EG3D_BARTS && (f) <= EG3D_CAICOS)

/* Lo propio de cada familia (evergreen_gpu_init de Linux 6.6; GB_ADDR_CONFIG «golden» de evergreend.h). */
typedef struct {
    const char *nome;            /* «Cedar»... (el nombre de la tabla del 2D empieza así) */
    const char *firmware;        /* CEDAR, REDWOOD... (_pfp.bin y _me.bin de linux-firmware) */
    int num_ses, pipes, tile_pipes, simds, backends, gprs, threads, gs_threads, pilha, sx_sets;
    int exp, exp_pos, exp_smx, contextos, cf_insts, prim_fifo, hiz_fifo, earlyz_fifo;
    uint32_t gb_addr_config;
    bool cache_vertices;         /* Cedar y Caicos no tienen: los vértices pasan por la caché de texturas */
} eg3d_chip_t;
const eg3d_chip_t *eg3d_chip(int familia);
/* La familia por el nombre de la tabla del 2D (evergreen_dma.c): -1 si no es de las que lleva el 3D. */
int eg3d_familia_por_nome(const char *nome);

/* Microcódigo del CP (evergreen_cp_load_microcode): las mismas medidas en Evergreen y en Barts/Turks/Caicos. */
#define EG3D_PFP_PALAVRAS   1120     /* 4480 bytes */
#define EG3D_ME_PALAVRAS    1376     /* 5504 bytes */

/* Registros del CP y del motor por MMIO: los mismos números que en R7xx (r600_3d.h, R6_*), salvo estos. */
#define EG3D_CP_SEM_INCOMPLETE_TIMER_CNTL 0x85C8
#define EG3D_SCRATCH_ADDR        0x8544
#define EG3D_GRBM_GFX_INDEX      0x802C
#define EG3D_RLC_GFX_INDEX       0x3FC4
#define   EG3D_GFX_BROADCAST     ((1u << 30) | (1u << 31))   /* INSTANCE_BROADCAST_WRITES | SE_BROADCAST_WRITES */
#define   EG3D_GFX_SE(i)         ((1u << 30) | ((uint32_t)(i) << 16))
#define EG3D_MC_ARB_RAMCFG       0x2760
#define EG3D_GB_ADDR_CONFIG      0x98F8
#define EG3D_GB_BACKEND_MAP      0x98FC
#define EG3D_CC_RB_BACKEND_DISABLE 0x98F4
#define EG3D_CC_GC_SHADER_PIPE_CONFIG 0x8950
#define EG3D_SX_DEBUG_1          0x9058
#define EG3D_SMX_DC_CTL0         0xA020
#define EG3D_SQ_CONFIG           0x8C00
#define EG3D_SQ_LDS_RESOURCE_MGMT 0x8E2C
#define EG3D_MC_SEQ_SUP_CNTL     0x28C8      /* solo se lee: bit 0, el secuenciador del MC está en marcha (microcódigo MC ya cargado) */
/* GRBM_SOFT_RESET: Linux reinicia junto al CP los bloques que dependen de él (evergreen_cp_resume). */
#define EG3D_SOFT_RESET_CP_Y_3D  ((1u << 0) | (1u << 5) | (1u << 8) | (1u << 9) | (1u << 10) | (1u << 14))   /* CP PA SPI SH SX VGT */

typedef struct {
    uint32_t mc_arb_ramcfg, gb_addr_config, gb_backend_map;
    uint32_t rb_desativados;          /* bits de CC_RB_BACKEND_DISABLE (23:16) de cada motor de sombreado, 4 por motor */
    uint32_t sx_debug_1, smx_dc_ctl0, sq_config, sq_lds_resource_mgmt;
} eg3d_leituras_t;

/* El mapa de backends de GB_BACKEND_MAP (r6xx_remap_render_backend de Linux con 4 bits por tubería). */
uint32_t eg3d_mapa_backends(uint32_t n_tub_log2, uint32_t max_rb, uint32_t total_rb, uint32_t desativados);
/* GB_ADDR_CONFIG y GB_BACKEND_MAP que se van a usar: lo que dejó la BIOS si no es cero (la BIOS los programa
 * al arrancar; cambiarlos con la pantalla encendida no aporta nada); si no, el «golden» y el calculado. */
uint32_t eg3d_gb_addr_config(int familia, const eg3d_leituras_t *l);
uint32_t eg3d_gb_backend_map(int familia, const eg3d_leituras_t *l);
/* Las escrituras por MMIO de evergreen_gpu_init que caen en el motor 3D (no toca pantalla, DMIF, HDP, DMA,
 * UVD ni el MC). Devuelve cuántos pares, o -1 si no caben o la familia no está. */
#define EG3D_INICIO_MAX 64
int eg3d_inicio_motor(int familia, const eg3d_leituras_t *l, r6_par_t *p, int max, uint32_t *max_contextos);
/* SQ_CONFIG ... SQ_STACK_RESOURCE_MGMT_3 (9 valores, en el orden de los registros 0x8C00, 0x8C04, 0x8C08,
 * 0x8C0C, 0x8C18, 0x8C1C, 0x8C20, 0x8C24, 0x8C28), con la fórmula de Linux. */
void eg3d_valores_sq(int familia, uint32_t sq_config_lido, uint32_t v[9]);
/* Alineación del paso de una superficie lineal alineada (píxeles de 32 bits): max(64, grupo / 4), con el
 * tamaño del grupo (PIPE_INTERLEAVE_SIZE, bits 6:4 de GB_ADDR_CONFIG: 256 << n bytes). */
int eg3d_alinhamento_passo(uint32_t gb_addr_config);
/* NUM_BANKS de CB_COLOR*_ATTRIB, DB_Z_INFO y la palabra 7 de la textura: 0 = 2, 1 = 4, 2 = 8, 3 = 16 bancos
 * (MC_ARB_RAMCFG bits 1:0: 0 = 4, 1 = 8, 2 = 16). */
uint32_t eg3d_bancos(uint32_t mc_arb_ramcfg);

/* Lotes: las mismas funciones que en R6xx/R7xx salvo la cabecera de los registros (rangos de Evergreen). */
void eg3d_reg(r6_lote_t *l, uint32_t reg, uint32_t valor);
void eg3d_regs(r6_lote_t *l, uint32_t reg, const uint32_t *valores, int n);

/* El estado base (una vez tras arrancar el CP): CONTEXT_CONTROL, el estado de contexto por defecto dentro
 * del preámbulo (PREAMBLE_CNTL + CLEAR_STATE, como evergreen_cp_start) y otra vez detrás (por si el
 * microcódigo no lo guardase), las constantes de control, el secuenciador (SQ) y el sombreador de lectura
 * de vértices aparte apuntando a «mc_fs» (no se usa: el de vértices lee él mismo). */
void eg3d_estado_base(r6_lote_t *l, int familia, uint32_t sq_config_lido, uint64_t mc_fs);
/* Superficie de color (lineal alineada, ARGB8888) y Z (mosaico 1D, 16 o 24 bits; zbits 0: sin Z). */
void eg3d_alvo(r6_lote_t *l, uint64_t mc_cor, uint64_t mc_z, int passo, int alt_buf, int larg, int alt, int zbits, uint32_t bancos);
/* D432: con el ARRAY_MODE del color dado (0 lineal general, 1 lineal alineada): la pantalla como destino. */
void eg3d_alvo_modo(r6_lote_t *l, uint64_t mc_cor, uint64_t mc_z, int passo, int alt_buf, int larg, int alt, int zbits, uint32_t bancos,
                    uint32_t modo);
/* Sombreadores en VRAM (direcciones múltiplo de 256): el de vértices y el de píxeles (con o sin textura). */
void eg3d_programas(r6_lote_t *l, uint64_t mc_vs, uint64_t mc_ps);
void eg3d_textura(r6_lote_t *l, uint64_t mc, int larg, int alt, int passo, bool borde_s, bool borde_t, bool linear, uint32_t bancos);
/* Z, mezcla y recorte (z_func: 1 <, 3 <=, 7 siempre; factores: 0 cero, 1 uno, 4 alfa, 5 uno menos alfa). */
void eg3d_estado(r6_lote_t *l, bool z_teste, bool z_escrita, uint32_t z_func, bool mezcla, uint32_t src, uint32_t dst, bool cor_escrita,
                 int rx, int ry, int rl, int ra);
/* n vértices de 32 bytes en mc_vb. prim: 4 triángulos, 2 líneas. */
void eg3d_desenhar(r6_lote_t *l, uint64_t mc_vb, int n, int prim, bool cache_vertices);
/* Vacía las cachés de color y Z, espera al motor y escribe «valor» en un registro de apuntes. */
void eg3d_valla(r6_lote_t *l, uint32_t scratch, uint32_t valor, bool cache_vertices);
/* SURFACE_SYNC sobre toda la memoria (CP_COHER_SIZE 0xFFFFFFFF, base 0), como Mesa en Evergreen. */
void eg3d_sync(r6_lote_t *l, uint32_t acoes);
#define EG3D_SYNC_CB0  ((1u << 25) | (1u << 6))     /* CB_ACTION_ENA | CB0_DEST_BASE_ENA */
#define EG3D_SYNC_DB   ((1u << 26) | (1u << 14))    /* DB_ACTION_ENA | DB_DEST_BASE_ENA */
#define EG3D_SYNC_TC   (1u << 23)
#define EG3D_SYNC_VC   (1u << 24)
#define EG3D_SYNC_SH   (1u << 27)

/* Los sombreadores, escritos a mano (ISA de Evergreen). Devuelven cuántas palabras (<= EG3D_SH_MAX).
 *   vértices:  R1 = posición (4 flotantes), R2 = color y R3 = especular (4 bytes -> RGBA 0..1),
 *              R4 = (u, v, 0, 1); exporta POS0 = R1, PARAM0 = R2, PARAM1 = R3, PARAM2 = R4;
 *   píxeles:   R0.xy = baricéntricas (perspectiva, centro); interpola R1 = color, R2 = especular
 *              (y R3 = uv); color: sale sat(R1 + R2); textura: R3 = muestra(textura 0, R3.xy),
 *              sale sat(sat(R1 * R3) + R2). */
#define EG3D_SH_MAX     96
#define EG3D_SH_PASSO   0x200      /* bytes entre sombreadores en la página: VS en 0, color en 0x200, textura en 0x400 */
#define EG3D_PS_GPRS    4
#define EG3D_VS_GPRS    5
int eg3d_vs(uint32_t *p);
int eg3d_ps_cor(uint32_t *p);
int eg3d_ps_tex(uint32_t *p);

/* Recursos: el 0 del sombreador de píxeles (textura) y el 0 del de vértices (el 176). */
#define EG3D_RES_PS0    0x30000u
#define EG3D_RES_VS0    (0x30000u + 176u * 32u)

#define EG3D_VERT_BYTES 32   /* gpu3d_vert_t: x, y, z, 1/w, color, especular, u, v */

#endif
