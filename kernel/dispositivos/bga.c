// SPDX-License-Identifier: GPL-2.0-or-later
/* bga.c - tarjeta de vídeo de Bochs/QEMU ("dispi", BGA). Ver D059.
 *
 * Después del modo protegido no hay BIOS, así que el VBE ya no cambia de modo.
 * Las tarjetas virtuales de Bochs y de QEMU (-vga std, PCI 1234:1111) y la
 * VBoxVGA de VirtualBox (80EE:BEEF) tienen una interfaz propia, sencilla, por
 * dos puertos: 0x1CE elige el registro y 0x1CF lee o escribe el valor. El
 * VBE de la propia BIOS de ellas se hace encima de eso, así que el modo que
 * la etapa 2 lo activó y ya está en esos registros.
 *
 * Solo se intenta si el PCI muestra una de estas tarjetas: en el G470 (Intel HD) las
 * puertos 0x1CE/0x1CF ni se tocan. */
#include "bga.h"
#include "bootinfo.h"
#include "console.h"
#include "pci.h"
#include "portas.h"

#define BGA_INDICE   0x1CE
#define BGA_DADOS    0x1CF
#define REG_ID       0
#define REG_XRES     1
#define REG_YRES     2
#define REG_BPP      3
#define REG_LIGAR    4
#define REG_LARG_VIRT 6
#define REG_ALT_VIRT 7
#define REG_X        8
#define REG_Y        9
#define REG_MEM_64K  10     /* memoria de vídeo en bloques de 64 KB (QEMU) */
#define LIGADO       0x01
#define CAPACIDADES  0x02   /* con él activado, XRES/YRES/BPP devuelven los máximos */
#define COM_LFB      0x40

static bool presente;
static uint16_t id;
static uint32_t max_l, max_a, memoria;

static uint16_t ler(uint16_t reg)              { porta_escreve16(BGA_INDICE, reg); return porta_le16(BGA_DADOS); }
static void     gravar(uint16_t reg, uint16_t v) { porta_escreve16(BGA_INDICE, reg); porta_escreve16(BGA_DADOS, v); }

bool     bga_presente(void)     { return presente; }
uint32_t bga_memoria(void)      { return memoria; }
uint32_t bga_max_largura(void)  { return max_l; }
uint32_t bga_max_altura(void)   { return max_a; }

bool bga_detectar(uint32_t memoria_vbe)
{
    pci_disp_t v[8];
    int n = pci_procurar_classe(0x03, 0x00, v, 8);
    bool achou = false;
    for (int i = 0; i < n; i++)
        if ((v[i].fabricante == 0x1234 && v[i].modelo == 0x1111) || (v[i].fabricante == 0x80EE && v[i].modelo == 0xBEEF))
            achou = true;
    if (!achou) return false;
    id = ler(REG_ID);
    if (id < 0xB0C0 || id > 0xB0C5) return false;
    /* El modo actual tiene que ser el que activó el VBE por aquí; si no (otra tarjeta
     * respondiendo en los mismos puertos) no lo tocamos. */
    uint16_t ligar = ler(REG_LIGAR);
    if (!(ligar & LIGADO) || ler(REG_XRES) != info_boot.vesa_largura || ler(REG_YRES) != info_boot.vesa_altura)
        return false;
    gravar(REG_LIGAR, ligar | CAPACIDADES);
    max_l = ler(REG_XRES);
    max_a = ler(REG_YRES);
    gravar(REG_LIGAR, ligar);
    uint16_t m = id >= 0xB0C2 ? ler(REG_MEM_64K) : 0;
    memoria = (m && m != 0xFFFF) ? (uint32_t)m << 16 : memoria_vbe;
    presente = memoria != 0;
    klog("[VIDEO] BGA (dispi) id %x: maximo %ux%u, %u KB de memoria de video\n", id, max_l, max_a, memoria >> 10);
    return presente;
}

/* Cambia el modo; devuelve el pitch (bytes por línea) o 0 si la tarjeta no lo aceptó.
 * bpp: 32, 24, 16 o 15 (D078; 15 ocupa 2 bytes por píxel). */
static uint32_t programar(uint32_t l, uint32_t a, uint32_t bpp)
{
    gravar(REG_LIGAR, 0);
    gravar(REG_XRES, (uint16_t)l);
    gravar(REG_YRES, (uint16_t)a);
    gravar(REG_BPP, (uint16_t)bpp);
    gravar(REG_LARG_VIRT, (uint16_t)l);
    gravar(REG_ALT_VIRT, (uint16_t)a);
    gravar(REG_X, 0);
    gravar(REG_Y, 0);
    gravar(REG_LIGAR, LIGADO | COM_LFB);
    if (ler(REG_XRES) != l || ler(REG_YRES) != a || ler(REG_BPP) != bpp || !(ler(REG_LIGAR) & LIGADO))
        return 0;
    return (uint32_t)ler(REG_LARG_VIRT) * ((bpp + 7) / 8);
}

uint32_t bga_mudar(uint32_t l, uint32_t a, uint32_t bpp, uint32_t l_antes, uint32_t a_antes, uint32_t bpp_antes)
{
    if (!presente || l > max_l || a > max_a || (l & 7)) return 0;
    uint32_t pitch = programar(l, a, bpp);
    if (pitch && pitch * a <= memoria) return pitch;
    klog("[VIDEO] BGA recusou %ux%ux%u; voltando a %ux%u\n", l, a, bpp, l_antes, a_antes);
    programar(l_antes, a_antes, bpp_antes);
    return 0;
}
