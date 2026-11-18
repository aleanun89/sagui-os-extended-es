/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Menús (v8.5.4.7): menú principal, configuración de la partida con vista previa del mapa, opciones y partidas guardadas.
 * Todo se maneja con el ratón y con el teclado (Arriba/Abajo o Tab mueven el foco, Izquierda/Derecha cambian el valor,
 * Intro pulsa, Esc vuelve). */
#include "interfaz.h"
#include "config.h"
#include "efectos.h"
#include <time.h>

/* ---------- configuración de la partida ---------- */

static const char *map_names[] = { "Aleatorio", "Islas", "Continentes", "Costero", "Interior", "Tierras altas" };
static const char *map_desc[] = {
	"Uno de los cinco tipos, según la semilla.", "Cada jugador en su isla: hacen falta muelle y barcos.", "Dos o tres grandes masas de tierra separadas por mar.",
	"Tierra con costa y bahías: hay pesca y se puede navegar.", "Todo tierra con algún lago: no hay mar.", "Tierra con colinas y pasos estrechos.",
};
static const char *size_names[] = { "Diminuto (48)", "Pequeño (72)", "Mediano (100)", "Grande (150)", "Enorme (200)", "Gigante (260)", "Colosal (320)", "Titánico (400)", "Épico (512)" };
static const int size_values[] = { 48, 72, 100, 150, 200, 260, 320, 400, 512 };
static const int res_values[] = { 0, 100, 200, 500, 1000 };
static const char *res_preset_names[] = { "Estándar", "Pocos", "Medios", "Muchos", "Personalizados" };
static const int res_presets[4][RES_COUNT] = { { 200, 200, 100, 0 }, { 100, 100, 0, 0 }, { 500, 500, 300, 200 }, { 1000, 1000, 1000, 1000 } };
static const int pop_values[] = { 50, 75, 100, 150, 200, 300 };
static const char *diff_names[] = { "Fácil", "Normal", "Difícil", "Muy difícil" };
static const char *diff_desc[] = {
	"Avanza despacio y ataca tarde.",
	"Economía completa, mejoras y ataques a su tiempo.",
	"Sin trampas: economía y ataques organizados; contraataca, se retira, usa murallas, asedio y barcos.",
	"Ve un 20 % más lejos y recoge más rápido.",
};
static const char *vic_names[] = { "Estándar", "Conquista", "Maravilla", "Reliquias", "Puntuación" };
static const char *vic_desc[] = {
	"Gana quien derrota a todos o mantiene una Maravilla 10 minutos.",
	"Solo cuenta derrotar a todos los enemigos.",
	"Conquista, o una Maravilla terminada que resista 10 minutos.",
	"Reunir todas las reliquias en tus templos y guardarlas 60 s (o conquista).",
	"Al acabar el tiempo gana la mayor puntuación (o conquista antes).",
};
static const int tiempo_values[] = { 15, 30, 45, 60, 90, 120 };

ConfPartida su = { .players = 4, .map = 0, .size = 2, .tiempo = 1, .victoria = VIC_STANDARD, .compartida = 1, .recursos_mapa = 1, .recursos = 0, .popcap = 4, .seed = 1,
	.diff = { 1, 1, 1, 1, 1, 1, 1, 1 }, .civ = { 0, 1, 2, 3, 4, 5, 6, 7 }, .team = { 0 }, .color = { 1, 2, 3, 4, 5, 6, 7, 8 }, .res = { 2, 2, 2, 2 } };

void new_seed(void) { su.seed = (int)(SDL_GetTicks() * 2654435761u) & 0x7fffffff; if (su.seed < 1) su.seed = 1; }

/* convierte lo elegido en los menús en la descripción de la partida */
void setup_desde_menu(Setup *s) {
	memset(s, 0, sizeof *s);
	s->width = s->height = size_values[CLAMP(su.size, 0, ARRAY_LEN(size_values) - 1)];
	s->seed = su.seed;
	s->players = su.players;
	s->map_kind = su.map == 0 ? su.seed % MAP_COUNT : su.map - 1;
	s->popcap = pop_values[CLAMP(su.popcap, 0, ARRAY_LEN(pop_values) - 1)];
	s->victory = CLAMP(su.victoria, 0, VIC_COUNT - 1);
	s->time_limit = tiempo_values[CLAMP(su.tiempo, 0, ARRAY_LEN(tiempo_values) - 1)];
	s->victoria_compartida = su.compartida ? 1 : 0;
	s->comercio_mejorado = su.comercio ? 1 : 0;
	/* en Setup, 0 es «normales» (lo de antes, también en las partidas hechas sin el menú); 1 escasos, 2 abundantes */
	s->recursos_mapa = su.recursos_mapa == 0 ? 1 : su.recursos_mapa == 2 ? 2 : 0;
	s->explored = su.revelado || getenv("AOEN_REVEAL");
	s->villagers = getenv("AOEN_VILL") ? atoi(getenv("AOEN_VILL")) : 4;
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		s->ai_diff[i] = su.diff[i]; s->civ[i] = su.civ[i]; s->color[i] = su.color[i];
		s->team[i] = su.team[i] > 0 ? su.team[i] : 10 + i;   /* sin equipo: uno propio */
	}
	if (su.recursos < 4) for (int k = 0; k < RES_COUNT; ++k) s->start_res[k] = res_presets[su.recursos][k];
	else for (int k = 0; k < RES_COUNT; ++k) s->start_res[k] = res_values[su.res[k]];
}

