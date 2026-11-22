/* SPDX-License-Identifier: MIT */
/* raster.c - rasterizador por software de los cuadriláteros del motor, para el lienzo del editor (D460). */
#include "raster.h"

static int *orden; static int corden;

static int cmp(const void *a, const void *b) {
	const Quad *x = &g_quads[*(const int *)a], *y = &g_quads[*(const int *)b];
	if (x->abs != y->abs) return x->abs - y->abs;
	if (x->z != y->z) return x->z < y->z ? -1 : 1;
	return x->seq < y->seq ? -1 : x->seq > y->seq;
}

static inline void mezclar(uint32_t *d, uint32_t t, uint32_t col, int aditivo) {
	unsigned a = ((t >> 24) * (col >> 24)) / 255;
	if (!a) return;
	unsigned r = (((t >> 16) & 255) * ((col >> 16) & 255)) / 255, g = (((t >> 8) & 255) * ((col >> 8) & 255)) / 255, b = ((t & 255) * (col & 255)) / 255;
	unsigned dr = (*d >> 16) & 255, dg = (*d >> 8) & 255, db = *d & 255;
	if (aditivo) { dr += r * a / 255; dg += g * a / 255; db += b * a / 255; if (dr > 255) dr = 255; if (dg > 255) dg = 255; if (db > 255) db = 255; }
	else if (a == 255) { dr = r; dg = g; db = b; }
	else { dr = dr + (r - dr) * a / 255; dg = dg + (g - dg) * a / 255; db = db + (b - db) * a / 255; }
	*d = (dr << 16) | (dg << 8) | db;
}

static inline uint32_t texel(const Textura *t, float u, float v) {
	int x = (int)floorf(u), y = (int)floorf(v);
	if (x < 0 || y < 0 || x >= t->w || y >= t->h) return 0;
	return t->px[(size_t)y * (size_t)t->w + (size_t)x];
}

static void recto(uint32_t *buf, int w, int h, const Quad *q) {
	int X0 = (int)floorf(q->x[0] + 0.5f), X1 = (int)floorf(q->x[2] + 0.5f), Y0 = (int)floorf(q->y[0] + 0.5f), Y1 = (int)floorf(q->y[2] + 0.5f);
	if (X1 <= X0 || Y1 <= Y0) return;
	float du = (q->u[2] - q->u[0]) / (float)(X1 - X0), dv = (q->v[2] - q->v[0]) / (float)(Y1 - Y0);
	for (int y = Y0 < 0 ? 0 : Y0; y < Y1 && y < h; ++y) {
		float v = q->v[0] + ((float)(y - Y0) + 0.5f) * dv;
		for (int x = X0 < 0 ? 0 : X0; x < X1 && x < w; ++x) {
			float u = q->u[0] + ((float)(x - X0) + 0.5f) * du;
			mezclar(&buf[(size_t)y * (size_t)w + (size_t)x], texel(q->tex, u, v), q->col, q->mezcla);
		}
	}
}

static void girado(uint32_t *buf, int w, int h, const Quad *q) {
	float ex = q->x[1] - q->x[0], ey = q->y[1] - q->y[0], fx = q->x[3] - q->x[0], fy = q->y[3] - q->y[0];
	float det = ex * fy - ey * fx;
	if (fabsf(det) < 1e-6f) return;
	float minx = q->x[0], maxx = minx, miny = q->y[0], maxy = miny;
	for (int i = 1; i < 4; ++i) { if (q->x[i] < minx) minx = q->x[i]; if (q->x[i] > maxx) maxx = q->x[i]; if (q->y[i] < miny) miny = q->y[i]; if (q->y[i] > maxy) maxy = q->y[i]; }
	int x0 = (int)floorf(minx), x1 = (int)ceilf(maxx), y0 = (int)floorf(miny), y1 = (int)ceilf(maxy);
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > w) x1 = w;
	if (y1 > h) y1 = h;
	float du1 = q->u[1] - q->u[0], dv1 = q->v[1] - q->v[0], du2 = q->u[3] - q->u[0], dv2 = q->v[3] - q->v[0];
	for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) {
		float dx = (float)x + 0.5f - q->x[0], dy = (float)y + 0.5f - q->y[0];
		float s = (dx * fy - dy * fx) / det, t = (ex * dy - ey * dx) / det;
		if (s < 0 || s >= 1 || t < 0 || t >= 1) continue;
		mezclar(&buf[(size_t)y * (size_t)w + (size_t)x], texel(q->tex, q->u[0] + s * du1 + t * du2, q->v[0] + s * dv1 + t * dv2), q->col, q->mezcla);
	}
}

void rast_dibujar(uint32_t *buf, int w, int h) {
	int n = g_nquads;
	if (n <= 0) return;
	if (corden < n) { corden = n + 256; orden = realloc(orden, sizeof(int) * (size_t)corden); }
	for (int i = 0; i < n; ++i) orden[i] = i;
	qsort(orden, (size_t)n, sizeof(int), cmp);
	for (int k = 0; k < n; ++k) {
		const Quad *q = &g_quads[orden[k]];
		if (!q->tex || !q->tex->px) continue;
		if (q->recto) recto(buf, w, h, q); else girado(buf, w, h, q);
	}
}
