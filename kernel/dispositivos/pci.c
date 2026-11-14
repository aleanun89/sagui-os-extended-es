// SPDX-License-Identifier: GPL-3.0-only
/* pci.c - el bus PCI (H29).
 *
 * Cada función de cada dispositivo tiene 256 bytes de configuración: quién lo fabricó,
 * qué tipo de cosa es (clase, subclase, interfaz) y dónde están sus
 * registros (BARs). El acceso es por el "mecanismo 1" de la especificación PCI:
 * se escribe la dirección (bus, dispositivo, función, registro) en el
 * puerto 0xCF8 y se lee el valor en el 0xCFC. */
#include "pci.h"
#include "portas.h"

static uint32_t endereco(uint8_t b, uint8_t d, uint8_t f, uint8_t reg)
{
    return 0x80000000u | ((uint32_t)b << 16) | ((uint32_t)d << 11) | ((uint32_t)f << 8) | (reg & 0xFC);
}

static uint32_t ler(uint8_t b, uint8_t d, uint8_t f, uint8_t reg)
{
    porta_escreve32(0xCF8, endereco(b, d, f, reg));
    return porta_le32(0xCFC);
}

uint32_t pci_le32(const pci_disp_t *d, uint8_t reg)
{
    return ler(d->barramento, d->dispositivo, d->funcao, reg);
}

void pci_escreve32(const pci_disp_t *d, uint8_t reg, uint32_t v)
{
    porta_escreve32(0xCF8, endereco(d->barramento, d->dispositivo, d->funcao, reg));
    porta_escreve32(0xCFC, v);
}

static int procurar(uint8_t classe, uint8_t subclasse, int interface, pci_disp_t *achados, int max)
{
    int n = 0;
    for (int b = 0; b < 256; b++)
        for (int d = 0; d < 32; d++) {
            uint32_t id = ler((uint8_t)b, (uint8_t)d, 0, 0);
            if ((id & 0xFFFF) == 0xFFFF) continue;                    /* nadie aquí */
            bool multifuncao = (ler((uint8_t)b, (uint8_t)d, 0, 0x0C) >> 16) & 0x80;
            for (int f = 0; f < (multifuncao ? 8 : 1); f++) {
                uint32_t idf = ler((uint8_t)b, (uint8_t)d, (uint8_t)f, 0);
                if ((idf & 0xFFFF) == 0xFFFF) continue;
                uint32_t cl = ler((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x08);
                uint8_t pi = (uint8_t)(cl >> 8);
                if ((cl >> 24) == classe && ((cl >> 16) & 0xFF) == subclasse && (interface < 0 || pi == interface) && n < max)
                    achados[n++] = (pci_disp_t){ (uint8_t)b, (uint8_t)d, (uint8_t)f, (uint16_t)idf, (uint16_t)(idf >> 16),
                                                 classe, subclasse, pi };
            }
        }
    return n;
}

/* Lista todas las funciones presentes (para el infopc, D041). Solo lectura. */
int pci_listar(pci_disp_t *v, int max)
{
    int n = 0;
    for (int b = 0; b < 256; b++)
        for (int d = 0; d < 32; d++) {
            uint32_t id = ler((uint8_t)b, (uint8_t)d, 0, 0);
            if ((id & 0xFFFF) == 0xFFFF) continue;
            bool multifuncao = (ler((uint8_t)b, (uint8_t)d, 0, 0x0C) >> 16) & 0x80;
            for (int f = 0; f < (multifuncao ? 8 : 1); f++) {
                uint32_t idf = ler((uint8_t)b, (uint8_t)d, (uint8_t)f, 0);
                if ((idf & 0xFFFF) == 0xFFFF) continue;
                uint32_t cl = ler((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x08);
                if (n < max)
                    v[n] = (pci_disp_t){ (uint8_t)b, (uint8_t)d, (uint8_t)f, (uint16_t)idf, (uint16_t)(idf >> 16),
                                         (uint8_t)(cl >> 24), (uint8_t)(cl >> 16), (uint8_t)(cl >> 8) };
                n++;
            }
        }
    return n < max ? n : max;
}

int pci_procurar(uint8_t classe, uint8_t subclasse, uint8_t interface, pci_disp_t *achados, int max)
{
    return procurar(classe, subclasse, interface, achados, max);
}

/* Cualquier interfaz (el IDE cambia la interfaz según el modo de cada canal). */
int pci_procurar_classe(uint8_t classe, uint8_t subclasse, pci_disp_t *achados, int max)
{
    return procurar(classe, subclasse, -1, achados, max);
}
