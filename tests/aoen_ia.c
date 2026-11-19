/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Arnés de h389 para el anfitrión: partidas de AoEN de IA contra IA, sin ventana y a toda velocidad.
 *   aoen_ia SEMILLA DIF_A DIF_B [MAPA] [TAMAÑO] [MINUTOS] [JUGADORES]
 * Los jugadores pares son del equipo 1 con dificultad DIF_A y los impares del equipo 2 con DIF_B
 * (0 fácil, 1 normal, 2 difícil, 3 muy difícil). Imprime una línea por jugador y al final
 *   resultado ganador=EQUIPO tiempo=SEGUNDOS
 * (ganador=0 si se acaba el tiempo: gana entonces quien tiene más puntuación, «ganador_puntos=»). */
#include "../userland/juegos/aoen/game.h"
#include "../userland/juegos/aoen/ia.h"
#include "../userland/juegos/aoen/hilos.h"

/* Comprobación de terreno: ningún barco fuera del agua y ninguna unidad de tierra en el agua (salvo a bordo o muriendo).
 * Cuenta las unidades mal colocadas en este momento; la primera de cada partida se cuenta en stderr. */
static long terreno_malas, terreno_comprobaciones;
static void comprobar_terreno(void) {
	++terreno_comprobaciones;
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!e->alive || !is_unit(e->type) || e->state == ST_DYING || e->state == ST_ABOARD) continue;
		int x = (int)floorf(e->x), y = (int)floorf(e->y);
		if (x < 0 || y < 0 || x >= W.w || y >= W.h) { if (!terreno_malas++) fprintf(stderr, "terreno: %s fuera del mapa en %.2f,%.2f\n", unit_def[e->type - E_VILLAGER].name, e->x, e->y); continue; }
		int agua = W.tile[y * W.w + x] >= TILE_WATER;
		if (agua != is_naval(e->type)) {
			if (!terreno_malas++) fprintf(stderr, "terreno: %s del jugador %d en %s (%.2f,%.2f) estado %d t=%.1f\n", unit_def[e->type - E_VILLAGER].name, e->owner, agua ? "agua" : "tierra", e->x, e->y, e->state, W.time);
		}
	}
}

/* AOEN_IA_AZAR=SEGUNDOS: sin IA, cada jugador recibe barcos (en agua) y soldados y aldeanos (en tierra) junto a su centro
 * urbano y, cada medio segundo, órdenes al azar (moverse a cualquier sitio del mapa, atacar o seguir a cualquier entidad,
 * embarcar, desembarcar en cualquier punto, cola, patrulla). Al final imprime «terreno malas=N comprobaciones=M». */
