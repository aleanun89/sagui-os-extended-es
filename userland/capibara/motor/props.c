/* SPDX-License-Identifier: MIT */
/* props.c - tabla de propiedades de los nodos. La usan el cargador de escenas (clave = valor) y Lua (nodo.clave). */
#include "capi.h"
#include <ctype.h>
#include <stddef.h>

enum { K_F, K_I, K_B, K_S, K_C, K_V2, K_V2I, K_V4, K_FLAG, K_H };
enum { B_N, B_D };   /* base: el nodo o sus datos */
#define M(t) (1u << (t))
#define M_2D (M(T_NODO2D) | M(T_SPRITE) | M(T_MAPA) | M(T_CAMARA) | M(T_ETIQUETA) | M(T_BOTON) | M(T_AREA) | M(T_CUERPO) | M(T_PARTICULAS))
#define M_TODOS 0xFFFFu

typedef struct { const char *nom; int kind; unsigned mask; int base; size_t off; int aux; void (*h)(Nodo *, const Valor *); } PropDef;

static void h_forma(Nodo *n, const Valor *v);
static void h_modo(Nodo *n, const Valor *v);
static void h_alineado(Nodo *n, const Valor *v);
static void h_anim(Nodo *n, const Valor *v);
static void h_solidas(Nodo *n, const Valor *v);
static void h_grupo(Nodo *n, const Valor *v);
static void h_maximo(Nodo *n, const Valor *v);
static void h_semilla(Nodo *n, const Valor *v);
static void h_animar(Nodo *n, const Valor *v);
static void h_mapa_tam(Nodo *n, const Valor *v);

