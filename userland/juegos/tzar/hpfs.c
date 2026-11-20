/* SPDX-License-Identifier: GPL-3.0-or-later */
/* hpfs.c - contenedores HPFS de Haemimont: mapas *.wmp y campañas *.wcm (D417).
 *
 * Traducido de ReTzar, src/hpfs.rs (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later).
 * Bloques de 512 bytes; el bloque 1 es el inodo del directorio raíz. Un inodo lleva el tamaño, la
 * profundidad de indirección (0-3) y 126 punteros; una página indirecta, 128 punteros. Un directorio es
 * una lista de (inodo, tipo 0 fichero / 1 carpeta, longitud del nombre, nombre). Una campaña guarda sus
 * mapas como ficheros que son a su vez HPFS: se abren como vistas (tramos) sobre el padre, sin copiarlos.
 */
#include "tzar.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define BLOCO 512u

static int intervalo(TzHpfs *c, uint32_t pos, uint32_t n, uint8_t *dst)
{
	if ((uint64_t)pos + n > c->tam) return tz_falha("bloque HPFS truncado");
	if (!n) return 0;
	if (c->mem) { memcpy(dst, c->mem + pos, n); return 0; }
	if (c->f) {
		if (fseek(c->f, (long)pos, SEEK_SET) || fread(dst, 1, n, c->f) != n) return tz_falha("lectura HPFS incompleta");
		return 0;
	}
	/* anidado: los tramos están ordenados por inicio lógico */
	uint32_t fim = pos + n;
	for (int i = 0; i < c->ntramos; ++i) {
		uint32_t ini = c->tramos[i][0], em = c->tramos[i][1], len = c->tramos[i][2];
		if (ini + len <= pos) continue;
		if (ini >= fim) break;
		uint32_t de = pos > ini ? pos : ini, ate = fim < ini + len ? fim : ini + len;
		if (intervalo(c->pai, em + (de - ini), ate - de, dst + (de - pos))) return -1;
	}
	return 0;
}

static int bloco(TzHpfs *c, uint32_t b, uint8_t *dst)
{
	if (b == 0 || b >= c->nblocos) return tz_falha("bloque HPFS fuera del contenedor: %lu", (unsigned long)b);
	uint32_t pos = b * BLOCO, n = c->tam - pos < BLOCO ? c->tam - pos : BLOCO;
	memset(dst, 0, BLOCO);
	return intervalo(c, pos, n, dst);
}

static int coletar(TzHpfs *c, uint32_t b, uint32_t prof, uint32_t cnt, uint32_t *out, uint32_t *n)
{
	if (b == 0 || b >= c->nblocos) return tz_falha("bloque HPFS fuera del contenedor: %lu", (unsigned long)b);
	if (prof == 0) { out[(*n)++] = b; return 0; }
	uint8_t pg[BLOCO];
	if (bloco(c, b, pg)) return -1;
	for (int i = 0; i < 128 && *n < cnt; ++i)
		if (coletar(c, tz_u32(pg + i * 4), prof - 1, cnt, out, n)) return -1;
	return 0;
}

