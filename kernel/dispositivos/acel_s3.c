// SPDX-License-Identifier: GPL-2.0-or-later
/* acel_s3.c - el 2D de las S3 Trio64 y ViRGE (D308).
 *
 * SIN PROBAR EN HARDWARE REAL: solo en 86Box (ViRGE/DX 5333:8a01 y Trio64V2/DX
 * 5333:8901). QEMU no emula ninguna S3. La capa común (acel2d.c) solo lo activa si su
 * autoprueba coincide con la CPU píxel a píxel y termina a tiempo; el vigilante lo
 * apaga si después deja de responder.
 *
 * Son dos motores distintos, uno por familia (la «geração» de la tabla de acel2d.c):
 *
 * 0 = ViRGE (325, VX, DX/GX, GX2, Trio3D, Trio3D/2X). El motor 2D se programa por MMIO,
 *     en la ventana «nueva» de 64 KB que la tarjeta pone 16 MB después del framebuffer
 *     lineal (BAR 0 + 0x1000000), si CR53 bit 3 está encendido (si la BIOS no lo dejó
 *     así, se enciende y se deja como estaba al apagar). Registros de BitBLT/relleno:
 *     SRC_BASE/DEST_BASE (dónde empieza la pantalla), DEST_SRC_STR (los pitch, de 8 en 8
 *     bytes y como mucho 4088), PAT_FG_CLR/PAT_BG_CLR/MONO_PAT (el color del relleno),
 *     RWIDTH_HEIGHT, RSRC_XY, RDEST_XY y CMD_SET, que lanza la orden: la orden (BitBLT 0,
 *     relleno 2), el formato (8, 16 o 24 bits), el ROP (0xCC copiar, 0xF0 patrón) y el
 *     sentido (XP, YP: de izquierda a derecha, de arriba abajo; si no, se dan la última
 *     columna / fila). Estado: SUBSYS_STAT (0x8504), bit 13 = libre y los huecos de la FIFO.
 *     El motor no tiene 32 bits por píxel: en una pantalla de 32 bits se trabaja en 16 con
 *     las coordenadas por 2 (copiar es mover bytes: exacto); para rellenar se usa un BitBLT
 *     de patrón monocromo 0xAA (columnas pares el color de delante, las impares el de
 *     detrás): las palabras bajas de cada píxel con PAT_FG_CLR y las altas con PAT_BG_CLR.
 *     El pitch de 4096 bytes (1024x768x32) no cabe en DEST_SRC_STR: entonces cada fila es
 *     una orden con su SRC_BASE/DEST_BASE (más escrituras, pero sigue sin pasar los
 *     píxeles por el bus).
 *
 * 1 = Trio64 (Trio64, Trio64V+, Trio64UV+, Aurora64V+, Trio64V2/DX/GX). El «Graphics
 *     Engine» clásico de S3 (compatible con el 8514/A) por puertos de E/S de 16 bits, que
 *     existe en todas (la ventana MMIO no la tienen todas las Trio64): CUR_X/CUR_Y (0x86E8,
 *     0x82E8), DESTX/DESTY (0x8EE8, 0x8AE8), MAJ_AXIS_PCNT (0x96E8, ancho - 1),
 *     MULTIFUNC_CNTL (0xBEE8: alto - 1, recorte, PIX_CNTL, MULT_MISC), FRGD_COLOR (0xA6E8),
 *     FRGD_MIX (0xBAE8: de dónde sale el color y la operación) y CMD (0x9AE8), que lanza la
 *     orden (relleno de rectángulo 0x4000, BitBLT 0xC000, con INC_X/INC_Y). En 32 bits los
 *     colores y la máscara se escriben en dos mitades de 16 (la baja primero; MULT_MISC
 *     bit 4 dice cuál toca y la orden lo vuelve a la baja). Estado: GP_STAT (lectura de
 *     0x9AE8), bit 9 = ocupado. La pantalla tiene que empezar en el principio de la memoria
 *     de vídeo y el ancho del motor (CR50) ser el pitch del VBE. Solo 16 y 32 bits.
 *
 * Los registros de la VGA de S3 (CR2D/CR2E: el identificador del chip, CR40, CR50, CR53)
 * se leen con CR38/CR39 abiertos y se vuelven a cerrar como estaban.
 *
 * Referencias: la documentación pública de S3 («Trio64/Trio64V+ Data Book», «ViRGE
 * Integrated 3D Accelerator», «ViRGE/DX & GX») y los números de registro de los drivers
 * s3 y s3virge de X.Org (licencia MIT/X11; docs/terceros.md). El código de 86Box (GPL v2
 * o posterior) se leyó solo para saber qué emula. El código es propio; nada viene de
 * s3fb ni de vt8623fb de Linux. */
