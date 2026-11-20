/* SPDX-License-Identifier: GPL-3.0-or-later */
/* animacao.c - tiempos de animación, pájaros en vuelo y paso del reloj de la escena (D417, fase 3).
 *
 * Traducido de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later):
 *   src/animation.rs  Timeline: directional, stepped, uniform, frame (tz_tempo_*)
 *   src/flight.rs     Flyer: new, advance, step, turn, manoeuvre, heading (TzVoo; sin buscador de caminos de vuelo)
 * El motor original (Tzar.exe 1.11) tiene dos formas de temporizar un dibujo: las hojas con direcciones (8 o 24
 * columnas) llevan un registro de animación (`anim_time`, 1000 ms por defecto redondeado a múltiplo de las filas, y
 * `delay` por fila, rellenados de 1 ms en 1 ms fila a fila hasta llegar a anim_time) y las de una columna avanzan
 * con un temporizador de 1000 / fps ms.
 */
#include "tzar.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ tiempos */

int tz_tempo_novo(TzTempo *t, const uint64_t *atrasos, int n)
{
	memset(t, 0, sizeof *t);
	if (n < 1) n = 1;
	t->fins = tz_malloc((size_t)n * sizeof *t->fins);
	if (!t->fins) return -1;
	uint64_t d = 0;
	for (int i = 0; i < n; ++i) {
		uint64_t a = atrasos ? atrasos[i] : 1;
		d += a > 1 ? a : 1;
		t->fins[i] = d;
	}
	t->n = n;
	t->duracao = d;
	return 0;
}

int tz_tempo_uniforme(TzTempo *t, int rows, uint64_t periodo)
{
	if (rows < 1) rows = 1;
	if (rows > 100000) return tz_falha("demasiadas filas");
	uint64_t *a = tz_malloc((size_t)rows * sizeof *a);
	if (!a) return -1;
	for (int i = 0; i < rows; ++i) a[i] = periodo > 1 ? periodo : 1;
	int r = tz_tempo_novo(t, a, rows);
	free(a);
	return r;
}

static int numero(const TzIni *ini, const char *secao, const char *chave, uint64_t *v)
{
	const char *s = tz_ini(ini, secao, chave);
	if (!s) return 0;
	while (*s == ' ' || *s == '\t') ++s;
	if (*s < '0' || *s > '9') return 0;
	uint64_t x = 0;
	while (*s >= '0' && *s <= '9') { x = x * 10 + (uint64_t)(*s - '0'); if (x > 1000000000ull) x = 1000000000ull; ++s; }
	*v = x;
	return 1;
}

int tz_tempo_direcional(TzTempo *t, const TzIni *ini, const char *secao, int rows)
{
	if (rows < 1) rows = 1;
	if (rows > 100000) return tz_falha("demasiadas filas");
	uint64_t anim = 1000;
	numero(ini, secao, "anim_time", &anim);
	uint64_t por_fila = anim / (uint64_t)rows, total = por_fila * (uint64_t)rows;
	uint64_t *at = tz_malloc((size_t)rows * sizeof *at);
	if (!at) return -1;
	/* `delay`: números separados por comas; vacíos y lo que no es número se saltan (como ReTzar) */
	int nd = 0;
	const char *s = tz_ini(ini, secao, "delay");
	uint64_t *lst = tz_malloc((size_t)(s ? strlen(s) / 2 + 2 : 1) * sizeof *lst);
	if (!lst) { free(at); return -1; }
	while (s && *s) {
		const char *f = strchr(s, ',');
		size_t len = f ? (size_t)(f - s) : strlen(s);
		char b[32];
		snprintf(b, sizeof b, "%.*s", (int)(len < 31 ? len : 31), s);
		char *p = b;
		while (*p == ' ' || *p == '\t') ++p;
		char *fin = p;
		uint64_t v = 0;
		while (*fin >= '0' && *fin <= '9') { v = v * 10 + (uint64_t)(*fin - '0'); if (v > 1000000000ull) v = 1000000000ull; ++fin; }
		while (*fin == ' ' || *fin == '\t') ++fin;
		if (fin != p && !*fin) lst[nd++] = v;
		if (!f) break;
		s = f + 1;
	}
	if (nd == rows) memcpy(at, lst, (size_t)rows * sizeof *at);
	else for (int i = 0; i < rows; ++i) at[i] = por_fila;   /* cuenta equivocada: filas iguales */
	free(lst);
	uint64_t suma = 0;
	for (int i = 0; i < rows; ++i) suma += at[i];
	int fila = 0;
	while (suma < total) { at[fila]++; fila = (fila + 1) % rows; suma++; }
	int r = tz_tempo_novo(t, at, rows);
	free(at);
	return r;
}

