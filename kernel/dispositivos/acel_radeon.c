// SPDX-License-Identifier: GPL-2.0-or-later
/* acel_radeon.c - el motor 2D clásico de las Radeon R100 a R500 (D195).
 *
 * SIN PROBAR EN HARDWARE: QEMU no emula ninguna Radeon. La capa común solo lo
 * activa si su autoprueba del arranque coincide con la CPU y termina a tiempo.
 * El objetivo es la Mobility Radeon X1400 (RV515/M54, 0x7145) del mantenedor.
 *
 * Desde la R100 hasta la R500 las Radeon tienen el mismo motor 2D, que se
 * puede programar escribiendo sus registros por MMIO (BAR 2), sin el
 * procesador de comandos ni microcódigo: es lo que hacía el driver «radeon» de
 * XFree86/X.org sin CP. Una operación:
 *   DP_GUI_MASTER_CNTL  qué se hace: destino (y origen) con su pitch/offset,
 *                       32 bits por píxel, color sólido o memoria, ROP 0xF0 o 0xCC;
 *   DST_PITCH_OFFSET    (pitch / 64) << 22 | (dirección en la GPU) >> 10;
 *   DP_BRUSH_FRGD_CLR   el color del relleno;
 *   DP_CNTL             la dirección del recorrido (izquierda a derecha, arriba
 *                       a abajo, o al revés): así las copias solapadas salen bien,
 *                       empezando por la esquina que no pisa el origen;
 *   SRC_Y_X, DST_Y_X    las esquinas; DST_HEIGHT_WIDTH lanza la operación.
 * Antes de escribir hay que mirar que quepa en la FIFO del motor (RBBM_STATUS,
 * bits 6:0) y para esperar, que GUI_ACTIVE (bit 31) se apague y vaciar la caché
 * de destino del 2D.
 *
 * La dirección de la pantalla «para la GPU» (en su mapa de memoria, el MC):
 *   - R5xx (AVIVO): la que tiene el controlador de pantalla activo,
 *     D1GRPH_PRIMARY_SURFACE_ADDRESS (o D2): el VBE la dejó apuntando a la pantalla;
 *   - R100-R4xx: el inicio de la VRAM en el MC (MC_FB_LOCATION) más el
 *     desplazamiento de la pantalla dentro del BAR 0.
 *
 * Referencia documental: los registros públicos del driver radeon (radeon_reg.h
 * de Linux y de xf86-video-ati); el código es propio. Qué hay que comprobar en
 * la X1400: docs/decisoes.md, D195.
 *
 * D387: en las R5xx con PCI Express, además, el volcado del búfer de atrás a la
 * pantalla por la GART PCIE (al final del archivo). D391: y la vuelta del cuadro
 * del 3D (VRAM) a la RAM del programa por la misma GART (región 2, con escritura
 * solo mientras dura cada copia). */
#include "acel2d.h"
#include "console.h"
#include "memoria.h"
#include "paginacao.h"
#include "texto.h"
#include "video.h"
#include "volcado_logica.h"
#include "vram_zona_logica.h"
#include "rastro.h"
#include "prazo.h"
#include "sensores_conv.h"
#include "radeon2d_ordens.h"

#define MMIO_TAM               0x10000
#define RBBM_STATUS            0x0E40
#define   RBBM_FIFOCNT         0x007Fu
#define   RBBM_ACTIVE          0x80000000u
#define MC_FB_LOCATION         0x0148
#define CRTC_OFFSET            0x0224
#define CRTC_PITCH             0x022C      /* bits 10:0: pitch en grupos de 8 píxeles */
#define DP_SRC_FRGD_CLR        0x15D8
#define DP_SRC_BKGD_CLR        0x15DC
#define DP_CNTL                0x16C0
#define   DST_X_ESQ_DIR        0x1u
#define   DST_Y_CIMA_BAIXO     0x2u
#define DP_WRITE_MASK          0x16CC
#define DEFAULT_SC_BOTTOM_RIGHT 0x16E8
#define SC_TOP_LEFT            0x16EC
#define SC_BOTTOM_RIGHT        0x16F0
#define R300_DSTCACHE_CTLSTAT  0x1714
#define RB3D_DSTCACHE_CTLSTAT  0x325C
#define   DC_FLUSH_ALL         0xFu
#define   DC_BUSY              0x80000000u
#define D1GRPH_ENABLE          0x6100
#define D1GRPH_PRIMARY_SURFACE 0x6110
#define D1GRPH_PITCH           0x6120
#define D2_DESLOC              0x0800

/* Las órdenes del 2D (registros, GMC y ROP): radeon2d_ordens.h. */

static volatile uint8_t *mmio;
static uint32_t mmio_fis, pitch_offset;
static int gen;
/* D387: lo que el volcado necesita saber de ra_iniciar. */
static pci_disp_t ra_disp;
static acel_tela_t ra_tela;
static uint32_t ra_bar0, ra_tela_mc;

static inline uint32_t rd(uint32_t r)             { return *(volatile uint32_t *)(mmio + r); }
static inline void     wr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(mmio + r) = v; }

/* D387 (Amilo, 2026-11-19): con el CP del 3D en marcha (gpu3d_r500.c), el 2D va por su anillo, nunca por
 * MMIO (ver radeon2d_ordens.h). gpu3d_r500_2d pone la orden (PACKET0 + vaciado del 2D + WAIT_UNTIL) sin
 * avisar al CP; gpu3d_r500_2d_esperar avisa, espera la valla y el motor parado. */
bool gpu3d_r500_cp_em_marcha(void);
bool gpu3d_r500_2d(const ra2d_ordem_t *o);
bool gpu3d_r500_2d_esperar(uint32_t ms);
bool gpu3d_r500_opcao(int paso);   /* pasos 27-35: la valla del 3D y su espera (gpu3d_r500.c) */
static uint32_t gv_opc;          /* dbg5: opciones de depuración (abajo) */
#define GVO_MMIO      0x20u      /* «acel2d volcado paso 26»: el 2D por MMIO aunque el CP esté en marcha (lo de antes) */

static bool fifo(uint32_t n);
static bool ra_pelo_cp(void)
{
    static bool avisado;
    bool cp = gen == 2 && !(gv_opc & GVO_MMIO) && gpu3d_r500_cp_em_marcha();
    if (cp != avisado) {
        avisado = cp;
        klog("[ACEL2D] radeon: %s\n", cp ? "el CP del 3D esta en marcha: el 2D va por su anillo (PACKET0), no por MMIO (D387)"
                                         : "el 2D vuelve a ir por MMIO (el CP no esta en marcha o se pidio MMIO)");
    }
    return cp;
}

/* Una orden del 2D: por el anillo del CP si está en marcha; si no, sus registros por MMIO (holgura: «folga»
 * entradas libres de la FIFO de más, como antes). */
static bool ra_mandar(const ra2d_ordem_t *o, uint32_t folga)
{
    if (!ra2d_ok(o)) return false;
    if (ra_pelo_cp()) return gpu3d_r500_2d(o);
    if (!fifo((uint32_t)o->n + folga)) return false;
    for (int i = 0; i < o->n; i++) *(volatile uint32_t *)(mmio + o->e[i].reg) = o->e[i].val;
    return true;
}

static bool fifo(uint32_t n)
{
    acel_prazo_t p;
    acel_prazo(&p, 250);
    while ((rd(RBBM_STATUS) & RBBM_FIFOCNT) < n)
        if (acel_prazo_esgotado(&p)) return false;
    return true;
}

static bool ra_esperar_prazo(uint32_t ms)
{
    acel_prazo_t p;
    if (ra_pelo_cp()) {   /* D387: la valla del CP (con el 2D y el 3D vaciados y parados) y el motor parado */
        if (gpu3d_r500_2d_esperar(ms ? ms : 250)) return true;
        klog("[ACEL2D] radeon: el CP no termina el 2D (RBBM_STATUS %x)\n", rd(RBBM_STATUS));
        return false;
    }
    if (!fifo(64)) { klog("[ACEL2D] radeon: la FIFO no se vacia (RBBM_STATUS %x)\n", rd(RBBM_STATUS)); return false; }
    if (ms) acel_prazo_longo(&p, ms); else acel_prazo(&p, 250);
    while (rd(RBBM_STATUS) & RBBM_ACTIVE)
        if (acel_prazo_esgotado(&p)) { klog("[ACEL2D] radeon: el motor no para (RBBM_STATUS %x)\n", rd(RBBM_STATUS)); return false; }
    uint32_t cache = gen >= 1 ? R300_DSTCACHE_CTLSTAT : RB3D_DSTCACHE_CTLSTAT;
    wr(cache, rd(cache) | DC_FLUSH_ALL);
    acel_prazo(&p, 250);
    while (rd(cache) & DC_BUSY)
        if (acel_prazo_esgotado(&p)) { klog("[ACEL2D] radeon: la cache 2D no se vacia (%x)\n", rd(cache)); return false; }
    return true;
}

static bool ra_esperar(void) { return ra_esperar_prazo(0); }

static bool ra_preencher_po(uint32_t dst_po, int x, int y, int l, int a, uint32_t cor)
{
    ra2d_ordem_t o;
    ra2d_preencher(&o, dst_po, x, y, l, a, cor);
    return ra_mandar(&o, 0);
}

static bool ra_preencher(int x, int y, int l, int a, uint32_t cor) { return ra_preencher_po(pitch_offset, x, y, l, a, cor); }

static bool ra_copiar(int ox, int oy, int dx, int dy, int l, int a)
{
    /* Si el destino está a la derecha (abajo) del origen, se recorre de derecha a
     * izquierda (de abajo arriba) empezando por la última columna (fila). */
    uint32_t dir = 0;
    if (ox >= dx) dir |= DST_X_ESQ_DIR; else { ox += l - 1; dx += l - 1; }
    if (oy >= dy) dir |= DST_Y_CIMA_BAIXO; else { oy += a - 1; dy += a - 1; }
    ra2d_ordem_t o;
    ra2d_copiar(&o, pitch_offset, pitch_offset, dir, ox, oy, dx, dy, l, a, false, false);
    return ra_mandar(&o, 0);
}

static void ra_desligar(void)
{
    /* El motor 2D no toca la pantalla por su cuenta: basta con no usarlo más. */
    klog("[ACEL2D] radeon: motor 2D sin usar\n");
}

static uint32_t ler_bar(const pci_disp_t *d, uint8_t reg, bool *alto)
{
    uint32_t v = pci_le32(d, reg);
    *alto = (v & 1) == 0 && ((v >> 1) & 3) == 2 && pci_le32(d, (uint8_t)(reg + 4)) != 0;
    return v;
}

