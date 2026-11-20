/* SPDX-License-Identifier: GPL-3.0-or-later */
/* partida.h - partida jugable mínima en el visor (partida.c, D417 fase 5). La interfaz completa es de la fase 4. */
#ifndef TZAR_PARTIDA_H
#define TZAR_PARTIDA_H

#include "mundo.h"

typedef struct {
	int ativa, pausa, velocidade;     /* velocidad: 10 = normal */
	int acum;
	TzCatalogo cat;
	TzMundo m;
	TzMapa *mapa;
	TzId sel[TZ_CMD_IDS]; int nsel;
	int colocar;                      /* clase del edificio por colocar, o -1 */
	int rato_cx, rato_cy;
	void *vis; int capvis;
	int falhas_dibujo;
} TzPartida;

int tz_partida_iniciar(TzPartida *p, TzRecursos *r, TzBiblioteca *b, TzMapa *m, uint32_t semente);
void tz_partida_liberar(TzPartida *p);
int tz_partida_avancar(TzPartida *p, int dt_ms);     /* tics jugados */
int tz_partida_esconder(const TzPartida *p, int obj_mapa);
/* el mundo vivo (escena + objetos + proyectiles + niebla) sobre el terreno ya pintado; devuelve los dibujos que fallaron */
int tz_desenhar_mundo(TzMundo *m, TzEscena *e, TzLienzo *l, int camx, int camy, uint64_t ms);
void tz_partida_dibujar(TzPartida *p, TzEscena *e, TzLienzo *l, int camx, int camy, uint64_t ms);
void tz_partida_rato(TzPartida *p, int wx, int wy);
void tz_partida_clique(TzPartida *p, int wx, int wy, int botao, int mayus);   /* botao 1 izquierdo, 3 derecho */
void tz_partida_arrastar(TzPartida *p, int x0, int y0, int x1, int y1, int mayus);
int tz_partida_tecla(TzPartida *p, int tecla);       /* '1'-'9', ' ', '+', '-', 127 (Supr) */
void tz_partida_resumo(const TzPartida *p, char *dst, size_t n);
/* demo.c: jugador automático de prueba para todos los reinos (gancho de cada tic) */
void tz_demo_tic(TzMundo *m, void *dados);
extern int tz_demo_so_reino;   /* 0 = todos los reinos; k = solo el reino k (el jugador en las pruebas del juego) */

#endif
