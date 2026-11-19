// SPDX-License-Identifier: GPL-2.0-or-later
/* acel_tdfx.c - el motor 2D de las 3dfx Banshee, Voodoo3 y Voodoo4/5 (VSA-100) (D305).
 *
 * SIN PROBAR EN HARDWARE REAL: solo en 86Box (Voodoo3 2000 y 3500 AGP), que emula
 * el motor 2D de la Banshee/Voodoo3. QEMU no emula ninguna 3dfx. La capa común
 * (acel2d.c) solo lo activa si su autoprueba coincide con la CPU píxel a píxel y
 * termina a tiempo; el vigilante lo apaga si después deja de responder.
 *
 * Desde la Banshee, las 3dfx tienen un motor 2D que se programa escribiendo sus
 * registros por MMIO (BAR 0), sin la cola de órdenes en memoria (CMDFIFO), que
 * aquí no se usa: cada escritura entra en la FIFO del PCI de la tarjeta, que
 * tiene 32 huecos; antes de escribir se mira cuántos quedan (status, bits 4:0).
 *
 * El BAR 0 (32 MB) tiene, entre otros:
 *   0x000000  registros de E/S: status, vgaInit0, vidProcCfg, vidDesktopStartAddr,
 *             vidDesktopOverlayStride (lo que dejó el VBE: se leen, no se tocan);
 *   0x100000  registros del 2D: clip, dstBaseAddr/dstFormat, srcBaseAddr/srcFormat,
 *             colorFore, srcXY, dstSize, dstXY y command.
 * El BAR 1 (32 MB) es la memoria de vídeo: el framebuffer del VBE está dentro.
 *
 * Una operación: tamaño (dstSize), esquina de destino (dstXY), de origen (srcXY,
 * en las copias) y la orden en command con el bit «initiate» (8), que la lanza:
 *   rellenar:  orden 5 (rectangle fill), ROP 0xCC con colorFore como origen;
 *   copiar:    orden 1 (screen-to-screen blit), ROP 0xCC; los bits 14 y 15 dan la
 *              dirección del recorrido (de derecha a izquierda, de abajo arriba),
 *              y entonces las esquinas que se dan son la última columna / fila:
 *              así las copias solapadas salen como memmove.
 * Para esperar: la FIFO del PCI vacía (status, bits 4:0 = 0x1F) y el bit 9 (la
 * tarjeta ocupada) apagado, tres lecturas seguidas.
 *
 * Formatos (dstFormat/srcFormat, bits 18:16): 3 = 16 bits, 4 = 24 bits, 5 = 32
 * bits; el stride en bytes en los bits 12:0. El motor trabaja en 16, 24 y 32 bits,
 * así que este acelerador acepta las tres profundidades (outras_profundidades):
 * la BIOS de la Voodoo3 que trae 86Box solo ofrece modos VESA de 16 y 24 bits.
 *
 * Referencias: la documentación pública de 3dfx («Voodoo Banshee 2D/3D
 * Specification», «Voodoo3 High-Performance Graphics Engine») y los números de
 * registro del driver tdfx de XFree86/X.Org (licencia MIT/X11). El código de
 * 86Box (GPL v2 o posterior) se leyó solo para saber qué emula. El código es
 * propio; nada viene del tdfxfb de Linux. */
#include "acel2d.h"
#include "console.h"
#include "paginacao.h"
#include "texto.h"

/* Registros de E/S (BAR 0 + 0). */
#define VENTANA_TAM            0x1000
#define SST_STATUS             0x000
#define   ST_FIFO_LIVRE        0x1Fu        /* huecos libres en la FIFO del PCI */
#define   ST_OCUPADO           (1u << 9)    /* la tarjeta (2D, 3D o la FIFO) trabaja */
#define VGA_INIT0              0x028
#define   VGA_INIT0_ESTENDIDO  (1u << 12)   /* la pantalla la saca el procesador de vídeo, no la VGA */
#define VID_PROC_CFG           0x05C
#define   VPC_ATIVO            (1u << 0)
#define   VPC_FORMATO(v)       (((v) >> 18) & 7u)   /* 0 = 8 bits, 1 = 565, 2 = 24, 3 = 32 */
#define   VPC_DESKTOP_TILE     (1u << 24)
#define VID_DESKTOP_START      0x0E4
#define VID_DESKTOP_STRIDE     0x0E8        /* bits 13:0: bytes por línea del escritorio (lineal) */

