/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Carga de recursos y dibujo del mundo (terreno isométrico, entidades, niebla, minimapa). */
#include "ui.h"
#include "efectos.h"

void render_terrain_tiles(int x0, int x1, int y0, int y1, float time);

View V = { 0, 0, 1.0f, 800, 600 };
u32 col_player[MAX_PLAYERS];

static Drs d_graphics, d_interfac, d_terrain;
static Slp *trn[4];
Sprite *terrain_sprite(int kind, int variant) { return slp_frame(trn[kind], variant); }
static AnimSet set_vill, set_wood, set_miner, set_forager;
#define NUM_UNIT_ART 29
static AnimSet unit_art[NUM_UNIT_ART];
/* gráficos de estar quieto, andar, atacar, morir y descomposición de cada unidad, en el orden de los tipos de unidad */
static const u32 unit_art_ids[NUM_UNIT_ART][5] = {
	{ 418, 657, 224, 314, 373 }, /* aldeano */
	{ 425, 664, 212, 321, 380 }, /* garrotero */
	{ 415, 654, 205, 311, 370 }, /* hachero */
	{ 437, 678, 226, 335, 371 }, /* espadachín corto */
	{ 413, 652, 203, 309, 368 }, /* arquero */
	{ 414, 653, 204, 309, 368 }, /* arquero mejorado */
	{ 412, 665, 202, 318, 377 }, /* arquero en carro */
	{ 422, 661, 209, 318, 377 }, /* explorador */
	{ 424, 663, 211, 320, 379 }, /* caballería */
	{ 423, 662, 210, 319, 378 }, /* caballería pesada */
	{ 416, 655, 206, 312, 371 }, /* hoplita */
	{ 443, 685, 634, 341, 402 }, /* sacerdote */
	{ 656, 656, 207, 316, 316 }, /* balista */
	{ 660, 660, 208, 316, 316 }, /* catapulta */
	{ 659, 659, 208, 316, 316 }, /* catapulta pesada */
	{ 414, 653, 204, 309, 368 }, /* lanzador de honda (provisional: gráfico del arquero) */
	{ 429, 668, 216, 386, 386 }, /* elefante de guerra */
	{ 427, 666, 228, 386, 386 }, /* arquero de elefante */
	{ 647, 647, 647, 647, 647 }, /* barco de pesca */
	{ 640, 640, 640, 640, 640 }, /* barco de transporte */
	{ 693, 693, 693, 693, 693 }, /* trirreme */
	{ 438, 677, 221, 312, 371 }, /* falange */
	{ 436, 676, 219, 335, 371 }, /* centurión */
	{ 423, 662, 210, 319, 378 }, /* catafracta (gráfico de la caballería pesada) */
	{ 424, 663, 211, 320, 379 }, /* arquero a caballo (gráfico de la caballería) */
	{ 648, 648, 648, 648, 648 }, /* barco mercante (648: el barco de carga del original) */
	{ 412, 665, 412, 318, 377 }, /* carro mercante (provisional: gráfico del carro arquero) */
	{ 475, 696, 475, 386, 385 }, /* caballo salvaje: 475 quieto, 696 galopando */
	{ 428, 667, 215, 386, 386 }, /* elefante salvaje (386 = cayendo) */
};
static Slp *s_tc, *s_tc_flag, *s_barracks, *s_house, *s_granary, *s_storage, *s_farm;
static Slp *s_archery, *s_stable, *s_temple, *s_academy, *s_government, *s_tower;
static Slp *s_berries, *s_gold, *s_stone, *s_tree_d[4], *s_tree_g[4], *s_tree_dead;
static Slp *s_icons_unit, *s_icons_bld, *s_fire[3];
static Slp *s_bkg[2];
static u32 pal_game[256], pal_bkg[2][256];
static int have_bkg[2];

/* los gráficos de los cuatro estilos de edificio se cargan cuando hacen falta por primera vez */
static Slp *style_cache[260];
static Slp *style_slp_id(int id) {
	if (id < 0 || id >= 260) return NULL;
	if (!style_cache[id]) style_cache[id] = slp_load(&d_graphics, (u32)id);
	return style_cache[id];
}

static int open_drs(Drs *d, const char *dir, const char *name) {
	char p[640];
	const char *fmt[] = { "%s/%s", "%s/DATA/%s", "%s/data/%s", "%s/GAME/DATA/%s" };
	for (int i = 0; i < 4; ++i) {
		snprintf(p, sizeof p, fmt[i], dir, name);
		if (drs_open(d, p)) return 1;
		/* variante en minúsculas */
		char low[64];
		size_t n = 0;
		for (; name[n] && n < 63; ++n) low[n] = (char)((name[n] >= 'A' && name[n] <= 'Z') ? name[n] + 32 : name[n]);
		low[n] = 0;
		snprintf(p, sizeof p, fmt[i], dir, low);
		if (drs_open(d, p)) return 1;
	}
	return 0;
}

static void load_set(AnimSet *a, u32 stand, u32 move, u32 attack, u32 die, u32 decay) {
	a->stand = slp_load(&d_graphics, stand);
	a->move = slp_load(&d_graphics, move);
	a->attack = slp_load(&d_graphics, attack);
	a->die = slp_load(&d_graphics, die);
	a->decay = slp_load(&d_graphics, decay);
}

static int load_bkg(int which, u32 bkg_id) {
	int sz = 0;
	char *txt = (char *)drs_read(&d_interfac, bkg_id, &sz);
	if (!txt) return 0;
	txt = (char *)realloc(txt, (size_t)sz + 1);
	txt[sz] = 0;

	unsigned img = 0, pal = 0;
	char name[32];
	const char *p = strstr(txt, "background1_files");
	int ok = p && sscanf(p, "background1_files %31s none %u", name, &img) == 2;
	p = strstr(txt, "palette_file");
	ok = ok && p && sscanf(p, "palette_file %31s %u", name, &pal) == 2;
	free(txt);
	if (!ok) return 0;

	s_bkg[which] = slp_load(&d_interfac, img);
	have_bkg[which] = pal_load(&d_interfac, pal, pal_bkg[which]);
	return s_bkg[which] != NULL && have_bkg[which];
}

