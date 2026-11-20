/* SPDX-License-Identifier: GPL-3.0-or-later */
/* campanha.c - catálogo de campañas de Tzar, orden de la historia, progreso y paso entre misiones (D417, fase 7).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): src/scenario.rs (carga del catálogo, orden de
 * las misiones, búsqueda de la misión a la que lleva «cargar mapa») y frontend/menu_ui.rs (next_map, CarryOver). El progreso en
 * disco es propio (texto plano; ReTzar lo cifra). Las campañas son contenedores HPFS con sus mapas anidados («MAPS/x.wmp») y
 * «MAPS/maps.ini» con la lista de misiones y cuál está disponible de entrada. */
#include "campanha.h"
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void minusc(char *s) { for (; *s; ++s) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s + 32); }

static void clave_mapa(const char *src, char *dst, size_t n)
{
	size_t i = 0;
	for (; src[i] && i + 1 < n; ++i) dst[i] = src[i] == '\\' ? '/' : (src[i] >= 'A' && src[i] <= 'Z') ? (char)(src[i] + 32) : src[i];
	dst[i] = 0;
}

static char *texto_de(TzHpfs *c, const char *nome)
{
	uint32_t n;
	if (!tz_hpfs_entrada(c, nome)) return tz_strdup("");
	uint8_t *d = tz_hpfs_ler(c, nome, &n);
	if (!d) return tz_strdup("");
	char *s = tz_malloc((size_t)n + 1);
	if (!s) { free(d); return NULL; }
	uint32_t k = 0;
	for (uint32_t i = 0; i < n && d[i]; ++i) s[k++] = (char)d[i];
	s[k] = 0;
	while (k && (unsigned char)s[k - 1] <= 0x20) s[--k] = 0;
	char *a = s;
	while (*a && (unsigned char)*a <= 0x20) a++;
	if (a != s) memmove(s, a, strlen(a) + 1);
	free(d);
	return s;
}

/* una misión a partir del contenedor de su mapa (el nested, o el .wmp suelto) */
static int misiao_de(TzHpfs *c, const char *fich, const char *dentro, int inicial, TzMissao *out)
{
	memset(out, 0, sizeof *out);
	const TzEntrada *h = NULL;
	for (int i = 0; i < c->n && !h; ++i) {
		size_t l = strlen(c->e[i].nome);
		if (l > 7 && !strcasecmp(c->e[i].nome + l - 7, ".header")) h = &c->e[i];
	}
	if (!h) return tz_falha("el mapa no tiene cabecera");
	char base[400];
	snprintf(base, sizeof base, "%.*s", (int)(strlen(h->nome) - 7), h->nome);
	uint32_t nh;
	uint8_t *cab = tz_hpfs_ler(c, h->nome, &nh);
	if (!cab) return -1;
	if (nh < 0x116 || memcmp(cab, "2mzt", 4)) { free(cab); return tz_falha("cabecera de escenario no admitida"); }
	out->lado = tz_u16(cab + 6);
	out->jogadores[0] = cab[0x4c]; out->jogadores[1] = cab[0x4d];
	out->tipo = nh > 0x138 ? cab[0x138] : 0;
	{
		size_t k = 0;
		for (; k < 64 && cab[0x56 + k]; ++k) out->titulo[k] = (char)cab[0x56 + k];
		out->titulo[k] = 0;
		while (k && (unsigned char)out->titulo[k - 1] <= 0x20) out->titulo[--k] = 0;
		char *a = out->titulo;
		while (*a == ' ') a++;
		if (a != out->titulo) memmove(out->titulo, a, strlen(a) + 1);
		if (!out->titulo[0]) { const char *b = strrchr(base, '/'); snprintf(out->titulo, sizeof out->titulo, "%.99s", b ? b + 1 : base); }
	}
	free(cab);
	char nome[420];
	snprintf(nome, sizeof nome, "%s.description", base);
	out->descricao = texto_de(c, nome);
	snprintf(out->fich, sizeof out->fich, "%s", fich);
	snprintf(out->dentro, sizeof out->dentro, "%s", dentro);
	const char *b = strrchr(base, '/');
	char fn[300];
	const char *barra = strrchr(fich, '/');
	snprintf(fn, sizeof fn, "%s", barra ? barra + 1 : fich);
	snprintf(out->id, sizeof out->id, "%s::%s", fn, b ? b + 1 : base);
	minusc(out->id);
	out->inicial = inicial;
	return 0;
}

