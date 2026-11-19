/* acel_nv.c - el motor 2D de las NVIDIA GeForce 6/7 y Go 6xxx/7xxx (NV4x/G7x)
 * y de las integradas en los chipsets nForce (GeForce 6100/6150 y 7025/7050/
 * 7100/7150: C51, C61, C67, C68 y C73) (D224).
 *
 * SIN PROBAR EN HARDWARE: QEMU no emula ninguna GeForce. La capa común solo lo
 * activa si su autoprueba del arranque coincide con la CPU y termina a tiempo.
 *
 * Cómo se usa el motor (lo mismo que el driver «nv» de X.Org, que escribió
 * NVIDIA; sus valores están en kernel/terceros/mit/nv/nv_2d.h, licencia MIT):
 *   - PFIFO: el canal 0 en modo DMA lee las órdenes de un búfer de 32 KB en la
 *     memoria de vídeo (aquí, debajo de la pantalla y de la zona de prueba);
 *     la CPU escribe las órdenes y avanza PUT (BAR 0 + 0x800040); la GPU
 *     avanza GET (0x800044) al leerlas. Sin firmware ni microcódigo: las NV4x
 *     no lo necesitan para el 2D;
 *   - PRAMIN: la tabla hash con 8 objetos (SURFACES_2D, ROP, PATTERN, CLIP,
 *     LINE, IMAGE_BLIT, GDI_RECTANGLE_TEXT, SCALED_IMAGE), atados a los
 *     subcanales 0-7 al principio del búfer;
 *   - una copia es IMAGE_BLIT (origen, destino, tamaño; la propia GPU resuelve
 *     los solapes) y un relleno, GDI_RECTANGLE_TEXT con ROP 0xCC.
 *
 * Seguridad en hardware real (lo que se comprueba antes de escribir nada):
 *   1. el identificador PCI está en la lista exacta (acel_nv_logica.h) y el
 *      chip que dice PMC_BOOT_0 es el que corresponde (la tarjeta, en modo
 *      little endian: PMC_BOOT_1 = 0);
 *   2. la pantalla del VBE está en el BAR 1 y una de las dos cabezas del CRTC
 *      la muestra: su dirección de inicio (PCRTC 0x600800/0x602800) es la del
 *      VBE dentro de la memoria de vídeo y su pitch (CR13/CR19/CR42) el del VBE;
 *   3. la memoria de vídeo que dice PFB 0x10020C cubre lo que anuncia el VBE,
 *      y el búfer de órdenes cabe debajo de la pantalla sin tocar el último MB
 *      (PRAMIN). En las integradas (UMA) esa memoria es RAM del sistema que la
 *      BIOS reservó: el driver comprueba además que PMC 0x1700/0x170C ya
 *      apuntan donde los pondrían X.Org y nouveau (PFB 0x10020C); si no, no
 *      se toca nada. Los objetos DMA tienen como límite la memoria de vídeo:
 *      la GPU no puede escribir fuera de ella;
 *   4. ninguna región de tiling de PFB está activa (la pantalla del VBE es
 *      lineal; el driver no programa el tiling).
 * No se tocan relojes, la memoria (PFB), el modo ni el CRTC (solo se leen;
 * para leer CR19/CR42 se abre el candado CR1F y se vuelve a cerrar). Se
 * reinician PFIFO y PGRAPH (bits 8 y 12 de PMC_ENABLE) y sus interrupciones
 * quedan apagadas (todo por sondeo con plazo de TSC, como pide D195). Al
 * apagar se paran el canal y los dos motores, se deja PMC_ENABLE y la
 * máscara de interrupciones como estaban y se devuelven las palabras de
 * PRAMIN que se escribieron (y RAMFC/RAMRO, donde PFIFO escribe solo). Qué mirar en el log: docs/decisoes.md, D224. */
#include "acel2d.h"
#include "console.h"
#include "paginacao.h"
#include "texto.h"
#include "acel_nv_logica.h"
#include "../terceros/mit/nv/nv_2d.h"