void start_game(void) {
	Setup s;
	setup_desde_menu(&s);
	if (su.espectador) s.explored = 1;
	start_game_setup(&s);
	/* modo espectador (demostración): la máquina juega también por el jugador 1 y el mapa se ve entero */
	if (su.espectador) { W.pl[0].ai = 1; W.pl[0].diff = 2; app.game_speed = 2; AOEN_LOG("[AOEN] modo espectador: la IA juega por todos"); }
}

/* ---------- vista previa del mapa ----------
 * Se genera el mapa de verdad (world_new con la misma semilla) y se dibuja en pequeño con los inicios de cada jugador;
 * después se libera. Se rehace un cuarto de segundo después del último cambio. */
static u32 *prev_px; static int prev_w, prev_h, prev_listo, prev_sucio = 1;
static Uint32 prev_cambio;
static float prev_ini[MAX_PLAYERS][2]; static int prev_n;
static unsigned prev_firma;

static unsigned firma_mapa(void) { return (unsigned)su.map * 31u + (unsigned)su.size * 977u + (unsigned)su.seed * 7u + (unsigned)su.players * 131071u + (unsigned)su.recursos_mapa * 524287u; }

static void generar_prevista(int w, int h) {
	if (W.nents) return;   /* nunca con una partida en marcha */
	Setup s;
	setup_desde_menu(&s);
	Uint32 t0 = SDL_GetTicks();
	world_new(&s);
	if (!W.tile || !W.block || !W.w || !W.h) {   /* sin memoria: world_new liberó el mundo; no hay vista previa */
		world_free();
		free(prev_px); prev_px = NULL; prev_n = 0;
		prev_w = w; prev_h = h; prev_listo = 1; prev_sucio = 0; prev_firma = firma_mapa();   /* no se reintenta en cada cuadro */
		AOEN_LOG("[AOEN] vista previa del mapa: sin memoria");
		return;
	}
	free(prev_px);
	prev_px = (u32 *)malloc((size_t)w * h * 4);
	prev_w = w; prev_h = h;
	if (prev_px) {
		static const u32 tc[4] = { 0xffd2b46e, 0xff3c8232, 0xff1e5aaa, 0xff0f3c82 };
		for (int py = 0; py < h; ++py)
			for (int px = 0; px < w; ++px) {
				float dx = (float)px / w, dy = (float)py / h, u = dx + (dy - 0.5f), v = dx - (dy - 0.5f);
				u32 c = 0;
				if (u >= 0 && u < 1 && v >= 0 && v < 1) {
					int i = (int)(v * W.h) * W.w + (int)(u * W.w);
					c = tc[W.tile[i] & 3];
					if (W.block[i] && W.tile[i] < TILE_WATER) c = ((c >> 1) & 0x7f7f7f) | 0xff000000u;   /* bosques y minas, más oscuros */
				}
				prev_px[py * w + px] = c;
			}
		/* oro y piedra: un punto amarillo o gris de 2x2 en la vista previa */
		for (int i = 0; i < W.nents; ++i) {
			Entity *e = &W.ents[i];
			if (!e->alive || (e->type != E_GOLD && e->type != E_STONE)) continue;
			float u = e->x / W.w, v = e->y / W.h;
			int px = (int)((u + v) * 0.5f * w), py = (int)((0.5f + (u - v) * 0.5f) * h);
			for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx)
				if (px + dx >= 0 && px + dx < w && py + dy >= 0 && py + dy < h) prev_px[(py + dy) * w + px + dx] = e->type == E_GOLD ? 0xffffd700u : 0xffc0c0c0u;
		}
	}
	prev_n = 0;
	for (int i = 0; i < W.nents; ++i)
		if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER && W.ents[i].owner >= 0 && W.ents[i].owner < MAX_PLAYERS) {
			prev_ini[W.ents[i].owner][0] = W.ents[i].x / W.w; prev_ini[W.ents[i].owner][1] = W.ents[i].y / W.h;
			prev_n = MAX(prev_n, W.ents[i].owner + 1);
		}
	int mk = W.setup.map_kind;
	world_free();
	prev_listo = 1; prev_sucio = 0; prev_firma = firma_mapa();
	AOEN_LOG("[AOEN] vista previa del mapa: %s %dx%d, semilla %d, %d jugadores (%u ms)", map_names[mk + 1], s.width, s.height, s.seed, prev_n, (unsigned)(SDL_GetTicks() - t0));
}

