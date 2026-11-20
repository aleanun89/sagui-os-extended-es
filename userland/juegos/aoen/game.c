/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Lógica del juego de AoEN: generación del mapa, búsqueda de caminos, unidades, economía, edificios, combate y niebla de guerra. */
#include "game.h"
#include "efectos.h"
#include "ia.h"
#include "hilos.h"

World W;

const BldDef bld_def[] = {
	/* nombre            pv  tamaño madera comida oro piedra construir edad */
	{ "Centro urbano",  600, 3, 200, 0,   0,   0,  60, AGE_STONE },
	{ "Cuartel",        350, 3, 150, 0,   0,   0,  40, AGE_STONE },
	{ "Casa",            75, 2,  30, 0,   0,   0,  20, AGE_STONE },
	{ "Granero",        350, 3, 120, 0,   0,   0,  35, AGE_STONE },
	{ "Almacén",        350, 2, 120, 0,   0,   0,  35, AGE_STONE },
	{ "Granja",         100, 3,  60, 0,   0,   0,  15, AGE_TOOL  },
	{ "Campo de tiro",  350, 3, 150, 0,   0,   0,  40, AGE_TOOL  },
	{ "Establo",        350, 3, 150, 0,   0,   0,  40, AGE_TOOL  },
	{ "Templo",         350, 3, 200, 0,   0,   0,  45, AGE_BRONZE },
	{ "Academia",       350, 3, 200, 0,   0,   0,  45, AGE_BRONZE },
	{ "Centro de gobierno", 400, 3, 175, 0, 100, 0, 50, AGE_BRONZE },
	{ "Torre",          200, 1,  50, 0,   0, 100,  30, AGE_TOOL  },
	{ "Mercado",        300, 3, 175, 0,   0,   0,  40, AGE_BRONZE },
	{ "Taller de asedio",350, 3, 200, 0,   0,   0,  45, AGE_IRON  },
	{ "Muralla",        250, 1,   0, 0,   0,   5,   6, AGE_TOOL  },
	{ "Muelle",         350, 3, 150, 0,   0,   0,  45, AGE_TOOL  },
	{ "Maravilla",     1500, 5, 600, 0, 600, 400, 160, AGE_IRON  },
};

const UnitDef unit_def[] = {
	/* nombre           pv madera comida oro piedra entrenar velocidad dmg dmgb alcance edad        entrenador       arm parm rng */
	{ "Aldeano",        25,  0,  50,   0, 0, 15, 1.4f, 2.0f, 0.5f, 0.8f, AGE_STONE,  E_TOWN_CENTER, 0, 0, 0 },
	{ "Clubman",        40,  0,  50,   0, 0, 20, 1.6f, 5.0f, 2.0f, 0.8f, AGE_STONE,  E_BARRACKS,    0, 0, 0 },
	{ "Hachero",        50,  0,  50,  20, 0, 22, 1.6f, 7.0f, 3.0f, 0.8f, AGE_TOOL,   E_BARRACKS,    0, 1, 0 },
	{ "Espadachín",     60,  0,  35,  15, 0, 22, 1.7f, 9.0f, 4.0f, 0.8f, AGE_BRONZE, E_BARRACKS,    1, 1, 0 },
	{ "Arquero",        30, 20,  40,   0, 0, 24, 1.5f, 4.0f, 0.5f, 6.0f, AGE_TOOL,   E_ARCHERY,     0, 0, 1 },
	{ "Arquero mejorado",35,20,  40,   0, 0, 24, 1.5f, 6.0f, 1.0f, 7.0f, AGE_BRONZE, E_ARCHERY,     0, 1, 1 },
	{ "Carro arquero",  70, 40,  70,   0, 0, 30, 2.3f, 5.0f, 1.0f, 6.0f, AGE_IRON,   E_ARCHERY,     1, 2, 1 },
	{ "Explorador",     60,  0, 100,   0, 0, 30, 2.8f, 3.0f, 1.0f, 0.8f, AGE_TOOL,   E_STABLE,      0, 0, 0 },
	{ "Caballería",    100,  0,  70,  40, 0, 34, 2.4f, 7.0f, 3.0f, 0.8f, AGE_BRONZE, E_STABLE,      1, 1, 0 },
	{ "Caballería pesada",150,0,  80,  50, 0, 40, 2.3f,10.0f, 4.0f, 0.8f, AGE_IRON,   E_STABLE,      2, 2, 0 },
	{ "Hoplita",        80,  0,  60,  40, 0, 28, 1.5f, 6.0f, 3.0f, 0.8f, AGE_BRONZE, E_ACADEMY,     3, 5, 0 },
	{ "Sacerdote",      25,  0,   0, 125, 0, 35, 1.4f, 0.0f, 0.0f, 8.0f, AGE_BRONZE, E_TEMPLE,      0, 0, 0 },
	{ "Balista",        75, 100,   0,  80, 0, 36, 1.2f,14.0f, 3.0f, 8.0f, AGE_IRON,   E_SIEGE,       0, 5, 1 },
	{ "Catapulta",      90, 150,   0, 100, 0, 45, 0.9f, 8.0f,40.0f, 9.0f, AGE_IRON,   E_SIEGE,       0, 8, 1 },
	{ "Catapulta pesada",120,200,  0, 150, 0, 55, 0.8f,10.0f,60.0f,11.0f, AGE_IRON,   E_SIEGE,       0,10, 1 },
	{ "Lanzador de honda",30, 0,  30,   0,10, 22, 1.5f, 5.0f, 0.5f, 5.0f, AGE_BRONZE, E_BARRACKS,     0, 0, 1 },
	{ "Elefante de guerra",220,0,100,  70, 0, 45, 1.9f,12.0f, 5.0f, 0.9f, AGE_IRON,   E_STABLE,      1, 4, 0 },
	{ "Arquero de elefante",160,0, 90,  80, 0, 45, 1.9f, 7.0f, 1.0f, 7.0f, AGE_IRON,  E_ARCHERY,      1, 3, 1 },
	{ "Barco de pesca", 45, 75,   0,   0, 0, 25, 2.0f, 0.0f, 0.0f, 0.8f, AGE_TOOL,   E_DOCK,        0, 0, 0 },
	{ "Barco de transporte",100,125,0,  0, 0, 35, 1.8f, 0.0f, 0.0f, 0.8f, AGE_TOOL,   E_DOCK,        0, 2, 0 },
	{ "Trirreme",      120, 100,   0,  50, 0, 40, 2.0f, 8.0f, 6.0f, 6.5f, AGE_BRONZE, E_DOCK,        0, 4, 1 },
	{ "Falange",       120,   0,  70,  50, 0, 32, 1.3f, 7.0f, 3.0f, 0.8f, AGE_IRON,   E_ACADEMY,     5, 8, 0 },
	{ "Centurión",     110,   0,  80,  60, 0, 30, 1.6f,11.0f, 4.0f, 0.8f, AGE_IRON,   E_ACADEMY,     3, 3, 0 },
	{ "Catafracta",    190,   0, 110,  90, 0, 42, 2.1f,11.0f, 4.0f, 0.8f, AGE_IRON,   E_STABLE,      4, 6, 0 },
	{ "Arquero a caballo",60, 0,  50,  70, 0, 34, 2.6f, 5.0f, 1.0f, 6.0f, AGE_BRONZE, E_ARCHERY,      0, 0, 1 },
	{ "Barco mercante", 100,100,   0,   0, 0, 35, 2.0f, 0.0f, 0.0f, 0.8f, AGE_TOOL,   E_DOCK,        0, 2, 0 },
	{ "Carro mercante",  70, 80,  60,   0, 0, 40, 1.8f, 0.0f, 0.0f, 0.8f, AGE_BRONZE, E_MARKET,      0, 1, 0 },
	{ "Caballo salvaje",30,  0,   0,   0, 0,  0, 3.2f, 0.0f, 0.0f, 0.8f, AGE_STONE,  E_TYPE_COUNT,  0, 0, 0 },
	{ "Elefante salvaje",200,0,   0,   0, 0,  0, 1.8f, 8.0f, 2.0f, 0.9f, AGE_STONE,  E_TYPE_COUNT,  1, 3, 0 },
};

/* Cada mejora se investiga donde en el Age of Empires original (v8.5.4.10r2, tabla en docs/aoen.md): armas y armaduras en el
 * Almacén, torres y murallas en el Granero, Ingeniería y Balística en el Centro de gobierno. Las de recolección siguen en el
 * Almacén y el Granero (en el original, en el Mercado, que aquí llega en la Edad del Bronce) */
const TechDef tech_def[T_COUNT] = {
	/* nombre                      efecto                                     edificio    edad        madera comida oro piedra tiempo requisito */
	{ "Herramientas",            "Madera +25%",                             E_STORAGE,  AGE_TOOL,   60, 120,  0, 0, 30, -1 },
	{ "Hachas de bronce",        "Madera +25% más",                         E_STORAGE,  AGE_BRONZE, 100, 200, 50, 0, 40, T_TOOLWORKING },
	{ "Minería",                 "Oro y piedra +25%",                       E_STORAGE,  AGE_TOOL,   70, 100,  0, 0, 30, -1 },
	{ "Domesticación",           "Granjas y bayas +25%",                    E_GRANARY,  AGE_TOOL,   50, 200,  0, 0, 30, -1 },
	{ "Arado",                   "Granjas y bayas +25% más",                E_GRANARY,  AGE_BRONZE, 100, 250, 0, 0, 40, T_DOMESTICATION },
	{ "Armadura de cuero",       "Armadura +1 a todas las unidades",        E_STORAGE,  AGE_TOOL,   0, 100, 80, 0, 35, -1 },
	{ "Armadura de escamas",     "Armadura +1 más",                         E_STORAGE,  AGE_BRONZE, 0, 150, 120, 0, 45, T_LEATHER },
	{ "Armas de bronce",         "Ataque cuerpo a cuerpo +2",               E_STORAGE, AGE_TOOL,   0, 100, 80, 0, 35, -1 },
	{ "Armas de hierro",         "Ataque cuerpo a cuerpo +2 más",           E_STORAGE, AGE_BRONZE, 0, 200, 160, 0, 45, T_BRONZE_WEAPONS },
	{ "Flechas mejoradas",       "Ataque a distancia +1",                   E_ARCHERY,  AGE_TOOL,   60, 100, 0, 0, 35, -1 },
	{ "Arcos compuestos",        "Ataque a distancia +1 y alcance +1",      E_ARCHERY,  AGE_BRONZE, 100, 200, 0, 0, 45, T_BOW },
	{ "Armadura de caballería",  "Caballería: armadura +1 y +1 a flechas",  E_STORAGE,   AGE_TOOL,   0, 150, 100, 0, 40, -1 },
	{ "Medicina",                "Los sacerdotes curan el doble de rápido", E_TEMPLE,   AGE_BRONZE, 0, 0, 200, 0, 40, -1 },
	{ "Ingeniería",              "Asedio: alcance +1; permite la catapulta pesada", E_GOVERNMENT, AGE_IRON, 150, 0, 150, 0, 45, -1 },
	{ "Balística",               "Asedio: ataque +25%",                     E_GOVERNMENT,    AGE_IRON,   100, 0, 175, 0, 45, -1 },
	{ "Arquitectura",            "Edificios: reciben un 15% menos de daño", E_GOVERNMENT, AGE_BRONZE, 150, 0, 120, 0, 45, -1 },
	{ "Torre centinela",         "Torres: ataque +3",                       E_GRANARY,    AGE_BRONZE, 0, 0, 80, 100, 35, -1 },
	{ "Torre de guardia",        "Torres: ataque +3 y alcance +2",          E_GRANARY,    AGE_IRON,   0, 0, 150, 150, 45, T_SENTRY },
	{ "Torres de ballesta",      "Torres: disparan un 50% más rápido",      E_GRANARY,    AGE_IRON,   0, 0, 200, 150, 50, T_GUARD },
	{ "Muralla media",           "Murallas: resisten el doble",             E_GRANARY, AGE_BRONZE, 0, 0, 0, 150, 40, -1 },
	{ "Fortificación",           "Murallas: resisten el triple",            E_GRANARY, AGE_IRON,  0, 0, 0, 300, 50, T_WALL_MED },
	{ "Rueda",                   "Aldeanos y caballería: velocidad +10%",   E_MARKET,   AGE_BRONZE, 100, 0, 80, 0, 40, -1 },
	{ "Monoteísmo",              "Sacerdotes: convierten el doble de rápido", E_TEMPLE, AGE_IRON,   0, 0, 300, 0, 50, -1 },
	{ "Fe",                      "Sacerdotes: la conversión funciona el 90%", E_TEMPLE, AGE_BRONZE, 0, 0, 150, 0, 40, -1 },
	{ "Astronomía",              "Todas las unidades y edificios ven +2",   E_MARKET,   AGE_BRONZE, 0, 0, 120, 0, 40, -1 },
	{ "Mística",                 "Sacerdotes: curan un 50% más",            E_TEMPLE,   AGE_IRON,   0, 0, 200, 0, 45, T_FAITH },
	{ "Logística",               "Población máxima +20",                    E_GOVERNMENT, AGE_IRON, 0, 200, 150, 0, 45, -1 },
};

const Civ civ_def[NUM_CIVS] = {
	/* nombre      descripción                                              madera comida oro piedra  uhp  bhp  entrenar velocidad ma ra rng sw sf sg estilo */
	{ "Egipcios",  "Oro +25%, edificios +15% de vida, sacerdotes convierten más rápido, sin Balística",                      { 1.0f, 1.0f, 1.25f, 1.0f }, 1.0f, 1.15f, 1.0f, 1.0f, 0, 0, 0,  0,  0, 100, 0, 0, 0, 1u << T_BALLISTICS , 0, 0, 1.5f },
	{ "Griegos",   "Entrenan un 15% más rápido, soldados +10% de vida, barcos +10%",     { 1.0f, 1.0f, 1.0f, 1.0f }, 1.1f, 1.0f, 0.85f, 1.0f, 0, 0, 0,  0, 50,   0, 1, 0, 1.1f, 0 },
	{ "Babilonios","Edificios +25% de vida, piedra +20%, asedio +25%, torres +3 de daño",                   { 1.0f, 1.0f, 1.0f, 1.2f }, 1.0f, 1.25f, 1.0f, 1.0f, 0, 0, 0, 50,  0,   0, 3, 1.25f, 0, 0 , 0, 3, 0 },
	{ "Asirios",   "Arqueros con +1 de alcance y +1 de ataque, asedio +15%",             { 1.0f, 1.0f, 1.0f, 1.0f }, 1.0f, 1.0f, 1.0f, 1.0f, 0, 1, 1,  0,  0,   0, 3, 1.15f, 0, 0 },
	{ "Minoicos",  "Comida +20%, empiezan con comida extra, barcos +20%, sin Fortificación",                { 1.0f, 1.2f, 1.0f, 1.0f }, 1.0f, 1.0f, 1.0f, 1.0f, 0, 0, 0,  0,150,   0, 1, 0, 1.2f, 1u << T_FORTIFICATION },
	{ "Hititas",   "Soldados +10% más rápidos y +1 de ataque cuerpo a cuerpo, asedio +10%",{ 1.0f, 1.0f, 1.0f, 1.0f }, 1.0f, 1.0f, 1.0f, 1.1f, 1, 0, 0,  0,  0,   0, 3, 1.1f, 0, 0 },
	{ "Fenicios",  "Madera +20%, empiezan con madera extra, barcos +30% de vida",                { 1.2f, 1.0f, 1.0f, 1.0f }, 1.0f, 1.0f, 1.0f, 1.0f, 0, 0, 0,150,  0,   0, 0, 0, 1.3f, 0 , 1.15f, 0, 0 },
	{ "Persas",    "Todo se recolecta un 8% más rápido, unidades +5% de vida, sin Arquitectura",{ 1.08f, 1.08f, 1.08f, 1.08f }, 1.05f, 1.0f, 1.0f, 1.0f, 0, 0, 0,  0,  0,   0, 0, 0, 0, 1u << T_ARCHITECTURE , 0, 0, 1.25f },
	{ "Shang",     "Madera y comida +10%, construyen un 30% más rápido, sin Balística",                                  { 1.1f, 1.1f, 1.0f, 1.0f }, 1.0f, 1.0f, 1.0f, 1.0f, 0, 0, 0,  0,  0,   0, 2, 0, 0, 1u << T_BALLISTICS , 1.3f, 0, 0 },
	{ "Yamato",    "Infantería +2 de ataque, entrenan un 10% más rápido, sin Arcos compuestos",   { 1.0f, 1.0f, 1.0f, 1.0f }, 1.0f, 1.0f, 0.9f, 1.0f, 2, 0, 0,  0,  0,   0, 2, 0, 1.1f, 1u << T_COMPOSITE },
};

const AgeDef age_def[AGE_COUNT] = {
	{ "Edad de Piedra",       0,    0,  0 },
	{ "Edad de Herramientas", 500,  0, 30 },
	{ "Edad del Bronce",      800, 450, 45 },
	{ "Edad del Hierro",     1000, 800, 60 },
};

#define CARRY_CAP 10
#define GATHER_RATE 1.0f

int is_building(EntType t) { return t >= E_TOWN_CENTER && t <= E_WONDER; }   /* E_WALL y E_DOCK están dentro del rango */
int is_unit(EntType t) { return t >= E_VILLAGER && t <= E_ELEPHANT; }
int is_naval(EntType t) { return t == E_FISHBOAT || t == E_TRANSPORT || t == E_WARSHIP || t == E_TRADESHIP; }
int is_trader(EntType t) { return t == E_TRADESHIP || t == E_TRADECART; }
int is_siege(EntType t) { return t == E_BALLISTA || t == E_CATAPULT || t == E_HEAVYCAT; }
float unit_cooldown(EntType t) { return t == E_BALLISTA ? 3.0f : (t == E_CATAPULT || t == E_HEAVYCAT) ? 4.0f : unit_def[t - E_VILLAGER].ranged ? 2.0f : 1.5f; }
float unit_min_range(EntType t) { return t == E_CATAPULT ? 2.5f : t == E_HEAVYCAT ? 3.0f : 0.0f; }
float unit_splash(EntType t) { return t == E_CATAPULT ? 1.3f : t == E_HEAVYCAT ? 1.8f : 0.0f; }
int unit_tech_req(EntType t) { return t == E_HEAVYCAT ? T_ENGINEERING : -1; }
int pcolor(int p) { int c = (p >= 0 && p < MAX_PLAYERS) ? W.setup.color[p] : 0; return c >= 1 && c <= MAX_PLAYERS ? c - 1 : (p < 0 ? 0 : p & 7); }   /* color 0 = automático (el del número de jugador) */
static int is_animal(EntType t) { return t == E_HORSE || t == E_ELEPHANT; }
int is_resource(EntType t) { return t >= E_BERRIES && t <= E_RELIC; }
/* diplomacia: el equipo de la partida fija las relaciones iniciales; después se pueden cambiar (cmd_diplomacy) */
int same_team(int a, int b) { return a >= 0 && b >= 0 && a < MAX_PLAYERS && b < MAX_PLAYERS && (a == b || W.pl[a].dipl[b] == DIPL_ALLY); }
const char *dipl_name(int level) { return level == DIPL_ALLY ? "Aliado" : level == DIPL_NEUTRAL ? "Neutral" : "Enemigo"; }

/* ---------- aleatorio ---------- */

float frand(void) {
	W.rng ^= W.rng << 13; W.rng ^= W.rng >> 17; W.rng ^= W.rng << 5;
	return (W.rng & 0xffffff) / (float)0x1000000;
}
int irand(int n) { return n <= 0 ? 0 : (int)(frand() * n) % n; }

static unsigned hash2(int x, int y, unsigned seed) {
	unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + seed * 2246822519u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}

