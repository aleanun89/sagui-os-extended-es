// SPDX-License-Identifier: GPL-2.0-or-later
/* r500_pacotes.h - las órdenes 3D de las Radeon R5xx (RV515, R520, RV530, RV560,
 * RV570, R580) para el procesador de órdenes (CP): estado de la tubería (VAP, RS,
 * US, FG, RB3D, ZB), el programa de vértices y los dos programas de fragmentos
 * escritos a mano, y los triángulos (D287).
 *
 * Solo construye palabras en una cola (g3d_fila_t): no toca hardware ni usa nada
 * del kernel, y por eso se prueba en el PC (tests/r500_3d_unidad.c). Quien las
 * manda a la tarjeta es kernel/dispositivos/gpu3d_r500.c.
 *
 * La tubería tal como se usa aquí:
 *   - el vértice llega ya en coordenadas del destino (gpu3d_vert_t, 8 palabras:
 *     x, y, z, 1/w, color, especular, u, v), dentro de la propia orden de dibujo
 *     (3D_DRAW_IMMD_2): no hay búferes de vértices;
 *   - VAP: el flujo se describe con PROG_STREAM_CNTL (4 flotantes, 2 colores de 4
 *     bytes en orden D3D y 2 flotantes). El programa de vértices (PVS) solo copia
 *     las cuatro entradas a las cuatro salidas; VTE_CNTL dice que x, y, z ya
 *     vienen divididos por w y que la cuarta coordenada es 1/w (con ella el
 *     rasterizador corrige la perspectiva de colores y texturas). Sin recorte;
 *   - RS: el color va al registro temporal 0 del programa de fragmentos, el
 *     especular al 1 y las coordenadas de textura al 2;
 *   - US (programa de fragmentos de R500, 6 palabras por instrucción):
 *       sin textura:  salida = t0 * 1  + t1          (una instrucción)
 *       con textura:  t2 = textura(t2); salida = t0 * t2 + t1   (dos)
 *     con el alfa = alfa del color (por el de la textura si la hay);
 *   - Z, mezcla y recorte por tijera según gpu3d_estado_t; destino ARGB8888 y Z
 *     de 16 bits o de 24 (+8 de plantilla sin usar).
 *
 * Referencias (valores de registros y orden de las secuencias; el código es
 * propio, ver docs/terceros.md): r300_reg.h de Mesa (MIT), radeon_reg.h,
 * radeon_commonfuncs.c y radeon_exa_render.c de xf86-video-ati 6.14 (MIT), y
 * r100.c / rv515.c del DRM de Linux (ficheros con licencia MIT). */
#ifndef SAGUI_R500_PACOTES_H
#define SAGUI_R500_PACOTES_H
#include "gpu3d.h"

/* ------------------------------------------------------------ órdenes del CP */

#define R5_PACOTE0(reg, n)     (((uint32_t)(n) << 16) | ((uint32_t)(reg) >> 2))   /* n + 1 registros seguidos */
#define R5_PACOTE2             0x80000000u                                         /* no hace nada (relleno) */
#define R5_P3_DRAW_IMMD_2      0xC0003500u   /* | (palabras del cuerpo - 1) << 16 */
#define R5_PRIM_TRIANGULOS     4u
#define R5_PRIM_NO_ANEL        (3u << 4)     /* los vértices van en la orden */
#define R5_VERT_PALAVRAS       8u            /* gpu3d_vert_t */
#define R5_VERT_POR_PACOTE     510           /* múltiplo de 3; 4080 palabras (el tope de una orden es 16380) */

/* ------------------------------------------------------------ registros */

#define R5_WAIT_UNTIL          0x1720
#define   R5_WAIT_2D_LIMPO     (1u << 16)
#define   R5_WAIT_3D_LIMPO     (1u << 17)
#define R5_ISYNC_CNTL          0x1724
#define R5_HOST_PATH_CNTL      0x0130        /* D387 (Amilo): el HDP, la ventana de la CPU a la VRAM */
#define   R5_HDP_LEITURA_INVALIDAR (1u << 27)
#define   R5_ISYNC_PADRAO      0x33u         /* 2D espera al 3D, 3D al 2D, y los apuntes al motor parado */
#define R5_DST_PIPE_CONFIG     0x170C
#define   R5_PIPE_AUTO         (1u << 31)
#define R5_SCRATCH_REG0        0x15E0

#define R5_VAP_CNTL            0x2080
#define R5_VAP_VF_CNTL         0x2084
#define R5_VAP_INDEX_OFFSET    0x208C
#define R5_VAP_OUT_VTX_FMT_0   0x2090
#define R5_VAP_OUT_VTX_FMT_1   0x2094
#define R5_VAP_VTE_CNTL        0x20B0
#define   R5_VTE_XY_DIVIDIDO   (1u << 8)
#define   R5_VTE_Z_DIVIDIDO    (1u << 9)
#define R5_VAP_VTX_SIZE        0x20B4
#define R5_VAP_CNTL_STATUS     0x2140
#define R5_VAP_PSC_0           0x2150
#define R5_VAP_PSC_1           0x2154
#define   R5_PSC_FLOAT_2       1u
#define   R5_PSC_FLOAT_4       3u
#define   R5_PSC_D3DCOLOR      5u
#define   R5_PSC_DESTINO(n)    ((uint32_t)(n) << 8)
#define   R5_PSC_ULTIMO        (1u << 13)
#define   R5_PSC_COM_SINAL     (1u << 14)
#define   R5_PSC_NORMALIZAR    (1u << 15)
#define R5_VAP_VTX_STATE_CNTL  0x2180
#define R5_VAP_VSM_VTX_ASSM    0x2184
#define R5_VAP_PSC_SGN_NORM    0x21DC
#define R5_VAP_PSC_EXT_0       0x21E0
#define R5_VAP_PSC_EXT_1       0x21E4
#define   R5_SWZ_XYZW          0xF688u       /* x, y, z, w y las cuatro escrituras */
#define   R5_SWZ_XY01          0xFB08u       /* x, y, 0.0, 1.0 */
#define R5_VAP_PVS_INDICE      0x2200
#define R5_VAP_PVS_DADOS       0x2204
#define R5_VAP_CLIP_CNTL       0x221C
#define   R5_CLIP_DESLIGADO    (1u << 16)
#define R5_VAP_GB_VERT_CLIP    0x2220        /* y 0x2224, 0x2228, 0x222C */
#define R5_VAP_PVS_FLUSH       0x2284
#define R5_VAP_PVS_CODE_CNTL_0 0x22D0
#define R5_VAP_PVS_CONST_CNTL  0x22D4
#define R5_VAP_PVS_CODE_CNTL_1 0x22D8
#define R5_VAP_PVS_FLOW_CNTL   0x22DC

#define R5_GB_ENABLE           0x4008
#define R5_GB_MSPOS0           0x4010
#define R5_GB_MSPOS1           0x4014
#define R5_GB_TILE_CONFIG      0x4018
#define R5_GB_SELECT           0x401C
#define R5_GB_AA_CONFIG        0x4020
#define R5_GB_Z_PEQ_CONFIG     0x4028
#define R5_GB_PIPE_SELECT      0x402C
#define R5_RS_IP_0             0x4074
#define   R5_RS_K0             62u           /* la constante 0.0 */
#define   R5_RS_K1             63u           /* la constante 1.0 */
#define R5_TX_INVALTAGS        0x4100
#define R5_TX_ENABLE           0x4104
#define R5_GA_POINT_SIZE       0x421C
#define R5_GA_LINE_CNTL        0x4234
#define R5_GA_US_INDICE        0x4250
#define R5_GA_US_DADOS         0x4254
#define R5_GA_ENHANCE          0x4274
#define R5_GA_COLOR_CONTROL    0x4278
#define R5_GA_POLY_MODE        0x4288
#define R5_GA_ROUND_MODE       0x428C
#define R5_GA_OFFSET           0x4290
#define R5_SU_TEX_WRAP         0x42A0
#define R5_SU_POLY_OFFSET_EN   0x42B4
#define R5_SU_CULL_MODE        0x42B8
#define R5_SU_DEPTH_SCALE      0x42C0
#define R5_SU_DEPTH_OFFSET     0x42C4
#define R5_SU_REG_DEST         0x42C8
#define R5_RS_COUNT            0x4300
#define   R5_RS_ALTA_RES       (1u << 18)
#define R5_RS_INST_COUNT       0x4304
#define R5_RS_INST_0           0x4320
#define   R5_RS_TEX_ESCREVE    (1u << 4)
#define   R5_RS_COR_ESCREVE    (1u << 16)
#define R5_SC_HYPERZ           0x43A4
#define R5_SC_EDGERULE         0x43A8
#define R5_SC_CLIP_0_A         0x43B0
#define R5_SC_CLIP_0_B         0x43B4
#define R5_SC_CLIP_RULE        0x43D0
#define R5_SC_SCISSOR0         0x43E0
#define R5_SC_SCISSOR1         0x43E4
#define R5_SC_SCREENDOOR       0x43E8
#define R5_TX_FILTER0_0        0x4400
#define   R5_TX_BORDE_S        (2u << 0)     /* sujetar al último texel (si no, repetir) */
#define   R5_TX_BORDE_T        (2u << 3)
#define   R5_TX_MAG_PROXIMO    (1u << 9)
#define   R5_TX_MAG_LINEAR     (2u << 9)
#define   R5_TX_MIN_PROXIMO    (1u << 11)
#define   R5_TX_MIN_LINEAR     (2u << 11)
#define R5_TX_FILTER1_0        0x4440
#define R5_TX_FORMAT0_0        0x4480
#define   R5_TX_PASSO_PROPRIO  (1u << 31)    /* las direcciones se calculan con FORMAT2 (el paso), no con el ancho */
#define R5_TX_FORMAT1_0        0x44C0
#define   R5_TX_ARGB8888       0xA60Cu       /* W8Z8Y8X8 con B = X, G = Y, R = Z, A = W */
#define R5_TX_FORMAT2_0        0x4500
#define R5_TX_OFFSET_0         0x4540
#define R5_TX_BORDER_COLOR_0   0x45C0
#define R5_US_CONFIG           0x4600
#define   R5_US_ZERO_POR_ALGO  (1u << 1)     /* 0 * x = 0 aunque x sea infinito o NaN */
#define R5_US_PIXSIZE          0x4604
#define R5_US_FC_CTRL          0x4624
#define R5_US_CODE_ADDR        0x4630
#define R5_US_CODE_RANGE       0x4634
#define R5_US_CODE_OFFSET      0x4638
#define R5_US_FORMAT0_0        0x4640
#define R5_US_OUT_FMT_0        0x46A4
#define   R5_US_SAIDA_ARGB     0x1B00u       /* 4 canales de 8 bits: C0 = azul, C1 = verde, C2 = rojo, C3 = alfa */
#define   R5_US_SAIDA_NADA     0x1B0Fu
#define R5_US_W_FMT            0x46B4
#define R5_FG_FOG_BLEND        0x4BC0
#define R5_FG_ALPHA_FUNC       0x4BD4
#define R5_FG_DEPTH_SRC        0x4BD8
#define R5_RB3D_CCTL           0x4E00
#define R5_RB3D_CBLEND         0x4E04
#define   R5_BLEND_LIGADO      (1u << 0)
#define   R5_BLEND_LER_DESTINO (1u << 2)
#define R5_RB3D_ABLEND         0x4E08
#define R5_RB3D_MASCARA        0x4E0C
#define R5_RB3D_ROPCNTL        0x4E18
#define R5_RB3D_COLOROFFSET0   0x4E28
#define R5_RB3D_COLORPITCH0    0x4E38
#define   R5_COR_ARGB8888      (6u << 21)
#define R5_RB3D_DSTCACHE       0x4E4C
#define   R5_DC_VACIAR         0x0Au         /* vaciar y soltar la caché de color del 3D */
#define R5_RB3D_DITHER_CTL     0x4E50
#define R5_RB3D_AARESOLVE_CTL  0x4E88
#define R5_ZB_CNTL             0x4F00
#define   R5_Z_LIGADO          (1u << 1)
#define   R5_Z_ESCREVE         (1u << 2)
#define R5_ZB_ZSTENCILCNTL     0x4F04
#define   R5_ZF_MENOR          1u
#define   R5_ZF_MENOR_IGUAL    2u
#define   R5_ZF_SEMPRE         7u
#define R5_ZB_STENCILREFMASK   0x4F08
#define R5_ZB_FORMAT           0x4F10
#define   R5_ZFMT_16           0u
#define   R5_ZFMT_24_8         2u
#define R5_ZB_ZTOP             0x4F14
#define R5_ZB_ZCACHE           0x4F18
#define   R5_ZC_VACIAR         0x03u
#define R5_ZB_BW_CNTL          0x4F1C
#define R5_ZB_DEPTHOFFSET      0x4F20
#define R5_ZB_DEPTHPITCH       0x4F24
#define R5_ZB_DEPTHCLEARVALUE  0x4F28