#include "acel2d.h"
#include "console.h"
#include "cpu.h"
#include "paginacao.h"
#include "portas.h"
#include "texto.h"

#define FAM_VIRGE              0
#define FAM_TRIO               1

static const struct { uint16_t id; uint8_t fam; uint8_t fifo_novo; const char *nome; } modelos[] = {
    { 0x5631, FAM_VIRGE, 0, "ViRGE (86C325)" },
    { 0x883D, FAM_VIRGE, 0, "ViRGE/VX (86C988)" },
    { 0x8A01, FAM_VIRGE, 0, "ViRGE/DX o GX (86C375/385)" },
    { 0x8A10, FAM_VIRGE, 1, "ViRGE/GX2 (86C357)" },
    { 0x8904, FAM_VIRGE, 1, "Trio3D (86C365)" },
    { 0x8A13, FAM_VIRGE, 1, "Trio3D/2X (86C362)" },
    { 0x8811, FAM_TRIO,  0, "Trio64 o Trio64V+ (86C764/765)" },
    { 0x8812, FAM_TRIO,  0, "Aurora64V+ (86C M65)" },
    { 0x8814, FAM_TRIO,  0, "Trio64UV+ (86C767)" },
    { 0x8901, FAM_TRIO,  0, "Trio64V2/DX o GX (86C775/785)" },
};
#define N_MODELOS (int)(sizeof modelos / sizeof modelos[0])

/* ---- la VGA de S3 ---- */
static uint16_t crtc;                 /* 0x3D4 (color) o 0x3B4 */
static uint8_t  cr_ler(uint8_t i)             { porta_escreve8(crtc, i); return porta_le8((uint16_t)(crtc + 1)); }
static void     cr_escrever(uint8_t i, uint8_t v) { porta_escreve8(crtc, i); porta_escreve8((uint16_t)(crtc + 1), v); }

typedef struct { uint8_t indice, cr38, cr39; bool sim; } trava_t;
static void abrir(trava_t *t)
{
    t->sim = int_salva_desliga();
    crtc = (porta_le8(0x3CC) & 1) ? 0x3D4 : 0x3B4;
    t->indice = porta_le8(crtc);
    t->cr38 = cr_ler(0x38);
    t->cr39 = cr_ler(0x39);
    cr_escrever(0x38, 0x48);
    cr_escrever(0x39, 0xA5);
}
static void fechar(const trava_t *t)
{
    cr_escrever(0x39, t->cr39);
    cr_escrever(0x38, t->cr38);
    porta_escreve8(crtc, t->indice);
    int_restaura(t->sim);
}

static int fam = -1;
static const char *nome_modelo = "?";
static bool fifo_novo;                /* GX2/Trio3D: los huecos de la FIFO en otros bits */
static int bpx;                       /* bytes por píxel de la pantalla */
static uint32_t pitch;
static int restaurar_cr = -1;         /* el registro de la VGA que se cambió (CR53 o CR40), o -1 */
static uint8_t cr_original;

/* ================================================================== ViRGE */