static float vnoise(float x, float y, unsigned seed) {
	int xi = (int)floorf(x), yi = (int)floorf(y);
	float fx = x - xi, fy = y - yi;
	fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
	float a = (hash2(xi, yi, seed) & 0xffff) / 65535.0f, b = (hash2(xi + 1, yi, seed) & 0xffff) / 65535.0f;
	float c = (hash2(xi, yi + 1, seed) & 0xffff) / 65535.0f, d = (hash2(xi + 1, yi + 1, seed) & 0xffff) / 65535.0f;
	return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

static float fbm(float x, float y, int oct, unsigned seed) {
	float v = 0, amp = 0.5f, tot = 0;
	for (int i = 0; i < oct; ++i) {
		v += vnoise(x, y, seed + i * 7919u) * amp;
		tot += amp; amp *= 0.5f; x *= 2; y *= 2;
	}
	return v / tot;
}

/* ---------- referencias ---------- */

Ref ent_ref(int idx) { Ref r = { idx, W.ents[idx].gen }; return r; }

Entity *ent_get(Ref r) {
	if (r.idx < 0 || r.idx >= W.nents)
		return NULL;
	Entity *e = &W.ents[r.idx];
	return e->alive && e->gen == r.gen ? e : NULL;
}

static const Ref NOREF = { -1, 0 };

/* ---------- casillas ---------- */

static int in_map(int x, int y) { return x >= 0 && y >= 0 && x < W.w && y < W.h; }
static int tidx(int x, int y) { return y * W.w + x; }

int ent_is_trainer(EntType b) {
	for (int i = 0; i < NUM_UNIT_TYPES; ++i)
		if (unit_def[i].trainer == b) return 1;
	return 0;
}

int tile_walk(int x, int y) {
	if (!in_map(x, y))
		return 0;
	int i = tidx(x, y);
	return !W.block[i] && W.tile[i] < TILE_WATER;
}

int tile_sail(int x, int y) {
	if (!in_map(x, y)) return 0;
	int i = tidx(x, y);
	return !W.block[i] && W.tile[i] >= TILE_WATER;
}

/* el terreno de la casilla es el de la unidad: agua para los barcos, tierra para el resto (sin mirar edificios) */
static int terreno_propio(const Entity *e, int x, int y) {
	if (!in_map(x, y)) return 0;
	return (W.tile[tidx(x, y)] >= TILE_WATER) == (is_naval(e->type) != 0);
}

/* casilla libre por la que puede ir la unidad */
static int casilla_paso(const Entity *e, int x, int y) { return is_naval(e->type) ? tile_sail(x, y) : tile_walk(x, y); }

/* Mueve la unidad a (nx, ny) si no cambia de terreno: ningún barco entra en tierra ni ninguna unidad de tierra en el agua,
 * pase lo que pase con la ruta (tramo final en línea recta, sin ruta, empujones, pasos a un lado). Si el paso entero no
 * vale, prueba a deslizarse por un solo eje. Una unidad que ya está en terreno ajeno (partida antigua) puede moverse para
 * salir. Devuelve 1 si se movió. */
static int mover_a(Entity *e, float nx, float ny) {
	int cx = (int)floorf(e->x), cy = (int)floorf(e->y);
	int ix = (int)floorf(nx), iy = (int)floorf(ny);
	if ((ix == cx && iy == cy) || !terreno_propio(e, cx, cy) || terreno_propio(e, ix, iy)) {
		if (!in_map(ix, iy)) return 0;
		/* en diagonal se exige también una de las dos casillas de lado: sin ella cruzaría una esquina de tierra */
		if (ix != cx && iy != cy && terreno_propio(e, cx, cy) && !terreno_propio(e, ix, cy) && !terreno_propio(e, cx, iy)) return 0;
		e->x = nx; e->y = ny; return 1;
	}
	if (terreno_propio(e, ix, cy)) { e->x = nx; return 1; }
	if (terreno_propio(e, cx, iy)) { e->y = ny; return 1; }
	return 0;
}

int tech_allowed(int p, int tech) { return p < 0 || p >= MAX_PLAYERS || !((civ_def[W.pl[p].civ].bans >> tech) & 1u); }

static void block_rect(int x, int y, int s, int d) {
	for (int j = y; j < y + s; ++j)
		for (int i = x; i < x + s; ++i)
			if (in_map(i, j))
				W.block[tidx(i, j)] += d;
}

void world_msg(const char *s) {
	snprintf(W.msg, sizeof W.msg, "%s", s);
	W.msg_t = 3.0f;
}

/* ---------- gestión de entidades ---------- */

/* el array de entidades nunca se mueve: se dimensiona una vez para el mapa, así los punteros a entidades siguen siendo válidos */
static int ent_alloc(void) {
	for (int i = 0; i < W.nents; ++i)
		if (!W.ents[i].alive)
			return i;
	if (W.nents == W.capents)
		return -1;
	memset(&W.ents[W.nents], 0, sizeof(Entity));
	return W.nents++;
}

static int ent_new(EntType t, int owner, float x, float y) {
	int i = ent_alloc();
	if (i < 0) return -1;
	int gen = W.ents[i].gen + 1;
	Entity *e = &W.ents[i];
	memset(e, 0, sizeof *e);
	e->gen = gen;
	e->alive = 1;
	e->type = t;
	e->owner = owner;
	e->x = x; e->y = y;
	e->target = NOREF;
	e->progress = 1;
	e->stance = STANCE_AGGRESSIVE;
	e->hx = x; e->hy = y;
	e->rtarget = NOREF;
	e->com_origen = e->com_destino = NOREF;
	e->com_dar = RES_FOOD; e->com_recibir = RES_GOLD; e->com_dado = RES_FOOD;
	e->variant = irand(4);
	e->angle = frand() * 6.2831853f;

	if (is_building(t)) {
		e->size = bld_def[t].size;
		e->maxhp = e->hp = (float)bld_def[t].hp * (owner >= 0 ? civ_def[W.pl[owner].civ].bld_hp : 1.0f);
		e->bx = (int)floorf(x - e->size / 2.0f + 0.5f);
		e->by = (int)floorf(y - e->size / 2.0f + 0.5f);
		e->x = e->bx + e->size / 2.0f;
		e->y = e->by + e->size / 2.0f;
		block_rect(e->bx, e->by, e->size, 1);
	} else if (is_unit(t)) {
		e->maxhp = e->hp = (float)unit_def[t - E_VILLAGER].hp * (owner >= 0 && t != E_VILLAGER && t < E_HORSE ? civ_def[W.pl[owner].civ].unit_hp : 1.0f);
		if (owner >= 0 && is_naval(t) && civ_def[W.pl[owner].civ].naval > 0) { e->maxhp *= civ_def[W.pl[owner].civ].naval; e->hp = e->maxhp; }
	} else {
		e->size = 1;
		e->bx = (int)floorf(x); e->by = (int)floorf(y);
		e->x = e->bx + 0.5f; e->y = e->by + 0.5f;
		switch (t) {
		case E_BERRIES: e->amount = 200; break;
		case E_GOLD: e->amount = 400; break;
		case E_STONE: e->amount = 250; break;
		case E_CARCASS: e->amount = 150; break;
		case E_FISH: e->amount = 300; break;
		case E_RELIC: e->amount = 1; break;
		default: e->amount = 75; break;
		}
		e->maxhp = e->hp = 1;
		block_rect(e->bx, e->by, 1, 1);
	}
	return i;
}

static void ent_kill(Entity *e) {
	if (is_building(e->type) && e->progress >= 1 && e->alive) fx_evento(FX_EV_EDIFICIO_DESTRUIDO, e);
	else if (e->type == E_TREE && e->alive) fx_evento(FX_EV_ARBOL_CAE, e);
	if (is_building(e->type) || is_resource(e->type))
		block_rect(e->bx, e->by, e->size, -1);
	free(e->path);
	e->path = NULL;
	e->alive = 0;
}

/* ---------- búsqueda de caminos (A* sobre casillas, 8 vecinos) ---------- */

/* D393: el espacio de trabajo de una búsqueda va aparte (Busca), para poder buscar varios caminos a la vez en varios hilos.
 * La 0 usa los arrays del mundo (W.pf_*) y es la de siempre; las demás son de los hilos de trabajo. */
typedef struct { float f; int i; } HeapItem;
typedef struct {
	float *g; int *from, *stamp, *cur;
	HeapItem *heap; int heap_n, heap_cap;
	int sea;              /* la búsqueda en curso es de un barco: solo casillas de agua */
	int own_cur, n;       /* las de los hilos: su contador y el tamaño de sus arrays */
} Busca;

static Busca busca0;
static Busca busca_hilo[HILOS_MAX];

static void heap_push(Busca *b, float f, int i) {
	if (b->heap_n == b->heap_cap) { b->heap_cap = b->heap_cap ? b->heap_cap * 2 : 4096; b->heap = (HeapItem *)realloc(b->heap, sizeof(HeapItem) * (size_t)b->heap_cap); }
	HeapItem *heap = b->heap;
	int k = b->heap_n++;
	while (k > 0) {
		int p = (k - 1) / 2;
		if (heap[p].f <= f) break;
		heap[k] = heap[p]; k = p;
	}
	heap[k].f = f; heap[k].i = i;
}

static HeapItem heap_pop(Busca *b) {
	HeapItem *heap = b->heap;
	HeapItem top = heap[0], last = heap[--b->heap_n];
	int k = 0, heap_n = b->heap_n;
	for (;;) {
		int c = 2 * k + 1;
		if (c >= heap_n) break;
		if (c + 1 < heap_n && heap[c + 1].f < heap[c].f) ++c;
		if (heap[c].f >= last.f) break;
		heap[k] = heap[c]; k = c;
	}
	if (heap_n > 0) heap[k] = last;
	return top;
}

/* rectángulo objetivo [gx0,gx1]x[gy0,gy1] incluido; las casillas dentro del rectángulo interior (ix0..) no son objetivos válidos */
static int pf_pass(const Busca *b, int x, int y) { return b->sea ? tile_sail(x, y) : tile_walk(x, y); }

static int find_path_en(Busca *b, Entity *e, int gx0, int gy0, int gx1, int gy1, int inner)
{
	int sx = (int)floorf(e->x), sy = (int)floorf(e->y);
	b->sea = is_naval(e->type);
	free(e->path); e->path = NULL; e->plen = e->pidx = 0;
	if (!in_map(sx, sy))
		return 0;

	float *pf_g = b->g; int *pf_from = b->from, *pf_stamp = b->stamp;
	int cur_n = ++*b->cur;
	int start = tidx(sx, sy);
	float gcx = (gx0 + gx1) * 0.5f, gcy = (gy0 + gy1) * 0.5f;
	int best = start; float bestd = 1e9f;
	int expanded = 0, found = -1;

	pf_stamp[start] = cur_n; pf_g[start] = 0; pf_from[start] = -1;
	b->heap_n = 0;
	heap_push(b, 0, start);

	static const int dx[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, dy[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

	while (b->heap_n > 0 && expanded < 120000) {
		HeapItem it = heap_pop(b);
		int cur = it.i, cx = cur % W.w, cy = cur / W.w;
		if (it.f > pf_g[cur] + 4000) continue;
		++expanded;

		int inrect = cx >= gx0 && cx <= gx1 && cy >= gy0 && cy <= gy1;
		if (inrect && !(inner && cx > gx0 && cx < gx1 && cy > gy0 && cy < gy1)) {
			found = cur;
			break;
		}

		float ddx = cx - gcx, ddy = cy - gcy, hd = sqrtf(ddx * ddx + ddy * ddy);
		if (hd < bestd) { bestd = hd; best = cur; }

		for (int k = 0; k < 8; ++k) {
			int nx = cx + dx[k], ny = cy + dy[k];
			int ni = in_map(nx, ny) ? tidx(nx, ny) : -1;
			if (ni < 0) continue;
			/* las casillas objetivo dentro de un edificio están bloqueadas, así que se permite entrar en el anillo objetivo igualmente */
			if (!pf_pass(b, nx, ny)) continue;
			if (k >= 4 && (!pf_pass(b, cx + dx[k], cy) || !pf_pass(b, cx, cy + dy[k]))) continue;

			float ng = pf_g[cur] + (k >= 4 ? 1.4142f : 1.0f);
			if (pf_stamp[ni] != cur_n || ng < pf_g[ni]) {
				pf_stamp[ni] = cur_n; pf_g[ni] = ng; pf_from[ni] = cur;
				float hx = nx - gcx, hy = ny - gcy;
				heap_push(b, ng + sqrtf(hx * hx + hy * hy), ni);
			}
		}
	}

	int goal = found >= 0 ? found : best;
	int n = 0;
	for (int i = goal; i >= 0 && i != start; i = pf_from[i]) ++n;
	if (n == 0)
		return found >= 0;

	e->path = (int *)malloc(sizeof(int) * (size_t)n);
	e->plen = n;
	int k = n - 1;
	for (int i = goal; i >= 0 && i != start; i = pf_from[i])
		e->path[k--] = i;
	e->pidx = 0;
	return found >= 0;
}

/* ---------- caminos aplazados (D393) ----------
 * Mientras piensan las IA (ia.c, en varios hilos) las órdenes no buscan el camino: lo apuntan (uno por entidad, el último
 * que pidió) y al acabar caminos_resolver() los busca todos de una vez, repartidos entre los hilos. Cada camino depende solo
 * del mapa y de su entidad, así que sale igual lo busque quien lo busque. */
typedef struct { int gx0, gy0, gx1, gy1, inner, pendiente; } PedidoCamino;
static PedidoCamino *pedidos;
static int pedidos_cap, caminos_aplazados, *lista_pedidos, lista_cap, n_lista;

void caminos_aplazar(int si) {
	if (si && pedidos_cap < W.capents) {
		free(pedidos);
		pedidos_cap = W.capents;
		pedidos = (PedidoCamino *)calloc((size_t)pedidos_cap, sizeof(PedidoCamino));
		if (!pedidos) pedidos_cap = 0;
	}
	caminos_aplazados = si && pedidos;
}

static int find_path(Entity *e, int gx0, int gy0, int gx1, int gy1, int inner) {
	if (caminos_aplazados) {
		/* lo que haría find_path ahora (sin camino) y el pedido para luego; la entidad es de un solo jugador, así que su
		 * casilla de pedidos solo la escribe el hilo de ese jugador */
		free(e->path); e->path = NULL; e->plen = e->pidx = 0;
		PedidoCamino *pc = &pedidos[e - W.ents];
		pc->gx0 = gx0; pc->gy0 = gy0; pc->gx1 = gx1; pc->gy1 = gy1; pc->inner = inner; pc->pendiente = 1;
		return 0;
	}
	busca0.g = W.pf_g; busca0.from = W.pf_from; busca0.stamp = W.pf_stamp; busca0.cur = &W.pf_cur;
	return find_path_en(&busca0, e, gx0, gy0, gx1, gy1, inner);
}

static void resolver_parte(void *ctx, int k, int partes) {
	(void)ctx;
	Busca *b = k == 0 ? &busca0 : &busca_hilo[k];
	for (int j = k; j < n_lista; j += partes) {
		int i = lista_pedidos[j];
		PedidoCamino *pc = &pedidos[i];
		find_path_en(b, &W.ents[i], pc->gx0, pc->gy0, pc->gx1, pc->gy1, pc->inner);
	}
}

void caminos_resolver(void) {
	caminos_aplazados = 0;
	if (!pedidos) return;
	n_lista = 0;
	int lim = MIN(W.nents, pedidos_cap);
	for (int i = 0; i < lim; ++i) {
		if (!pedidos[i].pendiente) continue;
		pedidos[i].pendiente = 0;
		if (!W.ents[i].alive) continue;
		if (n_lista == lista_cap) { lista_cap = lista_cap ? lista_cap * 2 : 256; lista_pedidos = (int *)realloc(lista_pedidos, sizeof(int) * (size_t)lista_cap); }
		lista_pedidos[n_lista++] = i;
	}
	if (!n_lista) return;
	busca0.g = W.pf_g; busca0.from = W.pf_from; busca0.stamp = W.pf_stamp; busca0.cur = &W.pf_cur;
	int partes = MIN(hilos_n(), n_lista);
	/* los espacios de trabajo de los hilos, del tamaño del mapa (se rehacen si cambia) */
	int n = W.w * W.h;
	for (int k = 1; k < partes; ++k) {
		Busca *b = &busca_hilo[k];
		if (b->n != n) {
			free(b->g); free(b->from); free(b->stamp);
			b->g = (float *)calloc((size_t)n, sizeof(float)); b->from = (int *)calloc((size_t)n, sizeof(int)); b->stamp = (int *)calloc((size_t)n, sizeof(int));
			b->n = n; b->own_cur = 0; b->cur = &b->own_cur;
			if (!b->g || !b->from || !b->stamp) { free(b->g); free(b->from); free(b->stamp); b->g = NULL; b->from = b->stamp = NULL; b->n = 0; partes = k; break; }
		}
	}
	hilos_repartir(resolver_parte, NULL, partes);
}

/* asegura que un objetivo puntual sea transitable: busca la casilla transitable más cercana */
static void nearest_pass(float *x, float *y, int sea) {
	int ix = (int)floorf(*x), iy = (int)floorf(*y);
	if (sea ? tile_sail(ix, iy) : tile_walk(ix, iy)) return;
	for (int r = 1; r < 14; ++r)
		for (int dy = -r; dy <= r; ++dy)
			for (int dx = -r; dx <= r; ++dx)
				if (MAX(abs(dx), abs(dy)) == r && (sea ? tile_sail(ix + dx, iy + dy) : tile_walk(ix + dx, iy + dy))) {
					*x = ix + dx + 0.5f; *y = iy + dy + 0.5f;
					return;
				}
}
static void nearest_walk(float *x, float *y) { nearest_pass(x, y, 0); }

static void goto_point(Entity *e, float x, float y) {
	nearest_pass(&x, &y, is_naval(e->type));
	e->tx = x; e->ty = y;
	int gx = (int)floorf(x), gy = (int)floorf(y);
	find_path(e, gx, gy, gx, gy, 0);
	e->state = ST_MOVE;
	e->stuck = 0;
}

static void ent_rect(const Entity *t, int *x0, int *y0, int *x1, int *y1) {
	*x0 = t->bx - 1; *y0 = t->by - 1;
	*x1 = t->bx + t->size; *y1 = t->by + t->size;
}

/* anda junto a un edificio o recurso */
static void goto_target(Entity *e, const Entity *t, EntState st) {
	int x0, y0, x1, y1;
	ent_rect(t, &x0, &y0, &x1, &y1);
	find_path(e, x0, y0, x1, y1, 1);
	e->state = st;
	e->stuck = 0;
	e->tx = t->x; e->ty = t->y;
}

/* ---------- órdenes ---------- */

static void stop_unit(Entity *e) {
	free(e->path); e->path = NULL; e->plen = e->pidx = 0;
	e->state = ST_IDLE; e->target = NOREF;
}

/* añade un punto de ruta a la unidad (órdenes en cola con Mayús); si está parada, va directa */
void cmd_move_queue(Ref r, float x, float y) {
	Entity *e = ent_get(r);
	x = CLAMP(x, 0.0f, (float)W.w); y = CLAMP(y, 0.0f, (float)W.h);   /* dentro del mapa (lo exige también la carga) */
	if (!e || !is_unit(e->type) || e->state == ST_DYING || e->state == ST_ABOARD) return;
	if (e->state == ST_IDLE && e->nwp == 0) { cmd_move(r, x, y); return; }
	if (e->nwp < 4) { e->wpx[e->nwp] = x; e->wpy[e->nwp] = y; ++e->nwp; }
}

/* patrulla entre la posición actual y el punto dado; se interrumpe para combatir y luego sigue */
void cmd_patrol(Ref r, float x, float y) {
	Entity *e = ent_get(r);
	x = CLAMP(x, 0.0f, (float)W.w); y = CLAMP(y, 0.0f, (float)W.h);
	if (!e || !is_unit(e->type) || e->type == E_VILLAGER || e->state == ST_DYING || e->state == ST_ABOARD) return;
	float ox = e->x, oy = e->y;
	cmd_move(r, x, y);
	e->patrol = 1; e->wpx[0] = ox; e->wpy[0] = oy; e->ptx = x; e->pty = y;
}

void cmd_move(Ref r, float x, float y) {
	Entity *e = ent_get(r);
	x = CLAMP(x, 0.0f, (float)W.w); y = CLAMP(y, 0.0f, (float)W.h);
	/* un pasajero no obedece: su target es el barco, y sin él moriría en el tic siguiente */
	if (!e || !is_unit(e->type) || e->state == ST_DYING || e->state == ST_ABOARD) return;
	e->target = NOREF;
	e->auto_target = 0;
	e->nwp = 0; e->patrol = 0;
	if (is_trader(e->type)) { e->com_destino = NOREF; e->com_fase = 0; e->com_espera = 0; }   /* mover a mano deja la ruta */
	if (e->stance == STANCE_GUARD) e->stance = STANCE_AGGRESSIVE;
	e->hx = x; e->hy = y;
	/* un transporte al que se manda a tierra se acerca a la costa y desembarca allí */
	e->unload = e->type == E_TRANSPORT && e->ncargo > 0 && !tile_sail((int)floorf(x), (int)floorf(y));
	goto_point(e, x, y);
}

/* la comida que lleva un aldeano es carne de caza (los aldeanos no comercian: com_espera, 0 o 1, no se usa para otra cosa
 * en ellos y así la partida guardada no cambia) */
static int lleva_carne(const Entity *u) { return u && u->type == E_VILLAGER && u->carry_kind == RES_FOOD && u->com_espera == 1; }

/* Dónde se entrega cada recurso (como en el original): todo en el centro urbano; bayas, granjas y caza en el granero; madera,
 * oro, piedra y también la carne de caza en el almacén; el pescado de los barcos en el muelle. u es quien lo lleva (puede
 * ser NULL: entonces la comida no se toma por carne) */
static int dropsite_accepts_u(const Entity *b, int kind, const Entity *u) {
	if (!is_building(b->type) || b->progress < 1) return 0;
	if (kind == RES_COUNT) return b->type == E_TEMPLE;
	switch (b->type) {
	case E_TOWN_CENTER: return 1;
	case E_DOCK: return kind == RES_FOOD;
	case E_GRANARY: return kind == RES_FOOD;
	case E_STORAGE: return kind != RES_FOOD || lleva_carne(u);
	default: return 0;
	}
}

/* para la interfaz y las pruebas: el edificio b (terminado) recoge ese recurso; carne = comida de caza */
int edificio_recoge(EntType b, int kind, int carne) {
	Entity f, u;
	memset(&f, 0, sizeof f); memset(&u, 0, sizeof u);
	f.type = b; f.progress = 1;
	u.type = E_VILLAGER; u.carry_kind = kind; u.com_espera = carne ? 1 : 0;
	return dropsite_accepts_u(&f, kind, &u);
}

static int res_kind_of(EntType t) {
	switch (t) {
	case E_RELIC: return RES_COUNT;
	case E_BERRIES: case E_FARM: case E_CARCASS: case E_FISH: return RES_FOOD;
	case E_GOLD: return RES_GOLD;
	case E_STONE: return RES_STONE;
	default: return RES_WOOD;
	}
}

static int find_dropsite(const Entity *u, int kind) {
	int best = -1; float bd = 1e9f;
	for (int i = 0; i < W.nents; ++i) {
		Entity *b = &W.ents[i];
		if (!b->alive || b->owner != u->owner || !dropsite_accepts_u(b, kind, u)) continue;
		if ((u->type == E_FISHBOAT) != (b->type == E_DOCK)) continue;   /* los barcos descargan en muelles; el resto, no */
		float d = (b->x - u->x) * (b->x - u->x) + (b->y - u->y) * (b->y - u->y);
		if (d < bd) { bd = d; best = i; }
	}
	return best;
}

static int hostile(int a, int b) {
	return a >= 0 && b >= 0 && a != b && W.pl[a].dipl[b] == DIPL_ENEMY;
}

static float dist_to_ent(const Entity *u, const Entity *t);

/* ---------- comercio: barcos mercantes (entre muelles) y carros mercantes (entre mercados) ---------- */

/* Fórmulas (docs/aoen.md). d es la distancia en línea recta entre los centros de los dos edificios de la ruta, la misma para
 * barcos y carros (el camino real cambia con lo construido; así el pago de una ruta no varía de un viaje a otro).
 *   modo oro:        oro = 5 + 0,6 · d por viaje de ida y vuelta, como mucho 120 (nadie pierde nada)
 *   modo intercambio: recibido = floor(15 · bonus · tasa), bonus = min(3, 1 + d / 60),
 *                     tasa = clamp(total_pedido / total_dado, 0,25, 4) con lo que TIENEN todos los jugadores vivos ahora
 *                     (total_dado 0 cuenta como 1); al destino no se le quita ni se le da nada */
int comercio_oro(float d) { return CLAMP((int)(5.0f + 0.6f * d), 0, 120); }

int comercio_intercambio(int dar, int recibir, float d) {
	if (dar < 0 || dar >= RES_COUNT || recibir < 0 || recibir >= RES_COUNT) return 0;
	long tp = 0, td = 0;
	for (int p = 0; p < MAX_PLAYERS; ++p) if (W.pl[p].active && W.pl[p].alive) { tp += W.pl[p].res[recibir]; td += W.pl[p].res[dar]; }
	float tasa = CLAMP((float)tp / (float)MAX(1, td), 0.25f, 4.0f), bonus = MIN(3.0f, 1.0f + d / 60.0f);
	return (int)floorf(15.0f * bonus * tasa);
}

static EntType puerto_de(const Entity *e) { return e->type == E_TRADESHIP ? E_DOCK : E_MARKET; }

float comercio_distancia(const Entity *e) {
	Entity *o = ent_get(e->com_origen), *d = ent_get(e->com_destino);
	if (!o || !d) return 0;
	return sqrtf((o->x - d->x) * (o->x - d->x) + (o->y - d->y) * (o->y - d->y));
}

/* el muelle o mercado propio terminado más cercano a un punto */
static int puerto_propio(const Entity *e, float x, float y) {
	int best = -1; float bd = 1e18f;
	for (int i = 0; i < W.nents; ++i) {
		Entity *b = &W.ents[i];
		if (!b->alive || b->state == ST_DYING || b->owner != e->owner || b->type != puerto_de(e) || b->progress < 1) continue;
		float d = (b->x - x) * (b->x - x) + (b->y - y) * (b->y - y);
		if (d < bd) { bd = d; best = i; }
	}
	return best;
}

/* clic derecho de un mercante sobre un edificio: un muelle/mercado de otro jugador es el destino (cualquier jugador); uno
 * propio, el origen. Otra cosa: solo se mueve allí y deja la ruta */
static void comercio_ruta(Entity *e, Entity *t) {
	e->target = NOREF;
	if (!is_building(t->type) || t->type != puerto_de(e) || t->progress < 1 || t->owner < 0) {
		e->com_destino = NOREF; e->com_fase = 0; e->com_espera = 0;
		goto_point(e, t->x, t->y);
		return;
	}
	if (t->owner == e->owner) {
		e->com_origen = ent_ref((int)(t - W.ents));
	} else {
		int o = puerto_propio(e, e->x, e->y);
		if (o < 0) { if (e->owner == W.human) world_msg(e->type == E_TRADESHIP ? "Necesitas un muelle propio" : "Necesitas un mercado propio"); return; }
		if (!ent_get(e->com_origen)) e->com_origen = ent_ref(o);
		e->com_destino = ent_ref((int)(t - W.ents));
		if (e->owner == W.human) world_msg("Ruta comercial fijada");
	}
	/* la ruta empieza en el origen (allí carga en el modo intercambio) */
	if (e->com_fase != 2) e->com_fase = 0;
	e->com_espera = 0; e->timer = 0;
	stop_unit(e);
}

int cmd_comercio(Ref r, int modo, int dar, int recibir) {
	Entity *e = ent_get(r);
	if (!e || !is_trader(e->type) || e->state == ST_DYING) return 0;
	if (modo == 1 && !W.setup.comercio_mejorado) return 0;
	if (modo < 0 || modo > 1 || dar < 0 || dar >= RES_COUNT || recibir < 0 || recibir >= RES_COUNT) return 0;
	if ((modo != e->com_modo || dar != e->com_dar || recibir != e->com_recibir) && (e->carry > 0 || e->com_fase != 0)) {
		/* cambiar el trato a mitad de ruta la reinicia: sin esto se cobraría sin haber cargado (o se perdería la carga
		 * sin aviso). Si cargó 15 y aún no ha salido de su puerto, se le devuelven; si ya salió, se pierden */
		Entity *o = ent_get(e->com_origen);
		if (e->com_modo == 1 && e->carry > 0 && e->com_fase == 1 && e->carry_kind == e->com_dado && o &&
		    dist_to_ent(e, o) <= (e->type == E_TRADESHIP ? 3.2f : 2.2f) && e->owner >= 0)
			W.pl[e->owner].res[e->com_dado] += e->carry;
		e->carry = 0; e->com_fase = 0; e->com_espera = 0; e->timer = 0;
	}
	e->com_modo = modo; e->com_dar = dar; e->com_recibir = recibir;
	return 1;
}

static void actualizar_comercio(Entity *e, float dt) {
	Entity *o = ent_get(e->com_origen), *d = ent_get(e->com_destino);
	if (!d) {
		/* sin ruta, obedece órdenes normales; si la tenía y el puerto ajeno ya no existe, queda libre */
		if (e->com_destino.idx >= 0) {
			e->com_destino = NOREF; e->com_fase = 0; e->com_espera = 0; e->carry = 0;
			if (e->owner == W.human) world_msg("Ruta perdida");
		}
		return;
	}
	if (!o || o->owner != e->owner || o->state == ST_DYING || d->state == ST_DYING || d->owner == e->owner || d->owner < 0) {
		/* se perdió un extremo de la ruta: busca otro origen propio; si no hay o falta el destino, se para */
		int no = d && d->state != ST_DYING && d->owner != e->owner && d->owner >= 0 ? puerto_propio(e, e->x, e->y) : -1;
		if (no >= 0) { e->com_origen = ent_ref(no); o = &W.ents[no]; }
		else { e->com_destino = NOREF; e->com_fase = 0; e->com_espera = 0; if (e->com_modo == 0) e->carry = 0; return; }
	}
	if (e->state != ST_IDLE && e->state != ST_MOVE) return;
	if (W.setup.comercio_mejorado == 0) e->com_modo = 0;
	Entity *meta = e->com_fase == 1 ? d : o;
	float alcance = e->type == E_TRADESHIP ? 3.2f : 2.2f;
	if (dist_to_ent(e, meta) > alcance) {
		if (e->state == ST_IDLE) {
			/* reintenta el camino cada 2 s como mucho (si no lo hay, no se calcula en cada tic) */
			e->timer -= dt;
			if (e->timer <= 0) { e->timer = 2.0f; goto_point(e, meta->x, meta->y); }
		}
		return;
	}
	if (e->state == ST_MOVE) stop_unit(e);
	float dist = comercio_distancia(e);
	Player *pl = &W.pl[e->owner];
	switch (e->com_fase) {
	case 0:   /* en el origen: carga (intercambio) y sale */
		if (e->com_modo == 1) {
			if (e->carry <= 0) {
				if (pl->res[e->com_dar] < 15) {   /* espera en su puerto sin zarpar */
					if (!e->com_espera) AOEN_LOG("[AOEN] comercio: %s espera 15 de %d (tiene %d)", unit_def[e->type - E_VILLAGER].name, e->com_dar, pl->res[e->com_dar]);
					e->com_espera = 1;
					return;
				}
				pl->res[e->com_dar] -= 15;
				e->carry = 15; e->carry_kind = e->com_dar; e->com_dado = e->com_dar;
				AOEN_LOG("[AOEN] comercio: %s carga 15 de %d (jugador %d tiene %d)", unit_def[e->type - E_VILLAGER].name, e->com_dar, e->owner + 1, pl->res[e->com_dar]);
			}
		} else e->carry = 0;
		e->com_espera = 0;
		e->com_fase = 1; e->timer = 0;
		goto_point(e, d->x, d->y);
		break;
	case 1:   /* en el puerto ajeno: cambia la carga (sin tocar al dueño) o recoge el oro, y vuelve */
		if (e->com_modo == 1 && e->carry > 0) e->carry_kind = e->com_recibir;
		else { e->carry = 1; e->carry_kind = RES_GOLD; }
		e->com_fase = 2; e->timer = 0;
		goto_point(e, o->x, o->y);
		break;
	default: {   /* de vuelta: descarga */
		int k, n;
		if (e->com_modo == 1 && e->carry > 0 && e->carry_kind == e->com_recibir) { k = e->com_recibir; n = comercio_intercambio(e->com_dado, k, dist); }
		else { k = RES_GOLD; n = comercio_oro(dist); }
		pl->res[k] += n;
		pl->gathered += n;
		pl->gathered_k[k] += n;
		{
			long tp = 0, td = 0;
			for (int q = 0; q < MAX_PLAYERS; ++q) if (W.pl[q].active && W.pl[q].alive) { tp += W.pl[q].res[k]; td += W.pl[q].res[e->com_dado]; }
			/* los totales son los de antes de sumar lo entregado (los que usó la fórmula) */
			AOEN_LOG("[AOEN] comercio: %s entrega %d de %d (modo %d, distancia %.1f, total_pedido %ld, total_dado %ld)", unit_def[e->type - E_VILLAGER].name, n, k, e->com_modo, dist, tp - n, td);
		}
		e->carry = 0;
		e->com_fase = 0; e->timer = 0;
		break;
	}
	}
}

void cmd_target(Ref r, Ref tr) {
	Entity *e = ent_get(r), *t = ent_get(tr);
	if (!e || !t || !is_unit(e->type) || e->state == ST_DYING || t->state == ST_DYING || e->state == ST_ABOARD) return;

	e->target = tr;
	e->auto_target = 0;
	e->nwp = 0; e->patrol = 0;
	if (is_trader(e->type)) { comercio_ruta(e, t); return; }
	if (t->type == E_RELIC) {
		if (e->type != E_PRIEST) { goto_point(e, t->x, t->y); e->target = NOREF; return; }
		e->carry_kind = RES_COUNT; e->carry = 0;
		goto_target(e, t, ST_GATHER);
	} else if (t->type == E_TRANSPORT && t->owner == e->owner && !is_naval(e->type)) {
		/* subir a un transporte: se anda hasta la costa y se embarca al llegar */
		if (t->ncargo >= 5) { if (e->owner == W.human) world_msg("El transporte está lleno"); goto_point(e, t->x, t->y); e->target = NOREF; return; }
		goto_point(e, t->x, t->y);
		e->target = tr;
		e->state = ST_BOARD;
	} else if (is_resource(t->type)) {
		if (e->type == E_FISHBOAT && t->type == E_FISH) { e->carry_kind = RES_FOOD; goto_target(e, t, ST_GATHER); return; }
		if (e->type != E_VILLAGER || t->type == E_FISH) { goto_point(e, t->x, t->y); e->target = NOREF; return; }
		if (e->carry_kind != res_kind_of(t->type)) { e->carry = 0; }
		e->carry_kind = res_kind_of(t->type);
		goto_target(e, t, ST_GATHER);
	} else if (is_building(t->type) && t->owner == e->owner) {
		if (e->type != E_VILLAGER && !(e->type == E_PRIEST && e->carry_kind == RES_COUNT && e->carry > 0 && t->type == E_TEMPLE)) { goto_point(e, t->x, t->y); e->target = NOREF; return; }
		if (t->type == E_FARM && t->progress >= 1) {
			if (e->carry_kind != RES_FOOD) e->carry = 0;
			e->carry_kind = RES_FOOD;
			goto_target(e, t, ST_GATHER);
		} else if (t->progress < 1) {
			goto_target(e, t, ST_BUILD);
		} else if (e->carry > 0 && dropsite_accepts_u(t, e->carry_kind, e)) {
			goto_target(e, t, ST_RETURN);
		} else {
			goto_point(e, t->x, t->y + t->size);
			e->target = NOREF;
		}
	} else if (hostile(e->owner, t->owner) || t->owner < 0) {
		goto_target(e, t, ST_ATTACK);
	} else if (e->type == E_PRIEST && t->owner == e->owner && is_unit(t->type) && t->hp < t->maxhp) {
		goto_target(e, t, ST_ATTACK);
	} else {
		goto_point(e, t->x, t->y);
		e->target = NOREF;
	}
}

int can_place(EntType t, int tx, int ty) {
	int s = bld_def[t].size;
	if (tx < 1 || ty < 1 || tx + s >= W.w || ty + s >= W.h) return 0;
	for (int j = ty; j < ty + s; ++j)
		for (int i = tx; i < tx + s; ++i)
			if (!tile_walk(i, j)) return 0;
	if (t == E_DOCK) {   /* el muelle va en la orilla: alguna casilla de agua libre pegada al borde */
		int agua = 0;
		for (int k = -1; k <= s; ++k) agua += tile_sail(tx + k, ty - 1) + tile_sail(tx + k, ty + s) + tile_sail(tx - 1, ty + k) + tile_sail(tx + s, ty + k);
		if (!agua) return 0;
	}
	return 1;
}

/* La población máxima se calcula para todos los jugadores de una pasada y se guarda hasta el tic siguiente (la piden el
 * mundo, la IA y la interfaz muchas veces por tic). La clave es el instante de juego y la generación del mundo, que cambia
 * con cada partida nueva o cargada; un edificio terminado a mitad de tic cuenta en el siguiente. */
static unsigned mundo_gen = 1;
unsigned world_gen(void) { return mundo_gen; }
static struct { unsigned gen; float t; int nents; int cap[MAX_PLAYERS]; } popcache;

int world_pop_cap(int p) {
	if (p < 0 || p >= MAX_PLAYERS) return 0;
	if (popcache.gen != mundo_gen || popcache.t != W.time || popcache.nents != W.nents) {
		int cap[MAX_PLAYERS] = { 0 };
		for (int i = 0; i < W.nents; ++i) {
			Entity *e = &W.ents[i];
			if (!e->alive || e->owner < 0 || e->owner >= MAX_PLAYERS || e->progress < 1) continue;
			if (e->type == E_TOWN_CENTER) cap[e->owner] += 10;
			else if (e->type == E_HOUSE) cap[e->owner] += 5;
		}
		for (int q = 0; q < MAX_PLAYERS; ++q) {
			cap[q] += has_tech(q, T_LOGISTICS) ? 20 : 0;
			popcache.cap[q] = MIN(cap[q], W.setup.popcap > 0 ? W.setup.popcap : 200);
		}
		popcache.gen = mundo_gen; popcache.t = W.time; popcache.nents = W.nents;
	}
	return popcache.cap[p];
}

static int afford(int p, const int cost[RES_COUNT]) {
	for (int k = 0; k < RES_COUNT; ++k)
		if (W.pl[p].res[k] < cost[k]) return 0;
	return 1;
}

void cmd_set_stance(Ref r, int stance) {
	Entity *e = ent_get(r);
	if (!e || !is_unit(e->type) || e->type == E_VILLAGER) return;
	e->stance = stance;
	e->hx = e->x; e->hy = e->y;
	if (stance == STANCE_PASSIVE && e->state == ST_ATTACK) { free(e->path); e->path = NULL; e->plen = e->pidx = 0; e->state = ST_IDLE; e->target = NOREF; }
}

/* la unidad sigue y defiende a otra unidad o edificio de su propio equipo */
void cmd_guard(Ref r, Ref prot) {
	Entity *e = ent_get(r), *t = ent_get(prot);
	if (!e || !t || e == t || !is_unit(e->type) || e->type == E_VILLAGER || !same_team(e->owner, t->owner) || e->state == ST_ABOARD || e->state == ST_DYING) return;
	e->stance = STANCE_GUARD;
	e->target = prot;
	if (e->state != ST_IDLE) { free(e->path); e->path = NULL; e->plen = e->pidx = 0; e->state = ST_IDLE; }
}

void cmd_set_rally(Ref r, float x, float y, Ref target) {
	Entity *b = ent_get(r);
	if (!b || !is_building(b->type)) return;
	b->has_rally = 1; b->rx = CLAMP(x, 0.0f, (float)W.w); b->ry = CLAMP(y, 0.0f, (float)W.h); b->rtarget = target;
}

void world_score(int p, int *econ, int *mil, int *tech) {
	Player *pl = &W.pl[p];
	if (econ) *econ = pl->gathered / 10;
	if (mil) *mil = pl->units_killed * 10 + pl->bld_razed * 40;
	int t = pl->age * 150;
	for (int i = 0; i < T_COUNT; ++i) t += ((pl->techs >> i) & 1u) * 40;
	if (tech) *tech = t;
}

int has_tech(int p, int t) { return p >= 0 && p < MAX_PLAYERS && ((W.pl[p].techs >> t) & 1u) != 0; }

float eff_attack(int p, EntType t, int vs_building) {
	const UnitDef *u = &unit_def[t - E_VILLAGER];
	float d = vs_building ? u->dmg_bld : u->dmg;
	if (is_siege(t)) { if (has_tech(p, T_BALLISTICS)) d *= 1.25f; if (p >= 0 && civ_def[W.pl[p].civ].siege > 0) d *= civ_def[W.pl[p].civ].siege; }
	else if (u->ranged) d += has_tech(p, T_BOW) + has_tech(p, T_COMPOSITE) + (p >= 0 ? civ_def[W.pl[p].civ].ranged_atk : 0);
	else if (t != E_PRIEST && t != E_VILLAGER) d += 2 * has_tech(p, T_BRONZE_WEAPONS) + 2 * has_tech(p, T_IRON_WEAPONS) + (p >= 0 ? civ_def[W.pl[p].civ].melee_atk : 0);
	return d;
}

int eff_armor(int p, EntType t, int ranged) {
	const UnitDef *u = &unit_def[t - E_VILLAGER];
	int a = ranged ? u->parmor : u->armor;
	a += has_tech(p, T_LEATHER) + has_tech(p, T_SCALE);
	if (t == E_SCOUT || t == E_CAVALRY || t == E_HEAVYCAV) a += has_tech(p, T_CAVARMOR);
	return a;
}

float eff_range(int p, EntType t) {
	const UnitDef *u = &unit_def[t - E_VILLAGER];
	if (is_siege(t)) return u->range + (float)has_tech(p, T_ENGINEERING);
	return u->range + (u->ranged ? (float)has_tech(p, T_COMPOSITE) + (p >= 0 ? (float)civ_def[W.pl[p].civ].range : 0.0f) : 0.0f);
}

int cmd_research(int p, Ref bref, int tech) {
	Entity *b = ent_get(bref);
	if (p < 0 || p >= MAX_PLAYERS || !b || b->owner != p || b->progress < 1 || tech < 0 || tech >= T_COUNT) return 0;
	const TechDef *td = &tech_def[tech];
	if (td->bld != b->type) return 0;
	if (b->tech_cur) { if (p == W.human) world_msg("Ya se está investigando algo aquí"); return 0; }
	if (has_tech(p, tech)) return 0;
	if (!tech_allowed(p, tech)) { if (p == W.human) world_msg("Tu civilización no puede investigar esto"); return 0; }
	if (W.pl[p].age < td->age) { if (p == W.human) world_msg("Necesitas una edad superior"); return 0; }
	if (td->prereq >= 0 && !has_tech(p, td->prereq)) { if (p == W.human) world_msg("Necesitas investigar antes otra mejora"); return 0; }
	int cost[RES_COUNT] = { td->wood, td->food, td->gold, td->stone };
	if (!afford(p, cost)) { if (p == W.human) world_msg("No tienes suficientes recursos"); return 0; }
	/* la misma tecnología no se puede investigar dos veces a la vez */
	for (int i = 0; i < W.nents; ++i)
		if (W.ents[i].alive && W.ents[i].owner == p && W.ents[i].tech_cur == tech + 1) return 0;

	for (int k = 0; k < RES_COUNT; ++k) W.pl[p].res[k] -= cost[k];
	b->tech_cur = tech + 1;
	b->tech_t = 0;
	return 1;
}

static int count_bld_p(int p, EntType t);

int cmd_trade(int p, int sell, int buy) {
	Player *pl = &W.pl[p];
	if (sell == buy || sell < 0 || buy < 0 || sell >= RES_COUNT || buy >= RES_COUNT) return 0;
	if (!count_bld_p(p, E_MARKET)) { if (p == W.human) world_msg("Necesitas un mercado"); return 0; }
	const int lot = 100;
	if (sell == RES_GOLD) {
		/* comprar: paga oro, recibe mucha cantidad del recurso; el precio sube con cada compra */
		int cost = lot * pl->price[buy] / 100 + 12;
		if (pl->res[RES_GOLD] < cost) { if (p == W.human) world_msg("No tienes suficiente oro"); return 0; }
		pl->res[RES_GOLD] -= cost; pl->res[buy] += lot;
		pl->price[buy] = MIN(200, pl->price[buy] + 4);
	} else {
		/* vender: da mucha cantidad, recibe oro; el precio baja con cada venta */
		if (pl->res[sell] < lot) { if (p == W.human) world_msg("No tienes suficientes recursos"); return 0; }
		int gold = lot * pl->price[sell] / 100 - 12;
		pl->res[sell] -= lot; pl->res[RES_GOLD] += MAX(1, gold);
		pl->price[sell] = MAX(40, pl->price[sell] - 4);
	}
	return 1;
}

int cmd_build(int p, Ref worker, EntType t, int tx, int ty) {
	Entity *w = ent_get(worker);
	if (!w || w->type != E_VILLAGER || w->owner != p || !is_building(t) || t == E_TOWN_CENTER) return 0;

	if (W.pl[p].age < bld_def[t].age) { if (p == W.human) world_msg("Necesitas una edad superior"); return 0; }
	int cost[RES_COUNT] = { bld_def[t].wood, bld_def[t].food, bld_def[t].gold, bld_def[t].stone };
	if (!afford(p, cost)) { if (p == W.human) world_msg("No tienes suficientes recursos"); return 0; }
	if (!can_place(t, tx, ty)) { if (p == W.human) world_msg("No se puede construir aquí"); return 0; }

	for (int k = 0; k < RES_COUNT; ++k) W.pl[p].res[k] -= cost[k];

	int i = ent_new(t, p, tx + bld_def[t].size / 2.0f, ty + bld_def[t].size / 2.0f);
	if (i < 0) return 0;
	Entity *b = &W.ents[i];
	b->progress = 0.01f;
	b->hp = b->maxhp * 0.1f;
	w = ent_get(worker);
	cmd_target(worker, ent_ref(i));
	(void)w;
	return 1;
}

int cmd_train(int p, Ref bref, EntType t) {
	Entity *b = ent_get(bref);
	if (!b || b->owner != p || b->progress < 1 || !is_unit(t)) return 0;
	if (unit_def[t - E_VILLAGER].trainer != b->type) return 0;
	if (W.pl[p].age < unit_def[t - E_VILLAGER].age) { if (p == W.human) world_msg("Necesitas una edad superior"); return 0; }
	if (unit_tech_req(t) >= 0 && !has_tech(p, unit_tech_req(t))) { if (p == W.human) { char m[96]; snprintf(m, sizeof m, "Necesitas la mejora: %s", tech_def[unit_tech_req(t)].name); world_msg(m); } return 0; }
	if (b->ntrain >= 5) { if (p == W.human) world_msg("La cola de producción está llena"); return 0; }

	const UnitDef *u = &unit_def[t - E_VILLAGER];
	int cost[RES_COUNT] = { u->wood, u->food, u->gold, u->stone };
	if (!afford(p, cost)) { if (p == W.human) world_msg("No tienes suficientes recursos"); return 0; }
	if (W.pl[p].pop + b->ntrain >= world_pop_cap(p)) { if (p == W.human) world_msg("Necesitas más casas"); return 0; }

	for (int k = 0; k < RES_COUNT; ++k) W.pl[p].res[k] -= cost[k];
	if (b->ntrain == 0) b->train_time = 0;
	b->train[b->ntrain++] = t;
	return 1;
}

int cmd_cancel_train(int p, Ref bref, int idx) {
	Entity *b = ent_get(bref);
	if (!b || b->owner != p || idx < 0 || idx >= b->ntrain) return 0;
	const UnitDef *u = &unit_def[b->train[idx] - E_VILLAGER];
	W.pl[p].res[RES_WOOD] += u->wood; W.pl[p].res[RES_FOOD] += u->food; W.pl[p].res[RES_GOLD] += u->gold; W.pl[p].res[RES_STONE] += u->stone;
	memmove(b->train + idx, b->train + idx + 1, sizeof(int) * (size_t)(b->ntrain - idx - 1));
	--b->ntrain;
	if (idx == 0) b->train_time = 0;
	return 1;
}

/* ---------- edades ---------- */

static int count_bld(int p, EntType t) {
	int n = 0;
	for (int i = 0; i < W.nents; ++i)
		if (W.ents[i].alive && W.ents[i].owner == p && W.ents[i].type == t && W.ents[i].progress >= 1) ++n;
	return n;
}

static int count_bld_p(int p, EntType t) { return count_bld(p, t); }

/* avanzar necesita dos edificios terminados de la edad actual */
int age_requirements_met(int p, int target) {
	int n = 0;
	switch (target) {
	case AGE_TOOL:
		n = count_bld(p, E_HOUSE) + count_bld(p, E_GRANARY) + count_bld(p, E_STORAGE) + count_bld(p, E_BARRACKS);
		break;
	case AGE_BRONZE:
		n = count_bld(p, E_ARCHERY) + count_bld(p, E_STABLE) + count_bld(p, E_FARM) + count_bld(p, E_TOWER);
		break;
	case AGE_IRON:
		n = count_bld(p, E_TEMPLE) + count_bld(p, E_ACADEMY) + count_bld(p, E_GOVERNMENT);
		break;
	default: return 0;
	}
	return n >= 2;
}

int cmd_advance_age(int p) {
	Player *pl = &W.pl[p];
	int target = pl->age + 1;
	if (target >= AGE_COUNT) return 0;
	if (pl->age_t >= 0) { if (p == W.human) world_msg("Ya se está investigando una edad"); return 0; }
	if (!age_requirements_met(p, target)) { if (p == W.human) world_msg("Faltan edificios para avanzar de edad"); return 0; }
	if (pl->res[RES_FOOD] < age_def[target].food || pl->res[RES_GOLD] < age_def[target].gold) { if (p == W.human) world_msg("No tienes suficientes recursos"); return 0; }

	pl->res[RES_FOOD] -= age_def[target].food;
	pl->res[RES_GOLD] -= age_def[target].gold;
	pl->age_t = 0;
	return 1;
}

/* ---------- simulación de unidades ---------- */

static float dist_to_ent(const Entity *u, const Entity *t) {
	if (!is_building(t->type) && !is_resource(t->type)) {
		float dx = t->x - u->x, dy = t->y - u->y;
		return sqrtf(dx * dx + dy * dy);
	}
	float cx = CLAMP(u->x, (float)t->bx, (float)(t->bx + t->size)), cy = CLAMP(u->y, (float)t->by, (float)(t->by + t->size));
	float dx = cx - u->x, dy = cy - u->y;
	return sqrtf(dx * dx + dy * dy);
}

static int walk_step(Entity *e, float dt) {
	float speed = unit_def[e->type - E_VILLAGER].speed;
	if (e->owner >= 0 && e->type != E_VILLAGER && !is_naval(e->type)) speed *= civ_def[W.pl[e->owner].civ].speed;
	if (e->owner >= 0 && has_tech(e->owner, T_WHEEL) && (e->type == E_VILLAGER || e->type == E_SCOUT || e->type == E_CAVALRY || e->type == E_HEAVYCAV || e->type == E_CHARIOT)) speed *= 1.1f;
	float gx, gy;

	if (e->path && e->pidx < e->plen) {
		int ti = e->path[e->pidx];
		gx = ti % W.w + 0.5f; gy = ti / W.w + 0.5f;
	} else {
		gx = e->tx; gy = e->ty;
	}

	float dx = gx - e->x, dy = gy - e->y, d = sqrtf(dx * dx + dy * dy);
	float step = speed * dt;

	/* basta con pasar cerca de los puntos intermedios de una ruta; si no, las unidades que se apiñan en la misma esquina se empujarían para siempre */
	int on_route = e->path && e->pidx < e->plen;
	float reach = on_route ? 0.4f : MAX(step, 0.12f);

	if (d < 1e-4f || d <= reach) {
		if (!mover_a(e, gx, gy)) { free(e->path); e->path = NULL; e->plen = e->pidx = 0; return 1; }   /* el punto está en otro terreno: se queda en la orilla */
		if (e->path && e->pidx < e->plen) { ++e->pidx; return e->pidx >= e->plen; }
		return 1;
	}

	e->angle = atan2f(dy, dx);
	if (!mover_a(e, e->x + dx / d * step, e->y + dy / d * step)) {
		/* bloqueado por la orilla: deja la ruta (quien llama vuelve a buscar camino o se para) */
		free(e->path); e->path = NULL; e->plen = e->pidx = 0;
		return 1;
	}
	e->walking = 1;
	e->anim += dt * speed * 9.0f; /* los cuadros por segundo siguen la velocidad de andar */
	return 0;
}

static EntType kind_type(int kind) {
	return kind == RES_FOOD ? E_BERRIES : kind == RES_GOLD ? E_GOLD : kind == RES_STONE ? E_STONE : E_TREE;
}

static void find_next_resource(Entity *e, EntType kind) {
	int best = -1; float bd = 1e9f;
	for (int i = 0; i < W.nents; ++i) {
		Entity *r = &W.ents[i];
		int farm = r->type == E_FARM && r->owner == e->owner && r->progress >= 1;
		if (!r->alive || !(is_resource(r->type) || farm) || res_kind_of(r->type) != res_kind_of(kind) || r->amount <= 0) continue;
		if (res_kind_of(kind) == RES_FOOD && (e->type == E_FISHBOAT) != (r->type == E_FISH)) continue;   /* los barcos solo pescan; los aldeanos no */
		if (W.fog[tidx(r->bx, r->by)] == 0 && e->owner == W.human) continue;
		float d = (r->x - e->x) * (r->x - e->x) + (r->y - e->y) * (r->y - e->y);
		if (d < bd && d < 18 * 18) { bd = d; best = i; }
	}
	if (best >= 0) cmd_target(ent_ref((int)(e - W.ents)), ent_ref(best));
	else stop_unit(e);
}

static void start_dying(Entity *e) {
	if (e->state == ST_DYING) return;
	if (e->type == E_TRANSPORT) {   /* los pasajeros se hunden con el barco */
		/* se marca y se vacía antes del bucle: una carga cruzada o a sí mismo (partida manipulada) no puede volver aquí */
		int n = CLAMP(e->ncargo, 0, 5);
		Ref carga[5];
		memcpy(carga, e->cargo, sizeof(Ref) * (size_t)n);
		e->ncargo = 0;
		e->state = ST_DYING;
		for (int k = 0; k < n; ++k) {
			Entity *p = ent_get(carga[k]);
			if (!p || p == e || p->state == ST_DYING || p->state != ST_ABOARD) continue;
			p->state = ST_IDLE;
			if (p->owner >= 0 && p->owner < MAX_PLAYERS) ++W.pl[p->owner].units_lost;
			start_dying(p);
		}
		e->state = ST_IDLE;   /* se vuelve a marcar abajo, con el resto */
	}
	fx_evento(FX_EV_UNIDAD_MUERE, e);
	snd_event(SND_DIE_UNIT, e->x, e->y);
	if (e->carry_kind == RES_COUNT && e->carry > 0) { ent_new(E_RELIC, -1, e->x, e->y); e->carry = 0; e->carry_kind = RES_WOOD; }
	e->state = ST_DYING;
	e->die_t = 0;
	free(e->path); e->path = NULL; e->plen = 0;
	e->sel = 0;
	if (is_naval(e->type)) ent_kill(e);   /* los barcos no dejan cadáver */
}

static void add_shot(float x0, float y0, float x1, float y1) {
	if (W.nshots >= 64) return;
	W.shots[W.nshots].x0 = x0; W.shots[W.nshots].y0 = y0;
	W.shots[W.nshots].x1 = x1; W.shots[W.nshots].y1 = y1;
	W.shots[W.nshots].t = 0;
	++W.nshots;
}

static void raise_alert(float x, float y) {
	if (W.alert_t > 0 && W.time - W.alert_t < 7.0f) return;
	W.alert_t = W.time;
	if (W.npings < 8) { W.pings[W.npings].x = x; W.pings[W.npings].y = y; W.pings[W.npings].t = 0; ++W.npings; }
	snd_ui(SND_ALERT);
	world_msg("¡Te atacan!");
}

static void damage(Entity *t, float dmg, Entity *from, int ranged) {
	if (t->state == ST_DYING) return;
	if (from && t->owner == W.human && from->owner != W.human && !same_team(from->owner, W.human) && !is_resource(t->type)) raise_alert(t->x, t->y);
	if (is_unit(t->type)) dmg = MAX(1.0f, dmg - (float)eff_armor(t->owner, t->type, ranged));
	else if (is_building(t->type) && t->owner >= 0) {
		if (has_tech(t->owner, T_ARCHITECTURE)) dmg *= 0.85f;
		if (t->type == E_WALL) dmg /= 1.0f + has_tech(t->owner, T_WALL_MED) + has_tech(t->owner, T_FORTIFICATION);
	}
	t->hp -= dmg;

	if (is_animal(t->type) && from) {
		if (t->hp <= 0) {
			/* deja carne */
			if (from->owner >= 0) ++W.pl[from->owner].units_killed;
			snd_event(SND_DIE_UNIT, t->x, t->y);
			int ci = ent_new(E_CARCASS, -1, t->x, t->y);
			if (ci >= 0) {
				W.ents[ci].variant = t->type;
				W.ents[ci].angle = t->angle;
				W.ents[ci].amount = t->type == E_ELEPHANT ? 400.0f : 150.0f;
			}
			ent_kill(t);
			return;
		}
		if (t->type == E_ELEPHANT) {
			t->target = ent_ref((int)(from - W.ents));
			goto_target(t, from, ST_ATTACK);
		} else {
			/* los caballos huyen del cazador */
			float ax = t->x - from->x, ay = t->y - from->y, d = sqrtf(ax * ax + ay * ay) + 0.01f;
			float x = t->x + ax / d * 9.0f, y = t->y + ay / d * 9.0f;
			goto_point(t, x, y);
		}
		return;
	}

	if (t->hp <= 0) {
		t->hp = 0;
		if (t->owner >= 0 && t->owner < MAX_PLAYERS && !is_animal(t->type)) { if (is_building(t->type)) ++W.pl[t->owner].bld_lost; else ++W.pl[t->owner].units_lost; }
		if (from && is_naval(from->type)) AOEN_LOG("[AOEN] flota: %s destruye %s (t=%.0f)", unit_def[from->type - E_VILLAGER].name, is_building(t->type) ? bld_def[t->type].name : unit_def[t->type - E_VILLAGER].name, W.time);
		if (from && is_siege(from->type)) AOEN_LOG("[AOEN] asedio: %s destruye %s (t=%.0f)", unit_def[from->type - E_VILLAGER].name, is_building(t->type) ? bld_def[t->type].name : unit_def[t->type - E_VILLAGER].name, W.time);
		if (from && from->owner >= 0) {
			if (is_building(t->type)) ++W.pl[from->owner].bld_razed;
			else ++W.pl[from->owner].units_killed;
		}
		if (is_building(t->type)) {
			snd_event(SND_DIE_BLD, t->x, t->y);
			for (int k = 0; k < t->relics; ++k) {
				float x = t->x + (k % 3 - 1) * 1.5f, y = t->y + t->size * 0.5f + 1.5f + (k / 3);
				nearest_walk(&x, &y);
				ent_new(E_RELIC, -1, x, y);
			}
			ent_kill(t);
		} else {
			start_dying(t);
		}
	} else if (is_unit(t->type) && t->state == ST_IDLE) {
		/* contraataca */
		if (from && hostile(t->owner, from->owner) && t->type != E_VILLAGER && t->type != E_PRIEST && !is_trader(t->type) && t->stance != STANCE_PASSIVE) {
			int in_reach = 1;
			if (t->stance == STANCE_STAND) {
				float dx = from->x - t->x, dy = from->y - t->y;
				in_reach = sqrtf(dx * dx + dy * dy) <= eff_range(t->owner, t->type) + 0.6f;
			}
			if (in_reach) {
				t->target = ent_ref((int)(from - W.ents));
				t->auto_target = 1;
				goto_target(t, from, ST_ATTACK);
			}
		}
	}
}

/* Rejilla de enemigos posibles (entidades con dueño que no son recursos), por celdas de 8 casillas. Se rehace al principio
 * de cada tic: lo que nace durante el tic entra en el siguiente. nearest_enemy solo mira las celdas a su alcance. */
#define RJ_CELDA 8
static int *rj_cab, *rj_sig, rj_w, rj_h, rj_cap, rj_ncab, rj_n;
static unsigned rj_gen;

static void rejilla_rehacer(void) {
	rj_w = (W.w + RJ_CELDA - 1) / RJ_CELDA; rj_h = (W.h + RJ_CELDA - 1) / RJ_CELDA;
	if (rj_ncab < rj_w * rj_h) { free(rj_cab); rj_ncab = rj_w * rj_h; rj_cab = (int *)malloc(sizeof(int) * (size_t)rj_ncab); }
	if (rj_cap < W.nents) { free(rj_sig); rj_cap = W.nents + 1024; rj_sig = (int *)malloc(sizeof(int) * (size_t)rj_cap); }
	if (!rj_cab || !rj_sig) { free(rj_cab); free(rj_sig); rj_cab = rj_sig = NULL; rj_gen = 0; rj_ncab = rj_cap = 0; return; }
	for (int k = 0; k < rj_w * rj_h; ++k) rj_cab[k] = -1;
	/* de atrás adelante: cada celda queda en orden creciente de índice, como el recorrido lineal */
	for (int i = W.nents - 1; i >= 0; --i) {
		Entity *t = &W.ents[i];
		if (!t->alive || t->owner < 0 || t->state == ST_DYING || is_resource(t->type)) continue;
		int cx = CLAMP((int)t->x / RJ_CELDA, 0, rj_w - 1), cy = CLAMP((int)t->y / RJ_CELDA, 0, rj_h - 1);
		rj_sig[i] = rj_cab[cy * rj_w + cx];
		rj_cab[cy * rj_w + cx] = i;
	}
	rj_n = W.nents;
	rj_gen = mundo_gen;
}

static int nearest_enemy(const Entity *u, float range) {
	int best = -1; float bd = range * range;
	if (rj_gen == mundo_gen && range <= 40) {
		/* los edificios cuentan a 0,6 de su distancia: se buscan hasta range / sqrt(0,6) */
		float r = range * 1.30f;
		int x0 = CLAMP((int)(u->x - r) / RJ_CELDA, 0, rj_w - 1), x1 = CLAMP((int)(u->x + r) / RJ_CELDA, 0, rj_w - 1);
		int y0 = CLAMP((int)(u->y - r) / RJ_CELDA, 0, rj_h - 1), y1 = CLAMP((int)(u->y + r) / RJ_CELDA, 0, rj_h - 1);
		for (int cy = y0; cy <= y1; ++cy)
			for (int cx = x0; cx <= x1; ++cx)
				for (int i = rj_cab[cy * rj_w + cx]; i >= 0; i = rj_sig[i]) {
					Entity *t = &W.ents[i];
					if (!t->alive || t->state == ST_DYING || t->state == ST_ABOARD || !hostile(u->owner, t->owner) || is_resource(t->type)) continue;
					float d = (t->x - u->x) * (t->x - u->x) + (t->y - u->y) * (t->y - u->y);
					if (is_building(t->type)) d *= 0.6f;
					if (d < bd || (d == bd && i < best)) { bd = d; best = i; }
				}
		return best;
	}
	for (int i = 0; i < W.nents; ++i) {
		Entity *t = &W.ents[i];
		if (!t->alive || t->state == ST_DYING || t->state == ST_ABOARD || !hostile(u->owner, t->owner) || is_resource(t->type)) continue;
		if (t->owner == W.human) { /* los enemigos solo se ven cuando no están ocultos */ }
		float d = (t->x - u->x) * (t->x - u->x) + (t->y - u->y) * (t->y - u->y);
		if (is_building(t->type)) d *= 0.6f;
		if (d < bd) { bd = d; best = i; }
	}
	return best;
}

/* una unidad que anda y no avanza durante un rato da un paso a un lado o busca otro camino */
static void unstick(Entity *e) {
	if (!e->path || e->pidx >= e->plen) {
		/* no queda ruta: si ya casi hemos llegado (otros están parados en el sitio) simplemente paramos; si no, lo intentamos de nuevo */
		float dx = e->tx - e->x, dy = e->ty - e->y;
		if (e->state == ST_MOVE) {
			if (dx * dx + dy * dy < 1.0f) { e->state = ST_IDLE; return; }
			goto_point(e, e->tx, e->ty);
		}
		return;
	}

	float side = (e->stuck % 2) ? 1.0f : -1.0f;
	++e->stuck;
	for (int attempt = 0; attempt < 2; ++attempt) {
		float a = e->angle + side * 1.5708f;
		float sx = e->x + cosf(a) * 1.1f, sy = e->y + sinf(a) * 1.1f;
		int tx = (int)floorf(sx), ty = (int)floorf(sy);
		if (casilla_paso(e, tx, ty)) {   /* los barcos se apartan por el agua; el resto, por tierra */
			int remaining = e->plen - e->pidx;
			int *np = (int *)malloc(sizeof(int) * (size_t)(remaining + 1));
			np[0] = ty * W.w + tx;
			memcpy(np + 1, e->path + e->pidx, sizeof(int) * (size_t)remaining);
			free(e->path);
			e->path = np; e->plen = remaining + 1; e->pidx = 0;
			return;
		}
		side = -side;
	}

	/* muros a ambos lados: replanifica toda la ruta */
	if (e->state == ST_MOVE) goto_point(e, e->tx, e->ty);
}

/* desembarca a los pasajeros en la tierra más cercana al barco; devuelve cuántos bajaron */
static int unload_boat(Entity *b) {
	int bajados = 0;
	for (int k = 0; k < b->ncargo; ) {
		Entity *u = ent_get(b->cargo[k]);
		if (!u || u->state != ST_ABOARD || u->hp <= 0) { memmove(b->cargo + k, b->cargo + k + 1, sizeof(Ref) * (size_t)(b->ncargo - k - 1)); --b->ncargo; continue; }
		int hecho = 0;
		for (int r = 1; r <= 6 && !hecho; ++r)
			for (int a = 0; a < 16 && !hecho; ++a) {
				float ang = (a + k * 3) * 6.2831853f / 16.0f, x = b->x + cosf(ang) * r, y = b->y + sinf(ang) * r;
				if (tile_walk((int)floorf(x), (int)floorf(y))) { u->x = x; u->y = y; hecho = 1; }
			}
		if (!hecho) { ++k; continue; }
		u->state = ST_IDLE; u->target = NOREF; u->hx = u->x; u->hy = u->y;
		free(u->path); u->path = NULL; u->plen = u->pidx = 0;
		memmove(b->cargo + k, b->cargo + k + 1, sizeof(Ref) * (size_t)(b->ncargo - k - 1)); --b->ncargo;
		++bajados;
	}
	if (bajados) {
		AOEN_LOG("[AOEN] transporte: desembarcan %d unidades (jugador %d, en %.0f,%.0f, t=%.0f)", bajados, b->owner, b->x, b->y, W.time);
		if (b->owner == W.human) world_msg("Unidades desembarcadas");
	} else if (b->ncargo && b->owner == W.human) world_msg("No hay tierra libre cerca para desembarcar");
	return bajados;
}

int transport_load(const Entity *boat) { return boat ? boat->ncargo : 0; }

static void actualizar_comercio(Entity *e, float dt);

static void update_unit(Entity *e, float dt) {
	const UnitDef *ud = &unit_def[e->type - E_VILLAGER];
	e->walking = 0;
	e->acting = 0;
	if (is_trader(e->type) && e->state != ST_DYING) actualizar_comercio(e, dt);

	if (e->state == ST_DYING) {
		e->die_t += dt;
		if (e->die_t > 20) ent_kill(e);
		return;
	}

	if (e->state == ST_ABOARD) {   /* el pasajero acompaña al barco */
		Entity *b = ent_get(e->target);
		if (!b || b->state == ST_DYING) { e->state = ST_IDLE; if (e->owner >= 0 && e->owner < MAX_PLAYERS) ++W.pl[e->owner].units_lost; start_dying(e); }
		else { e->x = b->x; e->y = b->y; }
		return;
	}

	Entity *t = ent_get(e->target);

	switch (e->state) {
	case ST_BOARD: {
		if (!t || t->type != E_TRANSPORT || t->state == ST_DYING) { stop_unit(e); break; }
		if (dist_to_ent(e, t) <= 3.6f) {
			if (t->ncargo < 5) {
				t->cargo[t->ncargo++] = ent_ref((int)(e - W.ents));
				free(e->path); e->path = NULL; e->plen = e->pidx = 0;
				e->state = ST_ABOARD; e->x = t->x; e->y = t->y;
				e->sel = 0;   /* a bordo no se puede mandar: deja de estar seleccionado */
				AOEN_LOG("[AOEN] transporte: embarca %s (%d a bordo)", unit_def[e->type - E_VILLAGER].name, t->ncargo);
				if (e->owner == W.human) world_msg("Embarcado");
			} else { stop_unit(e); if (e->owner == W.human) world_msg("El transporte está lleno"); }
			break;
		}
		if (walk_step(e, dt)) {   /* llegó al final de la ruta: reintenta acercarse al barco, que puede haberse movido */
			if (++e->stuck > 8) { stop_unit(e); break; }
			Ref keep = e->target;
			goto_point(e, t->x, t->y);
			e->target = keep; e->state = ST_BOARD;
		}
		break;
	}
	case ST_IDLE: {
		if (e->type == E_TRANSPORT && e->unload) { e->unload = 0; unload_boat(e); break; }
		if (!is_animal(e->type) && e->nwp > 0) {
			/* siguiente punto de ruta de las órdenes en cola */
			float wx = e->wpx[0], wy = e->wpy[0];
			memmove(e->wpx, e->wpx + 1, sizeof(float) * 3); memmove(e->wpy, e->wpy + 1, sizeof(float) * 3);
			--e->nwp;
			goto_point(e, wx, wy);
			break;
		}
		if (is_animal(e->type)) {
			/* deambula alrededor del lugar donde vive la manada */
			e->timer -= dt;
			if (e->timer <= 0) {
				e->timer = 3.0f + frand() * 6.0f;
				float x = e->bx + (frand() - 0.5f) * 10.0f, y = e->by + (frand() - 0.5f) * 10.0f;
				if (tile_walk((int)floorf(x), (int)floorf(y))) goto_point(e, x, y);
			}
		} else if (e->type == E_PRIEST) {
			/* cura a los amigos heridos cercanos */
			int best = -1; float bd = 25.0f;
			for (int i = 0; i < W.nents; ++i) {
				Entity *f = &W.ents[i];
				if (!f->alive || f == e || f->owner != e->owner || !is_unit(f->type) || f->state == ST_DYING || f->hp >= f->maxhp) continue;
				float d = (f->x - e->x) * (f->x - e->x) + (f->y - e->y) * (f->y - e->y);
				if (d < bd) { bd = d; best = i; }
			}
			if (best >= 0) cmd_target(ent_ref((int)(e - W.ents)), ent_ref(best));
		} else if (e->type != E_VILLAGER && e->type != E_FISHBOAT && e->type != E_TRANSPORT && !is_trader(e->type)) {
			Ref me = ent_ref((int)(e - W.ents));
			int en = -1, pat_antes = e->patrol;
			switch (e->stance) {
			case STANCE_PASSIVE: break;
			case STANCE_STAND:
				en = nearest_enemy(e, eff_range(e->owner, e->type) + 0.5f);
				if (en >= 0) { cmd_target(me, ent_ref(en)); e->auto_target = 1; }
				break;
			case STANCE_DEFENSIVE:
				en = nearest_enemy(e, 7.0f);
				if (en >= 0) { cmd_target(me, ent_ref(en)); e->auto_target = 1; }
				else if ((e->x - e->hx) * (e->x - e->hx) + (e->y - e->hy) * (e->y - e->hy) > 2.5f) goto_point(e, e->hx, e->hy);
				break;
			case STANCE_GUARD: {
				Entity *pr = ent_get(e->target);
				if (!pr || pr->state == ST_DYING) { e->stance = STANCE_AGGRESSIVE; e->target = NOREF; break; }
				Entity probe = *pr;
				probe.owner = e->owner;
				en = nearest_enemy(&probe, 7.0f);
				if (en >= 0) { Ref keep = e->target; cmd_target(me, ent_ref(en)); e->auto_target = 1; e->target = ent_ref(en); (void)keep; }
				else {
					float dx = pr->x - e->x, dy = pr->y - e->y;
					if (dx * dx + dy * dy > 12.0f) goto_point(e, pr->x - dx * 0.2f, pr->y - dy * 0.2f);
				}
				break;
			}
			default:
				en = nearest_enemy(e, ud->ranged ? ud->range + 1.0f : 6.0f);
				if (en >= 0) { cmd_target(me, ent_ref(en)); e->auto_target = 1; }
				break;
			}
			if (en >= 0) e->patrol = pat_antes;   /* cmd_target la borra: el combate no cancela la patrulla */
			/* patrulla: sin enemigos a la vista, va al otro extremo */
			if (en < 0 && e->patrol && e->state == ST_IDLE) {
				if (e->patrol == 1) { e->patrol = 2; goto_point(e, e->wpx[0], e->wpy[0]); }
				else { e->patrol = 1; goto_point(e, e->ptx, e->pty); }
			}
		}
		break;
	}
	case ST_MOVE:
		if (walk_step(e, dt)) { e->state = ST_IDLE; }
		else if (e->pidx >= e->plen && e->plen > 0) {}
		break;
	case ST_GATHER: {
		if (t && t->type == E_RELIC) {
			if (dist_to_ent(e, t) > 1.3f) {
				if (walk_step(e, dt) && dist_to_ent(e, t) > 1.3f) goto_target(e, t, ST_GATHER);
				break;
			}
			e->carry = 1; e->carry_kind = RES_COUNT;
			ent_kill(t);
			e->target = NOREF;
			int di = find_dropsite(e, RES_COUNT);
			if (di >= 0) { e->target = ent_ref(di); goto_target(e, &W.ents[di], ST_RETURN); }
			else { world_msg("Necesitas un templo para guardar la reliquia"); stop_unit(e); }
			break;
		}
		if (!t || t->amount <= 0) { find_next_resource(e, kind_type(e->carry_kind)); break; }
		if (dist_to_ent(e, t) > 1.3f) {
			if (walk_step(e, dt) && dist_to_ent(e, t) > 1.3f) {
				if (++e->stuck > 3) { find_next_resource(e, t->type); }
				else goto_target(e, t, ST_GATHER);
			}
			break;
		}
		e->angle = atan2f(t->y - e->y, t->x - e->x);
		e->acting = 1;
		e->timer += dt;
		if (e->timer >= 1.2f) {
			e->timer -= 1.2f;
			if (e->owner == W.human || rand() % 3 == 0)
				snd_event(t->type == E_GOLD || t->type == E_STONE ? SND_MINE : t->type == E_BERRIES || t->type == E_FARM || t->type == E_CARCASS || t->type == E_FISH ? SND_FORAGE : SND_WOOD, e->x, e->y);
		}
		e->phase = e->timer / 1.2f;

		/* la velocidad de recolección depende de lo que se recoge (recursos por segundo) */
		float rate = t->type == E_FARM ? 0.6f : t->type == E_CARCASS ? 1.2f : t->type == E_FISH ? 0.8f : t->type == E_BERRIES ? 0.9f : t->type == E_GOLD ? 0.7f : t->type == E_STONE ? 0.65f : 0.75f;
		if (t->type == E_TREE) rate *= 1.0f + 0.25f * (has_tech(e->owner, T_TOOLWORKING) + has_tech(e->owner, T_BRONZE_AXE));
		else if (t->type == E_GOLD || t->type == E_STONE) rate *= 1.0f + 0.25f * has_tech(e->owner, T_MINING);
		else if (t->type == E_FARM || t->type == E_BERRIES || t->type == E_FISH) rate *= 1.0f + 0.25f * (has_tech(e->owner, T_DOMESTICATION) + has_tech(e->owner, T_PLOW));
		if (e->owner >= 0 && W.pl[e->owner].ai) rate *= W.pl[e->owner].diff == 0 ? 0.8f : W.pl[e->owner].diff == 2 ? 1.3f : 1.0f;
		if (e->carry_kind >= 0 && e->carry_kind < RES_COUNT) rate *= civ_def[W.pl[e->owner].civ].gather[e->carry_kind];
		e->gacc += rate * dt;
		if (e->type == E_VILLAGER) e->com_espera = t->type == E_CARCASS;   /* carne: también se entrega en el almacén */
		while (e->gacc >= 1.0f && t && t->amount > 0 && e->carry < CARRY_CAP) {
			e->gacc -= 1.0f;
			t->amount -= 1;
			e->carry += 1;
		}
		if (e->carry >= CARRY_CAP) {
			e->gacc = 0;
			int d = find_dropsite(e, e->carry_kind);
			if (d >= 0) goto_target(e, &W.ents[d], ST_RETURN);
		}
		if (t->amount <= 0) {
			EntType tt = t->type;
			ent_kill(t);
			e->target = NOREF;
			if (e->state == ST_GATHER) find_next_resource(e, tt == E_FARM ? E_BERRIES : tt);
		}
		break;
	}
	case ST_RETURN: {
		Entity *d = t;
		if (!d || !dropsite_accepts_u(d, e->carry_kind, e)) {
			int di = find_dropsite(e, e->carry_kind);
			if (di < 0) { stop_unit(e); break; }
			e->target = ent_ref(di);
			goto_target(e, &W.ents[di], ST_RETURN);
			break;
		}
		if (dist_to_ent(e, d) > 1.5f) {
			if (walk_step(e, dt) && dist_to_ent(e, d) > 1.5f) goto_target(e, d, ST_RETURN);
			break;
		}
		if (e->carry_kind == RES_COUNT) {
			/* una reliquia: guardada dentro del templo, produce oro */
			d->relics += e->carry;
			e->carry = 0;
			e->carry_kind = RES_WOOD;
			stop_unit(e);
			break;
		}
		W.pl[e->owner].res[e->carry_kind] += e->carry;
		W.pl[e->owner].gathered += e->carry;
		if (e->carry_kind >= 0 && e->carry_kind < 4) W.pl[e->owner].gathered_k[e->carry_kind] += e->carry;
		e->carry = 0;
		/* vuelve al último recurso cercano */
		find_next_resource(e, e->carry_kind == RES_FOOD ? E_BERRIES : e->carry_kind == RES_GOLD ? E_GOLD : e->carry_kind == RES_STONE ? E_STONE : E_TREE);
		break;
	}
	case ST_BUILD: {
		if (!t || t->progress >= 1) { stop_unit(e); break; }
		if (dist_to_ent(e, t) > 1.5f) {
			if (walk_step(e, dt) && dist_to_ent(e, t) > 1.5f) goto_target(e, t, ST_BUILD);
			break;
		}
		e->angle = atan2f(t->y - e->y, t->x - e->x);
		e->acting = 1;
		e->timer += dt;
		if (e->timer >= 1.0f) e->timer -= 1.0f;
		e->phase = e->timer;
		/* más constructores ayudan, pero con rendimientos decrecientes */
		t->nbuild_now += 1.0f;
		float eff = dt / bld_def[t->type].build_time / sqrtf(MAX(1.0f, t->builders));
		if (e->owner >= 0 && civ_def[W.pl[e->owner].civ].build > 0) eff *= civ_def[W.pl[e->owner].civ].build;
		t->progress += eff;
		t->hp = MIN(t->maxhp, t->hp + t->maxhp * 0.9f * eff);
		if (t->progress >= 1) {
			t->progress = 1; t->hp = t->maxhp;
			fx_evento(FX_EV_EDIFICIO_ACABADO, t);
			if (t->type == E_FARM) t->amount = 250;
			if (t->owner == W.human) { world_msg("Construcción terminada"); snd_event(SND_BUILD_DONE, t->x, t->y); }
			stop_unit(e);
			/* los constructores de los puntos de depósito empiezan a recolectar cerca */
			if (t->type == E_GRANARY) find_next_resource(e, E_BERRIES);
			else if (t->type == E_STORAGE) find_next_resource(e, E_TREE);
			else if (t->type == E_FARM) { e->carry_kind = RES_FOOD; cmd_target(ent_ref((int)(e - W.ents)), ent_ref((int)(t - W.ents))); }
		}
		break;
	}
	case ST_ATTACK: {
		if (!t || t->state == ST_DYING || !t->alive) { stop_unit(e); break; }
		int friendly = t->owner == e->owner && is_unit(t->type);
		if (friendly && (e->type != E_PRIEST || t->hp >= t->maxhp)) { stop_unit(e); break; }

		float d = dist_to_ent(e, t);
		float reach = eff_range(e->owner, e->type) + (ud->ranged || e->type == E_PRIEST ? 0.2f : 0.4f);
		if (d > reach && e->auto_target && e->stance == STANCE_STAND) { stop_unit(e); break; }
		if (d > reach && e->auto_target && e->stance == STANCE_DEFENSIVE && (e->x - e->hx) * (e->x - e->hx) + (e->y - e->hy) * (e->y - e->hy) > 81.0f) { stop_unit(e); break; }
		if (d > reach) {
			e->timer = 0;
			if (e->plen == 0 || e->pidx >= e->plen || (int)(W.time * 2) != (int)((W.time - dt) * 2)) {
				if (!is_building(t->type) && !is_resource(t->type)) {
					int gx = (int)floorf(t->x), gy = (int)floorf(t->y);
					find_path(e, gx, gy, gx, gy, 0);
					e->tx = t->x; e->ty = t->y;   /* sin ruta, walk_step va hacia aquí (y se queda en la orilla si es otro terreno) */
				} else goto_target(e, t, ST_ATTACK);
			}
			if (walk_step(e, dt) && dist_to_ent(e, t) > reach + 0.2f) ++e->stuck;
			if (e->stuck > 6) stop_unit(e);
			break;
		}

		if (d < unit_min_range(e->type) && !is_building(t->type) && !friendly) {
			/* demasiado cerca para una catapulta: retrocede hasta tener alcance */
			float ax = e->x - t->x, ay = e->y - t->y, al = sqrtf(ax * ax + ay * ay) + 0.001f;
			float nx = e->x + ax / al * unit_def[e->type - E_VILLAGER].speed * dt, ny = e->y + ay / al * unit_def[e->type - E_VILLAGER].speed * dt;
			static int avisado;
			if (!avisado) { avisado = 1; AOEN_LOG("[AOEN] asedio: %s se aparta, alcance mínimo %.1f", unit_def[e->type - E_VILLAGER].name, unit_min_range(e->type)); }
			if (casilla_paso(e, (int)floorf(nx), (int)floorf(ny)) && mover_a(e, nx, ny)) { e->walking = 1; e->anim += dt * 9.0f; e->timer = 0; break; }
		}
		e->angle = atan2f(t->y - e->y, t->x - e->x);
		e->timer += dt;
		e->acting = 1;
		e->phase = MIN(1.0f, e->timer / unit_cooldown(e->type));

		if (e->type == E_PRIEST) {
			if (friendly) {
				t->hp = MIN(t->maxhp, t->hp + 4.0f * (has_tech(e->owner, T_MEDICINE) ? 2.0f : 1.0f) * (has_tech(e->owner, T_MYSTICISM) ? 1.5f : 1.0f) * dt);
				e->timer = 0;
			} else if (e->timer >= (has_tech(e->owner, T_MONOTHEISM) ? 2.5f : 5.0f) / (civ_def[W.pl[e->owner].civ].priest > 0 ? civ_def[W.pl[e->owner].civ].priest : 1.0f)) {
				/* conversión */
				e->timer = 0;
				if (frand() < (has_tech(e->owner, T_FAITH) ? 0.9f : 0.7f) && is_unit(t->type) && t->type != E_PRIEST) {
					t->owner = e->owner;
					stop_unit(t);
					t->carry = 0;
					snd_event(SND_CONVERT, t->x, t->y);
					if (e->owner == W.human) world_msg("¡Unidad convertida!");
				}
				stop_unit(e);
			}
			break;
		}

		float cooldown = unit_cooldown(e->type);
		if (e->timer >= cooldown) {
			e->timer = 0;
			if (is_resource(t->type)) { stop_unit(e); break; }
			float dmg = eff_attack(e->owner, e->type, is_building(t->type));
			if (ud->ranged) add_shot(e->x, e->y, t->x, t->y);
			snd_event(ud->ranged ? SND_SHOOT : SND_HIT_MELEE, e->x, e->y);
			float tx0 = t->x, ty0 = t->y;
			damage(t, dmg, e, ud->ranged);
			/* las catapultas dañan en área a todo lo enemigo cerca del impacto (mitad de daño) */
			float rad = unit_splash(e->type);
			if (rad > 0) {
				int golpes = 0;
				for (int k = 0; k < W.nents; ++k) {
					Entity *o = &W.ents[k];
					if (!o->alive || o == t || o->state == ST_DYING || o->state == ST_ABOARD || is_resource(o->type) || !hostile(e->owner, o->owner)) continue;
					float ox = o->x - tx0, oy = o->y - ty0;
					float reachr = rad + (is_building(o->type) ? o->size * 0.5f : 0.0f);
					if (ox * ox + oy * oy <= reachr * reachr) { damage(o, eff_attack(e->owner, e->type, is_building(o->type)) * 0.5f, e, 1); ++golpes; }
				}
				if (golpes) AOEN_LOG("[AOEN] área: %s alcanza %d objetivos más", unit_def[e->type - E_VILLAGER].name, golpes);
			}
		}
		break;
	}
	default: break;
	}

	/* comprobación del progreso una vez cada medio segundo mientras anda */
	if (e->walking) {
		e->stuck_t += dt;
		if (e->stuck_t >= 0.5f) {
			float mx = e->x - e->lx, my = e->y - e->ly;
			if (mx * mx + my * my < 0.012f) unstick(e);
			e->lx = e->x; e->ly = e->y; e->stuck_t = 0;
		}
	} else {
		e->stuck_t = 0; e->lx = e->x; e->ly = e->y;
	}
}

/* separa las unidades que se solapan para que no se apilen (usa una rejilla gruesa para que los mundos grandes vayan rápido) */
static void separate_units(void) {
	static int *units; static int ucap;
	static int *cell_head, *next; static int cells_cap;
	int gw = (W.w + 3) / 4, gh = (W.h + 3) / 4;
	int n = 0;

	if (ucap < W.nents) { ucap = W.nents + 512; units = (int *)realloc(units, sizeof(int) * (size_t)ucap); next = (int *)realloc(next, sizeof(int) * (size_t)ucap); }
	if (cells_cap < gw * gh) { cells_cap = gw * gh; cell_head = (int *)realloc(cell_head, sizeof(int) * (size_t)cells_cap); }
	for (int i = 0; i < gw * gh; ++i) cell_head[i] = -1;

	for (int i = 0; i < W.nents; ++i) {
		Entity *a = &W.ents[i];
		if (!a->alive || !is_unit(a->type) || a->state == ST_DYING || a->state == ST_ABOARD) continue;
		int c = CLAMP((int)a->y / 4, 0, gh - 1) * gw + CLAMP((int)a->x / 4, 0, gw - 1);
		units[n] = i;
		next[n] = cell_head[c];
		cell_head[c] = n;
		++n;
	}

	for (int k = 0; k < n; ++k) {
		Entity *a = &W.ents[units[k]];
		int cx = CLAMP((int)a->x / 4, 0, gw - 1), cy = CLAMP((int)a->y / 4, 0, gh - 1);
		for (int yy = MAX(0, cy - 1); yy <= MIN(gh - 1, cy + 1); ++yy) {
			for (int xx = MAX(0, cx - 1); xx <= MIN(gw - 1, cx + 1); ++xx) {
				for (int j = cell_head[yy * gw + xx]; j >= 0; j = next[j]) {
					if (j <= k) continue;
					Entity *b = &W.ents[units[j]];
					float dx = b->x - a->x, dy = b->y - a->y;
					if (fabsf(dx) > 0.4f || fabsf(dy) > 0.4f) continue;
					float d2 = dx * dx + dy * dy;
					if (d2 >= 0.16f || d2 < 1e-6f) continue;
					float d = sqrtf(d2), push = (0.4f - d) * 0.3f;
					float ux = dx / d, uy = dy / d;
					/* quién cede el paso: las unidades quietas dejan sitio a las que andan */
					float wa = 0.5f, wb = 0.5f;
					if (a->walking && !b->walking) { wa = 0.1f; wb = 0.9f; }
					else if (!a->walking && b->walking) { wa = 0.9f; wb = 0.1f; }

					float ax = -ux * push * wa * 2, ay = -uy * push * wa * 2, bx = ux * push * wb * 2, by = uy * push * wb * 2;

					/* de frente: ambas se apartan a su derecha, como personas que pasan por un pasillo */
					if (a->walking && b->walking) {
						float ha = cosf(a->angle) * cosf(b->angle) + sinf(a->angle) * sinf(b->angle);
						if (ha < -0.3f) {
							const float side = 0.04f;
							ax += sinf(a->angle) * side; ay += -cosf(a->angle) * side;
							bx += sinf(b->angle) * side; by += -cosf(b->angle) * side;
						}
					}

					if (casilla_paso(a, (int)floorf(a->x + ax), (int)floorf(a->y + ay))) mover_a(a, a->x + ax, a->y + ay);
					if (casilla_paso(b, (int)floorf(b->x + bx), (int)floorf(b->y + by))) mover_a(b, b->x + bx, b->y + by);
				}
			}
		}
	}
}

static void spawn_near(const Entity *b, EntType t) {
	int sea = is_naval(t);
	for (int r = 2; r < (sea ? 12 : 8); ++r) {
		for (int k = 0; k < 24; ++k) {
			float a = (float)k / 24.0f * 6.2831853f;
			float x = b->x + cosf(a) * (b->size * 0.5f + r * 0.5f), y = b->y + sinf(a) * (b->size * 0.5f + r * 0.5f);
			if (sea ? tile_sail((int)floorf(x), (int)floorf(y)) : tile_walk((int)floorf(x), (int)floorf(y))) {
				int i = ent_new(t, b->owner, x, y);
				if (i < 0) return;
				Entity *u = &W.ents[i];
				u->state = ST_IDLE;
				if (b->owner == W.human) snd_event(t == E_VILLAGER ? SND_TRAIN_VILL : SND_TRAIN_MIL, x, y);
				if (b->has_rally) {
					Ref ur = ent_ref(i);
					if (ent_get(b->rtarget)) cmd_target(ur, b->rtarget);
					else cmd_move(ur, b->rx, b->ry);
				}
				return;
			}
		}
	}
}

static void terminar(int g);

static void update_building(Entity *b, float dt) {
	b->anim += dt * 8;
	b->builders = b->nbuild_now;
	b->nbuild_now = 0;

	if (b->type == E_TOWER && b->progress >= 1) {
		b->timer += dt;
		float trange = 8.0f + 2.0f * has_tech(b->owner, T_GUARD), tdmg = 6.0f + 3.0f * has_tech(b->owner, T_SENTRY) + 3.0f * has_tech(b->owner, T_GUARD) + civ_def[W.pl[b->owner].civ].tower;
		if (b->timer >= (has_tech(b->owner, T_BALLISTA_T) ? 1.33f : 2.0f)) {
			int best = -1; float bd = trange * trange;
			for (int i = 0; i < W.nents; ++i) {
				Entity *t = &W.ents[i];
				if (!t->alive || t->state == ST_DYING || t->state == ST_ABOARD || !is_unit(t->type) || !hostile(b->owner, t->owner)) continue;
				float d = (t->x - b->x) * (t->x - b->x) + (t->y - b->y) * (t->y - b->y);
				if (d < bd) { bd = d; best = i; }
			}
			if (best >= 0) {
				b->timer = 0;
				add_shot(b->x, b->y, W.ents[best].x, W.ents[best].y);
				damage(&W.ents[best], tdmg, b, 1);
			}
		}
	}

	if (b->type == E_WONDER && b->progress >= 1 && (W.setup.victory == VIC_STANDARD || W.setup.victory == VIC_WONDER)) {
		if (b->timer == 0) world_msg("¡Se ha terminado una Maravilla! Hay que defenderla 10 minutos");
		b->timer += dt;
		if (b->timer >= 600.0f && !W.over) {
			terminar(b->owner);
		}
	}

	if (b->type == E_TEMPLE && b->progress >= 1 && b->relics > 0) {
		b->racc += b->relics * 0.4f * dt;
		while (b->racc >= 1.0f) { b->racc -= 1.0f; W.pl[b->owner].res[RES_GOLD] += 1; W.pl[b->owner].gathered += 1; }
	}

	if (b->progress >= 1 && b->tech_cur) {
		b->tech_t += dt;
		const TechDef *td = &tech_def[b->tech_cur - 1];
		if (b->tech_t >= td->time) {
			W.pl[b->owner].techs |= 1u << (b->tech_cur - 1);
			if (b->owner == W.human) { char m[96]; snprintf(m, sizeof m, "Investigado: %s", td->name); world_msg(m); snd_ui(SND_AGE); }
			b->tech_cur = 0;
			b->tech_t = 0;
		}
	}

	if (b->progress < 1 || b->ntrain == 0) return;

	EntType t = (EntType)b->train[0];
	b->train_time += dt;
	if (b->train_time >= unit_def[t - E_VILLAGER].train_time * civ_def[W.pl[b->owner].civ].train_time) {
		spawn_near(b, t);
		memmove(b->train, b->train + 1, sizeof(int) * 4);
		--b->ntrain;
		b->train_time = 0;
		if (b->owner == W.human) world_msg("Unidad entrenada");
	}
}

/* ---------- niebla de guerra (para el jugador humano) ---------- */

static void update_fog(void) {
	for (int i = 0; i < W.w * W.h; ++i)
		if (W.fog[i] == 2) W.fog[i] = 1;

	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!e->alive || e->owner < 0 || !same_team(e->owner, W.human) || e->state == ST_DYING) continue;
		int r = (e->type == E_TOWN_CENTER ? 10 : is_building(e->type) ? 7 : 6) + 2 * has_tech(e->owner, T_ASTRONOMY);
		int cx = (int)floorf(e->x), cy = (int)floorf(e->y);
		for (int dy = -r; dy <= r; ++dy) {
			int y = cy + dy;
			if (y < 0 || y >= W.h) continue;
			for (int dx = -r; dx <= r; ++dx) {
				int x = cx + dx;
				if (x < 0 || x >= W.w || dx * dx + dy * dy > r * r) continue;
				W.fog[tidx(x, y)] = 2;
			}
		}
	}
}

/* ---------- victoria ---------- */

/* puntuación total de un jugador (economía + militar + tecnología) */
int world_team_score(int p) {
	int ec, mi, te;
	world_score(p, &ec, &mi, &te);
	return ec + mi + te;
}

/* cambia la relación entre dos jugadores (simétrica). Un jugador de la máquina acepta la paz si el otro es más fuerte que él,
 * y la alianza solo si ya son neutrales y su dificultad no es la difícil. Declarar la guerra siempre se acepta. */
int cmd_diplomacy(int from, int to, int level) {
	if (from < 0 || to < 0 || from >= MAX_PLAYERS || to >= MAX_PLAYERS || from == to || !W.pl[from].active || !W.pl[to].active) return 0;
	if (level < DIPL_ENEMY || level > DIPL_ALLY) return 0;
	int cur = W.pl[from].dipl[to];
	if (level == cur) return 1;
	/* D415: la máquina responde según su personalidad y dificultad, la fuerza relativa, las amenazas comunes, los tributos
	 * recibidos y las traiciones que recuerda (ia_responde); su respuesta sale en el chat */
	if (level > cur && W.pl[to].ai && !W.pl[from].ai) {
		char m[112] = "";
		int ok = ia_responde(to, from, level, m, sizeof m);
		if (from == W.human) {
			if (m[0]) world_chat(to, m); else if (!ok) world_msg("Rechazan tu propuesta");
		}
		if (!ok) return 0;
	} else if (level > cur && W.pl[to].ai) {   /* entre máquinas, la regla de siempre */
		int ok = world_team_score(from) >= world_team_score(to);
		if (level == DIPL_ALLY && (cur != DIPL_NEUTRAL || W.pl[to].diff >= 2)) ok = 0;   /* difícil y muy difícil */
		if (!ok) return 0;
	}
	/* romper un pacto con la máquina es una traición que recuerda (la guerra se puede declarar siempre) */
	if (level == DIPL_ENEMY && cur != DIPL_ENEMY && W.pl[to].ai && !W.pl[from].ai) ia_traicion(to, from);
	if (from == W.human) W.prop_nivel[to] = 0;
	W.pl[from].dipl[to] = W.pl[to].dipl[from] = level;
	if (from == W.human || to == W.human) {
		char m[96];
		snprintf(m, sizeof m, "Relación con el jugador %d: %s", (from == W.human ? to : from) + 1, dipl_name(level));
		world_msg(m);
	}
	return 1;
}

/* regala recursos a otro jugador (el mercado se queda un 20%) */
int cmd_tribute(int from, int to, int kind, int amount) {
	if (from < 0 || to < 0 || from >= MAX_PLAYERS || to >= MAX_PLAYERS || from == to || !W.pl[from].active || !W.pl[to].active) return 0;
	if (kind < 0 || kind >= RES_COUNT || amount <= 0 || W.pl[from].dipl[to] == DIPL_ENEMY) return 0;
	if (!count_bld_p(from, E_MARKET)) { if (from == W.human) world_msg("Necesitas un mercado para enviar tributo"); return 0; }
	if (W.pl[from].res[kind] < amount) { if (from == W.human) world_msg("No tienes suficientes recursos"); return 0; }
	W.pl[from].res[kind] -= amount;
	W.pl[to].res[kind] += amount * 80 / 100;
	if (W.pl[to].ai && !W.pl[from].ai) ia_tributo(to, from, kind, amount * 80 / 100);   /* D415: la máquina recuerda lo recibido */
	return 1;
}

/* D415: pide un tributo. A un jugador de la máquina le toca decidir (ia_pide); si acepta, lo envía con el mismo 20 % del mercado */
int cmd_pedir(int from, int to, int kind, int amount) {
	if (from < 0 || to < 0 || from >= MAX_PLAYERS || to >= MAX_PLAYERS || from == to || !W.pl[from].active || !W.pl[to].active) return 0;
	if (kind < 0 || kind >= RES_COUNT || amount <= 0) return 0;
	if (!W.pl[to].ai) { if (from == W.human) world_msg("Ese jugador es una persona: pídeselo por otro medio"); return 0; }
	char m[112] = "";
	int ok = ia_pide(to, from, kind, amount, m, sizeof m);
	if (ok && !cmd_tribute(to, from, kind, amount)) { ok = 0; snprintf(m, sizeof m, "No puedo enviarlo ahora"); }
	if (from == W.human && m[0]) world_chat(to, m);
	return ok;
}

/* ---------- chat y propuestas de la máquina (D415) ---------- */

void world_chat(int de, const char *texto) {
	int k = (int)(W.chat_n++ % ARRAY_LEN(W.chat));
	W.chat[k].de = de;
	snprintf(W.chat[k].texto, sizeof W.chat[k].texto, "%s", texto);   /* la interfaz lo enseña como aviso (ui_hud.c) */
}

/* el jugador de la máquina `de` propone al humano pasar al nivel de relación `nivel`; caduca a los 90 s */
void world_proponer(int de, int nivel) {
	if (de < 0 || de >= MAX_PLAYERS || nivel < DIPL_ENEMY || nivel > DIPL_ALLY) return;
	W.prop_nivel[de] = nivel + 1; W.prop_t[de] = W.time + 90.0f;
}

/* el humano acepta o rechaza la propuesta del jugador `de`; devuelve 1 si la relación cambió */
int cmd_responder_propuesta(int de, int acepta) {
	if (de < 0 || de >= MAX_PLAYERS || !W.prop_nivel[de]) return 0;
	int nivel = W.prop_nivel[de] - 1;
	W.prop_nivel[de] = 0;
	if (!acepta || W.time > W.prop_t[de]) return 0;
	W.pl[W.human].dipl[de] = W.pl[de].dipl[W.human] = nivel;
	char m[96];
	snprintf(m, sizeof m, "Relación con el jugador %d: %s", de + 1, dipl_name(nivel));
	world_msg(m);
	return 1;
}

/* ¿ganan juntos? Siempre el mismo equipo del menú; con la victoria compartida, también los aliados por la diplomacia */
int mismo_bando(int a, int b) {
	if (a < 0 || b < 0 || a >= MAX_PLAYERS || b >= MAX_PLAYERS) return 0;
	if (a == b || W.setup.team[a] == W.setup.team[b]) return 1;
	return W.setup.victoria_compartida && W.pl[a].dipl[b] == DIPL_ALLY;
}

/* fin de la partida: gana el bando del jugador g (todos los que son de su bando, también los que ya cayeron) */
static void terminar(int g) {
	if (W.over) return;
	W.ganadores = 0;
	for (int p = 0; p < MAX_PLAYERS; ++p) if (W.pl[p].active && mismo_bando(p, g)) W.ganadores |= 1u << p;
	W.over = (W.ganadores >> W.human) & 1u ? 1 : 2;
	AOEN_LOG("[AOEN] fin: ganan %#x (compartida=%d) t=%.0f", W.ganadores, W.setup.victoria_compartida, W.time);
	snd_ui(W.over == 1 ? SND_VICTORY : SND_DEFEAT);
}

/* Regla de conquista (D415): quién sigue «en pie». Un jugador sin ningún edificio (las murallas no cuentan) cae:
 *  - enseguida, si solo le quedan barcos de pesca, mercantes o transportes (no pueden ni luchar ni construir), y
 *  - a los 180 s, si le quedan unidades (un aldeano escondido, tropa varada en un islote): es el tiempo que tarda en
 *    levantar otro edificio; al caer, el resto de sus unidades se rinde y desaparece.
 * Así la partida no se bloquea por unidades inalcanzables. Con un edificio en pie, el contador vuelve a cero. */
#define CAIDA_SIN_EDIFICIOS 180.0f
static float caida_t[MAX_PLAYERS];     /* segundos sin edificios */
static unsigned char tuvo_edif[MAX_PLAYERS], rendido[MAX_PLAYERS];

static void reglas_caida(int has[MAX_PLAYERS]) {
	int bld[MAX_PLAYERS] = { 0 }, activas[MAX_PLAYERS] = { 0 };
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!e->alive || e->owner < 0 || is_resource(e->type) || e->state == ST_DYING) continue;
		if (is_building(e->type)) { if (e->type != E_WALL) ++bld[e->owner]; }
		else if (e->type != E_FISHBOAT && !is_trader(e->type) && e->type != E_TRANSPORT) ++activas[e->owner];
	}
	for (int p = 0; p < MAX_PLAYERS; ++p) {
		if (!W.pl[p].active || rendido[p]) { if (rendido[p]) has[p] = 0; continue; }
		if (bld[p]) { tuvo_edif[p] = 1; caida_t[p] = 0; continue; }
		if (!tuvo_edif[p] || !has[p]) continue;
		int cae = !activas[p];
		if (!cae) {
			caida_t[p] += 1.0f;
			if (caida_t[p] == 1.0f && p == W.human && !W.pl[p].ai) world_msg("¡Sin edificios! Levanta uno en 3 minutos o perderás");
			cae = caida_t[p] >= CAIDA_SIN_EDIFICIOS;
		}
		if (!cae) continue;
		rendido[p] = 1; has[p] = 0;
		for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].owner == p && !is_resource(W.ents[i].type)) ent_kill(&W.ents[i]);
		AOEN_LOG("[AOEN] caida: el jugador %d se queda sin edificios y cae (t=%.0f)", p, W.time);
		if (p != W.human) world_msg("Un jugador se queda sin edificios y cae");
	}
}