/* ------------------------------------------------------------ programa de vértices */

/* Una instrucción del PVS son cuatro palabras: operación y destino, y tres orígenes. */
#define R5_PVS_SOMA_SAIDA(n)   (3u | (2u << 8) | ((uint32_t)(n) << 13) | (0xFu << 20))   /* ADD -> salida n, xyzw */
#define R5_PVS_ENTRADA(n)      (1u | ((uint32_t)(n) << 5) | (0u << 13) | (1u << 16) | (2u << 19) | (3u << 22))   /* entrada n .xyzw */
#define R5_PVS_ENTRADA_XY01(n) (1u | ((uint32_t)(n) << 5) | (0u << 13) | (1u << 16) | (4u << 19) | (5u << 22))   /* entrada n .xy01 */
/* El cero de los orígenes 1 y 2: la MISMA entrada n con las cuatro componentes a 0, como hace
 * Mesa (r3xx_vertprog.c, __CONST(0, RC_SWIZZLE_ZERO)). D386: en la X1400 del Amilo, con el cero
 * leído de la entrada 0 (otra entrada), el PVS leía la entrada 0 también en el origen 0 y los
 * colores salían con la posición (degradado x/ancho, y/alto); con la misma entrada, bien. */
#define R5_PVS_ZERO(n)         (1u | ((uint32_t)(n) << 5) | (4u << 13) | (4u << 16) | (4u << 19) | (4u << 22))   /* entrada n .0000 */
#define R5_PVS_INSTRUCOES      4

/* ------------------------------------------------------------ programa de fragmentos */

/* Palabra 0 de una instrucción del US. */
#define R5_FP_TIPO_SAIDA       (1u << 0)
#define R5_FP_TIPO_TEX         (3u << 0)
#define R5_FP_ESPERA_TEX       (1u << 2)     /* espera a que llegue el texel pedido */
#define R5_FP_ULTIMA           (1u << 8)
#define R5_FP_ESCREVE_RGBA     (0xFu << 11)  /* al registro temporal */
#define R5_FP_SAIDA_RGBA       (0xFu << 15)  /* al destino de color */
#define R5_FP_SATURAR          (3u << 19)    /* rgb y alfa a 0..1 */
/* Palabra 1 de una instrucción de textura. */
#define R5_FP_TEX_UNIDADE(n)   ((uint32_t)(n) << 16)
#define R5_FP_TEX_LER          (1u << 22)
#define R5_FP_TEX_PEDIR        (1u << 25)
#define R5_FP_TEX_SO_CUBIERTOS (1u << 26)
/* Palabra 2 de una instrucción de textura: origen (s = r, t = g) y destino (rgba). */
#define R5_FP_TEX_END(orig, dest) ((uint32_t)(orig) | (1u << 10) | ((uint32_t)(dest) << 16) | (1u << 26) | (2u << 28) | (3u << 30))
/* Palabras 1 y 2 de una instrucción aritmética: los tres registros de origen. */
#define R5_FP_ORIGENS(a, b, c) ((uint32_t)(a) | ((uint32_t)(b) << 10) | ((uint32_t)(c) << 20))
/* Palabra 3: A = origen 0 (rgb), B = origen 1 (rgb o 1,1,1). */
#define R5_FP_RGB_A_RGB        ((0u << 2) | (1u << 5) | (2u << 8))
#define R5_FP_RGB_B_ORIGEM1    (1u << 13)
#define R5_FP_RGB_B_RGB        ((0u << 15) | (1u << 18) | (2u << 21))
#define R5_FP_RGB_B_UM         ((6u << 15) | (6u << 18) | (6u << 21))
/* Palabra 4: alfa = A * B + C (MAD = 0), A = origen 0 .a, B = origen 1 .a o 1. */
#define R5_FP_ALFA_A_A         (3u << 14)
#define R5_FP_ALFA_B_ORIGEM1   (1u << 19)
#define R5_FP_ALFA_B_A         (3u << 21)
#define R5_FP_ALFA_B_UM        (6u << 21)
/* Palabra 5: rgb = A * B + C (MAD = 0), C = origen 2 (rgb); en el alfa C = 0. */
#define R5_FP_C_ORIGEM2_RGB    ((2u << 12) | (0u << 14) | (1u << 17) | (2u << 20))
#define R5_FP_C_ALFA_ZERO      (4u << 27)

#define R5_FP_PAL_INST         6
#define R5_T_COR               0             /* registros temporales que rellena el RS */
#define R5_T_ESPEC             1
#define R5_T_TEX               2

/* Flags internos (además de los G3D_* de abi.h) para el borrado. */
#define R5_INT_SEM_COR         0x4000u       /* no escribir el color */
#define R5_INT_Z_FORCAR        0x8000u       /* escribir el Z sin compararlo */

typedef struct {
    uint32_t cor_gpu, z_gpu;                 /* direcciones en el mapa de memoria de la tarjeta */
    uint32_t passo_px;                       /* píxeles por fila (color y Z) */
    int      larg, alt, zbits;               /* zbits: 16 o 24 */
} r5_alvo_t;

typedef struct {
    uint32_t gpu;
    uint32_t passo_px;                       /* texels por fila */
    int      larg, alt;
    uint32_t formato1;                       /* D426: TX_FORMAT1 (0: ARGB8888, el de v1) */
} r5_tex_t;

static inline void r5_reg(g3d_fila_t *f, uint32_t reg, uint32_t v)
{
    g3d_por(f, R5_PACOTE0(reg, 0));
    g3d_por(f, v);
}

/* Vaciar las cachés de color y de Z del 3D y esperar a que el motor quede parado y limpio. */
static inline void r5_vaciar(g3d_fila_t *f)
{
    r5_reg(f, R5_RB3D_DSTCACHE, R5_DC_VACIAR);
    r5_reg(f, R5_ZB_ZCACHE, R5_ZC_VACIAR);
    r5_reg(f, R5_WAIT_UNTIL, R5_WAIT_2D_LIMPO | R5_WAIT_3D_LIMPO);
}

/* La valla: cuando todo lo anterior ha terminado, el CP escribe «seq» en un registro de apuntes.
 * Por defecto (banderas 0), la de siempre: vaciar las cachés de color y de Z del 3D, WAIT_UNTIL 2D y 3D
 * parados y limpios, y el apunte (con ISYNC_CPSCRATCH_IDLEGUI, el CP lo escribe con el motor parado).
 * D387 (Amilo, 2026-11-19): cada añadido de la valla de Linux (r300_fence_ring_emit, drivers/gpu/drm/radeon/
 * r300.c, 6.6) es una bandera, para quitarlo o ponerlo sin reiniciar: con la tijera y el HDP el CP se paraba
 * ANTES (19.494 cuadros sin nada, 1.986 con la tijera, 971 con la tijera y el HDP).
 *   R5V_TIJERA  delante, la tijera del rasterizador (SC_SCISSOR0/1: «Write SC register so SC & US assert idle»),
 *               con la del destino actual («tijera1»; Linux escribe 0, pero aquí la valla puede caer entre dos
 *               trozos de un dibujo)
 *   R5V_LINUX   las cachés con los valores de Linux (RB3D_DC_FLUSH 2, ZC_FLUSH 1, sin «soltar») y WAIT_UNTIL
 *               también con WAIT_DMA_GUI_IDLE (bit 9)
 *   R5V_SEM_ESPERA  sin el WAIT_UNTIL (queda la espera del apunte por ISYNC_CPSCRATCH_IDLEGUI)
 *   R5V_HDP     detrás de la espera, invalidar el búfer de lectura del HDP (HOST_PATH_CNTL con el bit 27 y otra
 *               vez sin él, como r100_ring_hdp_flush; «hdp», el valor que tenía al encender) */
