/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ia_economia.c - el trabajo de un reino de la IA con sus recursos: cuántos recolectores quiere de cada uno y adónde
 * los manda, los almacenes junto al trabajo, la busca de un bosque y los campesinos ociosos (D417, fase 6).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): src/gameplay/ai/harvest.rs (fase de
 * recolectar 0x40dd90 con 0x40e7a0, 0x40eab0, 0x40f370 y 0x40f880; fase de almacenes 0x40dff0 con 0x40e150 y 0x40f0a0;
 * el bosque 0x40d5c0 con su prueba 0x40d790) e idle.rs (el campesino ocioso 0x4153a0).
 * Diferencias: la busca del bosque mira 1000 casillas en cada fase que la necesita en vez de cada 500 ms y se queda con
 * el primer árbol bueno; los campos nuevos los empieza un campesino con el comando de construir.
 */
#include "ia_int.h"
#include <stdio.h>

#define MINEIROS 10          /* una mina recibe tantos antes de buscar la siguiente (0x40df14) */
#define LENHADORES 4         /* leñadores a un árbol a la vez (0x40df22) */
#define PERTO 8              /* un almacén así de cerca basta (0x40e216) */
#define ALCANCE_ARMAZEM 16   /* se busca uno así de lejos (0x40f3b2) */
#define ARVORES_SERRARIA 8   /* 0x40ecd4 */
#define ARVORES_BOSQUE 10    /* 0x40ede0 */
#define CASILLAS_BOSQUE 1000 /* 0x40d74d */
#define ALCANCE_OBRA 21      /* idle.rs SITE_REACH */
#define ALCANCE_ITEM 16      /* idle.rs ITEM_REACH */
#define ALCANCE_REPARO 15    /* idle.rs REPAIR_REACH */

static int quantos_trabalham(const TzIa *ia, int r, TzId j)
{
	int n = 0;
	const TzIaLista *g = &ia->grupo[G_OURO + r];
	for (int i = 0; i < g->n; ++i) if (ia->peao[g->v[i]].alvo == j) n++;
	return n;
}

/* 0x40e6c0: lo que las partes reservan de un recurso que aún se puede recolectar */
static int32_t precisa(const TzIa *ia, int r)
{
	if (ia->bloqueado[r]) return 0;
	uint32_t s = 0;
	for (int b = 0; b < B_N; ++b) s += (uint32_t)ia->baldes[b][r];
	return (int32_t)s;
}

/* 0x40e7a0: los recolectores que quiere de cada recurso, repartidos por lo que le falta de cada uno */
static void querer(TzIa *ia)
{
	/* el orden por necesidad (en centenas, el de número mayor primero en los empates) */
	for (int a = 1; a < 4; ++a)
		for (int b = a; b > 0; --b) {
			int x = ia->ordem[b - 1], y = ia->ordem[b];
			int32_t nx = precisa(ia, x) / 100, ny = precisa(ia, y) / 100;
			if (ny > nx || (ny == nx && y > x)) { ia->ordem[b - 1] = (uint8_t)y; ia->ordem[b] = (uint8_t)x; }
			else break;
		}
	uint32_t f, e;
	ia_registro(ia, ia->k, TZC_BARRACK, &f, &e);
	int quarteis = f != 0;
	int32_t peso[4] = { 0, 0, 0, 0 };
	uint16_t gente = 0;
	for (int r = 0; r < 4; ++r) {
		gente = (uint16_t)(gente + ia->grupo[G_OURO + r].n);
		for (int b = 0; b < B_N; ++b) {
			int vezes = 1, por = 1;
			if (b == B_GUIAO) { vezes = 3; por = 2; }
			else if (b == B_DEFESA) por = quarteis ? 1 : 2;
			peso[r] = (int32_t)((uint32_t)peso[r] + (uint32_t)(ia->baldes[b][r] * vezes / por));
		}
	}
	peso[0] = peso[0] * 3 / 2;
	gente = (uint16_t)(gente + ia->grupo[G_OCIOSO].n);
	if (!gente) { memset(ia->quer, 0, sizeof ia->quer); return; }
	uint32_t falta[4], total = 0;
	for (int r = 0; r < 4; ++r) {
		falta[r] = !ia->bloqueado[r] && peso[r] > ia->disponivel[r] ? (uint32_t)(peso[r] - ia->disponivel[r]) : 0;
		total += falta[r];
	}
	uint16_t resto = gente;
	for (int r = 0; r < 4; ++r) {
		if (falta[r] && total) {
			uint32_t n = falta[r] * gente / total;
			resto = (uint16_t)(resto - n);
			ia->quer[r] = (uint16_t)n;
		} else ia->quer[r] = 0;
	}
	uint16_t abertos = 0;
	for (int r = 0; r < 4; ++r) abertos += !ia->bloqueado[r];
	if (!abertos) return;
	while (resto) {
		uint16_t cada = resto / abertos, extra = resto % abertos;
		int at = 0;
		while (extra && at < 4) {
			int r = ia->ordem[at];
			if (!ia->bloqueado[r]) { ia->quer[r] = (uint16_t)(ia->quer[r] + cada + 1); resto = (uint16_t)(resto - cada - 1); extra--; }
			at++;
		}
		for (; at < 4; ++at) {
			int r = ia->ordem[at];
			if (!ia->bloqueado[r]) { ia->quer[r] = (uint16_t)(ia->quer[r] + cada); resto = (uint16_t)(resto - cada); }
		}
	}
}