int assets_load(const char *dir) {
	if (!open_drs(&d_graphics, dir, "GRAPHICS.DRS") || !open_drs(&d_interfac, dir, "INTERFAC.DRS") || !open_drs(&d_terrain, dir, "TERRAIN.DRS"))
		return 0;

	u32 rgb[256];
	if (!pal_load(&d_interfac, 50500, rgb)) return 0;
	memcpy(pal_game, rgb, sizeof rgb);

	for (int i = 0; i < 4; ++i) trn[i] = slp_load(&d_terrain, 15000 + i);

	load_set(&set_vill, 418, 657, 224, 314, 373);
	load_set(&set_wood, 440, 682, 625, 338, 399);
	load_set(&set_miner, 441, 683, 633, 339, 400);
	load_set(&set_forager, 418, 657, 632, 314, 373);
	for (int i = 0; i < NUM_UNIT_ART; ++i)
		load_set(&unit_art[i], unit_art_ids[i][0], unit_art_ids[i][1], unit_art_ids[i][2], unit_art_ids[i][3], unit_art_ids[i][4]);

	s_tc = slp_load(&d_graphics, 280); s_tc_flag = slp_load(&d_graphics, 230);
	s_barracks = slp_load(&d_graphics, 254);
	s_house = slp_load(&d_graphics, 165);
	s_granary = slp_load(&d_graphics, 164);
	s_storage = slp_load(&d_graphics, 166);
	s_farm = slp_load(&d_graphics, 354);
	s_archery = slp_load(&d_graphics, 27); s_stable = slp_load(&d_graphics, 26); s_temple = slp_load(&d_graphics, 29);
	s_academy = slp_load(&d_graphics, 17); s_government = slp_load(&d_graphics, 19); s_tower = slp_load(&d_graphics, 31);
	s_berries = slp_load(&d_graphics, 240);
	s_gold = slp_load(&d_graphics, 481);
	s_stone = slp_load(&d_graphics, 622);
	for (int i = 0; i < 4; ++i) { s_tree_d[i] = slp_load(&d_graphics, 463 + i); s_tree_g[i] = slp_load(&d_graphics, 492 + i); }
	s_tree_dead = slp_load(&d_graphics, 636);

	for (int i = 0; i < 3; ++i) s_fire[i] = slp_load(&d_graphics, 2 + i);
	s_icons_unit = slp_load(&d_interfac, 50730);
	s_icons_bld = slp_load(&d_interfac, 50704);

	load_bkg(0, 50051);
	load_bkg(1, 50052);

	col_player[0] = gfx_rgb(40, 90, 255); col_player[1] = gfx_rgb(230, 40, 40);
	col_player[2] = gfx_rgb(40, 190, 60); col_player[3] = gfx_rgb(240, 230, 40);
	col_player[4] = gfx_rgb(255, 140, 30); col_player[5] = gfx_rgb(150, 100, 45);
	col_player[6] = gfx_rgb(190, 190, 190); col_player[7] = gfx_rgb(70, 190, 190);

	for (int i = 0; i < 256; ++i) fb.pal[i] = 0xff000000u | pal_game[i];
	gfx_pal_changed();
	return 1;
}

void assets_free(void) {
	drs_close(&d_graphics); drs_close(&d_interfac); drs_close(&d_terrain);
}

/* gráficos de la interfaz (INTERFAC.DRS) que se cargan al pedirlos: iconos de órdenes, recursos y mejoras, paneles y banderas */
Slp *iface_slp(u32 id) {
	static struct { u32 id; Slp *s; int tried; } c[48];
	for (int i = 0; i < ARRAY_LEN(c); ++i) if (c[i].tried && c[i].id == id) return c[i].s;
	for (int i = 0; i < ARRAY_LEN(c); ++i)
		if (!c[i].tried) { c[i].tried = 1; c[i].id = id; c[i].s = d_interfac.f ? slp_load(&d_interfac, id) : NULL; return c[i].s; }
	return NULL;
}

/* un cuadro de la interfaz, solo si tiene un dibujo de verdad (los DRS sintéticos o recortados dan cuadros vacíos o diminutos) */
Sprite *iface_frame(u32 id, int frame, int min_w) {
	Slp *s = iface_slp(id);
	if (!s || frame < 0 || frame >= s->nframes) return NULL;
	Sprite *f = slp_frame(s, frame);
	return f && f->px && f->w >= min_w && f->h >= 3 ? f : NULL;
}

Sprite *icon_unit(int idx) { return slp_frame(s_icons_unit, idx); }
Sprite *icon_bld(int idx) { return slp_frame(s_icons_bld, idx); }

void draw_background(int which) {
	Sprite *s = (s_bkg[which] && have_bkg[which]) ? slp_frame(s_bkg[which], 0) : NULL;
	if (!s) { fondo_propio(); return; }   /* sin arte original: paisaje propio (fondo.c) */
	gfx_fill(0, 0, fb.w, fb.h, gfx_rgb(10, 8, 6));

	u32 save[256];
	memcpy(save, fb.pal, sizeof save);
	for (int i = 0; i < 256; ++i) fb.pal[i] = 0xff000000u | pal_bkg[which][i];
	gfx_pal_changed();

	/* escala para cubrir la ventana */
	int sc = MAX(fb.w * 256 / s->w, fb.h * 256 / s->h);
	int w = s->w * sc / 256, h = s->h * sc / 256;
	gfx_sprite(s, (fb.w - w) / 2 + s->hx * sc / 256, (fb.h - h) / 2 + s->hy * sc / 256, sc, 0, 256, 0);

	memcpy(fb.pal, save, sizeof save);
	gfx_pal_changed();
}

/* ---------- proyección ---------- */

void view_proj(float wx, float wy, float *px, float *py) { *px = (wx + wy) * 32.0f; *py = (wx - wy) * 16.0f; }

void view_to_screen(float wx, float wy, int *sx, int *sy) {
	float px, py;
	view_proj(wx, wy, &px, &py);
	*sx = (int)floorf(V.sw * 0.5f + (px - V.cx) * V.zoom);
	*sy = (int)floorf(V.sh * 0.5f + (py - V.cy) * V.zoom);
}

void screen_to_world(int sx, int sy, float *wx, float *wy) {
	float px = (sx - V.sw * 0.5f) / V.zoom + V.cx, py = (sy - V.sh * 0.5f) / V.zoom + V.cy;
	float a = px / 32.0f, b = py / 16.0f; /* a = x+y, b = x-y */
	*wx = (a + b) * 0.5f;
	*wy = (a - b) * 0.5f;
}

void view_center_on(float wx, float wy) { view_proj(wx, wy, &V.cx, &V.cy); }

void view_clamp(void) {
	float minx, miny, maxx, maxy, p;
	view_proj(0, 0, &minx, &miny);
	view_proj((float)W.w, (float)W.h, &maxx, &p);
	view_proj((float)W.w, 0, &p, &maxy);
	float tx, ty;
	view_proj(0, (float)W.h, &tx, &ty);
	V.cx = CLAMP(V.cx, minx, maxx);
	V.cy = CLAMP(V.cy, ty - 0, maxy);
}

/* ---------- dibujo del mundo ---------- */

static int anim_frames(Slp *s) { return s ? MAX(1, s->nframes / 5) : 1; }

/* devuelve el índice de cuadro para una animación de 8 direcciones con 5 orientaciones guardadas */
static int dir_frame(Slp *s, float anim, float angle, int *flip) {
	static const int faces[8] = { 1, 2, 3, 4, 3, 2, 1, 0 };
	float a = fmodf(angle + 0.3926991f + 6.2831853f * 4, 6.2831853f);
	int uface = (int)(a / 0.7853982f) % 8;
	int n = anim_frames(s);
	*flip = uface < 3;
	return ((int)anim % n) + faces[uface] * n;
}

typedef struct { int idx; float depth; } DrawItem;

static int cmp_depth(const void *a, const void *b) {
	float x = ((const DrawItem *)a)->depth, y = ((const DrawItem *)b)->depth;
	return x < y ? -1 : x > y;
}

static const AnimSet *unit_set(const Entity *e) {
	if (e->type != E_VILLAGER) return &unit_art[e->type - E_VILLAGER];
	if (e->state == ST_GATHER || e->state == ST_RETURN || e->carry > 0) {
		switch (e->carry_kind) {
		case RES_WOOD: return &set_wood;
		case RES_GOLD: case RES_STONE: return &set_miner;
		case RES_FOOD: return &set_forager;
		}
	}
	return &set_vill;
}

