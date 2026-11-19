// SPDX-License-Identifier: GPL-2.0-or-later
/* acel_gcn.c - copiar y rellenar rectángulos con el motor DMA de las Radeon
 * GCN 1.0 (Southern Islands: Tahiti, Pitcairn, Cape Verde, Oland; HD 7000,
 * R7/R9 200 y 300 de esas familias). D360; la capa común es la de la D195.
 *
 * SIN PROBAR EN HARDWARE: QEMU no emula ninguna Radeon. La capa común solo lo
 * activa si su autoprueba completa da los mismos píxeles que la CPU y termina
 * a tiempo (ninguna GCN está en «confirmadas»).
 *
 * Por qué así (D360): las Southern Islands conservan el motor DMA asíncrono de
 * Cayman (D286), **sin microcódigo** (amdgpu_ucode_get_load_type: SI es
 * «DIRECT» y si_dma no carga nada; radeon lo enciende con cayman_dma_resume).
 * Cambia el paquete de copia (cuenta en bytes, gcn_dma.h) y hay un relleno
 * con valor constante (CONSTANT_FILL) que amdgpu usa en SI: se prueba al
 * encender y, si no da lo esperado, los rellenos copian una fila pintada como
 * en R7xx. No toca el CP, el motor 3D, los relojes, el SMC, la memoria ni el
 * modo de pantalla; el segundo motor DMA (0xD800) no se toca.
 *
 * GCN 1.1 en adelante (Bonaire/Hawaii, Tonga/Fiji/Polaris, Vega) están en la
 * tabla solo para el diagnóstico: el driver dice la familia, los BAR y por qué
 * copia la CPU, y no mapea ni lee nada de la tarjeta (D360, gcn_por_que_nao).
 *
 * Antes de escribir nada se comprueba (solo leyendo):
 *   - el modelo exacto (gcn_dma.c) y que es de la generación de la fila;
 *   - el MMIO (BAR 2, de 64 bits por debajo de 4 GB; se mapean 128 KB) y que la
 *     pantalla del VBE/GOP está en el BAR 0;
 *   - un controlador de pantalla DCE 6 encendido, de 32 bits ARGB8888, lineal,
 *     con el pitch del VBE/GOP y sin desplazamiento, cuya superficie (40 bits)
 *     es la que da la apertura de la CPU (HDP_NONSURFACE_BASE + lo que hay
 *     entre el BAR 0 y la pantalla): dos registros independientes que tienen
 *     que coincidir;
 *   - que todo lo que va a tocar el DMA (pantalla, zona de prueba, anillo,
 *     página de control, fila del color, zona de paso) cae dentro de la VRAM
 *     del MC (MC_VM_FB_LOCATION), de CONFIG_MEMSIZE y de la memoria del VBE, a
 *     menos de 64 MB del principio del BAR 0, y no cruza un límite de 4 GB del
 *     MC (los paquetes llevan un byte alto fijo);
 *   - que el contexto 0 de memoria virtual de la GPU está apagado, que el DMA
 *     no está retenido en reinicio (SRBM_SOFT_RESET, solo se lee), que su
 *     anillo está apagado y el motor parado, y que ningún cursor por hardware
 *     tiene su imagen donde iría el anillo.
 * Después: anillo de 64 KB en la memoria de vídeo detrás de la zona de prueba,
 * sin escritura de vuelta del puntero ni interrupciones; se prueba escribiendo
 * una palabra, con una valla y con un relleno corto. Toda espera tiene plazo
 * (TSC). Al apagar se para el anillo y se dejan sus registros como estaban;
 * nunca se reinicia ningún bloque (SRBM_SOFT_RESET no se escribe).
 *
 * Referencia de los números (Linux 6.6, licencia MIT/X11 de AMD): sid.h,
 * si_dma.c, ni_dma.c, evergreen_reg.h, evergreen_dma.c y radeon_asic.c de
 * radeon; si_dma.c, si.c y sid.h de amdgpu. El código es propio
 * (docs/terceros.md). */
#include "acel2d.h"
#include "console.h"
#include "gcn_dma.h"
#include "paginacao.h"
#include "r600_dma.h"
#include "texto.h"

