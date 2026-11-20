/* SPDX-License-Identifier: GPL-3.0-or-later */
/* terreno.c - suelo de Tzar: losetas, transiciones, árboles, acantilados y minimapa (D417).
 *
 * Traducido de ReTzar, src/terrain.rs y de TreeAtlas/add_trees/add_cliffs de src/world.rs
 * (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later).
 *
 * Una casilla (u16): terreno «a» en los bits 7-9, terreno «b» (o columna del acantilado) en 4-6, forma de la
 * transición en 0-3, acantilado en el bit 10 y árbol (1-31) en 11-15. El bioma (IMAGES/TERRAIN/<b>.TERRAIN.INI)
 * da un atlas por pareja de terrenos ([terrainfiles] «ab=fichero», [terrainsize] columnas,filas[,cuadros de
 * costa]). Las transiciones son máscaras: magenta = terreno a, verde = terreno b; se componen una vez por
 * (código, x % periodo, y % periodo, cuadro) y se guardan. Agua animada (fase 3): el agua (pareja 00) pasa páginas
 * verticales y las costas horizontales, a WATERFPS = 4 (tic = ms / 250, ReTzar frontend/scene.rs: animate); una
 * transición compuesta repite el ciclo común (mínimo común múltiplo) de sus tres losetas, como Terrain::prepare.
 */
#include "tzar.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int mcd(int a, int b) { while (b) { int t = a % b; a = b; b = t; } return a; }
static int mcm(int a, int b) { return a / mcd(a, b) * b; }

static int ler_bmp(TzRecursos *r, const char *nome, int verde, TzImg *o)
{
	uint32_t n;
	uint8_t *d = tz_recursos_ler(r, nome, &n);
	if (!d) return -1;
	int x = tz_bmp(d, n, verde, -1, o);
	free(d);
	if (x) { char m[256]; snprintf(m, sizeof m, "%s", tz_erro()); return tz_falha("%s: %s", nome, m); }
	return 0;
}

