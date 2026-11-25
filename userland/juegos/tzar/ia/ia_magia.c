/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ia_magia.c - los hechizos de las unidades de un reino de la IA y las metas que hace su propia rutina: invocaciones
 * del mago, la cruzada, el guerrero de la yihad y el cambio de comida en la taberna (D417, fase 6; D470).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later, commit cf2af88):
 * src/gameplay/ai/spells.rs (lo que lanza una unidad antes de atacar 0x414730 con 0x414cd0, 0x4145e0 y 0x4146e0; el
 * sacerdote ocioso 0x414f10; el ninja que se esconde al salir 0x4ebe10; la huida de las unidades del reino de su propia
 * lluvia de fuego y su hielo 0x472760), src/gameplay/ai/goals.rs (la criatura del mago 0x409ab0, la cruzada 0x409ed0 y
 * 0x409d30, el guerrero de la yihad 0x409500 y 0x409290) y de rules.rs la meta CONVERT (convert, 0x409771).
 * Diferencias (docs/tzar.md, «La IA»): el original decide el hechizo dentro del combate de la simulación, antes de cada
 * golpe; aquí la IA lo decide en cada mirada para sus unidades que están atacando (y vuelve a encolar el ataque tras el
 * hechizo); la huida del área de la lluvia de fuego y del hielo se manda al pedir el hechizo; los sorteos del sacerdote
 * usan el generador de la IA (el original, el de la partida, que la IA no puede tocar). El cambio de la taberna se paga
 * en múltiplos de 500 entre 500 y 20 000 (lo que acepta TZ_C_TROCAR).
 */
#include "ia_int.h"

/* los números de los hechizos (0x58a2f0, world.rs spell_key) */
enum { F_LLUVIA = 0, F_RAYO = 1, F_CURAR = 2, F_BENDECIR = 5, F_HIELO = 6, F_FURIA = 7, F_CAPA = 9, F_REMOLINO = 15,
       F_BOLA = 17, F_ENSENAR = 18 };

#define AREA 3               /* spells.rs AREA */
#define FORCA_AREA 20        /* spells.rs AREA_STRENGTH */
#define VIDA_RAYO 100        /* spells.rs BOLT_HEALTH */
#define FUGA 5               /* las cinco casillas de la huida (0x430be0) */
#define ALCANCE_CURA 64      /* spells.rs HEAL_REACH */
#define ALCANCE_SACERDOTE 15 /* spells.rs PRIEST_REACH */
#define MANA_NINJA 90        /* spells.rs HIDE_MANA */
#define ANEIS_INVOCAR 6      /* goals.rs SUMMON_RINGS */
#define ANEIS_ANCORA 4       /* goals.rs ANCHOR_RINGS */
#define REFUGIO 24           /* idle.rs REFUGE_REACH */

static const char *chave(int f) { return tz_feitico_chave(f); }

static int maquina(int c) { return c == 16 || (c >= 19 && c <= 25) || c == 31 || c == 36 || c == TZC_FISHING_BOAT; }

static int com_efeito(const TzIa *ia, const TzObj *o, int ef)
{
	return o->ator && o->ator->efeito == ef && ia->m->tic < o->ator->efeito_ate;
}

/* el primer libro del hechizo entre lo que lleva la unidad, con cargas (0x414ce5) */
static TzId livro(const TzIa *ia, TzId i, int f)
{
	const TzObj *u = ia_obj(ia, i);
	if (!u || !u->ator || !u->ator->nitens) return TZ_NENHUM;
	for (TzId j = 0; j < ia->m->nobj; ++j) {
		const TzObj *x = &ia->m->obj[j];
		if (x->classe != TZC_LIVRO || !(x->flags & TZ_F_CARREGADO) || x->dentro_de != i) continue;
		if (x->item.feitico == f && !(x->item.finito && !x->item.cargas)) return j;
	}
	return TZ_NENHUM;
}

/* 0x414d3f: la unidad tiene maná y aprendió el hechizo de un libro, o es mago, o sacerdote para curar, bendecir y
 * (asiático) enseñar; y el reino tiene la investigación del hechizo (tz_mundo_pode mira el menú y la investigación) */
