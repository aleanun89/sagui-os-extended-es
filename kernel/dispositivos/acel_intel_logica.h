// SPDX-License-Identifier: GPL-2.0-or-later
/* acel_intel_logica.h - la parte pura del blitter de Intel (D195, D359): los
 * modelos de Gen7 (Ivy Bridge), Gen7.5 (Haswell) y Gen8 (Broadwell) con
 * identificador exacto, las familias que se reconocen pero no llevan 2D (Atom
 * Valleyview/Cherryview, Gen9 y Gen11-Gen12: necesitan execlists), el formato
 * de las órdenes del blitter (XY_COLOR_BLT, XY_SRC_COPY_BLT y el vaciado, con
 * direcciones de 32 bits hasta Gen7 y de 64 en Gen8), el plano de la pantalla
 * (qué plano muestra el framebuffer del VBE/GOP, con qué formato, pitch y
 * tiling), las entradas del GTT y la zona reservada de la memoria robada.
 *
 * Sin acceso al hardware: se prueba en el anfitrión
 * (tests/recursos/acel_intel_anfitriao.c, tests/h361_acel2d_intel_moderna.py)
 * con el mismo código que compila el kernel. Valores tomados del PRM público de
 * Intel y de i915 de Linux (i915_reg.h, intel_gtt, i915_gem_stolen.c,
 * i915_pciids.h); el código es propio. */
#ifndef SAGUI_ACEL_INTEL_LOGICA_H
#define SAGUI_ACEL_INTEL_LOGICA_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

/* ---------------------------------------------------------- modelos */

/* Variantes con 2D por el anillo heredado del blitter (BCS). */
enum {
    INTEL_VAR_NADA = 0,
    INTEL_VAR_IVB  = 70,    /* Gen7, Ivy Bridge (HD 2500/4000): GTT de 32 bits, IDs 0x015x/0x016x */
    INTEL_VAR_HSW  = 75,    /* Gen7.5, Haswell (HD 4200-5200): GTT de 32 bits con otra codificación */
    INTEL_VAR_BDW  = 80,    /* Gen8, Broadwell (HD 5300-6300): GTT de 64 bits, órdenes con direcciones de 64 */
    /* D434: Gen9-Gen12 por execlists (acel_intel_exec.c); el anillo heredado ya no sirve. */
    INTEL_VAR_SKL  = 90,    /* Gen9: Skylake, Kaby Lake, Coffee Lake y Comet Lake (ELSP) */
    INTEL_VAR_ICL  = 110,   /* Gen11: Ice Lake (ELSQ, GFX_DISABLE_LEGACY_MODE) */
    INTEL_VAR_TGL  = 120,   /* Gen12: Tiger, Rocket y Alder Lake, Xe incluida (ELSQ) */
};

/* Generación del blitter de cada variante (la que va en la tabla de acel2d.c). */
static inline int intel_var_geracao(int var)
{
    return var == INTEL_VAR_BDW ? 8 : var == INTEL_VAR_IVB || var == INTEL_VAR_HSW ? 7 :
           var == INTEL_VAR_SKL ? 9 : var == INTEL_VAR_ICL ? 11 : var == INTEL_VAR_TGL ? 12 : 0;
}

/* La variante de un identificador PCI (fabricante 0x8086) con la lista exacta
 * de i915_pciids.h; INTEL_VAR_NADA si no es de Gen7/7.5/8 «grande» (las Atom
 * Valleyview y Cherryview tienen otro reparto de registros y no entran). */
static inline int intel_variante(uint16_t m, const char **nome)
{
    const char *n = NULL;
    int v = INTEL_VAR_NADA;
    uint8_t alto = (uint8_t)(m >> 8), meio = (uint8_t)((m >> 4) & 0xF), baixo = (uint8_t)(m & 0xF);
    switch (m) {
    case 0x0152: n = "Intel HD 2500 (Ivy Bridge GT1)"; v = INTEL_VAR_IVB; break;
    case 0x0156: n = "Intel HD 2500 M (Ivy Bridge GT1)"; v = INTEL_VAR_IVB; break;
    case 0x015A: n = "Intel HD P2500 (Ivy Bridge GT1 servidor)"; v = INTEL_VAR_IVB; break;
    case 0x0162: n = "Intel HD 4000 (Ivy Bridge GT2)"; v = INTEL_VAR_IVB; break;
    case 0x0166: n = "Intel HD 4000 M (Ivy Bridge GT2)"; v = INTEL_VAR_IVB; break;
    case 0x016A: n = "Intel HD P4000 (Ivy Bridge GT2 servidor)"; v = INTEL_VAR_IVB; break;
    default: break;
    }
    /* Haswell: 0x04xx (escritorio/móvil), 0x0Axx (ULT/ULX), 0x0Cxx (SDV), 0x0Dxx (CRW,
     * Iris Pro); GT1/GT2/GT3 en el nibble del medio; 2, 6, A, B, E en el bajo. */
    if (!v && (alto == 0x04 || alto == 0x0A || alto == 0x0C || alto == 0x0D) && meio <= 2 &&
        (baixo == 0x2 || baixo == 0x6 || baixo == 0xA || baixo == 0xB || baixo == 0xE)) {
        static const char *const gt[] = { "Intel HD (Haswell GT1)", "Intel HD 4200-4600 (Haswell GT2)",
                                          "Intel HD 5000/Iris (Haswell GT3)" };
        n = gt[meio]; v = INTEL_VAR_HSW;
    }
    /* Broadwell: 0x16xx, GT1/GT2/GT3/reservado; 2, 6, A, B, D, E en el bajo. */
    if (!v && alto == 0x16 && meio <= 3 &&
        (baixo == 0x2 || baixo == 0x6 || baixo == 0xA || baixo == 0xB || baixo == 0xD || baixo == 0xE)) {
        static const char *const gt[] = { "Intel HD (Broadwell GT1)", "Intel HD 5500/5600 (Broadwell GT2)",
                                          "Intel HD 6000/Iris 6100 (Broadwell GT3)", "Intel HD (Broadwell)" };
        n = gt[meio]; v = INTEL_VAR_BDW;
    }
    if (nome) *nome = n;
    return v;
}

/* Familias de Intel que se reconocen pero no llevan 2D (solo para el motivo del
 * log: no se toca la tarjeta). Devuelve la generación (7, 8, 9, 11, 12) o 0. */
static inline int intel_familia_sem_2d(uint16_t m, const char **nome, const char **porque)
{
    /* D434: Skylake, Kaby, Coffee, Comet, Ice Lake, Tiger, Rocket y Alder Lake ya tienen su
     * lista exacta (intel_variante_exec): aquí solo quedan las que no entran. */
    static const struct { uint16_t de, ate; uint8_t gen; const char *nome; } f[] = {
        { 0x0F30, 0x0F33, 7, "Valleyview (Atom Bay Trail)" },
        { 0x0155, 0x0155, 7, "Valleyview (Atom Bay Trail)" },
        { 0x0157, 0x0157, 7, "Valleyview (Atom Bay Trail)" },
        { 0x22B0, 0x22B3, 8, "Cherryview (Atom Braswell)" },
        { 0x0A84, 0x0A84, 9, "Apollo Lake" },
        { 0x1A84, 0x1A85, 9, "Apollo Lake" },
        { 0x5A84, 0x5A85, 9, "Apollo Lake" },
        { 0x3184, 0x3185, 9, "Gemini Lake" },
        { 0x4500, 0x4571, 11, "Elkhart Lake" },
        { 0x4E51, 0x4E71, 11, "Jasper Lake" },
        { 0x4905, 0x4909, 12, "DG1 (Iris Xe dedicada)" },
        { 0xA720, 0xA720, 12, "Raptor Lake-P" },
        { 0xA780, 0xA78B, 12, "Raptor Lake-S" },
        { 0xA7A0, 0xA7AD, 12, "Raptor Lake-P" },
        { 0x7D40, 0x7DD5, 12, "Meteor Lake" },
    };
    for (size_t i = 0; i < sizeof f / sizeof f[0]; i++)
        if (m >= f[i].de && m <= f[i].ate) {
            if (nome) *nome = f[i].nome;
            if (porque)
                *porque = f[i].gen == 7 || f[i].gen == 8
                    ? "Atom: otro reparto de registros, sin 2D (D359)"
                    : "sin lista de execlists para esta familia (D434)";
            return f[i].gen;
        }
    return 0;
}

