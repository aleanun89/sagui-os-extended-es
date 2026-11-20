/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ia_jogador.c - un jugador de la IA de Tzar: su gancho en el tic, la mirada de cada periodo, la casa que guarda, las
 * listas de sus objetos, las cinco fases por turno, la orden «AI», guardar y cargar, y el pequeño evaluador de EXTERN
 * (D417, fase 6).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): src/gameplay/ai.rs (Player,
 * prepare, command, no_attack_time) y src/gameplay/ai/think.rs (periodo 0x5aa0e8, casa 0x407690, listas 0x407110,
 * 0x40c720, 0x40c810, 0x40be30, fases 0x408d80, mirada 0x407c20).
 * Diferencias: las listas se rehacen en cada mirada con lo que el reino tiene y ve (el original las lleva al día con
 * ganchos del motor), y la mirada corre en el gancho del reino cada periodo en vez de en la cola de mensajes
 * (docs/tzar.md, «La IA (fase 6)»).
 */
#include "ia_int.h"
#include <ctype.h>
#include <stdio.h>

TzIaExterno tz_ia_externo = tz_ia_expr;
int tz_ia_depurar;

/* ------------------------------------------------------------------ utilidades */
int ia_meu(const TzIa *ia, TzId i)
{
	const TzObj *o = ia_obj(ia, i);
	return o && o->dono == ia->k && ia_contado(o);
}

int ia_ve(const TzIa *ia, TzId i)
{
	const TzObj *o = ia_obj(ia, i);
	if (!o) return 0;
	if (o->dono == ia->k) return ia_contado(o);
	return tz_mundo_ve(ia->m, ia->k, i);
}

void ia_lista_por(TzIaLista *l, TzId i)
{
	if (l->n >= l->cap) {
		int c = l->cap ? l->cap * 2 : 16;
		TzId *v = realloc(l->v, (size_t)c * sizeof *v);
		if (!v) return;
		l->v = v;
		l->cap = c;
	}
	l->v[l->n++] = i;
}

void ia_lista_tira(TzIaLista *l, TzId i)
{
	for (int j = 0; j < l->n; ++j)
		if (l->v[j] == i) { memmove(l->v + j, l->v + j + 1, (size_t)(l->n - j - 1) * sizeof *l->v); l->n--; return; }
}

int ia_lista_tem(const TzIaLista *l, TzId i)
{
	for (int j = 0; j < l->n; ++j) if (l->v[j] == i) return 1;
	return 0;
}

void ia_pegada(const TzIa *ia, int classe, int *w, int *h)
{
	*w = *h = 1;
	if (!ia->m->cat || classe <= 0 || classe >= TZ_NCLASSES) return;
	const TzAtributos *a = tz_catalogo_at(ia->m->cat, classe, ia->civ);
	if (a && a->pegada[0] && a->pegada[1]) { *w = a->pegada[0]; *h = a->pegada[1]; }
}

void ia_rect(const TzIa *ia, TzId i, int r[4])
{
	const TzObj *o = ia_obj(ia, i);
	if (!o) { r[0] = r[1] = r[2] = r[3] = 0; return; }
	int w = 1, h = 1;
	if (ia->m->cat && !tz_classe_e_unidade(o->classe)) {
		const TzAtributos *a = tz_catalogo_at(ia->m->cat, o->classe, o->civ);
		if (a && a->pegada[0] && a->pegada[1]) { w = a->pegada[0]; h = a->pegada[1]; }
	}
	r[0] = o->cx; r[1] = o->cy; r[2] = o->cx + w - 1; r[3] = o->cy + h - 1;
}

/* records.rs gap (0x401ad0): las casillas vacías entre un punto y un rectángulo */
uint32_t ia_dist(int px, int py, const int r[4])
{
	int dx = px < r[0] ? r[0] - px - 1 : px - r[2] - 1;
	int dy = py < r[1] ? r[1] - py - 1 : py - r[3] - 1;
	int d = dx > dy ? dx : dy;
	return d > 0 ? (uint32_t)d : 0;
}

/* build.rs rect_gap */
uint32_t ia_dist_rect(const int a[4], const int b[4])
{
	int dx = a[2] < b[0] ? b[0] - a[2] - 1 : (b[2] < a[0] ? a[0] - b[2] - 1 : 0);
	int dy = a[3] < b[1] ? b[1] - a[3] - 1 : (b[3] < a[1] ? a[1] - b[3] - 1 : 0);
	int d = dx > dy ? dx : dy;
	return d > 0 ? (uint32_t)d : 0;
}

/* selection.rs combat: unidad que pelea */
int ia_combate(const TzObj *o)
{
	if (!tz_classe_e_unidade(o->classe)) return 0;
	switch (o->classe) {
	case TZC_PEON: case TZC_TRADER: case 22: case TZC_COW: case 112: case 119: case TZC_FISHING_BOAT: return 0;
	}
	return 1;
}

/* gameplay.rs military: ataca y es unidad de combate o torre terminada */
int ia_militar(const TzIa *ia, const TzObj *o)
{
	if (!o->ataque) return 0;
	if (tz_classe_e_unidade(o->classe))
		return !(o->classe == TZC_PEON || o->classe == TZC_TRADER || o->classe == TZC_SPY || o->classe == TZC_COW ||
		         o->classe == TZC_FISHING_BOAT);
	return o->classe >= 39 && o->classe <= 43 && !ia_obra(o);
}

/* 0x401fa0: ataque + defensa + alcance / 5 + vida máxima / 20, el doble en las torres */
uint32_t ia_forca(const TzObj *o)
{
	uint32_t s = (uint32_t)o->ataque + o->defesa + o->alcance / 5u + (uint32_t)(o->vida_max > 0 ? o->vida_max : 0) / 20u;
	if (o->classe >= 39 && o->classe <= 43) s *= 2;
	return s;
}

int ia_ilha(const TzIa *ia, int x, int y)
{
	if (x < 0 || y < 0 || x >= ia->m->lado || y >= ia->m->lado || !ia->m->cel) return 0;
	const TzCelula *c = &ia->m->cel[y * ia->m->lado + x];
	if (c->bits & TZ_CEL_AGUA && !(c->bits & TZ_CEL_PONTE)) return 0;
	return c->ilha;
}

