/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Lógica del juego de AoEN: generación del mapa, búsqueda de caminos, unidades, economía, edificios, combate y niebla de guerra. */
#include "game.h"
#include "efectos.h"

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
	{ "Caballo salvaje",30,  0,   0,   0, 0,  0, 3.2f, 0.0f, 0.0f, 0.8f, AGE_STONE,  E_TYPE_COUNT,  0, 0, 0 },
	{ "Elefante salvaje",200,0,   0,   0, 0,  0, 1.8f, 8.0f, 2.0f, 0.9f, AGE_STONE,  E_TYPE_COUNT,  1, 3, 0 },
};

const TechDef tech_def[T_COUNT] = {
	/* nombre                      efecto                                     edificio    edad        madera comida oro piedra tiempo requisito */
	{ "Herramientas",            "Madera +25%",                             E_STORAGE,  AGE_TOOL,   60, 120,  0, 0, 30, -1 },
	{ "Hachas de bronce",        "Madera +25% más",                         E_STORAGE,  AGE_BRONZE, 100, 200, 50, 0, 40, T_TOOLWORKING },
	{ "Minería",                 "Oro y piedra +25%",                       E_STORAGE,  AGE_TOOL,   70, 100,  0, 0, 30, -1 },
	{ "Domesticación",           "Granjas y bayas +25%",                    E_GRANARY,  AGE_TOOL,   50, 200,  0, 0, 30, -1 },
	{ "Arado",                   "Granjas y bayas +25% más",                E_GRANARY,  AGE_BRONZE, 100, 250, 0, 0, 40, T_DOMESTICATION },
	{ "Armadura de cuero",       "Armadura +1 a todas las unidades",        E_STORAGE,  AGE_TOOL,   0, 100, 80, 0, 35, -1 },
	{ "Armadura de escamas",     "Armadura +1 más",                         E_STORAGE,  AGE_BRONZE, 0, 150, 120, 0, 45, T_LEATHER },
	{ "Armas de bronce",         "Ataque cuerpo a cuerpo +2",               E_BARRACKS, AGE_TOOL,   0, 100, 80, 0, 35, -1 },
	{ "Armas de hierro",         "Ataque cuerpo a cuerpo +2 más",           E_BARRACKS, AGE_BRONZE, 0, 200, 160, 0, 45, T_BRONZE_WEAPONS },
	{ "Flechas mejoradas",       "Ataque a distancia +1",                   E_ARCHERY,  AGE_TOOL,   60, 100, 0, 0, 35, -1 },
	{ "Arcos compuestos",        "Ataque a distancia +1 y alcance +1",      E_ARCHERY,  AGE_BRONZE, 100, 200, 0, 0, 45, T_BOW },
	{ "Armadura de caballería",  "Caballería: armadura +1 y +1 a flechas",  E_STABLE,   AGE_TOOL,   0, 150, 100, 0, 40, -1 },
	{ "Medicina",                "Los sacerdotes curan el doble de rápido", E_TEMPLE,   AGE_BRONZE, 0, 0, 200, 0, 40, -1 },
};

const Civ civ_def[NUM_CIVS] = {
	/* nombre      descripción                                              madera comida oro piedra  uhp  bhp  entrenar velocidad ma ra rng sw sf sg estilo */
	{ "Egipcios",  "Oro +25%, edificios +15% de vida",                      { 1.0f, 1.0f, 1.25f, 1.0f }, 1.0f, 1.15f, 1.0f, 1.0f, 0, 0, 0,  0,  0, 100, 0 },
	{ "Griegos",   "Entrenan un 15% más rápido, soldados +10% de vida",     { 1.0f, 1.0f, 1.0f, 1.0f }, 1.1f, 1.0f, 0.85f, 1.0f, 0, 0, 0,  0, 50,   0, 1 },
	{ "Babilonios","Edificios +25% de vida, piedra +20%",                   { 1.0f, 1.0f, 1.0f, 1.2f }, 1.0f, 1.25f, 1.0f, 1.0f, 0, 0, 0, 50,  0,   0, 3 },
	{ "Asirios",   "Arqueros con +1 de alcance y +1 de ataque",             { 1.0f, 1.0f, 1.0f, 1.0f }, 1.0f, 1.0f, 1.0f, 1.0f, 0, 1, 1,  0,  0,   0, 3 },
	{ "Minoicos",  "Comida +20%, empiezan con comida extra",                { 1.0f, 1.2f, 1.0f, 1.0f }, 1.0f, 1.0f, 1.0f, 1.0f, 0, 0, 0,  0,150,   0, 1 },
	{ "Hititas",   "Soldados +10% más rápidos y +1 de ataque cuerpo a cuerpo",{ 1.0f, 1.0f, 1.0f, 1.0f }, 1.0f, 1.0f, 1.0f, 1.1f, 1, 0, 0,  0,  0,   0, 3 },
	{ "Fenicios",  "Madera +20%, empiezan con madera extra",                { 1.2f, 1.0f, 1.0f, 1.0f }, 1.0f, 1.0f, 1.0f, 1.0f, 0, 0, 0,150,  0,   0, 0 },
	{ "Persas",    "Todo se recolecta un 8% más rápido, unidades +5% de vida",{ 1.08f, 1.08f, 1.08f, 1.08f }, 1.05f, 1.0f, 1.0f, 1.0f, 0, 0, 0,  0,  0,   0, 0 },
	{ "Shang",     "Madera y comida +10%",                                  { 1.1f, 1.1f, 1.0f, 1.0f }, 1.0f, 1.0f, 1.0f, 1.0f, 0, 0, 0,  0,  0,   0, 2 },
	{ "Yamato",    "Infantería +2 de ataque, entrenan un 10% más rápido",   { 1.0f, 1.0f, 1.0f, 1.0f }, 1.0f, 1.0f, 0.9f, 1.0f, 2, 0, 0,  0,  0,   0, 2 },
};

