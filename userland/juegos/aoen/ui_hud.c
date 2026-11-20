/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Interfaz de la partida (v8.5.4.7): barra superior, barra inferior (ficha de la selección, rejilla de órdenes con ayudas,
 * minimapa con sus botones), avisos, cuentas atrás de la victoria y las órdenes con el ratón y el teclado.
 *
 * Disposición (coordenadas lógicas, de 640x480 a 960x540 con la interfaz x2 a 1920x1080):
 *   barra superior (32): recursos | población | edad | reloj | puntuación, diplomacia, ayuda, menú
 *   barra inferior (150 si caben 3 filas de órdenes, si no 118): ficha | rejilla de órdenes | minimapa (rombo) con los
 *   botones de inactivos y de filtros en las esquinas que deja libres el rombo.
 * Nada se superpone: cada bloque se mide a partir del ancho lógico y lo comprueba AOEN_SOLAPES=1. */
#include "interfaz.h"
#include "efectos.h"

int top_h = 32, bottom_h = 150;
EntType placing = E_TYPE_COUNT;
int selecting, sel_x0, sel_y0;

typedef struct {
	Rect sup, inf, sel, ord, mm;
	int bsz, gap, filas, cols;
	Rect inactivo_ald, inactivo_mil, filtro_rec, filtro_mil;
} Disposicion;
static Disposicion L;

/* zonas de la interfaz del cuadro anterior: el ratón ahí no selecciona ni da órdenes en el mapa */
static Rect zonas[24], zonas_prev[24];
static int nzonas, nzonas_prev;
static void zona(int x, int y, int w, int h) { if (nzonas < ARRAY_LEN(zonas)) { Rect r = { x, y, w, h }; zonas[nzonas++] = r; } }
static int en_interfaz(void) {
	int s = app.us;
	if (in.my < top_h * s || in.my >= gfx_real_h() - bottom_h * s) return 1;
	for (int i = 0; i < nzonas_prev; ++i) {
		Rect r = zonas_prev[i];
		if (in.mx >= r.x * s && in.my >= r.y * s && in.mx < (r.x + r.w) * s && in.my < (r.y + r.h) * s) return 1;
	}
	return panel_modal();
}

/* modos de orden: el siguiente clic en el mapa elige el destino */
enum { MODO_NADA, MODO_PATRULLA, MODO_REUNION, MODO_DESEMBARCAR, MODO_EMBARCAR, MODO_PROTEGER };
static int modo;
static int pagina;
static EntType pagina_tipo = E_TYPE_COUNT;

void hud_disponer(void) {
	int lw = gfx_real_w() / app.us, lh = gfx_real_h() / app.us;
	top_h = 32;
	L.filas = lh >= 600 ? 3 : 2;
	bottom_h = L.filas == 3 ? 150 : 118;
	L.sup = (Rect){ 0, 0, lw, top_h };
	L.inf = (Rect){ 0, lh - bottom_h, lw, bottom_h };
	int mh = bottom_h - 14, mw = 2 * mh;
	if (mw > lw * 30 / 100) { mw = lw * 30 / 100; mh = mw / 2; }
	L.mm = (Rect){ lw - mw - 10, L.inf.y + (bottom_h - mh) / 2 + 1, mw, mh };
	int sw = CLAMP(lw * 30 / 100, 214, 330);
	L.sel = (Rect){ 8, L.inf.y + 8, sw, bottom_h - 14 };
	int ox = L.sel.x + L.sel.w + 10;
	L.ord = (Rect){ ox, L.inf.y + 6, L.mm.x - 12 - ox, bottom_h - 10 };
	L.gap = 4;
	L.bsz = MIN(44, (L.ord.h - 16 - (L.filas - 1) * L.gap) / L.filas);
	L.cols = MAX(1, (L.ord.w + L.gap) / (L.bsz + L.gap));
	int bs = mh >= 110 ? 26 : 22;
	L.inactivo_ald = (Rect){ L.mm.x - 2, L.mm.y - 2, bs, bs };
	L.inactivo_mil = (Rect){ L.mm.x - 2, L.mm.y + L.mm.h - bs + 2, bs, bs };
	L.filtro_rec = (Rect){ L.mm.x + L.mm.w - bs + 2, L.mm.y - 2, bs, bs };
	L.filtro_mil = (Rect){ L.mm.x + L.mm.w - bs + 2, L.mm.y + L.mm.h - bs + 2, bs, bs };
}

/* ---------- selección ---------- */

int first_selected(void) {
	for (int i = 0; i < W.nents; ++i)
		if (W.ents[i].alive && W.ents[i].sel) return i;
	return -1;
}

void select_none(void) {
	for (int i = 0; i < W.nents; ++i) W.ents[i].sel = 0;
}

static int es_militar(EntType t) { return is_unit(t) && t != E_VILLAGER && t != E_FISHBOAT && t != E_TRANSPORT && !is_trader(t) && t < E_HORSE; }

typedef struct { int n, ntipos, tipo[24], cuenta[24], primero[24], propias, militares, aldeanos, transportes, navales; } ResumenSel;
static ResumenSel RS;

static void resumir_seleccion(void) {
	memset(&RS, 0, sizeof RS);
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!e->alive || !e->sel) continue;
		if (e->state == ST_DYING || e->state == ST_ABOARD) { e->sel = 0; continue; }   /* los pasajeros no se mandan */
		++RS.n;
		if (e->owner == W.human && is_unit(e->type)) {
			++RS.propias;
			if (e->type == E_VILLAGER) ++RS.aldeanos;
			else if (es_militar(e->type)) ++RS.militares;
			if (e->type == E_TRANSPORT) ++RS.transportes;
			if (is_naval(e->type)) ++RS.navales;
		}
		int k = 0;
		while (k < RS.ntipos && RS.tipo[k] != (int)e->type) ++k;
		if (k == RS.ntipos && k < ARRAY_LEN(RS.tipo)) { RS.tipo[k] = e->type; RS.cuenta[k] = 0; RS.primero[k] = i; ++RS.ntipos; }
		if (k < RS.ntipos) ++RS.cuenta[k];
	}
}

static int pick_entity(float wx, float wy) {
	int best = -1; float bd = 1e9f;
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!e->alive || e->state == ST_DYING || e->state == ST_ABOARD) continue;
		int fx = CLAMP((int)floorf(e->x), 0, W.w - 1), fy = CLAMP((int)floorf(e->y), 0, W.h - 1);
		u8 f = W.fog[fy * W.w + fx];
		if (!f || (is_unit(e->type) && !same_team(e->owner, W.human) && f < 2)) continue;
		float d;
		if (is_unit(e->type)) {
			float dx = e->x - wx, dy = e->y - (wy + 0.3f);
			d = sqrtf(dx * dx + dy * dy);
			if (d > 0.7f) continue;
			d -= 10;   /* las unidades tienen prioridad sobre los edificios */
		} else {
			if (wx < e->bx || wx >= e->bx + e->size || wy < e->by || wy >= e->by + e->size) continue;
			d = 0;
		}
		if (d < bd) { bd = d; best = i; }
	}
	return best;
}

static void select_box(int x0, int y0, int x1, int y1, int anadir) {
	if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
	if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
	if (!anadir) select_none();
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!e->alive || e->owner != W.human || !is_unit(e->type) || e->state == ST_DYING || e->state == ST_ABOARD) continue;
		int sx, sy;
		view_to_screen(e->x, e->y, &sx, &sy);
		sy -= (int)(14 * V.zoom);
		if (sx >= x0 && sx <= x1 && sy >= y0 && sy <= y1) e->sel = 1;
	}
}

/* todas las unidades propias de ese tipo que se ven en pantalla (doble clic) */
static void seleccionar_tipo_visible(EntType t) {
	select_none();
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!e->alive || e->owner != W.human || e->type != t || e->state == ST_DYING || e->state == ST_ABOARD) continue;
		int sx, sy;
		view_to_screen(e->x, e->y, &sx, &sy);
		if (sx >= 0 && sy >= top_h * app.us && sx < gfx_real_w() && sy < gfx_real_h() - bottom_h * app.us) e->sel = 1;
	}
}

static int inactivo(const Entity *e, int militar) {
	if (!e->alive || e->owner != W.human || e->state != ST_IDLE || e->nwp || e->patrol) return 0;
	return militar ? es_militar(e->type) : e->type == E_VILLAGER;
}

static int contar_inactivos(int militar) {
	int n = 0;
	for (int i = 0; i < W.nents; ++i) n += inactivo(&W.ents[i], militar);
	return n;
}

static void siguiente_inactivo(int militar) {
	static int last[2] = { -1, -1 };
	int *l = &last[militar ? 1 : 0];
	for (int k = 1; k <= W.nents; ++k) {
		int i = (*l + k) % W.nents;
		if (inactivo(&W.ents[i], militar)) {
			*l = i;
			select_none();
			W.ents[i].sel = 1;
			view_center_on(W.ents[i].x, W.ents[i].y);
			return;
		}
	}
	world_msg(militar ? "No hay soldados inactivos" : "No hay aldeanos inactivos");
}

/* ---------- avisos ---------- */

typedef struct { int icono; char texto[96]; u32 col; float x, y, t; } Aviso;
static Aviso avisos[6];
static int navisos;
static float ult_x = -1, ult_y = -1;   /* posición del último aviso con sitio (Espacio) */

void aviso(int ic, const char *texto, u32 col, float x, float y) {
	if (navisos == ARRAY_LEN(avisos)) { memmove(avisos, avisos + 1, sizeof(Aviso) * (ARRAY_LEN(avisos) - 1)); --navisos; }
	Aviso *a = &avisos[navisos++];
	a->icono = ic; snprintf(a->texto, sizeof a->texto, "%s", texto); a->col = col; a->x = x; a->y = y; a->t = 0;
	if (x >= 0) { ult_x = x; ult_y = y; }
	AOEN_LOG("[AOEN] aviso: %s", texto);
}

int saltar_aviso(void) {
	if (ult_x < 0) { world_msg("No hay avisos con lugar"); return 0; }
	view_center_on(ult_x, ult_y);
	AOEN_LOG("[AOEN] salta al aviso en %.0f,%.0f", ult_x, ult_y);
	return 1;
}

/* lo que se vigila para avisar: edades, mejoras, obras terminadas, población, diplomacia y derrotas */
static struct { int edad[MAX_PLAYERS]; unsigned techs; int lleno, vivo[MAX_PLAYERS], dipl[MAX_PLAYERS], npings, maravilla; unsigned chat; Ref obras[96]; int nobras; int listo; } vig;

void hud_reiniciar(void) {
	memset(&vig, 0, sizeof vig);
	navisos = 0; ult_x = ult_y = -1;
	placing = E_TYPE_COUNT; selecting = 0; modo = MODO_NADA; pagina = 0;
	panel_abierto = PANEL_NINGUNO;
}

static const char *nombre_jugador(int p, char *b, size_t n) {
	if (p == W.human) snprintf(b, n, "Tú");
	else snprintf(b, n, "Jugador %d (%s)", p + 1, civ_def[W.pl[p].civ].name);
	return b;
}

