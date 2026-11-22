/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Interfaz del multijugador en red local (D461): pantalla de salas, sala con plazas, chat, y lo que se ve en la partida
 * (pausa común, espera, desincronización, chat). La red está en red.c, las órdenes en ordenes.c. */
#include "interfaz.h"
#include "red.h"
#include "ordenes.h"
#include "efectos.h"

/* ---------- campos de texto ---------- */
enum { CAMPO_NINGUNO, CAMPO_NOMBRE, CAMPO_IP, CAMPO_CHAT };
static char nombre[16] = "Jugador", ip_txt[32], chat_txt[90];
static int campo_foco;

static char *buf_campo(int c, int *max) {
	*max = c == CAMPO_NOMBRE ? 15 : c == CAMPO_IP ? 21 : 88;
	return c == CAMPO_NOMBRE ? nombre : c == CAMPO_IP ? ip_txt : c == CAMPO_CHAT ? chat_txt : NULL;
}

static void campo(int x, int y, int w, int id, const char *vacio) {
	int max; char *b = buf_campo(id, &max);
	gfx_fill(x, y, w, 26, COL_PANEL2);
	bevel(x, y, w, 26, campo_foco == id ? COL_CREAM : COL_GOLD);
	char t[120]; snprintf(t, sizeof t, "%s%s", b, campo_foco == id && ((SDL_GetTicks() / 400) & 1) ? "_" : "");
	if (!b[0] && campo_foco != id && vacio) gfx_ts(x + 8, y + 5, vacio, COL_DIM, 1);
	else {
		/* si no cabe, se ve el final */
		const char *s = t;
		while (*s && gfx_t_w(s, 1) > w - 14) ++s;
		gfx_ts(x + 8, y + 5, s, COL_CREAM, 1);
	}
	if (in.mclick && hit(x, y, w, 26)) { campo_foco = id; in.bloqueo = 1; }
}

void ui_red_texto(const char *t) {
	int max; char *b = buf_campo(campo_foco, &max);
	if (!b) return;
	for (const char *c = t; *c; ++c) {
		int n = (int)strlen(b);
		unsigned char u = (unsigned char)*c;
		if (n >= max || u < 32 || u == 127) continue;
		if (campo_foco != CAMPO_CHAT && u >= 128) continue;   /* nombre e IP: solo ASCII */
		b[n] = *c; b[n + 1] = 0;
	}
}

static void enviar_chat(void) {
	if (chat_txt[0]) { red_chat(chat_txt); chat_txt[0] = 0; }
}

int ui_red_chat_activo(void) { return campo_foco == CAMPO_CHAT && app.screen == SC_GAME; }
static int unir_por_ip(void);

/* teclas especiales en los campos; devuelve 1 si las usó */
int ui_red_tecla(int sym) {
	if (!campo_foco) return 0;
	int max; char *b = buf_campo(campo_foco, &max);
	if (sym == SDLK_BACKSPACE && b) {
		int n = (int)strlen(b);
		while (n > 0) { --n; if (((unsigned char)b[n] & 0xc0) != 0x80) break; }
		b[n] = 0;
		return 1;
	}
	if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
		if (campo_foco == CAMPO_CHAT) { enviar_chat(); if (app.screen == SC_GAME) campo_foco = CAMPO_NINGUNO; }
		else if (campo_foco == CAMPO_IP) unir_por_ip();
		else campo_foco = CAMPO_NINGUNO;
		return 1;
	}
	if (sym == SDLK_ESCAPE) { campo_foco = CAMPO_NINGUNO; return 1; }
	return 0;
}

void ui_red_abrir_chat(void) { campo_foco = CAMPO_CHAT; }

/* ---------- entrada en la partida ---------- */
static void entrar_en_partida(void) {
	fx_limpiar();
	hud_reiniciar();
	for (int i = 0; i < W.nents; ++i)
		if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER && W.ents[i].owner == W.human) { view_center_on(W.ents[i].x, W.ents[i].y); break; }
	V.zoom = default_zoom();
	app.game_speed = 1; app.paused = 0; app.last_autosave = 0;
	campo_foco = CAMPO_NINGUNO;
	app.screen = SC_GAME;
}

