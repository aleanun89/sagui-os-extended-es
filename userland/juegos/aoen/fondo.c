/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Fondo propio de los menús (v8.5.4.10): un paisaje de la Antigüedad al atardecer dibujado por código, sin imágenes de
 * nadie, para cuando faltan los fondos originales de INTERFAC.DRS. Se pinta una sola vez en una rejilla pequeña (pixel
 * art: la altura se divide en ~270 filas), se amplía a un búfer del tamaño de la ventana y cada cuadro solo se copia.
 * Fuera de «Equipos lentos» se le suman unas nubes y destellos en el agua (unos pocos rectángulos por cuadro). */
#include "ui.h"
#include "efectos.h"

static u32 *bg;            /* paisaje ya ampliado, w*h píxeles */
static int bg_w, bg_h, bg_c;   /* tamaño del búfer y factor de ampliación */
static int g_lw, g_lh, g_hy, g_sx, g_sy, g_sr;   /* rejilla baja, horizonte y sol */

typedef struct { int r, g, b; } Cor;

static int lerp(int a, int b, int t) { return a + (b - a) * t / 256; }
static Cor mezcla(Cor a, Cor b, int t) { Cor c = { lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t) }; return c; }
/* onda triangular 0..256 de periodo `per` */
static int tri(int p, int per) {
	if (per < 2) return 0;
	p %= per; if (p < 0) p += per;
	int d = p * 2 - per; if (d < 0) d = -d;
	return d * 256 / per;
}
/* degradado de varias paradas, t en 0..256 */
static Cor grad(const Cor *s, int n, int t) {
	int seg = CLAMP(t, 0, 255) * (n - 1);
	int i = seg / 256;
	return mezcla(s[i], s[i + 1], seg % 256);
}

static u32 *lo;   /* rejilla baja */
static void px(int x, int y, Cor c) { if (x >= 0 && y >= 0 && x < g_lw && y < g_lh) lo[y * g_lw + x] = ((u32)CLAMP(c.r, 0, 255) << 16) | ((u32)CLAMP(c.g, 0, 255) << 8) | (u32)CLAMP(c.b, 0, 255); }
static void rect(int x, int y, int w, int h, Cor c) { for (int j = 0; j < h; ++j) for (int i = 0; i < w; ++i) px(x + i, y + j, c); }

static int cresta_lejana(int x) { return g_hy - g_lh * 6 / 100 - (tri(x + g_lw / 7, g_lw * 45 / 100) * 6 + tri(x * 2, g_lw * 23 / 100) * 3) * g_lh / (256 * 100) * 2; }
static int cresta_cercana(int x) { return g_hy - g_lh * 2 / 100 - (tri(x + g_lw / 3, g_lw * 60 / 100) * 5 + tri(x * 3, g_lw * 21 / 100)) * g_lh / (256 * 100) * 2; }
static int orilla(int x) { return g_lh * 80 / 100 + tri(x + g_lw / 5, g_lw * 70 / 100) * g_lh * 5 / (256 * 100); }

/* y más baja de la cresta cercana en [x0, x1]: los edificios se apoyan ahí y no flotan */
static int apoyo(int x0, int x1) {
	int m = 0;
	for (int x = x0; x <= x1; x += 2) m = MAX(m, cresta_cercana(x));
	return m + 2;
}

static void piramide(int cx, Cor c) {
	int bw = g_lh * 13 / 100, bh = g_lh * 16 / 100, base = apoyo(cx - bw, cx + bw);
	for (int i = 0; i < 6; ++i) rect(cx - bw * (6 - i) / 6, base - (i + 1) * bh / 6, bw * (6 - i) / 3, bh / 6 + 1, c);
}

static void templo(int cx, Cor c, Cor luz) {
	int u = MAX(1, g_lh / 60), w = 20 * u, base = apoyo(cx - w / 2 - 3 * u, cx + w / 2 + 3 * u);
	for (int i = 0; i < 3; ++i) rect(cx - w / 2 - (2 - i) * u, base - (i + 1) * u, w + (2 - i) * 2 * u, u + 1, c);
	int top = base - 3 * u;
	for (int k = 0; k < 6; ++k) rect(cx - w / 2 + k * (w - u) / 5, top - 9 * u, u, 9 * u + 1, c);
	rect(cx - w / 2 - u, top - 11 * u, w + 2 * u, 2 * u, c);
	for (int j = 0; j < 5 * u; ++j) { int hw = (w / 2 + u) * (5 * u - j) / (5 * u); rect(cx - hw, top - 11 * u - 1 - j, 2 * hw + 1, 1, j == 0 ? luz : c); }
}

