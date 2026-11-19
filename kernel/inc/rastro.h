// SPDX-License-Identifier: GPL-2.0-or-later
/* rastro.h - marcas de depuración que sobreviven a un reinicio en caliente (Amilo, dbg5, 2026-11-19).
 *
 * Una página de RAM fija (la última libre del E820 bajo 4 GB menos 64 KB; la misma en cada arranque de la
 * misma máquina), apartada del gestor de memoria y escrita SIN caché: lo escrito llega a la RAM antes de
 * seguir. En un reinicio en caliente la RAM suele conservarse; al arrancar se lee y se escribe en el log
 * («[RASTRO] marcas del arranque anterior: ...») antes de empezar de nuevo. Además se apuntan las causas de
 * reinicio que dice el chipset Intel (TCO del ICH) y, en una excepción del kernel (NMI incluido), el vector,
 * el EIP y los bits de error del PCI. */
#ifndef SAGUI_RASTRO_H
#define SAGUI_RASTRO_H
#include "tipos.h"

uint64_t rastro_pagina(void);            /* fisica.c: la elegida y apartada en memfisica_iniciar (0 si ninguna) */
void rastro_iniciar(void);               /* con la paginación: lee lo del arranque anterior y empieza */
void rastro_marca(uint32_t v);           /* apunta un valor (código de marca) */
void rastro_excecao(uint32_t vetor, uint32_t erro, uint32_t eip);   /* excecao_fatal */
void rastro_mce_secundaria(void);        /* CR4.MCE en un procesador secundario (ver rastro.c) */
void rastro_pci_erros_log(const char *quando);   /* los bits de error del PCI de todo el bus, al log */

/* Códigos: 0x0100 | k para las marcas de gart_encender (vmarca k); 0x10000 | fila << 12 | col << 4 | estado
 * para las finas; 0x200000 | paso << 12 | franja << 4 | fase para los experimentos («acel2d volcado paso 7-16»:
 * fase 0 antes de la orden, 1 orden puesta, 2 espera terminada, 3 plazo agotado, 4 comparada igual, 5 distinta);
 * 0xE0000000 | vector para una excepción; 0xF... para valores con etiqueta (0x30/0x31 páginas de una región,
 * 0x40 opciones de depuración, 0x50 error del PCI, 0x61 puerto 61h en un NMI, 0x70 banco del MCE). */
#define RASTRO_VALOR(etq) (0xF0000000u | ((uint32_t)(etq) << 16))

#endif