/* 0x417640 por tierra: alguna casilla del rectángulo está en la región de la de salida */
int ia_alcanca(const TzIa *ia, int fx, int fy, const int r[4])
{
	int de = ia_ilha(ia, fx, fy);
	if (!de) {
		/* la unidad está sobre algo (obra, puerta): vale la región de una casilla vecina */
		for (int dy = -1; dy <= 1 && !de; ++dy)
			for (int dx = -1; dx <= 1 && !de; ++dx) de = ia_ilha(ia, fx + dx, fy + dy);
		if (!de) return 1;
	}
	for (int y = r[1]; y <= r[3]; ++y)
		for (int x = r[0]; x <= r[2]; ++x)
			if (ia_ilha(ia, x, y) == de) return 1;
	return 0;
}

int ia_rng(TzIa *ia, int lo, int hi) { return tz_rng_entre(&ia->rng, lo, hi); }

/* ------------------------------------------------------------------ comandos */
static void marcar(TzIa *ia, TzId i)
{
	if (i >= 0 && i < ia->cappeao) ia->mandado[i] = ia->m->tic + 1;
	if (i >= 0 && i < ia->cappeao) ia->ocioso[i] = 0;
}

static int enviar(TzIa *ia, TzComando *c)
{
	c->reino = (uint8_t)ia->k;
	if (tz_mundo_comando(ia->m, c)) return -1;
	ia->contas.comandos++;
	for (int j = 0; j < c->nids; ++j) marcar(ia, c->ids[j]);
	return 0;
}

int ia_ordem_grupo(TzIa *ia, const TzId *ids, int n, int tipo, TzId alvo, int x, int y)
{
	int r = 0;
	for (int j = 0; j < n; j += TZ_CMD_IDS) {
		TzComando c;
		memset(&c, 0, sizeof c);
		c.tipo = TZ_C_ORDEM;
		c.ordem.tipo = (uint8_t)tipo;
		c.ordem.alvo = alvo;
		c.ordem.x = (int16_t)x;
		c.ordem.y = (int16_t)y;
		c.nids = n - j < TZ_CMD_IDS ? n - j : TZ_CMD_IDS;
		memcpy(c.ids, ids + j, (size_t)c.nids * sizeof *ids);
		if (enviar(ia, &c)) r = -1;
	}
	return r;
}

int ia_ordem(TzIa *ia, TzId id, int tipo, TzId alvo, int x, int y)
{
	if (ia_ordem_grupo(ia, &id, 1, tipo, alvo, x, y)) return -1;
	const TzObj *o = ia_obj(ia, id);
	if (o && o->classe == TZC_PEON) ia_reclassificar(ia, id, tipo, alvo, x, y);
	return 0;
}

int ia_construir(TzIa *ia, TzId peao, int classe, int x, int y)
{
	TzComando c;
	memset(&c, 0, sizeof c);
	c.tipo = TZ_C_CONSTRUIR;
	c.arg = (int16_t)classe;
	c.x = (int16_t)x;
	c.y = (int16_t)y;
	c.nids = 1;
	c.ids[0] = peao;
	if (enviar(ia, &c)) return -1;
	ia->contas.construidos++;
	/* el constructor va a una obra que aún no existe: cuenta como constructor de esa casilla */
	ia_reclassificar(ia, peao, TZ_O_CONSTRUIR, TZ_NENHUM, x, y);
	return 0;
}

int ia_produzir(TzIa *ia, TzId edif, int classe)
{
	TzComando c;
	memset(&c, 0, sizeof c);
	c.tipo = TZ_C_PRODUZIR;
	c.arg = (int16_t)classe;
	c.nids = 1;
	c.ids[0] = edif;
	if (enviar(ia, &c)) return -1;
	ia->contas.treinados++;
	return 0;
}

int ia_pesquisar(TzIa *ia, TzId edif, int tec)
{
	TzComando c;
	memset(&c, 0, sizeof c);
	c.tipo = TZ_C_PESQUISAR;
	c.arg = (int16_t)tec;
	c.nids = 1;
	c.ids[0] = edif;
	if (enviar(ia, &c)) return -1;
	ia->contas.pesquisas++;
	return 0;
}

/* rules.rs idle_building: un edificio sin nada que hacer (ni ya mandado en este tic) */
int ia_edificio_ocioso(const TzIa *ia, TzId i)
{
	const TzObj *o = ia_obj(ia, i);
	if (!o || !o->ator) return 0;
	if (i < ia->cappeao && ia->mandado[i] == ia->m->tic + 1) return 0;
	return o->ator->ntrab == 0 && o->ator->ordem.tipo == TZ_O_NADA;
}

/* ------------------------------------------------------------------ campesinos y grupos (think.rs) */
static void sair_grupo(TzIa *ia, TzId i)
{
	if (i < 0 || i >= ia->cappeao) return;
	TzIaPeao *p = &ia->peao[i];
	if (p->grupo < G_N) ia_lista_tira(&ia->grupo[p->grupo], i);
	p->grupo = 0xff;
	p->campo = 0;
	p->alvo = TZ_NENHUM;
}

static int granjeiros(const TzIa *ia, TzId campo)
{
	int n = 0;
	for (int j = 0; j < ia->grupo[G_COMIDA].n; ++j) {
		TzId b = ia->grupo[G_COMIDA].v[j];
		if (ia->peao[b].campo && ia->peao[b].alvo == campo) n++;
	}
	return n;
}

/* 0x40be30: un campesino ordenado por su programa (aquí, por su orden) */
void ia_reclassificar(TzIa *ia, TzId i, int tipo, TzId alvo, int x, int y)
{
	if (i < 0 || i >= ia->cappeao) return;
	sair_grupo(ia, i);
	const TzObj *o = &ia->m->obj[i];
	TzIaPeao p;
	p.grupo = 0xff; p.campo = 0; p.alvo = TZ_NENHUM; p.x = o->cx; p.y = o->cy;
	const TzObj *t = ia_obj(ia, alvo);
	switch (tipo) {
	case TZ_O_NADA: case TZ_O_PARAR:
		p.grupo = G_OCIOSO;
		break;
	case TZ_O_CONSTRUIR:
		if (alvo == TZ_NENHUM) { p.grupo = G_CONSTRUTOR; p.x = (int16_t)x; p.y = (int16_t)y; }
		else if (t && ia_contado(t) && ia_obra(t)) { p.grupo = G_CONSTRUTOR; p.alvo = alvo; p.x = t->cx; p.y = t->cy; }
		else p.grupo = G_OCIOSO;
		break;
	case TZ_O_CORTAR:
		p.grupo = G_MADEIRA; p.x = (int16_t)x; p.y = (int16_t)y;
		break;
	case TZ_O_RECOLHER:
		if (!t) break;
		p.alvo = alvo; p.x = t->cx; p.y = t->cy;
		if (t->classe == TZC_FIELD) {
			if (granjeiros(ia, alvo) < 1) { p.grupo = G_COMIDA; p.campo = 1; }
			else { p.grupo = G_OCIOSO; p.alvo = TZ_NENHUM; p.x = o->cx; p.y = o->cy; }
		} else if (t->classe == TZC_RES_GOLD) p.grupo = G_OURO;
		else if (t->classe == TZC_RES_STONE) p.grupo = G_PEDRA;
		else if (t->classe == TZC_COW) p.grupo = G_ORDENHA;
		break;
	}
	if (p.grupo < G_N) {
		ia_lista_por(&ia->grupo[p.grupo], i);
		ia->peao[i] = p;
	}
}