#define V_SUBSYS_STAT          0x8504
#define   V_LIVRE              (1u << 13)
#define V_SRC_BASE             0xA4D4
#define V_DEST_BASE            0xA4D8
#define V_CLIP_L_R             0xA4DC
#define V_CLIP_T_B             0xA4E0
#define V_DEST_SRC_STR         0xA4E4
#define V_MONO_PAT_0           0xA4E8
#define V_MONO_PAT_1           0xA4EC
#define V_PAT_BG_CLR           0xA4F0
#define V_PAT_FG_CLR           0xA4F4
#define V_CMD_SET              0xA500
#define V_RWIDTH_HEIGHT        0xA504
#define V_RSRC_XY              0xA508
#define V_RDEST_XY             0xA50C
#define   C_FMT_8              (0u << 2)
#define   C_FMT_16             (1u << 2)
#define   C_FMT_24             (2u << 2)
#define   C_MP                 (1u << 8)            /* patrón monocromo */
#define   C_ROP(r)             ((uint32_t)(r) << 17)
#define   C_XP                 (1u << 25)           /* de izquierda a derecha */
#define   C_YP                 (1u << 26)           /* de arriba abajo */
#define   C_BITBLT             (0u << 27)
#define   C_RELLENO            (2u << 27)

static volatile uint8_t *vmmio;
static uint32_t vmmio_fis;
static uint32_t v_base;               /* la pantalla dentro de la memoria de vídeo */
static uint32_t v_fmt;                /* C_FMT_* del motor */
static int v_xm;                      /* 2 en 32 bits (el motor trabaja en 16), 1 en los demás */
static bool v_por_linha;              /* el pitch no cabe en DEST_SRC_STR: una orden por fila */

static inline uint32_t vrd(uint32_t r)             { return *(volatile uint32_t *)(vmmio + r); }
static inline void     vwr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(vmmio + r) = v; }

static uint32_t v_huecos(uint32_t st)
{
    return fifo_novo ? (st >> 23) & 0x3Fu : (st >> 8) & 0x1Fu;
}

static bool v_fifo(uint32_t n)
{
    acel_prazo_t p;
    acel_prazo(&p, 250);
    while (v_huecos(vrd(V_SUBSYS_STAT)) < n)
        if (acel_prazo_esgotado(&p)) return false;
    return true;
}

static bool v_esperar(void)
{
    acel_prazo_t p;
    int libres = 0;
    acel_prazo(&p, 250);
    /* Libre tres lecturas seguidas (en 86Box el bit cuenta también lo que queda en la FIFO). */
    while (libres < 3) {
        if (vrd(V_SUBSYS_STAT) & V_LIVRE) libres++; else libres = 0;
        if (acel_prazo_esgotado(&p)) { klog("[ACEL2D] s3: el ViRGE no para (SUBSYS_STAT %x)\n", vrd(V_SUBSYS_STAT)); return false; }
    }
    return true;
}

/* Una orden de a filas desde (sx, sy) / (dx, dy), ya en unidades del motor. */
static bool v_ordem(uint32_t cmd, int sx, int sy, int dx, int dy, int l, int a)
{
    if (!v_por_linha) {
        if (!v_fifo(4)) return false;
        vwr(V_RWIDTH_HEIGHT, ((uint32_t)(l - 1) << 16) | (uint32_t)a);
        vwr(V_RSRC_XY, ((uint32_t)sx << 16) | (uint32_t)sy);
        vwr(V_RDEST_XY, ((uint32_t)dx << 16) | (uint32_t)dy);
        vwr(V_CMD_SET, cmd);
        return true;
    }
    /* Una orden por fila, con la fila en SRC_BASE/DEST_BASE: el orden de las filas, el
     * que no pisa el origen (si el destino está más abajo, de abajo arriba). */
    bool subir = !(cmd & C_YP);
    for (int k = 0; k < a; k++) {
        int j = subir ? -k : k;
        if (!v_fifo(7)) return false;
        vwr(V_SRC_BASE, v_base + (uint32_t)(sy + j) * pitch);
        vwr(V_DEST_BASE, v_base + (uint32_t)(dy + j) * pitch);
        vwr(V_RWIDTH_HEIGHT, ((uint32_t)(l - 1) << 16) | 1u);
        vwr(V_RSRC_XY, (uint32_t)sx << 16);
        vwr(V_RDEST_XY, (uint32_t)dx << 16);
        vwr(V_CMD_SET, cmd | C_YP);
    }
    return true;
}