static void centro(const int r[4], int *x, int *y) { *x = (r[0] + r[2]) / 2; *y = (r[1] + r[3]) / 2; }

/* 0x40f880: n recolectores de r que puede llevarse vuelven a estar ociosos, cada vez el más lejos de su trabajo */
static void soltar(TzIa *ia, int r, int n)
{
	while (n > 0) {
		TzId melhor = TZ_NENHUM;
		uint32_t dm = 0;
		const TzIaLista *g = &ia->grupo[G_OURO + r];
		for (int j = 0; j < g->n; ++j) {
			TzId i = g->v[j];
			if (!ia_livre(ia, i)) continue;
			const TzObj *o = &ia->m->obj[i];
			int p[4] = { o->cx, o->cy, o->cx, o->cy };
			uint32_t d = ia_dist(ia->peao[i].x, ia->peao[i].y, p);
			if (melhor == TZ_NENHUM || d > dm) { melhor = i; dm = d; }
		}
		if (melhor == TZ_NENHUM) return;
		ia_ordem(ia, melhor, TZ_O_PARAR, TZ_NENHUM, 0, 0);
		n--;
	}
}

/* ------------------------------------------------------------------ árboles y bosque */
static int arvore(const TzIa *ia, int x, int y)
{
	if (x < 0 || y < 0 || x >= ia->m->lado || y >= ia->m->lado || !ia->m->cel) return 0;
	const TzCelula *c = &ia->m->cel[y * ia->m->lado + x];
	return (c->bits & TZ_CEL_ARVORE) && c->madeira > 0;
}

static int de_pe(const TzIa *ia, int x, int y)
{
	if (x < 0 || y < 0 || x >= ia->m->lado || y >= ia->m->lado || !ia->m->cel) return 0;
	uint16_t b = ia->m->cel[y * ia->m->lado + x].bits;
	if ((b & (TZ_CEL_AGUA | TZ_CEL_PENHASCO)) && !(b & TZ_CEL_PONTE)) return 0;
	return !(b & (TZ_CEL_ARVORE | TZ_CEL_EDIFICIO | TZ_CEL_MURO));
}

/* 0x44d670: el primer árbol en pie de la inundación desde p dentro de r */
static int arvore_perto(const TzIa *ia, int px, int py, int raio, int *ox, int *oy)
{
	int lado = 2 * raio + 1;
	uint8_t *visto = calloc((size_t)lado * (size_t)lado, 1);
	int16_t *fila = malloc((size_t)lado * (size_t)lado * 2 * sizeof *fila);
	if (!visto || !fila) { free(visto); free(fila); return 0; }
	int ini = 0, fim = 0, chao = 0, achou = 0;
	fila[fim++] = (int16_t)px; fila[fim++] = (int16_t)py;
	visto[raio * lado + raio] = 1;
	while (ini < fim) {
		int x = fila[ini++], y = fila[ini++];
		if (arvore(ia, x, y)) { *ox = x; *oy = y; achou = 1; break; }
		if (de_pe(ia, x, y)) chao = 1;
		else if (chao) continue;
		for (int dy = -1; dy <= 1; ++dy)
			for (int dx = -1; dx <= 1; ++dx) {
				int nx = x + dx, ny = y + dy;
				if ((!dx && !dy) || nx < px - raio || ny < py - raio || nx > px + raio || ny > py + raio) continue;
				if (nx < 0 || ny < 0 || nx >= ia->m->lado || ny >= ia->m->lado) continue;
				uint8_t *v = &visto[(ny - py + raio) * lado + (nx - px + raio)];
				if (*v) continue;
				*v = 1;
				fila[fim++] = (int16_t)nx; fila[fim++] = (int16_t)ny;
			}
	}
	free(visto);
	free(fila);
	return achou;
}

