// SPDX-License-Identifier: GPL-2.0-or-later
/* iso9660.h - sistema de archivos de los CD/DVD (ISO 9660), solo lectura (D207). */
#ifndef SAGUI_ISO9660_H
#define SAGUI_ISO9660_H
#include "tipos.h"
#include "disco.h"

/* ¿Hay un ISO 9660 en el disco? Rellena la etiqueta (identificador del volumen)
 * y una descripción ("ISO 9660", "ISO 9660 + Joliet", "ISO 9660 + Rock Ridge"). */
bool iso_reconhecer(disco_t *d, char *rotulo, int max_rotulo, char *formato, int max_formato);

/* El Torito: busca la entrada de arranque con emulación de disco duro y devuelve
 * dónde empieza la imagen, en sectores de 512 del CD. false = no hay. */
bool iso_el_torito(disco_t *d, uint32_t *inicio512);

#endif
