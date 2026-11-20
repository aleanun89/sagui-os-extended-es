/* SPDX-License-Identifier: GPL-3.0-or-later */
/* mundo.c - el mundo de una partida de Tzar: creación, tic, comandos, eventos, consultas, niebla, población, victoria
 * y acciones de los guiones (D417, fase 5).
 *
 * Traducido a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later):
 *  - simulation.rs: NativeRng, ObjectState::read (atributos del registro), level_bonus, live_bonus, Fog;
 *  - gameplay.rs: relation/enemy/hostile_to, population/housing_sum, alive, Runtime;
 *  - gameplay/pathing.rs: kind() (tierra, agua, costa, acantilado) y árboles (tocones con TERRAIN.INI treename);
 *  - gameplay/scripting.rs: check_end/defeat_test/major (victoria y derrota).
 */
#include "mundo_int.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ NativeRng (simulation.rs) */
uint32_t tz_rng(TzRng *r)
{
	uint32_t x = r->s + 1;
	for (int i = 0; i < 32; ++i) {
		x <<= 1;
		if (((x >> 31) ^ (x >> 30)) & 1) x += 1;
	}
	r->s = x;
	return x;
}

int32_t tz_rng_entre(TzRng *r, int32_t lo, int32_t hi)
{
	if (hi <= lo) return lo;
	uint32_t span = (uint32_t)((int64_t)hi - lo + 1);
	return (int32_t)(lo + (int64_t)(tz_rng(r) % span));
}

/* ------------------------------------------------------------------ objetos */
const TzAtributos *tz_obj_at(const TzMundo *m, const TzObj *o) { return tz_catalogo_at(m->cat, o->classe, o->civ); }

int tz_obj_vivo(const TzObj *o)
{
	return o && !(o->flags & (TZ_F_MORTO | TZ_F_MORRENDO | TZ_F_INATIVO | TZ_F_DENTRO | TZ_F_CARREGADO));
}
int tz_obj_unidade(const TzObj *o) { return o && tz_classe_e_unidade(o->classe); }
int tz_obj_edificio(const TzObj *o) { return o && tz_classe_e_edificio(o->classe); }

int tz_obj_capa(const TzMundo *m, const TzObj *o)
{
	const TzAtributos *a = tz_obj_at(m, o);
	return a->voa ? TZ_CAPA_AR : a->naval ? TZ_CAPA_AGUA : TZ_CAPA_TERRA;
}

static void pegada(const TzMundo *m, const TzObj *o, int *w, int *h)
{
	const TzAtributos *a = tz_obj_at(m, o);
	*w = a->pegada[0]; *h = a->pegada[1];
	if (o->classe == TZC_GATE_WOOD || o->classe == TZC_GATE_STONE) {
		/* la puerta de lado (byte 56 & 3 impar) está girada */
		int dir = o->original && o->original->ndados > 56 ? o->original->dados[56] & 3 : 0;
		if (dir & 1) { int t = *w; *w = *h; *h = t; }
	}
	if (!tz_obj_edificio(o) && o->classe != TZC_RES_GOLD && o->classe != TZC_RES_STONE) *w = *h = 1;
}

void tz_obj_ret(const TzMundo *m, const TzObj *o, int r[4])
{
	int w, h;
	pegada(m, o, &w, &h);
	r[0] = o->cx; r[1] = o->cy; r[2] = o->cx + w - 1; r[3] = o->cy + h - 1;
}

int tz_obj_dist(const TzMundo *m, const TzObj *a, const TzObj *b)
{
	int ra[4], rb[4];
	tz_obj_ret(m, a, ra); tz_obj_ret(m, b, rb);
	int dx = ra[2] < rb[0] ? rb[0] - ra[2] : rb[2] < ra[0] ? ra[0] - rb[2] : 0;
	int dy = ra[3] < rb[1] ? rb[1] - ra[3] : rb[3] < ra[1] ? ra[1] - rb[3] : 0;
	return dx > dy ? dx : dy;
}

void tz_obj_pixel(const TzObj *o, int *x, int *y)
{
	*x = o->px * TZ_CASILLA / TZ_SUB;
	*y = o->py * TZ_CASILLA / TZ_SUB;
}

/* ------------------------------------------------------------------ eventos */
void tz_ev(TzMundo *m, int tipo, int reino, TzId obj, TzId alvo, int x, int y, int classe, int32_t valor)
{
	TzEvento *e = &m->ev[m->ev_seq & (TZ_EVENTOS - 1)];
	e->seq = ++m->ev_seq;
	e->tic = m->tic;
	e->tipo = (uint8_t)tipo; e->reino = (uint8_t)reino;
	e->obj = obj; e->alvo = alvo;
	e->x = (int16_t)x; e->y = (int16_t)y;
	e->classe = (uint16_t)classe;
	e->valor = valor;
}

void tz_mundo_evento(TzMundo *m, const TzEvento *e)
{
	tz_ev(m, e->tipo, e->reino, e->obj, e->alvo, e->x, e->y, e->classe, e->valor);
}

int tz_mundo_eventos(const TzMundo *m, uint32_t *cursor, TzEvento *out, int max)
{
	uint32_t c = *cursor;
	if (m->ev_seq - c > TZ_EVENTOS) c = m->ev_seq - TZ_EVENTOS;
	int n = 0;
	while (c < m->ev_seq && n < max) {
		out[n++] = m->ev[c & (TZ_EVENTOS - 1)];
		++c;
	}
	*cursor = c;
	return n;
}

static int32_t texto(TzMundo *m, const char *t)
{
	uint32_t i = m->ntextos++;
	snprintf(m->textos[i & 63], sizeof m->textos[0], "%s", t ? t : "");
	return (int32_t)i;
}

const char *tz_mundo_texto(const TzMundo *m, int32_t idx)
{
	if (idx < 0 || (uint32_t)idx >= m->ntextos || m->ntextos - (uint32_t)idx > 64) return "";
	return m->textos[idx & 63];
}

/* ------------------------------------------------------------------ diplomacia (gameplay.rs relation) */
static uint32_t relacao_bits(const TzMundo *m, int a, int b)
{
	if (a == b || a == 0 || b == 0) return 0x47;
	if (b == 9) return 0x40;
	if (a > 8 || b > 8) return 0x10;
	uint32_t d = m->reino[a].diplomacia[b];
	if (!(d & 0x70)) d |= 0x10;
	return d;
}

int tz_mundo_relacao(const TzMundo *m, int a, int b)
{
	uint32_t r = relacao_bits(m, a, b);
	if (r & 0x40) return 1;
	if (r & 0x10) return -1;
	return 0;
}

/* gameplay.rs hostile_to: ¿el reino k ataca solo a t? */
static int hostil_auto(const TzMundo *m, int k, const TzObj *t)
{
	uint32_t r = relacao_bits(m, k, t->dono);
	if (t->classe >= 39 && t->classe <= 42) return !(r & 0x40);
	if (t->classe == 112 || t->classe == 119) return 0;   /* pájaros */
	if (t->classe == TZC_DUMMY || t->classe == TZC_COW) return 0;
	if (tz_obj_edificio(t)) return (r & 0x10) != 0;
	if (tz_obj_unidade(t) && t->dono != 0) return !(r & 0x40) && (r & 0x10);
	return 0;
}
int tz_hostil(const TzMundo *m, int k, TzId t) { return hostil_auto(m, k, &m->obj[t]); }

/* ------------------------------------------------------------------ investigaciones */
int tz_pesquisas(const TzMundo *m, int k, const char *nome)
{
	if (k < 1 || k > 8) return 0;
	int t = tz_catalogo_tec(m->cat, nome);
	return t >= 0 && m->reino[k].pesquisas ? m->reino[k].pesquisas[t] : 0;
}

/* simulation.rs level_bonus + live_bonus + researched: ataque y defensa con el nivel y las investigaciones */
void tz_recalcular(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	const TzAtributos *a = tz_obj_at(m, o);
	uint32_t at = a->ataque, df = a->defesa;
	const TzObjeto *r = o->original;
	if (r && r->ndados > 0x19) {
		unsigned va = tz_u16(r->dados + 0x16), vd = tz_u16(r->dados + 0x18);
		if (va && !(r->flags & 0x100)) at = va;
		if (vd && !(r->flags & 0x200)) df = vd;
	}
	int unid = tz_obj_unidade(o), bow = a->categoria_arma == 1, torre = o->classe >= 39 && o->classe <= 43;
	unsigned lv = o->nivel;
	if (unid || torre) at += unid && bow && o->dono ? (lv + 1) / 2 : lv;
	if (unid) df += (lv + (bow ? 3 : 1)) / (bow ? 4 : 2);
	int k = o->dono;
	if (k >= 1 && k <= 8) {
#define N(x) ((uint32_t)tz_pesquisas(m, k, x))
		if (unid) {
			switch (a->categoria_arma) {
			case 0: at += N("te_temperiron") + N("te_scimitar"); break;
			case 1: at += N("te_crossbow") + N("te_chemistry"); break;
			case 2: case 5: at += 5 * N("te_engineering"); break;
			}
			int c = o->classe;
			if (c == 1 || (c >= 3 && c <= 7) || (c >= 12 && c <= 14) || c == 17 || c == 18 || (c >= 27 && c <= 31) || c == 33 || c == 36)
				df += N("te_scalearmor") + 2 * N("te_chainmail");
			if (c == 10 || c == 35) { at += 2 * N("te_ridinghorses"); df += N("te_ridinghorses"); }
			if (c == TZC_PEON) at += N("te_kungfu");
		}
		if (o->classe == 18) at += 5 * N("te_inquisition");
		if (o->classe == 42) at += 10 * N("te_gunpowder");
#undef N
	}
	/* hechizos: bendición +3/+3, furia +50 % de ataque (magia.c) */
	if (o->ator && o->ator->efeito && m->tic < o->ator->efeito_ate) {
		if (o->ator->efeito == TZ_EF_BENDICAO) { at += 3; df += 3; }
		if (o->ator->efeito == TZ_EF_FURIA) at += at / 2;
	}
	o->ataque = (uint16_t)(at > 65535 ? 65535 : at);
	o->defesa = (uint16_t)(df > 65535 ? 65535 : df);
}

