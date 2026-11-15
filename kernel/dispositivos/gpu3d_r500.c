// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d_r500.c - 3D por la GPU en las Radeon R5xx (X1300 a X1950: RV515, R520,
 * RV530, RV560, RV570, R580), debajo de la capa común gpu3d (D285, D287).
 *
 * SIN PROBAR EN HARDWARE: QEMU no emula ninguna Radeon. Por eso este driver NO se
 * enciende solo: la capa lo deja «apagado» hasta «gpu3d encender» o «gpu3d probar»
 * (o la opción de arranque --gpu3d=radeon), y solo queda activo si la autoprueba de
 * la capa lee los píxeles esperados y a tiempo. El objetivo es la Mobility Radeon
 * X1400 (RV515/M54, 1002:7145) del Amilo del mantenedor. Las órdenes las construye
 * r500_pacotes.h, que sí se prueba en el PC (tests/r500_3d_unidad.c).
 *
 * Cómo funciona:
 *   - en R5xx los triángulos solo se pueden mandar por el procesador de órdenes
 *     (CP), que necesita su microcódigo (R520_cp.bin de linux-firmware, en
 *     /lib/firmware/radeon; no entra con SEM_FIRMWARE=1 y entonces no hay 3D);
 *   - la cola de órdenes del CP (64 KB) y todo lo demás (destino de color, Z,
 *     texturas) van en VRAM de la tarjeta, DETRÁS de la pantalla y de la zona de
 *     prueba del 2D, en una zona de hasta 16 MB que se comprueba contra el tamaño
 *     real de la VRAM, la apertura de la CPU, lo que el controlador de memoria (MC)
 *     llama memoria de vídeo, los cursores por hardware y el otro controlador de
 *     pantalla. La CPU la ve por el BAR 0;
 *   - los vértices van dentro de las órdenes (3D_DRAW_IMMD_2): no hay búferes de
 *     vértices. Cada dibujo lleva su destino, su Z/mezcla, su textura y su programa
 *     de fragmentos; lo fijo (r5_inicio) se manda una vez al encender;
 *   - el CP solo trabaja cuando se le avisa (CP_RB_WPTR), y eso se hace al esperar
 *     (leer el cuadro, cambiar una textura o el destino) o si la cola se llena:
 *     al volver de esas esperas el motor está parado, y el 2D por MMIO de
 *     acel_radeon.c (que comparte el motor) nunca se cruza con un dibujo a medias;
 *   - vallas: cada aviso acaba vaciando las cachés del 3D y escribiendo su número
 *     en un registro de apuntes (SCRATCH_REG2), que el CP solo escribe con el motor
 *     parado (ISYNC_CNTL). Se espera a ese número, con plazo.
 *
 * Qué toca y qué no (seguridad en hardware real):
 *   - sin bus maestro, sin GART, sin interrupciones: la tarjeta no lee ni escribe
 *     memoria del sistema (la cola está en VRAM; la escritura de vuelta del puntero
 *     y de los apuntes está apagada y además apunta a nuestra zona);
 *   - registros: los del CP (0x700-0x7FF), ISYNC_CNTL (se restaura al apagar), los
 *     apuntes 1 y 2, el registro 0x0D del PLL (reparto del reloj entre las tuberías
 *     de píxeles, como hacen Linux y X.org; solo si no está ya así) y, por la cola,
 *     el estado 3D (0x2000-0x4FFF). NO toca los controladores de pantalla, los
 *     relojes de pantalla, la energía, el MC (solo lo lee) ni RBBM_SOFT_RESET;
 *   - toda espera tiene plazo. Si algo no responde se para el CP (CP_CSQ_CNTL = 0)
 *     y se deja: nunca se reinicia el motor con la pantalla del VBE encendida. Si el
 *     motor quedara ocupado, el 2D lo nota con su propio vigilante (D244) y sigue
 *     la CPU.
 *
 * Diagnóstico: «[GPU3D] r500: etapa N ...» en cada paso del encendido, y
 * r5_detalle() («gpu3d probar --detalle», o sola tras un fallo) vuelca RBBM_STATUS,
 * el estado del CP, los punteros de la cola, los apuntes y la zona.
 *
 * Qué hay que comprobar en el hardware: docs/gpu3d.md. */
#include "gpu3d.h"
#include "console.h"
#include "memoria.h"
#include "paginacao.h"
#include "prazo.h"
#include "texto.h"
#include "vfs.h"
#include "r500_pacotes.h"

#define MMIO_TAM             0x10000
#define ANEL_BYTES           (64 * 1024)
#define ANEL_PAL             (ANEL_BYTES / 4)
#define RESERVA_BYTES        4096              /* página a la que apuntan las escrituras de vuelta (apagadas) */
#define PREP_PAL             6144              /* cola en RAM: un dibujo de 510 vértices y su estado */
#define ZONA_MIN             (2u << 20)
#define ZONA_MAX             (16u << 20)
#define UCODE_BYTES          2048
#define PRAZO_CURTO_MS       250
#ifdef SAGUI_SEM_FIRMWARE
#define SEM_FIRMWARE         1
#else
#define SEM_FIRMWARE         0
#endif