/* 0x40d790: un árbol en pie al que pueden ir leñadores: tres vecinos abiertos, no más de cuatro leñadores, al alcance
 * desde la casa y seguro */
static int arvore_boa(TzIa *ia, int x, int y)
{
	if (!arvore(ia, x, y)) return 0;
	int livres = 0;
	for (int dy = -1; dy <= 1; ++dy)
		for (int dx = -1; dx <= 1; ++dx) {
			int nx = x + dx, ny = y + dy;
			if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= ia->m->lado || ny >= ia->m->lado) continue;
			uint16_t b = ia->m->cel[ny * ia->m->lado + nx].bits;
			if (!(b & (TZ_CEL_AGUA | TZ_CEL_PENHASCO | TZ_CEL_EDIFICIO | TZ_CEL_MURO | TZ_CEL_OCUPADA)) && !arvore(ia, nx, ny)) livres++;
		}
	int lenhadores = 0;
	for (int j = 0; j < ia->grupo[G_MADEIRA].n; ++j) {
		TzId i = ia->grupo[G_MADEIRA].v[j];
		if (ia->peao[i].x == x && ia->peao[i].y == y) lenhadores++;
	}
	int ao[4] = { x - 1, y - 1, x + 1, y + 1 };
	if (livres >= 3 && lenhadores <= LENHADORES && ia_alcanca(ia, ia->casa[0], ia->casa[1], ao) && ia_seguro(ia, x, y, 1, 1))
		return 1;
	if (!ia->floresta.guardado) ia->floresta.guardado = ia->floresta.raio;
	return 0;
}

/* 0x401be0 y 0x401c90: los anillos alrededor de la casa, en el sentido de las agujas del reloj */
static int anel_primeiro(int x0, int y0, int x1, int y1, int lado, int16_t at[2])
{
	if (y0 >= lado || y1 < 0 || x0 >= lado || x1 < 0) return 0;
	if (y0 >= 0) { at[0] = (int16_t)(x0 > 0 ? x0 : 0); at[1] = (int16_t)y0; }
	else if (x1 < lado) { at[0] = (int16_t)x1; at[1] = 0; }
	else if (y1 < lado) { at[0] = (int16_t)(lado - 1); at[1] = (int16_t)y1; }
	else if (x0 >= 0) { at[0] = (int16_t)x0; at[1] = (int16_t)(lado - 1); }
	else return 0;
	return 1;
}

static int anel_seguinte(int x0, int y0, int x1, int y1, int lado, int16_t at[2])
{
	int x = at[0], y = at[1];
	int lado_at;   /* 0 derecha, 1 abajo, 2 izquierda */
	if (y == y0) {
		x++;
		if (x <= x1 && x < lado) { at[0] = (int16_t)x; return 1; }
		x = x1;
		lado_at = 0;
	} else if (x == x1) lado_at = 0;
	else if (y == y1) lado_at = 1;
	else if (x == x0) lado_at = 2;
	else return 0;
	for (;;) {
		if (lado_at == 0) {
			if (x < lado) {
				y = y + 1 > 0 ? y + 1 : 0;
				if (y <= y1 && y < lado) { at[0] = (int16_t)x; at[1] = (int16_t)y; return 1; }
			}
			y = y1;
			lado_at = 1;
		} else if (lado_at == 1) {
			if (y0 == y1) { if (x != x0) return 0; }
			else if (y < lado) {
				x = x - 1 < lado - 1 ? x - 1 : lado - 1;
				if (x >= x0 && x >= 0) { at[0] = (int16_t)x; at[1] = (int16_t)y; return 1; }
				x = x0;
			} else x = x0;
			lado_at = 2;
		} else {
			if (x0 == x1 || x < 0) return 0;
			y = y - 1 < lado - 1 ? y - 1 : lado - 1;
			if (y > y0 && y >= 0) { at[0] = (int16_t)x; at[1] = (int16_t)y; return 1; }
			return 0;
		}
	}
}