/* ------------------------------------------------------------------ casillas */
static int tipo_terreno(uint16_t t)   /* pathing.rs kind(): 0 acantilado, 1 tierra, 2 agua, 3 costa */
{
	int first = (t >> 4) & 7, second = (t >> 7) & 7;
	if (t & 0x400) return 0;
	if (second >= 1) return 1;
	if (first >= 1) return 3;
	return 2;
}

static void cel_terreno(TzMundo *m, int i)
{
	TzCelula *c = &m->cel[i];
	uint16_t t = m->casillas[i];
	c->bits &= (uint16_t)~(TZ_CEL_AGUA | TZ_CEL_COSTA | TZ_CEL_PENHASCO | TZ_CEL_ARVORE);
	switch (tipo_terreno(t)) {
	case 0: c->bits |= TZ_CEL_PENHASCO; break;
	case 2: c->bits |= TZ_CEL_AGUA; break;
	case 3: c->bits |= TZ_CEL_COSTA; break;
	}
	int troncos = m->regras.troncos ? m->regras.troncos : 3;
	if (((t >> 11) & 31) > troncos && c->madeira) c->bits |= TZ_CEL_ARVORE;
}

/* ¿una unidad de la capa puede quedarse en la casilla ahora? */
int tz_cel_livre(const TzMundo *m, int capa, int dono, int x, int y)
{
	if (!tz_cel_transitavel(m, capa, dono, x, y)) return 0;
	const TzCelula *c = &m->cel[y * m->lado + x];
	if (capa == TZ_CAPA_AR) return !(c->bits & TZ_CEL_OCUPADA_AR);
	return c->ocupante < 0 && !(c->bits & TZ_CEL_PORTA);
}

/* una casilla libre alrededor del rectángulo, de dentro afuera, empezando por abajo (como salen del edificio) */
int tz_lugar_livre(const TzMundo *m, int capa, int dono, const int r[4], int *x, int *y)
{
	for (int d = 1; d < 24; ++d) {
		int x0 = r[0] - d, y0 = r[1] - d, x1 = r[2] + d, y1 = r[3] + d;
		/* abajo, derecha, arriba, izquierda */
		for (int xx = x0; xx <= x1; ++xx) if (tz_cel_livre(m, capa, dono, xx, y1)) { *x = xx; *y = y1; return 1; }
		for (int yy = y1; yy >= y0; --yy) if (tz_cel_livre(m, capa, dono, x1, yy)) { *x = x1; *y = yy; return 1; }
		for (int xx = x1; xx >= x0; --xx) if (tz_cel_livre(m, capa, dono, xx, y0)) { *x = xx; *y = y0; return 1; }
		for (int yy = y0; yy <= y1; ++yy) if (tz_cel_livre(m, capa, dono, x0, yy)) { *x = x0; *y = yy; return 1; }
	}
	return 0;
}

static int fixo(const TzMundo *m, const TzObj *o)
{
	if (o->flags & (TZ_F_MORTO | TZ_F_INATIVO)) return 0;
	return tz_obj_edificio(o) || o->classe == TZC_RES_GOLD || o->classe == TZC_RES_STONE;
}

void tz_marcar(TzMundo *m, TzId i, int pon)
{
	TzObj *o = &m->obj[i];
	int n = m->lado;
	if (fixo(m, o)) {
		int r[4];
		tz_obj_ret(m, o, r);
		uint16_t b = o->classe == TZC_FIELD ? TZ_CEL_CAMPO : o->classe == TZC_BRIDGE ? TZ_CEL_PONTE :
		             (o->classe == TZC_GATE_WOOD || o->classe == TZC_GATE_STONE) ? TZ_CEL_PORTA :
		             (o->classe == TZC_WALL_STONE || o->classe == TZC_WALL_WOOD) ? (TZ_CEL_MURO | TZ_CEL_EDIFICIO) : TZ_CEL_EDIFICIO;
		for (int y = r[1]; y <= r[3]; ++y) for (int x = r[0]; x <= r[2]; ++x) {
			if (x < 0 || y < 0 || x >= n || y >= n) continue;
			TzCelula *c = &m->cel[y * n + x];
			if (pon) { c->bits |= b; c->fixo = i; }
			else if (c->fixo == i) { c->bits &= (uint16_t)~b; c->fixo = TZ_NENHUM; }
		}
		((TzPriv *)m->priv)->ilhas_sujas = 1;
		return;
	}
	if (!tz_obj_unidade(o) || o->cx < 0 || o->cy < 0 || o->cx >= n || o->cy >= n) return;
	TzCelula *c = &m->cel[o->cy * n + o->cx];
	if (tz_obj_capa(m, o) == TZ_CAPA_AR) {
		if (pon) c->bits |= TZ_CEL_OCUPADA_AR; else c->bits &= (uint16_t)~TZ_CEL_OCUPADA_AR;
		return;
	}
	if (pon) { c->ocupante = i; c->bits |= TZ_CEL_OCUPADA; }
	else if (c->ocupante == i) { c->ocupante = TZ_NENHUM; c->bits &= (uint16_t)~TZ_CEL_OCUPADA; }
}

/* ------------------------------------------------------------------ creación de objetos */
static TzId reservar(TzMundo *m)
{
	if (m->nobj == m->capobj) {
		int cap = m->capobj ? m->capobj * 2 : 1024;
		TzObj *no = realloc(m->obj, (size_t)cap * sizeof *no);
		if (!no) return TZ_NENHUM;
		m->obj = no; m->capobj = cap;
	}
	TzId i = m->nobj++;
	memset(&m->obj[i], 0, sizeof m->obj[i]);
	m->obj[i].dentro_de = TZ_NENHUM;
	return i;
}

static int precisa_ator(const TzObj *o)
{
	return tz_obj_unidade(o) || tz_obj_edificio(o);
}

static TzAtor *novo_ator(void)
{
	TzAtor *a = calloc(1, sizeof *a);
	if (!a) return NULL;
	a->alvo_auto = a->origem = TZ_NENHUM;
	a->ordem.alvo = a->retomar.alvo = TZ_NENHUM;
	a->reuniao[0] = a->reuniao[1] = -1;
	a->destino[0] = a->destino[1] = -1;
	return a;
}

static void centrar(TzObj *o)
{
	o->px = o->cx * TZ_SUB + TZ_SUB / 2;
	o->py = o->cy * TZ_SUB + TZ_SUB / 2;
}

static int cor_reino(const TzMundo *m, int dono)
{
	if (dono < 1 || dono > 8) return 0;
	return m->reino[dono].cor;
}

TzId tz_novo_obj(TzMundo *m, int classe, int civ, int dono, int cx, int cy, int obra)
{
	TzId i = reservar(m);
	if (i < 0) return i;
	TzObj *o = &m->obj[i];
	TzPriv *p = m->priv;
	o->id_mapa = p->prox_id++;
	if (!p->prox_id || p->prox_id >= 0xfc17) p->prox_id = 1;
	o->classe = (uint16_t)classe;
	o->civ = (uint8_t)(civ >= 1 && civ <= 3 ? civ : 1);
	o->dono = (uint8_t)dono;
	o->cor = (uint8_t)cor_reino(m, dono);
	o->cx = (int16_t)cx; o->cy = (int16_t)cy;
	centrar(o);
	const TzAtributos *a = tz_obj_at(m, o);
	o->vida_max = a->vida ? a->vida : 1;
	o->vida = obra ? 1 : o->vida_max;
	o->visao = a->visao; o->alcance = a->alcance; o->resistencia = a->resistencia;
	o->mana = o->mana_max = a->mana_max;
	o->anim_tic = m->tic;
	if (obra) o->flags |= TZ_F_OBRA;
	if (classe == TZC_RES_GOLD) o->recurso = m->cat->mina_ouro;
	if (classe == TZC_RES_STONE) o->recurso = m->cat->mina_pedra;
	if (classe == TZC_FISHPATCH) o->recurso = m->cat->pesqueiro;
	if (classe == TZC_COW) o->mana = o->mana_max = 100;
	if (precisa_ator(o)) {
		o->ator = novo_ator();
		if (o->ator && (classe == TZC_GATE_WOOD || classe == TZC_GATE_STONE)) o->ator->ciclos = 6;
		if (obra && o->ator) {
			uint32_t t = a->tempo_ms ? a->tempo_ms : 1000;
			if (tz_pesquisas(m, dono, "te_architecture")) for (int k = tz_pesquisas(m, dono, "te_architecture"); k; --k) t = t * 70 / 100;
			o->ator->obra_ms = o->ator->obra_total_ms = t;
		}
	}
	tz_recalcular(m, i);
	tz_marcar(m, i, 1);
	return i;
}

