/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Inteligencia artificial de los jugadores de la máquina (v8.5.4.7; antes vivía en game.c).
 *
 * Cada jugador de la máquina «piensa» cada cierto tiempo (más a menudo cuanto más difícil) y en cada vuelta:
 *   1. hace el censo de lo suyo (aldeanos por tarea, edificios, ejército) y actualiza lo que VE: cuadrícula de
 *      celdas exploradas y visibles y memoria de los edificios enemigos que ha visto (no hace trampas: solo sabe
 *      lo que han visto sus unidades, salvo en «muy difícil», que lo dice en el menú);
 *   2. economía: aldeanos repartidos según lo que necesita (edad siguiente, edificios, ejército), casas antes de
 *      llenar la población, almacenes y graneros junto a los recursos, granjas cuando se acaba la comida silvestre,
 *      caza, mercado para cambiar sobrantes;
 *   3. edades: construye los edificios que pide la siguiente edad y ahorra para ella;
 *   4. ejército: unidades elegidas según su fuerza y lo que ha visto del rival (contraunidades), defensa de la base,
 *      ataques en grupo (se reúnen antes de salir, avanzan juntos, se retiran si pierden), sacerdotes que curan,
 *      exploración inicial y con exploradores, torres.
 *
 * Las tablas (edificios, unidades, tecnologías) se recorren por su tamaño, sin números fijos: lo que añada el otro
 * trabajo (asedio, barcos, murallas) entra solo si existe; lo que necesita un tipo concreto se busca por su papel
 * (p. ej. asedio = mucho daño a edificios y alcance) y se comprueba antes de usarlo (ver NOTAS-IA.md). */
#include "game.h"
#include "ia.h"
#include "ordenes.h"
#include "hilos.h"

/* ---------- copias locales de ayudas internas de game.c (allí son static) ---------- */

static int res_kind_of(EntType t) {
	switch (t) {
	case E_RELIC: return RES_COUNT;
	case E_BERRIES: case E_FARM: case E_CARCASS: case E_FISH: return RES_FOOD;
	case E_GOLD: return RES_GOLD;
	case E_STONE: return RES_STONE;
	default: return RES_WOOD;
	}
}

static int islas(void) { return W.setup.map_kind == MAP_ISLANDS; }   /* mapa de islas (puede que haya tierra entre rivales) */
static int hostile(int a, int b) {
	return a >= 0 && b >= 0 && a != b && W.pl[a].dipl[b] == DIPL_ENEMY;
}

static float d2(float ax, float ay, float bx, float by) { return (ax - bx) * (ax - bx) + (ay - by) * (ay - by); }
static int vivo(const Entity *e) { return e->alive && e->state != ST_DYING; }
static int militar(int t) { return is_unit(t) && t != E_VILLAGER && t != E_PRIEST && t != E_HORSE && t != E_ELEPHANT && !is_naval((EntType)t) && !is_trader((EntType)t); }
static const UnitDef *ud(int t) { return &unit_def[t - E_VILLAGER]; }

/* ---------- niveles de dificultad ---------- */

typedef struct {
	float pensar;          /* segundos entre vueltas */
	int aldeanos, aldeanos_edad, aldeanos_max;
	int obras;             /* construcciones a la vez */
	int edad_aldeanos[AGE_COUNT]; /* aldeanos para empezar a investigar cada edad */
	float edad_min[AGE_COUNT];    /* minutos mínimos de partida para cada edad (la fácil va despacio) */
	int tecnologias;       /* 0 ninguna, 1 economía (y militares desde el Bronce), 2 todas */
	int ataque_min;        /* unidades militares del primer ataque */
	int ataque_crece;      /* unidades más en cada oleada */
	float ataque_t;        /* minutos antes del primer ataque */
	int retirada;          /* se retira si pierde y retira a los heridos */
	int contraunidades;    /* elige unidades según lo que ha visto, y contraataca */
	int explora;           /* exploración inicial y exploradores */
	int torres;            /* torres que construye */
	int caza, mercado, sacerdotes, huida;
	int trampa;            /* muy difícil: ve un 20 % más lejos y recoge más (+0,22 por segundo y recolector, alrededor de un 40 %) */
} Nivel;

static const Nivel niveles[IA_DIFICULTADES] = {
	/* fácil: pocos aldeanos, edades lentas, sin mejoras, ataques tardíos y sin retirada */
	{ 2.0f, 10, 2, 18, 1, { 0, 10, 14, 18 }, { 0, 7, 16, 26 }, 0, 6, 2, 15.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* normal */
	{ 1.0f, 14, 5, 34, 2, { 0, 12, 18, 24 }, { 0, 0, 0, 0 }, 1, 8, 3, 10.0f, 1, 1, 1, 1, 0, 1, 1, 0, 0 },
	/* difícil */
	{ 0.6f, 18, 7, 50, 3, { 0, 12, 20, 26 }, { 0, 0, 0, 0 }, 2, 10, 4, 8.0f, 1, 1, 1, 3, 0, 1, 2, 1, 0 },
	/* muy difícil: la difícil con ventajas declaradas (visión +20 %, recogida +0,22 por segundo y recolector) */
	{ 0.6f, 18, 7, 50, 3, { 0, 12, 20, 26 }, { 0, 0, 0, 0 }, 2, 10, 4, 8.0f, 1, 1, 1, 3, 0, 1, 2, 1, 1 },
};

/* ---------- memoria de cada jugador de la máquina (no se guarda: se rehace al cargar) ---------- */

#define CELDA 4                 /* lado de una celda de la cuadrícula de exploración, en casillas */
#define MAXC (512 / CELDA)
#define MAX_CONOCIDOS 160

typedef struct { Ref r; int t, owner; float x, y; } Conocido;

enum { M_PAZ, M_REUNIR, M_ATAQUE, M_RETIRADA };

typedef struct {
	int iniciado;
	unsigned gen;                 /* generación del mundo en que se empezó (world_gen) */
	float ultimo_t;
	int cw, ch;
	u8 explorada[MAXC * MAXC];
	u8 casa[MAXC * MAXC];        /* celdas de la base propia (para detectar amenazas) */
	unsigned short vista[MAXC * MAXC]; /* vuelta en la que se vio por última vez */
	unsigned short vuelta;
	Conocido con[MAX_CONOCIDOS];
	int ncon;
	/* lo visto del ejército rival: infantería, caballería, a distancia */
	float visto_inf, visto_cab, visto_dist;
	float fuerza_rival;           /* la mayor fuerza militar rival vista hace poco (se va olvidando) */
	/* ejército */
	int modo, oleada;
	float modo_t, reunion_x, reunion_y, obj_x, obj_y, fuerza_inicial;
	Ref objetivo;
	Ref explorador, explorador_ini;
	float explora_t;
	float embarque_t;
	float diplo_t;
	float colonia_x, colonia_y, colonia_t;   /* isla a la que se llevan aldeanos (sin madera u oro en la propia) */
	int colonia;                /* última vez que pensó en la diplomacia */             /* desde cuándo hay transportes cargando en casa */
	int ruta;                     /* paso de la vuelta de exploración inicial */
	float amenaza_x, amenaza_y; int amenaza;
	float defendido_t;            /* cuándo se rechazó el último ataque */
	float primer_ataque_t;
	int ataques_rechazados;
	char ultimo[80];              /* lo último que decidió (para la depuración) */
	/* islas (D415): isla y agua de la base, sitio de desembarco y rescate de varados */
	int base_isla, base_mar;
	int anf_ok; float anf_wx, anf_wy, anf_lx, anf_ly, anf_ox, anf_oy, anf_t;   /* agua y tierra donde desembarcar junto al objetivo */
	int resc_fase;                /* 0 nada, 1 el transporte va a por un varado, 2 vuelve a casa con él */
	Ref resc_tr, resc_u, resc_ign; float resc_t, resc_ign_t, resc_wx, resc_wy;
	unsigned alc_gen; int alc_isla[12], alc_mar[12], alc_ok[12], alc_n;   /* islas ya comprobadas: ¿hay orilla alcanzable por mar? */
} Ia;

static Ia ia[MAX_PLAYERS];
/* aldeanos que van a cada entidad (índice), se rehace en cada vuelta; uno por jugador (D393: piensan a la vez) */
static int *ocupa[MAX_PLAYERS];
static int ocupa_cap[MAX_PLAYERS];

/* ---------- obras aplazadas (D393) ----------
 * Mientras las IA piensan a la vez (cada una en un hilo), nadie crea entidades ni cambia el mapa: las obras se apuntan (con
 * el coste ya apartado de los recursos del jugador, el sitio y el aldeano reservados para las siguientes decisiones de esa
 * misma vuelta) y se ponen después en el hilo principal, jugador por jugador y en orden (ia_aplicar). Con 1 hilo se hace
 * exactamente lo mismo, así que la partida sale igual con 1 o con N hilos. */
#define MAX_OBRAS 32
typedef struct { int obrero, ayudante, t, tx, ty, coste[RES_COUNT]; } Obra;
static struct { Obra v[MAX_OBRAS]; int n; } obras[MAX_PLAYERS];

/* ¿choca el edificio t en (tx,ty) con una obra ya apuntada por el jugador? (con una casilla de margen) */
static int choca_obra(int p, int t, int tx, int ty) {
	int s = bld_def[t].size;
	for (int k = 0; k < obras[p].n; ++k) {
		const Obra *o = &obras[p].v[k];
		int os = bld_def[o->t].size;
		if (tx - 1 < o->tx + os && o->tx < tx + s + 1 && ty - 1 < o->ty + os && o->ty < ty + s + 1) return 1;
	}
	return 0;
}

static int obrero_reservado(int p, int i) {
	for (int k = 0; k < obras[p].n; ++k) if (obras[p].v[k].obrero == i || obras[p].v[k].ayudante == i) return 1;
	return 0;
}
static const Ref SIN = { -1, 0 };

static void ia_reiniciar(int p) {
	Ia *a = &ia[p];
	memset(a, 0, sizeof *a);
	a->iniciado = 1;
	a->gen = world_gen();
	a->cw = (W.w + CELDA - 1) / CELDA; a->ch = (W.h + CELDA - 1) / CELDA;
	a->objetivo = a->explorador = a->explorador_ini = SIN;
	a->resc_tr = a->resc_u = a->resc_ign = SIN;
	a->primer_ataque_t = -1;
}

/* ---------- condiciones de victoria ---------- */

static int vic_maravilla(void) { return W.setup.victory == VIC_STANDARD || W.setup.victory == VIC_WONDER; }
static int vic_reliquias(void) { return W.setup.victory == VIC_RELICS && W.setup.relics_total > 0; }

/* una Maravilla rival terminada o muy avanzada (la partida avisa a todos de las terminadas): hay que ir a por ella */
static int maravilla_rival(int p, float *x, float *y, Ref *r);

/* sacerdotes que quiere: más si la partida se gana con reliquias */
static int sacerdotes_quiero(int p);
static int quiere_maravilla(int p);

/* ---------- visión y memoria ---------- */

static int celda(float x, float y, const Ia *a) {
	int cx = CLAMP((int)x / CELDA, 0, a->cw - 1), cy = CLAMP((int)y / CELDA, 0, a->ch - 1);
	return cy * a->cw + cx;
}

static int ve(int p, float x, float y) {
	Ia *a = &ia[p];
	return a->vista[celda(x, y, a)] == a->vuelta;
}

static void marcar(Ia *a, u8 *m, unsigned short *v, float x, float y, int r) {
	int cx = (int)x / CELDA, cy = (int)y / CELDA, rc = (r + CELDA - 1) / CELDA;
	for (int j = cy - rc; j <= cy + rc; ++j) {
		if (j < 0 || j >= a->ch) continue;
		for (int i = cx - rc; i <= cx + rc; ++i) {
			if (i < 0 || i >= a->cw) continue;
			if (m) m[j * a->cw + i] = 1;
			if (v) v[j * a->cw + i] = a->vuelta;
		}
	}
}

static void recordar(Ia *a, int i) {
	Entity *e = &W.ents[i];
	for (int k = 0; k < a->ncon; ++k) if (a->con[k].r.idx == i && a->con[k].r.gen == e->gen) { a->con[k].x = e->x; a->con[k].y = e->y; return; }
	if (a->ncon >= MAX_CONOCIDOS) return;
	a->con[a->ncon++] = (Conocido){ ent_ref(i), e->type, e->owner, e->x, e->y };
}

static float fuerza(const Entity *e);

static void ia_ver(int p) {
	Ia *a = &ia[p];
	if (++a->vuelta == 0) { memset(a->vista, 0, sizeof a->vista); a->vuelta = 1; }
	memset(a->casa, 0, sizeof a->casa);
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!vivo(e) || e->owner < 0 || !same_team(e->owner, p)) continue;
		int r = e->type == E_TOWN_CENTER ? 10 : is_building(e->type) ? 7 : 6;
		/* muy difícil: todo lo suyo ve un 20 % más lejos (decisión del mantenedor; no lo ve todo) */
		if (niveles[W.pl[p].diff].trampa && e->owner == p) r = (r * 6 + 4) / 5;
		marcar(a, a->explorada, a->vista, e->x, e->y, r);
		if (e->owner == p && is_building(e->type)) marcar(a, a->casa, NULL, e->x, e->y, 10);
	}
	/* lo que se ve del rival: edificios a la memoria y composición del ejército */
	float inf = 0, cab = 0, dist = 0, fv = 0;
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!vivo(e) || !hostile(p, e->owner) || !ve(p, e->x, e->y)) continue;
		if (is_building(e->type)) recordar(a, i);
		else if (militar(e->type)) {
			fv += fuerza(e);
			if (ud(e->type)->ranged) dist += 1;
			else if (ud(e->type)->speed >= 2.2f) cab += 1;
			else inf += 1;
		}
	}
	/* las Maravillas terminadas se anuncian a todos: se conocen aunque no se vean */
	if (vic_maravilla())
		for (int i = 0; i < W.nents; ++i) {
			Entity *e = &W.ents[i];
			if (vivo(e) && e->type == E_WONDER && e->progress >= 1 && hostile(p, e->owner)) recordar(a, i);
		}
	/* se olvida lo que ya no está (destruido) */
	for (int k = 0; k < a->ncon; ) {
		Entity *e = ent_get(a->con[k].r);
		if (!e || e->state == ST_DYING || !hostile(p, e->owner)) a->con[k] = a->con[--a->ncon];
		else ++k;
	}
	/* memoria del ejército visto, que se va olvidando poco a poco */
	a->fuerza_rival = MAX(a->fuerza_rival * 0.995f, fv);
	a->visto_inf *= 0.98f; a->visto_cab *= 0.98f; a->visto_dist *= 0.98f;
	a->visto_inf = MAX(a->visto_inf, inf); a->visto_cab = MAX(a->visto_cab, cab); a->visto_dist = MAX(a->visto_dist, dist);
}

/* ---------- censo ---------- */

typedef struct {
	int n[E_TYPE_COUNT], hechos[E_TYPE_COUNT];
	int obras, aldeanos, ejercito, sacerdotes, constructores;
	int tarea[RES_COUNT];   /* aldeanos que recogen cada recurso */
	Entity *tc;
	float bx, by;           /* centro de la base */
} Censo;

static void censar(int p, Censo *c) {
	memset(c, 0, sizeof *c);
	float sx = 0, sy = 0; int nb = 0;
	if (ocupa_cap[p] < W.nents) { free(ocupa[p]); ocupa_cap[p] = W.nents + 256; ocupa[p] = malloc(sizeof(int) * (size_t)ocupa_cap[p]); if (!ocupa[p]) ocupa_cap[p] = 0; }
	if (ocupa[p]) memset(ocupa[p], 0, sizeof(int) * (size_t)ocupa_cap[p]);
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!vivo(e) || e->owner != p) continue;
		++c->n[e->type];
		if (is_building(e->type)) {
			if (e->progress >= 1) ++c->hechos[e->type]; else ++c->obras;
			if (e->type == E_TOWN_CENTER && e->progress >= 1 && !c->tc) c->tc = e;
			sx += e->x; sy += e->y; ++nb;
		} else if (e->type == E_VILLAGER) {
			++c->aldeanos;
			if ((e->state == ST_GATHER || e->state == ST_RETURN) && e->carry_kind < RES_COUNT) ++c->tarea[e->carry_kind];
			if (e->state == ST_BUILD) ++c->constructores;
			if (ocupa[p] && e->target.idx >= 0 && e->target.idx < W.nents && (e->state == ST_GATHER || e->state == ST_MOVE)) ++ocupa[p][e->target.idx];
		} else if (e->type == E_PRIEST) ++c->sacerdotes;
		else if (militar(e->type)) ++c->ejercito;
	}
	if (c->tc) { c->bx = c->tc->x; c->by = c->tc->y; }
	else if (nb) { c->bx = sx / nb; c->by = sy / nb; }
}

