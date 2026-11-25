/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ia_guerra.c - la guerra de un reino de la IA: los lugares del mapa que conoce y lo que sabe de cada uno, los grupos
 * que manda a los que se quedan cortos, las torres y defensas, la milicia de campesinos, las unidades ociosas y la
 * exploración (D417, fase 6).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): src/gameplay/ai/places.rs (lugares
 * alineados a sectores de 8x8: casas, recursos, pérdidas, portales, bases y unidades, 0x404d00, 0x403210),
 * records.rs (prioridad, mínimo y fuerza querida, 0x40fdd0), war.rs (fase de guerra 0x410be0, grupos 0x4113a0,
 * candidatos 0x4111b0, 0x410cd0, 0x410fe0, milicia 0x4120d0), defence.rs (fase especial 0x409220, torres 0x4107d0,
 * defensas 0x410b60, 0x4104f0), build.rs (seguridad 0x411a30, 0x411dd0) e idle.rs (unidades ociosas 0x4140b0).
 * Diferencias: los lugares se rehacen en cada mirada con lo que el reino VE (el gestor del original lo sabe todo y los
 * lleva al día con ganchos); de los enemigos no se sabe adónde van; los grupos van con «mover» al punto del lugar (la
 * simulación los hace pelear con lo que encuentran) en vez del programa Approach; y la exploración es nuestra (el
 * original no la necesita porque ve todo el mapa; D470: también cruza en barco). La flota, el transporte, los hechizos y
 * las brechas están en ia_agua.c, ia_magia.c e ia_brecha.c (D470).
 */
#include "ia_int.h"

#define CERCA_GRUPO 32       /* war.rs NEAR */
#define TENTATIVAS 100       /* war.rs POINT_TRIES */
#define OUSADIA 10           /* war.rs BOLD */
#define TORRES_CASA_1 5      /* defence.rs HOME_TOWERS */
#define TORRES_CASA_2 10
#define ALCANCE_ITEM 16      /* idle.rs FIGHTER_ITEMS */
#define ALCANCE_REFUGIO 24   /* idle.rs REFUGE_REACH */

uint32_t ia_periodo(const TzIa *ia)
{
	static const uint32_t p[3] = IA_PERIODOS;
	int d = ia->m->regras.dificuldade;
	return p[d > 2 ? 2 : d];
}

static uint32_t forca_mask(const TzIaForcas *f, int mask)
{
	uint32_t s = 0;
	for (int c = 0; c < CAT_N; ++c) if (mask & (1 << c)) s += f->c[c].forca;
	return s;
}
/* las máscaras de places.rs no siguen el orden de las categorías: militar 1, torres 2, civil 4, edificios 8, cuarteles 16 */
static int bits(int mask)
{
	int b = 0;
	if (mask & M_MILITAR) b |= 1 << CAT_MILITAR;
	if (mask & M_TORRES) b |= 1 << CAT_TORRES;
	if (mask & M_CIVIL) b |= 1 << CAT_CIVIL;
	if (mask & M_EDIFICIOS) b |= 1 << CAT_EDIFICIOS;
	if (mask & M_QUARTEIS) b |= 1 << CAT_QUARTEIS;
	return b;
}
static uint32_t forca(const TzIaForcas *f, int mask) { return forca_mask(f, bits(mask)); }
static uint32_t conta(const TzIaForcas *f, int mask)
{
	uint32_t s = 0;
	int b = bits(mask);
	for (int c = 0; c < CAT_N; ++c) if (b & (1 << c)) s += f->c[c].n;
	return s;
}

static int categoria(const TzIa *ia, const TzObj *o)
{
	if (tz_classe_e_unidade(o->classe)) return ia_militar(ia, o) ? CAT_MILITAR : CAT_CIVIL;
	if (o->classe >= 39 && o->classe <= 43 && !ia_obra(o)) return CAT_TORRES;
	if (o->classe == TZC_BARRACK) return CAT_QUARTEIS;
	return CAT_EDIFICIOS;
}

static int parede(int c) { return c == TZC_WALL_STONE || c == TZC_WALL_WOOD || c == TZC_GATE_WOOD || c == TZC_GATE_STONE; }

static void ret_setores(const TzIa *ia, TzId i, int n, int16_t out[4])
{
	int r[4];
	ia_rect(ia, i, r);
	int cx = ((r[0] + r[2]) / 2) / 8, cy = ((r[1] + r[3]) / 2) / 8, u = ia->m->lado - 1;
	int v[4] = { (cx - n) * 8, (cy - n) * 8, (cx + n) * 8 + 7, (cy + n) * 8 + 7 };
	out[0] = (int16_t)(v[0] < 0 ? 0 : v[0]); out[1] = (int16_t)(v[1] < 0 ? 0 : v[1]);
	out[2] = (int16_t)(v[2] > u ? u : v[2]); out[3] = (int16_t)(v[3] > u ? u : v[3]);
}

static int dentro(const int16_t r[4], int x, int y) { return x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3]; }
static int setor_dentro(const int16_t r[4], int sx, int sy) { return sx >= r[0] / 8 && sx <= r[2] / 8 && sy >= r[1] / 8 && sy <= r[3] / 8; }
static int sobrepoe(const int16_t a[4], const int16_t b[4]) { return a[0] <= b[2] && b[0] <= a[2] && a[1] <= b[3] && b[1] <= a[3]; }

static int setor_tem_lugar(const TzIa *ia, int x, int y)
{
	for (int p = 0; p < ia->nlugares; ++p) if (setor_dentro(ia->lugar[p].rect, x / 8, y / 8)) return 1;
	return 0;
}

static TzIaLugar *novo(TzIa *ia, int tipo, uint32_t param, TzId ancora)
{
	if (ia->nlugares >= ia->caplugares) {
		int c = ia->caplugares ? ia->caplugares * 2 : 32;
		TzIaLugar *l = realloc(ia->lugar, (size_t)c * sizeof *l);
		if (!l) return NULL;
		ia->lugar = l;
		ia->caplugares = c;
	}
	TzIaLugar *l = &ia->lugar[ia->nlugares++];
	memset(l, 0, sizeof *l);
	l->tipo = (uint8_t)tipo;
	l->param = param;
	l->ancora = ancora;
	if (ancora != TZ_NENHUM) {
		int r[4];
		ia_rect(ia, ancora, r);
		for (int j = 0; j < 4; ++j) l->arect[j] = (int16_t)r[j];
	}
	return l;
}

