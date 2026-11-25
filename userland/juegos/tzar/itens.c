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
 * v8.5.6: las piedras del alma (magia.c); los pergaminos abren su historia. El libro no tiene tiempo de recarga de maná.
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
static uint32_t valor_de(const TzMundo *m, int classe, const TzItem *item)
{
	struct { int classe; const TzItem *item; } tt = { classe, item }, *t = &tt;
	static const uint8_t tarifa[21] = { 50, 20, 5, 0, 0, 5, 50, 30, 10, 10, 0, 5, 10, 30, 10, 20, 0, 30, 5, 0, 30 };
	switch (t->classe) {
	case TZC_ITEM_MAIS: return 4u * ((uint32_t)t->item->ataque + t->item->defesa);
	case TZC_LIVRO: {
		int cl = t->item->feitico == 3 ? 22 : t->item->feitico == 4 ? 15 : t->item->feitico == 10 ? 16 : t->item->feitico == 16 ? 32 : t->item->feitico == 19 ? 30 : 0;
		uint32_t tf = cl ? tz_catalogo_at(m->cat, cl, 1)->custo[TZ_OURO] : t->item->feitico < 21 ? tarifa[t->item->feitico] : 10;
		uint32_t x = tf * t->item->cargas;
		return x + x / 2;
	}
	case TZC_POCAO: return (uint32_t)t->item->vida + (uint32_t)t->item->mana + 5u * t->item->xp + 2u * ((uint32_t)t->item->ataque + t->item->defesa);
	}
	return 0;
}

uint32_t tz_mundo_valor_item(const TzMundo *m, TzId j)
{
	const TzObj *t = tz_mundo_obj((TzMundo *)m, j);
	return t ? valor_de(m, t->classe, &t->item) : 0;
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
		if (t->classe == TZC_PERGAMINHO) tz_ev(m, TZ_EV_PERGAMINHO, o->dono, i, j, t->cx, t->cy, t->classe, 1);
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
	case TZC_PERGAMINHO:
		tz_ev(m, TZ_EV_PERGAMINHO, o->dono, i, j, o->cx, o->cy, t->classe, 0);
		break;
	default:
		tz_ev(m, TZ_EV_ITEM, o->dono, i, j, o->cx, o->cy, t->classe, 3);
		break;
	}
	tz_proxima_ordem(m, i);
}

/* ------------------------------------------------------------------ fábrica mágica y joyas del mercado (v8.5.6)
 * Traducido de ReTzar, gameplay.rs: book_minimum y book_divisor (0x4a5220, 0x465ca0), create_item (0x4743b0: el objeto de la
 * fábrica mágica, con el oro ya pagado, que sale por una esquina del edificio; el código original cobra otra vez lo mismo si el
 * reino puede), market_jewels (el mercado asiático con Jewelry ofrece un objeto al minuto, hasta ocho) y random_item (el registro
 * al azar: libro, equipo o poción). Una fábrica a la vez por reino (K+0x12a). */
static uint32_t oro_criatura(const TzMundo *m, int classe) { return tz_catalogo_at(m->cat, classe, 1)->custo[TZ_OURO]; }

uint32_t tz_mundo_livro_minimo(const TzMundo *m, int f)
{
	switch (f) {
	case 0: case 6: return 200;
	case 7: case 13: case 17: case 20: return 120;
	case 1: case 15: return 80;
	case 4: case 8: case 9: case 12: case 14: return 40;
	case 3: return 4 * oro_criatura(m, 22);
	case 10: return 4 * oro_criatura(m, 16);
	case 16: return 4 * oro_criatura(m, 32);
	case 19: return 4 * oro_criatura(m, 30);
	}
	return 20;
}

