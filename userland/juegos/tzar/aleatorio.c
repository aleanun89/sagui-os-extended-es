/* SPDX-License-Identifier: GPL-3.0-or-later */
/* aleatorio.c - mapas aleatorios de Tzar (D417, fase 7).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): src/random_map.rs (referencias del Tzar.exe:
 * 0x4b4c80 generación, 0x401610 plasma, 0x4b3c90 formas de inicio, 0x4b2760 codificación de esquinas, 0x4b1560 y 0x4b1880 colocación de
 * objetos). El generador de números es el de ReTzar (SplitMix64, no la corriente global del original). La geometría, los núcleos del
 * campo y la colocación están traducidos.
 *
 * NO se traduce todo: faltan las montañas (la opción de acantilados de ReTzar, apagada por defecto), las ruinas del rey de la colina (solo
 * existe ese modo de juego en ReTzar) y los parches y objetos de suelo decorativos (la edición FX no trae dibujos «gp_» ni «go_», así que
 * no habría nada que colocar). Desde la v8.5.5.2 sí están las puertas mágicas (la mitad que reinos) y los objetos al azar (equipo, libros
 * y pociones con las cifras del original), y las águilas ya son águilas. Lo demás: el plasma, el terreno por capas, los bosques, las formas
 * de inicio con su castillo, torres, minas, aldeanos y árboles, los peces y los animales. */
#include "aleatorio.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define SHAPE_BRANCO 0
#define SHAPE_VERDE  1
#define SHAPE_PRETO  2
#define SHAPE_TERRA  3

static const int LADOS[7] = { 64, 96, 128, 192, 256, 384, 512 };
static const uint8_t PARAMETROS[6][4] = { { 0, 0, 0, 0 }, { 190, 0, 5, 2 }, { 160, 0, 3, 2 }, { 130, 0, 1, 1 }, { 120, 0, 3, 1 }, { 0, 0, 1, 1 } };
static const uint16_t FORMAS[16] = { 15, 6, 10, 2, 12, 4, 8, 0, 13, 5, 9, 1, 11, 3, 7, 15 };

/* ------------------------------------------------------------------ números */
uint64_t tz_aleatorio_proximo(uint64_t *s)
{
	*s += 0x9e3779b97f4a7c15ull;
	uint64_t z = *s;
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
	z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
	return z ^ (z >> 31);
}
static size_t abaixo(uint64_t *s, size_t n) { return (size_t)(tz_aleatorio_proximo(s) % (n ? n : 1)); }
static size_t entre(uint64_t *s, size_t lo, size_t hi) { return lo + abaixo(s, (hi >= lo ? hi - lo : 0) + 1); }

/* ------------------------------------------------------------------ plasma (0x401000/0x401610) */
typedef struct { int n; uint16_t *a; uint64_t rng; int aspereza, divisor; } Campo;

static void campo_set(Campo *f, int x, int y, const int (*pts)[2], int np, int amp)
{
	int i = y * f->n + x;
	if (f->a[i] & 0x8000) return;
	int soma = 0;
	for (int k = 0; k < np; ++k) soma += f->a[pts[k][1] * f->n + pts[k][0]] & 0x7fff;
	int media = soma / np;
	/* el original compara palabras sin signo: un desplazamiento negativo satura en 255, no en 0 */
	uint16_t v = (uint16_t)(media + (int)abaixo(&f->rng, (size_t)amp * 2 + 1) - amp);
	f->a[i] = (uint16_t)((v > 255 ? 255 : v) | 0x8000);
}

static void campo_divide(Campo *f, int x0, int y0, int x1, int y1)
{
	int dx = x1 - x0, dy = y1 - y0;
	if (dx == 1) {
		if (dy == 1) return;
		int amp = 2 * f->aspereza / f->divisor;
		{ const int p[2][2] = { { x0, y0 }, { x0, y0 + 2 } }; campo_set(f, x0, y0 + 1, p, 2, amp); }
		{ const int p[2][2] = { { x1, y0 }, { x1, y0 + 2 } }; campo_set(f, x1, y0 + 1, p, 2, amp); }
		return;
	}
	int x = x0 + dx / 2, y = y0 + dy / 2;
	int ax = dx * f->aspereza / f->divisor, ay = dy * f->aspereza / f->divisor;
	{ const int p[2][2] = { { x1, y0 }, { x1, y1 } }; campo_set(f, x1, y, p, 2, ay); }
	{ const int p[2][2] = { { x0, y0 }, { x0, y1 } }; campo_set(f, x0, y, p, 2, ay); }
	{ const int p[2][2] = { { x0, y0 }, { x1, y0 } }; campo_set(f, x, y0, p, 2, ax); }
	{ const int p[2][2] = { { x0, y1 }, { x1, y1 } }; campo_set(f, x, y1, p, 2, ax); }
	{ const int p[4][2] = { { x0, y0 }, { x1, y0 }, { x1, y1 }, { x0, y1 } }; campo_set(f, x, y, p, 4, ax); }
	campo_divide(f, x0, y0, x, y);
	campo_divide(f, x, y0, x1, y);
	campo_divide(f, x, y, x1, y1);
	campo_divide(f, x0, y, x, y1);
}

void tz_aleatorio_plasma(int tamanho, uint64_t semente, int aspereza, int divisor, uint8_t *saida)
{
	Campo f;
	f.n = tamanho;
	f.a = calloc((size_t)tamanho * (size_t)tamanho, sizeof *f.a);
	f.rng = semente;
	f.aspereza = aspereza;
	f.divisor = divisor < 1 ? 1 : divisor;
	if (!f.a) { memset(saida, 0, (size_t)tamanho * (size_t)tamanho); return; }
	const int cantos[4] = { 0, tamanho - 1, tamanho * tamanho - 1, (tamanho - 1) * tamanho };
	for (int k = 0; k < 4; ++k) f.a[cantos[k]] = (uint16_t)abaixo(&f.rng, 256);
	campo_divide(&f, 0, 0, tamanho - 1, tamanho - 1);
	for (int i = 0; i < tamanho * tamanho; ++i) saida[i] = (uint8_t)f.a[i];
	free(f.a);
}

static uint8_t *plasma512(uint64_t semente, int asp, int div)
{
	uint8_t *p = malloc(512 * 512);
	if (p) tz_aleatorio_plasma(512, semente, asp, div, p);
	return p;
}
static uint8_t amostra(const uint8_t *campo, int lado, int x, int y)
{
	int yy = y < lado - 1 ? y : lado - 1, xx = x < lado - 1 ? x : lado - 1;
	return campo[(yy * 512 / lado) * 512 + xx * 512 / lado];
}

/* ------------------------------------------------------------------ sectores y formas de inicio */
typedef struct { int x0, y0, x1, y1; } Rect;

static int sectores(int lado, int jog, Rect *s)
{
	int h = lado / 2, q = lado / 4, t = lado / 3;
	switch (jog) {
	case 2: s[0] = (Rect){ 0, 0, lado - 1, h - 1 }; s[1] = (Rect){ 0, h, lado - 1, lado - 1 }; return 2;
	case 3: s[0] = (Rect){ 0, 0, h - 1, h - 1 }; s[1] = (Rect){ h, 0, lado - 1, h - 1 }; s[2] = (Rect){ q, h, q + h - 1, lado - 1 }; return 3;
	case 4: s[0] = (Rect){ 0, 0, h - 1, h - 1 }; s[1] = (Rect){ h, 0, lado - 1, h - 1 }; s[2] = (Rect){ h, h, lado - 1, lado - 1 }; s[3] = (Rect){ 0, h, h - 1, lado - 1 }; return 4;
	case 5: s[0] = (Rect){ 0, 0, t - 1, h - 1 }; s[1] = (Rect){ t, 0, 2 * t - 1, h - 1 }; s[2] = (Rect){ 2 * t, 0, 3 * t - 1, h - 1 };
		s[3] = (Rect){ h, h, h + t - 1, lado - 1 }; s[4] = (Rect){ lado / 6, h, lado / 6 + t - 1, lado - 1 }; return 5;
	case 6: s[0] = (Rect){ 0, 0, t - 1, h - 1 }; s[1] = (Rect){ t, 0, 2 * t - 1, h - 1 }; s[2] = (Rect){ 2 * t, 0, 3 * t - 1, h - 1 };
		s[3] = (Rect){ 2 * t, h, 3 * t - 1, lado - 1 }; s[4] = (Rect){ t, h, 2 * t - 1, lado - 1 }; s[5] = (Rect){ 0, h, t - 1, h + t - 1 }; return 6;
	case 7: s[0] = (Rect){ 0, 0, q - 1, h - 1 }; s[1] = (Rect){ q, 0, 2 * q - 1, h - 1 }; s[2] = (Rect){ 2 * q, 0, 3 * q - 1, h - 1 };
		s[3] = (Rect){ 3 * q, 0, lado - 1, h - 1 }; s[4] = (Rect){ 2 * t, h, 3 * t - 1, lado - 1 }; s[5] = (Rect){ t, h, 2 * t - 1, lado - 1 };
		s[6] = (Rect){ 0, h, t - 1, h + t - 1 }; return 7;
	default: s[0] = (Rect){ 0, 0, q - 1, h - 1 }; s[1] = (Rect){ q, 0, 2 * q - 1, h - 1 }; s[2] = (Rect){ 2 * q, 0, 3 * q - 1, h - 1 };
		s[3] = (Rect){ 3 * q, 0, lado - 1, h - 1 }; s[4] = (Rect){ 3 * q, h, lado - 1, lado - 1 }; s[5] = (Rect){ 2 * q, h, 3 * q - 1, lado - 1 };
		s[6] = (Rect){ q, h, 2 * q - 1, lado - 1 }; s[7] = (Rect){ 0, h, q - 1, lado - 1 }; return 8;
	}
}