/* el destino de una unidad propia (war.rs heading): la casilla de su orden */
static int destino(const TzIa *ia, TzId i, int *x, int *y)
{
	const TzObj *o = &ia->m->obj[i];
	if (!o->ator) return 0;
	const TzOrdem *d = &o->ator->ordem;
	if (d->tipo == TZ_O_MOVER || d->tipo == TZ_O_PATRULHAR || d->tipo == TZ_O_EXPLORAR) { *x = d->x; *y = d->y; return 1; }
	if (d->tipo == TZ_O_ATACAR) {
		const TzObj *t = ia_obj(ia, d->alvo);
		if (t) { *x = t->cx; *y = t->cy; return 1; }
	}
	return 0;
}

/* 0x40fdd0: el registro evaluado: distancia, prioridad, fuerzas queridas */
static void avaliar_lugar(TzIa *ia, TzIaLugar *l, uint32_t militar)
{
	uint32_t lado = (uint32_t)ia->m->lado;
	int r[4] = { l->rect[0], l->rect[1], l->rect[2], l->rect[3] };
	uint32_t dist = ia_dist(ia->casa[0], ia->casa[1], r);
	uint32_t fl = 0, mn = 0, qu = 0, pri = 0;
	int dif = ia->m->regras.dificuldade;
	switch (l->tipo) {
	case L_CASA:
		if (l->param == (uint32_t)ia->k) {
			fl |= 7; mn = militar / 10 > 1 ? militar / 10 : 1; qu = militar / 8 > 1 ? militar / 8 : 1;
		} else if (tz_mundo_relacao(ia->m, (int)l->param, ia->k) > 0) {
			fl |= 6; mn = militar / 50; qu = militar / 10;
		}
		pri = (lado * 3 - dist * 3) >> 1;
		break;
	case L_RECURSO: {
		if (dif && dist >= 16) {
			if (dist < 32) fl |= 1;
			else { fl |= 7; mn = qu = 1; }
		}
		int trabalhada = 0;
		for (int g = G_OURO; g <= G_PEDRA && !trabalhada; ++g)
			for (int j = 0; j < ia->grupo[g].n && !trabalhada; ++j) trabalhada = ia->peao[ia->grupo[g].v[j]].alvo == l->ancora;
		pri = trabalhada ? lado - (dist >> 1) : lado - dist;
		break;
	}
	case L_PERDAS: pri = lado - dist; break;
	case L_PORTAL: fl |= 4; mn = qu = 1; pri = lado - dist; break;
	case L_BASE:
		if (dif == 2 && l->param == (uint32_t)ia->k) fl |= 1;
		pri = lado - dist;
		break;
	case L_UNIDADES: pri = (lado - dist) * 2 / 3; break;
	}
	l->flags = fl; l->distancia = dist; l->prioridade = pri;
	uint32_t ameaca = forca(&l->hostil, M_MILITAR | M_TORRES);
	if (ameaca) {
		l->minimo = ameaca * 3 / 2 > mn ? ameaca * 3 / 2 : mn;
		l->desejado = ameaca * 3 > qu ? ameaca * 3 : qu;
	} else {
		uint32_t resto = forca(&l->hostil, M_CIVIL | M_EDIFICIOS);
		l->minimo = resto / 5 > mn ? resto / 5 : mn;
		l->desejado = resto * 2 > qu ? resto * 2 : qu;
	}
}

