/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ui_menu.c - menús principales de Tzar (D417, fase 4): principal, un jugador, campañas, misiones, escaramuza con
 * sus opciones, cargar y guardar, opciones, créditos, salir y el menú de dentro de la partida.
 * Traducción a C de menu.rs de ReTzar (GPL-3.0-or-later): los rectángulos son los del Tzar.exe original sobre un
 * lienzo de 800x600 (centrado en la ventana), los fondos BACK.BMP y las tiras BTN.BMP salen de los datos y los
 * textos de STRINGS.INI (lo que va tras «|» es la ayuda y no se muestra en el botón). Los menús no abren mapas
 * ni ficheros: devuelven una acción (TzMenu.acao) con la elección y el llamador la atiende. */
#include "ui.h"
#include <stdlib.h>
#include <string.h>

#define ROW 16
#define SAVE_ROWS 9
#define MAXW 96

enum { E_BOTAO, E_ITEM, E_SPIN, E_CHECK, E_TEXTO, E_AREA, E_NOME };
enum {
	A_NADA, A_IR, A_VOLTAR, A_CAMPANHA, A_MAPA, A_GUARDADO, A_JOGAR, A_CONFIG, A_SPIN, A_CHECK, A_SIM_SAIR, A_CONTINUAR,
	A_REINICIAR, A_CARREGAR, A_GUARDAR, A_NOME, A_CAMPANHA_JOGAR, A_ABRIR_MAPA,
};
enum {   /* campos de los spins */
	C_JOG, C_TIPO, C_VITORIA, C_POP, C_DIF, C_REC, C_RACA, C_VOL_SOM, C_VOL_MUS, C_VELOC, C_DIF_CAMP,
};

typedef struct {
	TzRect r;
	char rot[96];
	int est, acao, arg;
	int ok, marcado;
	const char *art;       /* tira BTN.BMP bajo IMAGES/SCREENS/ (con 2 o 4 estados) */
	int estados;
} Wd;

static const char *txt(const TzMenu *m, const char *sec, const char *k, const char *def, char *buf, size_t n)
{
	const char *v = tz_ui_txt(m->skin, sec, k, def);
	size_t l = strcspn(v, "|");
	if (l >= n) l = n - 1;
	memcpy(buf, v, l);
	while (l && buf[l - 1] == ' ') --l;
	buf[l] = 0;
	return buf;
}

static void wd_add(Wd *w, int *n, TzRect r, const char *rot, int est, int acao, int arg)
{
	if (*n >= MAXW) return;
	Wd *x = &w[(*n)++];
	memset(x, 0, sizeof *x);
	x->r = r; x->est = est; x->acao = acao; x->arg = arg; x->ok = 1; x->marcado = -1;
	snprintf(x->rot, sizeof x->rot, "%s", rot);
}

static Wd *ultimo(Wd *w, int n) { return &w[n - 1]; }

static void wd_boton(Wd *w, int *n, TzRect r, const char *rot, int acao, int arg, const char *art, int estados, int ok)
{
	wd_add(w, n, r, rot, E_BOTAO, acao, arg);
	Wd *x = ultimo(w, *n);
	x->art = art; x->estados = estados; x->ok = ok;
}

static void wd_spin(Wd *w, int *n, TzRect r, const char *valor, int campo, int ok, int idx)
{
	wd_add(w, n, (TzRect){ r.x, r.y, 27, r.h }, "<", E_SPIN, A_SPIN, (campo << 8) | (idx << 1));
	ultimo(w, *n)->ok = ok;
	wd_add(w, n, (TzRect){ r.x + 27, r.y, r.w - 54, r.h }, valor, E_TEXTO, A_NADA, 2);   /* 2 = centrado */
	wd_add(w, n, (TzRect){ r.x + r.w - 27, r.y, 27, r.h }, ">", E_SPIN, A_SPIN, (campo << 8) | (idx << 1) | 1);
	ultimo(w, *n)->ok = ok;
}

static int dialogo_tamanho(int tela, int *w, int *h)
{
	switch (tela) {
	case TZ_MN_PARTIDA: *w = 291; *h = 413; return 1;
	case TZ_MN_SAIR: *w = 500; *h = 320; return 1;
	case TZ_MN_CARREGAR: case TZ_MN_GUARDAR: *w = 550; *h = 320; return 1;
	default: *w = 800; *h = 600; return 0;
	}
}

/* origen del lienzo de la pantalla en la ventana */
static void origem(const TzMenu *m, int *ox, int *oy)
{
	int w, h;
	dialogo_tamanho(m->tela, &w, &h);
	*ox = (m->w - w) / 2; *oy = (m->h - h) / 2;
	/* cargar y guardar van sobre la pantalla de 800x600 en (125,140): su origen es el del lienzo grande */
	if (m->tela == TZ_MN_CARREGAR || m->tela == TZ_MN_GUARDAR) { *ox = (m->w - 800) / 2 + 125; *oy = (m->h - 600) / 2 + 140; }
}

