/* SPDX-License-Identifier: GPL-3.0-or-later */
/* aoen_gpu_unidad.c - prueba de unidad en el PC del dibujo por GPU de AoEN (D436, tests/h436_aoen_gpu.py «unidad»).
 *
 * Compila userland/juegos/aoen/gpu.c con la SDL 2 del PC (renderizador por software, vídeo «dummy») y AOEN_GPU_FORZAR=1:
 * el mismo camino que en SaguiOS, pero la «GPU» es la SDL del PC. Comprueba:
 *  - lotes: muchos sprites del mismo atlas van en pocas llamadas a SDL_RenderGeometry;
 *  - atlas: cada sprite sale con sus colores (transparente, sombra a 140/256, color de jugador, sombra de la niebla), el
 *    mismo sprite con otro jugador es otra entrada solo si lleva color de jugador, y tras pasar la página de paso a las
 *    grandes (o vaciar una por falta de sitio) se sigue dibujando bien;
 *  - orden: lo que se graba después tapa a lo de antes (rellenos, sprites, sombras), y el recorte de cada operación;
 *  - la capa de la interfaz: lo pintado se ve encima y, si el cuadro siguiente no la toca, desaparece.
 * Sale con 0 si todo va bien. */
#include "gpu.h"
#include "ui.h"
#include "hilos.h"
#include "config.h"

/* lo que gpu.c usa del resto de AoEN */
Fb fb;
int perf_activo;
unsigned long long perf_reloj(void) { return (unsigned long long)SDL_GetPerformanceCounter(); }
void perf_sumar(int f, unsigned long long d) { (void)f; (void)d; }
static int pal_ver = 1;
int fb_pal_version(void) { return pal_ver; }
void world_msg(const char *s) { fprintf(stderr, "aviso: %s\n", s); }
int cfg_get_int(const char *k, int d) { (void)k; return d; }

static int fallos, pruebas;
#define VER(c, ...) do { ++pruebas; if (!(c)) { ++fallos; fprintf(stderr, "MAL: "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } } while (0)

#define LW 320
#define LH 240
static u32 tela[LW * LH];
static SDL_Renderer *R;

static u32 px(int x, int y) { return tela[y * LW + x] & 0xffffff; }
static int perto(u32 a, u32 b, int tol) {
	for (int k = 0; k < 24; k += 8) { int d = (int)((a >> k) & 255) - (int)((b >> k) & 255); if (d < -tol || d > tol) return 0; }
	return 1;
}

/* un sprite de w x h con índice v (0 = transparente) y, si pc, la mitad derecha del color del jugador */
static Sprite *sprite(int w, int h, u8 v, int pc, int hx, int hy) {
	Sprite *s = (Sprite *)calloc(1, sizeof *s);
	s->w = w; s->h = h; s->hx = hx; s->hy = hy;
	s->px = (u8 *)calloc((size_t)w * h, 1); s->pc = (u8 *)calloc((size_t)w * h, 1);
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x) {
			s->px[y * w + x] = v;
			if (pc && x >= w / 2) { s->px[y * w + x] = 2; s->pc[y * w + x] = 1; }   /* 2 + 16 * (jugador + 1) */
		}
	return s;
}

static const Recorte TODO = { 0, 0, LW, LH };

static void cuadro(void (*dibujar)(void)) {
	VER(gpu_cuadro_empezar(LW, LH), "el cuadro empieza por la «GPU»");
	gpu_mundo_empezar();
	dibujar();
	gpu_mundo_terminar();
	gpu_capa_empezar();
	gpu_capa_terminar();
	gpu_cuadro_presentar(tela, LW);
}

static Sprite *rojo, *verde, *sombra, *jug, *hueco, *grandes[64];

static void d_lotes(void) {
	gpu_op_relleno(0, 0, LW, LH, 0xff202020u, 255, &TODO);
	for (int k = 0; k < 200; ++k) gpu_op_sprite(k % 2 ? rojo : verde, 10 + (k % 20) * 14, 10 + (k / 20) * 20, 256, 0, 256, 0, &TODO);
}

static void d_colores(void) {
	gpu_op_relleno(0, 0, LW, LH, 0xff808080u, 255, &TODO);
	gpu_op_sprite(rojo, 20, 20, 256, 0, 256, 0, &TODO);           /* 20..29 x 20..29 */
	gpu_op_sprite(rojo, 40, 20, 256, 0, 140, 0, &TODO);           /* niebla: 140/256 */
	gpu_op_sprite(sombra, 60, 20, 256, 0, 256, 0, &TODO);         /* negro puro: sombra sobre el gris */
	gpu_op_sprite(jug, 80, 20, 256, 0, 256, 0, &TODO);            /* derecha: color del jugador 0 */
	gpu_op_sprite(jug, 100, 20, 256, 1, 256, 0, &TODO);           /* jugador 1 */
	gpu_op_sprite(jug, 130, 20, 256, 1, 256, 1, &TODO);           /* jugador 1 y espejo (de 120 a 129): el color a la izquierda */
	gpu_op_sprite(hueco, 140, 20, 256, 0, 256, 0, &TODO);         /* transparente: se ve el gris */
	gpu_op_sprite(rojo, 20, 60, 512, 0, 256, 0, &TODO);           /* x2: 20..39 x 60..79 */
}