static void check_victory(void) {
	int has[MAX_PLAYERS] = { 0 };
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (e->alive && e->owner >= 0 && !is_resource(e->type) && e->state != ST_DYING)
			has[e->owner] = 1;
	}
	reglas_caida(has);

	/* conquista: la partida acaba cuando ha caído alguien y todos los que quedan son de un mismo bando (mismo equipo; con la
	 * victoria compartida, también aliados entre sí). Sin ella, dos aliados de equipos distintos aún tienen que decidirlo. */
	int caidos = 0, primero = -1, decidido = 1;
	for (int p = 0; p < MAX_PLAYERS; ++p) {
		W.pl[p].alive = has[p];
		if (!W.pl[p].active) continue;
		if (!has[p]) { ++caidos; continue; }
		if (primero < 0) primero = p;
		else for (int q = 0; q < p; ++q) if (W.pl[q].active && has[q] && !mismo_bando(p, q)) decidido = 0;
	}
	/* el humano derrotado pierde enseguida; si su civilización la lleva la máquina (espectador), se sigue hasta el final */
	if (!has[W.human] && !W.pl[W.human].ai) { W.ganadores = 0; W.over = 2; snd_ui(SND_DEFEAT); return; }
	if (caidos > 0 && decidido && primero >= 0) { terminar(primero); return; }

	if (W.setup.victory == VIC_RELICS && W.setup.relics_total > 0) {
		/* un equipo con todas las reliquias guardadas en sus templos las conserva 60 s */
		int owner = -1;
		for (int p = 0; p < MAX_PLAYERS && owner < 0; ++p) {
			if (!W.pl[p].active || !has[p]) continue;
			int tot = 0;
			for (int i = 0; i < W.nents; ++i) {
				Entity *e = &W.ents[i];
				if (e->alive && e->type == E_TEMPLE && e->progress >= 1 && e->owner >= 0 && mismo_bando(p, e->owner)) tot += e->relics;
			}
			if (tot >= W.setup.relics_total) owner = p;
		}
		if (owner < 0) { W.relic_t = 0; W.relic_team = -1; }
		else {
			if (W.relic_team < 0 || !mismo_bando(owner, W.relic_team)) { W.relic_team = owner; W.relic_t = 0; world_msg("¡Un equipo tiene todas las reliquias! Hay que conservarlas 60 s"); }
			W.relic_t += 1.0f;
			if (W.relic_t >= 60.0f) terminar(owner);
		}
	}

	if (W.setup.victory == VIC_SCORE && W.time >= W.setup.time_limit * 60.0f) {
		int best = -1, bs = -1;
		for (int p = 0; p < MAX_PLAYERS; ++p) {
			if (!W.pl[p].active || !has[p]) continue;
			int sc = world_team_score(p);
			if (sc > bs) { bs = sc; best = p; }
		}
		if (best >= 0) terminar(best);
		else { W.over = 2; snd_ui(SND_DEFEAT); }
	}
}

