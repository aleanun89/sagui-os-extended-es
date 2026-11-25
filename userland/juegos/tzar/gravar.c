/* SPDX-License-Identifier: GPL-3.0-or-later */
/* gravar.c - partidas guardadas (formato propio «TZSV») y suma del estado de la simulación de Tzar (D417, fase 5).
 *
 * Un solo recorrido (visitar) por todo el estado que influye en el futuro sirve para las tres cosas: escribir,
 * leer y sumar (FNV-1a). Así la suma cubre exactamente lo que se guarda, y una partida cargada sigue con la misma
 * suma que la original. Todo en little-endian y campo a campo: el mismo fichero vale en el PC (64 bits) y en
 * SaguiOS (32 bits). ReTzar guarda en JSON cifrado (save.rs); aquí el formato es propio, la idea (lo que se guarda:
 * objetos, actores, reinos, casillas que cambian, proyectiles, niebla, ganchos) es la de save.rs y Simulation.
 *
 * Formato: «TZSV» u32 versión, u32 lado, u32 número de objetos del mapa; luego el estado, la niebla y la cola de
 * eventos; luego por cada gancho con etiqueta: 8 bytes de etiqueta, u32 tamaño y sus datos; al final 8 bytes a cero.
 */
#include "mundo_int.h"
#include <stdlib.h>
#include <string.h>

#define VERSAO 6

enum { V_SUMA = 0, V_GRAVAR = 1, V_LER = 2 };
typedef struct { int modo; FILE *f; uint32_t h; int erro; } V;

static void vb(V *v, void *p, int n)
{
	uint8_t b[8];
	uint64_t x = 0;
	if (v->erro) return;
	if (v->modo == V_LER) {
		if (fread(b, 1, (size_t)n, v->f) != (size_t)n) { v->erro = 1; return; }
		for (int i = n - 1; i >= 0; --i) x = (x << 8) | b[i];
		switch (n) { case 1: *(uint8_t *)p = (uint8_t)x; break; case 2: *(uint16_t *)p = (uint16_t)x; break;
		case 4: *(uint32_t *)p = (uint32_t)x; break; default: *(uint64_t *)p = x; }
		return;
	}
	switch (n) { case 1: x = *(uint8_t *)p; break; case 2: x = *(uint16_t *)p; break; case 4: x = *(uint32_t *)p; break; default: x = *(uint64_t *)p; }
	for (int i = 0; i < n; ++i) b[i] = (uint8_t)(x >> (8 * i));
	if (v->modo == V_GRAVAR) { if (fwrite(b, 1, (size_t)n, v->f) != (size_t)n) v->erro = 1; return; }
	for (int i = 0; i < n; ++i) { v->h ^= b[i]; v->h *= 16777619u; }
}
#define VV(x) vb(v, &(x), (int)sizeof(x))
#define VA(a) do { for (size_t _i = 0; _i < sizeof(a) / sizeof((a)[0]); ++_i) VV((a)[_i]); } while (0)

static void v_ordem(V *v, TzOrdem *o) { VV(o->tipo); VV(o->modo); VV(o->arg); VV(o->alvo); VV(o->x); VV(o->y); VV(o->x2); VV(o->y2); }