#define ND(f) B_N, offsetof(Nodo, f)
#define DT(T, f) B_D, offsetof(T, f)
static const PropDef P[] = {
	{ "visible", K_FLAG, M_TODOS, B_N, 0, F_VISIBLE, 0 },
	{ "procesar", K_FLAG, M_TODOS, B_N, 0, F_PROCESAR, 0 },
	{ "interpolar", K_FLAG, M_TODOS, B_N, 0, F_INTERP, 0 },
	{ "grupo", K_H, M_TODOS, B_N, 0, 0, h_grupo },
	{ "grupos", K_H, M_TODOS, B_N, 0, 0, h_grupo },
	{ "guion", K_S, M_TODOS, ND(guion), sizeof(((Nodo *)0)->guion), 0 },
	{ "x", K_F, M_2D, ND(x), 0, 0 }, { "y", K_F, M_2D, ND(y), 0, 0 },
	{ "posicion", K_V2, M_2D, ND(x), 0, 0 },
	{ "rotacion", K_F, M_2D, ND(rot), 0, 0 },
	{ "escala_x", K_F, M_2D, ND(sx), 0, 0 }, { "escala_y", K_F, M_2D, ND(sy), 0, 0 },
	{ "escala", K_V2, M_2D, ND(sx), 0, 0 },
	{ "z", K_I, M_2D, ND(z), 0, 0 },
	{ "color", K_C, M_2D, ND(mod), 0, 0 },
	/* Sprite */
	{ "textura", K_S, M(T_SPRITE), DT(DSprite, ruta), sizeof(((DSprite *)0)->ruta), 0 },
	{ "cuadricula", K_V2I, M(T_SPRITE), DT(DSprite, hf), 0, 0 },
	{ "fotograma", K_I, M(T_SPRITE), DT(DSprite, fot), 0, 0 },
	{ "region", K_V4, M(T_SPRITE), DT(DSprite, rx), 1, 0 },   /* aux=1: cuatro enteros */
	{ "espejo_h", K_B, M(T_SPRITE), DT(DSprite, volt_h), 0, 0 }, { "espejo_v", K_B, M(T_SPRITE), DT(DSprite, volt_v), 0, 0 },
	{ "centrado", K_B, M(T_SPRITE), DT(DSprite, centrado), 0, 0 },
	{ "desplazamiento", K_V2, M(T_SPRITE), DT(DSprite, ofx), 0, 0 },
	{ "velocidad_anim", K_F, M(T_SPRITE), DT(DSprite, vel), 0, 0 },
	{ "animacion", K_H, M(T_SPRITE), B_N, 0, 0, h_anim },
	{ "animar", K_H, M(T_SPRITE), B_N, 0, 0, h_animar },
	/* MapaCasillas */
	{ "textura", K_S, M(T_MAPA), DT(DMapa, ruta), sizeof(((DMapa *)0)->ruta), 0 },
	{ "casilla", K_V2I, M(T_MAPA), DT(DMapa, cw), 0, 0 },
	{ "separacion", K_I, M(T_MAPA), DT(DMapa, sep), 0, 0 },
	{ "tamano", K_H, M(T_MAPA), B_N, 0, 0, h_mapa_tam },
	{ "solidas", K_H, M(T_MAPA), B_N, 0, 0, h_solidas },
	{ "capa", K_I, M(T_MAPA), DT(DMapa, capa_col), 0, 0 }, { "mascara", K_I, M(T_MAPA), DT(DMapa, mascara_col), 0, 0 },
	/* Camara2D */
	{ "actual", K_B, M(T_CAMARA), DT(DCamara, actual), 0, 0 }, { "zoom", K_F, M(T_CAMARA), DT(DCamara, zoom), 0, 0 },
	{ "suavizado", K_F, M(T_CAMARA), DT(DCamara, suav), 0, 0 },
	{ "limites", K_V4, M(T_CAMARA), DT(DCamara, limites), 0, 0 },
	{ "seguir", K_S, M(T_CAMARA), DT(DCamara, seguir_ruta), sizeof(((DCamara *)0)->seguir_ruta), 0 },
	{ "usar_limites", K_B, M(T_CAMARA), DT(DCamara, con_limites), 0, 0 },
	{ "desplazamiento", K_V2, M(T_CAMARA), DT(DCamara, dx), 0, 0 },
	/* Etiqueta */
	{ "texto", K_S, M(T_ETIQUETA), DT(DEtiqueta, texto), sizeof(((DEtiqueta *)0)->texto), 0 },
	{ "escala_texto", K_I, M(T_ETIQUETA), DT(DEtiqueta, escala), 0, 0 },
	{ "alineado", K_H, M(T_ETIQUETA), B_N, 0, 0, h_alineado },
	{ "ancho", K_F, M(T_ETIQUETA), DT(DEtiqueta, ancho), 0, 0 },
	{ "sombra", K_B, M(T_ETIQUETA), DT(DEtiqueta, sombra), 0, 0 },
	{ "fijo", K_B, M(T_ETIQUETA), DT(DEtiqueta, fijo), 0, 0 },
	/* Boton */
	{ "texto", K_S, M(T_BOTON), DT(DBoton, texto), sizeof(((DBoton *)0)->texto), 0 },
	{ "tamano", K_V2, M(T_BOTON), DT(DBoton, w), 0, 0 },
	{ "escala_texto", K_I, M(T_BOTON), DT(DBoton, escala), 0, 0 },
	{ "fijo", K_B, M(T_BOTON), DT(DBoton, fijo), 0, 0 },
	{ "deshabilitado", K_B, M(T_BOTON), DT(DBoton, deshabilitado), 0, 0 },
	{ "color_normal", K_C, M(T_BOTON), DT(DBoton, col_n), 0, 0 }, { "color_encima", K_C, M(T_BOTON), DT(DBoton, col_e), 0, 0 },
	{ "color_pulsado", K_C, M(T_BOTON), DT(DBoton, col_p), 0, 0 },
	/* Area y Cuerpo */
	{ "forma", K_H, M(T_AREA) | M(T_CUERPO), B_N, 0, 0, h_forma },
	{ "desplazamiento_forma", K_V2, M(T_AREA), DT(DArea, f.ox), 0, 0 },
	{ "desplazamiento_forma", K_V2, M(T_CUERPO), DT(DCuerpo, f.ox), 0, 0 },
	{ "capa", K_I, M(T_AREA), DT(DArea, f.capa), 0, 0 }, { "mascara", K_I, M(T_AREA), DT(DArea, f.mascara), 0, 0 },
	{ "capa", K_I, M(T_CUERPO), DT(DCuerpo, f.capa), 0, 0 }, { "mascara", K_I, M(T_CUERPO), DT(DCuerpo, f.mascara), 0, 0 },
	{ "vigilar", K_B, M(T_AREA), DT(DArea, vigilar), 0, 0 },
	{ "modo", K_H, M(T_CUERPO), B_N, 0, 0, h_modo },
	{ "vx", K_F, M(T_CUERPO), DT(DCuerpo, vx), 0, 0 }, { "vy", K_F, M(T_CUERPO), DT(DCuerpo, vy), 0, 0 },
	{ "escala_gravedad", K_F, M(T_CUERPO), DT(DCuerpo, escala_grav), 0, 0 },
	{ "rebote", K_F, M(T_CUERPO), DT(DCuerpo, rebote), 0, 0 }, { "friccion", K_F, M(T_CUERPO), DT(DCuerpo, friccion), 0, 0 },
	/* Particulas */
	{ "textura", K_S, M(T_PARTICULAS), DT(DParticulas, ruta), sizeof(((DParticulas *)0)->ruta), 0 },
	{ "cuadricula", K_V2I, M(T_PARTICULAS), DT(DParticulas, hf), 0, 0 }, { "fotograma", K_I, M(T_PARTICULAS), DT(DParticulas, fot), 0, 0 },
	{ "emitiendo", K_B, M(T_PARTICULAS), DT(DParticulas, emitiendo), 0, 0 }, { "una_vez", K_B, M(T_PARTICULAS), DT(DParticulas, una_vez), 0, 0 },
	{ "cantidad", K_I, M(T_PARTICULAS), DT(DParticulas, cantidad), 0, 0 }, { "maximo", K_H, M(T_PARTICULAS), B_N, 0, 0, h_maximo },
	{ "vida", K_F, M(T_PARTICULAS), DT(DParticulas, vida), 0, 0 }, { "vida_azar", K_F, M(T_PARTICULAS), DT(DParticulas, vida_az), 0, 0 },
	{ "velocidad", K_V2, M(T_PARTICULAS), DT(DParticulas, v_min), 0, 0 },
	{ "angulo", K_F, M(T_PARTICULAS), DT(DParticulas, angulo), 0, 0 }, { "dispersion", K_F, M(T_PARTICULAS), DT(DParticulas, dispersion), 0, 0 },
	{ "gravedad", K_F, M(T_PARTICULAS), DT(DParticulas, grav), 0, 0 },
	{ "tamano_part", K_V2, M(T_PARTICULAS), DT(DParticulas, tam_ini), 0, 0 },
	{ "color_inicial", K_C, M(T_PARTICULAS), DT(DParticulas, col_ini), 0, 0 }, { "color_final", K_C, M(T_PARTICULAS), DT(DParticulas, col_fin), 0, 0 },
	{ "aditivo", K_B, M(T_PARTICULAS), DT(DParticulas, aditivo), 0, 0 },
	{ "semilla", K_H, M(T_PARTICULAS), B_N, 0, 0, h_semilla },
	/* ReproductorAudio */
	{ "sonido", K_S, M(T_AUDIO), DT(DAudio, ruta), sizeof(((DAudio *)0)->ruta), 0 },
	{ "volumen", K_F, M(T_AUDIO), DT(DAudio, vol), 0, 0 }, { "bucle", K_B, M(T_AUDIO), DT(DAudio, bucle), 0, 0 },
	{ "musica", K_B, M(T_AUDIO), DT(DAudio, musica), 0, 0 }, { "autoreproducir", K_B, M(T_AUDIO), DT(DAudio, autoplay), 0, 0 },
	/* Temporizador */
	{ "tiempo", K_F, M(T_TEMP), DT(DTemp, tiempo), 0, 0 }, { "una_vez", K_B, M(T_TEMP), DT(DTemp, una_vez), 0, 0 },
	{ "autoinicio", K_B, M(T_TEMP), DT(DTemp, autoinicio), 0, 0 },
};
#define NP ((int)(sizeof P / sizeof P[0]))

