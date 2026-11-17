// SPDX-License-Identifier: GPL-2.0-or-later
/* sobreposicao.h - la raíz de la live: ext2 de solo lectura con una capa escribible (D379). */
#ifndef SAGUI_SOBREPOSICAO_H
#define SAGUI_SOBREPOSICAO_H
#include "tipos.h"
#include "disco.h"

/* Antes de montar el tipo "sobreposicao": la capa persistente (SAGUI-PERSIST, ext2) o NULL
 * (capa en la RAM). Calcula también el tamaño del archivo abierto de la tabla. */
void sobreposicao_preparar(disco_t *persistente);
bool sobreposicao_persistente(void);   /* la capa montada es la de disco */
/* v8.5.4.2: la capa inferior (el sistema de serie), de solo lectura, en "ponto" (/sistema/serie). */
int  sobreposicao_montar_serie(const char *ponto);

#endif