#define MMIO_TAM               0x20000     /* hasta el 6.º controlador de pantalla (0xBC00 + 0x6E70) */
/* Pantalla (DCE 6 = Evergreen): se suma gcn_crtc_desloc(n). */
#define GRPH_ENABLE            0x6800
#define GRPH_CONTROL           0x6804
#define GRPH_SWAP_CONTROL      0x680C
#define GRPH_SUPERFICIE        0x6810
#define GRPH_PITCH             0x6818
#define GRPH_SUPERFICIE_HI     0x681C
#define GRPH_OFF_X             0x6824
#define GRPH_OFF_Y             0x6828
#define GRPH_X_END             0x6834
#define GRPH_Y_END             0x6838
#define CUR_CONTROL            0x6998      /* bit 0: cursor por hardware encendido */
#define CUR_SUPERFICIE         0x699C
#define CUR_SUPERFICIE_HI      0x69A4
#define CRTC_CONTROL           0x6E70      /* bit 0: encendido */
/* Memoria y estado (sid.h). */
#define SRBM_STATUS            0x0E50
#define SRBM_SOFT_RESET        0x0E60      /* SOLO se lee */
#define   SOFT_RESET_DMA       (1u << 20)
#define SRBM_STATUS2           0x0EC4
#define VM_CONTEXT0_CNTL       0x1410
#define MC_VM_FB_LOCATION      0x2024
#define MC_VM_SYS_APER_BAIXO   0x2034      /* MC_VM_SYSTEM_APERTURE_LOW_ADDR (>> 12), solo para el log */
#define MC_VM_SYS_APER_ALTO    0x2038      /* MC_VM_SYSTEM_APERTURE_HIGH_ADDR (>> 12), solo para el log */
#define MC_VM_SYS_APER_PADRAO  0x203C      /* MC_VM_SYSTEM_APERTURE_DEFAULT_ADDR (>> 12), solo para el log */
#define MC_VM_MX_L1_TLB_CNTL   0x2064      /* solo para el log (el plan B de la D360 la programaría) */
#define HDP_NONSURFACE_BASE    0x2C04      /* la apertura de la CPU (BAR 0) en el MC, >> 8 */
#define HDP_DEBUG0             0x2F30      /* escribir 1: invalida la caché de lectura del HDP (si_invalidate_hdp) */
#define CONFIG_MEMSIZE         0x5428      /* en MB */
#define HDP_MEM_COHERENCY_FLUSH 0x5480
/* Motor DMA 0 (el 1 está en +0x800 y no se toca). */
#define DMA_RB_CNTL            0xD000
#define   DMA_RB_ENABLE        0x1u
#define   DMA_RB_TAM(l2)       ((uint32_t)(l2) << 1)
#define DMA_RB_BASE            0xD004
#define DMA_RB_RPTR            0xD008
#define DMA_RB_WPTR            0xD00C
#define DMA_RB_RPTR_ADDR_HI    0xD01C
#define DMA_RB_RPTR_ADDR_LO    0xD020
#define DMA_IB_CNTL            0xD024
#define   DMA_IB_ENABLE        0x1u
#define   DMA_CMD_VMID_FORCE   (1u << 31)
#define DMA_CNTL               0xD02C
#define   DMA_INTERRUPCOES     ((1u << 0) | (1u << 1) | (1u << 2) | (1u << 28))  /* trap, semáforos, ctx vacío */
#define   DMA_TROCA_BYTES      ((1u << 3) | (1u << 4))   /* DATA_SWAP y FENCE_SWAP (solo big endian) */
#define DMA_STATUS_REG         0xD034
#define   DMA_IDLE             0x1u
#define DMA_SEM_INCOMPLETE_TIMER_CNTL 0xD044
#define DMA_SEM_WAIT_FAIL_TIMER_CNTL  0xD048
#define PTR_MASCARA            0x3FFFCu

#define RING_BYTES             (64 * 1024)
#define RING_DW                (RING_BYTES / 4)
#define RING_L2                14          /* log2 de las palabras del anillo */
#define CTRL_BYTES             4096
#define CTRL_VALLA             0           /* palabras de la página de control */
#define CTRL_TESTE             1
#define CTRL_RPTR              2
#define CTRL_ENCHER            16          /* 16 palabras para probar CONSTANT_FILL */
#define MIN_FILAS_PASO         8
#define LIMITE_BAR0            (64u << 20) /* no se usa nada más allá de 64 MB desde el principio del BAR 0 */

static volatile uint8_t *mmio;
static uint32_t mmio_fis;
static volatile uint32_t *area;       /* anillo + página de control + fila del color (ventana sin caché) */
static uint32_t area_fis, area_tam;
static volatile uint32_t *ring, *ctrl, *linha_cor;
static uint32_t mc_ring, mc_ctrl, mc_linha_cor;   /* los 32 bits bajos; el byte alto es «alto» */
static uint8_t alto;
static r600_geo_t geo;
static const gcn_modelo_t *modelo;
static int larg, alt_util, crtc_tela;
static bool programado, pendente, usar_encher;
static uint32_t wptr, seq;
static uint32_t cor_atual, cor_largura;
static uint32_t o_rb_cntl, o_rb_base, o_ib_cntl, o_cntl, o_rptr_hi, o_rptr_lo, o_sem1, o_sem2;

static inline uint32_t rd(uint32_t r)             { return *(volatile uint32_t *)(mmio + r); }
static inline void     wr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(mmio + r) = v; }

/* Lo que la CPU escribió por el BAR 0 (anillo, fila del color) tiene que
 * llegar a la VRAM antes de que el DMA lo lea (si_flush_hdp). */
static void hdp_vaciar(void)
{
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    wr(HDP_MEM_COHERENCY_FLUSH, 1);
    (void)rd(HDP_MEM_COHERENCY_FLUSH);
}

