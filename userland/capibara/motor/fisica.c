/* SPDX-License-Identifier: MIT */
/* fisica.c - colisiones 2D sencillas: cajas alineadas con los ejes y círculos, mapas de casillas sólidas, cuerpos
 * estáticos, cinemáticos (los mueve el guion con mover_y_deslizar) y dinámicos (gravedad y velocidad), y áreas que avisan
 * cuando entra o sale algo. Sin rotación de las formas: un juego de plataformas o de vista cenital no la necesita. */
#include "capi.h"

float g_gravedad = 900;
long g_fis_pasos, g_fis_pares;

typedef struct { float cx, cy, hw, hh, r; int circ; } SH;

static Nodo **cuerpos, **areas, **mapas; static int ncu, nar, nma, ccu, car, cma;

static void anadir(Nodo ***v, int *n, int *c, Nodo *x) {
	if (*n == *c) { *c = *c ? *c * 2 : 64; *v = realloc(*v, sizeof(Nodo *) * (size_t)*c); }
	(*v)[(*n)++] = x;
}

static void recoger(Nodo *n) {
	if (n->flags & F_BORRAR) return;
	if (n->tipo == T_CUERPO) anadir(&cuerpos, &ncu, &ccu, n);
	else if (n->tipo == T_AREA) anadir(&areas, &nar, &car, n);
	else if (n->tipo == T_MAPA) anadir(&mapas, &nma, &cma, n);
	for (int i = 0; i < n->nh; ++i) recoger(n->hijos[i]);
}

static SH forma_mundo(Nodo *n, const Forma *f) {
	float m[6]; nodo_matriz(n, m);
	float sx = sqrtf(m[0] * m[0] + m[1] * m[1]), sy = sqrtf(m[2] * m[2] + m[3] * m[3]);
	SH s; s.cx = m[4] + f->ox * sx; s.cy = m[5] + f->oy * sy; s.circ = f->forma == 1;
	if (s.circ) { s.r = f->radio * (sx > sy ? sx : sy); s.hw = s.hh = s.r; }
	else { s.hw = f->w * sx * 0.5f; s.hh = f->h * sy * 0.5f; s.r = 0; }
	return s;
}
static Forma *forma_de(Nodo *n) { return n->tipo == T_AREA ? &((DArea *)n->d)->f : &((DCuerpo *)n->d)->f; }

/* penetración entre dos formas: devuelve 1 si se solapan y deja en (nx,ny) la dirección para sacar A de B y la profundidad */
static int pen(const SH *a, const SH *b, float *nx, float *ny, float *p) {
	if (a->circ && b->circ) {
		float dx = a->cx - b->cx, dy = a->cy - b->cy, d2 = dx * dx + dy * dy, rr = a->r + b->r;
		if (d2 >= rr * rr) return 0;
		float d = sqrtf(d2); if (d < 1e-6f) { *nx = 0; *ny = -1; *p = rr; return 1; }
		*nx = dx / d; *ny = dy / d; *p = rr - d; return 1;
	}
	if (!a->circ && !b->circ) {
		float dx = a->cx - b->cx, dy = a->cy - b->cy, ox = a->hw + b->hw - fabsf(dx), oy = a->hh + b->hh - fabsf(dy);
		if (ox <= 0 || oy <= 0) return 0;
		if (ox < oy) { *nx = dx < 0 ? -1 : 1; *ny = 0; *p = ox; } else { *nx = 0; *ny = dy < 0 ? -1 : 1; *p = oy; }
		return 1;
	}
	/* círculo contra caja */
	const SH *c = a->circ ? a : b, *bx = a->circ ? b : a;
	float px = capi_clamp(c->cx, bx->cx - bx->hw, bx->cx + bx->hw), py = capi_clamp(c->cy, bx->cy - bx->hh, bx->cy + bx->hh);
	float dx = c->cx - px, dy = c->cy - py, d2 = dx * dx + dy * dy;
	if (d2 >= c->r * c->r) return 0;
	float d = sqrtf(d2);
	if (d < 1e-6f) {   /* centro dentro de la caja: sale por el lado más cercano */
		float l = c->cx - (bx->cx - bx->hw), r = bx->cx + bx->hw - c->cx, t = c->cy - (bx->cy - bx->hh), bt = bx->cy + bx->hh - c->cy;
		float m = capi_min(capi_min(l, r), capi_min(t, bt));
		if (m == l) { dx = -1; dy = 0; } else if (m == r) { dx = 1; dy = 0; } else if (m == t) { dx = 0; dy = -1; } else { dx = 0; dy = 1; }
		*nx = dx; *ny = dy; *p = m + c->r;
	} else { *nx = dx / d; *ny = dy / d; *p = c->r - d; }
	if (a != c) { *nx = -*nx; *ny = -*ny; }   /* la normal se da para sacar A de B */
	return 1;
}

