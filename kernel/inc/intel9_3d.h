// SPDX-License-Identifier: GPL-2.0-or-later
/* intel9_3d.h - el 3D de Intel Gen9-12 (Skylake ... Alder Lake): órdenes del motor de render
 * (RCS), estado del cauce, sombreadores en la ISA de las EU con un ensamblador mínimo propio y
 * workarounds (D438). Lógica pura, sin registros ni memoria de la GPU: la usa el driver
 * (kernel/dispositivos/gpu3d_intel9.c) y se compila también en el PC para la prueba de unidad
 * (tests/intel9_3d_unidad.c, con -DINTEL9_ANFITRION).
 *
 * Fase A (gpu3d v1): triángulos ya transformados (x, y en píxeles, z de 0 a 1 y rhw), Z, mezcla
 * y una textura ARGB; color = saturar(textura x color + especular).
 *
 * Cómo se dibuja (como blorp de Mesa, que hace lo mismo para sus copias):
 *   - el sombreador de vértices va APAGADO: el VF escribe la VUE directamente (cabecera a 0,
 *     posición x, y, z, rhw tal cual, color, especular y u, v) y el clipper la pasa sin la
 *     división de perspectiva (3DSTATE_CLIP: PerspectiveDivideDisable) ni recorte; el SF no
 *     transforma (ViewportTransformEnable = 0), así que x, y ya son píxeles y la cuarta
 *     componente llega al rasterizador como 1/w (las baricéntricas de perspectiva);
 *   - el sombreador de píxeles es SIMD8, escrito a mano (dos: color y textura x color +
 *     especular). Interpola con MUL/ADD (sin PLN ni MAD: así vale tal cual para Gen11, que no
 *     tiene PLN) y termina con un RT write con EOT desde g120-g123;
 *   - cada tanda (lote, D448) empieza con TODO el estado fijo (el contexto no se restaura),
 *     lleva varios dibujos con solo el estado que cambia y acaba con un PIPE_CONTROL que vacía
 *     las cachés de color, Z y datos con CS stall y escribe la marca de fin en la página de
 *     estado (GGTT): el vaciado va siempre delante de la espera (la lección de la X1400, D426).
 *
 * Referencias (valores y secuencias; el código es propio, docs/terceros.md):
 *   - Mesa 23.1 (MIT): src/intel/genxml/gen9.xml, gen11.xml, gen12.xml (posición de cada campo
 *     de cada orden), blorp/blorp_genX_exec.h (el cauce sin VS), gallium/drivers/iris/iris_state.c
 *     (inicio del contexto y workarounds de PIPE_CONTROL), compiler/brw_inst.h, brw_eu.h,
 *     brw_eu_defines.h, brw_reg_type.c, brw_eu.c (codificación de las instrucciones y de los
 *     mensajes), common/intel_l3_config.c, intel_urb_config.c, dev/intel_device_info.c;
 *   - i915 de Linux 6.6 (ficheros MIT): gt/intel_workarounds.c (workarounds de Gen9),
 *     gt/intel_lrc.c (imagen del RCS, en acel_intel_logica.h), gt/gen8_engine_cs.c (la marca de
 *     fin del RCS), gt/intel_gt_regs.h, gt/intel_engine_regs.h, intel_uncore.c (forcewake);
 *   - los vectores de prueba del ensamblador de Mesa (src/intel/tools/tests/gen9/, ficheros .expected)
 *     comprueban el codificador en tests/intel9_3d_unidad.c.
 *
 * Copyright de los valores, posiciones de campos y secuencias tomados: Intel Corporation y los
 * demás autores de Mesa y de i915, con licencia MIT (docs/terceros.md, D438). El código es propio.
 *
 * Gen11 y Gen12: el codificador ya sabe sus tipos (Gen11) y su formato (Gen12, con SWSB: cada
 * instrucción de ALU espera a la anterior y el SEND del muestreador lleva su testigo), y las
 * órdenes que cambian están marcadas; el driver todavía solo arranca en Gen9 (ver D438). */
#ifndef SAGUI_INTEL9_3D_H
#define SAGUI_INTEL9_3D_H

#ifdef INTEL9_ANFITRION
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#else
#include "tipos.h"
#endif

/* ------------------------------------------------------------------ ensamblador de las EU */

/* Ficheros de registros (BRW_*_REGISTER_FILE) y tipos lógicos. */
#define I9_ARF   0
#define I9_GRF   1
#define I9_IMM   3
#define I9_UD    0
#define I9_D     1
#define I9_UW    2
#define I9_F     3

/* Un operando: registro (nr, sub en bytes, región <vs;w,hs> en elementos) o inmediato. */
typedef struct {
    uint8_t  arq, tipo;
    uint8_t  nr, sub;           /* sub: desplazamiento en bytes dentro del registro */
    uint8_t  vs, w, hs;         /* región en elementos: <8;8,1> vector, <0;1,0> escalar */
    uint32_t imm;
    uint8_t  neg;               /* D453: fuente negada (-x); solo registros, solo Gen8-11 */
} i9_op_t;