/* los lugares que el reino conoce, sus fuerzas y su registro, por prioridad */
void ia_lugares(TzIa *ia)
{
	TzMundo *m = ia->m;
	ia->nlugares = 0;
	/* minas, castillos y portales (0x404d00) */
	for (TzId i = 0; i < m->nobj; ++i) {
		const TzObj *o = &m->obj[i];
		if (!ia_contado(o) || (o->flags & (TZ_F_INATIVO | TZ_F_DENTRO)) || !ia_ve(ia, i)) continue;
		if (o->classe == TZC_RES_GOLD || o->classe == TZC_RES_STONE) {
			TzIaLugar *l = novo(ia, L_RECURSO, o->classe == TZC_RES_STONE, i);
			if (l) ret_setores(ia, i, 1, l->rect);
		} else if (o->classe == TZC_CASTLE && o->dono >= 1 && o->dono <= TZ_REINOS) {
			int ja = 0;
			for (int p = 0; p < ia->nlugares && !ja; ++p)
				ja = ia->lugar[p].tipo == L_CASA && ia->lugar[p].param == o->dono && setor_dentro(ia->lugar[p].rect, o->cx / 8, o->cy / 8);
			if (ja) continue;
			TzIaLugar *l = novo(ia, L_CASA, o->dono, i);
			if (l) ret_setores(ia, i, 2, l->rect);
		} else if (o->classe == IC_MAGICGATEWAY) {
			TzIaLugar *l = novo(ia, L_PORTAL, 0, i);
			if (l) ret_setores(ia, i, 1, l->rect);
		}
	}
	/* las bases y las unidades lejos de todo lo demás */
	for (TzId i = 0; i < m->nobj; ++i) {
		const TzObj *o = &m->obj[i];
		if (!ia_contado(o) || (o->flags & (TZ_F_INATIVO | TZ_F_DENTRO)) || o->dono < 1 || o->dono > TZ_REINOS) continue;
		if (parede(o->classe) || !ia_ve(ia, i) || setor_tem_lugar(ia, o->cx, o->cy)) continue;
		TzIaLugar *l = NULL;
		if (tz_classe_e_unidade(o->classe)) l = novo(ia, L_UNIDADES, o->dono, i);
		else if (tz_classe_e_edificio(o->classe)) l = novo(ia, L_BASE, o->dono, i);
		if (l) ret_setores(ia, i, 1, l->rect);
	}
	for (int p = 0; p < ia->nperdas; ++p) {
		TzIaLugar *l = novo(ia, L_PERDAS, ia->perda[p].mascara, TZ_NENHUM);
		if (!l) break;
		int cx = ia->perda[p].x / 8, cy = ia->perda[p].y / 8, u = m->lado - 1;
		l->rect[0] = (int16_t)((cx - 1) * 8 < 0 ? 0 : (cx - 1) * 8); l->rect[1] = (int16_t)((cy - 1) * 8 < 0 ? 0 : (cy - 1) * 8);
		l->rect[2] = (int16_t)((cx + 1) * 8 + 7 > u ? u : (cx + 1) * 8 + 7); l->rect[3] = (int16_t)((cy + 1) * 8 + 7 > u ? u : (cy + 1) * 8 + 7);
		memcpy(l->arect, l->rect, sizeof l->arect);
	}
	/* las fuerzas de cada lugar */
	uint32_t militar = 0;
	for (TzId i = 0; i < m->nobj; ++i) {
		const TzObj *o = &m->obj[i];
		if (!ia_contado(o) || (o->flags & TZ_F_DENTRO) || o->dono < 1 || o->dono > TZ_REINOS || parede(o->classe)) continue;
		if (!tz_classe_e_unidade(o->classe) && !tz_classe_e_edificio(o->classe)) continue;
		if (!ia_ve(ia, i)) continue;
		int cat = categoria(ia, o);
		uint32_t s = ia_forca(o);
		int meu = o->dono == ia->k, amigo = tz_mundo_relacao(m, ia->k, o->dono) > 0;
		if (meu && cat == CAT_MILITAR) militar += s;
		int dx = 0, dy = 0, vai = meu && tz_classe_e_unidade(o->classe) && destino(ia, i, &dx, &dy);
		for (int p = 0; p < ia->nlugares; ++p) {
			TzIaLugar *l = &ia->lugar[p];
			if (dentro(l->rect, o->cx, o->cy)) {
				TzIaForcas *f = amigo ? &l->amigo : &l->hostil;
				f->c[cat].n++; f->c[cat].forca += s;
				if (meu) { l->meu.c[cat].n++; l->meu.c[cat].forca += s; }
			} else if (vai && dentro(l->rect, dx, dy)) {
				l->amigo_vindo.c[cat].n++; l->amigo_vindo.c[cat].forca += s;
				l->meu_vindo.c[cat].n++; l->meu_vindo.c[cat].forca += s;
			}
		}
	}
	for (int p = 0; p < ia->nlugares; ++p) avaliar_lugar(ia, &ia->lugar[p], militar);
	/* por prioridad, de mayor a menor (estable) */
	for (int a = 1; a < ia->nlugares; ++a) {
		TzIaLugar t = ia->lugar[a];
		int b = a;
		while (b > 0 && ia->lugar[b - 1].prioridade < t.prioridade) { ia->lugar[b] = ia->lugar[b - 1]; b--; }
		ia->lugar[b] = t;
	}
}

/* 0x40d4a0: en los lugares del sector del edificio, cuánto les falta de su mínimo */
uint32_t ia_descoberto(const TzIa *ia, TzId edif)
{
	const TzObj *o = ia_obj(ia, edif);
	if (!o) return 0;
	uint32_t s = 0;
	for (int p = 0; p < ia->nlugares; ++p) {
		const TzIaLugar *l = &ia->lugar[p];
		if (!setor_dentro(l->rect, o->cx / 8, o->cy / 8)) continue;
		uint32_t tem = forca(&l->amigo, M_MILITAR);
		if (tem < l->minimo) s += l->minimo - tem;
	}
	return s;
}

/* 0x411dd0: ningún luchador ni torre terminada de un reino no aliado tiene el rectángulo a la vista */
static int setor_seguro(const TzIa *ia, int sx, int sy, const int r[4])
{
	TzId v[256];
	int n = tz_mundo_area(ia->m, sx * 8, sy * 8, sx * 8 + 7, sy * 8 + 7, TZ_FILTRO_UNIDADES | TZ_FILTRO_EDIFICIOS, -1, v, 256);
	for (int j = 0; j < n; ++j) {
		const TzObj *o = &ia->m->obj[v[j]];
		if (o->dono == ia->k || tz_mundo_relacao(ia->m, ia->k, o->dono) > 0 || !ia_ve(ia, v[j])) continue;
		int armado = tz_classe_e_unidade(o->classe) ? ia_combate(o) : (o->classe >= 39 && o->classe <= 43 && !ia_obra(o));
		int q[4];
		ia_rect(ia, v[j], q);
		if (armado && ia_dist_rect(q, r) <= o->visao) return 0;
	}
	return 1;
}

/* 0x411a30: un rectángulo es seguro salvo que un lugar de su sector tenga más fuerza hostil que luchadores propios */
int ia_seguro(const TzIa *ia, int x, int y, int w, int h)
{
	int u = ia->m->lado - 1;
	x = x < 0 ? 0 : x > u ? u : x;
	y = y < 0 ? 0 : y > u ? u : y;
	int sx = x / 8, sy = y / 8, ameaca = 0, lutadores = 0, torres = 0, casa = 0;
	for (int p = 0; p < ia->nlugares; ++p) {
		const TzIaLugar *l = &ia->lugar[p];
		if (!setor_dentro(l->rect, sx, sy)) continue;
		if (forca(&l->hostil, M_MILITAR | M_TORRES) > conta(&l->amigo, M_MILITAR)) ameaca = 1;
		lutadores |= conta(&l->hostil, M_MILITAR) != 0;
		torres |= conta(&l->hostil, M_TORRES) != 0;
		if (l->tipo == L_CASA && l->param == (uint32_t)ia->k) casa = 1;
	}
	if (!ameaca) return 1;
	if (!casa && lutadores) return 0;
	if (!lutadores && !torres) return 1;
	int r[4] = { x, y, x + w - 1, y + h - 1 }, n = (ia->m->lado + 7) / 8;
	for (int yy = sy - 2 < 0 ? 0 : sy - 2; yy < n && yy < sy + 3; ++yy)
		for (int xx = sx - 2 < 0 ? 0 : sx - 2; xx < n && xx < sx + 3; ++xx)
			if (!setor_seguro(ia, xx, yy, r)) return 0;
	return 1;
}