const AgeDef age_def[AGE_COUNT] = {
	{ "Edad de Piedra",       0,    0,  0 },
	{ "Edad de Herramientas", 500,  0, 30 },
	{ "Edad del Bronce",      800, 450, 45 },
	{ "Edad del Hierro",     1000, 800, 60 },
};

#define CARRY_CAP 10
#define GATHER_RATE 1.0f

int is_building(EntType t) { return t >= E_TOWN_CENTER && t <= E_WONDER; }
int is_unit(EntType t) { return t >= E_VILLAGER && t <= E_ELEPHANT; }
static int is_animal(EntType t) { return t == E_HORSE || t == E_ELEPHANT; }
int is_resource(EntType t) { return t >= E_BERRIES && t <= E_RELIC; }
int same_team(int a, int b) { return a >= 0 && b >= 0 && W.pl[a].team == W.pl[b].team; }

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

typedef struct { float f; int i; } HeapItem;
static HeapItem *heap; static int heap_n, heap_cap;

static void heap_push(float f, int i) {
	if (heap_n == heap_cap) { heap_cap = heap_cap ? heap_cap * 2 : 4096; heap = (HeapItem *)realloc(heap, sizeof(HeapItem) * (size_t)heap_cap); }
	int k = heap_n++;
	while (k > 0) {
		int p = (k - 1) / 2;
		if (heap[p].f <= f) break;
		heap[k] = heap[p]; k = p;
	}
	heap[k].f = f; heap[k].i = i;
}

