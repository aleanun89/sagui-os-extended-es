// SPDX-License-Identifier: GPL-2.0-or-later
/* modos_video.h - lista de modos, cambio en caliente y resolución preferida (D058, D059). */
#ifndef SAGUI_MODOS_VIDEO_H
#define SAGUI_MODOS_VIDEO_H
#include "tipos.h"

void    modos_iniciar(void);              /* después de video_iniciar: registra la lista en el log */
int32_t modos_info_tela(uint32_t ptr);    /* INFO_TELA */
#include "abi.h"
int     modos_lista(modo_video_t *v, int n);   /* la lista de SIS_MODOS_VIDEO, para el kernel (D066) */

#endif