/* Los bloques de un inodo, juntados en tramos contiguos (inicio lógico, posición, longitud). */
static int tramos_de(TzHpfs *c, uint32_t inodo, uint32_t *tam, uint32_t (**tr)[3], int *ntr)
{
	uint8_t no[BLOCO];
	if (bloco(c, inodo, no)) return -1;
	uint32_t t = tz_u32(no), prof = tz_u32(no + 4);
	if (t > TZ_MAX_RECURSO || prof > 3) return tz_falha("inodo HPFS no válido");
	uint32_t cnt = (t + BLOCO - 1) / BLOCO, n = 0;
	if (cnt > c->nblocos) return tz_falha("inodo HPFS más grande que el contenedor");
	uint32_t *bl = tz_malloc((cnt ? cnt : 1) * sizeof *bl);
	if (!bl) return -1;
	for (int i = 0; i < 126 && n < cnt; ++i)
		if (coletar(c, tz_u32(no + 8 + i * 4), prof, cnt, bl, &n)) { free(bl); return -1; }
	if (n != cnt) { free(bl); return tz_falha("árbol de bloques HPFS truncado"); }
	uint32_t (*r)[3] = tz_malloc((cnt ? cnt : 1) * sizeof *r);
	if (!r) { free(bl); return -1; }
	int nr = 0;
	for (uint32_t i = 0; i < cnt; ++i) {
		uint32_t logico = i * BLOCO, len = t - logico < BLOCO ? t - logico : BLOCO, em = bl[i] * BLOCO;
		if (nr && r[nr - 1][1] + r[nr - 1][2] == em && r[nr - 1][0] + r[nr - 1][2] == logico) r[nr - 1][2] += len;
		else { r[nr][0] = logico; r[nr][1] = em; r[nr][2] = len; nr++; }
	}
	free(bl);
	*tam = t; *tr = r; *ntr = nr;
	return 0;
}

static uint8_t *ler_inodo(TzHpfs *c, uint32_t inodo, uint32_t *tam)
{
	uint32_t t, (*r)[3];
	int nr;
	if (tramos_de(c, inodo, &t, &r, &nr)) return NULL;
	uint8_t *d = tz_malloc(t);
	if (d)
		for (int i = 0; i < nr; ++i)
			if (intervalo(c, r[i][1], r[i][2], d + r[i][0])) { free(d); d = NULL; break; }
	free(r);
	if (d && tam) *tam = t;
	return d;
}

#define MAX_VISTOS 4096
static int diretorio(TzHpfs *c, uint32_t inodo, const char *pref, uint32_t *vistos, int *nvistos, int prof, int *cap)
{
	if (prof > 32) return tz_falha("directorio HPFS demasiado profundo");
	for (int i = 0; i < *nvistos; ++i) if (vistos[i] == inodo) return tz_falha("ciclo en el directorio HPFS");
	if (*nvistos >= MAX_VISTOS) return tz_falha("demasiadas carpetas HPFS");
	vistos[(*nvistos)++] = inodo;
	uint32_t n;
	uint8_t *d = ler_inodo(c, inodo, &n);
	if (!d) return -1;
	uint32_t a = 0;
	int r = 0;
	while (a < n) {
		if (a + 8 > n) { r = tz_falha("entrada HPFS truncada"); break; }
		uint32_t filho = tz_u32(d + a);
		uint16_t tipo = tz_u16(d + a + 4), len = tz_u16(d + a + 6);
		a += 8;
		if (a + len > n) { r = tz_falha("nombre HPFS truncado"); break; }
		char nome[600];
		if (len == 0 || strlen(pref) + len + 2 > sizeof nome) { r = tz_falha("nombre HPFS no válido"); break; }
		int mal = (len == 1 && d[a] == '.') || (len == 2 && d[a] == '.' && d[a + 1] == '.');
		for (uint32_t k = 0; k < len; ++k) if (d[a + k] == '/' || d[a + k] == '\\' || d[a + k] == ':' || d[a + k] == 0) mal = 1;
		if (mal) { r = tz_falha("nombre HPFS no válido"); break; }
		snprintf(nome, sizeof nome, "%s%.*s", pref, (int)len, (const char *)d + a);
		a += len;
		if (tipo == 1) {
			char sub[600];
			snprintf(sub, sizeof sub, "%s/", nome);
			if (diretorio(c, filho, sub, vistos, nvistos, prof + 1, cap)) { r = -1; break; }
		} else if (tipo == 0) {
			uint8_t no[BLOCO];
			if (bloco(c, filho, no)) { r = -1; break; }
			if (c->n == *cap) {
				int nc = *cap ? *cap * 2 : 32;
				TzEntrada *ne = realloc(c->e, (size_t)nc * sizeof *ne);
				if (!ne) { r = tz_falha("sin memoria"); break; }
				c->e = ne; *cap = nc;
			}
			c->e[c->n].nome = tz_strdup(nome);
			c->e[c->n].pos = filho;
			c->e[c->n].tam = tz_u32(no);
			if (!c->e[c->n].nome) { r = -1; break; }
			c->n++;
		} else { r = tz_falha("tipo de entrada HPFS no admitido: %u", tipo); break; }
	}
	free(d);
	return r;
}