void avisos_actualizar(float dt) {
	for (int k = 0; k < navisos;) {
		avisos[k].t += dt;
		if (avisos[k].t > 12.0f) { memmove(avisos + k, avisos + k + 1, sizeof(Aviso) * (navisos - k - 1)); --navisos; }
		else ++k;
	}
	Player *pl = &W.pl[W.human];
	if (!vig.listo) {
		for (int p = 0; p < MAX_PLAYERS; ++p) { vig.edad[p] = W.pl[p].age; vig.vivo[p] = W.pl[p].active; vig.dipl[p] = pl->dipl[p]; }
		vig.techs = pl->techs; vig.npings = W.npings; vig.chat = W.chat_n; vig.listo = 1;
	}
	/* chat de la partida (D415): lo que dicen los jugadores de la máquina al humano, como avisos */
	if (W.chat_n < vig.chat) vig.chat = W.chat_n;
	if (W.chat_n - vig.chat > ARRAY_LEN(W.chat)) vig.chat = W.chat_n - ARRAY_LEN(W.chat);
	for (; vig.chat < W.chat_n; ++vig.chat) {
		char cb[160], cn[64], *t = W.chat[vig.chat % ARRAY_LEN(W.chat)].texto;
		int de = W.chat[vig.chat % ARRAY_LEN(W.chat)].de;
		snprintf(cb, sizeof cb, "%s: %s", de >= 0 ? nombre_jugador(de, cn, sizeof cn) : "Sistema", t);
		aviso(IC_DIPLOMACIA, cb, COL_AVISO, -1, -1);
	}
	char b[120], nb[64];
	/* ataques: los círculos del minimapa */
	if (W.npings > vig.npings || (W.npings && W.pings[W.npings - 1].t < dt * 1.5f && W.npings != vig.npings)) {
		const char *donde = "";
		float x = W.pings[W.npings - 1].x, y = W.pings[W.npings - 1].y;
		for (int i = 0; i < W.nents; ++i) {
			Entity *e = &W.ents[i];
			if (e->alive && e->owner == W.human && fabsf(e->x - x) < 0.6f && fabsf(e->y - y) < 0.6f) { donde = is_building(e->type) ? bld_def[e->type].name : unit_def[e->type - E_VILLAGER].name; break; }
		}
		snprintf(b, sizeof b, "¡Te atacan!%s%s (Espacio: ir)", donde[0] ? " " : "", donde);
		aviso(IC_ALERTA, b, gfx_rgb(255, 110, 90), x, y);
	}
	vig.npings = W.npings;
	for (int p = 0; p < MAX_PLAYERS; ++p) {
		if (!W.pl[p].active) continue;
		if (W.pl[p].age != vig.edad[p]) {
			if (p == W.human) snprintf(b, sizeof b, "¡Has llegado a la %s!", age_def[W.pl[p].age].name);
			else snprintf(b, sizeof b, "%s llega a la %s", nombre_jugador(p, nb, sizeof nb), age_def[W.pl[p].age].name);
			aviso(IC_EDAD, b, p == W.human ? COL_AVISO : COL_DIM, -1, -1);
			vig.edad[p] = W.pl[p].age;
		}
		if (vig.vivo[p] && !W.pl[p].alive && W.time > 5 && p != W.human) {
			snprintf(b, sizeof b, "%s ha sido derrotado", nombre_jugador(p, nb, sizeof nb));
			aviso(IC_CANCELAR, b, COL_DIM, -1, -1);
		}
		vig.vivo[p] = W.pl[p].alive || W.time <= 5;
		if (p != W.human && pl->dipl[p] != vig.dipl[p]) {
			snprintf(b, sizeof b, "%s: ahora %s", nombre_jugador(p, nb, sizeof nb), dipl_name(pl->dipl[p]));
			aviso(IC_DIPLOMACIA, b, COL_CREAM, -1, -1);
			vig.dipl[p] = pl->dipl[p];
		}
	}
	if (pl->techs != vig.techs) {
		for (int t = 0; t < T_COUNT; ++t)
			if ((pl->techs >> t & 1u) && !(vig.techs >> t & 1u)) { snprintf(b, sizeof b, "Mejora terminada: %s", tech_def[t].name); aviso(IC_MEJORA, b, COL_OK, -1, -1); }
		vig.techs = pl->techs;
	}
	int lleno = pl->pop >= pl->popcap && pl->popcap > 0;
	if (lleno && !vig.lleno) aviso(IC_POB, pl->popcap >= (W.setup.popcap > 0 ? W.setup.popcap : 200) ? "Población máxima alcanzada" : "Población llena: construye casas", COL_AVISO, -1, -1);
	vig.lleno = lleno;
	/* obras: las que ayer estaban a medias y hoy están terminadas */
	for (int k = 0; k < vig.nobras; ++k) {
		Entity *e = ent_get(vig.obras[k]);
		if (e && e->progress >= 1 && e->type != E_WALL) { snprintf(b, sizeof b, "Terminado: %s", bld_def[e->type].name); aviso(IC_CONSTRUIR, b, COL_OK, e->x, e->y); }
	}
	vig.nobras = 0;
	for (int i = 0; i < W.nents && vig.nobras < ARRAY_LEN(vig.obras); ++i)
		if (W.ents[i].alive && W.ents[i].owner == W.human && is_building(W.ents[i].type) && W.ents[i].progress < 1) vig.obras[vig.nobras++] = ent_ref(i);
	/* maravillas terminadas */
	int nm = 0;
	for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].type == E_WONDER && W.ents[i].progress >= 1) {
		++nm;
		if (nm > vig.maravilla) { snprintf(b, sizeof b, "Maravilla terminada: %s", nombre_jugador(W.ents[i].owner, nb, sizeof nb)); aviso(IC_EDAD, b, same_team(W.ents[i].owner, W.human) ? COL_OK : COL_MAL, W.ents[i].x, W.ents[i].y); }
	}
	vig.maravilla = nm;
}

/* los mensajes que ya salen como aviso no se repiten en la línea central */
static int mensaje_repetido(const char *m) {
	return !strcmp(m, "¡Te atacan!") || !strncmp(m, "Investigado:", 12) || strstr(m, "alcanzada!") || !strcmp(m, "Construcción terminada") || !strncmp(m, "Relación con el jugador", 23);
}

static void dibujar_avisos(void) {
	int x = 8, y = top_h + 8, w = MIN(420, fb.w / 2 - 20);
	for (int k = 0; k < navisos; ++k) {
		Aviso *a = &avisos[k];
		int alfa = a->t > 10.5f ? (int)((12.0f - a->t) / 1.5f * 200) : 200;
		if (alfa <= 8) continue;
		int tw = MIN(w, gfx_t_w(a->texto, 1) + 34);
		gfx_fill_a(x, y, tw, 22, gfx_rgb(16, 11, 7), alfa);
		int parp = a->icono == IC_ALERTA && a->t < 4 && ((int)(a->t * 4) & 1);
		gfx_fill(x, y, 3, 22, parp ? gfx_rgb(255, 255, 255) : a->col);
		icono(a->icono, x + 6, y + 2, 18);
		gfx_ts(x + 28, y + 3, a->texto, a->col, 1);
		if (a->x >= 0 && hit(x, y, tw, 22)) {
			gfx_rect(x, y, tw, 22, COL_GOLD);
			if (in.mclick) { view_center_on(a->x, a->y); in.bloqueo = 1; }
		}
		zona(x, y, tw, 22);
		solape_registrar(x, y, tw, 22, "aviso");
		y += 24;
	}
}

/* cuentas atrás de la victoria, visibles para todos */
static void dibujar_cuentas(void) {
	char b[120], nb[64];
	int y = top_h + 6;
	for (int i = 0; i < W.nents; ++i) {
		Entity *w = &W.ents[i];
		if (!w->alive || w->type != E_WONDER || w->progress < 1) continue;
		if (W.setup.victory != VIC_STANDARD && W.setup.victory != VIC_WONDER) break;
		int left = MAX(0, 600 - (int)w->timer);
		snprintf(b, sizeof b, "Maravilla de %s: %d:%02d", nombre_jugador(w->owner, nb, sizeof nb), left / 60, left % 60);
		u32 c = same_team(w->owner, W.human) ? COL_OK : COL_MAL;
		int tw = gfx_t_w(b, 1) + 40, x = fb.w / 2 - tw / 2;
		gfx_fill_a(x, y, tw, 24, gfx_rgb(16, 11, 7), 215); bevel(x, y, tw, 24, c);
		icono(IC_EDAD, x + 6, y + 3, 18);
		gfx_ts(x + 30, y + 4, b, c, 1);
		y += 28;
	}
	if (W.setup.victory == VIC_RELICS && W.relic_team >= 0) {
		int left = MAX(0, 60 - (int)W.relic_t);
		snprintf(b, sizeof b, "Todas las reliquias: %s: %d:%02d", nombre_jugador(W.relic_team, nb, sizeof nb), left / 60, left % 60);
		u32 c = same_team(W.relic_team, W.human) ? COL_OK : COL_MAL;
		int tw = gfx_t_w(b, 1) + 40, x = fb.w / 2 - tw / 2;
		gfx_fill_a(x, y, tw, 24, gfx_rgb(16, 11, 7), 215); bevel(x, y, tw, 24, c);
		icono(IC_ORO, x + 6, y + 3, 18);
		gfx_ts(x + 30, y + 4, b, c, 1);
		y += 28;
	}
	if (W.setup.victory == VIC_SCORE) {
		int left = MAX(0, W.setup.time_limit * 60 - (int)W.time), best = -1, bs = -1;
		for (int p = 0; p < MAX_PLAYERS; ++p) if (W.pl[p].active && W.pl[p].alive) { int s = world_team_score(p); if (s > bs) { bs = s; best = p; } }
		snprintf(b, sizeof b, "Tiempo: %d:%02d, gana %s", left / 60, left % 60, best >= 0 ? nombre_jugador(best, nb, sizeof nb) : "nadie");
		int tw = gfx_t_w(b, 1) + 40, x = fb.w / 2 - tw / 2;
		u32 c = best >= 0 && same_team(best, W.human) ? COL_OK : COL_AVISO;
		gfx_fill_a(x, y, tw, 24, gfx_rgb(16, 11, 7), 200); bevel(x, y, tw, 24, c);
		icono(IC_TIEMPO, x + 6, y + 3, 18);
		gfx_ts(x + 30, y + 4, b, c, 1);
	}
}

/* ---------- barra superior ---------- */

static int recolectores(int kind) {
	int n = 0;
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (e->alive && e->owner == W.human && (e->type == E_VILLAGER || e->type == E_FISHBOAT) && (e->state == ST_GATHER || e->state == ST_RETURN) && e->carry_kind == kind) ++n;
	}
	return n;
}

static void barra_superior(void) {
	Player *pl = &W.pl[W.human];
	fondo_barra(0, 0, fb.w, top_h, 1);
	static const char *rn[4] = { "Madera", "Comida", "Oro", "Piedra" };
	int x = 8;
	for (int k = 0; k < RES_COUNT; ++k) {
		char b[32];
		snprintf(b, sizeof b, "%d", pl->res[k]);
		int w = 30 + MAX(gfx_t_w(b, 1), 32) + 10;
		gfx_fill_a(x, 4, w, 24, gfx_rgb(14, 9, 6), 150);
		icono_rec(k, x + 2, 7, 18);
		gfx_ts(x + 32, 8, b, COL_CREAM, 1);
		if (hit(x, 4, w, 24)) {
			char d[96]; snprintf(d, sizeof d, "%d aldeanos o barcos recogiendo ahora.", recolectores(k));
			tip_simple((Rect){ x, 4, w, 24 }, rn[k], d);
		}
		solape_registrar(x, 4, w, 24, "recurso");
		x += w + 4;
	}
	{
		char b[32];
		snprintf(b, sizeof b, "%d/%d", pl->pop, pl->popcap);
		int w = 30 + gfx_t_w(b, 1) + 10, lleno = pl->pop >= pl->popcap;
		gfx_fill_a(x, 4, w, 24, lleno ? gfx_rgb(90, 20, 14) : gfx_rgb(14, 9, 6), 160);
		icono(IC_POB, x + 4, 6, 20);
		gfx_ts(x + 30, 8, b, lleno ? COL_MAL : COL_CREAM, 1);
		if (hit(x, 4, w, 24)) {
			char d[160];
			snprintf(d, sizeof d, "%d de %d. Cada casa da 5 y cada centro urbano 10; máximo de la partida: %d.", pl->pop, pl->popcap, W.setup.popcap > 0 ? W.setup.popcap : 200);
			tip_simple((Rect){ x, 4, w, 24 }, "Población", d);
		}
		solape_registrar(x, 4, w, 24, "poblacion");
		x += w + 10;
	}
	/* a la derecha: menú, ayuda, diplomacia, puntuación y el reloj */
	int rx = fb.w - 8;
	rx -= 64;
	if (button(rx, 4, 64, 24, "Menú", 1, 1)) app.paused = 1;
	solape_registrar(rx, 4, 64, 24, "menu");
	static const struct { int ic, panel; const char *t, *d; } bt[3] = {
		{ IC_AYUDA, PANEL_AYUDA, "Ayuda (F1)", "Teclas y órdenes del juego." },
		{ IC_DIPLOMACIA, PANEL_DIPLOMACIA, "Diplomacia y tributo (F3)", "Relaciones con cada jugador y envío de recursos." },
		{ IC_PUNTOS, -1, "Puntuación (Tab)", "Tab cambia entre el marcador pequeño, el ampliado y ninguno." },
	};
	for (int k = 0; k < 3; ++k) {
		rx -= 30;
		int on = bt[k].panel >= 0 ? panel_abierto == bt[k].panel : marcador_modo != 2, ov = hit(rx, 4, 26, 24);
		gfx_fill_grad(rx, 4, 26, 24, ov ? gfx_rgb(90, 60, 36) : gfx_rgb(50, 33, 22), gfx_rgb(26, 17, 11));
		bevel(rx, 4, 26, 24, on ? COL_CREAM : COL_GOLD);
		icono(bt[k].ic, rx + 3, 5, 21);
		if (ov) {
			tip_simple((Rect){ rx, 4, 26, 24 }, bt[k].t, bt[k].d);
			if (in.mclick) {
				in.bloqueo = 1; snd_ui(SND_CLICK);
				if (bt[k].panel >= 0) panel_abierto = panel_abierto == bt[k].panel ? PANEL_NINGUNO : bt[k].panel;
				else marcador_modo = (marcador_modo + 1) % 3;
			}
		}
		solape_registrar(rx, 4, 26, 24, "boton superior");
	}
	int t = (int)W.time;
	char tb[32];
	snprintf(tb, sizeof tb, "%02d:%02d", t / 60, t % 60);
	char vb[8]; snprintf(vb, sizeof vb, "x%d", app.game_speed);
	int cw = gfx_t_w(tb, 1) + gfx_t_w(vb, 1) + 26;
	rx -= cw + 8;
	gfx_fill_a(rx, 4, cw, 24, gfx_rgb(14, 9, 6), 150);
	icono(IC_TIEMPO, rx + 2, 6, 20);
	gfx_ts(rx + 22, 8, tb, COL_CREAM, 1);
	gfx_ts(rx + cw - gfx_t_w(vb, 1) - 4, 8, vb, app.game_speed > 1 ? COL_AVISO : COL_DIM, 1);
	if (hit(rx, 4, cw, 24)) tip_simple((Rect){ rx, 4, cw, 24 }, "Tiempo de partida", "Velocidad: + y - (de x1 a x4).");
	solape_registrar(rx, 4, cw, 24, "reloj");
	/* la edad entre los recursos y el reloj, con el nombre corto si no cabe */
	{
		const char *an = age_def[pl->age].name;
		char corto[48];
		const char *sp = strstr(an, " de ") ? strstr(an, " de ") + 4 : strstr(an, " del ") ? strstr(an, " del ") + 5 : an;
		snprintf(corto, sizeof corto, "%s", sp);
		int hueco = rx - 8 - x;
		const char *nom = gfx_t_w(an, 1) + 30 <= hueco ? an : corto;
		int aw = gfx_t_w(nom, 1) + 30;
		if (aw <= hueco) {
			int ax = x + (hueco - aw) / 2;
			icono(IC_EDAD, ax, 6, 20);
			gfx_ts(ax + 26, 8, nom, COL_GOLD, 1);
			if (pl->age_t >= 0 && pl->age + 1 < AGE_COUNT) barra_progreso(ax + 26, 25, aw - 26, 4, (int)(pl->age_t * 100), (int)(age_def[pl->age + 1].time * 100), gfx_rgb(240, 200, 60));
			if (hit(ax, 4, aw, 24)) {
				char d[120];
				if (pl->age_t >= 0 && pl->age + 1 < AGE_COUNT) snprintf(d, sizeof d, "Avanzando a la %s: %d%%", age_def[pl->age + 1].name, (int)(100 * pl->age_t / age_def[pl->age + 1].time));
				else snprintf(d, sizeof d, "Se avanza de edad en el centro urbano.");
				tip_simple((Rect){ ax, 4, aw, 24 }, an, d);
			}
			solape_registrar(ax, 4, aw, 24, "edad");
		}
	}
}

