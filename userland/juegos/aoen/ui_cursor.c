/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Cursor propio del juego (v8.5.5.3, D394): cambia según lo que hay debajo del ratón (atacar, talar, recoger, extraer,
 * construir, elegir un destino, desplazar la vista) y se dibuja en el cuadro, ampliado con la escala de la interfaz.
 * También el aro y el rótulo de lo que hay bajo el ratón en el mapa. Diseño propio: mapas de bits en texto. */
#include "interfaz.h"

/* Cada cursor es un dibujo de 16x16: '.' transparente, 'X' borde oscuro, 'o' claro, 'g' oro, 'w' madera, 'r' rojo,
 * 's' acero, 'v' verde. El punto activo (donde «apunta») va aparte. */
typedef struct { int hx, hy; const char *filas[16]; } DibujoCursor;

static const DibujoCursor dib[CUR_COUNT] = {
	[CUR_FLECHA] = { 1, 1, {
		"XX..............",
		"XoX.............",
		"XooX............",
		"XoooX...........",
		"XooooX..........",
		"XoooooX.........",
		"XooooooX........",
		"XoooooooX.......",
		"XooooooooX......",
		"XoooooggggX.....",
		"XoooXogXXXXX....",
		"XooXXooX........",
		"XoX..XogX.......",
		"XX...XooX.......",
		"......XogX......",
		"......XXX.......",
	} },
	[CUR_ATACAR] = { 1, 1, {
		"XXX.............",
		"XssX............",
		"XsosX...........",
		".XsosX..........",
		"..XsosX.........",
		"...XsosX........",
		"....XsosX.......",
		".....XsosX.XX...",
		"......XsosXgX...",
		".......XsosgX...",
		"........XsggX...",
		".......XggXwXX..",
		"......XgXXXwwX..",
		"......XX...XwrX.",
		"............XrrX",
		".............XX.",
	} },
	[CUR_MADERA] = { 3, 3, {
		"...XXXX.........",
		"..XsoooX........",
		".XsooooXX.......",
		"XsoooooXwX......",
		"XsoooXXwwwX.....",
		".XssX.XwwwX.....",
		"..XX...XwwwX....",
		"........XwwwX...",
		".........XwwwX..",
		"..........XwwwX.",
		"...........XwwX.",
		"............XX..",
		"................",
		"................",
		"................",
		"................",
	} },
	[CUR_ORO] = { 7, 2, {
		"...XXXXXXXX.....",
		".XXooooooooXX...",
		"XssXXXXwXXXssX..",
		"XsX...XwX...XsX.",
		"XX....XwX....XX.",
		"......XwX.......",
		"......XwX.......",
		"......XwX.......",
		"......XwX.......",
		"......XwX.......",
		"......XwX.......",
		"......XwX.......",
		"......XXX.......",
		"................",
		"................",
		"................",
	} },
	[CUR_COMIDA] = { 7, 8, {
		"................",
		".....XXXXXX.....",
		"....XwXXXXwX....",
		"...XwX....XwX...",
		"..XXX.XXX..XXX..",
		".XrrXXrrrXXvvX..",
		".XrorXrorXvvvX..",
		"XXXXXXXXXXXXXXX.",
		"XgwgwgwgwgwgwgX.",
		".XwgwgwgwgwgwX..",
		".XgwgwgwgwgwgX..",
		"..XwgwgwgwgwX...",
		"..XXXXXXXXXXX...",
		"................",
		"................",
		"................",
	} },
	[CUR_CONSTRUIR] = { 4, 4, {
		"..XXXXXX........",
		".XooooooX.......",
		"XsoooooosX......",
		"XssssssssX......",
		".XXXXwXXX.......",
		"....XwwX........",
		".....XwwX.......",
		"......XwwX......",
		".......XwwX.....",
		"........XwwX....",
		".........XwwX...",
		"..........XwwX..",
		"...........XwwX.",
		"............XX..",
		"................",
		"................",
	} },
};

static u32 color_de(char c, int tinte) {
	switch (c) {
	case 'X': return gfx_rgb(20, 12, 6);
	case 'o': return tinte ? (u32)tinte : gfx_rgb(250, 240, 210);
	case 'g': return gfx_rgb(214, 168, 64);
	case 'w': return gfx_rgb(150, 98, 48);
	case 'r': return gfx_rgb(200, 40, 32);
	case 's': return gfx_rgb(176, 184, 196);
	case 'v': return gfx_rgb(70, 160, 50);
	}
	return 0;
}

/* píxel ampliado s veces, en coordenadas reales */
static void punto(int x, int y, int s, u32 c) { gfx_fill(x, y, s, s, c); }

static void mapa(const DibujoCursor *d, int x, int y, int s, int tinte) {
	x -= d->hx * s; y -= d->hy * s;
	/* sombra suave abajo a la derecha */
	for (int f = 0; f < 16; ++f)
		for (int c = 0; c < 16; ++c)
			if (d->filas[f][c] != '.') gfx_fill_a(x + (c + 1) * s, y + (f + 1) * s, s, s, 0xff000000u, 70);
	for (int f = 0; f < 16; ++f)
		for (int c = 0; c < 16; ++c) {
			char ch = d->filas[f][c];
			if (ch != '.') punto(x + c * s, y + f * s, s, color_de(ch, tinte));
		}
}

