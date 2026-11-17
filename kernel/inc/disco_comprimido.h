// SPDX-License-Identifier: GPL-2.0-or-later
/* disco_comprimido.h - la raíz del CD comprimida por bloques (v8.5.4.2, D379 P5). Ver disco_comprimido.c. */
#ifndef SAGUI_DISCO_COMPRIMIDO_H
#define SAGUI_DISCO_COMPRIMIDO_H
#include "disco.h"

#define DISCO_TIPO_RAIZ_COMPRIMIDA 0x5A   /* tipo MBR de la partición (tools/mkimagem.py) */

/* Busca, en los discos que pueden ser el de arranque, una partición con la raíz comprimida de esa
 * etiqueta y la registra como "rz0" (de solo lectura, ya descomprimida). NULL si no la hay. */
disco_t *disco_comprimido_achar(const char *rotulo);

#endif