static void palmera(int bx, int by, int alto, int incl, Cor c) {
	int grosor = MAX(2, alto / 22), tx = bx, ty = by - alto;
	int pasos = MAX(20, alto * 2);
	for (int k = 0; k <= pasos; ++k) {
		int t = k * 256 / pasos;
		int x = bx + incl * t / 256 * t / 256, y = by - alto * t / 256;
		int g = MAX(1, grosor - grosor * t / 400);
		rect(x - g / 2, y, g, 3, c);
		tx = x; ty = y;
	}
	int L = alto * 45 / 100;
	for (int f = -4; f <= 4; ++f) {
		if (!f && (bx & 1)) continue;
		int np = MAX(16, L * 2);
		for (int k = 0; k <= np; ++k) {
			int s = k * 256 / np, dir = f * 256 / 4;
			int x = tx + dir * L / 256 * s / 256 + f * 2 / 3;
			int y = ty - L * 30 / 100 * s / 256 + L * (s * s / 256) / 256 * (40 + 25 * (f < 0 ? -f : f)) / 100;
			rect(x, y, MAX(2, grosor - 1), MAX(2, grosor - 1), c);
			if (k > np / 6 && k < np - 2) rect(x, y + grosor - 1, 1, 2, c);
		}
	}
}

/* dibuja el paisaje en la rejilla baja */
static void pinta_paisaje(void) {
	static const Cor cielo[] = { { 28, 24, 78 }, { 92, 44, 112 }, { 214, 92, 82 }, { 250, 160, 90 }, { 255, 214, 130 } };
	static const Cor agua[] = { { 255, 190, 110 }, { 196, 100, 96 }, { 84, 56, 112 }, { 34, 34, 82 } };
	static const Cor suelo[] = { { 104, 66, 58 }, { 60, 38, 48 }, { 28, 20, 34 } };
	Cor lejana = { 142, 82, 116 }, cercana = { 84, 46, 88 }, sol = { 255, 238, 170 }, arena = { 214, 152, 96 }, espuma = { 255, 232, 196 };
	int hy = g_hy;
	for (int y = 0; y < g_lh; ++y) for (int x = 0; x < g_lw; ++x) {
		Cor c;
		if (y < hy) {
			int t = (y * 256 / hy) >> 4 << 4;   /* bandas: aspecto de pixel art */
			c = grad(cielo, 5, t);
			int dx = x - g_sx, dy = y - g_sy, d2 = dx * dx + dy * dy;
			if (d2 < g_sr * g_sr) c = sol;
			else if (d2 < g_sr * g_sr * 3) c = mezcla(c, (Cor){ 255, 190, 100 }, 120);
			else if (d2 < g_sr * g_sr * 9) c = mezcla(c, (Cor){ 255, 170, 90 }, 50);
			if (y >= cresta_lejana(x)) c = mezcla(lejana, cielo[4], 40 + (y - cresta_lejana(x)) * 3);
			if (y >= cresta_cercana(x)) c = mezcla(cercana, (Cor){ 40, 24, 60 }, (y - cresta_cercana(x)) * 8);
		} else if (y < orilla(x)) {
			int t = (y - hy) * 256 / MAX(1, g_lh * 80 / 100 - hy) >> 4 << 4;
			c = grad(agua, 4, t);
			int ancho = g_sr / 2 + (y - hy) * 2 / 3;
			if (abs(x - g_sx) < ancho && ((y & 3) < 2) && abs(x - g_sx) * 5 < ancho * (3 + (y * 7 + x) % 3)) c = mezcla(c, sol, 150);
		} else {
			int d = y - orilla(x);
			if (d < 1) c = espuma;
			else if (d < 4) c = arena;
			else c = grad(suelo, 3, (d * 256 / MAX(1, g_lh - orilla(x))) >> 4 << 4);
		}
		px(x, y, c);
	}
	Cor sil = { 52, 30, 66 }, luz = { 150, 78, 96 };
	piramide(g_lw * 22 / 100, sil);
	templo(g_lw * 83 / 100, sil, luz);
	Cor palma = { 20, 14, 30 };
	int by = g_lh * 94 / 100;
	palmera(g_lw * 8 / 100, by, g_lh * 38 / 100, g_lh * 6 / 100, palma);
	palmera(g_lw * 15 / 100, g_lh * 90 / 100, g_lh * 24 / 100, -(g_lh * 4 / 100), palma);
	palmera(g_lw * 93 / 100, by, g_lh * 36 / 100, -(g_lh * 7 / 100), palma);
	/* viñeta leve: oscurece las esquinas para que los botones del centro destaquen */
	int cx = g_lw / 2, cy = g_lh / 2;
	for (int y = 0; y < g_lh; ++y) for (int x = 0; x < g_lw; ++x) {
		int nx = (x - cx) * 256 / MAX(1, cx), ny = (y - cy) * 256 / MAX(1, cy);
		int oscuro = MAX(0, ((nx * nx + ny * ny) >> 8) - 140) * 100 / 372;
		u32 p = lo[y * g_lw + x];
		int r = (p >> 16 & 255) * (256 - oscuro) >> 8, g = (p >> 8 & 255) * (256 - oscuro) >> 8, b = (p & 255) * (256 - oscuro) >> 8;
		lo[y * g_lw + x] = (u32)(r << 16 | g << 8 | b);
	}
}