static void missao_liberar(TzMissao *m) { free(m->descricao); m->descricao = NULL; }

static int cmp_str(const void *a, const void *b) { return strcasecmp(*(char *const *)a, *(char *const *)b); }

/* ---------------------------------------------------------------- orden de la historia */
typedef struct { char **alvo; int n; } Seguintes;

static int numero_interno(const TzMissao *m, uint32_t *n, char *interno, size_t ni)
{
	const char *b = strrchr(m->dentro, '/');
	snprintf(interno, ni, "%s", b ? b + 1 : m->dentro);
	*n = 0;
	for (const char *p = interno; *p >= '0' && *p <= '9'; ++p) *n = *n * 10 + (uint32_t)(*p - '0');
	return interno[0] >= '0' && interno[0] <= '9';
}

typedef struct { uint32_t n; char interno[200]; TzMissao m; } Ord;
static int cmp_ord(const void *a, const void *b)
{
	const Ord *x = a, *y = b;
	if (x->n != y->n) return x->n < y->n ? -1 : 1;
	return strcmp(x->interno, y->interno);
}

/* ¿la misión anidada `nested` es la que nombra `ruta` salvo un sufijo de idioma «_xx»? */
static int mesma_menos_idioma(const char *nested, const char *ruta)
{
	char n[300], r[300];
	clave_mapa(nested, n, sizeof n);
	clave_mapa(ruta, r, sizeof r);
	char *ponto = strrchr(r, '.');
	if (!ponto) return 0;
	size_t stem = (size_t)(ponto - r);
	const char *ext = ponto + 1;
	char *np = strrchr(n, '.');
	if (!np || strcmp(np + 1, ext)) return 0;
	size_t nl = (size_t)(np - n);
	if (nl != stem + 3) return 0;
	if (strncmp(n, r, stem) || n[stem] != '_') return 0;
	return ((n[stem + 1] | 32) >= 'a' && (n[stem + 1] | 32) <= 'z') && ((n[stem + 2] | 32) >= 'a' && (n[stem + 2] | 32) <= 'z');
}

int tz_campanha_achar(const TzCampanha *c, const char *ruta)
{
	char quer[300];
	clave_mapa(ruta, quer, sizeof quer);
	for (int i = 0; i < c->nmissoes; ++i) {
		char n[300];
		clave_mapa(c->missoes[i].dentro, n, sizeof n);
		if (!strcmp(n, quer)) return i;
	}
	for (int i = 0; i < c->nmissoes; ++i) if (mesma_menos_idioma(c->missoes[i].dentro, ruta)) return i;
	return -1;
}

/* el orden de una campaña cuyos nombres no llevan número: las que abre Maps.ini y después las que alcanzan sus «cargar mapa»,
 * a medida que se encuentran; las demás, al final en el orden de Maps.ini */
static void orden_historia(TzCampanha *c, Seguintes *sg)
{
	int n = c->nmissoes;
	if (n <= 0) return;
	int *ord = malloc((size_t)n * sizeof *ord);
	int no = 0;
	if (!ord) return;
	for (int i = 0; i < n; ++i) if (c->missoes[i].inicial) ord[no++] = i;
	for (int k = 0; k < no; ++k)
		for (int t = 0; t < sg[ord[k]].n; ++t) {
			int j = tz_campanha_achar(c, sg[ord[k]].alvo[t]);
			int ya = 0;
			for (int q = 0; q < no; ++q) if (ord[q] == j) ya = 1;
			if (j >= 0 && !ya) ord[no++] = j;
		}
	for (int i = 0; i < n; ++i) {
		int ya = 0;
		for (int q = 0; q < no; ++q) if (ord[q] == i) ya = 1;
		if (!ya) ord[no++] = i;
	}
	TzMissao *nv = malloc((size_t)n * sizeof *nv);
	if (nv) {
		for (int i = 0; i < n; ++i) nv[i] = c->missoes[ord[i]];
		memcpy(c->missoes, nv, (size_t)n * sizeof *nv);
		free(nv);
	}
	free(ord);
}