static const PropDef *buscar(CapiTipo t, const char *nom) {
	for (int i = 0; i < NP; ++i) if ((P[i].mask & M(t)) && !strcmp(P[i].nom, nom)) return &P[i];
	return NULL;
}
int prop_existe(CapiTipo t, const char *nom) { return buscar(t, nom) != NULL; }
int prop_es_entero(CapiTipo t, const char *nom) { const PropDef *p = buscar(t, nom); return p && p->kind == K_I; }
int prop_es_bool(CapiTipo t, const char *nom) { const PropDef *p = buscar(t, nom); return p && (p->kind == K_B || p->kind == K_FLAG); }

static void *ptr(Nodo *n, const PropDef *p) { return (char *)(p->base == B_N ? (void *)n : n->d) + p->off; }

static int num(const Valor *v, int i, double *o) { if (i >= v->n || v->t[i].str) return 0; *o = v->t[i].num; return 1; }

static int hexd(int c) { return c >= '0' && c <= '9' ? c - '0' : (tolower(c) >= 'a' && tolower(c) <= 'f' ? tolower(c) - 'a' + 10 : -1); }
static int color_de(const Valor *v, uint8_t o[4]) {
	if (v->n >= 1 && v->t[0].str) {
		const char *s = v->t[0].s;
		if (*s == '#') ++s;
		size_t l = strlen(s);
		if (l != 6 && l != 8) return 0;
		uint8_t c[4] = { 0, 0, 0, 255 };
		for (size_t i = 0; i < l / 2; ++i) { int a = hexd(s[2 * i]), b = hexd(s[2 * i + 1]); if (a < 0 || b < 0) return 0; c[i] = (uint8_t)(a * 16 + b); }
		memcpy(o, c, 4); return 1;
	}
	double d[4] = { 0, 0, 0, 255 };
	if (v->n < 3) return 0;
	for (int i = 0; i < v->n && i < 4; ++i) if (!num(v, i, &d[i])) return 0;
	for (int i = 0; i < 4; ++i) o[i] = (uint8_t)(d[i] < 0 ? 0 : d[i] > 255 ? 255 : d[i]);
	return 1;
}