static void v_ator(V *v, TzAtor *a)
{
	v_ordem(v, &a->ordem);
	VV(a->nfila);
	if (a->nfila < 0 || a->nfila > TZ_FILA_ORDENS) { v->erro = 1; return; }
	for (int i = 0; i < a->nfila; ++i) v_ordem(v, &a->fila[i]);
	v_ordem(v, &a->retomar);
	VV(a->automatica);
	/* el camino: solo lo que queda */
	int32_t resto = a->ncaminho - a->icaminho;
	if (resto < 0) resto = 0;
	VV(resto);
	if (resto < 0 || resto > TZ_CAMINHO_MAX) { v->erro = 1; return; }
	if (v->modo == V_LER) {
		free(a->caminho);
		a->caminho = resto ? malloc((size_t)resto * sizeof *a->caminho) : NULL;
		if (resto && !a->caminho) { v->erro = 1; return; }
		a->capcaminho = resto; a->ncaminho = resto; a->icaminho = 0;
		for (int i = 0; i < resto; ++i) { VV(a->caminho[i][0]); VV(a->caminho[i][1]); }
	} else {
		for (int i = a->icaminho; i < a->ncaminho; ++i) { VV(a->caminho[i][0]); VV(a->caminho[i][1]); }
	}
	VA(a->destino); VV(a->falhas); VV(a->espera); VA(a->de); VV(a->passo); VV(a->passo_total);
	VV(a->recarga); VV(a->trabalho_tic); VV(a->ciclos); VV(a->alvo_auto); VV(a->trabalho); VV(a->carga_tipo); VV(a->carga);
	VV(a->origem);
	VV(a->ntrab);
	if (a->ntrab < 0 || a->ntrab > TZ_FILA_TRABALHO) { v->erro = 1; return; }
	for (int i = 0; i < a->ntrab; ++i) {
		TzTrabalho *t = &a->trab[i];
		VV(t->tipo); VV(t->arg); VV(t->restante_ms); VV(t->total_ms); VA(t->pago); VV(t->esperando);
	}
	VA(a->reuniao); VV(a->obra_ms); VV(a->obra_total_ms); VV(a->modo); VV(a->ultimo_golpe); VV(a->efeito); VV(a->efeito_ate);
	/* v8.5.5.2: comercio, taberna, hechizos, objetos */
	struct TzRota *r = &a->rota;
	VV(r->ativa); VA(r->rec); VV(r->quantidade); VV(r->distancia); VV(r->lado_mapa); VV(r->carga); VV(r->reintento);
	VV(r->carga_tipo); VV(r->reservado); VV(a->com_origem); VV(a->com_parceiro);
	struct TzTroca *t = &a->troca;
	VV(t->ativa); VV(t->de); VV(t->para); VV(t->pago); VV(t->total); VV(t->entregue); VV(t->proximo);
	for (int l = 0; l < 2; ++l) {
		VV(a->noferta[l]);
		if (a->noferta[l] > 8) { v->erro = 1; return; }
		for (int q = 0; q < 8; ++q) {
			struct TzOferta *o = &a->oferta[l][q];
			VV(o->id); VV(o->classe); VV(o->civ); VV(o->quantidade); VV(o->nivel);
		}
	}
	VV(a->oferta_prox); VV(a->cruzada_prox); VV(a->dono_orig); VV(a->maldicao_ate); VV(a->hipnose_ate); VV(a->invisivel_ate);
	VV(a->espia_energia); VV(a->espia_em); VA(a->aprendidos); VV(a->naprendidos); VV(a->ancla_cruzada); VV(a->nitens);
	VV(a->remoinho_dono); VV(a->remoinho_mov); VV(a->livro); VV(a->remoinho_golpe); VV(a->remoinho_mueve);
	VV(a->espia_reino); VV(a->espia_civ); VV(a->espia_nivel); VV(a->espia_heroi); VV(a->espia_xp); VV(a->espia_vida); VV(a->espia_vida_max);
	if (a->naprendidos > 8) v->erro = 1;
}

static void v_obj(V *v, TzMundo *m, TzObj *o)
{
	VV(o->id_mapa); VV(o->classe); VV(o->civ); VV(o->dono); VV(o->cor); VV(o->estado); VV(o->direcao); VV(o->nivel);
	VV(o->flags); VV(o->cx); VV(o->cy); VV(o->px); VV(o->py); VV(o->vida); VV(o->vida_max);
	VV(o->ataque); VV(o->defesa); VV(o->visao); VV(o->alcance); VV(o->resistencia); VV(o->xp); VV(o->mana); VV(o->mana_max);
	VV(o->recurso); VV(o->anim_tic); VV(o->dentro_de); VV(o->ndentro);
	VV(o->item.vida); VV(o->item.mana); VV(o->item.ataque); VV(o->item.defesa); VV(o->item.xp); VV(o->item.feitico);
	VV(o->item.finito); VV(o->item.cargas); VV(o->item.texto); VV(o->item.ligado); VA(o->item.imagem); VV(o->pocao_ataque); VV(o->pocao_defesa); VV(o->heroi_nome);
	uint8_t tem = o->ator != NULL;
	VV(tem);
	if (v->modo == V_LER) {
		if (tem && !o->ator) { o->ator = calloc(1, sizeof *o->ator); if (!o->ator) { v->erro = 1; return; } }
		if (!tem && o->ator) { free(o->ator->caminho); free(o->ator); o->ator = NULL; }
	}
	if (tem) v_ator(v, o->ator);
	(void)m;
}

