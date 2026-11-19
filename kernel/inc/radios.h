// SPDX-License-Identifier: GPL-2.0-or-later
/* radios.h - las radios y el panel de los HP (hp_wmi.c) y el volcado de tablas ACPI,
 * vistos desde las llamadas al sistema (D405). Sin tipos de uACPI. */
#ifndef SAGUI_RADIOS_H
#define SAGUI_RADIOS_H
#include "tipos.h"
#include "abi.h"

int32_t radios_info(uint32_t ptr, uint32_t n);    /* INFO_RADIOS */
int32_t radios_mudar(uint32_t valor);             /* INFO_RADIOS_MUDAR: radio | ligar << 8 */
int32_t acpi_tabela_ler(uint32_t ptr, uint32_t n);   /* INFO_ACPI_TABELA (info_hardware.c) */

#endif
