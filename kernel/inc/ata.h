// SPDX-License-Identifier: GPL-2.0-or-later
/* ata.h - discos internos ATA/IDE por PIO (D040) y SATA/AHCI (D155). */
#ifndef SAGUI_ATA_H
#define SAGUI_ATA_H
#include "tipos.h"

int ata_iniciar(void);
void ata_modo_assincrono(bool ligado);   /* D281: con planificador e IRQ, el DMA duerme esperando en vez de sondear */
uint32_t ata_cesiones(void);
void ata_estatisticas(uint32_t *ordens, uint32_t *setores, uint32_t *irqs, uint32_t *falhas, uint32_t *tsc_ms);            /* busca los discos y registra cada uno ("hda", "hdb"...); devuelve cuántos */
int ata_ahci_sem_suporte(void);   /* cuántos controladores AHCI siguen sin soporte (0 si el driver funciona) */
int ata_ahci_quantidade(void);    /* cuántos controladores AHCI se han visto en el PCI */

#endif
