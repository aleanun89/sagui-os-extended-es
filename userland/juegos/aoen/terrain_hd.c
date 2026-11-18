/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Terreno "HD" opcional: texturas PNG sin costuras proyectadas sobre el suelo isométrico, con transiciones suaves, sombreado de niebla y agua en movimiento. */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"
#include "ui.h"
#include "config.h"

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

void render_terrain_hd(float time) {
	float z = V.zoom;
	/* posición en el mundo del píxel superior izquierdo, y pasos */
	float pxp0 = (0 - V.sw * 0.5f) / z + V.cx, pyp0 = (0 - V.sh * 0.5f) / z + V.cy;
	float wx0 = pxp0 / 64.0f + pyp0 / 32.0f, wy0 = pxp0 / 64.0f - pyp0 / 32.0f;
	float dxx = 1.0f / (64.0f * z), dxy = 1.0f / (32.0f * z); /* d(wx)/dsx, d(wx)/dsy */
	int W_ = W.w, H_ = W.h;

	for (int sy = 0; sy < fb.h; ++sy) {
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
