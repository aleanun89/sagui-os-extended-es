/* SPDX-License-Identifier: MIT */
/* escritor.c - edición de escenas (.esc) línea a línea (D460). Ver escritor.h. */
#include "escritor.h"
#include <ctype.h>

/* ---------------------------------------------------------------------------------------------------- utilidades de líneas */
static int es_vacia(const char *s) { while (*s == ' ' || *s == '\t' || *s == '\r') ++s; return *s == 0; }
static int es_comentario(const char *s) { while (*s == ' ' || *s == '\t') ++s; return *s == '#' || *s == ';'; }
static int es_vacia_o_comentario(const char *s) { return es_vacia(s) || es_comentario(s); }

static char *dup(const char *s) { size_t l = strlen(s); char *r = malloc(l + 1); memcpy(r, s, l + 1); return r; }

static void ins_lineas(EscDoc *d, int pos, char *const *nuevas, int n) {
	if (n <= 0) return;
	if (pos < 0) pos = 0;
	if (pos > d->n) pos = d->n;
	if (d->n + n > d->cap) { d->cap = (d->n + n) * 2; d->lin = realloc(d->lin, sizeof(char *) * (size_t)d->cap); }
	memmove(d->lin + pos + n, d->lin + pos, sizeof(char *) * (size_t)(d->n - pos));
	for (int i = 0; i < n; ++i) d->lin[pos + i] = dup(nuevas[i]);
	d->n += n;
}
static void ins_linea(EscDoc *d, int pos, const char *s) { char *a[1] = { (char *)s }; ins_lineas(d, pos, a, 1); }

static void del_lineas(EscDoc *d, int a, int b) {   /* [a, b) */
	if (a < 0) a = 0;
	if (b > d->n) b = d->n;
	if (b <= a) return;
	for (int i = a; i < b; ++i) free(d->lin[i]);
	memmove(d->lin + a, d->lin + b, sizeof(char *) * (size_t)(d->n - b));
	d->n -= b - a;
	/* dos líneas en blanco seguidas donde se borró: se queda una */
	if (a > 0 && a < d->n && es_vacia(d->lin[a - 1]) && es_vacia(d->lin[a])) { free(d->lin[a]); memmove(d->lin + a, d->lin + a + 1, sizeof(char *) * (size_t)(d->n - a - 1)); --d->n; }
}

/* primer «=» fuera de comillas y de comentarios */
static const char *igual_en(const char *p) {
	int q = 0;
	for (const char *s = p; *s; ++s) {
		if (*s == '"') q = !q;
		else if (!q && *s == '=') return s;
		else if (!q && (*s == '#' || (*s == ';' && s > p && (s[-1] == ' ' || s[-1] == '\t')))) return NULL;
	}
	return NULL;
}

/* dónde empieza el comentario final de una línea de propiedad (fuera de comillas, tras un espacio o el «=»); NULL si no hay */
static const char *comentario_en(const char *desde) {
	int q = 0;
	for (const char *s = desde; *s; ++s) {
		if (*s == '"') q = !q;
		else if (!q && (*s == '#' || *s == ';') && (s == desde || s[-1] == ' ' || s[-1] == '\t' || s[-1] == '=')) return s;
	}
	return NULL;
}

/* ---------------------------------------------------------------------------------------------------- análisis */
typedef struct { EscDoc *d; int cur, nodo_cur, raiz_vista; } Est;

static const char *cab_attr_c(const Cab *c, const char *k) { for (int i = 0; i < c->na; ++i) if (!strcmp(c->an[i], k)) return c->av[i]; return NULL; }

static int an_seccion(void *u, const Cab *c, int linea) {
	Est *e = u; EscDoc *d = e->d;
	if (!c->nt) { e->cur = -1; return 0; }
	if (d->nsec == d->csec) { d->csec = d->csec ? d->csec * 2 : 32; d->sec = realloc(d->sec, sizeof(EscSec) * (size_t)d->csec); }
	EscSec *s = &d->sec[d->nsec]; memset(s, 0, sizeof *s);
	s->ini = linea - 1; s->fin = linea; s->mapa = -1;
	const char *t0 = c->t[0];
	snprintf(s->tipo, sizeof s->tipo, "%s", t0);
	if (!strcmp(t0, "capa") || !strcmp(t0, "Capa")) {
		s->clase = ES_CAPA;
		snprintf(s->nombre, sizeof s->nombre, "%s", c->nt > 1 ? c->t[1] : "capa");
		if (e->nodo_cur >= 0 && !strcmp(d->sec[e->nodo_cur].tipo, "MapaCasillas")) s->mapa = e->nodo_cur;
	} else if (!strcmp(t0, "Conexion") || !strcmp(t0, "conexion")) {
		s->clase = ES_CONEXION;
	} else if (!strcmp(t0, "Escena")) {
		s->clase = ES_OTRO;
	} else {
		s->clase = !strcmp(t0, "Instancia") ? ES_INSTANCIA : ES_NODO;
		snprintf(s->nombre, sizeof s->nombre, "%s", c->nt > 1 ? c->t[1] : t0);
		const char *p = cab_attr_c(c, "padre"); if (p && (!strcmp(p, ".") || !strcmp(p, "/"))) p = "";   /* «.» es la raíz, como no poner nada */
		snprintf(s->padre, sizeof s->padre, "%s", p ? p : "");
		if (!e->raiz_vista) { snprintf(s->ruta, sizeof s->ruta, "."); e->raiz_vista = 1; s->padre[0] = 0; }
		else if (s->padre[0]) snprintf(s->ruta, sizeof s->ruta, "%s/%s", s->padre, s->nombre);
		else snprintf(s->ruta, sizeof s->ruta, "%s", s->nombre);
		e->nodo_cur = d->nsec;
	}
	e->cur = d->nsec++;
	return 0;
}
static int an_prop(void *u, const char *k, const Valor *v, int linea) { Est *e = u; (void)k; (void)v; if (e->cur >= 0) e->d->sec[e->cur].fin = linea; return 0; }
static int an_fila(void *u, const char *s, int linea) { Est *e = u; (void)s; if (e->cur >= 0) e->d->sec[e->cur].fin = linea; return 0; }