/* Registros que se usan por MMIO (los del estado 3D están en r500_pacotes.h). */
#define CLOCK_CNTL_INDEX     0x0008
#define CLOCK_CNTL_DATA      0x000C
#define   PLL_ESCREVER       0x80u
#define PLL_SCLK_PIPE        0x0D              /* R500_DYN_SCLK_PWMEM_PIPE */
#define MC_IND_INDEX         0x0070
#define MC_IND_DATA          0x0074
#define MC_FB_LOC_RV515      0x01
#define MC_FB_LOC_R520       0x04
#define CONFIG_MEMSIZE       0x00F8
#define CONFIG_APER_SIZE     0x0108
#define CP_RB_BASE           0x0700
#define CP_RB_CNTL           0x0704
#define   RB_SEM_ATUALIZAR   (1u << 27)        /* el CP no escribe su puntero de lectura en memoria */
#define   RB_RPTR_ESCREVER   (1u << 31)
#define CP_RB_RPTR_ADDR      0x070C
#define CP_RB_RPTR           0x0710
#define CP_RB_WPTR           0x0714
#define CP_RB_WPTR_DELAY     0x0718
#define CP_RB_RPTR_WR        0x071C
#define CP_CSQ_CNTL          0x0740
#define   CSQ_ANEL           (4u << 28)        /* cola principal por el anillo (PRIBM_INDBM) */
#define CP_CSQ_MODE          0x0744
#define SCRATCH_UMSK         0x0770
#define SCRATCH_ADDR         0x0774
#define CP_ME_RAM_ADDR       0x07D4
#define CP_ME_RAM_DATAH      0x07DC
#define CP_ME_RAM_DATAL      0x07E0
#define CP_CSQ_STAT          0x07F8
#define RBBM_STATUS          0x0E40
#define   RBBM_ATIVO         0x80000000u
#define APUNTE_TESTE         (R5_SCRATCH_REG0 + 4)
#define APUNTE_VALLA         (R5_SCRATCH_REG0 + 8)
#define D1GRPH_ENABLE        0x6100
#define D1GRPH_SUPERFICIE    0x6110
#define D1GRPH_PITCH         0x6120
#define D1CUR_CONTROL        0x6400
#define D1CUR_SUPERFICIE     0x6408
#define D2_DESLOC            0x0800

enum { F_RV515, F_R520, F_RV530, F_RV560, F_RV570, F_R580 };
static const char *const nome_familia[] = { "RV515", "R520", "RV530", "RV560", "RV570", "R580" };
static const uint8_t fpus_familia[] = { 2, 8, 5, 8, 8, 8 };
/* Solo estas tarjetas (las R5xx discretas de la lista del driver radeon de Linux). Las
 * integradas RS600/RS690 usan otro microcódigo y otra memoria: no están. */
static const struct { uint16_t de, ate; uint8_t familia; } modelos[] = {
    { 0x7100, 0x710F, F_R520 },  { 0x7140, 0x715F, F_RV515 }, { 0x7180, 0x719F, F_RV515 }, { 0x71C0, 0x71DE, F_RV530 },
    { 0x7200, 0x7211, F_RV515 }, { 0x7240, 0x724F, F_R580 },  { 0x7280, 0x7280, F_RV570 }, { 0x7284, 0x7284, F_R580 },
    { 0x7288, 0x728C, F_RV570 }, { 0x7290, 0x7297, F_RV560 },
};

static volatile uint8_t *mmio;
static uint32_t mmio_fis;
static int familia;
static uint16_t modelo;
static uint32_t mc_fb_ini, mc_fb_fim, mc_tela, mc_zona;   /* direcciones en el MC */
static uint32_t zona_off, zona_bytes;                     /* desde el principio de la VRAM (BAR 0) */
static uint8_t *zona_cpu;
static uint32_t zona_fis_map, zona_bytes_map;
static volatile uint32_t *anel;
static uint32_t wptr, enviado;             /* wptr: hasta dónde se ha escrito; enviado: lo que sabe el CP */
static uint32_t seq, seq_vista;            /* última valla emitida y última confirmada */
static uint32_t isync_antes, tuberias;
static bool pronto, cp_ligado, isync_guardado;
static g3d_arena_t arena;
static g3d_buffer_t b_cor, b_z, b_tex[GPU3D_TEXTURAS];
static r5_alvo_t alvo;
static uint32_t *prep;

static inline uint32_t rd(uint32_t r)             { return *(volatile uint32_t *)(mmio + r); }
static inline void     wr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(mmio + r) = v; }

static void esperar_ms(uint32_t ms)
{
    prazo_t p = prazo_ms(ms);
    while (!prazo_vencido(&p)) { }
}

/* Registros indirectos: el MC (solo lectura) y el PLL. */
static uint32_t mc_ler(uint32_t reg)
{
    wr(MC_IND_INDEX, 0x7F0000u | (reg & 0xFFFFu));
    uint32_t v = rd(MC_IND_DATA);
    wr(MC_IND_INDEX, 0);
    return v;
}

static uint32_t pll_ler(uint32_t reg)
{
    *(volatile uint8_t *)(mmio + CLOCK_CNTL_INDEX) = (uint8_t)(reg & 0x3F);
    return rd(CLOCK_CNTL_DATA);
}

static void pll_escrever(uint32_t reg, uint32_t v)
{
    *(volatile uint8_t *)(mmio + CLOCK_CNTL_INDEX) = (uint8_t)((reg & 0x3F) | PLL_ESCREVER);
    wr(CLOCK_CNTL_DATA, v);
    *(volatile uint8_t *)(mmio + CLOCK_CNTL_INDEX) = (uint8_t)(reg & 0x3F);
}

