/* SPDX-License-Identifier: MIT */
/* guion.c - guiones en Lua 5.4. Cada nodo puede llevar un guion: un fichero .lua que devuelve una tabla de métodos
 * (_listo, _proceso, _fisica, _entrada y los suyos). Dentro de ellos «self» es el propio nodo: sus propiedades (x, y,
 * visible, ...) se leen y escriben como campos; los campos nuevos (self.vidas = 3) se guardan en el nodo. */
#include "capi.h"
#include <stdarg.h>

lua_State *g_L;
int g_salir;
static const char MT[] = "Capibara.Nodo";
static char clave_cache;
typedef struct { int id; } UD;
static int g_errores;

/* ---------------------------------------------------------------------------------------------------- pila */
static int msgh(lua_State *L) {
	const char *m = lua_tostring(L, 1);
	luaL_traceback(L, L, m ? m : "(error sin texto)", 1);
	return 1;
}
static int llamar(lua_State *L, int nargs, int nres, const char *quien) {
	int base = lua_gettop(L) - nargs;
	lua_pushcfunction(L, msgh); lua_insert(L, base);
	int r = lua_pcall(L, nargs, nres, base);
	lua_remove(L, base);
	if (r != 0) {
		++g_errores;
		const char *m = lua_tostring(L, -1);
		capi_log("error en %s: %s", quien, m ? m : "?");
		lua_pop(L, 1);
	}
	return r;
}

void guion_empujar_nodo(lua_State *L, Nodo *n) {
	if (!n) { lua_pushnil(L); return; }
	lua_rawgetp(L, LUA_REGISTRYINDEX, &clave_cache);
	lua_rawgeti(L, -1, n->id);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		UD *u = lua_newuserdatauv(L, sizeof *u, 0); u->id = n->id;
		luaL_setmetatable(L, MT);
		lua_pushvalue(L, -1); lua_rawseti(L, -3, n->id);
	}
	lua_remove(L, -2);
}

static Nodo *nodo_en(lua_State *L, int i) {
	UD *u = luaL_checkudata(L, i, MT);
	Nodo *n = nodo_de_id(u->id);
	if (!n) luaL_error(L, "el nodo ya fue liberado");
	return n;
}
static Nodo *nodo_op(lua_State *L, int i) {   /* nodo o nil */
	if (lua_isnoneornil(L, i)) return NULL;
	return nodo_en(L, i);
}

/* ---------------------------------------------------------------------------------------------------- llamadas a métodos del guion */
static int tiene_metodo(Nodo *n, const char *m) {
	if (n->ref_clase == LUA_NOREF) return 0;
	lua_rawgeti(g_L, LUA_REGISTRYINDEX, n->ref_clase); lua_getfield(g_L, -1, m);
	int ok = lua_isfunction(g_L, -1); lua_pop(g_L, 2);
	return ok;
}

int guion_llamar(Nodo *n, const char *metodo, int tiene_num, double num) {
	lua_State *L = g_L;
	if (!n || n->ref_clase == LUA_NOREF) return 0;
	lua_rawgeti(L, LUA_REGISTRYINDEX, n->ref_clase);
	lua_getfield(L, -1, metodo); lua_remove(L, -2);
	if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return 0; }
	guion_empujar_nodo(L, n);
	if (tiene_num) lua_pushnumber(L, num);
	char q[160]; snprintf(q, sizeof q, "%s:%s (%s)", n->guion, metodo, n->nombre);
	llamar(L, 1 + (tiene_num ? 1 : 0), 0, q);
	return 1;
}

static void tras_listo(Nodo *n) {
	if (n->tipo == T_TEMP) { DTemp *t = n->d; if (t->autoinicio) { t->activo = 1; t->resto = t->tiempo; } }
	else if (n->tipo == T_AUDIO) {
		DAudio *a = n->d; if (a->ruta[0] && !a->snd) a->snd = rec_sonido(a->ruta);
		if (a->autoplay && a->snd) a->voz = a->musica ? (mez_musica(a->snd, a->vol, a->bucle), -2) : mez_voz(a->snd, a->vol, 0, a->bucle, 0);
	} else if (n->tipo == T_CAMARA) { DCamara *c = n->d; if (c->actual) { /* la actual única */ } }
}

void guion_listo(Nodo *n) {
	for (int i = 0; i < n->nh; ++i) guion_listo(n->hijos[i]);
	if (n->flags & F_LISTO) return;
	n->flags |= F_LISTO;
	tras_listo(n);
	guion_llamar(n, "_listo", 0, 0);
}

void guion_dato_liberar(Nodo *n) {
	if (!g_L) return;
	if (n->ref_clase != LUA_NOREF) { luaL_unref(g_L, LUA_REGISTRYINDEX, n->ref_clase); n->ref_clase = LUA_NOREF; }
	if (n->ref_datos != LUA_NOREF) { luaL_unref(g_L, LUA_REGISTRYINDEX, n->ref_datos); n->ref_datos = LUA_NOREF; }
	for (int i = 0; i < n->ncon; ++i) if (n->con[i].ref_fn != LUA_NOREF) { luaL_unref(g_L, LUA_REGISTRYINDEX, n->con[i].ref_fn); n->con[i].ref_fn = LUA_NOREF; }
	n->flags &= ~(unsigned)(F_PROC | F_FISICA | F_ENTRADA);
	lua_rawgetp(g_L, LUA_REGISTRYINDEX, &clave_cache); lua_pushnil(g_L); lua_rawseti(g_L, -2, n->id); lua_pop(g_L, 1);
}

