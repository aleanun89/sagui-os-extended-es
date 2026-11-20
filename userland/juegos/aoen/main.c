/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* AoEN - aplicación: ventana, bucle del juego, partidas guardadas y escenas de desarrollo y de prueba.
 * La interfaz está en ui_comun.c (controles), ui_menu.c (menús), ui_hud.c (partida) y ui_paneles.c (paneles). */
#include "interfaz.h"
#include "config.h"
#include "efectos.h"
#include "hilos.h"
#include <time.h>

static SDL_Window *win;
static SDL_Surface *fbs;

/* zoom inicial: en pantallas grandes la partida empieza más cerca (a 1080p, 1.5x) para que el mapa no se vea pequeño */
float default_zoom(void) { return gfx_real_h() >= 1000 ? 1.5f : 1.0f; }

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

/* Pone o quita la pantalla completa según app.fullscreen. En SaguiOS el lienzo no cambia solo con la ventana (la SDL del
 * sistema lo escala): se pide el tamaño del escritorio (o el de la ventana al salir) y el búfer se rehace a ese tamaño,
 * así se dibuja a la resolución real y no se amplía un lienzo pequeño. */
void apply_fullscreen(void) {
	SDL_SetWindowFullscreen(win, app.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
#ifdef SAGUI_AOEN
	int w = win_w, h = win_h;
	if (app.fullscreen) {
		SDL_DisplayMode dm;
		if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) {
			w = dm.w; h = dm.h;
			/* resolución de dibujo (v8.5.5.3): al 75 % o al 50 % el búfer es más pequeño y la SDL del sistema lo amplía a la
			 * pantalla (al 50 %, x2 exacto); nunca por debajo de 640x480 */
			int pct = app.escala_dibujo > 0 && app.escala_dibujo < 100 ? app.escala_dibujo : 100;
			int ew = (dm.w * pct / 100) & ~1, eh = (dm.h * pct / 100) & ~1;
			if (pct < 100 && ew >= 640 && eh >= 480) { w = ew; h = eh; }
			AOEN_LOG("[AOEN] pantalla completa: escritorio %dx%d, dibujo al %d%% (%dx%d)", dm.w, dm.h, w == dm.w ? 100 : pct, w, h);
		}
	}
	SDL_SetWindowSize(win, w, h);
	resize_fb(w, h);
#endif
}
static char cmd_data[1100];

/* ---------- empezar, guardar y cargar ---------- */

void start_game_setup(const Setup *s) {
	world_new(s);
	fx_limpiar();
	hud_reiniciar();
	static const char *vic[] = { "estándar", "conquista", "maravilla", "reliquias", "puntuación" };
	AOEN_LOG("[AOEN] partida iniciada: %d jugadores, mapa %dx%d, semilla %d, victoria %s, población %d, dificultad %d, civilización %s, color %d",
		s->players, s->width, s->height, s->seed, vic[CLAMP(s->victory, 0, 4)], s->popcap, s->players > 1 ? s->ai_diff[1] : -1, civ_def[CLAMP(s->civ[0], 0, NUM_CIVS - 1)].name, pcolor(0));
	app.last_autosave = 0;
	for (int i = 0; i < W.nents; ++i)
		if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER && W.ents[i].owner == W.human) view_center_on(W.ents[i].x, W.ents[i].y);
	V.zoom = default_zoom();
	app.game_speed = 1;
	app.paused = 0;
	app.screen = SC_GAME;
}

Slot slots[NUM_SLOTS];
char saves_msg[1400];
void slot_path(int i, char *out, size_t n) {
	char d[1100];
	cfg_saves_dir(d, sizeof d);
	if (i == 0) snprintf(out, n, "%sauto.sav", d);
	else if (i == 1) snprintf(out, n, "%squick.sav", d);
	else snprintf(out, n, "%sslot%d.sav", d, i - 1);
}

int any_save(void) {
	for (int i = 0; i < NUM_SLOTS; ++i) {
		char p[1200];
		slot_path(i, p, sizeof p);
		FILE *f = fopen(p, "rb");
		if (f) { fclose(f); return 1; }
	}
	return 0;
}

