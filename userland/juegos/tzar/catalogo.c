/* SPDX-License-Identifier: GPL-3.0-or-later */
/* catalogo.c - lo que dicen los datos de cada clase: atributos, costes, tiempos, tecnologías, órdenes y bonos (D417).
 *
 * Traducido a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later):
 *  - simulation.rs AttributeCatalog::load (tablas DATA/<civ> UNITS|BUILDINGS.TXT, EMIT);
 *  - gameplay.rs Catalog::load, parse_cost, group/bonus/received (BONUS.INI), TECHNOLOGIES.INI y UPGRADES.INI;
 *  - animation.rs Timeline::directional y gameplay.rs animation_timing/action_ms (tiempos de los dibujos);
 *  - movement.rs picture_step_ms (move_len) y gameplay.rs missile() (proyectiles de cada tirador);
 *  - world/starting.rs picture_cells (huella de los edificios: «object» / 32).
 */
#include "mundo.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *const CIVS[4] = { "comm", "euro", "arab", "asia" };

static void minusc(char *s) { for (; *s; ++s) *s = (char)tolower((unsigned char)*s); }

static char *recorta(char *s)
{
	while (*s == ' ' || *s == '\t' || *s == '"') ++s;
	size_t n = strlen(s);
	while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '"' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = 0;
	return s;
}

int tz_catalogo_classe(const char *nome)
{
	if (!nome) return -1;
	for (int i = 0; i < TZ_NCLASSES; ++i) if (!strcasecmp(tz_classes[i], nome)) return i;
	return -1;
}

/* C atoi: signo opcional y dígitos */
static int atoi_c(const char *s)
{
	while (*s == ' ' || *s == '\t' || *s == '"') ++s;
	int sg = 1;
	if (*s == '-') { sg = -1; ++s; } else if (*s == '+') ++s;
	long n = 0;
	while (isdigit((unsigned char)*s) && n < 0x7fffffff / 10) n = n * 10 + (*s++ - '0');
	return (int)(sg * n);
}

/* gameplay.rs parse_cost: «40f, 20w», «1200w», «25m» (g s f w m) */
static void custo(const char *raw, uint32_t c[TZ_NREC], uint16_t *mana)
{
	uint32_t n = 0;
	int tem = 0;
	memset(c, 0, sizeof(uint32_t) * TZ_NREC);
	*mana = 0;
	for (; *raw; ++raw) {
		if (isdigit((unsigned char)*raw)) { if (n < 100000000u) n = n * 10 + (uint32_t)(*raw - '0'); tem = 1; continue; }
		int i = -1;
		switch (tolower((unsigned char)*raw)) { case 'g': i = 0; break; case 's': i = 1; break; case 'f': i = 2; break;
		case 'w': i = 3; break; case 'm': i = 4; break; }
		if (i >= 0 && tem) { if (i == 4) *mana = (uint16_t)(n > 65535 ? 65535 : n); else c[i] = n; }
		n = 0; tem = 0;
	}
}