static int estudou(const TzIa *ia, TzId i, int f)
{
	const TzObj *o = ia_obj(ia, i);
	const char *k = chave(f);
	if (!o || !o->mana_max || !k) return 0;
	if (o->ator)
		for (int q = 0; q < o->ator->naprendidos; ++q) if (o->ator->aprendidos[q] == f) return 1;
	int ensinado = o->classe == TZC_MAGE ||
	               (o->classe == TZC_PRIEST && (f == F_CURAR || f == F_BENDECIR || (f == F_ENSENAR && o->civ == TZ_CIV_ASIA)));
	if (!ensinado) return 0;
	int ord = tz_feitico_ordem(ia->m, k);
	return ord >= 0 && tz_mundo_pode(ia->m, ia->k, i, ia->m->cat->ord[ord].chave) == 0;
}

/* 0x4723e0: el maná del hechizo; bendecir pide todo el maná de la unidad */
static int custo(const TzIa *ia, TzId i, int f)
{
	if (f == F_BENDECIR) return ia->m->obj[i].mana_max;
	int alc, mana;
	tz_feitico_parametros(ia->m, i, chave(f), &alc, &mana);
	return mana;
}

/* 0x414cd0: un libro cargado del hechizo, si no el maná propio si lo estudió y llega */
enum { MEIO_NADA = 0, MEIO_LIVRO, MEIO_MANA };
static int meio(const TzIa *ia, TzId i, int f, TzId *lv)
{
	*lv = livro(ia, i, f);
	if (*lv != TZ_NENHUM) return MEIO_LIVRO;
	return estudou(ia, i, f) && ia->m->obj[i].mana >= custo(ia, i, f) ? MEIO_MANA : MEIO_NADA;
}

/* un comando de orden, encolado o no */
static int ordem_fila(TzIa *ia, TzId i, int tipo, int arg, TzId alvo, int x, int y, int fila)
{
	TzComando c;
	memset(&c, 0, sizeof c);
	c.tipo = TZ_C_ORDEM;
	c.flags = fila ? TZ_CMD_FILA : 0;
	c.ordem.tipo = (uint8_t)tipo;
	c.ordem.arg = (int16_t)arg;
	c.ordem.alvo = alvo;
	c.ordem.x = (int16_t)x;
	c.ordem.y = (int16_t)y;
	c.nids = 1;
	c.ids[0] = i;
	return ia_enviar(ia, &c);
}

/* 0x4146e0: el uso del libro o el hechizo con maná */
static int lancar(TzIa *ia, TzId i, int f, int meio_, TzId lv, TzId alvo, int x, int y)
{
	int r;
	if (meio_ == MEIO_LIVRO) r = ordem_fila(ia, i, TZ_O_USAR_ITEM, 1, lv, x, y, 0);
	else {
		int ord = tz_feitico_ordem(ia->m, chave(f));
		if (ord < 0) return -1;
		r = ordem_fila(ia, i, TZ_O_HECHIZO, ord, alvo, x, y, 0);
	}
	if (!r) ia->contas_d470.hechizos++;
	return r;
}

/* el centro de un objeto */
static void centro(const TzIa *ia, TzId j, int *x, int *y)
{
	int r[4];
	ia_rect(ia, j, r);
	*x = (r[0] + r[2]) / 2; *y = (r[1] + r[3]) / 2;
}

/* un punto a cinco casillas de p alejándose de q, dentro del mapa */
static void longe(const TzIa *ia, int px, int py, int qx, int qy, int *ox, int *oy)
{
	int dx = px - qx, dy = py - qy;
	dx = dx > 0 ? 1 : dx < 0 ? -1 : 0;
	dy = dy > 0 ? 1 : dy < 0 ? -1 : 0;
	if (!dx && !dy) dy = 1;
	int u = ia->m->lado - 1, x = px + dx * FUGA, y = py + dy * FUGA;
	*ox = x < 0 ? 0 : x > u ? u : x;
	*oy = y < 0 ? 0 : y > u ? u : y;
}