static void r5_detalle(const char *fase)
{
    if (!mmio) { klog("[GPU3D] r500 %s: sin iniciar (ningun registro leido)\n", fase); return; }
    klog("[GPU3D] r500 %s: RBBM_STATUS %x CP_CSQ_CNTL %x CP_CSQ_STAT %x CP_RB_CNTL %x ISYNC_CNTL %x\n", fase, rd(RBBM_STATUS),
         rd(CP_CSQ_CNTL), rd(CP_CSQ_STAT), rd(CP_RB_CNTL), rd(R5_ISYNC_CNTL));
    klog("[GPU3D] r500 %s: cola en MC %x: RPTR %x WPTR %x (escrito %x, avisado %x); apuntes: prueba %x valla %x (emitida %x, vista %x)\n",
         fase, rd(CP_RB_BASE), rd(CP_RB_RPTR), rd(CP_RB_WPTR), wptr, enviado, rd(APUNTE_TESTE), rd(APUNTE_VALLA), seq, seq_vista);
    klog("[GPU3D] r500 %s: %s %04x, %u tuberias; VRAM en MC %x-%x, pantalla %x; zona 3D %x+%u KB (libres %u KB)\n", fase,
         nome_familia[familia], modelo, tuberias, mc_fb_ini, mc_fb_fim, mc_tela, mc_zona, zona_bytes >> 10,
         pronto ? (uint32_t)(g3d_arena_livre(&arena) >> 10) : 0);
    klog("[GPU3D] r500 %s: destino %dx%d paso %u: color %x, Z%d %x; GB_PIPE_SELECT %x PLL[0D] %x\n", fase, alvo.larg, alvo.alt,
         alvo.passo_px, alvo.cor_gpu, alvo.zbits, alvo.z_gpu, rd(R5_GB_PIPE_SELECT), pll_ler(PLL_SCLK_PIPE));
}

/* ------------------------------------------------------------ la cola */

static uint32_t livre(void) { return ((rd(CP_RB_RPTR) & (ANEL_PAL - 1)) - wptr - 1) & (ANEL_PAL - 1); }

/* Dice al CP hasta dónde puede leer (rellenando hasta un múltiplo de 16 palabras, como Linux). */
static void avisar(void)
{
    while (wptr & 15) { anel[wptr] = R5_PACOTE2; wptr = (wptr + 1) & (ANEL_PAL - 1); }
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");   /* lo escrito en la VRAM, antes que el aviso */
    wr(CP_RB_WPTR, wptr);
    (void)rd(CP_RB_WPTR);
    enviado = wptr;
}

static bool motor_parado(uint32_t ms)
{
    prazo_t p = prazo_ms(ms);
    while (rd(RBBM_STATUS) & RBBM_ATIVO)
        if (prazo_vencido(&p)) return false;
    return true;
}

/* Manda lo pendiente con una valla detrás y espera a que el CP la escriba y el motor pare. */
static int sincronizar(uint32_t ms)
{
    if (!cp_ligado) return G3D_FALHA;
    if (wptr == enviado && seq_vista == seq) return G3D_OK;   /* nada pendiente */
    uint32_t b[16];
    g3d_fila_t f;
    g3d_fila_iniciar(&f, b, 16);
    seq++;
    r5_valla(&f, APUNTE_VALLA, seq);
    /* Siempre queda sitio para la valla: emitir() guarda 64 palabras. */
    for (uint32_t i = 0; i < f.n; i++) { anel[wptr] = b[i]; wptr = (wptr + 1) & (ANEL_PAL - 1); }
    avisar();
    prazo_t p = prazo_ms(ms);
    while (rd(APUNTE_VALLA) != seq)
        if (prazo_vencido(&p)) {
            klog("[GPU3D] r500: la valla %x no llega en %u ms (apunte %x, RPTR %x, WPTR %x, RBBM_STATUS %x)\n", seq, ms,
                 rd(APUNTE_VALLA), rd(CP_RB_RPTR), wptr, rd(RBBM_STATUS));
            return G3D_FALHA;
        }
    seq_vista = seq;
    if (!motor_parado(ms)) {
        klog("[GPU3D] r500: la valla %x llego pero el motor sigue ocupado (RBBM_STATUS %x)\n", seq, rd(RBBM_STATUS));
        return G3D_FALHA;
    }
    return G3D_OK;
}

/* Copia una cola preparada al anillo. Si no cabe, manda lo que hay y espera a que termine. */
static int emitir(const g3d_fila_t *f)
{
    if (!cp_ligado) return G3D_FALHA;
    if (f->transbordou || f->n + 64 >= ANEL_PAL) {
        klog("[GPU3D] r500: una cola de ordenes no cupo en su bufer (%u palabras; error interno)\n", f->n);
        return G3D_FALHA;
    }
    if (livre() < f->n + 64) {
        int r = sincronizar(GPU3D_PRAZO_MS);
        if (r != G3D_OK) return r;
        if (livre() < f->n + 64) {
            klog("[GPU3D] r500: la cola no se vacia (RPTR %x, WPTR %x)\n", rd(CP_RB_RPTR), wptr);
            return G3D_FALHA;
        }
    }
    for (uint32_t i = 0; i < f->n; i++) { anel[wptr] = f->p[i]; wptr = (wptr + 1) & (ANEL_PAL - 1); }
    return G3D_OK;
}

/* ------------------------------------------------------------ apagar */

static void cp_parar(void)
{
    wr(CP_CSQ_MODE, 0);
    wr(CP_CSQ_CNTL, 0);
    wr(SCRATCH_UMSK, 0);
    cp_ligado = false;
}