static void visitar(V *v, TzMundo *m)
{
	VV(m->tic); VV(m->rng.s); VV(m->semente); VV(m->jogador); VV(m->acabou); VV(m->ganhou);
	VV(m->regras.sem_nevoa); VV(m->regras.explorado); VV(m->regras.populacao); VV(m->regras.dificuldade);
	VV(m->regras.vitoria); VV(m->regras.vitoria_limite); VV(m->regras.nivel_recursos); VV(m->regras.campanha);
	VV(m->regras.troncos); VV(m->regras.sem_vitoria_auto); VV(m->sem_resgate); VA(m->herois_usados);
	/* objetos: los del mapa ya existen (mismo índice); los nuevos se añaden */
	int32_t n = m->nobj;
	VV(n);
	if (n < m->mapa->nobj || n > 4 * 1024 * 1024) { v->erro = 1; return; }
	if (v->modo == V_LER && n > m->nobj) {
		if (n > m->capobj) {
			TzObj *no = realloc(m->obj, (size_t)n * sizeof *no);
			if (!no) { v->erro = 1; return; }
			m->obj = no; m->capobj = n;
		}
		memset(m->obj + m->nobj, 0, (size_t)(n - m->nobj) * sizeof *m->obj);
		m->nobj = n;
	}
	for (int i = 0; i < n && !v->erro; ++i) v_obj(v, m, &m->obj[i]);
	/* reinos */
	for (int k = 0; k <= TZ_REINOS && !v->erro; ++k) {
		TzReinoEstado *r = &m->reino[k];
		VV(r->ativo); VV(r->ia); VV(r->civ); VV(r->cor); VV(r->equipe);
		VV(r->derrotado); VV(r->venceu); VV(r->perdeu); VV(r->nao_pode_perder); VV(r->nao_pode_ganhar); VV(r->rendido);
		for (int c = 0; c < (int)sizeof r->nome; ++c) VV(r->nome[c]);
		VA(r->recursos); VV(r->populacao); VV(r->populacao_max); VV(r->populacao_limite); VA(r->diplomacia);
		VV(r->pontos); VV(r->criadas); VV(r->perdidas); VV(r->matadas); VA(r->recolhido);
		VV(r->emp_quantia); VV(r->emp_rec); VV(r->emp_aumenta_tic); VV(r->emp_aumenta_quantia); VV(r->cruzados);
		VV(r->cruzadas_completas); VV(r->cruz_ancla); VV(r->cruz_faltan); VV(r->cruz_proximo); VV(r->proxima_oferta); VV(r->torneio);
		VV(r->joia_mercado); VV(r->njoias); VV(r->joia_prox); VV(r->joia_id);
		if (r->njoias > 8) { v->erro = 1; return; }
		for (int q = 0; q < 8; ++q) {
			struct TzJoia *jo = &r->joia[q];
			VV(jo->id); VV(jo->classe); VV(jo->item.vida); VV(jo->item.mana); VV(jo->item.ataque); VV(jo->item.defesa); VV(jo->item.xp);
			VV(jo->item.feitico); VV(jo->item.finito); VV(jo->item.cargas); VA(jo->item.imagem);
		}
		if (k >= 1 && r->pesquisas) {
			for (int t = 0; t < m->cat->ntec; ++t) VV(r->pesquisas[t]);
			for (int t = 0; t < m->cat->ntec + TZ_NCLASSES; ++t) VV(r->proibido[t]);
		}
	}
	for (int q = 0; q < TZ_VISOES; ++q) { VV(m->visao[q].reino); VV(m->visao[q].x); VV(m->visao[q].y); VV(m->visao[q].raio); VV(m->visao[q].ate); }
	/* casillas que cambian: madera, dibujo (tocones) */
	int lado = m->lado;
	for (int i = 0; i < lado * lado && !v->erro; ++i) { VV(m->cel[i].madeira); VV(m->casillas[i]); }
	/* proyectiles */
	int32_t np = m->nproj;
	VV(np);
	if (np < 0 || np > 1 << 20) { v->erro = 1; return; }
	if (v->modo == V_LER) {
		if (np > m->capproj) {
			void *no = realloc(m->proj, (size_t)np * sizeof *m->proj);
			if (!no) { v->erro = 1; return; }
			m->proj = no; m->capproj = np;
		}
		m->nproj = np;
	}
	for (int i = 0; i < np; ++i) {
		struct TzProjetil *p = &m->proj[i];
		VV(p->atacante); VV(p->alvo); VV(p->classe); VV(p->dano); VV(p->area); VV(p->categoria);
		VV(p->x0); VV(p->y0); VV(p->x1); VV(p->y1); VV(p->sai); VV(p->chega);
	}
}

