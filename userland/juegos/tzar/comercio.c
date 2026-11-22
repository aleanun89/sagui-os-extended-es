/* SPDX-License-Identifier: GPL-3.0-or-later */
/* comercio.c - rutas de mercaderes entre mercados, intercambio de recursos, préstamos, la taberna (mercenarios y
 * héroes), las cruzadas de la catedral y los cambios de clase (monje a sacerdote, peón a guerrero de la yihad) de Tzar
 * (D417, v8.5.5.2).
 *
 * Traducido a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later), gameplay.rs:
 *  - Intent::Trade (la ruta: 0x4e8060 y el programa 0x4e7c98): la unidad va al mercado, lo vende todo lo que lleva del
 *    otro extremo (con la ganancia si lo cargado es el recurso de ese extremo), carga lo suyo de 100 en 100 como
 *    mucho (1000 / mercados del dueño) cada segundo hasta completar y marcha al otro; un solo mercader a la vez por
 *    mercado y ninguno en una obra; trade_capacity (500, 300 o 100 según la civilización), trade_bonus, route_distance
 *    (Chebyshev entre centros, 0 por debajo de 12) y profit;
 *  - «trade1» de la taberna (0x4d3104): de 500 a 20000 de 500 en 500 a cambio de una décima parte, una unidad cada 300 ms;
 *  - «loan» y «loan_return» (0x4d2cec, 0x4d2d6a): 1000, y 200 más por cada préstamo devuelto, un minuto después;
 *  - tavern_offers y add_hire (0x4d2030, 0x4d2e2d) y hire_price (0x4d23b0): una oferta de mercenarios (8 a 12 de nivel 0)
 *    y otra de héroes (nivel 10 a 30) cada 60 s según Mercenaries y Heroes Quest, hasta ocho de cada, con sus precios;
 *  - economy() (cruzadas, 0x439640, 0x439760, 0x439d50, 0x422ca0): la catedral suma cruzados hasta el techo (16, +4 por
 *    cruzada completa, 36 como mucho) y la cruzada los llama uno cada 500 ms en casillas libres alrededor del ancla;
 *  - convert (become_priest, become_jihad_warrior).
 * Diferencias: los precios no llevan las rebajas de investigaciones; los héroes no reciben nombre de HEROES.INI.
 */
#include "mundo_int.h"
#include <stdlib.h>
#include <string.h>

enum { OFERTAS_MAX = 8, TIC_SEG = TZ_TICS_SEG };

static int e_mercado(const TzObj *o) { return o && tz_obj_vivo(o) && (o->classe == TZC_MARKET || o->classe == TZC_HARBOR); }
static int reino_ok(int k) { return k >= 1 && k <= TZ_REINOS; }

/* ------------------------------------------------------------------ rutas de mercaderes */
uint32_t tz_mundo_capacidade_comercio(const TzMundo *m, TzId i)
{
	const TzObj *o = tz_mundo_obj((TzMundo *)m, i);
	if (!o || !tz_obj_unidade(o)) return 0;
	if (o->classe == TZC_PEON && o->civ != TZ_CIV_ASIA) return 0;
	if (o->classe != TZC_PEON && o->classe != TZC_TRADER && !(o->classe >= TZC_GALLEON && o->classe <= TZC_XEBEC)) return 0;
	return o->civ == TZ_CIV_EURO ? 500 : o->civ == TZ_CIV_ARAB ? 300 : 100;
}

/* centro de la huella (ReTzar centre: x + ancho / 2) */
static void centro(const TzMundo *m, const TzObj *o, int *x, int *y)
{
	int r[4];
	tz_obj_ret(m, o, r);
	*x = r[0] + (r[2] - r[0] + 1) / 2;
	*y = r[1] + (r[3] - r[1] + 1) / 2;
}

/* route_distance (0x454e40): Chebyshev entre los centros, 0 por debajo de 12 casillas o con el mismo mercado */
static uint32_t distancia_rota(const TzMundo *m, TzId a, TzId b)
{
	int ax, ay, bx, by;
	centro(m, &m->obj[a], &ax, &ay);
	centro(m, &m->obj[b], &bx, &by);
	int dx = ax > bx ? ax - bx : bx - ax, dy = ay > by ? ay - by : by - ay;
	int span = dx > dy ? dx : dy;
	return span < 12 || a == b ? 0 : (uint32_t)span;
}

