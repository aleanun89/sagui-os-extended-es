/* acel_r600.c - copiar y rellenar rectángulos con el motor DMA de las Radeon
 * R6xx/R7xx (D223; la capa común es la de la D195).
 *
 * SIN PROBAR EN HARDWARE: QEMU no emula ninguna Radeon. La capa común solo lo
 * activa si su autoprueba del arranque da los mismos píxeles que la CPU y
 * termina a tiempo. El objetivo es la Mobility Radeon HD 4550 (RV710/M92) del
 * mantenedor.
 *
 * Por qué el motor DMA (D223): las R6xx/R7xx ya no tienen el motor 2D por
 * registros de las R5xx (acel_radeon.c). Lo que queda es el procesador de
 * órdenes (CP, que necesita su microcódigo PFP/ME y, para el 3D, preparar el
 * motor gráfico entero) o el motor DMA asíncrono, que **no lleva microcódigo**
 * en las R6xx/R7xx (el driver radeon de Linux solo carga el del CP; el DMA se
 * enciende con sus registros). El DMA copia memoria en línea: un rectángulo es
 * una copia por fila (r600_dma_pacotes.c), y un relleno es copiar a cada fila
 * una fila ya pintada del color. No toca el CP, el motor 3D, los relojes, la
 * memoria ni el modo de pantalla.
 *
 * Las R7xx (RV710, RV730, RV740, RV770/RV790) se eligen solas. Las R6xx (RV610,
 * RV620, RV630, RV635, RV670, R600) solo con --acel2d=radeon: Linux dejó de
 * usar su DMA para mover memoria porque en algunas daba inestabilidad.
 *
 * Antes de escribir nada se comprueba (solo leyendo):
 *   - que el modelo es uno de la lista exacta de abajo;
 *   - el MMIO (BAR 2, 64 KB) y que la pantalla del VBE está en el BAR 0;
 *   - un controlador de pantalla AVIVO (D1 o D2) encendido, de 32 bits ARGB, lineal,
 *     con el pitch del VBE y sin desplazamiento, cuya superficie en el MC es la
 *     misma que da la apertura de la CPU (HDP_NONSURFACE_BASE + lo que hay entre
 *     el BAR 0 y la pantalla): dos registros independientes que tienen que
 *     coincidir;
 *   - que todo lo que va a tocar el DMA (pantalla, zona de prueba, anillo, zona
 *     de paso) cae dentro de la VRAM del MC (MC_VM_FB_LOCATION), por debajo de
 *     4 GB y dentro de la memoria que anunció el VBE;
 *   - que el contexto 0 de memoria virtual de la GPU está apagado (si estuviera
 *     encendido, una dirección podría acabar en la RAM del PC);
 *   - que el anillo del DMA está apagado y el motor parado (no se toca uno que
 *     ya esté en uso).
 * Después: el anillo (64 KB) va en la memoria de vídeo detrás de la zona de
 * prueba, sin escritura de vuelta del puntero (se lee el registro) y sin
 * interrupciones; se prueba escribiendo una palabra y con una valla. Cualquier
 * espera tiene plazo (TSC). Al apagar se para el anillo y se dejan los
 * registros del DMA como estaban; no se reinicia ningún bloque de la GPU (un
 * bit equivocado de SRBM_SOFT_RESET podría apagar la pantalla).
 *
 * Referencia de los números de registro y de los paquetes: r600d.h, rv770d.h,
 * r500_reg.h, r600_reg.h, r600.c, rv770.c, r600_dma.c, rv770_dma.c y
 * r600_cs.c del driver radeon de Linux 6.6 (todas con la licencia MIT/X11 de
 * AMD y Red Hat en la cabecera) y la lista de modelos de drm_pciids.h
 * (SPDX MIT); el código es propio (docs/terceros.md). */
#include "acel2d.h"
#include "console.h"
#include "paginacao.h"
#include "r600_dma.h"
#include "texto.h"