void mp_abrir(void) {
	red_cerrar();
	red_cb_inicio = entrar_en_partida;
	if (!red_iniciar()) { /* el aviso sale en la pantalla */ }
	app.screen = SC_MP;
	campo_foco = CAMPO_NINGUNO;
}

void mp_salir(void) {
	red_cerrar();
	app.screen = SC_MENU;
}

static int unir_por_ip(void) {
	RDir d;
	if (!red_dir_texto(ip_txt, &d)) { snprintf(ip_txt, sizeof ip_txt, "%s", ip_txt); return 0; }
	red_unirse(d, nombre);
	return 1;
}

static void fondo_pantalla(void) {
	draw_background(0);
	gfx_fill_a(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0), 150);
}

/* ---------- pantalla de salas ---------- */
void draw_mp(void) {
	fondo_pantalla();
	foco_inicio();
	int pw = MIN(fb.w - 20, 720), ph = MIN(fb.h - 20, 540), px = (fb.w - pw) / 2, py = (fb.h - ph) / 2;
	panel(px, py, pw, ph);
	const char *tit = "Multijugador (red local)";
	gfx_ts(fb.w / 2 - gfx_t_w(tit, 2) / 2, py + 12, tit, COL_GOLD, 2);
	int x = px + 24, w = pw - 48, y = py + 56;
	int uniendo = red_estado() == RED_UNIENDO;
	if (red_estado() == RED_SALA_CLI || red_estado() == RED_SALA_ANF) { app.screen = SC_SALA; return; }

	gfx_ts(x, y + 5, "Tu nombre", COL_CREAM, 1);
	campo(x + 110, y, MIN(220, w - 110), CAMPO_NOMBRE, "Jugador");
	y += 36;
	if (button(x, y, 200, 36, "Crear sala", !uniendo, 2)) {
		if (red_crear_sala(nombre)) { app.screen = SC_SALA; red_sala_cambiada(); }
	}
	gfx_text(x + 212, y + 6, "Tú eres el anfitrión: eliges el mapa,", COL_DIM, 1);
	gfx_text(x + 212, y + 20, "añades jugadores de la máquina y empiezas.", COL_DIM, 1);
	y += 50;
	/* búsqueda por difusión cada segundo */
	static unsigned t_busca;
	if (!uniendo && SDL_GetTicks() - t_busca > 1000) { t_busca = SDL_GetTicks(); red_buscar(); }
	gfx_ts(x, y, "Salas en la red local", COL_GOLD, 1);
	y += 22;
	int lh = MAX(70, MIN(150, ph - 56 - 36 - 50 - 22 - 120 - 60));
	panel_hueco(x - 4, y - 2, w + 8, lh + 4);
	RPartida ps[16]; int n = red_partidas(ps, 16);
	if (!n) gfx_ts(x + 8, y + 8, uniendo ? "Conectando..." : "Buscando... (la sala del anfitrión debe estar creada)", COL_DIM, 1);
	for (int k = 0; k < n && (k + 1) * 30 <= lh; ++k) {
		int yy = y + k * 30;
		char t[96];
		snprintf(t, sizeof t, "%s  -  %d jugadores (%d personas)  -  %u.%u.%u.%u", ps[k].nombre, ps[k].jugadores, ps[k].humanos, ps[k].dir.ip >> 24, (ps[k].dir.ip >> 16) & 255, (ps[k].dir.ip >> 8) & 255, ps[k].dir.ip & 255);
		gfx_ts(x + 6, yy + 7, t, COL_CREAM, 1);
		if (button(x + w - 100, yy + 1, 96, 26, "Unirse", !uniendo, 1)) red_unirse(ps[k].dir, nombre);
	}
	y += lh + 14;
	gfx_ts(x, y + 5, "IP del anfitrión", COL_CREAM, 1);
	campo(x + 130, y, MIN(230, w - 130 - 150), CAMPO_IP, "192.168.1.5");
	if (button(x + 130 + MIN(230, w - 130 - 150) + 8, y, 130, 26, "Unirse por IP", !uniendo && ip_txt[0], 1)) unir_por_ip();
	y += 38;
	if (red_estado() == RED_ERROR || (red_error()[0] && !uniendo)) gfx_ts(x, y, red_error(), COL_MAL, 1);
	else if (uniendo) gfx_ts(x, y, "Conectando con el anfitrión...", COL_AVISO, 1);
	else gfx_text(x, y + 2, "Las dos máquinas deben estar en la misma red y usar la misma versión de AoEN.", COL_DIM, 1);
	if (red_estado() == RED_ERROR && SDL_GetTicks() / 1000 > 0) {
		/* el error se limpia al volver a intentarlo */
		static char ultimo[120];
		if (strcmp(ultimo, red_error())) { snprintf(ultimo, sizeof ultimo, "%s", red_error()); }
		if (button(x + w - 150, y - 4, 150, 30, "Reintentar", 1, 1)) { red_cerrar(); red_iniciar(); }
	}
	if (button(px + 24, py + ph - 52, 140, 40, "< Volver", 1, 2) || (tecla(SDL_SCANCODE_ESCAPE) && !campo_foco)) mp_salir();
	solape_registrar(px, py, pw, ph, "panel");
}

