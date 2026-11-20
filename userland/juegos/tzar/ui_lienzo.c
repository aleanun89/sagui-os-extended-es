/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ui_lienzo.c - lienzo ARGB de la interfaz de Tzar (D417, fase 4): relleno, copia de imágenes con la convención
 * de tzar.h (alfa 0 transparente, 0x80 sombra, 0xFF opaco) y captura en BMP para las pruebas. Código propio. */
#include "ui.h"
#include <stdlib.h>
#include <string.h>

int tz_tela_criar(TzTela *t, int w, int h)
{
	t->w = w; t->h = h;
	t->px = calloc((size_t)w * (size_t)h, 4);
	return t->px ? 0 : tz_falha("sin memoria para el lienzo %dx%d", w, h);
}

void tz_tela_liberar(TzTela *t) { free(t->px); t->px = NULL; t->w = t->h = 0; }

int tz_rect_dentro(TzRect r, int x, int y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }

static int recortar(const TzTela *t, TzRect *r)
{
	if (r->x < 0) { r->w += r->x; r->x = 0; }
	if (r->y < 0) { r->h += r->y; r->y = 0; }
	if (r->x + r->w > t->w) r->w = t->w - r->x;
	if (r->y + r->h > t->h) r->h = t->h - r->y;
	return r->w > 0 && r->h > 0;
}

static inline uint32_t mezclar(uint32_t d, uint32_t s, unsigned a)
{
	unsigned ia = 255 - a;
	unsigned r = (((d >> 16) & 255) * ia + ((s >> 16) & 255) * a) / 255;
	unsigned g = (((d >> 8) & 255) * ia + ((s >> 8) & 255) * a) / 255;
	unsigned b = ((d & 255) * ia + (s & 255) * a) / 255;
	return 0xff000000u | r << 16 | g << 8 | b;
}

void tz_tela_preencher(TzTela *t, TzRect r, uint32_t argb)
{
	if (!recortar(t, &r)) return;
	unsigned a = argb >> 24;
	for (int j = 0; j < r.h; ++j) {
		uint32_t *d = t->px + (size_t)(r.y + j) * t->w + r.x;
		for (int i = 0; i < r.w; ++i) d[i] = a == 255 ? (argb | 0xff000000u) : mezclar(d[i], argb, a);
	}
}

void tz_tela_moldura(TzTela *t, TzRect r, uint32_t argb)
{
	tz_tela_preencher(t, (TzRect){ r.x, r.y, r.w, 1 }, argb);
	tz_tela_preencher(t, (TzRect){ r.x, r.y + r.h - 1, r.w, 1 }, argb);
	tz_tela_preencher(t, (TzRect){ r.x, r.y + 1, 1, r.h - 2 }, argb);
	tz_tela_preencher(t, (TzRect){ r.x + r.w - 1, r.y + 1, 1, r.h - 2 }, argb);
}

void tz_tela_imagem(TzTela *t, const TzImg *im, TzRect s, int dx, int dy, int escurecer)
{
	if (!im || !im->px) return;
	if (s.w <= 0 || s.h <= 0) s = (TzRect){ 0, 0, im->w, im->h };
	if (s.x < 0 || s.y < 0 || s.x + s.w > im->w || s.y + s.h > im->h) return;
	int x0 = dx < 0 ? -dx : 0, y0 = dy < 0 ? -dy : 0;
	int x1 = dx + s.w > t->w ? t->w - dx : s.w, y1 = dy + s.h > t->h ? t->h - dy : s.h;
	unsigned k = escurecer < 0 ? 0 : escurecer > 100 ? 100 : (unsigned)escurecer;
	for (int j = y0; j < y1; ++j) {
		const uint32_t *p = im->px + (size_t)(s.y + j) * im->w + s.x;
		uint32_t *d = t->px + (size_t)(dy + j) * t->w + dx;
		for (int i = x0; i < x1; ++i) {
			uint32_t c = p[i];
			if (!c) continue;
			if (c >> 24 == 0xff) d[i] = k ? mezclar(c, 0, k * 255 / 100) : c;
			else if (c == TZ_SOMBRA) d[i] = (d[i] >> 1) & 0xff7f7f7f;
		}
	}
}

void tz_tela_imagem_escala(TzTela *t, const TzImg *im, TzRect s, TzRect d)
{
	if (!im || !im->px || d.w <= 0 || d.h <= 0) return;
	if (s.w <= 0 || s.h <= 0) s = (TzRect){ 0, 0, im->w, im->h };
	for (int j = 0; j < d.h; ++j) {
		int y = d.y + j;
		if (y < 0 || y >= t->h) continue;
		int sy = s.y + j * s.h / d.h;
		for (int i = 0; i < d.w; ++i) {
			int x = d.x + i;
			if (x < 0 || x >= t->w) continue;
			uint32_t c = im->px[(size_t)sy * im->w + s.x + i * s.w / d.w];
			if (c >> 24 == 0xff) t->px[(size_t)y * t->w + x] = c;
		}
	}
}

int tz_tela_gravar_bmp(const TzTela *t, const char *ruta)
{
	FILE *f = fopen(ruta, "wb");
	if (!f) return tz_falha("no se puede escribir %s", ruta);
	uint32_t tam = 54 + (uint32_t)t->w * (uint32_t)t->h * 4;
	uint8_t c[54] = { 'B', 'M' };
	memcpy(c + 2, &tam, 4);
	uint32_t off = 54, dib = 40, pl = 1 | 32u << 16;
	int32_t w = t->w, h = -t->h;
	memcpy(c + 10, &off, 4); memcpy(c + 14, &dib, 4); memcpy(c + 18, &w, 4); memcpy(c + 22, &h, 4); memcpy(c + 26, &pl, 4);
	fwrite(c, 1, 54, f);
	for (int i = 0; i < t->w * t->h; ++i) { uint32_t p = t->px[i] | 0xff000000u; fwrite(&p, 4, 1, f); }
	fclose(f);
	return 0;
}
