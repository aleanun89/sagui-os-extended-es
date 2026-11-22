/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Paneles de la partida (v8.5.4.7): marcador (Tab), diplomacia y tributo (F3), ayuda de teclas (F1), pausa y la pantalla
 * final con las estadísticas de cada jugador y la gráfica de la puntuación en el tiempo. */
#include "interfaz.h"
#include "ia.h"

int panel_abierto = PANEL_NINGUNO;
Rect prueba_tributo, prueba_relacion;
int marcador_modo;

extern Rect ui_modal;
extern int ui_modal_on, ui_en_modal;

static const char *nombre_corto(int p, char *b, size_t n) {
	if (p == W.human && !W.pl[p].ai) snprintf(b, n, "Tú");   /* en el modo espectador también es «Jugador 1» */
	else snprintf(b, n, "Jugador %d", p + 1);
	return b;
}

static int jugadores_por_puntos(int *ids) {
	int n = 0, sc[MAX_PLAYERS];
	for (int p = 0; p < MAX_PLAYERS; ++p) if (W.pl[p].active) { sc[p] = world_team_score(p); ids[n++] = p; }
	for (int a = 0; a < n; ++a) for (int b = a + 1; b < n; ++b) if (sc[ids[b]] > sc[ids[a]]) { int t = ids[a]; ids[a] = ids[b]; ids[b] = t; }
	return n;
}

static int num_techs(int p) { int n = 0; for (int t = 0; t < T_COUNT; ++t) n += has_tech(p, t); return n; }

/* ---------- marcador (Tab): compacto o ampliado ---------- */
void draw_marcador(int xr, int y, int ancho_max) {
	int ids[MAX_PLAYERS], n = jugadores_por_puntos(ids);
	char b[128], nb[32];
	if (marcador_modo == 0 || ancho_max < 460) {
		int w = MIN(260, ancho_max), h = 24 + n * 18, x = xr - w;
		gfx_fill_a(x, y, w, h, gfx_rgb(16, 11, 7), 205);
		bevel(x, y, w, h, COL_GOLD);
		gfx_text(x + 8, y + 7, "PUNTUACIÓN (TAB)", COL_GOLD, 1);
		for (int i = 0; i < n; ++i) {
			int p = ids[i], yy = y + 20 + i * 18;
			gfx_fill(x + 8, yy + 3, 10, 10, col_player[pcolor(p)]);
			snprintf(b, sizeof b, "%s", nombre_corto(p, nb, sizeof nb));
			u32 c = W.pl[p].alive || W.over ? COL_CREAM : gfx_rgb(120, 100, 90);
			gfx_ts(x + 24, yy, b, c, 1);
			snprintf(b, sizeof b, "%d", world_team_score(p));
			gfx_ts(x + w - 8 - gfx_t_w(b, 1), yy, b, c, 1);
			if (p != W.human) {
				const char *d = W.pl[W.human].dipl[p] == DIPL_ALLY ? "aliado" : W.pl[W.human].dipl[p] == DIPL_NEUTRAL ? "neutral" : "";
				if (d[0]) gfx_text(x + 104, yy + 5, d, W.pl[W.human].dipl[p] == DIPL_ALLY ? COL_OK : COL_DIM, 1);
			}
		}
		solape_registrar(x, y, w, h, "marcador");
		return;
	}
	/* ampliado: edad, población, militar, economía, tecnología, total y relación */
	static const char *col[] = { "Jugador", "Edad", "Pob.", "Militar", "Econ.", "Tecn.", "Total" };
	static const int cx[] = { 0, 130, 200, 248, 306, 364, 410 };
	int w = 460, h = 30 + n * 20, x = xr - w;
	gfx_fill_a(x, y, w, h, gfx_rgb(16, 11, 7), 215);
	bevel(x, y, w, h, COL_GOLD);
	for (int c = 0; c < 7; ++c) gfx_text(x + 10 + cx[c], y + 8, col[c], COL_GOLD, 1);
	static const char *an[] = { "Piedra", "Herram.", "Bronce", "Hierro" };
	for (int i = 0; i < n; ++i) {
		int p = ids[i], yy = y + 24 + i * 20, ec, mi, te;
		world_score(p, &ec, &mi, &te);
		u32 c = W.pl[p].alive || W.over ? COL_CREAM : gfx_rgb(120, 100, 90);
		gfx_fill(x + 10, yy + 3, 10, 10, col_player[pcolor(p)]);
		/* el nombre del color de la relación: aliado verde, neutral claro, enemigo rojizo */
		snprintf(b, sizeof b, "%s", nombre_corto(p, nb, sizeof nb));
		int rel = W.pl[W.human].dipl[p];
		gfx_ts(x + 26, yy, b, !W.pl[p].alive && !W.over ? c : p == W.human ? COL_AVISO : rel == DIPL_ALLY ? COL_OK : rel == DIPL_NEUTRAL ? COL_CREAM : gfx_rgb(255, 170, 150), 1);
		gfx_ts(x + 10 + cx[1], yy, an[W.pl[p].age], c, 1);
		snprintf(b, sizeof b, "%d", W.pl[p].pop); gfx_ts(x + 10 + cx[2], yy, b, c, 1);
		snprintf(b, sizeof b, "%d", mi); gfx_ts(x + 10 + cx[3], yy, b, c, 1);
		snprintf(b, sizeof b, "%d", ec); gfx_ts(x + 10 + cx[4], yy, b, c, 1);
		snprintf(b, sizeof b, "%d", te); gfx_ts(x + 10 + cx[5], yy, b, c, 1);
		snprintf(b, sizeof b, "%d", ec + mi + te); gfx_ts(x + 10 + cx[6], yy, b, COL_AVISO, 1);
	}
	solape_registrar(x, y, w, h, "marcador");
}