/* 0x4145e0: lo que hay en las casillas a 3 del punto que pasa el filtro, una vez por cada casilla suya allí, entre
 * lo que el reino ataca solo y lo demás; vale con más de un hostil y, si el blanco ataca a la unidad o a un mago del
 * reino, fuerza hostil al menos la de los otros; si no, 20 o más y el doble de la de los otros */
enum { FL_QUIETO, FL_HIELO, FL_FURIA };
static int vale(const TzIa *ia, int px, int py, int filtro, int apuntado)
{
	TzId v[256];
	int n = tz_mundo_area(ia->m, px - AREA - 1, py - AREA - 1, px + AREA, py + AREA, TZ_FILTRO_UNIDADES | TZ_FILTRO_EDIFICIOS, -1, v, 256);
	uint32_t nh = 0, fh = 0, fo = 0;
	int area[4] = { px - AREA, py - AREA, px + AREA, py + AREA };
	for (int q = 0; q < n; ++q) {
		const TzObj *o = &ia->m->obj[v[q]];
		if (o->flags & TZ_F_DENTRO || !ia_ve(ia, v[q])) continue;
		int r[4];
		ia_rect(ia, v[q], r);
		int w = (r[2] < area[2] ? r[2] : area[2]) - (r[0] > area[0] ? r[0] : area[0]) + 1;
		int h = (r[3] < area[3] ? r[3] : area[3]) - (r[1] > area[1] ? r[1] : area[1]) + 1;
		if (w <= 0 || h <= 0) continue;
		int unid = tz_classe_e_unidade(o->classe);
		if (unid && o->estado == TZ_E_ANDANDO) continue;
		if (filtro == FL_HIELO && (!unid || maquina(o->classe) || o->vida == 1)) continue;
		if (filtro == FL_FURIA && com_efeito(ia, o, TZ_EF_FURIA)) continue;
		int hostil = o->dono >= 1 && o->dono <= TZ_REINOS && tz_mundo_relacao(ia->m, ia->k, o->dono) < 0;
		uint32_t s = ia_forca(o) * (uint32_t)(w * h);
		if (hostil) { nh += (uint32_t)(w * h); fh += s; }
		else fo += s;
	}
	if (nh <= 1) return 0;
	return apuntado ? fh >= fo : fh >= FORCA_AREA && fh >= 2 * fo;
}

/* 0x472760: la lluvia de fuego o el hielo de la IA saca del cuadrado de 3 alrededor del punto a las unidades del reino
 * (menos quien lanza y, con el hielo, las máquinas): cada una corre cinco casillas */
static void limpar_area(TzIa *ia, TzId quem, int f, int px, int py)
{
	if (f != F_LLUVIA && f != F_HIELO) return;
	TzId v[128];
	int n = tz_mundo_area(ia->m, px - AREA, py - AREA, px + AREA, py + AREA, TZ_FILTRO_UNIDADES, ia->k, v, 128);
	for (int q = 0; q < n; ++q) {
		const TzObj *o = &ia->m->obj[v[q]];
		if (v[q] == quem || !o->ator || (o->flags & TZ_F_DENTRO) || (f == F_HIELO && maquina(o->classe))) continue;
		if (com_efeito(ia, o, TZ_EF_GELO) || com_efeito(ia, o, TZ_EF_PEDRA)) continue;
		int x, y;
		longe(ia, o->cx, o->cy, px, py, &x, &y);
		ia_ordem(ia, v[q], TZ_O_MOVER, TZ_NENHUM, x, y);
	}
}

/* idle.rs refuge: el castillo o puesto del reino más cercano a 24 con sitio */
static TzId refugio(const TzIa *ia, TzId i)
{
	const TzObj *o = &ia->m->obj[i];
	TzId ref = TZ_NENHUM;
	uint32_t dr = 0;
	for (TzId j = ia->m->nobj - 1; j >= 0; --j) {
		const TzObj *c = &ia->m->obj[j];
		if (c->dono != ia->k || !ia_contado(c) || ia_obra(c) || (c->classe != TZC_CASTLE && c->classe != IC_OUTPOST)) continue;
		const TzAtributos *a = tz_obj_at(ia->m, c);
		if (!a || c->ndentro >= a->capacidade) continue;
		int r[4];
		ia_rect(ia, j, r);
		uint32_t d = ia_dist(o->cx, o->cy, r);
		if (d <= REFUGIO && (ref == TZ_NENHUM || d < dr)) { ref = j; dr = d; }
	}
	return ref;
}