void ia_floresta_inicio(TzIa *ia)
{
	ia->floresta.raio = 0; ia->floresta.guardado = 0;
	ia->floresta.at[0] = ia->casa[0]; ia->floresta.at[1] = ia->casa[1];
	ia->floresta.tem_madeira = 0;
}

/* 0x40d5c0: hasta 1000 casillas de los anillos alrededor de la casa; el primer árbol bueno es el bosque */
static int floresta_buscar(TzIa *ia)
{
	if (ia->floresta.nenhuma) return 0;
	int lado = ia->m->lado, cx = ia->casa[0], cy = ia->casa[1];
	for (int n = 0; n < CASILLAS_BOSQUE; ++n) {
		int16_t *at = ia->floresta.at;
		if (at[0] >= 0 && arvore_boa(ia, at[0], at[1])) {
			ia->floresta.tem_madeira = 1;
			ia->floresta.madeira[0] = at[0]; ia->floresta.madeira[1] = at[1];
			return 1;
		}
		int r = ia->floresta.raio;
		if (at[0] >= 0 && anel_seguinte(cx - r, cy - r, cx + r, cy + r, lado, at)) continue;
		ia->floresta.raio++;
		r = ia->floresta.raio;
		if (anel_primeiro(cx - r, cy - r, cx + r, cy + r, lado, at)) continue;
		if (!ia->floresta.guardado) { ia->floresta.nenhuma = 1; at[0] = at[1] = -1; return 0; }
		ia->floresta.raio = ia->floresta.guardado;
		ia->floresta.guardado = 0;
		r = ia->floresta.raio;
		if (!anel_primeiro(cx - r, cy - r, cx + r, cy + r, lado, at)) { at[0] = at[1] = -1; return 0; }
	}
	return 0;
}

static int madeira(TzIa *ia, int *ox, int *oy)
{
	for (int m = 0; m < 2; ++m) {
		const TzIaLista *l = m ? &ia->castelos : &ia->serrarias;
		for (int j = 0; j < l->n; ++j) {
			TzId i = l->v[j];
			if (ia_lista_tem(&ia->nus, i)) continue;
			int r[4], x, y;
			ia_rect(ia, i, r);
			centro(r, &x, &y);
			if (arvore_perto(ia, x, y, ARVORES_SERRARIA, ox, oy)) return 1;
			ia_lista_por(&ia->nus, i);
		}
	}
	if (ia->floresta.tem_madeira) {
		int fx = ia->floresta.madeira[0], fy = ia->floresta.madeira[1];
		if (arvore_boa(ia, fx, fy)) { *ox = fx; *oy = fy; return 1; }
		if (arvore_perto(ia, fx, fy, ARVORES_BOSQUE, ox, oy) && arvore_boa(ia, *ox, *oy)) return 1;
	}
	ia->floresta.tem_madeira = 0;
	if (floresta_buscar(ia)) { *ox = ia->floresta.madeira[0]; *oy = ia->floresta.madeira[1]; return 1; }
	return 0;
}

/* ------------------------------------------------------------------ comida */
static int comida(TzIa *ia, int *ox, int *oy, TzId *sitio)
{
	uint32_t vacas, e;
	ia_registro(ia, ia->k, TZC_COW, &vacas, &e);
	if (vacas && (uint32_t)ia->grupo[G_ORDENHA].n < vacas / 2 + 1) {
		for (TzId i = ia->m->nobj - 1; i >= 0; --i)
			if (ia->m->obj[i].classe == TZC_COW && ia_meu(ia, i) && !(ia->m->obj[i].flags & TZ_F_DENTRO)) {
				*ox = ia->m->obj[i].cx; *oy = ia->m->obj[i].cy; *sitio = i;
				return 1;
			}
	}
	TzId melhor = TZ_NENHUM;
	for (int j = 0; j < ia->campos.n; ++j) {
		TzId f = ia->campos.v[j];
		int farmers = 0;
		for (int g = 0; g < ia->grupo[G_COMIDA].n; ++g) {
			TzId b = ia->grupo[G_COMIDA].v[g];
			if (ia->peao[b].campo && ia->peao[b].alvo == f) farmers++;
		}
		if (farmers >= 1) continue;
		const TzObj *o = &ia->m->obj[f];
		if (!ia_seguro(ia, o->cx, o->cy, 1, 1)) continue;
		if (melhor == TZ_NENHUM || ia->m->obj[melhor].vida >= o->vida) melhor = f;
	}
	if (melhor != TZ_NENHUM) { *ox = ia->m->obj[melhor].cx; *oy = ia->m->obj[melhor].cy; *sitio = melhor; return 1; }
	/* 0x40efeb: un campo nuevo; si no se puede pagar, lo reservan los trabajadores */
	int32_t c[4];
	ia_custo(ia, TZC_FIELD, c);
	if (!ia_pagavel(ia, c, B_TRABALHADORES, 1, 0)) { ia_reservar(ia, B_TRABALHADORES, c, 1); return 0; }
	if (!ia_disponivel_tipo(ia, TZC_FIELD)) return 0;
	ia_colocar(ia, TZC_FIELD, ia->casa[0], ia->casa[1], 0, 50, TZ_NENHUM);
	return 0;   /* el campo existe en el tic siguiente: lo trabaja quien lo empieza */
}