static HeapItem heap_pop(void) {
	HeapItem top = heap[0], last = heap[--heap_n];
	int k = 0;
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
static int find_path(Entity *e, int gx0, int gy0, int gx1, int gy1, int inner)
{
	int sx = (int)floorf(e->x), sy = (int)floorf(e->y);
	free(e->path); e->path = NULL; e->plen = e->pidx = 0;
	if (!in_map(sx, sy))
		return 0;

	++W.pf_cur;
	int start = tidx(sx, sy);
	float gcx = (gx0 + gx1) * 0.5f, gcy = (gy0 + gy1) * 0.5f;
	int best = start; float bestd = 1e9f;
	int expanded = 0, found = -1;

	W.pf_stamp[start] = W.pf_cur; W.pf_g[start] = 0; W.pf_from[start] = -1;
	heap_n = 0;
	heap_push(0, start);

	static const int dx[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, dy[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

	while (heap_n > 0 && expanded < 120000) {
		HeapItem it = heap_pop();
		int cur = it.i, cx = cur % W.w, cy = cur / W.w;
		if (it.f > W.pf_g[cur] + 4000) continue;
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
			if (!tile_walk(nx, ny)) continue;
			if (k >= 4 && (!tile_walk(cx + dx[k], cy) || !tile_walk(cx, cy + dy[k]))) continue;

			float ng = W.pf_g[cur] + (k >= 4 ? 1.4142f : 1.0f);
			if (W.pf_stamp[ni] != W.pf_cur || ng < W.pf_g[ni]) {
				W.pf_stamp[ni] = W.pf_cur; W.pf_g[ni] = ng; W.pf_from[ni] = cur;
				float hx = nx - gcx, hy = ny - gcy;
				heap_push(ng + sqrtf(hx * hx + hy * hy), ni);
			}
		}
	}

	int goal = found >= 0 ? found : best;
	int n = 0;
	for (int i = goal; i >= 0 && i != start; i = W.pf_from[i]) ++n;
	if (n == 0)
		return found >= 0;

	e->path = (int *)malloc(sizeof(int) * (size_t)n);
	e->plen = n;
	int k = n - 1;
	for (int i = goal; i >= 0 && i != start; i = W.pf_from[i])
		e->path[k--] = i;
	e->pidx = 0;
	return found >= 0;
}

/* asegura que un objetivo puntual sea transitable: busca la casilla transitable más cercana */
static void nearest_walk(float *x, float *y) {
	int ix = (int)floorf(*x), iy = (int)floorf(*y);
	if (tile_walk(ix, iy)) return;
	for (int r = 1; r < 14; ++r)
		for (int dy = -r; dy <= r; ++dy)
			for (int dx = -r; dx <= r; ++dx)
				if (MAX(abs(dx), abs(dy)) == r && tile_walk(ix + dx, iy + dy)) {
					*x = ix + dx + 0.5f; *y = iy + dy + 0.5f;
					return;
				}
}

static void goto_point(Entity *e, float x, float y) {
	nearest_walk(&x, &y);
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

void cmd_move(Ref r, float x, float y) {
	Entity *e = ent_get(r);
	if (!e || !is_unit(e->type) || e->state == ST_DYING) return;
	e->target = NOREF;
	e->auto_target = 0;
	if (e->stance == STANCE_GUARD) e->stance = STANCE_AGGRESSIVE;
	e->hx = x; e->hy = y;
	goto_point(e, x, y);
}

static int dropsite_accepts(const Entity *b, int kind) {
	if (!is_building(b->type) || b->progress < 1) return 0;
	if (kind == RES_COUNT) return b->type == E_TEMPLE;
	switch (b->type) {
	case E_TOWN_CENTER: return 1;
	case E_GRANARY: return kind == RES_FOOD;
	case E_STORAGE: return kind != RES_FOOD;
	default: return 0;
	}
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
		if (!b->alive || b->owner != u->owner || !dropsite_accepts(b, kind)) continue;
		float d = (b->x - u->x) * (b->x - u->x) + (b->y - u->y) * (b->y - u->y);
		if (d < bd) { bd = d; best = i; }
	}
	return best;
}

static int hostile(int a, int b) {
	return a >= 0 && b >= 0 && a != b && W.pl[a].team != W.pl[b].team;
}

void cmd_target(Ref r, Ref tr) {
	Entity *e = ent_get(r), *t = ent_get(tr);
	if (!e || !t || !is_unit(e->type) || e->state == ST_DYING || t->state == ST_DYING) return;

	e->target = tr;
	e->auto_target = 0;
	if (t->type == E_RELIC) {
		if (e->type != E_PRIEST) { goto_point(e, t->x, t->y); e->target = NOREF; return; }
		e->carry_kind = RES_COUNT; e->carry = 0;
		goto_target(e, t, ST_GATHER);
	} else if (is_resource(t->type)) {
		if (e->type != E_VILLAGER) { goto_point(e, t->x, t->y); e->target = NOREF; return; }
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
		} else if (e->carry > 0 && dropsite_accepts(t, e->carry_kind)) {
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
	return 1;
}

int world_pop_cap(int p) {
	int cap = 0;
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!e->alive || e->owner != p || e->progress < 1) continue;
		if (e->type == E_TOWN_CENTER) cap += 10;
		else if (e->type == E_HOUSE) cap += 5;
	}
	return MIN(cap, W.setup.popcap > 0 ? W.setup.popcap : 200);
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
	if (!e || !t || e == t || !is_unit(e->type) || e->type == E_VILLAGER || !same_team(e->owner, t->owner)) return;
	e->stance = STANCE_GUARD;
	e->target = prot;
	if (e->state != ST_IDLE) { free(e->path); e->path = NULL; e->plen = e->pidx = 0; e->state = ST_IDLE; }
}

void cmd_set_rally(Ref r, float x, float y, Ref target) {
	Entity *b = ent_get(r);
	if (!b || !is_building(b->type)) return;
	b->has_rally = 1; b->rx = x; b->ry = y; b->rtarget = target;
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
	if (u->ranged) d += has_tech(p, T_BOW) + has_tech(p, T_COMPOSITE) + (p >= 0 ? civ_def[W.pl[p].civ].ranged_atk : 0);
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
	return u->range + (u->ranged ? (float)has_tech(p, T_COMPOSITE) + (p >= 0 ? (float)civ_def[W.pl[p].civ].range : 0.0f) : 0.0f);
}

int cmd_research(int p, Ref bref, int tech) {
	Entity *b = ent_get(bref);
	if (p < 0 || p >= MAX_PLAYERS || !b || b->owner != p || b->progress < 1 || tech < 0 || tech >= T_COUNT) return 0;
	const TechDef *td = &tech_def[tech];
	if (td->bld != b->type) return 0;
	if (b->tech_cur) { if (p == W.human) world_msg("Ya se está investigando algo aquí"); return 0; }
	if (has_tech(p, tech)) return 0;
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
	if (e->owner >= 0 && e->type != E_VILLAGER) speed *= civ_def[W.pl[e->owner].civ].speed;
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
		e->x = gx; e->y = gy;
		if (e->path && e->pidx < e->plen) { ++e->pidx; return e->pidx >= e->plen; }
		return 1;
	}

	e->angle = atan2f(dy, dx);
	e->x += dx / d * step;
	e->y += dy / d * step;
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
		if (W.fog[tidx(r->bx, r->by)] == 0 && e->owner == W.human) continue;
		float d = (r->x - e->x) * (r->x - e->x) + (r->y - e->y) * (r->y - e->y);
		if (d < bd && d < 18 * 18) { bd = d; best = i; }
	}
	if (best >= 0) cmd_target(ent_ref((int)(e - W.ents)), ent_ref(best));
	else stop_unit(e);
}