/* ---------- creación del mundo ---------- */

static void carve_land(int cx, int cy, int r) {
	for (int y = cy - r; y <= cy + r; ++y)
		for (int x = cx - r; x <= cx + r; ++x)
			if (in_map(x, y) && (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) {
				int i = tidx(x, y);
				if (W.tile[i] >= TILE_WATER) { W.tile[i] = TILE_DESERT; W.variant[i] = (u8)irand(9); }
			}
}

static void gen_terrain(const Setup *s) {
	float freq, water, grass, ring;
	switch (s->map_kind) {
	case MAP_ISLANDS: freq = 7; water = 0.52f; grass = 0.62f; ring = 0.30f; break;
	case MAP_CONTINENTS: freq = 2.5f; water = 0.43f; grass = 0.55f; ring = 0.25f; break;
	case MAP_INLAND: freq = 4; water = 0.10f; grass = 0.50f; ring = 0.10f; break;
	case MAP_HIGHLAND: freq = 6; water = 0.14f; grass = 0.42f; ring = 0.10f; break;
	default: freq = 4; water = 0.30f; grass = 0.50f; ring = 0.20f; break;
	}

	for (int y = 0; y < W.h; ++y) {
		for (int x = 0; x < W.w; ++x) {
			float v = fbm(x * freq / W.w, y * freq / W.w, 5, (unsigned)s->seed);
			float dx = (x - W.w * 0.5f) / (W.w * 0.35f), dy = (y - W.h * 0.5f) / (W.h * 0.35f);
			float d = sqrtf(dx * dx + dy * dy);
			v += ring * MAX(0.0f, 1.0f - fabsf(d - 1.0f) / 0.3f);
			/* agua profunda lejos de la tierra */
			int i = tidx(x, y);
			W.variant[i] = (u8)irand(9);
			if (v > grass) W.tile[i] = TILE_GRASS;
			else if (v > water) W.tile[i] = TILE_DESERT;
			else W.tile[i] = v < water - 0.12f ? TILE_DEEP : TILE_WATER;
		}
	}
}

/* mapas de islas y costeros: cada inicio tiene tierra hasta 12 casillas y una ensenada de agua pegada (sitio para un muelle) */
static void carve_bay(int sx, int sy, int *obx, int *oby) {
	float ax = sx - W.w * 0.5f, ay = sy - W.h * 0.5f, al = sqrtf(ax * ax + ay * ay) + 0.001f;
	float ux = ax / al, uy = ay / al;
	int bx = (int)(sx + ux * 15.5f), by = (int)(sy + uy * 15.5f);
	if (bx < 6 || by < 6 || bx > W.w - 7 || by > W.h - 7) { ux = -ux; uy = -uy; bx = (int)(sx + ux * 15.5f); by = (int)(sy + uy * 15.5f); }
	for (int y = sy - 12; y <= sy + 12; ++y) for (int x = sx - 12; x <= sx + 12; ++x)
		if (in_map(x, y) && (x - sx) * (x - sx) + (y - sy) * (y - sy) <= 144 && W.tile[tidx(x, y)] >= TILE_WATER) W.tile[tidx(x, y)] = TILE_DESERT;
	for (int y = by - 5; y <= by + 5; ++y) for (int x = bx - 5; x <= bx + 5; ++x)
		if (in_map(x, y) && (x - bx) * (x - bx) + (y - by) * (y - by) <= 25) W.tile[tidx(x, y)] = TILE_WATER;
	if (obx) *obx = bx;
	if (oby) *oby = by;
}

/* D415: la ensenada de cada jugador comunica con el mar. Si carve_bay la dejó como un estanque (a veces solo la une al mar un
 * paso en diagonal, que los barcos no cruzan), se abre un canal de 3 casillas hasta el agua del mar (la mayor masa de agua) más
 * cercana, sin pasar a menos de 11 casillas de ningún inicio. Sin esto, la flota se quedaba encerrada y la partida no acababa. */
static void conectar_bahias(const int *sx, const int *sy, const int *bx, const int *by, int np) {
	size_t n = (size_t)W.w * W.h;
	unsigned short *c = (unsigned short *)calloc(n, sizeof *c);
	int *cola = (int *)malloc(sizeof(int) * n), *tam = (int *)calloc(n + 2, sizeof(int));
	if (!c || !cola || !tam) { free(c); free(cola); free(tam); return; }
	int ncomp = 0, mayor = 0;
	for (size_t i = 0; i < n; ++i) {
		if (c[i] || W.tile[i] < TILE_WATER) continue;
		++ncomp;
		int ini = 0, fin = 0; cola[fin++] = (int)i; c[i] = (unsigned short)ncomp;
		while (ini < fin) {
			int k = cola[ini++], x = k % W.w, y = k / W.w;
			static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
			for (int d = 0; d < 4; ++d) {
				int nx = x + dx[d], ny = y + dy[d];
				if (!in_map(nx, ny)) continue;
				int j = ny * W.w + nx;
				if (!c[j] && W.tile[j] >= TILE_WATER) { c[j] = (unsigned short)ncomp; cola[fin++] = j; }
			}
		}
		tam[ncomp] = fin;
		if (!mayor || tam[ncomp] > tam[mayor]) mayor = ncomp;
	}
	for (int p = 0; p < np; ++p) {
		if (!in_map(bx[p], by[p]) || c[by[p] * W.w + bx[p]] == mayor) continue;
		int mejor = -1; float md = 1e18f;
		for (int y = 0; y < W.h; ++y)
			for (int x = 0; x < W.w; ++x)
				if (c[y * W.w + x] == mayor) { float d = (float)((x - bx[p]) * (x - bx[p]) + (y - by[p]) * (y - by[p])); if (d < md) { md = d; mejor = y * W.w + x; } }
		if (mejor < 0) continue;
		float tx = (float)(mejor % W.w), ty = (float)(mejor / W.w), l = sqrtf(md) + 0.001f;
		for (float t = 0; t <= l; t += 0.5f) {
			int x = (int)(bx[p] + (tx - bx[p]) * t / l), y = (int)(by[p] + (ty - by[p]) * t / l);
			int cerca = 0;
			for (int q = 0; q < np; ++q) if ((x - sx[q]) * (x - sx[q]) + (y - sy[q]) * (y - sy[q]) < 11 * 11) cerca = 1;
			if (cerca) continue;
			for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) if (in_map(x + dx, y + dy)) W.tile[tidx(x + dx, y + dy)] = TILE_WATER;
		}
		AOEN_LOG("[AOEN] costa: canal de la ensenada del jugador %d al mar (%.0f casillas)", p, l);
	}
	free(c); free(cola); free(tam);
}