static void d_orden(void) {
	gpu_op_relleno(0, 0, LW, LH, 0xff000000u, 255, &TODO);
	gpu_op_relleno(10, 100, 40, 40, 0xffff0000u, 255, &TODO);
	gpu_op_sprite(verde, 20, 110, 256, 0, 256, 0, &TODO);        /* el verde tapa al rojo */
	gpu_op_relleno(25, 115, 10, 10, 0xff0000ffu, 255, &TODO);    /* y el azul al verde */
	gpu_op_elipse(100, 120, 10, 10, 0, 256, &TODO);              /* sombra sobre negro: negro */
	gpu_op_relleno(130, 100, 40, 40, 0xffffffffu, 255, &TODO);
	gpu_op_elipse(150, 120, 10, 6, 0, 256, &TODO);               /* sombra: mitad de luz sobre el blanco */
	gpu_op_relleno(180, 100, 40, 40, 0xffffffffu, 255, &TODO);
	gpu_op_oscuro(180, 100, 20, 40, &TODO);                      /* mitad izquierda a la mitad */
	Recorte r = { 230, 100, 250, 120 };
	gpu_op_relleno(220, 90, 50, 50, 0xffffff00u, 255, &r);       /* solo dentro del recorte */
	gpu_op_linea(10, 200, 100, 200, 0xff00ff00u, &TODO);
	gpu_op_relleno(200, 180, 20, 20, 0xffffffffu, 128, &TODO);   /* mezcla al 50 % sobre negro */
}

static int ngr;
static void d_grandes(void) {
	gpu_op_relleno(0, 0, LW, LH, 0xff000000u, 255, &TODO);
	for (int k = 0; k < ngr; ++k) gpu_op_sprite(grandes[k], 0, 0, 256, 0, 256, 0, &(Recorte){ 0, 0, 1, 1 });
	gpu_op_sprite(rojo, 200, 200, 256, 0, 256, 0, &TODO);
}