static bool v_copiar(int ox, int oy, int dx, int dy, int l, int a)
{
    ox *= v_xm; dx *= v_xm; l *= v_xm;
    uint32_t cmd = C_BITBLT | v_fmt | C_ROP(0xCC);
    if (ox >= dx) cmd |= C_XP; else { ox += l - 1; dx += l - 1; }
    if (oy >= dy) cmd |= C_YP; else { oy += a - 1; dy += a - 1; }
    return v_ordem(cmd, ox, oy, dx, dy, l, a);
}

static bool v_preencher(int x, int y, int l, int a, uint32_t cor)
{
    if (!v_fifo(4)) return false;
    if (v_xm == 2) {
        /* 32 bits con el motor en 16: patrón monocromo de columnas alternas, la palabra
         * baja de cada píxel (columnas pares) con el color de delante y la alta con el de
         * detrás. El patrón va por la x de la pantalla, que aquí siempre empieza en par. */
        vwr(V_MONO_PAT_0, 0xAAAAAAAAu);
        vwr(V_MONO_PAT_1, 0xAAAAAAAAu);
        vwr(V_PAT_FG_CLR, cor & 0xFFFFu);
        vwr(V_PAT_BG_CLR, (cor >> 16) & 0xFFFFu);
        x *= 2; l *= 2;
        return v_ordem(C_BITBLT | v_fmt | C_MP | C_ROP(0xF0) | C_XP | C_YP, x, y, x, y, l, a);
    }
    vwr(V_PAT_FG_CLR, cor);
    return v_ordem(C_RELLENO | v_fmt | C_ROP(0xF0) | C_XP | C_YP, x, y, x, y, l, a);
}

