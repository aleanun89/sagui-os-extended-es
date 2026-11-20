/* SPDX-License-Identifier: GPL-3.0-or-later */
/* caminho.c - búsqueda de caminos por casillas (D417, fase 5).
 *
 * Traducido a C (simplificado) de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later),
 * gameplay/pathing.rs: el A* por casillas de Tzar.exe (0x4a9060) con sus costes (16 recto, 22 en diagonal), el
 * terreno de cada capa (tierra, agua, aire), los árboles, lo fijo, los muros, las puertas y los puentes, y el destino
 * como un rectángulo con alcance. El grafo de regiones de 8x8 de ReTzar se sustituye por las «islas» (regiones
 * conexas a pie, TzCelula.ilha): si el destino está en otra isla, se va a la casilla más cercana alcanzable.
 * Determinista: el montículo desempata por orden de inserción.
 */
#include "mundo_int.h"
#include <stdlib.h>
#include <string.h>

typedef struct { uint32_t f, seq; int32_t i; } Nodo;

static void sobe(Nodo *h, int k)
{
	while (k > 0) {
		int p = (k - 1) / 2;
		if (h[p].f < h[k].f || (h[p].f == h[k].f && h[p].seq < h[k].seq)) break;
		Nodo t = h[p]; h[p] = h[k]; h[k] = t; k = p;
	}
}

static void desce(Nodo *h, int n, int k)
{
	for (;;) {
		int a = 2 * k + 1, b = a + 1, m = k;
		if (a < n && (h[a].f < h[m].f || (h[a].f == h[m].f && h[a].seq < h[m].seq))) m = a;
		if (b < n && (h[b].f < h[m].f || (h[b].f == h[m].f && h[b].seq < h[m].seq))) m = b;
		if (m == k) return;
		Nodo t = h[m]; h[m] = h[k]; h[k] = t; k = m;
	}
}

/* ¿puede una unidad de la capa entrar en la casilla (sin contar otras unidades)? */
int tz_cel_transitavel(const TzMundo *m, int capa, int dono, int x, int y)
{
	if (x < 0 || y < 0 || x >= m->lado || y >= m->lado) return 0;
	const TzCelula *c = &m->cel[y * m->lado + x];
	if (capa == TZ_CAPA_AR) return 1;
	if (capa == TZ_CAPA_AGUA) return (c->bits & TZ_CEL_AGUA) && !(c->bits & (TZ_CEL_EDIFICIO | TZ_CEL_PONTE));
	if (c->bits & TZ_CEL_PORTA) {
		/* la puerta abre para su reino y sus aliados (gate_tests.rs) */
		const TzObj *g = c->fixo >= 0 ? &m->obj[c->fixo] : NULL;
		return g && tz_mundo_relacao(m, dono, g->dono) > 0;
	}
	if (c->bits & TZ_CEL_PONTE) return 1;
	if (c->bits & (TZ_CEL_AGUA | TZ_CEL_COSTA | TZ_CEL_PENHASCO | TZ_CEL_ARVORE | TZ_CEL_EDIFICIO | TZ_CEL_MURO)) return 0;
	return 1;
}

static int ocupada(const TzMundo *m, int capa, int x, int y, TzId eu)
{
	const TzCelula *c = &m->cel[y * m->lado + x];
	if (capa == TZ_CAPA_AR) return 0;
	return c->ocupante >= 0 && c->ocupante != eu;
}

/* distancia de Chebyshev de una casilla a un rectángulo (0 = dentro) */
int tz_dist_ret(int x, int y, const int r[4])
{
	int dx = x < r[0] ? r[0] - x : x > r[2] ? x - r[2] : 0;
	int dy = y < r[1] ? r[1] - y : y > r[3] ? y - r[3] : 0;
	return dx > dy ? dx : dy;
}

static uint32_t heur(int x, int y, const int r[4], int alc)
{
	int dx = x < r[0] ? r[0] - x : x > r[2] ? x - r[2] : 0;
	int dy = y < r[1] ? r[1] - y : y > r[3] ? y - r[3] : 0;
	dx -= alc; dy -= alc;
	if (dx < 0) dx = 0;
	if (dy < 0) dy = 0;
	int mn = dx < dy ? dx : dy, mx = dx < dy ? dy : dx;
	return (uint32_t)(mn * 22 + (mx - mn) * 16);
}

