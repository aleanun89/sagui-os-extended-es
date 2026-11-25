/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ia_brecha.c - las brechas de un reino de la IA en los muros enemigos (D417, fase 6; D470).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later, commit cf2af88):
 * src/gameplay/ai/breach.rs (el gancho de la busca de caminos que marca un tramo de muro y manda el ataque contra él
 * 0x4171e0, las marcas que se quitan cuando cae un muro marcado 0x402c50 y 0x417190).
 * Diferencias (docs/tzar.md, «La IA»): la IA no tiene gancho en la busca de caminos de la simulación; en su lugar, cuando
 * un grupo no llega por tierra a su lugar, la IA mira si llegaría quitando los muros de los reinos con los que está en
 * guerra (islas «sin muros», hechas una vez por mirada) y, si es así, manda a la unidad contra el muro enemigo que toca
 * su isla más cerca del destino (o uno ya marcado a 12 casillas de él), y marca ese muro y los vecinos. El original
 * también abre brecha cuando el camino existe pero es más del doble de largo que la distancia; aquí no.
 */
#include "ia_int.h"

#define MIRA 12          /* breach.rs LOOK */
#define MARCA 1          /* breach.rs MARK */
#define LIMPA 16         /* breach.rs CLEAR */

static int muro(int c) { return c == TZC_WALL_STONE || c == TZC_WALL_WOOD; }

/* el muro de un reino al que el nuestro ataca solo (relación de guerra, 0x468190) */
static int muro_hostil(const TzIa *ia, TzId j)
{
	const TzObj *o = ia_obj(ia, j);
	if (!o || !ia_contado(o) || !ia_ve(ia, j)) return 0;
	if (!muro(o->classe) && o->classe != TZC_GATE_WOOD && o->classe != TZC_GATE_STONE) return 0;
	return o->dono >= 1 && o->dono <= TZ_REINOS && tz_mundo_relacao(ia->m, ia->k, o->dono) < 0;
}

/* las islas sin los muros y puertas enemigos: relleno como tz_ilhas (caminho.c), una vez por mirada */
static void fazer_sem_muros(TzIa *ia)
{
	if (ia->sem_muros && ia->sem_muros_olhar == ia->contas.olhares) return;
	TzMundo *m = ia->m;
	int n = m->lado;
	if (!ia->sem_muros) { ia->sem_muros = calloc((size_t)n * n, sizeof *ia->sem_muros); if (!ia->sem_muros) return; }
	ia->sem_muros_olhar = ia->contas.olhares;
	ia->tem_muros = 0;
	memset(ia->sem_muros, 0, (size_t)n * n * sizeof *ia->sem_muros);
	int32_t *pilha = malloc((size_t)n * n * sizeof *pilha);
	if (!pilha) return;
	uint8_t *passa = malloc((size_t)n * n);
	if (!passa) { free(pilha); return; }
	for (int i = 0; i < n * n; ++i) {
		const TzCelula *c = &m->cel[i];
		passa[i] = m->cel[i].ilha != 0 && !(c->bits & TZ_CEL_AGUA && !(c->bits & TZ_CEL_PONTE));
		if (!passa[i] && (c->bits & (TZ_CEL_MURO | TZ_CEL_PORTA)) && c->fixo >= 0 && muro_hostil(ia, c->fixo)) { passa[i] = 1; ia->tem_muros = 1; }
	}
	uint16_t prox = 1;
	for (int i = 0; i < n * n; ++i) {
		if (ia->sem_muros[i] || !passa[i]) continue;
		int np = 0;
		pilha[np++] = i;
		ia->sem_muros[i] = prox;
		while (np) {
			int c = pilha[--np], x = c % n, y = c / n;
			for (int dy = -1; dy <= 1; ++dy)
				for (int dx = -1; dx <= 1; ++dx) {
					int nx = x + dx, ny = y + dy;
					if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= n || ny >= n) continue;
					int j = ny * n + nx;
					if (ia->sem_muros[j] || !passa[j]) continue;
					ia->sem_muros[j] = prox;
					pilha[np++] = j;
				}
		}
		if (prox < 65535) ++prox;
	}
	free(passa);
	free(pilha);
}

static int sm(const TzIa *ia, int x, int y)
{
	if (!ia->sem_muros || x < 0 || y < 0 || x >= ia->m->lado || y >= ia->m->lado) return 0;
	return ia->sem_muros[y * ia->m->lado + x];
}

/* ¿llegaría la unidad de (fx, fy) al rectángulo quitando los muros enemigos? */
int ia_brecha_alcanca(TzIa *ia, int fx, int fy, const int r[4])
{
	int de = ia_ilha_perto(ia, fx, fy, 1);
	if (!de) return 0;
	fazer_sem_muros(ia);
	if (!ia->tem_muros) return 0;   /* sin muros enemigos a la vista no hay brecha que abrir */
	int s = sm(ia, fx, fy);
	for (int d = 1; d <= 1 && !s; ++d)
		for (int y = fy - d; y <= fy + d && !s; ++y)
			for (int x = fx - d; x <= fx + d && !s; ++x) s = sm(ia, x, y);
	if (!s) return 0;
	for (int y = r[1] - 1; y <= r[3] + 1; ++y)
		for (int x = r[0] - 1; x <= r[2] + 1; ++x)
			if (sm(ia, x, y) == s) return 1;
	return 0;
}