/* ------------------------------------------------------------------ tablas TXT */
static void ler_tabela(TzCatalogo *c, TzRecursos *r, int civ, const char *tipo)
{
	char nome[80];
	snprintf(nome, sizeof nome, "DATA/%s %s.TXT", CIVS[civ], tipo);
	for (char *q = nome + 5; *q; ++q) *q = (char)toupper((unsigned char)*q);
	if (!tz_recursos_existe(r, nome)) return;
	uint32_t n;
	uint8_t *d = tz_recursos_ler(r, nome, &n);
	if (!d) return;
	char *txt = tz_malloc(n + 1);
	if (!txt) { free(d); return; }
	memcpy(txt, d, n); txt[n] = 0; free(d);
	char col[24][32]; int ncol = 0;
	char *lin = txt;
	while (lin && *lin) {
		char *fim = strchr(lin, '\n');
		if (fim) *fim++ = 0;
		char *campo[24]; int nc = 0;
		char *p = lin;
		while (nc < 24) {
			campo[nc++] = p;
			char *t = strchr(p, '\t');
			if (!t) break;
			*t = 0; p = t + 1;
		}
		for (int i = 0; i < nc; ++i) campo[i] = recorta(campo[i]);
		if (!strcasecmp(campo[0], "UNIT") || !strcasecmp(campo[0], "BUILDING")) {
			ncol = nc;
			for (int i = 0; i < nc; ++i) { snprintf(col[i], sizeof col[i], "%s", campo[i]); for (char *q = col[i]; *q; ++q) *q = (char)toupper((unsigned char)*q); }
			lin = fim; continue;
		}
		int ic = -1;
		for (int i = 0; i < ncol && i < nc; ++i) if (!strcmp(col[i], "CLASS")) ic = i;
		int k = ic >= 0 ? tz_catalogo_classe(campo[ic]) : -1;
		if (k < 0) { lin = fim; continue; }
		TzAtributos *a = &c->at[civ][k];
		a->existe = 1;
		for (int i = 0; i < ncol && i < nc; ++i) {
			const char *v = campo[i], *h = col[i];
			int num = atoi_c(v);
			if (num < 0) num = 0;
			if (num > 65535) num = 65535;
			if (!strcmp(h, "HEALTH")) a->vida = (uint16_t)num;
			else if (!strcmp(h, "ATTACK")) a->ataque = (uint16_t)num;
			else if (!strcmp(h, "DEFENCE")) a->defesa = (uint16_t)num;
			else if (!strcmp(h, "SIGHT")) a->visao = (uint16_t)num;
			else if (!strcmp(h, "RANGE")) a->alcance = (uint16_t)num;
			else if (!strcmp(h, "RESISTANCE")) a->resistencia = (uint16_t)num;
			else if (!strcmp(h, "SPEED")) a->velocidade = (uint16_t)num;
			else if (!strcmp(h, "RATE")) a->cadencia = (uint16_t)num;
			else if (!strcmp(h, "TIME")) a->tempo_ms = (uint32_t)num * 1000u;
			else if (!strcmp(h, "COST")) custo(v, a->custo, &a->mana);
			else if (!strncmp(h, "DEPEND", 6)) { snprintf(a->depende, sizeof a->depende, "%s", strcmp(v, "*") ? v : ""); minusc(a->depende); }
			else if (!strcmp(h, "BUILD AT") || !strcmp(h, "SPECIAL")) { snprintf(a->especial, sizeof a->especial, "%s", strcmp(v, "*") ? v : ""); minusc(a->especial); }
			else if (!strncmp(h, "EMIT", 4) && *v && strcmp(v, "*")) {
				const char *vg = strchr(v, ',');
				int own = atoi_c(v), other = vg ? atoi_c(vg + 1) : -own;
				a->emite_proprio = (int16_t)own; a->emite_outro = (int16_t)other;
			}
		}
		lin = fim;
	}
	free(txt);
}

/* ------------------------------------------------------------------ propiedades de DATA/OBJECTS/<clase>.INI */
static const TzIni *definicao(const TzBiblioteca *b, int classe)
{
	if (!b) return NULL;
	for (int i = 0; i < b->nobjdefs; ++i) if (!strcmp(b->objclasse[i], tz_classes[classe])) return &b->objdefs[i];
	return NULL;
}

static const char *prop(const TzBiblioteca *b, int classe, int civ, const char *chave)
{
	const TzIni *d = definicao(b, classe);
	if (!d) return NULL;
	const char *v = tz_ini(d, civ == 2 ? "arab" : civ == 3 ? "asia" : "euro", chave);
	if (!v) v = tz_ini(d, "comm", chave);
	if (!v) v = tz_ini(d, "general", chave);
	return v && *v ? v : NULL;
}