static void draw_hpbar(int sx, int sy, float frac, int w) {
	/* barra de vida: marco oscuro, relleno con degradado y un brillo arriba */
	gfx_fill(sx - w / 2 - 1, sy - 1, w + 2, 6, gfx_rgb(10, 8, 6));
	gfx_fill(sx - w / 2, sy, w, 4, gfx_rgb(52, 18, 16));
	int fw = (int)(w * CLAMP(frac, 0, 1));
	u32 c0 = frac > 0.5f ? gfx_rgb(110, 235, 90) : frac > 0.25f ? gfx_rgb(250, 225, 70) : gfx_rgb(250, 90, 70);
	u32 c1 = frac > 0.5f ? gfx_rgb(40, 150, 40) : frac > 0.25f ? gfx_rgb(180, 140, 20) : gfx_rgb(160, 30, 24);
	if (fw > 0) gfx_fill_grad(sx - w / 2, sy, fw, 4, c0, c1);
}

/* efectos de edificios (D382): g_sink > 0 es un derrumbe (0..1); la construcción usa e->progress */
static float g_sink;

/* sombras: elipses que oscurecen lo de debajo; se dibujan todas antes que los sprites para no tapar a los vecinos */
static void draw_shadow(const Entity *e) {
	int sx, sy;
	view_to_screen(e->x, e->y, &sx, &sy);
	float z = V.zoom;
	if (is_unit(e->type)) {
		if (e->state == ST_DYING) return;
		gfx_shadow_ellipse(sx, sy - (int)(1 * z), (int)(10 * z), MAX(2, (int)(4 * z)));
	} else if (is_building(e->type)) {
		gfx_shadow_ellipse(sx + (int)(e->size * 7 * z), sy + (int)(e->size * 4 * z), (int)(e->size * 15 * z), (int)(e->size * 6 * z));
	} else if (e->type == E_TREE) {
		gfx_shadow_ellipse(sx + (int)(4 * z), sy, (int)(12 * z), MAX(2, (int)(5 * z)));
	} else if (e->type == E_BERRIES || e->type == E_GOLD || e->type == E_STONE) {
		gfx_shadow_ellipse(sx + (int)(2 * z), sy, (int)(9 * z), MAX(2, (int)(4 * z)));
	}
}

