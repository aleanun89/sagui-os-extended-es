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
};

/* Generación del blitter de cada variante (la que va en la tabla de acel2d.c). */
static inline int intel_var_geracao(int var)
{
    return var == INTEL_VAR_BDW ? 8 : var == INTEL_VAR_IVB || var == INTEL_VAR_HSW ? 7 : 0;
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
    static const struct { uint16_t de, ate; uint8_t gen; const char *nome; } f[] = {
        { 0x0F30, 0x0F33, 7, "Valleyview (Atom Bay Trail)" },
        { 0x0155, 0x0155, 7, "Valleyview (Atom Bay Trail)" },
        { 0x0157, 0x0157, 7, "Valleyview (Atom Bay Trail)" },
        { 0x22B0, 0x22B3, 8, "Cherryview (Atom Braswell)" },
        { 0x1900, 0x193F, 9, "Skylake" },
        { 0x5900, 0x593F, 9, "Kaby Lake" },
        { 0x87C0, 0x87CA, 9, "Amber Lake" },
        { 0x3E90, 0x3EA9, 9, "Coffee Lake" },
        { 0x9B21, 0x9BCC, 9, "Comet Lake" },
        { 0x0A84, 0x0A84, 9, "Apollo Lake" },
        { 0x1A84, 0x1A85, 9, "Apollo Lake" },
        { 0x5A84, 0x5A85, 9, "Apollo Lake" },
        { 0x3184, 0x3185, 9, "Gemini Lake" },
        { 0x8A50, 0x8A71, 11, "Ice Lake" },
        { 0x4500, 0x4571, 11, "Elkhart Lake" },
        { 0x4E51, 0x4E71, 11, "Jasper Lake" },
        { 0x9A40, 0x9AF8, 12, "Tiger Lake" },
        { 0x4C80, 0x4C9A, 12, "Rocket Lake" },
        { 0x4680, 0x469F, 12, "Alder Lake-S" },
        { 0x46A0, 0x46C3, 12, "Alder Lake-P" },
        { 0x46D0, 0x46D4, 12, "Alder Lake-N" },
        { 0xA720, 0xA720, 12, "Raptor Lake-P" },
        { 0xA780, 0xA78B, 12, "Raptor Lake-S" },
        { 0xA7A0, 0xA7AD, 12, "Raptor Lake-P" },
        { 0x4905, 0x4909, 12, "DG1 (Iris Xe dedicada)" },
        { 0x7D40, 0x7DD5, 12, "Meteor Lake" },
    };
    for (size_t i = 0; i < sizeof f / sizeof f[0]; i++)
        if (m >= f[i].de && m <= f[i].ate) {
            if (nome) *nome = f[i].nome;
            if (porque)
                *porque = f[i].gen == 7 || f[i].gen == 8
                    ? "Atom: otro reparto de registros, sin 2D (D359)"
                    : "el 2D necesita execlists, aun no (D359)";
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

#endif
