/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ia_obras.c - lo que un reino de la IA puede hacer (árbol de su civilización) y dónde pone sus edificios: la busca
 * de un sitio, el espacio que deja, quién construye y la obra (D417, fase 6).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): src/gameplay/ai/tree.rs (quién
 * entrena o investiga cada tipo 0x405a20, si está en el árbol 0x407d50, 0x494c90) y src/gameplay/ai/build.rs (0x40a990,
 * la busca 0x40a050 con la inundación 0x44fbb0 y su prueba 0x40a360, el espacio 0x40b300, la tierra libre 0x469530,
 * quién construye 0x40cec0, la obra 0x40b610).
 * Diferencias: la busca se hace entera en la misma mirada (hasta IA_BUSCA casillas) en vez de 200 cada 100 ms, y la
 * obra lejana la coloca la simulación con el comando (no hay «sitio virtual» mientras el constructor camina).
 */
#include "ia_int.h"
#include <ctype.h>
#include <stdio.h>

#define IA_BUSCA 4096        /* casillas que mira la busca de un sitio */
#define IA_LONGE 10          /* build.rs FAR: un constructor más lejos va andando a la obra */

static const int INVOCADOS[5] = { 15, 16, 22, 30, 32 };   /* tree.rs SUMMONED: los hace el mago (0x405a64) */

static int primeira_palavra(const char *acao, const char *w)
{
	size_t n = strlen(w);
	return !strncmp(acao, w, n) && (acao[n] == ' ' || acao[n] == 0);
}

/* tree.rs producer y present: quién hace cada tipo y qué tipos están en el árbol de la civilización */
void ia_arvore(TzIa *ia)
{
	int nt = ia->b->ntipos;
	free(ia->presente); free(ia->produtor);
	ia->presente = calloc((size_t)nt + 1, 1);
	ia->produtor = malloc(((size_t)nt + 1) * sizeof *ia->produtor);
	if (!ia->presente || !ia->produtor) return;
	for (int t = 0; t < nt; ++t) ia->produtor[t] = -1;
	for (int j = 0; j < 5; ++j) if (INVOCADOS[j] < nt) ia->produtor[INVOCADOS[j]] = IC_MAGE;
	const TzCatalogo *c = ia->m->cat;
	if (!c) return;
	char vistos[128][32];
	int nvistos = 0;
	for (int cl = 0; cl < TZ_NCLASSES; ++cl) {
		const TzAtributos *at = tz_catalogo_at(c, cl, ia->civ);
		if (!at || !at->menu[0]) continue;
		/* 0x405a20: el menú propio, sin submenús; el último en orden de clase gana */
		const TzIniSecao *s = tz_ini_secao(&c->menus, at->menu);
		for (int p = 0; s && p < s->n; ++p) {
			const TzOrdemDef *od = tz_catalogo_ordem(c, s->p[p].chave);
			if (!od) continue;
			if (primeira_palavra(od->acao, "create") || primeira_palavra(od->acao, "technology")) {
				int t = tz_ia_bib_tipo(ia->b, od->classe);
				if (t >= 0) ia->produtor[t] = (int16_t)cl;
			}
		}
		/* el flag +0x107: en un menú que enseñan sus objetos, submenús incluidos */
		char pend[32][32];
		int np = 0;
		snprintf(pend[np++], 32, "%s", at->menu);
		while (np) {
			char menu[32];
			snprintf(menu, sizeof menu, "%s", pend[--np]);
			for (char *q = menu; *q; ++q) *q = (char)tolower((unsigned char)*q);
			int ja = 0;
			for (int v = 0; v < nvistos && !ja; ++v) ja = !strcmp(vistos[v], menu);
			if (ja) continue;
			if (nvistos < 128) snprintf(vistos[nvistos++], 32, "%s", menu);
			s = tz_ini_secao(&c->menus, menu);
			for (int p = 0; s && p < s->n; ++p) {
				const TzOrdemDef *od = tz_catalogo_ordem(c, s->p[p].chave);
				if (!od) continue;
				if (od->submenu[0]) { if (np < 32) snprintf(pend[np++], 32, "%s", od->submenu); continue; }
				if (primeira_palavra(od->acao, "create") || primeira_palavra(od->acao, "build") ||
				    primeira_palavra(od->acao, "technology") || primeira_palavra(od->acao, "upgrade")) {
					int t = tz_ia_bib_tipo(ia->b, od->classe);
					if (t >= 0) ia->presente[t] = 1;
				}
			}
		}
	}
}

