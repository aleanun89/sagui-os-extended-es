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

/* ------------------------------------------------------------------ pantalla */
static SDL_Window *win;
static SDL_Surface *fbs;
static TzTela tela;
static int W, H;

static void presentar(void)
{
	uint32_t *d = fbs->pixels;
	int pitch = fbs->pitch / 4;
	for (int y = 0; y < H; ++y) {
		const uint32_t *s = tela.px + (size_t)y * W;
		for (int x = 0; x < W; ++x) d[(size_t)y * pitch + x] = s[x] & 0xffffff;
	}
	SDL_Surface *ws = SDL_GetWindowSurface(win);
	if (ws) { SDL_BlitSurface(fbs, NULL, ws, NULL); SDL_UpdateWindowSurface(win); }
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

enum { E_MENU, E_JOGO, E_RESULTADO, E_SAIR };
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
	int ganhou, acabou;
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
static int jogo_montar(const TzRegras *regras, uint32_t semente)
{
	TzMapa *mp = J.mapa;
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

static void jogo_ui(void)
{
	tz_ui_iniciar(&J.ui, &skin, &J.m, &J.bib, W, H);
	J.ui.minimapa = J.minimapa;
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
	if (som) {
		if (J.tipo != 1) tz_som_fonte_missao(som, J.ab.tem_sub ? &J.ab.sub : &J.ab.c);
		tz_musica_jogo(tz_som_musica(som));
	}
	J.ativa = 1;
	estado = E_JOGO;
}

/* escaramuza: un mapa suelto (o de campaña) sin guion, castillo y tres peones por reino, la IA en los demás */
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
		static const char *const biomas[4] = { "grass", "summer", "snow", "desert" };
		snprintf(op.bioma, sizeof op.bioma, "%s", biomas[op.semente % 4]);
		/* el catálogo da los tamaños de castillos y minas: se monta uno de paso con el bioma del mapa */
		TzBiblioteca b; TzCatalogo c;
		if (tz_biblioteca_carregar(&b, &rec, op.bioma)) return -1;
		if (tz_catalogo_carregar(&c, &rec, &b)) { tz_biblioteca_liberar(&b); return -1; }
		int r = tz_mapa_aleatorio(&J.mapa_aleat, &rec, &c, &op);
		tz_catalogo_liberar(&c); tz_biblioteca_liberar(&b);
		if (r) return -1;
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
	fclose(f);
	if (tz_mundo_gravar(&J.m, r)) return -1;
	registro("[TZAR] partida guardada: %s", r);
	return 0;
}

static int carregar(const char *nome)
{
	char r[800], ri[800], l[6][320];
	ruta_guardada(nome, "tzs", r, sizeof r);
	ruta_guardada(nome, "txt", ri, sizeof ri);
	FILE *f = fopen(ri, "r");
	if (!f) return tz_falha("no existe la partida %s", nome);
	int n = 0;
	while (n < 6 && fgets(l[n], sizeof l[n], f)) { l[n][strcspn(l[n], "\r\n")] = 0; ++n; }
	fclose(f);
	if (n < 6 || strcmp(l[0], "TZSAVE1")) return tz_falha("partida no válida");
	int tipo = atoi(l[1]), ci = atoi(l[2]), mi = atoi(l[3]);
	if (tipo == 1) return tz_falha("las partidas en mapas aleatorios no se pueden cargar todavía");
	if (tipo == 2) { if (jogar_missao(ci, mi, NULL)) return -1; }
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
	}
	menu.nmapas = mapas.nsoltas;
	menu.mapas = calloc((size_t)(mapas.nsoltas ? mapas.nsoltas : 1), sizeof *menu.mapas);
	for (int i = 0; i < mapas.nsoltas && menu.mapas; ++i) {
		TzMnItem *it = &menu.mapas[i];
		snprintf(it->titulo, sizeof it->titulo, "%s", mapas.soltas[i].titulo);
		snprintf(it->fich, sizeof it->fich, "%s", mapas.soltas[i].fich);
		it->jogadores_min = mapas.soltas[i].jogadores[0]; it->jogadores_max = mapas.soltas[i].jogadores[1];
	}
	listar_guardadas();
}

