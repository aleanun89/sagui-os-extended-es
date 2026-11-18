/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* AoEN - aplicación: ventana, entrada, menús, HUD y bucle del juego. */
#include "ui.h"
#include "config.h"
#include "efectos.h"
#include <time.h>

/* ---------- tema ---------- */
#define COL_PANEL  gfx_rgb(46, 29, 19)
#define COL_PANEL2 gfx_rgb(30, 19, 13)
#define COL_GOLD   gfx_rgb(205, 165, 70)
#define COL_CREAM  gfx_rgb(245, 230, 190)
#define COL_RED    gfx_rgb(150, 28, 24)
#define COL_REDH   gfx_rgb(190, 45, 38)
#define COL_DIM    gfx_rgb(150, 135, 105)

typedef enum { SC_MENU, SC_SETUP, SC_OPTIONS, SC_GAME, SC_SAVES } Screen;

static SDL_Window *win;
static SDL_Surface *fbs;
static Screen screen = SC_MENU, options_back = SC_MENU, saves_back = SC_MENU;
static int saves_mode; /* 0 cargar, 1 guardar */
static int top_h = 30, bottom_h = 132;
static int guard_mode, show_score = 1;

static int ui_s = 1;   /* escala de la interfaz de este cuadro (1 o 2): ver efectos.h */
static int lw(void) { return gfx_real_w() / ui_s; }   /* ancho lógico de la interfaz */
static int lh(void) { return gfx_real_h() / ui_s; }

static void minimap_rect(int *x, int *y, int *w, int *h) {
	int mw = MIN(240, lw() / 3);
	*w = mw; *h = mw / 2;
	*x = lw() - mw - 14;
	*y = lh() - bottom_h + (bottom_h - mw / 2) / 2;
}
static float last_autosave;
static int running = 1, fullscreen, show_fps, scroll_speed = 2, paused_menu, game_speed = 1;
/* Presentación del cuadro: por la superficie de la ventana (lo de siempre) o por el renderizador 2D de la SDL, que en SaguiOS
 * usa la aceleración 2D del sistema si la GPU la tiene (D288) y si no sigue por software. Se elige al arrancar (opción
 * «render2d» en aoen.cfg o AOEN_RENDER=1). */
static SDL_Renderer *rnd;
static SDL_Texture *rtex;
static int rtex_w, rtex_h, use_renderer;

static void present_frame(void) {
	if (use_renderer && rnd) {
		if (!rtex || rtex_w != fb.w || rtex_h != fb.h) {
			if (rtex) SDL_DestroyTexture(rtex);
			rtex = SDL_CreateTexture(rnd, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, fb.w, fb.h);
			rtex_w = fb.w; rtex_h = fb.h;
		}
		if (rtex) {
			SDL_UpdateTexture(rtex, NULL, fb.px, fb.pitch * 4);
			SDL_RenderCopy(rnd, rtex, NULL, NULL);
			SDL_RenderPresent(rnd);
			return;
		}
	}
	SDL_Surface *ws = SDL_GetWindowSurface(win);
	if (ws) {
		SDL_BlitSurface(fbs, NULL, ws, NULL);
		SDL_UpdateWindowSurface(win);
	}
}

static int win_w = 1024, win_h = 720;   /* tamaño de la ventana fuera de la pantalla completa */
static void resize_fb(int w, int h);

/* Pone o quita la pantalla completa según «fullscreen». En SaguiOS el lienzo no cambia solo con la ventana (la SDL del
 * sistema lo escala): se pide el tamaño del escritorio (o el de la ventana al salir) y el búfer se rehace a ese tamaño,
 * así se dibuja a la resolución real y no se amplía un lienzo pequeño. */
static void apply_fullscreen(void) {
	SDL_SetWindowFullscreen(win, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
#ifdef SAGUI_AOEN
	int w = win_w, h = win_h;
	if (fullscreen) { SDL_DisplayMode dm; if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) { w = dm.w; h = dm.h; } }
	SDL_SetWindowSize(win, w, h);
	resize_fb(w, h);
#endif
}
static char data_dir[1100];
static char cmd_data[1100];

/* estado de la entrada del cuadro actual */
static int mx, my, mdown, mclick, rclick, wheel, mouse_seen, mpressed, mdx, mdy;
static int key_pressed[512];
static const Uint8 *keys;
static char typed[32];

/* ---------- controles de la interfaz ---------- */

static int mod_down(void) { return keys && (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL] || keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]); }

/* el ratón va siempre en píxeles reales; los rectángulos de la interfaz, en coordenadas lógicas (x ui_s) */
/* zoom inicial: en pantallas grandes la partida empieza más cerca (a 1080p, 1.5x) para que el mapa no se vea pequeño */
static float default_zoom(void) { return gfx_real_h() >= 1000 ? 1.5f : 1.0f; }

static int hit(int x, int y, int w, int h) { return mx >= x * ui_s && my >= y * ui_s && mx < (x + w) * ui_s && my < (y + h) * ui_s; }

/* interfaz moderna: degradados suaves, esquinas recortadas, filo claro arriba y sombra abajo */
static void bevel(int x, int y, int w, int h, u32 border) {
	gfx_fill(x + 1, y, w - 2, 1, border); gfx_fill(x + 1, y + h - 1, w - 2, 1, border);
	gfx_fill(x, y + 1, 1, h - 2, border); gfx_fill(x + w - 1, y + 1, 1, h - 2, border);
}

static void panel(int x, int y, int w, int h) {
	gfx_fill_grad(x, y, w, h, gfx_rgb(62, 42, 28), gfx_rgb(34, 22, 15));
	bevel(x, y, w, h, COL_GOLD);
	bevel(x + 1, y + 1, w - 2, h - 2, gfx_rgb(96, 70, 34));
	gfx_fill(x + 3, y + 2, w - 6, 1, gfx_rgb(130, 100, 56));   /* brillo */
	gfx_fill(x + 3, y + h - 3, w - 6, 1, gfx_rgb(14, 9, 6));   /* sombra */
}

static int button(int x, int y, int w, int h, const char *label, int enabled, int size) {
	int over = enabled && hit(x, y, w, h), down = over && mdown;
	u32 top = !enabled ? gfx_rgb(78, 62, 50) : down ? gfx_rgb(110, 22, 18) : over ? gfx_rgb(214, 66, 54) : gfx_rgb(184, 44, 36);
	u32 bot = !enabled ? gfx_rgb(58, 46, 38) : down ? gfx_rgb(84, 14, 12) : over ? gfx_rgb(150, 32, 26) : gfx_rgb(118, 24, 20);
	gfx_fill_grad(x, y, w, h, top, bot);
	bevel(x, y, w, h, enabled ? COL_GOLD : gfx_rgb(110, 95, 70));
	if (enabled && !down) gfx_fill(x + 2, y + 1, w - 4, 1, gfx_rgb(255, 190, 150));   /* brillo superior */
	gfx_text_c(x + w / 2, y + (h - 7 * size) / 2 + (down ? 1 : 0), label, enabled ? COL_CREAM : gfx_rgb(160, 150, 130), size);
	if (over && mclick) snd_ui(SND_CLICK);
	return over && mclick;
}

/* selector "< valor >", devuelve -1, 0 o +1 */
static int cycler(int x, int y, int w, const char *label) {
	int h = 28, ret = 0;
	gfx_fill_grad(x, y, w, h, gfx_rgb(40, 26, 18), COL_PANEL2);
	bevel(x, y, w, h, COL_GOLD);
	int tsz = gfx_text_w(label, 2) > w - 64 ? 1 : 2;
	gfx_text_c(x + w / 2, y + (tsz == 2 ? 7 : 10), label, COL_CREAM, tsz);
	if (button(x, y, 28, h, "<", 1, 2)) ret = -1;
	if (button(x + w - 28, y, 28, h, ">", 1, 2)) ret = 1;
	return ret;
}

static int checkbox(int x, int y, const char *label, int *v) {
	int over = hit(x, y, 32 + gfx_text_w(label, 2), 22);
	gfx_fill_grad(x, y, 22, 22, over ? gfx_rgb(58, 38, 26) : gfx_rgb(40, 26, 18), COL_PANEL2);
	bevel(x, y, 22, 22, COL_GOLD);
	if (*v) { gfx_fill(x + 5, y + 5, 12, 12, gfx_rgb(120, 220, 110)); gfx_fill(x + 5, y + 5, 12, 2, gfx_rgb(190, 255, 170)); }
	gfx_text(x + 32, y + 4, label, COL_CREAM, 2);
	if (over && mclick) { *v = !*v; snd_ui(SND_CLICK); return 1; }
	return 0;
}

/* ---------- estado de la pantalla de configuración ---------- */

static const char *map_names[] = { "Aleatorio", "Islas", "Continentes", "Costero", "Interior", "Tierras altas" };
static const char *size_names[] = { "Diminuto (48)", "Pequeño (72)", "Mediano (100)", "Grande (150)", "Enorme (200)", "Gigante (260)", "Colosal (320)", "Titánico (400)", "Épico (512)" };
static const int size_values[] = { 48, 72, 100, 150, 200, 260, 320, 400, 512 };
static const char *team_mode_names[] = { "Cada uno por su cuenta", "Aleatorios", "Personalizados" };
static const int res_values[] = { 0, 100, 200, 500, 1000 };

static const int pop_values[] = { 50, 75, 100, 150, 200, 300 };
static const char *diff_names[] = { "Fácil", "Normal", "Difícil" };

static struct {
	int players, map, size, team_mode, seed, std_res, seed_focus, popcap;
	int diff[MAX_PLAYERS], civ[MAX_PLAYERS];
	int team[MAX_PLAYERS];
	int res[RES_COUNT]; /* índice en res_values */
} su = { 4, 0, 2, 1, 1, 1, 0, 4, { 1, 1, 1, 1, 1, 1, 1, 1 }, { 0, 1, 2, 3, 4, 5, 6, 7 }, { 1, 2, 3, 4, 5, 6, 7, 8 }, { 2, 2, 2, 2 } };

static void new_seed(void) { su.seed = (int)(SDL_GetTicks() * 2654435761u) & 0x7fffffff; if (su.seed < 1) su.seed = 1; }

static void start_game(void) {
	Setup s;
	memset(&s, 0, sizeof s);
	s.width = s.height = size_values[su.size];
	s.seed = su.seed;
	s.players = su.players;
	s.map_kind = su.map == 0 ? su.seed % MAP_COUNT : su.map - 1;
	s.popcap = pop_values[CLAMP(su.popcap, 0, 5)];
	for (int i = 0; i < MAX_PLAYERS; ++i) { s.ai_diff[i] = su.diff[i]; s.civ[i] = su.civ[i]; }
	s.villagers = getenv("AOEN_VILL") ? atoi(getenv("AOEN_VILL")) : 4;
	s.explored = getenv("AOEN_REVEAL") ? 1 : 0;

	if (su.std_res) {
		s.start_res[RES_WOOD] = 200; s.start_res[RES_FOOD] = 200; s.start_res[RES_GOLD] = 100; s.start_res[RES_STONE] = 0;
	} else {
		for (int k = 0; k < RES_COUNT; ++k) s.start_res[k] = res_values[su.res[k]];
	}

	unsigned rs = (unsigned)su.seed * 7u + 3u;
	for (int i = 0; i < su.players; ++i) {
		if (su.team_mode == 0) s.team[i] = i + 1;
		else if (su.team_mode == 2) s.team[i] = su.team[i];
		else {
			rs = rs * 1103515245u + 12345u;
			int teams = MAX(2, MIN(su.players, 2 + (int)((rs >> 16) % (unsigned)MAX(1, su.players - 1))));
			s.team[i] = i < 2 ? i + 1 : 1 + (int)((rs >> 8) % (unsigned)teams);
		}
	}

	world_new(&s);
	fx_limpiar();
	AOEN_LOG("[AOEN] partida iniciada: %d jugadores, mapa %dx%d, semilla %d", su.players, s.width, s.height, su.seed);
	last_autosave = 0;
	/* busca el centro urbano del jugador humano */
	for (int i = 0; i < W.nents; ++i)
		if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER && W.ents[i].owner == W.human) view_center_on(W.ents[i].x, W.ents[i].y);
	V.zoom = default_zoom();
	game_speed = 1;
	paused_menu = 0;
	screen = SC_GAME;
}

/* ---------- partidas guardadas ---------- */

#define NUM_SLOTS 10
static struct { int exists, incompatible; SaveInfo si; } slots[NUM_SLOTS];
static char saves_msg[1400];
static void slot_path(int i, char *out, size_t n) {
	char d[1100];
	cfg_saves_dir(d, sizeof d);
	if (i == 0) snprintf(out, n, "%sauto.sav", d);
	else if (i == 1) snprintf(out, n, "%squick.sav", d);
	else snprintf(out, n, "%sslot%d.sav", d, i - 1);
}

static void slot_name(int i, char *out, size_t n) {
	if (i == 0) snprintf(out, n, "Guardado automático");
	else if (i == 1) snprintf(out, n, "Guardado rápido");
	else snprintf(out, n, "Ranura %d", i - 1);
}

