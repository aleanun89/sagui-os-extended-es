/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Terreno con las losetas del DRS original y las mejoras gráficas (todas graduables en Opciones, ver efectos.h):
 *  - transiciones suaves entre terrenos distintos (se mezcla la textura del vecino cerca del borde), con espuma en la costa;
 *  - agua con reflejos que se mueven (suma de dos ondas en coordenadas del mundo);
 *  - niebla de guerra con borde difuminado: la luz de cada esquina de casilla es el promedio de las cuatro vecinas y se
 *    interpola dentro de la casilla; las zonas explotadas uniformes se oscurecen con una pasada rápida por filas (SSE2).
 * Sin efectos activados dibuja exactamente lo de antes: una loseta por casilla. */
#include "ui.h"
#include "efectos.h"
#include "hilos.h"
#include "gpu.h"

Sprite *terrain_sprite(int kind, int variant);

static int8_t sin_lut[256];
static int lut_ok;

static void init_lut(void) {
	for (int i = 0; i < 256; ++i) sin_lut[i] = (int8_t)(sinf(i * 6.2831853f / 256.0f) * 127.0f);
	lut_ok = 1;
}

/* luz de una esquina de casilla (0..256): promedio de las cuatro casillas que la comparten; con alguna oculta, la mitad */
static int vertice(int i, int j) {
	int sum = 0, ocultas = 0;
	for (int k = 0; k < 4; ++k) {
		int x = i - 1 + (k & 1), y = j - 1 + (k >> 1);
		int f = (x < 0 || y < 0 || x >= W.w || y >= W.h) ? 0 : W.fog[y * W.w + x];
		if (!f) ++ocultas;
		sum += f == 2 ? 256 : f == 1 ? 128 : 0;
	}
	int l = sum / 4;
	return ocultas ? l / 2 : l;
}

static inline u32 mezclar(u32 a, u32 b, int t) {   /* t 0..256: de a hacia b */
	u32 it = (u32)(256 - t), ut = (u32)t;
	u32 rb = (((a & 0xff00ffu) * it + (b & 0xff00ffu) * ut) >> 8) & 0xff00ffu;
	u32 g = (((a & 0x00ff00u) * it + (b & 0x00ff00u) * ut) >> 8) & 0x00ff00u;
	return 0xff000000u | rb | g;
}

static inline u32 luz_px(u32 c, int l) {   /* l 0..256 */
	return 0xff000000u | ((((c & 0xff00ffu) * (u32)l) >> 8) & 0xff00ffu) | ((((c & 0x00ff00u) * (u32)l) >> 8) & 0x00ff00u);
}

static inline u32 sumar(u32 c, int d) {   /* suma d (con signo) a los tres canales */
	int r = (int)((c >> 16) & 255) + d, g = (int)((c >> 8) & 255) + d, b = (int)(c & 255) + d;
	return gfx_rgb(r, g, b);
}

typedef struct {
	int sx, sy, tw, th;
	int kind, x, y;
	int dir_kind[4];          /* tipo del vecino en cada dirección (-1: el mismo) */
	const Sprite *dir_spr[4];
	int use_trans, use_water, light_mode;   /* light_mode: 0 ninguna, 1 uniforme a la mitad, 2 interpolada */
	int L00, L10, L01, L11;
	float time;
	Recorte r;                /* recorte de esta franja (D393: cada hilo dibuja la suya) */
} TileCtx;

