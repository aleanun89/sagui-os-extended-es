/* SPDX-License-Identifier: GPL-3.0-or-later */
/* magia.c - los hechizos de Tzar (D417, fase 5), simplificados.
 *
 * Traducido a C (simplificado) de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later), gameplay.rs:
 * spell_parameters (alcance y maná de cada hechizo, 0x4721e0/0x4723e0), summoned (las criaturas que se invocan y su
 * coste de maná de la tabla de la civilización), los efectos (effect::BLESS, FREEZE, STONE, BERSERK) y la lluvia de
 * fuego (40 a todo lo que hay alrededor de cada bola). Las cifras de daño y duración de los demás son aproximadas:
 * el rayo y la bola de fuego hieren como magia (sin armadura), la curación da 25 puntos, la bendición +3/+3 un minuto,
 * el hielo 20 s, la piedra 30 s, la furia +50 % de ataque 30 s, la visión mágica descubre 20 casillas.
 * Pendientes: remolino, maldición, alzar muertos, hipnosis, invisibilidad, subir experiencia.
 */
#include "mundo_int.h"
#include <stdlib.h>
#include <string.h>

/* la clave del hechizo («spell_heal») de la orden arg (índice en cat->ord) */
static const char *chave(const TzMundo *m, int arg)
{
	if (arg < 0 || arg >= m->cat->nord) return "";
	return m->cat->ord[arg].classe;
}

static int invocado(const char *k)
{
	if (!strcmp(k, "spell_spawnbat")) return 15;
	if (!strcmp(k, "spell_spawnstonegolem")) return 16;
	if (!strcmp(k, "spell_spawncarpet")) return 22;
	if (!strcmp(k, "spell_spawnjin")) return 30;
	if (!strcmp(k, "spell_spawndragon")) return 32;
	return 0;
}

/* gameplay.rs spell_parameters */
static void parametros(const TzMundo *m, const TzObj *o, const char *k, int *alc, int *mana)
{
	int a = 8, mn = 50;
	if (!strcmp(k, "spell_firerain")) { a = 14; mn = 100; }
	else if (!strcmp(k, "spell_lightning") || !strcmp(k, "spell_berserk")) a = 12;
	else if (!strcmp(k, "spell_freeze")) { a = 12; mn = 60; }
	else if (!strcmp(k, "spell_heal")) { a = tz_pesquisas(m, o->dono, "te_medicine") ? 5 : 3; mn = 4; }
	else if (invocado(k)) a = !strcmp(k, "spell_spawnbat") ? 16 : 4;
	else if (!strcmp(k, "spell_magicvision")) { a = 1024; mn = 30; }
	else if (!strcmp(k, "spell_turntostone")) a = 20;
	else if (!strcmp(k, "spell_flamebolt")) a = 18;
	int c = invocado(k);
	if (c) { mn = tz_catalogo_at(m->cat, c, o->civ)->mana; if (mn < 1) mn = 1; }
	*alc = a; *mana = mn;
}

int tz_efeito(const TzMundo *m, const TzObj *o, int ef)
{
	return o->ator && o->ator->efeito == ef && m->tic < o->ator->efeito_ate;
}

static void por_efeito(TzMundo *m, TzId j, int ef, uint32_t tics)
{
	TzObj *t = &m->obj[j];
	if (!t->ator || !tz_obj_unidade(t) || t->classe == TZC_MESSIAH) return;
	t->ator->efeito = (uint8_t)ef;
	t->ator->efeito_ate = m->tic + tics;
	tz_recalcular(m, j);
}

static void ferir_area(TzMundo *m, TzId de, int cx, int cy, int raio, uint32_t dano)
{
	TzId v[64];
	int n = tz_mundo_area(m, cx - raio, cy - raio, cx + raio, cy + raio, TZ_FILTRO_UNIDADES | TZ_FILTRO_EDIFICIOS, -1, v, 64);
	for (int q = 0; q < n; ++q) tz_dano(m, v[q], dano, 3, de);
}