#define R5V_TIJERA      0x1u
#define R5V_LINUX       0x2u
#define R5V_SEM_ESPERA  0x4u
#define R5V_HDP         0x8u
#define R5_WAIT_DMA_GUI (1u << 9)
static inline void r5_valla(g3d_fila_t *f, uint32_t apunte, uint32_t seq, uint32_t banderas, uint32_t tijera1, uint32_t hdp)
{
    if (banderas & R5V_TIJERA) {
        r5_reg(f, R5_SC_SCISSOR0, 0);
        r5_reg(f, R5_SC_SCISSOR1, tijera1);
    }
    r5_reg(f, R5_RB3D_DSTCACHE, (banderas & R5V_LINUX) ? 0x2u : R5_DC_VACIAR);
    r5_reg(f, R5_ZB_ZCACHE, (banderas & R5V_LINUX) ? 0x1u : R5_ZC_VACIAR);
    if (!(banderas & R5V_SEM_ESPERA))
        r5_reg(f, R5_WAIT_UNTIL, R5_WAIT_2D_LIMPO | R5_WAIT_3D_LIMPO | ((banderas & R5V_LINUX) ? R5_WAIT_DMA_GUI : 0));
    if (banderas & R5V_HDP) {
        r5_reg(f, R5_HOST_PATH_CNTL, hdp | R5_HDP_LEITURA_INVALIDAR);
        r5_reg(f, R5_HOST_PATH_CNTL, hdp);
    }
    r5_reg(f, apunte, seq);
}

/* La tijera de un destino (SC_SCISSOR1): la esquina de abajo a la derecha. */
static inline uint32_t r5_tijera1(int larg, int alt)
{
    return larg > 0 && alt > 0 ? (uint32_t)(larg - 1) | ((uint32_t)(alt - 1) << 13) : 0;
}

/* VAP_CNTL: tamaño de las memorias del procesador de vértices, por familia (fpus: 2 la RV515,
 * 5 la RV530, 8 las R520, RV560, RV570 y R580). */
static inline uint32_t r5_vap_cntl(uint32_t fpus)
{
    return 5u | (5u << 4) | (fpus << 8) | (9u << 18);
}

static inline uint32_t r5_tile_config(uint32_t tuberias)
{
    static const uint32_t cod[5] = { 0, 0, 3, 6, 7 };
    return 1u | (1u << 4) | (cod[tuberias > 4 ? 4 : tuberias] << 1);   /* mosaico del rasterizador de 16 */
}

/* Lo que no cambia de un dibujo a otro: se manda una vez al encender (unas 230 palabras).
 * tuberias: las de píxeles (GB_PIPE_SELECT); fpus: ver r5_vap_cntl. */
static inline void r5_inicio(g3d_fila_t *f, uint32_t tuberias, uint32_t fpus)
{
    if (tuberias < 1) tuberias = 1;
    if (tuberias > 4) tuberias = 4;
    /* Sincronización entre el 2D y el 3D y reparto de las tuberías de píxeles. */
    r5_reg(f, R5_ISYNC_CNTL, R5_ISYNC_PADRAO);
    r5_vaciar(f);
    r5_reg(f, R5_GB_TILE_CONFIG, r5_tile_config(tuberias));
    r5_reg(f, R5_WAIT_UNTIL, R5_WAIT_2D_LIMPO | R5_WAIT_3D_LIMPO);
    r5_reg(f, R5_DST_PIPE_CONFIG, R5_PIPE_AUTO);
    r5_reg(f, R5_GB_SELECT, 0);
    r5_reg(f, R5_GB_ENABLE, 0);
    r5_reg(f, R5_SU_REG_DEST, (1u << tuberias) - 1);
    r5_reg(f, R5_VAP_INDEX_OFFSET, 0);
    r5_vaciar(f);
    r5_reg(f, R5_GB_AA_CONFIG, 0);
    r5_vaciar(f);
    r5_reg(f, R5_GB_MSPOS0, 0x66666666u);
    r5_reg(f, R5_GB_MSPOS1, 0x06666666u);
    r5_reg(f, R5_GB_Z_PEQ_CONFIG, 0);
    /* Geometría: triángulos rellenos, sombreado de Gouraud, sin descarte de caras. */
    r5_reg(f, R5_GA_ENHANCE, 3u);
    r5_reg(f, R5_GA_POLY_MODE, (2u << 4) | (2u << 7));
    r5_reg(f, R5_GA_ROUND_MODE, 5u);
    r5_reg(f, R5_GA_COLOR_CONTROL, 0xAAAAu);
    r5_reg(f, R5_GA_OFFSET, 0);
    r5_reg(f, R5_GA_POINT_SIZE, 0x00060006u);
    r5_reg(f, R5_GA_LINE_CNTL, 0x00030006u);
    r5_reg(f, R5_SU_TEX_WRAP, 0);
    r5_reg(f, R5_SU_POLY_OFFSET_EN, 0);
    r5_reg(f, R5_SU_CULL_MODE, 0);
    r5_reg(f, R5_SU_DEPTH_SCALE, 0x4B7FFFFFu);
    r5_reg(f, R5_SU_DEPTH_OFFSET, 0);
    /* Procesador de vértices: formato del flujo y programa que copia. */
    r5_reg(f, R5_VAP_VTX_STATE_CNTL, 0);
    r5_reg(f, R5_VAP_VSM_VTX_ASSM, 0);
    r5_reg(f, R5_VAP_PVS_FLUSH, 0);
    r5_reg(f, R5_VAP_CNTL_STATUS, 0);
    r5_reg(f, R5_VAP_CNTL, r5_vap_cntl(fpus));
    r5_reg(f, R5_VAP_PVS_FLUSH, 0);
    r5_reg(f, R5_VAP_VTE_CNTL, R5_VTE_XY_DIVIDIDO | R5_VTE_Z_DIVIDIDO);
    r5_reg(f, R5_VAP_PSC_SGN_NORM, 0);
    r5_reg(f, R5_VAP_PSC_0, (R5_PSC_FLOAT_4 | R5_PSC_COM_SINAL | R5_PSC_DESTINO(0)) |
                            ((R5_PSC_D3DCOLOR | R5_PSC_NORMALIZAR | R5_PSC_DESTINO(1)) << 16));
    r5_reg(f, R5_VAP_PSC_1, (R5_PSC_D3DCOLOR | R5_PSC_NORMALIZAR | R5_PSC_DESTINO(2)) |
                            ((R5_PSC_FLOAT_2 | R5_PSC_COM_SINAL | R5_PSC_DESTINO(3) | R5_PSC_ULTIMO) << 16));
    r5_reg(f, R5_VAP_PSC_EXT_0, R5_SWZ_XYZW | (R5_SWZ_XYZW << 16));
    r5_reg(f, R5_VAP_PSC_EXT_1, R5_SWZ_XYZW | (R5_SWZ_XY01 << 16));
    r5_reg(f, R5_VAP_PVS_FLOW_CNTL, 0);
    for (uint32_t i = 0; i < 4; i++) r5_reg(f, R5_VAP_GB_VERT_CLIP + i * 4, 0x3F800000u);
    r5_reg(f, R5_VAP_CLIP_CNTL, R5_CLIP_DESLIGADO);
    r5_reg(f, R5_VAP_PVS_INDICE, 0);
    for (uint32_t i = 0; i < R5_PVS_INSTRUCOES; i++) {
        r5_reg(f, R5_VAP_PVS_DADOS, R5_PVS_SOMA_SAIDA(i));
        r5_reg(f, R5_VAP_PVS_DADOS, i == 3 ? R5_PVS_ENTRADA_XY01(i) : R5_PVS_ENTRADA(i));
        r5_reg(f, R5_VAP_PVS_DADOS, R5_PVS_ZERO(i));
        r5_reg(f, R5_VAP_PVS_DADOS, R5_PVS_ZERO(i));
    }
    r5_reg(f, R5_VAP_PVS_CONST_CNTL, 0);
    r5_reg(f, R5_VAP_PVS_CODE_CNTL_0, 0u | ((R5_PVS_INSTRUCOES - 1u) << 10) | ((R5_PVS_INSTRUCOES - 1u) << 20));
    r5_reg(f, R5_VAP_PVS_CODE_CNTL_1, R5_PVS_INSTRUCOES - 1u);
    r5_reg(f, R5_VAP_OUT_VTX_FMT_0, 7u);            /* posición y dos colores */
    r5_reg(f, R5_VAP_OUT_VTX_FMT_1, 2u);            /* textura 0 con dos coordenadas */
    r5_reg(f, R5_VAP_VTX_SIZE, R5_VERT_PALAVRAS);
    /* Rasterizador: color -> t0, especular -> t1, (s, t, 0, 1) de la textura 0 -> t2. */
    r5_reg(f, R5_RS_COUNT, 2u | (2u << 7) | R5_RS_ALTA_RES);
    r5_reg(f, R5_RS_INST_COUNT, 1u);
    r5_reg(f, R5_RS_IP_0, 0u | (1u << 6) | (R5_RS_K0 << 12) | (R5_RS_K1 << 18) | (0u << 24));
    r5_reg(f, R5_RS_IP_0 + 4, R5_RS_K0 | (R5_RS_K0 << 6) | (R5_RS_K0 << 12) | (R5_RS_K1 << 18) | (1u << 24));
    r5_reg(f, R5_RS_INST_0, 0u | R5_RS_TEX_ESCREVE | ((uint32_t)R5_T_TEX << 5) | (0u << 12) | R5_RS_COR_ESCREVE |
                            ((uint32_t)R5_T_COR << 18));
    r5_reg(f, R5_RS_INST_0 + 4, (1u << 12) | R5_RS_COR_ESCREVE | ((uint32_t)R5_T_ESPEC << 18));
    /* Fragmentos y salida. */
    r5_reg(f, R5_US_CONFIG, R5_US_ZERO_POR_ALGO);
    r5_reg(f, R5_US_FC_CTRL, 0);
    r5_reg(f, R5_US_W_FMT, 0);
    r5_reg(f, R5_US_PIXSIZE, R5_T_TEX);
    r5_reg(f, R5_US_OUT_FMT_0, R5_US_SAIDA_ARGB);
    for (uint32_t i = 1; i < 4; i++) r5_reg(f, R5_US_OUT_FMT_0 + i * 4, R5_US_SAIDA_NADA);
    r5_reg(f, R5_TX_ENABLE, 0);
    r5_reg(f, R5_FG_DEPTH_SRC, 0);
    r5_reg(f, R5_FG_FOG_BLEND, 0);
    r5_reg(f, R5_FG_ALPHA_FUNC, 0);
    r5_reg(f, R5_RB3D_ABLEND, 0);
    r5_reg(f, R5_RB3D_CBLEND, 0);
    r5_reg(f, R5_ZB_ZSTENCILCNTL, 0);
    r5_reg(f, R5_ZB_STENCILREFMASK, 0);
    r5_reg(f, R5_ZB_ZCACHE, R5_ZC_VACIAR);
    r5_reg(f, R5_ZB_BW_CNTL, 0);
    r5_reg(f, R5_ZB_CNTL, 0);
    r5_reg(f, R5_ZB_ZTOP, 0);
    r5_reg(f, R5_ZB_DEPTHCLEARVALUE, 0);
    r5_reg(f, R5_RB3D_ROPCNTL, 0);
    r5_reg(f, R5_RB3D_AARESOLVE_CTL, 0);
    r5_reg(f, R5_RB3D_MASCARA, 0xFu);
    r5_reg(f, R5_RB3D_DSTCACHE, R5_DC_VACIAR);
    r5_reg(f, R5_RB3D_CCTL, 0);
    r5_reg(f, R5_RB3D_DITHER_CTL, 0);
    /* Tijera y recortes del rasterizador (en R5xx sin el desplazamiento de 1440 de las R300). */
    r5_reg(f, R5_SC_HYPERZ, 0x1Cu);
    r5_reg(f, R5_SC_EDGERULE, 0x0A5294A5u);
    r5_reg(f, R5_SC_CLIP_0_A, 0);
    r5_reg(f, R5_SC_CLIP_0_B, 4080u | (4080u << 13));
    r5_reg(f, R5_SC_CLIP_RULE, 0xAAAAu);
    r5_reg(f, R5_SC_SCREENDOOR, 0x00FFFFFFu);
    r5_vaciar(f);
}