static void dibujar_prevista(int x, int y, int w, int h) {
	panel_hueco(x - 4, y - 4, w + 8, h + 8);
	if (firma_mapa() != prev_firma && !prev_sucio) { prev_sucio = 1; prev_cambio = SDL_GetTicks(); }
	if ((prev_sucio && SDL_GetTicks() - prev_cambio > 250) || !prev_listo || prev_w != w || prev_h != h) {
		if (prev_listo && prev_w == w && prev_h == h) {
			/* se avisa en este cuadro y se genera en el siguiente, para que el aviso se vea */
			static int avisado;
			if (!avisado) { avisado = 1; gfx_ts(x + w / 2 - gfx_t_w("Generando...", 1) / 2, y + h / 2 - 8, "Generando...", COL_AVISO, 1); return; }
			avisado = 0;
		}
		generar_prevista(w, h);
	}
	if (!prev_px) return;
	int us = gfx_ui_suspend();
	for (int py = 0; py < h * us; ++py) {
		int yy = y * us + py;
		if (yy < 0 || yy >= gfx_real_h()) continue;
		u32 *dst = fb.px + (size_t)yy * fb.pitch + x * us;
		const u32 *src = prev_px + (size_t)(py / us) * prev_w;
		for (int px = 0; px < w * us; ++px) { u32 c = src[px / us]; if (c) dst[px] = c; }
	}
	gfx_ui_resume(us);
	for (int p = 0; p < prev_n && p < su.players; ++p) {
		float u = prev_ini[p][0], v = prev_ini[p][1];
		int mx = x + (int)((u + v) * 0.5f * w), my = y + (int)((0.5f + (u - v) * 0.5f) * h);
		gfx_fill(mx - 6, my - 6, 13, 13, gfx_rgb(10, 8, 6));
		gfx_fill(mx - 5, my - 5, 11, 11, col_player[(su.color[p] - 1) & 7]);
		char b[4]; snprintf(b, sizeof b, "%d", p + 1);
		gfx_text(mx - 2, my - 3, b, gfx_rgb(10, 8, 6), 1);
	}
	if (prev_sucio) gfx_text(x + 4, y + h - 10, "(se actualiza)", COL_DIM, 1);
}

/* ---------- pantalla de configuración ---------- */
static int setup_fila = 0;   /* jugador cuya civilización se describe */
static int setup_pest;       /* en pantallas estrechas: 0 jugadores, 1 mapa */

static int color_libre(int i, int dir) {
	int c = su.color[i];
	for (int k = 0; k < MAX_PLAYERS; ++k) {
		c = (c - 1 + dir + MAX_PLAYERS) % MAX_PLAYERS + 1;
		int usado = 0;
		for (int j = 0; j < su.players; ++j) if (j != i && su.color[j] == c) usado = 1;
		if (!usado) return c;
	}
	return su.color[i];
}

