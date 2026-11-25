/* SPDX-License-Identifier: GPL-3.0-or-later */
/* opcoes.c - opciones guardadas de Tzar (tzar.cfg) y modos de pantalla: el lienzo del juego, su escalado a la ventana
 * o a la pantalla completa (entero o ajustado, con bandas negras) y el filtro (D417). Código propio; la regla de la
 * interfaz x2 sigue a ReTzar (frontend/view.rs hud_layout, GPL-3.0-or-later). */
#include "opcoes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int tz_resolucoes[TZ_NRESOLUCOES][2] = { { 0, 0 }, { 640, 480 }, { 800, 600 }, { 1024, 768 } };

/* lo mínimo que necesita la interfaz original (LAYOUT800) */
#define MIN_W 800
#define MIN_H 600

void tz_opcoes_padrao(TzOpcoes *o)
{
	memset(o, 0, sizeof *o);
	o->resolucao = 0;
	o->escala = TZ_ESC_INTEIRA;
	o->suave = 0;
	o->interface = 0;
	o->cursor_jogo = 1;
	o->volume_som = 80;
	o->volume_musica = 60;
}

static int limitar(int v, int a, int b) { return v < a ? a : v > b ? b : v; }

int tz_opcoes_ler(TzOpcoes *o, const char *ruta)
{
	FILE *f = ruta ? fopen(ruta, "r") : NULL;
	if (!f) return -1;
	char l[800];
	while (fgets(l, sizeof l, f)) {
		char *eq = strchr(l, '=');
		if (!eq || l[0] == '#') continue;
		*eq = 0;
		int v = atoi(eq + 1);
		if (!strcmp(l, "aleatorio")) { eq[1 + strcspn(eq + 1, "\r\n")] = 0; snprintf(o->aleat, sizeof o->aleat, "%s", eq + 1); continue; }
		if (!strcmp(l, "pantalla_completa")) o->tela_cheia = v != 0;
		else if (!strcmp(l, "resolucion")) o->resolucao = limitar(v, 0, TZ_NRESOLUCOES - 1);
		else if (!strcmp(l, "escalado")) o->escala = v ? TZ_ESC_AJUSTAR : TZ_ESC_INTEIRA;
		else if (!strcmp(l, "suave")) o->suave = v != 0;
		else if (!strcmp(l, "interfaz")) o->interface = limitar(v, 0, 3);
		else if (!strcmp(l, "panel_derecha")) o->painel_direita = v != 0;
		else if (!strcmp(l, "cursor_juego")) o->cursor_jogo = v != 0;
		else if (!strcmp(l, "volumen_sonido")) o->volume_som = limitar(v, 0, 100);
		else if (!strcmp(l, "volumen_musica")) o->volume_musica = limitar(v, 0, 100);
	}
	fclose(f);
	return 0;
}

int tz_opcoes_gravar(const TzOpcoes *o, const char *ruta)
{
	FILE *f = ruta ? fopen(ruta, "w") : NULL;
	if (!f) return -1;
	fprintf(f, "# Tzar en SaguiOS: opciones (se cambian en el menú Opciones)\n");
	fprintf(f, "pantalla_completa=%d\nresolucion=%d\nescalado=%d\nsuave=%d\ninterfaz=%d\npanel_derecha=%d\ncursor_juego=%d\n"
	        "volumen_sonido=%d\nvolumen_musica=%d\n", o->tela_cheia, o->resolucao, o->escala, o->suave, o->interface,
	        o->painel_direita, o->cursor_jogo, o->volume_som, o->volume_musica);
	if (o->aleat[0]) fprintf(f, "aleatorio=%s\n", o->aleat);
	fclose(f);
	return 0;
}

const char *tz_resolucao_nome(int i, char *buf, int n)
{
	if (i <= 0 || i >= TZ_NRESOLUCOES) snprintf(buf, (size_t)n, "Pantalla");
	else snprintf(buf, (size_t)n, "%dx%d", tz_resolucoes[i][0], tz_resolucoes[i][1]);
	return buf;
}

