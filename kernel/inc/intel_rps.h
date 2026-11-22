// SPDX-License-Identifier: GPL-2.0-or-later
/* intel_rps.h - la frecuencia de la GPU Intel Gen9 mientras trabajan el 2D (acel_intel_exec.c, D434) y el 3D
 * (gpu3d_intel9.c, D438/D448): un gobernador común (kernel/dispositivos/intel_rps.c). */
#ifndef SAGUI_INTEL_RPS_H
#define SAGUI_INTEL_RPS_H
#include "tipos.h"

#define INTEL_RPS_2D 1u
#define INTEL_RPS_3D 2u

/* Un usuario más (INTEL_RPS_*) con su ventana del MMIO (al menos 2 MB desde el BAR 0). La primera vez se lee RP0 y lo
 * que había pedido el firmware y se crea la tarea «intel-rps». false: no se toca la frecuencia (RP0 fuera de 50 MHz a
 * 2 GHz, sin TSC o sin tarea). */
bool intel_rps_ligar(volatile uint8_t *mmio, uint32_t quem);
/* Hay trabajo para la GPU: se pide RP0 si no estaba pedido. Barato (una comparación); se puede llamar con las
 * interrupciones cerradas. */
void intel_rps_trabalho(void);
/* Ese usuario deja la GPU; sin ninguno, se vuelve a lo que había pedido el firmware. */
void intel_rps_soltar(uint32_t quem);
/* Para los «--detalle»: frecuencia, si está en RP0, cuántas subidas, quién la usa. */
void intel_rps_texto(char *b, int n);
/* RP0 en MHz (0: sin tocar). */
uint32_t intel_rps_rp0_mhz(void);

#endif