/* una pasada por los píxeles del rombo, con las coordenadas dentro de la casilla en 16.16 */
static void pasada_pixeles(const TileCtx *c, int sc, int left, int top) {
	int tw = c->tw, th = c->th;
	if (tw < 1 || th < 1) return;
	int k1 = 65536 / (2 * tw), k2 = 65536 / (2 * th);
	int inv = (int)(((long long)256 << 16) / sc);
	int t1 = (int)(c->time * 40.0f), t2 = (int)(c->time * 33.0f);
	for (int dy = -th; dy <= th; ++dy) {
		int yy = c->sy + dy;
		if (yy < c->r.y0 || yy >= c->r.y1) continue;
		int hw = tw * (th - (dy < 0 ? -dy : dy)) / th;
		int xa = MAX(c->sx - hw, c->r.x0), xb = MIN(c->sx + hw + 1, c->r.x1);
		if (xb <= xa) continue;
		u32 *row = fb.px + (size_t)yy * fb.pitch;
		int dx0 = xa - c->sx;
		int tx = dx0 * k1 + dy * k2 + 32768, ty = dx0 * k1 - dy * k2 + 32768;
		int srcy = (int)(((long long)(yy - top) * inv) >> 16);
		for (int xx = xa; xx < xb; ++xx, tx += k1, ty += k1) {
			u32 col = row[xx];
			int srcx = (int)(((long long)(xx - left) * inv) >> 16);
			if (c->use_trans) {
				for (int d = 0; d < 4; ++d) {
					if (c->dir_kind[d] < 0) continue;
					int p = d == 0 ? tx : d == 1 ? 65536 - tx : d == 2 ? ty : 65536 - ty;   /* 0..65536: cerca del borde de esa dirección */
					if (p <= 20000) continue;
					/* hasta la mitad en el borde: el vecino hace lo mismo desde su lado y los dos lados casan sin salto */
					int a = (int)(((long long)(p - 20000) * 128) / (65536 - 20000));
					if (a > 128) a = 128;
					a = a * a / 128 * 3 / 4 + a / 4;   /* curva suave: poco cerca del centro */
					const Sprite *ns = c->dir_spr[d];
					u32 nc = 0;
					if (ns && srcy >= 0 && srcy < ns->h) {
						/* el escalado de la loseta (257/256) deja algún píxel del borde sobre un hueco del vecino: se prueban los de al lado */
						static const int ds[5] = { 0, 1, -1, 2, -2 };
						for (int q = 0; q < 5 && !nc; ++q) {
							int sxq = srcx + ds[q];
							if (sxq < 0 || sxq >= ns->w) continue;
							u8 v = ns->px[srcy * ns->w + sxq];
							if (v) nc = fb.pal[v];
						}
					}
					if (!nc) continue;
					col = mezclar(col, nc, a);
					/* espuma: en el agua pegada a la tierra, una franja clara que late */
					if (c->kind >= TILE_WATER && c->dir_kind[d] < TILE_WATER && p > 56000) {
						int fo = (p - 56000) * 50 / 9536 + (int)(10 * sin_lut[((xx + yy) * 3 + (int)(c->time * 60)) & 255] / 127);   /* franja que late */
						col = mezclar(col, 0xffe8f4f8u, CLAMP(fo, 0, 64));
					}
				}
			}
			if (c->use_water) {
				int w = c->x * 256 + (tx >> 8), v = c->y * 256 + (ty >> 8);
				int ola = sin_lut[(((w * 3 + v * 2) >> 4) + t1) & 255] + sin_lut[(((w * 2 - v * 3) >> 4) - t2) & 255];   /* ±254 */
				col = sumar(col, ola * 14 / 254);
			}
			row[xx] = col;
		}
	}
}

static void pasada_luz(const TileCtx *c) {
	int tw = c->tw, th = c->th;
	if (tw < 1 || th < 1) return;
	int k1 = 65536 / (2 * tw), k2 = 65536 / (2 * th);
	for (int dy = -th; dy <= th; ++dy) {
		int yy = c->sy + dy;
		if (yy < c->r.y0 || yy >= c->r.y1) continue;
		int hw = tw * (th - (dy < 0 ? -dy : dy)) / th;
		int xa = MAX(c->sx - hw, c->r.x0), xb = MIN(c->sx + hw + 1, c->r.x1);
		if (xb <= xa) continue;
		u32 *row = fb.px + (size_t)yy * fb.pitch;
		if (c->light_mode == 1) { px_dim(row + xa, xb - xa, 1, 0x7f7f7f7fu); continue; }
		int dx0 = xa - c->sx;
		int tx = dx0 * k1 + dy * k2 + 32768, ty = dx0 * k1 - dy * k2 + 32768;
		for (int xx = xa; xx < xb; ++xx, tx += k1, ty += k1) {
			int tx8 = CLAMP(tx >> 8, 0, 256), ty8 = CLAMP(ty >> 8, 0, 256);
			int l0 = (c->L00 * (256 - tx8) + c->L10 * tx8) >> 8, l1 = (c->L01 * (256 - tx8) + c->L11 * tx8) >> 8;
			int l = (l0 * (256 - ty8) + l1 * ty8) >> 8;
			row[xx] = luz_px(row[xx], l);
		}
	}
}

/* una casilla visible: dónde cae en la pantalla y qué filas toca (sprite y rombo de los efectos) */
typedef struct { int x, y, sx, sy, f0, f1; } Visible;

/* lo que comparten las franjas: se prepara en el hilo principal (paletas, tablas, la lista de casillas visibles) y los
 * hilos solo lo leen */
typedef struct {
	int sc, tw, th, any_fx;
	float time;
	Recorte r;
	const u32 *pal_luz, *pal_niebla;
	const Visible *vis;
	int nvis;
} Franjas;

static Visible *vis_buf;
static int vis_cap;