/* trade_bonus: Trade taxes +2, Banking +3 sobre la base de 40 */
static uint32_t bonus_comercio(const TzMundo *m, int k)
{
	return 2u * (uint32_t)tz_pesquisas(m, k, "te_tradetaxes") + 3u * (uint32_t)tz_pesquisas(m, k, "te_banking");
}

static uint32_t lucro(uint32_t carga, uint32_t dist, uint32_t bonus, uint32_t lado)
{
	if (!lado) lado = 1;
	return (uint32_t)((uint64_t)carga * dist * (40 + bonus) / ((uint64_t)lado * 100));
}

uint32_t tz_mundo_lucro_rota(const TzMundo *m, int k, TzId a, TzId b, uint32_t carga)
{
	if (a < 0 || b < 0 || a >= m->nobj || b >= m->nobj) return 0;
	return lucro(carga, distancia_rota(m, a, b), bonus_comercio(m, k), (uint32_t)m->lado);
}

static void credito(TzMundo *m, int k, int rec, uint32_t q)
{
	if (reino_ok(k) && rec >= 0 && rec < TZ_NREC) m->reino[k].recursos[rec] += (int32_t)q;
}

/* debit: solo si el reino (con lo que comparten con él) tiene todo; sin aviso (el programa vuelve a probar) */
static int debito(TzMundo *m, int k, int rec, uint32_t q)
{
	if (!reino_ok(k) || rec < 0 || rec >= TZ_NREC) return -1;
	int32_t tem[TZ_NREC];
	tz_mundo_recursos_compartidos(m, k, tem);
	if (tem[rec] < (int32_t)q) return -1;
	uint32_t c[TZ_NREC] = { 0 };
	c[rec] = q;
	tz_gastar(m, k, c);
	return 0;
}

int tz_comercio_ordem(TzMundo *m, TzId i, const TzOrdem *o, int fila)
{
	TzObj *u = &m->obj[i];
	uint32_t cap = tz_mundo_capacidade_comercio(m, i);
	if (!cap) return tz_falha("esta unidad no comercia");
	TzId dest = o->alvo;
	if (dest < 0 || dest >= m->nobj || !e_mercado(&m->obj[dest])) return tz_falha("elige un mercado o puerto");
	TzId origem = TZ_NENHUM;
	if (o->x2 >= 0 && o->y2 >= 0) {
		TzId q = m->lado > 0 && o->x2 < m->lado && o->y2 < m->lado ? m->cel[o->y2 * m->lado + o->x2].fixo : TZ_NENHUM;
		if (q >= 0 && e_mercado(&m->obj[q])) origem = q;
	}
	if (origem < 0) {
		/* sin origen: el mercado propio más cercano a la unidad que no sea el de destino */
		int mel = 1 << 30;
		for (int q = 0; q < m->nobj; ++q) {
			const TzObj *t = &m->obj[q];
			if (q == dest || !e_mercado(t) || t->dono != u->dono || (t->flags & TZ_F_OBRA)) continue;
			int d = tz_obj_dist(m, u, t);
			if (d < mel) { mel = d; origem = q; }
		}
	}
	if (origem < 0) return tz_falha("elige el mercado o puerto de partida");
	TzAtor *a = u->ator;
	struct TzRota r = a->rota;   /* lo que ya lleva se queda */
	r.ativa = 1;
	r.rec[0] = (uint8_t)(o->arg & 3); r.rec[1] = (uint8_t)((o->arg >> 2) & 3);
	uint32_t q = (uint32_t)((o->arg >> 4) & 0x7ff) * 50;
	r.quantidade = q < 50 ? 50 : q > cap ? cap : q;
	r.distancia = distancia_rota(m, origem, dest);
	r.lado_mapa = (uint32_t)m->lado;
	r.reintento = 0;
	r.reservado = TZ_NENHUM;
	a->rota = r;
	a->com_origem = origem; a->com_parceiro = dest;
	TzOrdem ord = *o;
	ord.tipo = TZ_O_COMERCIAR; ord.alvo = origem;
	tz_dar_ordem(m, i, &ord, fila);
	return 0;
}