static inline i9_op_t i9_vec(uint8_t nr, uint8_t tipo) { i9_op_t o = { I9_GRF, tipo, nr, 0, 8, 8, 1, 0, 0 }; return o; }
static inline i9_op_t i9_esc(uint8_t nr, uint8_t sub, uint8_t tipo) { i9_op_t o = { I9_GRF, tipo, nr, sub, 0, 1, 0, 0, 0 }; return o; }
static inline i9_op_t i9_nulo(uint8_t tipo) { i9_op_t o = { I9_ARF, tipo, 0, 0, 8, 8, 1, 0, 0 }; return o; }
static inline i9_op_t i9_imf(uint32_t bits) { i9_op_t o = { I9_IMM, I9_F, 0, 0, 0, 1, 0, bits, 0 }; return o; }
static inline i9_op_t i9_neg(i9_op_t o) { o.neg = (uint8_t)!o.neg; return o; }

/* Opcodes lógicos (los valores de la ISA cambian en Gen12: i9eu_opcode). */
#define I9_OP_MOV    1
#define I9_OP_ADD    2
#define I9_OP_MUL    3
#define I9_OP_SEND   4
#define I9_OP_SENDC  5
#define I9_OP_NOP    6
#define I9_OP_MATH   7          /* D457: MATH (función en los bits 27:24 en Gen8-11) */

/* Funciones de MATH (BRW_MATH_FUNCTION_*). */
#define I9_MATH_INV   1
#define I9_MATH_SQRT  4
#define I9_MATH_RSQ   5

/* Funciones compartidas (SFID) de los mensajes. */
#define I9_SFID_SAMPLER  2
#define I9_SFID_RENDER   5      /* GFX6_SFID_DATAPORT_RENDER_CACHE */
#define I9_SFID_URB      6      /* D457: BRW_SFID_URB (la salida del sombreador de vértices) */

/* SWSB de Gen12 (tgl_swsb_encode de Mesa, sin tubos: Tiger/Alder Lake son 12.0). */
#define I9_SWSB_NADA         0x00
#define I9_SWSB_REG(n)       ((uint8_t)(n))                         /* @n: espera a la instrucción n de atrás */
#define I9_SWSB_SET(t)       ((uint8_t)(0x40 | (t)))                /* $t: el SEND pone el testigo t */
#define I9_SWSB_DST(t)       ((uint8_t)(0x20 | (t)))                /* $t.dst: espera a que el SEND t escriba */
#define I9_SWSB_REG_SET(n, t) ((uint8_t)(0x80 | ((n) << 4) | (t)))  /* @n y $t a la vez */

typedef struct {
    uint32_t *p;                /* 4 palabras por instrucción */
    int n, max;                 /* instrucciones */
    int gen;                    /* 9, 11 o 12 */
    bool transbordou;
} i9eu_t;

void i9eu_iniciar(i9eu_t *e, uint32_t *p, int max_inst, int gen);
/* Dos fuentes (ADD, MUL), una (MOV, s1 se ignora) o ninguna (NOP). «sat»: saturar a [0, 1]. */
void i9eu_alu(i9eu_t *e, int op, bool sat, i9_op_t dst, i9_op_t s0, i9_op_t s1, uint8_t swsb);
/* SEND/SENDC de 8 canales: «desc» = descriptor del mensaje (longitudes, función), sin el EOT. */
void i9eu_send(i9eu_t *e, int op, int sfid, i9_op_t dst, uint8_t src0_nr, uint32_t desc, bool eot, uint8_t swsb);
/* El valor de opcode de la ISA para la generación (para la prueba de unidad). */
uint32_t i9eu_opcode(int op, int gen);
/* D457: MATH de una fuente (I9_MATH_*: SQRT, RSQ, INV), de 8 canales y tipo F (gfx6_math de Mesa con src1 nulo).
 * Solo Gen8-11 (en Gen12 no se ha comprobado el formato: no hace nada y marca transbordou). */
void i9eu_math(i9eu_t *e, uint32_t funcao, i9_op_t dst, i9_op_t src);
/* D457: la última instrucción con «Mask Control» = NoMask (WE_all: todos los canales, como las cabeceras de los
 * mensajes en Mesa: LOAD_PAYLOAD con exec_all). Bit 34 en Gen8-11. */
void i9eu_nomask(i9eu_t *e);

/* Descriptores de los mensajes (brw_message_desc, brw_fb_write_desc, brw_sampler_desc). */
static inline uint32_t i9_desc_msg(uint32_t mlen, uint32_t rlen, bool cab)
{
    return (mlen & 0xFu) << 25 | (rlen & 0x1Fu) << 20 | (cab ? 1u << 19 : 0u);
}
/* RT write SIMD8 de una fuente (msg_control 4), último destino, superficie «bti». */
static inline uint32_t i9_desc_rt_simd8(uint32_t bti)
{
    return i9_desc_msg(4, 0, false) | 12u << 14 | 1u << 12 | 4u << 8 | (bti & 0xFFu);
}
/* «sample» SIMD8 (u, v), cuatro registros de vuelta (R, G, B, A), retorno de 32 bits. */
static inline uint32_t i9_desc_sample_simd8(uint32_t bti, uint32_t sampler)
{
    return i9_desc_msg(2, 4, false) | 0u << 12 | 1u << 17 | (sampler & 0xFu) << 8 | (bti & 0xFFu);
}

