/* SPDX-License-Identifier: GPL-3.0-or-later */
/* dibujo.c - lienzo de 32 bits y fuentes .FNT del juego (D417, fase 3).
 *
 * Las fuentes se traducen de ReTzar, src/font.rs (BitmapFont::decode, slot, blend, draw)
 * (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): 50 bytes de métricas, tabla de avances (256), tabla de
 * glifos (256) y un BMP indexado de 8 bits desde el byte 562 cuyos valores son cobertura (0 a `niveis`), no
 * paleta; una fila de glifos por alto. El texto va en bytes Windows-1252 (en SaguiOS, Latin-1: igual salvo 0x80-0x9f).
 */
#include "tzar.h"
#include <stdlib.h>
#include <string.h>

void tz_ret(TzLienzo *l, int x, int y, int w, int h, uint32_t cor)
{
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > l->w) w = l->w - x;
	if (y + h > l->h) h = l->h - y;
	for (int j = 0; j < h; ++j)
		for (int i = 0; i < w; ++i) l->px[(y + j) * l->passo + x + i] = cor;
}

void tz_ret_escuro(TzLienzo *l, int x, int y, int w, int h)
{
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > l->w) w = l->w - x;
	if (y + h > l->h) h = l->h - y;
	for (int j = 0; j < h; ++j)
		for (int i = 0; i < w; ++i) { uint32_t *p = &l->px[(y + j) * l->passo + x + i]; *p = (*p >> 1) & 0x7f7f7f; }
}

void tz_pintar(TzLienzo *l, const TzImg *im, int x, int y)
{
	int x0 = x < 0 ? -x : 0, y0 = y < 0 ? -y : 0;
	int x1 = x + im->w > l->w ? l->w - x : im->w, y1 = y + im->h > l->h ? l->h - y : im->h;
	for (int j = y0; j < y1; ++j) {
		const uint32_t *s = im->px + (size_t)j * im->w;
		uint32_t *d = l->px + (size_t)(y + j) * l->passo + x;
		for (int i = x0; i < x1; ++i) {
			uint32_t c = s[i];
			if (!c) continue;
			if (c >> 24 == 0xff) d[i] = c & 0xffffff;
			else if (c == TZ_SOMBRA) d[i] = (d[i] >> 1) & 0x7f7f7f;
		}
	}
}

int tz_fonte_ler(TzFonte *f, const uint8_t *d, uint32_t n)
{
	memset(f, 0, sizeof *f);
	if (n < 616 + 0u) return tz_falha("FNT truncada");
	const uint8_t *bmp = d + 562;
	uint32_t bn = n - 562;
	if (bmp[0] != 'B' || bmp[1] != 'M' || bn < 54) return tz_falha("FNT no admitida");
	if (tz_u32(bmp + 30) != 0 || bmp[28] != 8 || bmp[29] != 0) return tz_falha("FNT no admitida");
	int alt = tz_u16(d + 32);
	uint32_t larg = tz_u32(bmp + 18), filas = tz_u32(bmp + 22);
	if (alt <= 0 || alt > 128 || larg == 0 || larg > 128 || filas == 0 || filas % (uint32_t)alt || filas / (uint32_t)alt > 4096)
		return tz_falha("dimensiones de FNT no válidas");
	uint32_t off = tz_u32(bmp + 10), passo = (larg + 3) / 4 * 4;
	if ((uint64_t)off + (uint64_t)passo * filas > bn) return tz_falha("píxeles de FNT truncados");
	f->px = tz_malloc((size_t)larg * filas);
	if (!f->px) return -1;
	int niv = 0;
	for (uint32_t y = 0; y < filas; ++y) {
		const uint8_t *s = bmp + off + (size_t)(filas - 1 - y) * passo;
		memcpy(f->px + (size_t)y * larg, s, larg);
		for (uint32_t x = 0; x < larg; ++x) if (s[x] > niv) niv = s[x];
	}
	if (niv > 7) { free(f->px); f->px = NULL; return tz_falha("FNT de color, no de cobertura"); }
	f->alt = alt; f->larg = (int)larg; f->nglifos = (int)(filas / (uint32_t)alt); f->niveis = niv ? niv : 1;
	memcpy(f->avanco, d + 50, 256);
	memcpy(f->glifo, d + 306, 256);
	return 0;
}

int tz_fonte_medir(const TzFonte *f, const char *s)
{
	int w = 0;
	for (; *s; ++s) w += f->avanco[(unsigned char)*s];
	return w;
}

void tz_fonte_texto(const TzFonte *f, TzLienzo *l, int x, int y, const char *s, uint32_t cor)
{
	for (; *s; ++s) {
		unsigned c = (unsigned char)*s;
		int g = f->glifo[c];
		if (g < f->nglifos)
			for (int gy = 0; gy < f->alt; ++gy)
				for (int gx = 0; gx < f->larg; ++gx) {
					int cob = f->px[((size_t)g * (size_t)f->alt + (size_t)gy) * (size_t)f->larg + (size_t)gx];
					int px = x + gx, py = y + gy;
					if (!cob || px < 0 || py < 0 || px >= l->w || py >= l->h) continue;
					uint32_t *d = &l->px[(size_t)py * l->passo + px];
					uint32_t o = *d, r = 0;
					for (int k = 0; k < 24; k += 8) {
						uint32_t a = (o >> k) & 255, b = (cor >> k) & 255;
						r |= ((a * (uint32_t)(f->niveis - cob) + b * (uint32_t)cob) / (uint32_t)f->niveis) << k;
					}
					*d = r;
				}
		x += f->avanco[c];
	}
}

void tz_fonte_liberar(TzFonte *f)
{
	free(f->px);
	memset(f, 0, sizeof *f);
}