static uint32_t livro_divisor(const TzMundo *m, int f)
{
	static const uint8_t d[21] = { 50, 20, 5, 0, 1, 5, 50, 30, 10, 10, 0, 5, 10, 30, 10, 20, 0, 30, 5, 0, 30 };
	uint32_t r = f == 3 ? oro_criatura(m, 22) : f == 10 ? oro_criatura(m, 16) : f == 16 ? oro_criatura(m, 32) : f == 19 ? oro_criatura(m, 30) : f >= 0 && f < 21 ? d[f] : 1;
	return r ? r : 1;
}

static int classe_oficina(void)
{
	static int c = -2;
	if (c == -2) c = tz_catalogo_classe("magicworkshop");
	return c;
}

int tz_mundo_fabrica_ocupada(const TzMundo *m, int k)
{
	for (int q = 0; q < m->nobj; ++q) {
		const TzObj *o = &m->obj[q];
		if (o->dono != k || !o->ator || !tz_obj_vivo(o)) continue;
		for (int w = 0; w < o->ator->ntrab; ++w) if (o->ator->trab[w].tipo == TZ_J_ITEM) return 1;
	}
	return 0;
}

/* la suma de los dos bytes de una palabra (así lee el registro del mapa su «ataque») */
static uint16_t dois_bytes(uint32_t v) { v &= 0xffff; return (uint16_t)((v & 255) + (v >> 8)); }

/* el objeto de la fábrica de la categoría e índice dados con el oro pagado; 0 si no es válido */
static int feito_fabrica(const TzMundo *m, int cat, int idx, uint32_t ouro, int *classe, TzItem *it, uint32_t *de_novo)
{
	memset(it, 0, sizeof *it);
	uint32_t g4 = ouro / 4 > 65535 ? 65535 : ouro / 4, meio = ouro / 2 > 65535 ? 65535 : ouro / 2, o16 = ouro > 65535 ? 65535 : ouro;
	*de_novo = ouro;
	if (cat == 0) {
		if (idx < 0 || idx > 20) return 0;
		uint32_t d = livro_divisor(m, idx), cargas = g4 / d;
		if (!cargas) return 0;
		if (cargas > 65535) cargas = 65535;
		*classe = TZC_LIVRO; it->feitico = (uint8_t)idx; it->finito = 1; it->cargas = (uint16_t)cargas;
		snprintf(it->imagem, sizeof it->imagem, "item_book1");
		*de_novo = cargas * d;
		return 1;
	}
	if (cat == 1) {
		static const char *const img[5] = { "item_potion2", "item_potion3", "item_potion1", "item_potion7", "item_potion6" };
		if (idx < 0 || idx > 4) return 0;
		*classe = TZC_POCAO;
		if (idx == 0) it->vida = (int32_t)o16;
		else if (idx == 1) it->xp = (uint16_t)(ouro / 5);
		else if (idx == 2) it->mana = (int32_t)o16;
		else if (idx == 3) it->ataque = dois_bytes(meio);
		else it->defesa = (uint16_t)meio;
		snprintf(it->imagem, sizeof it->imagem, "%s", img[idx]);
		return 1;
	}
	if (cat == 2) {
		if (idx < 0 || idx > 2) return 0;
		*classe = TZC_ITEM_MAIS;
		uint32_t a = g4 < 250 ? g4 : 250;
		if (idx == 0) { it->ataque = (uint16_t)a; snprintf(it->imagem, sizeof it->imagem, "item_sword1"); }
		else if (idx == 1) { it->ataque = (uint16_t)a; snprintf(it->imagem, sizeof it->imagem, "item_bow1"); }
		else { it->defesa = (uint16_t)g4; snprintf(it->imagem, sizeof it->imagem, "item_shield1"); }
		return 1;
	}
	return 0;
}