/* ---------- la sala ---------- */
static const char *tipo_nombre[] = { "Abierta", "Persona", "Máquina", "Cerrada" };
static const char *dif_nombre[] = { "Fácil", "Normal", "Difícil", "Muy difícil" };

static int color_libre_sala(int i, int dir) {
	int c = red_plaza[i].color;
	for (int k = 0; k < MAX_PLAYERS; ++k) {
		c = (c - 1 + dir + MAX_PLAYERS) % MAX_PLAYERS + 1;
		int usado = 0;
		for (int j = 0; j < MAX_PLAYERS; ++j) if (j != i && (red_plaza[j].tipo == PL_HUMANO || red_plaza[j].tipo == PL_IA) && red_plaza[j].color == c) usado = 1;
		if (!usado) return c;
	}
	return red_plaza[i].color;
}

static void cambios_anfitrion(int i, int quien_cambia_civ) { (void)i; (void)quien_cambia_civ; red_sala_cambiada(); }

static void lista_plazas(int x, int y, int w, int h) {
	int anf = red_anfitrion();
	gfx_ts(x, y + 2, "Jugadores", COL_GOLD, 1);
	char buf[96];
	snprintf(buf, sizeof buf, "Turnos de %d ms", red_ticks_turno * 1000 / 30);
	if (anf) { int c = cycler(x + w - 170, y - 4, 170, buf); if (c) { red_ticks_turno = CLAMP(red_ticks_turno + c, 3, 6); red_sala_cambiada(); } }
	y += 30;
	int cw_col = 26, cw_tipo = 112, cw_civ = MAX(96, (w - cw_col - cw_tipo - 4 * 6) * 45 / 100), cw_eq = 66, gap = 6;
	int cw_ex = w - cw_col - cw_tipo - cw_civ - cw_eq - 4 * gap;
	int xc = x, xt = xc + cw_col + gap, xv = xt + cw_tipo + gap, xe = xv + cw_civ + gap, xx = xe + cw_eq + gap;
	gfx_text(xt, y, "JUGADOR", COL_GOLD, 1); gfx_text(xv, y, "CIVILIZACIÓN", COL_GOLD, 1); gfx_text(xe, y, "EQUIPO", COL_GOLD, 1);
	y += 12;
	int rowh = MIN(34, MAX(28, (h - 40 - 120) / MAX_PLAYERS));
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		Plaza *p = &red_plaza[i];
		int yy = y + i * rowh;
		panel_hueco(x - 2, yy - 1, w + 4, rowh - 2);
		int propia = i == red_mi_plaza && p->tipo == PL_HUMANO;
		int edito = anf || propia;   /* el anfitrión edita todo; cada uno, lo suyo */
		int usada = p->tipo == PL_HUMANO || p->tipo == PL_IA;
		if (usada) {
			int over = hit(xc, yy + 1, cw_col, 24);
			gfx_fill(xc, yy + 1, cw_col, 24, gfx_rgb(10, 8, 6));
			gfx_fill(xc + 3, yy + 4, cw_col - 6, 18, col_player[(p->color - 1) & 7]);
			bevel(xc, yy + 1, cw_col, 24, over && edito ? COL_CREAM : COL_GOLD);
			if (over && edito) {
				tip_simple((Rect){ xc, yy + 1, cw_col, 24 }, "Color", "Clic: el siguiente libre. Clic derecho: el anterior.");
				int nc = in.mclick ? color_libre_sala(i, 1) : in.rclick ? color_libre_sala(i, -1) : 0;
				if (nc) { if (anf) { p->color = nc; red_sala_cambiada(); } else red_elegir(p->civ, nc, p->equipo, p->listo); snd_ui(SND_CLICK); }
			}
		}
		/* tipo de plaza */
		if (i == 0) {
			snprintf(buf, sizeof buf, "%s", p->nombre[0] ? p->nombre : nombre);
			gfx_ts(xt, yy + 5, buf, i == red_mi_plaza ? COL_AVISO : COL_CREAM, 1);
		} else if (anf && p->tipo != PL_HUMANO) {
			int c = cycler(xt, yy + 1, cw_tipo + 6, tipo_nombre[p->tipo]);
			if (c) {
				static const int orden[3] = { PL_LIBRE, PL_IA, PL_CERRADA };
				int k = p->tipo == PL_IA ? 1 : p->tipo == PL_CERRADA ? 2 : 0;
				p->tipo = orden[(k + c + 3) % 3];
				cambios_anfitrion(i, 0);
			}
		} else {
			snprintf(buf, sizeof buf, "%s", p->tipo == PL_HUMANO ? p->nombre : tipo_nombre[p->tipo]);
			gfx_ts(xt, yy + 5, buf, propia ? COL_AVISO : p->tipo == PL_HUMANO ? COL_CREAM : COL_DIM, 1);
		}
		if (usada) {
			int c = 0;
			if (edito) c = cycler(xv, yy + 1, cw_civ, civ_def[p->civ].name);
			else { gfx_ts(xv + 6, yy + 5, civ_def[p->civ].name, COL_CREAM, 1); }
			if (c) { int nc = (p->civ + c + NUM_CIVS) % NUM_CIVS; if (anf) { p->civ = nc; red_sala_cambiada(); } else red_elegir(nc, p->color, p->equipo, p->listo); }
			snprintf(buf, sizeof buf, p->equipo ? "%d" : "No", p->equipo);
			c = 0;
			if (edito) c = cycler(xe, yy + 1, cw_eq, buf);
			else gfx_ts(xe + 6, yy + 5, buf, COL_CREAM, 1);
			if (c) { int ne = (p->equipo + c + 5) % 5; if (anf) { p->equipo = ne; red_sala_cambiada(); } else red_elegir(p->civ, p->color, ne, p->listo); }
			if (p->tipo == PL_IA && anf) {
				int d = cycler(xx, yy + 1, cw_ex, dif_nombre[CLAMP(p->dif, 0, 3)]);
				if (d) { p->dif = (p->dif + d + 4) % 4; red_sala_cambiada(); }
			} else if (p->tipo == PL_HUMANO) {
				if (anf && i > 0) {
					if (button(xx, yy + 1, MAX(60, cw_ex), 24, "Echar", 1, 1)) red_expulsar(i);
				} else {
					snprintf(buf, sizeof buf, "%s%s", p->listo || i == 0 ? "Listo" : "...", i > 0 && red_ping_ms(i) ? "" : "");
					gfx_ts(xx + 4, yy + 5, buf, p->listo || i == 0 ? COL_OK : COL_DIM, 1);
				}
			} else gfx_ts(xx + 4, yy + 5, "máquina", COL_DIM, 1);
		}
	}
}

