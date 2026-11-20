/* SPDX-License-Identifier: GPL-3.0-or-later */
/* mundo_int.h - lo interno de la simulación de Tzar (D417, fase 5): solo para mundo.c, acoes.c, caminho.c y
 * gravar.c. Los demás módulos usan mundo.h. Traducción de ReTzar (ver mundo.h). */
#ifndef TZAR_MUNDO_INT_H
#define TZAR_MUNDO_INT_H

#include "mundo.h"

enum { TZ_CAPA_AR = 0, TZ_CAPA_TERRA = 1, TZ_CAPA_AGUA = 2 };

typedef struct {
	/* búsqueda de caminos (lado*lado) */
	uint32_t *g; int32_t *pai; uint32_t *visto; uint8_t *fechado; uint32_t marca;
	void *heap; int capheap;
	int limite_nos;
	int ilhas_sujas;
	/* población contada en el último tic, por reino */
	uint16_t pop[TZ_REINOS + 1], casas[TZ_REINOS + 1];
	uint32_t ultimo_aviso[TZ_REINOS + 1];   /* tic del último aviso de ataque de cada reino */
	/* id de mapa para los objetos nuevos */
	uint16_t prox_id;
} TzPriv;

/* caminho.c */
int tz_cel_transitavel(const TzMundo *m, int capa, int dono, int x, int y);
int tz_dist_ret(int x, int y, const int r[4]);
int tz_caminho(TzMundo *m, TzId u, int x0, int y0, const int r[4], int alc, int evita);
void tz_ilhas(TzMundo *m);

/* mundo.c */
int tz_obj_capa(const TzMundo *m, const TzObj *o);
void tz_obj_ret(const TzMundo *m, const TzObj *o, int r[4]);   /* casillas que ocupa (x0, y0, x1, y1) */
int tz_obj_dist(const TzMundo *m, const TzObj *a, const TzObj *b);   /* Chebyshev entre huellas (1 = al lado) */
void tz_ev(TzMundo *m, int tipo, int reino, TzId obj, TzId alvo, int x, int y, int classe, int32_t valor);
TzId tz_novo_obj(TzMundo *m, int classe, int civ, int dono, int cx, int cy, int obra);
void tz_marcar(TzMundo *m, TzId i, int pon);   /* pone o quita el objeto de sus casillas */
void tz_matar(TzMundo *m, TzId i, TzId por);
void tz_trocar_dono(TzMundo *m, TzId i, int dono);
void tz_recalcular(TzMundo *m, TzId i);        /* ataque y defensa con nivel e investigaciones */
int tz_pesquisas(const TzMundo *m, int k, const char *nome);   /* veces completada */
int tz_cel_livre(const TzMundo *m, int capa, int dono, int x, int y);
int tz_lugar_livre(const TzMundo *m, int capa, int dono, const int r[4], int *x, int *y);   /* casilla libre alrededor */
void tz_aplicar_comando(TzMundo *m, const TzComando *c);
int tz_hostil(const TzMundo *m, int k, TzId t);   /* gameplay.rs hostile_to: ¿el reino k ataca solo a t? */
int tz_disponivel(const TzMundo *m, int k, int civ, const char *nome, int tecnologia);   /* 0 o -1 con tz_erro */

/* acoes.c */
void tz_ator_tic(TzMundo *m, TzId i);
void tz_producao_tic(TzMundo *m, TzId i);
void tz_projeteis_tic(TzMundo *m);
void tz_dar_ordem(TzMundo *m, TzId i, const TzOrdem *o, int fila);
void tz_parar(TzMundo *m, TzId i);
int tz_enfileirar_trabalho(TzMundo *m, TzId edificio, int tipo, int arg);
void tz_cancelar_trabalho(TzMundo *m, TzId edificio, int idx);
TzId tz_colocar_obra(TzMundo *m, int k, int classe, int cx, int cy);
void tz_obra_pronta(TzMundo *m, TzId j);
void tz_dano(TzMundo *m, TzId alvo, uint32_t golpe, int categoria, TzId de);

/* magia.c */
void tz_hechizo_tic(TzMundo *m, TzId i);
int tz_mover_para(TzMundo *m, TzId i, const int r[4], int alc);   /* acoes.c: 1 llegó, 0 andando, -1 no puede */
int tz_efeito(const TzMundo *m, const TzObj *o, int ef);   /* el efecto está activo */

/* gravar.c */
uint32_t tz_visitar_suma(const TzMundo *m);

#endif