/* D457: escritura SIMD8 en la URB (brw_urb_desc de Mesa para Gen8+: GFX8_URB_OPCODE_SIMD8_WRITE = 7, desplazamiento
 * global en unidades de 128 bits, es decir, de «ranuras» de la VUE, en los bits 14:4), con cabecera (los 8 asideros
 * de la URB, copiados de g1). «mlen» = 1 + 4 x ranuras. */
static inline uint32_t i9_desc_urb_simd8(uint32_t mlen, uint32_t ranura)
{
    return i9_desc_msg(mlen, 0, true) | (ranura & 0x7FFu) << 4 | 7u;
}

/* ------------------------------------------------------------------ los sombreadores */

/* Disposición de la entrada del sombreador de píxeles SIMD8 (setup_fs_payload_gen6 de Mesa):
 * g0 cabecera, g1 coordenadas de los subtramos, g2-g3 baricéntricas de perspectiva (b1, b2), y
 * desde g4 los datos de cada atributo (2 registros por atributo: por componente, 4 floats
 * [v1-v0, v2-v0, -, v0]). Atributos: 0 color, 1 especular, 2 u v. */
#define I9_PS_GRF_SETUP   4
#define I9_PS_N_ATRIB     3
#define I9_PS_MAX_INST    96
#define I9_KERNEL_BYTES   2048     /* sitio de cada sombreador (con relleno detrás para la precarga) */

/* Escribe el sombreador de color (textura = false) o de textura en «p» (I9_PS_MAX_INST * 4
 * palabras como mucho) y devuelve el número de instrucciones (0: no cabe o generación no
 * válida). Muestreador 0 y superficie 1 para la textura; destino de color en la superficie 0. */
int i9_ps_gerar(uint32_t *p, int gen, bool textura);

/* D453, gpu3d v2: el sombreador de píxeles se genera de la combinación de las unidades (GL_COMBINE),
 * como r63d_ps2_gerar en las Radeon. Atributos: 0 color, 1 especular (con el factor de niebla en el
 * alfa), 2 + u las (s, t) de la unidad u (datos desde g4, 2 registros cada uno). La unidad u muestrea
 * con el muestreador u y la superficie 1 + u. Constantes de G3D_CB_* / G3D_FT_* / G3D_OPD_* (abi.h). */
typedef struct {
    uint8_t ativa;
    uint8_t rgb_func, alfa_func, rgb_escala, alfa_escala;
    uint8_t rgb_fonte[3], rgb_opd[3], alfa_fonte[3], alfa_opd[3];
    uint32_t constante;          /* 0xAARRGGBB */
} i9_unid_t;
typedef struct {
    uint8_t n;                   /* unidades que cuentan (0..4) */
    bool neblina;
    uint32_t cor_neblina;        /* 0x00RRGGBB */
    i9_unid_t u[4];
} i9_ps2_t;
#define I9_PS2_MAX_INST   240
#define I9_KERNEL2_BYTES  4096   /* sitio de cada sombreador de v2 (240 instrucciones y el relleno) */
/* Escribe el sombreador en «p» (como mucho max_inst instrucciones) y devuelve cuántas (0: no cabe, o
 * Gen12, que todavía no sabe negar fuentes). Rellenar «c» desde un memset a cero (se compara entero). */
int i9_ps2_gerar(uint32_t *p, int max_inst, int gen, const i9_ps2_t *c);

/* ---- D457 (gpu3d v3 en Gen9): el sombreador de vértices SIMD8 de la transformación fija de ES 1.1.
 * Cada canal es un vértice. Entrada (setup_vs_payload de Mesa, VS escalar): g0 cabecera, g1 los 8 asideros de la
 * URB, g2.. las constantes empujadas (nc registros de 8 floats: se leen como escalares <0;1,0>) y desde g2 + nc los
 * atributos que escribe el VF, 4 registros cada uno (x, y, z, w de los 8 vértices): 0 la posición, después el color
 * (si viene de un array) y las coordenadas de las unidades que vienen de un array, por orden de unidad.
 * Salida (la VUE, como la que escribe el VF en v1/v2, así el SBE y los sombreadores de píxeles no cambian):
 * ranura 0 cabecera a 0, 1 la posición de recorte (mvp x pos), 2 el color (de 0 a 1), 3 el especular (r, g, b, f:
 * el factor de niebla en el alfa, o la niebla ya mezclada, D451), 4 + u las (s, t, 0, 1) de la unidad u (por su
 * matriz si se pide). Lo escriben mensajes URB SIMD8 de dos ranuras (8 registros) con la cabecera; el último con EOT
 * desde g112. El recorte, la división por w y la vista los hace la GPU (3DSTATE_CLIP, 3DSTATE_SF).
 * Constantes (floats, i9_vs_consts): 0-15 mvp, 16-31 mv (por columnas, como GL), 32-35 el color constante, 36 neb_a,
 * 37 neb_b, 40-42 el color de la niebla, 48 + 4u + k la coordenada constante de la unidad u, 64 + 16u la matriz de
 * textura de la unidad u (solo si alguna la lleva). */
