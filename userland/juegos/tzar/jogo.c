/* SPDX-License-Identifier: GPL-3.0-or-later */
/* jogo.c - «tzar», el juego completo (D417): introducción, menú principal de la interfaz original, escaramuza en un
 * mapa o en uno aleatorio contra la IA, campañas (lista, misión, historia, partida, resultado y siguiente misión con
 * el progreso guardado), cargar y guardar, opciones de volumen y salir. «tzar --visor ...» es el visor de mapas de las
 * fases 2-5 (visor.c). Código propio (no sale de ReTzar): une los módulos de mundo.h, ui.h, guiao.h, campanha.h,
 * aleatorio.h, som.h, intro.h e ia/ia.h, cada uno traducido de su parte de ReTzar (GPL-3.0-or-later).
 *
 * Datos: el primer argumento, TZAR_DATA o /jogos/TZAR (en las imágenes privadas, input_share/«Tzar Anthology»).
 * Partidas guardadas y progreso: TZAR_GUARDAR, o $HOME/.tzar, o /jogos/TZAR/guardadas.
 * Pruebas: TZAR_W/TZAR_H, TZAR_FRAMES=n (sale tras n cuadros), TZAR_SHOT=f.bmp (captura del último cuadro),
 * TZAR_AUTO=escaramuza|campanha (entra sola en una partida: la escaramuza en el primer mapa; la campaña, la primera),
 * TZAR_SEM_INTRO=1, TZAR_SEM_SOM=1, TZAR_DEMO=1 (el jugador de prueba lleva el reino de la persona en la escaramuza).
 * Registro: «[TZAR] ...».
 */
#include "tzar.h"
#include "mundo.h"
#include "ui.h"
#include "partida.h"
#include "guiao.h"
#include "campanha.h"
#include "aleatorio.h"
#include "som.h"
#include "musica.h"
#include "intro.h"
#include "ia/ia.h"
#include "opcoes.h"
#include "vigia.h"
#include <SDL.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#ifdef SAGUI_TZAR
#include "sagui.h"
#include "energia.h"
#endif

int tz_visor_main(int argc, char **argv);

static void registro(const char *fmt, ...)
{
	char b[500];
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

/* ------------------------------------------------------------------ pantalla
 * El juego dibuja en un lienzo (tela, W x H, al menos 800x600) y presentar() lo escala a la superficie de la ventana (o de la
 * pantalla completa) según las opciones (opcoes.c): entero o ajustado con bandas negras, nítido o suave. El ratón se pasa de la
 * ventana al lienzo con el mismo modo. */
static SDL_Window *win;
static SDL_Surface *fbs;         /* intermedia, solo si la superficie de la ventana no es de 32 bits RGB */
static TzTela tela;
static int W, H;
static TzOpcoes op;
static TzModo modo;
static int janela_w = 800, janela_h = 600;   /* la ventana fuera de la pantalla completa */
static int tela_cheia_atual, sem_molestar, tamanho_fixo;
static char ruta_opcoes[700];

static void registro(const char *fmt, ...);
static void redimensionar_lienzo(int w, int h);
static int rato_x, rato_y, rato_dentro;   /* el ratón en el lienzo y si está en la ventana */

static void ajustar_modo(int ow, int oh)
{
	TzModo nv;
	if (tamanho_fixo) tz_modo_destino(&op, W, H, ow, oh, &nv);   /* TZAR_W/TZAR_H: el lienzo no cambia */
	else tz_modo_calcular(&op, ow, oh, &nv);
	if (nv.cw != W || nv.ch != H) redimensionar_lienzo(nv.cw, nv.ch);
	modo = nv;
	registro("[TZAR] pantalla: %s %dx%d, lienzo %dx%d, dibujo %dx%d en (%d,%d)%s%s", tela_cheia_atual ? "completa" : "ventana", ow, oh, W, H,
	         modo.dw, modo.dh, modo.dx, modo.dy, modo.fator ? " (entero)" : " (ajustado)", op.suave && !modo.fator ? ", suave" : "");
}

static void presentar(void)
{
	SDL_Surface *ws = SDL_GetWindowSurface(win);
	if (!ws) return;
	if (ws->w != modo.ow || ws->h != modo.oh) ajustar_modo(ws->w, ws->h);
	int directo = ws->format->BytesPerPixel == 4 && ws->format->Rmask == 0xff0000 && ws->format->Gmask == 0xff00 && !SDL_MUSTLOCK(ws);
	if (directo) tz_apresentar_escala(tela.px, &modo, ws->pixels, ws->pitch / 4, op.suave);
	else {
		if (!fbs || fbs->w != ws->w || fbs->h != ws->h) {
			if (fbs) SDL_FreeSurface(fbs);
			fbs = SDL_CreateRGBSurfaceWithFormat(0, ws->w, ws->h, 32, SDL_PIXELFORMAT_RGB888);
			if (!fbs) return;
		}
		tz_apresentar_escala(tela.px, &modo, fbs->pixels, fbs->pitch / 4, op.suave);
		SDL_BlitSurface(fbs, NULL, ws, NULL);
	}
	SDL_UpdateWindowSurface(win);
}

/* captura del lienzo (pruebas: TZAR_SHOT) */
static void captura(const char *ruta)
{
	SDL_Surface *c = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_RGB888);
	if (!c) return;
	for (int y = 0; y < H; ++y) {
		uint32_t *d = (uint32_t *)((uint8_t *)c->pixels + (size_t)y * c->pitch);
		for (int x = 0; x < W; ++x) d[x] = tela.px[(size_t)y * W + x] & 0xffffff;
	}
	SDL_SaveBMP(c, ruta);
	SDL_FreeSurface(c);
}

static void guardar_opcoes(void)
{
	if (ruta_opcoes[0] && tz_opcoes_gravar(&op, ruta_opcoes)) registro("[TZAR] no se pueden guardar las opciones en %s", ruta_opcoes);
}

/* ventana o pantalla completa (F11, Alt+Intro, el menú de opciones o el F11 del sistema) */
static void aplicar_tela_cheia(int cheia)
{
	op.tela_cheia = cheia != 0;
	int ja = (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN) != 0;
	if (ja != op.tela_cheia && SDL_SetWindowFullscreen(win, op.tela_cheia ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0))
		registro("[TZAR] pantalla completa: %s", SDL_GetError());
	tela_cheia_atual = (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN) != 0;
#ifdef SAGUI_TZAR
	/* la SDL del sistema escala sola el lienzo de la ventana: a pantalla completa se le pide uno del tamaño de la pantalla y el
	 * escalado lo hace el juego (entero o ajustado, con su filtro) */
	if (tela_cheia_atual) { SDL_DisplayMode dm; if (!SDL_GetDesktopDisplayMode(0, &dm) && dm.w > 0 && dm.h > 0) SDL_SetWindowSize(win, dm.w, dm.h); }
	else SDL_SetWindowSize(win, janela_w, janela_h);
	/* D422: a pantalla completa, que no salte el salvapantallas */
	if (tela_cheia_atual != sem_molestar) { no_molestar(tela_cheia_atual); sem_molestar = tela_cheia_atual; }
#else
	sem_molestar = tela_cheia_atual;
#endif
	SDL_Surface *ws = SDL_GetWindowSurface(win);
	if (ws) ajustar_modo(ws->w, ws->h);
}

/* de la ventana al lienzo */
static void ponto_lienzo(int *x, int *y)
{
	int cx, cy;
	tz_modo_ponto(&modo, *x, *y, &cx, &cy);
	*x = cx; *y = cy;
}

/* ------------------------------------------------------------------ estado general */
static char raiz[512], pasta_guardar[600];
static TzRecursos rec;
static TzUiSkin skin;
static TzMapas mapas;
static TzMenu menu;
static TzSom *som;
static TzBiblioteca bib_som;
static int tem_bib_som;
static TzProgresso progresso;
static char ruta_progresso[700];
static TzIaBiblioteca ia_bib;
static int ia_bib_ok = -1;

enum { E_MENU, E_JOGO, E_SAIR };
static int estado = E_MENU;

/* ------------------------------------------------------------------ la partida */
typedef struct {
	int ativa;
	int tipo;                    /* 0 escaramuza en un mapa, 1 aleatorio, 2 campaña */
	int camp, missao;            /* campaña y misión (tipo 2) */
	char fich[300], dentro[200]; /* el mapa (para guardar) */
	TzMissaoAberta ab;           /* mapa y guion (tipos 0 y 2) */
	TzMapa mapa_aleat;           /* tipo 1 */
	TzMapa *mapa;
	TzTerreno ter;
	TzBiblioteca bib;
	TzEscena esc;
	TzCatalogo cat;
	TzMundo m;
	TzUi ui;
	uint32_t *minimapa;
	TzIa *ias[TZ_REINOS]; int nias;
	int com_guiao;
	int acum, velocidade;
	uint64_t ms;                 /* reloj del juego (animaciones) */
	int ganhou, acabou, fim_visto;
	uint32_t fps_quadros, fps_ms;
	char fala[400]; uint32_t fala_ate;
} Jogo;
static Jogo J;

static void ia_modo(TzMundo *m, void *d, int k, int modo)
{
	(void)m; (void)d;
	for (int i = 0; i < J.nias; ++i) if (J.ias[i]) {
		static const char *const ord[4] = { "none", "eco", "war", "full" };
		if (k >= 1 && k <= 8 && J.m.reino[k].ia) tz_ia_ordem(J.ias[i], ord[modo & 3]);
	}
}

static void ia_sem_ataque(TzMundo *m, void *d, uint32_t mascara, uint32_t tempo)
{
	(void)m; (void)d; (void)mascara;
	for (int i = 0; i < J.nias; ++i) if (J.ias[i]) tz_ia_sem_ataque(J.ias[i], tempo);
}

static void jogo_fechar(void)
{
	if (!J.ativa) return;
	for (int i = 0; i < J.nias; ++i) if (J.ias[i]) tz_ia_liberar(J.ias[i]);
	tz_ui_liberar(&J.ui);
	tz_mundo_liberar(&J.m);
	tz_catalogo_liberar(&J.cat);
	tz_escena_liberar(&J.esc);
	tz_biblioteca_liberar(&J.bib);
	tz_terreno_liberar(&J.ter);
	free(J.minimapa);
	if (J.tipo == 1) tz_mapa_liberar(&J.mapa_aleat);
	else tz_missao_fechar(&J.ab);
	if (som) { tz_som_parar_todos(som); tz_musica_menu(tz_som_musica(som)); }
	memset(&J, 0, sizeof J);
}

/* Carga lo que depende del bioma: terreno, biblioteca, escena, catálogo y mundo. regras puede ser NULL. */
static void previa_soltar(void);

