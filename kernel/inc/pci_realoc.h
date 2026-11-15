// SPDX-License-Identifier: GPL-2.0-or-later
/* pci_realoc.h - recolocar por debajo de 4 GB una BAR de 64 bits que el firmware dejó arriba (D283). */
#ifndef SAGUI_PCI_REALOC_H
#define SAGUI_PCI_REALOC_H
#include "pci.h"

/* Si la BAR de 64 bits "indice" de d está por encima de 4 GB, busca un hueco libre por debajo y la
 * reescribe. Devuelve la nueva dirección, o 0 si no se pudo (sin hueco, detrás de un puente) o si no
 * hacía falta. Avisa por el log. "quem" es el nombre que sale en los avisos. */
uint32_t pci_realoc_bar_baixo(const pci_disp_t *d, int indice, const char *quem);

/* Algoritmo puro (para la autoprueba): "ocup" es un vector de n pares {ini, fim} de uint64_t. */
uint32_t pci_realoc_hueco(const void *ocup, int n, uint64_t tam, uint64_t tope, uint64_t piso);

#endif