static int marcado(const TzIa *ia, TzId j)
{
	for (int q = 0; q < ia->nbrechas; ++q) if (ia->brecha[q].muro == j) return 1;
	return 0;
}

static void marcar_muro(TzIa *ia, TzId j)
{
	if (marcado(ia, j) || ia->nbrechas >= IA_BRECHAS) return;
	ia->brecha[ia->nbrechas].muro = j;
	ia->brecha[ia->nbrechas].x = ia->m->obj[j].cx;
	ia->brecha[ia->nbrechas].y = ia->m->obj[j].cy;
	ia->nbrechas++;
}

/* la unidad ataca un muro marcado (el programa de prioridad 4000 del original: no es candidata para la guerra) */
int ia_brecha_de(const TzIa *ia, TzId i)
{
	if (!ia->nbrechas) return 0;
	const TzObj *o = ia_obj(ia, i);
	if (!o || !o->ator || o->ator->ordem.tipo != TZ_O_ATACAR) return 0;
	return marcado(ia, o->ator->ordem.alvo);
}

/* 0x4171e0: manda a la unidad contra el muro de la brecha hacia (px, py); 1 si la mandó */
int ia_brecha_mandar(TzIa *ia, TzId u, int px, int py)
{
	const TzObj *o = ia_obj(ia, u);
	if (!o || !o->ataque) return 0;
	int r[4] = { px, py, px, py };
	if (!ia_brecha_alcanca(ia, o->cx, o->cy, r)) return 0;
	int de = ia_ilha_perto(ia, o->cx, o->cy, 1), s = sm(ia, px, py);
	/* el muro enemigo que toca la isla de la unidad y la región sin muros del destino, el más cercano al destino */
	TzId melhor = TZ_NENHUM;
	uint32_t dm = 0;
	for (TzId j = 0; j < ia->m->nobj; ++j) {
		if (!muro(ia->m->obj[j].classe) || !muro_hostil(ia, j)) continue;
		int q[4];
		ia_rect(ia, j, q);
		int toca = 0;
		for (int y = q[1] - 1; y <= q[3] + 1 && !toca; ++y)
			for (int x = q[0] - 1; x <= q[2] + 1 && !toca; ++x) toca = ia_ilha(ia, x, y) == de;
		if (!toca || (s && sm(ia, q[0], q[1]) != s)) continue;
		uint32_t d = ia_dist(px, py, q);
		if (melhor == TZ_NENHUM || d < dm) { melhor = j; dm = d; }
	}
	if (melhor == TZ_NENHUM) return 0;
	/* uno ya marcado a 12 casillas; si no hay, se marcan ese y los vecinos */
	TzId alvo = TZ_NENHUM;
	const TzObj *w = &ia->m->obj[melhor];
	for (int q = 0; q < ia->nbrechas && alvo == TZ_NENHUM; ++q) {
		const TzIaBrecha *b = &ia->brecha[q];
		int dx = b->x - w->cx, dy = b->y - w->cy;
		if (dx < 0) dx = -dx;
		if (dy < 0) dy = -dy;
		if (dx <= MIRA && dy <= MIRA && muro_hostil(ia, b->muro)) {
			/* tiene que tocar la isla de la unidad para poder golpearlo */
			int q4[4], toca = 0;
			ia_rect(ia, b->muro, q4);
			for (int y = q4[1] - 1; y <= q4[3] + 1 && !toca; ++y)
				for (int x = q4[0] - 1; x <= q4[2] + 1 && !toca; ++x) toca = ia_ilha(ia, x, y) == de;
			if (toca) alvo = b->muro;
		}
	}
	if (alvo == TZ_NENHUM) {
		alvo = melhor;
		TzId v[32];
		int n = tz_mundo_area(ia->m, w->cx - MARCA, w->cy - MARCA, w->cx + MARCA, w->cy + MARCA, TZ_FILTRO_EDIFICIOS, -1, v, 32);
		marcar_muro(ia, melhor);
		for (int q = 0; q < n; ++q) if (muro(ia->m->obj[v[q]].classe) && muro_hostil(ia, v[q])) marcar_muro(ia, v[q]);
	}
	if (ia_ordem(ia, u, TZ_O_ATACAR, alvo, ia->m->obj[alvo].cx, ia->m->obj[alvo].cy)) return 0;
	ia->contas_d470.brechas++;
	return 1;
}

/* 0x402c50 con 0x417190: un muro marcado que cae quita las marcas de los muros a 16 casillas */
void ia_brechas_olhar(TzIa *ia)
{
	for (int q = 0; q < ia->nbrechas;) {
		const TzObj *o = ia_obj(ia, ia->brecha[q].muro);
		if (o && ia_contado(o)) { q++; continue; }
		int x = ia->brecha[q].x, y = ia->brecha[q].y;
		ia->brecha[q] = ia->brecha[--ia->nbrechas];
		for (int p = 0; p < ia->nbrechas;) {
			int dx = ia->brecha[p].x - x, dy = ia->brecha[p].y - y;
			if (dx < 0) dx = -dx;
			if (dy < 0) dy = -dy;
			if (dx <= LIMPA && dy <= LIMPA) ia->brecha[p] = ia->brecha[--ia->nbrechas];
			else p++;
		}
		q = 0;
	}
}