/* 0x414730: antes de que la unidad u ataque a t (ver spells.rs before_attack); 1 si lanza un hechizo, 2 si el mago
 * huye, 0 si nada */
static int antes_de_atacar(TzIa *ia, TzId u, TzId t)
{
	const TzObj *o = &ia->m->obj[u], *x = &ia->m->obj[t];
	if ((uint32_t)x->vida <= o->ataque || x->classe == TZC_DUMMY) return 0;
	TzId mira = TZ_NENHUM;
	if (x->ator && x->ator->ordem.tipo == TZ_O_ATACAR) mira = x->ator->ordem.alvo;
	const TzObj *v = ia_obj(ia, mira);
	if (v && !ia_contado(v)) v = NULL, mira = TZ_NENHUM;
	int apuntado = v && (mira == u || (v->classe == TZC_MAGE && v->dono == o->dono));
	int px, py, md;
	TzId lv;
	/* contra un barco, el remolino, lanzado con maná aunque se encuentre un libro */
	if (x->classe >= 23 && x->classe <= 25 && meio(ia, u, F_REMOLINO, &lv) != MEIO_NADA)
		return !lancar(ia, u, F_REMOLINO, MEIO_MANA, TZ_NENHUM, t, x->cx, x->cy);
	centro(ia, t, &px, &py);
	static const int area[3][2] = { { F_LLUVIA, FL_QUIETO }, { F_HIELO, FL_HIELO }, { F_FURIA, FL_FURIA } };
	for (int q = 0; q < 3; ++q) {
		int f = area[q][0];
		if ((md = meio(ia, u, f, &lv)) == MEIO_NADA || !vale(ia, px, py, area[q][1], apuntado)) continue;
		if (lancar(ia, u, f, md, lv, t, px, py)) return 0;
		limpar_area(ia, u, f, px, py);
		return 1;
	}
	static const int raio[2] = { F_RAYO, F_BOLA };
	for (int q = 0; q < 2; ++q) {
		int f = raio[q];
		if ((md = meio(ia, u, f, &lv)) == MEIO_NADA || !(apuntado || x->vida >= VIDA_RAYO)) continue;
		return !lancar(ia, u, f, md, lv, t, px, py);
	}
	if (o->classe != TZC_MAGE || mira != u) return 0;
	/* el mago al que atacan se refugia o se aleja cinco casillas */
	TzId r = refugio(ia, u);
	if (r != TZ_NENHUM) return ia_ordem(ia, u, TZ_O_ENTRAR, r, ia->m->obj[r].cx, ia->m->obj[r].cy) ? 0 : 2;
	int fx, fy;
	longe(ia, o->cx, o->cy, x->cx, x->cy, &fx, &fy);
	return ia_ordem(ia, u, TZ_O_MOVER, TZ_NENHUM, fx, fy) ? 0 : 2;
}

/* cada mirada: las unidades del reino con maná (o con libros) que están atacando */
void ia_magias(TzIa *ia)
{
	for (TzId u = 0; u < ia->m->nobj; ++u) {
		const TzObj *o = &ia->m->obj[u];
		if (o->dono != ia->k || !ia_contado(o) || !o->ator || (o->flags & (TZ_F_DENTRO | TZ_F_HISTORIA))) continue;
		if (!o->mana_max && !o->ator->nitens) continue;
		if (u < ia->cappeao && ia->mandado[u] == ia->m->tic + 1) continue;
		TzId t = TZ_NENHUM;
		if (o->ator->ordem.tipo == TZ_O_ATACAR) t = o->ator->ordem.alvo;
		const TzObj *x = ia_obj(ia, t);
		if (!x || !ia_contado(x) || !ia_ve(ia, t)) continue;
		/* tras el hechizo, el ataque sigue (el original lanza antes del golpe y luego golpea) */
		if (antes_de_atacar(ia, u, t) == 1) ordem_fila(ia, u, TZ_O_ATACAR, 0, t, x->cx, x->cy, 1);
	}
}