static void r5_desligar(void)
{
    bool estava = pronto || cp_ligado;
    pronto = false;
    if (mmio && cp_ligado) {
        if (!motor_parado(50)) klog("[GPU3D] r500: el motor sigue ocupado al parar el CP (RBBM_STATUS %x)\n", rd(RBBM_STATUS));
        cp_parar();
        if (isync_guardado) wr(R5_ISYNC_CNTL, isync_antes);
        klog("[GPU3D] r500: CP parado (CP_CSQ_CNTL %x, RBBM_STATUS %x); la pantalla y el 2D siguen como estaban\n",
             rd(CP_CSQ_CNTL), rd(RBBM_STATUS));
    }
    isync_guardado = false;
    if (prep) { kfree(prep); prep = NULL; }
    memset(&alvo, 0, sizeof alvo);
    if (estava) klog("[GPU3D] r500: 3D apagado\n");
}

/* ------------------------------------------------------------ encender */

static int familia_de(uint16_t m)
{
    for (unsigned i = 0; i < sizeof modelos / sizeof modelos[0]; i++)
        if (m >= modelos[i].de && m <= modelos[i].ate) return modelos[i].familia;
    return -1;
}

static inline uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

static uint32_t ler_bar(const pci_disp_t *d, uint8_t reg, bool *alto)
{
    uint32_t v = pci_le32(d, reg);
    *alto = (v & 1) == 0 && ((v >> 1) & 3) == 2 && pci_le32(d, (uint8_t)(reg + 4)) != 0;
    return v;
}