/* animation.rs Timeline::directional: duración del ciclo y fin de cada fila */
static uint32_t linha_tempo(const TzIni *img, const char *secao, uint32_t *lead)
{
	const char *s = tz_ini(img, secao, "rows");
	int rows = s ? atoi(s) : 1;
	if (rows < 1) rows = 1;
	if (rows > 256) rows = 256;
	s = tz_ini(img, secao, "anim_time");
	uint32_t anim = s ? (uint32_t)atoi_c(s) : 1000;
	uint32_t por = anim / (uint32_t)rows, total = por * (uint32_t)rows;
	uint32_t d[256]; int nd = 0;
	s = tz_ini(img, secao, "delay");
	if (s) {
		const char *p = s;
		while (*p && nd < 257) {
			while (*p == ' ') ++p;
			if (!*p) break;
			if (nd < 256) d[nd] = (uint32_t)atoi_c(p);
			++nd;
			while (*p && *p != ',') ++p;
		}
	}
	if (nd != rows) for (int i = 0; i < rows; ++i) d[i] = por;
	uint32_t soma = 0;
	for (int i = 0; i < rows; ++i) soma += d[i];
	for (int i = 0; soma < total; i = (i + 1) % rows, ++soma) d[i]++;
	s = tz_ini(img, secao, "actionrow");
	int ar = s ? atoi(s) : rows - 1;
	if (ar < 0 || ar >= rows) ar = rows - 1;
	uint32_t antes = 0;
	for (int i = 0; i < ar; ++i) antes += d[i];
	if (lead) *lead = antes;
	return soma;
}

static uint16_t tempo_imagem(const TzBiblioteca *b, int classe, int civ, const char *chave, uint16_t sem, uint16_t *lead)
{
	const char *sec = prop(b, classe, civ, chave);
	if (lead) *lead = 0;
	if (!sec || !b) return sem;
	uint32_t l = 0, t = linha_tempo(&b->imagens, sec, &l);
	if (t < 1) t = 1;
	if (t > 60000) t = 60000;
	if (lead) *lead = (uint16_t)(l > t ? t : l);
	return (uint16_t)t;
}

/* gameplay.rs missile(): proyectil y velocidad (px/s) de cada tirador */
static void missil(int k, uint8_t *tipo, uint8_t *vel)
{
	int t = 0xff, v = 0;
	switch (k) {
	case 4: case 6: case 10: case 17: case 38: case 39: case 40: case 41: t = 0; v = 500; break;
	case 25: t = 0; v = 600; break;
	case 23: case 24: t = 1; v = 300; break;
	case 19: case 20: t = 1; v = 100; break;
	case 42: t = 1; v = 250; break;
	case 43: t = 1; v = 400; break;
	case 21: t = 2; v = 498; break;
	}
	*tipo = (uint8_t)t; *vel = (uint8_t)(v / 10);
}

/* simulation.rs weapon_category */
static uint8_t categoria(int k)
{
	switch (k) {
	case 4: case 6: case 10: case 21: case 25: case 39: case 40: case 41: return 1;
	case 19: case 20: case 23: case 24: case 42: return 2;
	case 17: case 18: case 32: return 3;
	case 43: return 5;
	}
	return 0;
}

static uint8_t tipo_classe(int k)
{
	if (tz_classe_e_unidade(k)) return TZ_T_UNIDADE;
	if (tz_classe_e_edificio(k)) return TZ_T_EDIFICIO;
	if (k == TZC_RES_GOLD || k == TZC_RES_STONE || k == TZC_FISHPATCH || k == 80) return TZ_T_RECURSO;
	if (k == TZC_ITEM || (k >= 103 && k <= 106) || k == 81 || k == 125 || k == 137) return TZ_T_ITEM;
	if (k == TZC_ARROW || k == 96 || k == 120 || k == 121 || k == 124 || k == 132 || k == 134) return TZ_T_PROJETIL;
	if (tz_classe_e_marcador(k)) return TZ_T_MARCA;
	return TZ_T_DECORADO;
}