/* ------------------------------------------------------------------ la fase de guerra (0x410be0) */
/* 0x410c70: el lugar guarda más que su mínimo por al menos la fuerza de la unidad */
static int sobra(const TzIaLugar *l, uint32_t s)
{
	uint32_t tem = forca(&l->amigo, M_MILITAR) + forca(&l->amigo_vindo, M_MILITAR);
	return tem > l->minimo && tem - l->minimo >= s;
}

/* 0x410cd0: un luchador que el jugador puede mandar */
static int candidato(TzIa *ia, TzId i, const TzIaLugar *l)
{
	const TzObj *o = &ia->m->obj[i];
	if (!ia_contado(o) || (o->flags & (TZ_F_HISTORIA | TZ_F_MORRENDO | TZ_F_DENTRO)) || !o->ator) return 0;
	if (ia_em_transporte(ia, i) || ia_brecha_de(ia, i)) return 0;
	const TzOrdem *d = &o->ator->ordem;
	if (i < ia->cappeao && ia->mandado[i] == ia->m->tic + 1) return 0;
	switch (d->tipo) {
	case TZ_O_NADA: case TZ_O_MOVER: break;
	case TZ_O_ATACAR: {
		const TzObj *t = ia_obj(ia, d->alvo);
		if (t && ia_contado(t) && !parede(t->classe) && t->classe != 133) return 0;
		break;
	}
	default: return 0;
	}
	const TzAtributos *at = ia->m->cat ? tz_obj_at(ia->m, o) : NULL;
	if (!at || !at->voa) {
		int r[4];
		if (l->ancora != TZ_NENHUM) { ia_rect(ia, l->ancora, r); r[0]--; r[1]--; r[2]++; r[3]++; }
		else { r[0] = l->arect[0]; r[1] = l->arect[1]; r[2] = l->arect[2]; r[3] = l->arect[3]; }
		/* 0x417640: por tierra, por mar con un barco del reino (D470) o abriendo brecha en un muro enemigo */
		if (!ia_alcanca(ia, o->cx, o->cy, r) && ia_via_ponto(ia, o->cx, o->cy, r) != IA_VIA_MAR &&
		    !ia_brecha_alcanca(ia, o->cx, o->cy, r)) return 0;
	}
	int dx, dy;
	if (!destino(ia, i, &dx, &dy)) return 1;
	int calmo = forca(&l->hostil, M_MILITAR | M_TORRES) == 0;
	if (calmo && (!ia->produtor || ia->produtor[o->classe] != TZC_BARRACK)) return 0;
	for (int p = 0; p < ia->nlugares; ++p) {
		const TzIaLugar *q = &ia->lugar[p];
		if (!setor_dentro(q->rect, dx / 8, dy / 8)) continue;
		if (q == l) return 1;
		if (calmo && conta(&q->hostil, 0xff)) return 0;
		if (q->prioridade >= l->prioridade && !sobra(q, ia_forca(o))) return 0;
	}
	return 1;
}

/* 0x410fe0: el lado del mapa en sectores menos la distancia del sector de la unidad al del lugar; dos tercios si ataca */
static uint32_t pontos(TzIa *ia, TzId i, const TzIaLugar *l)
{
	if (!candidato(ia, i, l)) return 0;
	const TzObj *o = &ia->m->obj[i];
	int s[4] = { l->rect[0] / 8, l->rect[1] / 8, l->rect[2] / 8, l->rect[3] / 8 };
	uint32_t d = ia_dist(o->cx / 8, o->cy / 8, s);
	uint32_t p = (uint32_t)(ia->m->lado / 8) - d;
	p &= 0xffff;
	if (o->ator->ordem.tipo == TZ_O_ATACAR) p = p * 2 / 3;
	return p;
}

/* 0x411100: los luchadores amigos de otros reinos con s llegan al mínimo (la mitad con una de cada diez) */
static int basta(TzIa *ia, const TzIaLugar *l, uint32_t s)
{
	uint32_t outros = forca(&l->amigo, M_MILITAR) - forca(&l->meu, M_MILITAR) - forca(&l->meu_vindo, M_MILITAR) +
	                  forca(&l->amigo_vindo, M_MILITAR);
	uint32_t precisa = ia_rng(ia, 1, 100) <= OUSADIA ? l->minimo >> 1 : l->minimo;
	return outros + s >= precisa;
}

/* 0x4118e0: ya va al lugar o está en él */
static int la(TzIa *ia, TzId i, const TzIaLugar *l)
{
	int dx, dy;
	if (destino(ia, i, &dx, &dy) && setor_dentro(l->rect, dx / 8, dy / 8)) return 1;
	const TzObj *o = &ia->m->obj[i];
	if (!setor_dentro(l->rect, o->cx / 8, o->cy / 8)) return 0;
	if (o->ator->ordem.tipo != TZ_O_NADA && o->ator->ordem.tipo != TZ_O_ATACAR) ia_ordem(ia, i, TZ_O_PARAR, TZ_NENHUM, 0, 0);
	return 1;
}