int fisica_solapa(Nodo *a, Nodo *b) {
	SH sa = forma_mundo(a, forma_de(a)), sb = forma_mundo(b, forma_de(b));
	float x, y, p; return pen(&sa, &sb, &x, &y, &p);
}

/* ---------------------------------------------------------------------------------------------------- mapas */
static int16_t *trozo(DMapa *m, int capa, int cx, int cy, int crear) {
	if (cx < 0 || cy < 0 || cx >= m->w || cy >= m->h || capa < 0 || capa >= m->ncapas) return NULL;
	int ti = (cy / MAPA_TROZO) * m->tw + cx / MAPA_TROZO;
	int16_t **tp = &m->capas[capa].trozos[ti];
	if (!*tp && crear) { *tp = malloc(sizeof(int16_t) * MAPA_TROZO * MAPA_TROZO); for (int i = 0; i < MAPA_TROZO * MAPA_TROZO; ++i) (*tp)[i] = -1; }
	return *tp;
}
int mapa_celda(Nodo *n, int capa, int cx, int cy) {
	DMapa *m = n->d; int16_t *t = trozo(m, capa, cx, cy, 0);
	return t ? t[(cy % MAPA_TROZO) * MAPA_TROZO + cx % MAPA_TROZO] : -1;
}
void mapa_poner(Nodo *n, int capa, int cx, int cy, int v) {
	DMapa *m = n->d; int16_t *t = trozo(m, capa, cx, cy, v >= 0);
	if (t) t[(cy % MAPA_TROZO) * MAPA_TROZO + cx % MAPA_TROZO] = (int16_t)v;
}
int mapa_capa(Nodo *n, const char *nome) { DMapa *m = n->d; for (int i = 0; i < m->ncapas; ++i) if (!strcmp(m->capas[i].nome, nome)) return i; return -1; }
int mapa_solida(Nodo *n, int cx, int cy) {
	DMapa *m = n->d;
	for (int c = 0; c < m->ncapas; ++c) {
		if (!m->capas[c].solida) continue;
		int v = mapa_celda(n, c, cx, cy);
		if (v < 0) continue;
		if (!m->usa_solidas || (v < 1024 && m->solidas[v])) return 1;
	}
	return 0;
}

/* casillas sólidas cuya caja toca la de A: la más profunda primero no importa; se resuelve una a una */
typedef struct { SH s; Nodo *dueno; } Solido;
static Solido sol[256];
static int solidos_cerca(const SH *a, unsigned mascara, Nodo *excepto, Solido *out, int max) {
	int n = 0;
	for (int i = 0; i < nma && n < max; ++i) {
		Nodo *mn = mapas[i]; DMapa *m = mn->d;
		if (!(m->capa_col & mascara) || !m->ncapas) continue;
		float mm[6]; nodo_matriz(mn, mm);
		float sx = sqrtf(mm[0] * mm[0] + mm[1] * mm[1]), sy = sqrtf(mm[2] * mm[2] + mm[3] * mm[3]);
		float cw = m->cw * sx, ch = m->ch * sy;
		if (cw <= 0 || ch <= 0) continue;
		int x0 = (int)floorf((a->cx - a->hw - mm[4]) / cw), x1 = (int)floorf((a->cx + a->hw - mm[4]) / cw);
		int y0 = (int)floorf((a->cy - a->hh - mm[5]) / ch), y1 = (int)floorf((a->cy + a->hh - mm[5]) / ch);
		for (int cy = y0; cy <= y1; ++cy) for (int cx = x0; cx <= x1 && n < max; ++cx) {
			if (!mapa_solida(mn, cx, cy)) continue;
			SH s; s.circ = 0; s.hw = cw * 0.5f; s.hh = ch * 0.5f; s.cx = mm[4] + (cx + 0.5f) * cw; s.cy = mm[5] + (cy + 0.5f) * ch; s.r = 0;
			out[n].s = s; out[n].dueno = mn; ++n;
		}
	}
	for (int i = 0; i < ncu && n < max; ++i) {
		Nodo *b = cuerpos[i]; DCuerpo *d = b->d;
		if (b == excepto || d->modo != 0 || !(d->f.capa & mascara)) continue;
		SH s = forma_mundo(b, &d->f);
		if (fabsf(s.cx - a->cx) > s.hw + a->hw || fabsf(s.cy - a->cy) > s.hh + a->hh) continue;
		out[n].s = s; out[n].dueno = b; ++n;
	}
	return n;
}

