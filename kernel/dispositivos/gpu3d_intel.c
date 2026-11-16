// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d_intel.c - 3D por la GPU en Intel Gen3: 915, 945, G33 y Pineview (D285, D358).
 *
 * SIN PROBAR EN HARDWARE: QEMU no emula ninguna GPU de Intel. La capa común
 * (kernel/gui/gpu3d.c) solo lo usa si su autoprueba da los píxeles esperados y
 * a tiempo; si no, SaguiGL sigue por software. Las órdenes las construye
 * gen3_pacotes.h, que sí se prueba en el anfitrión (tests/gen3_3d_unidad.c), y
 * la lógica del GTT, gen3_gtt.h (tests/gen3_gtt_unidad.c).
 *
 * Qué usa:
 *   - el ring de órdenes que ya pone en marcha el blitter 2D (acel_intel.c): en
 *     Gen3 es el mismo para 2D y 3D. Si el 2D quedó en la CPU por ser más lento,
 *     el 3D enciende el ring él mismo; si el 2D falló, no se toca (salvo
 *     --gpu3d=intel);
 *   - memoria: la que el VBE dejó en el GTT (la «robada» que anuncia), entre la
 *     zona de prueba del 2D y el ring. Ahí van el lote, la zona de pruebas de
 *     las etapas, el Z y, si no hay RAM en el GTT, el destino y las texturas;
 *   - D358, solo si se pide («gpu3d encender gtt», --gpu3d-gtt): RAM normal del
 *     sistema en el GTT (8 o 16 MB) para el destino de color (traer el cuadro
 *     es leer RAM con caché, no la apertura sin caché) y, si el chipset tiene
 *     encendida la página de vaciado, las texturas. Es lo único que escribe
 *     fuera del ring: las entradas del GTT de una ventana que nadie usa
 *     (comprobado leyendo antes). Una vez escritas se quedan, con sus páginas,
 *     hasta reiniciar: nunca se liberan, para que una GPU colgada o una entrada
 *     que la GPU aún recuerde no puedan escribir en memoria que el kernel haya
 *     dado a otro;
 *   - órdenes: por lotes (MI_BATCH_BUFFER_START con dirección del GTT y sin
 *     privilegios, a un búfer de la zona; lo normal) o escritas en el propio ring
 *     («gpu3d encender anillo»). Si el lote termina pero no hace lo que lleva, se
 *     sigue en el anillo (etapa 5);
 *   - cada entrega lleva el estado completo y empieza vaciando lo pintado e
 *     invalidando las cachés de lectura de la GPU (texturas recargadas en el
 *     mismo sitio);
 *   - limpiar el color y el Z: con el blitter (XY_COLOR_BLT), que es lo que ya
 *     prueba el 2D;
 *   - vallas: el número de entrega; una valla está cumplida cuando HEAD alcanza
 *     a TAIL (cada entrega acaba en MI_FLUSH). Tras cada espera se mira EIR: si
 *     la GPU marcó una orden no válida o un fallo de la tabla de páginas, el 3D
 *     se apaga aunque el ring haya quedado vacío.
 *
 * Encendido por etapas (líneas «[GPU3D] intel: etapa N» en el log): 1 la sonda
 * (solo lectura), 2 el ring, 3 la zona de la memoria del VBE, 4 el blitter
 * escribe en ella, 5 el lote se ejecuta, 6 la RAM en el GTT (si se pidió) y
 * 7 el estado 3D y un triángulo. La última que aparece dice dónde se paró.
 *
 * Qué hay que comprobar en el hardware: docs/gpu3d.md. */
#include "gpu3d.h"
#include "acel_intel.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "paginacao.h"
#include "texto.h"
#include "gen3_pacotes.h"
#include "gen3_gtt.h"

#define LOTE_BYTES      (64 * 1024)
#define LOTE_PAL        (LOTE_BYTES / 4 - 8)
#define ANEL_PAL        6000           /* órdenes en el anillo: menos de la mitad del ring (64 KB) */
#define ARENA_MAX       (24u * 1024 * 1024)
#define TESTE_BYTES     8192           /* zona de las etapas: color 16x16 (paso 64) y, a 4 KB, Z de 16 bits */
#define TESTE_PASSO     64
#define RAM_TESTE       8192           /* las dos últimas páginas de la RAM en el GTT: pruebas de la etapa 6 */

/* Registros que solo se leen (diagnóstico). */
#define R_PGTBL_CTL     0x2020
#define R_PGTBL_ER      0x2024
#define R_IPEIR         0x2088
#define R_IPEHR         0x208C
#define R_INSTDONE      0x2090
#define R_EIR           0x20B0
#define R_ESR           0x20B8
#define R_INSTPM        0x20C0
#define R_ACTHD         0x20C8
#define EIR_GRAVES      0x11u          /* bit 0: orden no válida; bit 4: tabla de páginas */

/* Solo estos modelos (los de Gen3 de la tabla del 2D). */
static const uint16_t modelos[] = { 0x2582, 0x2592, 0x2772, 0x27A2, 0x27AE, 0x29B2, 0x29C2, 0x29D2, 0xA001, 0xA011 };

static g3d_arena_t arena, arena_ram;
static g3d_buffer_t b_lote, b_teste, b_cor, b_z, b_tex[GPU3D_TEXTURAS];
static g3_alvo_t alvo;
static uint32_t *prep;                 /* la cola se prepara en RAM */
static uint32_t arena_fis;
static uint8_t *arena_cpu;
static uint32_t arena_mapeada;
static bool pronto, por_anel, anel_meu;
static g3d_vala_t vala_emitida;
static pci_disp_t placa;
static acel_tela_t tela;
static uint32_t eir_base;              /* EIR al encender: solo cuentan los bits nuevos */
static uint32_t gmadr;                 /* la apertura (física) */

/* D358: RAM del sistema en el GTT. Se prepara una vez por arranque y se queda. */
static struct {
    bool     pronta;                   /* entradas escritas y comprobadas por la CPU */
    bool     vetada;                   /* falló: no se vuelve a intentar hasta reiniciar */
    bool     tex;                      /* texturas en la RAM (la GPU lee bien lo que escribe la CPU) */
    uint32_t gfx, bytes;               /* ventana gráfica y tamaño (con las dos páginas de prueba al final) */
    uint8_t *cpu;                      /* ventana del kernel sobre esas páginas (con caché) */
    uint32_t fis0;                     /* la primera página (para el log) */
    uint32_t antes;                    /* lo que había en las entradas (todas iguales) */
    volatile uint32_t *pte;            /* las entradas del GTT de la ventana (sin caché) */
    volatile uint32_t *ifp;            /* página de vaciado del chipset (NULL: no hay) */
    uint32_t sonda;                    /* página de la ventana del kernel para mirar la apertura y el GTT */
    char     motivo[120];
} ram;
static bool ram_ativa;                 /* en este encendido */
static uint32_t linha_cache = 64;

