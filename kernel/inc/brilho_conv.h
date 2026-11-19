// SPDX-License-Identifier: GPL-2.0-or-later
/* brilho_conv.h - de los niveles de brillo que da el firmware a índices y porcentajes (D403).
 *
 * Solo cálculo, sin hardware ni uACPI, como bateria_conv.h: los drivers de los
 * portátiles (video_acpi.c, fujitsu.c, toshiba.c) leen con el intérprete el
 * paquete _BCL, el _BQC o el nivel de la BIOS y dejan aquí los números tal cual;
 * esto ordena los niveles, descarta lo que no tiene sentido y decide el nivel de
 * un porcentaje o de un paso. Las pruebas compilan el mismo archivo en el
 * anfitrión (tests/h404_portatiles.py, -DSAGUI_HOST) con casos raros de BIOS. */
#ifndef SAGUI_BRILHO_CONV_H
#define SAGUI_BRILHO_CONV_H
#ifdef SAGUI_HOST
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#else
#include "tipos.h"
#endif

#define BRILHO_MAX_NIVEIS 101   /* 0-100 %: lo más que da una BIOS razonable */

/* Los niveles de un _BCL. El paquete es: nivel con el cargador, nivel con la
 * batería y después los niveles que admite la pantalla (en cualquier orden, a
 * veces repetidos o con los dos primeros dentro). Aquí quedan solo los de la
 * pantalla (desde el tercero), ordenados de menor a mayor y sin repetir. Si el
 * paquete no trae más de dos (BIOS rotas), se usan esos. Valores por encima de
 * 0xFFFF se descartan. Devuelve cuántos quedan en "niv" (0 o 1 = no sirve). */
int brilho_niveis(const uint64_t *bcl, int n, uint32_t *niv, int max);

/* El índice del nivel con ese valor; si no está, el del más cercano. */
int brilho_indice(const uint32_t *niv, int n, uint32_t valor);

/* Lo que devolvió _BQC: el valor del nivel (lo normal) o, en algunas BIOS, su
 * índice en la lista del _BCL. *e_indice dice cuál se tomó. Devuelve el índice
 * o -1 si no encaja con nada (un _BQC de 0 con niveles que empiezan más arriba
 * es «no lo sé»: la BIOS lo devuelve antes del primer _BCM). */
int brilho_bqc(const uint32_t *niv, int n, uint64_t bqc, bool *e_indice);

/* El porcentaje de un nivel: el valor mismo si los niveles van de 0 a 100
 * (lo que pide la especificación) y, si no, la proporción sobre el mayor. */
int brilho_porcentagem(const uint32_t *niv, int n, int indice);

/* El índice cuyo porcentaje está más cerca de "pct" (0-100). */
int brilho_indice_de_pct(const uint32_t *niv, int n, int pct);

/* Un paso arriba (passo > 0) o abajo desde "indice", sin salir de la lista. */
int brilho_passo(int n, int indice, int passo);

/* Niveles que no son una lista (Fujitsu: 0..total-1; Toshiba: 0..7): porcentaje
 * del nivel y nivel de un porcentaje. */
int brilho_pct_linear(int nivel, int total);
int brilho_nivel_linear(int pct, int total);

#endif