void tz_hechizo_tic(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	const char *k = chave(m, a->ordem.arg);
	if (!*k) { a->ordem.tipo = TZ_O_NADA; return; }
	int alc, mana;
	parametros(m, o, k, &alc, &mana);
	TzId j = a->ordem.alvo;
	int tx = a->ordem.x, ty = a->ordem.y, r[4];
	if (j >= 0 && j < m->nobj) {
		if (!tz_obj_vivo(&m->obj[j])) { a->ordem.tipo = TZ_O_NADA; return; }
		tz_obj_ret(m, &m->obj[j], r);
		tx = (r[0] + r[2]) / 2; ty = (r[1] + r[3]) / 2;
	} else { r[0] = r[2] = tx; r[1] = r[3] = ty; }
	if (tz_dist_ret(o->cx, o->cy, r) > alc || (a->passo < a->passo_total)) {
		/* acercarse (el paso a mitad de casilla se termina primero) */
		if (tz_mover_para(m, i, r, alc) < 0) a->ordem.tipo = TZ_O_NADA;
		return;
	}
	if (o->mana < mana) {
		tz_ev(m, TZ_EV_SEM_RECURSOS, o->dono, i, TZ_NENHUM, o->cx, o->cy, 4, mana);
		a->ordem.tipo = TZ_O_NADA;
		return;
	}
	/* lanzar: la pose de hechizo, un segundo, y el efecto al final */
	if (o->estado != TZ_E_LANCANDO) { o->estado = TZ_E_LANCANDO; o->anim_tic = m->tic; a->trabalho_tic = TZ_TICS_SEG; return; }
	if (a->trabalho_tic > 1) { a->trabalho_tic--; return; }
	o->mana = (uint16_t)(o->mana - mana);
	if (tx != o->cx || ty != o->cy) o->direcao = o->direcao;   /* (la dirección la puso el acercamiento) */
	tz_ev(m, TZ_EV_HECHIZO, o->dono, i, j, tx, ty, o->classe, a->ordem.arg);
	int c = invocado(k);
	if (!strcmp(k, "spell_heal") && j >= 0) {
		TzObj *t = &m->obj[j];
		t->vida += 25;
		if (t->vida > t->vida_max) t->vida = t->vida_max;
	} else if (!strcmp(k, "spell_lightning") && j >= 0) tz_dano(m, j, 60, 3, i);
	else if (!strcmp(k, "spell_flamebolt") && j >= 0) tz_dano(m, j, 50, 3, i);
	else if (!strcmp(k, "spell_firerain")) {
		/* cinco bolas: en el punto y alrededor, 40 a todo lo que hay a una casilla de cada una */
		static const int d[5][2] = { { 0, 0 }, { -2, -1 }, { 2, 1 }, { -1, 2 }, { 1, -2 } };
		for (int q = 0; q < 5; ++q) ferir_area(m, i, tx + d[q][0], ty + d[q][1], 1, 40);
	} else if (!strcmp(k, "spell_freeze") && j >= 0) por_efeito(m, j, TZ_EF_GELO, 20 * TZ_TICS_SEG);
	else if (!strcmp(k, "spell_turntostone") && j >= 0) por_efeito(m, j, TZ_EF_PEDRA, 30 * TZ_TICS_SEG);
	else if (!strcmp(k, "spell_bless") && j >= 0) por_efeito(m, j, TZ_EF_BENDICAO, 60 * TZ_TICS_SEG);
	else if (!strcmp(k, "spell_berserk") && j >= 0) por_efeito(m, j, TZ_EF_FURIA, 30 * TZ_TICS_SEG);
	else if (!strcmp(k, "spell_magicvision")) tz_mundo_guiao_revelar(m, o->dono, tx, ty, 20);
	else if (c) {
		TzId n = tz_mundo_guiao_criar(m, c, o->civ, o->dono, tx, ty);
		if (n >= 0) { m->obj[n].estado = TZ_E_APARECENDO; m->obj[n].anim_tic = m->tic; }
		o = &m->obj[i];   /* crear un objeto puede mover m->obj */
	}
	o->estado = TZ_E_PARADO;
	o->anim_tic = m->tic;
	a->ordem.tipo = TZ_O_NADA;
}
