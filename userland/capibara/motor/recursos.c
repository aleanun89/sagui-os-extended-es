/* SPDX-License-Identifier: MIT */
/* recursos.c - ficheros del proyecto (carpeta o paquete .capi), caché de texturas y de sonidos, decodificador de WAV. */
#include "capi.h"
#include <ctype.h>

/* ---------------------------------------------------------------------------------------------------- ficheros y paquete */
static char raiz[CAPI_RUTA_MAX];
static unsigned char *pq; static size_t pq_n;
typedef struct { char ruta[CAPI_RUTA_MAX]; size_t off, tam; } PEnt;
static PEnt *pe; static int npe;

const char *cap_raiz(void) { return raiz; }
int cap_es_paquete(void) { return pq != NULL; }

char *cap_unir(const char *base, const char *rel, char *out, size_t n) {
	char t[CAPI_RUTA_MAX * 2];
	if (*rel == '/') snprintf(t, sizeof t, "%s", rel + 1);
	else {
		snprintf(t, sizeof t, "%s", base ? base : "");
		char *b = strrchr(t, '/'); if (b) b[1] = 0; else t[0] = 0;
		strncat(t, rel, sizeof t - strlen(t) - 1);
	}
	/* normaliza «./» y «x/../»: recorre los trozos separados por «/» */
	char *seg[64]; int ns = 0;
	for (char *c = t; *c;) {
		char *fin = strchr(c, '/'); if (fin) *fin = 0;
		if (*c && strcmp(c, ".")) { if (!strcmp(c, "..")) { if (ns) --ns; } else if (ns < 64) seg[ns++] = c; }
		if (!fin) break;
		c = fin + 1;
	}
	out[0] = 0;
	for (int i = 0; i < ns; ++i) { if (i) strncat(out, "/", n - strlen(out) - 1); strncat(out, seg[i], n - strlen(out) - 1); }
	return out;
}