/* 0x4113a0: un grupo para el lugar */
static int mandar(TzIa *ia, int at)
{
	TzIaLugar *l = &ia->lugar[at];
	int casa_outro = l->tipo == L_CASA && l->param != (uint32_t)ia->k;
	int so_hostil = conta(&l->hostil, 0xff) && !conta(&l->amigo, 0xff);
	uint32_t agora = ia_ms(ia);
	if ((casa_outro || so_hostil) && ia->paz_ate && ia->paz_ate > agora) return 0;
	for (int p = 0; p < ia->nlugares; ++p) {
		const TzIaLugar *q = &ia->lugar[p];
		if (p == at || !sobrepoe(q->rect, l->rect)) continue;
		if (q->prioridade > l->prioridade && forca(&q->hostil, M_MILITAR | M_TORRES)) return 0;
	}
	if (tz_ia_depurar) printf("[DBG]     mandar ia%d: exército %d paz %u\n", ia->k, ia->exercito.n, ia->paz_ate);
	if (!ia->exercito.n) return 0;
	/* 0x4111b0: los candidatos y su fuerza cerca y lejos */
	int n = ia->exercito.n;
	TzId *id = malloc((size_t)n * sizeof *id);
	uint32_t *pt = malloc((size_t)n * sizeof *pt);
	if (!id || !pt) { free(id); free(pt); return 0; }
	uint32_t perto = 0, longe = 0;
	for (int j = 0; j < n; ++j) {
		TzId i = ia->exercito.v[j];
		id[j] = i;
		pt[j] = pontos(ia, i, l);
		if (!pt[j]) continue;
		const TzObj *o = &ia->m->obj[i];
		int r[4] = { l->arect[0], l->arect[1], l->arect[2], l->arect[3] };
		if (ia_dist(o->cx, o->cy, r) < CERCA_GRUPO) perto += ia_forca(o);
		else longe += ia_forca(o);
	}
	int modo = 0, r = 0;
	if (tz_ia_depurar) {
		int nc = 0;
		for (int j = 0; j < n; ++j) nc += pt[j] != 0;
		printf("[DBG]     mandar ia%d: exército %d candidatos %d perto %u longe %u\n", ia->k, n, nc, perto, longe);
	}
	if (perto || longe) {
		if (basta(ia, l, perto)) modo = 1;
		else if (basta(ia, l, perto + longe)) modo = 2;
		int invadida = l->tipo == L_CASA && tz_mundo_relacao(ia->m, ia->k, (int)l->param) > 0 && conta(&l->hostil, M_MILITAR | M_TORRES);
		if (!modo && invadida) modo = 2;
	}
	if (modo) {
		/* por puntos, de mayor a menor (estable) */
		for (int a = 1; a < n; ++a) {
			TzId ti = id[a];
			uint32_t tp = pt[a];
			int b = a;
			while (b > 0 && pt[b - 1] < tp) { id[b] = id[b - 1]; pt[b] = pt[b - 1]; b--; }
			id[b] = ti; pt[b] = tp;
		}
		TzId *membros = malloc((size_t)n * sizeof *membros);
		int nm = 0;
		uint32_t total = 0;
		for (int j = 0; membros && j < n; ++j) {
			if (!pt[j]) continue;
			if (!la(ia, id[j], l)) membros[nm++] = id[j];
			total += ia_forca(&ia->m->obj[id[j]]);
			if (total >= l->desejado || (modo == 1 && total >= perto)) break;
		}
		if (nm) {
			/* un punto del lugar en la isla de su ancla */
			int ilha = l->ancora != TZ_NENHUM ? ia_ilha(ia, ia->m->obj[l->ancora].cx, ia->m->obj[l->ancora].cy) : 0;
			if (!ilha && l->ancora != TZ_NENHUM) {
				int q[4];
				ia_rect(ia, l->ancora, q);
				for (int y = q[1] - 1; y <= q[3] + 1 && !ilha; ++y)
					for (int x = q[0] - 1; x <= q[2] + 1 && !ilha; ++x) ilha = ia_ilha(ia, x, y);
			}
			int px = l->arect[0], py = l->arect[1];
			for (int t = 0; ilha && t < TENTATIVAS; ++t) {
				px = ia_rng(ia, l->rect[0], l->rect[2]);
				py = ia_rng(ia, l->rect[1], l->rect[3]);
				if (ia_ilha(ia, px, py) == ilha) break;
			}
			/* los que no llegan por tierra cruzan el agua o abren brecha (D470: transport.rs organise, breach.rs) */
			int nt = 0;
			int alvo_r[4] = { px, py, px, py };
			for (int j = 0; j < nm; ++j) {
				const TzObj *o = &ia->m->obj[membros[j]];
				const TzAtributos *at = ia->m->cat ? tz_obj_at(ia->m, o) : NULL;
				if ((at && at->voa) || ia_alcanca(ia, o->cx, o->cy, alvo_r)) { membros[nt++] = membros[j]; continue; }
				if (ia_transportar(ia, membros[j], px, py, 1) == 1) continue;
				if (ia_brecha_mandar(ia, membros[j], px, py)) continue;
				membros[nt++] = membros[j];
			}
			nt = ia_avancar(ia, membros, nt, px, py);
			if (nt) ia_ordem_grupo(ia, membros, nt, TZ_O_MOVER, TZ_NENHUM, px, py);
			ia->contas.grupos++;
			if (casa_outro && ia->paz_ate && !(ia->modo & 0x80)) ia->paz_ate = agora + IA_PAUSA_CASA_MS;
			r = 1;
		}
		free(membros);
	}
	free(id);
	free(pt);
	return r;
}

void ia_fase_guerra(TzIa *ia)
{
	for (int at = 0; at < ia->nlugares; ++at) {
		const TzIaLugar *l = &ia->lugar[at];
		uint32_t amigos = forca(&l->amigo_vindo, M_MILITAR) + forca(&l->amigo, M_MILITAR);
		if (amigos < l->minimo) {
			int r = mandar(ia, at);
			if (tz_ia_depurar) printf("[DBG]   guerra ia%d: lugar %d tipo %d reino %u mínimo %u amigos %u: %s\n", ia->k, at, l->tipo, l->param,
			                           l->minimo, amigos, r ? "grupo" : "nada");
			if (r) break;
		}
	}
}

/* ------------------------------------------------------------------ la milicia (0x4120d0) */
/* war.rs uncaptured (0x271e): lo que un campesino no captura: muros y puertas, el campo, el portal mágico, las minas y el
 * muñeco */
static int sem_captura(int c)
{
	return c == TZC_WALL_STONE || c == TZC_WALL_WOOD || c == TZC_FIELD || c == TZC_GATE_WOOD || c == TZC_GATE_STONE || c == IC_MAGICGATEWAY ||
	       c == TZC_RES_GOLD || c == TZC_RES_STONE || c == TZC_DUMMY;
}

/* gameplay.rs capturable (D470): un edificio terminado que se captura, enemigo (las torres, de cualquiera no aliado) */
int ia_capturavel(const TzIa *ia, TzId i)
{
	const TzObj *o = ia_obj(ia, i);
	if (!o || !ia_contado(o) || !tz_classe_e_edificio(o->classe) || ia_obra(o) || sem_captura(o->classe)) return 0;
	if (o->dono < 1 || o->dono > TZ_REINOS || o->dono == ia->k) return 0;
	int rel = tz_mundo_relacao(ia->m, ia->k, o->dono);
	if (o->classe >= 39 && o->classe <= 42) return rel <= 0;
	return rel < 0;
}