static void panel_chat(int x, int y, int w, int h) {
	panel_hueco(x - 2, y, w + 4, h);
	int lineas = (h - 40) / 16;
	int n = red_chat_n();
	for (int k = MAX(0, n - lineas), r = 0; k < n; ++k, ++r) {
		int de; const char *t = red_chat_texto(k, &de);
		char b[140]; snprintf(b, sizeof b, "%s%s%s", de >= 0 && de < MAX_PLAYERS && red_plaza[de].nombre[0] ? red_plaza[de].nombre : "", de >= 0 ? ": " : "* ", t);
		gfx_ts(x + 6, y + 6 + r * 16, b, de >= 0 ? COL_CREAM : COL_AVISO, 1);
	}
	campo(x, y + h - 32, w - 90, CAMPO_CHAT, "Escribe un mensaje y pulsa Intro");
	if (button(x + w - 84, y + h - 32, 84, 26, "Enviar", 1, 1)) { enviar_chat(); campo_foco = CAMPO_CHAT; }
}

void sala_columna_mapa(int x, int y, int w, int h);
void sala_resumen_mapa(int x, int y, int w, int h, const Setup *s);

static Setup ultimo_cfg;

void draw_sala(void) {
	fondo_pantalla();
	foco_inicio();
	if (red_estado() == RED_NADA && !red_anfitrion()) { app.screen = SC_MP; return; }
	int anf = red_anfitrion();
	int px = 12, py = 12, pw = fb.w - 24, ph = fb.h - 24 - 56;
	panel(px, py, pw, ph);
	char tit[96]; snprintf(tit, sizeof tit, anf ? "Sala de %s" : "Sala", red_plaza[0].nombre[0] ? red_plaza[0].nombre : "Anfitrión");
	gfx_ts(fb.w / 2 - gfx_t_w(tit, 2) / 2, py + 8, tit, COL_GOLD, 2);
	int ancho = pw >= 760;
	int ix = px + 20, iy = py + 40, iw = pw - 40, ih = ph - 52;
	/* el anfitrión: lo del menú de partida aleatoria (mapa y reglas) pasa a la sala */
	if (anf) {
		int n = 0;
		for (int i = 0; i < MAX_PLAYERS; ++i) if (red_plaza[i].tipo == PL_HUMANO || red_plaza[i].tipo == PL_IA) { su.color[n] = red_plaza[i].color; ++n; }
		su.players = MAX(2, n); su.espectador = 0;
		Setup s; setup_desde_menu(&s);
		if (memcmp(&s, &red_cfg, sizeof s)) { red_cfg = s; red_sala_cambiada(); }
	}
	static int pest;
	if (ancho) {
		int lw = iw * 56 / 100, ch = MIN(140, ih / 3);
		lista_plazas(ix, iy, lw, ih - ch - 8);
		panel_chat(ix, iy + ih - ch, lw, ch);
		gfx_fill(ix + lw + 12, iy, 1, ih, gfx_rgb(110, 85, 45));
		if (anf) sala_columna_mapa(ix + lw + 26, iy, iw - lw - 26, ih);
		else sala_resumen_mapa(ix + lw + 26, iy, iw - lw - 26, ih, &red_cfg);
	} else {
		if (pestana(ix, iy, 130, 26, "Jugadores", pest == 0)) pest = 0;
		if (pestana(ix + 136, iy, 130, 26, "Mapa", pest == 1)) pest = 1;
		if (pestana(ix + 272, iy, 130, 26, "Chat", pest == 2)) pest = 2;
		if (pest == 0) lista_plazas(ix, iy + 32, iw, ih - 32);
		else if (pest == 1) { if (anf) sala_columna_mapa(ix, iy + 32, iw, ih - 32); else sala_resumen_mapa(ix, iy + 32, iw, ih - 32, &red_cfg); }
		else panel_chat(ix, iy + 32, iw, ih - 32);
	}
	(void)ultimo_cfg;
	/* pie */
	int by = fb.h - 52;
	if (button(12, by, 140, 40, "< Salir", 1, 2) || (tecla(SDL_SCANCODE_ESCAPE) && !campo_foco)) { red_cerrar(); red_iniciar(); app.screen = SC_MP; return; }
	if (red_estado() == RED_ERROR) {
		gfx_ts(170, by + 12, red_error(), COL_MAL, 1);
		return;
	}
	if (red_error()[0]) gfx_ts(170, by + 12, red_error(), COL_MAL, 1);
	else {
		char b[96]; int ms = red_ping_ms(red_mi_plaza);
		if (!anf && ms) { snprintf(b, sizeof b, "Ida y vuelta con el anfitrión: %d ms", ms); gfx_ts(170, by + 12, b, COL_DIM, 1); }
		else if (anf) { int h = 0; for (int i = 1; i < MAX_PLAYERS; ++i) h += red_plaza[i].tipo == PL_HUMANO; snprintf(b, sizeof b, h ? "%d personas esperando" : "Esperando a otras personas (o añade una plaza de la máquina)", h); gfx_ts(170, by + 12, b, COL_DIM, 1); }
	}
	if (anf) {
		int hay = 0;
		for (int i = 1; i < MAX_PLAYERS; ++i) hay += red_plaza[i].tipo == PL_HUMANO || red_plaza[i].tipo == PL_IA;
		if (button(fb.w - 12 - 260, by - 2, 260, 44, "Empezar partida", hay > 0, 3)) {
			if (!red_empezar() && !red_error()[0]) { /* el motivo sale abajo */ }
		}
	} else {
		Plaza *p = &red_plaza[red_mi_plaza];
		if (button(fb.w - 12 - 260, by - 2, 260, 44, p->listo ? "No estoy listo" : "Estoy listo", 1, 3)) red_elegir(p->civ, p->color, p->equipo, !p->listo);
	}
	solape_registrar(px, py, pw, ph, "panel");
}