typedef struct {
    uint8_t n_unid;              /* unidades cuyas coordenadas se escriben (0..4) */
    uint8_t cor;                 /* 0 constante, 1 de un array UBYTE4N (el VF ya lo da de 0 a 1), 2 de un array FLOAT4 (se satura) */
    uint8_t tc[4];               /* 0 constante, 1 de un array */
    uint8_t tex_mat;             /* bit u: la unidad u por su matriz (filas 0 y 1: s y t) */
    uint8_t neblina;             /* 0 sin niebla, 1 el factor en el alfa del especular, 2 mezclada en el color (D451) */
} i9_vs_t;
#define I9_VS_MAX_INST   220
#define I9_KERNELV_BYTES 4096    /* sitio de cada sombreador de vértices (220 instrucciones y el relleno) */
#define I9_VS_ATR_MAX    6
/* Registros de constantes que lee (8 floats cada uno) y atributos de entrada. */
int i9_vs_nc(const i9_vs_t *k);
int i9_vs_n_atr(const i9_vs_t *k);
/* Escribe el sombreador (como mucho max_inst instrucciones) y devuelve cuántas (0: no cabe o generación no válida:
 * solo Gen9 y Gen11). Rellenar «k» desde un memset a cero (se compara entero en la caché del driver). */
int i9_vs_gerar(uint32_t *p, int max_inst, int gen, const i9_vs_t *k);
/* Lo que va en las constantes (todo en bits de float salvo los colores 0xAARRGGBB / 0x00RRGGBB). */
typedef struct {
    const uint32_t *mvp, *mv;    /* 16 cada una */
    const uint32_t (*tex)[16];   /* 4 matrices */
    uint32_t cor;                /* 0xAARRGGBB */
    const uint32_t (*tc_const)[4];
    uint32_t neb_a, neb_b, neb_cor;
} i9_vs_dados_t;
/* Llena «c» (i9_vs_nc(k) x 8 palabras). */
void i9_vs_consts(uint32_t *c, const i9_vs_t *k, const i9_vs_dados_t *d);

/* ------------------------------------------------------------------ órdenes y estado */