static int iniciar(TzHpfs *c)
{
	uint8_t cab[12] = { 0 };
	if (c->tam < 12 || intervalo(c, 0, 12, cab) || memcmp(cab, "HPFS", 4)) return tz_falha("falta la firma HPFS");
	c->nblocos = tz_u32(cab + 8);
	if (tz_u32(cab + 4) != BLOCO || c->nblocos == 0 || c->nblocos != (c->tam + BLOCO - 1) / BLOCO)
		return tz_falha("geometría HPFS no válida");
	uint32_t *vistos = tz_malloc(MAX_VISTOS * sizeof *vistos);
	if (!vistos) return -1;
	int nv = 0, cap = 0;
	int r = diretorio(c, 1, "", vistos, &nv, 0, &cap);
	free(vistos);
	return r;
}

int tz_hpfs_abrir(TzHpfs *c, const char *ruta)
{
	memset(c, 0, sizeof *c);
	c->f = fopen(ruta, "rb");
	if (!c->f) return tz_falha("no se puede abrir %s", ruta);
	fseek(c->f, 0, SEEK_END);
	long l = ftell(c->f);
	if (l < 0 || (unsigned long)l > 0x7fffffffUL) { tz_hpfs_fechar(c); return tz_falha("%s: tamaño no válido", ruta); }
	c->tam = (uint32_t)l;
	if (iniciar(c)) {
		char m[256];
		snprintf(m, sizeof m, "%s", tz_erro());
		tz_hpfs_fechar(c);
		return tz_falha("%s: %s", ruta, m);
	}
	return 0;
}

int tz_hpfs_memoria(TzHpfs *c, uint8_t *dados, uint32_t n)
{
	memset(c, 0, sizeof *c);
	c->mem = dados;
	c->tam = n;
	if (iniciar(c)) { tz_hpfs_fechar(c); return -1; }
	return 0;
}

const TzEntrada *tz_hpfs_entrada(const TzHpfs *c, const char *nome)
{
	for (int i = 0; i < c->n; ++i)
		if (!strcasecmp(c->e[i].nome, nome)) return &c->e[i];
	return NULL;
}

int tz_hpfs_anidado(TzHpfs *pai, const char *nome, TzHpfs *c)
{
	memset(c, 0, sizeof *c);
	const TzEntrada *e = tz_hpfs_entrada(pai, nome);
	if (!e) return tz_falha("no está en el contenedor: %s", nome);
	c->pai = pai;
	if (tramos_de(pai, e->pos, &c->tam, &c->tramos, &c->ntramos)) return -1;
	if (iniciar(c)) {
		char m[256];
		snprintf(m, sizeof m, "%s", tz_erro());
		tz_hpfs_fechar(c);
		return tz_falha("%s: %s", nome, m);
	}
	return 0;
}

uint8_t *tz_hpfs_ler(TzHpfs *c, const char *nome, uint32_t *tam)
{
	const TzEntrada *e = tz_hpfs_entrada(c, nome);
	if (!e) { tz_falha("no está en el contenedor: %s", nome); return NULL; }
	return ler_inodo(c, e->pos, tam);
}

void tz_hpfs_fechar(TzHpfs *c)
{
	if (c->f) fclose(c->f);
	free(c->mem);
	free(c->tramos);
	for (int i = 0; i < c->n; ++i) free(c->e[i].nome);
	free(c->e);
	memset(c, 0, sizeof *c);
}