void tz_modo_janela(const TzOpcoes *o, int maxw, int maxh, int *w, int *h)
{
	int r = o->resolucao > 0 && o->resolucao < TZ_NRESOLUCOES ? o->resolucao : 0;
	int ww = r ? tz_resolucoes[r][0] : 1024, wh = r ? tz_resolucoes[r][1] : 768;
	/* la ventana cabe en la pantalla; si no, la mayor de las del original que quepa (y como poco 640x480) */
	for (int i = TZ_NRESOLUCOES - 1; (ww > maxw || wh > maxh) && i >= 1; --i)
		if (tz_resolucoes[i][0] <= maxw && tz_resolucoes[i][1] <= maxh) { ww = tz_resolucoes[i][0]; wh = tz_resolucoes[i][1]; }
	if (ww > maxw || wh > maxh) { ww = 640; wh = 480; }
	*w = ww; *h = wh;
}

void tz_modo_destino(const TzOpcoes *o, int cw, int ch, int ow, int oh, TzModo *m)
{
	memset(m, 0, sizeof *m);
	if (cw < 1) cw = 1;
	if (ch < 1) ch = 1;
	if (ow < 1) ow = 1;
	if (oh < 1) oh = 1;
	m->cw = cw; m->ch = ch; m->ow = ow; m->oh = oh;
	int k = ow / cw < oh / ch ? ow / cw : oh / ch;
	if (o->escala == TZ_ESC_INTEIRA && k >= 1) { m->dw = cw * k; m->dh = ch * k; }
	else if ((long long)ow * ch <= (long long)oh * cw) { m->dw = ow; m->dh = (int)((long long)ch * ow / cw); }
	else { m->dh = oh; m->dw = (int)((long long)cw * oh / ch); }
	if (m->dw < 1) m->dw = 1;
	if (m->dh < 1) m->dh = 1;
	m->fator = k >= 1 && m->dw == cw * k && m->dh == ch * k ? k : 0;
	m->dx = (ow - m->dw) / 2;
	m->dy = (oh - m->dh) / 2;
}

void tz_modo_calcular(const TzOpcoes *o, int ow, int oh, TzModo *m)
{
	int cw, ch;
	int r = o->resolucao > 0 && o->resolucao < TZ_NRESOLUCOES ? o->resolucao : 0;
	if (!r) {
		/* la de la pantalla: la interfaz x n dibuja a 1/n y se amplía (automática: el mayor n hasta 3 que deja 800x600) */
		int k = o->interface;
		if (k <= 0) { k = 1; while (k < 3 && ow / (k + 1) >= MIN_W && oh / (k + 1) >= MIN_H) ++k; }
		while (k > 1 && (ow / k < MIN_W || oh / k < MIN_H)) --k;
		cw = ow / k; ch = oh / k;
	} else { cw = tz_resolucoes[r][0]; ch = tz_resolucoes[r][1]; }
	/* la interfaz original no cabe en menos de 800x600: un lienzo menor se dibuja a 800x600 y se reduce (como ReTzar) */
	if (cw < MIN_W) cw = MIN_W;
	if (ch < MIN_H) ch = MIN_H;
	tz_modo_destino(o, cw, ch, ow, oh, m);
}

void tz_modo_ponto(const TzModo *m, int x, int y, int *cx, int *cy)
{
	int px = m->dw > 0 ? (int)((long long)(x - m->dx) * m->cw / m->dw) : x;
	int py = m->dh > 0 ? (int)((long long)(y - m->dy) * m->ch / m->dh) : y;
	*cx = px < 0 ? 0 : px >= m->cw ? m->cw - 1 : px;
	*cy = py < 0 ? 0 : py >= m->ch ? m->ch - 1 : py;
}

/* columnas de origen de cada columna de destino (vecino más próximo), guardadas mientras no cambie el modo */
static int *tab_x, *tab_y, tab_dw, tab_dh, tab_cw, tab_ch;

static int tabelas(const TzModo *m)
{
	if (tab_x && tab_dw == m->dw && tab_cw == m->cw && tab_dh == m->dh && tab_ch == m->ch) return 0;
	free(tab_x); free(tab_y);
	tab_x = malloc((size_t)m->dw * sizeof *tab_x);
	tab_y = malloc((size_t)m->dh * sizeof *tab_y);
	if (!tab_x || !tab_y) { free(tab_x); free(tab_y); tab_x = tab_y = NULL; return -1; }
	for (int x = 0; x < m->dw; ++x) tab_x[x] = (int)(((long long)x * 2 + 1) * m->cw / (2LL * m->dw));
	for (int y = 0; y < m->dh; ++y) tab_y[y] = (int)(((long long)y * 2 + 1) * m->ch / (2LL * m->dh));
	tab_dw = m->dw; tab_dh = m->dh; tab_cw = m->cw; tab_ch = m->ch;
	return 0;
}