static void draw_entity(const Entity *e, int shade) {
	int sx, sy, sc = (int)(V.zoom * 256), flip = 0;
	view_to_screen(e->x, e->y, &sx, &sy);

	if (is_unit(e->type)) {
		const AnimSet *as = unit_set(e);
		Slp *s = as->stand;
		float anim = W.time * 3.0f + e->variant * 2.0f; /* animación lenta de reposo */

		if (e->state == ST_DYING) {
			int n = anim_frames(as->die), dn = anim_frames(as->decay);
			float dur = n / 10.0f;
			if (e->die_t < dur) { s = as->die; anim = MIN((float)n - 0.01f, e->die_t * 10.0f); }
			else { s = as->decay; anim = MIN((float)dn - 0.01f, (e->die_t - dur) * dn / 12.0f); }
		} else if (e->walking) {
			s = as->move; anim = e->anim;
		} else if (e->acting) {
			s = as->attack; anim = e->phase * anim_frames(as->attack);
		}

		int f = dir_frame(s, anim, e->angle, &flip);
		int owner = e->owner >= 0 ? pcolor(e->owner) : 0;   /* color elegido en la configuración de la partida */
		if (e->sel) {
			int rx = (int)(14 * V.zoom), ry = (int)(7 * V.zoom);
			gfx_ellipse(sx, sy, rx, MAX(1, ry), gfx_rgb(90, 255, 90), 38);
			for (int a = 0; a < 40; ++a) {
				float t = a * 6.2831853f / 40;
				int px = sx + (int)(cosf(t) * rx), py = sy + (int)(sinf(t) * ry);
				gfx_fill(px, py, 2, 1, gfx_rgb(90, 255, 90));
			}
		}
		gfx_sprite(slp_frame(s, f), sx, sy, sc, owner, shade, flip);
		if (e->sel || e->hp < e->maxhp) {
			if (e->state != ST_DYING) draw_hpbar(sx, sy - (int)(42 * V.zoom), e->hp / e->maxhp, (int)(26 * V.zoom));
		}
		return;
	}

	if (e->type == E_CARCASS && e->variant == E_HORSE) {
		/* no se encontró ningún sprite de caballo muerto: se dibuja una carcasa sencilla con unas pocas formas */
		int rx = (int)(15 * V.zoom), ry = (int)(7 * V.zoom);
		u32 body = shade >= 256 ? gfx_rgb(118, 112, 104) : gfx_rgb(70, 66, 62), blood = shade >= 256 ? gfx_rgb(150, 36, 30) : gfx_rgb(90, 24, 22);
		for (int y = -ry; y <= ry; ++y) {
			int w = (int)(rx * sqrtf(1.0f - (float)(y * y) / (float)(ry * ry + 1)));
			gfx_fill(sx - w, sy + y, 2 * w, 1, body);
		}
		gfx_fill(sx - rx - (int)(5 * V.zoom), sy - (int)(2 * V.zoom), (int)(7 * V.zoom), (int)(4 * V.zoom), body);
		gfx_fill(sx - rx / 3, sy - ry / 2, rx / 2, ry / 2, blood);
		for (int k = 0; k < 4; ++k) gfx_fill(sx - rx + k * rx * 2 / 3, sy + ry - 1, MAX(1, (int)(2 * V.zoom)), (int)(4 * V.zoom), body);
		return;
	}

	if (e->type == E_CARCASS) {
		const AnimSet *as = &unit_art[e->variant - E_VILLAGER];
		int n = anim_frames(as->die);
		int f = dir_frame(as->die, (float)n - 0.01f, e->angle, &flip);
		gfx_sprite(slp_frame(as->die, f), sx, sy, sc, 0, shade, flip);
		return;
	}

	if (e->type == E_RELIC) {
		/* una urna dorada con un brillo suave */
		float t = (float)SDL_GetTicks() / 1000.0f;
		int rr = (int)((12 + 2 * sinf(t * 3.0f)) * V.zoom);
		gfx_fill_a(sx - rr, sy - rr / 2 - (int)(10 * V.zoom), rr * 2, rr, gfx_rgb(255, 220, 90), shade >= 256 ? 40 : 18);
		int w = MAX(4, (int)(9 * V.zoom)), h = MAX(6, (int)(13 * V.zoom));
		u32 gold = shade >= 256 ? gfx_rgb(240, 200, 60) : gfx_rgb(130, 108, 40), dark = shade >= 256 ? gfx_rgb(160, 120, 30) : gfx_rgb(80, 60, 20);
		gfx_fill(sx - w / 2, sy - h, w, h, gold);
		gfx_fill(sx - w / 2 - 1, sy - h, w + 2, MAX(2, h / 5), dark);
		gfx_fill(sx - w / 3, sy - h - MAX(2, h / 4), w * 2 / 3, MAX(2, h / 4), gold);
		gfx_fill(sx - w / 3, sy - h / 2, 1, h / 3, dark);
		return;
	}

	if (e->type == E_FISH) {
		/* banco de peces dibujado con unos pocos trazos: ondas y tres pececillos */
		float t = (float)SDL_GetTicks() / 1000.0f;
		int rx = (int)(18 * V.zoom), ry = (int)(9 * V.zoom);
		u32 rip = shade >= 256 ? gfx_rgb(190, 225, 240) : gfx_rgb(110, 130, 140);
		for (int k = 0; k < 2; ++k) {
			float ph = fmodf(t * 0.6f + k * 0.5f, 1.0f);
			int a = (int)(rx * (0.4f + ph * 0.6f)), b = (int)(ry * (0.4f + ph * 0.6f));
			for (int d = 0; d < 28; ++d) {
				float ang = d * 6.2831853f / 28;
				gfx_fill(sx + (int)(cosf(ang) * a), sy + (int)(sinf(ang) * b), 1, 1, rip);
			}
		}
		for (int k = 0; k < 3; ++k) {
			float ang = t * 0.8f + k * 2.1f;
			int fx = sx + (int)(cosf(ang) * 9 * V.zoom), fy = sy + (int)(sinf(ang) * 4.5f * V.zoom);
			int len = MAX(3, (int)(5 * V.zoom));
			u32 fc = shade >= 256 ? gfx_rgb(220, 235, 245) : gfx_rgb(130, 140, 150);
			gfx_fill(fx - len / 2, fy, len, MAX(1, len / 3), fc);
			gfx_fill(fx + (cosf(ang + 1.57f) > 0 ? len / 2 : -len / 2 - 1), fy - 1, 1, MAX(2, len / 3 + 1), fc);
		}
		return;
	}

	if (is_resource(e->type)) {
		Slp *s = s_tree_d[e->variant & 3];
		if (e->type == E_BERRIES) s = s_berries;
		else if (e->type == E_GOLD) s = s_gold;
		else if (e->type == E_STONE) s = s_stone;
		else if (e->amount < 30) s = s_tree_dead;
		else if (W.tile[(int)e->by * W.w + (int)e->bx] == TILE_GRASS) s = s_tree_g[e->variant & 3];
		int f = (e->type == E_GOLD || e->type == E_STONE) ? e->variant : 0;
		gfx_sprite(slp_frame(s, f), sx, sy, sc, 0, shade, 0);
		return;
	}

	/* edificios: anclaje en el centro de la huella. Desde la Edad de Herramientas, el aspecto sigue el estilo de la civilización */
	Slp *s = NULL, *flag = NULL;
	int f = 0, style = e->owner >= 0 ? civ_def[W.pl[e->owner].civ].style : 0, age = e->owner >= 0 ? W.pl[e->owner].age : 0;
	static const int style_start[4] = { 15, 59, 103, 159 };
	static const int wonder_ids[4] = { 30, 77, 121, 177 };
	int rel = -1;
	switch (e->type) {
	case E_TOWN_CENTER: if (age == 0) { s = s_tc; flag = s_tc_flag; } else rel = 3; break;
	case E_BARRACKS: if (age == 0) s = s_barracks; else rel = 0; break;
	case E_HOUSE: if (age == 0) s = s_house; else rel = 6; break;
	case E_GRANARY: if (age == 0) s = s_granary; else rel = 5; break;
	case E_STORAGE: rel = 13; break;
	case E_FARM: s = s_farm; break;
	case E_ARCHERY: rel = 12; break;
	case E_STABLE: rel = 11; break;
	case E_TEMPLE: rel = 14; break;
	case E_ACADEMY: rel = 2; break;
	case E_GOVERNMENT: rel = 1; break;
	case E_TOWER: rel = 16; break;
	case E_MARKET: rel = 9; break;
	case E_SIEGE: rel = 4; break;
	case E_WALL: rel = 10; break;
	case E_DOCK: rel = 7; break;
	case E_WONDER: s = style_slp_id(wonder_ids[style]); break;
	default: s = s_storage; break;
	}
	if (rel >= 0) {
		s = style_slp_id(style_start[style] + rel);
		/* los centros urbanos y las casas tienen una imagen para cada edad */
		if (s && s->nframes >= 3 && (e->type == E_TOWN_CENTER || e->type == E_HOUSE)) f = MIN(MAX(age - 1, 0), s->nframes - 1);
	}
	if (!s) s = s_storage;

	int sh = e->progress < 1 ? MIN(shade, 150) : shade;
	if (e->type == E_TOWN_CENTER && age == 0) f = e->hp < e->maxhp * 0.5f ? 1 : 0;
	if (e->sel) {
		int cx[4], cy[4];
		view_to_screen((float)e->bx, (float)e->by, &cx[0], &cy[0]);
		view_to_screen((float)(e->bx + e->size), (float)e->by, &cx[1], &cy[1]);
		view_to_screen((float)(e->bx + e->size), (float)(e->by + e->size), &cx[2], &cy[2]);
		view_to_screen((float)e->bx, (float)(e->by + e->size), &cx[3], &cy[3]);
		for (int k = 0; k < 4; ++k) gfx_line(cx[k], cy[k], cx[(k + 1) % 4], cy[(k + 1) % 4], gfx_rgb(90, 255, 90));
	}
	{
		Sprite *sp = slp_frame(s, f);
		int own = e->owner >= 0 ? pcolor(e->owner) : 0;
		int obra = efx.construccion && e->progress < 1 && sp, ruina = efx.construccion && g_sink > 0 && sp;
		if (obra || ruina) {
			/* obra: el edificio sube desde el suelo con andamios; ruina: se hunde en el suelo */
			int top = sy - (int)(sp->hy * V.zoom), hh = (int)(sp->h * V.zoom), bottom = top + hh, shift = 0, cut0 = 0, cut1 = fb.h;
			if (obra) cut0 = bottom - (int)(hh * (0.2f + 0.8f * e->progress));
			if (ruina) { shift = (int)(g_sink * hh * 0.9f); cut1 = bottom + (int)(4 * V.zoom); }
			gfx_clip(0, MAX(0, cut0), fb.w, cut1);
			gfx_sprite(sp, sx, sy + shift, sc, own, sh, 0);
			if (flag && !ruina) gfx_sprite(slp_frame(flag, f), sx, sy, sc, own, sh, 0);
			gfx_clip_reset();
			if (obra) {
				/* andamios: cuatro postes en las esquinas de la base y travesaños */
				int cx[4], cy[4];
				view_to_screen((float)e->bx, (float)e->by, &cx[0], &cy[0]);
				view_to_screen((float)(e->bx + e->size), (float)e->by, &cx[1], &cy[1]);
				view_to_screen((float)(e->bx + e->size), (float)(e->by + e->size), &cx[2], &cy[2]);
				view_to_screen((float)e->bx, (float)(e->by + e->size), &cx[3], &cy[3]);
				int alto = (int)(hh * (0.35f + 0.45f * e->progress));
				u32 madera = gfx_rgb(150, 108, 58), sombra = gfx_rgb(96, 66, 34);
				for (int k = 0; k < 4; ++k) {
					gfx_line(cx[k], cy[k], cx[k], cy[k] - alto, madera);
					gfx_line(cx[k] + 1, cy[k], cx[k] + 1, cy[k] - alto, sombra);
					gfx_line(cx[k], cy[k] - alto, cx[(k + 1) % 4], cy[(k + 1) % 4] - alto, madera);
					gfx_line(cx[k], cy[k] - alto / 2, cx[(k + 1) % 4], cy[(k + 1) % 4] - alto / 2, sombra);
				}
			}
		} else {
			gfx_sprite(sp, sx, sy, sc, own, sh, 0);
			if (flag) gfx_sprite(slp_frame(flag, f), sx, sy, sc, own, sh, 0);
		}
	}

	/* llamas en los edificios muy dañados */
	if (e->progress >= 1 && e->hp < e->maxhp * 0.66f) {
		int nf = e->hp < e->maxhp * 0.33f ? 3 : 1;
		for (int k = 0; k < nf; ++k) {
			Slp *fs = s_fire[k % 3];
			int ox = (int)((k - 1) * e->size * 12 * V.zoom), oy = (int)((k % 2 ? -6 : 8) * V.zoom);
			if (fs) gfx_sprite(slp_frame(fs, (int)(W.time * 10 + k * 7)), sx + ox, sy + oy, sc, 0, shade, 0);
		}
	}

	if (e->progress < 1) {
		int w = (int)(60 * V.zoom);
		gfx_fill(sx - w / 2, sy + (int)(10 * V.zoom), w, 5, gfx_rgb(20, 20, 20));
		gfx_fill(sx - w / 2 + 1, sy + (int)(10 * V.zoom) + 1, (int)((w - 2) * e->progress), 3, gfx_rgb(240, 200, 60));
	}
	if (e->sel || e->hp < e->maxhp * 0.9f)
		if (e->progress >= 1) draw_hpbar(sx, sy - (int)(46 * V.zoom), e->hp / e->maxhp, (int)(50 * V.zoom));
}