/* 0x40eab0: el siguiente trabajo para recolectores de r tras prev */
static int sitio_para(TzIa *ia, int r, TzId prev, int *ox, int *oy, TzId *sitio)
{
	*sitio = TZ_NENHUM;
	if (r <= 1) {
		TzId reserva = prev;
		int saltar = prev != TZ_NENHUM, bloqueado = 1;
		for (int j = 0; j < ia->minas.n; ++j) {
			TzId i = ia->minas.v[j];
			if (saltar) { if (i == prev) saltar = 0; continue; }
			const TzObj *o = &ia->m->obj[i];
			if ((o->classe == TZC_RES_STONE) != r || !o->recurso) continue;
			bloqueado = 0;
			if (!ia_seguro(ia, o->cx, o->cy, 1, 1)) continue;
			if (quantos_trabalham(ia, r, i) < MINEIROS) { *ox = o->cx; *oy = o->cy; *sitio = i; return 1; }
			reserva = i;
		}
		ia->bloqueado[r] = (uint8_t)bloqueado;
		if (reserva == TZ_NENHUM) return 0;
		*ox = ia->m->obj[reserva].cx; *oy = ia->m->obj[reserva].cy; *sitio = reserva;
		return 1;
	}
	if (r == 3) return madeira(ia, ox, oy);
	return comida(ia, ox, oy, sitio);
}

/* ------------------------------------------------------------------ almacenes */
static int construtor_de(const TzIa *ia, TzId b, int *classe, int r[4])
{
	const TzIaPeao *p = &ia->peao[b];
	const TzObj *s = ia_obj(ia, p->alvo);
	if (s && ia_contado(s) && ia_obra(s)) { *classe = s->classe; ia_rect(ia, p->alvo, r); return 1; }
	return 0;
}

/* 0x40f0a0: el castillo o almacén de r más cerca del trabajo, dentro de max y en su isla; antes la obra de uno que
 * construya un campesino */
static TzId armazem_perto(const TzIa *ia, int r, int ax, int ay, TzId sitio, uint32_t max, int *pronto)
{
	int a[4];
	if (sitio != TZ_NENHUM) { ia_rect(ia, sitio, a); a[0]--; a[1]--; a[2]++; a[3]++; }
	else { a[0] = ax - 1; a[1] = ay - 1; a[2] = ax + 1; a[3] = ay + 1; }
	int tipo = ia_armazem_de(r);
	for (int j = 0; j < ia->grupo[G_CONSTRUTOR].n; ++j) {
		int cl, b[4];
		TzId bi = ia->grupo[G_CONSTRUTOR].v[j];
		if (!construtor_de(ia, bi, &cl, b) || (cl != TZC_CASTLE && cl != tipo)) continue;
		b[0]--; b[1]--; b[2]++; b[3]++;
		if (ia_dist_rect(a, b) <= max) { *pronto = 0; return ia->peao[bi].alvo; }
	}
	TzId melhor = TZ_NENHUM;
	uint32_t dm = 0;
	for (TzId i = ia->m->nobj - 1; i >= 0; --i) {
		const TzObj *o = &ia->m->obj[i];
		if (o->dono != ia->k || !ia_contado(o) || (o->classe != TZC_CASTLE && o->classe != tipo)) continue;
		int b[4];
		ia_rect(ia, i, b);
		b[0]--; b[1]--; b[2]++; b[3]++;
		uint32_t d = ia_dist_rect(a, b);
		int cx, cy;
		centro(a, &cx, &cy);
		if (d > max || !ia_alcanca(ia, cx, cy, b)) continue;
		if (melhor == TZ_NENHUM || d < dm) { melhor = i; dm = d; }
	}
	*pronto = melhor != TZ_NENHUM && !ia_obra(&ia->m->obj[melhor]);
	return melhor;
}

