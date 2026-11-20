/* SPDX-License-Identifier: GPL-3.0-or-later */
/* wdt.c - paquetes *.wdt de Tzar: «HMMSYS PackFile» dentro de un LZSS por trozos (D417).
 *
 * Traducido de ReTzar, src/archive.rs (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later).
 * Diferencias: en C, sin la copia portátil de ReTzar; la caché guarda un solo trozo descomprimido
 * y un recurso se lee trozo a trozo, sin tener nunca el paquete entero (107 MB el de imágenes)
 * en memoria. También aquí las utilidades comunes (errores, enteros, claves).
 */
#include "tzar.h"
#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------ utilidades */

static char erro[256];

const char *tz_erro(void) { return erro; }

int tz_falha(const char *fmt, ...)
{
	va_list a;
	va_start(a, fmt);
	vsnprintf(erro, sizeof erro, fmt, a);
	va_end(a);
	return -1;
}

uint32_t tz_u32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
uint16_t tz_u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

void *tz_malloc(size_t n)
{
	void *p = malloc(n ? n : 1);
	if (!p) tz_falha("sin memoria (%lu bytes)", (unsigned long)n);
	return p;
}

char *tz_strdup(const char *s)
{
	size_t n = strlen(s) + 1;
	char *d = tz_malloc(n);
	if (d) memcpy(d, s, n);
	return d;
}

void tz_chave(char *dst, const char *src, size_t n)
{
	size_t i = 0;
	for (; src[i] && i + 1 < n; ++i) {
		unsigned char c = (unsigned char)src[i];
		dst[i] = c == '\\' ? '/' : (c < 128 ? (char)toupper(c) : (char)c);
	}
	dst[i] = 0;
}

uint32_t tz_hash(const char *s)
{
	uint32_t h = 2166136261u;   /* FNV-1a */
	while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
	return h;
}

/* ------------------------------------------------------------------ LZSS */

/* Un trozo: banderas de 1 bit (1 = literal de 8 bits; 0 = referencia de `dist` bits de distancia codificada y
 * `lon` bits de longitud - 2), leído de los bits más altos a los más bajos. Distancia codificada 0 = ceros. */
int tz_lzss_trozo(const uint8_t *src, uint32_t n, uint8_t alg, uint8_t *dst, uint32_t esperado)
{
	int dbits, lbits = alg & 15;
	switch (alg >> 4) {
	case 0xa: dbits = 10; break;
	case 0xb: dbits = 11; break;
	case 0xc: dbits = 12; break;
	default: return tz_falha("algoritmo LZSS no admitido (0x%02x)", alg);
	}
	if (lbits < 1 || lbits > 5 || esperado > TZ_MAX_RECURSO) return tz_falha("anchura LZSS no admitida");
	uint64_t total = (uint64_t)n * 8, pos = 0;
	uint32_t acc = 0;      /* bits pendientes, alineados a la izquierda */
	int nacc = 0;
	uint32_t in = 0, out = 0;
#define LER(var, cnt) do { \
		int _c = (cnt); \
		if (pos + (uint64_t)_c > total) return tz_falha("token LZSS truncado"); \
		while (nacc < _c) { acc |= (uint32_t)src[in++] << (24 - nacc); nacc += 8; } \
		(var) = acc >> (32 - _c); acc <<= _c; nacc -= _c; pos += (uint64_t)_c; \
	} while (0)
	while (out < esperado) {
		uint32_t f, v;
		LER(f, 1);
		if (f) {
			LER(v, 8);
			dst[out++] = (uint8_t)v;
		} else {
			uint32_t d, l;
			LER(d, dbits);
			LER(l, lbits);
			l += 2;
			if (l > esperado - out) l = esperado - out;
			if (d == 0) {
				memset(dst + out, 0, l);
				out += l;
			} else {
				uint32_t dist = (1u << dbits) - d;
				if (dist > out) return tz_falha("referencia LZSS antes del diccionario");
				for (uint32_t k = 0; k < l; ++k, ++out) dst[out] = dst[out - dist];
			}
		}
	}
#undef LER
	return 0;
}

static int validar_lzss(uint32_t tam, uint32_t trozo, uint32_t base, uint32_t n)
{
	if (tam > TZ_MAX_RECURSO || trozo == 0 || trozo > TZ_MAX_RECURSO || base < 18 || base > n || (base - 14) % 4)
		return tz_falha("cabecera LZSS no válida");
	if ((base - 14) / 4 != (tam + trozo - 1) / trozo) return tz_falha("número de trozos LZSS incorrecto");
	return 0;
}