static int campanha_ler(const char *raiz, const char *fich, TzCampanha *out)
{
	char abs_[900];
	snprintf(abs_, sizeof abs_, "%s/%s", raiz, fich);
	TzHpfs c;
	if (tz_hpfs_abrir(&c, abs_)) return -1;
	memset(out, 0, sizeof *out);
	snprintf(out->ficheiro, sizeof out->ficheiro, "%s", fich);
	out->descricao = texto_de(&c, "MAPS/campaign description.ini");
	{
		const char *nl = strpbrk(out->descricao, "\r\n");
		size_t l = nl ? (size_t)(nl - out->descricao) : strlen(out->descricao);
		if (l > sizeof out->titulo - 1) l = sizeof out->titulo - 1;
		memcpy(out->titulo, out->descricao, l);
		out->titulo[l] = 0;
		while (l && (unsigned char)out->titulo[l - 1] <= 0x20) out->titulo[--l] = 0;
	}
	uint32_t nm;
	uint8_t *lista = tz_hpfs_entrada(&c, "MAPS/maps.ini") ? tz_hpfs_ler(&c, "MAPS/maps.ini", &nm) : NULL;
	if (!lista) { tz_hpfs_fechar(&c); free(out->descricao); return tz_falha("%s: no tiene MAPS/maps.ini", fich); }
	TzIni ini;
	memset(&ini, 0, sizeof ini);
	tz_ini_ler(&ini, lista, nm);
	free(lista);
	const TzIniSecao *sec = tz_ini_secao(&ini, "maps");
	int nsec = sec ? sec->n : 0;
	int rc = 0;
	Seguintes *sg = NULL;
	if (sec) {
		out->missoes = calloc((size_t)sec->n + 1, sizeof *out->missoes);
		sg = calloc((size_t)sec->n + 1, sizeof *sg);
		if (!out->missoes || !sg) rc = -1;
		for (int i = 0; i < sec->n && !rc; ++i) {
			char nome[300];
			snprintf(nome, sizeof nome, "MAPS/%s.wmp", sec->p[i].chave);
			const TzEntrada *e = tz_hpfs_entrada(&c, nome);
			if (!e) { rc = tz_falha("%s: falta la misión %s", fich, sec->p[i].chave); break; }
			TzHpfs sub;
			if (tz_hpfs_anidado(&c, e->nome, &sub)) { rc = -1; break; }
			TzMissao *mi = &out->missoes[out->nmissoes];
			int r = misiao_de(&sub, fich, e->nome, !strcasecmp(sec->p[i].valor, "yes"), mi);
			if (!r) {
				TzGuiao g;
				tz_guiao_ler(&g, &sub);
				for (int a = 0; a < g.nacoes; ++a)
					if (g.acoes[a].tipo == TZ_ACC_MAPA && g.acoes[a].t[0]) {
						char **nv = realloc(sg[out->nmissoes].alvo, (size_t)(sg[out->nmissoes].n + 1) * sizeof *nv);
						if (nv) { sg[out->nmissoes].alvo = nv; nv[sg[out->nmissoes].n++] = tz_strdup(g.acoes[a].t[0]); }
					}
				tz_guiao_liberar(&g);
				out->nmissoes++;
			} else rc = r;
			tz_hpfs_fechar(&sub);
		}
	}
	tz_ini_liberar(&ini);
	if (!rc && out->nmissoes) {
		/* los nombres numerados («01 ...») dan el orden de la historia; si no, el que sale de Maps.ini y los enlaces */
		int numeradas = 0;
		for (int i = 0; i < out->nmissoes; ++i) { uint32_t n; char in[200]; if (numero_interno(&out->missoes[i], &n, in, sizeof in)) numeradas = 1; }
		if (numeradas) {
			Ord *o = malloc((size_t)out->nmissoes * sizeof *o);
			if (o) {
				for (int i = 0; i < out->nmissoes; ++i) { numero_interno(&out->missoes[i], &o[i].n, o[i].interno, sizeof o[i].interno); o[i].m = out->missoes[i]; }
				qsort(o, (size_t)out->nmissoes, sizeof *o, cmp_ord);
				for (int i = 0; i < out->nmissoes; ++i) out->missoes[i] = o[i].m;
				free(o);
			}
		} else orden_historia(out, sg);
	}
	if (sg) {
		for (int i = 0; i < nsec; ++i) { for (int t = 0; t < sg[i].n; ++t) free(sg[i].alvo[t]); free(sg[i].alvo); }
		free(sg);
	}
	tz_hpfs_fechar(&c);
	if (rc) {
		char motivo[256];
		snprintf(motivo, sizeof motivo, "%s", tz_erro());
		for (int i = 0; i < out->nmissoes; ++i) missao_liberar(&out->missoes[i]);
		free(out->missoes); free(out->descricao);
		memset(out, 0, sizeof *out);
		return tz_falha("%s", motivo);
	}
	return 0;
}