/* ---------------------------------------------------------- órdenes del blitter */

#define INTEL_MI_NOOP        0x00000000u
#define INTEL_MI_FLUSH       0x02000000u                          /* Gen3-5: MI_INSTR(0x04, 0) */
#define INTEL_MI_FLUSH_DW(g) ((0x26u << 23) | ((g) >= 8 ? 2u : 1u))  /* Gen6+: 3 palabras (4 en Gen8) */
#define INTEL_BLT_32BPP      (3u << 24)                           /* BR13 bits 25:24 = 11b */
#define INTEL_ROP_PATCOPY    (0xF0u << 16)
#define INTEL_ROP_SRCCOPY    (0xCCu << 16)
/* Cabeceras: cliente 2D (2 << 29), código, escribir alfa y RGB (3 << 20), longitud - 2. */
#define INTEL_XY_COLOR_BLT(g) ((2u << 29) | (0x50u << 22) | (3u << 20) | ((g) >= 8 ? 5u : 4u))
#define INTEL_XY_SRC_COPY(g)  ((2u << 29) | (0x53u << 22) | (3u << 20) | ((g) >= 8 ? 8u : 6u))
#define INTEL_PAL_MAX        12   /* palabras de la orden más larga (copia en Gen8: 10) */

/* El vaciado que sigue a cada orden (que lo escrito llegue a la memoria antes de
 * que la CPU lo lea), con un NOOP si hace falta para que el total sea par.
 * Sin escritura posterior: la dirección no se usa. Devuelve las palabras. */
static inline int intel_cmd_flush(uint32_t *b, int gen)
{
    if (gen >= 8) { b[0] = INTEL_MI_FLUSH_DW(gen); b[1] = 0; b[2] = 0; b[3] = 0; return 4; }
    if (gen >= 6) { b[0] = INTEL_MI_FLUSH_DW(gen); b[1] = 0; b[2] = 0; b[3] = INTEL_MI_NOOP; return 4; }
    b[0] = INTEL_MI_FLUSH; b[1] = INTEL_MI_NOOP;
    return 2;
}

/* Rellenar (x, y, l, a) con «cor» en una superficie de 32 bits en la dirección
 * gráfica «dst» (GTT) con «pitch» bytes por línea. */
static inline int intel_cmd_relleno(uint32_t *b, int gen, uint32_t pitch, uint32_t dst, int x, int y, int l, int a,
                                    uint32_t cor)
{
    int n = 0;
    b[n++] = INTEL_XY_COLOR_BLT(gen);
    b[n++] = INTEL_BLT_32BPP | INTEL_ROP_PATCOPY | pitch;
    b[n++] = ((uint32_t)y << 16) | (uint32_t)x;
    b[n++] = ((uint32_t)(y + a) << 16) | (uint32_t)(x + l);
    b[n++] = dst;
    if (gen >= 8) b[n++] = 0;          /* bits altos de la dirección */
    b[n++] = cor;
    return n;
}

/* Copiar (ox, oy) -> (dx, dy) de l x a píxeles dentro de la misma superficie. */
static inline int intel_cmd_copia(uint32_t *b, int gen, uint32_t pitch, uint32_t fb, int ox, int oy, int dx, int dy,
                                  int l, int a)
{
    int n = 0;
    b[n++] = INTEL_XY_SRC_COPY(gen);
    b[n++] = INTEL_BLT_32BPP | INTEL_ROP_SRCCOPY | pitch;
    b[n++] = ((uint32_t)dy << 16) | (uint32_t)dx;
    b[n++] = ((uint32_t)(dy + a) << 16) | (uint32_t)(dx + l);
    b[n++] = fb;
    if (gen >= 8) b[n++] = 0;
    b[n++] = ((uint32_t)oy << 16) | (uint32_t)ox;
    b[n++] = pitch;
    b[n++] = fb;
    if (gen >= 8) b[n++] = 0;
    return n;
}

/* ---------------------------------------------------------- el plano de la pantalla */

/* Registros del plano primario de cada pipe (A, B, C) en Gen4-Gen8: +0x1000 por pipe. */
#define INTEL_DSPCNTR(p)     (0x70180u + 0x1000u * (uint32_t)(p))
#define INTEL_DSPLINOFF(p)   (0x70184u + 0x1000u * (uint32_t)(p))
#define INTEL_DSPSTRIDE(p)   (0x70188u + 0x1000u * (uint32_t)(p))
#define INTEL_DSPSURF(p)     (0x7019Cu + 0x1000u * (uint32_t)(p))
#define INTEL_DSPTILEOFF(p)  (0x701A4u + 0x1000u * (uint32_t)(p))   /* DSPOFFSET en Haswell/Broadwell */
#define INTEL_PLANO_LIGADO   (1u << 31)
#define INTEL_PLANO_TILED    (1u << 10)
#define INTEL_PLANO_FORMATO(c) (((c) >> 26) & 0xFu)
#define INTEL_FMT_BGRX888    0x6u   /* 32 bits, 0x00RRGGBB en memoria (little endian) */
#define INTEL_FMT_BGRA888    0x7u

typedef struct { uint32_t cntr, linoff, stride, surf, tileoff; } intel_plano_t;

enum { INTEL_PLANO_OK = 0, INTEL_PLANO_OUTRO, INTEL_PLANO_EM_TILES, INTEL_PLANO_FORMATO_MAU, INTEL_PLANO_PITCH_MAU };

/* Dirección gráfica del primer píxel visible del plano. */
static inline uint32_t intel_plano_inicio(const intel_plano_t *p, int var)
{
    uint32_t surf = p->surf & ~0xFFFu;
    if (var == INTEL_VAR_HSW || var == INTEL_VAR_BDW) {     /* DSPOFFSET: y << 16 | x */
        uint32_t x = p->tileoff & 0xFFFu, y = (p->tileoff >> 16) & 0xFFFu;
        return surf + y * (p->stride & ~0x3Fu) + x * 4u;
    }
    return surf + p->linoff;
}

/* ¿Este plano muestra la pantalla del VBE/GOP (dirección gráfica fb_gfx) tal
 * como la escribe la CPU (lineal, 32 bits, «pitch» bytes por línea)? */
static inline int intel_plano_conferir(const intel_plano_t *p, int var, uint32_t fb_gfx, uint32_t pitch)
{
    if (!(p->cntr & INTEL_PLANO_LIGADO) || intel_plano_inicio(p, var) != fb_gfx) return INTEL_PLANO_OUTRO;
    if (p->cntr & INTEL_PLANO_TILED) return INTEL_PLANO_EM_TILES;
    uint32_t f = INTEL_PLANO_FORMATO(p->cntr);
    if (f != INTEL_FMT_BGRX888 && f != INTEL_FMT_BGRA888) return INTEL_PLANO_FORMATO_MAU;
    if ((p->stride & 0x3FFC0u) != pitch) return INTEL_PLANO_PITCH_MAU;
    return INTEL_PLANO_OK;
}

static inline const char *intel_plano_texto(int r)
{
    switch (r) {
    case INTEL_PLANO_OK: return "lineal, 32 bits, mismo pitch";
    case INTEL_PLANO_OUTRO: return "no muestra la pantalla";
    case INTEL_PLANO_EM_TILES: return "en tiles";
    case INTEL_PLANO_FORMATO_MAU: return "con otro formato de pixel";
    default: return "con otro pitch";
    }
}

/* ---------------------------------------------------------- GTT y memoria robada */

/* Dónde está el GTT global dentro del BAR 0 (GTTMMADR): Gen6-7 a 2 MB (4 MB de
 * BAR), Gen8 a 8 MB (16 MB de BAR). Bytes de cada entrada. */
static inline uint32_t intel_gtt_desloc(int var) { return var == INTEL_VAR_BDW ? 8u << 20 : 2u << 20; }
static inline uint32_t intel_pte_bytes(int var)  { return var == INTEL_VAR_BDW ? 8u : 4u; }

