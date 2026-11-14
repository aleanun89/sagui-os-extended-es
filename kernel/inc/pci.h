// SPDX-License-Identifier: GPL-3.0-only
/* pci.h - acceso al espacio de configuración PCI (mecanismo 1, puertos 0xCF8/0xCFC). */
#ifndef SAGUI_PCI_H
#define SAGUI_PCI_H
#include "tipos.h"

typedef struct {
    uint8_t  barramento, dispositivo, funcao;
    uint16_t fabricante, modelo;
    uint8_t  classe, subclasse, interface;
} pci_disp_t;

uint32_t pci_le32(const pci_disp_t *d, uint8_t reg);
void     pci_escreve32(const pci_disp_t *d, uint8_t reg, uint32_t v);
/* Busca dispositivos de una clase/subclase/interfaz; devuelve cuántos encontró. */
int      pci_procurar(uint8_t classe, uint8_t subclasse, uint8_t interface, pci_disp_t *achados, int max);
/* Todas las funciones presentes, hasta max (infopc, D041). */
int      pci_listar(pci_disp_t *v, int max);
int      pci_procurar_classe(uint8_t classe, uint8_t subclasse, pci_disp_t *achados, int max);

#endif