/* 0x4ebe10: al salir un grupo, el ninja con 90 de maná que no está escondido se esconde; devuelve los que quedan para
 * la orden del grupo (los ninjas reciben la suya encolada tras el hechizo) */
int ia_avancar(TzIa *ia, TzId *ids, int n, int px, int py)
{
	int k = 0;
	int ord = tz_feitico_ordem(ia->m, chave(F_CAPA));
	for (int j = 0; j < n; ++j) {
		const TzObj *o = &ia->m->obj[ids[j]];
		if (ord >= 0 && o->classe == TZC_NINJA && o->mana >= MANA_NINJA && o->ator && o->ator->invisivel_ate <= ia->m->tic &&
		    !(o->flags & (TZ_F_HISTORIA | TZ_F_MORRENDO)) &&
		    !ordem_fila(ia, ids[j], TZ_O_HECHIZO, ord, ids[j], o->cx, o->cy, 0)) {
			ordem_fila(ia, ids[j], TZ_O_MOVER, 0, TZ_NENHUM, px, py, 1);
			ia->contas_d470.hechizos++;
			continue;
		}
		ids[k++] = ids[j];
	}
	return k;
}

/* ------------------------------------------------------------------ el sacerdote ocioso (0x414f10) */
static int aliado_dos(const TzIa *ia, int a, int b)
{
	return a == b || ((ia->m->reino[a].diplomacia[b] & TZ_DIP_ALIADO) && (ia->m->reino[b].diplomacia[a] & TZ_DIP_ALIADO));
}

/* 0x44e690: en el cuadrado de 15 alrededor del sacerdote, el más cercano que acepte el filtro */
enum { P_ALUNO, P_BENDECIR };
static TzId perto(const TzIa *ia, TzId i, int que)
{
	const TzObj *s = &ia->m->obj[i];
	TzId v[256];
	int n = tz_mundo_area(ia->m, s->cx - ALCANCE_SACERDOTE, s->cy - ALCANCE_SACERDOTE, s->cx + ALCANCE_SACERDOTE, s->cy + ALCANCE_SACERDOTE,
	                      TZ_FILTRO_UNIDADES, -1, v, 256);
	TzId melhor = TZ_NENHUM;
	uint32_t dm = 0;
	for (int q = 0; q < n; ++q) {
		TzId j = v[q];
		const TzObj *x = &ia->m->obj[j];
		if (!ia_contado(x) || (x->flags & TZ_F_DENTRO) || x->dono < 1 || x->dono > TZ_REINOS || !aliado_dos(ia, s->dono, x->dono)) continue;
		int r[4];
		ia_rect(ia, j, r);
		uint32_t d = ia_dist(s->cx, s->cy, r);
		if (melhor != TZ_NENHUM && d >= dm) continue;
		if (que == P_ALUNO) {
			if (x->civ != TZ_CIV_ASIA || x->nivel >= s->nivel || maquina(x->classe)) continue;
			if (x->classe != TZC_PEON && x->classe != TZC_MONK && x->classe != TZC_NINJA && x->classe != TZC_PRIEST) continue;
		} else {
			if (!ia_combate(x) || com_efeito(ia, x, TZ_EF_BENDICAO) || !ia_seguro(ia, x->cx, x->cy, 1, 1)) continue;
		}
		melhor = j; dm = d;
	}
	return melhor;
}

