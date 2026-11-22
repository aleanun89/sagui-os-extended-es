/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ui_cursor.c - los cursores del juego Tzar en vez de los del sistema (D417, fase 4).
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): frontend/mod.rs update_cursor (la flecha
 * IMAGES/CURSORS/S_DEF.BMP con su punto en (2, 0); las demás formas, secciones «cs_*» de DATA/IMAGES.INI con image y hotspot) y
 * frontend/orders.rs (qué forma lleva cada orden). El cursor se pinta en el lienzo del juego, encima de todo, y el del sistema se
 * oculta. Sin el arte (datos sintéticos) se dibuja una flecha propia. */
#include "ui.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define FORMAS 24

typedef struct { char nome[24]; int ok; TzImg img; int hx, hy; } Forma;
static Forma formas[FORMAS];
static int nformas;
static TzIni imagens_ini;
static int ini_lido;
static TzRecursos *rec_ini;

void tz_ui_cursor_liberar(void)
{
	for (int i = 0; i < nformas; ++i) tz_img_liberar(&formas[i].img);
	nformas = 0;
	if (ini_lido == 1) tz_ini_liberar(&imagens_ini);
	ini_lido = 0;
	rec_ini = NULL;
}

/* la imagen de una forma: un BMP de verde transparente; si es una tira (cols, rows), el primer cuadro */
static const Forma *forma(TzUiSkin *s, const char *nome)
{
	for (int i = 0; i < nformas; ++i) if (!strcmp(formas[i].nome, nome)) return formas[i].ok ? &formas[i] : NULL;
	if (nformas == FORMAS) return NULL;
	Forma *f = &formas[nformas++];
	memset(f, 0, sizeof *f);
	snprintf(f->nome, sizeof f->nome, "%s", nome);
	if (!s->rec) return NULL;
	if (rec_ini != s->rec) {
		if (ini_lido == 1) tz_ini_liberar(&imagens_ini);
		memset(&imagens_ini, 0, sizeof imagens_ini);
		ini_lido = tz_ini_recurso(s->rec, &imagens_ini, "DATA/IMAGES.INI") == 0 ? 1 : -1;
		rec_ini = s->rec;
	}
	char ruta[160];
	int cols = 1, rows = 1;
	if (!strcmp(nome, "arrow")) { snprintf(ruta, sizeof ruta, "CURSORS/S_DEF"); f->hx = 2; f->hy = 0; }
	else {
		const char *im = ini_lido == 1 ? tz_ini(&imagens_ini, nome, "image") : NULL;
		if (!im || !*im) return NULL;
		snprintf(ruta, sizeof ruta, "%s", im);
		for (char *p = ruta; *p; ++p) if (*p == '\\') *p = '/';
		size_t l = strlen(ruta);
		if (l > 4 && !strcasecmp(ruta + l - 4, ".bmp")) ruta[l - 4] = 0;
		const char *hs = tz_ini(&imagens_ini, nome, "hotspot");
		if (hs) { f->hx = atoi(hs); const char *c = strchr(hs, ','); f->hy = c ? atoi(c + 1) : 0; }
		const char *vc = tz_ini(&imagens_ini, nome, "cols"), *vr = tz_ini(&imagens_ini, nome, "rows");
		if (vc && atoi(vc) > 0) cols = atoi(vc);
		if (vr && atoi(vr) > 0) rows = atoi(vr);
	}
	const TzImg *img = tz_ui_img(s, ruta, -1);
	if (!img || img->w < cols || img->h < rows) return NULL;
	int w = img->w / cols, h = img->h / rows;
	f->img.w = w; f->img.h = h;
	f->img.px = malloc((size_t)w * h * 4);
	if (!f->img.px) return NULL;
	for (int y = 0; y < h; ++y) memcpy(f->img.px + (size_t)y * w, img->px + (size_t)y * img->w, (size_t)w * 4);
	f->ok = 1;
	return f;
}

