/* SPDX-License-Identifier: GPL-3.0-or-later */
/* mapa.c - escenarios de Tzar: cabecera, casillas, objetos y reinos (D417).
 *
 * Traducido de ReTzar, src/map.rs y la lectura de src/world.rs (MissionWorld::read_original, la tabla
 * CLASSES y el módulo `class`) (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later).
 * Por ahora solo lo que hace falta para dibujar el mapa: los guiones, las conversaciones, los sonidos y la
 * diplomacia se leerán en las fases siguientes (docs/tzar.md).
 */
#include "tzar.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Los nombres de las clases del original, en su orden (el número de clase es el índice). */
const char *const tz_classes[144] = {
	"none", "peon", "trader", "footman", "archer", "maceman", "longbowman", "pikeman", "knight", "samurai",
	"horse_archer", "crusader", "monk", "ninja", "spy", "bat", "troll", "mage", "priest", "catapult",
	"hcatapult", "ballista", "carpet", "galleon", "hulk", "xebec", "bear", "dwarf", "ork", "enichar",
	"jinn", "ghost", "dragon", "jihad_warrior", "horse_samurai", "heavy_cavalry", "skeleton", "chariot", "messiah", "tower_stone",
	"tower_wood", "tower_wood2", "tower_stone2", "tower_rocket", "castle", "barrack", "lumber", "farm", "tavern", "church",
	"stable", "storage", "outpost", "market", "harbor", "cathedral", "academy", "shipyard", "blacksmith", "workshop",
	"magetower", "wall_stone", "wall_wood", "field", "bridge", "tcguild", "house", "gate_wood", "gate_stone", "magicworkshop",
	"shaolinn", "mosque", "magicgateway", "evilcastle", "arrow", "cursor", "res_gold", "fog_revealer", "res_stone", "item",
	"resitem", "item_plusplus", "animation", "ruin", "cloud", "open_next_map", "not used 1", "debris", "not used 2", "explosion",
	"command_move", "command_attack", "command_harvest", "command_capture", "command_pickitem", "command_patrol", "cannon", "command_default", "fire_rain", "command_repair",
	"command_guard", "lightning", "treeselection", "item_scroll", "item_magic", "magic", "item_health", "ground_object", "ground_patch", "command_spell",
	"cow", "fish", "bird", "command_unload", "command_trade", "fishpatch", "crusadeanchor", "fakebuilding", "fakeresource", "eagle",
	"bolt", "flame_bolt", "visualobject", "command_explore", "freeze_bolt", "item_soul", "command_murder", "command_impersonate", "command_bribe", "command_board",
	"whirlpool", "sound_object", "flame_ball", "dummy", "rocket_arrow", "item_flag", "fishing_boat", "command_fishing", "command_attackground", "enemy_ground",
	"fakewall", "fakeitem", "rally_flag", "fakegate",
};

enum {
	C_NONE = 0, C_PEON = 1, C_MESSIAH = 38, C_TOWER_STONE = 39, C_GATE_WOOD = 67, C_GATE_STONE = 68, C_EVILCASTLE = 73,
	C_CURSOR = 75, C_FOG_REVEALER = 77, C_ITEM = 79, C_ITEM_PLUSPLUS = 81, C_ANIMATION = 82, C_RUIN = 83, C_OPEN_NEXT_MAP = 85,
	C_ITEM_SCROLL = 103, C_ITEM_MAGIC = 104, C_ITEM_HEALTH = 106, C_GROUND_OBJECT = 107, C_GROUND_PATCH = 108, C_COW = 110,
	C_BIRD = 112, C_FISHPATCH = 115, C_CRUSADEANCHOR = 116, C_FAKEBUILDING = 117, C_FAKERESOURCE = 118, C_EAGLE = 119,
	C_VISUALOBJECT = 122, C_SOUND_OBJECT = 131, C_DUMMY = 133, C_FISHING_BOAT = 136, C_ENEMY_GROUND = 139, C_FAKEWALL = 140,
	C_FAKEITEM = 141, C_FAKEGATE = 143,
};

int tz_classe_e_unidade(int c) { return (c >= C_PEON && c <= C_MESSIAH) || c == C_COW || c == C_BIRD || c == C_EAGLE || c == C_FISHING_BOAT; }
int tz_classe_e_edificio(int c) { return (c >= C_TOWER_STONE && c <= C_EVILCASTLE) || c == C_DUMMY; }

/* Marcas del motor y órdenes, sin dibujo propio. */
int tz_classe_e_marcador(int c)
{
	static const int m[] = { C_NONE, C_CURSOR, C_FOG_REVEALER, C_OPEN_NEXT_MAP, C_FISHPATCH, C_CRUSADEANCHOR, C_FAKEBUILDING,
		C_FAKERESOURCE, C_SOUND_OBJECT, C_ENEMY_GROUND, C_FAKEWALL, C_FAKEITEM, C_FAKEGATE };
	for (unsigned i = 0; i < sizeof m / sizeof *m; ++i) if (m[i] == c) return 1;
	return c >= 0 && c < 144 && !strncmp(tz_classes[c], "command_", 8);
}