static int curar(TzIa *ia, TzId i)
{
	const TzObj *s = &ia->m->obj[i];
	TzId lv;
	int md = meio(ia, i, F_CURAR, &lv);
	if (md == MEIO_NADA) return 0;
	TzId melhor = TZ_NENHUM;
	uint32_t dm = 0;
	for (TzId j = 0; j < ia->m->nobj; ++j) {
		const TzObj *x = &ia->m->obj[j];
		if (x->dono != ia->k || !ia_contado(x) || !tz_classe_e_unidade(x->classe) || (x->flags & TZ_F_DENTRO)) continue;
		if (x->vida >= x->vida_max || maquina(x->classe)) continue;
		int r[4];
		ia_rect(ia, j, r);
		uint32_t d = ia_dist(s->cx, s->cy, r);
		if (melhor == TZ_NENHUM || d < dm) { melhor = j; dm = d; }
	}
	if (melhor == TZ_NENHUM || dm > ALCANCE_CURA) return 0;
	if (lancar(ia, i, F_CURAR, md, lv, melhor, ia->m->obj[melhor].cx, ia->m->obj[melhor].cy)) return 0;
	ia->contas_d470.curas++;
	return 1;
}

int ia_ocioso_sacerdote(TzIa *ia, TzId i)
{
	TzId lv;
	if (ia_rng(ia, 0, 1) == 0 && meio(ia, i, F_CURAR, &lv) != MEIO_NADA) return curar(ia, i);
	if (ia_rng(ia, 0, 1) != 0) return 0;
	int md;
	if ((md = meio(ia, i, F_ENSENAR, &lv)) != MEIO_NADA) {
		TzId j = perto(ia, i, P_ALUNO);
		return j != TZ_NENHUM && !lancar(ia, i, F_ENSENAR, md, lv, j, ia->m->obj[j].cx, ia->m->obj[j].cy);
	}
	if ((md = meio(ia, i, F_BENDECIR, &lv)) != MEIO_NADA) {
		TzId j = perto(ia, i, P_BENDECIR);
		return j != TZ_NENHUM && !lancar(ia, i, F_BENDECIR, md, lv, j, ia->m->obj[j].cx, ia->m->obj[j].cy);
	}
	return 0;
}

/* ------------------------------------------------------------------ metas con rutina propia (goals.rs) */
static const char *invocacao(int tipo, int *anda)
{
	*anda = tipo == 16;
	switch (tipo) {
	case 15: return "spell_spawnbat";
	case 16: return "spell_spawnstonegolem";
	case 22: return "spell_spawncarpet";
	case 30: return "spell_spawnjin";
	case 32: return "spell_spawndragon";
	}
	return NULL;
}

static int setor_dentro(const int16_t r[4], int sx, int sy) { return sx >= r[0] / 8 && sx <= r[2] / 8 && sy >= r[1] / 8 && sy <= r[3] / 8; }

static uint32_t hostis(const TzIaLugar *l)
{
	uint32_t n = 0;
	for (int c = 0; c < CAT_N; ++c) n += l->hostil.c[c].n;
	return n;
}

/* 0x409ab0: la criatura que hace un mago (ver goals.rs summon) */
int ia_invocar(TzIa *ia, int tipo)
{
	if (!ia->magos.n) return 0;
	int32_t c[4];
	ia_custo(ia, tipo, c);
	if (!ia_pagavel(ia, c, B_GUIAO, 1, 0)) return 0;
	for (int r = 0; r < 4; ++r) ia->disponivel[r] -= c[r];
	int anda;
	const char *k = invocacao(tipo, &anda);
	int ord = k ? tz_feitico_ordem(ia->m, k) : -1;
	if (ord < 0) return 0;
	TzId mago = TZ_NENHUM;
	uint32_t pm = 0;
	for (int j = 0; j < ia->magos.n; ++j) {
		TzId i = ia->magos.v[j];
		const TzObj *o = &ia->m->obj[i];
		if (!o->ator || o->ator->ordem.tipo != TZ_O_NADA || (o->flags & TZ_F_DENTRO)) continue;
		/* la investigación del hechizo (0x60a850): el menú y lo que pide */
		if (tz_mundo_pode(ia->m, ia->k, i, ia->m->cat->ord[ord].chave)) continue;
		for (int p = 0; p < ia->nlugares; ++p) {
			const TzIaLugar *l = &ia->lugar[p];
			if (!setor_dentro(l->rect, o->cx / 8, o->cy / 8)) continue;
			if (hostis(l)) break;
			if (mago == TZ_NENHUM || l->prioridade > pm) { mago = i; pm = l->prioridade; }
		}
	}
	if (mago == TZ_NENHUM) return 0;
	const TzObj *o = &ia->m->obj[mago];
	for (int d = 1; d <= ANEIS_INVOCAR; ++d)
		for (int y = o->cy - d; y <= o->cy + d; ++y)
			for (int x = o->cx - d; x <= o->cx + d; ++x) {
				if (x != o->cx - d && x != o->cx + d && y != o->cy - d && y != o->cy + d) continue;
				if (x < 0 || y < 0 || x >= ia->m->lado || y >= ia->m->lado || tz_mundo_em(ia->m, x, y) >= 0) continue;
				if (anda && !ia_ilha(ia, x, y)) continue;
				if (ordem_fila(ia, mago, TZ_O_HECHIZO, ord, TZ_NENHUM, x, y, 0)) return 0;
				ia->contas_d470.invocacoes++;
				return 1;
			}
	return 0;
}