/* El programa de fragmentos en «p» (6 o 12 palabras); devuelve cuántas instrucciones. */
static inline int r5_programa_fp(uint32_t *p, bool com_textura)
{
    int n = 0;
    uint32_t b = com_textura ? R5_T_TEX : R5_T_COR;   /* sin textura B no se lee (es 1) */
    if (com_textura) {
        p[n++] = R5_FP_TIPO_TEX | R5_FP_ESPERA_TEX | R5_FP_ESCREVE_RGBA | R5_FP_SATURAR;
        p[n++] = R5_FP_TEX_UNIDADE(0) | R5_FP_TEX_LER | R5_FP_TEX_PEDIR | R5_FP_TEX_SO_CUBIERTOS;
        p[n++] = R5_FP_TEX_END(R5_T_TEX, R5_T_TEX);
        p[n++] = (uint32_t)R5_T_TEX | ((uint32_t)R5_T_TEX << 16);   /* derivadas: del mismo registro */
        p[n++] = 0;
        p[n++] = 0;
    }
    p[n++] = R5_FP_TIPO_SAIDA | R5_FP_ESPERA_TEX | R5_FP_ULTIMA | R5_FP_SAIDA_RGBA | R5_FP_SATURAR;
    p[n++] = R5_FP_ORIGENS(R5_T_COR, b, R5_T_ESPEC);
    p[n++] = R5_FP_ORIGENS(R5_T_COR, b, R5_T_ESPEC);
    p[n++] = R5_FP_RGB_A_RGB | R5_FP_RGB_B_ORIGEM1 | (com_textura ? R5_FP_RGB_B_RGB : R5_FP_RGB_B_UM);
    p[n++] = R5_FP_ALFA_A_A | R5_FP_ALFA_B_ORIGEM1 | (com_textura ? R5_FP_ALFA_B_A : R5_FP_ALFA_B_UM);
    p[n++] = R5_FP_C_ORIGEM2_RGB | R5_FP_C_ALFA_ZERO;
    return n / R5_FP_PAL_INST;
}

/* El destino: color, Z y tijera a todo el destino. */
static inline void r5_estado_alvo(g3d_fila_t *f, const r5_alvo_t *a)
{
    r5_reg(f, R5_RB3D_COLOROFFSET0, a->cor_gpu);
    r5_reg(f, R5_RB3D_COLORPITCH0, a->passo_px | R5_COR_ARGB8888);
    r5_reg(f, R5_ZB_DEPTHOFFSET, a->z_gpu);
    r5_reg(f, R5_ZB_DEPTHPITCH, a->passo_px);
    r5_reg(f, R5_ZB_FORMAT, a->zbits == 16 ? R5_ZFMT_16 : R5_ZFMT_24_8);
    r5_reg(f, R5_SC_SCISSOR0, 0);
    r5_reg(f, R5_SC_SCISSOR1, r5_tijera1(a->larg, a->alt));
}

/* Z, mezcla, textura y programa de fragmentos de un dibujo. t: la textura o NULL. */
static inline void r5_estado_desenho(g3d_fila_t *f, const gpu3d_estado_t *e, const r5_tex_t *t)
{
    static const uint32_t zf[3] = { R5_ZF_MENOR, R5_ZF_MENOR_IGUAL, R5_ZF_SEMPRE };
    static const uint32_t fator[4] = { 32, 33, 38, 39 };   /* 0, 1, alfa del origen, 1 - alfa del origen */
    uint32_t fl = e->flags;
    /* Z: sin comparación no se escribe (como en OpenGL), salvo en el borrado. */
    uint32_t zc = 0, zfun = R5_ZF_SEMPRE;
    if (fl & G3D_Z_TESTE) {
        zc = R5_Z_LIGADO | (fl & G3D_Z_ESCRITA ? R5_Z_ESCREVE : 0);
        zfun = zf[e->func_z > G3D_Z_SEMPRE ? G3D_Z_SEMPRE : e->func_z];
    } else if (fl & R5_INT_Z_FORCAR)
        zc = R5_Z_LIGADO | R5_Z_ESCREVE;
    r5_reg(f, R5_ZB_CNTL, zc);
    r5_reg(f, R5_ZB_ZSTENCILCNTL, zfun);
    r5_reg(f, R5_RB3D_MASCARA, fl & R5_INT_SEM_COR ? 0 : 0xFu);
    if (fl & G3D_MEZCLA)
        r5_reg(f, R5_RB3D_CBLEND, R5_BLEND_LIGADO | R5_BLEND_LER_DESTINO | (fator[e->mez_orig & 3] << 16) |
                                  (fator[e->mez_dest & 3] << 24));
    else
        r5_reg(f, R5_RB3D_CBLEND, 0);
    if (t) {
        /* Repetir solo vale con lados potencia de 2 (quien llama lo comprueba). */
        uint32_t filtro = (fl & G3D_TEX_REPETE_S ? 0 : R5_TX_BORDE_S) | (fl & G3D_TEX_REPETE_T ? 0 : R5_TX_BORDE_T) |
                          (fl & G3D_TEX_LINEAR ? R5_TX_MAG_LINEAR | R5_TX_MIN_LINEAR : R5_TX_MAG_PROXIMO | R5_TX_MIN_PROXIMO);
        uint32_t lados = ((uint32_t)(t->larg - 1) & 0x7FFu) | (((uint32_t)(t->alt - 1) & 0x7FFu) << 11);
        r5_reg(f, R5_TX_FILTER0_0, filtro);
        r5_reg(f, R5_TX_FILTER1_0, 0);
        r5_reg(f, R5_TX_FORMAT0_0, lados | R5_TX_PASSO_PROPRIO);
        r5_reg(f, R5_TX_FORMAT1_0, R5_TX_ARGB8888);
        r5_reg(f, R5_TX_FORMAT2_0, t->passo_px - 1);
        r5_reg(f, R5_TX_OFFSET_0, t->gpu);
        r5_reg(f, R5_TX_BORDER_COLOR_0, 0);
        r5_reg(f, R5_US_FORMAT0_0, lados);
        r5_reg(f, R5_TX_INVALTAGS, 0);
    }
    r5_reg(f, R5_TX_ENABLE, t ? 1u : 0);
    uint32_t p[2 * R5_FP_PAL_INST];
    int ni = r5_programa_fp(p, t != NULL);
    r5_reg(f, R5_US_CODE_ADDR, 0u | ((uint32_t)(ni - 1) << 16));
    r5_reg(f, R5_US_CODE_RANGE, 0u | ((uint32_t)(ni - 1) << 16));
    r5_reg(f, R5_US_CODE_OFFSET, 0);
    r5_reg(f, R5_GA_US_INDICE, 0);
    for (int i = 0; i < ni * R5_FP_PAL_INST; i++) r5_reg(f, R5_GA_US_DADOS, p[i]);
}

/* Palabras que ocupa r5_triangulos con n vértices. */
static inline uint32_t r5_pal_triangulos(int n) { return 2u + (uint32_t)n * R5_VERT_PALAVRAS; }

/* n vértices (múltiplo de 3, como mucho R5_VERT_POR_PACOTE) dentro de la orden. */
static inline void r5_triangulos(g3d_fila_t *f, const gpu3d_vert_t *v, int n)
{
    g3d_por(f, R5_P3_DRAW_IMMD_2 | (((uint32_t)n * R5_VERT_PALAVRAS) << 16));
    g3d_por(f, R5_PRIM_TRIANGULOS | R5_PRIM_NO_ANEL | ((uint32_t)n << 16));
    for (int i = 0; i < n; i++) {
        g3d_por(f, v[i].x); g3d_por(f, v[i].y); g3d_por(f, v[i].z); g3d_por(f, v[i].rhw);
        g3d_por(f, v[i].cor); g3d_por(f, v[i].espec);
        g3d_por(f, v[i].u); g3d_por(f, v[i].v);
    }
}

/* ------------------------------------------------------------ ajustes de depuración (D386, D390) */

/* Los registros que /sistema/conf/gpu3d-r500.conf puede tocar (la capa lee el fichero,
 * gpu3d_diag.h): solo los del 3D, 0x2000-0x4FFC (VAP, GA, SU, RS, SC, US, FG, RB3D, ZB):
 * nunca la pantalla, la memoria ni los relojes. */
/* D426 (Amilo): «inicio 0x4FF0 N» no es un registro: son banderas de depuración del driver (no se escriben). */
#define R5_REG_DEPURAR 0x4FF0u
#define R5_N_FAIXAS_AJUSTES 1
static const g3d_faixa_t r5_faixas_ajustes[R5_N_FAIXAS_AJUSTES] = { { 0x2000u, 0x4FFCu } };