typedef struct { int x, y, w, h; uint8_t *px; Rect sector; } Forma;

/* 0x4b04a0: agrupa por equipo, baraja los grupos y después sus miembros; los cuatro huecos de grupo cuentan aunque estén vacíos */
static void orden_equipos(const uint8_t *eq, int n, uint64_t *rng, int *orden)
{
	int grupos[16][8], ng_[16];
	int claves[16], ng = 0;
	/* BTreeMap: los grupos van por clave de equipo creciente */
	for (int p = 0; p < n; ++p) {
		int g = -1;
		for (int k = 0; k < ng; ++k) if (claves[k] == eq[p]) g = k;
		if (g < 0) { g = ng++; claves[g] = eq[p]; ng_[g] = 0; }
		grupos[g][ng_[g]++] = p;
	}
	for (int i = 1; i < ng; ++i) {   /* orden por clave */
		int kc = claves[i], gm[8], nm = ng_[i];
		memcpy(gm, grupos[i], sizeof gm);
		int j = i - 1;
		while (j >= 0 && claves[j] > kc) { claves[j + 1] = claves[j]; memcpy(grupos[j + 1], grupos[j], sizeof gm); ng_[j + 1] = ng_[j]; j--; }
		claves[j + 1] = kc; memcpy(grupos[j + 1], gm, sizeof gm); ng_[j + 1] = nm;
	}
	while (ng < 4) { ng_[ng] = 0; ng++; }
	int no = 0;
	while (ng > 0) {
		int g = (int)abaixo(rng, (size_t)ng);
		int gm[8], nm = ng_[g];
		memcpy(gm, grupos[g], sizeof gm);
		for (int k = g; k + 1 < ng; ++k) { memcpy(grupos[k], grupos[k + 1], sizeof gm); ng_[k] = ng_[k + 1]; }
		ng--;
		while (nm > 0) {
			int idx = (int)abaixo(rng, (size_t)nm);
			orden[no++] = gm[idx];
			for (int k = idx; k + 1 < nm; ++k) gm[k] = gm[k + 1];
			nm--;
		}
	}
}

static int formas_inicio(TzRecursos *r, int lado, int jog, uint64_t semente, const uint8_t *eq, Forma *out)
{
	uint64_t rng = semente ^ 0x5348415045ull;
	int area = lado * lado / jog / 512;
	int familia = area < 35 ? 30 : area < 50 ? 40 : 60;
	Rect sec[8], regioes[8];
	sectores(lado, jog, sec);
	int orden[16];
	orden_equipos(eq, jog, &rng, orden);
	for (int s = 0; s < jog; ++s) regioes[orden[s]] = sec[s];
	for (int p = 0; p < jog; ++p) {
		char nome[100];
		snprintf(nome, sizeof nome, "IMAGES/RANDOMMAP/%d_RND_SHAPE%d.BMP", familia, (int)abaixo(&rng, 3) + 1);
		uint32_t nb;
		uint8_t *d = tz_recursos_ler(r, nome, &nb);
		if (!d) return tz_falha("falta la forma de inicio %s", nome);
		TzImg im;
		int rc = tz_bmp(d, nb, 0, -1, &im);
		free(d);
		if (rc) return -1;
		Rect re = regioes[p];
		int w = im.w < re.x1 - re.x0 - 1 ? im.w : re.x1 - re.x0 - 1;
		int h = im.h < re.y1 - re.y0 - 1 ? im.h : re.y1 - re.y0 - 1;
		if (w < 1 || h < 1) { tz_img_liberar(&im); return tz_falha("sector demasiado pequeño"); }
		Forma *f = &out[p];
		f->w = w; f->h = h;
		f->x = re.x0 + 1 + (int)abaixo(&rng, (size_t)((re.x1 - re.x0 - w) > 1 ? re.x1 - re.x0 - w : 1));
		f->y = re.y0 + 1 + (int)abaixo(&rng, (size_t)((re.y1 - re.y0 - h) > 1 ? re.y1 - re.y0 - h : 1));
		f->sector = re;
		f->px = malloc((size_t)w * (size_t)h);
		if (!f->px) { tz_img_liberar(&im); return -1; }
		for (int yy = 0; yy < h; ++yy)
			for (int xx = 0; xx < w; ++xx) {
				uint32_t c = im.px[(yy * im.h / h) * im.w + xx * im.w / w];
				int rr = (c >> 16 & 255) >> 3, gg = (c >> 8 & 255) >> 3, bb = (c & 255) >> 3;
				f->px[yy * w + xx] = (uint8_t)(rr == 0 && gg == 31 && bb == 0 ? SHAPE_VERDE : rr == 0 && gg == 0 && bb == 0 ? SHAPE_PRETO :
				                               rr == 31 && gg == 31 && bb == 31 ? SHAPE_BRANCO : SHAPE_TERRA);
			}
		tz_img_liberar(&im);
	}
	return 0;
}

/* ------------------------------------------------------------------ terreno */
static uint16_t codificar(const uint8_t c[4])
{
	uint16_t a = c[0], b = c[0];
	for (int i = 1; i < 4; ++i) { if (c[i] < a) a = c[i]; if (c[i] > b) b = c[i]; }
	if (a == b) return (uint16_t)((a << 7) | (b << 4) | FORMAS[15]);
	int mask = 0;
	for (int i = 0; i < 4; ++i) mask |= (c[i] == b) << (3 - i);
	return (uint16_t)((a << 7) | (b << 4) | FORMAS[mask]);
}

static uint8_t *vertices(int lado, uint64_t semente, const uint8_t p[4], int invertido)
{
	uint8_t *campo = plasma512(semente, p[2], p[3]);
	int n = lado + 1;
	uint8_t *v = calloc((size_t)n * (size_t)n, 1);
	if (!campo || !v) { free(campo); free(v); return NULL; }
	for (int y = 0; y <= lado; ++y)
		for (int x = 0; x <= lado; ++x) {
			int h = amostra(campo, lado, x, y);
			v[y * n + x] = (uint8_t)(invertido ? h <= p[0] : h > p[0]);
		}
	free(campo);
	return v;
}

static void passo_capa(uint8_t *v, int lado, const uint8_t *campo, int pinta, int base, int min, int max)
{
	int stride = lado + 1;
	for (int y = 0; y < lado; ++y)
		for (int x = 0; x < lado; ++x) {
			int i = y * stride + x;
			int h = amostra(campo, lado, x, y);
			if (v[i] != base || h < min || h > max) continue;
			int ok = 1;
			for (int yy = y ? y - 1 : 0; yy <= y + 1 && ok; ++yy)
				for (int xx = x ? x - 1 : 0; xx <= x + 1; ++xx) { int t = v[yy * stride + xx]; if (t != base && t != pinta) { ok = 0; break; } }
			if (ok) v[i] = (uint8_t)pinta;
		}
}

typedef struct { int n; int16_t *lista[8]; int cnt[8]; } Arvores;   /* por terreno: los dibujos de árbol permitidos */

