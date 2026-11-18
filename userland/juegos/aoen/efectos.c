/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Efectos gráficos: configuración y perfiles, partículas, derrumbes de edificios y ciclo de día y noche. */
#include "ui.h"
#include "efectos.h"
#include "config.h"

Fx efx;

static const char *nombres[] = { "Equipos lentos", "Normal", "Completo", "Personalizado" };
const char *fx_nombre_perfil(int p) { return nombres[CLAMP(p, 0, 3)]; }

void fx_perfil(int p) {
	memset(&efx, 0, sizeof efx);
	efx.perfil = p;
	switch (p) {
	case FX_COMPLETO: efx.diaynoche = 1; efx.suave = 1; /* sin break: continúa */
	case FX_NORMAL: efx.transiciones = efx.agua = efx.niebla = efx.sombras = efx.particulas = efx.construccion = 1; break;
	default: break;   /* FX_LENTO: todo apagado, igual que el AoEN de antes */
	}
	efx.escala = cfg_get_int("fx_escala", 0);
}

void fx_init(void) {
	int p = cfg_get_int("fx_perfil", -1);
	/* sin configuración: normal si la CPU tiene SSE2 (las rutas rápidas de píxeles), lento si no (Pentium II/III antiguos) */
	if (p < 0 || p > 3) p = px_sse2() ? FX_NORMAL : FX_LENTO;
	const char *e = getenv("AOEN_EFECTOS");
	if (e && *e) p = CLAMP(atoi(e), 0, 2);
	fx_perfil(p);
	if (p == FX_PERSONAL) {
		efx.transiciones = cfg_get_int("fx_transiciones", 0); efx.agua = cfg_get_int("fx_agua", 0);
		efx.niebla = cfg_get_int("fx_niebla", 0); efx.sombras = cfg_get_int("fx_sombras", 0);
		efx.particulas = cfg_get_int("fx_particulas", 0); efx.construccion = cfg_get_int("fx_construccion", 0);
		efx.diaynoche = cfg_get_int("fx_diaynoche", 0); efx.suave = cfg_get_int("fx_suave", 0);
	}
	/* AOEN_FX=máscara de bits (1 transiciones, 2 agua, 4 niebla, 8 sombras, 16 partículas, 32 construcción, 64 día y noche, 128 zoom suave):
	 * para medir y depurar cada efecto por separado */
	const char *m = getenv("AOEN_FX");
	if (m && *m) {
		int b = atoi(m);
		efx.perfil = FX_PERSONAL;
		efx.transiciones = !!(b & 1); efx.agua = !!(b & 2); efx.niebla = !!(b & 4); efx.sombras = !!(b & 8);
		efx.particulas = !!(b & 16); efx.construccion = !!(b & 32); efx.diaynoche = !!(b & 64); efx.suave = !!(b & 128);
	}
	gfx_smooth = efx.suave;
	AOEN_LOG("[AOEN] efectos: perfil %s (transiciones=%d agua=%d niebla=%d sombras=%d particulas=%d construccion=%d diaynoche=%d suave=%d) sse2=%d",
		fx_nombre_perfil(efx.perfil), efx.transiciones, efx.agua, efx.niebla, efx.sombras, efx.particulas, efx.construccion, efx.diaynoche, efx.suave, px_sse2());
}

void fx_guardar(void) {
	cfg_set_int("fx_perfil", efx.perfil);
	cfg_set_int("fx_escala", efx.escala);
	cfg_set_int("fx_transiciones", efx.transiciones); cfg_set_int("fx_agua", efx.agua);
	cfg_set_int("fx_niebla", efx.niebla); cfg_set_int("fx_sombras", efx.sombras);
	cfg_set_int("fx_particulas", efx.particulas); cfg_set_int("fx_construccion", efx.construccion);
	cfg_set_int("fx_diaynoche", efx.diaynoche); cfg_set_int("fx_suave", efx.suave);
}

/* La interfaz se dibuja en un espacio lógico y se multiplica por un entero (píxeles nítidos): de 1600x960 hacia arriba, x2. */
int fx_escala_ui(int w, int h) {
	if (efx.escala == 1 || efx.escala == 2) return efx.escala;
	return (w >= 1600 && h >= 960) ? 2 : 1;
}

/* ---------- aleatorio propio: no toca la semilla del juego (las partidas guardadas no dependen de los efectos) ---------- */
static unsigned rs = 12345;
static float rnd(void) { rs = rs * 1664525u + 1013904223u; return (float)(rs >> 8) / 16777216.0f; }
static float rnd2(float a, float b) { return a + (b - a) * rnd(); }

/* ---------- partículas ---------- */

enum { P_HUMO, P_POLVO, P_FUEGO, P_ESCOMBRO, P_HOJA };
typedef struct { float x, y, z, vx, vy, vz, vida, max; u8 tipo, tam; } Part;
#define MAXP 480
static Part parts[MAXP];
static int np;

int fx_particulas_vivas(void) { return np; }

static int cupo(void) { return efx.perfil == FX_COMPLETO ? MAXP : 200; }