/* simulation.rs ObjectState::read: un objeto del mapa con lo que dice su registro */
static TzId obj_do_mapa(TzMundo *m, const TzObjeto *r)
{
	TzId i = reservar(m);
	if (i < 0) return i;
	TzObj *o = &m->obj[i];
	o->original = r;
	o->id_mapa = r->id;
	o->classe = r->classe;
	o->civ = (uint8_t)(r->civ >= 1 && r->civ <= 3 ? r->civ : 1);
	o->dono = r->dono;
	o->cor = (uint8_t)cor_reino(m, r->dono);
	o->flags = r->flags & (TZ_F_MORTO | TZ_F_HISTORIA | TZ_F_INATIVO | TZ_F_HEROI | TZ_F_DEUS | TZ_F_BLOQUEADO | TZ_F_OBRA | TZ_F_CARREGADO);
	o->cx = (int16_t)r->x; o->cy = (int16_t)r->y;
	/* como la escena: lo de un reino que no juega y lo marcado con el bit 4 no se crea */
	if ((r->flags & 4) || (r->dono >= 1 && r->dono <= 8 && !m->reino[r->dono].ativo)) o->flags |= TZ_F_INATIVO;
	centrar(o);
	const TzAtributos *a = tz_obj_at(m, o);
	o->vida_max = r->vida_max ? r->vida_max : a->vida;
	o->vida = r->vida;
	if (!o->vida && !(o->flags & TZ_F_OBRA)) o->vida = o->vida_max;
	if (o->vida > o->vida_max) o->vida_max = o->vida;
	o->nivel = r->ndados > 0x1d ? (uint8_t)(tz_u16(r->dados + 0x1c) > 255 ? 255 : tz_u16(r->dados + 0x1c)) : 0;
	unsigned v = r->ndados > 0x1a ? r->dados[0x1a] : 0;
	o->visao = v == 0 ? 5 : v == 255 ? a->visao : (uint16_t)v;
	o->alcance = a->alcance;
	o->resistencia = a->resistencia;
	o->mana_max = a->mana_max; o->mana = o->mana_max;
	o->direcao = (uint8_t)((r->variante & 0x8000) ? (r->variante & 7) : 0);
	o->anim_tic = 0;
	if (o->classe == TZC_RES_GOLD || o->classe == TZC_RES_STONE) {
		/* gameplay.rs: +0x3f marca la mina inagotable, +0x3a la cantidad */
		if (r->ndados > 0x3f && r->dados[0x3f]) o->recurso = 0xffffffffu;
		else o->recurso = r->ndados >= 0x3e ? tz_u32(r->dados + 0x3a) : 0;
		if (!o->recurso) o->recurso = o->classe == TZC_RES_GOLD ? m->cat->mina_ouro : m->cat->mina_pedra;
	} else if (o->classe == TZC_FISHPATCH) {
		o->recurso = r->ndados >= 0x3c ? tz_u32(r->dados + 0x38) : 0;
		if (!o->recurso) o->recurso = m->cat->pesqueiro;
	} else if (o->classe == TZC_COW) {
		o->mana = o->mana_max = 100;
	} else if (o->classe == 80) {   /* resitem: montón de recursos */
		o->recurso = 100;
	}
	if (precisa_ator(o) && !(o->flags & TZ_F_MORTO)) {
		o->ator = novo_ator();
		if (o->ator && (o->classe == TZC_GATE_WOOD || o->classe == TZC_GATE_STONE)) o->ator->ciclos = r->ndados > 57 && r->dados[57] ? 0 : 6;
		if (o->ator && (o->flags & TZ_F_OBRA)) {
			uint32_t t = a->tempo_ms ? a->tempo_ms : 1000;
			o->ator->obra_total_ms = t;
			uint64_t feito = o->vida_max ? (uint64_t)t * o->vida / o->vida_max : 0;
			o->ator->obra_ms = (uint32_t)(t - (feito > t ? t : feito));
			if (!o->ator->obra_ms) o->ator->obra_ms = 1;
		}
	}
	tz_recalcular(m, i);
	return i;
}

/* ------------------------------------------------------------------ crear y liberar */
static int priv_criar(TzMundo *m)
{
	TzPriv *p = calloc(1, sizeof *p);
	if (!p) return -1;
	size_t n = (size_t)m->lado * m->lado;
	p->g = malloc(n * sizeof *p->g);
	p->pai = malloc(n * sizeof *p->pai);
	p->visto = calloc(n, sizeof *p->visto);
	p->fechado = malloc(n);
	p->capheap = (int)(n < 65536 ? 65536 : n);
	p->heap = malloc((size_t)p->capheap * 12);
	p->limite_nos = 40000;
	p->prox_id = 1;
	m->priv = p;
	if (!p->g || !p->pai || !p->visto || !p->fechado || !p->heap) return -1;
	return 0;
}

int tz_mundo_criar(TzMundo *m, const TzCatalogo *c, const TzMapa *mapa, const TzRegras *regras, uint32_t semente)
{
	memset(m, 0, sizeof *m);
	m->cat = c;
	m->mapa = mapa;
	m->lado = mapa->lado;
	if (regras) m->regras = *regras;
	if (!m->regras.populacao) m->regras.populacao = 200;
	m->semente = semente;
	m->rng.s = semente;
	int n = m->lado;
	m->casillas = malloc((size_t)n * n * sizeof *m->casillas);
	m->cel = calloc((size_t)n * n, sizeof *m->cel);
	m->cmd = calloc(TZ_COMANDOS, sizeof *m->cmd);
	if (!m->casillas || !m->cel || !m->cmd || priv_criar(m)) { tz_mundo_liberar(m); return tz_falha("sin memoria para el mundo"); }
	memcpy(m->casillas, mapa->casillas, (size_t)n * n * sizeof *m->casillas);
	for (int i = 0; i < n * n; ++i) {
		m->cel[i].ocupante = m->cel[i].fixo = TZ_NENHUM;
		m->cel[i].madeira = 100;   /* gameplay.rs: un árbol da diez cargas (contadas de diez en diez) */
		cel_terreno(m, i);
		if (!(m->cel[i].bits & TZ_CEL_ARVORE)) m->cel[i].madeira = 0;
	}
	/* reinos */
	m->jogador = 1;
	for (int k = 1; k <= TZ_REINOS; ++k) {
		TzReinoEstado *r = &m->reino[k];
		const TzReino *mr = k <= mapa->nreinos ? &mapa->reinos[k - 1] : NULL;
		r->ativo = mr ? (uint8_t)mr->ativo : 0;
		r->ia = mr ? (uint8_t)mr->ia : 0;
		r->civ = (uint8_t)(mr && mr->civ >= 1 && mr->civ <= 3 ? mr->civ : 1);
		r->cor = (uint8_t)tz_mapa_cor(mapa, k);
		r->equipe = mr ? (uint8_t)mr->equipe : 0;
		snprintf(r->nome, sizeof r->nome, "%s", mr ? mr->nome : "");
		int nivel = m->regras.nivel_recursos;
		for (int j = 0; j < TZ_NREC; ++j)
			r->recursos[j] = nivel >= 1 && nivel <= 3 ? (int32_t)c->recursos_ini_nivel[nivel][j] : (mr ? (int32_t)mr->recursos[j] : 0);
		r->populacao_limite = m->regras.populacao;
		r->pesquisas = calloc((size_t)(c->ntec ? c->ntec : 1), 1);
		r->proibido = calloc((size_t)(c->ntec + TZ_NCLASSES), 1);
		r->nevoa = calloc((size_t)n * n, 1);
		if (!r->pesquisas || !r->proibido || !r->nevoa) { tz_mundo_liberar(m); return tz_falha("sin memoria para los reinos"); }
		if (m->regras.explorado || m->regras.sem_nevoa) memset(r->nevoa, 1, (size_t)n * n);
		for (int b = 1; b <= TZ_REINOS; ++b) r->diplomacia[b] = mapa->tem_diplomacia ? mapa->diplomacia[(k - 1) * 8 + b - 1] : 0x10;
	}
	if (!mapa->tem_diplomacia)
		for (int a = 1; a <= 8; ++a) for (int b = 1; b <= 8; ++b)
			if (a != b && m->reino[a].equipe && m->reino[a].equipe == m->reino[b].equipe) m->reino[a].diplomacia[b] = 0x41;
	/* el jugador local: el primer reino activo que no juega el ordenador (world.rs player) */
	for (int k = 1; k <= 8; ++k) if (m->reino[k].ativo && !m->reino[k].ia) { m->jogador = k; break; }
	/* objetos del mapa, con los mismos índices que mapa->obj */
	TzPriv *p = m->priv;
	for (int i = 0; i < mapa->nobj; ++i) {
		if (obj_do_mapa(m, &mapa->obj[i]) < 0) { tz_mundo_liberar(m); return tz_falha("sin memoria para los objetos"); }
		if (mapa->obj[i].id >= p->prox_id) p->prox_id = (uint16_t)(mapa->obj[i].id + 1);
	}
	for (int i = 0; i < m->nobj; ++i) {
		TzObj *o = &m->obj[i];
		if (o->flags & (TZ_F_MORTO | TZ_F_INATIVO | TZ_F_CARREGADO)) continue;
		tz_marcar(m, i, 1);
	}
	tz_ilhas(m);
	p->ilhas_sujas = 0;
	return 0;
}

