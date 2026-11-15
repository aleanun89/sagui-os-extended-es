// SPDX-License-Identifier: GPL-2.0-or-later
/* pci_realoc.c - baja por debajo de 4 GB una BAR de 64 bits que el firmware dejó arriba (D283).
 *
 * El kernel es de 32 bits sin PAE: no puede mapear una BAR por encima de 4 GB. OVMF (UEFI) coloca
 * ahí las BAR de 64 bits de los dispositivos del bus raíz (el NVMe de QEMU, por ejemplo). Esta pieza
 * busca un hueco libre por debajo de 4 GB y reescribe la BAR.
 *
 * Qué se considera ocupado (y por tanto no se toca):
 *   - todo el mapa E820 (RAM y zonas reservadas: MMCONFIG, ACPI, firmware);
 *   - las BAR de memoria de todas las funciones PCI que ya están por debajo de 4 GB;
 *   - las ventanas de memoria y prefetchable de los puentes PCI;
 *   - 0xFEC00000 en adelante (IOAPIC, HPET, LAPIC, flash del firmware).
 * El hueco se busca de arriba abajo desde 0xFC000000 (donde acaban las ventanas de casi todos los
 * firmwares), alineado al tamaño de la BAR, y nunca por debajo de la RAM (E820 lo cubre).
 *
 * Solo se mueven BAR de dispositivos del bus 0 (cuelgan del puente raíz): detrás de un puente PCIe la
 * BAR tiene que caer dentro de la ventana del puente, que no se reprograma aquí. Un NVMe M.2 real es
 * no prefetchable y su BAR ya está por debajo de 4 GB. No se lee el _CRS del puente raíz (uACPI): el
 * hueco se deduce de lo anterior; si no hay hueco, se avisa y no se toca nada.
 *
 * Las funciones del algoritmo (hueco_libre) no tocan el hardware: se prueban en la autoprueba. */
#include "pci_realoc.h"
#include "bootinfo.h"
#include "console.h"
#include "pci.h"
#include "texto.h"

#define PCI_COMANDO   0x04
#define REALOC_TOPE   0xFC000000ull   /* se busca hacia abajo desde aquí */
#define REALOC_FIJO   0xFEC00000ull   /* IOAPIC, HPET, LAPIC, flash: ocupado hasta 4 GB */
#define REALOC_MAX_PCI 96
#define REALOC_MAX_OCUP 256

typedef struct { uint64_t ini, fim; } rango_t;   /* [ini, fim) */

static bool solapa(uint64_t a0, uint64_t a1, uint64_t b0, uint64_t b1) { return a0 < b1 && b0 < a1; }

/* Primer hueco libre de "tam" bytes (potencia de 2), buscando hacia abajo desde "tope", sin bajar de
 * "piso"; 0 si no hay. "ocup" son los rangos ocupados. Puro: probado en la autoprueba. */
uint32_t pci_realoc_hueco(const void *ocup_v, int n, uint64_t tam, uint64_t tope, uint64_t piso)
{
    const rango_t *ocup = ocup_v;
    if (!tam || (tam & (tam - 1))) return 0;
    uint64_t c = (tope - tam) & ~(tam - 1);
    for (;;) {
        if (c < piso || c + tam > tope) return 0;
        bool libre = true;
        uint64_t salta = c;
        for (int i = 0; i < n; i++)
            if (solapa(c, c + tam, ocup[i].ini, ocup[i].fim)) {
                libre = false;
                if (ocup[i].ini < salta) salta = ocup[i].ini;   /* saltar por debajo de lo que estorba */
            }
        if (libre) return (uint32_t)c;
        if (salta == c) { if (c < tam) return 0; c -= tam; }
        else { if (salta < tam) return 0; c = (salta - tam) & ~(tam - 1); }
    }
}

/* Tamaño de la BAR en "reg" (con el truco de escribir unos, como bar.c); 0 si no existe. */
static uint64_t tam_bar(const pci_disp_t *d, uint8_t reg, bool par64)
{
    uint32_t o = pci_le32(d, reg), oa = par64 ? pci_le32(d, (uint8_t)(reg + 4)) : 0;
    uint32_t cmd = pci_le32(d, PCI_COMANDO);
    pci_escreve32(d, PCI_COMANDO, cmd & ~0x7u);
    pci_escreve32(d, reg, 0xFFFFFFFFu);
    if (par64) pci_escreve32(d, (uint8_t)(reg + 4), 0xFFFFFFFFu);
    uint32_t b = pci_le32(d, reg), a = par64 ? pci_le32(d, (uint8_t)(reg + 4)) : 0xFFFFFFFFu;
    pci_escreve32(d, reg, o);
    if (par64) pci_escreve32(d, (uint8_t)(reg + 4), oa);
    pci_escreve32(d, PCI_COMANDO, cmd);
    uint64_t m = ((uint64_t)a << 32) | (b & ~0xFu);
    return m ? (~m) + 1 : 0;
}

