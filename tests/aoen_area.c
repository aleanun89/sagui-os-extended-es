/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN en el PC (D443): daño en área de la catapulta y recarga de la fe del sacerdote, con un mundo sintético pequeño.
 * Imprime «FALLO ...» por cada comprobación que no se cumple y «aoen_area OK» si todo va bien. */
#include "../userland/juegos/aoen/game.h"
#include "../userland/juegos/aoen/hilos.h"

static int fallos;
#define COMPROBAR(c, ...) do { if (!(c)) { ++fallos; printf("FALLO: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void mundo(int amigo) {
	Setup s;
	memset(&s, 0, sizeof s);
	s.fuego_amigo = amigo;   /* D447: la opción de la partida */
	s.width = s.height = 64; s.seed = 5; s.players = 2; s.map_kind = 3; s.popcap = 75; s.villagers = 4;
	s.start_res[RES_WOOD] = 200; s.start_res[RES_FOOD] = 200;
	for (int i = 0; i < MAX_PLAYERS; ++i) { s.civ[i] = i % NUM_CIVS; s.team[i] = i + 1; }
	world_new(&s);
	for (int p = 0; p < 2; ++p) { W.pl[p].ai = 0; W.pl[p].age = AGE_IRON; }
}

/* despeja un claro de hierba de 40x20 alrededor de (x, y) */
static void claro(float x, float y) {
	for (int i = 0; i < W.nents; ++i) { Entity *r = &W.ents[i]; if (r->alive && is_resource(r->type) && fabsf(r->bx - x) < 24 && fabsf(r->by - y) < 14) r->alive = 0; }
	for (int j = (int)y - 12; j <= (int)y + 12; ++j) for (int i = (int)x - 22; i <= (int)x + 22; ++i)
		if (i > 0 && j > 0 && i < W.w - 1 && j < W.h - 1) { W.tile[j * W.w + i] = TILE_GRASS; W.block[j * W.w + i] = 0; }
}

static int pasivo(EntType t, int dueno, float x, float y) {
	int i = dev_place(t, dueno, x, y);
	if (i >= 0) W.ents[i].stance = STANCE_PASSIVE;
	return i;
}

static void area(int amigo) {
	mundo(amigo);
	COMPROBAR(fuego_amigo == amigo && W.setup.fuego_amigo == amigo, "la opción de la partida no llegó al motor");
	float cx = 0, cy = 0;
	for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER && W.ents[i].owner == 0) { cx = W.ents[i].x; cy = W.ents[i].y; }
	claro(cx + 12, cy + 12);
	float x = cx + 4, y = cy + 12;
	int cat = dev_place(E_CATAPULT, 0, x, y);
	int a = pasivo(E_CLUBMAN, 1, x + 7.0f, y);           /* el objetivo */
	int b = pasivo(E_CLUBMAN, 1, x + 7.0f, y + 0.8f);    /* dentro del radio (1,3) */
	int c = pasivo(E_CLUBMAN, 1, x + 7.0f, y + 3.5f);    /* fuera del radio */
	int f = pasivo(E_CLUBMAN, 0, x + 7.0f, y - 0.8f);    /* propio, dentro del radio */
	COMPROBAR(cat >= 0 && a >= 0 && b >= 0 && c >= 0 && f >= 0, "no se pudieron colocar las unidades");
	float hp_b = W.ents[b].hp, hp_c = W.ents[c].hp, hp_f = W.ents[f].hp;
	cmd_target(ent_ref(cat), ent_ref(a));
	int disparos = 0;
	for (float t = 0; t < 30 && W.ents[a].hp >= W.ents[a].maxhp; t += 1.0f / 30) { world_update(1.0f / 30); disparos += W.nshots > 0; }
	COMPROBAR(W.ents[a].hp < W.ents[a].maxhp, "la catapulta no alcanzó al objetivo");
	COMPROBAR(W.ents[b].hp < hp_b, "amigo=%d: el enemigo dentro del radio no recibió daño en área", amigo);
	COMPROBAR(W.ents[c].hp == hp_c, "amigo=%d: el enemigo fuera del radio recibió daño (%.1f de %.1f)", amigo, W.ents[c].hp, hp_c);
	if (amigo) COMPROBAR(W.ents[f].hp < hp_f, "con fuego amigo el propio dentro del radio no recibió daño");
	else COMPROBAR(W.ents[f].hp == hp_f, "sin fuego amigo el propio recibió daño");
	COMPROBAR(disparos > 0, "no hubo piedra en vuelo");
	/* el proyectil es de tipo piedra y deja un impacto */
	for (float t = 0; t < 2 && W.nimpactos == 0; t += 1.0f / 30) world_update(1.0f / 30);
	COMPROBAR(W.nimpactos > 0, "la piedra no dejó impacto");
	world_free();
}