void guion_poner_dato(Nodo *n, const char *clave, const Valor *v) {
	lua_State *L = g_L;
	if (n->ref_datos == LUA_NOREF) { lua_newtable(L); n->ref_datos = luaL_ref(L, LUA_REGISTRYINDEX); }
	lua_rawgeti(L, LUA_REGISTRYINDEX, n->ref_datos);
	if (v->n == 1) { if (v->t[0].str) lua_pushstring(L, v->t[0].s); else if (v->t[0].num == floor(v->t[0].num)) lua_pushinteger(L, (lua_Integer)v->t[0].num); else lua_pushnumber(L, v->t[0].num); }
	else { lua_createtable(L, v->n, 0); for (int i = 0; i < v->n; ++i) { if (v->t[i].str) lua_pushstring(L, v->t[i].s); else lua_pushnumber(L, v->t[i].num); lua_rawseti(L, -2, i + 1); } }
	lua_setfield(L, -2, clave); lua_pop(L, 1);
}

/* Cada fichero de guion se ejecuta una sola vez: su tabla de métodos (la clase) la comparten todos los nodos que lo usan. */
static struct { char ruta[CAPI_RUTA_MAX]; int ref; } clases[128]; static int nclases;

int guion_cargar(Nodo *n, const char *ruta) {
	lua_State *L = g_L; size_t tam;
	if (g_modo_editor) return 0;   /* el editor no ejecuta los guiones al mostrar una escena */
	int ref = LUA_NOREF;
	for (int i = 0; i < nclases; ++i) if (!strcmp(clases[i].ruta, ruta)) ref = clases[i].ref;
	if (ref == LUA_NOREF) {
		unsigned char *src = cap_leer(ruta, &tam);
		if (!src) { capi_log("no existe el guion %s (nodo %s)", ruta, n->nombre); return -1; }
		char nom[CAPI_RUTA_MAX + 2]; snprintf(nom, sizeof nom, "@%s", ruta);
		int r = luaL_loadbuffer(L, (char *)src, tam, nom);
		free(src);
		if (r != 0) { capi_log("guion %s: %s", ruta, lua_tostring(L, -1)); lua_pop(L, 1); return -1; }
		guion_empujar_nodo(L, n);   /* el fichero recibe, por si le sirve, el primer nodo que lo usa */
		if (llamar(L, 1, 1, ruta) != 0) return -1;
		if (!lua_istable(L, -1)) { capi_log("el guion %s debe devolver una tabla de métodos (return Clase)", ruta); lua_pop(L, 1); return -1; }
		if (nclases < 128) { lua_pushvalue(L, -1); clases[nclases].ref = luaL_ref(L, LUA_REGISTRYINDEX); snprintf(clases[nclases].ruta, sizeof clases[0].ruta, "%s", ruta); ref = clases[nclases++].ref; }
		n->ref_clase = luaL_ref(L, LUA_REGISTRYINDEX);
	} else { lua_rawgeti(L, LUA_REGISTRYINDEX, ref); n->ref_clase = luaL_ref(L, LUA_REGISTRYINDEX); }
	if (n->ref_datos == LUA_NOREF) { lua_newtable(L); n->ref_datos = luaL_ref(L, LUA_REGISTRYINDEX); }
	if (tiene_metodo(n, "_proceso")) n->flags |= F_PROC;
	if (tiene_metodo(n, "_fisica")) n->flags |= F_FISICA;
	if (tiene_metodo(n, "_entrada")) n->flags |= F_ENTRADA;
	return 0;
}

/* ---------------------------------------------------------------------------------------------------- recorrido */
static Nodo **lista; static int nl, cl;
static void juntar(Nodo *n, unsigned marca) {
	if (n->flags & F_BORRAR) return;
	if ((n->flags & marca) && (n->flags & F_LISTO) && (n->flags & F_PROCESAR)) {
		if (nl == cl) { cl = cl ? cl * 2 : 64; lista = realloc(lista, sizeof(Nodo *) * (size_t)cl); }
		lista[nl++] = n;
	}
	for (int i = 0; i < n->nh; ++i) juntar(n->hijos[i], marca);
}
static void recorrer_llamando(Nodo *raiz, unsigned marca, const char *m, float dt) {
	int base = nl; juntar(raiz, marca);
	int fin = nl; int *ids = malloc(sizeof(int) * (size_t)(fin - base + 1));
	for (int i = base; i < fin; ++i) ids[i - base] = lista[i]->id;
	nl = base;
	for (int i = 0; i < fin - base; ++i) {
		Nodo *n = nodo_de_id(ids[i]);
		if (n && !(n->flags & F_BORRAR) && (n->flags & marca)) guion_llamar(n, m, 1, dt);
	}
	free(ids);
}
void guion_proceso(Nodo *raiz, float dt) { recorrer_llamando(raiz, F_PROC, "_proceso", dt); }
void guion_fisica(Nodo *raiz, float dt) { recorrer_llamando(raiz, F_FISICA, "_fisica", dt); }