void esc_analizar(EscDoc *d) {
	size_t n; char *t = esc_texto(d, &n);
	d->nsec = 0;
	Est e = { d, -1, -1, 0 };
	TextoCB cb = { an_seccion, an_prop, an_fila };
	char err[160]; texto_analizar(t, &cb, &e, err, sizeof err);
	free(t);
	for (int i = 0; i < d->nsec; ++i) {
		EscSec *s = &d->sec[i];
		int prev = i ? d->sec[i - 1].fin : 0, a = s->ini;
		while (a - 1 >= prev && es_comentario(d->lin[a - 1])) --a;
		s->ant = a;
	}
}

EscDoc *esc_abrir_texto(const char *texto) {
	EscDoc *d = calloc(1, sizeof *d);
	const char *p = texto ? texto : "";
	while (*p) {
		const char *f = strchr(p, '\n'); size_t l = f ? (size_t)(f - p) : strlen(p);
		char *s = malloc(l + 1); memcpy(s, p, l); s[l] = 0;
		if (d->n == d->cap) { d->cap = d->cap ? d->cap * 2 : 64; d->lin = realloc(d->lin, sizeof(char *) * (size_t)d->cap); }
		d->lin[d->n++] = s;
		if (!f) { d->nl_final = 0; p += l; break; }
		p = f + 1; d->nl_final = 1;
	}
	if (d->n == 0) d->nl_final = 1;
	esc_analizar(d);
	return d;
}

void esc_cerrar(EscDoc *d) {
	if (!d) return;
	for (int i = 0; i < d->n; ++i) free(d->lin[i]);
	free(d->lin); free(d->sec); free(d);
}

char *esc_texto(const EscDoc *d, size_t *n) {
	size_t tot = 1;
	for (int i = 0; i < d->n; ++i) tot += strlen(d->lin[i]) + 1;
	char *r = malloc(tot), *o = r;
	for (int i = 0; i < d->n; ++i) { size_t l = strlen(d->lin[i]); memcpy(o, d->lin[i], l); o += l; if (i + 1 < d->n || d->nl_final) *o++ = '\n'; }
	*o = 0;
	if (n) *n = (size_t)(o - r);
	return r;
}

int esc_guardar(const EscDoc *d, const char *ruta) {
	size_t n; char *t = esc_texto(d, &n);
	FILE *f = fopen(ruta, "wb");
	if (!f) { free(t); return -1; }
	size_t w = fwrite(t, 1, n, f); int c = fclose(f); free(t);
	return w == n && c == 0 ? 0 : -1;
}

/* ---------------------------------------------------------------------------------------------------- consultas */
int esc_buscar_ruta(const EscDoc *d, const char *ruta) {
	if (!ruta || !*ruta) ruta = ".";
	for (int i = 0; i < d->nsec; ++i) if ((d->sec[i].clase == ES_NODO || d->sec[i].clase == ES_INSTANCIA) && !strcmp(d->sec[i].ruta, ruta)) return i;
	return -1;
}

const char *esc_base_hijos(const EscDoc *d, int sec) { return !strcmp(d->sec[sec].ruta, ".") ? "" : d->sec[sec].ruta; }

static int con_prefijo(const char *r, const char *base) {   /* r == base o r empieza por base + "/" */
	size_t l = strlen(base);
	return !strncmp(r, base, l) && (r[l] == 0 || r[l] == '/');
}

/* ¿la sección i forma parte del subárbol de la sección s? (la propia s incluida; las capas de un mapa miembro también) */
static int es_miembro(const EscDoc *d, int i, int s) {
	const EscSec *x = &d->sec[i];
	if (x->clase == ES_CAPA) return x->mapa >= 0 && es_miembro(d, x->mapa, s);
	if (x->clase != ES_NODO && x->clase != ES_INSTANCIA) return 0;
	if (!strcmp(d->sec[s].ruta, ".")) return 1;
	return con_prefijo(x->ruta, d->sec[s].ruta);
}
int esc_es_descendiente(const EscDoc *d, int sec, int de) { return sec != de && es_miembro(d, sec, de); }

static int fin_subarbol(const EscDoc *d, int s) {
	int fin = d->sec[s].fin;
	for (int i = 0; i < d->nsec; ++i) if (es_miembro(d, i, s) && d->sec[i].fin > fin) fin = d->sec[i].fin;
	return fin;
}