static int any_save(void) {
	for (int i = 0; i < NUM_SLOTS; ++i) {
		char p[1200];
		slot_path(i, p, sizeof p);
		FILE *f = fopen(p, "rb");
		if (f) { fclose(f); return 1; }
	}
	return 0;
}

static void refresh_slots(void) {
	for (int i = 0; i < NUM_SLOTS; ++i) {
		char p[1200];
		slot_path(i, p, sizeof p);
		slots[i].exists = save_info(p, &slots[i].si);
		/* un fichero que no se puede leer pertenece a otra versión del juego */
		FILE *f = fopen(p, "rb");
		slots[i].incompatible = !slots[i].exists && f != NULL;
		if (f) fclose(f);
	}
}

static int do_save(int i) {
	char p[1200];
	slot_path(i, p, sizeof p);
	if (!W.nents || !world_save(p, (long long)time(NULL))) {
		AOEN_LOG("[AOEN] guardado fallido ranura %d", i); world_msg("No se pudo guardar la partida"); snprintf(saves_msg, sizeof saves_msg, "No se pudo guardar en %s", p); return 0; }
	AOEN_LOG("[AOEN] guardada ranura %d (%d entidades)", i, W.nents);
	world_msg(i == 0 ? "Guardado automático" : "Partida guardada");
	snprintf(saves_msg, sizeof saves_msg, "Partida guardada");
	return 1;
}

static void enter_loaded_game(void) {
	fx_limpiar();
	for (int i = 0; i < W.nents; ++i)
		if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER && W.ents[i].owner == W.human) { view_center_on(W.ents[i].x, W.ents[i].y); break; }
	V.zoom = default_zoom();
	game_speed = 1;
	paused_menu = 0;
	last_autosave = W.time;
	screen = SC_GAME;
	world_msg("Partida cargada");
}

static int do_load(int i) {
	char p[1200];
	slot_path(i, p, sizeof p);
	if (!world_load(p)) { world_msg("No se pudo cargar la partida"); snprintf(saves_msg, sizeof saves_msg, "No se pudo cargar: el archivo es de otra versión o está dañado"); return 0; }
	AOEN_LOG("[AOEN] cargada ranura %d (%d entidades)", i, W.nents);
	enter_loaded_game();
	return 1;
}

static void draw_saves(void) {
	if (W.nents) { render_world(top_h * ui_s, bottom_h * ui_s); gfx_fill_a(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0), 170); }
	else { draw_background(0); gfx_fill_a(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0), 150); }

	int pw = MIN(fb.w - 40, 720), ph = MIN(fb.h - 60, 90 + NUM_SLOTS * 44 + 70), px = (fb.w - pw) / 2, py = (fb.h - ph) / 2;
	panel(px, py, pw, ph);
	gfx_text_c(fb.w / 2, py + 20, saves_mode ? "GUARDAR PARTIDA" : "CARGAR PARTIDA", COL_GOLD, 3);

	int y = py + 70;
	for (int i = 0; i < NUM_SLOTS; ++i) {
		if (saves_mode && i == 0) continue; /* la ranura automática no se escribe a mano */
		char nm[64], info[160];
		slot_name(i, nm, sizeof nm);
		gfx_fill(px + 20, y, pw - 40, 38, COL_PANEL2);
		gfx_rect(px + 20, y, pw - 40, 38, gfx_rgb(90, 66, 30));
		gfx_text(px + 30, y + 6, nm, COL_CREAM, 2);

		if (slots[i].exists) {
			time_t t = (time_t)slots[i].si.stamp;
			char ts[48] = "";
			struct tm *lt = localtime(&t);
			if (lt) strftime(ts, sizeof ts, "%d/%m/%Y %H:%M", lt);
			int tm = (int)slots[i].si.time;
			snprintf(info, sizeof info, "%d jug.  %dx%d  %02d:%02d  %s", slots[i].si.players, slots[i].si.w, slots[i].si.h, tm / 60, tm % 60, ts);
			gfx_text(px + 30, y + 26, info, COL_DIM, 1);
		} else if (slots[i].incompatible) {
			gfx_text(px + 30, y + 26, "Guardado de otra versión del juego: no se puede cargar", gfx_rgb(255, 150, 110), 1);
		} else {
			gfx_text(px + 30, y + 26, "(vacía)", COL_DIM, 1);
		}

		int bx = px + pw - 150;
		if (saves_mode) {
			if (button(bx, y + 4, 120, 30, slots[i].exists ? "Sobrescribir" : "Guardar", 1, 1)) {
				if (do_save(i)) { refresh_slots(); screen = saves_back; }
			}
		} else if (button(bx, y + 4, 120, 30, "Cargar", slots[i].exists, 2)) {
			do_load(i);
		}
		y += 44;
	}

	{
		char dir[1100];
		cfg_saves_dir(dir, sizeof dir);
		if (saves_msg[0]) gfx_text_c(fb.w / 2, py + ph - 76, saves_msg, gfx_rgb(255, 235, 150), 1);
		gfx_text_c(fb.w / 2, py + ph - 16, dir, COL_DIM, 1);
	}
	if (button(px + (pw - 200) / 2, py + ph - 56, 200, 34, "Volver", 1, 2)) screen = saves_back;
}

static void draw_setup(void) {
	draw_background(1);
	gfx_fill_a(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0), 120);

	int pw = MIN(fb.w - 40, 860), ph = MIN(fb.h - 120, 520);
	int px = (fb.w - pw) / 2, py = (fb.h - ph) / 2 + 20;
	panel(px, py, pw, ph);

	/* pancarta del título */
	int bw = MIN(pw - 120, 460), bx = (fb.w - bw) / 2;
	gfx_fill(bx, py - 36, bw, 62, COL_RED);
	gfx_rect(bx, py - 36, bw, 62, COL_GOLD);
	gfx_rect(bx + 2, py - 34, bw - 4, 58, gfx_rgb(110, 70, 30));
	gfx_text_c(fb.w / 2, py - 36 + 19, "PARTIDA ALEATORIA", COL_GOLD, 3);

	if (button(16, 16, 130, 36, "<  Volver", 1, 2)) screen = SC_MENU;

	int lx = px + 28, lw = pw / 2 - 50, rx = px + pw / 2 + 10, rw = pw / 2 - 40, y = py + 44;

	/* jugadores */
	gfx_text(lx, y, "JUGADORES", COL_GOLD, 2);
	if (button(lx + lw - 100, y - 6, 28, 26, "-", 1, 2) && su.players > 1) --su.players;
	char buf[64];
	snprintf(buf, sizeof buf, "%d", su.players);
	gfx_text_c(lx + lw - 52, y, buf, COL_CREAM, 2);
	if (button(lx + lw - 28, y - 6, 28, 26, "+", 1, 2) && su.players < MAX_PLAYERS) ++su.players;
	y += 34;

	int rowh = MIN(42, (ph - 190) / MAX_PLAYERS);
	for (int i = 0; i < su.players; ++i) {
		gfx_fill(lx, y, lw, rowh - 4, COL_PANEL2);
		gfx_rect(lx, y, lw, rowh - 4, gfx_rgb(90, 66, 30));
		gfx_fill(lx, y, 12, rowh - 4, col_player[i]);
		gfx_text(lx + 24, y + (rowh - 4 - 14) / 2, i == 0 ? "Tú" : "IA", COL_CREAM, 2);
		{
			int c2 = cycler(lx + 52, y + (rowh - 4 - 28) / 2, 150, civ_def[su.civ[i]].name);
			if (c2) su.civ[i] = (su.civ[i] + c2 + NUM_CIVS) % NUM_CIVS;
		}
		int rx2 = lx + lw - 6;
		if (su.team_mode == 2) {
			snprintf(buf, sizeof buf, "Equipo %d", su.team[i]);
			rx2 -= 112;
			int c = cycler(rx2, y + (rowh - 4 - 28) / 2, 112, buf);
			if (c) su.team[i] = (su.team[i] - 1 + c + MAX_PLAYERS) % MAX_PLAYERS + 1;
			rx2 -= 6;
		}
		if (i > 0) {
			rx2 -= 112;
			int c = cycler(rx2, y + (rowh - 4 - 28) / 2, 112, diff_names[su.diff[i]]);
			if (c) su.diff[i] = (su.diff[i] + c + 3) % 3;
		}
		y += rowh;
	}

	/* opciones */
	int ry = py + 40, c;
	const int lbl = 110;
	gfx_text(rx, ry + 7, "Mapa:", COL_CREAM, 2);
	if ((c = cycler(rx + lbl, ry, rw - lbl, map_names[su.map]))) su.map = (su.map + c + 6) % 6;
	ry += 40;
	gfx_text(rx, ry + 7, "Tamaño:", COL_CREAM, 2);
	if ((c = cycler(rx + lbl, ry, rw - lbl, size_names[su.size]))) su.size = (su.size + c + 9) % 9;
	ry += 40;
	gfx_text(rx, ry + 7, "Equipos:", COL_CREAM, 2);
	if ((c = cycler(rx + lbl, ry, rw - lbl, team_mode_names[su.team_mode]))) su.team_mode = (su.team_mode + c + 3) % 3;
	ry += 40;

	gfx_text(rx, ry + 7, "Semilla:", COL_CREAM, 2);
	int sw = rw - lbl - 92;
	gfx_fill(rx + lbl, ry, sw, 28, COL_PANEL2);
	gfx_rect(rx + lbl, ry, sw, 28, su.seed_focus ? COL_CREAM : COL_GOLD);
	snprintf(buf, sizeof buf, "%d%s", su.seed, su.seed_focus && ((SDL_GetTicks() / 400) & 1) ? "_" : "");
	gfx_text(rx + lbl + 8, ry + 7, buf, COL_CREAM, 2);
	if (mclick && hit(rx + lbl, ry, sw, 28)) su.seed_focus = 1;
	else if (mclick) su.seed_focus = 0;
	if (button(rx + lbl + sw + 6, ry, 86, 28, "Nueva", 1, 2)) new_seed();
	ry += 44;

	gfx_text(rx, ry + 7, "Pob. máx.:", COL_CREAM, 2);
	snprintf(buf, sizeof buf, "%d", pop_values[su.popcap]);
	if ((c = cycler(rx + lbl, ry, 140, buf))) su.popcap = (su.popcap + c + 6) % 6;
	ry += 40;

	checkbox(rx, ry, "Recursos estándar", &su.std_res);
	ry += 34;
	if (!su.std_res) {
		static const char *rn[] = { "Madera", "Comida", "Oro", "Piedra" };
		for (int k = 0; k < RES_COUNT; ++k) {
			snprintf(buf, sizeof buf, "%d", res_values[su.res[k]]);
			gfx_text(rx, ry + 7, rn[k], COL_CREAM, 2);
			if ((c = cycler(rx + lbl, ry, 140, buf))) su.res[k] = (su.res[k] + c + 5) % 5;
			ry += 32;
		}
	}

	if (button(px + (pw - 320) / 2, py + ph - 70, 320, 52, "EMPEZAR PARTIDA", su.players >= 1, 3)) start_game();
}

static int opt_page;   /* 0 gráficos, 1 sonido y control */

/* cambia un efecto suelto: el perfil pasa a «personalizado» y se guarda */
static void fx_toggle_changed(void) {
	efx.perfil = FX_PERSONAL;
	gfx_smooth = efx.suave;
	fx_guardar();
}

