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
	E_TOWN_CENTER, E_BARRACKS, E_HOUSE, E_GRANARY, E_STORAGE, E_FARM, E_ARCHERY, E_STABLE, E_TEMPLE, E_ACADEMY, E_GOVERNMENT, E_TOWER, E_MARKET, E_SIEGE, E_WALL, E_DOCK, E_WONDER,
	/* unidades */
	E_VILLAGER, E_CLUBMAN, E_AXEMAN, E_SHORTSWORD, E_BOWMAN, E_IMPBOW, E_CHARIOT, E_SCOUT, E_CAVALRY, E_HEAVYCAV, E_HOPLITE, E_PRIEST,
	/* máquinas de asedio (Taller de asedio, v8.5.4.7) */
	E_BALLISTA, E_CATAPULT, E_HEAVYCAT,
	/* unidades nuevas: honda, elefantes de guerra y flota (Muelle) */
	E_SLINGER, E_WARELE, E_ELEARCHER, E_FISHBOAT, E_TRANSPORT, E_WARSHIP,
	E_PHALANX, E_CENTURION, E_CATAPHRACT, E_HORSEARCHER,
	/* comercio (v8.5.4.7): barco mercante (Muelle) y carro mercante (Mercado) */
	E_TRADESHIP, E_TRADECART,
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

typedef enum { ST_IDLE, ST_MOVE, ST_GATHER, ST_RETURN, ST_BUILD, ST_ATTACK, ST_DYING, ST_BOARD, ST_ABOARD } EntState;   /* ST_BOARD: va a embarcar; ST_ABOARD: viaja en un transporte */

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
	int nwp; float wpx[4], wpy[4]; /* órdenes en cola con Mayús (puntos de ruta) */
	Ref cargo[5]; int ncargo; int unload;   /* transporte: pasajeros y orden de desembarcar al llegar */
	int patrol; float ptx, pty;    /* patrulla entre el punto de origen (hx,hy) y (ptx,pty) */
	/* comercio: ruta entre un muelle o mercado propio (origen) y otro de otro jugador (destino); fase 0 hacia el origen o
	 * cargando, 1 hacia el destino, 2 de vuelta con la mercancía. Modo 0 oro, 1 intercambio (da com_dar, pide com_recibir;
	 * com_dado es lo que lleva cargado). La carga va en carry. En los aldeanos, com_espera = 1 marca que su comida es carne
	 * de caza (se entrega también en el almacén) */
	Ref com_origen, com_destino;
	int com_modo, com_dar, com_recibir, com_dado, com_fase, com_espera;
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
	int units_lost, bld_lost;   /* estadísticas de la pantalla final */
	int gathered_k[4];          /* recursos entregados, por tipo */
	int dipl[MAX_PLAYERS];      /* relación con cada jugador: DIPL_ENEMY, DIPL_NEUTRAL o DIPL_ALLY (simétrica) */
	int hist_n;                 /* muestras de la gráfica de puntuación (una cada 15 s) */
	unsigned short hist[240];
} Player;

/* relaciones diplomáticas */
enum { DIPL_ENEMY, DIPL_NEUTRAL, DIPL_ALLY };
/* condiciones de victoria */
typedef enum { VIC_STANDARD, VIC_CONQUEST, VIC_WONDER, VIC_RELICS, VIC_SCORE, VIC_COUNT } VictoryKind;

