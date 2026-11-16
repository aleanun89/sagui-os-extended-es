// SPDX-License-Identifier: GPL-2.0-or-later
/* gen3_pacotes.h - las órdenes 3D de las GPU Intel Gen3 (915, 945, G33, Pineview), D285.
 *
 * Solo construye palabras en una cola (g3d_fila_t): no toca ningún registro,
 * así que se compila igual en el kernel y en la prueba del anfitrión
 * (tests/gen3_3d_unidad.c), que la descodifica y comprueba longitudes y campos.
 *
 * La tubería de Gen3 es casi fija: los vértices llegan ya transformados
 * (x, y en píxeles, z, 1/w), el rasterizador interpola color, especular y
 * coordenadas de textura con corrección de perspectiva, y un programa de
 * píxeles corto (declaraciones + instrucciones de tres palabras) da el color.
 * El estado va en «3DSTATE_*» y los triángulos en «3DPRIMITIVE» con los vértices
 * dentro de la propia orden.
 *
 * Fuentes de los números (código propio; ver docs/terceros.md): los nombres
 * de campos y sus valores son los de la documentación de Intel del 3D de
 * Gen3, cotejados con las cabeceras de Mesa «i915_reg.h» e «intel_reg.h» (MIT,
 * rama amber) y «src/gallium/drivers/i915/i915_reg.h» (MIT, mesa-23.0.0); los
 * bits de MI_BATCH_BUFFER_START, con «gt/intel_gpu_commands.h» de Linux 6.6
 * (SPDX MIT, Intel). */
#ifndef SAGUI_GEN3_PACOTES_H
#define SAGUI_GEN3_PACOTES_H
#include "gpu3d.h"

/* ---- órdenes de memoria (MI) ---- */
#define G3_MI_NOOP            0x00000000u
#define G3_MI_FLUSH           (0x04u << 23)
#define G3_MI_FLUSH_LEITURA   (1u << 0)                /* D358: además invalida las cachés de lectura (texturas) */
#define G3_MI_LOTE_FIM        (0x0Au << 23)            /* MI_BATCH_BUFFER_END */
#define G3_MI_LOTE_INICIO     (0x31u << 23)            /* MI_BATCH_BUFFER_START: 2 palabras */
#define G3_LOTE_GTT           (2u << 6)                /* la dirección del lote es del GTT (antes de Gen4) */
#define G3_LOTE_NAO_SEGURO    1u                       /* bit 0 de la dirección: lote sin privilegios */

/* ---- blitter (para limpiar el destino y el Z) ---- */
#define G3_XY_COLOR_BLT       ((2u << 29) | (0x50u << 22) | 4)   /* 6 palabras */
#define G3_BLT_ALFA_RGB       (3u << 20)               /* escribe los cuatro bytes (32 bpp) */
#define G3_BR13_565           (1u << 24)
#define G3_BR13_8888          (3u << 24)
#define G3_ROP_PATCOPY        (0xF0u << 16)
#define G3_XY_SRC_COPY_BLT    ((2u << 29) | (0x53u << 22) | 6)   /* 8 palabras (D358: pruebas de la RAM en el GTT) */
#define G3_ROP_SRCCOPY        (0xCCu << 16)

/* ---- 3D ---- */
#define G3_3D                 (3u << 29)
#define G3_PRIM               (G3_3D | (0x1Fu << 24))  /* vértices dentro de la orden */
#define G3_PRIM_TRILISTA      (0u << 18)

#define G3_AA                 (G3_3D | (0x06u << 24))
#define G3_REGRAS_RASTER      (G3_3D | (0x07u << 24))
#define G3_STENCIL_TRAS       (G3_3D | (0x08u << 24))
#define G3_ALFA_INDEP         (G3_3D | (0x0Bu << 24))
#define G3_MODOS_4            (G3_3D | (0x0Du << 24))
#define G3_COORD_BINDINGS     (G3_3D | (0x16u << 24))
#define G3_TESOURA_ATIVA      (G3_3D | (0x1Cu << 24) | (0x10u << 19))
#define G3_SUBRECT_Z_NAO      (G3_3D | (0x1Cu << 24) | (0x11u << 19) | 2)
#define G3_MAPAS              (G3_3D | (0x1Du << 24) | (0x00u << 16))
#define G3_AMOSTRADORES       (G3_3D | (0x1Du << 24) | (0x01u << 16))
#define G3_ESTADO_IMEDIATO    (G3_3D | (0x1Du << 24) | (0x04u << 16))
#define G3_PROGRAMA           (G3_3D | (0x1Du << 24) | (0x05u << 16))
#define G3_ESTADO_INDIRETO    (G3_3D | (0x1Du << 24) | (0x07u << 16))
#define G3_RET_DESENHO        (G3_3D | (0x1Du << 24) | (0x80u << 16) | 3)
#define G3_TESOURA_RET        (G3_3D | (0x1Du << 24) | (0x81u << 16) | 1)
#define G3_VARS_DESTINO       (G3_3D | (0x1Du << 24) | (0x85u << 16))
#define G3_BUFFER             (G3_3D | (0x1Du << 24) | (0x8Eu << 16) | 1)
#define G3_Z_PADRAO           (G3_3D | (0x1Du << 24) | (0x98u << 16))
#define G3_DIFUSO_PADRAO      (G3_3D | (0x1Du << 24) | (0x99u << 16))
#define G3_ESPEC_PADRAO       (G3_3D | (0x1Du << 24) | (0x9Au << 16))