static inline void r5_ajustes_por(g3d_fila_t *f, const g3d_ajustes_t *a, int quando)
{
    if (!a) return;
    for (int i = 0; i < a->n; i++)
        if (a->quando[i] == quando && a->reg[i] != R5_REG_DEPURAR) r5_reg(f, a->reg[i], a->val[i]);
}

/* ============================================================ gpu3d v2 (D426) */

/* Lo de v2 que cambia con el dibujo: el formato del vértice (cuántos juegos de coordenadas), las
 * unidades de textura, el programa de fragmentos generado de la combinación (gpu3d_unidade_t) y el
 * estado por fragmento (esténcil, prueba alfa, tijera, máscaras, mezcla separada, operación lógica).
 *
 * Vértice v2 tal como va en la orden de dibujo (lo empaqueta el driver desde gpu3d_vert2_t):
 * x, y, z, 1/w, color, especular y (s, t) de cada juego de coordenadas que se usa: 6 + 2 * ntc palabras.
 * Con ntc = 1 es exactamente el vértice de v1 (y el formato que deja r5_inicio).
 *
 * Registros temporales del programa de fragmentos v2: */
#define R5V_T_COR       0u            /* color del vértice (RS) */
#define R5V_T_ESPEC     1u            /* especular; su alfa es el factor de niebla */
#define R5V_T_COORD(i)  (2u + (i))    /* coordenadas del juego i (RS) */
#define R5V_T_TEX(i)    (6u + (i))    /* texel de la unidad i */
#define R5V_T_ANT(k)    (10u + (k))   /* resultado de la última unidad (dos, alternando: ver r5_fp_gerar) */
#define R5V_T_AUX(k)    (12u + (k))   /* operandos «1 - x» y pasos intermedios (12 a 17) */
#define R5V_T_MAX       17u
#define R5V_CONST       0x100u        /* bandera de «constante» en una dirección de origen */
#define R5V_C_UNID(i)   (R5V_CONST | (i))   /* GL_TEXTURE_ENV_COLOR de la unidad i */
#define R5V_C_NEBLINA   (R5V_CONST | 4u)
#define R5V_NADA        0xFFFFu       /* el argumento es una constante de la mezcla (0, 1/2, 1): no lee ningún origen */
#define R5V_MAX_INST    64            /* el US de las R5xx guarda 512; con 4 unidades no se pasa de ~40 */

/* Mezcla de cada instrucción: componente (0 r, 1 g, 2 b, 3 a, 4 cero, 5 medio, 6 uno). */
#define R5S_R 0u
#define R5S_G 1u
#define R5S_B 2u
#define R5S_A 3u
#define R5S_0 4u
#define R5S_H 5u
#define R5S_1 6u

/* Un argumento de media instrucción (la de color o la de alfa): de qué registro, con qué mezcla y si va negado.
 * En la media de alfa solo cuenta swz[0]. */
typedef struct { uint16_t reg; uint8_t swz[3]; uint8_t neg; } r5_arg_t;
/* Media instrucción: res = A * B + C (MAD) o, en el color, A·B de tres componentes (DP3; en el alfa, DP: el
 * mismo producto). omod: 0 nada, 1 x2, 2 x4. */
#define R5H_MAD 0u
#define R5H_DP  1u                    /* R500_ALU_RGBA_OP_DP3 en el color, R500_ALPHA_OP_DP en el alfa */
typedef struct { bool usada; uint8_t op, dest, omod; bool sat; r5_arg_t a, b, c; } r5_meia_t;

static inline r5_arg_t r5a_rgb(uint16_t reg)   { r5_arg_t a = { reg, { R5S_R, R5S_G, R5S_B }, 0 }; return a; }
static inline r5_arg_t r5a_aaa(uint16_t reg)   { r5_arg_t a = { reg, { R5S_A, R5S_A, R5S_A }, 0 }; return a; }
static inline r5_arg_t r5a_k(uint8_t s)        { r5_arg_t a = { R5V_NADA, { s, s, s }, 0 }; return a; }
static inline r5_arg_t r5a_neg(r5_arg_t a)     { a.neg ^= 1u; return a; }

/* Un programa generado: palabras (6 por instrucción), cuántas instrucciones, constantes que usa. */
typedef struct {
    uint32_t p[R5V_MAX_INST * R5_FP_PAL_INST];
    int      n;                       /* instrucciones */
    uint32_t consts;                  /* bit i: usa la constante i (0-3 unidades, 4 niebla) */
    int      max_temp;                /* para US_PIXSIZE */
    bool     erro;                    /* no cabe o algo imposible: el dibujo va por software */
    int      ntc;                     /* juegos de coordenadas que lleva el vértice (1 a 4) */
    uint8_t  mapa[4];                 /* juego del vértice v2 (gpu3d_vert2_t.tc) de cada posición */
} r5_fp_t;

/* Los orígenes de una media instrucción (hasta tres registros distintos) y la palabra de direcciones. */
static inline bool r5_meia_origens(const r5_meia_t *m, uint16_t o[3], int *n_o, uint8_t sel[3])
{
    const r5_arg_t *as[3] = { &m->a, &m->b, &m->c };
    *n_o = 0;
    for (int k = 0; k < 3; k++) {
        sel[k] = 0;
        if (!m->usada || as[k]->reg == R5V_NADA) continue;
        int j = 0;
        while (j < *n_o && o[j] != as[k]->reg) j++;
        if (j == *n_o) { if (*n_o == 3) return false; o[(*n_o)++] = as[k]->reg; }
        sel[k] = (uint8_t)j;
    }
    return true;
}

static inline uint32_t r5_end_origens(const uint16_t o[3], int n_o)
{
    uint32_t w = 0;
    for (int k = 0; k < 3; k++) {
        uint16_t r = k < n_o ? o[k] : (n_o ? o[0] : 0);   /* sin usar: el mismo que el primero */
        w |= ((uint32_t)r & 0x1FFu) << (10 * k);           /* dirección (bits 0-7) y constante (bit 8) */
    }
    return w;
}

static inline uint32_t r5_bits_arg_rgb(const r5_arg_t *a, uint8_t sel)
{
    return (uint32_t)sel | ((uint32_t)a->swz[0] << 2) | ((uint32_t)a->swz[1] << 5) | ((uint32_t)a->swz[2] << 8) |
           ((uint32_t)(a->neg & 1u) << 11);
}

static inline uint32_t r5_bits_arg_alfa(const r5_arg_t *a, uint8_t sel)
{
    return (uint32_t)sel | ((uint32_t)a->swz[0] << 2) | ((uint32_t)(a->neg & 1u) << 5);
}

/* Junta una media de color y una de alfa en una instrucción del US. saida: escribe el color del destino
 * (y es la última). */
static inline bool r5_fp_emitir(r5_fp_t *fp, const r5_meia_t *rgb, const r5_meia_t *alfa, bool saida)
{
    if (fp->n >= R5V_MAX_INST) { fp->erro = true; return false; }
    uint16_t orc[3], ora[3];
    uint8_t sc[3], sa[3];
    int nc, na;
    if (!r5_meia_origens(rgb, orc, &nc, sc) || !r5_meia_origens(alfa, ora, &na, sa)) { fp->erro = true; return false; }
    uint32_t *w = fp->p + fp->n * R5_FP_PAL_INST;
    uint32_t i0 = (saida ? R5_FP_TIPO_SAIDA : 0u) | R5_FP_ESPERA_TEX;
    if (saida) i0 |= R5_FP_ULTIMA | R5_FP_SAIDA_RGBA;
    else i0 |= (rgb->usada ? (7u << 11) : 0u) | (alfa->usada ? (1u << 14) : 0u);
    if (rgb->usada && rgb->sat) i0 |= 1u << 19;
    if (alfa->usada && alfa->sat) i0 |= 1u << 20;
    w[0] = i0;
    w[1] = r5_end_origens(orc, nc);
    w[2] = r5_end_origens(ora, na);
    w[3] = r5_bits_arg_rgb(&rgb->a, sc[0]) | (r5_bits_arg_rgb(&rgb->b, sc[1]) << 13) | ((uint32_t)(rgb->omod & 7u) << 26);
    w[4] = (uint32_t)(alfa->op & 0xFu) | ((uint32_t)(alfa->dest & 0x7Fu) << 4) | (r5_bits_arg_alfa(&alfa->a, sa[0]) << 12) |
           (r5_bits_arg_alfa(&alfa->b, sa[1]) << 19) | ((uint32_t)(alfa->omod & 7u) << 26);
    w[5] = (uint32_t)(rgb->op & 0xFu) | ((uint32_t)(rgb->dest & 0x7Fu) << 4) | (r5_bits_arg_rgb(&rgb->c, sc[2]) << 12) |
           (r5_bits_arg_alfa(&alfa->c, sa[2]) << 25);
    for (int k = 0; k < nc; k++) if (!(orc[k] & R5V_CONST) && (int)orc[k] > fp->max_temp) fp->max_temp = orc[k];
    for (int k = 0; k < na; k++) if (!(ora[k] & R5V_CONST) && (int)ora[k] > fp->max_temp) fp->max_temp = ora[k];
    if (rgb->usada && !saida && rgb->dest > fp->max_temp) fp->max_temp = rgb->dest;
    if (alfa->usada && !saida && alfa->dest > fp->max_temp) fp->max_temp = alfa->dest;
    fp->n++;
    return true;
}

/* Lectura de la textura de la unidad u con las coordenadas del juego «coord» al temporal R5V_T_TEX(u). */
static inline void r5_fp_tex(r5_fp_t *fp, uint32_t u, uint32_t coord)
{
    if (fp->n >= R5V_MAX_INST) { fp->erro = true; return; }
    uint32_t *w = fp->p + fp->n * R5_FP_PAL_INST, o = R5V_T_COORD(coord), d = R5V_T_TEX(u);
    w[0] = R5_FP_TIPO_TEX | R5_FP_ESPERA_TEX | R5_FP_ESCREVE_RGBA;
    w[1] = R5_FP_TEX_UNIDADE(u) | R5_FP_TEX_LER | R5_FP_TEX_PEDIR | R5_FP_TEX_SO_CUBIERTOS;
    w[2] = R5_FP_TEX_END(o, d);
    w[3] = o | (o << 16);
    w[4] = w[5] = 0;
    if ((int)d > fp->max_temp) fp->max_temp = (int)d;
    if ((int)o > fp->max_temp) fp->max_temp = (int)o;
    fp->n++;
}