/* Cabeceras de las órdenes (genxml gen9.xml; las de Gen11/12 que cambian, con su generación). */
#define I9_MI_NOOP                     0x00000000u
#define I9_MI_BATCH_BUFFER_END         0x05000000u
#define I9_MI_LOAD_REGISTER_IMM(n)     (0x11000000u | (2u * (n) - 1u))
#define I9_PIPE_CONTROL                0x7a000004u   /* 6 palabras */
#define I9_PIPELINE_SELECT             0x69040000u   /* 1 palabra */
#define I9_STATE_BASE_ADDRESS_9        0x61010011u   /* 19 palabras en Gen9 */
#define I9_STATE_BASE_ADDRESS_11       0x61010014u   /* 22 en Gen11 y Gen12 */
#define I9_3DSTATE_VS                  0x78100007u
#define I9_3DSTATE_HS                  0x781b0007u
#define I9_3DSTATE_TE                  0x781c0002u
#define I9_3DSTATE_DS                  0x781d0009u
#define I9_3DSTATE_GS                  0x78110008u
#define I9_3DSTATE_STREAMOUT           0x781e0003u
#define I9_3DSTATE_CLIP                0x78120002u
#define I9_3DSTATE_SF                  0x78130002u
#define I9_3DSTATE_RASTER              0x78500003u
#define I9_3DSTATE_SBE                 0x781f0004u
#define I9_3DSTATE_SBE_SWIZ            0x78510009u
#define I9_3DSTATE_WM                  0x78140000u
#define I9_3DSTATE_PS                  0x7820000au
#define I9_3DSTATE_PS_EXTRA            0x784f0000u
#define I9_3DSTATE_PS_BLEND            0x784d0000u
#define I9_3DSTATE_WM_DEPTH_STENCIL    0x784e0002u
#define I9_3DSTATE_DEPTH_BUFFER        0x78050006u
#define I9_3DSTATE_STENCIL_BUFFER_9    0x78060003u   /* 5 palabras (8 en Gen12) */
#define I9_3DSTATE_HIER_DEPTH_BUFFER   0x78070003u
#define I9_3DSTATE_CLEAR_PARAMS        0x78040001u
#define I9_3DSTATE_VERTEX_BUFFERS      0x78080000u   /* + 4 por búfer - 1 */
#define I9_3DSTATE_VERTEX_ELEMENTS     0x78090000u   /* + 2 por elemento - 1 */
#define I9_3DSTATE_VF                  0x780c0000u
#define I9_3DSTATE_VF_TOPOLOGY         0x784b0000u
#define I9_3DSTATE_VF_SGVS             0x784a0000u
#define I9_3DSTATE_VF_INSTANCING       0x78490001u
#define I9_3DSTATE_VF_STATISTICS       0x680b0000u
#define I9_3DPRIMITIVE                 0x7b000005u
#define I9_3DSTATE_INDEX_BUFFER        0x780a0003u   /* D457 */
#define I9_3DSTATE_URB_VS              0x78300000u   /* +0x10000: HS, DS, GS */
#define I9_3DSTATE_PUSH_CONSTANT_ALLOC_VS 0x79120000u /* +0x10000: HS, DS, GS, PS */
#define I9_3DSTATE_CONSTANT_VS         0x78150009u
#define I9_3DSTATE_CONSTANT_HS         0x78190009u
#define I9_3DSTATE_CONSTANT_DS         0x781a0009u
#define I9_3DSTATE_CONSTANT_GS         0x78160009u
#define I9_3DSTATE_CONSTANT_PS         0x78170009u
#define I9_3DSTATE_BINDING_TABLE_POINTERS_VS 0x78260000u /* +0x10000: HS, DS, GS, PS */
#define I9_3DSTATE_SAMPLER_STATE_POINTERS_PS 0x782f0000u
#define I9_3DSTATE_BLEND_STATE_POINTERS 0x78240000u
#define I9_3DSTATE_CC_STATE_POINTERS   0x780e0000u
#define I9_3DSTATE_VIEWPORT_STATE_POINTERS_CC      0x78230000u
#define I9_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP 0x78210000u
#define I9_3DSTATE_SCISSOR_STATE_POINTERS 0x780f0000u
#define I9_3DSTATE_DRAWING_RECTANGLE   0x79000002u
#define I9_3DSTATE_MULTISAMPLE         0x780d0000u
#define I9_3DSTATE_SAMPLE_MASK         0x78180000u
#define I9_3DSTATE_WM_HZ_OP            0x78520003u
#define I9_3DSTATE_POLY_STIPPLE_OFFSET 0x79060000u
#define I9_3DSTATE_AA_LINE_PARAMETERS  0x790a0001u
#define I9_3DSTATE_WM_CHROMAKEY        0x784c0000u
#define I9_3DSTATE_SAMPLE_PATTERN      0x791c0007u

/* PIPE_CONTROL: bits de la palabra 1 (genxml, desplazados de 32). */
#define I9_PC_DEPTH_FLUSH      (1u << 0)
#define I9_PC_STALL_SCOREBOARD (1u << 1)
#define I9_PC_STATE_INVAL      (1u << 2)
#define I9_PC_CONST_INVAL      (1u << 3)
#define I9_PC_VF_INVAL         (1u << 4)
#define I9_PC_DC_FLUSH         (1u << 5)
#define I9_PC_TEX_INVAL        (1u << 10)
#define I9_PC_INST_INVAL       (1u << 11)
#define I9_PC_RT_FLUSH         (1u << 12)
#define I9_PC_DEPTH_STALL      (1u << 13)
#define I9_PC_WRITE_IMM        (1u << 14)    /* Post Sync Operation = Write Immediate Data */
#define I9_PC_WRITE_TS         (3u << 14)    /* Post Sync Operation = Write Timestamp */
#define I9_PC_TLB_INVAL        (1u << 18)    /* con CS stall y una escritura (gen8_emit_flush_rcs) */
#define I9_PC_CS_STALL         (1u << 20)
#define I9_PC_GGTT             (1u << 24)    /* Destination Address Type */

/* Workarounds de Gen9 (i915: gen9_ctx_workarounds_init + kbl/cfl, rcs_engine_wa_init y
 * gen9_gt_workarounds_init). «mascarado»: registro con máscara en la mitad alta. */
typedef struct i9_wa {
    uint32_t reg, set, clr;
    uint8_t  mascarado;
    uint8_t  tipo;               /* I9_WA_* */
    const char *nome;
} i9_wa_t;
#define I9_WA_CTX     1          /* van por LRI en cada tanda (contexto) */
#define I9_WA_MOTOR   2          /* registro del RCS por MMIO, al encender */
#define I9_WA_GT      3          /* registro global del GT por MMIO, al encender */
/* Las de la variante (0 SKL, 1 KBL, 2 CFL/CML, como i915): devuelve cuántas y deja la tabla. */
int i9_workarounds(int gen, int plataforma, const i9_wa_t **t);
#define I9_PLAT_SKL  0
#define I9_PLAT_KBL  1
#define I9_PLAT_CFL  2           /* Coffee, Whiskey, Amber y Comet Lake: lo de i915 para CFL/CML */
int i9_plataforma(uint16_t modelo);

