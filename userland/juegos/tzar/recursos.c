/* SPDX-License-Identifier: GPL-3.0-or-later */
/* recursos.c - los paquetes de una instalación de Tzar y los ficheros INI (D417).
 *
 * Traducido de ReTzar, src/assets.rs (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): orden de
 * los paquetes, carpetas sueltas y lector de INI. Sin los idiomas superpuestos de ReTzar: la edición FX
 * ya está en castellano y los textos se usan tal como vienen (Windows-1252, que para el castellano
 * coincide con el Latin-1 de SaguiOS).
 */
#include "tzar.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

/* Los paquetes, en el orden en que los lee el juego: los de después sustituyen a los de antes. */
static const char *const PACOTES[] = { "sounds.wdt", "data.wdt", "data2.wdt", "images.wdt", "images2.wdt", "terrains.wdt", "local.wdt", "help.wdt", "v101.wdt" };
static const char *const SOLTAS[] = { "DATA", "HELP", "IMAGES", "LOCAL" };

/* ------------------------------------------------------------------ INI */

static uint32_t hash_par(const char *s, const char *k)
{
	uint32_t h = tz_hash(s);
	h ^= 1; h *= 16777619u;
	while (*k) { h ^= (unsigned char)*k++; h *= 16777619u; }
	return h;
}

static void minusculas(char *s) { for (; *s; ++s) if ((unsigned char)*s < 128) *s = (char)tolower((unsigned char)*s); }

static int secao_idx(const TzIni *ini, const char *nome)
{
	if (!ini->tseccap) return -1;
	uint32_t h = tz_hash(nome);
	for (uint32_t i = h & (ini->tseccap - 1);; i = (i + 1) & (ini->tseccap - 1)) {
		uint32_t v = ini->tsec[i];
		if (!v) return -1;
		if (!strcmp(ini->s[v - 1].nome, nome)) return (int)v - 1;
	}
}

static void rehash(TzIni *ini)
{
	/* tabla de pares: tamaño potencia de 2 con carga < 1/2 */
	uint32_t total = 0;
	for (int i = 0; i < ini->ns; ++i) total += (uint32_t)ini->s[i].n;
	if (ini->tabcap < (total + 1) * 2) {
		uint32_t cap = 64;
		while (cap < (total + 1) * 4) cap <<= 1;
		uint32_t *t = calloc(cap, sizeof *t);
		if (!t) return;
		free(ini->tab);
		ini->tab = t; ini->tabcap = cap;
		for (int si = 0; si < ini->ns; ++si)
			for (int pi = 0; pi < ini->s[si].n; ++pi) {
				uint32_t h = hash_par(ini->s[si].nome, ini->s[si].p[pi].chave);
				uint32_t j = h & (cap - 1);
				while (t[j]) j = (j + 1) & (cap - 1);
				t[j] = ((uint32_t)si << 16 | (uint32_t)pi) + 1;
			}
	}
}

static int par_idx(const TzIni *ini, const char *s, const char *k, int *si, int *pi)
{
	if (!ini->tabcap) return 0;
	uint32_t h = hash_par(s, k);
	for (uint32_t i = h & (ini->tabcap - 1);; i = (i + 1) & (ini->tabcap - 1)) {
		uint32_t v = ini->tab[i];
		if (!v) return 0;
		v--;
		int a = (int)(v >> 16), b = (int)(v & 0xffff);
		if (!strcmp(ini->s[a].nome, s) && !strcmp(ini->s[a].p[b].chave, k)) { *si = a; *pi = b; return 1; }
	}
}