int main(void) {
	setenv("SDL_VIDEODRIVER", "dummy", 1);
	setenv("AOEN_GPU_FORZAR", "1", 1);
	setenv("AOEN_GPU_PAGINAS", "1", 1);   /* una sola página grande: el atlas se llena y hay que vaciar */
	if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 2; }
	SDL_Window *w = SDL_CreateWindow("aoen_gpu_unidad", 0, 0, LW, LH, 0);
	R = w ? SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE) : NULL;
	if (!R) { fprintf(stderr, "sin renderizador: %s\n", SDL_GetError()); return 2; }
	for (int i = 0; i < 256; ++i) fb.pal[i] = 0xff000000u | (u32)(i * 0x010101);
	fb.pal[1] = 0xffff0000u; fb.pal[3] = 0xff00ff00u; fb.pal[4] = 0xff000000u; fb.pal[5] = 0xff000000u | 0x123456;
	fb.pal[2 + 16] = 0xff0000ffu; fb.pal[2 + 32] = 0xffffff00u;   /* color de jugador 0 (azul) y 1 (amarillo) */
	rojo = sprite(10, 10, 1, 0, 0, 0); verde = sprite(10, 10, 3, 0, 0, 0); sombra = sprite(10, 10, 4, 0, 0, 0);
	jug = sprite(10, 10, 5, 1, 0, 0); hueco = sprite(10, 10, 0, 0, 0, 0);
	gpu_pedido = 1;
	VER(gpu_iniciar(R), "gpu_iniciar con el renderizador del PC (forzado)");

	/* lotes */
	cuadro(d_lotes);
	GpuMedidas m;
	gpu_medidas(&m);
	VER(m.tris >= 402 && m.lotes <= 3, "200 sprites y un relleno en %lu lotes (%lu triángulos): uno o pocos", m.lotes, m.tris);
	VER(m.sprites == 2, "dos sprites distintos en el atlas (%d)", m.sprites);
	VER(perto(px(10, 10), 0x00ff00, 2) && perto(px(24, 10), 0xff0000, 2), "los sprites salen con su color (%06x %06x)", px(10, 10), px(24, 10));

	/* colores */
	cuadro(d_colores);
	VER(perto(px(25, 25), 0xff0000, 2), "sprite: su color (%06x)", px(25, 25));
	VER(perto(px(45, 25), 0x8b0000, 4), "niebla: 140/256 del color (%06x)", px(45, 25));
	VER(perto(px(65, 25), 0x464646, 6), "sombra: 140/256 de lo de debajo (%06x)", px(65, 25));
	VER(perto(px(82, 25), 0x123456, 2) && perto(px(87, 25), 0x0000ff, 2), "color del jugador 0 a la derecha (%06x %06x)", px(82, 25), px(87, 25));
	VER(perto(px(107, 25), 0xffff00, 2), "jugador 1: otro color (%06x)", px(107, 25));
	VER(perto(px(122, 25), 0xffff00, 2) && perto(px(127, 25), 0x123456, 2), "espejo: el color del jugador a la izquierda (%06x %06x)", px(122, 25), px(127, 25));
	VER(perto(px(145, 25), 0x808080, 2), "transparente: se ve lo de debajo (%06x)", px(145, 25));
	VER(perto(px(38, 78), 0xff0000, 2) && perto(px(41, 78), 0x808080, 2), "escala x2: 20 píxeles (%06x %06x)", px(38, 78), px(41, 78));
	gpu_medidas(&m);
	VER(m.sprites == 6, "rojo, verde, sombra, hueco y el del jugador dos veces (0 y 1): 6 entradas en el atlas (%d)", m.sprites);

	/* orden y recorte */
	cuadro(d_orden);
	VER(perto(px(12, 102), 0xff0000, 2) && perto(px(22, 112), 0x00ff00, 2) && perto(px(28, 118), 0x0000ff, 2),
	    "orden: rojo, verde encima, azul encima (%06x %06x %06x)", px(12, 102), px(22, 112), px(28, 118));
	VER(perto(px(100, 120), 0, 2), "sombra sobre negro (%06x)", px(100, 120));
	VER(perto(px(150, 120), 0x7f7f7f, 3) && perto(px(132, 102), 0xffffff, 2), "sombra: mitad de luz dentro, blanco fuera (%06x %06x)", px(150, 120), px(132, 102));
	VER(perto(px(185, 120), 0x7f7f7f, 3) && perto(px(210, 120), 0xffffff, 2), "oscurecer: la mitad (%06x %06x)", px(185, 120), px(210, 120));
	VER(perto(px(240, 110), 0xffff00, 2) && perto(px(225, 95), 0, 2) && perto(px(260, 130), 0, 2), "recorte (%06x %06x %06x)", px(240, 110), px(225, 95), px(260, 130));
	VER(perto(px(50, 200), 0x00ff00, 2) && perto(px(50, 201), 0, 2), "línea de un píxel (%06x %06x)", px(50, 200), px(50, 201));
	VER(perto(px(210, 190), 0x808080, 3), "relleno al 50 %% (%06x)", px(210, 190));

	/* atlas lleno: con una página grande, sprites de 300x300 obligan a pasar la de paso y a vaciar páginas; lo de cada
	 * cuadro se sigue viendo */
	for (int k = 0; k < 64; ++k) grandes[k] = sprite(150 + (k % 5) * 20, 150, (u8)(10 + k), 0, 0, 0);
	for (int f = 0; f < 20; ++f) {
		ngr = 4;
		for (int k = 0; k < 4; ++k) grandes[k] = grandes[(f * 4 + k) % 64];
		cuadro(d_grandes);
		VER(perto(px(205, 205), 0xff0000, 2), "cuadro %d con el atlas lleno: el sprite pequeño sigue bien (%06x)", f, px(205, 205));
	}
	gpu_medidas(&m);
	printf("atlas: %d páginas de %d, %d sprites\n", m.paginas, m.paginas_max, m.sprites);
	VER(m.paginas >= 1 && m.paginas <= m.paginas_max, "páginas dentro del límite");
	/* el primero vuelve tras muchas vueltas: se vuelve a convertir y sale bien */
	cuadro(d_colores);
	VER(perto(px(25, 25), 0xff0000, 2) && perto(px(107, 25), 0xffff00, 2), "tras desalojos, los sprites viejos vuelven bien (%06x %06x)", px(25, 25), px(107, 25));

	/* la capa: lo pintado en ella se ve encima; si el cuadro siguiente no la toca, desaparece */
	VER(gpu_cuadro_empezar(LW, LH), "cuadro con capa");
	gpu_mundo_empezar(); gpu_op_relleno(0, 0, LW, LH, 0xff000000u, 255, &TODO); gpu_mundo_terminar();
	gpu_capa_empezar();
	gfx_capa = 1;
	for (int y = 50; y < 60; ++y) for (int x = 50; x < 60; ++x) fb.px[y * fb.pitch + x] = 0xff00ffffu;
	for (int y = 50; y < 60; ++y) for (int x = 70; x < 80; ++x) fb.px[y * fb.pitch + x] = 0x80000000u;   /* negro al 50 % */
	gpu_capa_marca(50, 60);
	gpu_capa_terminar();
	gpu_cuadro_presentar(tela, LW);
	VER(perto(px(55, 55), 0x00ffff, 2), "la capa se ve encima (%06x)", px(55, 55));
	cuadro(d_colores);
	VER(perto(px(55, 55), 0x808080, 2), "sin tocarla, la capa ya no está (%06x)", px(55, 55));

	printf("aoen_gpu_unidad: %d pruebas, %d mal\n", pruebas, fallos);
	SDL_DestroyRenderer(R);
	SDL_Quit();
	return fallos ? 1 : 0;
}