static int tec_conhecida(const TzIa *ia, const char *nome)
{
	char n[48];
	int j = 0;
	while (*nome == ' ') nome++;
	for (; nome[j] && j < 47; ++j) n[j] = (char)tolower((unsigned char)nome[j]);
	while (j > 0 && n[j - 1] == ' ') j--;
	n[j] = 0;
	int t = ia->m->cat ? tz_catalogo_tec(ia->m->cat, n) : -1;
	return t >= 0 && ia->m->reino[ia->k].pesquisas && ia->m->reino[ia->k].pesquisas[t] > 0;
}

/* TzReinoEstado.proibido: las tecnologías por su índice y las clases tras ellas (mundo.c: [ntec + clase]) */
static int proibido(const TzIa *ia, int tipo)
{
	const uint8_t *p = ia->m->reino[ia->k].proibido;
	if (!p || !ia->m->cat) return 0;
	if (tipo < TZ_NCLASSES) return p[ia->m->cat->ntec + tipo];
	int t = ia_tec(ia, tipo);
	return t >= 0 && p[t];
}

/* 0x407d50 con 0x494ba0 y 0x494c90: la meta se puede perseguir: un recurso siempre; un tipo si no está prohibido,
 * está en el árbol (una investigación no hecha ni en curso), con su investigación hecha y su edificio terminado */
int ia_disponivel_tipo(const TzIa *ia, int tipo)
{
	if (tipo >= 0x8000) return 1;
	if (tipo <= 0 || tipo >= ia->b->ntipos) return 0;
	if (proibido(ia, tipo)) return 0;
	const char *nome = ia->b->tipo[tipo];
	if (tipo >= ia->b->tecnologias) {
		if (tec_conhecida(ia, nome)) return 0;
		int t = ia_tec(ia, tipo);
		for (TzId i = 0; t >= 0 && i < ia->m->nobj; ++i)
			if (ia_meu(ia, i) && ia_trab_frente(&ia->m->obj[i], TZ_J_PESQUISAR, t)) return 0;
	}
	if (!ia->presente || !ia->presente[tipo]) return 0;
	const TzCatalogo *c = ia->m->cat;
	if (!c || ia->civ < 0 || ia->civ > 3) return 1;
	const TzIni *arv = &c->arvores[ia->civ];
	const char *dep = tz_ini(arv, "depend", nome);
	if (dep && *dep && !tec_conhecida(ia, dep)) return 0;
	const char *req = tz_ini(arv, "required", nome);
	if (req && *req) {
		char r[40];
		int j = 0;
		while (*req == ' ') req++;
		for (; req[j] && j < 39; ++j) r[j] = req[j];
		while (j > 0 && r[j - 1] == ' ') j--;
		r[j] = 0;
		int cl = tz_ia_bib_tipo(ia->b, r);
		int achou = 0;
		for (TzId i = 0; i < ia->m->nobj && !achou; ++i) {
			const TzObj *o = &ia->m->obj[i];
			achou = o->dono == ia->k && ia_contado(o) && !ia_obra(o) && o->classe == cl;
		}
		if (!achou) return 0;
	}
	return 1;
}

int ia_armazem_de(int recurso)
{
	return recurso <= 1 ? TZC_STORAGE : recurso == 2 ? TZC_FARM : TZC_LUMBER;
}

/* build.rs free: un campesino que el jugador puede llevarse a otro trabajo */
int ia_livre(const TzIa *ia, TzId i)
{
	const TzObj *o = ia_obj(ia, i);
	if (!o || !ia_contado(o) || (o->flags & (TZ_F_HISTORIA | TZ_F_MORRENDO | TZ_F_DENTRO)) || !o->ator) return 0;
	if (i < ia->cappeao && ia->mandado[i] == ia->m->tic + 1) return ia->peao[i].grupo != G_CONSTRUTOR;
	const TzAtor *a = o->ator;
	int carga = a->carga > 0;
	switch (a->ordem.tipo) {
	case TZ_O_CONSTRUIR: return 0;
	case TZ_O_CORTAR:
		if (a->trabalho == TZ_TRAB_CORTAR && !carga && !ia->construindo) return 0;
		break;
	case TZ_O_RECOLHER: {
		const TzObj *t = ia_obj(ia, a->ordem.alvo);
		if (t && t->classe == TZC_COW) return 1;
		if (t && t->classe == TZC_FIELD) {
			if (a->trabalho == TZ_TRAB_CAMPO && !carga && !ia->construindo) return 0;
			if (t->vida < t->vida_max) return 0;
		}
		break;
	}
	case TZ_O_ENTREGAR:
		if (carga && !ia->construindo) {
			const TzObj *s = ia_obj(ia, a->ordem.alvo);
			if (s && (s->classe == TZC_CASTLE || s->classe == ia_armazem_de(a->carga_tipo))) return 0;
		}
		break;
	}
	return 1;
}