static void draw_options(void) {
	draw_background(0);
	gfx_fill_a(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0), 150);
	int c;
	int pw = MIN(fb.w - 20, 620), ph = MIN(fb.h - 20, 520), px = (fb.w - pw) / 2, py = (fb.h - ph) / 2;
	panel(px, py, pw, ph);
	gfx_text_c(fb.w / 2, py + 16, "OPCIONES", COL_GOLD, 3);
	if (button(px + 24, py + 50, pw / 2 - 30, 30, "Gráficos", opt_page != 0, 2)) opt_page = 0;
	if (button(px + pw / 2 + 6, py + 50, pw / 2 - 30, 30, "Sonido y control", opt_page != 1, 2)) opt_page = 1;

	int y = py + 96;
	if (opt_page == 0) {
		int fs = fullscreen;
		if (checkbox(px + 30, y, "Pantalla completa", &fs)) {
			fullscreen = fs;
			cfg_set_int("fullscreen", fs);
			apply_fullscreen();
		}
		checkbox(px + pw / 2, y, "Mostrar FPS", &show_fps);
		y += 34;
		if (tex_available()) {
			int hd = hd_enabled;
			if (checkbox(px + 30, y, "Terreno con texturas HD", &hd)) { hd_enabled = hd; cfg_set_int("hd_terrain", hd); }
		} else {
			gfx_text(px + 30, y + 4, "Texturas HD no encontradas (aoen-config --textures RUTA)", COL_DIM, 1);
		}
		y += 36;

		gfx_text(px + 30, y + 7, "Efectos gráficos:", COL_GOLD, 2);
		if ((c = cycler(px + pw - 250, y, 220, fx_nombre_perfil(efx.perfil)))) {
			int p = (efx.perfil + c + 4) % 4;
			if (p == FX_PERSONAL) { efx.perfil = FX_PERSONAL; fx_guardar(); }
			else { fx_perfil(p); gfx_smooth = efx.suave; fx_guardar(); }
		}
		y += 40;
		/* cada efecto por separado, en dos columnas */
		struct { const char *n; int *v; } L[] = {
			{ "Transiciones", &efx.transiciones }, { "Agua animada", &efx.agua },
			{ "Niebla suave", &efx.niebla }, { "Sombras", &efx.sombras },
			{ "Humo y polvo", &efx.particulas }, { "Obras y ruinas", &efx.construccion },
			{ "Día y noche", &efx.diaynoche }, { "Zoom suave", &efx.suave },
		};
		for (int k = 0; k < 8; ++k) {
			int cx = px + 30 + (k & 1) * (pw / 2 - 10), cy = y + (k >> 1) * 32;
			if (checkbox(cx, cy, L[k].n, L[k].v)) fx_toggle_changed();
		}
		y += 4 * 32 + 8;
		gfx_text(px + 30, y + 7, "Tamaño de la interfaz:", COL_CREAM, 2);
		static const char *en[] = { "Automático", "Normal (1x)", "Grande (2x)" };
		if ((c = cycler(px + pw - 250, y, 220, en[CLAMP(efx.escala, 0, 2)]))) { efx.escala = (efx.escala + c + 3) % 3; fx_guardar(); }
		y += 36;
		{
			int r2 = cfg_get_int("render2d", 0);
			if (checkbox(px + 30, y, "Renderizador 2D del sistema (al reiniciar)", &r2)) cfg_set_int("render2d", r2);
		}
		y += 30;
		gfx_text(px + 30, y, "Equipos lentos: sin efectos, como el AoEN original. Normal: los baratos. Completo: todo.", COL_DIM, 1);
	} else {
		static const char *vn[] = { "0%", "25%", "50%", "75%", "100%" };
		int vs = CLAMP((int)(audio_sfx_volume() * 4 + 0.5f), 0, 4), va = CLAMP((int)(audio_ambience_volume() * 4 + 0.5f), 0, 4);
		gfx_text(px + 30, y + 7, "Volumen de efectos:", COL_CREAM, 2);
		if ((c = cycler(px + pw - 210, y, 170, vn[vs]))) { vs = (vs + c + 5) % 5; audio_set_volumes(vs / 4.0f, audio_ambience_volume()); cfg_set_int("volume_sfx", vs * 25); snd_ui(SND_CLICK); }
		y += 40;
		int vm = CLAMP((int)(music_volume() * 4 + 0.5f), 0, 4);
		gfx_text(px + 30, y + 7, "Volumen de música:", COL_CREAM, 2);
		if ((c = cycler(px + pw - 210, y, 170, vn[vm]))) { vm = (vm + c + 5) % 5; music_set_volume(vm / 4.0f); cfg_set_int("volume_music", vm * 25); }
		y += 40;
		gfx_text(px + 30, y + 7, "Volumen de ambiente:", COL_CREAM, 2);
		if ((c = cycler(px + pw - 210, y, 170, vn[va]))) { va = (va + c + 5) % 5; audio_set_volumes(audio_sfx_volume(), va / 4.0f); cfg_set_int("volume_amb", va * 25); }
		y += 46;
		static const char *sp[] = { "Lenta", "Normal", "Rápida" };
		gfx_text(px + 30, y + 7, "Velocidad de desplazamiento:", COL_CREAM, 2);
		c = cycler(px + pw - 210, y, 170, sp[scroll_speed - 1]);
		if (c) { scroll_speed = (scroll_speed - 1 + c + 3) % 3 + 1; cfg_set_int("scroll_speed", scroll_speed); }
		y += 56;
		gfx_text(px + 30, y, "Carpeta de datos (se cambia con aoen-config):", COL_DIM, 1);
		gfx_text(px + 30, y + 14, data_dir, COL_DIM, 1);
		{ char cp[1100]; cfg_path(cp, sizeof cp); gfx_text(px + 30, y + 28, cp, COL_DIM, 1); }
	}

	if (button(px + (pw - 200) / 2, py + ph - 52, 200, 38, "Volver", 1, 2)) screen = options_back;
}

static void draw_menu(void) {
	draw_background(0);
	int bw = 320, bh = 52, x = (fb.w - bw) / 2, y = fb.h * 46 / 100;
	if (button(x, y, bw, bh, "Un Jugador", 1, 3)) screen = SC_SETUP;
	static int has_saves = -1;
	if (has_saves < 0 || (SDL_GetTicks() / 1000) % 2 == 0) has_saves = any_save();
	if (button(x, y + 68, bw, bh, "Cargar partida", has_saves, 3)) { refresh_slots(); saves_msg[0] = 0; saves_mode = 0; saves_back = SC_MENU; screen = SC_SAVES; }
	if (!has_saves) gfx_text_c(fb.w / 2, y + 68 + bh + 6, "Aún no hay partidas guardadas: guarda con F5 o desde la pausa", gfx_rgb(220, 210, 190), 1);
	if (button(x, y + 136, bw, bh, "Opciones", 1, 3)) { options_back = SC_MENU; screen = SC_OPTIONS; }
	if (button(x, y + 204, bw, bh, "Salir", 1, 3)) running = 0;
	gfx_text_c(fb.w / 2, fb.h - 20, "AoEN - motor nuevo en C. Age of Empires (c) Microsoft. Se necesita una copia legitima del juego.", gfx_rgb(220, 210, 190), 1);
}

/* ---------- dentro de la partida ---------- */

static EntType placing = E_TYPE_COUNT;
static int selecting, sel_x0, sel_y0;

static int first_selected(void) {
	for (int i = 0; i < W.nents; ++i)
		if (W.ents[i].alive && W.ents[i].sel) return i;
	return -1;
}

static void select_none(void) {
	for (int i = 0; i < W.nents; ++i) W.ents[i].sel = 0;
}

static int pick_entity(float wx, float wy) {
	int best = -1; float bd = 1e9f;
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!e->alive || e->state == ST_DYING) continue;
		int fx = CLAMP((int)floorf(e->x), 0, W.w - 1), fy = CLAMP((int)floorf(e->y), 0, W.h - 1);
		u8 f = W.fog[fy * W.w + fx];
		if (!f || (is_unit(e->type) && !same_team(e->owner, W.human) && f < 2)) continue;

		float d;
		if (is_unit(e->type)) {
			float dx = e->x - wx, dy = e->y - (wy + 0.3f);
			d = sqrtf(dx * dx + dy * dy);
			if (d > 0.7f) continue;
			d -= 10; /* las unidades tienen prioridad sobre los edificios */
		} else {
			if (wx < e->bx || wx >= e->bx + e->size || wy < e->by || wy >= e->by + e->size) continue;
			d = 0;
		}
		if (d < bd) { bd = d; best = i; }
	}
	return best;
}

static void select_box(int x0, int y0, int x1, int y1) {
	if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
	if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }

	select_none();
	int any = 0;
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!e->alive || e->owner != W.human || !is_unit(e->type) || e->state == ST_DYING) continue;
		int sx, sy;
		view_to_screen(e->x, e->y, &sx, &sy);
		sy -= (int)(14 * V.zoom);
		if (sx >= x0 && sx <= x1 && sy >= y0 && sy <= y1) { e->sel = 1; any = 1; }
	}
	(void)any;
}

static void center_on_idle(void) {
	static int last = -1;
	for (int k = 1; k <= W.nents; ++k) {
		int i = (last + k) % W.nents;
		Entity *e = &W.ents[i];
		if (e->alive && e->owner == W.human && e->type == E_VILLAGER && e->state == ST_IDLE) {
			last = i;
			select_none();
			e->sel = 1;
			view_center_on(e->x, e->y);
			return;
		}
	}
	world_msg("No hay aldeanos inactivos");
}

typedef struct { const char *label; EntType t; int icon; int key; } Cmd;

static void draw_cost(int x, int y, int wood, int food, int gold, int stone) {
	char b[64]; b[0] = 0;
	char t[16];
	if (wood) { snprintf(t, sizeof t, "%dM ", wood); strcat(b, t); }
	if (food) { snprintf(t, sizeof t, "%dC ", food); strcat(b, t); }
	if (gold) { snprintf(t, sizeof t, "%dO ", gold); strcat(b, t); }
	if (stone) { snprintf(t, sizeof t, "%dP ", stone); strcat(b, t); }
	gfx_text(x, y, b, COL_CREAM, 1);
}

static int mw0_x(void) { return lw() - MIN(240, lw() / 3) - 14; }

static void hud_command_button(int x, int y, int sz, const char *hotkey, Sprite *icon, const char *name, int wood, int food, int gold, int stone, int enabled, int *clicked, int *hover) {
	int over = hit(x, y, sz, sz);
	gfx_fill(x, y, sz, sz, over && enabled ? gfx_rgb(80, 50, 30) : COL_PANEL2);
	gfx_rect(x, y, sz, sz, enabled ? COL_GOLD : gfx_rgb(90, 75, 55));
	if (icon) {
		gfx_clip(x + 2, y + 2, x + sz - 2, y + sz - 2);
		gfx_sprite(icon, x + sz / 2 - icon->w / 2 + icon->hx, y + sz / 2 - icon->h / 2 + icon->hy, 256, W.human, enabled ? 256 : 110, 0);
		gfx_clip_reset();
	}
	gfx_text(x + 3, y + 3, hotkey, COL_GOLD, 1);
	if (over) {
		*hover = 1;
		int tw = MAX(gfx_text_w(name, 2), 120) + 16;
		int tx = MIN(x, fb.w - tw - 4), ty = y - 58;
		gfx_fill_a(tx, ty, tw, 52, gfx_rgb(15, 10, 8), 240);
		gfx_rect(tx, ty, tw, 52, COL_GOLD);
		gfx_text(tx + 8, ty + 8, name, COL_CREAM, 2);
		draw_cost(tx + 8, ty + 32, wood, food, gold, stone);
	}
	if (enabled && over && mclick) *clicked = 1;
}

static void draw_techs(Entity *b, int bi, int x0, int y0, int *hover) {
	Player *pl = &W.pl[W.human];
	int k = 0, w = 112, h = 22, cols = MAX(1, (mw0_x() - 12 - x0) / (w + 6));
	int any = 0;
	for (int t = 0; t < T_COUNT; ++t) if (tech_def[t].bld == b->type) any = 1;
	if (!any) return;

	gfx_text(x0, y0 - 12, "Mejoras", COL_GOLD, 1);

	for (int t = 0; t < T_COUNT; ++t) {
		const TechDef *td = &tech_def[t];
		if (td->bld != b->type) continue;
		int x = x0 + (k % cols) * (w + 6), y = y0 + (k / cols) * (h + 5);
		int done = has_tech(W.human, t), busy = b->tech_cur == t + 1;
		int locked = pl->age < td->age || (td->prereq >= 0 && !has_tech(W.human, td->prereq));
		int ok = !done && !busy && !b->tech_cur && !locked && pl->res[RES_WOOD] >= td->wood && pl->res[RES_FOOD] >= td->food && pl->res[RES_GOLD] >= td->gold && pl->res[RES_STONE] >= td->stone;

		u32 bg = done ? gfx_rgb(40, 80, 40) : busy ? gfx_rgb(90, 70, 20) : ok ? COL_RED : gfx_rgb(60, 45, 38);
		int over = hit(x, y, w, h);
		gfx_fill(x, y, w, h, over && ok ? COL_REDH : bg);
		gfx_rect(x, y, w, h, done ? gfx_rgb(120, 220, 120) : COL_GOLD);
		char nm[40];
		snprintf(nm, sizeof nm, "%s", td->name);
		gfx_text(x + 4, y + 7, nm, locked && !done ? gfx_rgb(150, 135, 110) : COL_CREAM, 1);
		if (busy) {
			gfx_fill(x, y + h - 3, w, 3, gfx_rgb(20, 20, 20));
			gfx_fill(x + 1, y + h - 3, (int)((w - 2) * b->tech_t / td->time), 2, gfx_rgb(240, 200, 60));
		}
		if (over) {
			*hover = 1;
			int tw = MAX(gfx_text_w(td->desc, 1), 150) + 16, tx = MIN(x, fb.w - tw - 4), ty = y - 66;
			gfx_fill_a(tx, ty, tw, 60, gfx_rgb(15, 10, 8), 240);
			gfx_rect(tx, ty, tw, 60, COL_GOLD);
			gfx_text(tx + 8, ty + 8, td->name, COL_CREAM, 2);
			gfx_text(tx + 8, ty + 28, td->desc, COL_CREAM, 1);
			draw_cost(tx + 8, ty + 44, td->wood, td->food, td->gold, td->stone);
			if (mclick) {
				if (done) world_msg("Ya investigado");
				else if (locked) world_msg(pl->age < td->age ? "Necesitas una edad superior" : "Necesitas investigar antes otra mejora");
				else cmd_research(W.human, ent_ref(bi), t);
			}
		}
		++k;
	}
}

