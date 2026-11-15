// SPDX-License-Identifier: GPL-2.0-or-later
/* ym2413_vacio.c - YM2413 sin sonido (D321): ver plataforma/ym2413/audio/emu2413/emu2413.h. Solo guarda la
 * dirección del registro y los datos escritos, para que los juegos que sondean el chip no se cuelguen. */
#include "audio/emu2413/emu2413.h"

static YM2413_OPLL ctx;

void YM2413Init(void) { ctx.address = 0; ctx.status = 0; }
void YM2413ResetChip(void) { ctx.address = 0; ctx.status = 0; }
int YM2413Update(void) { return 0; }
void YM2413Write(unsigned int a, unsigned int v)
{
    if ((a & 1) == 0) ctx.address = (uint8_t)v;
}
unsigned int YM2413Read(void) { return 0xFF; }
unsigned char *YM2413GetContextPtr(void) { return (unsigned char *)&ctx; }
unsigned int YM2413GetContextSize(void) { return (unsigned int)sizeof ctx; }
