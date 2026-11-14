// SPDX-License-Identifier: GPL-2.0-or-later
/* conf_sistema.h - la partición SAGUI-CONF: configuración persistente del sistema (D255). */
#ifndef SAGUI_CONF_SISTEMA_H
#define SAGUI_CONF_SISTEMA_H
#include "tipos.h"
#include "abi.h"

#define CONF_PONTO CONF_SISTEMA_PONTO   /* abi.h: /sistema/conf */

bool conf_sistema_montada(void);     /* SAGUI-CONF montada con escritura */
void conf_sistema_superpor(void);    /* copia su etc/ sobre el /etc de la RAM (al arrancar) */
#endif