void guion_enviar_evento(Nodo *raiz, const Evento *e) {
	lua_State *L = g_L;
	int base = nl; juntar(raiz, F_ENTRADA); int fin = nl;
	int *ids = malloc(sizeof(int) * (size_t)(fin - base + 1));
	for (int i = base; i < fin; ++i) ids[i - base] = lista[i]->id;
	nl = base;
	if (fin == base) { free(ids); return; }
	lua_newtable(L);
	lua_pushstring(L, e->tipo); lua_setfield(L, -2, "tipo");
	if (!strcmp(e->tipo, "accion")) { lua_pushstring(L, e->nome); lua_setfield(L, -2, "accion"); lua_pushboolean(L, e->abajo); lua_setfield(L, -2, "presionada"); }
	else if (!strcmp(e->tipo, "tecla")) { lua_pushstring(L, e->nome); lua_setfield(L, -2, "tecla"); lua_pushboolean(L, e->abajo); lua_setfield(L, -2, "presionada"); }
	else { lua_pushinteger(L, e->x); lua_setfield(L, -2, "x"); lua_pushinteger(L, e->y); lua_setfield(L, -2, "y");
	       if (!strcmp(e->tipo, "raton")) { lua_pushinteger(L, e->boton); lua_setfield(L, -2, "boton"); lua_pushboolean(L, e->abajo); lua_setfield(L, -2, "presionada"); } }
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);
	for (int i = 0; i < fin - base; ++i) {
		Nodo *n = nodo_de_id(ids[i]);
		if (!n || (n->flags & F_BORRAR) || !(n->flags & F_ENTRADA)) continue;
		lua_rawgeti(L, LUA_REGISTRYINDEX, n->ref_clase); lua_getfield(L, -1, "_entrada"); lua_remove(L, -2);
		if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
		guion_empujar_nodo(L, n); lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
		char q[160]; snprintf(q, sizeof q, "%s:_entrada (%s)", n->guion, n->nombre);
		llamar(L, 2, 0, q);
	}
	luaL_unref(L, LUA_REGISTRYINDEX, ref);
	free(ids);
}

/* ---------------------------------------------------------------------------------------------------- señales */
void sen_conectar_nodo(Nodo *n, const char *senal, Nodo *dest, const char *metodo) {
	if (n->ncon == n->ccon) { n->ccon = n->ccon ? n->ccon * 2 : 4; n->con = realloc(n->con, sizeof(Conexion) * (size_t)n->ccon); }
	Conexion *c = &n->con[n->ncon++]; memset(c, 0, sizeof *c);
	snprintf(c->senal, sizeof c->senal, "%s", senal); c->ref_fn = LUA_NOREF; c->destino_id = dest ? dest->id : 0;
	snprintf(c->metodo, sizeof c->metodo, "%s", metodo ? metodo : "");
}

void sen_emitir(Nodo *n, const char *senal, int nargs) {
	lua_State *L = g_L;
	Conexion cs[16]; int k = 0;
	for (int i = 0; i < n->ncon && k < 16; ++i) if (!strcmp(n->con[i].senal, senal)) cs[k++] = n->con[i];
	int top = lua_gettop(L);
	for (int i = 0; i < k; ++i) {
		int extra = 0;
		if (cs[i].ref_fn != LUA_NOREF) lua_rawgeti(L, LUA_REGISTRYINDEX, cs[i].ref_fn);
		else {
			Nodo *d = nodo_de_id(cs[i].destino_id); if (!d) continue;
			guion_empujar_nodo(L, d); lua_getfield(L, -1, cs[i].metodo);
			if (!lua_isfunction(L, -1)) { lua_pop(L, 2); continue; }
			lua_insert(L, -2); extra = 1;
		}
		for (int a = 0; a < nargs; ++a) lua_pushvalue(L, top - nargs + 1 + a);
		char q[96]; snprintf(q, sizeof q, "señal %s de %s", senal, n->nombre);
		llamar(L, extra + nargs, 0, q);
	}
	lua_settop(L, top - nargs);
}

/* ---------------------------------------------------------------------------------------------------- Valor desde Lua */
static void valor_de_lua(lua_State *L, int desde, Valor *v) {
	v->n = 0;
	for (int i = desde; i <= lua_gettop(L) && v->n < VAL_MAX; ++i) {
		Tok *t = &v->t[v->n++]; t->s[0] = 0; t->num = 0; t->str = 0;
		if (lua_isboolean(L, i)) t->num = lua_toboolean(L, i);
		else if (lua_type(L, i) == LUA_TNUMBER) t->num = lua_tonumber(L, i);
		else { t->str = 1; snprintf(t->s, sizeof t->s, "%s", luaL_tolstring(L, i, NULL)); lua_pop(L, 1); }
	}
}