static unsigned azar_s = 12345;
static int azar(int n) { azar_s = azar_s * 1103515245u + 12345u; return (int)((azar_s >> 8) % (unsigned)n); }
static void prueba_azar(float segundos) {
	int np = W.setup.players;
	for (int p = 0; p < np; ++p) W.pl[p].ai = 0;
	for (int p = 0; p < np; ++p) {
		float cx = 0, cy = 0;
		for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER && W.ents[i].owner == p) { cx = W.ents[i].x; cy = W.ents[i].y; }
		static const EntType barcos[] = { E_WARSHIP, E_WARSHIP, E_TRANSPORT, E_TRANSPORT, E_FISHBOAT, E_TRADESHIP };
		static const EntType tierra[] = { E_CLUBMAN, E_CLUBMAN, E_VILLAGER, E_VILLAGER, E_BOWMAN, E_SCOUT, E_PRIEST, E_CATAPULT };
		int nb = 0, nt = 0;
		for (int r = 2; r < 40 && (nb < 6 || nt < 8); ++r)
			for (int k = 0; k < 16 && (nb < 6 || nt < 8); ++k) {
				int x = (int)cx + azar(2 * r + 1) - r, y = (int)cy + azar(2 * r + 1) - r;
				if (nb < 6 && tile_sail(x, y)) { dev_place(barcos[nb], p, x + 0.5f, y + 0.5f); ++nb; }
				else if (nt < 8 && tile_walk(x, y)) { dev_place(tierra[nt], p, x + 0.5f, y + 0.5f); ++nt; }
			}
		fprintf(stderr, "azar: jugador %d con %d barcos y %d de tierra\n", p, nb, nt);
	}
	float next = 0, rec_barcos = 0, rec_tierra = 0;
	static float px[1 << 16], py[1 << 16];
	while (W.time < segundos) {
		int n0 = MIN(W.nents, 1 << 16);
		for (int i = 0; i < n0; ++i) { px[i] = W.ents[i].x; py[i] = W.ents[i].y; }
		world_update(1.0f / 30);
		comprobar_terreno();
		for (int i = 0; i < n0; ++i) {   /* lo que anda cada clase de unidad: que nadie se quede clavado en la orilla */
			Entity *e = &W.ents[i];
			if (!e->alive || !is_unit(e->type) || e->owner < 0 || e->state == ST_ABOARD) continue;
			float d = fabsf(e->x - px[i]) + fabsf(e->y - py[i]);
			if (d > 1.0f) continue;   /* desembarco o aparición: no es andar */
			if (is_naval(e->type)) rec_barcos += d; else rec_tierra += d;
		}
		if (W.time < next) continue;
		next += 0.5f;
		for (int i = 0; i < W.nents; ++i) {
			Entity *e = &W.ents[i];
			if (!e->alive || !is_unit(e->type) || e->owner < 0 || e->state == ST_DYING || e->state == ST_ABOARD || azar(4)) continue;
			Ref r = ent_ref(i);
			float x = (float)azar(W.w * 10) / 10.0f, y = (float)azar(W.h * 10) / 10.0f;
			int q = azar(100);
			if (q < 35) cmd_move(r, x, y);
			else if (q < 60) {
				int j = azar(W.nents);
				if (W.ents[j].alive) cmd_target(r, ent_ref(j));
			} else if (q < 75 && !is_naval(e->type)) {
				for (int j = 0; j < W.nents; ++j) if (W.ents[j].alive && W.ents[j].type == E_TRANSPORT && W.ents[j].owner == e->owner && !azar(2)) { cmd_target(r, ent_ref(j)); break; }
			} else if (q < 85) cmd_move_queue(r, x, y);
			else if (q < 92) cmd_patrol(r, x, y);
			else cmd_set_stance(r, azar(4));
		}
	}
	int a_bordo = 0;
	for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].type == E_TRANSPORT) a_bordo += W.ents[i].ncargo;
	printf("azar recorrido barcos=%.0f tierra=%.0f a_bordo=%d\n", rec_barcos, rec_tierra, a_bordo);
	printf("terreno malas=%ld comprobaciones=%ld\n", terreno_malas, terreno_comprobaciones);
}