static bool modelo_ok(uint16_t m)
{
    for (unsigned i = 0; i < sizeof modelos / sizeof modelos[0]; i++)
        if (modelos[i] == m) return true;
    return false;
}

/* ------------------------------------------------------------ caché de la CPU */

static inline void cpuid1(uint32_t *b, uint32_t *d)
{
    uint32_t a = 1, c = 0;
    __asm__ volatile("cpuid" : "+a"(a), "=b"(*b), "+c"(c), "=d"(*d));
}

/* La GPU de Gen3 no ve la caché de la CPU: lo que escribe la CPU en la RAM del
 * GTT se vacía a la memoria, y lo que escribió la GPU se lee tras invalidar.
 * Solo se llama con la RAM en el GTT, que exige CLFLUSH y SSE2 (mfence). */
static void cache_vaciar(const void *p, uint32_t n)
{
    uint32_t a = (uint32_t)p & ~(linha_cache - 1), fim = (uint32_t)p + n;
    __asm__ volatile("mfence" ::: "memory");
    for (; a < fim; a += linha_cache) __asm__ volatile("clflush (%0)" :: "r"(a) : "memory");
    __asm__ volatile("mfence" ::: "memory");
}

/* Lo escrito por la CPU tiene que haber salido del chipset antes de que lo lea la GPU. */
static void chipset_vaciar(void)
{
    __asm__ volatile("mfence" ::: "memory");
    /* Solo se escribe, como i9xx_chipset_flush() de intel-gtt.c: leer esa dirección no está
     * documentado y no hace falta. */
    if (ram.ifp) *ram.ifp = 1;
}

/* ------------------------------------------------------------ diagnóstico */

static void i3_detalle(const char *fase)
{
    intel_anel_t a;
    if (!intel_anel_info(&a)) { klog("[GPU3D] intel %s: el ring no esta en marcha\n", fase); return; }
    klog("[GPU3D] intel %s: CTL %x HEAD %x TAIL %x; IPEIR %x IPEHR %x INSTDONE %x ACTHD %x\n", fase, a.ctl, a.head, a.tail,
         intel_anel_reg(R_IPEIR), intel_anel_reg(R_IPEHR), intel_anel_reg(R_INSTDONE), intel_anel_reg(R_ACTHD));
    klog("[GPU3D] intel %s: EIR %x (al encender %x) ESR %x INSTPM %x PGTBL_CTL %x PGTBL_ER %x; vallas emitidas %u; %s\n", fase,
         intel_anel_reg(R_EIR), eir_base, intel_anel_reg(R_ESR), intel_anel_reg(R_INSTPM), intel_anel_reg(R_PGTBL_CTL),
         intel_anel_reg(R_PGTBL_ER), vala_emitida, por_anel ? "ordenes en el anillo" : "lotes");
    klog("[GPU3D] intel %s: zona %x+%u KB (libres %u KB); lote %x; color %x (%dx%d, paso %u%s); Z%d %x (paso %u%s)\n", fase,
         (uint32_t)arena.gpu, (uint32_t)(arena.bytes >> 10), (uint32_t)(g3d_arena_livre(&arena) >> 10), (uint32_t)b_lote.gpu,
         alvo.cor_gpu, alvo.larg, alvo.alt, alvo.cor_passo, ram_ativa && b_cor.usado && b_cor.gpu >= ram.gfx ? ", RAM" : "",
         alvo.zbits, alvo.z_gpu, alvo.z_passo, ram_ativa && b_z.usado && b_z.gpu >= ram.gfx ? ", RAM" : "");
    if (ram.pronta || ram.motivo[0])
        klog("[GPU3D] intel %s: RAM en el GTT: %s; ventana %x+%u KB (libres %u KB), texturas %s%s%s\n", fase,
             ram_ativa ? "en uso" : ram.pronta ? "preparada, sin usar" : "no", ram.gfx, ram.bytes >> 10,
             ram_ativa ? (uint32_t)(g3d_arena_livre(&arena_ram) >> 10) : 0, ram.tex ? "en la RAM" : "en la memoria del VBE",
             ram.motivo[0] ? "; " : "", ram.motivo);
}

/* Bits graves nuevos de EIR (orden no válida, tabla de páginas). true: ninguno. */
static bool eir_bem(const char *onde, char *m, int nm)
{
    uint32_t e = intel_anel_reg(R_EIR);
    if (!(e & ~eir_base & EIR_GRAVES)) return true;
    snformatar(m, (size_t)nm, "la GPU marco un error %s (EIR %x:%s%s; IPEHR %x, PGTBL_ER %x)", onde, e,
               e & ~eir_base & 1 ? " orden no valida" : "", e & ~eir_base & 0x10 ? " tabla de paginas" : "",
               intel_anel_reg(R_IPEHR), intel_anel_reg(R_PGTBL_ER));
    klog("[GPU3D] intel: %s\n", m);
    return false;
}

/* ------------------------------------------------------------ apagar */

static void i3_desligar(void)
{
    if (!pronto && !anel_meu) return;
    pronto = false;
    ram_ativa = false;   /* la ventana y sus páginas se quedan (D358): solo se dejan de usar */
    if (anel_meu) {
        /* El ring lo encendió el 3D: se para (acel_intel avisa a la capa; ya no hay nada que apagar). */
        anel_meu = false;
        if (acel_intel.desligar) acel_intel.desligar();
    }
    if (prep) { kfree(prep); prep = NULL; }
    klog("[GPU3D] intel: 3D apagado\n");
}

/* ------------------------------------------------------------ entregar órdenes */

static int i3_esperar(g3d_vala_t vala, uint32_t ms)
{
    (void)vala;   /* una sola cola: cumplida cuando el ring queda vacío */
    if (!pronto) return G3D_FALHA;
    if (!intel_anel_esperar(ms)) { i3_detalle("tiempo agotado"); return G3D_FALHA; }
    char m[160];
    if (!eir_bem("tras una entrega", m, sizeof m)) { i3_detalle("error de la GPU"); return G3D_FALHA; }
    return G3D_OK;
}

