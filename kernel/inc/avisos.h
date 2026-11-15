// SPDX-License-Identifier: GPL-2.0-or-later
/* avisos.h - notificaciones emergentes (H34, D125, D233; issue #16, D269).
 *
 * Texto completo (la caja crece en alto), corte de línea por el ancho real
 * del texto, duración según la longitud y cola de avisos (no se pisan). */
#ifndef SAGUI_AVISOS_H
#define SAGUI_AVISOS_H
#include "tipos.h"
#include "desenho.h"

/* Encola un aviso. caminho != NULL y vacío no: lo hace pulsable (abre el Visor, o con audio
 * reproduce el CD). segundos = 0: la duración la decide la longitud del texto (mínimo 5 s). */
void gui_aviso_ex(const char *texto, const char *caminho, bool audio, uint32_t segundos);
bool avisos_visivel(void);
void avisos_retangulo(int *x, int *y, int *l, int *a);   /* de la caja actual (con margen para redibujar) */
void avisos_desenhar(superficie_t *t);
bool avisos_clique(int x, int y);   /* true = el clic era para un aviso pulsable (ya lo atendió) */
void avisos_tique(void);            /* caduca el actual y muestra el siguiente */

#endif