/* ---------- órdenes ---------- */

enum { OR_CONSTRUIR, OR_ENTRENAR, OR_INVESTIGAR, OR_EDAD, OR_POSTURA, OR_PROTEGER, OR_PATRULLA, OR_DETENER, OR_DESEMBARCAR, OR_EMBARCAR,
	OR_REUNION, OR_VENDER, OR_COMPRAR, OR_TRIBUTO, OR_PAGINA, OR_ELIMINAR, OR_COM_MODO, OR_COM_DAR, OR_COM_RECIBIR };

typedef struct {
	int tipo, arg, ok, activo, prog, prog_max, sc;
	Tip tip;
} Orden;

static Orden ord[48];
static int nord;

static Orden *orden_nueva(int tipo, int arg) {
	if (nord >= ARRAY_LEN(ord)) return NULL;
	Orden *o = &ord[nord++];
	memset(o, 0, sizeof *o);
	o->tipo = tipo; o->arg = arg; o->ok = 1;
	return o;
}

/* motivo por el que falta un coste: "Falta oro: 30" (o varios) */
static int motivo_coste(const int *c, char *out, size_t n) {
	static const char *rn[4] = { "madera", "comida", "oro", "piedra" };
	Player *pl = &W.pl[W.human];
	char b[120] = "";
	int falta = 0;
	for (int k = 0; k < RES_COUNT; ++k)
		if (c[k] > pl->res[k]) {
			char t[32]; snprintf(t, sizeof t, "%s%s: %d", falta ? ", " : "Falta ", rn[k], c[k] - pl->res[k]);
			strncat(b, t, sizeof b - strlen(b) - 1);
			falta = 1;
		}
	if (falta) snprintf(out, n, "%s", b);
	return falta;
}

static const char *nombre_edad_corto(int a) { static const char *n[] = { "Edad de Piedra", "Edad de Herramientas", "Edad del Bronce", "Edad del Hierro" }; return n[CLAMP(a, 0, 3)]; }

static void orden_construir(EntType t) {
	Orden *o = orden_nueva(OR_CONSTRUIR, t);
	if (!o) return;
	const BldDef *d = &bld_def[t];
	Player *pl = &W.pl[W.human];
	snprintf(o->tip.titulo, sizeof o->tip.titulo, "Construir %s", d->name);
	o->tip.coste[0] = d->wood; o->tip.coste[1] = d->food; o->tip.coste[2] = d->gold; o->tip.coste[3] = d->stone;
	o->tip.con_coste = 1; o->tip.tiempo = d->build_time;
	static const char *desc[NUM_BLD_TYPES] = {
		"Entrena aldeanos, recoge todos los recursos y avanza de edad. Población +10.",
		"Entrena infantería: clubman, hacheros, espadachines y honderos.", "Población +5.", "Recoge comida (bayas, granjas y caza); mejoras de agricultura, torres y murallas.",
		"Recoge madera, oro, piedra y carne de caza; mejoras de recolección, armas y armaduras.", "250 de comida para un aldeano; agotada, se reconstruye.",
		"Entrena arqueros, carros arqueros, arqueros a caballo y de elefante.", "Entrena exploradores, caballería, catafractas y elefantes.", "Entrena sacerdotes; mejoras religiosas; guarda reliquias (dan oro).",
		"Entrena hoplitas, falanges y centuriones.", "Mejoras de gobierno: arquitectura, ingeniería, balística y logística.", "Dispara a los enemigos cercanos.",
		"Compra y vende recursos, envía tributo, entrena carros mercantes; la Rueda y la Astronomía.", "Construye máquinas de asedio.", "Corta el paso. Se construyen tramos de 1x1.",
		"En la orilla: barcos de pesca, mercantes, de transporte y trirremes; recoge el pescado.", "Si resiste 10 minutos terminada, ganas la partida.",
	};
	snprintf(o->tip.desc, sizeof o->tip.desc, "%s", desc[t - E_TOWN_CENTER]);
	snprintf(o->tip.extra, sizeof o->tip.extra, "Tamaño %dx%d, %d puntos de vida.", d->size, d->size, d->hp);
	if (pl->age < d->age) { o->ok = 0; snprintf(o->tip.motivo, sizeof o->tip.motivo, "Necesitas la %s", nombre_edad_corto(d->age)); }
	else if (motivo_coste(o->tip.coste, o->tip.motivo, sizeof o->tip.motivo)) o->ok = 0;
}

static int sel_edificio = -1;   /* edificio propio seleccionado (para entrenar, investigar, reunión) */

static void orden_entrenar(EntType t, const Entity *b) {
	const UnitDef *u = &unit_def[t - E_VILLAGER];
	Player *pl = &W.pl[W.human];
	Orden *o = orden_nueva(OR_ENTRENAR, t);
	if (!o) return;
	snprintf(o->tip.titulo, sizeof o->tip.titulo, "Entrenar %s", u->name);
	o->tip.coste[0] = u->wood; o->tip.coste[1] = u->food; o->tip.coste[2] = u->gold; o->tip.coste[3] = u->stone;
	o->tip.con_coste = 1; o->tip.tiempo = u->train_time * civ_def[pl->civ].train_time;
	if (u->dmg > 0) snprintf(o->tip.desc, sizeof o->tip.desc, "Vida %d, ataque %d%s, armadura %d/%d, alcance %.0f, velocidad %.1f.", u->hp, (int)eff_attack(W.human, t, 0),
		u->dmg_bld > u->dmg ? " (mucho contra edificios)" : "", eff_armor(W.human, t, 0), eff_armor(W.human, t, 1), eff_range(W.human, t), u->speed);
	else snprintf(o->tip.desc, sizeof o->tip.desc, "Vida %d, velocidad %.1f.%s", u->hp, u->speed, t == E_VILLAGER ? " Recoge recursos y construye." : t == E_PRIEST ? " Convierte enemigos y cura." : t == E_TRANSPORT ? " Lleva 5 unidades de tierra." : t == E_FISHBOAT ? " Pesca en los bancos de peces." : "");
	if (unit_min_range(t) > 0) snprintf(o->tip.extra, sizeof o->tip.extra, "Alcance mínimo %.1f: no dispara a lo que tiene pegado. Daño en área.", unit_min_range(t));
	else if (is_siege(t)) snprintf(o->tip.extra, sizeof o->tip.extra, "Máquina de asedio: mucho daño contra edificios.");
	o->tip.extra[0] ? (void)0 : (void)snprintf(o->tip.extra, sizeof o->tip.extra, "Mayús+clic: entrena 5.");
	int req = unit_tech_req(t);
	if (pl->age < u->age) { o->ok = 0; snprintf(o->tip.motivo, sizeof o->tip.motivo, "Necesitas la %s", nombre_edad_corto(u->age)); }
	else if (req >= 0 && !has_tech(W.human, req)) { o->ok = 0; snprintf(o->tip.motivo, sizeof o->tip.motivo, "Necesitas la mejora %s", tech_def[req].name); }
	else if (b->ntrain >= 5) { o->ok = 0; snprintf(o->tip.motivo, sizeof o->tip.motivo, "La cola está llena (5)"); }
	else if (pl->pop + b->ntrain >= pl->popcap) { o->ok = 0; snprintf(o->tip.motivo, sizeof o->tip.motivo, pl->popcap >= (W.setup.popcap > 0 ? W.setup.popcap : 200) ? "Población máxima alcanzada" : "Necesitas más casas"); }
	else if (motivo_coste(o->tip.coste, o->tip.motivo, sizeof o->tip.motivo)) o->ok = 0;
	/* en cola: un número en el botón */
	for (int q = 0; q < b->ntrain; ++q) if (b->train[q] == (int)t) ++o->activo;
}

static void orden_investigar(int t, const Entity *b) {
	const TechDef *td = &tech_def[t];
	Player *pl = &W.pl[W.human];
	if (has_tech(W.human, t) || !tech_allowed(W.human, t)) return;   /* las hechas y las prohibidas no ocupan sitio */
	Orden *o = orden_nueva(OR_INVESTIGAR, t);
	if (!o) return;
	snprintf(o->tip.titulo, sizeof o->tip.titulo, "Investigar %s", td->name);
	o->tip.coste[0] = td->wood; o->tip.coste[1] = td->food; o->tip.coste[2] = td->gold; o->tip.coste[3] = td->stone;
	o->tip.con_coste = 1; o->tip.tiempo = td->time;
	snprintf(o->tip.desc, sizeof o->tip.desc, "%s", td->desc);
	if (b->tech_cur == t + 1) { o->ok = 0; o->activo = 1; o->prog = (int)(b->tech_t * 100); o->prog_max = (int)(td->time * 100); snprintf(o->tip.extra, sizeof o->tip.extra, "Investigando: %d%%", (int)(100 * b->tech_t / td->time)); }
	else if (pl->age < td->age) { o->ok = 0; snprintf(o->tip.motivo, sizeof o->tip.motivo, "Necesitas la %s", nombre_edad_corto(td->age)); }
	else if (td->prereq >= 0 && !has_tech(W.human, td->prereq)) { o->ok = 0; snprintf(o->tip.motivo, sizeof o->tip.motivo, "Necesitas antes: %s", tech_def[td->prereq].name); }
	else if (b->tech_cur) { o->ok = 0; snprintf(o->tip.motivo, sizeof o->tip.motivo, "Este edificio ya investiga otra mejora"); }
	else if (motivo_coste(o->tip.coste, o->tip.motivo, sizeof o->tip.motivo)) o->ok = 0;
}

static void orden_edad(void) {
	Player *pl = &W.pl[W.human];
	if (pl->age + 1 >= AGE_COUNT) return;
	const AgeDef *ad = &age_def[pl->age + 1];
	Orden *o = orden_nueva(OR_EDAD, pl->age + 1);
	if (!o) return;
	snprintf(o->tip.titulo, sizeof o->tip.titulo, "Avanzar a la %s", ad->name);
	o->tip.coste[1] = ad->food; o->tip.coste[2] = ad->gold; o->tip.con_coste = 1; o->tip.tiempo = ad->time;
	static const char *req[AGE_COUNT] = { "", "Hacen falta 2 de: Casa, Granero, Almacén o Cuartel.", "Hacen falta 2 de: Campo de tiro, Establo, Granja o Torre.", "Hacen falta 2 de: Templo, Academia o Centro de gobierno." };
	snprintf(o->tip.desc, sizeof o->tip.desc, "Nuevos edificios, unidades y mejoras.");
	snprintf(o->tip.extra, sizeof o->tip.extra, "%s", req[pl->age + 1]);
	if (pl->age_t >= 0) { o->ok = 0; o->activo = 1; o->prog = (int)(pl->age_t * 100); o->prog_max = (int)(ad->time * 100); snprintf(o->tip.motivo, sizeof o->tip.motivo, "Investigando: %d%%", (int)(100 * pl->age_t / ad->time)); }
	else if (!age_requirements_met(W.human, pl->age + 1)) { o->ok = 0; snprintf(o->tip.motivo, sizeof o->tip.motivo, "Faltan edificios de la %s terminados", age_def[pl->age].name); }
	else if (motivo_coste(o->tip.coste, o->tip.motivo, sizeof o->tip.motivo)) o->ok = 0;
}

static void orden_simple(int tipo, int arg, int sc, const char *tecla_txt, const char *titulo, const char *desc, int activo) {
	Orden *o = orden_nueva(tipo, arg);
	if (!o) return;
	o->sc = sc; o->activo = activo;
	snprintf(o->tip.titulo, sizeof o->tip.titulo, "%s", titulo);
	snprintf(o->tip.desc, sizeof o->tip.desc, "%s", desc);
	if (tecla_txt) snprintf(o->tip.tecla, sizeof o->tip.tecla, "%s", tecla_txt);
}

