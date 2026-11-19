// SPDX-License-Identifier: GPL-2.0-or-later
/* acel_vmware.c - acelerador 2D de la VMware SVGA II (PCI 15ad:0405, D195).
 *
 * La tarjeta de VMware (y la que emula QEMU con -device vmware-svga) se
 * programa con dos puertos de E/S (índice y valor, en el BAR 0) y una FIFO de
 * órdenes en memoria (el BAR 2). El VBE ya dejó el modo puesto con la interfaz
 * de Bochs; aquí se pasa la tarjeta a su modo propio («SVGA») con el mismo
 * tamaño y 32 bits: la imagen sigue en el mismo sitio de la memoria de vídeo
 * (desplazamiento 0, el mismo pitch), así que el framebuffer del VBE no cambia.
 * Si algo no coincide, no se activa y la pantalla queda como la dejó el VBE.
 *
 * En modo SVGA la tarjeta solo lleva a la pantalla lo que se le dice con
 * SVGA_CMD_UPDATE: video.c lo pide (acel_atualizar) después de cada copia de
 * la CPU. Las órdenes 2D: SVGA_CMD_RECT_COPY y SVGA_CMD_RECT_FILL, si las
 * capacidades las anuncian (QEMU 8.2 anuncia la copia; el relleno depende de
 * cómo se compiló: sin él, rellena la CPU directamente en la memoria de vídeo).
 *
 * Apagar (desligar) vuelve al modo VGA/VBE (SVGA_REG_ENABLE = 0): la tarjeta
 * vuelve a mostrar el framebuffer del VBE, que tiene la misma imagen. */
#include "acel2d.h"
#include "console.h"
#include "paginacao.h"
#include "portas.h"
#include "texto.h"
#include "video.h"

#define SVGA_ID_2              0x90000002u
#define REG_ID                 0
#define REG_ENABLE             1
#define REG_WIDTH              2
#define REG_HEIGHT             3
#define REG_MAX_WIDTH          4
#define REG_MAX_HEIGHT         5
#define REG_BITS_PER_PIXEL     7
#define REG_BYTES_PER_LINE     12
#define REG_FB_START           13
#define REG_FB_OFFSET          14
#define REG_VRAM_SIZE          15
#define REG_CAPABILITIES       17
#define REG_MEM_START          18
#define REG_MEM_SIZE           19
#define REG_CONFIG_DONE        20
#define REG_SYNC               21
#define REG_BUSY               22

#define CAP_RECT_FILL          0x01
#define CAP_RECT_COPY          0x02

#define FIFO_MIN               0
#define FIFO_MAX               1
#define FIFO_NEXT_CMD          2
#define FIFO_STOP              3
#define FIFO_INICIO            16      /* bytes: los cuatro registros de arriba */

#define CMD_UPDATE             1
#define CMD_RECT_FILL          2
#define CMD_RECT_COPY          3

static uint16_t porta;                 /* BAR 0: índice en porta, valor en porta + 1 */
static volatile uint32_t *fifo;
static uint32_t fifo_bytes, caps;
static bool fifo_mapeada, ligado;
static int larg, alt;

static uint32_t ler(uint32_t r)           { porta_escreve32(porta, r); return porta_le32((uint16_t)(porta + 1)); }
static void     gravar(uint32_t r, uint32_t v) { porta_escreve32(porta, r); porta_escreve32((uint16_t)(porta + 1), v); }

/* Espera a que la tarjeta haya consumido toda la FIFO (con tiempo máximo). */
static bool vm_esperar(void)
{
    acel_prazo_t p;
    acel_prazo(&p, 250);
    while (fifo[FIFO_NEXT_CMD] != fifo[FIFO_STOP]) {
        gravar(REG_SYNC, 1);
        while (ler(REG_BUSY))
            if (acel_prazo_esgotado(&p)) return false;
        if (acel_prazo_esgotado(&p)) return false;
    }
    return true;
}

/* Libres en la FIFO, en bytes (siempre se deja una palabra sin usar). */
static uint32_t livres(void)
{
    uint32_t prox = fifo[FIFO_NEXT_CMD], stop = fifo[FIFO_STOP];
    if (prox >= stop) return (fifo_bytes - prox) + (stop - FIFO_INICIO) - 4;
    return stop - prox - 4;
}

/* Escribe una orden entera y solo después avanza NEXT_CMD. */
static bool fifo_ordem(const uint32_t *w, int n)
{
    if (livres() < (uint32_t)n * 4 && !vm_esperar()) return false;
    uint32_t prox = fifo[FIFO_NEXT_CMD];
    for (int i = 0; i < n; i++) {
        fifo[prox / 4] = w[i];
        prox += 4;
        if (prox >= fifo_bytes) prox = FIFO_INICIO;
    }
    __asm__ volatile("" ::: "memory");
    fifo[FIFO_NEXT_CMD] = prox;
    return true;
}

