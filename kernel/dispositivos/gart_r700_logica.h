// SPDX-License-Identifier: GPL-2.0-or-later
/* gart_r700_logica.h - la GART (memoria virtual, contexto 0) de las Radeon R7xx (RV710, RV730,
 * RV740, RV770), D406. La parte pura, sin hardware, para probarla en el anfitrión
 * (tests/gart_r700_unidad.c, tests/h406_gart_r700.py) con el mismo código que compila el kernel.
 *
 * Qué es: una ventana del mapa de direcciones del controlador de memoria (MC) de la GPU, fuera de la
 * VRAM, cuyas páginas de 4 KB apuntan a páginas de la RAM del PC por una tabla de entradas de 64 bits
 * que vive en la VRAM. Con ella el procesador de órdenes (CP_DMA) puede dejar el cuadro del 3D en la RAM
 * («la vuelta», como la D391 de la X1400), en vez de que la CPU lo lea de la VRAM sin caché por el PCIe.
 *
 * Números y secuencias de Linux 6.6, drivers/gpu/drm/radeon (solo valores; código propio, D073):
 *   - rv770_pcie_gart_enable / rv770_pcie_gart_disable (rv770.c), r600_pcie_gart_tlb_flush (r600.c);
 *   - registros de rv770d.h y r600d.h; bits de las entradas R600_PTE_* (radeon.h) y
 *     rs600_gart_get_page_entry (rs600.c), que es la que usan las R6xx/R7xx;
 *   - dónde va la ventana: radeon_gtt_location (radeon_device.c): justo detrás de la VRAM (o
 *     delante, si hay más sitio).
 * Lo que NO se hace (Linux lo hace en rv770_mc_program, con el MC parado): cambiar la apertura del
 * sistema, MC_VM_FB_LOCATION ni la AGP. Se leen y, si no cuadran con lo que la GART necesita, no se
 * enciende. */
#ifndef SAGUI_GART_R700_LOGICA_H
#define SAGUI_GART_R700_LOGICA_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

/* Registros (rv770d.h / r600d.h de Linux 6.6). */
#define R7G_VM_L2_CNTL                 0x1400u
#define   R7G_ENABLE_L2_CACHE          (1u << 0)
#define   R7G_ENABLE_L2_FRAGMENT       (1u << 1)
#define   R7G_L2_PTE_LRU_POR_ESCRITA   (1u << 9)
#define   R7G_L2_QUEUE(x)              (((x) & 7u) << 14)
#define R7G_VM_L2_CNTL2                0x1404u
#define R7G_VM_L2_CNTL3                0x1408u
#define   R7G_BANK_SELECT(x)           ((x) << 0)
#define   R7G_CACHE_UPDATE_MODE(x)     ((x) << 6)
#define R7G_VM_L2_STATUS               0x140Cu
#define R7G_VM_CONTEXT0_CNTL           0x1410u      /* y los contextos 1 a 6, de 4 en 4 */
#define   R7G_ENABLE_CONTEXT           (1u << 0)
#define   R7G_PAGE_TABLE_DEPTH(x)      (((x) & 3u) << 1)
#define   R7G_RANGE_FAULT_DEFAULT      (1u << 4)
#define R7G_VM_CONTEXT0_REQUEST_RESPONSE 0x1470u
#define   R7G_REQUEST_TYPE(x)          ((x) & 0xFu)
#define   R7G_RESPONSE_MASK            0xF0u
#define   R7G_RESPONSE_SHIFT           4
#define R7G_VM_CONTEXT0_INVAL_LO       0x1490u
#define R7G_VM_CONTEXT0_INVAL_HI       0x14B0u
#define R7G_VM_CONTEXT0_FAULT_DEFAULT  0x1518u
#define R7G_VM_CONTEXT0_PT_BASE        0x153Cu
#define R7G_VM_CONTEXT0_PT_START       0x155Cu
#define R7G_VM_CONTEXT0_PT_END         0x157Cu
#define R7G_MC_VM_FB_LOCATION          0x2024u
#define R7G_MC_VM_AGP_TOP              0x2028u
#define R7G_MC_VM_AGP_BOT              0x202Cu
#define R7G_MC_VM_AGP_BASE             0x2030u
#define R7G_MC_VM_SYS_APER_LOW         0x2034u
#define R7G_MC_VM_SYS_APER_HIGH        0x2038u
#define R7G_MC_VM_SYS_APER_DEFAULT     0x203Cu
#define R7G_MC_VM_MB_L1_TLB0_CNTL      0x2234u      /* TLB1 0x2238, TLB2 0x223C, TLB3 0x2240 */
#define R7G_MC_VM_MD_L1_TLB0_CNTL      0x2654u      /* TLB1 0x2658, TLB2 0x265C */
#define R7G_MC_VM_MD_L1_TLB3_CNTL      0x2698u      /* solo RV740 */
#define   R7G_ENABLE_L1_TLB            (1u << 0)
#define   R7G_ENABLE_L1_FRAGMENT       (1u << 1)
#define   R7G_ACCESO_NO_EN_SISTEMA     (3u << 3)    /* SYSTEM_ACCESS_MODE_NOT_IN_SYS */
#define   R7G_APERTURA_SIN_MAPA_PASA   (0u << 5)    /* SYSTEM_APERTURE_UNMAPPED_ACCESS_PASS_THRU */
#define   R7G_L1_TLB_SIZE(x)           ((x) << 15)
#define   R7G_L1_QUEUE(x)              ((x) << 18)