static int jogo_montar(const TzRegras *regras, uint32_t semente)
{
	TzMapa *mp = J.mapa;
	previa_soltar();   /* el terreno de la previsualización no hace falta en la partida */
	if (tz_terreno_carregar(&J.ter, &rec, mp->bioma)) return -1;
	if (tz_biblioteca_carregar(&J.bib, &rec, mp->bioma)) return -1;
	if (tz_escena_montar(&J.esc, &J.bib, &rec, &J.ter, mp)) return -1;
	if (tz_catalogo_carregar(&J.cat, &rec, &J.bib)) return -1;
	if (tz_mundo_criar(&J.m, &J.cat, mp, regras, semente)) return -1;
	J.minimapa = malloc((size_t)mp->lado * mp->lado * 4);
	if (J.minimapa && tz_terreno_minimapa(&J.ter, mp, J.minimapa)) { free(J.minimapa); J.minimapa = NULL; }
	return 0;
}

static void jogo_ia(void)
{
	if (getenv("TZAR_SEM_IA")) return;
	if (ia_bib_ok < 0) {
		ia_bib_ok = !tz_ia_bib_carregar(&ia_bib, &rec);
		if (!ia_bib_ok) registro("[TZAR] sin guiones de IA: %s", tz_erro());
	}
	if (!ia_bib_ok) return;
	TzIaErro e;
	memset(&e, 0, sizeof e);
	J.nias = tz_ia_criar_todas(&J.m, &ia_bib, J.ias, TZ_REINOS, &e);
	if (e.mensagem) { char t[600]; tz_ia_erro_texto(&e, t, sizeof t); registro("[TZAR] IA: %s", t); }
	registro("[TZAR] IA original en %d reinos", J.nias);
}

/* ------------------------------------------------------------------ clima: lluvia y nieve (simulation.rs update_weather)
 * Ocho pares de 480-720 s despejado y 60-120 s de lluvia (o nieve) sacados de la semiente de la partida; el primer despejado dura
 * la cuarta parte; la intensidad sube y baja en 3,64 s. Llueve en los biomas cuyo TERRAIN.INI dice rain=1 y nieva en el de nieve.
 * Es solo cosa de ver y oír: no cambia la simulación. */
static uint32_t clima_par[8][2];
static int clima_chuva, clima_neve;

static void clima_iniciar(void)
{
	uint32_t sem = J.m.semente;
	for (int i = 0; i < 8; ++i) {
		sem = sem * 1664525u + 1013904223u; clima_par[i][0] = 480000u + sem % 240001u;
		sem = sem * 1664525u + 1013904223u; clima_par[i][1] = 60000u + sem % 60001u;
	}
	clima_chuva = clima_neve = 0;
	if (getenv("TZAR_SEM_CLIMA")) return;
	char b[120];
	snprintf(b, sizeof b, "IMAGES/TERRAIN/%s.TERRAIN.INI", J.mapa->bioma);
	TzIni ini;
	memset(&ini, 0, sizeof ini);
	if (!tz_ini_recurso(&rec, &ini, b)) {
		const char *v = tz_ini(&ini, "terrain", "rain");
		clima_chuva = v && atoi(v) == 1;
		tz_ini_liberar(&ini);
	}
	clima_neve = !strcasecmp(J.mapa->bioma, "snow");
	if (clima_neve) clima_chuva = 0;
}

/* 0..256: cuánto llueve (o nieva) en el instante ms del juego */
static int clima_intensidade(uint64_t ms)
{
	if (!clima_chuva && !clima_neve) return 0;
	if (getenv("TZAR_CHUVA")) return 256;   /* pruebas: lluvia ya */
	uint64_t primeiro = clima_par[0][0] / 4;
	if (ms < primeiro) return 0;
	ms -= primeiro;
	uint64_t periodo = 0;
	for (int i = 0; i < 8; ++i) periodo += clima_par[i][0] + clima_par[i][1];
	ms %= periodo;
	for (int i = 0; i < 8; ++i) {
		uint64_t dur = clima_par[i][1];
		if (ms < dur) {
			uint64_t a = ms < dur - ms ? ms : dur - ms;
			return a >= 3640 ? 256 : (int)(a * 256 / 3640);
		}
		ms -= dur;
		uint64_t limpo = clima_par[(i + 1) % 8][0];
		if (ms < limpo) return 0;
		ms -= limpo;
	}
	return 0;
}

/* gotas (o copos) sobre la vista del mapa: posiciones que dependen del índice y del reloj, nada guardado */
static void clima_desenhar(int intens)
{
	if (intens <= 0) return;
	TzRect r = J.ui.mapa_rect;
	int n = (int)((long)r.w * r.h / 2600) * intens / 256;
	uint32_t t = (uint32_t)(J.ms / 20);
	for (int i = 0; i < n; ++i) {
		uint32_t h = (uint32_t)i * 2654435761u;
		int vel = clima_neve ? 1 : 9;
		int x = r.x + (int)((h >> 8) % (uint32_t)r.w), y0 = (int)((h >> 3) % (uint32_t)r.h);
		int y = r.y + (int)((y0 + t * (uint32_t)vel) % (uint32_t)r.h);
		if (clima_neve) {
			x += (int)((t / 8 + h) % 5) - 2;
			if (x >= r.x && x + 1 < r.x + r.w && y + 1 < r.y + r.h) tz_tela_preencher(&tela, (TzRect){ x, y, 2, 2 }, 0xd0f0f0ff);
		} else
			for (int k = 0; k < 7; ++k) {
				int px = x - k / 3, py = y - k;
				if (px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h) {
					uint32_t *q = &tela.px[(size_t)py * tela.w + px];
					*q = ((*q >> 1) & 0x7f7f7f) + 0x506070;
				}
			}
	}
}

static void jogo_ui(void)
{
	tz_ui_iniciar(&J.ui, &skin, &J.m, &J.bib, W, H);
	tz_ui_lado(&J.ui, op.painel_direita);
	J.ui.minimapa = J.minimapa;
	/* el ratón aún no se movió: en el centro (si no, en (0,0) la vista correría sola hacia la esquina) */
	J.ui.mx = rato_dentro ? rato_x : W / 2; J.ui.my = rato_dentro ? rato_y : H / 2;
	if (som) tz_som_cursor(som, J.m.ev_seq);
	J.velocidade = 10;
	/* la cámara sobre algo del jugador: su castillo, o su primera unidad */
	int cx = -1, cy = -1;
	for (int i = 0; i < J.m.nobj; ++i) {
		const TzObj *o = &J.m.obj[i];
		if (o->dono != J.m.jogador || !tz_obj_vivo(o)) continue;
		if (o->classe == TZC_CASTLE) { cx = o->cx + 3; cy = o->cy + 3; break; }
		if (cx < 0 && (tz_obj_unidade(o) || tz_obj_edificio(o))) { cx = o->cx; cy = o->cy; }
	}
	if (cx >= 0) tz_ui_centrar(&J.ui, cx, cy);
	clima_iniciar();
	if (som) {
		if (J.tipo != 1) {
			TzHpfs *c = J.ab.tem_sub ? &J.ab.sub : &J.ab.c;
			tz_som_fonte_missao(som, c);
			/* los sonidos puestos en el mapa (<mapa>.sounds) */
			for (int i = 0; i < c->n; ++i) {
				size_t l = strlen(c->e[i].nome);
				if (l < 7 || strcasecmp(c->e[i].nome + l - 7, ".sounds")) continue;
				uint32_t n = 0;
				uint8_t *d = tz_hpfs_ler(c, c->e[i].nome, &n);
				if (d) { int k = tz_som_fontes_mapa(som, d, n, J.mapa); if (k) registro("[TZAR] sonidos del mapa: %d", k); free(d); }
				break;
			}
		} else tz_som_fonte_missao(som, NULL);
		tz_musica_jogo(tz_som_musica(som));
	}
	J.ativa = 1;
	estado = E_JOGO;
}

/* escaramuza: un mapa suelto (o de campaña) sin guion, castillo y tres peones por reino, la IA en los demás */
static TzAleatorio aleat_usado;          /* los parámetros del mapa aleatorio de la partida (se guardan con ella) */
static const TzAleatorio *aleat_forcado;   /* al cargar: los guardados, para regenerar el mismo mapa */

/* lo elegido en la pantalla del mapa aleatorio -> parámetros del generador (el terreno «al azar» sale de la semilla) */
static void aleat_de_menu(const TzMnAleat *a, TzAleatorio *op)
{
	tz_aleatorio_padrao(op);
	op->lado = tz_mn_lados[a->lado];
	op->jogadores = a->c.jogadores;
	op->humano = 0;
	op->tipo = a->tipo;
	op->recursos_mapa = a->recursos_mapa;
	op->nivel_recursos = a->c.recursos;
	op->guardado = a->guardado; op->animais = a->animais; op->itens = a->itens; op->portas = a->portas; op->penhascos = a->penhascos;
	op->semente = a->semente;
	snprintf(op->bioma, sizeof op->bioma, "%s", tz_mn_biomas[a->terreno ? a->terreno - 1 : (int)(a->semente % 4)]);
	for (int i = 0; i < 8; ++i) { op->equipe[i] = (uint8_t)a->c.equipe[i]; op->civ[i] = (uint8_t)a->c.raca[i]; }
	/* D470: el comportamiento de la IA de cada reino (la persona, el 0, no lleva) y la rendición */
	for (int i = 1; i < 8; ++i) snprintf(op->ia[i], sizeof op->ia[i], "%s", a->ia[i]);
	op->rendicao = a->rendicao;
}

/* catálogo del bioma: se queda guardado entre cambios de opciones de la vista previa y se suelta al empezar la partida */
static TzBiblioteca ma_bib; static TzCatalogo ma_cat; static char ma_bioma[16];
static void aleat_cache_soltar(void)
{
	if (!ma_bioma[0]) return;
	tz_catalogo_liberar(&ma_cat); tz_biblioteca_liberar(&ma_bib);
	ma_bioma[0] = 0;
}
static int aleat_gerar(TzMapa *out, const TzAleatorio *op)
{
	if (strcmp(ma_bioma, op->bioma)) {
		aleat_cache_soltar();
		if (tz_biblioteca_carregar(&ma_bib, &rec, op->bioma)) return -1;
		if (tz_catalogo_carregar(&ma_cat, &rec, &ma_bib)) { tz_biblioteca_liberar(&ma_bib); return -1; }
		snprintf(ma_bioma, sizeof ma_bioma, "%s", op->bioma);
	}
	return tz_mapa_aleatorio(out, &rec, &ma_cat, op);
}