/* Lista de medias instrucciones de un canal (color o alfa) de una unidad. */
typedef struct { r5_meia_t m[8]; int n; } r5_lista_t;

static inline r5_meia_t *r5_l_nova(r5_lista_t *l)
{
    static r5_meia_t lixo;
    if (l->n >= 8) return &lixo;
    r5_meia_t *m = &l->m[l->n++];
    *m = (r5_meia_t){ true, R5H_MAD, 0, 0, false, r5a_k(R5S_0), r5a_k(R5S_1), r5a_k(R5S_0) };
    return m;
}

/* El registro de un origen G3D_FT_* de la unidad u (ant: el resultado de la anterior). */
static inline uint16_t r5_fonte(uint8_t ft, uint32_t u, uint16_t ant)
{
    switch (ft) {
    case G3D_FT_TEXTURA:   return (uint16_t)R5V_T_TEX(u);
    case G3D_FT_CONSTANTE: return (uint16_t)R5V_C_UNID(u);
    case G3D_FT_PRIMARIA:  return (uint16_t)R5V_T_COR;
    default:               return ant;
    }
}

/* Los tres argumentos de un canal: el operando se aplica con la mezcla («alfa» repetido en los tres
 * componentes del color) y, si es «1 - x», con una media instrucción antes a un temporal auxiliar.
 * alfa: canal de alfa (operandos 2 y 3; solo cuenta el componente alfa). */
static inline void r5_args(r5_lista_t *l, const uint8_t ft[3], const uint8_t opd[3], uint32_t u, uint16_t ant, bool alfa,
                           r5_arg_t arg[3], uint32_t *consts)
{
    for (int k = 0; k < 3; k++) {
        uint16_t reg = r5_fonte(ft[k], u, ant);
        if (ft[k] == G3D_FT_CONSTANTE) *consts |= 1u << u;
        bool de_alfa = alfa || opd[k] == G3D_OPD_ALFA || opd[k] == G3D_OPD_UM_MENOS_ALFA;
        r5_arg_t a = de_alfa ? r5a_aaa(reg) : r5a_rgb(reg);
        if (opd[k] == G3D_OPD_UM_MENOS_COR || opd[k] == G3D_OPD_UM_MENOS_ALFA) {
            r5_meia_t *m = r5_l_nova(l);       /* aux = -x * 1 + 1 */
            m->dest = (uint8_t)R5V_T_AUX(k);
            m->a = r5a_neg(a); m->b = r5a_k(R5S_1); m->c = r5a_k(R5S_1);
            a = alfa ? r5a_aaa((uint16_t)R5V_T_AUX(k)) : r5a_rgb((uint16_t)R5V_T_AUX(k));
        }
        arg[k] = a;
    }
}

/* Un canal de una unidad: func(arg0, arg1, arg2) * escala, saturado, al temporal «dest». */
static inline void r5_canal(r5_lista_t *l, uint8_t func, const r5_arg_t a[3], uint8_t escala, uint8_t dest, bool alfa)
{
    uint8_t omod = escala == 4 ? 2 : escala == 2 ? 1 : 0;
    uint16_t t3 = (uint16_t)R5V_T_AUX(3), t4 = (uint16_t)R5V_T_AUX(4);
    r5_arg_t at3 = alfa ? r5a_aaa(t3) : r5a_rgb(t3), at4 = alfa ? r5a_aaa(t4) : r5a_rgb(t4);
    r5_meia_t *m;
    switch (func) {
    case G3D_CB_MODULAR:
        m = r5_l_nova(l); m->a = a[0]; m->b = a[1]; break;
    case G3D_CB_SOMAR:
        m = r5_l_nova(l); m->a = a[0]; m->c = a[1]; break;
    case G3D_CB_SOMAR_SINAL:      /* t3 = a0 + a1; res = t3 - 0,5 */
        m = r5_l_nova(l); m->dest = (uint8_t)t3; m->a = a[0]; m->c = a[1];
        m = r5_l_nova(l); m->a = at3; m->c = r5a_neg(r5a_k(R5S_H)); break;
    case G3D_CB_INTERPOLAR:       /* t3 = a0 - a1; res = t3 * a2 + a1 */
        m = r5_l_nova(l); m->dest = (uint8_t)t3; m->a = r5a_neg(a[1]); m->c = a[0];
        m = r5_l_nova(l); m->a = at3; m->b = a[2]; m->c = a[1]; break;
    case G3D_CB_SUBTRAIR:
        m = r5_l_nova(l); m->a = r5a_neg(a[1]); m->c = a[0]; break;
    case G3D_CB_DOT3_RGB:
    case G3D_CB_DOT3_RGBA:        /* t3 = 2 a0 - 1; t4 = 2 a1 - 1; res = t3 · t4 (= 4 por la suma de (a0 - 0,5)(a1 - 0,5)) */
        m = r5_l_nova(l); m->dest = (uint8_t)t3; m->a = a[0]; m->c = r5a_neg(r5a_k(R5S_H)); m->omod = 1;
        m = r5_l_nova(l); m->dest = (uint8_t)t4; m->a = a[1]; m->c = r5a_neg(r5a_k(R5S_H)); m->omod = 1;
        m = r5_l_nova(l); m->op = R5H_DP; m->a = at3; m->b = at4; m->c = r5a_k(R5S_0); break;
    default:                      /* G3D_CB_SUBSTITUIR */
        m = r5_l_nova(l); m->a = a[0]; break;
    }
    m->dest = dest;
    m->omod = omod;
    m->sat = true;
}

/* Las instrucciones de las dos listas, juntas de dos en dos (la de color y la de alfa son independientes en
 * el US). dot3_rgba: la última del color es un DP3 y el alfa lleva el DP en esa misma instrucción. */
static inline void r5_juntar(r5_fp_t *fp, const r5_lista_t *c, const r5_lista_t *a, bool dot3_rgba, uint8_t dest)
{
    static const r5_meia_t vazia = { false, 0, 0, 0, false, { 0, { 0, 0, 0 }, 0 }, { 0, { 0, 0, 0 }, 0 }, { 0, { 0, 0, 0 }, 0 } };
    if (dot3_rgba) {
        for (int i = 0; i < c->n; i++) {
            r5_meia_t al = vazia;
            if (i == c->n - 1) {
                al = c->m[i];   /* DP: el mismo producto en el alfa, con la misma escala (sus argumentos, a cero: DP3) */
                al.a = r5a_k(R5S_0); al.b = r5a_k(R5S_0); al.c = r5a_k(R5S_0);
                al.dest = dest;
            }
            r5_fp_emitir(fp, &c->m[i], &al, false);
        }
        return;
    }
    int n = c->n > a->n ? c->n : a->n;
    for (int i = 0; i < n; i++) r5_fp_emitir(fp, i < c->n ? &c->m[i] : &vazia, i < a->n ? &a->m[i] : &vazia, false);
}

/* El programa de fragmentos de un estado v2:
 *   1. una lectura de textura por unidad activa;
 *   2. por unidad activa, su combinación de color y de alfa (resultado en R5V_T_ANT(k), alternando k para que
 *      la media de alfa de una unidad no pise lo que la de color de esa misma unidad aún tiene que leer);
 *   3. salida = saturar(resultado + especular), con la niebla (mezcla con R5V_C_NEBLINA por el alfa del
 *      especular) si neblina. */
static inline void r5_fp_gerar(r5_fp_t *fp, const gpu3d_estado2_t *e)
{
    fp->n = 0; fp->consts = 0; fp->max_temp = (int)R5V_T_ESPEC; fp->erro = false;
    fp->ntc = 0; fp->mapa[0] = fp->mapa[1] = fp->mapa[2] = fp->mapa[3] = 0;
    int nu = e->unidades > 4 ? 4 : e->unidades;
    /* Las coordenadas van en el vértice por orden de unidad activa: la k-ésima en la posición k. */
    for (int u = 0; u < nu; u++)
        if (e->u[u].ativa) {
            fp->mapa[fp->ntc] = (uint8_t)(e->u[u].coord > 3 ? u : e->u[u].coord);
            r5_fp_tex(fp, (uint32_t)u, (uint32_t)fp->ntc);
            fp->ntc++;
        }
    /* Semáforos de las lecturas (D426, Amilo): como Mesa (radeon_pair_schedule.c, «Set semaphore bits for last TEX
     * instruction in the block»), solo la ÚLTIMA lectura del bloque lleva TEX_SEM_WAIT y TEX_SEM_ACQUIRE; las de antes,
     * ninguno. Con los dos en cada lectura (lo que hacía el primer generador), la X1400 se colgaba en cuanto un
     * programa tenía dos o más lecturas seguidas: el US no termina, el 3D no queda parado y el CP se para en la
     * siguiente espera (RBBM_STATUS 9401c100). Con una sola lectura (v1, prueba v2 0) no se nota. */
    for (int k = 0; k + 1 < fp->n; k++) {
        fp->p[k * R5_FP_PAL_INST + 0] &= ~R5_FP_ESPERA_TEX;
        fp->p[k * R5_FP_PAL_INST + 1] &= ~R5_FP_TEX_PEDIR;
    }
    if (!fp->ntc) fp->ntc = 1;   /* sin texturas el vértice lleva igual un juego (el formato de v1) */
    uint16_t ant = (uint16_t)R5V_T_COR;
    int k = 0;
    for (int u = 0; u < nu; u++) {
        const gpu3d_unidade_t *un = &e->u[u];
        if (!un->ativa) continue;
        uint8_t dest = (uint8_t)R5V_T_ANT(k);
        r5_lista_t lc = { .n = 0 }, la = { .n = 0 };
        r5_arg_t ac[3], aa[3];
        r5_args(&lc, un->rgb_fonte, un->rgb_opd, (uint32_t)u, ant, false, ac, &fp->consts);
        bool d3a = un->rgb_func == G3D_CB_DOT3_RGBA;
        if (!d3a) r5_args(&la, un->alfa_fonte, un->alfa_opd, (uint32_t)u, ant, true, aa, &fp->consts);
        r5_canal(&lc, un->rgb_func, ac, un->rgb_escala, dest, false);
        if (!d3a) r5_canal(&la, un->alfa_func > G3D_CB_SUBTRAIR ? G3D_CB_SUBSTITUIR : un->alfa_func, aa, un->alfa_escala, dest, true);
        r5_juntar(fp, &lc, &la, d3a, dest);
        ant = dest;
        k ^= 1;
    }
    /* Salida. */
    r5_meia_t c = { true, R5H_MAD, 0, 0, true, r5a_rgb(ant), r5a_k(R5S_1), r5a_rgb((uint16_t)R5V_T_ESPEC) };
    r5_meia_t a = { true, R5H_MAD, 0, 0, true, r5a_aaa(ant), r5a_k(R5S_1), r5a_k(R5S_0) };
    a.a.swz[0] = R5S_A;
    if (e->flags & G3D2_NEBLINA) {
        /* aux0 = sat(ant + espec); aux1 = aux0 - niebla; sale aux1 * f + niebla (f = alfa del especular). */
        uint16_t t0 = (uint16_t)R5V_T_AUX(0), t1 = (uint16_t)R5V_T_AUX(1);
        c.dest = (uint8_t)t0; a.dest = (uint8_t)t0;
        r5_fp_emitir(fp, &c, &a, false);
        r5_meia_t c2 = { true, R5H_MAD, (uint8_t)t1, 0, false, r5a_neg(r5a_rgb((uint16_t)R5V_C_NEBLINA)), r5a_k(R5S_1), r5a_rgb(t0) };
        r5_meia_t a2 = { false, 0, 0, 0, false, r5a_k(R5S_0), r5a_k(R5S_0), r5a_k(R5S_0) };
        r5_fp_emitir(fp, &c2, &a2, false);
        c = (r5_meia_t){ true, R5H_MAD, 0, 0, true, r5a_rgb(t1), r5a_aaa((uint16_t)R5V_T_ESPEC), r5a_rgb((uint16_t)R5V_C_NEBLINA) };
        a = (r5_meia_t){ true, R5H_MAD, 0, 0, true, r5a_aaa(t0), r5a_k(R5S_1), r5a_k(R5S_0) };
        fp->consts |= 1u << 4;
    }
    r5_fp_emitir(fp, &c, &a, true);
}