static int reservado_por_otro(const TzMundo *m, TzId i, TzId j)
{
	for (int q = 0; q < m->nobj; ++q) {
		const TzObj *t = &m->obj[q];
		if (q == i || !t->ator) continue;
		if (t->ator->ordem.tipo == TZ_O_COMERCIAR && t->ator->ordem.alvo == j && t->ator->rota.reservado == j) return 1;
	}
	return 0;
}

void tz_comerciar_tic(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	TzId j = a->ordem.alvo;
	if (j < 0 || j >= m->nobj || !e_mercado(&m->obj[j])) { a->rota.reservado = TZ_NENHUM; tz_proxima_ordem(m, i); return; }
	if (tz_obj_dist(m, o, &m->obj[j]) > 1 || (a->passo < a->passo_total)) {
		if (tz_mover_obj(m, i, j, 1) < 0) { tz_proxima_ordem(m, i); }
		return;
	}
	if (o->estado != TZ_E_PARADO) tz_estado(m, o, TZ_E_PARADO, 0);
	TzId origem = a->com_origem, parceiro = a->com_parceiro;
	if (origem < 0 || parceiro < 0 || origem >= m->nobj || parceiro >= m->nobj || !tz_obj_vivo(&m->obj[origem]) || !tz_obj_vivo(&m->obj[parceiro]) || !a->rota.ativa) {
		a->rota.reservado = TZ_NENHUM; tz_proxima_ordem(m, i); return;
	}
	struct TzRota *r = &a->rota;
	int aqui = j != origem;
	int dono = m->obj[j].dono;
	if (r->reservado != j) {
		/* un mercader a la vez, y ninguno en una obra (0x4e7ec0, 0x45b7b0); queda reservado hasta que se vaya */
		if ((m->obj[j].flags & TZ_F_OBRA) || reservado_por_otro(m, i, j)) return;
		r->reservado = j;
		/* 0x4e7c98: una vez por parada se vende al dueño del edificio lo que viene del otro extremo, con la ganancia si es el
		 * recurso de aquel extremo */
		if (r->carga > 0) {
			uint32_t c = r->carga;
			if (r->carga_tipo == r->rec[1 - aqui]) c += lucro(r->carga, r->distancia, bonus_comercio(m, o->dono), r->lado_mapa);
			credito(m, dono, r->carga_tipo, c);
			tz_ev(m, TZ_EV_COMERCIO, dono, i, j, m->obj[j].cx, m->obj[j].cy, r->carga_tipo, (int32_t)c);
			r->carga = 0;
		}
	}
	if (m->tic < r->reintento) return;
	/* cada intento carga como mucho min(100, 1000 / mercados del dueño) y espera un segundo hasta completar */
	int n = 0;
	for (int q = 0; q < m->nobj; ++q) {
		const TzObj *t = &m->obj[q];
		if (t->dono == dono && e_mercado(t) && !(t->flags & TZ_F_OBRA)) n++;
	}
	uint32_t toma = n == 0 ? 100 : 1000u / (uint32_t)n;
	if (toma > 100) toma = 100;
	int rec = r->rec[aqui];
	uint32_t quer = r->quantidade > r->carga ? r->quantidade - r->carga : 0;
	if (quer > toma) quer = toma;
	if (quer > 0 && debito(m, dono, rec, quer) == 0) { r->carga += quer; r->carga_tipo = (uint8_t)rec; }
	if (r->carga < r->quantidade) { r->reintento = m->tic + TIC_SEG; return; }
	r->reintento = 0;
	r->reservado = TZ_NENHUM;
	a->ordem.alvo = j == origem ? parceiro : origem;
	a->ncaminho = a->icaminho = 0; a->falhas = 0; a->espera = 0;
}

/* Sell (op 0x34 sin ruta): va a un mercado, puerto o castillo y vende lo que lleva del comercio, sin ganancia */
void tz_vender_tic(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	TzId j = a->ordem.alvo;
	if (j < 0 || j >= m->nobj || !tz_obj_vivo(&m->obj[j]) || !(m->obj[j].classe == TZC_MARKET || m->obj[j].classe == TZC_HARBOR || m->obj[j].classe == TZC_CASTLE)) {
		tz_proxima_ordem(m, i);
		return;
	}
	if (tz_obj_dist(m, o, &m->obj[j]) > 1 || (a->passo < a->passo_total)) {
		if (tz_mover_obj(m, i, j, 1) < 0) tz_proxima_ordem(m, i);
		return;
	}
	if (a->rota.carga > 0) {
		credito(m, m->obj[j].dono, a->rota.carga_tipo, a->rota.carga);
		tz_ev(m, TZ_EV_COMERCIO, m->obj[j].dono, i, j, m->obj[j].cx, m->obj[j].cy, a->rota.carga_tipo, (int32_t)a->rota.carga);
		a->rota.carga = 0;
	}
	tz_proxima_ordem(m, i);
}