/* línea de puntos (rutas, punto de reunión) */
static void linea_puntos(int x0, int y0, int x1, int y1, u32 col) {
	int n = MAX(abs(x1 - x0), abs(y1 - y0)) / 4;
	for (int k = 0; k <= n; k += 2) {
		int x = x0 + (n ? (x1 - x0) * k / n : 0), y = y0 + (n ? (y1 - y0) * k / n : 0);
		gfx_fill(x - 1, y - 1, 3, 3, 0xff100c08u);
		gfx_fill(x, y, 2, 2, col);
	}
}

/* bandera del punto de reunión: la de los datos originales (animada, del color del jugador) o una dibujada */
static void bandera(int x, int y, int color) {
	Sprite *f = iface_frame(50404, (int)(SDL_GetTicks() / 120) % 7, 8);
	int sc = (int)(V.zoom * 256);
	if (f) { gfx_sprite(f, x, y, sc, color, 256, 0); return; }
	int hh = (int)(26 * V.zoom), ww = (int)(14 * V.zoom);
	gfx_fill(x - 1, y - hh, 2, hh, gfx_rgb(230, 225, 210));
	gfx_fill(x + 1, y - hh, ww, ww * 2 / 3, col_player[color & 7]);
	gfx_rect(x + 1, y - hh, ww, ww * 2 / 3, gfx_rgb(20, 14, 8));
	gfx_ellipse(x, y, (int)(6 * V.zoom) + 1, (int)(3 * V.zoom) + 1, gfx_rgb(0, 0, 0), 90);
}

static struct { float x, y; Uint32 t0; int tipo; } marcas[6];
static int marca_sig;
void marca_orden(float wx, float wy, int tipo) {
	marcas[marca_sig].x = wx; marcas[marca_sig].y = wy; marcas[marca_sig].t0 = SDL_GetTicks(); marcas[marca_sig].tipo = tipo;
	if (!marcas[marca_sig].t0) marcas[marca_sig].t0 = 1;
	marca_sig = (marca_sig + 1) % ARRAY_LEN(marcas);
}

int mm_filtro;

