/* SPDX-License-Identifier: GPL-3.0-or-later */
/* visor.c - «tzar»: visor de los mapas de Tzar con la SDL2 (D417, fase 2 de docs/tzar.md).
 *
 * Abre un mapa de la instalación (suelto en Maps/ o dentro de una campaña de Campaign/) y lo dibuja con su
 * terreno, acantilados, árboles, edificios, unidades y objetos en reposo; se recorre con las flechas, el
 * ratón en los bordes o pulsando en el minimapa. N y P cambian de mapa. Fase 3: el mundo se anima (agua y costas a
 * 4 por segundo, unidades, banderas, peces, pájaros), un clic selecciona un objeto (marco, barra de vida y ficha con
 * su nombre de STRINGS.INI) y H muestra las barras de vida. Código propio (no sale de ReTzar).
 *
 * Datos: el primer argumento, TZAR_DATA o /jogos/TZAR (input_share/games/TZAR en las imágenes privadas): la
 * carpeta de la instalación, con Packs/ y Maps/. Mapa: segundo argumento o TZAR_MAPA («Maps/MP1.wmp» o
 * «Campaign/X.wcm|MAPS/Y.wmp»). Pruebas: TZAR_SHOT=fichero.bmp y TZAR_FRAMES=n (guarda el cuadro n y sale),
 * TZAR_POS=x,y (casilla de arriba a la izquierda), TZAR_W y TZAR_H (tamaño de la ventana), TZAR_MS=n (reloj fijo de la
 * animación, para capturas repetibles), TZAR_CLICK=x,y (un clic en ese píxel de la ventana en el primer cuadro).
 * Cada suceso sale al registro como «[TZAR] ...».
 * Fase 5: con J (o TZAR_JOGAR=1) el mapa se juega: la simulación (mundo.h) mueve el mundo y partida.c dibuja y
 * recibe el ratón y las teclas (ver allí los controles). TZAR_SEMENTE fija la semilla; con TZAR_FRAMES, un tic por
 * cuadro (repetible).
 */
#include "tzar.h"
#include "partida.h"
#include "ia/ia.h"
#include <SDL.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#ifdef SAGUI_TZAR
#include "sagui.h"
#endif

extern const unsigned char fonte_sagui[256][16];   /* la letra de 8x16 del sistema (Latin-1) */

static void registro(const char *fmt, ...)
{
	char b[400];
	va_list a;
	va_start(a, fmt);
	vsnprintf(b, sizeof b, fmt, a);
	va_end(a);
#ifdef SAGUI_TZAR
	registrar_texto(b);
#else
	fprintf(stderr, "%s\n", b);
#endif
}

/* ------------------------------------------------------------------ lienzo */

static SDL_Window *win;
static SDL_Surface *fbs;
static uint32_t *fb;
static int fw, fh, fpitch;
static TzLienzo lz;
static TzFonte fonte;
static int tem_fonte;
static TzTextos textos;
static int tem_textos;
static int sel_obj = -1, barras;
static uint64_t reloj;

static void retangulo(int x, int y, int w, int h, uint32_t c)
{
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > fw) w = fw - x;
	if (y + h > fh) h = fh - y;
	for (int j = 0; j < h; ++j)
		for (int i = 0; i < w; ++i) fb[(y + j) * fpitch + x + i] = c;
}

static void escurecer(int x, int y, int w, int h)
{
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > fw) w = fw - x;
	if (y + h > fh) h = fh - y;
	for (int j = 0; j < h; ++j)
		for (int i = 0; i < w; ++i) { uint32_t *p = &fb[(y + j) * fpitch + x + i]; *p = (*p >> 1) & 0x7f7f7f; }
}

static void texto(int x, int y, const char *s, uint32_t cor)
{
	for (; *s; ++s, x += 8) {
		const unsigned char *g = fonte_sagui[(unsigned char)*s];
		for (int r = 0; r < 16; ++r)
			for (int c = 0; c < 8; ++c)
				if (g[r] & (0x80 >> c)) {
					int px = x + c, py = y + r;
					if (px >= 0 && py >= 0 && px < fw && py < fh) fb[py * fpitch + px] = cor;
					if (px + 1 < fw && py + 1 < fh && px >= -1 && py >= -1) {
						uint32_t *q = &fb[(py + 1) * fpitch + px + 1];
						if (*q != cor) *q = 0;   /* sombra */
					}
				}
	}
}