int esc_buscar_nombre_libre(const EscDoc *d, const char *base_padre, const char *base, char *out, size_t no) {
	char raiz[CAPI_NOME_MAX]; snprintf(raiz, sizeof raiz, "%s", base);
	size_t l = strlen(raiz); while (l && isdigit((unsigned char)raiz[l - 1])) raiz[--l] = 0;
	if (!l) snprintf(raiz, sizeof raiz, "%s", base);
	for (int k = 1;; ++k) {
		if (k == 1) snprintf(out, no, "%s", base); else snprintf(out, no, "%.40s%d", raiz, k);
		char ruta[CAPI_RUTA_MAX]; snprintf(ruta, sizeof ruta, "%s%s%s", base_padre, *base_padre ? "/" : "", out);
		if (esc_buscar_ruta(d, ruta) < 0) return 1;
		if (k > 9999) return 0;
	}
}

/* ---------------------------------------------------------------------------------------------------- cabeceras */
typedef struct { int ini, fin; } Rango;
typedef struct { char nombre[32]; int ini_nombre, ini_val, fin_val; char val[CAPI_RUTA_MAX]; } AttrPos;

/* recorre la cabecera: tokens sueltos (posicionales) y atributos con su sitio en la línea */
static int cab_recorrer(const char *h, Rango *pos, int maxpos, AttrPos *at, int maxat, int *cierre) {
	const char *a = strchr(h, '['), *z = strrchr(h, ']');
	int np = 0, na = 0;
	if (!a || !z) return 0;
	if (cierre) *cierre = (int)(z - h);
	const char *q = a + 1;
	while (q < z) {
		while (q < z && (*q == ' ' || *q == '\t')) ++q;
		if (q >= z) break;
		const char *ini = q; char w[96]; size_t k = 0; int comilla = 0;
		if (*q == '"') { comilla = 1; ++q; while (q < z && *q != '"') { if (k < sizeof w - 1) w[k++] = *q; ++q; } if (q < z) ++q; w[k] = 0; }
		else { while (q < z && *q != ' ' && *q != '\t' && *q != '=') { if (k < sizeof w - 1) w[k++] = *q; ++q; } w[k] = 0; }
		if (!comilla && q < z && *q == '=') {
			++q;
			const char *vi = q; char v[CAPI_RUTA_MAX]; size_t j = 0;
			if (*q == '"') { ++q; while (q < z && *q != '"') { if (j < sizeof v - 1) v[j++] = *q; ++q; } if (q < z) ++q; }
			else while (q < z && *q != ' ' && *q != '\t') { if (j < sizeof v - 1) v[j++] = *q; ++q; }
			v[j] = 0;
			if (na < maxat) { AttrPos *p = &at[na++]; snprintf(p->nombre, sizeof p->nombre, "%s", w); p->ini_nombre = (int)(ini - h); p->ini_val = (int)(vi - h); p->fin_val = (int)(q - h); snprintf(p->val, sizeof p->val, "%s", v); }
		} else if (np < maxpos) { pos[np].ini = (int)(ini - h); pos[np].fin = (int)(q - h); ++np; }
	}
	return np | (na << 8);
}

static char *reemplazar(const char *s, int a, int b, const char *nuevo) {   /* s[a,b) -> nuevo (malloc) */
	size_t l = strlen(s), ln = strlen(nuevo);
	char *r = malloc(l - (size_t)(b - a) + ln + 1);
	memcpy(r, s, (size_t)a); memcpy(r + a, nuevo, ln); strcpy(r + a + ln, s + b);
	return r;
}

static void cod_valor(const char *v, char *out, size_t no) {
	int raro = !*v; for (const char *c = v; *c; ++c) if (*c == ' ' || *c == '\t' || *c == '=' || *c == ']' || *c == '"') raro = 1;
	if (raro) snprintf(out, no, "\"%s\"", v); else snprintf(out, no, "%s", v);
}

int esc_attr_valor(const EscDoc *d, int sec, const char *k, char *out, size_t no) {
	if (sec < 0 || sec >= d->nsec) return 0;
	Rango pos[8]; AttrPos at[16];
	int r = cab_recorrer(d->lin[d->sec[sec].ini], pos, 8, at, 16, NULL), na = r >> 8;
	for (int i = 0; i < na; ++i) if (!strcmp(at[i].nombre, k)) { snprintf(out, no, "%s", at[i].val); return 1; }
	return 0;
}

static int attr_poner_linea(EscDoc *d, int linea, const char *k, const char *v) {
	Rango pos[8]; AttrPos at[16]; int cierre = 0;
	char *h = d->lin[linea];
	int r = cab_recorrer(h, pos, 8, at, 16, &cierre), na = r >> 8;
	char cod[CAPI_RUTA_MAX + 4]; cod_valor(v, cod, sizeof cod);
	for (int i = 0; i < na; ++i) if (!strcmp(at[i].nombre, k)) {
		char *n;
		if (!*v) { int a = at[i].ini_nombre; while (a > 0 && (h[a - 1] == ' ' || h[a - 1] == '\t')) --a; n = reemplazar(h, a, at[i].fin_val, ""); }
		else n = reemplazar(h, at[i].ini_val, at[i].fin_val, cod);
		free(d->lin[linea]); d->lin[linea] = n; return 1;
	}
	if (!*v) return 1;
	char ins[CAPI_RUTA_MAX + 48]; snprintf(ins, sizeof ins, " %s=%s", k, cod);
	char *n = reemplazar(h, cierre, cierre, ins);
	free(d->lin[linea]); d->lin[linea] = n;
	return 1;
}