/* ---------- diplomacia y tributo ---------- */
static int tributo_cant = 1;
static int modo_pedir;   /* D415: los botones de recurso piden un tributo a la máquina en vez de enviarlo */
static const int tributo_val[] = { 50, 100, 200, 500 };

static void panel_diplomacia(void) {
	int ids[MAX_PLAYERS], n = 0;
	for (int p = 0; p < MAX_PLAYERS; ++p) if (W.pl[p].active && p != W.human) ids[n++] = p;
	int w = MIN(fb.w - 20, 760), rowh = 34, h = 116 + n * rowh, x = (fb.w - w) / 2, y = MAX(top_h + 6, (fb.h - bottom_h - h) / 2 + top_h / 2);
	if (y + h > fb.h - bottom_h - 4) y = MAX(top_h + 4, fb.h - bottom_h - 4 - h);
	ui_modal = (Rect){ x, y, w, h }; ui_modal_on = 1; ui_en_modal = 1;
	panel(x, y, w, h);
	gfx_ts(x + 20, y + 16, "Diplomacia y tributo", COL_GOLD, 2);
	if (button(x + w - 44, y + 14, 28, 26, "X", 1, 1)) panel_abierto = PANEL_NINGUNO;
	char b[160], nb[64];
	int hay_mercado = 0;
	for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].owner == W.human && W.ents[i].type == E_MARKET && W.ents[i].progress >= 1) hay_mercado = 1;
	/* columnas */
	int c_nom = x + 24, c_rel = x + 250, c_trib = x + 420;
	int compacto = w < 700;
	if (compacto) { c_rel = x + 200; c_trib = x + 350; }
	int yy = y + 58;
	gfx_text(c_nom, yy, "JUGADOR", COL_GOLD, 1);
	gfx_text(c_rel, yy, "RELACIÓN", COL_GOLD, 1);
	snprintf(b, sizeof b, modo_pedir ? "PEDIR TRIBUTO (%d)" : "TRIBUTO (%d, LLEGA EL 80%%)", tributo_val[tributo_cant]);
	gfx_text(c_trib, yy, b, COL_GOLD, 1);
	yy += 16;
	for (int i = 0; i < n; ++i, yy += rowh) {
		int p = ids[i];
		panel_hueco(x + 16, yy, w - 32, rowh - 4);
		gfx_fill(c_nom - 2, yy + 8, 12, 12, col_player[pcolor(p)]);
		snprintf(b, sizeof b, "%s", nombre_corto(p, nb, sizeof nb));
		u32 c = W.pl[p].alive ? COL_CREAM : gfx_rgb(130, 110, 95);
		gfx_ts(c_nom + 16, yy + 2, b, c, 1);
		if (W.pl[p].team >= 10) snprintf(b, sizeof b, "%s · sin equipo%s", civ_def[W.pl[p].civ].name, W.pl[p].alive ? "" : " · derrotado");
		else snprintf(b, sizeof b, "%s · equipo %d%s", civ_def[W.pl[p].civ].name, W.pl[p].team, W.pl[p].alive ? "" : " · derrotado");
		/* lo que hace un aliado de la máquina (ia_estado): solo de los aliados, para no regalar información del rival */
		if (W.pl[p].ai && W.pl[W.human].dipl[p] == DIPL_ALLY && W.pl[p].alive && !compacto) {
			char est[80]; snprintf(est, sizeof est, "%s", ia_estado(p));
			if (hit(c_nom, yy, c_rel - c_nom - 8, rowh - 4)) tip_simple((Rect){ c_nom, yy, c_rel - c_nom - 8, rowh - 4 }, "Tu aliado", est);
		}
		gfx_text(c_nom + 16, yy + 20, b, COL_DIM, 1);
		int rel = W.pl[W.human].dipl[p];
		if (W.pl[p].alive) {
			if (i == 0) prueba_relacion = (Rect){ c_rel, yy + 2, compacto ? 140 : 156, 26 };
			/* propuesta de la máquina (D415): dos botones en lugar del selector de relación */
			int propone = W.prop_nivel[p] && W.time <= W.prop_t[p];
			int cyc = 0;
			if (propone) {
				snprintf(b, sizeof b, "Propone: %s", dipl_name(W.prop_nivel[p] - 1));
				gfx_text(c_rel, yy + 1, b, COL_AVISO, 1);
				int bw2 = compacto ? 66 : 74;
				if (boton_t(c_rel, yy + 13, bw2, 20, "Aceptar", 1)) { cmd_responder_propuesta(p, 1); snd_ui(SND_CLICK); }
				if (boton_t(c_rel + bw2 + 4, yy + 13, bw2, 20, "Rechazar", 1)) { cmd_responder_propuesta(p, 0); snd_ui(SND_CLICK); }
			} else cyc = cycler(c_rel, yy + 2, compacto ? 140 : 156, dipl_name(rel));
			if (cyc) {
				int nuevo = CLAMP(rel + cyc, DIPL_ENEMY, DIPL_ALLY);
				if (nuevo != rel) { int ok = cmd_diplomacy(W.human, p, nuevo); AOEN_LOG("[AOEN] diplomacia: jugador %d -> %s (%s)", p + 1, dipl_name(nuevo), ok ? "aceptada" : "rechazada"); }
			}
			if (!propone && hit(c_rel, yy + 2, 156, 26)) tip_simple((Rect){ c_rel, yy + 2, 156, 26 }, "Relación", "Declarar la guerra siempre se puede. La máquina decide según su personalidad y dificultad, vuestra fuerza, los enemigos comunes, los tributos que le envías y si la traicionaste. La alianza solo si ya sois neutrales. Romper un pacto es una traición que recuerda."); 
			/* tributo: un botón por recurso */
			int bx = c_trib;
			for (int k = 0; k < RES_COUNT; ++k) {
				int bw = compacto ? 46 : 64, can = modo_pedir ? (rel != DIPL_ENEMY && W.pl[p].ai) : (hay_mercado && rel != DIPL_ENEMY && W.pl[W.human].res[k] >= tributo_val[tributo_cant]);
				if (i == 0 && k == RES_WOOD) prueba_tributo = (Rect){ bx, yy + 2, bw, 26 };
				int over = hit(bx, yy + 2, bw, 26);
				gfx_fill_grad(bx, yy + 2, bw, 26, over && can ? gfx_rgb(90, 60, 36) : gfx_rgb(46, 31, 20), gfx_rgb(22, 15, 10));
				bevel(bx, yy + 2, bw, 26, can ? COL_GOLD : gfx_rgb(105, 88, 66));
				icono_rec(k, bx + 3, yy + 7, 16);
				if (!compacto) { snprintf(b, sizeof b, "%d", tributo_val[tributo_cant]); gfx_ts(bx + 30, yy + 7, b, can ? COL_CREAM : COL_DIM, 1); }
				if (over) {
					static const char *rn[4] = { "madera", "comida", "oro", "piedra" };
					Tip t; memset(&t, 0, sizeof t);
					snprintf(t.titulo, sizeof t.titulo, modo_pedir ? "Pedir %d de %s a %s" : "Enviar %d de %s a %s", tributo_val[tributo_cant], rn[k], nombre_corto(p, nb, sizeof nb));
					if (modo_pedir) snprintf(t.desc, sizeof t.desc, "La máquina lo envía si es tu aliada (o si le has enviado tributo) y le sobra.");
					else snprintf(t.desc, sizeof t.desc, "Llegan %d: el mercado se queda el 20 %%.", tributo_val[tributo_cant] * 80 / 100);
					if (modo_pedir) { if (rel == DIPL_ENEMY) snprintf(t.motivo, sizeof t.motivo, "No se pide tributo a un enemigo"); }
					else if (!hay_mercado) snprintf(t.motivo, sizeof t.motivo, "Necesitas un mercado terminado");
					else if (rel == DIPL_ENEMY) snprintf(t.motivo, sizeof t.motivo, "No se envía tributo a un enemigo");
					else if (!modo_pedir && W.pl[W.human].res[k] < tributo_val[tributo_cant]) snprintf(t.motivo, sizeof t.motivo, "Falta %s: %d", rn[k], tributo_val[tributo_cant] - W.pl[W.human].res[k]);
					t.ancla = (Rect){ bx, yy + 2, bw, 26 };
					tip_poner(&t);
					if (in.mclick) {
						in.bloqueo = 1;
						if (modo_pedir && can) {
							int ok = cmd_pedir(W.human, p, k, tributo_val[tributo_cant]);
							AOEN_LOG("[AOEN] petición: %d de recurso %d al jugador %d (%s)", tributo_val[tributo_cant], k, p + 1, ok ? "enviado" : "rechazada");
							snd_ui(ok ? SND_CLICK : SND_ERROR);
						} else if (!modo_pedir && can && cmd_tribute(W.human, p, k, tributo_val[tributo_cant])) {
							snprintf(b, sizeof b, "Tributo: %d de %s a %s", tributo_val[tributo_cant] * 80 / 100, rn[k], nombre_corto(p, nb, sizeof nb));
							world_msg(b); snd_ui(SND_CLICK);
							AOEN_LOG("[AOEN] tributo: %d de recurso %d al jugador %d", tributo_val[tributo_cant], k, p + 1);
						} else { world_msg(t.motivo[0] ? t.motivo : "No se puede enviar"); snd_ui(SND_ERROR); }
					}
				}
				bx += bw + 4;
			}
		}
	}
	yy += 6;
	gfx_ts(x + 24, yy + 4, "Cantidad:", COL_CREAM, 1);
	snprintf(b, sizeof b, "%d", tributo_val[tributo_cant]);
	int c = cycler(x + 104, yy, 110, b);
	if (c) tributo_cant = (tributo_cant + c + ARRAY_LEN(tributo_val)) % ARRAY_LEN(tributo_val);
	if (boton_t(x + 224, yy, 104, 26, modo_pedir ? "Modo: pedir" : "Modo: enviar", 1)) modo_pedir = !modo_pedir;
	gfx_text(x + 340, yy + 9, hay_mercado ? "El tributo necesita un mercado: lo tienes." : "Para enviar tributo hace falta un mercado terminado.", hay_mercado ? COL_DIM : COL_MAL, 1);
	ui_en_modal = 0;
}