/* ---- Lotes (D448): una tanda con varios dibujos.
 * Antes (D438) cada dibujo era una tanda con TODO el estado y la CPU esperaba a la anterior. Ahora
 * una tanda («lote») se abre con el estado fijo y el del destino (prólogo), lleva tantos dibujos
 * como quepan (de cada uno solo el estado que cambia y un 3DPRIMITIVE) y se cierra con el vaciado y
 * la marca. El driver la manda al leer el cuadro, al esperar, o cuando se llena; mientras la GPU
 * la ejecuta, la CPU monta la siguiente en el otro hueco.
 *
 * Direcciones gráficas (PPGTT) de 32 bits; los desplazamientos de estado son desde «base»
 * (STATE_BASE_ADDRESS de superficies, estado dinámico e instrucciones). */
typedef struct {
    int gen;
    uint32_t base;               /* dirección gráfica de la zona */
    uint32_t marca_ggtt;         /* dirección en la GGTT de la palabra de la marca (alineada a 8) */
    uint32_t marca;              /* valor de la marca de este lote */
    uint32_t scratch;            /* dirección gráfica de 8 bytes para los PIPE_CONTROL de los workarounds */
    uint32_t k_cor, k_tex;       /* desplazamientos de los dos sombreadores (alineados a 64) */
    /* destino */
    uint32_t cor, cor_passo;     /* bytes por fila (múltiplo de 64) */
    int larg, alt;
    uint8_t cor_mocs;            /* entrada de MOCS de la superficie del destino (0: la de la zona; 1: RAM con caché) */
    uint32_t z, z_passo;         /* Z en mosaico Y (paso múltiplo de 128) */
    int zbits;                   /* 16 o 24 */
    uint32_t est, est_passo;     /* D453: esténcil de 8 bits en mosaico W (0: sin esténcil; paso de i9_passo_est, múltiplo de 128) */
    /* vértices del lote: un búfer de gpu3d_vert_t (32 bytes); cada dibujo empieza en su vértice */
    uint32_t vb, vb_bytes;
    bool tlb;                    /* invalidar el TLB de la GPU al principio (la PPGTT cambió) */
    uint32_t perfil_ggtt;        /* D448: != 0: marcas de tiempo de la GPU (5 x 8 bytes en la GGTT) en el
                                  * principio, tras los registros, tras el estado, tras los dibujos y al final */
    bool sem_l3;                 /* D448 (prueba): sin el LRI de L3CNTLREG (se queda el del lote anterior) */
    /* ajustes de depuración: pares registro/valor por LRI tras el estado fijo (n_aj) */
    const uint32_t *aj_reg, *aj_val;
    int n_aj;
    /* workarounds de contexto por LRI (pueden ser 0) */
    const struct i9_wa *wa;
    int n_wa;
} i9_lote_cfg_t;

/* Una textura de un dibujo (lineal, B8G8R8A8). env_*: G3D_ENV_* (0 repetir, 1 borde, 2 espejo). */
typedef struct {
    uint32_t tex, passo;
    int larg, alt;
    uint8_t mag_linear, min_linear, env_s, env_t;
    uint8_t niveis;              /* D453: niveles de mipmap (0 o 1: solo el 0) */
    uint8_t mip;                 /* 0 sin mipmaps, 1 el nivel más próximo, 2 entre dos niveles */
    uint8_t mosaico_y;           /* D453: en mosaico Y (la CPU la escribe con i9_ytile_off); si no, lineal */
    int8_t lod_bias;             /* D453 (prueba): sesgo del LOD en niveles enteros */
} i9_tex_t;

/* D453: el byte de (x_bytes, y) en una superficie en mosaico Y (bloques de 4 KB de 128 bytes x 32 filas,
 * columnas de 16 bytes de alto 32; «passo» múltiplo de 128). */
static inline uint32_t i9_ytile_off(uint32_t xb, uint32_t y, uint32_t passo)
{
    uint32_t tile = (y / 32u) * (passo / 128u) + xb / 128u;
    return tile * 4096u + ((xb % 128u) / 16u) * 512u + (y % 32u) * 16u + (xb % 16u);
}

/* D453: niveles de una textura 2D lineal como los coloca la GPU (Gen4-9 «2D», isl ISL_DIM_LAYOUT_GEN4_2D):
 * el 0 arriba, el 1 debajo, del 2 en adelante a la derecha del 1, uno debajo de otro; cada nivel alineado
 * a 4x4 (HALIGN_4, VALIGN_4). Posición del nivel l en píxeles y tamaño total para «niveis» niveles. */
void i9_mip_pos(int w0, int h0, int l, int *x, int *y);
void i9_mip_tamanho(int w0, int h0, int niveis, int *w, int *h);

#define I9_FMT_V1   1            /* gpu3d_vert_t (32 bytes): posición, color, especular, u v */
#define I9_FMT_V2   2            /* gpu3d_vert2_t (88 bytes): además (s, t) de las cuatro unidades */
#define I9_FMT_V3   3            /* D457: los atributos de los búferes residentes, por el sombreador de vértices */
#define I9_VERT2_BYTES 88

