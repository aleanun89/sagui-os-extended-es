/* SPDX-License-Identifier: GPL-3.0-or-later */
/* itens.c - los objetos de Tzar: recoger, llevar, soltar, usar (pociones, libros, pergaminos, equipo) y vender (D417,
 * v8.5.5.2).
 *
 * Traducido a C (simplificado donde se indica) de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later), gameplay.rs:
 *  - Intent::PickItem, DropItem, UseItem (0x42e993 y el programa 17), item_command y use_held;
 *  - refresh_equipment (0x463480, 0x4634f0): un ITEM_PLUSPLUS que se lleva suma su ataque (los dos bytes) y su defensa a
 *    quien lo lleva, y se los quita al soltarlo (aquí lo suma tz_recalcular, en mundo.c);
 *  - drink (0x464ac0): la poción sube el máximo de vida y de maná y rellena lo mismo (la vida no sube en una piedra), suma
 *    ataque y defensa para siempre y da experiencia (el campo es experiencia, no niveles);
 *  - read_book (0x4640f0): sin objetivo, el libro enseña su hechizo a quien tenga maná y no lo conozca y se gasta; con objetivo
 *    lanza el hechizo sin maná gastando una carga (el libro sin cargas desaparece);
 *  - item_value (vt+0x2c4) y la venta en el mercado (oro igual al valor), y la lista de hechizos de world.rs spell_key.
 * Diferencias: los pergaminos no abren historia (la guarda el guion), el libro no tiene tiempo de recarga de maná y las
 * piedras del alma no están.
 */
#include "mundo_int.h"
#include <stdlib.h>
#include <string.h>

int tz_mundo_capacidade_itens(const TzMundo *m, TzId i)
{
	const TzObj *o = tz_mundo_obj((TzMundo *)m, i);
	if (!o || !tz_obj_unidade(o) || !o->ator) return 0;
	const TzAtributos *at = tz_obj_at(m, o);
	return at->naval ? 0 : at->capacidade;
}

int tz_mundo_itens(const TzMundo *m, TzId i, TzId *out, int max)
{
	int n = 0;
	for (int q = 0; q < m->nobj && n < max; ++q) {
		const TzObj *t = &m->obj[q];
		if ((t->flags & TZ_F_CARREGADO) && t->dentro_de == i && !(t->flags & (TZ_F_MORTO | TZ_F_INATIVO)) && tz_obj_item(t)) out[n++] = q;
	}
	return n;
}

static int no_chao(const TzObj *t) { return tz_obj_item(t) && !(t->flags & (TZ_F_CARREGADO | TZ_F_MORTO | TZ_F_INATIVO | TZ_F_MORRENDO)); }

/* ¿hay otro objeto en el suelo de esa casilla? (un objeto por casilla, 0x5072a0) */
static int item_em(const TzMundo *m, int x, int y, TzId menos)
{
	for (int q = 0; q < m->nobj; ++q) if (q != menos && no_chao(&m->obj[q]) && m->obj[q].cx == x && m->obj[q].cy == y) return 1;
	return 0;
}

/* la casilla libre de objetos más cercana a (x, y) por la que se pueda andar, hasta ocho casillas */
static int casilla_item(const TzMundo *m, int x0, int y0, TzId menos, int *ox, int *oy)
{
	for (int d = 0; d <= 8; ++d)
		for (int dy = -d; dy <= d; ++dy) for (int dx = -d; dx <= d; ++dx) {
			if (dx != -d && dx != d && dy != -d && dy != d) continue;
			int x = x0 + dx, y = y0 + dy;
			if (x < 0 || y < 0 || x >= m->lado || y >= m->lado) continue;
			if (!tz_cel_transitavel(m, TZ_CAPA_TERRA, 0, x, y) || item_em(m, x, y, menos)) continue;
			*ox = x; *oy = y;
			return 1;
		}
	return 0;
}