#define ZONA_LINHAS      64           /* las filas de la zona de prueba de acel2d.c (ZONA_A) */
#define PMC_BOOT_0      0x000000
#define PMC_BOOT_1      0x000004
#define PMC_INTR_EN     0x000140
#define PMC_ENABLE      0x000200
#define PMC_1540        0x001540
#define PMC_1700        0x001700
#define PMC_170C        0x00170C
#define PMC_RESET_2D    0x00001100u  /* PFIFO (bit 8) y PGRAPH (bit 12) */
#define PFIFO_INTR      0x002100
#define PFIFO_CACHES    0x002500
#define PFIFO_PUSH0     0x003200
#define PFIFO_DMA_PUSH  0x003220
#define PFIFO_DMA_STATE 0x003228
#define PFIFO_DMA_PUT   0x003240
#define PFIFO_DMA_GET   0x003244
#define PFIFO_PULL0     0x003250
#define PFB_20C         0x10020C
#define PGRAPH_INTR     0x400100
#define PGRAPH_NSTATUS  0x400104
#define PGRAPH_NSOURCE  0x400108
#define PGRAPH_TRAP_END 0x400704
#define PGRAPH_TRAP_DAT 0x400708
#define PCRTC_START     0x600800     /* + 0x2000 la cabeza 1 */
#define PCIO_CRX        0x6013D4     /* índice del CRTC (+ 0x2000 la cabeza 1); el dato en +1 */

/* Las partes del BAR 0 que se usan (se mapean una vez, sin caché). */
typedef struct { uint32_t ini, tam; volatile uint8_t *v; } nv_janela_t;
static nv_janela_t jan[] = {
    { 0x000000, 0x4000, 0 },   /* PMC y PFIFO */
    { 0x100000, 0x1000, 0 },   /* PFB */
    { 0x400000, 0x7000, 0 },   /* PGRAPH */
    { 0x600000, 0x4000, 0 },   /* PCRTC y PCIO de las dos cabezas */
    { 0x710000, 0x3000, 0 },   /* PRAMIN: tabla hash y objetos */
    { 0x800000, 0x1000, 0 },   /* el canal 0 (PUT y GET) */
};
#define N_JAN (int)(sizeof jan / sizeof jan[0])
static uint32_t bar0_mapeado;
static volatile uint32_t lixo;      /* para un registro fuera de las ventanas (no debería pasar) */

static volatile uint8_t *reg8(uint32_t off)
{
    for (int i = 0; i < N_JAN; i++)
        if (off >= jan[i].ini && off < jan[i].ini + jan[i].tam) return jan[i].v + (off - jan[i].ini);
    return (volatile uint8_t *)&lixo;
}
static inline uint32_t rd(uint32_t off)             { return *(volatile uint32_t *)reg8(off); }
static inline void     wr(uint32_t off, uint32_t v) { *(volatile uint32_t *)reg8(off) = v; }

static volatile uint32_t *pb;        /* el búfer de órdenes visto por la CPU (sin caché) */
static uint32_t pb_fisico;
static uint32_t atual, put;          /* en palabras: dónde se escribe y lo último lanzado */
static bool ligado;                  /* algo cambiado en la tarjeta: desligar lo deshace */
static int chip, diag;
static uint32_t pmc_enable, pmc_intr_en;
static uint32_t pramin_antes[sizeof nv40_pramin / sizeof nv40_pramin[0] + 2];
/* RAMFC (0x11000) y RAMRO (0x11200), donde PFIFO puede escribir por su cuenta
 * (el contexto del canal y las órdenes que no pudo entregar): PRAMIN 0x400-0x4FF. */
#define RAMFC_INICIO 0x0400
#define RAMFC_N      0x0100
static uint32_t ramfc_antes[RAMFC_N];

/* ------------------------------------------------------------------ diagnóstico */

static void nv_diagnostico(const char *quando)
{
    if (diag >= 4) return;           /* solo los primeros fallos: el log no se llena */
    diag++;
    klog("[ACEL2D] nvidia: %s: GET %x PUT %x (palabra %u), PFIFO intr %x push %x estado %x put %x get %x; "
         "PGRAPH intr %x nstatus %x nsource %x estado %x trap %x dato %x\n", quando,
         rd(NV_USER_DMA_GET), rd(NV_USER_DMA_PUT), put, rd(PFIFO_INTR), rd(PFIFO_DMA_PUSH), rd(PFIFO_DMA_STATE),
         rd(PFIFO_DMA_PUT), rd(PFIFO_DMA_GET), rd(PGRAPH_INTR), rd(PGRAPH_NSTATUS), rd(PGRAPH_NSOURCE),
         rd(NV_PGRAPH_STATUS), rd(PGRAPH_TRAP_END), rd(PGRAPH_TRAP_DAT));
}