void ia_milicia(TzIa *ia, TzId ameaca)
{
	const TzObj *t = ia_obj(ia, ameaca);
	if (!t || !ia_contado(t)) return;
	/* D470: lo que se puede capturar lo toma el lugar del sector con más campesinos del reino (war.rs militia) */
	int cap = ia_capturavel(ia, ameaca);
	int sx = t->cx / 8, sy = t->cy / 8, casa = 0;
	const TzIaLugar *melhor = NULL;
	for (int p = 0; p < ia->nlugares; ++p) {
		const TzIaLugar *l = &ia->lugar[p];
		if (!setor_dentro(l->rect, sx, sy)) continue;
		if (l->tipo == L_CASA && l->param == (uint32_t)ia->k) casa = 1;
		if (!conta(&l->meu, M_CIVIL)) continue;
		if (cap) { if (!melhor || conta(&melhor->meu, M_CIVIL) < conta(&l->meu, M_CIVIL)) melhor = l; continue; }
		uint32_t hostil = forca(&l->hostil, M_MILITAR | M_TORRES) * 3, amigo = forca(&l->amigo, M_MILITAR);
		if (amigo > hostil >> 1) return;
		if (amigo + forca(&l->meu, M_CIVIL) >= hostil) { melhor = l; break; }
	}
	if (!melhor) return;
	if (!casa && forca(&melhor->amigo, M_MILITAR | M_CIVIL) < forca(&melhor->hostil, M_MILITAR | M_TORRES)) return;
	/* los campesinos del lugar, el más cercano a la amenaza primero */
	int n = ia->campones.n;
	TzId *v = malloc((size_t)(n ? n : 1) * sizeof *v);
	uint32_t *d = malloc((size_t)(n ? n : 1) * sizeof *d);
	if (!v || !d) { free(v); free(d); return; }
	int q[4];
	ia_rect(ia, ameaca, q);
	int nv = 0;
	for (int j = 0; j < n; ++j) {
		TzId i = ia->campones.v[j];
		const TzObj *o = &ia->m->obj[i];
		if (!o->ator || (o->flags & (TZ_F_HISTORIA | TZ_F_MORRENDO | TZ_F_DENTRO)) || !setor_dentro(melhor->rect, o->cx / 8, o->cy / 8)) continue;
		if (o->ator->ordem.tipo == TZ_O_ATACAR || o->ator->ordem.tipo == TZ_O_CAPTURAR) continue;
		int r[4];
		ia_rect(ia, i, r);
		uint32_t dd = ia_dist_rect(r, q);
		int b = nv++;
		while (b > 0 && d[b - 1] > dd) { v[b] = v[b - 1]; d[b] = d[b - 1]; b--; }
		v[b] = i; d[b] = dd;
	}
	uint32_t s = ia_forca(t), soma = 0;
	/* para capturar cuenta cada campesino uno, y se buscan resistencia/15 + 1 y luego resistencia/10 + 1 */
	uint32_t n1 = cap ? (uint32_t)t->resistencia / 15 + 1 : s * 3 / 2, n2 = cap ? (uint32_t)t->resistencia / 10 + 1 : s * 2;
	for (int j = 0; j < nv; ++j) {
		soma += cap ? 1 : ia_forca(&ia->m->obj[v[j]]);
		if (soma >= n1) break;
	}
	for (int j = 0; j < nv; ++j) {
		ia_ordem(ia, v[j], cap ? TZ_O_CAPTURAR : TZ_O_ATACAR, ameaca, t->cx, t->cy);
		soma += cap ? 1 : ia_forca(&ia->m->obj[v[j]]);
		if (soma >= n2) break;
	}
	if (cap) ia->contas_d470.capturas++;
	free(v);
	free(d);
}

/* ------------------------------------------------------------------ unidades ociosas (idle.rs 0x4140b0) */
static int item_luta(int c) { return c == IC_ITEM_PLUSPLUS || c == IC_ITEM_MAGIC || c == IC_ITEM_HEALTH; }

void ia_ocioso_unidade(TzIa *ia, TzId i, uint32_t ms)
{
	const TzObj *o = &ia->m->obj[i];
	if (ia_militar(ia, o) && ia->m->regras.dificuldade != 0) {
		/* el objeto más cercano a 16 */
		TzId item = TZ_NENHUM;
		uint32_t di = 0;
		TzId v[64];
		int n = tz_mundo_area(ia->m, o->cx - ALCANCE_ITEM, o->cy - ALCANCE_ITEM, o->cx + ALCANCE_ITEM, o->cy + ALCANCE_ITEM, TZ_FILTRO_TODOS, 0, v, 64);
		for (int j = 0; j < n; ++j) {
			const TzObj *t = &ia->m->obj[v[j]];
			if (!item_luta(t->classe) || !ia_ve(ia, v[j])) continue;
			int r[4];
			ia_rect(ia, v[j], r);
			uint32_t d = ia_dist(o->cx, o->cy, r);
			if (d <= ALCANCE_ITEM && (item == TZ_NENHUM || d < di)) { item = v[j]; di = d; }
		}
		if (item != TZ_NENHUM) { ia_ordem(ia, i, TZ_O_PEGAR_ITEM, item, ia->m->obj[item].cx, ia->m->obj[item].cy); return; }
		/* 0x41429d: de los 3x3 sectores alrededor, el de más fuerza hostil; ninguno si el suyo tiene */
		int sx = o->cx / 8, sy = o->cy / 8, bx = -1, by = -1, aqui = 0;
		uint32_t melhor = 0;
		for (int x = sx - 1; x <= sx + 1; ++x)
			for (int y = sy - 1; y <= sy + 1; ++y) {
				if (x < 0 || y < 0 || x * 8 >= ia->m->lado || y * 8 >= ia->m->lado) continue;
				uint32_t s = 0;
				for (int p = 0; p < ia->nlugares; ++p)
					if (setor_dentro(ia->lugar[p].rect, x, y)) s += forca(&ia->lugar[p].hostil, 0xff);
				if (s && x == sx && y == sy) aqui = 1;
				if (s > melhor) { melhor = s; bx = x; by = y; }
			}
		if (!aqui && bx >= 0) { ia_ordem(ia, i, TZ_O_MOVER, TZ_NENHUM, bx * 8 + 4, by * 8 + 4); return; }
	}
	/* 0x413f50: herido, al castillo o puesto más cercano a 24 */
	if (o->vida < o->vida_max) {
		TzId ref = TZ_NENHUM;
		uint32_t dr = 0;
		for (TzId j = ia->m->nobj - 1; j >= 0; --j) {
			const TzObj *c = &ia->m->obj[j];
			if (c->dono != ia->k || !ia_contado(c) || ia_obra(c) || (c->classe != TZC_CASTLE && c->classe != IC_OUTPOST)) continue;
			const TzAtributos *a = tz_obj_at(ia->m, c);
			if (a && c->ndentro >= a->capacidade) continue;
			int r[4];
			ia_rect(ia, j, r);
			uint32_t d = ia_dist(o->cx, o->cy, r);
			if (d <= ALCANCE_REFUGIO && (ref == TZ_NENHUM || d < dr || (d == dr && c->classe == IC_OUTPOST))) { ref = j; dr = d; }
		}
		if (ref != TZ_NENHUM) ia_ordem(ia, i, TZ_O_ENTRAR, ref, ia->m->obj[ref].cx, ia->m->obj[ref].cy);
	}
}