int prop_poner(Nodo *n, const char *nom, const Valor *v) {
	const PropDef *p = buscar(n->tipo, nom);
	if (!p) return 0;
	double a = 0, b = 0, c = 0, d = 0;
	if (p->kind == K_H) { if (p->h) p->h(n, v); prop_tras_cambiar(n, nom); return 1; }
	void *q = ptr(n, p);
	switch (p->kind) {
	case K_F: if (!num(v, 0, &a)) return 0; *(float *)q = (float)a; break;
	case K_I: if (!num(v, 0, &a)) return 0; *(int *)q = (int)a; break;
	case K_B: if (!num(v, 0, &a)) return 0; *(int *)q = a != 0; break;
	case K_FLAG: if (!num(v, 0, &a)) return 0; if (a != 0) n->flags |= (unsigned)p->aux; else n->flags &= ~(unsigned)p->aux; break;
	case K_S: if (!v->n) return 0; snprintf((char *)q, (size_t)p->aux, "%s", v->t[0].s); break;
	case K_C: if (!color_de(v, (uint8_t *)q)) return 0; break;
	case K_V2: if (!num(v, 0, &a)) return 0; b = a; num(v, 1, &b); ((float *)q)[0] = (float)a; ((float *)q)[1] = (float)b; break;
	case K_V2I: if (!num(v, 0, &a) || !num(v, 1, &b)) return 0; ((int *)q)[0] = (int)a; ((int *)q)[1] = (int)b; break;
	case K_V4:
		if (!num(v, 0, &a) || !num(v, 1, &b) || !num(v, 2, &c) || !num(v, 3, &d)) return 0;
		if (p->aux == 1) { ((int *)q)[0] = (int)a; ((int *)q)[1] = (int)b; ((int *)q)[2] = (int)c; ((int *)q)[3] = (int)d; }
		else { ((float *)q)[0] = (float)a; ((float *)q)[1] = (float)b; ((float *)q)[2] = (float)c; ((float *)q)[3] = (float)d; }
		break;
	}
	prop_tras_cambiar(n, nom);
	return 1;
}

void prop_tras_cambiar(Nodo *n, const char *nom) {
	++g_epoca;
	if (!strcmp(nom, "textura")) {
		if (n->tipo == T_SPRITE) { DSprite *d = n->d; d->tex = d->ruta[0] ? rec_textura(d->ruta) : NULL; }
		else if (n->tipo == T_MAPA) {
			DMapa *d = n->d; d->tex = d->ruta[0] ? rec_textura(d->ruta) : NULL;
			if (d->tex) d->cols = (d->tex->w + d->sep) / (d->cw + d->sep);
		} else if (n->tipo == T_PARTICULAS) { DParticulas *d = n->d; d->tex = d->ruta[0] ? rec_textura(d->ruta) : NULL; }
	} else if ((!strcmp(nom, "casilla") || !strcmp(nom, "separacion")) && n->tipo == T_MAPA) {
		DMapa *d = n->d; if (d->tex && d->cw > 0) d->cols = (d->tex->w + d->sep) / (d->cw + d->sep);
	} else if (!strcmp(nom, "sonido") && n->tipo == T_AUDIO) { DAudio *d = n->d; d->snd = d->ruta[0] && !g_modo_editor ? rec_sonido(d->ruta) : NULL; }
	else if (!strcmp(nom, "x") || !strcmp(nom, "y") || !strcmp(nom, "posicion")) ++n->cuenta_mov;
}