static int tela_cheia(int tela)
{
	return tela != TZ_MN_PARTIDA && tela != TZ_MN_SAIR && tela != TZ_MN_CARREGAR && tela != TZ_MN_GUARDAR;
}

static const char *const nomes_tela[TZ_MN_NTELAS] = {
	"S_MAIN", "S_SINGLEPLAYER", "S_CAMPAIGN", "S_SINGLECHOOSEMAP", "S_SINGLESETUP", "D_LOAD", "D_SAVE", "D_OPTIONS", "S_CREDITS",
	"D_MSGWND", "D_MENU",
};

static const int pops[] = { 50, 75, 100, 150, 200, 250, 300, 400, 500 };

static void valor_cfg(const TzMenu *m, int campo, int idx, char *b, size_t n)
{
	const TzMnConfig *c = &m->cfg;
	char t[64];
	switch (campo) {
	case C_JOG: snprintf(b, n, "%d", c->jogadores); break;
	case C_TIPO: snprintf(b, n, "%s", txt(m, "sp_setupmap", (char[]){ 'g', '_', 't', 'y', 'p', 'e', '_', (char)('0' + c->tipo), 0 }, "", t, sizeof t)); break;
	case C_VITORIA: snprintf(b, n, "%s", txt(m, "sp_setupmap", (char[]){ 'v', 'c', 'o', 'n', 'd', '_', (char)('0' + c->vitoria), 0 }, "", t, sizeof t)); break;
	case C_POP: snprintf(b, n, "%d", c->populacao); break;
	case C_DIF: case C_DIF_CAMP: snprintf(b, n, "%s", txt(m, "sp_setupmap", (char[]){ 'd', 'i', 'f', 'f', 'l', 'v', 'l', '_', (char)('0' + c->dificuldade), 0 }, "", t, sizeof t)); break;
	case C_REC: snprintf(b, n, "%s", txt(m, "sp_setupmap", (char[]){ 'r', 'e', 's', 'u', 'r', 'c', 's', '_', (char)('0' + c->recursos), 0 }, "", t, sizeof t)); break;
	case C_RACA: snprintf(b, n, "%s", txt(m, "sp_setupmap", (char[]){ 'r', 'a', 'c', 'e', '_', (char)('0' + c->raca[idx & 7]), 0 }, "", t, sizeof t)); break;
	case C_VOL_SOM: snprintf(b, n, "%d", m->volume_som); break;
	case C_VOL_MUS: snprintf(b, n, "%d", m->volume_musica); break;
	default: b[0] = 0;
	}
}

static int escolher_pop(int atual, int d)
{
	int n = (int)(sizeof pops / sizeof *pops), k = 0;
	for (int i = 0; i < n; ++i) if (pops[i] <= atual) k = i;
	k += d;
	return pops[k < 0 ? 0 : k >= n ? n - 1 : k];
}

static void mudar(TzMenu *m, int campo, int idx, int d)
{
	TzMnConfig *c = &m->cfg;
	int maxj = 8, minj = 2;
	if (m->mapa_sel >= 0 && m->mapa_sel < m->nmapas) { maxj = m->mapas[m->mapa_sel].jogadores_max; minj = m->mapas[m->mapa_sel].jogadores_min; }
	if (maxj < 2) maxj = 2;
	if (minj < 2) minj = 2;
#define CICLO(v, n) ((v) = (((v) + d) % (n) + (n)) % (n))
	switch (campo) {
	case C_JOG: c->jogadores += d; if (c->jogadores < minj) c->jogadores = minj; if (c->jogadores > maxj) c->jogadores = maxj; if (c->jogadores > 8) c->jogadores = 8; break;
	case C_TIPO: CICLO(c->tipo, 3); break;
	case C_VITORIA: CICLO(c->vitoria, 4); break;
	case C_POP: c->populacao = escolher_pop(c->populacao, d); break;
	case C_DIF: case C_DIF_CAMP: CICLO(c->dificuldade, 3); break;
	case C_REC: CICLO(c->recursos, 4); break;
	case C_RACA: CICLO(c->raca[idx & 7], 4); break;
	case C_VOL_SOM: m->volume_som += 10 * d; if (m->volume_som < 0) m->volume_som = 0; if (m->volume_som > 100) m->volume_som = 100; break;
	case C_VOL_MUS: m->volume_musica += 10 * d; if (m->volume_musica < 0) m->volume_musica = 0; if (m->volume_musica > 100) m->volume_musica = 100; break;
	default: break;
	}
#undef CICLO
}