int tz_tempo_passos(TzTempo *t, const TzIni *ini, const char *secao, int rows, int fps_padrao)
{
	memset(t, 0, sizeof *t);
	uint64_t fps = 0;
	if (!numero(ini, secao, "fps", &fps) || !fps) fps = (uint64_t)(fps_padrao > 0 ? fps_padrao : 0);
	if (rows <= 1 || !fps) return -1;
	return tz_tempo_uniforme(t, rows, 1000 / fps);
}

uint64_t tz_tempo_inicio(const TzTempo *t, int k)
{
	if (k <= 0 || !t->n) return 0;
	return k - 1 < t->n ? t->fins[k - 1] : 0;
}

uint64_t tz_tempo_atraso(const TzTempo *t, int k)
{
	return k >= 0 && k < t->n ? t->fins[k] - tz_tempo_inicio(t, k) : 0;
}

int tz_tempo_quadro(const TzTempo *t, uint64_t decorrido, uint64_t fase)
{
	if (!t->n || !t->duracao) return 0;
	uint64_t x = (decorrido % t->duracao + fase % t->duracao) % t->duracao;
	int a = 0, b = t->n;   /* partition_point(end <= x) */
	while (a < b) {
		int m = (a + b) / 2;
		if (t->fins[m] <= x) a = m + 1; else b = m;
	}
	return a < t->n ? a : t->n - 1;
}

void tz_tempo_liberar(TzTempo *t)
{
	free(t->fins);
	memset(t, 0, sizeof *t);
}

/* ------------------------------------------------------------------ pájaros */

static const int PASSOS[8][2] = { { 0, 1 }, { 1, 1 }, { 1, 0 }, { 1, -1 }, { 0, -1 }, { -1, -1 }, { -1, 0 }, { -1, 1 } };

static int rumo(int dx, int dy)
{
	int sx = (dx > 0) - (dx < 0), sy = (dy > 0) - (dy < 0);
	for (int i = 0; i < 8; ++i) if (PASSOS[i][0] == sx && PASSOS[i][1] == sy) return i;
	return 0;
}

static uint32_t aleatorio(TzVoo *v)
{
	uint32_t x = v->aleatorio;
	x ^= x << 13; x ^= x >> 17; x ^= x << 5;
	v->aleatorio = x;
	return x;
}

static int div_floor(int a, int b) { int q = a / b; return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q; }

/* La vuelta del que gira más de 45 grados (0x4edc80): 22 puntos en casillas, de dos plantillas (A y B) volteadas o
 * giradas según el rumbo de salida y el de llegada. */
static void gira(const int ab[22][2], int op, int out[22][2])
{
	for (int i = 0; i < 22; ++i) {
		int x = ab[i][0], y = ab[i][1], t;
		switch (op) {
		case 0: break;                              /* identidad */
		case 1: x = -x; y = -y; break;              /* both */
		case 2: y = -y; break;                      /* flip_y */
		case 3: x = -x; break;                      /* flip_x */
		case 4: t = x; x = -y; y = t; break;        /* quarter */
		case 5: t = x; x = y; y = -t; break;        /* three */
		case 6: y = -y; t = x; x = y; y = -t; break;        /* three(flip_y) */
		case 7: t = x; x = -y; y = t; y = -y; break;        /* flip_y(quarter) */
		case 8: y = -y; t = x; x = -y; y = t; break;        /* quarter(flip_y) */
		case 9: t = x; x = y; y = -t; y = -y; break;        /* flip_y(three) */
		}
		out[i][0] = x; out[i][1] = y;
	}
}