void tz_mundo_liberar(TzMundo *m)
{
	for (int i = 0; i < m->nobj; ++i) if (m->obj[i].ator) { free(m->obj[i].ator->caminho); free(m->obj[i].ator); }
	free(m->obj);
	free(m->cel);
	free(m->casillas);
	free(m->proj);
	free(m->cmd);
	for (int k = 0; k <= TZ_REINOS; ++k) { free(m->reino[k].pesquisas); free(m->reino[k].proibido); free(m->reino[k].nevoa); }
	TzPriv *p = m->priv;
	if (p) { free(p->g); free(p->pai); free(p->visto); free(p->fechado); free(p->heap); free(p); }
	memset(m, 0, sizeof *m);
}

/* ------------------------------------------------------------------ muerte y cambios de dueño */
void tz_matar(TzMundo *m, TzId i, TzId por)
{
	TzObj *o = &m->obj[i];
	if (o->flags & (TZ_F_MORTO | TZ_F_MORRENDO)) return;
	o->vida = 0;
	o->flags |= TZ_F_MORRENDO;
	o->estado = TZ_E_MORRENDO;
	o->anim_tic = m->tic;
	tz_ev(m, TZ_EV_MORREU, o->dono, i, por, o->cx, o->cy, o->classe, 0);
	if (o->dono >= 1 && o->dono <= 8) m->reino[o->dono].perdidas++;
	if (por >= 0 && m->obj[por].dono >= 1 && m->obj[por].dono <= 8) {
		m->reino[m->obj[por].dono].matadas++;
		m->reino[m->obj[por].dono].pontos += (uint32_t)tz_obj_at(m, o)->vida / 2 + 10;
		TzObj *a = &m->obj[por];
		if (tz_obj_unidade(a) && tz_mundo_relacao(m, a->dono, o->dono) < 1 && ++a->xp >= 4u + a->nivel * 2u && a->nivel < 255) {
			a->xp = 0; a->nivel++;
			tz_recalcular(m, por);
			tz_ev(m, TZ_EV_NIVEL, a->dono, por, TZ_NENHUM, a->cx, a->cy, a->classe, a->nivel);
		}
	}
	tz_marcar(m, i, 0);
	/* los que van dentro mueren con él (barco) o salen (edificio) */
	for (int j = 0; j < m->nobj; ++j) {
		TzObj *q = &m->obj[j];
		if (q->dentro_de != i) continue;
		q->dentro_de = TZ_NENHUM;
		q->flags &= ~(uint32_t)TZ_F_DENTRO;
		int r[4], x, y;
		tz_obj_ret(m, o, r);
		if (!tz_obj_edificio(o) || !tz_lugar_livre(m, tz_obj_capa(m, q), q->dono, r, &x, &y)) { q->flags |= TZ_F_MORTO; q->vida = 0; continue; }
		q->cx = (int16_t)x; q->cy = (int16_t)y; centrar(q);
		tz_marcar(m, j, 1);
	}
	o->ndentro = 0;
}

void tz_trocar_dono(TzMundo *m, TzId i, int dono)
{
	TzObj *o = &m->obj[i];
	int antes = o->dono;
	if (antes == dono) return;
	o->dono = (uint8_t)dono;
	o->cor = (uint8_t)cor_reino(m, dono);
	if (o->ator) {
		o->ator->ordem.tipo = TZ_O_NADA; o->ator->nfila = 0; o->ator->ncaminho = 0;
		o->ator->retomar.tipo = TZ_O_NADA;
		/* la producción se pierde con el dueño */
		o->ator->ntrab = 0;
	}
	tz_recalcular(m, i);
	tz_ev(m, TZ_EV_CAPTURADO, dono, i, TZ_NENHUM, o->cx, o->cy, o->classe, antes);
}

/* ------------------------------------------------------------------ población (gameplay.rs population) */
static void contar_populacao(TzMundo *m)
{
	TzPriv *p = m->priv;
	memset(p->pop, 0, sizeof p->pop);
	memset(p->casas, 0, sizeof p->casas);
	for (int i = 0; i < m->nobj; ++i) {
		const TzObj *o = &m->obj[i];
		if (o->dono < 1 || o->dono > 8 || (o->flags & (TZ_F_MORTO | TZ_F_MORRENDO | TZ_F_INATIVO))) continue;
		if (tz_obj_unidade(o) && o->classe != TZC_COW) p->pop[o->dono]++;
		else if (tz_obj_edificio(o) && !(o->flags & TZ_F_OBRA)) p->casas[o->dono] = (uint16_t)(p->casas[o->dono] + tz_obj_at(m, o)->pop_conta);
	}
	for (int k = 1; k <= 8; ++k) {
		TzReinoEstado *r = &m->reino[k];
		r->populacao = p->pop[k];
		r->populacao_max = (uint16_t)(1 + p->casas[k]);
		uint32_t lim = r->populacao_limite ? r->populacao_limite : m->regras.populacao;
		lim += lim * 25 / 100 * (uint32_t)(tz_pesquisas(m, k, "te_supplystorage") + tz_pesquisas(m, k, "te_seafood"));
		if (r->populacao_max > lim) r->populacao_max = (uint16_t)lim;
	}
}

/* ------------------------------------------------------------------ niebla (simulation.rs Fog, simplificada) */
static int ve_por(const TzMundo *m, const TzObj *o, int k)
{
	if (o->dono == k) return 1;
	if (o->dono < 1 || o->dono > 8) return 0;
	return (relacao_bits(m, o->dono, k) & 0x41) == 0x41 && (relacao_bits(m, k, o->dono) & 0x40);
}

static void atualizar_nevoa(TzMundo *m)
{
	int n = m->lado;
	if (m->regras.sem_nevoa) return;
	for (int k = 1; k <= 8; ++k) {
		TzReinoEstado *r = &m->reino[k];
		if (!r->ativo) continue;
		for (int i = 0; i < n * n; ++i) if (r->nevoa[i] == 2) r->nevoa[i] = 1;
	}
	for (int q = 0; q < TZ_VISOES; ++q) {
		struct TzVisao *v = &m->visao[q];
		if (!v->reino || v->ate <= m->tic || !m->reino[v->reino].ativo) continue;
		for (int y = v->y - v->raio; y <= v->y + v->raio; ++y) for (int x = v->x - v->raio; x <= v->x + v->raio; ++x)
			if (x >= 0 && y >= 0 && x < n && y < n && (x - v->x) * (x - v->x) + (y - v->y) * (y - v->y) <= v->raio * v->raio + v->raio)
				m->reino[v->reino].nevoa[y * n + x] = 2;
	}
	for (int i = 0; i < m->nobj; ++i) {
		const TzObj *o = &m->obj[i];
		if (!tz_obj_vivo(o) || o->dono < 1 || o->dono > 8 || !o->visao) continue;
		if (!tz_obj_unidade(o) && !tz_obj_edificio(o)) continue;
		int rr[4];
		tz_obj_ret(m, o, rr);
		int v = o->visao, v2 = v * v + v;
		int cx = (rr[0] + rr[2]) / 2, cy = (rr[1] + rr[3]) / 2;
		int ex = (rr[2] - rr[0]) / 2, ey = (rr[3] - rr[1]) / 2;
		for (int k = 1; k <= 8; ++k) {
			if (!m->reino[k].ativo || !ve_por(m, o, k)) continue;
			uint8_t *nv = m->reino[k].nevoa;
			for (int dy = -v - ey; dy <= v + ey; ++dy) {
				int y = cy + dy;
				if (y < 0 || y >= n) continue;
				int ay = dy < -ey ? dy + ey : dy > ey ? dy - ey : 0;
				for (int dx = -v - ex; dx <= v + ex; ++dx) {
					int x = cx + dx;
					if (x < 0 || x >= n) continue;
					int ax = dx < -ex ? dx + ex : dx > ex ? dx - ex : 0;
					if (ax * ax + ay * ay <= v2) nv[y * n + x] = 2;
				}
			}
		}
	}
}

int tz_mundo_nevoa(const TzMundo *m, int k, int cx, int cy)
{
	if (cx < 0 || cy < 0 || cx >= m->lado || cy >= m->lado) return 0;
	if (k < 1 || k > 8 || m->regras.sem_nevoa) return 2;
	return m->reino[k].nevoa[cy * m->lado + cx];
}