#define G3_BUF_COR            (0x3u << 24)
#define G3_BUF_Z              (0x7u << 24)

/* VARS_DESTINO */
#define G3_DV_BIAS_MEIO       ((0x8u << 20) | (0x8u << 16))   /* centros de píxel en .5 */
#define G3_DV_LOD_OGL         (1u << 28)
#define G3_DV_ARGB8888        (3u << 8)
#define G3_DV_Z16             0u
#define G3_DV_Z24             (2u << 2)

/* Estado inmediato: S2 (formato de las coordenadas de textura), S4 (formato del
 * vértice y caras), S5 y S6 (Z, mezcla y escritura). */
#define G3_CARGA_S(n)         (1u << (4 + (n)))
#define G3_S2_TEX0_2D         0xFFFFFFF0u              /* unidad 0 con (u, v); las demás, ausentes */
#define G3_S4_PONTO_1         (1u << 23)
#define G3_S4_LINHA_1         (2u << 19)
#define G3_S4_SEM_CULL        (1u << 13)
#define G3_S4_ESPEC           (1u << 11)
#define G3_S4_COR             (1u << 10)
#define G3_S4_XYZW            (2u << 6)
#define G3_S6_Z_TESTE         (1u << 19)
#define G3_S6_Z_FUNC(f)       ((uint32_t)(f) << 16)
#define G3_S6_MEZCLA          (1u << 15)
#define G3_S6_FATOR_ORIG(f)   ((uint32_t)(f) << 8)
#define G3_S6_FATOR_DEST(f)   ((uint32_t)(f) << 4)
#define G3_S6_Z_ESCRITA       (1u << 3)
#define G3_S6_COR_ESCRITA     (1u << 2)
#define G3_CMP_SEMPRE         0
#define G3_CMP_MENOR          2
#define G3_CMP_MENOR_IGUAL    4
#define G3_FAT_ZERO           1
#define G3_FAT_UM             2
#define G3_FAT_ALFA           5
#define G3_FAT_UM_MENOS_ALFA  6

/* Mapas (texturas) y amostradores. */
#define G3_MS3_ALT(a)         ((uint32_t)((a) - 1) << 21)
#define G3_MS3_LARG(l)        ((uint32_t)((l) - 1) << 10)
#define G3_MS3_32BIT_ARGB     (3u << 7)                /* superficie de 32 bits, formato ARGB8888 (0 << 3) */
#define G3_MS4_PASSO(p)       ((uint32_t)((p) / 4 - 1) << 21)
#define G3_SS2_MAG(f)         ((uint32_t)(f) << 17)
#define G3_SS2_MIN(f)         ((uint32_t)(f) << 14)
#define G3_FILTRO_PROXIMO     0
#define G3_FILTRO_LINEAR      1
#define G3_SS3_MODO_X(m)      ((uint32_t)(m) << 12)
#define G3_SS3_MODO_Y(m)      ((uint32_t)(m) << 9)
#define G3_SS3_NORMALIZADAS   (1u << 5)
#define G3_COORD_REPETE       0
#define G3_COORD_BORDA        2