static int jogar_mapa(const TzMissao *mi, const TzMnConfig *cfg, int aleatorio)
{
	jogo_fechar();
	memset(&J, 0, sizeof J);
	TzRegras rg;
	memset(&rg, 0, sizeof rg);
	rg.populacao = (uint16_t)(cfg && cfg->populacao ? cfg->populacao : 200);
	rg.dificuldade = (uint8_t)(cfg ? cfg->dificuldade : 1);
	rg.nivel_recursos = (uint8_t)(cfg ? cfg->recursos + 1 : 2);
	rg.vitoria = (uint8_t)(cfg ? cfg->vitoria : 0);
	rg.vitoria_limite = 30;
	rg.sem_nevoa = (uint8_t)(cfg ? cfg->sem_nevoa : 0);
	rg.explorado = (uint8_t)(cfg ? cfg->explorado : 0);
	int njog = cfg && cfg->jogadores >= 2 ? cfg->jogadores : 2;
	if (aleatorio) {
		J.tipo = 1;
		TzAleatorio op;
		tz_aleatorio_padrao(&op);
		op.jogadores = njog;
		op.semente = SDL_GetTicks() | 1;
		snprintf(op.bioma, sizeof op.bioma, "%s", tz_mn_biomas[op.semente % 4]);
		if (aleat_forcado) { op = *aleat_forcado; njog = op.jogadores; }
		aleat_usado = op;
		rg.ia_rendicao = (uint8_t)(op.rendicao != 0);   /* D470 */
		/* el catálogo del bioma da los tamaños de castillos y minas */
		if (aleat_gerar(&J.mapa_aleat, &op)) return -1;
		J.mapa = &J.mapa_aleat;
		snprintf(J.fich, sizeof J.fich, "(aleatorio)");
	} else {
		J.tipo = 0;
		if (tz_missao_abrir(&mapas, mi, &J.ab)) return -1;
		J.mapa = &J.ab.mapa;
		snprintf(J.fich, sizeof J.fich, "%s", mi->fich);
		snprintf(J.dentro, sizeof J.dentro, "%s", mi->dentro);
		/* los reinos de la escaramuza: el 1 la persona, hasta njog la máquina */
		if (!J.mapa->nreinos) J.mapa->nreinos = 8;
		int max = J.mapa->jogadores[1] > 0 ? J.mapa->jogadores[1] : 8;
		if (njog > max) njog = max;
		for (int k = 0; k < 8; ++k) {
			TzReino *r = &J.mapa->reinos[k];
			r->ativo = k < njog;
			r->ia = k > 0;
			if (cfg && cfg->raca[k]) r->civ = cfg->raca[k];
			r->equipe = cfg ? cfg->equipe[k] : 0;
		}
		J.mapa->tem_diplomacia = 0;
	}
	if (jogo_montar(&rg, SDL_GetTicks() | 1)) return -1;
	if (!aleatorio && tz_mundo_inicio_livre(&J.m)) registro("[TZAR] partida libre: %s", tz_erro());
	jogo_ia();
	if (getenv("TZAR_DEMO")) {
		/* pruebas: el jugador de prueba (demo.c) lleva el reino de la persona */
		TzGancho gd;
		memset(&gd, 0, sizeof gd);
		gd.tic = tz_demo_tic; gd.cada = 1;
		tz_demo_so_reino = J.m.jogador;
		tz_mundo_gancho(&J.m, &gd);
	}
	jogo_ui();
	registro("[TZAR] escaramuza: %s, %d reinos, jugador %d, lado %d", aleatorio ? "mapa aleatorio" : mi->titulo, njog, J.m.jogador, J.mapa->lado);
	return 0;
}

/* la misión de una campaña con su guion, como la juega el original */
static int jogar_missao(int ci, int mi_idx, const TzLevar *levar)
{
	jogo_fechar();
	memset(&J, 0, sizeof J);
	if (ci < 0 || ci >= mapas.ncamp || mi_idx < 0 || mi_idx >= mapas.camp[ci].nmissoes) return tz_falha("misión no válida");
	const TzMissao *mi = &mapas.camp[ci].missoes[mi_idx];
	J.tipo = 2; J.camp = ci; J.missao = mi_idx;
	if (tz_missao_abrir(&mapas, mi, &J.ab)) return -1;
	J.mapa = &J.ab.mapa;
	snprintf(J.fich, sizeof J.fich, "%s", mi->fich);
	snprintf(J.dentro, sizeof J.dentro, "%s", mi->dentro);
	TzRegras rg;
	memset(&rg, 0, sizeof rg);
	rg.populacao = 200; rg.nivel_recursos = 0; rg.campanha = 1; rg.dificuldade = (uint8_t)menu.cfg.dificuldade;
	if (jogo_montar(&rg, 666)) return -1;
	if (J.ab.guiao_ok || J.ab.guiao.nregras) {
		tz_guiao_registrar(&J.m, &J.ab.guiao, 1);
		TzGuiaoServicos sv;
		memset(&sv, 0, sizeof sv);
		sv.ia_modo = ia_modo; sv.ia_sem_ataque = ia_sem_ataque;
		tz_guiao_servicos(&J.ab.guiao, &sv);
		if (levar) tz_levar_para(&J.m, &J.ab.guiao, levar);
		J.com_guiao = 1;
	}
	jogo_ia();
	jogo_ui();
	tz_progresso_marcar(&progresso, mi->id, ruta_progresso);
	registro("[TZAR] campaña %s, misión %s: jugador %d, guion %d reglas", mapas.camp[ci].titulo, mi->titulo, J.m.jogador, J.ab.guiao.nregras);
	return 0;
}

/* la misión que toca de una campaña: la última alcanzada (o la primera) */
static int missao_atual(int ci)
{
	const TzCampanha *c = &mapas.camp[ci];
	int ult = 0;
	for (int i = 0; i < c->nmissoes; ++i) if (tz_progresso_tem(&progresso, c->missoes[i].id)) ult = i;
	return ult;
}

/* ------------------------------------------------------------------ guardar y cargar */
static void ruta_guardada(const char *nome, const char *ext, char *out, size_t n)
{
	char limpo[64];
	int k = 0;
	for (const char *p = nome; *p && k < 60; ++p) limpo[k++] = (*p == '/' || *p == '\\' || *p == ':') ? '_' : *p;
	limpo[k] = 0;
	snprintf(out, n, "%s/%s.%s", pasta_guardar, k ? limpo : "partida", ext);
}

static int guardar(const char *nome)
{
	if (!J.ativa) return -1;
	char r[800], ri[800];
	ruta_guardada(nome, "tzs", r, sizeof r);
	ruta_guardada(nome, "txt", ri, sizeof ri);
	FILE *f = fopen(ri, "w");
	if (!f) return tz_falha("no se puede escribir %s", ri);
	fprintf(f, "TZSAVE1\n%d\n%d\n%d\n%s\n%s\n", J.tipo, J.camp, J.missao, J.fich, J.dentro);
	if (J.tipo == 1) {
		char t[640];
		if (tz_aleatorio_texto(&aleat_usado, t, sizeof t)) { fclose(f); return -1; }
		fprintf(f, "%s\n", t);
	}
	fclose(f);
	if (tz_mundo_gravar(&J.m, r)) return -1;
	registro("[TZAR] partida guardada: %s", r);
	return 0;
}

static int carregar(const char *nome)
{
	char r[800], ri[800], l[7][640];
	ruta_guardada(nome, "tzs", r, sizeof r);
	ruta_guardada(nome, "txt", ri, sizeof ri);
	FILE *f = fopen(ri, "r");
	if (!f) return tz_falha("no existe la partida %s", nome);
	int n = 0;
	while (n < 7 && fgets(l[n], sizeof l[n], f)) { l[n][strcspn(l[n], "\r\n")] = 0; ++n; }
	fclose(f);
	if (n < 6 || strcmp(l[0], "TZSAVE1")) return tz_falha("partida no válida");
	int tipo = atoi(l[1]), ci = atoi(l[2]), mi = atoi(l[3]);
	if (tipo == 1) {
		/* mapa aleatorio: se regenera con los parámetros guardados (la partida trae el resto) */
		TzAleatorio op;
		if (n < 7 || tz_aleatorio_de_texto(l[6], &op)) return n < 7 ? tz_falha("partida en mapa aleatorio sin sus parámetros") : -1;
		aleat_forcado = &op;
		int r_ = jogar_mapa(NULL, &menu.cfg, 1);
		aleat_forcado = NULL;
		if (r_) return -1;
	} else if (tipo == 2) { if (jogar_missao(ci, mi, NULL)) return -1; }
	else {
		TzMissao m;
		memset(&m, 0, sizeof m);
		snprintf(m.fich, sizeof m.fich, "%s", l[4]);
		snprintf(m.dentro, sizeof m.dentro, "%s", l[5]);
		snprintf(m.titulo, sizeof m.titulo, "%s", l[4]);
		if (jogar_mapa(&m, &menu.cfg, 0)) return -1;
		/* la IA de la partida nueva se descarta: la cargada trae sus bloques */
	}
	if (tz_mundo_carregar(&J.m, &J.cat, J.mapa, r)) return -1;
	J.ui.minimapa = J.minimapa;
	/* los lectores de eventos siguen desde la cola de la partida cargada (su número no tiene que ver con el de antes) */
	J.ui.ev_cursor = J.m.ev_seq;
	if (som) tz_som_cursor(som, J.m.ev_seq);
	registro("[TZAR] partida cargada: %s (tic %u)", r, J.m.tic);
	return 0;
}

static void listar_guardadas(void)
{
	free(menu.guardados);
	menu.guardados = NULL; menu.nguardados = 0;
	DIR *d = opendir(pasta_guardar);
	if (!d) return;
	struct dirent *e;
	while ((e = readdir(d))) {
		size_t l = strlen(e->d_name);
		if (l < 5 || strcmp(e->d_name + l - 4, ".tzs")) continue;
		TzMnGuardado *g = realloc(menu.guardados, (size_t)(menu.nguardados + 1) * sizeof *g);
		if (!g) break;
		menu.guardados = g;
		memset(&g[menu.nguardados], 0, sizeof *g);
		snprintf(g[menu.nguardados].nome, sizeof g->nome, "%.*s", (int)(l - 4), e->d_name);
		menu.nguardados++;
	}
	closedir(d);
}

/* ------------------------------------------------------------------ las listas del menú */
static void preparar_menu(void)
{
	menu.ncampanhas = mapas.ncamp;
	menu.campanhas = calloc((size_t)(mapas.ncamp ? mapas.ncamp : 1), sizeof *menu.campanhas);
	for (int i = 0; i < mapas.ncamp && menu.campanhas; ++i) {
		snprintf(menu.campanhas[i].titulo, sizeof menu.campanhas[i].titulo, "%s", mapas.camp[i].titulo);
		snprintf(menu.campanhas[i].fich, sizeof menu.campanhas[i].fich, "%s", mapas.camp[i].ficheiro);
		menu.campanhas[i].descricao = mapas.camp[i].descricao;
	}
	menu.nmapas = mapas.nsoltas;
	menu.mapas = calloc((size_t)(mapas.nsoltas ? mapas.nsoltas : 1), sizeof *menu.mapas);
	for (int i = 0; i < mapas.nsoltas && menu.mapas; ++i) {
		TzMnItem *it = &menu.mapas[i];
		snprintf(it->titulo, sizeof it->titulo, "%s", mapas.soltas[i].titulo);
		snprintf(it->fich, sizeof it->fich, "%s", mapas.soltas[i].fich);
		it->jogadores_min = mapas.soltas[i].jogadores[0]; it->jogadores_max = mapas.soltas[i].jogadores[1];
		it->descricao = mapas.soltas[i].descricao;
	}
	listar_guardadas();
}