/* los widgets de la pantalla actual, en coordenadas de su lienzo */
static int construir(TzMenu *m, Wd *w)
{
	int n = 0;
	char t[96], t2[96];
	switch (m->tela) {
	case TZ_MN_PRINCIPAL: {
		static const struct { const char *k; const char *def; int tela; } it[5] = {
			{ "single", "UN SOLO JUGADOR", TZ_MN_UM }, { "multi", "VARIOS JUGADORES", TZ_MN_PRINCIPAL }, { "pref", "OPCIONES", TZ_MN_OPCOES },
			{ "credits", "CRÉDITOS", TZ_MN_CREDITOS }, { "quit", "SALIR", TZ_MN_SAIR } };
		for (int i = 0; i < 5; ++i)
			wd_boton(w, &n, (TzRect){ 270, 277 + 60 * i, 235, 25 }, txt(m, "mainscr", it[i].k, it[i].def, t, sizeof t), i == 1 ? A_NADA : A_IR, it[i].tela,
			         "SCREENS/S_MAIN/BTN", 2, i != 1);   /* sin partida en red */
		break;
	}
	case TZ_MN_UM: {
		static const struct { const char *k, *def; int y, tela; } it[5] = {
			{ "campaign", "CAMPAÑA", 141, TZ_MN_CAMPANHAS }, { "custom", "MISIÓN", 233, TZ_MN_MAPAS }, { "random", "MAPA ALEATORIO", 329, TZ_MN_CONFIG },
			{ "load", "CARGAR PARTIDA", 425, TZ_MN_CARREGAR }, { "back", "VOLVER", 521, TZ_MN_PRINCIPAL } };
		for (int i = 0; i < 5; ++i)
			wd_boton(w, &n, (TzRect){ 299, it[i].y, 235, 25 }, txt(m, "singlescr", it[i].k, it[i].def, t, sizeof t), i == 4 ? A_VOLTAR : A_IR, it[i].tela,
			         "SCREENS/S_SINGLEPLAYER/BTN", 2, 1);
		break;
	}
	case TZ_MN_CAMPANHAS: {
		for (int i = 0; i < m->ncampanhas && i < 15; ++i) {
			wd_add(w, &n, (TzRect){ 80, 43 + i * ROW, 251, ROW }, m->campanhas[i].titulo, E_ITEM, A_CAMPANHA, i);
			ultimo(w, n)->marcado = i == m->campanha_sel;
		}
		wd_spin(w, &n, (TzRect){ 332, 542, 180, 27 }, "", C_DIF_CAMP, 1, 0);
		valor_cfg(m, C_DIF_CAMP, 0, w[n - 2].rot, sizeof w[0].rot);
		wd_boton(w, &n, (TzRect){ 78, 545, 235, 25 }, txt(m, "campaign", "play", "JUGAR", t, sizeof t), A_CAMPANHA_JOGAR, 0, "SCREENS/S_CAMPAIGN/BTN", 2, m->ncampanhas > 0);
		wd_boton(w, &n, (TzRect){ 535, 544, 235, 25 }, txt(m, "campaign", "back", "VOLVER", t, sizeof t), A_VOLTAR, 0, "SCREENS/S_CAMPAIGN/BTN", 2, 1);
		break;
	}
	case TZ_MN_MAPAS: {
		for (int i = 0; i < 15 && i + m->rolo < m->nmapas; ++i) {
			const TzMnItem *it = &m->mapas[i + m->rolo];
			snprintf(t, sizeof t, "(%d - %d)  %s", it->jogadores_min, it->jogadores_max, it->titulo);
			wd_add(w, &n, (TzRect){ 79, 41 + i * ROW, 251, ROW }, t, E_ITEM, A_MAPA, i + m->rolo);
			ultimo(w, n)->marcado = i + m->rolo == m->mapa_sel;
		}
		wd_boton(w, &n, (TzRect){ 77, 544, 235, 25 }, txt(m, "sp_choosemap", "play", "JUGAR", t, sizeof t), A_IR, TZ_MN_CONFIG, "SCREENS/S_SINGLECHOOSEMAP/BTN", 2, m->nmapas > 0);
		wd_boton(w, &n, (TzRect){ 534, 543, 235, 25 }, txt(m, "sp_choosemap", "back", "VOLVER", t, sizeof t), A_VOLTAR, 0, "SCREENS/S_SINGLECHOOSEMAP/BTN", 2, 1);
		break;
	}
	case TZ_MN_CONFIG: {
		for (int i = 0; i < m->cfg.jogadores && i < 8; ++i) {
			int y = 35 * i;
			wd_add(w, &n, (TzRect){ 73, 31 + y, 13, 13 }, "", E_AREA, A_NADA, 0x100 + i);   /* muestra del color */
			if (i) snprintf(t, sizeof t, "IA %d", i); else snprintf(t, sizeof t, "%s", txt(m, "peons", "player", "Jugador 1", t2, sizeof t2));
			wd_add(w, &n, (TzRect){ 110, 30 + y, 182, 14 }, t, E_TEXTO, A_NADA, 0);
			wd_spin(w, &n, (TzRect){ 311, 23 + y, 180, 27 }, "", C_RACA, 1, i);
			valor_cfg(m, C_RACA, i, w[n - 2].rot, sizeof w[0].rot);
		}
		static const struct { int x, y, campo; } sp[6] = { { 66, 369, C_JOG }, { 66, 427, C_TIPO }, { 66, 487, C_VITORIA }, { 270, 369, C_POP }, { 270, 427, C_DIF }, { 270, 487, C_REC } };
		for (int i = 0; i < 6; ++i) {
			wd_spin(w, &n, (TzRect){ sp[i].x, sp[i].y, 180, 27 }, "", sp[i].campo, 1, 0);
			valor_cfg(m, sp[i].campo, 0, w[n - 2].rot, sizeof w[0].rot);
		}
		wd_add(w, &n, (TzRect){ 268, 539, 30, 30 }, txt(m, "sp_setupmap", "t_fog", "Sin niebla", t, sizeof t), E_CHECK, A_CHECK, 0);
		ultimo(w, n)->marcado = m->cfg.sem_nevoa;
		wd_add(w, &n, (TzRect){ 268, 569, 30, 30 }, txt(m, "sp_setupmap", "t_expolre", "Explorar", t, sizeof t), E_CHECK, A_CHECK, 1);
		ultimo(w, n)->marcado = m->cfg.explorado;
		wd_boton(w, &n, (TzRect){ 556, 499, 210, 27 }, txt(m, "sp_setupmap", "play", "JUGAR", t, sizeof t), A_JOGAR, 0, "SCREENS/S_SINGLESETUP/BTN", 2, 1);
		wd_boton(w, &n, (TzRect){ 556, 554, 210, 27 }, txt(m, "sp_setupmap", "back", "VOLVER", t, sizeof t), A_VOLTAR, 0, "SCREENS/S_SINGLESETUP/BTN", 2, 1);
		break;
	}
	case TZ_MN_CARREGAR: case TZ_MN_GUARDAR: {
		int carga = m->tela == TZ_MN_CARREGAR;
		const char *sec = carga ? "load" : "save", *art = carga ? "SCREENS/D_LOAD/BTN" : "SCREENS/D_SAVE/BTN";
		int primeiro = carga ? 0 : 1, total = m->nguardados + primeiro;
		if (!carga) wd_add(w, &n, (TzRect){ 121, 39, 276, 16 }, m->nome_guardar, E_NOME, A_NOME, 0);
		for (int row = 0; row < SAVE_ROWS; ++row) {
			int k = row + m->rolo;
			if (k >= total) break;
			if (k < primeiro) { wd_add(w, &n, (TzRect){ 44, 77 + row * ROW, 431, ROW }, txt(m, sec, "new", "NUEVA", t, sizeof t), E_ITEM, A_GUARDADO, -1); ultimo(w, n)->marcado = m->guardado_sel < 0; }
			else {
				const TzMnGuardado *g = &m->guardados[k - primeiro];
				snprintf(t, sizeof t, "%s   %s", g->nome, g->data);
				wd_add(w, &n, (TzRect){ 44, 77 + row * ROW, 431, ROW }, t, E_ITEM, A_GUARDADO, k - primeiro);
				ultimo(w, n)->marcado = m->guardado_sel == k - primeiro;
			}
		}
		int usado = m->guardado_sel >= 0 && m->guardado_sel < m->nguardados;
		wd_boton(w, &n, (TzRect){ 42, 253, 137, 27 }, txt(m, sec, sec, carga ? "CARGAR" : "GUARDAR", t, sizeof t), carga ? A_CARREGAR : A_GUARDAR, 0, art, 2, carga ? usado : 1);
		wd_boton(w, &n, (TzRect){ 370, 253, 137, 27 }, txt(m, sec, "cancel", "CANCELAR", t, sizeof t), A_VOLTAR, 0, art, 2, 1);
		break;
	}
	case TZ_MN_OPCOES: {
		static const struct { int x, y; const char *k, *def; } tit[4] = { { 92, 52, "t_sopt", "SONIDO" }, { 460, 52, "t_mopt", "MÚSICA" }, { 92, 300, "t_gopt", "OPCIONES DE JUEGO" }, { 460, 300, "t_uint", "ELEMENTOS DE PANTALLA" } };
		for (int i = 0; i < 4; ++i) wd_add(w, &n, (TzRect){ tit[i].x, tit[i].y, 300, 16 }, txt(m, "options", tit[i].k, tit[i].def, t, sizeof t), E_TEXTO, A_NADA, 1);
		wd_add(w, &n, (TzRect){ 125, 90, 95, 16 }, txt(m, "options", "t_svol", "Volumen", t, sizeof t), E_TEXTO, A_NADA, 0);
		wd_spin(w, &n, (TzRect){ 222, 82, 182, 27 }, "", C_VOL_SOM, 1, 0); valor_cfg(m, C_VOL_SOM, 0, w[n - 2].rot, sizeof w[0].rot);
		static const struct { int y; const char *k, *def; } ck[3] = { { 135, "t_usp", "Voces" }, { 180, "t_bs", "Sonido de edificios" }, { 225, "t_ns", "Sonido de la naturaleza" } };
		for (int i = 0; i < 3; ++i) { wd_add(w, &n, (TzRect){ 125, ck[i].y, 280, 16 }, txt(m, "options", ck[i].k, ck[i].def, t, sizeof t), E_TEXTO, A_NADA, 0); }
		wd_add(w, &n, (TzRect){ 492, 90, 55, 16 }, txt(m, "options", "t_mv", "Volumen", t, sizeof t), E_TEXTO, A_NADA, 0);
		wd_spin(w, &n, (TzRect){ 550, 82, 182, 27 }, "", C_VOL_MUS, 1, 0); valor_cfg(m, C_VOL_MUS, 0, w[n - 2].rot, sizeof w[0].rot);
		wd_add(w, &n, (TzRect){ 125, 338, 95, 16 }, txt(m, "options", "t_gs", "Velocidad", t, sizeof t), E_TEXTO, A_NADA, 0);
		wd_boton(w, &n, (TzRect){ 538, 541, 210, 27 }, txt(m, "options", "ok", "OK", t, sizeof t), A_VOLTAR, 0, "SCREENS/S_SINGLESETUP/BTN", 2, 1);
		break;
	}
	case TZ_MN_CREDITOS:
		wd_add(w, &n, (TzRect){ 0, 0, 800, 600 }, "", E_AREA, A_VOLTAR, 0);
		break;
	case TZ_MN_SAIR:
		wd_add(w, &n, (TzRect){ 50, 60, 400, 20 }, txt(m, "q", "quit_title", "Salir de Tzar", t, sizeof t), E_TEXTO, A_NADA, 1);
		wd_add(w, &n, (TzRect){ 50, 130, 400, 20 }, txt(m, "q", "quit", "¿Seguro que quieres salir del juego?", t, sizeof t), E_TEXTO, A_NADA, 1);
		wd_boton(w, &n, (TzRect){ 50, 258, 189, 27 }, txt(m, "general", "yes", "Sí", t, sizeof t), A_SIM_SAIR, 0, "SCREENS/D_MSGWND/BTN", 2, 1);
		wd_boton(w, &n, (TzRect){ 261, 258, 189, 27 }, txt(m, "general", "no", "No", t, sizeof t), A_VOLTAR, 0, "SCREENS/D_MSGWND/BTN", 2, 1);
		break;
	case TZ_MN_PARTIDA: {
		static const struct { const char *k, *def; int dy, acao, arg; } it[7] = {
			{ "inst", "INSTRUCCIONES", 46, A_NADA, 0 }, { "load", "CARGAR PARTIDA", 86, A_IR, TZ_MN_CARREGAR }, { "save", "GUARDAR PARTIDA", 126, A_IR, TZ_MN_GUARDAR },
			{ "opti", "OPCIONES", 166, A_IR, TZ_MN_OPCOES }, { "rest", "REINICIAR", 206, A_REINICIAR, 0 }, { "quit", "SALIR", 246, A_IR, TZ_MN_SAIR },
			{ "back", "VOLVER AL JUEGO", 326, A_CONTINUAR, 0 } };
		for (int i = 0; i < 7; ++i)
			wd_boton(w, &n, (TzRect){ 63, it[i].dy, 150, 25 }, txt(m, "menu", it[i].k, it[i].def, t, sizeof t), it[i].acao, it[i].arg, "SCREENS/D_MENU/BTN BACK", 4, it[i].acao != A_NADA);
		break;
	}
	default: break;
	}
	return n;
}