/* Lo que escribió la GPU, visto por la CPU a través del BAR 0: sin datos
 * viejos en la caché de lectura del HDP (si_invalidate_hdp). */
static void hdp_invalidar(void)
{
    wr(HDP_DEBUG0, 1);
    (void)rd(HDP_DEBUG0);
}

static uint32_t ler_ctrl(int i)
{
    hdp_invalidar();
    return ctrl[i];
}

static uint32_t rptr_lido(void) { return (rd(DMA_RB_RPTR) & PTR_MASCARA) >> 2; }
static uint32_t livre(void) { return (rptr_lido() - wptr - 1) & (RING_DW - 1); }

static void emitir(uint32_t v)
{
    ring[wptr] = v;
    wptr = (wptr + 1) & (RING_DW - 1);
}

/* Rellena con NOP hasta 16 palabras (el align_mask 0xf del anillo de si_dma) y avisa al motor. */
static void submeter(void)
{
    while (wptr & 15) emitir(SI_DMA_NOP);
    hdp_vaciar();
    wr(DMA_RB_WPTR, (wptr << 2) & PTR_MASCARA);
}

static void diagnostico(const char *que)
{
    klog("[ACEL2D] gcn: %s: RPTR %x WPTR %x (escrito %x) STATUS %x RB_CNTL %x SRBM_STATUS %x STATUS2 %x valla %x (esperada %x)\n",
         que, rd(DMA_RB_RPTR), rd(DMA_RB_WPTR), (wptr << 2) & PTR_MASCARA, rd(DMA_STATUS_REG), rd(DMA_RB_CNTL),
         rd(SRBM_STATUS), rd(SRBM_STATUS2), ctrl ? ler_ctrl(CTRL_VALLA) : 0, seq);
}

static bool reservar(uint32_t n)
{
    if (livre() >= n + 16) return true;
    submeter();   /* lo escrito tiene que salir para que RPTR avance */
    acel_prazo_t p;
    acel_prazo(&p, 250);
    while (livre() < n + 16)
        if (acel_prazo_esgotado(&p)) { diagnostico("el anillo no avanza"); return false; }
    return true;
}

static bool pacote(const uint32_t *p, int n)
{
    if (n <= 0 || !reservar((uint32_t)n)) return false;
    for (int i = 0; i < n; i++) emitir(p[i]);
    pendente = true;
    return true;
}

/* Espera a que el motor acabe todo: una valla con un número nuevo y leerla. */
static bool g_esperar(void)
{
    if (!programado) return false;
    if (!pendente) return true;
    uint32_t p[4];
    seq++;
    if (!pacote(p, si_pkt_valla(p, mc_ctrl + CTRL_VALLA * 4, seq, alto))) return false;
    submeter();
    acel_prazo_t pz;
    acel_prazo(&pz, 250);
    while (ler_ctrl(CTRL_VALLA) != seq)
        if (acel_prazo_esgotado(&pz)) { diagnostico("tiempo agotado esperando la valla"); return false; }
    pendente = false;
    return true;
}

static bool saida_copia(void *ctx, uint32_t dst, uint32_t src, uint32_t ndw)
{
    (void)ctx;
    uint32_t p[SI_PKT_COPIAR_DW];
    return pacote(p, si_pkt_copiar(p, dst, src, ndw * 4u, alto));
}

static bool saida_barreira(void *ctx) { (void)ctx; return g_esperar(); }

static bool saida_encher(void *ctx, uint32_t dst, uint32_t bytes, uint32_t cor)
{
    (void)ctx;
    uint32_t p[SI_PKT_ENCHER_DW];
    return pacote(p, si_pkt_encher(p, dst, cor, bytes, alto));
}

static const r600_saida_t saida = { NULL, saida_copia, saida_barreira };
static const gcn_saida_encher_t saida_e = { NULL, saida_encher };

static bool dentro(int x, int y, int l, int a)
{
    return x >= 0 && y >= 0 && l > 0 && a > 0 && x + l <= larg && y + a <= alt_util;
}

