// SPDX-License-Identifier: GPL-2.0-or-later
/* sensores.h - lo que el kernel pregunta al monitor de hardware (D092) sin pasar por la syscall. */
#ifndef SAGUI_SENSORES_H
#define SAGUI_SENSORES_H
#include "tipos.h"

/* Bandeja de la barra (D114): la temperatura más alta de la CPU en milésimas de
 * grado (false = no se puede leer: máquina virtual o sin sensor) y los bits
 * SENS_THR_* (0 si la fuente no informa). Lee a lo sumo una vez por segundo y,
 * si ya sabe que no hay sensor, no vuelve a tocar el hardware. */
bool sensores_temp_bandeja(int *mili_c, unsigned *throttling);

#endif