/* de la orden actual del campesino; volviendo con la carga sigue en su grupo (su origen) */
static void classificar_campones(TzIa *ia, TzId i)
{
	const TzObj *o = &ia->m->obj[i];
	const TzAtor *a = o->ator;
	if (!a) { ia_reclassificar(ia, i, TZ_O_NADA, TZ_NENHUM, 0, 0); return; }
	const TzOrdem *d = &a->ordem;
	if (d->tipo == TZ_O_ENTREGAR) {
		const TzObj *de = ia_obj(ia, a->origem);
		if (de && ia_contado(de)) { ia_reclassificar(ia, i, TZ_O_RECOLHER, a->origem, de->cx, de->cy); return; }
		if (a->carga_tipo == TZ_MADEIRA) { ia_reclassificar(ia, i, TZ_O_CORTAR, TZ_NENHUM, o->cx, o->cy); return; }
		sair_grupo(ia, i);
		return;
	}
	ia_reclassificar(ia, i, d->tipo, d->alvo, d->x, d->y);
}

static void listas_limpar(TzIa *ia)
{
	TzIaLista *ls[] = { &ia->campones, &ia->castelos, &ia->obras, &ia->magos, &ia->armazens, &ia->serrarias,
	                    &ia->granjas, &ia->minas, &ia->campos, &ia->torres, &ia->postos, &ia->oficinas, &ia->bonecos,
	                    &ia->exercito, &ia->quarteis };
	for (unsigned j = 0; j < sizeof ls / sizeof *ls; ++j) ls[j]->n = 0;
	for (int g = 0; g < G_N; ++g) ia->grupo[g].n = 0;
}

static int crescer_ate(TzIa *ia, int n)
{
	if (n <= ia->cappeao) return 0;
	int c = ia->cappeao ? ia->cappeao : 256;
	while (c < n) c *= 2;
	TzIaPeao *p = realloc(ia->peao, (size_t)c * sizeof *p);
	if (!p) return -1;
	ia->peao = p;
	uint32_t *o = realloc(ia->ocioso, (size_t)c * sizeof *o);
	if (!o) return -1;
	ia->ocioso = o;
	uint32_t *md = realloc(ia->mandado, (size_t)c * sizeof *md);
	if (!md) return -1;
	ia->mandado = md;
	for (int i = ia->cappeao; i < c; ++i) {
		ia->peao[i].grupo = 0xff; ia->peao[i].campo = 0; ia->peao[i].alvo = TZ_NENHUM;
		ia->ocioso[i] = 0; ia->mandado[i] = 0;
	}
	ia->cappeao = c;
	return 0;
}

static int crescer(TzIa *ia) { return crescer_ate(ia, ia->m->nobj); }

/* las minas conocidas, la más cercana a la casa primero (0x40db60) */
static void listar_minas(TzIa *ia)
{
	for (TzId i = ia->m->nobj - 1; i >= 0; --i) {
		const TzObj *o = &ia->m->obj[i];
		if ((o->classe != TZC_RES_GOLD && o->classe != TZC_RES_STONE) || !ia_contado(o) || !ia_ve(ia, i)) continue;
		int r[4];
		ia_rect(ia, i, r);
		uint32_t d = ia_dist(ia->casa[0], ia->casa[1], r);
		int at = ia->minas.n;
		while (at > 0) {
			int q[4];
			ia_rect(ia, ia->minas.v[at - 1], q);
			if (ia_dist(ia->casa[0], ia->casa[1], q) <= d) break;
			at--;
		}
		ia_lista_por(&ia->minas, i);
		memmove(ia->minas.v + at + 1, ia->minas.v + at, (size_t)(ia->minas.n - 1 - at) * sizeof *ia->minas.v);
		ia->minas.v[at] = i;
	}
}

/* 0x407110 y 0x40c810: las listas del reino, de sus objetos de ahora */
static void listas(TzIa *ia)
{
	listas_limpar(ia);
	TzMundo *m = ia->m;
	for (int i = 0; i < ia->cappeao; ++i) { ia->peao[i].grupo = 0xff; ia->peao[i].campo = 0; ia->peao[i].alvo = TZ_NENHUM; }
	/* los campos primero (los granjeros se cuentan sobre ellos) */
	for (TzId i = m->nobj - 1; i >= 0; --i)
		if (m->obj[i].classe == TZC_FIELD && ia_meu(ia, i)) ia_lista_por(&ia->campos, i);
	listar_minas(ia);
	for (TzId i = m->nobj - 1; i >= 0; --i) {
		const TzObj *o = &m->obj[i];
		if (o->dono != ia->k || !ia_contado(o) || (o->flags & TZ_F_DENTRO)) continue;
		switch (o->classe) {
		case TZC_PEON:
			ia_lista_por(&ia->campones, i);
			/* el campesino que explora no está en ningún grupo (lo nuestro: el original no explora) */
			if (i != ia->explorador) classificar_campones(ia, i);
			continue;
		case TZC_CASTLE: ia_lista_por(&ia->castelos, i); continue;
		case TZC_LUMBER: ia_lista_por(&ia->serrarias, i); continue;
		case TZC_FARM: ia_lista_por(&ia->granjas, i); continue;
		case TZC_STORAGE: ia_lista_por(&ia->armazens, i); continue;
		case IC_OUTPOST: ia_lista_por(&ia->postos, i); continue;
		case IC_WORKSHOP: ia_lista_por(&ia->oficinas, i); continue;
		case 133: ia_lista_por(&ia->bonecos, i); continue;
		case TZC_BARRACK: ia_lista_por(&ia->quarteis, i); continue;
		case IC_MAGE: ia_lista_por(&ia->magos, i); break;
		}
		if (tz_classe_e_unidade(o->classe)) { if (ia_combate(o)) ia_lista_por(&ia->exercito, i); }
		else if (o->classe >= 39 && o->classe <= 43) ia_lista_por(&ia->torres, i);
	}
	/* las obras que nadie construye (+0x9ca) */
	for (TzId i = m->nobj - 1; i >= 0; --i) {
		const TzObj *o = &m->obj[i];
		if (o->dono != ia->k || !ia_contado(o) || !ia_obra(o) || o->classe == TZC_FIELD) continue;
		int ninguem = 1;
		for (int j = 0; j < ia->grupo[G_CONSTRUTOR].n && ninguem; ++j)
			if (ia->peao[ia->grupo[G_CONSTRUTOR].v[j]].alvo == i) ninguem = 0;
		if (ninguem) ia_lista_por(&ia->obras, i);
	}
	/* los aserraderos y castillos sin árboles que ya no existen */
	for (int j = ia->nus.n - 1; j >= 0; --j)
		if (!ia_meu(ia, ia->nus.v[j])) ia_lista_tira(&ia->nus, ia->nus.v[j]);
}