int esc_attr_poner(EscDoc *d, int sec, const char *k, const char *v) {
	if (sec < 0 || sec >= d->nsec) return 0;
	attr_poner_linea(d, d->sec[sec].ini, k, v);
	esc_analizar(d);
	return 1;
}

static int cab_nombre_poner(EscDoc *d, int linea, const char *nombre) {
	Rango pos[8]; AttrPos at[16];
	char *h = d->lin[linea];
	int r = cab_recorrer(h, pos, 8, at, 16, NULL), np = r & 255;
	char cod[CAPI_NOME_MAX + 4]; cod_valor(nombre, cod, sizeof cod);
	char *n;
	if (np >= 2) n = reemplazar(h, pos[1].ini, pos[1].fin, cod);
	else if (np == 1) { char t[CAPI_NOME_MAX + 8]; snprintf(t, sizeof t, " %s", cod); n = reemplazar(h, pos[0].fin, pos[0].fin, t); }
	else return 0;
	free(d->lin[linea]); d->lin[linea] = n;
	return 1;
}

/* ---------------------------------------------------------------------------------------------------- propiedades */
static int buscar_prop(const EscDoc *d, int sec, const char *clave) {
	const EscSec *s = &d->sec[sec];
	if (s->clase == ES_CAPA) return -1;
	size_t kl0 = strlen(clave);
	for (int i = s->ini + 1; i < s->fin; ++i) {
		const char *l = d->lin[i];
		if (es_vacia_o_comentario(l)) continue;
		const char *eq = igual_en(l); if (!eq) continue;
		const char *k = l; while (*k == ' ' || *k == '\t') ++k;
		size_t kl = (size_t)(eq - k); while (kl && (k[kl - 1] == ' ' || k[kl - 1] == '\t')) --kl;
		if (kl == kl0 && !strncmp(k, clave, kl)) return i;
	}
	return -1;
}

static void valor_linea(const char *l, int *ini, int *fin) {
	const char *eq = igual_en(l);
	const char *v = eq + 1; while (*v == ' ' || *v == '\t') ++v;
	const char *c = comentario_en(v);
	const char *e = c ? c : v + strlen(v);
	while (e > v && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) --e;
	*ini = (int)(v - l); *fin = (int)(e - l);
}

int esc_prop_valor(const EscDoc *d, int sec, const char *clave, char *out, size_t no) {
	if (sec < 0 || sec >= d->nsec) return 0;
	int i = buscar_prop(d, sec, clave);
	if (i < 0) return 0;
	int a, b; valor_linea(d->lin[i], &a, &b);
	snprintf(out, no, "%.*s", b - a, d->lin[i] + a);
	return 1;
}

int esc_prop_poner(EscDoc *d, int sec, const char *clave, const char *valor) {
	if (sec < 0 || sec >= d->nsec || d->sec[sec].clase == ES_CAPA || d->sec[sec].clase == ES_CONEXION) return 0;
	int i = buscar_prop(d, sec, clave);
	if (i >= 0) {
		char *l = d->lin[i]; int a, b; valor_linea(l, &a, &b);
		/* ancho del hueco antes del valor: si el valor estaba pegado al «=», sigue pegado */
		char *n = reemplazar(l, a, b, valor);
		free(d->lin[i]); d->lin[i] = n;
	} else {
		char t[CAPI_RUTA_MAX + 700]; snprintf(t, sizeof t, "%s = %s", clave, valor);
		ins_linea(d, d->sec[sec].fin, t);
	}
	esc_analizar(d);
	return 1;
}

int esc_prop_quitar(EscDoc *d, int sec, const char *clave) {
	if (sec < 0 || sec >= d->nsec) return 0;
	int i = buscar_prop(d, sec, clave);
	if (i < 0) return 0;
	del_lineas(d, i, i + 1);
	esc_analizar(d);
	return 1;
}

/* ---------------------------------------------------------------------------------------------------- nodos */
static void base_de(const EscDoc *d, const char *padre_ruta, char *base, size_t no) {
	(void)d;
	if (!padre_ruta || !strcmp(padre_ruta, ".")) base[0] = 0; else snprintf(base, no, "%s", padre_ruta);
}

/* dónde se inserta un nodo nuevo: antes de la sección hermana, o al final del subárbol del padre */
static int posicion_insercion(const EscDoc *d, const char *padre_ruta, int antes) {
	if (antes >= 0 && antes < d->nsec) return d->sec[antes].ant;
	int p = esc_buscar_ruta(d, padre_ruta && *padre_ruta ? padre_ruta : ".");
	if (p < 0) {
		int fin = 0;
		for (int i = 0; i < d->nsec; ++i) if (d->sec[i].fin > fin) fin = d->sec[i].fin;
		return fin;
	}
	return fin_subarbol(d, p);
}

static int insertar_cabecera(EscDoc *d, int pos, const char *cab) {
	/* si el documento separa con líneas en blanco, la nueva sección también */
	int sep = pos < d->n && es_vacia(d->lin[pos]);
	if (sep) { ins_linea(d, pos, ""); ++pos; }
	ins_linea(d, pos, cab);
	return pos;
}