/* ------------------------------------------------------------------ el búfer */

/* La GPU ve lo escrito en el búfer antes de que llegue el PUT: leer el
 * búfer obliga a que las escrituras pendientes por el PCI lleguen antes. */
static void lancar(void)
{
    if (atual == put) return;
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    (void)pb[atual - 1];
    put = atual;
    wr(NV_USER_DMA_PUT, put * 4);
}

/* Hasta que GET llegue a «ate» (en palabras), con plazo. Un error de PFIFO o de
 * PGRAPH termina antes: con sus interrupciones apagadas la GPU se pararía. */
static bool esperar_get(uint32_t ate, acel_prazo_t *p)
{
    while ((rd(NV_USER_DMA_GET) >> 2) != ate) {
        if (rd(PFIFO_INTR) || rd(PGRAPH_INTR)) { nv_diagnostico("error de la GPU leyendo el bufer"); return false; }
        if (acel_prazo_esgotado(p)) { nv_diagnostico("el bufer no avanza"); return false; }
    }
    return true;
}

static bool nv_esperar(void)
{
    acel_prazo_t p;
    acel_prazo(&p, 250);
    lancar();
    if (!esperar_get(put, &p)) return false;
    while (rd(NV_PGRAPH_STATUS)) {
        if (rd(PGRAPH_INTR)) { nv_diagnostico("error de PGRAPH"); return false; }
        if (acel_prazo_esgotado(&p)) { nv_diagnostico("PGRAPH no termina"); return false; }
    }
    return true;
}

/* Sitio para n palabras. Al final del búfer: se espera a que la GPU lo
 * termine, se escribe un salto al principio y PUT se deja SKIPS palabras más
 * allá (las primeras son ceros: «no se puede saltar a donde está PUT»). */
static bool reservar(uint32_t n)
{
    if (nv_anel_cabe(atual, n)) return true;
    if (!nv_esperar()) return false;
    pb[atual] = nv_salto(0);
    atual = NV_ANEL_SKIPS;
    lancar();
    acel_prazo_t p;
    acel_prazo(&p, 250);
    return esperar_get(atual, &p);
}

static inline void emitir(uint32_t v) { pb[atual++] = v; }
static inline void ordem(uint32_t metodo, uint32_t n) { emitir(nv_cabecalho(metodo, n)); }

static bool nv_copiar(int ox, int oy, int dx, int dy, int l, int a)
{
    if (!nv_coord_valida(ox, oy, l, a) || !nv_coord_valida(dx, dy, l, a) || !reservar(4)) return false;
    ordem(BLIT_POINT_SRC, 3);
    emitir(nv_ponto(ox, oy));
    emitir(nv_ponto(dx, dy));
    emitir(nv_tamanho(l, a));
    lancar();
    return true;
}

static bool nv_preencher(int x, int y, int l, int a, uint32_t cor)
{
    if (!nv_coord_valida(x, y, l, a) || !reservar(5)) return false;
    ordem(RECT_SOLID_COLOR, 1);
    emitir(cor);
    ordem(RECT_SOLID_RECTS(0), 2);
    emitir(nv_rect_ponto(x, y));
    emitir(nv_rect_tamanho(l, a));
    lancar();
    return true;
}

/* ------------------------------------------------------------------ apagar */

