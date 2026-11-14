// SPDX-License-Identifier: GPL-2.0-or-later
/* acel_intel.c - el blitter de las GPU de Intel, de Gen3 a Gen6 (D195).
 *
 * SIN PROBAR EN HARDWARE: QEMU no emula ninguna GPU de Intel. Por eso la capa
 * común solo lo activa si su autoprueba del arranque da los mismos píxeles que
 * la CPU y termina a tiempo; si no, apaga el ring y se queda la CPU.
 *
 * Las órdenes son las del motor 2D («BLT») de Intel, iguales de Gen3 a Gen6:
 *   XY_COLOR_BLT     (0x50): rellena un rectángulo con un color (ROP 0xF0, PATCOPY);
 *   XY_SRC_COPY_BLT  (0x53): copia un rectángulo (ROP 0xCC, SRCCOPY);
 * con 32 bits por píxel (BR13 bits 25:24 = 11b) y escribiendo los cuatro bytes.
 * Se mandan por un ring buffer: una zona de la memoria de vídeo donde la CPU
 * escribe las órdenes y avanza TAIL; la GPU las ejecuta y avanza HEAD.
 *   - Gen3 a Gen5 (945GM del Amilo, G33, GM45, Ironlake): el ring principal
 *     (RCS, registros 0x2030-0x203C), que también ejecuta las órdenes 2D;
 *   - Gen6 (HD 2000/3000 del G470): el ring propio del blitter (BCS,
 *     0x22030-0x2203C), y antes hay que despertar la GPU (FORCEWAKE), porque
 *     sus registros no responden mientras duerme.
 *
 * Memoria: el VBE dejó la pantalla en la memoria robada a la RAM, vista por la
 * CPU en la apertura (BAR 2, GMADR) y por la GPU en el GTT con la misma
 * posición: la dirección gráfica de la pantalla es vesa_fb - GMADR. El ring va
 * al final de la memoria que anunció el VBE (ya está en el GTT), lejos de la
 * pantalla y de la zona de prueba.
 *
 * Copias solapadas: no se confía en que el blitter elija la dirección: una
 * copia que se pisa se parte en franjas que no se solapan (de alto o de ancho
 * igual al desplazamiento), en el orden que no pisa lo que falta por copiar,
 * con un vaciado (MI_FLUSH) entre franjas.
 *
 * Qué hay que comprobar en el hardware: docs/decisoes.md, D195. */
#include "acel2d.h"
#include "console.h"
#include "paginacao.h"
#include "texto.h"

#define RING_BYTES     (64 * 1024)
#define MI_NOOP        0x00000000u
#define MI_FLUSH       0x02000000u                 /* Gen3-5: MI_INSTR(0x04, 0) */
#define MI_FLUSH_DW    ((0x26u << 23) | 1)         /* Gen6: 3 palabras (y un NOOP para alinear) */
#define XY_COLOR_BLT   ((2u << 29) | (0x50u << 22) | (3u << 20) | 4)   /* 6 palabras, escribe alfa y RGB */
#define XY_SRC_COPY    ((2u << 29) | (0x53u << 22) | (3u << 20) | 6)   /* 8 palabras */
#define BR13_32BPP     (3u << 24)
#define ROP_PATCOPY    (0xF0u << 16)
#define ROP_SRCCOPY    (0xCCu << 16)

/* Registros del ring (desplazamientos desde la base de cada ring). */
#define RCS_BASE       0x02000
#define BCS_BASE       0x22000
#define R_TAIL         0x30
#define R_HEAD         0x34
#define R_START        0x38
#define R_CTL          0x3C
#define R_MI_MODE      0x9C          /* bit 8: STOP_RING (Gen4 en adelante); solo para el log */
#define CTL_VALIDO     1u
#define HEAD_MASCARA   0x001FFFFCu
#define FORCEWAKE      0xA18C
#define FORCEWAKE_ACK  0x130090

