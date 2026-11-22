/* SPDX-License-Identifier: MIT */
/* escena.c - carga de escenas (.esc) y del proyecto (capibara.cfg). Una escena es un árbol de nodos en texto; puede
 * contener instancias de otras escenas. Las rutas de los ficheros son siempre relativas a la raíz del proyecto. */
#include "capi.h"

Proyecto g_proy = { "Capibara", "", 320, 180, 1, { 32, 32, 48, 255 }, 900, 60, 1234, 0 };

typedef struct { char desde[96], senal[32], hacia[96], metodo[32]; } ConexPend;
typedef struct {
	Nodo *raiz, *cur; Nodo **creados; int nc, cc;
	ConexPend *cx; int ncx, ccx;
	DMapa *mapa; int capa; int fila;
	const char *nombre; int prof; int errores; int en_capa;
} Est;

static const char *attr(const Cab *c, const char *k) { for (int i = 0; i < c->na; ++i) if (!strcmp(c->an[i], k)) return c->av[i]; return NULL; }
static int attr_bool(const Cab *c, const char *k, int def) {
	const char *v = attr(c, k);
	if (!v) return def;
	return !strcmp(v, "si") || !strcmp(v, "verdadero") || !strcmp(v, "true") || !strcmp(v, "1");
}

static void anotar(Est *e, Nodo *n) {
	if (e->nc == e->cc) { e->cc = e->cc ? e->cc * 2 : 32; e->creados = realloc(e->creados, sizeof(Nodo *) * (size_t)e->cc); }
	e->creados[e->nc++] = n;
}

static int es_seccion(void *u, const Cab *c, int linea) {
	Est *e = u;
	if (!c->nt) return 0;
	const char *t0 = c->t[0];
	e->en_capa = 0;
	if (!strcmp(t0, "Escena")) return 0;
	if (!strcmp(t0, "capa") || !strcmp(t0, "Capa")) {
		if (!e->cur || e->cur->tipo != T_MAPA) { capi_log("%s:%d: [capa] fuera de un MapaCasillas", e->nombre, linea); ++e->errores; return 0; }
		DMapa *m = e->cur->d;
		if (m->ncapas >= 6 || m->w <= 0) { capi_log("%s:%d: capa no válida (¿falta «tamano» o hay más de 6 capas?)", e->nombre, linea); ++e->errores; return 0; }
		Capa *k = &m->capas[m->ncapas]; memset(k, 0, sizeof *k);
		snprintf(k->nome, sizeof k->nome, "%s", c->nt > 1 ? c->t[1] : "capa");
		k->visible = attr_bool(c, "visible", 1); k->solida = attr_bool(c, "solida", 0);
		k->trozos = calloc((size_t)(m->tw * m->th), sizeof(int16_t *));
		e->mapa = m; e->capa = m->ncapas++; e->fila = 0; e->en_capa = 1;
		return 0;
	}
	if (!strcmp(t0, "Conexion") || !strcmp(t0, "conexion")) {
		if (e->ncx == e->ccx) { e->ccx = e->ccx ? e->ccx * 2 : 8; e->cx = realloc(e->cx, sizeof(ConexPend) * (size_t)e->ccx); }
		ConexPend *p = &e->cx[e->ncx++]; memset(p, 0, sizeof *p);
		snprintf(p->desde, sizeof p->desde, "%s", attr(c, "desde") ? attr(c, "desde") : ".");
		snprintf(p->senal, sizeof p->senal, "%s", attr(c, "senal") ? attr(c, "senal") : "");
		snprintf(p->hacia, sizeof p->hacia, "%s", attr(c, "hacia") ? attr(c, "hacia") : ".");
		snprintf(p->metodo, sizeof p->metodo, "%s", attr(c, "metodo") ? attr(c, "metodo") : "");
		e->cur = NULL;
		return 0;
	}
	Nodo *n = NULL;
	const char *nombre = c->nt > 1 ? c->t[1] : NULL;
	if (!strcmp(t0, "Instancia")) {
		const char *ruta = attr(c, "escena");
		char err[160] = "";
		if (!ruta || e->prof >= 16) { capi_log("%s:%d: instancia sin «escena» o demasiado anidada", e->nombre, linea); ++e->errores; e->cur = NULL; return 0; }
		n = escena_instanciar(ruta, err, sizeof err);
		if (!n) { capi_log("%s:%d: no se pudo instanciar %s: %s", e->nombre, linea, ruta, err); ++e->errores; e->cur = NULL; return 0; }
		if (nombre) snprintf(n->nombre, sizeof n->nombre, "%s", nombre);
	} else {
		int t = capi_tipo_por_nombre(t0);
		if (t < 0) { capi_log("%s:%d: tipo de nodo desconocido «%s»", e->nombre, linea, t0); ++e->errores; e->cur = NULL; return 0; }
		n = nodo_nuevo((CapiTipo)t, nombre);
	}
	const char *padre = attr(c, "padre");
	if (!e->raiz) e->raiz = n;
	else {
		Nodo *p = padre ? nodo_buscar(e->raiz, padre) : e->raiz;
		if (!p) { capi_log("%s:%d: padre «%s» no encontrado (debe ir antes en el fichero)", e->nombre, linea, padre); ++e->errores; p = e->raiz; }
		nodo_agregar(p, n);
	}
	anotar(e, n);
	e->cur = n;
	return 0;
}