int tz_menu_botoes(const TzMenu *m, TzRect *out, char rotulos[][64], int max)
{
	Wd w[MAXW];
	int n = construir((TzMenu *)m, w), k = 0;
	for (int i = 0; i < n && k < max; ++i)
		if (w[i].est == E_BOTAO) { if (out) out[k] = w[i].r; if (rotulos) snprintf(rotulos[k], 64, "%s", w[i].rot); k++; }
	return k;
}

void tz_menu_iniciar(TzMenu *m, TzUiSkin *s, int w, int h)
{
	memset(m, 0, sizeof *m);
	m->skin = s; m->w = w; m->h = h; m->sobre = m->prem = -1;
	m->cfg.jogadores = 2; m->cfg.populacao = 200; m->cfg.dificuldade = 1; m->cfg.recursos = 1;
	m->volume_som = 80; m->volume_musica = 70;
	m->guardado_sel = -1;
	snprintf(m->nome_guardar, sizeof m->nome_guardar, "Partida 1");
}

void tz_menu_liberar(TzMenu *m)
{
	free(m->campanhas); free(m->mapas); free(m->guardados);
	m->campanhas = NULL; m->mapas = NULL; m->guardados = NULL;
}

void tz_menu_ir(TzMenu *m, int tela)
{
	if (tela == m->tela) return;
	if (m->npilha < 8) m->pilha[m->npilha++] = m->tela;
	m->tela = tela; m->rolo = 0; m->sobre = m->prem = -1; m->editando = 0;
}

