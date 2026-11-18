/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Mundo del juego de AoEN: mapa, entidades, economía, combate, IA. */
#ifndef AOEN_GAME_H
#define AOEN_GAME_H

#include "aoen.h"

typedef enum { TILE_DESERT, TILE_GRASS, TILE_WATER, TILE_DEEP } TileKind;
typedef enum { MAP_ISLANDS, MAP_CONTINENTS, MAP_COASTAL, MAP_INLAND, MAP_HIGHLAND, MAP_COUNT } MapKind;

typedef enum {
	/* edificios */
	E_TOWN_CENTER, E_BARRACKS, E_HOUSE, E_GRANARY, E_STORAGE, E_FARM, E_ARCHERY, E_STABLE, E_TEMPLE, E_ACADEMY, E_GOVERNMENT, E_TOWER, E_MARKET, E_SIEGE, E_WONDER,
	/* unidades */
	E_VILLAGER, E_CLUBMAN, E_AXEMAN, E_SHORTSWORD, E_BOWMAN, E_IMPBOW, E_CHARIOT, E_SCOUT, E_CAVALRY, E_HEAVYCAV, E_HOPLITE, E_PRIEST,
	/* animales salvajes (unidades sin dueño) */
	E_HORSE, E_ELEPHANT,
	/* recursos */
	E_BERRIES, E_GOLD, E_STONE, E_TREE, E_CARCASS, E_FISH, E_RELIC,
	E_TYPE_COUNT
} EntType;

#define NUM_UNIT_TYPES (E_ELEPHANT - E_VILLAGER + 1)
#define NUM_BLD_TYPES (E_WONDER - E_TOWN_CENTER + 1)

typedef enum { AGE_STONE, AGE_TOOL, AGE_BRONZE, AGE_IRON, AGE_COUNT } Age;

typedef enum { RES_WOOD, RES_FOOD, RES_GOLD, RES_STONE, RES_COUNT } ResKind;

typedef enum { STANCE_AGGRESSIVE, STANCE_DEFENSIVE, STANCE_PASSIVE, STANCE_STAND, STANCE_GUARD } Stance;

typedef enum { ST_IDLE, ST_MOVE, ST_GATHER, ST_RETURN, ST_BUILD, ST_ATTACK, ST_DYING } EntState;

typedef struct { int idx, gen; } Ref;

typedef struct {
	int alive, gen;
	EntType type;
	int owner;            /* jugador 0..7, -1 neutral */
	float x, y;           /* posición en el mundo (unidades de casilla, la casilla i cubre [i,i+1)) */
	float hp, maxhp;
	EntState state;
	Ref target;           /* recurso / edificio / enemigo */
	float tx, ty;         /* objetivo de movimiento */
	int *path;            /* índices de casilla, siguiente punto intermedio en path[pidx] */
	int plen, pidx;
	float angle;          /* orientación, radianes */
	float anim;           /* posición del ciclo de andar en cuadros */
	int walking;          /* se movió en este tick */
	int acting;           /* trabajando / luchando en el sitio (animación de ataque) */
	float phase;          /* 0..1 posición dentro del golpe de trabajo */
	float gacc;           /* recursos recolectados fraccionarios */
	float builders;       /* constructores en el último tick (edificios) */
	float nbuild_now;
	int variant;          /* variante de sprite (árboles, oro) */
	int carry_kind, carry; /* carga del trabajador */
	float timer;          /* enfriamiento de recolección / ataque */
	int stance;           /* Postura */
	int auto_target;      /* objetivo elegido por la propia unidad, no por una orden */
	float hx, hy;         /* posición de origen para la postura defensiva */
	int has_rally; float rx, ry; Ref rtarget; /* punto de reunión de los edificios de producción */
	int relics; float racc; /* templo: reliquias guardadas dentro y fracción de oro */
	int stuck;
	float lx, ly, stuck_t; /* última posición comprobada y tiempo sin avanzar */
	/* edificios */
	int bx, by, size;     /* huella */
	float progress;       /* 0..1 construcción (1 = terminado) */
	int train[5], ntrain; /* cola de entrenamiento (EntType) */
	int tech_cur;         /* tecnología que se investiga aquí más uno, 0 = ninguna */
	float tech_t;
	float train_time;
	/* recursos */
	float amount;
	int sel;              /* seleccionado por el humano */
	float die_t;
} Entity;

typedef struct {
	int active, ai, team;
	int res[RES_COUNT];
	int pop, popcap;
	int alive;
	int age;
	unsigned techs;       /* tecnologías investigadas (máscara de bits) */
	float age_t;          /* progreso de la investigación en segundos, <0 cuando no se investiga */
	/* ia */
	float ai_t;
	int ai_attack_wave;
	int units_killed, bld_razed;
	int gathered;         /* recursos entregados, para la puntuación */
	int civ;
	int diff;             /* dificultad de la IA */
	int price[4];         /* mercado: oro pagado por 100 de cada recurso, en porcentaje del precio base */
} Player;

typedef struct {
	int width, height, seed, map_kind, players, villagers;
	int team[MAX_PLAYERS];
	int start_res[RES_COUNT];
	int explored;
	int popcap;                 /* población máxima por jugador */
	int ai_diff[MAX_PLAYERS];   /* 0 fácil, 1 normal, 2 difícil */
	int civ[MAX_PLAYERS];       /* índice en civ_def */
} Setup;