/* ------------------------------------------------------------------ taberna: ofertas */
/* hire_price (0x4d23b0): coste de la clase por cantidad por 1,6, más 30 de oro por nivel y 15 por nivel de los demás recursos
 * que cueste la clase */
void tz_mundo_preco_oferta(const TzMundo *m, const struct TzOferta *of, uint32_t preco[TZ_NREC])
{
	const TzAtributos *at = tz_catalogo_at(m->cat, of->classe, of->civ);
	for (int r = 0; r < TZ_NREC; ++r) {
		uint32_t base = (uint32_t)((uint64_t)at->custo[r] * of->quantidade * 160 / 100);
		preco[r] = 0;
		if (r == 0 || base) preco[r] = base + (r == 0 ? 30u : 15u) * of->nivel;
	}
}

int tz_mundo_ofertas(const TzMundo *m, TzId t, int lista, struct TzOferta *out, int max)
{
	if (t < 0 || t >= m->nobj || lista < 0 || lista > 1 || !m->obj[t].ator) return 0;
	const TzAtor *a = m->obj[t].ator;
	int n = a->noferta[lista] < max ? a->noferta[lista] : max;
	for (int q = 0; q < n; ++q) out[q] = a->oferta[lista][q];
	return n;
}

static void nova_oferta(TzMundo *m, TzId t, int lista)
{
	TzObj *o = &m->obj[t];
	TzAtor *a = o->ator;
	int civ = tz_rng_entre(&m->rng, 1, 3);
	const TzOrdemDef *menu[160];
	int n = tz_catalogo_menu(m->cat, TZC_BARRACK, civ, menu, 160), cand[160], nc = 0;
	for (int q = 0; q < n; ++q) {
		if (strncmp(menu[q]->acao, "create ", 7)) continue;
		int cl = tz_catalogo_classe(menu[q]->classe);
		if (cl > 0 && tz_classe_e_unidade(cl)) cand[nc++] = cl;
	}
	if (!nc) return;
	int classe = cand[tz_rng_entre(&m->rng, 0, nc - 1)];
	if (a->noferta[lista] >= OFERTAS_MAX) {
		memmove(&a->oferta[lista][0], &a->oferta[lista][1], (OFERTAS_MAX - 1) * sizeof a->oferta[lista][0]);
		a->noferta[lista]--;
	}
	struct TzOferta *of = &a->oferta[lista][a->noferta[lista]++];
	int cant = lista ? 1 : 8 + tz_rng_entre(&m->rng, 0, 4);
	int nivel = lista ? 10 + tz_rng_entre(&m->rng, 0, 20) : 0;
	nivel += 4 * tz_pesquisas(m, o->dono, "te_fame");
	of->id = (uint16_t)(++m->reino[o->dono].proxima_oferta);
	of->classe = (uint8_t)classe; of->civ = (uint8_t)civ; of->quantidade = (uint16_t)cant; of->nivel = (uint16_t)nivel;
}