/* Entradas de la tabla (R600_PTE_*, radeon.h). */
#define R7G_PTE_VALIDA     (1u << 0)
#define R7G_PTE_SISTEMA    (1u << 1)
#define R7G_PTE_SNOOP      (1u << 2)
#define R7G_PTE_LECTURA    (1u << 5)
#define R7G_PTE_ESCRITURA  (1u << 6)

/* Una entrada que apunta a la página física «fis» de la RAM. Con snoop: la CPU ve lo que escribe la GPU
 * sin vaciar su caché (la RAM es write-back). La de relleno, como la «dummy» de Linux: solo SISTEMA, sin
 * VALIDA, así que un acceso a ella es un fallo que el MC manda a la página de relleno por defecto. */
static inline uint64_t r7g_pte(uint64_t fis) { return (fis & ~0xFFFull) | R7G_PTE_SISTEMA | R7G_PTE_VALIDA | R7G_PTE_SNOOP | R7G_PTE_LECTURA | R7G_PTE_ESCRITURA; }
static inline uint64_t r7g_pte_relleno(uint64_t fis) { return (fis & ~0xFFFull) | R7G_PTE_SISTEMA; }

/* La VRAM en el mapa del MC según MC_VM_FB_LOCATION (bits 15:0 inicio, 31:16 fin, en unidades de
 * 16 MB): [ini, fim], fin incluido. false si no tiene sentido. */
static inline bool r7g_fb(uint32_t loc, uint64_t *ini, uint64_t *fim)
{
    uint64_t i = (uint64_t)(loc & 0xFFFFu) << 24, f = ((uint64_t)(loc >> 16) << 24) | 0xFFFFFFull;
    if (loc == 0xFFFFFFFFu || f <= i) return false;
    *ini = i; *fim = f;
    return true;
}

/* ¿[a, b] (incluido) se cruza con [c, d]? */
static inline bool r7g_cruza(uint64_t a, uint64_t b, uint64_t c, uint64_t d) { return a <= d && c <= b; }