int tz_lzss_desempacotar(const uint8_t *src, uint32_t n, uint8_t **outp, uint32_t *tamp)
{
	if (n < 4 || memcmp(src, "LZSS", 4)) {
		uint8_t *o = tz_malloc(n);
		if (!o) return -1;
		memcpy(o, src, n);
		*outp = o; *tamp = n;
		return 0;
	}
	if (n < 18) return tz_falha("cabecera LZSS truncada");
	uint32_t tam = tz_u32(src + 4), trozo = tz_u32(src + 8), base = tz_u32(src + 14);
	uint8_t alg = src[12];
	if (validar_lzss(tam, trozo, base, n)) return -1;
	uint32_t cnt = (tam + trozo - 1) / trozo;
	uint8_t *o = tz_malloc(tam);
	if (!o) return -1;
	uint32_t hecho = 0;
	for (uint32_t i = 0; i < cnt; ++i) {
		uint32_t ini = tz_u32(src + 14 + i * 4);
		uint32_t fim = i + 1 == cnt ? n : tz_u32(src + 18 + i * 4);
		if (ini < base || ini > fim || fim > n) { free(o); return tz_falha("desplazamiento de trozo LZSS no válido"); }
		uint32_t esp = tam - hecho < trozo ? tam - hecho : trozo;
		if (fim - ini == trozo) memcpy(o + hecho, src + ini, esp);
		else if (tz_lzss_trozo(src + ini, fim - ini, alg, o + hecho, esp)) { free(o); return -1; }
		hecho += esp;
	}
	*outp = o; *tamp = tam;
	return 0;
}

/* ------------------------------------------------------------------ directorio */

/* Directorio HMMSYS: «HMMSYS PackFile», número de entradas en 0x20 y, desde 0x28, por entrada: longitud total
 * del nombre, bytes que comparte con el anterior, el resto del nombre, desplazamiento y tamaño.
 * Devuelve 0, -1 (no válido) o 1 (hacen falta más bytes). */
static int ler_diretorio(TzPacote *p, const uint8_t *d, uint32_t n)
{
	if (n < 0x28) return n < p->tam ? 1 : tz_falha("directorio del paquete truncado");
	if (memcmp(d, "HMMSYS PackFile", 15)) return tz_falha("falta la firma HMMSYS PackFile");
	uint32_t cnt = tz_u32(d + 0x20);
	if (cnt > 100000) return tz_falha("número de entradas no plausible");
	TzEntrada *e = calloc(cnt ? cnt : 1, sizeof *e);
	if (!e) return tz_falha("sin memoria para el directorio");
	char ant[256] = "";
	uint32_t c = 0x28, i;
	int r = 0;
	for (i = 0; i < cnt; ++i) {
		if (c + 2 > n) { r = n < p->tam ? 1 : tz_falha("directorio truncado"); break; }
		uint32_t total = d[c], pre = d[c + 1];
		c += 2;
		if (pre > strlen(ant) || pre > total) { r = tz_falha("prefijo de nombre no válido"); break; }
		if (c + (total - pre) + 8 > n) { r = n < p->tam ? 1 : tz_falha("directorio truncado"); break; }
		char nome[256];
		memcpy(nome, ant, pre);
		memcpy(nome + pre, d + c, total - pre);
		nome[total] = 0;
		c += total - pre;
		uint32_t pos = tz_u32(d + c), tam = tz_u32(d + c + 4);
		c += 8;
		if ((uint64_t)pos + tam > p->tam) { r = tz_falha("entrada fuera del paquete"); break; }
		memcpy(ant, nome, total + 1);
		/* nombres peligrosos: absolutos, con unidad, con «..» o con un NUL dentro */
		if (strlen(nome) != total) { r = tz_falha("nombre de entrada con NUL"); break; }
		for (char *q = nome; *q; ++q) if (*q == '\\') *q = '/';
		int mal = nome[0] == '/' || strchr(nome, ':') != NULL;
		for (char *q = nome; !mal && *q; ) {
			char *f = strchr(q, '/');
			size_t l = f ? (size_t)(f - q) : strlen(q);
			if (l == 2 && q[0] == '.' && q[1] == '.') mal = 1;
			q += l + (f ? 1 : 0);
			if (!f) break;
		}
		if (mal) { r = tz_falha("ruta de entrada peligrosa: %s", nome); break; }
		e[i].nome = tz_strdup(nome);
		e[i].pos = pos;
		e[i].tam = tam;
		if (!e[i].nome) { r = -1; break; }
	}
	if (!r)
		for (uint32_t k = 0; k < cnt; ++k)
			if (e[k].tam > 0 && e[k].pos < c) { r = tz_falha("entrada encima del directorio"); break; }
	if (r) {
		for (uint32_t k = 0; k < cnt; ++k) free(e[k].nome);
		free(e);
		return r;
	}
	p->e = e;
	p->n = (int)cnt;
	return 0;
}