typedef struct {
	Setup setup;
	int w, h;
	u8 *tile, *variant;     /* por casilla */
	u8 *block;              /* número de obstrucciones (edificios, recursos) */
	u8 *fog;                /* 0 sin explorar, 1 explorada, 2 visible (humano) */
	Entity *ents;
	int nents, capents;
	Player pl[MAX_PLAYERS];
	float time;
	int over;               /* 0 en juego, 1 victoria, 2 derrota */
	char msg[96];
	float msg_t;
	struct { float x0, y0, x1, y1, t; } shots[64]; /* efectos de flechas y virotes */
	int nshots;
	struct { float x, y, t; } pings[8]; /* círculos rojos en el minimapa cuando alguien es atacado */
	int npings;
	float alert_t;
	int human;
	/* espacio de trabajo de búsqueda de caminos */
	float *pf_g;
	int *pf_from, *pf_stamp, pf_cur;
	unsigned rng;
} World;

extern World W;

typedef struct { const char *name; int hp, size, wood, food, gold, stone; float build_time; int age; } BldDef;
typedef struct {
	const char *name;
	int hp, wood, food, gold, stone;
	float train_time, speed, dmg, dmg_bld, range;
	int age;          /* edad necesaria */
	EntType trainer;  /* edificio que lo entrena */
	int armor, parmor; /* armadura cuerpo a cuerpo y perforante */
	int ranged;
} UnitDef;
typedef struct { const char *name; int food, gold; float time; } AgeDef;

/* civilizaciones: cada una tiene sus propias fortalezas */
typedef struct {
	const char *name, *desc;
	float gather[4];      /* velocidad de recolección de madera, comida, oro, piedra */
	float unit_hp;        /* vida de cada unidad */
	float bld_hp;         /* vida de los edificios */
	float train_time;     /* tiempo de entrenamiento (menor es más rápido) */
	float speed;          /* velocidad de andar de los soldados */
	int melee_atk;        /* ataque cuerpo a cuerpo extra */
	int ranged_atk;       /* ataque a distancia extra */
	int range;            /* alcance extra de los arqueros */
	int start_wood, start_food, start_gold; /* recursos iniciales extra */
	int style;            /* aspecto de los edificios: 0 egipcio, 1 griego, 2 asiático, 3 babilonio */
} Civ;
#define NUM_CIVS 10
extern const Civ civ_def[NUM_CIVS];

typedef enum {
	T_TOOLWORKING, T_BRONZE_AXE, T_MINING, T_DOMESTICATION, T_PLOW, T_LEATHER, T_SCALE,
	T_BRONZE_WEAPONS, T_IRON_WEAPONS, T_BOW, T_COMPOSITE, T_CAVARMOR, T_MEDICINE, T_COUNT
} TechId;

typedef struct {
	const char *name, *desc;
	EntType bld;
	int age, wood, food, gold, stone;
	float time;
	int prereq;
} TechDef;

extern const TechDef tech_def[T_COUNT];
int has_tech(int player, int tech);
int cmd_research(int player, Ref bld, int tech);
float eff_attack(int player, EntType t, int vs_building);
int eff_armor(int player, EntType t, int ranged);
float eff_range(int player, EntType t);

extern const AgeDef age_def[];
int age_requirements_met(int player, int target_age);
int cmd_advance_age(int player);

extern const BldDef bld_def[];
extern const UnitDef unit_def[];

int is_building(EntType t);
int is_unit(EntType t);
int is_resource(EntType t);

void world_new(const Setup *s);
int world_alloc(int w, int h);   /* reserva los arrays del mapa y el pool de entidades (lo usan la partida nueva y la carga) */

/* partidas guardadas */
typedef struct { long long stamp; int players, w, h, map_kind, age; float time; } SaveInfo;
int world_save(const char *path, long long stamp);
int world_load(const char *path);
int save_info(const char *path, SaveInfo *si);
void world_free(void);
void world_update(float dt);

Entity *ent_get(Ref r);
Ref ent_ref(int idx);
int tile_walk(int x, int y);
float frand(void);
int irand(int n);

/* órdenes dadas por el humano o la IA */
void cmd_move(Ref e, float x, float y);
void cmd_target(Ref e, Ref target);   /* recolectar / atacar / construir / guarnecer según el objetivo */
int cmd_build(int player, Ref worker, EntType t, int tx, int ty);
int cmd_train(int player, Ref bld, EntType t);
int cmd_cancel_train(int player, Ref bld, int index);
int cmd_trade(int player, int sell_kind, int buy_kind); /* vende 100 de un recurso y compra otro, a través del mercado */
void cmd_set_stance(Ref unit, int stance);
void cmd_guard(Ref unit, Ref protectee);
void cmd_set_rally(Ref building, float x, float y, Ref target);
void world_score(int player, int *econ, int *military, int *tech);
int same_team(int a, int b);
int dev_place(EntType t, int owner, float x, float y); /* ayuda para desarrollo: coloca una entidad terminada en cualquier sitio */ /* quita una unidad de la cola y la reembolsa */
int can_place(EntType t, int tx, int ty);
int ent_is_trainer(EntType b);
void world_msg(const char *s);
int world_pop_cap(int player);

#endif