/* 0x409d30: de las catedrales del reino, la más nueva primero, la primera casilla de los anillos alrededor (su
 * rectángulo crecido en una, cuatro anillos) donde puede estar una unidad de tierra en la isla de la catedral */
static TzId ancora(TzIa *ia, int *ox, int *oy)
{
	for (TzId b = ia->m->nobj - 1; b >= 0; --b) {
		const TzObj *o = &ia->m->obj[b];
		if (o->classe != TZC_CATHEDRAL || o->dono != ia->k || !ia_contado(o) || ia_obra(o)) continue;
		int r[4];
		ia_rect(ia, b, r);
		int ilha = ia_ilha_perto(ia, r[0] - 1, r[1] - 1, 2);
		for (int d = 1; d <= ANEIS_ANCORA + 1; ++d)
			for (int y = r[1] - d; y <= r[3] + d; ++y)
				for (int x = r[0] - d; x <= r[2] + d; ++x) {
					if (x != r[0] - d && x != r[2] + d && y != r[1] - d && y != r[3] + d) continue;
					if (tz_mundo_em(ia->m, x, y) >= 0 || !ia_ilha(ia, x, y) || (ilha && ia_ilha(ia, x, y) != ilha)) continue;
					*ox = x; *oy = y;
					return b;
				}
	}
	return TZ_NENHUM;
}

/* 0x409ed0: un reino europeo con castillo, catedral y sin cruzados, con la parte del guion para pagar y todos los
 * cruzados de la catedral reunidos, pone el ancla */
int ia_cruzada(TzIa *ia)
{
	if (ia->civ != TZ_CIV_EURO) return 0;
	uint32_t f, e;
	ia_registro(ia, ia->k, TZC_CASTLE, &f, &e);
	if (!f) return 0;
	ia_registro(ia, ia->k, TZC_CRUSADER, &f, &e);
	if (f) return 0;
	ia_registro(ia, ia->k, TZC_CATHEDRAL, &f, &e);
	if (!f) return 0;
	int32_t c[4];
	ia_custo(ia, IC_CRUSADEANCHOR, c);
	if (!ia_pagavel(ia, c, B_GUIAO, 1, 0)) return 0;
	const TzReinoEstado *r = &ia->m->reino[ia->k];
	if (r->cruzados == 0xffff || r->cruz_ancla >= 0) return 0;
	int teto = 16 + 4 * r->cruzadas_completas;
	if (teto > 36) teto = 36;
	if (r->cruzados < teto) return 0;
	int x, y;
	TzId cat = ancora(ia, &x, &y);
	if (cat == TZ_NENHUM) return 0;
	TzComando cm;
	memset(&cm, 0, sizeof cm);
	cm.tipo = TZ_C_CRUZADA;
	cm.x = (int16_t)x; cm.y = (int16_t)y;
	cm.nids = 1; cm.ids[0] = cat;
	if (ia_enviar(ia, &cm)) return 0;
	for (int q = 0; q < 4; ++q) ia->disponivel[q] -= c[q];
	ia->contas_d470.cruzadas++;
	return 1;
}

