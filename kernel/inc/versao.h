// SPDX-License-Identifier: GPL-2.0-or-later
/* versao.h - nombre y versión del sistema, en un solo lugar (D101).
 * El kernel y los programas (Acerca de, sobre, infopc, arranque) incluyen
 * este archivo: para cambiar de versión basta con editar SAGUI_VERSAO. */
#ifndef SAGUI_VERSAO_H
#define SAGUI_VERSAO_H

#define SAGUI_NOME         "SaguiOS-Extended"
#define SAGUI_VERSAO       "v8.5.6.2"
#define SAGUI_NOME_VERSAO  SAGUI_NOME " " SAGUI_VERSAO


/* Variante de licencia (mejora 057, D109): la fijan `make SEM_GPL=1` y
 * `make SEM_FIRMWARE=1` con -DSAGUI_SEM_GPL / -DSAGUI_SEM_FIRMWARE. */
#ifdef SAGUI_SEM_GPL
#  ifdef SAGUI_SEM_FIRMWARE
#    define SAGUI_VARIANTE "sin GPL (permisiva) y sin firmware"
#  else
#    define SAGUI_VARIANTE "sin GPL (permisiva)"
#  endif
#elif defined(SAGUI_SEM_FIRMWARE)
#  define SAGUI_VARIANTE "con GPL, sin firmware"
#else
#  define SAGUI_VARIANTE "con GPL"
#endif

#endif