/* ------------------------------------------------------------------ entrada */

static int traduzir(const SDL_Event *e, TzUiEntrada *o)
{
	memset(o, 0, sizeof *o);
	if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_LEAVE) rato_dentro = 0;
	if (e->type == SDL_MOUSEMOTION) { int x = e->motion.x, y = e->motion.y; ponto_lienzo(&x, &y); rato_x = x; rato_y = y; rato_dentro = 1; }
	SDL_Keymod km = SDL_GetModState();
	o->mods = (km & KMOD_SHIFT ? TZ_MOD_SHIFT : 0) | (km & KMOD_CTRL ? TZ_MOD_CTRL : 0) | (km & KMOD_ALT ? TZ_MOD_ALT : 0);
	switch (e->type) {
	case SDL_MOUSEMOTION: o->tipo = TZ_IN_MOVER; o->x = rato_x; o->y = rato_y; return 1;
	case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
		o->tipo = e->type == SDL_MOUSEBUTTONDOWN ? TZ_IN_BAIXO : TZ_IN_CIMA;
		o->x = e->button.x; o->y = e->button.y;
		ponto_lienzo(&o->x, &o->y);
		rato_x = o->x; rato_y = o->y; rato_dentro = 1;
		o->botao = e->button.button == SDL_BUTTON_LEFT ? 1 : e->button.button == SDL_BUTTON_RIGHT ? 3 : 2;
		return 1;
	case SDL_MOUSEWHEEL: o->tipo = TZ_IN_RODA; o->delta = e->wheel.y; return 1;
	case SDL_KEYDOWN: {
		SDL_Keycode k = e->key.keysym.sym;
		o->tipo = TZ_IN_TECLA;
		switch (k) {
		case SDLK_ESCAPE: o->tecla = TZ_TC_ESC; break;
		case SDLK_RETURN: case SDLK_KP_ENTER: o->tecla = TZ_TC_ENTER; break;
		case SDLK_BACKSPACE: case SDLK_DELETE: o->tecla = TZ_TC_BORRAR; break;
		case SDLK_LEFT: o->tecla = TZ_TC_ESQ; break;
		case SDLK_RIGHT: o->tecla = TZ_TC_DIR; break;
		case SDLK_UP: o->tecla = TZ_TC_CIMA; break;
		case SDLK_DOWN: o->tecla = TZ_TC_BAIXO; break;
		case SDLK_HOME: o->tecla = TZ_TC_INICIO; break;
		case SDLK_PAGEUP: o->tecla = TZ_TC_PGUP; break;
		case SDLK_PAGEDOWN: o->tecla = TZ_TC_PGDN; break;
		default:
			if (k >= SDLK_F1 && k <= SDLK_F12) o->tecla = TZ_TC_F1 + (int)(k - SDLK_F1);
			else if (k > 0 && k < 256) o->tecla = (int)k;
			else return 0;
		}
		return 1;
	}
	}
	return 0;
}

/* ------------------------------------------------------------------ ventanas del guion (historia, avisos, conversaciones) */
static TzRect caixa_modal(void)
{
	int w = W < 640 ? W - 40 : 600, h = 300;
	return (TzRect){ (W - w) / 2, (H - h) / 2, w, h };
}

static TzRect rect_escolha(int i)
{
	TzRect c = caixa_modal();
	return (TzRect){ c.x + 20, c.y + 150 + i * 24, c.w - 40, 22 };
}

static void desenhar_modal(void)
{
	const TzMensagem *msg = tz_guiao_mensagem(&J.ab.guiao);
	if (!msg) return;
	TzRect c = caixa_modal();
	tz_tela_preencher(&tela, (TzRect){ 0, 0, W, H }, 0x60000000);
	tz_tela_preencher(&tela, c, 0xf0201810);
	tz_tela_moldura(&tela, c, 0xffc0a060);
	tz_ui_fonte_caixa(&tela, &skin.f14b, (TzRect){ c.x + 10, c.y + 8, c.w - 20, 22 }, msg->titulo[0] ? msg->titulo : "Tzar", 0xe8d890, 1);
	char linhas[12][160];
	int n = tz_ui_fonte_quebrar(&skin.f12b, msg->texto, c.w - 40, linhas, msg->nescolhas ? 5 : 10);
	for (int i = 0; i < n; ++i) tz_ui_fonte_texto(&tela, &skin.f12b, c.x + 20, c.y + 40 + i * 18, linhas[i], 0xf0e8d0);
	for (int i = 0; i < msg->nescolhas && i < 6; ++i) {
		TzRect r = rect_escolha(i);
		tz_tela_preencher(&tela, r, 0xc0403020);
		tz_ui_fonte_caixa(&tela, &skin.f12b, (TzRect){ r.x + 6, r.y + 2, r.w - 12, r.h - 4 }, msg->escolhas[i], 0xffe0a0, 0);
	}
	if (!msg->nescolhas) tz_ui_fonte_caixa(&tela, &skin.f10, (TzRect){ c.x, c.y + c.h - 24, c.w, 18 }, "(clic o Intro para seguir)", 0xb0a080, 1);
}

/* Cierra la ventana del guion o sigue la conversación. Una frase de conversación sin elecciones no se cierra con
 * tz_guiao_fechar (solo cierra historias y avisos): sigue con tz_guiao_responder, que dice la frase siguiente o acaba la
 * conversación. Antes se llamaba a fechar y la partida se quedaba para siempre con la frase en pantalla (el mundo parado,
 * la imagen congelada y la música sonando: el «cuelgue» de las campañas). */
static void modal_seguir(void)
{
	if (tz_guiao_fala(&J.ab.guiao)) tz_guiao_responder(&J.m, &J.ab.guiao, 0);
	else tz_guiao_fechar(&J.m, &J.ab.guiao);
}

static void modal_entrada(const TzUiEntrada *e)
{
	const TzMensagem *msg = tz_guiao_mensagem(&J.ab.guiao);
	int n = msg ? msg->nescolhas : 0;
	if (e->tipo == TZ_IN_BAIXO && e->botao == 1) {
		for (int i = 0; i < n && i < 6; ++i) if (tz_rect_dentro(rect_escolha(i), e->x, e->y)) { tz_guiao_responder(&J.m, &J.ab.guiao, i); return; }
		if (!n) modal_seguir();
	} else if (e->tipo == TZ_IN_TECLA) {
		if (n && e->tecla >= '1' && e->tecla < '1' + n) tz_guiao_responder(&J.m, &J.ab.guiao, e->tecla - '1');
		else if (!n && (e->tecla == TZ_TC_ENTER || e->tecla == ' ' || e->tecla == TZ_TC_ESC)) modal_seguir();
	}
}

static void pedidos(void)
{
	TzPedido p;
	while (tz_guiao_pedido(&J.ab.guiao, &p)) {
		switch (p.tipo) {
		case TZ_PD_HISTORIA:
			if (som && p.som[0]) tz_som_fala(som, p.som);
			if (som && p.musica[0]) tz_musica_tocar(tz_som_musica(som), p.musica, 0);
			break;
		case TZ_PD_FALA: {
			const TzFrase *f = tz_guiao_fala(&J.ab.guiao);
			if (f && f->texto) {
				const TzPersonagem *pe = f->fala_de ? tz_guiao_personagem(&J.ab.guiao, f->fala_de) : NULL;
				snprintf(J.fala, sizeof J.fala, "%s%s%s", pe ? pe->nome : "", pe ? ": " : "", f->texto);
				J.fala_ate = (uint32_t)J.ms + 6000;
				if (som && f->voz && *f->voz) tz_som_fala(som, f->voz);
			}
			break;
		}
		case TZ_PD_FECHAR: J.fala_ate = 0; break;
		case TZ_PD_LINHA: tz_ui_aviso(&J.ui, "%s", p.texto); break;
		case TZ_PD_NOTA: tz_ui_aviso(&J.ui, "%s", tz_ui_txt(&skin, "viewmsg", p.chave, p.texto)); break;
		case TZ_PD_TRAVAR: case TZ_PD_ROLAR:
			if (p.obj >= 0 && p.obj < J.m.nobj) tz_ui_centrar(&J.ui, J.m.obj[p.obj].cx, J.m.obj[p.obj].cy);
			else tz_ui_centrar(&J.ui, p.tipo == TZ_PD_ROLAR ? p.cx2 : p.cx, p.tipo == TZ_PD_ROLAR ? p.cy2 : p.cy);
			break;
		case TZ_PD_SELECIONAR:
			if (p.obj >= 0 && J.ui.sel.n < TZ_SEL_MAX) J.ui.sel.id[J.ui.sel.n++] = p.obj;
			break;
		case TZ_PD_LIMPAR_SELECAO: case TZ_PD_DESSELECIONAR: tz_ui_sel_limpar(&J.ui.sel); break;
		default: break;
		}
	}
}

/* ------------------------------------------------------------------ fin de la partida (D_ENDGAME) */
static int jogar_missao(int ci, int mi_idx, const TzLevar *levar);