static void arvores_ler(const TzIni *ini, Arvores *a)
{
	memset(a, 0, sizeof *a);
	const TzIniSecao *nomes = tz_ini_secao(ini, "treename"), *terr = tz_ini_secao(ini, "treeterrain");
	if (!nomes || !terr) return;
	int comeco[256], cuenta[256], next = 0;
	for (int i = 0; i < nomes->n && i < 256; ++i) { comeco[i] = next; cuenta[i] = atoi(nomes->p[i].valor); if (cuenta[i] < 0) cuenta[i] = 0; next += cuenta[i]; }
	for (int t = 0; t < 8; ++t) { a->lista[t] = malloc((size_t)(next + 1) * sizeof(int16_t)); a->cnt[t] = 0; }
	for (int i = 0; i < terr->n; ++i) {
		int k = -1;
		for (int j = 0; j < nomes->n && j < 256; ++j) if (!strcasecmp(nomes->p[j].chave, terr->p[i].chave)) { k = j; break; }
		if (k < 0) continue;
		const char *s = terr->p[i].valor;
		while (*s) {
			char *fim;
			long t = strtol(s, &fim, 10);
			if (fim != s && t >= 0 && t < 8 && a->lista[t])
				for (int q = comeco[k]; q < comeco[k] + cuenta[k]; ++q) a->lista[t][a->cnt[t]++] = (int16_t)q;
			s = fim == s ? s + 1 : fim;
			while (*s == ',' || *s == ' ') s++;
		}
	}
	/* ordenados y sin repetidos */
	for (int t = 0; t < 8; ++t) {
		if (!a->lista[t]) continue;
		for (int i = 1; i < a->cnt[t]; ++i) { int16_t x = a->lista[t][i]; int j = i - 1; while (j >= 0 && a->lista[t][j] > x) { a->lista[t][j + 1] = a->lista[t][j]; j--; } a->lista[t][j + 1] = x; }
		int w = 0;
		for (int i = 0; i < a->cnt[t]; ++i) if (!w || a->lista[t][w - 1] != a->lista[t][i]) a->lista[t][w++] = a->lista[t][i];
		a->cnt[t] = w;
	}
}
static void arvores_liberar(Arvores *a) { for (int t = 0; t < 8; ++t) free(a->lista[t]); }

static uint16_t bits_arvore(const Arvores *a, int terreno, size_t r)
{
	int cnt = a->lista[terreno] ? a->cnt[terreno] : 0;
	unsigned rec = r < (size_t)cnt ? (unsigned)a->lista[terreno][r] : 0xffffu;
	return (uint16_t)((rec & 0x1f) << 11);
}
/* los dos terrenos de una casilla, cuántos árboles caben y el terreno que elige la especie */
static void terreno_arvore(const Arvores *a, uint16_t cel, int *n, int *terr)
{
	int t0 = cel >> 7 & 7, t1 = cel >> 4 & 7;
	int n0 = a->lista[t0] ? a->cnt[t0] : 0, n1 = a->lista[t1] ? a->cnt[t1] : 0;
	*n = n0 > n1 ? n0 : n1;
	*terr = n0 > n1 ? t0 : t1;
}

static int num_ini(const TzIni *ini, const char *sec, const char *chave, int *v, int max) { return tz_ini_numeros(ini, sec, chave, v, max); }

static void natureza(uint16_t *cel, int lado, const TzIni *ini, const uint8_t *reservado, uint64_t semente)
{
	int ajuste[8] = { 0 };
	char k[16];
	snprintf(k, sizeof k, "%d", lado);
	int nset = num_ini(ini, "randomtreeset", k, ajuste, 8);
	#define AJ(i, d) ((i) < nset ? ajuste[i] : (d))
	const char *w = tz_ini(ini, "terrain", "water");
	int agua = w ? atoi(w) : 1;
	Arvores arv;
	arvores_ler(ini, &arv);
	int a2 = AJ(2, 11), a3 = AJ(3, 5);
	uint8_t *floresta = plasma512(semente ^ 0x464f52455354ull, a2 < 1 ? 1 : a2 > 255 ? 255 : a2, a3 < 1 ? 1 : a3 > 255 ? 255 : a3);
	uint64_t rng = semente ^ 0x5452454553ull;
	if (floresta) {
		for (int y = 0; y < lado; ++y)
			for (int x = 0; x < lado; ++x) {
				int i = y * lado + x;
				uint16_t c = cel[i];
				int ring = 1;
				for (int dy = 0; dy <= 4 && ring; dy += 2)
					for (int dx = 0; dx <= 4; dx += 2) {
						if (dx == 2 && dy == 2) continue;
						int xx = x + dx - 2, yy = y + dy - 2;
						if (xx < 0 || yy < 0 || xx >= lado || yy >= lado) continue;
						uint16_t o = cel[yy * lado + xx];
						if (!((o >> 7 & 7) >= agua && (o >> 4 & 7) >= agua)) { ring = 0; break; }
					}
				int n, terr;
				terreno_arvore(&arv, c, &n, &terr);
				if (reservado[i] || !ring || (c & 0xfc00) != 0 || !((c >> 7 & 7) >= agua && (c >> 4 & 7) >= agua) || n == 0) continue;
				int h = amostra(floresta, lado, x, y);
				int kk;
				if (h >= AJ(0, 1) && h <= AJ(1, 255)) {
					int d = h > AJ(7, 235) ? AJ(6, 40) : AJ(5, 1);
					kk = 100 / (d < 1 ? 1 : d);
				} else if ((int)abaixo(&rng, 10001) <= AJ(4, 1)) kk = 1;
				else continue;
				size_t r2 = abaixo(&rng, (size_t)n * (size_t)(kk > 0 ? kk : 0) + 1);
				if (r2 <= (size_t)n) cel[i] |= bits_arvore(&arv, terr, r2);
			}
	}
	free(floresta);
	arvores_liberar(&arv);
	#undef AJ
}

/* ------------------------------------------------------------------ capas de las formas: bloques */
typedef struct { uint16_t x, y; } Pt;
typedef struct { Pt *v; int n, cap; } Pts;
static void pts_add(Pts *l, int x, int y) { if (l->n == l->cap) { l->cap = l->cap ? l->cap * 2 : 64; l->v = realloc(l->v, (size_t)l->cap * sizeof *l->v); } l->v[l->n].x = (uint16_t)x; l->v[l->n].y = (uint16_t)y; l->n++; }

static int bloco(const uint8_t *capa, int w, int h, int x, int y, int sw, int sh)
{
	if (x + sw > w || y + sh > h) return 0;
	for (int yy = y; yy < y + sh; ++yy) for (int xx = x; xx < x + sw; ++xx) if (!capa[yy * w + xx]) return 0;
	return 1;
}
static void esquinas(const uint8_t *capa, int w, int h, int sw, int sh, Pts *out)
{
	out->n = 0;
	for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) if (bloco(capa, w, h, x, y, sw, sh)) pts_add(out, x, y);
}
static void limpar(uint8_t *capa, int w, Pts *l, int ax, int ay, int sw, int sh)
{
	for (int y = ay; y < ay + sh; ++y) for (int x = ax; x < ax + sw; ++x) capa[y * w + x] = 0;
	int k = 0;
	for (int i = 0; i < l->n; ++i) {
		int x = l->v[i].x, y = l->v[i].y;
		if (!(x >= ax && x < ax + sw && y >= ay && y < ay + sh)) l->v[k++] = l->v[i];
	}
	l->n = k;
}

/* ------------------------------------------------------------------ el suelo del generador */
#define AR        0x800000u
#define EM_TERRA  0x1000000u
#define EM_AGUA   0x2000000u
#define OCUPADO   0x10000u

typedef struct { uint16_t *cel; uint32_t *marcas; int lado, agua; } Chao;

static uint32_t palavra(const Chao *c, int x, int y) { int i = y * c->lado + x; return (uint32_t)c->cel[i] | c->marcas[i]; }
static int terra(const Chao *c, uint32_t w) { return (int)(w >> 7 & 7) >= c->agua && (int)(w >> 4 & 7) >= c->agua; }
static void marcar(Chao *c, int ax, int ay, int sw, int sh, uint32_t bits)
{
	for (int y = ay; y < ay + sh && y < c->lado; ++y) for (int x = ax; x < ax + sw && x < c->lado; ++x) c->marcas[y * c->lado + x] |= bits;
}
static void tomar(Chao *c, int ax, int ay, int sw, int sh) { marcar(c, ax, ay, sw, sh, EM_TERRA | OCUPADO); }
static int cabe(const Chao *c, int camada, int ax, int ay, int tam)
{
	for (int y = ay; y < ay + tam; ++y)
		for (int x = ax; x < ax + tam; ++x) {
			uint32_t w = palavra(c, x, y);
			int ok = camada == 0 ? (w & AR) == 0 : camada == 1 ? (terra(c, w) && (w & 0x108fc00u) == 0) :
			         camada == 2 ? ((int)(w >> 7 & 7) < c->agua && (int)(w >> 4 & 7) < c->agua && (w & EM_AGUA) == 0) : 1;
			if (!ok) return 0;
		}
	return 1;
}
/* 0x4b1560: hasta `cuenta` objetos en casillas al azar con margen `margen` (16 para pájaros), en 6999 sorteos */
static int espalhar(Chao *c, uint64_t *rng, int classe, int camada, int tw, int th, int cuenta, int margen, Pt *saida, int max)
{
	int borda = (classe == 112 || classe == 119) ? 16 : margen;   /* bird, eagle */
	int colocados = 0;
	if (classe == TZC_COW || cuenta == 0 || c->lado < 2 * (borda > margen ? borda : margen) + 1) return 0;
	for (int t = 1; t < 7000; ++t) {
		int ax = (int)entre(rng, (size_t)borda, (size_t)(c->lado - borda - 1));
		int ay = (int)entre(rng, (size_t)borda, (size_t)(c->lado - borda - 1));
		if (camada <= 2 && cabe(c, camada, ax, ay, margen)) {
			marcar(c, ax, ay, tw, th, camada == 0 ? AR : camada == 1 ? EM_TERRA : EM_AGUA);
			if (colocados < max) { saida[colocados].x = (uint16_t)ax; saida[colocados].y = (uint16_t)ay; }
			colocados++;
		}
		if (colocados >= cuenta) break;
	}
	return colocados < max ? colocados : max;
}