/* Una clave de la parte del estado que decide el programa (para la caché del driver). */
static inline uint32_t r5_fp_chave(const gpu3d_estado2_t *e)
{
    uint32_t h = 2166136261u;
    #define R5H_MIX(v) do { h ^= (uint32_t)(v); h *= 16777619u; } while (0)
    int nu = e->unidades > 4 ? 4 : e->unidades;
    R5H_MIX(nu); R5H_MIX(e->flags & G3D2_NEBLINA);
    for (int u = 0; u < nu; u++) {
        const gpu3d_unidade_t *x = &e->u[u];
        R5H_MIX(x->ativa);
        if (!x->ativa) continue;
        R5H_MIX(x->coord); R5H_MIX(x->rgb_func); R5H_MIX(x->alfa_func); R5H_MIX(x->rgb_escala); R5H_MIX(x->alfa_escala);
        for (int k = 0; k < 3; k++) { R5H_MIX(x->rgb_fonte[k]); R5H_MIX(x->rgb_opd[k]); R5H_MIX(x->alfa_fonte[k]); R5H_MIX(x->alfa_opd[k]); }
    }
    #undef R5H_MIX
    return h;
}

/* Las constantes del programa (cada una cuatro flotantes r, g, b, a de 0 a 1): la de cada unidad y la niebla. */
static inline uint32_t r5_float_byte(uint32_t b) { return b ? g3d_float_de_fixo((int32_t)(b * 0x10101u), 24) : 0; }
static inline void r5_fp_constantes(g3d_fila_t *f, const r5_fp_t *fp, const gpu3d_estado2_t *e)
{
    for (uint32_t i = 0; i < 5; i++) {
        if (!(fp->consts & (1u << i))) continue;
        uint32_t c = i < 4 ? e->u[i].constante : (e->cor_neblina | 0xFF000000u);
        r5_reg(f, R5_GA_US_INDICE, (1u << 16) | i);    /* GA_US_VECTOR_INDEX: constante i */
        r5_reg(f, R5_GA_US_DADOS, r5_float_byte((c >> 16) & 0xFFu));
        r5_reg(f, R5_GA_US_DADOS, r5_float_byte((c >> 8) & 0xFFu));
        r5_reg(f, R5_GA_US_DADOS, r5_float_byte(c & 0xFFu));
        r5_reg(f, R5_GA_US_DADOS, r5_float_byte(c >> 24));
    }
}

/* El programa en el US (direcciones 0 a n - 1, como en v1). */
static inline void r5_fp_carregar(g3d_fila_t *f, const r5_fp_t *fp)
{
    r5_reg(f, R5_US_PIXSIZE, (uint32_t)fp->max_temp);
    r5_reg(f, R5_US_CODE_ADDR, 0u | ((uint32_t)(fp->n - 1) << 16));
    r5_reg(f, R5_US_CODE_RANGE, 0u | ((uint32_t)(fp->n - 1) << 16));
    r5_reg(f, R5_US_CODE_OFFSET, 0);
    r5_reg(f, R5_GA_US_INDICE, 0);
    for (int i = 0; i < fp->n * R5_FP_PAL_INST; i++) r5_reg(f, R5_GA_US_DADOS, fp->p[i]);
}

/* El procesador de vértices y el rasterizador para ntc juegos de coordenadas (1 a 4): PSC, programa de
 * vértices que copia (3 + ntc salidas), formato de salida y RS (juego i -> R5V_T_COORD(i)). Delante,
 * el motor parado (se cambia el programa de vértices). */
static inline uint32_t r5_pal_vert(int ntc) { return 6u + 2u * (uint32_t)ntc; }
static inline void r5_estado_vertices(g3d_fila_t *f, int ntc)
{
    if (ntc < 1) ntc = 1;
    if (ntc > 4) ntc = 4;
    uint32_t ne = 3u + (uint32_t)ntc;                  /* entradas: posición, color, especular y los juegos */
    /* D426 (Amilo, 2026-11-20): antes iba solo WAIT_UNTIL con 3D_IDLECLEAN. «Limpio» quiere decir con las cachés de
     * color y de Z vaciadas, y nadie había pedido vaciarlas: tras cualquier dibujo la espera no terminaba nunca y el
     * CP se quedaba parado (RBBM_STATUS 9401c100, la regresión de la X1400 con v2). Con el vaciado delante (lo mismo
     * que la valla y r5_inicio) espera a que termine el dibujo anterior y sigue. */
    r5_vaciar(f);
    r5_reg(f, R5_VAP_PVS_FLUSH, 0);
    uint16_t psc[8], ext[8];
    psc[0] = (uint16_t)(R5_PSC_FLOAT_4 | R5_PSC_COM_SINAL | R5_PSC_DESTINO(0)); ext[0] = (uint16_t)R5_SWZ_XYZW;
    psc[1] = (uint16_t)(R5_PSC_D3DCOLOR | R5_PSC_NORMALIZAR | R5_PSC_DESTINO(1)); ext[1] = (uint16_t)R5_SWZ_XYZW;
    psc[2] = (uint16_t)(R5_PSC_D3DCOLOR | R5_PSC_NORMALIZAR | R5_PSC_DESTINO(2)); ext[2] = (uint16_t)R5_SWZ_XYZW;
    for (uint32_t i = 3; i < ne; i++) { psc[i] = (uint16_t)(R5_PSC_FLOAT_2 | R5_PSC_COM_SINAL | R5_PSC_DESTINO(i)); ext[i] = (uint16_t)R5_SWZ_XY01; }
    psc[ne - 1] |= (uint16_t)R5_PSC_ULTIMO;
    for (uint32_t i = ne; i < 8; i++) { psc[i] = 0; ext[i] = 0; }
    for (uint32_t r = 0; r < (ne + 1) / 2; r++) {
        r5_reg(f, R5_VAP_PSC_0 + r * 4, psc[2 * r] | ((uint32_t)psc[2 * r + 1] << 16));
        r5_reg(f, R5_VAP_PSC_EXT_0 + r * 4, ext[2 * r] | ((uint32_t)ext[2 * r + 1] << 16));
    }
    r5_reg(f, R5_VAP_PVS_INDICE, 0);
    for (uint32_t i = 0; i < ne; i++) {
        r5_reg(f, R5_VAP_PVS_DADOS, R5_PVS_SOMA_SAIDA(i));
        r5_reg(f, R5_VAP_PVS_DADOS, i >= 3 ? R5_PVS_ENTRADA_XY01(i) : R5_PVS_ENTRADA(i));
        r5_reg(f, R5_VAP_PVS_DADOS, R5_PVS_ZERO(i));
        r5_reg(f, R5_VAP_PVS_DADOS, R5_PVS_ZERO(i));
    }
    r5_reg(f, R5_VAP_PVS_CODE_CNTL_0, 0u | ((ne - 1u) << 10) | ((ne - 1u) << 20));
    r5_reg(f, R5_VAP_PVS_CODE_CNTL_1, ne - 1u);
    uint32_t fmt1 = 0;
    for (uint32_t i = 0; i < (uint32_t)ntc; i++) fmt1 |= 2u << (3 * i);
    r5_reg(f, R5_VAP_OUT_VTX_FMT_0, 7u);
    r5_reg(f, R5_VAP_OUT_VTX_FMT_1, fmt1);
    r5_reg(f, R5_VAP_VTX_SIZE, r5_pal_vert(ntc));
    /* Rasterizador: instrucción i = juego i (si lo hay) y color i (0 color, 1 especular). */
    uint32_t ni = (uint32_t)ntc > 2 ? (uint32_t)ntc : 2u;
    r5_reg(f, R5_RS_COUNT, (2u * (uint32_t)ntc) | (2u << 7) | R5_RS_ALTA_RES);
    r5_reg(f, R5_RS_INST_COUNT, ni - 1u);
    for (uint32_t i = 0; i < ni; i++) {
        uint32_t ip = i < (uint32_t)ntc ? (2u * i) | ((2u * i + 1u) << 6) | (R5_RS_K0 << 12) | (R5_RS_K1 << 18)
                                       : R5_RS_K0 | (R5_RS_K0 << 6) | (R5_RS_K0 << 12) | (R5_RS_K1 << 18);
        if (i < 2) ip |= i << 24;
        r5_reg(f, R5_RS_IP_0 + i * 4, ip);
        uint32_t in = 0;
        if (i < (uint32_t)ntc) in |= i | R5_RS_TEX_ESCREVE | (R5V_T_COORD(i) << 5);
        if (i < 2) in |= (i << 12) | R5_RS_COR_ESCREVE | ((i == 0 ? R5V_T_COR : R5V_T_ESPEC) << 18);
        r5_reg(f, R5_RS_INST_0 + i * 4, in);
    }
}