static void nueva(int tipo, float x, float y, float z, float vx, float vy, float vz, float vida, int tam) {
	if (np >= cupo()) return;
	Part *p = &parts[np++];
	p->x = x; p->y = y; p->z = z; p->vx = vx; p->vy = vy; p->vz = vz; p->vida = p->max = vida; p->tipo = (u8)tipo; p->tam = (u8)tam;
}

static void humo(float x, float y, float z, float fuerza) {
	nueva(P_HUMO, x + rnd2(-0.15f, 0.15f), y + rnd2(-0.15f, 0.15f), z, rnd2(-0.05f, 0.1f), rnd2(-0.1f, 0.05f), rnd2(14, 26) * fuerza, rnd2(1.4f, 2.4f), 3 + (int)(rnd() * 3));
}
static void polvo(float x, float y, int n, float radio) {
	for (int k = 0; k < n; ++k)
		nueva(P_POLVO, x + rnd2(-radio, radio), y + rnd2(-radio, radio), rnd2(0, 3), rnd2(-0.3f, 0.3f), rnd2(-0.3f, 0.3f), rnd2(4, 12), rnd2(0.6f, 1.1f), 2 + (int)(rnd() * 3));
}
static void escombros(float x, float y, int n, float radio) {
	for (int k = 0; k < n; ++k)
		nueva(P_ESCOMBRO, x + rnd2(-radio, radio), y + rnd2(-radio, radio), rnd2(4, 20), rnd2(-1.2f, 1.2f), rnd2(-1.2f, 1.2f), rnd2(25, 60), rnd2(0.7f, 1.3f), 1 + (int)(rnd() * 2));
}

/* ---------- derrumbes de edificios destruidos ---------- */

typedef struct { Entity e; float t; } Derrumbe;
#define MAXD 24
static Derrumbe der[MAXD];
static int nd;
#define DUR_DERRUMBE 2.2f

int fx_derrumbes(void) { return nd; }
const Entity *fx_derrumbe(int i, float *t) { *t = der[i].t / DUR_DERRUMBE; return &der[i].e; }

void fx_limpiar(void) { np = 0; nd = 0; }

void fx_evento(int ev, const Entity *e) {
	if (!efx.particulas && !efx.construccion) return;
	if (!W.fog) return;
	int fi = CLAMP((int)floorf(e->y), 0, W.h - 1) * W.w + CLAMP((int)floorf(e->x), 0, W.w - 1);
	if (W.fog[fi] != 2) return;   /* solo lo que se ve */
	switch (ev) {
	case FX_EV_EDIFICIO_ACABADO:
		if (efx.particulas) polvo(e->x, e->y, 14, e->size * 0.5f);
		break;
	case FX_EV_EDIFICIO_DESTRUIDO:
		if (efx.construccion && nd < MAXD) { der[nd].e = *e; der[nd].e.path = NULL; der[nd].e.alive = 1; der[nd].t = 0; ++nd; }
		if (efx.particulas) { polvo(e->x, e->y, 22, e->size * 0.6f); escombros(e->x, e->y, 16, e->size * 0.5f); }
		break;
	case FX_EV_UNIDAD_MUERE:
		if (efx.particulas) polvo(e->x, e->y, 4, 0.2f);
		break;
	case FX_EV_ARBOL_CAE:
		if (efx.particulas) { polvo(e->x, e->y, 5, 0.3f); for (int k = 0; k < 4; ++k) nueva(P_HOJA, e->x + rnd2(-0.2f, 0.2f), e->y + rnd2(-0.2f, 0.2f), rnd2(20, 40), rnd2(-0.4f, 0.4f), rnd2(-0.4f, 0.4f), rnd2(-6, 4), rnd2(0.8f, 1.4f), 2); }
		break;
	}
}