/* la cola de eventos y sus textos: se guardan (los lectores siguen por su cursor al cargar) pero no entran en la suma */
static void visitar_eventos(V *v, TzMundo *m)
{
	VV(m->ev_seq);
	for (int i = 0; i < TZ_EVENTOS; ++i) {
		TzEvento *e = &m->ev[i];
		VV(e->seq); VV(e->tic); VV(e->tipo); VV(e->reino); VV(e->classe); VV(e->obj); VV(e->alvo); VV(e->x); VV(e->y); VV(e->valor);
	}
	VV(m->ntextos);
	for (int i = 0; i < 64; ++i) for (int c = 0; c < (int)sizeof m->textos[0]; ++c) VV(m->textos[i][c]);
}

uint32_t tz_visitar_suma(const TzMundo *m)
{
	V v = { V_SUMA, NULL, 2166136261u, 0 };
	visitar(&v, (TzMundo *)m);
	/* la niebla del jugador también cuenta (la IA la usa) */
	for (int k = 1; k <= TZ_REINOS; ++k) {
		if (!m->reino[k].nevoa) continue;
		for (int i = 0; i < m->lado * m->lado; ++i) { v.h ^= m->reino[k].nevoa[i]; v.h *= 16777619u; }
	}
	return v.h;
}

static void cab(V *v, TzMundo *m, uint32_t *ver, uint32_t *lado, uint32_t *nmapa)
{
	VV(*ver); VV(*lado); VV(*nmapa);
	(void)m;
}

int tz_mundo_gravar(TzMundo *m, const char *ruta)
{
	FILE *f = fopen(ruta, "wb");
	if (!f) return tz_falha("no se puede escribir %s", ruta);
	fwrite("TZSV", 1, 4, f);
	V v = { V_GRAVAR, f, 0, 0 };
	uint32_t ver = VERSAO, lado = (uint32_t)m->lado, nmapa = (uint32_t)m->mapa->nobj;
	cab(&v, m, &ver, &lado, &nmapa);
	visitar(&v, m);
	for (int k = 1; k <= TZ_REINOS && !v.erro; ++k) if (m->reino[k].nevoa) fwrite(m->reino[k].nevoa, 1, (size_t)m->lado * m->lado, f);
	visitar_eventos(&v, m);
	for (int g = 0; g < m->nganchos && !v.erro; ++g) {
		TzGancho *h = &m->gancho[g];
		if (!h->gravar || !h->etiqueta[0]) continue;
		fwrite(h->etiqueta, 1, 8, f);
		long pos = ftell(f);
		uint32_t tam = 0;
		vb(&v, &tam, 4);
		long ini = ftell(f);
		if (h->gravar(m, h->dados, f)) { v.erro = 1; break; }
		long fim = ftell(f);
		tam = (uint32_t)(fim - ini);
		fseek(f, pos, SEEK_SET);
		vb(&v, &tam, 4);
		fseek(f, fim, SEEK_SET);
	}
	static const char zero[8] = { 0 };
	fwrite(zero, 1, 8, f);
	int erro = v.erro || ferror(f);
	if (fclose(f) || erro) return tz_falha("error al escribir %s", ruta);
	return 0;
}