/* ------------------------------------------------------------------ el sitio de un edificio */
static int livre_chao(const TzIa *ia, int x, int y)
{
	if (x < 0 || y < 0 || x >= ia->m->lado || y >= ia->m->lado || !ia->m->cel) return 0;
	uint16_t b = ia->m->cel[y * ia->m->lado + x].bits;
	if ((b & (TZ_CEL_AGUA | TZ_CEL_PENHASCO)) && !(b & TZ_CEL_PONTE)) return 0;
	return !(b & (TZ_CEL_ARVORE | TZ_CEL_EDIFICIO | TZ_CEL_MURO | TZ_CEL_PORTA | TZ_CEL_CAMPO));
}

static int arvore_em(const TzIa *ia, int x, int y)
{
	if (x < 0 || y < 0 || x >= ia->m->lado || y >= ia->m->lado || !ia->m->cel) return 0;
	const TzCelula *c = &ia->m->cel[y * ia->m->lado + x];
	return (c->bits & TZ_CEL_ARVORE) && c->madeira > 0;
}

/* 0x469530: cada fila empieza con tierra libre y tiene tantas como el ancho detrás (una columna más) */
static int terra_livre(const TzIa *ia, const int r[4])
{
	int largo = r[2] - r[0] + 1;
	for (int y = r[1]; y <= r[3]; ++y)
		for (int x = r[0]; x <= r[0] + largo; ++x)
			if (!livre_chao(ia, x, y)) return 0;
	return 1;
}

/* 0x40b300: el margen alrededor libre (2; 1 para cuarteles, catedral, academia, torre de magos, gremio, casa, shaolin,
 * mezquita y muñeco; 0 para un campo); torres, aserraderos y almacenes a 8 de los suyos; un aserradero en la fase de
 * construir quiere 10 árboles a 8 */
static int espaco(const TzIa *ia, int classe, int x, int y, int w, int h)
{
	int margem = 2;
	const TzIaLista *longe = NULL;
	if (classe >= 39 && classe <= 42) { margem = 1; longe = &ia->torres; }
	else if (classe == TZC_LUMBER) { margem = 1; longe = &ia->serrarias; }
	else if (classe == TZC_STORAGE) { margem = 1; longe = &ia->armazens; }
	else if (classe == TZC_FIELD) margem = 0;
	else if (classe == TZC_BARRACK || classe == 55 || classe == 56 || classe == 60 || classe == 65 || classe == 66 ||
	         classe == 70 || classe == 71 || classe == 133) margem = 1;
	int u = ia->m->lado - 1;
	int r[4] = { x - margem < 0 ? 0 : x - margem, y - margem < 0 ? 0 : y - margem,
	             x + w - 1 + margem > u ? u : x + w - 1 + margem, y + h - 1 + margem > u ? u : y + h - 1 + margem };
	if (!terra_livre(ia, r)) return 0;
	for (int j = 0; longe && j < longe->n; ++j) {
		int q[4];
		ia_rect(ia, longe->v[j], q);
		if (ia_dist_rect(r, q) < 8) return 0;
	}
	if (ia->construindo && classe == TZC_LUMBER) {
		int arv = 0;
		for (int cx = x - 8; cx <= x + w - 1 + 8; ++cx)
			for (int cy = y - 8; cy <= y + h - 1 + 8; ++cy)
				if (arvore_em(ia, cx, cy) && ++arv >= 10) return 1;
		return 0;
	}
	return 1;
}

/* 0x40cec0: quién construye: el ocioso libre más cercano si está a menos de 10; si no, de los recursos menos
 * queridos, el recolector libre más cercano si está a dos tercios de eso; si no, aquel ocioso */