static bool ra_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *m, int nm)
{
    gen = geracao;
    /* El motor 2D solo toca la VRAM de la tarjeta: no hace falta el bus maestro.
     * La memoria ya la tiene activa (la capa común lo comprueba: el VBE la usa). */
    uint32_t cmd = pci_le32(d, 0x04);
    if (!(cmd & 0x02)) pci_escreve32(d, 0x04, (cmd & 0xFFFFu) | 0x02);
    bool alto0, alto2;
    uint32_t bar0 = ler_bar(d, 0x10, &alto0), bar2 = ler_bar(d, 0x18, &alto2);
    klog("[ACEL2D] radeon: generacion %d (%s), BAR0 %x, BAR2 %x, pantalla %x, pitch %u, %u KB del VBE\n", gen,
         gen >= 2 ? "R5xx" : gen == 1 ? "R300/R420" : "R100/R200", bar0, bar2, t->fb_fisico, t->pitch, t->memoria >> 10);
    if ((bar2 & 1) || alto2 || alto0 || !(bar2 & ~0xFu)) { snformatar(m, (size_t)nm, "BAR 2 no es MMIO de 32 bits (%x)", bar2); return false; }
    bar0 &= ~0xFu;
    if (t->fb_fisico < bar0) { snformatar(m, (size_t)nm, "la pantalla (%x) no esta en el BAR 0 (%x)", t->fb_fisico, bar0); return false; }
    if (t->pitch & 63 || t->pitch / 64 > 0xFF) { snformatar(m, (size_t)nm, "pitch %u no es multiplo de 64", t->pitch); return false; }
    if (!mmio || mmio_fis != (bar2 & ~0xFu)) {
        mmio_fis = bar2 & ~0xFu;
        mmio = (volatile uint8_t *)janela_mapear_tipo(mmio_fis, MMIO_TAM, PG_PCD | PG_PWT);
    }
    if (!mmio) { snformatar(m, (size_t)nm, "sin ventana para el MMIO"); return false; }
    uint32_t est = rd(RBBM_STATUS);
    if (est == 0xFFFFFFFFu) { snformatar(m, (size_t)nm, "el MMIO no responde (RBBM_STATUS %x)", est); return false; }
    acel2d_vram_real(rd(0x00F8), "CONFIG_MEMSIZE");   /* D387: en bytes de la R100 a la R500 */
    /* La dirección de la pantalla en el MC. */
    uint32_t mc;
    if (gen >= 2) {
        uint32_t e1 = rd(D1GRPH_ENABLE), s1 = rd(D1GRPH_PRIMARY_SURFACE), p1 = rd(D1GRPH_PITCH);
        uint32_t e2 = rd(D1GRPH_ENABLE + D2_DESLOC), s2 = rd(D1GRPH_PRIMARY_SURFACE + D2_DESLOC), p2 = rd(D1GRPH_PITCH + D2_DESLOC);
        klog("[ACEL2D] radeon: D1 %s superficie %x pitch %u; D2 %s superficie %x pitch %u\n",
             e1 & 1 ? "activo" : "apagado", s1, p1, e2 & 1 ? "activo" : "apagado", s2, p2);
        /* El controlador activo cuyo pitch (en píxeles) es el del VBE: si ninguno
         * coincide, la pantalla no es la que se cree y no se toca el motor. */
        uint32_t pp = t->pitch / 4;
        if ((e1 & 1) && p1 == pp) mc = s1;
        else if ((e2 & 1) && p2 == pp) mc = s2;
        else {
            snformatar(m, (size_t)nm, "ningun controlador AVIVO activo con el pitch del VBE (%u pixeles)", pp);
            return false;
        }
    } else {
        uint32_t loc = rd(MC_FB_LOCATION), crtc = rd(CRTC_OFFSET), cp = rd(CRTC_PITCH);
        mc = ((loc & 0xFFFFu) << 16) + (t->fb_fisico - bar0);
        klog("[ACEL2D] radeon: MC_FB_LOCATION %x, CRTC_OFFSET %x, CRTC_PITCH %x\n", loc, crtc, cp);
        /* El controlador tiene que mostrar con el pitch del VBE: si no, la
         * pantalla no es la que se cree y no se toca el motor. */
        if ((cp & 0x7FFu) * 8 * 4 != t->pitch) {
            snformatar(m, (size_t)nm, "CRTC_PITCH %x no es el pitch del VBE (%u bytes)", cp, t->pitch);
            return false;
        }
    }
    if (mc & 0x3FF) { snformatar(m, (size_t)nm, "la pantalla en el MC (%x) no esta alineada a 1 KB", mc); return false; }
    pitch_offset = ((t->pitch / 64) << 22) | (mc >> 10);
    ra_disp = *d; ra_tela = *t; ra_bar0 = bar0; ra_tela_mc = mc;   /* D387 */
    klog("[ACEL2D] radeon: pantalla en el MC %x, DST_PITCH_OFFSET %x, RBBM_STATUS %x\n", mc, pitch_offset, est);
    if (!ra_esperar()) { snformatar(m, (size_t)nm, "el motor 2D no esta libre (RBBM_STATUS %x)", rd(RBBM_STATUS)); return false; }
    /* Lo que el resto de órdenes da por hecho: sin recorte y todos los bits. */
    if (!fifo(6)) { snformatar(m, (size_t)nm, "la FIFO del motor no responde"); return false; }
    wr(DEFAULT_SC_BOTTOM_RIGHT, (0x1FFFu << 16) | 0x1FFFu);
    wr(SC_TOP_LEFT, 0);
    wr(SC_BOTTOM_RIGHT, (0x1FFFu << 16) | 0x1FFFu);
    wr(DP_WRITE_MASK, 0xFFFFFFFFu);
    wr(DP_SRC_FRGD_CLR, 0xFFFFFFFFu);
    wr(DP_SRC_BKGD_CLR, 0);
    if (!ra_esperar()) { snformatar(m, (size_t)nm, "el motor 2D no responde"); return false; }
    return true;
}

/* ================================================================ D387: GART PCIE
 *
 * El volcado del búfer de atrás (RAM) a la pantalla por el motor 2D. La GPU no ve la RAM del sistema
 * por sí sola: en las R300-R5xx con PCI Express hay una GART en el puente PCIE de la tarjeta (la «PCIE
 * GART» de rv370_pcie_gart_* en Linux, drivers/gpu/drm/radeon/r300.c): una ventana del mapa de la GPU
 * (MC) en la que cada página de 4 KB se traduce, por una tabla en la VRAM, a una página física de la
 * RAM. El motor 2D copia desde esa ventana como desde la VRAM (SRC_PITCH_OFFSET con la dirección de
 * la ventana), igual que hace Linux al mover búferes con r100_copy_blit.
 *
 * Lo que se toca, y en qué orden (el de rv370_pcie_gart_enable de Linux 6.6):
 *   - nada si el modelo no es una R5xx discreta de la lista, si no es PCI Express (las AGP con puente
 *     no tienen esta GART), si la GART ya estaba encendida (la usa otro: no se toca) o si no cuadra la
 *     VRAM (MC_FB_LOCATION, la pantalla en el BAR 0 y en el MC, CONFIG_MEMSIZE/APER_SIZE);
 *   - la tabla (16 KB: 4096 páginas, 16 MB de ventana) va en la VRAM, en las 64 filas que hay detrás
 *     de la zona de prueba del 2D (las que la capa deja a los drivers; el 3D empieza más allá, D386),
 *     comprobando que no pisa los cursores por hardware;
 *   - la ventana va en el mapa del MC delante (o detrás) de la VRAM sin pisar la VRAM ni la AGP; no se
 *     cambia el MC (MC_FB_LOCATION, MC_AGP_LOCATION) ni nada de la pantalla, relojes o energía;
 *   - las páginas no encontradas leen la VRAM (TX_DISCARD_RD_ADDR) y fuera de la ventana se descarta:
 *     un error de la tabla da píxeles malos (la autoprueba lo ve), nunca escrituras en la RAM: el
 *     motor 2D solo LEE de la ventana (el destino es siempre la VRAM);
 *   - el bus maestro del PCI se enciende DESPUÉS de la GART (sin GART el puente deja pasar las
 *     direcciones tal cual) y se apaga ANTES que ella; al apagar, los registros de la GART vuelven a
 *     sus valores de antes.
 * Las entradas son «con snoop» (sin el bit 0): el puente mira la caché de la CPU, así que no hace
 * falta vaciar nada antes de que la GPU lea lo que la CPU acaba de escribir en el búfer.
 *
 * D391, la excepción a «el motor 2D solo lee»: la vuelta del 3D. La capa común apunta las páginas del
 * destino (el búfer del programa, ya comprobadas: presentes, de RAM, suyas y con escritura) CON
 * ESCRITURA en la región 2 (detrás de la región 0), el motor copia del destino del 3D (VRAM) a ellas
 * y, tras esperar a que termine, las entradas vuelven a la página de relleno solo de lectura. La
 * escritura «con snoop» invalida las líneas de la caché de la CPU: la CPU lee después lo que escribió
 * la GPU. La lectura del estado del motor (MMIO) no adelanta a las escrituras de la tarjeta en el bus
 * (orden de PCI Express: una respuesta no pasa a las escrituras anteriores en el mismo sentido), así
 * que al ver el motor parado y la caché vaciada, lo escrito ya está en la RAM.
 * Sin probar en hardware: qué hay que mirar en el Amilo, docs/decisoes.md (D387). */

#define PCIE_INDEX            0x0030
#define PCIE_DATA             0x0034
#define PCIE_TX_GART_CNTL     0x10
#define   GART_EN             0x1u
#define   GART_DESCARTAR      (3u << 1)        /* lo que cae fuera de la ventana se descarta */
#define   GART_INVALIDAR_TLB  (1u << 8)
#define PCIE_TX_DISCARD_RD_LO 0x11
#define PCIE_TX_DISCARD_RD_HI 0x12
#define PCIE_TX_GART_BASE     0x13
#define PCIE_TX_GART_START_LO 0x14
#define PCIE_TX_GART_START_HI 0x15
#define PCIE_TX_GART_END_LO   0x16
#define PCIE_TX_GART_END_HI   0x17
#define PCIE_TX_GART_ERROR    0x18
#define MC_IND_INDEX          0x0070
#define MC_IND_DATA           0x0074
#define CONFIG_MEMSIZE        0x00F8
#define CONFIG_APER_SIZE      0x0108
#define D1CUR_CONTROL         0x6400
#define D1CUR_SURFACE         0x6408
#define GTT_BYTES             (16u << 20)
#define GTT_PAG               4096u
#define TAB_BYTES             (GTT_PAG * 4u)
#define REG1_PAG              3584u            /* la región de la autoprueba: las últimas 512 páginas */
/* Filas por orden del motor. dbg4 (Amilo): 64, no 128. Lo que se ha visto ir en la X1400 son copias de hasta 64
 * filas (pasos 5 y 6, la autoprueba); el primer volcado de la pantalla entera (6 franjas de 128 filas, 512 KB de
 * la RAM cada una) colgó el equipo dentro de gv_volcar. Hasta saber por qué, la forma ya probada. */
#define FAIXA_FILAS           64

/* Las R5xx discretas (como la lista de gpu3d_r500.c; sin las integradas RS600/RS690, que tienen
 * otra GART). rv515: 1 = RV515/RV505/RV516 (MC_FB_LOCATION en el índice 1 del MC), 0 = R520,
 * RV530, RV560, RV570, R580 (índice 4). */
static const struct { uint16_t de, ate; uint8_t rv515; } r5_modelos[] = {
    { 0x7100, 0x710F, 0 }, { 0x7140, 0x715F, 1 }, { 0x7180, 0x719F, 1 }, { 0x71C0, 0x71DE, 0 },
    { 0x7200, 0x7211, 1 }, { 0x7240, 0x724F, 0 }, { 0x7280, 0x7280, 0 }, { 0x7284, 0x7284, 0 },
    { 0x7288, 0x728C, 0 }, { 0x7290, 0x7297, 0 },
};

static struct {
    bool ligada, bm_nosso;
    bool aparcada;                    /* revisión: encendida con todo a la página de relleno tras un cuelgue */
    uint32_t orig[9];                 /* PCIE_TX_GART_CNTL ... ERROR antes de tocarlos */
    uint32_t gtt_ini, tab_mc, fb_ini, fb_fim, agp;
    volatile uint32_t *tab;
    uint32_t tab_fis;
    uint32_t pte_vazia;               /* página de relleno (como la dummy page de Linux) */
    uint32_t npag[2];
    volcado_fonte_t f[2];
    uint32_t r2_ini, npag2;           /* D391: región de escritura (páginas desde el principio de la ventana) */
    /* dbg5: lo que las opciones de depuración cambian (para devolverlo en gv_soltar) y dónde cabe un destino
     * de prueba del tamaño de la pantalla fuera de todo lo que se muestra. */
    int rv515;
    bool pll_mudado, agp_mudado;
    uint32_t pll_orig[3], agp_orig;
    uint32_t tab_off, vram_util;
} gt;

