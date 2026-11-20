/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ui_mapa.c - el mundo visible bajo el HUD (D417, fases 4 y 5): el terreno animado y, encima, el mundo vivo de la
 * simulación (tz_desenhar_mundo de partida.c: escena, objetos en su pose, proyectiles y niebla), recortado a la
 * ventana del mapa. Código propio. */
#include "ui.h"
#include "partida.h"

void tz_ui_mapa_desenhar(TzUi *u, TzTela *t, const TzUiMapa *mp)
{
	TzRect mr = u->mapa_rect;
	const TzMapa *mapa = mp->mapa;
	tz_tela_preencher(t, mr, 0xff000000);
	if (mr.w <= 0 || mr.h <= 0) return;
	int cx0 = u->camx / TZ_CASILLA, cy0 = u->camy / TZ_CASILLA;
	int cx1 = (u->camx + mr.w + TZ_CASILLA - 1) / TZ_CASILLA, cy1 = (u->camy + mr.h + TZ_CASILLA - 1) / TZ_CASILLA;
	if (cx1 > mapa->lado) cx1 = mapa->lado;
	if (cy1 > mapa->lado) cy1 = mapa->lado;
	uint32_t tic = (uint32_t)(mp->ms / 250);
	for (int cy = cy0; cy < cy1; ++cy)
		for (int cx = cx0; cx < cx1; ++cx) {
			int paso;
			const uint32_t *px = tz_terreno_loseta_t(mp->ter, mapa->casillas[cy * mapa->lado + cx], cx, cy, tic, &paso);
			int sx = mr.x + cx * TZ_CASILLA - u->camx, sy = mr.y + cy * TZ_CASILLA - u->camy;
			for (int j = 0; j < TZ_CASILLA; ++j) {
				int y = sy + j;
				if (y < mr.y || y >= mr.y + mr.h) continue;
				uint32_t *d = t->px + (size_t)y * t->w;
				for (int i = 0; i < TZ_CASILLA; ++i) {
					int x = sx + i;
					if (x < mr.x || x >= mr.x + mr.w) continue;
					d[x] = px ? 0xff000000u | (px[j * paso + i] & 0xffffff) : 0xffff00ff;
				}
			}
		}
	/* el mundo vivo en un lienzo que es la ventana del mapa dentro de la pantalla */
	TzLienzo l = { t->px + (size_t)mr.y * t->w + mr.x, mr.w, mr.h, t->w };
	tz_desenhar_mundo(u->m, mp->esc, &l, u->camx, u->camy, mp->ms);
	/* lo que pintó sin alfa vuelve a ser opaco para la tela */
	for (int y = 0; y < mr.h; ++y) {
		uint32_t *d = l.px + (size_t)y * t->w;
		for (int x = 0; x < mr.w; ++x) d[x] |= 0xff000000u;
	}
}
