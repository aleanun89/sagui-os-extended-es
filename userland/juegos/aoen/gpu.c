/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Dibujo de la partida por la GPU (D436, «fase 2»; ver gpu.h y docs/aoen.md).
 *
 * Por qué así: en el Amilo (Radeon X1400) el terreno HD por la CPU se comía 50 de los 70 ms del cuadro y subir el cuadro
 * entero a la GPU (fase 1, D432) otros 11. Aquí:
 *  - el terreno es una malla de triángulos con las texturas HD (o las losetas del DRS) y la luz de la niebla en el color de
 *    cada vértice: nada por píxel en la CPU;
 *  - los sprites (unidades, edificios, árboles, efectos) se convierten una vez a ARGB y se guardan en páginas de atlas de
 *    1024x1024 (con el color de cada jugador ya puesto si lo llevan); cada página se sube a la GPU a lo sumo una vez por
 *    cuadro: las operaciones del mundo se graban, se resuelven todas en el atlas y luego se mandan en orden;
 *  - rellenos, líneas, sombras y elipses usan zonas reservadas de cada página (un blanco y un disco), así casi todo va en
 *    el mismo lote (una llamada de 3072 vértices al kernel);
 *  - la interfaz la sigue pintando la CPU, en una capa con alfa premultiplicado partida en franjas: solo se sube la franja
 *    que cambió y solo se dibujan las que tienen algo. */
#include "gpu.h"
#include "ui.h"
#include "hilos.h"
#include "config.h"

int gpu_pedido, gpu_ativo, gfx_gpu, gfx_capa;
/* AOEN_GPU_QUITAR (medidas): 1 sin terreno, 2 sin lo de encima del suelo, 4 sin la capa de la interfaz */
static int quitar;

static SDL_Renderer *R;
static int pode;                 /* el renderizador lleva la GPU y nada ha fallado */
static int cw, ch;               /* lienzo */
static SDL_BlendMode M_PREMULT;
static unsigned long long quadro; /* contador de cuadros (para el atlas) */

/* ---------------------------------------------------------------- lote de triángulos */

#define LOTE_MAX 3072
static SDL_Vertex lote[LOTE_MAX];
static int capa_fase;   /* qué se dibuja: 0 el terreno, 1 lo de encima del suelo, 2 la capa */
static int lote_n, lote_m, geom_erro;
static SDL_Texture *lote_t;
static unsigned long med_tris, med_lotes, med_subidas_kb;
static GpuMedidas ultimas;
static unsigned long long soma_tris, soma_lotes, soma_kb, soma_franjas, soma_cuadros;   /* para el informe del banco */

static unsigned long long espera;   /* ciclos mandando triángulos y presentando (la «espera a la GPU» del HUD) */
static int ativo_ultimo;            /* el último cuadro fue por la GPU */
unsigned long long gpu_espera_ciclos(void) { return espera; }
static Uint32 t_ultimo;
int gpu_ativo_ultimo(void) { return ativo_ultimo && pode && SDL_GetTicks() - t_ultimo < 500; }

static void lote_mandar(void) {
	if (!lote_n) return;
	unsigned long long t0 = perf_reloj();
	PERF_INI();
	if (lote_t) SDL_SetTextureBlendMode(lote_t, (SDL_BlendMode)lote_m);
	else SDL_SetRenderDrawBlendMode(R, (SDL_BlendMode)lote_m);
	if (SDL_RenderGeometry(R, lote_t, lote, lote_n, NULL, 0) != 0 && !geom_erro) { geom_erro = 1; AOEN_LOG("[AOEN] GPU: SDL_RenderGeometry falló: %s", SDL_GetError()); }
	med_tris += (unsigned long)(lote_n / 3); ++med_lotes;
	lote_n = 0;
	PERF_FIN(PERF_GEOM);
	espera += perf_reloj() - t0;
}

void gpu_triangulos(SDL_Texture *t, int modo, const SDL_Vertex *v, int n) {
	if ((quitar & 1) && capa_fase == 0) return;
	while (n > 0) {
		if (lote_n && (lote_t != t || lote_m != modo || lote_n + 3 > LOTE_MAX)) lote_mandar();
		lote_t = t; lote_m = modo;
		int k = MIN(n, (LOTE_MAX - lote_n) / 3 * 3);
		memcpy(lote + lote_n, v, sizeof(SDL_Vertex) * (size_t)k);
		lote_n += k; v += k; n -= k;
	}
}

static inline SDL_Color cor_de(u32 c, int a) {
	SDL_Color k = { (Uint8)(c >> 16), (Uint8)(c >> 8), (Uint8)c, (Uint8)CLAMP(a, 0, 255) };
	return k;
}

/* un rectángulo con textura (x0 > x1 o u0 > u1 dan el espejo), ya recortado por quien llama */
static void quad(SDL_Texture *t, int modo, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, SDL_Color c) {
	SDL_Vertex q[6];
	q[0].position.x = x0; q[0].position.y = y0; q[0].tex_coord.x = u0; q[0].tex_coord.y = v0;
	q[1].position.x = x1; q[1].position.y = y0; q[1].tex_coord.x = u1; q[1].tex_coord.y = v0;
	q[2].position.x = x1; q[2].position.y = y1; q[2].tex_coord.x = u1; q[2].tex_coord.y = v1;
	q[3] = q[0]; q[4] = q[2];
	q[5].position.x = x0; q[5].position.y = y1; q[5].tex_coord.x = u0; q[5].tex_coord.y = v1;
	for (int i = 0; i < 6; ++i) q[i].color = c;
	gpu_triangulos(t, modo, q, 6);
}