/* Programa de píxeles: registros, declaraciones e instrucciones (3 palabras cada una). */
#define G3_REG_R              0u    /* temporal */
#define G3_REG_T              1u    /* interpolado: 0-7 coordenadas, 8 difuso, 9 especular */
#define G3_REG_S              3u    /* amostrador */
#define G3_REG_OC             4u    /* color de salida */
#define G3_T_TEX0             0u
#define G3_T_DIFUSO           8u
#define G3_T_ESPEC            9u
#define G3_DCL                (0x19u << 24)
#define G3_OP_ADD             (0x01u << 24)
#define G3_OP_MOV             (0x02u << 24)
#define G3_OP_MUL             (0x03u << 24)
#define G3_OP_MAD             (0x04u << 24)
#define G3_OP_TEXLD           (0x15u << 24)
#define G3_SATURA             (1u << 22)
#define G3_CANAIS_TODOS       (0xFu << 10)
#define G3_X 0u
#define G3_Y 1u
#define G3_Z 2u
#define G3_W 3u
#define G3_ZERO 4u
#define G3_UM 5u
#define G3_XYZW               ((G3_X << 12) | (G3_Y << 8) | (G3_Z << 4) | G3_W)   /* canales de un origen, sin cambiar */
#define G3_XYZ0               ((G3_X << 12) | (G3_Y << 8) | (G3_Z << 4) | G3_ZERO)

/* Programas que hay. */
#define G3_PROG_COR           0     /* oC = difuso + especular.rgb */
#define G3_PROG_TEXTURA       1     /* oC = textura * difuso + especular.rgb */

/* ------------------------------------------------------------ programa de píxeles */

static inline void g3_dcl(g3d_fila_t *f, uint32_t tipo, uint32_t nr)
{
    g3d_por(f, G3_DCL | (tipo << 19) | (nr << 14) | (tipo == G3_REG_S ? 0 : G3_CANAIS_TODOS));
    g3d_por(f, 0);
    g3d_por(f, 0);
}

/* Una instrucción aritmética: destino y hasta tres orígenes (tipo, número, canales
 * de 16 bits: x<<12 | y<<8 | z<<4 | w). Los orígenes que no se usan van a cero. */
static inline void g3_arit(g3d_fila_t *f, uint32_t op, uint32_t dt, uint32_t dn, uint32_t satura,
                           uint32_t t0, uint32_t n0, uint32_t c0, uint32_t t1, uint32_t n1, uint32_t c1,
                           uint32_t t2, uint32_t n2, uint32_t c2)
{
    g3d_por(f, op | (dt << 19) | (dn << 14) | G3_CANAIS_TODOS | satura | (t0 << 7) | (n0 << 2));
    g3d_por(f, (c0 << 16) | (t1 << 13) | (n1 << 8) | (c1 >> 8));
    g3d_por(f, ((c1 & 0xFFu) << 24) | (t2 << 21) | (n2 << 16) | c2);
}

static inline void g3_texld(g3d_fila_t *f, uint32_t dt, uint32_t dn, uint32_t amostrador, uint32_t ct, uint32_t cn)
{
    g3d_por(f, G3_OP_TEXLD | (dt << 19) | (dn << 14) | amostrador);
    g3d_por(f, (ct << 24) | (cn << 17));
    g3d_por(f, 0);
}

/* El programa entero, con su cabecera. Devuelve las palabras escritas. */
static inline uint32_t g3_programa(g3d_fila_t *f, int qual)
{
    uint32_t ini = f->n;
    g3d_por(f, 0);   /* la cabecera, al final (lleva la longitud) */
    g3_dcl(f, G3_REG_T, G3_T_DIFUSO);
    g3_dcl(f, G3_REG_T, G3_T_ESPEC);
    if (qual == G3_PROG_TEXTURA) {
        g3_dcl(f, G3_REG_T, G3_T_TEX0);
        g3_dcl(f, G3_REG_S, 0);
        g3_texld(f, G3_REG_R, 0, 0, G3_REG_T, G3_T_TEX0);
        /* oC = saturar(r0 * difuso + especular.xyz0) */
        g3_arit(f, G3_OP_MAD, G3_REG_OC, 0, G3_SATURA, G3_REG_R, 0, G3_XYZW, G3_REG_T, G3_T_DIFUSO, G3_XYZW,
                G3_REG_T, G3_T_ESPEC, G3_XYZ0);
    } else {
        /* oC = saturar(difuso + especular.xyz0) */
        g3_arit(f, G3_OP_ADD, G3_REG_OC, 0, G3_SATURA, G3_REG_T, G3_T_DIFUSO, G3_XYZW, G3_REG_T, G3_T_ESPEC, G3_XYZ0,
                0, 0, 0);
    }
    if (ini < f->max) f->p[ini] = G3_PROGRAMA | (f->n - ini - 2);
    return f->n - ini;
}

/* ------------------------------------------------------------ estado */

/* Lo que no cambia nunca (líneas, valores por defecto, tijera apagada, sin
 * estado indirecto, plantilla neutra). */