/* ------------------------------------------------------------ dbg5 (Amilo, 2026-11-19): opciones de depuración
 *
 * «acel2d volcado paso 20» las borra todas; «paso 21» a «paso 25» cambian una (y dicen cómo quedan). Valen para
 * los pasos siguientes y para «acel2d volcado encender», hasta reiniciar. Cada una prueba una hipótesis del
 * cuelgue del primer volcado de la pantalla entera:
 *   21 RELOJ   fuerza encendido el reloj de los bloques CP, E2 (el motor 2D) e IDCT (bit 0 de los registros
 *              0x0F, 0x11 y 0x13 del PLL), como hace SIEMPRE rv515_clock_startup del driver radeon («We need to
 *              force on some of the block»). Con los relojes dinámicos que deja la BIOS de un portátil, un motor
 *              que espera a que la RAM le llegue por PCI Express puede quedarse sin reloj a mitad de una copia
 *              larga; con el origen en la VRAM nunca espera tanto.
 *   22 AGP     MC_AGP_LOCATION = 0xFFFFFFFF (rv515_mc_program en una tarjeta que no es AGP); la BIOS deja
 *              0x003F0000 (una «ventana AGP» de 0 a 4 MB en el mapa de la GPU).
 *   23 SNOOP   entradas de la GART sin snoop (bit 0) y wbinvd antes de cada orden (la caché de la CPU a la RAM).
 *   24 CIEGO   en los pasos con espera por franja: 20 ms sin tocar la tarjeta antes de leer RBBM_STATUS.
 *   25 MARCAS  las marcas en pantalla también con el motor ocupado (como dbg3/dbg4: la CPU escribe en la VRAM
 *              mientras el motor lee la RAM). Sin ella solo se pintan con el motor parado; al rastro van siempre. */
#define GVO_RELOJ     0x01u
#define GVO_AGP       0x02u
#define GVO_SEM_SNOOP 0x04u
#define GVO_CEGO      0x08u
#define GVO_MARCAS    0x10u
/* 26 MMIO   (D387, arreglo del 3D con el volcado) el 2D por MMIO aunque el CP esté en marcha, como antes del
 *           arreglo: solo para comparar en el Amilo (con el 3D encendido y el volcado, el motor acababa colgado). */
bool acel_radeon_dbg_marcas(void) { return (gv_opc & GVO_MARCAS) != 0; }

#define CLOCK_CNTL_INDEX 0x0008
#define CLOCK_CNTL_DATA  0x000C
static uint32_t pll_rd(uint32_t r)
{
    *(volatile uint8_t *)(mmio + CLOCK_CNTL_INDEX) = (uint8_t)(r & 0x3F);
    return rd(CLOCK_CNTL_DATA);
}
static void pll_wr(uint32_t r, uint32_t v)
{
    *(volatile uint8_t *)(mmio + CLOCK_CNTL_INDEX) = (uint8_t)((r & 0x3F) | 0x80u);
    wr(CLOCK_CNTL_DATA, v);
    *(volatile uint8_t *)(mmio + CLOCK_CNTL_INDEX) = (uint8_t)(r & 0x3F);
}
static const uint8_t pll_forcar[3] = { 0x0F, 0x11, 0x13 };   /* CP_DYN_CNTL, E2_DYN_CNTL, IDCT_DYN_CNTL: bit 0 FORCEON */

static uint32_t pcie_rd(uint32_t r)             { wr(PCIE_INDEX, r & 0x7FFu); return rd(PCIE_DATA); }
static void     pcie_wr(uint32_t r, uint32_t v) { wr(PCIE_INDEX, r & 0x7FFu); wr(PCIE_DATA, v); }
static uint32_t mc_rd(uint32_t r)
{
    wr(MC_IND_INDEX, 0x7F0000u | (r & 0xFFFFu));
    uint32_t v = rd(MC_IND_DATA);
    wr(MC_IND_INDEX, 0);
    return v;
}

static void mc_wr(uint32_t r, uint32_t v)   /* como rv515_mc_wreg */
{
    wr(MC_IND_INDEX, 0xFF0000u | (r & 0xFFFFu));
    wr(MC_IND_DATA, v);
    wr(MC_IND_INDEX, 0);
}

/* Linux lo hace dos veces (fallo del hardware) y lee de vuelta. */
static void gart_tlb(void)
{
    for (int i = 0; i < 2; i++) {
        uint32_t c = pcie_rd(PCIE_TX_GART_CNTL);
        pcie_wr(PCIE_TX_GART_CNTL, c | GART_INVALIDAR_TLB);
        (void)pcie_rd(PCIE_TX_GART_CNTL);
        pcie_wr(PCIE_TX_GART_CNTL, c);
    }
}

/* Capacidad del PCI (0x10 PCI Express, 0x02 AGP): true si la tiene. */
static bool pci_cap(const pci_disp_t *d, uint8_t id)
{
    if (!((pci_le32(d, 0x04) >> 16) & 0x10)) return false;   /* sin lista de capacidades */
    uint8_t p = (uint8_t)(pci_le32(d, 0x34) & 0xFC);
    for (int n = 0; p >= 0x40 && n < 48; n++) {
        uint32_t v = pci_le32(d, p);
        if ((v & 0xFF) == id) return true;
        p = (uint8_t)((v >> 8) & 0xFC);
    }
    return false;
}

static uint8_t pci_cap_pos(const pci_disp_t *d, uint8_t id)
{
    if (!((pci_le32(d, 0x04) >> 16) & 0x10)) return 0;
    uint8_t p = (uint8_t)(pci_le32(d, 0x34) & 0xFC);
    for (int n = 0; p >= 0x40 && n < 48; n++) {
        uint32_t v = pci_le32(d, p);
        if ((v & 0xFF) == id) return p;
        p = (uint8_t)((v >> 8) & 0xFC);
    }
    return 0;
}

/* El puerto raíz por encima de la tarjeta. */
static bool gv_ponte(pci_disp_t *p)
{
    static pci_disp_t v[64];
    int n = pci_listar(v, 64);
    for (int i = 0; i < n; i++)
        if (v[i].classe == 0x06 && v[i].subclasse == 0x04 && ((pci_le32(&v[i], 0x18) >> 8) & 0xFF) == ra_disp.barramento) {
            *p = v[i];
            return true;
        }
    return false;
}

/* dbg5: solo lectura. PCI Express de la tarjeta y de su puerto raíz (control y ESTADO del dispositivo: bits 0-3
 * del estado = error corregible, no fatal, fatal, petición no soportada; control y estado del enlace: ASPM en
 * los bits 1:0 del control), las ventanas del puente y los relojes dinámicos (PLL). */
static void gv_pcie_log(const char *quando)
{
    pci_disp_t pt;
    bool tem = gv_ponte(&pt);
    for (int k = 0; k < 2; k++) {
        const pci_disp_t *d = k ? &pt : &ra_disp;
        if (k && !tem) break;
        uint8_t c = pci_cap_pos(d, 0x10);
        uint32_t cmd = pci_le32(d, 0x04);
        if (!c) { klog("[ACEL2D] radeon: PCIe (%s): %s sin capacidad PCI Express; orden %x estado %x\n", quando, k ? "puente" : "tarjeta",
                       cmd & 0xFFFF, cmd >> 16); continue; }
        uint32_t dc = pci_le32(d, (uint8_t)(c + 8)), lc = pci_le32(d, (uint8_t)(c + 0x10));
        klog("[ACEL2D] radeon: PCIe (%s): %s %x:%x.%x orden %x estado %x; DevCap %x DevCtl %x DevSta %x; LnkCap %x LnkCtl %x LnkSta %x\n",
             quando, k ? "puente" : "tarjeta", d->barramento, d->dispositivo, d->funcao, cmd & 0xFFFF, cmd >> 16,
             pci_le32(d, (uint8_t)(c + 4)), dc & 0xFFFF, dc >> 16, pci_le32(d, (uint8_t)(c + 0x0C)), lc & 0xFFFF, lc >> 16);
        if (k)
            klog("[ACEL2D] radeon: PCIe (%s): puente: buses %x, ventana de memoria %x, prefetch %x (%x:%x), control %x, estado secundario %x\n",
                 quando, pci_le32(d, 0x18) & 0xFFFFFF, pci_le32(d, 0x20), pci_le32(d, 0x24), pci_le32(d, 0x28), pci_le32(d, 0x2C),
                 pci_le32(d, 0x3C) >> 16, pci_le32(d, 0x1C) >> 16);
    }
    klog("[ACEL2D] radeon: relojes (%s): PLL 0D %x, 0F CP_DYN %x, 11 E2_DYN %x, 13 IDCT_DYN %x, 14 %x (bit 0 = forzado encendido, "
         "bit 2 = reloj en marcha); opciones de depuracion %x\n", quando, pll_rd(0x0D), pll_rd(0x0F), pll_rd(0x11), pll_rd(0x13),
         pll_rd(0x14), gv_opc);
}

static void gv_detalle(const char *fase)
{
    if (!mmio) return;
    uint32_t r[9];
    for (int i = 0; i < 9; i++) r[i] = pcie_rd(PCIE_TX_GART_CNTL + (uint32_t)i);
    klog("[ACEL2D] radeon: GART (%s): CNTL %x DISCARD %x:%x BASE %x START %x:%x END %x:%x ERROR %x; RBBM_STATUS %x; PCI orden %x\n",
         fase, r[0], r[2], r[1], r[3], r[5], r[4], r[7], r[6], r[8], rd(RBBM_STATUS), pci_le32(&ra_disp, 0x04) & 0xFFFF);
    if (gt.tab)
        klog("[ACEL2D] radeon: GART: tabla en MC %x (fisica %x), entradas 0 %x 1 %x %u %x; region 0 %u paginas en MC %x, "
             "region 1 %u en MC %x\n", gt.tab_mc, gt.tab_fis, gt.tab[0], gt.tab[1], REG1_PAG, gt.tab[REG1_PAG], gt.npag[0],
             gt.f[0].gtt_base, gt.npag[1], gt.f[1].gtt_base);
    if (gt.tab)   /* D391: la región de escritura tiene que estar vacía (solo lectura) fuera de una vuelta */
        klog("[ACEL2D] radeon: GART: region 2 (escritura) desde la pagina %u (MC %x), %u apuntadas, entrada %x\n", gt.npag[0],
             gt.gtt_ini + gt.npag[0] * 4096u, gt.npag2, gt.npag[0] < REG1_PAG ? gt.tab[gt.npag[0]] : 0);
}

/* Revisión (depurador-drivers): todas las entradas a la página de relleno. La GART de la R5xx no mira los bits
 * de lectura/escritura de las entradas salvo con PCIE_TX_GART_CHK_RW_VALID_EN (bit 5 de CNTL), que Linux no
 * enciende (rv370_pcie_gart_enable, r300.c, 6.6): una entrada «solo de lectura» sigue siendo escribible por
 * el motor. Lo que no se usa, por tanto, se deja apuntando a la página de relleno (nuestra, nunca liberada). */
static void gart_vaciar(uint32_t de, uint32_t n)
{
    if (!gt.tab || !n) return;
    for (uint32_t i = 0; i < n; i++) gt.tab[de + i] = gt.pte_vazia;
    (void)gt.tab[de + n - 1];   /* empuja las escrituras antes de vaciar la TLB */
    gart_tlb();
}