/* ------------------------------------------------------------------ entrada */
static int traduzir(const SDL_Event *e, TzUiEntrada *o)
{
	memset(o, 0, sizeof *o);
	SDL_Keymod km = SDL_GetModState();
	o->mods = (km & KMOD_SHIFT ? TZ_MOD_SHIFT : 0) | (km & KMOD_CTRL ? TZ_MOD_CTRL : 0) | (km & KMOD_ALT ? TZ_MOD_ALT : 0);
	switch (e->type) {
	case SDL_MOUSEMOTION: o->tipo = TZ_IN_MOVER; o->x = e->motion.x; o->y = e->motion.y; return 1;
	case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
		o->tipo = e->type == SDL_MOUSEBUTTONDOWN ? TZ_IN_BAIXO : TZ_IN_CIMA;
		o->x = e->button.x; o->y = e->button.y;
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

static void modal_entrada(const TzUiEntrada *e)
{
	const TzMensagem *msg = tz_guiao_mensagem(&J.ab.guiao);
	int n = msg ? msg->nescolhas : 0;
	if (e->tipo == TZ_IN_BAIXO && e->botao == 1) {
		for (int i = 0; i < n && i < 6; ++i) if (tz_rect_dentro(rect_escolha(i), e->x, e->y)) { tz_guiao_responder(&J.m, &J.ab.guiao, i); return; }
		if (!n) tz_guiao_fechar(&J.m, &J.ab.guiao);
	} else if (e->tipo == TZ_IN_TECLA) {
		if (n && e->tecla >= '1' && e->tecla < '1' + n) tz_guiao_responder(&J.m, &J.ab.guiao, e->tecla - '1');
		else if (!n && (e->tecla == TZ_TC_ENTER || e->tecla == ' ' || e->tecla == TZ_TC_ESC)) tz_guiao_fechar(&J.m, &J.ab.guiao);
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

/* ------------------------------------------------------------------ resultado */
static void desenhar_resultado(void)
{
	tz_tela_preencher(&tela, (TzRect){ 0, 0, W, H }, 0xff100c08);
	const char *t = J.ganhou ? tz_ui_txt(&skin, "msg", "victory", "¡Victoria!") : tz_ui_txt(&skin, "msg", "defeat", "Derrota");
	tz_ui_fonte_caixa(&tela, &skin.f22, (TzRect){ 0, H / 4, W, 40 }, t, J.ganhou ? 0xf0d060 : 0xd06040, 1);
	char b[200];
	int s = (int)(J.m.tic / TZ_TICS_SEG);
	int y = H / 4 + 70;
	snprintf(b, sizeof b, "Tiempo de juego: %d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
	tz_ui_fonte_caixa(&tela, &skin.f14b, (TzRect){ 0, y, W, 24 }, b, 0xe0d8c0, 1);
	for (int k = 1; k <= 8; ++k) {
		const TzReinoEstado *r = &J.m.reino[k];
		if (!r->ativo) continue;
		y += 26;
		snprintf(b, sizeof b, "%s%s: creadas %u, perdidas %u, matadas %u, recogido %u, puntos %u%s", r->nome[0] ? r->nome : "Reino",
		         k == J.m.jogador ? " (tú)" : "", r->criadas, r->perdidas, r->matadas,
		         r->recolhido[0] + r->recolhido[1] + r->recolhido[2] + r->recolhido[3], (r->pontos + 50) / 100, r->derrotado ? " - derrotado" : "");
		tz_ui_fonte_caixa(&tela, &skin.f12b, (TzRect){ 0, y, W, 20 }, b, J.m.obj && J.m.reino[k].cor < 8 ? skin.cores[J.m.reino[k].cor] | 0xff000000u : 0xffffffff, 1);
	}
	const char *seg = J.tipo == 2 && J.ganhou && J.com_guiao && *tz_guiao_proximo_mapa(&J.ab.guiao) ? "Intro: siguiente misión" : "Intro: volver al menú";
	tz_ui_fonte_caixa(&tela, &skin.f14b, (TzRect){ 0, H - 80, W, 24 }, seg, 0xc0b080, 1);
}

static void resultado_seguir(void)
{
	if (J.tipo == 2 && J.ganhou && J.com_guiao && *tz_guiao_proximo_mapa(&J.ab.guiao)) {
		int ci = J.camp;
		int prox = tz_campanha_achar(&mapas.camp[ci], tz_guiao_proximo_mapa(&J.ab.guiao));
		static TzLevar lv;
		tz_levar_de(&J.m, &J.ab.guiao, &lv);
		if (prox >= 0) {
			tz_progresso_marcar(&progresso, mapas.camp[ci].missoes[prox].id, ruta_progresso);
			if (!jogar_missao(ci, prox, &lv)) return;
			registro("[TZAR] no se puede abrir la misión siguiente: %s", tz_erro());
		}
	}
	jogo_fechar();
	estado = E_MENU;
	tz_menu_ir(&menu, TZ_MN_PRINCIPAL);
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
	if (som) tz_som_volumes(som, menu.volume_som * 256 / 100, menu.volume_som * 256 / 100, menu.volume_musica * 256 / 100);
}

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "--visor")) return tz_visor_main(argc - 1, argv + 1);
	W = getenv("TZAR_W") ? atoi(getenv("TZAR_W")) : 1024; H = getenv("TZAR_H") ? atoi(getenv("TZAR_H")) : 768;
	if (W < 800) W = 800;
	if (H < 600) H = 600;
	if (SDL_Init(SDL_INIT_VIDEO)) { registro("[TZAR] SDL: %s", SDL_GetError()); return 1; }
	/* la ventana cabe en la pantalla (con su marco y la barra de tareas); si no, 800x600, lo mínimo de la interfaz */
	SDL_DisplayMode dm;
	if (!getenv("TZAR_W") && !SDL_GetDesktopDisplayMode(0, &dm) && dm.w > 0 && (W > dm.w - 16 || H > dm.h - 64)) { W = 800; H = 600; }
	win = SDL_CreateWindow("Tzar", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, 0);
	if (!win && (W != 800 || H != 600)) { W = 800; H = 600; win = SDL_CreateWindow("Tzar", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, 0); }
	fbs = win ? SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_RGB888) : NULL;
	if (!win || !fbs || tz_tela_criar(&tela, W, H)) { registro("[TZAR] SDL: no se puede abrir la ventana %dx%d: %s", W, H, SDL_GetError()); return 1; }
	const char *shot = getenv("TZAR_SHOT");
	int frames = getenv("TZAR_FRAMES") ? atoi(getenv("TZAR_FRAMES")) : 0;
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
	registro("[TZAR] juego: ventana %dx%d, datos %s", W, H, *motivo ? "no" : raiz);
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
			if (shot && n + 1 == frames) SDL_SaveBMP(fbs, shot);
			SDL_Delay(30);
		}
		SDL_Quit();
		return 1;
	}
	/* dónde se guarda */
	const char *g = getenv("TZAR_GUARDAR"), *home = getenv("HOME");
	if (g && *g) snprintf(pasta_guardar, sizeof pasta_guardar, "%s", g);
	else if (home && *home) snprintf(pasta_guardar, sizeof pasta_guardar, "%s/.tzar", home);
	else snprintf(pasta_guardar, sizeof pasta_guardar, "%s/guardadas", raiz);
	mkdir(pasta_guardar, 0755);
	snprintf(ruta_progresso, sizeof ruta_progresso, "%s/progreso.txt", pasta_guardar);
	tz_progresso_ler(&progresso, ruta_progresso);
	/* sonido */
	if (!getenv("TZAR_SEM_SOM")) {
		tem_bib_som = !tz_biblioteca_carregar(&bib_som, &rec, "grass");
		som = tz_som_criar(&rec, tem_bib_som ? &bib_som : NULL, 1);
	}
	tz_menu_iniciar(&menu, &skin, W, H);
	preparar_menu();
	audio_volumes();
	registro("[TZAR] instalacion: %d paquetes, %d mapas sueltos, %d campanas, sonido %s, guardar en %s", rec.npac, mapas.nsoltas, mapas.ncamp,
	         som && tz_som_tem_dispositivo(som) ? "si" : "no", pasta_guardar);
	/* introducción */
	if (!getenv("TZAR_SEM_INTRO") && !frames) {
		TzIntro *v = tz_intro_abrir(raiz);
		if (v) {
			int vw, vh, fps;
			tz_intro_tamanho(v, &vw, &vh, &fps);
			uint32_t *px = malloc((size_t)vw * vh * 4);
			Uint32 t0 = SDL_GetTicks();
			for (int q = 0, fim = 0; px && !fim; ++q) {
				SDL_Event e;
				while (SDL_PollEvent(&e)) if (e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_QUIT) fim = 1;
				int r = tz_intro_quadro(v, px, som);
				if (r == 1) break;
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
			free(px);
			tz_intro_fechar(v);
		}
	}
	if (som) tz_musica_menu(tz_som_musica(som));
	/* entrar solo en una partida (pruebas) */
	const char *aut = getenv("TZAR_AUTO");
	if (aut && !strcmp(aut, "escaramuza") && mapas.nsoltas) { if (jogar_mapa(&mapas.soltas[0], &menu.cfg, 0)) registro("[TZAR] escaramuza: %s", tz_erro()); }
	else if (aut && !strcmp(aut, "aleatorio")) { if (jogar_mapa(NULL, &menu.cfg, 1)) registro("[TZAR] mapa aleatorio: %s", tz_erro()); }
	else if (aut && !strncmp(aut, "campanha", 8) && mapas.ncamp) {
		int ci = aut[8] == ':' ? atoi(aut + 9) : 0;
		if (ci < 0 || ci >= mapas.ncamp) ci = 0;
		if (jogar_missao(ci, missao_atual(ci), NULL)) registro("[TZAR] campaña: %s", tz_erro());
	}
	Uint32 ultimo = SDL_GetTicks(), fps_t0 = ultimo;
	int fps_n = 0;
	for (int n = 0; estado != E_SAIR && (!frames || n < frames); ++n) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_QUIT) { estado = E_SAIR; break; }
			TzUiEntrada e;
			if (!traduzir(&ev, &e)) continue;
			Uint32 agora = SDL_GetTicks();
			if (estado == E_MENU) tz_menu_entrada(&menu, &e);
			else if (estado == E_RESULTADO) { if ((e.tipo == TZ_IN_TECLA && (e.tecla == TZ_TC_ENTER || e.tecla == TZ_TC_ESC || e.tecla == ' ')) || (e.tipo == TZ_IN_BAIXO)) resultado_seguir(); }
			else if (estado == E_JOGO) {
				if (J.com_guiao && tz_guiao_modal(&J.ab.guiao)) modal_entrada(&e);
				else if (e.tipo == TZ_IN_TECLA && e.tecla == '+') { if (J.velocidade < 40) J.velocidade += 5; }
				else if (e.tipo == TZ_IN_TECLA && e.tecla == '-') { if (J.velocidade > 5) J.velocidade -= 5; }
				else if (e.tipo == TZ_IN_TECLA && e.tecla == TZ_TC_F1 + 4) { if (guardar("rapida")) registro("[TZAR] guardar: %s", tz_erro()); else tz_ui_aviso(&J.ui, "Partida guardada"); }
				else if (e.tipo == TZ_IN_TECLA && e.tecla == TZ_TC_F1 + 8) { if (carregar("rapida")) registro("[TZAR] cargar: %s", tz_erro()); }
				else tz_ui_entrada(&J.ui, &e, agora);
			}
		}
		if (estado == E_SAIR) break;
		Uint32 agora = SDL_GetTicks();
		int dt = (int)(agora - ultimo);
		ultimo = agora;
		if (dt > 200) dt = 200;
		if (frames) dt = TZ_TIC_MS;   /* las pruebas: un tic por cuadro */
		/* el menú: sus acciones */
		if (estado == E_MENU) {
			audio_volumes();
			int a = menu.acao;
			menu.acao = 0;
			if (a == TZ_MR_SAIR) { estado = E_SAIR; break; }
			if (a == TZ_MR_JOGAR) {
				int i = menu.res_indice;
				if (jogar_mapa(i >= 0 && i < mapas.nsoltas ? &mapas.soltas[i] : NULL, &menu.res_cfg, i < 0 || i >= mapas.nsoltas))
					registro("[TZAR] no se puede jugar: %s", tz_erro());
			} else if (a == TZ_MR_CAMPANHA && menu.res_indice >= 0 && menu.res_indice < mapas.ncamp) {
				if (jogar_missao(menu.res_indice, missao_atual(menu.res_indice), NULL)) registro("[TZAR] campaña: %s", tz_erro());
			} else if (a == TZ_MR_CARREGAR && menu.res_indice >= 0 && menu.res_indice < menu.nguardados) {
				if (carregar(menu.guardados[menu.res_indice].nome)) registro("[TZAR] cargar: %s", tz_erro());
			} else if (a == TZ_MR_GUARDAR && J.ativa) {
				if (guardar(menu.res_nome)) registro("[TZAR] guardar: %s", tz_erro());
				listar_guardadas();
				estado = E_JOGO;
			} else if (a == TZ_MR_CONTINUAR && J.ativa) estado = E_JOGO;
			else if (a == TZ_MR_REINICIAR && J.ativa) { jogo_fechar(); tz_menu_ir(&menu, TZ_MN_PRINCIPAL); }
			if (!J.ativa && estado == E_JOGO) estado = E_MENU;
		}
		/* la partida */
		if (estado == E_JOGO && J.ativa) {
			if (J.ui.pedir_menu) { J.ui.pedir_menu = 0; listar_guardadas(); estado = E_MENU; tz_menu_ir(&menu, TZ_MN_PARTIDA); }
			else if (J.ui.pedir_sair) { J.ui.pedir_sair = 0; jogo_fechar(); estado = E_MENU; tz_menu_ir(&menu, TZ_MN_PRINCIPAL); }
		}
		if (estado == E_JOGO && J.ativa) {
			int modal = J.com_guiao && tz_guiao_modal(&J.ab.guiao);
			if (!modal && !J.ui.pausa) {
				J.acum += dt * J.velocidade;
				int t = 0;
				while (J.acum >= TZ_TIC_MS * 10 && t < 6) { tz_mundo_tic(&J.m); J.acum -= TZ_TIC_MS * 10; ++t; J.ms += TZ_TIC_MS; }
				if (t == 6) J.acum = 0;
				if (t) memcpy(J.mapa->casillas, J.m.casillas, (size_t)J.m.lado * J.m.lado * sizeof *J.mapa->casillas);
			}
			if (J.com_guiao) pedidos();
			tz_ui_tic(&J.ui, agora);
			if (som) {
				tz_som_camara(som, (J.ui.camx + J.ui.mapa_rect.w / 2) / TZ_CASILLA, (J.ui.camy + J.ui.mapa_rect.h / 2) / TZ_CASILLA, J.ui.mapa_rect.w);
				tz_som_eventos(som, &J.m);
				tz_musica_tic(tz_som_musica(som), agora);
			}
			int ganhou = 0;
			if (J.com_guiao && tz_guiao_acabou(&J.ab.guiao, &ganhou)) { J.acabou = 1; J.ganhou = ganhou; }
			else if (J.m.acabou) { J.acabou = 1; J.ganhou = J.m.ganhou; }
			if (J.acabou && !modal) {
				estado = E_RESULTADO;
				if (som) tz_musica_resultado(tz_som_musica(som), J.ganhou);
				registro("[TZAR] fin de la partida: %s (tic %u)", J.ganhou ? "victoria" : "derrota", J.m.tic);
			}
		}
		/* dibujo */
		if (estado == E_MENU) tz_menu_desenhar(&menu, &tela);
		else if (estado == E_RESULTADO) desenhar_resultado();
		else if (estado == E_JOGO && J.ativa) {
			TzUiMapa mp = { &J.ter, J.mapa, &J.esc, J.ms };
			tz_animar(&J.esc, J.ms);
			tz_ui_mapa_desenhar(&J.ui, &tela, &mp);
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
		presentar();
		fps_n++;
		if (agora - fps_t0 >= 5000) {
			if (estado == E_JOGO) registro("[TZAR] %.1f cuadros por segundo (tic %u, %d objetos)", fps_n * 1000.0 / (agora - fps_t0), J.m.tic, J.m.nobj);
			fps_t0 = agora; fps_n = 0;
		}
		if (shot && frames && n + 1 == frames) { SDL_SaveBMP(fbs, shot); registro("[TZAR] captura guardada en el cuadro %d", n + 1); }
		Uint32 gasto = SDL_GetTicks() - agora;
		if (gasto < 16) SDL_Delay(16 - gasto);
	}
	if (J.ativa) registro("[TZAR] partida: tic %u suma %08x objetos %d", J.m.tic, tz_mundo_suma(&J.m), J.m.nobj);
	jogo_fechar();
	tz_menu_liberar(&menu);
	if (som) tz_som_fechar(som);
	if (tem_bib_som) tz_biblioteca_liberar(&bib_som);
	tz_ui_skin_liberar(&skin);
	tz_mapas_liberar(&mapas);
	tz_progresso_liberar(&progresso);
	tz_recursos_fechar(&rec);
	SDL_Quit();
	registro("[TZAR] fin");
	return 0;
}

#ifdef SAGUI_TZAR
/* SaguiOS empieza los programas en principal(); la SDL del sistema no necesita más. */
int principal(int argc, char **argv) { return main(argc, argv); }
#endif