/* un objeto hecho en la partida aparece por un lado del edificio (esquina al azar) en la casilla libre más cercana */
static TzId aparece_junto(TzMundo *m, TzId edificio, int classe, const TzItem *it)
{
	const TzObj *b = &m->obj[edificio];
	int r[4];
	tz_obj_ret(m, b, r);
	int esq = tz_rng_entre(&m->rng, 0, 3);
	int x = esq == 0 || esq == 2 ? r[2] + 1 : r[0] - 1, y = esq <= 1 ? r[3] + 1 : r[1] - 1;
	x = x < 0 ? 0 : x >= m->lado ? m->lado - 1 : x;
	y = y < 0 ? 0 : y >= m->lado ? m->lado - 1 : y;
	TzId j = tz_mundo_criar_item(m, classe, x, y, it);
	if (j >= 0) {
		m->obj[j].item = *it;
		if (classe == TZC_LIVRO) m->obj[j].item.finito = 1;
	}
	return j;
}

/* el trabajo TZ_J_ITEM acaba (acoes.c): arg = categoría | (índice << 2), el oro en pago[0] */
void tz_fabrica_pronta(TzMundo *m, TzId e, int arg, uint32_t ouro)
{
	TzObj *o = &m->obj[e];
	int cat = arg & 3, idx = arg >> 2, classe = 0;
	TzItem it;
	uint32_t outra;
	if (!feito_fabrica(m, cat, idx, ouro, &classe, &it, &outra)) {
		tz_falha("este libro mágico necesita más oro");   /* el libro se descarta y el oro está gastado */
		return;
	}
	TzId j = aparece_junto(m, e, classe, &it);
	o = &m->obj[e];
	if (j < 0) return;
	m->obj[j].dono = 0;
	tz_ev(m, TZ_EV_CRIADO, o->dono, j, e, m->obj[j].cx, m->obj[j].cy, classe, 5);
	/* el original vuelve a cobrar lo mismo si el reino puede pagarlo */
	if (o->dono >= 1 && o->dono <= TZ_REINOS && m->reino[o->dono].recursos[TZ_OURO] >= (int32_t)outra) m->reino[o->dono].recursos[TZ_OURO] -= (int32_t)outra;
}

static void fabricar(TzMundo *m, const TzComando *c)
{
	if (c->nids < 1 || c->ids[0] < 0 || c->ids[0] >= m->nobj) return;
	TzObj *e = &m->obj[c->ids[0]];
	int k = c->reino;
	if (e->classe != classe_oficina() || e->dono != k || !tz_obj_vivo(e) || (e->flags & TZ_F_OBRA) || !e->ator) return;
	int cat = c->arg & 3, idx = c->arg >> 2;
	uint32_t ouro = c->valor > 0 ? (uint32_t)c->valor : 0;
	int ok = cat == 0 ? idx >= 0 && idx < 21 && ouro >= tz_mundo_livro_minimo(m, idx) : cat == 1 ? idx >= 0 && idx < 5 && ouro > 0 : cat == 2 ? idx >= 0 && idx < 3 && ouro > 0 : 0;
	if (!ok) { tz_falha("oro insuficiente para este objeto"); return; }
	if (tz_mundo_fabrica_ocupada(m, k)) { tz_falha("ya se está fabricando un objeto mágico"); return; }
	if (e->ator->ntrab >= TZ_FILA_TRABALHO) { tz_falha("la cola está llena"); return; }
	uint32_t custo[TZ_NREC] = { ouro };
	if (tz_pagar(m, k, custo)) return;
	TzTrabalho *t = &e->ator->trab[e->ator->ntrab++];
	memset(t, 0, sizeof *t);
	t->tipo = TZ_J_ITEM; t->arg = (int16_t)c->arg;
	t->pago[TZ_OURO] = ouro;
	t->total_ms = t->restante_ms = (uint32_t)((uint64_t)ouro * (cat == 0 ? 500 : 250));
	if (!t->total_ms) t->total_ms = t->restante_ms = 1;
}

