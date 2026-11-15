// SPDX-License-Identifier: GPL-2.0-or-later
/* bga_extra.c - segundas (y terceras) tarjetas de Bochs/QEMU como salidas de vídeo extra (D290).
 *
 * QEMU puede llevar varias pantallas: -device bochs-display (o secondary-vga) añade otra tarjeta
 * PCI 1234:1111 con su propio framebuffer (BAR 0) y los mismos registros "dispi" del BGA (bga.c),
 * pero en memoria: el BAR 2 tiene una ventana MMIO con el registro n en 0x500 + 2 * n. Las
 * tarjetas extra no tienen puertos de E/S; la principal (la del arranque) sigue con bga.c.
 *
 * Cada una se registra en telas.c como una salida de 32 bits por píxel. El modo de arranque de
 * esta salida es el mismo de la principal si lo acepta (así el espejo sale limpio) o 1024x768.
 * Es un driver pensado para probar el escritorio de varias pantallas sin hardware real. */
#include "telas.h"
#include "bootinfo.h"
#include "console.h"
#include "memoria.h"
#include "paginacao.h"
#include "pci.h"
#include "texto.h"
#include "video.h"
#include "wc.h"

#define MMIO_DISPI   0x500
#define REG_ID       0
#define REG_XRES     1
#define REG_YRES     2
#define REG_BPP      3
#define REG_LIGAR    4
#define REG_LARG_VIRT 6
#define REG_ALT_VIRT 7
#define REG_X        8
#define REG_Y        9
#define REG_MEM_64K  10
#define LIGADO       0x01
#define CAPACIDADES  0x02
#define COM_LFB      0x40

typedef struct {
    volatile uint8_t *mmio;
} extra_priv_t;

static extra_priv_t privs[TELAS_MAX];
static int npriv;

static uint16_t ler(volatile uint8_t *m, uint16_t reg) { return *(volatile uint16_t *)(m + MMIO_DISPI + reg * 2); }
static void gravar(volatile uint8_t *m, uint16_t reg, uint16_t v) { *(volatile uint16_t *)(m + MMIO_DISPI + reg * 2) = v; }

/* Programa un modo de 32 bits; devuelve el pitch o 0 si la tarjeta no lo aceptó. */
static uint32_t programar(volatile uint8_t *m, uint32_t l, uint32_t a)
{
    gravar(m, REG_LIGAR, 0);
    gravar(m, REG_XRES, (uint16_t)l);
    gravar(m, REG_YRES, (uint16_t)a);
    gravar(m, REG_BPP, 32);
    gravar(m, REG_LARG_VIRT, (uint16_t)l);
    gravar(m, REG_ALT_VIRT, (uint16_t)a);
    gravar(m, REG_X, 0);
    gravar(m, REG_Y, 0);
    gravar(m, REG_LIGAR, LIGADO | COM_LFB);
    if (ler(m, REG_XRES) != l || ler(m, REG_YRES) != a || ler(m, REG_BPP) != 32 || !(ler(m, REG_LIGAR) & LIGADO))
        return 0;
    return (uint32_t)ler(m, REG_LARG_VIRT) * 4;
}

static bool mudar_modo(saida_t *s, uint32_t l, uint32_t a)
{
    extra_priv_t *p = s->priv;
    if (!l || !a || l > s->max_l || a > s->max_a || (l & 7)) return false;
    uint32_t pitch = programar(p->mmio, l, a);
    if (!pitch || pitch * a > s->memoria) {   /* vuelve al modo de antes */
        s->pitch = programar(p->mmio, s->largura, s->altura);
        return false;
    }
    s->pitch = pitch; s->largura = l; s->altura = a;
    memset(s->fb, 0, pitch * a);
    return true;
}