static void gv_soltar(void)
{
    if (!gt.ligada || !mmio) return;
    /* Revisión: si el motor no está parado (cuelgue, plazo agotado, órdenes en la FIFO), apagar la GART
     * dejaría pasar sus escrituras pendientes tal cual (la dirección del MC de la ventana, bf000000 en el
     * Amilo, como dirección física: RAM o MMIO de otro), y el registro de órdenes del PCI se restaura después
     * con el bus maestro que tuviera. Se deja la GART ENCENDIDA y aparcada: todas las entradas a la página de
     * relleno y lo de fuera de la ventana descartado. No se vuelve a usar en esta sesión (D244: tras un
     * cuelgue el 2D ya no vuelve). */
    if (ra_pelo_cp()) (void)ra_esperar_prazo(250);   /* D387: lo que quede en el anillo, antes de mirar el motor */
    uint32_t est = rd(RBBM_STATUS);
    if ((est & RBBM_ACTIVE) || (est & RBBM_FIFOCNT) < 64) {
        gart_vaciar(0, GTT_PAG);
        if (gt.bm_nosso) {
            uint32_t cmd = pci_le32(&ra_disp, 0x04);
            pci_escreve32(&ra_disp, 0x04, (cmd & 0xFFFFu) & ~0x04u);
        }
        gt.ligada = gt.bm_nosso = false;
        gt.aparcada = true;
        gt.npag[0] = gt.npag[1] = gt.npag2 = 0;
        klog("[ACEL2D] radeon: GART aparcada (el motor no esta parado, RBBM_STATUS %x): encendida con todo a la pagina "
             "de relleno; no se apaga en esta sesion\n", est);
        return;
    }
    /* Primero el bus maestro (si lo encendimos): con la GART apagada el puente vuelve a dejar pasar las
     * direcciones tal cual. */
    if (gt.bm_nosso) {
        uint32_t cmd = pci_le32(&ra_disp, 0x04);
        pci_escreve32(&ra_disp, 0x04, (cmd & 0xFFFFu) & ~0x04u);
    }
    /* Como rv370_pcie_gart_disable, y después los valores de antes. */
    pcie_wr(PCIE_TX_GART_START_LO, 0);
    pcie_wr(PCIE_TX_GART_END_LO, 0);
    pcie_wr(PCIE_TX_GART_START_HI, 0);
    pcie_wr(PCIE_TX_GART_END_HI, 0);
    uint32_t c = pcie_rd(PCIE_TX_GART_CNTL);
    pcie_wr(PCIE_TX_GART_CNTL, (c | GART_DESCARTAR) & ~GART_EN);
    gart_tlb();
    for (uint32_t i = 1; i < 9; i++) pcie_wr(PCIE_TX_GART_CNTL + i, gt.orig[i]);
    pcie_wr(PCIE_TX_GART_CNTL, gt.orig[0] & ~GART_EN);
    /* dbg5: lo que cambiaron las opciones de depuración, como estaba (el motor está parado). */
    if (gt.agp_mudado) { mc_wr(gt.rv515 ? 0x02 : 0x05, gt.agp_orig); gt.agp_mudado = false; }
    if (gt.pll_mudado) {
        for (int i = 0; i < 3; i++) if (!(gt.pll_orig[i] & 1u)) pll_wr(pll_forcar[i], gt.pll_orig[i]);
        gt.pll_mudado = false;
    }
    gt.ligada = gt.bm_nosso = false;
    gt.npag[0] = gt.npag[1] = gt.npag2 = 0;
    klog("[ACEL2D] radeon: GART apagada y como estaba (CNTL %x)\n", pcie_rd(PCIE_TX_GART_CNTL));
}

/* Depuración en el Amilo (rama v8.5.4.10r2-volcado-amilo): «acel2d volcado paso N» hace la puesta en marcha
 * de la GART solo hasta el paso N y la deshace. Cada paso deja su línea en el log ANTES de hacer lo
 * arriesgado: si el equipo se cuelga en el paso N, el log guardado tras el N-1 dice todo lo de antes.
 *   0  solo lee (MC, CONFIG, registros de la GART, PCI) y calcula ventana y tabla; no escribe nada
 *   1  + la tabla en la VRAM (todas las entradas a la página de relleno) y la lee de vuelta
 *   2  + los registros de la GART SIN encenderla (CNTL solo «descartar»), leídos de vuelta
 *   3  + GART_EN y vaciado de la TLB
 *   4  + el bus maestro del PCI
 *   5  + una copia de 64 píxeles de UNA página de la RAM (región 1) a la zona de prueba
 *   6  + una copia de 1024x64 de un búfer de 256 KB (región 1, dos franjas), como la autoprueba
 * El arranque normal es «hasta el final» (GV_TODO). */
#define GV_TODO 99
static void gv_marca(int ate, int paso, const char *que)
{
    if (ate != GV_TODO) klog("[ACEL2D] radeon: GART paso %d: %s\n", paso, que);
}