/* ¿hay un sitio donde construir un muelle cerca del centro urbano del jugador? */
static int dock_site(int p) {
	for (int i = 0; i < W.nents; ++i) {
		Entity *tc = &W.ents[i];
		if (!tc->alive || tc->type != E_TOWN_CENTER || tc->owner != p) continue;
		for (int y = tc->by - 14; y <= tc->by + 14; ++y) for (int x = tc->bx - 14; x <= tc->bx + 14; ++x)
			if (can_place(E_DOCK, x, y)) return 1;
	}
	return 0;
}

static void place_start(int p, int sx, int sy) {
	carve_land(sx, sy, 9);
	int tc = ent_new(E_TOWN_CENTER, p, (float)sx + 0.5f, (float)sy + 0.5f);
	(void)tc;

	/* aldeanos alrededor del centro urbano */
	int n = W.setup.villagers;
	for (int k = 0; k < n; ++k) {
		float a = (float)k / n * 6.2831853f;
		float x = sx + 0.5f + cosf(a) * 3.2f, y = sy + 0.5f + sinf(a) * 3.2f;
		nearest_walk(&x, &y);
		ent_new(E_VILLAGER, p, x, y);
	}

	/* bayas, árboles, oro y piedra cercanos */
	float a0 = frand() * 6.2831853f;
	float bx = sx + cosf(a0) * 7.0f, by = sy + sinf(a0) * 7.0f;
	for (int k = 0; k < 6; ++k) {
		int x = (int)bx + k % 3 - 1, y = (int)by + k / 3 - 1;
		if (tile_walk(x, y)) ent_new(E_BERRIES, -1, (float)x, (float)y);
	}

	float a1 = a0 + 2.1f;
	for (int k = 0; k < 14; ++k) {
		int x = (int)(sx + cosf(a1) * 9.0f + irand(5) - 2), y = (int)(sy + sinf(a1) * 9.0f + irand(5) - 2);
		if (tile_walk(x, y)) ent_new(E_TREE, -1, (float)x, (float)y);
	}

	float a2 = a0 + 4.2f;
	for (int k = 0; k < 4; ++k) {
		int x = (int)(sx + cosf(a2) * 11.0f) + k % 2, y = (int)(sy + sinf(a2) * 11.0f) + k / 2;
		if (tile_walk(x, y)) ent_new(k < 2 ? E_GOLD : E_STONE, -1, (float)x, (float)y);
	}
}