void fx_actualizar(float dt) {
	if (dt > 0.1f) dt = 0.1f;
	/* mover y envejecer */
	for (int i = 0; i < np; ) {
		Part *p = &parts[i];
		p->vida -= dt;
		if (p->vida <= 0) { parts[i] = parts[--np]; continue; }
		p->x += p->vx * dt; p->y += p->vy * dt; p->z += p->vz * dt;
		if (p->tipo == P_ESCOMBRO) { p->vz -= 90 * dt; if (p->z < 0) { p->z = 0; p->vz = 0; p->vx = p->vy = 0; } }
		else if (p->tipo == P_HOJA) p->vz = -8;
		else if (p->tipo == P_POLVO) { p->vx *= 0.96f; p->vy *= 0.96f; }
		++i;
	}
	for (int i = 0; i < nd; ) {
		der[i].t += dt;
		if (der[i].t >= DUR_DERRUMBE) { der[i] = der[--nd]; continue; }
		if (efx.particulas && rnd() < dt * 14) { humo(der[i].e.x + rnd2(-1, 1) * der[i].e.size * 0.4f, der[i].e.y + rnd2(-1, 1) * der[i].e.size * 0.4f, rnd2(0, 10), 1.2f); }
		if (efx.particulas && rnd() < dt * 6) polvo(der[i].e.x, der[i].e.y, 1, der[i].e.size * 0.5f);
		++i;
	}
	if (!efx.particulas || !W.nents || !W.fog) return;
	/* lo que sale del mundo: polvo de los que caminan, humo de los edificios dañados, polvo de las obras */
	int budget = 8;
	for (int i = 0; i < W.nents && budget > 0; ++i) {
		const Entity *e = &W.ents[i];
		if (!e->alive || e->state == ST_DYING) continue;
		int fx_ = CLAMP((int)floorf(e->x), 0, W.w - 1), fy_ = CLAMP((int)floorf(e->y), 0, W.h - 1);
		if (W.fog[fy_ * W.w + fx_] != 2) continue;
		if (is_unit(e->type)) {
			if (e->walking && e->type >= E_SCOUT && e->type <= E_HEAVYCAV) {   /* los jinetes levantan polvo; el resto de unidades, sólo al correr muchos */
				if (rnd() < dt * 5) { polvo(e->x, e->y, 1, 0.1f); --budget; }
			} else if (e->walking && rnd() < dt * 1.5f) { polvo(e->x, e->y, 1, 0.08f); --budget; }
		} else if (is_building(e->type)) {
			if (e->progress < 1 && e->builders > 0 && rnd() < dt * 4) { polvo(e->x, e->y, 1, e->size * 0.45f); if (rnd() < 0.3f) escombros(e->x, e->y, 1, e->size * 0.4f); --budget; }
			else if (e->progress >= 1 && e->hp < e->maxhp * 0.66f) {
				float dano = 1.0f - e->hp / e->maxhp;
				if (rnd() < dt * (2 + 8 * dano)) { humo(e->x + rnd2(-0.3f, 0.3f) * e->size, e->y + rnd2(-0.3f, 0.3f) * e->size, 22 + 12 * e->size * 0.3f, 0.8f + dano); --budget; }
			}
		}
	}
}

/* ---------- dibujo de partículas ---------- */

void fx_dibujar(void) {
	for (int i = 0; i < np; ++i) {
		const Part *p = &parts[i];
		int sx, sy;
		view_to_screen(p->x, p->y, &sx, &sy);
		sy -= (int)(p->z * V.zoom);
		if (sx < -20 || sx > V.sw + 20 || sy < -20 || sy > V.sh + 20) continue;
		float k = p->vida / p->max;   /* 1 al nacer, 0 al morir */
		int r = MAX(1, (int)(p->tam * V.zoom));
		switch (p->tipo) {
		case P_HUMO: {
			int rr = r + (int)((1 - k) * 6 * V.zoom);
			int g = 70 + (int)(60 * (1 - k));
			gfx_ellipse(sx, sy, rr, MAX(1, rr * 3 / 4), gfx_rgb(g, g, g + 5), (int)(110 * k));
			break;
		}
		case P_POLVO:
			gfx_ellipse(sx, sy, r + (int)((1 - k) * 3 * V.zoom), MAX(1, (r + (int)((1 - k) * 3 * V.zoom)) / 2), gfx_rgb(196, 170, 120), (int)(90 * k));
			break;
		case P_FUEGO:
			gfx_ellipse(sx, sy, r, MAX(1, r), gfx_rgb(255, 150 + (int)(90 * (1 - k)), 30), (int)(200 * k));
			break;
		case P_ESCOMBRO:
			gfx_fill(sx, sy, r, r, gfx_rgb(96, 70, 44));
			break;
		case P_HOJA:
			gfx_fill(sx, sy, r, r, gfx_rgb(60, 130, 40));
			break;
		}
	}
}

/* ---------- día y noche ---------- */

#define CICLO 600.0f   /* segundos de juego de un día completo */

float fx_luz_dia(void) {
	if (!efx.diaynoche) return 1.0f;
	float t = fmodf(W.time / CICLO, 1.0f);
	return 0.5f + 0.5f * cosf(t * 6.2831853f);   /* t=0: mediodía; t=0.5: medianoche */
}

void fx_luz(int y0, int y1) {
	if (!efx.diaynoche) return;
	float l = fx_luz_dia();
	float noche = 1.0f - l;
	if (noche < 0.06f) return;
	int h = y1 - y0;
	if (h <= 0) return;
	/* atardecer y amanecer: naranja al principio de la caída de luz; noche: azul oscuro */
	float t = fmodf(W.time / CICLO, 1.0f);
	float calido = 1.0f - fabsf(noche - 0.45f) / 0.45f;
	if (calido < 0) calido = 0;
	int ar = (int)(18 + 160 * calido * (t < 0.5f ? 1.0f : 0.8f)), ag = (int)(26 + 60 * calido), ab = (int)(70 - 40 * calido);
	int a = (int)(noche * 130);
	gfx_clip(0, 0, fb.w, y1);
	gfx_fill_a(0, y0, fb.w, h, gfx_rgb(ar, ag, ab), a);
	gfx_clip_reset();
}
