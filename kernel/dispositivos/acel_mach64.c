// SPDX-License-Identifier: GPL-2.0-or-later
/* acel_mach64.c - el motor 2D de las ATI Mach64 y Rage (GX, CT, VT, Rage II, Rage Pro,
 * Rage LT/XL/Mobility) (D308).
 *
 * SIN PROBAR EN HARDWARE REAL: solo en 86Box (Mach64 VT2, 1002:5654). QEMU no emula
 * ninguna ATI. La capa común (acel2d.c) solo lo activa si su autoprueba coincide con
 * la CPU píxel a píxel y termina a tiempo; el vigilante lo apaga si después deja de
 * responder.
 *
 * Desde la primera Mach64 (1994) hasta la Rage XL, el motor 2D es el mismo: se programa
 * escribiendo sus registros por MMIO, en el «bloque 0» de 1 KB que la tarjeta pone:
 *   - al final de los primeros 8 MB de la apertura lineal (BAR 0 + 0x7FFC00), en todas
 *     desde la CT (en la GX/CX, al final de la apertura de 4 u 8 MB que dice CONFIG_CNTL);
 *   - en las Rage Pro/LT Pro/XL/Mobility, además, en el BAR 2 (4 KB): bloque 0 en +0x400.
 * Una operación:
 *   DST_OFF_PITCH   dónde está la pantalla en la memoria de vídeo (en unidades de 8
 *                   bytes, bits 19:0) y su pitch (en grupos de 8 píxeles, bits 31:22);
 *   DP_PIX_WIDTH    bits por píxel del destino y del origen (32: 6, 16: 4, 8: 2);
 *   DP_SRC          de dónde sale el color: DP_FRGD_CLR (relleno) o la memoria (copia);
 *   DP_MIX          la operación (7: el origen tal cual);
 *   DST_CNTL        el sentido del recorrido (bit 0: de izquierda a derecha, bit 1:
 *                   de arriba abajo); en las copias solapadas se empieza por la esquina
 *                   que no pisa el origen y se dan la última columna / fila;
 *   SRC_Y_X, SRC_WIDTH1, DST_Y_X y DST_HEIGHT_WIDTH, que lanza la operación.
 * Antes de escribir se mira que quepa en la FIFO (FIFO_STAT, bits 15:0: un bit por hueco
 * ocupado) y para esperar, la FIFO vacía y GUI_STAT bit 0 (el motor trabaja) apagado.
 *
 * 24 bits por píxel (D308): el motor no tiene ese formato; se dibuja en 8 bits con las
 * coordenadas y los anchos por 3. Copiar así es mover bytes y sale exacto; para rellenar,
 * DST_CNTL lleva DST_24_ROT_EN y en DST_24_ROT (bits 10:8) qué byte del color toca al
 * empezar ((x * 3 / 4) % 6, como hace el driver mach64 de X.Org). Lo comprueba la
 * autoprueba de la capa común como en 32 bits. La BIOS de la VT2 que trae 86Box solo
 * ofrece modos de 15, 16 y 24 bits; las Rage Pro de verdad suelen dar también 32.
 *
 * Referencias: la documentación pública de ATI («Mach64 Register Reference Guide»,
 * «Rage Pro / Rage XL Register Reference») y los números de registro del driver mach64
 * de X.Org (xf86-video-mach64, licencia MIT/X11; docs/terceros.md). El código de 86Box
 * (GPL v2 o posterior) se leyó solo para saber qué emula. El código es propio; nada
 * viene del atyfb de Linux. */
#include "acel2d.h"
#include "console.h"
#include "portas.h"
#include "paginacao.h"
#include "texto.h"

