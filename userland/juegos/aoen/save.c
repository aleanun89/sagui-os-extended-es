/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Partidas guardadas: todo el mundo se escribe en un fichero binario y se vuelve a leer. */
#include "game.h"

#define SAVE_MAGIC "AOENSAV1"
#define SAVE_VERSION 3

typedef struct {
	char magic[8];
	int version;
	long long stamp;
	int sizeof_setup, sizeof_player, sizeof_entity;
	int w, h, nents, capents;
	float time;
	int over, human;
	unsigned rng;
} SaveHeader;

static int wr(FILE *f, const void *p, size_t n) { return fwrite(p, 1, n, f) == n; }
static int rd(FILE *f, void *p, size_t n) { return fread(p, 1, n, f) == n; }

int world_save(const char *path, long long stamp) {
	if (!W.w || !W.ents) return 0;

	/* escribe primero en un fichero temporal para que un guardado fallido nunca destruya el anterior */
	char tmp[1200];
	snprintf(tmp, sizeof tmp, "%s.tmp", path);
	FILE *f = fopen(tmp, "wb");
	if (!f) return 0;

	SaveHeader h;
	memset(&h, 0, sizeof h);
	memcpy(h.magic, SAVE_MAGIC, 8);
	h.version = SAVE_VERSION;
	h.stamp = stamp;
	h.sizeof_setup = (int)sizeof(Setup); h.sizeof_player = (int)sizeof(Player); h.sizeof_entity = (int)sizeof(Entity);
	h.w = W.w; h.h = W.h; h.nents = W.nents; h.capents = W.capents;
	h.time = W.time; h.over = W.over; h.human = W.human; h.rng = W.rng;

	size_t n = (size_t)W.w * W.h;
	int ok = wr(f, &h, sizeof h) && wr(f, &W.setup, sizeof W.setup) && wr(f, W.pl, sizeof W.pl)
		&& wr(f, W.tile, n) && wr(f, W.variant, n) && wr(f, W.block, n) && wr(f, W.fog, n);

	for (int i = 0; ok && i < W.nents; ++i) {
		Entity e = W.ents[i];
		int plen = e.alive && e.path ? e.plen : 0;
		e.path = NULL;
		ok = wr(f, &e, sizeof e) && wr(f, &plen, sizeof plen) && (plen == 0 || wr(f, W.ents[i].path, sizeof(int) * (size_t)plen));
	}

	if (fclose(f) != 0) ok = 0;
	if (!ok) { remove(tmp); return 0; }

	remove(path);
	if (rename(tmp, path) != 0) { remove(tmp); return 0; }
	return 1;
}

static int read_header(FILE *f, SaveHeader *h) {
	if (!rd(f, h, sizeof *h) || memcmp(h->magic, SAVE_MAGIC, 8) || h->version != SAVE_VERSION) return 0;
	if (h->sizeof_setup != (int)sizeof(Setup) || h->sizeof_player != (int)sizeof(Player) || h->sizeof_entity != (int)sizeof(Entity)) return 0;
	if (h->w < 48 || h->w > 512 || h->h < 48 || h->h > 512) return 0;
	if (h->capents != (int)((size_t)h->w * h->h / 4) + 8192 || h->nents < 0 || h->nents > h->capents) return 0;
	return 1;
}

int save_info(const char *path, SaveInfo *si) {
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	SaveHeader h;
	Setup s;
	int ok = read_header(f, &h) && rd(f, &s, sizeof s);
	fclose(f);
	if (!ok) return 0;
	si->stamp = h.stamp; si->w = h.w; si->h = h.h; si->players = s.players; si->map_kind = s.map_kind; si->time = h.time;
	si->age = 0;
	return 1;
}

/* Comprueba que lo leído del fichero es coherente antes de usarlo como índice: un .sav manipulado no puede escribir fuera de los
 * arrays. Devuelve 1 si todo está en rango. */