int tz_mundo_ve(const TzMundo *m, int k, TzId i)
{
	if (i < 0 || i >= m->nobj) return 0;
	const TzObj *o = &m->obj[i];
	if (o->flags & (TZ_F_MORTO | TZ_F_INATIVO | TZ_F_DENTRO | TZ_F_CARREGADO)) return 0;
	if (o->dono == k) return 1;
	int r[4];
	tz_obj_ret(m, o, r);
	int fixo_ = tz_obj_edificio(o) || o->classe == TZC_RES_GOLD || o->classe == TZC_RES_STONE;
	for (int y = r[1]; y <= r[3]; ++y) for (int x = r[0]; x <= r[2]; ++x) {
		int f = tz_mundo_nevoa(m, k, x, y);
		if (f == 2 || (f == 1 && fixo_)) return 1;
	}
	return 0;
}

/* ------------------------------------------------------------------ victoria (scripting.rs check_end) */
static int maior(int c) { return !(c == 2 || (c >= 61 && c <= 64) || c == 67 || c == 68 || c == 133 || c == 136); }

static void derrotar(TzMundo *m, int k)
{
	TzReinoEstado *r = &m->reino[k];
	if (r->derrotado) return;
	r->derrotado = 1;
	tz_ev(m, TZ_EV_DERROTADO, k, TZ_NENHUM, TZ_NENHUM, 0, 0, 0, 0);
}

static void fim_partida(TzMundo *m, int ganhou)
{
	if (m->acabou) return;
	m->acabou = 1;
	m->ganhou = (uint8_t)ganhou;
	m->reino[m->jogador].venceu = (uint8_t)ganhou;
	tz_ev(m, TZ_EV_FIM, m->jogador, TZ_NENHUM, TZ_NENHUM, 0, 0, 0, ganhou);
}

static void verificar_vitoria(TzMundo *m)
{
	if (m->acabou) return;
	uint8_t vivo[9] = { 0 }, castelo[9] = { 0 };
	for (int i = 0; i < m->nobj; ++i) {
		const TzObj *o = &m->obj[i];
		if (o->dono < 1 || o->dono > 8 || (o->flags & (TZ_F_MORTO | TZ_F_MORRENDO | TZ_F_INATIVO))) continue;
		if ((tz_obj_unidade(o) || tz_obj_edificio(o)) && maior(o->classe)) vivo[o->dono] = 1;
		if (o->classe == TZC_CASTLE) castelo[o->dono] = 1;
	}
	for (int k = 1; k <= 8; ++k) {
		TzReinoEstado *r = &m->reino[k];
		if (!r->ativo || r->derrotado || r->nao_pode_perder) continue;
		if (r->perdeu || r->rendido || !vivo[k] || (m->regras.vitoria == 1 && !castelo[k])) derrotar(m, k);
	}
	if (m->regras.vitoria == 2 && m->tic * (uint64_t)TZ_TIC_MS > (uint64_t)m->regras.vitoria_limite * 60000u) {
		int best = 0; uint32_t bp = 0;
		for (int k = 1; k <= 8; ++k) if (m->reino[k].ativo && !m->reino[k].derrotado && (best == 0 || m->reino[k].pontos > bp)) { best = k; bp = m->reino[k].pontos; }
		for (int k = 1; k <= 8; ++k) if (k != best && m->reino[k].ativo) derrotar(m, k);
	}
	if (m->regras.vitoria == 3)
		for (int k = 1; k <= 8; ++k)
			if (m->reino[k].ativo && !m->reino[k].derrotado && (m->reino[k].pontos + 50) / 100 >= m->regras.vitoria_limite)
				for (int q = 1; q <= 8; ++q) if (q != k && m->reino[q].ativo) derrotar(m, q);
	if (m->regras.sem_vitoria_auto) { if (m->reino[m->jogador].derrotado) fim_partida(m, 0); return; }
	/* gana quien queda sin enemigos en pie (los aliados que comparten la victoria cuentan juntos) */
	int ganhador = 0;
	for (int p = 1; p <= 8; ++p) {
		TzReinoEstado *r = &m->reino[p];
		if (!r->ativo || r->derrotado || r->nao_pode_ganhar) continue;
		int perde = 0;
		for (int q = 1; q <= 8 && !perde; ++q) {
			if (q == p || !m->reino[q].ativo || m->reino[q].derrotado) continue;
			int partilha = (relacao_bits(m, p, q) & 0x40) && (m->reino[p].diplomacia[p] & 8) && (m->reino[q].diplomacia[q] & 8);
			if (!partilha && !((relacao_bits(m, p, q) & 0x40) && (relacao_bits(m, q, p) & 0x40))) perde = 1;
		}
		if (!perde) { ganhador = p; break; }
	}
	if (m->reino[m->jogador].derrotado) fim_partida(m, 0);
	else if (ganhador) fim_partida(m, tz_mundo_relacao(m, m->jogador, ganhador) > 0);
}

/* ------------------------------------------------------------------ comandos */
int tz_mundo_comando(TzMundo *m, const TzComando *c)
{
	if (!c || c->reino > TZ_REINOS || c->nids < 0 || c->nids > TZ_CMD_IDS) return -1;
	if (m->dentro_tic) { tz_aplicar_comando(m, c); return 0; }
	if (m->ncmd >= TZ_COMANDOS) return -1;
	m->cmd[m->ncmd++] = *c;
	return 0;
}

int tz_mundo_gancho(TzMundo *m, const TzGancho *g)
{
	if (m->nganchos >= TZ_GANCHOS || !g) return -1;
	m->gancho[m->nganchos] = *g;
	if (!m->gancho[m->nganchos].cada) m->gancho[m->nganchos].cada = 1;
	m->nganchos++;
	return 0;
}

/* ------------------------------------------------------------------ el tic */
void tz_mundo_tic(TzMundo *m)
{
	TzPriv *p = m->priv;
	/* 1. comandos que llegaron desde el tic anterior, en orden */
	m->dentro_tic = 1;
	for (int i = 0; i < m->ncmd; ++i) tz_aplicar_comando(m, &m->cmd[i]);
	m->ncmd = 0;
	contar_populacao(m);
	/* 2. ganchos: IA, guiones */
	for (int g = 0; g < m->nganchos; ++g) {
		TzGancho *h = &m->gancho[g];
		if (h->tic && m->tic % h->cada == h->desfase % h->cada) h->tic(m, h->dados);
	}
	/* 3. cada objeto que actúa, por su índice */
	int n = m->nobj;
	for (int i = 0; i < n; ++i) {
		TzObj *o = &m->obj[i];
		if (!o->ator) continue;
		if (o->flags & TZ_F_MORRENDO) {
			/* image_die: un par de segundos y desaparece */
			if (m->tic - o->anim_tic >= 3u * TZ_TICS_SEG) {
				o->flags = (o->flags & ~(uint32_t)TZ_F_MORRENDO) | TZ_F_MORTO;
				free(o->ator->caminho); free(o->ator); o->ator = NULL;
			}
			continue;
		}
		if (o->flags & (TZ_F_MORTO | TZ_F_INATIVO | TZ_F_DENTRO | TZ_F_CARREGADO)) continue;
		if (tz_obj_unidade(o)) tz_ator_tic(m, i);
		else tz_producao_tic(m, i);
	}
	/* 4. proyectiles */
	tz_projeteis_tic(m);
	m->dentro_tic = 0;
	/* 5. niebla, islas, victoria */
	if (m->tic % 5 == 0) atualizar_nevoa(m);
	if (p->ilhas_sujas && m->tic % 40 == 0) { tz_ilhas(m); p->ilhas_sujas = 0; }
	if (m->tic % TZ_TICS_SEG == 0) verificar_vitoria(m);
	m->tic++;
}

uint32_t tz_mundo_suma(const TzMundo *m) { return tz_visitar_suma(m); }

/* ------------------------------------------------------------------ consultas */
TzId tz_mundo_em(const TzMundo *m, int cx, int cy)
{
	if (cx < 0 || cy < 0 || cx >= m->lado || cy >= m->lado) return TZ_NENHUM;
	const TzCelula *c = &m->cel[cy * m->lado + cx];
	if (c->ocupante >= 0) return c->ocupante;
	if (c->fixo >= 0) return c->fixo;
	/* unidades que vuelan, objetos sueltos */
	for (int i = m->nobj - 1; i >= 0; --i) {
		const TzObj *o = &m->obj[i];
		if (o->cx == cx && o->cy == cy && tz_obj_vivo(o) && (tz_obj_unidade(o) || o->classe == TZC_FISHPATCH || o->classe == 80 || o->classe == TZC_ITEM)) return i;
	}
	return TZ_NENHUM;
}

TzId tz_mundo_por_id_mapa(const TzMundo *m, int id_mapa)
{
	for (int i = 0; i < m->nobj; ++i) if (m->obj[i].id_mapa == id_mapa && !(m->obj[i].flags & TZ_F_MORTO)) return i;
	return TZ_NENHUM;
}

int tz_mundo_area(const TzMundo *m, int x0, int y0, int x1, int y1, int filtro, int dono, TzId *out, int max)
{
	if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
	if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
	int n = 0;
	for (int i = 0; i < m->nobj && n < max; ++i) {
		const TzObj *o = &m->obj[i];
		if (!tz_obj_vivo(o)) continue;
		if (dono >= 0 && o->dono != dono) continue;
		int u = tz_obj_unidade(o), e = tz_obj_edificio(o);
		int rc = o->classe == TZC_RES_GOLD || o->classe == TZC_RES_STONE || o->classe == TZC_FISHPATCH || o->classe == 80;
		if (filtro && !((filtro & TZ_FILTRO_UNIDADES && u) || (filtro & TZ_FILTRO_EDIFICIOS && e) || (filtro & TZ_FILTRO_RECURSOS && rc))) continue;
		if (!filtro && !u && !e && !rc) continue;
		int r[4];
		tz_obj_ret(m, o, r);
		if (r[2] < x0 || r[0] > x1 || r[3] < y0 || r[1] > y1) continue;
		out[n++] = i;
	}
	return n;
}