/* ---------- ayuda de teclas (F1) ---------- */
/* tabla de teclas: en dos columnas si caben (ayuda F1 y Opciones > Teclas) */
void dibujar_teclas(int x, int y, int w, int h) {
	static const char *izq[][2] = {
		{ "Clic", "elegir; arrastrar: caja" }, { "Mayús+clic", "añadir o quitar de la selección" }, { "Doble clic", "todos los de ese tipo a la vista" },
		{ "Clic derecho", "mover, atacar, recoger, construir" }, { "Mayús+clic der.", "encadenar órdenes (hasta 4)" },
		{ "Ctrl+número", "guardar un grupo" }, { "Mayús+número", "elegir el grupo" }, { "1..9, 0", "botones de la rejilla de órdenes" },
		{ "AvPág / RePág", "página de órdenes" }, { "Rueda", "acercar y alejar" }, { "Flechas, WASD", "mover la vista" },
	};
	static const char *der[][2] = {
		{ "Z X C V", "agresiva, defensiva, quieta, pasiva" }, { "G", "proteger a un aliado" }, { "P", "patrullar" },
		{ "Q", "detener" }, { "E / R", "desembarcar / embarcar" }, { "Y", "punto de reunión (o clic der.)" }, { ". / ,", "aldeano / soldado inactivo" },
		{ "Espacio", "ir al último aviso (ataque)" }, { "H", "centro urbano" }, { "Tab", "marcador: pequeño, ampliado, nada" },
		{ "F1 F3", "ayuda, diplomacia" }, { "F5 F9", "guardado rápido, cargar" }, { "+ / -", "velocidad" }, { "Esc / F10", "cancelar / pausa" },
		{ "F11", "pantalla completa" }, { "Ctrl+F", "HUD de rendimiento" },
	};
	/* ancho de la columna de las teclas y de la más larga de las descripciones: dos columnas solo si caben enteras */
	int ki = 0, di = 0, kd = 0, dd = 0;
	for (int i = 0; i < ARRAY_LEN(izq); ++i) { ki = MAX(ki, gfx_t_w(izq[i][0], 1)); di = MAX(di, gfx_t_w(izq[i][1], 1)); }
	for (int i = 0; i < ARRAY_LEN(der); ++i) { kd = MAX(kd, gfx_t_w(der[i][0], 1)); dd = MAX(dd, gfx_t_w(der[i][1], 1)); }
	ki += 8; kd += 8;
	int dos = ki + di + 4 <= w / 2 && kd + dd <= w / 2, colw = dos ? w / 2 : w;
	int max_filas = h / 18;
	for (int i = 0; i < ARRAY_LEN(izq) && i < max_filas; ++i) {
		gfx_ts(x, y + i * 18, izq[i][0], COL_AVISO, 1);
		gfx_ts(x + ki, y + i * 18, izq[i][1], COL_CREAM, 1);
	}
	int dx = dos ? x + colw : x, dy = dos ? y : y + ARRAY_LEN(izq) * 18 + 8;
	for (int i = 0; i < ARRAY_LEN(der) && (dy - y) / 18 + i < max_filas; ++i) {
		gfx_ts(dx, dy + i * 18, der[i][0], COL_AVISO, 1);
		gfx_ts(dx + (dos ? kd : ki), dy + i * 18, der[i][1], COL_CREAM, 1);
	}
}