/* ------------------------------------------------------------------ objetos nuevos */
static int base_guardada(int c)
{
	if (c == 107 || c == 108) return 82;
	if (c == 81 || c == 103 || c == 104 || c == 106) return 79;
	if (c == 68) return 67;
	return c;
}

typedef struct { TzMapa *m; int cap; int prox_id; const TzCatalogo *cat; } Lista;

static TzObjeto *novo_objeto(Lista *l, int classe, int dono, int civ, int x, int y, int tam)
{
	if (l->m->nobj == l->cap) {
		l->cap = l->cap ? l->cap * 2 : 256;
		TzObjeto *nv = realloc(l->m->obj, (size_t)l->cap * sizeof *nv);
		if (!nv) return NULL;
		l->m->obj = nv;
	}
	TzObjeto *o = &l->m->obj[l->m->nobj];
	memset(o, 0, sizeof *o);
	int vida = 100;
	if (l->cat) { const TzAtributos *at = tz_catalogo_at(l->cat, classe, civ ? civ : 1); if (at && at->existe && at->vida) vida = at->vida; }
	o->id = (uint16_t)l->prox_id++;
	o->classe = (uint16_t)classe; o->x = (uint16_t)x; o->y = (uint16_t)y; o->dono = (uint8_t)dono; o->civ = (uint8_t)civ;
	o->vida = o->vida_max = (uint16_t)vida;
	o->ndados = (uint32_t)tam;
	o->dados = calloc(1, (size_t)tam);
	if (!o->dados) return NULL;
	uint8_t *p = o->dados;
	p[0] = (uint8_t)tam; p[1] = (uint8_t)(tam >> 8);
	p[2] = (uint8_t)base_guardada(classe); p[3] = (uint8_t)(base_guardada(classe) >> 8);
	p[8] = (uint8_t)x; p[9] = (uint8_t)(x >> 8); p[10] = (uint8_t)y; p[11] = (uint8_t)(y >> 8);
	p[12] = (uint8_t)dono; p[13] = (uint8_t)civ; p[14] = (uint8_t)o->id; p[15] = (uint8_t)(o->id >> 8);
	p[18] = (uint8_t)vida; p[19] = (uint8_t)(vida >> 8); p[20] = (uint8_t)vida; p[21] = (uint8_t)(vida >> 8);
	l->m->nobj++;
	return o;
}

static void poner_cantidad(TzObjeto *o, uint32_t n)
{
	int at = o->classe == TZC_FISHPATCH ? 56 : 58;
	o->dados[at] = (uint8_t)n; o->dados[at + 1] = (uint8_t)(n >> 8); o->dados[at + 2] = (uint8_t)(n >> 16); o->dados[at + 3] = (uint8_t)(n >> 24);
}

static void pegada(const TzCatalogo *cat, int classe, int civ, int def, int *w, int *h)
{
	*w = *h = def;
	if (!cat) return;
	const TzAtributos *at = tz_catalogo_at(cat, classe, civ);
	if ((!at || !at->pegada[0]) && civ) at = tz_catalogo_at(cat, classe, 0);
	if (at && at->pegada[0] && at->pegada[1]) { *w = at->pegada[0]; *h = at->pegada[1]; }
}

/* ------------------------------------------------------------------ la generación */
void tz_aleatorio_padrao(TzAleatorio *o)
{
	memset(o, 0, sizeof *o);
	o->lado = 128; o->jogadores = 2; o->tipo = 0; o->recursos_mapa = 1; o->nivel_recursos = 1; o->guardado = 1; o->animais = 1; o->portas = 1; o->itens = 1; o->penhascos = 0;
	snprintf(o->bioma, sizeof o->bioma, "grass");
	o->semente = 417;
}

int tz_aleatorio_texto(const TzAleatorio *op, char *out, size_t n)
{
	int k = snprintf(out, n, "ALEAT %d %d %d %d %d %d %d %d %s %llu", op->lado, op->jogadores, op->humano, op->tipo, op->recursos_mapa,
	                 op->nivel_recursos, op->guardado, op->animais, op->bioma[0] ? op->bioma : "grass", (unsigned long long)op->semente);
	for (int i = 0; i < 8 && k > 0 && (size_t)k < n; ++i) k += snprintf(out + k, n - (size_t)k, " %d", op->equipe[i]);
	for (int i = 0; i < 8 && k > 0 && (size_t)k < n; ++i) k += snprintf(out + k, n - (size_t)k, " %d", op->civ[i]);
	if (k > 0 && (size_t)k < n) k += snprintf(out + k, n - (size_t)k, " %d %d %d", op->portas != 0, op->itens != 0, op->penhascos != 0);
	/* D470: comportamiento de la IA de cada reino y rendición (solo si hay algo) */
	int ia = op->rendicao != 0;
	for (int i = 0; i < 8; ++i) ia |= op->ia[i][0] != 0;
	if (ia && k > 0 && (size_t)k < n) k += snprintf(out + k, n - (size_t)k, " IA %d", op->rendicao != 0);
	for (int i = 0; ia && i < 8 && k > 0 && (size_t)k < n; ++i) k += snprintf(out + k, n - (size_t)k, " %s", op->ia[i][0] ? op->ia[i] : "-");
	return k > 0 && (size_t)k < n ? 0 : tz_falha("no cabe el texto del mapa aleatorio");
}

/* la siguiente palabra del texto (separadas por espacios); NULL al acabar */
static const char *pal_texto(const char *p, char *out, size_t n, const char **fim)
{
	while (*p == ' ') ++p;
	if (!*p) return NULL;
	size_t k = 0;
	while (*p && *p != ' ') { if (k + 1 < n) out[k++] = *p; ++p; }
	out[k] = 0;
	*fim = p;
	return out;
}

int tz_aleatorio_de_texto(const char *t, TzAleatorio *op)
{
	char w[40];
	const char *p = t;
	if (!pal_texto(p, w, sizeof w, &p) || strcmp(w, "ALEAT")) return tz_falha("no es un mapa aleatorio guardado");
	TzAleatorio o;
	tz_aleatorio_padrao(&o);
	int v[8];
	for (int i = 0; i < 8; ++i) {
		if (!pal_texto(p, w, sizeof w, &p)) return tz_falha("texto de mapa aleatorio incompleto");
		v[i] = atoi(w);
	}
	o.lado = v[0]; o.jogadores = v[1]; o.humano = v[2]; o.tipo = v[3]; o.recursos_mapa = v[4]; o.nivel_recursos = v[5]; o.guardado = v[6]; o.animais = v[7];
	if (!pal_texto(p, w, sizeof w, &p)) return tz_falha("texto de mapa aleatorio incompleto");
	snprintf(o.bioma, sizeof o.bioma, "%s", w);
	if (!pal_texto(p, w, sizeof w, &p)) return tz_falha("texto de mapa aleatorio incompleto");
	o.semente = strtoull(w, NULL, 10);
	for (int i = 0; i < 16; ++i) {
		if (!pal_texto(p, w, sizeof w, &p)) return tz_falha("texto de mapa aleatorio incompleto");
		int x = atoi(w);
		if (x < 0 || x > 8) return tz_falha("texto de mapa aleatorio fuera de rango");
		if (i < 8) o.equipe[i] = (uint8_t)x; else o.civ[i - 8] = (uint8_t)x;
	}
	/* puertas y objetos: opcionales (valen 1 si faltan) */
	if (pal_texto(p, w, sizeof w, &p)) { o.portas = atoi(w) != 0; if (pal_texto(p, w, sizeof w, &p)) o.itens = atoi(w) != 0; }
	/* v8.5.6: «penhascos» (opcional) y luego «IA rendición guion[8]» (D470, opcional) */
	int tem = pal_texto(p, w, sizeof w, &p);
	if (tem && strcmp(w, "IA")) { o.penhascos = atoi(w) != 0; tem = pal_texto(p, w, sizeof w, &p); }
	if (tem && !strcmp(w, "IA")) {
		if (!pal_texto(p, w, sizeof w, &p)) return tz_falha("texto de mapa aleatorio incompleto");
		o.rendicao = atoi(w) != 0;
		for (int i = 0; i < 8; ++i) {
			if (!pal_texto(p, w, sizeof w, &p)) return tz_falha("texto de mapa aleatorio incompleto");
			if (strcmp(w, "-")) {
				if (strchr(w, '/') || strchr(w, '\\') || strstr(w, "..") || strlen(w) > 32) return tz_falha("guion de IA no válido: %s", w);
				snprintf(o.ia[i], sizeof o.ia[i], "%s", w);
			}
		}
	}
	int lado_ok = 0;
	for (int i = 0; i < 7; ++i) if (LADOS[i] == o.lado) lado_ok = 1;
	if (!lado_ok || o.jogadores < 2 || o.jogadores > 8 || o.humano < 0 || o.humano >= o.jogadores || o.tipo < 0 || o.tipo > 5 || o.recursos_mapa < 0 ||
	    o.recursos_mapa > 2 || o.nivel_recursos < 0 || o.nivel_recursos > 2)
		return tz_falha("parámetros de mapa aleatorio fuera de rango");
	*op = o;
	return 0;
}