static void sacerdote(void) {
	fuego_amigo = 0;
	mundo(0);
	float cx = 0, cy = 0;
	for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER && W.ents[i].owner == 0) { cx = W.ents[i].x; cy = W.ents[i].y; }
	claro(cx + 12, cy + 12);
	float x = cx + 4, y = cy + 12;
	int pr = dev_place(E_PRIEST, 0, x, y);
	int e1 = pasivo(E_CLUBMAN, 1, x + 2.0f, y), e2 = pasivo(E_CLUBMAN, 1, x + 2.0f, y + 1.0f);
	COMPROBAR(pr >= 0 && e1 >= 0 && e2 >= 0, "no se pudieron colocar el sacerdote y los enemigos");
	W.pl[0].dipl[1] = W.pl[1].dipl[0] = DIPL_ENEMY;
	COMPROBAR(priest_fe(&W.ents[pr]) == 0, "el sacerdote empieza sin fe");
	float t = 0, t1 = -1, t2 = -1;
	cmd_target(ent_ref(pr), ent_ref(e1));
	for (; t < 120 && t2 < 0; t += 1.0f / 30) {
		world_update(1.0f / 30);
		if (getenv("AOEN_AREA_DEP") && ((int)(t * 30) % 150) == 0) printf("t=%.1f estado=%d timer=%.2f fe=%.2f e1.dueno=%d alive=%d dist=%.2f\n", t, W.ents[pr].state, W.ents[pr].timer, W.ents[pr].gacc, W.ents[e1].owner, W.ents[e1].alive, hypotf(W.ents[pr].x - W.ents[e1].x, W.ents[pr].y - W.ents[e1].y));
		/* una conversión fallida deja al sacerdote ocioso: se le vuelve a ordenar */
		if (W.ents[pr].state == ST_IDLE) cmd_target(ent_ref(pr), ent_ref(t1 < 0 ? e1 : e2));
		if (t1 < 0 && W.ents[e1].owner == 0) {
			t1 = t;
			COMPROBAR(priest_fe(&W.ents[pr]) > 9.0f && priest_fe(&W.ents[pr]) <= 10.0f, "tras convertir faltan %.2f s de fe (esperaba ~10)", priest_fe(&W.ents[pr]));
			cmd_target(ent_ref(pr), ent_ref(e2));
		}
		if (t1 >= 0 && t2 < 0 && W.ents[e2].owner == 0) t2 = t;
		/* mientras se recarga no puede haber convertido a la segunda */
		if (t1 >= 0 && t < t1 + 9.5f) COMPROBAR(W.ents[e2].owner == 1, "convirtió otra unidad con la fe sin recargar (t=%.1f, primera a %.1f)", t, t1);
		if (t1 >= 0 && t < t1 + 9.5f && W.ents[e2].owner != 1) break;
	}
	COMPROBAR(t1 >= 0, "no convirtió la primera unidad");
	COMPROBAR(t2 >= t1 + 9.5f, "la segunda conversión llegó antes de la recarga (primera %.1f, segunda %.1f)", t1, t2);
	COMPROBAR(t2 >= 0, "no volvió a convertir tras la recarga");
	printf("sacerdote: primera conversión a %.1f s, segunda a %.1f s\n", t1, t2);
	world_free();
}

/* D447: la opción se guarda con la partida; las de formato 5 (Setup sin fuego_amigo) se cargan con ella apagada */
typedef struct { char magic[8]; int version; long long stamp; int sizeof_setup, sizeof_player, sizeof_entity; int w, h, nents, capents; float time; int over, human; unsigned rng; } CabGuardado;   /* copia de SaveHeader */

