// SPDX-License-Identifier: GPL-2.0-or-later
/* aleatorio.h - generador de números aleatorios del kernel (D061). */
#ifndef SAGUI_ALEATORIO_H
#define SAGUI_ALEATORIO_H
#include "tipos.h"

/* Detecta RDRAND/RDSEED y siembra el reservorio (TSC, reloj, jitter). */
void aleatorio_iniciar(void);
/* Lo llama el despachador en cada IRQ (solo guarda el TSC en un anillo: es barato). */
void aleatorio_evento(uint32_t dado);
/* Mezcla bytes venidos de fuera (escribir en /dev/urandom). */
void aleatorio_misturar(const void *dados, uint32_t n);
/* n bytes impredecibles. Llamar con las interrupciones desactivadas. */
void aleatorio_bytes(void *destino, uint32_t n);
uint32_t aleatorio_u32(void);

#endif