static volatile uint8_t *mmio;
static uint32_t mmio_fis;
static volatile uint32_t *ring;
static uint32_t ring_fis;
static uint32_t ring_gfx, fb_gfx, pitch, tail, base;
static int gen;
static bool ligado, acordada;

static inline uint32_t rd(uint32_t r)              { return *(volatile uint32_t *)(mmio + r); }
static inline void     wr(uint32_t r, uint32_t v)  { *(volatile uint32_t *)(mmio + r) = v; }

/* Espacio libre en el ring (bytes), con HEAD leído del registro. */
static uint32_t ring_livre(void)
{
    uint32_t head = rd(base + R_HEAD) & HEAD_MASCARA;
    return (head - tail - 8) & (RING_BYTES - 1);
}

static void avancar(void);

static bool reservar(uint32_t palavras)
{
    if (ring_livre() >= palavras * 4 + 16) return true;
    avancar();   /* lo que ya está escrito tiene que salir para que HEAD avance */
    acel_prazo_t p;
    acel_prazo(&p, 250);
    while (ring_livre() < palavras * 4 + 16)
        if (acel_prazo_esgotado(&p)) return false;
    return true;
}

static void emitir(uint32_t v)
{
    ring[tail / 4] = v;
    tail = (tail + 4) & (RING_BYTES - 1);
}

/* Alinea a 8 bytes y avisa a la GPU. */
static void avancar(void)
{
    if (tail & 7) emitir(MI_NOOP);
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    wr(base + R_TAIL, tail);
}

static void emitir_flush(void)
{
    if (gen >= 6) { emitir(MI_FLUSH_DW); emitir(0); emitir(0); emitir(MI_NOOP); }
    else { emitir(MI_FLUSH); emitir(MI_NOOP); }
}

static bool in_esperar(void)
{
    acel_prazo_t p;
    acel_prazo(&p, 250);
    while ((rd(base + R_HEAD) & HEAD_MASCARA) != tail)
        if (acel_prazo_esgotado(&p)) {
            klog("[ACEL2D] intel: tiempo agotado: HEAD %x TAIL %x CTL %x\n", rd(base + R_HEAD), tail, rd(base + R_CTL));
            return false;
        }
    return true;
}

static bool blit_copia(int ox, int oy, int dx, int dy, int l, int a)
{
    if (!reservar(16)) return false;
    emitir(XY_SRC_COPY);
    emitir(BR13_32BPP | ROP_SRCCOPY | pitch);
    emitir(((uint32_t)dy << 16) | (uint32_t)dx);
    emitir(((uint32_t)(dy + a) << 16) | (uint32_t)(dx + l));
    emitir(fb_gfx);
    emitir(((uint32_t)oy << 16) | (uint32_t)ox);
    emitir(pitch);
    emitir(fb_gfx);
    emitir_flush();
    return true;
}

static bool in_copiar(int ox, int oy, int dx, int dy, int l, int a)
{
    bool solapa = dx < ox + l && ox < dx + l && dy < oy + a && oy < dy + a;
    bool ok = true;
    if (!solapa) ok = blit_copia(ox, oy, dx, dy, l, a);
    else if (dy != oy) {
        int h = dy > oy ? dy - oy : oy - dy;
        if (dy > oy)   /* hacia abajo: desde la franja de abajo */
            for (int j = a; j > 0 && ok; j -= h) { int hh = j < h ? j : h; ok = blit_copia(ox, oy + j - hh, dx, dy + j - hh, l, hh); }
        else
            for (int j = 0; j < a && ok; j += h) { int hh = a - j < h ? a - j : h; ok = blit_copia(ox, oy + j, dx, dy + j, l, hh); }
    } else if (dx != ox) {
        int w = dx > ox ? dx - ox : ox - dx;
        if (dx > ox)   /* a la derecha: desde la franja de la derecha */
            for (int i = l; i > 0 && ok; i -= w) { int ww = i < w ? i : w; ok = blit_copia(ox + i - ww, oy, dx + i - ww, dy, ww, a); }
        else
            for (int i = 0; i < l && ok; i += w) { int ww = l - i < w ? l - i : w; ok = blit_copia(ox + i, oy, dx + i, dy, ww, a); }
    }
    avancar();
    return ok;
}