static void liberar_formas(Forma *f, int n) { for (int i = 0; i < n; ++i) free(f[i].px); }

/* random_map.rs item (0x4b5c50-0x4b6150): el dibujo, el hechizo y las cifras de un objeto al azar. Registro de 106 bytes: nombre del dibujo
 * en 56, vida 0x12, ataque 0x16 y 0x17, defensa 0x18, experiencia 0x1c, maná 0x24, hechizo 0x60, cargas 0x66 */
static void palavra_reg(TzObjeto *o, int at, unsigned v) { o->dados[at] = (uint8_t)v; o->dados[at + 1] = (uint8_t)(v >> 8); }

static void preencher_item(TzObjeto *o, uint64_t *rng)
{
	static const uint8_t HECHIZOS[7] = { 9, 17, 14, 1, 12, 5, 2 };
	char imagem[32];
	int at, bonus;
	if (o->classe == TZC_LIVRO) {
		snprintf(imagem, sizeof imagem, "item_book%d", (int)abaixo(rng, 3) + 1);
		at = 102; bonus = 1 + (int)abaixo(rng, 3);
	} else if (o->classe == TZC_ITEM_MAIS) {
		int t = (int)entre(rng, 2, 4);
		if (t == 2) { snprintf(imagem, sizeof imagem, "item_bow%d", (int)abaixo(rng, 3) + 1); at = 22; bonus = (int)entre(rng, 5, 15) << 8; }
		else if (t == 3) { snprintf(imagem, sizeof imagem, "item_shield%d", (int)abaixo(rng, 3) + 1); at = 24; bonus = (int)entre(rng, 5, 15); }
		else {
			static const char *const esp[4] = { "item_sword", "item_sword1", "item_sword2", "item_sword3" };
			snprintf(imagem, sizeof imagem, "%s", esp[abaixo(rng, 4)]); at = 22; bonus = (int)entre(rng, 5, 15);
		}
	} else {
		int t = abaixo(rng, 2) == 1 ? (int)entre(rng, 2, 4) : (int)abaixo(rng, 2);
		static const char *const pocao[5] = { "item_potion2", "item_potion4", "item_potion3", "item_potion7", "item_potion6" };
		static const int onde[5] = { 18, 36, 28, 22, 24 };
		snprintf(imagem, sizeof imagem, "%s", pocao[t]);
		at = onde[t];
		bonus = t < 2 ? (int)entre(rng, 10, 50) : (int)entre(rng, 2, 8);
	}
	memcpy(o->dados + 56, imagem, strlen(imagem) < 31 ? strlen(imagem) : 31);
	o->vida = o->vida_max = 0;
	o->dados[18] = o->dados[19] = o->dados[20] = o->dados[21] = 0;
	palavra_reg(o, at, (unsigned)bonus);
	if (o->classe == TZC_LIVRO) {
		o->dados[96] = HECHIZOS[abaixo(rng, 7)];
		o->dados[98] = 1; o->dados[99] = 1;
		palavra_reg(o, 100, (unsigned)entre(rng, 1, 5));
	} else if (o->classe == TZC_ITEM_MAIS) o->dados[96] = !strncmp(imagem, "item_shield", 11) ? 2 : 1;
}

/* ------------------------------------------------------------------ montañas (random_map.rs mountains, native_cliffs; 0x4b33b0)
 * Cadenas de piezas de 2x2 casillas (acantilados) que siguen la pendiente del campo del plasma: el recorrido (cliff_path 0x4b3090),
 * el giro o remate de cada pieza (cliff_turn, cliff_cap) y su cuadro del atlas (tablas 0x58e7fc y 0x58e814, en losetas de 32 px). Solo
 * sobre tierra firme sin la forma de inicio de ningún reino. Una casilla de acantilado lleva 0x400 | terreno << 7 | columna << 4 | fila. */
static void caminho_penhasco(const uint8_t *campo, int lado, int comp, uint64_t *rng, int pt[][2], int *npt)
{
	int coluna = (int)abaixo(rng, (size_t)(lado - comp + 1)), fila = 10 + (int)abaixo(rng, (size_t)(lado - 19));
	int n = 1, progresso = 0;
	pt[0][0] = 0; pt[0][1] = 0;
	while (n < comp) {
		int delta = 0;
		if (n > 2) { int d = (int)campo[fila * lado + coluna + 1] - (int)campo[fila * lado + coluna]; delta = d > 0 ? 1 : d < 0 ? -1 : 0; }
		coluna++; progresso++;
		if (delta) { pt[n][0] = progresso - 1; pt[n][1] = pt[n - 1][1] + delta; n++; }
		pt[n][0] = progresso; pt[n][1] = pt[n - 1][1]; n++;
	}
	*npt = comp;
}

static int giro_penhasco(int a[2], int b[2], int c[2], int anterior, int *lado)
{
	int kind;
	if (a[0] == b[0] && b[0] == c[0]) kind = 0;
	else if (a[1] == b[1] && b[1] == c[1]) kind = 1;
	else if (a[0] < c[0]) kind = a[0] == b[0] ? 2 + (a[1] <= c[1]) : 4 + (a[1] > c[1]);
	else kind = a[0] == b[0] ? 4 + (a[1] <= c[1]) : 2 + (a[1] > c[1]);
	int vira = 0;
	switch (anterior) {
	case 0: vira = *lado ? !(kind == 0 || kind == 2 || kind == 3) : !(kind == 0 || kind == 4 || kind == 5); break;
	case 1: vira = !(kind == 1 || kind == 2 || kind == 4); break;
	case 2: vira = kind == 0 || kind == 5; break;
	case 3: vira = kind == 0 || kind == 1 || kind == 4; break;
	case 4: vira = kind == 3; break;
	case 5: vira = kind == 1 || kind == 2; break;
	default: break;
	}
	if (vira) *lado = !*lado;
	return kind;
}

static int remate_penhasco(int a[2], int b[2], int inicio, int anterior, int *lado, int *reverso)
{
	int kind = a[1] == b[1] ? 1 : 0;
	int rev = kind == 1 ? b[0] < a[0] : b[1] < a[1];
	if (!inicio) {
		rev = !rev;
		if ((anterior == 2 && kind == 0 && rev) || anterior == 3 || (anterior == 5 && kind == 1 && rev)) *lado = !*lado;
	}
	*reverso = rev;
	return kind;
}

static const uint8_t GIROS_PENHASCO[12][2] = { { 4, 2 }, { 0, 2 }, { 2, 0 }, { 2, 4 }, { 0, 0 }, { 0, 6 }, { 0, 4 }, { 0, 8 }, { 4, 0 }, { 2, 6 }, { 4, 4 }, { 2, 8 } };
static const uint8_t REMATES_PENHASCO[8][2] = { { 4, 6 }, { 4, 8 }, { 4, 10 }, { 4, 12 }, { 0, 10 }, { 2, 10 }, { 0, 12 }, { 2, 12 } };

typedef struct { int cel[2]; int atlas[2]; } PecaPenhasco;

