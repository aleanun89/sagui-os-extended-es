// SPDX-License-Identifier: GPL-2.0-or-later
/* modos_video.h - lista de modos, cambio en caliente y resolución preferida (D058, D059). */
#ifndef SAGUI_MODOS_VIDEO_H
#define SAGUI_MODOS_VIDEO_H
#include "tipos.h"

void    modos_iniciar(void);              /* después de video_iniciar: registra la lista en el log */
int32_t modos_info_tela(uint32_t ptr);    /* INFO_TELA */
#include "abi.h"
int32_t cab_opc_gravar(uint32_t pedido);   /* D323: INFO_CAB_OPC */
/* D353: lee, cambia con "mudar" (devuelve si cambió algo) y graba la cabecera del disco de arranque. */
int     cab_regravar(bool (*mudar)(uint8_t *s, void *ctx), void *ctx, const char **nome);
int     modos_lista(modo_video_t *v, int n);   /* la lista de SIS_MODOS_VIDEO, para el kernel (D066) */

#endif