/* Recursos del mapa (Setup.recursos_mapa: 0 normales, 1 escasos, 2 abundantes). Además de lo de la base (place_start), cada
 * jugador recibe los mismos parches «de expansión» de oro y piedra a la misma distancia, en su lado del mapa y en su isla;
 * luego hay minas y bayas repartidas por el mapa según el tamaño y los jugadores, y en los mapas de islas una isla neutral rica.
 * Cada sitio se busca con reintentos (con límite) hasta encontrar casillas andables. */
static int nivel_recursos(const Setup *s) { return s->recursos_mapa == 1 ? 0 : s->recursos_mapa == 2 ? 2 : 1; }   /* 0 escasos, 1 normales, 2 abundantes */

/* etiqueta de isla de cada casilla (0 agua), por relleno; devuelve el número de islas */
static int etiquetar_islas(unsigned short *lab) {
	int *cola = (int *)malloc(sizeof(int) * (size_t)W.w * W.h);
	if (!cola) return 0;
	memset(lab, 0, sizeof *lab * (size_t)W.w * W.h);
	int n = 0;
	for (int i = 0; i < W.w * W.h; ++i) {
		if (lab[i] || W.tile[i] >= TILE_WATER) continue;
		if (n < 65535) ++n;
		int ini = 0, fin = 0;
		cola[fin++] = i; lab[i] = (unsigned short)n;
		while (ini < fin) {
			int k = cola[ini++], x = k % W.w, y = k / W.w;
			static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
			for (int d = 0; d < 4; ++d) {
				int nx = x + dx[d], ny = y + dy[d];
				if (!in_map(nx, ny)) continue;
				int j = tidx(nx, ny);
				if (!lab[j] && W.tile[j] < TILE_WATER) { lab[j] = (unsigned short)n; cola[fin++] = j; }
			}
		}
	}
	free(cola);
	return n;
}