typedef struct {
	int width, height, seed, map_kind, players, villagers;
	int victory;                /* VictoryKind */
	int time_limit;             /* minutos de la victoria por puntuación */
	int relics_total;           /* reliquias repartidas por el mapa (victoria por reliquias) */
	int color[MAX_PLAYERS];     /* color elegido por cada jugador: 0 automático, 1..8 */
	int team[MAX_PLAYERS];
	int start_res[RES_COUNT];
	int explored;
	int popcap;                 /* población máxima por jugador */
	int ai_diff[MAX_PLAYERS];   /* 0 fácil, 1 normal, 2 difícil */
	int civ[MAX_PLAYERS];       /* índice en civ_def */
	int victoria_compartida;    /* 1: la partida acaba cuando los que quedan son del mismo equipo o aliados, y ganan juntos */
	int recursos_mapa;          /* oro, piedra, bayas y bosques del mapa: 0 normales, 1 escasos, 2 abundantes */
	int comercio_mejorado;      /* 1: barcos y carros mercantes con el modo intercambio además del oro */
	int fuego_amigo;            /* D447, formato de partida 6: 1: el daño en área alcanza también a lo propio y lo aliado (0 por defecto) */
} Setup;
/* tamaño de Setup en las partidas guardadas con formato 5 (sin fuego_amigo): se cargan con fuego_amigo apagado */
#define SETUP_TAM_V5 ((int)offsetof(Setup, fuego_amigo))

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
	unsigned ganadores;     /* al acabar: jugadores del bando ganador (bit p), 0 si no se sabe (no se guarda) */
	char msg[96];
	float msg_t;
	struct { float x0, y0, x1, y1, t; int kind; } shots[64]; /* flechas, virotes y piedras (kind: 0 flecha, 1 virote, 2 piedra, 3 piedra de fuego) */
	int nshots;
	struct { float x, y, t; } impactos[24]; /* polvo y explosión donde cae una piedra de catapulta (solo se dibuja) */
	int nimpactos;
	struct { float x, y, t; } pings[8]; /* círculos rojos en el minimapa cuando alguien es atacado */
	int npings;
	float alert_t;
	int human;
	/* espacio de trabajo de búsqueda de caminos */
	float *pf_g;
	int *pf_from, *pf_stamp, pf_cur;
	unsigned rng;
	float relic_t; int relic_team;   /* cuenta atrás de la victoria por reliquias */
	float popwarn_t;
	float hist_t;
	/* D415: chat de la partida (lo que dicen los jugadores de la máquina al humano) y sus propuestas de diplomacia pendientes */
	struct { int de; char texto[112]; } chat[8]; unsigned chat_n;
	int prop_nivel[MAX_PLAYERS]; float prop_t[MAX_PLAYERS];   /* propuesta del jugador p al humano: nivel DIPL_* + 1 (0 = ninguna) y cuándo caduca */
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
	float siege;          /* multiplicador de daño de asedio (0 = 1) */
	float naval;          /* multiplicador de vida de los barcos (0 = 1) */
	unsigned bans;        /* tecnologías que esta civilización no puede investigar (máscara de TechId) */
	float build;          /* velocidad de construcción (0 = 1) */
	int tower;            /* daño extra de las torres */
	float priest;         /* velocidad de conversión de los sacerdotes (0 = 1) */
} Civ;
#define NUM_CIVS 10
extern const Civ civ_def[NUM_CIVS];