/* ---------- en la partida ---------- */
static unsigned t_espera; static int t_espera_ok;

void ui_red_hud(void) {
	if (!ord_red && red_estado() == RED_NADA) return;
	char b[160];
	int y = fb.h - bottom_h - 16;
	/* abajo a la derecha, sobre la barra inferior: el ida y vuelta */
	if (red_estado() == RED_JUEGO) {
		int worst = 0;
		for (int p = 0; p < MAX_PLAYERS; ++p) if (red_jugador_humano(p) && p != W.human) { int ms = red_ping_ms(red_anfitrion() ? p : red_mi_plaza); if (ms > worst) worst = ms; }
		snprintf(b, sizeof b, "Red %d ms · turno %d", worst, red_turno());
		gfx_text(fb.w - 8 - gfx_text_w(b, 1), y, b, worst > 250 ? COL_MAL : COL_DIM, 1);
	}
	if (red_estado() == RED_ARRANCANDO) {
		const char *m = "Preparando la partida... esperando a los demás";
		int w = gfx_t_w(m, 2) + 60, h = 70;
		gfx_fill_a(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0), 120);
		panel((fb.w - w) / 2, (fb.h - h) / 2, w, h);
		gfx_ts((fb.w - gfx_t_w(m, 2)) / 2, (fb.h - 32) / 2, m, COL_GOLD, 2);
	}
	if (red_pausado()) {
		const char *m = "PAUSA (Pausa o F6: seguir)";
		gfx_fill_a(0, fb.h / 2 - 28, fb.w, 56, gfx_rgb(0, 0, 0), 150);
		gfx_ts((fb.w - gfx_t_w(m, 2)) / 2, fb.h / 2 - 16, m, COL_AVISO, 2);
	}
	unsigned esp = red_esperando();
	if (esp) {
		if (!t_espera_ok) { t_espera_ok = 1; t_espera = SDL_GetTicks(); }
		if (SDL_GetTicks() - t_espera > 600) {
			char nb[80] = "";
			for (int p = 0; p < MAX_PLAYERS; ++p) if (esp & (1u << p)) { if (nb[0]) strcat(nb, ", "); strncat(nb, p == 0 && !red_anfitrion() ? "el anfitrión" : red_nombre(p), 20); }
			snprintf(b, sizeof b, "Esperando a %s...", nb[0] ? nb : "los demás");
			int w = gfx_t_w(b, 1) + 24;
			gfx_fill_a((fb.w - w) / 2, fb.h * 30 / 100, w, 28, gfx_rgb(40, 14, 10), 200);
			gfx_ts((fb.w - w) / 2 + 12, fb.h * 30 / 100 + 6, b, COL_AVISO, 1);
		}
	} else t_espera_ok = 0;
	if (red_desincronizado() >= 0) {
		snprintf(b, sizeof b, "¡Desincronizados en el turno %d! La partida puede estar rota: se guardó un volcado.", red_desincronizado());
		int w = gfx_t_w(b, 1) + 24;
		gfx_fill_a((fb.w - w) / 2, fb.h - bottom_h - 70, w, 28, gfx_rgb(120, 20, 14), 220);
		gfx_ts((fb.w - w) / 2 + 12, fb.h - bottom_h - 64, b, COL_CREAM, 1);
	}
	if (campo_foco == CAMPO_CHAT) {
		int w = MIN(fb.w - 40, 560), x = 20, yy = fb.h - bottom_h - 40;
		gfx_fill_a(x - 4, yy - 4, w + 8, 36, gfx_rgb(0, 0, 0), 180);
		campo(x, yy, w, CAMPO_CHAT, "Mensaje (Intro envía, Esc cancela)");
	}
	if (red_estado() == RED_ERROR) {
		int w = 520, h = 120, x = (fb.w - w) / 2, yy = (fb.h - h) / 2;
		panel(x, yy, w, h);
		gfx_ts(x + 20, yy + 20, red_error(), COL_MAL, 1);
		if (button(x + w / 2 - 100, yy + 62, 200, 40, "Volver al menú", 1, 2)) { red_cerrar(); world_free(); app.screen = SC_MENU; }
	}
}