/* ---------- construcción ---------- */

static int ia_find_site(int p, int t, float ax, float ay, int rmin, int rmax) {
	int s = bld_def[t].size;
	for (int r = rmin; r <= rmax; ++r) {
		int pasos = MAX(8, 8 * r);
		for (int k = 0; k < pasos; ++k) {
			float an = (float)k / (float)pasos * 6.2831853f + (float)p;
			int tx = (int)(ax + dcosf(an) * r) - s / 2, ty = (int)(ay + dsinf(an) * r) - s / 2;
			if (!can_place((EntType)t, tx, ty) || choca_obra(p, t, tx, ty)) continue;
			/* mantén un anillo libre para que las unidades puedan rodear */
			int ok = 1;
			for (int j = ty - 1; j <= ty + s && ok && t != E_DOCK && t != E_WALL; ++j)
				for (int i = tx - 1; i <= tx + s; ++i)
					if (!tile_walk(i, j)) { ok = 0; break; }
			if (ok) return ty * W.w + tx;
		}
	}
	return -1;
}

static void coste(int t, int c[RES_COUNT]) {
	if (is_building(t)) { c[RES_WOOD] = bld_def[t].wood; c[RES_FOOD] = bld_def[t].food; c[RES_GOLD] = bld_def[t].gold; c[RES_STONE] = bld_def[t].stone; }
	else { c[RES_WOOD] = ud(t)->wood; c[RES_FOOD] = ud(t)->food; c[RES_GOLD] = ud(t)->gold; c[RES_STONE] = ud(t)->stone; }
}

static int puede_pagar(int p, int t, int reserva_comida, int reserva_oro) {
	const Player *pl = &W.pl[p];
	int c[RES_COUNT];
	coste(t, c);
	/* lo apartado para la edad siguiente solo cuenta en los recursos que la cosa gasta */
	return pl->res[RES_WOOD] >= c[RES_WOOD] && pl->res[RES_FOOD] >= c[RES_FOOD] + (c[RES_FOOD] ? reserva_comida : 0) &&
	       pl->res[RES_GOLD] >= c[RES_GOLD] + (c[RES_GOLD] ? reserva_oro : 0) && pl->res[RES_STONE] >= c[RES_STONE];
}

/* el aldeano más cercano a un punto que no esté construyendo ni lleve mucha carga */
static int constructor(int p, float x, float y) {
	int best = -1; float bd = 1e9f;
	for (int i = 0; i < W.nents; ++i) {
		Entity *u = &W.ents[i];
		if (!vivo(u) || u->owner != p || u->type != E_VILLAGER || u->state == ST_BUILD || u->carry > 6) continue;
		if (ia[p].explorador_ini.idx == i || obrero_reservado(p, i)) continue;
		float d = d2(u->x, u->y, x, y);
		if (u->state == ST_IDLE) d *= 0.5f;
		if (d < bd) { bd = d; best = i; }
	}
	return best;
}

/* apunta la obra (D393): aparta el coste, reserva el sitio y el aldeano y, en la difícil, un segundo aldeano que ayudará en
 * los edificios grandes. La pone ia_aplicar. */
static int encargar_obra(int p, int b, int t, int tx, int ty) {
	if (obras[p].n >= MAX_OBRAS || !puede_pagar(p, t, 0, 0) || !can_place((EntType)t, tx, ty) || choca_obra(p, t, tx, ty)) return 0;
	Obra *o = &obras[p].v[obras[p].n++];
	o->obrero = b; o->ayudante = -1; o->t = t; o->tx = tx; o->ty = ty;
	coste(t, o->coste);
	for (int k = 0; k < RES_COUNT; ++k) W.pl[p].res[k] -= o->coste[k];
	if (W.pl[p].diff >= 2 && bld_def[t].size >= 3) {
		int b2 = constructor(p, tx, ty);
		if (b2 >= 0 && b2 != b) o->ayudante = b2;
	}
	return 1;
}

/* pone las obras apuntadas por p (hilo principal, en orden): devuelve el coste y deja que cmd_build lo vuelva a cobrar */
static void poner_obras(int p) {
	for (int k = 0; k < obras[p].n; ++k) {
		const Obra *o = &obras[p].v[k];
		for (int r = 0; r < RES_COUNT; ++r) W.pl[p].res[r] += o->coste[r];
		if (!W.ents[o->obrero].alive || !cmd_build(p, ent_ref(o->obrero), (EntType)o->t, o->tx, o->ty)) continue;
		if (o->ayudante >= 0 && W.ents[o->ayudante].alive)
			for (int j = W.nents - 1; j >= 0; --j)
				if (W.ents[j].alive && (int)W.ents[j].type == o->t && W.ents[j].owner == p && W.ents[j].bx == o->tx && W.ents[j].by == o->ty) { cmd_target(ent_ref(o->ayudante), ent_ref(j)); break; }
	}
	obras[p].n = 0;
}

static int construir(int p, int t, float ax, float ay, int rmin, int rmax) {
	if (!is_building(t) || W.pl[p].age < bld_def[t].age || !puede_pagar(p, t, 0, 0)) return 0;
	int site = ia_find_site(p, t, ax, ay, rmin, rmax);
	if (site < 0) return 0;
	int tx = site % W.w, ty = site / W.w;
	int b = constructor(p, tx, ty);
	if (b < 0) return 0;
	if (!encargar_obra(p, b, t, tx, ty)) return 0;
	snprintf(ia[p].ultimo, sizeof ia[p].ultimo, "construye %s", bld_def[t].name);
	return 1;
}

/* ---------- islas: componentes de tierra ---------- */

/* Cada casilla de tierra lleva el número de su isla (0 = agua), calculado una vez por mapa por relleno: así un aldeano no
 * va a por un árbol de otra isla, al que no puede llegar. Se rehace si cambia el mapa. */
static unsigned short *comp, *marc, *terr;   /* terr: zonas por las que se anda al empezar la partida (con los bosques como barrera) */

static unsigned terr_gen;   /* cambia cada vez que se rehacen las zonas: invalida lo recordado por zona */   /* isla de cada casilla de tierra y «mar» (masa de agua) de cada casilla de agua */
static int comp_w, comp_h;
static unsigned comp_gen;   /* generación del mundo (world_gen): el puntero W.tile se reutiliza entre partidas */

/* etiqueta con 1, 2, 3... las masas conexas (4 vecinos) de tierra (modo 0), de agua (1) o de tierra por la que se puede ir
 * andando (2: sin edificios ni recursos, como tile_walk) */
static int es_modo(int i, int modo) {
	if (modo == 2) return W.tile[i] < TILE_WATER && !W.block[i];
	return (W.tile[i] >= TILE_WATER) == modo;
}
static void etiquetar(unsigned short *dst, int agua, int *cola) {
	unsigned short n = 0;
	for (int i = 0; i < W.w * W.h; ++i) {
		if (dst[i] || !es_modo(i, agua)) continue;
		if (n < 65535) ++n;
		int ini = 0, fin = 0;
		cola[fin++] = i; dst[i] = n;
		while (ini < fin) {
			int k = cola[ini++], x = k % W.w, y = k / W.w;
			static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
			for (int d = 0; d < 4; ++d) {
				int nx = x + dx[d], ny = y + dy[d];
				if (nx < 0 || ny < 0 || nx >= W.w || ny >= W.h) continue;
				int j = ny * W.w + nx;
				if (!dst[j] && es_modo(j, agua)) { dst[j] = n; cola[fin++] = j; }
			}
		}
	}
}

static void componentes(void) {
	if (comp && comp_w == W.w && comp_h == W.h && comp_gen == world_gen()) return;
	free(comp); free(marc); free(terr);
	comp_w = W.w; comp_h = W.h; comp_gen = world_gen();
	comp = (unsigned short *)calloc((size_t)W.w * W.h, sizeof *comp);
	marc = (unsigned short *)calloc((size_t)W.w * W.h, sizeof *marc);
	terr = (unsigned short *)calloc((size_t)W.w * W.h, sizeof *terr);
	int *cola = (int *)malloc(sizeof(int) * (size_t)W.w * W.h);
	if (!comp || !marc || !terr || !cola) { free(cola); free(comp); free(marc); free(terr); comp = marc = terr = NULL; return; }
	etiquetar(comp, 0, cola);
	etiquetar(marc, 1, cola);
	etiquetar(terr, 2, cola); ++terr_gen;
	free(cola);
}

/* isla de un punto (0 si es agua o no se sabe); mira también las casillas vecinas por si el punto cae en la orilla */
static int isla_de(float x, float y) {
	if (!comp) return 0;
	int cx = CLAMP((int)x, 0, W.w - 1), cy = CLAMP((int)y, 0, W.h - 1);
	for (int r = 0; r <= 1; ++r)
		for (int j = cy - r; j <= cy + r; ++j)
			for (int i = cx - r; i <= cx + r; ++i)
				if (i >= 0 && j >= 0 && i < W.w && j < W.h && comp[j * W.w + i]) return comp[j * W.w + i];
	return 0;
}

/* zona por la que se anda de un punto (0 si no se sabe); mira las casillas vecinas (radio 2) por si el punto está sobre un obstáculo */
static int zona_de(float x, float y) {
	if (!terr) return 0;
	int cx = CLAMP((int)x, 0, W.w - 1), cy = CLAMP((int)y, 0, W.h - 1);
	for (int r = 0; r <= 2; ++r)
		for (int j = cy - r; j <= cy + r; ++j)
			for (int i = cx - r; i <= cx + r; ++i)
				if (i >= 0 && j >= 0 && i < W.w && j < W.h && terr[j * W.w + i]) return terr[j * W.w + i];
	return 0;
}

/* masa de agua de un punto (0 si no se sabe); busca el agua más cercana en un radio de 3 casillas */
static int mar_de(float x, float y) {
	if (!marc) return 0;
	int cx = CLAMP((int)x, 0, W.w - 1), cy = CLAMP((int)y, 0, W.h - 1);
	for (int r = 0; r <= 3; ++r)
		for (int j = cy - r; j <= cy + r; ++j)
			for (int i = cx - r; i <= cx + r; ++i)
				if (i >= 0 && j >= 0 && i < W.w && j < W.h && marc[j * W.w + i]) return marc[j * W.w + i];
	return 0;
}

/* D415: orilla de la isla `isla` alcanzable desde la masa de agua `mar`: una casilla de tierra libre (lx,ly) con una de agua
 * libre pegada (wx,wy); la más cercana a (ox,oy) dentro de `radio` casillas (0 = todo el mapa). Las torres del rival que
 * ya se conocen alejan el sitio. Devuelve 0 si no hay orilla (la isla no se alcanza con barcos desde esa agua). */
static int sitio_orilla(const Ia *a, int p, int isla, int mar, float ox, float oy, float radio, float *wx, float *wy, float *lx, float *ly) {
	if (!terr || !marc || !isla || !mar) return 0;
	int x0 = 0, x1 = W.w - 1, y0 = 0, y1 = W.h - 1;
	if (radio > 0) { x0 = MAX(0, (int)(ox - radio)); x1 = MIN(W.w - 1, (int)(ox + radio)); y0 = MAX(0, (int)(oy - radio)); y1 = MIN(W.h - 1, (int)(oy + radio)); }
	static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
	float mejor = 1e18f; int hay = 0;
	for (int y = y0; y <= y1; ++y)
		for (int x = x0; x <= x1; ++x) {
			if (terr[y * W.w + x] != isla) continue;
			for (int d = 0; d < 4; ++d) {
				int nx = x + dx[d], ny = y + dy[d];
				if (nx < 0 || ny < 0 || nx >= W.w || ny >= W.h || marc[ny * W.w + nx] != mar || !tile_sail(nx, ny)) continue;
				float v = d2(x + 0.5f, y + 0.5f, ox, oy);
				for (int k = 0; k < a->ncon; ++k)
					if (a->con[k].t == E_TOWER && hostile(p, a->con[k].owner) && d2(a->con[k].x, a->con[k].y, x + 0.5f, y + 0.5f) < 8 * 8) { v += 900; break; }
				if (v < mejor) { mejor = v; hay = 1; *wx = nx + 0.5f; *wy = ny + 0.5f; *lx = x + 0.5f; *ly = y + 0.5f; }
			}
		}
	return hay;
}

/* D415: ¿hay rivales a los que solo se llega por mar? Con la base en una isla y algún edificio rival conocido en otra (o
 * ninguno conocido aún). El generador de «islas» a veces deja a todos en la misma masa de tierra. */
static int hay_rivales_por_mar(int p) {
	Ia *a = &ia[p];
	if (!islas()) return 0;
	int base = a->base_isla, conocidos = 0;
	for (int k = 0; k < a->ncon; ++k) {
		if (!hostile(p, a->con[k].owner) || a->con[k].t == E_WALL) continue;
		++conocidos;
		if (zona_de(a->con[k].x, a->con[k].y) != base) return 1;
	}
	return !conocidos;
}

/* ¿se llega por mar a la isla desde el agua de la base? (se recuerda por isla) */
static int isla_alcanzable(Ia *a, int p, int isla) {
	if (!isla || !a->base_mar || isla == a->base_isla) return 1;
	if (a->alc_gen != terr_gen) { a->alc_gen = terr_gen; a->alc_n = 0; }
	for (int k = 0; k < a->alc_n; ++k) if (a->alc_isla[k] == isla && a->alc_mar[k] == a->base_mar) return a->alc_ok[k];
	float wx, wy, lx, ly;
	int ok = sitio_orilla(a, p, isla, a->base_mar, 0, 0, 0, &wx, &wy, &lx, &ly);
	if (a->alc_n < 12) { a->alc_isla[a->alc_n] = isla; a->alc_mar[a->alc_n] = a->base_mar; a->alc_ok[a->alc_n++] = ok; }
	return ok;
}

/* recurso conocido más cercano a un punto de un tipo, en la misma isla y no demasiado lleno de aldeanos */
static int recurso_cercano(int p, int kind, float x, float y, float maxd) {
	int best = -1; float bd = maxd * maxd;
	int isla = isla_de(x, y);
	for (int j = 0; j < W.nents; ++j) {
		Entity *r = &W.ents[j];
		if (!r->alive) continue;
		int farm = r->type == E_FARM && r->owner == p && r->progress >= 1;
		if (!(is_resource(r->type) || farm) || r->type == E_RELIC || res_kind_of(r->type) != kind || r->amount <= 0) continue;
		int lim = farm ? 2 : r->type == E_TREE ? 2 : 4;
		if (ocupa[p] && j < ocupa_cap[p] && ocupa[p][j] >= lim) continue;
		if (!farm && !ia[p].explorada[celda(r->x, r->y, &ia[p])]) continue; /* solo lo que conoce */
		if (r->type == E_FISH) continue;   /* los peces, para los barcos de pesca */
		if (isla && isla_de(r->x, r->y) != isla) continue;
		float d = d2(r->x, r->y, x, y);
		if (farm) d *= 0.4f;
		if (d < bd) { bd = d; best = j; }
	}
	return best;
}

/* punto de entrega más cercano de un recurso */
static Entity *entrega(int p, int kind, float x, float y, float *dist) {
	Entity *best = NULL; float bd = 1e9f;
	for (int i = 0; i < W.nents; ++i) {
		Entity *b = &W.ents[i];
		if (!vivo(b) || b->owner != p || b->progress < 1) continue;
		int ok = b->type == E_TOWN_CENTER || (b->type == E_GRANARY && kind == RES_FOOD) || (b->type == E_STORAGE && kind != RES_FOOD);
		if (!ok) continue;
		float d = d2(b->x, b->y, x, y);
		if (d < bd) { bd = d; best = b; }
	}
	if (dist) *dist = sqrtf(bd);
	return best;
}