typedef enum {
	T_TOOLWORKING, T_BRONZE_AXE, T_MINING, T_DOMESTICATION, T_PLOW, T_LEATHER, T_SCALE,
	T_BRONZE_WEAPONS, T_IRON_WEAPONS, T_BOW, T_COMPOSITE, T_CAVARMOR, T_MEDICINE,
	T_ENGINEERING, T_BALLISTICS, T_ARCHITECTURE,
	T_SENTRY, T_GUARD, T_BALLISTA_T, T_WALL_MED, T_FORTIFICATION, T_WHEEL, T_MONOTHEISM, T_FAITH, T_ASTRONOMY, T_MYSTICISM, T_LOGISTICS, T_COUNT
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
int is_siege(EntType t);
int is_naval(EntType t);                 /* barcos: solo navegan por el agua */
int is_trader(EntType t);                /* barco o carro mercante */
int cmd_comercio(Ref e, int modo, int dar, int recibir);   /* modo de un mercante (el intercambio solo con comercio mejorado) */
int comercio_oro(float d);               /* oro de un viaje de ida y vuelta entre dos puertos o mercados a distancia d */
int comercio_intercambio(int dar, int recibir, float d);  /* lo que se recibe por 15 de «dar» a esa distancia, ahora */
float comercio_distancia(const Entity *e);   /* distancia de su ruta (entre centros), 0 sin ruta */
int tile_sail(int x, int y);             /* casilla de agua libre */
int tech_allowed(int player, int tech);  /* la civilización puede investigarla */
int transport_load(const Entity *boat);   /* pasajeros a bordo */
float unit_cooldown(EntType t);          /* segundos entre golpes */
float unit_splash(EntType t);
float priest_recarga(int owner);       /* segundos de recarga de la fe tras convertir */
float priest_fe(const Entity *e);       /* segundos que le faltan para volver a convertir (0 = lista) */
extern int fuego_amigo;   /* 1: el daño en área de las catapultas alcanza también a lo propio y lo aliado, como en el AoE original (0 por defecto, regla propia de AoEN) */
float unit_min_range(EntType t);         /* alcance mínimo (catapultas): más cerca no pueden disparar */            /* radio del daño de área (casillas), 0 si no tiene */
int unit_tech_req(EntType t);            /* tecnología necesaria para entrenarla, -1 ninguna */
int pcolor(int player);                  /* color (0..7) del jugador */
const char *dipl_name(int level);
int cmd_diplomacy(int from, int to, int level);   /* devuelve 1 si el otro jugador acepta */
int cmd_tribute(int from, int to, int kind, int amount);
int cmd_pedir(int from, int to, int kind, int amount);   /* D415: pide un tributo a otro jugador; si es de la máquina, decide ella (1 si lo envía) */
int cmd_responder_propuesta(int de, int acepta);          /* D415: el humano responde a la propuesta de diplomacia del jugador `de` */
void world_chat(int de, const char *texto);               /* D415: mensaje del jugador `de` al chat de la partida (-1: del sistema) */
void world_proponer(int de, int nivel);                   /* D415: el jugador de la máquina `de` propone al humano un nivel de relación */
void cmd_move_queue(Ref e, float x, float y);     /* añade un punto de ruta (Mayús) */
void cmd_patrol(Ref e, float x, float y);
int world_team_score(int player);
int mismo_bando(int a, int b);   /* ganan juntos: mismo equipo del menú, o aliados si la victoria es compartida */

extern const AgeDef age_def[];
int age_requirements_met(int player, int target_age);
int cmd_advance_age(int player);

extern const BldDef bld_def[];
extern const UnitDef unit_def[];

int is_building(EntType t);
int is_unit(EntType t);
int is_resource(EntType t);

void world_new(const Setup *s);
int world_alloc(int w, int h);
unsigned world_gen(void);       /* cambia con cada mundo nuevo o cargado (para invalidar cachés de la IA y de la interfaz) */   /* reserva los arrays del mapa y el pool de entidades (lo usan la partida nueva y la carga) */

/* partidas guardadas */
typedef struct { long long stamp; int players, w, h, map_kind, age; float time; } SaveInfo;
int world_save(const char *path, long long stamp);
int world_load(const char *path);
int save_info(const char *path, SaveInfo *si);
void world_free(void);
void world_update(float dt);
unsigned world_suma(void);       /* suma del estado del mundo: igual con 1 y con N hilos (D393) */
/* D393: mientras piensan las IA los caminos se apuntan y luego se buscan todos juntos, repartidos entre los hilos */
void caminos_aplazar(int si);
void caminos_resolver(void);

Entity *ent_get(Ref r);
Ref ent_ref(int idx);
int tile_walk(int x, int y);
float frand(void);
float dsinf(float x);                    /* D461: seno, coseno y arco tangente deterministas (misma salida en todas las máquinas) */
float dcosf(float x);
float datan2f(float y, float x);
int irand(int n);

/* órdenes dadas por el humano o la IA */
void cmd_move(Ref e, float x, float y);
int cmd_build_grupo(int player, const Ref *workers, int n, EntType t, int tx, int ty);   /* D461: el primer aldeano que puede levanta el edificio y los demás lo ayudan */
void cmd_eliminar(Ref e);              /* D461: la unidad se elimina (la interfaz ya no toca el mundo) */
void world_invalidar_cachas(void);     /* D461: antes de aplicar las órdenes de un turno */
unsigned world_suma_grupos(unsigned g[16]);   /* D461: suma total y sumas por grupos (para localizar una desincronización) */
int world_volcar(const char *ruta);    /* D461: volcado de texto del estado, para comparar dos máquinas */
extern int ord_red;                    /* D461: 1 con una partida multijugador en marcha (ordenes.c) */
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
int edificio_recoge(EntType b, int kind, int carne);   /* el edificio recoge ese recurso (carne: comida de caza) */
int ent_is_trainer(EntType b);
void world_msg(const char *s);
int world_pop_cap(int player);

#endif
