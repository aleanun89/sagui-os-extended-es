/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Renderizador por software: búfer de 32 bits, copia de sprites con paleta, escalado y colores de jugador, y una pequeña fuente integrada. */
#include "aoen.h"

Fb fb;
int gfx_us = 1;                    /* factor de escala de la interfaz (1, 2...): ver gfx_ui_begin */
int gfx_smooth;                    /* 1: los sprites ampliados o reducidos se filtran (zoom suave) */
static int real_w, real_h;         /* tamaño real del búfer: fb.w y fb.h pasan a ser lógicos con la escala */

u32 gfx_rgb(int r, int g, int b) {
	return 0xff000000u | ((u32)CLAMP(r, 0, 255) << 16) | ((u32)CLAMP(g, 0, 255) << 8) | (u32)CLAMP(b, 0, 255);
}

/* ---------- escala de la interfaz ----------
 * La interfaz se dibuja en coordenadas lógicas (fb.w/us x fb.h/us) y cada primitiva las multiplica por us: así los botones,
 * los textos y los iconos se ven nítidos (píxeles enteros) en pantallas grandes sin tocar el código de cada pantalla. */
void gfx_set_size(int w, int h) { real_w = w; real_h = h; fb.w = w; fb.h = h; gfx_us = 1; }

void gfx_ui_begin(int us) {
	if (us < 1) us = 1;
	gfx_us = us;
	fb.w = real_w / us; fb.h = real_h / us;
	fb.cx0 = 0; fb.cy0 = 0; fb.cx1 = real_w; fb.cy1 = real_h;
}

void gfx_ui_end(void) {
	gfx_us = 1;
	fb.w = real_w; fb.h = real_h;
	fb.cx0 = 0; fb.cy0 = 0; fb.cx1 = real_w; fb.cy1 = real_h;
}

int gfx_ui_suspend(void) { int s = gfx_us; if (s > 1) gfx_ui_end(); return s; }
void gfx_ui_resume(int s) { if (s > 1) gfx_ui_begin(s); }

int gfx_real_w(void) { return real_w; }
int gfx_real_h(void) { return real_h; }

void gfx_clip(int x0, int y0, int x1, int y1) {
	x0 *= gfx_us; y0 *= gfx_us; x1 *= gfx_us; y1 *= gfx_us;
	fb.cx0 = MAX(0, x0); fb.cy0 = MAX(0, y0);
	fb.cx1 = MIN(real_w, x1); fb.cy1 = MIN(real_h, y1);
}

void gfx_clip_reset(void) { fb.cx0 = 0; fb.cy0 = 0; fb.cx1 = real_w; fb.cy1 = real_h; }

/* ---------- rellenos (en coordenadas reales) ---------- */

static void fill_raw(int x, int y, int w, int h, u32 col) {
	int x0 = MAX(x, fb.cx0), y0 = MAX(y, fb.cy0), x1 = MIN(x + w, fb.cx1), y1 = MIN(y + h, fb.cy1);
	if (x1 <= x0) return;
	for (int yy = y0; yy < y1; ++yy)
		px_fill(fb.px + (size_t)yy * fb.pitch + x0, col, x1 - x0);
}

static void fill_a_raw(int x, int y, int w, int h, u32 col, int alpha) {
	int x0 = MAX(x, fb.cx0), y0 = MAX(y, fb.cy0), x1 = MIN(x + w, fb.cx1), y1 = MIN(y + h, fb.cy1);
	if (x1 <= x0 || alpha <= 0) return;
	if (alpha >= 255) { fill_raw(x, y, w, h, col); return; }
	static u32 *row; static int rowcap;
	int n = x1 - x0;
	if (rowcap < n) { free(row); rowcap = n + 256; row = (u32 *)malloc((size_t)rowcap * 4); }
	if (row) {
		px_fill(row, col, n);
		for (int yy = y0; yy < y1; ++yy) px_blend(fb.px + (size_t)yy * fb.pitch + x0, row, n, alpha * 256 / 255);
	}
}