static int enviar_recolector(int p, int ui, int kind, const Censo *c) {
	Entity *u = &W.ents[ui];
	Entity *dp = entrega(p, kind, u->x, u->y, NULL);
	float ox = dp ? dp->x : c->bx, oy = dp ? dp->y : c->by;
	if (isla_de(ox, oy) != isla_de(u->x, u->y)) { ox = u->x; oy = u->y; }   /* el aldeano está en otra isla (colonia) */
	int r = recurso_cercano(p, kind, ox, oy, 40);
	if (r < 0) r = recurso_cercano(p, kind, u->x, u->y, 70);
	if (r < 0) return 0;
	cmd_target(ent_ref(ui), ent_ref(r));
	if (ocupa[p] && r < ocupa_cap[p]) ++ocupa[p][r];
	return 1;
}

/* ---------- economía ---------- */

static int celda_sin_explorar(int p, float x, float y, int anillo);

static void repartir(int p, Censo *c, const int reserva[RES_COUNT]) {
	const Player *pl = &W.pl[p];
	const Nivel *nv = &niveles[pl->diff];
	int age = pl->age;
	/* reparto deseado: madera, comida, oro, piedra */
	float want[RES_COUNT] = { 0.40f, 0.45f, 0.0f, 0.0f };
	if (age >= AGE_TOOL) { want[RES_GOLD] = 0.15f + 0.05f * age; want[RES_FOOD] = 0.40f; want[RES_WOOD] = 0.35f; }
	if (age >= AGE_TOOL && nv->torres) want[RES_STONE] = 0.06f;
	/* lo que hace falta ahorrar (edad siguiente, edificios) pesa más; lo que sobra, menos */
	for (int k = 0; k < RES_COUNT; ++k) {
		if (reserva[k] > pl->res[k]) want[k] += 0.25f * MIN(1.0f, (reserva[k] - pl->res[k]) / 400.0f);
		if (pl->res[k] > 1200) want[k] *= 0.3f;
		else if (pl->res[k] > 700) want[k] *= 0.7f;
	}
	if (pl->res[RES_WOOD] < 120) want[RES_WOOD] += 0.15f;
	float tot = 0;
	for (int k = 0; k < RES_COUNT; ++k) tot += want[k];
	for (int k = 0; k < RES_COUNT; ++k) want[k] /= tot;

	int total = c->tarea[0] + c->tarea[1] + c->tarea[2] + c->tarea[3];
	/* mueve trabajadores del recurso con más exceso al que más falta */
	for (int vez = 0; vez < (nv->tecnologias ? 2 : 1) && total > 5; ++vez) {
		int worst = -1, best = -1; float wd = 0.12f, bd = 0.10f;
		for (int k = 0; k < RES_COUNT; ++k) {
			float have = (float)c->tarea[k] / total;
			if (have - want[k] > wd) { wd = have - want[k]; worst = k; }
			if (want[k] - have > bd) { bd = want[k] - have; best = k; }
		}
		if (worst < 0 || best < 0) break;
		int movido = 0;
		for (int i = 0; i < W.nents && !movido; ++i) {
			Entity *u = &W.ents[i];
			if (!vivo(u) || u->owner != p || u->type != E_VILLAGER || u->state != ST_GATHER || u->carry_kind != worst || u->carry > 3) continue;
			if (enviar_recolector(p, i, best, c)) { --c->tarea[worst]; ++c->tarea[best]; movido = 1; }
		}
		if (!movido) break;
	}

	/* los ociosos van al recurso que más falta */
	for (int i = 0; i < W.nents; ++i) {
		Entity *u = &W.ents[i];
		if (!vivo(u) || u->owner != p || u->type != E_VILLAGER || u->state != ST_IDLE) continue;
		if (ia[p].explorador_ini.idx == i && ent_get(ia[p].explorador_ini)) continue;
		/* si lleva carga, primero la entrega */
		if (u->carry > 0 && u->carry_kind < RES_COUNT) {
			Entity *dp = entrega(p, u->carry_kind, u->x, u->y, NULL);
			if (dp) { cmd_target(ent_ref(i), ent_ref((int)(dp - W.ents))); continue; }
		}
		int order[RES_COUNT];
		for (int k = 0; k < RES_COUNT; ++k) order[k] = k;
		for (int a = 0; a < RES_COUNT; ++a)
			for (int b = a + 1; b < RES_COUNT; ++b) {
				float da = want[order[a]] - (float)c->tarea[order[a]] / (float)MAX(1, total + 1);
				float db = want[order[b]] - (float)c->tarea[order[b]] / (float)MAX(1, total + 1);
				if (db > da) { int t = order[a]; order[a] = order[b]; order[b] = t; }
			}
		int ok = 0;
		for (int k = 0; k < RES_COUNT && !ok; ++k) {
			if (want[order[k]] <= 0.01f) continue;
			if (enviar_recolector(p, i, order[k], c)) { ++c->tarea[order[k]]; ++total; ok = 1; }
		}
		/* no conoce nada que recoger de lo que hace falta: sale a explorar */
		if (!ok || (c->tarea[RES_FOOD] == 0 && want[RES_FOOD] > 0.3f && recurso_cercano(p, RES_FOOD, c->bx, c->by, 200) < 0 && !c->hechos[E_FARM])) {
			int cel = celda_sin_explorar(p, u->x, u->y, 0);
			if (cel >= 0) cmd_move(ent_ref(i), (cel % ia[p].cw) * CELDA + CELDA * 0.5f, (cel / ia[p].cw) * CELDA + CELDA * 0.5f);
		}
	}
}

/* caza: unos pocos aldeanos van a por los caballos cercanos (la carne la recogen ellos mismos) */
static void cazar(int p, Censo *c) {
	/* la caza solo como recurso cuando se acaba la comida silvestre conocida y aún no hay granjas */
	int sin_comida = recurso_cercano(p, RES_FOOD, c->bx, c->by, 40) < 0 && !c->hechos[E_FARM];
	if (!(niveles[W.pl[p].diff].caza || sin_comida) || c->aldeanos < 4 || W.pl[p].age >= AGE_BRONZE) return;
	int cazando = 0;
	for (int i = 0; i < W.nents; ++i) {
		Entity *u = &W.ents[i];
		if (!vivo(u) || u->owner != p || u->type != E_VILLAGER) continue;
		Entity *t = ent_get(u->target);
		if (t && (t->type == E_HORSE || t->type == E_CARCASS)) ++cazando;
	}
	if (cazando >= 3) return;
	int presa = -1; float bd = 30.0f * 30.0f;
	for (int j = 0; j < W.nents; ++j) {
		Entity *a = &W.ents[j];
		if (!vivo(a) || a->type != E_HORSE || !ve(p, a->x, a->y)) continue;
		float d = d2(a->x, a->y, c->bx, c->by);
		if (d < bd) { bd = d; presa = j; }
	}
	if (presa < 0) return;
	for (int i = 0; i < W.nents && cazando < 3; ++i) {
		Entity *u = &W.ents[i];
		if (!vivo(u) || u->owner != p || u->type != E_VILLAGER || u->state == ST_BUILD) continue;
		if (!sin_comida && (u->carry_kind != RES_FOOD || u->state != ST_GATHER)) continue;
		Entity *t = ent_get(u->target);
		if (t && (t->type == E_FARM || t->type == E_HORSE || t->type == E_CARCASS)) continue;
		cmd_target(ent_ref(i), ent_ref(presa));
		++cazando;
	}
}

static int comida_silvestre(int p, const Censo *c) {
	int n = 0;
	for (int j = 0; j < W.nents; ++j) {
		Entity *r = &W.ents[j];
		if (!r->alive || (r->type != E_BERRIES && r->type != E_CARCASS) || r->amount <= 0) continue;
		if (d2(r->x, r->y, c->bx, c->by) < 24 * 24 && ia[p].explorada[celda(r->x, r->y, &ia[p])]) n += (int)r->amount;
	}
	return n;
}

/* sitio para un almacén o un granero: junto al recurso que se recoge más lejos de su punto de entrega */
static int construir_entrega(int p, Censo *c, int t) {
	int kind_food = t == E_GRANARY;
	float bx = 0, by = 0, peor = 0;
	for (int i = 0; i < W.nents; ++i) {
		Entity *u = &W.ents[i];
		if (!vivo(u) || u->owner != p || u->type != E_VILLAGER || u->state != ST_GATHER) continue;
		if ((u->carry_kind == RES_FOOD) != kind_food || u->carry_kind >= RES_COUNT) continue;
		Entity *r = ent_get(u->target);
		if (!r || r->type == E_FARM) continue;
		float d;
		entrega(p, u->carry_kind, r->x, r->y, &d);
		if (d > peor) { peor = d; bx = r->x; by = r->y; }
	}
	if (peor < 9.0f) {
		/* nadie lejos: si aún no hay ninguno, junto al recurso de ese tipo más cercano */
		if (c->n[t]) return 0;
		int r = recurso_cercano(p, kind_food ? RES_FOOD : RES_WOOD, c->bx, c->by, 30);
		if (r < 0) return 0;
		bx = W.ents[r].x; by = W.ents[r].y;
	}
	return construir(p, t, bx, by, 2, 6);
}

/* mapa de islas: no se llega por tierra al rival (se necesitan transportes) */

static int peces_cerca(int p, const Censo *c) {
	int n = 0;
	for (int j = 0; j < W.nents; ++j) {
		Entity *r = &W.ents[j];
		if (r->alive && r->type == E_FISH && r->amount > 0 && d2(r->x, r->y, c->bx, c->by) < 30 * 30 && ia[p].explorada[celda(r->x, r->y, &ia[p])]) ++n;
	}
	return n;
}

static int productores(const Censo *c) {
	int n = c->hechos[E_TOWN_CENTER];
	for (int t = 0; t < E_TYPE_COUNT; ++t) if (is_building(t) && t != E_TOWN_CENTER && ent_is_trainer((EntType)t)) n += c->hechos[t];
	return n;
}

static void edificios(int p, Censo *c) {
	Player *pl = &W.pl[p];
	const Nivel *nv = &niveles[pl->diff];
	int age = pl->age, cap = world_pop_cap(p), pop = pl->pop;
	int maxcap = W.setup.popcap > 0 ? W.setup.popcap : 200;
	if (c->obras >= nv->obras || !c->tc) return;
	int hechas = 0;
#define INTENTA(cond, t, ax, ay, r0, r1) do { if (c->obras + hechas < nv->obras && (cond) && construir(p, t, ax, ay, r0, r1)) { ++hechas; ++c->n[t]; } } while (0)

	/* casas: antes de llenarse, con más margen cuanto más se produce */
	int margen = pl->diff >= 2 ? 3 + 2 * productores(c) : pl->diff == 1 ? 3 + productores(c) : 2;
	int casas_obra = c->n[E_HOUSE] - c->hechos[E_HOUSE];
	if (cap < maxcap && pop + margen >= cap && casas_obra < (pl->diff >= 2 ? 2 : 1)) INTENTA(1, E_HOUSE, c->bx, c->by, 4, 14);

	/* almacén para los aldeanos que están en otra isla (colonia) */
	{
		int isla_base = isla_de(c->bx, c->by);
		for (int j = 0; j < W.nents && isla_base; ++j) {
			Entity *u = &W.ents[j];
			if (!vivo(u) || u->owner != p || u->type != E_VILLAGER || u->state == ST_BOARD || u->state == ST_ABOARD) continue;
			int isla = isla_de(u->x, u->y);
			if (!isla || isla == isla_base) continue;
			int hay = 0;
			for (int k = 0; k < W.nents && !hay; ++k) {
				Entity *b = &W.ents[k];
				if (vivo(b) && b->owner == p && (b->type == E_STORAGE || b->type == E_TOWN_CENTER) && isla_de(b->x, b->y) == isla) hay = 1;
			}
			if (!hay && pl->res[RES_WOOD] >= bld_def[E_STORAGE].wood && construir(p, E_STORAGE, u->x, u->y, 2, 8)) { ++hechas; ++c->n[E_STORAGE]; }
			break;
		}
	}
	/* puntos de entrega junto a los recursos */
	if (c->aldeanos >= 5 && c->n[E_STORAGE] < 4 && pl->res[RES_WOOD] >= bld_def[E_STORAGE].wood && c->obras + hechas < nv->obras)
		if (construir_entrega(p, c, E_STORAGE)) { ++hechas; ++c->n[E_STORAGE]; }
	if (c->aldeanos >= 7 && c->n[E_GRANARY] < 2 && pl->res[RES_WOOD] >= bld_def[E_GRANARY].wood && c->obras + hechas < nv->obras && comida_silvestre(p, c) > 300)
		if (construir_entrega(p, c, E_GRANARY)) { ++hechas; ++c->n[E_GRANARY]; }

	/* granjas cuando se acaba la comida silvestre */
	if (age >= bld_def[E_FARM].age && comida_silvestre(p, c) < 150 * MAX(2, c->tarea[RES_FOOD])) {
		int quiero = (c->aldeanos * 2 / 5 + 1) / 2 + 1;
		Entity *g = NULL;
		for (int i = 0; i < W.nents && !g; ++i) if (vivo(&W.ents[i]) && W.ents[i].owner == p && W.ents[i].type == E_GRANARY && W.ents[i].progress >= 1) g = &W.ents[i];
		int junto_granero = g && (c->n[E_FARM] & 1);
		INTENTA(c->n[E_FARM] < quiero, E_FARM, junto_granero ? g->x : c->bx, junto_granero ? g->y : c->by, 3, 12);
	}

	/* el cuartel pronto, para defenderse */
	if (c->aldeanos >= (pl->diff == 0 ? 9 : 8)) INTENTA(!c->n[E_BARRACKS], E_BARRACKS, c->bx, c->by, 5, 14);
	/* cada edificio de producción militar disponible (también los que añadan versiones nuevas) */
	for (int t = 0; t < E_TYPE_COUNT; ++t) {
		if (!is_building(t) || t == E_TOWN_CENTER || t == E_DOCK || !ent_is_trainer((EntType)t) || c->n[t] || age < bld_def[t].age) continue;
		int sirve = 0;
		for (int u = 0; u < E_TYPE_COUNT; ++u) if (is_unit(u) && (int)ud(u)->trainer == t && ud(u)->age <= age && u != E_VILLAGER && !is_naval((EntType)u) && !is_trader((EntType)u)) sirve = 1;
		if (!sirve || c->aldeanos < 10 + 2 * age) continue;
		if (t == E_TEMPLE && !sacerdotes_quiero(p) && age_requirements_met(p, MIN(age + 1, AGE_COUNT - 1))) continue;
		INTENTA(1, t, c->bx, c->by, 6, 16);
	}
	/* edificios sin producción que cuentan para la edad (centro de gobierno) */
	if (age + 1 < AGE_COUNT && !age_requirements_met(p, age + 1) && c->aldeanos >= nv->edad_aldeanos[age + 1] - 4) {
		for (int t = 0; t < E_TYPE_COUNT; ++t) {
			if (!is_building(t) || c->n[t] || age < bld_def[t].age || t == E_WONDER || t == E_TOWER || t == E_MARKET || t == E_TOWN_CENTER || t == E_WALL || t == E_DOCK) continue;
			INTENTA(1, t, c->bx, c->by, 6, 16);
		}
	}
	/* más producción en la difícil cuando sobra */
	if (pl->diff >= 2 && age >= AGE_BRONZE && c->aldeanos >= 30 && pl->res[RES_WOOD] > 400) {
		static const int extra[] = { E_BARRACKS, E_ARCHERY, E_STABLE };
		for (int k = 0; k < ARRAY_LEN(extra); ++k) INTENTA(c->n[extra[k]] < 2, extra[k], c->bx, c->by, 7, 18);
	}
	/* torres: junto al centro urbano, mirando hacia el centro del mapa */
	if (nv->torres && age >= bld_def[E_TOWER].age && c->n[E_TOWER] < nv->torres && c->aldeanos >= 14) {
		float dx = W.w * 0.5f - c->bx, dy = W.h * 0.5f - c->by, l = sqrtf(dx * dx + dy * dy) + 0.01f;
		float an = c->n[E_TOWER] * 1.2f;
		float ex = dx / l * dcosf(an) - dy / l * dsinf(an), ey = dx / l * dsinf(an) + dy / l * dcosf(an);
		INTENTA(1, E_TOWER, c->bx + ex * 6, c->by + ey * 6, 0, 5);
	}
	/* muelle en los mapas de agua (y si hay peces cerca, también en los demás para la difícil) */
	if (age >= bld_def[E_DOCK].age && c->aldeanos >= 10 && !c->n[E_DOCK] && (islas() || (pl->diff >= 2 && peces_cerca(p, c) >= 4)))
		INTENTA(1, E_DOCK, c->bx, c->by, 4, 26);
	/* murallas (difícil): tramos sueltos delante de la base, mirando al centro del mapa, siempre con huecos
	 * porque la búsqueda de caminos no abre brechas */
	if (pl->diff >= 2 && age >= MAX(bld_def[E_WALL].age, AGE_TOOL) && c->aldeanos >= 20 && c->n[E_WALL] < 14 && pl->res[RES_STONE] >= 60 && !islas()) {
		float dx = W.w * 0.5f - c->bx, dy = W.h * 0.5f - c->by, l = sqrtf(dx * dx + dy * dy) + 0.01f;
		dx /= l; dy /= l;
		int k = c->n[E_WALL];
		/* dos tramos de 7 a cada lado del eje; la casilla central del eje queda libre */
		float lado = (k % 2 ? 1.0f : -1.0f) * (2 + k / 2);
		int wx = (int)(c->bx + dx * 13 - dy * lado), wy = (int)(c->by + dy * 13 + dx * lado);
		if (c->obras + hechas < nv->obras && can_place(E_WALL, wx, wy) && !choca_obra(p, E_WALL, wx, wy)) {
			int b = constructor(p, (float)wx, (float)wy);
			if (b >= 0 && encargar_obra(p, b, E_WALL, wx, wy)) { ++hechas; ++c->n[E_WALL]; }
		}
	}
	/* mercado en la difícil, para cambiar lo que sobra */
	/* (antes si falta oro: en las islas se acaba pronto y el mercado lo saca de la comida) */
	int sin_oro = c->tarea[RES_GOLD] == 0 && pl->res[RES_GOLD] < 200;
	if (nv->mercado && age >= bld_def[E_MARKET].age && c->aldeanos >= (islas() || sin_oro ? 14 : 24)) INTENTA(!c->n[E_MARKET], E_MARKET, c->bx, c->by, 7, 16);
	/* maravilla cuando sobra de todo en la Edad del Hierro (difícil) */
	if (quiere_maravilla(p) && !c->n[E_WONDER]) {
		/* sin contar con las obras en curso: la Maravilla no espera */
		if (construir(p, E_WONDER, c->bx, c->by, 8, 22)) { ++hechas; ++c->n[E_WONDER]; }
	}
	/* muchos aldeanos a la obra de la Maravilla */
	for (int i = 0; i < W.nents; ++i) {
		Entity *w = &W.ents[i];
		if (!vivo(w) || w->owner != p || w->type != E_WONDER || w->progress >= 1) continue;
		int quiero = pl->diff >= 2 ? 10 : 6, ya = 0;
		for (int j = 0; j < W.nents; ++j) if (vivo(&W.ents[j]) && W.ents[j].owner == p && W.ents[j].type == E_VILLAGER && W.ents[j].target.idx == i && W.ents[j].state == ST_BUILD) ++ya;
		for (; ya < quiero; ++ya) { int b = constructor(p, w->x, w->y); if (b < 0) break; cmd_target(ent_ref(b), ent_ref(i)); }
		break;
	}
#undef INTENTA
}