static bool gv_preparar_ate(int ate, char *m, int nm)
{
    if (!mmio || gen != 2) { snformatar(m, (size_t)nm, "solo en las R5xx"); return false; }
    int rv515 = -1;
    for (unsigned i = 0; i < sizeof r5_modelos / sizeof r5_modelos[0]; i++)
        if (ra_disp.modelo >= r5_modelos[i].de && ra_disp.modelo <= r5_modelos[i].ate) rv515 = r5_modelos[i].rv515;
    if (rv515 < 0) { snformatar(m, (size_t)nm, "la %04x no esta en la lista de R5xx con GART PCIE", ra_disp.modelo); return false; }
    if (gt.aparcada) { snformatar(m, (size_t)nm, "la GART quedo aparcada tras un cuelgue del motor: no se reutiliza"); return false; }
    if (pci_cap(&ra_disp, 0x02)) { snformatar(m, (size_t)nm, "tarjeta AGP: sin GART PCIE"); return false; }
    if (!pci_cap(&ra_disp, 0x10)) { snformatar(m, (size_t)nm, "la tarjeta no dice ser PCI Express"); return false; }
    /* La VRAM en el mapa del MC, y la pantalla dentro de ella en el mismo sitio que en el BAR 0. */
    uint32_t loc = mc_rd(rv515 ? 0x01 : 0x04), agp = mc_rd(rv515 ? 0x02 : 0x05);
    uint32_t memsize = rd(CONFIG_MEMSIZE), aper = rd(CONFIG_APER_SIZE);
    uint32_t fi, ff;
    gt.rv515 = rv515;
    for (int i = 0; i < 9; i++) gt.orig[i] = pcie_rd(PCIE_TX_GART_CNTL + (uint32_t)i);
    klog("[ACEL2D] radeon: GART: %s %04x, MC_FB_LOCATION %x, MC_AGP_LOCATION %x, CONFIG_MEMSIZE %u MB, apertura %u MB; "
         "PCIE_TX_GART_CNTL %x BASE %x START %x END %x\n", rv515 ? "RV515" : "R520", ra_disp.modelo, loc, agp, memsize >> 20,
         aper >> 20, gt.orig[0], gt.orig[3], gt.orig[4], gt.orig[6]);
    if (ate != GV_TODO)
        klog("[ACEL2D] radeon: GART paso 0: MC_AGP_BASE %x BASE_2 %x, HDP_FB_LOCATION %x, PCIE_TX_GART 10-18: %x %x %x %x %x %x %x %x %x; "
             "PCI orden %x estado %x; RBBM_STATUS %x; BAR0 %x pantalla MC %x (CPU %x) pitch %u %dx%d\n",
             mc_rd(rv515 ? 0x03 : 0x06), mc_rd(rv515 ? 0x04 : 0x07), rd(0x0134), gt.orig[0], gt.orig[1], gt.orig[2], gt.orig[3],
             gt.orig[4], gt.orig[5], gt.orig[6], gt.orig[7], gt.orig[8], pci_le32(&ra_disp, 0x04) & 0xFFFF,
             pci_le32(&ra_disp, 0x04) >> 16, rd(RBBM_STATUS), ra_bar0, ra_tela_mc, ra_tela.fb_fisico, ra_tela.pitch, ra_tela.larg,
             ra_tela.alt);
    if (gt.orig[0] & GART_EN) { snformatar(m, (size_t)nm, "la GART ya estaba encendida (CNTL %x): no se toca", gt.orig[0]); return false; }
    if (!mc_rango(loc, &fi, &ff)) { snformatar(m, (size_t)nm, "MC_FB_LOCATION sin sentido (%x)", loc); return false; }
    if (memsize < (16u << 20) || memsize == 0xFFFFFFFFu || aper < (16u << 20) || aper == 0xFFFFFFFFu) {
        snformatar(m, (size_t)nm, "tamanos de memoria raros (CONFIG_MEMSIZE %x, APER_SIZE %x)", memsize, aper);
        return false;
    }
    if (ra_tela_mc < fi || ra_tela_mc > ff || ra_tela.fb_fisico - ra_bar0 != ra_tela_mc - fi) {
        snformatar(m, (size_t)nm, "la pantalla esta en MC %x y en el BAR 0 +%x: no cuadra con la VRAM en MC %x", ra_tela_mc,
                   ra_tela.fb_fisico - ra_bar0, fi);
        return false;
    }
    /* La tabla (Amilo, 2026-11-19): ya no en las 64 filas detrás de la zona de prueba del 2D (c0340000 en el
     * Amilo, dentro de la superficie que la BIOS deja en D2: c0100000 + 3840 KB), sino detrás de TODO lo que la
     * tarjeta muestra o usa, con la misma lógica que la zona 3D (vram_zona_logica.h, D386): la pantalla del
     * VBE con las dos zonas de 64 filas debajo, la superficie de cada controlador con la imagen activa (con su
     * tamaño de los registros, o 8 MB), los cursores por hardware, y la zona 3D de gpu3d_r500.c (el mismo
     * hueco que buscaría él, con sus 16 MB como mucho), y detrás de todo eso. */
    uint32_t tela_off = ra_tela_mc - fi;
    uint64_t vram = (uint64_t)ff - fi + 1;
    if (vram > memsize) vram = memsize;
    if (vram > aper) vram = aper;
    vram_ocupado_t oc[7];
    int noc = 0;
    oc[noc].ini = tela_off;
    oc[noc++].fim = (uint64_t)tela_off + (uint64_t)ra_tela.pitch * (uint32_t)(ra_tela.alt + 2 * ACEL2D_ZONA_A);
    for (int c = 0; c < 2; c++) {
        uint32_t o = c ? D2_DESLOC : 0;
        uint32_t ge = rd(D1GRPH_ENABLE + o), gs = rd(D1GRPH_PRIMARY_SURFACE + o), gp = rd(D1GRPH_PITCH + o);
        uint32_t crtc = rd(0x6080 + o), ctl = rd(0x6104 + o), dy = rd(0x6128 + o), yf = rd(0x6138 + o);
        uint32_t vpi = rd(0x6580 + o), vpt = rd(0x6584 + o);
        uint32_t cc = rd(D1CUR_CONTROL + o), cs = rd(D1CUR_SURFACE + o);
        uint64_t bytes = avivo_superficie_bytes(ctl, gp, yf, dy, vpi, vpt);
        klog("[ACEL2D] radeon: GART: D%d imagen %s, CRTC %s, superficie MC %x, %u KB%s; cursor %s en MC %x\n", c + 1,
             ge & 1 ? "activa" : "apagada", crtc & 1 ? "encendido" : "apagado", gs, (uint32_t)(bytes >> 10),
             bytes == VRAM_MARGEM_DESCONHECIDA ? " (tamano desconocido: margen)" : "", cc & 1 ? "encendido" : "apagado", cs);
        if ((ge & 1) && gs >= fi && gs <= ff) { oc[noc].ini = gs - fi; oc[noc++].fim = (uint64_t)(gs - fi) + bytes; }
        if ((cc & 1) && cs >= fi && cs <= ff) { oc[noc].ini = cs - fi; oc[noc++].fim = (uint64_t)(cs - fi) + 0x4000u; }
    }
    uint64_t z3;
    uint32_t z3t;
    /* D387 (cuarta tanda): la misma búsqueda que gpu3d_r500.c (r5_zona3d_buscar), con su tamaño de verdad: la
     * zona llega hasta 8 MB antes del final de la VRAM útil y la tabla va en esos 8 MB. */
    if (r5_zona3d_buscar(oc, noc, oc[0].fim, vram, &z3, &z3t)) {
        oc[noc].ini = z3; oc[noc++].fim = z3 + z3t;
        klog("[ACEL2D] radeon: GART: zona 3D (la de gpu3d_r500) en MC %x-%x\n", fi + (uint32_t)z3, fi + (uint32_t)(z3 + z3t) - 1);
    }
    uint64_t desde = 0;
    for (int i = 0; i < noc; i++) if (oc[i].fim > desde) desde = oc[i].fim;
    uint64_t ini;
    if (!vram_tabela_gart(oc, noc, vram, TAB_BYTES, &ini)) {
        snformatar(m, (size_t)nm, "no caben los 16 KB de la tabla detras de todo lo que usa la tarjeta (VRAM util %u KB)",
                   (uint32_t)(vram >> 10));
        return false;
    }
    klog("[ACEL2D] radeon: GART: %d regiones ocupadas de la VRAM (hasta MC %x); la tabla va en MC %x (VRAM +%u KB)\n", noc,
         fi + (uint32_t)desde - 1, fi + (uint32_t)ini, (uint32_t)(ini >> 10));
    uint32_t gi;
    if (!gart_r300_janela(fi, ff, agp, GTT_BYTES, &gi)) {
        snformatar(m, (size_t)nm, "no hay sitio para la ventana de 16 MB en el mapa del MC (VRAM %x-%x, AGP %x)", fi, ff, agp);
        return false;
    }
    gt.fb_ini = fi; gt.fb_fim = ff; gt.agp = agp; gt.gtt_ini = gi;
    gt.tab_off = (uint32_t)ini; gt.vram_util = (uint32_t)(vram > 0xFFFFFFFFull ? 0xFFFFFFFFu : vram);
    gv_pcie_log(ate == GV_TODO ? "al preparar" : "paso 0");   /* dbg5: solo lectura */
    gt.tab_mc = fi + (uint32_t)ini;
    uint32_t tf = ra_bar0 + (uint32_t)ini;
    if (ate != GV_TODO) {
        klog("[ACEL2D] radeon: GART paso 0: VRAM en MC %x-%x, ventana de 16 MB en MC %x-%x, tabla en MC %x (CPU %x, VRAM +%u KB)\n",
             fi, ff, gi, gi + GTT_BYTES - 1, gt.tab_mc, tf, (uint32_t)(ini >> 10));
        /* El puente por encima (el puerto raíz PCIe): sin su bus maestro (bit 2) las lecturas de la tarjeta
         * hacia la RAM no suben. */
        static pci_disp_t v[64];
        int n = pci_listar(v, 64);
        for (int i = 0; i < n; i++)
            if (v[i].classe == 0x06 && v[i].subclasse == 0x04 && ((pci_le32(&v[i], 0x18) >> 8) & 0xFF) == ra_disp.barramento)
                klog("[ACEL2D] radeon: GART paso 0: puente %x:%x.%x (%04x:%04x) orden %x estado %x, control del puente %x\n",
                     v[i].barramento, v[i].dispositivo, v[i].funcao, v[i].fabricante, v[i].modelo, pci_le32(&v[i], 0x04) & 0xFFFF,
                     pci_le32(&v[i], 0x04) >> 16, pci_le32(&v[i], 0x3C) >> 16);
        if (ate == 0) return true;
        gv_marca(ate, 1, "tabla en la VRAM (escritura por la CPU)");
    }
    if (!gt.tab || gt.tab_fis != tf) {
        gt.tab_fis = tf;
        gt.tab = (volatile uint32_t *)janela_mapear_tipo(tf, TAB_BYTES, PG_PCD | PG_PWT);
    }
    if (!gt.tab) { snformatar(m, (size_t)nm, "sin ventana para la tabla"); return false; }
    /* Página de relleno para las entradas vacías: una página nuestra a ceros (nunca se libera). */
    static uint32_t vazia;
    if (!vazia) { vazia = memfisica_alocar(); if (vazia) memset((void *)vazia, 0, 4096); }
    if (!vazia || !gart_r300_pte(vazia, GART_R300_LEITURA, &gt.pte_vazia)) { snformatar(m, (size_t)nm, "sin pagina de relleno"); return false; }
    if (!ra_esperar()) { snformatar(m, (size_t)nm, "el motor 2D no esta libre"); return false; }
    for (uint32_t i = 0; i < GTT_PAG; i++) gt.tab[i] = gt.pte_vazia;
    if (gt.tab[GTT_PAG - 1] != gt.pte_vazia || gt.tab[0] != gt.pte_vazia) {
        snformatar(m, (size_t)nm, "la tabla no se lee como se escribio (%x)", gt.tab[0]);
        return false;
    }
    if (ate == 1) { klog("[ACEL2D] radeon: GART paso 1: tabla escrita y leida (entrada 0 %x)\n", gt.tab[0]); return true; }
    gv_marca(ate, 2, "registros de la GART sin encenderla");
    /* rv370_pcie_gart_enable. Antes que el bus maestro: con la GART apagada el puente deja pasar
     * las direcciones tal cual (PASS_THRU); encendida, lo que cae fuera de la ventana se descarta.
     * Depuración: «ligada» desde el primer registro, para que gv_soltar los deje como estaban. */
    gt.ligada = true;
    gt.npag[0] = gt.npag[1] = gt.npag2 = 0;
    /* dbg5: las opciones de depuración que cambian algo de la tarjeta (gv_soltar lo devuelve). */
    if (gv_opc & GVO_RELOJ) {
        for (int i = 0; i < 3; i++) {
            gt.pll_orig[i] = pll_rd(pll_forcar[i]);
            if (!(gt.pll_orig[i] & 1u)) pll_wr(pll_forcar[i], gt.pll_orig[i] | 1u);
        }
        gt.pll_mudado = true;
        klog("[ACEL2D] radeon: GART: opcion RELOJ: PLL 0F %x -> %x, 11 %x -> %x, 13 %x -> %x\n", gt.pll_orig[0], pll_rd(0x0F),
             gt.pll_orig[1], pll_rd(0x11), gt.pll_orig[2], pll_rd(0x13));
    }
    if (gv_opc & GVO_AGP) {
        gt.agp_orig = agp;
        mc_wr(rv515 ? 0x02 : 0x05, 0xFFFFFFFFu);
        gt.agp_mudado = true;
        klog("[ACEL2D] radeon: GART: opcion AGP: MC_AGP_LOCATION %x -> %x (MC_AGP_BASE %x, sin tocar)\n", agp, mc_rd(rv515 ? 0x02 : 0x05),
             mc_rd(rv515 ? 0x03 : 0x06));
    }
    pcie_wr(PCIE_TX_GART_CNTL, GART_DESCARTAR);
    pcie_wr(PCIE_TX_GART_START_LO, gi);
    pcie_wr(PCIE_TX_GART_END_LO, (gi + GTT_BYTES - 1) & ~0xFFFu);
    pcie_wr(PCIE_TX_GART_START_HI, 0);
    pcie_wr(PCIE_TX_GART_END_HI, 0);
    pcie_wr(PCIE_TX_GART_BASE, gt.tab_mc);
    pcie_wr(PCIE_TX_DISCARD_RD_LO, fi);
    pcie_wr(PCIE_TX_DISCARD_RD_HI, 0);
    pcie_wr(PCIE_TX_GART_ERROR, 0);
    if (ate == 2) { gv_detalle("paso 2, sin encender"); return true; }
    gv_marca(ate, 3, "GART_EN y TLB");
    pcie_wr(PCIE_TX_GART_CNTL, pcie_rd(PCIE_TX_GART_CNTL) | GART_EN | GART_DESCARTAR);
    gart_tlb();
    uint32_t base = pcie_rd(PCIE_TX_GART_BASE), st = pcie_rd(PCIE_TX_GART_START_LO);
    if (base != gt.tab_mc || st != gi || !(pcie_rd(PCIE_TX_GART_CNTL) & GART_EN)) {
        snformatar(m, (size_t)nm, "los registros de la GART no se quedan (BASE %x, START %x, CNTL %x)", base, st,
                   pcie_rd(PCIE_TX_GART_CNTL));
        gv_soltar();
        return false;
    }
    if (ate == 3) { gv_detalle("paso 3, encendida sin bus maestro"); return true; }
    gv_marca(ate, 4, "bus maestro del PCI");
    /* Bus maestro: sin él la tarjeta no puede leer la RAM. */
    uint32_t cmd = pci_le32(&ra_disp, 0x04);
    if (!(cmd & 0x04)) { pci_escreve32(&ra_disp, 0x04, (cmd & 0xFFFFu) | 0x04u); gt.bm_nosso = true; }
    klog("[ACEL2D] radeon: GART PCIE de 16 MB en MC %x-%x, tabla en MC %x (VRAM +%u KB), bus maestro %s; leido: BASE %x START %x CNTL %x\n",
         gi, gi + GTT_BYTES - 1, gt.tab_mc, (uint32_t)(ini >> 10), gt.bm_nosso ? "encendido ahora" : "ya estaba", base, st,
         pcie_rd(PCIE_TX_GART_CNTL));
    return true;
}

static bool gv_preparar(char *m, int nm) { return gv_preparar_ate(GV_TODO, m, nm); }

static bool gv_mapear(int reg, const void *buf, uint32_t passo, uint32_t larg, uint32_t alt, char *m, int nm)
{
    if (!gt.ligada || reg < 0 || reg > 1) { snformatar(m, (size_t)nm, "GART apagada"); return false; }
    uint32_t pri0 = reg ? REG1_PAG : 0;
    if (!buf) {   /* revisión: la región sin uso, a la página de relleno (ver gart_vaciar) */
        gart_vaciar(pri0, gt.npag[reg]);
        gt.npag[reg] = 0;
        return true;
    }
    uint32_t v = (uint32_t)buf, desl = v & 0xFFFu, np = volcado_paginas(desl, passo, alt);
    uint32_t pri = reg ? REG1_PAG : 0, cap = reg ? GTT_PAG - REG1_PAG : REG1_PAG;
    if (np > cap) { snformatar(m, (size_t)nm, "el bufer (%u paginas) no cabe en la ventana (%u)", np, cap); return false; }
    for (uint32_t i = 0; i < np; i++) {
        uint32_t fl, pte;
        fis_t f = espaco_traduzir(espaco_kernel(), (v & ~0xFFFu) + i * 4096u, &fl);
        if (!f || !gart_r300_pte(f & ~0xFFFull, GART_R300_LEITURA | ((gv_opc & GVO_SEM_SNOOP) ? GART_R300_SEM_SNOOP : 0), &pte)) {
            snformatar(m, (size_t)nm, "la pagina %u del bufer (fisica %x:%x) no cabe en la GART", i, (uint32_t)(f >> 32), (uint32_t)f);
            for (uint32_t k = 0; k < i; k++) gt.tab[pri + k] = gt.pte_vazia;
            return false;
        }
        gt.tab[pri + i] = pte;
    }
    for (uint32_t i = np; i < gt.npag[reg]; i++) gt.tab[pri + i] = gt.pte_vazia;
    {   /* dbg5: dónde están las páginas (al rastro, que sobrevive a un reinicio): la menor, la mayor y cuántas no son
         * RAM libre del E820. */
        uint64_t mn = ~0ull, mx = 0;
        uint32_t fora = 0;
        for (uint32_t i = 0; i < np; i++) {
            uint32_t fl;
            uint64_t ff = espaco_traduzir(espaco_kernel(), (v & ~0xFFFu) + i * 4096u, &fl) & ~0xFFFull;
            if (ff < mn) mn = ff;
            if (ff > mx) mx = ff;
            if (!memfisica_sobre_ram(ff, ff + 4096u)) fora++;
        }
        rastro_marca(RASTRO_VALOR(0x30 + reg) | np);
        rastro_marca((uint32_t)mn);
        rastro_marca((uint32_t)mx);
        rastro_marca(fora);
        klog("[ACEL2D] radeon: GART: region %d: paginas fisicas de %x a %x, %u fuera de la RAM del E820\n", reg, (uint32_t)mn,
             (uint32_t)mx, fora);
    }
    /* Leer de vuelta empuja las escrituras por el bus antes de vaciar la TLB de la GART. */
    uint32_t ult = gt.tab[pri + np - 1];
    gart_tlb();
    gt.npag[reg] = np;
    gt.f[reg] = (volcado_fonte_t){ gt.gtt_ini + pri * 4096u, desl, passo, larg, alt };
    const char *porque;
    if (!volcado_fonte_ok(&gt.f[reg], &porque)) { snformatar(m, (size_t)nm, "%s", porque); return false; }
    klog("[ACEL2D] radeon: GART: region %d: %u paginas (%x... -> MC %x), ultima entrada %x\n", reg, np, v, gt.f[reg].gtt_base, ult);
    return true;
}