static bool in_preencher(int x, int y, int l, int a, uint32_t cor)
{
    if (!reservar(12)) return false;
    emitir(XY_COLOR_BLT);
    emitir(BR13_32BPP | ROP_PATCOPY | pitch);
    emitir(((uint32_t)y << 16) | (uint32_t)x);
    emitir(((uint32_t)(y + a) << 16) | (uint32_t)(x + l));
    emitir(fb_gfx);
    emitir(cor);
    emitir_flush();
    avancar();
    return true;
}

static void in_desligar(void)
{
    if (ligado) {
        wr(base + R_CTL, 0);   /* el ring se para; la pantalla no depende de él */
        wr(base + R_HEAD, 0);
        wr(base + R_TAIL, 0);
        ligado = false;
    }
    if (acordada) { wr(FORCEWAKE, 0); acordada = false; }
    klog("[ACEL2D] intel: ring parado\n");
}

/* Gen6: la GPU tiene que estar despierta para que sus registros respondan. */
static bool acordar(char *m, int nm)
{
    acel_prazo_t p;
    acel_prazo(&p, 50);
    while (rd(FORCEWAKE_ACK) & 1)
        if (acel_prazo_esgotado(&p)) break;
    wr(FORCEWAKE, 1);
    acel_prazo(&p, 50);
    while (!(rd(FORCEWAKE_ACK) & 1))
        if (acel_prazo_esgotado(&p)) { snformatar(m, (size_t)nm, "FORCEWAKE sin respuesta (ack %x)", rd(FORCEWAKE_ACK)); return false; }
    acordada = true;
    return true;
}