int tz_terreno_carregar(TzTerreno *t, TzRecursos *r, const char *bioma)
{
	memset(t, 0, sizeof *t);
	memset(t->pares, 0xff, sizeof t->pares);
	if (!*bioma || strlen(bioma) >= sizeof t->bioma) return tz_falha("bioma del mapa no válido");
	for (const char *q = bioma; *q; ++q)
		if (!isalnum((unsigned char)*q) && *q != '_') return tz_falha("bioma del mapa no válido: %s", bioma);
	snprintf(t->bioma, sizeof t->bioma, "%s", bioma);
	char nome[300];
	snprintf(nome, sizeof nome, "IMAGES/TERRAIN/%s.TERRAIN.INI", bioma);
	TzIni ini = { 0 };
	if (tz_ini_recurso(r, &ini, nome)) return -1;
	const char *dir = tz_ini(&ini, "terrain", "dir");
	snprintf(t->dir, sizeof t->dir, "%s", dir && *dir ? dir : bioma);
	for (const char *q = t->dir; *q; ++q)
		if (*q == '/' || *q == '\\' || *q == '.') { tz_ini_liberar(&ini); return tz_falha("carpeta de terreno no válida"); }
	const TzIniSecao *tf = tz_ini_secao(&ini, "terrainfiles");
	int capf = 0;
	for (int i = 0; tf && i < tf->n; ++i) {
		const char *k = tf->p[i].chave, *f = tf->p[i].valor;
		if (strlen(k) != 2 || !isdigit((unsigned char)k[0]) || !isdigit((unsigned char)k[1])) continue;
		int a = k[0] - '0', b = k[1] - '0';
		if (a > 7 || b > 7) continue;
		int dims[3] = { 0, 0, 0 };
		int nd = tz_ini_numeros(&ini, "terrainsize", k, dims, 3);
		if (!nd) {
			char sem[200];
			snprintf(sem, sizeof sem, "%s", f);
			size_t l = strlen(sem);
			if (l > 4 && !strcmp(sem + l - 4, ".bmp")) sem[l - 4] = 0;
			nd = tz_ini_numeros(&ini, "terrainsize", sem, dims, 3);
		}
		if (!nd) continue;
		if (nd < 2 || dims[0] <= 0 || dims[1] <= 0 || dims[0] > 64 || dims[1] > 64 || dims[2] < 0 || dims[2] > 64) {
			tz_ini_liberar(&ini);
			return tz_falha("rejilla de atlas de terreno no válida (%s)", k);
		}
		snprintf(nome, sizeof nome, "IMAGES/TERRAIN/%s/%s", t->dir, f);
		int at = -1;
		for (int s = 0; s < t->nfolhas; ++s)
			if (!strcasecmp(t->folhas[s].nome, nome) && t->folhas[s].cols == dims[0] && t->folhas[s].rows == dims[1]) at = s;
		if (at < 0) {
			if (t->nfolhas == capf) {
				capf = capf ? capf * 2 : 16;
				TzFolha *nf = realloc(t->folhas, (size_t)capf * sizeof *nf);
				if (!nf) { tz_ini_liberar(&ini); return tz_falha("sin memoria"); }
				t->folhas = nf;
			}
			TzFolha *s = &t->folhas[t->nfolhas];
			memset(s, 0, sizeof *s);
			snprintf(s->nome, sizeof s->nome, "%s", nome);
			s->cols = dims[0]; s->rows = dims[1]; s->quadros_costa = dims[2];
			if (ler_bmp(r, nome, 0, &s->img)) { tz_ini_liberar(&ini); return -1; }
			int w = s->cols * 32, h = s->rows * 32;
			/* el agua anima páginas verticales; las costas, horizontales (tercer campo de terrainsize) */
			s->paginas = !strcmp(k, "00") ? s->img.h / h : (s->quadros_costa > 0 ? s->quadros_costa : 1);
			int ok = s->quadros_costa ? (s->img.w >= w * s->paginas && s->img.h >= h) : (s->img.w >= w && s->img.h >= h);
			if (!ok || s->paginas < 1) { tz_img_liberar(&s->img); tz_ini_liberar(&ini); return tz_falha("atlas de terreno demasiado pequeño: %s", nome); }
			at = t->nfolhas++;
		}
		t->pares[a][b] = (int16_t)at;
	}
	t->periodo[0] = 2; t->periodo[1] = 4;
	for (int a = 0; a < 8; ++a)
		if (t->pares[a][a] >= 0) {
			TzFolha *s = &t->folhas[t->pares[a][a]];
			t->periodo[0] = mcm(t->periodo[0], s->cols);
			t->periodo[1] = mcm(t->periodo[1], s->rows);
			if (t->periodo[0] * t->periodo[1] > 4096) { tz_ini_liberar(&ini); return tz_falha("periodos de los atlas de terreno demasiado grandes"); }
		}
	for (int i = 0; i < 8; ++i) {
		char k[4];
		int v[3];
		snprintf(k, sizeof k, "%d", i);
		if (tz_ini_numeros(&ini, "minidatargb", k, v, 3) >= 3) { t->tem_mini[i] = 1; for (int j = 0; j < 3; ++j) t->mini[i][j] = (uint8_t)v[j]; }
	}
	int v[3];
	t->cor_selecao[0] = t->cor_selecao[1] = t->cor_selecao[2] = 255;
	if (tz_ini_numeros(&ini, "colors", "selection", v, 3) >= 3) for (int j = 0; j < 3; ++j) t->cor_selecao[j] = (uint8_t)v[j];
	if (tz_ini_numeros(&ini, "minidatargb", "tree", v, 3) >= 3) { for (int j = 0; j < 3; ++j) t->mini_arvore[j] = (uint8_t)v[j]; }
	else { t->mini_arvore[0] = 11; t->mini_arvore[1] = 131; t->mini_arvore[2] = 11; }
	if (tz_ini_numeros(&ini, "minidatargb", "cliff", v, 3) >= 3) { for (int j = 0; j < 3; ++j) t->mini_penhasco[j] = (uint8_t)v[j]; }
	else { t->mini_penhasco[0] = 142; t->mini_penhasco[1] = 107; t->mini_penhasco[2] = 72; }

	/* árboles: atlas de 31 columnas y su sombra (que el original pinta al 50 %) */
	const char *fa = tz_ini(&ini, "trees", "img_trees"), *fs = tz_ini(&ini, "trees", "img_shadows");
	snprintf(nome, sizeof nome, "IMAGES/TERRAIN/%s/%s", t->dir, fa && *fa ? fa : "trees.bmp");
	if (tz_recursos_existe(r, nome)) {
		if (ler_bmp(r, nome, 1, &t->arvores)) { tz_ini_liberar(&ini); return -1; }
		snprintf(nome, sizeof nome, "IMAGES/TERRAIN/%s/%s", t->dir, fs && *fs ? fs : "trees_s.bmp");
		if (ler_bmp(r, nome, 1, &t->arvores_s)) { tz_ini_liberar(&ini); return -1; }
		if (t->arvores.w % 31 || t->arvores_s.w % 31 || !t->arvores.w || !t->arvores_s.w) { tz_ini_liberar(&ini); return tz_falha("atlas de árboles no válido"); }
		for (int i = 0; i < t->arvores_s.w * t->arvores_s.h; ++i)
			if (t->arvores_s.px[i]) t->arvores_s.px[i] = TZ_SOMBRA;
	}
	int an[2];
	int na = tz_ini_numeros(&ini, "trees", "pos_tree", an, 2);
	t->arv_anc[0] = (na > 0 ? an[0] : 1) * 32; t->arv_anc[1] = (na > 1 ? an[1] : 3) * 32;
	na = tz_ini_numeros(&ini, "trees", "pos_shadow", an, 2);
	t->arv_s_anc[0] = (na > 0 ? an[0] : 0) * 32; t->arv_s_anc[1] = (na > 1 ? an[1] : 1) * 32;

	/* acantilados: un atlas de 6x14 por tipo de terreno, «<tipo>8.bmp» salvo que [terrainfiles] lo cambie */
	for (int k = 0; k < 8; ++k) {
		char ch[4];
		snprintf(ch, sizeof ch, "%d8", k);
		const char *f = tz_ini(&ini, "terrainfiles", ch);
		char fich[200];
		snprintf(fich, sizeof fich, "%s", f && *f ? f : "");
		if (!*fich) snprintf(fich, sizeof fich, "%d8.bmp", k);
		snprintf(nome, sizeof nome, "IMAGES/TERRAIN/%s/%s", t->dir, fich);
		if (tz_recursos_existe(r, nome)) {
			if (ler_bmp(r, nome, 1, &t->penhascos[k])) { tz_ini_liberar(&ini); return -1; }
			t->tem_penhasco[k] = 1;
		}
	}
	tz_ini_liberar(&ini);
	return 0;
}