/* 0x40f370: hasta n campesinos ociosos libres, el más cercano primero, al trabajo; minas, campos y vacas quieren un
 * almacén a 16, la madera no. Si fueron todos, y lo que queda de n. */
static int destinar(TzIa *ia, int r, int n, int ax, int ay, TzId sitio, int *resto)
{
	int pronto;
	TzId arm = armazem_perto(ia, r, ax, ay, sitio, ALCANCE_ARMAZEM, &pronto);
	if (arm != TZ_NENHUM && !pronto)
		for (int j = 0; j < ia->grupo[G_CONSTRUTOR].n && n; ++j)
			if (ia->peao[ia->grupo[G_CONSTRUTOR].v[j]].alvo == arm) n--;
	if (r < 2 && sitio == TZ_NENHUM) { *resto = n; return 0; }
	if (r != 3 && arm == TZ_NENHUM) { *resto = n; return 0; }
	int alvo[4];
	if (sitio != TZ_NENHUM) { ia_rect(ia, sitio, alvo); alvo[0]--; alvo[1]--; alvo[2]++; alvo[3]++; }
	else { alvo[0] = ax - 1; alvo[1] = ay - 1; alvo[2] = ax + 1; alvo[3] = ay + 1; }
	while (n) {
		TzId melhor = TZ_NENHUM;
		uint32_t dm = 0;
		for (int j = 0; j < ia->grupo[G_OCIOSO].n; ++j) {
			TzId i = ia->grupo[G_OCIOSO].v[j];
			const TzObj *o = &ia->m->obj[i];
			if (o->ator && o->ator->carga && o->ator->carga_tipo == r) continue;
			if (!ia_livre(ia, i) || !ia_alcanca(ia, o->cx, o->cy, alvo)) continue;
			int q[4];
			ia_rect(ia, i, q);
			uint32_t d = ia_dist(ax, ay, q);
			if (melhor == TZ_NENHUM || d < dm) { melhor = i; dm = d; }
		}
		if (melhor == TZ_NENHUM) { *resto = n; return 0; }
		if (r == 3 || sitio == TZ_NENHUM) ia_ordem(ia, melhor, TZ_O_CORTAR, TZ_NENHUM, ax, ay);
		else ia_ordem(ia, melhor, TZ_O_RECOLHER, sitio, ax, ay);
		n--;
	}
	*resto = 0;
	return 1;
}

/* 0x40dd90: la fase de recolectar */
void ia_fase_colher(TzIa *ia)
{
	querer(ia);
	memset(ia->baldes[B_TRABALHADORES], 0, sizeof ia->baldes[0]);
	for (int r = 0; r < 4; ++r) {
		int tem = ia->grupo[G_OURO + r].n;
		if (tem > ia->quer[r]) soltar(ia, r, tem - ia->quer[r]);
	}
	for (int r = 0; r < 4; ++r) {
		int16_t falta = (int16_t)(uint16_t)(ia->quer[r] - ia->grupo[G_OURO + r].n);
		if (falta <= 0) continue;
		int x, y;
		TzId sitio;
		int tem = sitio_para(ia, r, TZ_NENHUM, &x, &y, &sitio);
		while (tem) {
			int n;
			if (r == 2) n = 1;
			else if (r == 3) n = falta < LENHADORES ? falta : LENHADORES;
			else {
				int t = sitio != TZ_NENHUM ? quantos_trabalham(ia, r, sitio) : 0;
				n = t < MINEIROS ? (falta < MINEIROS - t ? falta : MINEIROS - t) : falta;
			}
			int resto;
			int ok_d = destinar(ia, r, n, x, y, sitio, &resto);
			if (tz_ia_depurar) printf("[DBG]   colher r%d en %d,%d sitio %d: %d de %d (resto %d)\n", r, x, y, sitio, ok_d, n, resto);
			if (!ok_d) { falta = (int16_t)(falta + resto - n); break; }
			falta = (int16_t)(falta - n);
			if (!falta) break;
			tem = sitio_para(ia, r, sitio, &x, &y, &sitio);
		}
		if (falta && r != 3) {
			ia->quer[r] = (uint16_t)(ia->quer[r] - falta);
			ia->quer[3] = (uint16_t)(ia->quer[3] + falta);
		}
	}
}