/* Etapa 1 (solo lecturas): la tarjeta, su memoria y dónde cabe la zona 3D. */
static int sondar(const pci_disp_t *d, const acel_tela_t *t, char *m, int nm)
{
    bool alto0, alto2;
    uint32_t bar0 = ler_bar(d, 0x10, &alto0), bar2 = ler_bar(d, 0x18, &alto2);
    if ((bar0 & 1) || (bar2 & 1) || alto0 || alto2 || !(bar2 & ~0xFu) || !(bar0 & ~0xFu)) {
        snformatar(m, (size_t)nm, "BAR 0 (%x) o BAR 2 (%x) no son de memoria de 32 bits", bar0, bar2);
        return G3D_NAO;
    }
    bar0 &= ~0xFu;
    if (!(pci_le32(d, 0x04) & 0x02)) { snformatar(m, (size_t)nm, "la tarjeta no tiene la memoria activada en el PCI"); return G3D_NAO; }
    if (!mmio || mmio_fis != (bar2 & ~0xFu)) {
        mmio_fis = bar2 & ~0xFu;
        mmio = (volatile uint8_t *)janela_mapear_tipo(mmio_fis, MMIO_TAM, PG_PCD | PG_PWT);
    }
    if (!mmio) { snformatar(m, (size_t)nm, "sin ventana para el MMIO"); return G3D_NAO; }
    uint32_t est = rd(RBBM_STATUS);
    if (est == 0xFFFFFFFFu) { snformatar(m, (size_t)nm, "el MMIO no responde (RBBM_STATUS %x)", est); return G3D_NAO; }
    /* La memoria de vídeo en el MC y la pantalla dentro de ella. */
    uint32_t loc = mc_ler(familia == F_RV515 ? MC_FB_LOC_RV515 : MC_FB_LOC_R520);
    uint32_t memsize = rd(CONFIG_MEMSIZE), aper = rd(CONFIG_APER_SIZE);
    mc_fb_ini = (loc & 0xFFFFu) << 16;
    mc_fb_fim = ((loc >> 16) << 16) | 0xFFFFu;
    uint32_t pp = t->pitch / 4;
    uint32_t e1 = rd(D1GRPH_ENABLE), s1 = rd(D1GRPH_SUPERFICIE), p1 = rd(D1GRPH_PITCH);
    uint32_t e2 = rd(D1GRPH_ENABLE + D2_DESLOC), s2 = rd(D1GRPH_SUPERFICIE + D2_DESLOC), p2 = rd(D1GRPH_PITCH + D2_DESLOC);
    klog("[GPU3D] r500: etapa 1: %s %04x; BAR0 %x BAR2 %x; MC_FB_LOCATION %x (VRAM en MC %x-%x); CONFIG_MEMSIZE %u MB, apertura %u MB, VBE %u KB\n",
         nome_familia[familia], modelo, bar0, bar2, loc, mc_fb_ini, mc_fb_fim, memsize >> 20, aper >> 20, t->memoria >> 10);
    klog("[GPU3D] r500: etapa 1: D1 %s superficie %x pitch %u; D2 %s superficie %x pitch %u; RBBM_STATUS %x CP_CSQ_CNTL %x\n",
         e1 & 1 ? "activo" : "apagado", s1, p1, e2 & 1 ? "activo" : "apagado", s2, p2, est, rd(CP_CSQ_CNTL));
    if ((e1 & 1) && p1 == pp) mc_tela = s1;
    else if ((e2 & 1) && p2 == pp) mc_tela = s2;
    else { snformatar(m, (size_t)nm, "ningun controlador AVIVO activo con el pitch del VBE (%u pixeles)", pp); return G3D_NAO; }
    if (loc == 0xFFFFFFFFu || mc_fb_fim <= mc_fb_ini || mc_tela < mc_fb_ini || mc_tela > mc_fb_fim) {
        snformatar(m, (size_t)nm, "la pantalla (MC %x) no esta en la VRAM que dice el MC (%x-%x)", mc_tela, mc_fb_ini, mc_fb_fim);
        return G3D_NAO;
    }
    /* La CPU tiene que ver la VRAM desde su principio por el BAR 0: la pantalla lo confirma. */
    uint32_t tela_off = mc_tela - mc_fb_ini;
    if (t->fb_fisico < bar0 || t->fb_fisico - bar0 != tela_off) {
        snformatar(m, (size_t)nm, "la pantalla esta en %x del BAR 0 pero en %x de la VRAM: no se sabe donde cae cada direccion",
                   t->fb_fisico - bar0, tela_off);
        return G3D_NAO;
    }
    /* Límite: lo menor entre la VRAM real, la apertura de la CPU y la VRAM del MC. */
    uint64_t lim = (uint64_t)mc_fb_fim - mc_fb_ini + 1;
    if (memsize < (16u << 20) || memsize == 0xFFFFFFFFu || aper < (16u << 20) || aper == 0xFFFFFFFFu) {
        snformatar(m, (size_t)nm, "tamanos de memoria raros (CONFIG_MEMSIZE %x, CONFIG_APER_SIZE %x)", memsize, aper);
        return G3D_NAO;
    }
    if (lim > memsize) lim = memsize;
    if (lim > aper) lim = aper;
    uint64_t ini64 = (uint64_t)tela_off + (uint64_t)t->pitch * (uint32_t)(t->alt + 2 * ACEL2D_ZONA_A);
    ini64 = (ini64 + 0xFFFFFu) & ~0xFFFFFull;
    if (lim < ini64 + ZONA_MIN) {
        snformatar(m, (size_t)nm, "no caben 2 MB para el 3D detras de la pantalla (VRAM util %u KB, hace falta desde %u KB)",
                   (uint32_t)(lim >> 10), (uint32_t)(ini64 >> 10));
        return G3D_NAO;
    }
    uint32_t ini = (uint32_t)ini64, tam = (uint32_t)(lim - ini64);
    if (tam > ZONA_MAX) tam = ZONA_MAX;
    tam &= ~0xFFFFFu;
    if ((uint64_t)mc_fb_ini + ini + tam > 0xFFFFFFFFull || (uint64_t)bar0 + ini + tam > 0xFFFFFFFFull) {
        snformatar(m, (size_t)nm, "la zona 3D pasa de los 4 GB");
        return G3D_NAO;
    }
    mc_zona = mc_fb_ini + ini;
    /* Nada de lo que muestra la tarjeta puede estar ahí: cursores y el otro controlador. */
    for (int c = 0; c < 2; c++) {
        uint32_t cc = rd(D1CUR_CONTROL + (c ? D2_DESLOC : 0)), cs = rd(D1CUR_SUPERFICIE + (c ? D2_DESLOC : 0));
        uint32_t ge = c ? e2 : e1, gs = c ? s2 : s1;
        if ((cc & 1) && cs + 0x4000u > mc_zona && cs < mc_zona + tam) {
            snformatar(m, (size_t)nm, "el cursor por hardware de D%d (MC %x) esta en la zona 3D", c + 1, cs);
            return G3D_NAO;
        }
        if ((ge & 1) && gs != mc_tela && gs + (8u << 20) > mc_zona && gs < mc_zona + tam) {
            snformatar(m, (size_t)nm, "la superficie de D%d (MC %x) esta cerca de la zona 3D", c + 1, gs);
            return G3D_NAO;
        }
    }
    zona_off = ini; zona_bytes = tam;
    uint32_t fis = bar0 + ini;
    if (!zona_cpu || zona_fis_map != fis || zona_bytes_map < tam) {
        zona_cpu = (uint8_t *)janela_mapear_tipo(fis, tam, PG_PCD | PG_PWT);
        zona_fis_map = fis; zona_bytes_map = tam;
    }
    if (!zona_cpu) { snformatar(m, (size_t)nm, "sin ventana del kernel para %u KB de memoria de video", tam >> 10); return G3D_NAO; }
    klog("[GPU3D] r500: etapa 1: zona 3D de %u KB en MC %x (a %u KB del principio de la VRAM; CPU %x)\n", tam >> 10, mc_zona,
         ini >> 10, fis);
    return G3D_OK;
}