int fisica_choca_mapa_punto(Nodo *raiz, float x, float y) {
	(void)raiz;
	SH p; p.cx = x; p.cy = y; p.hw = p.hh = 0.01f; p.circ = 0; p.r = 0;
	return solidos_cerca(&p, 0xFFFFFFFFu, NULL, sol, 256) > 0;
}

/* ---------------------------------------------------------------------------------------------------- movimiento */
void fisica_mover(Nodo *n, float dx, float dy, float *rx, float *ry) {
	DCuerpo *d = n->d;
	d->suelo = d->techo = d->pared = 0; d->nx = d->ny = 0; d->col_id = 0;
	float gx, gy; nodo_pos_global(n, &gx, &gy);
	SH s = forma_mundo(n, &d->f);
	float minh = capi_min(s.hw, s.hh); if (minh < 1) minh = 1;
	float mx = fabsf(dx) > fabsf(dy) ? fabsf(dx) : fabsf(dy);
	int pasos = (int)ceilf(mx / (minh * 0.5f)); if (pasos < 1) pasos = 1; if (pasos > 32) pasos = 32;
	float sx = dx / pasos, sy = dy / pasos, bx = 0, by = 0;
	for (int p = 0; p < pasos; ++p) {
		if (s.circ) {
			s.cx += sx; s.cy += sy; gx += sx; gy += sy;
			int k = solidos_cerca(&s, d->f.mascara, n, sol, 256);
			for (int i = 0; i < k; ++i) {
				float nx, ny, pe;
				if (!pen(&s, &sol[i].s, &nx, &ny, &pe)) continue;
				s.cx += nx * pe; s.cy += ny * pe; gx += nx * pe; gy += ny * pe;
				if (ny < -0.5f) { d->suelo = 1; by = 1; } else if (ny > 0.5f) { d->techo = 1; by = 1; }
				if (fabsf(nx) > 0.5f) { d->pared = 1; bx = 1; }
				d->nx = nx; d->ny = ny; d->col_id = sol[i].dueno->id;
			}
			continue;
		}
		/* caja: primero en X y luego en Y, para deslizar por paredes y suelos */
		for (int eje = 0; eje < 2; ++eje) {
			float v = eje == 0 ? sx : sy;
			if (v == 0) continue;
			if (eje == 0) { s.cx += v; gx += v; } else { s.cy += v; gy += v; }
			int k = solidos_cerca(&s, d->f.mascara, n, sol, 256);
			for (int i = 0; i < k; ++i) {
				SH *b = &sol[i].s;
				if (fabsf(s.cx - b->cx) >= s.hw + b->hw || fabsf(s.cy - b->cy) >= s.hh + b->hh) continue;
				if (b->circ) { float nx, ny, pe; if (pen(&s, b, &nx, &ny, &pe)) { s.cx += nx * pe; s.cy += ny * pe; gx += nx * pe; gy += ny * pe; } continue; }
				if (eje == 0) {
					float nuevo = v > 0 ? b->cx - b->hw - s.hw : b->cx + b->hw + s.hw;
					gx += nuevo - s.cx; s.cx = nuevo; d->pared = 1; bx = 1; d->nx = v > 0 ? -1.f : 1.f; d->ny = 0;
				} else {
					float nuevo = v > 0 ? b->cy - b->hh - s.hh : b->cy + b->hh + s.hh;
					gy += nuevo - s.cy; s.cy = nuevo; by = 1;
					if (v > 0) d->suelo = 1; else d->techo = 1;
					d->nx = 0; d->ny = v > 0 ? -1.f : 1.f;
				}
				d->col_id = sol[i].dueno->id;
			}
		}
	}
	nodo_poner_pos_global(n, gx, gy);
	if (rx) *rx = bx ? 0 : dx;
	if (ry) *ry = by ? 0 : dy;
	++g_fis_pasos;
}

/* ---------------------------------------------------------------------------------------------------- paso */
static void guardar_previo(Nodo *n) {
	if (n->flags & F_INTERP) { n->ax = n->x; n->ay = n->y; }
	for (int i = 0; i < n->nh; ++i) guardar_previo(n->hijos[i]);
}

void fisica_preparar(Nodo *raiz) {
	ncu = nar = nma = 0;
	recoger(raiz);
	guardar_previo(raiz);
}

static int en_lista(const Dentro *l, int n, int id) { for (int i = 0; i < n; ++i) if (l[i].id == id) return 1; return 0; }
static void anadir_dentro(Dentro **l, int *n, int *c, int id) {
	if (*n == *c) { *c = *c ? *c * 2 : 8; *l = realloc(*l, sizeof(Dentro) * (size_t)*c); }
	(*l)[(*n)++].id = id;
}