/* mercado: vende lo que sobra y compra lo que falta */
static void comerciar(int p, const int reserva[RES_COUNT]) {
	Player *pl = &W.pl[p];
	if (!niveles[pl->diff].mercado) return;
	for (int k = 0; k < RES_COUNT; ++k) {
		if (k == RES_GOLD) continue;
		int escaso = 0;
		for (int j = 0; j < RES_COUNT; ++j) if (j != k && j != RES_STONE && pl->res[j] < 150) escaso = 1;
		if (pl->res[k] > 1100 || (escaso && pl->res[k] > 500)) cmd_trade(p, k, RES_GOLD);
		else if ((pl->res[k] < reserva[k] || pl->res[k] < 200) && pl->res[RES_GOLD] > 300 + reserva[RES_GOLD]) cmd_trade(p, RES_GOLD, k);
	}
}

static void investigar(int p, const Censo *c, int reserva_comida, int reserva_oro) {
	Player *pl = &W.pl[p];
	const Nivel *nv = &niveles[pl->diff];
	if (!nv->tecnologias || c->aldeanos < 12) return;
	for (int i = 0; i < W.nents; ++i) {
		Entity *b = &W.ents[i];
		if (!vivo(b) || b->owner != p || b->progress < 1 || b->tech_cur || !is_building(b->type)) continue;
		for (int t = 0; t < T_COUNT; ++t) {
			const TechDef *td = &tech_def[t];
			if (td->bld != b->type || has_tech(p, t) || td->age > pl->age) continue;
			if (td->prereq >= 0 && !has_tech(p, td->prereq)) continue;
			if (!tech_allowed(p, t)) continue;
			/* las de recolección (no todo lo del Almacén y el Granero: allí están también armas, armaduras y torres) */
			int economia = t == T_TOOLWORKING || t == T_BRONZE_AXE || t == T_MINING || t == T_DOMESTICATION || t == T_PLOW;
			if (nv->tecnologias < 2 && !economia && pl->age < AGE_BRONZE) continue;
			/* lo económico va primero y con menos colchón */
			int colchon = economia ? 50 : 150;
			if (pl->res[RES_FOOD] < td->food + reserva_comida / 2 + colchon || pl->res[RES_GOLD] < td->gold + reserva_oro / 2 ||
			    pl->res[RES_WOOD] < td->wood + 50 || pl->res[RES_STONE] < td->stone) continue;
			if (cmd_research(p, ent_ref(i), t)) break;
		}
	}
}

static int islas(void);

/* ---------- ejército ---------- */

static float fuerza_tipo(int p, int t) {
	const UnitDef *u = ud(t);
	float atk = eff_attack(p, (EntType)t, 0), hp = (float)u->hp * (1 + 0.12f * (eff_armor(p, (EntType)t, 0) + eff_armor(p, (EntType)t, 1)));
	return sqrtf(MAX(1.0f, atk) * hp) * (u->ranged ? 1.15f : 1.0f);
}

static float fuerza(const Entity *e) {
	if (!militar(e->type) || e->owner < 0) return 0;
	return fuerza_tipo(e->owner, e->type) * (e->hp / MAX(1.0f, e->maxhp));
}

/* ¿parece asedio? mucho daño a edificios y alcance: se reconoce por sus números, no por su nombre */
static int es_asedio(int t) { return is_siege((EntType)t); }

/* puntuación de una unidad para entrenarla ahora: fuerza por coste, y contraunidades según lo visto */
static float valor_unidad(int p, int t) {
	const UnitDef *u = ud(t);
	const Ia *a = &ia[p];
	float cst = u->wood + u->food + u->gold * 1.3f + u->stone + 1;
	float v = fuerza_tipo(p, t) * 100 / cst;
	if (niveles[W.pl[p].diff].contraunidades) {
		float tot = a->visto_inf + a->visto_cab + a->visto_dist + 0.01f;
		float fi = a->visto_inf / tot, fc = a->visto_cab / tot, fd = a->visto_dist / tot;
		int cab = !u->ranged && u->speed >= 2.2f, armado = u->armor + u->parmor >= 4;
		if (u->ranged) v *= 1.0f + 0.6f * fi - 0.4f * fc;           /* arqueros contra infantería; sufren con caballería */
		if (cab) v *= 1.0f + 0.7f * fd + 0.2f * fi;                  /* caballería contra arqueros */
		if (armado) v *= 1.0f + 0.6f * fc + 0.4f * fd;               /* hoplitas contra caballería y flechas */
	}
	/* lo que cuesta oro, peor si el oro escasea */
	if (u->gold && W.pl[p].res[RES_GOLD] < 150) v *= 0.6f;
	return v;
}

static void producir(int p, Censo *c, int reserva_comida, int reserva_oro) {
	Player *pl = &W.pl[p];
	int cap = world_pop_cap(p);
	if (pl->pop >= cap || c->aldeanos < (pl->diff == 0 ? 10 : 8)) return;
	int tope = pl->diff == 0 ? 8 + 6 * pl->age : 12 + 12 * pl->age;
	/* en las islas, el ejército de tierra se limita a lo que caben en cuatro viajes: el resto de la población, para la flota */
	if (hay_rivales_por_mar(p)) {
		tope = MIN(tope, 20);
		/* D415: y se dejan plazas libres para los transportes que faltan (con la población llena no se puede construir ninguno
		 * y el ejército se quedaba en tierra para siempre) */
		int falta = MAX(0, 2 + pl->age - c->n[E_TRANSPORT]);
		if (c->n[E_DOCK] && falta && pl->pop + MIN(6, 2 * falta) >= cap && c->ejercito >= 8) return;
	}
	if (c->ejercito >= tope && (!ia[p].amenaza || (hay_rivales_por_mar(p) && c->ejercito >= 30))) return;
	/* un ejército mínimo según la edad se mantiene aunque se esté ahorrando para la edad siguiente */
	int minimo = pl->diff == 0 ? 0 : pl->diff == 1 ? 4 + 4 * pl->age : 6 + 5 * pl->age;
	if (c->ejercito < minimo || ia[p].amenaza) { reserva_comida = 0; reserva_oro = 0; }
	int asedios = 0;
	for (int u = 0; u < E_TYPE_COUNT; ++u) if (is_unit(u) && u != E_VILLAGER && es_asedio(u)) asedios += c->n[u];
	int sac = sacerdotes_quiero(p);
	for (int i = 0; i < W.nents; ++i) {
		Entity *b = &W.ents[i];
		if (!vivo(b) || b->owner != p || b->progress < 1 || !is_building(b->type) || b->type == E_TOWN_CENTER || !ent_is_trainer(b->type)) continue;
		if (b->ntrain >= (pl->diff >= 2 ? 2 : 1)) continue;
		/* la mejor unidad de este edificio */
		int mejor = -1; float mv = 0;
		for (int u = 0; u < E_TYPE_COUNT; ++u) {
			if (!is_unit(u) || u == E_VILLAGER || ud(u)->trainer != b->type || ud(u)->age > pl->age || is_naval((EntType)u) || is_trader((EntType)u)) continue;
			if (unit_tech_req((EntType)u) >= 0 && !has_tech(p, unit_tech_req((EntType)u))) continue;
			if (u == E_PRIEST) {
				if (sac && c->sacerdotes < sac && puede_pagar(p, u, reserva_comida, reserva_oro)) { mejor = u; mv = 1e9f; }
				continue;
			}
			if (ia[p].explorador.idx >= 0 && ent_get(ia[p].explorador) == NULL && ud(u)->speed >= 2.5f && !ud(u)->gold) { /* el explorador lo pide explorar() */ }
			/* asedio: pocos, solo para derribar edificios (difícil y normal) */
			if (es_asedio(u) && (pl->diff == 0 || asedios >= 1 + pl->age)) continue;
			if (!puede_pagar(p, u, reserva_comida, reserva_oro)) continue;
			float v = valor_unidad(p, u);
			if (es_asedio(u)) v = 1e8f;
			if (pl->diff == 0) v = 1000.0f - ud(u)->food - ud(u)->gold - ud(u)->wood;   /* la fácil coge lo más barato */
			if (v > mv) { mv = v; mejor = u; }
		}
		if (mejor == E_PRIEST && c->sacerdotes >= sac) continue;
		if (mejor >= 0 && cmd_train(p, ent_ref(i), (EntType)mejor)) {
			if (mejor == E_PRIEST) ++c->sacerdotes;
			else if (es_asedio(mejor)) ++asedios;
		}
	}
}

/* exploración: celda sin explorar más cercana (o, para buscar al rival, en el anillo donde empiezan los jugadores) */
static int celda_sin_explorar(int p, float x, float y, int anillo) {
	Ia *a = &ia[p];
	int best = -1; float bd = 1e18f;
	float cx = W.w * 0.5f, cy = W.h * 0.5f, ra = W.w * 0.35f;
	for (int j = 0; j < a->ch; ++j)
		for (int i = 0; i < a->cw; ++i) {
			if (a->explorada[j * a->cw + i]) continue;
			float px = i * CELDA + CELDA * 0.5f, py = j * CELDA + CELDA * 0.5f;
			if (!tile_walk((int)px, (int)py)) continue;
			float d = d2(px, py, x, y);
			if (anillo) { if (d < 20 * 20) continue; float r = sqrtf(d2(px, py, cx, cy)) - ra; d += r * r * 4; }
			if (d < bd) { bd = d; best = j * a->cw + i; }
		}
	return best;
}

/* todo explorado: la celda transitable que hace más tiempo que no se ve (para encontrar lo que queda del rival) */
static int celda_olvidada(int p, float x, float y) {
	Ia *a = &ia[p];
	int best = -1; float bv = -1e18f;
	for (int j = 0; j < a->ch; ++j)
		for (int i = 0; i < a->cw; ++i) {
			float px = i * CELDA + CELDA * 0.5f, py = j * CELDA + CELDA * 0.5f;
			if (!tile_walk((int)px, (int)py) || a->casa[j * a->cw + i]) continue;
			float viejo = (unsigned short)(a->vuelta - a->vista[j * a->cw + i]);
			float v = viejo * 4 - sqrtf(d2(px, py, x, y));
			if (v > bv) { bv = v; best = j * a->cw + i; }
		}
	return best;
}

static void explorar(int p, Censo *c) {
	Ia *a = &ia[p];
	const Nivel *nv = &niveles[W.pl[p].diff];
	if (!nv->explora || !c->tc) return;
	/* vuelta inicial con un aldeano: seis puntos alrededor de la base */
	Entity *e = ent_get(a->explorador_ini);
	if (W.time < 60 && !e && a->ruta == 0) {
		for (int i = W.nents - 1; i >= 0; --i)
			if (vivo(&W.ents[i]) && W.ents[i].owner == p && W.ents[i].type == E_VILLAGER) { a->explorador_ini = ent_ref(i); e = &W.ents[i]; break; }
	}
	if (e && a->ruta < 6) {
		if (e->state == ST_IDLE || a->ruta == 0) {
			float an = a->ruta * 1.047f + p, r = 15.0f;
			float x = CLAMP(c->bx + dcosf(an) * r, 2, W.w - 3), y = CLAMP(c->by + dsinf(an) * r, 2, W.h - 3);
			cmd_move(a->explorador_ini, x, y);
			++a->ruta;
		}
	} else if (e && a->ruta >= 6 && e->state == ST_IDLE) {
		a->explorador_ini = SIN;   /* vuelve a trabajar: lo recoge el reparto */
	}
	/* explorador a caballo: la unidad militar más rápida sin oro */
	Entity *x = ent_get(a->explorador);
	if (!x && W.pl[p].age >= AGE_TOOL) {
		int mejor = -1;
		for (int u = 0; u < E_TYPE_COUNT; ++u)
			if (militar(u) && ud(u)->speed >= 2.5f && !ud(u)->gold && ud(u)->age <= W.pl[p].age) mejor = u;
		if (mejor < 0) return;
		for (int i = 0; i < W.nents; ++i)
			if (vivo(&W.ents[i]) && W.ents[i].owner == p && (int)W.ents[i].type == mejor) { a->explorador = ent_ref(i); x = &W.ents[i]; break; }
		if (!x && (a->explora_t == 0 || W.time - a->explora_t > 240)) {
			for (int i = 0; i < W.nents; ++i) {
				Entity *b = &W.ents[i];
				if (vivo(b) && b->owner == p && b->type == ud(mejor)->trainer && b->progress >= 1 && b->ntrain == 0) {
					if (cmd_train(p, ent_ref(i), (EntType)mejor)) a->explora_t = W.time;
					break;
				}
			}
		}
	}
	if (x && (x->state == ST_IDLE || (x->state == ST_ATTACK && x->hp < x->maxhp * 0.6f))) {
		if (x->hp < x->maxhp * 0.4f) cmd_move(a->explorador, c->bx, c->by);
		else {
			int cel = celda_sin_explorar(p, x->x, x->y, 0);
			if (cel >= 0) cmd_move(a->explorador, (cel % a->cw) * CELDA + CELDA * 0.5f, (cel / a->cw) * CELDA + CELDA * 0.5f);
		}
	}
}

