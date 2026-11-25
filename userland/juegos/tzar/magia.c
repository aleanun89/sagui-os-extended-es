/* SPDX-License-Identifier: GPL-3.0-or-later */
/* magia.c - los hechizos de Tzar (D417, fase 5), simplificados.
 *
 * Traducido a C (simplificado) de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later), gameplay.rs:
 * spell_parameters (alcance y maná de cada hechizo, 0x4721e0/0x4723e0), summoned (las criaturas que se invocan y su
 * coste de maná de la tabla de la civilización), los efectos (effect::BLESS, FREEZE, STONE, BERSERK) y la lluvia de
 * fuego (40 a todo lo que hay alrededor de cada bola). Las cifras de daño y duración de los demás son aproximadas:
 * el rayo y la bola de fuego hieren como magia (sin armadura), la curación da 25 puntos, la bendición +3/+3 un minuto,
 * el hielo 20 s, la piedra 30 s, la furia +50 % de ataque 30 s, la visión mágica descubre 20 casillas.
 *
 * v8.5.5.2, también traducidos de gameplay.rs cast(): spell_curse (maldición: la mitad del ataque y nada de defensa, 30 s, a
 * lo hostil a la vista de quien lanza), spell_hypnotize (30 s bajo el reino de quien lanza), spell_cloak (invisibilidad 45 s,
 * 2 s del ninja), spell_whirlpool (remolino 3x3 en el agua, 20 de daño a los barcos cada 250 ms, se mueve cada 2 s, 15 veces),
 * spell_raisedead (esqueletos en los cuerpos a ocho casillas), spell_raiseexp (enseñar), los libros (lanzan sin maná) y los
 * espías (suplantar, revelar, el golpe mortal del ninja: quince veces su ataque con 60 de maná). Cifras de daño y duración
 * de los hechizos viejos, aproximadas como antes. Los parámetros de alcance y maná son los de spell_parameters de ReTzar.
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

/* world.rs spell_key: la tabla de hechizos del ejecutable */
const char *tz_feitico_chave(int idx)
{
	static const char *const t[21] = { "spell_firerain", "spell_lightning", "spell_heal", "spell_spawncarpet", "spell_spawnbat",
		"spell_bless", "spell_freeze", "spell_berserk", "spell_soulbind", "spell_cloak", "spell_spawnstonegolem",
		"spell_magicvision", "spell_raisedead", "spell_hypnotize", "spell_turntostone", "spell_whirlpool", "spell_spawndragon",
		"spell_flamebolt", "spell_raiseexp", "spell_spawnjin", "spell_curse" };
	return idx >= 0 && idx < 21 ? t[idx] : NULL;
}

int tz_feitico_ordem(const TzMundo *m, const char *k)
{
	if (!k) return -1;
	for (int q = 0; q < m->cat->nord; ++q)
		if (!strncmp(m->cat->ord[q].acao, "spell ", 6) && !strcmp(m->cat->ord[q].classe, k)) return q;
	return -1;
}