void fisica_area_cuerpos(Nodo *a, Nodo ***lista, int *n) {
	static Nodo **tmp; static int ct;
	DArea *d = a->d; int k = 0;
	if (ct < d->nd + 1) { ct = d->nd + 8; tmp = realloc(tmp, sizeof(Nodo *) * (size_t)ct); }
	for (int i = 0; i < d->nd; ++i) { Nodo *o = nodo_de_id(d->d[i].id); if (o && o->tipo == T_CUERPO) tmp[k++] = o; }
	*lista = tmp; *n = k;
}

void fisica_paso(Nodo *raiz, float dt) {
	(void)raiz;
	++g_fis_pasos;
	/* cuerpos dinámicos */
	for (int i = 0; i < ncu; ++i) {
		Nodo *n = cuerpos[i]; DCuerpo *d = n->d;
		if (d->modo != 2 || !(n->flags & F_PROCESAR) || (n->flags & F_BORRAR)) continue;
		d->vy += g_gravedad * d->escala_grav * dt;
		float rx, ry; fisica_mover(n, d->vx * dt, d->vy * dt, &rx, &ry);
		if (d->suelo || d->techo) {
			if (d->rebote > 0 && fabsf(d->vy) > 20) d->vy = -d->vy * d->rebote; else d->vy = 0;
			if (d->suelo && d->friccion > 0) { float f = 1.f - d->friccion * dt; d->vx *= f < 0 ? 0 : f; }
		}
		if (d->pared) d->vx = d->rebote > 0 ? -d->vx * d->rebote : 0;
	}
	/* áreas: quién está dentro ahora, y avisos de entrada y salida */
	for (int i = 0; i < nar; ++i) {
		Nodo *a = areas[i]; DArea *d = a->d;
		if (!d->vigilar || (a->flags & F_BORRAR)) continue;
		SH sa = forma_mundo(a, &d->f);
		d->nd_ant = 0;
		for (int k = 0; k < d->nd; ++k) anadir_dentro(&d->d_ant, &d->nd_ant, &d->cd_ant, d->d[k].id);
		d->nd = 0;
		for (int k = 0; k < ncu; ++k) {
			Nodo *b = cuerpos[k]; DCuerpo *db = b->d;
			if (b->flags & F_BORRAR) continue;
			if (!(d->f.mascara & db->f.capa)) continue;
			SH sb = forma_mundo(b, &db->f); float x, y, p; ++g_fis_pares;
			if (pen(&sa, &sb, &x, &y, &p)) anadir_dentro(&d->d, &d->nd, &d->cd, b->id);
		}
		for (int k = 0; k < nar; ++k) {
			Nodo *b = areas[k]; DArea *db = b->d;
			if (b == a || (b->flags & F_BORRAR) || !(d->f.mascara & db->f.capa)) continue;
			SH sb = forma_mundo(b, &db->f); float x, y, p; ++g_fis_pares;
			if (pen(&sa, &sb, &x, &y, &p)) anadir_dentro(&d->d, &d->nd, &d->cd, b->id);
		}
	}
	/* los avisos van después de calcular todo (los guiones pueden cambiar el mundo) */
	for (int i = 0; i < nar; ++i) {
		Nodo *a = nodo_de_id(areas[i]->id); if (!a) continue;
		DArea *d = a->d;
		if (!d->vigilar) continue;
		/* copia, porque un guion puede liberar y reutilizar */
		Dentro *nuevos = malloc(sizeof(Dentro) * (size_t)(d->nd + 1)); memcpy(nuevos, d->d, sizeof(Dentro) * (size_t)d->nd); int nn = d->nd;
		Dentro *viejos = malloc(sizeof(Dentro) * (size_t)(d->nd_ant + 1)); memcpy(viejos, d->d_ant, sizeof(Dentro) * (size_t)d->nd_ant); int nv = d->nd_ant;
		for (int k = 0; k < nn; ++k) if (!en_lista(viejos, nv, nuevos[k].id)) {
			Nodo *o = nodo_de_id(nuevos[k].id); if (!o || !nodo_de_id(areas[i]->id)) continue;
			guion_empujar_nodo(g_L, o); sen_emitir(a, o->tipo == T_AREA ? "area_entro" : "cuerpo_entro", 1);
		}
		for (int k = 0; k < nv; ++k) if (!en_lista(nuevos, nn, viejos[k].id)) {
			Nodo *o = nodo_de_id(viejos[k].id); if (!o || !nodo_de_id(areas[i]->id)) continue;
			guion_empujar_nodo(g_L, o); sen_emitir(a, o->tipo == T_AREA ? "area_salio" : "cuerpo_salio", 1);
		}
		free(nuevos); free(viejos);
	}
}
