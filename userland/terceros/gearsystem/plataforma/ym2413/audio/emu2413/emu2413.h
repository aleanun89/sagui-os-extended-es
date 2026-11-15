// SPDX-License-Identifier: GPL-2.0-or-later
/* emu2413.h - sustituto vacío del YM2413 (FM de la Master System japonesa) para Gearsystem en SaguiOS (D321).
 *
 * El upstream trae aquí el generador de FM de Genesis Plus GX (EkeEke) sobre el de MAME (Jarek Burczynski),
 * cuya licencia (no comercial, la de Genesis Plus GX) no es compatible con la GPL v3: no se trae. Con este
 * sustituto el núcleo funciona igual salvo que la música FM de los juegos japoneses calla (el PSG suena).
 */
#ifndef _H_YM2413_
#define _H_YM2413_

#include <stdint.h>

typedef struct { uint8_t address; uint8_t status; uint32_t relleno[7]; } YM2413_OPLL;

#ifdef __cplusplus
extern "C" {
#endif
void YM2413Init(void);
void YM2413ResetChip(void);
int YM2413Update(void);
void YM2413Write(unsigned int a, unsigned int v);
unsigned int YM2413Read(void);
unsigned char *YM2413GetContextPtr(void);
unsigned int YM2413GetContextSize(void);
#ifdef __cplusplus
}
#endif

#endif