static void draw_hud(void) {
	int sw = fb.w, sh = fb.h;
	Player *pl = &W.pl[W.human];

	/* barra superior */
	gfx_fill_grad(0, 0, sw, top_h, gfx_rgb(48, 32, 22), gfx_rgb(20, 13, 9));
	gfx_fill(0, top_h - 1, sw, 1, COL_GOLD);
	gfx_fill(0, top_h, sw, 2, gfx_rgb(12, 8, 6));
	static const u32 rc[4] = { 0xff8b5a2b, 0xffd04040, 0xffe8c030, 0xff9a9a9a };
	static const char *rn[4] = { "Madera", "Comida", "Oro", "Piedra" };
	int x = 12;
	for (int k = 0; k < RES_COUNT; ++k) {
		gfx_fill(x, 7, 16, 16, rc[k]);
		gfx_rect(x, 7, 16, 16, COL_GOLD);
		char b[32];
		snprintf(b, sizeof b, "%d", pl->res[k]);
		gfx_text(x + 22, 8, b, COL_CREAM, 2);
		if (hit(x, 7, 90, 16)) gfx_text(x, top_h + 4, rn[k], COL_CREAM, 1);
		x += 98;
	}
	char b[64];
	snprintf(b, sizeof b, "Pob. %d/%d", pl->pop, pl->popcap);
	gfx_text(x + 10, 8, b, pl->pop >= pl->popcap ? gfx_rgb(255, 120, 100) : COL_CREAM, 2);
	int t = (int)W.time;
	char tb[32];
	snprintf(tb, sizeof tb, "%02d:%02d  x%d", t / 60, t % 60, game_speed);
	int tx_time = sw - 110 - gfx_text_w(tb, 2);
	int age_sz = x + 160 + gfx_text_w(age_def[pl->age].name, 2) > tx_time - 8 ? 1 : 2;   /* en ventanas estrechas el nombre de la edad no pisa al reloj */
	gfx_text(x + 160, age_sz == 2 ? 8 : 11, age_def[pl->age].name, COL_GOLD, age_sz);
	gfx_text(tx_time, 8, tb, COL_CREAM, 2);
	if (show_fps) { snprintf(b, sizeof b, "%d fps", (int)(1000.0f / MAX(1, (int)SDL_GetTicks() % 1000 + 1))); }
	if (button(sw - 92, 3, 84, 24, "Menú", 1, 2)) paused_menu = 1;

	/* barra inferior */
	int by = sh - bottom_h;
	gfx_fill_grad(0, by, sw, bottom_h, gfx_rgb(40, 27, 19), gfx_rgb(16, 10, 7));
	gfx_fill(0, by, sw, 1, COL_GOLD);
	gfx_fill(0, by + 1, sw, 1, gfx_rgb(130, 100, 56));
	gfx_fill(0, by - 2, sw, 2, gfx_rgb(12, 8, 6));

	/* minimapa */
	int mx0, my0, mw, mh;
	minimap_rect(&mx0, &my0, &mw, &mh);
	render_minimap(mx0, my0, mw, mh);
	if (mdown || mclick) {
		float wx, wy;
		if (hit(mx0, my0, mw, mh) && minimap_pick(mx0, my0, mw, mh, mx / ui_s, my / ui_s, &wx, &wy)) view_center_on(wx, wy);
	}

	/* información de la selección */
	int sel = first_selected();
	int nsel = 0;
	for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel) ++nsel;

	panel(8, by + 8, 250, bottom_h - 16);
	if (sel >= 0) {
		Entity *e = &W.ents[sel];
		const char *name = is_building(e->type) ? bld_def[e->type].name : is_unit(e->type) ? unit_def[e->type - E_VILLAGER].name : "Recurso";
		gfx_text(20, by + 18, name, COL_GOLD, 2);
		if (nsel > 1) { snprintf(b, sizeof b, "x%d seleccionados", nsel); gfx_text(20, by + 40, b, COL_CREAM, 1); }
		if (e->owner >= 0) { gfx_fill(238, by + 16, 12, 12, col_player[e->owner]); if (nsel <= 1) gfx_text(20, by + 40, civ_def[W.pl[e->owner].civ].name, COL_DIM, 1); }

		/* barra de vida */
		float f = e->hp / e->maxhp;
		gfx_fill(20, by + 56, 150, 10, gfx_rgb(20, 20, 20));
		gfx_fill(21, by + 57, (int)(148 * CLAMP(f, 0, 1)), 8, f > 0.5f ? gfx_rgb(60, 200, 60) : f > 0.25f ? gfx_rgb(230, 200, 40) : gfx_rgb(220, 50, 50));
		snprintf(b, sizeof b, "%d/%d", (int)ceilf(e->hp), (int)e->maxhp);
		gfx_text(176, by + 56, b, COL_CREAM, 1);

		if (is_unit(e->type)) {
			const UnitDef *u = &unit_def[e->type - E_VILLAGER];
			snprintf(b, sizeof b, "Ataque %d  Edif. %d  Alcance %.0f", (int)u->dmg, (int)u->dmg_bld, u->range);
			gfx_text(20, by + 76, b, COL_CREAM, 1);
			static const char *kn[] = { "madera", "comida", "oro", "piedra", "una reliquia" };
			if (e->carry > 0) { snprintf(b, sizeof b, "Lleva %d de %s", e->carry, kn[e->carry_kind]); gfx_text(20, by + 92, b, gfx_rgb(255, 220, 120), 1); }
			snprintf(b, sizeof b, "Armadura %d/%d  Vel. %.1f", u->armor, u->parmor, u->speed);
			gfx_text(20, by + 108, b, COL_DIM, 1);
		} else if (is_resource(e->type)) {
			snprintf(b, sizeof b, "Quedan %d", (int)e->amount);
			gfx_text(20, by + 76, b, COL_CREAM, 1);
		} else if (e->progress < 1) {
			snprintf(b, sizeof b, "Construyendo %d%%", (int)(e->progress * 100));
			gfx_text(20, by + 76, b, COL_CREAM, 1);
		} else if (e->ntrain > 0) {
			snprintf(b, sizeof b, "Entrenando %d%%  cola %d", (int)(100 * e->train_time / unit_def[e->train[0] - E_VILLAGER].train_time), e->ntrain);
			gfx_text(20, by + 76, b, COL_CREAM, 1);
		}
	} else {
		gfx_text(20, by + 20, "Nada seleccionado", COL_DIM, 2);
		gfx_text(20, by + 50, "Clic: seleccionar", COL_DIM, 1);
		gfx_text(20, by + 64, "Arrastrar: caja", COL_DIM, 1);
		gfx_text(20, by + 78, "Clic derecho: ordenar", COL_DIM, 1);
		gfx_text(20, by + 92, "Rueda: zoom  H: casa", COL_DIM, 1);
	}

	/* órdenes */
	int cx = 270, cy = by + 18, sz = 44, gap = 6, hover = 0, clicked;
	int avail = (mw0_x() - 12 - cx);
	int cols = MAX(3, avail / (sz + gap));
	Entity *e = sel >= 0 ? &W.ents[sel] : NULL;
	int age = pl->age;

	int military_sel = e && e->owner == W.human && is_unit(e->type) && e->type != E_VILLAGER && e->type < E_HORSE;
	if (military_sel) {
		static const char *st_names[] = { "Agresiva", "Defensiva", "Pasiva", "Mantener", "Proteger" };
		static const char *st_tips[] = { "Persigue a los enemigos que ve", "Ataca cerca y vuelve a su puesto", "No ataca por su cuenta", "Dispara sin moverse del sitio", "Sigue y defiende a un aliado (clic derecho)" };
		gfx_text(cx, by + 5, "Actitud", COL_GOLD, 1);
		for (int k = 0; k < 5; ++k) {
			int bx = cx + k * 86, by2 = cy, active = e->stance == k || (k == STANCE_GUARD && guard_mode);
			int over = hit(bx, by2, 80, 26);
			gfx_fill(bx, by2, 80, 26, active ? gfx_rgb(110, 70, 25) : over ? gfx_rgb(80, 50, 30) : COL_PANEL2);
			gfx_rect(bx, by2, 80, 26, active ? COL_CREAM : COL_GOLD);
			gfx_text_c(bx + 40, by2 + 9, st_names[k], COL_CREAM, 1);
			if (over) {
				hover = 1;
				gfx_text(cx, cy + 34, st_tips[k], COL_DIM, 1);
				if (mclick) {
					snd_ui(SND_CLICK);
					if (k == STANCE_GUARD) { guard_mode = 1; world_msg("Clic derecho en el aliado que debe proteger"); }
					else {
						guard_mode = 0;
						for (int i = 0; i < W.nents; ++i)
							if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human) cmd_set_stance(ent_ref(i), k);
					}
				}
			}
		}
		if (e->type == E_PRIEST) gfx_text(cx, cy + 50, "Clic derecho en una reliquia para recogerla; llévala al templo para ganar oro", COL_DIM, 1);
	} else if (e && e->owner == W.human && e->type == E_VILLAGER) {
		static const struct { EntType t; int icon; } bl[] = {
			{ E_HOUSE, 15 }, { E_BARRACKS, 3 }, { E_GRANARY, 11 }, { E_STORAGE, 28 }, { E_FARM, 10 }, { E_TOWER, 26 },
			{ E_ARCHERY, 1 }, { E_STABLE, 25 }, { E_TEMPLE, 31 }, { E_ACADEMY, 0 }, { E_GOVERNMENT, 19 }, { E_MARKET, 21 }, { E_SIEGE, 9 }, { E_WONDER, 27 },
		};
		gfx_text(cx, by + 5, "Construir", COL_GOLD, 1);
		for (int k = 0; k < ARRAY_LEN(bl); ++k) {
			const BldDef *d = &bld_def[bl[k].t];
			int ok = age >= d->age && pl->res[RES_WOOD] >= d->wood && pl->res[RES_STONE] >= d->stone && pl->res[RES_GOLD] >= d->gold;
			char name[96];
			if (age < d->age) snprintf(name, sizeof name, "%s (%s)", d->name, age_def[d->age].name);
			else snprintf(name, sizeof name, "%s", d->name);
			char hk[4] = { 0 };
			if (k < 9) { hk[0] = (char)('1' + k); }
			clicked = 0;
			hud_command_button(cx + (k % cols) * (sz + gap), cy + (k / cols) * (sz + gap), sz, hk, icon_bld(bl[k].icon), name, d->wood, d->food, d->gold, d->stone, ok, &clicked, &hover);
			if (clicked || (k < 9 && (key_pressed[SDL_SCANCODE_1 + k] && !mod_down())) ) { if (ok) placing = bl[k].t; else if (age < d->age) world_msg("Necesitas una edad superior"); else world_msg("No tienes suficientes recursos"); }
		}
	} else if (e && e->owner == W.human && e->progress >= 1 && ent_is_trainer(e->type)) {
		static const struct { EntType t; int icon; } ul[] = {
			{ E_VILLAGER, 0 }, { E_CLUBMAN, 2 }, { E_AXEMAN, 3 }, { E_SHORTSWORD, 4 }, { E_BOWMAN, 6 }, { E_IMPBOW, 7 }, { E_CHARIOT, 10 },
			{ E_SCOUT, 31 }, { E_CAVALRY, 11 }, { E_HEAVYCAV, 12 }, { E_HOPLITE, 16 }, { E_PRIEST, 1 },
		};
		gfx_text(cx, by + 5, "Entrenar", COL_GOLD, 1);
		int k = 0;
		for (int i = 0; i < ARRAY_LEN(ul); ++i) {
			const UnitDef *u = &unit_def[ul[i].t - E_VILLAGER];
			if (u->trainer != e->type) continue;
			int ok = age >= u->age && pl->res[RES_WOOD] >= u->wood && pl->res[RES_FOOD] >= u->food && pl->res[RES_GOLD] >= u->gold;
			char name[96];
			if (age < u->age) snprintf(name, sizeof name, "%s (%s)", u->name, age_def[u->age].name);
			else snprintf(name, sizeof name, "%s", u->name);
			char hk[4] = { 0 };
			if (k < 9) hk[0] = (char)('1' + k);
			clicked = 0;
			hud_command_button(cx + k * (sz + gap), cy, sz, hk, icon_unit(ul[i].icon), name, u->wood, u->food, u->gold, u->stone, ok, &clicked, &hover);
			if (clicked || (k < 9 && (key_pressed[SDL_SCANCODE_1 + k] && !mod_down()))) {
				if (age < u->age) world_msg("Necesitas una edad superior");
				else cmd_train(W.human, ent_ref(sel), ul[i].t);
			}
			++k;
		}

		/* cola */
		for (int q = 0; q < e->ntrain; ++q) {
			int qx = cx + q * 30, qy = cy + sz + 10;
			gfx_fill(qx, qy, 26, 26, COL_PANEL2);
			gfx_rect(qx, qy, 26, 26, COL_GOLD);
			if (q == 0) gfx_fill(qx + 1, qy + 22, (int)(24 * e->train_time / unit_def[e->train[0] - E_VILLAGER].train_time), 3, gfx_rgb(240, 200, 60));
			char l[2] = { unit_def[e->train[q] - E_VILLAGER].name[0], 0 };
			gfx_text(qx + 9, qy + 9, l, COL_CREAM, 1);
			if (hit(qx, qy, 26, 26)) {
				gfx_text(qx, qy + 30, "Clic: cancelar", COL_DIM, 1);
				if (mclick) { cmd_cancel_train(W.human, ent_ref(sel), q); break; }
			}
		}

		/* las tecnologías de este edificio se muestran debajo de la cola */
		/* (ver draw_techs) */

		/* avanza a la siguiente edad en el centro urbano */
		if (e->type == E_TOWN_CENTER && age + 1 < AGE_COUNT) {
			int tx = cx, ty = cy + sz + 46;
			const AgeDef *ad = &age_def[age + 1];
			int met = age_requirements_met(W.human, age + 1);
			int ok = met && pl->res[RES_FOOD] >= ad->food && pl->res[RES_GOLD] >= ad->gold && pl->age_t < 0;
			char lbl[96];
			if (pl->age_t >= 0) snprintf(lbl, sizeof lbl, "Investigando %s...", ad->name);
			else snprintf(lbl, sizeof lbl, "Avanzar: %s", ad->name);
			int bw = MIN(260, gfx_text_w(lbl, 1) + 20);
			if (button(tx, ty, bw, 22, lbl, ok, 1)) cmd_advance_age(W.human);
			if (pl->age_t >= 0) {
				gfx_fill(tx, ty + 24, bw, 4, gfx_rgb(20, 20, 20));
				gfx_fill(tx + 1, ty + 25, (int)((bw - 2) * pl->age_t / ad->time), 2, gfx_rgb(240, 200, 60));
			} else {
				char cs[96];
				snprintf(cs, sizeof cs, "%dC %dO %s", ad->food, ad->gold, met ? "" : "(faltan 2 edificios)");
				gfx_text(tx + bw + 8, ty + 7, cs, met ? COL_CREAM : gfx_rgb(255, 140, 120), 1);
			}
		}
	}

	if (e && e->owner == W.human && e->progress >= 1) draw_techs(e, sel, ent_is_trainer(e->type) ? cx + 190 : cx, by + 30, &hover);

	/* marcador de todos los jugadores */
	if (show_score) {
		int ids[MAX_PLAYERS], n = 0, scores[MAX_PLAYERS];
		for (int p = 0; p < MAX_PLAYERS; ++p) {
			if (!W.pl[p].active) continue;
			int ec, mi, te;
			world_score(p, &ec, &mi, &te);
			scores[p] = ec + mi + te;
			ids[n++] = p;
		}
		for (int a = 0; a < n; ++a) for (int b = a + 1; b < n; ++b) if (scores[ids[b]] > scores[ids[a]]) { int t = ids[a]; ids[a] = ids[b]; ids[b] = t; }
		int pw = 250, ph = 22 + n * 17, px = sw - pw - 8, py = top_h + 6;
		gfx_fill_a(px, py, pw, ph, gfx_rgb(20, 13, 9), 215);
		gfx_rect(px, py, pw, ph, COL_GOLD);
		gfx_text(px + 6, py + 5, "Puntuación (Tab)", COL_GOLD, 1);
		static const char *an[] = { "Piedra", "Herram.", "Bronce", "Hierro" };
		for (int i = 0; i < n; ++i) {
			int p = ids[i], ec, mi, te;
			world_score(p, &ec, &mi, &te);
			int yy = py + 20 + i * 17;
			gfx_fill(px + 6, yy, 10, 10, col_player[p]);
			char row[80];
			snprintf(row, sizeof row, "%s %-7s %3d/%3d  %5d", p == W.human ? "Tú" : "IA", an[W.pl[p].age], W.pl[p].pop, W.pl[p].popcap, scores[p]);
			gfx_text(px + 22, yy + 1, row, W.pl[p].alive || W.over ? COL_CREAM : gfx_rgb(120, 100, 90), 1);
			if (hit(px, yy - 2, pw, 16)) {
				char tip[96];
				snprintf(tip, sizeof tip, "Economía %d  Militar %d  Tecnología %d  Bajas %d", ec, mi, te, W.pl[p].units_killed);
				gfx_text(MAX(4, px - gfx_text_w(tip, 1) - 8), yy + 1, tip, COL_CREAM, 1);
			}
		}
	}

	/* mercado: cambio de recursos */
	if (e && e->owner == W.human && e->type == E_MARKET && e->progress >= 1) {
		static const char *nm[] = { "madera", "comida", "oro", "piedra" };
		gfx_text(cx, by + 5, "Mercado: 100 unidades por operación", COL_GOLD, 1);
		int k = 0;
		for (int r = 0; r < 4; ++r) {
			if (r == RES_GOLD) continue;
			char lbl[64];
			int gold = 100 * pl->price[r] / 100 - 12;
			snprintf(lbl, sizeof lbl, "Vender %s (+%d oro)", nm[r], gold);
			if (button(cx, cy + k * 28, 190, 24, lbl, pl->res[r] >= 100, 1)) cmd_trade(W.human, r, RES_GOLD);
			snprintf(lbl, sizeof lbl, "Comprar %s (-%d oro)", nm[r], 100 * pl->price[r] / 100 + 12);
			if (button(cx + 196, cy + k * 28, 200, 24, lbl, pl->res[RES_GOLD] >= 100 * pl->price[r] / 100 + 12, 1)) cmd_trade(W.human, RES_GOLD, r);
			++k;
		}
	}

	/* cuenta atrás de la maravilla */
	for (int i = 0; i < W.nents; ++i) {
		Entity *w = &W.ents[i];
		if (!w->alive || w->type != E_WONDER || w->progress < 1) continue;
		char wb[80];
		int left = MAX(0, 600 - (int)w->timer);
		snprintf(wb, sizeof wb, "Maravilla de %s: %d:%02d", w->owner == W.human ? "tu equipo" : "un rival", left / 60, left % 60);
		gfx_text_c(sw / 2, top_h + 8, wb, w->owner == W.human || same_team(w->owner, W.human) ? gfx_rgb(130, 255, 130) : gfx_rgb(255, 130, 110), 2);
		break;
	}

	/* línea de mensajes */
	if (W.msg_t > 0 && W.msg[0]) gfx_text_c(sw / 2, by - 24, W.msg, gfx_rgb(255, 235, 150), 2);
	(void)hover;
}