/* Reúne lo ocupado por debajo de 4 GB, sin contar la BAR "yo" (reg yo_reg de d). */
static int reunir(rango_t *v, int max, const pci_disp_t *yo, uint8_t yo_reg)
{
    int n = 0;
    for (uint32_t i = 0; i < info_boot.mapa_qtd && i < MAPA_MAX && n < max; i++) {
        const regiao_e820_t *r = &mapa_memoria[i];
        if (r->base >= 0x100000000ull || !r->tamanho) continue;
        uint64_t f = r->base + r->tamanho;
        v[n++] = (rango_t){ r->base, f > 0x100000000ull ? 0x100000000ull : f };
    }
    if (n < max) v[n++] = (rango_t){ REALOC_FIJO, 0x100000000ull };
    static pci_disp_t lista[REALOC_MAX_PCI];
    int q = pci_listar(lista, REALOC_MAX_PCI);
    for (int i = 0; i < q && n < max; i++) {
        const pci_disp_t *d = &lista[i];
        uint32_t cab = (pci_le32(d, 0x0C) >> 16) & 0x7F;
        int nbar = cab == 0 ? 6 : cab == 1 ? 2 : 0;
        for (int b = 0; b < nbar && n < max; b++) {
            uint8_t reg = (uint8_t)(0x10 + 4 * b);
            if (d->barramento == yo->barramento && d->dispositivo == yo->dispositivo && d->funcao == yo->funcao && reg == yo_reg) {
                if ((pci_le32(d, reg) & 6) == 4) b++;
                continue;
            }
            uint32_t v0 = pci_le32(d, reg);
            if (v0 & 1) continue;
            bool par64 = (v0 & 6) == 4;
            uint64_t base = v0 & ~0xFull;
            if (par64) { base |= (uint64_t)pci_le32(d, (uint8_t)(reg + 4)) << 32; b++; }
            if (!base || base >= 0x100000000ull) continue;
            uint64_t t = tam_bar(d, reg, par64);
            if (t) v[n++] = (rango_t){ base, base + t };
        }
        if (cab == 1) {   /* ventanas del puente: memoria (0x20) y prefetchable (0x24, parte de 32 bits) */
            for (uint8_t r = 0x20; r <= 0x24 && n < max; r += 4) {
                uint32_t w = pci_le32(d, r);
                uint64_t b0 = (uint64_t)(w & 0xFFF0u) << 16, l = ((uint64_t)(w >> 16) << 16) | 0xFFFFFu;
                if (b0 <= l && w != 0 && w != 0xFFFFFFFFu) v[n++] = (rango_t){ b0, l + 1 };
            }
        }
    }
    return n;
}

uint32_t pci_realoc_bar_baixo(const pci_disp_t *d, int indice, const char *quem)
{
    uint8_t reg = (uint8_t)(0x10 + 4 * indice);
    uint32_t b0 = pci_le32(d, reg);
    if ((b0 & 1) || (b0 & 6) != 4) return 0;
    uint32_t alto = pci_le32(d, (uint8_t)(reg + 4));
    if (!alto) return 0;                  /* ya está por debajo de 4 GB */
    if (d->barramento != 0) {
        klog("[PCI] %s: BAR%d por encima de 4 GB y el dispositivo está detrás de un puente: no se recoloca\n", quem, indice);
        return 0;
    }
    uint64_t t = tam_bar(d, reg, true);
    if (!t || t > 0x10000000ull) { klog("[PCI] %s: tamaño de BAR%d no válido\n", quem, indice); return 0; }
    static rango_t ocup[REALOC_MAX_OCUP];
    int n = reunir(ocup, REALOC_MAX_OCUP, d, reg);
    uint32_t nuevo = pci_realoc_hueco(ocup, n, t, REALOC_TOPE, 0x10000000ull);
    if (!nuevo) { klog("[PCI] %s: BAR%d por encima de 4 GB y no hay hueco libre por debajo de 4 GB\n", quem, indice); return 0; }
    uint32_t cmd = pci_le32(d, PCI_COMANDO);
    pci_escreve32(d, PCI_COMANDO, cmd & ~0x7u);
    pci_escreve32(d, (uint8_t)(reg + 4), 0);
    pci_escreve32(d, reg, nuevo | (b0 & 0xFu));
    pci_escreve32(d, PCI_COMANDO, cmd);
    uint32_t leido = pci_le32(d, reg) & ~0xFu;
    if (leido != nuevo || pci_le32(d, (uint8_t)(reg + 4)) != 0) {
        klog("[PCI] %s: BAR%d no aceptó la nueva dirección (%x)\n", quem, indice, leido);
        return 0;
    }
    klog("[PCI] %s: BAR%d recolocada de %x:%x (alto:bajo) a %x (%u KB)\n", quem, indice, alto, b0 & ~0xFu, nuevo, (uint32_t)(t >> 10));
    return nuevo;
}