static void voltar(TzMenu *m)
{
	if (m->tela == TZ_MN_PARTIDA) { m->acao = TZ_MR_CONTINUAR; return; }
	if (m->npilha) { m->tela = m->pilha[--m->npilha]; m->rolo = 0; m->sobre = m->prem = -1; m->editando = 0; }
}

static void executar(TzMenu *m, const Wd *x)
{
	switch (x->acao) {
	case A_IR: tz_menu_ir(m, x->arg); break;
	case A_VOLTAR: voltar(m); break;
	case A_CAMPANHA: m->campanha_sel = x->arg; break;
	case A_MAPA: m->mapa_sel = x->arg; if (m->cfg.jogadores > m->mapas[x->arg].jogadores_max) m->cfg.jogadores = m->mapas[x->arg].jogadores_max < 2 ? 2 : m->mapas[x->arg].jogadores_max; break;
	case A_GUARDADO: m->guardado_sel = x->arg; if (x->arg < 0) m->editando = 1; break;
	case A_SPIN: mudar(m, x->arg >> 8, (x->arg >> 1) & 7, (x->arg & 1) ? 1 : -1); break;
	case A_CHECK: if (x->arg == 0) m->cfg.sem_nevoa ^= 1; else m->cfg.explorado ^= 1; break;
	case A_NOME: m->editando = 1; break;
	case A_JOGAR:
		m->acao = TZ_MR_JOGAR; m->res_cfg = m->cfg;
		m->res_indice = m->mapa_sel >= 0 ? m->mapa_sel : -1;   /* -1: mapa aleatorio */
		break;
	case A_CAMPANHA_JOGAR: m->acao = TZ_MR_CAMPANHA; m->res_indice = m->campanha_sel; m->res_cfg = m->cfg; break;
	case A_CARREGAR: m->acao = TZ_MR_CARREGAR; m->res_indice = m->guardado_sel; break;
	case A_GUARDAR: m->acao = TZ_MR_GUARDAR; m->res_indice = m->guardado_sel; snprintf(m->res_nome, sizeof m->res_nome, "%s", m->nome_guardar); break;
	case A_SIM_SAIR: m->acao = TZ_MR_SAIR; break;
	case A_CONTINUAR: m->acao = TZ_MR_CONTINUAR; break;
	case A_REINICIAR: m->acao = TZ_MR_REINICIAR; break;
	default: break;
	}
}