static void pintar(const TzImg *im, int x, int y) { tz_pintar(&lz, im, x, y); }

/* ------------------------------------------------------------------ mapas de la instalación */

typedef struct { char fich[512]; char dentro[300]; } Entrada;
static Entrada *lista;
static int nlista;

static int cmp_entrada(const void *a, const void *b)
{
	const Entrada *x = a, *y = b;
	int c = strcasecmp(x->fich, y->fich);
	return c ? c : strcasecmp(x->dentro, y->dentro);
}

static void anadir(const char *fich, const char *dentro)
{
	Entrada *n = realloc(lista, (size_t)(nlista + 1) * sizeof *n);
	if (!n) return;
	lista = n;
	snprintf(lista[nlista].fich, sizeof lista[0].fich, "%s", fich);
	snprintf(lista[nlista].dentro, sizeof lista[0].dentro, "%s", dentro);
	nlista++;
}

static void listar_mapas(const char *raiz)
{
	const char *pastas[2] = { "Maps", "Campaign" };
	int desde = 0;
	for (int k = 0; k < 2; ++k) {
		char ruta[700];
		snprintf(ruta, sizeof ruta, "%s/%s", raiz, pastas[k]);
		DIR *d = opendir(ruta);
		struct dirent *de;
		while (d && (de = readdir(d))) {
			size_t l = strlen(de->d_name);
			if (l < 5) continue;
			char f[1300];
			snprintf(f, sizeof f, "%s/%s", pastas[k], de->d_name);
			if (!strcasecmp(de->d_name + l - 4, ".wmp")) anadir(f, "");
			else if (!strcasecmp(de->d_name + l - 4, ".wcm")) {
				char abs_[2100];
				snprintf(abs_, sizeof abs_, "%s/%s", raiz, f);
				TzHpfs c;
				if (tz_hpfs_abrir(&c, abs_)) continue;
				for (int i = 0; i < c.n; ++i) {
					size_t l2 = strlen(c.e[i].nome);
					if (l2 > 4 && !strcasecmp(c.e[i].nome + l2 - 4, ".wmp")) anadir(f, c.e[i].nome);
				}
				tz_hpfs_fechar(&c);
			}
		}
		if (d) closedir(d);
		/* primero los mapas sueltos y después las campañas, cada grupo por nombre */
		if (nlista - desde > 1) qsort(lista + desde, (size_t)(nlista - desde), sizeof *lista, cmp_entrada);
		desde = nlista;
	}
}

/* ------------------------------------------------------------------ estado del visor */

static TzRecursos rec;
static TzMapa mapa;
static TzTerreno ter;
static TzBiblioteca bib;
static TzEscena esc;
static uint32_t *minimapa;
static int cargado, actual = -1;
static int camx, camy;   /* esquina del mundo en píxeles */
static char titulo[400];
static TzPartida part;
static int jogar;

static void descargar(void)
{
	if (!cargado) return;
	tz_partida_liberar(&part);
	tz_escena_liberar(&esc);
	tz_biblioteca_liberar(&bib);
	tz_terreno_liberar(&ter);
	tz_mapa_liberar(&mapa);
	free(minimapa);
	minimapa = NULL;
	cargado = 0;
}

/* la IA original (fase 6): un jugador por cada reino del ordenador; TZAR_SEM_IA=1 la quita */
static TzIaBiblioteca ia_bib;
static int ia_bib_ok = -1;
static TzIa *ias[TZ_REINOS];
static int nias;

static void ia_soltar(void)
{
	for (int i = 0; i < nias; ++i) tz_ia_liberar(ias[i]);
	nias = 0;
}

static void ia_iniciar(void)
{
	if (getenv("TZAR_SEM_IA") || getenv("TZAR_DEMO")) return;
	if (ia_bib_ok < 0) {
		ia_bib_ok = !tz_ia_bib_carregar(&ia_bib, &rec);
		if (!ia_bib_ok) registro("[TZAR] sin guiones de IA: %s", tz_erro());
	}
	if (!ia_bib_ok) return;
	TzIaErro e;
	nias = tz_ia_criar_todas(&part.m, &ia_bib, ias, TZ_REINOS, &e);
	if (e.mensagem) {
		char t[600];
		tz_ia_erro_texto(&e, t, sizeof t);
		registro("[TZAR] IA: %s", t);
	}
	registro("[TZAR] IA original en %d reinos", nias);
}

