/* SPDX-License-Identifier: MIT */
/* nodo.c - el árbol de nodos de Capibara: creación, jerarquía, grupos, rutas, matrices y la cola de liberación. */
#include "capi.h"
#include <stdarg.h>

const char *const CAPI_TIPOS[T_N] = { "Nodo", "Nodo2D", "Sprite", "MapaCasillas", "Camara2D", "Etiqueta", "Boton", "Area", "Cuerpo",
                                      "Particulas", "ReproductorAudio", "Temporizador" };
unsigned g_epoca = 1;
int g_modo_editor;   /* D460: el editor muestra las escenas sin ejecutar guiones ni cargar sonidos */

/* ---------------------------------------------------------------------------------------------------- registro de ids */
static Nodo **slots; static unsigned short *gens; static int nslots, cslots; static int *libres; static int nlibres, clibres;
static int total_vivos;

static int registrar(Nodo *n) {
	int s;
	if (nlibres) s = libres[--nlibres];
	else {
		if (nslots == cslots) {
			cslots = cslots ? cslots * 2 : 256;
			slots = realloc(slots, sizeof(Nodo *) * (size_t)cslots);
			gens = realloc(gens, sizeof(unsigned short) * (size_t)cslots);
		}
		s = nslots++; gens[s] = 0;
	}
	if (++gens[s] >= 2000) gens[s] = 1;
	slots[s] = n; ++total_vivos;
	return (gens[s] << 20) | s;
}

Nodo *nodo_de_id(int id) {
	if (id <= 0) return NULL;
	int s = id & 0xFFFFF; unsigned g = (unsigned)id >> 20;
	if (s >= nslots || gens[s] != g) return NULL;
	return slots[s];
}

static void desregistrar(Nodo *n) {
	int s = n->id & 0xFFFFF;
	if (s >= nslots || slots[s] != n) return;
	slots[s] = NULL; ++gens[s];
	if (nlibres == clibres) { clibres = clibres ? clibres * 2 : 64; libres = realloc(libres, sizeof(int) * (size_t)clibres); }
	libres[nlibres++] = s; --total_vivos;
}

int capi_tipo_por_nombre(const char *s) {
	for (int i = 0; i < T_N; ++i) if (!strcmp(CAPI_TIPOS[i], s)) return i;
	if (!strcmp(s, "Sprite2D")) return T_SPRITE;
	return -1;
}

int nodo_es2d(const Nodo *n) { return n->tipo != T_NODO && n->tipo != T_TEMP && n->tipo != T_AUDIO; }

/* ---------------------------------------------------------------------------------------------------- creación */
static void *datos_nuevos(Nodo *n) {
	switch (n->tipo) {
	case T_SPRITE: { DSprite *d = calloc(1, sizeof *d); d->hf = d->vf = 1; d->centrado = 1; d->vel = 1; d->act = -1; return d; }
	case T_MAPA: { DMapa *d = calloc(1, sizeof *d); d->cw = d->ch = 16; d->w = d->h = 0; d->capa_col = 1; d->mascara_col = 1; return d; }
	case T_CAMARA: { DCamara *d = calloc(1, sizeof *d); d->zoom = 1; d->suav = 0; return d; }
	case T_ETIQUETA: { DEtiqueta *d = calloc(1, sizeof *d); d->escala = 1; d->sombra = 1; return d; }
	case T_BOTON: {
		DBoton *d = calloc(1, sizeof *d); d->w = 80; d->h = 20; d->escala = 1; d->fijo = 1;
		uint8_t a[4] = { 60, 70, 100, 255 }, b[4] = { 90, 110, 160, 255 }, c[4] = { 40, 50, 80, 255 };
		memcpy(d->col_n, a, 4); memcpy(d->col_e, b, 4); memcpy(d->col_p, c, 4); return d;
	}
	case T_AREA: { DArea *d = calloc(1, sizeof *d); d->f.w = d->f.h = 16; d->f.radio = 8; d->f.capa = 1; d->f.mascara = 1; d->vigilar = 1; return d; }
	case T_CUERPO: {
		DCuerpo *d = calloc(1, sizeof *d); d->f.w = d->f.h = 16; d->f.radio = 8; d->f.capa = 1; d->f.mascara = 1; d->escala_grav = 1; d->friccion = 0;
		d->modo = 1; return d;
	}
	case T_PARTICULAS: {
		DParticulas *d = calloc(1, sizeof *d); d->hf = d->vf = 1; d->max = 128; d->cantidad = 16; d->vida = 1; d->v_min = 20; d->v_max = 60; d->angulo = -90;
		d->dispersion = 360; d->tam_ini = 3; d->tam_fin = 0; memset(d->col_ini, 255, 4); memset(d->col_fin, 255, 3); d->col_fin[3] = 0; d->sem = 12345;
		d->p = calloc((size_t)d->max, sizeof(Part)); return d;
	}
	case T_AUDIO: { DAudio *d = calloc(1, sizeof *d); d->vol = 1; d->voz = -1; return d; }
	case T_TEMP: { DTemp *d = calloc(1, sizeof *d); d->tiempo = 1; return d; }
	default: return NULL;
	}
}