static int r5_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, uint32_t variante, char *m, int nm)
{
    (void)variante;   /* en R5xx las órdenes van siempre en el anillo del CP */
    pronto = false;
    if (SEM_FIRMWARE) {
        snformatar(m, (size_t)nm, "esta compilacion no lleva firmware (SEM_FIRMWARE=1): sin el microcodigo del CP no hay 3D en R5xx");
        return G3D_NAO;
    }
    int fam = d->fabricante == 0x1002 && geracao == 2 ? familia_de(d->modelo) : -1;
    if (fam < 0) {
        snformatar(m, (size_t)nm, "%04x:%04x no es una Radeon R5xx de la lista", d->fabricante, d->modelo);
        return G3D_NAO;
    }
    if (cp_ligado) r5_desligar();
    familia = fam; modelo = d->modelo;
    if (!hw_tsc_khz()) { snformatar(m, (size_t)nm, "sin TSC medido para los plazos de la GPU"); return G3D_NAO; }
    int r = sondar(d, t, m, nm);
    if (r != G3D_OK) return r;

    /* Etapa 2: el microcódigo, del sistema de archivos. */
    uint8_t *uc = NULL;
    uint32_t nuc = 0;
    if (vfs_ler_inteiro("/lib/firmware/radeon/R520_cp.bin", &uc, &nuc) < 0 || nuc != UCODE_BYTES) {
        snformatar(m, (size_t)nm, "falta el microcodigo /lib/firmware/radeon/R520_cp.bin (o no mide %u bytes: %u)", UCODE_BYTES, nuc);
        if (uc) kfree(uc);
        return G3D_NAO;
    }
    if (!prep) prep = kmalloc(PREP_PAL * 4);
    if (!prep) { kfree(uc); snformatar(m, (size_t)nm, "sin memoria para preparar las ordenes"); return G3D_NAO; }
    /* ¿La CPU ve de verdad esa VRAM? Escribir y leer en la página de reserva. */
    anel = (volatile uint32_t *)zona_cpu;
    volatile uint32_t *res = (volatile uint32_t *)(zona_cpu + ANEL_BYTES);
    res[0] = 0x5A6D3D05u; res[1] = ~0x5A6D3D05u;
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    if (res[0] != 0x5A6D3D05u || res[1] != ~0x5A6D3D05u) {
        snformatar(m, (size_t)nm, "la VRAM de la zona 3D no guarda lo escrito (%x): no es memoria utilizable", res[0]);
        kfree(uc);
        return G3D_NAO;
    }
    if (!motor_parado(PRAZO_CURTO_MS)) {
        snformatar(m, (size_t)nm, "el motor grafico esta ocupado antes de empezar (RBBM_STATUS %x): no se toca", rd(RBBM_STATUS));
        kfree(uc);
        return G3D_NAO;
    }
    klog("[GPU3D] r500: etapa 2: microcodigo R520_cp.bin leido (%u bytes, primeras palabras %x %x); VRAM de la zona comprobada\n",
         nuc, be32(uc), be32(uc + 4));

    /* Etapa 3: tuberías de píxeles y su reloj (como rv515_gpu_init de Linux). */
    uint32_t gps = rd(R5_GB_PIPE_SELECT), pll = pll_ler(PLL_SCLK_PIPE);
    tuberias = ((gps >> 12) & 3) + 1;
    uint32_t pll_novo = (1u << ((rd(R5_DST_PIPE_CONFIG) >> 2) & 3)) | (((gps >> 8) & 0xFu) << 4);
    if (pll != pll_novo) pll_escrever(PLL_SCLK_PIPE, pll_novo);
    klog("[GPU3D] r500: etapa 3: GB_PIPE_SELECT %x (%u tuberias), PLL[0D] %x -> %x%s\n", gps, tuberias, pll, pll_ler(PLL_SCLK_PIPE),
         pll != pll_novo ? " (cambiado)" : " (ya estaba)");

    /* Etapa 4: CP parado, microcódigo dentro y la cola (en VRAM) a cero. Sin escrituras de vuelta. */
    isync_antes = rd(R5_ISYNC_CNTL); isync_guardado = true;
    cp_parar();
    wr(CP_ME_RAM_ADDR, 0);
    for (uint32_t i = 0; i < UCODE_BYTES; i += 8) {
        wr(CP_ME_RAM_DATAH, be32(uc + i));
        wr(CP_ME_RAM_DATAL, be32(uc + i + 4));
    }
    kfree(uc);
    for (uint32_t i = 0; i < ANEL_PAL; i++) anel[i] = R5_PACOTE2;
    uint32_t cntl = 13u | (9u << 8) | (1u << 18);   /* log2(64 KB / 8), bloques de 4 KB, lectura de 1 en 1 */
    wr(CP_RB_WPTR_DELAY, 64);
    wr(CP_RB_CNTL, cntl | RB_SEM_ATUALIZAR);
    wr(CP_RB_BASE, mc_zona);
    wr(CP_RB_CNTL, cntl | RB_RPTR_ESCREVER | RB_SEM_ATUALIZAR);
    wr(CP_RB_RPTR_WR, 0);
    wptr = enviado = 0;
    wr(CP_RB_WPTR, 0);
    wr(CP_RB_RPTR_ADDR, (mc_zona + ANEL_BYTES) & ~3u);
    wr(SCRATCH_ADDR, mc_zona + ANEL_BYTES + 0x800u);
    wr(SCRATCH_UMSK, 0);
    wr(CP_RB_CNTL, cntl | RB_SEM_ATUALIZAR);
    esperar_ms(1);
    uint32_t rptr0 = rd(CP_RB_RPTR);
    wr(CP_CSQ_MODE, (80u << 0) | (16u << 8));   /* dónde empiezan las colas indirectas (no se usan) */
    wr(CP_RB_WPTR_DELAY, 0);
    wr(CP_CSQ_MODE, 0x00004D4Du);
    wr(CP_CSQ_CNTL, CSQ_ANEL);
    cp_ligado = true;
    klog("[GPU3D] r500: etapa 4: CP en marcha: cola en MC %x, CP_RB_CNTL %x, RPTR %x, CP_CSQ_CNTL %x, RBBM_STATUS %x\n", rd(CP_RB_BASE),
         rd(CP_RB_CNTL), rptr0, rd(CP_CSQ_CNTL), rd(RBBM_STATUS));
    if (rptr0 != 0) {
        snformatar(m, (size_t)nm, "el puntero de lectura de la cola no quedo a cero (%x)", rptr0);
        r5_detalle("al iniciar");
        r5_desligar();
        return G3D_FALHA;
    }

    /* Etapa 5: la prueba de la cola: el CP tiene que escribir un registro de apuntes. */
    wr(APUNTE_TESTE, 0xCAFEDEADu);
    wr(APUNTE_VALLA, 0);
    seq = seq_vista = 0x3D500000u;
    anel[wptr++] = R5_PACOTE0(APUNTE_TESTE, 0);
    anel[wptr++] = 0xDEADBEEFu;
    avisar();
    prazo_t pz = prazo_ms(PRAZO_CURTO_MS);
    while (rd(APUNTE_TESTE) != 0xDEADBEEFu)
        if (prazo_vencido(&pz)) {
            snformatar(m, (size_t)nm, "el CP no ejecuta la prueba de la cola (apunte %x, RPTR %x, WPTR %x, RBBM_STATUS %x)",
                       rd(APUNTE_TESTE), rd(CP_RB_RPTR), wptr, rd(RBBM_STATUS));
            r5_detalle("prueba de la cola");
            r5_desligar();
            return G3D_FALHA;
        }
    klog("[GPU3D] r500: etapa 5: prueba de la cola superada (el CP lee la cola de la VRAM; RPTR %x)\n", rd(CP_RB_RPTR));

    /* Etapa 6: el estado fijo del 3D, con su valla. */
    g3d_fila_t f;
    g3d_fila_iniciar(&f, prep, PREP_PAL);
    r5_inicio(&f, tuberias, fpus_familia[familia]);
    if (emitir(&f) != G3D_OK || sincronizar(PRAZO_CURTO_MS) != G3D_OK) {
        snformatar(m, (size_t)nm, "el CP no termina el estado fijo del 3D (%u palabras; apunte %x, RBBM_STATUS %x)", f.n,
                   rd(APUNTE_VALLA), rd(RBBM_STATUS));
        r5_detalle("estado fijo");
        r5_desligar();
        return G3D_FALHA;
    }
    klog("[GPU3D] r500: etapa 6: estado fijo del 3D aceptado (%u palabras); RBBM_STATUS %x\n", f.n, rd(RBBM_STATUS));

    g3d_arena_iniciar(&arena, (uint64_t)mc_zona + ANEL_BYTES + RESERVA_BYTES, zona_cpu + ANEL_BYTES + RESERVA_BYTES,
                      zona_bytes - ANEL_BYTES - RESERVA_BYTES);
    memset(&b_cor, 0, sizeof b_cor); memset(&b_z, 0, sizeof b_z); memset(b_tex, 0, sizeof b_tex); memset(&alvo, 0, sizeof alvo);
    pronto = true;
    return G3D_OK;
}