static void start_dying(Entity *e) {
	fx_evento(FX_EV_UNIDAD_MUERE, e);
	snd_event(SND_DIE_UNIT, e->x, e->y);
	if (e->carry_kind == RES_COUNT && e->carry > 0) { ent_new(E_RELIC, -1, e->x, e->y); e->carry = 0; e->carry_kind = RES_WOOD; }
	e->state = ST_DYING;
	e->die_t = 0;
	free(e->path); e->path = NULL; e->plen = 0;
	e->sel = 0;
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
		if (from && hostile(t->owner, from->owner) && t->type != E_VILLAGER && t->type != E_PRIEST && t->stance != STANCE_PASSIVE) {
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

static int nearest_enemy(const Entity *u, float range) {
	int best = -1; float bd = range * range;
	for (int i = 0; i < W.nents; ++i) {
		Entity *t = &W.ents[i];
		if (!t->alive || t->state == ST_DYING || !hostile(u->owner, t->owner) || is_resource(t->type)) continue;
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
		if (tile_walk(tx, ty)) {
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

static void update_unit(Entity *e, float dt) {
	const UnitDef *ud = &unit_def[e->type - E_VILLAGER];
	e->walking = 0;
	e->acting = 0;

	if (e->state == ST_DYING) {
		e->die_t += dt;
		if (e->die_t > 20) ent_kill(e);
		return;
	}

	Entity *t = ent_get(e->target);

	switch (e->state) {
	case ST_IDLE: {
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
		} else if (e->type != E_VILLAGER) {
			Ref me = ent_ref((int)(e - W.ents));
			int en = -1;
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
		if (!d || !dropsite_accepts(d, e->carry_kind)) {
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
				} else goto_target(e, t, ST_ATTACK);
			}
			if (walk_step(e, dt) && dist_to_ent(e, t) > reach + 0.2f) ++e->stuck;
			if (e->stuck > 6) stop_unit(e);
			break;
		}

		e->angle = atan2f(t->y - e->y, t->x - e->x);
		e->timer += dt;
		e->acting = 1;
		e->phase = MIN(1.0f, e->timer / (ud->ranged ? 2.0f : 1.5f));

		if (e->type == E_PRIEST) {
			if (friendly) {
				t->hp = MIN(t->maxhp, t->hp + 4.0f * (has_tech(e->owner, T_MEDICINE) ? 2.0f : 1.0f) * dt);
				e->timer = 0;
			} else if (e->timer >= 5.0f) {
				/* conversión */
				e->timer = 0;
				if (frand() < 0.7f && is_unit(t->type) && t->type != E_PRIEST) {
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

		float cooldown = ud->ranged ? 2.0f : 1.5f;
		if (e->timer >= cooldown) {
			e->timer = 0;
			if (is_resource(t->type)) { stop_unit(e); break; }
			float dmg = eff_attack(e->owner, e->type, is_building(t->type));
			if (ud->ranged) add_shot(e->x, e->y, t->x, t->y);
			snd_event(ud->ranged ? SND_SHOOT : SND_HIT_MELEE, e->x, e->y);
			damage(t, dmg, e, ud->ranged);
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
		if (!a->alive || !is_unit(a->type) || a->state == ST_DYING) continue;
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

					if (tile_walk((int)floorf(a->x + ax), (int)floorf(a->y + ay))) { a->x += ax; a->y += ay; }
					if (tile_walk((int)floorf(b->x + bx), (int)floorf(b->y + by))) { b->x += bx; b->y += by; }
				}
			}
		}
	}
}

static void spawn_near(const Entity *b, EntType t) {
	for (int r = 2; r < 8; ++r) {
		for (int k = 0; k < 24; ++k) {
			float a = (float)k / 24.0f * 6.2831853f;
			float x = b->x + cosf(a) * (b->size * 0.5f + r * 0.5f), y = b->y + sinf(a) * (b->size * 0.5f + r * 0.5f);
			if (tile_walk((int)floorf(x), (int)floorf(y))) {
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

static void update_building(Entity *b, float dt) {
	b->anim += dt * 8;
	b->builders = b->nbuild_now;
	b->nbuild_now = 0;

	if (b->type == E_TOWER && b->progress >= 1) {
		b->timer += dt;
		if (b->timer >= 2.0f) {
			int best = -1; float bd = 8.0f * 8.0f;
			for (int i = 0; i < W.nents; ++i) {
				Entity *t = &W.ents[i];
				if (!t->alive || t->state == ST_DYING || !is_unit(t->type) || !hostile(b->owner, t->owner)) continue;
				float d = (t->x - b->x) * (t->x - b->x) + (t->y - b->y) * (t->y - b->y);
				if (d < bd) { bd = d; best = i; }
			}
			if (best >= 0) {
				b->timer = 0;
				add_shot(b->x, b->y, W.ents[best].x, W.ents[best].y);
				damage(&W.ents[best], 6.0f, b, 1);
			}
		}
	}

	if (b->type == E_WONDER && b->progress >= 1) {
		if (b->timer == 0) world_msg("¡Se ha terminado una Maravilla! Hay que defenderla 10 minutos");
		b->timer += dt;
		if (b->timer >= 600.0f && !W.over) {
			W.over = (b->owner >= 0 && same_team(b->owner, W.human)) ? 1 : 2;
			snd_ui(W.over == 1 ? SND_VICTORY : SND_DEFEAT);
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
		int r = e->type == E_TOWN_CENTER ? 10 : is_building(e->type) ? 7 : 6;
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

/* ---------- inteligencia artificial ---------- */

static int ai_count(int p, EntType t) {
	int n = 0;
	for (int i = 0; i < W.nents; ++i)
		if (W.ents[i].alive && W.ents[i].owner == p && W.ents[i].type == t && W.ents[i].state != ST_DYING) ++n;
	return n;
}

static int ai_find_site(int p, EntType t, float ax, float ay, int rmin, int rmax) {
	int s = bld_def[t].size;
	for (int r = rmin; r <= rmax; ++r) {
		for (int k = 0; k < 8 * r; ++k) {
			float a = (float)k / (8.0f * r) * 6.2831853f + (float)p;
			int tx = (int)(ax + cosf(a) * r), ty = (int)(ay + sinf(a) * r);
			if (can_place(t, tx, ty)) {
				/* mantén un anillo libre para que las unidades puedan rodear */
				int ok = 1;
				for (int j = ty - 1; j <= ty + s && ok; ++j)
					for (int i = tx - 1; i <= tx + s; ++i)
						if (!tile_walk(i, j)) { ok = 0; break; }
				if (ok) return tidx(tx, ty);
			}
		}
	}
	return -1;
}

static int farm_workers(int j) {
	int on = 0;
	for (int q = 0; q < W.nents; ++q)
		if (W.ents[q].alive && W.ents[q].target.idx == j && W.ents[q].state == ST_GATHER) ++on;
	return on;
}

/* envía a un aldeano ocioso al recurso más cercano de un tipo */
static int ai_send_gatherer(int p, int ui, Entity *tc, EntType kind) {
	int best = -1; float bd = 1e9f;
	for (int j = 0; j < W.nents; ++j) {
		Entity *r = &W.ents[j];
		int farm = r->type == E_FARM && r->owner == p && r->progress >= 1;
		if (!r->alive || !(is_resource(r->type) || farm) || res_kind_of(r->type) != res_kind_of(kind) || r->amount <= 0) continue;
		if (farm && farm_workers(j) >= 2) continue;
		float d = (r->x - tc->x) * (r->x - tc->x) + (r->y - tc->y) * (r->y - tc->y);
		if (farm) d *= 0.5f;
		if (d < bd) { bd = d; best = j; }
	}
	if (best < 0) return 0;
	cmd_target(ent_ref(ui), ent_ref(best));
	return 1;
}

static void ai_assign_gatherers(int p, Entity *tc) {
	int cnt[RES_COUNT] = { 0 }, total = 0;
	for (int i = 0; i < W.nents; ++i) {
		Entity *u = &W.ents[i];
		if (!u->alive || u->owner != p || u->type != E_VILLAGER || (u->state != ST_GATHER && u->state != ST_RETURN)) continue;
		++cnt[u->carry_kind];
		++total;
	}

	int age = W.pl[p].age;
	/* reparto deseado de trabajadores: comida, madera, oro, piedra */
	float share[RES_COUNT] = { 0.35f, 0.45f, age >= 1 ? 0.15f : 0.0f, age >= 1 ? 0.05f : 0.0f };

	/* exceso de existencias: pasa trabajadores a lo que escasea (también oro cuando la siguiente edad lo necesita) */
	int need_gold = age + 1 < AGE_COUNT && age_requirements_met(p, age + 1) && W.pl[p].res[RES_GOLD] < age_def[age + 1].gold;
	if (need_gold) { share[RES_GOLD] = 0.35f; share[RES_FOOD] = 0.3f; share[RES_WOOD] = 0.3f; }
	for (int k = 0; k < RES_COUNT; ++k)
		if (W.pl[p].res[k] > 900) share[k] *= 0.3f;
	if (W.pl[p].res[RES_WOOD] < 150) share[RES_WOOD] += 0.2f;

	/* mueve un trabajador de un recurso con exceso */
	if (total > 6) {
		int worst = -1, best = -1;
		for (int k = 0; k < RES_COUNT; ++k) {
			float have = (float)cnt[k] / (float)total;
			if (share[k] > 0 && have > share[k] + 0.2f && (worst < 0 || have - share[k] > (float)cnt[worst] / total - share[worst])) worst = k;
			if (share[k] > 0 && have < share[k] - 0.15f && (best < 0 || share[k] - have > share[best] - (float)cnt[best] / total)) best = k;
		}
		if (worst >= 0 && best >= 0) {
			for (int i = 0; i < W.nents; ++i) {
				Entity *u = &W.ents[i];
				if (!u->alive || u->owner != p || u->type != E_VILLAGER || u->state != ST_GATHER || u->carry_kind != worst || u->carry > 3) continue;
				EntType et = best == RES_FOOD ? E_BERRIES : best == RES_GOLD ? E_GOLD : best == RES_STONE ? E_STONE : E_TREE;
				if (ai_send_gatherer(p, i, tc, et)) { --cnt[worst]; ++cnt[best]; break; }
			}
		}
	}

	for (int i = 0; i < W.nents; ++i) {
		Entity *u = &W.ents[i];
		if (!u->alive || u->owner != p || u->type != E_VILLAGER || u->state != ST_IDLE) continue;

		/* elige el recurso que está más por debajo de su reparto deseado */
		int order[RES_COUNT];
		for (int k = 0; k < RES_COUNT; ++k) order[k] = k;
		for (int a = 0; a < RES_COUNT; ++a)
			for (int b = a + 1; b < RES_COUNT; ++b) {
				float da = share[order[a]] - (float)cnt[order[a]] / (float)MAX(1, total + 1);
				float db = share[order[b]] - (float)cnt[order[b]] / (float)MAX(1, total + 1);
				if (db > da) { int t = order[a]; order[a] = order[b]; order[b] = t; }
			}

		for (int k = 0; k < RES_COUNT; ++k) {
			int kind = order[k];
			if (share[kind] <= 0) continue;
			EntType et = kind == RES_FOOD ? E_BERRIES : kind == RES_GOLD ? E_GOLD : kind == RES_STONE ? E_STONE : E_TREE;
			if (ai_send_gatherer(p, i, tc, et)) { ++cnt[kind]; ++total; break; }
		}
	}
}

static Entity *ai_first(int p, EntType t) {
	for (int i = 0; i < W.nents; ++i)
		if (W.ents[i].alive && W.ents[i].owner == p && W.ents[i].type == t && W.ents[i].progress >= 1 && W.ents[i].state != ST_DYING) return &W.ents[i];
	return NULL;
}

static void ai_think(int p) {
	Player *pl = &W.pl[p];
	Entity *tc = NULL;
	int n[E_TYPE_COUNT] = { 0 }, building = 0, army = 0;

	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (!e->alive || e->owner != p || e->state == ST_DYING) continue;
		++n[e->type];
		if (e->type == E_TOWN_CENTER && !tc) tc = e;
		if (is_building(e->type) && e->progress < 1) ++building;
		if (is_unit(e->type) && e->type != E_VILLAGER && e->type != E_PRIEST) ++army;
	}

	if (!tc) {
		/* sin centro urbano: sigue luchando */
		for (int i = 0; i < W.nents; ++i) {
			Entity *u = &W.ents[i];
			if (u->alive && u->owner == p && is_unit(u->type) && u->type != E_VILLAGER && u->state == ST_IDLE) {
				int en = nearest_enemy(u, 200);
				if (en >= 0) cmd_target(ent_ref(i), ent_ref(en));
			}
		}
		return;
	}

	ai_assign_gatherers(p, tc);

	Ref tcr = ent_ref((int)(tc - W.ents));
	int pop = pl->pop, cap = world_pop_cap(p), nvill = n[E_VILLAGER], age = pl->age;

	/* economía: aldeanos */
	int vill_target = pl->diff == 0 ? 10 + age * 2 : pl->diff == 1 ? 14 + age * 4 : 20 + age * 5;
	if (nvill < vill_target && tc->ntrain < 2 && pop + tc->ntrain < cap) cmd_train(p, tcr, E_VILLAGER);

	/* avanzar de edad */
	if (age + 1 < AGE_COUNT && pl->age_t < 0 && nvill >= 10 + age * 3 && age_requirements_met(p, age + 1))
		cmd_advance_age(p);

	/* construcciones: una cada vez */
	Entity *builder = NULL;
	if (!building) {
		for (int i = 0; i < W.nents && !builder; ++i) {
			Entity *u = &W.ents[i];
			if (u->alive && u->owner == p && u->type == E_VILLAGER && u->state != ST_BUILD && u->carry == 0) builder = u;
		}
	}

	if (builder) {
		EntType wants[12];
		int nw = 0, wood = pl->res[RES_WOOD];
		int berries = 0;
		for (int j = 0; j < W.nents; ++j) {
			Entity *r = &W.ents[j];
			if (r->alive && r->type == E_BERRIES && r->amount > 0 && (r->x - tc->x) * (r->x - tc->x) + (r->y - tc->y) * (r->y - tc->y) < 22 * 22) ++berries;
		}

		if (pop + 3 >= cap && wood >= bld_def[E_HOUSE].wood) wants[nw++] = E_HOUSE;
		if (!n[E_STORAGE] && nvill >= 6 && wood >= bld_def[E_STORAGE].wood) wants[nw++] = E_STORAGE;
		if (!n[E_GRANARY] && nvill >= 8 && wood >= bld_def[E_GRANARY].wood) wants[nw++] = E_GRANARY;
		if (age >= 1 && nvill >= 6 && berries < 4 && n[E_FARM] < 6 && n[E_FARM] < nvill / 3 && wood >= bld_def[E_FARM].wood) wants[nw++] = E_FARM;
		if (!n[E_BARRACKS] && nvill >= 10 && wood >= bld_def[E_BARRACKS].wood) wants[nw++] = E_BARRACKS;
		if (age >= 1 && !n[E_ARCHERY] && nvill >= 11 && wood >= bld_def[E_ARCHERY].wood) wants[nw++] = E_ARCHERY;
		if (age >= 1 && !n[E_STABLE] && nvill >= 12 && wood >= bld_def[E_STABLE].wood) wants[nw++] = E_STABLE;
		if (age >= 2 && !n[E_ACADEMY] && wood >= bld_def[E_ACADEMY].wood) wants[nw++] = E_ACADEMY;
		if (age >= 2 && !n[E_TEMPLE] && wood >= bld_def[E_TEMPLE].wood) wants[nw++] = E_TEMPLE;
		if (age >= 2 && !n[E_GOVERNMENT] && wood >= bld_def[E_GOVERNMENT].wood && pl->res[RES_GOLD] >= bld_def[E_GOVERNMENT].gold) wants[nw++] = E_GOVERNMENT;
		if (age >= 1 && n[E_TOWER] < 2 && wood >= bld_def[E_TOWER].wood && pl->res[RES_STONE] >= bld_def[E_TOWER].stone) wants[nw++] = E_TOWER;
		if (n[E_HOUSE] < 20 && pop + 6 >= cap && wood >= 100 && nw < 12) wants[nw++] = E_HOUSE;

		for (int w = 0; w < nw; ++w) {
			EntType want = wants[w];
			float ax = tc->x, ay = tc->y;
			if (want == E_STORAGE || want == E_GRANARY) {
				float bd = 1e9f;
				for (int j = 0; j < W.nents; ++j) {
					Entity *r = &W.ents[j];
					if (!r->alive || !is_resource(r->type)) continue;
					int k = res_kind_of(r->type);
					if ((want == E_GRANARY) != (k == RES_FOOD)) continue;
					float d = (r->x - tc->x) * (r->x - tc->x) + (r->y - tc->y) * (r->y - tc->y);
					if (d < bd) { bd = d; ax = r->x; ay = r->y; }
				}
			}
			int site = ai_find_site(p, want, ax, ay, 3, want == E_FARM ? 16 : 20);
			if (site < 0 && (want == E_STORAGE || want == E_GRANARY)) site = ai_find_site(p, want, tc->x, tc->y, 3, 20);
			if (site >= 0 && cmd_build(p, ent_ref((int)(builder - W.ents)), want, site % W.w, site / W.w)) break;
		}
	}

	/* tecnologías: una cada vez por edificio, cuando hay dinero sobrante */
	for (int i = 0; i < W.nents; ++i) {
		Entity *b = &W.ents[i];
		if (!b->alive || b->owner != p || b->progress < 1 || b->tech_cur || b->state == ST_DYING) continue;
		if (pl->diff == 0) break; /* fácil: sin tecnologías */
		for (int t = 0; t < T_COUNT; ++t) {
			if (tech_def[t].bld != b->type || has_tech(p, t) || nvill < 12) continue;
			if (pl->res[RES_FOOD] < tech_def[t].food + 150 || pl->res[RES_GOLD] < tech_def[t].gold) continue;
			if (cmd_research(p, ent_ref(i), t)) break;
		}
	}

	/* ahorra para la siguiente edad en lugar de gastarlo todo en soldados */
	int reserve_food = 0, reserve_gold = 0;
	if (age + 1 < AGE_COUNT && pl->age_t < 0 && age_requirements_met(p, age + 1)) {
		reserve_food = age_def[age + 1].food;
		reserve_gold = age_def[age + 1].gold;
	}

	/* producción militar: la mejor unidad que ofrece la edad actual */
	if (nvill >= 10 && pop < cap && pl->res[RES_FOOD] >= reserve_food + 60 && pl->res[RES_GOLD] >= reserve_gold) {
		Entity *b;
		static const struct { EntType bld; EntType units[3]; } prod[] = {
			{ E_BARRACKS, { E_SHORTSWORD, E_AXEMAN, E_CLUBMAN } },
			{ E_ARCHERY,  { E_CHARIOT, E_IMPBOW, E_BOWMAN } },
			{ E_STABLE,   { E_HEAVYCAV, E_CAVALRY, E_SCOUT } },
			{ E_ACADEMY,  { E_HOPLITE, E_HOPLITE, E_HOPLITE } },
		};
		for (int k = 0; k < 4; ++k) {
			b = ai_first(p, prod[k].bld);
			if (!b || b->ntrain >= 2) continue;
			for (int u = 0; u < 3; ++u) {
				EntType ut = prod[k].units[u];
				if (unit_def[ut - E_VILLAGER].age <= age && cmd_train(p, ent_ref((int)(b - W.ents)), ut)) break;
			}
		}
		b = ai_first(p, E_TEMPLE);
		if (b && n[E_PRIEST] < 2 && b->ntrain < 1) cmd_train(p, ent_ref((int)(b - W.ents)), E_PRIEST);
	}

	/* los sacerdotes recogen reliquias cuando hay un templo donde guardarlas */
	if (ai_first(p, E_TEMPLE)) {
		for (int i = 0; i < W.nents; ++i) {
			Entity *pr = &W.ents[i];
			if (!pr->alive || pr->owner != p || pr->type != E_PRIEST || pr->state != ST_IDLE || pr->carry) continue;
			int best = -1; float bd = 1e9f;
			for (int j = 0; j < W.nents; ++j) {
				Entity *r = &W.ents[j];
				if (!r->alive || r->type != E_RELIC) continue;
				float d = (r->x - pr->x) * (r->x - pr->x) + (r->y - pr->y) * (r->y - pr->y);
				if (d < bd) { bd = d; best = j; }
			}
			if (best >= 0) cmd_target(ent_ref(i), ent_ref(best));
		}
	}

	int wave = (pl->diff == 0 ? 14 : pl->diff == 1 ? 8 : 6) + pl->ai_attack_wave * 4;
	if (army >= wave) {
		int en = -1; float bd = 1e9f;
		for (int i = 0; i < W.nents; ++i) {
			Entity *t = &W.ents[i];
			if (!t->alive || t->state == ST_DYING || !hostile(p, t->owner) || is_resource(t->type)) continue;
			float d = (t->x - tc->x) * (t->x - tc->x) + (t->y - tc->y) * (t->y - tc->y);
			if (t->type == E_TOWN_CENTER) d *= 0.5f;
			if (d < bd) { bd = d; en = i; }
		}
		if (en >= 0) {
			for (int i = 0; i < W.nents; ++i) {
				Entity *u = &W.ents[i];
				if (u->alive && u->owner == p && is_unit(u->type) && u->type != E_VILLAGER && u->state == ST_IDLE)
					cmd_target(ent_ref(i), ent_ref(en));
			}
			++pl->ai_attack_wave;
		}
	}
}

/* ---------- victoria ---------- */

static void check_victory(void) {
	int has[MAX_PLAYERS] = { 0 };
	for (int i = 0; i < W.nents; ++i) {
		Entity *e = &W.ents[i];
		if (e->alive && e->owner >= 0 && !is_resource(e->type) && e->state != ST_DYING)
			has[e->owner] = 1;
	}

	int enemies = 0;
	for (int p = 0; p < MAX_PLAYERS; ++p) {
		W.pl[p].alive = has[p];
		if (W.pl[p].active && p != W.human && W.pl[p].team != W.pl[W.human].team && has[p]) ++enemies;
	}

	int others = 0;
	for (int p = 0; p < MAX_PLAYERS; ++p)
		if (W.pl[p].active && p != W.human && W.pl[p].team != W.pl[W.human].team) ++others;

	if (!has[W.human]) { W.over = 2; snd_ui(SND_DEFEAT); }
	else if (others > 0 && enemies == 0) { W.over = 1; snd_ui(SND_VICTORY); }
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

static void scatter_resources(const Setup *s, const int *sx, const int *sy, int n) {
	/* bosques a partir de ruido, con claro alrededor de las posiciones iniciales */
	for (int y = 2; y < W.h - 2; ++y) {
		for (int x = 2; x < W.w - 2; ++x) {
			if (!tile_walk(x, y)) continue;
			int near = 0;
			for (int p = 0; p < n; ++p)
				if ((x - sx[p]) * (x - sx[p]) + (y - sy[p]) * (y - sy[p]) < 11 * 11) near = 1;
			if (near) continue;

			float f = fbm(x * 9.0f / W.w, y * 9.0f / W.w, 3, (unsigned)s->seed + 99u);
			if (f > 0.58f && frand() < 0.55f)
				ent_new(E_TREE, -1, (float)x, (float)y);
		}
	}

	/* unas pocas minas y parches de bayas extra */
	int mines = W.w * W.h / 2500 + 2;
	for (int k = 0; k < mines; ++k) {
		int x = 3 + irand(W.w - 6), y = 3 + irand(W.h - 6);
		int near = 0;
		for (int p = 0; p < n; ++p)
			if ((x - sx[p]) * (x - sx[p]) + (y - sy[p]) * (y - sy[p]) < 12 * 12) near = 1;
		if (near || !tile_walk(x, y)) continue;
		EntType t = k % 3 == 0 ? E_GOLD : k % 3 == 1 ? E_STONE : E_BERRIES;
		int cnt = t == E_BERRIES ? 5 : 4;
		for (int j = 0; j < cnt; ++j) {
			int xx = x + j % 2, yy = y + j / 2;
			if (tile_walk(xx, yy)) ent_new(t, -1, (float)xx, (float)yy);
		}
	}
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
		if (ent_new(E_RELIC, -1, x, y) >= 0) ++placed;
	}

	/* peces a lo largo de la costa: solo casillas de agua junto a tierra, para que los aldeanos pesquen desde la orilla */
	for (int y = 1; y < W.h - 1; ++y) {
		for (int x = 1; x < W.w - 1; ++x) {
			if (W.tile[tidx(x, y)] != TILE_WATER || W.block[tidx(x, y)]) continue;
			if (!(tile_walk(x + 1, y) || tile_walk(x - 1, y) || tile_walk(x, y + 1) || tile_walk(x, y - 1))) continue;
			if (frand() < 0.05f) ent_new(E_FISH, -1, (float)x, (float)y);
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

void world_new(const Setup *s) {
	world_free();
	memset(&W, 0, sizeof W);
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
		W.pl[p].diff = CLAMP(s->ai_diff[p], 0, 2);
		W.pl[p].ai_t = 1.0f + p * 0.3f;
	}

	/* excava todos los inicios antes de colocar nada, para que los vecinos no se inunden entre sí */
	for (int p = 0; p < np; ++p) carve_land(sx[p], sy[p], 9);
	for (int p = 0; p < np; ++p) place_start(p, sx[p], sy[p]);
	scatter_resources(s, sx, sy, np);
	place_wildlife(s, sx, sy, np);

	memset(W.fog, s->explored ? 1 : 0, n);
	update_fog();
}

void world_free(void) {
	for (int i = 0; i < W.nents; ++i) free(W.ents[i].path);
	free(W.ents); free(W.tile); free(W.variant); free(W.block); free(W.fog);
	free(W.pf_g); free(W.pf_from); free(W.pf_stamp);
	memset(&W, 0, sizeof W);
}

void world_update(float dt) {
	if (W.over) return;
	W.time += dt;
	if (W.msg_t > 0) W.msg_t -= dt;

	for (int p = 0; p < MAX_PLAYERS; ++p) {
		W.pl[p].pop = 0;
		W.pl[p].popcap = world_pop_cap(p);
	}

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

	for (int p = 0; p < MAX_PLAYERS; ++p) {
		Player *pl = &W.pl[p];
		if (!pl->active || !pl->ai) continue;
		pl->ai_t -= dt;
		if (pl->ai_t <= 0) { pl->ai_t = pl->diff == 0 ? 2.0f : pl->diff == 1 ? 1.0f : 0.6f; ai_think(p); }
	}

	static float fog_t, win_t;
	if ((fog_t -= dt) <= 0) { fog_t = 0.15f; update_fog(); }
	if ((win_t -= dt) <= 0) { win_t = 1.0f; check_victory(); }
}