/* ------------------------------------------------------------------ la fase especial (0x409220) */
/* 0x4107d0: torres de madera a madera 2; más allá de fácil, madera 2 a piedra (o piedra 2 con una de cada tres) */
static void torres(TzIa *ia)
{
	int visto = 0;
	for (int j = 0; j < ia->torres.n; ++j) {
		TzId t = ia->torres.v[j];
		const TzObj *o = &ia->m->obj[t];
		if (!ia_contado(o) || ia_obra(o) || o->classe == 39 || o->classe == 42 || o->classe == 43) continue;
		if (o->ator && o->ator->ntrab && o->ator->trab[0].tipo == TZ_J_MELHORAR) continue;
		const char *nome;
		int bit;
		if (o->classe == IC_TOWER_WOOD) { nome = "to_wtower2"; bit = 1; }
		else {
			if (!ia->m->regras.dificuldade) continue;
			int k2 = tz_ia_bib_tipo(ia->b, "to_stower2_1");
			if (k2 >= 0 && ia_disponivel_tipo(ia, k2) && ia_rng(ia, 0, 2) == 0) { nome = "to_stower2_1"; bit = 4; }
			else { nome = "to_stower"; bit = 2; }
		}
		if (visto & bit) continue;
		visto |= bit;
		int tipo = tz_ia_bib_tipo(ia->b, nome);
		if (tipo < 0 || !ia_disponivel_tipo(ia, tipo)) continue;
		int32_t c[4];
		ia_custo(ia, tipo, c);
		if (ia_pagavel(ia, c, B_DEFESA, 1, 0)) {
			int tec = ia_tec(ia, tipo);
			if (tec >= 0 && !ia_pesquisar(ia, t, tec)) ia_pagar(ia, c);
			return;
		}
		ia_reservar(ia, B_DEFESA, c, 1);
		if (visto == 7) break;
	}
}

static int erguer(TzIa *ia, int tipo, int cx, int cy, uint32_t alcance, int primeiro)
{
	int32_t c[4];
	ia_custo(ia, tipo, c);
	if (primeiro && ia_pagavel(ia, c, B_DEFESA, 1, 0)) return ia_colocar(ia, tipo, cx, cy, 1, alcance, TZ_NENHUM);
	ia_reservar(ia, B_DEFESA, c, 1);
	return 0;
}

static uint32_t de_pe_no(const TzIa *ia, const TzIaLista *l, const int16_t r[4])
{
	uint32_t n = 0;
	for (int j = 0; j < l->n; ++j) {
		const TzObj *o = &ia->m->obj[l->v[j]];
		if (setor_dentro(r, o->cx / 8, o->cy / 8)) n++;
	}
	return n;
}

/* 0x4104f0: lo que pide un lugar: una torre de madera, cuarteles, y en difícil un puesto y un taller */
static int defesa(TzIa *ia, const TzIaLugar *l, int primeiro)
{
	if (!(l->flags & 3)) return 0;
	uint32_t ntorres = l->meu.c[CAT_TORRES].n + l->meu_vindo.c[CAT_TORRES].n;
	uint32_t nquarteis = l->meu.c[CAT_QUARTEIS].n + l->meu_vindo.c[CAT_QUARTEIS].n;
	/* las torres en obras también cuentan */
	for (int j = 0; j < ia->torres.n; ++j) {
		const TzObj *o = &ia->m->obj[ia->torres.v[j]];
		if (ia_obra(o) && dentro(l->rect, o->cx, o->cy)) ntorres++;
	}
	int cx = (l->rect[0] + l->rect[2]) / 2, cy = (l->rect[1] + l->rect[3]) / 2;
	uint32_t alcance = (uint32_t)(cx - l->rect[0]);
	uint32_t limite = 1;
	if (l->tipo == L_CASA && l->param == (uint32_t)ia->k) {
		limite = TORRES_CASA_1;
		if (ia->m->regras.dificuldade && ntorres >= TORRES_CASA_1) limite = TORRES_CASA_2;
	}
	int feito = 0;
	if ((l->flags & 1) && ntorres < limite) feito = erguer(ia, IC_TOWER_WOOD, cx, cy, alcance, primeiro);
	if (!(l->flags & 2) || feito) return feito;
	if (!ia->quarteis.n) return 0;
	if (nquarteis < 2) return erguer(ia, TZC_BARRACK, cx, cy, alcance, primeiro);
	if (ia->m->regras.dificuldade != 2) return 0;
	if (!de_pe_no(ia, &ia->postos, l->rect)) return erguer(ia, IC_OUTPOST, cx, cy, alcance, primeiro);
	if (!de_pe_no(ia, &ia->oficinas, l->rect)) return erguer(ia, IC_WORKSHOP, cx, cy, alcance, primeiro);
	return 0;
}