static void manobra(int de, int a, int out[22][2])
{
	static const int A[22][2] = { { 0, -1 }, { 0, -2 }, { 0, -3 }, { 0, -4 }, { 0, -5 }, { 0, -6 }, { 1, -7 }, { 2, -8 }, { 3, -8 }, { 4, -8 }, { 5, -8 },
	                              { 6, -7 }, { 7, -6 }, { 7, -5 }, { 7, -4 }, { 6, -3 }, { 5, -2 }, { 4, -2 }, { 3, -2 }, { 2, -2 }, { 1, -1 }, { 0, 0 } };
	static const int B[22][2] = { { -1, -1 }, { -2, -2 }, { -3, -2 }, { -4, -2 }, { -5, -2 }, { -6, -3 }, { -7, -4 }, { -7, -5 }, { -7, -6 }, { -6, -7 }, { -5, -8 },
	                              { -4, -8 }, { -3, -8 }, { -2, -8 }, { -1, -7 }, { 0, -6 }, { 0, -5 }, { 0, -4 }, { 0, -3 }, { 0, -2 }, { 0, -1 }, { 0, 0 } };
#define PERTO(a0, a1) (a >= (a0) && a <= (a1))
	switch (de) {
	case 0: gira(A, PERTO(2, 4) ? 1 : 2, out); break;
	case 1: gira(B, PERTO(3, 5) ? 1 : 6, out); break;
	case 2: gira(A, PERTO(4, 6) ? 4 : 7, out); break;
	case 3: gira(B, PERTO(5, 7) ? 4 : 3, out); break;
	case 4: gira(A, PERTO(1, 2) ? 3 : 0, out); break;
	case 5: gira(B, PERTO(2, 3) ? 8 : 0, out); break;
	case 6: gira(A, PERTO(0, 2) ? 5 : 9, out); break;
	default: gira(B, PERTO(1, 3) ? 5 : 2, out); break;
	}
#undef PERTO
}

/* Hacia la columna del rumbo (tres por rumbo en las de 24): una columna de golpe, dos a un cuarto de la casilla y la
 * del rumbo a la mitad (0x4edb10). */
static void vira(TzVoo *v)
{
	int columns = v->ncol > 0 ? v->ncol : 1;
	int meta = v->direcao * columns / 8, c = v->col, p, q;
	v->passos_casa = 0;
	v->batidas = 0;
	if (meta == c) { p = c; q = c; }
	else if (c == 0 && meta + 3 == columns) { p = columns - 1; q = columns - 2; }
	else if (meta == 0 && c + 3 == columns) { p = columns - 2; q = columns - 1; }
	else if (meta > c) { p = c + 1; q = c + 2 < columns - 1 ? c + 2 : columns - 1; }
	else { int cc = c > 2 ? c : 2; p = cc - 1; q = cc - 2; }
	v->col = p;
	v->volta[0] = q; v->volta[1] = meta;
	v->tem_volta = 1;
}

