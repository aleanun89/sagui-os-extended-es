/* SPDX-License-Identifier: MIT */
/* dibujo_sdl.c - dibuja los cuadriláteros por la SDL 2. Con un renderizador acelerado (en SaguiOS, «saguios-gpu» sobre la
 * capa gpu3d) todo va por SDL_RenderGeometry en lotes grandes: una llamada por cambio de textura o de mezcla, con las
 * texturas del juego en atlas (una hoja de casillas o de sprites por textura). Por software, los cuadriláteros sin giro
 * van como copias de textura (más baratas que rasterizar triángulos) y los girados por geometría. */
#include "capi.h"
#include "dibujo_sdl.h"

static SDL_Renderer *R;
long dibsdl_lotes, dibsdl_triangulos, dibsdl_copias;
int dibsdl_geometria;

#define LOTE_QUADS 500
static SDL_Vertex vv[LOTE_QUADS * 4]; static int ii[LOTE_QUADS * 6];
static int nq; static Textura *lote_t; static int lote_m;
static int *orden; static int corden;

int dibsdl_iniciar(SDL_Renderer *r, int acel) {
	R = r; dibsdl_geometria = acel;
	for (int i = 0; i < LOTE_QUADS; ++i) { int b = i * 4, k = i * 6; ii[k] = b; ii[k + 1] = b + 1; ii[k + 2] = b + 2; ii[k + 3] = b; ii[k + 4] = b + 2; ii[k + 5] = b + 3; }
	return 0;
}

void dibsdl_soltar(void) {
	for (Textura *t = rec_primera(); t; t = t->sig) if (t->gpu) { SDL_DestroyTexture((SDL_Texture *)t->gpu); t->gpu = NULL; }
	free(orden); orden = NULL; corden = 0;
}

static SDL_Texture *sdl_tex(Textura *t) {
	if (t->gpu) return t->gpu;
	if (!t->px) return NULL;
	SDL_Texture *x = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, t->w, t->h);
	if (!x) { capi_log("no se pudo crear la textura %s (%dx%d): %s", t->ruta, t->w, t->h, SDL_GetError()); return NULL; }
	SDL_UpdateTexture(x, NULL, t->px, t->w * 4);
	SDL_SetTextureBlendMode(x, SDL_BLENDMODE_BLEND);
	t->gpu = x;
	return x;
}

static void soltar_lote(void) {
	if (!nq) return;
	SDL_Texture *x = lote_t ? sdl_tex(lote_t) : NULL;
	if (x) {
		SDL_SetTextureBlendMode(x, lote_m ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
		if (!dibsdl_geometria) { SDL_SetTextureColorMod(x, 255, 255, 255); SDL_SetTextureAlphaMod(x, 255); }
		SDL_RenderGeometry(R, x, vv, nq * 4, ii, nq * 6);
		++dibsdl_lotes; dibsdl_triangulos += nq * 2;
	}
	nq = 0;
}

static int cmp(const void *a, const void *b) {
	const Quad *x = &g_quads[*(const int *)a], *y = &g_quads[*(const int *)b];
	if (x->abs != y->abs) return x->abs - y->abs;
	if (x->z != y->z) return x->z < y->z ? -1 : 1;
	return x->seq < y->seq ? -1 : x->seq > y->seq;
}

static void a_geometria(const Quad *q, float esc, float ox, float oy) {
	SDL_Vertex *v = &vv[nq * 4];
	SDL_Color c = { (Uint8)(q->col >> 16), (Uint8)(q->col >> 8), (Uint8)q->col, (Uint8)(q->col >> 24) };
	float iw = 1.0f / (float)q->tex->w, ih = 1.0f / (float)q->tex->h;
	for (int i = 0; i < 4; ++i) {
		v[i].position.x = q->abs ? q->x[i] : ox + q->x[i] * esc; v[i].position.y = q->abs ? q->y[i] : oy + q->y[i] * esc;
		v[i].color = c; v[i].tex_coord.x = q->u[i] * iw; v[i].tex_coord.y = q->v[i] * ih;
	}
	++nq;
}

void dibsdl_enviar(float esc, float ox, float oy) {
	int n = g_nquads;
	dibsdl_lotes = dibsdl_triangulos = dibsdl_copias = 0;
	if (n == 0) return;
	if (corden < n) { corden = n + 256; orden = realloc(orden, sizeof(int) * (size_t)corden); }
	int ordenado = 1;
	for (int i = 0; i < n; ++i) { orden[i] = i; if (i && cmp(&orden[i - 1], &orden[i]) > 0) ordenado = 0; }
	if (!ordenado) qsort(orden, (size_t)n, sizeof(int), cmp);
	nq = 0; lote_t = NULL; lote_m = 0;
	for (int k = 0; k < n; ++k) {
		const Quad *q = &g_quads[orden[k]];
		if (!q->tex) continue;
		if (!dibsdl_geometria && q->recto) {
			soltar_lote();
			SDL_Texture *x = sdl_tex(q->tex); if (!x) continue;
			float x0 = q->abs ? q->x[0] : ox + q->x[0] * esc, y0 = q->abs ? q->y[0] : oy + q->y[0] * esc;
			float x1 = q->abs ? q->x[2] : ox + q->x[2] * esc, y1 = q->abs ? q->y[2] : oy + q->y[2] * esc;
			if (!q->abs) { x0 = floorf(x0 + 0.5f); y0 = floorf(y0 + 0.5f); x1 = floorf(x1 + 0.5f); y1 = floorf(y1 + 0.5f); }
			SDL_Rect d = { (int)x0, (int)y0, (int)(x1 - x0), (int)(y1 - y0) };
			if (d.w <= 0 || d.h <= 0) continue;
			int flip = 0; float u0 = q->u[0], u1 = q->u[2], v0 = q->v[0], v1 = q->v[2];
			if (u0 > u1) { float t = u0; u0 = u1; u1 = t; flip |= SDL_FLIP_HORIZONTAL; }
			if (v0 > v1) { float t = v0; v0 = v1; v1 = t; flip |= SDL_FLIP_VERTICAL; }
			SDL_Rect s = { (int)u0, (int)v0, (int)(u1 - u0), (int)(v1 - v0) };
			SDL_SetTextureBlendMode(x, q->mezcla ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
			SDL_SetTextureColorMod(x, (Uint8)(q->col >> 16), (Uint8)(q->col >> 8), (Uint8)q->col);
			SDL_SetTextureAlphaMod(x, (Uint8)(q->col >> 24));
			if (flip) SDL_RenderCopyEx(R, x, &s, &d, 0, NULL, (SDL_RendererFlip)flip); else SDL_RenderCopy(R, x, &s, &d);
			++dibsdl_copias;
			continue;
		}
		if (nq && (lote_t != q->tex || lote_m != q->mezcla || nq >= LOTE_QUADS)) soltar_lote();
		lote_t = q->tex; lote_m = q->mezcla;
		a_geometria(q, esc, ox, oy);
	}
	soltar_lote();
}