static bool vm_atualizar(int x, int y, int l, int a)
{
    uint32_t w[5] = { CMD_UPDATE, (uint32_t)x, (uint32_t)y, (uint32_t)l, (uint32_t)a };
    return fifo_ordem(w, 5);
}

static bool vm_copiar(int ox, int oy, int dx, int dy, int l, int a)
{
    if (!(caps & CAP_RECT_COPY)) return false;
    uint32_t w[7] = { CMD_RECT_COPY, (uint32_t)ox, (uint32_t)oy, (uint32_t)dx, (uint32_t)dy, (uint32_t)l, (uint32_t)a };
    if (!fifo_ordem(w, 7)) return false;
    return vm_atualizar(dx, dy, l, a);
}

/* Sin SVGA_CAP_RECT_FILL, la CPU rellena directamente la memoria de vídeo
 * (después de que la tarjeta termine lo anterior, para no adelantarse) y pide
 * que se muestre: el resultado es el mismo y la capa común no lo distingue. */
static bool vm_preencher(int x, int y, int l, int a, uint32_t cor)
{
    if (caps & CAP_RECT_FILL) {
        uint32_t w[6] = { CMD_RECT_FILL, cor, (uint32_t)x, (uint32_t)y, (uint32_t)l, (uint32_t)a };
        if (!fifo_ordem(w, 6)) return false;
    } else {
        if (!vm_esperar()) return false;
        uint32_t passo = video_pitch() / 4;
        uint32_t *p = (uint32_t *)video_placa() + (uint32_t)y * passo + (uint32_t)x;
        for (int j = 0; j < a; j++, p += passo)
            for (int i = 0; i < l; i++) p[i] = cor;
        __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
    }
    return vm_atualizar(x, y, l, a);
}

/* El QEMU (hw/display/vmware_vga.c) recorta y copia contra la «superficie» de
 * su consola, que solo se rehace con el tamaño nuevo cuando refresca la pantalla
 * (cada ~30 ms con ventana; sin pantalla, -display none, solo al pedir una
 * captura). Hasta entonces las órdenes usan el tamaño viejo y escriben donde no
 * deben. Por eso, antes de nada, una sonda: copiar el último píxel de la pantalla
 * al de al lado; cuando sale bien, la tarjeta ya trabaja con el modo nuevo.
 * Devuelve los ms que tardó, o -1 si no llegó a tiempo. Los dos píxeles se
 * redibujan después desde el búfer de atrás (video_mostrar_tudo). */
#define ESPERA_SUPERFICIE_MS 3000
static int superficie_pronta(void)
{
    uint32_t passo = video_pitch() / 4;
    volatile uint32_t *p = (volatile uint32_t *)video_placa() + (uint32_t)(alt - 1) * passo + (uint32_t)(larg - 2);
    acel_prazo_t total;
    acel_prazo(&total, ESPERA_SUPERFICIE_MS);
    for (int vez = 0;; vez++) {
        uint32_t marca = 0x00A5C300u + (uint32_t)vez;
        p[0] = 0x0012345u;
        p[1] = marca;
        __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
        uint32_t w[7] = { CMD_RECT_COPY, (uint32_t)(larg - 1), (uint32_t)(alt - 1), (uint32_t)(larg - 2), (uint32_t)(alt - 1), 1, 1 };
        if (!fifo_ordem(w, 7) || !vm_esperar()) return -1;
        if (p[0] == marca) return vez * 20;
        /* Unos 20 ms entre intentos (la pantalla del QEMU refresca cada ~30). */
        acel_prazo_t pausa;
        acel_prazo(&pausa, 20);
        while (!acel_prazo_esgotado(&pausa))
            if (acel_prazo_esgotado(&total)) return -1;
    }
}

static void vm_desligar(void)
{
    if (!ligado) return;
    vm_esperar();
    gravar(REG_ENABLE, 0);   /* vuelve el modo VGA/VBE: el mismo framebuffer */
    ligado = false;
    klog("[ACEL2D] vmware: modo SVGA apagado, la pantalla vuelve al VBE\n");
}