static int pecas_penhasco(int pt[][2], int npt, int origem[2], int *lado, PecaPenhasco *out)
{
	int anterior = 255, cel[2] = { origem[0], origem[1] }, orient = 0, n = 0;
	for (int i = 0; i < npt - 1; ++i) {
		int cap = i == 0 || i == npt - 2, kind;
		if (cap) kind = remate_penhasco(pt[i], pt[i + 1], i == 0, anterior, lado, &orient);
		else kind = giro_penhasco(pt[i], pt[i + 1], pt[i + 2], anterior, lado);
		anterior = kind;
		const uint8_t *at = cap ? REMATES_PENHASCO[orient + 2 * (*lado + 2 * kind)] : GIROS_PENHASCO[kind * 2 + *lado];
		out[n].cel[0] = cel[0]; out[n].cel[1] = cel[1]; out[n].atlas[0] = at[0]; out[n].atlas[1] = at[1]; n++;
		switch (kind) {
		case 0: cel[1] += orient ? -2 : 2; break;
		case 1: cel[0] += orient ? -2 : 2; break;
		case 2: if (pt[i][0] < pt[i + 2][0]) cel[0] += 2; if (pt[i][1] < pt[i + 2][1]) cel[1] += 2; break;
		case 3: if (pt[i][0] < pt[i + 2][0]) cel[0] += 2; if (pt[i][1] >= pt[i + 2][1]) cel[1] -= 2; break;
		case 4: if (pt[i][0] >= pt[i + 2][0]) cel[0] -= 2; if (pt[i][1] < pt[i + 2][1]) cel[1] += 2; break;
		case 5: if (pt[i][0] >= pt[i + 2][0]) cel[0] -= 2; if (pt[i][1] >= pt[i + 2][1]) cel[1] -= 2; break;
		}
	}
	return n;
}

static void montanhas(uint16_t *cel, int lado, uint64_t semente, const uint8_t params[4], const Forma *formas, int jog)
{
	uint8_t *campo = plasma512(semente, params[2], params[3]);
	if (!campo) return;
	uint8_t *alturas = malloc((size_t)lado * (size_t)lado);
	if (!alturas) { free(campo); return; }
	for (int y = 0; y < lado; ++y) for (int x = 0; x < lado; ++x) alturas[y * lado + x] = amostra(campo, lado, x, y);
	free(campo);
	int terra = 0;
	for (int i = 0; i < lado * lado; ++i) if (((cel[i] >> 7) & 7) != 0 && ((cel[i] >> 4) & 7) != 0) terra++;
	int raiz = 0;
	while ((raiz + 1) * (raiz + 1) <= terra) raiz++;   /* raíz entera: (count as f64).sqrt() as usize */
	int quantas = raiz / 16;
	uint64_t rng = semente ^ 0x434c49464653ull;
	int min = 13 - (512 - lado) * 10 / 512, max = 21 - (512 - lado) * 18 / 512, postas = 0;
	for (int tent = 0; tent < 500 && postas < quantas; ++tent) {
		int origem[2] = { (int)((24 + abaixo(&rng, (size_t)(lado - 58))) & ~1u), (int)((24 + abaixo(&rng, (size_t)(lado - 58))) & ~1u) };
		int comp = min + (int)abaixo(&rng, (size_t)(max - min + 1));
		int pt[64][2], npt;
		caminho_penhasco(alturas, lado, comp, &rng, pt, &npt);
		int face = abaixo(&rng, 2) != 0;
		PecaPenhasco pc[64];
		int np = pecas_penhasco(pt, npt, origem, &face, pc), bom = 1;
		for (int q = 0; q < np && bom; ++q) {
			int x = pc[q].cel[0], y = pc[q].cel[1];
			if (x < 1 || y < 1 || x + 2 >= lado || y + 2 >= lado) { bom = 0; break; }
			for (int yy = y; yy < y + 2 && bom; ++yy)
				for (int xx = x; xx < x + 2; ++xx) {
					uint16_t c = cel[yy * lado + xx];
					if ((c & 0xfc00) != 0 || ((c >> 7) & 7) == 0 || ((c >> 7) & 7) != ((c >> 4) & 7)) { bom = 0; break; }
					for (int p = 0; p < jog; ++p)
						if (xx >= formas[p].x && yy >= formas[p].y && xx < formas[p].x + formas[p].w && yy < formas[p].y + formas[p].h) { bom = 0; break; }
					if (!bom) break;
				}
		}
		if (!bom) continue;
		/* la segunda pasada parte de la cara en que acabó la de comprobar */
		np = pecas_penhasco(pt, npt, origem, &face, pc);
		for (int q = 0; q < np; ++q)
			for (int dy = 0; dy < 2; ++dy)
				for (int dx = 0; dx < 2; ++dx) {
					int i = (pc[q].cel[1] + dy) * lado + pc[q].cel[0] + dx;
					int terreno = (cel[i] >> 7) & 7;
					cel[i] = (uint16_t)(0x400 | (terreno << 7) | ((pc[q].atlas[0] + dx) << 4) | (pc[q].atlas[1] + dy));
				}
		postas++;
	}
	free(alturas);
}