/* Entrega lo preparado en «prep» (n palabras): por lote o en el anillo. */
static int entregar(uint32_t n, g3d_vala_t *vala)
{
    if (!por_anel) {
        /* El búfer del lote se reutiliza: la entrega anterior tiene que haber terminado. */
        if (i3_esperar(vala_emitida, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
        prep[n++] = G3_MI_LOTE_FIM;
        if (n & 1) prep[n++] = G3_MI_NOOP;
        uint32_t *dst = (uint32_t *)b_lote.cpu;
        for (uint32_t i = 0; i < n; i++) dst[i] = prep[i];
        __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    }
    bool sim = int_salva_desliga();
    bool ok;
    if (!por_anel) {
        ok = intel_anel_reservar(2 + INTEL_ANEL_PAL_ENVIO);
        if (ok) {
            intel_anel_emitir(G3_MI_LOTE_INICIO | G3_LOTE_GTT);
            intel_anel_emitir((uint32_t)b_lote.gpu | G3_LOTE_NAO_SEGURO);
        }
    } else {
        ok = intel_anel_reservar(n + INTEL_ANEL_PAL_ENVIO);
        if (ok) for (uint32_t i = 0; i < n; i++) intel_anel_emitir(prep[i]);
    }
    if (ok) intel_anel_enviar(true);
    int_restaura(sim);
    if (!ok) { i3_detalle("sin sitio en el ring"); return G3D_FALHA; }
    vala_emitida++;
    if (!vala_emitida) vala_emitida = 1;
    if (vala) *vala = vala_emitida;
    return G3D_OK;
}

/* Órdenes del blitter por el ring (como el 2D), con su vaciado; espera a que terminen. */
static bool anel_blt(uint32_t n, uint32_t ms)
{
    bool sim = int_salva_desliga();
    bool ok = intel_anel_reservar(n + INTEL_ANEL_PAL_ENVIO);
    if (ok) { for (uint32_t i = 0; i < n; i++) intel_anel_emitir(prep[i]); intel_anel_enviar(true); }
    int_restaura(sim);
    if (!ok) return false;
    vala_emitida++;
    return intel_anel_esperar(ms);
}

/* ------------------------------------------------------------ memoria: robada y RAM en el GTT */

static g3d_arena_t *arena_de(const g3d_buffer_t *b)
{
    return ram_ativa && b->gpu >= arena_ram.gpu && b->gpu < arena_ram.gpu + arena_ram.bytes ? &arena_ram : &arena;
}

static bool na_ram(const g3d_buffer_t *b) { return b->usado && arena_de(b) == &arena_ram; }

static void soltar(g3d_buffer_t *b) { g3d_buffer_soltar(arena_de(b), b); }

#define SO_VBE       0   /* solo la memoria del VBE */
#define RAM_PRIMEIRO 1   /* la RAM del GTT si la hay; si no cabe, la del VBE */
#define VBE_PRIMEIRO 2   /* la del VBE; si no cabe, la RAM del GTT */
static bool criar(g3d_buffer_t *b, uint8_t tipo, uint64_t bytes, int onde)
{
    if (ram_ativa && onde == RAM_PRIMEIRO && g3d_buffer_criar(&arena_ram, b, tipo, bytes, 4096)) return true;
    if (g3d_buffer_criar(&arena, b, tipo, bytes, 4096)) return true;
    return ram_ativa && onde == VBE_PRIMEIRO && g3d_buffer_criar(&arena_ram, b, tipo, bytes, 4096);
}

/* Lee una palabra física sin caché por la página de sonda (el GTT o la apertura). */
static uint32_t sonda_ler(uint32_t fis)
{
    janela_pagina(ram.sonda, fis & ~0xFFFu, PG_PCD | PG_PWT);
    uint32_t v = *(volatile uint32_t *)(ram.sonda + (fis & 0xFFCu));
    janela_soltar(ram.sonda);
    return v;
}

/* No se usa la RAM en el GTT (el motivo ya está en ram.motivo). Si había entradas
 * escritas, vuelven a lo que tenían y las páginas se quedan sin liberar hasta
 * reiniciar. No se repite hasta reiniciar. */
static int ram_vetar(int r)
{
    bool escritas = ram.pronta;
    if (escritas) {
        uint32_t n = ram.bytes / PAGINA;
        for (uint32_t i = 0; i < n; i++) ram.pte[i] = ram.antes;
        (void)ram.pte[n - 1];
        chipset_vaciar();
    }
    ram.pronta = false; ram.vetada = true; ram.tex = false;
    klog("[GPU3D] intel: etapa 6: RAM en el GTT no: %s; se sigue con la memoria del VBE%s\n", ram.motivo,
         escritas ? " (las entradas vuelven a lo que tenian; sus paginas quedan reservadas hasta reiniciar)" : "");
    return r;
}

#define VETAR(...) do { snformatar(ram.motivo, sizeof ram.motivo, __VA_ARGS__); return ram_vetar(G3D_NAO); } while (0)

/* La GPU no terminó una prueba de la etapa 6: no se vuelve a intentar hasta reiniciar.
 * Las entradas NO se devuelven: la orden colgada aún puede escribir por ellas, y sus
 * páginas (reservadas para siempre) son el sitio seguro donde hacerlo. */
static int ram_vetar_colgada(const char *que)
{
    snformatar(ram.motivo, sizeof ram.motivo, "la GPU no termina %s (las entradas se quedan en sus paginas)", que);
    ram.vetada = true; ram.tex = false;
    klog("[GPU3D] intel: etapa 6: RAM en el GTT no: %s\n", ram.motivo);
    return G3D_FALHA;
}

/* Etapa 6a-6b: elegir la ventana, comprobar que está libre, escribir las entradas
 * y que la CPU vea por la apertura lo que hay en cada página. Solo la primera vez. */
static int ram_montar(const intel_anel_t *a)
{
    uint32_t b, d;
    cpuid1(&b, &d);
    if (!(d & (1u << 19)) || !(d & (1u << 26))) VETAR("la CPU no tiene CLFLUSH o SSE2 (cpuid 1: edx %x)", d);
    if ((b >> 8) & 0xFF) linha_cache = ((b >> 8) & 0xFF) * 8;
    bool g33 = g3_gtt_tipo_g33(placa.modelo);
    pci_disp_t ponte;
    memset(&ponte, 0, sizeof ponte);
    uint32_t id = pci_le32(&ponte, 0x00);
    if ((id & 0xFFFF) != 0x8086) VETAR("el puente 0:0.0 no es de Intel (%x)", id);
    uint16_t gmch = (uint16_t)(pci_le32(&ponte, G3_GMCH_CTRL & ~3) >> 16);
    uint32_t robada = g3_gtt_robada(gmch, g33);
    uint32_t gttadr = pci_le32(&placa, G3_PCI_GTTADR);
    uint32_t ifp = pci_le32(&ponte, g33 ? G3_IFP_G33 : G3_IFP_915);
    uint32_t ifp_alto = g33 ? pci_le32(&ponte, G3_IFP_G33 + 4) : 0;
    uint32_t livres = memfisica_livres();
    uint32_t bytes = g3_gtt_tamanho(livres);
    /* Lo que usa el VBE en direcciones gráficas termina en fb_gfx + memoria (ahí acaba el ring del
     * 2D, acel_intel.c), no en «memoria»: la pantalla no tiene por qué empezar en la dirección 0. */
    uint32_t vbe_fim = a->fb_gfx + tela.memoria < a->fb_gfx ? 0xFFFFFFFFu : a->fb_gfx + tela.memoria;
    uint32_t gfx = g3_gtt_janela(robada, vbe_fim, bytes);
    klog("[GPU3D] intel: etapa 6: puente %x, GMCH_CTRL %x (robada %u KB), VBE %u KB, GTTADR %x, PGTBL_CTL %x, pagina de vaciado %x%s; "
         "%u paginas libres -> ventana de %u KB en %x\n", id, gmch, robada == 0xFFFFFFFFu ? 0 : robada >> 10, vbe_fim >> 10,
         gttadr, intel_anel_reg(R_PGTBL_CTL), ifp, ifp_alto ? " (por encima de 4 GB: no se usa)" : "", livres, bytes >> 10, gfx);
    if (robada == 0xFFFFFFFFu) VETAR("codigo de memoria robada desconocido (GMCH_CTRL %x)", gmch);
    if (!bytes) VETAR("poca memoria libre (%u paginas)", livres);
    if (!gfx) VETAR("no cabe una ventana por debajo de 128 MB (robada %u KB, VBE hasta %u KB)", robada >> 10, vbe_fim >> 10);
    if (!(gttadr & ~0xFFFu) || (gttadr & 1) || ((gttadr >> 1) & 3) == 2) VETAR("GTTADR %x no es un BAR de memoria de 32 bits", gttadr);
    gttadr &= ~0xFu;
    if (gttadr >= gmadr && gttadr < gmadr + G3_GTT_LIMITE) VETAR("GTTADR %x dentro de la apertura %x", gttadr, gmadr);
    if (!ram.sonda) ram.sonda = janela_reservar(PAGINA);
    if (!ram.sonda) VETAR("sin ventana del kernel para la sonda");
    /* La entrada de la pantalla tiene que ser válida: si no, GTTADR no es lo que parece. */
    uint32_t idx = a->fb_gfx >> 12;
    uint32_t e_tela = sonda_ler(gttadr + idx * 4);
    klog("[GPU3D] intel: etapa 6: la pantalla (grafica %x) es la entrada %u del GTT: %x\n", a->fb_gfx, idx, e_tela);
    if (!(e_tela & G3_GTT_VALIDA)) VETAR("la entrada de la pantalla no es valida (%x)", e_tela);
    uint32_t n = bytes / PAGINA;
    uint32_t pte_fis = gttadr + (gfx >> 12) * 4;
    uint32_t pte_virt = janela_mapear_tipo(pte_fis & ~0xFFFu, (pte_fis & 0xFFFu) + n * 4, PG_PCD | PG_PWT);
    if (!pte_virt) VETAR("sin ventana del kernel para %u entradas del GTT", n);
    ram.pte = (volatile uint32_t *)(pte_virt + (pte_fis & 0xFFFu));
    uint32_t dif = g3_gtt_livre(ram.pte, n);
    ram.antes = ram.pte[0];
    klog("[GPU3D] intel: etapa 6: entradas %u-%u del GTT: %s (%x)\n", gfx >> 12, (gfx >> 12) + n - 1,
         dif == n ? "todas iguales: libres" : "en uso", ram.antes);
    if (dif != n) VETAR("la ventana del GTT esta en uso (entrada %u = %x)", (gfx >> 12) + dif, ram.pte[dif]);
    /* La página de vaciado, solo si la BIOS la dejó encendida (no se programa). */
    ram.ifp = NULL;
    if ((ifp & 1) && !ifp_alto && (ifp & ~0xFFFu) && !memfisica_sobre_ram(ifp & ~0xFFFu, (ifp & ~0xFFFu) + PAGINA))
        ram.ifp = (volatile uint32_t *)janela_mapear_tipo(ifp & ~0xFFFu, PAGINA, PG_PCD | PG_PWT);
    /* Las páginas: de la memoria baja, de una en una, vistas seguidas por una ventana con caché. */
    uint32_t virt = janela_reservar(bytes);
    if (!virt) VETAR("sin %u KB de ventana del kernel", bytes >> 10);
    uint32_t *fis = kmalloc(n * 4);
    if (!fis) VETAR("sin memoria para la lista de %u paginas", n);
    for (uint32_t i = 0; i < n; i++) {
        fis[i] = memfisica_alocar();
        if (!fis[i]) {
            for (uint32_t k = 0; k < i; k++) memfisica_liberar(fis[k]);
            kfree(fis);
            VETAR("solo %u de %u paginas libres", i, n);
        }
        janela_pagina(virt + i * PAGINA, fis[i], 0);
    }
    ram.cpu = (uint8_t *)virt;
    ram.gfx = gfx; ram.bytes = bytes; ram.fis0 = fis[0];
    /* Cada página lleva su número; todo a la memoria antes de que la mire nadie más. */
    for (uint32_t i = 0; i < n; i++) {
        uint32_t *p = (uint32_t *)(ram.cpu + i * PAGINA);
        memset(p, 0, PAGINA);
        p[0] = 0x5A660000u ^ i;
    }
    cache_vaciar(ram.cpu, bytes);
    /* Desde aquí hay entradas escritas: cualquier fallo las devuelve y deja las páginas sin liberar. */
    ram.pronta = true;
    for (uint32_t i = 0; i < n; i++) ram.pte[i] = g3_gtt_entrada(fis[i]);
    uint32_t mal = n;
    for (uint32_t i = 0; i < n && mal == n; i++) if (ram.pte[i] != g3_gtt_entrada(fis[i])) mal = i;
    chipset_vaciar();
    kfree(fis);
    if (mal != n) VETAR("la entrada %u del GTT no guarda lo escrito (%x)", (gfx >> 12) + mal, ram.pte[mal]);
    for (uint32_t i = 0; i < n; i++) {
        uint32_t v = sonda_ler(gmadr + gfx + i * PAGINA);
        if (v != (0x5A660000u ^ i)) VETAR("la CPU no ve por la apertura la pagina %u de la RAM (%x)", i, v);
    }
    klog("[GPU3D] intel: etapa 6: %u entradas escritas (primera pagina %x); la CPU ve las %u paginas por la apertura; "
         "texturas: %s\n", n, ram.fis0, n, ram.ifp ? "se prueba que la GPU lea la RAM" : "no (la BIOS no encendio la pagina de vaciado)");
    return G3D_OK;
}

/* Etapa 6c-6d: la GPU escribe en la RAM (y solo donde debe) y la lee. En cada encendido. */
static int ram_provar(void)
{
    uint32_t t1 = ram.gfx + ram.bytes - RAM_TESTE, t2 = t1 + PAGINA;
    uint32_t *c1 = (uint32_t *)(ram.cpu + ram.bytes - RAM_TESTE), *c2 = c1 + PAGINA / 4;
    for (uint32_t i = 0; i < PAGINA / 4; i++) { c1[i] = 0xA5A50000u | i; c2[i] = 0x5A5A0000u | i; }
    cache_vaciar(c1, RAM_TESTE);
    g3d_fila_t f;
    g3d_fila_iniciar(&f, prep, 32);
    g3_blt_preencher(&f, t1, TESTE_PASSO, 16, 4, 32, 0x00C0FFEEu);
    if (!anel_blt(f.n, 500)) { i3_detalle("etapa 6"); return ram_vetar_colgada("el relleno en la RAM"); }
    char m[160];
    if (!eir_bem("al escribir en la RAM del GTT", m, sizeof m)) {
        txt_copia_n(ram.motivo, m, sizeof ram.motivo);
        ram_vetar(G3D_FALHA);
        return G3D_FALHA;
    }
    cache_vaciar(c1, RAM_TESTE);
    for (uint32_t i = 0; i < PAGINA / 4; i++) {
        uint32_t esperado = i < 16 * 4 ? 0x00C0FFEEu : 0xA5A50000u | i;
        if (c1[i] != esperado) VETAR("la GPU no escribe bien en la RAM (palabra %u: %x)", i, c1[i]);
        if (c2[i] != (0x5A5A0000u | i)) VETAR("la GPU escribio fuera de su sitio (pagina siguiente, palabra %u: %x)", i, c2[i]);
    }
    klog("[GPU3D] intel: etapa 6: la GPU escribe en la RAM del GTT (y solo donde debe)\n");
    ram.tex = false;
    if (!ram.ifp) return G3D_OK;
    /* La GPU lee la RAM: lo que escribió la CPU, copiado a la zona de pruebas (memoria del VBE). */
    for (uint32_t i = 0; i < 64; i++) c2[i] = 0x01000193u * (i + 7);
    cache_vaciar(c2, PAGINA);
    chipset_vaciar();
    volatile uint32_t *tc = (volatile uint32_t *)b_teste.cpu;
    for (uint32_t i = 0; i < 64; i++) tc[i] = 0;
    g3d_fila_iniciar(&f, prep, 32);
    g3_blt_copiar(&f, (uint32_t)b_teste.gpu, TESTE_PASSO, t2, TESTE_PASSO, 16, 4);
    if (!anel_blt(f.n, 500)) { i3_detalle("etapa 6"); return ram_vetar_colgada("la copia desde la RAM"); }
    for (uint32_t i = 0; i < 64; i++)
        if (tc[i] != 0x01000193u * (i + 7)) {
            klog("[GPU3D] intel: etapa 6: la GPU no lee bien la RAM (palabra %u: %x, se esperaba %x): texturas en la memoria del VBE\n",
                 i, tc[i], 0x01000193u * (i + 7));
            return G3D_OK;
        }
    ram.tex = true;
    klog("[GPU3D] intel: etapa 6: la GPU lee la RAM del GTT: las texturas van alli\n");
    return G3D_OK;
}

/* ------------------------------------------------------------ encender */

static int i3_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, uint32_t variante, char *m, int nm)
{
    pronto = false; ram_ativa = false;
    if (d->fabricante != 0x8086 || geracao != 3 || !modelo_ok(d->modelo)) {
        snformatar(m, (size_t)nm, "%04x:%04x no es una Intel Gen3 de la lista", d->fabricante, d->modelo);
        return G3D_NAO;
    }
    placa = *d; tela = *t;
    por_anel = (variante & GPU3D_CAB_ANEL) != 0;
    bool quer_ram = (variante & GPU3D_CAB_GTT) != 0;
    /* Etapa 1: la sonda (solo lectura del PCI). */
    klog("[GPU3D] intel: etapa 1: %04x:%04x Gen3 (%s); MMIO %x, apertura %x, GTTADR %x; pantalla %x, %u KB del VBE; %s%s\n",
         d->fabricante, d->modelo, g3_gtt_tipo_g33(d->modelo) ? "familia G33" : "familia 915/945", pci_le32(d, 0x10) & ~0xFu,
         pci_le32(d, 0x18) & ~0xFu, pci_le32(d, G3_PCI_GTTADR) & ~0xFu, t->fb_fisico, t->memoria >> 10,
         por_anel ? "ordenes en el anillo" : "lotes", quer_ram ? ", RAM en el GTT pedida" : "");
    /* Etapa 2: el ring. */
    intel_anel_t a;
    if (!intel_anel_info(&a)) {
        /* El 2D no lo tiene encendido (más lento que la CPU, o apagado a mano): lo enciende el 3D. */
        char m2[96] = "";
        klog("[GPU3D] intel: el ring del 2D no esta en marcha: lo enciende el 3D\n");
        uint32_t cmd = pci_le32(d, 0x04);
        if (!acel_intel.iniciar(d, geracao, t, m2, sizeof m2)) {
            pci_escreve32(d, 0x04, cmd & 0xFFFFu);
            snformatar(m, (size_t)nm, "etapa 2: el ring no arranca: %s", m2);
            return G3D_FALHA;
        }
        anel_meu = true;
        if (!intel_anel_info(&a)) { snformatar(m, (size_t)nm, "etapa 2: el ring no da sus datos"); i3_desligar(); return G3D_FALHA; }
    }
    if (a.geracao != 3) { snformatar(m, (size_t)nm, "el ring es de Gen%d", a.geracao); return G3D_NAO; }
    gmadr = a.fb_fisico - a.fb_gfx;
    eir_base = intel_anel_reg(R_EIR);
    vala_emitida = 0;
    {
        /* El ring tiene que estar quieto y avanzar con una orden vacía antes de mandar 3D. */
        bool sim = int_salva_desliga();
        bool ok = intel_anel_reservar(INTEL_ANEL_PAL_ENVIO);
        if (ok) intel_anel_enviar(false);
        int_restaura(sim);
        if (!ok || !intel_anel_esperar(250)) {
            snformatar(m, (size_t)nm, "etapa 2: el ring no avanza con una orden vacia");
            i3_detalle("al iniciar");
            i3_desligar();
            return G3D_FALHA;
        }
    }
    klog("[GPU3D] intel: etapa 2: ring %s en marcha (CTL %x HEAD %x); EIR %x PGTBL_CTL %x INSTPM %x\n", anel_meu ? "propio" : "del 2D",
         a.ctl, a.head, eir_base, intel_anel_reg(R_PGTBL_CTL), intel_anel_reg(R_INSTPM));
    /* Etapa 3: la zona libre de la memoria del VBE, página a página. */
    uint32_t off = (a.arena_off + 0xFFFu) & ~0xFFFu;
    uint32_t fim = (a.arena_off + a.arena_bytes) & ~0xFFFu;
    uint32_t bytes = fim > off ? fim - off : 0;
    if (bytes > ARENA_MAX) bytes = ARENA_MAX;
    if (bytes < LOTE_BYTES + TESTE_BYTES + 512 * 1024) {
        snformatar(m, (size_t)nm, "etapa 3: sin memoria de video libre para el 3D (%u KB tras la pantalla)", bytes >> 10);
        i3_desligar();
        return G3D_NAO;
    }
    if (!arena_cpu || arena_fis != a.fb_fisico + off || arena_mapeada < bytes) {
        arena_cpu = (uint8_t *)janela_mapear_tipo(a.fb_fisico + off, bytes, PG_PCD | PG_PWT);
        arena_fis = a.fb_fisico + off;
        arena_mapeada = bytes;
    }
    if (!arena_cpu) { snformatar(m, (size_t)nm, "etapa 3: sin ventana del kernel para %u KB de memoria de video", bytes >> 10); i3_desligar(); return G3D_NAO; }
    /* La CPU tiene que leer lo que escribe al principio, en medio y al final de la zona (si no,
     * el VBE anunció memoria que no está en el GTT). */
    for (int i = 0; i < 3; i++) {
        uint32_t o = i == 0 ? 0 : i == 1 ? (bytes / 2) & ~0xFFFu : bytes - PAGINA;
        volatile uint32_t *p = (volatile uint32_t *)(arena_cpu + o);
        p[0] = 0x3D5A0000u ^ o; p[1] = ~(0x3D5A0000u ^ o);
        if (p[0] != (0x3D5A0000u ^ o) || p[1] != ~(0x3D5A0000u ^ o)) {
            snformatar(m, (size_t)nm, "etapa 3: la memoria del VBE en %x no guarda lo escrito (%x)", a.fb_fisico + off + o, p[0]);
            i3_desligar();
            return G3D_NAO;
        }
    }
    g3d_arena_iniciar(&arena, a.fb_gfx + off, arena_cpu, bytes);
    memset(&b_lote, 0, sizeof b_lote); memset(&b_teste, 0, sizeof b_teste); memset(&b_cor, 0, sizeof b_cor);
    memset(&b_z, 0, sizeof b_z); memset(b_tex, 0, sizeof b_tex); memset(&alvo, 0, sizeof alvo);
    if (!prep) prep = kmalloc(LOTE_BYTES);
    if (!prep || !g3d_buffer_criar(&arena, &b_lote, G3D_BUF_LOTE, LOTE_BYTES, 4096) ||
        !g3d_buffer_criar(&arena, &b_teste, G3D_BUF_ALVO, TESTE_BYTES, 4096)) {
        snformatar(m, (size_t)nm, "etapa 3: sin memoria para la cola de ordenes");
        i3_desligar();
        return G3D_NAO;
    }
    klog("[GPU3D] intel: etapa 3: zona de la memoria del VBE: %u KB en %x (grafica %x); lote %x, pruebas %x\n", bytes >> 10,
         a.fb_fisico + off, a.fb_gfx + off, (uint32_t)b_lote.gpu, (uint32_t)b_teste.gpu);
    pronto = true;   /* desde aquí, i3_esperar y entregar funcionan */
    /* Etapa 4: el blitter (por el ring) escribe en la zona y solo donde debe. */
    volatile uint32_t *tc = (volatile uint32_t *)b_teste.cpu;
    for (uint32_t i = 0; i < 256; i++) tc[i] = 0xDEAD0000u | i;
    g3d_fila_t f;
    g3d_fila_iniciar(&f, prep, 32);
    g3_blt_preencher(&f, (uint32_t)b_teste.gpu, TESTE_PASSO, 16, 4, 32, 0x00C0FFEEu);
    if (!anel_blt(f.n, 500)) {
        snformatar(m, (size_t)nm, "etapa 4: el blitter no termina en la zona 3D");
        i3_detalle("etapa 4");
        i3_desligar();
        return G3D_FALHA;
    }
    if (!eir_bem("en la etapa 4", m, nm)) { i3_detalle("etapa 4"); i3_desligar(); return G3D_FALHA; }
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t esperado = i < 64 ? 0x00C0FFEEu : 0xDEAD0000u | i;
        if (tc[i] != esperado) {
            snformatar(m, (size_t)nm, "etapa 4: el blitter no escribe bien en la zona 3D (palabra %u: %x, se esperaba %x)", i, tc[i], esperado);
            i3_desligar();
            return G3D_FALHA;
        }
    }
    klog("[GPU3D] intel: etapa 4: el blitter escribe en la zona 3D\n");
    /* Etapa 5: un lote con un relleno (si no son órdenes en el anillo). */
    if (!por_anel) {
        for (uint32_t i = 0; i < 64; i++) tc[i] = 0;
        g3d_fila_iniciar(&f, prep, 32);
        g3d_por(&f, G3_MI_FLUSH | G3_MI_FLUSH_LEITURA);
        g3_blt_preencher(&f, (uint32_t)b_teste.gpu, TESTE_PASSO, 16, 4, 32, 0x00BEEF01u);
        if (entregar(f.n, NULL) != G3D_OK || !intel_anel_esperar(1000)) {
            snformatar(m, (size_t)nm, "etapa 5: el lote no termina (MI_BATCH_BUFFER_START); probar «gpu3d encender anillo»");
            i3_detalle("etapa 5");
            i3_desligar();
            return G3D_FALHA;
        }
        uint32_t e = intel_anel_reg(R_EIR);
        bool bem = !(e & ~eir_base & EIR_GRAVES);
        for (uint32_t i = 0; i < 64 && bem; i++) if (tc[i] != 0x00BEEF01u) bem = false;
        if (!bem) {
            klog("[GPU3D] intel: etapa 5: el lote termina pero no hace lo que lleva (palabra 0: %x, EIR %x, IPEHR %x): "
                 "se sigue con las ordenes en el anillo\n", tc[0], e, intel_anel_reg(R_IPEHR));
            por_anel = true;
            eir_base = e;
        } else
            klog("[GPU3D] intel: etapa 5: el lote se ejecuta\n");
    }
    /* Etapa 6: RAM del sistema en el GTT (solo si se pidió). */
    if (quer_ram) {
        int r = G3D_OK;
        if (ram.vetada) klog("[GPU3D] intel: etapa 6: RAM en el GTT no: ya fallo en este arranque (%s)\n", ram.motivo);
        else {
            if (!ram.pronta) r = ram_montar(&a);
            if (r == G3D_OK && ram.pronta) r = ram_provar();
            if (r == G3D_FALHA) {
                snformatar(m, (size_t)nm, "etapa 6: la GPU no termina o marca un error con la RAM del GTT");
                i3_desligar();
                return G3D_FALHA;
            }
            if (ram.pronta) {
                ram_ativa = true;
                g3d_arena_iniciar(&arena_ram, ram.gfx, ram.cpu, ram.bytes - RAM_TESTE);
                ram.motivo[0] = 0;
                klog("[GPU3D] intel: etapa 6: RAM en el GTT en uso: %u KB para el destino%s\n", (ram.bytes - RAM_TESTE) >> 10,
                     ram.tex ? " y las texturas" : "");
            }
        }
    }
    /* Etapa 7: el estado 3D completo y un triángulo en la zona de pruebas (16x16, Z de 16 bits sin usar). */
    for (uint32_t i = 0; i < 256; i++) tc[i] = 0;
    g3_alvo_t at = { (uint32_t)b_teste.gpu, TESTE_PASSO, (uint32_t)b_teste.gpu + PAGINA, TESTE_PASSO, 16, 16, 16 };
    gpu3d_estado_t e = { 0, G3D_Z_SEMPRE, G3D_F_UM, G3D_F_ZERO, 0 };
    gpu3d_vert_t v[3];
    static const uint32_t xs[3] = { 0, 0x41800000u, 0 }, ys[3] = { 0, 0, 0x41800000u };   /* (0,0) (16,0) (0,16) */
    for (int i = 0; i < 3; i++) v[i] = (gpu3d_vert_t){ xs[i], ys[i], 0x3F000000u, 0x3F800000u, 0xFF3366CCu, 0, 0, 0 };
    g3d_fila_iniciar(&f, prep, 256);
    g3d_por(&f, G3_MI_FLUSH | G3_MI_FLUSH_LEITURA);
    g3_estado_fixo(&f);
    g3_estado_alvo(&f, &at);
    g3_estado_desenho(&f, &e, NULL);
    g3_triangulos(&f, v, 3);
    if (f.n & 1) g3d_por(&f, G3_MI_NOOP);
    if (f.transbordou || entregar(f.n, NULL) != G3D_OK || i3_esperar(vala_emitida, 1000) != G3D_OK) {
        snformatar(m, (size_t)nm, "etapa 7: la GPU no termina el estado 3D y un triangulo (%s)", por_anel ? "anillo" : "lote");
        i3_detalle("etapa 7");
        i3_desligar();
        return G3D_FALHA;
    }
    uint32_t px = tc[2 * TESTE_PASSO / 4 + 2] & 0x00FFFFFFu;
    klog("[GPU3D] intel: etapa 7: estado 3D aceptado (%s); pixel 2,2 del triangulo: %x (se espera 3366cc)%s\n",
         por_anel ? "anillo" : "lote", px, px == 0x3366CCu ? "" : ": no coincide, la autoprueba dira mas");
    return G3D_OK;
}