static void contratar(TzMundo *m, const TzComando *c)
{
	if (c->nids < 1 || c->ids[0] < 0 || c->ids[0] >= m->nobj) return;
	TzObj *t = &m->obj[c->ids[0]];
	if (t->classe != TZC_TAVERN || t->dono != c->reino || !tz_obj_vivo(t) || (t->flags & TZ_F_OBRA) || !t->ator) return;
	TzAtor *a = t->ator;
	int lista = -1, idx = -1;
	for (int l = 0; l < 2; ++l) for (int q = 0; q < a->noferta[l]; ++q) if (a->oferta[l][q].id == c->arg) { lista = l; idx = q; }
	if (lista < 0) { tz_falha("la oferta ya no está"); return; }
	const TzReinoEstado *rk = &m->reino[t->dono];
	if (rk->populacao >= rk->populacao_max) { tz_ev(m, TZ_EV_SEM_POPULACAO, t->dono, c->ids[0], TZ_NENHUM, t->cx, t->cy, 0, 0); return; }
	struct TzOferta of = a->oferta[lista][idx];
	uint32_t preco[TZ_NREC];
	tz_mundo_preco_oferta(m, &of, preco);
	if (tz_pagar(m, t->dono, preco)) return;
	int r[4];
	tz_obj_ret(m, t, r);
	int esq = tz_rng_entre(&m->rng, 0, 3);
	int x = esq == 0 ? r[2] + 1 : esq == 1 ? r[0] - 1 : esq == 2 ? r[2] + 1 : r[0] - 1;
	int y = esq == 0 ? r[3] + 1 : esq == 1 ? r[3] + 1 : esq == 2 ? r[1] - 1 : r[1] - 1;
	x = x < 0 ? 0 : x >= m->lado ? m->lado - 1 : x;
	y = y < 0 ? 0 : y >= m->lado ? m->lado - 1 : y;
	int feitos = 0;
	for (int q = 0; q < of.quantidade; ++q) {
		TzObj tmp; memset(&tmp, 0, sizeof tmp); tmp.classe = of.classe; tmp.civ = of.civ;
		int px = x, py = y, rr[4] = { x, y, x, y };
		if (!tz_cel_livre(m, tz_obj_capa(m, &tmp), t->dono, px, py) && !tz_lugar_livre(m, tz_obj_capa(m, &tmp), t->dono, rr, &px, &py)) break;
		TzId u = tz_novo_obj(m, of.classe, of.civ, t->dono, px, py, 0);
		if (u < 0) break;
		t = &m->obj[c->ids[0]]; a = t->ator;
		TzObj *n = &m->obj[u];
		n->nivel = (uint8_t)(of.nivel > 255 ? 255 : of.nivel);
		if (lista == 1) n->flags |= TZ_F_HEROI;
		tz_recalcular(m, u);
		m->reino[t->dono].criadas++;
		tz_ev(m, TZ_EV_CRIADO, t->dono, u, c->ids[0], px, py, of.classe, 0);
		feitos++;
	}
	m->reino[t->dono].populacao = (uint16_t)(m->reino[t->dono].populacao + feitos);
	tz_ev(m, TZ_EV_CONTRATO, t->dono, c->ids[0], TZ_NENHUM, t->cx, t->cy, of.classe, feitos);
	a = m->obj[c->ids[0]].ator;
	/* la oferta se va (el índice pudo moverse si otra se añadió: se busca otra vez) */
	for (int q = 0; q < a->noferta[lista]; ++q)
		if (a->oferta[lista][q].id == of.id) {
			memmove(&a->oferta[lista][q], &a->oferta[lista][q + 1], (size_t)(a->noferta[lista] - q - 1) * sizeof a->oferta[lista][0]);
			a->noferta[lista]--;
			break;
		}
}

/* ------------------------------------------------------------------ cruzadas */
static int techo_cruzada(const TzReinoEstado *r)
{
	int t = 16 + 4 * r->cruzadas_completas;
	return t > 36 ? 36 : t;
}

static void lanzar_cruzada(TzMundo *m, const TzComando *c)
{
	if (c->nids < 1 || c->ids[0] < 0 || c->ids[0] >= m->nobj) return;
	TzObj *cat = &m->obj[c->ids[0]];
	int k = c->reino;
	TzReinoEstado *r = &m->reino[k];
	if (cat->classe != TZC_CATHEDRAL || cat->dono != k || !tz_obj_vivo(cat) || (cat->flags & TZ_F_OBRA)) return;
	if (r->cruz_ancla >= 0 || r->cruzados == 0xffff) { tz_falha("ya hay una cruzada en marcha"); return; }
	if (c->x < 0 || c->y < 0 || c->x >= m->lado || c->y >= m->lado) return;
	uint32_t custo_[TZ_NREC];
	tz_catalogo_custo_ini(m->cat, TZC_ANCORA, cat->civ, custo_);
	if (tz_pagar(m, k, custo_)) return;
	TzId an = tz_novo_obj(m, TZC_ANCORA, cat->civ, k, c->x, c->y, 0);
	if (an < 0) return;
	int ready = r->cruzados;
	if (ready >= techo_cruzada(r)) r->cruzadas_completas++;
	r->cruzados = 0xffff;
	r->cruz_ancla = an;
	r->cruz_faltan = (uint16_t)ready;
	r->cruz_proximo = m->tic + 10;
	tz_ev(m, TZ_EV_CRUZADA, k, c->ids[0], an, c->x, c->y, TZC_ANCORA, ready);
}