/* Las clases derivadas guardan en el registro la clase de la que derivan. */
static int base_guardada(int c)
{
	switch (c) {
	case C_GROUND_OBJECT: case C_GROUND_PATCH: return C_ANIMATION;
	case C_ITEM_PLUSPLUS: case C_ITEM_SCROLL: case C_ITEM_MAGIC: case C_ITEM_HEALTH: return C_ITEM;
	case C_GATE_STONE: return C_GATE_WOOD;
	default: return c;
	}
}

/* Clases cuyo registro nombra en 56 una sección de dibujo propia. */
static int tem_imagem_propria(int c)
{
	return c == C_ITEM_PLUSPLUS || c == C_ITEM_SCROLL || c == C_ITEM_MAGIC || c == C_ITEM_HEALTH || c == C_ITEM ||
	       c == C_ANIMATION || c == C_RUIN || c == C_GROUND_OBJECT || c == C_GROUND_PATCH || c == C_VISUALOBJECT || c == C_SOUND_OBJECT;
}

static void texto(char *dst, size_t n, const uint8_t *src, size_t len)
{
	size_t i = 0;
	while (i < len && src[i] && i + 1 < n) { dst[i] = (char)src[i]; ++i; }
	dst[i] = 0;
	/* sin espacios a los lados */
	while (i && (dst[i - 1] == ' ' || dst[i - 1] == '\t')) dst[--i] = 0;
	size_t a = 0;
	while (dst[a] == ' ' || dst[a] == '\t') ++a;
	if (a) memmove(dst, dst + a, strlen(dst + a) + 1);
}

int tz_mapa_cor(const TzMapa *m, int dono)
{
	if (dono < 1 || dono > 8) return -1;
	int c = dono <= m->nreinos ? m->reinos[dono - 1].cor : 0;
	if (c < 1 || c > 8) c = dono;
	return c - 1;
}

static int ler_objetos(TzMapa *m, const uint8_t *d, uint32_t n, uint32_t esperados)
{
	static const int BASE[7] = { 0, 0, 1, 3, 39, 44, 74 };
	uint32_t a = 0;
	int cap = 0;
	while (a < n) {
		if (a + 9 > n) return tz_falha("registro de objeto truncado");
		uint32_t cod = tz_u32(d + a + 1), len = tz_u32(d + a + 5);
		if (len < 56 || len > 65536) return tz_falha("tamaño de objeto no válido");
		if ((uint64_t)a + 9 + len > n) return tz_falha("registro de objeto truncado");
		const uint8_t *p = d + a + 9;
		uint32_t grupo = cod >> 16;
		if (grupo >= 7) return tz_falha("grupo de objetos desconocido");
		uint32_t classe = (uint32_t)BASE[grupo] + (cod & 0xffff);
		if (classe >= 144 || tz_u16(p + 2) != (uint32_t)base_guardada((int)classe) || tz_u16(p) > len)
			return tz_falha("clase o tamaño del objeto no coinciden");
		if ((uint32_t)m->nobj >= esperados || m->nobj >= 65536) return tz_falha("más objetos de los que dice la cabecera");
		if (m->nobj == cap) {
			cap = cap ? cap * 2 : 256;
			TzObjeto *no = realloc(m->obj, (size_t)cap * sizeof *no);
			if (!no) return tz_falha("sin memoria para los objetos");
			m->obj = no;
		}
		TzObjeto *o = &m->obj[m->nobj];
		memset(o, 0, sizeof *o);
		o->classe = (uint16_t)classe;
		o->flags = tz_u32(p + 4);
		o->x = tz_u16(p + 8);
		o->y = tz_u16(p + 10);
		o->dono = p[12] == 0xff ? 0 : p[12];   /* 0xff = de nadie */
		o->civ = p[13];
		o->id = tz_u16(p + 14);
		o->variante = tz_u16(p + 16);
		o->vida = tz_u16(p + 18);
		o->vida_max = tz_u16(p + 20);
		if (tem_imagem_propria((int)classe) && len > 56) texto(o->imagem, sizeof o->imagem, p + 56, (len < 88 ? len : 88) - 56);
		o->dados = tz_malloc(len);
		if (!o->dados) return -1;
		memcpy(o->dados, p, len);
		o->dados[12] = o->dono;
		o->ndados = len;
		m->nobj++;
		a += 9 + len;
	}
	if ((uint32_t)m->nobj != esperados) return tz_falha("la cabecera dice %lu objetos y hay %d", (unsigned long)esperados, m->nobj);
	return 0;
}