/* El muestreador y el formato de la unidad u. */
static inline void r5_estado_unidade(g3d_fila_t *f, uint32_t u, const r5_tex_t *t, const gpu3d_unidade_t *un)
{
    static const uint8_t env[3] = { 0, 2, 1 };   /* repetir, sujetar al borde, espejo (R300_TX_REPEAT/CLAMP_TO_EDGE/MIRRORED) */
    uint32_t fl = (uint32_t)env[un->env_s > 2 ? 1 : un->env_s] | ((uint32_t)env[un->env_t > 2 ? 1 : un->env_t] << 3);
    uint32_t f1 = 0;
    if (un->aniso > 1) {
        uint32_t r = un->aniso >= 16 ? 4u : un->aniso >= 8 ? 3u : un->aniso >= 4 ? 2u : 1u;
        fl |= (3u << 9) | (3u << 11) | (r << 21);                     /* MAG/MIN_FILTER_ANISO, MAX_ANISO */
        uint32_t q = (uint32_t)(un->aniso - 1) * 42u / 10u;
        f1 |= ((q > 63 ? 63u : q) << 23) | (1u << 30);                 /* R500_TX_MAX_ANISO, ANISO_HIGH_QUALITY */
    } else {
        fl |= (un->filtro & G3D_FIL_LINEAR_MAG ? R5_TX_MAG_LINEAR : R5_TX_MAG_PROXIMO) |
              (un->filtro & G3D_FIL_LINEAR_MIN ? R5_TX_MIN_LINEAR : R5_TX_MIN_PROXIMO);
    }
    uint32_t lados = ((uint32_t)(t->larg - 1) & 0x7FFu) | (((uint32_t)(t->alt - 1) & 0x7FFu) << 11);
    r5_reg(f, R5_TX_FILTER0_0 + u * 4, fl);
    r5_reg(f, R5_TX_FILTER1_0 + u * 4, f1);
    r5_reg(f, R5_TX_FORMAT0_0 + u * 4, lados | R5_TX_PASSO_PROPRIO);
    r5_reg(f, R5_TX_FORMAT1_0 + u * 4, t->formato1 ? t->formato1 : R5_TX_ARGB8888);
    r5_reg(f, R5_TX_FORMAT2_0 + u * 4, t->passo_px - 1);
    r5_reg(f, R5_TX_OFFSET_0 + u * 4, t->gpu);
    r5_reg(f, R5_TX_BORDER_COLOR_0 + u * 4, 0);
    r5_reg(f, R5_US_FORMAT0_0 + u * 4, lados);
}

/* TX_FORMAT1 de los formatos v2 (mezcla de canales de ARGB8888, que es la de los DXT en R5xx: dxtc_swizzle). */
#define R5_TX_DXT1   0xA60Fu
#define R5_TX_DXT3   0xA610u
#define R5_TX_DXT5   0xA611u

/* Funciones de comparación: G3D_CMP_* (orden de GL) -> R300_ZS_* (Z y esténcil); la prueba alfa (FG) ya va en el de GL. */
static inline uint32_t r5_cmp_zs(uint8_t c) { static const uint8_t m[8] = { 0, 1, 3, 2, 5, 6, 4, 7 }; return m[c & 7]; }
/* Factores: G3D_F_* -> R300_BLEND_GL_*. */
static inline uint32_t r5_fator(uint8_t f)
{
    static const uint8_t m[11] = { 32, 33, 38, 39, 34, 35, 36, 37, 40, 41, 42 };
    return f <= 10 ? m[f] : 33;
}
/* Ecuaciones: suma, resta (origen - destino) y resta invertida, saturando. */
static inline uint32_t r5_eq(uint8_t q) { return q == G3D_EQ_SUBTRAI ? (2u << 12) : q == G3D_EQ_SUBTRAI_INV ? (6u << 12) : 0u; }
/* Operación lógica: el orden de GL -> el de RB3D_ROPCNTL (el de Gallium: CLEAR, NOR, AND_INVERTED...). */
static inline uint32_t r5_rop(uint8_t g) { static const uint8_t m[16] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 }; return m[g & 15]; }

/* Lo de por fragmento de un dibujo v2 (todo cada vez: el driver no guarda estado). larg/alt: el destino. */
static inline void r5_estado_fragmento(g3d_fila_t *f, const gpu3d_estado2_t *e, int larg, int alt, bool com_estencil)
{
    uint32_t fl = e->flags;
    /* Z y esténcil. */
    uint32_t zc = 0, zs = R5_ZF_SEMPRE;
    if (fl & G3D2_Z_TESTE) { zc |= R5_Z_LIGADO | (fl & G3D2_Z_ESCRITA ? R5_Z_ESCREVE : 0u); zs = r5_cmp_zs(e->func_z); }
    if ((fl & G3D2_ESTENCIL) && com_estencil) {
        zc |= 1u;                                                       /* STENCIL_ENABLE (delante y detrás, lo mismo) */
        zs |= (r5_cmp_zs(e->est_func) << 3) | ((uint32_t)(e->est_falha & 7) << 6) | ((uint32_t)(e->est_zpassa & 7) << 9) |
              ((uint32_t)(e->est_zfalha & 7) << 12);
        zs |= (r5_cmp_zs(e->est_func) << 15) | ((uint32_t)(e->est_falha & 7) << 18) | ((uint32_t)(e->est_zpassa & 7) << 21) |
              ((uint32_t)(e->est_zfalha & 7) << 24);
    }
    r5_reg(f, R5_ZB_CNTL, zc);
    r5_reg(f, R5_ZB_ZSTENCILCNTL, zs);
    r5_reg(f, R5_ZB_STENCILREFMASK, (uint32_t)e->est_ref | ((uint32_t)e->est_mascara << 8) | ((uint32_t)e->est_escrita << 16));
    /* Prueba alfa (fija en el FG, con el valor de 8 bits). */
    r5_reg(f, R5_FG_ALPHA_FUNC, (fl & G3D2_ALFA_TESTE) ? (uint32_t)e->ref_alfa | ((uint32_t)(e->func_alfa & 7) << 8) | (1u << 11) | (1u << 12) : 0u);
    /* Máscara de color: RB3D_COLOR_CHANNEL_MASK es azul, verde, rojo, alfa (bits 0-3). */
    uint32_t mc = e->mascara_cor;
    r5_reg(f, R5_RB3D_MASCARA, ((mc & G3D_MC_B) ? 1u : 0u) | ((mc & G3D_MC_G) ? 2u : 0u) | ((mc & G3D_MC_R) ? 4u : 0u) | ((mc & G3D_MC_A) ? 8u : 0u));
    /* Mezcla u operación lógica. */
    if (fl & G3D2_LOGICA) {
        r5_reg(f, R5_RB3D_CBLEND, 0);
        r5_reg(f, R5_RB3D_ABLEND, 0);
        r5_reg(f, R5_RB3D_ROPCNTL, 4u | (r5_rop(e->op_logica) << 8));
    } else {
        r5_reg(f, R5_RB3D_ROPCNTL, 0);
        if (fl & G3D2_MEZCLA) {
            r5_reg(f, R5_RB3D_CBLEND, R5_BLEND_LIGADO | (1u << 1) | R5_BLEND_LER_DESTINO | r5_eq(e->mez_eq_rgb) |
                                      (r5_fator(e->mez_orig_rgb) << 16) | (r5_fator(e->mez_dest_rgb) << 24));
            r5_reg(f, R5_RB3D_ABLEND, r5_eq(e->mez_eq_alfa) | (r5_fator(e->mez_orig_alfa) << 16) | (r5_fator(e->mez_dest_alfa) << 24));
        } else {
            r5_reg(f, R5_RB3D_CBLEND, 0);
            r5_reg(f, R5_RB3D_ABLEND, 0);
        }
    }
    /* Tijera (inclusiva). */
    int x0 = 0, y0 = 0, x1 = larg, y1 = alt;
    if (fl & G3D2_TIJERA) { x0 = e->tijera_x; y0 = e->tijera_y; x1 = x0 + e->tijera_l; y1 = y0 + e->tijera_a; }
    if (x1 > larg) x1 = larg;
    if (y1 > alt) y1 = alt;
    r5_reg(f, R5_SC_SCISSOR0, (uint32_t)x0 | ((uint32_t)y0 << 13));
    r5_reg(f, R5_SC_SCISSOR1, r5_tijera1(x1, y1));
}

/* Los vértices v2 empaquetados (6 + 2 ntc palabras): «mapa[i]» es el juego de coordenadas de la posición i. */
static inline uint32_t r5_pal_triangulos2(int n, int ntc) { return 2u + (uint32_t)n * r5_pal_vert(ntc); }
static inline void r5_triangulos2(g3d_fila_t *f, const gpu3d_vert2_t *v, int n, int ntc, const uint8_t mapa[4])
{
    uint32_t pv = r5_pal_vert(ntc);
    g3d_por(f, R5_P3_DRAW_IMMD_2 | (((uint32_t)n * pv) << 16));
    g3d_por(f, R5_PRIM_TRIANGULOS | R5_PRIM_NO_ANEL | ((uint32_t)n << 16));
    for (int i = 0; i < n; i++) {
        g3d_por(f, v[i].x); g3d_por(f, v[i].y); g3d_por(f, v[i].z); g3d_por(f, v[i].rhw);
        g3d_por(f, v[i].cor); g3d_por(f, v[i].espec);
        for (int k = 0; k < ntc; k++) { g3d_por(f, v[i].tc[mapa[k] & 3][0]); g3d_por(f, v[i].tc[mapa[k] & 3][1]); }
    }
}
/* Cuántos vértices caben en una orden (múltiplo de 3; el tope de una orden es 16380 palabras, y la cola en
 * RAM del driver de PREP_PAL). */
static inline int r5_vert_por_pacote2(int ntc) { int n = 4080 / (int)r5_pal_vert(ntc); return n - n % 3; }

#endif