static void columna_jugadores(int x, int y, int w, int h) {
	char buf[96];
	gfx_ts(x, y + 4, "Jugadores", COL_GOLD, 1);
	if (button(x + 96, y, 26, 24, "-", su.players > 1, 1)) --su.players;
	snprintf(buf, sizeof buf, "%d", su.players);
	gfx_ts(x + 134 - gfx_t_w(buf, 1) / 2 + 4, y + 4, buf, COL_CREAM, 1);
	if (button(x + 150, y, 26, 24, "+", su.players < MAX_PLAYERS, 1)) {
		++su.players;
		su.color[su.players - 1] = color_libre(su.players - 1, 0) ? color_libre(su.players - 1, 1) : su.color[su.players - 1];
	}
	if (button(x + w - 150, y, 150, 24, "Equipos al azar", su.players > 2, 1)) {
		unsigned r = (unsigned)SDL_GetTicks() * 2654435761u;
		int nt = 2 + (int)(r % (unsigned)MAX(1, MIN(3, su.players / 2)));
		for (int i = 0; i < su.players; ++i) { r = r * 1103515245u + 12345u; su.team[i] = 1 + (int)((r >> 16) % (unsigned)nt); }
		su.team[1] = su.team[0] % nt + 1;   /* el primer rival nunca va con el jugador */
	}
	y += 32;
	/* cabecera y filas */
	int cw_col = 26, cw_nom = 44, cw_eq = 72, cw_dif = 138, gap = 6;
	int cw_civ = w - cw_col - cw_nom - cw_eq - cw_dif - 4 * gap;
	int xc = x, xn = xc + cw_col + gap, xv = xn + cw_nom + gap, xe = xv + cw_civ + gap, xd = xe + cw_eq + gap;
	gfx_text(xv, y, "CIVILIZACIÓN", COL_GOLD, 1);
	gfx_text(xe, y, "EQUIPO", COL_GOLD, 1);
	gfx_text(xd, y, "DIFICULTAD", COL_GOLD, 1);
	y += 12;
	int rowh = MIN(32, MAX(28, (h - 44 - 90) / MAX(1, su.players)));
	for (int i = 0; i < su.players; ++i) {
		int yy = y + i * rowh;
		if (hit(x, yy, w, rowh - 2)) setup_fila = i;
		panel_hueco(x - 2, yy - 1, w + 4, rowh - 2);
		if (setup_fila == i) bevel(x - 2, yy - 1, w + 4, rowh - 2, gfx_rgb(150, 120, 70));
		/* color: clic, el siguiente libre; clic derecho, el anterior */
		int over = hit(xc, yy + 1, cw_col, 24);
		gfx_fill(xc, yy + 1, cw_col, 24, gfx_rgb(10, 8, 6));
		gfx_fill(xc + 3, yy + 4, cw_col - 6, 18, col_player[(su.color[i] - 1) & 7]);
		bevel(xc, yy + 1, cw_col, 24, over ? COL_CREAM : COL_GOLD);
		if (over) {
			tip_simple((Rect){ xc, yy + 1, cw_col, 24 }, "Color", "Clic: el siguiente libre. Clic derecho: el anterior.");
			if (in.mclick) { su.color[i] = color_libre(i, 1); snd_ui(SND_CLICK); }
			if (in.rclick) { su.color[i] = color_libre(i, -1); snd_ui(SND_CLICK); }
		}
		gfx_ts(xn, yy + 5, i == 0 ? "Tú" : (snprintf(buf, sizeof buf, "IA %d", i + 1), buf), i == 0 ? COL_AVISO : COL_CREAM, 1);
		int c = cycler(xv, yy + 1, cw_civ, civ_def[su.civ[i]].name);
		if (c) { su.civ[i] = (su.civ[i] + c + NUM_CIVS) % NUM_CIVS; setup_fila = i; }
		snprintf(buf, sizeof buf, su.team[i] ? "%d" : "No", su.team[i]);
		c = cycler(xe, yy + 1, cw_eq, buf);
		if (c) su.team[i] = (su.team[i] + c + 5) % 5;
		if (hit(xe, yy + 1, cw_eq, 26)) tip_simple((Rect){ xe, yy + 1, cw_eq, 26 }, "Equipo", "Los del mismo equipo empiezan aliados. «No»: sin equipo.");
		if (i > 0) {
			c = cycler(xd, yy + 1, cw_dif, diff_names[CLAMP(su.diff[i], 0, 3)]);
			if (c) su.diff[i] = (su.diff[i] + c + 4) % 4;
			if (hit(xd, yy + 1, cw_dif, 26)) tip_simple((Rect){ xd, yy + 1, cw_dif, 26 }, diff_names[CLAMP(su.diff[i], 0, 3)], diff_desc[CLAMP(su.diff[i], 0, 3)]);
		} else gfx_text(xd + 8, yy + 9, "jugador humano", COL_DIM, 1);
	}
	/* civilización del jugador marcado: su descripción y bonificación */
	int dy = y + su.players * rowh + 6, dh = MIN(84, y + h - 44 - dy);
	if (dh >= 40) {
		int c = su.civ[CLAMP(setup_fila, 0, su.players - 1)];
		panel_hueco(x - 2, dy, w + 4, dh);
		snprintf(buf, sizeof buf, "%s %s", civ_def[c].name, setup_fila == 0 ? "(tu civilización)" : "");
		gfx_ts(x + 6, dy + 6, buf, COL_GOLD, 1);
		static const char *estilos[] = { "Edificios egipcios", "Edificios griegos", "Edificios asiáticos", "Edificios babilonios" };
		gfx_text(x + w - 6 - gfx_text_w(estilos[civ_def[c].style & 3], 1), dy + 10, estilos[civ_def[c].style & 3], COL_DIM, 1);
		texto_ajustado(x + 6, dy + 26, w - 12, civ_def[c].desc, COL_CREAM, (dh - 30) / 16);
		/* y la dificultad de ese jugador, si es de la máquina */
		int f = CLAMP(setup_fila, 0, su.players - 1), ey = dy + dh + 8;
		if (f > 0 && ey + 56 <= y + h - 4) {
			int d = CLAMP(su.diff[f], 0, 3);
			panel_hueco(x - 2, ey, w + 4, 56);
			snprintf(buf, sizeof buf, "Dificultad de IA %d: %s", f + 1, diff_names[d]);
			gfx_ts(x + 6, ey + 6, buf, COL_GOLD, 1);
			texto_ajustado(x + 6, ey + 24, w - 12, diff_desc[d], COL_CREAM, 2);
		}
	}
}

static int fila_opcion(int x, int y, int w, const char *etiqueta, const char *valor, int lbl) {
	gfx_ts(x, y + 5, etiqueta, COL_CREAM, 1);
	return cycler(x + lbl, y, w - lbl, valor);
}

/* «mapa explorado» y «espectador»: en pantalla ancha van bajo la columna de jugadores (en la de mapa no caben a 800x600) */
static int extra_a_la_izquierda;
static void casillas_extra(int x, int y, int w) {
	checkbox(x, y, "Mapa explorado desde el principio", &su.revelado);
	y += 28;
	checkbox(x, y, "Espectador: la máquina juega por ti", &su.espectador);
	if (hit(x, y, w, 22)) tip_simple((Rect){ x, y, w, 22 }, "Modo espectador", "Partida de demostración: tu civilización también la lleva la máquina (difícil) a velocidad x2. Puedes mirar, usar el minimapa y el marcador.");
}