/* ---------------------------------------------------------------------------------------------------- métodos de nodo */
#define NODO_ARG Nodo *n = nodo_en(L, 1)
static int m_obtener_nodo(lua_State *L) { NODO_ARG; Nodo *r = nodo_buscar(n, luaL_checkstring(L, 2)); guion_empujar_nodo(L, r); return 1; }
static int m_buscar(lua_State *L) { NODO_ARG; guion_empujar_nodo(L, nodo_buscar_nombre(n, luaL_checkstring(L, 2))); return 1; }
static int m_hijos(lua_State *L) { NODO_ARG; lua_createtable(L, n->nh, 0); for (int i = 0; i < n->nh; ++i) { guion_empujar_nodo(L, n->hijos[i]); lua_rawseti(L, -2, i + 1); } return 1; }
static int m_agregar_hijo(lua_State *L) { NODO_ARG; Nodo *h = nodo_en(L, 2); nodo_agregar(n, h); return 0; }
static int m_quitar_hijo(lua_State *L) { NODO_ARG; Nodo *h = nodo_en(L, 2); if (h->padre == n) nodo_quitar(h); return 0; }
static int m_liberar(lua_State *L) { NODO_ARG; nodo_encolar_liberar(n); return 0; }
static int m_instanciar(lua_State *L) {
	NODO_ARG; (void)n; char err[160] = "";
	Nodo *r = escena_instanciar(luaL_checkstring(L, 2), err, sizeof err);
	if (!r) { lua_pushnil(L); lua_pushstring(L, err); return 2; }
	guion_empujar_nodo(L, r); return 1;
}
static int m_en_grupo(lua_State *L) { NODO_ARG; lua_pushboolean(L, nodo_en_grupo(n, luaL_checkstring(L, 2))); return 1; }
static int m_agregar_a_grupo(lua_State *L) { NODO_ARG; nodo_grupo_agregar(n, luaL_checkstring(L, 2)); return 0; }
static int m_quitar_de_grupo(lua_State *L) { NODO_ARG; nodo_grupo_quitar(n, luaL_checkstring(L, 2)); return 0; }
static int m_conectar(lua_State *L) {
	NODO_ARG; const char *s = luaL_checkstring(L, 2);
	if (lua_isfunction(L, 3)) {
		sen_conectar_nodo(n, s, NULL, NULL);
		lua_pushvalue(L, 3); n->con[n->ncon - 1].ref_fn = luaL_ref(L, LUA_REGISTRYINDEX);
	} else { Nodo *d = nodo_en(L, 3); sen_conectar_nodo(n, s, d, luaL_checkstring(L, 4)); }
	return 0;
}
static int m_desconectar(lua_State *L) {
	NODO_ARG; const char *s = luaL_checkstring(L, 2);
	for (int i = 0; i < n->ncon;) if (!strcmp(n->con[i].senal, s)) {
		if (n->con[i].ref_fn != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, n->con[i].ref_fn);
		memmove(&n->con[i], &n->con[i + 1], sizeof(Conexion) * (size_t)(n->ncon - i - 1)); --n->ncon;
	} else ++i;
	return 0;
}
static int m_emitir(lua_State *L) {
	NODO_ARG; const char *s = luaL_checkstring(L, 2);
	int na = lua_gettop(L) - 2;
	/* los argumentos ya están al final de la pila */
	sen_emitir(n, s, na);
	(void)n; return 0;
}
static int m_mover(lua_State *L) { NODO_ARG; n->x += (float)luaL_checknumber(L, 2); n->y += (float)luaL_optnumber(L, 3, 0); ++g_epoca; return 0; }
static int m_pos_global(lua_State *L) { NODO_ARG; float x, y; nodo_pos_global(n, &x, &y); lua_pushnumber(L, x); lua_pushnumber(L, y); return 2; }
static int m_poner_pos_global(lua_State *L) { NODO_ARG; nodo_poner_pos_global(n, (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3)); return 0; }
static int m_teletransportar(lua_State *L) { NODO_ARG; nodo_teletransportar(n, (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3)); return 0; }
static int m_poner(lua_State *L) {
	NODO_ARG; static Valor v; valor_de_lua(L, 3, &v);
	if (!prop_poner(n, luaL_checkstring(L, 2), &v)) return luaL_error(L, "propiedad «%s» desconocida o no válida para %s", lua_tostring(L, 2), CAPI_TIPOS[n->tipo]);
	return 0;
}
static int m_animar(lua_State *L) { NODO_ARG; if (n->tipo != T_SPRITE) return luaL_error(L, "animar: solo para Sprite"); sprite_animar(n, luaL_checkstring(L, 2)); return 0; }
static int m_detener_anim(lua_State *L) { NODO_ARG; if (n->tipo == T_SPRITE) ((DSprite *)n->d)->reproduciendo = 0; return 0; }
static int m_mover_y_deslizar(lua_State *L) {
	NODO_ARG; if (n->tipo != T_CUERPO) return luaL_error(L, "mover_y_deslizar: solo para Cuerpo");
	float vx = (float)luaL_checknumber(L, 2), vy = (float)luaL_checknumber(L, 3), dt = 1.0f / (float)g_hz, rx, ry;
	fisica_mover(n, vx * dt, vy * dt, &rx, &ry);
	lua_pushnumber(L, rx / dt); lua_pushnumber(L, ry / dt); return 2;
}
static int m_ultimo_choque(lua_State *L) { NODO_ARG; if (n->tipo != T_CUERPO) return 0; DCuerpo *d = n->d; guion_empujar_nodo(L, nodo_de_id(d->col_id)); lua_pushnumber(L, d->nx); lua_pushnumber(L, d->ny); return 3; }
static int m_solapados(lua_State *L) {
	NODO_ARG; if (n->tipo != T_AREA) return luaL_error(L, "solapados: solo para Area");
	DArea *d = n->d; lua_newtable(L); int k = 1;
	for (int i = 0; i < d->nd; ++i) { Nodo *o = nodo_de_id(d->d[i].id); if (o) { guion_empujar_nodo(L, o); lua_rawseti(L, -2, k++); } }
	return 1;
}
static int m_hacer_actual(lua_State *L) { NODO_ARG; if (n->tipo == T_CAMARA) ((DCamara *)n->d)->actual = 1; return 0; }
static int m_seguir(lua_State *L) {
	NODO_ARG; if (n->tipo != T_CAMARA) return luaL_error(L, "seguir: solo para Camara2D");
	DCamara *c = n->d; Nodo *t = nodo_op(L, 2); c->seguir_id = t ? t->id : 0; c->seguir_ruta[0] = 0; return 0;
}
static int m_sacudir(lua_State *L) { NODO_ARG; if (n->tipo == T_CAMARA) { DCamara *c = n->d; c->sac_amp = (float)luaL_checknumber(L, 2); c->sac_dur = c->sac_t = (float)luaL_optnumber(L, 3, 0.3); } return 0; }
static int m_iniciar(lua_State *L) { NODO_ARG; if (n->tipo == T_TEMP) { DTemp *t = n->d; if (lua_isnumber(L, 2)) t->tiempo = (float)lua_tonumber(L, 2); t->resto = t->tiempo; t->activo = 1; } return 0; }
static int m_detener(lua_State *L) {
	NODO_ARG;
	if (n->tipo == T_TEMP) ((DTemp *)n->d)->activo = 0;
	else if (n->tipo == T_AUDIO) { DAudio *a = n->d; if (a->musica) mez_detener_musica(); else if (a->voz >= 0) mez_detener(a->voz); a->voz = -1; }
	return 0;
}
static int m_reproducir(lua_State *L) {
	NODO_ARG; if (n->tipo != T_AUDIO) return luaL_error(L, "reproducir: solo para ReproductorAudio");
	DAudio *a = n->d; if (!a->snd && a->ruta[0]) a->snd = rec_sonido(a->ruta);
	if (!a->snd) return 0;
	if (a->musica) mez_musica(a->snd, a->vol, a->bucle); else a->voz = mez_voz(a->snd, a->vol, 0, a->bucle, 0);
	return 0;
}
static int m_emitir_part(lua_State *L) { NODO_ARG; if (n->tipo == T_PARTICULAS) particulas_emitir(n, (int)luaL_optinteger(L, 2, 0)); return 0; }