static int es_prop(void *u, const char *k, const Valor *v, int linea) {
	Est *e = u;
	if (!e->cur) return 0;
	if (e->en_capa) return 0;
	if (!strcmp(k, "guion")) {
		if (e->cur->ref_clase != LUA_NOREF) guion_dato_liberar(e->cur);
		snprintf(e->cur->guion, sizeof e->cur->guion, "%s", v->n ? v->t[0].s : "");
		return 0;
	}
	if (!strncmp(k, "var.", 4)) { guion_poner_dato(e->cur, k + 4, v); return 0; }   /* variable propia del nodo (self.nombre en Lua) */
	if (!prop_poner(e->cur, k, v)) {
		capi_log("%s:%d: propiedad «%s» desconocida o con un valor no válido para %s", e->nombre, linea, k, CAPI_TIPOS[e->cur->tipo]);
		++e->errores;
	}
	return 0;
}

static int es_fila(void *u, const char *s, int linea) {
	Est *e = u;
	if (!e->en_capa || !e->mapa) { capi_log("%s:%d: línea sin sentido: %s", e->nombre, linea, s); ++e->errores; return 0; }
	int x = 0;
	while (*s) {
		while (*s == ' ' || *s == '\t' || *s == ',') ++s;
		if (!*s) break;
		if (*s == '.' || *s == '-') { ++x; while (*s && *s != ' ' && *s != '\t' && *s != ',') ++s; continue; }
		char *f; long v = strtol(s, &f, 10);
		if (f == s) { ++s; continue; }
		mapa_poner(e->cur, e->capa, x, e->fila, (int)v); ++x; s = f;
	}
	++e->fila;
	return 0;
}

static void finalizar(Est *e) {
	/* conexiones declaradas en el fichero */
	for (int i = 0; i < e->ncx; ++i) {
		ConexPend *p = &e->cx[i];
		Nodo *a = nodo_buscar(e->raiz, p->desde), *b = nodo_buscar(e->raiz, p->hacia);
		if (!a || !b || !*p->senal || !*p->metodo) { capi_log("%s: conexión no válida (%s.%s -> %s.%s)", e->nombre, p->desde, p->senal, p->hacia, p->metodo); ++e->errores; continue; }
		sen_conectar_nodo(a, p->senal, b, p->metodo);
	}
	/* guiones: se cargan con el árbol ya completo, para que obtener_nodo funcione al ejecutarlos */
	for (int i = 0; i < e->nc; ++i) {
		Nodo *n = e->creados[i];
		if (n->guion[0] && n->ref_clase == LUA_NOREF && guion_cargar(n, n->guion) != 0) ++e->errores;
	}
}

Nodo *escena_instanciar_texto(const char *texto, const char *nombre, char *err, size_t nerr) {
	static int prof;
	Est e; memset(&e, 0, sizeof e); e.nombre = nombre; e.prof = prof; e.capa = -1;
	TextoCB cb = { es_seccion, es_prop, es_fila };
	++prof;
	int r = texto_analizar(texto, &cb, &e, err, nerr);
	--prof;
	if (r < 0 || !e.raiz) { if (e.raiz) nodo_liberar_ya(e.raiz); free(e.creados); free(e.cx); if (err && nerr && !err[0]) snprintf(err, nerr, "escena vacía"); return NULL; }
	snprintf(e.raiz->escena, sizeof e.raiz->escena, "%s", nombre);
	finalizar(&e);
	if (e.errores && err && nerr) snprintf(err, nerr, "%d avisos al cargar %s (ver el registro)", e.errores, nombre);
	Nodo *raiz = e.raiz;
	free(e.creados); free(e.cx);
	return raiz;
}