static bool v_iniciar(const pci_disp_t *d, const acel_tela_t *t, char *m, int nm)
{
    uint32_t bar0 = pci_le32(d, 0x10);
    if ((bar0 & 1) || ((bar0 >> 1) & 3) != 0 || !(bar0 & ~0xFu)) { snformatar(m, (size_t)nm, "el BAR 0 no es memoria de 32 bits (%x)", bar0); return false; }
    bar0 &= ~0xFu;
    if (t->fb_fisico < bar0 || t->fb_fisico - bar0 >= 0x800000u) {
        snformatar(m, (size_t)nm, "la pantalla (%x) no esta en el framebuffer lineal del BAR 0 (%x)", t->fb_fisico, bar0);
        return false;
    }
    v_base = t->fb_fisico - bar0;
    if (bpx == 4) { v_fmt = C_FMT_16; v_xm = 2; }
    else if (bpx == 3) { v_fmt = C_FMT_24; v_xm = 1; }
    else { v_fmt = C_FMT_16; v_xm = 1; }
    if (pitch & 7 || v_base & 7) { snformatar(m, (size_t)nm, "pitch %u o inicio %x no son multiplos de 8", pitch, v_base); return false; }
    if (t->larg * v_xm > 2048 || t->alt + ACEL2D_ZONA_A > 2048) {
        snformatar(m, (size_t)nm, "%dx%d no cabe en las coordenadas de 11 bits del ViRGE", t->larg, t->alt);
        return false;
    }
    v_por_linha = pitch > 4088;
    /* La ventana MMIO «nueva» (CR53 bit 3) y el pitch que usa el CRTC (CR13 + CR51 bits 5:4). */
    trava_t tv;
    abrir(&tv);
    uint8_t id_alto = cr_ler(0x2D), id_baixo = cr_ler(0x2E), cr53 = cr_ler(0x53), cr58 = cr_ler(0x58);
    uint32_t desloc = (uint32_t)cr_ler(0x13) | ((uint32_t)((cr_ler(0x51) >> 4) & 3) << 8);
    bool mudou = false;
    if (((uint16_t)(id_alto << 8) | id_baixo) == d->modelo && !(cr53 & 0x08)) {
        cr_escrever(0x53, (uint8_t)(cr53 | 0x08));
        mudou = true;
    }
    fechar(&tv);
    klog("[ACEL2D] s3: ViRGE: CR2D/2E %x%x, CR53 %x%s, CR58 %x, pitch del CRTC %u, BAR0 %x, pantalla +%x, pitch %u, %s\n",
         id_alto, id_baixo, cr53, mudou ? " (se enciende la ventana MMIO)" : "", cr58, desloc * 8, bar0, v_base, pitch,
         v_por_linha ? "una orden por fila (pitch > 4088)" : "ordenes de varias filas");
    if (((uint16_t)(id_alto << 8) | id_baixo) != d->modelo) {
        snformatar(m, (size_t)nm, "CR2D/CR2E %x%x no son el chip del PCI (%x): la VGA es de otra tarjeta", id_alto, id_baixo, d->modelo);
        return false;
    }
    if (mudou) { restaurar_cr = 0x53; cr_original = cr53; }
    if (desloc * 8 != pitch) { snformatar(m, (size_t)nm, "el CRTC usa un pitch de %u bytes, no el del VBE (%u)", desloc * 8, pitch); return false; }
    uint32_t fis = bar0 + 0x1000000u;
    if (!vmmio || vmmio_fis != fis) {
        vmmio_fis = fis;
        vmmio = (volatile uint8_t *)janela_mapear_tipo(fis, 0x10000, PG_PCD | PG_PWT);
    }
    if (!vmmio) { snformatar(m, (size_t)nm, "sin ventana para el MMIO"); return false; }
    uint32_t st = vrd(V_SUBSYS_STAT);
    klog("[ACEL2D] s3: ViRGE: SUBSYS_STAT %x (%u huecos en la FIFO)\n", st, v_huecos(st));
    if (st == 0xFFFFFFFFu || !v_huecos(st)) { snformatar(m, (size_t)nm, "el MMIO no responde (SUBSYS_STAT %x)", st); return false; }
    if (!v_esperar()) { snformatar(m, (size_t)nm, "el motor 2D no esta libre (SUBSYS_STAT %x)", vrd(V_SUBSYS_STAT)); return false; }
    if (!v_fifo(5)) { snformatar(m, (size_t)nm, "la FIFO del motor no responde"); return false; }
    uint32_t str = v_por_linha ? 8u : pitch;   /* por filas, el stride no se usa (alto 1) */
    vwr(V_SRC_BASE, v_base);
    vwr(V_DEST_BASE, v_base);
    vwr(V_DEST_SRC_STR, (str << 16) | str);
    vwr(V_CLIP_L_R, 0x7FFu);
    vwr(V_CLIP_T_B, 0x7FFu);
    return v_esperar();
}

/* ================================================================== Trio64 */

#define T_CUR_Y                0x82E8
#define T_CUR_X                0x86E8
#define T_DESTY                0x8AE8
#define T_DESTX                0x8EE8
#define T_MAJ_AXIS_PCNT        0x96E8
#define T_CMD                  0x9AE8      /* al leerlo, GP_STAT */
#define   T_OCUPADO            (1u << 9)
#define T_FRGD_COLOR           0xA6E8
#define T_WRT_MASK             0xAAE8
#define T_RD_MASK              0xAEE8
#define T_BKGD_MIX             0xB6E8
#define T_FRGD_MIX             0xBAE8
#define T_MULTIFUNC            0xBEE8
#define   MF_MIN_AXIS          0x0000u     /* alto - 1 */
#define   MF_SCISSORS_T        0x1000u
#define   MF_SCISSORS_L        0x2000u
#define   MF_SCISSORS_B        0x3000u
#define   MF_SCISSORS_R        0x4000u
#define   MF_PIX_CNTL          0xA000u
#define   MF_MULT_MISC         0xE000u
#define   CMD_WRTDATA          (1u << 0)
#define   CMD_DRAW             (1u << 4)
#define   CMD_INC_X            (1u << 5)
#define   CMD_INC_Y            (1u << 7)
#define   CMD_RETANGULO        0x4000u
#define   CMD_BITBLT           0xC000u
#define MIX_FRGD_COR           0x20u       /* el color sale de FRGD_COLOR */
#define MIX_MEMORIA            0x60u       /* el color sale de la memoria de vídeo */
#define MIX_NOVO               0x07u       /* el nuevo píxel es el origen */