/* 0x40e150: un almacén para el trabajo en (ax, ay); si se empezó uno */
static int precisa_armazem(TzIa *ia, int r, int ax, int ay, TzId sitio)
{
	int tipo = ia_armazem_de(r);
	int32_t c[4];
	ia_custo(ia, tipo, c);
	TzId arm = TZ_NENHUM;
	int pronto = 0;
	uint32_t d = ALCANCE_ARMAZEM;
	if (r != 3) {
		arm = armazem_perto(ia, r, ax, ay, sitio, ALCANCE_ARMAZEM, &pronto);
		if (arm != TZ_NENHUM && sitio != TZ_NENHUM) {
			int a[4], b[4];
			ia_rect(ia, sitio, a);
			ia_rect(ia, arm, b);
			d = ia_dist_rect(a, b);
			if (pronto && d <= PERTO) return 0;
		}
		int longe = arm == TZ_NENHUM || d > PERTO;
		if (longe && !ia_pagavel(ia, c, B_ARMAZENS, 1, 0)) { ia_reservar(ia, B_ARMAZENS, c, 1); return 0; }
		if (longe && !ia->grupo[G_OURO + r].n) {
			ia_colocar(ia, tipo, ax, ay, 1, arm != TZ_NENHUM ? PERTO : ALCANCE_ARMAZEM, TZ_NENHUM);
			return 1;
		}
	}
	int comecou = 0;
	TzIaLista copia = { 0, 0, 0 };
	for (int j = 0; j < ia->grupo[G_OURO + r].n; ++j) ia_lista_por(&copia, ia->grupo[G_OURO + r].v[j]);
	for (int j = 0; j < copia.n; ++j) {
		TzId g = copia.v[j];
		if (!ia_lista_tem(&ia->grupo[G_OURO + r], g) || !ia_livre(ia, g)) continue;
		int wx = ia->peao[g].x, wy = ia->peao[g].y;
		if (r == 3) {
			ax = wx; ay = wy;
			arm = armazem_perto(ia, 3, wx, wy, TZ_NENHUM, ALCANCE_ARMAZEM, &pronto);
			if (arm != TZ_NENHUM) { int b[4]; ia_rect(ia, arm, b); d = ia_dist(wx, wy, b); }
			else d = ALCANCE_ARMAZEM;
			if (arm != TZ_NENHUM && pronto && d <= PERTO) continue;
			int longe = arm == TZ_NENHUM || d > PERTO;
			if (longe && !ia_pagavel(ia, c, B_ARMAZENS, 1, 1)) { ia_reservar(ia, B_ARMAZENS, c, 1); break; }
		} else if (wx != ax || wy != ay) {
			const TzObj *so = ia_obj(ia, sitio);
			if (!so || ia->peao[g].alvo != sitio) continue;
		}
		if (arm != TZ_NENHUM && d <= PERTO) {
			/* 0x40e535: ayuda a construir el almacén */
			const TzObj *a = &ia->m->obj[arm];
			if (ia_obra(a)) ia_ordem(ia, g, TZ_O_CONSTRUIR, arm, a->cx, a->cy);
			continue;
		}
		comecou = 1;
		if (!ia_colocar(ia, tipo, ax, ay, 1, PERTO, g)) break;
		break;   /* la obra existe en el tic siguiente */
	}
	free(copia.v);
	return comecou;
}

/* 0x40dff0: la fase de almacenes, cada recurso por orden de necesidad; un lugar servido cada vez */
void ia_fase_armazens(TzIa *ia)
{
	memset(ia->baldes[B_ARMAZENS], 0, sizeof ia->baldes[0]);
	for (int at = 0; at < 4; ++at) {
		int r = ia->ordem[at];
		if (r <= 1) {
			for (int j = 0; j < ia->minas.n; ++j) {
				TzId i = ia->minas.v[j];
				if ((ia->m->obj[i].classe == TZC_RES_STONE) != r || !quantos_trabalham(ia, r, i)) continue;
				int q[4], x, y;
				ia_rect(ia, i, q);
				centro(q, &x, &y);
				if (precisa_armazem(ia, r, x, y, i)) break;
			}
		} else if (r == 2) {
			for (int j = 0; j < ia->campos.n; ++j) {
				int q[4], x, y;
				ia_rect(ia, ia->campos.v[j], q);
				centro(q, &x, &y);
				if (precisa_armazem(ia, 2, x, y, ia->campos.v[j])) break;
			}
		} else {
			precisa_armazem(ia, 3, -1, -1, TZ_NENHUM);
		}
	}
}