/* Registros del bloque 0 (desplazamiento en bytes). */
#define CRTC_OFF_PITCH         0x014
#define CRTC_GEN_CNTL          0x01C
#define   CRTC_PIX_WIDTH(v)    (((v) >> 8) & 7u)      /* 2: 8 bits, 3: 15, 4: 16, 5: 24, 6: 32 */
#define   CRTC_EXT_DISP_EN     (1u << 24)             /* la pantalla la saca el CRTC del acelerador */
#define BUS_CNTL               0x0A0
#define GEN_TEST_CNTL          0x0D0
#define   GUI_ENGINE_ENABLE    (1u << 8)
#define CONFIG_CNTL            0x0DC
#define CONFIG_CHIP_ID         0x0E0
#define DST_OFF_PITCH          0x100
#define DST_Y_X                0x10C
#define DST_HEIGHT_WIDTH       0x118
#define DST_CNTL               0x130
#define   DST_X_ESQ_DIR        (1u << 0)              /* de izquierda a derecha */
#define   DST_Y_CIMA_BAIXO     (1u << 1)              /* de arriba abajo */
#define   DST_24_ROT_EN        (1u << 7)
#define   DST_24_ROT(n)        ((uint32_t)(n) << 8)
#define SRC_OFF_PITCH          0x180
#define SRC_Y_X                0x18C
#define SRC_WIDTH1             0x190
#define SRC_CNTL               0x1B4
#define HOST_CNTL              0x240
#define PAT_CNTL               0x288
#define SC_LEFT_RIGHT          0x2A8
#define SC_TOP_BOTTOM          0x2B4
#define DP_BKGD_CLR            0x2C0
#define DP_FRGD_CLR            0x2C4
#define DP_WRITE_MASK          0x2C8
#define DP_CHAIN_MASK          0x2CC
#define DP_PIX_WIDTH           0x2D0
#define DP_MIX                 0x2D4
#define DP_SRC                 0x2D8
#define CLR_CMP_CNTL           0x308
#define FIFO_STAT              0x310
#define   FIFO_OCUPADA         0xFFFFu
#define   FIFO_ERRO            (1u << 31)
#define CONTEXT_MASK           0x320
#define GUI_STAT               0x338
#define   GUI_ATIVO            (1u << 0)

#define PIX_8                  2u
#define PIX_16                 4u
#define PIX_32                 6u
#define MIX_ORIGEM             7u                     /* el nuevo píxel es el origen */
#define MIX_DESTINO            3u                     /* el destino no cambia */
#define FONTE_FRGD             1u                     /* DP_FRGD_CLR */
#define FONTE_BLIT             3u                     /* la memoria de vídeo (SRC_*) */

/* Las familias (la «geração» de la tabla de acel2d.c). */
#define M64_GX                 0   /* Mach64 GX/CX: la apertura según CONFIG_CNTL */
#define M64_CT                 1   /* CT, ET, VT, GT (Rage, Rage II/II+/IIC) */
#define M64_PRO                2   /* Rage Pro, LT Pro, XL, Mobility: también el BAR 2 */