static int world_validate(void) {
	int n = W.w * W.h;
	if (W.nents < 0 || W.nents > W.capents) return 0;
	if (W.human < 0 || W.human >= MAX_PLAYERS || W.over < 0 || W.over > 2) return 0;
	if (W.setup.players < 1 || W.setup.players > MAX_PLAYERS || W.setup.map_kind < 0 || W.setup.map_kind >= MAP_COUNT) return 0;
	for (int p = 0; p < MAX_PLAYERS; ++p) {
		const Player *pl = &W.pl[p];
		if (pl->civ < 0 || pl->civ >= NUM_CIVS || pl->age < 0 || pl->age >= AGE_COUNT || pl->diff < 0 || pl->diff > 2) return 0;
		if (W.setup.civ[p] < 0 || W.setup.civ[p] >= NUM_CIVS || W.setup.ai_diff[p] < 0 || W.setup.ai_diff[p] > 2) return 0;
	}
	if (!W.pl[W.human].active) return 0;
	for (int i = 0; i < n; ++i) if (W.tile[i] > TILE_DEEP || W.fog[i] > 2) return 0;
	for (int i = 0; i < W.nents; ++i) {
		const Entity *e = &W.ents[i];
		if (!e->alive) continue;
		if (e->type < 0 || e->type >= E_TYPE_COUNT) return 0;
		if (e->owner < -1 || e->owner >= MAX_PLAYERS) return 0;
		if (!(e->x >= 0 && e->x <= W.w && e->y >= 0 && e->y <= W.h)) return 0;
		if (e->target.idx < -1 || e->target.idx >= W.nents || e->rtarget.idx < -1 || e->rtarget.idx >= W.nents) return 0;
		if (e->bx < 0 || e->by < 0 || e->size < 0 || e->size > 16 || e->bx + e->size > W.w || e->by + e->size > W.h) return 0;
		if (e->ntrain < 0 || e->ntrain > 5) return 0;
		for (int k = 0; k < e->ntrain; ++k) if (e->train[k] < E_VILLAGER || e->train[k] > E_ELEPHANT) return 0;
		if (e->carry_kind < 0 || e->carry_kind > RES_COUNT || e->stance < 0 || e->stance > STANCE_GUARD) return 0;
		if (e->tech_cur < 0 || e->tech_cur > T_COUNT || e->pidx < 0 || e->plen < 0 || e->pidx > e->plen) return 0;
		if (e->state < ST_IDLE || e->state > ST_DYING) return 0;
		for (int k = 0; k < e->plen; ++k) if (e->path[k] < 0 || e->path[k] >= n) return 0;
	}
	return 1;
}

int world_load(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) return 0;

	SaveHeader h;
	if (!read_header(f, &h)) { fclose(f); return 0; }

	/* lee en un mundo nuevo; solo reemplaza el actual cuando se ha leído todo */
	World old = W;
	memset(&W, 0, sizeof W);
	int ok = world_alloc(h.w, h.h);
	size_t n = (size_t)h.w * h.h;

	ok = ok && rd(f, &W.setup, sizeof W.setup) && rd(f, W.pl, sizeof W.pl)
		&& rd(f, W.tile, n) && rd(f, W.variant, n) && rd(f, W.block, n) && rd(f, W.fog, n);

	W.nents = h.nents;
	for (int i = 0; ok && i < h.nents; ++i) {
		Entity *e = &W.ents[i];
		int plen = 0;
		ok = rd(f, e, sizeof *e) && rd(f, &plen, sizeof plen);
		e->path = NULL;
		if (ok && plen > 0) {
			if (plen > (int)n) { ok = 0; break; }
			e->path = (int *)malloc(sizeof(int) * (size_t)plen);
			ok = e->path && rd(f, e->path, sizeof(int) * (size_t)plen);
			e->plen = plen;
		} else {
			e->plen = 0; e->pidx = 0;
		}
	}
	fclose(f);

	if (ok) { W.human = h.human; W.over = h.over; ok = world_validate(); }
	if (!ok) {
		world_free();
		W = old; /* conserva la partida que estaba en marcha */
		return 0;
	}

	W.time = h.time; W.over = h.over; W.human = h.human; W.rng = h.rng;
	W.setup.width = h.w; W.setup.height = h.h;

	/* el mundo anterior ya puede liberarse */
	World loaded = W;
	W = old;
	world_free();
	W = loaded;
	return 1;
}