static void panel_ayuda(void) {
	int w = MIN(fb.w - 20, 860), h = MIN(fb.h - top_h - bottom_h - 10, 26 + 40 + 16 * 18 + 30), x = (fb.w - w) / 2, y = top_h + 5;
	ui_modal = (Rect){ x, y, w, h }; ui_modal_on = 1; ui_en_modal = 1;
	panel(x, y, w, h);
	gfx_ts(x + 20, y + 14, "Ayuda: teclas y órdenes", COL_GOLD, 2);
	if (button(x + w - 44, y + 14, 28, 26, "X", 1, 1)) panel_abierto = PANEL_NINGUNO;
	dibujar_teclas(x + 20, y + 56, w - 40, h - 80);
	gfx_t(x + 20, y + h - 24, w >= 820 ? "Ayuda completa en /ayuda/aoen.md. Los botones grises dicen en su ayuda por qué no se pueden usar." : "Ayuda completa en /ayuda/aoen.md.", COL_DIM, 1);
	ui_en_modal = 0;
}

/* el ratón está sobre un panel abierto (diplomacia, ayuda): no llega al mapa */
int panel_modal(void) {
	int s = app.us;
	return ui_modal_on && panel_abierto != PANEL_NINGUNO && in.mx >= ui_modal.x * s && in.my >= ui_modal.y * s && in.mx < (ui_modal.x + ui_modal.w) * s && in.my < (ui_modal.y + ui_modal.h) * s;
}