static void poner_en_suelo(TzMundo *m, TzId j, int x, int y)
{
	TzObj *t = &m->obj[j];
	t->cx = (int16_t)x; t->cy = (int16_t)y;
	t->px = x * TZ_SUB + TZ_SUB / 2; t->py = y * TZ_SUB + TZ_SUB / 2;
}

/* el objeto deja a quien lo lleva (se suelta o se gasta); quien lo llevaba recalcula ataque y defensa */
static void dejar(TzMundo *m, TzId j)
{
	TzObj *t = &m->obj[j];
	TzId h = t->dentro_de;
	t->flags &= ~(uint32_t)TZ_F_CARREGADO;
	t->dentro_de = TZ_NENHUM;
	if (h >= 0 && h < m->nobj && m->obj[h].ator) {
		if (m->obj[h].ator->nitens) m->obj[h].ator->nitens--;
		tz_recalcular(m, h);
	}
}

static void gastar(TzMundo *m, TzId j)
{
	dejar(m, j);
	m->obj[j].flags |= TZ_F_INATIVO;
}
void tz_item_gastar(TzMundo *m, TzId j) { gastar(m, j); }

void tz_largar_itens(TzMundo *m, TzId i)
{
	TzObj *h = &m->obj[i];
	if (!h->ator || !h->ator->nitens) return;
	for (int q = 0; q < m->nobj; ++q) {
		TzObj *t = &m->obj[q];
		if (!(t->flags & TZ_F_CARREGADO) || t->dentro_de != i) continue;
		int x, y;
		dejar(m, q);
		if (casilla_item(m, h->cx, h->cy, q, &x, &y)) poner_en_suelo(m, q, x, y);
		else t->flags |= TZ_F_INATIVO;
		tz_ev(m, TZ_EV_ITEM, h->dono, i, q, h->cx, h->cy, t->classe, 2);
	}
	h->ator->nitens = 0;
}

TzId tz_mundo_criar_item(TzMundo *m, int classe, int cx, int cy, const TzItem *item)
{
	TzObj tmp; memset(&tmp, 0, sizeof tmp); tmp.classe = (uint16_t)classe;
	if (!tz_obj_item(&tmp)) return TZ_NENHUM;
	int x, y;
	if (!casilla_item(m, cx, cy, TZ_NENHUM, &x, &y)) return TZ_NENHUM;
	TzId j = tz_novo_obj(m, classe, 1, 0, x, y, 0);
	if (j < 0) return j;
	if (item) m->obj[j].item = *item;
	tz_ev(m, TZ_EV_CRIADO, 0, j, TZ_NENHUM, x, y, classe, 0);
	return j;
}

/* ------------------------------------------------------------------ valor y venta */
uint32_t tz_mundo_valor_item(const TzMundo *m, TzId j)
{
	const TzObj *t = tz_mundo_obj((TzMundo *)m, j);
	if (!t) return 0;
	static const uint8_t tarifa[21] = { 50, 20, 5, 0, 0, 5, 50, 30, 10, 10, 0, 5, 10, 30, 10, 20, 0, 30, 5, 0, 30 };
	switch (t->classe) {
	case TZC_ITEM_MAIS: return 4u * ((uint32_t)t->item.ataque + t->item.defesa);
	case TZC_LIVRO: {
		int cl = t->item.feitico == 3 ? 22 : t->item.feitico == 4 ? 15 : t->item.feitico == 10 ? 16 : t->item.feitico == 16 ? 32 : t->item.feitico == 19 ? 30 : 0;
		uint32_t tf = cl ? tz_catalogo_at(m->cat, cl, 1)->custo[TZ_OURO] : t->item.feitico < 21 ? tarifa[t->item.feitico] : 10;
		uint32_t x = tf * t->item.cargas;
		return x + x / 2;
	}
	case TZC_POCAO: return (uint32_t)t->item.vida + (uint32_t)t->item.mana + 5u * t->item.xp + 2u * ((uint32_t)t->item.ataque + t->item.defesa);
	}
	return 0;
}