/* Una entrada del GTT: si es válida y la dirección física de la página.
 * IVB: bits 31:12 y 39:32 en 11:4; HSW: 38:32 en 10:4 (el bit 11 es de caché);
 * BDW: entrada de 64 bits, dirección en 45:12. */
static inline bool intel_pte(int var, uint32_t lo, uint32_t hi, uint64_t *fis)
{
    uint64_t f;
    if (var == INTEL_VAR_BDW) f = (((uint64_t)hi << 32) | lo) & 0x00003FFFFFFFF000ull;
    else if (var == INTEL_VAR_HSW) f = (uint64_t)(lo & 0xFFFFF000u) | ((uint64_t)(lo & 0x7F0u) << 28);
    else f = (uint64_t)(lo & 0xFFFFF000u) | ((uint64_t)(lo & 0xFF0u) << 28);
    if (fis) *fis = f;
    return (lo & 1u) != 0;
}

/* La memoria robada (la de la pantalla): base física en el registro BDSM (configuración
 * PCI 0x5C, bits 31:20) y tamaño en GMCH_CTRL (0x50) en unidades de 32 MB: bits 7:3 en
 * Gen6-7, bits 15:8 en Gen8 (i915_gem_gtt.c, gen6_get_stolen_size/gen8_get_stolen_size;
 * Cherryview es otra cosa y no entra). false: el firmware no la dejó configurada. */
static inline bool intel_robada(int var, uint32_t gmch_ctrl, uint32_t bdsm, uint64_t *base, uint64_t *tam)
{
    uint32_t gms = var == INTEL_VAR_BDW ? (gmch_ctrl >> 8) & 0xFFu : (gmch_ctrl >> 3) & 0x1Fu;
    *base = bdsm & 0xFFF00000u;
    *tam = (uint64_t)gms << 25;
    return *base != 0 && *tam != 0;
}

/* «n» páginas físicas seguidas desde «f0», ¿todas dentro de la memoria robada? */
static inline bool intel_dentro_robada(uint64_t f0, uint32_t n, uint64_t base, uint64_t tam)
{
    return f0 >= base && f0 + (uint64_t)n * 4096u <= base + tam;
}

/* La zona reservada de la memoria robada (GEN6_STOLEN_RESERVED, 0x1082C0): la usa
 * el propio hardware (contexto de energía RC6...). false: no hay. Gen7 (Ivy Bridge y
 * Haswell): base en los bits 31:18 y 1 MB o 256 KB (bit 5). Broadwell: el registro no
 * dice el tamaño (los bits 8:7 son de Cherryview y Gen9): la zona va de la base hasta
 * el final de la memoria robada «topo» (i915_gem_stolen.c, bdw_get_stolen_reserved). */
#define INTEL_STOLEN_RESERVED 0x1082C0u
static inline bool intel_reservado(int var, uint32_t reg, uint64_t topo, uint64_t *base, uint32_t *tam)
{
    if (!(reg & 1u)) return false;
    if (var == INTEL_VAR_BDW) {
        *base = reg & 0xFFF00000u;
        if (!*base) return false;   /* i915: base 0 = nada reservado */
        /* sin «topo» (o por debajo de la base), lo más prudente: hasta los 4 GB */
        uint64_t fim = topo > *base ? topo : 0x100000000ull;
        uint64_t t = fim - *base;
        *tam = t > 0xFFFFF000ull ? 0xFFFFF000u : (uint32_t)t;
    } else {
        *base = reg & 0xFFFC0000u;
        *tam = (reg & (1u << 5)) ? 256u << 10 : 1u << 20;
    }
    return true;
}

/* ¿Una superficie de la tarjeta (otro plano, sprite o cursor que el firmware dejó
 * encendido) en [s, s + ext) pisa la zona [ini, fim) que va a escribir el 2D? */
static inline bool intel_solapa(uint32_t s, uint32_t ext, uint32_t ini, uint32_t fim)
{
    return (uint64_t)s < fim && (uint64_t)s + ext > ini;
}

enum { INTEL_GTT_OK = 0, INTEL_GTT_INVALIDA, INTEL_GTT_SALTO, INTEL_GTT_RESERVADA };

/* Las «n» entradas del GTT a partir de «e» (4 u 8 bytes cada una, como el
 * hardware) tienen que ser válidas y apuntar a páginas físicas seguidas (la
 * memoria robada es contigua: si dos apuntan a la misma página de relleno, el
 * anillo se pisaría a sí mismo), fuera de la zona reservada. «ruim»: la primera
 * entrada que falla; «fis0»: la página física de la primera. */
static inline int intel_gtt_conferir(int var, const volatile uint32_t *e, uint32_t n, bool com_res, uint64_t res_base,
                                     uint32_t res_tam, uint32_t *ruim, uint64_t *fis0)
{
    uint32_t passo = intel_pte_bytes(var) / 4u;
    uint64_t primeiro = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint64_t f;
        uint32_t lo = e[i * passo], hi = passo == 2 ? e[i * passo + 1] : 0;
        *ruim = i;
        if (!intel_pte(var, lo, hi, &f)) return INTEL_GTT_INVALIDA;
        if (i == 0) { primeiro = f; if (fis0) *fis0 = f; }
        else if (f != primeiro + (uint64_t)i * 4096u) return INTEL_GTT_SALTO;
        if (com_res && f + 4096u > res_base && f < res_base + res_tam) return INTEL_GTT_RESERVADA;
    }
    return INTEL_GTT_OK;
}

static inline const char *intel_gtt_texto(int r)
{
    switch (r) {
    case INTEL_GTT_OK: return "valido y contiguo";
    case INTEL_GTT_INVALIDA: return "entrada sin mapear";
    case INTEL_GTT_SALTO: return "paginas no contiguas";
    default: return "pisa la zona reservada";
    }
}

/* Dónde va el anillo de órdenes (desplazamiento desde el principio de la
 * pantalla): justo detrás de la pantalla y de las 64 filas de la zona de
 * prueba, alineado a 4 KB. En Gen7+ nunca al final de la memoria del VBE: allí
 * suele estar la zona reservada del hardware. */
static inline uint32_t intel_anel_desloc(uint32_t pitch, int alt, int zona_a)
{
    return (pitch * (uint32_t)(alt + zona_a) + 0xFFFu) & ~0xFFFu;
}


/* ---------------------------------------------------------- D434: Gen9-Gen12 por execlists */

/* Copyright: los identificadores PCI de la lista exacta, los desplazamientos de los
 * registros y el orden de la imagen del contexto se tomaron de i915 de Linux (MIT:
 * include/drm/i915_pciids.h, gt/intel_lrc.c, gt/intel_lrc_reg.h, gt/intel_engine_regs.h,
 * gt/intel_execlists_submission.c): Copyright (c) 2014-2018 Intel Corporation y los demás
 * autores de i915. D438: también las tablas gen9_rcs_offsets, gen11_rcs_offsets y
 * gen12_rcs_offsets de gt/intel_lrc.c (Linux v6.6, MIT), como datos codificados. El código de
 * este fichero es propio; la licencia MIT de esos ficheros está en docs/terceros.md. */