int esc_nodo_nuevo(EscDoc *d, const char *tipo, const char *nombre, const char *padre_ruta, int antes) {
	char base[CAPI_RUTA_MAX]; base_de(d, padre_ruta, base, sizeof base);
	char nom[CAPI_NOME_MAX]; esc_buscar_nombre_libre(d, base, nombre, nom, sizeof nom);
	char cab[CAPI_RUTA_MAX + 96];
	int es_primero = esc_buscar_ruta(d, ".") < 0;
	if (base[0] && !es_primero) { char cod[CAPI_RUTA_MAX + 4]; cod_valor(base, cod, sizeof cod); snprintf(cab, sizeof cab, "[%s %s padre=%s]", tipo, nom, cod); }
	else snprintf(cab, sizeof cab, "[%s %s]", tipo, nom);
	int pos = posicion_insercion(d, padre_ruta, antes);
	insertar_cabecera(d, pos, cab);
	esc_analizar(d);
	char ruta[CAPI_RUTA_MAX]; snprintf(ruta, sizeof ruta, "%s%s%s", base, base[0] ? "/" : "", nom);
	return esc_buscar_ruta(d, es_primero ? "." : ruta);
}

int esc_instancia_nueva(EscDoc *d, const char *escena, const char *nombre, const char *padre_ruta, int antes) {
	char base[CAPI_RUTA_MAX]; base_de(d, padre_ruta, base, sizeof base);
	char nom[CAPI_NOME_MAX]; esc_buscar_nombre_libre(d, base, nombre, nom, sizeof nom);
	char cab[CAPI_RUTA_MAX * 2 + 96], ce[CAPI_RUTA_MAX + 4];
	cod_valor(escena, ce, sizeof ce);
	if (base[0]) { char cod[CAPI_RUTA_MAX + 4]; cod_valor(base, cod, sizeof cod); snprintf(cab, sizeof cab, "[Instancia %s escena=%s padre=%s]", nom, ce, cod); }
	else snprintf(cab, sizeof cab, "[Instancia %s escena=%s]", nom, ce);
	int pos = posicion_insercion(d, padre_ruta, antes);
	insertar_cabecera(d, pos, cab);
	esc_analizar(d);
	char ruta[CAPI_RUTA_MAX]; snprintf(ruta, sizeof ruta, "%s%s%s", base, base[0] ? "/" : "", nom);
	return esc_buscar_ruta(d, ruta);
}

/* cambia el prefijo de ruta «viejo» por «nuevo» en padres, conexiones y «seguir» de las cámaras */
static void reescribir_prefijo(EscDoc *d, const char *viejo, const char *nuevo) {
	size_t lv = strlen(viejo);
	for (int i = 0; i < d->nsec; ++i) {
		EscSec *s = &d->sec[i]; char v[CAPI_RUTA_MAX], t[CAPI_RUTA_MAX];
		if ((s->clase == ES_NODO || s->clase == ES_INSTANCIA) && s->padre[0] && con_prefijo(s->padre, viejo)) {
			snprintf(t, sizeof t, "%s%s", nuevo, s->padre + lv);
			attr_poner_linea(d, s->ini, "padre", t);
		}
		if (s->clase == ES_CONEXION) {
			static const char *const K[] = { "desde", "hacia" };
			for (int k = 0; k < 2; ++k) if (esc_attr_valor(d, i, K[k], v, sizeof v) && con_prefijo(v, viejo)) { snprintf(t, sizeof t, "%s%s", nuevo, v + lv); attr_poner_linea(d, s->ini, K[k], t); }
		}
		if (s->clase == ES_NODO && !strcmp(s->tipo, "Camara2D") && esc_prop_valor(d, i, "seguir", v, sizeof v)) {
			char u[CAPI_RUTA_MAX]; snprintf(u, sizeof u, "%s", v[0] == '"' ? v + 1 : v);
			size_t l = strlen(u); if (l && u[l - 1] == '"') u[--l] = 0;
			if (con_prefijo(u, viejo)) {
				snprintf(t, sizeof t, "\"%s%s\"", nuevo, u + lv);
				int li = buscar_prop(d, i, "seguir"); int a, b; valor_linea(d->lin[li], &a, &b);
				char *n = reemplazar(d->lin[li], a, b, t); free(d->lin[li]); d->lin[li] = n;
			}
		}
	}
}

int esc_nodo_renombrar(EscDoc *d, int sec, const char *nombre) {
	if (sec < 0 || sec >= d->nsec || (d->sec[sec].clase != ES_NODO && d->sec[sec].clase != ES_INSTANCIA)) return -1;
	EscSec s = d->sec[sec];
	if (!strcmp(s.nombre, nombre)) return sec;
	char base[CAPI_RUTA_MAX]; snprintf(base, sizeof base, "%s", s.padre);
	char libre[CAPI_NOME_MAX];
	if (!strcmp(s.ruta, ".")) snprintf(libre, sizeof libre, "%s", nombre);
	else { char r2[CAPI_RUTA_MAX]; snprintf(r2, sizeof r2, "%s%s%s", base, base[0] ? "/" : "", nombre); if (esc_buscar_ruta(d, r2) >= 0) return -1; snprintf(libre, sizeof libre, "%s", nombre); }
	char viejo[CAPI_RUTA_MAX], nuevo[CAPI_RUTA_MAX];
	snprintf(viejo, sizeof viejo, "%s", s.ruta);
	if (!strcmp(s.ruta, ".")) snprintf(nuevo, sizeof nuevo, "."); else snprintf(nuevo, sizeof nuevo, "%s%s%s", base, base[0] ? "/" : "", libre);
	cab_nombre_poner(d, s.ini, libre);
	if (strcmp(viejo, ".")) reescribir_prefijo(d, viejo, nuevo);
	esc_analizar(d);
	return esc_buscar_ruta(d, nuevo);
}