static bool in_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *m, int nm)
{
    gen = geracao;
    base = gen >= 6 ? BCS_BASE : RCS_BASE;
    uint32_t bar0 = pci_le32(d, 0x10), gmadr = pci_le32(d, 0x18);
    /* Gen4 en adelante: los dos BAR son de 64 bits; por encima de 4 GB no se llega. */
    if ((((bar0 >> 1) & 3) == 2 && pci_le32(d, 0x14)) || (((gmadr >> 1) & 3) == 2 && pci_le32(d, 0x1C))) {
        snformatar(m, (size_t)nm, "BAR por encima de 4 GB (MMIO %x, apertura %x)", bar0, gmadr);
        return false;
    }
    bar0 &= ~0xFu; gmadr &= ~0xFu;
    uint32_t mmio_tam = gen >= 4 ? 2 * 1024 * 1024 : 512 * 1024;
    klog("[ACEL2D] intel: Gen%d, MMIO %x (%u KB), apertura %x, pantalla %x, %u KB de memoria del VBE, ring %s\n",
         gen, bar0, mmio_tam >> 10, gmadr, t->fb_fisico, t->memoria >> 10, gen >= 6 ? "BCS" : "RCS");
    if (!bar0 || !gmadr) { snformatar(m, (size_t)nm, "BAR sin asignar (MMIO %x, apertura %x)", bar0, gmadr); return false; }
    if (t->fb_fisico < gmadr || t->fb_fisico - gmadr >= 0x20000000u) {
        snformatar(m, (size_t)nm, "la pantalla (%x) no esta en la apertura (%x)", t->fb_fisico, gmadr);
        return false;
    }
    fb_gfx = t->fb_fisico - gmadr;
    pitch = t->pitch;
    if (pitch & 3 || pitch >= 32768) { snformatar(m, (size_t)nm, "pitch %u no valido para el blitter", pitch); return false; }
    /* El ring al final de la memoria del VBE, por encima de la pantalla y de la zona de prueba. */
    uint32_t usado = (t->pitch * (uint32_t)(t->alt + 64) + 0xFFFu) & ~0xFFFu;
    uint32_t ring_off = (t->memoria - RING_BYTES) & ~0xFFFu;
    if (t->memoria < RING_BYTES || ring_off < usado) {
        snformatar(m, (size_t)nm, "sin memoria de video para el ring (%u KB, la pantalla usa %u KB)", t->memoria >> 10, usado >> 10);
        return false;
    }
    /* Memoria y bus maestro (la GPU lee el ring y la pantalla por el GTT); solo
     * ahora, con la pantalla comprobada en su apertura. */
    uint32_t cmd = pci_le32(d, 0x04);
    if ((cmd & 0x06) != 0x06) pci_escreve32(d, 0x04, (cmd & 0xFFFFu) | 0x06);
    if (!mmio || mmio_fis != bar0) {
        mmio = (volatile uint8_t *)janela_mapear_tipo(bar0, mmio_tam, PG_PCD | PG_PWT);
        mmio_fis = bar0;
    }
    if (!ring || ring_fis != t->fb_fisico + ring_off) {
        ring = (volatile uint32_t *)janela_mapear_tipo(t->fb_fisico + ring_off, RING_BYTES, PG_PCD | PG_PWT);
        ring_fis = t->fb_fisico + ring_off;
    }
    if (!mmio || !ring) { snformatar(m, (size_t)nm, "sin ventana para el MMIO o el ring"); return false; }
    ring_gfx = fb_gfx + ring_off;
    if (gen >= 6 && !acordar(m, nm)) return false;
    uint32_t ctl = rd(base + R_CTL);
    klog("[ACEL2D] intel: antes: CTL %x HEAD %x TAIL %x START %x MI_MODE %x\n", ctl, rd(base + R_HEAD), rd(base + R_TAIL),
         rd(base + R_START), rd(base + R_MI_MODE));
    if (ctl & CTL_VALIDO) {
        snformatar(m, (size_t)nm, "el ring ya estaba en uso (CTL %x): no se toca", ctl);
        if (acordada) { wr(FORCEWAKE, 0); acordada = false; }
        return false;
    }
    for (uint32_t i = 0; i < RING_BYTES / 4; i++) ring[i] = MI_NOOP;
    wr(base + R_CTL, 0);
    wr(base + R_HEAD, 0);
    wr(base + R_TAIL, 0);
    wr(base + R_START, ring_gfx);
    wr(base + R_CTL, ((RING_BYTES - 4096) & 0x001FF000u) | CTL_VALIDO);
    tail = 0;
    ligado = true;
    uint32_t head = rd(base + R_HEAD), start = rd(base + R_START);
    ctl = rd(base + R_CTL);
    klog("[ACEL2D] intel: ring en %x (grafica %x): CTL %x HEAD %x START %x\n", ring_fis, ring_gfx, ctl, head, start);
    if ((head & HEAD_MASCARA) != 0 || start != ring_gfx || !(ctl & CTL_VALIDO)) {
        in_desligar();
        snformatar(m, (size_t)nm, "el ring no arranco (CTL %x HEAD %x START %x)", ctl, head, start);
        return false;
    }
    /* Una orden vacía: el ring tiene que avanzar antes de mandar nada de verdad. */
    emitir_flush();
    avancar();
    if (!in_esperar()) { in_desligar(); snformatar(m, (size_t)nm, "el ring no avanza (HEAD %x)", rd(base + R_HEAD)); return false; }
    return true;
}

const acel_driver_t acel_intel = {
    "intel", ACEL2D_INTEL, in_iniciar, in_copiar, in_preencher, in_esperar, NULL, in_desligar,
    true,    /* la zona de prueba va debajo de la pantalla visible */
};