/* Lista exacta de Gen9 (Skylake, Kaby Lake, Coffee Lake, Comet Lake y Amber Lake). */
static const uint16_t intel_ids_gen9[] = {
    0x1902, 0x1906, 0x190A, 0x190B, 0x190E, 0x1912, 0x1913, 0x1915, 0x1916, 0x1917, 0x191A, 0x191B, 0x191D,
    0x191E, 0x1921, 0x1923, 0x1926, 0x1927, 0x192A, 0x192B, 0x192D, 0x1932, 0x193A, 0x193B, 0x193D,
    0x5902, 0x5906, 0x5908, 0x590A, 0x590B, 0x590E, 0x5912, 0x5913, 0x5915, 0x5916, 0x5917, 0x591A, 0x591B,
    0x591C, 0x591D, 0x591E, 0x5921, 0x5923, 0x5926, 0x5927, 0x593B, 0x87C0,
    0x3E90, 0x3E91, 0x3E92, 0x3E93, 0x3E94, 0x3E96, 0x3E98, 0x3E99, 0x3E9A, 0x3E9B, 0x3E9C, 0x3EA0, 0x3EA1,
    0x3EA2, 0x3EA3, 0x3EA4, 0x3EA5, 0x3EA6, 0x3EA7, 0x3EA8, 0x3EA9, 0x87CA, 0x9B21, 0x9B41, 0x9BA2, 0x9BA4,
    0x9BA5, 0x9BA8, 0x9BAA, 0x9BAC, 0x9BC2, 0x9BC4, 0x9BC5, 0x9BC6, 0x9BC8, 0x9BCA, 0x9BCC, 0x9BE6, 0x9BF6,
};
/* Gen11: Ice Lake (la lista de i915 INTEL_ICL_11_IDS, sin Elkhart ni Jasper Lake). */
static const uint16_t intel_ids_gen11[] = {
    0x8A50, 0x8A51, 0x8A52, 0x8A53, 0x8A54, 0x8A56, 0x8A57, 0x8A58, 0x8A59, 0x8A5A, 0x8A5B, 0x8A5C, 0x8A5D,
    0x8A70, 0x8A71,
};
/* Gen12: Tiger Lake, Rocket Lake, Alder Lake-S, -P y -N (Xe incluida, la del i3-1220P es 0x46A6/0x46A8). */
static const uint16_t intel_ids_gen12[] = {
    0x9A40, 0x9A49, 0x9A59, 0x9A60, 0x9A68, 0x9A70, 0x9A78, 0x9AC0, 0x9AC9, 0x9AD9, 0x9AF8,
    0x4C80, 0x4C8A, 0x4C8B, 0x4C8C, 0x4C90, 0x4C9A,
    0x4680, 0x4682, 0x4688, 0x468A, 0x468B, 0x4690, 0x4692, 0x4693,
    0x4626, 0x4628, 0x462A, 0x46A0, 0x46A1, 0x46A2, 0x46A3, 0x46A6, 0x46A8, 0x46AA, 0x46B0, 0x46B1, 0x46B2,
    0x46B3, 0x46C0, 0x46C1, 0x46C2, 0x46C3,
    0x46D0, 0x46D1, 0x46D2,
};

/* La variante de un identificador PCI con la lista exacta de Gen9-Gen12; INTEL_VAR_NADA si no
 * entra. «nome» dice la familia para el log. */
static inline int intel_variante_exec(uint16_t m, const char **nome)
{
    static const struct { const uint16_t *ids; uint16_t n; uint8_t var; const char *nome; } t[] = {
        { intel_ids_gen9, sizeof intel_ids_gen9 / sizeof intel_ids_gen9[0], INTEL_VAR_SKL,
          "Intel Skylake/Kaby Lake/Coffee Lake/Comet Lake (Gen9)" },
        { intel_ids_gen11, sizeof intel_ids_gen11 / sizeof intel_ids_gen11[0], INTEL_VAR_ICL,
          "Intel Ice Lake (Gen11)" },
        { intel_ids_gen12, sizeof intel_ids_gen12 / sizeof intel_ids_gen12[0], INTEL_VAR_TGL,
          "Intel Tiger/Rocket/Alder Lake, Xe (Gen12)" },
    };
    for (size_t g = 0; g < sizeof t / sizeof t[0]; g++)
        for (uint16_t i = 0; i < t[g].n; i++)
            if (t[g].ids[i] == m) {
                if (nome) *nome = t[g].nome;
                return t[g].var;
            }
    if (nome) *nome = NULL;
    return INTEL_VAR_NADA;
}

/* D438: lo más que ocupa la zona del 2D por execlists detrás de la pantalla y de la zona de prueba
 * (EX_AREA_MAX de acel_intel_exec.c: contexto, página de estado, anillo y PPGTT). El 3D
 * (gpu3d_intel9.c) pone su zona detrás de esto, alineada a 64 KB. */
#define INTEL_EXEC_ZONA2D_MAX (160u * 1024u)

/* ---- registros del motor (desplazamientos desde la base del BCS, 0x22000) ---- */
#define EX_RING_HWSTAM     0x98
#define EX_RING_HWS_PGA    0x80
#define EX_RING_MI_MODE    0x9C
#define EX_RING_MODE       0x29C    /* RING_MODE_GEN7: GFX_RUN_LIST_ENABLE (Gen9-10), GEN11_GFX_DISABLE_LEGACY_MODE (Gen11+) */
#define EX_RING_ELSP       0x230    /* Gen9-10: submit por ELSP (cuatro escrituras por tanda) */
#define EX_RING_EXEC_STAT  0x234    /* EXECLIST_STATUS_LO: puerto 0 válido y activo */
#define EX_RING_ELSQ       0x510    /* Gen11+: cola de envío ELSQ (dos palabras por puerto) */
#define EX_RING_ELSQ_CTRL  0x550    /* EXECLIST_CONTROL: EL_CTRL_LOAD carga la cola */
#define EX_RING_CSB_PTR    0x3A0    /* RING_CONTEXT_STATUS_PTR: puntero de escritura y de lectura del CSB */
#define EX_RING_CTL        0x3C
#define EX_RING_START      0x38
#define EX_RING_HEAD       0x34
#define EX_RING_TAIL       0x30

#define EX_GFX_MODE_LISTA      (1u << 15)  /* GFX_RUN_LIST_ENABLE (Gen9-10) */
#define EX_GFX_MODE_SEM_LEGADO (1u << 3)   /* GEN11_GFX_DISABLE_LEGACY_MODE (Gen11+) */
#define EX_STOP_RING           (1u << 8)
#define EX_STAT_VALIDO0        (1u << 4)   /* RING_EXECLIST0_VALID */
#define EX_STAT_ATIVO0         (1u << 18)  /* RING_EXECLIST0_ACTIVE */
#define EX_EL_CTRL_LOAD        1u

/* Forcewake del dominio GT (Gen9 en adelante, i915 FORCEWAKE_GT_GEN9 / FORCEWAKE_ACK_GT_GEN9). */
#define EX_FW_GT          0xA188u
#define EX_FW_GT_ACK      0x130044u
#define EX_GT_THREAD      0x13805Cu
/* Vaciado de la GGTT tras cambiar sus entradas (GFX_FLSH_CNTL_GEN6, bit 0). */
#define EX_GFX_FLSH_CNTL  0x101008u

/* Estado de los contextos: página 0 = PPHWSP; página 1 = la imagen de registros (LRC_STATE). */
#define EX_LRC_PN          1u
/* Posiciones (en palabras desde el principio de la página de registros) de los valores que
 * el driver pone; las de i915 (gt/intel_lrc_reg.h: CTX_CONTEXT_CONTROL... = dirección + 1). */
#define EX_CTX_CONTROL     3u
#define EX_CTX_RING_HEAD   5u
#define EX_CTX_RING_TAIL   7u
#define EX_CTX_RING_START  9u
#define EX_CTX_RING_CTL    11u
#define EX_CTX_TIMESTAMP   0x23u
#define EX_CTX_PDP0_UDW    0x31u
#define EX_CTX_PDP0_LDW    0x33u
#define EX_CTX_LRI_HEADER  1u
#define EX_CTX_CONTROL_INHIBIT_SYN   (1u << 3)   /* CTX_CTRL_INHIBIT_SYN_CTX_SWITCH */
#define EX_CTX_CONTROL_RESTORE_INHIBIT (1u << 0) /* CTX_CTRL_ENGINE_CTX_RESTORE_INHIBIT */
#define EX_CTX_CONTROL_SAVE_INHIBIT  (1u << 2)   /* CTX_CTRL_ENGINE_CTX_SAVE_INHIBIT */
#define EX_CTX_CONTROL_RS_ENABLE     (1u << 1)   /* CTX_CTRL_RS_CTX_ENABLE */

/* La imagen de registros del BCS tal como la monta i915 (gt/intel_lrc.c, set_offsets() con las
 * tablas gen9_xcs_offsets, que también usa Gen11, y gen12_xcs_offsets; MIT). Codificación de la
 * tabla: NOP(x) = 0x80 | x salta x palabras; LRI(n) = 0x40 | n abre un MI_LOAD_REGISTER_IMM de n
 * registros con FORCE_POSTED; REG(x) = x >> 2 en un byte (x < 0x200); REG16(x) en dos bytes; 0 fin.
 * La dirección de cada registro es la base del motor + el desplazamiento (también en Gen11+, donde
 * además va LRM_CS_MMIO): D434 revisada, la primera versión ponía solo el desplazamiento. */