#define MMIO_TAM               0x10000
/* Pantalla (AVIVO; D2 = D1 + 0x800). */
#define D1CRTC_CONTROL         0x6080
#define D1GRPH_ENABLE          0x6100
#define D1GRPH_CONTROL         0x6104      /* bits 1:0 profundidad (2 = 32 bits), 23:20 mosaico (0-1 lineal) */
#define D1GRPH_PRIMARY_SURFACE 0x6110
#define D1GRPH_PITCH           0x6120      /* en píxeles */
#define D1GRPH_SURFACE_OFF_X   0x6124
#define D1GRPH_SURFACE_OFF_Y   0x6128
#define D2_DESLOC              0x0800
#define D1GRPH_SURFACE_HIGH    0x6914      /* R7xx: bits 39:32 de la superficie */
#define D2GRPH_SURFACE_HIGH    0x6114
#define D1CUR_CONTROL          0x6400      /* bit 0: cursor por hardware encendido */
#define D1CUR_SURFACE          0x6408
/* Memoria. */
#define VM_CONTEXT0_CNTL       0x1410
#define HDP_NONSURFACE_BASE    0x2C04      /* la apertura de la CPU (BAR 0) en el MC, >> 8 */
#define R600_MC_VM_FB_LOCATION 0x2180
#define R700_MC_VM_FB_LOCATION 0x2024
#define CONFIG_MEMSIZE         0x5428
#define CONFIG_APER_SIZE       0x5430
#define HDP_MEM_COHERENCY_FLUSH 0x5480
#define HDP_DEBUG1             0x2F34
#define SRBM_STATUS            0x0E50
/* Motor DMA. */
#define DMA_RB_CNTL            0xD000
#define   DMA_RB_ENABLE        0x1u
#define   DMA_RB_TAM(l2)       ((uint32_t)(l2) << 1)
#define DMA_RB_BASE            0xD004
#define DMA_RB_RPTR            0xD008
#define DMA_RB_WPTR            0xD00C
#define DMA_RB_RPTR_ADDR_HI    0xD01C
#define DMA_RB_RPTR_ADDR_LO    0xD020
#define DMA_IB_CNTL            0xD024
#define DMA_CNTL               0xD02C
#define   DMA_INTERRUPCOES     ((1u << 0) | (1u << 1) | (1u << 2) | (1u << 28))  /* trap, semáforos, ctx vacío */
#define   DMA_TROCA_BYTES      ((1u << 3) | (1u << 4))   /* DATA_SWAP y FENCE_SWAP (solo big endian) */
#define DMA_STATUS_REG         0xD034
#define DMA_SEM_INCOMPLETE_TIMER_CNTL 0xD044   /* D246: Linux los pone a 0 al encender el anillo */
#define DMA_SEM_WAIT_FAIL_TIMER_CNTL  0xD048
#define   DMA_IB_ENABLE       0x1u
#define   DMA_IDLE             0x1u
#define DMA_MODE               0xD0BC      /* R7xx: 1 (como hace Linux) */
#define PTR_MASCARA            0x3FFFCu

#define RING_BYTES             (64 * 1024)
#define RING_DW                (RING_BYTES / 4)
#define RING_L2                14          /* log2 de las palabras del anillo */
#define CTRL_BYTES             4096
#define CTRL_VALLA             0           /* palabras de la página de control */
#define CTRL_TESTE             1
#define CTRL_RPTR              2
#define CTRL_HDP               3
#define MIN_FILAS_PASO         8

/* Los modelos exactos: todos los de esas familias en drm_pciids.h de Linux 6.6
 * (sin las integradas RS780/RS880). 7 = R7xx, 6 = R6xx (solo forzado). */
