/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Arnés de h389 para el anfitrión: partidas de AoEN de IA contra IA, sin ventana y a toda velocidad.
 *   aoen_ia SEMILLA DIF_A DIF_B [MAPA] [TAMAÑO] [MINUTOS] [JUGADORES]
 * Los jugadores pares son del equipo 1 con dificultad DIF_A y los impares del equipo 2 con DIF_B
 * (0 fácil, 1 normal, 2 difícil, 3 muy difícil). Imprime una línea por jugador y al final
 *   resultado ganador=EQUIPO tiempo=SEGUNDOS
 * (ganador=0 si se acaba el tiempo: gana entonces quien tiene más puntuación, «ganador_puntos=»). */
#include "../userland/juegos/aoen/game.h"
#include "../userland/juegos/aoen/ia.h"

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
	int age_t[MAX_PLAYERS][AGE_COUNT] = { { 0 } }, peak_vill[MAX_PLAYERS] = { 0 }, peak_army[MAX_PLAYERS] = { 0 };
	int alive_t[MAX_PLAYERS];
	for (int p = 0; p < np; ++p) alive_t[p] = -1;
	float next = 0;
	while (!W.over && W.time < maxt) {
		world_update(1.0f / 30);
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
	printf("fin over=%d ganadores=%#x alianza01=%s\n", W.over, W.ganadores, dipl_name(W.pl[0].dipl[1]));
	printf("resultado ganador=%d tiempo=%d ganador_puntos=%d como=%s\n", win, (int)W.time, team_score[1] >= team_score[2] ? 1 : 2, como);
	return 0;
}