int prop_obtener_num(Nodo *n, const char *nom, double *out) {
	if (!strcmp(nom, "id")) { *out = n->id; return 1; }
	const PropDef *p = buscar(n->tipo, nom);
	if (!p) return 0;
	if (p->kind == K_FLAG) { *out = (n->flags & (unsigned)p->aux) != 0; return 1; }
	if (p->kind == K_H) {
		if (n->tipo == T_CUERPO && !strcmp(nom, "modo")) { *out = ((DCuerpo *)n->d)->modo; return 1; }
		if (n->tipo == T_MAPA && !strcmp(nom, "tamano")) return 0;
		return 0;
	}
	void *q = ptr(n, p);
	switch (p->kind) {
	case K_F: *out = *(float *)q; return 1;
	case K_I: case K_B: *out = *(int *)q; return 1;
	default: return 0;
	}
}

int prop_obtener_str(Nodo *n, const char *nom, char *out, size_t no) {
	const PropDef *p = buscar(n->tipo, nom);
	if (!p || p->kind != K_S) return 0;
	snprintf(out, no, "%s", (char *)ptr(n, p));
	return 1;
}

/* ---------------------------------------------------------------------------------------------------- manejadores */
static void h_grupo(Nodo *n, const Valor *v) { for (int i = 0; i < v->n; ++i) nodo_grupo_agregar(n, v->t[i].s); }

static void h_forma(Nodo *n, const Valor *v) {
	Forma *f = n->tipo == T_AREA ? &((DArea *)n->d)->f : &((DCuerpo *)n->d)->f;
	if (!v->n) return;
	if (!strcmp(v->t[0].s, "circulo")) { f->forma = 1; f->radio = v->n > 1 ? (float)v->t[1].num : 8; f->w = f->h = f->radio * 2; }
	else { f->forma = 0; f->w = v->n > 1 ? (float)v->t[1].num : 16; f->h = v->n > 2 ? (float)v->t[2].num : f->w; f->radio = f->w / 2; }
}
static void h_modo(Nodo *n, const Valor *v) {
	DCuerpo *d = n->d; if (!v->n) return;
	if (!strcmp(v->t[0].s, "estatico")) d->modo = 0; else if (!strcmp(v->t[0].s, "dinamico")) d->modo = 2; else d->modo = 1;
}
static void h_alineado(Nodo *n, const Valor *v) {
	DEtiqueta *d = n->d; if (!v->n) return;
	d->alineado = !strcmp(v->t[0].s, "centro") ? 1 : !strcmp(v->t[0].s, "derecha") ? 2 : 0;
}
static void h_anim(Nodo *n, const Valor *v) {
	/* animacion = "correr" 8 si 1 2 3   (nombre, fotogramas por segundo, bucle, lista de fotogramas) */
	DSprite *d = n->d;
	if (v->n < 4 || d->nanims >= 12) return;
	Anim *a = &d->anims[d->nanims++];
	memset(a, 0, sizeof *a);
	snprintf(a->nome, sizeof a->nome, "%s", v->t[0].s);
	a->fps = (float)v->t[1].num; a->bucle = v->t[2].num != 0;
	for (int i = 3; i < v->n && a->n < 48; ++i) a->fr[a->n++] = (int)v->t[i].num;
}
static void h_animar(Nodo *n, const Valor *v) { if (v->n) sprite_animar(n, v->t[0].s); }
static void h_solidas(Nodo *n, const Valor *v) {
	DMapa *m = n->d; memset(m->solidas, 0, sizeof m->solidas); m->usa_solidas = v->n > 0;
	for (int i = 0; i < v->n; ++i) { int k = (int)v->t[i].num; if (k >= 0 && k < 1024) m->solidas[k] = 1; }
}
static void h_maximo(Nodo *n, const Valor *v) {
	DParticulas *d = n->d; if (!v->n) return;
	int m = (int)v->t[0].num; if (m < 1) m = 1; if (m > 4096) m = 4096;
	d->p = realloc(d->p, sizeof(Part) * (size_t)m); memset(d->p, 0, sizeof(Part) * (size_t)m); d->max = m; d->np = 0;
}
static void h_semilla(Nodo *n, const Valor *v) { if (v->n) ((DParticulas *)n->d)->sem = (uint32_t)v->t[0].num; }
static void h_mapa_tam(Nodo *n, const Valor *v) {
	/* tamano = ancho alto  (en celdas): reserva el mapa; las capas se crean después */
	DMapa *m = n->d; if (v->n < 2) return;
	m->w = (int)v->t[0].num; m->h = (int)v->t[1].num;
	m->tw = (m->w + MAPA_TROZO - 1) / MAPA_TROZO; m->th = (m->h + MAPA_TROZO - 1) / MAPA_TROZO;
}