/* lista de órdenes según la selección */
static const char *titulo_ordenes = "";
static void construir_ordenes(void) {
	nord = 0; sel_edificio = -1; titulo_ordenes = "";
	Player *pl = &W.pl[W.human];
	int i0 = RS.ntipos ? RS.primero[0] : -1;
	if (i0 < 0) return;
	Entity *e = &W.ents[i0];
	if (RS.propias > 0) {
		/* unidades: si hay aldeanos, construir; si hay soldados, posturas y órdenes */
		if (RS.aldeanos) {
			titulo_ordenes = "Construir";
			static const EntType bl[] = { E_HOUSE, E_FARM, E_GRANARY, E_STORAGE, E_BARRACKS, E_DOCK, E_TOWER, E_WALL, E_ARCHERY, E_STABLE,
				E_MARKET, E_TEMPLE, E_ACADEMY, E_GOVERNMENT, E_SIEGE, E_TOWN_CENTER, E_WONDER };
			for (int k = 0; k < ARRAY_LEN(bl); ++k) orden_construir(bl[k]);
		} else {
			titulo_ordenes = "Órdenes";
			int st = -1;
			for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human && es_militar(W.ents[i].type)) { st = W.ents[i].stance; break; }
			if (RS.militares) {
				orden_simple(OR_POSTURA, STANCE_AGGRESSIVE, SDL_SCANCODE_Z, "Z", "Postura agresiva", "Persigue a los enemigos que ve.", st == STANCE_AGGRESSIVE);
				orden_simple(OR_POSTURA, STANCE_DEFENSIVE, SDL_SCANCODE_X, "X", "Postura defensiva", "Ataca a lo que se acerca y vuelve a su puesto.", st == STANCE_DEFENSIVE);
				orden_simple(OR_POSTURA, STANCE_STAND, SDL_SCANCODE_C, "C", "Quieta", "No se mueve del sitio; dispara a lo que tiene a tiro.", st == STANCE_STAND);
				orden_simple(OR_POSTURA, STANCE_PASSIVE, SDL_SCANCODE_V, "V", "Pasiva", "No ataca por su cuenta.", st == STANCE_PASSIVE);
				orden_simple(OR_PROTEGER, 0, SDL_SCANCODE_G, "G", "Proteger", "Sigue y defiende a un aliado: clic en él.", modo == MODO_PROTEGER || st == STANCE_GUARD);
				orden_simple(OR_PATRULLA, 0, SDL_SCANCODE_P, "P", "Patrullar", "Va y viene entre su sitio y el punto que elijas, luchando con lo que encuentre.", modo == MODO_PATRULLA);
			}
		}
		orden_simple(OR_DETENER, 0, SDL_SCANCODE_Q, "Q", "Detener", "Deja lo que esté haciendo.", 0);
		/* mercantes: modo oro o intercambio (este, solo con «Comercio mejorado») y los recursos del intercambio */
		{
			Entity *m = NULL;
			for (int i = 0; i < W.nents && !m; ++i) if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human && is_trader(W.ents[i].type)) m = &W.ents[i];
			if (m) {
				static const char *rn[] = { "madera", "comida", "oro", "piedra" };
				char d[200];
				snprintf(d, sizeof d, "Va y viene entre tu %s y el de otro jugador (clic derecho en él) y trae oro según la distancia: %d por viaje en esta ruta. A nadie se le quita nada.",
				         m->type == E_TRADESHIP ? "muelle" : "mercado", comercio_oro(comercio_distancia(m)));
				orden_simple(OR_COM_MODO, 0, SDL_SCANCODE_O, "O", "Modo oro", d, m->com_modo == 0);
				if (W.setup.comercio_mejorado) {
					snprintf(d, sizeof d, "Carga 15 de %s en tu puerto (si no los tienes, espera), los cambia en el ajeno por %s y te trae 15 x bonus de distancia x escasez (ahora: %d). Al otro jugador no se le quita nada.",
					         rn[CLAMP(m->com_dar, 0, 3)], rn[CLAMP(m->com_recibir, 0, 3)], comercio_intercambio(m->com_dar, m->com_recibir, comercio_distancia(m)));
					orden_simple(OR_COM_MODO, 1, SDL_SCANCODE_I, "I", "Modo intercambio", d, m->com_modo == 1);
					/* el arg de Dar y Recibir es el recurso elegido: lo dibuja el icono (ejecutar no lo usa) */
					snprintf(d, sizeof d, "Recurso que carga en tu puerto: %s. Clic o N: el siguiente.", rn[CLAMP(m->com_dar, 0, 3)]);
					orden_simple(OR_COM_DAR, CLAMP(m->com_dar, 0, 3), SDL_SCANCODE_N, "N", "Dar", d, 0);
					snprintf(d, sizeof d, "Recurso que trae a cambio: %s. Clic o M: el siguiente.", rn[CLAMP(m->com_recibir, 0, 3)]);
					orden_simple(OR_COM_RECIBIR, CLAMP(m->com_recibir, 0, 3), SDL_SCANCODE_M, "M", "Recibir", d, 0);
				} else {
					/* sin la opción de partida, el botón está gris y dice cómo tenerlo */
					orden_simple(OR_COM_MODO, 1, SDL_SCANCODE_I, "I", "Modo intercambio", "Lleva 15 de un recurso tuyo al puerto ajeno y trae otro.", 0);
					ord[nord - 1].ok = 0;
					snprintf(ord[nord - 1].tip.motivo, sizeof ord[nord - 1].tip.motivo, "Activa Comercio mejorado al crear la partida");
				}
			}
		}
		if (RS.transportes) {
			int carga = 0;
			for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel && W.ents[i].type == E_TRANSPORT && W.ents[i].owner == W.human) carga += W.ents[i].ncargo;
			orden_simple(OR_DESEMBARCAR, 0, SDL_SCANCODE_E, "E", "Desembarcar", "Clic en la orilla: el transporte se acerca y deja allí a los pasajeros.", modo == MODO_DESEMBARCAR);
			if (!carga) { ord[nord - 1].ok = 0; snprintf(ord[nord - 1].tip.motivo, sizeof ord[nord - 1].tip.motivo, "El transporte va vacío"); }
		} else if (RS.propias > RS.navales) {   /* los barcos no embarcan */
			orden_simple(OR_EMBARCAR, 0, SDL_SCANCODE_R, "R", "Embarcar", "Clic en un transporte propio: las unidades van a la orilla y suben (5 plazas).", modo == MODO_EMBARCAR);
			int hay = 0;
			for (int i = 0; i < W.nents && !hay; ++i) hay = W.ents[i].alive && W.ents[i].type == E_TRANSPORT && W.ents[i].owner == W.human;
			if (!hay) { ord[nord - 1].ok = 0; snprintf(ord[nord - 1].tip.motivo, sizeof ord[nord - 1].tip.motivo, "No tienes transportes (se hacen en el Muelle)"); }
		}
		orden_simple(OR_ELIMINAR, 0, SDL_SCANCODE_DELETE, "Supr", "Eliminar", "Elimina las unidades seleccionadas.", 0);
		return;
	}
	if (e->owner != W.human || !is_building(e->type) || e->state == ST_DYING) return;
	if (e->progress < 1) return;
	sel_edificio = i0;
	titulo_ordenes = bld_def[e->type].name;
	/* unidades que entrena, en el orden de la tabla */
	for (int t = E_VILLAGER; t < E_HORSE; ++t) if (unit_def[t - E_VILLAGER].trainer == e->type) orden_entrenar((EntType)t, e);
	if (e->type == E_TOWN_CENTER) orden_edad();
	if (e->type == E_MARKET) {
		static const char *rn[4] = { "madera", "comida", "oro", "piedra" };
		for (int r = 0; r < RES_COUNT; ++r) {
			if (r == RES_GOLD) continue;
			int venta = 100 * pl->price[r] / 100 - 12, compra = 100 * pl->price[r] / 100 + 12;
			Orden *o = orden_nueva(OR_VENDER, r);
			if (o) {
				snprintf(o->tip.titulo, sizeof o->tip.titulo, "Vender 100 de %s", rn[r]);
				snprintf(o->tip.desc, sizeof o->tip.desc, "Recibes %d de oro. El precio baja con cada venta.", venta);
				if (pl->res[r] < 100) { o->ok = 0; snprintf(o->tip.motivo, sizeof o->tip.motivo, "Falta %s: %d", rn[r], 100 - pl->res[r]); }
			}
			o = orden_nueva(OR_COMPRAR, r);
			if (o) {
				snprintf(o->tip.titulo, sizeof o->tip.titulo, "Comprar 100 de %s", rn[r]);
				o->tip.coste[RES_GOLD] = compra; o->tip.con_coste = 1;
				snprintf(o->tip.desc, sizeof o->tip.desc, "El precio sube con cada compra.");
				if (pl->res[RES_GOLD] < compra) { o->ok = 0; snprintf(o->tip.motivo, sizeof o->tip.motivo, "Falta oro: %d", compra - pl->res[RES_GOLD]); }
			}
		}
		orden_simple(OR_TRIBUTO, 0, SDL_SCANCODE_F3, "F3", "Enviar tributo", "Abre la diplomacia para regalar recursos a otro jugador (el mercado se queda el 20 %).", panel_abierto == PANEL_DIPLOMACIA);
	}
	for (int t = 0; t < T_COUNT; ++t) if (tech_def[t].bld == e->type) orden_investigar(t, e);
	if (ent_is_trainer(e->type)) orden_simple(OR_REUNION, 0, SDL_SCANCODE_Y, "Y o clic derecho", "Punto de reunión", "Clic en el mapa: las unidades nuevas irán allí (sobre un recurso o un enemigo, a trabajar o a atacar).", modo == MODO_REUNION);
}

static void ejecutar(const Orden *o) {
	Player *pl = &W.pl[W.human];
	(void)pl;
	if (!o->ok && o->tipo != OR_PAGINA) {
		if (o->tip.motivo[0]) world_msg(o->tip.motivo);
		snd_ui(SND_ERROR);
		AOEN_LOG("[AOEN] orden no disponible: %s (%s)", o->tip.titulo, o->tip.motivo);
		return;
	}
	snd_ui(SND_CLICK);
	AOEN_LOG("[AOEN] orden: %s", o->tip.titulo);
	switch (o->tipo) {
	case OR_CONSTRUIR: placing = (EntType)o->arg; break;
	case OR_ENTRENAR: {
		int n = mayus() ? 5 : 1;
		for (int k = 0; k < n; ++k) if (!cmd_train(W.human, ent_ref(sel_edificio), (EntType)o->arg)) break;
		break;
	}
	case OR_INVESTIGAR: cmd_research(W.human, ent_ref(sel_edificio), o->arg); break;
	case OR_EDAD: cmd_advance_age(W.human); break;
	case OR_POSTURA:
		modo = MODO_NADA;
		for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human && es_militar(W.ents[i].type)) cmd_set_stance(ent_ref(i), o->arg);
		break;
	case OR_PROTEGER: modo = MODO_PROTEGER; break;
	case OR_PATRULLA: modo = MODO_PATRULLA; break;
	case OR_DESEMBARCAR: modo = MODO_DESEMBARCAR; break;
	case OR_EMBARCAR: modo = MODO_EMBARCAR; break;
	case OR_REUNION: modo = MODO_REUNION; break;
	case OR_COM_MODO: case OR_COM_DAR: case OR_COM_RECIBIR:
		for (int i = 0; i < W.nents; ++i) {
			Entity *m = &W.ents[i];
			if (!m->alive || !m->sel || m->owner != W.human || !is_trader(m->type)) continue;
			if (o->tipo == OR_COM_MODO) cmd_comercio(ent_ref(i), o->arg, m->com_dar, m->com_recibir);
			else if (o->tipo == OR_COM_DAR) { int k = (m->com_dar + 1) % RES_COUNT; if (k == m->com_recibir) k = (k + 1) % RES_COUNT; cmd_comercio(ent_ref(i), m->com_modo, k, m->com_recibir); }
			else { int k = (m->com_recibir + 1) % RES_COUNT; if (k == m->com_dar) k = (k + 1) % RES_COUNT; cmd_comercio(ent_ref(i), m->com_modo, m->com_dar, k); }
		}
		break;
	case OR_DETENER:
		modo = MODO_NADA;
		for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human && is_unit(W.ents[i].type)) cmd_move(ent_ref(i), W.ents[i].x, W.ents[i].y);
		break;
	case OR_VENDER: cmd_trade(W.human, o->arg, RES_GOLD); break;
	case OR_COMPRAR: cmd_trade(W.human, RES_GOLD, o->arg); break;
	case OR_TRIBUTO: panel_abierto = panel_abierto == PANEL_DIPLOMACIA ? PANEL_NINGUNO : PANEL_DIPLOMACIA; break;
	case OR_PAGINA: ++pagina; break;
	case OR_ELIMINAR:
		for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human && is_unit(W.ents[i].type)) W.ents[i].hp = 0;
		break;
	}
}

/* icono de una orden en su botón */
static void icono_orden(const Orden *o, int x, int y, int sz) {
	Sprite *s = NULL;
	switch (o->tipo) {
	case OR_CONSTRUIR: s = icono_edificio((EntType)o->arg); break;
	case OR_ENTRENAR: s = icono_unidad((EntType)o->arg); break;
	case OR_INVESTIGAR: s = icono_mejora(o->arg); break;
	case OR_EDAD: s = iface_frame(50729, 65 + o->arg, 30); break;
	default: break;
	}
	if (s) {
		int sc = MIN((sz - 4) * 256 / s->w, (sz - 4) * 256 / s->h);
		gfx_clip(x + 2, y + 2, x + sz - 2, y + sz - 2);
		gfx_sprite(s, x + sz / 2 - s->w * sc / 512 + s->hx * sc / 256, y + sz / 2 - s->h * sc / 512 + s->hy * sc / 256, sc, pcolor(W.human), o->ok || o->activo ? 256 : 120, 0);
		gfx_clip_reset();
		return;
	}
	if (o->tipo == OR_COM_MODO || o->tipo == OR_COM_DAR || o->tipo == OR_COM_RECIBIR) {
		/* comercio: el oro, o el recurso que se da o se recibe con una flecha (antes leía fuera de por_tipo) */
		int k = o->tipo == OR_COM_MODO ? RES_GOLD : CLAMP(o->arg, 0, RES_COUNT - 1);
		icono_rec(k, x + 4, y + 6, sz / 2);
		u32 c = o->tipo == OR_COM_DAR ? gfx_rgb(255, 120, 90) : o->tipo == OR_COM_RECIBIR ? gfx_rgb(130, 255, 130) : COL_CREAM;
		gfx_ts(x + 3, y + sz - 17, o->tipo == OR_COM_DAR ? "Dar" : o->tipo == OR_COM_RECIBIR ? "Rec" : o->arg ? "<>" : "Oro", c, 1);
		return;
	}
	static const int por_tipo[] = { IC_CONSTRUIR, IC_SOLDADO, IC_MEJORA, IC_EDAD, IC_AGRESIVA, IC_PROTEGER, IC_PATRULLA, IC_DETENER, IC_DESEMBARCAR, IC_EMBARCAR,
		IC_REUNION, IC_VENDER, IC_COMPRAR, IC_TRIBUTO, IC_SIGUIENTE, IC_ELIMINAR };
	if (o->tipo < 0 || o->tipo >= ARRAY_LEN(por_tipo)) return;
	int ic = por_tipo[o->tipo];
	if (o->tipo == OR_POSTURA) ic = o->arg == STANCE_AGGRESSIVE ? IC_AGRESIVA : o->arg == STANCE_DEFENSIVE ? IC_DEFENSIVA : o->arg == STANCE_STAND ? IC_QUIETA : IC_PASIVA;
	if (o->tipo == OR_ENTRENAR && o->arg == E_VILLAGER) ic = IC_ALDEANO;
	if (o->tipo == OR_VENDER || o->tipo == OR_COMPRAR) {
		icono_rec(o->arg, x + 4, y + 6, sz / 2);
		int ax = x + sz - 14, ay = y + sz - 16;
		u32 c = o->tipo == OR_VENDER ? gfx_rgb(255, 120, 90) : gfx_rgb(130, 255, 130);
		gfx_fill(ax + 4, ay, 4, 10, c);
		for (int k = 0; k < 6; ++k) gfx_fill(ax + 6 - k, o->tipo == OR_VENDER ? ay + 10 + k : ay - k, 2 * k + 1 - (k ? 0 : 0), 1, c);
		gfx_ts(x + 3, y + sz - 17, o->tipo == OR_VENDER ? "V" : "C", c, 1);
		return;
	}
	icono(ic, x + 3, y + 3, sz - 6);
}