int tz_mapa_aleatorio(TzMapa *out, TzRecursos *r, const TzCatalogo *cat, const TzAleatorio *op)
{
	memset(out, 0, sizeof *out);
	int lado_ok = 0;
	for (int i = 0; i < 7; ++i) if (LADOS[i] == op->lado) lado_ok = 1;
	if (!lado_ok) return tz_falha("tamaño de mapa aleatorio no válido: %d", op->lado);
	if (op->tipo < 0 || op->tipo > 5 || op->recursos_mapa < 0 || op->recursos_mapa > 2 || op->nivel_recursos < 0 || op->nivel_recursos > 2)
		return tz_falha("ajustes del mapa aleatorio no válidos");
	int jog = op->jogadores;
	if (jog < 2 || jog > 8 || op->humano < 0 || op->humano >= jog) return tz_falha("número de reinos o jugador humano no válido");
	if (strcmp(op->bioma, "grass") && strcmp(op->bioma, "summer") && strcmp(op->bioma, "snow") && strcmp(op->bioma, "desert")) return tz_falha("terreno aleatorio no válido");
	int lado = op->lado;
	uint64_t semente = op->semente;
	char nome[100];
	TzIni ini, res;
	memset(&ini, 0, sizeof ini); memset(&res, 0, sizeof res);
	snprintf(nome, sizeof nome, "IMAGES/TERRAIN/%s.TERRAIN.INI", op->bioma);
	if (tz_ini_recurso(r, &ini, nome)) return -1;
	int tem_res = tz_ini_recurso(r, &res, "DATA/RES.INI") == 0;
	int rc = -1;
	Forma formas[8];
	memset(formas, 0, sizeof formas);
	uint8_t *v = NULL, *reservado = NULL;
	uint32_t *marcas = NULL;
	uint8_t eq[8];
	for (int i = 0; i < jog; ++i) eq[i] = op->equipe[i];
	if (formas_inicio(r, lado, jog, semente, eq, formas)) goto fim;
	uint8_t params[4];
	if (op->tipo == 0) {
		const TzIniSecao *s = tz_ini_secao(&ini, "randomterrainset");
		int val[8], nv = 0;
		if (s && s->n) nv = tz_ini_numeros(&ini, "randomterrainset", s->p[0].chave, val, 8);
		if (nv < 4) { tz_falha("faltan los parámetros originales del terreno aleatorio"); goto fim; }
		for (int i = 0; i < 4; ++i) params[i] = (uint8_t)(val[i] < 0 ? 0 : val[i] > 255 ? 255 : val[i]);
	} else memcpy(params, PARAMETROS[op->tipo], 4);
	v = vertices(lado, semente, params, op->tipo == 2);
	if (!v) { tz_falha("sin memoria"); goto fim; }
	int n = lado + 1;
	int vertices_agua = 0;
	for (int y = 0; y < lado; ++y) for (int x = 0; x < lado; ++x) if (v[y * n + x] == 0) vertices_agua++;
	int castelo_w, castelo_h;
	pegada(cat, TZC_CASTLE, 1, 7, &castelo_w, &castelo_h);
	for (int p = 0; p < jog; ++p) {
		Forma *f = &formas[p];
		/* si la forma quedó sin bloque verde para el castillo, se pone uno en el centro (castle_room) */
		uint8_t *verde = malloc((size_t)f->w * (size_t)f->h);
		for (int i = 0; i < f->w * f->h; ++i) verde[i] = f->px[i] == SHAPE_VERDE;
		Pts lst = { 0 };
		esquinas(verde, f->w, f->h, castelo_w + 2, castelo_h + 2, &lst);
		if (!lst.n) {
			int ww = castelo_w + 2 < f->w ? castelo_w + 2 : f->w, hh = castelo_h + 2 < f->h ? castelo_h + 2 : f->h;
			int x0 = (f->w - ww) / 2, y0 = (f->h - hh) / 2;
			for (int y = y0; y < y0 + hh; ++y) for (int x = x0; x < x0 + ww; ++x) f->px[y * f->w + x] = SHAPE_VERDE;
		}
		free(lst.v); free(verde);
		for (int y = 0; y < f->h; ++y) for (int x = 0; x < f->w; ++x) if (f->px[y * f->w + x] != SHAPE_BRANCO) v[(f->y + y) * n + f->x + x] = 1;
	}
	/* las capas del terreno */
	{
		const TzIniSecao *s = tz_ini_secao(&ini, "randomterrainset");
		uint64_t capa_rng = semente ^ 0x4c4159455253ull;
		for (int k = 1; s && k < s->n; ++k) {
			const char *c = s->p[k].chave;
			if (!(strlen(c) == 2 && c[0] >= '0' && c[0] <= '9' && c[1] >= '0' && c[1] <= '9')) continue;
			int val[8];
			if (tz_ini_numeros(&ini, "randomterrainset", c, val, 8) < 4) continue;
			uint8_t *campo = plasma512(capa_rng, val[2] < 1 ? 1 : val[2] > 255 ? 255 : val[2], val[3] < 1 ? 1 : val[3] > 255 ? 255 : val[3]);
			capa_rng++;
			if (!campo) goto fim;
			passo_capa(v, lado, campo, c[0] - '0', c[1] - '0', val[0], val[1]);
			free(campo);
		}
	}
	out->lado = lado;
	out->casillas = malloc((size_t)lado * (size_t)lado * sizeof *out->casillas);
	if (!out->casillas) goto fim;
	for (int y = 0; y < lado; ++y)
		for (int x = 0; x < lado; ++x) {
			uint8_t c4[4] = { v[y * n + x], v[y * n + x + 1], v[(y + 1) * n + x + 1], v[(y + 1) * n + x] };
			out->casillas[y * lado + x] = codificar(c4);
		}
	snprintf(out->bioma, sizeof out->bioma, "%s", op->bioma);
	out->jogadores[0] = out->jogadores[1] = jog;
	reservado = calloc((size_t)lado * (size_t)lado, 1);
	if (!reservado) goto fim;
	for (int p = 0; p < jog; ++p)
		for (int y = 0; y < formas[p].h; ++y)
			for (int x = 0; x < formas[p].w; ++x) {
				uint8_t q = formas[p].px[y * formas[p].w + x];
				reservado[(formas[p].y + y) * lado + formas[p].x + x] = q == SHAPE_VERDE || q == SHAPE_PRETO;
			}
	if (op->penhascos) montanhas(out->casillas, lado, semente, params, formas, jog);
	natureza(out->casillas, lado, &ini, reservado, semente);

	/* los reinos */
	int recursos[4] = { 0, 0, 0, 0 };
	if (tem_res) {
		static const char *const sec[3] = { "low", "medium", "high" };
		static const char *const ch[4] = { "gold", "stone", "food", "wood" };
		for (int k = 0; k < 4; ++k) { int val[4]; if (tz_ini_numeros(&res, sec[op->nivel_recursos], ch[k], val, 4) > 0 && val[0] > 0) recursos[k] = val[0]; }
	}
	uint64_t reino_rng = semente ^ 0x4b494e47444f4dull;
	out->nreinos = 8;
	uint8_t civs[8];
	for (int i = 0; i < 8; ++i) {
		TzReino *k = &out->reinos[i];
		snprintf(k->nome, sizeof k->nome, "Reino %d", i);
		snprintf(k->lider, sizeof k->lider, "Heroe");
		k->ativo = i < jog; k->cor = i + 1;
		/* equipo: 0 = reino solo (equipo propio, sin aliados); el mismo número = aliados (v8.5.5.2) */
		k->equipe = i < jog && op->equipe[i] ? op->equipe[i] : 10 + i;
		k->civ = (int)(1 + abaixo(&reino_rng, 3));
		if (i < jog && op->civ[i] >= 1 && op->civ[i] <= 3) k->civ = op->civ[i];
		civs[i] = (uint8_t)k->civ;
		for (int j = 0; j < 4; ++j) k->recursos[j] = (uint32_t)recursos[j];
		snprintf(k->ia_guiao, sizeof k->ia_guiao, "%s", i < jog ? op->ia[i] : "");   /* D470: el comportamiento elegido */
	}

	/* los objetos, en el orden del original */
	Lista lis = { out, 0, 1, cat };
	uint64_t rng = semente ^ 0x5245534f55524345ull;
	marcas = calloc((size_t)lado * (size_t)lado, sizeof *marcas);
	if (!marcas) goto fim;
	Chao chao = { out->casillas, marcas, lado, 1 };
	{
		const char *w = tz_ini(&ini, "terrain", "water");
		chao.agua = w ? atoi(w) : 1;
	}
	uint8_t *verde[8] = { 0 }, *preto[8] = { 0 };
	for (int p = 0; p < jog; ++p) {
		size_t tam = (size_t)formas[p].w * (size_t)formas[p].h;
		verde[p] = malloc(tam); preto[p] = malloc(tam);
		for (size_t i = 0; i < tam; ++i) { verde[p][i] = formas[p].px[i] == SHAPE_VERDE; preto[p][i] = formas[p].px[i] == SHAPE_PRETO; }
	}
	/* torres de madera: tres en los cuatro brazos de los ejes del castillo (0x4b1dd0) */
	if (op->guardado) {
		int tw, th;
		pegada(cat, TZC_TOWER_STONE + 1, 1, 2, &tw, &th);
		for (int p = 0; p < jog; ++p) {
			const Forma *f = &formas[p];
			int w = f->w, h = f->h, x0 = (w - 10) / 2, y0 = (h - 10) / 2, cx = (w - 1) / 2, cy = (2 * y0 + 9) / 2;
			Pts brazos[4];
			memset(brazos, 0, sizeof brazos);
			for (int y = 0; y < h; ++y)
				for (int x = 0; x < w; ++x) {
					if (!bloco(preto[p], w, h, x, y, tw + 3, th + 3)) continue;
					if (x >= x0 && x <= x0 + 9) pts_add(&brazos[y < cy ? 1 : 3], x, y);
					else if (y >= y0 && y <= y0 + 9) pts_add(&brazos[x < cx ? 0 : 2], x, y);
				}
			int total = brazos[0].n + brazos[1].n + brazos[2].n + brazos[3].n;
			int quer = total < 3 ? total : 3, feitas = 0;
			int brazo = (int)abaixo(&rng, 4);
			while (feitas < quer) {
				Pts *l = &brazos[brazo];
				if (l->n) {
					Pt c = l->v[abaixo(&rng, (size_t)l->n)];
					int ax = c.x + 1, ay = c.y + 1;
					limpar(preto[p], w, l, ax, ay, tw, th);
					tomar(&chao, f->x + ax, f->y + ay, tw, th);
					novo_objeto(&lis, TZC_TOWER_STONE + 1, p + 1, civs[p], f->x + ax, f->y + ay, 60);
					feitas++;
				}
				brazo = (brazo + 1) % 4;
			}
			for (int b = 0; b < 4; ++b) free(brazos[b].v);
		}
	}
	/* oro y piedra: el primer yacimiento de cada reino y, después, los otros (0x4b4950) */
	struct { int classe, x, y; uint32_t cant; } dep[8][16];
	int ndep[8] = { 0 };
	int primeiro = (int[]){ 2300, 3700, 5200 }[op->recursos_mapa];
	int outros = (int[]){ 2, 2, 3 }[op->recursos_mapa], baixo = (int[]){ 2300, 3700, 5200 }[op->recursos_mapa], alto = (int[]){ 3700, 5200, 7500 }[op->recursos_mapa];
	for (int tipo = 0; tipo < 2; ++tipo) {
		int classe = tipo ? TZC_RES_STONE : TZC_RES_GOLD, mw, mh;
		pegada(cat, classe, 0, 3, &mw, &mh);
		for (int p = 0; p < jog; ++p) {
			const Forma *f = &formas[p];
			Pts lst = { 0 };
			esquinas(preto[p], f->w, f->h, mw + 2, mh + 2, &lst);
			if (lst.n) {
				Pt c = lst.v[abaixo(&rng, (size_t)lst.n)];
				limpar(preto[p], f->w, &lst, c.x, c.y, mw + 2, mh + 2);
				int ax = f->x + c.x + 1, ay = f->y + c.y + 1;
				tomar(&chao, ax, ay, mw, mh);
				dep[p][ndep[p]].classe = classe; dep[p][ndep[p]].x = ax; dep[p][ndep[p]].y = ay; dep[p][ndep[p]].cant = (uint32_t)primeiro; ndep[p]++;
			}
			free(lst.v);
		}
		for (int p = 0; p < jog; ++p) {
			const Forma *f = &formas[p];
			for (int k = 0; k < outros; ++k) {
				int achou = 0, ax = 0, ay = 0;
				for (int t = 0; t < 40 && !achou; ++t) {
					int x1 = f->sector.x1 + 1 > mw + 2 ? f->sector.x1 + 1 - (mw + 2) : 0, y1 = f->sector.y1 + 1 > mh + 2 ? f->sector.y1 + 1 - (mh + 2) : 0;
					int x = (int)entre(&rng, (size_t)f->sector.x0, (size_t)x1), y = (int)entre(&rng, (size_t)f->sector.y0, (size_t)y1);
					int perto = x + mw >= f->x && x < f->x + f->w && y + mh >= f->y && y < f->y + f->h;
					if (perto || x + mw > lado || y + mh > lado) continue;
					int ok = 1;
					for (int yy = y; yy < y + mh && ok; ++yy) for (int xx = x; xx < x + mw; ++xx) { uint32_t w = palavra(&chao, xx, yy); if (!terra(&chao, w) || (w & 0x101fc00u) != 0) { ok = 0; break; } }
					if (ok) { achou = 1; ax = x; ay = y; }
				}
				if (!achou || ndep[p] >= 16) break;
				tomar(&chao, ax, ay, mw, mh);
				dep[p][ndep[p]].classe = classe; dep[p][ndep[p]].x = ax; dep[p][ndep[p]].y = ay;
				dep[p][ndep[p]].cant = (uint32_t)((int)entre(&rng, (size_t)baixo, (size_t)alto) / 100 * 100); ndep[p]++;
			}
		}
	}
	/* el castillo en una esquina al azar de los bloques verdes (0x4b21d0) */
	for (int p = 0; p < jog; ++p) {
		const Forma *f = &formas[p];
		Pts lst = { 0 };
		esquinas(verde[p], f->w, f->h, castelo_w + 2, castelo_h + 2, &lst);
		if (!lst.n) { free(lst.v); tz_falha("no hay sitio para el castillo"); goto fim2; }
		Pt c = lst.v[abaixo(&rng, (size_t)lst.n)];
		limpar(verde[p], f->w, &lst, c.x + 1, c.y + 1, castelo_w, castelo_h);
		free(lst.v);
		int ax = f->x + c.x + 1, ay = f->y + c.y + 1;
		tomar(&chao, ax, ay, castelo_w, castelo_h);
		novo_objeto(&lis, TZC_CASTLE, p + 1, civs[p], ax, ay, 60);
	}
	/* los yacimientos (el oro con la mitad más) */
	for (int p = 0; p < jog; ++p)
		for (int k = 0; k < ndep[p]; ++k) {
			TzObjeto *o = novo_objeto(&lis, dep[p][k].classe, 0, 0, dep[p][k].x, dep[p][k].y, 64);
			if (o) poner_cantidad(o, dep[p][k].classe == TZC_RES_GOLD ? dep[p][k].cant * 150 / 100 : dep[p][k].cant);
		}
	/* tres aldeanos en celdas verdes libres */
	for (int p = 0; p < jog; ++p) {
		const Forma *f = &formas[p];
		Pts lst = { 0 };
		esquinas(verde[p], f->w, f->h, 1, 1, &lst);
		for (int k = 0; k < 3 && lst.n; ++k) {
			Pt c = lst.v[abaixo(&rng, (size_t)lst.n)];
			limpar(verde[p], f->w, &lst, c.x, c.y, 1, 1);
			tomar(&chao, f->x + c.x, f->y + c.y, 1, 1);
			novo_objeto(&lis, TZC_PEON, p + 1, civs[p], f->x + c.x, f->y + c.y, 60);
		}
		free(lst.v);
	}
	/* treinta árboles en bloques negros de 3x3 */
	{
		Arvores arv;
		arvores_ler(&ini, &arv);
		for (int p = 0; p < jog; ++p) {
			const Forma *f = &formas[p];
			Pts lst = { 0 };
			esquinas(preto[p], f->w, f->h, 3, 3, &lst);
			int plantadas = 0;
			while (plantadas < 30 && lst.n) {
				int idx = (int)abaixo(&rng, (size_t)lst.n);
				Pt c = lst.v[idx];
				int cx = f->x + c.x + 1, cy = f->y + c.y + 1;
				uint32_t w = palavra(&chao, cx, cy);
				int nn, terr;
				terreno_arvore(&arv, (uint16_t)w, &nn, &terr);
				if (!terra(&chao, w) || (w & 0x108fc00u) != 0 || nn == 0) { lst.v[idx] = lst.v[--lst.n]; continue; }
				limpar(preto[p], f->w, &lst, c.x, c.y, 3, 3);
				out->casillas[cy * lado + cx] |= bits_arvore(&arv, terr, abaixo(&rng, (size_t)nn + 1));
				plantadas++;
			}
			free(lst.v);
		}
		arvores_liberar(&arv);
	}
	/* peces: entre 1/361 y 4/361 del agua, a cuatro casillas de los bordes, en agua libre de 4x4 */
	{
		int peces = (int)entre(&rng, (size_t)(vertices_agua / 361), (size_t)(vertices_agua * 4 / 361));
		Pt sitios[512];
		int w, h;
		pegada(cat, TZC_FISHPATCH, 0, 4, &w, &h);
		int ns = espalhar(&chao, &rng, TZC_FISHPATCH, 2, w, h, peces < 512 ? peces : 512, 4, sitios, 512);
		for (int i = 0; i < ns; ++i) { TzObjeto *o = novo_objeto(&lis, TZC_FISHPATCH, 0, 0, sitios[i].x, sitios[i].y, 60); if (o) poner_cantidad(o, 1000); }
	}
	/* 0x4b54e9: puertas mágicas, la mitad que reinos, con el ancho de su dibujo más dos de margen */
	if (op->portas) {
		Pt sitios[8];
		int w, h;
		pegada(cat, 72, 0, 3, &w, &h);
		int ns = espalhar(&chao, &rng, 72, 1, w, h, jog / 2, w + 2, sitios, 8);
		for (int i = 0; i < ns; ++i) novo_objeto(&lis, 72, 10, 0, sitios[i].x, sitios[i].y, 60);
	}
	/* 0x4b5b14: objetos al azar, R(1,10) + lado/32 de cada clase (equipo, libros, pociones), cada uno con sus cifras */
	if (op->itens) {
		int itens = (int)entre(&rng, 1, 10) + lado / 32;
		static const int clases_item[3] = { TZC_ITEM_MAIS, TZC_LIVRO, TZC_POCAO };
		for (int k = 0; k < 3; ++k) {
			Pt sitios[64];
			int w, h;
			pegada(cat, clases_item[k], 0, 1, &w, &h);
			int ns = espalhar(&chao, &rng, clases_item[k], 1, w, h, itens < 64 ? itens : 64, 1, sitios, 64);
			for (int i = 0; i < ns; ++i) {
				TzObjeto *o = novo_objeto(&lis, clases_item[k], 0, 0, sitios[i].x, sitios[i].y, 106);
				if (o) preencher_item(o, &rng);
			}
		}
	}
	/* animales: osos, pájaros y águilas (el número del original: R(1,5) + lado/64) */
	if (op->animais) {
		int animais = (int)entre(&rng, 1, 5) + lado / 64;
		static const struct { int classe, camada; } clases[] = { { 26, 1 }, { TZC_COW, 1 }, { 112, 0 }, { 119, 0 } };   /* oso, vaca, pájaro, águila */
		for (unsigned k = 0; k < sizeof clases / sizeof *clases; ++k) {
			Pt sitios[256];
			int w, h;
			pegada(cat, clases[k].classe, 0, 1, &w, &h);
			int ns = espalhar(&chao, &rng, clases[k].classe, clases[k].camada, w, h, animais < 256 ? animais : 256, 1, sitios, 256);
			for (int i = 0; i < ns; ++i) novo_objeto(&lis, clases[k].classe, 10, 0, sitios[i].x, sitios[i].y, 60);
		}
	}
	rc = 0;
fim2:
	for (int p = 0; p < jog; ++p) { free(verde[p]); free(preto[p]); }
fim:
	free(v); free(reservado); free(marcas);
	liberar_formas(formas, jog);
	tz_ini_liberar(&ini);
	if (tem_res) tz_ini_liberar(&res);
	if (rc) {
		char motivo[256];
		snprintf(motivo, sizeof motivo, "%s", tz_erro());
		tz_mapa_liberar(out);
		tz_falha("%s", motivo);
	}
	return rc;
}