static int wd_em(const TzMenu *m, const Wd *w, int n, int x, int y)
{
	int ox, oy;
	origem(m, &ox, &oy);
	for (int i = n - 1; i >= 0; --i)
		if (w[i].ok && w[i].est != E_TEXTO && tz_rect_dentro((TzRect){ w[i].r.x + ox, w[i].r.y + oy, w[i].r.w, w[i].r.h }, x, y)) return i;
	return -1;
}

void tz_menu_entrada(TzMenu *m, const TzUiEntrada *e)
{
	Wd w[MAXW];
	int n = construir(m, w);
	switch (e->tipo) {
	case TZ_IN_MOVER: m->mx = e->x; m->my = e->y; m->sobre = wd_em(m, w, n, e->x, e->y); break;
	case TZ_IN_BAIXO: if (e->botao == 1) m->prem = wd_em(m, w, n, e->x, e->y); break;
	case TZ_IN_CIMA:
		if (e->botao == 1) {
			int i = wd_em(m, w, n, e->x, e->y);
			if (i >= 0 && i == m->prem) executar(m, &w[i]);
			m->prem = -1;
		}
		break;
	case TZ_IN_RODA: {
		int max = m->tela == TZ_MN_MAPAS ? m->nmapas - 15 : m->tela == TZ_MN_CARREGAR || m->tela == TZ_MN_GUARDAR ? m->nguardados + (m->tela == TZ_MN_GUARDAR) - SAVE_ROWS : 0;
		m->rolo -= e->delta;
		if (m->rolo > max) m->rolo = max;
		if (m->rolo < 0) m->rolo = 0;
		break;
	}
	case TZ_IN_TECLA:
		if (m->editando && m->tela == TZ_MN_GUARDAR) {
			size_t l = strlen(m->nome_guardar);
			if (e->tecla >= 32 && e->tecla < 256 && l + 1 < sizeof m->nome_guardar) { m->nome_guardar[l] = (char)((e->mods & TZ_MOD_SHIFT) && e->tecla >= 'a' && e->tecla <= 'z' ? e->tecla - 32 : e->tecla); m->nome_guardar[l + 1] = 0; return; }
			if (e->tecla == TZ_TC_BORRAR && l) { m->nome_guardar[l - 1] = 0; return; }
			if (e->tecla == TZ_TC_ENTER) { m->editando = 0; return; }
		}
		if (e->tecla == TZ_TC_ESC) voltar(m);
		else if (e->tecla == TZ_TC_ENTER) {   /* el botón principal de la pantalla */
			for (int i = 0; i < n; ++i)
				if (w[i].est == E_BOTAO && w[i].ok && (w[i].acao == A_JOGAR || w[i].acao == A_CAMPANHA_JOGAR || w[i].acao == A_CARREGAR || w[i].acao == A_GUARDAR || w[i].acao == A_SIM_SAIR)) { executar(m, &w[i]); break; }
		} else if (e->tecla == TZ_TC_CIMA || e->tecla == TZ_TC_BAIXO) {
			int d = e->tecla == TZ_TC_BAIXO ? 1 : -1;
			if (m->tela == TZ_MN_CAMPANHAS && m->ncampanhas) m->campanha_sel = (m->campanha_sel + d + m->ncampanhas) % m->ncampanhas;
			else if (m->tela == TZ_MN_MAPAS && m->nmapas) m->mapa_sel = (m->mapa_sel + d + m->nmapas) % m->nmapas;
		}
		break;
	default: break;
	}
}