/* coste con las rebajas de las investigaciones (gameplay.rs Catalog::stats) */
void tz_mundo_custo_classe(const TzMundo *m, int k, int classe, uint32_t custo[TZ_NREC], uint32_t *tempo_ms)
{
	int civ = k >= 1 && k <= 8 ? m->reino[k].civ : 1;
	const TzAtributos *a = tz_catalogo_at(m->cat, classe, civ);
	memcpy(custo, a->custo, sizeof a->custo);
	uint32_t t = a->tempo_ms;
	const char *nome = tz_classes[classe];
	struct { const char *te; int pct, comida; } cortes[3];
	int nc = 0;
	if (!strcmp(nome, "peon")) { cortes[nc++] = (typeof(cortes[0])){ "te_feudalism", 80, 1 }; cortes[nc++] = (typeof(cortes[0])){ "te_slavery", 80, 1 }; }
	else if (!strcmp(nome, "horse_archer") || !strcmp(nome, "heavy_cavalry")) cortes[nc++] = (typeof(cortes[0])){ "te_horsemanship", 75, 0 };
	else if (!strcmp(nome, "monk") && civ == 3) cortes[nc++] = (typeof(cortes[0])){ "te_meditation", 75, 0 };
	for (int c = 0; c < nc; ++c)
		for (int v = tz_pesquisas(m, k, cortes[c].te); v; --v)
			for (int i = 0; i < TZ_NREC; ++i) if (!cortes[c].comida || i == TZ_COMIDA) custo[i] = custo[i] * (uint32_t)cortes[c].pct / 100;
	if (tz_classe_e_edificio(classe)) for (int v = tz_pesquisas(m, k, "te_architecture"); v; --v) t = t * 70 / 100;
	if (tempo_ms) *tempo_ms = t;
}

void tz_mundo_custo_tec(const TzMundo *m, int k, int tec, uint32_t custo[TZ_NREC], uint32_t *tempo_ms)
{
	(void)k;
	if (tec < 0 || tec >= m->cat->ntec) { memset(custo, 0, sizeof(uint32_t) * TZ_NREC); if (tempo_ms) *tempo_ms = 0; return; }
	memcpy(custo, m->cat->tec[tec].custo, sizeof(uint32_t) * TZ_NREC);
	if (tempo_ms) *tempo_ms = m->cat->tec[tec].tempo_ms;
}

int tz_mundo_cabe(const TzMundo *m, int k, int classe, int cx, int cy)
{
	const TzAtributos *a = tz_catalogo_at(m->cat, classe, k >= 1 && k <= 8 ? m->reino[k].civ : 1);
	int w = a->pegada[0], h = a->pegada[1];
	if (cx < 0 || cy < 0 || cx + w > m->lado || cy + h > m->lado) return tz_falha("fuera del mapa");
	for (int y = cy; y < cy + h; ++y) for (int x = cx; x < cx + w; ++x) {
		const TzCelula *c = &m->cel[y * m->lado + x];
		int naval = classe == TZC_HARBOR || classe == TZC_SHIPYARD;
		if (c->bits & (TZ_CEL_EDIFICIO | TZ_CEL_MURO | TZ_CEL_PORTA | TZ_CEL_ARVORE | TZ_CEL_PENHASCO | TZ_CEL_CAMPO | TZ_CEL_PONTE))
			return tz_falha("el sitio está ocupado");
		if (classe == TZC_BRIDGE) { if (!(c->bits & (TZ_CEL_AGUA | TZ_CEL_COSTA))) continue; continue; }
		if (!naval && (c->bits & (TZ_CEL_AGUA | TZ_CEL_COSTA))) return tz_falha("no se puede construir en el agua");
		if (c->ocupante >= 0 && m->obj[c->ocupante].dono != k) return tz_falha("hay unidades en el sitio");
		if (k >= 1 && k <= 8 && !m->regras.sem_nevoa && !m->reino[k].nevoa[y * m->lado + x]) return tz_falha("sitio sin explorar");
	}
	return 0;
}

/* gameplay.rs Catalog::available: prohibido, ya investigado, depende de una investigación o de un edificio */
int tz_disponivel(const TzMundo *m, int k, int civ, const char *nome, int tecnologia)
{
	const TzCatalogo *c = m->cat;
	TzReinoEstado *r = (TzReinoEstado *)&m->reino[k];
	int t = tz_catalogo_tec(c, nome), cl = tz_catalogo_classe(nome);
	if (t >= 0 && r->proibido[t]) return tz_falha("prohibido en esta misión");
	if (cl >= 0 && r->proibido[c->ntec + cl]) return tz_falha("prohibido en esta misión");
	if (tecnologia && t >= 0 && r->pesquisas[t]) return tz_falha("ya investigado");
	const TzIni *tree = civ >= 1 && civ <= 3 ? &c->arvores[civ] : NULL;
	const char *dep = NULL;
	for (int q = 1; q <= 3 && !dep; ++q) dep = tz_ini(&c->arvores[q], "orders", nome);
	if (dep && !tz_pesquisas(m, k, dep)) return tz_falha("necesita %s", dep);
	if (tree) {
		dep = tz_ini(tree, "depend", nome);
		if (dep && *dep && !tz_pesquisas(m, k, dep)) return tz_falha("necesita %s", dep);
		dep = tz_ini(tree, "required", nome);
		if (dep && *dep) {
			int tem = 0;
			for (int i = 0; i < m->nobj && !tem; ++i) {
				const TzObj *o = &m->obj[i];
				if (o->dono == k && tz_obj_vivo(o) && !(o->flags & TZ_F_OBRA) && !strcasecmp(tz_classes[o->classe], dep)) tem = 1;
			}
			if (!tem) return tz_falha("necesita %s", dep);
		}
	}
	return 0;
}

int tz_mundo_pode(const TzMundo *m, int k, TzId i, const char *chave)
{
	const TzObj *o = tz_mundo_obj((TzMundo *)m, i);
	if (!o || !tz_obj_vivo(o)) return tz_falha("el objeto no existe");
	if (k < 1 || k > 8 || o->dono != k) return tz_falha("no es de ese reino");
	const TzOrdemDef *d = tz_catalogo_ordem(m->cat, chave);
	if (!d) return tz_falha("orden desconocida: %s", chave);
	const TzOrdemDef *menu[160];
	int n = tz_catalogo_menu(m->cat, o->classe, o->civ, menu, 160), esta = 0;
	for (int j = 0; j < n; ++j) if (menu[j] == d) esta = 1;
	if (!esta) return tz_falha("esa orden no está en su menú");
	if (o->flags & TZ_F_OBRA) return tz_falha("aún está en obras");
	int tec = !strcmp(d->acao, "technology") || !strncmp(d->acao, "upgrade", 7);
	if (tz_disponivel(m, k, o->civ, d->classe, tec)) return -1;
	return 0;
}

/* gameplay.rs, la orden de «default»: lo que haría el clic derecho */
TzOrdem tz_mundo_ordem_padrao(const TzMundo *m, int k, const TzId *ids, int n, int cx, int cy, TzId alvo)
{
	TzOrdem o;
	memset(&o, 0, sizeof o);
	o.tipo = TZ_O_MOVER; o.alvo = TZ_NENHUM; o.x = (int16_t)cx; o.y = (int16_t)cy;
	int peao = 0, atacante = 0;
	for (int i = 0; i < n; ++i) {
		const TzObj *u = &m->obj[ids[i]];
		if (u->classe == TZC_PEON) peao = 1;
		if (u->ataque) atacante = 1;
	}
	if (alvo >= 0 && alvo < m->nobj && tz_obj_vivo(&m->obj[alvo])) {
		const TzObj *t = &m->obj[alvo];
		o.alvo = alvo;
		int rel = tz_mundo_relacao(m, k, t->dono);
		if (t->dono && t->dono != k && rel < 0 && atacante) { o.tipo = TZ_O_ATACAR; return o; }
		if (peao && (t->flags & TZ_F_OBRA) && rel > 0) { o.tipo = TZ_O_CONSTRUIR; return o; }
		if (peao && tz_obj_edificio(t) && rel > 0 && t->vida < t->vida_max) { o.tipo = TZ_O_REPARAR; return o; }
		if (peao && (t->classe == TZC_RES_GOLD || t->classe == TZC_RES_STONE || t->classe == TZC_FIELD || t->classe == TZC_COW || t->classe == 80)) { o.tipo = TZ_O_RECOLHER; return o; }
		if (tz_obj_at(m, t)->capacidade && rel > 0 && (tz_obj_edificio(t) || tz_obj_at(m, t)->naval)) { o.tipo = TZ_O_ENTRAR; return o; }
		if (tz_obj_unidade(t) && rel > 0) { o.tipo = TZ_O_SEGUIR; return o; }
		if (t->dono && t->dono != k && atacante) { o.tipo = TZ_O_ATACAR; return o; }
		o.alvo = TZ_NENHUM;
	}
	if (peao && cx >= 0 && cy >= 0 && cx < m->lado && cy < m->lado && (m->cel[cy * m->lado + cx].bits & TZ_CEL_ARVORE)) o.tipo = TZ_O_CORTAR;
	return o;
}