static void rejilla_ordenes(void) {
	Rect r = L.ord;
	panel_hueco(r.x - 4, r.y - 2, r.w + 8, r.h + 4);
	solape_registrar(r.x - 4, r.y - 2, r.w + 8, r.h + 4, "rejilla");
	zona(r.x - 4, r.y - 2, r.w + 8, r.h + 4);
	int por_pag = L.cols * L.filas;
	int i0 = RS.ntipos ? RS.primero[0] : -1;
	EntType tsel = i0 >= 0 ? W.ents[i0].type : E_TYPE_COUNT;
	if (tsel != pagina_tipo) { pagina_tipo = tsel; pagina = 0; }
	int paginas = 1, util = por_pag;
	if (nord > por_pag) { util = por_pag - 1; paginas = (nord + util - 1) / util; }
	if (pagina >= paginas) pagina = 0;
	char cab[96];
	if (paginas > 1) snprintf(cab, sizeof cab, "%s  (%d/%d)", titulo_ordenes, pagina + 1, paginas); else snprintf(cab, sizeof cab, "%s", titulo_ordenes);
	gfx_text(r.x, r.y + 2, cab, COL_GOLD, 1);
	if (tecla(SDL_SCANCODE_PAGEDOWN) && paginas > 1) pagina = (pagina + 1) % paginas;
	if (tecla(SDL_SCANCODE_PAGEUP) && paginas > 1) pagina = (pagina + paginas - 1) % paginas;
	int gy = r.y + 14;
	int primero = pagina * util, n = MIN(util, nord - primero);
	int slots = n + (paginas > 1 ? 1 : 0);
	for (int k = 0; k < slots; ++k) {
		Orden pagina_o, *o;
		if (k == n) {
			memset(&pagina_o, 0, sizeof pagina_o);
			pagina_o.tipo = OR_PAGINA; pagina_o.ok = 1;
			snprintf(pagina_o.tip.titulo, sizeof pagina_o.tip.titulo, "Más órdenes (%d/%d)", (pagina + 1) % paginas + 1, paginas);
			snprintf(pagina_o.tip.tecla, sizeof pagina_o.tip.tecla, "AvPág");
			o = &pagina_o;
		} else o = &ord[primero + k];
		int c = k % L.cols, f = k / L.cols;
		int x = r.x + c * (L.bsz + L.gap), y = gy + f * (L.bsz + L.gap), sz = L.bsz;
		int over = hit(x, y, sz, sz);
		u32 fondo = o->activo && o->tipo != OR_ENTRENAR ? gfx_rgb(110, 80, 30) : over ? gfx_rgb(80, 52, 30) : gfx_rgb(34, 23, 15);
		gfx_fill_grad(x, y, sz, sz, fondo, gfx_rgb(20, 13, 9));
		icono_orden(o, x, y, sz);
		if (!o->ok && !o->activo) gfx_fill_a(x + 1, y + 1, sz - 2, sz - 2, gfx_rgb(20, 16, 14), 120);
		bevel(x, y, sz, sz, o->activo && o->tipo != OR_ENTRENAR ? COL_CREAM : o->ok ? COL_GOLD : gfx_rgb(105, 88, 66));
		/* tecla: la fija (letra) o la posición (1..9, 0) */
		char hk[8] = "";
		if (o->sc && o->tip.tecla[0] && strlen(o->tip.tecla) <= 2) snprintf(hk, sizeof hk, "%s", o->tip.tecla);
		else if (k < 10 && o->tipo != OR_PAGINA) snprintf(hk, sizeof hk, "%d", (k + 1) % 10);
		if (hk[0]) { gfx_fill_a(x + 1, y + 1, 7 * (int)strlen(hk) + 3, 10, gfx_rgb(0, 0, 0), 150); gfx_text(x + 2, y + 2, hk, COL_AVISO, 1); }
		if (o->tipo == OR_ENTRENAR && o->activo) { char b[8]; snprintf(b, sizeof b, "%d", o->activo); gfx_fill(x + sz - 11, y + sz - 11, 10, 10, gfx_rgb(160, 30, 20)); gfx_text(x + sz - 9, y + sz - 10, b, COL_CREAM, 1); }
		if (o->prog_max > 0) barra_progreso(x + 2, y + sz - 7, sz - 4, 5, o->prog, o->prog_max, gfx_rgb(240, 200, 60));
		solape_registrar(x, y, sz, sz, "orden");
		if (over) {
			Tip t = o->tip;
			t.ancla = (Rect){ x, y, sz, sz };
			if (!t.tecla[0] && hk[0]) snprintf(t.tecla, sizeof t.tecla, "%s", hk);
			tip_poner(&t);
			if (in.mclick) { in.bloqueo = 1; ejecutar(o); }
		}
		/* teclas: posición sin modificadores, o la letra fija */
		if (!ctrl() && !mayus() && k < 10 && o->tipo != OR_PAGINA && !(o->sc && strlen(o->tip.tecla) <= 2) && tecla(k == 9 ? SDL_SCANCODE_0 : SDL_SCANCODE_1 + k)) ejecutar(o);
		else if (o->sc && tecla(o->sc) && o->sc != SDL_SCANCODE_F3) ejecutar(o);
		else if (o->tipo == OR_PAGINA && in.mclick && over) pagina = (pagina + 1) % paginas;
	}
	/* las letras fijas funcionan aunque su botón esté en otra página */
	for (int k = 0; k < nord; ++k) {
		if (k >= primero && k < primero + n) continue;
		if (ord[k].sc && ord[k].sc != SDL_SCANCODE_F3 && tecla(ord[k].sc)) ejecutar(&ord[k]);
	}
	if (!nord && i0 < 0) {
		gfx_ts(r.x + 4, gy + 4, "Clic: elegir · arrastrar: caja", COL_DIM, 1);
		gfx_ts(r.x + 4, gy + 22, "Clic derecho: ordenar", COL_DIM, 1);
		gfx_ts(r.x + 4, gy + 40, "Mayús: encadenar órdenes", COL_DIM, 1);
		if (L.filas > 2) gfx_ts(r.x + 4, gy + 58, "F1: ayuda con todas las teclas", COL_DIM, 1);
	}
}

/* ---------- ficha de la selección ---------- */

static void retrato(const Entity *e, int x, int y, int sz) {
	gfx_fill_grad(x, y, sz, sz, gfx_rgb(40, 30, 22), gfx_rgb(14, 10, 7));
	Sprite *s = is_unit(e->type) ? icono_unidad(e->type) : is_building(e->type) ? icono_edificio(e->type) : NULL;
	int color = e->owner >= 0 ? pcolor(e->owner) : 0;
	if (s) {
		int sc = MIN((sz - 4) * 256 / s->w, (sz - 4) * 256 / s->h);
		gfx_clip(x + 2, y + 2, x + sz - 2, y + sz - 2);
		gfx_sprite(s, x + sz / 2 - s->w * sc / 512 + s->hx * sc / 256, y + sz / 2 - s->h * sc / 512 + s->hy * sc / 256, sc, color, 256, 0);
		gfx_clip_reset();
	} else {
		int ic = e->type == E_VILLAGER ? IC_ALDEANO : is_unit(e->type) ? IC_SOLDADO : e->type == E_GOLD ? IC_ORO : e->type == E_STONE ? IC_PIEDRA : e->type == E_TREE ? IC_MADERA : e->type == E_BERRIES || e->type == E_FISH ? IC_COMIDA : IC_CONSTRUIR;
		icono(ic, x + 4, y + 4, sz - 8);
	}
	bevel(x, y, sz, sz, e->owner >= 0 ? col_player[color] : COL_GOLD);
	bevel(x + 1, y + 1, sz - 2, sz - 2, gfx_rgb(20, 14, 9));
}

static int stat(int x, int y, int ic, const char *v, int bonus, const char *tit, const char *desc) {
	icono(ic, x, y, 16);
	gfx_ts(x + 19, y, v, COL_CREAM, 1);
	int w = 19 + gfx_t_w(v, 1);
	if (bonus) { char b[12]; snprintf(b, sizeof b, "%+d", bonus); gfx_ts(x + w + 2, y, b, COL_OK, 1); w += 2 + gfx_t_w(b, 1); }
	if (hit(x, y, w, 16)) tip_simple((Rect){ x, y, w, 16 }, tit, desc);
	return w + 10;
}

/* mejoras investigadas que cambian una estadística (0 ataque, 1 armadura, 2 alcance), para la ayuda de la ficha */
static void mejoras_de(int p, EntType t, int stat, char *out, size_t n) {
	const UnitDef *u = &unit_def[t - E_VILLAGER];
	int lista[6], k = 0;
	if (stat == 0) {
		if (is_siege(t)) lista[k++] = T_BALLISTICS;
		else if (u->ranged) { lista[k++] = T_BOW; lista[k++] = T_COMPOSITE; }
		else if (t != E_VILLAGER && t != E_PRIEST) { lista[k++] = T_BRONZE_WEAPONS; lista[k++] = T_IRON_WEAPONS; }
	} else if (stat == 1) {
		lista[k++] = T_LEATHER; lista[k++] = T_SCALE;
		if (t == E_SCOUT || t == E_CAVALRY || t == E_HEAVYCAV) lista[k++] = T_CAVARMOR;
	} else {
		if (is_siege(t)) lista[k++] = T_ENGINEERING; else if (u->ranged) lista[k++] = T_COMPOSITE;
	}
	out[0] = 0;
	for (int i = 0; i < k; ++i) if (has_tech(p, lista[i])) {
		size_t l = strlen(out);
		snprintf(out + l, n - l, "%s%s", out[0] ? ", " : " Mejoras: ", tech_def[lista[i]].name);
	}
}

static void ficha_unidad(const Entity *e, Rect r, int cx, int cy) {
	const UnitDef *u = &unit_def[e->type - E_VILLAGER];
	int p = e->owner >= 0 ? e->owner : -1;
	char b[64], d[220], m[120];
	int x = r.x + 6, y = cy, xmax = r.x + r.w - 6;
	(void)cx;
	if (u->dmg > 0) {
		int ef = (int)eff_attack(p, e->type, 0), base = (int)u->dmg;
		snprintf(b, sizeof b, "%d", base);
		mejoras_de(p, e->type, 0, m, sizeof m);
		snprintf(d, sizeof d, "Daño por golpe: %d base%s. Contra edificios: %d.%s%s", base, ef > base ? " más mejoras y civilización" : "", (int)eff_attack(p, e->type, 1),
			unit_splash(e->type) > 0 ? " Daño en área." : "", m);
		x += stat(x, y, IC_ATAQUE, b, ef - base, "Ataque", d);
	}
	{
		int a0 = eff_armor(p, e->type, 0), a1 = eff_armor(p, e->type, 1);
		snprintf(b, sizeof b, "%d/%d", a0, a1);
		mejoras_de(p, e->type, 1, m, sizeof m);
		snprintf(d, sizeof d, "Cuerpo a cuerpo %d (base %d), a distancia %d (base %d). Cada punto quita 1 de daño por golpe.%s", a0, u->armor, a1, u->parmor, m);
		int bonus = (a0 - u->armor);
		x += stat(x, y, IC_ARMADURA, b, bonus, "Armadura (cuerpo / distancia)", d);
	}
	if (u->range > 1.0f || u->ranged) {
		float rg = eff_range(p, e->type);
		if (x + 60 > xmax) { x = r.x + 6; y += 18; }
		if (unit_min_range(e->type) > 0) snprintf(b, sizeof b, "%.0f-%.0f", unit_min_range(e->type), rg); else snprintf(b, sizeof b, "%.0f", rg);
		mejoras_de(p, e->type, 2, m, sizeof m);
		snprintf(d, sizeof d, "Alcance en casillas (base %.0f).%s%s", u->range, unit_min_range(e->type) > 0 ? " Tiene alcance mínimo." : "", m);
		x += stat(x, y, IC_ALCANCE, b, (int)(rg - u->range + 0.01f), "Alcance", d);
	}
	if (x + 60 > xmax) { x = r.x + 6; y += 18; }
	snprintf(b, sizeof b, "%.1f", u->speed);
	x += stat(x, y, IC_VELOCIDAD, b, 0, "Velocidad", "Casillas por segundo (la Rueda y las civilizaciones la cambian).");
	y += 20;
	if (y + 16 > r.y + r.h) return;
	static const char *kn[] = { "madera", "comida", "oro", "piedra", "una reliquia" };
	if (e->carry > 0 && !is_trader(e->type)) {
		if (e->carry_kind < RES_COUNT) icono_rec(e->carry_kind, r.x + 6, y, 16);
		if (e->carry_kind < RES_COUNT) snprintf(b, sizeof b, "Lleva %d de %s", e->carry, kn[e->carry_kind]);
		else snprintf(b, sizeof b, "Lleva una reliquia");
		gfx_ts(r.x + 32, y, b, COL_AVISO, 1);
	} else if (is_trader(e->type)) {
		/* ruta comercial: estado, y si espera porque falta lo que hay que cargar */
		static const char *rn[] = { "madera", "comida", "oro", "piedra" };
		Entity *dst = ent_get(e->com_destino);
		if (!dst) snprintf(b, sizeof b, "Sin ruta: clic derecho en un %s de otro jugador", e->type == E_TRADESHIP ? "muelle" : "mercado");
		else if (e->com_espera) snprintf(b, sizeof b, "Esperando 15 de %s", rn[CLAMP(e->com_dar, 0, 3)]);
		else snprintf(b, sizeof b, "Ruta con el jugador %d (%.0f casillas) · %s", dst->owner + 1, comercio_distancia(e), e->com_modo ? "intercambio" : "oro");
		gfx_ts(r.x + 6, y, b, e->com_espera ? COL_AVISO : COL_CREAM, 1);
		if (y + 34 <= r.y + r.h) {
			if (W.setup.comercio_mejorado) snprintf(b, sizeof b, "Intercambio: da %s, trae %s (N, M)", rn[CLAMP(e->com_dar, 0, 3)], rn[CLAMP(e->com_recibir, 0, 3)]);
			else snprintf(b, sizeof b, "Intercambio: actívalo al crear la partida");
			gfx_ts(r.x + 6, y + 18, b, COL_DIM, 1);
		}
	} else if (e->type == E_TRANSPORT) {
		snprintf(b, sizeof b, "Pasajeros: %d de 5", e->ncargo);
		gfx_ts(r.x + 6, y, b, COL_CREAM, 1);
	} else if (e->state == ST_ABOARD) {
		gfx_ts(r.x + 6, y, "A bordo de un transporte", COL_DIM, 1);
	} else {
		static const char *st[] = { "Agresiva", "Defensiva", "Pasiva", "Quieta", "Protegiendo" };
		const char *estado = e->patrol ? "Patrullando" : e->nwp ? "Siguiendo una ruta" : e->state == ST_IDLE ? "Inactiva" : e->state == ST_GATHER ? "Recogiendo" : e->state == ST_BUILD ? "Construyendo" : e->state == ST_ATTACK ? "Atacando" : e->state == ST_MOVE ? "En marcha" : e->state == ST_BOARD ? "Va a embarcar" : "";
		if (es_militar(e->type)) snprintf(b, sizeof b, "%s · %s", estado, st[CLAMP(e->stance, 0, 4)]);
		else snprintf(b, sizeof b, "%s", estado);
		gfx_ts(r.x + 6, y, b, COL_DIM, 1);
	}
}