static const int DX[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
static const int DY[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };

/* A*: de (x0, y0) a cualquier casilla a distancia <= alc del rectángulo r. evita = radio alrededor del origen en el
 * que las unidades paradas cuentan como obstáculo (0 = ninguna). Escribe el camino (sin el origen) en el ator y
 * devuelve 1 si llega, 0 si se queda en la casilla más cercana, -1 si no hay ni un paso posible. */
int tz_caminho(TzMundo *m, TzId u, int x0, int y0, const int r[4], int alc, int evita)
{
	TzPriv *p = m->priv;
	TzObj *o = &m->obj[u];
	int capa = tz_obj_capa(m, o), n = m->lado;
	if (++p->marca == 0) { memset(p->visto, 0, (size_t)n * n * sizeof *p->visto); p->marca = 1; }
	int nh = 0;
	uint32_t seq = 0;
	int ini = y0 * n + x0;
	p->visto[ini] = p->marca; p->g[ini] = 0; p->pai[ini] = -1; p->fechado[ini] = 0;
	((Nodo *)p->heap)[nh++] = (Nodo){ heur(x0, y0, r, alc), seq++, ini };
	int melhor = ini; uint32_t hmelhor = heur(x0, y0, r, alc), gmelhor = 0;
	int achou = -1, expand = 0;
	int lim = p->limite_nos;
	int parcial = 0, rr[4] = { r[0], r[1], r[2], r[3] };
	if (capa == TZ_CAPA_TERRA && m->cel[ini].ilha) {
		/* si ninguna casilla útil del destino está en nuestra isla, el A* recorrería toda la isla para nada: se
		 * busca en espiral la casilla de la isla más cercana al destino y se va allí (como el grafo de regiones) */
		int ilha = m->cel[ini].ilha, alguma = 0;
		for (int yy = r[1] - alc; yy <= r[3] + alc && !alguma; ++yy)
			for (int xx = r[0] - alc; xx <= r[2] + alc; ++xx)
				if (xx >= 0 && yy >= 0 && xx < n && yy < n && m->cel[yy * n + xx].ilha == ilha) { alguma = 1; break; }
		if (!alguma) {
			int cx = (r[0] + r[2]) / 2, cy = (r[1] + r[3]) / 2, ok = 0;
			for (int d = 1; d <= 64 && !ok; ++d)
				for (int dy = -d; dy <= d && !ok; ++dy)
					for (int dx = -d; dx <= d; ++dx) {
						if (dx != -d && dx != d && dy != -d && dy != d) continue;
						int xx = cx + dx, yy = cy + dy;
						if (xx < 0 || yy < 0 || xx >= n || yy >= n || m->cel[yy * n + xx].ilha != ilha) continue;
						rr[0] = rr[2] = xx; rr[1] = rr[3] = yy; ok = 1; break;
					}
			if (!ok) lim = 2000;
			else { r = rr; alc = 0; parcial = 1; }
		}
	}
	/* lejos (más de 48 casillas), A* con la heurística doble: muchos menos nodos a cambio de un camino algo peor
	 * (ReTzar guía la búsqueda con su grafo de regiones; esto hace de guía sin él) */
	uint32_t peso = heur(x0, y0, r, alc) > 48 * 16 ? 2 : 1;
	Nodo *h = p->heap;
	while (nh) {
		Nodo c = h[0];
		h[0] = h[--nh];
		desce(h, nh, 0);
		if (p->fechado[c.i]) continue;
		p->fechado[c.i] = 1;
		int x = c.i % n, y = c.i / n;
		/* llegar: una casilla ocupada al final también vale (el paso esperará o se dará por llegado al lado) */
		if (tz_dist_ret(x, y, r) <= alc) { achou = c.i; break; }
		uint32_t hh = heur(x, y, r, alc);
		if (hh < hmelhor || (hh == hmelhor && p->g[c.i] < gmelhor)) { hmelhor = hh; gmelhor = p->g[c.i]; melhor = c.i; }
		if (++expand > lim) break;
		for (int d = 0; d < 8; ++d) {
			int nx = x + DX[d], ny = y + DY[d];
			if (!tz_cel_transitavel(m, capa, o->dono, nx, ny)) continue;
			if (DX[d] && DY[d] && capa != TZ_CAPA_AR &&
			    (!tz_cel_transitavel(m, capa, o->dono, x + DX[d], y) || !tz_cel_transitavel(m, capa, o->dono, x, y + DY[d]))) continue;
			int j = ny * n + nx;
			if (evita) {
				int ddx = nx - x0, ddy = ny - y0;
				if (ddx < 0) ddx = -ddx;
				if (ddy < 0) ddy = -ddy;
				if ((ddx > ddy ? ddx : ddy) <= evita && tz_dist_ret(nx, ny, r) > alc && ocupada(m, capa, nx, ny, u)) {
					const TzObj *q = &m->obj[m->cel[j].ocupante];
					if (!q->ator || q->ator->ncaminho <= q->ator->icaminho) continue;
				}
			}
			uint32_t g = p->g[c.i] + ((DX[d] && DY[d]) ? 22 : 16);
			if (p->visto[j] == p->marca && (p->fechado[j] || p->g[j] <= g)) continue;
			p->visto[j] = p->marca; p->g[j] = g; p->pai[j] = c.i; p->fechado[j] = 0;
			if (nh < p->capheap) { h[nh] = (Nodo){ g + peso * heur(nx, ny, r, alc), seq++, j }; sobe(h, nh); ++nh; }
		}
	}
	int fim = achou >= 0 ? achou : melhor;
	TzAtor *a = o->ator;
	a->ncaminho = a->icaminho = 0;
	if (fim == ini) { a->destino[0] = (int16_t)x0; a->destino[1] = (int16_t)y0; return achou >= 0 && !parcial ? 1 : -1; }
	int len = 0;
	for (int k = fim; k != ini && k >= 0; k = p->pai[k]) ++len;
	if (len > TZ_CAMINHO_MAX) len = TZ_CAMINHO_MAX;
	if (a->capcaminho < len) {
		int cap = len < 32 ? 32 : len;
		void *nv = realloc(a->caminho, (size_t)cap * sizeof *a->caminho);
		if (!nv) return -1;
		a->caminho = nv; a->capcaminho = cap;
	}
	/* del final hacia atrás; si el camino es más largo que el máximo, se guarda el principio */
	int tot = 0;
	for (int k = fim; k != ini && k >= 0; k = p->pai[k]) ++tot;
	int pular = tot - len, i = len;
	for (int k = fim; k != ini && k >= 0; k = p->pai[k]) {
		if (pular > 0) { --pular; continue; }
		--i;
		a->caminho[i][0] = (int16_t)(k % n); a->caminho[i][1] = (int16_t)(k / n);
	}
	a->ncaminho = len;
	a->destino[0] = (int16_t)(fim % n); a->destino[1] = (int16_t)(fim / n);
	return achou >= 0 && !parcial ? 1 : 0;
}

/* islas: regiones conexas a pie (8 vecinos, sin cortar esquinas), por relleno; 0 = no transitable */
void tz_ilhas(TzMundo *m)
{
	TzPriv *p = m->priv;
	int n = m->lado;
	for (int i = 0; i < n * n; ++i) m->cel[i].ilha = 0;
	uint16_t prox = 1;
	int32_t *pilha = p->pai;   /* se reutiliza: no se busca mientras tanto */
	for (int i = 0; i < n * n; ++i) {
		if (m->cel[i].ilha || !tz_cel_transitavel(m, TZ_CAPA_TERRA, 0, i % n, i / n)) {
			/* las puertas cuentan como transitables para las islas */
			if (m->cel[i].ilha || !(m->cel[i].bits & TZ_CEL_PORTA)) continue;
		}
		int np = 0;
		pilha[np++] = i;
		m->cel[i].ilha = prox;
		while (np) {
			int c = pilha[--np], x = c % n, y = c / n;
			for (int d = 0; d < 8; ++d) {
				int nx = x + DX[d], ny = y + DY[d];
				if (nx < 0 || ny < 0 || nx >= n || ny >= n) continue;
				int j = ny * n + nx;
				if (m->cel[j].ilha) continue;
				if (!tz_cel_transitavel(m, TZ_CAPA_TERRA, 0, nx, ny) && !(m->cel[j].bits & TZ_CEL_PORTA)) continue;
				m->cel[j].ilha = prox;
				pilha[np++] = j;
			}
		}
		if (prox < 65535) ++prox;
	}
}