/* Dónde va una ventana de «bytes» (múltiplo de 4 KB): detrás de la VRAM, alineada a su tamaño (o, si no
 * cabe por debajo de 4 GB, delante), por debajo de 4 GB, sin cruzarse con la apertura del sistema (que
 * con SYSTEM_ACCESS_MODE_NOT_IN_SYS va directa, sin la tabla) ni con la AGP. La apertura del sistema tiene
 * que cubrir TODA la VRAM: si no, lo que la pantalla o el DMA del 2D leen de la VRAM fuera de ella pasaría
 * por la tabla. Si no la cubre, «*mudar_aper» dice que hay que ponerla como Linux (rv770_mc_program: LOW =
 * inicio de la VRAM, HIGH = fin, en páginas de 4 KB) ANTES de encender las TLB, y las cuentas se hacen con
 * esa. Devuelve 0 si vale y, si no, un código (para el log):
 *   1 VRAM sin sentido, 3 sin sitio por debajo de 4 GB,
 *   4 la ventana caería en la apertura del sistema, 5 la ventana caería en la AGP. */
static inline int r7g_janela(uint32_t fb_loc, uint32_t aper_lo, uint32_t aper_hi, uint32_t agp_bot, uint32_t agp_top,
                             uint32_t bytes, uint64_t *ini, bool *mudar_aper)
{
    uint64_t vi, vf;
    if (!r7g_fb(fb_loc, &vi, &vf)) return 1;
    uint64_t ai = (uint64_t)aper_lo << 12, af = ((uint64_t)aper_hi << 12) | 0xFFFu;
    *mudar_aper = ai > vi || af < vf || aper_lo > aper_hi;
    if (*mudar_aper) { ai = vi; af = vf; }
    uint64_t g;
    uint64_t al = bytes;
    g = (vf + 1 + al - 1) / al * al;
    if (g + bytes > 0x100000000ull) {
        if (vi < bytes) return 3;
        g = (vi - bytes) / al * al;
    }
    if (r7g_cruza(g, g + bytes - 1, ai, af)) return 4;
    /* AGP en unidades de 64 KB (rv770_mc_program); sin AGP Linux deja BOT = TOP = 0x0FFFFFFF (por encima de
     * 4 GB) y entonces no se cruza. */
    uint64_t gb = (uint64_t)agp_bot << 16, gt = ((uint64_t)agp_top << 16) | 0xFFFFu;
    if (gb <= gt && r7g_cruza(g, g + bytes - 1, gb, gt)) return 5;
    *ini = g;
    return 0;
}

static inline const char *r7g_janela_motivo(int c)
{
    switch (c) {
    case 0: return "bien";
    case 1: return "MC_VM_FB_LOCATION sin sentido";
    case 3: return "no hay sitio para la ventana por debajo de 4 GB";
    case 4: return "la ventana caeria en la apertura del sistema";
    case 5: return "la ventana caeria en la AGP";
    }
    return "?";
}

/* La apertura del sistema como la pone Linux (rv770_mc_program, sin AGP): toda la VRAM. */
static inline void r7g_aper(uint32_t fb_loc, uint32_t *lo, uint32_t *hi)
{
    uint64_t vi = 0, vf = 0;
    r7g_fb(fb_loc, &vi, &vf);
    *lo = (uint32_t)(vi >> 12); *hi = (uint32_t)(vf >> 12);
}

/* Las escrituras para encenderla, en el orden de rv770_pcie_gart_enable (sin la de la TLB, que va
 * aparte). «fim» es la última dirección de la ventana (incluida); «tabela» la dirección en el MC de la
 * tabla (VRAM, alineada a 4 KB); «relleno» la física de la página de relleno. Devuelve cuántas. */