static void columna_mapa(int x, int y, int w, int h) {
	char buf[96];
	int c, lbl = MIN(130, w / 3);
	gfx_ts(x, y + 4, "Mapa y reglas", COL_GOLD, 1);
	y += 30;
	int pw = MIN(w - 8, 2 * MAX(60, MIN(160, h - 8 * 30 - 30 - 16))), ph = pw / 2;
	dibujar_prevista(x + (w - pw) / 2, y + 4, pw, ph);
	y += ph + 16;
	if ((c = fila_opcion(x, y, w, "Tipo", map_names[su.map], lbl))) su.map = (su.map + c + 6) % 6;
	if (hit(x + lbl, y, w - lbl, 26)) tip_simple((Rect){ x + lbl, y, w - lbl, 26 }, map_names[su.map], map_desc[su.map]);
	y += 30;
	if ((c = fila_opcion(x, y, w, "Tamaño", size_names[su.size], lbl))) su.size = (su.size + c + 9) % 9;
	y += 30;
	gfx_ts(x, y + 5, "Semilla", COL_CREAM, 1);
	int sw = w - lbl - 80;
	gfx_fill(x + lbl, y, sw, 26, COL_PANEL2);
	bevel(x + lbl, y, sw, 26, su.seed_focus ? COL_CREAM : COL_GOLD);
	snprintf(buf, sizeof buf, "%d%s", su.seed, su.seed_focus && ((SDL_GetTicks() / 400) & 1) ? "_" : "");
	gfx_ts(x + lbl + 8, y + 5, buf, COL_CREAM, 1);
	if (in.mclick && hit(x + lbl, y, sw, 26)) su.seed_focus = 1;
	else if (in.mclick) su.seed_focus = 0;
	if (hit(x + lbl, y, sw, 26)) tip_simple((Rect){ x + lbl, y, sw, 26 }, "Semilla", "La misma semilla da el mismo mapa. Clic para escribirla.");
	if (button(x + lbl + sw + 6, y, 74, 26, "Nueva", 1, 1)) new_seed();
	y += 30;
	if ((c = fila_opcion(x, y, w, "Recursos", res_preset_names[su.recursos], lbl))) su.recursos = (su.recursos + c + 5) % 5;
	if (hit(x + lbl, y, w - lbl, 26)) {
		static const char *rd[] = { "200 madera, 200 comida, 100 oro.", "100 madera, 100 comida.", "500 madera y comida, 300 oro, 200 piedra.", "1000 de cada.", "Elige cada recurso abajo." };
		tip_simple((Rect){ x + lbl, y, w - lbl, 26 }, "Recursos iniciales", rd[su.recursos]);
	}
	y += 30;
	if (su.recursos == 4) {
		int cw = (w - 8) / 4;
		for (int k = 0; k < RES_COUNT; ++k) {
			icono_rec(k, x + k * cw, y + 4, 16);
			snprintf(buf, sizeof buf, "%d", res_values[su.res[k]]);
			if ((c = cycler(x + k * cw + 26, y, cw - 30, buf))) su.res[k] = (su.res[k] + c + 5) % 5;
		}
		y += 30;
	}
	snprintf(buf, sizeof buf, "%d", pop_values[su.popcap]);
	if ((c = fila_opcion(x, y, w, "Población máx.", buf, lbl))) su.popcap = (su.popcap + c + 6) % 6;
	y += 30;
	if ((c = fila_opcion(x, y, w, "Victoria", vic_names[su.victoria], lbl))) su.victoria = (su.victoria + c + VIC_COUNT) % VIC_COUNT;
	if (hit(x + lbl, y, w - lbl, 26)) tip_simple((Rect){ x + lbl, y, w - lbl, 26 }, vic_names[su.victoria], vic_desc[su.victoria]);
	y += 30;
	if (su.victoria == VIC_SCORE) {
		snprintf(buf, sizeof buf, "%d minutos", tiempo_values[su.tiempo]);
		if ((c = fila_opcion(x, y, w, "Tiempo límite", buf, lbl))) su.tiempo = (su.tiempo + c + 6) % 6;
		y += 30;
	}
	{
		static const char *rmn[] = { "Escasos", "Normales", "Abundantes" };
		static const char *rmd[] = { "La mitad de minas, bayas y bosques: partidas más cortas y disputadas.",
		                             "Cada jugador tiene oro y piedra en su base y un parche de cada a media distancia, más minas repartidas.",
		                             "El doble de minas y más grandes, más bayas y bosques: partidas largas, de economía." };
		if ((c = fila_opcion(x, y, w, "Riqueza", rmn[CLAMP(su.recursos_mapa, 0, 2)], lbl))) su.recursos_mapa = (su.recursos_mapa + c + 3) % 3;
		if (hit(x + lbl, y, w - lbl, 26)) tip_simple((Rect){ x + lbl, y, w - lbl, 26 }, "Recursos del mapa", rmd[CLAMP(su.recursos_mapa, 0, 2)]);
		y += 30;
	}
	checkbox(x, y, "Comercio mejorado (barcos y carros)", &su.comercio);
	if (hit(x, y, w, 22))
		tip_simple((Rect){ x, y, w, 22 }, "Comercio mejorado (barcos y carros)",
		           "Además del modo oro clásico, los barcos y carros mercantes pueden intercambiar: llevan 15 de un recurso tuyo a un puerto o mercado ajeno y traen otro; cuanto más lejos y más escaso lo que das, más recibes.");
	y += 28;
	checkbox(x, y, "Victoria compartida por equipos y aliados", &su.compartida);
	if (hit(x, y, w, 22))
		tip_simple((Rect){ x, y, w, 22 }, "Victoria compartida por equipos y aliados",
		           su.compartida ? "Activada: la partida acaba cuando todos los que quedan son del mismo equipo o están aliados, y ganan juntos. La Maravilla y las reliquias de un aliado cuentan para el bando."
		                         : "Desactivada: solo un equipo del menú puede ganar. Las alianzas de la partida se rompen al final y los aliados tienen que decidirlo entre ellos.");
	y += 28;
	y += 28;
	if (!extra_a_la_izquierda) casillas_extra(x, y, w);
}