/* copia las líneas de las secciones del subárbol (en orden de fichero) a una lista */
typedef struct { char **lin; int n, cap; } Lineas;
static void lineas_juntar(Lineas *L, const char *s) {
	if (L->n == L->cap) { L->cap = L->cap ? L->cap * 2 : 64; L->lin = realloc(L->lin, sizeof(char *) * (size_t)L->cap); }
	L->lin[L->n++] = dup(s);
}
static void lineas_liberar(Lineas *L) { for (int i = 0; i < L->n; ++i) free(L->lin[i]); free(L->lin); memset(L, 0, sizeof *L); }

static int extraer_subarbol(const EscDoc *d, int s, Lineas *L, int *rangos, int *nr) {
	int k = 0;
	for (int i = 0; i < d->nsec; ++i) if (es_miembro(d, i, s)) {
		for (int l = d->sec[i].ant; l < d->sec[i].fin; ++l) lineas_juntar(L, d->lin[l]);
		if (rangos) { rangos[2 * k] = d->sec[i].ant; rangos[2 * k + 1] = d->sec[i].fin; }
		++k;
	}
	if (nr) *nr = k;
	return k;
}

int esc_nodo_borrar(EscDoc *d, int sec) {
	if (sec < 0 || sec >= d->nsec || (d->sec[sec].clase != ES_NODO && d->sec[sec].clase != ES_INSTANCIA) || !strcmp(d->sec[sec].ruta, ".")) return 0;
	enum { MAXR = 512 };
	static int rg[2 * MAXR]; int n = 0;
	char ruta[CAPI_RUTA_MAX]; snprintf(ruta, sizeof ruta, "%s", d->sec[sec].ruta);
	for (int i = 0; i < d->nsec && n < MAXR; ++i) {
		int borrar = es_miembro(d, i, sec);
		if (!borrar && d->sec[i].clase == ES_CONEXION) {
			char v[CAPI_RUTA_MAX];
			if ((esc_attr_valor(d, i, "desde", v, sizeof v) && con_prefijo(v, ruta)) || (esc_attr_valor(d, i, "hacia", v, sizeof v) && con_prefijo(v, ruta))) borrar = 1;
		}
		if (borrar) { rg[2 * n] = d->sec[i].ant; rg[2 * n + 1] = d->sec[i].fin; ++n; }
	}
	/* de las últimas a las primeras para no mover las que quedan */
	for (int i = 0; i < n; ++i) for (int j = i + 1; j < n; ++j) if (rg[2 * j] > rg[2 * i]) { int a = rg[2 * i], b = rg[2 * i + 1]; rg[2 * i] = rg[2 * j]; rg[2 * i + 1] = rg[2 * j + 1]; rg[2 * j] = a; rg[2 * j + 1] = b; }
	for (int i = 0; i < n; ++i) del_lineas(d, rg[2 * i], rg[2 * i + 1]);
	esc_analizar(d);
	return 1;
}

/* pone el subárbol copiado (lineas) en la posición pos con la raíz en padre_nuevo/nombre_nuevo, partiendo de la ruta vieja */
static void recolocar_cabeceras(Lineas *L, const char *ruta_vieja, const char *base_nueva, const char *nombre_nuevo) {
	/* el análisis se hace sobre un documento auxiliar con solo esas líneas */
	EscDoc aux = { 0 }; aux.lin = L->lin; aux.n = L->n; aux.cap = L->cap; aux.nl_final = 1;
	esc_analizar(&aux);
	char nueva[CAPI_RUTA_MAX]; snprintf(nueva, sizeof nueva, "%s%s%s", base_nueva, *base_nueva ? "/" : "", nombre_nuevo);
	size_t lv = strlen(ruta_vieja);
	for (int i = 0; i < aux.nsec; ++i) {
		EscSec *s = &aux.sec[i];
		if (s->clase != ES_NODO && s->clase != ES_INSTANCIA) continue;
		if (i == 0 || !strcmp(s->ruta, ruta_vieja)) {   /* la raíz del subárbol (la primera sección) */
			if (strcmp(s->nombre, nombre_nuevo)) cab_nombre_poner(&aux, s->ini, nombre_nuevo);
			attr_poner_linea(&aux, s->ini, "padre", base_nueva);
		} else {
			char t[CAPI_RUTA_MAX]; snprintf(t, sizeof t, "%s%s", nueva, s->padre + lv);
			attr_poner_linea(&aux, s->ini, "padre", t);
		}
	}
	L->lin = aux.lin; L->n = aux.n; L->cap = aux.cap;
	free(aux.sec);
}