/* ¿cabe un parche de t casillas en (x,y)? andable, lejos de los inicios y, si se pide, en la isla dada */
static int cabe_parche(int x, int y, int t, const int *sx, const int *sy, int n, float lejos, const unsigned short *lab, int isla) {
	for (int j = 0; j < t; ++j) {
		int xx = x + j % 2, yy = y + j / 2;
		if (xx < 2 || yy < 2 || xx >= W.w - 2 || yy >= W.h - 2 || !tile_walk(xx, yy)) return 0;
		if (lab && isla && lab[tidx(xx, yy)] != isla) return 0;
	}
	for (int p = 0; p < n; ++p)
		if ((x - sx[p]) * (x - sx[p]) + (y - sy[p]) * (y - sy[p]) < lejos * lejos) return 0;
	return 1;
}

static void poner_parche(EntType t, int x, int y, int cnt) {
	for (int j = 0; j < cnt; ++j) {
		int xx = x + j % 2, yy = y + j / 2;
		if (tile_walk(xx, yy)) ent_new(t, -1, (float)xx, (float)yy);
	}
}

static void scatter_resources(const Setup *s, const int *sx, const int *sy, int n) {
	int nivel = nivel_recursos(s);
	static const float umbral_bosque[3] = { 0.62f, 0.58f, 0.54f };
	/* bosques a partir de ruido, con claro alrededor de las posiciones iniciales */
	for (int y = 2; y < W.h - 2; ++y) {
		for (int x = 2; x < W.w - 2; ++x) {
			if (!tile_walk(x, y)) continue;
			int near = 0;
			for (int p = 0; p < n; ++p)
				if ((x - sx[p]) * (x - sx[p]) + (y - sy[p]) * (y - sy[p]) < 11 * 11) near = 1;
			if (near) continue;

			float f = fbm(x * 9.0f / W.w, y * 9.0f / W.w, 3, (unsigned)s->seed + 99u);
			if (f > umbral_bosque[nivel] && frand() < 0.55f)
				ent_new(E_TREE, -1, (float)x, (float)y);
		}
	}

	unsigned short *lab = (unsigned short *)malloc(sizeof(unsigned short) * (size_t)W.w * W.h);
	if (lab && !etiquetar_islas(lab)) { free(lab); lab = NULL; }

	/* parches de expansión de cada jugador: los mismos y a la misma distancia para todos */
	static const int parches[3] = { 1, 1, 2 }, casillas[3] = { 2, 4, 5 };
	const float base_r = W.w >= 72 ? 24.0f : 16.0f;
	for (int p = 0; p < n; ++p) {
		int isla = lab ? lab[tidx(sx[p], sy[p])] : 0;
		/* su lado del mapa: alrededor de la dirección que va del centro al inicio */
		float a0 = atan2f(sy[p] - W.h * 0.5f, sx[p] - W.w * 0.5f);
		for (int k = 0; k < 2 * parches[nivel]; ++k) {
			EntType t = (k & 1) ? E_STONE : E_GOLD;
			int puesto = 0;
			for (int intento = 0; intento < 240 && !puesto; ++intento) {
				/* primero la distancia ideal barriendo el ángulo (el lado propio y luego todo el círculo); solo después se
				 * acerca o aleja de casilla en casilla, para que todos los jugadores lo tengan a una distancia parecida */
				int vuelta = intento / 48, paso = intento % 48;
				float r = base_r + (vuelta % 2 ? 1.0f : -1.0f) * (float)((vuelta + 1) / 2);
				float an = a0 + (k * 1.3f - 0.65f) + (paso % 2 ? 1.0f : -1.0f) * (float)((paso + 1) / 2) * 0.13f;
				int x = (int)(sx[p] + cosf(an) * r), y = (int)(sy[p] + sinf(an) * r);
				if (cabe_parche(x, y, casillas[nivel], sx, sy, n, 14.0f, lab, isla)) {
					poner_parche(t, x, y, casillas[nivel]); puesto = 1;
					AOEN_LOG("[AOEN] recursos: parche de %s del jugador %d a %.1f casillas (%d)", t == E_GOLD ? "oro" : "piedra", p + 1,
					         sqrtf((float)((x - sx[p]) * (x - sx[p]) + (y - sy[p]) * (y - sy[p]))), casillas[nivel]);
				}
			}
			if (!puesto) AOEN_LOG("[AOEN] recursos: sin sitio para un parche de %s del jugador %d", t == E_GOLD ? "oro" : "piedra", p + 1);
		}
	}

	/* minas y bayas repartidas por el mapa, según el tamaño y los jugadores (con reintentos) */
	static const float factor[3] = { 0.5f, 1.0f, 2.0f };
	int mines = (int)((W.w * W.h / 2500 + 2 * n) * factor[nivel] + 0.5f);
	int puestas = 0;
	for (int k = 0; k < mines; ++k) {
		EntType t = k % 3 == 0 ? E_GOLD : k % 3 == 1 ? E_STONE : E_BERRIES;
		int cnt = t == E_BERRIES ? 5 : casillas[nivel] < 4 ? 3 : casillas[nivel];
		for (int intento = 0; intento < 40; ++intento) {
			int x = 3 + irand(W.w - 6), y = 3 + irand(W.h - 6);
			if (cabe_parche(x, y, cnt, sx, sy, n, 14.0f, NULL, 0)) { poner_parche(t, x, y, cnt); ++puestas; break; }
		}
	}
	AOEN_LOG("[AOEN] recursos: nivel %d, %d de %d minas y bayas repartidas", nivel, puestas, mines);

	/* islas: una isla sin jugadores (la mayor) recibe oro y piedra de sobra */
	if (lab && s->map_kind == MAP_ISLANDS) {
		int nis = 0;
		for (int i = 0; i < W.w * W.h; ++i) if (lab[i] > nis) nis = lab[i];
		int *tam = (int *)calloc((size_t)nis + 1, sizeof(int));
		if (tam) {
			for (int i = 0; i < W.w * W.h; ++i) ++tam[lab[i]];
			for (int p = 0; p < n; ++p) tam[lab[tidx(sx[p], sy[p])]] = 0;
			tam[0] = 0;
			int mejor = 0;
			for (int k = 1; k <= nis; ++k) if (tam[k] > tam[mejor]) mejor = k;
			if (mejor && tam[mejor] >= 40) {
				int puestos = 0;
				for (int intento = 0; intento < 400 && puestos < 2 + nivel; ++intento) {
					int x = 3 + irand(W.w - 6), y = 3 + irand(W.h - 6);
					if (cabe_parche(x, y, 4, sx, sy, n, 14.0f, lab, mejor)) { poner_parche(puestos % 2 ? E_STONE : E_GOLD, x, y, 4); ++puestos; }
				}
				AOEN_LOG("[AOEN] recursos: isla neutral rica con %d parches", puestos);
			}
			free(tam);
		}
	}
	free(lab);
}

static void spawn_animal(EntType t, float x, float y) {
	nearest_walk(&x, &y);
	if (tile_walk((int)floorf(x), (int)floorf(y))) ent_new(t, -1, x, y);
}

static void place_wildlife(const Setup *s, const int *sx, const int *sy, int np) {
	/* manadas cerca de cada inicio para que haya algo que cazar, y algunas por todo el mapa */
	for (int p = 0; p < np; ++p) {
		float a = frand() * 6.2831853f, r = 15.0f + frand() * 6.0f;
		float cx = sx[p] + cosf(a) * r, cy = sy[p] + sinf(a) * r;
		for (int k = 0; k < 3; ++k) spawn_animal(E_HORSE, cx + k * 0.8f, cy + (k % 2) * 0.8f);
		a += 2.4f; r = 22.0f + frand() * 6.0f;
		spawn_animal(E_ELEPHANT, sx[p] + cosf(a) * r, sy[p] + sinf(a) * r);
	}

	int herds = W.w * W.h / 4000 + 2;
	for (int k = 0; k < herds; ++k) {
		float x = 4 + frand() * (W.w - 8), y = 4 + frand() * (W.h - 8);
		int near = 0;
		for (int p = 0; p < np; ++p)
			if ((x - sx[p]) * (x - sx[p]) + (y - sy[p]) * (y - sy[p]) < 14 * 14) near = 1;
		if (near || !tile_walk((int)x, (int)y)) continue;
		if (k % 3 == 0) spawn_animal(E_ELEPHANT, x, y);
		else for (int j = 0; j < 3; ++j) spawn_animal(E_HORSE, x + j * 0.8f, y);
	}

	/* reliquias: repartidas por el mapa, lejos de las zonas iniciales */
	for (int k = 0, placed = 0; k < 400 && placed < 3 + W.w * W.h / 9000; ++k) {
		float x = 4 + frand() * (W.w - 8), y = 4 + frand() * (W.h - 8);
		int near = 0;
		for (int p = 0; p < np; ++p)
			if ((x - sx[p]) * (x - sx[p]) + (y - sy[p]) * (y - sy[p]) < 18 * 18) near = 1;
		if (near || !tile_walk((int)x, (int)y)) continue;
		if (ent_new(E_RELIC, -1, x, y) >= 0) { ++placed; ++W.setup.relics_total; }
	}

	/* peces a lo largo de la costa: solo casillas de agua junto a tierra, para que los aldeanos pesquen desde la orilla */
	for (int y = 1; y < W.h - 1; ++y) {
		for (int x = 1; x < W.w - 1; ++x) {
			if (W.tile[tidx(x, y)] != TILE_WATER || W.block[tidx(x, y)]) continue;
			if (!(tile_walk(x + 1, y) || tile_walk(x - 1, y) || tile_walk(x, y + 1) || tile_walk(x, y - 1))) continue;
			/* D415: nunca en un canal de una casilla de ancho (tierra a los dos lados): un pez cerraba la salida de las
			 * ensenadas y los barcos de un jugador se quedaban encerrados para siempre */
			#define TIERRA_T(xx, yy) (W.tile[tidx((xx), (yy))] < TILE_WATER)
			int canal = (TIERRA_T(x + 1, y) && TIERRA_T(x - 1, y)) || (TIERRA_T(x, y + 1) && TIERRA_T(x, y - 1));
#undef TIERRA_T
			if (frand() < 0.05f && !canal) ent_new(E_FISH, -1, (float)x, (float)y);
		}
	}
	(void)s;
}

int dev_place(EntType t, int owner, float x, float y) {
	int i = ent_new(t, owner, x, y);
	return i;
}

int world_alloc(int w, int h) {
	size_t n = (size_t)w * h;
	++mundo_gen;   /* invalida las cachés (población, rejilla) */
	W.w = w; W.h = h;
	W.tile = (u8 *)calloc(n, 1); W.variant = (u8 *)calloc(n, 1);
	W.block = (u8 *)calloc(n, 1); W.fog = (u8 *)calloc(n, 1);
	W.pf_g = (float *)calloc(n, sizeof(float));
	W.pf_from = (int *)calloc(n, sizeof(int));
	W.pf_stamp = (int *)calloc(n, sizeof(int));
	W.capents = (int)(n / 4) + 8192;
	W.ents = (Entity *)calloc((size_t)W.capents, sizeof(Entity));
	return W.tile && W.variant && W.block && W.fog && W.pf_g && W.pf_from && W.pf_stamp && W.ents;   /* 0: sin memoria (quien llama libera con world_free) */
}

/* Escenas de prueba (AOEN_ESCENA=nombre, solo para desarrollo y pruebas automáticas):
 *   asedio: dos catapultas y una balista del jugador 0 frente a un cuartel y soldados del jugador 1, con ataque ya ordenado. */
static int dev_tc(int p, float *x, float *y) {
	for (int i = 0; i < W.nents; ++i)
		if (W.ents[i].alive && W.ents[i].type == E_TOWN_CENTER && W.ents[i].owner == p) { *x = W.ents[i].x; *y = W.ents[i].y; return i; }
	return -1;
}

/* una bahía de 21 casillas de agua con tierra a los lados, despejada de árboles (escenas «mar» y «flota») */
static void dev_bahia(float cx, float cy) {
	int x0 = (int)cx - 6, y0 = (int)cy - 8;
	for (int i = 0; i < W.nents; ++i) {   /* despeja árboles y demás de la bahía */
		Entity *r = &W.ents[i];
		if (r->alive && is_resource(r->type) && r->bx >= x0 && r->bx <= x0 + 40 && r->by >= y0 && r->by <= y0 + 18) ent_kill(r);
	}
	for (int y = y0; y <= y0 + 18; ++y) for (int x = x0; x <= x0 + 40; ++x) {
		if (x < 1 || y < 1 || x >= W.w - 1 || y >= W.h - 1) continue;
		int i = tidx(x, y);
		if (x > (int)cx + 4) W.block[i] = 0;
		W.tile[i] = (x < (int)cx + 8 || x > (int)cx + 28) ? TILE_GRASS : TILE_WATER;
	}
}

static char esc_nombre[16];
static int esc_listo;

