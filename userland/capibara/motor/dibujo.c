/* SPDX-License-Identifier: MIT */
/* dibujo.c - recoge las órdenes de dibujo del árbol (cuadriláteros con textura) sin usar la SDL. La capa dibujo_sdl.c las
 * ordena por z, las agrupa por textura y las manda por lotes. Aquí también están la fuente de mapa de bits incorporada
 * (5x7, con las letras del español), el texto y el HUD. */
#include "capi.h"

Quad *g_quads; int g_nquads; static int cquads;
int g_vista_w, g_vista_h;
int g_hud, g_dep_col; long g_hud_lotes, g_hud_nodos; char g_hud_txt[10][100]; int g_hud_n;
static unsigned seq; static int modo_abs;

static struct { float cx, cy, zoom; Nodo *cam; } V = { 0, 0, 1, NULL };
static int cam_libre;   /* D460: el editor manda la cámara */
void dib_camara_libre(int activa, float cx, float cy, float zoom) { cam_libre = activa; if (activa) { V.cx = cx; V.cy = cy; V.zoom = zoom > 0.01f ? zoom : 1; V.cam = NULL; } }

static Quad *nuevo_quad(void) {
	if (g_nquads == cquads) { cquads = cquads ? cquads * 2 : 1024; g_quads = realloc(g_quads, sizeof(Quad) * (size_t)cquads); }
	Quad *q = &g_quads[g_nquads++]; memset(q, 0, sizeof *q); q->seq = seq++; q->abs = modo_abs; return q;
}

/* ---------------------------------------------------------------------------------------------------- cámara */
Nodo *dib_camara_actual(void) {
	Nodo *mejor = NULL;
	Nodo *pila[256]; int sp = 0;
	if (!g_mundo.arbol) return NULL;
	pila[sp++] = g_mundo.arbol;
	while (sp) {
		Nodo *n = pila[--sp];
		if (n->flags & F_BORRAR) continue;
		if (n->tipo == T_CAMARA) { if (((DCamara *)n->d)->actual) return n; if (!mejor) mejor = n; }
		for (int i = n->nh - 1; i >= 0 && sp < 255; --i) pila[sp++] = n->hijos[i];
	}
	return mejor;
}

void dib_cam_mundo_a_pantalla(float wx, float wy, float *sx, float *sy) { *sx = (wx - V.cx) * V.zoom + g_vista_w * 0.5f; *sy = (wy - V.cy) * V.zoom + g_vista_h * 0.5f; }
void dib_cam_pantalla_a_mundo(float sx, float sy, float *wx, float *wy) { *wx = (sx - g_vista_w * 0.5f) / V.zoom + V.cx; *wy = (sy - g_vista_h * 0.5f) / V.zoom + V.cy; }

static void fijar_camara(void) {
	if (cam_libre) return;
	V.cam = dib_camara_actual();
	if (!V.cam) { V.cx = g_vista_w * 0.5f; V.cy = g_vista_h * 0.5f; V.zoom = 1; return; }
	DCamara *c = V.cam->d; float m[6]; nodo_matriz_dibujo(V.cam, m);
	V.zoom = c->zoom > 0.05f ? c->zoom : 1; V.cx = m[4] + c->dx + c->cx; V.cy = m[5] + c->dy + c->cy;
	/* con el zoom, el centro se ajusta al píxel de la pantalla para que las casillas no tiemblen */
	V.cx = floorf(V.cx * V.zoom + 0.5f) / V.zoom; V.cy = floorf(V.cy * V.zoom + 0.5f) / V.zoom;
}

/* ---------------------------------------------------------------------------------------------------- cuadriláteros */
static float redond(float v) { return floorf(v + 0.5f); }

