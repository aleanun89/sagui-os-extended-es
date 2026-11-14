// SPDX-License-Identifier: GPL-2.0-or-later
/* info_hardware.h - datos del hardware para el infopc (D041). */
#ifndef SAGUI_INFO_HARDWARE_H
#define SAGUI_INFO_HARDWARE_H
#include "tipos.h"

void    hw_capturar_boot(void);   /* VBE y SMBIOS: llamar antes de activar la paginación */
void    hw_medir_tsc(void);       /* después de activar el reloj y las interrupciones */
void    arranque_marca(const char *fase);   /* D201: tiempo de una fase del arranque */
uint32_t hw_tsc_khz(void);      /* 0 = sin medida (la red lo usa para el tiempo del ping) */
uint32_t hw_video_memoria_kb(void);   /* memoria de vídeo anunciada por el VBE */
void    hw_video_atualizar(uint32_t modo);   /* después de cambiar de modo (D059) */
#include "abi.h"
const info_acpi_t *hw_acpi(void);   /* las tablas encontradas en el arranque (D076) */
int32_t hw_info(uint32_t tipo, uint32_t ptr, uint32_t n);   /* INFO_E820..INFO_CPU_MHZ */

#endif