static void nv_desligar(void)
{
    if (!ligado) return;
    nv_esperar();
    /* El canal parado y PFIFO/PGRAPH reiniciados: no queda nada leyendo el búfer. */
    wr(PFIFO_CACHES, 0);
    wr(PFIFO_DMA_PUSH, 0);
    wr(PFIFO_PUSH0, 0);
    wr(PFIFO_PULL0, 0);
    wr(PMC_ENABLE, pmc_enable & ~PMC_RESET_2D);
    (void)rd(PMC_ENABLE);
    wr(PMC_ENABLE, pmc_enable);
    /* PRAMIN y las interrupciones, como estaban. */
    int n = (int)(sizeof nv40_pramin / sizeof nv40_pramin[0]);
    volatile uint32_t *ram = (volatile uint32_t *)reg8(NV40_PRAMIN_BASE);
    for (int i = 0; i < n; i++) ram[nv40_pramin[i].indice] = pramin_antes[i];
    ram[NV40_PRAMIN_DMA_VRAM_LIMITE] = pramin_antes[n];
    ram[NV40_PRAMIN_PB_ENDERECO] = pramin_antes[n + 1];
    for (int i = 0; i < RAMFC_N; i++) ram[RAMFC_INICIO + i] = ramfc_antes[i];
    wr(PMC_INTR_EN, pmc_intr_en);
    ligado = false;
    klog("[ACEL2D] nvidia: motor 2D parado; PMC_ENABLE %x y PRAMIN como estaban\n", rd(PMC_ENABLE));
}

/* ------------------------------------------------------------------ iniciar */

static void escrever_tabela(const nv_escrita_t *t, int n)
{
    for (int i = 0; i < n; i++)
        wr(t[i].reg, (t[i].mascara ? (rd(t[i].reg) & t[i].mascara) : 0) | t[i].valor);
}
#define TABELA(t) escrever_tabela(t, (int)(sizeof t / sizeof t[0]))

/* Lee un registro del CRTC de una cabeza dejando el índice como estaba. */
static uint8_t ler_cr(int cabeca, uint8_t indice)
{
    uint32_t base = PCIO_CRX + (uint32_t)cabeca * 0x2000;
    uint8_t antes = *reg8(base);
    *reg8(base) = indice;
    uint8_t v = *reg8(base + 1);
    *reg8(base) = antes;
    return v;
}

/* ¿Qué cabeza muestra la pantalla del VBE? -1: ninguna. */
static int cabeca_da_tela(uint32_t fb_off, uint32_t pitch_vbe)
{
    /* CR19 y CR42 son extendidos: el candado CR1F (0 = cerrado) se abre con
     * 0x57 y se vuelve a cerrar con 0x99 si lo estaba. */
    uint8_t idx = *reg8(PCIO_CRX);
    *reg8(PCIO_CRX) = 0x1F;
    bool fechado = *reg8(PCIO_CRX + 1) == 0;
    if (fechado) *reg8(PCIO_CRX + 1) = 0x57;
    int cab = -1;
    for (int h = 0; h < 2; h++) {
        uint32_t ini = rd(PCRTC_START + (uint32_t)h * 0x2000);
        uint8_t cr13 = ler_cr(h, 0x13), cr19 = ler_cr(h, 0x19), cr42 = ler_cr(h, 0x42);
        uint32_t pitch = nv_pitch_crtc(cr13, cr19, cr42);
        klog("[ACEL2D] nvidia: cabeza %d: inicio %x, pitch %u (CR13 %x CR19 %x CR42 %x)\n", h, ini, pitch, cr13, cr19, cr42);
        if (cab < 0 && ini == fb_off && pitch == pitch_vbe) cab = h;
    }
    if (fechado) { *reg8(PCIO_CRX) = 0x1F; *reg8(PCIO_CRX + 1) = 0x99; }
    *reg8(PCIO_CRX) = idx;
    return cab;
}

static bool ler_bar(const pci_disp_t *d, uint8_t reg, uint32_t *base)
{
    uint32_t v = pci_le32(d, reg);
    if (v & 1) return false;                                       /* de E/S */
    if (((v >> 1) & 3) == 2 && pci_le32(d, (uint8_t)(reg + 4))) return false;   /* por encima de 4 GB */
    *base = v & ~0xFu;
    return *base != 0;
}

