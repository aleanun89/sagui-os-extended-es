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
int tz_unid_celulas(int classe);                   /* casillas por lado de una unidad: 2 las grandes (mercader, catapultas, barcos, dragón) */
int tz_unid_cabe(const TzMundo *m, TzId i, int x, int y);   /* toda la huella de la unidad i cabe con el ancla en (x, y) */
int tz_lugar_livre_cls(const TzMundo *m, int classe, int dono, const int r[4], int *x, int *y);   /* como tz_lugar_livre, con la huella de la clase */
void tz_aplicar_comando(TzMundo *m, const TzComando *c);
int tz_hostil(const TzMundo *m, int k, TzId t);   /* gameplay.rs hostile_to: ¿el reino k ataca solo a t? */
int tz_disponivel(const TzMundo *m, int k, int civ, const char *nome, int tecnologia);   /* 0 o -1 con tz_erro */

void tz_experiencia(TzMundo *m, TzId i, int pontos, int propagar);   /* mundo.c */
void tz_gastar(TzMundo *m, int k, const uint32_t c[TZ_NREC]);       /* mundo.c: gasta lo propio y luego lo de quien comparte */

/* itens.c */
void tz_largar_itens(TzMundo *m, TzId i);          /* quien lleva objetos los deja caer a su alrededor (muerte) */
void tz_item_tic(TzMundo *m, TzId i);              /* órdenes de objetos: TZ_O_PEGAR_ITEM, TZ_O_USAR_ITEM, TZ_O_SOLTAR_ITEM */
void tz_vender_item(TzMundo *m, int k, TzId mercado, TzId item);
void tz_item_gastar(TzMundo *m, TzId item);        /* el objeto desaparece (lo llevaba alguien o estaba en el suelo) */

/* comercio.c */
void tz_comerciar_tic(TzMundo *m, TzId i);         /* TZ_O_COMERCIAR */
void tz_vender_tic(TzMundo *m, TzId i);            /* TZ_O_VENDER */
void tz_negocio_tic(TzMundo *m);                   /* préstamos, intercambios, ofertas de taberna, cruzadas: una vez por tic */
int tz_negocio_comando(TzMundo *m, const TzComando *c);   /* 1 si lo atendió */
int tz_comercio_ordem(TzMundo *m, TzId i, const TzOrdem *o, int fila);   /* prepara la ruta; 0 bien, -1 no vale */

/* magia.c */
void tz_efeitos_tic(TzMundo *m, TzId i);           /* caducan maldición, hipnosis, invisibilidad; energía del espía */
void tz_remoinhos_tic(TzMundo *m);
const char *tz_feitico_chave(int idx);             /* world.rs spell_key: «spell_firerain»... o NULL */
int tz_feitico_ordem(const TzMundo *m, const char *chave);   /* índice en cat->ord de la orden del hechizo o -1 */
void tz_espia_revelar(TzMundo *m, TzId host);      /* el espía infiltrado vuelve a aparecer */
void tz_espia_tic(TzMundo *m, TzId i);             /* TZ_O_ASESINAR, TZ_O_SUPLANTAR */
int tz_converter(TzMundo *m, TzId i, int classe);  /* cambia la clase de una unidad (monje a sacerdote...) */

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
int tz_mover_obj(TzMundo *m, TzId i, TzId j, int alc);             /* acoes.c: igual, hacia un objeto */
void tz_proxima_ordem(TzMundo *m, TzId i);                          /* acoes.c: la orden de ahora acabó */
void tz_estado(TzMundo *m, TzObj *o, int e, int trab);              /* acoes.c */
void tz_olhar_obj(const TzMundo *m, TzObj *o, const TzObj *t);      /* acoes.c */
int tz_pagar(TzMundo *m, int k, const uint32_t c[TZ_NREC]);         /* acoes.c: cobra y avisa; 0 bien */
int tz_efeito(const TzMundo *m, const TzObj *o, int ef);   /* el efecto está activo */

/* gravar.c */
uint32_t tz_visitar_suma(const TzMundo *m);

#endif