void draw_setup(void) {
	draw_background(1);
	gfx_fill_a(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0), 120);
	foco_inicio();
	const char *tit = "Partida aleatoria";
	gfx_ts(fb.w / 2 - gfx_t_w(tit, 2) / 2, 10, tit, COL_GOLD, 2);
	int px = 12, py = 48, pw = fb.w - 24, ph = fb.h - py - 58;
	panel(px, py, pw, ph);
	int ancho = pw >= 760;
	int ix = px + 20, iy = py + 18, iw = pw - 40, ih = ph - 30;
	extra_a_la_izquierda = ancho;
	if (ancho) {
		int lw = iw * 57 / 100;
		columna_jugadores(ix, iy, lw, ih - 60);
		casillas_extra(ix, iy + ih - 52, lw);
		gfx_fill(ix + lw + 12, iy, 1, ih, gfx_rgb(110, 85, 45));
		columna_mapa(ix + lw + 26, iy, iw - lw - 26, ih);
	} else {
		if (button(ix, iy, 150, 24, "Jugadores", setup_pest != 0, 1)) setup_pest = 0;
		if (button(ix + 156, iy, 150, 24, "Mapa y reglas", setup_pest != 1, 1)) setup_pest = 1;
		if (setup_pest == 0) columna_jugadores(ix, iy + 32, iw, ih - 32);
		else columna_mapa(ix, iy + 32, iw, ih - 32);
	}
	if (button(12, fb.h - 50, 140, 40, "< Volver", 1, 2) || tecla(SDL_SCANCODE_ESCAPE)) app.screen = SC_MENU;
	const char *pie = "Flechas o Tab: moverse · Izq./Der.: cambiar · Intro: empezar";
	if (fb.w >= 760) gfx_text(fb.w / 2 - gfx_text_w(pie, 1) / 2, fb.h - 34, pie, COL_DIM, 1);
	int empezar = button(fb.w - 12 - 260, fb.h - 52, 260, 44, "Empezar partida", su.players >= 1, 3);
	if (empezar || (tecla(SDL_SCANCODE_RETURN) && foco_actual < 0 && !su.seed_focus)) start_game();
	solape_registrar(px, py, pw, ph, "panel");
	solape_registrar(12, fb.h - 50, 140, 40, "volver");
	solape_registrar(fb.w - 272, fb.h - 52, 260, 44, "empezar");
}

void setup_tecla_texto(const char *t) {
	if (!su.seed_focus) return;
	for (const char *c = t; *c; ++c)
		if (*c >= '0' && *c <= '9' && su.seed < 100000000) su.seed = su.seed * 10 + (*c - '0');
}

int setup_tecla(int sym) {
	if (!su.seed_focus) return 0;
	if (sym == SDLK_BACKSPACE) { su.seed /= 10; return 1; }
	if (sym == SDLK_RETURN || sym == SDLK_ESCAPE) { su.seed_focus = 0; return 1; }
	return 0;
}

/* ---------- menú principal ---------- */
void draw_menu(void) {
	draw_background(0);
	foco_inicio();
	int bw = 320, bh = 50, gap = 14, x = (fb.w - bw) / 2, y = fb.h * 44 / 100;
	if (y + 4 * (bh + gap) + 40 > fb.h) y = fb.h - 4 * (bh + gap) - 40;
	if (button(x, y, bw, bh, "Un jugador", 1, 3)) app.screen = SC_SETUP;
	static int has_saves = -1;
	if (has_saves < 0 || (SDL_GetTicks() / 1000) % 2 == 0) has_saves = any_save();
	if (button(x, y + (bh + gap), bw, bh, "Cargar partida", has_saves, 3)) { refresh_slots(); saves_msg[0] = 0; app.saves_mode = 0; app.saves_back = SC_MENU; app.screen = SC_SAVES; }
	if (!has_saves && hit(x, y + (bh + gap), bw, bh)) tip_simple((Rect){ x, y + (bh + gap), bw, bh }, "Cargar partida", "Aún no hay partidas guardadas: guarda con F5 o desde la pausa.");
	if (button(x, y + 2 * (bh + gap), bw, bh, "Opciones", 1, 3)) { app.options_back = SC_MENU; app.screen = SC_OPTIONS; }
	if (button(x, y + 3 * (bh + gap), bw, bh, "Salir", 1, 3)) app.running = 0;
	const char *pie = "AoEN - motor nuevo en C. Age of Empires (c) Microsoft. Se necesita una copia legítima del juego.";
	gfx_fill_a(0, fb.h - 26, fb.w, 26, gfx_rgb(0, 0, 0), 140);
	gfx_ts(fb.w / 2 - gfx_t_w(pie, 1) / 2, fb.h - 21, pie, gfx_rgb(220, 210, 190), 1);
	if (fb.w < gfx_t_w(pie, 1) + 10) { gfx_fill_a(0, fb.h - 26, fb.w, 26, gfx_rgb(0, 0, 0), 255); gfx_text(fb.w / 2 - gfx_text_w(pie, 1) / 2, fb.h - 16, pie, gfx_rgb(220, 210, 190), 1); }
}

