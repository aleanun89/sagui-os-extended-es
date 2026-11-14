// SPDX-License-Identifier: GPL-3.0-only
/* som.h - el altavoz del PC (bonus B3). */
#ifndef SAGUI_SOM_H
#define SAGUI_SOM_H
#include "tipos.h"
#include "abi.h"

void som_tique(void);   /* llamado en cada tic del reloj: avanza la cola de notas */
void som_parar(void);
int32_t sis_bip(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

#endif
