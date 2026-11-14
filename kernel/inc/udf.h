// SPDX-License-Identifier: GPL-2.0-or-later
/* udf.h - sistema de archivos UDF de los CD/DVD, solo lectura (D233). */
#ifndef SAGUI_UDF_H
#define SAGUI_UDF_H
#include "tipos.h"
#include "disco.h"

/* ¿Hay un UDF que se sepa leer en el disco? Rellena la etiqueta (el identificador
 * del volumen lógico) y una descripción ("UDF 2.01"). Si lo hay pero no se sabe
 * leer (partición virtual o de metadatos), devuelve false y lo dice en "formato". */
bool udf_reconhecer(disco_t *d, char *rotulo, int max_rotulo, char *formato, int max_formato);

#endif