/* Loseta sin componer de una casilla: la folha, la esquina en la primera página y el paso. */
static int loseta_simples(TzTerreno *t, uint16_t cel, int x, int y, uint32_t tic, const uint32_t **px, int *paso, int *paginas)
{
	int a = (cel >> 7) & 7;
	/* bajo un acantilado se pinta el terreno a; el otro campo es el cuadro del acantilado */
	int b = cel & 0x400 ? a : (cel >> 4) & 7;
	int forma = cel & 15;
	int s = t->pares[a][b] >= 0 ? t->pares[a][b] : t->pares[b][a];
	if (s < 0) return tz_falha("falta la pareja de terrenos %d%d en %s", a, b, t->bioma);
	TzFolha *f = &t->folhas[s];
	int col, row;
	if (a == b) { col = x % f->cols; row = y % f->rows; }
	else {
		/* el original alterna las variantes de costa en x impar y el bit 1 de y */
		col = (x & 1) && (y & 2) ? 1 : 0;
		if (col > f->cols - 1) col = f->cols - 1;
		row = forma;
	}
	if (row >= f->rows) return tz_falha("transición de terreno no válida: %d", forma);
	int pag = (int)(tic % (uint32_t)f->paginas);
	*paginas = f->paginas;
	if (f->quadros_costa) {
		/* costas: las páginas son columnas, cada variante con sus cuadros seguidos */
		*px = f->img.px + (size_t)row * 32 * f->img.w + (col * f->paginas + pag) * 32;
	} else {
		/* el agua: páginas cuadradas una debajo de otra */
		*px = f->img.px + (size_t)(pag * f->rows * 32 + row * 32) * f->img.w + col * 32;
	}
	*paso = f->img.w;
	return 0;
}

static uint32_t chave_cache(TzTerreno *t, uint16_t cel, int x, int y)
{
	return ((uint32_t)(cel & 0x7ff) * (uint32_t)t->periodo[0] + (uint32_t)(x % t->periodo[0])) * (uint32_t)t->periodo[1] + (uint32_t)(y % t->periodo[1]);
}

const uint32_t *tz_terreno_loseta(TzTerreno *t, uint16_t cel, int x, int y, int *paso)
{
	return tz_terreno_loseta_t(t, cel, x, y, 0, paso);
}