/* las casillas que tocan las filas [r->y0, r->y1), en el mismo orden que siempre: el resultado es el mismo que de una vez */
static void terreno_franja(const Franjas *f, const Recorte *r, GfxTemp *tmp) {
	int sc = f->sc, tw = f->tw, th = f->th;
	for (int n = 0; n < f->nvis; ++n) {
		const Visible *v = &f->vis[n];
		if (v->f1 <= r->y0 || v->f0 >= r->y1) continue;   /* fuera de la franja: ni el sprite ni el rombo la tocan */
		int x = v->x, y = v->y, sx = v->sx, sy = v->sy;
		int i = y * W.w + x;
		u8 fg = W.fog[i];
		int kind = W.tile[i];
		int frame = kind >= TILE_WATER ? 0 : W.variant[i] % 9;
		Sprite *sp = terrain_sprite(kind, frame);
		gfx_sprite_franja(sp, sx - tw, sy - th, sc, 0, (fg == 2 || efx.niebla) ? f->pal_luz : f->pal_niebla, 0, r, tmp);
		if (!f->any_fx) continue;

		TileCtx c;
		memset(&c, 0, sizeof c);
		c.sx = sx; c.sy = sy; c.tw = tw; c.th = th; c.kind = kind; c.x = x; c.y = y; c.time = f->time; c.r = *r;
		if (efx.transiciones) {
			static const int ox[4] = { 1, -1, 0, 0 }, oy[4] = { 0, 0, 1, -1 };
			for (int d = 0; d < 4; ++d) {
				int nx = x + ox[d], ny = y + oy[d];
				c.dir_kind[d] = -1;
				if (nx < 0 || ny < 0 || nx >= W.w || ny >= W.h || !W.fog[ny * W.w + nx]) continue;
				int nk = W.tile[ny * W.w + nx];
				/* grass/arena entre sí se mezclan; agua y agua profunda también, pero más tenue */
				if (nk == kind) continue;
				c.dir_kind[d] = nk;
				c.dir_spr[d] = terrain_sprite(nk, nk >= TILE_WATER ? 0 : W.variant[ny * W.w + nx] % 9);
				c.use_trans = 1;
			}
		}
		c.use_water = efx.agua && kind >= TILE_WATER;
		if (efx.niebla) {
			c.L00 = vertice(x, y); c.L10 = vertice(x + 1, y); c.L01 = vertice(x, y + 1); c.L11 = vertice(x + 1, y + 1);
			if (c.L00 == 256 && c.L10 == 256 && c.L01 == 256 && c.L11 == 256) c.light_mode = 0;
			else if (c.L00 == 128 && c.L10 == 128 && c.L01 == 128 && c.L11 == 128) c.light_mode = 1;
			else c.light_mode = 2;
		}
		if (c.use_trans || c.use_water) pasada_pixeles(&c, sc, sx - tw, sy - th);
		if (c.light_mode) pasada_luz(&c);
	}
}

static void franja(void *ctx, int k, int partes) {
	const Franjas *f = (const Franjas *)ctx;
	int h = f->r.y1 - f->r.y0;
	Recorte r = f->r;
	r.y0 = f->r.y0 + h * k / partes; r.y1 = f->r.y0 + h * (k + 1) / partes;
	GfxTemp tmp;
	terreno_franja(f, &r, &tmp);
}

/* D393: la pantalla se parte en franjas horizontales que dibujan los hilos (hilos.c). Primero se apuntan, en el orden de
 * siempre, las casillas visibles y las filas que toca cada una; cada franja recorre esa lista y dibuja solo las que la
 * tocan, así que los solapes entre losetas vecinas quedan igual que con un hilo */