static void draw_endscreen(void) {
	gfx_fill_a(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0), 150);
	int pw = 460, ph = 300, px = (fb.w - pw) / 2, py = (fb.h - ph) / 2;
	panel(px, py, pw, ph);
	gfx_text_c(fb.w / 2, py + 26, W.over == 1 ? "VICTORIA" : "DERROTA", W.over == 1 ? gfx_rgb(120, 255, 120) : gfx_rgb(255, 100, 90), 4);
	char b[96];
	int t = (int)W.time;
	snprintf(b, sizeof b, "Tiempo de partida: %02d:%02d", t / 60, t % 60);
	gfx_text_c(fb.w / 2, py + 100, b, COL_CREAM, 2);
	snprintf(b, sizeof b, "Unidades enemigas eliminadas: %d", W.pl[W.human].units_killed);
	gfx_text_c(fb.w / 2, py + 130, b, COL_CREAM, 2);
	snprintf(b, sizeof b, "Edificios destruidos: %d", W.pl[W.human].bld_razed);
	gfx_text_c(fb.w / 2, py + 160, b, COL_CREAM, 2);
	if (button(px + (pw - 260) / 2, py + ph - 70, 260, 48, "Volver al menú", 1, 2)) { world_free(); screen = SC_MENU; }
}

static void draw_pause(void) {
	gfx_fill_a(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0), 140);
	int pw = 360, ph = 450, px = (fb.w - pw) / 2, py = (fb.h - ph) / 2;
	panel(px, py, pw, ph);
	gfx_text_c(fb.w / 2, py + 24, "PAUSA", COL_GOLD, 3);
	if (button(px + 40, py + 80, pw - 80, 46, "Continuar", 1, 2)) paused_menu = 0;
	if (button(px + 40, py + 140, pw - 80, 46, "Guardar partida", 1, 2)) { refresh_slots(); saves_msg[0] = 0; saves_mode = 1; saves_back = SC_GAME; screen = SC_SAVES; }
	if (button(px + 40, py + 200, pw - 80, 46, "Cargar partida", any_save(), 2)) { refresh_slots(); saves_msg[0] = 0; saves_mode = 0; saves_back = SC_GAME; screen = SC_SAVES; }
	if (button(px + 40, py + 260, pw - 80, 46, "Opciones", 1, 2)) { options_back = SC_GAME; screen = SC_OPTIONS; }
	if (button(px + 40, py + 320, pw - 80, 46, "Abandonar partida", 1, 2)) { world_free(); paused_menu = 0; screen = SC_MENU; }
	if (button(px + 40, py + 380, pw - 80, 46, "Salir del juego", 1, 2)) running = 0;
}

