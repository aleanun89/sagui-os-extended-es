/* SPDX-License-Identifier: GPL-3.0-or-later */
/* imagem.c - BMP y sprites RLE de Tzar a ARGB (D417).
 *
 * Traducido de ReTzar, src/bitmap.rs y src/sprite.rs (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later).
 * Diferencias: se decodifica solo el recorte o el cuadro que se pide (ReTzar decodifica la hoja entera y
 * la reordena), para no tener hojas enteras en memoria en SaguiOS.
 *
 * Color del reino: en las imágenes indexadas de 256 colores, los grises claros 240-255 se cambian por la
 * rampa del reino, 112 + 16 * color (los ocho colores de COLOR BARS.BMP, 16 tonos cada uno).
 */
#include "tzar.h"
#include <stdlib.h>
#include <string.h>

#define GRISES 240
#define RAMPAS 112

static uint32_t argb(uint8_t r, uint8_t g, uint8_t b) { return 0xff000000u | (uint32_t)r << 16 | (uint32_t)g << 8 | b; }
static uint8_t expandir5(unsigned v) { return (uint8_t)((v << 3) | (v >> 2)); }

void tz_img_liberar(TzImg *i) { free(i->px); i->px = NULL; i->w = i->h = 0; }

static int nova(TzImg *o, int w, int h)
{
	o->w = w; o->h = h;
	o->px = calloc(w > 0 && h > 0 ? (size_t)w * (size_t)h : 1, 4);
	if (!o->px) return tz_falha("sin memoria para una imagen de %dx%d", w, h);
	return 0;
}

/* ------------------------------------------------------------------ BMP */

int tz_bmp_indexado(const uint8_t *d, uint32_t n) { return n >= 30 && d[0] == 'B' && d[1] == 'M' && d[28] == 8 && d[29] == 0; }

int tz_bmp_info(const uint8_t *d, uint32_t n, int *w, int *h, int *prof)
{
	if (n < 54 || d[0] != 'B' || d[1] != 'M') return tz_falha("cabecera BMP no válida");
	int32_t ww = (int32_t)tz_u32(d + 18), hh = (int32_t)tz_u32(d + 22);
	int p = tz_u16(d + 28);
	if (ww <= 0 || hh == 0 || hh == INT32_MIN || ww > 16384 || (hh < 0 ? -hh : hh) > 16384 || tz_u32(d + 30) != 0 ||
	    (p != 8 && p != 16 && p != 24 && p != 32) || tz_u16(d + 26) != 1)
		return tz_falha("BMP no admitido: %dx%d, %d bits, compresión %lu", (int)ww, (int)hh, p, (unsigned long)tz_u32(d + 30));
	*w = ww; *h = hh < 0 ? -hh : hh;
	if (prof) *prof = p;
	return 0;
}

int tz_bmp_recorte(const uint8_t *d, uint32_t n, int x0, int y0, int w, int h, int verde_transp, int slot, TzImg *o)
{
	int W, H, prof;
	memset(o, 0, sizeof *o);
	if (tz_bmp_info(d, n, &W, &H, &prof)) return -1;
	if (tz_u32(d + 14) < 40) return tz_falha("DIB de BMP no admitido");
	if (x0 < 0 || y0 < 0 || w <= 0 || h <= 0 || x0 + w > W || y0 + h > H) return tz_falha("recorte fuera del BMP");
	int32_t hs = (int32_t)tz_u32(d + 22);
	uint32_t off = tz_u32(d + 10);
	uint64_t stride = ((uint64_t)W * (uint64_t)prof + 31) / 32 * 4;
	if (off < 54 || (uint64_t)off + stride * (uint64_t)H > n) return tz_falha("píxeles del BMP truncados");
	uint32_t pal = 14 + tz_u32(d + 14), npal = 0;
	if (prof == 8) {
		if (n < 50) return tz_falha("BMP truncado");
		npal = tz_u32(d + 46);
		if (!npal) npal = 256;
		if (npal > 256 || (uint64_t)pal + npal * 4 > off) return tz_falha("paleta de BMP no válida");
	}
	if (nova(o, w, h)) return -1;
	for (int y = 0; y < h; ++y) {
		int sy = hs > 0 ? H - 1 - (y0 + y) : y0 + y;
		const uint8_t *fila = d + off + (uint64_t)sy * stride;
		for (int x = 0; x < w; ++x) {
			int sx = x0 + x;
			uint8_t r, g, b;
			if (prof == 16) {
				unsigned v = tz_u16(fila + sx * 2);
				r = expandir5((v >> 10) & 31); g = expandir5((v >> 5) & 31); b = expandir5(v & 31);
			} else if (prof == 8) {
				unsigned i = fila[sx];
				if (i >= npal) { tz_img_liberar(o); return tz_falha("índice de paleta del BMP fuera de rango"); }
				if (slot >= 0 && npal == 256 && i >= GRISES) i = RAMPAS + (unsigned)(slot > 7 ? 7 : slot) * 16 + (i - GRISES);
				const uint8_t *q = d + pal + i * 4;
				r = q[2]; g = q[1]; b = q[0];
			} else {
				const uint8_t *q = fila + sx * (prof / 8);
				r = q[2]; g = q[1]; b = q[0];
			}
			o->px[y * w + x] = verde_transp && r == 0 && g == 255 && b == 0 ? 0 : argb(r, g, b);
		}
	}
	return 0;
}