/* aro de puntería (elegir un destino): círculo y cruz */
static void mira(int x, int y, int s, u32 col) {
	int r = 7 * s;
	for (int k = 0; k < 64; ++k) {
		float a = k * 6.2831853f / 64;
		int px = x + (int)(cosf(a) * r), py = y + (int)(sinf(a) * r);
		gfx_fill(px - s, py - s, 2 * s + 1, 2 * s + 1, gfx_rgb(20, 12, 6));
	}
	for (int k = 0; k < 64; ++k) {
		float a = k * 6.2831853f / 64;
		gfx_fill(x + (int)(cosf(a) * r), y + (int)(sinf(a) * r), s, s, col);
	}
	gfx_fill(x - 10 * s, y - s, 7 * s, 3 * s, gfx_rgb(20, 12, 6)); gfx_fill(x + 3 * s, y - s, 7 * s, 3 * s, gfx_rgb(20, 12, 6));
	gfx_fill(x - s, y - 10 * s, 3 * s, 7 * s, gfx_rgb(20, 12, 6)); gfx_fill(x - s, y + 3 * s, 3 * s, 7 * s, gfx_rgb(20, 12, 6));
	gfx_fill(x - 9 * s, y, 5 * s, s, col); gfx_fill(x + 4 * s, y, 5 * s, s, col);
	gfx_fill(x, y - 9 * s, s, 5 * s, col); gfx_fill(x, y + 4 * s, s, 5 * s, col);
	gfx_fill(x, y, s, s, col);
}

/* prohibido: círculo rojo con la barra */
static void prohibido(int x, int y, int s) {
	int r = 7 * s;
	for (int pasada = 0; pasada < 2; ++pasada) {
		u32 c = pasada ? gfx_rgb(230, 50, 40) : gfx_rgb(20, 12, 6);
		int g = pasada ? s : 2 * s;
		for (int k = 0; k < 72; ++k) {
			float a = k * 6.2831853f / 72;
			gfx_fill(x + (int)(cosf(a) * r) - g / 2, y + (int)(sinf(a) * r) - g / 2, g + (pasada ? s : 0), g + (pasada ? s : 0), c);
		}
		for (int k = -r + s; k < r - s; k += 1) gfx_fill(x + k * 7 / 10 - g / 2, y - k * 7 / 10 - g / 2, g + s, g + s, c);
	}
}

/* triángulo relleno (coordenadas reales), por filas */
static void triangulo(float ax, float ay, float bx, float by, float cx, float cy, u32 col) {
	int y0 = (int)floorf(MIN(ay, MIN(by, cy))), y1 = (int)ceilf(MAX(ay, MAX(by, cy)));
	float px[3] = { ax, bx, cx }, py[3] = { ay, by, cy };
	for (int y = y0; y <= y1; ++y) {
		float yc = y + 0.5f, xmin = 1e9f, xmax = -1e9f;
		for (int k = 0; k < 3; ++k) {
			float x0 = px[k], y0e = py[k], x1 = px[(k + 1) % 3], y1e = py[(k + 1) % 3];
			if ((yc < y0e) == (yc < y1e)) continue;
			float x = x0 + (yc - y0e) * (x1 - x0) / (y1e - y0e);
			xmin = MIN(xmin, x); xmax = MAX(xmax, x);
		}
		if (xmax >= xmin) gfx_fill((int)floorf(xmin), y, (int)ceilf(xmax) - (int)floorf(xmin) + 1, 1, col);
	}
}

/* flecha de desplazamiento hacia (dx, dy), en el borde de la pantalla: la punta en el punto activo */
static void flecha_borde(int x, int y, int s, int dx, int dy) {
	float len = sqrtf((float)(dx * dx + dy * dy));
	float ux = dx / len, uy = dy / len, px = -uy, py = ux;
	float l = 13.0f * s, w = 7.0f * s, g = 1.5f * s;
	/* borde oscuro (el triángulo agrandado) y relleno */
	triangulo(x + ux * g, y + uy * g, x - ux * (l + g) + px * (w + g), y - uy * (l + g) + py * (w + g), x - ux * (l + g) - px * (w + g), y - uy * (l + g) - py * (w + g), gfx_rgb(20, 12, 6));
	triangulo(x, y, x - ux * l + px * w, y - uy * l + py * w, x - ux * l - px * w, y - uy * l - py * w, COL_AVISO);
	triangulo(x - ux * 3 * s, y - uy * 3 * s, x - ux * l + px * w * 0.4f, y - uy * l + py * w * 0.4f, x - ux * l - px * w * 0.4f, y - uy * l - py * w * 0.4f, COL_GOLD);
}

void cursor_dibujar(int tipo, int x, int y, int s, int dx, int dy) {
	switch (tipo) {
	case CUR_ORO: mapa(&dib[CUR_ORO], x, y, s, (int)gfx_rgb(250, 214, 80)); break;
	case CUR_PIEDRA: mapa(&dib[CUR_ORO], x, y, s, (int)gfx_rgb(190, 190, 186)); break;
	case CUR_OBJETIVO: mira(x, y, s, COL_AVISO); break;
	case CUR_PROHIBIDO: prohibido(x, y, s); break;
	case CUR_DESPLAZAR: if (dx || dy) { flecha_borde(x, y, s, dx, dy); break; } /* sin dirección: la flecha normal */
		/* fall through */
	default: mapa(&dib[tipo >= 0 && tipo < CUR_COUNT && dib[tipo].filas[0] ? tipo : CUR_FLECHA], x, y, s, 0); break;
	}
}

/* ---------- aro y rótulo de lo que hay bajo el ratón ---------- */

/* contorno de elipse de un píxel de grueso (más s), en coordenadas reales */
void aro_elipse(int cx, int cy, int rx, int ry, u32 col, int grueso) {
	if (rx <= 0 || ry <= 0) return;
	int n = MAX(24, (rx + ry) * 2);
	for (int k = 0; k < n; ++k) {
		float a = k * 6.2831853f / n;
		gfx_fill(cx + (int)(cosf(a) * rx), cy + (int)(sinf(a) * ry), grueso, grueso, col);
	}
}