static bool nv_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *m, int nm)
{
    (void)geracao;
    diag = 0;
    /* 1. El modelo exacto: si no está en la lista, no se lee ni un registro. */
    const nv_modelo_t *mod = nv_modelo(d->fabricante, d->modelo);
    if (!mod) { snformatar(m, (size_t)nm, "%04x:%04x no esta en la lista de GeForce 6/7 (no se toca)", d->fabricante, d->modelo); return false; }
    uint32_t bar0, bar1;
    if (!ler_bar(d, 0x10, &bar0)) { snformatar(m, (size_t)nm, "el BAR 0 no es MMIO por debajo de 4 GB (%x)", pci_le32(d, 0x10)); return false; }
    if (!ler_bar(d, 0x14, &bar1)) { snformatar(m, (size_t)nm, "el BAR 1 no es memoria por debajo de 4 GB (%x)", pci_le32(d, 0x14)); return false; }
    if (t->fb_fisico < bar1 || t->fb_fisico - bar1 >= 0x10000000u) {
        snformatar(m, (size_t)nm, "la pantalla (%x) no esta en el BAR 1 (%x)", t->fb_fisico, bar1);
        return false;
    }
    uint32_t fb_off = t->fb_fisico - bar1;
    if (bar0_mapeado != bar0) {
        for (int i = 0; i < N_JAN; i++) {
            jan[i].v = (volatile uint8_t *)janela_mapear_tipo(bar0 + jan[i].ini, jan[i].tam, PG_PCD | PG_PWT);
            if (!jan[i].v) { bar0_mapeado = 0; snformatar(m, (size_t)nm, "sin ventana para el MMIO"); return false; }
        }
        bar0_mapeado = bar0;
    }
    /* 2. El chip. */
    uint32_t boot0 = rd(PMC_BOOT_0), boot1 = rd(PMC_BOOT_1);
    chip = nv_chip_de_boot0(boot0);
    klog("[ACEL2D] nvidia: %s (%04x), PMC_BOOT_0 %x (chip %x), BOOT_1 %x, BAR0 %x, BAR1 %x, pantalla en %x\n",
         mod->nome, d->modelo, boot0, chip, boot1, bar0, bar1, fb_off);
    if (boot1) { snformatar(m, (size_t)nm, "la tarjeta no esta en little endian (PMC_BOOT_1 %x)", boot1); return false; }
    if (!nv_modelo_aceita(mod, chip)) {
        snformatar(m, (size_t)nm, "PMC_BOOT_0 %x: el chip %x no es el de %04x", boot0, chip, d->modelo);
        return false;
    }
    /* 3. La memoria de vídeo y, en las de PMC 0x1700, que ya está como la dejarían X.Org y nouveau. */
    uint32_t p20c = rd(PFB_20C), vram = nv_vram_bytes(p20c);
    klog("[ACEL2D] nvidia: PFB cfg %x %x, 0x20c %x: %u MB%s; el VBE dice %u KB\n", rd(NV40_PFB_CFG0), rd(NV40_PFB_CFG1),
         p20c, vram >> 20, nv_chip_integrado(chip) ? " (integrada: RAM del sistema)" : "", t->memoria >> 10);
    if (vram < 16u * 1024 * 1024) { snformatar(m, (size_t)nm, "PFB 0x10020c (%x) da menos de 16 MB", p20c); return false; }
    acel2d_vram_real(vram, nv_chip_integrado(chip) ? "PFB 0x10020c (RAM del sistema)" : "PFB 0x10020c");   /* D387 */
    /* Contra lo que dice PFB sin el tope de 256 MB (una 7900 GTX de 512 MB). */
    if (t->memoria > (p20c & 0xFFF00000u)) {
        snformatar(m, (size_t)nm, "el VBE dice %u KB y PFB %u KB: no cuadran", t->memoria >> 10, (p20c & 0xFFF00000u) >> 10);
        return false;
    }
    if (nv_chip_pmc1700(chip)) {
        uint32_t a = rd(PMC_1700), b = rd(PMC_170C);
        klog("[ACEL2D] nvidia: PMC 0x1700 %x, 0x170c %x\n", a, b);
        if (a != p20c || b != p20c) {
            snformatar(m, (size_t)nm, "PMC 0x1700/0x170c (%x/%x) no valen PFB 0x10020c (%x): no se toca", a, b, p20c);
            return false;
        }
    }
    uint32_t tb;
    int nt = nv_regioes_tiling(chip, &tb);
    for (int i = 0; i < nt; i++)
        if (rd(tb + (uint32_t)i * 16) & 1) {
            snformatar(m, (size_t)nm, "region de tiling %d activa (%x = %x)", i, tb + (uint32_t)i * 16, rd(tb + (uint32_t)i * 16));
            return false;
        }
    /* 4. El CRTC muestra la pantalla del VBE. */
    int cab = cabeca_da_tela(fb_off, t->pitch);
    if (cab < 0) { snformatar(m, (size_t)nm, "ninguna cabeza del CRTC muestra %x con el pitch del VBE (%u)", fb_off, t->pitch); return false; }
    if (t->pitch & 63) { snformatar(m, (size_t)nm, "pitch %u no es multiplo de 64", t->pitch); return false; }
    /* 5. El búfer de órdenes, debajo de la pantalla. */
    uint32_t pb_off;
    int r = nv_colocar_pb(fb_off, t->pitch, (uint32_t)t->alt, ZONA_LINHAS, vram, t->memoria, &pb_off);
    if (r != NV_PB_OK) {
        snformatar(m, (size_t)nm, "no hay sitio para el bufer de ordenes (%s)",
                   r == NV_PB_SEM_VBE ? "fuera de la memoria del VBE" : r == NV_PB_RAMIN ? "pisaria PRAMIN" : "pantalla mal alineada");
        return false;
    }
    uint32_t fis = t->fb_fisico + (pb_off - fb_off);
    if (!pb || pb_fisico != fis) {
        pb = (volatile uint32_t *)janela_mapear_tipo(fis, NV_PB_BYTES, PG_PCD | PG_PWT);
        pb_fisico = pb ? fis : 0;
        if (!pb) { snformatar(m, (size_t)nm, "sin ventana para el bufer de ordenes"); return false; }
    }
    klog("[ACEL2D] nvidia: cabeza %d, bufer de ordenes en la VRAM %x (fisica %x)\n", cab, pb_off, fis);

    /* A partir de aquí se cambia la tarjeta: desligar lo deshace. */
    pmc_enable = rd(PMC_ENABLE);
    pmc_intr_en = rd(PMC_INTR_EN);
    int n = (int)(sizeof nv40_pramin / sizeof nv40_pramin[0]);
    volatile uint32_t *ram = (volatile uint32_t *)reg8(NV40_PRAMIN_BASE);
    for (int i = 0; i < n; i++) pramin_antes[i] = ram[nv40_pramin[i].indice];
    pramin_antes[n] = ram[NV40_PRAMIN_DMA_VRAM_LIMITE];
    pramin_antes[n + 1] = ram[NV40_PRAMIN_PB_ENDERECO];
    for (int i = 0; i < RAMFC_N; i++) ramfc_antes[i] = ram[RAMFC_INICIO + i];
    klog("[ACEL2D] nvidia: PMC_ENABLE %x, interrupciones %x, PGRAPH estado %x, PFIFO caches %x\n",
         pmc_enable, pmc_intr_en, rd(NV_PGRAPH_STATUS), rd(PFIFO_CACHES));
    ligado = true;
    wr(PMC_INTR_EN, 0);                               /* la tarjeta no pide interrupciones */
    wr(PMC_ENABLE, pmc_enable & ~PMC_RESET_2D);       /* PFIFO y PGRAPH reiniciados */
    (void)rd(PMC_ENABLE);
    wr(PMC_ENABLE, pmc_enable | PMC_RESET_2D);

    /* PRAMIN: tabla hash y objetos (y se comprueba que se guardan). */
    for (int i = 0; i < n; i++) ram[nv40_pramin[i].indice] = nv40_pramin[i].valor;
    ram[NV40_PRAMIN_DMA_VRAM_LIMITE] = vram - 1;
    ram[NV40_PRAMIN_PB_ENDERECO] = pb_off | 2;
    for (int i = 0; i < n; i++)
        if (ram[nv40_pramin[i].indice] != nv40_pramin[i].valor) {
            snformatar(m, (size_t)nm, "PRAMIN no guarda lo escrito (palabra %x: %x)", nv40_pramin[i].indice, ram[nv40_pramin[i].indice]);
            return false;
        }

    /* PGRAPH. */
    TABELA(nv40_pgraph_inicio);
    uint32_t j = rd(PMC_1540) & 0xFF;
    if (j) {
        uint32_t i = 0;
        while (!(j & 1)) { j >>= 1; i++; }
        wr(0x405000, i);
    }
    if (chip == 0x40) TABELA(nv40_pgraph_nv40); else TABELA(nv40_pgraph_outros);
    switch (chip) {
    case 0x40: case 0x45: TABELA(nv40_pgraph_40_45); break;
    case 0x41: case 0x42: TABELA(nv40_pgraph_41_42); break;
    case 0x43: TABELA(nv40_pgraph_43); break;
    case 0x47: case 0x49: case 0x4B: TABELA(nv40_pgraph_g70); break;
    default: if (nv_chip_pmc1700(chip)) TABELA(nv40_pgraph_nv44); break;
    }
    TABELA(nv40_pgraph_meio);
    uint32_t c0 = rd(NV40_PFB_CFG0), c1 = rd(NV40_PFB_CFG1);
    int mc = nv_memcfg(chip);
    if (mc & NV_MEMCFG_9A4) {
        wr(0x4009A4, c0); wr(0x4009A8, c1); wr(0x4069A4, c0); wr(0x4069A8, c1);
        wr(0x400820, 0); wr(0x400824, 0); wr(0x400864, vram - 1); wr(0x400868, vram - 1);
    } else {
        if (mc & NV_MEMCFG_9F0) { wr(0x4009F0, c0); wr(0x4009F4, c1); }
        if (mc & NV_MEMCFG_DF0) { wr(0x400DF0, c0); wr(0x400DF4, c1); }
        wr(0x4069F0, c0); wr(0x4069F4, c1);
        wr(0x400840, 0); wr(0x400844, 0); wr(0x4008A0, vram - 1); wr(0x4008A4, vram - 1);
    }
    TABELA(nv40_pgraph_fim);

    /* El búfer: SKIPS ceros, los objetos atados a los subcanales y el formato
     * de la pantalla (32 bits, su pitch y su dirección en la VRAM). */
    for (int i = 0; i < NV_ANEL_SKIPS; i++) pb[i] = 0;
    for (int i = 0; i < 16; i++) pb[NV_ANEL_SKIPS + i] = nv_dma_ligacoes[i];
    put = 0;
    atual = NV_ANEL_SKIPS + 16;
    ordem(SURFACE_FORMAT, 4);
    emitir(SURFACE_FORMAT_DEPTH24);
    emitir(nv_pitch_surface(t->pitch));
    emitir(fb_off);
    emitir(fb_off);
    ordem(PATTERN_FORMAT, 1);
    emitir(PATTERN_FORMAT_DEPTH24);
    ordem(RECT_FORMAT, 1);
    emitir(RECT_FORMAT_DEPTH24);
    ordem(LINE_FORMAT, 1);
    emitir(LINE_FORMAT_DEPTH24);
    ordem(PATTERN_COLOR_0, 4);
    for (int i = 0; i < 4; i++) emitir(0xFFFFFFFFu);
    ordem(ROP_SET, 1);
    emitir(NV_ROP_GXCOPY);

    /* PFIFO: el canal 0 empieza a leer el búfer. */
    TABELA(nv40_pfifo_inicio);
    lancar();
    if (!nv_esperar()) { snformatar(m, (size_t)nm, "la GPU no lee el bufer de ordenes (GET %x)", rd(NV_USER_DMA_GET)); return false; }
    klog("[ACEL2D] nvidia: PFIFO y PGRAPH listos (chip %x, memoria en PGRAPH %s), GET %x\n", chip,
         mc == NV_MEMCFG_9A4 ? "9a4" : mc == NV_MEMCFG_9F0 ? "9f0" : mc == NV_MEMCFG_DF0 ? "df0" : "9f0 y df0",
         rd(NV_USER_DMA_GET));
    return true;
}

const acel_driver_t acel_nv = {
    "nvidia", ACEL2D_NVIDIA, nv_iniciar, nv_copiar, nv_preencher, nv_esperar, NULL, nv_desligar,
    true,    /* la zona de prueba va debajo de la pantalla visible */
};