static void iniciar_partida(void)
{
	ia_soltar();
	tz_partida_liberar(&part);
	uint32_t semente = getenv("TZAR_SEMENTE") ? (uint32_t)strtoul(getenv("TZAR_SEMENTE"), NULL, 0) : 666;
	Uint32 t0 = SDL_GetTicks();
	if (tz_partida_iniciar(&part, &rec, &bib, &mapa, semente)) { registro("[TZAR] no se puede jugar el mapa: %s", tz_erro()); jogar = 0; return; }
	/* partida libre como el original (castillo y tres peones por reino): TZAR_INICIO=1 */
	if (getenv("TZAR_INICIO") && tz_mundo_inicio_livre(&part.m)) registro("[TZAR] inicio libre: %s", tz_erro());
	if (getenv("TZAR_DEMO")) {
		TzGancho g;
		memset(&g, 0, sizeof g);
		g.tic = tz_demo_tic; g.cada = 1;
		tz_mundo_gancho(&part.m, &g);
	}
	ia_iniciar();
	registro("[TZAR] partida: jugador %d, %d objetos, %d tecnologias, semilla %u (%u ms)", part.m.jogador, part.m.nobj, part.cat.ntec, semente,
	         (unsigned)(SDL_GetTicks() - t0));
}

static int cargar(int i)
{
	descargar();
	if (i < 0 || i >= nlista) return tz_falha("no hay mapas");
	const Entrada *e = &lista[i];
	char abs_[1100];
	snprintf(abs_, sizeof abs_, "%s/%s", rec.raiz, e->fich);
	TzHpfs c, sub;
	if (tz_hpfs_abrir(&c, abs_)) return -1;
	TzHpfs *m = &c;
	if (e->dentro[0]) {
		if (tz_hpfs_anidado(&c, e->dentro, &sub)) { tz_hpfs_fechar(&c); return -1; }
		m = &sub;
	}
	int r = tz_mapa_ler(m, &mapa);
	if (e->dentro[0]) tz_hpfs_fechar(&sub);
	tz_hpfs_fechar(&c);
	if (r) return -1;
	Uint32 t0 = SDL_GetTicks();
	if (tz_terreno_carregar(&ter, &rec, mapa.bioma)) { tz_mapa_liberar(&mapa); return -1; }
	if (tz_biblioteca_carregar(&bib, &rec, mapa.bioma)) { tz_terreno_liberar(&ter); tz_mapa_liberar(&mapa); return -1; }
	if (tz_escena_montar(&esc, &bib, &rec, &ter, &mapa)) {
		tz_biblioteca_liberar(&bib); tz_terreno_liberar(&ter); tz_mapa_liberar(&mapa);
		return -1;
	}
	minimapa = malloc((size_t)mapa.lado * mapa.lado * 4);
	if (minimapa && tz_terreno_minimapa(&ter, &mapa, minimapa)) { free(minimapa); minimapa = NULL; }
	cargado = 1;
	actual = i;
	if (jogar) iniciar_partida();
	const char *nome = e->dentro[0] ? strrchr(e->dentro, '/') : strrchr(e->fich, '/');
	nome = nome ? nome + 1 : (e->dentro[0] ? e->dentro : e->fich);
	snprintf(titulo, sizeof titulo, "%s (%dx%d, %s, %d objetos)", nome, mapa.lado, mapa.lado, mapa.bioma, mapa.nobj);
	registro("[TZAR] mapa %s%s%s: lado %d bioma %s objetos %d visuales %d imagenes %d sin dibujo %d fallos %d (%u ms)",
	         e->fich, e->dentro[0] ? "|" : "", e->dentro, mapa.lado, mapa.bioma, mapa.nobj, esc.nv, esc.nimgs, esc.sem_imagem, esc.falhas,
	         (unsigned)(SDL_GetTicks() - t0));
	if (esc.falhas) registro("[TZAR] ultimo fallo: %s", esc.ultima_falha);
	return 0;
}