#define EXL_NOP(x)   (0x80u | (x))
#define EXL_LRI(n)   (0x40u | (n))
#define EXL_REG(x)   ((x) >> 2)
#define EXL_REG16(x) (((x) >> 9) | 0x80u), (((x) >> 2) & 0x7Fu)
static const uint8_t intel_lrc_gen9_xcs[] = {
    EXL_NOP(1), EXL_LRI(14),
    EXL_REG16(0x244), EXL_REG(0x034), EXL_REG(0x030), EXL_REG(0x038), EXL_REG(0x03c), EXL_REG(0x168), EXL_REG(0x140),
    EXL_REG(0x110), EXL_REG(0x11c), EXL_REG(0x114), EXL_REG(0x118), EXL_REG(0x1c0), EXL_REG(0x1c4), EXL_REG(0x1c8),
    EXL_NOP(3), EXL_LRI(9),
    EXL_REG16(0x3a8), EXL_REG16(0x28c), EXL_REG16(0x288), EXL_REG16(0x284), EXL_REG16(0x280), EXL_REG16(0x27c),
    EXL_REG16(0x278), EXL_REG16(0x274), EXL_REG16(0x270),
    EXL_NOP(13), EXL_LRI(1),
    EXL_REG16(0x200),
    EXL_NOP(13), EXL_LRI(44),
    EXL_REG(0x028), EXL_REG(0x09c), EXL_REG(0x0c0), EXL_REG(0x178), EXL_REG(0x17c), EXL_REG16(0x358), EXL_REG(0x170),
    EXL_REG(0x150), EXL_REG(0x154), EXL_REG(0x158), EXL_REG16(0x41c),
    EXL_REG16(0x600), EXL_REG16(0x604), EXL_REG16(0x608), EXL_REG16(0x60c), EXL_REG16(0x610), EXL_REG16(0x614),
    EXL_REG16(0x618), EXL_REG16(0x61c), EXL_REG16(0x620), EXL_REG16(0x624), EXL_REG16(0x628), EXL_REG16(0x62c),
    EXL_REG16(0x630), EXL_REG16(0x634), EXL_REG16(0x638), EXL_REG16(0x63c), EXL_REG16(0x640), EXL_REG16(0x644),
    EXL_REG16(0x648), EXL_REG16(0x64c), EXL_REG16(0x650), EXL_REG16(0x654), EXL_REG16(0x658), EXL_REG16(0x65c),
    EXL_REG16(0x660), EXL_REG16(0x664), EXL_REG16(0x668), EXL_REG16(0x66c), EXL_REG16(0x670), EXL_REG16(0x674),
    EXL_REG16(0x678), EXL_REG16(0x67c), EXL_REG(0x068),
    0
};
static const uint8_t intel_lrc_gen12_xcs[] = {
    EXL_NOP(1), EXL_LRI(13),
    EXL_REG16(0x244), EXL_REG(0x034), EXL_REG(0x030), EXL_REG(0x038), EXL_REG(0x03c), EXL_REG(0x168), EXL_REG(0x140),
    EXL_REG(0x110), EXL_REG(0x1c0), EXL_REG(0x1c4), EXL_REG(0x1c8), EXL_REG(0x180), EXL_REG16(0x2b4),
    EXL_NOP(5), EXL_LRI(9),
    EXL_REG16(0x3a8), EXL_REG16(0x28c), EXL_REG16(0x288), EXL_REG16(0x284), EXL_REG16(0x280), EXL_REG16(0x27c),
    EXL_REG16(0x278), EXL_REG16(0x274), EXL_REG16(0x270),
    0
};

/* La tabla de la generación (NULL si no es Gen9, Gen11 ni Gen12). */
static inline const uint8_t *intel_lrc_tabela(int gen)
{
    return gen == 12 ? intel_lrc_gen12_xcs : gen == 9 || gen == 11 ? intel_lrc_gen9_xcs : NULL;
}

/* D438: la imagen del motor de render (RCS) para el 3D de Gen9-12 (gpu3d_intel9.c). Las tablas
 * gen9_rcs_offsets, gen11_rcs_offsets y gen12_rcs_offsets de i915 (gt/intel_lrc.c, MIT), con la
 * misma codificación. Los primeros bloques son iguales a los del BCS, así que las posiciones de
 * CONTEXT_CONTROL, del anillo, del sello de tiempo y de la PML4 (EX_CTX_*) son las mismas; el RCS
 * añade R_PWR_CLK_STATE (0xc8) y, en Gen11+, 0x1b0 y otros. Los valores que no se ponen quedan a
 * 0: con el inhibidor de la carga el motor solo lee el contexto del anillo. */
static const uint8_t intel_lrc_gen9_rcs[] = {
    EXL_NOP(1), EXL_LRI(14),
    EXL_REG16(0x244), EXL_REG(0x034), EXL_REG(0x030), EXL_REG(0x038), EXL_REG(0x03c), EXL_REG(0x168), EXL_REG(0x140),
    EXL_REG(0x110), EXL_REG(0x11c), EXL_REG(0x114), EXL_REG(0x118), EXL_REG(0x1c0), EXL_REG(0x1c4), EXL_REG(0x1c8),
    EXL_NOP(3), EXL_LRI(9),
    EXL_REG16(0x3a8), EXL_REG16(0x28c), EXL_REG16(0x288), EXL_REG16(0x284), EXL_REG16(0x280), EXL_REG16(0x27c),
    EXL_REG16(0x278), EXL_REG16(0x274), EXL_REG16(0x270),
    EXL_NOP(13), 0x01u /* LRI(1) sin FORCE_POSTED */,
    EXL_REG(0x0c8),
    EXL_NOP(13), EXL_LRI(44),
    EXL_REG(0x028), EXL_REG(0x09c), EXL_REG(0x0c0), EXL_REG(0x178), EXL_REG(0x17c), EXL_REG16(0x358), EXL_REG(0x170),
    EXL_REG(0x150), EXL_REG(0x154), EXL_REG(0x158), EXL_REG16(0x41c),
    EXL_REG16(0x600), EXL_REG16(0x604), EXL_REG16(0x608), EXL_REG16(0x60c), EXL_REG16(0x610), EXL_REG16(0x614),
    EXL_REG16(0x618), EXL_REG16(0x61c), EXL_REG16(0x620), EXL_REG16(0x624), EXL_REG16(0x628), EXL_REG16(0x62c),
    EXL_REG16(0x630), EXL_REG16(0x634), EXL_REG16(0x638), EXL_REG16(0x63c), EXL_REG16(0x640), EXL_REG16(0x644),
    EXL_REG16(0x648), EXL_REG16(0x64c), EXL_REG16(0x650), EXL_REG16(0x654), EXL_REG16(0x658), EXL_REG16(0x65c),
    EXL_REG16(0x660), EXL_REG16(0x664), EXL_REG16(0x668), EXL_REG16(0x66c), EXL_REG16(0x670), EXL_REG16(0x674),
    EXL_REG16(0x678), EXL_REG16(0x67c), EXL_REG(0x068),
    0
};
static const uint8_t intel_lrc_gen11_rcs[] = {
    EXL_NOP(1), EXL_LRI(15),
    EXL_REG16(0x244), EXL_REG(0x034), EXL_REG(0x030), EXL_REG(0x038), EXL_REG(0x03c), EXL_REG(0x168), EXL_REG(0x140),
    EXL_REG(0x110), EXL_REG(0x11c), EXL_REG(0x114), EXL_REG(0x118), EXL_REG(0x1c0), EXL_REG(0x1c4), EXL_REG(0x1c8),
    EXL_REG(0x180),
    EXL_NOP(1), EXL_LRI(9),
    EXL_REG16(0x3a8), EXL_REG16(0x28c), EXL_REG16(0x288), EXL_REG16(0x284), EXL_REG16(0x280), EXL_REG16(0x27c),
    EXL_REG16(0x278), EXL_REG16(0x274), EXL_REG16(0x270),
    EXL_LRI(1), EXL_REG(0x1b0),
    EXL_NOP(10), 0x01u /* LRI(1) sin FORCE_POSTED */, EXL_REG(0x0c8),
    0
};
static const uint8_t intel_lrc_gen12_rcs[] = {
    EXL_NOP(1), EXL_LRI(13),
    EXL_REG16(0x244), EXL_REG(0x034), EXL_REG(0x030), EXL_REG(0x038), EXL_REG(0x03c), EXL_REG(0x168), EXL_REG(0x140),
    EXL_REG(0x110), EXL_REG(0x1c0), EXL_REG(0x1c4), EXL_REG(0x1c8), EXL_REG(0x180), EXL_REG16(0x2b4),
    EXL_NOP(5), EXL_LRI(9),
    EXL_REG16(0x3a8), EXL_REG16(0x28c), EXL_REG16(0x288), EXL_REG16(0x284), EXL_REG16(0x280), EXL_REG16(0x27c),
    EXL_REG16(0x278), EXL_REG16(0x274), EXL_REG16(0x270),
    EXL_LRI(3), EXL_REG(0x1b0), EXL_REG16(0x5a8), EXL_REG16(0x5ac),
    EXL_NOP(6), 0x01u /* LRI(1) sin FORCE_POSTED */, EXL_REG(0x0c8),
    EXL_NOP(3 + 9 + 1),
    EXL_LRI(51),
    EXL_REG16(0x588), EXL_REG16(0x588), EXL_REG16(0x588), EXL_REG16(0x588), EXL_REG16(0x588), EXL_REG16(0x588),
    EXL_REG(0x028), EXL_REG(0x09c), EXL_REG(0x0c0), EXL_REG(0x178), EXL_REG(0x17c), EXL_REG16(0x358), EXL_REG(0x170),
    EXL_REG(0x150), EXL_REG(0x154), EXL_REG(0x158), EXL_REG16(0x41c),
    EXL_REG16(0x600), EXL_REG16(0x604), EXL_REG16(0x608), EXL_REG16(0x60c), EXL_REG16(0x610), EXL_REG16(0x614),
    EXL_REG16(0x618), EXL_REG16(0x61c), EXL_REG16(0x620), EXL_REG16(0x624), EXL_REG16(0x628), EXL_REG16(0x62c),
    EXL_REG16(0x630), EXL_REG16(0x634), EXL_REG16(0x638), EXL_REG16(0x63c), EXL_REG16(0x640), EXL_REG16(0x644),
    EXL_REG16(0x648), EXL_REG16(0x64c), EXL_REG16(0x650), EXL_REG16(0x654), EXL_REG16(0x658), EXL_REG16(0x65c),
    EXL_REG16(0x660), EXL_REG16(0x664), EXL_REG16(0x668), EXL_REG16(0x66c), EXL_REG16(0x670), EXL_REG16(0x674),
    EXL_REG16(0x678), EXL_REG16(0x67c), EXL_REG(0x068), EXL_REG(0x084), EXL_NOP(1),
    0
};