static void fim_mostrar(void)
{
	menu.fim_ganhou = J.ganhou;
	menu.fim_seguinte = J.tipo == 2 && J.ganhou && J.com_guiao && *tz_guiao_proximo_mapa(&J.ab.guiao);
	int s = (int)(J.m.tic / TZ_TICS_SEG), n = 0;
	snprintf(menu.fim_resumo[n], sizeof menu.fim_resumo[0], "Tiempo de juego: %d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
	menu.fim_cor[n++] = 0;
	for (int k = 1; k <= 8 && n < 8; ++k) {
		const TzReinoEstado *r = &J.m.reino[k];
		if (!r->ativo) continue;
		snprintf(menu.fim_resumo[n], sizeof menu.fim_resumo[0], "%s%s: creadas %u, perdidas %u, matadas %u, recogido %u, puntos %u%s",
		         r->nome[0] ? r->nome : "Reino", k == J.m.jogador ? " (tú)" : "", r->criadas, r->perdidas, r->matadas,
		         r->recolhido[0] + r->recolhido[1] + r->recolhido[2] + r->recolhido[3], (r->pontos + 50) / 100, r->derrotado ? " - derrotado" : "");
		menu.fim_cor[n++] = r->cor < 8 ? skin.cores[r->cor] & 0xffffff : 0xffffff;
	}
	menu.nfim_resumo = n;
	estado = E_MENU;
	menu.tela = TZ_MN_PRINCIPAL; menu.npilha = 0;
	tz_menu_ir(&menu, TZ_MN_FIM);
	menu.npilha = 0;   /* «volver» del fin es «ocultar» */
}

static void atualizar_missoes(void);

static void resultado_seguir(void)
{
	if (J.tipo == 2 && J.ganhou && J.com_guiao && *tz_guiao_proximo_mapa(&J.ab.guiao)) {
		int ci = J.camp;
		int prox = tz_campanha_achar(&mapas.camp[ci], tz_guiao_proximo_mapa(&J.ab.guiao));
		static TzLevar lv;
		tz_levar_de(&J.m, &J.ab.guiao, &lv);
		if (prox >= 0) {
			tz_progresso_marcar(&progresso, mapas.camp[ci].missoes[prox].id, ruta_progresso);
			atualizar_missoes();
			if (!jogar_missao(ci, prox, &lv)) return;
			registro("[TZAR] no se puede abrir la misión siguiente: %s", tz_erro());
		}
	}
	jogo_fechar();
	estado = E_MENU;
	menu.npilha = 0; menu.tela = TZ_MN_PRINCIPAL;
}

/* ------------------------------------------------------------------ sin datos */
static void sem_dados(const char *motivo)
{
	tz_tela_preencher(&tela, (TzRect){ 0, 0, W, H }, 0xff140d09);
	static const char *const l[] = {
		"Tzar: no se encuentran los datos del juego.",
		"",
		"Copia la instalacion de Tzar (la carpeta con Packs, Maps y Campaign)",
		"en /jogos/TZAR, o indica la carpeta: tzar /ruta/a/Tzar",
		"(o en la variable TZAR_DATA). En las imagenes privadas: input_share/Tzar Anthology.",
		"Los datos del juego no vienen con SaguiOS. Ayuda: /ayuda/tzar.md",
		"",
		"Esc: salir",
	};
	int y = H / 3;
	for (unsigned i = 0; i < sizeof l / sizeof *l; ++i, y += 22) tz_ui_fonte_texto(&tela, &skin.f12b, 40, y, l[i], 0xf0e0c0);
	char b[400];
	snprintf(b, sizeof b, "Motivo: %s", motivo);
	tz_ui_fonte_texto(&tela, &skin.f12b, 40, y + 10, b, 0xff9060);
}

static int existe_pasta(const char *p)
{
	struct stat st;
	return p && !stat(p, &st) && S_ISDIR(st.st_mode);
}

/* ------------------------------------------------------------------ programa */
static void audio_volumes(void)
{
	if (som) tz_som_volumes(som, menu.volume_som, menu.volume_som, menu.volume_musica);
	if (menu.volume_som != op.volume_som || menu.volume_musica != op.volume_musica) {
		op.volume_som = menu.volume_som; op.volume_musica = menu.volume_musica;
		menu.op.volume_som = op.volume_som; menu.op.volume_musica = op.volume_musica;
		guardar_opcoes();
	}
}

/* el lienzo cambia de tamaño (resolución, interfaz x2, pantalla completa): menús e interfaz a la medida nueva */
static void redimensionar_lienzo(int w, int h)
{
	TzTela nt;
	if (tz_tela_criar(&nt, w, h)) { registro("[TZAR] sin memoria para un lienzo de %dx%d", w, h); return; }
	if (tela.px) memset(nt.px, 0, (size_t)w * h * 4);
	tz_tela_liberar(&tela);
	tela = nt; W = w; H = h;
	tz_menu_redimensionar(&menu, W, H);
	if (J.ativa) tz_ui_redimensionar(&J.ui, W, H);
}

/* la lista de misiones de las campañas, con las alcanzadas (el progreso cambia al pasar de misión) */
static void atualizar_missoes(void)
{
	int total = 0;
	for (int c = 0; c < mapas.ncamp; ++c) total += mapas.camp[c].nmissoes;
	if (menu.nmissoes != total || !menu.missoes) {
		free(menu.missoes);
		menu.missoes = calloc((size_t)(total ? total : 1), sizeof *menu.missoes);
		menu.nmissoes = menu.missoes ? total : 0;
	}
	int k = 0;
	for (int c = 0; c < mapas.ncamp && menu.missoes; ++c)
		for (int i = 0; i < mapas.camp[c].nmissoes; ++i, ++k) {
			const TzMissao *mi = &mapas.camp[c].missoes[i];
			TzMnItem *it = &menu.missoes[k];
			snprintf(it->titulo, sizeof it->titulo, "%s", mi->titulo);
			snprintf(it->fich, sizeof it->fich, "%s", mi->fich);
			snprintf(it->dentro, sizeof it->dentro, "%s", mi->dentro);
			it->descricao = mi->descricao;
			it->campanha = c; it->indice = i;
			it->disponivel = tz_missao_disponivel(&progresso, mi) || i == 0;
		}
}

/* ------------------------------------------------------------------ previsualización del mapa de la escaramuza */
static TzTerreno prev_ter;
static char prev_bioma[32];
static TzImg prev_img;
static int prev_idx = -2;

static char prev_aleat[640];   /* la línea del mapa aleatorio que se está viendo («» = la vista previa es de un mapa de la lista) */

static void previa_soltar(void)
{
	prev_aleat[0] = 0;
	aleat_cache_soltar();
	if (prev_bioma[0]) tz_terreno_liberar(&prev_ter);
	prev_bioma[0] = 0;
	tz_img_liberar(&prev_img);
	menu.previa = NULL;
	prev_idx = -2;
}

/* el minimapa del mapa mp con los objetos de los reinos como puntos de su color -> menu.previa */
static void previa_desenhar(TzMapa *mp)
{
	if (strcmp(prev_bioma, mp->bioma)) {
		if (prev_bioma[0]) tz_terreno_liberar(&prev_ter);
		prev_bioma[0] = 0;
		if (!tz_terreno_carregar(&prev_ter, &rec, mp->bioma)) snprintf(prev_bioma, sizeof prev_bioma, "%s", mp->bioma);
	}
	int lado = mp->lado;
	if (prev_bioma[0] && lado > 0) {
		prev_img.px = malloc((size_t)lado * lado * 4);
		if (prev_img.px && !tz_terreno_minimapa(&prev_ter, mp, prev_img.px)) {
			prev_img.w = prev_img.h = lado;
			for (int i = 0; i < lado * lado; ++i) prev_img.px[i] |= 0xff000000u;
			/* los objetos de los reinos, como puntos de su color */
			for (int i = 0; i < mp->nobj; ++i) {
				const TzObjeto *o = &mp->obj[i];
				int cor = tz_mapa_cor(mp, o->dono);
				if (o->dono < 1 || o->dono > 8 || cor < 0 || o->x >= lado || o->y >= lado) continue;
				uint32_t c = 0xff000000u | skin.cores_mini[cor & 7];
				for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx)
					if (o->x + dx < lado && o->y + dy < lado) prev_img.px[(size_t)(o->y + dy) * lado + o->x + dx] = c;
			}
			menu.previa = &prev_img;
		} else { free(prev_img.px); prev_img.px = NULL; }
	}
}

static void previa_atualizar(void)
{
	if (menu.tela != TZ_MN_MAPAS && menu.tela != TZ_MN_CONFIG && menu.tela != TZ_MN_ALEAT) return;
	if (menu.tela == TZ_MN_ALEAT) {
		/* el mapa aleatorio con lo que hay en pantalla: se regenera solo cuando cambia algún dato (la misma línea = el mismo mapa) */
		char clave[640];
		if (tz_mn_aleat_texto(&menu.aleat, clave, sizeof clave)) return;
		char *cauda = strstr(clave, " IA ");   /* la IA de los reinos no cambia el mapa (D470) */
		if (cauda) *cauda = 0;
		if (!strcmp(clave, prev_aleat)) return;
		snprintf(prev_aleat, sizeof prev_aleat, "%s", clave);
		prev_idx = -2;
		tz_img_liberar(&prev_img);
		menu.previa = NULL;
		TzAleatorio ao;
		TzMapa mp;
		aleat_de_menu(&menu.aleat, &ao);
		tz_vigia_entrar("previa del mapa aleatorio");
		if (!aleat_gerar(&mp, &ao)) { previa_desenhar(&mp); tz_mapa_liberar(&mp); }
		else registro("[TZAR] vista previa aleatoria: %s", tz_erro());
		tz_vigia_sair();
		return;
	}
	if (menu.mapa_sel == prev_idx && !prev_aleat[0]) return;
	prev_aleat[0] = 0;
	prev_idx = menu.mapa_sel;
	tz_img_liberar(&prev_img);
	menu.previa = NULL;
	if (prev_idx < 0 || prev_idx >= mapas.nsoltas) return;
	tz_vigia_entrar("previa del mapa");
	TzMissaoAberta ab;
	if (!tz_missao_abrir(&mapas, &mapas.soltas[prev_idx], &ab)) {
		previa_desenhar(&ab.mapa);
		tz_missao_fechar(&ab);
	}
	tz_vigia_sair();
}

/* ------------------------------------------------------------------ chat y voces de la IA */
static uint32_t ia_falas[TZ_REINOS];

static void chat_tic(void)
{
	if (J.ui.chat_saida[0]) {
		/* «ai <orden>» (o «hmai <orden>») va a la IA de todos los reinos del ordenador: talk on/off, none, eco, war, full */
		const char *t = J.ui.chat_saida;
		const char *o = !strncasecmp(t, "hmai ", 5) ? t + 5 : !strncasecmp(t, "ai ", 3) ? t + 3 : NULL;
		int ok = 0;
		if (o) for (int i = 0; i < J.nias; ++i) if (J.ias[i] && !tz_ia_ordem(J.ias[i], o)) ok = 1;
		tz_ui_chat_linha(&J.ui, J.m.jogador, o ? (ok ? "(la IA obedece)" : "(la IA no entiende esa orden)") : NULL);
		registro("[TZAR] chat: %s", t);
		J.ui.chat_saida[0] = 0;
		if (som) tz_som_tocar(som, "SOUND/NOTIFY/CHAT.WAV", TZ_CAT_UNIDADE_PRONTA, TZ_CANAL_UNIDADE, -1, -1);
	}
	for (int i = 0; i < J.nias && i < TZ_REINOS; ++i) {
		const char *f;
		while (J.ias[i] && (f = tz_ia_fala(J.ias[i], &ia_falas[i])) != NULL) tz_ui_chat_recebido(&J.ui, f);
	}
}

/* ------------------------------------------------------------------ vigilante */
static void vigia_parar(int parar) { if (som) tz_som_pausar(som, parar); }
static void vigia_registro(const char *l) { registro("%s", l); }

/* ------------------------------------------------------------------ dibujo de la partida */
static void desenhar_jogo(void)
{
	TzUiMapa mp = { &J.ter, J.mapa, &J.esc, J.ms };
	tz_animar(&J.esc, J.ms);
	tz_ui_mapa_desenhar(&J.ui, &tela, &mp);
	clima_desenhar(clima_intensidade(J.ms));
	tz_ui_desenhar_marcas(&J.ui, &tela);
	tz_ui_desenhar_hud(&J.ui, &tela);
	if (J.fala_ate > J.ms && J.fala[0]) {
		TzRect r = { J.ui.mapa_rect.x + 10, J.ui.mapa_rect.y + J.ui.mapa_rect.h - 60, J.ui.mapa_rect.w - 20, 50 };
		tz_tela_preencher(&tela, r, 0xb0000000);
		char l2[3][160];
		int nl = tz_ui_fonte_quebrar(&skin.f12b, J.fala, r.w - 20, l2, 3);
		for (int i = 0; i < nl; ++i) tz_ui_fonte_texto(&tela, &skin.f12b, r.x + 10, r.y + 4 + i * 15, l2[i], 0xf8f0c0);
	}
	if (J.com_guiao && tz_guiao_modal(&J.ab.guiao)) desenhar_modal();
}