static void limitar_camara(void)
{
	int mw = mapa.lado * 32, mh = mapa.lado * 32;
	if (camx > mw - fw) camx = mw - fw;
	if (camy > mh - fh) camy = mh - fh;
	if (camx < 0) camx = 0;
	if (camy < 0) camy = 0;
}

/* Rectángulo del minimapa (abajo a la derecha): hasta 192 píxeles, una casilla por píxel o menos. */
static void minimapa_rect(int *x, int *y, int *lado)
{
	int l = mapa.lado < 192 ? mapa.lado : 192;
	if (l > fw / 4) l = fw / 4;
	*lado = l;
	*x = fw - l - 10;
	*y = fh - l - 10;
}

static void dibujar(void)
{
	retangulo(0, 0, fw, fh, 0);
	if (!cargado) return;
	/* terreno */
	int cx0 = camx / 32, cy0 = camy / 32, cx1 = (camx + fw + 31) / 32, cy1 = (camy + fh + 31) / 32;
	if (cx1 > mapa.lado) cx1 = mapa.lado;
	if (cy1 > mapa.lado) cy1 = mapa.lado;
	for (int cy = cy0; cy < cy1; ++cy)
		for (int cx = cx0; cx < cx1; ++cx) {
			int paso;
			const uint32_t *px = tz_terreno_loseta_t(&ter, mapa.casillas[cy * mapa.lado + cx], cx, cy, (uint32_t)(reloj / 250), &paso);
			int sx = cx * 32 - camx, sy = cy * 32 - camy;
			if (!px) { retangulo(sx, sy, 32, 32, 0xff00ff); continue; }
			for (int j = 0; j < 32; ++j) {
				int y = sy + j;
				if (y < 0 || y >= fh) continue;
				int i0 = sx < 0 ? -sx : 0, i1 = sx + 32 > fw ? fw - sx : 32;
				for (int i = i0; i < i1; ++i) fb[y * fpitch + sx + i] = px[j * paso + i] & 0xffffff;
			}
		}
	/* objetos, árboles y acantilados en el orden del pintor */
	tz_animar(&esc, reloj);
	if (part.ativa) tz_partida_dibujar(&part, &esc, &lz, camx, camy, (uint64_t)part.m.tic * TZ_TIC_MS);
	else for (int i = 0; i < esc.nv; ++i) {
		const TzVisual *v = &esc.v[i];
		const TzImg *im = &esc.imgs[v->img];
		int x = v->x - camx, y = v->y - camy;
		if (x >= fw || y >= fh || x + im->w <= 0 || y + im->h <= 0) continue;
		pintar(im, x, y);
	}
	/* selección: marco, barra de vida y ficha del objeto; con H, las barras de todos los que se ven */
	int mostrar_barra[1] = { 0 };
	(void)mostrar_barra;
	for (int i = 0; i < esc.nv && !part.ativa; ++i) {
		const TzVisual *v = &esc.v[i];
		if (v->obj < 0 || v->capa != TZ_CAPA_CORPO || v->sel[2] <= 0) continue;
		const TzObjeto *o = &mapa.obj[v->obj];
		int x = v->sel[0] - camx, y = v->sel[1] - camy;
		if (x >= fw || y >= fh || x + v->sel[2] <= 0 || y + v->sel[3] <= 0) continue;
		int sel = v->obj == sel_obj;
		if (sel) tz_marco_selecao(&lz, x, y, v->sel[2], v->sel[3], 0xff000000u | (uint32_t)ter.cor_selecao[0] << 16 | (uint32_t)ter.cor_selecao[1] << 8 | ter.cor_selecao[2]);
		if ((sel || barras) && o->vida_max > 0 && o->vida > 0 && tz_classe_e_unidade(o->classe) + tz_classe_e_edificio(o->classe) > 0) {
			int casillas = (v->sel[2] + 16) / 32;
			tz_barra_vida(&lz, o->x * 32 - camx, o->y * 32 - camy, tz_classe_e_edificio(o->classe) ? casillas : 1, o->vida, o->vida_max, 0, 0);
		}
	}
	if (sel_obj >= 0 && tem_textos && !part.ativa) {
		char ficha[300];
		tz_ficha_objeto(&textos, &mapa.obj[sel_obj], ficha, sizeof ficha);
		int tw = tem_fonte ? tz_fonte_medir(&fonte, ficha) : (int)strlen(ficha) * 8;
		tz_ret_escuro(&lz, 6, fh - 30, tw + 12, 24);
		if (tem_fonte) tz_fonte_texto(&fonte, &lz, 12, fh - 28 + (20 - fonte.alt) / 2, ficha, 0xf0e8c0);
		else texto(12, fh - 28, ficha, 0xf0e8c0);
	}
	/* minimapa con el rectángulo de la vista */
	int mx, my, ml;
	minimapa_rect(&mx, &my, &ml);
	retangulo(mx - 2, my - 2, ml + 4, ml + 4, 0x302010);
	if (minimapa)
		for (int j = 0; j < ml; ++j)
			for (int i = 0; i < ml; ++i) fb[(my + j) * fpitch + mx + i] = minimapa[(j * mapa.lado / ml) * mapa.lado + i * mapa.lado / ml] & 0xffffff;
	int rx = mx + camx * ml / (mapa.lado * 32), ry = my + camy * ml / (mapa.lado * 32);
	int rw = fw * ml / (mapa.lado * 32), rh = fh * ml / (mapa.lado * 32);
	if (rw > ml) rw = ml;
	if (rh > ml) rh = ml;
	retangulo(rx, ry, rw, 1, 0xffffff); retangulo(rx, ry + rh - 1, rw, 1, 0xffffff);
	retangulo(rx, ry, 1, rh, 0xffffff); retangulo(rx + rw - 1, ry, 1, rh, 0xffffff);
	/* barra de arriba */
	escurecer(0, 0, fw, 22);
	char resumo[200];
	if (part.ativa) tz_partida_resumo(&part, resumo, sizeof resumo);
	texto(6, 3, part.ativa ? resumo : titulo, 0xf0d890);
	char ayuda[120];
	snprintf(ayuda, sizeof ayuda, "Mapa %d/%d  N/P: cambiar  Flechas: mover  Esc: salir", actual + 1, nlista);
	if ((int)strlen(ayuda) * 8 + 12 < fw - (int)strlen(titulo) * 8) texto(fw - (int)strlen(ayuda) * 8 - 6, 3, ayuda, 0xc0c0c0);
}