static void ficha_edificio(Entity *e, Rect r, int y) {
	char b[96];
	int x = r.x + 6, w = r.w - 12;
	if (e->progress < 1) {
		snprintf(b, sizeof b, "En obras: %d%%", (int)(e->progress * 100));
		gfx_ts(x, y, b, COL_CREAM, 1);
		barra_progreso(x, y + 18, w, 8, (int)(e->progress * 1000), 1000, gfx_rgb(240, 200, 60));
		return;
	}
	if (e->owner != W.human) return;
	int qy = y;
	if (e->ntrain > 0) {
		/* cola: la primera, grande con su progreso; las demás, pequeñas. Clic: cancelar (devuelve el coste) */
		int sz = 30;
		for (int q = 0; q < e->ntrain; ++q) {
			int qx = x + (q == 0 ? 0 : sz + 6 + (q - 1) * 26), qs = q == 0 ? sz : 24, yy = qy + (q == 0 ? 0 : 3);
			if (qx + qs > r.x + r.w - 4) break;
			gfx_fill(qx, yy, qs, qs, gfx_rgb(20, 14, 9));
			Sprite *s = icono_unidad((EntType)e->train[q]);
			if (s) { int sc = (qs - 2) * 256 / MAX(s->w, s->h); gfx_clip(qx + 1, yy + 1, qx + qs - 1, yy + qs - 1); gfx_sprite(s, qx + qs / 2 - s->w * sc / 512 + s->hx * sc / 256, yy + qs / 2 - s->h * sc / 512 + s->hy * sc / 256, sc, pcolor(W.human), 256, 0); gfx_clip_reset(); }
			else icono(IC_SOLDADO, qx + 2, yy + 2, qs - 4);
			int over = hit(qx, yy, qs, qs);
			bevel(qx, yy, qs, qs, over ? COL_MAL : COL_GOLD);
			if (over) {
				gfx_line(qx + 3, yy + 3, qx + qs - 4, yy + qs - 4, COL_MAL); gfx_line(qx + qs - 4, yy + 3, qx + 3, yy + qs - 4, COL_MAL);
				char t[96]; snprintf(t, sizeof t, "%s (en cola)", unit_def[e->train[q] - E_VILLAGER].name);
				tip_simple((Rect){ qx, yy, qs, qs }, t, "Clic: cancelar y recuperar el coste.");
				if (in.mclick) { in.bloqueo = 1; cmd_cancel_train(W.human, ent_ref((int)(e - W.ents)), q); snd_ui(SND_CLICK); break; }
			}
		}
		float tt = unit_def[e->train[0] - E_VILLAGER].train_time * civ_def[W.pl[W.human].civ].train_time;
		barra_progreso(x, qy + 32, MIN(w, 140), 6, (int)(e->train_time * 100), (int)(tt * 100), gfx_rgb(240, 200, 60));
		int left = (int)(tt - e->train_time + 0.99f);
		snprintf(b, sizeof b, "%d s", MAX(0, left));
		gfx_text(x + MIN(w, 140) + 6, qy + 31, b, COL_DIM, 1);
		qy += 42;
	}
	if (e->tech_cur && qy + 16 <= r.y + r.h) {
		const TechDef *td = &tech_def[e->tech_cur - 1];
		snprintf(b, sizeof b, "%s %d%%", td->name, (int)(100 * e->tech_t / td->time));
		icono(IC_MEJORA, x, qy, 16);
		gfx_ts(x + 20, qy, b, COL_CREAM, 1);
		qy += 18;
	}
	if (e->type == E_TOWN_CENTER && W.pl[W.human].age_t >= 0 && W.pl[W.human].age + 1 < AGE_COUNT && qy + 16 <= r.y + r.h) {
		const AgeDef *ad = &age_def[W.pl[W.human].age + 1];
		snprintf(b, sizeof b, "Hacia la %s %d%%", ad->name, (int)(100 * W.pl[W.human].age_t / ad->time));
		icono(IC_EDAD, x, qy, 16);
		gfx_ts(x + 20, qy, b, COL_AVISO, 1);
		qy += 18;
	}
	if (qy + 16 > r.y + r.h) return;
	if (e->type == E_TEMPLE && e->relics) { snprintf(b, sizeof b, "Reliquias: %d (dan oro)", e->relics); gfx_ts(x, qy, b, COL_AVISO, 1); }
	else if (e->type == E_FARM) { snprintf(b, sizeof b, "Comida: %d", (int)e->amount); icono_rec(RES_FOOD, x, qy, 16); gfx_ts(x + 28, qy, b, COL_CREAM, 1); }
	else if (e->type == E_HOUSE) gfx_ts(x, qy, "Población +5", COL_DIM, 1);
	else if (e->type == E_MARKET) {
		snprintf(b, sizeof b, "Precios: M %d  C %d  P %d", W.pl[W.human].price[0], W.pl[W.human].price[1], W.pl[W.human].price[3]);
		gfx_ts(x, qy, b, COL_DIM, 1);
	} else if (e->type == E_WONDER) {
		int left = MAX(0, 600 - (int)e->timer);
		snprintf(b, sizeof b, "Victoria en %d:%02d", left / 60, left % 60);
		gfx_ts(x, qy, b, COL_OK, 1);
	} else if (!e->ntrain && !e->tech_cur && e->has_rally) gfx_ts(x, qy, "Con punto de reunión", COL_DIM, 1);
}

static void ficha_seleccion(void) {
	Rect r = L.sel;
	panel_hueco(r.x, r.y, r.w, r.h);
	solape_registrar(r.x, r.y, r.w, r.h, "ficha");
	zona(r.x, r.y, r.w, r.h);
	char b[96];
	if (RS.n == 0) {
		gfx_ts(r.x + 8, r.y + 8, "Nada seleccionado", COL_DIM, 1);
		int n = contar_inactivos(0);
		snprintf(b, sizeof b, "Aldeanos inactivos: %d (tecla .)", n);
		gfx_ts(r.x + 8, r.y + 30, b, n ? COL_AVISO : COL_DIM, 1);
		n = contar_inactivos(1);
		snprintf(b, sizeof b, "Soldados inactivos: %d (tecla ,)", n);
		gfx_ts(r.x + 8, r.y + 48, b, n ? COL_AVISO : COL_DIM, 1);
		return;
	}
	if (RS.ntipos > 1 || (RS.n > 1 && is_unit(RS.tipo[0]))) {
		/* varios: un retrato por tipo con su número; clic elige solo ese tipo, Mayús+clic lo quita */
		snprintf(b, sizeof b, "%d seleccionados", RS.n);
		gfx_ts(r.x + 6, r.y + 4, b, COL_GOLD, 1);
		int sz = r.h >= 120 ? 38 : 34, x = r.x + 6, y = r.y + 24;
		for (int k = 0; k < RS.ntipos; ++k) {
			if (x + sz > r.x + r.w - 4) { x = r.x + 6; y += sz + 4; }
			if (y + sz > r.y + r.h - 2) break;
			Entity *e = &W.ents[RS.primero[k]];
			retrato(e, x, y, sz);
			/* vida media del grupo */
			float hp = 0, mh = 0;
			for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel && (int)W.ents[i].type == RS.tipo[k]) { hp += W.ents[i].hp; mh += W.ents[i].maxhp; }
			gfx_fill(x + 2, y + sz - 5, sz - 4, 3, gfx_rgb(40, 10, 8));
			gfx_fill(x + 2, y + sz - 5, (int)((sz - 4) * (mh > 0 ? hp / mh : 0)), 3, gfx_rgb(90, 220, 80));
			snprintf(b, sizeof b, "%d", RS.cuenta[k]);
			int bw = gfx_t_w(b, 1) + 4;
			gfx_fill_a(x + sz - bw, y, bw, 16, gfx_rgb(0, 0, 0), 170);
			gfx_ts(x + sz - bw + 2, y, b, COL_CREAM, 1);
			if (hit(x, y, sz, sz)) {
				const char *nm = is_unit(e->type) ? unit_def[e->type - E_VILLAGER].name : is_building(e->type) ? bld_def[e->type].name : "Recurso";
				char d[96]; snprintf(d, sizeof d, "%d en la selección. Clic: solo estos. Mayús+clic: quitarlos.", RS.cuenta[k]);
				tip_simple((Rect){ x, y, sz, sz }, nm, d);
				if (in.mclick) {
					in.bloqueo = 1;
					for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel) W.ents[i].sel = mayus() ? (int)W.ents[i].type != RS.tipo[k] : (int)W.ents[i].type == RS.tipo[k];
					AOEN_LOG("[AOEN] selección filtrada por tipo: %s", nm);
				}
			}
			x += sz + 4;
		}
		return;
	}
	Entity *e = &W.ents[RS.primero[0]];
	int ps = r.h >= 120 ? 54 : 46;
	retrato(e, r.x + 6, r.y + 6, ps);
	const char *name = is_building(e->type) ? bld_def[e->type].name : is_unit(e->type) ? unit_def[e->type - E_VILLAGER].name : e->type == E_TREE ? "Árbol" : e->type == E_GOLD ? "Mina de oro" : e->type == E_STONE ? "Cantera" : e->type == E_BERRIES ? "Bayas" : e->type == E_FISH ? "Banco de peces" : e->type == E_RELIC ? "Reliquia" : e->type == E_CARCASS ? "Carne" : "Recurso";
	int tx = r.x + ps + 14, tw = r.x + r.w - 6 - tx;
	gfx_ts(tx, r.y + 6, name, COL_GOLD, 1);
	if (e->owner >= 0) {
		snprintf(b, sizeof b, "%s · %s", e->owner == W.human ? "Tú" : dipl_name(W.pl[W.human].dipl[e->owner]), civ_def[W.pl[e->owner].civ].name);
		gfx_fill(tx, r.y + 26, 10, 10, col_player[pcolor(e->owner)]);
		gfx_text(tx + 14, r.y + 28, b, COL_DIM, 1);
	} else if (is_resource(e->type)) {
		snprintf(b, sizeof b, "Quedan %d", (int)e->amount);
		gfx_ts(tx, r.y + 24, b, COL_CREAM, 1);
	}
	if (!is_resource(e->type) || e->type == E_CARCASS) {
		/* vida */
		int hy = r.y + 40;
		float f = e->maxhp > 0 ? e->hp / e->maxhp : 0;
		gfx_fill(tx, hy, tw, 12, gfx_rgb(40, 10, 8));
		u32 c0 = f > 0.5f ? gfx_rgb(110, 235, 90) : f > 0.25f ? gfx_rgb(250, 225, 70) : gfx_rgb(250, 90, 70);
		gfx_fill_grad(tx, hy, (int)(tw * CLAMP(f, 0, 1)), 12, c0, (c0 >> 1 & 0x7f7f7f) | 0xff000000u);
		gfx_rect(tx, hy, tw, 12, gfx_rgb(10, 8, 6));
		snprintf(b, sizeof b, "%d/%d", (int)ceilf(e->hp), (int)e->maxhp);
		gfx_text(tx + tw / 2 - gfx_text_w(b, 1) / 2, hy + 3, b, COL_CREAM, 1);
		if (hit(tx, hy, tw, 12)) tip_simple((Rect){ tx, hy, tw, 12 }, "Puntos de vida", NULL);
	}
	int cy = r.y + ps + 12;
	if (is_unit(e->type)) ficha_unidad(e, r, tx, cy);
	else if (is_building(e->type)) ficha_edificio(e, r, cy);
}

