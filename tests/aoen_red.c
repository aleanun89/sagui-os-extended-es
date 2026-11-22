/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Arnés de h461 para el PC: un jugador de AoEN multijugador sin ventana (D461). Varias copias hablan entre sí por UDP en
 * 127.0.0.1 (con pérdida y retardo simulados si se pide); dan órdenes al azar y escriben la suma de sincronía de cada turno.
 *   aoen_red anf  PUERTO HUMANOS IA TURNOS [opciones]     el anfitrión: espera HUMANOS-1 clientes y añade IA plazas de la máquina
 *   aoen_red cli  PUERTO_ANFITRION MI_PUERTO TURNOS [opciones]
 * Opciones: semilla=N mapa=N tam=N ordenes=PORCENTAJE salir=TURNO pausa=TURNO vel=N@TURNO buscar=1 chat=1 fuego=1 */
#include "../userland/juegos/aoen/game.h"
#include "../userland/juegos/aoen/red.h"
#include "../userland/juegos/aoen/ordenes.h"

static const char *opcion(int argc, char **argv, const char *k) {
	size_t n = strlen(k);
	for (int i = 0; i < argc; ++i) if (!strncmp(argv[i], k, n) && argv[i][n] == '=') return argv[i] + n + 1;
	return NULL;
}
static int opc_int(int argc, char **argv, const char *k, int def) { const char *v = opcion(argc, argv, k); return v ? atoi(v) : def; }

static void esperar(float seg, int (*cond)(void)) {
	unsigned t0 = SDL_GetTicks();
	while (SDL_GetTicks() - t0 < (unsigned)(seg * 1000) && (!cond || !cond())) { red_tick(); SDL_Delay(2); }
}
static int en_sala(void) { return red_estado() == RED_SALA_CLI || red_estado() == RED_ERROR; }
static int en_juego(void) { return red_estado() == RED_JUEGO || red_estado() == RED_ERROR; }