static int listar(const char *dir, const char *ext, char ***out, int *n)
{
	DIR *d = opendir(dir);
	if (!d) return -1;
	struct dirent *de;
	char **v = NULL;
	int k = 0;
	while ((de = readdir(d))) {
		size_t l = strlen(de->d_name), le = strlen(ext);
		if (l <= le || strcasecmp(de->d_name + l - le, ext)) continue;
		char **nv = realloc(v, (size_t)(k + 1) * sizeof *nv);
		if (!nv) break;
		v = nv;
		v[k++] = tz_strdup(de->d_name);
	}
	closedir(d);
	if (k > 1) qsort(v, (size_t)k, sizeof *v, cmp_str);
	*out = v; *n = k;
	return 0;
}

int tz_mapas_ler(TzMapas *c, const char *raiz, int *falhas)
{
	memset(c, 0, sizeof *c);
	snprintf(c->raiz, sizeof c->raiz, "%s", raiz);
	int ff = 0;
	char dir[700];
	char **nomes = NULL;
	int nn = 0;
	snprintf(dir, sizeof dir, "%s/Maps", raiz);
	if (!listar(dir, ".wmp", &nomes, &nn)) {
		c->soltas = calloc((size_t)nn + 1, sizeof *c->soltas);
		for (int i = 0; i < nn && c->soltas; ++i) {
			char fich[400], abs_[1200];
			snprintf(fich, sizeof fich, "Maps/%s", nomes[i]);
			snprintf(abs_, sizeof abs_, "%s/%s", raiz, fich);
			TzHpfs h;
			if (tz_hpfs_abrir(&h, abs_)) { ff++; continue; }
			if (misiao_de(&h, fich, "", 1, &c->soltas[c->nsoltas])) ff++; else c->nsoltas++;
			tz_hpfs_fechar(&h);
		}
		for (int i = 0; i < nn; ++i) free(nomes[i]);
		free(nomes);
	}
	nomes = NULL; nn = 0;
	snprintf(dir, sizeof dir, "%s/Campaign", raiz);
	if (!listar(dir, ".wcm", &nomes, &nn)) {
		c->camp = calloc((size_t)nn + 1, sizeof *c->camp);
		for (int i = 0; i < nn && c->camp; ++i) {
			char fich[400];
			snprintf(fich, sizeof fich, "Campaign/%s", nomes[i]);
			if (campanha_ler(raiz, fich, &c->camp[c->ncamp])) ff++; else c->ncamp++;
		}
		for (int i = 0; i < nn; ++i) free(nomes[i]);
		free(nomes);
	}
	if (falhas) *falhas = ff;
	return 0;
}

void tz_mapas_liberar(TzMapas *c)
{
	for (int i = 0; i < c->nsoltas; ++i) missao_liberar(&c->soltas[i]);
	free(c->soltas);
	for (int k = 0; k < c->ncamp; ++k) {
		for (int i = 0; i < c->camp[k].nmissoes; ++i) missao_liberar(&c->camp[k].missoes[i]);
		free(c->camp[k].missoes); free(c->camp[k].descricao);
	}
	free(c->camp);
	memset(c, 0, sizeof *c);
}