/* ---------- minimapa y sus botones ---------- */

static void boton_esquina(Rect b, int ic, int n, int activo, const char *titulo, const char *desc, int conteo) {
	int over = hit_r(b);
	gfx_fill_grad(b.x, b.y, b.w, b.h, over ? gfx_rgb(90, 60, 36) : gfx_rgb(46, 31, 20), gfx_rgb(22, 15, 10));
	icono(ic, b.x + 2, b.y + 2, b.w - 4);
	if (conteo && !n) gfx_fill_a(b.x + 1, b.y + 1, b.w - 2, b.h - 2, gfx_rgb(20, 16, 14), 130);
	bevel(b.x, b.y, b.w, b.h, activo ? COL_CREAM : COL_GOLD);
	if (conteo && n) {
		char c[8]; snprintf(c, sizeof c, "%d", n);
		int cw = gfx_text_w(c, 1) + 4;
		gfx_fill(b.x + b.w - cw, b.y + b.h - 9, cw, 9, gfx_rgb(170, 30, 20));
		gfx_text(b.x + b.w - cw + 2, b.y + b.h - 8, c, COL_CREAM, 1);
	}
	solape_registrar(b.x, b.y, b.w, b.h, "boton minimapa");
	if (over) tip_simple(b, titulo, desc);
}

static void minimapa(void) {
	Rect m = L.mm;
	render_minimap(m.x, m.y, m.w, m.h);
	solape_registrar(m.x + m.w / 4, m.y + m.h / 4, m.w / 2, m.h / 2, "minimapa");
	zona(m.x - 3, m.y - 3, m.w + 6, m.h + 6);
	int na = contar_inactivos(0), nm = contar_inactivos(1);
	boton_esquina(L.inactivo_ald, IC_ALDEANO, na, 0, "Aldeano inactivo (.)", "Elige el siguiente aldeano parado y centra la vista en él.", 1);
	if (hit_r(L.inactivo_ald) && in.mclick) { in.bloqueo = 1; siguiente_inactivo(0); }
	boton_esquina(L.inactivo_mil, IC_SOLDADO, nm, 0, "Soldado inactivo (,)", "Elige el siguiente soldado parado.", 1);
	if (hit_r(L.inactivo_mil) && in.mclick) { in.bloqueo = 1; siguiente_inactivo(1); }
	boton_esquina(L.filtro_rec, IC_RECURSOS, 0, mm_filtro & MM_RECURSOS, "Minimapa: recursos", "Muestra el oro, la piedra, las bayas y los peces.", 0);
	if (hit_r(L.filtro_rec) && in.mclick) { in.bloqueo = 1; mm_filtro ^= MM_RECURSOS; }
	boton_esquina(L.filtro_mil, IC_MILITAR, 0, mm_filtro & MM_MILITAR, "Minimapa: solo ejércitos", "Oculta aldeanos y barcos; los edificios, apagados.", 0);
	if (hit_r(L.filtro_mil) && in.mclick) { in.bloqueo = 1; mm_filtro ^= MM_MILITAR; }
	/* clic izquierdo: mover la vista (también arrastrando) */
	if ((in.mdown || in.mclick) && !in.bloqueo && hit(m.x, m.y, m.w, m.h) && !selecting) {
		float wx, wy;
		if (minimap_pick(m.x, m.y, m.w, m.h, in.mx / app.us, in.my / app.us, &wx, &wy)) view_center_on(wx, wy);
	}
}

/* ---------- barra inferior y dibujo general ---------- */

void draw_hud(void) {
	Player *pl = &W.pl[W.human];
	(void)pl;
	nzonas = 0;
	resumir_seleccion();
	construir_ordenes();
	dibujar_cuentas();
	dibujar_avisos();
	barra_superior();
	fondo_barra(0, L.inf.y, fb.w, bottom_h, 0);
	ficha_seleccion();
	rejilla_ordenes();
	minimapa();
	if (marcador_modo != 2 && panel_abierto == PANEL_NINGUNO) draw_marcador(fb.w - 8, top_h + 6, fb.w - 440);
	/* línea de mensajes: lo que no es ya un aviso */
	if (W.msg_t > 0 && W.msg[0] && !mensaje_repetido(W.msg)) {
		int tw = gfx_t_w(W.msg, 1) + 24, x = fb.w / 2 - tw / 2, y = L.inf.y - 30;
		gfx_fill_a(x, y, tw, 22, gfx_rgb(16, 11, 7), 190);
		gfx_ts(x + 12, y + 3, W.msg, COL_AVISO, 1);
	}
	/* modo de orden o colocación: una línea junto al ratón */
	{
		const char *h = NULL;
		char b[96];
		switch (modo) {
		case MODO_PATRULLA: h = "Patrullar: clic en el destino"; break;
		case MODO_REUNION: h = "Punto de reunión: clic en el mapa"; break;
		case MODO_DESEMBARCAR: h = "Desembarcar: clic en la orilla"; break;
		case MODO_EMBARCAR: h = "Embarcar: clic en un transporte"; break;
		case MODO_PROTEGER: h = "Proteger: clic en un aliado"; break;
		}
		if (!h && placing != E_TYPE_COUNT) { snprintf(b, sizeof b, "Construir %s: clic en el mapa (Mayús: varios)", bld_def[placing].name); h = b; }
		if (h) {
			int mx = in.mx / app.us + 18, my = in.my / app.us + 14, tw = gfx_t_w(h, 1) + 10;
			mx = MIN(mx, fb.w - tw - 4); my = MIN(my, L.inf.y - 24);
			gfx_fill_a(mx, my, tw + gfx_text_w(" (Esc cancela)", 1) + 4, 20, gfx_rgb(16, 11, 7), 200);
			gfx_ts(mx + 5, my + 2, h, COL_AVISO, 1);
			gfx_text(mx + tw, my + 7, " (Esc cancela)", COL_DIM, 1);
		}
	}
	memcpy(zonas_prev, zonas, sizeof(Rect) * (size_t)nzonas);
	nzonas_prev = nzonas;
}

/* ---------- lo que hay bajo el ratón (v8.5.5.3): aro, rótulo y cursor ---------- */
static int raton_ent = -1;          /* entidad bajo el ratón en el mapa (-1: nada) */
static Uint32 raton_desde;          /* desde cuándo está ahí (el rótulo sale tras una pausa corta) */

static int es_animal(EntType t) { return t == E_HORSE || t == E_ELEPHANT; }
static int hostil(const Entity *e) { return (e->owner >= 0 && !same_team(e->owner, W.human)) || (e->owner < 0 && es_animal(e->type)); }

static void raton_actualizar(void) {
	int antes = raton_ent;
	raton_ent = -1;
	if (!in.mouse_seen || !(in.foco || prueba_activa) || app.paused || W.over || selecting || en_interfaz() || placing != E_TYPE_COUNT || !W.fog) { raton_desde = SDL_GetTicks(); return; }
	float wx, wy;
	screen_to_world(in.mx, in.my, &wx, &wy);
	raton_ent = pick_entity(wx, wy);
	if (raton_ent != antes) raton_desde = SDL_GetTicks();
}

/* cursor de la partida: qué haría el clic derecho con lo que hay debajo */
int hud_cursor(int *dx, int *dy) {
	*dx = *dy = 0;
	if (app.paused || W.over || panel_modal()) return CUR_FLECHA;
	int sw = gfx_real_w(), sh = gfx_real_h();
	if (in.foco) {
		if (in.mx <= 2) *dx = -1; else if (in.mx >= sw - 3) *dx = 1;
		if (in.my <= 2) *dy = -1; else if (in.my >= sh - 3) *dy = 1;
		if (*dx || *dy) return CUR_DESPLAZAR;
	}
	if (en_interfaz()) return CUR_FLECHA;
	if (placing != E_TYPE_COUNT) {
		float wx, wy;
		screen_to_world(in.mx, in.my, &wx, &wy);
		int s = bld_def[placing].size;
		return can_place(placing, (int)floorf(wx - s / 2.0f + 0.5f), (int)floorf(wy - s / 2.0f + 0.5f)) ? CUR_CONSTRUIR : CUR_PROHIBIDO;
	}
	if (modo != MODO_NADA) return CUR_OBJETIVO;
	if (RS.propias == 0 || raton_ent < 0) return CUR_FLECHA;
	const Entity *e = &W.ents[raton_ent];
	if (hostil(e)) return CUR_ATACAR;
	if (RS.aldeanos > 0) {
		switch (e->type) {
		case E_TREE: return CUR_MADERA;
		case E_GOLD: return CUR_ORO;
		case E_STONE: return CUR_PIEDRA;
		case E_BERRIES: case E_FISH: case E_CARCASS: return CUR_COMIDA;
		default: break;
		}
		if (is_building(e->type) && e->owner == W.human && (e->progress < 1 || e->hp < e->maxhp)) return CUR_CONSTRUIR;
		if (e->type == E_FARM && e->owner == W.human) return CUR_COMIDA;
	}
	return CUR_FLECHA;
}

static const char *nombre_ent(const Entity *e) {
	if (is_building(e->type)) return bld_def[e->type].name;
	if (is_unit(e->type)) return unit_def[e->type - E_VILLAGER].name;
	switch (e->type) {
	case E_TREE: return "Árbol"; case E_GOLD: return "Mina de oro"; case E_STONE: return "Cantera de piedra";
	case E_BERRIES: return "Arbustos de bayas"; case E_FISH: return "Banco de peces"; case E_CARCASS: return "Presa";
	case E_RELIC: return "Reliquia";
	default: return "?";
	}
}

/* rótulo pequeño junto al ratón: nombre, dueño y vida o recurso que queda */
void hud_rotulo_raton(void) {
	if (!app.nombres_raton || raton_ent < 0 || raton_ent >= W.nents || !W.ents[raton_ent].alive) return;
	if (SDL_GetTicks() - raton_desde < 350 || tip_actual()) return;
	const Entity *e = &W.ents[raton_ent];
	char l1[96], l2[96] = "";
	snprintf(l1, sizeof l1, "%s", nombre_ent(e));
	u32 c1 = COL_CREAM;
	if (e->owner >= 0) {
		char nb[64];
		snprintf(l2, sizeof l2, "%s · %d/%d", e->owner == W.human ? "Tú" : nombre_jugador(e->owner, nb, sizeof nb), (int)ceilf(e->hp), (int)e->maxhp);
		c1 = e->owner == W.human ? COL_CREAM : same_team(e->owner, W.human) ? COL_OK : COL_MAL;
		if (is_building(e->type) && e->progress < 1) snprintf(l2, sizeof l2, "En obras: %d%%", (int)(e->progress * 100));
	} else if (is_resource(e->type) && e->type != E_RELIC) snprintf(l2, sizeof l2, "Quedan %d", (int)ceilf(e->amount));
	else if (es_animal(e->type)) { snprintf(l2, sizeof l2, "Salvaje · %d/%d", (int)ceilf(e->hp), (int)e->maxhp); c1 = COL_AVISO; }
	int w = MAX(gfx_t_w(l1, 1), gfx_t_w(l2, 1)) + 12, h = l2[0] ? 40 : 22;
	int x = in.mx / app.us + 16, y = in.my / app.us + 20;
	if (x + w > fb.w - 4) x = in.mx / app.us - w - 8;
	if (y + h > L.inf.y - 2) y = in.my / app.us - h - 8;
	gfx_fill_a(x, y, w, h, gfx_rgb(16, 11, 7), 205);
	bevel(x, y, w, h, gfx_rgb(120, 92, 52));
	gfx_ts(x + 6, y + 3, l1, c1, 1);
	if (l2[0]) gfx_ts(x + 6, y + 21, l2, COL_DIM, 1);
}

void hud_mundo(void) {
	raton_actualizar();
	/* aro de lo que hay bajo el ratón: blanco lo propio, verde lo aliado, rojo lo enemigo, oro los recursos */
	if (raton_ent >= 0 && !W.ents[raton_ent].sel) {
		const Entity *e = &W.ents[raton_ent];
		int sx, sy;
		view_to_screen(e->x, e->y, &sx, &sy);
		u32 col = e->owner == W.human ? gfx_rgb(240, 236, 220) : e->owner >= 0 ? (same_team(e->owner, W.human) ? COL_OK : gfx_rgb(255, 80, 64)) : es_animal(e->type) ? COL_AVISO : COL_GOLD;
		float z = V.zoom;
		int rx = is_building(e->type) ? (int)(e->size * 22 * z) : e->type == E_TREE ? (int)(14 * z) : (int)(12 * z);
		int ry = MAX(3, rx / 2);
		int g = MAX(1, (int)(z + 0.5f));
		aro_elipse(sx, sy + 1, rx, ry, gfx_rgb(10, 8, 6), g + 1);
		aro_elipse(sx, sy, rx, ry, col, g);
	}
	if (placing != E_TYPE_COUNT && !app.paused) {
		float wx, wy;
		screen_to_world(in.mx, in.my, &wx, &wy);
		int s = bld_def[placing].size;
		render_placement(placing, (int)floorf(wx - s / 2.0f + 0.5f), (int)floorf(wy - s / 2.0f + 0.5f));
	}
	if (selecting && (abs(in.mx - sel_x0) > 5 || abs(in.my - sel_y0) > 5)) {
		int x = MIN(in.mx, sel_x0), y = MIN(in.my, sel_y0), w = abs(in.mx - sel_x0), h = abs(in.my - sel_y0);
		gfx_fill_a(x, y, w, h, gfx_rgb(120, 255, 120), 28);
		gfx_rect(x, y, w, h, gfx_rgb(200, 255, 200));
	}
}

/* ---------- entrada en la partida ---------- */