static bool vm_iniciar(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *m, int nm)
{
    (void)geracao;
    uint32_t cmd = pci_le32(d, 0x04);
    if ((cmd & 0x03) != 0x03) pci_escreve32(d, 0x04, (cmd & 0xFFFFu) | 0x03);   /* E/S y memoria */
    uint32_t bar0 = pci_le32(d, 0x10), bar2 = pci_le32(d, 0x18);
    if (!(bar0 & 1)) { snformatar(m, (size_t)nm, "el BAR 0 no es de E/S (%x)", bar0); return false; }
    porta = (uint16_t)(bar0 & ~3u);
    gravar(REG_ID, SVGA_ID_2);
    uint32_t id = ler(REG_ID);
    if (id != SVGA_ID_2) { snformatar(m, (size_t)nm, "no habla SVGA II (id %x)", id); return false; }
    caps = ler(REG_CAPABILITIES);
    uint32_t fb = ler(REG_FB_START), vram = ler(REG_VRAM_SIZE);
    acel2d_vram_real(vram, "SVGA VRAM_SIZE");   /* D387 */
    uint32_t mem = ler(REG_MEM_START), mem_tam = ler(REG_MEM_SIZE);
    uint32_t maxl = ler(REG_MAX_WIDTH), maxa = ler(REG_MAX_HEIGHT);
    klog("[ACEL2D] vmware: puertos %x, capacidades %x (copia %s, relleno %s), framebuffer %x, %u KB, FIFO %x (%u KB), maximo %ux%u\n",
         porta, caps, caps & CAP_RECT_COPY ? "si" : "no", caps & CAP_RECT_FILL ? "si" : "no (lo hace la CPU)",
         fb, vram >> 10, mem, mem_tam >> 10, maxl, maxa);
    if (!(caps & CAP_RECT_COPY)) { snformatar(m, (size_t)nm, "la tarjeta no anuncia SVGA_CAP_RECT_COPY"); return false; }
    if (fb != t->fb_fisico) { snformatar(m, (size_t)nm, "el framebuffer del VBE (%x) no es el de la tarjeta (%x)", t->fb_fisico, fb); return false; }
    if ((bar2 & ~0xFu) != mem || mem_tam < 16 * 1024) { snformatar(m, (size_t)nm, "FIFO extrana (%x, %u bytes)", mem, mem_tam); return false; }
    if ((uint32_t)t->larg > maxl || (uint32_t)t->alt > maxa) { snformatar(m, (size_t)nm, "modo %dx%d mayor que el maximo", t->larg, t->alt); return false; }
    if (!fifo_mapeada) {
        fifo = (volatile uint32_t *)janela_mapear_tipo(mem, mem_tam, PG_PCD | PG_PWT);
        if (!fifo) { snformatar(m, (size_t)nm, "sin ventana para la FIFO"); return false; }
        fifo_mapeada = true;
    }
    fifo_bytes = mem_tam > 256 * 1024 ? 256 * 1024 : mem_tam;
    larg = t->larg; alt = t->alt;
    /* El modo SVGA con el mismo tamaño que el VBE, y la FIFO. */
    gravar(REG_WIDTH, (uint32_t)larg);
    gravar(REG_HEIGHT, (uint32_t)alt);
    gravar(REG_BITS_PER_PIXEL, 32);
    fifo[FIFO_MIN] = FIFO_INICIO;
    fifo[FIFO_MAX] = fifo_bytes;
    fifo[FIFO_NEXT_CMD] = FIFO_INICIO;
    fifo[FIFO_STOP] = FIFO_INICIO;
    gravar(REG_ENABLE, 1);
    gravar(REG_CONFIG_DONE, 1);
    ligado = true;
    uint32_t bpl = ler(REG_BYTES_PER_LINE), off = ler(REG_FB_OFFSET);
    if (bpl != t->pitch || off != 0) {
        vm_desligar();
        snformatar(m, (size_t)nm, "en modo SVGA el pitch es %u y el desplazamiento %u (el VBE: %u y 0)", bpl, off, t->pitch);
        return false;
    }
    vm_atualizar(0, 0, larg, alt);
    if (!vm_esperar()) { vm_desligar(); snformatar(m, (size_t)nm, "la FIFO no avanza"); return false; }
    int ms = superficie_pronta();
    if (ms < 0) {
        vm_desligar();
        snformatar(m, (size_t)nm, "la tarjeta no paso a %dx%d en %d ms (QEMU sin pantalla que refresque)", larg, alt, ESPERA_SUPERFICIE_MS);
        return false;
    }
    klog("[ACEL2D] vmware: la tarjeta ve el modo nuevo (%d ms)\n", ms);
    klog("[ACEL2D] vmware: modo SVGA %dx%d, 32 bits, pitch %u, FIFO de %u KB\n", larg, alt, bpl, fifo_bytes >> 10);
    return true;
}

const acel_driver_t acel_vmware = {
    "vmware", ACEL2D_VMWARE, vm_iniciar, vm_copiar, vm_preencher, vm_esperar, vm_atualizar, vm_desligar,
    false,   /* QEMU solo acepta rectángulos dentro de la pantalla visible */
};