/* la exploración (nuestra): una unidad de combate, o un campesino si no hay, va al sector sin ver más cercano */
void ia_explorar(TzIa *ia)
{
	TzMundo *m = ia->m;
	if (m->regras.sem_nevoa || m->regras.explorado) return;
	const TzObj *e = ia_obj(ia, ia->explorador);
	if (!e || e->dono != ia->k || !ia_contado(e) || !e->ator) {
		ia->explorador = TZ_NENHUM;
		uint32_t menor = 0;
		for (int j = 0; j < ia->exercito.n; ++j) {
			TzId i = ia->exercito.v[j];
			const TzObj *o = &m->obj[i];
			if (!o->ator || o->ator->ordem.tipo != TZ_O_NADA) continue;
			if (ia->explorador == TZ_NENHUM || ia_forca(o) < menor) { ia->explorador = i; menor = ia_forca(o); }
		}
		/* sin ejército, un campesino cuando ya hay al menos seis */
		if (ia->explorador == TZ_NENHUM && ia->campones.n >= 6) {
			for (int g = G_MADEIRA; g >= G_OCIOSO && ia->explorador == TZ_NENHUM; --g) {
				if (g == G_CONSTRUTOR) continue;
				for (int j = 0; j < ia->grupo[g].n; ++j)
					if (ia_livre(ia, ia->grupo[g].v[j])) { ia->explorador = ia->grupo[g].v[j]; break; }
			}
			if (ia->explorador != TZ_NENHUM) {
				TzId x = ia->explorador;
				for (int g = 0; g < G_N; ++g) ia_lista_tira(&ia->grupo[g], x);
				ia->peao[x].grupo = 0xff;
			}
		}
		e = ia_obj(ia, ia->explorador);
		if (!e) return;
	}
	if (e->ator->ordem.tipo != TZ_O_NADA && e->ator->ordem.tipo != TZ_O_CORTAR && e->ator->ordem.tipo != TZ_O_RECOLHER &&
	    e->ator->ordem.tipo != TZ_O_ENTREGAR) return;
	int n = (m->lado + 7) / 8, bx = -1, by = -1;
	uint32_t dm = 0;
	for (int sy = 0; sy < n; ++sy)
		for (int sx = 0; sx < n; ++sx) {
			int cx = sx * 8 + 4, cy = sy * 8 + 4;
			if (cx >= m->lado) cx = m->lado - 1;
			if (cy >= m->lado) cy = m->lado - 1;
			if (tz_mundo_nevoa(m, ia->k, cx, cy) != 0) continue;
			int r[4] = { cx, cy, cx, cy };
			if (!ia_alcanca(ia, e->cx, e->cy, r)) {
				/* D470: el centro está en el agua o tras un acantilado: vale una casilla del sector en la isla del explorador
				 * (sin esto la costa no se explora nunca y no hay sitio para un puerto) */
				/* (la más cercana al centro, para que este quede a la vista) */
				int ilha = ia_ilha_perto(ia, e->cx, e->cy, 1), achou = 0, ox = cx, oy = cy;
				uint32_t dc = 0;
				for (int yy = sy * 8; ilha && yy < sy * 8 + 8 && yy < m->lado; ++yy)
					for (int xx = sx * 8; xx < sx * 8 + 8 && xx < m->lado; ++xx) {
						if (ia_ilha(ia, xx, yy) != ilha) continue;
						uint32_t d = (uint32_t)((xx - ox) * (xx - ox) + (yy - oy) * (yy - oy));
						if (!achou || d < dc) { cx = xx; cy = yy; dc = d; achou = 1; }
					}
				if (!achou) continue;
			}
			int dx = cx - e->cx, dy = cy - e->cy;
			uint32_t d = (uint32_t)(dx * dx + dy * dy);
			if (bx < 0 || d < dm) { bx = cx; by = cy; dm = d; }
		}
	if (tz_ia_depurar) printf("[DBG]   explorar ia%d: #%d %s en %d,%d -> %d,%d\n", ia->k, ia->explorador, tz_classes[e->classe], e->cx, e->cy, bx, by);
	if (bx >= 0) { ia_ordem(ia, ia->explorador, TZ_O_MOVER, TZ_NENHUM, bx, by); return; }
	/* D470: nada por explorar en su isla: el sector sin ver más cercano al que se llega en barco (transport.rs) */
	if (ia->nagua <= 0 || ia_em_transporte(ia, ia->explorador) || e->ator->ordem.tipo != TZ_O_NADA) return;
	for (int sy = 0; sy < n; ++sy)
		for (int sx = 0; sx < n; ++sx) {
			int cx = sx * 8 + 4, cy = sy * 8 + 4;
			if (cx >= m->lado) cx = m->lado - 1;
			if (cy >= m->lado) cy = m->lado - 1;
			if (tz_mundo_nevoa(m, ia->k, cx, cy) != 0 || !ia_ilha(ia, cx, cy)) continue;
			int dx = cx - e->cx, dy = cy - e->cy;
			uint32_t d = (uint32_t)(dx * dx + dy * dy);
			if (bx >= 0 && d >= dm) continue;
			int r[4] = { cx, cy, cx, cy };
			if (ia_via_ponto(ia, e->cx, e->cy, r) != IA_VIA_MAR) continue;
			bx = cx; by = cy; dm = d;
		}
	if (bx >= 0) ia_transportar(ia, ia->explorador, bx, by, 1);
}

/* 0x409220: la fase especial */
void ia_fase_especial(TzIa *ia)
{
	ia_gastavel(ia, ia->disponivel);
	memset(ia->baldes[B_NAVAL], 0, sizeof ia->baldes[0]);
	ia_frota(ia);   /* D470: navy.rs */
	if (ia->modo & 2) {
		memset(ia->baldes[B_DEFESA], 0, sizeof ia->baldes[0]);
		torres(ia);
		int primeiro = 1;
		for (int at = 0; at < ia->nlugares; ++at)
			if (defesa(ia, &ia->lugar[at], primeiro)) primeiro = 0;
	}
	ia_explorar(ia);
}