/* el mismo rectángulo recortado a r (las coordenadas de textura se recortan en proporción) */
static void quad_recortado(SDL_Texture *t, int modo, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, SDL_Color c, const Recorte *r) {
	if (x1 <= x0 || y1 <= y0) return;
	float cx0 = (float)r->x0, cy0 = (float)r->y0, cx1 = (float)r->x1, cy1 = (float)r->y1;
	if (x1 <= cx0 || x0 >= cx1 || y1 <= cy0 || y0 >= cy1) return;
	float du = (u1 - u0) / (x1 - x0), dv = (v1 - v0) / (y1 - y0);
	if (x0 < cx0) { u0 += (cx0 - x0) * du; x0 = cx0; }
	if (x1 > cx1) { u1 -= (x1 - cx1) * du; x1 = cx1; }
	if (y0 < cy0) { v0 += (cy0 - y0) * dv; y0 = cy0; }
	if (y1 > cy1) { v1 -= (y1 - cy1) * dv; y1 = cy1; }
	quad(t, modo, x0, y0, x1, y1, u0, v0, u1, v1, c);
}

/* ---------------------------------------------------------------- atlas de sprites */

#define PAG_LADO 1024
#define PAG_MAX 12
/* D436: los sprites nuevos van primero a una página pequeña de paso (512x512: subirla cuesta la cuarta parte que una
 * grande) y, cuando se llena, pasan juntos a las grandes: así, mientras van apareciendo cuadros de animación nuevos, no se
 * sube una página de 4 MB en cada cuadro */
#define PAG_EST PAG_MAX
#define EST_LADO 512
#define PRATELEIRAS 96
/* zona reservada de cada página: un blanco de 4x4 en (0,0) y un disco de 64x64 en (8,0) */
#define BRANCO_X 1
#define BRANCO_Y 1
#define DISCO_X 8
#define DISCO_L 64
#define RES_ALTO 72

typedef struct {
	SDL_Texture *t;
	int lado;                     /* las páginas son cuadradas */
	u32 *cpu;                     /* la de paso guarda una copia (para pasar sus sprites a las grandes) */
	int gen;
	int pr_x[PRATELEIRAS], pr_y[PRATELEIRAS], pr_h[PRATELEIRAS], npr;
	int y_livre;
	unsigned long long usada;     /* último cuadro en que se usó */
	unsigned long long subida;    /* último cuadro en que cambió (se vuelve a subir) */
	int sprites;
} Pagina;
static Pagina pag[PAG_MAX + 1];
static int npag, pag_max = PAG_MAX, est_cheia;

typedef struct { const Sprite *s; int var; short pag, gen, x, y; } Entrada;
#define TAB 16384
static Entrada tab[TAB];
static int tab_n, pal_ver = -1, atlas_cheio_avisado;
static u32 *conv; static int conv_cap;

static void pagina_reservado(u32 *b, int l) {
	for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) b[y * l + x] = 0xffffffffu;
	for (int y = 0; y < DISCO_L; ++y)
		for (int x = 0; x < DISCO_L; ++x) {
			float dx = x + 0.5f - DISCO_L / 2.0f, dy = y + 0.5f - DISCO_L / 2.0f;
			b[y * l + DISCO_X + x] = dx * dx + dy * dy <= (DISCO_L / 2.0f) * (DISCO_L / 2.0f) ? 0xffffffffu : 0;
		}
}

static int pagina_criar(int k, int lado) {
	SDL_Texture *t = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, lado, lado);
	if (!t) { AOEN_LOG("[AOEN] GPU: no se pudo crear la página %d del atlas", k); return 0; }
	u32 *z = (u32 *)calloc((size_t)lado * lado, 4);   /* a cero (en el PC la textura nace con basura) y lo reservado */
	if (!z) { SDL_DestroyTexture(t); return 0; }
	pagina_reservado(z, lado);
	SDL_UpdateTexture(t, NULL, z, lado * 4);
	Pagina *p = &pag[k];
	memset(p, 0, sizeof *p);
	if (k == PAG_EST) p->cpu = z; else free(z);
	p->t = t; p->lado = lado; p->gen = 1; p->y_livre = RES_ALTO; p->subida = quadro;
	med_subidas_kb += (unsigned long)lado * lado * 4 / 1024;
	return 1;
}

static int pagina_nova(void) {
	if (npag >= pag_max) return -1;
	if (!pagina_criar(npag, PAG_LADO)) { pag_max = npag; return -1; }
	return npag++;
}

/* vacía una página (desalojo): sus entradas quedan viejas por la generación */
static void pagina_vaciar(int k) {
	Pagina *p = &pag[k];
	p->gen = p->gen >= 30000 ? 1 : p->gen + 1;
	p->npr = 0; p->y_livre = RES_ALTO; p->sprites = 0;
}

/* sitio de w x h (con un píxel de margen) en la página k; 0 si no cabe */
static int pagina_sitio(Pagina *p, int w, int h, int *ox, int *oy) {
	int bw = w + 1, bh = h + 1, mejor = -1, L = p->lado;
	if (!p->t || bw > L || bh > L - RES_ALTO) return 0;
	for (int i = 0; i < p->npr; ++i)
		if (p->pr_h[i] >= bh && p->pr_x[i] + bw <= L && (mejor < 0 || p->pr_h[i] < p->pr_h[mejor])) mejor = i;
	if (mejor >= 0 && p->pr_h[mejor] <= bh + bh / 2 + 8) {
		*ox = p->pr_x[mejor]; *oy = p->pr_y[mejor]; p->pr_x[mejor] += bw; return 1;
	}
	if (p->npr < PRATELEIRAS && p->y_livre + bh <= L) {
		int i = p->npr++;
		p->pr_x[i] = bw; p->pr_y[i] = p->y_livre; p->pr_h[i] = bh; p->y_livre += bh;
		*ox = 0; *oy = p->pr_y[i]; return 1;
	}
	if (mejor >= 0) { *ox = p->pr_x[mejor]; *oy = p->pr_y[mejor]; p->pr_x[mejor] += bw; return 1; }
	return 0;
}

static unsigned hash_de(const Sprite *s, int var) {
	uintptr_t v = (uintptr_t)s;
	return ((unsigned)(v >> 3) * 2654435761u ^ (unsigned)(var + 3) * 40503u) & (TAB - 1);
}

