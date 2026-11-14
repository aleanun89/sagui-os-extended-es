// SPDX-License-Identifier: GPL-2.0-or-later
/* bloqueo.h - bloqueo de publicidad y rastreo por DNS en el resolvedor del
 * kernel, al estilo de un sumidero DNS (D218). Desactivado por defecto. */
#ifndef SAGUI_BLOQUEO_H
#define SAGUI_BLOQUEO_H
#include "tipos.h"

/* Al arrancar, después del fstab: si bloqueo.conf dice «activo 1», carga la
 * tabla. Ningún error impide arrancar: solo se anota y el bloqueo queda apagado. */
void bloqueo_iniciar(void);

/* El resolvedor pregunta por un nombre ya validado y en minúsculas (sin el
 * punto final). true = bloqueado: se responde 0.0.0.0 sin preguntar a la red. */
bool bloqueo_filtrar(const char *nome);

int32_t sis_bloqueo(uint32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t d);

#endif