static int cruzados_vivos(const TzMundo *m, int k, TzId an)
{
	int n = 0;
	for (int q = 0; q < m->nobj; ++q) {
		const TzObj *o = &m->obj[q];
		if (o->classe == TZC_CRUSADER && o->dono == k && o->ator && o->ator->ancla_cruzada == an && tz_obj_vivo(o)) n++;
	}
	return n;
}

static void cruzada_tic(TzMundo *m, int k)
{
	TzReinoEstado *r = &m->reino[k];
	TzId an = r->cruz_ancla;
	int de_pie = an >= 0 && an < m->nobj && tz_obj_vivo(&m->obj[an]);
	if (de_pie && r->cruz_faltan > 0 && m->tic >= r->cruz_proximo) {
		r->cruz_proximo = m->tic + 10;
		const TzObj *a = &m->obj[an];
		TzObj tmp; memset(&tmp, 0, sizeof tmp); tmp.classe = TZC_CRUSADER; tmp.civ = a->civ;
		/* cada 500 ms llega uno a una casilla libre sin bosque a siete casillas del ancla; hasta casillas / 4 + 1 intentos */
		int ncel = 0, cx[225], cy[225];
		for (int y = a->cy - 7; y <= a->cy + 7; ++y) for (int x = a->cx - 7; x <= a->cx + 7; ++x) {
			if (x < 0 || y < 0 || x >= m->lado || y >= m->lado) continue;
			if (tz_cel_livre(m, TZ_CAPA_TERRA, k, x, y) && !(m->cel[y * m->lado + x].bits & TZ_CEL_ARVORE)) { cx[ncel] = x; cy[ncel] = y; ncel++; }
		}
		int tentativas = ncel / 4 + 1;
		if (tentativas > 255) tentativas = 255;
		for (int t = 0; t < tentativas && ncel; ++t) {
			int q = (int)(tz_rng(&m->rng) % (uint32_t)ncel);
			if (!tz_cel_livre(m, TZ_CAPA_TERRA, k, cx[q], cy[q])) continue;
			TzId u = tz_novo_obj(m, TZC_CRUSADER, a->civ, k, cx[q], cy[q], 0);
			if (u < 0) break;
			if (m->obj[u].ator) m->obj[u].ator->ancla_cruzada = an;
			m->reino[k].criadas++;
			tz_ev(m, TZ_EV_CRIADO, k, u, an, cx[q], cy[q], TZC_CRUSADER, 0);
			r->cruz_faltan--;
			break;
		}
	}
	/* la cruzada acaba con el ancla caída o con todos llegados y muertos; la catedral vuelve a empezar desde 4 */
	if (!de_pie || (r->cruz_faltan == 0 && cruzados_vivos(m, k, an) == 0)) {
		if (de_pie) { tz_marcar(m, an, 0); m->obj[an].flags |= TZ_F_MORTO; }
		r->cruz_ancla = TZ_NENHUM; r->cruz_faltan = 0;
		r->cruzados = 4;
	}
}

/* ------------------------------------------------------------------ cambios de clase */
int tz_converter(TzMundo *m, TzId i, int classe)
{
	TzObj *o = &m->obj[i];
	if (!tz_obj_vivo(o) || !tz_obj_unidade(o) || o->classe == classe) return -1;
	tz_marcar(m, i, 0);
	int32_t vm = o->vida_max ? o->vida_max : 1;
	o->classe = (uint16_t)classe;
	const TzAtributos *at = tz_obj_at(m, o);
	if (at->vida) o->vida_max = at->vida;
	o->vida = (int32_t)((int64_t)o->vida * o->vida_max / vm);
	if (o->vida < 1) o->vida = 1;
	o->alcance = at->alcance; o->visao = at->visao ? at->visao : o->visao;
	o->mana_max = at->mana_max; if (o->mana > o->mana_max) o->mana = o->mana_max;
	tz_marcar(m, i, 1);
	tz_parar(m, i);
	tz_recalcular(m, i);
	return 0;
}