static int limpezas;   /* veces que se vació la tabla (las entradas ya resueltas dejan de valer) */
static void atlas_limpar(void) {
	++limpezas;
	memset(tab, 0, sizeof tab);
	tab_n = 0;
	for (int k = 0; k < npag; ++k) pagina_vaciar(k);
	if (pag[PAG_EST].t) pagina_vaciar(PAG_EST);
}

/* el sprite en ARGB: transparente 0, sombra (negro puro) alfa 116 (deja 140/256 de luz, como shadow_px), el resto opaco.
 * var >= 0: jugador; -1: sin color de jugador; -2: loseta del terreno (los huecos toman el color del vecino de la fila,
 * así el borde del rombo nunca muestrea un transparente) */
static int convertir(const Sprite *s, int var, u32 *o) {
	int poff = 16 * ((var < 0 ? 0 : var) + 1), tem_pc = 0;
	for (int y = 0; y < s->h; ++y) {
		const u8 *row = s->px + (size_t)y * s->w, *prow = s->pc + (size_t)y * s->w;
		u32 *d = o + (size_t)y * s->w;
		for (int x = 0; x < s->w; ++x) {
			u8 v = row[x];
			if (!v) { d[x] = 0; continue; }
			if (prow[x]) { tem_pc = 1; v = (u8)(v + poff); }
			u32 c = fb.pal[v];
			d[x] = (c & 0xffffff) == 0 ? 0x74000000u : (0xff000000u | (c & 0xffffff));
		}
		if (var == -2) {
			int ult = -1;
			for (int x = 0; x < s->w; ++x) if (d[x] >> 24 == 0xff) { if (ult < 0) for (int k = 0; k < x; ++k) d[k] = d[x]; ult = x; } else if (ult >= 0) d[x] = d[ult];
		}
	}
	return tem_pc;
}

/* busca o pone el sprite en el atlas: entrada válida o NULL (no cabe) */
static Entrada *atlas_achar(const Sprite *s, int var) {
	unsigned h = hash_de(s, var);
	for (int i = 0; i < TAB; ++i, h = (h + 1) & (TAB - 1)) {
		Entrada *e = &tab[h];
		if (!e->s) return NULL;
		if (e->s == s && e->var == var) return ((e->pag >= 0 && e->pag < npag) || e->pag == PAG_EST) && pag[e->pag].gen == e->gen ? e : NULL;
	}
	return NULL;
}

static Entrada *atlas_entrada_nova(const Sprite *s, int var) {
	if (tab_n > TAB * 3 / 4) { AOEN_LOG("[AOEN] GPU: atlas: tabla llena (%d sprites), se vacía", tab_n); atlas_limpar(); }
	unsigned h = hash_de(s, var);
	for (int i = 0; i < TAB; ++i, h = (h + 1) & (TAB - 1)) {
		Entrada *e = &tab[h];
		if (!e->s) { e->s = s; e->var = var; e->pag = -1; ++tab_n; return e; }
		if (e->s == s && e->var == var) return e;
	}
	return NULL;
}

static Entrada *atlas_sprite(const Sprite *s, int player, int loseta) {
	if (fb_pal_version() != pal_ver) { if (pal_ver >= 0) atlas_limpar(); pal_ver = fb_pal_version(); }
	int var = loseta ? -2 : -1;
	Entrada *e = atlas_achar(s, var);
	if (!e && !loseta) e = atlas_achar(s, player);
	if (e) { pag[e->pag].usada = quadro; return e; }
	if (s->w + 1 >= PAG_LADO || s->h + 1 >= PAG_LADO - RES_ALTO) return NULL;
	size_t n = (size_t)s->w * s->h;
	if ((size_t)conv_cap < n) { free(conv); conv_cap = (int)n + 4096; conv = (u32 *)malloc((size_t)conv_cap * 4); if (!conv) { conv_cap = 0; return NULL; } }
	if (convertir(s, loseta ? -2 : player, conv) && !loseta) var = player;   /* sin color de jugador: la misma para todos */
	/* sitio: en la de paso; si no cabe, en una grande con hueco; si no, una nueva; si no, se vacía la grande menos usada
	 * que no se use en este cuadro */
	int k = -1, ox = 0, oy = 0;
	if (!pag[PAG_EST].t && !getenv("AOEN_GPU_SIN_PASO")) pagina_criar(PAG_EST, EST_LADO);
	if (pagina_sitio(&pag[PAG_EST], s->w, s->h, &ox, &oy)) k = PAG_EST;
	else if (pag[PAG_EST].t && s->w + 1 <= EST_LADO && s->h + 1 <= EST_LADO - RES_ALTO) est_cheia = 1;
	for (int i = 0; i < npag && k < 0; ++i) if (pagina_sitio(&pag[i], s->w, s->h, &ox, &oy)) k = i;
	if (k < 0 && (k = pagina_nova()) >= 0 && !pagina_sitio(&pag[k], s->w, s->h, &ox, &oy)) k = -1;
	if (k < 0) {
		int v = -1;
		for (int i = 0; i < npag; ++i) if (pag[i].usada != quadro && (v < 0 || pag[i].usada < pag[v].usada)) v = i;
		if (v >= 0) {
			pagina_vaciar(v);
			if (pagina_sitio(&pag[v], s->w, s->h, &ox, &oy)) k = v;
		}
	}
	if (k < 0) {
		if (!atlas_cheio_avisado) { atlas_cheio_avisado = 1; AOEN_LOG("[AOEN] GPU: atlas lleno (%d páginas en uso en un cuadro): faltan sprites", npag); }
		return NULL;
	}
	e = atlas_entrada_nova(s, var);
	if (!e) return NULL;
	e->pag = (short)k; e->gen = (short)pag[k].gen; e->x = (short)ox; e->y = (short)oy;
	SDL_Rect q = { ox, oy, s->w, s->h };
	SDL_UpdateTexture(pag[k].t, &q, conv, s->w * 4);
	if (pag[k].cpu) for (int y = 0; y < s->h; ++y) memcpy(pag[k].cpu + (size_t)(oy + y) * pag[k].lado + ox, conv + (size_t)y * s->w, (size_t)s->w * 4);
	if (pag[k].subida != quadro) { pag[k].subida = quadro; med_subidas_kb += (unsigned long)pag[k].lado * pag[k].lado * 4 / 1024; }
	pag[k].usada = quadro;
	pag[k].sprites++;
	return e;
}