static void dev_escena(const char *nombre) {
	snprintf(esc_nombre, sizeof esc_nombre, "%s", nombre);
	esc_listo = 0;
	float cx, cy;
	if (dev_tc(0, &cx, &cy) < 0) return;
	/* las escenas son deterministas: sin IA que mueva a los jugadores (la de la v8.5.4.7 defiende su base y lo estropearía) */
	for (int p = 0; p < MAX_PLAYERS; ++p) W.pl[p].ai = 0;
	if (!strcmp(nombre, "asedio")) {
		W.pl[0].age = AGE_IRON; W.pl[1].age = AGE_IRON;
		W.pl[0].techs |= (1u << T_ENGINEERING);
		memset(W.fog, 2, (size_t)W.w * W.h);
		for (int y = (int)cy - 6; y <= (int)cy + 10; ++y) for (int x = (int)cx - 10; x <= (int)cx + 16; ++x)
			if (x > 0 && y > 0 && x < W.w - 1 && y < W.h - 1) { W.tile[tidx(x, y)] = TILE_GRASS; }
		int cu = dev_place(E_BARRACKS, 1, cx + 11, cy + 2);
		int sold[6];
		for (int k = 0; k < 6; ++k) sold[k] = dev_place(E_CLUBMAN, 1, cx + 8 + (k % 3) * 0.6f, cy + 1 + (k / 3) * 0.6f);
		int m[3] = { E_CATAPULT, E_HEAVYCAT, E_BALLISTA };
		for (int k = 0; k < 3; ++k) {
			int u = dev_place(m[k], 0, cx + 1 + k, cy + 5);
			if (u >= 0 && cu >= 0) cmd_target(ent_ref(u), ent_ref(cu));
		}
		for (int k = 0; k < 6; ++k) if (sold[k] >= 0) { W.ents[sold[k]].stance = STANCE_STAND; }
		AOEN_LOG("[AOEN] escena asedio: cuartel %d, soldados 6, máquinas 3", cu);
	} else if (!strcmp(nombre, "mar")) {
		/* una bahía: tierra (aldeanos y soldados), muelle, peces, un barco de pesca y un transporte; al otro lado, otra orilla */
		W.pl[0].age = AGE_BRONZE;
		memset(W.fog, 2, (size_t)W.w * W.h);
		dev_bahia(cx, cy);
		AOEN_LOG("[AOEN] muelle: orilla=%d tierra_adentro=%d", can_place(E_DOCK, (int)cx + 5, (int)cy - 5), can_place(E_DOCK, (int)cx - 4, (int)cy - 5));
		int muelle = dev_place(E_DOCK, 0, cx + 6.5f, cy + 3.5f);
		for (int k = 0; k < 3; ++k) dev_place(E_FISH, -1, cx + 12 + k, cy + 5);
		int pesca = dev_place(E_FISHBOAT, 0, cx + 10.5f, cy + 1.5f);
		int trans = dev_place(E_TRANSPORT, 0, cx + 8.8f, cy + 7.5f);
		int fish = -1;
		for (int i = 0; i < W.nents; ++i) if (W.ents[i].alive && W.ents[i].type == E_FISH) { fish = i; break; }
		if (pesca >= 0 && fish >= 0) cmd_target(ent_ref(pesca), ent_ref(fish));
		for (int k = 0; k < 3; ++k) {
			int u = dev_place(E_CLUBMAN, 0, cx + 7.2f, cy + 8.5f + k * 0.6f);
			if (u >= 0 && trans >= 0) cmd_target(ent_ref(u), ent_ref(trans));
		}
		AOEN_LOG("[AOEN] escena mar: muelle %d, barco de pesca %d, transporte %d", muelle, pesca, trans);
	} else if (!strcmp(nombre, "comercio")) {
		/* una bahía con un muelle de cada jugador en cada orilla y, al sur, un mercado de cada uno unidos por tierra; un barco
		 * y un carro mercantes del jugador 0 con la ruta puesta. AOEN_COMERCIO: oro (por defecto), cambio (da comida, pide
		 * piedra) o espera (da piedra y no la tiene) */
		const char *modo = getenv("AOEN_COMERCIO") ? getenv("AOEN_COMERCIO") : "oro";
		W.pl[0].age = W.pl[1].age = AGE_BRONZE;
		W.setup.comercio_mejorado = strcmp(modo, "oro") != 0;
		memset(W.fog, 2, (size_t)W.w * W.h);
		dev_bahia(cx, cy);
		for (int i = 0; i < W.nents; ++i) {   /* despeja el camino de tierra entre los mercados */
			Entity *r = &W.ents[i];
			if (r->alive && is_resource(r->type) && r->bx >= (int)cx - 6 && r->bx <= (int)cx + 40 && r->by >= (int)cy + 10 && r->by <= (int)cy + 18) ent_kill(r);
		}
		for (int y = (int)cy + 11; y <= (int)cy + 18; ++y) for (int x = (int)cx - 6; x <= (int)cx + 40; ++x)
			if (x > 0 && y > 0 && x < W.w - 1 && y < W.h - 1) { W.tile[tidx(x, y)] = TILE_GRASS; W.block[tidx(x, y)] = 0; }
		for (int y = (int)cy - 8; y <= (int)cy + 10; ++y) for (int x = (int)cx + 29; x <= (int)cx + 34; ++x)
			if (x > 0 && y > 0 && x < W.w - 1 && y < W.h - 1) { W.tile[tidx(x, y)] = TILE_GRASS; W.block[tidx(x, y)] = 0; }
		int m0 = dev_place(E_DOCK, 0, cx + 6.5f, cy + 3.5f), m1 = dev_place(E_DOCK, 1, cx + 30.5f, cy + 3.5f);
		int k0 = dev_place(E_MARKET, 0, cx + 0.5f, cy + 14.5f), k1 = dev_place(E_MARKET, 1, cx + 30.5f, cy + 14.5f);
		int barco = dev_place(E_TRADESHIP, 0, cx + 10.5f, cy + 3.5f), carro = dev_place(E_TRADECART, 0, cx + 3.5f, cy + 14.5f);
		for (int p = 0; p < 2; ++p) { W.pl[p].res[RES_WOOD] = 500; W.pl[p].res[RES_FOOD] = 500; W.pl[p].res[RES_GOLD] = 500; W.pl[p].res[RES_STONE] = 500; }
		int dar = RES_FOOD, recibir = RES_STONE;
		if (!strcmp(modo, "espera")) { dar = RES_STONE; W.pl[0].res[RES_STONE] = 10; recibir = RES_WOOD; }
		if (barco >= 0) { cmd_comercio(ent_ref(barco), strcmp(modo, "oro") != 0, dar, recibir); if (m1 >= 0) cmd_target(ent_ref(barco), ent_ref(m1)); }
		if (carro >= 0) { cmd_comercio(ent_ref(carro), strcmp(modo, "oro") != 0, dar, recibir); if (k1 >= 0) cmd_target(ent_ref(carro), ent_ref(k1)); }
		AOEN_LOG("[AOEN] escena comercio: modo %s, muelles %d %d, mercados %d %d, barco %d, carro %d, jugador 2 tiene %d/%d/%d/%d",
		         modo, m0, m1, k0, k1, barco, carro, W.pl[1].res[0], W.pl[1].res[1], W.pl[1].res[2], W.pl[1].res[3]);
	} else if (!strcmp(nombre, "minimo")) {
		/* una catapulta con un soldado enemigo pegado: debe apartarse antes de disparar */
		memset(W.fog, 2, (size_t)W.w * W.h);
		W.pl[0].age = AGE_IRON;
		for (int y = (int)cy - 6; y <= (int)cy + 8; ++y) for (int x = (int)cx - 10; x <= (int)cx + 16; ++x)
			if (x > 0 && y > 0 && x < W.w - 1 && y < W.h - 1) { W.tile[tidx(x, y)] = TILE_GRASS; W.block[tidx(x, y)] = 0; }
		int u = dev_place(E_CATAPULT, 0, cx + 3, cy + 3);
		int o = dev_place(E_CLUBMAN, 1, cx + 4.2f, cy + 3);
		if (o >= 0) W.ents[o].stance = STANCE_PASSIVE;
		if (u >= 0 && o >= 0) cmd_target(ent_ref(u), ent_ref(o));
		AOEN_LOG("[AOEN] escena minimo: catapulta %d contra %d", u, o);
	} else if (!strcmp(nombre, "flota")) {
		/* un trirreme contra soldados enemigos quietos en la orilla (estando a más alcance que ellos) */
		W.pl[0].age = AGE_BRONZE;
		memset(W.fog, 2, (size_t)W.w * W.h);
		dev_bahia(cx, cy);
		int nave = dev_place(E_WARSHIP, 0, cx + 9.5f, cy + 2.5f);
		int obj = -1;
		for (int k = 0; k < 3; ++k) {
			int u = dev_place(E_CLUBMAN, 1, cx + 6.5f, cy + 2.0f + k * 0.8f);
			if (u >= 0) { W.ents[u].stance = STANCE_STAND; if (obj < 0) obj = u; }
		}
		if (nave >= 0 && obj >= 0) cmd_target(ent_ref(nave), ent_ref(obj));
		AOEN_LOG("[AOEN] escena flota: trirreme %d contra %d", nave, obj);
	} else if (!strcmp(nombre, "muralla") || !strcmp(nombre, "muralla2")) {
		/* una catapulta contra una muralla; «muralla2» con Muralla media y Fortificación del dueño de la muralla */
		memset(W.fog, 2, (size_t)W.w * W.h);
		W.pl[0].age = AGE_IRON;
		for (int y = (int)cy - 6; y <= (int)cy + 8; ++y) for (int x = (int)cx - 10; x <= (int)cx + 16; ++x)
			if (x > 0 && y > 0 && x < W.w - 1 && y < W.h - 1) { W.tile[tidx(x, y)] = TILE_GRASS; W.block[tidx(x, y)] = 0; }
		if (!strcmp(nombre, "muralla2")) W.pl[1].techs |= (1u << T_WALL_MED) | (1u << T_FORTIFICATION);
		int primero = -1;
		for (int k = 0; k < 5; ++k) { int w = dev_place(E_WALL, 1, cx + 10, cy + k); if (primero < 0) primero = w; }
		int u = dev_place(E_CATAPULT, 0, cx + 3, cy + 1);
		if (u >= 0 && primero >= 0) cmd_target(ent_ref(u), ent_ref(primero));
		AOEN_LOG("[AOEN] escena %s: muralla %d", nombre, primero);
	}
}

/* sigue las escenas de prueba: informes y órdenes diferidas */
static void dev_escena_tick(void) {
	if (!esc_nombre[0]) return;
	if (!strcmp(esc_nombre, "mar") && esc_listo < 2 && W.time >= (esc_listo ? 40.0f : 14.0f)) {
		for (int i = 0; i < W.nents; ++i) {
			Entity *b = &W.ents[i];
			if (!b->alive) continue;
			if (b->type == E_TRANSPORT) {
				AOEN_LOG("[AOEN] escena mar: a bordo %d, posición %.1f,%.1f, estado %d", b->ncargo, b->x, b->y, b->state);
				if (!esc_listo && b->ncargo > 0) cmd_move(ent_ref(i), b->x + 20.5f, b->y);
			}
			if (b->type == E_FISHBOAT) AOEN_LOG("[AOEN] escena mar: barco de pesca lleva %d de comida, entregado %d, estado %d", b->carry, W.pl[0].gathered_k[RES_FOOD], b->state);
			if (b->type == E_CLUBMAN && b->owner == 0 && esc_listo) AOEN_LOG("[AOEN] escena mar: soldado en %.1f,%.1f estado %d", b->x, b->y, b->state);
		}
		++esc_listo;
	}
	if (!strcmp(esc_nombre, "minimo") && esc_listo < 2 && W.time >= (esc_listo ? 12.0f : 0.2f)) {
		Entity *c = NULL, *o = NULL;
		for (int i = 0; i < W.nents; ++i) { Entity *e = &W.ents[i]; if (!e->alive) continue; if (e->type == E_CATAPULT) c = e; else if (e->type == E_CLUBMAN && e->owner == 1) o = e; }
		if (c && o) AOEN_LOG("[AOEN] minimo: distancia %s %.2f", esc_listo ? "final" : "inicial", sqrtf((c->x - o->x) * (c->x - o->x) + (c->y - o->y) * (c->y - o->y)));
		++esc_listo;
	}
	if ((!strcmp(esc_nombre, "muralla") || !strcmp(esc_nombre, "muralla2")) && !esc_listo && W.time >= 20.0f) {
		esc_listo = 1;
		for (int i = 0; i < W.nents; ++i)
			if (W.ents[i].alive && W.ents[i].type == E_WALL) { AOEN_LOG("[AOEN] %s: pv de la muralla %.0f de %.0f", esc_nombre, W.ents[i].hp, W.ents[i].maxhp); break; }
		/* si no queda ninguna es que cayó: lo dice el registro de asedio */
	}
}

void world_new(const Setup *s) {
	world_free();
	memset(&W, 0, sizeof W);
	memset(caida_t, 0, sizeof caida_t); memset(tuvo_edif, 0, sizeof tuvo_edif); memset(rendido, 0, sizeof rendido);   /* D415 */
	W.setup = *s;
	W.w = CLAMP(s->width, 48, 512);
	W.h = CLAMP(s->height, 48, 512);
	W.rng = (unsigned)s->seed * 2654435761u + 12345u;
	if (!W.rng) W.rng = 1;

	size_t n = (size_t)W.w * W.h;
	if (!world_alloc(W.w, W.h)) { world_free(); return; }
	W.human = 0;

	gen_terrain(s);

	int np = CLAMP(s->players, 1, MAX_PLAYERS);
	int sx[MAX_PLAYERS], sy[MAX_PLAYERS];
	float a0 = frand() * 6.2831853f;

	for (int p = 0; p < np; ++p) {
		float a = a0 + (float)p / np * 6.2831853f;
		float rx = W.w * 0.35f, ry = W.h * 0.35f;
		int x = (int)(W.w * 0.5f + cosf(a) * rx), y = (int)(W.h * 0.5f + sinf(a) * ry);
		sx[p] = CLAMP(x, 14, W.w - 15); sy[p] = CLAMP(y, 14, W.h - 15);
		W.pl[p].active = 1;
		W.pl[p].ai = p != 0;
		W.pl[p].team = s->team[p];
		W.pl[p].civ = CLAMP(s->civ[p], 0, NUM_CIVS - 1);
		for (int k = 0; k < 4; ++k) W.pl[p].price[k] = 100;
		for (int k = 0; k < RES_COUNT; ++k) W.pl[p].res[k] = s->start_res[k];
		W.pl[p].res[RES_WOOD] += civ_def[W.pl[p].civ].start_wood;
		W.pl[p].res[RES_FOOD] += civ_def[W.pl[p].civ].start_food;
		W.pl[p].res[RES_GOLD] += civ_def[W.pl[p].civ].start_gold;
		W.pl[p].ai_t = 1.0f + p * 0.3f;
		W.pl[p].age = AGE_STONE;
		W.pl[p].age_t = -1;
		W.pl[p].diff = CLAMP(s->ai_diff[p], 0, 3);   /* 3 = muy difícil (ia.c) */
		W.pl[p].ai_t = 1.0f + p * 0.3f;
	}
	/* diplomacia inicial: mismo equipo = aliados, resto = enemigos */
	for (int p = 0; p < np; ++p)
		for (int q = 0; q < np; ++q)
			W.pl[p].dipl[q] = p == q || s->team[p] == s->team[q] ? DIPL_ALLY : DIPL_ENEMY;
	W.setup.relics_total = 0; W.relic_team = -1;
	W.setup.victory = CLAMP(s->victory, 0, VIC_COUNT - 1);
	W.setup.time_limit = s->time_limit > 0 ? CLAMP(s->time_limit, 5, 240) : 30;

	/* excava todos los inicios antes de colocar nada, para que los vecinos no se inunden entre sí */
	for (int p = 0; p < np; ++p) carve_land(sx[p], sy[p], 9);
	if (s->map_kind == MAP_ISLANDS || s->map_kind == MAP_COASTAL) {
		int bbx[MAX_PLAYERS], bby[MAX_PLAYERS];
		for (int p = 0; p < np; ++p) carve_bay(sx[p], sy[p], &bbx[p], &bby[p]);
		conectar_bahias(sx, sy, bbx, bby, np);
	}
	for (int p = 0; p < np; ++p) place_start(p, sx[p], sy[p]);
	scatter_resources(s, sx, sy, np);
	place_wildlife(s, sx, sy, np);
	if (s->map_kind == MAP_ISLANDS || s->map_kind == MAP_COASTAL) for (int p = 0; p < np; ++p) AOEN_LOG("[AOEN] costa: el jugador %d tiene sitio para un muelle: %d", p, dock_site(p));

	memset(W.fog, s->explored ? 1 : 0, n);
	update_fog();
	if (getenv("AOEN_ESCENA")) dev_escena(getenv("AOEN_ESCENA"));
}

void world_free(void) {
	for (int i = 0; i < W.nents; ++i) free(W.ents[i].path);
	free(W.ents); free(W.tile); free(W.variant); free(W.block); free(W.fog);
	free(W.pf_g); free(W.pf_from); free(W.pf_stamp);
	memset(&W, 0, sizeof W); memset(caida_t, 0, sizeof caida_t); memset(tuvo_edif, 0, sizeof tuvo_edif); memset(rendido, 0, sizeof rendido);
}

void world_update(float dt) {
	if (W.over) return;
	W.time += dt;
	if (W.msg_t > 0) W.msg_t -= dt;

	for (int p = 0; p < MAX_PLAYERS; ++p) {
		W.pl[p].pop = 0;
		W.pl[p].popcap = world_pop_cap(p);
	}
	rejilla_rehacer();

	int n = W.nents;
	for (int i = 0; i < n; ++i) {
		Entity *e = &W.ents[i];
		if (!e->alive) continue;
		if (is_unit(e->type)) {
			if (e->state != ST_DYING && e->owner >= 0) ++W.pl[e->owner].pop;
			update_unit(e, dt);
		} else if (is_building(e->type)) {
			update_building(e, dt);
		}
	}

	separate_units();

	for (int k = 0; k < W.npings; ) {
		W.pings[k].t += dt;
		if (W.pings[k].t > 6.0f) W.pings[k] = W.pings[--W.npings];
		else ++k;
	}

	for (int k = 0; k < W.nshots; ) {
		W.shots[k].t += dt * 4;
		if (W.shots[k].t >= 1) W.shots[k] = W.shots[--W.nshots];
		else ++k;
	}

	for (int p = 0; p < MAX_PLAYERS; ++p) {
		Player *pl = &W.pl[p];
		if (pl->age_t >= 0 && pl->active) {
			pl->age_t += dt;
			if (pl->age_t >= age_def[pl->age + 1].time) {
				++pl->age;
				pl->age_t = -1;
				if (p == W.human) { char b[96]; snprintf(b, sizeof b, "¡%s alcanzada!", age_def[pl->age].name); world_msg(b); snd_ui(SND_AGE); }
			}
		}
	}

	{
		PERF_INI();
		ia_actualizar(dt); /* inteligencia artificial de la máquina: ia.c */
		PERF_FIN(PERF_IA);
	}

	/* gráfica de puntuación: una muestra cada 15 s (los últimos 60 minutos) */
	if ((W.hist_t -= dt) <= 0) {
		W.hist_t = 15.0f;
		for (int p = 0; p < MAX_PLAYERS; ++p) {
			Player *pl = &W.pl[p];
			if (!pl->active) continue;
			if (pl->hist_n >= 240) { memmove(pl->hist, pl->hist + 1, sizeof(unsigned short) * 239); pl->hist_n = 239; }
			pl->hist[pl->hist_n++] = (unsigned short)MIN(65535, world_team_score(p));
		}
	}

	dev_escena_tick();
	static float fog_t, win_t;
	if ((fog_t -= dt) <= 0) { fog_t = 0.15f; update_fog(); }
	if ((win_t -= dt) <= 0) { win_t = 1.0f; check_victory(); }
}

/* ---------- suma del estado del mundo (D393) ----------
 * Para comprobar que la partida es determinista: con la misma semilla y los mismos pasos, la suma tiene que salir igual
 * con 1 hilo que con N (y en el orden que sea en que piensen las IA). Entra todo lo que decide la partida: entidades
 * (posición, vida, estado, órdenes y caminos), jugadores (recursos, edad, tecnologías, diplomacia), el reloj y el azar. */
static unsigned suma_u(unsigned h, unsigned v) {
	for (int k = 0; k < 4; ++k) { h = (h ^ (v & 255u)) * 16777619u; v >>= 8; }
	return h;
}
static unsigned suma_f(unsigned h, float f) { unsigned v; memcpy(&v, &f, 4); return suma_u(h, v); }

unsigned world_suma(void) {
	unsigned h = 2166136261u;
	h = suma_f(h, W.time); h = suma_u(h, W.rng); h = suma_u(h, (unsigned)W.nents); h = suma_u(h, (unsigned)W.over);
	for (int i = 0; i < W.nents; ++i) {
		const Entity *e = &W.ents[i];
		h = suma_u(h, (unsigned)e->alive); h = suma_u(h, (unsigned)e->gen);
		if (!e->alive) continue;
		h = suma_u(h, (unsigned)e->type); h = suma_u(h, (unsigned)e->owner);
		h = suma_f(h, e->x); h = suma_f(h, e->y); h = suma_f(h, e->hp); h = suma_u(h, (unsigned)e->state);
		h = suma_u(h, (unsigned)e->target.idx); h = suma_u(h, (unsigned)e->target.gen); h = suma_f(h, e->tx); h = suma_f(h, e->ty);
		h = suma_u(h, (unsigned)e->plen); h = suma_u(h, (unsigned)e->pidx);
		for (int k = 0; k < e->plen && e->path; ++k) h = suma_u(h, (unsigned)e->path[k]);
		h = suma_u(h, (unsigned)e->carry_kind); h = suma_u(h, (unsigned)e->carry); h = suma_f(h, e->amount); h = suma_f(h, e->progress);
		h = suma_u(h, (unsigned)e->ntrain); for (int k = 0; k < e->ntrain; ++k) h = suma_u(h, (unsigned)e->train[k]);
		h = suma_u(h, (unsigned)e->tech_cur); h = suma_u(h, (unsigned)e->stance); h = suma_u(h, (unsigned)e->nwp); h = suma_u(h, (unsigned)e->patrol);
	}
	for (int p = 0; p < MAX_PLAYERS; ++p) {
		const Player *pl = &W.pl[p];
		for (int k = 0; k < RES_COUNT; ++k) h = suma_u(h, (unsigned)pl->res[k]);
		h = suma_u(h, (unsigned)pl->age); h = suma_u(h, pl->techs); h = suma_f(h, pl->age_t); h = suma_u(h, (unsigned)pl->pop);
		h = suma_u(h, (unsigned)pl->alive); h = suma_u(h, (unsigned)pl->units_killed);
		for (int q = 0; q < MAX_PLAYERS; ++q) h = suma_u(h, (unsigned)pl->dipl[q]);
	}
	return h;
}
