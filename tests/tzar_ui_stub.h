/* SPDX-License-Identifier: GPL-3.0-or-later */
/* tzar_ui_stub.h - mundo de prueba mínimo detrás de la API de mundo.h, solo para probar la interfaz de Tzar (D417,
 * fase 4) mientras la simulación real (rama tzar-sim) no está fusionada. No es parte del juego: no se compila en
 * SaguiOS. Implementa lo que usa la interfaz: consultas, comandos (se anotan y algunos se aplican), eventos. */
#ifndef TZAR_UI_STUB_H
#define TZAR_UI_STUB_H
#include "ui.h"

extern TzComando stub_log[512];
extern int stub_nlog;

/* catálogo: con recursos reales (ORDERS.INI, MENU.INI, OBJECTS) o el mínimo sintético si rec == NULL */
void stub_catalogo(TzCatalogo *c, TzRecursos *rec, const TzBiblioteca *bib);
void stub_catalogo_liberar(TzCatalogo *c);
/* mundo vacío de lado x lado con el catálogo (sin objetos) */
void stub_mundo(TzMundo *m, const TzCatalogo *c, int lado);
/* mundo con los objetos de un mapa leído */
void stub_mundo_de_mapa(TzMundo *m, const TzCatalogo *c, const TzMapa *mapa, int jogador);
TzId stub_obj(TzMundo *m, int classe, int civ, int dono, int cx, int cy);
void stub_nevoa(TzMundo *m);              /* recalcula la niebla del jugador desde sus objetos */
void stub_liberar(TzMundo *m);
void stub_evento(TzMundo *m, int tipo, int reino, int classe);
#endif