static TzId construtor(TzIa *ia, const int sitio[4], TzId prefer)
{
	if (prefer != TZ_NENHUM) return prefer;
	int ao[4] = { sitio[0] - 1, sitio[1] - 1, sitio[2] + 1, sitio[3] + 1 };
	TzId melhor = TZ_NENHUM;
	uint32_t dm = 0;
	for (int j = 0; j < ia->grupo[G_OCIOSO].n; ++j) {
		TzId i = ia->grupo[G_OCIOSO].v[j];
		if (!ia_livre(ia, i)) continue;
		const TzObj *o = &ia->m->obj[i];
		if (!ia_alcanca(ia, o->cx, o->cy, ao)) continue;
		int r[4];
		ia_rect(ia, i, r);
		uint32_t d = ia_dist_rect(r, sitio);
		if (melhor == TZ_NENHUM || d < dm) { melhor = i; dm = d; }
	}
	if (melhor != TZ_NENHUM && dm < IA_LONGE) return melhor;
	uint32_t limite = melhor == TZ_NENHUM ? 0xffffffffu : (uint32_t)((2ull * dm) / 3);
	for (int at = 3; at >= 0; --at) {
		int g = G_OURO + ia->ordem[at];
		TzId perto = TZ_NENHUM;
		uint32_t dp = 0;
		for (int j = 0; j < ia->grupo[g].n; ++j) {
			TzId i = ia->grupo[g].v[j];
			if (!ia_livre(ia, i)) continue;
			const TzObj *o = &ia->m->obj[i];
			if (!ia_alcanca(ia, o->cx, o->cy, ao)) continue;
			int r[4];
			ia_rect(ia, i, r);
			uint32_t d = ia_dist_rect(r, sitio);
			if (perto == TZ_NENHUM || d < dp) { perto = i; dp = d; }
		}
		if (perto != TZ_NENHUM && dp <= limite) return perto;
	}
	return melhor;
}

/* 0x40b610: la obra en (x, y): el precio tiene que estar; se elige el constructor; la simulación la coloca y la cobra
 * al aplicar el comando */
static int lancar(TzIa *ia, int classe, int x, int y, TzId prefer)
{
	ia_gastavel(ia, ia->disponivel);
	int32_t c[4];
	ia_custo(ia, classe, c);
	if (classe == TZC_CASTLE || classe == TZC_LUMBER) ia_reserva_inicial(ia, 0);
	if (!ia_pagavel(ia, c, B_NENHUMA, 1, 0)) return 0;
	int w, h;
	ia_pegada(ia, classe, &w, &h);
	int r[4] = { x, y, x + w - 1, y + h - 1 };
	TzId b = construtor(ia, r, prefer);
	if (b == TZ_NENHUM) return 0;
	if (ia_construir(ia, b, classe, x, y)) return 0;
	ia_pagar(ia, c);
	ia_tocar(ia, V_TIPO, classe);
	return 1;
}

/* 0x40a050 con la inundación 0x44fbb0: en anchura sobre los ocho vecinos dentro de sus límites; se extiende por casillas
 * donde se puede estar de pie, y por todas hasta encontrar la primera */
