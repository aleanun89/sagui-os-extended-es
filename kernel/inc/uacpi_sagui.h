// SPDX-License-Identifier: GPL-2.0-or-later
/* uacpi_sagui.h - lo que comparten la capa de uACPI (uacpi_sagui.c), el
 * controlador embebido (ec.c) y la batería (bateria.c). D192.
 *
 * Todo esto corre en una sola tarea del kernel, "acpi-aml", en la CPU 0: es
 * la única que llama a uACPI. El SCI (una IRQ) también entra en uACPI, pero
 * solo para los eventos fijos (botón, cerrojo global). */
#ifndef SAGUI_UACPI_SAGUI_H
#define SAGUI_UACPI_SAGUI_H
#include "tipos.h"
#include <uacpi/uacpi.h>
#include <uacpi/namespace.h>
#include <uacpi/utilities.h>
#include <uacpi/opregion.h>
#include <uacpi/notify.h>
#include <uacpi/event.h>
#include <uacpi/resources.h>

/* uacpi_sagui.c */
void uacpi_sagui_dormir_ms(uint32_t ms);          /* duerme la tarea (no gira) */
void uacpi_sagui_trabalho_pendente(void);         /* corre las notificaciones encoladas */

/* ec.c: el controlador embebido (PNP0C09) por sondeo, sin su GPE. */
bool     ec_iniciar(void);                 /* busca el EC e instala su manejador (antes de _INI) */
void     ec_atender_eventos(void);         /* SCI_EVT: consulta (0x84) y ejecuta _Qxx */
bool     ec_presente(void);
uint32_t ec_consultas(void);

/* bateria.c */
void bateria_iniciar(void);                /* busca PNP0C0A y ACPI0003 (después de _INI) */
void bateria_ciclo(void);                  /* lee _STA/_BIF/_BST/_PSR si toca */
void bateria_uacpi_estado(uint32_t estado, const char *motivo);   /* UACPI_* para INFO_BATERIA */

#endif