void render_terrain_tiles(int x0, int x1, int y0, int y1, float time) {
	if (!lut_ok) init_lut();
	Franjas f;
	f.time = time;
	f.sc = (int)(V.zoom * 256) + (V.zoom == 1.0f ? 0 : 1);   /* a zoom 1 la loseta se dibuja exacta (65x33): sin píxeles de borde perdidos */
	f.tw = (int)(32 * V.zoom); f.th = (int)(16 * V.zoom);
	f.any_fx = efx.transiciones || efx.agua || efx.niebla;
	f.r.x0 = fb.cx0; f.r.y0 = fb.cy0; f.r.x1 = fb.cx1; f.r.y1 = fb.cy1;
	if (f.r.y1 <= f.r.y0 || f.r.x1 <= f.r.x0) return;
	f.pal_luz = gfx_paleta(256); f.pal_niebla = gfx_paleta(130);
	/* los cuadros de las losetas se decodifican la primera vez que se piden: aquí, en el hilo principal, antes de repartir */
	for (int k = 0; k < 4; ++k) for (int v = 0; v < 9; ++v) terrain_sprite(k, v);

	int n = (x1 - x0 + 1) * (y1 - y0 + 1);
	if (n <= 0) return;
	if (vis_cap < n) { free(vis_buf); vis_cap = n + 1024; vis_buf = (Visible *)malloc(sizeof(Visible) * (size_t)vis_cap); if (!vis_buf) { vis_cap = 0; return; } }
	int tw = f.tw, th = f.th, nv = 0;
	for (int y = y0; y <= y1; ++y) {
		for (int x = x0; x <= x1; ++x) {
			int i = y * W.w + x;
			if (!W.fog[i]) continue;
			int sx, sy;
			view_to_screen(x + 0.5f, y + 0.5f, &sx, &sy);
			if (sx < -tw * 2 || sx > V.sw + tw * 2 || sy < -th * 2 || sy > V.sh + th * 2) continue;
			int kind = W.tile[i];
			const Sprite *sp = terrain_sprite(kind, kind >= TILE_WATER ? 0 : W.variant[i] % 9);
			int a = sy - th, b = sy + th + 1;
			if (sp && sp->px) {
				int dy0 = sy - th - ((sp->hy * f.sc) >> 8), dh = (sp->h * f.sc + 255) >> 8;
				a = MIN(a, dy0); b = MAX(b, dy0 + dh);
			}
			if (b <= f.r.y0 || a >= f.r.y1) continue;
			Visible *v = &vis_buf[nv++];
			v->x = x; v->y = y; v->sx = sx; v->sy = sy; v->f0 = a; v->f1 = b;
		}
	}
	f.vis = vis_buf; f.nvis = nv;
	hilos_repartir(franja, &f, hilos_partes());
}

/* ---------- D436: las losetas por la GPU ----------
 * Cada casilla visible es un rombo con su loseta del atlas (los vértices, en coma flotante desde la vista: rombos vecinos
 * comparten vértices y no quedan rendijas) y la luz de la niebla en las esquinas: con «Niebla suave», la de cada esquina
 * (vertice(), como la CPU); sin ella, toda la casilla a la luz de su paleta (130/256 en lo explorado). Las transiciones y el
 * agua animada de la CPU no están (D436: diferencias conocidas). */
static inline void pantalla_f(float wx, float wy, float *x, float *y) {
	*x = V.sw * 0.5f + ((wx + wy) * 32.0f - V.cx) * V.zoom + 0.5f;
	*y = V.sh * 0.5f + ((wx - wy) * 16.0f - V.cy) * V.zoom + 0.5f;
}

void render_terrain_tiles_gpu(int x0, int x1, int y0, int y1) {
	int sc = (int)(V.zoom * 256) + (V.zoom == 1.0f ? 0 : 1), tw = (int)(32 * V.zoom), th = (int)(16 * V.zoom);
	for (int k = 0; k < 4; ++k) for (int v = 0; v < 9; ++v) gpu_loseta_preparar(terrain_sprite(k, v));
	for (int y = y0; y <= y1; ++y) {
		for (int x = x0; x <= x1; ++x) {
			int i = y * W.w + x;
			u8 fg = W.fog[i];
			if (!fg) continue;
			int sx, sy;
			view_to_screen(x + 0.5f, y + 0.5f, &sx, &sy);
			if (sx < -tw * 2 || sx > V.sw + tw * 2 || sy < -th * 2 || sy > V.sh + th * 2) continue;
			int kind = W.tile[i];
			const Sprite *sp = terrain_sprite(kind, kind >= TILE_WATER ? 0 : W.variant[i] % 9);
			if (!sp || !sp->px) continue;
			int ox = sx - tw - ((sp->hx * sc) >> 8), oy = sy - th - ((sp->hy * sc) >> 8);
			float px[4], py[4];
			pantalla_f((float)x, (float)y, &px[0], &py[0]);           /* izquierda */
			pantalla_f((float)x + 1, (float)y, &px[1], &py[1]);       /* abajo */
			pantalla_f((float)x, (float)y + 1, &px[2], &py[2]);       /* arriba */
			pantalla_f((float)x + 1, (float)y + 1, &px[3], &py[3]);   /* derecha */
			int l[4];
			if (efx.niebla) { l[0] = vertice(x, y); l[1] = vertice(x + 1, y); l[2] = vertice(x, y + 1); l[3] = vertice(x + 1, y + 1); }
			else l[0] = l[1] = l[2] = l[3] = fg == 2 ? 256 : 130;
			gpu_loseta(sp, ox, oy, sc, px, py, l);
		}
	}
}