/* lo que cambió desde la mirada anterior: las reglas que lo leen se evalúan otra vez (los ganchos 0x408670,
 * 0x408730 y 0x41eed0 del original) */
static void recontar(TzIa *ia)
{
	int nt = ia->b->ntipos;
	uint32_t *agora = calloc((size_t)nt * 2, sizeof *agora);
	if (!agora) return;
	TzMundo *m = ia->m;
	for (TzId i = 0; i < m->nobj; ++i) {
		const TzObj *o = &m->obj[i];
		if (o->dono != ia->k || !ia_contado(o)) continue;
		if (o->classe < nt) {
			agora[o->classe * 2]++;
			if (tz_classe_e_edificio(o->classe) && ia_obra(o)) agora[o->classe * 2 + 1]++;
		}
		if (o->ator && o->ator->ntrab > 0 && o->ator->trab[0].tipo == TZ_J_TREINAR && o->ator->trab[0].arg > 0 &&
		    o->ator->trab[0].arg < nt) agora[o->ator->trab[0].arg * 2 + 1]++;
	}
	int unidades = 0;
	for (int t = 1; t < nt && t < TZ_NCLASSES; ++t)
		if (agora[t * 2] != ia->conta_ant[t * 2] || agora[t * 2 + 1] != ia->conta_ant[t * 2 + 1]) {
			ia_tocar(ia, V_TIPO, t);
			if (tz_classe_e_unidade(t)) unidades = 1;
		}
	if (unidades) ia_tocar(ia, V_TIPO, 0);
	/* sin castillo ni aserradero, se vuelve a reservar el aserradero (0x408730) */
	int antes = ia->conta_ant[TZC_CASTLE * 2] + ia->conta_ant[TZC_LUMBER * 2];
	int depois = agora[TZC_CASTLE * 2] + agora[TZC_LUMBER * 2];
	if (antes && !depois) ia_reserva_inicial(ia, 1);
	memcpy(ia->conta_ant, agora, (size_t)nt * 2 * sizeof *agora);
	free(agora);
	/* las investigaciones hechas */
	if (m->cat && m->reino[ia->k].pesquisas) {
		for (int t = ia->b->tecnologias; t < nt; ++t) {
			int tec = ia_tec(ia, t);
			if (tec < 0 || tec >= m->cat->ntec) continue;
			uint8_t v = m->reino[ia->k].pesquisas[tec];
			if (v != ia->pesq_ant[tec]) { ia->pesq_ant[tec] = v; ia_tocar(ia, V_TIPO, t); }
		}
	}
}

/* ------------------------------------------------------------------ la casa (0x407690) */
/* los puentes, muros, puertas y campos del reino no hacen casa (en los mapas hay puentes con dueño lejos de todo) */
static int edificio_casa(const TzObj *o)
{
	return tz_classe_e_edificio(o->classe) && o->classe != TZC_BRIDGE && o->classe != TZC_FIELD && o->classe != TZC_WALL_STONE &&
	       o->classe != TZC_WALL_WOOD && o->classe != TZC_GATE_WOOD && o->classe != TZC_GATE_STONE;
}

static void achar_casa(TzIa *ia)
{
	ia->sem_casa = 1;
	TzMundo *m = ia->m;
	uint32_t sx = 0, sy = 0, ned = 0, nun = 0;
	int castelos = 0;
	TzId castelo = TZ_NENHUM;
	for (TzId i = m->nobj - 1; i >= 0; --i) {
		const TzObj *o = &m->obj[i];
		if (o->dono != ia->k || !ia_contado(o) || (o->flags & TZ_F_DENTRO)) continue;
		if (edificio_casa(o)) {
			sx += (uint32_t)o->cx; sy += (uint32_t)o->cy; ned++;
			if (o->classe == TZC_CASTLE) { castelos++; castelo = i; }
		} else if (tz_classe_e_unidade(o->classe)) nun++;
	}
	if (!ned && !nun) return;
	if (castelos > 1) {
		int mx = (int)(uint16_t)(sx / ned), my = (int)(uint16_t)(sy / ned);
		uint32_t melhor = 0;
		castelo = TZ_NENHUM;
		for (TzId i = m->nobj - 1; i >= 0; --i) {
			const TzObj *o = &m->obj[i];
			if (o->dono != ia->k || o->classe != TZC_CASTLE || !ia_contado(o)) continue;
			int r[4];
			ia_rect(ia, i, r);
			uint32_t d = ia_dist(mx, my, r);
			if (castelo == TZ_NENHUM || d < melhor) { castelo = i; melhor = d; }
		}
	}
	if (castelo != TZ_NENHUM) {
		int r[4];
		ia_rect(ia, castelo, r);
		ia->casa[0] = (int16_t)(r[0] + (r[2] - r[0] + 1) / 2);
		ia->casa[1] = (int16_t)(r[1] + (r[3] - r[1] + 1) / 2);
		ia->sem_casa = 0;
	} else {
		uint32_t total = ned + nun;
		for (TzId i = m->nobj - 1; i >= 0; --i) {
			const TzObj *o = &m->obj[i];
			if (o->dono == ia->k && ia_contado(o) && !(o->flags & TZ_F_DENTRO) && tz_classe_e_unidade(o->classe)) {
				sx += (uint32_t)o->cx; sy += (uint32_t)o->cy;
			}
		}
		int mx = (int)(uint16_t)(sx / total), my = (int)(uint16_t)(sy / total);
		int edificios = ned != 0;
		TzId perto = TZ_NENHUM;
		uint32_t melhor = 0;
		for (TzId i = m->nobj - 1; i >= 0; --i) {
			const TzObj *o = &m->obj[i];
			if (o->dono != ia->k || !ia_contado(o) || (o->flags & TZ_F_DENTRO)) continue;
			if (edificios ? !edificio_casa(o) : !tz_classe_e_unidade(o->classe)) continue;
			int r[4];
			ia_rect(ia, i, r);
			uint32_t d = ia_dist(mx, my, r);
			if (perto == TZ_NENHUM || d < melhor) { perto = i; melhor = d; }
		}
		ia->sem_casa = !edificios;
		ia->casa[0] = m->obj[perto].cx;
		ia->casa[1] = m->obj[perto].cy;
	}
	ia->tem_casa = 1;
	/* su isla: la región de tierra más cercana en cinco anillos */
	ia->ilha = 0;
	for (int r = 0; r < 5 && !ia->ilha; ++r)
		for (int y = ia->casa[1] - r; y <= ia->casa[1] + r && !ia->ilha; ++y)
			for (int x = ia->casa[0] - r; x <= ia->casa[0] + r && !ia->ilha; ++x)
				ia->ilha = (uint16_t)ia_ilha(ia, x, y);
}