void gfx_fill(int x, int y, int w, int h, u32 col) { fill_raw(x * gfx_us, y * gfx_us, w * gfx_us, h * gfx_us, col); }
void gfx_fill_a(int x, int y, int w, int h, u32 col, int alpha) { fill_a_raw(x * gfx_us, y * gfx_us, w * gfx_us, h * gfx_us, col, alpha); }

void gfx_rect(int x, int y, int w, int h, u32 col) {
	gfx_fill(x, y, w, 1, col);
	gfx_fill(x, y + h - 1, w, 1, col);
	gfx_fill(x, y, 1, h, col);
	gfx_fill(x + w - 1, y, 1, h, col);
}

/* relleno con degradado vertical de un color a otro (interfaz moderna) */
void gfx_fill_grad(int x, int y, int w, int h, u32 top, u32 bottom) {
	for (int k = 0; k < h; ++k) {
		int t = h > 1 ? k * 256 / (h - 1) : 0;
		int r = (int)((top >> 16) & 255) * (256 - t) + (int)((bottom >> 16) & 255) * t;
		int g = (int)((top >> 8) & 255) * (256 - t) + (int)((bottom >> 8) & 255) * t;
		int b = (int)(top & 255) * (256 - t) + (int)(bottom & 255) * t;
		gfx_fill(x, y + k, w, 1, gfx_rgb(r >> 8, g >> 8, b >> 8));
	}
}

