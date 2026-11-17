// SPDX-License-Identifier: GPL-2.0-or-later
/* disco_ram.h - "ram0": la copia en la RAM de un volumen (D379, toram). Ver kernel/fs/disco_ram.c. */
#ifndef SAGUI_DISCO_RAM_H
#define SAGUI_DISCO_RAM_H
#include "disco.h"

/* Copia el volumen entero a la RAM y lo registra como disco ("ram0"). NULL y *erro si no
 * cabe (con una reserva para el sistema) o falla la lectura: no queda nada a medias. */
disco_t *disco_ram_copiar(disco_t *origem, int *erro);

#endif