static void completar(TzCatalogo *c, const TzBiblioteca *b, int civ, int k)
{
	TzAtributos *a = &c->at[civ][k];
	a->tipo = tipo_classe(k);
	a->categoria_arma = categoria(k);
	missil(k, &a->missil, &a->missil_vel);
	a->naval = (k >= 23 && k <= 25) || k == TZC_FISHING_BOAT;
	a->voa = k == 15 || k == 22 || k == 32 || k == 112 || k == 119;
	const char *v;
	if ((v = prop(b, k, civ, "max_obj_in_holder"))) a->capacidade = (uint16_t)atoi_c(v);
	if ((v = prop(b, k, civ, "pop_count"))) a->pop_conta = (uint16_t)atoi_c(v);
	if ((v = prop(b, k, civ, "max_manna"))) a->mana_max = (uint16_t)atoi_c(v);
	if (!a->existe && (v = prop(b, k, civ, "fog_range"))) a->visao = (uint16_t)atoi_c(v);
	if (!a->visao) a->visao = a->tipo == TZ_T_UNIDADE ? 5 : a->tipo == TZ_T_EDIFICIO ? 4 : 0;
	if ((v = prop(b, k, civ, "menu"))) { snprintf(a->menu, sizeof a->menu, "%s", v); minusc(a->menu); }
	/* tiempos de los dibujos */
	const char *andar = prop(b, k, civ, "image");
	int move_len = 32;
	if (andar && b) { const char *ml = tz_ini(&b->imagens, andar, "move_len"); if (ml && atoi_c(ml) > 0) move_len = atoi_c(ml); }
	uint32_t pms = (uint32_t)((32000 / move_len) >> 5);
	if (pms < 1) pms = 1;
	a->passo_ms = (uint16_t)(pms * 32 > 60000 ? 60000 : pms * 32);
	a->ataque_ms = tempo_imagem(b, k, civ, "image_attack", 1000, &a->ataque_lead_ms);
	a->construir_ms = tempo_imagem(b, k, civ, "image_build", 100, NULL);
	a->minar_ms = tempo_imagem(b, k, civ, "image_mine", 100, NULL);
	a->cortar_ms = tempo_imagem(b, k, civ, "image_chop", 100, NULL);
	a->cavar_ms = tempo_imagem(b, k, civ, "image_dig", 100, NULL);
	a->ordenhar_ms = tempo_imagem(b, k, civ, "image_milk", 100, NULL);
	/* huella: «object» del dibujo / 32 (edificios y minas); el resto 1x1 */
	a->pegada[0] = a->pegada[1] = 1;
	if ((a->tipo == TZ_T_EDIFICIO || k == TZC_RES_GOLD || k == TZC_RES_STONE) && b) {
		TzObjeto o;
		memset(&o, 0, sizeof o);
		o.classe = (uint16_t)k; o.civ = (uint8_t)(civ ? civ : 1); o.vida = o.vida_max = 1;
		uint8_t dados[96] = { 0 };
		if (k == TZC_WALL_STONE || k == TZC_WALL_WOOD) dados[56] = 0;   /* torre de muro: 1x1 */
		o.dados = dados; o.ndados = sizeof dados;
		const char *sec = tz_imagem_objeto(b, &o);
		int r[4] = { 0, 0, 0, 0 };
		if (k == TZC_GATE_WOOD || k == TZC_GATE_STONE) sec = k == TZC_GATE_STONE ? "gs0_anim" : "gw0_anim";
		/* sin «object», el tamaño del cuadro (aquí, el de «selection») */
		if (sec && tz_ini_numeros(&b->imagens, sec, "object", r, 4) < 4) tz_ini_numeros(&b->imagens, sec, "selection", r, 4);
		if (sec && (r[2] || r[3])) {
			int w = r[2] / 32, h = r[3] / 32;
			a->pegada[0] = (uint8_t)(w < 1 ? 1 : w > 16 ? 16 : w);
			a->pegada[1] = (uint8_t)(h < 1 ? 1 : h > 16 ? 16 : h);
		}
		if (k == TZC_WALL_STONE || k == TZC_WALL_WOOD) a->pegada[0] = a->pegada[1] = 1;
	}
}

/* ------------------------------------------------------------------ tecnologías y órdenes */
static void ler_tecnologias(TzCatalogo *c, const TzIni *ini, int mejora)
{
	for (int s = 0; s < ini->ns; ++s) {
		const TzIniSecao *sec = &ini->s[s];
		TzTecnologia *t = realloc(c->tec, (size_t)(c->ntec + 1) * sizeof *t);
		if (!t) return;
		c->tec = t;
		t = &c->tec[c->ntec++];
		memset(t, 0, sizeof *t);
		snprintf(t->nome, sizeof t->nome, "%s", sec->nome);
		minusc(t->nome);
		t->mejora = (uint8_t)mejora;
		const char *v;
		static const char *const rec[4] = { "gold", "stone", "food", "wood" };
		for (int i = 0; i < 4; ++i) if ((v = tz_ini(ini, sec->nome, rec[i]))) t->custo[i] = (uint32_t)atoi_c(v);
		if ((v = tz_ini(ini, sec->nome, "manna"))) t->mana = (uint16_t)atoi_c(v);
		if ((v = tz_ini(ini, sec->nome, "time"))) t->tempo_ms = (uint32_t)atoi_c(v);
	}
}