typedef struct { uint32_t reg, valor; } r7g_par_t;
#define R7G_MAX_PARES 24
static inline uint32_t r7g_l1(void)
{
    return R7G_ENABLE_L1_TLB | R7G_ENABLE_L1_FRAGMENT | R7G_ACCESO_NO_EN_SISTEMA | R7G_APERTURA_SIN_MAPA_PASA |
           R7G_L1_TLB_SIZE(5) | R7G_L1_QUEUE(5);
}
static inline int r7g_ligar(bool rv740, uint64_t ini, uint64_t fim, uint64_t tabela, uint64_t relleno, r7g_par_t *p)
{
    int n = 0;
    uint32_t l1 = r7g_l1();
    p[n++] = (r7g_par_t){ R7G_VM_L2_CNTL, R7G_ENABLE_L2_CACHE | R7G_ENABLE_L2_FRAGMENT | R7G_L2_PTE_LRU_POR_ESCRITA | R7G_L2_QUEUE(7) };
    p[n++] = (r7g_par_t){ R7G_VM_L2_CNTL2, 0 };
    p[n++] = (r7g_par_t){ R7G_VM_L2_CNTL3, R7G_BANK_SELECT(0) | R7G_CACHE_UPDATE_MODE(2) };
    p[n++] = (r7g_par_t){ R7G_MC_VM_MD_L1_TLB0_CNTL, l1 };
    p[n++] = (r7g_par_t){ R7G_MC_VM_MD_L1_TLB0_CNTL + 4, l1 };
    p[n++] = (r7g_par_t){ R7G_MC_VM_MD_L1_TLB0_CNTL + 8, l1 };
    if (rv740) p[n++] = (r7g_par_t){ R7G_MC_VM_MD_L1_TLB3_CNTL, l1 };
    for (uint32_t i = 0; i < 4; i++) p[n++] = (r7g_par_t){ R7G_MC_VM_MB_L1_TLB0_CNTL + 4 * i, l1 };
    p[n++] = (r7g_par_t){ R7G_VM_CONTEXT0_PT_START, (uint32_t)(ini >> 12) };
    p[n++] = (r7g_par_t){ R7G_VM_CONTEXT0_PT_END, (uint32_t)(fim >> 12) };
    p[n++] = (r7g_par_t){ R7G_VM_CONTEXT0_PT_BASE, (uint32_t)(tabela >> 12) };
    p[n++] = (r7g_par_t){ R7G_VM_CONTEXT0_CNTL, R7G_ENABLE_CONTEXT | R7G_PAGE_TABLE_DEPTH(0) | R7G_RANGE_FAULT_DEFAULT };
    p[n++] = (r7g_par_t){ R7G_VM_CONTEXT0_FAULT_DEFAULT, (uint32_t)(relleno >> 12) };
    for (uint32_t i = 1; i < 7; i++) p[n++] = (r7g_par_t){ R7G_VM_CONTEXT0_CNTL + 4 * i, 0 };
    return n;
}

/* Los registros que r7g_ligar escribe (para guardarlos antes y dejarlos como estaban al apagar). */
static inline int r7g_regs_tocados(bool rv740, uint32_t *r)
{
    r7g_par_t p[R7G_MAX_PARES];
    int n = r7g_ligar(rv740, 0, 0, 0, 0, p);
    for (int i = 0; i < n; i++) r[i] = p[i].reg;
    return n;
}

/* Las de apagarla, como rv770_pcie_gart_disable: los 7 contextos a 0, la L2 sin caché y las L1 sin TLB. */
static inline int r7g_desligar(r7g_par_t *p)
{
    int n = 0;
    uint32_t l1 = R7G_L1_TLB_SIZE(5) | R7G_L1_QUEUE(5);
    for (uint32_t i = 0; i < 7; i++) p[n++] = (r7g_par_t){ R7G_VM_CONTEXT0_CNTL + 4 * i, 0 };
    p[n++] = (r7g_par_t){ R7G_VM_L2_CNTL, R7G_ENABLE_L2_FRAGMENT | R7G_L2_QUEUE(7) };
    p[n++] = (r7g_par_t){ R7G_VM_L2_CNTL2, 0 };
    p[n++] = (r7g_par_t){ R7G_VM_L2_CNTL3, R7G_BANK_SELECT(0) | R7G_CACHE_UPDATE_MODE(2) };
    for (uint32_t i = 0; i < 3; i++) p[n++] = (r7g_par_t){ R7G_MC_VM_MD_L1_TLB0_CNTL + 4 * i, l1 };
    for (uint32_t i = 0; i < 4; i++) p[n++] = (r7g_par_t){ R7G_MC_VM_MB_L1_TLB0_CNTL + 4 * i, l1 };
    return n;
}

#endif