/* La página de paso, a las grandes (al empezar un cuadro, si se llenó): cada grande que recibe algo se sube una vez. */
static void atlas_promover(void) {
	Pagina *e0 = &pag[PAG_EST];
	if (!e0->t || !e0->sprites) return;
	int movidos = 0;
	for (int i = 0; i < TAB; ++i) {
		Entrada *e = &tab[i];
		if (!e->s || e->pag != PAG_EST || e->gen != e0->gen) continue;
		const Sprite *s = e->s;
		int k = -1, ox = 0, oy = 0;
		for (int j = 0; j < npag && k < 0; ++j) if (pagina_sitio(&pag[j], s->w, s->h, &ox, &oy)) k = j;
		if (k < 0 && (k = pagina_nova()) >= 0 && !pagina_sitio(&pag[k], s->w, s->h, &ox, &oy)) k = -1;
		if (k < 0) {   /* todas llenas: se vacía la grande menos usada (al empezar el cuadro ninguna está en uso) */
			int v = -1;
			for (int j = 0; j < npag; ++j) if (v < 0 || pag[j].usada < pag[v].usada) v = j;
			if (v >= 0) { pagina_vaciar(v); if (pagina_sitio(&pag[v], s->w, s->h, &ox, &oy)) k = v; }
		}
		if (k < 0) continue;   /* (no debería pasar) queda vieja al vaciar la de paso: se volverá a convertir */
		SDL_Rect q = { ox, oy, s->w, s->h };
		SDL_UpdateTexture(pag[k].t, &q, e0->cpu + (size_t)e->y * e0->lado + e->x, e0->lado * 4);
		if (pag[k].subida != quadro) { pag[k].subida = quadro; med_subidas_kb += PAG_LADO * PAG_LADO * 4 / 1024; }
		pag[k].sprites++;
		e->pag = (short)k; e->gen = (short)pag[k].gen; e->x = (short)ox; e->y = (short)oy;
		++movidos;
	}
	pagina_vaciar(PAG_EST);
	est_cheia = 0;
	(void)movidos;
}

/* ---------------------------------------------------------------- operaciones del mundo */

enum { OP_REL, OP_LIN, OP_OSC, OP_ELI, OP_SPR };
typedef struct {
	u8 tipo, flip;
	short jog;
	int x, y, w, h;      /* rectángulo; línea: extremos; elipse: centro y radios; sprite: anclaje, escala y sombra (h) */
	u32 col;
	int alfa;
	const Sprite *s;
	const Entrada *e;
	Recorte r;
} Op;
static Op *ops; static int nops, cap_ops;

static Op *op_nova(int tipo, const Recorte *r) {
	if (nops == cap_ops) {
		int c = cap_ops ? cap_ops * 2 : 4096;
		Op *n = (Op *)realloc(ops, sizeof(Op) * (size_t)c);
		if (!n) return NULL;
		ops = n; cap_ops = c;
	}
	Op *o = &ops[nops++];
	o->tipo = (u8)tipo; o->r = *r; o->s = NULL; o->e = NULL;
	return o;
}

void gpu_op_relleno(int x, int y, int w, int h, u32 col, int alfa, const Recorte *r) {
	if (w <= 0 || h <= 0 || alfa <= 0) return;
	Op *o = op_nova(OP_REL, r);
	if (o) { o->x = x; o->y = y; o->w = w; o->h = h; o->col = col; o->alfa = alfa; }
}
void gpu_op_linea(int x0, int y0, int x1, int y1, u32 col, const Recorte *r) {
	Op *o = op_nova(OP_LIN, r);
	if (o) { o->x = x0; o->y = y0; o->w = x1; o->h = y1; o->col = col; o->alfa = 255; }
}
void gpu_op_oscuro(int x, int y, int w, int h, const Recorte *r) {
	if (w <= 0 || h <= 0) return;
	Op *o = op_nova(OP_OSC, r);
	if (o) { o->x = x; o->y = y; o->w = w; o->h = h; }
}
void gpu_op_elipse(int cx, int cy, int rx, int ry, u32 col, int alfa, const Recorte *r) {
	if (rx <= 0 || ry <= 0 || alfa <= 0) return;
	Op *o = op_nova(OP_ELI, r);
	if (o) { o->x = cx; o->y = cy; o->w = rx; o->h = ry; o->col = col; o->alfa = alfa; }
}
void gpu_op_sprite(const Sprite *s, int x, int y, int scale, int player, int shade, int flip, const Recorte *r) {
	if (!s || !s->px || scale <= 0) return;
	Op *o = op_nova(OP_SPR, r);
	if (o) { o->s = s; o->x = x; o->y = y; o->w = scale; o->h = shade; o->jog = (short)player; o->flip = (u8)(flip != 0); }
}

void gpu_mundo_empezar(void) { nops = 0; gfx_gpu = 1; }

/* la página con la que se dibuja lo que no es sprite: la del lote en curso si es del atlas (no corta el lote) */
static int pag_atual(void) {
	for (int k = 0; k < npag; ++k) if (pag[k].t == lote_t) return k;
	if (pag[PAG_EST].t && pag[PAG_EST].t == lote_t) return PAG_EST;
	if (!npag) return pagina_nova();
	return 0;
}