/* «acel2d probar --detalle»: los registros clave de la memoria, del motor y de la pantalla, para el log. */
static void g_detalle(const char *fase)
{
    if (!mmio || !modelo || modelo->gen != GCN_GEN_SI) return;
    klog("[ACEL2D] gcn detalle (%s): %s (%04x) MC_VM_FB_LOCATION %x HDP_NONSURFACE_BASE %x VM_CONTEXT0_CNTL %x "
         "L1_TLB_CNTL %x CONFIG_MEMSIZE %x SRBM_SOFT_RESET %x\n", fase, modelo->nome, modelo->id, rd(MC_VM_FB_LOCATION),
         rd(HDP_NONSURFACE_BASE), rd(VM_CONTEXT0_CNTL), rd(MC_VM_MX_L1_TLB_CNTL), rd(CONFIG_MEMSIZE), rd(SRBM_SOFT_RESET));
    klog("[ACEL2D] gcn detalle (%s): DMA_RB_CNTL %x BASE %x (anillo en MC %x:%x) RPTR %x WPTR %x (escrito %x) "
         "DMA_CNTL %x IB_CNTL %x STATUS %x SRBM_STATUS %x STATUS2 %x\n", fase, rd(DMA_RB_CNTL), rd(DMA_RB_BASE), alto, mc_ring,
         rd(DMA_RB_RPTR), rd(DMA_RB_WPTR), (wptr << 2) & PTR_MASCARA, rd(DMA_CNTL), rd(DMA_IB_CNTL), rd(DMA_STATUS_REG),
         rd(SRBM_STATUS), rd(SRBM_STATUS2));
    if (crtc_tela >= 0) {
        uint32_t o = gcn_crtc_desloc(crtc_tela);
        klog("[ACEL2D] gcn detalle (%s): controlador %d: CRTC_CONTROL %x GRPH_ENABLE %x GRPH_CONTROL %x superficie %x "
             "(alta %x) pitch %u fin %ux%u cursor %x\n", fase, crtc_tela + 1, rd(CRTC_CONTROL + o), rd(GRPH_ENABLE + o),
             rd(GRPH_CONTROL + o), rd(GRPH_SUPERFICIE + o), rd(GRPH_SUPERFICIE_HI + o), rd(GRPH_PITCH + o),
             rd(GRPH_X_END + o), rd(GRPH_Y_END + o), rd(CUR_CONTROL + o));
    }
    klog("[ACEL2D] gcn detalle (%s): valla %x (esperada %x), pendiente %d, pantalla en MC %x:%x, pitch %u, zona de paso %d "
         "filas, rellenos por %s\n", fase, ctrl ? ler_ctrl(CTRL_VALLA) : 0, seq, pendente, alto, geo.mc_tela, geo.pitch,
         geo.filas_rascunho, usar_encher ? "CONSTANT_FILL" : "copia de una fila pintada");
}

static bool g_copiar(int ox, int oy, int dx, int dy, int l, int a)
{
    if (!programado || !dentro(ox, oy, l, a) || !dentro(dx, dy, l, a)) return false;
    if (!r600_copiar_ret(&geo, &saida, ox, oy, dx, dy, l, a)) return false;
    submeter();
    return true;
}

static bool g_preencher(int x, int y, int l, int a, uint32_t cor)
{
    if (!programado || !dentro(x, y, l, a)) return false;
    if (usar_encher) {
        if (!gcn_preencher_ret(&geo, &saida_e, x, y, l, a, cor)) return false;
        submeter();
        return true;
    }
    if (cor != cor_atual || (uint32_t)l > cor_largura) {
        /* La fila del color cambia: antes, que nadie la esté leyendo. */
        if (!g_esperar()) return false;
        uint32_t ini = cor == cor_atual ? cor_largura : 0;
        for (uint32_t i = ini; i < (uint32_t)l; i++) linha_cor[i] = cor;
        cor_atual = cor;
        cor_largura = (uint32_t)l;
    }
    if (!r600_preencher_ret(&geo, &saida, mc_linha_cor, x, y, l, a)) return false;
    submeter();   /* vacía el HDP: la fila del color llega a la VRAM antes que el paquete */
    return true;
}

static void g_desligar(void)
{
    if (!programado || !mmio) return;
    programado = false;
    wr(DMA_RB_CNTL, rd(DMA_RB_CNTL) & ~DMA_RB_ENABLE);   /* como cayman_dma_stop */
    acel_prazo_t p;
    acel_prazo(&p, 50);
    while (!(rd(DMA_STATUS_REG) & DMA_IDLE))
        if (acel_prazo_esgotado(&p)) { diagnostico("el DMA sigue ocupado al apagarlo"); break; }
    wr(DMA_RB_RPTR_ADDR_HI, o_rptr_hi);
    wr(DMA_RB_RPTR_ADDR_LO, o_rptr_lo);
    wr(DMA_IB_CNTL, o_ib_cntl);
    wr(DMA_SEM_INCOMPLETE_TIMER_CNTL, o_sem1);
    wr(DMA_SEM_WAIT_FAIL_TIMER_CNTL, o_sem2);
    wr(DMA_CNTL, o_cntl);
    wr(DMA_RB_BASE, o_rb_base);
    wr(DMA_RB_RPTR, 0);
    wr(DMA_RB_WPTR, 0);
    wr(DMA_RB_CNTL, o_rb_cntl & ~DMA_RB_ENABLE);
    klog("[ACEL2D] gcn: anillo del DMA apagado y sus registros como estaban\n");
}

/* Un BAR de memoria (de 32 o de 64 bits) por debajo de 4 GB: su base; 0 si no. */
static uint32_t bar_mem(const pci_disp_t *d, uint8_t reg, uint32_t *bruto)
{
    uint32_t v = pci_le32(d, reg);
    *bruto = v;
    if (v & 1) return 0;                                       /* de E/S */
    if (((v >> 1) & 3) == 2 && pci_le32(d, (uint8_t)(reg + 4)) != 0) return 0;   /* por encima de 4 GB */
    return v & ~0xFu;
}