/* los textos de las escenas se guardan al leerlos: instanciar muchas veces una escena pequeña (balas, partículas) no vuelve al disco */
static struct { char ruta[CAPI_RUTA_MAX]; char *texto; } cache[64]; static int ncache;
void escena_vaciar_cache(void) { for (int i = 0; i < ncache; ++i) free(cache[i].texto); ncache = 0; }

Nodo *escena_instanciar(const char *ruta, char *err, size_t nerr) {
	const char *txt = NULL;
	for (int i = 0; i < ncache; ++i) if (!strcmp(cache[i].ruta, ruta)) txt = cache[i].texto;
	if (!txt) {
		size_t n; unsigned char *b = cap_leer(ruta, &n);
		if (!b) { if (err) snprintf(err, nerr, "no existe la escena %s", ruta); return NULL; }
		if (ncache < 64) { snprintf(cache[ncache].ruta, sizeof cache[0].ruta, "%s", ruta); cache[ncache].texto = (char *)b; txt = (char *)b; ++ncache; }
		else { char e2[160] = ""; Nodo *r = escena_instanciar_texto((char *)b, ruta, e2, sizeof e2); free(b); if (!r && err) snprintf(err, nerr, "%s", e2); return r; }
	}
	char e2[160] = "";
	Nodo *r = escena_instanciar_texto(txt, ruta, e2, sizeof e2);
	if (!r && err) snprintf(err, nerr, "%s", e2);
	return r;
}

/* ---------------------------------------------------------------------------------------------------- proyecto */
typedef struct { char sec[32]; } PEst;
static int p_sec(void *u, const Cab *c, int linea) { (void)linea; snprintf(((PEst *)u)->sec, 32, "%s", c->nt ? c->t[0] : ""); return 0; }
static int p_prop(void *u, const char *k, const Valor *v, int linea) {
	PEst *s = u; (void)linea;
	if (!v->n) return 0;
	const Tok *t = &v->t[0];
	if (!strcmp(s->sec, "proyecto")) {
		if (!strcmp(k, "nombre")) snprintf(g_proy.nombre, sizeof g_proy.nombre, "%s", t->s);
		else if (!strcmp(k, "escena_principal")) snprintf(g_proy.principal, sizeof g_proy.principal, "%s", t->s);
		else if (!strcmp(k, "semilla")) g_proy.semilla = (int)t->num;
	} else if (!strcmp(s->sec, "pantalla")) {
		if (!strcmp(k, "ancho")) g_proy.ancho = (int)t->num;
		else if (!strcmp(k, "alto")) g_proy.alto = (int)t->num;
		else if (!strcmp(k, "escala_entera")) g_proy.entera = t->num != 0;
		else if (!strcmp(k, "fondo")) {
			const char *c = t->s; if (*c == '#') ++c;
			if (strlen(c) == 6) for (int i = 0; i < 3; ++i) { char h[3] = { c[2 * i], c[2 * i + 1], 0 }; g_proy.fondo[i] = (uint8_t)strtol(h, NULL, 16); }
		}
	} else if (!strcmp(s->sec, "fisica")) {
		if (!strcmp(k, "gravedad")) g_proy.gravedad = (float)t->num;
		else if (!strcmp(k, "paso")) g_proy.paso = (int)t->num;
	} else if (!strcmp(s->sec, "acciones")) {
		char lig[256] = "";
		for (int i = 0; i < v->n; ++i) { if (i) strncat(lig, " ", sizeof lig - strlen(lig) - 1); strncat(lig, v->t[i].s, sizeof lig - strlen(lig) - 1); }
		accion_declarar(k, lig);
	}
	return 0;
}

int proyecto_cargar(char *err, size_t nerr) {
	size_t n; unsigned char *b = cap_leer("capibara.cfg", &n);
	if (!b) { if (err) snprintf(err, nerr, "falta capibara.cfg en el proyecto"); return -1; }
	PEst s = { "" };
	TextoCB cb = { p_sec, p_prop, NULL };
	int r = texto_analizar((char *)b, &cb, &s, err, nerr);
	free(b);
	if (g_proy.paso < 10) g_proy.paso = 10;
	if (g_proy.paso > 240) g_proy.paso = 240;
	if (g_proy.ancho < 64) g_proy.ancho = 64;
	if (g_proy.alto < 48) g_proy.alto = 48;
	g_gravedad = g_proy.gravedad;
	return r;
}