static int buscar(TzIa *ia, int classe, int cx, int cy, uint32_t mn, uint32_t mx, TzId prefer, int *ox, int *oy)
{
	int w, h;
	ia_pegada(ia, classe, &w, &h);
	int lado = ia->m->lado, u = lado - 1;
	int lim[4] = { cx - w - (int)mx, cy - h - (int)mx, cx + (int)mx, cy + (int)mx };
	if (lim[0] < 0) lim[0] = 0;
	if (lim[1] < 0) lim[1] = 0;
	if (lim[2] > u) lim[2] = u;
	if (lim[3] > u) lim[3] = u;
	if (cx < lim[0] || cy < lim[1] || cx > lim[2] || cy > lim[3]) return 0;
	/* la referencia: la mina del almacén o el castillo de la casa */
	int ref[4], tem_ref = 0;
	for (TzId i = ia->m->nobj - 1; i >= 0 && !tem_ref; --i) {
		const TzObj *o = &ia->m->obj[i];
		if (!ia_contado(o)) continue;
		int quer = classe == TZC_STORAGE ? (o->classe == TZC_RES_GOLD || o->classe == TZC_RES_STONE) && ia_ve(ia, i)
		         : cx == ia->casa[0] && cy == ia->casa[1] && o->classe == TZC_CASTLE && o->dono == ia->k;
		if (!quer) continue;
		int r[4];
		ia_rect(ia, i, r);
		if (cx >= r[0] && cx <= r[2] && cy >= r[1] && cy <= r[3]) { memcpy(ref, r, sizeof ref); tem_ref = 1; }
	}
	int fx = ia->casa[0], fy = ia->casa[1];
	const TzObj *po = ia_obj(ia, prefer);
	if (po) { fx = po->cx; fy = po->cy; }
	int lw = lim[2] - lim[0] + 1, lh = lim[3] - lim[1] + 1;
	uint8_t *visto = calloc((size_t)lw * (size_t)lh, 1);
	int32_t *fila = malloc(IA_BUSCA * 2 * sizeof *fila);
	if (!visto || !fila) { free(visto); free(fila); return 0; }
	int ini = 0, fim = 0, achou = 0, chao = 0;
	fila[fim++] = cx; fila[fim++] = cy;
	visto[(cy - lim[1]) * lw + (cx - lim[0])] = 1;
	int testadas = 0, est[5] = { 0, 0, 0, 0, 0 };
	while (ini < fim && testadas < IA_BUSCA) {
		int x = fila[ini++], y = fila[ini++];
		testadas++;
		int r[4] = { x, y, x + w - 1, y + h - 1 };
		uint32_t d = tem_ref ? ia_dist_rect(ref, r) : ia_dist(cx, cy, r);
		int p1 = d >= mn && d <= mx, p2 = p1 && espaco(ia, classe, x, y, w, h), p3 = p2 && tz_mundo_cabe(ia->m, ia->k, classe, x, y) == 0;
		if (p2 && !p3 && tz_ia_depurar && est[1] < 4) printf("[DBG]     %d,%d no cabe: %s\n", x, y, tz_erro());
		int p4 = p3 && ia_alcanca(ia, fx, fy, r), p5 = p4 && ia_seguro(ia, x, y, w, h);
		est[0] += p1; est[1] += p2; est[2] += p3; est[3] += p4; est[4] += p5;
		if (p5) {
			*ox = x; *oy = y; achou = 1;
			break;
		}
		int pe = livre_chao(ia, x, y);
		if (pe) chao = 1;
		else if (chao) continue;
		for (int dy = -1; dy <= 1; ++dy)
			for (int dx = -1; dx <= 1; ++dx) {
				int nx = x + dx, ny = y + dy;
				if ((!dx && !dy) || nx < lim[0] || ny < lim[1] || nx > lim[2] || ny > lim[3]) continue;
				uint8_t *v = &visto[(ny - lim[1]) * lw + (nx - lim[0])];
				if (*v || fim + 2 > IA_BUSCA * 2) continue;
				*v = 1;
				fila[fim++] = nx; fila[fim++] = ny;
			}
	}
	free(visto);
	free(fila);
	if (tz_ia_depurar)
		printf("[DBG]   busca %s en %d,%d [%u,%u] %dx%d: %d casillas, distancia %d espacio %d cabe %d alcance %d seguro %d\n",
		       tz_classes[classe], cx, cy, mn, mx, w, h, testadas, est[0], est[1], est[2], est[3], est[4]);
	return achou;
}

/* 0x40a990: un edificio entre min y max del centro: una obra abandonada de su tipo retomada por un campesino libre;
 * sin campesinos libres, nada; si no, la busca de un sitio */
int ia_colocar(TzIa *ia, int classe, int cx, int cy, uint32_t mn, uint32_t mx, TzId prefer)
{
	if (!ia_disponivel_tipo(ia, classe)) return 0;
	for (int j = 0; j < ia->obras.n; ++j) {
		TzId s = ia->obras.v[j];
		const TzObj *o = &ia->m->obj[s];
		if (!ia_obra(o) || o->classe != classe) continue;
		int r[4];
		ia_rect(ia, s, r);
		if (classe != TZC_HARBOR) {
			uint32_t d = ia_dist(cx, cy, r);
			if (d < mn || d > mx) continue;
		}
		TzId b = construtor(ia, r, prefer);
		if (b == TZ_NENHUM) continue;
		if (ia_seguro(ia, r[0] - 1, r[1] - 1, r[2] - r[0] + 3, r[3] - r[1] + 3)) {
			ia_ordem(ia, b, TZ_O_CONSTRUIR, s, o->cx, o->cy);
			ia_lista_tira(&ia->obras, s);
			return 1;
		}
	}
	if (!ia->campones.n) return 0;
	int algum = 0;
	for (int g = G_OCIOSO; g <= G_MADEIRA && !algum; ++g) {
		if (g == G_CONSTRUTOR || g == G_ORDENHA) continue;
		for (int j = 0; j < ia->grupo[g].n && !algum; ++j) algum = ia_livre(ia, ia->grupo[g].v[j]);
	}
	if (!algum) return 0;
	if (classe == TZC_HARBOR) return 0;   /* 0x40a710: los puertos (la flota) no están traducidos */
	int x, y;
	if (!buscar(ia, classe, cx, cy, mn, mx, prefer, &x, &y)) return 0;
	return lancar(ia, classe, x, y, prefer);
}