/* mapa */
static int capa_arg(lua_State *L, Nodo *m, int i) {
	if (lua_isnoneornil(L, i)) return 0;
	if (lua_isstring(L, i) && !lua_isnumber(L, i)) { int c = mapa_capa(m, lua_tostring(L, i)); if (c < 0) luaL_error(L, "no hay capa «%s»", lua_tostring(L, i)); return c; }
	return (int)lua_tointeger(L, i) - 1;
}
static int m_celda(lua_State *L) {
	NODO_ARG; if (n->tipo != T_MAPA) return luaL_error(L, "celda: solo para MapaCasillas");
	int v = mapa_celda(n, capa_arg(L, n, 4), (int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3));
	if (v < 0) lua_pushnil(L); else lua_pushinteger(L, v);
	return 1;
}
static int m_poner_celda(lua_State *L) {
	NODO_ARG; if (n->tipo != T_MAPA) return luaL_error(L, "poner_celda: solo para MapaCasillas");
	int v = lua_isnil(L, 5) ? -1 : (int)luaL_checkinteger(L, 5);
	mapa_poner(n, capa_arg(L, n, 4), (int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3), v); return 0;
}
static int m_mundo_a_celda(lua_State *L) {
	NODO_ARG; if (n->tipo != T_MAPA) return 0; DMapa *m = n->d; float mm[6]; nodo_matriz(n, mm);
	float cw = m->cw * sqrtf(mm[0] * mm[0] + mm[1] * mm[1]), ch = m->ch * sqrtf(mm[2] * mm[2] + mm[3] * mm[3]);
	lua_pushinteger(L, (lua_Integer)floorf(((float)luaL_checknumber(L, 2) - mm[4]) / cw)); lua_pushinteger(L, (lua_Integer)floorf(((float)luaL_checknumber(L, 3) - mm[5]) / ch)); return 2;
}
static int m_celda_a_mundo(lua_State *L) {
	NODO_ARG; if (n->tipo != T_MAPA) return 0; DMapa *m = n->d; float mm[6]; nodo_matriz(n, mm);
	float cw = m->cw * sqrtf(mm[0] * mm[0] + mm[1] * mm[1]), ch = m->ch * sqrtf(mm[2] * mm[2] + mm[3] * mm[3]);
	lua_pushnumber(L, mm[4] + ((float)luaL_checknumber(L, 2) + 0.5f) * cw); lua_pushnumber(L, mm[5] + ((float)luaL_checknumber(L, 3) + 0.5f) * ch); return 2;
}
static int m_tamano_mapa(lua_State *L) { NODO_ARG; if (n->tipo != T_MAPA) return 0; DMapa *m = n->d; lua_pushinteger(L, m->w); lua_pushinteger(L, m->h); return 2; }

