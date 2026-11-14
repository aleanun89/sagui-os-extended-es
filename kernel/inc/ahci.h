// SPDX-License-Identifier: GPL-2.0-or-later
/* ahci.h - controlador SATA AHCI (D155). */
#ifndef SAGUI_AHCI_H
#define SAGUI_AHCI_H

/* Busca los controladores AHCI en el PCI, los enciende y registra cada disco
 * SATA que encuentre en la capa de discos ("hda", "hdb"...). Devuelve cuántos
 * discos ha registrado. */
int ahci_iniciar(void);

/* Cuántos controladores AHCI se han visto en el PCI. */
int ahci_controladores(void);

/* Cuántos controladores AHCI se han visto pero NO se han podido usar
 * (sin barra de memoria, sin puertos, etc.): los que siguen "sin soporte". */
int ahci_sem_suporte(void);

#endif