/* ------------------------------------------------------------------ acciones de los guiones */
TzId tz_mundo_guiao_criar(TzMundo *m, int classe, int civ, int dono, int cx, int cy)
{
	if (classe <= 0 || classe >= TZ_NCLASSES || cx < 0 || cy < 0 || cx >= m->lado || cy >= m->lado) return TZ_NENHUM;
	if (!civ && dono >= 1 && dono <= 8) civ = m->reino[dono].civ;
	TzObj tmp; memset(&tmp, 0, sizeof tmp); tmp.classe = (uint16_t)classe; tmp.civ = (uint8_t)(civ ? civ : 1);
	if (tz_classe_e_unidade(classe) && !tz_cel_livre(m, tz_obj_capa(m, &tmp), dono, cx, cy)) {
		int r[4] = { cx, cy, cx, cy }, x, y;
		if (!tz_lugar_livre(m, tz_obj_capa(m, &tmp), dono, r, &x, &y)) return TZ_NENHUM;
		cx = x; cy = y;
	}
	TzId i = tz_novo_obj(m, classe, civ, dono, cx, cy, 0);
	if (i >= 0) tz_ev(m, TZ_EV_CRIADO, dono, i, TZ_NENHUM, cx, cy, classe, 0);
	return i;
}

void tz_mundo_guiao_remover(TzMundo *m, TzId i)
{
	TzObj *o = tz_mundo_obj(m, i);
	if (!o || (o->flags & TZ_F_MORTO)) return;
	tz_marcar(m, i, 0);
	o->flags |= TZ_F_MORTO;
	if (o->ator) { free(o->ator->caminho); free(o->ator); o->ator = NULL; }
	tz_ev(m, TZ_EV_REMOVIDO, o->dono, i, TZ_NENHUM, o->cx, o->cy, o->classe, 0);
}

void tz_mundo_guiao_matar(TzMundo *m, TzId i) { if (tz_mundo_obj(m, i)) tz_matar(m, i, TZ_NENHUM); }
void tz_mundo_guiao_dono(TzMundo *m, TzId i, int dono) { if (tz_mundo_obj(m, i)) tz_trocar_dono(m, i, dono); }

void tz_mundo_guiao_recursos(TzMundo *m, int k, int recurso, int32_t delta)
{
	if (k < 1 || k > 8 || recurso < 0 || recurso >= TZ_NREC) return;
	int64_t v = (int64_t)m->reino[k].recursos[recurso] + delta;
	m->reino[k].recursos[recurso] = (int32_t)(v < 0 ? 0 : v > 0x7fffffff ? 0x7fffffff : v);
}

void tz_mundo_guiao_diplomacia(TzMundo *m, int a, int b, uint32_t bits)
{
	if (a < 1 || a > 8 || b < 1 || b > 8) return;
	m->reino[a].diplomacia[b] = bits;
	tz_ev(m, TZ_EV_DIPLOMACIA, a, TZ_NENHUM, TZ_NENHUM, 0, 0, 0, b);
}

void tz_mundo_guiao_revelar(TzMundo *m, int k, int cx, int cy, int raio)
{
	if (k < 1 || k > 8) return;
	for (int y = cy - raio; y <= cy + raio; ++y) for (int x = cx - raio; x <= cx + raio; ++x)
		if (x >= 0 && y >= 0 && x < m->lado && y < m->lado && (x - cx) * (x - cx) + (y - cy) * (y - cy) <= raio * raio + raio)
			if (!m->reino[k].nevoa[y * m->lado + x]) m->reino[k].nevoa[y * m->lado + x] = 1;
}

void tz_mundo_guiao_pesquisa(TzMundo *m, int k, int tec)
{
	if (k < 1 || k > 8 || tec < 0 || tec >= m->cat->ntec) return;
	if (m->reino[k].pesquisas[tec] < 255) m->reino[k].pesquisas[tec]++;
	for (int i = 0; i < m->nobj; ++i) if (m->obj[i].dono == k) tz_recalcular(m, i);
	tz_ev(m, TZ_EV_PESQUISA, k, TZ_NENHUM, TZ_NENHUM, 0, 0, 0, tec);
}

void tz_mundo_guiao_proibir(TzMundo *m, int k, const char *nome, int proibido)
{
	if (k < 1 || k > 8) return;
	int t = tz_catalogo_tec(m->cat, nome), c = tz_catalogo_classe(nome);
	if (t >= 0) m->reino[k].proibido[t] = (uint8_t)!!proibido;
	if (c >= 0) m->reino[k].proibido[m->cat->ntec + c] = (uint8_t)!!proibido;
}

void tz_mundo_guiao_ordem(TzMundo *m, TzId i, const TzOrdem *o)
{
	TzObj *u = tz_mundo_obj(m, i);
	if (u && u->ator && tz_obj_vivo(u)) tz_dar_ordem(m, i, o, 0);
}

void tz_mundo_guiao_mensagem(TzMundo *m, int k, const char *t)
{
	tz_ev(m, TZ_EV_MENSAGEM, k, TZ_NENHUM, TZ_NENHUM, 0, 0, 0, texto(m, t));
}

void tz_mundo_guiao_som(TzMundo *m, const char *nome, int cx, int cy)
{
	tz_ev(m, TZ_EV_SOM, 0, TZ_NENHUM, TZ_NENHUM, cx, cy, 0, texto(m, nome));
}

void tz_mundo_guiao_fim(TzMundo *m, int k, int ganhou)
{
	if (k < 1 || k > 8) return;
	if (ganhou) m->reino[k].venceu = 1; else { m->reino[k].perdeu = 1; derrotar(m, k); }
	if (k == m->jogador) fim_partida(m, ganhou);
}

/* ------------------------------------------------------------------ puente con la escena */
void tz_mundo_objeto_mapa(const TzMundo *m, TzId i, TzObjeto *out)
{
	const TzObj *o = &m->obj[i];
	if (o->original) *out = *o->original;
	else {
		memset(out, 0, sizeof *out);
		out->civ = o->civ;
	}
	out->id = o->id_mapa;
	out->classe = o->classe;
	out->flags = (o->original ? o->original->flags : 0) & ~(uint32_t)(TZ_F_OBRA | TZ_F_MORTO);
	out->flags |= o->flags & (TZ_F_OBRA | TZ_F_MORTO | TZ_F_INATIVO);
	if (o->flags & TZ_F_MORRENDO) out->flags |= 0x10000000u;
	out->x = (uint16_t)(o->cx < 0 ? 0 : o->cx);
	out->y = (uint16_t)(o->cy < 0 ? 0 : o->cy);
	out->dono = o->dono;
	if (tz_obj_unidade(o)) out->variante = (uint16_t)(0x8000 | (o->direcao & 7));
	out->vida = (uint16_t)(o->vida > 65535 ? 65535 : o->vida < 0 ? 0 : o->vida);
	out->vida_max = (uint16_t)(o->vida_max > 65535 ? 65535 : o->vida_max);
}

void tz_mundo_pose(const TzMundo *m, TzId i, TzEstado *s, int *px, int *py)
{
	const TzObj *o = &m->obj[i];
	memset(s, 0, sizeof *s);
	switch (o->estado) {
	case TZ_E_ANDANDO: s->pose = TZ_POSE_ANDAR; break;
	case TZ_E_ATACANDO: s->pose = TZ_POSE_ATACAR; break;
	case TZ_E_CONSTRUINDO: s->pose = TZ_POSE_CONSTRUIR; break;
	case TZ_E_LANCANDO: s->pose = TZ_POSE_FEITICO; break;
	case TZ_E_MORRENDO: s->pose = TZ_POSE_MORRER; break;
	case TZ_E_TRABALHANDO:
		s->pose = TZ_POSE_TRABALHAR;
		switch (o->ator ? o->ator->trabalho : 0) {
		case TZ_TRAB_CORTAR: s->tarefa = TZ_TAREFA_CORTAR; break;
		case TZ_TRAB_MINAR: s->tarefa = TZ_TAREFA_MINAR; break;
		case TZ_TRAB_ORDENHAR: s->tarefa = TZ_TAREFA_ORDENHAR; break;
		case TZ_TRAB_CAPTURAR: s->tarefa = TZ_TAREFA_CAPTURAR; break;
		default: s->tarefa = TZ_TAREFA_CAVAR; break;
		}
		break;
	default: s->pose = TZ_POSE_PARADO; break;
	}
	if (o->flags & TZ_F_MORRENDO) s->pose = TZ_POSE_MORRER;
	s->direcao = o->direcao & 7;
	s->em_obra = (o->flags & TZ_F_OBRA) != 0;
	s->vida = o->vida > 0 ? o->vida : 0;
	s->vida_max = o->vida_max;
	s->linha_ponte = o->ator && (o->classe == TZC_GATE_WOOD || o->classe == TZC_GATE_STONE) ? o->ator->ciclos : 6;
	s->desde_ms = o->anim_tic * TZ_TIC_MS;
	int x, y;
	tz_obj_pixel(o, &x, &y);
	if (tz_obj_unidade(o)) { x -= TZ_CASILLA / 2; y -= TZ_CASILLA / 2; }
	else { x = o->cx * TZ_CASILLA; y = o->cy * TZ_CASILLA; }
	*px = x; *py = y;
}