static int montar_diretorio(TzPacote *p)
{
	uint32_t prova = p->tam < 65536 ? p->tam : 65536;
	for (;;) {
		uint8_t *d = tz_malloc(prova);
		if (!d) return -1;
		if (tz_pacote_intervalo(p, 0, prova, d)) { free(d); return -1; }
		int r = ler_diretorio(p, d, prova);
		free(d);
		if (r <= 0) return r;
		if (prova >= p->tam) return tz_falha("directorio truncado");
		prova = prova > p->tam / 2 ? p->tam : prova * 2;
	}
}

/* ------------------------------------------------------------------ paquetes */

static int ler_em(FILE *f, uint32_t pos, uint32_t n, uint8_t *dst)
{
	if (fseek(f, (long)pos, SEEK_SET)) return tz_falha("no se puede ir a %lu", (unsigned long)pos);
	if (n && fread(dst, 1, n, f) != n) return tz_falha("fin de fichero inesperado");
	return 0;
}

int tz_pacote_abrir(TzPacote *p, const char *ruta)
{
	memset(p, 0, sizeof *p);
	p->cache_i = -1;
	p->f = fopen(ruta, "rb");
	if (!p->f) return tz_falha("no se puede abrir %s", ruta);
	fseek(p->f, 0, SEEK_END);
	long l = ftell(p->f);
	if (l < 0) { tz_pacote_fechar(p); return tz_falha("%s: tamaño desconocido", ruta); }
	p->tam_fisico = (uint32_t)l;
	uint8_t cab[18] = { 0 };
	uint32_t nc = p->tam_fisico < 18 ? p->tam_fisico : 18;
	if (ler_em(p->f, 0, nc, cab)) { tz_pacote_fechar(p); return -1; }
	if (nc >= 4 && !memcmp(cab, "LZSS", 4)) {
		if (nc < 18) { tz_pacote_fechar(p); return tz_falha("%s: cabecera LZSS truncada", ruta); }
		p->lzss = 1;
		p->tam = tz_u32(cab + 4);
		p->trozo = tz_u32(cab + 8);
		p->alg = cab[12];
		uint32_t base = tz_u32(cab + 14);
		if (validar_lzss(p->tam, p->trozo, base, p->tam_fisico)) { tz_pacote_fechar(p); return -1; }
		p->ntrozos = (base - 14) / 4;
		p->offs = tz_malloc((p->ntrozos + 1) * sizeof *p->offs);
		uint8_t *tab = tz_malloc(base - 14);
		if (!p->offs || !tab || ler_em(p->f, 14, base - 14, tab)) { free(tab); tz_pacote_fechar(p); return -1; }
		for (uint32_t i = 0; i < p->ntrozos; ++i) p->offs[i] = tz_u32(tab + i * 4);
		free(tab);
		p->offs[p->ntrozos] = p->tam_fisico;
		for (uint32_t i = 0; i < p->ntrozos; ++i)
			if (p->offs[i] < base || p->offs[i] > p->offs[i + 1]) { tz_pacote_fechar(p); return tz_falha("%s: trozo LZSS fuera de sitio", ruta); }
		p->cache = tz_malloc(p->trozo);
		if (!p->cache) { tz_pacote_fechar(p); return -1; }
	} else {
		p->tam = p->tam_fisico;
	}
	if (montar_diretorio(p)) {
		char m[256];
		snprintf(m, sizeof m, "%s", tz_erro());
		tz_pacote_fechar(p);
		return tz_falha("%s: %s", ruta, m);
	}
	return 0;
}

int tz_pacote_memoria(TzPacote *p, const uint8_t *dados, uint32_t n)
{
	memset(p, 0, sizeof *p);
	p->cache_i = -1;
	if (tz_lzss_desempacotar(dados, n, &p->mem, &p->tam)) return -1;
	if (montar_diretorio(p)) { tz_pacote_fechar(p); return -1; }
	return 0;
}

static const uint8_t *trozo(TzPacote *p, uint32_t i)
{
	if (p->cache_i == (int64_t)i) return p->cache;
	uint32_t ini = p->offs[i], fim = p->offs[i + 1], len = fim - ini;
	uint32_t logico = i * p->trozo;
	uint32_t esp = p->tam - logico < p->trozo ? p->tam - logico : p->trozo;
	p->cache_i = -1;
	if (len == p->trozo) {
		if (ler_em(p->f, ini, esp, p->cache)) return NULL;
	} else {
		if (len > p->trozo * 2 + 64) { tz_falha("trozo LZSS demasiado grande"); return NULL; }
		if (!p->cache_comp) { p->cache_comp = tz_malloc(p->trozo * 2 + 64); if (!p->cache_comp) return NULL; }
		if (ler_em(p->f, ini, len, p->cache_comp)) return NULL;
		if (tz_lzss_trozo(p->cache_comp, len, p->alg, p->cache, esp)) return NULL;
	}
	p->cache_i = i;
	return p->cache;
}