/* flecha propia de 12x19 (sin datos): 1 = negro, 2 = blanco */
static const char *const SETA[19] = {
	"1", "11", "121", "1221", "12221", "122221", "1222221", "12222221", "122222221", "1222222221", "12222211111",
	"1221221", "121 1221", "11  1221", "1    1221", "     1221", "      1221", "      1221", "       11",
};

void tz_ui_cursor_desenhar(TzUiSkin *s, TzTela *t, const char *nome, int x, int y)
{
	const Forma *f = nome && *nome ? forma(s, nome) : NULL;
	if (!f) f = forma(s, "arrow");
	if (f) { tz_tela_imagem(t, &f->img, (TzRect){ 0, 0, f->img.w, f->img.h }, x - f->hx, y - f->hy, 0); return; }
	for (int j = 0; j < 19; ++j)
		for (int i = 0; SETA[j][i]; ++i) {
			int px = x + i, py = y + j;
			if (SETA[j][i] == ' ' || px < 0 || py < 0 || px >= t->w || py >= t->h) continue;
			t->px[(size_t)py * t->w + px] = SETA[j][i] == '1' ? 0xff000000u : 0xffffffffu;
		}
}

/* frontend/orders.rs: la forma de una acción de ORDERS.INI */
static const char *forma_acao(const char *a)
{
	static const struct { const char *a, *f; } tab[] = {
		{ "attack", "cs_attack" }, { "murder", "cs_attack" }, { "attack_ground", "cs_attackset" }, { "move", "cs_move" },
		{ "move_advance", "cs_move" }, { "move_sneak", "cs_move" }, { "unload", "cs_move" }, { "board", "cs_move" },
		{ "harvest", "cs_harvest" }, { "chop", "cs_harvest" }, { "fishing", "cs_harvest" }, { "mine", "cs_harvest" },
		{ "dig", "cs_harvest" }, { "repair", "cs_repair" }, { "build", "cs_repair" }, { "capture", "cs_magicharm" },
		{ "guard", "cs_guard" }, { "patrol", "cs_patrol_ex" }, { "explore", "cs_patrol_ex" }, { "pick_item", "cs_pick" },
		{ "spell", "cs_spell" }, { "spell_item", "cs_spell" },
	};
	for (unsigned i = 0; i < sizeof tab / sizeof *tab; ++i) if (!strcasecmp(a, tab[i].a)) return tab[i].f;
	return "";
}

const char *tz_ui_cursor_forma(TzUi *u)
{
	if (u->modo == TZ_UI_ALVO) return forma_acao(u->acao_alvo);
	if (u->modo == TZ_UI_COLOCAR || !tz_rect_dentro(u->mapa_rect, u->mx, u->my) || !u->sel.n || !u->m) return "";
	/* la orden de clic derecho sobre lo que hay bajo el ratón (como el original, solo con unidades propias elegidas) */
	const TzMundo *m = u->m;
	TzId ids[TZ_SEL_MAX];
	int n = 0;
	for (int i = 0; i < u->sel.n && n < TZ_SEL_MAX; ++i) {
		const TzObj *o = &m->obj[u->sel.id[i]];
		if (o->dono == m->jogador && tz_obj_vivo(o) && tz_obj_unidade(o)) ids[n++] = u->sel.id[i];
	}
	if (!n) return "";
	int wx = u->mx - u->mapa_rect.x + u->camx, wy = u->my - u->mapa_rect.y + u->camy;
	TzId alvo = tz_ui_pick(m, wx, wy);
	if (alvo == TZ_NENHUM) return "";
	TzOrdem o = tz_mundo_ordem_padrao(m, m->jogador, ids, n, wx / TZ_CASILLA, wy / TZ_CASILLA, alvo);
	switch (o.tipo) {
	case TZ_O_ATACAR: return "cs_attack";
	case TZ_O_RECOLHER: case TZ_O_CORTAR: return "cs_harvest";
	case TZ_O_REPARAR: return "cs_repair";
	case TZ_O_CAPTURAR: return "cs_magicharm";
	case TZ_O_PEGAR_ITEM: return "cs_pick";
	default: return "";
	}
}