typedef struct { uint16_t id; uint8_t gen; const char *nome; } r600_modelo_t;
static const r600_modelo_t modelos[] = {
    /* RV710 (HD 4350/4550 y Mobility HD 4330-4570, M92/M93: la del mantenedor) */
    { 0x9540, 7, "RV710" }, { 0x9541, 7, "RV710" }, { 0x9542, 7, "RV710" }, { 0x954E, 7, "RV710" },
    { 0x954F, 7, "RV710" }, { 0x9552, 7, "RV710 (M92)" }, { 0x9553, 7, "RV710 (M92)" },
    { 0x9555, 7, "RV710 (M92/M93)" }, { 0x9557, 7, "RV710 (M93)" }, { 0x955F, 7, "RV710 (M92)" },
    /* RV730 (HD 4650/4670 y Mobility HD 4650/4670, M96) */
    { 0x9480, 7, "RV730 (M96)" }, { 0x9487, 7, "RV730" }, { 0x9488, 7, "RV730 (M96)" }, { 0x9489, 7, "RV730 (M96)" },
    { 0x948A, 7, "RV730 (M96)" }, { 0x948F, 7, "RV730" }, { 0x9490, 7, "RV730" }, { 0x9491, 7, "RV730 (M96)" },
    { 0x9495, 7, "RV730" }, { 0x9498, 7, "RV730" }, { 0x949C, 7, "RV730" }, { 0x949E, 7, "RV730" },
    { 0x949F, 7, "RV730" },
    /* RV740 (HD 4770 y Mobility HD 4830/4860, M97) */
    { 0x94A0, 7, "RV740 (M97)" }, { 0x94A1, 7, "RV740 (M97)" }, { 0x94A3, 7, "RV740 (M97)" }, { 0x94B1, 7, "RV740" },
    { 0x94B3, 7, "RV740" }, { 0x94B4, 7, "RV740" }, { 0x94B5, 7, "RV740" }, { 0x94B9, 7, "RV740" },
    /* RV770/RV790 (HD 4830-4890 y Mobility HD 4850/4870, M98) */
    { 0x9440, 7, "RV770" }, { 0x9441, 7, "RV770" }, { 0x9442, 7, "RV770" }, { 0x9443, 7, "RV770" },
    { 0x9444, 7, "RV770" }, { 0x9446, 7, "RV770" }, { 0x944A, 7, "RV770 (M98)" }, { 0x944B, 7, "RV770 (M98)" },
    { 0x944C, 7, "RV770" }, { 0x944E, 7, "RV770" }, { 0x9450, 7, "RV770" }, { 0x9452, 7, "RV770" },
    { 0x9456, 7, "RV770" }, { 0x945A, 7, "RV770 (M98)" }, { 0x945B, 7, "RV770 (M98)" }, { 0x945E, 7, "RV770 (M98)" },
    { 0x9460, 7, "RV770 (RV790)" }, { 0x9462, 7, "RV770 (RV790)" }, { 0x946A, 7, "RV770 (M98)" }, { 0x946B, 7, "RV770 (M98)" },
    { 0x947A, 7, "RV770 (M98)" }, { 0x947B, 7, "RV770 (M98)" },
    /* RV620 (HD 3450/3470 y Mobility HD 3400, M82) */
    { 0x95C0, 6, "RV620" }, { 0x95C2, 6, "RV620 (M82)" }, { 0x95C4, 6, "RV620 (M82)" }, { 0x95C5, 6, "RV620" },
    { 0x95C6, 6, "RV620" }, { 0x95C7, 6, "RV620" }, { 0x95C9, 6, "RV620" }, { 0x95CC, 6, "RV620" },
    { 0x95CD, 6, "RV620" }, { 0x95CE, 6, "RV620" }, { 0x95CF, 6, "RV620" },
    /* RV635 (HD 3650 y Mobility HD 3650/3670, M86) */
    { 0x9590, 6, "RV635" }, { 0x9591, 6, "RV635 (M86)" }, { 0x9593, 6, "RV635 (M86)" }, { 0x9595, 6, "RV635 (M86)" },
    { 0x9596, 6, "RV635" }, { 0x9597, 6, "RV635" }, { 0x9598, 6, "RV635" }, { 0x9599, 6, "RV635" },
    { 0x959B, 6, "RV635 (M86)" },
    /* RV610 (HD 2400 y Mobility HD 2400, M72/M74) */
    { 0x94C0, 6, "RV610" }, { 0x94C1, 6, "RV610" }, { 0x94C3, 6, "RV610" }, { 0x94C4, 6, "RV610" },
    { 0x94C5, 6, "RV610" }, { 0x94C6, 6, "RV610" }, { 0x94C7, 6, "RV610" }, { 0x94C8, 6, "RV610 (M74)" },
    { 0x94C9, 6, "RV610 (M72)" }, { 0x94CB, 6, "RV610 (M72)" }, { 0x94CC, 6, "RV610" }, { 0x94CD, 6, "RV610" },
    /* RV630 (HD 2600 y Mobility HD 2600, M76) */
    { 0x9580, 6, "RV630" }, { 0x9581, 6, "RV630 (M76)" }, { 0x9583, 6, "RV630 (M76)" }, { 0x9586, 6, "RV630" },
    { 0x9587, 6, "RV630" }, { 0x9588, 6, "RV630" }, { 0x9589, 6, "RV630" }, { 0x958A, 6, "RV630" },
    { 0x958B, 6, "RV630 (M76)" }, { 0x958C, 6, "RV630" }, { 0x958D, 6, "RV630" }, { 0x958E, 6, "RV630" },
    { 0x958F, 6, "RV630 (M76)" },
    /* RV670 (HD 3850/3870 y Mobility HD 3850/3870, M88) */
    { 0x9500, 6, "RV670" }, { 0x9501, 6, "RV670" }, { 0x9504, 6, "RV670 (M88)" }, { 0x9505, 6, "RV670" },
    { 0x9506, 6, "RV670 (M88)" }, { 0x9507, 6, "RV670" }, { 0x9508, 6, "RV670 (M88)" }, { 0x9509, 6, "RV670 (M88)" },
    { 0x950F, 6, "RV670" }, { 0x9511, 6, "RV670" }, { 0x9515, 6, "RV670" }, { 0x9517, 6, "RV670" },
    { 0x9519, 6, "RV670" },
    /* R600 (HD 2900) */
    { 0x9400, 6, "R600" }, { 0x9401, 6, "R600" }, { 0x9402, 6, "R600" }, { 0x9403, 6, "R600" },
    { 0x9405, 6, "R600" }, { 0x940A, 6, "R600" }, { 0x940B, 6, "R600" }, { 0x940F, 6, "R600" },
};
#define N_MODELOS (int)(sizeof modelos / sizeof modelos[0])