static void passo(TzVoo *v)
{
	int baixo = v->faixa[0], alto = v->faixa[1] > v->faixa[0] ? v->faixa[1] : v->faixa[0];
	int chegou = 0;
	if (v->nrota && v->rota[0][0] == v->pos[0] && v->rota[0][1] == v->pos[1]) {
		memmove(v->rota[0], v->rota[1], (size_t)(v->nrota - 1) * sizeof v->rota[0]);
		v->nrota--;
		chegou = 1;
	}
	int alvo[2] = { v->alvo[0], v->alvo[1] };
	if (!(v->tem_alvo && (v->alvo[0] != v->pos[0] || v->alvo[1] != v->pos[1]))) {
		int primeiro = (baixo + 31) / 32, ultimo = alto / 32, span = ultimo - primeiro + 1;
		if (span < 1) span = 1;
		alvo[0] = (primeiro + (int)(aleatorio(v) % (uint32_t)span)) * 32;
		alvo[1] = (primeiro + (int)(aleatorio(v) % (uint32_t)span)) * 32;
		v->alvo[0] = alvo[0]; v->alvo[1] = alvo[1]; v->tem_alvo = 1;
		int novo = rumo(alvo[0] - v->pos[0], alvo[1] - v->pos[1]);
		int giro = v->direcao > novo ? v->direcao - novo : novo - v->direcao;
		if (giro > 1 && giro != 7) {
			int cel[2] = { div_floor(v->pos[0], 32), div_floor(v->pos[1], 32) }, m[22][2];
			manobra(v->direcao, novo, m);
			v->nrota = 0;
			for (int i = 0; i < 22; ++i) {
				int x = (cel[0] + m[i][0]) * 32, y = (cel[1] + m[i][1]) * 32;
				if (x >= baixo && x <= alto && y >= baixo && y <= alto) { v->rota[v->nrota][0] = x; v->rota[v->nrota][1] = y; v->nrota++; }
			}
		}
	}
	int prox[2] = { alvo[0], alvo[1] };
	if (v->nrota) { prox[0] = v->rota[0][0]; prox[1] = v->rota[0][1]; }
	int dir = rumo(prox[0] - v->pos[0], prox[1] - v->pos[1]);
	int nova_casa = chegou || v->passos_casa >= 32;   /* sin camino de vuelo: una casilla son 32 pasos */
	if (dir != v->direcao) { v->direcao = dir; vira(v); }
	else if (nova_casa) { v->passos_casa = 0; v->batidas = 0; v->tem_volta = 0; }
	v->pos[0] += PASSOS[v->direcao][0];
	v->pos[1] += PASSOS[v->direcao][1];
	v->passos_casa++;
	v->passos++;
	if (v->passos >= v->passos_quadro) {
		v->passos = 0;
		int ticks = 32 / v->passos_quadro;
		if (ticks < 1) ticks = 1;
		if (v->tem_volta) {
			if (v->batidas == ticks / 4) v->col = v->volta[0];
			else if (v->batidas == ticks / 2) v->col = v->volta[1];
		}
		int rows = v->nfil > 0 ? v->nfil : 1;
		if (v->batidas % 2 == 1 && (v->fil != 0 || aleatorio(v) % 701 <= 100)) v->fil = (v->fil + 1) % rows;
		v->batidas++;
	}
}

/* ------------------------------------------------------------------ el reloj de la escena */

void tz_animar(TzEscena *e, uint64_t ms)
{
	for (int i = 0; i < e->nanimv; ++i) {
		TzVisual *v = &e->v[e->animv[i]];
		if (v->anim >= 0) {
			const TzAnim *a = &e->anims[v->anim];
			int k = tz_tempo_quadro(&a->t, ms, a->fase);
			if (k >= 0 && k < a->nq && a->q[k] >= 0) v->img = a->q[k];
		}
	}
	for (int i = 0; i < e->nvoos; ++i) {
		TzVoo *f = &e->voos[i];
		uint64_t d = ms > f->ultimo_ms ? ms - f->ultimo_ms : 0;
		f->ultimo_ms = ms;
		f->pendente_ms += d;
		if (f->pendente_ms > 2000) f->pendente_ms = 2000;   /* una pausa larga no repite minutos de vuelo */
		while (f->pendente_ms >= (uint64_t)f->passo_ms) { f->pendente_ms -= (uint64_t)f->passo_ms; passo(f); }
		int col = f->col < f->ncol ? f->col : 0, fil = f->fil < f->nfil ? f->fil : 0;
		TzVisual *b = &e->v[f->corpo];
		b->x = f->pos[0] + f->desvio[0];
		b->y = f->pos[1] + f->desvio[1];
		if (f->fq[col * f->nfil + fil] >= 0) b->img = f->fq[col * f->nfil + fil];
		if (f->sombra >= 0) {
			TzVisual *s = &e->v[f->sombra];
			s->x = b->x + f->elevacao[0];
			s->y = b->y + f->elevacao[1];
			if (f->fs && f->fs[col * f->nfil + fil] >= 0) s->img = f->fs[col * f->nfil + fil];
		}
	}
}

void tz_voo_iniciar(TzVoo *v, int ax, int ay, int coluna)
{
	int cols = v->ncol > 0 ? v->ncol : 1;
	v->coluna = coluna % cols;
	v->pos[0] = ax; v->pos[1] = ay;
	v->inicio[0] = ax; v->inicio[1] = ay;
	v->col = v->coluna;
	v->direcao = (v->coluna * 8 + cols / 2) / cols % 8;
	v->aleatorio = v->semente | 1;
}