/* ---------------------------------------------------------------- dibujo */

static void desenhar_botao(TzMenu *m, TzTela *t, const Wd *x, int ox, int oy, int sobre, int premido)
{
	TzUiSkin *s = m->skin;
	TzRect r = { x->r.x + ox, x->r.y + oy, x->r.w, x->r.h };
	const TzImg *art = x->art ? tz_ui_img(s, x->art, -1) : NULL;
	int est = !x->ok ? 0 : premido ? 1 : 0;
	if (art) {
		int sw = art->w / x->estados;
		int e2 = x->estados == 4 ? (!x->ok ? 3 : premido ? 2 : sobre ? 1 : 0) : est;
		tz_tela_imagem(t, art, (TzRect){ e2 * sw, 0, sw < r.w ? sw : r.w, art->h < r.h ? art->h : r.h }, r.x, r.y, x->ok ? 0 : 50);
	} else {
		tz_tela_preencher(t, r, premido ? 0xff504838 : 0xff383020);
		tz_tela_moldura(t, r, 0xff907850);
	}
	uint32_t c = !x->ok ? 0x808070 : sobre ? 0xffffe0 : 0xd8c070;
	int d = premido;
	tz_ui_fonte_caixa(t, &s->f14b, (TzRect){ r.x + d, r.y + d, r.w, r.h }, x->rot, c, 1);
}