static int com_alvo(const char *a)
{
	static const char *const l[] = { "move", "move_advance", "move_sneak", "attack", "attack_ground", "repair", "capture", "harvest",
		"dig", "chop", "guard", "patrol", "explore", "pick_item", "fishing", "murder", "bribe", "impersonate", "unload", "item_drop", "item_use" };
	if (!strncmp(a, "build ", 6) || !strncmp(a, "spell ", 6) || !strcmp(a, "create crusadeanchor")) return 1;
	for (unsigned i = 0; i < sizeof l / sizeof *l; ++i) if (!strcmp(a, l[i])) return 1;
	return 0;
}

static void ler_ordens(TzCatalogo *c)
{
	const TzIni *ini = &c->ordens_ini;
	c->ord = calloc((size_t)(ini->ns ? ini->ns : 1), sizeof *c->ord);
	if (!c->ord) return;
	for (int s = 0; s < ini->ns; ++s) {
		const char *n = ini->s[s].nome;
		TzOrdemDef *o = &c->ord[c->nord++];
		snprintf(o->chave, sizeof o->chave, "%s", n); minusc(o->chave);
		const char *v = tz_ini(ini, n, "action");
		snprintf(o->acao, sizeof o->acao, "%s", v ? v : ""); minusc(o->acao);
		char *a = recorta(o->acao); memmove(o->acao, a, strlen(a) + 1);
		v = tz_ini(ini, n, "classname");
		if (v && *v) snprintf(o->classe, sizeof o->classe, "%s", v);
		else { const char *sp = strchr(o->acao, ' '); snprintf(o->classe, sizeof o->classe, "%s", sp ? sp + 1 : o->chave); }
		minusc(o->classe);
		v = tz_ini(ini, n, "submenu");
		snprintf(o->submenu, sizeof o->submenu, "%s", v ? v : ""); minusc(o->submenu);
		v = tz_ini(ini, n, "img");
		o->icone = (int16_t)(v && *v ? atoi_c(v) : -1);
		o->com_alvo = (uint8_t)com_alvo(o->acao);
	}
}

/* ------------------------------------------------------------------ BONUS.INI (gameplay.rs group/bonus) */
/* ¿la clase (con su civilización) está en el grupo o es la clase nombrada? (prefijos «e », «r », «a ») */
static int grupo2(const TzIni *bonus, const char *chave, int classe, int civ, int prof)
{
	if (prof > 8) return 0;
	char nome[80];
	snprintf(nome, sizeof nome, "%s", chave);
	char *n = recorta(nome);
	minusc(n);
	if (strlen(n) > 2 && n[1] == ' ' && (n[0] == 'e' || n[0] == 'r' || n[0] == 'a')) {
		int c = n[0] == 'e' ? 1 : n[0] == 'r' ? 2 : 3;
		return civ == c && !strcmp(recorta(n + 2), tz_classes[classe]);
	}
	if (!strcmp(n, tz_classes[classe])) return 1;
	const char *g = tz_ini(bonus, "groups", n);
	if (!g) return 0;
	const char *p = g;
	while (*p) {
		const char *f = strchr(p, ',');
		size_t l = f ? (size_t)(f - p) : strlen(p);
		char sub[80];
		snprintf(sub, sizeof sub, "%.*s", (int)(l < 79 ? l : 79), p);
		if (grupo2(bonus, sub, classe, civ, prof + 1)) return 1;
		if (!f) break;
		p = f + 1;
	}
	return 0;
}

