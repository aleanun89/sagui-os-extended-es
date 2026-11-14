// SPDX-License-Identifier: GPL-3.0-only
/* caminho.h - montar y simplificar rutas ("/", ".", "..") */
#ifndef SAGUI_CAMINHO_H
#define SAGUI_CAMINHO_H
#include "tipos.h"

#define CAMINHO_MAX 128

/* Resuelve 'alvo' a partir de la carpeta 'atual' (absoluta) y graba en 'saida' una
 * ruta absoluta simplificada, sin "." ni "..", sin barra al final (excepto
 * la raíz "/"). Devuelve false si no cabe. */
bool caminho_resolver(const char *atual, const char *alvo, char *saida, size_t max);

#endif