/* ------------------------------------------------------------------ eventos: pérdidas y amenazas */
static void eventos(TzIa *ia)
{
	TzEvento ev[64];
	int n;
	while ((n = tz_mundo_eventos(ia->m, &ia->cursor_ev, ev, 64)) > 0) {
		for (int j = 0; j < n; ++j) {
			const TzEvento *e = &ev[j];
			const TzObj *o = ia_obj(ia, e->obj);
			if (e->tipo == TZ_EV_MORREU && o && o->dono == ia->k && e->alvo != TZ_NENHUM) {
				const TzObj *a = ia_obj(ia, e->alvo);
				if (!a || tz_mundo_relacao(ia->m, ia->k, a->dono) > 0) continue;
				/* un lugar de pérdidas donde el reino perdió algo (places.rs losses) */
				int cx = o->cx, cy = o->cy, achou = 0;
				for (int p = 0; p < ia->nperdas && !achou; ++p)
					if (ia->perda[p].x / 8 == cx / 8 && ia->perda[p].y / 8 == cy / 8) { ia->perda[p].ate_ms = ia_ms(ia) + 60000; achou = 1; }
				if (!achou && ia->nperdas < 32) {
					TzIaPerda *pp = &ia->perda[ia->nperdas++];
					pp->x = (int16_t)cx; pp->y = (int16_t)cy; pp->mascara = 1u << (ia->k - 1); pp->ate_ms = ia_ms(ia) + 60000;
				}
			} else if (e->tipo == TZ_EV_FERIDO) {
				const TzObj *v = ia_obj(ia, e->alvo);
				if (!v || v->dono != ia->k || !o || tz_mundo_relacao(ia->m, ia->k, o->dono) > 0) continue;
				/* el que nos pega es una amenaza para la milicia (0x411ed0) */
				int ja = 0;
				for (int a = 0; a < ia->nameacas; ++a) if (ia->ameaca[a] == e->obj) ja = 1;
				if (!ja && ia->nameacas < 8) ia->ameaca[ia->nameacas++] = e->obj;
			}
		}
		if (n < 64) break;
	}
	for (int p = 0; p < ia->nperdas;)
		if (ia->perda[p].ate_ms <= ia_ms(ia)) ia->perda[p] = ia->perda[--ia->nperdas];
		else p++;
}

/* ------------------------------------------------------------------ la mirada (0x407c20) y las fases (0x408d80) */
#define periodo ia_periodo

static void tomar_conta(TzIa *ia)
{
	/* 0x407110: cada unidad puede moverse otra vez (no hay comando para el «stance»), la reserva del principio */
	ia->floresta.raio = 0; ia->floresta.guardado = 0;
	ia->floresta.at[0] = ia->casa[0]; ia->floresta.at[1] = ia->casa[1];
	ia->floresta.tem_madeira = 0;
	ia_reserva_inicial(ia, 1);
}

static void pensar(TzIa *ia)
{
	switch (ia->fase) {
	case 0: if (ia->modo & 1) ia_fase_construir(ia); break;
	case 1: if (ia->modo & 1) ia_fase_colher(ia); break;
	case 2: if (ia->modo & 1) ia_fase_armazens(ia); break;
	case 3: if (ia->modo & 2) ia_fase_guerra(ia); break;
	case 4: ia_fase_especial(ia); break;
	}
	ia->fase = (uint8_t)((ia->fase + 1) % 5);
}

/* las unidades ociosas (0x4140b0), tras la fase */
static void ociosas(TzIa *ia)
{
	uint32_t agora = ia_ms(ia);
	for (TzId i = 0; i < ia->m->nobj && i < ia->cappeao; ++i) {
		const TzObj *o = &ia->m->obj[i];
		if (o->dono != ia->k || !ia_contado(o) || !o->ator || !tz_classe_e_unidade(o->classe)) { ia->ocioso[i] = 0; continue; }
		if (o->ator->ordem.tipo != TZ_O_NADA || o->ator->nfila || ia->mandado[i] == ia->m->tic + 1) { ia->ocioso[i] = 0; continue; }
		if (!ia->ocioso[i]) ia->ocioso[i] = agora + 1;
		uint32_t ms = agora + 1 - ia->ocioso[i];
		if (i == ia->explorador) continue;
		if (o->classe == TZC_PEON) { if (ia->modo & 1) ia_ocioso_campones(ia, i, ms); }
		else ia_ocioso_unidade(ia, i, ms);
	}
}