/* dbg5: la copia con el destino (DST_PITCH_OFFSET) y las filas por orden que se digan. Las marcas finas de dbg4
 * (fila 4: columna = franja; la 19, el vaciado de la caché) van siempre al rastro; a la pantalla solo con la
 * opción MARCAS (el motor está ocupado con las franjas de antes: ver acel_marca_fina). */
/* «esperar» (D387, arreglo del Amilo, 2026-11-19): cada franja se manda con el motor PARADO y se espera a que
 * termine antes de la siguiente. En la X1400 del Amilo (945PM, PCIe) mandar órdenes al motor 2D mientras lee de la
 * RAM por la GART congela el equipo entero (paso 11 de dbg5: franjas en cola, con o sin el reloj forzado), y las
 * mismas franjas una a una funcionan (pasos 9, 10 y 12, de 16 a 128 filas, también a la pantalla visible). Sin
 * «esperar» (solo los experimentos de depuración, paso «en cola») se encolan como antes. */
static bool gv_volcar_em(int reg, int x, int y, int l, int a, int dx, int dy, uint32_t dst_po, int max_filas, bool marcas,
                         bool esperar)
{
    static volcado_faixa_t fx[64];
    if (!gt.ligada || !gt.npag[reg]) return false;
    int n = volcado_trocear(&gt.f[reg], x, y, l, a, max_filas, fx, 64);
    if (n < 0) return false;
    if (gv_opc & GVO_SEM_SNOOP) __asm__ volatile("wbinvd" ::: "memory");   /* sin snoop: lo de la caché, a la RAM */
    for (int k = 0; k < n; k++) {
        if (marcas) acel_marca_fina(4, k < 18 ? k : 18, 0);
        if (esperar && !ra_esperar_prazo(1000)) return false;   /* también la primera: el CP del 3D puede estar ocupado */
        if (marcas) acel_marca_fina(4, k < 18 ? k : 18, 2);
        /* Por MMIO o, con el CP en marcha, por su anillo (D387): las mismas escrituras. */
        ra2d_ordem_t o;
        ra2d_copiar(&o, fx[k].pitch_offset, dst_po, DST_X_ESQ_DIR | DST_Y_CIMA_BAIXO, fx[k].sx, fx[k].sy, dx + fx[k].dx - x,
                    dy + fx[k].dy - y, fx[k].l, fx[k].a, false, false);
        if (!ra_mandar(&o, 1)) return false;
        if (marcas) acel_marca_fina(4, k < 18 ? k : 18, 1);
    }
    /* Vaciar la caché de destino del 2D detrás de las copias (por la FIFO, como RADEON_PURGE_CACHE de
     * X.org): sin esto, las últimas filas podrían no llegar a la VRAM hasta la siguiente espera. */
    if (marcas) acel_marca_fina(4, 19, 0);
    if (esperar) {
        /* Parado y con la caché vaciada (ra_esperar_prazo lo hace) antes de volver: quien venga detrás (el 3D
         * por el CP, otra ventana) encuentra el motor libre. */
        bool ok = ra_esperar_prazo(1000);
        if (marcas) acel_marca_fina(4, 19, 1);
        return ok;
    }
    if (ra_pelo_cp()) { if (marcas) acel_marca_fina(4, 19, 1); return true; }   /* el anillo ya lleva el vaciado tras cada orden */
    if (!fifo(1)) return false;
    wr(R300_DSTCACHE_CTLSTAT, DC_FLUSH_ALL);
    if (marcas) acel_marca_fina(4, 19, 1);
    return true;
}

static bool gv_volcar(int reg, int x, int y, int l, int a, int dx, int dy)
{
    return gv_volcar_em(reg, x, y, l, a, dx, dy, pitch_offset, FAIXA_FILAS, true, true);
}

static bool gv_esperar(uint32_t ms) { return ra_esperar_prazo(ms ? ms : 1); }

/* D391: la región de escritura, detrás de la región 0 (lo que ocupa ahora el búfer de atrás) y antes de
 * la de la autoprueba. npag = 0: todas a la página de relleno (solo lectura) otra vez. */
static bool gv_mapear_escrita(const uint64_t *pag, uint32_t npag, char *m, int nm)
{
    if (!gt.ligada || !gt.tab) { snformatar(m, (size_t)nm, "GART apagada"); return false; }
    if (!npag) {
        if (!gt.npag2) return true;
        gart_vaciar(gt.r2_ini, gt.npag2);
        gt.npag2 = 0;
        return true;
    }
    uint32_t ini = gt.npag[0], cap = REG1_PAG > ini ? REG1_PAG - ini : 0;
    if (npag > cap) { snformatar(m, (size_t)nm, "el destino (%u paginas) no cabe en la ventana (%u libres)", npag, cap); return false; }
    if (gt.npag2) { snformatar(m, (size_t)nm, "la region de escritura ya estaba apuntada"); return false; }
    for (uint32_t i = 0; i < npag; i++) {
        uint32_t pte;
        if (!gart_r300_pte(pag[i], GART_R300_LEITURA | GART_R300_ESCRITA, &pte)) {
            for (uint32_t k = 0; k < i; k++) gt.tab[ini + k] = gt.pte_vazia;
            snformatar(m, (size_t)nm, "la pagina %u (fisica %x:%x) no cabe en la GART", i, (uint32_t)(pag[i] >> 32), (uint32_t)pag[i]);
            return false;
        }
        gt.tab[ini + i] = pte;
    }
    uint32_t ult = gt.tab[ini + npag - 1];
    gart_tlb();
    gt.r2_ini = ini; gt.npag2 = npag;
    if (ult != (((uint32_t)pag[npag - 1] >> 8) | ((uint32_t)((pag[npag - 1] >> 32) & 0xFFu) << 24) | GART_R300_LEITURA | GART_R300_ESCRITA)) {
        snformatar(m, (size_t)nm, "la tabla no se lee como se escribio (%x)", ult);
        return false;   /* la capa llama a mapear_escrita(NULL, 0) y la deja vacía */
    }
    return true;
}

/* D391: copia l x a píxeles del origen (VRAM) a la región 2, en franjas (fila a fila si el paso del destino
 * no es múltiplo de 64). Con «mascara», ROP origen AND patrón con el patrón 0x00FFFFFF: sin el alfa. */
static bool gv_retornar(const acel_origem_t *o, int l, int a, uint32_t desl, uint32_t passo, bool mascara)
{
    if (!gt.ligada || !gt.npag2) return false;
    uint32_t src_po;
    int sy0;
    if (o->gpu == ACEL_ORIGEM_TELA) { src_po = pitch_offset; sy0 = o->y; }
    else {
        if (!o->gpu || (o->gpu & 0x3FFu) || o->gpu > 0xFFFFFFFFull || (o->passo & 63u) || o->passo / 64u > 255u) return false;
        src_po = ((o->passo / 64u) << 22) | ((uint32_t)o->gpu >> 10);
        sy0 = 0;
    }
    retorno_destino_t d = { gt.gtt_ini + gt.r2_ini * 4096u, desl, passo };
    const char *porque;
    if (!retorno_destino_ok(&d, l, a, &porque) || retorno_bytes(&d, l, a) > gt.npag2 * 4096u || sy0 < 0 || sy0 + a > 8191) return false;
    int n = retorno_n_faixas(&d, a, FAIXA_FILAS);
    for (int k = 0; k < n; k++) {
        volcado_faixa_t f;
        /* Como gv_volcar_em con «esperar»: el motor escribe en la RAM por la GART; nada de órdenes con él ocupado. */
        if (!ra_esperar_prazo(1000)) return false;
        if (!retorno_faixa(&d, l, a, FAIXA_FILAS, k, &f)) return false;
        ra2d_ordem_t o;   /* por MMIO o por el anillo del CP (D387) */
        ra2d_copiar(&o, src_po, f.pitch_offset, DST_X_ESQ_DIR | DST_Y_CIMA_BAIXO, 0, sy0 + f.sy, f.dx, 0, f.l, f.a, mascara, true);
        if (!ra_mandar(&o, 1)) return false;
    }
    return ra_esperar_prazo(1000);   /* parado y con la caché vaciada */
}

/* Depuración (ver gv_preparar_ate): los pasos 5 y 6, una copia de la RAM a la zona de prueba por la GART. La
 * zona: las filas de debajo de la pantalla visible (las de la autoprueba del 2D). */