int main(int argc, char **argv) {
	if (argc < 4) { fprintf(stderr, "uso: aoen_ia SEMILLA DIF_A DIF_B [MAPA] [TAMAÑO] [MINUTOS] [JUGADORES]\n"); return 2; }
	int seed = atoi(argv[1]), da = atoi(argv[2]), db = atoi(argv[3]);
	int map = argc > 4 ? atoi(argv[4]) : 3, size = argc > 5 ? atoi(argv[5]) : 100;
	float maxt = (argc > 6 ? atof(argv[6]) : 60.0f) * 60.0f;
	int np = argc > 7 ? atoi(argv[7]) : 2;
	Setup s;
	memset(&s, 0, sizeof s);
	s.width = s.height = size; s.seed = seed; s.players = np; s.map_kind = map; s.popcap = 75; s.villagers = 4;
	s.victory = getenv("AOEN_IA_VICTORIA") ? atoi(getenv("AOEN_IA_VICTORIA")) : VIC_STANDARD;   /* 0 estándar, 1 conquista, 2 Maravilla, 3 reliquias */
	s.start_res[RES_WOOD] = 200; s.start_res[RES_FOOD] = 200; s.start_res[RES_GOLD] = 100;
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		int d = (i & 1) ? db : da;
		s.ai_diff[i] = d; s.civ[i] = (seed + i * 3) % NUM_CIVS; s.team[i] = getenv("AOEN_IA_FFA") ? i + 1 : (i & 1) + 1;
	}
	s.recursos_mapa = getenv("AOEN_IA_RECURSOS") ? atoi(getenv("AOEN_IA_RECURSOS")) : 0;   /* 0 normales, 1 escasos, 2 abundantes */
	s.victoria_compartida = getenv("AOEN_IA_COMPARTIDA") ? atoi(getenv("AOEN_IA_COMPARTIDA")) : 0;
	world_new(&s);
	hilos_iniciar();   /* D393: AOEN_HILOS=N reparte la IA y los caminos entre N hilos */
	/* AOEN_IA_REPARTO=1: solo genera el mapa y cuenta las casillas de oro y piedra (el reparto lo escribe game.c en el registro) */
	if (getenv("AOEN_IA_REPARTO")) {
		int oro = 0, piedra = 0;
		for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive) { oro += W.ents[i].type == E_GOLD; piedra += W.ents[i].type == E_STONE; }
		printf("mapa oro=%d piedra=%d\n", oro, piedra);
		return 0;
	}
	/* AOEN_IA_ALIADOS=1: los jugadores 0 y 1 empiezan aliados por la diplomacia (equipos distintos con AOEN_IA_FFA) */
	if (getenv("AOEN_IA_ALIADOS")) { W.pl[0].dipl[1] = W.pl[1].dipl[0] = DIPL_ALLY; }
	/* AOEN_IA_SIN_IA=1: nadie lo lleva la máquina (escenas de prueba con AOEN_ESCENA) */
	for (int p = 0; p < np; ++p) { W.pl[p].ai = !getenv("AOEN_IA_SIN_IA"); W.pl[p].diff = (p & 1) ? db : da; }
	/* AOEN_IA_RICO=1: todos empiezan en la Edad del Hierro con 3000 de cada recurso (para probar la Maravilla y el final) */
	if (getenv("AOEN_IA_RICO"))
		for (int p = 0; p < np; ++p) { W.pl[p].age = AGE_IRON; for (int k = 0; k < RES_COUNT; ++k) W.pl[p].res[k] = 3000; }
	/* AOEN_IA_PRUEBA_CAMBIO=1 (con la escena «comercio» en modo cambio): a los 30 s se cambia el modo de los mercantes a mitad
	 * de ruta (debe reiniciarla: carga 0, fase 0) y a los 40 s se derriba el muelle y el mercado ajenos (la ruta se pierde) */
	if (getenv("AOEN_IA_PRUEBA_CAMBIO")) {
		while (W.time < 30) world_update(1.0f / 30);
		for (int i = 0; i < W.nents; ++i) {
			Entity *e = &W.ents[i];
			if (!e->alive || !is_trader(e->type)) continue;
			int c0 = e->carry, f0 = e->com_fase;
			cmd_comercio(ent_ref(i), 0, e->com_dar, e->com_recibir);
			printf("cambio %s: antes carga %d fase %d, después carga %d fase %d\n", unit_def[e->type - E_VILLAGER].name, c0, f0, e->carry, e->com_fase);
		}
		for (int i = 0; i < W.nents; ++i) {
			Entity *e = &W.ents[i];
			if (e->alive && e->owner == 1 && (e->type == E_DOCK || e->type == E_MARKET)) e->alive = 0;   /* derribo brusco */
		}
		while (W.time < 40) world_update(1.0f / 30);
		for (int i = 0; i < W.nents; ++i) {
			Entity *e = &W.ents[i];
			if (e->alive && is_trader(e->type)) printf("ruta %s: destino %d fase %d carga %d\n", unit_def[e->type - E_VILLAGER].name, e->com_destino.idx, e->com_fase, e->carry);
		}
		return 0;
	}
	/* AOEN_IA_EDIFICIOS=1: qué entrena, qué investiga y qué recoge cada edificio (la tabla de docs/aoen.md) */
	if (getenv("AOEN_IA_EDIFICIOS")) {
		static const char *rn[] = { "madera", "comida", "oro", "piedra" };
		for (int b = E_TOWN_CENTER; b <= E_WONDER; ++b) {
			printf("edificio %s|entrena:", bld_def[b].name);
			for (int t = E_VILLAGER; t < E_HORSE; ++t) if (unit_def[t - E_VILLAGER].trainer == b) printf(" %s;", unit_def[t - E_VILLAGER].name);
			printf("|investiga:");
			for (int t = 0; t < T_COUNT; ++t) if (tech_def[t].bld == b) printf(" %s;", tech_def[t].name);
			printf("|recoge:");
			/* el muelle solo recoge lo que traen los barcos de pesca */
			if (b == E_DOCK) printf(" pescado;");
			else {
				for (int k = 0; k < RES_COUNT; ++k) if (edificio_recoge(b, k, 0)) printf(" %s;", rn[k]);
				if (edificio_recoge(b, RES_FOOD, 1)) printf(" carne;");
			}
			printf("\n");
		}
		return 0;
	}
	if (getenv("AOEN_IA_AZAR")) { prueba_azar((float)atof(getenv("AOEN_IA_AZAR"))); return 0; }
	int age_t[MAX_PLAYERS][AGE_COUNT] = { { 0 } }, peak_vill[MAX_PLAYERS] = { 0 }, peak_army[MAX_PLAYERS] = { 0 };
	int alive_t[MAX_PLAYERS];
	for (int p = 0; p < np; ++p) alive_t[p] = -1;
	float next = 0, sig_suma = 60;
	int suma = getenv("AOEN_IA_SUMA") != NULL;   /* D393: suma del mundo cada minuto (h398: igual con 1 y con N hilos) */
	while (!W.over && W.time < maxt) {
		world_update(1.0f / 30);
		comprobar_terreno();
		if (suma && W.time >= sig_suma) { printf("suma t=%.0f %08x\n", W.time, world_suma()); sig_suma += 60; }
		if (W.time >= next) {
			next += 5;
			for (int p = 0; p < np; ++p) {
				for (int a = 1; a < AGE_COUNT; ++a) if (!age_t[p][a] && W.pl[p].age >= a) age_t[p][a] = (int)W.time;
				int v = 0, m = 0;
				for (int i = 0; i < W.nents; ++i) {
					Entity *e = &W.ents[i];
					if (!e->alive || e->owner != p || e->state == ST_DYING || !is_unit(e->type)) continue;
					if (e->type == E_VILLAGER) ++v; else ++m;
				}
				peak_vill[p] = MAX(peak_vill[p], v); peak_army[p] = MAX(peak_army[p], m);
				if (!W.pl[p].alive && alive_t[p] < 0 && W.time > 5) alive_t[p] = (int)W.time;
			}
			if (getenv("AOEN_IA_TRAZA") && (int)W.time % 60 == 0) {
				for (int p = 0; p < np; ++p) {
					int v = 0, m = 0, b = 0;
					for (int i = 0; i < W.nents; ++i) { Entity *e = &W.ents[i]; if (!e->alive || e->owner != p || e->state == ST_DYING) continue; if (e->type == E_VILLAGER) ++v; else if (is_unit(e->type)) ++m; else if (is_building(e->type)) ++b; }
					fprintf(stderr, "t=%4d P%d edad%d res %d/%d/%d/%d pob %d/%d aldeanos %d ejército %d edificios %d bajas %d | %s\n", (int)W.time, p, W.pl[p].age, W.pl[p].res[0], W.pl[p].res[1], W.pl[p].res[2], W.pl[p].res[3], W.pl[p].pop, world_pop_cap(p), v, m, b, W.pl[p].units_killed, ia_estado(p));
				}
			}
			if (getenv("AOEN_IA_TRAZA") && atoi(getenv("AOEN_IA_TRAZA")) > 1 && (int)W.time % 300 == 0) {
				for (int p = 0; p < np; ++p) {
					int c[E_TYPE_COUNT] = { 0 }, o[E_TYPE_COUNT] = { 0 };
					for (int i = 0; i < W.nents; ++i) { Entity *e = &W.ents[i]; if (e->alive && e->owner == p && e->state != ST_DYING) { ++c[e->type]; if (is_building(e->type) && e->progress < 1) ++o[e->type]; } }
					fprintf(stderr, "   P%d:", p);
					for (int t = 0; t < E_TYPE_COUNT; ++t) if (c[t]) fprintf(stderr, " %s=%d%s", is_building(t) ? bld_def[t].name : unit_def[t - E_VILLAGER].name, c[t], o[t] ? "(obra)" : "");
					fputc('\n', stderr);
				}
			}
			/* el mundo solo declara el final cuando cae el jugador 0 o todos sus rivales: aquí basta con un equipo */
			int team_alive[MAX_PLAYERS + 2] = { 0 }, equipos = 0;
			for (int p = 0; p < np; ++p) if (W.pl[p].alive && !team_alive[W.pl[p].team]++) ++equipos;
			if (W.time > 10 && equipos <= 1) break;
		}
	}
	int team_alive[MAX_PLAYERS + 2] = { 0 }, team_score[MAX_PLAYERS + 2] = { 0 };
	for (int p = 0; p < np; ++p) {
		int ec, mi, te;
		world_score(p, &ec, &mi, &te);
		team_score[W.pl[p].team] += ec + mi + te;
		if (W.pl[p].alive) team_alive[W.pl[p].team] = 1;
		printf("jugador %d equipo %d dif %d civ %s edades %d/%d/%d aldeanos_max %d ejercito_max %d bajas %d edificios_arrasados %d recogido %d puntos %d vivo %d\n",
			p, W.pl[p].team, W.pl[p].diff, civ_def[W.pl[p].civ].name, age_t[p][1], age_t[p][2], age_t[p][3], peak_vill[p], peak_army[p],
			W.pl[p].units_killed, W.pl[p].bld_razed, W.pl[p].gathered, ec + mi + te, W.pl[p].alive);
	}
	if (getenv("AOEN_IA_TRAZA"))
		for (int i = 0; i < W.nents; ++i) {
			Entity *e = &W.ents[i];
			if (e->alive && e->owner >= 0 && e->state != ST_DYING && e->owner < np && W.pl[e->owner].hist_n >= 0 && !is_resource(e->type) && e->owner % 2 == (team_alive[1] ? 1 : 0))
				fprintf(stderr, "queda: P%d %s en %.1f,%.1f estado %d casilla %d\n", e->owner, is_building(e->type) ? bld_def[e->type].name : unit_def[e->type - E_VILLAGER].name, e->x, e->y, e->state, W.tile[(int)e->y * W.w + (int)e->x]);
		}
	int win = team_alive[1] && !team_alive[2] ? 1 : team_alive[2] && !team_alive[1] ? 2 : 0;
	/* final por Maravilla o reliquias: W.over es 1 si gana el equipo del jugador 0 */
	const char *como = "conquista";
	if (W.over && team_alive[1] && team_alive[2]) { win = W.over == 1 ? W.pl[0].team : 3 - W.pl[0].team; como = W.setup.victory == VIC_RELICS ? "reliquias" : "maravilla"; }
	if (!W.over && !win) como = "tiempo";
	if (getenv("AOEN_IA_FFA"))
		for (int p = 0; p < np; ++p) for (int q = p + 1; q < np; ++q) if (W.pl[p].dipl[q] != DIPL_ENEMY) printf("diplomacia %d-%d: %s\n", p, q, dipl_name(W.pl[p].dipl[q]));
	for (int p = 0; p < np; ++p) printf("recursos jugador %d: %d %d %d %d\n", p + 1, W.pl[p].res[0], W.pl[p].res[1], W.pl[p].res[2], W.pl[p].res[3]);
	printf("terreno malas=%ld comprobaciones=%ld\n", terreno_malas, terreno_comprobaciones);
	printf("fin over=%d ganadores=%#x alianza01=%s\n", W.over, W.ganadores, dipl_name(W.pl[0].dipl[1]));
	if (suma) printf("suma final t=%.0f %08x hilos=%d\n", W.time, world_suma(), hilos_n());
	hilos_parar();
	printf("resultado ganador=%d tiempo=%d ganador_puntos=%d como=%s\n", win, (int)W.time, team_score[1] >= team_score[2] ? 1 : 2, como);
	return 0;
}