static const luaL_Reg metodos[] = {
	{ "obtener_nodo", m_obtener_nodo }, { "buscar", m_buscar }, { "hijos", m_hijos }, { "agregar_hijo", m_agregar_hijo }, { "quitar_hijo", m_quitar_hijo },
	{ "liberar", m_liberar }, { "instanciar", m_instanciar }, { "en_grupo", m_en_grupo }, { "agregar_a_grupo", m_agregar_a_grupo },
	{ "quitar_de_grupo", m_quitar_de_grupo }, { "conectar", m_conectar }, { "desconectar", m_desconectar }, { "emitir", m_emitir },
	{ "mover", m_mover }, { "posicion_global", m_pos_global }, { "poner_posicion_global", m_poner_pos_global }, { "teletransportar", m_teletransportar },
	{ "poner", m_poner }, { "animar", m_animar }, { "detener_anim", m_detener_anim }, { "mover_y_deslizar", m_mover_y_deslizar },
	{ "ultimo_choque", m_ultimo_choque }, { "solapados", m_solapados }, { "hacer_actual", m_hacer_actual }, { "seguir", m_seguir }, { "sacudir", m_sacudir },
	{ "iniciar", m_iniciar }, { "detener", m_detener }, { "reproducir", m_reproducir }, { "emitir_particulas", m_emitir_part },
	{ "celda", m_celda }, { "poner_celda", m_poner_celda }, { "mundo_a_celda", m_mundo_a_celda }, { "celda_a_mundo", m_celda_a_mundo },
	{ "tamano_mapa", m_tamano_mapa }, { NULL, NULL }
};

/* ---------------------------------------------------------------------------------------------------- __index y __newindex */
static int mt_index(lua_State *L) {
	UD *u = luaL_checkudata(L, 1, MT);
	const char *k = luaL_checkstring(L, 2);
	/* 1. métodos incorporados */
	lua_getmetatable(L, 1); lua_getfield(L, -1, "__metodos"); lua_getfield(L, -1, k);
	if (!lua_isnil(L, -1)) return 1;
	lua_pop(L, 3);
	Nodo *n = nodo_de_id(u->id);
	if (!n) return luaL_error(L, "el nodo ya fue liberado (campo «%s»)", k);
	/* 2. propiedades */
	if (!strcmp(k, "nombre")) { lua_pushstring(L, n->nombre); return 1; }
	if (!strcmp(k, "tipo")) { lua_pushstring(L, CAPI_TIPOS[n->tipo]); return 1; }
	if (!strcmp(k, "padre")) { guion_empujar_nodo(L, n->padre); return 1; }
	if (!strcmp(k, "id")) { lua_pushinteger(L, n->id); return 1; }
	if (n->tipo == T_CUERPO) {
		DCuerpo *d = n->d;
		if (!strcmp(k, "en_suelo")) { lua_pushboolean(L, d->suelo); return 1; }
		if (!strcmp(k, "en_techo")) { lua_pushboolean(L, d->techo); return 1; }
		if (!strcmp(k, "en_pared")) { lua_pushboolean(L, d->pared); return 1; }
	}
	if (n->tipo == T_SPRITE && !strcmp(k, "animacion")) { lua_pushstring(L, ((DSprite *)n->d)->act_nome); return 1; }
	if (n->tipo == T_TEMP && !strcmp(k, "activo")) { lua_pushboolean(L, ((DTemp *)n->d)->activo); return 1; }
	double num; char str[CAPI_RUTA_MAX];
	if (prop_obtener_num(n, k, &num)) { if (prop_es_bool(n->tipo, k)) lua_pushboolean(L, num != 0); else if (prop_es_entero(n->tipo, k)) lua_pushinteger(L, (lua_Integer)num); else lua_pushnumber(L, num); return 1; }
	if (prop_obtener_str(n, k, str, sizeof str)) { lua_pushstring(L, str); return 1; }
	/* 3. variables del nodo y 4. métodos de la clase del guion */
	if (n->ref_datos != LUA_NOREF) { lua_rawgeti(L, LUA_REGISTRYINDEX, n->ref_datos); lua_getfield(L, -1, k); if (!lua_isnil(L, -1)) return 1; lua_pop(L, 2); }
	if (n->ref_clase != LUA_NOREF) { lua_rawgeti(L, LUA_REGISTRYINDEX, n->ref_clase); lua_getfield(L, -1, k); return 1; }
	lua_pushnil(L); return 1;
}