#define NB 74
static void montar_bonus(TzCatalogo *c)
{
	c->bonus_tab = calloc(NB * 4 * NB * 4, sizeof *c->bonus_tab);
	c->bonus_pct = calloc(NB * 4 * NB * 4, 1);
	if (!c->bonus_tab || !c->bonus_pct) return;
	const TzIni *b = &c->bonus;
	/* gameplay.rs bonus(): la última entrada que encaja gana (no se suman) */
	for (int s = 0; s < b->ns; ++s) {
		const TzIniSecao *sec = &b->s[s];
		if (!strcasecmp(sec->nome, "groups")) continue;
		for (int ka = 1; ka < NB; ++ka) for (int ca = 1; ca < 4; ++ca) {
			if (!grupo2(b, sec->nome, ka, ca, 0)) continue;
			for (int p = 0; p < sec->n; ++p) {
				const char *v = sec->p[p].valor;
				int val = atoi_c(v), pct = strchr(v, '%') != NULL;
				for (int kb = 1; kb < NB; ++kb) for (int cb = 1; cb < 4; ++cb) {
					if (!grupo2(b, sec->p[p].chave, kb, cb, 0)) continue;
					int i = ((ka * 4 + ca) * NB + kb) * 4 + cb;
					c->bonus_tab[i] = (int16_t)val;
					c->bonus_pct[i] = (uint8_t)(pct ? 1 : 2);
				}
			}
		}
	}
}

/* ------------------------------------------------------------------ carga */
int tz_catalogo_carregar(TzCatalogo *c, TzRecursos *r, const TzBiblioteca *b)
{
	memset(c, 0, sizeof *c);
	c->bib = b;
	for (int civ = 0; civ < 4; ++civ) { ler_tabela(c, r, civ, "BUILDINGS"); ler_tabela(c, r, civ, "UNITS"); }
	/* sin fila propia de la civilización, la común */
	for (int civ = 1; civ < 4; ++civ)
		for (int k = 0; k < TZ_NCLASSES; ++k)
			if (!c->at[civ][k].existe && c->at[0][k].existe) c->at[civ][k] = c->at[0][k];
	for (int civ = 0; civ < 4; ++civ) for (int k = 0; k < TZ_NCLASSES; ++k) completar(c, b, civ, k);
	TzIni tec = { 0 }, upg = { 0 };
	if (tz_recursos_existe(r, "DATA/TECHNOLOGIES.INI")) tz_ini_recurso(r, &tec, "DATA/TECHNOLOGIES.INI");
	if (tz_recursos_existe(r, "DATA/UPGRADES.INI")) tz_ini_recurso(r, &upg, "DATA/UPGRADES.INI");
	ler_tecnologias(c, &tec, 0);
	ler_tecnologias(c, &upg, 1);
	tz_ini_liberar(&tec); tz_ini_liberar(&upg);
	if (tz_recursos_existe(r, "DATA/ORDERS.INI")) tz_ini_recurso(r, &c->ordens_ini, "DATA/ORDERS.INI");
	if (tz_recursos_existe(r, "DATA/MENU.INI")) tz_ini_recurso(r, &c->menus, "DATA/MENU.INI");
	if (tz_recursos_existe(r, "DATA/BONUS.INI")) tz_ini_recurso(r, &c->bonus, "DATA/BONUS.INI");
	if (tz_recursos_existe(r, "DATA/RES.INI")) tz_ini_recurso(r, &c->recursos_ini, "DATA/RES.INI");
	static const char *const civs[4] = { NULL, "EURO", "ARAB", "ASIA" };
	for (int i = 1; i < 4; ++i) {
		char n[60]; snprintf(n, sizeof n, "DATA/TECHTREE %s.INI", civs[i]);
		if (tz_recursos_existe(r, n)) tz_ini_recurso(r, &c->arvores[i], n);
	}
	ler_ordens(c);
	montar_bonus(c);
	static const char *const niv[3] = { "Low", "Medium", "High" };
	static const char *const rec[4] = { "gold", "stone", "food", "wood" };
	for (int i = 0; i < 3; ++i) for (int j = 0; j < 4; ++j) {
		const char *v = tz_ini(&c->recursos_ini, niv[i], rec[j]);
		c->recursos_ini_nivel[i + 1][j] = v ? (uint32_t)atoi_c(v) : 0;
	}
	const char *v;
	c->mina_ouro = (v = tz_ini(&c->recursos_ini, "Others", "golden rocks")) ? (uint32_t)atoi_c(v) : 20000;
	c->mina_pedra = (v = tz_ini(&c->recursos_ini, "Others", "rocks")) ? (uint32_t)atoi_c(v) : 10000;
	c->pesqueiro = (v = tz_ini(&c->recursos_ini, "Others", "fish patch")) ? (uint32_t)atoi_c(v) : 500;
	return 0;
}