int esc_nodo_duplicar(EscDoc *d, int sec) {
	if (sec < 0 || sec >= d->nsec || (d->sec[sec].clase != ES_NODO && d->sec[sec].clase != ES_INSTANCIA) || !strcmp(d->sec[sec].ruta, ".")) return -1;
	EscSec s = d->sec[sec];
	Lineas L = { 0 };
	extraer_subarbol(d, sec, &L, NULL, NULL);
	char nom[CAPI_NOME_MAX]; esc_buscar_nombre_libre(d, s.padre, s.nombre, nom, sizeof nom);
	if (!strcmp(nom, s.nombre)) snprintf(nom, sizeof nom, "%.40s2", s.nombre);
	/* la primera sección de L puede llevar comentarios delante: las cabeceras se recolocan sobre el análisis de L */
	recolocar_cabeceras(&L, s.ruta, s.padre, nom);
	int pos = fin_subarbol(d, sec);
	int sep = pos < d->n && es_vacia(d->lin[pos]);
	if (sep) { ins_linea(d, pos, ""); ++pos; }
	ins_lineas(d, pos, L.lin, L.n);
	lineas_liberar(&L);
	esc_analizar(d);
	char ruta[CAPI_RUTA_MAX]; snprintf(ruta, sizeof ruta, "%s%s%s", s.padre, s.padre[0] ? "/" : "", nom);
	return esc_buscar_ruta(d, ruta);
}

int esc_nodo_mover(EscDoc *d, int sec, const char *padre_ruta, int antes) {
	if (sec < 0 || sec >= d->nsec || (d->sec[sec].clase != ES_NODO && d->sec[sec].clase != ES_INSTANCIA) || !strcmp(d->sec[sec].ruta, ".")) return -1;
	EscSec s = d->sec[sec];
	char base[CAPI_RUTA_MAX]; base_de(d, padre_ruta, base, sizeof base);
	if (con_prefijo(base, s.ruta)) return -1;                         /* dentro de sí mismo */
	if (antes >= 0 && (antes == sec || es_miembro(d, antes, sec))) return -1;
	char nom[CAPI_NOME_MAX]; snprintf(nom, sizeof nom, "%s", s.nombre);
	char ruta_nueva[CAPI_RUTA_MAX]; snprintf(ruta_nueva, sizeof ruta_nueva, "%s%s%s", base, base[0] ? "/" : "", nom);
	if (strcmp(base, s.padre)) {   /* otro padre: el nombre no puede repetirse entre los nuevos hermanos */
		int e = esc_buscar_ruta(d, ruta_nueva); if (e >= 0 && e != sec) esc_buscar_nombre_libre(d, base, s.nombre, nom, sizeof nom);
		snprintf(ruta_nueva, sizeof ruta_nueva, "%s%s%s", base, base[0] ? "/" : "", nom);
	}
	char antes_ruta[CAPI_RUTA_MAX] = ""; int tiene_antes = antes >= 0;
	if (tiene_antes) snprintf(antes_ruta, sizeof antes_ruta, "%s", d->sec[antes].ruta);
	char viejo[CAPI_RUTA_MAX]; snprintf(viejo, sizeof viejo, "%s", s.ruta);
	char padre_resuelto[CAPI_RUTA_MAX]; snprintf(padre_resuelto, sizeof padre_resuelto, "%s", base[0] ? base : ".");
	Lineas L = { 0 }; static int rg[2 * 512]; int nr = 0;
	extraer_subarbol(d, sec, &L, rg, &nr);
	recolocar_cabeceras(&L, viejo, base, nom);
	for (int i = nr - 1; i >= 0; --i) del_lineas(d, rg[2 * i], rg[2 * i + 1]);
	esc_analizar(d);
	int pos;
	if (tiene_antes) { int a = esc_buscar_ruta(d, antes_ruta); pos = a >= 0 ? d->sec[a].ant : posicion_insercion(d, padre_resuelto, -1); }
	else pos = posicion_insercion(d, padre_resuelto, -1);
	int sep = pos < d->n && es_vacia(d->lin[pos]);
	if (sep) { ins_linea(d, pos, ""); ++pos; }
	ins_lineas(d, pos, L.lin, L.n);
	lineas_liberar(&L);
	esc_analizar(d);
	if (strcmp(viejo, ruta_nueva)) { reescribir_prefijo(d, viejo, ruta_nueva); esc_analizar(d); }
	return esc_buscar_ruta(d, ruta_nueva);
}

/* ---------------------------------------------------------------------------------------------------- conexiones */
int esc_conexion_nueva(EscDoc *d, const char *desde, const char *senal, const char *hacia, const char *metodo) {
	char a[CAPI_RUTA_MAX + 4], b[CAPI_RUTA_MAX + 4], c[64], e[64], cab[CAPI_RUTA_MAX * 2 + 200];
	cod_valor(desde, a, sizeof a); cod_valor(hacia, b, sizeof b); cod_valor(senal, c, sizeof c); cod_valor(metodo, e, sizeof e);
	snprintf(cab, sizeof cab, "[Conexion desde=%s senal=%s hacia=%s metodo=%s]", a, c, b, e);
	int pos = d->n;
	if (pos && !es_vacia(d->lin[pos - 1])) ins_linea(d, pos++, "");
	ins_linea(d, pos, cab);
	esc_analizar(d);
	return d->nsec - 1;
}