void render_world(int top_h, int bottom_h) {
	int ui_saved = gfx_ui_suspend();   /* el mundo se dibuja a la resolución real aunque la interfaz esté escalada */
	gfx_fill(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0));

	/* rango de casillas visibles: proyecta las cuatro esquinas de la pantalla */
	float wx[4], wy[4];
	screen_to_world(0, 0, &wx[0], &wy[0]);
	screen_to_world(V.sw, 0, &wx[1], &wy[1]);
	screen_to_world(0, V.sh, &wx[2], &wy[2]);
	screen_to_world(V.sw, V.sh, &wx[3], &wy[3]);
	int x0 = W.w, x1 = 0, y0 = W.h, y1 = 0;
	for (int k = 0; k < 4; ++k) {
		x0 = MIN(x0, (int)floorf(wx[k]) - 3); x1 = MAX(x1, (int)ceilf(wx[k]) + 3);
		y0 = MIN(y0, (int)floorf(wy[k]) - 3); y1 = MAX(y1, (int)ceilf(wy[k]) + 3);
	}
	x0 = CLAMP(x0, 0, W.w - 1); x1 = CLAMP(x1, 0, W.w - 1);
	y0 = CLAMP(y0, 0, W.h - 1); y1 = CLAMP(y1, 0, W.h - 1);

	if (hd_enabled) {
		render_terrain_hd((float)SDL_GetTicks() / 1000.0f, top_h, fb.h - bottom_h);
	} else {
		/* bajo las barras de la interfaz no hace falta dibujar el suelo: quedan tapadas */
		gfx_clip(0, top_h, fb.w, fb.h - bottom_h);
		render_terrain_tiles(x0, x1, y0, y1, (float)SDL_GetTicks() / 1000.0f);
		gfx_clip_reset();
	}

	/* recoge las entidades */
	static DrawItem *items; static int icap;
	int n = 0;
	if (icap < W.nents) { icap = W.nents + 256; items = (DrawItem *)realloc(items, sizeof(DrawItem) * (size_t)icap); }

	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!e->alive) continue;
		int fx = CLAMP((int)floorf(e->x), 0, W.w - 1), fy = CLAMP((int)floorf(e->y), 0, W.h - 1);
		u8 f = W.fog[fy * W.w + fx];
		if (!f) continue;
		if (is_unit(e->type) && !same_team(e->owner, W.human) && f < 2) continue;

		int sx, sy;
		view_to_screen(e->x, e->y, &sx, &sy);
		if (sx < -200 * V.zoom || sx > V.sw + 200 * V.zoom || sy < -200 * V.zoom || sy > V.sh + 200 * V.zoom) continue;

		items[n].idx = i;
		/* orden del pintor: lo que está más abajo en la pantalla está más cerca y se dibuja después. Aquí la y de pantalla es (x - y).
		   x + y solo desplaza las cosas hacia los lados, pero mantiene el orden estable para las cosas a la misma altura. */
		items[n].depth = (e->x - e->y) + (e->x + e->y) * 0.0005f + (e->state == ST_DYING ? -0.3f : 0.0f);
		++n;
	}

	qsort(items, (size_t)n, sizeof(DrawItem), cmp_depth);

	/* edificios que se derrumban (se dibujan antes, a ras de suelo) */
	for (int k = 0; k < fx_derrumbes(); ++k) {
		float t;
		const Entity *g = fx_derrumbe(k, &t);
		g_sink = t < 0.02f ? 0.02f : t;
		draw_entity(g, 256);
	}
	g_sink = 0;
	if (efx.sombras) for (int k = 0; k < n; ++k) draw_shadow(&W.ents[items[k].idx]);
	for (int k = 0; k < n; ++k) {
		Entity *e = &W.ents[items[k].idx];
		int fx = CLAMP((int)floorf(e->x), 0, W.w - 1), fy = CLAMP((int)floorf(e->y), 0, W.h - 1);
		draw_entity(e, W.fog[fy * W.w + fx] == 2 ? 256 : 140);
	}

	/* puntos de reunión de los edificios de producción seleccionados: línea de puntos y la bandera del jugador */
	for (int i = 0; i < W.nents; ++i) {
		Entity *b = &W.ents[i];
		if (!b->alive || !b->sel || !b->has_rally || b->owner != W.human) continue;
		int bx, by, fx, fy;
		view_to_screen(b->x, b->y, &bx, &by);
		Entity *rt = ent_get(b->rtarget);
		view_to_screen(rt ? rt->x : b->rx, rt ? rt->y : b->ry, &fx, &fy);
		linea_puntos(bx, by, fx, fy, gfx_rgb(250, 240, 200));
		bandera(fx, fy, pcolor(b->owner));
	}

	/* órdenes en cola (Mayús) y patrullas de las unidades propias seleccionadas */
	for (int i = 0; i < W.nents; ++i) {
		Entity *u = &W.ents[i];
		if (!u->alive || !u->sel || u->owner != W.human || !is_unit(u->type) || u->state == ST_DYING) continue;
		int px, py, qx, qy;
		view_to_screen(u->x, u->y, &px, &py);
		if (u->patrol) {
			view_to_screen(u->wpx[0], u->wpy[0], &px, &py);
			view_to_screen(u->ptx, u->pty, &qx, &qy);
			linea_puntos(px, py, qx, qy, gfx_rgb(255, 200, 80));
			gfx_diamond(px, py, 4, 2, gfx_rgb(255, 200, 80)); gfx_diamond(qx, qy, 4, 2, gfx_rgb(255, 200, 80));
			continue;
		}
		if (u->nwp <= 0) continue;
		if (u->state == ST_MOVE) { int ax, ay; view_to_screen(u->tx, u->ty, &ax, &ay); linea_puntos(px, py, ax, ay, gfx_rgb(140, 255, 140)); px = ax; py = ay; }
		for (int k = 0; k < u->nwp && k < 4; ++k) {
			view_to_screen(u->wpx[k], u->wpy[k], &qx, &qy);
			linea_puntos(px, py, qx, qy, gfx_rgb(140, 255, 140));
			gfx_diamond(qx, qy, 4, 2, gfx_rgb(140, 255, 140));
			px = qx; py = qy;
		}
	}

	/* marcas de las órdenes recién dadas (un rombo que se encoge) */
	{
		Uint32 now = SDL_GetTicks();
		for (int k = 0; k < ARRAY_LEN(marcas); ++k) {
			if (!marcas[k].t0 || now - marcas[k].t0 > 600) continue;
			int sx, sy;
			view_to_screen(marcas[k].x, marcas[k].y, &sx, &sy);
			int r = 14 - (int)(now - marcas[k].t0) * 12 / 600;
			u32 c = marcas[k].tipo == 1 ? gfx_rgb(255, 80, 60) : marcas[k].tipo == 2 ? gfx_rgb(255, 200, 80) : gfx_rgb(120, 255, 120);
			for (int a = 0; a < 2; ++a) {
				gfx_line(sx - r * 2, sy + a, sx, sy - r + a, c); gfx_line(sx, sy - r + a, sx + r * 2, sy + a, c);
				gfx_line(sx + r * 2, sy + a, sx, sy + r + a, c); gfx_line(sx, sy + r + a, sx - r * 2, sy + a, c);
			}
		}
	}

	/* flechas y virotes */
	for (int k = 0; k < W.nshots; ++k) {
		float t = W.shots[k].t;
		float x = W.shots[k].x0 + (W.shots[k].x1 - W.shots[k].x0) * t, y = W.shots[k].y0 + (W.shots[k].y1 - W.shots[k].y0) * t;
		float t2 = MAX(0.0f, t - 0.12f);
		float xb = W.shots[k].x0 + (W.shots[k].x1 - W.shots[k].x0) * t2, yb = W.shots[k].y0 + (W.shots[k].y1 - W.shots[k].y0) * t2;
		int sx, sy, bx, by;
		view_to_screen(x, y, &sx, &sy);
		view_to_screen(xb, yb, &bx, &by);
		float arc = sinf(t * 3.1416f) * 14 * V.zoom;
		gfx_line(bx, by - (int)(10 * V.zoom) - (int)arc, sx, sy - (int)(10 * V.zoom) - (int)arc, gfx_rgb(240, 230, 200));
	}

	/* partículas (humo, polvo, fuego, escombros) y la luz del día y la noche */
	fx_dibujar();
	fx_luz(top_h, fb.h - bottom_h);
	gfx_ui_resume(ui_saved);
}

void render_placement(EntType t, int tx, int ty) {
	int s = bld_def[t].size, ok = can_place(t, tx, ty);
	int cx[4], cy[4];
	view_to_screen((float)tx, (float)ty, &cx[0], &cy[0]);
	view_to_screen((float)(tx + s), (float)ty, &cx[1], &cy[1]);
	view_to_screen((float)(tx + s), (float)(ty + s), &cx[2], &cy[2]);
	view_to_screen((float)tx, (float)(ty + s), &cx[3], &cy[3]);

	u32 col = ok ? gfx_rgb(90, 255, 90) : gfx_rgb(255, 70, 70);
	for (int k = 0; k < 4; ++k) gfx_line(cx[k], cy[k], cx[(k + 1) % 4], cy[(k + 1) % 4], col);

	Entity ghost;
	memset(&ghost, 0, sizeof ghost);
	ghost.type = t; ghost.owner = W.human; ghost.bx = tx; ghost.by = ty; ghost.size = s;
	ghost.x = tx + s / 2.0f; ghost.y = ty + s / 2.0f; ghost.progress = 1; ghost.maxhp = ghost.hp = 1;
	draw_entity(&ghost, 170);
}

/* ---------- minimapa ---------- */

static void mini_map_to_tile(int x, int y, int w, int h, int mx, int my, float *tx, float *ty, int *inside) {
	/* rombo: izquierda=(0,0) abajo=(W,0) derecha=(W,H) arriba=(0,H) en el espacio de casillas */
	float dx = (mx - x) / (float)w, dy = (my - y) / (float)h; /* 0..1 */
	float u = dx + (dy - 0.5f), v = dx - (dy - 0.5f);
	*tx = u * W.w; *ty = v * W.h;
	*inside = u >= 0 && u < 1 && v >= 0 && v < 1;
}

/* La base del minimapa (terreno y niebla) cambia despacio: se calcula en un búfer y se rehace unas 3 veces por segundo
 * (antes eran w*h cálculos con coma flotante por cuadro). Se vuelca a la escala actual de la interfaz. */
static u32 *mm_cache; static int mm_w, mm_h; static Uint32 mm_t; static const void *mm_world;