void tz_vender_item(TzMundo *m, int k, TzId mercado, TzId j)
{
	if (j < 0 || j >= m->nobj) return;
	TzObj *t = &m->obj[j];
	if (!(t->flags & TZ_F_CARREGADO) || t->dentro_de < 0 || m->obj[t->dentro_de].dono != k || !tz_obj_item(t)) return;
	uint32_t v = tz_mundo_valor_item(m, j);
	if (!v) { tz_falha("ese objeto no se vende"); return; }
	m->reino[k].recursos[TZ_OURO] += (int32_t)v;
	tz_ev(m, TZ_EV_ITEM, k, mercado, j, m->obj[mercado].cx, m->obj[mercado].cy, t->classe, 4);
	gastar(m, j);
}

/* ------------------------------------------------------------------ pociones y libros */
static void beber(TzMundo *m, TzId t, TzId j)
{
	TzObj *u = &m->obj[t];
	TzObj *p = &m->obj[j];
	int pedra = u->ator && u->ator->efeito == TZ_EF_PEDRA && m->tic < u->ator->efeito_ate;
	u->vida_max += p->item.vida;
	if (!pedra) { u->vida += p->item.vida; if (u->vida > u->vida_max) u->vida = u->vida_max; }
	u->mana_max = (uint16_t)(u->mana_max + p->item.mana);
	u->mana = (uint16_t)(u->mana + p->item.mana); if (u->mana > u->mana_max) u->mana = u->mana_max;
	u->pocao_ataque = (uint16_t)(u->pocao_ataque + p->item.ataque);
	u->pocao_defesa = (uint16_t)(u->pocao_defesa + p->item.defesa);
	TzItem it = p->item;
	tz_ev(m, TZ_EV_ITEM, u->dono, t, j, u->cx, u->cy, p->classe, 3);
	gastar(m, j);
	tz_recalcular(m, t);
	if (it.xp) tz_experiencia(m, t, it.xp, 1);
}

/* aprender del libro: quien tiene maná y no lo conoce; el libro se gasta */
static int aprender(TzMundo *m, TzId i, TzId j)
{
	TzObj *u = &m->obj[i];
	const char *k = tz_feitico_chave(m->obj[j].item.feitico);
	if (!k || !u->mana_max || tz_mundo_conhece_feitico(m, i, k) || u->ator->naprendidos >= 8) return -1;
	u->ator->aprendidos[u->ator->naprendidos++] = m->obj[j].item.feitico;
	tz_ev(m, TZ_EV_ITEM, u->dono, i, j, u->cx, u->cy, m->obj[j].classe, 3);
	gastar(m, j);
	return 0;
}