/* ---------------------------------------------------------------------------------------------------- introspección (D460) */
/* Límites y etiquetas para el editor: por nombre de propiedad; lo que no esté aquí usa los de su tipo. */
static const struct { const char *nom; float min, max, paso; const char *etiq; } LIM[] = {
	{ "rotacion", -3600, 3600, 1, NULL }, { "escala", -100, 100, 0.05f, "x|y" }, { "posicion", -100000, 100000, 1, "x|y" },
	{ "z", -1000, 1000, 1, NULL }, { "fotograma", 0, 4095, 1, NULL }, { "cuadricula", 1, 256, 1, "columnas|filas" },
	{ "region", 0, 8192, 1, "x|y|ancho|alto" }, { "desplazamiento", -10000, 10000, 1, "x|y" }, { "velocidad_anim", 0, 20, 0.1f, NULL },
	{ "casilla", 1, 512, 1, "ancho|alto" }, { "separacion", 0, 64, 1, NULL }, { "capa", 0, 65535, 1, NULL }, { "mascara", 0, 65535, 1, NULL },
	{ "zoom", 0.1f, 10, 0.1f, NULL }, { "suavizado", 0, 100, 0.5f, NULL }, { "limites", -100000, 100000, 1, "izq|arriba|der|abajo" },
	{ "escala_texto", 1, 16, 1, NULL }, { "ancho", 0, 4096, 1, NULL }, { "tamano", 0, 4096, 1, "ancho|alto" },
	{ "desplazamiento_forma", -1000, 1000, 1, "x|y" }, { "escala_gravedad", -10, 10, 0.1f, NULL }, { "rebote", 0, 1, 0.05f, NULL },
	{ "friccion", 0, 1, 0.05f, NULL }, { "cantidad", 1, 1024, 1, NULL }, { "vida", 0.01f, 60, 0.1f, NULL }, { "vida_azar", 0, 1, 0.05f, NULL },
	{ "velocidad", -2000, 2000, 1, "min|max" }, { "angulo", -360, 360, 1, NULL }, { "dispersion", 0, 360, 1, NULL },
	{ "gravedad", -5000, 5000, 10, NULL }, { "tamano_part", 0, 256, 0.5f, "ini|fin" }, { "volumen", 0, 1, 0.05f, NULL },
	{ "tiempo", 0, 3600, 0.1f, NULL },
};

int prop_listar(CapiTipo t, int i, PropInfo *o) {
	int k = -1;
	for (int j = 0; j < NP; ++j) {
		if (!(P[j].mask & M(t))) continue;
		if (++k != i) continue;
		const PropDef *p = &P[j];
		memset(o, 0, sizeof *o);
		o->nombre = p->nom; o->n = 1; o->min = -100000; o->max = 100000; o->paso = 1;
		switch (p->kind) {
		case K_F: o->tipo = PK_FLOAT; break;
		case K_I: o->tipo = PK_INT; break;
		case K_B: case K_FLAG: o->tipo = PK_BOOL; break;
		case K_S: o->tipo = PK_TEXT; break;
		case K_C: o->tipo = PK_COLOR; break;
		case K_V2: o->tipo = PK_V2; o->n = 2; break;
		case K_V2I: o->tipo = PK_V2I; o->n = 2; break;
		case K_V4: o->tipo = p->aux == 1 ? PK_V4I : PK_V4; o->n = 4; break;
		default: o->tipo = PK_LISTA; break;
		}
		if (p->kind == K_H) {
			if (!strcmp(p->nom, "modo")) { o->tipo = PK_ENUM; o->opciones = "estatico|cinematico|dinamico"; }
			else if (!strcmp(p->nom, "alineado")) { o->tipo = PK_ENUM; o->opciones = "izquierda|centro|derecha"; }
			else if (!strcmp(p->nom, "maximo")) { o->tipo = PK_INT; o->min = 1; o->max = 4096; }
			else if (!strcmp(p->nom, "semilla")) { o->tipo = PK_INT; o->min = 0; o->max = 1000000000; }
		}
		for (unsigned q = 0; q < sizeof LIM / sizeof LIM[0]; ++q) if (!strcmp(LIM[q].nom, p->nom)) {
			/* "tamano" y "desplazamiento" repiten nombre en varios tipos con la misma forma: sirve el mismo límite */
			o->min = LIM[q].min; o->max = LIM[q].max; o->paso = LIM[q].paso; o->etiquetas = LIM[q].etiq;
			if (!strcmp(p->nom, "tamano") && p->kind == K_H) o->etiquetas = "ancho|alto";
			break;
		}
		if (!strcmp(p->nom, "tamano") && p->kind == K_H) { o->tipo = PK_V2I; o->n = 2; o->min = 1; o->max = 4096; o->etiquetas = "ancho|alto"; }
		if (!strcmp(p->nom, "textura")) o->ruta = 1;
		else if (!strcmp(p->nom, "sonido")) o->ruta = 2;
		else if (!strcmp(p->nom, "guion")) o->ruta = 3;
		if (!strcmp(p->nom, "x") || !strcmp(p->nom, "y") || !strcmp(p->nom, "escala_x") || !strcmp(p->nom, "escala_y") || !strcmp(p->nom, "grupos") ||
		    !strcmp(p->nom, "animacion")) o->oculta = 1;
		/* grupo del inspector */
		static const char *const G0[] = { "visible", "procesar", "interpolar", "grupo", "guion", NULL };
		static const char *const G1[] = { "posicion", "rotacion", "escala", "z", NULL };
		o->grupo = 3;
		for (int q = 0; G0[q]; ++q) if (!strcmp(G0[q], p->nom)) o->grupo = 0;
		for (int q = 0; G1[q]; ++q) if (!strcmp(G1[q], p->nom)) o->grupo = 1;
		if (!strcmp(p->nom, "color")) o->grupo = 2;
		return 1;
	}
	return 0;
}