static void emitir_op(const Op *o) {
	if (o->tipo == OP_SPR) {
		const Entrada *e = o->e;
		if (!e) return;
		const float L = (float)pag[e->pag].lado;
		const Sprite *s = o->s;
		int sc = o->w, dw = (s->w * sc + 255) >> 8, dh = (s->h * sc + 255) >> 8;
		if (dw <= 0 || dh <= 0) return;
		int dx0 = o->x - ((s->hx * sc) >> 8), dy0 = o->y - ((s->hy * sc) >> 8);
		if (o->flip) dx0 = o->x - dw + ((s->hx * sc) >> 8);
		float u0 = e->x / L, v0 = e->y / L, u1 = (e->x + s->w) / L, v1 = (e->y + s->h) / L;
		if (o->flip) { float t = u0; u0 = u1; u1 = t; }
		int sh = MIN(o->h, 255);
		quad_recortado(pag[e->pag].t, SDL_BLENDMODE_BLEND, (float)dx0, (float)dy0, (float)(dx0 + dw), (float)(dy0 + dh), u0, v0, u1, v1, cor_de(0xff000000u | (u32)sh * 0x010101u, 255), &o->r);
		return;
	}
	int k = pag_atual();
	if (k < 0) return;
	const float L = (float)pag[k].lado;
	SDL_Texture *t = pag[k].t;
	pag[k].usada = quadro;
	float bu = (BRANCO_X + 0.5f) / L, bv = (BRANCO_Y + 0.5f) / L;
	switch (o->tipo) {
	case OP_REL:
		quad_recortado(t, SDL_BLENDMODE_BLEND, (float)o->x, (float)o->y, (float)(o->x + o->w), (float)(o->y + o->h), bu, bv, bu, bv, cor_de(o->col, o->alfa), &o->r);
		break;
	case OP_OSC:
		quad_recortado(t, SDL_BLENDMODE_BLEND, (float)o->x, (float)o->y, (float)(o->x + o->w), (float)(o->y + o->h), bu, bv, bu, bv, cor_de(0, 128), &o->r);
		break;
	case OP_ELI: {
		/* el disco reservado de la página, estirado a la elipse (como la de la CPU: de cx-rx a cx+rx, de cy-ry a cy+ry) */
		SDL_Color c = o->alfa >= 256 ? cor_de(0, 128) : cor_de(o->col, o->alfa);
		quad_recortado(t, SDL_BLENDMODE_BLEND, (float)(o->x - o->w), (float)(o->y - o->h), (float)(o->x + o->w + 1), (float)(o->y + o->h + 1),
		               DISCO_X / L, 0, (DISCO_X + DISCO_L) / L, DISCO_L / L, c, &o->r);
		break;
	}
	case OP_LIN: {
		int x0 = o->x, y0 = o->y, x1 = o->w, y1 = o->h;
		SDL_Color c = cor_de(o->col, 255);
		if (x0 == x1 || y0 == y1) {
			quad_recortado(t, SDL_BLENDMODE_BLEND, (float)MIN(x0, x1), (float)MIN(y0, y1), (float)(MAX(x0, x1) + 1), (float)(MAX(y0, y1) + 1), bu, bv, bu, bv, c, &o->r);
			break;
		}
		/* oblicua: un cuadrilátero de un píxel de ancho entre los centros de los extremos (alargado medio píxel) */
		float ax = x0 + 0.5f, ay = y0 + 0.5f, bx = x1 + 0.5f, by = y1 + 0.5f, dx = bx - ax, dy = by - ay;
		float len = sqrtf(dx * dx + dy * dy);
		if (len <= 0) break;
		dx /= len; dy /= len;
		ax -= dx * 0.5f; ay -= dy * 0.5f; bx += dx * 0.5f; by += dy * 0.5f;
		float nx = -dy * 0.5f, ny = dx * 0.5f;
		if (ax < o->r.x0 || bx < o->r.x0 || ax > o->r.x1 || bx > o->r.x1 || ay < o->r.y0 || by < o->r.y0 || ay > o->r.y1 || by > o->r.y1) {
			/* fuera del recorte (raro en el mundo): por puntos, como la CPU */
			int ddx = abs(x1 - x0), ddy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = ddx + ddy;
			for (;;) {
				if (x0 >= o->r.x0 && x0 < o->r.x1 && y0 >= o->r.y0 && y0 < o->r.y1)
					quad(t, SDL_BLENDMODE_BLEND, (float)x0, (float)y0, (float)(x0 + 1), (float)(y0 + 1), bu, bv, bu, bv, c);
				if (x0 == x1 && y0 == y1) break;
				int e2 = 2 * err;
				if (e2 >= ddy) { err += ddy; x0 += sx; }
				if (e2 <= ddx) { err += ddx; y0 += sy; }
			}
			break;
		}
		SDL_Vertex q[6];
		float px[4] = { ax + nx, bx + nx, bx - nx, ax - nx }, py[4] = { ay + ny, by + ny, by - ny, ay - ny };
		static const int ord[6] = { 0, 1, 2, 0, 2, 3 };
		for (int i = 0; i < 6; ++i) { q[i].position.x = px[ord[i]]; q[i].position.y = py[ord[i]]; q[i].tex_coord.x = bu; q[i].tex_coord.y = bv; q[i].color = c; }
		gpu_triangulos(t, SDL_BLENDMODE_BLEND, q, 6);
		break;
	}
	}
}

void gpu_mundo_terminar(void) {
	gfx_gpu = 0;
	/* primero todos los sprites al atlas (cada página cambia, y se sube, una sola vez en el cuadro) */
	for (int vez = 0; vez < 2; ++vez) {
		int l0 = limpezas;
		for (int i = 0; i < nops; ++i) if (ops[i].tipo == OP_SPR) ops[i].e = atlas_sprite(ops[i].s, ops[i].jog, 0);
		if (limpezas == l0) break;   /* si la tabla se vació a mitad, se resuelve otra vez */
	}
	capa_fase = 1;
	if (!(quitar & 2)) for (int i = 0; i < nops; ++i) emitir_op(&ops[i]);
	nops = 0;
}

/* ---------------------------------------------------------------- terreno */

SDL_Texture *gpu_textura_fija(const u32 *px, int w, int h) {
	if (!R) return NULL;
	SDL_Texture *t = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
	if (!t) return NULL;
	SDL_UpdateTexture(t, NULL, px, w * 4);
	med_subidas_kb += (unsigned long)w * h * 4 / 1024;
	return t;
}

void gpu_recorte(int y0, int y1) {
	lote_mandar();   /* el recorte de la SDL vale para lo que se mande desde ahora */
	if (y1 <= y0) { SDL_RenderSetClipRect(R, NULL); return; }
	SDL_Rect r = { 0, y0, cw, y1 - y0 };
	SDL_RenderSetClipRect(R, &r);
}