static inline void g3_estado_fixo(g3d_fila_t *f)
{
    g3d_por(f, G3_AA | (1u << 16) | (1u << 14) | (1u << 8) | (1u << 6));
    g3d_por(f, G3_DIFUSO_PADRAO); g3d_por(f, 0);
    g3d_por(f, G3_ESPEC_PADRAO);  g3d_por(f, 0);
    g3d_por(f, G3_Z_PADRAO);      g3d_por(f, 0);
    /* Cada juego de coordenadas, a su unidad (sin cruces). */
    g3d_por(f, G3_COORD_BINDINGS | (0u << 0) | (1u << 3) | (2u << 6) | (3u << 9) | (4u << 12) | (5u << 15) | (6u << 18) | (7u << 21));
    /* Reglas de rasterizado de OpenGL (puntos; vértice que manda en tiras y abanicos; texkill de 4 canales). */
    g3d_por(f, G3_REGRAS_RASTER | (1u << 15) | (1u << 13) | (1u << 10) | (1u << 9) | (1u << 8) | (1u << 6) | (1u << 5) | (2u << 3));
    g3d_por(f, G3_TESOURA_RET); g3d_por(f, 0); g3d_por(f, 0);
    g3d_por(f, G3_TESOURA_ATIVA | (1u << 1));      /* tijera apagada */
    g3d_por(f, G3_SUBRECT_Z_NAO);
    g3d_por(f, G3_ESTADO_INDIRETO | 0); g3d_por(f, 0);
    g3d_por(f, G3_STENCIL_TRAS | (1u << 1));       /* plantilla de dos caras: cambiar a «no» */
    /* Operación lógica COPY y máscaras de plantilla completas. */
    g3d_por(f, G3_MODOS_4 | (1u << 23) | (0xCu << 18) | (1u << 17) | (0xFFu << 8) | (1u << 16) | 0xFFu);
    g3d_por(f, G3_ALFA_INDEP | (1u << 23));        /* el alfa se mezcla como el color */
}

typedef struct {
    uint32_t cor_gpu, cor_passo;   /* destino de color ARGB8888 */
    uint32_t z_gpu, z_passo;
    int      larg, alt, zbits;     /* zbits: 16 o 24 */
} g3_alvo_t;

typedef struct { uint32_t gpu, passo; int larg, alt; } g3_tex_t;

static inline void g3_estado_alvo(g3d_fila_t *f, const g3_alvo_t *a)
{
    g3d_por(f, G3_BUFFER); g3d_por(f, G3_BUF_COR | (a->cor_passo & ~3u)); g3d_por(f, a->cor_gpu & ~3u);
    g3d_por(f, G3_BUFFER); g3d_por(f, G3_BUF_Z | (a->z_passo & ~3u));     g3d_por(f, a->z_gpu & ~3u);
    g3d_por(f, G3_VARS_DESTINO);
    g3d_por(f, G3_DV_BIAS_MEIO | G3_DV_LOD_OGL | G3_DV_ARGB8888 | (a->zbits == 16 ? G3_DV_Z16 : G3_DV_Z24));
    g3d_por(f, G3_RET_DESENHO);
    g3d_por(f, 0);
    g3d_por(f, 0);
    g3d_por(f, ((uint32_t)(a->alt - 1) << 16) | (uint32_t)(a->larg - 1));
    g3d_por(f, 0);
}

static inline uint32_t g3_fator(uint8_t g)
{
    return g == G3D_F_ZERO ? G3_FAT_ZERO : g == G3D_F_ALFA ? G3_FAT_ALFA : g == G3D_F_UM_MENOS_ALFA ? G3_FAT_UM_MENOS_ALFA : G3_FAT_UM;
}