/* (re)genera el búfer si cambió el tamaño de la ventana */
static void prepara(int w, int h) {
	if (bg && bg_w == w && bg_h == h) return;
	free(bg); bg = NULL;
	int c = (h + 269) / 270;
	g_lw = (w + c - 1) / c; g_lh = (h + c - 1) / c;
	g_hy = g_lh * 60 / 100; g_sr = MAX(3, g_lh / 11);
	g_sx = g_lw * 68 / 100; g_sy = g_hy - g_sr * 3;
	lo = malloc((size_t)g_lw * g_lh * sizeof(u32));
	bg = malloc((size_t)w * h * sizeof(u32));
	if (!lo || !bg) { free(lo); free(bg); lo = NULL; bg = NULL; return; }
	pinta_paisaje();
	for (int y = 0; y < h; ++y) {
		const u32 *f = lo + (y / c) * g_lw;
		u32 *d = bg + (size_t)y * w;
		for (int x = 0; x < w; ++x) d[x] = 0xff000000u | f[x / c];
	}
	free(lo); lo = NULL;
	bg_w = w; bg_h = h; bg_c = c;
}

void fondo_propio(void) {
	int s = gfx_ui_suspend();   /* se escribe en coordenadas reales */
	int w = gfx_real_w(), h = gfx_real_h();
	prepara(w, h);
	if (bg && bg_w == w && bg_h == h) {
		for (int y = 0; y < h; ++y) memcpy(fb.px + (size_t)y * fb.pitch, bg + (size_t)y * w, (size_t)w * sizeof(u32));
		if (efx.perfil != FX_LENTO) {
			int c = bg_c, t = (int)(SDL_GetTicks() / 90);   /* nubes: una décima de fila lógica cada 90 ms */
			for (int i = 0; i < 5; ++i) {
				int cw = (14 + i * 7) * c, span = w + cw * 2;
				int x = ((i * 977 + t * (1 + i % 2)) * c / 2 + i * 313) % span - cw;
				int y = (g_lh * (8 + i * 7) / 100) * c;
				gfx_fill_a(x, y, cw, 2 * c, gfx_rgb(255, 200, 170), 70);
				gfx_fill_a(x + cw / 5, y - c, cw * 3 / 5, c, gfx_rgb(255, 200, 170), 70);
				gfx_fill_a(x + cw / 8, y + 2 * c, cw * 3 / 4, c, gfx_rgb(255, 200, 170), 40);
			}
			for (int i = 0; i < 9; ++i) {   /* destellos del sol sobre el agua */
				if ((t / 5 + i) % 3) continue;
				int gy = g_hy + 2 + i * g_lh * 2 / 100 + (i & 1);
				int gx = g_sx - g_sr / 2 + (i * 37 + t / 5 * 5) % (g_sr + i);
				gfx_fill_a(gx * c, gy * c, (2 + i % 3) * c, c, gfx_rgb(255, 236, 180), 150);
			}
		}
	}
	gfx_ui_resume(s);
}