static inline uint16_t trd(uint16_t p)             { return porta_le16(p); }
static inline void     twr(uint16_t p, uint16_t v) { porta_escreve16(p, v); }

/* Un valor de 32 bits (color, máscara) en dos mitades de 16 en 32 bits por píxel. */
static inline void twr_cor(uint16_t p, uint32_t v)
{
    twr(p, (uint16_t)v);
    if (bpx == 4) twr(p, (uint16_t)(v >> 16));
}

/* La FIFO de 8 huecos: GP_STAT bit (8 - n) encendido = quedan menos de n. */
static bool t_fifo(uint32_t n)
{
    acel_prazo_t p;
    acel_prazo(&p, 250);
    while (trd(T_CMD) & (0x0100u >> n))
        if (acel_prazo_esgotado(&p)) return false;
    return true;
}

static bool t_esperar(void)
{
    acel_prazo_t p;
    int libres = 0;
    acel_prazo(&p, 250);
    while (libres < 3) {
        if (trd(T_CMD) & T_OCUPADO) libres = 0; else libres++;
        if (acel_prazo_esgotado(&p)) { klog("[ACEL2D] s3: la Trio64 no para (GP_STAT %x)\n", trd(T_CMD)); return false; }
    }
    return true;
}

static bool t_preencher(int x, int y, int l, int a, uint32_t cor)
{
    if (!t_fifo(8)) return false;
    twr(T_FRGD_MIX, MIX_FRGD_COR | MIX_NOVO);
    twr_cor(T_FRGD_COLOR, cor);
    twr(T_CUR_X, (uint16_t)x);
    twr(T_CUR_Y, (uint16_t)y);
    twr(T_MAJ_AXIS_PCNT, (uint16_t)(l - 1));
    twr(T_MULTIFUNC, (uint16_t)(MF_MIN_AXIS | (uint32_t)(a - 1)));
    twr(T_CMD, CMD_RETANGULO | CMD_INC_Y | CMD_INC_X | CMD_DRAW | CMD_WRTDATA);
    return true;
}

static bool t_copiar(int ox, int oy, int dx, int dy, int l, int a)
{
    if (!t_fifo(8)) return false;
    uint16_t dir = 0;
    if (ox >= dx) dir |= CMD_INC_X; else { ox += l - 1; dx += l - 1; }
    if (oy >= dy) dir |= CMD_INC_Y; else { oy += a - 1; dy += a - 1; }
    twr(T_FRGD_MIX, MIX_MEMORIA | MIX_NOVO);
    twr(T_CUR_X, (uint16_t)ox);
    twr(T_CUR_Y, (uint16_t)oy);
    twr(T_DESTX, (uint16_t)dx);
    twr(T_DESTY, (uint16_t)dy);
    twr(T_MAJ_AXIS_PCNT, (uint16_t)(l - 1));
    twr(T_MULTIFUNC, (uint16_t)(MF_MIN_AXIS | (uint32_t)(a - 1)));
    twr(T_CMD, (uint16_t)(CMD_BITBLT | dir | CMD_DRAW | CMD_WRTDATA));
    return true;
}

/* El ancho que el motor cree que tiene la pantalla (CR50 bits 7:6 y 0; CR31 bit 1 para 2048). */
static int t_largura(uint8_t cr50, uint8_t cr31)
{
    switch (cr50 & 0xC1) {
    case 0x00: return (cr31 & 0x02) ? 2048 : 1024;
    case 0x01: return 1152;
    case 0x40: return 640;
    case 0x80: return 800;
    case 0x81: return 1600;
    case 0xC0: return 1280;
    default:   return -1;
    }
}