/* S2, S4, S5 y S6 a partir del estado común; textura y programa. tex NULL: sin textura. */
static inline void g3_estado_desenho(g3d_fila_t *f, const gpu3d_estado_t *e, const g3_tex_t *tex)
{
    uint32_t s6 = G3_S6_COR_ESCRITA;
    if (e->flags & G3D_Z_TESTE) {
        uint32_t fn = e->func_z == G3D_Z_SEMPRE ? G3_CMP_SEMPRE : e->func_z == G3D_Z_MENOR_IGUAL ? G3_CMP_MENOR_IGUAL : G3_CMP_MENOR;
        s6 |= G3_S6_Z_TESTE | G3_S6_Z_FUNC(fn);
        if (e->flags & G3D_Z_ESCRITA) s6 |= G3_S6_Z_ESCRITA;
    }
    if (e->flags & G3D_MEZCLA)
        s6 |= G3_S6_MEZCLA | G3_S6_FATOR_ORIG(g3_fator(e->mez_orig)) | G3_S6_FATOR_DEST(g3_fator(e->mez_dest));
    else
        s6 |= G3_S6_FATOR_ORIG(G3_FAT_UM) | G3_S6_FATOR_DEST(G3_FAT_ZERO);
    g3d_por(f, G3_ESTADO_IMEDIATO | G3_CARGA_S(2) | G3_CARGA_S(3) | G3_CARGA_S(4) | G3_CARGA_S(5) | G3_CARGA_S(6) | 4);
    g3d_por(f, G3_S2_TEX0_2D);
    g3d_por(f, 0);                                                    /* S3: perspectiva en todas las coordenadas */
    g3d_por(f, G3_S4_PONTO_1 | G3_S4_LINHA_1 | G3_S4_SEM_CULL | G3_S4_ESPEC | G3_S4_COR | G3_S4_XYZW);
    g3d_por(f, 0);                                                    /* S5: se escriben los cuatro canales, sin plantilla */
    g3d_por(f, s6);
    if (tex) {
        uint32_t filtro = e->flags & G3D_TEX_LINEAR ? G3_FILTRO_LINEAR : G3_FILTRO_PROXIMO;
        g3d_por(f, G3_MAPAS | 3); g3d_por(f, 1);
        g3d_por(f, tex->gpu & ~3u);
        g3d_por(f, G3_MS3_ALT(tex->alt) | G3_MS3_LARG(tex->larg) | G3_MS3_32BIT_ARGB);
        g3d_por(f, G3_MS4_PASSO(tex->passo));
        g3d_por(f, G3_AMOSTRADORES | 3); g3d_por(f, 1);
        g3d_por(f, G3_SS2_MAG(filtro) | G3_SS2_MIN(filtro));
        g3d_por(f, G3_SS3_MODO_X(e->flags & G3D_TEX_REPETE_S ? G3_COORD_REPETE : G3_COORD_BORDA) |
                   G3_SS3_MODO_Y(e->flags & G3D_TEX_REPETE_T ? G3_COORD_REPETE : G3_COORD_BORDA) | G3_SS3_NORMALIZADAS);
        g3d_por(f, 0);
    }
    g3_programa(f, tex ? G3_PROG_TEXTURA : G3_PROG_COR);
}

/* Lista de triángulos con los vértices dentro (8 palabras cada uno: es gpu3d_vert_t tal cual). */
static inline void g3_triangulos(g3d_fila_t *f, const gpu3d_vert_t *v, int n)
{
    g3d_por(f, G3_PRIM | G3_PRIM_TRILISTA | (uint32_t)(n * 8 - 1));
    const uint32_t *p = (const uint32_t *)v;
    for (int i = 0; i < n * 8; i++) g3d_por(f, p[i]);
}

/* Palabras del estado completo (fijo + destino + dibujo), como mucho. */
#define G3_PAL_ESTADO 96

/* Relleno del blitter (6 palabras): limpia el color o el Z. bpp 32 o 16. */
static inline void g3_blt_preencher(g3d_fila_t *f, uint32_t gpu, uint32_t passo, int larg, int alt, int bpp, uint32_t valor)
{
    g3d_por(f, G3_XY_COLOR_BLT | (bpp == 32 ? G3_BLT_ALFA_RGB : 0));
    g3d_por(f, (bpp == 32 ? G3_BR13_8888 : G3_BR13_565) | G3_ROP_PATCOPY | passo);
    g3d_por(f, 0);
    g3d_por(f, ((uint32_t)alt << 16) | (uint32_t)larg);
    g3d_por(f, gpu);
    g3d_por(f, valor);
}

/* D358: copia del blitter de 32 bits (8 palabras): de «orig» a «dest», los dos sin mosaico. */
static inline void g3_blt_copiar(g3d_fila_t *f, uint32_t dest, uint32_t dpasso, uint32_t orig, uint32_t opasso, int larg, int alt)
{
    g3d_por(f, G3_XY_SRC_COPY_BLT | G3_BLT_ALFA_RGB);
    g3d_por(f, G3_BR13_8888 | G3_ROP_SRCCOPY | dpasso);
    g3d_por(f, 0);
    g3d_por(f, ((uint32_t)alt << 16) | (uint32_t)larg);
    g3d_por(f, dest);
    g3d_por(f, 0);
    g3d_por(f, opasso);
    g3d_por(f, orig);
}

#endif