static bool gv_paso_copia(int paso, char *m, int nm)
{
    int L = paso == 5 ? 64 : (ra_tela.larg < 1024 ? ra_tela.larg : 1024), A = paso == 5 ? 1 : ACEL2D_ZONA_A;
    uint32_t pitch = ra_tela.pitch;
    static volatile uint8_t *zc;   /* la zona de prueba vista por la CPU (como «zona» en acel2d.c) */
    if (!zc) zc = (volatile uint8_t *)janela_mapear_tipo(ra_tela.fb_fisico + pitch * (uint32_t)ra_tela.alt, pitch * ACEL2D_ZONA_A,
                                                         PG_PCD | PG_PWT);
    if (!zc) { snformatar(m, (size_t)nm, "sin ventana para la zona de prueba"); return false; }
    #define ZC(x, y) (((volatile uint32_t *)(zc + pitch * (uint32_t)(y)))[x])
    uint32_t *p = paso == 5 ? (uint32_t *)memfisica_alocar() : (uint32_t *)kmalloc((uint32_t)L * (uint32_t)A * 4u);
    if (!p) { snformatar(m, (size_t)nm, "sin memoria para el paso %d", paso); return false; }
    for (int y = 0; y < A; y++)
        for (int x = 0; x < L; x++) {
            p[y * L + x] = ((uint32_t)x * 2654435761u ^ (uint32_t)y * 40503u ^ 0x5A5A5Au) & 0x00FFFFFFu;
            ZC(x, y) = 0x00102030u;
        }
    bool ok = gv_mapear(1, p, (uint32_t)L * 4u, (uint32_t)L, (uint32_t)A, m, nm);
    uint32_t fl, fis = (uint32_t)espaco_traduzir(espaco_kernel(), (uint32_t)p, &fl);
    klog("[ACEL2D] radeon: GART paso %d: region 1 apuntada (%s): bufer %x fisica %x, entrada %x; RBBM_STATUS %x\n", paso,
         ok ? "bien" : m, (uint32_t)p, fis, gt.tab[REG1_PAG], rd(RBBM_STATUS));
    if (ok) {
        klog("[ACEL2D] radeon: GART paso %d: copia de %dx%d desde MC %x a la fila %d (el motor lee la RAM)\n", paso, L, A,
             gt.f[1].gtt_base, ra_tela.alt);
        ok = gv_volcar(1, 0, 0, L, A, 0, ra_tela.alt);
        if (!ok) snformatar(m, (size_t)nm, "el motor no acepto la copia");
    }
    bool parado = gv_esperar(50);
    klog("[ACEL2D] radeon: GART paso %d: tras la espera: %s; RBBM_STATUS %x, GART_ERROR %x\n", paso,
         parado ? "motor parado" : "MOTOR OCUPADO", rd(RBBM_STATUS), pcie_rd(PCIE_TX_GART_ERROR));
    if (ok && !parado) { snformatar(m, (size_t)nm, "el motor no termina la copia"); ok = false; }
    if (!parado) { klog("[ACEL2D] radeon: GART paso %d: el bufer no se libera (la GART aun lo apunta)\n", paso); return false; }
    uint32_t dif = 0, d0 = 0, d0g = 0, d0c = 0;
    for (int y = 0; y < A; y++)
        for (int x = 0; x < L; x++) {
            uint32_t g = ZC(x, y) & 0x00FFFFFFu;
            if (g != p[y * L + x]) { if (!dif) { d0 = (uint32_t)(y * L + x); d0g = g; d0c = p[y * L + x]; } dif++; }
        }
    /* Muestras (el patrón nunca es cero; «primero» solo vale si hay distintos): el primer y el último píxel. */
    uint32_t g0 = ZC(0, 0) & 0x00FFFFFFu, gu = ZC(L - 1, A - 1) & 0x00FFFFFFu;
    #undef ZC
    klog("[ACEL2D] radeon: GART paso %d: %u pixeles distintos de %d%s; muestras: gpu %x ram %x, gpu %x ram %x (fondo 102030)\n",
         paso, dif, L * A, dif ? "" : ": IGUAL", g0, p[0], gu, p[L * A - 1]);
    if (dif) klog("[ACEL2D] radeon: GART paso %d: primer distinto en %u: gpu %x ram %x\n", paso, d0, d0g, d0c);
    if (ok && dif) { snformatar(m, (size_t)nm, "%u pixeles distintos (gpu %x, ram %x)", dif, d0g, d0c); ok = false; }
    char m2[8];
    gv_mapear(1, NULL, 0, 0, 0, m2, sizeof m2);
    if (paso == 5) memfisica_liberar((uint32_t)p); else kfree(p);
    return ok;
}

/* ------------------------------------------------------------ dbg5: los experimentos (pasos 7 a 17)
 *
 * Cada uno enciende la GART (como el paso 4), copia un búfer del tamaño de la pantalla entera por ella y la
 * apaga. Cambia UNA cosa cada vez respecto de lo que ya se vio ir (paso 6: 64 filas de un búfer de prueba a la
 * zona de prueba), hasta llegar a la forma exacta que colgó el equipo con dbg3 (paso 16):
 *    7  búfer de PRUEBA (kmalloc de 3 MB), franjas de 16 filas, cada una esperada y comparada; destino FUERA
 *       de la pantalla (VRAM libre detrás de la tabla de la GART)       -> muchas páginas distintas, poco a poco
 *    8  lo mismo con el BÚFER DE ATRÁS de verdad                         -> ¿son sus páginas?
 *    9  búfer de atrás, franjas de 64 esperadas, fuera                   -> órdenes como las del paso 6
 *   10  búfer de atrás, franjas de 128 esperadas, fuera                  -> el tamaño de orden de dbg3
 *   11  búfer de atrás, franjas de 64 EN COLA (sin esperar entre ellas), fuera -> la FIFO llena con el motor leyendo
 *   12  búfer de atrás, franjas de 16 esperadas, a la PANTALLA visible   -> ¿es el destino que lee el CRTC?
 *   13  búfer de atrás, franjas de 64 en cola, a la pantalla             -> un volcado real (lo que hace dbg4)
 *   14  como el 9, y la CPU escribe en la pantalla visible mientras el motor copia -> ¿es la CPU en la VRAM?
 *   15  búfer de atrás, UNA sola orden de toda la pantalla (3 MB), fuera -> como r100_copy_blit de Linux
 *   16  búfer de atrás, franjas de 128 en cola, a la pantalla            -> exactamente lo que colgó con dbg3
 *   17  como el 9, pero las órdenes van por el CP (paquetes BITBLT_MULTI en el anillo, con WAIT_UNTIL y el
 *       vaciado de la caché detrás, como r100_copy_blit) en vez de por MMIO. Necesita el 3D encendido antes
 *       («gpu3d encender»): usa el CP y el anillo de gpu3d_r500.c.
 * Marcas: en la pantalla, fila (paso - 2), una columna por franja, pintadas SOLO con el motor parado: amarillo
 * antes de la orden, blanco esperada e igual, rojo plazo agotado o distinta. Y al rastro (rastro.h), que se lee
 * en el arranque siguiente si el equipo se reinicia: 0x200000 | paso << 12 | franja << 4 | fase. */
typedef struct { uint8_t tras, encolar, tela, toca, cp; uint16_t filas; const char *que; } gv_exp_t;
static const gv_exp_t gv_exps[] = {
    /*  7 */ { 0, 0, 0, 0, 0, 16,  "bufer de prueba, franjas de 16 esperadas, destino fuera de la pantalla" },
    /*  8 */ { 1, 0, 0, 0, 0, 16,  "bufer de atras, franjas de 16 esperadas, fuera de la pantalla" },
    /*  9 */ { 1, 0, 0, 0, 0, 64,  "bufer de atras, franjas de 64 esperadas, fuera de la pantalla" },
    /* 10 */ { 1, 0, 0, 0, 0, 128, "bufer de atras, franjas de 128 esperadas, fuera de la pantalla" },
    /* 11 */ { 1, 1, 0, 0, 0, 64,  "bufer de atras, franjas de 64 en cola, fuera de la pantalla" },
    /* 12 */ { 1, 0, 1, 0, 0, 16,  "bufer de atras, franjas de 16 esperadas, a la pantalla visible" },
    /* 13 */ { 1, 1, 1, 0, 0, 64,  "bufer de atras, franjas de 64 en cola, a la pantalla visible" },
    /* 14 */ { 1, 0, 0, 1, 0, 64,  "bufer de atras, franjas de 64 esperadas, fuera; la CPU escribe en la pantalla mientras" },
    /* 15 */ { 1, 0, 0, 0, 0, 0,   "bufer de atras, una sola orden de toda la pantalla, fuera de la pantalla" },
    /* 16 */ { 1, 1, 1, 0, 0, 128, "bufer de atras, franjas de 128 en cola, a la pantalla visible (lo de dbg3)" },
    /* 17 */ { 1, 0, 0, 0, 1, 64,  "bufer de atras, franjas de 64 esperadas, fuera; ordenes por el CP" },
};
#define GV_EXP_PRIMEIRO 7
#define GV_EXP_ULTIMO   (GV_EXP_PRIMEIRO + (int)(sizeof gv_exps / sizeof gv_exps[0]) - 1)
#define RASTRO_EXP(paso, k, fase) (0x200000u | ((uint32_t)(paso) << 12) | (((uint32_t)(k) & 0xFFu) << 4) | (uint32_t)(fase))

/* gpu3d_r500.c (dbg5): una copia del motor 2D por el CP. */
bool gpu3d_r500_cp_em_marcha(void);
bool gpu3d_r500_copia_cp(uint32_t src_po, uint32_t dst_po, int sx, int sy, int dx, int dy, int l, int a, uint32_t ms);

static uint32_t gv_us(uint64_t t0, uint64_t t1)
{
    uint32_t khz = hw_tsc_khz();
    return khz ? (uint32_t)sconv_div64((t1 - t0) * 1000u, khz) : 0;
}

/* Espera sin tocar la tarjeta (ni MMIO ni VRAM): solo el TSC y el puerto 0x80. */
static void gv_pausa_cega(uint32_t ms)
{
    prazo_t p = prazo_ms(ms);
    while (!prazo_vencido(&p)) { }
}