static void fmt_num(char *o, size_t no, double v) {
	char b[40]; snprintf(b, sizeof b, "%.4f", v);
	char *e = b + strlen(b);
	while (e > b && e[-1] == '0') *--e = 0;
	if (e > b && e[-1] == '.') *--e = 0;
	if (!strcmp(b, "-0")) strcpy(b, "0");
	snprintf(o, no, "%s", b);
}

static void fmt_cad(char *o, size_t no, const char *s) {
	size_t n = 0;
	if (no < 3) return;
	o[n++] = '"';
	for (; *s && n + 3 < no; ++s) { if (*s == '"' || *s == '\\') o[n++] = '\\'; if (*s == '\n') { o[n++] = '\\'; o[n++] = 'n'; continue; } o[n++] = *s; }
	o[n++] = '"'; o[n] = 0;
}

int prop_texto(Nodo *n, const char *nom, char *out, size_t no) {
	const PropDef *p = buscar(n->tipo, nom);
	if (!p) return 0;
	out[0] = 0;
	if (p->kind == K_FLAG) { snprintf(out, no, "%s", (n->flags & (unsigned)p->aux) ? "si" : "no"); return 1; }
	if (p->kind == K_H) {
		if (n->tipo == T_CUERPO && !strcmp(nom, "modo")) { static const char *const m[] = { "estatico", "cinematico", "dinamico" }; snprintf(out, no, "%s", m[((DCuerpo *)n->d)->modo % 3]); }
		else if (n->tipo == T_ETIQUETA && !strcmp(nom, "alineado")) { static const char *const m[] = { "izquierda", "centro", "derecha" }; snprintf(out, no, "%s", m[((DEtiqueta *)n->d)->alineado % 3]); }
		else if (!strcmp(nom, "forma")) {
			Forma *f = n->tipo == T_AREA ? &((DArea *)n->d)->f : &((DCuerpo *)n->d)->f; char a[24], b[24];
			if (f->forma) { fmt_num(a, sizeof a, f->radio); snprintf(out, no, "circulo %s", a); }
			else { fmt_num(a, sizeof a, f->w); fmt_num(b, sizeof b, f->h); snprintf(out, no, "caja %s %s", a, b); }
		}
		else if (n->tipo == T_MAPA && !strcmp(nom, "tamano")) snprintf(out, no, "%d %d", ((DMapa *)n->d)->w, ((DMapa *)n->d)->h);
		else if (n->tipo == T_MAPA && !strcmp(nom, "solidas")) {
			DMapa *m = n->d; size_t l = 0;
			if (m->usa_solidas) for (int k = 0; k < 1024 && l + 8 < no; ++k) if (m->solidas[k]) l += (size_t)snprintf(out + l, no - l, "%s%d", l ? " " : "", k);
		}
		else if (!strcmp(nom, "grupo")) { size_t l = 0; for (int k = 0; k < n->ng; ++k) l += (size_t)snprintf(out + l, no - l, "%s%s", k ? " " : "", n->grupos[k]); }
		else if (n->tipo == T_PARTICULAS && !strcmp(nom, "maximo")) snprintf(out, no, "%d", ((DParticulas *)n->d)->max);
		else if (n->tipo == T_PARTICULAS && !strcmp(nom, "semilla")) snprintf(out, no, "%u", (unsigned)((DParticulas *)n->d)->sem);
		return 1;
	}
	void *q = ptr(n, p);
	char a[24], b[24], c[24], d[24];
	switch (p->kind) {
	case K_F: fmt_num(out, no, *(float *)q); break;
	case K_I: case K_B: snprintf(out, no, p->kind == K_B ? (*(int *)q ? "si" : "no") : "%d", *(int *)q); break;
	case K_S: fmt_cad(out, no, (char *)q); break;
	case K_C: { uint8_t *k = q; if (k[3] == 255) snprintf(out, no, "\"#%02x%02x%02x\"", k[0], k[1], k[2]); else snprintf(out, no, "\"#%02x%02x%02x%02x\"", k[0], k[1], k[2], k[3]); break; }
	case K_V2: fmt_num(a, sizeof a, ((float *)q)[0]); fmt_num(b, sizeof b, ((float *)q)[1]); snprintf(out, no, "%s %s", a, b); break;
	case K_V2I: snprintf(out, no, "%d %d", ((int *)q)[0], ((int *)q)[1]); break;
	case K_V4:
		if (p->aux == 1) snprintf(out, no, "%d %d %d %d", ((int *)q)[0], ((int *)q)[1], ((int *)q)[2], ((int *)q)[3]);
		else { fmt_num(a, sizeof a, ((float *)q)[0]); fmt_num(b, sizeof b, ((float *)q)[1]); fmt_num(c, sizeof c, ((float *)q)[2]); fmt_num(d, sizeof d, ((float *)q)[3]); snprintf(out, no, "%s %s %s %s", a, b, c, d); }
		break;
	}
	return 1;
}