static void olhar(TzIa *ia)
{
	ia->contas.olhares++;
	if (crescer(ia)) return;
	int primeira = !ia->tem_casa;
	if (primeira || ia->sem_casa) achar_casa(ia);
	if (!ia->tem_casa) return;
	listas(ia);
	recontar(ia);
	if (primeira) tomar_conta(ia);
	ia_lugares(ia);
	/* las amenazas de los eventos llaman a la milicia (0x413a99) */
	for (int a = 0; a < ia->nameacas; ++a) {
		const TzObj *o = ia_obj(ia, ia->ameaca[a]);
		if (o && ia_contado(o) && ia_ve(ia, ia->ameaca[a]) && (ia->modo & 1)) ia_milicia(ia, ia->ameaca[a]);
	}
	ia->nameacas = 0;
	if (tz_ia_depurar)
		printf("[DBG] t%u ia%d fase %d casa %d,%d disp %d/%d/%d/%d quer %u/%u/%u/%u grupos o%d c%d m%d %d/%d/%d/%d camp %d exer %d minas %d campos %d lugares %d baldes g%d/%d/%d/%d t%d/%d/%d/%d\n",
		       ia->m->tic, ia->k, ia->fase, ia->casa[0], ia->casa[1], ia->disponivel[0], ia->disponivel[1], ia->disponivel[2],
		       ia->disponivel[3], ia->quer[0], ia->quer[1], ia->quer[2], ia->quer[3], ia->grupo[0].n, ia->grupo[1].n, ia->grupo[2].n,
		       ia->grupo[3].n, ia->grupo[4].n, ia->grupo[5].n, ia->grupo[6].n, ia->campones.n, ia->exercito.n, ia->minas.n, ia->campos.n,
		       ia->nlugares, ia->baldes[B_GUIAO][0], ia->baldes[B_GUIAO][1], ia->baldes[B_GUIAO][2], ia->baldes[B_GUIAO][3],
		       ia->baldes[B_TRABALHADORES][0], ia->baldes[B_TRABALHADORES][1], ia->baldes[B_TRABALHADORES][2], ia->baldes[B_TRABALHADORES][3]);
	pensar(ia);
	if (ia->fase != 0) ia_explorar(ia);   /* la fase especial (que acaba de pasar a 0) ya exploró */
	ociosas(ia);
}

static void gancho_tic(TzMundo *m, void *dados)
{
	TzIa *ia = dados;
	if (ia->m != m) return;
	if (m->reino[ia->k].derrotado || m->reino[ia->k].rendido) return;
	eventos(ia);
	if (!(ia->modo & 3)) return;
	if (ia_ms(ia) < ia->proximo_olhar) return;
	ia->proximo_olhar += periodo(ia);
	if (ia->proximo_olhar <= ia_ms(ia)) ia->proximo_olhar = ia_ms(ia) + periodo(ia);
	olhar(ia);
}

/* ------------------------------------------------------------------ guardar y cargar */
#define ESC(v) do { if (fwrite(&(v), sizeof(v), 1, f) != 1) return -1; } while (0)
#define LER(v) do { if (lidos + sizeof(v) > tam || fread(&(v), sizeof(v), 1, f) != 1) return -1; lidos += sizeof(v); } while (0)
#define IA_VERSAO 2u

static int gravar(TzMundo *m, void *dados, FILE *f)
{
	TzIa *ia = dados;
	uint32_t v = IA_VERSAO, n;
	/* la simulación no guarda la cola de eventos: lo que queda por leer se lee ya (igual en la partida que sigue) */
	eventos(ia);
	ESC(v);
	ESC(ia->modo); ESC(ia->rng.s); ESC(ia->paz_ate); ESC(ia->proximo_olhar); ESC(ia->fase); ESC(ia->tem_casa);
	ESC(ia->sem_casa); ESC(ia->casa); ESC(ia->ilha); ESC(ia->tope); ESC(ia->disponivel); ESC(ia->comprometido);
	ESC(ia->baldes); ESC(ia->trancado); ESC(ia->ordem); ESC(ia->quer); ESC(ia->bloqueado);
	ESC(ia->floresta.raio); ESC(ia->floresta.guardado); ESC(ia->floresta.at); ESC(ia->floresta.tem_madeira);
	ESC(ia->floresta.nenhuma); ESC(ia->floresta.madeira); ESC(ia->cursor_ev); ESC(ia->explorador); ESC(ia->nfalas);
	ESC(ia->contas);
	n = (uint32_t)ia->nperdas; ESC(n);
	for (int j = 0; j < ia->nperdas; ++j) { ESC(ia->perda[j].x); ESC(ia->perda[j].y); ESC(ia->perda[j].mascara); ESC(ia->perda[j].ate_ms); }
	n = (uint32_t)ia->nameacas; ESC(n);
	for (int j = 0; j < ia->nameacas; ++j) ESC(ia->ameaca[j]);
	n = (uint32_t)ia->prog.nregras; ESC(n);
	for (int j = 0; j < ia->prog.nregras; ++j) ESC(ia->prog.regra[j].flags);
	n = (uint32_t)ia->prog.ngat; ESC(n);
	for (int j = 0; j < ia->prog.ngat; ++j) ESC(ia->prog.gat[j].ligado);
	n = (uint32_t)ia->prog.nconds; ESC(n);
	if (n && fwrite(ia->sorteio, 1, n, f) != n) return -1;
	n = (uint32_t)ia->b->ntipos * 2; ESC(n);
	if (n && fwrite(ia->conta_ant, sizeof *ia->conta_ant, n, f) != n) return -1;
	n = m->cat ? (uint32_t)m->cat->ntec : 0; ESC(n);
	if (n && fwrite(ia->pesq_ant, 1, n, f) != n) return -1;
	n = (uint32_t)ia->nus.n; ESC(n);
	for (int j = 0; j < ia->nus.n; ++j) ESC(ia->nus.v[j]);
	n = (uint32_t)ia->cappeao; ESC(n);
	for (int j = 0; j < ia->cappeao; ++j) { ESC(ia->ocioso[j]); ESC(ia->mandado[j]); }
	return 0;
}