/* Registros del 2D (BAR 0 + 0x100000). */
#define DESLOC_2D              0x100000u
#define R2_CLIP0_MIN           0x008
#define R2_CLIP0_MAX           0x00C
#define R2_DST_BASE            0x010
#define R2_DST_FORMAT          0x014
#define R2_ROP                 0x030
#define R2_SRC_BASE            0x034
#define R2_COMMAND_EXTRA       0x038
#define R2_SRC_FORMAT          0x054
#define R2_COLOR_BACK          0x060
#define R2_COLOR_FORE          0x064
#define R2_SRC_XY              0x05C
#define R2_DST_SIZE            0x068
#define R2_DST_XY              0x06C
#define R2_COMMAND             0x070

#define CMD_BLIT_TELA          1u           /* screen-to-screen blit */
#define CMD_RETANGULO          5u           /* rectangle fill */
#define CMD_INICIAR            (1u << 8)
#define CMD_DIR_X              (1u << 14)   /* de derecha a izquierda */
#define CMD_DIR_Y              (1u << 15)   /* de abajo arriba */
#define ROP_ORIGEM             (0xCCu << 24)
#define FMT_16                 (3u << 16)
#define FMT_24                 (4u << 16)
#define FMT_32                 (5u << 16)

#define MMIO_TAM_BAR0          0x2000000u   /* 32 MB de registros */

static volatile uint8_t *io, *r2;
static uint32_t io_fis;
static uint32_t base_tela, formato;          /* lo que se escribe en dst/srcBaseAddr y dst/srcFormat */

static inline uint32_t rd(uint32_t r)              { return *(volatile uint32_t *)(io + r); }
static inline void     wr2(uint32_t r, uint32_t v) { *(volatile uint32_t *)(r2 + r) = v; }

/* Espera a que queden n huecos en la FIFO del PCI de la tarjeta. */
static bool fifo(uint32_t n)
{
    acel_prazo_t p;
    acel_prazo(&p, 250);
    while ((rd(SST_STATUS) & ST_FIFO_LIVRE) < n)
        if (acel_prazo_esgotado(&p)) return false;
    return true;
}

static bool tf_esperar(void)
{
    acel_prazo_t p;
    acel_prazo(&p, 250);
    int livres = 0;
    /* Tres lecturas seguidas con la FIFO del PCI vacía y sin el bit de ocupado (el driver tdfx
     * de X mira solo el bit). La FIFO vacía hace falta: en 86Box el bit de ocupado no cuenta
     * las órdenes del 2D que aún esperan en la FIFO, y la CPU escribía en la pantalla mientras
     * el motor todavía desplazaba el texto (letras rotas y copias del cursor). */
    while (livres < 3) {
        uint32_t st = rd(SST_STATUS);
        if ((st & ST_OCUPADO) || (st & ST_FIFO_LIVRE) != ST_FIFO_LIVRE) livres = 0;
        else livres++;
        if (livres < 3 && acel_prazo_esgotado(&p)) {
            klog("[ACEL2D] tdfx: el motor no para (status %x)\n", rd(SST_STATUS));
            return false;
        }
    }
    return true;
}

static bool tf_preencher(int x, int y, int l, int a, uint32_t cor)
{
    if (!fifo(4)) return false;
    wr2(R2_COLOR_FORE, cor);
    wr2(R2_DST_SIZE, ((uint32_t)a << 16) | (uint32_t)l);
    wr2(R2_DST_XY, ((uint32_t)y << 16) | (uint32_t)x);
    wr2(R2_COMMAND, CMD_RETANGULO | CMD_INICIAR | ROP_ORIGEM);
    return true;
}

static bool tf_copiar(int ox, int oy, int dx, int dy, int l, int a)
{
    if (!fifo(4)) return false;
    /* Si el destino está a la derecha (abajo) del origen, se recorre de derecha a
     * izquierda (de abajo arriba) empezando por la última columna (fila). */
    uint32_t dir = 0;
    if (ox < dx) { dir |= CMD_DIR_X; ox += l - 1; dx += l - 1; }
    if (oy < dy) { dir |= CMD_DIR_Y; oy += a - 1; dy += a - 1; }
    wr2(R2_SRC_XY, ((uint32_t)oy << 16) | (uint32_t)ox);
    wr2(R2_DST_SIZE, ((uint32_t)a << 16) | (uint32_t)l);
    wr2(R2_DST_XY, ((uint32_t)dy << 16) | (uint32_t)dx);
    wr2(R2_COMMAND, CMD_BLIT_TELA | CMD_INICIAR | ROP_ORIGEM | dir);
    return true;
}