int tz_mapa_ler(TzHpfs *c, TzMapa *m)
{
	memset(m, 0, sizeof *m);
	const TzEntrada *h = NULL;
	for (int i = 0; i < c->n && !h; ++i) {
		size_t l = strlen(c->e[i].nome);
		if (l > 7 && !strcasecmp(c->e[i].nome + l - 7, ".header")) h = &c->e[i];
	}
	if (!h) return tz_falha("el contenedor no tiene ningún escenario (.header)");
	char base[600], nome[640];
	snprintf(base, sizeof base, "%.*s", (int)(strlen(h->nome) - 7), h->nome);
	uint32_t nh, nt, no;
	uint8_t *cab = tz_hpfs_ler(c, h->nome, &nh);
	if (!cab) return -1;
	int r = -1;
	uint8_t *tiles = NULL, *objs = NULL, *rein = NULL;
	if (nh < 0x116 || memcmp(cab, "2mzt", 4)) { tz_falha("cabecera de escenario no admitida"); goto fim; }
	m->lado = tz_u16(cab + 6);
	if (m->lado < 32 || m->lado > 1024) { tz_falha("tamaño de mapa no válido: %d", m->lado); goto fim; }
	m->jogadores[0] = cab[0x4c];
	m->jogadores[1] = cab[0x4d];
	texto(m->bioma, sizeof m->bioma, cab + 0xd6, 0x116 - 0xd6);
	for (char *q = m->bioma; *q; ++q) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
	snprintf(nome, sizeof nome, "%s.tiles", base);
	tiles = tz_hpfs_ler(c, nome, &nt);
	if (!tiles) goto fim;
	if (nt != (uint32_t)m->lado * (uint32_t)m->lado * 2) { tz_falha("el tamaño del mapa no coincide con sus casillas"); goto fim; }
	m->casillas = tz_malloc(nt);
	if (!m->casillas) goto fim;
	for (uint32_t i = 0; i < nt / 2; ++i) m->casillas[i] = tz_u16(tiles + i * 2);
	snprintf(nome, sizeof nome, "%s.objects", base);
	objs = tz_hpfs_ler(c, nome, &no);
	if (!objs) goto fim;
	if (ler_objetos(m, objs, no, tz_u32(cab + 8))) goto fim;
	snprintf(nome, sizeof nome, "%s.kingdominfo", base);
	uint32_t nr;
	if (tz_hpfs_entrada(c, nome) && (rein = tz_hpfs_ler(c, nome, &nr)) != NULL) {
		if (nr != 8 * 152) { tz_falha("registros de reinos no válidos"); goto fim; }
		for (int i = 0; i < 8; ++i) {
			const uint8_t *p = rein + i * 152;
			TzReino *k = &m->reinos[i];
			texto(k->nome, sizeof k->nome, p, 32);
			texto(k->lider, sizeof k->lider, p + 32, 32);
			k->ativo = p[64] != 0;
			k->cor = p[66];
			k->equipe = p[67];
			k->civ = p[88];
			k->ia = p[122] != 0;
			k->ia_modo = p[122] == 2 ? 0 : p[121];
			k->inicio[0] = (int16_t)tz_u16(p + 84) > 0 ? tz_u16(p + 84) : -1;
			k->inicio[1] = (int16_t)tz_u16(p + 86) > 0 ? tz_u16(p + 86) : -1;
			if (k->inicio[0] < 0 || k->inicio[1] < 0) k->inicio[0] = k->inicio[1] = -1;
			texto(k->ia_guiao, sizeof k->ia_guiao, p + 89, 32);
			for (int j = 0; j < 4; ++j) k->recursos[j] = tz_u32(p + 68 + j * 4);
		}
		m->nreinos = 8;
	}
	snprintf(nome, sizeof nome, "%s.diplomacy", base);
	if (tz_hpfs_entrada(c, nome)) {
		uint32_t nd;
		uint8_t *dip = tz_hpfs_ler(c, nome, &nd);
		if (dip && nd >= 64 * 4) {
			for (int i = 0; i < 64; ++i) m->diplomacia[i] = tz_u32(dip + i * 4);
			m->tem_diplomacia = 1;
		}
		free(dip);
	}
	r = 0;
fim:
	free(cab); free(tiles); free(objs); free(rein);
	if (r) {
		char msg[256];
		snprintf(msg, sizeof msg, "%s", tz_erro());
		tz_mapa_liberar(m);
		tz_falha("%s", msg);
	}
	return r;
}

void tz_mapa_liberar(TzMapa *m)
{
	for (int i = 0; i < m->nobj; ++i) free(m->obj[i].dados);
	free(m->obj);
	free(m->casillas);
	memset(m, 0, sizeof *m);
}