static void sin_datos(const char *motivo)
{
	retangulo(0, 0, fw, fh, 0x140d09);
	const char *l[] = {
		"Tzar: no se encuentran los datos del juego",
		"",
		"Este visor necesita una copia de Tzar (cualquier edición; probado con la de FX",
		"en castellano). Los datos no vienen con SaguiOS.",
		"",
		"Copie la carpeta de la instalación (la que tiene Packs/ y Maps/) en:",
		"    /jogos/TZAR",
		"(en una imagen privada: input_share/games/TZAR antes de compilar),",
		"o indique la carpeta con TZAR_DATA=/ruta o como primer argumento: tzar /ruta",
		"",
		"Motivo:",
	};
	int y = 40;
	for (unsigned i = 0; i < sizeof l / sizeof *l; ++i, y += 20) texto(40, y, l[i], i ? 0xe0d8c0 : 0xf0c060);
	texto(56, y, motivo, 0xff9070);
	texto(40, fh - 40, "Pulse Esc o cierre la ventana para salir.", 0xa0a0a0);
}

static void presentar(void)
{
	SDL_Surface *ws = SDL_GetWindowSurface(win);
	if (ws) { SDL_BlitSurface(fbs, NULL, ws, NULL); SDL_UpdateWindowSurface(win); }
}

static int existe_pasta(const char *p)
{
	struct stat st;
	return p && *p && !stat(p, &st) && S_ISDIR(st.st_mode);
}