/* el enemigo visible más interesante cerca de una unidad: soldados, luego aldeanos y sacerdotes, luego torres y edificios */
static int enemigo_cerca(int p, const Entity *u, float r) {
	int best = -1; float bd = 1e18f;
	int asedio = es_asedio(u->type);
	for (int i = 0; i < W.nents; ++i) {
		Entity *t = &W.ents[i];
		if (!vivo(t) || !hostile(p, t->owner) || is_resource(t->type)) continue;
		float d = d2(t->x, t->y, u->x, u->y);
		if (d > r * r || !ve(p, t->x, t->y)) continue;
		if (militar(t->type)) d *= 0.4f;
		else if (t->type == E_TOWER) d *= 0.7f;
		else if (is_building(t->type)) d = d * 1.5f + 30;
		if (t->type == E_PRIEST) d *= 0.5f;
		if (asedio) d = is_building(t->type) ? d * 0.1f : d * 4;   /* el asedio, contra edificios */
		if (d < bd) { bd = d; best = i; }
	}
	return best;
}

static int ordenar_ataque(int p, int i, float r) {
	Entity *u = &W.ents[i];
	Entity *t = ent_get(u->target);
	if (u->state == ST_ATTACK && t && militar(t->type)) return 1;
	int en = enemigo_cerca(p, u, r);
	if (en < 0) return u->state == ST_ATTACK && t;
	if (u->state == ST_ATTACK && t && t == &W.ents[en]) return 1;
	/* deja un edificio solo si aparece un soldado cerca */
	if (u->state == ST_ATTACK && t && !(militar(W.ents[en].type) && is_building(t->type))) return 1;
	cmd_target(ent_ref(i), ent_ref(en));
	return 1;
}

static void mover_si_lejos(int i, float x, float y, float tol) {
	Entity *u = &W.ents[i];
	if (d2(u->x, u->y, x, y) < tol * tol) return;
	/* ya va hacia allí (o cerca): no se recalcula el camino, que es lo que más cuesta */
	if (u->state == ST_MOVE && d2(u->tx, u->ty, x, y) < 25) return;
	cmd_move(ent_ref(i), x, y);
}

static int sacerdotes_quiero(int p) {
	int n = niveles[W.pl[p].diff].sacerdotes;
	if (vic_reliquias()) n = MAX(1, n) + (W.pl[p].diff >= 1 ? 2 : 0);
	return n;
}

/* ¿va a por la victoria con una Maravilla? Con la victoria por Maravilla, la normal y las difíciles en la Edad del Hierro;
 * con la estándar, la difícil cuando le sobra de todo. */
static int quiere_maravilla(int p) {
	const Player *pl = &W.pl[p];
	if (!vic_maravilla() || pl->age < bld_def[E_WONDER].age || pl->diff == 0) return 0;
	if (W.setup.victory == VIC_WONDER) return 1;
	return pl->diff >= 2 && pl->res[RES_WOOD] > bld_def[E_WONDER].wood + 400 && pl->res[RES_GOLD] > bld_def[E_WONDER].gold + 400;
}

static int maravilla_rival(int p, float *x, float *y, Ref *r) {
	Ia *a = &ia[p];
	if (!vic_maravilla()) return 0;
	for (int k = 0; k < a->ncon; ++k) {
		if (a->con[k].t != E_WONDER) continue;
		Entity *e = ent_get(a->con[k].r);
		if (!e || e->progress < 0.5f) continue;
		if (x) *x = a->con[k].x;
		if (y) *y = a->con[k].y;
		if (r) *r = a->con[k].r;
		return 1;
	}
	return 0;
}

/* objetivo de ataque: el edificio enemigo conocido más cercano; si no conoce ninguno, explorar hacia donde estarán */
static int elegir_objetivo(int p, float x, float y) {
	Ia *a = &ia[p];
	int best = -1; float bd = 1e18f;
	int isla_ref = islas() ? zona_de(x, y) : 0;
	for (int k = 0; k < a->ncon; ++k) {
		float d = d2(a->con[k].x, a->con[k].y, x, y);
		if (islas()) {
			/* D415: un edificio en una isla a la que no se llega por mar no es objetivo; los de la isla donde ya está el ejército, primero */
			int ik = zona_de(a->con[k].x, a->con[k].y);
			if (!isla_alcanzable(a, p, ik)) continue;
			if (ik && isla_ref && ik != isla_ref) d *= 3.0f;
		}
		if (a->con[k].t == E_TOWN_CENTER) d *= 0.7f;
		if (a->con[k].t == E_TOWER) d *= 1.6f;   /* rodea las torres si puede */
		if (a->con[k].t == E_WONDER) d *= 0.02f; /* una Maravilla rival es lo primero */
		if (a->con[k].t == E_TEMPLE && vic_reliquias()) d *= 0.1f;   /* y los templos que guardan reliquias */
		if (d < bd) { bd = d; best = k; }
	}
	if (best >= 0) { a->obj_x = a->con[best].x; a->obj_y = a->con[best].y; a->objetivo = a->con[best].r; return 1; }
	int cel = -1;
	/* D415: en las islas, una celda en una zona a la que no se llega por mar se da por explorada y se busca otra */
	for (int intento = 0; intento < 12; ++intento) {
		cel = celda_sin_explorar(p, x, y, 1);
		if (cel < 0) cel = celda_sin_explorar(p, x, y, 0);
		if (cel < 0) cel = celda_olvidada(p, x, y);
		if (cel < 0 || !islas()) break;
		float cx = (cel % a->cw) * CELDA + CELDA * 0.5f, cy = (cel / a->cw) * CELDA + CELDA * 0.5f;
		if (isla_alcanzable(a, p, zona_de(cx, cy))) break;
		a->vista[cel] = a->vuelta; a->explorada[cel] = 1;
		cel = -1;
	}
	if (cel < 0) return 0;
	a->obj_x = (cel % a->cw) * CELDA + CELDA * 0.5f; a->obj_y = (cel / a->cw) * CELDA + CELDA * 0.5f;
	a->objetivo = SIN;
	return 1;
}

static int del_ejercito(int p, const Entity *u) {
	return vivo(u) && u->owner == p && militar(u->type) && ent_get(ia[p].explorador) != u && u->state != ST_BOARD && u->state != ST_ABOARD;
}

static void ejercito(int p, Censo *c) {
	Ia *a = &ia[p];
	Player *pl = &W.pl[p];
	const Nivel *nv = &niveles[pl->diff];
	if (!c->n[E_TOWN_CENTER]) {
		/* sin centro urbano: todos al ataque */
		for (int i = 0; i < W.nents; ++i)
			if (del_ejercito(p, &W.ents[i]) && !ordenar_ataque(p, i, 40) && W.ents[i].state == ST_IDLE && elegir_objetivo(p, W.ents[i].x, W.ents[i].y))
				cmd_move(ent_ref(i), a->obj_x, a->obj_y);
		return;
	}
	/* punto de reunión: entre la base y el centro del mapa */
	{
		float dx = W.w * 0.5f - c->bx, dy = W.h * 0.5f - c->by, l = sqrtf(dx * dx + dy * dy) + 0.01f;
		float rx = c->bx + dx / l * 8, ry = c->by + dy / l * 8;
		for (int k = 0; k < 8 && !tile_walk((int)rx, (int)ry); ++k) { rx -= dx / l; ry -= dy / l; }
		a->reunion_x = rx; a->reunion_y = ry;
		/* con una Maravilla propia, el ejército la guarda */
		for (int i = 0; i < W.nents; ++i) {
			Entity *w = &W.ents[i];
			if (vivo(w) && w->owner == p && w->type == E_WONDER) {
				float wx = w->x + (W.w * 0.5f - w->x) * 0.1f, wy = w->y + w->size * 0.5f + 2;
				if (tile_walk((int)wx, (int)wy)) { a->reunion_x = wx; a->reunion_y = wy; }
				break;
			}
		}
	}

	/* amenaza: soldados enemigos visibles dentro de la base */
	float ax = 0, ay = 0, fam = 0; int nam = 0;
	for (int i = 0; i < W.nents; ++i) {
		Entity *t = &W.ents[i];
		if (!vivo(t) || !hostile(p, t->owner) || !is_unit(t->type) || t->type == E_VILLAGER) continue;
		/* los barcos y quien va a bordo no son amenaza para el ejército de tierra (no los alcanza: se quedaría defendiendo
		 * para siempre); de los barcos se ocupa la flota */
		if (is_naval(t->type) || t->state == ST_ABOARD) continue;
		if (!a->casa[celda(t->x, t->y, a)] || !ve(p, t->x, t->y)) continue;
		ax += t->x; ay += t->y; ++nam; fam += fuerza(t);
	}
	int habia = a->amenaza;
	a->amenaza = nam;
	if (nam) { a->amenaza_x = ax / nam; a->amenaza_y = ay / nam; }
	else if (habia) { a->defendido_t = W.time; ++a->ataques_rechazados; }

	/* fuerza y centro del ejército (en las islas, aparte el de los que están en la isla de la base y el de los desembarcados) */
	a->base_isla = islas() ? zona_de(c->bx, c->by) : 0;
	int isla_obj = islas() ? zona_de(a->obj_x, a->obj_y) : 0;
	int por_mar = islas() && isla_obj && a->base_isla && isla_obj != a->base_isla;
	float fz = 0, cx = 0, cy = 0, bcx = 0, bcy = 0, lcx = 0, lcy = 0; int n = 0, nb = 0, nl = 0;
	for (int i = 0; i < W.nents; ++i) {
		Entity *u = &W.ents[i];
		if (!del_ejercito(p, u)) continue;
		fz += fuerza(u); cx += u->x; cy += u->y; ++n;
		if (!por_mar) continue;
		int iu = zona_de(u->x, u->y);
		if (iu == a->base_isla) { bcx += u->x; bcy += u->y; ++nb; }
		else if (iu == isla_obj) { lcx += u->x; lcy += u->y; ++nl; }
	}
	if (n) { cx /= n; cy /= n; }
	if (nb) { bcx /= nb; bcy /= nb; } else { bcx = cx; bcy = cy; nb = n; }
	if (nl) { lcx /= nl; lcy /= nl; } else { lcx = cx; lcy = cy; }
	if (!por_mar) { nb = n; bcx = cx; bcy = cy; }

	/* defensa: si hay amenaza, el ejército de casa (y el que está fuera si la amenaza es seria) a por ella */
	/* D415: en las islas, uno o dos infiltrados (un transporte con 1-2 soldados) los frenan los de casa y las torres; no se
	 * deshace por ellos la reunión ni la invasión (antes el ejército entero se quedaba «defendiendo» para siempre) */
	int pequena = islas() && nam <= 3 && fam < fz * 0.35f;
	if (nam && pequena) {
		for (int i = 0; i < W.nents; ++i) {
			Entity *u = &W.ents[i];
			if (!del_ejercito(p, u) || d2(u->x, u->y, c->bx, c->by) > 30 * 30 || zona_de(u->x, u->y) != a->base_isla) continue;
			if (!ordenar_ataque(p, i, 12) && u->state == ST_IDLE) mover_si_lejos(i, a->amenaza_x, a->amenaza_y, 3);
		}
	}
	if (nam && !pequena && (a->modo != M_ATAQUE || fam > fz * 0.5f || d2(cx, cy, c->bx, c->by) < 30 * 30)) {
		if (a->modo == M_ATAQUE || a->modo == M_REUNIR) a->modo = M_RETIRADA;
		for (int i = 0; i < W.nents; ++i) {
			if (!del_ejercito(p, &W.ents[i])) continue;
			if (!ordenar_ataque(p, i, 12)) mover_si_lejos(i, a->amenaza_x, a->amenaza_y, 3);
		}
		/* los aldeanos heridos con soldados encima huyen al centro urbano (normal y difícil) */
		if (nv->huida && fam > fz) {
			for (int i = 0; i < W.nents; ++i) {
				Entity *u = &W.ents[i];
				if (!vivo(u) || u->owner != p || u->type != E_VILLAGER || u->hp >= u->maxhp) continue;
				if (d2(u->x, u->y, a->amenaza_x, a->amenaza_y) < 6 * 6 && d2(u->x, u->y, c->bx, c->by) > 4 * 4) cmd_move(ent_ref(i), c->bx + 1, c->by + 2);
			}
		}
		snprintf(a->ultimo, sizeof a->ultimo, "defiende (%d enemigos)", nam);
		return;
	}

	int umbral = nv->ataque_min + a->oleada * nv->ataque_crece;
	umbral = MIN(umbral, (W.setup.popcap > 0 ? W.setup.popcap : 200) * 6 / 10);
	int contra = nv->contraunidades && a->defendido_t > 0 && W.time - a->defendido_t < 60 && n >= nv->ataque_min / 2 + 2;

	switch (a->modo) {
	case M_PAZ:
	case M_RETIRADA:
		/* todos a la reunión; los heridos se curan junto a los sacerdotes */
		for (int i = 0; i < W.nents; ++i) {
			Entity *u = &W.ents[i];
			if (!del_ejercito(p, u)) continue;
			if (islas() && a->base_isla && zona_de(u->x, u->y) != a->base_isla) continue;   /* los varados los recoge un transporte */
			if (u->state == ST_ATTACK && a->modo == M_PAZ) continue;
			if (u->state == ST_IDLE || a->modo == M_RETIRADA) mover_si_lejos(i, a->reunion_x + (i % 5) - 2, a->reunion_y + (i / 5 % 5) - 2, 4);
		}
		if (a->modo == M_RETIRADA && (nb == 0 || d2(bcx, bcy, a->reunion_x, a->reunion_y) < 8 * 8)) a->modo = M_PAZ;
		/* normal y difícil solo atacan si su ejército supera lo que han visto del rival (o si ya no caben más soldados) */
		int torres = 0;
		for (int k = 0; k < a->ncon; ++k) torres += a->con[k].t == E_TOWER;
		int lleno = pl->pop >= world_pop_cap(p) && world_pop_cap(p) >= (W.setup.popcap > 0 ? W.setup.popcap : 200);
		/* tras 6 minutos sin atacar con el ejército listo, se ataca igualmente: si no, dos rivales parejos (sobre todo en
		 * islas) se quedan mirándose hasta el final */
		int harto = W.time - a->modo_t > 360;
		int compensa = !nv->contraunidades || lleno || harto || fz >= 1.3f * a->fuerza_rival + 20.0f * MIN(torres, 3);
		/* urgencia: una Maravilla rival en pie o un rival con todas las reliquias; hay que salir con lo que haya */
		int urge = maravilla_rival(p, NULL, NULL, NULL) || (vic_reliquias() && W.relic_team >= 0 && hostile(p, W.relic_team));
		if (a->modo == M_PAZ && n > 0 && (((n >= umbral || contra) && compensa && W.time >= nv->ataque_t * 60 * (a->oleada == 0 ? 1.0f : 0.5f)) || (urge && n >= 3))) {
			a->modo = M_REUNIR; a->modo_t = W.time;
		}
		break;
	case M_REUNIR: {
		/* espera a que estén casi todos juntos (o a que pase un rato) antes de salir */
		int juntos = 0;
		for (int i = 0; i < W.nents; ++i) {
			Entity *u = &W.ents[i];
			if (!del_ejercito(p, u)) continue;
			if (islas() && a->base_isla && zona_de(u->x, u->y) != a->base_isla) continue;
			if (d2(u->x, u->y, a->reunion_x, a->reunion_y) < 7 * 7) ++juntos;
			else mover_si_lejos(i, a->reunion_x, a->reunion_y, 5);
		}
		if (n == 0) { a->modo = M_PAZ; break; }
		if (juntos >= nb * 8 / 10 || W.time - a->modo_t > 45) {
			if (elegir_objetivo(p, a->reunion_x, a->reunion_y)) {
				a->modo = M_ATAQUE; a->modo_t = W.time; a->fuerza_inicial = fz;
				if (a->primer_ataque_t < 0) a->primer_ataque_t = W.time;
				++a->oleada;
				snprintf(a->ultimo, sizeof a->ultimo, "ataca con %d", n);
			}
		}
		break;
	}
	case M_ATAQUE: {
		/* retirada si ha perdido mucho y lo que ve es más fuerte (normal y difícil) */
		float fe = 0;
		for (int i = 0; i < W.nents; ++i) {
			Entity *t = &W.ents[i];
			if (!vivo(t) || !hostile(p, t->owner) || !militar(t->type)) continue;
			if (d2(t->x, t->y, cx, cy) < 12 * 12 && ve(p, t->x, t->y)) fe += fuerza(t);
		}
		int a_bordo = 0;
		for (int i = 0; i < W.nents; ++i) if (vivo(&W.ents[i]) && W.ents[i].owner == p && W.ents[i].state == ST_ABOARD) ++a_bordo;
		if ((n == 0 && !a_bordo) || (nv->retirada && fz < a->fuerza_inicial * 0.35f && fe > fz)) { a->modo = M_RETIRADA; snprintf(a->ultimo, sizeof a->ultimo, "se retira"); break; }
		/* refuerzos: los que estén en casa se unen si el grupo sigue fuerte */
		/* objetivo destruido o alcanzado: el siguiente */
		Entity *ob = ent_get(a->objetivo);
		if ((a->objetivo.idx >= 0 && (!ob || ob->state == ST_DYING || !hostile(p, ob->owner))) || (a->objetivo.idx < 0 && d2(cx, cy, a->obj_x, a->obj_y) < 5 * 5) ||
		    W.time - a->modo_t > (a->objetivo.idx < 0 ? (por_mar ? 420 : 120) : 240)) {
			a->modo_t = W.time;
			/* una celda que no se alcanza se da por vista, para que la siguiente búsqueda elija otra */
			if (a->objetivo.idx < 0) { int ce = celda(a->obj_x, a->obj_y, a); a->vista[ce] = a->vuelta; a->explorada[ce] = 1; }
				if (!elegir_objetivo(p, por_mar ? lcx : cx, por_mar ? lcy : cy)) { a->modo = M_RETIRADA; break; }
			isla_obj = zona_de(a->obj_x, a->obj_y);
			por_mar = islas() && isla_obj && a->base_isla && isla_obj != a->base_isla;
		}
		/* avance en grupo: cada unidad va a un punto algo por delante del centro del grupo */
		if (por_mar) { cx = lcx; cy = lcy; }
		float dx = a->obj_x - cx, dy = a->obj_y - cy, l = sqrtf(dx * dx + dy * dy) + 0.01f;
		float paso = MIN(l, 7.0f);
		float px = cx + dx / l * paso, py = cy + dy / l * paso;
		for (int i = 0; i < W.nents; ++i) {
			Entity *u = &W.ents[i];
			if (!del_ejercito(p, u)) continue;
			int iu = por_mar ? zona_de(u->x, u->y) : 0;
			/* los heridos vuelven a casa (con los sacerdotes) */
			if (nv->retirada && u->hp < u->maxhp * 0.25f && fe > 0 && !ud(u->type)->ranged && (!por_mar || iu == a->base_isla)) { mover_si_lejos(i, a->reunion_x, a->reunion_y, 3); continue; }
			if (ordenar_ataque(p, i, 8)) continue;
			if (por_mar) {
				/* D415: los desembarcados van directos al objetivo; los de casa esperan el transporte en la reunión; los que
				 * estén en otra isla los recoge un transporte (rescate) */
				if (iu == isla_obj) { mover_si_lejos(i, ob ? ob->x : a->obj_x, ob ? ob->y : a->obj_y, 2); continue; }
				if (iu == a->base_isla) { mover_si_lejos(i, a->reunion_x, a->reunion_y, 5); }
				continue;
			}
			/* el objetivo está a la vista: a por él */
			if (ob && d2(u->x, u->y, ob->x, ob->y) < 8 * 8) { cmd_target(ent_ref(i), a->objetivo); continue; }
			/* los rezagados se acercan al grupo; los demás avanzan */
			if (d2(u->x, u->y, cx, cy) > 10 * 10) mover_si_lejos(i, cx, cy, 3);
			else mover_si_lejos(i, px + (i % 3) - 1, py + (i / 3 % 3) - 1, 2);
		}
		/* los sacerdotes siguen al grupo por detrás (curan solos a los heridos cercanos) */
		if (nv->sacerdotes)
			for (int i = 0; i < W.nents; ++i) {
				Entity *u = &W.ents[i];
				if (!vivo(u) || u->owner != p || u->type != E_PRIEST || u->state == ST_ATTACK || u->carry) continue;
				mover_si_lejos(i, cx - dx / l * 3, cy - dy / l * 3, 4);
			}
		break;
	}
	}
}