int tz_mundo_conhece_feitico(const TzMundo *m, TzId i, const char *k)
{
	const TzObj *o = tz_mundo_obj((TzMundo *)m, i);
	if (!o || !k) return 0;
	if (o->ator) for (int q = 0; q < o->ator->naprendidos; ++q) { const char *c = tz_feitico_chave(o->ator->aprendidos[q]); if (c && !strcmp(c, k)) return 1; }
	const TzOrdemDef *menu[160];
	int n = tz_catalogo_menu(m->cat, o->classe, o->civ, menu, 160);
	for (int q = 0; q < n; ++q) if (!strncmp(menu[q]->acao, "spell ", 6) && !strcmp(menu[q]->classe, k)) return 1;
	return 0;
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

/* gameplay.rs spell_parameters (0x4721e0 alcance, 0x4723e0 maná) */
static void parametros(const TzMundo *m, const TzObj *o, const char *k, int *alc, int *mana)
{
	int a = 8, mn = 50;
	if (!strcmp(k, "spell_firerain")) { a = 14; mn = 100; }
	else if (!strcmp(k, "spell_lightning") || !strcmp(k, "spell_freeze") || !strcmp(k, "spell_berserk")) a = 12;
	else if (!strcmp(k, "spell_heal")) a = tz_pesquisas(m, o->dono, "te_medicine") ? 5 : 3;
	else if (invocado(k)) a = !strcmp(k, "spell_spawnbat") ? 16 : 4;
	else if (!strcmp(k, "spell_whirlpool")) a = 16;
	else if (!strcmp(k, "spell_magicvision")) a = 1024;
	else if (!strcmp(k, "spell_turntostone")) a = 20;
	else if (!strcmp(k, "spell_flamebolt")) a = 18;
	else if (!strcmp(k, "spell_raiseexp")) a = 1;
	int c = invocado(k);
	if (c) { mn = tz_catalogo_at(m->cat, c, o->civ)->mana; if (mn < 1) mn = 1; }
	else if (!strcmp(k, "spell_freeze")) mn = 60;
	else if (!strcmp(k, "spell_heal")) mn = 4;
	else if (!strcmp(k, "spell_berserk") || !strcmp(k, "spell_raisedead")) mn = 70;
	else if (!strcmp(k, "spell_magicvision")) mn = 30;
	else if (!strcmp(k, "spell_raiseexp")) mn = 10;
	else if (!strcmp(k, "spell_cloak") && o->classe == TZC_NINJA) mn = 20;
	else if (!strcmp(k, "spell_curse") && o->classe == 31) mn = 0;
	*alc = a; *mana = mn;
}

/* D470: el alcance y el maná de un hechizo para la unidad (la IA decide con ellos) */
void tz_feitico_parametros(const TzMundo *m, TzId i, const char *k, int *alc, int *mana)
{
	const TzObj *o = tz_mundo_obj((TzMundo *)m, i);
	*alc = 0; *mana = 0;
	if (o && k) parametros(m, o, k, alc, mana);
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

/* spell_soulbind (0x473403): el objetivo sigue siendo de su dueño y una piedra del alma ligada a él aparece en el inventario de
 * quien lo lanza (si no cabe, no pasa nada) */
static void ligar_alma(TzMundo *m, TzId i, TzId j)
{
	TzObj *o = &m->obj[i];
	if (!o->ator || j == i || o->ator->nitens >= tz_mundo_capacidade_itens(m, i)) return;
	TzId g = tz_novo_obj(m, TZC_ITEM_ALMA, o->civ, 0, o->cx, o->cy, 0);
	if (g < 0) return;
	o = &m->obj[i];
	TzObj *s = &m->obj[g];
	s->item.ligado = j + 1;
	s->flags |= TZ_F_CARREGADO;
	s->dentro_de = i;
	o->ator->nitens++;
	tz_ev(m, TZ_EV_ITEM, o->dono, i, g, o->cx, o->cy, TZC_ITEM_ALMA, 1);
}

/* las piedras ligadas a una unidad que muere desaparecen (0x4e219f, 0x465440) */
void tz_almas_morrer(TzMundo *m, TzId i)
{
	for (int q = 0; q < m->nobj; ++q) {
		TzObj *s = &m->obj[q];
		if (s->classe == TZC_ITEM_ALMA && s->item.ligado == i + 1 && !(s->flags & TZ_F_INATIVO)) tz_item_gastar(m, q);
	}
}

/* órdenes de la piedra (0x465340): actúan sobre la unidad ligada, con la piedra llevada o en el suelo; quien la lleva da el golpe */
void tz_alma_comando(TzMundo *m, const TzComando *c)
{
	if (c->nids < 1 || c->ids[0] < 0 || c->ids[0] >= m->nobj) return;
	TzObj *s = &m->obj[c->ids[0]];
	if (s->classe != TZC_ITEM_ALMA || (s->flags & (TZ_F_INATIVO | TZ_F_MORTO))) return;
	TzId h = (s->flags & TZ_F_CARREGADO) ? s->dentro_de : TZ_NENHUM;
	if (h >= 0 && m->obj[h].dono != c->reino) { tz_falha("el inventario no es de tu reino"); return; }
	TzId b = s->item.ligado - 1;
	if (b < 0 || b >= m->nobj || !tz_obj_vivo(&m->obj[b])) { tz_falha("el alma ligada ya no existe"); return; }
	if (c->arg == 0) tz_dano(m, b, 10000, 3, h);
	else por_efeito(m, b, TZ_EF_FURIA, 30 * TZ_TICS_SEG);
}

static void ferir_area(TzMundo *m, TzId de, int cx, int cy, int raio, uint32_t dano)
{
	TzId v[64];
	int n = tz_mundo_area(m, cx - raio, cy - raio, cx + raio, cy + raio, TZ_FILTRO_UNIDADES | TZ_FILTRO_EDIFICIOS, -1, v, 64);
	for (int q = 0; q < n; ++q) tz_dano(m, v[q], dano, 3, de);
}

static int maquina(const TzObj *o)
{
	int c = o->classe;
	return c == 16 || (c >= 19 && c <= 25) || c == 31 || c == 36 || c == TZC_FISHING_BOAT;
}

/* spell_curse: lo hostil a la vista de quien lanza, alrededor del punto, 30 s (effect 20) */
static void maldicao(TzMundo *m, TzId i, int tx, int ty)
{
	int r = m->obj[i].visao ? m->obj[i].visao : 5, dono = m->obj[i].dono;
	TzId v[128];
	int n = tz_mundo_area(m, tx - r, ty - r, tx + r, ty + r, TZ_FILTRO_UNIDADES, -1, v, 128);
	for (int q = 0; q < n; ++q) {
		TzObj *t = &m->obj[v[q]];
		if (!tz_hostil(m, dono, v[q]) || maquina(t) || t->classe == TZC_MESSIAH || !t->ator) continue;
		t->ator->maldicao_ate = m->tic + 600;
		tz_recalcular(m, v[q]);
	}
}

/* spell_hypnotize: la unidad cambia de reino 30 s (effect 13); el Mesías no se deja */
static void hipnotizar(TzMundo *m, TzId i, TzId j)
{
	TzObj *t = &m->obj[j];
	if (!t->ator || !tz_obj_unidade(t) || t->classe == TZC_MESSIAH || t->dono == m->obj[i].dono) return;
	if (!(t->ator->hipnose_ate && m->tic < t->ator->hipnose_ate)) t->ator->dono_orig = t->dono;
	t->ator->hipnose_ate = m->tic + 600;
	tz_trocar_dono(m, j, m->obj[i].dono);
}

/* spell_whirlpool: 3x3 en el agua, centrado en el punto */
static int agua_3x3(const TzMundo *m, int x, int y)
{
	for (int dy = 0; dy < 3; ++dy) for (int dx = 0; dx < 3; ++dx) {
		int cx = x + dx, cy = y + dy;
		if (cx < 0 || cy < 0 || cx >= m->lado || cy >= m->lado || !(m->cel[cy * m->lado + cx].bits & TZ_CEL_AGUA)) return 0;
	}
	return 1;
}

static void remoinho(TzMundo *m, TzId i, int tx, int ty)
{
	int x = tx > 0 ? tx - 1 : 0, y = ty > 0 ? ty - 1 : 0;
	if (!agua_3x3(m, x, y)) { tz_falha("el remolino solo se forma en el agua"); return; }
	TzId w = tz_novo_obj(m, TZC_REDEMOINHO, m->obj[i].civ, 0, x, y, 0);
	if (w < 0 || !m->obj[w].ator) return;
	TzAtor *a = m->obj[w].ator;
	a->remoinho_dono = i; a->remoinho_mov = 15;
	a->remoinho_golpe = m->tic + 5;             /* 250 ms */
	a->remoinho_mueve = m->tic + 2 * TZ_TICS_SEG;   /* 2000 ms */
}

/* cada tic: el daño a los barcos (20 por 250 ms) y el movimiento al azar cada dos segundos; a la 15.ª vez se va */
void tz_remoinhos_tic(TzMundo *m)
{
	int n = m->nobj;
	for (int w = 0; w < n; ++w) {
		TzObj *o = &m->obj[w];
		if (o->classe != TZC_REDEMOINHO || !o->ator || (o->flags & (TZ_F_MORTO | TZ_F_INATIVO))) continue;
		TzAtor *a = o->ator;
		TzId dono = a->remoinho_dono >= 0 && a->remoinho_dono < m->nobj && tz_obj_vivo(&m->obj[a->remoinho_dono]) ? a->remoinho_dono : TZ_NENHUM;
		if (m->tic >= a->remoinho_golpe) {
			a->remoinho_golpe = m->tic + 5;
			TzId v[64];
			int nv = tz_mundo_area(m, o->cx, o->cy, o->cx + 2, o->cy + 2, TZ_FILTRO_UNIDADES, -1, v, 64);
			for (int q = 0; q < nv; ++q) if (tz_obj_at(m, &m->obj[v[q]])->naval) tz_dano(m, v[q], 20, 3, dono);
			o = &m->obj[w]; a = o->ator;
		}
		if (m->tic >= a->remoinho_mueve) {
			a->remoinho_mueve = m->tic + 2 * TZ_TICS_SEG;
			if (a->remoinho_mov) a->remoinho_mov--;
			if (!a->remoinho_mov) { o->flags |= TZ_F_INATIVO; continue; }
			int dx = tz_rng_entre(&m->rng, 0, 2) - 1, dy = tz_rng_entre(&m->rng, 0, 2) - 1;
			if (agua_3x3(m, o->cx + dx, o->cy + dy)) { o->cx = (int16_t)(o->cx + dx); o->cy = (int16_t)(o->cy + dy); o->px = o->cx * TZ_SUB + TZ_SUB / 2; o->py = o->cy * TZ_SUB + TZ_SUB / 2; }
		}
	}
}

/* spell_raisedead: un esqueleto en la casilla de cada cuerpo (unidad de tierra caída, no máquina ni criatura sin cuerpo) a
 * ocho casillas del que lanza; el cuerpo desaparece */
static int sin_cuerpo(int c) { return c == 15 || c == 16 || c == 17 || c == 22 || c == 27 || c == 28 || c == 30 || c == 31 || c == 32 || c == 36; }

static void alzar_mortos(TzMundo *m, TzId i)
{
	int cx = m->obj[i].cx, cy = m->obj[i].cy, dono = m->obj[i].dono, civ = m->obj[i].civ;
	int n = m->nobj;
	for (int q = 0; q < n; ++q) {
		TzObj *c = &m->obj[q];
		if (!(c->flags & TZ_F_MORRENDO) || (c->flags & TZ_F_MORTO) || !tz_obj_unidade(c) || maquina(c) || sin_cuerpo(c->classe) || c->classe == TZC_COW) continue;
		if (tz_obj_capa(m, c) != TZ_CAPA_TERRA) continue;
		int dx = c->cx - cx, dy = c->cy - cy;
		if (dx < -8 || dx > 8 || dy < -8 || dy > 8) continue;
		int x = c->cx, y = c->cy;
		if (!tz_cel_livre(m, TZ_CAPA_TERRA, dono, x, y)) continue;
		/* el cuerpo se va */
		c->flags = (c->flags & ~(uint32_t)TZ_F_MORRENDO) | TZ_F_MORTO;
		if (c->ator) { free(c->ator->caminho); free(c->ator); c->ator = NULL; }
		TzId e = tz_novo_obj(m, TZC_SKELETON, civ, dono, x, y, 0);
		if (e < 0) continue;
		m->obj[e].estado = TZ_E_APARECENDO; m->obj[e].anim_tic = m->tic;
		if (dono >= 1 && dono <= 8) m->reino[dono].criadas++;
		tz_ev(m, TZ_EV_CRIADO, dono, e, i, x, y, TZC_SKELETON, 1);
	}
}

/* ------------------------------------------------------------------ efectos por tic, espías */
/* caducan la maldición, la hipnosis y la invisibilidad; el espía recupera un punto de energía por segundo */
void tz_efeitos_tic(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	if (a->maldicao_ate && m->tic >= a->maldicao_ate) { a->maldicao_ate = 0; tz_recalcular(m, i); }
	if (a->invisivel_ate && m->tic >= a->invisivel_ate) a->invisivel_ate = 0;
	if (a->hipnose_ate && m->tic >= a->hipnose_ate) {
		a->hipnose_ate = 0;
		if (a->dono_orig != (uint32_t)o->dono && a->dono_orig <= 8) tz_trocar_dono(m, i, (int)a->dono_orig);
	}
	if (m->tic % TZ_TICS_SEG == (uint32_t)i % TZ_TICS_SEG) {
		if (o->classe == TZC_SPY && a->espia_energia < 100) a->espia_energia++;
		if (a->espia_reino && a->espia_energia < 100) a->espia_energia++;
	}
}

static int suplantable(const TzMundo *m, const TzObj *x)
{
	int c = x->classe;
	return tz_obj_vivo(x) && tz_obj_unidade(x) && tz_obj_capa(m, x) == TZ_CAPA_TERRA && !maquina(x) && c != TZC_NINJA && c != 26 && c != TZC_MESSIAH && c != TZC_COW &&
	       c != TZC_SPY && !(x->flags & TZ_F_HEROI) && x->ator && !x->ator->espia_reino;
}

/* quita al espía del mundo: se mete en j (suplantar) */
static void espia_entra(TzMundo *m, TzId i, TzId j)
{
	TzObj *s = &m->obj[i], *h = &m->obj[j];
	TzAtor *ha = h->ator;
	ha->espia_reino = s->dono; ha->espia_civ = s->civ; ha->espia_nivel = s->nivel; ha->espia_xp = (uint16_t)s->xp;
	ha->espia_vida = s->vida; ha->espia_vida_max = s->vida_max; ha->espia_heroi = (s->flags & TZ_F_HEROI) != 0;
	ha->espia_energia = 0;
	tz_marcar(m, i, 0);
	s->flags |= TZ_F_MORTO;
	if (s->ator) { free(s->ator->caminho); free(s->ator); s->ator = NULL; }
	tz_ev(m, TZ_EV_REMOVIDO, h->dono, i, j, h->cx, h->cy, TZC_SPY, 2);
}

void tz_espia_revelar(TzMundo *m, TzId j)
{
	TzObj *h = &m->obj[j];
	if (!h->ator || !h->ator->espia_reino) return;
	TzAtor *ha = h->ator;
	int dono = ha->espia_reino, civ = ha->espia_civ, x, y;
	int r[4] = { h->cx, h->cy, h->cx, h->cy };
	if (!tz_lugar_livre(m, TZ_CAPA_TERRA, dono, r, &x, &y)) return;
	uint8_t nivel = ha->espia_nivel, heroi = ha->espia_heroi, energia = ha->espia_energia;
	uint16_t xp = ha->espia_xp;
	int32_t vida = ha->espia_vida, vmax = ha->espia_vida_max;
	ha->espia_reino = 0; ha->espia_energia = 0;
	TzId e = tz_novo_obj(m, TZC_SPY, civ, dono, x, y, 0);
	if (e < 0) return;
	TzObj *s = &m->obj[e];
	s->nivel = nivel; s->xp = xp; s->vida = vida; s->vida_max = vmax;
	if (heroi) s->flags |= TZ_F_HEROI;
	if (s->ator) s->ator->espia_energia = energia;
	tz_recalcular(m, e);
	tz_ev(m, TZ_EV_CRIADO, dono, e, j, x, y, TZC_SPY, 5);
}

/* TZ_O_SUPLANTAR (op 0x60: junto a una unidad corriente, con 60 de energía, el espía se mete en ella; un anfitrión puede
 * pasar el espía a otra) y TZ_O_ASESINAR (op 0x5b: 60 de maná, todo el maná y un golpe de quince veces su ataque) */
void tz_espia_tic(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	TzId j = a->ordem.alvo;
	if (j < 0 || j >= m->nobj || j == i || !tz_obj_vivo(&m->obj[j])) { tz_proxima_ordem(m, i); return; }
	TzObj *t = &m->obj[j];
	if (a->ordem.tipo == TZ_O_SUPLANTAR && !suplantable(m, t)) { tz_proxima_ordem(m, i); return; }
	if (a->ordem.tipo == TZ_O_ASESINAR && (o->classe != TZC_NINJA || !tz_hostil(m, o->dono, j))) { tz_proxima_ordem(m, i); return; }
	int alc = a->ordem.tipo == TZ_O_ASESINAR && o->alcance ? o->alcance : 1;
	if (tz_obj_dist(m, o, t) > alc || a->passo < a->passo_total) {
		if (tz_mover_obj(m, i, j, alc) < 0) tz_proxima_ordem(m, i);
		return;
	}
	if (a->ordem.tipo == TZ_O_ASESINAR) {
		if (o->mana < 60) { tz_ev(m, TZ_EV_SEM_RECURSOS, o->dono, i, TZ_NENHUM, o->cx, o->cy, 4, 60); tz_proxima_ordem(m, i); return; }
		tz_olhar_obj(m, o, t);
		o->mana = 0;
		a->invisivel_ate = 0;
		uint32_t golpe = (uint32_t)o->ataque * 15;
		tz_ev(m, TZ_EV_ATACOU, o->dono, i, j, o->cx, o->cy, o->classe, 1);
		tz_dano(m, j, golpe > 65535 ? 65535 : golpe, 0, i);
		tz_proxima_ordem(m, i);
		return;
	}
	/* suplantar: hace falta energía (60) */
	int energia = a->espia_reino ? a->espia_energia : a->espia_energia;
	if ((o->classe != TZC_SPY && !a->espia_reino) || energia < 60) { tz_proxima_ordem(m, i); return; }
	if (a->espia_reino) {
		/* el anfitrión pasa el espía: el nuevo lo recibe con energía 0, el viejo lo suelta */
		TzAtor *ha = t->ator;
		ha->espia_reino = a->espia_reino; ha->espia_civ = a->espia_civ; ha->espia_nivel = a->espia_nivel; ha->espia_xp = a->espia_xp;
		ha->espia_vida = a->espia_vida; ha->espia_vida_max = a->espia_vida_max; ha->espia_heroi = a->espia_heroi; ha->espia_energia = 0;
		a->espia_reino = 0; a->espia_energia = 0;
		tz_proxima_ordem(m, i);
		return;
	}
	espia_entra(m, i, j);
}

void tz_hechizo_tic(TzMundo *m, TzId i)
{
	TzObj *o = &m->obj[i];
	TzAtor *a = o->ator;
	const char *k = chave(m, a->ordem.arg);
	if (!*k) { a->ordem.tipo = TZ_O_NADA; return; }
	int alc, mana;
	parametros(m, o, k, &alc, &mana);
	/* un libro lanza sin gastar maná (read_book); si ya no se lleva, el hechizo se pierde */
	if (a->livro >= 0 && (a->livro >= m->nobj || !(m->obj[a->livro].flags & TZ_F_CARREGADO) || m->obj[a->livro].dentro_de != i)) { a->livro = TZ_NENHUM; a->ordem.tipo = TZ_O_NADA; return; }
	int do_livro = a->livro >= 0;
	if (do_livro) mana = 0;
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
	/* lanzar descubre a quien estaba invisible (salvo la propia invisibilidad) */
	if (strcmp(k, "spell_cloak")) a->invisivel_ate = 0;
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
	else if (!strcmp(k, "spell_curse")) maldicao(m, i, tx, ty);
	else if (!strcmp(k, "spell_hypnotize") && j >= 0) hipnotizar(m, i, j);
	else if (!strcmp(k, "spell_cloak") && j >= 0) {
		TzObj *t = &m->obj[j];
		if (t->ator && tz_obj_unidade(t)) { t->ator->invisivel_ate = m->tic + (o->classe == TZC_NINJA ? 40u : 900u); tz_recalcular(m, j); }
	} else if (!strcmp(k, "spell_whirlpool")) remoinho(m, i, tx, ty);
	else if (!strcmp(k, "spell_soulbind") && j >= 0) ligar_alma(m, i, j);
	else if (!strcmp(k, "spell_raisedead")) alzar_mortos(m, i);
	else if (!strcmp(k, "spell_raiseexp") && j >= 0) {
		const TzObj *t = &m->obj[j];
		if (t->civ == TZ_CIV_ASIA && (t->classe == TZC_PEON || t->classe == TZC_MONK || t->classe == TZC_NINJA || t->classe == TZC_PRIEST) && t->nivel < o->nivel) tz_experiencia(m, j, 1, 0);
	}
	else if (c) {
		TzId n = tz_mundo_guiao_criar(m, c, o->civ, o->dono, tx, ty);
		if (n >= 0) { m->obj[n].estado = TZ_E_APARECENDO; m->obj[n].anim_tic = m->tic; }
		o = &m->obj[i];   /* crear un objeto puede mover m->obj */
	}
	o->estado = TZ_E_PARADO;
	o->anim_tic = m->tic;
	a->ordem.tipo = TZ_O_NADA;
	if (do_livro) {
		/* una carga menos; el libro sin cargas desaparece */
		TzId b = a->livro;
		a->livro = TZ_NENHUM;
		if (b >= 0 && m->obj[b].item.finito) {
			if (m->obj[b].item.cargas) m->obj[b].item.cargas--;
			if (!m->obj[b].item.cargas) tz_item_gastar(m, b);
		}
	}
}