static bool t_iniciar(const pci_disp_t *d, const acel_tela_t *t, char *m, int nm)
{
    if (bpx == 3) { snformatar(m, (size_t)nm, "24 bits: el motor de la Trio64 no lo acelera"); return false; }
    uint32_t bar0 = pci_le32(d, 0x10);
    if ((bar0 & 1) || !(bar0 & ~0xFu)) { snformatar(m, (size_t)nm, "el BAR 0 no es memoria (%x)", bar0); return false; }
    bar0 &= ~0xFu;
    if (t->fb_fisico != bar0) {
        snformatar(m, (size_t)nm, "la pantalla (%x) no empieza en la memoria de video (BAR 0 %x)", t->fb_fisico, bar0);
        return false;
    }
    if (t->alt + ACEL2D_ZONA_A > 4096 || t->larg > 4096) { snformatar(m, (size_t)nm, "la pantalla no cabe en las coordenadas de 12 bits"); return false; }
    trava_t tv;
    abrir(&tv);
    uint8_t id_alto = cr_ler(0x2D), id_baixo = cr_ler(0x2E), cr40 = cr_ler(0x40), cr50 = cr_ler(0x50), cr31 = cr_ler(0x31);
    uint8_t cr0c = cr_ler(0x0C), cr0d = cr_ler(0x0D), cr69 = cr_ler(0x69);
    bool mudou = false;
    bool e_ele = ((uint16_t)(id_alto << 8) | id_baixo) == d->modelo;
    if (e_ele && !(cr40 & 0x01)) {
        cr_escrever(0x40, (uint8_t)(cr40 | 0x01));   /* abre los registros del motor (8514/A) */
        mudou = true;
    }
    fechar(&tv);
    int larg_motor = t_largura(cr50, cr31);
    int pix_motor = (cr50 >> 4) & 3;     /* 0: 8 bits, 1: 16, 3: 32 */
    klog("[ACEL2D] s3: Trio64: CR2D/2E %x%x, CR40 %x%s, CR50 %x (motor de %d pixeles, %d bytes por pixel), CR31 %x, inicio %x%x (CR69 %x)\n",
         id_alto, id_baixo, cr40, mudou ? " (se abren los registros del motor)" : "", cr50, larg_motor, pix_motor + 1,
         cr31, cr0c, cr0d, cr69);
    if (!e_ele) {
        snformatar(m, (size_t)nm, "CR2D/CR2E %x%x no son el chip del PCI (%x): la VGA es de otra tarjeta", id_alto, id_baixo, d->modelo);
        return false;
    }
    if (mudou) { restaurar_cr = 0x40; cr_original = cr40; }
    if (pix_motor + 1 != bpx || pix_motor == 2) {
        snformatar(m, (size_t)nm, "CR50 %x: el motor no esta en el formato de la pantalla (%d bytes por pixel)", cr50, bpx);
        return false;
    }
    if (larg_motor < 0 || (uint32_t)larg_motor * (uint32_t)bpx != pitch) {
        snformatar(m, (size_t)nm, "CR50 %x: el ancho del motor (%d) no es el pitch del VBE (%u bytes)", cr50, larg_motor, pitch);
        return false;
    }
    if (cr0c || cr0d) { snformatar(m, (size_t)nm, "la pantalla no empieza en la direccion 0 (CR0C/CR0D %x%x)", cr0c, cr0d); return false; }
    uint16_t st = trd(T_CMD);
    klog("[ACEL2D] s3: Trio64: GP_STAT %x\n", st);
    if (st == 0xFFFFu) { snformatar(m, (size_t)nm, "los puertos del motor no responden (GP_STAT %x)", st); return false; }
    if (!t_esperar()) { snformatar(m, (size_t)nm, "el motor 2D no esta libre (GP_STAT %x)", trd(T_CMD)); return false; }
    /* Lo que el resto de órdenes da por hecho: colores en dos mitades empezando por la baja,
     * siempre el color de delante, sin recorte y todos los bits. */
    if (!t_fifo(8)) { snformatar(m, (size_t)nm, "la FIFO del motor no responde"); return false; }
    twr(T_MULTIFUNC, MF_MULT_MISC);
    twr(T_MULTIFUNC, MF_PIX_CNTL);
    twr(T_MULTIFUNC, MF_SCISSORS_T);
    twr(T_MULTIFUNC, MF_SCISSORS_L);
    twr(T_MULTIFUNC, (uint16_t)(MF_SCISSORS_B | 0xFFFu));
    twr(T_MULTIFUNC, (uint16_t)(MF_SCISSORS_R | 0xFFFu));
    twr(T_BKGD_MIX, MIX_NOVO);
    if (!t_fifo(4)) { snformatar(m, (size_t)nm, "la FIFO del motor no responde"); return false; }
    twr_cor(T_WRT_MASK, 0xFFFFFFFFu);
    twr_cor(T_RD_MASK, 0xFFFFFFFFu);
    return t_esperar();
}

