// SPDX-License-Identifier: GPL-2.0-or-later
/* bateria.h - uACPI, el controlador embebido y la batería, vistos desde el resto
 * del kernel (D192). Sin tipos de uACPI: quien necesita el intérprete incluye
 * uacpi_sagui.h. */
#ifndef SAGUI_BATERIA_H
#define SAGUI_BATERIA_H
#include "tipos.h"
#include "abi.h"

/* Después de acpi_iniciar(): crea la tarea "acpi-aml", que (si se puede)
 * arranca uACPI, el controlador embebido y la batería. Nunca bloquea el
 * arranque: todo pasa en esa tarea. */
void uacpi_sagui_iniciar(void);

/* Copia de lo último que se leyó de la batería (para la barra de tareas).
 * Devuelve true si hay al menos una batería puesta. */
bool bateria_estado(info_bateria_t *b);

/* INFO_BATERIA */
int32_t bateria_info(uint32_t ptr, uint32_t n);

#endif
