/* SPDX-License-Identifier: GPL-3.0-or-later */
/* selecao.c - marcas de selección, barras de vida y qué objeto hay bajo el ratón (D417, fase 3).
 *
 * Traducido de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later):
 *   frontend/mission.rs  brackets/bracket_pieces (esquinas blancas de 6 píxeles), barras de vida (0x501040: desde la
 *                        esquina del objeto L = x + 6W y T = y - 24, 20W píxeles de ancho, rojo bajo verde, maná
 *                        azul en las dos filas de debajo)
 *   selection.rs         pointed/click (0x44e900: la unidad cuyo centro está más cerca, a menos de CLICK_REACH = 30
 *                        píxeles en cada eje, entre las casillas vecinas; si no, lo que ocupe la casilla)
 * Coordenadas del mundo con y hacia abajo (ReTzar usa y hacia arriba).
 */
#include "tzar.h"
#include <stdlib.h>
#include <string.h>

#define CLICK_ALCANCE 30

void tz_marco_selecao(TzLienzo *l, int x, int y, int w, int h, uint32_t cor)
{
	if (w < 1 || h < 1) return;
	int lx = w < 6 ? w : 6, ly = h < 6 ? h : 6;
	tz_ret(l, x, y, lx, 1, cor);
	tz_ret(l, x + w - lx, y, lx, 1, cor);
	tz_ret(l, x, y + h - 1, lx, 1, cor);
	tz_ret(l, x + w - lx, y + h - 1, lx, 1, cor);
	tz_ret(l, x, y, 1, ly, cor);
	tz_ret(l, x + w - 1, y, 1, ly, cor);
	tz_ret(l, x, y + h - ly, 1, ly, cor);
	tz_ret(l, x + w - 1, y + h - ly, 1, ly, cor);
}

/* (x, y) = esquina de la casilla del objeto en pantalla; casillas = lado del objeto en casillas. */
void tz_barra_vida(TzLienzo *l, int x, int y, int casillas, int vida, int vida_max, int mana, int mana_max)
{
	if (casillas < 1) casillas = 1;
	if (vida_max <= 0 || vida <= 0) return;
	int bx = x + 6 * casillas, n = 20 * casillas - 1, by = y - 24;
	int verde = (int)((long)vida * n / vida_max) + 1;
	if (verde > n + 1) verde = n + 1;
	tz_ret(l, bx, by, n + 1, 2, 0xff0000);
	tz_ret(l, bx, by, verde, 2, 0x00ff00);
	if (mana_max > 0) {
		int azul = (int)((long)mana * n / mana_max) + 1;
		if (azul > n + 1) azul = n + 1;
		tz_ret(l, bx, by + 2, n + 1, 2, 0xff0000);
		if (mana > 0) tz_ret(l, bx, by + 2, azul, 2, 0x0000ff);
	}
}

int tz_selecao_caixa(const TzEscena *e, int obj, int *x, int *y, int *w, int *h)
{
	for (int i = 0; i < e->nv; ++i) {
		const TzVisual *v = &e->v[i];
		if (v->obj == obj && v->capa == TZ_CAPA_CORPO && v->sel[2] > 0) {
			*x = v->sel[0]; *y = v->sel[1]; *w = v->sel[2]; *h = v->sel[3];
			return 1;
		}
	}
	return 0;
}

int tz_selecao_apontar(const TzEscena *e, const TzMapa *m, int wx, int wy)
{
	int cx = wx / 32, cy = wy / 32, melhor = -1, dist = CLICK_ALCANCE;
	if (wx < 0 || wy < 0) return -1;
	/* unidades en las casillas vecinas: el centro de la caja más cercano al punto */
	for (int i = 0; i < e->nv; ++i) {
		const TzVisual *v = &e->v[i];
		if (v->obj < 0 || v->capa != TZ_CAPA_CORPO || v->sel[2] <= 0) continue;
		const TzObjeto *o = &m->obj[v->obj];
		if (!tz_classe_e_unidade(o->classe) || abs((int)o->x - cx) > 1 || abs((int)o->y - cy) > 1) continue;
		int mx = v->sel[0] + v->sel[2] / 2, my = v->sel[1] + v->sel[3] / 2;
		int gx = abs(mx - wx), gy = abs(my - wy), g = gx > gy ? gx : gy;
		if (g < dist) { dist = g; melhor = v->obj; }
	}
	if (melhor >= 0) return melhor;
	/* si no, lo que se pinta delante y cuya caja contiene el punto (las cajas de los edificios cubren sus casillas) */
	for (int i = e->nv - 1; i >= 0; --i) {
		const TzVisual *v = &e->v[i];
		if (v->obj < 0 || v->capa != TZ_CAPA_CORPO || v->sel[2] <= 0 || v->chao == 1) continue;
		if (wx >= v->sel[0] && wx < v->sel[0] + v->sel[2] && wy >= v->sel[1] && wy < v->sel[1] + v->sel[3]) return v->obj;
	}
	return -1;
}