/* sacerdotes: recogen reliquias y, en casa, curan (lo hacen solos al estar ociosos) */
static void reliquias(int p, Censo *c) {
	if (!c->hechos[E_TEMPLE]) return;
	for (int i = 0; i < W.nents; ++i) {
		Entity *pr = &W.ents[i];
		if (!vivo(pr) || pr->owner != p || pr->type != E_PRIEST || pr->state != ST_IDLE) continue;
		if (pr->carry) {
			for (int j = 0; j < W.nents; ++j)
				if (vivo(&W.ents[j]) && W.ents[j].owner == p && W.ents[j].type == E_TEMPLE && W.ents[j].progress >= 1) { cmd_target(ent_ref(i), ent_ref(j)); break; }
			continue;
		}
		if (ia[p].modo == M_ATAQUE && !vic_reliquias()) continue;
		int best = -1; float bd = vic_reliquias() ? 1e18f : 30.0f * 30.0f;
		for (int j = 0; j < W.nents; ++j) {
			Entity *r = &W.ents[j];
			if (!r->alive || r->type != E_RELIC || !ia[p].explorada[celda(r->x, r->y, &ia[p])]) continue;
			float d = d2(r->x, r->y, pr->x, pr->y);
			if (d < bd) { bd = d; best = j; }
		}
		if (best >= 0) cmd_target(ent_ref(i), ent_ref(best));
		else if (d2(pr->x, pr->y, ia[p].reunion_x, ia[p].reunion_y) > 25) cmd_move(ent_ref(i), ia[p].reunion_x - 1, ia[p].reunion_y - 1);
	}
}

/* ---------- flota: pesca, trirremes y transportes (mapas de agua) ---------- */

static int en_barco(const Entity *u) { return u->state == ST_BOARD || u->state == ST_ABOARD; }

/* D415: recoger varados. Una unidad militar ociosa en otra isla que la de la base (desembarcada que no sigue el ataque, o
 * dejada en un islote) la recoge un transporte libre: va a la orilla más cercana, la unidad embarca y vuelve al muelle.
 * Si en el viaje no se llega o pasan 4 minutos, se aparta a esa unidad 10 minutos. Un rescate a la vez. */
static void rescatar(int p, Entity *dock, int isla_obj, int por_mar) {
	Ia *a = &ia[p];
	if (!a->base_isla || !a->base_mar) return;
	Entity *tr = ent_get(a->resc_tr), *v = ent_get(a->resc_u);
	if (a->resc_fase && (!tr || !vivo(tr) || tr->type != E_TRANSPORT)) { a->resc_fase = 0; a->resc_tr = a->resc_u = SIN; return; }
	if (!a->resc_fase) {
		if (W.time - a->resc_t < 20) return;
		a->resc_t = W.time;
		/* un transporte libre en casa (si solo hay uno y se ataca, no se toca) */
		int libre = -1, total = 0;
		for (int i = 0; i < W.nents; ++i) {
			Entity *b = &W.ents[i];
			if (!vivo(b) || b->owner != p || b->type != E_TRANSPORT) continue;
			++total;
			if (libre < 0 && b->state == ST_IDLE && transport_load(b) == 0 && d2(b->x, b->y, dock->x, dock->y) < 16 * 16) libre = i;
		}
		if (libre < 0 || (a->modo == M_ATAQUE && total < 2)) return;
		for (int j = 0; j < W.nents; ++j) {
			Entity *u = &W.ents[j];
			if (!vivo(u) || u->owner != p || !(militar(u->type) || u->type == E_PRIEST) || u->state != ST_IDLE) continue;
			int iu = zona_de(u->x, u->y);
			if (!iu || iu == a->base_isla || (a->modo == M_ATAQUE && por_mar && iu == isla_obj)) continue;
			if (ent_get(a->resc_ign) == u && W.time - a->resc_ign_t < 600) continue;
			if (enemigo_cerca(p, u, 10) >= 0) continue;
			if (!sitio_orilla(a, p, iu, a->base_mar, u->x, u->y, 16, &a->resc_wx, &a->resc_wy, &(float){ 0 }, &(float){ 0 })) { a->resc_ign = ent_ref(j); a->resc_ign_t = W.time; continue; }
			a->resc_fase = 1; a->resc_tr = ent_ref(libre); a->resc_u = ent_ref(j); a->resc_t = W.time;
			cmd_move(a->resc_tr, a->resc_wx, a->resc_wy);
			snprintf(a->ultimo, sizeof a->ultimo, "rescata a un varado en %.0f,%.0f", u->x, u->y);
			break;
		}
		return;
	}
	if (a->resc_fase == 1) {
		int carga = transport_load(tr);
		if (carga > 0 || !v || !vivo(v) || v->state == ST_ABOARD) { a->resc_fase = 2; a->resc_t = W.time; cmd_move(a->resc_tr, dock->x, dock->y); return; }
		if (W.time - a->resc_t > 240) { a->resc_ign = a->resc_u; a->resc_ign_t = W.time; a->resc_fase = 2; a->resc_t = W.time; cmd_move(a->resc_tr, dock->x, dock->y); return; }
		if (d2(tr->x, tr->y, a->resc_wx, a->resc_wy) > 4 * 4) { if (tr->state == ST_IDLE) cmd_move(a->resc_tr, a->resc_wx, a->resc_wy); }
		else if (v->state != ST_BOARD) cmd_target(a->resc_u, a->resc_tr);
		return;
	}
	/* fase 2: de vuelta; en casa baja a los rescatados (en un ataque, se quedan a bordo para la siguiente oleada) */
	if (d2(tr->x, tr->y, dock->x, dock->y) < 10 * 10 && tr->state == ST_IDLE) {
		if (transport_load(tr) > 0 && a->modo != M_ATAQUE) { cmd_move(a->resc_tr, dock->x, dock->y); }
		else { a->resc_fase = 0; a->resc_tr = a->resc_u = SIN; }
	} else if (tr->state == ST_IDLE) cmd_move(a->resc_tr, dock->x, dock->y);
	else if (W.time - a->resc_t > 300) { a->resc_fase = 0; a->resc_tr = a->resc_u = SIN; }
}