void tz_catalogo_liberar(TzCatalogo *c)
{
	free(c->tec); free(c->ord); free(c->bonus_tab); free(c->bonus_pct);
	tz_ini_liberar(&c->menus); tz_ini_liberar(&c->bonus); tz_ini_liberar(&c->ordens_ini); tz_ini_liberar(&c->recursos_ini);
	for (int i = 0; i < 4; ++i) tz_ini_liberar(&c->arvores[i]);
	memset(c, 0, sizeof *c);
}

const TzAtributos *tz_catalogo_at(const TzCatalogo *c, int classe, int civ)
{
	if (classe < 0 || classe >= TZ_NCLASSES) classe = 0;
	if (civ < 0 || civ > 3) civ = 0;
	return &c->at[civ][classe];
}

/* el coste que dice el INI de la clase («cost = 1000g,2000f», p. ej. el ancla de la cruzada); 0 si lo tiene, -1 si no */
int tz_catalogo_custo_ini(const TzCatalogo *c, int classe, int civ, uint32_t custo_[TZ_NREC])
{
	const char *v = classe > 0 && classe < TZ_NCLASSES ? prop(c->bib, classe, civ, "cost") : NULL;
	uint16_t mana;
	if (!v) { memset(custo_, 0, sizeof(uint32_t) * TZ_NREC); return -1; }
	custo(v, custo_, &mana);
	return 0;
}

int tz_catalogo_tec(const TzCatalogo *c, const char *nome)
{
	if (!nome) return -1;
	for (int i = 0; i < c->ntec; ++i) if (!strcasecmp(c->tec[i].nome, nome)) return i;
	return -1;
}

const TzOrdemDef *tz_catalogo_ordem(const TzCatalogo *c, const char *chave)
{
	if (!chave) return NULL;
	for (int i = 0; i < c->nord; ++i) if (!strcasecmp(c->ord[i].chave, chave)) return &c->ord[i];
	return NULL;
}

/* gameplay.rs permitted: las órdenes del menú de la clase, con los submenús */
int tz_catalogo_menu(const TzCatalogo *c, int classe, int civ, const TzOrdemDef **out, int max)
{
	const TzAtributos *a = tz_catalogo_at(c, classe, civ);
	if (!a->menu[0]) return 0;
	char pend[32][32]; int np = 0, n = 0;
	char visto[32][32]; int nv = 0;
	snprintf(pend[np++], 32, "%s", a->menu);
	while (np) {
		char menu[32]; snprintf(menu, sizeof menu, "%s", pend[--np]);
		int ja = 0;
		for (int i = 0; i < nv; ++i) if (!strcasecmp(visto[i], menu)) ja = 1;
		if (ja || nv >= 32) continue;
		snprintf(visto[nv++], 32, "%s", menu);
		const TzIniSecao *s = tz_ini_secao(&c->menus, menu);
		if (!s) continue;
		for (int p = 0; p < s->n; ++p) {
			const TzOrdemDef *o = tz_catalogo_ordem(c, s->p[p].chave);
			if (!o) continue;
			if (o->submenu[0]) { if (np < 32) snprintf(pend[np++], 32, "%s", o->submenu); continue; }
			int rep = 0;
			for (int i = 0; i < n; ++i) if (out[i] == o) rep = 1;
			if (!rep && n < max) out[n++] = o;
		}
	}
	return n;
}

/* gameplay.rs received(): el golpe con el bono de BONUS.INI del atacante contra el defensor */
uint32_t tz_catalogo_bonus(const TzCatalogo *c, int ka, int ca, int kb, int cb, uint32_t golpe)
{
	if (!c->bonus_tab || ka <= 0 || kb <= 0 || ka >= NB || kb >= NB || ca < 1 || ca > 3 || cb < 1 || cb > 3) return golpe;
	int i = ((ka * 4 + ca) * NB + kb) * 4 + cb;
	if (!c->bonus_pct[i]) return golpe;
	int32_t v = c->bonus_tab[i];
	int64_t r = c->bonus_pct[i] == 1 ? (int64_t)golpe + (int64_t)golpe * v / 100 : (int64_t)golpe + v;
	return (uint32_t)(r < 0 ? 0 : r > 65535 ? 65535 : r);
}