static bool gv_experimento(int paso, char *m, int nm)
{
    const gv_exp_t *e = &gv_exps[paso - GV_EXP_PRIMEIRO];
    int L = ra_tela.larg, H = ra_tela.alt, fila_m = paso - 2;
    uint32_t pitch = ra_tela.pitch;
    if ((int)video_largura() != L || (int)video_altura() != H || pitch != (uint32_t)L * 4u) {
        snformatar(m, (size_t)nm, "el escritorio (%ux%u) no es la pantalla (%dx%d, pitch %u)", video_largura(), video_altura(), L, H, pitch);
        return false;
    }
    if (e->cp && !gpu3d_r500_cp_em_marcha()) { snformatar(m, (size_t)nm, "el CP no esta en marcha: antes «gpu3d encender»"); return false; }
    int F = e->filas ? e->filas : H, nf = (H + F - 1) / F;
    /* El destino de fuera de la pantalla: el primer MB entero detrás de la tabla de la GART (que ya va detrás
     * de todo lo que la tarjeta muestra o usa). */
    uint32_t fora = (gt.tab_off + TAB_BYTES + 0xFFFFFu) & ~0xFFFFFu, bytes = pitch * (uint32_t)H;
    static volatile uint8_t *fora_cpu;
    static uint32_t fora_fis;
    volatile uint8_t *dst_cpu;
    uint32_t dst_po;
    if (e->tela) { dst_cpu = (volatile uint8_t *)video_placa(); dst_po = pitch_offset; }
    else {
        if ((uint64_t)fora + bytes > gt.vram_util) { snformatar(m, (size_t)nm, "no cabe el destino de prueba en la VRAM"); return false; }
        if (!fora_cpu || fora_fis != ra_bar0 + fora) {
            fora_fis = ra_bar0 + fora;
            fora_cpu = (volatile uint8_t *)janela_mapear_tipo(fora_fis, bytes, PG_PCD | PG_PWT);
        }
        if (!fora_cpu) { snformatar(m, (size_t)nm, "sin ventana para el destino de prueba"); return false; }
        dst_cpu = fora_cpu;
        dst_po = ((pitch / 64u) << 22) | ((gt.fb_ini + fora) >> 10);
    }
    #define DST(x, y) (((volatile uint32_t *)(dst_cpu + pitch * (uint32_t)(y)))[x])
    uint32_t *src = e->tras ? video_buffer() : (uint32_t *)kmalloc((uint32_t)L * (uint32_t)H * 4u);
    if (!src) { snformatar(m, (size_t)nm, "sin memoria para el bufer de prueba"); return false; }
    klog("[ACEL2D] radeon: GART paso %d: %s; %d franjas de %d filas; opciones %x; destino MC %x (DST_PITCH_OFFSET %x); origen %x\n",
         paso, e->que, nf, F, gv_opc, e->tela ? ra_tela_mc : gt.fb_ini + fora, dst_po, (uint32_t)src);
    rastro_marca(RASTRO_VALOR(0x40) | gv_opc);
    if (!e->tras)
        for (int y = 0; y < H; y++)
            for (int x = 0; x < L; x++) src[y * L + x] = ((uint32_t)x * 2654435761u ^ (uint32_t)y * 40503u ^ 0x3C3C3Cu) & 0x00FFFFFFu;
    /* Las marcas que este paso pinte en el búfer de atrás (solo a la pantalla) se quitan al final. */
    static uint32_t salva[10 * 602];
    uint32_t *tras_b = video_buffer();
    int my0 = 2 + fila_m * 12;
    if (e->tela && my0 + 10 <= H && L >= 602)
        for (int j = 0; j < 10; j++) for (int i = 0; i < 602; i++) salva[j * 602 + i] = tras_b[(my0 + j) * L + i];
    bool ok = true, parado = true;
    /* El fondo del destino de fuera, por el motor (VRAM, lo de siempre): si la copia no llega, se ve. */
    if (!e->tela) {
        ok = ra_preencher_po(dst_po, 0, 0, L, H, 0x00102030u);
        if (ok) ok = ra_esperar();
        if (ok && ((DST(0, 0) & 0x00FFFFFFu) != 0x00102030u || (DST(L - 1, H - 1) & 0x00FFFFFFu) != 0x00102030u)) {
            snformatar(m, (size_t)nm, "el relleno del destino de prueba no se lee (%x, %x)", DST(0, 0), DST(L - 1, H - 1));
            ok = false;
        } else if (!ok) snformatar(m, (size_t)nm, "el motor no hizo el relleno del destino de prueba");
        if (!ok) { if (!e->tras) kfree(src); return false; }
    }
    /* A la pantalla: lo que hay ya es igual que el búfer de atrás. Un testigo por franja (16x2 píxeles
     * invertidos SOLO en el búfer de atrás, a la derecha): si el motor copia, llega a la pantalla y la
     * comparación lo ve; al final se deshace (el que llama redibuja la pantalla). */
    #define TESTIGO(k) do { int ty_ = (k) * F + 1; if (ty_ + 2 <= H) for (int j_ = 0; j_ < 2; j_++) for (int i_ = 0; i_ < 16; i_++) \
                              src[(ty_ + j_) * L + L - 24 + i_] ^= 0x00FFFFFFu; } while (0)
    if (!gv_mapear(0, src, (uint32_t)L * 4u, (uint32_t)L, (uint32_t)H, m, nm)) {
        if (!e->tras) kfree(src);
        return false;
    }
    uint32_t us_total = 0, us_max = 0, dif = 0, d_x = 0, d_y = 0, d_g = 0, d_c = 0;
    int k_max = 0, k = 0;
    uint64_t t0 = prazo_tsc(), t1;
    if (e->encolar) {
        acel_marca_paso(fila_m, 0, 0, e->tela);
        if (e->tela) for (int i = 0; i < nf; i++) TESTIGO(i);
        rastro_marca(RASTRO_EXP(paso, 0, 0));
        t0 = prazo_tsc();
        ok = gv_volcar_em(0, 0, 0, L, H, 0, 0, dst_po, F, false, false);   /* en cola, a propósito */
        rastro_marca(RASTRO_EXP(paso, nf - 1, 1));
        if (!ok) snformatar(m, (size_t)nm, "el motor no acepto las ordenes (FIFO llena 250 ms)");
        parado = ra_esperar_prazo(3000);
        t1 = prazo_tsc();
        us_total = us_max = gv_us(t0, t1);
        rastro_marca(RASTRO_EXP(paso, nf - 1, parado ? 2 : 3));
        if (ok && !parado) { snformatar(m, (size_t)nm, "el motor no termina las %d franjas en cola", nf); ok = false; }
        if (parado)
            for (int y = 0; y < H; y++)
                for (int x = 0; x < L; x++) {
                    uint32_t g = DST(x, y) & 0x00FFFFFFu, c = src[y * L + x] & 0x00FFFFFFu;
                    if (g != c) { if (!dif) { d_x = (uint32_t)x; d_y = (uint32_t)y; d_g = g; d_c = c; } dif++; }
                }
        if (parado) {
            rastro_marca(RASTRO_EXP(paso, nf - 1, dif ? 5 : 4));
            if (e->tela) for (int i = 0; i < nf; i++) TESTIGO(i);
            acel_marca_paso(fila_m, 0, ok && !dif ? 1 : 2, e->tela);
        }
        k = nf;
    } else {
        for (int y0 = 0; ok && y0 < H; y0 += F, k++) {
            int h = H - y0 < F ? H - y0 : F;
            acel_marca_paso(fila_m, k, 0, e->tela);   /* el motor está parado */
            if (e->tela) TESTIGO(k);
            rastro_marca(RASTRO_EXP(paso, k, 0));
            t0 = prazo_tsc();
            if (e->cp) {
                /* Por el CP: el origen, como lo da volcado_trocear para esta franja. */
                volcado_faixa_t fx;
                ok = volcado_trocear(&gt.f[0], 0, y0, L, h, h, &fx, 1) == 1 &&
                     gpu3d_r500_copia_cp(fx.pitch_offset, dst_po, fx.sx, fx.sy, 0, y0, L, h, 1000);
                rastro_marca(RASTRO_EXP(paso, k, ok ? 1 : 3));
                if (!ok) { snformatar(m, (size_t)nm, "la copia por el CP de la franja %d no termina", k); parado = !(rd(RBBM_STATUS) & RBBM_ACTIVE); break; }
            } else {
                ok = gv_volcar_em(0, 0, y0, L, h, 0, y0, dst_po, h, false, false);
                rastro_marca(RASTRO_EXP(paso, k, 1));
                if (!ok) { snformatar(m, (size_t)nm, "el motor no acepto la franja %d", k); parado = gv_esperar(250); break; }
                if (gv_opc & GVO_CEGO) gv_pausa_cega(20);
                if (e->toca) {
                    /* La CPU escribe en la pantalla visible (un cuadrado de la fila de marcas) mientras el motor copia. */
                    prazo_t p = prazo_ms(1000);
                    uint32_t n = 0;
                    while ((rd(RBBM_STATUS) & RBBM_ACTIVE) && !prazo_vencido(&p)) acel_marca_paso(fila_m, 49, (int)(n++ & 1), false);
                }
            }
            parado = gv_esperar(1000);   /* holgado: un plazo agotado en falso cuesta un reinicio (la GART se aparca) */
            t1 = prazo_tsc();
            if (!parado) {
                rastro_marca(RASTRO_EXP(paso, k, 3));
                snformatar(m, (size_t)nm, "el motor no termina la franja %d (filas %d a %d)", k, y0, y0 + h - 1);
                ok = false;
                break;
            }
            rastro_marca(RASTRO_EXP(paso, k, 2));
            uint32_t us = gv_us(t0, t1), dk = 0;
            us_total += us;
            if (us > us_max) { us_max = us; k_max = k; }
            for (int y = y0; y < y0 + h; y++)
                for (int x = 0; x < L; x++) {
                    uint32_t g = DST(x, y) & 0x00FFFFFFu, c = src[y * L + x] & 0x00FFFFFFu;
                    if (g != c) { if (!dif && !dk) { d_x = (uint32_t)x; d_y = (uint32_t)y; d_g = g; d_c = c; } dk++; }
                }
            dif += dk;
            rastro_marca(RASTRO_EXP(paso, k, dk ? 5 : 4));
            if (e->tela) TESTIGO(k);   /* deshecho en el búfer de atrás (la pantalla la redibuja quien llama) */
            acel_marca_paso(fila_m, k, dk ? 2 : 1, e->tela);
            if (dk && dif == dk) klog("[ACEL2D] radeon: GART paso %d: la franja %d llega distinta (%u pixeles)\n", paso, k, dk);
        }
        /* Con el plazo agotado no se pinta nada (el motor sigue ocupado): queda el amarillo de la franja k. */
    }
    #undef TESTIGO
    #undef DST
    if (ok && dif) { snformatar(m, (size_t)nm, "%u pixeles distintos (el primero en %u,%u: gpu %x, ram %x)", dif, d_x, d_y, d_g, d_c); ok = false; }
    klog("[ACEL2D] radeon: GART paso %d: %d de %d franjas; %u us en total, la mas lenta %u us (franja %d); %u pixeles distintos%s; "
         "motor %s; RBBM_STATUS %x, GART_ERROR %x\n", paso, k, nf, us_total, us_max, k_max, dif, dif ? "" : " (IGUAL)",
         parado ? "parado" : "OCUPADO", rd(RBBM_STATUS), pcie_rd(PCIE_TX_GART_ERROR));
    if (dif) klog("[ACEL2D] radeon: GART paso %d: primer distinto en %u,%u: gpu %x ram %x\n", paso, d_x, d_y, d_g, d_c);
    gv_pcie_log("tras el experimento");
    rastro_pci_erros_log("tras el experimento");
    if (!parado) {
        klog("[ACEL2D] radeon: GART paso %d: el motor no para: el bufer no se suelta (la GART quedara aparcada)\n", paso);
        return false;   /* el búfer de prueba no se libera: la GART aún lo apunta */
    }
    char m2[8];
    gv_mapear(0, NULL, 0, 0, 0, m2, sizeof m2);
    if (!e->tras) kfree(src);
    if (e->tela && my0 + 10 <= H && L >= 602)
        for (int j = 0; j < 10; j++) for (int i = 0; i < 602; i++) tras_b[(my0 + j) * L + i] = salva[j * 602 + i];
    return ok;
}

/* dbg5: pasos 20 a 25, las opciones de depuración. */
static bool gv_opcao(int paso)
{
    if (paso == 20) gv_opc = 0;
    else gv_opc ^= 1u << (paso - 21);
    klog("[ACEL2D] radeon: GART opciones de depuracion: %x =%s%s%s%s%s%s%s\n", gv_opc, gv_opc ? "" : " ninguna",
         gv_opc & GVO_RELOJ ? " RELOJ" : "", gv_opc & GVO_AGP ? " AGP" : "", gv_opc & GVO_SEM_SNOOP ? " SNOOP" : "",
         gv_opc & GVO_CEGO ? " CIEGO" : "", gv_opc & GVO_MARCAS ? " MARCAS" : "", gv_opc & GVO_MMIO ? " MMIO" : "");
    rastro_marca(RASTRO_VALOR(0x40) | gv_opc);
    return true;
}

static bool gv_paso(int paso, char *m, int nm)
{
    if (paso >= 20 && paso <= 26) return gv_opcao(paso);
    if (paso >= 27 && paso <= 35) return gpu3d_r500_opcao(paso);   /* D387: la valla del 3D y su espera */
    if (paso < 0 || paso > GV_EXP_ULTIMO) { snformatar(m, (size_t)nm, "pasos del 0 al %d y del 20 al 35", GV_EXP_ULTIMO); return false; }
    if (gt.ligada) { snformatar(m, (size_t)nm, "la GART esta en uso: antes «acel2d volcado apagar» y «acel2d vuelta apagar»"); return false; }
    gv_marca(paso, 0, "solo lectura de registros");
    bool ok = gv_preparar_ate(paso > 4 ? 4 : paso, m, nm);
    if (ok && paso >= 5 && paso <= 6) {
        gv_marca(paso, paso, paso == 5 ? "copia de una pagina por la GART" : "copia de 1024x64 por la GART");
        ok = gv_paso_copia(paso, m, nm);
    }
    if (ok && paso >= GV_EXP_PRIMEIRO) ok = gv_experimento(paso, m, nm);
    if (gt.ligada) {
        if (paso >= 3) gv_detalle("al final del paso");
        gv_soltar();
    }
    klog("[ACEL2D] radeon: GART paso %d: %s%s%s\n", paso, ok ? "BIEN" : "FALLO", ok ? "" : ": ", ok ? "" : m);
    return ok;
}

static const acel_volcado_t radeon_volcado = { gv_preparar, gv_mapear, gv_volcar, gv_esperar, gv_soltar, gv_detalle,
                                               gv_mapear_escrita, gv_retornar, gv_paso };

const acel_driver_t acel_radeon = {
    "radeon", ACEL2D_RADEON, ra_iniciar, ra_copiar, ra_preencher, ra_esperar, NULL, ra_desligar,
    true,    /* la zona de prueba va debajo de la pantalla visible */
    0, false, NULL, false,
    &radeon_volcado,   /* D387 */
};