/* idle.rs 0x4153a0: el campesino ocioso ayuda en una obra cercana, lleva su carga, recoge un objeto de recursos y,
 * ocioso dos periodos, repara lo más cercano */
void ia_ocioso_campones(TzIa *ia, TzId i, uint32_t ms)
{
	int longo = ms >= 2 * ia_periodo(ia);
	const TzObj *o = &ia->m->obj[i];
	int eu[4];
	ia_rect(ia, i, eu);
	TzId melhor = TZ_NENHUM;
	uint32_t limite = ALCANCE_OBRA;
	for (int j = 0; j < ia->obras.n; ++j) {
		TzId s = ia->obras.v[j];
		int r[4];
		ia_rect(ia, s, r);
		uint32_t d = ia_dist_rect(r, eu);
		if (d >= limite) continue;
		int construtores = 0;
		for (int b = 0; b < ia->grupo[G_CONSTRUTOR].n; ++b) if (ia->peao[ia->grupo[G_CONSTRUTOR].v[b]].alvo == s) construtores++;
		if (construtores <= (longo ? 3 : 1)) { melhor = s; limite = d; }
	}
	if (melhor != TZ_NENHUM) {
		ia_ordem(ia, i, TZ_O_CONSTRUIR, melhor, ia->m->obj[melhor].cx, ia->m->obj[melhor].cy);
		return;
	}
	if (o->ator && o->ator->carga) {
		int tipo = ia_armazem_de(o->ator->carga_tipo);
		TzId arm = TZ_NENHUM;
		uint32_t dm = 0;
		for (TzId j = ia->m->nobj - 1; j >= 0; --j) {
			const TzObj *s = &ia->m->obj[j];
			if (s->dono != ia->k || !ia_contado(s) || ia_obra(s) || (s->classe != TZC_CASTLE && s->classe != tipo)) continue;
			int r[4];
			ia_rect(ia, j, r);
			int ao[4] = { r[0] - 1, r[1] - 1, r[2] + 1, r[3] + 1 };
			if (!ia_alcanca(ia, o->cx, o->cy, ao)) continue;
			uint32_t d = ia_dist_rect(eu, r);
			if (arm == TZ_NENHUM || d < dm) { arm = j; dm = d; }
		}
		if (arm != TZ_NENHUM) { ia_ordem(ia, i, TZ_O_ENTREGAR, arm, ia->m->obj[arm].cx, ia->m->obj[arm].cy); return; }
	}
	TzId item = TZ_NENHUM;
	uint32_t di = 0;
	for (TzId j = 0; j < ia->m->nobj; ++j) {
		const TzObj *t = &ia->m->obj[j];
		if (t->classe != IC_RESITEM || !ia_contado(t) || (t->flags & TZ_F_DENTRO) || !ia_ve(ia, j)) continue;
		int r[4];
		ia_rect(ia, j, r);
		uint32_t d = ia_dist(o->cx, o->cy, r);
		if (d <= ALCANCE_ITEM && (item == TZ_NENHUM || d < di)) { item = j; di = d; }
	}
	if (item != TZ_NENHUM) { ia_ordem(ia, i, TZ_O_RECOLHER, item, ia->m->obj[item].cx, ia->m->obj[item].cy); return; }
	if (!longo) return;
	TzId rep = TZ_NENHUM;
	int32_t dr = 0;
	for (TzId j = 0; j < ia->m->nobj; ++j) {
		const TzObj *t = &ia->m->obj[j];
		if (!tz_classe_e_edificio(t->classe) || !ia_contado(t) || ia_obra(t) || t->vida >= t->vida_max) continue;
		if (tz_mundo_relacao(ia->m, ia->k, t->dono) <= 0 || !ia_ve(ia, j)) continue;
		int r[4];
		ia_rect(ia, j, r);
		int cx = (r[0] + r[2]) / 2 - o->cx, cy = (r[1] + r[3]) / 2 - o->cy;
		if (cx < -ALCANCE_REPARO || cx > ALCANCE_REPARO || cy < -ALCANCE_REPARO || cy > ALCANCE_REPARO) continue;
		int32_t d = cx * cx + cy * cy;
		if (rep == TZ_NENHUM || d < dr) { rep = j; dr = d; }
	}
	if (rep != TZ_NENHUM) ia_ordem(ia, i, TZ_O_REPARAR, rep, ia->m->obj[rep].cx, ia->m->obj[rep].cy);
}