static int carregar(TzMundo *m, void *dados, FILE *f, uint32_t tam)
{
	TzIa *ia = dados;
	uint32_t lidos = 0, v, n;
	LER(v);
	if (v != IA_VERSAO) return -1;
	LER(ia->modo); LER(ia->rng.s); LER(ia->paz_ate); LER(ia->proximo_olhar); LER(ia->fase); LER(ia->tem_casa);
	LER(ia->sem_casa); LER(ia->casa); LER(ia->ilha); LER(ia->tope); LER(ia->disponivel); LER(ia->comprometido);
	LER(ia->baldes); LER(ia->trancado); LER(ia->ordem); LER(ia->quer); LER(ia->bloqueado);
	LER(ia->floresta.raio); LER(ia->floresta.guardado); LER(ia->floresta.at); LER(ia->floresta.tem_madeira);
	LER(ia->floresta.nenhuma); LER(ia->floresta.madeira); LER(ia->cursor_ev); LER(ia->explorador); LER(ia->nfalas);
	LER(ia->contas);
	LER(n);
	if (n > 32) return -1;
	ia->nperdas = (int)n;
	for (int j = 0; j < ia->nperdas; ++j) { LER(ia->perda[j].x); LER(ia->perda[j].y); LER(ia->perda[j].mascara); LER(ia->perda[j].ate_ms); }
	LER(n);
	if (n > 8) return -1;
	ia->nameacas = (int)n;
	for (int j = 0; j < ia->nameacas; ++j) LER(ia->ameaca[j]);
	LER(n);
	if (n != (uint32_t)ia->prog.nregras) return -1;
	for (int j = 0; j < ia->prog.nregras; ++j) LER(ia->prog.regra[j].flags);
	LER(n);
	if (n != (uint32_t)ia->prog.ngat) return -1;
	for (int j = 0; j < ia->prog.ngat; ++j) LER(ia->prog.gat[j].ligado);
	LER(n);
	if (n != (uint32_t)ia->prog.nconds || lidos + n > tam || (n && fread(ia->sorteio, 1, n, f) != n)) return -1;
	lidos += n;
	LER(n);
	if (n != (uint32_t)ia->b->ntipos * 2 || lidos + n * 4 > tam || (n && fread(ia->conta_ant, 4, n, f) != n)) return -1;
	lidos += n * 4;
	LER(n);
	if (n != (m->cat ? (uint32_t)m->cat->ntec : 0) || lidos + n > tam || (n && fread(ia->pesq_ant, 1, n, f) != n)) return -1;
	lidos += n;
	LER(n);
	if (n > 4096) return -1;
	ia->nus.n = 0;
	for (uint32_t j = 0; j < n; ++j) { TzId x; LER(x); ia_lista_por(&ia->nus, x); }
	LER(n);
	if (n > (1u << 24)) return -1;
	if (crescer_ate(ia, (int)n)) return -1;
	for (uint32_t j = 0; j < n; ++j) { LER(ia->ocioso[j]); LER(ia->mandado[j]); }
	ia->cursor_ev = m->ev_seq;   /* la cola de eventos empieza de nuevo al cargar */
	return lidos == tam ? 0 : -1;
}

/* ------------------------------------------------------------------ crear y liberar */
TzIa *tz_ia_criar(TzMundo *m, const TzIaBiblioteca *b, int k, int modo, const char *guiao, int ordem, int total,
                  TzIaErro *e)
{
	if (k < 1 || k > TZ_REINOS) return NULL;
	TzIa *ia = calloc(1, sizeof *ia);
	if (!ia) return NULL;
	ia->m = m;
	ia->b = b;
	ia->k = k;
	ia->civ = m->reino[k].civ;
	ia->modo = (uint16_t)(modo & 3);
	/* ai.rs script_name: el del reino, si no random.ais */
	snprintf(ia->guiao, sizeof ia->guiao, "%s", guiao && *guiao ? guiao : "random.ais");
	if (tz_ia_compilar(b, ia->guiao, &ia->prog, e)) { free(ia); return NULL; }
	ia->rng.s = m->semente ^ (0x9e3779b9u * (uint32_t)k);
	ia->paz_ate = m->regras.dificuldade == 0 ? IA_PAZ_FACIL_MS : 0;
	ia->ordem[0] = 0; ia->ordem[1] = 1; ia->ordem[2] = 2; ia->ordem[3] = 3;
	ia->explorador = TZ_NENHUM;
	ia->sorteio = calloc((size_t)ia->prog.nconds + 1, 1);
	ia->conta_ant = calloc((size_t)b->ntipos * 2 + 1, sizeof *ia->conta_ant);
	ia->pesq_ant = calloc((size_t)(m->cat ? m->cat->ntec : 0) + 1, 1);
	if (!ia->sorteio || !ia->conta_ant || !ia->pesq_ant) { tz_ia_liberar(ia); return NULL; }
	ia_arvore(ia);
	/* 0x402760: las primeras miradas, a un periodo entre todos en orden de reino */
	if (total < 1) total = 1;
	ia->proximo_olhar = m->tic * (uint32_t)TZ_TIC_MS + periodo(ia) / (uint32_t)total * (uint32_t)ordem;
	TzGancho g;
	memset(&g, 0, sizeof g);
	g.dados = ia;
	g.tic = gancho_tic;
	g.gravar = gravar;
	g.carregar = carregar;
	snprintf(g.etiqueta, sizeof g.etiqueta, "IA%d", k);
	g.cada = 1;
	if (tz_mundo_gancho(m, &g)) {
		if (e) { memset(e, 0, sizeof *e); e->mensagem = "No caben más ganchos"; }
		tz_ia_liberar(ia);
		return NULL;
	}
	return ia;
}

int tz_ia_criar_todas(TzMundo *m, const TzIaBiblioteca *b, TzIa **out, int max, TzIaErro *e)
{
	int total = 0, n = 0;
	for (int k = 1; k <= TZ_REINOS; ++k) if (m->reino[k].ativo && m->reino[k].ia) total++;
	for (int k = 1; k <= TZ_REINOS && n < max; ++k) {
		if (!m->reino[k].ativo || !m->reino[k].ia) continue;
		/* el guion y los bits del registro del reino (0x402760: K+0x64 y K+0xa); sin bits, completa */
		const TzReino *r = m->mapa && k <= m->mapa->nreinos ? &m->mapa->reinos[k - 1] : NULL;
		int modo = r && (r->ia_modo & 3) ? r->ia_modo & 3 : TZ_IA_COMPLETA;
		TzIa *ia = tz_ia_criar(m, b, k, modo, r ? r->ia_guiao : NULL, n, total, e);
		if (!ia) break;   /* 0x402760: los siguientes al que falla se quedan sin IA */
		out[n++] = ia;
	}
	return n;
}

void tz_ia_liberar(TzIa *ia)
{
	if (!ia) return;
	/* el gancho se queda en el mundo: quitarlo es cosa de quien libere el mundo antes */
	tz_ia_programa_liberar(&ia->prog);
	free(ia->sorteio); free(ia->conta_ant); free(ia->pesq_ant); free(ia->vig_ini); free(ia->vig_reg);
	TzIaLista *ls[] = { &ia->campones, &ia->castelos, &ia->nus, &ia->obras, &ia->magos, &ia->armazens, &ia->serrarias,
	                    &ia->granjas, &ia->minas, &ia->campos, &ia->torres, &ia->postos, &ia->oficinas, &ia->bonecos,
	                    &ia->exercito, &ia->quarteis };
	for (unsigned j = 0; j < sizeof ls / sizeof *ls; ++j) free(ls[j]->v);
	for (int g = 0; g < G_N; ++g) free(ia->grupo[g].v);
	free(ia->peao); free(ia->ocioso); free(ia->mandado); free(ia->lugar); free(ia->presente); free(ia->produtor);
	free(ia);
}

