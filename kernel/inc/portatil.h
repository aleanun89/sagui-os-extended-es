// SPDX-License-Identifier: GPL-2.0-or-later
/* portatil.h - lo propio de los portátiles visto desde el resto del kernel (D403).
 *
 * Sin tipos de uACPI: el brillo, los avisos del firmware y el volcado de las
 * tablas los atiende portatil.c junto con video_acpi.c, fujitsu.c y toshiba.c,
 * en la tarea "acpi-aml" (la única que llama al intérprete). Desde fuera solo
 * se piden cosas; la tarea las hace cuando despierta. */
#ifndef SAGUI_PORTATIL_H
#define SAGUI_PORTATIL_H
#include "tipos.h"
#include "abi.h"

/* SIS_PORTATIL (PORTATIL_* de abi.h). */
int32_t sis_portatil(uint32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t d);

/* Pide un cambio de brillo (PORTATIL_BRILHO_FIXAR o _PASSO) sin esperar: lo
 * hace la tarea "acpi-aml", que deja el aviso en pantalla. ERRO_NAO_EXISTE si
 * no hay con qué (sin --uacpi o sin dispositivo de brillo). Vale desde una IRQ. */
int32_t portatil_brilho_pedir(int modo, int valor, int origem);

/* El modo avión cambió: la luz de la radio de los Fujitsu que la tienen. */
void portatil_radio_mudou(bool apagada);

/* Despierta la tarea "acpi-aml" (uacpi_sagui.c). Vale desde una IRQ. */
void uacpi_sagui_acordar(void);

#endif
