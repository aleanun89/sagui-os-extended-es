// SPDX-License-Identifier: GPL-2.0-or-later
/* frequencia.h - driver de frecuencia de la CPU (D106, mejora 061). */
#ifndef SAGUI_FREQUENCIA_H
#define SAGUI_FREQUENCIA_H
#include "tipos.h"
#include "abi.h"

/* Detecta el mecanismo, lee la configuración (cabecera de arranque,
 * /home/.frecuencia o /etc/frecuencia), aplica la política y arranca la
 * tarea que vigila la temperatura. Después de montar /home (fstab). */
void    frequencia_iniciar(void);
int32_t frequencia_info(uint32_t ptr, uint32_t n);   /* INFO_FREQUENCIA */
bool    frequencia_info_kernel(info_frequencia_t *f);   /* false = sin iniciar */
int32_t frequencia_info_nucleos(uint32_t ptr, uint32_t n);   /* INFO_FREQ_NUCLEOS (D175) */
/* El tic de cada CPU (D175), con las interrupciones apagadas: si la decisión
 * le dejó algo, escribe ella misma su registro de P-state. */
void    frequencia_tic_cpu(void);

/* Del monitor (kernel/dispositivos/sensores.c): la mayor temperatura de la
 * CPU, leída ahora, sin escribir nada. false = no hay lectura. */
bool sensores_temp_cpu(int *mc);

/* Byte de la cabecera de arranque (CAB_FREQ, boot/cabecalho.inc). */
#define CAB_FREQ               133
#define CAB_FREQ_DESATIVAR     1   /* mkimagem.py --sin-frecuencia */
#define CAB_FREQ_SIMULADA      2   /* mkimagem.py --frecuencia-simulada (pruebas) */

#endif