Nodo *nodo_nuevo(CapiTipo t, const char *nombre) {
	Nodo *n = calloc(1, sizeof *n);
	if (!n) return NULL;
	n->tipo = t; n->sx = n->sy = 1; memset(n->mod, 255, 4);
	n->flags = F_VISIBLE | F_PROCESAR | (t == T_CUERPO || t == T_CAMARA ? F_INTERP : 0);
	n->ref_clase = n->ref_datos = LUA_NOREF;
	snprintf(n->nombre, sizeof n->nombre, "%s", nombre && *nombre ? nombre : CAPI_TIPOS[t]);
	n->d = datos_nuevos(n);
	n->id = registrar(n);
	return n;
}

/* ---------------------------------------------------------------------------------------------------- liberación */
static Nodo **cola; static int ncola, ccola;

static void liberar_datos(Nodo *n) {
	if (n->tipo == T_MAPA) {
		DMapa *m = n->d;
		for (int c = 0; c < m->ncapas; ++c) {
			if (m->capas[c].trozos) {
				for (int i = 0; i < m->tw * m->th; ++i) free(m->capas[c].trozos[i]);
				free(m->capas[c].trozos);
			}
		}
	} else if (n->tipo == T_AREA) { DArea *a = n->d; free(a->d); free(a->d_ant); }
	else if (n->tipo == T_PARTICULAS) free(((DParticulas *)n->d)->p);
	free(n->d); n->d = NULL;
}

void nodo_liberar_ya(Nodo *n) {
	if (!n) return;
	if (n->padre) nodo_quitar(n);
	for (int i = 0; i < n->nh; ++i) { n->hijos[i]->padre = NULL; nodo_liberar_ya(n->hijos[i]); }
	n->nh = 0;
	for (int i = 0; i < ncola; ++i) if (cola[i] == n) cola[i] = NULL;
	if (g_L) guion_dato_liberar(n);
	liberar_datos(n);
	free(n->hijos); free(n->con);
	desregistrar(n);
	free(n);
}

void nodo_encolar_liberar(Nodo *n) {
	if (!n || (n->flags & F_BORRAR)) return;
	n->flags |= F_BORRAR;
	if (ncola == ccola) { ccola = ccola ? ccola * 2 : 32; cola = realloc(cola, sizeof(Nodo *) * (size_t)ccola); }
	cola[ncola++] = n;
}

static void marcar_borrar(Nodo *n) { n->flags |= F_BORRAR; for (int i = 0; i < n->nh; ++i) marcar_borrar(n->hijos[i]); }

void nodo_vaciar_cola(void) {
	/* un nodo puede estar dentro de otro ya liberado: se resuelve por la marca y se libera solo el más alto */
	for (int i = 0; i < ncola; ++i) if (cola[i]) marcar_borrar(cola[i]);
	for (int i = 0; i < ncola; ++i) {
		Nodo *n = cola[i];
		if (!n) continue;
		/* si un antecesor también está en la cola, se libera con él */
		int dentro = 0;
		for (Nodo *p = n->padre; p; p = p->padre) { for (int j = 0; j < ncola; ++j) if (cola[j] == p) dentro = 1; }
		if (!dentro) nodo_liberar_ya(n);
	}
	/* los que quedaban (hijos de uno liberado ya se pusieron a NULL) */
	ncola = 0;
}

/* ---------------------------------------------------------------------------------------------------- jerarquía */
int nodo_en_arbol(const Nodo *n) { return n && (n->flags & F_ARBOL) != 0; }