static void game_input(float dt) {
	int sw = fb.w, sh = fb.h;
	int in_hud = my < top_h * ui_s || my > sh - bottom_h * ui_s;

	/* cámara */
	float sp = (scroll_speed == 1 ? 450.0f : scroll_speed == 2 ? 700.0f : 1000.0f) * dt / V.zoom;
	int dx = 0, dy = 0;
	if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) --dx;
	if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) ++dx;
	if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) --dy;
	if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) ++dy;
	if (mouse_seen && (SDL_GetWindowFlags(win) & SDL_WINDOW_MOUSE_FOCUS)) {
		if (mx <= 2) --dx;
		else if (mx >= sw - 3) ++dx;
		if (my <= 2) --dy;
		else if (my >= sh - 3) ++dy;
	}
	V.cx += CLAMP(dx, -1, 1) * sp;
	V.cy += CLAMP(dy, -1, 1) * sp;

	/* zoom alrededor del cursor */
	if (wheel && !in_hud) {
		float wx, wy;
		screen_to_world(mx, my, &wx, &wy);
		float nz = CLAMP(V.zoom * powf(1.12f, (float)wheel), 0.4f, 2.5f);
		V.zoom = nz;
		float px, py;
		view_proj(wx, wy, &px, &py);
		V.cx = px - (mx - sw * 0.5f) / V.zoom;
		V.cy = py - (my - sh * 0.5f) / V.zoom;
	}
	view_clamp();

	if (key_pressed[SDL_SCANCODE_H]) {
		for (int i = 0; i < W.nents; ++i)
			if (W.ents[i].alive && W.ents[i].owner == W.human && W.ents[i].type == E_TOWN_CENTER) { view_center_on(W.ents[i].x, W.ents[i].y); break; }
	}
	if (key_pressed[SDL_SCANCODE_PERIOD]) center_on_idle();
	if (key_pressed[SDL_SCANCODE_EQUALS] || key_pressed[SDL_SCANCODE_KP_PLUS]) game_speed = MIN(4, game_speed + 1);
	if (key_pressed[SDL_SCANCODE_MINUS] || key_pressed[SDL_SCANCODE_KP_MINUS]) game_speed = MAX(1, game_speed - 1);
	if (key_pressed[SDL_SCANCODE_DELETE]) {
		for (int i = 0; i < W.nents; ++i)
			if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human && is_unit(W.ents[i].type)) { W.ents[i].hp = 0.01f; W.ents[i].hp = 0; }
	}

	/* grupos de control: Ctrl+número guarda la selección, Mayús+número la recupera */
	{
		static Ref groups[10][96];
		static int gcount[10];
		int ctrl = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL], shift = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
		for (int k = 0; k < 10; ++k) {
			int sc = k == 9 ? SDL_SCANCODE_0 : SDL_SCANCODE_1 + k;
			if (!key_pressed[sc]) continue;
			if (ctrl) {
				gcount[k] = 0;
				for (int i = 0; i < W.nents && gcount[k] < 96; ++i)
					if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human) groups[k][gcount[k]++] = ent_ref(i);
				if (gcount[k]) world_msg("Grupo guardado");
			} else if (shift && gcount[k]) {
				select_none();
				int first = -1;
				for (int j = 0; j < gcount[k]; ++j) {
					Entity *e = ent_get(groups[k][j]);
					if (e && e->state != ST_DYING) { e->sel = 1; if (first < 0) first = groups[k][j].idx; }
				}
				if (first >= 0) view_center_on(W.ents[first].x, W.ents[first].y);
			}
		}
	}

	if (key_pressed[SDL_SCANCODE_TAB]) show_score = !show_score;
	if (key_pressed[SDL_SCANCODE_F5]) do_save(1);
	if (key_pressed[SDL_SCANCODE_F9]) { if (any_save()) do_load(1); }

	if (key_pressed[SDL_SCANCODE_ESCAPE]) {
		if (placing != E_TYPE_COUNT) placing = E_TYPE_COUNT;
		else paused_menu = 1;
	}

	/* botón izquierdo: colocación y selección */
	int over_minimap = mx > sw - 260 * ui_s && my > sh - bottom_h * ui_s;

	if (placing != E_TYPE_COUNT) {
		if (rclick) placing = E_TYPE_COUNT;
		else if (mclick && !in_hud) {
			float wx, wy;
			screen_to_world(mx, my, &wx, &wy);
			int s = bld_def[placing].size;
			int tx = (int)floorf(wx - s / 2.0f + 0.5f), ty = (int)floorf(wy - s / 2.0f + 0.5f);
			int built = 0;
			for (int i = 0; i < W.nents && !built; ++i) {
				Entity *u = &W.ents[i];
				if (u->alive && u->sel && u->type == E_VILLAGER && u->owner == W.human) {
					built = cmd_build(W.human, ent_ref(i), placing, tx, ty);
					if (built) {
						/* los demás aldeanos seleccionados ayudan */
						Ref b = { -1, 0 };
						for (int j = W.nents - 1; j >= 0; --j) if (W.ents[j].alive && W.ents[j].type == placing && W.ents[j].owner == W.human && W.ents[j].bx == tx && W.ents[j].by == ty) { b = ent_ref(j); break; }
						for (int j = 0; j < W.nents; ++j)
							if (j != i && W.ents[j].alive && W.ents[j].sel && W.ents[j].type == E_VILLAGER && b.idx >= 0) cmd_target(ent_ref(j), b);
					}
				}
			}
			if (built) snd_ui(SND_PLACE);
			if (built && !keys[SDL_SCANCODE_LSHIFT]) placing = E_TYPE_COUNT;
		}
		return;
	}

	/* mpressed: con pocos cuadros por segundo el botón puede bajar y subir dentro del mismo cuadro; no se pierde el clic */
	if ((mdown || mpressed) && !selecting && !in_hud && !over_minimap) { selecting = 1; sel_x0 = mpressed ? mdx : mx; sel_y0 = mpressed ? mdy : my; }
	if (!mdown && selecting) {
		selecting = 0;
		if (abs(mx - sel_x0) > 5 || abs(my - sel_y0) > 5) {
			select_box(sel_x0, sel_y0, mx, my);
			{ int ns = 0; for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel) ++ns; AOEN_LOG("[AOEN] selección con caja: %d", ns); }
		} else {
			float wx, wy;
			screen_to_world(mx, my, &wx, &wy);
			int i = pick_entity(wx, wy);
			select_none();
			AOEN_LOG("[AOEN] selección por clic: %s", i >= 0 ? "una entidad" : "nada");
			if (i >= 0) {
				W.ents[i].sel = 1;
				if (W.ents[i].owner == W.human) {
					EntType ty = W.ents[i].type;
					snd_ui(ty == E_VILLAGER ? SND_SEL_VILL : ty == E_TOWN_CENTER ? SND_SEL_TC : is_building(ty) ? SND_SEL_BLD : SND_SEL_MIL);
				}
			}
		}
	}

	/* clic derecho en el minimapa: envía allí las unidades seleccionadas */
	if (rclick) {
		int mx0, my0, mw, mh;
		minimap_rect(&mx0, &my0, &mw, &mh);
		float wx, wy;
		if (hit(mx0, my0, mw, mh) && minimap_pick(mx0, my0, mw, mh, mx / ui_s, my / ui_s, &wx, &wy)) {
			int n = 0;
			for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human && is_unit(W.ents[i].type)) ++n;
			int k = 0, cols = (int)ceilf(sqrtf((float)MAX(n, 1)));
			for (int i = 0; i < W.nents; ++i) {
				Entity *u = &W.ents[i];
				if (!u->alive || !u->sel || u->owner != W.human || !is_unit(u->type)) continue;
				cmd_move(ent_ref(i), wx + (k % cols - cols / 2) * 0.7f, wy + (k / cols - cols / 2) * 0.7f);
				++k;
			}
			if (n) snd_ui(SND_CLICK);
			rclick = 0;
		}
	}

	/* clic derecho con un edificio de producción seleccionado: fija su punto de reunión */
	if (rclick && !in_hud) {
		int sb = first_selected();
		if (sb >= 0 && W.ents[sb].owner == W.human && is_building(W.ents[sb].type) && W.ents[sb].progress >= 1 && ent_is_trainer(W.ents[sb].type)) {
			float wx, wy;
			screen_to_world(mx, my, &wx, &wy);
			int t = pick_entity(wx, wy);
			cmd_set_rally(ent_ref(sb), wx, wy, t >= 0 && t != sb ? ent_ref(t) : (Ref){ -1, 0 });
			world_msg(t >= 0 && t != sb ? "Punto de reunión: las unidades irán a trabajar o a atacar allí" : "Punto de reunión fijado");
			rclick = 0;
		}
	}

	/* protección: el siguiente clic derecho sobre un aliado */
	if (guard_mode && rclick && !in_hud) {
		float wx, wy;
		screen_to_world(mx, my, &wx, &wy);
		int t = pick_entity(wx, wy);
		if (t >= 0 && same_team(W.ents[t].owner, W.human)) {
			for (int i = 0; i < W.nents; ++i)
				if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human && i != t) cmd_guard(ent_ref(i), ent_ref(t));
			world_msg("Protegiendo");
		} else world_msg("Hay que elegir un aliado");
		guard_mode = 0;
		rclick = 0;
	}

	/* botón derecho: órdenes */
	if (rclick && !in_hud) {
		float wx, wy;
		screen_to_world(mx, my, &wx, &wy);
		int t = pick_entity(wx, wy);
		int n = 0;
		for (int i = 0; i < W.nents; ++i)
			if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human && is_unit(W.ents[i].type)) ++n;

		if (n) AOEN_LOG("[AOEN] orden %s: %d unidades hacia %.1f,%.1f", t >= 0 ? "objetivo" : "mover", n, wx, wy);
		int k = 0, cols = (int)ceilf(sqrtf((float)MAX(n, 1)));
		for (int i = 0; i < W.nents; ++i) {
			Entity *u = &W.ents[i];
			if (!u->alive || !u->sel || u->owner != W.human || !is_unit(u->type)) continue;
			if (t >= 0 && t != i) cmd_target(ent_ref(i), ent_ref(t));
			else cmd_move(ent_ref(i), wx + (k % cols - cols / 2) * 0.7f, wy + (k / cols - cols / 2) * 0.7f);
			++k;
		}
		/* edificio seleccionado: el punto de reunión no está implementado, se ignora */
	}
}

/* ---------- bucle principal ---------- */

static void resize_fb(int w, int h) {
	if (fbs) SDL_FreeSurface(fbs);
	fbs = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGB888);   /* sin alfa: las rutas rápidas oscurecen todos los canales */
	fb.px = (u32 *)fbs->pixels;
	fb.pitch = fbs->pitch / 4;
	gfx_set_size(w, h);
	gfx_clip_reset();
	V.sw = w; V.sh = h;
}

int main(int argc, char **argv) {
	const char *shot = getenv("AOEN_SHOT");
	const char *scene = getenv("AOEN_SCENE");
	int shot_frame = getenv("AOEN_FRAMES") ? atoi(getenv("AOEN_FRAMES")) : 20;
	float ffwd = getenv("AOEN_FFWD") ? (float)atof(getenv("AOEN_FFWD")) : 0.0f;

	for (int i = 1; i < argc; ++i)
		if (!strcmp(argv[i], "--data") && i + 1 < argc) snprintf(cmd_data, sizeof cmd_data, "%s", argv[++i]);
		else if (argv[i][0] != '-') snprintf(cmd_data, sizeof cmd_data, "%s", argv[i]);
		else if (!strcmp(argv[i], "--help")) { printf("Uso: aoen [carpeta de datos]\n\n%s", cfg_instructions()); return 0; }

	int have_data = cfg_find_data(cmd_data, data_dir, sizeof data_dir);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
		AOEN_LOG("[AOEN] SDL_Init falló: %s", SDL_GetError());
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}

	int ww = getenv("AOEN_W") ? atoi(getenv("AOEN_W")) : 1024, wh = getenv("AOEN_H") ? atoi(getenv("AOEN_H")) : 720;
#ifdef SAGUI_AOEN
	/* en SaguiOS una ventana normal mide como mucho 1000x700; con F11 (pantalla completa) ocupa todo el escritorio */
	if (ww > 1000) ww = 1000;
	if (wh > 700) wh = 700;