static void tf_desligar(void)
{
    /* El motor 2D no toca la pantalla por su cuenta: basta con no usarlo más. */
    klog("[ACEL2D] tdfx: motor 2D sin usar\n");
}

static void tf_detalle(const char *fase)
{
    if (!io) return;
    klog("[ACEL2D] tdfx (%s): status %x, vgaInit0 %x, vidProcCfg %x, escritorio en %x stride %x; dstBase %x formato %x\n",
         fase, rd(SST_STATUS), rd(VGA_INIT0), rd(VID_PROC_CFG), rd(VID_DESKTOP_START), rd(VID_DESKTOP_STRIDE),
         base_tela, formato);
}

/* Un BAR de memoria de 32 bits (sin la parte alta de 64). 0 si no lo es. */
static uint32_t bar_mem32(const pci_disp_t *d, uint8_t reg)
{
    uint32_t v = pci_le32(d, reg);
    if (v & 1) return 0;                                         /* de E/S */
    if (((v >> 1) & 3) == 2 && pci_le32(d, (uint8_t)(reg + 4)) != 0) return 0;   /* por encima de 4 GB */
    return v & ~0xFu;
}

static bool tf_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *m, int nm)
{
    /* Sonda de solo lectura: la lista exacta de modelos, los BAR y el modo que dejó el VBE.
     * Hasta que todo coincide no se escribe nada en la tarjeta. */
    if (d->fabricante != 0x121A || (d->modelo != 0x0003 && d->modelo != 0x0005 && d->modelo != 0x0009)) {
        snformatar(m, (size_t)nm, "el modelo %04x:%04x no esta en la lista (Banshee 0003, Voodoo3 0005, VSA-100 0009)",
                   d->fabricante, d->modelo);
        return false;
    }
    uint32_t bar0 = bar_mem32(d, 0x10), bar1 = bar_mem32(d, 0x14);
    klog("[ACEL2D] tdfx: %s, BAR0 %x, BAR1 %x, pantalla %x, pitch %u, %d bytes por pixel, %u KB del VBE\n",
         geracao == 0 ? "Banshee" : geracao == 1 ? "Voodoo3" : "VSA-100", bar0, bar1, t->fb_fisico, t->pitch,
         t->bytes_px, t->memoria >> 10);
    if (!bar0 || !bar1) { snformatar(m, (size_t)nm, "BAR 0 (%x) o BAR 1 (%x) no son memoria de 32 bits", bar0, bar1); return false; }
    if (t->fb_fisico < bar1 || t->fb_fisico - bar1 >= MMIO_TAM_BAR0) {
        snformatar(m, (size_t)nm, "la pantalla (%x) no esta en el BAR 1 (%x)", t->fb_fisico, bar1);
        return false;
    }
    uint32_t desloc = t->fb_fisico - bar1;
    if (desloc + t->pitch * (uint32_t)(t->alt + ACEL2D_ZONA_A) > 0x1000000u) {
        snformatar(m, (size_t)nm, "la pantalla (desplazamiento %x) pasa de los 16 MB que alcanza el 2D", desloc);
        return false;
    }
    if (t->pitch >= 0x2000 || t->larg >= 0xFFF || t->alt + ACEL2D_ZONA_A >= 0xFFF) {
        snformatar(m, (size_t)nm, "pantalla %dx%d pitch %u: fuera de los limites del motor 2D", t->larg, t->alt, t->pitch);
        return false;
    }
    uint32_t fmt_vid = t->bytes_px == 4 ? 3u : t->bytes_px == 3 ? 2u : 1u;
    formato = t->pitch | (t->bytes_px == 4 ? FMT_32 : t->bytes_px == 3 ? FMT_24 : FMT_16);
    base_tela = desloc;
    if (!io || io_fis != bar0) {
        io_fis = bar0;
        io = (volatile uint8_t *)janela_mapear_tipo(bar0, VENTANA_TAM, PG_PCD | PG_PWT);
        r2 = (volatile uint8_t *)janela_mapear_tipo(bar0 + DESLOC_2D, VENTANA_TAM, PG_PCD | PG_PWT);
    }
    if (!io || !r2) { io = NULL; snformatar(m, (size_t)nm, "sin ventana para el MMIO"); return false; }
    uint32_t st = rd(SST_STATUS);
    if (st == 0xFFFFFFFFu) { snformatar(m, (size_t)nm, "el MMIO no responde (status %x)", st); return false; }
    uint32_t vga0 = rd(VGA_INIT0), vpc = rd(VID_PROC_CFG), ini = rd(VID_DESKTOP_START), str = rd(VID_DESKTOP_STRIDE);
    klog("[ACEL2D] tdfx: status %x, vgaInit0 %x, vidProcCfg %x (formato %u%s), escritorio en %x, stride %u\n", st, vga0,
         vpc, VPC_FORMATO(vpc), vpc & VPC_DESKTOP_TILE ? ", en mosaico" : "", ini, str & 0x3FFFu);
    /* La pantalla tiene que ser la que se cree: el procesador de vídeo activo, lineal, con el
     * formato, el pitch y el inicio del VBE. Si no, no se toca el motor. */
    if (!(vga0 & VGA_INIT0_ESTENDIDO) || !(vpc & VPC_ATIVO)) {
        snformatar(m, (size_t)nm, "el procesador de video no saca la pantalla (vgaInit0 %x, vidProcCfg %x)", vga0, vpc);
        return false;
    }
    if (vpc & VPC_DESKTOP_TILE) { snformatar(m, (size_t)nm, "el escritorio esta en mosaico (vidProcCfg %x)", vpc); return false; }
    if (VPC_FORMATO(vpc) != fmt_vid) {
        snformatar(m, (size_t)nm, "el formato del escritorio (%u) no es el del VBE (%d bytes por pixel)", VPC_FORMATO(vpc), t->bytes_px);
        return false;
    }
    if ((str & 0x3FFFu) != t->pitch) {
        snformatar(m, (size_t)nm, "el stride del escritorio (%u) no es el pitch del VBE (%u)", str & 0x3FFFu, t->pitch);
        return false;
    }
    if ((ini & 0xFFFFFFu) != desloc) {
        snformatar(m, (size_t)nm, "el escritorio empieza en %x y el VBE dice %x", ini & 0xFFFFFFu, desloc);
        return false;
    }
    if (!tf_esperar()) { snformatar(m, (size_t)nm, "el motor no esta libre (status %x)", rd(SST_STATUS)); return false; }
    /* Lo que el resto de órdenes da por hecho: sin recorte, sin claves de color, origen y
     * destino en la pantalla con su formato. */
    if (!fifo(10)) { snformatar(m, (size_t)nm, "la FIFO del PCI no responde (status %x)", rd(SST_STATUS)); return false; }
    wr2(R2_CLIP0_MIN, 0);
    wr2(R2_CLIP0_MAX, (0xFFFu << 16) | 0xFFFu);
    wr2(R2_COMMAND_EXTRA, 0);
    wr2(R2_ROP, 0xCCCCCCu);
    wr2(R2_DST_FORMAT, formato);
    wr2(R2_DST_BASE, base_tela);
    wr2(R2_SRC_FORMAT, formato);
    wr2(R2_SRC_BASE, base_tela);
    wr2(R2_COLOR_BACK, 0);
    wr2(R2_COLOR_FORE, 0);
    if (!tf_esperar()) { snformatar(m, (size_t)nm, "el motor 2D no responde"); return false; }
    klog("[ACEL2D] tdfx: 2D en %x, formato %x\n", base_tela, formato);
    return true;
}

const acel_driver_t acel_tdfx = {
    "tdfx", ACEL2D_TDFX, tf_iniciar, tf_copiar, tf_preencher, tf_esperar, NULL, tf_desligar,
    true,    /* la zona de prueba va debajo de la pantalla visible */
    4096,    /* D246: menos de 64x64 píxeles lo copia la CPU. En 86Box cada orden tarda ~0,2 ms en
                empezar (despertar su hilo del 2D) y la CPU gana hasta ~8000 píxeles; en una Voodoo3
                real el punto de corte será más bajo («acel2d probar --detalle» da los tiempos) */
    true,    /* el motor sigue trabajando tras copiar(): acel_sincronizar espera */
    tf_detalle,
    true,    /* D305: 16, 24 y 32 bits */
    NULL,    /* D387: sin volcado por la GPU */
};
