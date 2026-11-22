/* SPDX-License-Identifier: MIT */
/* entrada.c - entrada por acciones. Un juego no pregunta por teclas sino por acciones («saltar», «izquierda») que el
 * proyecto liga a teclas, botones del ratón y botones o ejes de mandos (capibara.cfg, sección [acciones]).
 * Ligaduras:  tecla:Left  tecla:Space  raton:izq  mando:a  mando:izquierda  eje:leftx-  eje:lefty+ */
#include "capi.h"
#include <ctype.h>
#include <strings.h>

Accion g_acc[ACC_MAX]; int g_nacc;
int g_raton_x, g_raton_y, g_raton_pulso_x, g_raton_pulso_y; int g_en_fisica;

#define DOWN_MAX 48
static char teclas[DOWN_MAX][32]; static int nteclas;
static char botones_mando[DOWN_MAX][24]; static int nbm;
static int raton_b;                       /* máscara de botones del ratón */
static struct { char nome[24]; float v; } ejes[8]; static int nejes;
static Evento cola[64]; static int cola_n;

static void norm(const char *s, char *o, size_t no) {
	size_t n = 0;
	for (; *s && n < no - 1; ++s) o[n++] = *s == ' ' ? '_' : (char)tolower((unsigned char)*s);
	o[n] = 0;
	static const char *const al[][2] = { { "izquierda", "left" }, { "derecha", "right" }, { "arriba", "up" }, { "abajo", "down" }, { "espacio", "space" },
	                                      { "entrar", "return" }, { "escape", "escape" }, { "mayus", "left_shift" } };
	for (unsigned i = 0; i < sizeof al / sizeof al[0]; ++i) if (!strcmp(o, al[i][0])) snprintf(o, no, "%s", al[i][1]);
}

int accion_declarar(const char *nome, const char *lig) {
	Accion *a = NULL;
	for (int i = 0; i < g_nacc; ++i) if (!strcmp(g_acc[i].nome, nome)) a = &g_acc[i];
	if (!a) { if (g_nacc >= ACC_MAX) return -1; a = &g_acc[g_nacc++]; memset(a, 0, sizeof *a); snprintf(a->nome, sizeof a->nome, "%s", nome); }
	a->nl = 0;
	char b[256]; snprintf(b, sizeof b, "%s", lig);
	for (char *t = strtok(b, " \t"); t && a->nl < 8; t = strtok(NULL, " \t")) {
		char *c = strchr(t, ':'); if (!c) continue;
		*c = 0; Ligadura *l = &a->l[a->nl];
		if (!strcmp(t, "tecla")) { l->tipo = 't'; norm(c + 1, l->nome, sizeof l->nome); }
		else if (!strcmp(t, "raton")) { l->tipo = 'r'; norm(c + 1, l->nome, sizeof l->nome); }
		else if (!strcmp(t, "mando")) { l->tipo = 'b'; norm(c + 1, l->nome, sizeof l->nome); }
		else if (!strcmp(t, "eje")) {
			l->tipo = 'e'; norm(c + 1, l->nome, sizeof l->nome);
			size_t n = strlen(l->nome); l->umbral = 0.5f;
			if (n && (l->nome[n - 1] == '-' || l->nome[n - 1] == '+')) { l->umbral = l->nome[n - 1] == '-' ? -0.5f : 0.5f; l->nome[n - 1] = 0; }
		} else continue;
		++a->nl;
	}
	return 0;
}

static int ligadura_activa(const Ligadura *l, float *fuerza) {
	switch (l->tipo) {
	case 't': for (int i = 0; i < nteclas; ++i) if (!strcmp(teclas[i], l->nome)) { *fuerza = 1; return 1; } return 0;
	case 'b': for (int i = 0; i < nbm; ++i) if (!strcmp(botones_mando[i], l->nome)) { *fuerza = 1; return 1; } return 0;
	case 'r': { int b = !strcmp(l->nome, "izq") || !strcmp(l->nome, "1") ? 1 : !strcmp(l->nome, "central") || !strcmp(l->nome, "2") ? 2 : 3; if (raton_b & (1 << b)) { *fuerza = 1; return 1; } return 0; }
	case 'e':
		for (int i = 0; i < nejes; ++i) if (!strcmp(ejes[i].nome, l->nome)) {
			float v = ejes[i].v;
			if ((l->umbral < 0 && v <= l->umbral) || (l->umbral > 0 && v >= l->umbral)) { *fuerza = v < 0 ? -v : v; return 1; }
		}
		return 0;
	}
	return 0;
}

static void poner_evento(const char *tipo, const char *nome, int abajo, int x, int y, int boton) {
	if (cola_n >= 64) return;
	Evento *e = &cola[cola_n++]; memset(e, 0, sizeof *e);
	snprintf(e->tipo, sizeof e->tipo, "%s", tipo); snprintf(e->nome, sizeof e->nome, "%s", nome); e->abajo = abajo; e->x = x; e->y = y; e->boton = boton;
}

