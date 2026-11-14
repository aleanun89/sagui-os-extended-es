// SPDX-License-Identifier: GPL-2.0-or-later
/* aparencia.h - color de resalte del escritorio (D112). */
#ifndef SAGUI_APARENCIA_H
#define SAGUI_APARENCIA_H
#include "tipos.h"

uint32_t aparencia_acento(void);                     /* 0xRRGGBB */
uint32_t aparencia_escurecer(uint32_t cor, int pct); /* mezcla con negro */
uint32_t aparencia_clarear(uint32_t cor, int pct);   /* mezcla con blanco */
#endif