/* La tabla del RCS de la generación (NULL si no es Gen9, Gen11 ni Gen12). */
static inline const uint8_t *intel_lrc_tabela_rcs(int gen)
{
    return gen == 12 ? intel_lrc_gen12_rcs : gen == 11 ? intel_lrc_gen11_rcs : gen == 9 ? intel_lrc_gen9_rcs : NULL;
}

/* Páginas de la imagen del RCS, con la PPHWSP (intel_engine_context_size de i915: Gen9 22, Gen11-12 14). */
static inline uint32_t intel_lrc_paginas_rcs(int gen) { return gen == 9 ? 22u : gen == 11 || gen == 12 ? 14u : 0u; }

/* Dónde va RING_MI_MODE en la imagen (lrc_ring_mi_mode de i915): Gen9-11 0x54, Gen12 0x60. */
static inline uint32_t intel_lrc_mi_mode(int gen) { return gen >= 12 ? 0x60u : 0x54u; }

/* Monta la imagen de registros (página 1 del contexto, «pag», 1024 palabras) como
 * __lrc_init_regs() de i915 con el contexto recién creado («inhibit»): todos los MI_LOAD_REGISTER_IMM
 * con sus direcciones (base del motor + desplazamiento), un MI_BATCH_BUFFER_END que cierra la imagen,
 * CONTEXT_CONTROL con el inhibidor de la carga del estado del motor, el anillo (cabeza, cola,
 * dirección y RING_CTL), la PML4 de la PPGTT en PDP0 (4 niveles) y RING_MI_MODE sin STOP_RING.
 * Devuelve false si la generación no es Gen9-12 o la página es pequeña. */
static inline bool intel_lrc_monta_tab(uint32_t *pag, uint32_t n_pag_palavras, const uint8_t *d, int gen, uint32_t base,
                                       uint32_t cabeca, uint32_t cauda, uint32_t inicio, uint32_t ctl_anel, uint64_t pml4)
{
    if (!d || n_pag_palavras < 1024 || (gen != 9 && gen != 11 && gen != 12)) return false;
    for (uint32_t i = 0; i < n_pag_palavras; i++) pag[i] = 0;
    uint32_t i = 0;
    while (*d) {
        if (*d & 0x80u) { i += *d++ & 0x7Fu; continue; }
        uint32_t cnt = *d & 0x3Fu, fl = (uint32_t)*d >> 6;
        d++;
        pag[i] = (0x22u << 23) | (2u * cnt - 1u);         /* MI_LOAD_REGISTER_IMM(cnt) */
        if (fl & 1u) pag[i] |= 1u << 12;                   /* MI_LRI_FORCE_POSTED */
        if (gen >= 11) pag[i] |= 1u << 19;                 /* MI_LRI_LRM_CS_MMIO */
        i++;
        while (cnt--) {
            uint32_t off = 0;
            uint8_t v;
            do { v = *d++; off = (off << 7) | (v & 0x7Fu); } while (v & 0x80u);
            pag[i] = base + (off << 2);
            i += 2;
        }
        if (i >= n_pag_palavras) return false;
    }
    pag[i] = (0x0Au << 23) | (gen >= 11 ? 1u : 0u);       /* MI_BATCH_BUFFER_END: cierra la imagen */
    /* Valores (init_common_regs, init_ppgtt_regs y __reset_stop_ring de i915). */
    uint32_t ctl = ((uint32_t)EX_CTX_CONTROL_INHIBIT_SYN << 16) | EX_CTX_CONTROL_INHIBIT_SYN |
                   ((uint32_t)EX_CTX_CONTROL_RESTORE_INHIBIT << 16) | EX_CTX_CONTROL_RESTORE_INHIBIT;
    if (gen < 11) ctl |= (uint32_t)(EX_CTX_CONTROL_SAVE_INHIBIT | EX_CTX_CONTROL_RS_ENABLE) << 16;
    pag[EX_CTX_CONTROL] = ctl;
    pag[EX_CTX_RING_HEAD] = cabeca;
    pag[EX_CTX_RING_TAIL] = cauda;
    pag[EX_CTX_RING_START] = inicio;
    pag[EX_CTX_RING_CTL] = ctl_anel;
    pag[EX_CTX_TIMESTAMP] = 0;
    pag[EX_CTX_PDP0_UDW] = (uint32_t)(pml4 >> 32);
    pag[EX_CTX_PDP0_LDW] = (uint32_t)pml4;
    pag[intel_lrc_mi_mode(gen) + 1] = EX_STOP_RING << 16;   /* STOP_RING borrado (con su máscara) */
    return true;
}

/* La del BCS (D434): la tabla xcs de la generación. */
static inline bool intel_lrc_monta(uint32_t *pag, uint32_t n_pag_palavras, int gen, uint32_t base, uint32_t cabeca,
                                   uint32_t cauda, uint32_t inicio, uint32_t ctl_anel, uint64_t pml4)
{
    return intel_lrc_monta_tab(pag, n_pag_palavras, intel_lrc_tabela(gen), gen, base, cabeca, cauda, inicio, ctl_anel, pml4);
}