/* El estado de un dibujo (D448 para v1; D453 para gpu3d v2). Los códigos ya son los de la GPU
 * (3D_Compare_Function, 3D_Stencil_Operation, 3D_Color_Buffer_Blend_Factor/Function,
 * 3D_Logic_Op_Function: i9_cmp, i9_op_est, i9_fator2, i9_eq, i9_logica). Se compara entero para
 * saber si cambió: rellenarlo siempre desde un memset a cero. */
typedef struct {
    uint8_t formato;             /* I9_FMT_* */
    uint8_t n_tex;               /* texturas que muestrea el sombreador (0..4): unidades 0..n-1 */
    uint32_t k_ps;               /* desplazamiento del sombreador (alineado a 64) */
    i9_tex_t t[4];
    bool z_teste, z_escrita;
    uint8_t func_z;
    bool est;                    /* esténcil (pide un búfer de esténcil en el lote) */
    uint8_t est_func, est_ref, est_mascara, est_escrita, est_falha, est_zfalha, est_zpassa;
    bool alfa;                   /* prueba alfa (función fija: BLEND_STATE + COLOR_CALC_STATE) */
    uint8_t alfa_func, alfa_ref;
    bool tijera;                 /* recorte; x, y, l, a en píxeles del destino */
    uint16_t tx, ty, tl, ta;
    bool mezcla, alfa_sep;       /* alfa_sep: factores y ecuación propios del alfa */
    uint8_t f_orig, f_dest, f_orig_a, f_dest_a, eq, eq_a;
    bool logica;
    uint8_t op_logica;
    uint8_t mascara;             /* canales que se escriben: G3D_MC_* (1 R, 2 G, 4 B, 8 A) */
} i9_desenho_t;

typedef struct {
    i9_lote_cfg_t c;
    uint32_t *cmd;               /* órdenes (las escribe el lote: puede ser la ventana WC del anillo) */
    int n, max;                  /* palabras */
    uint32_t *est;               /* estado dinámico y de superficies (la ventana del hueco de estado) */
    uint32_t est_off;            /* desplazamiento de est[0] desde la base (alineado a 64) */
    uint32_t est_n, est_max;     /* bytes usados y disponibles */
    uint32_t o_rt, o_borda;      /* superficie del destino y color de borde (desde la base) */
    uint8_t fmt;                 /* formato de vértices puesto (0: ninguno) */
    uint32_t vb_base;            /* byte del búfer del lote desde el que cuenta el primer vértice */
    i9_desenho_t ult;            /* el último estado puesto */
    bool tem_ult;
    int desenhos, mudancas;      /* dibujos y cambios de estado del lote */
    bool erro;
    /* D457: la geometría de v3 (sombreador de vértices, recorte y vista por la GPU) puesta, y lo último de ella. */
    uint32_t o_sfvp;             /* la vista de v1/v2 (sin transformación) */
    bool v3;                     /* el cauce está con el VS encendido */
    bool v3_tem;                 /* hay algo de v3 puesto en este lote (lo que sigue vale) */
    uint32_t v3_kvs, v3_vp[6], v3_cst[128], v3_ve[2 * I9_VS_ATR_MAX];
    uint8_t v3_nc, v3_natr, v3_nve, v3_cull, v3_frente, topo;
    int v3_desenhos, v3_estados;
} i9_lote_t;

/* D457: la geometría de un dibujo v3. Los búferes ya están en la PPGTT; la capa ya comprobó que cada atributo cabe en
 * lo subido para todos los vértices del dibujo (y el VF, además, no lee fuera de «bytes»: da 0). */
#define I9_VF_RGBA32F  0x000u
#define I9_VF_RGB32F   0x040u
#define I9_VF_RG32F    0x085u
#define I9_VF_RGBA8    0x0C7u    /* R8G8B8A8_UNORM: cuatro bytes R, G, B, A */
typedef struct {
    uint32_t dir, bytes;         /* dirección gráfica del atributo del vértice 0 y bytes que se pueden leer desde ahí */
    uint16_t passo, fmt;         /* paso (bytes) y formato del VF (I9_VF_*) */
    uint8_t comps;               /* componentes que trae (2, 3 o 4: el resto, 0 y 1) */
} i9_vb3_t;
typedef struct {
    uint32_t k_vs;               /* desplazamiento del sombreador de vértices (alineado a 64) */
    uint8_t nc, n_atr;           /* registros de constantes (i9_vs_nc) y atributos (i9_vs_n_atr) */
    const uint32_t *cst;         /* nc x 8 palabras (i9_vs_consts) */
    i9_vb3_t vb[I9_VS_ATR_MAX];  /* uno por atributo de entrada, en el orden del sombreador */
    uint32_t ib, ib_bytes;       /* índices de 16 bits (ib 0: sin índices) */
    uint8_t topologia;           /* 4 lista, 5 tira, 6 abanico (3DPRIM_*) */
    uint8_t cull, frente;        /* 3DSTATE_RASTER: CullMode (1 ninguno, 2 delante, 3 detrás) y FrontWinding (0 CW, 1 CCW) */
    uint8_t mocs;                /* entrada de MOCS de los búferes (1: la caché la dice la PTE) */
    uint32_t vp[6];              /* m00 m11 m22 m30 m31 m32 de la vista (bits de float) */
    uint32_t primeiro, n;        /* primer vértice (o índice) y cuántos */
} i9_geo3_t;