static bool g_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *m, int nm)
{
    programado = false;
    usar_encher = false;
    crtc_tela = -1;
    modelo = d->fabricante == 0x1002 ? gcn_modelo(d->modelo) : NULL;
    if (!modelo || modelo->gen != geracao) {
        snformatar(m, (size_t)nm, "el modelo %04x no esta en la lista exacta de GCN: no se toca", d->modelo);
        return false;
    }
    klog("[ACEL2D] gcn: %s (%04x), %s, pantalla %x, pitch %u, %ux%u, %u KB del VBE/GOP\n", modelo->nome, d->modelo,
         gcn_nome_geracao(modelo->gen), t->fb_fisico, t->pitch, t->larg, t->alt, t->memoria >> 10);
    if (modelo->gen != GCN_GEN_SI) {
        /* D360: GCN 1.1 en adelante, solo el diagnóstico (los BAR, sin mapear nada). */
        klog("[ACEL2D] gcn: BAR0 %x %x, BAR2 %x %x, BAR5 %x; %s\n", pci_le32(d, 0x10), pci_le32(d, 0x14),
             pci_le32(d, 0x18), pci_le32(d, 0x1C), pci_le32(d, 0x24), gcn_por_que_nao(modelo->gen));
        snformatar(m, (size_t)nm, "%s", gcn_por_que_nao(modelo->gen));
        return false;
    }
    uint32_t b0, b2;
    uint32_t bar0 = bar_mem(d, 0x10, &b0), bar2 = bar_mem(d, 0x18, &b2);
    klog("[ACEL2D] gcn: BAR0 %x (alto %x), BAR2 %x (alto %x)\n", b0, pci_le32(d, 0x14), b2, pci_le32(d, 0x1C));
    if (!bar0) { snformatar(m, (size_t)nm, "el BAR 0 no es memoria por debajo de 4 GB (%x)", b0); return false; }
    if (!bar2) { snformatar(m, (size_t)nm, "el BAR 2 no es MMIO por debajo de 4 GB (%x)", b2); return false; }
    if (t->fb_fisico < bar0) { snformatar(m, (size_t)nm, "la pantalla (%x) no esta en el BAR 0 (%x)", t->fb_fisico, bar0); return false; }
    uint32_t desloc = t->fb_fisico - bar0;
    if (t->pitch & 3 || t->pitch < (uint32_t)t->larg * 4) { snformatar(m, (size_t)nm, "pitch %u no vale para %u pixeles", t->pitch, t->larg); return false; }
    /* El DMA solo toca la VRAM de la tarjeta: no hace falta el bus maestro, solo la memoria. Además se
     * APAGA el bus maestro mientras el DMA es nuestro (revisión D360): sin el contexto 0 ni la TLB
     * programados por un driver, una dirección que el MC no tomase por VRAM saldría al PCIe hacia la
     * RAM del PC; con el bus maestro apagado el puente la corta y solo falla la autoprueba. La capa
     * común deja el registro de órdenes como estaba al soltar la tarjeta. Las GCN dedicadas leen la
     * pantalla de su VRAM: el bus maestro no hace falta para mostrarla. */
    uint32_t cmd = pci_le32(d, 0x04);
    if (!(cmd & 0x02) || (cmd & 0x04)) {
        pci_escreve32(d, 0x04, ((cmd & 0xFFFFu) | 0x02) & ~0x04u);
        klog("[ACEL2D] gcn: registro de ordenes del PCI %x -> %x (memoria encendida, bus maestro apagado)\n",
             cmd & 0xFFFFu, pci_le32(d, 0x04) & 0xFFFFu);
    }
    if (!mmio || mmio_fis != bar2) {
        mmio_fis = bar2;
        mmio = (volatile uint8_t *)janela_mapear_tipo(mmio_fis, MMIO_TAM, PG_PCD | PG_PWT);
    }
    if (!mmio) { snformatar(m, (size_t)nm, "sin ventana para el MMIO"); return false; }
    uint32_t memreg = rd(CONFIG_MEMSIZE);
    if (memreg == 0xFFFFFFFFu) { snformatar(m, (size_t)nm, "el MMIO no responde (CONFIG_MEMSIZE %x)", memreg); return false; }

    /* La apertura de la CPU en el MC (40 bits): dónde está la pantalla para la GPU. */
    uint32_t hdp = rd(HDP_NONSURFACE_BASE), fbloc = rd(MC_VM_FB_LOCATION), vm0 = rd(VM_CONTEXT0_CNTL);
    uint32_t tlb = rd(MC_VM_MX_L1_TLB_CNTL), srst = rd(SRBM_SOFT_RESET);
    uint64_t mc_tela = ((uint64_t)hdp << 8) + desloc;
    uint64_t vram_ini = (uint64_t)(fbloc & 0xFFFFu) << 24, vram_fim = ((uint64_t)(fbloc >> 16) << 24) | 0xFFFFFFu;
    uint64_t memsize = gcn_memsize_bytes(memreg);
    acel2d_vram_real(memsize, "CONFIG_MEMSIZE");   /* D387 */
    klog("[ACEL2D] gcn: CONFIG_MEMSIZE %u MB, HDP_NONSURFACE_BASE %x, MC_VM_FB_LOCATION %x (VRAM %x:%x-%x:%x), "
         "VM_CONTEXT0_CNTL %x, L1_TLB_CNTL %x, SRBM_SOFT_RESET %x\n", memreg, hdp, fbloc, (uint32_t)(vram_ini >> 32),
         (uint32_t)vram_ini, (uint32_t)(vram_fim >> 32), (uint32_t)vram_fim, vm0, tlb, srst);
    /* La apertura del sistema del MC (si_mc_program la programa; el VBE/GOP, quizá no): con el contexto 0
     * apagado decide, junto con L1_TLB_CNTL, si el DMA ve la VRAM por dirección física. Solo se lee. */
    klog("[ACEL2D] gcn: apertura del sistema del MC %x-%x (por defecto %x), en paginas de 4 KB\n",
         rd(MC_VM_SYS_APER_BAIXO), rd(MC_VM_SYS_APER_ALTO), rd(MC_VM_SYS_APER_PADRAO));
    if (mc_tela >> 40) { snformatar(m, (size_t)nm, "la pantalla en el MC pasa de 40 bits (HDP %x)", hdp); return false; }

    /* El controlador de pantalla que muestra el framebuffer del VBE/GOP. */
    int ctrl_ok = -1;
    for (int c = 0; c < modelo->crtcs && c < GCN_MAX_CRTC; c++) {
        uint32_t o = gcn_crtc_desloc(c);
        gcn_crtc_t e = { rd(CRTC_CONTROL + o), rd(GRPH_ENABLE + o), rd(GRPH_CONTROL + o), rd(GRPH_SUPERFICIE + o),
                         rd(GRPH_SUPERFICIE_HI + o), rd(GRPH_PITCH + o), rd(GRPH_OFF_X + o), rd(GRPH_OFF_Y + o) };
        if (!(e.crtc_control & 1) && !(e.grph_enable & 1) && c >= 2) continue;   /* apagado: sin línea en el log */
        klog("[ACEL2D] gcn: controlador %d %s, GRPH %s, control %x, superficie %x (alta %x), pitch %u, desplazamiento %u,%u, "
             "fin %ux%u, orden de colores %x\n", c + 1, e.crtc_control & 1 ? "encendido" : "apagado",
             e.grph_enable & 1 ? "activo" : "apagado", e.grph_control, e.superficie, e.superficie_alta, e.pitch, e.off_x,
             e.off_y, rd(GRPH_X_END + o), rd(GRPH_Y_END + o), rd(GRPH_SWAP_CONTROL + o));
        if (ctrl_ok < 0 && gcn_crtc_mostra(&e, mc_tela, t->pitch / 4)) ctrl_ok = c;
    }
    if (ctrl_ok < 0) {
        snformatar(m, (size_t)nm, "ningun controlador muestra la pantalla del VBE/GOP (32 bits, lineal, pitch %u, MC %x:%x)",
                   t->pitch / 4, (uint32_t)(mc_tela >> 32), (uint32_t)mc_tela);
        return false;
    }
    crtc_tela = ctrl_ok;

    /* Lo que va a tocar el DMA, detrás de la pantalla y de la zona de prueba. */
    uint32_t zona_fim = t->pitch * (uint32_t)(t->alt + ACEL2D_ZONA_A);
    uint32_t area_off = (zona_fim + 0xFFFFu) & ~0xFFFFu;
    uint32_t fixo = area_off + RING_BYTES + CTRL_BYTES + t->pitch;   /* hasta el final de la fila del color */
    /* Lo que se puede usar desde la pantalla: lo que dijo el VBE/GOP, CONFIG_MEMSIZE (si es creíble) y, como
     * no hay registro con el tamaño de la apertura de la CPU (medir el BAR obligaría a escribirlo), nada más
     * allá de 64 MB desde el principio del BAR 0 (toda GCN tiene 256 MB de apertura o más). */
    uint32_t mem = t->memoria;
    if (memsize >= (16u << 20) && memsize < mem) mem = (uint32_t)memsize;
    uint32_t lim = desloc < LIMITE_BAR0 ? LIMITE_BAR0 - desloc : 0;
    if (mem > lim) mem = lim;
    int filas = mem > fixo ? (int)((mem - fixo) / t->pitch) : 0;
    if (filas > t->alt) filas = t->alt;
    if (filas < MIN_FILAS_PASO) {
        snformatar(m, (size_t)nm, "no cabe el anillo y la zona de paso en la memoria del VBE/GOP (%u KB)", mem >> 10);
        return false;
    }
    uint32_t total = fixo + (uint32_t)filas * t->pitch;
    if (mc_tela < vram_ini || mc_tela + total - 1 > vram_fim) {
        snformatar(m, (size_t)nm, "la zona del DMA (MC %x:%x, %u KB) no cae en la VRAM (MC_VM_FB_LOCATION %x)",
                   (uint32_t)(mc_tela >> 32), (uint32_t)mc_tela, total >> 10, fbloc);
        return false;
    }
    if ((mc_tela >> 32) != ((mc_tela + total - 1) >> 32)) {
        snformatar(m, (size_t)nm, "la zona del DMA cruza un limite de 4 GB del MC (%x:%x)", (uint32_t)(mc_tela >> 32),
                   (uint32_t)mc_tela);
        return false;
    }
    alto = (uint8_t)(mc_tela >> 32);
    uint32_t mc_lo = (uint32_t)mc_tela;
    /* Un cursor por hardware encendido (64x64, 16 KB) no puede tener su imagen
     * en la zona de prueba ni donde va el anillo. */
    for (int c = 0; c < modelo->crtcs && c < GCN_MAX_CRTC; c++) {
        uint32_t o = gcn_crtc_desloc(c);
        uint32_t cc = rd(CUR_CONTROL + o);
        uint64_t cs = ((uint64_t)(rd(CUR_SUPERFICIE_HI + o) & 0xFFu) << 32) | (rd(CUR_SUPERFICIE + o) & 0xFFFFF000u);
        if ((cc & 1) && cs + 0x4000u > mc_tela + t->pitch * (uint32_t)t->alt && cs < mc_tela + total) {
            snformatar(m, (size_t)nm, "el cursor por hardware del controlador %d esta donde iria el anillo", c + 1);
            return false;
        }
    }
    if (vm0 & 1) {
        snformatar(m, (size_t)nm, "la memoria virtual de la GPU esta encendida (VM_CONTEXT0_CNTL %x): no se arriesga", vm0);
        return false;
    }
    if (srst & SOFT_RESET_DMA) {
        snformatar(m, (size_t)nm, "el DMA esta retenido en reinicio (SRBM_SOFT_RESET %x): no se toca", srst);
        return false;
    }
    uint32_t rb = rd(DMA_RB_CNTL), st = rd(DMA_STATUS_REG);
    klog("[ACEL2D] gcn: DMA_RB_CNTL %x, DMA_STATUS %x, DMA_CNTL %x, DMA_IB_CNTL %x, SRBM_STATUS %x, SRBM_STATUS2 %x\n", rb, st,
         rd(DMA_CNTL), rd(DMA_IB_CNTL), rd(SRBM_STATUS), rd(SRBM_STATUS2));
    if (rb & DMA_RB_ENABLE) { snformatar(m, (size_t)nm, "el anillo del DMA ya esta encendido (DMA_RB_CNTL %x): no se toca", rb); return false; }
    if (!(st & DMA_IDLE)) { snformatar(m, (size_t)nm, "el DMA no esta parado (DMA_STATUS %x)", st); return false; }

    /* La ventana de la CPU sobre el anillo, la página de control y la fila del color. */
    uint32_t fis = t->fb_fisico + area_off, tam = RING_BYTES + CTRL_BYTES + t->pitch;
    if (!area || area_fis != fis || area_tam != tam) {
        area_fis = fis; area_tam = tam;
        area = (volatile uint32_t *)janela_mapear_tipo(fis, tam, PG_PCD | PG_PWT);
    }
    if (!area) { snformatar(m, (size_t)nm, "sin ventana para el anillo"); return false; }
    ring = area;
    ctrl = area + RING_DW;
    linha_cor = ctrl + CTRL_BYTES / 4;
    mc_ring = mc_lo + area_off;
    mc_ctrl = mc_ring + RING_BYTES;
    mc_linha_cor = mc_ctrl + CTRL_BYTES;
    geo.mc_tela = mc_lo;
    geo.pitch = t->pitch;
    geo.mc_rascunho = mc_linha_cor + t->pitch;
    geo.filas_rascunho = filas;
    larg = t->larg;
    alt_util = t->alt + ACEL2D_ZONA_A;
    cor_atual = 0; cor_largura = 0;
    seq = 0x6C700000u;
    for (int i = 0; i < 64; i++) ctrl[i] = 0;
    ctrl[CTRL_VALLA] = seq;
    klog("[ACEL2D] gcn: controlador %d, pantalla en el MC %x:%x, anillo en %x, zona de paso de %d filas en %x (%u KB)\n",
         ctrl_ok + 1, alto, mc_lo, mc_ring, filas, geo.mc_rascunho, total >> 10);

    /* Encender el anillo como cayman_dma_resume / si_dma_start (Linux 6.6), sin
     * escritura de vuelta ni IB: temporizadores de semáforos a 0, tamaño,
     * punteros, dirección de vuelta (nuestra página de control), base, IB con
     * CMD_VMID_FORCE, DMA_CNTL sin la interrupción de contexto vacío, puntero de
     * escritura y, por último, el bit de encendido. */
    o_rb_cntl = rb; o_rb_base = rd(DMA_RB_BASE); o_ib_cntl = rd(DMA_IB_CNTL); o_cntl = rd(DMA_CNTL);
    o_rptr_hi = rd(DMA_RB_RPTR_ADDR_HI); o_rptr_lo = rd(DMA_RB_RPTR_ADDR_LO);
    o_sem1 = rd(DMA_SEM_INCOMPLETE_TIMER_CNTL); o_sem2 = rd(DMA_SEM_WAIT_FAIL_TIMER_CNTL);
    programado = true;
    pendente = false;
    wptr = 0;
    uint32_t cntl = DMA_RB_TAM(RING_L2);
    wr(DMA_SEM_INCOMPLETE_TIMER_CNTL, 0);
    wr(DMA_SEM_WAIT_FAIL_TIMER_CNTL, 0);
    wr(DMA_RB_CNTL, cntl);
    wr(DMA_RB_RPTR, 0);
    wr(DMA_RB_WPTR, 0);
    wr(DMA_RB_RPTR_ADDR_HI, alto);
    wr(DMA_RB_RPTR_ADDR_LO, mc_ctrl + CTRL_RPTR * 4);
    wr(DMA_RB_BASE, (uint32_t)((mc_tela + area_off) >> 8));
    wr(DMA_IB_CNTL, DMA_IB_ENABLE | DMA_CMD_VMID_FORCE);
    wr(DMA_CNTL, o_cntl & ~(DMA_INTERRUPCOES | DMA_TROCA_BYTES));
    wr(DMA_RB_WPTR, 0);
    hdp_vaciar();
    wr(DMA_RB_CNTL, cntl | DMA_RB_ENABLE);

    /* Prueba 1 del anillo: escribir una palabra y leerla (r600_dma_ring_test). */
    uint32_t p[4];
    if (!pacote(p, si_pkt_escrever(p, mc_ctrl + CTRL_TESTE * 4, 0xCAFEDEADu, alto))) {
        snformatar(m, (size_t)nm, "el anillo del DMA no avanza");
        return false;
    }
    submeter();
    acel_prazo_t pz;
    acel_prazo(&pz, 250);
    while (ler_ctrl(CTRL_TESTE) != 0xCAFEDEADu)
        if (acel_prazo_esgotado(&pz)) {
            diagnostico("prueba del anillo");
            /* Para decidir el plan B (D360, hipótesis a/b): si RPTR no se movió, el motor no lee el anillo
             * en la VRAM (traducción del MC: L1 TLB/apertura); si se movió, lo leyó pero su escritura no se
             * ve (destino traducido a otro sitio o caché de lectura del HDP). */
            uint32_t rp = rd(DMA_RB_RPTR) & PTR_MASCARA;
            snformatar(m, (size_t)nm, rp ? "el DMA lee el anillo (RPTR %x) pero su escritura no se ve; L1_TLB %x"
                                         : "el DMA no lee el anillo en la VRAM (RPTR %x); L1_TLB %x",
                       rp, rd(MC_VM_MX_L1_TLB_CNTL));
            return false;
        }
    /* Prueba 2: la valla. */
    if (!g_esperar()) { snformatar(m, (size_t)nm, "el DMA no escribe la valla"); return false; }
    /* Prueba 3: CONSTANT_FILL en 14 palabras con una de margen a cada lado. Si
     * no da exactamente eso, los rellenos van copiando una fila pintada. */
    if (pacote(p, si_pkt_encher(p, mc_ctrl + (CTRL_ENCHER + 1) * 4, 0x5EED1234u, 14 * 4, alto)) && g_esperar()) {
        bool bem = ler_ctrl(CTRL_ENCHER) == 0 && ctrl[CTRL_ENCHER + 15] == 0;
        for (int i = 1; i <= 14; i++)
            if (ctrl[CTRL_ENCHER + i] != 0x5EED1234u) bem = false;
        usar_encher = bem;
        klog("[ACEL2D] gcn: prueba de CONSTANT_FILL: %s (%x %x .. %x %x)\n",
             bem ? "bien, los rellenos van por ella" : "no da lo esperado, los rellenos copian una fila pintada",
             ctrl[CTRL_ENCHER], ctrl[CTRL_ENCHER + 1], ctrl[CTRL_ENCHER + 14], ctrl[CTRL_ENCHER + 15]);
    } else {
        diagnostico("prueba de CONSTANT_FILL");
        snformatar(m, (size_t)nm, "el DMA no termina la prueba de CONSTANT_FILL");
        return false;
    }
    klog("[ACEL2D] gcn: anillo del DMA en marcha (prueba, valla y relleno; RPTR %x)\n", rd(DMA_RB_RPTR));
    return true;
}

const acel_driver_t acel_gcn = {
    "gcndma", ACEL2D_RADEON, g_iniciar, g_copiar, g_preencher, g_esperar, NULL, g_desligar,
    true,    /* la zona de prueba va debajo de la pantalla visible */
    4096,    /* menos de 64x64 píxeles lo hace la CPU (como en R7xx; sin medida en hardware) */
    true,    /* el DMA sigue copiando tras copiar() */
    g_detalle,
    false,   /* solo 32 bits */
    NULL,    /* D387: sin volcado por la GPU */
};