/* El descriptor de 64 bits que se manda al motor (lrc_descriptor de i915): la dirección de la
 * imagen (4 KB alineada) en los bits bajos, válido, privilegiado y modo de direcciones de 4 niveles
 * (INTEL_LEGACY_64B_CONTEXT = 3, desplazado 3: la PPGTT de abajo); el identificador de contexto en la
 * mitad alta (Gen11+: SW ctx id en 37, instancia en 48 y clase en 61, COPY = 3). */
#define EX_CTX_VALIDO      (1u << 0)
#define EX_CTX_PRIVILEGIO  (1u << 8)
#define EX_CTX_MODO_64     (3u << 3)
#define EX_CTX_ID          1u
static inline uint64_t intel_ctx_descritor(uint32_t lrca, int gen, uint32_t ccid_clase)
{
    uint64_t d = (uint64_t)(lrca & 0xFFFFF000u) | EX_CTX_VALIDO | EX_CTX_PRIVILEGIO | EX_CTX_MODO_64;
    uint32_t alto;
    if (gen >= 11) alto = (EX_CTX_ID << (37 - 32)) | ((ccid_clase & 7u) << (61 - 32));   /* instancia 0 */
    else alto = EX_CTX_ID;
    return d | ((uint64_t)alto << 32);
}

/* ---- PPGTT (D434 revisada) ----
 * Con execlists las direcciones de las órdenes del blitter pasan por la PPGTT del contexto, no por
 * la GGTT: i915 carga siempre una PPGTT, también para los contextos «de GGTT» (vm_alias() en
 * gt/intel_lrc.c: la «aliasing PPGTT»). Aquí se monta una de 4 niveles que replica la GGTT solo en el
 * tramo que toca el blitter (la pantalla y la zona de prueba): PML4[0] -> PDP[0] -> PD -> tablas de
 * 2 MB. Entradas como gen8_pde_encode/gen8_pte_encode (presente, escritura) con PWT|PCD: índice 3 del
 * PAT, el que i915 deja sin caché (la pantalla no es coherente con la LLC). */
#define EX_PPGTT_BITS      (1u | 2u | (1u << 3) | (1u << 4))
#define EX_PPGTT_LIMITE    (1u << 30)       /* solo PDP[0]: direcciones gráficas por debajo de 1 GB */
static inline void intel_ppgtt_entrada(uint64_t fis, uint32_t *lo, uint32_t *hi)
{
    *lo = (uint32_t)(fis & 0xFFFFF000u) | EX_PPGTT_BITS;
    *hi = (uint32_t)((fis >> 32) & 0xFFFFu);
}

/* Cuántas tablas de páginas (2 MB cada una) hacen falta para [ini, fim). */
static inline uint32_t intel_ppgtt_n_pt(uint32_t ini, uint32_t fim)
{
    return fim > ini ? ((fim - 1u) >> 21) - (ini >> 21) + 1u : 0u;
}

/* Monta la PPGTT en «t» (palabras de 32 bits: 3 + n_pt páginas seguidas de 1024 palabras: PML4,
 * PDP, PD y las tablas), cuya primera página está en la física «f_t». Mapea cada página gráfica
 * ini + k*4096 de [ini, fim) a la física f_ini + k*4096 (la memoria robada es contigua: lo comprueba
 * el GTT). Lo demás, sin mapear. false si los límites no cuadran. La PML4 queda en la física «f_t». */
static inline bool intel_ppgtt_monta(uint32_t *t, uint32_t n_pt, uint64_t f_t, uint32_t ini, uint32_t fim,
                                     uint64_t f_ini)
{
    if ((ini & 0xFFFu) || (f_t & 0xFFFu) || (f_ini & 0xFFFu) || fim <= ini || fim > EX_PPGTT_LIMITE ||
        intel_ppgtt_n_pt(ini, fim) != n_pt)
        return false;
    uint32_t *pml4 = t, *pdp = t + 1024, *pd = t + 2048, *pt = t + 3072;
    for (uint32_t i = 0; i < (3u + n_pt) * 1024u; i++) t[i] = 0;
    intel_ppgtt_entrada(f_t + 4096u, &pml4[0], &pml4[1]);
    intel_ppgtt_entrada(f_t + 8192u, &pdp[0], &pdp[1]);
    uint32_t pd0 = ini >> 21;
    for (uint32_t j = 0; j < n_pt; j++)
        intel_ppgtt_entrada(f_t + 12288u + (uint64_t)j * 4096u, &pd[2 * (pd0 + j)], &pd[2 * (pd0 + j) + 1]);
    for (uint32_t g = ini; g < fim; g += 4096u) {
        uint32_t j = (g >> 21) - pd0, e = (g >> 12) & 511u;
        intel_ppgtt_entrada(f_ini + (uint64_t)(g - ini), &pt[j * 1024u + 2u * e], &pt[j * 1024u + 2u * e + 1u]);
    }
    return true;
}

/* El índice 3 del PAT sin caché (bdw/icl/tgl_setup_private_ppat de i915): el registro y el valor
 * que hay que escribir dado el que tiene. Gen9: GEN8_PRIVATE_PAT_LO, byte 3 = UC (0); Gen11:
 * GEN10_PAT_INDEX(3) = 0x40EC; Gen12: GEN12_PAT_INDEX(3) = 0x480C (UC = 0). */
static inline uint32_t intel_pat3_reg(int gen) { return gen >= 12 ? 0x480Cu : gen >= 11 ? 0x40ECu : 0x40E0u; }
static inline uint32_t intel_pat3_valor(int gen, uint32_t atual) { return gen >= 11 ? 0u : (atual & 0x00FFFFFFu); }

/* La secuencia de escrituras para mandar un descriptor (puerto 0; el 1 va vacío, como i915).
 * ELSP (Gen9-10): desplazamientos 0x230, de la parte alta a la baja del puerto 1 y luego del 0; el
 * último escrito dispara el envío. ELSQ (Gen11+): el puerto 1 (0x518/0x51C) y el 0 (0x510/0x514),
 * y después EXECLIST_CONTROL = EL_CTRL_LOAD. Devuelve el número de pares (off, val). */
static inline int intel_envio_seq(uint64_t desc, bool elsq, uint32_t *off, uint32_t *val)
{
    if (!elsq) {
        off[0] = EX_RING_ELSP; val[0] = 0;                              /* puerto 1, alta */
        off[1] = EX_RING_ELSP; val[1] = 0;                              /* puerto 1, baja */
        off[2] = EX_RING_ELSP; val[2] = (uint32_t)(desc >> 32);         /* puerto 0, alta */
        off[3] = EX_RING_ELSP; val[3] = (uint32_t)desc;                 /* puerto 0, baja: dispara */
        return 4;
    }
    off[0] = EX_RING_ELSQ + 8; val[0] = 0;                              /* puerto 1, baja */
    off[1] = EX_RING_ELSQ + 12; val[1] = 0;                             /* puerto 1, alta */
    off[2] = EX_RING_ELSQ; val[2] = (uint32_t)desc;                     /* puerto 0, baja */
    off[3] = EX_RING_ELSQ + 4; val[3] = (uint32_t)(desc >> 32);         /* puerto 0, alta */
    return 4;
}

/* El estado de los puertos (EXECLIST_STATUS_LO, bits de i915 para Gen8): ¿queda algo válido o
 * activo en el puerto 0? Solo para el log y para Gen9: en Gen11+ el registro no está documentado
 * en i915 y el fin de tanda se decide con la marca y el CSB. */
static inline bool intel_exec_ocupado(uint32_t stat) { return (stat & (EX_STAT_VALIDO0 | EX_STAT_ATIVO0)) != 0; }

/* El CSB: 6 entradas en Gen9-10, 12 desde Gen11 (GEN8_CSB_ENTRIES, GEN11_CSB_ENTRIES). */
static inline uint32_t intel_csb_tam(int gen) { return gen >= 11 ? 12u : 6u; }

/* La copia del CSB en los registros del motor (intel_lrc_reg.h de i915: GEN8_EXECLISTS_STATUS_BUF
 * 0x370 para las entradas 0-5 y GEN11_EXECLISTS_STATUS_BUF2 0x3C0 para las 6-11), 8 bytes cada una:
 * desplazamiento de la palabra baja desde la base del motor. Se usa solo si la de la memoria no llega. */
static inline uint32_t intel_csb_mmio(uint32_t j) { return j < 6 ? 0x370u + 8u * j : 0x3C0u + 8u * (j - 6u); }