static void definir(TzIni *ini, const char *secao, const char *chave, const char *valor)
{
	int si = secao_idx(ini, secao);
	if (si < 0) {
		if (ini->ns == ini->caps) {
			int nc = ini->caps ? ini->caps * 2 : 64;
			TzIniSecao *ns = realloc(ini->s, (size_t)nc * sizeof *ns);
			if (!ns) return;
			ini->s = ns; ini->caps = nc;
		}
		if (ini->ns >= 65535) return;
		si = ini->ns++;
		memset(&ini->s[si], 0, sizeof ini->s[si]);
		ini->s[si].nome = tz_strdup(secao);
		if (!ini->s[si].nome) { ini->ns--; return; }
		if ((uint32_t)ini->ns * 2 >= ini->tseccap) {
			uint32_t cap = ini->tseccap ? ini->tseccap * 2 : 128;
			while ((uint32_t)ini->ns * 2 >= cap) cap <<= 1;
			uint32_t *t = calloc(cap, sizeof *t);
			if (!t) return;
			free(ini->tsec);
			ini->tsec = t; ini->tseccap = cap;
			for (int i = 0; i < ini->ns; ++i) {
				uint32_t j = tz_hash(ini->s[i].nome) & (cap - 1);
				while (t[j]) j = (j + 1) & (cap - 1);
				t[j] = (uint32_t)i + 1;
			}
		} else {
			uint32_t j = tz_hash(secao) & (ini->tseccap - 1);
			while (ini->tsec[j]) j = (j + 1) & (ini->tseccap - 1);
			ini->tsec[j] = (uint32_t)si + 1;
		}
	}
	int a, b;
	if (par_idx(ini, secao, chave, &a, &b)) {
		char *v = tz_strdup(valor);
		if (v) { free(ini->s[a].p[b].valor); ini->s[a].p[b].valor = v; }
		return;
	}
	TzIniSecao *s = &ini->s[si];
	if (s->n >= 65535) return;
	if (s->n == s->cap) {
		int nc = s->cap ? s->cap * 2 : 8;
		TzIniPar *np = realloc(s->p, (size_t)nc * sizeof *np);
		if (!np) return;
		s->p = np; s->cap = nc;
	}
	s->p[s->n].chave = tz_strdup(chave);
	s->p[s->n].valor = tz_strdup(valor);
	if (!s->p[s->n].chave || !s->p[s->n].valor) { free(s->p[s->n].chave); free(s->p[s->n].valor); return; }
	s->n++;
	uint32_t total = 0;
	for (int i = 0; i < ini->ns; ++i) total += (uint32_t)ini->s[i].n;
	if (total * 2 >= ini->tabcap) rehash(ini);
	else {
		uint32_t j = hash_par(secao, chave) & (ini->tabcap - 1);
		while (ini->tab[j]) j = (j + 1) & (ini->tabcap - 1);
		ini->tab[j] = ((uint32_t)si << 16 | (uint32_t)(s->n - 1)) + 1;
	}
}

static char *recortar(char *s)
{
	while (*s == ' ' || *s == '\t') ++s;
	size_t n = strlen(s);
	while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) s[--n] = 0;
	return s;
}

/* Secciones y claves sin distinguir mayúsculas (se guardan en minúsculas); comentarios con ';' o '#';
 * una línea sin '=' es una clave con el valor vacío; una clave repetida se queda con el último valor. */
void tz_ini_ler(TzIni *ini, const uint8_t *dados, uint32_t n)
{
	char secao[256] = "";
	uint32_t i = 0;
	while (i < n) {
		uint32_t f = i;
		while (f < n && dados[f] != '\n') ++f;
		uint32_t len = f - i < 4095 ? f - i : 4095;
		char linha[4096];
		memcpy(linha, dados + i, len);
		linha[len] = 0;
		for (uint32_t k = 0; k < len; ++k) if (!linha[k]) linha[k] = ' ';
		i = f + 1;
		char *l = recortar(linha);
		if (!*l || *l == ';' || *l == '#') continue;
		size_t ll = strlen(l);
		if (l[0] == '[' && l[ll - 1] == ']') {
			l[ll - 1] = 0;
			snprintf(secao, sizeof secao, "%s", recortar(l + 1));
			minusculas(secao);
			continue;
		}
		char *eq = strchr(l, '=');
		char chave[256];
		const char *valor = "";
		if (eq) { *eq = 0; valor = recortar(eq + 1); }
		snprintf(chave, sizeof chave, "%s", recortar(l));
		minusculas(chave);
		definir(ini, secao, chave, valor);
	}
}