void paneles_cerrar(void) { panel_abierto = PANEL_NINGUNO; ui_modal_on = 0; }

void draw_panel_abierto(void) {
	ui_modal_on = 0;
	if (panel_abierto == PANEL_DIPLOMACIA) panel_diplomacia();
	else if (panel_abierto == PANEL_AYUDA) panel_ayuda();
}

/* ---------- gráfica de la puntuación en el tiempo (una muestra cada 15 s) ---------- */
void grafica_puntos(int x, int y, int w, int h) {
	panel_hueco(x, y, w, h);
	int maxn = 0, maxv = 10;
	for (int p = 0; p < MAX_PLAYERS; ++p) if (W.pl[p].active) { maxn = MAX(maxn, W.pl[p].hist_n); for (int k = 0; k < W.pl[p].hist_n; ++k) maxv = MAX(maxv, W.pl[p].hist[k]); }
	/* escala redonda */
	int paso = 1;
	for (int m = 1; maxv / paso > 5; m *= 10) {
		if (maxv / (2 * m) <= 5) paso = 2 * m;
		else if (maxv / (5 * m) <= 5) paso = 5 * m;
		else paso = 10 * m;
	}
	int top = ((maxv + paso - 1) / paso) * paso;
	int gx = x + 44, gy = y + 10, gw = w - 56, gh = h - 34;
	char b[32];
	for (int v = 0; v <= top; v += paso) {
		int yy = gy + gh - (int)((long long)gh * v / top);
		gfx_fill(gx, yy, gw, 1, gfx_rgb(60, 46, 32));
		snprintf(b, sizeof b, "%d", v);
		gfx_text(gx - 6 - gfx_text_w(b, 1), yy - 3, b, COL_DIM, 1);
	}
	int mins = maxn * 15 / 60;
	for (int m = 0; m <= mins; m += MAX(1, mins / 6 + (mins > 30 ? 4 : 0))) {
		int xx = gx + (maxn > 1 ? (int)((long long)gw * (m * 4) / (maxn - 1)) : 0);
		if (xx > gx + gw) break;
		snprintf(b, sizeof b, "%d min", m);
		gfx_text(xx - gfx_text_w(b, 1) / 2, gy + gh + 8, b, COL_DIM, 1);
	}
	gfx_fill(gx, gy + gh, gw, 1, COL_GOLD);
	gfx_fill(gx, gy, 1, gh, COL_GOLD);
	if (maxn < 2) { gfx_ts(gx + 20, gy + gh / 2 - 8, "Aún no hay datos (una muestra cada 15 s)", COL_DIM, 1); return; }
	for (int p = 0; p < MAX_PLAYERS; ++p) {
		if (!W.pl[p].active || W.pl[p].hist_n < 2) continue;
		u32 c = col_player[pcolor(p)];
		int off = maxn - W.pl[p].hist_n;   /* quien empezó a contar más tarde (no pasa) se alinea a la derecha */
		for (int k = 1; k < W.pl[p].hist_n; ++k) {
			int x0 = gx + (int)((long long)gw * (k - 1 + off) / (maxn - 1)), x1 = gx + (int)((long long)gw * (k + off) / (maxn - 1));
			int y0 = gy + gh - (int)((long long)gh * W.pl[p].hist[k - 1] / top), y1 = gy + gh - (int)((long long)gh * W.pl[p].hist[k] / top);
			gfx_line(x0, y0, x1, y1, c);
			gfx_line(x0, y0 - 1, x1, y1 - 1, c);
			if (p == W.human) gfx_line(x0, y0 + 1, x1, y1 + 1, c);
		}
	}
}