/* repetir la partida que acabó (la misma misión o el mismo mapa) */
static void repetir_partida(void)
{
	int tipo = J.tipo, ci = J.camp, mi = J.missao;
	TzMissao m;
	memset(&m, 0, sizeof m);
	snprintf(m.fich, sizeof m.fich, "%s", J.fich);
	snprintf(m.dentro, sizeof m.dentro, "%s", J.dentro);
	snprintf(m.titulo, sizeof m.titulo, "%s", J.fich);
	TzAleatorio mesmo = aleat_usado;   /* mapa aleatorio: el mismo mapa con las mismas opciones */
	if (tipo == 1) aleat_forcado = &mesmo;
	int r = tipo == 2 ? jogar_missao(ci, mi, NULL) : tipo == 1 ? jogar_mapa(NULL, &menu.cfg, 1) : jogar_mapa(&m, &menu.cfg, 0);
	aleat_forcado = NULL;
	if (r) { registro("[TZAR] no se puede repetir: %s", tz_erro()); jogo_fechar(); estado = E_MENU; menu.tela = TZ_MN_PRINCIPAL; menu.npilha = 0; }
}

/* Prueba (h417 «janelas_saguios», TZAR_PRUEBA_JANELAS=1): crea en la partida la taberna, el mercado, la catedral, la fábrica mágica, un mercader y
 * un infante, abre cada ventana de negocio una a una, la dibuja en la pantalla de verdad (SDL de SaguiOS) y cuenta cuántas salen con su
 * marco (la caja de piedra de ui_janelas.c). Una línea de registro con el resultado. */