void gpu_loseta_preparar(const Sprite *s) { if (s && s->px) atlas_sprite(s, 0, 1); }

void gpu_loseta(const Sprite *s, int ox, int oy, int scale, const float px[4], const float py[4], const int l[4]) {
	if (!s || !s->px) return;
	Entrada *e = atlas_sprite(s, 0, 1);
	if (!e) return;
	const float L = (float)pag[e->pag].lado, k = 256.0f / (float)scale;
	SDL_Vertex v[5];
	int suma = 0;
	for (int i = 0; i < 4; ++i) {
		float tx = (px[i] - ox) * k, ty = (py[i] - oy) * k;
		tx = CLAMP(tx, 0.5f, s->w - 0.5f); ty = CLAMP(ty, 0.5f, s->h - 0.5f);
		v[i].position.x = px[i]; v[i].position.y = py[i];
		v[i].tex_coord.x = (e->x + tx) / L; v[i].tex_coord.y = (e->y + ty) / L;
		int c = MIN(255, l[i]);
		v[i].color = cor_de((u32)c * 0x010101u, 255);
		suma += l[i];
	}
	/* centro: la media (la CPU interpola bilineal; cuatro triángulos se le acercan más que dos) */
	v[4].position.x = (px[0] + px[3]) * 0.5f; v[4].position.y = (py[0] + py[3]) * 0.5f;
	v[4].tex_coord.x = (v[0].tex_coord.x + v[3].tex_coord.x) * 0.5f; v[4].tex_coord.y = (v[0].tex_coord.y + v[3].tex_coord.y) * 0.5f;
	int cc = MIN(255, suma / 4);
	v[4].color = cor_de((u32)cc * 0x010101u, 255);
	/* izquierda, abajo, derecha, arriba alrededor del centro */
	static const int ord[12] = { 0, 1, 4, 1, 3, 4, 3, 2, 4, 2, 0, 4 };
	SDL_Vertex q[12];
	for (int i = 0; i < 12; ++i) q[i] = v[ord[i]];
	gpu_triangulos(pag[e->pag].t, SDL_BLENDMODE_NONE, q, 12);
}

/* ---------------------------------------------------------------- la capa de la interfaz */

#define FRANJAS 8
static u32 *capa, *capa_ant, *fb_cpu;
static int capa_w, capa_h, franja_h, fb_cpu_pitch;
static u8 tocada[FRANJAS], tocada_ant[FRANJAS], tex_ok[FRANJAS];
/* filas pintadas en el cuadro (fy) y filas con algo en la textura de la franja (ay): solo esas se borran y se comparan */
static int fy0[FRANJAS], fy1[FRANJAS], ay0[FRANJAS], ay1[FRANJAS];
static SDL_Texture *franja_t[FRANJAS];
static int med_franjas;
static int capa_recta;      /* el renderizador no sabe el alfa premultiplicado (la SDL del PC por software): se sube en alfa normal */
/* lo que se dibuja de cada franja: bloques de 64x32 con algo, juntados por filas; los que son opacos enteros van sin mezcla
 * (la GPU no lee lo de debajo). Así no se mezcla una franja entera para unos pocos píxeles de interfaz */
#define BLOQ_L 64
#define BLOQ_A 32
#define CAJAS 64
typedef struct { short x0, y0, x1, y1; u8 opaca; } Caja;
static Caja cajas[FRANJAS][CAJAS];
static int ncajas[FRANJAS];

static void franja_cajas(int k, const u32 *src, int fh) {
	ncajas[k] = 0;
	for (int by = 0; by < fh; by += BLOQ_A) {
		int bh = MIN(BLOQ_A, fh - by), ini = -1, op_ini = 0;
		for (int bx = 0; bx <= capa_w; bx += BLOQ_L) {
			int tipo = 0;   /* 0 vacío, 1 mezcla, 2 opaco */
			if (bx < capa_w) {
				int bw = MIN(BLOQ_L, capa_w - bx), alg = 0, todo = 1;
				for (int y = by; y < by + bh; ++y) {
					const u32 *f = src + (size_t)y * capa_w + bx;
					for (int x = 0; x < bw; ++x) { u32 a = f[x] >> 24; alg |= a != 0; todo &= a == 255; }
				}
				tipo = alg ? (todo ? 2 : 1) : 0;
			}
			if (ini >= 0 && (tipo == 0 || (tipo == 2) != op_ini || bx >= capa_w)) {
				if (ncajas[k] < CAJAS) {
					Caja *c = &cajas[k][ncajas[k]++];
					c->x0 = (short)ini; c->x1 = (short)MIN(bx, capa_w); c->y0 = (short)by; c->y1 = (short)(by + bh); c->opaca = (u8)op_ini;
				}
				ini = -1;
			}
			if (tipo && ini < 0 && bx < capa_w) { ini = bx; op_ini = tipo == 2; }
		}
	}
	if (ncajas[k] == CAJAS) {   /* demasiadas: la franja entera */
		ncajas[k] = 1;
		cajas[k][0] = (Caja){ 0, 0, (short)capa_w, (short)fh, 0 };
	}
}
static u32 *capa_tmp;

static void capa_soltar(void) {
	free(capa); free(capa_ant); capa = capa_ant = NULL;
	for (int k = 0; k < FRANJAS; ++k) { if (franja_t[k]) SDL_DestroyTexture(franja_t[k]); franja_t[k] = NULL; tex_ok[k] = 0; tocada[k] = tocada_ant[k] = 0; }
	capa_w = capa_h = 0;
}

