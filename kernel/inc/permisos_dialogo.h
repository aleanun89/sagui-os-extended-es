// SPDX-License-Identifier: GPL-2.0-or-later
/* permisos_dialogo.h - diálogo de permisos de administrador (D269, mejora 102). */
#ifndef SAGUI_PERMISOS_DIALOGO_H
#define SAGUI_PERMISOS_DIALOGO_H
#include "tipos.h"
#include "desenho.h"

bool pd_ativo(void);                              /* hay un diálogo en pantalla (modal) */
void pd_tique(void);                              /* gestor de ventanas, cada vuelta: abre, actualiza y cierra */
void pd_desenhar(superficie_t *t);                /* atenúa todo el recorte y pinta el diálogo encima */
bool pd_mouse(int x, int y, int botoes, int mudaram);
bool pd_tecla(int tecla);                         /* true = el diálogo se quedó con la tecla */

#endif
