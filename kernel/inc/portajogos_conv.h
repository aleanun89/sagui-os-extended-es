// SPDX-License-Identifier: GPL-2.0-or-later
/* portajogos_conv.h - las cuentas del puerto de juegos (D307), sin tocar el hardware.
 *
 * El puerto de juegos del PC (0x201; IBM PC Technical Reference, «Game Control Adapter»,
 * y el de las Sound Blaster de Creative, que es el mismo) mide cada eje con un monoestable:
 * al escribir en el puerto, los bits 0-3 suben y cada uno baja al cabo de
 *     t = 24,2 us + 0,011 us/ohmio * R
 * (R = resistencia del potenciómetro, 0-100 kohmios: de 24 us a ~1,1 ms). Los bits 4-7 son
 * los cuatro botones, a 0 cuando están pulsados. Aquí: el paso de ciclos del TSC a décimas
 * de microsegundo, la normalización a -32768..32767 con la calibración (mínimo, centro,
 * máximo) y la calibración automática. kernel/dispositivos/portajogos.c hace la lectura;
 * tests/h307_mandos.py compila este archivo en el anfitrión (-DSAGUI_HOST). */
#ifndef SAGUI_PORTAJOGOS_CONV_H
#define SAGUI_PORTAJOGOS_CONV_H
#ifdef SAGUI_HOST
#include <stdint.h>
#include <stdbool.h>
#else
#include "tipos.h"
#endif

#define PJ_EIXOS        4
#define PJ_SEM_EIXO     0xFFFFu      /* el bit no bajó antes del plazo: no hay eje */
#define PJ_PRAZO_US     3000u        /* más de 3 ms: eje ausente (100 kohmios son ~1,1 ms) */

typedef struct {
    uint16_t min, centro, max;       /* décimas de microsegundo */
} pj_cal_t;

/* Tiempo que se espera para una resistencia (décimas de us). */
uint32_t pj_tempo_esperado(uint32_t ohmios);
/* Ciclos del TSC -> décimas de microsegundo, con la frecuencia del TSC en kHz. */
uint32_t pj_decimas_us(uint32_t ciclos, uint32_t tsc_khz);
/* Tiempo medido -> -32768..32767 con la calibración (centro = 0). */
int16_t  pj_normalizar(uint32_t t, const pj_cal_t *c);
/* Calibración por defecto a partir de la primera lectura (el mando suelto, en el centro). */
void     pj_cal_inicial(uint32_t centro, pj_cal_t *c);
/* Calibración automática: amplía el mínimo y el máximo si la lectura se sale. */
void     pj_cal_ampliar(uint32_t t, pj_cal_t *c);
/* ¿Es razonable una calibración? (min < centro < max, sin ceros) */
bool     pj_cal_valida(const pj_cal_t *c);

#endif
