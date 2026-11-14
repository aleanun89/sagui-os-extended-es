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
 * la X1400: docs/decisoes.md, D195. */
#include "acel2d.h"
#include "console.h"
#include "paginacao.h"
#include "texto.h"

#define MMIO_TAM               0x10000
#define RBBM_STATUS            0x0E40
#define   RBBM_FIFOCNT         0x007Fu
#define   RBBM_ACTIVE          0x80000000u
#define MC_FB_LOCATION         0x0148
#define CRTC_OFFSET            0x0224
#define CRTC_PITCH             0x022C      /* bits 10:0: pitch en grupos de 8 píxeles */
#define SRC_PITCH_OFFSET       0x1428
#define DST_PITCH_OFFSET       0x142C
#define SRC_Y_X                0x1434
#define DST_Y_X                0x1438
#define DST_HEIGHT_WIDTH       0x143C
#define DP_GUI_MASTER_CNTL     0x146C
#define DP_BRUSH_FRGD_CLR      0x147C
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

#define GMC_SRC_PITCH_OFFSET   (1u << 0)
#define GMC_DST_PITCH_OFFSET   (1u << 1)
#define GMC_BRUSH_SOLIDO       (13u << 4)
#define GMC_BRUSH_NADA         (15u << 4)
#define GMC_DST_32BPP          (6u << 8)
#define GMC_SRC_COR            (3u << 12)
#define ROP3_S                 0x00CC0000u
#define ROP3_P                 0x00F00000u
#define GMC_SRC_MEMORIA        (2u << 24)
#define GMC_SEM_COMPARAR       (1u << 28)

static volatile uint8_t *mmio;
static uint32_t mmio_fis, pitch_offset;
static int gen;

static inline uint32_t rd(uint32_t r)             { return *(volatile uint32_t *)(mmio + r); }
static inline void     wr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(mmio + r) = v; }

static bool fifo(uint32_t n)
{
    acel_prazo_t p;
    acel_prazo(&p, 250);
    while ((rd(RBBM_STATUS) & RBBM_FIFOCNT) < n)
        if (acel_prazo_esgotado(&p)) return false;
    return true;
}

static bool ra_esperar(void)
{
    acel_prazo_t p;
    if (!fifo(64)) { klog("[ACEL2D] radeon: la FIFO no se vacia (RBBM_STATUS %x)\n", rd(RBBM_STATUS)); return false; }
    acel_prazo(&p, 250);
    while (rd(RBBM_STATUS) & RBBM_ACTIVE)
        if (acel_prazo_esgotado(&p)) { klog("[ACEL2D] radeon: el motor no para (RBBM_STATUS %x)\n", rd(RBBM_STATUS)); return false; }
    uint32_t cache = gen >= 1 ? R300_DSTCACHE_CTLSTAT : RB3D_DSTCACHE_CTLSTAT;
    wr(cache, rd(cache) | DC_FLUSH_ALL);
    acel_prazo(&p, 250);
    while (rd(cache) & DC_BUSY)
        if (acel_prazo_esgotado(&p)) { klog("[ACEL2D] radeon: la cache 2D no se vacia (%x)\n", rd(cache)); return false; }
    return true;
}

static bool ra_preencher(int x, int y, int l, int a, uint32_t cor)
{
    if (!fifo(7)) return false;
    wr(DP_GUI_MASTER_CNTL, GMC_DST_PITCH_OFFSET | GMC_BRUSH_SOLIDO | GMC_DST_32BPP | GMC_SRC_COR | ROP3_P | GMC_SEM_COMPARAR);
    wr(DP_BRUSH_FRGD_CLR, cor);
    wr(DP_WRITE_MASK, 0xFFFFFFFFu);
    wr(DP_CNTL, DST_X_ESQ_DIR | DST_Y_CIMA_BAIXO);
    wr(DST_PITCH_OFFSET, pitch_offset);
    wr(DST_Y_X, ((uint32_t)y << 16) | (uint32_t)x);
    wr(DST_HEIGHT_WIDTH, ((uint32_t)a << 16) | (uint32_t)l);
    return true;
}

static bool ra_copiar(int ox, int oy, int dx, int dy, int l, int a)
{
    if (!fifo(8)) return false;
    /* Si el destino está a la derecha (abajo) del origen, se recorre de derecha a
     * izquierda (de abajo arriba) empezando por la última columna (fila). */
    uint32_t dir = 0;
    if (ox >= dx) dir |= DST_X_ESQ_DIR; else { ox += l - 1; dx += l - 1; }
    if (oy >= dy) dir |= DST_Y_CIMA_BAIXO; else { oy += a - 1; dy += a - 1; }
    wr(DP_GUI_MASTER_CNTL, GMC_SRC_PITCH_OFFSET | GMC_DST_PITCH_OFFSET | GMC_BRUSH_NADA | GMC_DST_32BPP | GMC_SRC_COR |
                           ROP3_S | GMC_SRC_MEMORIA | GMC_SEM_COMPARAR);
    wr(DP_WRITE_MASK, 0xFFFFFFFFu);
    wr(DP_CNTL, dir);
    wr(DST_PITCH_OFFSET, pitch_offset);
    wr(SRC_PITCH_OFFSET, pitch_offset);
    wr(SRC_Y_X, ((uint32_t)oy << 16) | (uint32_t)ox);
    wr(DST_Y_X, ((uint32_t)dy << 16) | (uint32_t)dx);
    wr(DST_HEIGHT_WIDTH, ((uint32_t)a << 16) | (uint32_t)l);
    return true;
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

const acel_driver_t acel_radeon = {
    "radeon", ACEL2D_RADEON, ra_iniciar, ra_copiar, ra_preencher, ra_esperar, NULL, ra_desligar,
    true,    /* la zona de prueba va debajo de la pantalla visible */
};