static volatile uint8_t *mmio;
static uint32_t mmio_fis;
static volatile uint32_t *area;       /* anillo + página de control + fila del color (ventana sin caché) */
static uint32_t area_fis, area_tam;
static volatile uint32_t *ring, *ctrl, *linha_cor;
static uint32_t mc_ring, mc_ctrl, mc_linha_cor;
static r600_geo_t geo;
static int gen, larg, alt_util;       /* alt_util: filas que se pueden tocar (pantalla + zona de prueba) */
static bool agp, programado, pendente;
static uint32_t wptr, seq;
static uint32_t cor_atual, cor_largura;   /* lo que tiene pintado la fila del color */
/* Los registros del DMA antes de tocarlos (se dejan igual al apagar). */
static uint32_t o_rb_cntl, o_rb_base, o_ib_cntl, o_cntl, o_mode, o_rptr_hi, o_rptr_lo, o_sem1, o_sem2;

static inline uint32_t rd(uint32_t r)             { return *(volatile uint32_t *)(mmio + r); }
static inline void     wr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(mmio + r) = v; }

/* La lista exacta: NULL si el modelo no está. */
static const r600_modelo_t *modelo_de(uint16_t id)
{
    for (int i = 0; i < N_MODELOS; i++)
        if (modelos[i].id == id) return &modelos[i];
    return NULL;
}

/* Lo que la CPU escribió por el BAR 0 (anillo, pantalla, fila del color) tiene
 * que llegar a la VRAM antes de que el DMA lo lea: vaciar el write-combining de
 * la CPU y la caché de escritura del HDP. En las R7xx por PCIe, como Linux
 * (r600_mmio_hdp_flush), HDP_DEBUG1 y una lectura de la VRAM; si no, el
 * registro de vaciado. */
static void hdp_vaciar(void)
{
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    if (gen >= 7 && !agp) {
        wr(HDP_DEBUG1, 0);
        (void)ctrl[CTRL_HDP];
    } else {
        wr(HDP_MEM_COHERENCY_FLUSH, 1);
        (void)rd(HDP_MEM_COHERENCY_FLUSH);
    }
}

static uint32_t rptr_lido(void) { return (rd(DMA_RB_RPTR) & PTR_MASCARA) >> 2; }
static uint32_t livre(void) { return (rptr_lido() - wptr - 1) & (RING_DW - 1); }

static void emitir(uint32_t v)
{
    ring[wptr] = v;
    wptr = (wptr + 1) & (RING_DW - 1);
}

/* Rellena con NOP hasta 16 palabras (lo que hace Linux) y avisa al motor. */
static void submeter(void)
{
    while (wptr & 15) emitir(R600_DMA_NOP);
    hdp_vaciar();
    wr(DMA_RB_WPTR, (wptr << 2) & PTR_MASCARA);
}

static void diagnostico(const char *que)
{
    klog("[ACEL2D] r600: %s: RPTR %x WPTR %x (escrito %x) STATUS %x RB_CNTL %x SRBM_STATUS %x valla %x (esperada %x)\n",
         que, rd(DMA_RB_RPTR), rd(DMA_RB_WPTR), (wptr << 2) & PTR_MASCARA, rd(DMA_STATUS_REG), rd(DMA_RB_CNTL),
         rd(SRBM_STATUS), ctrl ? ctrl[CTRL_VALLA] : 0, seq);
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
    if (!reservar((uint32_t)n)) return false;
    for (int i = 0; i < n; i++) emitir(p[i]);
    pendente = true;
    return true;
}

/* Espera a que el motor acabe todo: una valla con un número nuevo y leerla. */
static bool r6_esperar(void)
{
    if (!programado) return false;
    if (!pendente) return true;
    uint32_t p[4];
    seq++;
    r600_pkt_valla(p, mc_ctrl + CTRL_VALLA * 4, seq);
    if (!pacote(p, 4)) return false;
    submeter();
    acel_prazo_t pz;
    acel_prazo(&pz, 250);
    while (ctrl[CTRL_VALLA] != seq)
        if (acel_prazo_esgotado(&pz)) { diagnostico("tiempo agotado esperando la valla"); return false; }
    pendente = false;
    return true;
}

static bool saida_copia(void *ctx, uint32_t dst, uint32_t src, uint32_t ndw)
{
    (void)ctx;
    uint32_t p[R600_PKT_MAX];
    int n = r600_pkt_copiar(p, dst, src, ndw, gen >= 7);   /* 5 palabras en R7xx, 4 en R6xx */
    return pacote(p, n);
}

static bool saida_barreira(void *ctx) { (void)ctx; return r6_esperar(); }

static const r600_saida_t saida = { NULL, saida_copia, saida_barreira };

static bool dentro(int x, int y, int l, int a)
{
    return x >= 0 && y >= 0 && l > 0 && a > 0 && x + l <= larg && y + a <= alt_util;
}