/* ------------------------------------------------------------------ más acciones de los guiones (pedidas por guiao.c) */
int tz_mundo_guiao_aparecer(TzMundo *m, TzId i)
{
	TzObj *o = tz_mundo_obj(m, i);
	if (!o || (o->flags & TZ_F_MORTO)) return -1;
	if (!(o->flags & TZ_F_INATIVO)) return 0;
	o->flags &= ~(uint32_t)TZ_F_INATIVO;
	if (tz_obj_unidade(o) && !tz_cel_livre(m, tz_obj_capa(m, o), o->dono, o->cx, o->cy)) {
		int r[4] = { o->cx, o->cy, o->cx, o->cy }, x, y;
		if (!tz_lugar_livre(m, tz_obj_capa(m, o), o->dono, r, &x, &y)) { o->flags |= TZ_F_INATIVO; return -1; }
		o->cx = (int16_t)x; o->cy = (int16_t)y;
	}
	centrar(o);
	if (!o->ator && precisa_ator(o)) o->ator = novo_ator();
	tz_marcar(m, i, 1);
	tz_ev(m, TZ_EV_CRIADO, o->dono, i, TZ_NENHUM, o->cx, o->cy, o->classe, 3);
	return 0;
}

void tz_mundo_guiao_atributos(TzMundo *m, TzId i, int32_t vida, int nivel, int mana, uint32_t poner, uint32_t quitar)
{
	TzObj *o = tz_mundo_obj(m, i);
	if (!o) return;
	if (vida >= 0) { if (vida > o->vida_max) o->vida_max = vida; o->vida = vida; }
	if (nivel >= 0) { o->nivel = (uint8_t)(nivel > 255 ? 255 : nivel); tz_recalcular(m, i); }
	if (mana >= 0) { o->mana = (uint16_t)(mana > 65535 ? 65535 : mana); if (o->mana > o->mana_max) o->mana_max = o->mana; }
	o->flags = (o->flags | poner) & ~quitar;
	if (vida == 0 && !(o->flags & TZ_F_DEUS)) tz_matar(m, i, TZ_NENHUM);
}

void tz_mundo_guiao_terminar_obra(TzMundo *m, TzId i)
{
	TzObj *o = tz_mundo_obj(m, i);
	if (o && (o->flags & TZ_F_OBRA) && !(o->flags & TZ_F_MORTO)) tz_obra_pronta(m, i);
}

void tz_mundo_guiao_revelar_temp(TzMundo *m, int k, int cx, int cy, int raio, uint32_t tics)
{
	if (k < 1 || k > 8) return;
	int livre = -1;
	for (int q = 0; q < TZ_VISOES; ++q) if (!m->visao[q].reino || m->visao[q].ate <= m->tic) { livre = q; break; }
	if (livre < 0) return;
	m->visao[livre].reino = (uint8_t)k;
	m->visao[livre].x = (int16_t)cx; m->visao[livre].y = (int16_t)cy; m->visao[livre].raio = (int16_t)raio;
	m->visao[livre].ate = m->tic + tics;
}

void tz_mundo_recursos_compartidos(const TzMundo *m, int k, int32_t out[TZ_NREC])
{
	memset(out, 0, sizeof(int32_t) * TZ_NREC);
	if (k < 1 || k > 8) return;
	for (int q = 1; q <= 8; ++q) {
		if (q != k && !((m->reino[k].diplomacia[q] & TZ_DIP_RECURSOS) && (m->reino[q].diplomacia[k] & TZ_DIP_RECURSOS))) continue;
		if (!m->reino[q].ativo) continue;
		for (int j = 0; j < TZ_NREC; ++j) out[j] += m->reino[q].recursos[j];
	}
}

/* world/starting.rs free_near: casilla libre (tierra, sin árbol, acantilado ni nada) para un rectángulo w x h */
static int livre_ret(const TzMundo *m, int x, int y, int w, int h)
{
	if (x < 0 || y < 0 || x + w >= m->lado || y + h >= m->lado) return 0;
	for (int cy = y; cy < y + h; ++cy) for (int cx = x; cx < x + w; ++cx) {
		const TzCelula *c = &m->cel[cy * m->lado + cx];
		if (c->bits & (TZ_CEL_AGUA | TZ_CEL_COSTA | TZ_CEL_PENHASCO | TZ_CEL_ARVORE | TZ_CEL_EDIFICIO | TZ_CEL_MURO | TZ_CEL_PORTA | TZ_CEL_CAMPO | TZ_CEL_PONTE)) return 0;
		if (c->ocupante >= 0) return 0;
	}
	return 1;
}

static int perto_livre(const TzMundo *m, int x0, int y0, int w, int h, int *x, int *y)
{
	for (int r = 0; r < m->lado; ++r) {
		for (int xx = x0 - r; xx <= x0 + r; ++xx)
			for (int k = 0; k < 2; ++k) { int yy = k ? y0 + r : y0 - r; if (livre_ret(m, xx, yy, w, h)) { *x = xx; *y = yy; return 1; } }
		for (int yy = y0 - r + 1; yy < y0 + r; ++yy)
			for (int k = 0; k < 2; ++k) { int xx = k ? x0 + r : x0 - r; if (livre_ret(m, xx, yy, w, h)) { *x = xx; *y = yy; return 1; } }
	}
	return 0;
}

int tz_mundo_inicio_livre(TzMundo *m)
{
	int ini[9][2];
	for (int k = 1; k <= 8; ++k) {
		ini[k][0] = ini[k][1] = -1;
		const TzReino *mr = k <= m->mapa->nreinos ? &m->mapa->reinos[k - 1] : NULL;
		if (mr && mr->inicio[0] >= 0) { ini[k][0] = mr->inicio[0]; ini[k][1] = mr->inicio[1]; continue; }
		/* sin casilla: el castillo más cercano al centro de sus edificios, o el centro de sus fuerzas */
		long sx = 0, sy = 0, n = 0, ux = 0, uy = 0, un = 0;
		for (int i = 0; i < m->nobj; ++i) {
			const TzObj *o = &m->obj[i];
			if (o->dono != k || (o->flags & (TZ_F_MORTO | TZ_F_INATIVO))) continue;
			if (tz_obj_edificio(o)) { sx += o->cx; sy += o->cy; ++n; }
			else if (tz_obj_unidade(o)) { ux += o->cx; uy += o->cy; ++un; }
		}
		if (n) {
			int cx = (int)(sx / n), cy = (int)(sy / n);
			long best = -1;
			for (int i = 0; i < m->nobj; ++i) {
				const TzObj *o = &m->obj[i];
				if (o->dono != k || o->classe != TZC_CASTLE || (o->flags & (TZ_F_MORTO | TZ_F_INATIVO))) continue;
				long d = (long)(o->cx - cx) * (o->cx - cx) + (long)(o->cy - cy) * (o->cy - cy);
				if (best < 0 || d < best) { best = d; ini[k][0] = o->cx; ini[k][1] = o->cy; }
			}
			if (best < 0 && n + un) { ini[k][0] = (int)((sx + ux) / (n + un)); ini[k][1] = (int)((sy + uy) / (n + un)); }
		} else if (un) { ini[k][0] = (int)(ux / un); ini[k][1] = (int)(uy / un); }
	}
	/* fuera las fuerzas de los reinos (la naturaleza se queda) */
	for (int i = 0; i < m->nobj; ++i) {
		TzObj *o = &m->obj[i];
		if (o->dono < 1 || o->dono > 8 || !(tz_obj_unidade(o) || tz_obj_edificio(o)) || (o->flags & TZ_F_MORTO)) continue;
		tz_marcar(m, i, 0);
		o->flags |= TZ_F_MORTO | TZ_F_INATIVO;
		if (o->ator) { free(o->ator->caminho); free(o->ator); o->ator = NULL; }
	}
	for (int k = 1; k <= 8; ++k) {
		if (!m->reino[k].ativo) continue;
		if (ini[k][0] < 0) return tz_falha("reino %d: ninguna posición inicial en el mapa", k);
		int x, y;
		if (!perto_livre(m, ini[k][0], ini[k][1], 8, 8, &x, &y)) return tz_falha("reino %d: no cabe el castillo", k);
		TzId c = tz_novo_obj(m, TZC_CASTLE, m->reino[k].civ, k, x, y, 0);
		if (c < 0) return -1;
		for (int q = 0; q < 3; ++q) {
			int px, py;
			if (!perto_livre(m, x, y, 1, 1, &px, &py)) return tz_falha("reino %d: no caben los peones", k);
			if (tz_novo_obj(m, TZC_PEON, m->reino[k].civ, k, px, py, 0) < 0) return -1;
		}
	}
	((TzPriv *)m->priv)->ilhas_sujas = 0;
	tz_ilhas(m);
	return 0;
}