static unsigned char *leer_fichero(const char *ruta, size_t *tam) {
	FILE *f = fopen(ruta, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
	if (l < 0) { fclose(f); return NULL; }
	unsigned char *b = malloc((size_t)l + 1);
	if (!b) { fclose(f); return NULL; }
	size_t r = fread(b, 1, (size_t)l, f); fclose(f);
	b[r] = 0; if (tam) *tam = r;
	return b;
}

unsigned char *cap_leer(const char *ruta, size_t *tam) {
	char p[CAPI_RUTA_MAX * 2];
	cap_unir("", ruta, p, sizeof p);
	if (pq) {
		for (int i = 0; i < npe; ++i) if (!strcmp(pe[i].ruta, p)) {
			unsigned char *b = malloc(pe[i].tam + 1); memcpy(b, pq + pe[i].off, pe[i].tam); b[pe[i].tam] = 0;
			if (tam) *tam = pe[i].tam;
			return b;
		}
		return NULL;
	}
	char full[CAPI_RUTA_MAX * 2 + 2];
	snprintf(full, sizeof full, "%s/%s", raiz, p);
	return leer_fichero(full, tam);
}
int cap_existe(const char *ruta) { size_t n; unsigned char *b = cap_leer(ruta, &n); free(b); return b != NULL; }

int cap_abrir_proyecto(const char *ruta, char *err, size_t nerr) {
	escena_vaciar_cache();
	size_t l = strlen(ruta);
	free(pq); pq = NULL; free(pe); pe = NULL; npe = 0;
	if (l > 5 && !strcmp(ruta + l - 5, ".capi")) {
		size_t n; unsigned char *b = leer_fichero(ruta, &n);
		if (!b || n < 6 || memcmp(b, "CAPI1\n", 6)) { if (err) snprintf(err, nerr, "no es un paquete .capi: %s", ruta); free(b); return -1; }
		size_t o = 6; int cap = 0;
		while (o < n && b[o] != '\n') {
			char *t = (char *)b + o; char *nl = memchr(t, '\n', n - o); if (!nl) break;
			*nl = 0; char *tab = strchr(t, '\t'); if (!tab) break;
			*tab = 0;
			if (npe == cap) { cap = cap ? cap * 2 : 32; pe = realloc(pe, sizeof(PEnt) * (size_t)cap); }
			snprintf(pe[npe].ruta, sizeof pe[0].ruta, "%s", t); pe[npe].tam = (size_t)atol(tab + 1); ++npe;
			o += (size_t)(nl - t) + 1;
		}
		++o;   /* la línea vacía */
		size_t off = o;
		for (int i = 0; i < npe; ++i) { pe[i].off = off; off += pe[i].tam; }
		if (off > n) { if (err) snprintf(err, nerr, "paquete .capi truncado"); free(b); return -1; }
		pq = b; pq_n = n; raiz[0] = 0;
		return 0;
	}
	snprintf(raiz, sizeof raiz, "%s", ruta);
	size_t rl = strlen(raiz); while (rl > 1 && raiz[rl - 1] == '/') raiz[--rl] = 0;
	return 0;
}

/* lista los ficheros de la carpeta de forma recursiva (para empaquetar) */
#include <dirent.h>
#include <sys/stat.h>
static int listar(const char *base, const char *rel, char (*lista)[CAPI_RUTA_MAX], int max, int n) {
	char dir[CAPI_RUTA_MAX * 2];
	snprintf(dir, sizeof dir, "%s/%s", base, rel);
	DIR *d = opendir(dir);
	if (!d) return n;
	struct dirent *e;
	while ((e = readdir(d)) != NULL) {
		if (e->d_name[0] == '.') continue;
		char r[CAPI_RUTA_MAX], full[CAPI_RUTA_MAX * 2 + 2];
		snprintf(r, sizeof r, "%s%s%s", rel, *rel ? "/" : "", e->d_name);
		snprintf(full, sizeof full, "%s/%s", base, r);
		struct stat st;
		if (stat(full, &st) != 0) continue;
		if (S_ISDIR(st.st_mode)) n = listar(base, r, lista, max, n);
		else if (n < max) snprintf(lista[n++], CAPI_RUTA_MAX, "%s", r);
	}
	closedir(d);
	return n;
}
static int cmp_ruta(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

int cap_empaquetar(const char *carpeta, const char *salida, char *err, size_t nerr) {
	static char lista[512][CAPI_RUTA_MAX];
	int n = listar(carpeta, "", lista, 512, 0);
	if (n == 0) { if (err) snprintf(err, nerr, "carpeta vacía o inexistente: %s", carpeta); return -1; }
	qsort(lista, (size_t)n, CAPI_RUTA_MAX, cmp_ruta);
	FILE *o = fopen(salida, "wb");
	if (!o) { if (err) snprintf(err, nerr, "no se puede escribir %s", salida); return -1; }
	fputs("CAPI1\n", o);
	size_t *tam = calloc((size_t)n, sizeof(size_t));
	for (int i = 0; i < n; ++i) {
		char full[CAPI_RUTA_MAX * 2 + 2]; snprintf(full, sizeof full, "%s/%s", carpeta, lista[i]);
		size_t t = 0; unsigned char *b = leer_fichero(full, &t); free(b); tam[i] = t;
		fprintf(o, "%s\t%lu\n", lista[i], (unsigned long)t);
	}
	fputc('\n', o);
	for (int i = 0; i < n; ++i) {
		char full[CAPI_RUTA_MAX * 2 + 2]; snprintf(full, sizeof full, "%s/%s", carpeta, lista[i]);
		size_t t = 0; unsigned char *b = leer_fichero(full, &t);
		if (b) fwrite(b, 1, t, o);
		free(b);
	}
	free(tam); fclose(o);
	return n;
}

/* ---------------------------------------------------------------------------------------------------- texturas */
static Textura *texs; static int ntex;
static RecDecodificador decod;
void rec_poner_decodificador(RecDecodificador f) { decod = f; }
int rec_numero_texturas(void) { return ntex; }
Textura *rec_primera(void) { return texs; }

static uint32_t *sin_decodificar(const unsigned char *d, size_t n, int *w, int *h) {
	/* sin decodificador (pruebas sin ventana): solo las medidas del PNG */
	if (n < 24 || memcmp(d, "\x89PNG", 4)) return NULL;
	*w = (int)((d[16] << 24) | (d[17] << 16) | (d[18] << 8) | d[19]); *h = (int)((d[20] << 24) | (d[21] << 16) | (d[22] << 8) | d[23]);
	return NULL;
}

Textura *rec_textura(const char *ruta) {
	char p[CAPI_RUTA_MAX * 2]; cap_unir("", ruta, p, sizeof p);
	for (Textura *t = texs; t; t = t->sig) if (!strcmp(t->ruta, p)) return t;
	size_t n; unsigned char *b = cap_leer(p, &n);
	if (!b) { capi_log("textura no encontrada: %s", p); return NULL; }
	int w = 0, h = 0; uint32_t *px = NULL;
	if (decod) px = decod(b, n, &w, &h); else sin_decodificar(b, n, &w, &h);
	free(b);
	if (!px && !(!decod && w > 0)) { capi_log("no se pudo decodificar la textura %s", p); return NULL; }
	Textura *t = calloc(1, sizeof *t);
	snprintf(t->ruta, sizeof t->ruta, "%s", p); t->w = w; t->h = h; t->px = px; t->id = ++ntex; t->sig = texs; texs = t;
	return t;
}

Textura *rec_textura_memoria(const char *nombre, int w, int h, uint32_t *px) {
	for (Textura *t = texs; t; t = t->sig) if (!strcmp(t->ruta, nombre)) return t;
	Textura *t = calloc(1, sizeof *t);
	snprintf(t->ruta, sizeof t->ruta, "%s", nombre); t->w = w; t->h = h; t->px = px; t->id = ++ntex; t->sig = texs; texs = t;
	return t;
}

/* ---------------------------------------------------------------------------------------------------- sonidos (WAV) */
typedef struct SonC { char ruta[CAPI_RUTA_MAX]; Sonido s; struct SonC *sig; } SonC;
static SonC *sons;
static uint32_t rd32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint32_t rd16(const unsigned char *p) { return p[0] | (p[1] << 8); }

static int wav_decodificar(const unsigned char *d, size_t size, Sonido *s) {
	if (size < 44 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) return 0;
	size_t pos = 12; int ch = 1, bits = 16, fmt = 1; uint32_t rate = 22050; const unsigned char *data = NULL; size_t dlen = 0;
	while (pos + 8 <= size) {
		uint32_t len = rd32(d + pos + 4);
		if (!memcmp(d + pos, "fmt ", 4) && pos + 24 <= size) { fmt = (int)rd16(d + pos + 8); ch = (int)rd16(d + pos + 10); rate = rd32(d + pos + 12); bits = (int)rd16(d + pos + 22); }
		else if (!memcmp(d + pos, "data", 4)) { data = d + pos + 8; dlen = len < size - pos - 8 ? len : size - pos - 8; break; }
		pos += 8 + len + (len & 1);
	}
	if (!data || fmt != 1 || (bits != 8 && bits != 16) || ch < 1 || ch > 2 || rate < 1000) return 0;
	int bps = bits / 8, nin = (int)(dlen / ((size_t)bps * (size_t)ch));
	if (nin < 1) return 0;
	int nout = (int)((long long)nin * MEZ_TASA / (long long)rate);
	if (nout < 1) nout = 1;
	int16_t *o = malloc(sizeof(int16_t) * 2 * (size_t)nout);
	if (!o) return 0;
	for (int i = 0; i < nout; ++i) {
		double fp = (double)i * rate / MEZ_TASA; int i0 = (int)fp, i1 = i0 + 1 < nin ? i0 + 1 : i0; double fr = fp - i0;
		for (int c = 0; c < 2; ++c) {
			int cc = c < ch ? c : 0; double v0, v1;
			if (bits == 16) { v0 = (int16_t)rd16(data + ((size_t)i0 * ch + cc) * 2); v1 = (int16_t)rd16(data + ((size_t)i1 * ch + cc) * 2); }
			else { v0 = (data[(size_t)i0 * ch + cc] - 128) * 256.0; v1 = (data[(size_t)i1 * ch + cc] - 128) * 256.0; }
			o[2 * i + c] = (int16_t)(v0 + (v1 - v0) * fr);
		}
	}
	s->pcm = o; s->n = nout;
	return 1;
}

Sonido *rec_sonido(const char *ruta) {
	char p[CAPI_RUTA_MAX * 2]; cap_unir("", ruta, p, sizeof p);
	for (SonC *c = sons; c; c = c->sig) if (!strcmp(c->ruta, p)) return c->s.pcm ? &c->s : NULL;
	size_t n; unsigned char *b = cap_leer(p, &n);
	SonC *c = calloc(1, sizeof *c); snprintf(c->ruta, sizeof c->ruta, "%s", p); c->sig = sons; sons = c;
	if (!b) { capi_log("sonido no encontrado: %s", p); return NULL; }
	int ok = wav_decodificar(b, n, &c->s); free(b);
	if (!ok) { capi_log("sonido no válido (solo WAV PCM de 8 o 16 bits): %s", p); return NULL; }
	return &c->s;
}

void rec_liberar_proyecto(void) {
	Textura **pp = &texs;
	while (*pp) { Textura *t = *pp; if (!strncmp(t->ruta, "capibara:", 9)) pp = &t->sig; else { *pp = t->sig; free(t->px); free(t); } }
	while (sons) { SonC *c = sons; sons = c->sig; free(c->s.pcm); free(c); }
}

void rec_liberar_todo(void) {
	while (texs) { Textura *t = texs; texs = t->sig; free(t->px); free(t); }
	while (sons) { SonC *c = sons; sons = c->sig; free(c->s.pcm); free(c); }
	ntex = 0;
}