/* D246: «acel2d probar --detalle»: los registros clave de la memoria y del motor, para el log. */
static void r6_detalle(const char *fase)
{
    if (!mmio) return;
    uint32_t fbl = rd(gen >= 7 ? R700_MC_VM_FB_LOCATION : R600_MC_VM_FB_LOCATION);
    klog("[ACEL2D] r600 detalle (%s): MC_VM_FB_LOCATION %x HDP_NONSURFACE_BASE %x VM_CONTEXT0_CNTL %x "
         "CONFIG_MEMSIZE %x APER %x\n", fase, fbl, rd(HDP_NONSURFACE_BASE), rd(VM_CONTEXT0_CNTL), rd(CONFIG_MEMSIZE),
         rd(CONFIG_APER_SIZE));
    klog("[ACEL2D] r600 detalle (%s): DMA_RB_CNTL %x BASE %x (anillo en MC %x) RPTR %x WPTR %x (escrito %x) "
         "DMA_CNTL %x IB_CNTL %x MODE %x STATUS %x SRBM_STATUS %x\n", fase, rd(DMA_RB_CNTL), rd(DMA_RB_BASE), mc_ring,
         rd(DMA_RB_RPTR), rd(DMA_RB_WPTR), (wptr << 2) & PTR_MASCARA, rd(DMA_CNTL), rd(DMA_IB_CNTL),
         gen >= 7 ? rd(DMA_MODE) : 0, rd(DMA_STATUS_REG), rd(SRBM_STATUS));
    klog("[ACEL2D] r600 detalle (%s): valla %x (esperada %x), pendiente %d, pantalla en MC %x, pitch %u, zona de paso %d filas\n",
         fase, ctrl ? ctrl[CTRL_VALLA] : 0, seq, pendente, geo.mc_tela, geo.pitch, geo.filas_rascunho);
}

static bool r6_copiar(int ox, int oy, int dx, int dy, int l, int a)
{
    if (!programado || !dentro(ox, oy, l, a) || !dentro(dx, dy, l, a)) return false;
    if (!r600_copiar_ret(&geo, &saida, ox, oy, dx, dy, l, a)) return false;
    submeter();
    return true;
}

static bool r6_preencher(int x, int y, int l, int a, uint32_t cor)
{
    if (!programado || !dentro(x, y, l, a)) return false;
    if (cor != cor_atual || (uint32_t)l > cor_largura) {
        /* La fila del color cambia: antes, que nadie la esté leyendo. */
        if (!r6_esperar()) return false;
        uint32_t ini = cor == cor_atual ? cor_largura : 0;
        for (uint32_t i = ini; i < (uint32_t)l; i++) linha_cor[i] = cor;
        cor_atual = cor;
        cor_largura = (uint32_t)l;
    }
    if (!r600_preencher_ret(&geo, &saida, mc_linha_cor, x, y, l, a)) return false;
    submeter();   /* vacía el HDP: la fila del color llega a la VRAM antes que el paquete */
    return true;
}

static void r6_desligar(void)
{
    if (!programado || !mmio) return;
    programado = false;
    wr(DMA_RB_CNTL, rd(DMA_RB_CNTL) & ~DMA_RB_ENABLE);
    /* Lo que estuviera copiando termina enseguida; se espera un poco (sin
     * reiniciar el bloque: la CPU redibuja la pantalla después). */
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
    if (gen >= 7) wr(DMA_MODE, o_mode);
    wr(DMA_RB_BASE, o_rb_base);
    wr(DMA_RB_RPTR, 0);
    wr(DMA_RB_WPTR, 0);
    wr(DMA_RB_CNTL, o_rb_cntl & ~DMA_RB_ENABLE);
    klog("[ACEL2D] r600: anillo del DMA apagado y sus registros como estaban\n");
}

static uint32_t ler_bar(const pci_disp_t *d, uint8_t reg, bool *alto)
{
    uint32_t v = pci_le32(d, reg);
    *alto = (v & 1) == 0 && ((v >> 1) & 3) == 2 && pci_le32(d, (uint8_t)(reg + 4)) != 0;
    return v;
}

/* ¿Tiene la capacidad AGP (id 2)? Entonces el HDP se vacía por su registro. */
static bool tem_agp(const pci_disp_t *d)
{
    if (!(pci_le32(d, 0x04) & (1u << 20))) return false;
    uint8_t c = (uint8_t)(pci_le32(d, 0x34) & 0xFC);
    for (int n = 0; c && n < 48; n++) {
        uint32_t v = pci_le32(d, c);
        if ((v & 0xFF) == 2) return true;
        c = (uint8_t)((v >> 8) & 0xFC);
    }
    return false;
}