/* 0x409290: lo que vale un campesino para la yihad: uno más de lo que les falta a los luchadores propios para el mínimo
 * de cada lugar de su sector, por 3/2 ocioso, 2 atacando, 2/3 talando, cavando, minando u ordeñando; nada construyendo */
static uint32_t pontos_jihad(TzIa *ia, TzId i)
{
	if (!ia_livre(ia, i)) return 0;
	const TzObj *o = &ia->m->obj[i];
	uint32_t falta = 0;
	for (int p = 0; p < ia->nlugares; ++p) {
		const TzIaLugar *l = &ia->lugar[p];
		if (!setor_dentro(l->rect, o->cx / 8, o->cy / 8)) continue;
		uint32_t tem = l->amigo.c[CAT_MILITAR].forca;
		if (tem < l->minimo) falta += l->minimo - tem;
	}
	uint32_t s;
	switch (o->ator->ordem.tipo) {
	case TZ_O_CORTAR: case TZ_O_RECOLHER: s = falta * 2 / 3; break;
	case TZ_O_ATACAR: s = falta * 2; break;
	case TZ_O_NADA: s = falta * 3 / 2; break;
	case TZ_O_CONSTRUIR: return 0;
	default: s = falta;
	}
	return s + 1;
}

/* 0x409500: un campesino de un reino árabe se hace guerrero de la yihad: el de más puntos; uno hipnotizado se queda */
int ia_jihad(TzIa *ia)
{
	if (ia->civ != TZ_CIV_ARAB) return 0;
	TzId melhor = TZ_NENHUM;
	uint32_t pm = 0;
	for (int j = 0; j < ia->campones.n; ++j) {
		TzId i = ia->campones.v[j];
		uint32_t s = pontos_jihad(ia, i);
		if (s && (melhor == TZ_NENHUM || s > pm)) { melhor = i; pm = s; }
	}
	if (melhor == TZ_NENHUM) return 0;
	const TzObj *o = &ia->m->obj[melhor];
	if (!(o->ator->hipnose_ate && ia->m->tic < o->ator->hipnose_ate)) {
		TzComando c;
		memset(&c, 0, sizeof c);
		c.tipo = TZ_C_CONVERTIR;
		c.arg = TZC_JIHAD;
		c.nids = 1; c.ids[0] = melhor;
		if (!ia_enviar(ia, &c)) ia->contas_d470.jihad++;
	}
	return 1;
}

/* rules.rs convert (0x409771): sin un cambio en marcha, la taberna del reino cambia comida (quer x 10) por la piedra
 * o el oro, lo que más falta para lo reservado */
void ia_converter(TzIa *ia, uint32_t quer)
{
	TzId tav = TZ_NENHUM;
	for (TzId i = 0; i < ia->m->nobj; ++i) {
		const TzObj *o = &ia->m->obj[i];
		if (o->dono != ia->k || o->classe != TZC_TAVERN || !ia_contado(o) || !o->ator) continue;
		if (o->ator->troca.ativa) return;
		if (tav == TZ_NENHUM && !ia_obra(o)) tav = i;
	}
	if (tav == TZ_NENHUM) return;
	int32_t falta[2];
	for (int r = 0; r < 2; ++r) {
		int32_t reservado = 0;
		for (int b = 0; b < B_N; ++b) reservado += ia->baldes[b][r];
		falta[r] = reservado > ia->disponivel[r] ? reservado - ia->disponivel[r] : 0;
	}
	int para = falta[1] > falta[0];
	uint32_t pago = quer * 10;
	pago = (pago + 499) / 500 * 500;
	if (pago < 500) pago = 500;
	if (pago > 20000) pago = 20000;
	if ((int32_t)pago > ia->disponivel[TZ_COMIDA]) return;
	TzComando c;
	memset(&c, 0, sizeof c);
	c.tipo = TZ_C_TROCAR;
	c.arg = (int16_t)(TZ_COMIDA | (para << 2));
	c.valor = (int32_t)pago;
	c.nids = 1; c.ids[0] = tav;
	if (!ia_enviar(ia, &c)) ia->contas_d470.trocas++;
}