/* La lista exacta (identificadores del PCI), con la familia de cada uno. */
static const struct { uint16_t id; uint8_t fam; const char *nome; } modelos[] = {
    { 0x4758, M64_GX,  "Mach64 GX" },        { 0x4358, M64_GX,  "Mach64 CX" },
    { 0x4354, M64_CT,  "Mach64 CT" },        { 0x4554, M64_CT,  "Mach64 ET" },
    { 0x5654, M64_CT,  "Mach64 VT" },        { 0x5655, M64_CT,  "Mach64 VT3" },
    { 0x5656, M64_CT,  "Mach64 VT4" },
    { 0x4754, M64_CT,  "3D Rage (GT)" },     { 0x4755, M64_CT,  "3D Rage II+ (GU)" },
    { 0x4756, M64_CT,  "3D Rage IIC (GV)" }, { 0x4757, M64_CT,  "3D Rage IIC AGP (GW)" },
    { 0x4759, M64_CT,  "3D Rage IIC (GY)" }, { 0x475A, M64_CT,  "3D Rage IIC AGP (GZ)" },
    { 0x4C47, M64_CT,  "3D Rage LT (LG)" },  { 0x4C54, M64_CT,  "3D Rage LT (LT)" },
    { 0x4742, M64_PRO, "Rage Pro AGP (GB)" }, { 0x4744, M64_PRO, "Rage Pro AGP 1x (GD)" },
    { 0x4749, M64_PRO, "Rage Pro PCI (GI)" }, { 0x4750, M64_PRO, "Rage Pro PCI (GP)" },
    { 0x4751, M64_PRO, "Rage Pro PCI (GQ)" },
    { 0x4C42, M64_PRO, "Rage LT Pro AGP (LB)" }, { 0x4C44, M64_PRO, "Rage LT Pro AGP 1x (LD)" },
    { 0x4C49, M64_PRO, "Rage LT Pro PCI (LI)" }, { 0x4C50, M64_PRO, "Rage LT Pro PCI (LP)" },
    { 0x4C51, M64_PRO, "Rage LT Pro PCI (LQ)" },
    { 0x474C, M64_PRO, "Rage XC PCI-66 (GL)" }, { 0x474D, M64_PRO, "Rage XL AGP (GM)" },
    { 0x474E, M64_PRO, "Rage XC AGP (GN)" },    { 0x474F, M64_PRO, "Rage XL PCI-66 (GO)" },
    { 0x4752, M64_PRO, "Rage XL PCI (GR)" },    { 0x4753, M64_PRO, "Rage XC PCI (GS)" },
    { 0x4C4D, M64_PRO, "Rage Mobility AGP 2x (LM)" }, { 0x4C4E, M64_PRO, "Rage Mobility AGP (LN)" },
    { 0x4C52, M64_PRO, "Rage Mobility PCI (LR)" },    { 0x4C53, M64_PRO, "Rage Mobility PCI (LS)" },
};
#define N_MODELOS (int)(sizeof modelos / sizeof modelos[0])

static volatile uint8_t *regs;        /* bloque 0 */
static uint32_t pagina_fis;           /* la página de 4 KB mapeada (el bloque 0 está dentro) */
static uint32_t desloc_bloco0;        /* dónde está el bloque 0 dentro de esa página */
static uint32_t off_pitch;            /* DST_OFF_PITCH / SRC_OFF_PITCH de la pantalla */
static uint32_t pix_width;            /* DP_PIX_WIDTH */
static int bpx;                       /* bytes por píxel de la pantalla (4, 3 o 2) */
static int xmul;                      /* 3 en 24 bits (el motor dibuja en 8), 1 en los demás */
static const char *nome_modelo = "?";

static inline uint32_t rd(uint32_t r)             { return *(volatile uint32_t *)(regs + r); }
static inline void     wr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(regs + r) = v; }

/* Espera a que queden n huecos libres en la FIFO de 16 (bit 15 - n + 1 apagado). */
static bool fifo(uint32_t n)
{
    acel_prazo_t p;
    acel_prazo(&p, 250);
    while ((rd(FIFO_STAT) & FIFO_OCUPADA) > (0x8000u >> n))
        if (acel_prazo_esgotado(&p)) return false;
    return true;
}

static bool m64_esperar(void)
{
    acel_prazo_t p;
    if (!fifo(16)) { klog("[ACEL2D] mach64: la FIFO no se vacia (FIFO_STAT %x)\n", rd(FIFO_STAT)); return false; }
    /* En 86Box GUI_STAT dice «ocupado» mientras la FIFO tiene algo: tres lecturas seguidas libres. */
    int libres = 0;
    acel_prazo(&p, 250);
    while (libres < 3) {
        if (rd(GUI_STAT) & GUI_ATIVO) libres = 0; else libres++;
        if (acel_prazo_esgotado(&p)) { klog("[ACEL2D] mach64: el motor no para (GUI_STAT %x)\n", rd(GUI_STAT)); return false; }
    }
    if (rd(FIFO_STAT) & FIFO_ERRO) { klog("[ACEL2D] mach64: error de la FIFO (FIFO_STAT %x)\n", rd(FIFO_STAT)); return false; }
    return true;
}