/* random_item (vt+0x2c8): un libro de un hechizo al azar (8 da 1 y 13 da 17), equipo o poción con cifras al azar */
static void item_ao_acaso(TzMundo *m, int classe, TzItem *it)
{
	memset(it, 0, sizeof *it);
	if (classe == TZC_LIVRO) {
		int f = tz_rng_entre(&m->rng, 0, 20);
		if (f == 8) f = 1; else if (f == 13) f = 17;
		uint32_t min = m->tic * TZ_TIC_MS / 60000u;
		it->feitico = (uint8_t)f; it->finito = 1;
		it->cargas = (uint16_t)(min <= 10 ? 1 : min <= 20 ? tz_rng_entre(&m->rng, 1, 3) : tz_rng_entre(&m->rng, 1, 10));
		snprintf(it->imagem, sizeof it->imagem, "item_book1");
	} else if (classe == TZC_ITEM_MAIS) {
		int t = tz_rng_entre(&m->rng, 0, 2), v = tz_rng_entre(&m->rng, 5, 20);
		if (t == 0) { it->ataque = (uint16_t)v; snprintf(it->imagem, sizeof it->imagem, "item_sword1"); }
		else if (t == 1) { it->ataque = (uint16_t)v; snprintf(it->imagem, sizeof it->imagem, "item_bow1"); }
		else { it->defesa = (uint16_t)v; snprintf(it->imagem, sizeof it->imagem, "item_shield1"); }
	} else {
		int t = tz_rng_entre(&m->rng, 0, 4);
		if (t == 0) { it->ataque = (uint16_t)tz_rng_entre(&m->rng, 5, 20); snprintf(it->imagem, sizeof it->imagem, "item_potion6"); }
		else if (t == 1) { it->defesa = (uint16_t)tz_rng_entre(&m->rng, 5, 20); snprintf(it->imagem, sizeof it->imagem, "item_potion7"); }
		else if (t == 2) { it->vida = tz_rng_entre(&m->rng, 25, 100); snprintf(it->imagem, sizeof it->imagem, "item_potion2"); }
		else if (t == 3) { it->xp = (uint16_t)tz_rng_entre(&m->rng, 5, 20); snprintf(it->imagem, sizeof it->imagem, "item_potion3"); }
		else { it->mana = tz_rng_entre(&m->rng, 25, 100); snprintf(it->imagem, sizeof it->imagem, "item_potion1"); }
	}
}

int tz_mundo_joias(const TzMundo *m, int k, struct TzJoia *out, int max)
{
	if (k < 1 || k > TZ_REINOS) return 0;
	int n = m->reino[k].njoias < max ? m->reino[k].njoias : max;
	for (int q = 0; q < n; ++q) out[q] = m->reino[k].joia[q];
	return n;
}

uint32_t tz_mundo_preco_joia(const TzMundo *m, const struct TzJoia *j) { return 4u * valor_de(m, j->classe, &j->item); }

/* cada mercado del reino mira cada minuto (el primero, un segundo después de abrir); con Jewelry, el primero que llegó ofrece */
void tz_joias_tic(TzMundo *m, int k)
{
	TzReinoEstado *r = &m->reino[k];
	if (!tz_pesquisas(m, k, "te_jewelry")) return;
	TzId mercado = TZ_NENHUM;
	if (r->joia_mercado) {
		TzId q = r->joia_mercado - 1;
		if (q < m->nobj && m->obj[q].classe == TZC_MARKET && m->obj[q].dono == k && tz_obj_vivo(&m->obj[q]) && !(m->obj[q].flags & TZ_F_OBRA)) mercado = q;
	}
	if (mercado < 0)
		for (int q = 0; q < m->nobj; ++q) {
			const TzObj *o = &m->obj[q];
			if (o->classe == TZC_MARKET && o->dono == k && tz_obj_vivo(o) && !(o->flags & TZ_F_OBRA)) { mercado = q; break; }
		}
	if (mercado < 0) { r->joia_mercado = 0; r->joia_prox = 0; return; }
	r->joia_mercado = mercado + 1;
	if (!r->joia_prox) r->joia_prox = m->tic + TZ_TICS_SEG;
	if (m->tic < r->joia_prox) return;
	r->joia_prox = m->tic + 60 * TZ_TICS_SEG;
	static const int classes[4] = { TZC_LIVRO, TZC_ITEM_MAIS, TZC_LIVRO, TZC_POCAO };
	int cl = classes[tz_rng_entre(&m->rng, 0, 3)];
	if (r->njoias >= 8) { memmove(&r->joia[0], &r->joia[1], 7 * sizeof r->joia[0]); r->njoias = 7; }
	struct TzJoia *j = &r->joia[r->njoias++];
	memset(j, 0, sizeof *j);
	j->id = ++r->joia_id; j->classe = (uint16_t)cl;
	item_ao_acaso(m, cl, &j->item);
}