static void minimap_refresh(int w, int h) {
	static const u32 tc[4] = { 0xffd2b46e, 0xff3c8232, 0xff0046a0, 0xff00286e };
	for (int py = 0; py < h; ++py) {
		for (int px = 0; px < w; ++px) {
			float tx, ty; int in;
			mini_map_to_tile(0, 0, w, h, px, py, &tx, &ty, &in);
			u32 c = 0xff0c0806u;
			if (in) {
				int i = (int)ty * W.w + (int)tx;
				u8 f = W.fog[i];
				c = 0xff000000u;
				if (f) {
					u32 b = hd_enabled ? tex_tile_color(W.tile[i]) : tc[W.tile[i]];
					int r = (b >> 16) & 255, g = (b >> 8) & 255, bl = b & 255;
					if (f == 1) { r = r * 55 / 100; g = g * 55 / 100; bl = bl * 55 / 100; }
					c = gfx_rgb(r, g, bl);
				}
			}
			mm_cache[py * w + px] = c;
		}
	}
}

void render_minimap(int x, int y, int w, int h) {
	int us = gfx_us;
	gfx_fill(x - 3, y - 3, w + 6, h + 6, gfx_rgb(12, 8, 6));
	gfx_rect(x - 3, y - 3, w + 6, h + 6, gfx_rgb(205, 165, 70));
	gfx_rect(x - 2, y - 2, w + 4, h + 4, gfx_rgb(96, 70, 34));
	if (!mm_cache || mm_w != w || mm_h != h) { free(mm_cache); mm_cache = (u32 *)malloc((size_t)w * h * 4); mm_w = w; mm_h = h; mm_t = 0; }
	if (!mm_cache) return;
	Uint32 now = SDL_GetTicks();
	if (!mm_t || now - mm_t > 350 || mm_world != W.fog) { mm_t = now; mm_world = W.fog; minimap_refresh(w, h); }
	for (int py = 0; py < h; ++py) {
		int yy0 = (y + py) * us;
		for (int k = 0; k < us; ++k) {
			int yy = yy0 + k;
			if (yy < 0 || yy >= gfx_real_h()) continue;
			u32 *dst = fb.px + (size_t)yy * fb.pitch + x * us;
			if (us == 1) { memcpy(dst, mm_cache + py * w, (size_t)w * 4); continue; }
			for (int px = 0; px < w; ++px) for (int q = 0; q < us; ++q) dst[px * us + q] = mm_cache[py * w + px];
		}
	}
	/* recursos (filtro «recursos»): oro, piedra, bayas y peces */
	if (mm_filtro & MM_RECURSOS) {
		for (int i = 0; i < W.nents; ++i) {
			Entity *e = &W.ents[i];
			if (!e->alive || !(e->type == E_GOLD || e->type == E_STONE || e->type == E_BERRIES || e->type == E_FISH)) continue;
			int fx = CLAMP((int)e->x, 0, W.w - 1), fy = CLAMP((int)e->y, 0, W.h - 1);
			if (!W.fog[fy * W.w + fx]) continue;
			float u = e->x / W.w, v = e->y / W.h;
			int mx = x + (int)((u + v) * 0.5f * w), my = y + (int)((0.5f + (u - v) * 0.5f) * h);
			u32 c = e->type == E_GOLD ? gfx_rgb(255, 215, 40) : e->type == E_STONE ? gfx_rgb(200, 200, 205) : e->type == E_BERRIES ? gfx_rgb(230, 80, 200) : gfx_rgb(140, 220, 255);
			gfx_fill(mx - 1, my - 1, 2, 2, c);
		}
	}
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!e->alive || e->owner < 0 || e->state == ST_DYING || e->state == ST_ABOARD) continue;
		int fx = CLAMP((int)e->x, 0, W.w - 1), fy = CLAMP((int)e->y, 0, W.h - 1);
		u8 f = W.fog[fy * W.w + fx];
		if (!f || (is_unit(e->type) && !same_team(e->owner, W.human) && f < 2)) continue;
		int militar = is_unit(e->type) && e->type != E_VILLAGER && e->type != E_FISHBOAT && e->type != E_TRANSPORT && !is_trader(e->type) && e->type < E_HORSE;
		if ((mm_filtro & MM_MILITAR) && !militar && !is_building(e->type)) continue;
		float u = e->x / W.w, v = e->y / W.h;
		int mx = x + (int)((u + v) * 0.5f * w), my = y + (int)((0.5f + (u - v) * 0.5f) * h);
		int s = is_building(e->type) ? 3 : 2;
		u32 c = col_player[pcolor(e->owner)];
		if ((mm_filtro & MM_MILITAR) && !militar) c = (c >> 1) & 0x7f7f7fu;   /* solo militar: los edificios, apagados */
		else if ((mm_filtro & MM_MILITAR) && militar) s = 3;
		gfx_fill(mx - s / 2, my - s / 2, s, s, c | 0xff000000u);
	}

	/* reliquias */
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!e->alive || e->type != E_RELIC) continue;
		int fx = CLAMP((int)e->x, 0, W.w - 1), fy = CLAMP((int)e->y, 0, W.h - 1);
		if (!W.fog[fy * W.w + fx]) continue;
		float u = e->x / W.w, v = e->y / W.h;
		int mx = x + (int)((u + v) * 0.5f * w), my = y + (int)((0.5f + (u - v) * 0.5f) * h);
		gfx_fill(mx - 1, my - 1, 3, 3, gfx_rgb(255, 230, 80));
	}

	/* alguien está siendo atacado: círculos rojos que crecen y una cruz que parpadea en el sitio */
	for (int k = 0; k < W.npings; ++k) {
		float u = W.pings[k].x / W.w, v = W.pings[k].y / W.h;
		int mx = x + (int)((u + v) * 0.5f * w), my = y + (int)((0.5f + (u - v) * 0.5f) * h);
		int rad = 3 + (int)(W.pings[k].t * 9) % 14;
		for (int a = 0; a < 20; ++a) {
			float ang = a * 6.2831853f / 20;
			gfx_fill(mx + (int)(cosf(ang) * rad * 1.6f), my + (int)(sinf(ang) * rad * 0.8f), 2, 2, gfx_rgb(255, 40, 40));
		}
		u32 pc = (SDL_GetTicks() / 250) & 1 ? gfx_rgb(255, 255, 255) : gfx_rgb(255, 40, 40);
		gfx_fill(mx - 4, my - 1, 9, 3, pc);
		gfx_fill(mx - 1, my - 4, 3, 9, pc);
	}

	/* cámara */
	float wx, wy;
	screen_to_world(V.sw / 2, V.sh / 2, &wx, &wy);
	float u = CLAMP(wx / W.w, 0, 1), v = CLAMP(wy / W.h, 0, 1);
	int mx = x + (int)((u + v) * 0.5f * w), my = y + (int)((0.5f + (u - v) * 0.5f) * h);
	gfx_rect(mx - 8, my - 5, 16, 10, gfx_rgb(255, 255, 255));
}

int minimap_pick(int x, int y, int w, int h, int mx, int my, float *wx, float *wy) {
	int in;
	mini_map_to_tile(x, y, w, h, mx, my, wx, wy, &in);
	return in;
}

/* ---------- ayuda para desarrollo: hojas de contacto de los sprites de GRAPHICS.DRS ---------- */