/* ------------------------------------------------------------------ la orden de cada tic */
void tz_item_tic(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	TzOrdem ord = a->ordem;
	TzId j = ord.alvo;
	if (j < 0 || j >= m->nobj || !tz_obj_item(&m->obj[j])) { tz_proxima_ordem(m, i); return; }
	TzObj *t = &m->obj[j];
	if (ord.tipo == TZ_O_PEGAR_ITEM) {
		if (!no_chao(t) || a->nitens >= tz_mundo_capacidade_itens(m, i)) { tz_proxima_ordem(m, i); return; }
		int r[4] = { t->cx, t->cy, t->cx, t->cy };
		int res = tz_mover_para(m, i, r, 1);
		if (res < 0) { tz_proxima_ordem(m, i); return; }
		if (res == 0) return;
		t->flags |= TZ_F_CARREGADO;
		t->dentro_de = i;
		a->nitens++;
		tz_recalcular(m, i);
		tz_ev(m, TZ_EV_ITEM, o->dono, i, j, t->cx, t->cy, t->classe, 1);
		tz_proxima_ordem(m, i);
		return;
	}
	/* los otros dos son sobre un objeto que se lleva */
	if (!(t->flags & TZ_F_CARREGADO) || t->dentro_de != i) { tz_proxima_ordem(m, i); return; }
	if (ord.tipo == TZ_O_SOLTAR_ITEM) {
		TzId destino = ord.arg == 1 ? tz_mundo_em(m, ord.x, ord.y) : TZ_NENHUM;
		if (destino >= 0 && destino != i && tz_obj_unidade(&m->obj[destino]) && tz_obj_vivo(&m->obj[destino])) {
			/* dárselo a otra unidad que lo pueda llevar */
			TzObj *d = &m->obj[destino];
			if (d->ator->nitens >= tz_mundo_capacidade_itens(m, destino)) { tz_proxima_ordem(m, i); return; }
			int r[4] = { d->cx, d->cy, d->cx, d->cy };
			int res = tz_mover_para(m, i, r, 1);
			if (res < 0) { tz_proxima_ordem(m, i); return; }
			if (res == 0) return;
			dejar(m, j);
			t->flags |= TZ_F_CARREGADO; t->dentro_de = destino;
			d->ator->nitens++;
			tz_recalcular(m, i); tz_recalcular(m, destino);
			tz_ev(m, TZ_EV_ITEM, o->dono, i, j, d->cx, d->cy, t->classe, 2);
			tz_proxima_ordem(m, i);
			return;
		}
		int x = ord.arg == 1 ? ord.x : o->cx, y = ord.arg == 1 ? ord.y : o->cy;
		if (ord.arg == 1) {
			int r[4] = { x, y, x, y };
			int res = tz_mover_para(m, i, r, 1);
			if (res < 0) { tz_proxima_ordem(m, i); return; }
			if (res == 0) return;
		}
		int fx, fy;
		if (!casilla_item(m, x, y, j, &fx, &fy)) { tz_proxima_ordem(m, i); return; }
		dejar(m, j);
		poner_en_suelo(m, j, fx, fy);
		tz_ev(m, TZ_EV_ITEM, o->dono, i, j, fx, fy, t->classe, 2);
		tz_proxima_ordem(m, i);
		return;
	}
	/* TZ_O_USAR_ITEM: arg = 1 con objetivo en (x, y); si no, quien lo lleva */
	TzId alvo = i;
	if (ord.arg == 1) {
		alvo = tz_mundo_em(m, ord.x, ord.y);
		if (t->classe == TZC_POCAO && (alvo < 0 || !tz_obj_unidade(&m->obj[alvo]) || !tz_obj_vivo(&m->obj[alvo]))) { tz_proxima_ordem(m, i); return; }
		if (alvo >= 0 && alvo != i && t->classe == TZC_POCAO && tz_obj_vivo(&m->obj[alvo])) {
			int r[4]; tz_obj_ret(m, &m->obj[alvo], r);
			int res = tz_mover_para(m, i, r, 1);
			if (res < 0) { tz_proxima_ordem(m, i); return; }
			if (res == 0) return;
		}
	}
	if (a->passo < a->passo_total) return;
	switch (t->classe) {
	case TZC_POCAO:
		if (alvo != i && tz_obj_dist(m, o, &m->obj[alvo]) > 1) return;
		beber(m, alvo, j);
		break;
	case TZC_LIVRO:
		if (ord.arg != 1) { if (aprender(m, i, j)) tz_ev(m, TZ_EV_SEM_RECURSOS, o->dono, i, j, o->cx, o->cy, 4, 0); break; }
		else {
			int ordi = tz_feitico_ordem(m, tz_feitico_chave(t->item.feitico));
			if (ordi < 0 || (t->item.finito && !t->item.cargas)) { gastar(m, j); break; }
			a->ordem.tipo = TZ_O_HECHIZO; a->ordem.arg = (int16_t)ordi; a->ordem.alvo = ord.alvo == j && alvo >= 0 && alvo != i ? alvo : TZ_NENHUM;
			a->ordem.x = ord.x; a->ordem.y = ord.y;
			a->ncaminho = a->icaminho = 0;
			a->livro = j;
			return;
		}
		break;
	default:
		tz_ev(m, TZ_EV_ITEM, o->dono, i, j, o->cx, o->cy, t->classe, 3);
		break;
	}
	tz_proxima_ordem(m, i);
}
