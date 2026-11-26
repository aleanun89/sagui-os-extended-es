/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Guiones de prueba de la interfaz (AOEN_PRUEBA=nombre, v8.5.4.7): simulan el ratón y el teclado cuadro a cuadro, como
 * los usaría un jugador, y dejan en el registro lo que pasó. Los usa tests/h389_aoen.py (parte «interfaz»). Sin
 * AOEN_PRUEBA no hacen nada. */
#include "interfaz.h"
#include "ia.h"
#include "config.h"

static Uint8 teclas_falsas[512];
int prueba_activa;   /* hay guion: el ratón simulado cuenta como dentro de la ventana */

static int centro_humano(float *x, float *y) {
	for (int i = 0; i < W.nents; ++i)
		if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER && W.ents[i].owner == W.human) { *x = W.ents[i].x; *y = W.ents[i].y; return i; }
	return -1;
}

static void raton_en_mundo(float wx, float wy) { view_to_screen(wx, wy, &in.mx, &in.my); in.mouse_seen = 1; }

static void con_mayus(void) {
	if (in.keys) memcpy(teclas_falsas, in.keys, sizeof teclas_falsas);
	teclas_falsas[SDL_SCANCODE_LSHIFT] = 1;
	in.keys = teclas_falsas;
}

static void soldados(int n, float x, float y) {
	select_none();
	for (int k = 0; k < n; ++k) {
		int i = dev_place(E_CLUBMAN, W.human, x + (k % 2) * 0.8f, y + (k / 2) * 0.8f);
		if (i >= 0) W.ents[i].sel = 1;
	}
}