static void marcar_arbol(Nodo *n, int si) {
	if (si) n->flags |= F_ARBOL; else n->flags &= ~(unsigned)F_ARBOL;
	for (int i = 0; i < n->nh; ++i) marcar_arbol(n->hijos[i], si);
}

void nodo_quitar(Nodo *n) {
	Nodo *p = n->padre;
	if (!p) return;
	for (int i = 0; i < p->nh; ++i) if (p->hijos[i] == n) { memmove(p->hijos + i, p->hijos + i + 1, sizeof(Nodo *) * (size_t)(p->nh - i - 1)); --p->nh; break; }
	n->padre = NULL;
	marcar_arbol(n, 0);
	++g_epoca;
}

void nodo_agregar(Nodo *p, Nodo *h) {
	if (!p || !h || h == p) return;
	if (h->padre) nodo_quitar(h);
	/* un nombre repetido entre hermanos se renombra (Nombre2, Nombre3...) para que las rutas valgan */
	char base[CAPI_NOME_MAX]; snprintf(base, sizeof base, "%s", h->nombre);
	for (int k = 2;; ++k) {
		int rep = 0;
		for (int i = 0; i < p->nh; ++i) if (!strcmp(p->hijos[i]->nombre, h->nombre)) { rep = 1; break; }
		if (!rep) break;
		snprintf(h->nombre, sizeof h->nombre, "%.40s%d", base, k);
	}
	if (p->nh == p->ch) { p->ch = p->ch ? p->ch * 2 : 4; p->hijos = realloc(p->hijos, sizeof(Nodo *) * (size_t)p->ch); }
	p->hijos[p->nh++] = h; h->padre = p;
	++g_epoca;
	if (p->flags & F_ARBOL) { marcar_arbol(h, 1); guion_listo(h); }
}

/* ---------------------------------------------------------------------------------------------------- rutas y grupos */
Nodo *nodo_buscar(Nodo *desde, const char *ruta) {
	if (!desde || !ruta) return NULL;
	Nodo *n = desde;
	if (*ruta == '/') { while (n->padre) n = n->padre; ++ruta; }
	char parte[CAPI_NOME_MAX];
	while (*ruta && n) {
		size_t l = 0;
		while (ruta[l] && ruta[l] != '/') ++l;
		if (l >= sizeof parte) return NULL;
		memcpy(parte, ruta, l); parte[l] = 0;
		ruta += l; if (*ruta == '/') ++ruta;
		if (!strcmp(parte, ".") || !*parte) continue;
		if (!strcmp(parte, "..")) { n = n->padre; continue; }
		Nodo *h = NULL;
		for (int i = 0; i < n->nh; ++i) if (!strcmp(n->hijos[i]->nombre, parte)) { h = n->hijos[i]; break; }
		n = h;
	}
	return n;
}

Nodo *nodo_buscar_nombre(Nodo *d, const char *nombre) {
	if (!d) return NULL;
	if (!strcmp(d->nombre, nombre)) return d;
	for (int i = 0; i < d->nh; ++i) { Nodo *r = nodo_buscar_nombre(d->hijos[i], nombre); if (r) return r; }
	return NULL;
}

int nodo_en_grupo(const Nodo *n, const char *g) { for (int i = 0; i < n->ng; ++i) if (!strcmp(n->grupos[i], g)) return 1; return 0; }
void nodo_grupo_agregar(Nodo *n, const char *g) {
	if (nodo_en_grupo(n, g) || n->ng >= 4) return;
	snprintf(n->grupos[n->ng++], sizeof n->grupos[0], "%s", g);
}
void nodo_grupo_quitar(Nodo *n, const char *g) {
	for (int i = 0; i < n->ng; ++i) if (!strcmp(n->grupos[i], g)) { memmove(n->grupos[i], n->grupos[i + 1], sizeof n->grupos[0] * (size_t)(n->ng - i - 1)); --n->ng; return; }
}

void nodo_recorrer(Nodo *r, void (*f)(Nodo *, void *), void *u) {
	if (!r) return;
	f(r, u);
	for (int i = 0; i < r->nh; ++i) nodo_recorrer(r->hijos[i], f, u);
}
static void c_cuenta(Nodo *n, void *u) { (void)n; ++*(int *)u; }
int nodo_contar(Nodo *r) { int c = 0; nodo_recorrer(r, c_cuenta, &c); return c; }

