// SPDX-License-Identifier: GPL-2.0-or-later
/* ocio.h - energía y pantalla: salvapantallas, bloqueo y apagado de la pantalla por inactividad, aviso de
 * batería y «no molestar» (D422). La parte de dibujo del salvapantallas está en kernel/lib/ocio_efectos.c. */
#ifndef SAGUI_OCIO_H
#define SAGUI_OCIO_H
#include "tipos.h"
#include "desenho.h"

void ocio_iniciar(void);                 /* gui_iniciar: lee /etc/energia.conf y reinicia la cuenta */
void ocio_tique(void);                   /* gestor de ventanas, en cada tic */
/* Se tocó una tecla o se movió el ratón. true = despertó el salvapantallas o la pantalla apagada y el evento
 * no debe llegar a nadie (la primera pulsación solo despierta). */
bool ocio_entrada(void);
bool ocio_cobre(void);                   /* el salvapantallas o la pantalla negra cubren todo */
void ocio_desenhar(superficie_t *t);     /* el compositor, en lugar de dibujar las ventanas */
int32_t ocio_info(uint32_t ptr, uint32_t n);   /* INFO_OCIO */
int32_t ocio_ctl(uint32_t pedido);             /* INFO_OCIO_CTL */
bool ocio_op_libre(uint32_t pedido);     /* ¿la operación la puede pedir cualquiera (permisos.c)? */

#endif