void prueba_paso(int f) {
	static const char *p;
	static int leido;
	if (!leido) { leido = 1; p = getenv("AOEN_PRUEBA"); if (p && !*p) p = NULL; if (p) AOEN_LOG("[AOEN] prueba de interfaz: %s", p); prueba_activa = p != NULL; }
	if (!p) return;
	float cx = 0, cy = 0;
	int tc = W.nents ? centro_humano(&cx, &cy) : -1;

	if (!strcmp(p, "teclado")) {
		/* configuración de la partida solo con el teclado: bajar hasta la civilización del jugador 1 y cambiarla */
		if (f >= 4 && f <= 7) in.key[SDL_SCANCODE_DOWN] = 1;
		if (f == 9) in.key[SDL_SCANCODE_RIGHT] = 1;
		if (f == 12) AOEN_LOG("[AOEN] prueba teclado: civilización del jugador 1 = %s, foco %d", civ_def[su.civ[0]].name, foco_actual);
		return;
	}
	if (!strcmp(p, "fps") && app.screen == SC_OPTIONS) {
		/* D476: dos clics en la flecha derecha del selector «Límite de FPS»: 60 -> 30 -> ilimitado; queda guardado */
		if (f == 4 || f == 8) { in.mx = (prueba_fps.x + prueba_fps.w - 10) * app.us; in.my = (prueba_fps.y + 10) * app.us; in.mclick = 1; in.mouse_seen = 1; }
		if (f == 6 || f == 10) AOEN_LOG("[AOEN] prueba fps: fps_max=%d guardado=%d", app.fps_max, cfg_get_int("fps_max", -1));
		return;
	}
	if (!strcmp(p, "comercio") && app.screen == SC_SETUP) {
		/* configuración: clic en la casilla «Comercio mejorado» y empezar con Intro */
		if (f == 4) { in.mx = (prueba_comercio.x + 8) * app.us; in.my = (prueba_comercio.y + 8) * app.us; in.mclick = 1; }
		if (f == 6) AOEN_LOG("[AOEN] prueba comercio: casilla=%d", su.comercio);
		if (f == 8) in.key[SDL_SCANCODE_RETURN] = 1;
		return;
	}
	if (!strcmp(p, "fuego") && app.screen == SC_SETUP) {
		/* D447: clic en la casilla «Fuego amigo» de la configuración y empezar con Intro */
		if (f == 4) { in.mx = (prueba_fuego.x + 8) * app.us; in.my = (prueba_fuego.y + 8) * app.us; in.mclick = 1; }
		if (f == 6) AOEN_LOG("[AOEN] prueba fuego: casilla=%d", su.fuego_amigo);
		if (f == 8) in.key[SDL_SCANCODE_RETURN] = 1;
		return;
	}
	if (tc < 0 || app.screen != SC_GAME) return;

	if (!strcmp(p, "fuego")) {
		static int g0 = -1;
		if (g0 < 0) g0 = f;
		if (f == g0 + 6) AOEN_LOG("[AOEN] prueba fuego: partida fuego_amigo=%d motor=%d", W.setup.fuego_amigo, fuego_amigo);
		return;
	}

	if (!strcmp(p, "comercio")) {
		/* un carro mercante elegido: I (modo intercambio), clic en «Dar», M (recibir) y la ayuda del botón del modo */
		static int f0 = -1;
		if (f0 < 0) f0 = f;
		int g = f - f0;
		if (g == 3) {
			select_none();
			int i = dev_place(E_TRADECART, W.human, cx + 3, cy + 4);
			if (i >= 0) W.ents[i].sel = 1;
		}
		if (g == 6) in.key[SDL_SCANCODE_I] = 1;
		if (g == 9) hud_orden_pos(3, &in.mx, &in.my);
		if (g == 10) in.mclick = 1;
		if (g == 13) in.key[SDL_SCANCODE_M] = 1;
		if (g == 16) {
			for (int i = 0; i < W.nents; ++i)
				if (W.ents[i].alive && W.ents[i].sel && is_trader(W.ents[i].type)) {
					AOEN_LOG("[AOEN] prueba comercio: mejorado=%d modo=%d dar=%d recibir=%d", W.setup.comercio_mejorado, W.ents[i].com_modo, W.ents[i].com_dar, W.ents[i].com_recibir);
					break;
				}
		}
		if (g >= 18) hud_orden_pos(2, &in.mx, &in.my);   /* la ayuda de «Modo intercambio» */
		return;
	}

	if (!strcmp(p, "ayuda")) {
		/* ayuda emergente de la rejilla: el centro urbano entrena aldeanos (coste) y su botón de edad está gris (motivo) */
		if (f == 4) { select_none(); W.ents[tc].sel = 1; }
		if (f >= 6 && f < 14) hud_orden_pos(0, &in.mx, &in.my);
		if (f >= 14) hud_orden_pos(1, &in.mx, &in.my);
	} else if (!strcmp(p, "reunion")) {
		if (f == 4) { select_none(); W.ents[tc].sel = 1; }
		if (f == 7) { raton_en_mundo(cx + 5, cy + 3); in.rclick = 1; }
		if (f == 9) hud_orden_pos(0, &in.mx, &in.my);
		if (f == 10) { in.mclick = 1; }   /* entrena un aldeano: irá a la bandera */
		if (f > 11) raton_en_mundo(cx + 2, cy - 5);
	} else if (!strcmp(p, "cola")) {
		if (f == 3) soldados(4, cx + 3, cy + 4);
		static const float px[3] = { 7, 9, 4 }, py[3] = { 5, 1, -2 };
		for (int k = 0; k < 3; ++k) if (f == 6 + 3 * k) { con_mayus(); raton_en_mundo(cx + px[k], cy + py[k]); in.rclick = 1; }
		if (f == 20) {
			for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel) { AOEN_LOG("[AOEN] prueba cola: la unidad tiene %d puntos de ruta pendientes", W.ents[i].nwp + (W.ents[i].state == ST_MOVE)); break; }
		}
		if (f > 20) raton_en_mundo(cx - 3, cy - 3);
	} else if (!strcmp(p, "patrulla")) {
		if (f == 3) soldados(3, cx + 3, cy + 4);
		if (f == 6) in.key[SDL_SCANCODE_P] = 1;
		if (f == 9) { raton_en_mundo(cx + 8, cy + 2); in.mclick = 1; }
		if (f == 30) for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel) { AOEN_LOG("[AOEN] prueba patrulla: patrulla=%d", W.ents[i].patrol); break; }
	} else if (!strcmp(p, "alerta")) {
		/* un hachero enemigo junto a un aldeano propio: «te atacan», marca en el minimapa y Espacio para ir */
		if (f == 3) {
			for (int i = 0; i < W.nents; ++i)
				if (W.ents[i].alive && W.ents[i].owner == W.human && W.ents[i].type == E_VILLAGER) {
					int a = dev_place(E_AXEMAN, 1, W.ents[i].x + 1.0f, W.ents[i].y);
					if (a >= 0) cmd_target(ent_ref(a), ent_ref(i));
					break;
				}
			view_center_on(cx - 12, cy + 10);   /* la vista lejos: Espacio debe traerla */
		}
		if (f == 70 || f == 100) in.key[SDL_SCANCODE_SPACE] = 1;
	} else if (!strcmp(p, "diplomacia")) {
		if (f == 3) {
			dev_place(E_MARKET, W.human, cx + 6, cy - 2);
			ia_tributo(1, W.human, RES_GOLD, 400);   /* D415: la máquina ya no acepta la paz sin más: antes recibe un tributo y confía */
		}
		if (f == 5) in.key[SDL_SCANCODE_F3] = 1;
		if (f == 8) { in.mx = (prueba_relacion.x + prueba_relacion.w - 10) * app.us; in.my = (prueba_relacion.y + 12) * app.us; in.mclick = 1; }
		if (f == 12) { in.mx = (prueba_tributo.x + 10) * app.us; in.my = (prueba_tributo.y + 10) * app.us; in.mclick = 1; }
		if (f == 14) AOEN_LOG("[AOEN] prueba diplomacia: relación con el jugador 2 = %s, madera de 2 = %d", dipl_name(W.pl[W.human].dipl[1]), W.pl[1].res[RES_WOOD]);
		if (f > 14) { in.mx = (prueba_tributo.x + 10) * app.us; in.my = (prueba_tributo.y + 10) * app.us; }
	} else if (!strcmp(p, "cursor")) {
		/* v8.5.5.3: el cursor según lo que hay debajo: soldados sobre un enemigo (atacar), un aldeano sobre oro (extraer),
		 * una casa que no cabe (prohibido) y al final el rótulo de la mina bajo el ratón */
		static int c_ataque = -1, c_oro = -1, c_obra = -1, enemigo = -1, mina = -1;
		int dx, dy;
		if (f == 3) {
			soldados(3, cx + 3, cy + 4);
			enemigo = dev_place(E_AXEMAN, 1, cx + 7, cy + 1);
			mina = dev_place(E_GOLD, -1, cx - 4, cy + 4);
			if (mina >= 0) W.ents[mina].amount = 400;
		}
		if (f >= 5 && f < 10 && enemigo >= 0 && W.ents[enemigo].alive) raton_en_mundo(W.ents[enemigo].x, W.ents[enemigo].y - 0.3f);
		if (f == 9) c_ataque = hud_cursor(&dx, &dy);
		if (f == 11) {
			select_none();
			for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].owner == W.human && W.ents[i].type == E_VILLAGER) { W.ents[i].sel = 1; break; }
			if (enemigo >= 0) W.ents[enemigo].alive = 0;   /* que no estorbe a la captura */
		}
		if (f >= 12 && f < 16 && mina >= 0) raton_en_mundo(W.ents[mina].x, W.ents[mina].y - 0.3f);
		if (f == 15) c_oro = hud_cursor(&dx, &dy);
		if (f == 17) { placing = E_HOUSE; raton_en_mundo(cx, cy); }   /* encima del centro urbano: no cabe */
		if (f == 19) { c_obra = hud_cursor(&dx, &dy); placing = E_TYPE_COUNT; }
		if (f == 20) AOEN_LOG("[AOEN] prueba cursor: ataque=%d oro=%d obra=%d (atacar=%d oro=%d prohibido=%d)", c_ataque, c_oro, c_obra, CUR_ATACAR, CUR_ORO, CUR_PROHIBIDO);
		if (f >= 21 && mina >= 0) raton_en_mundo(W.ents[mina].x, W.ents[mina].y - 0.3f);
	} else if (!strcmp(p, "teclas")) {
		if (f == 5) in.key[SDL_SCANCODE_F1] = 1;   /* la ayuda de teclas */
	} else if (!strcmp(p, "pausa")) {
		if (f == 5) { in.key[SDL_SCANCODE_ESCAPE] = 1; AOEN_LOG("[AOEN] prueba pausa: Esc"); }
		if (f == 8) AOEN_LOG("[AOEN] prueba pausa: pausa=%d", app.paused);
	} else if (!strcmp(p, "marcador")) {
		if (f == 5) in.key[SDL_SCANCODE_TAB] = 1;   /* el ampliado */
	} else if (!strcmp(p, "seleccion")) {
		/* varios tipos: retratos agrupados; clic en uno filtra la selección */
		if (f == 3) {
			soldados(3, cx + 3, cy + 4);
			for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].owner == W.human && W.ents[i].type == E_VILLAGER) W.ents[i].sel = 1;
		}
		if (f >= 5) { in.mx = (12 + 6 + 19) * app.us; in.my = (gfx_real_h() / app.us - bottom_h + 8 + 24 + 19) * app.us; }
	}
}
