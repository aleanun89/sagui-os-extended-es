// SPDX-License-Identifier: GPL-3.0-only
/* copia_escrita.h - cuenta de referencias de las páginas físicas que comparten varios espacios
 * de direcciones (copia al escribir del fork, D401).
 *
 * Solo están en la tabla las páginas con dos referencias o más: una página que no está tiene una.
 * memfisica_liberar() la consulta, así que liberar una página compartida solo baja su cuenta.
 * Todo se llama con el cerrojo general (D165); las funciones apagan además las interrupciones. */
#ifndef SAGUI_COPIA_ESCRITA_H
#define SAGUI_COPIA_ESCRITA_H
#include "tipos.h"

/* Deja sitio para "mais" páginas nuevas sin tener que crecer después (antes de un fork: así
 * clonar nunca falla a medias por la tabla). false = sin memoria para la tabla. */
bool     cow_reservar(uint32_t mais);
/* Una referencia más a la página física "fis" (la primera vez pasa de 1 a 2). Necesita sitio
 * reservado con cow_reservar(). */
void     cow_somar(uint64_t fis);
/* Una referencia menos. true si la página estaba compartida (no hay que liberarla); false si
 * tenía una sola referencia (quien llama la libera). */
bool     cow_soltar(uint64_t fis);
/* Referencias de la página (1 si no está en la tabla). */
uint32_t cow_refs(uint64_t fis);
/* Páginas en la tabla (compartidas ahora) y capacidad, para las estadísticas. */
uint32_t cow_paginas(void);
uint32_t cow_capacidade(void);

#endif