/* ---------- opciones ---------- */
static int opt_page;   /* 0 gráficos, 1 sonido y control */

static void fx_toggle_changed(void) {
	efx.perfil = FX_PERSONAL;
	gfx_smooth = efx.suave;
	fx_guardar();
}

void draw_options(void) {
	draw_background(0);
	gfx_fill_a(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0), 150);
	foco_inicio();
	int c;
	int pw = MIN(fb.w - 20, 640), ph = MIN(fb.h - 20, 520), px = (fb.w - pw) / 2, py = (fb.h - ph) / 2;
	panel(px, py, pw, ph);
	gfx_ts(fb.w / 2 - gfx_t_w("Opciones", 2) / 2, py + 16, "Opciones", COL_GOLD, 2);
	if (button(px + 24, py + 56, pw / 2 - 30, 28, "Gráficos", opt_page != 0, 1)) opt_page = 0;
	if (button(px + pw / 2 + 6, py + 56, pw / 2 - 30, 28, "Sonido y control", opt_page != 1, 1)) opt_page = 1;
	int y = py + 98;
	if (opt_page == 0) {
		int fs = app.fullscreen;
		if (checkbox(px + 30, y, "Pantalla completa", &fs)) { app.fullscreen = fs; cfg_set_int("fullscreen", fs); apply_fullscreen(); }
		checkbox(px + pw / 2, y, "Mostrar FPS", &app.show_fps);
		y += 32;
		if (tex_available()) {
			int hd = hd_enabled;
			if (checkbox(px + 30, y, "Terreno con texturas HD", &hd)) { hd_enabled = hd; cfg_set_int("hd_terrain", hd); }
		} else gfx_text(px + 30, y + 6, "Texturas HD no encontradas (aoen-config --textures RUTA)", COL_DIM, 1);
		y += 34;
		gfx_ts(px + 30, y + 5, "Efectos gráficos:", COL_GOLD, 1);
		if ((c = cycler(px + pw - 250, y, 220, fx_nombre_perfil(efx.perfil)))) {
			int p = (efx.perfil + c + 4) % 4;
			if (p == FX_PERSONAL) { efx.perfil = FX_PERSONAL; fx_guardar(); }
			else { fx_perfil(p); gfx_smooth = efx.suave; fx_guardar(); }
		}
		y += 36;
		struct { const char *n; int *v; } Lx[] = {
			{ "Transiciones", &efx.transiciones }, { "Agua animada", &efx.agua }, { "Niebla suave", &efx.niebla }, { "Sombras", &efx.sombras },
			{ "Humo y polvo", &efx.particulas }, { "Obras y ruinas", &efx.construccion }, { "Día y noche", &efx.diaynoche }, { "Zoom suave", &efx.suave },
		};
		for (int k = 0; k < 8; ++k) if (checkbox(px + 30 + (k & 1) * (pw / 2 - 10), y + (k >> 1) * 28, Lx[k].n, Lx[k].v)) fx_toggle_changed();
		y += 4 * 28 + 8;
		gfx_ts(px + 30, y + 5, "Tamaño de la interfaz:", COL_CREAM, 1);
		static const char *en[] = { "Automático", "Normal (1x)", "Grande (2x)" };
		if ((c = cycler(px + pw - 250, y, 220, en[CLAMP(efx.escala, 0, 2)]))) { efx.escala = (efx.escala + c + 3) % 3; fx_guardar(); }
		y += 34;
		int r2 = cfg_get_int("render2d", 0);
		if (checkbox(px + 30, y, "Renderizador 2D del sistema (al reiniciar)", &r2)) cfg_set_int("render2d", r2);
		y += 30;
		gfx_text(px + 30, y, "Equipos lentos: sin efectos, como el AoEN original. Normal: los baratos. Completo: todo.", COL_DIM, 1);
	} else {
		static const char *vn[] = { "0%", "25%", "50%", "75%", "100%" };
		int vs = CLAMP((int)(audio_sfx_volume() * 4 + 0.5f), 0, 4), va = CLAMP((int)(audio_ambience_volume() * 4 + 0.5f), 0, 4);
		gfx_ts(px + 30, y + 5, "Volumen de efectos:", COL_CREAM, 1);
		if ((c = cycler(px + pw - 210, y, 170, vn[vs]))) { vs = (vs + c + 5) % 5; audio_set_volumes(vs / 4.0f, audio_ambience_volume()); cfg_set_int("volume_sfx", vs * 25); snd_ui(SND_CLICK); }
		y += 38;
		int vm = CLAMP((int)(music_volume() * 4 + 0.5f), 0, 4);
		gfx_ts(px + 30, y + 5, "Volumen de música:", COL_CREAM, 1);
		if ((c = cycler(px + pw - 210, y, 170, vn[vm]))) { vm = (vm + c + 5) % 5; music_set_volume(vm / 4.0f); cfg_set_int("volume_music", vm * 25); }
		y += 38;
		gfx_ts(px + 30, y + 5, "Volumen de ambiente:", COL_CREAM, 1);
		if ((c = cycler(px + pw - 210, y, 170, vn[va]))) { va = (va + c + 5) % 5; audio_set_volumes(audio_sfx_volume(), va / 4.0f); cfg_set_int("volume_amb", va * 25); }
		y += 44;
		static const char *sp[] = { "Lenta", "Normal", "Rápida" };
		gfx_ts(px + 30, y + 5, "Velocidad de desplazamiento:", COL_CREAM, 1);
		c = cycler(px + pw - 210, y, 170, sp[app.scroll_speed - 1]);
		if (c) { app.scroll_speed = (app.scroll_speed - 1 + c + 3) % 3 + 1; cfg_set_int("scroll_speed", app.scroll_speed); }
		y += 52;
		gfx_text(px + 30, y, "Carpeta de datos (se cambia con aoen-config):", COL_DIM, 1);
		gfx_text(px + 30, y + 14, app.data_dir, COL_DIM, 1);
		{ char cp[1100]; cfg_path(cp, sizeof cp); gfx_text(px + 30, y + 28, cp, COL_DIM, 1); }
	}
	if (button(px + (pw - 200) / 2, py + ph - 52, 200, 38, "Volver", 1, 2) || tecla(SDL_SCANCODE_ESCAPE)) app.screen = app.options_back;
}

