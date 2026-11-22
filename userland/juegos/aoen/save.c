/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Partidas guardadas: todo el mundo se escribe en un fichero binario y se vuelve a leer. */
#include "game.h"

#define SAVE_MAGIC "AOENSAV1"
#define SAVE_VERSION 6   /* 6 (D447): Setup gana fuego_amigo; las de formato 5 (Setup de SETUP_TAM_V5 bytes) se siguen cargando con la opción apagada.
                           * 5 (v8.5.4.7): victoria compartida y recursos del mapa en Setup; 4: asedio, diplomacia, victorias, órdenes en cola. Las anteriores se
                           * rechazan como incompatibles (Setup cambia de tamaño) */

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
	if (!rd(f, h, sizeof *h) || memcmp(h->magic, SAVE_MAGIC, 8)) return 0;
	if (!((h->version == SAVE_VERSION && h->sizeof_setup == (int)sizeof(Setup)) || (h->version == 5 && h->sizeof_setup == SETUP_TAM_V5))) return 0;
	if ( h->sizeof_player != (int)sizeof(Player) || h->sizeof_entity != (int)sizeof(Entity)) return 0;
	if (h->w < 48 || h->w > 512 || h->h < 48 || h->h > 512) return 0;
	if (h->capents != (int)((size_t)h->w * h->h / 4) + 8192 || h->nents < 0 || h->nents > h->capents) return 0;
	return 1;
}