/* ---------------------------------------------------------------------------------------------------- matrices */
void nodo_moverse(Nodo *n) { (void)n; ++g_epoca; }

static void local(const Nodo *n, float x, float y, float o[6]) {
	double r = (double)n->rot * 3.14159265358979 / 180.0, cs = cos(r), sn = sin(r);
	o[0] = (float)(cs * n->sx); o[1] = (float)(sn * n->sx); o[2] = (float)(-sn * n->sy); o[3] = (float)(cs * n->sy); o[4] = x; o[5] = y;
}
static void mul(const float p[6], const float l[6], float o[6]) {
	float a = p[0] * l[0] + p[2] * l[1], b = p[1] * l[0] + p[3] * l[1];
	float c = p[0] * l[2] + p[2] * l[3], d = p[1] * l[2] + p[3] * l[3];
	float tx = p[0] * l[4] + p[2] * l[5] + p[4], ty = p[1] * l[4] + p[3] * l[5] + p[5];
	o[0] = a; o[1] = b; o[2] = c; o[3] = d; o[4] = tx; o[5] = ty;
}
static const float IDENT[6] = { 1, 0, 0, 1, 0, 0 };

void nodo_matriz(Nodo *n, float out[6]) {
	if (n->m_ep != g_epoca) {
		float l[6], pm[6];
		if (n->padre) nodo_matriz(n->padre, pm); else memcpy(pm, IDENT, sizeof pm);
		if (nodo_es2d(n)) { local(n, n->x, n->y, l); mul(pm, l, n->m); } else memcpy(n->m, pm, sizeof pm);
		n->m_ep = g_epoca;
	}
	memcpy(out, n->m, sizeof n->m);
}

static unsigned long long g_cuadro_dib; static float g_alpha_dib = 1;
void nodo_fijar_cuadro_dibujo(unsigned long long c, float alpha) { g_cuadro_dib = c; g_alpha_dib = alpha; }

void nodo_matriz_dibujo(Nodo *n, float out[6]) {
	if (n->mr_ep != g_cuadro_dib + 1 + ((unsigned long long)g_epoca << 32)) {
		float l[6], pm[6];
		if (n->padre) nodo_matriz_dibujo(n->padre, pm); else memcpy(pm, IDENT, sizeof pm);
		if (nodo_es2d(n)) {
			float ix = n->x, iy = n->y;
			if (n->flags & F_INTERP) { ix = n->ax + (n->x - n->ax) * g_alpha_dib; iy = n->ay + (n->y - n->ay) * g_alpha_dib; }
			local(n, ix, iy, l); mul(pm, l, n->mr);
		} else memcpy(n->mr, pm, sizeof pm);
		n->mr_ep = g_cuadro_dib + 1 + ((unsigned long long)g_epoca << 32);
	}
	memcpy(out, n->mr, sizeof n->mr);
}

void nodo_pos_global(Nodo *n, float *x, float *y) { float m[6]; nodo_matriz(n, m); if (x) *x = m[4]; if (y) *y = m[5]; }

void nodo_poner_pos_global(Nodo *n, float gx, float gy) {
	if (!n->padre) { n->x = gx; n->y = gy; ++g_epoca; return; }
	float m[6]; nodo_matriz(n->padre, m);
	float det = m[0] * m[3] - m[1] * m[2];
	if (fabsf(det) < 1e-9f) { n->x = gx; n->y = gy; ++g_epoca; return; }
	float dx = gx - m[4], dy = gy - m[5];
	n->x = (m[3] * dx - m[2] * dy) / det; n->y = (-m[1] * dx + m[0] * dy) / det;
	++g_epoca;
}

void nodo_teletransportar(Nodo *n, float x, float y) { n->x = n->ax = x; n->y = n->ay = y; ++g_epoca; }

/* ---------------------------------------------------------------------------------------------------- utilidades */
void (*g_capi_log_hook)(const char *);   /* D460: el editor recoge los avisos del cargador en su consola */
void capi_log(const char *fmt, ...) {
	char b[512]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
	if (g_capi_log_hook) { g_capi_log_hook(b); return; }
	printf("[CAPI] %s\n", b); fflush(stdout);
}
uint32_t capi_rnd(uint32_t *s) { uint32_t x = *s ? *s : 0x9E3779B9u; x ^= x << 13; x ^= x >> 17; x ^= x << 5; *s = x; return x; }