const uint32_t *tz_terreno_loseta_t(TzTerreno *t, uint16_t cel, int x, int y, uint32_t tic, int *paso)
{
	int a = (cel >> 7) & 7, b = (cel >> 4) & 7;
	const uint32_t *px;
	int pg;
	if ((cel & 0x400) || a == b) {
		if (loseta_simples(t, cel, x, y, tic, &px, paso, &pg)) return NULL;
		return px;
	}
	const uint32_t *m, *ta, *tb;
	int pm, pa, pb, gm, ga, gb;
	if (loseta_simples(t, cel, x, y, tic, &m, &pm, &gm)) return NULL;
	if (loseta_simples(t, (uint16_t)(a << 7 | a << 4), x, y, tic, &ta, &pa, &ga)) return NULL;
	if (loseta_simples(t, (uint16_t)(b << 7 | b << 4), x, y, tic, &tb, &pb, &gb)) return NULL;
	/* una transición dura el ciclo común de sus tres losetas */
	int ciclo = mcm(mcm(gm, ga), gb);
	if (ciclo > 256) ciclo = 256;
	uint32_t k = chave_cache(t, cel, x, y) * 256u + (tic % (uint32_t)ciclo);
	if (t->ccap) {
		for (uint32_t i = (k * 2654435761u) & (t->ccap - 1);; i = (i + 1) & (t->ccap - 1)) {
			if (!t->ccod[i]) break;
			if (t->ccod[i] == k + 1) { *paso = 32; return t->cpx[i]; }
		}
	}
	uint32_t *o = tz_malloc(32 * 32 * 4);
	if (!o) return NULL;
	for (int gy = 0; gy < 32; ++gy)
		for (int gx = 0; gx < 32; ++gx) {
			uint32_t c = m[gy * pm + gx] & 0xffffff;
			o[gy * 32 + gx] = c == 0xff00ff ? ta[gy * pa + gx] : c == 0x00ff00 ? tb[gy * pb + gx] : m[gy * pm + gx];
		}
	/* guardar en la tabla (carga < 1/2) */
	if (!t->ccap || t->ccap < 64) {
		t->ccap = 1024;
		t->ccod = calloc(t->ccap, sizeof *t->ccod);
		t->cpx = calloc(t->ccap, sizeof *t->cpx);
		if (!t->ccod || !t->cpx) { free(o); tz_falha("sin memoria"); return NULL; }
	}
	if ((t->cn + 1) * 2 > t->ccap) {
		uint32_t nc = t->ccap * 2;
		uint32_t *ncod = calloc(nc, sizeof *ncod);
		uint32_t **npx = calloc(nc, sizeof *npx);
		if (!ncod || !npx) { free(ncod); free(npx); free(o); tz_falha("sin memoria"); return NULL; }
		for (uint32_t i = 0; i < t->ccap; ++i)
			if (t->ccod[i]) {
				uint32_t j = ((t->ccod[i] - 1) * 2654435761u) & (nc - 1);
				while (ncod[j]) j = (j + 1) & (nc - 1);
				ncod[j] = t->ccod[i]; npx[j] = t->cpx[i];
			}
		free(t->ccod); free(t->cpx);
		t->ccod = ncod; t->cpx = npx; t->ccap = nc;
	}
	uint32_t j = (k * 2654435761u) & (t->ccap - 1);
	while (t->ccod[j]) j = (j + 1) & (t->ccap - 1);
	t->ccod[j] = k + 1;
	t->cpx[j] = o;
	t->cn++;
	*paso = 32;
	return o;
}

/* Un píxel por casilla, como el minimapa original: árboles y acantilados con su color y lo demás con la media
 * de los colores [minidatargb] de sus dos terrenos (o el centro de la loseta si el tipo no tiene color). */
int tz_terreno_minimapa(TzTerreno *t, const TzMapa *m, uint32_t *out)
{
	for (int i = 0; i < m->lado * m->lado; ++i) {
		uint16_t c = m->casillas[i];
		int x = i % m->lado, y = i / m->lado;
		uint8_t rgb[3];
		if (c >> 11) memcpy(rgb, t->mini_arvore, 3);
		else if (c & 0x400) memcpy(rgb, t->mini_penhasco, 3);
		else {
			uint8_t cor[2][3];
			int tipos[2] = { (c >> 7) & 7, (c >> 4) & 7 };
			for (int k = 0; k < 2; ++k) {
				if (t->tem_mini[tipos[k]]) { memcpy(cor[k], t->mini[tipos[k]], 3); continue; }
				int paso;
				const uint32_t *px = tz_terreno_loseta(t, c, x, y, &paso);
				if (!px) return -1;
				uint32_t v = px[16 * paso + 16];
				cor[k][0] = (uint8_t)(v >> 16); cor[k][1] = (uint8_t)(v >> 8); cor[k][2] = (uint8_t)v;
			}
			for (int k = 0; k < 3; ++k) rgb[k] = (uint8_t)((cor[0][k] + cor[1][k]) / 2);
		}
		out[i] = 0xff000000u | (uint32_t)rgb[0] << 16 | (uint32_t)rgb[1] << 8 | rgb[2];
	}
	return 0;
}

void tz_terreno_liberar(TzTerreno *t)
{
	for (int i = 0; i < t->nfolhas; ++i) tz_img_liberar(&t->folhas[i].img);
	free(t->folhas);
	for (uint32_t i = 0; i < t->ccap; ++i) free(t->cpx[i]);
	free(t->ccod); free(t->cpx);
	tz_img_liberar(&t->arvores);
	tz_img_liberar(&t->arvores_s);
	for (int k = 0; k < 8; ++k) tz_img_liberar(&t->penhascos[k]);
	memset(t, 0, sizeof *t);
}