static int capa_preparar(int w, int h) {
	if (capa && capa_w == w && capa_h == h) return 1;
	capa_soltar();
	capa = (u32 *)calloc((size_t)w * h, 4); capa_ant = (u32 *)calloc((size_t)w * h, 4);
	if (!capa || !capa_ant) { capa_soltar(); return 0; }
	capa_w = w; capa_h = h; franja_h = (h + FRANJAS - 1) / FRANJAS;
	for (int k = 0; k < FRANJAS; ++k) {
		int fh = MIN(franja_h, h - k * franja_h);
		if (fh <= 0) break;
		franja_t[k] = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, fh);
		if (!franja_t[k]) { capa_soltar(); return 0; }
		if (SDL_SetTextureBlendMode(franja_t[k], M_PREMULT) != 0 && !capa_recta) {
			capa_recta = 1;
			AOEN_LOG("[AOEN] GPU: el renderizador no sabe alfa premultiplicado; la capa de la interfaz va con alfa normal");
		}
	}
	return 1;
}

void gpu_capa_marca(int y0, int y1) {
	if (!gfx_capa || !franja_h) return;
	y0 = MAX(y0, 0); y1 = MIN(y1, capa_h);
	if (y1 <= y0) return;
	for (int k = y0 / franja_h; k <= (y1 - 1) / franja_h && k < FRANJAS; ++k) {
		if (!tocada[k]) { tocada[k] = 1; fy0[k] = y0; fy1[k] = y1; }
		else { fy0[k] = MIN(fy0[k], y0); fy1[k] = MAX(fy1[k], y1); }
	}
}

void gpu_capa_empezar(void) {
	if (!capa) return;
	for (int k = 0; k < FRANJAS; ++k) {
		if (tocada[k]) {   /* lo del cuadro anterior se borra (sus filas) */
			int y0 = MAX(fy0[k], k * franja_h), y1 = MIN(MIN(capa_h, (k + 1) * franja_h), fy1[k]);
			if (y1 > y0) memset(capa + (size_t)y0 * capa_w, 0, (size_t)(y1 - y0) * capa_w * 4);
		}
		tocada_ant[k] = tocada[k]; tocada[k] = 0;
	}
	fb_cpu = fb.px; fb_cpu_pitch = fb.pitch;
	fb.px = capa; fb.pitch = capa_w;
	gfx_capa = 1;
}

void gpu_capa_terminar(void) {
	if (!gfx_capa) return;
	gfx_capa = 0;
	fb.px = fb_cpu; fb.pitch = fb_cpu_pitch;
}

static void capa_dibujar(void) {
	PERF_INI();
	med_franjas = 0;
	capa_fase = 2;
	if (quitar & 4) return;
	for (int k = 0; k < FRANJAS; ++k) {
		if (!tocada[k] || !franja_t[k]) continue;
		int y0 = k * franja_h, fh = MIN(franja_h, capa_h - y0);
		size_t off = (size_t)y0 * capa_w, n = (size_t)fh * capa_w * 4;
		/* fuera de las filas pintadas ahora (fy) y de las que tiene la textura (ay) las dos están a cero: solo se mira eso */
		int r0 = tex_ok[k] ? MIN(fy0[k], ay0[k]) : y0, r1 = tex_ok[k] ? MAX(fy1[k], ay1[k]) : y0 + fh;
		r0 = MAX(r0, y0); r1 = MIN(r1, y0 + fh);
		size_t ro = (size_t)r0 * capa_w, rn = r1 > r0 ? (size_t)(r1 - r0) * capa_w * 4 : 0;
		if (!tex_ok[k] || (rn && memcmp(capa + ro, capa_ant + ro, rn))) {
			if (rn) memcpy(capa_ant + ro, capa + ro, rn);
			ay0[k] = fy0[k]; ay1[k] = fy1[k];
			const u32 *src = capa + off;
			if (capa_recta) {   /* de premultiplicado a alfa normal */
				if (!capa_tmp) capa_tmp = (u32 *)malloc((size_t)capa_w * franja_h * 4);
				if (!capa_tmp) continue;
				for (size_t i = 0; i < (size_t)fh * capa_w; ++i) {
					u32 c = src[i], a = c >> 24;
					if (!a || a == 255) { capa_tmp[i] = c; continue; }
					u32 r = MIN(255u, ((c >> 16) & 255) * 255 / a), g = MIN(255u, ((c >> 8) & 255) * 255 / a), b = MIN(255u, (c & 255) * 255 / a);
					capa_tmp[i] = (a << 24) | (r << 16) | (g << 8) | b;
				}
				src = capa_tmp;
			}
			SDL_UpdateTexture(franja_t[k], NULL, src, capa_w * 4);
			franja_cajas(k, capa + off, fh);
			tex_ok[k] = 1;
			++med_franjas;
			med_subidas_kb += (unsigned long)(n / 1024);
		}
		PERF_FIN(PERF_CAPA);
		_perf_t0 = perf_activo ? perf_reloj() : 0;
		for (int c = 0; c < ncajas[k]; ++c) {
			const Caja *q = &cajas[k][c];
			int modo = q->opaca ? (int)SDL_BLENDMODE_NONE : capa_recta ? (int)SDL_BLENDMODE_BLEND : (int)M_PREMULT;
			quad(franja_t[k], modo, (float)q->x0, (float)(y0 + q->y0), (float)q->x1, (float)(y0 + q->y1),
			     (float)q->x0 / capa_w, (float)q->y0 / fh, (float)q->x1 / capa_w, (float)q->y1 / fh, cor_de(0xffffff, 255));
		}
	}
}

/* ---------------------------------------------------------------- arranque, cuadro y vuelta a la CPU */

static int forzar = -1;
static int renderer_gpu(void) {
	if (forzar < 0) forzar = getenv("AOEN_GPU_FORZAR") && atoi(getenv("AOEN_GPU_FORZAR")) > 0;
	SDL_RendererInfo i;
	if (!R || SDL_GetRendererInfo(R, &i) != 0) return 0;
	if (forzar) return 1;   /* pruebas: AOEN_GPU_FORZAR=1 acepta un renderizador por software (el mismo camino, sin GPU) */
#ifdef SAGUI_AOEN
	return i.name && !strcmp(i.name, "saguios-gpu");
#else
	return (i.flags & SDL_RENDERER_ACCELERATED) != 0;
#endif
}