/* ---------- pantalla final ---------- */
int final_pest;

void draw_endscreen(void) {
	gfx_fill_a(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0), 160);
	int w = MIN(fb.w - 16, 860), h = MIN(fb.h - 16, 520), x = (fb.w - w) / 2, y = (fb.h - h) / 2;
	panel(x, y, w, h);
	/* W.over dice si ganó el equipo del jugador 0 (1) o no (2). Con el modo espectador ese jugador también lo lleva la
	 * máquina: entonces no hay «victoria» ni «derrota» propias, se dice quién gana. El ganador es el equipo del jugador 0
	 * o, si perdió, el rival vivo con más puntos (el que manda en su equipo). */
	char b[160], nb[32], gan[96];
	int espectador = W.pl[W.human].ai, ganador = W.human;
	int nganan = 0;
	for (int p = 0; p < MAX_PLAYERS; ++p) if ((W.ganadores >> p) & 1u) ++nganan;
	if (W.ganadores) {
		/* el bando ganador lo dice el juego (terminar): se nombra al primero vivo de él */
		ganador = -1;
		for (int p = 0; p < MAX_PLAYERS && ganador < 0; ++p) if (((W.ganadores >> p) & 1u) && W.pl[p].alive) ganador = p;
		for (int p = 0; p < MAX_PLAYERS && ganador < 0; ++p) if ((W.ganadores >> p) & 1u) ganador = p;
	} else if (W.over != 1) {
		int bs = -1;
		ganador = -1;
		for (int p = 0; p < MAX_PLAYERS; ++p)
			if (W.pl[p].active && W.pl[p].alive && !same_team(p, W.human)) { int s = world_team_score(p); if (s > bs) { bs = s; ganador = p; } }
	}
	if (ganador < 0) snprintf(gan, sizeof gan, "Nadie queda en pie");
	else {
		int aliados = 0;
		for (int p = 0; p < MAX_PLAYERS; ++p) if (p != ganador && W.pl[p].active && (W.ganadores ? (W.ganadores >> p) & 1u : same_team(p, ganador))) ++aliados;
		snprintf(gan, sizeof gan, "Gana: %s%s", nombre_corto(ganador, nb, sizeof nb), aliados ? " y sus aliados" : "");
	}
	/* con varios ganadores (equipo o aliados), el humano del bando ganador ve «VICTORIA (compartida)» */
	const char *tit = espectador ? "PARTIDA TERMINADA" : W.over == 1 ? (nganan > 1 ? "VICTORIA (compartida)" : "VICTORIA") : "DERROTA";
	u32 ctit = espectador ? COL_GOLD : W.over == 1 ? gfx_rgb(130, 255, 130) : gfx_rgb(255, 110, 90);
	gfx_ts(x + w / 2 - gfx_t_w(tit, 2) / 2, y + 12, tit, ctit, 2);
	int t = (int)W.time;
	static const char *vic[] = { "Estándar", "Conquista", "Maravilla", "Reliquias", "Puntuación" };
	snprintf(b, sizeof b, "%s · %02d:%02d · victoria: %s", gan, t / 60, t % 60, vic[CLAMP(W.setup.victory, 0, VIC_COUNT - 1)]);
	gfx_ts(x + w / 2 - gfx_t_w(b, 1) / 2, y + 50, b, COL_CREAM, 1);
	static const char *pest[] = { "Resumen", "Militar", "Economía", "Tecnología", "Gráfica" };
	int pw = (w - 40) / 5;
	for (int k = 0; k < 5; ++k) if (button(x + 20 + k * pw, y + 76, pw - 6, 28, pest[k], k != final_pest, 1)) final_pest = k;
	gfx_fill(x + 20 + final_pest * pw, y + 104, pw - 6, 3, COL_GOLD);
	if (tecla(SDL_SCANCODE_RIGHT)) final_pest = (final_pest + 1) % 5;
	if (tecla(SDL_SCANCODE_LEFT)) final_pest = (final_pest + 4) % 5;
	int ids[MAX_PLAYERS], n = jugadores_por_puntos(ids);
	int ty = y + 118, th = h - 118 - 64;
	if (final_pest == 4) {
		grafica_puntos(x + 20, ty, w - 40, th - 22);
		int lx = x + 24;
		for (int i = 0; i < n; ++i) {
			int p = ids[i];
			gfx_fill(lx, ty + th - 14, 14, 4, col_player[pcolor(p)]);
			nombre_corto(p, nb, sizeof nb);
			gfx_text(lx + 18, ty + th - 16, nb, COL_CREAM, 1);
			lx += 30 + gfx_text_w(nb, 1);
		}
	} else {
		static const char *cab[4][6] = {
			{ "Puntos", "Militar", "Economía", "Tecnología", "Edad", "Estado" },
			{ "Bajas causadas", "Unidades perdidas", "Edif. destruidos", "Edif. perdidos", "", "" },
			{ "Madera", "Comida", "Oro", "Piedra", "Total", "" },
			{ "Mejoras", "Edad", "Población", "", "", "" },
		};
		int ncol = 0;
		for (int c = 0; c < 6; ++c) if (cab[final_pest][c][0]) ncol = c + 1;
		int cx0 = x + 190, cw = (w - 210) / MAX(1, ncol);
		panel_hueco(x + 20, ty, w - 40, th);
		for (int c = 0; c < 6; ++c) if (cab[final_pest][c][0]) gfx_text(cx0 + c * cw, ty + 8, cab[final_pest][c], COL_GOLD, 1);
		static const char *an[] = { "Piedra", "Herramientas", "Bronce", "Hierro" };
		for (int i = 0; i < n; ++i) {
			int p = ids[i], yy = ty + 26 + i * 34, ec, mi, te;
			if (yy + 18 > ty + th) break;
			world_score(p, &ec, &mi, &te);
			Player *pl = &W.pl[p];
			gfx_fill(x + 30, yy + 3, 12, 12, col_player[pcolor(p)]);
			snprintf(b, sizeof b, "%s", nombre_corto(p, nb, sizeof nb));
			gfx_ts(x + 48, yy, b, p == W.human ? COL_AVISO : COL_CREAM, 1);
			gfx_text(x + 48, yy + 17, civ_def[pl->civ].name, COL_DIM, 1);
			char v[6][24];
			memset(v, 0, sizeof v);
			if (final_pest == 0) {
				snprintf(v[0], 24, "%d", ec + mi + te); snprintf(v[1], 24, "%d", mi); snprintf(v[2], 24, "%d", ec); snprintf(v[3], 24, "%d", te);
				snprintf(v[4], 24, "%s", an[pl->age]); snprintf(v[5], 24, "%s", (W.ganadores >> p) & 1u ? "Victoria" : pl->alive ? "Sobrevive" : "Derrotado");
			} else if (final_pest == 1) {
				snprintf(v[0], 24, "%d", pl->units_killed); snprintf(v[1], 24, "%d", pl->units_lost); snprintf(v[2], 24, "%d", pl->bld_razed); snprintf(v[3], 24, "%d", pl->bld_lost);
			} else if (final_pest == 2) {
				int tot = 0;
				for (int k = 0; k < 4; ++k) { snprintf(v[k], 24, "%d", pl->gathered_k[k]); tot += pl->gathered_k[k]; }
				snprintf(v[4], 24, "%d", tot);
			} else {
				snprintf(v[0], 24, "%d de %d", num_techs(p), T_COUNT); snprintf(v[1], 24, "%s", an[pl->age]); snprintf(v[2], 24, "%d", pl->pop);
			}
			for (int c = 0; c < 6; ++c) if (v[c][0]) gfx_ts(cx0 + c * cw, yy + 4, v[c], c == 5 && !pl->alive ? COL_MAL : COL_CREAM, 1);
		}
	}
	if (button(x + w / 2 - 130, y + h - 54, 260, 40, "Volver al menú", 1, 2)) { world_free(); app.screen = SC_MENU; }
	static int dicho;
	if (!dicho) { dicho = 1; AOEN_LOG("[AOEN] pantalla final: %s, %d jugadores, %d muestras en la gráfica; %s", tit, n, W.pl[W.human].hist_n, gan); }
}