/* El puntero del CSB (RING_CONTEXT_STATUS_PTR): escritura en los bits 2:0 (Gen9-10) o 3:0
 * (Gen11+), lectura en 10:8 o 11:8. */
static inline void intel_csb_ptrs(uint32_t reg, int gen, uint32_t *escrita, uint32_t *lectura)
{
    uint32_t mw = gen >= 11 ? 0xFu : 0x7u;
    *escrita = reg & mw;
    *lectura = (reg >> 8) & mw;
}

/* Cuántas entradas nuevas del CSB hay (de la lectura a la escritura, en anillo). Un puntero fuera
 * del anillo (el valor de después de un reinicio) cuenta como «ninguna». */
static inline uint32_t intel_csb_novas(uint32_t escrita, uint32_t lectura, int gen)
{
    uint32_t tam = intel_csb_tam(gen);
    if (escrita >= tam || lectura >= tam) return 0;
    return (escrita + tam - lectura) % tam;
}

/* La palabra con máscara que avisa al hardware de que la lectura del CSB llegó a «lectura»:
 * la mitad alta es la máscara del campo de lectura y la baja el valor (como i915 con
 * _MASKED_FIELD). */
static inline uint32_t intel_csb_avisar(uint32_t lectura, int gen)
{
    uint32_t mw = gen >= 11 ? 0xFu : 0x7u;
    return (mw << 24) | ((lectura & mw) << 8);
}

/* La puesta a cero de los punteros (reset_csb_pointers de i915): lectura y escritura en la última
 * entrada, de forma que la primera que escribe el hardware es la 0. */
static inline uint32_t intel_csb_reiniciar(int gen)
{
    uint32_t r = intel_csb_tam(gen) - 1u;
    return 0xFFFFu << 16 | r << 8 | r;
}

/* ¿Esta entrada del CSB dice que el motor dejó el contexto y quedó libre? Gen9-11 (gen8_csb_parse):
 * ACTIVE_IDLE (bit 3) en la palabra baja. Gen12 (gen12_csb_parse, otro formato): el contexto al que
 * se cambia (bits 25:15 de la palabra baja) es el 0x7FF («ninguno») y el que se deja (25:15 de la
 * alta) es uno de verdad. */
static inline bool intel_csb_terminou(uint32_t baixo, uint32_t alto, int gen)
{
    if (gen >= 12) return ((baixo >> 15) & 0x7FFu) == 0x7FFu && ((alto >> 15) & 0x7FFu) != 0x7FFu;
    return (baixo & (1u << 3)) != 0;
}

/* La marca de fin de tanda: MI_FLUSH_DW con escritura de una palabra en la GGTT («dir»: alineada
 * a 8 y con el bit 5 a 0, como gen8_emit_ggtt_write de i915). 4 palabras. */
static inline int intel_cmd_marca(uint32_t *b, uint32_t dir, uint32_t valor)
{
    b[0] = (0x26u << 23) | 2u | (1u << 14);   /* (MI_FLUSH_DW + 1) | MI_FLUSH_DW_OP_STOREDW */
    b[1] = (dir & ~7u) | (1u << 2);           /* MI_FLUSH_DW_USE_GTT */
    b[2] = 0;
    b[3] = valor;
    return 4;
}

/* El pie de la PTE de la GGTT (8 bytes, como Gen8-12): la dirección física de la página en
 * 45:12, con los bits de la plantilla (las banderas de la entrada de la pantalla) en los bajos.
 * «lo» y «hi» son las dos palabras de la entrada. */
static inline void intel_pte_gen9(uint64_t fis, uint32_t molde_lo, uint32_t *lo, uint32_t *hi)
{
    *lo = (uint32_t)(fis & 0xFFFFF000u) | (molde_lo & 0xFFEu) | 1u;   /* bit 0: válida */
    *hi = (uint32_t)((fis >> 32) & 0x3FFFu);
}

/* La memoria robada de Gen9 en adelante (early-quirks.c de Linux: gen9_stolen_size y, desde Gen11,
 * gen11_stolen_base). Tamaño: GMS (GMCH_CTRL 15:8) en pasos de 32 MB hasta 0xEF, y de 4 MB desde
 * 0xF0 (empezando en 4 MB). Base: BDSM (0x5C) en Gen9; en Gen11+ el registro de 64 bits 0xC0/0xC4. */
static inline bool intel_robada9(int gen, uint32_t gmch_ctrl, uint32_t bsm_lo, uint32_t bsm_hi, uint64_t *base,
                                 uint64_t *tam)
{
    uint32_t gms = (gmch_ctrl >> 8) & 0xFFu;
    *tam = gms < 0xF0u ? (uint64_t)gms << 25 : ((uint64_t)(gms - 0xF0u) << 22) + (4ull << 20);
    *base = (uint64_t)(bsm_lo & 0xFFF00000u) | (gen >= 11 ? (uint64_t)bsm_hi << 32 : 0);
    return *base != 0 && *tam != 0;
}

/* La zona reservada de la memoria robada en Gen9+ (GEN6_STOLEN_RESERVED, 0x1082C0, y en Gen11+ su
 * mitad alta 0x1082C4): Gen9 como Broadwell (habilitada en el bit 0; de la base al final de la robada);
 * Gen11+ como icl_get_stolen_reserved (sin bit de habilitar; la base de 64 bits). Aquí, por prudencia,
 * siempre de la base al final de la robada «topo» (el tamaño de los bits 8:7 cae dentro). */
static inline bool intel_reservado9(int gen, uint32_t lo, uint32_t hi, uint64_t topo, uint64_t *base, uint32_t *tam)
{
    if (gen < 11 && !(lo & 1u)) return false;
    *base = (uint64_t)(lo & 0xFFF00000u) | (gen >= 11 ? (uint64_t)hi << 32 : 0);
    if (!*base) return false;
    uint64_t fim = topo > *base ? topo : *base + (8ull << 20);
    uint64_t t = fim - *base;
    *tam = t > 0xFFFFF000ull ? 0xFFFFF000u : (uint32_t)t;
    return true;
}

/* El plano universal de Gen9 en adelante (PLANE_CTL, PLANE_STRIDE en unidades de 64 bytes,
 * PLANE_OFFSET 0): mismas direcciones que el DSPCNTR de Gen4-8, otro formato de bits (i915_reg.h:
 * formato en 27:24, ampliado a 27:23 desde Ice Lake; ORDER_RGBX en el bit 20; tiling en 12:10;
 * compresión en el 15 y, desde Tiger Lake, la de medios en el 4; stride en 11:0). Lo que no sea XRGB8888 (BGRX en memoria) lineal y sin
 * comprimir con el pitch de la pantalla, la CPU. */
#define EX_PLANO_LIGADO    (1u << 31)
#define EX_PLANO_FORMATO(c) (((c) >> 24) & 0xFu)
#define EX_PLANO_XRGB8888  4u
#define EX_PLANO_TILING(c) (((c) >> 10) & 0x7u)
#define EX_PLANO_RGBX      (1u << 20)
static inline int intel_plano9_conferir(int gen, uint32_t cntr, uint32_t surf, uint32_t stride, uint32_t offset,
                                        uint32_t fb_gfx, uint32_t pitch)
{
    if (!(cntr & EX_PLANO_LIGADO) || (surf & ~0xFFFu) != fb_gfx) return INTEL_PLANO_OUTRO;
    if (EX_PLANO_TILING(cntr) != 0) return INTEL_PLANO_EM_TILES;
    if (EX_PLANO_FORMATO(cntr) != EX_PLANO_XRGB8888 || (cntr & EX_PLANO_RGBX) || (cntr & (1u << 15)) ||
        (gen >= 11 && (cntr & (1u << 23))) || (gen >= 12 && (cntr & (1u << 4))))
        return INTEL_PLANO_FORMATO_MAU;
    if (offset & 0x1FFF1FFFu) return INTEL_PLANO_OUTRO;   /* x/y de la superficie: la pantalla empieza en 0,0 */
    if (((stride & 0xFFFu) * 64u) != pitch) return INTEL_PLANO_PITCH_MAU;
    return INTEL_PLANO_OK;
}

#endif