/* un rectángulo local (lx,ly,lw,lh) del nodo con matriz m, a pantalla */
static void quad_de_matriz(Textura *t, const float m[6], float lx, float ly, float lw, float lh, float u0, float v0, float u1, float v1,
                           uint32_t col, int z, int fijo, int mezcla) {
	Quad *q = nuevo_quad(); q->tex = t; q->col = col; q->z = z; q->mezcla = mezcla;
	float cx[4] = { lx, lx + lw, lx + lw, lx }, cy[4] = { ly, ly, ly + lh, ly + lh };
	q->recto = fabsf(m[1]) < 1e-4f && fabsf(m[2]) < 1e-4f && m[0] > 0 && m[3] > 0;
	for (int i = 0; i < 4; ++i) {
		float wx = m[0] * cx[i] + m[2] * cy[i] + m[4], wy = m[1] * cx[i] + m[3] * cy[i] + m[5], sx, sy;
		if (fijo) { sx = wx; sy = wy; } else dib_cam_mundo_a_pantalla(wx, wy, &sx, &sy);
		if (q->recto) { sx = redond(sx); sy = redond(sy); }
		q->x[i] = sx; q->y[i] = sy;
	}
	q->u[0] = u0; q->u[1] = u1; q->u[2] = u1; q->u[3] = u0; q->v[0] = v0; q->v[1] = v0; q->v[2] = v1; q->v[3] = v1;
}

void dib_rect(float x, float y, float w, float h, uint32_t col, int z, int mezcla) {
	Quad *q = nuevo_quad(); q->tex = dib_fuente(); q->col = col; q->z = z; q->mezcla = mezcla; q->recto = 1;
	float x1 = x + w, y1 = y + h;
	if (!modo_abs) { x = redond(x); y = redond(y); x1 = redond(x1); y1 = redond(y1); }
	q->x[0] = q->x[3] = x; q->x[1] = q->x[2] = x1; q->y[0] = q->y[1] = y; q->y[2] = q->y[3] = y1;
	/* el píxel blanco de la fuente: la celda del carácter 0x7F (un cuadrado lleno) */
	float u = 6.f * 15 + 1, v = 10.f * 7 + 1;
	q->u[0] = q->u[3] = u; q->u[1] = q->u[2] = u + 1; q->v[0] = q->v[1] = v; q->v[2] = q->v[3] = v + 1;
}