#define I9_DESENHO_PAL   200     /* palabras como mucho por dibujo (cambio de estado y de vértices + 3DPRIMITIVE; D457: v3) */
#define I9_DESENHO_EST   1408    /* bytes de estado como mucho por dibujo (con su alineación; D457: constantes y vista de v3) */
#define I9_FIM_PAL       20      /* el cierre: vaciado, marca, las dos marcas de tiempo y el relleno a par */
#define I9_PROLOGO_PAL   (560 + 3 * 32 + 3 * 16)   /* el prólogo con 32 workarounds y 16 ajustes, de sobra */
#define I9_PROLOGO_EST   512
#define I9_VERT_BYTES    32

/* Abre un lote: el prólogo en «cmd» (como mucho «max» palabras) y el estado del destino en «est»
 * (est_max bytes, que la GPU ve en base + est_off; est_off + est_max <= 64 KB: la tabla de
 * enlaces se apunta con 16 bits). 0 bien; -1 si algo no cuadra o no cabe. */
int i9_lote_abrir(i9_lote_t *l, const i9_lote_cfg_t *c, uint32_t *cmd, int max, uint32_t *est, uint32_t est_off,
                  uint32_t est_max);
/* ¿Cabe un dibujo más (con el peor cambio de estado) y el cierre? */
bool i9_lote_cabe(const i9_lote_t *l);
/* Un dibujo de «n» vértices (múltiplo de 3) que empiezan en el byte «off» del búfer del lote; el
 * estado solo se pone si cambió (y el búfer de vértices, si cambió el formato). 0 bien; -1 si no
 * cabe o no cuadra (el lote queda con error). */
int i9_lote_desenho(i9_lote_t *l, const i9_desenho_t *d, uint32_t off, int n);
/* D457: un dibujo v3 (el estado de píxeles en «d», como uno de v2, y la geometría en «g»). Al pasar de v1/v2 a v3 y al
 * revés se vuelve a poner el cauce de geometría (VS, recorte, vista, descarte de caras) con una espera del cauce
 * delante. De la geometría solo se pone lo que cambia (el sombreador de vértices, las constantes, la vista, los
 * elementos); los búferes y el 3DPRIMITIVE, siempre. 0 bien; -1 si no cabe o no cuadra (el lote queda con error). */
int i9_lote_desenho3(i9_lote_t *l, const i9_desenho_t *d, const i9_geo3_t *g);
/* Cierra el lote: vaciado de color, Z y datos con CS stall y la marca. Devuelve las palabras
 * (par) o -1 si hubo error. */
int i9_lote_fechar(i9_lote_t *l);

/* Factores de mezcla y funciones del Z de gpu3d (v1) en los de la GPU. */
int i9_fator(int g3d_f);
int i9_func_z(int g3d_z);
/* D453, gpu3d v2: G3D_CMP_*, G3D_EST_*, G3D_F_* (0-10), G3D_EQ_* y G3D_LOG_* (el orden de GL) en los de
 * la GPU; -1 si no hay equivalente. */
int i9_cmp(int g3d_cmp);
int i9_op_est(int g3d_est);
int i9_fator2(int g3d_f);
int i9_eq(int g3d_eq);
int i9_logica(int g3d_log);

/* Paso y alto del Z (mosaico Y: 128 bytes por 32 filas) y del color (lineal, 64 bytes). */
static inline uint32_t i9_passo_cor(int larg) { return ((uint32_t)larg * 4u + 63u) & ~63u; }
static inline uint32_t i9_passo_z(int larg, int zbits) { return ((uint32_t)larg * (zbits == 16 ? 2u : 4u) + 127u) & ~127u; }
static inline uint32_t i9_alt_z(int alt) { return ((uint32_t)alt + 31u) & ~31u; }
/* El esténcil de 8 bits en mosaico W (bloques de 64 bytes x 64 filas): el paso que se programa es el doble del ancho
 * alineado a 64 (cada bloque W ocupa 128 bytes x 32 filas, como uno Y; PRM de Broadwell, vol. 2d, «Surface Pitch», que
 * cita isl.c de Mesa 23.1.9 en ISL_TILING_W). Alto alineado a 64; bytes = i9_passo_est x i9_alt_est / 2. */
static inline uint32_t i9_passo_est(int larg) { return (((uint32_t)larg + 63u) & ~63u) * 2u; }
static inline uint32_t i9_alt_est(int alt) { return ((uint32_t)alt + 63u) & ~63u; }

#endif