int main(int argc, char **argv) {
	if (argc < 5) { fprintf(stderr, "uso: aoen_red anf PUERTO HUMANOS IA TURNOS [..] | aoen_red cli PUERTO_ANF MI_PUERTO TURNOS 0 [..]\n"); return 2; }
	int anf = !strcmp(argv[1], "anf");
	int turnos = atoi(argv[anf ? 5 : 5]);
	if (!anf) turnos = atoi(argv[4]);
	int semilla = opc_int(argc, argv, "semilla", 7), pct = opc_int(argc, argv, "ordenes", 40), salir = opc_int(argc, argv, "salir", -1);
	int pausa = opc_int(argc, argv, "pausa", -1), buscar = opc_int(argc, argv, "buscar", 0);
	char env[64];
	if (anf) { snprintf(env, sizeof env, "%d", atoi(argv[2])); setenv("AOEN_RED_PUERTO", env, 1); }
	else { snprintf(env, sizeof env, "%d", atoi(argv[3])); setenv("AOEN_RED_MIPUERTO", env, 1); snprintf(env, sizeof env, "%d", atoi(argv[2])); setenv("AOEN_RED_PUERTO", env, 1); }
	setenv("AOEN_RED_RAPIDO", getenv("AOEN_RED_RAPIDO") ? getenv("AOEN_RED_RAPIDO") : "1", 0);
	red_suma_cada = opc_int(argc, argv, "suma", 1);
	red_ticks_turno = opc_int(argc, argv, "ticks", 4);
	red_azar_semilla((unsigned)(semilla * 7919 + (anf ? 0 : atoi(argv[3]))));

	if (anf) {
		int humanos = atoi(argv[3]), ia = atoi(argv[4]);
		if (!red_crear_sala("Anfitrion")) { fprintf(stderr, "no hay sala: %s\n", red_error()); return 3; }
		memset(&red_cfg, 0, sizeof red_cfg);
		red_cfg.width = red_cfg.height = opc_int(argc, argv, "tam", 72); red_cfg.seed = semilla; red_cfg.map_kind = opc_int(argc, argv, "mapa", 2);
		red_cfg.villagers = 4; red_cfg.popcap = 100; red_cfg.victory = VIC_STANDARD; red_cfg.time_limit = 30; red_cfg.victoria_compartida = 1;
		red_cfg.fuego_amigo = opc_int(argc, argv, "fuego", 0);
		red_cfg.start_res[RES_WOOD] = 200; red_cfg.start_res[RES_FOOD] = 200; red_cfg.start_res[RES_GOLD] = 100;
		int plaza = 1;
		for (int k = 1; k < humanos; ++k) red_plaza[plaza++].tipo = PL_LIBRE;
		for (int k = 0; k < ia; ++k) { red_plaza[plaza].tipo = PL_IA; red_plaza[plaza].dif = k % 3; snprintf(red_plaza[plaza].nombre, 16, "IA%d", plaza + 1); ++plaza; }
		while (plaza < MAX_PLAYERS) red_plaza[plaza++].tipo = PL_CERRADA;
		red_plaza[0].listo = 1;
		red_sala_cambiada();
		if (opc_int(argc, argv, "chat", 0)) { /* el chat se prueba con un mensaje del anfitrión cuando entre alguien */ }
		unsigned t0 = SDL_GetTicks();
		for (;;) {
			red_tick();
			int h = 0;
			for (int p = 1; p < MAX_PLAYERS; ++p) h += red_plaza[p].tipo == PL_HUMANO && red_plaza[p].listo;
			if (h >= humanos - 1) break;
			if (SDL_GetTicks() - t0 > 30000) { fprintf(stderr, "anfitrión: nadie entró\n"); return 4; }
			SDL_Delay(5);
		}
		if (opc_int(argc, argv, "chat", 0)) { red_chat("hola desde el anfitrión"); esperar(0.5f, NULL); }
		if (!red_empezar()) { fprintf(stderr, "no se pudo empezar: %s\n", red_error()); return 5; }
	} else {
		RDir d;
		if (buscar) {
			char t[32]; snprintf(t, sizeof t, "127.0.0.1:%d", atoi(argv[2])); setenv("AOEN_RED_BUSCAR", t, 1);
			if (!red_iniciar()) return 3;
			RPartida p[4]; int n = 0;
			for (int k = 0; k < 40 && !n; ++k) { red_buscar(); esperar(0.1f, NULL); n = red_partidas(p, 4); }
			if (!n) { fprintf(stderr, "no encuentro partidas\n"); return 4; }
			printf("ENCONTRADA %s jugadores=%d humanos=%d mapa=%d tam=%d\n", p[0].nombre, p[0].jugadores, p[0].humanos, p[0].mapa, p[0].tam);
			d = p[0].dir;
		} else { char t[32]; snprintf(t, sizeof t, "127.0.0.1:%d", atoi(argv[2])); red_dir_texto(t, &d); }
		if (!red_iniciar() || !red_unirse(d, "Cliente")) return 3;
		esperar(10, en_sala);
		if (red_estado() != RED_SALA_CLI) { fprintf(stderr, "no entré: %s\n", red_error()); return 4; }
		printf("DENTRO plaza=%d\n", red_mi_plaza + 1);
		red_elegir(red_mi_plaza % NUM_CIVS, red_mi_plaza + 1, 0, 1);
		if (opc_int(argc, argv, "chat", 0)) { red_chat("hola desde un cliente"); }
	}
	red_turno_max = turnos;
	/* a jugar */
	esperar(120, en_juego);
	if (red_estado() != RED_JUEGO) { fprintf(stderr, "no empezó: %s (estado %d)\n", red_error(), red_estado()); return 6; }
	printf("JUEGO jugador=%d suma0=%08x\n", W.human + 1, world_suma());
	fflush(stdout);
	unsigned ult = SDL_GetTicks(); int turno_ant = -1;
	const char *vel = opcion(argc, argv, "vel");
	while (red_estado() == RED_JUEGO && red_turno() < turnos) {
		unsigned ahora = SDL_GetTicks();
		float dt = (ahora - ult) / 1000.0f; ult = ahora;
		red_tick();
		if (red_turno() != turno_ant) {
			turno_ant = red_turno();
			red_ordenes_azar(pct);
			if (salir >= 0 && turno_ant >= salir) { printf("SALGO turno=%d\n", turno_ant); fflush(stdout); red_salir_partida(); return 0; }
			static int p1, p2, v1;
			if (pausa >= 0 && !p1 && turno_ant >= pausa && W.human == 0) { p1 = 1; red_pedir_pausa(); }
			if (pausa >= 0 && p1 && !p2 && turno_ant >= pausa + 20 && W.human == 0) { p2 = 1; red_pedir_pausa(); }
			if (vel && !v1 && W.human == 0 && turno_ant >= atoi(strchr(vel, '@') + 1)) { v1 = 1; red_pedir_velocidad(atoi(vel)); }
		}
		red_avanzar(dt);
		SDL_Delay(1);
	}
	if (red_estado() == RED_NADA && W.nents) {   /* se perdió al anfitrión: la partida sigue sola contra la máquina */
		unsigned s0 = world_suma();
		for (int k = 0; k < 600 && !W.over; ++k) { red_ordenes_azar(pct); world_update(1.0f / 30); }
		printf("SOLO suma0=%08x tras 600 tics t=%.1f ia=%d\n", s0, W.time, W.pl[1].ai);
	}
	printf("PING ms=%d\n", red_ping_ms(1)); printf("FIN estado=%d turno=%d suma=%08x t=%.2f nents=%d desinc=%d tics=%d pausa=%d vel=%d chat=%d\n", red_estado(), red_turno(), world_suma(), W.time, W.nents, red_desincronizado(), 0, red_pausado(), red_velocidad(), red_chat_n());
	for (int p = 0; p < W.setup.players; ++p) printf("JUG %d ia=%d recursos=%d,%d,%d,%d pop=%d\n", p + 1, W.pl[p].ai, W.pl[p].res[0], W.pl[p].res[1], W.pl[p].res[2], W.pl[p].res[3], W.pl[p].pop);
	fflush(stdout);
	esperar(1.5f, NULL);   /* deja salir lo último hacia los demás */
	red_cerrar();
	return red_estado() == RED_ERROR ? 7 : 0;
}