/* ---------- partidas guardadas ---------- */
static void slot_name(int i, char *out, size_t n) {
	if (i == 0) snprintf(out, n, "Guardado automático");
	else if (i == 1) snprintf(out, n, "Guardado rápido");
	else snprintf(out, n, "Ranura %d", i - 1);
}

void draw_saves(void) {
	if (W.nents) { render_world(0, 0); gfx_fill_a(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0), 170); }
	else { draw_background(0); gfx_fill_a(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0), 150); }
	foco_inicio();
	int rowh = MIN(42, (fb.h - 170) / NUM_SLOTS);
	int pw = MIN(fb.w - 40, 720), ph = MIN(fb.h - 20, 90 + NUM_SLOTS * rowh + 80), px = (fb.w - pw) / 2, py = (fb.h - ph) / 2;
	panel(px, py, pw, ph);
	const char *tit = app.saves_mode ? "Guardar partida" : "Cargar partida";
	gfx_ts(fb.w / 2 - gfx_t_w(tit, 2) / 2, py + 18, tit, COL_GOLD, 2);
	int y = py + 64;
	for (int i = 0; i < NUM_SLOTS; ++i) {
		if (app.saves_mode && i == 0) continue;   /* la ranura automática no se escribe a mano */
		char nm[64], info[160];
		slot_name(i, nm, sizeof nm);
		panel_hueco(px + 20, y, pw - 40, rowh - 4);
		gfx_ts(px + 30, y + 3, nm, COL_CREAM, 1);
		if (slots[i].exists) {
			time_t t = (time_t)slots[i].si.stamp;
			char ts[48] = "";
			struct tm *lt = localtime(&t);
			if (lt) strftime(ts, sizeof ts, "%d/%m/%Y %H:%M", lt);
			int tm = (int)slots[i].si.time;
			snprintf(info, sizeof info, "%d jug.  %dx%d  %02d:%02d  %s", slots[i].si.players, slots[i].si.w, slots[i].si.h, tm / 60, tm % 60, ts);
			gfx_text(px + 30, y + rowh - 16, info, COL_DIM, 1);
		} else if (slots[i].incompatible) gfx_text(px + 30, y + rowh - 16, "Guardado de otra versión del juego: no se puede cargar", COL_MAL, 1);
		else gfx_text(px + 30, y + rowh - 16, "(vacía)", COL_DIM, 1);
		int bx = px + pw - 160, bh = MIN(30, rowh - 8);
		if (app.saves_mode) {
			if (button(bx, y + (rowh - 4 - bh) / 2, 130, bh, slots[i].exists ? "Sobrescribir" : "Guardar", 1, 1))
				if (do_save(i)) { refresh_slots(); app.screen = app.saves_back; }
		} else if (button(bx, y + (rowh - 4 - bh) / 2, 130, bh, "Cargar", slots[i].exists, 1)) do_load(i);
		y += rowh;
	}
	char dir[1100];
	cfg_saves_dir(dir, sizeof dir);
	if (saves_msg[0]) gfx_ts(fb.w / 2 - gfx_t_w(saves_msg, 1) / 2, py + ph - 78, saves_msg, COL_AVISO, 1);
	gfx_text(fb.w / 2 - gfx_text_w(dir, 1) / 2, py + ph - 14, dir, COL_DIM, 1);
	if (button(px + (pw - 200) / 2, py + ph - 56, 200, 34, "Volver", 1, 2) || tecla(SDL_SCANCODE_ESCAPE)) app.screen = app.saves_back;
}
