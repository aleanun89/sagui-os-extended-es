// SPDX-License-Identifier: GPL-2.0-or-later
/* ata.h - discos internos ATA/IDE por PIO (D040) y SATA/AHCI (D155). */
#ifndef SAGUI_ATA_H
#define SAGUI_ATA_H

int ata_iniciar(void);            /* busca los discos y registra cada uno ("hda", "hdb"...); devuelve cuántos */
int ata_ahci_sem_suporte(void);   /* cuántos controladores AHCI siguen sin soporte (0 si el driver funciona) */
int ata_ahci_quantidade(void);    /* cuántos controladores AHCI se han visto en el PCI */

#endif