static bool r6_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *m, int nm)
{
    programado = false;
    const r600_modelo_t *mod = modelo_de(d->modelo);
    if (!mod || mod->gen != geracao) {
        snformatar(m, (size_t)nm, "el modelo %04x no esta en la lista exacta de R6xx/R7xx: no se toca", d->modelo);
        return false;
    }
    gen = mod->gen;
    klog("[ACEL2D] r600: %s (%04x), %s, pantalla %x, pitch %u, %ux%u, %u KB del VBE\n", mod->nome, d->modelo,
         gen >= 7 ? "R7xx" : "R6xx", t->fb_fisico, t->pitch, t->larg, t->alt, t->memoria >> 10);
    if (gen < 7 && !acel2d_forcado()) {
        snformatar(m, (size_t)nm, "%s es R6xx: su DMA no es fiable en todas (Linux no lo usa); solo con --acel2d=radeon",
                   mod->nome);
        return false;
    }
    /* El DMA solo toca la VRAM de la tarjeta: no hace falta el bus maestro. */
    uint32_t cmd = pci_le32(d, 0x04);
    if (!(cmd & 0x02)) pci_escreve32(d, 0x04, (cmd & 0xFFFFu) | 0x02);
    bool alto0, alto2;
    uint32_t bar0 = ler_bar(d, 0x10, &alto0), bar2 = ler_bar(d, 0x18, &alto2);
    agp = tem_agp(d);
    klog("[ACEL2D] r600: BAR0 %x, BAR2 %x%s\n", bar0, bar2, agp ? ", AGP" : "");
    if ((bar0 & 1) || alto0) { snformatar(m, (size_t)nm, "BAR 0 no es memoria por debajo de 4 GB (%x)", bar0); return false; }
    if ((bar2 & 1) || alto2 || !(bar2 & ~0xFu)) { snformatar(m, (size_t)nm, "BAR 2 no es MMIO por debajo de 4 GB (%x)", bar2); return false; }
    bar0 &= ~0xFu;
    if (t->fb_fisico < bar0) { snformatar(m, (size_t)nm, "la pantalla (%x) no esta en el BAR 0 (%x)", t->fb_fisico, bar0); return false; }
    if (t->pitch & 3 || t->pitch < (uint32_t)t->larg * 4) { snformatar(m, (size_t)nm, "pitch %u no vale para %u pixeles", t->pitch, t->larg); return false; }
    if (!mmio || mmio_fis != (bar2 & ~0xFu)) {
        mmio_fis = bar2 & ~0xFu;
        mmio = (volatile uint8_t *)janela_mapear_tipo(mmio_fis, MMIO_TAM, PG_PCD | PG_PWT);
    }
    if (!mmio) { snformatar(m, (size_t)nm, "sin ventana para el MMIO"); return false; }
    uint32_t memsize = rd(CONFIG_MEMSIZE), aper = rd(CONFIG_APER_SIZE);
    if (memsize == 0xFFFFFFFFu) { snformatar(m, (size_t)nm, "el MMIO no responde (CONFIG_MEMSIZE %x)", memsize); return false; }

    /* La apertura de la CPU en el MC: dónde está la pantalla para la GPU. */
    uint32_t hdp = rd(HDP_NONSURFACE_BASE);
    uint32_t desloc = t->fb_fisico - bar0;
    uint64_t mc64 = ((uint64_t)hdp << 8) + desloc;
    uint32_t fbloc = rd(gen >= 7 ? R700_MC_VM_FB_LOCATION : R600_MC_VM_FB_LOCATION);
    uint64_t vram_ini = (uint64_t)(fbloc & 0xFFFFu) << 24, vram_fim = ((uint64_t)(fbloc >> 16) << 24) | 0xFFFFFFu;
    uint32_t vm0 = rd(VM_CONTEXT0_CNTL);
    klog("[ACEL2D] r600: CONFIG_MEMSIZE %u MB, CONFIG_APER_SIZE %u MB, HDP_NONSURFACE_BASE %x, MC_VM_FB_LOCATION %x, "
         "VM_CONTEXT0_CNTL %x\n", memsize >> 20, aper >> 20, hdp, fbloc, vm0);
    if (mc64 >> 32) { snformatar(m, (size_t)nm, "la pantalla en el MC pasa de 4 GB (HDP %x)", hdp); return false; }
    uint32_t mc_tela = (uint32_t)mc64;

    /* El controlador de pantalla que muestra el framebuffer del VBE. */
    int ctrl_ok = -1;
    for (int c = 0; c < 2; c++) {
        uint32_t o = c ? D2_DESLOC : 0;
        uint32_t crtc = rd(D1CRTC_CONTROL + o), en = rd(D1GRPH_ENABLE + o), gc = rd(D1GRPH_CONTROL + o);
        uint32_t sup = rd(D1GRPH_PRIMARY_SURFACE + o), pit = rd(D1GRPH_PITCH + o);
        uint32_t ox = rd(D1GRPH_SURFACE_OFF_X + o), oy = rd(D1GRPH_SURFACE_OFF_Y + o);
        uint32_t hi = gen >= 7 ? rd(c ? D2GRPH_SURFACE_HIGH : D1GRPH_SURFACE_HIGH) : 0;
        klog("[ACEL2D] r600: D%d %s, GRPH %s, control %x, superficie %x (alta %x), pitch %u, desplazamiento %u,%u\n",
             c + 1, crtc & 1 ? "encendido" : "apagado", en & 1 ? "activo" : "apagado", gc, sup, hi, pit, ox, oy);
        if ((crtc & 1) && (en & 1) && (gc & 3) == 2 && !((gc >> 8) & 7) && ((gc >> 20) & 0xF) <= 1 && pit == t->pitch / 4 && !ox && !oy &&
            !hi && sup == mc_tela && ctrl_ok < 0)
            ctrl_ok = c;
    }
    if (ctrl_ok < 0) {
        snformatar(m, (size_t)nm, "ningun controlador D1/D2 muestra la pantalla del VBE (32 bits, lineal, pitch %u, MC %x)",
                   t->pitch / 4, mc_tela);
        return false;
    }

    /* Lo que va a tocar el DMA, detrás de la pantalla y de la zona de prueba. */
    uint32_t zona_fim = t->pitch * (uint32_t)(t->alt + ACEL2D_ZONA_A);
    uint32_t area_off = (zona_fim + 0xFFFFu) & ~0xFFFFu;
    uint32_t fixo = area_off + RING_BYTES + CTRL_BYTES + t->pitch;   /* hasta el final de la fila del color */
    uint32_t mem = t->memoria;
    /* CONFIG_MEMSIZE va en bytes en las R6xx/R7xx; si no lo parece, solo cuenta el VBE. */
    if (memsize >= (16u << 20) && memsize < mem) mem = memsize;
    int filas = mem > fixo ? (int)((mem - fixo) / t->pitch) : 0;
    if (filas > t->alt) filas = t->alt;
    if (filas < MIN_FILAS_PASO) {
        snformatar(m, (size_t)nm, "no cabe el anillo y la zona de paso en la memoria del VBE (%u KB)", mem >> 10);
        return false;
    }
    uint32_t total = fixo + (uint32_t)filas * t->pitch;
    if ((uint64_t)mc_tela < vram_ini || (uint64_t)mc_tela + total - 1 > vram_fim || ((uint64_t)mc_tela + total) >> 32) {
        snformatar(m, (size_t)nm, "la zona del DMA (MC %x, %u KB) no cae en la VRAM (MC_VM_FB_LOCATION %x)", mc_tela,
                   total >> 10, fbloc);
        return false;
    }
    /* D246: la base del anillo se escribe >> 8 (256 bytes) y la CPU llega a la zona por el BAR 0: tiene
     * que caber en su apertura (CONFIG_APER_SIZE; si el registro no dice nada, no se comprueba). */
    if ((mc_tela + area_off) & 0xFFu) {
        snformatar(m, (size_t)nm, "el anillo (MC %x) no esta alineado a 256 bytes", mc_tela + area_off);
        return false;
    }
    if (aper >= (16u << 20) && aper != 0xFFFFFFFFu && desloc + total > aper) {
        snformatar(m, (size_t)nm, "la zona del DMA (%u KB desde la pantalla) pasa de la apertura de la CPU (%u MB)",
                   total >> 10, aper >> 20);
        return false;
    }
    /* Un cursor por hardware encendido (64x64, 16 KB) no puede tener su imagen
     * en la zona de prueba ni donde va el anillo. */
    for (int c = 0; c < 2; c++) {
        uint32_t cc = rd(D1CUR_CONTROL + (c ? D2_DESLOC : 0)), cs = rd(D1CUR_SURFACE + (c ? D2_DESLOC : 0));
        if ((cc & 1) && cs + 0x4000u > mc_tela + t->pitch * (uint32_t)t->alt && cs < mc_tela + total) {
            snformatar(m, (size_t)nm, "el cursor por hardware de D%d (MC %x) esta donde iria el anillo", c + 1, cs);
            return false;
        }
    }
    if (vm0 & 1) {
        snformatar(m, (size_t)nm, "la memoria virtual de la GPU esta encendida (VM_CONTEXT0_CNTL %x): no se arriesga", vm0);
        return false;
    }
    uint32_t rb = rd(DMA_RB_CNTL), st = rd(DMA_STATUS_REG);
    klog("[ACEL2D] r600: DMA_RB_CNTL %x, DMA_STATUS %x, DMA_CNTL %x, SRBM_STATUS %x\n", rb, st, rd(DMA_CNTL), rd(SRBM_STATUS));
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
    mc_ring = mc_tela + area_off;
    mc_ctrl = mc_ring + RING_BYTES;
    mc_linha_cor = mc_ctrl + CTRL_BYTES;
    geo.mc_tela = mc_tela;
    geo.pitch = t->pitch;
    geo.mc_rascunho = mc_linha_cor + t->pitch;
    geo.filas_rascunho = filas;
    larg = t->larg;
    alt_util = t->alt + ACEL2D_ZONA_A;
    cor_atual = 0; cor_largura = 0;
    seq = 0x52700000u;
    ctrl[CTRL_VALLA] = seq;
    ctrl[CTRL_TESTE] = 0;
    klog("[ACEL2D] r600: controlador D%d, pantalla en el MC %x, anillo en %x, zona de paso de %d filas en %x (%u KB)\n",
         ctrl_ok + 1, mc_tela, mc_ring, filas, geo.mc_rascunho, total >> 10);

    /* Encender el anillo (como r600_dma_resume, sin escritura de vuelta ni IB). */
    o_rb_cntl = rb; o_rb_base = rd(DMA_RB_BASE); o_ib_cntl = rd(DMA_IB_CNTL); o_cntl = rd(DMA_CNTL);
    o_mode = gen >= 7 ? rd(DMA_MODE) : 0;
    o_rptr_hi = rd(DMA_RB_RPTR_ADDR_HI); o_rptr_lo = rd(DMA_RB_RPTR_ADDR_LO);
    o_sem1 = rd(DMA_SEM_INCOMPLETE_TIMER_CNTL); o_sem2 = rd(DMA_SEM_WAIT_FAIL_TIMER_CNTL);
    programado = true;
    pendente = false;
    wptr = 0;
    /* Solo el tamaño: sin escritura de vuelta ni cambio de orden de bytes (el PC es little endian). */
    uint32_t cntl = DMA_RB_TAM(RING_L2);
    /* Como r600_dma_resume (Linux 6.6): temporizadores de semáforos a 0, tamaño, punteros, dirección de
     * vuelta, base, IB habilitado (no se usa), DMA_CNTL sin la interrupción de contexto vacío, DMA_MODE,
     * puntero de escritura y, por último, el bit de encendido. */
    wr(DMA_SEM_INCOMPLETE_TIMER_CNTL, 0);
    wr(DMA_SEM_WAIT_FAIL_TIMER_CNTL, 0);
    wr(DMA_RB_CNTL, cntl);
    wr(DMA_RB_RPTR, 0);
    wr(DMA_RB_WPTR, 0);
    wr(DMA_RB_RPTR_ADDR_HI, 0);
    wr(DMA_RB_RPTR_ADDR_LO, mc_ctrl + CTRL_RPTR * 4);
    wr(DMA_RB_BASE, mc_ring >> 8);
    wr(DMA_IB_CNTL, DMA_IB_ENABLE);
    wr(DMA_CNTL, o_cntl & ~(DMA_INTERRUPCOES | DMA_TROCA_BYTES));
    if (gen >= 7) wr(DMA_MODE, 1);
    wr(DMA_RB_WPTR, 0);
    hdp_vaciar();
    wr(DMA_RB_CNTL, cntl | DMA_RB_ENABLE);

    /* Prueba del anillo: escribir una palabra y leerla. */
    uint32_t p[4];
    r600_pkt_escrever(p, mc_ctrl + CTRL_TESTE * 4, 0xCAFEDEADu);
    if (!pacote(p, 4)) { snformatar(m, (size_t)nm, "el anillo del DMA no avanza"); return false; }
    submeter();
    acel_prazo_t pz;
    acel_prazo(&pz, 250);
    while (ctrl[CTRL_TESTE] != 0xCAFEDEADu)
        if (acel_prazo_esgotado(&pz)) {
            diagnostico("prueba del anillo");
            snformatar(m, (size_t)nm, "el DMA no escribe la prueba del anillo (RPTR %x, STATUS %x)", rd(DMA_RB_RPTR),
                       rd(DMA_STATUS_REG));
            return false;
        }
    if (!r6_esperar()) { snformatar(m, (size_t)nm, "el DMA no escribe la valla"); return false; }
    klog("[ACEL2D] r600: anillo del DMA en marcha (prueba y valla escritas, RPTR %x)\n", rd(DMA_RB_RPTR));
    return true;
}

const acel_driver_t acel_r600 = {
    "r600dma", ACEL2D_RADEON, r6_iniciar, r6_copiar, r6_preencher, r6_esperar, NULL, r6_desligar,
    true,    /* la zona de prueba va debajo de la pantalla visible */
    4096,    /* D246: menos de 64x64 píxeles lo hace la CPU (sin medida en hardware; «acel2d probar --detalle» da los tiempos) */
    true,    /* D246: el DMA sigue copiando tras copiar() */
    r6_detalle,
};