/* ---------------------------------------------------------------- abrir una misión */
int tz_missao_abrir(const TzMapas *c, const TzMissao *mi, TzMissaoAberta *out)
{
	memset(out, 0, sizeof *out);
	char abs_[1100];
	snprintf(abs_, sizeof abs_, "%s/%s", c->raiz, mi->fich);
	if (tz_hpfs_abrir(&out->c, abs_)) return -1;
	TzHpfs *m = &out->c;
	if (mi->dentro[0]) {
		if (tz_hpfs_anidado(&out->c, mi->dentro, &out->sub)) { tz_hpfs_fechar(&out->c); return -1; }
		out->tem_sub = 1;
		m = &out->sub;
	}
	if (tz_mapa_ler(m, &out->mapa)) { tz_missao_fechar(out); return -1; }
	out->guiao_ok = tz_guiao_ler(&out->guiao, m) == 0;
	if (!out->guiao_ok) snprintf(out->motivo, sizeof out->motivo, "%s", tz_erro());
	return 0;
}

void tz_missao_fechar(TzMissaoAberta *a)
{
	tz_guiao_liberar(&a->guiao);
	tz_mapa_liberar(&a->mapa);
	if (a->tem_sub) tz_hpfs_fechar(&a->sub);
	tz_hpfs_fechar(&a->c);
	memset(a, 0, sizeof *a);
}

/* ---------------------------------------------------------------- de una misión a la siguiente */
void tz_levar_de(const TzMundo *m, const TzGuiao *g, TzLevar *out)
{
	out->nherois = tz_guiao_herois(m, g, out->herois, TZ_LEVAR_HEROIS);
	out->nmantidas = tz_guiao_mantidas(g, out->mantidas, 256);
}

void tz_levar_para(TzMundo *m, TzGuiao *g, const TzLevar *l)
{
	tz_guiao_mantidas_definir(g, l->mantidas, l->nmantidas);
	tz_guiao_restaurar_herois(m, g, l->herois, l->nherois);
}

/* ---------------------------------------------------------------- progreso */
void tz_progresso_liberar(TzProgresso *p)
{
	for (int i = 0; i < p->n; ++i) free(p->ids[i]);
	free(p->ids);
	memset(p, 0, sizeof *p);
}

int tz_progresso_tem(const TzProgresso *p, const char *id)
{
	for (int i = 0; i < p->n; ++i) if (!strcmp(p->ids[i], id)) return 1;
	return 0;
}

static int progresso_adicionar(TzProgresso *p, const char *id)
{
	if (tz_progresso_tem(p, id)) return 0;
	if (p->n == p->cap) {
		int nc = p->cap ? p->cap * 2 : 16;
		char **nv = realloc(p->ids, (size_t)nc * sizeof *nv);
		if (!nv) return -1;
		p->ids = nv; p->cap = nc;
	}
	p->ids[p->n] = tz_strdup(id);
	if (!p->ids[p->n]) return -1;
	p->n++;
	return 1;
}

int tz_progresso_ler(TzProgresso *p, const char *ruta)
{
	memset(p, 0, sizeof *p);
	FILE *f = fopen(ruta, "rb");
	if (!f) return 0;
	char linha[400];
	if (!fgets(linha, sizeof linha, f) || strncmp(linha, "TZPROG1", 7)) { fclose(f); return 0; }
	while (fgets(linha, sizeof linha, f) && p->n < 4096) {
		size_t l = strlen(linha);
		while (l && (linha[l - 1] == '\n' || linha[l - 1] == '\r')) linha[--l] = 0;
		if (l && l < 200) progresso_adicionar(p, linha);
	}
	fclose(f);
	return 0;
}

static int progresso_gravar(const TzProgresso *p, const char *ruta)
{
	char tmp[1100];
	snprintf(tmp, sizeof tmp, "%s.tmp", ruta);
	FILE *f = fopen(tmp, "wb");
	if (!f) return tz_falha("no se pudo escribir %s", tmp);
	fputs("TZPROG1\n", f);
	for (int i = 0; i < p->n; ++i) fprintf(f, "%s\n", p->ids[i]);
	if (fclose(f)) return tz_falha("no se pudo escribir %s", tmp);
	if (rename(tmp, ruta)) return tz_falha("no se pudo renombrar %s", tmp);
	return 0;
}

int tz_progresso_marcar(TzProgresso *p, const char *id, const char *ruta)
{
	int r = progresso_adicionar(p, id);
	if (r < 0) return -1;
	if (r == 1 && ruta) return progresso_gravar(p, ruta);
	return 0;
}

int tz_missao_disponivel(const TzProgresso *p, const TzMissao *mi) { return mi->inicial || tz_progresso_tem(p, mi->id); }