/* ------------------------------------------------------------ destino, limpiar, texturas */

static void soltar_alvo(void)
{
    soltar(&b_cor);
    soltar(&b_z);
    memset(&alvo, 0, sizeof alvo);
}

static int i3_alvo(int larg, int alt, int zbits)
{
    if (!pronto) return G3D_FALHA;
    if (larg < 8 || alt < 8 || larg > GPU3D_MAX_LADO || alt > GPU3D_MAX_LADO) return G3D_NAO;
    if (i3_esperar(vala_emitida, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    soltar_alvo();
    uint32_t passo = ((uint32_t)larg * 4 + 63) & ~63u;
    /* El color, en la RAM del GTT si la hay (traerlo es leer RAM con caché). */
    if (!criar(&b_cor, G3D_BUF_ALVO, (uint64_t)passo * (uint32_t)alt, RAM_PRIMEIRO)) return G3D_NAO;
    /* Z de 24 bits si cabe; si no (o si se pidió 16), de 16. Primero en la memoria del VBE (solo la ve la GPU). */
    int zb = zbits == 16 ? 16 : 24;
    uint32_t zp = zb == 24 ? passo : (((uint32_t)larg * 2 + 63) & ~63u);
    if (!criar(&b_z, G3D_BUF_Z, (uint64_t)zp * (uint32_t)alt, VBE_PRIMEIRO)) {
        zb = 16; zp = ((uint32_t)larg * 2 + 63) & ~63u;
        if (!criar(&b_z, G3D_BUF_Z, (uint64_t)zp * (uint32_t)alt, VBE_PRIMEIRO)) { soltar_alvo(); return G3D_NAO; }
    }
    b_cor.passo = passo; b_cor.larg = larg; b_cor.alt = alt; b_cor.bpp = 32;
    b_z.passo = zp; b_z.larg = larg; b_z.alt = alt; b_z.bpp = zb == 24 ? 32 : 16;
    /* Ninguna línea de la caché de la CPU con datos viejos puede pisar después lo que pinte la GPU. */
    if (na_ram(&b_cor)) cache_vaciar(b_cor.cpu, passo * (uint32_t)alt);
    alvo.cor_gpu = (uint32_t)b_cor.gpu; alvo.cor_passo = passo;
    alvo.z_gpu = (uint32_t)b_z.gpu; alvo.z_passo = zp;
    alvo.larg = larg; alvo.alt = alt; alvo.zbits = zb;
    klog("[GPU3D] intel: destino %dx%d: color en %x (paso %u%s), Z de %d bits en %x (paso %u%s); libres %u KB%s\n", larg, alt,
         alvo.cor_gpu, passo, na_ram(&b_cor) ? ", RAM" : "", zb, alvo.z_gpu, zp, na_ram(&b_z) ? ", RAM" : "",
         (uint32_t)(g3d_arena_livre(&arena) >> 10), ram_ativa ? " en la memoria del VBE" : "");
    return G3D_OK;
}

static int i3_limpar(uint32_t mascara, uint32_t cor)
{
    if (!pronto || !b_cor.usado) return G3D_FALHA;
    g3d_fila_t f;
    g3d_fila_iniciar(&f, prep, 16);
    if (mascara & 1) g3_blt_preencher(&f, alvo.cor_gpu, alvo.cor_passo, alvo.larg, alvo.alt, 32, cor & 0x00FFFFFFu);
    if (mascara & 2) g3_blt_preencher(&f, alvo.z_gpu, alvo.z_passo, alvo.larg, alvo.alt, alvo.zbits == 24 ? 32 : 16,
                                      alvo.zbits == 24 ? 0x00FFFFFFu : 0xFFFFu);
    if (!f.n) return G3D_OK;
    /* El blitter va siempre por el ring (como el 2D), detrás de lo que haya pendiente. */
    bool sim = int_salva_desliga();
    bool ok = intel_anel_reservar(f.n + INTEL_ANEL_PAL_ENVIO);
    if (ok) { for (uint32_t i = 0; i < f.n; i++) intel_anel_emitir(prep[i]); intel_anel_enviar(true); }
    int_restaura(sim);
    if (!ok) { i3_detalle("sin sitio para limpiar"); return G3D_FALHA; }
    vala_emitida++;
    return G3D_OK;
}

static int i3_textura(int vaga, int larg, int alt, const uint32_t *argb)
{
    if (!pronto) return G3D_FALHA;
    if (vaga < 0 || vaga >= GPU3D_TEXTURAS) return G3D_NAO;
    /* Puede estar en uso por una entrega pendiente. */
    if (i3_esperar(vala_emitida, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    g3d_buffer_t *b = &b_tex[vaga];
    soltar(b);
    if (!argb) return G3D_OK;
    if (larg < 1 || alt < 1 || larg > GPU3D_MAX_LADO || alt > GPU3D_MAX_LADO) return G3D_NAO;
    uint32_t passo = ((uint32_t)larg * 4 + 63) & ~63u;
    if (!criar(b, G3D_BUF_TEXTURA, (uint64_t)passo * (uint32_t)alt, ram.tex ? RAM_PRIMEIRO : SO_VBE)) return G3D_NAO;
    b->passo = passo; b->larg = larg; b->alt = alt; b->bpp = 32;
    for (int y = 0; y < alt; y++) {
        uint32_t *dst = (uint32_t *)(b->cpu + (uint32_t)y * passo);
        for (int x = 0; x < larg; x++) dst[x] = argb[y * larg + x];
    }
    if (na_ram(b)) { cache_vaciar(b->cpu, passo * (uint32_t)alt); chipset_vaciar(); }
    else __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    return G3D_OK;
}

/* ------------------------------------------------------------ dibujar y leer */

static int i3_desenhar(const gpu3d_estado_t *e, const gpu3d_vert_t *v, int n, g3d_vala_t *vala)
{
    if (!pronto || !b_cor.usado) return G3D_FALHA;
    g3_tex_t tex, *pt = NULL;
    if (e->flags & G3D_TEXTURA) {
        const g3d_buffer_t *b = e->textura < GPU3D_TEXTURAS ? &b_tex[e->textura] : NULL;
        if (!b || !b->usado) return G3D_NAO;
        /* Repetir solo vale con lados potencia de 2 en Gen3. */
        bool pot = !(b->larg & (b->larg - 1)) && !(b->alt & (b->alt - 1));
        if (!pot && (e->flags & (G3D_TEX_REPETE_S | G3D_TEX_REPETE_T))) return G3D_NAO;
        tex.gpu = (uint32_t)b->gpu; tex.passo = b->passo; tex.larg = b->larg; tex.alt = b->alt;
        pt = &tex;
    }
    uint32_t max_pal = por_anel ? ANEL_PAL : LOTE_PAL;
    int por_vez = (int)((max_pal - G3_PAL_ESTADO - 6) / 8);
    por_vez -= por_vez % 3;
    for (int i = 0; i < n; i += por_vez) {
        int q = n - i < por_vez ? n - i : por_vez;
        g3d_fila_t f;
        g3d_fila_iniciar(&f, prep, max_pal);
        /* Vacía lo pintado e invalida las cachés de lectura (una textura recargada en el mismo sitio). */
        g3d_por(&f, G3_MI_FLUSH | G3_MI_FLUSH_LEITURA);
        g3_estado_fixo(&f);
        g3_estado_alvo(&f, &alvo);
        g3_estado_desenho(&f, e, pt);
        g3_triangulos(&f, v + i, q);
        if (f.n & 1) g3d_por(&f, G3_MI_NOOP);
        if (f.transbordou) return G3D_FALHA;
        int r = entregar(f.n, vala);
        if (r != G3D_OK) return r;
    }
    return G3D_OK;
}

static int i3_ler(uint32_t *dest, int passo_px)
{
    if (!pronto || !b_cor.usado) return G3D_FALHA;
    if (i3_esperar(vala_emitida, GPU3D_PRAZO_MS) != G3D_OK) return G3D_FALHA;
    /* En la RAM del GTT: las líneas de la caché que hubiera son viejas (lo escribió la GPU). */
    if (na_ram(&b_cor)) cache_vaciar(b_cor.cpu, alvo.cor_passo * (uint32_t)alvo.alt);
    for (int y = 0; y < alvo.alt; y++) {
        const uint32_t *src = (const uint32_t *)(b_cor.cpu + (uint32_t)y * alvo.cor_passo);
        uint32_t *d = dest + y * passo_px;
        for (int x = 0; x < alvo.larg; x++) d[x] = src[x] & 0x00FFFFFFu;
    }
    return G3D_OK;
}

static void i3_memoria(uint32_t *total_kb, uint32_t *livre_kb)
{
    uint64_t t = pronto ? arena.bytes : 0, l = pronto ? g3d_arena_livre(&arena) : 0;
    if (pronto && ram_ativa) { t += arena_ram.bytes; l += g3d_arena_livre(&arena_ram); }
    *total_kb = (uint32_t)(t >> 10);
    *livre_kb = (uint32_t)(l >> 10);
}

/* D358: lo que de verdad quedó en marcha (la capa lo enseña en «gpu3d» e infopc). */
void gpu3d_intel_estado(uint32_t *variante, uint32_t *ram_kb)
{
    *variante = (pronto && por_anel ? GPU3D_VAR_ANEL : 0) | (pronto && ram_ativa ? GPU3D_VAR_GTT : 0) |
                (pronto && ram_ativa && ram.tex ? GPU3D_VAR_GTT_TEX : 0);
    *ram_kb = pronto && ram_ativa ? (ram.bytes - RAM_TESTE) >> 10 : 0;
}

const gpu3d_driver_t gpu3d_intel = {
    "intel-gen3", i3_iniciar, i3_alvo, i3_limpar, i3_textura, i3_desenhar, i3_esperar, i3_ler, i3_desligar, i3_detalle, i3_memoria,
};