const TzIniSecao *tz_ini_secao(const TzIni *ini, const char *secao)
{
	char s[256];
	snprintf(s, sizeof s, "%s", secao);
	minusculas(s);
	int i = secao_idx(ini, s);
	return i < 0 ? NULL : &ini->s[i];
}

const char *tz_ini(const TzIni *ini, const char *secao, const char *chave)
{
	char s[256], k[256];
	snprintf(s, sizeof s, "%s", secao);
	snprintf(k, sizeof k, "%s", chave);
	minusculas(s);
	minusculas(k);
	int a, b;
	return par_idx(ini, s, k, &a, &b) ? ini->s[a].p[b].valor : NULL;
}

/* Enteros separados por comas; lo que no es un entero se salta (como ReTzar). */
int tz_ini_numeros(const TzIni *ini, const char *secao, const char *chave, int *v, int max)
{
	const char *t = tz_ini(ini, secao, chave);
	int n = 0;
	if (!t) return 0;
	while (*t && n < max) {
		const char *f = strchr(t, ',');
		size_t len = f ? (size_t)(f - t) : strlen(t);
		char b[32];
		if (len < sizeof b) {
			memcpy(b, t, len);
			b[len] = 0;
			char *x = recortar(b), *fim;
			long l = strtol(x, &fim, 10);
			if (*x && !*fim && l >= -2147483647L && l <= 2147483647L) v[n++] = (int)l;
		}
		if (!f) break;
		t = f + 1;
	}
	return n;
}

void tz_ini_liberar(TzIni *ini)
{
	for (int i = 0; i < ini->ns; ++i) {
		for (int j = 0; j < ini->s[i].n; ++j) { free(ini->s[i].p[j].chave); free(ini->s[i].p[j].valor); }
		free(ini->s[i].p);
		free(ini->s[i].nome);
	}
	free(ini->s);
	free(ini->tab);
	free(ini->tsec);
	memset(ini, 0, sizeof *ini);
}

/* ------------------------------------------------------------------ recursos */

static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static uint32_t procurar(const TzRecursos *r, const char *chave)
{
	if (!r->idxcap) return 0;
	for (uint32_t i = tz_hash(chave) & (r->idxcap - 1);; i = (i + 1) & (r->idxcap - 1)) {
		uint32_t v = r->idx[i];
		if (!v) return 0;
		const TzPacote *p = &r->pac[(v - 1) >> 24];
		char k[512];
		tz_chave(k, p->e[(v - 1) & 0xffffff].nome, sizeof k);
		if (!strcmp(k, chave)) return v;
	}
}

static int indexar(TzRecursos *r)
{
	uint32_t total = 0;
	for (int p = 0; p < r->npac; ++p) total += (uint32_t)r->pac[p].n;
	uint32_t cap = 1024;
	while (cap < total * 2 + 2) cap <<= 1;
	r->idx = calloc(cap, sizeof *r->idx);
	r->nomes = tz_malloc((total ? total : 1) * sizeof *r->nomes);
	if (!r->idx || !r->nomes) return tz_falha("sin memoria para el índice");
	r->idxcap = cap;
	for (int p = 0; p < r->npac; ++p)
		for (int e = 0; e < r->pac[p].n && e < 0xffffff; ++e) {
			char k[512];
			tz_chave(k, r->pac[p].e[e].nome, sizeof k);
			uint32_t val = ((uint32_t)p << 24 | (uint32_t)e) + 1;
			uint32_t i = tz_hash(k) & (cap - 1);
			for (;; i = (i + 1) & (cap - 1)) {
				uint32_t v = r->idx[i];
				if (!v) { r->idx[i] = val; r->nomes[r->nnomes] = tz_strdup(k); if (r->nomes[r->nnomes]) r->nnomes++; break; }
				const TzPacote *q = &r->pac[(v - 1) >> 24];
				char k2[512];
				tz_chave(k2, q->e[(v - 1) & 0xffffff].nome, sizeof k2);
				if (!strcmp(k, k2)) {
					/* dentro de un paquete gana la primera; un paquete posterior sustituye */
					if ((int)((v - 1) >> 24) != p) r->idx[i] = val;
					break;
				}
			}
		}
	qsort(r->nomes, (size_t)r->nnomes, sizeof *r->nomes, cmp_str);
	return 0;
}