static void convertir(TzMundo *m, const TzComando *c)
{
	if (c->arg == TZC_SPY) {
		/* revelarse: los anfitriones de un espía de este reino lo sueltan */
		for (int q = 0; q < c->nids; ++q) {
			TzId i = c->ids[q];
			if (i >= 0 && i < m->nobj && m->obj[i].ator && m->obj[i].ator->espia_reino == c->reino && tz_obj_vivo(&m->obj[i])) tz_espia_revelar(m, i);
		}
		return;
	}
	if (c->arg != TZC_PRIEST && c->arg != TZC_JIHAD) return;
	for (int q = 0; q < c->nids; ++q) {
		TzId i = c->ids[q];
		if (i < 0 || i >= m->nobj) continue;
		TzObj *o = &m->obj[i];
		if (o->dono != c->reino || !o->ator || !tz_obj_vivo(o) || (o->flags & TZ_F_BLOQUEADO)) continue;
		if (o->ator->hipnose_ate && m->tic < o->ator->hipnose_ate) { tz_falha("una unidad hipnotizada no se convierte"); continue; }
		if (c->arg == TZC_PRIEST) {
			if (o->classe != TZC_MONK) continue;
			uint32_t custo_[TZ_NREC], t;
			tz_mundo_custo_classe(m, o->dono, TZC_PRIEST, custo_, &t);
			if (tz_pagar(m, o->dono, custo_)) continue;
		} else if (o->classe != TZC_PEON) continue;
		tz_converter(m, i, c->arg);
		tz_ev(m, TZ_EV_CRIADO, o->dono, i, TZ_NENHUM, m->obj[i].cx, m->obj[i].cy, c->arg, 4);
	}
}

/* ------------------------------------------------------------------ comandos del negocio */
static TzObj *taberna(TzMundo *m, const TzComando *c)
{
	if (c->nids < 1 || c->ids[0] < 0 || c->ids[0] >= m->nobj) return NULL;
	TzObj *t = &m->obj[c->ids[0]];
	if (t->classe != TZC_TAVERN || t->dono != c->reino || !tz_obj_vivo(t) || (t->flags & TZ_F_OBRA) || !t->ator) return NULL;
	return t;
}

static int troca_en_marcha(const TzMundo *m, int k)
{
	for (int q = 0; q < m->nobj; ++q) {
		const TzObj *t = &m->obj[q];
		if (t->classe == TZC_TAVERN && t->dono == k && t->ator && t->ator->troca.ativa) return 1;
	}
	return 0;
}