int save_info(const char *path, SaveInfo *si) {
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	SaveHeader h;
	Setup s;
	memset(&s, 0, sizeof s);   /* en las de formato 5 los campos nuevos quedan a 0 */
	int ok = read_header(f, &h) && rd(f, &s, h.sizeof_setup);
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
		if (pl->civ < 0 || pl->civ >= NUM_CIVS || pl->age < 0 || pl->age >= AGE_COUNT || pl->diff < 0 || pl->diff > 3) return 0;   /* 3 = muy difícil */
		if (W.setup.civ[p] < 0 || W.setup.civ[p] >= NUM_CIVS || W.setup.ai_diff[p] < 0 || W.setup.ai_diff[p] > 3) return 0;
	}
	if (W.setup.victory < 0 || W.setup.victory >= VIC_COUNT || W.setup.time_limit < 0 || W.setup.time_limit > 240 || W.setup.relics_total < 0 || W.setup.relics_total > 4096) return 0;
	if (W.setup.victoria_compartida < 0 || W.setup.victoria_compartida > 1) return 0;
	if (W.setup.recursos_mapa < 0 || W.setup.recursos_mapa > 2) return 0;
	if (W.setup.comercio_mejorado < 0 || W.setup.comercio_mejorado > 1 || W.setup.fuego_amigo < 0 || W.setup.fuego_amigo > 1) return 0;
	for (int p = 0; p < MAX_PLAYERS; ++p) {
		const Player *pl = &W.pl[p];
		if (W.setup.color[p] < 0 || W.setup.color[p] > MAX_PLAYERS) return 0;
		if (pl->hist_n < 0 || pl->hist_n > 240 || pl->units_lost < 0 || pl->bld_lost < 0) return 0;
		for (int q = 0; q < MAX_PLAYERS; ++q) if (pl->dipl[q] < DIPL_ENEMY || pl->dipl[q] > DIPL_ALLY) return 0;
		for (int k = 0; k < 4; ++k) if (pl->gathered_k[k] < 0) return 0;
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
		if (e->state < ST_IDLE || e->state > ST_ABOARD) return 0;
		if (e->ncargo < 0 || e->ncargo > 5 || e->unload < 0 || e->unload > 1) return 0;
		/* solo los transportes llevan carga, y cada pasajero es una unidad de tierra viva a bordo, distinta del barco y de
		 * los demás pasajeros: sin esto, una carga cruzada o a sí mismo haría que start_dying no terminase */
		if (e->ncargo > 0 && e->type != E_TRANSPORT) return 0;
		for (int k = 0; k < e->ncargo; ++k) {
			int c = e->cargo[k].idx;
			if (c < 0 || c >= W.nents || c == i) return 0;
			const Entity *pc = &W.ents[c];
			if (!pc->alive || !is_unit(pc->type) || is_naval(pc->type) || pc->ncargo != 0) return 0;
			for (int j = 0; j < k; ++j) if (e->cargo[j].idx == c) return 0;
		}
		if (e->state == ST_ABOARD) {
			if (e->target.idx < 0 || !W.ents[e->target.idx].alive || W.ents[e->target.idx].type != E_TRANSPORT) return 0;
		}
		if (e->relics < 0 || e->relics > 64) return 0;
		/* comercio: modos, recursos, fase y puertos (un puerto es un muelle o mercado vivo) */
		if (e->com_modo < 0 || e->com_modo > 1 || e->com_dar < 0 || e->com_dar >= RES_COUNT || e->com_recibir < 0 || e->com_recibir >= RES_COUNT ||
		    e->com_dado < 0 || e->com_dado >= RES_COUNT || e->com_fase < 0 || e->com_fase > 2 || e->com_espera < 0 || e->com_espera > 1) return 0;
		if (e->com_origen.idx < -1 || e->com_origen.idx >= W.nents || e->com_destino.idx < -1 || e->com_destino.idx >= W.nents) return 0;
		if ((e->com_origen.idx >= 0 || e->com_destino.idx >= 0) && !is_trader(e->type)) return 0;
		/* los puertos de la ruta: vivos, del tipo del mercante; el origen suyo y el destino de otro jugador */
		if (is_trader(e->type)) {
			EntType pt = e->type == E_TRADESHIP ? E_DOCK : E_MARKET;
			if (e->com_origen.idx >= 0) { const Entity *o = &W.ents[e->com_origen.idx]; if (!o->alive || o->type != pt || o->owner != e->owner) return 0; }
			if (e->com_destino.idx >= 0) { const Entity *d = &W.ents[e->com_destino.idx]; if (!d->alive || d->type != pt || d->owner < 0 || d->owner == e->owner) return 0; }
		}
		if (is_trader(e->type) && (e->carry < 0 || e->carry > 15)) return 0;
#define EN_MAPA(a, lim) ((a) >= 0 && (a) <= (lim))   /* también rechaza NaN */
		if (!(e->racc >= 0 && e->racc <= 2) || !EN_MAPA(e->hx, W.w) || !EN_MAPA(e->hy, W.h) || !EN_MAPA(e->rx, W.w) || !EN_MAPA(e->ry, W.h) ||
		    !EN_MAPA(e->tx, W.w) || !EN_MAPA(e->ty, W.h) || !EN_MAPA(e->lx, W.w) || !EN_MAPA(e->ly, W.h)) return 0;
#undef EN_MAPA
		if (e->nwp < 0 || e->nwp > 4 || e->patrol < 0 || e->patrol > 2) return 0;
		for (int k = 0; k < 4; ++k) if (!(e->wpx[k] >= 0 && e->wpx[k] <= W.w && e->wpy[k] >= 0 && e->wpy[k] <= W.h)) return 0;
		if (!(e->ptx >= 0 && e->ptx <= W.w && e->pty >= 0 && e->pty <= W.h)) return 0;
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

	ok = ok && rd(f, &W.setup, h.sizeof_setup) && rd(f, W.pl, sizeof W.pl)
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

	W.time = h.time; W.over = h.over; W.human = h.human; W.rng = h.rng; W.relic_team = -1;
	W.setup.width = h.w; W.setup.height = h.h;
	fuego_amigo = W.setup.fuego_amigo;   /* D447: antiguas (formato 5) = apagado */

	/* el mundo anterior ya puede liberarse */
	World loaded = W;
	W = old;
	world_free();
	W = loaded;
	return 1;
}