#endif
	win = SDL_CreateWindow("AoEN", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh, SDL_WINDOW_RESIZABLE);
	if (!win) { AOEN_LOG("[AOEN] ventana falló: %s", SDL_GetError()); return 1; }
	win_w = ww; win_h = wh;
	SDL_SetWindowMinimumSize(win, 640, 480);
	int full_pendiente = getenv("AOEN_FULL") || cfg_get_int("fullscreen", 0);   /* se activa en el cuarto cuadro: antes la ventana aún no existe del todo */
	resize_fb(ww, wh);

	AOEN_LOG("[AOEN] inicio: ventana %dx%d, datos %s", ww, wh, have_data ? data_dir : "no encontrados");
	if (!have_data || !assets_load(data_dir)) {
		AOEN_LOG("[AOEN] sin datos del juego: se muestran las instrucciones");
		/* explica lo que hace falta, dentro de la ventana (la versión de Windows no tiene consola) */
		char cp[1100];
		cfg_path(cp, sizeof cp);
		fprintf(stderr, "No se encuentran los datos del juego.\n\n%s", cfg_instructions());
		int wait = !getenv("AOEN_SHOT");
		for (int shown = 0; shown < (wait ? 1 << 30 : 3); ++shown) {
			SDL_Event e;
			while (SDL_PollEvent(&e))
				if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) shown = 1 << 30;
			gfx_fill(0, 0, fb.w, fb.h, gfx_rgb(20, 13, 9));
			panel(30, 30, fb.w - 60, fb.h - 60);
			gfx_text(52, 52, "NO SE ENCUENTRAN LOS DATOS DEL JUEGO", COL_GOLD, 3);
			int y = 110;
			const char *t = cfg_instructions();
			char line[160];
			while (*t) {
				int n = 0;
				while (*t && *t != '\n' && n < 150) line[n++] = *t++;
				line[n] = 0;
				if (*t == '\n') ++t;
				gfx_text(52, y, line, line[0] == ' ' ? COL_CREAM : COL_DIM, gfx_text_w(line, 2) > fb.w - 110 ? 1 : 2);
				y += 20;
			}
			gfx_text(52, fb.h - 90, "Archivo de configuracion:", COL_GOLD, 2);
			gfx_text(52, fb.h - 66, cp, COL_CREAM, 2);
			gfx_text(52, fb.h - 44, "Pulse Esc o cierre la ventana para salir.", COL_DIM, 1);
			SDL_Surface *ws = SDL_GetWindowSurface(win);
			if (ws) { SDL_BlitSurface(fbs, NULL, ws, NULL); SDL_UpdateWindowSurface(win); }
			if (getenv("AOEN_SHOT")) SDL_SaveBMP(fbs, getenv("AOEN_SHOT"));
			SDL_Delay(40);
		}
		return 1;
	}

	{
		char td[1100];
		if (cfg_find_textures(td, sizeof td)) tex_load(td);
		if (getenv("AOEN_CLASSIC")) hd_enabled = 0;
	}
	fx_init();
	{
		int want = getenv("AOEN_RENDER") ? atoi(getenv("AOEN_RENDER")) : cfg_get_int("render2d", 0);
		if (want) {
			rnd = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
			use_renderer = rnd != NULL;
			AOEN_LOG("[AOEN] presentación: %s", use_renderer ? "renderizador 2D de la SDL" : "superficie de la ventana (no hay renderizador)");
		}
	}
	new_seed();
	if (!getenv("AOEN_NOSOUND") && !(scene && (!strcmp(scene, "sheet") || !strcmp(scene, "units") || !strcmp(scene, "match")))) audio_init(data_dir);
	audio_set_volumes(cfg_get_int("volume_sfx", 80) / 100.0f, cfg_get_int("volume_amb", 40) / 100.0f);
	music_set_volume(cfg_get_int("volume_music", 50) / 100.0f);
	music_setup(data_dir);
	scroll_speed = CLAMP(cfg_get_int("scroll_speed", 2), 1, 3);
	{
		int hd = cfg_get_int("hd_terrain", 1);
		if (!hd) hd_enabled = 0;
		/* las texturas HD se calculan píxel a píxel con coma flotante: en «Equipos lentos» se dejan para quien las pida en Opciones */
		if (efx.perfil == FX_LENTO && cfg_get_int("hd_terrain", -1) < 0) hd_enabled = 0;
	}

	if (scene && !strcmp(scene, "music")) {
		music_setup(data_dir);
		music_set_volume(1.0f);
		music_test();
		return 0;
	}

	if (scene && !strcmp(scene, "fx")) {
		/* ayuda de desarrollo y de las pruebas: una obra a medias, un edificio dañado que echa humo y otro que se derrumba */
		su.players = 1; su.size = 2; su.map = 3; su.seed = 4;
		start_game();
		memset(W.fog, 2, (size_t)W.w * W.h);
		float cx = 0, cy = 0;
		for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER) { cx = W.ents[i].x; cy = W.ents[i].y; }
		view_center_on(cx + 3, cy + 4);
		V.zoom = 1.6f;
		int a1 = dev_place(E_HOUSE, 0, cx + 5, cy - 1);
		if (a1 >= 0) { W.ents[a1].progress = 0.5f; W.ents[a1].builders = 2; }
		int b1 = dev_place(E_BARRACKS, 0, cx - 1, cy + 6);
		if (b1 >= 0) W.ents[b1].hp = W.ents[b1].maxhp * 0.2f;
		int c1 = dev_place(E_GRANARY, 0, cx + 6, cy + 6);
		if (c1 >= 0) { fx_evento(FX_EV_EDIFICIO_DESTRUIDO, &W.ents[c1]); W.ents[c1].alive = 0; }
		for (int k = 0; k < 40; ++k) fx_actualizar(1.0f / 30);
	}

	if (scene && !strcmp(scene, "styles")) {
		/* ayuda para desarrollo: cada edificio con el estilo de una civilización en una edad */
		su.players = 1; su.size = 2; su.map = 3; su.seed = 4;
		start_game();
		int civ = getenv("AOEN_CIV") ? atoi(getenv("AOEN_CIV")) : 0, age = getenv("AOEN_AGE") ? atoi(getenv("AOEN_AGE")) : 1;
		W.pl[0].civ = civ; W.pl[0].age = age;
		memset(W.fog, 2, (size_t)W.w * W.h);
		float cx = 0, cy = 0;
		for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER) { cx = W.ents[i].x; cy = W.ents[i].y; W.ents[i].alive = 0; }
		int n = 0;
		for (EntType t = E_TOWN_CENTER; t <= E_WONDER; ++t) {
			int col = n % 5, row = n / 5;
			float x = cx - 14 + col * 6.5f, y = cy - 14 + row * 6.5f;
			if (t == E_WONDER) { x += 1; }
			for (int yy = (int)y - 4; yy < (int)y + 6; ++yy) for (int xx = (int)x - 4; xx < (int)x + 6; ++xx) if (xx >= 0 && yy >= 0 && xx < W.w && yy < W.h) { W.tile[yy * W.w + xx] = TILE_GRASS; W.block[yy * W.w + xx] = 0; }
			dev_place(t, 0, x, y);
			++n;
		}
		view_center_on(cx + 3, cy + 1);
		V.zoom = 0.62f;
		render_world(top_h, bottom_h);
		if (shot) SDL_SaveBMP(fbs, shot);
		return 0;
	}

	if (scene && !strcmp(scene, "loadui")) {
		/* ayuda para desarrollo: guarda en una ranura y la carga de nuevo como lo hacen los menús */
		su.players = 3; start_game();
		for (float t = 0; t < 30; t += 1.0f / 30) world_update(1.0f / 30);
		int ok1 = do_save(2);
		float t1 = W.time; world_free();
		refresh_slots();
		fprintf(stderr, "save ok=%d, slot exists=%d, info: %d players %dx%d\n", ok1, slots[2].exists, slots[2].si.players, slots[2].si.w, slots[2].si.h);
		int ok2 = do_load(2);
		fprintf(stderr, "load ok=%d entities=%d time=%.1f (saved at %.1f) screen=%d\n", ok2, W.nents, W.time, t1, (int)screen);
		return 0;
	}

	if (scene && !strcmp(scene, "collide")) {
		/* ayuda para desarrollo: grupos de aldeanos se cruzan de frente; deben pasar unos por otros */
		su.players = 2; su.size = 2; su.map = 3; su.seed = 5;
		start_game();
		float cx = 0, cy = 0;
		for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER && W.ents[i].owner == 0) { cx = W.ents[i].x; cy = W.ents[i].y + 8; }
		int idx[4], n = 0;
		for (int i = 0; i < W.nents && n < 4; ++i) if (W.ents[i].alive && W.ents[i].owner == 0 && W.ents[i].type == E_VILLAGER) idx[n++] = i;
		float ax[4], ay[4];
		for (int k = 0; k < n; ++k) {
			int left = k < 2;
			W.ents[idx[k]].x = ax[k] = cx + (left ? 0.0f : 12.0f);
			W.ents[idx[k]].y = ay[k] = cy + (k % 2) * 0.3f;
		}
		for (int k = 0; k < n; ++k) {
			int left = k < 2;
			cmd_move(ent_ref(idx[k]), ax[k] + (left ? 12.0f : -12.0f), ay[k]);
		}
		float done_t[4] = { -1, -1, -1, -1 };
		for (float t = 0; t < 40; t += 1.0f / 30) {
			world_update(1.0f / 30);
			for (int k = 0; k < n; ++k) if (done_t[k] < 0 && W.ents[idx[k]].state == ST_IDLE) done_t[k] = t;
			if (getenv("AOEN_TRACE") && (int)(t * 30) % 60 == 0 && t < 30) { for (int k = 0; k < n; ++k) { Entity *e = &W.ents[idx[k]]; fprintf(stderr, "t=%.0f u%d %.2f,%.2f st%d path %d/%d stuck%d walk%d | ", t, k, e->x, e->y, e->state, e->pidx, e->plen, e->stuck, e->walking); } fputc(10, stderr); }
		}
		for (int k = 0; k < n; ++k) fprintf(stderr, "unit %d: %s (%.1f s) at %.1f,%.1f\n", k, done_t[k] < 0 ? "STUCK" : "arrived", done_t[k], W.ents[idx[k]].x, W.ents[idx[k]].y);
		return 0;
	}

	if (scene && !strcmp(scene, "savetest")) {
		/* ayuda para desarrollo: una partida guardada debe continuar exactamente igual que la original */
		su.players = 4; su.size = 2; su.map = 2; su.seed = 21;
		start_game();
		for (float t = 0; t < 120; t += 1.0f / 30) world_update(1.0f / 30);
		if (!world_save("/tmp/claude-1000/aoen_test.sav", 1234)) { fprintf(stderr, "save failed\n"); return 1; }
		for (float t = 0; t < 60; t += 1.0f / 30) world_update(1.0f / 30);
		long a = 0; int na = 0;
		for (int p = 0; p < 4; ++p) a += W.pl[p].res[0] * 7 + W.pl[p].res[1] * 3 + W.pl[p].pop;
		for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive) { ++na; a += (long)(W.ents[i].x * 100) + (long)(W.ents[i].y * 50); }
		if (!world_load("/tmp/claude-1000/aoen_test.sav")) { fprintf(stderr, "load failed\n"); return 1; }
		fprintf(stderr, "loaded at t=%.1f\n", W.time);
		for (float t = 0; t < 60; t += 1.0f / 30) world_update(1.0f / 30);
		long b = 0; int nb = 0;
		for (int p = 0; p < 4; ++p) b += W.pl[p].res[0] * 7 + W.pl[p].res[1] * 3 + W.pl[p].pop;
		for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive) { ++nb; b += (long)(W.ents[i].x * 100) + (long)(W.ents[i].y * 50); }
		fprintf(stderr, "original: entities %d checksum %ld\nreloaded: entities %d checksum %ld -> %s\n", na, a, nb, b, (a == b && na == nb) ? "IDENTICAL" : "DIFFERENT");
		SaveInfo si;
		if (save_info("/tmp/claude-1000/aoen_test.sav", &si)) fprintf(stderr, "info: %d players %dx%d time %.0f stamp %lld\n", si.players, si.w, si.h, si.time, si.stamp);
		return 0;
	}

	if (scene && !strcmp(scene, "hunt")) {
		/* ayuda para desarrollo: los aldeanos cazan al animal más cercano y luego otro pesca */
		su.players = 2; su.size = 2; su.map = 1; su.seed = 9;
		start_game();
		float tx = 0, ty = 0;
		for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER && W.ents[i].owner == 0) { tx = W.ents[i].x; ty = W.ents[i].y; }
		int target = -1, fish = -1; float bd = 1e9f, fd = 1e9f;
		for (int i = 0; i < W.nents; ++i) {
			Entity *e = &W.ents[i];
			if (!e->alive) continue;
			float d = (e->x - tx) * (e->x - tx) + (e->y - ty) * (e->y - ty);
			if ((e->type == E_HORSE || e->type == E_ELEPHANT) && d < bd) { bd = d; target = i; }
			if (e->type == E_FISH && d < fd) { fd = d; fish = i; }
		}
		fprintf(stderr, "animal %d at %.0f tiles, fish %d at %.0f tiles\n", target, sqrtf(bd), fish, sqrtf(fd));
		int n = 0;
		for (int i = 0; i < W.nents; ++i) {
			Entity *u = &W.ents[i];
			if (!u->alive || u->owner != 0 || u->type != E_VILLAGER) continue;
			if (n < 3 && target >= 0) cmd_target(ent_ref(i), ent_ref(target));
			else if (fish >= 0) cmd_target(ent_ref(i), ent_ref(fish));
			++n;
		}
		for (float t = 0; t < 240; t += 1.0f / 30) world_update(1.0f / 30);
		int meat = 0, nanimals = 0, fishes = 0;
		for (int i = 0; i < W.nents; ++i) { if (W.ents[i].alive && W.ents[i].type == E_CARCASS) meat += (int)W.ents[i].amount; if (W.ents[i].alive && (W.ents[i].type == E_HORSE || W.ents[i].type == E_ELEPHANT)) ++nanimals; if (W.ents[i].alive && W.ents[i].type == E_FISH) ++fishes; }
		fprintf(stderr, "food %d, carcass meat left %d, animals %d, fish spots %d, villagers %d\n", W.pl[0].res[RES_FOOD], meat, nanimals, fishes, n);
		return 0;
	}

	if (scene && !strcmp(scene, "dirs")) {
		/* ayuda para desarrollo: unidades mirando en 8 direcciones, con una flecha que muestra por dónde andarían */
		su.players = 2; su.size = 0; su.map = 3;
		start_game();
		int base = -1;
		for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].type == E_VILLAGER && W.ents[i].owner == 0) { base = i; break; }
		float cx = 0, cy = 0;
		for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER && W.ents[i].owner == 0) { cx = W.ents[i].x; cy = W.ents[i].y + 6; }
		int idx = 0;
		for (int i = 0; i < W.nents && idx < 8; ++i) {
			Entity *e = &W.ents[i];
			if (!e->alive || e->owner != 0 || e->type != E_VILLAGER) continue;
			e->x = cx + (idx % 4) * 1.6f - 2.4f; e->y = cy + (idx / 4) * 1.8f;
			e->angle = idx * 0.7853982f; e->walking = 1; e->anim = 3;
			++idx;
		}
		(void)base;
		/* añade las que faltan como copias */
		view_center_on(cx, cy);
		V.zoom = 2.6f;
		W.time = 5;
		render_world(top_h, bottom_h);
		for (int i = 0, k = 0; i < W.nents && k < 8; ++i) {
			Entity *e = &W.ents[i];
			if (!e->alive || e->owner != 0 || e->type != E_VILLAGER) continue;
			int sx, sy, tx, ty;
			view_to_screen(e->x, e->y, &sx, &sy);
			view_to_screen(e->x + cosf(e->angle) * 1.2f, e->y + sinf(e->angle) * 1.2f, &tx, &ty);
			gfx_line(sx, sy, tx, ty, gfx_rgb(255, 0, 0));
			gfx_fill(tx - 2, ty - 2, 4, 4, gfx_rgb(255, 255, 0));
			++k;
		}
		if (shot) SDL_SaveBMP(fbs, shot);
		return 0;
	}

	if (scene && !strcmp(scene, "units")) {
		units_sheet();
		if (shot) SDL_SaveBMP(fbs, shot);
		return 0;
	}

	if (scene && !strcmp(scene, "match")) {
		int ids[64], n = 0;
		for (const char *c = getenv("AOEN_IDS"); c && *c && n < 64;) { ids[n++] = atoi(c); while (*c && *c != ',') ++c; if (*c == ',') ++c; }
		match_report(ids, n);
		return 0;
	}

	if (scene && !strcmp(scene, "sheet")) {
		static int ids[4096];
		int mode = getenv("AOEN_SHEETMODE") ? atoi(getenv("AOEN_SHEETMODE")) : 0;
		int page = getenv("AOEN_PAGE") ? atoi(getenv("AOEN_PAGE")) : 0;
		int per = getenv("AOEN_COLS") ? atoi(getenv("AOEN_COLS")) * (fb.h / (getenv("AOEN_CELLH") ? atoi(getenv("AOEN_CELLH")) : 112)) : 8 * 6;
		int n = sheet_list(mode, ids, 4096);
		if (getenv("AOEN_IDS")) {
			n = 0;
			for (const char *c = getenv("AOEN_IDS"); *c;) {
				ids[n++] = atoi(c);
				while (*c && *c != ',') ++c;
				if (*c == ',') ++c;
			}
			per = 4096;
		}
		fprintf(stderr, "%d sprites, %d pages\n", n, (n + per - 1) / per);
		sheet_draw(ids + page * per, MAX(0, MIN(per, n - page * per)));
		if (shot) SDL_SaveBMP(fbs, shot);
		return 0;
	}

	if (scene && !strcmp(scene, "saves")) { start_game(); do_save(2); do_save(1); world_free(); refresh_slots(); saves_mode = 0; screen = SC_SAVES; }
	else if (scene && !strcmp(scene, "setup")) screen = SC_SETUP;
	else if (scene && !strcmp(scene, "options")) screen = SC_OPTIONS;
	else if (scene && !strcmp(scene, "game")) {
		if (getenv("AOEN_PLAYERS")) su.players = atoi(getenv("AOEN_PLAYERS"));
		if (getenv("AOEN_SIZE")) su.size = atoi(getenv("AOEN_SIZE"));
		if (getenv("AOEN_MAP")) su.map = atoi(getenv("AOEN_MAP"));
		if (getenv("AOEN_SEED")) su.seed = atoi(getenv("AOEN_SEED"));
		start_game();
		for (float t = 0; t < ffwd; t += 1.0f / 30) world_update(1.0f / 30);
		if (getenv("AOEN_FOCUS")) {
			/* ayuda para desarrollo: mira al animal salvaje más cercano y lo hace andar */
			EntType want = !strcmp(getenv("AOEN_FOCUS"), "elephant") ? E_ELEPHANT : E_HORSE;
			float bx = 0, by = 0; for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER && W.ents[i].owner == 0) { bx = W.ents[i].x; by = W.ents[i].y; }
			int best = -1; float bd = 1e9f;
			for (int i = 0; i < W.nents; ++i) { Entity *e = &W.ents[i]; if (!e->alive || e->type != want) continue; float d = (e->x - bx) * (e->x - bx) + (e->y - by) * (e->y - by); if (d < bd) { bd = d; best = i; } }
			if (best >= 0) {
				view_center_on(W.ents[best].x, W.ents[best].y);
				V.zoom = 2.2f;
				for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].owner == 0 && W.ents[i].type == E_VILLAGER) { W.ents[i].x = W.ents[best].x - 3; W.ents[i].y = W.ents[best].y + 1; break; }
				cmd_move(ent_ref(best), W.ents[best].x + 6, W.ents[best].y - 3);
			}
		}
		if (getenv("AOEN_SELECT")) {
			int n = 0;
			for (int i = 0; i < W.nents && n < 3; ++i) if (W.ents[i].alive && W.ents[i].owner == 0 && W.ents[i].type == E_VILLAGER) { W.ents[i].sel = 1; ++n; }
		}
	}

	SDL_StartTextInput();
	Uint32 last = SDL_GetTicks();
	float accum = 0;
	int frame = 0;
	static int fps_frames, fps_value;
	int bench_frames = getenv("AOEN_BENCH") ? atoi(getenv("AOEN_BENCH")) : 0, bench_n = 0, bench_warm = 0;
	Uint32 bench_t0 = 0;
	static Uint32 fps_t;

	while (running) {
		mclick = rclick = wheel = 0;
		mpressed = 0;
		memset(key_pressed, 0, sizeof key_pressed);
		typed[0] = 0;

		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			switch (ev.type) {
			case SDL_QUIT: running = 0; break;
			case SDL_WINDOWEVENT:
				if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || ev.window.event == SDL_WINDOWEVENT_RESIZED) {
					int w, h;
					SDL_GetWindowSize(win, &w, &h);
					if (w > 0 && h > 0 && (w != fb.w || h != fb.h)) resize_fb(w, h);
				}
				break;
			case SDL_MOUSEMOTION: mx = ev.motion.x; my = ev.motion.y; mouse_seen = 1; break;
			case SDL_MOUSEBUTTONDOWN: mx = ev.button.x; my = ev.button.y; if (ev.button.button == SDL_BUTTON_LEFT) { mdown = 1; mpressed = 1; mdx = mx; mdy = my; } break;
			case SDL_MOUSEBUTTONUP:
				AOEN_LOG("[AOEN] ratón: botón %d suelto en %d,%d", ev.button.button, ev.button.x, ev.button.y);
				mx = ev.button.x; my = ev.button.y;
				if (ev.button.button == SDL_BUTTON_LEFT) { mdown = 0; mclick = 1; }
				else if (ev.button.button == SDL_BUTTON_RIGHT) rclick = 1;
				break;
			case SDL_MOUSEWHEEL: wheel += ev.wheel.y; break;
			case SDL_KEYDOWN:
				if (!ev.key.repeat && ev.key.keysym.scancode < 512) key_pressed[ev.key.keysym.scancode] = 1;
				if (ev.key.keysym.sym == SDLK_F11 || (ev.key.keysym.sym == SDLK_RETURN && (ev.key.keysym.mod & KMOD_ALT))) {
					fullscreen = !fullscreen;
					apply_fullscreen();
				}
				if (screen == SC_SETUP && su.seed_focus) {
					if (ev.key.keysym.sym == SDLK_BACKSPACE) su.seed /= 10;
					else if (ev.key.keysym.sym == SDLK_RETURN) su.seed_focus = 0;
				}
				break;
			case SDL_TEXTINPUT:
				if (screen == SC_SETUP && su.seed_focus) {
					for (const char *c = ev.text.text; *c; ++c)
						if (*c >= '0' && *c <= '9' && su.seed < 100000000) su.seed = su.seed * 10 + (*c - '0');
				}
				break;
			}
		}
		if (full_pendiente && frame >= 3) { full_pendiente = 0; fullscreen = 1; apply_fullscreen(); }
		keys = SDL_GetKeyboardState(NULL);
		ui_s = fx_escala_ui(gfx_real_w(), gfx_real_h());
		{
			static int last_w, last_h, last_s;
			if (gfx_real_w() != last_w || gfx_real_h() != last_h || ui_s != last_s) {
				last_w = gfx_real_w(); last_h = gfx_real_h(); last_s = ui_s;
				AOEN_LOG("[AOEN] pantalla %dx%d, interfaz x%d, zoom inicial %.1f", last_w, last_h, ui_s, default_zoom());
			}
		}

		Uint32 now = SDL_GetTicks();
		float dt = MIN((now - last) / 1000.0f, 0.1f);
		last = now;

		if (screen == SC_GAME && W.nents) {
			if (!paused_menu && !W.over) {
				accum += dt * game_speed;
				int steps = 0;
				while (accum >= 1.0f / 30 && steps < 8) { world_update(1.0f / 30); accum -= 1.0f / 30; ++steps; }
				if (steps == 8) accum = 0;
				audio_ambience_tick(dt);
				fx_actualizar(dt);
				if (W.time - last_autosave >= 300.0f && !W.over) { last_autosave = W.time; do_save(0); }
			}
		}

		/* la música sigue lo que hace el jugador */
		{
			static int music_mode = -1;
			int want = W.nents ? (W.over ? 3 : 2) : 1;
			if (want != music_mode) {
				music_mode = want;
				if (want == 1) music_play_menu();
				else if (want == 2) music_play_game();
				else music_play_result(W.over == 1);
			}
			music_update();
		}

		/* dibujo */
		switch (screen) {
		case SC_MENU: gfx_ui_begin(ui_s); draw_menu(); gfx_ui_end(); break;
		case SC_SETUP: gfx_ui_begin(ui_s); draw_setup(); gfx_ui_end(); break;
		case SC_OPTIONS: gfx_ui_begin(ui_s); draw_options(); gfx_ui_end(); break;
		case SC_SAVES: gfx_ui_begin(ui_s); draw_saves(); gfx_ui_end(); break;
		case SC_GAME:
			if (!W.nents) { screen = SC_MENU; break; }
			if (!paused_menu && !W.over) game_input(dt);
			render_world(top_h * ui_s, bottom_h * ui_s);
			if (placing != E_TYPE_COUNT && !paused_menu) {
				float wx, wy;
				screen_to_world(mx, my, &wx, &wy);
				int s = bld_def[placing].size;
				render_placement(placing, (int)floorf(wx - s / 2.0f + 0.5f), (int)floorf(wy - s / 2.0f + 0.5f));
			}
			if (selecting && (abs(mx - sel_x0) > 5 || abs(my - sel_y0) > 5)) {
				gfx_rect(MIN(mx, sel_x0), MIN(my, sel_y0), abs(mx - sel_x0), abs(my - sel_y0), gfx_rgb(255, 255, 255));
			}
			gfx_ui_begin(ui_s);
			{
				int saved = mclick; (void)saved;
				if (paused_menu || W.over) { int c = mclick, d = mdown; mclick = 0; mdown = 0; draw_hud(); mclick = c; mdown = d; }
				else draw_hud();
			}
			if (W.over) draw_endscreen();
			else if (paused_menu) draw_pause();
			gfx_ui_end();
			break;
		}

		++fps_frames;
		if (now - fps_t >= 1000) {
			fps_value = fps_frames; fps_frames = 0; fps_t = now;
			if (screen == SC_GAME && W.nents) {
				int ns = 0, fi = -1;
				for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel) { ++ns; if (fi < 0) fi = i; }
				AOEN_LOG("[AOEN] estado t=%.0f ents=%d sel=%d pos=%.1f,%.1f fps=%d", W.time, W.nents, ns, fi >= 0 ? W.ents[fi].x : -1.0f, fi >= 0 ? W.ents[fi].y : -1.0f, fps_value);
			}
		}
		if (show_fps) {
			char b[32]; snprintf(b, sizeof b, "%d fps", fps_value);
			gfx_text(fb.w - 80, fb.h - 16, b, gfx_rgb(255, 255, 0), 1);
		}

		present_frame();

		++frame;
		if (shot && frame == shot_frame) {
			if (getenv("AOEN_STATS")) {
				for (int p = 0; p < MAX_PLAYERS; ++p) {
					if (!W.pl[p].active) continue;
					int c[E_TYPE_COUNT] = { 0 };
					for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].owner == p) ++c[W.ents[i].type];
					int army = 0;
					for (int t = E_CLUBMAN; t <= E_HOPLITE; ++t) army += c[t];
					fprintf(stderr, "P%d team%d age%d pop %d/%d res w%d f%d g%d s%d | tc%d bar%d arch%d stab%d temp%d acad%d gov%d tow%d farm%d house%d | vill%d army%d priests%d kills %d\n", p, W.pl[p].team, W.pl[p].age, W.pl[p].pop, W.pl[p].popcap,
						W.pl[p].res[0], W.pl[p].res[1], W.pl[p].res[2], W.pl[p].res[3], c[E_TOWN_CENTER], c[E_BARRACKS], c[E_ARCHERY], c[E_STABLE], c[E_TEMPLE], c[E_ACADEMY], c[E_GOVERNMENT], c[E_TOWER], c[E_FARM], c[E_HOUSE], c[E_VILLAGER], army, c[E_PRIEST], W.pl[p].units_killed);
				}
				fprintf(stderr, "time %.0f over %d\n", W.time, W.over);
			}
			SDL_SaveBMP(fbs, shot);
			break;
		}

		if (bench_frames && screen == SC_GAME && W.nents) {
			/* medida de rendimiento (AOEN_BENCH=N): N cuadros sin limitar a 60 por segundo, y se sale */
			if (++bench_warm <= 30) continue;   /* los primeros cuadros (cambio de tamaño, cachés) no cuentan */
			/* la cámara se desplaza en vaivén: cada cuadro cambia entero, como al mover el mapa (el peor caso de dibujo y presentación) */
			{ static float bx0, by0; if (!bench_t0) { bx0 = V.cx; by0 = V.cy; } V.cx = bx0 + 500.0f * sinf(bench_n * 0.07f); V.cy = by0 + 160.0f * sinf(bench_n * 0.05f); }
			if (!bench_t0) bench_t0 = SDL_GetTicks();
			if (++bench_n >= bench_frames) {
				Uint32 ms = SDL_GetTicks() - bench_t0;
				AOEN_LOG("[AOEN] bench %dx%d cuadros=%d ms=%u fps=%.1f efectos=%d", fb.w, fb.h, bench_n, (unsigned)ms, ms ? bench_n * 1000.0f / ms : 0.0f, getenv("AOEN_EFECTOS") ? atoi(getenv("AOEN_EFECTOS")) : -1);
				break;
			}
			continue;
		}
		Uint32 spent = SDL_GetTicks() - now;
		if (spent < 16) SDL_Delay(16 - spent);
	}

	world_free();
	audio_shutdown();
	assets_free();
	SDL_Quit();
	return 0;
}