int esc_conexion_borrar(EscDoc *d, int sec) {
	if (sec < 0 || sec >= d->nsec || d->sec[sec].clase != ES_CONEXION) return 0;
	del_lineas(d, d->sec[sec].ant, d->sec[sec].fin);
	esc_analizar(d);
	return 1;
}

/* ---------------------------------------------------------------------------------------------------- casillas */
int esc_capas(const EscDoc *d, int mapa, int *secs, int max) {
	int n = 0;
	for (int i = 0; i < d->nsec; ++i) if (d->sec[i].clase == ES_CAPA && d->sec[i].mapa == mapa) { if (n < max) secs[n] = i; ++n; }
	return n < max ? n : max;
}

typedef struct { int a, b; } Trozo;
static int trozos_de(const char *s, Trozo *t, int max) {
	int n = 0;
	for (const char *p = s; *p;) {
		while (*p == ' ' || *p == '\t' || *p == ',') ++p;
		if (!*p) break;
		const char *a = p; while (*p && *p != ' ' && *p != '\t' && *p != ',') ++p;
		if (n < max) { t[n].a = (int)(a - s); t[n].b = (int)(p - s); }
		++n;
	}
	return n < max ? n : max;
}

static int fila_linea(const EscDoc *d, int capa, int fila) {
	int k = 0;
	for (int i = d->sec[capa].ini + 1; i < d->sec[capa].fin; ++i) { if (es_vacia_o_comentario(d->lin[i])) continue; if (k++ == fila) return i; }
	return -1;
}

int esc_celda_leer(const EscDoc *d, int capa, int cx, int cy) {
	int l = fila_linea(d, capa, cy);
	if (l < 0) return -1;
	Trozo t[1024]; int n = trozos_de(d->lin[l], t, 1024);
	if (cx < 0 || cx >= n) return -1;
	const char *s = d->lin[l] + t[cx].a;
	if (*s == '.' || *s == '-') return -1;
	return atoi(s);
}

int esc_celda_poner(EscDoc *d, int capa, int cx, int cy, int v, int ancho, int alto) {
	if (capa < 0 || capa >= d->nsec || d->sec[capa].clase != ES_CAPA || cx < 0 || cy < 0 || cx >= ancho || cy >= alto) return 0;
	int filas = 0;
	for (int i = d->sec[capa].ini + 1; i < d->sec[capa].fin; ++i) if (!es_vacia_o_comentario(d->lin[i])) ++filas;
	if (filas <= cy) {   /* faltan filas: se crean vacías */
		char *r = malloc((size_t)ancho * 2 + 2); for (int i = 0; i < ancho; ++i) { r[2 * i] = '.'; r[2 * i + 1] = ' '; } r[2 * ancho - 1] = 0;
		int pos = d->sec[capa].fin;
		for (int k = filas; k <= cy; ++k) ins_linea(d, pos++, r);
		free(r);
		esc_analizar(d);
	}
	int l = fila_linea(d, capa, cy);
	if (l < 0) return 0;
	char val[16]; if (v < 0) snprintf(val, sizeof val, "."); else snprintf(val, sizeof val, "%d", v);
	Trozo t[1024]; int n = trozos_de(d->lin[l], t, 1024);
	char *nl;
	if (cx < n) nl = reemplazar(d->lin[l], t[cx].a, t[cx].b, val);
	else {
		size_t len = strlen(d->lin[l]); char *pad = malloc((size_t)(cx - n) * 2 + 16); size_t k = 0;
		for (int i = n; i < cx; ++i) { pad[k++] = ' '; pad[k++] = '.'; }
		pad[k++] = ' '; strcpy(pad + k, val);
		nl = reemplazar(d->lin[l], (int)len, (int)len, pad); free(pad);
	}
	free(d->lin[l]); d->lin[l] = nl;
	return 1;
}

int esc_capa_nueva(EscDoc *d, int mapa, const char *nombre, int solida, int ancho, int alto) {
	if (mapa < 0 || mapa >= d->nsec) return -1;
	int pos = d->sec[mapa].fin;
	for (int i = 0; i < d->nsec; ++i) if (d->sec[i].clase == ES_CAPA && d->sec[i].mapa == mapa && d->sec[i].fin > pos) pos = d->sec[i].fin;
	char cab[96]; snprintf(cab, sizeof cab, "[capa %s solida=%s]", nombre, solida ? "si" : "no");
	int sep = pos < d->n && es_vacia(d->lin[pos]);
	if (sep) { ins_linea(d, pos, ""); ++pos; }
	ins_linea(d, pos++, cab);
	char *r = malloc((size_t)ancho * 2 + 2); for (int i = 0; i < ancho; ++i) { r[2 * i] = '.'; r[2 * i + 1] = ' '; } r[ancho > 0 ? 2 * ancho - 1 : 0] = 0;
	for (int k = 0; k < alto; ++k) ins_linea(d, pos++, r);
	free(r);
	esc_analizar(d);
	int n = 0, ult = -1; for (int i = 0; i < d->nsec; ++i) if (d->sec[i].clase == ES_CAPA && d->sec[i].mapa == mapa) { ult = i; ++n; }
	return ult;
}