/* ---------- guion de pruebas (AOEN_RED_AUTO), solo para las pruebas automáticas ----------
 *   AOEN_RED_AUTO=anf,TURNOS[,IAS]   crea la sala, espera a una persona, empieza y da órdenes al azar hasta el turno TURNOS
 *   AOEN_RED_AUTO=cli,TURNOS         busca la sala por difusión, entra, se marca listo y hace lo mismo
 * Al llegar al turno escribe «[AOEN] red-auto: FIN turno=N suma=HHHHHHHH» y sale. Pasa por las pantallas de verdad. */
void ui_red_auto(int frame) {
	static int modo = -1, turnos, ias, fase, f0, turno_ant = -1;
	static unsigned t_fin;
	if (modo < 0) {
		const char *a = getenv("AOEN_RED_AUTO");
		modo = 0;
		if (a && !strncmp(a, "anf,", 4)) { modo = 1; turnos = atoi(a + 4); const char *c = strchr(a + 4, ','); ias = c ? atoi(c + 1) : 1; }
		else if (a && !strncmp(a, "cli,", 4)) { modo = 2; turnos = atoi(a + 4); }
	}
	if (!modo) return;
	if (fase == 0 && frame >= 5 && app.screen == SC_MENU) {
		mp_abrir();
		fase = 1; f0 = frame;
		red_azar_semilla(modo == 1 ? 11 : 22);
		return;
	}
	if (fase == 1 && app.screen == SC_MP) {
		if (modo == 1) {
			if (!red_crear_sala("Anfitrion")) { AOEN_LOG("[AOEN] red-auto: no se pudo crear la sala: %s", red_error()); app.running = 0; return; }
			su.size = 0; su.seed = getenv("AOEN_SEED") ? atoi(getenv("AOEN_SEED")) : 5; su.map = 3; su.recursos = 0;
			red_plaza[1].tipo = PL_LIBRE;
			for (int k = 0; k < ias && k < 6; ++k) { red_plaza[2 + k].tipo = PL_IA; red_plaza[2 + k].dif = 1; snprintf(red_plaza[2 + k].nombre, 16, "IA %d", 3 + k); }
			red_plaza[0].listo = 1; red_suma_cada = 1;   /* la suma de cada turno, para compararla */
			app.screen = SC_SALA; red_sala_cambiada();
			fase = 2; f0 = frame;
		} else {
			RPartida p[4];
			if (red_partidas(p, 4) > 0) {
				AOEN_LOG("[AOEN] red-auto: sala encontrada: %s (%d jugadores)", p[0].nombre, p[0].jugadores);
				red_unirse(p[0].dir, "Cliente"); fase = 2; f0 = frame;
			}
		}
		return;
	}
	if (fase == 2) {
		static unsigned t_sala; static int t_sala_ok;
		if (modo == 2 && red_estado() == RED_SALA_CLI && red_mi_plaza > 0) {
			Plaza *p = &red_plaza[red_mi_plaza];
			if (!p->listo) { static int dicho; if (!dicho) { dicho = 1; red_chat("hola desde el cliente"); } red_elegir(p->civ, p->color, p->equipo, 1); }
			if (red_estado() == RED_SALA_CLI && p->listo) { if (!t_sala_ok) { t_sala_ok = 1; t_sala = SDL_GetTicks(); AOEN_LOG("[AOEN] red-auto: listo en la sala"); } fase = 3; }
		}
		if (modo == 1 && red_estado() == RED_SALA_ANF && red_plaza[1].tipo == PL_HUMANO && red_plaza[1].listo && frame - f0 > 30) {
			if (!t_sala_ok) { t_sala_ok = 1; t_sala = SDL_GetTicks(); AOEN_LOG("[AOEN] red-auto: sala completa"); }
			if (SDL_GetTicks() - t_sala < (unsigned)(getenv("AOEN_RED_AUTO_SALA") ? atoi(getenv("AOEN_RED_AUTO_SALA")) : 0)) return;   /* tiempo para la captura de la sala */
			AOEN_LOG("[AOEN] red-auto: espera de la sala terminada (%u ms)", (unsigned)(SDL_GetTicks() - t_sala));
			red_chat("hola desde el anfitrión");
			AOEN_LOG("[AOEN] red-auto: empieza la partida (%d jugadores humanos y %d de la máquina)", 2, ias);
			if (!red_empezar()) { AOEN_LOG("[AOEN] red-auto: no se pudo empezar: %s", red_error()); app.running = 0; }
			fase = 3;
		}
		if (red_estado() == RED_ERROR) { AOEN_LOG("[AOEN] red-auto: error: %s", red_error()); app.running = 0; }
		return;
	}
	if (fase == 3 && red_estado() == RED_JUEGO) { fase = 4; red_turno_max = turnos; AOEN_LOG("[AOEN] red-auto: en juego como jugador %d", W.human + 1); }
	if (fase == 4) {
		if (red_turno() != turno_ant) { turno_ant = red_turno(); red_ordenes_azar(40); }
		if (red_turno() >= turnos) {
			AOEN_LOG("[AOEN] red-auto: FIN turno=%d suma=%08x t=%.2f nents=%d desinc=%d", red_turno(), world_suma(), W.time, W.nents, red_desincronizado());
			fase = 5; t_fin = SDL_GetTicks();
		}
	}
	if (fase == 5 && SDL_GetTicks() - t_fin > 2500) app.running = 0;   /* deja salir lo último hacia los demás */
}