static void barcos(int p, Censo *c) {
	Ia *a = &ia[p];
	Player *pl = &W.pl[p];
	Entity *dock = NULL;
	for (int i = 0; i < W.nents && !dock; ++i)
		if (vivo(&W.ents[i]) && W.ents[i].owner == p && W.ents[i].type == E_DOCK && W.ents[i].progress >= 1) dock = &W.ents[i];
	if (!dock) return;
	int dk = (int)(dock - W.ents);
	/* D415: isla y agua de la base, isla del objetivo y sitio de desembarco (agua y tierra de la misma agua que el muelle, junto al objetivo) */
	a->base_isla = zona_de(c->bx, c->by);
	a->base_mar = mar_de(dock->x, dock->y);
	int isla_obj = zona_de(a->obj_x, a->obj_y);
	int por_mar = islas() && isla_obj && a->base_isla && isla_obj != a->base_isla;
	if (por_mar && (!a->anf_ok || fabsf(a->anf_ox - a->obj_x) + fabsf(a->anf_oy - a->obj_y) > 0.5f || W.time - a->anf_t > 90)) {
		a->anf_ok = sitio_orilla(a, p, isla_obj, a->base_mar, a->obj_x, a->obj_y, 0, &a->anf_wx, &a->anf_wy, &a->anf_lx, &a->anf_ly);
		a->anf_ox = a->obj_x; a->anf_oy = a->obj_y; a->anf_t = W.time;
	}
	/* a dónde navegan la flota y los transportes: el agua del sitio de desembarco (o el objetivo, si no hay) */
	float nav_x = por_mar && a->anf_ok ? a->anf_wx : a->obj_x, nav_y = por_mar && a->anf_ok ? a->anf_wy : a->obj_y;
	rescatar(p, dock, isla_obj, por_mar);
	int t_rescate = a->resc_fase ? (int)(ent_get(a->resc_tr) - W.ents) : -1;
	/* producción: pesqueros según los peces, trirremes y un transporte en las islas */
	int peces = peces_cerca(p, c);
	int barcos_rivales = 0;
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (vivo(e) && is_naval(e->type) && hostile(p, e->owner) && ve(p, e->x, e->y)) ++barcos_rivales;
	}
	/* D415: transportes y flota de guerra solo si hay rivales a los que no se llega andando (el generador de «islas» a veces
	 * deja a todos en la misma masa de tierra); mientras no se conozca ninguno, se supone que sí */
	int hay_mar = hay_rivales_por_mar(p);
	int quiero[3] = { MIN(6, (peces + 3) / 4), islas() && hay_mar ? 3 + 3 * pl->age : pl->diff >= 2 ? 2 : 0, islas() && hay_mar ? 2 + pl->age : 0 };
	static const int tipos[3] = { E_FISHBOAT, E_WARSHIP, E_TRANSPORT };
	if (barcos_rivales) quiero[1] = MAX(quiero[1], MIN(barcos_rivales + 1, 6));
	/* sin el primer transporte no hay ataque: va antes que los trirremes */
	if (islas() && hay_mar && c->n[E_TRANSPORT] == 0 && quiero[2] > 0) { quiero[1] = MIN(quiero[1], c->n[E_WARSHIP]); }
	if (dock->ntrain == 0 && pl->pop < world_pop_cap(p))
		for (int k = 0; k < 3; ++k)
			if (c->n[tipos[k]] < quiero[k] && ud(tipos[k])->age <= pl->age && puede_pagar(p, tipos[k], 0, 0) && cmd_train(p, ent_ref(dk), (EntType)tipos[k])) break;
	/* desembarco en oleada: los transportes salen todos a la vez cuando están todos en casa y llenos (o no queda nadie que
	 * subir, o tras 60 s cargando), para que lleguen 10-20 soldados juntos y no de 5 en 5 */
	int total_t = 0, en_casa = 0, llenos = 0, esperando = 0;
	for (int i = 0; i < W.nents; ++i) {
		Entity *b = &W.ents[i];
		if (!vivo(b) || b->owner != p || b->type != E_TRANSPORT) continue;
		++total_t;
		if (b->state == ST_IDLE && d2(b->x, b->y, dock->x, dock->y) < 16 * 16) { ++en_casa; if (transport_load(b) >= 5) ++llenos; }
	}
	for (int j = 0; j < W.nents; ++j) {
		Entity *u = &W.ents[j];
		if (vivo(u) && u->owner == p && militar(u->type) && !en_barco(u) && d2(u->x, u->y, a->reunion_x, a->reunion_y) < 14 * 14) ++esperando;
	}
	int cargados = 0;
	for (int i = 0; i < W.nents; ++i) {
		Entity *b = &W.ents[i];
		if (vivo(b) && b->owner == p && b->type == E_TRANSPORT && b->state == ST_IDLE && transport_load(b) > 0 && d2(b->x, b->y, dock->x, dock->y) < 16 * 16) ++cargados;
	}
	if (!cargados) a->embarque_t = W.time;
	int zarpar = cargados > 0 && ((en_casa == total_t && (llenos == en_casa || esperando == 0)) || W.time - a->embarque_t > 60);
	/* D415: antes de zarpar, flota de guerra para dominar el agua del desembarco (los rivales vistos + 1, hasta 4; la fácil, sin
	 * flota mínima); si tarda más de 150 s en tenerla, salen igualmente */
	{
		int guerra = 0, rivales_g = 0;
		for (int i = 0; i < W.nents; ++i) {
			Entity *e = &W.ents[i];
			if (!vivo(e) || e->type != E_WARSHIP) continue;
			if (e->owner == p) ++guerra; else if (hostile(p, e->owner) && ve(p, e->x, e->y)) ++rivales_g;
		}
		int minima = pl->diff == 0 ? 0 : MIN(4, 1 + rivales_g);
		if (guerra < minima && W.time - a->embarque_t < 150) zarpar = 0;
	}
	/* colonia: si en la isla propia no queda madera u oro y se conoce en otra, un transporte lleva allí 5 aldeanos (que
	 * levantan un almacén al llegar) */
	int isla_base = isla_de(c->bx, c->by), colonos = 0;
	for (int j = 0; j < W.nents; ++j) {
		Entity *u = &W.ents[j];
		if (vivo(u) && u->owner == p && u->type == E_VILLAGER && !en_barco(u) && isla_de(u->x, u->y) != isla_base) ++colonos;
	}
	a->colonia = 0;
	/* durante un ataque solo si la falta de madera ya ahoga (sin madera no hay transportes, mercado ni granjas) */
	if (isla_base && colonos < 5 && (a->modo != M_ATAQUE || W.pl[p].res[RES_WOOD] < 150) && c->aldeanos >= 12) {
		static const int kinds[2] = { RES_WOOD, RES_GOLD };
		for (int k = 0; k < 2 && !a->colonia; ++k) {
			if (recurso_cercano(p, kinds[k], c->bx, c->by, 300) >= 0) continue;
			float bd = 1e18f;
			for (int j = 0; j < W.nents; ++j) {
				Entity *r = &W.ents[j];
				if (!r->alive || !is_resource(r->type) || res_kind_of(r->type) != kinds[k] || r->amount <= 0) continue;
				if (!a->explorada[celda(r->x, r->y, a)] || isla_de(r->x, r->y) == isla_base || !isla_de(r->x, r->y)) continue;
				float d = d2(r->x, r->y, dock->x, dock->y);
				if (d < bd) { bd = d; a->colonia_x = r->x; a->colonia_y = r->y; a->colonia = 1; }
			}
		}
	}
	int t_colonia = -1;
	for (int i = 0; i < W.nents; ++i) {
		Entity *b = &W.ents[i];
		if (!vivo(b) || b->owner != p || !is_naval(b->type)) continue;
		if (b->type == E_TRANSPORT && a->colonia && b->state == ST_IDLE && d2(b->x, b->y, dock->x, dock->y) < 16 * 16) {
			t_colonia = i;
			int carga = transport_load(b), subiendo = 0;
			for (int j = 0; j < W.nents; ++j) if (vivo(&W.ents[j]) && W.ents[j].owner == p && W.ents[j].state == ST_BOARD && W.ents[j].type == E_VILLAGER) ++subiendo;
			for (int j = 0; j < W.nents && carga + subiendo < 5 - colonos; ++j) {
				Entity *u = &W.ents[j];
				if (!vivo(u) || u->owner != p || u->type != E_VILLAGER || en_barco(u) || u->state == ST_BUILD || u->carry > 4) continue;
				if (isla_de(u->x, u->y) != isla_base || ia[p].explorador_ini.idx == j) continue;
				cmd_target(ent_ref(j), ent_ref(i));
				++subiendo;
			}
			if (carga == 0) a->colonia_t = W.time;
			if (carga > 0 && (carga + colonos >= 5 || subiendo == 0 || W.time - a->colonia_t > 40)) cmd_move(ent_ref(i), a->colonia_x, a->colonia_y);
			break;   /* un transporte basta */
		}
	}
	for (int i = 0; i < W.nents; ++i) {
		Entity *b = &W.ents[i];
		if (!vivo(b) || b->owner != p || !is_naval(b->type)) continue;
		if (b->type == E_FISHBOAT && b->state == ST_IDLE) {
			int best = -1; float bd = 1e18f;
			for (int j = 0; j < W.nents; ++j) {
				Entity *r = &W.ents[j];
				if (!r->alive || r->type != E_FISH || r->amount <= 0 || !a->explorada[celda(r->x, r->y, a)]) continue;
				float d = d2(r->x, r->y, dock->x, dock->y);
				if (d < bd) { bd = d; best = j; }
			}
			if (best >= 0) cmd_target(ent_ref(i), ent_ref(best));
		} else if (b->type == E_WARSHIP && (b->state == ST_IDLE || b->state == ST_MOVE)) {
			/* trirremes: atacan lo que ven; si no, exploran el agua sin explorar */
			int en = enemigo_cerca(p, b, 9);
			if (en >= 0) { cmd_target(ent_ref(i), ent_ref(en)); continue; }
			if (b->state != ST_IDLE) continue;
			/* en un ataque por mar, escoltan a los transportes hacia el objetivo y bombardean la costa */
			if (a->modo == M_ATAQUE && islas() && d2(b->x, b->y, nav_x, nav_y) > 8 * 8) { cmd_move(ent_ref(i), nav_x, nav_y); continue; }
			int best = -1; float bd = 1e18f;
			for (int j = 0; j < a->ch; ++j)
				for (int k = 0; k < a->cw; ++k) {
					float px = k * CELDA + CELDA * 0.5f, py = j * CELDA + CELDA * 0.5f;
					if (a->explorada[j * a->cw + k] || !tile_sail((int)px, (int)py)) continue;
					float d = d2(px, py, b->x, b->y);
					if (d < bd) { bd = d; best = j * a->cw + k; }
				}
			if (best >= 0) cmd_move(ent_ref(i), (best % a->cw) * CELDA + CELDA * 0.5f, (best / a->cw) * CELDA + CELDA * 0.5f);
			else if (a->modo == M_ATAQUE) cmd_move(ent_ref(i), nav_x, nav_y);
		} else if (b->type == E_TRANSPORT && i != t_colonia && i != t_rescate) {
			int carga = transport_load(b);
			if (b->state != ST_IDLE) continue;
			/* D415: llegó al agua del desembarco con tropa: a tierra (baja al llegar a la orilla) */
			if (carga > 0 && por_mar && a->anf_ok && d2(b->x, b->y, a->anf_wx, a->anf_wy) < 5 * 5) { cmd_move(ent_ref(i), a->anf_lx, a->anf_ly); continue; }
			/* con tropa a bordo y sin ataque (o en el muelle esperando más de 90 s sin zarpar): baja en casa */
			if (carga > 0 && d2(b->x, b->y, dock->x, dock->y) < 16 * 16 && (a->modo != M_ATAQUE || !por_mar) && W.time - a->embarque_t > 90) { cmd_move(ent_ref(i), dock->x, dock->y); continue; }
			/* con tropa lejos del muelle y de la costa de desembarco (se quedó parado): sigue hacia el sitio, o vuelve */
			if (carga > 0 && d2(b->x, b->y, dock->x, dock->y) >= 16 * 16) { if (por_mar && a->anf_ok && a->modo == M_ATAQUE) cmd_move(ent_ref(i), a->anf_wx, a->anf_wy); else cmd_move(ent_ref(i), dock->x, dock->y); continue; }
			if (a->modo == M_ATAQUE && por_mar) {
				/* embarca a los soldados que siguen en casa y, lleno o sin más que esperar, zarpa hacia el objetivo */
				int subiendo = 0;
				if (d2(b->x, b->y, dock->x, dock->y) >= 16 * 16) { if (!carga) cmd_move(ent_ref(i), dock->x, dock->y); continue; }
				for (int j = 0; j < W.nents && carga + subiendo < 5; ++j) {
					Entity *u = &W.ents[j];
					if (!vivo(u) || u->owner != p || !militar(u->type)) continue;
					if (u->state == ST_BOARD) { ++subiendo; continue; }
					if (en_barco(u) || d2(u->x, u->y, a->reunion_x, a->reunion_y) > 14 * 14) continue;
					cmd_target(ent_ref(j), ent_ref(i));
					++subiendo;
				}
				if (carga > 0 && zarpar) cmd_move(ent_ref(i), nav_x, nav_y);
			} else if (carga == 0 && d2(b->x, b->y, dock->x, dock->y) > 10 * 10) cmd_move(ent_ref(i), dock->x, dock->y);
		}
	}
}

/* ---------- comercio: barcos y carros mercantes ---------- */

/* Normal y más: con un muelle o mercado propio y otro ajeno conocido, unos pocos mercantes en modo oro hacia el puerto ajeno
 * más lejano que no sea enemigo (si no hay, el enemigo más cercano). Con el comercio mejorado, si le sobra un recurso y le
 * falta otro, uno de ellos pasa al modo intercambio. */
static void mercantes(int p, Censo *c) {
	Player *pl = &W.pl[p];
	if (pl->diff == 0 || c->aldeanos < 14) return;
	static const int tipos[2] = { E_TRADESHIP, E_TRADECART }, puertos[2] = { E_DOCK, E_MARKET };
	for (int k = 0; k < 2; ++k) {
		int tipo = tipos[k], puerto = puertos[k];
		if (pl->age < ud(tipo)->age) continue;
		/* puerto ajeno: el más lejano no enemigo, o el enemigo más cercano a la base */
		int dest = -1; float mejor = -1; int enemigo = 1;
		for (int i = 0; i < W.nents; ++i) {
			Entity *b = &W.ents[i];
			if (!vivo(b) || b->type != puerto || b->owner < 0 || b->owner == p || b->progress < 1) continue;
			if (!ia[p].explorada[celda(b->x, b->y, &ia[p])]) continue;
			int en = hostile(p, b->owner);
			float d = d2(b->x, b->y, c->bx, c->by);
			float v = en ? 1e6f - d : 2e6f + d;
			if (v > mejor) { mejor = v; dest = i; enemigo = en; }
		}
		if (dest < 0) continue;
		/* barcos: el puerto ajeno tiene que estar en la misma agua (islas aparte, casi siempre lo está) */
		int quiero = pl->diff >= 2 ? 3 : 2;
		if (enemigo) quiero = 1;
		int tengo = 0;
		Entity *taller = NULL;
		for (int i = 0; i < W.nents; ++i) {
			Entity *b = &W.ents[i];
			if (!vivo(b) || b->owner != p) continue;
			if ((int)b->type == tipo) {
				++tengo;
				if (!ent_get(b->com_destino) && (b->state == ST_IDLE)) cmd_target(ent_ref(i), ent_ref(dest));
				/* intercambio: lo que sobra por lo que falta */
				if (W.setup.comercio_mejorado && tengo == 1) {
					int sobra = -1, falta = -1;
					for (int r = 0; r < RES_COUNT; ++r) { if (pl->res[r] > 900 && (sobra < 0 || pl->res[r] > pl->res[sobra])) sobra = r; if (pl->res[r] < 150 && (falta < 0 || pl->res[r] < pl->res[falta])) falta = r; }
					if (sobra >= 0 && falta >= 0 && sobra != falta) cmd_comercio(ent_ref(i), 1, sobra, falta);
					else if (b->com_modo == 1 && b->carry <= 0) cmd_comercio(ent_ref(i), 0, b->com_dar, b->com_recibir);
				}
			} else if ((int)b->type == puerto && b->progress >= 1 && b->ntrain == 0 && !taller) taller = b;
		}
		if (tengo < quiero && taller && puede_pagar(p, tipo, 0, 0) && pl->pop < world_pop_cap(p)) cmd_train(p, ent_ref((int)(taller - W.ents)), (EntType)tipo);
	}
}

/* ---------- diplomacia con el humano (D415) ---------- */

/* Lo que cada jugador de la máquina recuerda de los demás (sobre todo del humano): lo recibido en tributos (`conf`, en
 * recursos) y las traiciones (romper un pacto). Se olvida con cada partida nueva. */
typedef struct {
	unsigned gen;
	float conf[MAX_PLAYERS], traicion_t[MAX_PLAYERS], prop_t[MAX_PLAYERS], pide_t[MAX_PLAYERS], queja_t[MAX_PLAYERS];
	int traiciones[MAX_PLAYERS];
} Dip;
static Dip dip[MAX_PLAYERS];

static Dip *dipm(int p) {
	Dip *d = &dip[p];
	if (d->gen != world_gen() + 1) {
		memset(d, 0, sizeof *d);
		d->gen = world_gen() + 1;
		for (int q = 0; q < MAX_PLAYERS; ++q) d->traicion_t[q] = -1e9f;
	}
	return d;
}

/* personalidad fija de cada jugador de la máquina: 0 belicosa, 1 equilibrada, 2 pacífica */
int ia_personalidad(int p) { return (int)(((unsigned)W.setup.seed * 7u + (unsigned)p * 5u + (unsigned)W.pl[p].civ * 3u) % 3u); }

/* un tercero vivo que es enemigo de los dos y más fuerte que `p` (amenaza común); devuelve su jugador + 1, o 0 */
static int amenaza_comun(int p, int q) {
	int mejor = 0, sp = world_team_score(p);
	for (int r = 0; r < MAX_PLAYERS; ++r) {
		if (r == p || r == q || !W.pl[r].active || !W.pl[r].alive) continue;
		if (W.pl[p].dipl[r] != DIPL_ENEMY || W.pl[q].dipl[r] != DIPL_ENEMY) continue;
		if (world_team_score(r) >= sp) mejor = r + 1;
	}
	return mejor;
}

int ia_responde(int p, int de, int nivel, char *msg, int n) {
	Dip *d = dipm(p);
	Player *pl = &W.pl[p];
	int cur = pl->dipl[de], pers = ia_personalidad(p);
	float sp = (float)MAX(1, world_team_score(p)), sd = (float)MAX(1, world_team_score(de));
	float rel = sd / sp;   /* >1: el que pide es más fuerte que yo */
	int com = amenaza_comun(p, de), trai = W.time - d->traicion_t[de] < 900.0f;
	/* puntuación de la propuesta: personalidad, dificultad, fuerza relativa, amenaza común, tributos recibidos y traiciones */
	static const float dif_v[IA_DIFICULTADES] = { 0.6f, 0.0f, -0.5f, -0.8f };
	float v = (pers - 1) * 1.0f + dif_v[CLAMP(pl->diff, 0, IA_DIFICULTADES - 1)];
	v += CLAMP((rel - 1.0f) * 2.0f, -1.5f, 1.5f);
	if (com) v += 1.5f;
	v += MIN(2.5f, d->conf[de] / 200.0f);
	v -= trai ? 4.0f : MIN(2, d->traiciones[de]) * 0.7f;
	int paz = nivel == DIPL_NEUTRAL;
	float need = paz ? 0.4f : 2.0f;
	const char *r = NULL; int ok = 0;
	if (nivel == DIPL_ALLY && cur != DIPL_NEUTRAL) r = "Primero hagamos las paces.";
	else if (trai) r = "Me traicionaste hace poco: no me fío de ti.";
	else if (nivel == DIPL_ALLY && pl->diff >= 2 && !(com && d->conf[de] > 0)) r = "No me alío sin un motivo: un enemigo común y un gesto tuyo.";
	else if (v < need) {
		if (rel < 0.7f) r = paz ? "Eres demasiado débil para pedirme la paz." : "No necesito aliados tan débiles.";
		else if (pers == 0) r = "Prefiero la guerra.";
		else r = "Todavía no me fío: envíame un tributo.";
	} else {
		ok = 1;
		if (com) r = paz ? "Tenemos un enemigo común: acepto la paz." : "Contra nuestro enemigo común, acepto la alianza.";
		else if (d->conf[de] >= 200) r = paz ? "Tu tributo cuenta: acepto la paz." : "Tus regalos cuentan: acepto la alianza.";
		else if (rel > 1.0f) r = paz ? "Eres fuerte: acepto la paz." : "Eres un buen aliado: acepto.";
		else r = paz ? "Acepto la paz." : "Acepto la alianza.";
	}
	snprintf(msg, (size_t)n, "%s", r);
	return ok;
}

