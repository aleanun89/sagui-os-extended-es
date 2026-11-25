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
#include <strings.h>

#define ROW 16
#define SAVE_ROWS 9
#define MAXW 160
#define IA_FILAS 9           /* menu.rs AI_ROWS */
#define IA_X 150             /* menu.rs CHOOSE_AI_AT */
#define IA_Y 140

#define PREVIA_X 452
#define PREVIA_Y 320
#define PREVIA_L 96

enum { E_BOTAO, E_ITEM, E_SPIN, E_CHECK, E_TEXTO, E_AREA, E_NOME };
enum {
	A_NADA, A_IR, A_VOLTAR, A_CAMPANHA, A_MAPA, A_GUARDADO, A_JOGAR, A_CONFIG, A_SPIN, A_CHECK, A_SIM_SAIR, A_CONTINUAR,
	A_REINICIAR, A_RENDIR, A_CARREGAR, A_GUARDAR, A_NOME, A_CAMPANHA_JOGAR, A_ABRIR_MAPA, A_MISSAO, A_SEGUINTE, A_REPETIR, A_ABANDONAR,
	A_ALEAT_CHECK, A_ALEAT_EQUIPE, A_ALEAT_OUTRO, A_ALEAT_JOGAR, A_ALEAT_AMPLIAR,   /* pantalla del mapa aleatorio */
	A_ALEAT_IA, A_ALEAT_IA_ITEM, A_ALEAT_IA_OK, A_ALEAT_IA_CANCELAR, A_ALEAT_RENDIR,  /* D470: comportamiento y rendición de la IA */
};
enum {   /* campos de los spins */
	C_JOG, C_TIPO, C_VITORIA, C_POP, C_DIF, C_REC, C_RACA, C_VOL_SOM, C_VOL_MUS, C_VELOC, C_DIF_CAMP,
	C_TELA, C_RESOL, C_ESCALA, C_FILTRO, C_INTERF, C_PAINEL, C_CURSOR,   /* opciones de pantalla (tzar.cfg) */
	C_A_LADO, C_A_TERRENO, C_A_TIPO, C_A_RMAPA,                          /* mapa aleatorio */
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

/* spin fuera del arte original (las opciones de pantalla): dibuja sus flechas y su caja */
static void wd_spin_proprio(Wd *w, int *n, TzRect r, int campo)
{
	wd_spin(w, n, r, "", campo, 1, 0);
	w[*n - 3].marcado = 2; w[*n - 2].marcado = 2; w[*n - 1].marcado = 2;
}

static int dialogo_tamanho(int tela, int *w, int *h)
{
	switch (tela) {
	case TZ_MN_PARTIDA: *w = 291; *h = 413; return 1;
	case TZ_MN_SAIR: case TZ_MN_FIM: case TZ_MN_AJUDA: *w = 500; *h = 320; return 1;
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
	return tela != TZ_MN_PARTIDA && tela != TZ_MN_SAIR && tela != TZ_MN_CARREGAR && tela != TZ_MN_GUARDAR && tela != TZ_MN_FIM &&
	       tela != TZ_MN_AJUDA;
}

static const char *const nomes_tela[TZ_MN_NTELAS] = {
	"S_MAIN", "S_SINGLEPLAYER", "S_CAMPAIGN", "S_SINGLECHOOSEMAP", "S_SINGLESETUP", "D_LOAD", "D_SAVE", "D_OPTIONS", "S_CREDITS",
	"D_MSGWND", "D_MENU", "D_ENDGAME", "D_MSGWND", "S_SINGLERND",
};

static const int pops[] = { 50, 75, 100, 150, 200, 250, 300, 400, 500 };


/* ---------------------------------------------------------------- mapa aleatorio: reglas de la pantalla (menu.rs random_*) */
const int tz_mn_lados[7] = { 64, 96, 128, 192, 256, 384, 512 };
const char *const tz_mn_biomas[4] = { "grass", "summer", "snow", "desert" };

int tz_mn_aleat_limite(int l) { return l <= 0 ? 4 : l == 1 ? 6 : 8; }

/* en los mapas pequeños no caben archipiélagos, islas ni lagos (0x4cae70) */
int tz_mn_aleat_tipo_ok(int l, int tipo)
{
	int lado = tz_mn_lados[l < 0 ? 0 : l > 6 ? 6 : l];
	return !((lado == 64 && (tipo == 1 || tipo == 2 || tipo == 4)) || (lado == 96 && (tipo == 1 || tipo == 4)) || (lado == 128 && tipo == 1));
}

void tz_mn_aleat_padrao(TzMnAleat *a)
{
	memset(a, 0, sizeof *a);
	a->lado = 2; a->recursos_mapa = 1; a->guardado = a->animais = a->itens = a->portas = 1;
	a->semente = 417;
	a->c.jogadores = 2; a->c.populacao = 200; a->c.dificuldade = 1; a->c.recursos = 1;
}

void tz_mn_aleat_normalizar(TzMnAleat *a)
{
	if (a->lado < 0 || a->lado > 6) a->lado = 2;
	if (a->terreno < 0 || a->terreno > 4) a->terreno = 0;
	if (a->recursos_mapa < 0 || a->recursos_mapa > 2) a->recursos_mapa = 1;
	if (a->tipo < 0 || a->tipo > 5 || !tz_mn_aleat_tipo_ok(a->lado, a->tipo)) a->tipo = 0;
	a->guardado = a->guardado != 0; a->animais = a->animais != 0; a->itens = a->itens != 0; a->portas = a->portas != 0;
	a->penhascos = a->penhascos != 0;
	TzMnConfig *c = &a->c;
	if (c->jogadores < 2) c->jogadores = 2;
	if (c->jogadores > tz_mn_aleat_limite(a->lado)) c->jogadores = tz_mn_aleat_limite(a->lado);
	if (c->tipo < 0 || c->tipo > 1) c->tipo = 0;
	if (c->vitoria < 0 || c->vitoria > 3) c->vitoria = 0;
	if (c->populacao < 50 || c->populacao > 500) c->populacao = 200;
	if (c->dificuldade < 0 || c->dificuldade > 2) c->dificuldade = 1;
	if (c->recursos < 0 || c->recursos > 2) c->recursos = 1;
	c->sem_nevoa = c->sem_nevoa != 0; c->explorado = c->explorado != 0;
	for (int i = 0; i < 8; ++i) {
		if (c->raca[i] < 0 || c->raca[i] > 3) c->raca[i] = 0;
		if (c->equipe[i] < 0 || c->equipe[i] > 4) c->equipe[i] = 0;
	}
}

int tz_mn_aleat_texto(const TzMnAleat *a, char *out, size_t n)
{
	const TzMnConfig *c = &a->c;
	int k = snprintf(out, n, "ALEAT2 %d %d %d %d %d %d %d %d %llu %d %d %d %d %d %d %d %d", a->lado, a->terreno, a->tipo, a->recursos_mapa, a->guardado, a->animais,
	                 a->itens, a->portas, a->semente, c->jogadores, c->tipo, c->vitoria, c->populacao, c->dificuldade, c->recursos, c->sem_nevoa, c->explorado);
	for (int i = 0; i < 8 && k > 0 && (size_t)k < n; ++i) k += snprintf(out + k, n - (size_t)k, " %d", c->raca[i]);
	for (int i = 0; i < 8 && k > 0 && (size_t)k < n; ++i) k += snprintf(out + k, n - (size_t)k, " %d", c->equipe[i]);
	if (k > 0 && (size_t)k < n) k += snprintf(out + k, n - (size_t)k, " %d", a->penhascos != 0);   /* v8.5.6: opcional al leer */
	/* D470: «IA rendición guion[8]» («-» = el de siempre) */
	if (k > 0 && (size_t)k < n) k += snprintf(out + k, n - (size_t)k, " IA %d", a->rendicao != 0);
	for (int i = 0; i < 8 && k > 0 && (size_t)k < n; ++i) k += snprintf(out + k, n - (size_t)k, " %s", a->ia[i][0] ? a->ia[i] : "-");
	return k > 0 && (size_t)k < n ? 0 : -1;
}

/* un nombre de guion de IA aceptable (sin carpetas) */
static int guiao_ok(const char *g) { return strlen(g) < 33 && !strchr(g, '/') && !strchr(g, '\\') && !strstr(g, ".."); }

int tz_mn_aleat_de_texto(const char *t, TzMnAleat *a)
{
	if (!t || strncmp(t, "ALEAT2 ", 7)) return -1;
	const char *p = t + 7;
	long long v[33];
	for (int i = 0; i < 33; ++i) {
		char *fim;
		if (i == 8) { unsigned long long u = strtoull(p, &fim, 10); if (fim == p) return -1; v[i] = (long long)u; }
		else { v[i] = strtoll(p, &fim, 10); if (fim == p) return -1; }
		p = fim;
	}
	TzMnAleat x;
	tz_mn_aleat_padrao(&x);
	x.lado = (int)v[0]; x.terreno = (int)v[1]; x.tipo = (int)v[2]; x.recursos_mapa = (int)v[3]; x.guardado = (int)v[4]; x.animais = (int)v[5];
	x.itens = (int)v[6]; x.portas = (int)v[7]; x.semente = (unsigned long long)v[8];
	x.c.jogadores = (int)v[9]; x.c.tipo = (int)v[10]; x.c.vitoria = (int)v[11]; x.c.populacao = (int)v[12]; x.c.dificuldade = (int)v[13];
	x.c.recursos = (int)v[14]; x.c.sem_nevoa = (int)v[15]; x.c.explorado = (int)v[16];
	for (int i = 0; i < 8; ++i) { x.c.raca[i] = (int)v[17 + i]; x.c.equipe[i] = (int)v[25 + i]; }
	/* v8.5.6: «penhascos» opcional; detrás, la cola «IA ...» de D470, también opcional (las líneas de antes no las llevan) */
	{ char *fim; long long q = strtoll(p, &fim, 10); if (fim != p) { x.penhascos = q != 0; p = fim; } }
	while (*p == ' ') ++p;
	if (!strncmp(p, "IA ", 3)) {
		char *fim;
		p += 3;
		x.rendicao = strtol(p, &fim, 10) != 0;
		if (fim == p) return -1;
		p = fim;
		for (int i = 0; i < 8; ++i) {
			while (*p == ' ') ++p;
			char g[64];
			int k = 0;
			while (*p && *p != ' ' && k < 63) g[k++] = *p++;
			g[k] = 0;
			if (!k) return -1;
			if (strcmp(g, "-") && guiao_ok(g)) snprintf(x.ia[i], sizeof x.ia[i], "%s", g);
		}
	}
	tz_mn_aleat_normalizar(&x);
	*a = x;
	return 0;
}

static TzMnConfig *cfg_de(TzMenu *m) { return m->tela == TZ_MN_ALEAT ? &m->aleat.c : &m->cfg; }
static const char *sec_cfg(const TzMenu *m) { return m->tela == TZ_MN_ALEAT ? "sp_setuprnd" : "sp_setupmap"; }

/* texto «prefijo_i» de una sección de STRINGS.INI */
static const char *txt_i(const TzMenu *m, const char *sec, const char *pref, int i, const char *def, char *buf, size_t n)
{
	char k[40];
	snprintf(k, sizeof k, "%s_%d", pref, i);
	return txt(m, sec, k, def, buf, n);
}

static void valor_aleat(const TzMenu *m, int campo, char *b, size_t n)
{
	static const char *const lado_def[7] = { "64 x 64", "96 x 96", "128 x 128", "192 x 192", "256 x 256", "384 x 384", "512 x 512" };
	static const char *const terr_def[5] = { "AL AZAR", "Hierba", "Verano", "Nieve", "Desierto" };
	static const char *const tipo_def[6] = { "AL AZAR", "Archipiélagos", "Islas", "Continente", "Lagos", "Tierra" };
	static const char *const rmap_def[3] = { "Pocos", "Normales", "Muchos" };
	const TzMnAleat *a = &m->aleat;
	char t[64];
	switch (campo) {
	case C_A_LADO: snprintf(b, n, "%s", txt_i(m, "sp_setuprnd", "m_size", a->lado, lado_def[a->lado], t, sizeof t)); break;
	case C_A_TERRENO: snprintf(b, n, "%s", terr_def[a->terreno]); break;
	case C_A_TIPO: snprintf(b, n, "%s", txt_i(m, "sp_setuprnd", "m_type", a->tipo, tipo_def[a->tipo], t, sizeof t)); break;
	case C_A_RMAPA: snprintf(b, n, "%s", txt_i(m, "sp_setuprnd", "m_res", a->recursos_mapa, rmap_def[a->recursos_mapa], t, sizeof t)); break;
	default: b[0] = 0;
	}
}

static void valor_cfg(const TzMenu *m, int campo, int idx, char *b, size_t n)
{
	const TzMnConfig *c = m->tela == TZ_MN_ALEAT ? &m->aleat.c : &m->cfg;
	const char *sc = sec_cfg(m);
	char t[64];
	switch (campo) {
	case C_A_LADO: case C_A_TERRENO: case C_A_TIPO: case C_A_RMAPA: valor_aleat(m, campo, b, n); break;
	case C_JOG: snprintf(b, n, "%d", c->jogadores); break;
	case C_TIPO: snprintf(b, n, "%s", txt(m, sc, (char[]){ 'g', '_', 't', 'y', 'p', 'e', '_', (char)('0' + c->tipo), 0 }, "", t, sizeof t)); break;
	case C_VITORIA: snprintf(b, n, "%s", txt(m, sc, (char[]){ 'v', 'c', 'o', 'n', 'd', '_', (char)('0' + c->vitoria), 0 }, "", t, sizeof t)); break;
	case C_POP: snprintf(b, n, "%d", c->populacao); break;
	case C_DIF: case C_DIF_CAMP: snprintf(b, n, "%s", txt(m, sc, (char[]){ 'd', 'i', 'f', 'f', 'l', 'v', 'l', '_', (char)('0' + c->dificuldade), 0 }, "", t, sizeof t)); break;
	case C_REC: snprintf(b, n, "%s", txt(m, sc, (char[]){ 'r', 'e', 's', 'u', 'r', 'c', 's', '_', (char)('0' + c->recursos), 0 }, "", t, sizeof t)); break;
	case C_RACA: snprintf(b, n, "%s", txt(m, sc, (char[]){ 'r', 'a', 'c', 'e', '_', (char)('0' + c->raca[idx & 7]), 0 }, "", t, sizeof t)); break;
	case C_VOL_SOM: snprintf(b, n, "%d", m->volume_som); break;
	case C_VOL_MUS: snprintf(b, n, "%d", m->volume_musica); break;
	case C_TELA: snprintf(b, n, "%s", m->op.tela_cheia ? "Pantalla completa" : "Ventana"); break;
	case C_RESOL: tz_resolucao_nome(m->op.resolucao, b, (int)n); break;
	case C_ESCALA: snprintf(b, n, "%s", m->op.escala == TZ_ESC_INTEIRA ? "Entero" : "Ajustado"); break;
	case C_FILTRO: snprintf(b, n, "%s", m->op.suave ? "Suave" : "Nítido"); break;
	case C_INTERF: if (m->op.interface) snprintf(b, n, "x%d", m->op.interface); else snprintf(b, n, "Automática"); break;
	case C_PAINEL: snprintf(b, n, "%s", m->op.painel_direita ? "Derecha" : "Izquierda"); break;
	case C_CURSOR: snprintf(b, n, "%s", m->op.cursor_jogo ? "Del juego" : "Del sistema"); break;
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
	TzMnConfig *c = cfg_de(m);
	int maxj = 8, minj = 2, aleat = m->tela == TZ_MN_ALEAT;
	TzMnAleat *al = &m->aleat;
	if (aleat) maxj = tz_mn_aleat_limite(al->lado);
	else if (m->mapa_sel >= 0 && m->mapa_sel < m->nmapas) { maxj = m->mapas[m->mapa_sel].jogadores_max; minj = m->mapas[m->mapa_sel].jogadores_min; }
	if (maxj < 2) maxj = 2;
	if (minj < 2) minj = 2;
#define CICLO(v, n) ((v) = (((v) + d) % (n) + (n)) % (n))
	switch (campo) {
	case C_JOG: c->jogadores += d; if (c->jogadores < minj) c->jogadores = minj; if (c->jogadores > maxj) c->jogadores = maxj; if (c->jogadores > 8) c->jogadores = 8; break;
	case C_TIPO: CICLO(c->tipo, aleat ? 2 : 3); break;
	case C_VITORIA: CICLO(c->vitoria, 4); break;
	case C_POP: c->populacao = escolher_pop(c->populacao, d); break;
	case C_DIF: case C_DIF_CAMP: CICLO(c->dificuldade, 3); break;
	case C_REC: CICLO(c->recursos, aleat ? 3 : 4); break;
	case C_RACA: CICLO(c->raca[idx & 7], 4); break;
	case C_A_LADO:
		CICLO(al->lado, 7);
		tz_mn_aleat_normalizar(al);
		break;
	case C_A_TERRENO: CICLO(al->terreno, 5); break;
	case C_A_TIPO:
		for (int i = 0; i < 6; ++i) { CICLO(al->tipo, 6); if (tz_mn_aleat_tipo_ok(al->lado, al->tipo)) break; }
		break;
	case C_A_RMAPA: CICLO(al->recursos_mapa, 3); break;
	case C_VOL_SOM: m->volume_som += 10 * d; if (m->volume_som < 0) m->volume_som = 0; if (m->volume_som > 100) m->volume_som = 100; break;
	case C_VOL_MUS: m->volume_musica += 10 * d; if (m->volume_musica < 0) m->volume_musica = 0; if (m->volume_musica > 100) m->volume_musica = 100; break;
	case C_TELA: m->op.tela_cheia ^= 1; m->op_mudou = 1; break;
	case C_RESOL: CICLO(m->op.resolucao, TZ_NRESOLUCOES); m->op_mudou = 1; break;
	case C_ESCALA: m->op.escala ^= 1; m->op_mudou = 1; break;
	case C_FILTRO: m->op.suave ^= 1; m->op_mudou = 1; break;
	case C_INTERF: CICLO(m->op.interface, 4); m->op_mudou = 1; break;
	case C_PAINEL: m->op.painel_direita ^= 1; m->op_mudou = 1; break;
	case C_CURSOR: m->op.cursor_jogo ^= 1; m->op_mudou = 1; break;
	default: break;
	}
	if (aleat && (campo <= C_RACA || campo >= C_A_LADO)) m->aleat_mudou = 1;
#undef CICLO
}

static char *ler_dados(TzUiSkin *s, const char *nome);

/* D470: los comportamientos de la IA (menu.rs Texts::load, 0x48a780): el de siempre y los de [scripts] de DATA/AI/AI.INI
 * («nombre = fichero»); y si se ofrece la rendición de la IA (están IMAGES/AIRESIGN.BMP y DATA/AI/RESIGN.AI) */
static void ler_guioes(TzMenu *m)
{
	if (m->ia_lido) return;
	m->ia_lido = 1;
	m->nia = 1;
	m->ia_nome[0][0] = m->ia_fich[0][0] = 0;
	char *t = ler_dados(m->skin, "DATA/AI/AI.INI");
	int dentro = 0;
	for (char *l = t; l && *l && m->nia < 24;) {
		char *fim = strchr(l, '\n');
		if (fim) *fim = 0;
		char *a = l;
		while (*a == ' ' || *a == '\t') ++a;
		size_t k = strlen(a);
		while (k && (a[k - 1] == '\r' || a[k - 1] == ' ' || a[k - 1] == '\t')) a[--k] = 0;
		if (*a == '[') dentro = !strncasecmp(a, "[scripts]", 9);
		else if (dentro && *a && *a != ';') {
			char *eq = strchr(a, '=');
			if (eq) {
				*eq = 0;
				char *nome = a, *fich = eq + 1;
				size_t kn = strlen(nome);
				while (kn && (nome[kn - 1] == ' ' || nome[kn - 1] == '\t')) nome[--kn] = 0;
				while (*fich == ' ' || *fich == '\t') ++fich;
				if (*fich && guiao_ok(fich)) {
					snprintf(m->ia_nome[m->nia], sizeof m->ia_nome[0], "%s", nome);
					snprintf(m->ia_fich[m->nia], sizeof m->ia_fich[0], "%s", fich);
					m->nia++;
				}
			}
		}
		if (!fim) break;
		l = fim + 1;
	}
	free(t);
	char *b = ler_dados(m->skin, "IMAGES/AIRESIGN.BMP"), *r = ler_dados(m->skin, "DATA/AI/RESIGN.AI");
	m->rendicao_oferecida = b && r;
	free(b); free(r);
}

/* el nombre del comportamiento de un guion (el fichero si AI.INI no lo nombra) */
static const char *nome_guiao(const TzMenu *m, const char *g)
{
	for (int i = 1; i < m->nia; ++i) if (!strcasecmp(m->ia_fich[i], g)) return m->ia_nome[i];
	return g;
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
			{ "campaign", "CAMPAÑA", 141, TZ_MN_CAMPANHAS }, { "custom", "MISIÓN", 233, TZ_MN_MAPAS }, { "random", "MAPA ALEATORIO", 329, TZ_MN_ALEAT },
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
		/* las misiones de la campaña elegida (menu.rs: 9 filas desde (80, 352)); las no alcanzadas, sin poder elegirse */
		for (int i = 0, row = 0, k = 0; i < m->nmissoes && row < 9; ++i) {
			const TzMnItem *it = &m->missoes[i];
			if (it->campanha != m->campanha_sel) continue;
			if (k++ < m->rolo) continue;
			wd_add(w, &n, (TzRect){ 80, 352 + row * ROW, 252, ROW }, it->titulo, E_ITEM, A_MISSAO, i);
			ultimo(w, n)->marcado = i == m->missao_sel;
			ultimo(w, n)->ok = it->disponivel;
			row++;
		}
		wd_spin(w, &n, (TzRect){ 332, 542, 180, 27 }, "", C_DIF_CAMP, 1, 0);
		valor_cfg(m, C_DIF_CAMP, 0, w[n - 2].rot, sizeof w[0].rot);
		wd_boton(w, &n, (TzRect){ 78, 545, 235, 25 }, txt(m, "campaign", "play", "JUGAR", t, sizeof t), A_CAMPANHA_JOGAR, 0, "SCREENS/S_CAMPAIGN/BTN", 2,
		         m->ncampanhas > 0 && (m->missao_sel < 0 || (m->missao_sel < m->nmissoes && m->missoes[m->missao_sel].disponivel)));
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
	case TZ_MN_ALEAT: {
		/* menu.rs Screen::Random: columna derecha con las opciones del mapa, filas de reinos a la izquierda y abajo las reglas */
		const TzMnAleat *al = &m->aleat;
		ler_guioes(m);
		static const struct { int x, y; const char *k, *def; } et[] = {
			{ 68, 353, "t_num_players", "Reinos" }, { 68, 410, "t_game_type", "Tipo de partida" }, { 68, 470, "t_vict_cond", "Condición de victoria" },
			{ 273, 353, "t_poplimit", "Límite de población" }, { 273, 412, "t_diff_lvl", "Dificultad" }, { 273, 470, "t_st_res", "Recursos iniciales" },
			{ 302, 549, "t_fog", "Sin niebla" }, { 302, 579, "t_expolre", "Explorar" },
			{ 568, 43, "t_map_size", "Tamaño del mapa" }, { 568, 98, "t_map_type", "Tipo de mapa" }, { 568, 155, "t_terrain", "Terreno" },
			{ 568, 214, "t_map_res", "Recursos del mapa" } };
		for (int i = 0; i < 12; ++i) {
			if (i == 6 || i == 7) continue;   /* las casillas de niebla llevan su propio rótulo */
			snprintf(t2, sizeof t2, "%s", txt(m, "sp_setuprnd", et[i].k, et[i].def, t, sizeof t));
			if (i == 0) { snprintf(t, sizeof t, "%.80s (2 - %d)", t2, tz_mn_aleat_limite(al->lado)); snprintf(t2, sizeof t2, "%s", t); }
			wd_add(w, &n, (TzRect){ et[i].x, et[i].y, 190, 13 }, t2, E_TEXTO, A_NADA, 0);
		}
		for (int i = 0; i < al->c.jogadores && i < 8; ++i) {
			int y = 35 * i;
			wd_add(w, &n, (TzRect){ 73, 31 + y, 13, 13 }, "", E_AREA, A_NADA, 0x100 + i);   /* muestra del color */
			if (i) snprintf(t, sizeof t, "IA %d", i); else snprintf(t, sizeof t, "%s", txt(m, "peons", "player", "Jugador 1", t2, sizeof t2));
			if (i && al->ia[i][0]) { snprintf(t2, sizeof t2, "%.40s (%.40s)", t, nome_guiao(m, al->ia[i])); snprintf(t, sizeof t, "%s", t2); }
			wd_add(w, &n, (TzRect){ 110, 30 + y, 163, 14 }, t, E_TEXTO, A_NADA, 0);
			/* D470: el icono de la IA (0x4ca6d3): abre «COMPORTAMIENTO IA»; encendido si el reino tiene uno propio. La persona no. */
			if (i) { wd_add(w, &n, (TzRect){ 273, 26 + y, 20, 20 }, "", E_AREA, A_ALEAT_IA, i); ultimo(w, n)->marcado = al->ia[i][0] != 0; }
			wd_spin(w, &n, (TzRect){ 311, 23 + y, 180, 27 }, "", C_RACA, 1, i);
			valor_cfg(m, C_RACA, i, w[n - 2].rot, sizeof w[0].rot);
			wd_add(w, &n, (TzRect){ 511, 31 + y, 13, 13 }, "", E_AREA, A_ALEAT_EQUIPE, 0x200 + i);   /* equipo */
			ultimo(w, n)->arg = i; ultimo(w, n)->marcado = al->c.equipe[i];
		}
		static const struct { int x, y, campo; } sp[6] = { { 66, 369, C_JOG }, { 66, 427, C_TIPO }, { 66, 487, C_VITORIA }, { 270, 369, C_POP }, { 270, 427, C_DIF }, { 270, 487, C_REC } };
		for (int i = 0; i < 6; ++i) {
			wd_spin(w, &n, (TzRect){ sp[i].x, sp[i].y, 180, 27 }, "", sp[i].campo, 1, 0);
			valor_cfg(m, sp[i].campo, 0, w[n - 2].rot, sizeof w[0].rot);
		}
		static const struct { int y, campo; } sm[4] = { { 56, C_A_LADO }, { 111, C_A_TIPO }, { 169, C_A_TERRENO }, { 228, C_A_RMAPA } };
		for (int i = 0; i < 4; ++i) {
			wd_spin(w, &n, (TzRect){ 565, sm[i].y, 180, 27 }, "", sm[i].campo, 1, 0);
			valor_cfg(m, sm[i].campo, 0, w[n - 2].rot, sizeof w[0].rot);
		}
		static const struct { int y; const char *k, *def; } ck[5] = { { 267, "t_gslocations", "Torres de guardia" }, { 303, "t_animals", "Animales" },
			{ 339, "t_mitems", "Objetos mágicos" }, { 376, "t_cliffs", "Montañas" }, { 411, "t_gateways", "Puertas mágicas" } };
		for (int i = 0; i < 5; ++i) {
			wd_add(w, &n, (TzRect){ 564, ck[i].y, 30, 30 }, txt(m, "sp_setuprnd", ck[i].k, ck[i].def, t, sizeof t), E_CHECK, A_ALEAT_CHECK, i);
			ultimo(w, n)->marcado = i == 0 ? al->guardado : i == 1 ? al->animais : i == 2 ? al->itens : i == 3 ? al->penhascos : al->portas;
		}
		/* D470: «rendición de la IA» (0x4c92b0), si los datos la traen */
		if (m->rendicao_oferecida) { wd_add(w, &n, (TzRect){ 471, 427, 32, 32 }, "", E_AREA, A_ALEAT_RENDIR, 0); ultimo(w, n)->marcado = al->rendicao; }
		wd_add(w, &n, (TzRect){ 268, 539, 30, 30 }, txt(m, "sp_setuprnd", "t_fog", "Sin niebla", t, sizeof t), E_CHECK, A_CHECK, 0);
		ultimo(w, n)->marcado = al->c.sem_nevoa;
		wd_add(w, &n, (TzRect){ 268, 569, 30, 30 }, txt(m, "sp_setuprnd", "t_expolre", "Explorar", t, sizeof t), E_CHECK, A_CHECK, 1);
		ultimo(w, n)->marcado = al->c.explorado;
		wd_add(w, &n, (TzRect){ PREVIA_X, PREVIA_Y, PREVIA_L, PREVIA_L }, "", E_AREA, A_ALEAT_AMPLIAR, 0);   /* clic: ver el mapa en grande */
		wd_boton(w, &n, (TzRect){ 556, 459, 210, 27 }, txt(m, "sp_setuprnd", "newmap", "OTRO MAPA", t, sizeof t), A_ALEAT_OUTRO, 0, "SCREENS/S_SINGLERND/BTN", 2, 1);
		wd_boton(w, &n, (TzRect){ 556, 499, 210, 27 }, txt(m, "sp_setuprnd", "play", "JUGAR", t, sizeof t), A_ALEAT_JOGAR, 0, "SCREENS/S_SINGLERND/BTN", 2, 1);
		wd_boton(w, &n, (TzRect){ 556, 554, 210, 27 }, txt(m, "sp_setuprnd", "back", "VOLVER", t, sizeof t), A_VOLTAR, 0, "SCREENS/S_SINGLERND/BTN", 2, 1);
		if (m->escolher_ia > 0) {
			/* D470: «COMPORTAMIENTO IA» (menu.rs choose_ai_widgets), la única que atiende mientras está abierta: una capa que
			 * tapa la pantalla (dibuja el fondo de la ventana), la lista y «Aceptar» y «Cancelar» */
			wd_add(w, &n, (TzRect){ -400, -300, 1600, 1200 }, txt(m, "chooseai", "title", "COMPORTAMIENTO IA", t, sizeof t), E_AREA, A_NADA, 0x300);
			int pr = m->rolo > m->nia - IA_FILAS ? m->nia - IA_FILAS : m->rolo;
			if (pr < 0) pr = 0;
			for (int k = pr; k < m->nia && k < pr + IA_FILAS; ++k) {
				const char *nm = k ? m->ia_nome[k] : txt(m, "chooseai", "default", "Por defecto", t2, sizeof t2);
				wd_add(w, &n, (TzRect){ IA_X + 46, IA_Y + 88 + (k - pr) * ROW, 377, ROW }, nm, E_ITEM, A_ALEAT_IA_ITEM, k);
				ultimo(w, n)->marcado = m->ia_sel == k;
			}
			wd_boton(w, &n, (TzRect){ IA_X + 47, IA_Y + 254, 189, 27 }, txt(m, "chooseai", "ok", "ACEPTAR", t, sizeof t), A_ALEAT_IA_OK, 0, "SCREENS/D_CHOOSEAI/BTN", 2, 1);
			wd_boton(w, &n, (TzRect){ IA_X + 261, IA_Y + 254, 189, 27 }, txt(m, "chooseai", "cancel", "CANCELAR", t, sizeof t), A_ALEAT_IA_CANCELAR, 0, "SCREENS/D_CHOOSEAI/BTN", 2, 1);
		}
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
		/* pantalla (tzar.cfg): ventana o pantalla completa (F11, Alt+Intro), resolución, escalado, filtro, interfaz, panel, cursor */
		static const struct { const char *rot; int campo; } pt[7] = {
			{ "Modo", C_TELA }, { "Resolución", C_RESOL }, { "Escalado", C_ESCALA }, { "Filtro", C_FILTRO }, { "Interfaz", C_INTERF },
			{ "Panel", C_PAINEL }, { "Cursor", C_CURSOR } };
		/* dentro del recuadro «ELEMENTOS DE PANTALLA» (sus casillas originales no se usan: se tapan) */
		wd_add(w, &n, (TzRect){ 450, 318, 300, 186 }, "", E_AREA, A_NADA, 1);
		for (int i = 0; i < 7; ++i) {
			int y = 322 + 26 * i;
			wd_add(w, &n, (TzRect){ 458, y + 4, 88, 16 }, pt[i].rot, E_TEXTO, A_NADA, 0);
			wd_spin_proprio(w, &n, (TzRect){ 548, y, 196, 23 }, pt[i].campo);
			valor_cfg(m, pt[i].campo, 0, w[n - 2].rot, sizeof w[0].rot);
		}
		wd_boton(w, &n, (TzRect){ 538, 552, 210, 27 }, txt(m, "options", "ok", "OK", t, sizeof t), A_VOLTAR, 0, "SCREENS/S_SINGLESETUP/BTN", 2, 1);
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
	case TZ_MN_FIM: {
		/* menu.rs Screen::End (0x442700): ganó -> repetir, siguiente, salir, ocultar; perdió -> repetir, cargar, salir, ocultar */
		static const char *const def[4] = { "REPETIR", "CONTINUAR", "SALIR", "OCULTAR" };
		int seg = m->fim_ganhou || m->fim_seguinte;
		for (int i = 0; i < 4; ++i) {
			const char *k = i == 0 ? "restart" : i == 1 ? (seg ? "next" : "load") : i == 2 ? "quit" : "hide";
			int acao = i == 0 ? A_REPETIR : i == 1 ? (seg ? A_SEGUINTE : A_IR) : i == 2 ? A_ABANDONAR : A_CONTINUAR;
			wd_boton(w, &n, (TzRect){ 233, 114 + 48 * i, 210, 27 }, txt(m, "endgamedlg", k, i == 1 && !seg ? "CARGAR" : def[i], t, sizeof t), acao,
			         i == 1 && !seg ? TZ_MN_CARREGAR : 0, "SCREENS/D_ENDGAME/BTN", 2, i != 1 || !seg || m->fim_seguinte);
		}
		break;
	}
	case TZ_MN_AJUDA:
		wd_boton(w, &n, (TzRect){ 155, 258, 189, 27 }, txt(m, "general", "close", "Cerrar", t, sizeof t), A_VOLTAR, 0, "SCREENS/D_MSGWND/BTN", 2, 1);
		break;
	case TZ_MN_PARTIDA: {
		static const struct { const char *k, *def; int dy, acao, arg; } it[8] = {
			{ "inst", "INSTRUCCIONES", 46, A_IR, TZ_MN_AJUDA }, { "load", "CARGAR PARTIDA", 86, A_IR, TZ_MN_CARREGAR }, { "save", "GUARDAR PARTIDA", 126, A_IR, TZ_MN_GUARDAR },
			{ "opti", "OPCIONES", 166, A_IR, TZ_MN_OPCOES }, { "rest", "REINICIAR", 206, A_REINICIAR, 0 }, { "quit", "SALIR", 246, A_IR, TZ_MN_SAIR },
			{ "surr", "RENDIRSE", 286, A_RENDIR, 0 },
			{ "back", "VOLVER AL JUEGO", 326, A_CONTINUAR, 0 } };
		for (int i = 0; i < 8; ++i)
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
	tz_mn_aleat_padrao(&m->aleat);
	m->volume_som = 80; m->volume_musica = 70;
	m->guardado_sel = -1;
	m->missao_sel = -1;
	tz_opcoes_padrao(&m->op);
	snprintf(m->nome_guardar, sizeof m->nome_guardar, "Partida 1");
}

void tz_menu_redimensionar(TzMenu *m, int w, int h) { m->w = w; m->h = h; }

void tz_menu_tic(TzMenu *m, uint32_t ms) { m->agora_ms = ms; }

void tz_menu_liberar(TzMenu *m)
{
	free(m->campanhas); free(m->mapas); free(m->guardados); free(m->missoes);
	m->campanhas = NULL; m->mapas = NULL; m->guardados = NULL; m->missoes = NULL;
	for (int i = 0; i < m->najuda; ++i) free(m->ajuda[i]);
	for (int i = 0; i < m->ncreditos; ++i) free(m->creditos[i]);
	free(m->ajuda); free(m->creditos); free(m->creditos_estilo);
	m->ajuda = m->creditos = NULL; m->creditos_estilo = NULL; m->najuda = m->ncreditos = 0;
}

void tz_menu_ir(TzMenu *m, int tela)
{
	if (tela == m->tela) return;
	if (m->npilha < 8) m->pilha[m->npilha++] = m->tela;
	m->tela = tela; m->rolo = 0; m->sobre = m->prem = -1; m->editando = 0; m->ampliada = 0;
	if (tela == TZ_MN_CREDITOS) m->creditos_ms0 = m->agora_ms;
	if (tela == TZ_MN_CAMPANHAS && (m->missao_sel < 0 || m->missao_sel >= m->nmissoes || m->missoes[m->missao_sel].campanha != m->campanha_sel))
		for (int i = 0; i < m->nmissoes; ++i) if (m->missoes[i].campanha == m->campanha_sel && m->missoes[i].disponivel) m->missao_sel = i;
}

static void voltar(TzMenu *m)
{
	if (m->tela == TZ_MN_PARTIDA) { m->acao = TZ_MR_CONTINUAR; return; }
	if (m->npilha) { m->tela = m->pilha[--m->npilha]; m->rolo = 0; m->sobre = m->prem = -1; m->editando = 0; m->ampliada = 0; }
}

static unsigned long long semente_siguiente(unsigned long long x) { return (x * 6364136223846793005ull + 1442695040888963407ull) | 1; }

static void executar(TzMenu *m, const Wd *x)
{
	switch (x->acao) {
	case A_IR: tz_menu_ir(m, x->arg); break;
	case A_VOLTAR: voltar(m); break;
	case A_CAMPANHA:
		m->campanha_sel = x->arg; m->rolo = 0; m->missao_sel = -1;
		/* la última misión alcanzada de esa campaña */
		for (int i = 0; i < m->nmissoes; ++i) if (m->missoes[i].campanha == x->arg && m->missoes[i].disponivel) m->missao_sel = i;
		break;
	case A_MISSAO: if (x->arg >= 0 && x->arg < m->nmissoes && m->missoes[x->arg].disponivel) m->missao_sel = x->arg; break;
	case A_SEGUINTE: m->acao = TZ_MR_SEGUINTE; break;
	case A_REPETIR: m->acao = TZ_MR_REPETIR; break;
	case A_ABANDONAR: m->acao = TZ_MR_ABANDONAR; break;
	case A_MAPA: m->mapa_sel = x->arg; if (m->cfg.jogadores > m->mapas[x->arg].jogadores_max) m->cfg.jogadores = m->mapas[x->arg].jogadores_max < 2 ? 2 : m->mapas[x->arg].jogadores_max; break;
	case A_GUARDADO: m->guardado_sel = x->arg; if (x->arg < 0) m->editando = 1; break;
	case A_SPIN: mudar(m, x->arg >> 8, (x->arg >> 1) & 7, (x->arg & 1) ? 1 : -1); break;
	case A_CHECK: {
		TzMnConfig *c = cfg_de(m);
		if (x->arg == 0) c->sem_nevoa ^= 1; else c->explorado ^= 1;
		if (m->tela == TZ_MN_ALEAT) m->aleat_mudou = 1;
		break;
	}
	case A_ALEAT_CHECK: {
		TzMnAleat *a = &m->aleat;
		switch (x->arg) { case 0: a->guardado ^= 1; break; case 1: a->animais ^= 1; break; case 2: a->itens ^= 1; break; case 3: a->penhascos ^= 1; break; case 4: a->portas ^= 1; break; default: break; }
		m->aleat_mudou = 1;
		break;
	}
	case A_ALEAT_EQUIPE:
		if (x->arg >= 0 && x->arg < 8) { m->aleat.c.equipe[x->arg] = (m->aleat.c.equipe[x->arg] + 1) % 5; m->aleat_mudou = 1; }
		break;
	case A_ALEAT_AMPLIAR: m->ampliada = 1; break;
	case A_ALEAT_IA:   /* D470: abrir la ventana con el guion de la fila marcado */
		if (x->arg > 0 && x->arg < 8) {
			m->escolher_ia = x->arg; m->ia_sel = 0; m->rolo = 0;
			for (int k = 1; k < m->nia; ++k) if (!strcasecmp(m->ia_fich[k], m->aleat.ia[x->arg])) m->ia_sel = k;
			m->sobre = m->prem = -1;
		}
		break;
	case A_ALEAT_IA_ITEM: if (x->arg >= 0 && x->arg < m->nia) m->ia_sel = x->arg; break;
	case A_ALEAT_IA_OK:
		if (m->escolher_ia > 0 && m->escolher_ia < 8 && m->ia_sel >= 0 && m->ia_sel < m->nia) {
			char g[33];
			snprintf(g, sizeof g, "%s", m->ia_fich[m->ia_sel]);
			memcpy(m->aleat.ia[m->escolher_ia], g, sizeof g);
			m->aleat_mudou = 1;
		}
		m->escolher_ia = 0; m->rolo = 0; m->sobre = m->prem = -1;
		break;
	case A_ALEAT_IA_CANCELAR: m->escolher_ia = 0; m->rolo = 0; m->sobre = m->prem = -1; break;
	case A_ALEAT_RENDIR: m->aleat.rendicao ^= 1; m->aleat_mudou = 1; break;
	case A_ALEAT_OUTRO: m->aleat.semente = semente_siguiente(m->aleat.semente); m->aleat_mudou = 1; break;
	case A_ALEAT_JOGAR:
		m->acao = TZ_MR_JOGAR; m->res_aleat = m->aleat; m->res_cfg = m->aleat.c; m->res_indice = -1;
		m->aleat.semente = semente_siguiente(m->aleat.semente); m->aleat_mudou = 1;   /* la próxima partida, otro mapa */
		break;
	case A_NOME: m->editando = 1; break;
	case A_JOGAR:
		m->acao = TZ_MR_JOGAR; m->res_cfg = m->cfg;
		m->res_indice = m->mapa_sel >= 0 ? m->mapa_sel : -1;   /* -1: mapa aleatorio */
		break;
	case A_CAMPANHA_JOGAR:
		m->acao = TZ_MR_CAMPANHA; m->res_indice = m->campanha_sel; m->res_cfg = m->cfg;
		m->res_missao = m->missao_sel >= 0 && m->missao_sel < m->nmissoes && m->missoes[m->missao_sel].campanha == m->campanha_sel ? m->missoes[m->missao_sel].indice : -1;
		break;
	case A_CARREGAR: m->acao = TZ_MR_CARREGAR; m->res_indice = m->guardado_sel; break;
	case A_GUARDAR: m->acao = TZ_MR_GUARDAR; m->res_indice = m->guardado_sel; snprintf(m->res_nome, sizeof m->res_nome, "%s", m->nome_guardar); break;
	case A_SIM_SAIR: m->acao = TZ_MR_SAIR; break;
	case A_CONTINUAR: m->acao = TZ_MR_CONTINUAR; break;
	case A_REINICIAR: m->acao = TZ_MR_REINICIAR; break;
	case A_RENDIR: m->acao = TZ_MR_RENDIR; break;
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
	if (m->ampliada) {   /* la vista previa en grande: cualquier clic o tecla la cierra */
		if (e->tipo == TZ_IN_BAIXO || e->tipo == TZ_IN_TECLA) m->ampliada = 0;
		if (e->tipo != TZ_IN_MOVER) return;
	}
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
		int nm = 0;
		for (int i = 0; i < m->nmissoes; ++i) nm += m->missoes[i].campanha == m->campanha_sel;
		int max = m->tela == TZ_MN_MAPAS ? m->nmapas - 15 : m->tela == TZ_MN_CARREGAR || m->tela == TZ_MN_GUARDAR ? m->nguardados + (m->tela == TZ_MN_GUARDAR) - SAVE_ROWS
		        : m->tela == TZ_MN_CAMPANHAS ? nm - 9 : m->tela == TZ_MN_AJUDA ? m->najuda - 9 : 0;
		if (m->tela == TZ_MN_ALEAT && m->escolher_ia > 0) max = m->nia - IA_FILAS;
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
		if (e->tecla == TZ_TC_ESC && m->tela == TZ_MN_ALEAT && m->escolher_ia > 0) { m->escolher_ia = 0; m->rolo = 0; }
		else if (e->tecla == TZ_TC_ESC) voltar(m);
		else if (e->tecla == TZ_TC_ENTER) {   /* el botón principal de la pantalla */
			for (int i = 0; i < n; ++i)
				if (w[i].est == E_BOTAO && w[i].ok && (w[i].acao == A_JOGAR || w[i].acao == A_ALEAT_JOGAR || w[i].acao == A_CAMPANHA_JOGAR || w[i].acao == A_CARREGAR || w[i].acao == A_GUARDAR || w[i].acao == A_SIM_SAIR)) { executar(m, &w[i]); break; }
		} else if (e->tecla == TZ_TC_CIMA || e->tecla == TZ_TC_BAIXO) {
			int d = e->tecla == TZ_TC_BAIXO ? 1 : -1;
			if (m->tela == TZ_MN_CAMPANHAS && m->ncampanhas) m->campanha_sel = (m->campanha_sel + d + m->ncampanhas) % m->ncampanhas;
			else if (m->tela == TZ_MN_MAPAS && m->nmapas) m->mapa_sel = (m->mapa_sel + d + m->nmapas) % m->nmapas;
		}
		break;
	default: break;
	}
}

/* ---------------------------------------------------------------- textos largos: instrucciones y créditos */

/* añade una línea a una lista dinámica */
static int linha_add(char ***l, uint8_t **est, int *n, const char *txt, int estilo)
{
	char **nl = realloc(*l, (size_t)(*n + 1) * sizeof *nl);
	if (!nl) return -1;
	*l = nl;
	if (est) {
		uint8_t *ne = realloc(*est, (size_t)(*n + 1));
		if (!ne) return -1;
		*est = ne;
		ne[*n] = (uint8_t)estilo;
	}
	nl[*n] = malloc(strlen(txt) + 1);
	if (!nl[*n]) return -1;
	strcpy(nl[*n], txt);
	(*n)++;
	return 0;
}

static const TzUiFonte *fonte_estilo(const TzUiSkin *s, int e)
{
	/* CREDITSFNT.INI: 0 títulos grandes, 1 nombres, 2 y más texto */
	return e == 0 ? &s->f22 : e == 1 ? &s->f14b : &s->f12b;
}

/* Parte un texto de los datos (Windows-1252, como Latin-1 en las fuentes) en líneas que caben en largura. «@N» al principio de
 * una línea cambia el estilo (LOCAL/CREDITS.TXT, menu.rs). */
static void partir_texto(const TzUiSkin *s, const char *txt, int largura, char ***l, uint8_t **est, int *n, int estilo_fixo)
{
	int estilo = 1;
	const char *p = txt;
	while (*p) {
		const char *fim = strchr(p, '\n');
		size_t len = fim ? (size_t)(fim - p) : strlen(p);
		char linha[600];
		if (len >= sizeof linha) len = sizeof linha - 1;
		memcpy(linha, p, len);
		linha[len] = 0;
		size_t k = strlen(linha);
		while (k && (linha[k - 1] == '\r' || linha[k - 1] == ' ')) linha[--k] = 0;
		char *t = linha;
		if (t[0] == '@' && t[1] >= '0' && t[1] <= '9') { estilo = t[1] - '0'; t += 2; }
		int e = estilo_fixo >= 0 ? estilo_fixo : estilo;
		if (!*t) linha_add(l, est, n, "", e);
		else {
			char partes[24][160];
			int np = tz_ui_fonte_quebrar(fonte_estilo(s, e), t, largura, partes, 24);
			for (int i = 0; i < np; ++i) linha_add(l, est, n, partes[i], e);
		}
		if (!fim) break;
		p = fim + 1;
	}
}

static char *ler_dados(TzUiSkin *s, const char *nome)
{
	if (!s->rec) return NULL;
	uint32_t n = 0;
	uint8_t *d = tz_recursos_ler(s->rec, nome, &n);
	if (!d) return NULL;
	char *t = malloc((size_t)n + 1);
	if (t) { memcpy(t, d, n); t[n] = 0; for (uint32_t i = 0; i < n; ++i) if (!t[i]) t[i] = ' '; }
	free(d);
	return t;
}

#define AJUDA_W 372
#define AJUDA_LINHAS 9
static void ler_ajuda(TzMenu *m)
{
	if (m->ajuda_lido) return;
	m->ajuda_lido = 1;
	char *t = ler_dados(m->skin, "HELP/HK.TXT");
	static const char *const propio =
		"Teclas de Tzar en SaguiOS\n\n"
		"F11 o Alt+Intro: ventana o pantalla completa.\n"
		"Flechas o el ratón en el borde: mover la vista. Inicio: ir a lo elegido.\n"
		"Clic: elegir. Mayús+clic: añadir. Arrastrar: elegir con un recuadro. Doble clic: todos los de esa clase.\n"
		"Clic derecho: la orden de siempre (Mayús la pone en cola).\n"
		"Ctrl+0-9: guardar un grupo; 0-9: llamarlo (dos veces: ir a él).\n"
		"Las letras de cada orden (MENU.INI) la dan directamente. RePág/AvPág: otra página de órdenes.\n"
		"Espacio: pausa. + y -: velocidad. F5 y F9: guardar y cargar la partida rápida.\n"
		"Esc: el menú de la partida.";
	partir_texto(m->skin, t ? t : propio, AJUDA_W, &m->ajuda, NULL, &m->najuda, 2);
	free(t);
}

#define CRED_X 492
#define CRED_Y 160
#define CRED_W 250
#define CRED_H 395
static void ler_creditos(TzMenu *m)
{
	if (m->creditos_lido) return;
	m->creditos_lido = 1;
	char *t = ler_dados(m->skin, "LOCAL/CREDITS.TXT");
	static const char *const propio =
		"@0Tzar\n@2The Burden of the Crown\n\n@1Haemimont Games\n@2(1999)\n\n"
		"@0SaguiOS\n@2Port en C a partir de ReTzar\n@2(GPL-3.0-or-later)\n\n@1Los datos del juego no vienen con SaguiOS.";
	partir_texto(m->skin, t ? t : propio, CRED_W, &m->creditos, &m->creditos_estilo, &m->ncreditos, -1);
	free(t);
}

static int altura_estilo(const TzUiSkin *s, int e)
{
	const TzUiFonte *f = fonte_estilo(s, e);
	return (f->alt > 0 ? f->alt : 14) + 3;
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

/* texto partido en líneas dentro de un rectángulo (lo que no cabe se corta) */
static void texto_caixa(TzTela *t, const TzUiFonte *f, TzRect r, const char *txt, uint32_t cor)
{
	if (!txt || !*txt) return;
	char l[24][160];
	int lh = (f->alt > 0 ? f->alt : 14) + 2, max = r.h / lh;
	if (max > 24) max = 24;
	int n = tz_ui_fonte_quebrar(f, txt, r.w, l, max);
	for (int i = 0; i < n; ++i) tz_ui_fonte_texto(t, f, r.x, r.y + i * lh, l[i], cor);
}

/* el minimapa del mapa elegido, centrado y con su proporción, con un marco */
static void previa(TzMenu *m, TzTela *t, TzRect r)
{
	const TzImg *p = m->previa;
	tz_tela_preencher(t, r, 0xff100c08);
	if (p && p->w > 0 && p->h > 0) {
		int w = r.w, h = r.h;
		if (p->w * r.h > p->h * r.w) h = p->h * r.w / p->w; else w = p->w * r.h / p->h;
		tz_tela_imagem_escala(t, p, (TzRect){ 0, 0, p->w, p->h }, (TzRect){ r.x + (r.w - w) / 2, r.y + (r.h - h) / 2, w, h });
	}
	tz_tela_moldura(t, r, 0xff907850);
}

/* fin de la partida (menu.rs Screen::End): el dibujo WIN o LOSE centrado en (118, 157), dos líneas en cgb_22b y debajo de la
 * ventana el tiempo y las cuentas de cada reino */
static void desenhar_fim(TzMenu *m, TzTela *t, int ox, int oy)
{
	TzUiSkin *s = m->skin;
	char b[96];
	const TzImg *p = tz_ui_img(s, m->fim_ganhou ? "SCREENS/WIN" : "SCREENS/LOSE", -1);
	if (p) tz_tela_imagem(t, p, (TzRect){ 0, 0, 0, 0 }, ox + 118 - (p->w - 1) / 2, oy + 157 - (p->h - 1) / 2, 0);
	static const char *const k[2][2] = { { "lose1", "lose2" }, { "win1", "win2" } };
	static const char *const d[2][2] = { { "Derrota", "" }, { "¡Victoria!", "" } };
	for (int i = 0; i < 2; ++i)
		tz_ui_fonte_caixa(t, &s->f22, (TzRect){ ox + 224, oy + 38 + 30 * i, 228, 29 }, txt(m, "endgamedlg", k[m->fim_ganhou != 0][i], d[m->fim_ganhou != 0][i], b, sizeof b), 0xffffff, 1);
	int y = oy + 330;
	for (int i = 0; i < m->nfim_resumo && i < 8; ++i, y += 18) {
		TzRect r = { ox - 100, y, 700, 18 };
		if (r.x < 0) { r.w += r.x; r.x = 0; }
		tz_tela_preencher(t, r, 0xa0000000);
		tz_ui_fonte_caixa(t, i ? &s->f12b : &s->f14b, r, m->fim_resumo[i], m->fim_cor[i] ? m->fim_cor[i] : 0xe8d890, 1);
	}
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
	if (m->tela == TZ_MN_ALEAT) {   /* el fondo de cada fila de reino, desde (62, 20), cada 35, y el marco de la vista previa */
		const TzImg *fila = tz_ui_img(s, "SCREENS/S_SINGLERND/ROW BACK", -1);
		for (int i = 0; fila && i < m->aleat.c.jogadores && i < 8; ++i) tz_tela_imagem(t, fila, (TzRect){ 0, 0, 0, 0 }, ox + 62, oy + 20 + 35 * i, 0);
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
			uint32_t c = !x->ok ? 0x706850 : x->marcado == 1 ? 0xffffd0 : sobre ? 0xf0e0a0 : 0xc8b070;
			if (x->marcado == 1) tz_tela_preencher(t, r, 0x40c0a040);
			tz_ui_fonte_caixa(t, &s->f12b, (TzRect){ r.x + 2, r.y, r.w - 4, r.h }, x->rot, c, 0);
			break;
		}
		case E_SPIN:   /* las flechas de los spins del original ya van en el fondo; las de los nuevos se dibujan */
			if (x->marcado == 2) {
				tz_tela_preencher(t, r, sobre ? (premido ? 0xff605030 : 0xff504428) : 0xff3c3420);
				tz_tela_moldura(t, r, 0xff907850);
				tz_ui_fonte_caixa(t, &s->f14b, r, x->rot, sobre ? 0xffffe0 : 0xd8c070, 1);
			}
			break;
		case E_TEXTO: {
			if (x->marcado == 2) { tz_tela_preencher(t, r, 0xc0181008); tz_tela_moldura(t, r, 0xff605030); }
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
			tz_ui_fonte_caixa(t, &s->f12b, (TzRect){ r.x + 36, r.y, 200, r.h }, x->rot, x->ok ? 0xd0c080 : 0x807050, 0);
			break;
		}
		case E_AREA:
			if (x->acao == A_ALEAT_EQUIPE) {   /* COMMON/TEAMS: 5 cuadros en vertical, «1»-«4» y el vacío */
				const TzImg *eq = tz_ui_img(s, "COMMON/TEAMS", -1);
				int fr = (x->marcado + 4) % 5;
				if (eq && eq->h >= 5) tz_tela_imagem(t, eq, (TzRect){ 0, eq->h / 5 * fr, eq->w, eq->h / 5 }, r.x, r.y, 0);
				else {
					char b[4] = { x->marcado ? (char)('0' + x->marcado) : '-', 0 };
					tz_tela_preencher(t, r, 0xff1e1c16); tz_tela_moldura(t, r, 0xff907850);
					tz_ui_fonte_caixa(t, &s->f10, r, b, 0xd8c070, 1);
				}
			}
			else if (x->acao == A_ALEAT_IA) {   /* D470: COMMON/AI, dos cuadros en horizontal: apagado y encendido */
				const TzImg *ai = tz_ui_img(s, "COMMON/AI", -1);
				int fw = ai ? ai->w / 2 : 0;
				if (ai && fw > 0) tz_tela_imagem(t, ai, (TzRect){ x->marcado == 1 ? fw : 0, 0, fw, ai->h }, r.x, r.y, 0);
				else { tz_tela_preencher(t, r, x->marcado == 1 ? 0xff605020 : 0xff1e1c16); tz_tela_moldura(t, r, 0xff907850); tz_ui_fonte_caixa(t, &s->f10, r, "IA", 0xd8c070, 1); }
			} else if (x->acao == A_ALEAT_RENDIR) {   /* D470: AIRESIGN, 2x2: bandera blanca (permitida) o espadas; la 2.ª columna bajo el ratón */
				const TzImg *rs = tz_ui_img(s, "AIRESIGN", -1);
				int fw = rs ? rs->w / 2 : 0, fh = rs ? rs->h / 2 : 0;
				if (rs && fw > 0 && fh > 0) tz_tela_imagem(t, rs, (TzRect){ sobre ? fw : 0, x->marcado == 1 ? 0 : fh, fw, fh }, r.x, r.y, 0);
				else { tz_tela_preencher(t, r, x->marcado == 1 ? 0xffd0d0c0 : 0xff602018); tz_tela_moldura(t, r, 0xff907850); }
			} else if (x->arg == 0x300) {   /* D470: el fondo de «COMPORTAMIENTO IA» (D_CHOOSEAI/BACK, 500x320) y su título */
				tz_tela_preencher(t, (TzRect){ 0, 0, t->w, t->h }, 0x80000000);
				const TzImg *bk = tz_ui_img(s, "SCREENS/D_CHOOSEAI/BACK", -1);
				if (bk) tz_tela_imagem(t, bk, (TzRect){ 0, 0, 0, 0 }, ox + IA_X, oy + IA_Y, 0);
				else { tz_tela_preencher(t, (TzRect){ ox + IA_X, oy + IA_Y, 500, 320 }, 0xff282018); tz_tela_moldura(t, (TzRect){ ox + IA_X, oy + IA_Y, 500, 320 }, 0xff907850); }
				tz_ui_fonte_caixa(t, &s->f14b, (TzRect){ ox + IA_X + 44, oy + IA_Y + 48, 407, 17 }, x->rot, 0xe8d890, 1);
			}
			else if (x->arg == 1) tz_tela_preencher(t, r, 0xff1e1c16);
			else if (x->arg >= 0x100) tz_tela_preencher(t, r, 0xff000000u | s->cores[(x->arg - 0x100) & 7]);
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
		/* menu.rs description: la campaña en (408,45,328,250) y la misión en (406,352,332,145) */
		const TzMnItem *c = &m->campanhas[m->campanha_sel];
		tz_ui_fonte_caixa(t, &s->f14b, (TzRect){ ox + 408, oy + 45, 328, 20 }, c->titulo, 0xe8d890, 0);
		texto_caixa(t, &s->f12b, (TzRect){ ox + 408, oy + 70, 328, 225 }, c->descricao, 0xc8b070);
		if (m->missao_sel >= 0 && m->missao_sel < m->nmissoes && m->missoes[m->missao_sel].campanha == m->campanha_sel) {
			const TzMnItem *mi = &m->missoes[m->missao_sel];
			tz_ui_fonte_caixa(t, &s->f14b, (TzRect){ ox + 406, oy + 352, 332, 20 }, mi->titulo, 0xe8d890, 0);
			texto_caixa(t, &s->f12b, (TzRect){ ox + 406, oy + 374, 332, 123 }, mi->descricao, 0xc8b070);
		}
	}
	if (m->tela == TZ_MN_MAPAS && m->mapa_sel >= 0 && m->mapa_sel < m->nmapas) {
		tz_ui_fonte_caixa(t, &s->f14b, (TzRect){ ox + 420, oy + 50, 340, 20 }, m->mapas[m->mapa_sel].titulo, 0xe8d890, 0);
		previa(m, t, (TzRect){ ox + 430, oy + 72, 210, 210 });
		texto_caixa(t, &s->f12b, (TzRect){ ox + 80, oy + 352, 658, 146 }, m->mapas[m->mapa_sel].descricao, 0xc8b070);
	}
	if (m->tela == TZ_MN_CONFIG && m->mapa_sel >= 0 && m->mapa_sel < m->nmapas) previa(m, t, (TzRect){ ox + 570, oy + 120, 170, 170 });
	if (m->tela == TZ_MN_ALEAT) {
		char b[64];
		previa(m, t, (TzRect){ ox + PREVIA_X, oy + PREVIA_Y, PREVIA_L, PREVIA_L });
		if (m->ampliada) {
			tz_tela_preencher(t, (TzRect){ 0, 0, t->w, t->h }, 0xc0000000);
			previa(m, t, (TzRect){ ox + 190, oy + 90, 420, 420 });
		}
		snprintf(b, sizeof b, "%llu", m->aleat.semente);
		tz_ui_fonte_caixa(t, &s->f10, (TzRect){ ox + PREVIA_X, oy + PREVIA_Y + PREVIA_L + 2, PREVIA_L, 12 }, b, 0xa89860, 1);
	}
	if (m->tela == TZ_MN_FIM) desenhar_fim(m, t, ox, oy);
	if (m->tela == TZ_MN_AJUDA) {
		ler_ajuda(m);
		tz_ui_fonte_caixa(t, &s->f14b, (TzRect){ ox + 44, oy + 46, 406, 20 }, txt(m, "menu", "inst", "INSTRUCCIONES", (char[96]){ 0 }, 96), 0xe8d890, 1);
		int lh = altura_estilo(s, 2);
		for (int i = 0; i < AJUDA_LINHAS && i + m->rolo < m->najuda; ++i)
			tz_ui_fonte_texto(t, &s->f12b, ox + 48, oy + 88 + i * lh, m->ajuda[i + m->rolo], 0xd8c890);
		if (m->najuda > AJUDA_LINHAS) {
			char b[32];
			snprintf(b, sizeof b, "%d/%d", m->rolo + 1, m->najuda - AJUDA_LINHAS + 1);
			tz_ui_fonte_caixa(t, &s->f10, (TzRect){ ox + 380, oy + 236, 70, 14 }, b, 0x908060, 1);
		}
	}
	if (m->tela == TZ_MN_CREDITOS) {
		/* créditos animados: suben sin parar por la página derecha del libro (menu.rs CREDITS_PAGE) */
		ler_creditos(m);
		int total = 0;
		for (int i = 0; i < m->ncreditos; ++i) total += altura_estilo(s, m->creditos_estilo[i]);
		if (total > 0) {
			int desl = (int)((m->agora_ms - m->creditos_ms0) / 40) % (total + CRED_H);
			int y = oy + CRED_Y + CRED_H - desl;
			TzRect pg = { ox + CRED_X, oy + CRED_Y, CRED_W, CRED_H };
			for (int i = 0; i < m->ncreditos; ++i) {
				int e = m->creditos_estilo[i], h = altura_estilo(s, e);
				if (y >= pg.y && y + h <= pg.y + pg.h)
					tz_ui_fonte_caixa(t, fonte_estilo(s, e), (TzRect){ pg.x, y, pg.w, h }, m->creditos[i], e == 0 ? 0xf0d890 : e == 1 ? 0xe0c880 : 0xc0a868, 1);
				y += h;
			}
		}
	}
}