/* ---------------------------------------------------------------------------------------------------- caja local de un nodo */
int nodo_caja_local(Nodo *n, float *x, float *y, float *w, float *h) {
	float bx = 0, by = 0, bw = 0, bh = 0;
	switch (n->tipo) {
	case T_SPRITE: {
		DSprite *d = n->d; int hf = d->hf > 0 ? d->hf : 1, vf = d->vf > 0 ? d->vf : 1;
		int fw = 16, fh = 16;
		if (d->tex) { int tw = d->rw > 0 ? d->rw : d->tex->w, th = d->rh > 0 ? d->rh : d->tex->h; fw = tw / hf; fh = th / vf; }
		bw = (float)fw; bh = (float)fh; bx = (d->centrado ? -bw * 0.5f : 0) + d->ofx; by = (d->centrado ? -bh * 0.5f : 0) + d->ofy;
		break;
	}
	case T_MAPA: { DMapa *d = n->d; bw = (float)(d->w * d->cw); bh = (float)(d->h * d->ch); break; }
	case T_ETIQUETA: {
		DEtiqueta *d = n->d; int lin = 1; for (const char *c = d->texto; *c; ++c) if (*c == '\n') ++lin;
		bw = dib_texto_ancho(d->texto, d->escala); bh = (float)(9 * d->escala * lin + d->escala);
		if (d->ancho > 0 && bw > d->ancho) bw = d->ancho;
		bx = d->alineado == 1 ? -bw * 0.5f : d->alineado == 2 ? -bw : 0;
		break;
	}
	case T_BOTON: bw = ((DBoton *)n->d)->w; bh = ((DBoton *)n->d)->h; break;
	case T_AREA: case T_CUERPO: {
		Forma *f = n->tipo == T_AREA ? &((DArea *)n->d)->f : &((DCuerpo *)n->d)->f;
		bw = f->forma ? f->radio * 2 : f->w; bh = f->forma ? f->radio * 2 : f->h; bx = f->ox - bw * 0.5f; by = f->oy - bh * 0.5f;
		break;
	}
	case T_CAMARA: { DCamara *d = n->d; float z = d->zoom > 0.05f ? d->zoom : 1; bw = g_proy.ancho / z; bh = g_proy.alto / z; bx = -bw * 0.5f; by = -bh * 0.5f; break; }
	default: return 0;
	}
	if (x) *x = bx;
	if (y) *y = by;
	if (w) *w = bw;
	if (h) *h = bh;
	return bw > 0 && bh > 0;
}
