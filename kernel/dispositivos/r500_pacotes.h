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
#define R5_PVS_ZERO            (1u | (4u << 13) | (4u << 16) | (4u << 19) | (4u << 22))                           /* 0, 0, 0, 0 */
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

/* La valla: cuando todo lo anterior ha terminado, el CP escribe «seq» en un registro de apuntes. */
static inline void r5_valla(g3d_fila_t *f, uint32_t apunte, uint32_t seq)
{
    r5_vaciar(f);
    r5_reg(f, apunte, seq);
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
        r5_reg(f, R5_VAP_PVS_DADOS, R5_PVS_ZERO);
        r5_reg(f, R5_VAP_PVS_DADOS, R5_PVS_ZERO);
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
    r5_reg(f, R5_SC_SCISSOR1, (uint32_t)(a->larg - 1) | ((uint32_t)(a->alt - 1) << 13));
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

#endif