/* ================================================================== común */

static bool s3_esperar(void) { return fam == FAM_VIRGE ? v_esperar() : t_esperar(); }
static bool s3_copiar(int ox, int oy, int dx, int dy, int l, int a)
{
    return fam == FAM_VIRGE ? v_copiar(ox, oy, dx, dy, l, a) : t_copiar(ox, oy, dx, dy, l, a);
}
static bool s3_preencher(int x, int y, int l, int a, uint32_t cor)
{
    return fam == FAM_VIRGE ? v_preencher(x, y, l, a, cor) : t_preencher(x, y, l, a, cor);
}

static void s3_desligar(void)
{
    /* El motor no toca la pantalla por su cuenta; solo se deja la VGA como estaba. */
    if (restaurar_cr >= 0) {
        trava_t tv;
        abrir(&tv);
        cr_escrever((uint8_t)restaurar_cr, cr_original);
        fechar(&tv);
        klog("[ACEL2D] s3: CR%x vuelve a %x\n", restaurar_cr, cr_original);
        restaurar_cr = -1;   /* la ventana del kernel se queda: «acel2d encender» vuelve a abrir el MMIO */
    }
    klog("[ACEL2D] s3: motor 2D sin usar\n");
}

static void s3_detalle(const char *fase)
{
    if (fam == FAM_VIRGE && vmmio)
        klog("[ACEL2D] s3 (%s): ViRGE SUBSYS_STAT %x CMD_SET %x DEST_SRC_STR %x DEST_BASE %x\n", fase,
             vrd(V_SUBSYS_STAT), vrd(V_CMD_SET), vrd(V_DEST_SRC_STR), vrd(V_DEST_BASE));
    else if (fam == FAM_TRIO)
        klog("[ACEL2D] s3 (%s): Trio64 GP_STAT %x\n", fase, trd(T_CMD));
}

static bool s3_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *m, int nm)
{
    fam = -1;
    for (int k = 0; k < N_MODELOS; k++)
        if (modelos[k].id == d->modelo) { fam = modelos[k].fam; fifo_novo = modelos[k].fifo_novo; nome_modelo = modelos[k].nome; }
    if (fam < 0 || fam != geracao) { snformatar(m, (size_t)nm, "el modelo 5333:%04x no esta en la lista de Trio64/ViRGE", d->modelo); return false; }
    bpx = t->bytes_px ? t->bytes_px : 4;
    pitch = t->pitch;
    restaurar_cr = -1;
    klog("[ACEL2D] s3: %s (5333:%04x), pantalla %x, %dx%d, pitch %u, %d bytes por pixel, %u KB del VBE\n", nome_modelo,
         d->modelo, t->fb_fisico, t->larg, t->alt, t->pitch, bpx, t->memoria >> 10);
    bool ok = fam == FAM_VIRGE ? v_iniciar(d, t, m, nm) : t_iniciar(d, t, m, nm);
    if (!ok && restaurar_cr >= 0) s3_desligar();
    return ok;
}

const acel_driver_t acel_s3 = {
    "s3", ACEL2D_S3, s3_iniciar, s3_copiar, s3_preencher, s3_esperar, NULL, s3_desligar,
    true,          /* la zona de prueba va debajo de la pantalla visible */
    0, false,      /* D246: sin mínimo; escribir los registros ya es la orden */
    s3_detalle,
    true,          /* D308: también 24 bits (ViRGE) y RGB565 */
};