/* «tzar --visor [carpeta [mapa]]» (jogo.c llama aquí) */
int tz_visor_main(int argc, char **argv)
{
	int ww = getenv("TZAR_W") ? atoi(getenv("TZAR_W")) : 1024, wh = getenv("TZAR_H") ? atoi(getenv("TZAR_H")) : 720;
	if (ww < 320) ww = 320;
	if (wh < 240) wh = 240;
	if (SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
	win = SDL_CreateWindow("Tzar (visor)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh, 0);
	fbs = SDL_CreateRGBSurfaceWithFormat(0, ww, wh, 32, SDL_PIXELFORMAT_RGB888);
	if (!win || !fbs) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
	fb = fbs->pixels; fw = ww; fh = wh; fpitch = fbs->pitch / 4;
	lz.px = fb; lz.w = fw; lz.h = fh; lz.passo = fpitch;

	/* una carpeta pedida (argumento o TZAR_DATA) se usa o se explica por qué no vale; si no, se buscan las de siempre */
	const char *pedida = argc > 1 ? argv[1] : getenv("TZAR_DATA");
	const char *cands[] = { "/jogos/TZAR", "/jogos/Tzar", "/jogos/tzar" };
	const char *raiz = NULL;
	if (pedida && *pedida) raiz = existe_pasta(pedida) ? pedida : NULL;
	else for (unsigned i = 0; i < sizeof cands / sizeof *cands && !raiz; ++i) if (existe_pasta(cands[i])) raiz = cands[i];
	registro("[TZAR] inicio: ventana %dx%d, datos %s", ww, wh, raiz ? raiz : "no encontrados");
	const char *shot = getenv("TZAR_SHOT");
	int frames = getenv("TZAR_FRAMES") ? atoi(getenv("TZAR_FRAMES")) : (shot ? 3 : 0);
	char motivo[300] = "";
	if (!raiz && pedida && *pedida) snprintf(motivo, sizeof motivo, "no existe la carpeta %s", pedida);
	else if (!raiz) snprintf(motivo, sizeof motivo, "no existe /jogos/TZAR");
	else if (tz_recursos_abrir(&rec, raiz)) snprintf(motivo, sizeof motivo, "%s", tz_erro());
	else {
		listar_mapas(raiz);
		if (!tz_textos_carregar(&textos, &rec)) tem_textos = 1;
		else registro("[TZAR] sin textos: %s", tz_erro());
		static const char *const fontes[] = { "LOCAL/FONTS/ARIAL12B_AA.FNT", "LOCAL/FONTS/ARIAL10_AA.FNT", "LOCAL/FONTS/SMALL.FNT" };
		for (int i = 0; i < 3 && !tem_fonte; ++i) {
			uint32_t fn;
			uint8_t *fd = tz_recursos_ler(&rec, fontes[i], &fn);
			if (fd) { tem_fonte = !tz_fonte_ler(&fonte, fd, fn); free(fd); }
		}
		registro("[TZAR] instalacion: %d paquetes, %d recursos, %d mapas", rec.npac, rec.nnomes, nlista);
		if (!nlista) snprintf(motivo, sizeof motivo, "la instalación no tiene mapas (Maps/*.wmp ni Campaign/*.wcm)");
	}
	if (*motivo) {
		registro("[TZAR] sin datos del juego: %s", motivo);
		for (int n = 0; n < (frames ? frames : 1 << 30); ++n) {
			SDL_Event e;
			while (SDL_PollEvent(&e))
				if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) n = 1 << 30;
			sin_datos(motivo);
			presentar();
			if (shot && n + 1 == frames) SDL_SaveBMP(fbs, shot);
			SDL_Delay(30);
		}
		if (shot) registro("[TZAR] captura guardada");
		SDL_Quit();
		return 1;
	}
	/* el mapa pedido, o el primero */
	const char *pedido = argc > 2 ? argv[2] : getenv("TZAR_MAPA");
	int ini = 0;
	if (pedido) {
		char f[600] = "", dentro[300] = "";
		const char *bar = strchr(pedido, '|');
		if (bar) { snprintf(f, sizeof f, "%.*s", (int)(bar - pedido), pedido); snprintf(dentro, sizeof dentro, "%s", bar + 1); }
		else snprintf(f, sizeof f, "%s", pedido);
		ini = -1;
		for (int i = 0; i < nlista && ini < 0; ++i)
			if (!strcasecmp(lista[i].fich, f) && !strcasecmp(lista[i].dentro, dentro)) ini = i;
		/* un mapa suelto fuera de Maps/ (pruebas): se añade si existe */
		if (ini < 0 && !dentro[0]) {
			char abs_[1200];
			struct stat st;
			snprintf(abs_, sizeof abs_, "%s/%s", rec.raiz, f);
			if (!stat(abs_, &st)) { anadir(f, ""); ini = nlista - 1; }
		}
		if (ini < 0) { registro("[TZAR] no se encuentra el mapa %s: se abre el primero", pedido); ini = 0; }
	}
	if (cargar(ini)) registro("[TZAR] error al abrir el mapa: %s", tz_erro());
	if (getenv("TZAR_POS")) {
		int x = 0, y = 0;
		sscanf(getenv("TZAR_POS"), "%d,%d", &x, &y);
		camx = x * 32; camy = y * 32;
	}
	if (cargado) limitar_camara();

	Uint32 ultimo = SDL_GetTicks();
	const Uint32 inicio = ultimo;
	int arrastrando = 0, cuadros = 0, sel_x0 = -1, sel_y0 = -1;
	if (getenv("TZAR_JOGAR") && cargado && !part.ativa) { jogar = 1; iniciar_partida(); }
	Uint32 dibujo_ms = 0;
	for (int n = 0; !frames || n < frames; ++n) {
		SDL_Event e;
		int salir = 0;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) salir = 1;
			else if (e.type == SDL_KEYDOWN) {
				SDL_Keycode k = e.key.keysym.sym;
				if (k == SDLK_ESCAPE) salir = 1;
				else if ((k == SDLK_n || k == SDLK_PAGEDOWN) && nlista) { if (cargar((actual + 1) % nlista)) registro("[TZAR] error: %s", tz_erro()); camx = camy = 0; sel_obj = -1; }
				else if ((k == SDLK_p || k == SDLK_PAGEUP) && nlista) { if (cargar((actual + nlista - 1) % nlista)) registro("[TZAR] error: %s", tz_erro()); camx = camy = 0; sel_obj = -1; }
				else if (k == SDLK_HOME) camx = camy = 0;
				else if (k == SDLK_h) barras = !barras;
				else if (k == SDLK_j && cargado) { jogar = !jogar; if (jogar) iniciar_partida(); else tz_partida_liberar(&part); }
				else if (k == SDLK_F5 && part.ativa) registro("[TZAR] guardar: %s", tz_mundo_gravar(&part.m, "tzar.sav") ? tz_erro() : "tzar.sav");
				else if (k == SDLK_F9 && part.ativa) registro("[TZAR] cargar: %s", tz_mundo_carregar(&part.m, &part.cat, part.mapa, "tzar.sav") ? tz_erro() : "tzar.sav");
				else if (part.ativa && k == SDLK_DELETE) tz_partida_tecla(&part, 127);
				else if (part.ativa && k < 128) tz_partida_tecla(&part, k == SDLK_KP_PLUS ? '+' : (int)k);
			} else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
				int mx, my, ml;
				minimapa_rect(&mx, &my, &ml);
				if (cargado && part.ativa && !(e.button.x >= mx && e.button.y >= my && e.button.x < mx + ml && e.button.y < my + ml)) {
					sel_x0 = e.button.x + camx; sel_y0 = e.button.y + camy;
				} else if (cargado && !(e.button.x >= mx && e.button.y >= my && e.button.x < mx + ml && e.button.y < my + ml)) {
					sel_obj = tz_selecao_apontar(&esc, &mapa, e.button.x + camx, e.button.y + camy);
					if (sel_obj >= 0) registro("[TZAR] seleccion: objeto %d (%s)", sel_obj, tz_classes[mapa.obj[sel_obj].classe]);
				}
				arrastrando = 1;
			}
			else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT && part.ativa)
				tz_partida_clique(&part, e.button.x + camx, e.button.y + camy, 3, (SDL_GetModState() & KMOD_SHIFT) != 0);
			else if (e.type == SDL_MOUSEMOTION && part.ativa) tz_partida_rato(&part, e.motion.x + camx, e.motion.y + camy);
			else if (e.type == SDL_MOUSEBUTTONUP) {
				arrastrando = 0;
				if (part.ativa && e.button.button == SDL_BUTTON_LEFT && sel_x0 >= 0) {
					int x1 = e.button.x + camx, y1 = e.button.y + camy, mayus = (SDL_GetModState() & KMOD_SHIFT) != 0;
					if (abs(x1 - sel_x0) > 4 || abs(y1 - sel_y0) > 4) tz_partida_arrastar(&part, sel_x0, sel_y0, x1, y1, mayus);
					else tz_partida_clique(&part, x1, y1, 1, mayus);
					sel_x0 = -1;
				}
			}
		}
		if (salir) break;
		Uint32 ahora = SDL_GetTicks();
		int dt = (int)(ahora - ultimo);
		ultimo = ahora;
		if (dt > 100) dt = 100;
		if (cargado) {
			const Uint8 *teclas = SDL_GetKeyboardState(NULL);
			int vel = dt * (teclas[SDL_SCANCODE_LSHIFT] || teclas[SDL_SCANCODE_RSHIFT] ? 3 : 1);
			int mxs, mys;
			SDL_GetMouseState(&mxs, &mys);
			int dx = 0, dy = 0;
			if (teclas[SDL_SCANCODE_LEFT] || teclas[SDL_SCANCODE_A]) dx -= vel;
			if (teclas[SDL_SCANCODE_RIGHT] || teclas[SDL_SCANCODE_D]) dx += vel;
			if (teclas[SDL_SCANCODE_UP] || teclas[SDL_SCANCODE_W]) dy -= vel;
			if (teclas[SDL_SCANCODE_DOWN] || teclas[SDL_SCANCODE_S]) dy += vel;
			if (!frames && SDL_GetMouseFocus() == win) {
				if (mxs < 4) dx -= vel;
				if (mxs >= fw - 4) dx += vel;
				if (mys < 4) dy -= vel;
				if (mys >= fh - 4) dy += vel;
			}
			int mx, my, ml;
			minimapa_rect(&mx, &my, &ml);
			if (arrastrando && mxs >= mx && mys >= my && mxs < mx + ml && mys < my + ml) {
				camx = (mxs - mx) * mapa.lado * 32 / ml - fw / 2;
				camy = (mys - my) * mapa.lado * 32 / ml - fh / 2;
			}
			camx += dx; camy += dy;
			limitar_camara();
		}
		reloj = getenv("TZAR_MS") ? (uint64_t)atoll(getenv("TZAR_MS")) : (uint64_t)(ahora - inicio);
		if (part.ativa) {
			int ant = part.m.acabou;
			tz_partida_avancar(&part, frames ? TZ_TIC_MS : dt);
			if (part.m.acabou && !ant) registro("[TZAR] fin de la partida: %s", part.m.ganhou ? "victoria" : "derrota");
		}
		if (n == 0 && cargado && getenv("TZAR_CLICK")) {
			int cx = 0, cy = 0;
			sscanf(getenv("TZAR_CLICK"), "%d,%d", &cx, &cy);
			sel_obj = tz_selecao_apontar(&esc, &mapa, cx + camx, cy + camy);
			registro("[TZAR] seleccion: objeto %d%s%s", sel_obj, sel_obj >= 0 ? " " : "", sel_obj >= 0 ? tz_classes[mapa.obj[sel_obj].classe] : "");
			if (getenv("TZAR_BARRAS")) barras = 1;
		}
		Uint32 t0 = SDL_GetTicks();
		dibujar();
		presentar();
		dibujo_ms += SDL_GetTicks() - t0;
		cuadros++;
		if (shot && n + 1 == frames) {
			SDL_SaveBMP(fbs, shot);
			registro("[TZAR] captura guardada en el cuadro %d (camara %d,%d)", n + 1, camx / 32, camy / 32);
		}
		SDL_Delay(15);   /* también con TZAR_FRAMES: la ventana dura lo bastante para la captura de la prueba */
	}
	if (cuadros) registro("[TZAR] %d cuadros, %u ms de dibujo por cuadro de media", cuadros, (unsigned)(dibujo_ms / (Uint32)cuadros));
	if (part.ativa) registro("[TZAR] partida: tic %u suma %08x objetos %d fallos de dibujo %d", part.m.tic, tz_mundo_suma(&part.m), part.m.nobj, part.falhas_dibujo);
	descargar();
	if (tem_fonte) tz_fonte_liberar(&fonte);
	if (tem_textos) tz_ini_liberar(&textos.ini);
	tz_recursos_fechar(&rec);
	free(lista);
	SDL_Quit();
	registro("[TZAR] fin");
	return 0;
}