void refresh_slots(void) {
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

int do_save(int i) {
	char p[1200];
	slot_path(i, p, sizeof p);
	if (!W.nents || !world_save(p, (long long)time(NULL))) {
		AOEN_LOG("[AOEN] guardado fallido ranura %d", i); world_msg("No se pudo guardar la partida"); snprintf(saves_msg, sizeof saves_msg, "No se pudo guardar en %.1300s", p); return 0; }
	AOEN_LOG("[AOEN] guardada ranura %d (%d entidades)", i, W.nents);
	world_msg(i == 0 ? "Guardado automático" : "Partida guardada");
	snprintf(saves_msg, sizeof saves_msg, "Partida guardada");
	return 1;
}

void enter_loaded_game(void) {
	fx_limpiar();
	hud_reiniciar();
	for (int i = 0; i < W.nents; ++i)
		if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER && W.ents[i].owner == W.human) { view_center_on(W.ents[i].x, W.ents[i].y); break; }
	V.zoom = default_zoom();
	app.game_speed = 1;
	app.paused = 0;
	app.last_autosave = W.time;
	app.screen = SC_GAME;
	world_msg("Partida cargada");
}

int do_load(int i) {
	char p[1200];
	slot_path(i, p, sizeof p);
	if (!world_load(p)) { world_msg("No se pudo cargar la partida"); snprintf(saves_msg, sizeof saves_msg, "No se pudo cargar: el archivo es de otra versión o está dañado"); return 0; }
	AOEN_LOG("[AOEN] cargada ranura %d (%d entidades)", i, W.nents);
	enter_loaded_game();
	return 1;
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

static void pantalla_sin_datos(void) {
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
		fondo_dialogo(30, 30, fb.w - 60, fb.h - 60);
		gfx_ts(52, 48, "No se encuentran los datos del juego", COL_GOLD, 2);
		int y = 100;
		const char *t = cfg_instructions();
		char line[160];
		while (*t) {
			int n = 0;
			while (*t && *t != '\n' && n < 150) line[n++] = *t++;
			line[n] = 0;
			if (*t == '\n') ++t;
			if (gfx_t_w(line, 1) > fb.w - 110) gfx_text(52, y + 4, line, line[0] == ' ' ? COL_CREAM : COL_DIM, 1);
			else gfx_ts(52, y, line, line[0] == ' ' ? COL_CREAM : COL_DIM, 1);
			y += 18;
		}
		gfx_ts(52, fb.h - 96, "Archivo de configuración:", COL_GOLD, 1);
		gfx_ts(52, fb.h - 76, cp, COL_CREAM, 1);
		gfx_ts(52, fb.h - 52, "Pulse Esc o cierre la ventana para salir.", COL_DIM, 1);
		SDL_Surface *ws = SDL_GetWindowSurface(win);
		if (ws) { SDL_BlitSurface(fbs, NULL, ws, NULL); SDL_UpdateWindowSurface(win); }
		if (getenv("AOEN_SHOT")) SDL_SaveBMP(fbs, getenv("AOEN_SHOT"));
		SDL_Delay(40);
	}
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

	int have_data = cfg_find_data(cmd_data, app.data_dir, sizeof app.data_dir);

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

	AOEN_LOG("[AOEN] inicio: ventana %dx%d, datos %s", ww, wh, have_data ? app.data_dir : "no encontrados");
	if (!have_data || !assets_load(app.data_dir)) {
		AOEN_LOG("[AOEN] sin datos del juego: se muestran las instrucciones");
		pantalla_sin_datos();
		return 1;
	}

	{
		char td[1100];
		if (cfg_find_textures(td, sizeof td)) tex_load(td);
		if (getenv("AOEN_CLASSIC")) hd_enabled = 0;
	}
	fx_init();
	hilos_iniciar();   /* D393: hilos de trabajo, uno por núcleo (AOEN_HILOS=1 o «hilos=1» en aoen.cfg: sin hilos) */
	perf_activo = getenv("AOEN_PERFIL") && atoi(getenv("AOEN_PERFIL")) > 0;
	{
		int want = getenv("AOEN_RENDER") ? atoi(getenv("AOEN_RENDER")) : cfg_get_int("render2d", 0);
		if (want) {
			rnd = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
			use_renderer = rnd != NULL;
			AOEN_LOG("[AOEN] presentación: %s", use_renderer ? "renderizador 2D de la SDL" : "superficie de la ventana (no hay renderizador)");
		}
	}
	new_seed();
	if (!getenv("AOEN_NOSOUND") && !(scene && (!strcmp(scene, "sheet") || !strcmp(scene, "units") || !strcmp(scene, "match")))) audio_init(app.data_dir);
	audio_set_volumes(cfg_get_int("volume_sfx", 80) / 100.0f, cfg_get_int("volume_amb", 40) / 100.0f);
	music_set_volume(cfg_get_int("volume_music", 50) / 100.0f);
	music_setup(app.data_dir);
	app.scroll_speed = CLAMP(cfg_get_int("scroll_speed", 2), 1, 3);
	app.cursor_propio = getenv("AOEN_CURSOR") ? atoi(getenv("AOEN_CURSOR")) : cfg_get_int("cursor_propio", 1);
	app.nombres_raton = cfg_get_int("nombres_raton", 1);
	app.escala_dibujo = getenv("AOEN_DIBUJO") ? atoi(getenv("AOEN_DIBUJO")) : cfg_get_int("escala_dibujo", 100);
	{
		int hd = cfg_get_int("hd_terrain", 1);
		if (!hd) hd_enabled = 0;
		/* en «Equipos lentos» las texturas HD se dejan para quien las pida en Opciones */
		if (efx.perfil == FX_LENTO && cfg_get_int("hd_terrain", -1) < 0) hd_enabled = 0;
	}

	if (scene && !strcmp(scene, "music")) {
		music_setup(app.data_dir);
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

	if (scene && !strcmp(scene, "cursores")) {
		/* ayuda de desarrollo y de la documentación (v8.5.5.3): todos los cursores del juego, ampliados, sobre el fondo */
		draw_background(0);
		gfx_fill_a(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0), 120);
		static const char *nom[CUR_COUNT] = { "Normal", "Atacar", "Talar", "Recoger", "Oro", "Piedra", "Construir", "Destino", "No cabe", "Desplazar" };
		int s = MAX(2, fb.w / 256), paso = fb.w / 5;
		for (int k = 0; k < CUR_COUNT; ++k) {
			int x = paso / 2 + (k % 5) * paso, y = fb.h / 4 + (k / 5) * fb.h / 2;
			gfx_fill_a(x - 12 * s, y - 12 * s, 24 * s, 24 * s, gfx_rgb(60, 110, 50), 255);
			bevel(x - 12 * s, y - 12 * s, 24 * s, 24 * s, COL_GOLD);
			cursor_dibujar(k, x - (k == CUR_OBJETIVO || k == CUR_PROHIBIDO || k == CUR_DESPLAZAR ? 0 : 4 * s), y - (k == CUR_OBJETIVO || k == CUR_PROHIBIDO || k == CUR_DESPLAZAR ? 0 : 4 * s), s, k == CUR_DESPLAZAR ? 1 : 0, k == CUR_DESPLAZAR ? -1 : 0);
			gfx_ts(x - gfx_t_w(nom[k], 1) / 2, y + 14 * s, nom[k], COL_CREAM, 1);
		}
		if (shot) SDL_SaveBMP(fbs, shot);
		AOEN_LOG("[AOEN] escena cursores: %d cursores", CUR_COUNT);
		/* se queda unos segundos en pantalla (para la captura de la prueba); Esc sale antes */
		for (int k = 0; k < (shot ? 100 : 1000); ++k) {
			SDL_Event e;
			int fin = 0;
			while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) fin = 1;
			if (fin) break;
			present_frame();
			SDL_Delay(40);
		}
		return 0;
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
		fprintf(stderr, "load ok=%d entities=%d time=%.1f (saved at %.1f) screen=%d\n", ok2, W.nents, W.time, t1, (int)app.screen);
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

	if (scene && !strcmp(scene, "saves")) { start_game(); do_save(2); do_save(1); world_free(); refresh_slots(); app.saves_mode = 0; app.screen = SC_SAVES; }
	else if (scene && !strcmp(scene, "setup")) app.screen = SC_SETUP;
	else if (scene && !strcmp(scene, "options")) { app.screen = SC_OPTIONS; if (getenv("AOEN_PAG")) opciones_pagina(atoi(getenv("AOEN_PAG"))); }
	else if (scene && !strcmp(scene, "game")) {
		if (getenv("AOEN_PLAYERS")) su.players = atoi(getenv("AOEN_PLAYERS"));
		if (getenv("AOEN_SIZE")) su.size = atoi(getenv("AOEN_SIZE"));
		if (getenv("AOEN_MAP")) su.map = atoi(getenv("AOEN_MAP"));
		if (getenv("AOEN_SEED")) su.seed = atoi(getenv("AOEN_SEED"));
		/* D393, partidas cargadas para medir: AOEN_DIF=0..3 dificultad de todas las IA; AOEN_ESPECTADOR=1 la máquina lleva
		 * también al jugador 1 (así nadie pierde enseguida por no hacer nada) */
		if (getenv("AOEN_DIF")) for (int i = 0; i < MAX_PLAYERS; ++i) su.diff[i] = atoi(getenv("AOEN_DIF"));
		start_game();
		if (getenv("AOEN_ESPECTADOR") && atoi(getenv("AOEN_ESPECTADOR")) > 0) { W.pl[0].ai = 1; W.pl[0].diff = su.diff[0]; }
		for (float t = 0; t < ffwd; t += 1.0f / 30) world_update(1.0f / 30);
		if (getenv("AOEN_SIM")) {
			/* D393: simulación sin dibujo, AOEN_SIM segundos de juego a pasos fijos, con la suma del mundo cada minuto: las
			 * pruebas comparan las sumas con 1 y con N hilos (deben ser iguales) y miden el tiempo de la simulación */
			float fin = (float)atof(getenv("AOEN_SIM")), siguiente = 60.0f;
			Uint32 t0 = SDL_GetTicks();
			int pasos = 0;
			while (W.time < fin && !W.over) {
				world_update(1.0f / 30);
				++pasos;
				if (W.time >= siguiente) { AOEN_LOG("[AOEN] sim t=%.0f suma=%08x ents=%d", W.time, world_suma(), W.nents); siguiente += 60.0f; }
			}
			Uint32 ms = SDL_GetTicks() - t0;
			AOEN_LOG("[AOEN] sim final t=%.0f pasos=%d ms=%u suma=%08x ents=%d hilos=%d", W.time, pasos, (unsigned)ms, world_suma(), W.nents, hilos_n());
			hilos_parar();
			world_free();
			SDL_Quit();
			return 0;
		}
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
			/* escena de prueba: sin IA, para que nada estorbe a las unidades que se mueven */
			for (int p = 0; p < MAX_PLAYERS; ++p) W.pl[p].ai = 0;
			int n = 0;
			for (int i = 0; i < W.nents && n < 3; ++i) if (W.ents[i].alive && W.ents[i].owner == 0 && W.ents[i].type == E_VILLAGER) { W.ents[i].sel = 1; ++n; }
		}
	}

	if (scene && !strcmp(scene, "final")) {
		/* prueba de la pantalla final: una partida corta y se acaba con victoria */
		start_game();
		for (float t = 0; t < (ffwd > 0 ? ffwd : 240); t += 1.0f / 30) world_update(1.0f / 30);
		/* AOEN_FINAL=victoria|derrota|espectador: cómo acaba (por defecto, victoria) */
		const char *fin = getenv("AOEN_FINAL") ? getenv("AOEN_FINAL") : "victoria";
		if (!strcmp(fin, "espectador")) W.pl[0].ai = 1;
		W.over = !strcmp(fin, "victoria") ? 1 : 2;
		if (getenv("AOEN_PEST")) final_pest = atoi(getenv("AOEN_PEST"));
	}

	SDL_StartTextInput();
	Uint32 last = SDL_GetTicks();
	float accum = 0;
	int frame = 0;
	static int fps_frames;
	int bench_frames = getenv("AOEN_BENCH") ? atoi(getenv("AOEN_BENCH")) : 0, bench_n = 0, bench_warm = 0;
	/* D393, para las pruebas: AOEN_PASO_FIJO=1 avanza un tic de juego por cuadro (el mundo no depende del reloj) y
	 * AOEN_SUMA_CUADRO=N escribe la suma de los píxeles del mundo dibujado en el cuadro N (igual con 1 y con N hilos) */
	int paso_fijo = getenv("AOEN_PASO_FIJO") && atoi(getenv("AOEN_PASO_FIJO")) > 0;
	int suma_cuadro = getenv("AOEN_SUMA_CUADRO") ? atoi(getenv("AOEN_SUMA_CUADRO")) : -1;
	Uint32 bench_t0 = 0;
	static Uint32 fps_t;

	while (app.running) {
		in.mclick = in.rclick = in.wheel = 0;
		in.mpressed = 0;
		in.bloqueo = 0;
		memset(in.key, 0, sizeof in.key);

		SDL_Event ev;
		unsigned long long t_eventos = perf_activo ? perf_reloj() : 0;
		while (SDL_PollEvent(&ev)) {
			switch (ev.type) {
			case SDL_QUIT: app.running = 0; break;
			case SDL_WINDOWEVENT:
				if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || ev.window.event == SDL_WINDOWEVENT_RESIZED) {
					int w, h;
					SDL_GetWindowSize(win, &w, &h);
					if (w > 0 && h > 0 && (w != fb.w || h != fb.h)) resize_fb(w, h);
				}
				break;
			case SDL_MOUSEMOTION: in.mx = ev.motion.x; in.my = ev.motion.y; in.mouse_seen = 1; break;
			case SDL_MOUSEBUTTONDOWN: in.mx = ev.button.x; in.my = ev.button.y; if (ev.button.button == SDL_BUTTON_LEFT) { in.mdown = 1; in.mpressed = 1; in.mdx = in.mx; in.mdy = in.my; } break;
			case SDL_MOUSEBUTTONUP:
				AOEN_LOG("[AOEN] ratón: botón %d suelto en %d,%d", ev.button.button, ev.button.x, ev.button.y);
				in.mx = ev.button.x; in.my = ev.button.y;
				if (ev.button.button == SDL_BUTTON_LEFT) { in.mdown = 0; in.mclick = 1; }
				else if (ev.button.button == SDL_BUTTON_RIGHT) in.rclick = 1;
				break;
			case SDL_MOUSEWHEEL: in.wheel += ev.wheel.y; break;
			case SDL_KEYDOWN:
				if (app.screen == SC_SETUP && setup_tecla(ev.key.keysym.sym)) break;
				if (!ev.key.repeat && ev.key.keysym.scancode < 512) in.key[ev.key.keysym.scancode] = 1;
				if (ev.key.keysym.sym == SDLK_F11 || (ev.key.keysym.sym == SDLK_RETURN && (ev.key.keysym.mod & KMOD_ALT))) {
					app.fullscreen = !app.fullscreen;
					apply_fullscreen();
				}
				break;
			case SDL_TEXTINPUT:
				if (app.screen == SC_SETUP) setup_tecla_texto(ev.text.text);
				break;
			}
		}
		if (perf_activo) perf_sumar(PERF_EVENTOS, t_eventos);
		if (full_pendiente && frame >= 3) { full_pendiente = 0; app.fullscreen = 1; apply_fullscreen(); }
		in.keys = SDL_GetKeyboardState(NULL);
		in.foco = (SDL_GetWindowFlags(win) & SDL_WINDOW_MOUSE_FOCUS) != 0;
		app.us = fx_escala_ui(gfx_real_w(), gfx_real_h());
		prueba_paso(frame);
		{
			static int last_w, last_h, last_s;
			if (gfx_real_w() != last_w || gfx_real_h() != last_h || app.us != last_s) {
				last_w = gfx_real_w(); last_h = gfx_real_h(); last_s = app.us;
				AOEN_LOG("[AOEN] pantalla %dx%d, interfaz x%d, zoom inicial %.1f", last_w, last_h, app.us, default_zoom());
			}
		}

		Uint32 now = SDL_GetTicks();
		float dt = MIN((now - last) / 1000.0f, 0.1f);
		last = now;
		if (paso_fijo) dt = 1.0f / 30;   /* D393: también las partículas y los avisos, para que el cuadro no dependa del reloj */

		if (app.screen == SC_GAME && W.nents) {
			if (!app.paused && !W.over) {
				accum += dt * app.game_speed;
				int steps = 0;
				PERF_INI();
				if (paso_fijo) { world_update(1.0f / 30); accum = 0; }
				else while (accum >= 1.0f / 30 && steps < 8) { world_update(1.0f / 30); accum -= 1.0f / 30; ++steps; }
				PERF_FIN(PERF_SIM);
				if (steps == 8) accum = 0;
				audio_ambience_tick(dt);
				fx_actualizar(dt);
				avisos_actualizar(dt);
				if (W.time - app.last_autosave >= 300.0f && !W.over) { app.last_autosave = W.time; do_save(0); }
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
		PERF_INI();
		hud_disponer();
		switch (app.screen) {
		case SC_MENU: gfx_ui_begin(app.us); draw_menu(); break;
		case SC_SETUP: gfx_ui_begin(app.us); draw_setup(); break;
		case SC_OPTIONS: gfx_ui_begin(app.us); draw_options(); break;
		case SC_SAVES: gfx_ui_begin(app.us); draw_saves(); break;
		case SC_GAME:
			if (!W.nents) { app.screen = SC_MENU; gfx_ui_begin(app.us); break; }
			if (!app.paused && !W.over) game_input(dt);
			if (!W.nents || app.screen != SC_GAME) { gfx_ui_begin(app.us); break; }
			render_world(top_h * app.us, bottom_h * app.us);
			if (frame == suma_cuadro) {
				unsigned h = 2166136261u;
				for (int y = top_h * app.us; y < fb.h - bottom_h * app.us; ++y)
					for (int x = 0; x < fb.w; ++x) h = (h ^ (fb.px[(size_t)y * fb.pitch + x] & 0xffffffu)) * 16777619u;
				AOEN_LOG("[AOEN] suma del cuadro %d: %08x (t=%.2f, hilos %d, mundo %08x)", frame, h, W.time, hilos_n(), world_suma());
			}
			hud_mundo();
			gfx_ui_begin(app.us);
			if (app.paused || W.over) { int c = in.mclick, d = in.mdown; in.mclick = 0; in.mdown = 0; paneles_cerrar(); draw_hud(); tip_cancelar(); in.mclick = c; in.mdown = d; }
			else { draw_hud(); draw_panel_abierto(); }
			if (W.over) draw_endscreen();
			else if (app.paused) draw_pause();
			break;
		}
		tip_dibujar();
		if (app.screen == SC_GAME && W.nents && !app.paused && !W.over) hud_rotulo_raton();
		solape_fin_cuadro();
		gfx_ui_end();

		/* fundido de entrada al cambiar de pantalla (v8.5.5.3): un cuarto de segundo desde negro */
		{
			static int pantalla_ant = -1;
			if ((int)app.screen != pantalla_ant) { if (pantalla_ant >= 0) app.t_pantalla = SDL_GetTicks(); else app.t_pantalla = SDL_GetTicks() - 1000; pantalla_ant = (int)app.screen; }
			Uint32 el = SDL_GetTicks() - app.t_pantalla;
			if (el < 250) gfx_fill_a(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0), 255 - (int)(el * 255 / 250));
		}
		/* cursor propio (v8.5.5.3): se oculta el del sistema y se dibuja el del juego encima de todo */
		{
			static int oculto;
			int quiero = app.cursor_propio && in.mouse_seen;
			if (quiero != oculto) { SDL_ShowCursor(quiero ? SDL_DISABLE : SDL_ENABLE); oculto = quiero; }
			if (quiero && (in.foco || prueba_activa)) {
				int cdx = 0, cdy = 0, tipo = CUR_FLECHA;
				if (app.screen == SC_GAME && W.nents) tipo = hud_cursor(&cdx, &cdy);
				cursor_dibujar(tipo, in.mx, in.my, app.us, cdx, cdy);
			}
		}

		++fps_frames;
		if (now - fps_t >= 1000) {
			app.fps = fps_frames; fps_frames = 0; fps_t = now;
			if (app.screen == SC_GAME && W.nents) {
				int ns = 0, fi = -1;
				for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel) { ++ns; if (fi < 0) fi = i; }
				/* id, destino y tiempo con décimas: la prueba sigue siempre a la misma unidad y espera por tiempo de juego */
				AOEN_LOG("[AOEN] estado t=%.0f ents=%d sel=%d pos=%.1f,%.1f fps=%d id=%d dest=%.1f,%.1f st=%d tj=%.1f", W.time, W.nents, ns, fi >= 0 ? W.ents[fi].x : -1.0f, fi >= 0 ? W.ents[fi].y : -1.0f, app.fps,
				         fi, fi >= 0 ? W.ents[fi].tx : -1.0f, fi >= 0 ? W.ents[fi].ty : -1.0f, fi >= 0 ? (int)W.ents[fi].state : -1, W.time);
			}
		}
		if (app.show_fps) {
			char b[32]; snprintf(b, sizeof b, "%d fps", app.fps);
			gfx_text(fb.w - 80, top_h * app.us + 4, b, gfx_rgb(255, 255, 0), 1);
		}

		PERF_FIN(PERF_INTERFAZ);
		{
			PERF_INI();
			present_frame();
			PERF_FIN(PERF_PRESENTAR);
		}

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
			AOEN_LOG("[AOEN] captura guardada en el cuadro %d", frame);
			break;
		}

		if (bench_frames && app.screen == SC_GAME && W.nents) {
			/* medida de rendimiento (AOEN_BENCH=N): N cuadros sin limitar a 60 por segundo, y se sale */
			if (++bench_warm <= 30) continue;   /* los primeros cuadros (cambio de tamaño, cachés) no cuentan */
			/* la cámara se desplaza en vaivén: cada cuadro cambia entero, como al mover el mapa (el peor caso de dibujo y presentación) */
			{ static float bx0, by0; if (!bench_t0) { bx0 = V.cx; by0 = V.cy; } V.cx = bx0 + 500.0f * sinf(bench_n * 0.07f); V.cy = by0 + 160.0f * sinf(bench_n * 0.05f); }
			if (!bench_t0) { bench_t0 = SDL_GetTicks(); perf_empezar(); }
			if (++bench_n >= bench_frames) {
				Uint32 ms = SDL_GetTicks() - bench_t0;
				AOEN_LOG("[AOEN] bench %dx%d cuadros=%d ms=%u fps=%.1f efectos=%d hilos=%d", fb.w, fb.h, bench_n, (unsigned)ms, ms ? bench_n * 1000.0f / ms : 0.0f, getenv("AOEN_EFECTOS") ? atoi(getenv("AOEN_EFECTOS")) : -1, hilos_n());
				perf_informe(bench_n);
				break;
			}
			continue;
		}
		Uint32 spent = SDL_GetTicks() - now;
		if (spent < 16) SDL_Delay(16 - spent);
	}

	hilos_parar();
	world_free();
	audio_shutdown();
	assets_free();
	SDL_Quit();
	return 0;
}