static void line_raw(int x0, int y0, int x1, int y1, u32 col) {
	int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
	for (;;) {
		if (x0 >= fb.cx0 && x0 < fb.cx1 && y0 >= fb.cy0 && y0 < fb.cy1)
			fb.px[(size_t)y0 * fb.pitch + x0] = col;
		if (x0 == x1 && y0 == y1)
			break;
		int e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
}

void gfx_line(int x0, int y0, int x1, int y1, u32 col) {
	if (gfx_us == 1) { line_raw(x0, y0, x1, y1, col); return; }
	for (int a = 0; a < gfx_us; ++a)
		for (int b = 0; b < gfx_us; ++b)
			line_raw(x0 * gfx_us + a, y0 * gfx_us + b, x1 * gfx_us + a, y1 * gfx_us + b, col);
}

void gfx_diamond(int cx, int cy, int hw, int hh, u32 col) {
	for (int y = -hh; y <= hh; ++y) {
		int w = hw * (hh - abs(y)) / (hh ? hh : 1);
		gfx_fill(cx - w, cy + y, 2 * w + 1, 1, col);
	}
}

/* elipse rellena: opaca (alpha 255) o mezclada con lo que hay debajo (sombras de las unidades y selección) */
void gfx_ellipse(int cx, int cy, int rx, int ry, u32 col, int alpha) {
	if (rx <= 0 || ry <= 0) return;
	for (int y = -ry; y <= ry; ++y) {
		int w = (int)(rx * sqrtf(1.0f - (float)(y * y) / (float)(ry * ry)));
		gfx_fill_a(cx - w, cy + y, 2 * w + 1, 1, col, alpha);
	}
}

/* sombra: oscurece lo que hay debajo (desplazamiento a la derecha y máscara, sin multiplicar) */
void gfx_shadow_ellipse(int cx, int cy, int rx, int ry) {
	cx *= gfx_us; cy *= gfx_us; rx *= gfx_us; ry *= gfx_us;
	if (rx <= 0 || ry <= 0) return;
	if (cx + rx < fb.cx0 || cx - rx >= fb.cx1 || cy + ry < fb.cy0 || cy - ry >= fb.cy1) return;
	for (int y = -ry; y <= ry; ++y) {
		int yy = cy + y;
		if (yy < fb.cy0 || yy >= fb.cy1) continue;
		int w = (int)(rx * sqrtf(1.0f - (float)(y * y) / (float)(ry * ry)));
		int x0 = MAX(cx - w, fb.cx0), x1 = MIN(cx + w + 1, fb.cx1);
		if (x1 > x0) px_dim(fb.px + (size_t)yy * fb.pitch + x0, x1 - x0, 1, 0x7f7f7f7fu);   /* mitad de luz */
	}
}

/* oscurece un rectángulo a la mitad (niebla de guerra, de las zonas ya exploradas) */
void gfx_dim(int x, int y, int w, int h) {
	int x0 = MAX(x * gfx_us, fb.cx0), y0 = MAX(y * gfx_us, fb.cy0), x1 = MIN((x + w) * gfx_us, fb.cx1), y1 = MIN((y + h) * gfx_us, fb.cy1);
	for (int yy = y0; yy < y1 && x1 > x0; ++yy) px_dim(fb.px + (size_t)yy * fb.pitch + x0, x1 - x0, 1, 0x7f7f7f7fu);
}

/* ---------- sprites ---------- */

/* paletas con sombra de luz: se guardan las últimas porque casi siempre se pide 140, 170 o 130 */
static u32 shade_pal[4][256];
static int shade_val[4] = { -1, -1, -1, -1 }, shade_next;
static int pal_version, shade_version[4];
void gfx_pal_changed(void) { ++pal_version; }

static const u32 *shaded_pal(int shade) {
	for (int k = 0; k < 4; ++k) if (shade_val[k] == shade && shade_version[k] == pal_version) return shade_pal[k];
	int k = shade_next; shade_next = (shade_next + 1) & 3;
	for (int i = 0; i < 256; ++i) {
		u32 c = fb.pal[i];
		shade_pal[k][i] = 0xff000000u | ((((c >> 16) & 255) * shade >> 8) << 16) | ((((c >> 8) & 255) * shade >> 8) << 8) | ((c & 255) * shade >> 8);
	}
	shade_val[k] = shade; shade_version[k] = pal_version;
	return shade_pal[k];
}

static inline void shadow_px(u32 *d) {
	u32 c = *d;
	*d = 0xff000000u | ((((c >> 16) & 255) * 140 >> 8) << 16) | ((((c >> 8) & 255) * 140 >> 8) << 8) | ((c & 255) * 140 >> 8);
}

/* escalado con filtrado bilineal de los colores de la paleta; el borde del sprite se mezcla con el fondo (sin dientes) */
static void sprite_smooth(const Sprite *s, int dx0, int dy0, int dw, int dh, int cx0, int cy0, int cx1, int cy1, int scale, int poff, const u32 *pal, int flip) {
	long long inv = ((long long)256 << 16) / scale;   /* paso en el origen por píxel de destino, 16.16 */
	static int xs[4096]; static u8 xf[4096];
	int n = cx1 - cx0;
	if (n > 4096) n = 4096;
	for (int i = 0; i < n; ++i) {
		int d = cx0 + i - dx0;
		if (flip) d = dw - 1 - d;   /* espejo: se recorre el origen al revés */
		long long p = (long long)d * inv + inv / 2 - 32768;
		if (p < 0) p = 0;
		int ix = (int)(p >> 16);
		xs[i] = ix; xf[i] = (u8)((p >> 8) & 255);
	}
	for (int yy = cy0; yy < cy1; ++yy) {
		long long py = (long long)(yy - dy0) * inv + inv / 2 - 32768;
		if (py < 0) py = 0;
		int iy = (int)(py >> 16), fy = (int)((py >> 8) & 255);
		int iy1 = MIN(iy + 1, s->h - 1);
		iy = MIN(iy, s->h - 1);
		const u8 *r0 = s->px + (size_t)iy * s->w, *r1 = s->px + (size_t)iy1 * s->w;
		const u8 *p0 = s->pc + (size_t)iy * s->w, *p1 = s->pc + (size_t)iy1 * s->w;
		u32 *dst = fb.px + (size_t)yy * fb.pitch;
		for (int i = 0; i < n; ++i) {
			int xx = cx0 + i;
			int ix0 = xs[i], fx = xf[i];
			int ix1 = MIN(ix0 + 1, s->w - 1);
			ix0 = MIN(ix0, s->w - 1);
			int v[4] = { r0[ix0], r0[ix1], r1[ix0], r1[ix1] };
			int pc[4] = { p0[ix0], p0[ix1], p1[ix0], p1[ix1] };
			int w[4] = { (256 - fx) * (256 - fy), fx * (256 - fy), (256 - fx) * fy, fx * fy };   /* suman 65536 */
			unsigned cr = 0, cg = 0, cb = 0, cov = 0;
			int nearest = (fy < 128 ? 0 : 2) + (fx < 128 ? 0 : 1);
			for (int k = 0; k < 4; ++k) {
				if (!v[k]) continue;
				u32 c = pal[pc[k] ? v[k] + poff : v[k]];
				if ((c & 0xffffff) == 0) continue;   /* los negros puros son sombras: no se filtran */
				cov += (unsigned)w[k];
				cr += ((c >> 16) & 255) * (unsigned)w[k]; cg += ((c >> 8) & 255) * (unsigned)w[k]; cb += (c & 255) * (unsigned)w[k];
			}
			if (!cov) {
				if (v[nearest] && ((pal[pc[nearest] ? v[nearest] + poff : v[nearest]]) & 0xffffff) == 0) shadow_px(&dst[xx]);
				continue;
			}
			unsigned r = cr / cov, g = cg / cov, b = cb / cov;
			if (cov >= 49152) { dst[xx] = 0xff000000u | (r << 16) | (g << 8) | b; continue; }   /* más de 3/4 de cobertura: sólido */
			unsigned a = cov * 256 / 49152;   /* cobertura de 0 a 256 */
			u32 d = dst[xx];
			unsigned dr = ((d >> 16) & 255), dg = ((d >> 8) & 255), db = (d & 255);
			dst[xx] = 0xff000000u | (((r * a + dr * (256 - a)) >> 8) << 16) | (((g * a + dg * (256 - a)) >> 8) << 8) | ((b * a + db * (256 - a)) >> 8);
		}
	}
}

void gfx_sprite(const Sprite *s, int x, int y, int scale, int player, int shade, int flip) {
	if (!s || !s->px || scale <= 0)
		return;
	x *= gfx_us; y *= gfx_us; scale *= gfx_us;

	int dw = (s->w * scale + 255) >> 8, dh = (s->h * scale + 255) >> 8;
	if (dw <= 0 || dh <= 0)
		return;

	int dx0 = x - ((s->hx * scale) >> 8), dy0 = y - ((s->hy * scale) >> 8);
	if (flip)
		dx0 = x - dw + ((s->hx * scale) >> 8);

	int cx0 = MAX(dx0, fb.cx0), cy0 = MAX(dy0, fb.cy0), cx1 = MIN(dx0 + dw, fb.cx1), cy1 = MIN(dy0 + dh, fb.cy1);
	if (cx0 >= cx1 || cy0 >= cy1)
		return;

	int poff = 16 * (player + 1);
	const u32 *pal = shade >= 256 ? fb.pal : shaded_pal(shade);

	if (gfx_smooth && scale != 256 && cx1 - cx0 <= 4096) {
		sprite_smooth(s, dx0, dy0, dw, dh, cx0, cy0, cx1, cy1, scale, poff, pal, flip);
		return;
	}

	/* tablas de columnas: sin una división por píxel */
	static int colmap[4096];
	int n = cx1 - cx0;
	if (n > 4096) return;
	long long step = ((long long)256 << 16) / scale;
	long long pos = (long long)(cx0 - dx0) * step;
	for (int i = 0; i < n; ++i, pos += step) {
		int sx = (int)(pos >> 16);
		if (sx >= s->w) sx = s->w - 1;
		colmap[i] = flip ? s->w - 1 - sx : sx;
	}

	for (int yy = cy0; yy < cy1; ++yy) {
		int sy = (int)(((long long)(yy - dy0) * step) >> 16);
		if (sy >= s->h)
			sy = s->h - 1;
		const u8 *row = s->px + (size_t)sy * s->w, *prow = s->pc + (size_t)sy * s->w;
		u32 *dst = fb.px + (size_t)yy * fb.pitch + cx0;

		for (int i = 0; i < n; ++i) {
			int sx = colmap[i];
			u8 v = row[sx];
			if (!v)
				continue;
			if (prow[sx])
				v = (u8)(v + poff);
			u32 c = pal[v];
			if ((c & 0xffffff) == 0)
				shadow_px(&dst[i]);   /* los negros puros son sombras: se oscurece lo de debajo en vez de pintar un bloque */
			else
				dst[i] = c;
		}
	}
}

/* ---------- fuente: glifos de 5x7, filas separadas por '|' ---------- */

typedef struct { unsigned cp; const char *rows; } Glyph;

static const Glyph glyphs[] = {
	{' ', ".....|.....|.....|.....|.....|.....|....."},
	{'A', ".###.|#...#|#...#|#####|#...#|#...#|#...#"},
	{'B', "####.|#...#|#...#|####.|#...#|#...#|####."},
	{'C', ".###.|#...#|#....|#....|#....|#...#|.###."},
	{'D', "####.|#...#|#...#|#...#|#...#|#...#|####."},
	{'E', "#####|#....|#....|####.|#....|#....|#####"},
	{'F', "#####|#....|#....|####.|#....|#....|#...."},
	{'G', ".###.|#...#|#....|#.###|#...#|#...#|.###."},
	{'H', "#...#|#...#|#...#|#####|#...#|#...#|#...#"},
	{'I', ".###.|..#..|..#..|..#..|..#..|..#..|.###."},
	{'J', "..###|...#.|...#.|...#.|...#.|#..#.|.##.."},
	{'K', "#...#|#..#.|#.#..|##...|#.#..|#..#.|#...#"},
	{'L', "#....|#....|#....|#....|#....|#....|#####"},
	{'M', "#...#|##.##|#.#.#|#.#.#|#...#|#...#|#...#"},
	{'N', "#...#|##..#|#.#.#|#..##|#...#|#...#|#...#"},
	{'O', ".###.|#...#|#...#|#...#|#...#|#...#|.###."},
	{'P', "####.|#...#|#...#|####.|#....|#....|#...."},
	{'Q', ".###.|#...#|#...#|#...#|#.#.#|#..#.|.##.#"},
	{'R', "####.|#...#|#...#|####.|#.#..|#..#.|#...#"},
	{'S', ".####|#....|#....|.###.|....#|....#|####."},
	{'T', "#####|..#..|..#..|..#..|..#..|..#..|..#.."},
	{'U', "#...#|#...#|#...#|#...#|#...#|#...#|.###."},
	{'V', "#...#|#...#|#...#|#...#|#...#|.#.#.|..#.."},
	{'W', "#...#|#...#|#...#|#.#.#|#.#.#|##.##|#...#"},
	{'X', "#...#|#...#|.#.#.|..#..|.#.#.|#...#|#...#"},
	{'Y', "#...#|#...#|.#.#.|..#..|..#..|..#..|..#.."},
	{'Z', "#####|....#|...#.|..#..|.#...|#....|#####"},
	{'0', ".###.|#...#|#..##|#.#.#|##..#|#...#|.###."},
	{'1', "..#..|.##..|..#..|..#..|..#..|..#..|.###."},
	{'2', ".###.|#...#|....#|...#.|..#..|.#...|#####"},
	{'3', "####.|....#|....#|.###.|....#|....#|####."},
	{'4', "...#.|..##.|.#.#.|#..#.|#####|...#.|...#."},
	{'5', "#####|#....|####.|....#|....#|#...#|.###."},
	{'6', "..##.|.#...|#....|####.|#...#|#...#|.###."},
	{'7', "#####|....#|...#.|..#..|.#...|.#...|.#..."},
	{'8', ".###.|#...#|#...#|.###.|#...#|#...#|.###."},
	{'9', ".###.|#...#|#...#|.####|....#|...#.|.##.."},
	{'.', ".....|.....|.....|.....|.....|.##..|.##.."},
	{',', ".....|.....|.....|.....|.##..|..#..|.#..."},
	{':', ".....|.##..|.##..|.....|.##..|.##..|....."},
	{';', ".....|.##..|.##..|.....|.##..|..#..|.#..."},
	{'!', "..#..|..#..|..#..|..#..|..#..|.....|..#.."},
	{'?', ".###.|#...#|....#|...#.|..#..|.....|..#.."},
	{'-', ".....|.....|.....|#####|.....|.....|....."},
	{'+', ".....|..#..|..#..|#####|..#..|..#..|....."},
	{'=', ".....|.....|#####|.....|#####|.....|....."},
	{'/', "....#|....#|...#.|..#..|.#...|#....|#...."},
	{'\\', "#....|#....|.#...|..#..|...#.|....#|....#"},
	{'@', ".###.|#...#|#.###|#.#.#|#.###|#....|.###."},
	{'&', ".##..|#..#.|.##..|.#.#.|#..##|#..#.|.##.#"},
	{'[', ".###.|.#...|.#...|.#...|.#...|.#...|.###."},
	{']', ".###.|...#.|...#.|...#.|...#.|...#.|.###."},
	{'%', "##..#|##..#|...#.|..#..|.#...|#..##|#..##"},
	{'(', "..#..|.#...|#....|#....|#....|.#...|..#.."},
	{')', "..#..|...#.|....#|....#|....#|...#.|..#.."},
	{'<', "...#.|..#..|.#...|#....|.#...|..#..|...#."},
	{'>', ".#...|..#..|...#.|....#|...#.|..#..|.#..."},
	{'*', ".....|#.#.#|.###.|#####|.###.|#.#.#|....."},
	{'_', ".....|.....|.....|.....|.....|.....|#####"},
	{'\'', "..#..|..#..|.#...|.....|.....|.....|....."},
	{'"', ".#.#.|.#.#.|.#.#.|.....|.....|.....|....."},
	{'#', ".#.#.|#####|.#.#.|.#.#.|.#.#.|#####|.#.#."},
	{0xd1, ".#.#.|#...#|##..#|#.#.#|#..##|#...#|#...#"}, /* N con tilde */
	{0xbf, "..#..|.....|..#..|...#.|..#..|#...#|.###."}, /* ? invertida */
	{0xa1, "..#..|.....|..#..|..#..|..#..|..#..|..#.."}, /* ! invertida */
};

static const Glyph *find_glyph(unsigned cp) {
	for (int i = 0; i < ARRAY_LEN(glyphs); ++i)
		if (glyphs[i].cp == cp)
			return &glyphs[i];
	return NULL;
}

/* decodifica un punto de código UTF-8 y lo asigna al glifo base (los acentos se dibujan como una marca encima) */
static unsigned next_cp(const char **ps, int *accent) {
	const unsigned char *s = (const unsigned char *)*ps;
	unsigned cp = *s;
	int n = 1;

	*accent = 0;
	if (cp >= 0xf0) { cp &= 7; n = 4; }
	else if (cp >= 0xe0) { cp &= 15; n = 3; }
	else if (cp >= 0xc0) { cp &= 31; n = 2; }

	for (int i = 1; i < n; ++i)
		cp = (cp << 6) | (s[i] & 63);
	*ps += n;

	switch (cp) {
	case 0xe1: case 0xc1: *accent = 1; return 'A';
	case 0xe9: case 0xc9: *accent = 1; return 'E';
	case 0xed: case 0xcd: *accent = 1; return 'I';
	case 0xf3: case 0xd3: *accent = 1; return 'O';
	case 0xfa: case 0xda: *accent = 1; return 'U';
	case 0xfc: case 0xdc: *accent = 2; return 'U';
	case 0xf1: return 0xd1;
	}

	if (cp >= 'a' && cp <= 'z')
		cp -= 32;
	return cp;
}

int gfx_text_w(const char *s, int size) {
	int n = 0, acc;
	while (*s) {
		next_cp(&s, &acc);
		++n;
	}
	return n * 6 * size;
}

void gfx_text(int x, int y, const char *s, u32 col, int size) {
	int acc, us = gfx_us;
	x *= us; y *= us;
	int cell = size * us;

	while (*s) {
		unsigned cp = next_cp(&s, &acc);
		const Glyph *g = find_glyph(cp);

		if (g) {
			const char *r = g->rows;
			for (int row = 0; row < 7; ++row) {
				for (int c = 0; c < 5; ++c)
					if (r[c] == '#')
						fill_raw(x + c * cell, y + row * cell, cell, cell, col);
				r += 6;
			}

			if (acc == 1) {
				fill_raw(x + 3 * cell, y - cell, cell, cell, col);
			} else if (acc == 2) {
				fill_raw(x + 1 * cell, y - cell, cell, cell, col);
				fill_raw(x + 3 * cell, y - cell, cell, cell, col);
			}
		}
		x += 6 * cell;
	}
}

void gfx_text_c(int cx, int y, const char *s, u32 col, int size) {
	gfx_text(cx - gfx_text_w(s, size) / 2, y, s, col, size);
}

/* ---------- texto de la interfaz: fuente de 8x16 con minúsculas y acentos (v8.5.4.7) ----------
 * En SaguiOS se usa la fuente del sistema (fonte_sagui, Latin-1, de la libsagui): legible a 1x y con todos los acentos.
 * Fuera de SaguiOS (sin AOEN_FONTE_SAGUI) se usa la de 5x7 de arriba, centrada en la misma línea de 16 píxeles. */
#ifdef AOEN_FONTE_SAGUI
extern const unsigned char fonte_sagui[256][16];
#endif

static unsigned cp_utf8(const char **ps) {
	const unsigned char *s = (const unsigned char *)*ps;
	unsigned cp = *s;
	int n = 1;
	if (cp >= 0xf0) { cp &= 7; n = 4; }
	else if (cp >= 0xe0) { cp &= 15; n = 3; }
	else if (cp >= 0xc0) { cp &= 31; n = 2; }
	for (int i = 1; i < n && s[i]; ++i) cp = (cp << 6) | (s[i] & 63);
	for (int i = 0; i < n && **ps; ++i) ++*ps;
	return cp;
}

int gfx_t_w(const char *s, int tam) {
	int n = 0;
	while (*s) { cp_utf8(&s); ++n; }
#ifdef AOEN_FONTE_SAGUI
	return n * 8 * tam;
#else
	return n * 6 * tam;
#endif
}

static void texto8x16(int x, int y, const char *s, u32 col, int tam) {
#ifdef AOEN_FONTE_SAGUI
	int us = gfx_us, cell = tam * us;
	x *= us; y *= us;
	while (*s) {
		unsigned cp = cp_utf8(&s);
		if (cp > 255) cp = '?';
		const unsigned char *g = fonte_sagui[cp];
		for (int r = 0; r < 16; ++r) {
			unsigned b = g[r];
			if (!b) continue;
			int yy = y + r * cell;
			/* tramos seguidos de píxeles encendidos: un relleno por tramo */
			for (int c = 0; c < 8;) {
				if (!(b & (0x80u >> c))) { ++c; continue; }
				int c0 = c;
				while (c < 8 && (b & (0x80u >> c))) ++c;
				fill_raw(x + c0 * cell, yy, (c - c0) * cell, cell, col);
			}
		}
		x += 8 * cell;
	}
#else
	gfx_text(x, y + 4 * tam, s, col, tam);
#endif
}

void gfx_t(int x, int y, const char *s, u32 col, int tam) { texto8x16(x, y, s, col, tam); }

/* con sombra oscura abajo a la derecha: se lee sobre cualquier fondo */
void gfx_ts(int x, int y, const char *s, u32 col, int tam) {
	texto8x16(x + 1, y + 1, s, 0xff0a0604u, tam);
	texto8x16(x, y, s, col, tam);
}