int gpu_iniciar(SDL_Renderer *rnd) {
	R = rnd;
	pode = 0;
	if (!R) { AOEN_LOG("[AOEN] dibujo por GPU: no hay renderizador; sigue por la CPU"); return 0; }
	if (!renderer_gpu()) { AOEN_LOG("[AOEN] dibujo por GPU: el renderizador no lleva la GPU (sin 3D); sigue por la CPU"); return 0; }
	M_PREMULT = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
	                                       SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
	SDL_RendererInfo i;
	SDL_GetRendererInfo(R, &i);
	if (i.max_texture_width && i.max_texture_width < PAG_LADO) { AOEN_LOG("[AOEN] dibujo por GPU: texturas de %d como mucho; sigue por la CPU", i.max_texture_width); return 0; }
	if (getenv("AOEN_GPU_QUITAR")) quitar = atoi(getenv("AOEN_GPU_QUITAR"));
	const char *e = getenv("AOEN_GPU_PAGINAS");
	if (e && atoi(e) > 0) pag_max = MIN(PAG_MAX, atoi(e));
	pode = 1;
	AOEN_LOG("[AOEN] dibujo por GPU (D436): renderizador %s, atlas de %dx%d (hasta %d páginas)", i.name ? i.name : "?", PAG_LADO, PAG_LADO, pag_max);
	return 1;
}

void gpu_apagar(const char *motivo) {
	if (!pode) return;
	pode = 0; gpu_ativo = 0; gfx_gpu = 0;
	gpu_capa_terminar();
	lote_n = 0;
	AOEN_LOG("[AOEN] dibujo por GPU apagado: %s; sigue por la CPU", motivo);
	world_msg("Dibujo por GPU desactivado: sigue por la CPU");
}

const char *gpu_estado(int *presenta_gpu) {
	unsigned long d = 0, ram = 0;
	(void)ram;
	int g = 0;
#ifdef SAGUI_AOEN
	g = R ? sagui_sdl2_gpu_estado(R, &d, &ram) : 0;
#endif
	if (presenta_gpu) *presenta_gpu = g && d > 0;
	return gpu_ativo ? "GPU" : "CPU";
}

int gpu_cuadro_empezar(int w, int h) {
	gpu_ativo = 0;
	if (!pode || !gpu_pedido) return 0;
	if (!renderer_gpu()) { gpu_apagar("el renderizador dejó la GPU"); return 0; }
	if (w != cw || h != ch) { cw = w; ch = h; }
	if (!capa_preparar(w, h)) { gpu_apagar("sin memoria para la capa de la interfaz"); return 0; }
	++quadro;
	med_tris = med_lotes = med_subidas_kb = 0;
	if (est_cheia || pag[PAG_EST].y_livre > EST_LADO * 3 / 4) atlas_promover();
	lote_n = 0; lote_t = NULL;
	SDL_RenderSetClipRect(R, NULL);
	SDL_SetRenderDrawColor(R, 0, 0, 0, 255);
	SDL_RenderClear(R);
	capa_fase = 0;
	gpu_ativo = 1;
	return 1;
}

void gpu_cuadro_presentar(u32 *shot, int pitch_px) {
	if (!gpu_ativo) return;
	capa_dibujar();
	capa_fase = 0;
	lote_mandar();
	if (shot) SDL_RenderReadPixels(R, NULL, SDL_PIXELFORMAT_ARGB8888, shot, pitch_px * 4);
	{
		unsigned long long t0 = perf_reloj();
		PERF_INI();
		SDL_RenderPresent(R);
		PERF_FIN(PERF_MOSTRAR);
		espera += perf_reloj() - t0;
	}
	ativo_ultimo = 1; t_ultimo = SDL_GetTicks();
	ultimas.tris = med_tris; ultimas.lotes = med_lotes; ultimas.subidas_kb = med_subidas_kb;
	ultimas.paginas = npag; ultimas.paginas_max = pag_max; ultimas.sprites = tab_n;
	ultimas.franjas_subidas = med_franjas; ultimas.franjas = FRANJAS;
	soma_tris += med_tris; soma_lotes += med_lotes; soma_kb += med_subidas_kb; soma_franjas += (unsigned)med_franjas; ++soma_cuadros;
	gpu_ativo = 0;
	/* el renderizador pudo dejar la GPU en este cuadro (la SDL ya lo trajo a la RAM: no se pierde nada) */
	if (!renderer_gpu()) gpu_apagar("el renderizador dejó la GPU");
}

void gpu_medidas(GpuMedidas *m) { *m = ultimas; }

/* La opción de Opciones > Gráficos: apagarlo vale ya; encenderlo, ya si el renderizador lleva la GPU (se pidió al arrancar),
 * si no, al volver a abrir el juego. */
static int opcion_reinicio;
void gpu_opcion(int si) {
	if (!si) { gpu_pedido = 0; opcion_reinicio = 0; AOEN_LOG("[AOEN] dibujo por GPU: apagado en las opciones"); return; }
	if (pode) { gpu_pedido = 1; opcion_reinicio = 0; AOEN_LOG("[AOEN] dibujo por GPU: encendido en las opciones"); }
	else opcion_reinicio = 1;
}
const char *gpu_opcion_texto(void) {
	if (opcion_reinicio) return "(al volver a abrir el juego)";
	if (gpu_pedido && pode) return gpu_ativo_ultimo() ? "en uso" : "en pantalla completa";
	if (gpu_pedido || cfg_get_int("dibujo_gpu", 0)) return "no disponible: va por la CPU";
	return "";
}

/* sumas para el informe del banco (AOEN_PERFIL) */
void gpu_informe_empezar(void) { soma_tris = soma_lotes = soma_kb = soma_franjas = soma_cuadros = 0; }
void gpu_informe(void) {
	if (!soma_cuadros) return;
	AOEN_LOG("[AOEN] GPU por cuadro: %llu triángulos en %llu lotes, %llu KB subidos, %llu franjas de la capa subidas; atlas: %d páginas, %d sprites",
	         soma_tris / soma_cuadros, soma_lotes / soma_cuadros, soma_kb / soma_cuadros, soma_franjas / soma_cuadros, npag, tab_n);
}
