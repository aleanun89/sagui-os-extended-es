/* SPDX-License-Identifier: GPL-3.0-or-later */
/* tzar_falso_nulo.c - en el binario «real» de tzar_campanas.c no hay mundo falso (está la simulación): estas no se llaman (D417, fase 7). */
#include "mundo.h"
TzMundo *falso_criar(const TzMapa *mapa, int jogador) { (void)mapa; (void)jogador; return 0; }
void falso_liberar(TzMundo *m) { (void)m; }
void falso_tic(TzMundo *m) { (void)m; }