static int mt_newindex(lua_State *L) {
	Nodo *n = nodo_en(L, 1); const char *k = luaL_checkstring(L, 2);
	if (!strcmp(k, "nombre")) { snprintf(n->nombre, sizeof n->nombre, "%s", luaL_checkstring(L, 3)); return 0; }
	if (!strcmp(k, "padre") || !strcmp(k, "tipo") || !strcmp(k, "id")) return luaL_error(L, "«%s» es de solo lectura", k);
	if (prop_existe(n->tipo, k)) {
		static Valor v; valor_de_lua(L, 3, &v);
		if (!prop_poner(n, k, &v)) return luaL_error(L, "valor no válido para %s.%s", CAPI_TIPOS[n->tipo], k);
		return 0;
	}
	if (n->tipo == T_CUERPO && (!strcmp(k, "en_suelo") || !strcmp(k, "en_techo") || !strcmp(k, "en_pared"))) return luaL_error(L, "«%s» es de solo lectura", k);
	if (n->ref_datos == LUA_NOREF) { lua_newtable(L); n->ref_datos = luaL_ref(L, LUA_REGISTRYINDEX); }
	lua_rawgeti(L, LUA_REGISTRYINDEX, n->ref_datos); lua_pushvalue(L, 3); lua_setfield(L, -2, k);
	return 0;
}
static int mt_eq(lua_State *L) { UD *a = luaL_checkudata(L, 1, MT), *b = luaL_checkudata(L, 2, MT); lua_pushboolean(L, a->id == b->id); return 1; }
static int mt_tostring(lua_State *L) {
	UD *u = luaL_checkudata(L, 1, MT); Nodo *n = nodo_de_id(u->id);
	if (n) lua_pushfstring(L, "%s(%s)", CAPI_TIPOS[n->tipo], n->nombre); else lua_pushstring(L, "Nodo(liberado)");
	return 1;
}

/* ---------------------------------------------------------------------------------------------------- tablas globales */
static int c_cargar_escena(lua_State *L) {
	char err[160] = ""; Nodo *n = escena_instanciar(luaL_checkstring(L, 1), err, sizeof err);
	if (!n) { lua_pushnil(L); lua_pushstring(L, err); return 2; }
	guion_empujar_nodo(L, n); return 1;
}
static int c_cambiar_escena(lua_State *L) { escena_cambiar(luaL_checkstring(L, 1)); return 0; }
static int c_salir(lua_State *L) { (void)L; g_salir = 1; return 0; }
static int c_nuevo(lua_State *L) {
	int t = capi_tipo_por_nombre(luaL_checkstring(L, 1));
	if (t < 0) return luaL_error(L, "tipo de nodo desconocido: %s", lua_tostring(L, 1));
	guion_empujar_nodo(L, nodo_nuevo((CapiTipo)t, luaL_optstring(L, 2, NULL))); return 1;
}
static void juntar_grupo(Nodo *n, const char *g, lua_State *L, int *k) {
	if (n->flags & F_BORRAR) return;
	if (nodo_en_grupo(n, g)) { guion_empujar_nodo(L, n); lua_rawseti(L, -2, (*k)++); }
	for (int i = 0; i < n->nh; ++i) juntar_grupo(n->hijos[i], g, L, k);
}
static int c_grupo(lua_State *L) { lua_newtable(L); int k = 1; juntar_grupo(g_mundo.arbol, luaL_checkstring(L, 1), L, &k); return 1; }
static int c_raiz(lua_State *L) { guion_empujar_nodo(L, g_mundo.raiz); return 1; }
static int c_valido(lua_State *L) { UD *u = luaL_testudata(L, 1, MT); lua_pushboolean(L, u && nodo_de_id(u->id) && !(nodo_de_id(u->id)->flags & F_BORRAR)); return 1; }
static int c_pantalla(lua_State *L) { lua_pushinteger(L, g_proy.ancho); lua_pushinteger(L, g_proy.alto); return 2; }
static int c_hud(lua_State *L) { g_hud = lua_toboolean(L, 1); return 0; }

static int e_presionada(lua_State *L) { lua_pushboolean(L, accion_presionada(luaL_checkstring(L, 1))); return 1; }
static int e_recien_p(lua_State *L) { lua_pushboolean(L, accion_recien(luaL_checkstring(L, 1))); return 1; }
static int e_recien_s(lua_State *L) { lua_pushboolean(L, accion_soltada(luaL_checkstring(L, 1))); return 1; }
static int e_fuerza(lua_State *L) { lua_pushnumber(L, accion_fuerza(luaL_checkstring(L, 1))); return 1; }
static int e_eje(lua_State *L) { lua_pushnumber(L, entrada_eje(luaL_checkstring(L, 1), luaL_checkstring(L, 2))); return 1; }
static int e_raton(lua_State *L) { lua_pushinteger(L, g_raton_x); lua_pushinteger(L, g_raton_y); return 2; }
static int e_raton_mundo(lua_State *L) { float x, y; dib_cam_pantalla_a_mundo((float)g_raton_x, (float)g_raton_y, &x, &y); lua_pushnumber(L, x); lua_pushnumber(L, y); return 2; }
static int e_declarar(lua_State *L) { lua_pushboolean(L, accion_declarar(luaL_checkstring(L, 1), luaL_checkstring(L, 2)) == 0); return 1; }