/* modo 0: sprites estáticos (1..4 cuadros), modo 1: animaciones (cuadros múltiplo de 5, al menos 20) */
int sheet_list(int mode, int *ids, int max) {
	int n = 0;
	for (int i = 0; i < d_graphics.count && n < max; ++i) {
		Slp *s = slp_load(&d_graphics, d_graphics.items[i].id);
		if (!s) continue;
		int ok = mode == 0 ? (s->nframes >= 1 && s->nframes <= 4) : mode == 2 ? (s->nframes >= 8 && s->nframes % 5 != 0) : (s->nframes >= 20 && s->nframes % 5 == 0);
		if (ok) {
			Sprite *f = slp_frame(s, 0);
			if (f && f->w > 0 && (mode == 1 || f->w >= 30)) ids[n++] = (int)d_graphics.items[i].id;
		}
		free(s->data);
		for (int k = 0; k < s->nframes; ++k) if (s->frames[k]) { free(s->frames[k]->px); free(s->frames[k]->pc); free(s->frames[k]); }
		free(s->frames); free(s);
	}
	return n;
}

void sheet_draw(const int *ids, int n) {
	gfx_fill(0, 0, fb.w, fb.h, gfx_rgb(50, 50, 50));
	int cols = getenv("AOEN_COLS") ? atoi(getenv("AOEN_COLS")) : 8, cw = fb.w / cols, ch = getenv("AOEN_CELLH") ? atoi(getenv("AOEN_CELLH")) : 112;
	int maxsc = getenv("AOEN_ZOOM") ? atoi(getenv("AOEN_ZOOM")) : 256;
	/* AOEN_SHEETDRS=i: los gráficos de INTERFAC.DRS; AOEN_SLPFRAMES=id: todos los cuadros de ese SLP */
	Drs *dd = getenv("AOEN_SHEETDRS") && getenv("AOEN_SHEETDRS")[0] == 'i' ? &d_interfac : &d_graphics;
	int allf = getenv("AOEN_SLPFRAMES") ? atoi(getenv("AOEN_SLPFRAMES")) : 0;
	if (allf) { Slp *s = slp_load(dd, (u32)allf); n = s ? MIN(s->nframes, 200) : 0; }
	for (int k = 0; k < n; ++k) {
		int cx = (k % cols) * cw, cy = (k / cols) * ch;
		Slp *s = slp_load(dd, allf ? (u32)allf : (u32)ids[k]);
		if (s) {
			Sprite *f = slp_frame(s, allf ? k : getenv("AOEN_FRAME") ? atoi(getenv("AOEN_FRAME")) + k * (getenv("AOEN_FSTEP") ? atoi(getenv("AOEN_FSTEP")) : 0) : 0);
			if (f && f->w > 0) {
				int sc = MIN(maxsc, MIN((cw - 6) * 256 / f->w, (ch - 24) * 256 / f->h));
				gfx_clip(cx, cy, cx + cw, cy + ch);
				gfx_sprite(f, cx + cw / 2 - (f->w * sc / 512) + f->hx * sc / 256, cy + 4 + f->hy * sc / 256, sc, 0, 256, 0);
				gfx_clip_reset();
			}
		}
		char b[32];
		snprintf(b, sizeof b, "%d", allf ? k : ids[k]);
		gfx_text(cx + 3, cy + ch - 12, b, gfx_rgb(255, 255, 0), 1);
	}
}

/* ---------- ayuda para desarrollo: busca animaciones de la misma unidad comparando histogramas de color ---------- */

static void histo(Slp *s, float *h) {
	memset(h, 0, sizeof(float) * 256);
	double tot = 0;
	for (int f = 0; f < s->nframes; f += MAX(1, s->nframes / 10)) {
		Sprite *sp = slp_frame(s, f);
		if (!sp || !sp->px) continue;
		for (int i = 0; i < sp->w * sp->h; ++i)
			if (sp->px[i]) { h[sp->px[i]] += 1; tot += 1; }
	}
	if (tot > 0) for (int i = 0; i < 256; ++i) h[i] /= (float)tot;
}

void match_report(const int *stand, int n) {
	static int ids[4096];
	static float hs[4096][256];
	static int nf[4096];
	int cnt = 0;

	for (int i = 0; i < d_graphics.count && cnt < 4096; ++i) {
		Slp *s = slp_load(&d_graphics, d_graphics.items[i].id);
		if (!s) continue;
		if (s->nframes >= 20 && s->nframes % 5 == 0 && s->nframes <= 200) {
			ids[cnt] = (int)d_graphics.items[i].id;
			nf[cnt] = s->nframes;
			histo(s, hs[cnt]);
			++cnt;
		}
	}

	for (int k = 0; k < n; ++k) {
		int self = -1;
		for (int i = 0; i < cnt; ++i) if (ids[i] == stand[k]) self = i;
		if (self < 0) { printf("%d: not found\n", stand[k]); continue; }
		printf("stand %d (%d frames):\n", stand[k], nf[self]);
		/* mejores candidatos por clase de número de cuadros */
		static const int classes[][2] = { { 75, 75 }, { 50, 50 }, { 55, 80 }, { 30, 30 } };
		static const char *cn[] = { "move(75)", "die(50)", "attack(55-80)", "decay/stand(30)" };
		for (int c = 0; c < 4; ++c) {
			int bi[3] = { -1, -1, -1 };
			float bs[3] = { 0, 0, 0 };
			for (int i = 0; i < cnt; ++i) {
				if (i == self || nf[i] < classes[c][0] || nf[i] > classes[c][1]) continue;
				float sc = 0;
				for (int p = 1; p < 256; ++p) sc += MIN(hs[self][p], hs[i][p]);
				for (int r = 0; r < 3; ++r)
					if (sc > bs[r]) { for (int q = 2; q > r; --q) { bs[q] = bs[q - 1]; bi[q] = bi[q - 1]; } bs[r] = sc; bi[r] = i; break; }
			}
			printf("  %-16s", cn[c]);
			for (int r = 0; r < 3; ++r) if (bi[r] >= 0) printf(" %d(%.2f)", ids[bi[r]], bs[r]);
			printf("\n");
		}
	}
}

/* ayuda para desarrollo: muestra los cuadros de cada conjunto de unidades, para comprobar los identificadores gráficos */
void units_sheet(void) {
	gfx_fill(0, 0, fb.w, fb.h, gfx_rgb(50, 50, 50));
	for (int u = 0; u < NUM_UNIT_ART; ++u) {
		int x = 4 + (u % 2) * (fb.w / 2), y = 4 + (u / 2) * 118;
		Slp *sl[5] = { unit_art[u].stand, unit_art[u].move, unit_art[u].attack, unit_art[u].die, unit_art[u].decay };
		for (int k = 0; k < 5; ++k) {
			Sprite *sp = NULL;
			if (sl[k]) { int n = MAX(1, sl[k]->nframes / 5); sp = slp_frame(sl[k], 2 * n + n / 2); }
			if (sp) gfx_sprite(sp, x + 40 + k * 96, y + 80, 300, 0, 256, 0);
			char b[16];
			snprintf(b, sizeof b, "%u", unit_art_ids[u][k]);
			gfx_text(x + 10 + k * 96, y + 100, b, gfx_rgb(255, 255, 0), 1);
		}
		char b[16];
		snprintf(b, sizeof b, "#%d", u);
		gfx_text(x, y, b, gfx_rgb(255, 255, 255), 2);
	}
}
