// SPDX-License-Identifier: GPL-3.0-only
/* memalta.h - ventanas temporales del kernel a páginas físicas (D348).
 *
 * El kernel ve la memoria baja por su propia dirección (mapeo 1:1, D008/D049),
 * pero la alta (por encima de MEMFIS_LIMITE) no tiene dirección en el kernel:
 * para tocar una página de un programa por su dirección FÍSICA hay que pasar
 * por aquí. Es la idea del kmap_atomic de Linux:
 *
 *     uint8_t *p = fisica_mapear(fis);   ... usar p, sin dormir ...   fisica_soltar(p);
 *
 * - Si la página es baja, devuelve la propia dirección y no cuesta nada.
 * - Si es alta (o superior, por encima de 4 GB con PAE, D349), la pone en una de las ventanas de ESTA CPU (cada CPU tiene las
 *   suyas, para el SMP), con las interrupciones apagadas hasta soltarla, y
 *   hace invlpg al poner y al quitar. Se pueden anidar (hasta MEMALTA_NIVEIS)
 *   y se sueltan en orden inverso.
 * - Entre mapear y soltar no se puede dormir ni ceder la CPU.
 *
 * Nunca convertir en puntero una dirección física de una página de programa
 * (espaco_traduzir, espaco_tirar, memfisica_alocar_usuario): tools/auditar_memalta.py
 * busca esos casos. Lo que el kernel lee o escribe del programa ACTUAL en una
 * llamada al sistema va por la dirección virtual del programa (usuario_ok) y no
 * necesita ventanas. */
#ifndef SAGUI_MEMALTA_H
#define SAGUI_MEMALTA_H
#include "tipos.h"

#define MEMALTA_NIVEIS 4   /* ventanas por CPU: anidamiento máximo */

void  memalta_iniciar(void);      /* después de paginacao_iniciar() */
void *fisica_mapear(uint64_t fis);   /* puntero con el desplazamiento de fis dentro de la página */
void  fisica_soltar(void *p);

/* Atajos para una sola página (lo de dentro de la página: "n" no la cruza). */
/* D349: con PAE las direcciones físicas llegan a 64 GB (uint64_t, = fis_t). */
void  fisica_zerar(uint64_t fis);                                   /* la página entera */
void  fisica_escrever(uint64_t fis, const void *origem, uint32_t n);
void  fisica_ler(uint64_t fis, void *destino, uint32_t n);
void  fisica_copiar_pagina(uint64_t destino, uint64_t origem);      /* para un futuro copy-on-write */

uint32_t memalta_usos(void);      /* cuántas veces se usó una ventana (estadística) */

#endif