static uint32_t mistura4(uint32_t a, uint32_t b, uint32_t c, uint32_t d, int fx, int fy)
{
	/* bilineal con pesos de 0 a 256 */
	uint32_t r = 0;
	for (int s = 0; s < 24; s += 8) {
		int va = (int)(a >> s & 255), vb = (int)(b >> s & 255), vc = (int)(c >> s & 255), vd = (int)(d >> s & 255);
		int top = va * (256 - fx) + vb * fx, bot = vc * (256 - fx) + vd * fx;
		int v = (top * (256 - fy) + bot * fy) >> 16;
		r |= (uint32_t)(v & 255) << s;
	}
	return r;
}

void tz_apresentar_escala(const uint32_t *src, const TzModo *m, uint32_t *dst, int pitch, int suave)
{
	int dx = m->dx, dy = m->dy, dw = m->dw, dh = m->dh, cw = m->cw, ch = m->ch;
	/* bandas negras */
	for (int y = 0; y < m->oh; ++y) {
		uint32_t *l = dst + (size_t)y * pitch;
		if (y < dy || y >= dy + dh) { memset(l, 0, (size_t)m->ow * 4); continue; }
		if (dx > 0) memset(l, 0, (size_t)dx * 4);
		if (dx + dw < m->ow) memset(l + dx + dw, 0, (size_t)(m->ow - dx - dw) * 4);
	}
	if (m->fator == 1) {
		for (int y = 0; y < ch; ++y) {
			const uint32_t *s = src + (size_t)y * cw;
			uint32_t *d = dst + (size_t)(dy + y) * pitch + dx;
			for (int x = 0; x < cw; ++x) d[x] = s[x] & 0xffffff;
		}
		return;
	}
	if (m->fator == 2 && !suave) {
		for (int y = 0; y < ch; ++y) {
			const uint32_t *s = src + (size_t)y * cw;
			uint32_t *d0 = dst + (size_t)(dy + 2 * y) * pitch + dx, *d1 = d0 + pitch;
			for (int x = 0; x < cw; ++x) { uint32_t v = s[x] & 0xffffff; d0[2 * x] = d0[2 * x + 1] = d1[2 * x] = d1[2 * x + 1] = v; }
		}
		return;
	}
	if (!suave || m->fator > 1) {
		/* vecino más próximo; con factor entero el suave no aporta nada (cada píxel es un bloque) */
		if (tabelas(m)) return;
		int prev = -1;
		for (int y = 0; y < dh; ++y) {
			uint32_t *d = dst + (size_t)(dy + y) * pitch + dx;
			int sy = tab_y[y];
			if (sy == prev) { memcpy(d, d - pitch, (size_t)dw * 4); continue; }
			prev = sy;
			const uint32_t *s = src + (size_t)sy * cw;
			for (int x = 0; x < dw; ++x) d[x] = s[tab_x[x]] & 0xffffff;
		}
		return;
	}
	/* suave: bilineal en punto fijo (centros de píxel alineados); la posición de cada columna se calcula una vez */
	static int *bx, bdw, bcw;
	if (!bx || bdw != dw || bcw != cw) {
		free(bx);
		bx = malloc((size_t)dw * sizeof *bx);
		if (!bx) return;
		for (int x = 0; x < dw; ++x) {
			long long f = ((long long)x * 2 + 1) * cw * 256 / (2LL * dw) - 128;
			bx[x] = f < 0 ? 0 : (int)f;
		}
		bdw = dw; bcw = cw;
	}
	for (int y = 0; y < dh; ++y) {
		long long fyl = ((long long)y * 2 + 1) * ch * 256 / (2LL * dh) - 128;
		if (fyl < 0) fyl = 0;
		int sy = (int)(fyl >> 8), fy = (int)(fyl & 255);
		int sy1 = sy + 1 < ch ? sy + 1 : sy;
		const uint32_t *s0 = src + (size_t)sy * cw, *s1 = src + (size_t)sy1 * cw;
		uint32_t *d = dst + (size_t)(dy + y) * pitch + dx;
		for (int x = 0; x < dw; ++x) {
			int sx = bx[x] >> 8, fx = bx[x] & 255;
			int sx1 = sx + 1 < cw ? sx + 1 : sx;
			d[x] = mistura4(s0[sx], s0[sx1], s1[sx], s1[sx1], fx, fy);
		}
	}
}