/* ---------------------------------------------------------------------------------------------------- fuente 5x7 */
static const uint8_t F57[95][5] = {
	{0,0,0,0,0},{0,0,0x5F,0,0},{0,7,0,7,0},{0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x56,0x20,0x50},{0,5,3,0,0},
	{0,0x1C,0x22,0x41,0},{0,0x41,0x22,0x1C,0},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},{0,0x80,0x60,0,0},{8,8,8,8,8},{0,0x60,0x60,0,0},{0x20,0x10,0x08,0x04,0x02},
	{0x3E,0x51,0x49,0x45,0x3E},{0,0x42,0x7F,0x40,0},{0x62,0x51,0x49,0x49,0x46},{0x22,0x41,0x49,0x49,0x36},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{1,0x71,9,5,3},
	{0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1E},{0,0x36,0x36,0,0},{0,0x80,0x56,0x36,0},{8,0x14,0x22,0x41,0},{0x14,0x14,0x14,0x14,0x14},{0,0x41,0x22,0x14,8},{2,1,0x51,9,6},
	{0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,9,9,9,1},{0x3E,0x41,0x49,0x49,0x7A},
	{0x7F,8,8,8,0x7F},{0,0x41,0x7F,0x41,0},{0x20,0x40,0x41,0x3F,1},{0x7F,8,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},{0x7F,2,0x0C,2,0x7F},{0x7F,4,8,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
	{0x7F,9,9,9,6},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,9,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{1,1,0x7F,1,1},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
	{0x63,0x14,8,0x14,0x63},{7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43},{0,0x7F,0x41,0x41,0},{2,4,8,0x10,0x20},{0,0x41,0x41,0x7F,0},{4,2,1,2,4},{0x40,0x40,0x40,0x40,0x40},
	{0,1,2,4,0},{0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},{8,0x7E,9,1,2},{0x18,0xA4,0xA4,0xA4,0x7C},
	{0x7F,8,4,4,0x78},{0,0x44,0x7D,0x40,0},{0x40,0x80,0x84,0x7D,0},{0x7F,0x10,0x28,0x44,0},{0,0x41,0x7F,0x40,0},{0x7C,4,0x18,4,0x78},{0x7C,8,4,4,0x78},{0x38,0x44,0x44,0x44,0x38},
	{0xFC,0x24,0x24,0x24,0x18},{0x18,0x24,0x24,0x18,0xFC},{0x7C,8,4,4,8},{0x48,0x54,0x54,0x54,0x20},{4,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
	{0x44,0x28,0x10,0x28,0x44},{0x1C,0xA0,0xA0,0xA0,0x7C},{0x44,0x64,0x54,0x4C,0x44},{0,8,0x36,0x41,0},{0,0,0x7F,0,0},{0,0x41,0x36,8,0},{8,4,8,16,8}
};

#define CW 6
#define CH 10
static void pix(uint32_t *px, int cx, int cy, int x, int y) { if (x >= 0 && x < CW && y >= 0 && y < CH) px[(cy * CH + y) * (16 * CW) + cx * CW + x] = 0xFFFFFFFFu; }
static void glifo(uint32_t *px, int code, const uint8_t col[5], int flipv) {
	int cx = code & 15, cy = code >> 4;
	for (int c = 0; c < 5; ++c) for (int r = 0; r < 8; ++r) if (col[c] & (1 << r)) pix(px, cx, cy, c, 2 + (flipv ? 8 - r : r));
}

Textura *dib_fuente(void) {
	static Textura *t;
	if (t) return t;
	uint32_t *px = calloc((size_t)(16 * CW) * (16 * CH), 4);
	for (int c = 32; c < 127; ++c) glifo(px, c, F57[c - 32], 0);
	/* el cuadrado lleno (para rectángulos): carácter 0x7F */
	for (int y = 0; y < CH; ++y) for (int x = 0; x < CW; ++x) pix(px, 15, 7, x, y);
	/* letras del español y signos de Latin-1 compuestos con un acento */
	static const struct { int code; char base; char acento; } L1[] = {
		{ 0xE1, 'a', 1 }, { 0xE9, 'e', 1 }, { 0xED, 'i', 1 }, { 0xF3, 'o', 1 }, { 0xFA, 'u', 1 }, { 0xC1, 'A', 1 }, { 0xC9, 'E', 1 }, { 0xCD, 'I', 1 }, { 0xD3, 'O', 1 },
		{ 0xDA, 'U', 1 }, { 0xF1, 'n', 2 }, { 0xD1, 'N', 2 }, { 0xFC, 'u', 3 }, { 0xDC, 'U', 3 }, { 0xE0, 'a', 4 }, { 0xE8, 'e', 4 }, { 0xE2, 'a', 5 }, { 0xEA, 'e', 5 },
		{ 0xF4, 'o', 5 }, { 0xE3, 'a', 2 }, { 0xF5, 'o', 2 }, { 0xE7, 'c', 6 }, { 0xC7, 'C', 6 } };
	for (unsigned i = 0; i < sizeof L1 / sizeof L1[0]; ++i) {
		int code = L1[i].code, cx = code & 15, cy = code >> 4;
		glifo(px, code, F57[L1[i].base - 32], 0);
		switch (L1[i].acento) {
		case 1: pix(px, cx, cy, 3, 0); pix(px, cx, cy, 2, 1); break;
		case 2: pix(px, cx, cy, 1, 1); pix(px, cx, cy, 2, 0); pix(px, cx, cy, 3, 0); pix(px, cx, cy, 4, 1); break;
		case 3: pix(px, cx, cy, 1, 0); pix(px, cx, cy, 3, 0); break;
		case 4: pix(px, cx, cy, 2, 0); pix(px, cx, cy, 3, 1); break;
		case 5: pix(px, cx, cy, 1, 1); pix(px, cx, cy, 2, 0); pix(px, cx, cy, 3, 1); break;
		case 6: pix(px, cx, cy, 2, 9); pix(px, cx, cy, 1, 9); break;
		}
	}
	glifo(px, 0xBF, F57['?' - 32], 1); glifo(px, 0xA1, F57['!' - 32], 1);
	t = rec_textura_memoria("capibara:fuente", 16 * CW, 16 * CH, px);
	return t;
}

/* siguiente carácter de una cadena UTF-8 (los de más de 255 salen como «?») */
static int sig_car(const char **p) {
	const unsigned char *s = (const unsigned char *)*p; int c = *s;
	if (c < 0x80) { *p += 1; return c; }
	if ((c & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) { *p += 2; c = ((c & 0x1F) << 6) | (s[1] & 0x3F); return c < 256 ? c : '?'; }
	if ((c & 0xF0) == 0xE0) { *p += 3; return '?'; }
	if ((c & 0xF8) == 0xF0) { *p += 4; return '?'; }
	*p += 1; return c;   /* ya en Latin-1 */
}

float dib_texto_ancho(const char *s, int escala) {
	float w = 0, mx = 0;
	while (*s) { int c = sig_car(&s); if (c == '\n') { if (w > mx) mx = w; w = 0; } else w += CW * escala; }
	return w > mx ? w : mx;
}

static void texto_linea(float x, float y, const char *s, size_t n, int escala, uint32_t col, int z) {
	Textura *t = dib_fuente(); const char *fin = s + n;
	while (s < fin) {
		int c = sig_car(&s);
		if (c > 32 && c < 256 && c != 0x7F && c != 0xA0) {
			Quad *q = nuevo_quad(); q->tex = t; q->col = col; q->z = z; q->recto = 1;
			float u = (float)((c & 15) * CW), v = (float)((c >> 4) * CH);
			q->x[0] = q->x[3] = x; q->x[1] = q->x[2] = x + CW * escala; q->y[0] = q->y[1] = y; q->y[2] = q->y[3] = y + CH * escala;
			q->u[0] = q->u[3] = u; q->u[1] = q->u[2] = u + CW; q->v[0] = q->v[1] = v; q->v[2] = q->v[3] = v + CH;
		}
		x += CW * escala;
	}
}

void dib_texto(float x, float y, const char *s, int escala, uint32_t col, int sombra, int z) {
	if (escala < 1) escala = 1;
	x = redond(x); y = redond(y);
	for (int pasada = sombra ? 0 : 1; pasada < 2; ++pasada) {
		/* la sombra va primero, un píxel (por escala) abajo y a la derecha, en negro con el alfa del texto */
		float ox = pasada == 0 ? (float)escala : 0, oy = pasada == 0 ? (float)escala : 0;
		uint32_t c = pasada == 0 ? (((col >> 24) * 200 / 255) << 24) : col;
		float yy = y;
		const char *p = s;
		while (1) {
			const char *nl = strchr(p, '\n'); size_t n = nl ? (size_t)(nl - p) : strlen(p);
			texto_linea(x + ox, yy + oy, p, n, escala, c, z);
			yy += (CH - 1) * escala;
			if (!nl) break;
			p = nl + 1;
		}
	}
}

/* ---------------------------------------------------------------------------------------------------- recorrido del árbol */
typedef struct { uint8_t c[4]; int z; } Heredado;

static uint32_t mezclar_col(const uint8_t a[4], const uint8_t b[4]) {
	return ((uint32_t)(a[3] * b[3] / 255) << 24) | ((uint32_t)(a[0] * b[0] / 255) << 16) | ((uint32_t)(a[1] * b[1] / 255) << 8) | (uint32_t)(a[2] * b[2] / 255);
}

static void dibujar_forma(Nodo *n, const Forma *f, const float m[6], int z, uint32_t col) {
	float sx = sqrtf(m[0] * m[0] + m[1] * m[1]), sy = sqrtf(m[2] * m[2] + m[3] * m[3]);
	float cx = m[4] + f->ox * sx, cy = m[5] + f->oy * sy, hw = (f->forma ? f->radio * sx : f->w * sx * 0.5f), hh = (f->forma ? f->radio * sy : f->h * sy * 0.5f);
	float x0, y0, x1, y1; dib_cam_mundo_a_pantalla(cx - hw, cy - hh, &x0, &y0); dib_cam_mundo_a_pantalla(cx + hw, cy + hh, &x1, &y1);
	(void)n;
	dib_rect(x0, y0, x1 - x0, 1, col, z, 0); dib_rect(x0, y1 - 1, x1 - x0, 1, col, z, 0); dib_rect(x0, y0, 1, y1 - y0, col, z, 0); dib_rect(x1 - 1, y0, 1, y1 - y0, col, z, 0);
}

static void nodo_dibujar(Nodo *n, Heredado h) {
	if (!(n->flags & F_VISIBLE) || (n->flags & F_BORRAR)) return;
	h.z += n->z;
	uint8_t col4[4] = { (uint8_t)(h.c[0] * n->mod[0] / 255), (uint8_t)(h.c[1] * n->mod[1] / 255), (uint8_t)(h.c[2] * n->mod[2] / 255), (uint8_t)(h.c[3] * n->mod[3] / 255) };
	Heredado hh = h; memcpy(hh.c, col4, 4);
	uint32_t col = col_pack(col4);
	float m[6];
	++g_hud_nodos;
	switch (n->tipo) {
	case T_SPRITE: {
		DSprite *d = n->d;
		if (!d->tex) break;
		nodo_matriz_dibujo(n, m);
		int bw = d->rw > 0 ? d->rw : d->tex->w, bh = d->rh > 0 ? d->rh : d->tex->h, hf = d->hf > 0 ? d->hf : 1, vf = d->vf > 0 ? d->vf : 1;
		int fw = bw / hf, fh = bh / vf, fi = d->fot < 0 ? 0 : d->fot % (hf * vf);
		float u0 = (float)(d->rx + (fi % hf) * fw), v0 = (float)(d->ry + (fi / hf) * fh), u1 = u0 + fw, v1 = v0 + fh;
		if (d->volt_h) { float t = u0; u0 = u1; u1 = t; }
		if (d->volt_v) { float t = v0; v0 = v1; v1 = t; }
		float lx = d->centrado ? -fw * 0.5f : 0, ly = d->centrado ? -fh * 0.5f : 0;
		quad_de_matriz(d->tex, m, lx + d->ofx, ly + d->ofy, (float)fw, (float)fh, u0, v0, u1, v1, col, h.z, 0, 0);
		break;
	}
	case T_MAPA: {
		DMapa *d = n->d;
		if (!d->tex || d->cols < 1) break;
		nodo_matriz_dibujo(n, m);
		float sx = sqrtf(m[0] * m[0] + m[1] * m[1]), sy = sqrtf(m[2] * m[2] + m[3] * m[3]);
		float cw = d->cw * sx, ch = d->ch * sy;
		/* celdas visibles: rectángulo de la vista en el mundo, pasado al mapa */
		float wx0, wy0, wx1, wy1; dib_cam_pantalla_a_mundo(0, 0, &wx0, &wy0); dib_cam_pantalla_a_mundo((float)g_vista_w, (float)g_vista_h, &wx1, &wy1);
		int c0 = (int)floorf((wx0 - m[4]) / cw), c1 = (int)floorf((wx1 - m[4]) / cw), r0 = (int)floorf((wy0 - m[5]) / ch), r1 = (int)floorf((wy1 - m[5]) / ch);
		if (c0 < 0) c0 = 0;
		if (r0 < 0) r0 = 0;
		if (c1 >= d->w) c1 = d->w - 1;
		if (r1 >= d->h) r1 = d->h - 1;
		for (int k = 0; k < d->ncapas; ++k) {
			if (!d->capas[k].visible) continue;
			for (int r = r0; r <= r1; ++r) for (int c = c0; c <= c1; ++c) {
				int v = mapa_celda(n, k, c, r);
				if (v < 0) continue;
				float u0 = (float)((v % d->cols) * (d->cw + d->sep)), v0 = (float)((v / d->cols) * (d->ch + d->sep));
				float mc[6] = { m[0], m[1], m[2], m[3], m[4] + c * cw, m[5] + r * ch };
				quad_de_matriz(d->tex, mc, 0, 0, (float)d->cw, (float)d->ch, u0, v0, u0 + d->cw, v0 + d->ch, col, h.z + k, 0, 0);
			}
		}
		break;
	}
	case T_ETIQUETA: {
		DEtiqueta *d = n->d; nodo_matriz_dibujo(n, m);
		float sx = m[4], sy = m[5];
		if (!d->fijo) dib_cam_mundo_a_pantalla(m[4], m[5], &sx, &sy);
		int z = h.z + (d->fijo ? 100000 : 0);
		/* ajuste de línea por palabras si hay ancho */
		char buf[512]; snprintf(buf, sizeof buf, "%s", d->texto);
		if (d->ancho > 0) {
			int max = (int)(d->ancho / (CW * d->escala)); if (max < 1) max = 1;
			int col_ = 0; char *ult = NULL; 
			for (char *p = buf; *p; ++p) {
				if (*p == '\n') { col_ = 0; ult = NULL; continue; }
				if (*p == ' ') ult = p;
				++col_;
				if (col_ > max && ult) { *ult = '\n'; col_ = (int)(p - ult); ult = NULL; }
			}
		}
		float w = dib_texto_ancho(buf, d->escala), ox = d->alineado == 1 ? -w * 0.5f : d->alineado == 2 ? -w : 0;
		dib_texto(sx + ox, sy, buf, d->escala, col, d->sombra, z);
		break;
	}
	case T_BOTON: {
		DBoton *d = n->d; nodo_matriz_dibujo(n, m);
		float sx = m[4], sy = m[5];
		if (!d->fijo) dib_cam_mundo_a_pantalla(m[4], m[5], &sx, &sy);
		int z = h.z + (d->fijo ? 100000 : 0);
		const uint8_t *c = d->deshabilitado ? (const uint8_t[]){ 70, 70, 70, 255 } : d->estado == 2 ? d->col_p : d->estado == 1 ? d->col_e : d->col_n;
		dib_rect(sx, sy, d->w, d->h, mezclar_col(c, col4), z, 0);
		dib_rect(sx, sy, d->w, 1, 0x60FFFFFFu, z + 1, 0); dib_rect(sx, sy + d->h - 1, d->w, 1, 0x80000000u, z + 1, 0);
		float tw = dib_texto_ancho(d->texto, d->escala);
		dib_texto(sx + (d->w - tw) * 0.5f, sy + (d->h - 9.f * d->escala) * 0.5f, d->texto, d->escala, 0xFFFFFFFFu, 1, z + 2);
		break;
	}
	case T_PARTICULAS: {
		DParticulas *d = n->d;
		Textura *t = d->tex ? d->tex : dib_fuente();
		for (int i = 0; i < d->np; ++i) {
			Part *p = &d->p[i]; float f = 1.f - p->vida / p->vida0; if (f < 0) f = 0; if (f > 1) f = 1;
			float sz = d->tam_ini + (d->tam_fin - d->tam_ini) * f;
			uint8_t c[4]; for (int k = 0; k < 4; ++k) c[k] = (uint8_t)(d->col_ini[k] + (d->col_fin[k] - d->col_ini[k]) * f);
			uint32_t cc = mezclar_col(c, col4);
			float sx, sy; dib_cam_mundo_a_pantalla(p->x, p->y, &sx, &sy); float s = sz * V.zoom;
			if (!d->tex) dib_rect(sx - s * 0.5f, sy - s * 0.5f, s, s, cc, h.z, d->aditivo);
			else {
				int hf = d->hf > 0 ? d->hf : 1, vf = d->vf > 0 ? d->vf : 1, fw = t->w / hf, fh = t->h / vf, fi = d->fot % (hf * vf);
				Quad *q = nuevo_quad(); q->tex = t; q->col = cc; q->z = h.z; q->mezcla = d->aditivo; q->recto = 1;
				float u0 = (float)((fi % hf) * fw), v0 = (float)((fi / hf) * fh);
				q->x[0] = q->x[3] = redond(sx - s * 0.5f); q->x[1] = q->x[2] = redond(sx + s * 0.5f); q->y[0] = q->y[1] = redond(sy - s * 0.5f); q->y[2] = q->y[3] = redond(sy + s * 0.5f);
				q->u[0] = q->u[3] = u0; q->u[1] = q->u[2] = u0 + fw; q->v[0] = q->v[1] = v0; q->v[2] = q->v[3] = v0 + fh;
			}
		}
		break;
	}
	case T_AREA: if (g_dep_col) { nodo_matriz_dibujo(n, m); dibujar_forma(n, &((DArea *)n->d)->f, m, 90000, 0xC040FF40u); } break;
	case T_CUERPO: if (g_dep_col) { nodo_matriz_dibujo(n, m); dibujar_forma(n, &((DCuerpo *)n->d)->f, m, 90000, 0xC0FF4040u); } break;
	default: break;
	}
	for (int i = 0; i < n->nh; ++i) nodo_dibujar(n->hijos[i], hh);
}

void dib_empezar_cuadro(Nodo *raiz, float alpha) {
	(void)alpha;
	g_nquads = 0; seq = 0; g_hud_nodos = 0; modo_abs = 0;
	if (!raiz) return;
	fijar_camara();
	Heredado h; memset(h.c, 255, 4); h.z = 0;
	nodo_dibujar(raiz, h);
}

void dib_hud(void) {
	if (!g_hud) return;
	modo_abs = 1;
	float w = 0; int n = g_hud_n;
	for (int i = 0; i < n; ++i) { float a = dib_texto_ancho(g_hud_txt[i], 1); if (a > w) w = a; }
	dib_rect(2, 2, w + 8, n * 9.f + 6, 0xB0000000u, 200000, 0);
	for (int i = 0; i < n; ++i) dib_texto(6, 5.f + i * 9, g_hud_txt[i], 1, 0xFFE0FFE0u, 0, 200001);
	modo_abs = 0;
}