static int a_sonido(lua_State *L) {
	Sonido *s = rec_sonido(luaL_checkstring(L, 1)); if (!s) { lua_pushboolean(L, 0); return 1; }
	lua_pushboolean(L, mez_voz(s, (float)luaL_optnumber(L, 2, 1), (float)luaL_optnumber(L, 3, 0), 0, 0) >= 0); return 1;
}
static int a_musica(lua_State *L) { Sonido *s = rec_sonido(luaL_checkstring(L, 1)); if (s) mez_musica(s, (float)luaL_optnumber(L, 2, 1), lua_isnoneornil(L, 3) ? 1 : lua_toboolean(L, 3)); return 0; }
static int a_detener_musica(lua_State *L) { (void)L; mez_detener_musica(); return 0; }
static int a_volumen(lua_State *L) { mez_volumen((float)luaL_checknumber(L, 1)); return 0; }

static int f_gravedad(lua_State *L) { if (lua_isnumber(L, 1)) g_gravedad = (float)lua_tonumber(L, 1); lua_pushnumber(L, g_gravedad); return 1; }
static int f_punto_solido(lua_State *L) { lua_pushboolean(L, fisica_choca_mapa_punto(g_mundo.raiz, (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2))); return 1; }

static int t_cuadros(lua_State *L) { lua_pushinteger(L, (lua_Integer)g_mundo.cuadro); return 1; }
static int t_segundos(lua_State *L) { lua_pushnumber(L, g_mundo.tiempo); return 1; }

static void tabla(lua_State *L, const char *nom, const luaL_Reg *r) { lua_newtable(L); luaL_setfuncs(L, r, 0); lua_setglobal(L, nom); }

int guion_iniciar(void) {
	lua_State *L = luaL_newstate();
	if (!L) return -1;
	g_L = L;
	luaL_requiref(L, "_G", luaopen_base, 1); lua_pop(L, 1);
	luaL_requiref(L, "table", luaopen_table, 1); lua_pop(L, 1);
	luaL_requiref(L, "string", luaopen_string, 1); lua_pop(L, 1);
	luaL_requiref(L, "math", luaopen_math, 1); lua_pop(L, 1);
	luaL_requiref(L, "utf8", luaopen_utf8, 1); lua_pop(L, 1);
	luaL_requiref(L, "coroutine", luaopen_coroutine, 1); lua_pop(L, 1);
	/* caché de userdata: tabla de valores débiles */
	lua_newtable(L); lua_newtable(L); lua_pushstring(L, "v"); lua_setfield(L, -2, "__mode"); lua_setmetatable(L, -2);
	lua_rawsetp(L, LUA_REGISTRYINDEX, &clave_cache);
	luaL_newmetatable(L, MT);
	lua_newtable(L); luaL_setfuncs(L, metodos, 0); lua_setfield(L, -2, "__metodos");
	lua_pushcfunction(L, mt_index); lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, mt_newindex); lua_setfield(L, -2, "__newindex");
	lua_pushcfunction(L, mt_eq); lua_setfield(L, -2, "__eq");
	lua_pushcfunction(L, mt_tostring); lua_setfield(L, -2, "__tostring");
	lua_pop(L, 1);
	static const luaL_Reg cap[] = { { "cargar_escena", c_cargar_escena }, { "cambiar_escena", c_cambiar_escena }, { "salir", c_salir }, { "nuevo", c_nuevo },
	                                { "grupo", c_grupo }, { "raiz", c_raiz }, { "valido", c_valido }, { "pantalla", c_pantalla }, { "hud", c_hud }, { NULL, NULL } };
	tabla(L, "Capibara", cap);
	lua_getglobal(L, "Capibara"); lua_pushstring(L, CAPI_VERSION); lua_setfield(L, -2, "version"); lua_pop(L, 1);
	static const luaL_Reg ent[] = { { "presionada", e_presionada }, { "recien_presionada", e_recien_p }, { "recien_soltada", e_recien_s }, { "fuerza", e_fuerza },
	                                { "eje", e_eje }, { "raton", e_raton }, { "raton_mundo", e_raton_mundo }, { "declarar", e_declarar }, { NULL, NULL } };
	tabla(L, "Entrada", ent);
	static const luaL_Reg aud[] = { { "sonido", a_sonido }, { "musica", a_musica }, { "detener_musica", a_detener_musica }, { "volumen", a_volumen }, { NULL, NULL } };
	tabla(L, "Audio", aud);
	static const luaL_Reg fis[] = { { "gravedad", f_gravedad }, { "punto_solido", f_punto_solido }, { NULL, NULL } };
	tabla(L, "Fisica", fis);
	static const luaL_Reg tie[] = { { "cuadros", t_cuadros }, { "segundos", t_segundos }, { NULL, NULL } };
	tabla(L, "Tiempo", tie);
	char sem[64]; snprintf(sem, sizeof sem, "math.randomseed(%d)", g_proy.semilla); luaL_dostring(L, sem);
	return 0;
}

void guion_cerrar(void) { if (g_L) { lua_close(g_L); g_L = NULL; } nclases = 0; }

int guion_ejecutar_texto(const char *texto, const char *nombre) {
	if (luaL_loadbuffer(g_L, texto, strlen(texto), nombre) != 0) { capi_log("%s", lua_tostring(g_L, -1)); lua_pop(g_L, 1); return -1; }
	return llamar(g_L, 0, 0, nombre) == 0 ? 0 : -1;
}