/* ------------------------------------------------------------ lo que pide la capa */

static int r5_esperar(g3d_vala_t vala, uint32_t ms)
{
    (void)vala;   /* una sola cola: se espera a todo lo pendiente */
    if (!pronto) return G3D_FALHA;
    int r = sincronizar(ms);
    if (r != G3D_OK) r5_detalle("tiempo agotado");
    return r;
}

static void soltar_alvo(void)
{
    g3d_buffer_soltar(&arena, &b_cor);
    g3d_buffer_soltar(&arena, &b_z);
    memset(&alvo, 0, sizeof alvo);
}

static int r5_alvo(int larg, int alt, int zbits)
{
    if (!pronto) return G3D_FALHA;
    if (larg < 8 || alt < 8 || larg > GPU3D_MAX_LADO || alt > GPU3D_MAX_LADO) return G3D_NAO;
    if (r5_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    soltar_alvo();
    uint32_t passo_px = ((uint32_t)larg + 15) & ~15u;
    int zb = zbits == 16 ? 16 : 24;
    if (!g3d_buffer_criar(&arena, &b_cor, G3D_BUF_ALVO, (uint64_t)passo_px * 4 * (uint32_t)alt, 4096)) return G3D_NAO;
    if (!g3d_buffer_criar(&arena, &b_z, G3D_BUF_Z, (uint64_t)passo_px * (zb == 24 ? 4 : 2) * (uint32_t)alt, 4096)) {
        zb = 16;
        if (!g3d_buffer_criar(&arena, &b_z, G3D_BUF_Z, (uint64_t)passo_px * 2 * (uint32_t)alt, 4096)) { soltar_alvo(); return G3D_NAO; }
    }
    b_cor.passo = passo_px * 4; b_cor.larg = larg; b_cor.alt = alt; b_cor.bpp = 32;
    b_z.passo = passo_px * (zb == 24 ? 4 : 2); b_z.larg = larg; b_z.alt = alt; b_z.bpp = zb == 24 ? 32 : 16;
    alvo.cor_gpu = (uint32_t)b_cor.gpu; alvo.z_gpu = (uint32_t)b_z.gpu;
    alvo.passo_px = passo_px; alvo.larg = larg; alvo.alt = alt; alvo.zbits = zb;
    klog("[GPU3D] r500: destino %dx%d (paso %u): color en MC %x, Z de %d bits en MC %x; libres %u KB\n", larg, alt, passo_px,
         alvo.cor_gpu, zb, alvo.z_gpu, (uint32_t)(g3d_arena_livre(&arena) >> 10));
    return G3D_OK;
}

/* Una entrega: destino, estado y los vértices, de R5_VERT_POR_PACOTE en R5_VERT_POR_PACOTE. */
static int desenhar_int(const gpu3d_estado_t *e, const r5_tex_t *t, const gpu3d_vert_t *v, int n)
{
    for (int i = 0; i < n; i += R5_VERT_POR_PACOTE) {
        int q = n - i < R5_VERT_POR_PACOTE ? n - i : R5_VERT_POR_PACOTE;
        g3d_fila_t f;
        g3d_fila_iniciar(&f, prep, PREP_PAL);
        if (i == 0) {
            r5_estado_alvo(&f, &alvo);
            r5_estado_desenho(&f, e, t);
        }
        r5_triangulos(&f, v + i, q);
        int r = emitir(&f);
        if (r != G3D_OK) return r;
    }
    return G3D_OK;
}

/* Borrar es dibujar un rectángulo que siempre pasa, con el Z al fondo. */
static int r5_limpar(uint32_t mascara, uint32_t cor)
{
    if (!pronto || !b_cor.usado) return G3D_FALHA;
    if (!(mascara & 3)) return G3D_OK;
    gpu3d_estado_t e = { (mascara & 1 ? 0 : R5_INT_SEM_COR) | (mascara & 2 ? R5_INT_Z_FORCAR : 0), G3D_Z_SEMPRE, G3D_F_UM, G3D_F_ZERO, 0 };
    uint32_t x1 = g3d_float_de_fixo(alvo.larg, 0), y1 = g3d_float_de_fixo(alvo.alt, 0), um = 0x3F800000u;
    uint32_t c = 0xFF000000u | (cor & 0x00FFFFFFu);
    gpu3d_vert_t q[6] = {
        { 0, 0, um, um, c, 0, 0, 0 }, { x1, 0, um, um, c, 0, 0, 0 }, { x1, y1, um, um, c, 0, 0, 0 },
        { 0, 0, um, um, c, 0, 0, 0 }, { x1, y1, um, um, c, 0, 0, 0 }, { 0, y1, um, um, c, 0, 0, 0 },
    };
    int r = desenhar_int(&e, NULL, q, 6);
    if (r != G3D_OK) r5_detalle("al limpiar");
    return r;
}

static int r5_textura(int vaga, int larg, int alt, const uint32_t *argb)
{
    if (!pronto) return G3D_FALHA;
    if (vaga < 0 || vaga >= GPU3D_TEXTURAS) return G3D_NAO;
    g3d_buffer_t *b = &b_tex[vaga];
    if (!b->usado && !argb) return G3D_OK;
    /* Puede estar en uso por un dibujo pendiente. */
    if (r5_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    g3d_buffer_soltar(&arena, b);
    if (!argb) return G3D_OK;
    if (larg < 1 || alt < 1 || larg > GPU3D_MAX_LADO || alt > GPU3D_MAX_LADO) return G3D_NAO;
    uint32_t passo = (((uint32_t)larg + 15) & ~15u) * 4;
    if (!g3d_buffer_criar(&arena, b, G3D_BUF_TEXTURA, (uint64_t)passo * (uint32_t)alt, 4096)) return G3D_NAO;
    b->passo = passo; b->larg = larg; b->alt = alt; b->bpp = 32;
    for (int y = 0; y < alt; y++) {
        volatile uint32_t *dst = (volatile uint32_t *)(b->cpu + (uint32_t)y * passo);
        for (int x = 0; x < larg; x++) dst[x] = argb[y * larg + x];
    }
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    return G3D_OK;
}

static int r5_desenhar(const gpu3d_estado_t *e, const gpu3d_vert_t *v, int n, g3d_vala_t *vala)
{
    if (!pronto || !b_cor.usado) return G3D_FALHA;
    if (n <= 0 || n % 3) return G3D_NAO;
    gpu3d_estado_t ee = *e;
    ee.flags &= 0xFFu;   /* los flags internos no vienen de fuera */
    r5_tex_t tex, *pt = NULL;
    if (ee.flags & G3D_TEXTURA) {
        const g3d_buffer_t *b = ee.textura < GPU3D_TEXTURAS ? &b_tex[ee.textura] : NULL;
        if (!b || !b->usado) return G3D_NAO;
        /* Repetir solo vale con lados potencia de 2. */
        if ((ee.flags & G3D_TEX_REPETE_S) && (b->larg & (b->larg - 1))) return G3D_NAO;
        if ((ee.flags & G3D_TEX_REPETE_T) && (b->alt & (b->alt - 1))) return G3D_NAO;
        tex.gpu = (uint32_t)b->gpu; tex.passo_px = b->passo / 4; tex.larg = b->larg; tex.alt = b->alt;
        pt = &tex;
    }
    int r = desenhar_int(&ee, pt, v, n);
    if (r != G3D_OK) { r5_detalle("al dibujar"); return r; }
    if (vala) *vala = seq + 1;   /* la valla que llevará el próximo aviso */
    return G3D_OK;
}

static int r5_ler(uint32_t *dest, int passo_px)
{
    if (!pronto || !b_cor.usado) return G3D_FALHA;
    if (r5_esperar(0, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    for (int y = 0; y < alvo.alt; y++) {
        const volatile uint32_t *src = (const volatile uint32_t *)(b_cor.cpu + (uint32_t)y * b_cor.passo);
        uint32_t *d = dest + y * passo_px;
        for (int x = 0; x < alvo.larg; x++) d[x] = src[x] & 0x00FFFFFFu;
    }
    return G3D_OK;
}

static void r5_memoria(uint32_t *total_kb, uint32_t *livre_kb)
{
    *total_kb = pronto ? (uint32_t)(arena.bytes >> 10) : 0;
    *livre_kb = pronto ? (uint32_t)(g3d_arena_livre(&arena) >> 10) : 0;
}

const gpu3d_driver_t gpu3d_r500 = {
    "radeon-r500", r5_iniciar, r5_alvo, r5_limpar, r5_textura, r5_desenhar, r5_esperar, r5_ler, r5_desligar, r5_detalle, r5_memoria,
};