int bga_extra_detectar(void)
{
    pci_disp_t v[8];
    int n = 0, achadas = 0;
    uint32_t l0 = video_ativo() ? video_largura() : 1024, a0 = video_ativo() ? video_altura() : 768;
    for (int sub = 0; sub < 2; sub++) {   /* 0x0300: VGA; 0x0380: otro controlador de pantalla (secondary-vga, bochs-display) */
        n = pci_procurar_classe(0x03, sub ? 0x80 : 0x00, v, 8);
        for (int i = 0; i < n && npriv < TELAS_MAX - 1; i++) {
            pci_disp_t *d = &v[i];
            if (d->fabricante != 0x1234 || d->modelo != 0x1111) continue;
            uint32_t bar0 = pci_le32(d, 0x10), bar2 = pci_le32(d, 0x18);
            klog("[VIDEO] candidata a salida extra %02x:%x.%x: BAR0 %x BAR2 %x\n", d->barramento, d->dispositivo, d->funcao, bar0, bar2);
            if ((bar0 & 1) || (bar2 & 1) || !(bar0 & ~0xFu) || !(bar2 & ~0xFu)) continue;
            uint32_t fb_fis = bar0 & ~0xFu;
            if (fb_fis == info_boot.vesa_fb) continue;   /* la tarjeta del arranque: la maneja bga.c */
            uint32_t cmd = pci_le32(d, 4) & 0xFFFF;
            pci_escreve32(d, 4, cmd | 0x2);              /* decodifica memoria */
            uint32_t mm = janela_mapear(bar2 & ~0xFu, 0x1000);
            if (!mm) { klog("[VIDEO] salida extra: no se pudo mapear el BAR 2 (%x)\n", bar2); continue; }
            volatile uint8_t *m = (volatile uint8_t *)mm;
            uint16_t id = ler(m, REG_ID);
            if (id < 0xB0C0 || id > 0xB0C5) { klog("[VIDEO] tarjeta extra %x:%x sin dispi (id %x)\n", d->fabricante, d->modelo, id); continue; }
            uint16_t lig = ler(m, REG_LIGAR);
            gravar(m, REG_LIGAR, lig | CAPACIDADES);
            uint32_t max_l = ler(m, REG_XRES), max_a = ler(m, REG_YRES);
            gravar(m, REG_LIGAR, lig);
            if (!max_l || !max_a || max_l == 0xFFFF) { max_l = 4096; max_a = 4096; }   /* bochs-display no responde a «capacidades»: valen la memoria y la lectura de vuelta */
            uint16_t m64 = id >= 0xB0C2 ? ler(m, REG_MEM_64K) : 0;
            uint32_t mem = (m64 && m64 != 0xFFFF) ? (uint32_t)m64 << 16 : 0;
            if (!mem || !max_l || !max_a) { klog("[VIDEO] salida extra: memoria %u, maximo %ux%u\n", mem, max_l, max_a); continue; }
            uint32_t fb = wc_mapear(fb_fis, mem);
            if (!fb) { klog("[VIDEO] salida extra: no se pudo mapear el framebuffer\n"); continue; }
            saida_t s;
            memset(&s, 0, sizeof s);
            snformatar(s.nome, sizeof s.nome, "Bochs %02x:%x.%x", d->barramento, d->dispositivo, d->funcao);
            s.fb = (uint8_t *)fb;
            s.max_l = max_l; s.max_a = max_a; s.memoria = mem;
            s.mudar_modo = mudar_modo;
            privs[npriv].mmio = m;
            s.priv = &privs[npriv];
            /* Modo inicial: el de la principal si lo acepta, si no 1024x768. */
            uint32_t p = programar(m, l0, a0);
            if (p && p * a0 <= mem && l0 <= max_l && a0 <= max_a) { s.largura = l0; s.altura = a0; s.pitch = p; }
            else {
                p = programar(m, 1024, 768);
                if (!p || p * 768 > mem) { klog("[VIDEO] salida extra: rechaza 1024x768\n"); continue; }
                s.largura = 1024; s.altura = 768; s.pitch = p;
            }
            memset(s.fb, 0, s.pitch * s.altura);
            if (telas_registrar_saida(&s) < 0) { klog("[VIDEO] salida extra: sin hueco en la tabla\n"); continue; }
            npriv++;
            achadas++;
            klog("[VIDEO] tarjeta Bochs extra %s: dispi %x, maximo %ux%u, %u KB, modo %ux%u\n", s.nome, id, max_l, max_a, mem >> 10,
                 s.largura, s.altura);
        }
    }
    return achadas;
}