/* recalcula las acciones y marca los flancos */
void entrada_actualizar(void) {
	for (int i = 0; i < g_nacc; ++i) {
		Accion *a = &g_acc[i]; float f = 0; int p = 0;
		for (int k = 0; k < a->nl; ++k) { float ff = 0; if (ligadura_activa(&a->l[k], &ff)) { p = 1; if (ff > f) f = ff; } }
		if (p && !a->pres) { a->rec_pres |= 3; poner_evento("accion", a->nome, 1, 0, 0, 0); }
		if (!p && a->pres) { a->rec_sol |= 3; poner_evento("accion", a->nome, 0, 0, 0, 0); }
		a->pres = p; a->fuerza_x1000 = (int)(f * 1000);
	}
}

void entrada_tecla(const char *nome, int abajo) {
	char n[32]; norm(nome, n, sizeof n);
	int idx = -1;
	for (int i = 0; i < nteclas; ++i) if (!strcmp(teclas[i], n)) idx = i;
	if (abajo && idx < 0 && nteclas < DOWN_MAX) { snprintf(teclas[nteclas++], 32, "%s", n); poner_evento("tecla", n, 1, 0, 0, 0); }
	else if (!abajo && idx >= 0) { memmove(teclas[idx], teclas[idx + 1], 32 * (size_t)(nteclas - idx - 1)); --nteclas; poner_evento("tecla", n, 0, 0, 0, 0); }
	entrada_actualizar();
}
void entrada_raton_boton(int b, int abajo) {
	if (abajo) { raton_b |= 1 << b; g_raton_pulso_x = g_raton_x; g_raton_pulso_y = g_raton_y; } else raton_b &= ~(1 << b);
	poner_evento("raton", "", abajo, g_raton_x, g_raton_y, b);
	entrada_actualizar();
}
void entrada_raton_pos(int x, int y) { poner_evento("raton_mov", "", 0, x, y, 0); g_raton_x = x; g_raton_y = y; }
void entrada_mando_boton(const char *nome, int abajo) {
	char n[24]; norm(nome, n, sizeof n); int idx = -1;
	for (int i = 0; i < nbm; ++i) if (!strcmp(botones_mando[i], n)) idx = i;
	if (abajo && idx < 0 && nbm < DOWN_MAX) snprintf(botones_mando[nbm++], 24, "%s", n);
	else if (!abajo && idx >= 0) { memmove(botones_mando[idx], botones_mando[idx + 1], 24 * (size_t)(nbm - idx - 1)); --nbm; }
	entrada_actualizar();
}
void entrada_mando_eje(const char *nome, float v) {
	char n[24]; norm(nome, n, sizeof n);
	for (int i = 0; i < nejes; ++i) if (!strcmp(ejes[i].nome, n)) { ejes[i].v = v; entrada_actualizar(); return; }
	if (nejes < 8) { snprintf(ejes[nejes].nome, 24, "%s", n); ejes[nejes++].v = v; entrada_actualizar(); }
}

void entrada_pulsa(const char *nome, int abajo) {
	/* para las pruebas: simula una acción pulsada sin pasar por teclas (se liga a una tecla interna) */
	char t[40]; snprintf(t, sizeof t, "__a_%s", nome);
	Accion *a = NULL; for (int i = 0; i < g_nacc; ++i) if (!strcmp(g_acc[i].nome, nome)) a = &g_acc[i];
	if (!a) return;
	int tiene = 0; for (int k = 0; k < a->nl; ++k) if (!strcmp(a->l[k].nome, t)) tiene = 1;
	if (!tiene && a->nl < 8) { a->l[a->nl].tipo = 't'; snprintf(a->l[a->nl].nome, sizeof a->l[0].nome, "%s", t); ++a->nl; }
	entrada_tecla(t, abajo);
}

static Accion *buscar(const char *n) { for (int i = 0; i < g_nacc; ++i) if (!strcmp(g_acc[i].nome, n)) return &g_acc[i]; return NULL; }
int accion_presionada(const char *n) { Accion *a = buscar(n); return a && a->pres; }
int accion_recien(const char *n) { Accion *a = buscar(n); return a && (a->rec_pres & (g_en_fisica ? 1 : 2)); }
int accion_soltada(const char *n) { Accion *a = buscar(n); return a && (a->rec_sol & (g_en_fisica ? 1 : 2)); }
float accion_fuerza(const char *n) { Accion *a = buscar(n); return a ? a->fuerza_x1000 / 1000.0f : 0; }
float entrada_eje(const char *neg, const char *pos) { return accion_fuerza(pos) - accion_fuerza(neg); }

void entrada_fin_paso_fisica(void) { for (int i = 0; i < g_nacc; ++i) { g_acc[i].rec_pres &= ~1; g_acc[i].rec_sol &= ~1; } }
void entrada_nuevo_cuadro(void) { for (int i = 0; i < g_nacc; ++i) { g_acc[i].rec_pres &= ~2; g_acc[i].rec_sol &= ~2; } }

void entrada_enviar_scripts(Nodo *raiz) {
	int n = cola_n; cola_n = 0;
	static Evento tmp[64]; memcpy(tmp, cola, sizeof(Evento) * (size_t)n);
	for (int i = 0; i < n; ++i) guion_enviar_evento(raiz, &tmp[i]);
}

int entrada_boton_raton(int b) { return (raton_b >> b) & 1; }