static void comprar(TzMundo *m, const TzComando *c)
{
	if (c->nids < 1 || c->ids[0] < 0 || c->ids[0] >= m->nobj) return;
	int k = c->reino;
	const TzObj *mk = &m->obj[c->ids[0]];
	if (mk->classe != TZC_MARKET || mk->dono != k || !tz_obj_vivo(mk) || (mk->flags & TZ_F_OBRA)) return;
	TzReinoEstado *r = &m->reino[k];
	int q = -1;
	for (int w = 0; w < r->njoias; ++w) if (r->joia[w].id == c->arg) q = w;
	if (q < 0) { tz_falha("la oferta ya no está"); return; }
	struct TzJoia j = r->joia[q];
	uint32_t custo[TZ_NREC] = { tz_mundo_preco_joia(m, &j) };
	if (tz_pagar(m, k, custo)) return;
	TzId n = aparece_junto(m, c->ids[0], j.classe, &j.item);
	if (n >= 0) { m->obj[n].dono = 0; tz_ev(m, TZ_EV_CRIADO, k, n, c->ids[0], m->obj[n].cx, m->obj[n].cy, j.classe, 6); }
	memmove(&r->joia[q], &r->joia[q + 1], (size_t)(r->njoias - q - 1) * sizeof r->joia[0]);
	r->njoias--;
}

void tz_fabrica_comando(TzMundo *m, const TzComando *c)
{
	if (c->tipo == TZ_C_FABRICAR) fabricar(m, c);
	else comprar(m, c);
}

/* scroll_story / scroll_entered (gameplay.rs): el texto está en el registro del objeto del mapa */
static void cadena(const TzObj *o, int at, int max, char *out, size_t n)
{
	out[0] = 0;
	if (!o->original || (int)o->original->ndados < at + 1) return;
	int l = 0;
	while (l < max && at + l < (int)o->original->ndados && o->original->dados[at + l]) ++l;
	if ((size_t)l >= n) l = (int)n - 1;
	memcpy(out, o->original->dados + at, (size_t)l);
	out[l] = 0;
}

int tz_mundo_pergaminho(const TzMundo *m, TzId j, int al_llegar, char *texto, size_t nt, char *retrato, char *som, char *musica, uint32_t *ms, int *fecha)
{
	const TzObj *o = tz_mundo_obj((TzMundo *)m, j);
	if (!o || o->classe != TZC_PERGAMINHO || !o->original) return 0;
	const uint8_t *d = o->original->dados;
	uint32_t n = o->original->ndados;
	uint32_t t = n >= 0x524 ? tz_u32(d + 0x520) : 0;
	if (al_llegar && (n < 0x524 || t == 0xffffffffu)) return 0;
	cadena(o, 96, 800, texto, nt);
	if (!texto[0]) return 0;
	cadena(o, 0x460, 64, retrato, 80);
	cadena(o, 0x4a0, 64, som, 64);
	cadena(o, 0x4e0, 64, musica, 64);
	*ms = n >= 0x524 && t != 0xffffffffu ? t : 0;
	*fecha = al_llegar && n >= 0x528 && tz_u32(d + 0x524) != 0;
	return 1;
}