void tz_menu_desenhar(TzMenu *m, TzTela *t)
{
	Wd w[MAXW];
	int n = construir(m, w), ox, oy;
	TzUiSkin *s = m->skin;
	int dw, dh;
	int dialogo = dialogo_tamanho(m->tela, &dw, &dh);
	if (tela_cheia(m->tela)) tz_tela_preencher(t, (TzRect){ 0, 0, t->w, t->h }, 0xff000000);
	else if (m->tela == TZ_MN_CARREGAR || m->tela == TZ_MN_GUARDAR) tz_tela_preencher(t, (TzRect){ 0, 0, t->w, t->h }, 0xc0000000);
	else tz_tela_preencher(t, (TzRect){ 0, 0, t->w, t->h }, 0xa0000000);
	origem(m, &ox, &oy);
	char fondo[96];
	const char *nome = nomes_tela[m->tela];
	snprintf(fondo, sizeof fondo, "SCREENS/%s/BACK", nome);
	int ax = ox, ay = oy;
	if (!dialogo || m->tela == TZ_MN_CARREGAR || m->tela == TZ_MN_GUARDAR) { if (tela_cheia(m->tela)) { ax = (m->w - 800) / 2; ay = (m->h - 600) / 2; } }
	const TzImg *bg = tz_ui_img(s, fondo, -1);
	if (bg) tz_tela_imagem(t, bg, (TzRect){ 0, 0, 0, 0 }, ax, ay, 0);
	else if (tela_cheia(m->tela)) tz_tela_preencher(t, (TzRect){ ax, ay, 800, 600 }, 0xff181410);
	else tz_tela_preencher(t, (TzRect){ ox, oy, dw, dh }, 0xff282018);
	if (m->tela == TZ_MN_CONFIG) {   /* el fondo de cada fila de reino (0x4c5f90): desde (62, 20), cada 35 */
		const TzImg *fila = tz_ui_img(s, "SCREENS/S_SINGLESETUP/ROW BACK", -1);
		for (int i = 0; fila && i < m->cfg.jogadores && i < 8; ++i) tz_tela_imagem(t, fila, (TzRect){ 0, 0, 0, 0 }, ox + 62, oy + 20 + 35 * i, 0);
	}
	/* título de las pantallas que lo llevan */
	if (m->tela == TZ_MN_CARREGAR || m->tela == TZ_MN_GUARDAR) {
		char b[64];
		const char *k = m->tela == TZ_MN_CARREGAR ? "load" : "save", *kt = m->tela == TZ_MN_CARREGAR ? "t_load" : "t_save";
		tz_ui_fonte_caixa(t, &s->f14b, (TzRect){ ox + 60, oy + 14, 300, 20 }, txt(m, k, kt, "", b, sizeof b), 0xd8c070, 0);
	}
	for (int i = 0; i < n; ++i) {
		const Wd *x = &w[i];
		TzRect r = { x->r.x + ox, x->r.y + oy, x->r.w, x->r.h };
		int sobre = m->sobre == i, premido = m->prem == i && sobre;
		switch (x->est) {
		case E_BOTAO: desenhar_botao(m, t, x, ox, oy, sobre, premido); break;
		case E_ITEM: {
			uint32_t c = x->marcado == 1 ? 0xffffd0 : sobre ? 0xf0e0a0 : 0xc8b070;
			if (x->marcado == 1) tz_tela_preencher(t, r, 0x40c0a040);
			tz_ui_fonte_caixa(t, &s->f12b, (TzRect){ r.x + 2, r.y, r.w - 4, r.h }, x->rot, c, 0);
			break;
		}
		case E_SPIN: break;   /* las flechas ya van en el fondo */
		case E_TEXTO: {
			const TzUiFonte *f = x->arg == 1 ? &s->f14b : &s->f12b;
			tz_ui_fonte_caixa(t, f, r, x->rot, x->arg == 1 ? 0xe8d890 : 0xd0c080, x->arg == 2 || (x->arg == 1 && r.w >= 400 && m->tela == TZ_MN_SAIR));
			break;
		}
		case E_CHECK: {
			const TzImg *chk = tz_ui_img(s, "SCREENS/COMMON/CHK", -1);   /* 31x62: marcada arriba, vacía abajo */
			if (chk && chk->w == 31 && chk->h == 62) tz_tela_imagem(t, chk, (TzRect){ 0, x->marcado == 1 ? 0 : 31, 31, 31 }, r.x, r.y, 0);
			else {
				tz_tela_moldura(t, (TzRect){ r.x + 6, r.y + 6, 18, 18 }, 0xff907850);
				if (x->marcado == 1) tz_tela_preencher(t, (TzRect){ r.x + 10, r.y + 10, 10, 10 }, 0xff30c030);
			}
			tz_ui_fonte_caixa(t, &s->f12b, (TzRect){ r.x + 36, r.y, 200, r.h }, x->rot, 0xd0c080, 0);
			break;
		}
		case E_AREA:
			if (x->arg >= 0x100) tz_tela_preencher(t, r, 0xff000000u | s->cores[(x->arg - 0x100) & 7]);
			break;
		case E_NOME:
			tz_tela_preencher(t, r, 0x80000000);
			{
				char b[48];
				snprintf(b, sizeof b, "%s%s", x->rot, m->editando && (m->tela == TZ_MN_GUARDAR) ? "_" : "");
				tz_ui_fonte_caixa(t, &s->f12b, r, b, 0xffffe0, 0);
			}
			break;
		}
	}
	/* la ventana de descripción del mapa elegido (derecha de la pantalla de opciones de la partida) */
	if (m->tela == TZ_MN_CONFIG && m->mapa_sel >= 0 && m->mapa_sel < m->nmapas) {
		const TzMnItem *it = &m->mapas[m->mapa_sel];
		char b[160];
		tz_ui_fonte_caixa(t, &s->f14b, (TzRect){ ox + 560, oy + 66, 200, 20 }, it->titulo, 0xe8d890, 0);
		snprintf(b, sizeof b, "%d - %d", it->jogadores_min, it->jogadores_max);
		tz_ui_fonte_caixa(t, &s->f12b, (TzRect){ ox + 560, oy + 92, 200, 16 }, b, 0xc8b070, 0);
	}
	if (m->tela == TZ_MN_CAMPANHAS && m->campanha_sel >= 0 && m->campanha_sel < m->ncampanhas) {
		tz_ui_fonte_caixa(t, &s->f14b, (TzRect){ ox + 420, oy + 50, 340, 20 }, m->campanhas[m->campanha_sel].titulo, 0xe8d890, 0);
	}
	if (m->tela == TZ_MN_MAPAS && m->mapa_sel >= 0 && m->mapa_sel < m->nmapas)
		tz_ui_fonte_caixa(t, &s->f14b, (TzRect){ ox + 420, oy + 50, 340, 20 }, m->mapas[m->mapa_sel].titulo, 0xe8d890, 0);
}