/* 0x404460: la orden «AI» (en minúsculas) */
int tz_ia_ordem(TzIa *ia, const char *o)
{
	if (!strcmp(o, "talk on")) { ia->modo |= TZ_IA_FALA; return 0; }
	if (!strcmp(o, "talk off")) { ia->modo &= (uint16_t)~TZ_IA_FALA; return 0; }
	if (!strcmp(o, "none")) { ia->modo &= (uint16_t)~3; return 0; }
	if (!strcmp(o, "eco")) { ia->modo = (uint16_t)((ia->modo & ~3) | 1); return 0; }
	if (!strcmp(o, "war")) { ia->modo = (uint16_t)((ia->modo & ~3) | 2); return 0; }
	if (!strcmp(o, "full")) { ia->modo = (uint16_t)((ia->modo & ~3) | 3); return 0; }
	return -1;
}

void tz_ia_sem_ataque(TzIa *ia, uint32_t ms) { ia->modo |= 0x80; ia->paz_ate = ms; }

const char *tz_ia_fala(const TzIa *ia, uint32_t *cursor)
{
	if (*cursor >= ia->nfalas) return NULL;
	if (ia->nfalas - *cursor > IA_FALAS) *cursor = ia->nfalas - IA_FALAS;
	return ia->falas[(*cursor)++ % IA_FALAS];
}

const TzIaPrograma *tz_ia_programa(const TzIa *ia) { return &ia->prog; }
TzIaContas tz_ia_contas(const TzIa *ia) { return ia->contas; }

static uint32_t fnv(uint32_t h, const void *p, size_t n)
{
	const uint8_t *b = p;
	for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 16777619u; }
	return h;
}

uint32_t tz_ia_suma(const TzIa *ia)
{
	uint32_t h = 2166136261u;
	h = fnv(h, &ia->modo, sizeof ia->modo); h = fnv(h, &ia->rng.s, sizeof ia->rng.s);
	h = fnv(h, &ia->fase, 1); h = fnv(h, ia->casa, sizeof ia->casa);
	h = fnv(h, ia->disponivel, sizeof ia->disponivel); h = fnv(h, ia->comprometido, sizeof ia->comprometido);
	h = fnv(h, ia->baldes, sizeof ia->baldes); h = fnv(h, ia->quer, sizeof ia->quer); h = fnv(h, ia->ordem, sizeof ia->ordem);
	for (int j = 0; j < ia->prog.nregras; ++j) h = fnv(h, &ia->prog.regra[j].flags, 4);
	for (int j = 0; j < ia->prog.ngat; ++j) h = fnv(h, &ia->prog.gat[j].ligado, 1);
	h = fnv(h, &ia->contas, sizeof ia->contas);
	return h;
}

/* ------------------------------------------------------------------ EXTERN: expresiones de los guiones */
typedef struct { const char *s; TzMundo *m; int k; } Expr;

static void eb(Expr *x) { while (*x->s == ' ' || *x->s == '\t') x->s++; }
static int32_t e_ou(Expr *x);

static int32_t e_prim(Expr *x)
{
	eb(x);
	if (*x->s == '(') { x->s++; int32_t v = e_ou(x); eb(x); if (*x->s == ')') x->s++; return v; }
	if (*x->s == '!') { x->s++; return !e_prim(x); }
	if (*x->s == '-') { x->s++; return -e_prim(x); }
	if (*x->s >= '0' && *x->s <= '9') { int32_t v = 0; while (*x->s >= '0' && *x->s <= '9') v = v * 10 + (*x->s++ - '0'); return v; }
	if (*x->s == '$' || *x->s == '@') {
		char nome[32];
		int fun = *x->s == '@', n = 0;
		x->s++;
		while ((isalnum((unsigned char)*x->s) || *x->s == '_') && n < 31) nome[n++] = (char)toupper((unsigned char)*x->s++);
		nome[n] = 0;
		int32_t arg = x->k;
		eb(x);
		if (fun && *x->s == '(') { x->s++; arg = e_ou(x); eb(x); if (*x->s == ')') x->s++; }
		if (!fun) return !strcmp(nome, "AIK") ? x->k : 0;
		static const char *const rec[4] = { "GOLD", "STONE", "FOOD", "WOOD" };
		for (int r = 0; r < 4; ++r)
			if (!strcmp(nome, rec[r])) return arg == x->k && arg >= 1 && arg <= TZ_REINOS ? x->m->reino[arg].recursos[r] : 0;
		return 0;
	}
	if (*x->s) x->s++;
	return 0;
}

static int32_t e_mul(Expr *x)
{
	int32_t v = e_prim(x);
	for (;;) {
		eb(x);
		char c = *x->s;
		if (c != '*' && c != '/' && c != '%') return v;
		x->s++;
		int32_t w = e_prim(x);
		if (c == '*') v *= w;
		else if (!w) v = 0;
		else v = c == '/' ? v / w : v % w;
	}
}

static int32_t e_soma(Expr *x)
{
	int32_t v = e_mul(x);
	for (;;) {
		eb(x);
		if (*x->s == '+') { x->s++; v += e_mul(x); }
		else if (*x->s == '-') { x->s++; v -= e_mul(x); }
		else return v;
	}
}

static int32_t e_cmp(Expr *x)
{
	int32_t v = e_soma(x);
	eb(x);
	const char *s = x->s;
	if (s[0] == '=' && s[1] == '=') { x->s += 2; return v == e_soma(x); }
	if (s[0] == '!' && s[1] == '=') { x->s += 2; return v != e_soma(x); }
	if (s[0] == '<' && s[1] == '=') { x->s += 2; return v <= e_soma(x); }
	if (s[0] == '>' && s[1] == '=') { x->s += 2; return v >= e_soma(x); }
	if (s[0] == '<') { x->s++; return v < e_soma(x); }
	if (s[0] == '>') { x->s++; return v > e_soma(x); }
	return v;
}

static int32_t e_e(Expr *x)
{
	int32_t v = e_cmp(x);
	for (;;) {
		eb(x);
		if (x->s[0] == '&' && x->s[1] == '&') { x->s += 2; int32_t w = e_cmp(x); v = v && w; }
		else return v;
	}
}

static int32_t e_ou(Expr *x)
{
	int32_t v = e_e(x);
	for (;;) {
		eb(x);
		if (x->s[0] == '|' && x->s[1] == '|') { x->s += 2; int32_t w = e_e(x); v = v || w; }
		else return v;
	}
}

int32_t tz_ia_expr(TzMundo *m, int k, const char *expr)
{
	Expr x = { expr, m, k };
	return e_ou(&x);
}
