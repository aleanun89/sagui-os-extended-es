// SPDX-License-Identifier: GPL-2.0-or-later
/* energia.h - «no molestar» para los programas que no quieren que salte el salvapantallas, el bloqueo ni el apagado de
 * la pantalla mientras trabajan (un vídeo, un juego, una presentación) (D422). Incluir después de sagui.h.
 *
 * no_molestar(1) al empezar y no_molestar(0) al acabar; si el programa acaba sin soltarlo, el kernel lo suelta solo.
 * Una ventana a pantalla completa (F11) cuenta también, sin llamar a nada. */
#ifndef SAGUI_ENERGIA_H
#define SAGUI_ENERGIA_H
#include "abi.h"

static inline int no_molestar(int si)
{
    return info_sistema(INFO_OCIO_CTL, (void *)(unsigned long)OCIO_PEDIDO(OCIO_OP_NO_MOLESTAR, si ? 1 : 0), 0);
}
#endif