static bool m64_preencher(int x, int y, int l, int a, uint32_t cor)
{
    if (!fifo(7)) return false;
    uint32_t dst = DST_X_ESQ_DIR | DST_Y_CIMA_BAIXO;
    x *= xmul; l *= xmul;
    if (xmul == 3) dst |= DST_24_ROT_EN | DST_24_ROT(((uint32_t)x / 4) % 6);
    wr(DP_SRC, FONTE_FRGD << 8);
    wr(DP_MIX, (MIX_ORIGEM << 16) | MIX_DESTINO);
    wr(DP_FRGD_CLR, cor);
    wr(DST_CNTL, dst);
    wr(DST_Y_X, ((uint32_t)x << 16) | (uint32_t)y);
    wr(DST_HEIGHT_WIDTH, ((uint32_t)l << 16) | (uint32_t)a);
    return true;
}

static bool m64_copiar(int ox, int oy, int dx, int dy, int l, int a)
{
    if (!fifo(8)) return false;
    ox *= xmul; dx *= xmul; l *= xmul;
    /* Si el destino está a la derecha (abajo) del origen, se recorre de derecha a
     * izquierda (de abajo arriba) empezando por la última columna (fila). */
    uint32_t dst = 0;
    if (ox >= dx) dst |= DST_X_ESQ_DIR; else { ox += l - 1; dx += l - 1; }
    if (oy >= dy) dst |= DST_Y_CIMA_BAIXO; else { oy += a - 1; dy += a - 1; }
    if (xmul == 3) dst |= DST_24_ROT_EN | DST_24_ROT(((uint32_t)dx / 4) % 6);
    wr(DP_SRC, FONTE_BLIT << 8);
    wr(DP_MIX, (MIX_ORIGEM << 16) | MIX_DESTINO);
    wr(DST_CNTL, dst);
    wr(SRC_Y_X, ((uint32_t)ox << 16) | (uint32_t)oy);
    wr(SRC_WIDTH1, (uint32_t)l);
    wr(DST_Y_X, ((uint32_t)dx << 16) | (uint32_t)dy);
    wr(DST_HEIGHT_WIDTH, ((uint32_t)l << 16) | (uint32_t)a);
    return true;
}

static void m64_desligar(void)
{
    /* El motor no toca la pantalla por su cuenta: basta con no usarlo más. */
    klog("[ACEL2D] mach64: motor 2D sin usar\n");
}

static void m64_detalle(const char *fase)
{
    if (!regs) return;
    klog("[ACEL2D] mach64 (%s): FIFO_STAT %x GUI_STAT %x DST_OFF_PITCH %x DP_PIX_WIDTH %x DST_CNTL %x GEN_TEST_CNTL %x\n",
         fase, rd(FIFO_STAT), rd(GUI_STAT), rd(DST_OFF_PITCH), rd(DP_PIX_WIDTH), rd(DST_CNTL), rd(GEN_TEST_CNTL));
}

static bool mapear(uint32_t fis_bloco0)
{
    uint32_t pag = fis_bloco0 & ~0xFFFu;
    if (!regs || pagina_fis != pag) {
        volatile uint8_t *v = (volatile uint8_t *)janela_mapear_tipo(pag, 0x1000, PG_PCD | PG_PWT);
        if (!v) return false;
        pagina_fis = pag;
        regs = v + (fis_bloco0 & 0xFFFu);
    }
    desloc_bloco0 = fis_bloco0 & 0xFFFu;
    return true;
}

