/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Terreno "HD" opcional: texturas PNG sin costuras proyectadas sobre el suelo isométrico, con transiciones suaves, sombreado de niebla y agua en movimiento. */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"
#include "ui.h"
#include "config.h"
#include "hilos.h"

#define TEX 512
#define TEXELS_PER_TILE 48

typedef struct { u32 px[TEX * TEX]; u32 avg; } Tex;

static Tex *tex_grass[2], *tex_desert[2], *tex_water, *tex_deep;
static int loaded;
int hd_enabled;

static Tex *load_tex(const char *dir, const char *name) {
	static const char *suffix[] = { "_00_color.png", "_00_COLOR.png" };
	for (int k = 0; k < 2; ++k) {
		char path[1300];
		snprintf(path, sizeof path, "%s/%s%s", dir, name, suffix[k]);
		FILE *f = fopen(path, "rb");
		if (!f) continue;
		fseek(f, 0, SEEK_END);
		long sz = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (sz <= 0 || sz > 16 * 1024 * 1024) { fclose(f); continue; }
		unsigned char *buf = (unsigned char *)malloc((size_t)sz);
		if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); continue; }
		fclose(f);

		int w, h, n;
		unsigned char *img = stbi_load_from_memory(buf, (int)sz, &w, &h, &n, 3);
		free(buf);
		if (!img) continue;
		if (w != TEX || h != TEX) { stbi_image_free(img); continue; } /* solo se acepta el tamaño esperado */

		Tex *t = (Tex *)malloc(sizeof *t);
		if (!t) { stbi_image_free(img); continue; }
		unsigned long long r = 0, g = 0, b = 0;
		for (int i = 0; i < TEX * TEX; ++i) {
			u32 pr = img[i * 3], pg = img[i * 3 + 1], pb = img[i * 3 + 2];
			t->px[i] = 0xff000000u | (pr << 16) | (pg << 8) | pb;
			r += pr; g += pg; b += pb;
		}
		t->avg = 0xff000000u | ((u32)(r / (TEX * TEX)) << 16) | ((u32)(g / (TEX * TEX)) << 8) | (u32)(b / (TEX * TEX));
		stbi_image_free(img);
		return t;
	}
	return NULL;
}

int tex_load(const char *dir) {
	if (!dir || !*dir) return 0;
	tex_grass[0] = load_tex(dir, "g_gr2"); tex_grass[1] = load_tex(dir, "g_gr6");
	tex_desert[0] = load_tex(dir, "g_des"); tex_desert[1] = load_tex(dir, "g_pal");
	tex_water = load_tex(dir, "g_wt5"); tex_deep = load_tex(dir, "g_wt3");
	loaded = tex_grass[0] && tex_desert[0] && tex_water && tex_deep;
	if (!tex_grass[1]) tex_grass[1] = tex_grass[0];
	if (!tex_desert[1]) tex_desert[1] = tex_desert[0];
	hd_enabled = loaded;
	return loaded;
}

int tex_available(void) { return loaded; }

u32 tex_tile_color(int kind) {
	if (!loaded) return 0;
	switch (kind) {
	case TILE_GRASS: return tex_grass[0]->avg;
	case TILE_DESERT: return tex_desert[0]->avg;
	case TILE_WATER: return tex_water->avg;
	default: return tex_deep->avg;
	}
}

static inline u32 lerp_col(u32 a, u32 b, int t) { /* t 0..256 */
	u32 r = (((a >> 16) & 255) * (256 - t) + ((b >> 16) & 255) * t) >> 8;
	u32 g = (((a >> 8) & 255) * (256 - t) + ((b >> 8) & 255) * t) >> 8;
	u32 bl = ((a & 255) * (256 - t) + (b & 255) * t) >> 8;
	return 0xff000000u | (r << 16) | (g << 8) | bl;
}