int tz_bmp(const uint8_t *d, uint32_t n, int verde_transp, int slot, TzImg *o)
{
	int w, h;
	memset(o, 0, sizeof *o);
	if (tz_bmp_info(d, n, &w, &h, NULL)) return -1;
	return tz_bmp_recorte(d, n, 0, 0, w, h, verde_transp, slot, o);
}

/* ------------------------------------------------------------------ RLE */

/* Cabecera: « elr», columnas (u32 en 4), filas (u32 en 8), «RLE\0» en 16, ancho y alto de la hoja (u16 en 20
 * y 22), formato (u16 en 24: 1 indexado, 3 RGB555, 7 sombra), colores (u32 en 30), paleta BGRA desde 38 y,
 * detrás, una tabla con el desplazamiento de cada fila (desde el final de la paleta). Cada fila es una
 * lista de (salto, cuenta) seguida de los píxeles de la cuenta. */
int tz_rle_cab(const uint8_t *d, uint32_t n, TzRle *r)
{
	if (n < 38 || memcmp(d, " elr", 4) || memcmp(d + 16, "RLE", 4)) return tz_falha("sprite RLE no válido");
	uint32_t cols = tz_u32(d + 4), rows = tz_u32(d + 8), cores = tz_u32(d + 30);
	unsigned w = tz_u16(d + 20), h = tz_u16(d + 22), fmt = tz_u16(d + 24);
	if (!cols || !rows || cols > 4096 || rows > 4096 || cols * rows > 4096 || !w || !h || w > 4096 || h < cols * rows ||
	    (fmt != 1 && fmt != 3 && fmt != 7) || (fmt == 1 && (cores < 1 || cores > 256)) || (fmt != 1 && cores) ||
	    (uint64_t)w * h > 32u * 1024 * 1024)
		return tz_falha("disposición de sprite RLE no admitida");
	uint32_t base = 38 + cores * 4;
	if ((uint64_t)base + (uint64_t)h * 4 > n) return tz_falha("tabla de filas del sprite truncada");
	r->cols = (int)cols; r->rows = (int)rows; r->w = (int)w; r->h = (int)h; r->formato = (int)fmt; r->cores = (int)cores;
	r->fh = (int)(h / (cols * rows));
	r->base = base;
	return 0;
}

int tz_rle_indexado(const uint8_t *d, uint32_t n)
{
	TzRle r;
	return !tz_rle_cab(d, n, &r) && r.formato == 1 && r.cores == 256;
}

/* Decodifica las filas [y0, y0 + nh) de la hoja en `o` (puede ser NULL para solo validar). */
static int filas(const uint8_t *d, uint32_t n, const TzRle *r, int y0, int nh, int slot, TzImg *o)
{
	uint32_t pal[256];
	for (int i = 0; i < r->cores; ++i) {
		int k = i;
		if (slot >= 0 && r->cores == 256 && i >= GRISES) k = RAMPAS + (slot > 7 ? 7 : slot) * 16 + (i - GRISES);
		const uint8_t *q = d + 38 + k * 4;
		pal[i] = argb(q[2], q[1], q[0]);
	}
	for (int y = y0; y < y0 + nh; ++y) {
		uint32_t rel = tz_u32(d + r->base + (uint32_t)y * 4);
		if (rel < (uint32_t)r->h * 4) return tz_falha("fila del sprite encima de la tabla");
		uint64_t a = (uint64_t)r->base + rel;
		int x = 0;
		while (x < r->w) {
			if (a + 2 > n) return tz_falha("tramo del sprite truncado");
			int salto = d[a], cnt = d[a + 1];
			a += 2;
			if (salto + cnt == 0 || x + salto + cnt > r->w) return tz_falha("tramo del sprite no válido");
			x += salto;
			for (int k = 0; k < cnt; ++k, ++x) {
				uint32_t c;
				if (r->formato == 7) c = TZ_SOMBRA;
				else if (r->formato == 1) {
					if (a + 1 > n) return tz_falha("sprite indexado truncado");
					unsigned i = d[a++];
					if (i >= (unsigned)r->cores) return tz_falha("índice de paleta del sprite no válido");
					c = pal[i];
				} else {
					if (a + 2 > n) return tz_falha("sprite RGB555 truncado");
					unsigned v = tz_u16(d + a);
					a += 2;
					c = argb(expandir5((v >> 10) & 31), expandir5((v >> 5) & 31), expandir5(v & 31));
				}
				if (o) o->px[(y - y0) * o->w + x] = c;
			}
		}
	}
	return 0;
}

int tz_rle_quadro(const uint8_t *d, uint32_t n, int col, int row, int slot, TzImg *o)
{
	TzRle r;
	memset(o, 0, sizeof *o);
	if (tz_rle_cab(d, n, &r)) return -1;
	if (col < 0 || row < 0 || col >= r.cols || row >= r.rows) return tz_falha("cuadro del sprite fuera de la hoja");
	/* el original guarda todos los cuadros de una dirección antes de pasar a la siguiente */
	int idx = col * r.rows + row;
	if (nova(o, r.w, r.fh)) return -1;
	if (filas(d, n, &r, idx * r.fh, r.fh, slot, o)) { tz_img_liberar(o); return -1; }
	return 0;
}

int tz_rle_validar(const uint8_t *d, uint32_t n)
{
	TzRle r;
	if (tz_rle_cab(d, n, &r)) return -1;
	return filas(d, n, &r, 0, r.h, -1, NULL);
}