static void guardado(void) {
	const char *ruta = "/tmp/aoen_area_d447.sav", *vieja = "/tmp/aoen_area_d447_v5.sav";
	mundo(1);
	COMPROBAR(world_save(ruta, 7), "no se pudo guardar la partida con fuego amigo");
	world_free();
	COMPROBAR(world_load(ruta) && W.setup.fuego_amigo == 1 && fuego_amigo == 1, "la partida guardada con fuego amigo no lo conserva");
	world_free();
	mundo(0);
	COMPROBAR(world_save(ruta, 8), "no se pudo guardar la partida sin fuego amigo");
	world_free();
	fuego_amigo = 1;
	COMPROBAR(world_load(ruta) && W.setup.fuego_amigo == 0 && fuego_amigo == 0, "la partida guardada sin fuego amigo lo trae activado");
	world_free();
	/* una partida de formato 5: misma cabecera con version 5 y sizeof_setup SETUP_TAM_V5, y el Setup sin el último campo */
	FILE *f = fopen(ruta, "rb");
	COMPROBAR(f != NULL, "no se pudo reabrir el guardado");
	if (f) {
		fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
		unsigned char *b = malloc(n);
		COMPROBAR(fread(b, 1, n, f) == (size_t)n, "lectura del guardado");
		fclose(f);
		long hdr = sizeof(CabGuardado);
		CabGuardado *c = (CabGuardado *)b;
		COMPROBAR(c->version == 6 && c->sizeof_setup == (int)sizeof(Setup), "el guardado nuevo debería ser de formato 6");
		c->version = 5; c->sizeof_setup = SETUP_TAM_V5;
		FILE *o = fopen(vieja, "wb");
		COMPROBAR((long)sizeof(Setup) > SETUP_TAM_V5, "Setup no creció");
		fwrite(b, 1, hdr + SETUP_TAM_V5, o);
		fwrite(b + hdr + sizeof(Setup), 1, n - hdr - sizeof(Setup), o);
		fclose(o); free(b);
		fuego_amigo = 1;
		SaveInfo si;
		COMPROBAR(save_info(vieja, &si) && si.stamp == 8, "save_info no lee la partida de formato 5");
		COMPROBAR(world_load(vieja) && W.setup.fuego_amigo == 0 && fuego_amigo == 0 && W.setup.players == 2, "la partida de formato 5 no carga con fuego amigo apagado");
		world_free();
		remove(vieja);
	}
	remove(ruta);
}

/* D447: el carro mercante lleva carga y banda dorada dibujadas con primitivas de relleno (las mismas por CPU y GPU) */
static int pixeles_distintos(int shade, int sc, int *oro) {
	enum { LW = 128, LH = 96 };
	static u32 px[LW * LH];
	for (int i = 0; i < LW * LH; ++i) px[i] = 0xff202020u;
	fb.px = px; fb.pitch = LW; gfx_set_size(LW, LH); gfx_clip_reset();
	gfx_us = 1;
	carro_mercante_carga(64, 70, sc, shade);
	int n = 0, ymin = LH, ymax = -1; *oro = 0;
	for (int y = 0; y < LH; ++y) for (int x = 0; x < LW; ++x) if (px[y * LW + x] != 0xff202020u) {
		++n; if (y < ymin) ymin = y; if (y > ymax) ymax = y;
		if (((px[y * LW + x] >> 16) & 255) > 220 && ((px[y * LW + x] >> 8) & 255) > 180 && (px[y * LW + x] & 255) < 100) ++*oro;
	}
	COMPROBAR(ymax < 70 + 2 && ymin > 70 - 40, "la carga se sale de la zona del carro (filas %d..%d)", ymin, ymax);
	return n;
}

static void carro(void) {
	int oro1, oro2, oro3;
	int a = pixeles_distintos(256, 256, &oro1), b = pixeles_distintos(256, 128, &oro2), c = pixeles_distintos(100, 256, &oro3);
	COMPROBAR(a > 300, "la carga del carro mercante dibuja pocos píxeles (%d)", a);
	COMPROBAR(oro1 > 20, "no se ve la banda dorada (%d píxeles de oro)", oro1);
	COMPROBAR(b > 40 && b < a, "a media escala la carga debería ser menor pero existir (%d frente a %d)", b, a);
	COMPROBAR(c > 300 && oro3 == 0, "a la sombra el oro no debería brillar como a plena luz (%d)", oro3);
	printf("carro mercante: %d píxeles a escala 1 (%d de oro), %d a media escala\n", a, oro1, b);
}

int main(void) {
	hilos_iniciar();
	area(0);
	area(1);
	sacerdote();
	guardado();
	carro();
	printf(fallos ? "aoen_area FALLA (%d)\n" : "aoen_area OK\n", fallos);
	return fallos ? 1 : 0;
}