/* ---------- pausa ---------- */
void draw_pause(void) {
	gfx_fill_a(0, 0, fb.w, fb.h, gfx_rgb(0, 0, 0), 140);
	int pw = 360, bh = 40, gap = 10, ph = 70 + 6 * (bh + gap) + 10, px = (fb.w - pw) / 2, py = (fb.h - ph) / 2;
	panel(px, py, pw, ph);
	gfx_ts(fb.w / 2 - gfx_t_w("Pausa", 2) / 2, py + 18, "Pausa", COL_GOLD, 2);
	int y = py + 64;
	foco_inicio();
	if (button(px + 40, y, pw - 80, bh, "Continuar", 1, 2) || tecla(SDL_SCANCODE_ESCAPE)) app.paused = 0;
	y += bh + gap;
	if (button(px + 40, y, pw - 80, bh, "Guardar partida", 1, 2)) { refresh_slots(); saves_msg[0] = 0; app.saves_mode = 1; app.saves_back = SC_GAME; app.screen = SC_SAVES; }
	y += bh + gap;
	if (button(px + 40, y, pw - 80, bh, "Cargar partida", any_save(), 2)) { refresh_slots(); saves_msg[0] = 0; app.saves_mode = 0; app.saves_back = SC_GAME; app.screen = SC_SAVES; }
	y += bh + gap;
	if (button(px + 40, y, pw - 80, bh, "Opciones", 1, 2)) { app.options_back = SC_GAME; app.screen = SC_OPTIONS; }
	y += bh + gap;
	if (button(px + 40, y, pw - 80, bh, "Abandonar partida", 1, 2)) { world_free(); app.paused = 0; app.screen = SC_MENU; }
	y += bh + gap;
	if (button(px + 40, y, pw - 80, bh, "Salir del juego", 1, 2)) app.running = 0;
}