int tz_mundo_carregar(TzMundo *m, const TzCatalogo *c, const TzMapa *mapa, const char *ruta)
{
	FILE *f = fopen(ruta, "rb");
	if (!f) return tz_falha("no se puede abrir %s", ruta);
	char mg[4];
	V v = { V_LER, f, 0, 0 };
	uint32_t ver = 0, lado = 0, nmapa = 0;
	if (fread(mg, 1, 4, f) != 4 || memcmp(mg, "TZSV", 4)) { fclose(f); return tz_falha("no es una partida de Tzar"); }
	cab(&v, m, &ver, &lado, &nmapa);
	if (v.erro || ver != VERSAO || lado != (uint32_t)mapa->lado || nmapa != (uint32_t)mapa->nobj) { fclose(f); return tz_falha("la partida es de otro mapa o de otra versión"); }
	/* se rehace el mundo del mapa y se le pone encima lo guardado; los ganchos se conservan */
	TzGancho g[TZ_GANCHOS];
	int ng = m->nganchos;
	memcpy(g, m->gancho, sizeof g);
	if (m->cel) tz_mundo_liberar(m);
	TzRegras r = { 0 };
	if (tz_mundo_criar(m, c, mapa, &r, 0)) { fclose(f); return -1; }
	memcpy(m->gancho, g, sizeof g);
	m->nganchos = ng;
	/* fuera de las casillas antes de cambiar posiciones */
	for (int i = 0; i < m->nobj; ++i) tz_marcar(m, i, 0);
	visitar(&v, m);
	for (int k = 1; k <= TZ_REINOS && !v.erro; ++k)
		if (fread(m->reino[k].nevoa, 1, (size_t)m->lado * m->lado, f) != (size_t)m->lado * m->lado) v.erro = 1;
	if (!v.erro) visitar_eventos(&v, m);
	if (v.erro) { fclose(f); tz_mundo_liberar(m); return tz_falha("partida dañada"); }
	/* los objetos nuevos no tienen registro del mapa */
	for (int i = 0; i < m->nobj; ++i) m->obj[i].original = i < mapa->nobj ? &mapa->obj[i] : NULL;
	for (int i = 0; i < m->lado * m->lado; ++i) {
		TzCelula *cl = &m->cel[i];
		uint16_t t = m->casillas[i];
		int troncos = m->regras.troncos ? m->regras.troncos : 3;
		cl->bits &= (uint16_t)~TZ_CEL_ARVORE;
		if (((t >> 11) & 31) > troncos && cl->madeira) cl->bits |= TZ_CEL_ARVORE;
	}
	for (int i = 0; i < m->nobj; ++i) {
		TzObj *o = &m->obj[i];
		if (o->flags & (TZ_F_MORTO | TZ_F_INATIVO | TZ_F_CARREGADO | TZ_F_DENTRO | TZ_F_MORRENDO)) continue;
		tz_marcar(m, i, 1);
	}
	TzPriv *p = m->priv;
	for (int i = 0; i < m->nobj; ++i) if (m->obj[i].id_mapa >= p->prox_id) p->prox_id = (uint16_t)(m->obj[i].id_mapa + 1);
	tz_ilhas(m);
	/* bloques de los ganchos */
	for (;;) {
		char et[8];
		if (fread(et, 1, 8, f) != 8) break;
		int vazio = 1;
		for (int i = 0; i < 8; ++i) if (et[i]) vazio = 0;
		if (vazio) break;
		uint32_t tam = 0;
		vb(&v, &tam, 4);
		if (v.erro) break;
		long ini = ftell(f);
		for (int q = 0; q < m->nganchos; ++q)
			if (m->gancho[q].carregar && !memcmp(m->gancho[q].etiqueta, et, 8)) m->gancho[q].carregar(m, m->gancho[q].dados, f, tam);
		fseek(f, ini + (long)tam, SEEK_SET);
	}
	fclose(f);
	return 0;
}