/* reparte las unidades seleccionadas alrededor de un punto (formación en cuadro) y les da la orden */
static void ordenar_seleccion(float wx, float wy, int objetivo, int encolar) {
	int n = 0;
	for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human && is_unit(W.ents[i].type) && W.ents[i].state != ST_ABOARD) ++n;
	if (!n) return;
	AOEN_LOG("[AOEN] orden %s: %d unidades hacia %.1f,%.1f", objetivo >= 0 ? "objetivo" : encolar ? "en cola" : "mover", n, wx, wy);
	int k = 0, cols = (int)ceilf(sqrtf((float)n));
	for (int i = 0; i < W.nents; ++i) {
		Entity *u = &W.ents[i];
		if (!u->alive || !u->sel || u->owner != W.human || !is_unit(u->type) || u->state == ST_ABOARD) continue;
		float ox = (k % cols - cols / 2) * 0.7f, oy = (k / cols - cols / 2) * 0.7f;
		if (objetivo >= 0 && objetivo != i) cmd_target(ent_ref(i), ent_ref(objetivo));
		else if (encolar) cmd_move_queue(ent_ref(i), wx + ox, wy + oy);
		else cmd_move(ent_ref(i), wx + ox, wy + oy);
		++k;
	}
	int hostil = objetivo >= 0 && W.ents[objetivo].owner >= 0 && !same_team(W.ents[objetivo].owner, W.human);
	marca_orden(wx, wy, hostil ? 1 : 0);
}

/* ejecuta el modo de orden pendiente en el punto (wx, wy); devuelve 1 si había modo */
static int aplicar_modo(float wx, float wy) {
	if (modo == MODO_NADA) return 0;
	int t = pick_entity(wx, wy), n = 0;
	switch (modo) {
	case MODO_PATRULLA:
		for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human && es_militar(W.ents[i].type)) { cmd_patrol(ent_ref(i), wx, wy); ++n; }
		AOEN_LOG("[AOEN] patrulla: %d unidades hacia %.1f,%.1f", n, wx, wy);
		marca_orden(wx, wy, 2);
		break;
	case MODO_REUNION:
		if (sel_edificio >= 0) {
			cmd_set_rally(ent_ref(sel_edificio), wx, wy, t >= 0 && t != sel_edificio ? ent_ref(t) : (Ref){ -1, 0 });
			AOEN_LOG("[AOEN] punto de reunión en %.1f,%.1f", wx, wy);
			marca_orden(wx, wy, 2);
		}
		break;
	case MODO_DESEMBARCAR:
		if (tile_walk((int)floorf(wx), (int)floorf(wy)) || (W.tile[CLAMP((int)wy, 0, W.h - 1) * W.w + CLAMP((int)wx, 0, W.w - 1)] < TILE_WATER)) {
			for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human && W.ents[i].type == E_TRANSPORT) cmd_move(ent_ref(i), wx, wy);
			marca_orden(wx, wy, 0);
		} else world_msg("Hay que elegir tierra firme");
		break;
	case MODO_EMBARCAR:
		if (t >= 0 && W.ents[t].type == E_TRANSPORT && W.ents[t].owner == W.human) {
			for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human && is_unit(W.ents[i].type) && !is_naval(W.ents[i].type)) cmd_target(ent_ref(i), ent_ref(t));
		} else world_msg("Hay que elegir un transporte propio");
		break;
	case MODO_PROTEGER:
		if (t >= 0 && same_team(W.ents[t].owner, W.human)) {
			for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human && i != t) cmd_guard(ent_ref(i), ent_ref(t));
			world_msg("Protegiendo");
		} else world_msg("Hay que elegir un aliado");
		break;
	}
	modo = MODO_NADA;
	return 1;
}

void game_input(float dt) {
	int sw = gfx_real_w(), sh = gfx_real_h();
	int fuera = en_interfaz();

	/* cámara: flechas, WASD y bordes de la pantalla */
	float sp = (app.scroll_speed == 1 ? 450.0f : app.scroll_speed == 2 ? 700.0f : 1000.0f) * dt / V.zoom;
	int dx = 0, dy = 0;
	{
		if (in.keys[SDL_SCANCODE_LEFT] || in.keys[SDL_SCANCODE_A]) --dx;
		if (in.keys[SDL_SCANCODE_RIGHT] || in.keys[SDL_SCANCODE_D]) ++dx;
		if (in.keys[SDL_SCANCODE_UP] || in.keys[SDL_SCANCODE_W]) --dy;
		if (in.keys[SDL_SCANCODE_DOWN] || in.keys[SDL_SCANCODE_S]) ++dy;
	}
	if (in.mouse_seen && in.foco) {
		if (in.mx <= 2) --dx; else if (in.mx >= sw - 3) ++dx;
		if (in.my <= 2) --dy; else if (in.my >= sh - 3) ++dy;
	}
	V.cx += CLAMP(dx, -1, 1) * sp;
	V.cy += CLAMP(dy, -1, 1) * sp;

	if (in.wheel && !fuera) {
		float wx, wy;
		screen_to_world(in.mx, in.my, &wx, &wy);
		V.zoom = CLAMP(V.zoom * powf(1.12f, (float)in.wheel), 0.4f, 2.5f);
		float px, py;
		view_proj(wx, wy, &px, &py);
		V.cx = px - (in.mx - sw * 0.5f) / V.zoom;
		V.cy = py - (in.my - sh * 0.5f) / V.zoom;
	}
	view_clamp();

	if (tecla(SDL_SCANCODE_H)) {
		for (int i = 0; i < W.nents; ++i)
			if (W.ents[i].alive && W.ents[i].owner == W.human && W.ents[i].type == E_TOWN_CENTER) { view_center_on(W.ents[i].x, W.ents[i].y); break; }
	}
	if (tecla(SDL_SCANCODE_PERIOD)) siguiente_inactivo(0);
	if (tecla(SDL_SCANCODE_COMMA)) siguiente_inactivo(1);
	if (tecla(SDL_SCANCODE_SPACE)) saltar_aviso();
	if (tecla(SDL_SCANCODE_EQUALS) || tecla(SDL_SCANCODE_KP_PLUS)) app.game_speed = MIN(4, app.game_speed + 1);
	if (tecla(SDL_SCANCODE_MINUS) || tecla(SDL_SCANCODE_KP_MINUS)) app.game_speed = MAX(1, app.game_speed - 1);

	/* grupos: Ctrl+número guarda la selección, Mayús+número la recupera */
	{
		static Ref groups[10][96];
		static int gcount[10];
		for (int k = 0; k < 10; ++k) {
			int sc = k == 9 ? SDL_SCANCODE_0 : SDL_SCANCODE_1 + k;
			if (!tecla(sc)) continue;
			if (ctrl()) {
				gcount[k] = 0;
				for (int i = 0; i < W.nents && gcount[k] < 96; ++i)
					if (W.ents[i].alive && W.ents[i].sel && W.ents[i].owner == W.human) groups[k][gcount[k]++] = ent_ref(i);
				if (gcount[k]) { char m[48]; snprintf(m, sizeof m, "Grupo %d guardado (Mayús+%d lo elige)", (k + 1) % 10, (k + 1) % 10); world_msg(m); }
			} else if (mayus() && gcount[k]) {
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

	if (tecla(SDL_SCANCODE_TAB)) marcador_modo = (marcador_modo + 1) % 3;
	if (tecla(SDL_SCANCODE_F1)) panel_abierto = panel_abierto == PANEL_AYUDA ? PANEL_NINGUNO : PANEL_AYUDA;
	if (tecla(SDL_SCANCODE_F3)) panel_abierto = panel_abierto == PANEL_DIPLOMACIA ? PANEL_NINGUNO : PANEL_DIPLOMACIA;
	if (tecla(SDL_SCANCODE_F5)) do_save(1);
	if (tecla(SDL_SCANCODE_F9)) { if (any_save()) do_load(1); return; }
	if (tecla(SDL_SCANCODE_F10)) app.paused = 1;
	if (tecla(SDL_SCANCODE_ESCAPE)) {
		if (modo != MODO_NADA) modo = MODO_NADA;
		else if (placing != E_TYPE_COUNT) placing = E_TYPE_COUNT;
		else if (panel_abierto != PANEL_NINGUNO) panel_abierto = PANEL_NINGUNO;
		else app.paused = 1;
		in.key[SDL_SCANCODE_ESCAPE] = 0;   /* la misma pulsación no cierra la pausa recién abierta */
	}

	/* colocación de edificios */
	if (placing != E_TYPE_COUNT) {
		if (in.rclick) placing = E_TYPE_COUNT;
		else if (in.mclick && !fuera) {
			float wx, wy;
			screen_to_world(in.mx, in.my, &wx, &wy);
			int s = bld_def[placing].size;
			int tx = (int)floorf(wx - s / 2.0f + 0.5f), ty = (int)floorf(wy - s / 2.0f + 0.5f);
			int built = 0;
			for (int i = 0; i < W.nents && !built; ++i) {
				Entity *u = &W.ents[i];
				if (u->alive && u->sel && u->type == E_VILLAGER && u->owner == W.human) {
					built = cmd_build(W.human, ent_ref(i), placing, tx, ty);
					if (built) {
						Ref b = { -1, 0 };
						for (int j = W.nents - 1; j >= 0; --j) if (W.ents[j].alive && W.ents[j].type == placing && W.ents[j].owner == W.human && W.ents[j].bx == tx && W.ents[j].by == ty) { b = ent_ref(j); break; }
						for (int j = 0; j < W.nents; ++j)
							if (j != i && W.ents[j].alive && W.ents[j].sel && W.ents[j].type == E_VILLAGER && b.idx >= 0) cmd_target(ent_ref(j), b);
					}
				}
			}
			if (built) snd_ui(SND_PLACE);
			if (built && !mayus()) placing = E_TYPE_COUNT;
		}
		return;
	}

	/* un modo de orden pendiente: el clic izquierdo en el mapa elige el destino; el derecho lo cancela */
	if (modo != MODO_NADA) {
		if (in.rclick && !fuera) { modo = MODO_NADA; in.rclick = 0; }
		else if (in.mclick && !fuera) {
			float wx, wy;
			screen_to_world(in.mx, in.my, &wx, &wy);
			aplicar_modo(wx, wy);
			in.mclick = 0;
		}
		return;
	}

	/* selección: clic, caja, Mayús para añadir y doble clic para todos los del tipo */
	if ((in.mdown || in.mpressed) && !selecting && !fuera) { selecting = 1; sel_x0 = in.mpressed ? in.mdx : in.mx; sel_y0 = in.mpressed ? in.mdy : in.my; }
	if (!in.mdown && selecting) {
		selecting = 0;
		if (abs(in.mx - sel_x0) > 5 || abs(in.my - sel_y0) > 5) {
			select_box(sel_x0, sel_y0, in.mx, in.my, mayus());
			int ns = 0; for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].sel) ++ns;
			AOEN_LOG("[AOEN] selección con caja: %d", ns);
		} else {
			float wx, wy;
			screen_to_world(in.mx, in.my, &wx, &wy);
			int i = pick_entity(wx, wy);
			static Uint32 ult_t; static int ult_i = -1;
			Uint32 now = SDL_GetTicks();
			if (i >= 0 && i == ult_i && now - ult_t < 400 && W.ents[i].owner == W.human && is_unit(W.ents[i].type)) {
				seleccionar_tipo_visible(W.ents[i].type);
				AOEN_LOG("[AOEN] doble clic: todos los del tipo");
			} else if (mayus() && i >= 0 && W.ents[i].owner == W.human && is_unit(W.ents[i].type)) {
				W.ents[i].sel = !W.ents[i].sel;
			} else {
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
			ult_t = now; ult_i = i;
		}
	}

	/* clic derecho en el minimapa: las unidades seleccionadas van allí (Mayús: en cola) */
	if (in.rclick) {
		Rect m = L.mm;
		float wx, wy;
		if (hit(m.x, m.y, m.w, m.h) && minimap_pick(m.x, m.y, m.w, m.h, in.mx / app.us, in.my / app.us, &wx, &wy)) {
			ordenar_seleccion(wx, wy, -1, mayus());
			in.rclick = 0;
		}
	}
	if (!in.rclick || fuera) return;

	float wx, wy;
	screen_to_world(in.mx, in.my, &wx, &wy);
	int t = pick_entity(wx, wy);
	/* edificio de producción seleccionado: punto de reunión */
	int sb = first_selected();
	if (sb >= 0 && W.ents[sb].owner == W.human && is_building(W.ents[sb].type) && W.ents[sb].progress >= 1 && ent_is_trainer(W.ents[sb].type)) {
		cmd_set_rally(ent_ref(sb), wx, wy, t >= 0 && t != sb ? ent_ref(t) : (Ref){ -1, 0 });
		world_msg(t >= 0 && t != sb ? "Punto de reunión: las unidades irán a trabajar o a atacar allí" : "Punto de reunión fijado");
		AOEN_LOG("[AOEN] punto de reunión en %.1f,%.1f", wx, wy);
		marca_orden(wx, wy, 2);
		return;
	}
	ordenar_seleccion(wx, wy, mayus() ? -1 : t, mayus());
}

/* ---------- ayudas para las pruebas ---------- */
int hud_orden_pos(int k, int *x, int *y) {
	if (k < 0 || k >= L.cols * L.filas) return 0;
	int c = k % L.cols, f = k / L.cols;
	*x = (L.ord.x + c * (L.bsz + L.gap) + L.bsz / 2) * app.us;
	*y = (L.ord.y + 14 + f * (L.bsz + L.gap) + L.bsz / 2) * app.us;
	return 1;
}

void hud_mm_pos(float wx, float wy, int *x, int *y) {
	float u = wx / W.w, v = wy / W.h;
	*x = (L.mm.x + (int)((u + v) * 0.5f * L.mm.w)) * app.us;
	*y = (L.mm.y + (int)((0.5f + (u - v) * 0.5f) * L.mm.h)) * app.us;
}