static bool m64_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *m, int nm)
{
    (void)geracao;
    int fam = -1;
    for (int k = 0; k < N_MODELOS; k++)
        if (modelos[k].id == d->modelo) { fam = modelos[k].fam; nome_modelo = modelos[k].nome; }
    if (fam < 0) { snformatar(m, (size_t)nm, "el modelo 1002:%04x no esta en la lista de Mach64/Rage", d->modelo); return false; }
    bpx = t->bytes_px ? t->bytes_px : 4;
    xmul = bpx == 3 ? 3 : 1;
    uint32_t bar0 = pci_le32(d, 0x10), bar2 = pci_le32(d, 0x18);
    klog("[ACEL2D] mach64: %s (1002:%04x, familia %d), BAR0 %x, BAR2 %x, pantalla %x, pitch %u, %d bytes por pixel, %u KB del VBE\n",
         nome_modelo, d->modelo, fam, bar0, bar2, t->fb_fisico, t->pitch, bpx, t->memoria >> 10);
    if ((bar0 & 1) || !(bar0 & ~0xFu)) { snformatar(m, (size_t)nm, "el BAR 0 no es la apertura lineal (%x)", bar0); return false; }
    bar0 &= ~0xFu;
    if (t->fb_fisico < bar0 || t->fb_fisico - bar0 >= 0x800000u) {
        snformatar(m, (size_t)nm, "la pantalla (%x) no esta en los primeros 8 MB del BAR 0 (%x)", t->fb_fisico, bar0);
        return false;
    }
    uint32_t fb_desloc = t->fb_fisico - bar0;
    /* Dónde está el bloque 0 de registros (solo se lee lo que es de la tarjeta). */
    uint32_t bloco0;
    const char *onde;
    if (fam == M64_PRO && !(bar2 & 1) && ((bar2 >> 1) & 3) == 0 && (bar2 & ~0xFFFu)) {
        bloco0 = (bar2 & ~0xFFFu) + 0x400;
        onde = "BAR 2";
    } else if (fam == M64_GX) {
        /* La GX/CX: la apertura es de 4 u 8 MB según CONFIG_CNTL (bits 1:0), que se lee por el
         * puerto de E/S «disperso» 0x6AEC; así no se lee nada fuera del BAR. */
        uint32_t cc = porta_le32(0x6AEC);
        uint32_t ap = (cc & 3) == 2 ? 0x800000u : (cc & 3) == 1 ? 0x400000u : 0;
        klog("[ACEL2D] mach64: CONFIG_CNTL %x por E/S\n", cc);
        if (!ap || cc == 0xFFFFFFFFu) { snformatar(m, (size_t)nm, "Mach64 GX: CONFIG_CNTL %x no da la apertura", cc); return false; }
        bloco0 = bar0 + ap - 0x400;
        onde = ap == 0x800000u ? "fin de la apertura de 8 MB" : "fin de la apertura de 4 MB";
    } else {
        bloco0 = bar0 + 0x7FFC00u;
        onde = "fin de los primeros 8 MB de la apertura";
    }
    if (!mapear(bloco0)) { snformatar(m, (size_t)nm, "sin ventana para los registros"); return false; }
    uint32_t chip = rd(CONFIG_CHIP_ID), gen = rd(CRTC_GEN_CNTL), crtc = rd(CRTC_OFF_PITCH), test = rd(GEN_TEST_CNTL);
    klog("[ACEL2D] mach64: registros en %x (%s): CONFIG_CHIP_ID %x, CRTC_GEN_CNTL %x, CRTC_OFF_PITCH %x, GEN_TEST_CNTL %x\n",
         bloco0, onde, chip, gen, crtc, test);
    /* Que de verdad sean los registros de esta tarjeta: el identificador del chip. */
    uint32_t esperado = fam == M64_GX ? (d->modelo == 0x4758 ? 0xD7u : 0x57u) : d->modelo;
    if ((chip & 0xFFFFu) != esperado) {
        snformatar(m, (size_t)nm, "CONFIG_CHIP_ID %x no es el del chip (%x): no son sus registros", chip, esperado);
        return false;
    }
    /* La pantalla que se cree tiene que ser la que saca el CRTC: pitch, formato y desplazamiento. */
    uint32_t pitch_px = t->pitch / (uint32_t)bpx;
    uint32_t pix_crtc = bpx == 4 ? 6u : bpx == 3 ? 5u : 4u;
    if (t->pitch % (uint32_t)bpx || ((crtc >> 22) & 0x3FFu) * 8u != pitch_px) {
        snformatar(m, (size_t)nm, "CRTC_OFF_PITCH %x no es el pitch del VBE (%u pixeles)", crtc, pitch_px);
        return false;
    }
    if (CRTC_PIX_WIDTH(gen) != pix_crtc) {
        snformatar(m, (size_t)nm, "CRTC_GEN_CNTL %x: el formato no es el de la pantalla (%d bytes por pixel)", gen, bpx);
        return false;
    }
    if ((crtc & 0xFFFFFu) * 8u != fb_desloc) {
        snformatar(m, (size_t)nm, "CRTC_OFF_PITCH %x: la pantalla no empieza donde dice el VBE (%x)", crtc, fb_desloc);
        return false;
    }
    /* Lo que el motor ve: la pantalla desde fb_desloc, con el pitch en grupos de 8 píxeles del
     * formato del motor (en 24 bits, píxeles de 8: el pitch en bytes / 8). */
    uint32_t pitch_motor = (uint32_t)t->pitch / (uint32_t)(bpx == 3 ? 1 : bpx);
    if (pitch_motor & 7 || pitch_motor / 8 > 0x3FF || fb_desloc & 7) {
        snformatar(m, (size_t)nm, "pitch %u o desplazamiento %x no valen para el motor", t->pitch, fb_desloc);
        return false;
    }
    if ((uint32_t)t->larg * (uint32_t)xmul > 0xFFF) { snformatar(m, (size_t)nm, "la pantalla es demasiado ancha para el motor"); return false; }
    off_pitch = ((pitch_motor / 8) << 22) | (fb_desloc / 8);
    uint32_t pw = bpx == 4 ? PIX_32 : bpx == 2 ? PIX_16 : PIX_8;
    pix_width = pw | (pw << 8) | (pw << 16) | (1u << 24);   /* destino, origen, host; bytes en orden LSB */
    /* El motor tiene que estar encendido. Si la BIOS lo dejó apagado, se enciende (es lo único
     * que se escribe fuera del motor). */
    if (!(test & GUI_ENGINE_ENABLE)) {
        klog("[ACEL2D] mach64: el motor estaba apagado (GEN_TEST_CNTL %x): se enciende\n", test);
        wr(GEN_TEST_CNTL, test | GUI_ENGINE_ENABLE);
    }
    if (!m64_esperar()) { snformatar(m, (size_t)nm, "el motor 2D no esta libre (GUI_STAT %x, FIFO_STAT %x)", rd(GUI_STAT), rd(FIFO_STAT)); return false; }
    /* Lo que el resto de órdenes da por hecho: sin recorte, todos los bits, sin comparar
     * colores, sin patrón ni datos del host. */
    if (!fifo(14)) { snformatar(m, (size_t)nm, "la FIFO del motor no responde"); return false; }
    wr(CONTEXT_MASK, 0xFFFFFFFFu);
    wr(DST_OFF_PITCH, off_pitch);
    wr(SRC_OFF_PITCH, off_pitch);
    wr(DP_PIX_WIDTH, pix_width);
    wr(DP_CHAIN_MASK, bpx == 2 ? 0x8410u : 0x8080u);
    wr(DP_WRITE_MASK, 0xFFFFFFFFu);
    wr(DP_BKGD_CLR, 0);
    wr(CLR_CMP_CNTL, 0);
    wr(SC_LEFT_RIGHT, (0x1FFFu << 16) | 0);
    wr(SC_TOP_BOTTOM, (0x7FFFu << 16) | 0);
    wr(SRC_CNTL, 0);
    wr(PAT_CNTL, 0);
    wr(HOST_CNTL, 0);
    wr(DST_CNTL, DST_X_ESQ_DIR | DST_Y_CIMA_BAIXO);
    if (!m64_esperar()) { snformatar(m, (size_t)nm, "el motor 2D no responde"); return false; }
    klog("[ACEL2D] mach64: DST_OFF_PITCH %x, DP_PIX_WIDTH %x%s\n", off_pitch, pix_width,
         xmul == 3 ? " (24 bits: el motor en 8 bits con x3)" : "");
    return true;
}

const acel_driver_t acel_mach64 = {
    "mach64", ACEL2D_MACH64, m64_iniciar, m64_copiar, m64_preencher, m64_esperar, NULL, m64_desligar,
    true,          /* la zona de prueba va debajo de la pantalla visible */
    0, false,      /* D246: sin mínimo (se mide en la autoprueba); escribir los registros ya es la orden */
    m64_detalle,
    true,          /* D308: también 24 bits (x3) y RGB565 */
    NULL,    /* D387: sin volcado por la GPU */
};