static float smooth_noise(float x, float y) {
	int xi = (int)floorf(x), yi = (int)floorf(y);
	float fx = x - xi, fy = y - yi;
	fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
	#define H(a, b) ((float)(((unsigned)(a) * 374761393u + (unsigned)(b) * 668265263u) * 1274126177u >> 16 & 0xffff) / 65535.0f)
	float a = H(xi, yi), b = H(xi + 1, yi), c = H(xi, yi + 1), d = H(xi + 1, yi + 1);
	#undef H
	return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

/* color de un tipo de terreno en una posición del mundo */
static inline u32 kind_color(int kind, float wx, float wy, float time) {
	float u = wx * TEXELS_PER_TILE, v = wy * TEXELS_PER_TILE;
	if (kind >= TILE_WATER) {
		/* olas suaves y deriva */
		u += sinf(wy * 1.7f + time * 0.9f) * 5.0f + time * 3.0f;
		v += sinf(wx * 1.3f + time * 0.7f) * 5.0f;
		const Tex *t = kind == TILE_WATER ? tex_water : tex_deep;
		return t->px[((int)v & (TEX - 1)) * TEX + ((int)u & (TEX - 1))];
	}
	Tex **set = kind == TILE_GRASS ? tex_grass : tex_desert;
	u32 a = set[0]->px[((int)v & (TEX - 1)) * TEX + ((int)u & (TEX - 1))];
	if (set[1] == set[0]) return a;
	/* variación en manchas entre las dos texturas del mismo tipo */
	float n = smooth_noise(wx * 0.11f, wy * 0.11f);
	int t = (int)(CLAMP((n - 0.4f) * 5.0f, 0.0f, 1.0f) * 256);
	if (t == 0) return a;
	u32 b = set[1]->px[((int)(v + 97) & (TEX - 1)) * TEX + ((int)(u + 211) & (TEX - 1))];
	return lerp_col(a, b, t);
}

static void render_terrain_hd_float(float time, int ry0, int ry1) {
	float z = V.zoom;
	/* posición en el mundo del píxel superior izquierdo, y pasos */
	float pxp0 = (0 - V.sw * 0.5f) / z + V.cx, pyp0 = (0 - V.sh * 0.5f) / z + V.cy;
	float wx0 = pxp0 / 64.0f + pyp0 / 32.0f, wy0 = pxp0 / 64.0f - pyp0 / 32.0f;
	float dxx = 1.0f / (64.0f * z), dxy = 1.0f / (32.0f * z); /* d(wx)/dsx, d(wx)/dsy */
	int W_ = W.w, H_ = W.h;

	for (int sy = ry0; sy < ry1; ++sy) {
		float wx = wx0 + dxy * sy, wy = wy0 - dxy * sy;
		u32 *dst = fb.px + (size_t)sy * fb.pitch;

		for (int sx = 0; sx < fb.w; ++sx, wx += dxx, wy += dxx) {
			if (wx < 0 || wy < 0 || wx >= W_ || wy >= H_) { dst[sx] = 0xff000000u; continue; }

			/* los cuatro centros de casilla alrededor de este punto */
			float fx = wx - 0.5f, fy = wy - 0.5f;
			int tx = (int)floorf(fx), ty = (int)floorf(fy);
			float ax = fx - tx, ay = fy - ty;
			int x0 = CLAMP(tx, 0, W_ - 1), x1 = CLAMP(tx + 1, 0, W_ - 1), y0 = CLAMP(ty, 0, H_ - 1), y1 = CLAMP(ty + 1, 0, H_ - 1);
			int i00 = y0 * W_ + x0, i10 = y0 * W_ + x1, i01 = y1 * W_ + x0, i11 = y1 * W_ + x1;

			/* niebla: bilineal para que el borde de la zona explorada sea suave */
			float lv00 = W.fog[i00] == 2 ? 1.0f : W.fog[i00] == 1 ? 0.5f : 0.0f;
			float lv10 = W.fog[i10] == 2 ? 1.0f : W.fog[i10] == 1 ? 0.5f : 0.0f;
			float lv01 = W.fog[i01] == 2 ? 1.0f : W.fog[i01] == 1 ? 0.5f : 0.0f;
			float lv11 = W.fog[i11] == 2 ? 1.0f : W.fog[i11] == 1 ? 0.5f : 0.0f;
			float light = (lv00 * (1 - ax) + lv10 * ax) * (1 - ay) + (lv01 * (1 - ax) + lv11 * ax) * ay;
			if (light <= 0.01f) { dst[sx] = 0xff000000u; continue; }

			int k00 = W.tile[i00], k10 = W.tile[i10], k01 = W.tile[i01], k11 = W.tile[i11];
			u32 col;
			if (k00 == k10 && k00 == k01 && k00 == k11) {
				col = kind_color(k00, wx, wy, time);
			} else {
				/* peso de cada tipo, realzado para que la línea de costa quede clara pero sin dientes */
				float w[4] = { 0, 0, 0, 0 };
				w[k00] += (1 - ax) * (1 - ay); w[k10] += ax * (1 - ay);
				w[k01] += (1 - ax) * ay; w[k11] += ax * ay;
				float tot = 0;
				for (int k = 0; k < 4; ++k) {
					if (w[k] > 0) { w[k] = CLAMP((w[k] - 0.5f) * 3.0f + 0.5f, 0.0f, 1.0f); tot += w[k]; }
				}
				if (tot <= 0) tot = 1, w[k00] = 1;
				float r = 0, g = 0, b = 0;
				for (int k = 0; k < 4; ++k) {
					if (w[k] <= 0) continue;
					u32 c = kind_color(k, wx, wy, time);
					r += ((c >> 16) & 255) * w[k]; g += ((c >> 8) & 255) * w[k]; b += (c & 255) * w[k];
				}
				col = 0xff000000u | ((u32)(r / tot) << 16) | ((u32)(g / tot) << 8) | (u32)(b / tot);
			}

			if (light < 0.99f) {
				int l = (int)(light * 256);
				col = 0xff000000u | ((((col >> 16) & 255) * l >> 8) << 16) | ((((col >> 8) & 255) * l >> 8) << 8) | ((col & 255) * l >> 8);
			}
			dst[sx] = col;
		}
	}
}

/* ---------- versión en punto fijo (v8.5.4.7) ----------
 * La de coma flotante hacía por píxel dos senos (agua), un ruido con floorf y cuatro hashes (manchas de hierba y arena),
 * la niebla bilineal en float y las mezclas de la costa en float: en SaguiOS (x87) daba unos 13 fps a 1024x768.
 * Esta hace lo mismo con enteros:
 *  - posición en el mundo en 16.16, que avanza sumando un paso constante por píxel;
 *  - el ruido de las manchas se precalcula en las esquinas de las casillas (una vez por mapa) y se interpola dentro;
 *    sus rasgos miden unas 9 casillas, así que la interpolación no se nota;
 *  - las ondas del agua salen de una tabla de senos con la fase en 0.32 (una vuelta = 2^32, se desborda sola);
 *  - niebla y mezclas con pesos de 0 a 256.
 * AOEN_HD_FLOAT=1 vuelve a la versión antigua (para comparar imágenes y medir). */

static u16 *ruido;               /* ruido de manchas en las esquinas de las casillas, (W+1)x(H+1), 0..65535 */
static int ruido_w, ruido_h;
static const void *ruido_mundo;
static int16_t ola_lut[1024];    /* sin * 5 texeles, en 8.8 */
static int ola_ok;

static void ruido_preparar(void) {
	if (ruido && ruido_mundo == (const void *)W.tile && ruido_w == W.w + 1 && ruido_h == W.h + 1) return;
	free(ruido);
	ruido_w = W.w + 1; ruido_h = W.h + 1; ruido_mundo = W.tile;
	ruido = (u16 *)malloc((size_t)ruido_w * ruido_h * sizeof(u16));
	if (!ruido) return;
	for (int j = 0; j < ruido_h; ++j)
		for (int i = 0; i < ruido_w; ++i) {
			float n = smooth_noise(i * 0.11f, j * 0.11f);
			ruido[j * ruido_w + i] = (u16)CLAMP((int)(n * 65535.0f), 0, 65535);
		}
	if (!ola_ok) {
		for (int k = 0; k < 1024; ++k) ola_lut[k] = (int16_t)floorf(sinf(k * 6.2831853f / 1024.0f) * 5.0f * 256.0f + 0.5f);
		ola_ok = 1;
	}
}

/* redondeo al entero más cercano sin lrint (la libc de SaguiOS no lo tiene) */
static long long redondear(double v) { return (long long)floor(v + 0.5); }

static inline u32 tex_px(const Tex *t, int u, int v) { return t->px[(v & (TEX - 1)) * TEX + (u & (TEX - 1))]; }

static inline u32 mezcla8(u32 a, u32 b, int t) {   /* t 0..256 de a hacia b */
	u32 it = (u32)(256 - t), ut = (u32)t;
	u32 rb = (((a & 0xff00ffu) * it + (b & 0xff00ffu) * ut) >> 8) & 0xff00ffu;
	u32 g = (((a & 0x00ff00u) * it + (b & 0x00ff00u) * ut) >> 8) & 0x00ff00u;
	return 0xff000000u | rb | g;
}

typedef struct {
	u32 uf, vf;          /* coordenadas de textura en 16.16 (texeles); sin signo: se desbordan sin problema, solo cuentan los bits bajos */
	u32 fa, fb_;         /* fases de las ondas del agua (una vuelta = 2^32) */
	int du_t, dv_t;      /* deriva del agua por el tiempo, en 8.8 */
	int wxf, wyf;        /* posición en el mundo, 16.16 */
} PuntoHD;

/* color de un tipo de terreno en un punto (versión entera de kind_color) */
static inline u32 color_tipo(int kind, const PuntoHD *p) {
	if (kind >= TILE_WATER) {
		int u = (int)((p->uf >> 8) & 0xffffff) + ola_lut[p->fa >> 22] + p->du_t, v = (int)((p->vf >> 8) & 0xffffff) + ola_lut[p->fb_ >> 22];   /* 8.8 */
		return tex_px(kind == TILE_WATER ? tex_water : tex_deep, u >> 8, v >> 8);
	}
	Tex **set = kind == TILE_GRASS ? tex_grass : tex_desert;
	int u = (int)(p->uf >> 16), v = (int)(p->vf >> 16);
	u32 a = tex_px(set[0], u, v);
	if (set[1] == set[0] || !ruido) return a;
	/* manchas: ruido bilineal entre las esquinas de la casilla */
	int ix = p->wxf >> 16, iy = p->wyf >> 16, fx = (p->wxf >> 8) & 255, fy = (p->wyf >> 8) & 255;
	if (ix >= ruido_w - 1) ix = ruido_w - 2;
	if (iy >= ruido_h - 1) iy = ruido_h - 2;
	const u16 *r0 = ruido + iy * ruido_w + ix, *r1 = r0 + ruido_w;
	int n0 = r0[0] + (((r0[1] - r0[0]) * fx) >> 8), n1 = r1[0] + (((r1[1] - r1[0]) * fx) >> 8);
	int n = n0 + (((n1 - n0) * fy) >> 8);
	int t = ((n - 26214) * 5) >> 8;   /* (n - 0.4) * 5, de 0 a 256 */
	if (t <= 0) return a;
	if (t > 256) t = 256;
	return mezcla8(a, tex_px(set[1], u + 211, v + 97), t);
}

/* las filas [ry0, ry1) en punto fijo; cada fila depende solo de la vista, el mapa y la niebla (D393: franjas en paralelo) */
static void render_terrain_hd_filas(float time, int ry0, int ry1) {
	float z = V.zoom;
	float pxp0 = (0 - V.sw * 0.5f) / z + V.cx, pyp0 = (0 - V.sh * 0.5f) / z + V.cy;
	float wx0 = pxp0 / 64.0f + pyp0 / 32.0f, wy0 = pxp0 / 64.0f - pyp0 / 32.0f;
	float dxx = 1.0f / (64.0f * z), dxy = 1.0f / (32.0f * z);
	const int W_ = W.w, H_ = W.h;
	const int lim_x = W_ << 16, lim_y = H_ << 16;
	const int paso = (int)redondear(dxx * 65536.0f);              /* wx y wy avanzan lo mismo por píxel */
	const u32 paso_t = (u32)redondear(dxx * TEXELS_PER_TILE * 65536.0f);
	/* fases de las ondas: sin(wy*1.7 + t*0.9) en u y sin(wx*1.3 + t*0.7) en v */
	const double vuelta = 4294967296.0 / 6.283185307179586;
	const u32 dfa = (u32)redondear(dxx * 1.7 * vuelta), dfb = (u32)redondear(dxx * 1.3 * vuelta);
	const int deriva = (int)redondear(fmodf(time * 3.0f, (float)TEX) * 256.0f);
	static const int luz_lv[3] = { 0, 128, 256 };

	for (int sy = ry0; sy < ry1; ++sy) {
		double wxr = wx0 + (double)dxy * sy, wyr = wy0 - (double)dxy * sy;
		PuntoHD p;
		p.wxf = (int)redondear(wxr * 65536.0); p.wyf = (int)redondear(wyr * 65536.0);
		p.uf = (u32)redondear(wxr * TEXELS_PER_TILE * 65536.0); p.vf = (u32)redondear(wyr * TEXELS_PER_TILE * 65536.0);
		p.fa = (u32)redondear(fmod(wyr * 1.7 + time * 0.9, 6.283185307179586 * 1024.0) * vuelta);
		p.fb_ = (u32)redondear(fmod(wxr * 1.3 + time * 0.7, 6.283185307179586 * 1024.0) * vuelta);
		p.du_t = deriva; p.dv_t = 0;
		u32 *dst = fb.px + (size_t)sy * fb.pitch;

		for (int sx = 0; sx < fb.w; ++sx, p.wxf += paso, p.wyf += paso, p.uf += paso_t, p.vf += paso_t, p.fa += dfa, p.fb_ += dfb) {
			if ((unsigned)p.wxf >= (unsigned)lim_x || (unsigned)p.wyf >= (unsigned)lim_y) { dst[sx] = 0xff000000u; continue; }

			/* los cuatro centros de casilla alrededor de este punto */
			int fx = p.wxf - 32768, fy = p.wyf - 32768;
			int tx = fx >> 16, ty = fy >> 16, ax = (fx >> 8) & 255, ay = (fy >> 8) & 255;
			int x0 = tx < 0 ? 0 : tx, x1 = tx + 1 >= W_ ? W_ - 1 : tx + 1, y0 = ty < 0 ? 0 : ty, y1 = ty + 1 >= H_ ? H_ - 1 : ty + 1;
			int i00 = y0 * W_ + x0, i10 = y0 * W_ + x1, i01 = y1 * W_ + x0, i11 = y1 * W_ + x1;

			/* niebla bilineal */
			int f00 = W.fog[i00], f10 = W.fog[i10], f01 = W.fog[i01], f11 = W.fog[i11];
			int light;
			if (f00 == 2 && f10 == 2 && f01 == 2 && f11 == 2) light = 256;
			else {
				int l0 = luz_lv[f00] * (256 - ax) + luz_lv[f10] * ax, l1 = luz_lv[f01] * (256 - ax) + luz_lv[f11] * ax;
				light = (l0 * (256 - ay) + l1 * ay) >> 16;
				if (light <= 2) { dst[sx] = 0xff000000u; continue; }
			}

			int k00 = W.tile[i00], k10 = W.tile[i10], k01 = W.tile[i01], k11 = W.tile[i11];
			u32 col;
			if (k00 == k10 && k00 == k01 && k00 == k11) {
				col = color_tipo(k00, &p);
			} else {
				/* pesos de cada tipo (0..65536), realzados para que la costa quede clara sin dientes */
				int w[4] = { 0, 0, 0, 0 };
				w[k00] += (256 - ax) * (256 - ay); w[k10] += ax * (256 - ay);
				w[k01] += (256 - ax) * ay; w[k11] += ax * ay;
				int tot = 0;
				for (int k = 0; k < 4; ++k)
					if (w[k] > 0) { w[k] = CLAMP((w[k] - 32768) * 3 + 32768, 0, 65536) >> 8; tot += w[k]; }
				if (tot <= 0) { tot = 256; w[0] = w[1] = w[2] = w[3] = 0; w[k00] = 256; }
				unsigned r = 0, g = 0, b = 0;
				for (int k = 0; k < 4; ++k) {
					if (w[k] <= 0) continue;
					u32 c = color_tipo(k, &p);
					r += ((c >> 16) & 255) * (unsigned)w[k]; g += ((c >> 8) & 255) * (unsigned)w[k]; b += (c & 255) * (unsigned)w[k];
				}
				col = 0xff000000u | ((r / (unsigned)tot) << 16) | ((g / (unsigned)tot) << 8) | (b / (unsigned)tot);
			}
			if (light < 253) {
				u32 l = (u32)light;
				col = 0xff000000u | ((((col & 0xff00ffu) * l) >> 8) & 0xff00ffu) | ((((col & 0x00ff00u) * l) >> 8) & 0x00ff00u);
			}
			dst[sx] = col;
		}
	}
}

/* ---------- reparto en franjas (D393) ----------
 * Las filas de la pantalla son independientes: cada hilo dibuja franjas enteras. Lo que se prepara una vez (el ruido de
 * las manchas, la tabla de ondas, la elección de la versión) se hace antes en el hilo principal, así los hilos solo leen. */
typedef struct { float time; int y0, y1, flotante; } FranjasHD;

static void franja_hd(void *ctx, int k, int partes) {
	const FranjasHD *f = (const FranjasHD *)ctx;
	int h = f->y1 - f->y0, a = f->y0 + h * k / partes, b = f->y0 + h * (k + 1) / partes;
	if (f->flotante) render_terrain_hd_float(f->time, a, b);
	else render_terrain_hd_filas(f->time, a, b);
}

void render_terrain_hd(float time, int ry0, int ry1) {
	ry0 = CLAMP(ry0, 0, fb.h); ry1 = CLAMP(ry1, ry0, fb.h);
	static int usar_float = -1;
	if (usar_float < 0) usar_float = getenv("AOEN_HD_FLOAT") && atoi(getenv("AOEN_HD_FLOAT")) > 0;
	if (!usar_float) ruido_preparar();
	FranjasHD f = { time, ry0, ry1, usar_float };
	hilos_repartir(franja_hd, &f, hilos_partes());
}