static void prova_janelas(void)
{
	TzMundo *m = &J.m;
	int k = m->jogador;
	TzId castelo = TZ_NENHUM;
	for (int i = 0; i < m->nobj; ++i) if (m->obj[i].classe == TZC_CASTLE && m->obj[i].dono == k) { castelo = i; break; }
	if (castelo < 0) { registro("[TZAR] prueba ventanas: no hay castillo"); return; }
	int cx = m->obj[castelo].cx, cy = m->obj[castelo].cy;
	struct { int classe; TzId id; } obj[] = { { TZC_TAVERN, -1 }, { TZC_MARKET, -1 }, { TZC_CATHEDRAL, -1 }, { tz_catalogo_classe("magicworkshop"), -1 }, { TZC_TRADER, -1 }, { 3, -1 } };
	for (unsigned q = 0; q < sizeof obj / sizeof *obj; ++q)
		for (int d = 4; d < 16 && obj[q].id < 0; d += 2)
			for (int s = 0; s < 4 && obj[q].id < 0; ++s)
				obj[q].id = tz_mundo_guiao_criar(m, obj[q].classe, 0, k, cx + (s & 1 ? -d : d), cy + (s & 2 ? -d : d));
	for (int r = 0; r < TZ_NREC; ++r) m->reino[k].recursos[r] = 5000;
	static const struct { int janela, qual; } js[] = {
		{ TZ_UJ_TABERNA, 0 }, { TZ_UJ_DADOS, 0 }, { TZ_UJ_PRESTAMOS, 0 }, { TZ_UJ_INTERCAMBIO, 0 }, { TZ_UJ_COMERCIO, 4 }, { TZ_UJ_OBJETOS, 5 },
		{ TZ_UJ_CRUZADA, 2 }, { TZ_UJ_TRIBUTO, -1 }, { TZ_UJ_RENDICION, -1 }, { TZ_UJ_FABRICA, 3 }, { TZ_UJ_MERCADO, 1 },
	};
	int abertas = 0, desenhadas = 0;
	for (unsigned q = 0; q < sizeof js / sizeof *js; ++q) {
		TzId o = js[q].qual >= 0 ? obj[js[q].qual].id : TZ_NENHUM;
		tz_ui_sel_limpar(&J.ui.sel);
		if (o >= 0) { J.ui.sel.id[0] = o; J.ui.sel.n = 1; }
		if (tz_ui_janela_abrir(&J.ui, js[q].janela, o)) continue;
		abertas++;
		desenhar_jogo();
		TzRect r = tz_ui_janela_rect(&J.ui);
		if (r.w > 20 && r.y >= 0 && r.y < tela.h && tela.px[(size_t)r.y * tela.w + r.x + r.w / 2] == 0xffc0a060u) desenhadas++;
		J.ui.janela = 0;
	}
	registro("[TZAR] prueba ventanas: %d abiertas de %d, %d dibujadas", abertas, (int)(sizeof js / sizeof *js), desenhadas);
}

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "--visor")) return tz_visor_main(argc - 1, argv + 1);
	if (SDL_Init(SDL_INIT_VIDEO)) { registro("[TZAR] SDL: %s", SDL_GetError()); return 1; }
	/* opciones guardadas (tzar.cfg en la carpeta de las partidas) */
	tz_opcoes_padrao(&op);
	const char *g = getenv("TZAR_GUARDAR"), *home = getenv("HOME");
	if (g && *g) snprintf(pasta_guardar, sizeof pasta_guardar, "%s", g);
	else if (home && *home) snprintf(pasta_guardar, sizeof pasta_guardar, "%s/.tzar", home);
	if (pasta_guardar[0]) { snprintf(ruta_opcoes, sizeof ruta_opcoes, "%s/tzar.cfg", pasta_guardar); tz_opcoes_ler(&op, ruta_opcoes); }
	if (getenv("TZAR_FULL")) op.tela_cheia = atoi(getenv("TZAR_FULL")) != 0;
	/* la ventana: la resolución elegida si cabe en la pantalla (con su marco y la barra de tareas); TZAR_W/TZAR_H la fijan (pruebas) */
	SDL_DisplayMode dm;
	int maxw = 1024, maxh = 768;
	if (!SDL_GetDesktopDisplayMode(0, &dm) && dm.w > 0 && dm.h > 0) { maxw = dm.w - 16; maxh = dm.h - 64; }
	if (getenv("TZAR_W") || getenv("TZAR_H")) {
		tamanho_fixo = 1;
		janela_w = getenv("TZAR_W") ? atoi(getenv("TZAR_W")) : 1024; janela_h = getenv("TZAR_H") ? atoi(getenv("TZAR_H")) : 768;
		if (janela_w < 800) janela_w = 800;
		if (janela_h < 600) janela_h = 600;
	} else tz_modo_janela(&op, maxw, maxh, &janela_w, &janela_h);
	win = SDL_CreateWindow("Tzar", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, janela_w, janela_h, 0);
	if (!win && (janela_w != 800 || janela_h != 600)) { janela_w = 800; janela_h = 600; win = SDL_CreateWindow("Tzar", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, 0); }
	if (tamanho_fixo) { W = janela_w; H = janela_h; tz_modo_destino(&op, W, H, janela_w, janela_h, &modo); }
	else { tz_modo_calcular(&op, janela_w, janela_h, &modo); W = modo.cw; H = modo.ch; }
	if (!win || tz_tela_criar(&tela, W, H)) { registro("[TZAR] SDL: no se puede abrir la ventana %dx%d: %s", janela_w, janela_h, SDL_GetError()); return 1; }
	const char *shot = getenv("TZAR_SHOT");
	int frames = getenv("TZAR_FRAMES") ? atoi(getenv("TZAR_FRAMES")) : 0;
	int auto_fechar = getenv("TZAR_AUTO_FECHAR") ? atoi(getenv("TZAR_AUTO_FECHAR")) : 0;
	/* pruebas del vigilante: TZAR_PRUEBA_PARADA=cuadro:ms para el bucle esos ms en ese cuadro, como un módulo colgado */
	int parada_q = 0, parada_ms = 0;
	if (getenv("TZAR_PRUEBA_PARADA")) { const char *pp = getenv("TZAR_PRUEBA_PARADA"); parada_q = atoi(pp); parada_ms = strchr(pp, ':') ? atoi(strchr(pp, ':') + 1) : 0; }
	/* datos */
	const char *pedida = argc > 1 ? argv[1] : getenv("TZAR_DATA");
	const char *cands[] = { "/jogos/TZAR", "/jogos/Tzar", "/jogos/tzar" };
	const char *r0 = NULL;
	if (pedida && *pedida) r0 = existe_pasta(pedida) ? pedida : NULL;
	else for (unsigned i = 0; i < sizeof cands / sizeof *cands && !r0; ++i) if (existe_pasta(cands[i])) r0 = cands[i];
	char motivo[400] = "";
	if (!r0) snprintf(motivo, sizeof motivo, "no existe la carpeta %s", pedida && *pedida ? pedida : "/jogos/TZAR");
	else {
		snprintf(raiz, sizeof raiz, "%s", r0);
		if (tz_recursos_abrir(&rec, raiz)) snprintf(motivo, sizeof motivo, "%s", tz_erro());
		else if (tz_mapas_ler(&mapas, raiz, NULL) || (!mapas.nsoltas && !mapas.ncamp)) snprintf(motivo, sizeof motivo, "la instalacion no tiene mapas ni campanas");
	}
	registro("[TZAR] juego: ventana %dx%d, datos %s", janela_w, janela_h, *motivo ? "no" : raiz);
	if (!*motivo && tz_ui_skin_carregar(&skin, &rec)) {
		/* sin el arte de la interfaz (instalación incompleta o sintética) se juega con la skin propia, sin arte */
		registro("[TZAR] interfaz sin arte: %s", tz_erro());
		tz_ui_skin_vazia(&skin);
	}
	if (*motivo) {
		tz_ui_skin_vazia(&skin);
		registro("[TZAR] sin datos del juego: %s", motivo);
		for (int n = 0; !frames || n < frames; ++n) {
			SDL_Event e;
			int sair = 0;
			while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) sair = 1;
			if (sair) break;
			sem_dados(motivo);
			presentar();
			if (shot && n + 1 == frames) captura(shot);
			SDL_Delay(30);
		}
		SDL_Quit();
		return 1;
	}
	/* dónde se guarda */
	if (!pasta_guardar[0]) {
		snprintf(pasta_guardar, sizeof pasta_guardar, "%s/guardadas", raiz);
		snprintf(ruta_opcoes, sizeof ruta_opcoes, "%s/tzar.cfg", pasta_guardar);
		tz_opcoes_ler(&op, ruta_opcoes);
	}
	mkdir(pasta_guardar, 0755);
	snprintf(ruta_progresso, sizeof ruta_progresso, "%s/progreso.txt", pasta_guardar);
	tz_progresso_ler(&progresso, ruta_progresso);
	/* sonido */
	if (!getenv("TZAR_SEM_SOM")) {
		tem_bib_som = !tz_biblioteca_carregar(&bib_som, &rec, "grass");
		som = tz_som_criar(&rec, tem_bib_som ? &bib_som : NULL, 1);
	}
	tz_menu_iniciar(&menu, &skin, W, H);
	menu.op = op;
	/* las últimas opciones del mapa aleatorio; sin ellas, una semilla del reloj */
	if (tz_mn_aleat_de_texto(op.aleat, &menu.aleat)) menu.aleat.semente = (SDL_GetTicks() * 2654435761u) | 1;
	menu.volume_som = op.volume_som; menu.volume_musica = op.volume_musica;
	preparar_menu();
	atualizar_missoes();
	audio_volumes();
	if (op.cursor_jogo) SDL_ShowCursor(SDL_DISABLE);
	registro("[TZAR] instalacion: %d paquetes, %d mapas sueltos, %d campanas, sonido %s, guardar en %s", rec.npac, mapas.nsoltas, mapas.ncamp,
	         som && tz_som_tem_dispositivo(som) ? "si" : "no", pasta_guardar);
	/* el vigilante: si el bucle no avanza en TZAR_VIGIA segundos (8), calla el audio y deja dónde estaba */
	tz_vigia_iniciar(getenv("TZAR_VIGIA") ? atoi(getenv("TZAR_VIGIA")) : 8, vigia_parar, vigia_registro);
	/* introducción */
	if (!getenv("TZAR_SEM_INTRO") && !frames) {
		TzIntro *v = tz_intro_abrir(raiz);
		if (!v) registro("[TZAR] sin introducción: %s", tz_erro());
		if (v) {
			int vw, vh, fps;
			tz_intro_tamanho(v, &vw, &vh, &fps);
			registro("[TZAR] introducción: %dx%d a %d cuadros por segundo", vw, vh, fps);
			uint32_t *px = malloc((size_t)vw * vh * 4);
			Uint32 t0 = SDL_GetTicks();
			int q = 0;
			tz_vigia_entrar("introducción");
			for (int fim = 0; px && !fim; ++q) {
				SDL_Event e;
				tz_vigia_latido();
				while (SDL_PollEvent(&e)) if (e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_QUIT) fim = 1;
				int r = tz_intro_quadro(v, px, som);
				if (r == 1) break;
				if (r < 0 && q > 0 && ++fim) registro("[TZAR] introducción: fallo al leer el cuadro %d", q);   /* un fallo no la deja colgada */
				if (r == 0) {
					tz_tela_preencher(&tela, (TzRect){ 0, 0, W, H }, 0xff000000);
					TzImg im = { vw, vh, px };
					int esc = (W / vw < H / vh ? W / vw : H / vh);
					if (esc < 1) esc = 1;
					tz_tela_imagem_escala(&tela, &im, (TzRect){ 0, 0, vw, vh }, (TzRect){ (W - vw * esc) / 2, (H - vh * esc) / 2, vw * esc, vh * esc });
					presentar();
				}
				Uint32 quer = t0 + (Uint32)((q + 1) * 1000 / (fps > 0 ? fps : 25));
				Uint32 agora = SDL_GetTicks();
				if (quer > agora) SDL_Delay(quer - agora);
			}
			tz_vigia_sair();
			registro("[TZAR] introducción: %d cuadros", q);
			free(px);
			tz_intro_fechar(v);
			if (som) tz_som_parar_filme(som);
		}
	}
	if (som) tz_musica_menu(tz_som_musica(som));
	/* pruebas: abrir una pantalla del menú (TZAR_MENU=n, TZ_MN_*) con el primer mapa o la primera campaña elegidos */
	if (getenv("TZAR_ALEAT")) {   /* los espacios pueden ir como «_» (en el terminal de SaguiOS es más fácil que las comillas) */
		char la[640];
		snprintf(la, sizeof la, "%s", getenv("TZAR_ALEAT"));
		for (char *p = la; *p; ++p) if (*p == '_') *p = ' ';
		if (tz_mn_aleat_de_texto(la, &menu.aleat)) registro("[TZAR] TZAR_ALEAT no vale");
	}   /* pruebas: las opciones del mapa aleatorio */
	if (getenv("TZAR_MENU")) { menu.mapa_sel = 0; menu.campanha_sel = 0; tz_menu_ir(&menu, atoi(getenv("TZAR_MENU"))); }
	if (getenv("TZAR_AMPLIAR")) menu.ampliada = 1;   /* pruebas: la vista previa del mapa aleatorio ampliada */
	/* entrar solo en una partida (pruebas) */
	const char *aut = getenv("TZAR_AUTO");
	if (aut && !strcmp(aut, "escaramuza") && mapas.nsoltas) { if (jogar_mapa(&mapas.soltas[0], &menu.cfg, 0)) registro("[TZAR] escaramuza: %s", tz_erro()); }
	else if (aut && !strcmp(aut, "aleatorio")) {
		/* pruebas: el mapa aleatorio con las opciones de la pantalla (TZAR_ALEAT = su línea de tzar.cfg, o las recordadas) */
		TzAleatorio ao;
		aleat_de_menu(&menu.aleat, &ao);
		aleat_forcado = &ao;
		if (jogar_mapa(NULL, &menu.aleat.c, 1)) registro("[TZAR] mapa aleatorio: %s", tz_erro());
		aleat_forcado = NULL;
		if (getenv("TZAR_PRUEBA_GUARDAR") && J.ativa) {
			/* pruebas: guardar, volver a cargar y comprobar que el mapa y sus opciones son los mismos (h417 «pc») */
			char t1[640], t2[640];
			uint32_t h1 = 2166136261u, h2 = 2166136261u;
			tz_aleatorio_texto(&aleat_usado, t1, sizeof t1);
			for (int i = 0; i < J.mapa->lado * J.mapa->lado; ++i) h1 = (h1 ^ J.mapa->casillas[i]) * 16777619u;
			int no1 = J.mapa->nobj, ok = !guardar("prueba") && !carregar("prueba") && J.tipo == 1;
			tz_aleatorio_texto(&aleat_usado, t2, sizeof t2);
			if (ok) for (int i = 0; i < J.mapa->lado * J.mapa->lado; ++i) h2 = (h2 ^ J.mapa->casillas[i]) * 16777619u;
			registro("[TZAR] prueba guardar/cargar aleatorio: %s (%d y %d objetos)", ok && h1 == h2 && !strcmp(t1, t2) && no1 == J.mapa->nobj ? "igual" : "DISTINTO", no1, ok ? J.mapa->nobj : -1);
		}
	}
	else if (aut && !strncmp(aut, "campanha", 8) && mapas.ncamp) {
		/* campanha[:campaña[:misión]] */
		int ci = aut[8] == ':' ? atoi(aut + 9) : 0;
		if (ci < 0 || ci >= mapas.ncamp) ci = 0;
		const char *pm = aut[8] == ':' ? strchr(aut + 9, ':') : NULL;
		int mi = pm ? atoi(pm + 1) : missao_atual(ci);
		if (mi < 0 || mi >= mapas.camp[ci].nmissoes) mi = 0;
		if (jogar_missao(ci, mi, NULL)) registro("[TZAR] campaña: %s", tz_erro());
	}
	if (getenv("TZAR_PRUEBA_JANELAS") && J.ativa) prova_janelas();
	Uint32 ultimo = SDL_GetTicks(), fps_t0 = ultimo;
	int fps_n = 0, modal_quadros = 0, full_pendente = op.tela_cheia && !tamanho_fixo;
	tela_cheia_atual = 0;
	for (int n = 0; estado != E_SAIR && (!frames || n < frames); ++n) {
		tz_vigia_latido();
		tz_vigia_entrar("eventos");
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_QUIT) { estado = E_SAIR; break; }
			/* F11 y Alt+Intro: ventana o pantalla completa (en SaguiOS el F11 lo atiende el sistema: se nota abajo) */
			if (ev.type == SDL_KEYDOWN && (ev.key.keysym.sym == SDLK_F11 || (ev.key.keysym.sym == SDLK_RETURN && (ev.key.keysym.mod & KMOD_ALT)))) {
				aplicar_tela_cheia(!op.tela_cheia);
				menu.op.tela_cheia = op.tela_cheia;
				guardar_opcoes();
				continue;
			}
			TzUiEntrada e;
			if (!traduzir(&ev, &e)) continue;
			Uint32 agora = SDL_GetTicks();
			if (estado == E_MENU) tz_menu_entrada(&menu, &e);
			else if (estado == E_JOGO) {
				if (J.com_guiao && tz_guiao_modal(&J.ab.guiao)) modal_entrada(&e);
				else if (J.ui.janela) tz_ui_entrada(&J.ui, &e, agora);   /* diplomacia o chat abiertos: todo para ellos */
				else if (e.tipo == TZ_IN_TECLA && e.tecla == '+') { if (J.velocidade < 40) J.velocidade += 5; }
				else if (e.tipo == TZ_IN_TECLA && e.tecla == '-') { if (J.velocidade > 5) J.velocidade -= 5; }
				else if (e.tipo == TZ_IN_TECLA && e.tecla == TZ_TC_F1) { estado = E_MENU; menu.tela = TZ_MN_PARTIDA; menu.npilha = 0; tz_menu_ir(&menu, TZ_MN_AJUDA); }
				else if (e.tipo == TZ_IN_TECLA && e.tecla == TZ_TC_F1 + 4) { if (guardar("rapida")) registro("[TZAR] guardar: %s", tz_erro()); else tz_ui_aviso(&J.ui, "Partida guardada"); }
				else if (e.tipo == TZ_IN_TECLA && e.tecla == TZ_TC_F1 + 8) { if (carregar("rapida")) registro("[TZAR] cargar: %s", tz_erro()); }
				else tz_ui_entrada(&J.ui, &e, agora);
			}
		}
		tz_vigia_sair();
		if (estado == E_SAIR) break;
		Uint32 agora = SDL_GetTicks();
		int dt = (int)(agora - ultimo);
		ultimo = agora;
		if (dt > 200) dt = 200;
		if (frames) dt = TZ_TIC_MS;   /* las pruebas: un tic por cuadro */
		/* la pantalla completa: la pedida al empezar (cuando la ventana ya existe) y la que cambió el sistema (su F11) */
		if (full_pendente && n >= 3) { full_pendente = 0; aplicar_tela_cheia(1); }
		else if (!full_pendente) {
			int fs = (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN) != 0;
			if (fs != tela_cheia_atual) { aplicar_tela_cheia(fs); menu.op.tela_cheia = op.tela_cheia; guardar_opcoes(); }
		}
		/* el menú: sus acciones */
		if (estado == E_MENU) {
			tz_vigia_entrar("menú");
			tz_menu_tic(&menu, agora);
			audio_volumes();
			if (menu.op_mudou) {
				/* opciones de pantalla cambiadas en el menú */
				int cheia = menu.op.tela_cheia;
				menu.op_mudou = 0;
				menu.op.volume_som = op.volume_som; menu.op.volume_musica = op.volume_musica;
				op = menu.op;
				SDL_ShowCursor(op.cursor_jogo ? SDL_DISABLE : SDL_ENABLE);
				if (J.ativa) tz_ui_lado(&J.ui, op.painel_direita);
				if (!tamanho_fixo && !tela_cheia_atual) {
					int jw, jh;
					tz_modo_janela(&op, maxw, maxh, &jw, &jh);
					if (jw != janela_w || jh != janela_h) { janela_w = jw; janela_h = jh; SDL_SetWindowSize(win, jw, jh); }
				}
				if (cheia != tela_cheia_atual) aplicar_tela_cheia(cheia);
				else { SDL_Surface *ws = SDL_GetWindowSurface(win); if (ws) ajustar_modo(ws->w, ws->h); }
				guardar_opcoes();
			}
			previa_atualizar();
			if (menu.aleat_mudou && (menu.tela != TZ_MN_ALEAT || menu.acao)) {
				/* se recuerdan las últimas opciones del mapa aleatorio (tzar.cfg) al salir de su pantalla o al jugar */
				menu.aleat_mudou = 0;
				tz_mn_aleat_texto(&menu.aleat, op.aleat, sizeof op.aleat);
				snprintf(menu.op.aleat, sizeof menu.op.aleat, "%s", op.aleat);
				guardar_opcoes();
			}
			int a = menu.acao;
			menu.acao = 0;
			if (a == TZ_MR_SAIR) { estado = E_SAIR; tz_vigia_sair(); break; }
			if (a == TZ_MR_JOGAR || a == TZ_MR_CAMPANHA || a == TZ_MR_CARREGAR) previa_soltar();
			tz_vigia_entrar("carga");
			if (a == TZ_MR_JOGAR) {
				int i = menu.res_indice;
				int aleat = i < 0 || i >= mapas.nsoltas;
				TzAleatorio ao;
				if (aleat) { aleat_de_menu(&menu.res_aleat, &ao); aleat_forcado = &ao; }   /* el mapa aleatorio con lo elegido en su pantalla */
				if (jogar_mapa(aleat ? NULL : &mapas.soltas[i], &menu.res_cfg, aleat))
					registro("[TZAR] no se puede jugar: %s", tz_erro());
				aleat_forcado = NULL;
			} else if (a == TZ_MR_CAMPANHA && menu.res_indice >= 0 && menu.res_indice < mapas.ncamp) {
				int ci = menu.res_indice, mi = menu.res_missao >= 0 && menu.res_missao < mapas.camp[ci].nmissoes ? menu.res_missao : missao_atual(ci);
				if (jogar_missao(ci, mi, NULL)) registro("[TZAR] campaña: %s", tz_erro());
			} else if (a == TZ_MR_CARREGAR && menu.res_indice >= 0 && menu.res_indice < menu.nguardados) {
				if (carregar(menu.guardados[menu.res_indice].nome)) registro("[TZAR] cargar: %s", tz_erro());
			} else if (a == TZ_MR_GUARDAR && J.ativa) {
				if (guardar(menu.res_nome)) registro("[TZAR] guardar: %s", tz_erro());
				listar_guardadas();
				estado = E_JOGO;
			} else if (a == TZ_MR_CONTINUAR && J.ativa) estado = E_JOGO;
			else if (a == TZ_MR_RENDIR && J.ativa) { estado = E_JOGO; tz_ui_janela_abrir(&J.ui, TZ_UJ_RENDICION, TZ_NENHUM); }
			else if (a == TZ_MR_REINICIAR && J.ativa) { jogo_fechar(); menu.tela = TZ_MN_PRINCIPAL; menu.npilha = 0; }
			else if (a == TZ_MR_SEGUINTE && J.ativa) resultado_seguir();
			else if (a == TZ_MR_REPETIR && J.ativa) repetir_partida();
			else if (a == TZ_MR_ABANDONAR) { jogo_fechar(); menu.tela = TZ_MN_PRINCIPAL; menu.npilha = 0; atualizar_missoes(); }
			tz_vigia_sair();
			if (!J.ativa && estado == E_JOGO) estado = E_MENU;
			tz_vigia_sair();
		}
		/* la partida */
		if (estado == E_JOGO && J.ativa) {
			if (J.ui.pedir_menu) { J.ui.pedir_menu = 0; listar_guardadas(); estado = E_MENU; menu.tela = TZ_MN_PRINCIPAL; menu.npilha = 0; tz_menu_ir(&menu, TZ_MN_PARTIDA); }
			else if (J.ui.pedir_sair) { J.ui.pedir_sair = 0; jogo_fechar(); estado = E_MENU; menu.tela = TZ_MN_PRINCIPAL; menu.npilha = 0; }
		}
		if (estado == E_JOGO && J.ativa) {
			int modal = J.com_guiao && tz_guiao_modal(&J.ab.guiao);
			/* pruebas: un «jugador» que cierra las ventanas del guion tras unos cuadros */
			if (modal && auto_fechar && ++modal_quadros >= auto_fechar) {
				modal_quadros = 0;
				const TzMensagem *msg = tz_guiao_mensagem(&J.ab.guiao);
				if (msg && msg->nescolhas) tz_guiao_responder(&J.m, &J.ab.guiao, 0); else modal_seguir();
				registro("[TZAR] guion: ventana cerrada (%s)", tz_guiao_modal(&J.ab.guiao) ? "sigue otra" : "sigue la partida");
			}
			if (!modal && !J.ui.pausa) {
				tz_vigia_entrar("mundo");
				J.acum += dt * J.velocidade;
				int t = 0;
				while (J.acum >= TZ_TIC_MS * 10 && t < 6) { tz_vigia_dado(J.m.tic); tz_mundo_tic(&J.m); J.acum -= TZ_TIC_MS * 10; ++t; J.ms += TZ_TIC_MS; }
				if (t == 6) J.acum = 0;
				if (t) memcpy(J.mapa->casillas, J.m.casillas, (size_t)J.m.lado * J.m.lado * sizeof *J.mapa->casillas);
				tz_vigia_sair();
			}
			tz_vigia_entrar("guion e interfaz");
			if (J.com_guiao && J.ui.pergaminho_pedido >= 0) {
				char tx[800], re[80], so[64], mu[64];
				uint32_t pms; int pf;
				if (tz_mundo_pergaminho(&J.m, J.ui.pergaminho_pedido, J.ui.pergaminho_ao_chegar, tx, sizeof tx, re, so, mu, &pms, &pf)
				    && tz_guiao_historia_texto(&J.ab.guiao, tx, re, so, mu, pms, pf) == 0)
					J.ui.pergaminho_pedido = TZ_NENHUM;
				else if (!tz_guiao_modal(&J.ab.guiao) || !J.ui.pergaminho_ao_chegar) J.ui.pergaminho_pedido = TZ_NENHUM;
			}
			if (J.com_guiao) pedidos();
			J.ui.scroll_bordes = rato_dentro && !getenv("TZAR_SEM_BORDES");   /* con el ratón fuera de la ventana, la vista no corre */
			tz_ui_tic(&J.ui, agora);
			chat_tic();
			tz_vigia_sair();
			if (som) {
				tz_vigia_entrar("sonido");
				tz_som_camara(som, (J.ui.camx + J.ui.mapa_rect.w / 2) / TZ_CASILLA, (J.ui.camy + J.ui.mapa_rect.h / 2) / TZ_CASILLA, J.ui.mapa_rect.w);
				tz_som_eventos(som, &J.m);
				tz_som_chuva(som, clima_chuva && clima_intensidade(J.ms) > 0, (uint32_t)J.ms);
				tz_musica_tic(tz_som_musica(som), agora);
				tz_vigia_sair();
			}
			int ganhou = 0;
			if (J.com_guiao && tz_guiao_acabou(&J.ab.guiao, &ganhou)) { J.acabou = 1; J.ganhou = ganhou; }
			else if (J.m.acabou) { J.acabou = 1; J.ganhou = J.m.ganhou; }
			if (J.acabou && !modal && !J.fim_visto) {
				J.fim_visto = 1;
				if (som) tz_musica_resultado(tz_som_musica(som), J.ganhou);
				registro("[TZAR] fin de la partida: %s (tic %u)", J.ganhou ? "victoria" : "derrota", J.m.tic);
				fim_mostrar();
			}
		}
		if (parada_ms > 0 && n == parada_q) {
			tz_vigia_entrar("prueba de parada");
			Uint32 t0 = SDL_GetTicks();
			while (SDL_GetTicks() - t0 < (Uint32)parada_ms) SDL_Delay(50);
			tz_vigia_sair();
		}
		/* dibujo */
		tz_vigia_entrar("dibujo");
		if (estado == E_MENU) {
			/* las ventanas sobre la partida (menú de la partida, fin, instrucciones) dejan verla debajo */
			if (J.ativa) desenhar_jogo();
			tz_menu_desenhar(&menu, &tela);
		} else if (estado == E_JOGO && J.ativa) desenhar_jogo();
		/* el cursor del juego, encima de todo */
		if (op.cursor_jogo && rato_dentro) tz_ui_cursor_desenhar(&skin, &tela, estado == E_JOGO && J.ativa && !(J.com_guiao && tz_guiao_modal(&J.ab.guiao)) ? tz_ui_cursor_forma(&J.ui) : "", rato_x, rato_y);
		tz_vigia_sair();
		tz_vigia_entrar("presentar");
		presentar();
		tz_vigia_sair();
		fps_n++;
		if (agora - fps_t0 >= 5000) {
			if (estado == E_JOGO) registro("[TZAR] %.1f cuadros por segundo (tic %u, %d objetos)", fps_n * 1000.0 / (agora - fps_t0), J.m.tic, J.m.nobj);
			fps_t0 = agora; fps_n = 0;
		}
		if (shot && frames && n + 1 == frames) { captura(shot); registro("[TZAR] captura guardada en el cuadro %d", n + 1); }
		Uint32 gasto = SDL_GetTicks() - agora;
		if (gasto < 16) SDL_Delay(16 - gasto);
	}
	if (J.ativa) registro("[TZAR] partida: tic %u suma %08x objetos %d", J.m.tic, tz_mundo_suma(&J.m), J.m.nobj);
	tz_vigia_terminar();
	if (tz_vigia_avisos()) registro("[TZAR] vigilante: %d avisos", tz_vigia_avisos());
#ifdef SAGUI_TZAR
	if (sem_molestar) no_molestar(0);
#endif
	jogo_fechar();
	previa_soltar();
	tz_menu_liberar(&menu);
	if (som) tz_som_fechar(som);
	if (tem_bib_som) tz_biblioteca_liberar(&bib_som);
	tz_ui_cursor_liberar();
	tz_ui_skin_liberar(&skin);
	tz_mapas_liberar(&mapas);
	tz_progresso_liberar(&progresso);
	tz_recursos_fechar(&rec);
	tz_tela_liberar(&tela);
	if (fbs) SDL_FreeSurface(fbs);
	SDL_Quit();
	registro("[TZAR] fin");
	return 0;
}

#ifdef SAGUI_TZAR
/* SaguiOS empieza los programas en principal(); la SDL del sistema no necesita más. */
int principal(int argc, char **argv) { return main(argc, argv); }
#endif