int tz_pacote_intervalo(TzPacote *p, uint32_t pos, uint32_t tam, uint8_t *dst)
{
	if ((uint64_t)pos + tam > p->tam) return tz_falha("recurso fuera del paquete");
	if (!tam) return 0;
	if (p->mem) { memcpy(dst, p->mem + pos, tam); return 0; }
	if (!p->lzss) return ler_em(p->f, pos, tam, dst);
	uint32_t fim = pos + tam;
	for (uint32_t i = pos / p->trozo; i <= (fim - 1) / p->trozo; ++i) {
		const uint8_t *t = trozo(p, i);
		if (!t) return -1;
		uint32_t logico = i * p->trozo;
		uint32_t de = pos > logico ? pos - logico : 0;
		uint32_t ate = fim - logico < p->trozo ? fim - logico : p->trozo;
		memcpy(dst + (logico + de - pos), t + de, ate - de);
	}
	return 0;
}

uint8_t *tz_pacote_ler(TzPacote *p, const TzEntrada *e, uint32_t *tam)
{
	if (p->pasta) {
		char ruta[1024];
		snprintf(ruta, sizeof ruta, "%s/%s", p->pasta, e->nome);
		FILE *f = fopen(ruta, "rb");
		if (!f) { tz_falha("no se puede abrir %s", ruta); return NULL; }
		uint8_t *d = tz_malloc(e->tam);
		if (d && e->tam && fread(d, 1, e->tam, f) != e->tam) { free(d); d = NULL; tz_falha("%s: lectura incompleta", ruta); }
		fclose(f);
		if (d && tam) *tam = e->tam;
		return d;
	}
	uint8_t *d = tz_malloc(e->tam);
	if (!d) return NULL;
	if (tz_pacote_intervalo(p, e->pos, e->tam, d)) { free(d); return NULL; }
	if (tam) *tam = e->tam;
	return d;
}

/* Carpetas sueltas (DATA, IMAGES, LOCAL, HELP) bajo la instalación, con el nombre que les daría un paquete
 * («DATA/AI/AI.INI»). Las versiones digitales guardan así los datos, sin paquetes. */
static int recorrer(TzPacote *p, const char *raiz, const char *rel, int *cap, int prof)
{
	if (prof > 16) return 0;
	char ruta[1024];
	snprintf(ruta, sizeof ruta, "%s/%s", raiz, rel);
	DIR *d = opendir(ruta);
	if (!d) return 0;
	struct dirent *de;
	while ((de = readdir(d))) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
		char sub[1024], abs_[1024];
		snprintf(sub, sizeof sub, "%s/%s", rel, de->d_name);
		snprintf(abs_, sizeof abs_, "%s/%s", raiz, sub);
		struct stat st;
		if (stat(abs_, &st)) continue;
		if (S_ISDIR(st.st_mode)) { recorrer(p, raiz, sub, cap, prof + 1); continue; }
		if (!S_ISREG(st.st_mode) || (uint64_t)st.st_size > TZ_MAX_RECURSO) continue;
		if (p->n == *cap) {
			int nc = *cap ? *cap * 2 : 256;
			TzEntrada *ne = realloc(p->e, (size_t)nc * sizeof *ne);
			if (!ne) break;
			p->e = ne; *cap = nc;
		}
		p->e[p->n].nome = tz_strdup(sub);
		p->e[p->n].pos = 0;
		p->e[p->n].tam = (uint32_t)st.st_size;
		if (p->e[p->n].nome) p->n++;
	}
	closedir(d);
	return 0;
}

int tz_pacote_pasta(TzPacote *p, const char *raiz, const char *const *topos, int ntopos)
{
	memset(p, 0, sizeof *p);
	p->cache_i = -1;
	p->pasta = tz_strdup(raiz);
	if (!p->pasta) return -1;
	DIR *d = opendir(raiz);
	if (!d) return tz_falha("no se puede abrir la carpeta %s", raiz);
	int cap = 0;
	struct dirent *de;
	while ((de = readdir(d))) {
		for (int i = 0; i < ntopos; ++i)
			if (!strcasecmp(de->d_name, topos[i])) {
				char abs_[1024];
				struct stat st;
				snprintf(abs_, sizeof abs_, "%s/%s", raiz, de->d_name);
				if (!stat(abs_, &st) && S_ISDIR(st.st_mode)) recorrer(p, raiz, de->d_name, &cap, 0);
			}
	}
	closedir(d);
	return 0;
}

void tz_pacote_fechar(TzPacote *p)
{
	if (p->f) fclose(p->f);
	for (int i = 0; i < p->n; ++i) free(p->e[i].nome);
	free(p->e);
	free(p->offs);
	free(p->cache);
	free(p->cache_comp);
	free(p->mem);
	free(p->pasta);
	memset(p, 0, sizeof *p);
	p->cache_i = -1;
}