void ia_tributo(int p, int de, int kind, int cantidad) {
	Dip *d = dipm(p);
	(void)kind;
	d->conf[de] += (float)cantidad;
	if (de == W.human) world_chat(p, "Gracias por el tributo: lo recordaré.");
}

void ia_traicion(int p, int de) {
	Dip *d = dipm(p);
	d->traicion_t[de] = W.time; ++d->traiciones[de]; d->conf[de] = 0;
	if (de == W.human) world_chat(p, "¡Traidor! No olvidaré esto.");
}

int ia_pide(int p, int de, int kind, int cantidad, char *msg, int n) {
	Dip *d = dipm(p);
	Player *pl = &W.pl[p];
	int cur = pl->dipl[de], pers = ia_personalidad(p);
	const char *r; int ok = 0;
	int reserva = pers == 0 ? 600 : 300;
	if (cur == DIPL_ENEMY) r = "No doy nada a un enemigo.";
	else if (W.time - d->traicion_t[de] < 900.0f) r = "Después de tu traición, nada.";
	else if (pl->res[kind] < cantidad + reserva) r = "No me sobra tanto.";
	else if (cur == DIPL_NEUTRAL && d->conf[de] < cantidad * 0.5f) r = "Solo a los aliados o a quien me ha enviado tributo.";
	else if (pers == 0 && cur != DIPL_ALLY) r = "Pídeselo a otro.";
	else { r = "Te lo envío; contamos con tu ayuda."; ok = 1; d->conf[de] = MAX(0.0f, d->conf[de] - cantidad * 0.5f); }
	snprintf(msg, (size_t)n, "%s", r);
	return ok;
}

/* La máquina propone al humano (cada 2,5 min, normal y más, pasados 5 min de partida): la paz si es más débil, si hay un
 * enemigo común o si es pacífica; la alianza si hay un enemigo común o ya ha recibido tributos; y pide tributo si le falta un
 * recurso. Si el humano la traicionó hace poco, no propone nada y se lo recuerda. La propuesta se responde en F3. */
static void proponer_al_humano(int p) {
	if (ord_red) return;   /* D461: en red la máquina no hace propuestas a los humanos */
	Player *pl = &W.pl[p];
	Dip *d = dipm(p);
	if (pl->diff == 0 || W.time < 300) return;
	for (int q = 0; q < MAX_PLAYERS; ++q) {
		if (q == p || !W.pl[q].active || !W.pl[q].alive || W.pl[q].ai || (W.prop_nivel[p] && W.time <= W.prop_t[p])) continue;
		if (W.time - d->prop_t[q] < 240.0f) continue;
		d->prop_t[q] = W.time;
		int cur = pl->dipl[q], pers = ia_personalidad(p), com = amenaza_comun(p, q);
		if (W.time - d->traicion_t[q] < 900.0f) {
			if (W.time - d->queja_t[q] > 300.0f) { world_chat(p, "No olvido tu traición: no habrá paz."); d->queja_t[q] = W.time; }
			continue;
		}
		float sp = (float)world_team_score(p), sq = (float)world_team_score(q);
		if (cur == DIPL_ENEMY && (sp < sq * 0.7f || com || (pers == 2 && sp < sq * 1.5f))) {
			world_proponer(p, DIPL_NEUTRAL);
			world_chat(p, com ? "Tenemos un enemigo común: ¿hacemos las paces? (F3)" : "Propongo la paz. Respóndeme en la diplomacia (F3).");
		} else if (cur == DIPL_NEUTRAL && pers != 0 && ((com && pl->diff <= 1) || d->conf[q] >= 200.0f)) {
			world_proponer(p, DIPL_ALLY);
			world_chat(p, "Te propongo una alianza. Respóndeme en la diplomacia (F3).");
		} else if (cur != DIPL_ENEMY && W.time - d->pide_t[q] > 300.0f) {
			static const char *rn[RES_COUNT] = { "madera", "comida", "oro", "piedra" };
			for (int k = 0; k < RES_COUNT; ++k)
				if (pl->res[k] < 60 && W.pl[q].res[k] > 800) {
					char m[112]; snprintf(m, sizeof m, "Me falta %s: ¿puedes enviarme algo?", rn[k]);
					world_chat(p, m); d->pide_t[q] = W.time; break;
				}
		}
	}
}

/* ---------- diplomacia entre jugadores de la máquina ---------- */

/* Cada dos minutos (normal y más): si hay un líder claro entre los rivales, el jugador busca la paz y luego la alianza con otro
 * rival de la máquina que tampoco es el líder (cmd_diplomacy decide si el otro acepta). Con mercado, envía lo que le sobra a un
 * aliado que va escaso. A los humanos no les propone nada: eso es cosa de la interfaz. */
static void diplomacia(int p) {
	Ia *a = &ia[p];
	Player *pl = &W.pl[p];
	/* sin enemigos vivos, las paces y alianzas hechas durante la partida se rompen: si no, la partida no acabaría nunca
	 * (los equipos del menú, W.setup.team, se respetan siempre; con la victoria compartida, también las alianzas) */
	int vivos_enemigos = 0;
	for (int q = 0; q < MAX_PLAYERS; ++q)
		if (q != p && W.pl[q].active && W.pl[q].alive && pl->dipl[q] == DIPL_ENEMY) ++vivos_enemigos;
	if (!vivos_enemigos)
		for (int q = 0; q < MAX_PLAYERS; ++q)
			/* con la victoria compartida las alianzas se respetan (los aliados ganan juntos): solo se rompen las paces */
			if (q != p && W.pl[q].active && W.pl[q].alive && pl->dipl[q] != DIPL_ENEMY && W.setup.team[q] != W.setup.team[p] &&
			    !(W.setup.victoria_compartida && pl->dipl[q] == DIPL_ALLY)) {
				cmd_diplomacy(p, q, DIPL_ENEMY);
				snprintf(a->ultimo, sizeof a->ultimo, "declara la guerra al jugador %d", q + 1);
			}
	proponer_al_humano(p);
	if (pl->diff == 0 || W.time - a->diplo_t < 120) return;
	a->diplo_t = W.time;
	int lider = -1, enemigos = 0, mejor = -1;
	for (int q = 0; q < MAX_PLAYERS; ++q) {
		if (q == p || !W.pl[q].active || !W.pl[q].alive) continue;
		if (pl->dipl[q] == DIPL_ENEMY) ++enemigos;
		if (lider < 0 || world_team_score(q) > mejor) { mejor = world_team_score(q); lider = q; }
	}
	int yo = world_team_score(p);
	if (enemigos >= 2 && lider >= 0 && pl->dipl[lider] == DIPL_ENEMY && mejor > yo * 3 / 2) {
		for (int q = 0; q < MAX_PLAYERS; ++q) {
			if (q == p || q == lider || !W.pl[q].active || !W.pl[q].alive || !W.pl[q].ai || pl->dipl[q] == DIPL_ALLY) continue;
			if (mejor < world_team_score(q) * 3 / 2) continue;
			if (cmd_diplomacy(p, q, pl->dipl[q] + 1)) { snprintf(a->ultimo, sizeof a->ultimo, "%s con el jugador %d", dipl_name(pl->dipl[q]), q + 1); break; }
		}
	}
	/* tributo a un aliado escaso */
	for (int q = 0; q < MAX_PLAYERS; ++q) {
		if (q == p || !W.pl[q].active || !W.pl[q].alive || pl->dipl[q] != DIPL_ALLY) continue;
		for (int k = 0; k < RES_COUNT; ++k)
			if (pl->res[k] > 1500 && W.pl[q].res[k] < 150) cmd_tribute(p, q, k, 300);
	}
}

/* ---------- vuelta de un jugador ---------- */

static int toca_diplomacia[MAX_PLAYERS];   /* llegó a la diplomacia en esta vuelta: se hace en ia_aplicar (D393) */

static void ia_pensar(int p) {
	Player *pl = &W.pl[p];
	const Nivel *nv = &niveles[pl->diff];
	Censo c;
	componentes();
	ia_ver(p);
	censar(p, &c);

	if (!c.tc) {
		ejercito(p, &c);
		repartir(p, &c, (int[RES_COUNT]){ 0 });
		/* sin centro urbano, los barcos de guerra que queden también van a por el rival */
		for (int i = 0; i < W.nents; ++i) {
			Entity *b = &W.ents[i];
			if (!vivo(b) || b->owner != p || b->type != E_WARSHIP || b->state != ST_IDLE) continue;
			int en = enemigo_cerca(p, b, 9);
			if (en >= 0) cmd_target(ent_ref(i), ent_ref(en));
			else if (elegir_objetivo(p, b->x, b->y)) cmd_move(ent_ref(i), ia[p].obj_x, ia[p].obj_y);
		}
		return;
	}

	int age = pl->age;
	/* ahorro para la edad siguiente */
	int reserva[RES_COUNT] = { 0 };
	int avanza = age + 1 < AGE_COUNT && pl->age_t < 0 && c.aldeanos >= nv->edad_aldeanos[age + 1] * 3 / 4 && W.time >= nv->edad_min[age + 1] * 60;
	if (avanza) { reserva[RES_FOOD] = age_def[age + 1].food; reserva[RES_GOLD] = age_def[age + 1].gold; }
	/* si no hay quien recoja oro (ni se conoce), no se congela todo esperándolo */
	if (avanza && pl->res[RES_GOLD] < reserva[RES_GOLD] && c.tarea[RES_GOLD] == 0 && recurso_cercano(p, RES_GOLD, c.bx, c.by, 200) < 0) { reserva[RES_GOLD] = 0; reserva[RES_FOOD] /= 3; }
	if (pl->res[RES_WOOD] < 150) reserva[RES_WOOD] = 150;
	if (nv->torres && age >= AGE_TOOL) reserva[RES_STONE] = 100;
	/* ahorro para la Maravilla (victoria por Maravilla) */
	if (W.setup.victory == VIC_WONDER && pl->diff >= 1 && age + 1 >= AGE_COUNT && !c.n[E_WONDER]) {
		reserva[RES_WOOD] = MAX(reserva[RES_WOOD], bld_def[E_WONDER].wood);
		reserva[RES_GOLD] = MAX(reserva[RES_GOLD], bld_def[E_WONDER].gold);
		reserva[RES_STONE] = MAX(reserva[RES_STONE], bld_def[E_WONDER].stone);
	}

	/* aldeanos: sin parar hasta el objetivo de la edad (la reserva de la edad solo frena cuando ya hay bastantes) */
	int objetivo = MIN(nv->aldeanos + age * nv->aldeanos_edad, nv->aldeanos_max);
	/* como mucho la mitad de la población máxima de la partida: el resto, para el ejército (y la flota) */
	objetivo = MIN(objetivo, MAX(12, (W.setup.popcap > 0 ? W.setup.popcap : 200) / 2));
	Ref tcr = ent_ref((int)(c.tc - W.ents));
	int cola = pl->diff >= 2 ? 2 : 1;
	int freno = avanza && c.aldeanos >= nv->edad_aldeanos[age + 1] ? reserva[RES_FOOD] : 0;
	if (c.aldeanos + c.tc->ntrain < objetivo && c.tc->ntrain < cola && pl->pop + c.tc->ntrain < world_pop_cap(p) && pl->res[RES_FOOD] >= ud(E_VILLAGER)->food + freno)
		cmd_train(p, tcr, E_VILLAGER);

	/* edad */
	if (avanza && c.aldeanos >= nv->edad_aldeanos[age + 1] && age_requirements_met(p, age + 1)) {
		if (cmd_advance_age(p)) snprintf(ia[p].ultimo, sizeof ia[p].ultimo, "avanza a %s", age_def[age + 1].name);
	}

	repartir(p, &c, reserva);
	cazar(p, &c);
	edificios(p, &c);
	investigar(p, &c, reserva[RES_FOOD], reserva[RES_GOLD]);
	comerciar(p, reserva);
	producir(p, &c, avanza ? reserva[RES_FOOD] : 0, avanza ? reserva[RES_GOLD] : 0);
	explorar(p, &c);
	barcos(p, &c);
	mercantes(p, &c);
	reliquias(p, &c);
	toca_diplomacia[p] = 1;   /* la diplomacia, después y en el hilo principal (ia_aplicar): toca a otros jugadores */
	ejercito(p, &c);

	/* muy difícil: recoge más deprisa, +0,22 por segundo y recolector (ventaja declarada en el menú; medida con tests/aoen_ia.c:
	 * con +20 % de visión y este bono gana a la difícil 14 de 18 partidas terminadas) */
	if (nv->trampa)
		for (int k = 0; k < RES_COUNT; ++k) pl->res[k] += (int)(c.tarea[k] * nv->pensar * 0.22f + 0.5f);
}

/* D393: la vuelta de cada jugador que piensa en este tic, repartida entre los hilos. Cada IA solo escribe lo suyo (su
 * memoria, sus recursos, las órdenes de sus unidades, sus obras y pedidos de caminos apuntados) y lee el mundo, que
 * nadie más cambia mientras tanto. */
static int quien[MAX_PLAYERS], n_quien;
static void pensar_parte(void *ctx, int k, int partes) { (void)ctx; (void)partes; ia_pensar(quien[k]); }

/* lo que toca a otros jugadores o al mapa, en el hilo principal y en orden de jugador */
static void ia_aplicar(int p) {
	poner_obras(p);
	if (toca_diplomacia[p]) { toca_diplomacia[p] = 0; diplomacia(p); }
}

void ia_actualizar(float dt) {
	n_quien = 0;
	for (int p = 0; p < MAX_PLAYERS; ++p) {
		Player *pl = &W.pl[p];
		if (!pl->active || !pl->ai) continue;
		Ia *a = &ia[p];
		/* partida nueva o cargada: la memoria se rehace */
		if (!a->iniciado || a->gen != world_gen() || W.time + 1.0f < a->ultimo_t || a->cw != (W.w + CELDA - 1) / CELDA || a->ch != (W.h + CELDA - 1) / CELDA) ia_reiniciar(p);
		a->ultimo_t = W.time;
		pl->ai_t -= dt;
		if (pl->ai_t <= 0) {
			if (pl->diff < 0 || pl->diff >= IA_DIFICULTADES) pl->diff = 1;
			pl->ai_t = niveles[pl->diff].pensar;
			obras[p].n = 0;
			quien[n_quien++] = p;
		}
	}
	if (!n_quien) return;
	/* lo que se calcula una vez y luego solo se lee: las islas y la población máxima de este tic */
	componentes();
	world_pop_cap(0);
	/* AOEN_IA_ORDEN=inverso (pruebas, con un hilo): las IA piensan en orden inverso; si alguna leyera lo que otra cambia
	 * mientras piensan, la partida saldría distinta */
	static int inverso = -1;
	if (inverso < 0) inverso = getenv("AOEN_IA_ORDEN") && !strcmp(getenv("AOEN_IA_ORDEN"), "inverso");
	if (inverso)
		for (int i = 0, j = n_quien - 1; i < j; ++i, --j) { int t = quien[i]; quien[i] = quien[j]; quien[j] = t; }
	caminos_aplazar(1);
	hilos_repartir(pensar_parte, NULL, n_quien);
	if (inverso)
		for (int i = 0, j = n_quien - 1; i < j; ++i, --j) { int t = quien[i]; quien[i] = quien[j]; quien[j] = t; }
	for (int k = 0; k < n_quien; ++k) ia_aplicar(quien[k]);
	caminos_resolver();
}

const char *ia_estado(int p) {
	if (p < 0 || p >= MAX_PLAYERS || !ia[p].iniciado) return "";
	static const char *mn[] = { "en paz", "reuniendo el ejército", "atacando", "retirándose" };
	static char b[160];
	snprintf(b, sizeof b, "%s; %s; objetivo %.0f,%.0f", ia[p].amenaza ? "defendiendo" : mn[ia[p].modo], ia[p].ultimo, ia[p].obj_x, ia[p].obj_y);
	return b;
}

void ia_olvidar(void) {
	for (int p = 0; p < MAX_PLAYERS; ++p) ia[p].iniciado = 0;
	comp_gen = 0;
}