int tz_negocio_comando(TzMundo *m, const TzComando *c)
{
	int k = c->reino;
	if (!reino_ok(k)) return 0;
	TzReinoEstado *r = &m->reino[k];
	switch (c->tipo) {
	case TZ_C_EMPRESTAR: {
		if (!taberna(m, c) || c->arg < 0 || c->arg >= TZ_NREC) return 1;
		if (r->emp_rec) { tz_falha("devuelve antes el préstamo anterior"); return 1; }
		r->emp_rec = (uint8_t)(c->arg + 1);
		r->recursos[c->arg] += (int32_t)r->emp_quantia;
		tz_ev(m, TZ_EV_EMPRESTIMO, k, c->ids[0], TZ_NENHUM, 0, 0, c->arg, (int32_t)r->emp_quantia);
		return 1;
	}
	case TZ_C_DEVOLVER: {
		if (!taberna(m, c)) return 1;
		if (!r->emp_rec) { tz_falha("no hay préstamo que devolver"); return 1; }
		int rec = r->emp_rec - 1;
		uint32_t cuanto = r->emp_quantia + 200, pago[TZ_NREC] = { 0 };
		pago[rec] = cuanto;
		if (tz_pagar(m, k, pago)) return 1;
		r->emp_rec = 0;
		r->emp_aumenta_tic = m->tic + 60 * TIC_SEG;
		r->emp_aumenta_quantia = cuanto;
		tz_ev(m, TZ_EV_EMPRESTIMO, k, c->ids[0], TZ_NENHUM, 0, 0, rec, -(int32_t)cuanto);
		return 1;
	}
	case TZ_C_TROCAR: {
		TzObj *t = taberna(m, c);
		if (!t) return 1;
		int de = c->arg & 3, para = (c->arg >> 2) & 3;
		int32_t pago = c->valor;
		if (pago < 500 || pago > 20000 || pago % 500) { tz_falha("elige una cantidad de 500 a 20000"); return 1; }
		if (troca_en_marcha(m, k)) { tz_falha("ya hay un intercambio en marcha"); return 1; }
		uint32_t custo_[TZ_NREC] = { 0 };
		custo_[de] = (uint32_t)pago;
		if (tz_pagar(m, k, custo_)) return 1;
		struct TzTroca *x = &t->ator->troca;
		x->ativa = 1; x->de = (uint8_t)de; x->para = (uint8_t)para; x->pago = (uint32_t)pago; x->total = (uint32_t)pago / 10;
		x->entregue = 0; x->proximo = m->tic + 6;
		return 1;
	}
	case TZ_C_CONTRATAR: if (taberna(m, c)) contratar(m, c); return 1;
	case TZ_C_CRUZADA: lanzar_cruzada(m, c); return 1;
	case TZ_C_CONVERTIR: convertir(m, c); return 1;
	case TZ_C_VENDER: {
		if (c->nids < 1 || c->ids[0] < 0 || c->ids[0] >= m->nobj) return 1;
		const TzObj *mk = &m->obj[c->ids[0]];
		if (mk->classe != TZC_MARKET || mk->dono != k || !tz_obj_vivo(mk) || (mk->flags & TZ_F_OBRA)) return 1;
		tz_vender_item(m, k, c->ids[0], c->valor);
		return 1;
	}
	}
	return 0;
}

/* ------------------------------------------------------------------ cada tic */
void tz_negocio_tic(TzMundo *m)
{
	for (int k = 1; k <= TZ_REINOS; ++k) {
		TzReinoEstado *r = &m->reino[k];
		if (!r->ativo) continue;
		if (r->emp_aumenta_tic && m->tic >= r->emp_aumenta_tic) { r->emp_quantia = r->emp_aumenta_quantia; r->emp_aumenta_tic = 0; }
		if (r->cruz_ancla >= 0) cruzada_tic(m, k);
	}
	int n = m->nobj;
	for (int i = 0; i < n; ++i) {
		TzObj *o = &m->obj[i];
		if (!o->ator || o->dono < 1 || o->dono > TZ_REINOS || !tz_obj_vivo(o) || (o->flags & TZ_F_OBRA)) continue;
		TzAtor *a = o->ator;
		if (o->classe == TZC_TAVERN) {
			if (a->troca.ativa && m->tic >= a->troca.proximo) {
				m->reino[o->dono].recursos[a->troca.para]++;
				a->troca.entregue++;
				a->troca.proximo = m->tic + 6;
				if (a->troca.entregue >= a->troca.total) {
					a->troca.ativa = 0;
					tz_ev(m, TZ_EV_TROCA, o->dono, i, TZ_NENHUM, o->cx, o->cy, a->troca.para, (int32_t)a->troca.total);
				}
			}
			if (!a->oferta_prox) a->oferta_prox = m->tic + TIC_SEG;
			if (m->tic >= a->oferta_prox) {
				if (tz_pesquisas(m, o->dono, "te_mercenaries")) nova_oferta(m, i, 0);
				if (tz_pesquisas(m, o->dono, "te_heroesquest")) nova_oferta(m, i, 1);
				o = &m->obj[i]; a = o->ator;
				a->oferta_prox = m->tic + 60 * TIC_SEG;
			}
		} else if (o->classe == TZC_CATHEDRAL) {
			/* una catedral suma un cruzado cada 12 s los primeros 15 minutos, 8 s hasta el 20, 4 s hasta el 30 y después cada segundo */
			if (m->tic >= a->cruzada_prox) {
				TzReinoEstado *r = &m->reino[o->dono];
				if (r->cruzados != 0xffff && r->cruzados < techo_cruzada(r)) r->cruzados++;
				uint32_t ms = m->tic * TZ_TIC_MS;
				a->cruzada_prox = m->tic + (ms < 900000u ? 12 : ms < 1200000u ? 8 : ms < 1800000u ? 4 : 1) * TIC_SEG;
			}
		}
	}
}