int tz_recursos_abrir(TzRecursos *r, const char *raiz)
{
	memset(r, 0, sizeof *r);
	snprintf(r->raiz, sizeof r->raiz, "%s", raiz);
	/* las carpetas sueltas solo completan lo que falta en los paquetes */
	if (!tz_pacote_pasta(&r->pac[0], raiz, SOLTAS, 4) && r->pac[0].n > 0) r->npac = 1;
	else tz_pacote_fechar(&r->pac[0]);
	char packs[600];
	snprintf(packs, sizeof packs, "%s/Packs", raiz);
	DIR *d = opendir(packs);
	if (!d) {
		/* la carpeta puede llamarse PACKS o packs */
		DIR *rd = opendir(raiz);
		struct dirent *de;
		while (rd && (de = readdir(rd)))
			if (!strcasecmp(de->d_name, "Packs")) { snprintf(packs, sizeof packs, "%s/%s", raiz, de->d_name); break; }
		if (rd) closedir(rd);
		d = opendir(packs);
	}
	char nomes[16][256];
	int nn = 0;
	struct dirent *de;
	while (d && (de = readdir(d)) && nn < 16) snprintf(nomes[nn++], sizeof nomes[0], "%s", de->d_name);
	if (d) closedir(d);
	for (unsigned i = 0; i < sizeof PACOTES / sizeof *PACOTES; ++i)
		for (int k = 0; k < nn; ++k)
			if (!strcasecmp(nomes[k], PACOTES[i])) {
				char ruta[900];
				snprintf(ruta, sizeof ruta, "%s/%s", packs, nomes[k]);
				if (tz_pacote_abrir(&r->pac[r->npac], ruta)) { tz_recursos_fechar(r); return -1; }
				r->npac++;
				break;
			}
	if (!r->npac) return tz_falha("no hay paquetes del juego (Packs/*.wdt) ni carpetas DATA/IMAGES en %s", raiz);
	if (indexar(r)) { tz_recursos_fechar(r); return -1; }
	return 0;
}

int tz_recursos_existe(const TzRecursos *r, const char *nome)
{
	char k[512];
	tz_chave(k, nome, sizeof k);
	return procurar(r, k) != 0;
}

uint8_t *tz_recursos_ler(TzRecursos *r, const char *nome, uint32_t *tam)
{
	char k[512];
	tz_chave(k, nome, sizeof k);
	uint32_t v = procurar(r, k);
	if (!v) { tz_falha("falta el recurso %s", nome); return NULL; }
	TzPacote *p = &r->pac[(v - 1) >> 24];
	uint8_t *d = tz_pacote_ler(p, &p->e[(v - 1) & 0xffffff], tam);
	if (!d) {
		char m[256];
		snprintf(m, sizeof m, "%s", tz_erro());
		tz_falha("%s: %s", nome, m);
	}
	return d;
}

int tz_ini_recurso(TzRecursos *r, TzIni *ini, const char *nome)
{
	uint32_t n;
	uint8_t *d = tz_recursos_ler(r, nome, &n);
	if (!d) return -1;
	tz_ini_ler(ini, d, n);
	free(d);
	return 0;
}

void tz_recursos_fechar(TzRecursos *r)
{
	for (int i = 0; i < 10; ++i) tz_pacote_fechar(&r->pac[i]);
	for (int i = 0; i < r->nnomes; ++i) free(r->nomes[i]);
	free(r->nomes);
	free(r->idx);
	memset(r, 0, sizeof *r);
}
