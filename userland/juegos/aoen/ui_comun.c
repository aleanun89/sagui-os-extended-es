/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Controles de la interfaz (v8.5.4.7): botones, selectores, ayudas emergentes, iconos, texturas de los paneles con los
 * gráficos originales y la comprobación de solapes que usan las pruebas. */
#include "interfaz.h"
#include "efectos.h"

App app = { SC_MENU, SC_MENU, SC_MENU, 0, 1, 0, 0, 2, 1, 0, 1 };
Entrada in;

int tecla(int sc) { return sc >= 0 && sc < 512 && in.key[sc]; }
int mayus(void) { return in.keys && (in.keys[SDL_SCANCODE_LSHIFT] || in.keys[SDL_SCANCODE_RSHIFT]); }
int ctrl(void) { return in.keys && (in.keys[SDL_SCANCODE_LCTRL] || in.keys[SDL_SCANCODE_RCTRL]); }
int mod_down(void) { return mayus() || ctrl(); }

/* ---------- paneles modales: lo que queda debajo no recibe el ratón ---------- */
Rect ui_modal;
int ui_modal_on, ui_en_modal;

int hit(int x, int y, int w, int h) {
	int s = app.us;
	if (ui_modal_on && !ui_en_modal && in.mx >= ui_modal.x * s && in.my >= ui_modal.y * s && in.mx < (ui_modal.x + ui_modal.w) * s && in.my < (ui_modal.y + ui_modal.h) * s) return 0;
	return in.mx >= x * s && in.my >= y * s && in.mx < (x + w) * s && in.my < (y + h) * s;
}
int hit_r(Rect r) { return hit(r.x, r.y, r.w, r.h); }

/* ---------- dibujo básico ---------- */

void bevel(int x, int y, int w, int h, u32 border) {
	gfx_fill(x + 1, y, w - 2, 1, border); gfx_fill(x + 1, y + h - 1, w - 2, 1, border);
	gfx_fill(x, y + 1, 1, h - 2, border); gfx_fill(x + w - 1, y + 1, 1, h - 2, border);
}

void panel(int x, int y, int w, int h) {
	fondo_dialogo(x, y, w, h);
}

void panel_hueco(int x, int y, int w, int h) {
	gfx_fill_a(x, y, w, h, gfx_rgb(14, 9, 6), 222);
	gfx_fill(x, y, w, 1, gfx_rgb(8, 5, 3));
	gfx_fill(x, y + h - 1, w, 1, gfx_rgb(120, 92, 52));
}

static void boton_fondo(int x, int y, int w, int h, int enabled, int over, int down, int foco) {
	u32 top = !enabled ? gfx_rgb(78, 62, 50) : down ? gfx_rgb(110, 22, 18) : over ? gfx_rgb(214, 66, 54) : gfx_rgb(184, 44, 36);
	u32 bot = !enabled ? gfx_rgb(58, 46, 38) : down ? gfx_rgb(84, 14, 12) : over ? gfx_rgb(150, 32, 26) : gfx_rgb(118, 24, 20);
	gfx_fill_grad(x, y, w, h, top, bot);
	bevel(x, y, w, h, foco ? COL_CREAM : enabled ? COL_GOLD : gfx_rgb(110, 95, 70));
	if (foco) bevel(x + 1, y + 1, w - 2, h - 2, COL_GOLD);
	if (enabled && !down) gfx_fill(x + 2, y + 1, w - 4, 1, gfx_rgb(255, 190, 150));
}

/* botón con texto. size: 1-2 texto normal (8x16), 3 o más texto grande (x2). Admite el foco del teclado. */
int button(int x, int y, int w, int h, const char *label, int enabled, int size) {
	int foco = enabled && foco_control(x, y, w, h);
	int over = enabled && hit(x, y, w, h), down = over && in.mdown;
	boton_fondo(x, y, w, h, enabled, over, down, foco);
	int tam = size >= 3 && gfx_t_w(label, 2) <= w - 12 && h >= 40 ? 2 : 1;
	int tw = gfx_t_w(label, tam);
	gfx_ts(x + (w - tw) / 2, y + (h - 16 * tam) / 2 + (down ? 1 : 0), label, enabled ? COL_CREAM : gfx_rgb(165, 150, 130), tam);
	int pulsado = (over && in.mclick) || (foco && tecla(SDL_SCANCODE_RETURN));
	if (pulsado) { snd_ui(SND_CLICK); in.bloqueo = 1; }
	return pulsado;
}

int boton_t(int x, int y, int w, int h, const char *label, int enabled) { return button(x, y, w, h, label, enabled, 1); }

/* selector "< valor >": devuelve -1, 0 o +1. Con el foco del teclado, Izquierda/Derecha lo cambian. */
int cycler(int x, int y, int w, const char *label) {
	int h = 26, ret = 0;
	int foco = foco_control(x, y, w, h);
	gfx_fill_grad(x, y, w, h, gfx_rgb(40, 26, 18), COL_PANEL2);
	bevel(x, y, w, h, foco ? COL_CREAM : COL_GOLD);
	int tw = gfx_t_w(label, 1);
	if (tw > w - 52) {
		/* no cabe: se recorta con puntos suspensivos (el valor completo se ve en la ayuda o en la descripción) */
		char b[96]; snprintf(b, sizeof b, "%s", label);
		int n = (int)strlen(b);
		while (n > 1 && gfx_t_w(b, 1) > w - 60) { do { b[--n] = 0; } while (n > 0 && ((unsigned char)b[n] & 0xc0) == 0x80); }
		strcat(b, "...");
		gfx_ts(x + 26, y + 5, b, COL_CREAM, 1);
	} else gfx_ts(x + (w - tw) / 2, y + 5, label, COL_CREAM, 1);
	int ol = hit(x, y, 24, h), orr = hit(x + w - 24, y, 24, h);
	gfx_fill_grad(x + 1, y + 1, 22, h - 2, ol ? gfx_rgb(214, 66, 54) : gfx_rgb(160, 40, 32), gfx_rgb(110, 24, 20));
	gfx_fill_grad(x + w - 23, y + 1, 22, h - 2, orr ? gfx_rgb(214, 66, 54) : gfx_rgb(160, 40, 32), gfx_rgb(110, 24, 20));
	/* flechas dibujadas */
	for (int k = 0; k < 5; ++k) {
		gfx_fill(x + 9 + k, y + h / 2 - k, 1, 2 * k + 1, COL_CREAM);
		gfx_fill(x + w - 10 - k, y + h / 2 - k, 1, 2 * k + 1, COL_CREAM);
	}
	if (in.mclick && ol) ret = -1;
	if (in.mclick && orr) ret = 1;
	if (in.mclick && !ol && !orr && hit(x, y, w, h)) ret = 1;   /* clic en el valor: el siguiente */
	if (foco && tecla(SDL_SCANCODE_LEFT)) ret = -1;
	if (foco && tecla(SDL_SCANCODE_RIGHT)) ret = 1;
	if (ret) { snd_ui(SND_CLICK); in.bloqueo = 1; }
	return ret;
}

int checkbox(int x, int y, const char *label, int *v) {
	int foco = foco_control(x, y, 32 + gfx_t_w(label, 1), 22);
	int over = hit(x, y, 32 + gfx_t_w(label, 1), 22);
	gfx_fill_grad(x, y, 22, 22, over ? gfx_rgb(58, 38, 26) : gfx_rgb(40, 26, 18), COL_PANEL2);
	bevel(x, y, 22, 22, foco ? COL_CREAM : COL_GOLD);
	if (*v) { gfx_fill(x + 5, y + 5, 12, 12, gfx_rgb(120, 220, 110)); gfx_fill(x + 5, y + 5, 12, 2, gfx_rgb(190, 255, 170)); }
	gfx_ts(x + 30, y + 3, label, COL_CREAM, 1);
	if ((over && in.mclick) || (foco && (tecla(SDL_SCANCODE_RETURN) || tecla(SDL_SCANCODE_SPACE)))) { *v = !*v; snd_ui(SND_CLICK); in.bloqueo = 1; return 1; }
	return 0;
}

void barra_progreso(int x, int y, int w, int h, int v, int max, u32 col) {
	gfx_fill(x, y, w, h, gfx_rgb(12, 9, 6));
	int f = max > 0 ? (int)((long long)(w - 2) * CLAMP(v, 0, max) / max) : 0;
	if (f > 0) gfx_fill_grad(x + 1, y + 1, f, h - 2, col, ((col >> 1) & 0x7f7f7f) | 0xff000000u);
	gfx_rect(x, y, w, h, gfx_rgb(90, 70, 40));
}

/* parte el texto en líneas que caben en w (8x16) y las dibuja; devuelve cuántas líneas usó */
static int texto_partir(int x, int y, int w, const char *s, u32 col, int max_lineas, int dibujar) {
	char linea[256];
	int n = 0, lineas = 0;
	const char *p = s;
	while (*p && lineas < max_lineas) {
		/* añade palabras mientras quepan */
		n = 0; linea[0] = 0;
		const char *ult = p;
		while (*p) {
			const char *q = p;
			while (*q == ' ') ++q;
			const char *f = q;
			while (*f && *f != ' ' && *f != '\n') ++f;
			char prueba[256];
			snprintf(prueba, sizeof prueba, "%s%s%.*s", linea, n ? " " : "", (int)(f - q), q);
			if (n && gfx_t_w(prueba, 1) > w) break;
			snprintf(linea, sizeof linea, "%s", prueba);
			n = 1; p = f; ult = p;
			if (*p == '\n') { ++p; break; }
		}
		if (!n) { /* una palabra que no cabe: se corta */ snprintf(linea, sizeof linea, "%.30s", p); p += strlen(linea); }
		(void)ult;
		if (dibujar) gfx_ts(x, y + lineas * 16, linea, col, 1);
		++lineas;
		while (*p == ' ') ++p;
	}
	return lineas;
}
void texto_ajustado(int x, int y, int w, const char *s, u32 col, int max_lineas) { texto_partir(x, y, w, s, col, max_lineas, 1); }
int texto_lineas(const char *s, int w) { return texto_partir(0, 0, w, s, 0, 20, 0); }

/* ---------- foco del teclado en los menús ---------- */
int foco_actual = -1, foco_n, foco_prev_n;
static int foco_activo;   /* el teclado se usó para mover el foco (si no, no se dibuja) */

void foco_inicio(void) {
	foco_prev_n = foco_n;
	foco_n = 0;
	if (tecla(SDL_SCANCODE_DOWN) || (tecla(SDL_SCANCODE_TAB) && !mayus())) { foco_activo = 1; foco_actual = foco_prev_n ? (foco_actual + 1) % foco_prev_n : 0; }
	if (tecla(SDL_SCANCODE_UP) || (tecla(SDL_SCANCODE_TAB) && mayus())) { foco_activo = 1; foco_actual = foco_prev_n ? (foco_actual - 1 + foco_prev_n) % foco_prev_n : 0; }
	if (in.mclick) foco_activo = 0;
}

int foco_control(int x, int y, int w, int h) {
	(void)x; (void)y; (void)w; (void)h;
	if (ui_modal_on && !ui_en_modal) return 0;
	int mio = foco_n++ == foco_actual;
	return mio && foco_activo && app.screen != SC_GAME;
}

/* ---------- iconos ---------- */

/* primitivas en píxeles reales (iconos nítidos con la interfaz x2) */
static int ix0, iy0, isz;   /* caja del icono en píxeles reales */
static void ip_fill(float x, float y, float w, float h, u32 c) {
	int a = ix0 + (int)(x * isz), b = iy0 + (int)(y * isz);
	int aw = MAX(1, (int)(w * isz)), bh = MAX(1, (int)(h * isz));
	gfx_fill(a, b, aw, bh, c);
}
static void ip_circ(float cx, float cy, float r, u32 c) {
	int x = ix0 + (int)(cx * isz), y = iy0 + (int)(cy * isz), rr = MAX(1, (int)(r * isz));
	for (int dy = -rr; dy <= rr; ++dy) { int w = (int)sqrtf((float)(rr * rr - dy * dy)); gfx_fill(x - w, y + dy, 2 * w + 1, 1, c); }
}
static void ip_linea(float x0, float y0, float x1, float y1, float t, u32 c) {
	float L = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)) * isz;
	int n = MAX(1, (int)L);
	int g = MAX(1, (int)(t * isz));
	for (int k = 0; k <= n; ++k) {
		float f = (float)k / n;
		int x = ix0 + (int)((x0 + (x1 - x0) * f) * isz), y = iy0 + (int)((y0 + (y1 - y0) * f) * isz);
		gfx_fill(x - g / 2, y - g / 2, g, g, c);
	}
}
static void ip_tri(float ax, float ay, float bx, float by, float cx, float cy, u32 c) {
	/* triángulo relleno por filas */
	float px[3] = { ax * isz + ix0, bx * isz + ix0, cx * isz + ix0 }, py[3] = { ay * isz + iy0, by * isz + iy0, cy * isz + iy0 };
	int y0 = (int)floorf(MIN(py[0], MIN(py[1], py[2]))), y1 = (int)ceilf(MAX(py[0], MAX(py[1], py[2])));
	for (int y = y0; y <= y1; ++y) {
		float xa = 1e9f, xb = -1e9f, yy = y + 0.5f;
		for (int k = 0; k < 3; ++k) {
			int j = (k + 1) % 3;
			float ya = py[k], yb = py[j];
			if ((yy < ya) == (yy < yb)) continue;
			float x = px[k] + (yy - ya) * (px[j] - px[k]) / (yb - ya);
			xa = MIN(xa, x); xb = MAX(xb, x);
		}
		if (xb >= xa) gfx_fill((int)xa, y, (int)(xb - xa) + 1, 1, c);
	}
}

#define C_OSC 0xff1a120cu
static void ic_espada(float x0, float y0, float x1, float y1, u32 hoja) {
	/* hoja de (x0,y0) a (x1,y1); la empuñadura en (x1,y1) */
	float dx = x1 - x0, dy = y1 - y0;
	ip_linea(x0, y0, x1, y1, 0.11f, C_OSC);
	ip_linea(x0, y0, x1, y1, 0.07f, hoja);
	ip_linea(x1 - dy * 0.25f, y1 + dx * 0.25f, x1 + dy * 0.25f, y1 - dx * 0.25f, 0.08f, gfx_rgb(220, 175, 60));
	ip_linea(x1, y1, x1 + dx * 0.22f, y1 + dy * 0.22f, 0.08f, gfx_rgb(120, 70, 30));
}
static void ic_escudo(float cx, float cy, float s, u32 c, u32 borde) {
	ip_fill(cx - s * 0.5f - 0.03f, cy - s * 0.55f - 0.03f, s + 0.06f, s * 0.6f + 0.03f, borde);
	ip_tri(cx - s * 0.5f - 0.03f, cy + 0.02f, cx + s * 0.5f + 0.03f, cy + 0.02f, cx, cy + s * 0.7f + 0.05f, borde);
	ip_fill(cx - s * 0.5f, cy - s * 0.55f, s, s * 0.6f, c);
	ip_tri(cx - s * 0.5f, cy + 0.04f, cx + s * 0.5f, cy + 0.04f, cx, cy + s * 0.7f, c);
	ip_fill(cx - s * 0.5f, cy - s * 0.55f, s, 0.04f, gfx_rgb(255, 255, 255));
}
static void ic_flecha(float x0, float y0, float x1, float y1, u32 c) {
	float dx = x1 - x0, dy = y1 - y0, L = sqrtf(dx * dx + dy * dy);
	dx /= L; dy /= L;
	ip_linea(x0, y0, x1 - dx * 0.1f, y1 - dy * 0.1f, 0.09f, c);
	ip_tri(x1, y1, x1 - dx * 0.22f - dy * 0.14f, y1 - dy * 0.22f + dx * 0.14f, x1 - dx * 0.22f + dy * 0.14f, y1 - dy * 0.22f - dx * 0.14f, c);
}
static void ic_barco(u32 c) {
	ip_tri(0.12f, 0.62f, 0.88f, 0.62f, 0.75f, 0.82f, c);
	ip_fill(0.25f, 0.62f, 0.5f, 0.2f, c);
	ip_fill(0.48f, 0.2f, 0.05f, 0.42f, gfx_rgb(120, 80, 40));
	ip_tri(0.53f, 0.22f, 0.53f, 0.55f, 0.78f, 0.52f, gfx_rgb(235, 225, 200));
}
static void ic_persona(float cx, float cy, float s, u32 c) {
	ip_circ(cx, cy - s * 0.32f, s * 0.16f, c);
	ip_tri(cx - s * 0.3f, cy + s * 0.4f, cx + s * 0.3f, cy + s * 0.4f, cx, cy - s * 0.16f, c);
	ip_fill(cx - s * 0.3f, cy + s * 0.05f, s * 0.6f, s * 0.35f, c);
}

/* dibujos propios (sin datos originales, o para órdenes que el juego original no tenía) */
static void icono_dibujado(int id) {
	u32 crema = COL_CREAM, oro = gfx_rgb(240, 200, 60), rojo = gfx_rgb(220, 50, 40), acero = gfx_rgb(200, 205, 215);
	switch (id) {
	case IC_MADERA:
		for (int k = 0; k < 3; ++k) { float y = 0.28f + k * 0.18f; ip_fill(0.15f, y, 0.62f, 0.15f, gfx_rgb(140, 90, 45)); ip_fill(0.15f, y, 0.62f, 0.04f, gfx_rgb(180, 125, 70)); ip_circ(0.78f, y + 0.075f, 0.08f, gfx_rgb(225, 190, 130)); }
		break;
	case IC_COMIDA:
		ip_circ(0.42f, 0.55f, 0.27f, gfx_rgb(190, 40, 40)); ip_circ(0.38f, 0.5f, 0.12f, gfx_rgb(235, 110, 100));
		ip_linea(0.6f, 0.38f, 0.85f, 0.15f, 0.1f, gfx_rgb(240, 235, 220)); ip_circ(0.86f, 0.14f, 0.07f, gfx_rgb(240, 235, 220));
		break;
	case IC_ORO:
		ip_circ(0.32f, 0.62f, 0.16f, gfx_rgb(200, 150, 20)); ip_circ(0.62f, 0.64f, 0.17f, gfx_rgb(220, 170, 30)); ip_circ(0.47f, 0.4f, 0.17f, oro);
		ip_circ(0.42f, 0.35f, 0.05f, gfx_rgb(255, 250, 200));
		break;
	case IC_PIEDRA:
		ip_circ(0.33f, 0.6f, 0.2f, gfx_rgb(120, 120, 125)); ip_circ(0.65f, 0.62f, 0.18f, gfx_rgb(150, 150, 155)); ip_circ(0.5f, 0.38f, 0.17f, gfx_rgb(180, 180, 185));
		ip_circ(0.45f, 0.33f, 0.05f, gfx_rgb(230, 230, 235));
		break;
	case IC_POB: ic_persona(0.36f, 0.55f, 0.6f, gfx_rgb(220, 180, 140)); ic_persona(0.64f, 0.52f, 0.66f, crema); break;
	case IC_ATAQUE: ic_espada(0.18f, 0.18f, 0.72f, 0.72f, acero); break;
	case IC_ARMADURA: ic_escudo(0.5f, 0.45f, 0.62f, gfx_rgb(150, 150, 160), oro); break;
	case IC_PERFORANTE: ic_escudo(0.5f, 0.45f, 0.62f, gfx_rgb(110, 80, 50), oro); ic_flecha(0.1f, 0.85f, 0.62f, 0.33f, acero); break;
	case IC_ALCANCE:
		for (int k = 0; k <= 16; ++k) { float a = -1.2f + 2.4f * k / 16; ip_circ(0.3f + cosf(a) * 0.38f, 0.5f + sinf(a) * 0.38f, 0.04f, gfx_rgb(160, 100, 50)); }
		ip_linea(0.44f, 0.15f, 0.44f, 0.85f, 0.03f, crema);
		ic_flecha(0.2f, 0.5f, 0.92f, 0.5f, acero);
		break;
	case IC_VELOCIDAD:
		for (int k = 0; k < 2; ++k) { float x = 0.18f + k * 0.3f; ip_linea(x, 0.22f, x + 0.28f, 0.5f, 0.12f, crema); ip_linea(x + 0.28f, 0.5f, x, 0.78f, 0.12f, crema); }
		break;
	case IC_VIDA:
		ip_circ(0.35f, 0.38f, 0.17f, rojo); ip_circ(0.65f, 0.38f, 0.17f, rojo); ip_tri(0.19f, 0.45f, 0.81f, 0.45f, 0.5f, 0.82f, rojo);
		ip_circ(0.3f, 0.33f, 0.05f, gfx_rgb(255, 180, 170));
		break;
	case IC_TIEMPO:
		ip_fill(0.22f, 0.12f, 0.56f, 0.07f, gfx_rgb(150, 100, 50)); ip_fill(0.22f, 0.81f, 0.56f, 0.07f, gfx_rgb(150, 100, 50));
		ip_tri(0.27f, 0.19f, 0.73f, 0.19f, 0.5f, 0.5f, gfx_rgb(220, 220, 230)); ip_tri(0.27f, 0.81f, 0.73f, 0.81f, 0.5f, 0.5f, gfx_rgb(220, 220, 230));
		ip_tri(0.36f, 0.81f, 0.64f, 0.81f, 0.5f, 0.6f, oro);
		break;
	case IC_AGRESIVA: ic_espada(0.15f, 0.15f, 0.7f, 0.7f, gfx_rgb(255, 150, 140)); ic_espada(0.85f, 0.15f, 0.3f, 0.7f, gfx_rgb(255, 150, 140)); break;
	case IC_DEFENSIVA: ic_escudo(0.5f, 0.45f, 0.66f, gfx_rgb(50, 90, 190), oro); break;
	case IC_QUIETA:
		ip_fill(0.15f, 0.8f, 0.7f, 0.07f, gfx_rgb(120, 90, 50));
		ic_espada(0.5f, 0.82f, 0.5f, 0.25f, acero);
		break;
	case IC_PASIVA:
		ip_fill(0.25f, 0.12f, 0.06f, 0.78f, gfx_rgb(150, 100, 50));
		ip_tri(0.31f, 0.14f, 0.31f, 0.52f, 0.85f, 0.3f, gfx_rgb(245, 245, 240));
		break;
	case IC_PROTEGER: ic_escudo(0.5f, 0.45f, 0.66f, gfx_rgb(60, 140, 70), oro); ip_fill(0.44f, 0.28f, 0.12f, 0.36f, crema); ip_fill(0.32f, 0.4f, 0.36f, 0.12f, crema); break;
	case IC_PATRULLA:
		for (int k = 0; k < 22; ++k) { float a = 0.4f + 2.3f * k / 22; ip_circ(0.5f + cosf(a) * 0.3f, 0.5f + sinf(a) * 0.3f, 0.05f, oro); }
		for (int k = 0; k < 22; ++k) { float a = 3.54f + 2.3f * k / 22; ip_circ(0.5f + cosf(a) * 0.3f, 0.5f + sinf(a) * 0.3f, 0.05f, oro); }
		ip_tri(0.06f, 0.48f, 0.3f, 0.48f, 0.18f, 0.7f, oro); ip_tri(0.7f, 0.52f, 0.94f, 0.52f, 0.82f, 0.3f, oro);
		break;
	case IC_DETENER:
		ip_circ(0.5f, 0.5f, 0.36f, gfx_rgb(240, 240, 240)); ip_circ(0.5f, 0.5f, 0.31f, rojo); ip_fill(0.3f, 0.45f, 0.4f, 0.1f, gfx_rgb(250, 250, 250));
		break;
	case IC_DESEMBARCAR: ic_barco(gfx_rgb(150, 100, 55)); ic_flecha(0.22f, 0.18f, 0.22f, 0.55f, gfx_rgb(130, 255, 130)); break;
	case IC_EMBARCAR: ic_barco(gfx_rgb(150, 100, 55)); ic_flecha(0.22f, 0.55f, 0.22f, 0.15f, gfx_rgb(130, 255, 130)); break;
	case IC_REUNION:
		ip_fill(0.28f, 0.12f, 0.06f, 0.76f, gfx_rgb(230, 225, 210));
		ip_fill(0.34f, 0.14f, 0.45f, 0.3f, gfx_rgb(50, 110, 230));
		break;
	case IC_SIGUIENTE: ic_flecha(0.15f, 0.5f, 0.88f, 0.5f, gfx_rgb(230, 60, 40)); break;
	case IC_CANCELAR: ip_linea(0.2f, 0.2f, 0.8f, 0.8f, 0.16f, rojo); ip_linea(0.8f, 0.2f, 0.2f, 0.8f, 0.16f, rojo); break;
	case IC_CONSTRUIR:
		ip_fill(0.48f, 0.35f, 0.08f, 0.55f, gfx_rgb(140, 90, 45));
		ip_fill(0.28f, 0.18f, 0.44f, 0.2f, gfx_rgb(150, 150, 155)); ip_fill(0.28f, 0.18f, 0.44f, 0.05f, gfx_rgb(210, 210, 215));
		break;
	case IC_ELIMINAR: ip_circ(0.5f, 0.45f, 0.28f, gfx_rgb(230, 225, 210)); ip_fill(0.36f, 0.6f, 0.28f, 0.2f, gfx_rgb(230, 225, 210)); ip_circ(0.4f, 0.43f, 0.07f, C_OSC); ip_circ(0.6f, 0.43f, 0.07f, C_OSC); break;
	case IC_DIPLOMACIA:
		ip_fill(0.15f, 0.12f, 0.05f, 0.76f, gfx_rgb(150, 100, 50)); ip_fill(0.2f, 0.14f, 0.32f, 0.24f, gfx_rgb(50, 110, 230));
		ip_fill(0.8f, 0.12f, 0.05f, 0.76f, gfx_rgb(150, 100, 50)); ip_fill(0.48f, 0.14f, 0.32f, 0.24f, gfx_rgb(220, 50, 40));
		ip_linea(0.32f, 0.66f, 0.68f, 0.66f, 0.12f, gfx_rgb(220, 180, 140));
		break;
	case IC_AYUDA:
		ip_circ(0.5f, 0.5f, 0.4f, gfx_rgb(60, 90, 160));
		break;
	case IC_PUNTOS:
		ip_fill(0.15f, 0.55f, 0.18f, 0.3f, gfx_rgb(80, 140, 230)); ip_fill(0.41f, 0.3f, 0.18f, 0.55f, oro); ip_fill(0.67f, 0.42f, 0.18f, 0.43f, rojo);
		ip_fill(0.1f, 0.85f, 0.8f, 0.05f, crema);
		break;
	case IC_ALERTA: ip_tri(0.5f, 0.1f, 0.92f, 0.86f, 0.08f, 0.86f, rojo); ip_fill(0.45f, 0.35f, 0.1f, 0.3f, gfx_rgb(255, 255, 255)); ip_fill(0.45f, 0.7f, 0.1f, 0.1f, gfx_rgb(255, 255, 255)); break;
	case IC_EDAD:
		ip_fill(0.25f, 0.15f, 0.5f, 0.08f, crema); ip_fill(0.32f, 0.23f, 0.36f, 0.56f, gfx_rgb(215, 205, 180)); ip_fill(0.2f, 0.79f, 0.6f, 0.08f, crema);
		ip_fill(0.4f, 0.23f, 0.04f, 0.56f, gfx_rgb(160, 150, 130)); ip_fill(0.52f, 0.23f, 0.04f, 0.56f, gfx_rgb(160, 150, 130));
		break;
	case IC_MEJORA:
		ip_fill(0.2f, 0.18f, 0.6f, 0.64f, gfx_rgb(225, 205, 160)); ip_fill(0.15f, 0.14f, 0.7f, 0.08f, gfx_rgb(150, 110, 60)); ip_fill(0.15f, 0.78f, 0.7f, 0.08f, gfx_rgb(150, 110, 60));
		for (int k = 0; k < 4; ++k) ip_fill(0.28f, 0.3f + k * 0.11f, 0.44f, 0.04f, gfx_rgb(110, 80, 50));
		break;
	case IC_VENDER: case IC_COMPRAR: case IC_TRIBUTO:
		ip_circ(0.5f, 0.5f, 0.3f, oro); ip_circ(0.5f, 0.5f, 0.2f, gfx_rgb(200, 150, 20));
		break;
	case IC_ALDEANO: ic_persona(0.5f, 0.55f, 0.8f, gfx_rgb(220, 180, 140)); break;
	case IC_SOLDADO: ic_espada(0.2f, 0.2f, 0.72f, 0.72f, acero); ic_escudo(0.38f, 0.55f, 0.36f, gfx_rgb(160, 40, 30), oro); break;
	case IC_RECURSOS: ip_circ(0.35f, 0.6f, 0.2f, oro); ip_circ(0.62f, 0.45f, 0.2f, gfx_rgb(170, 170, 175)); break;
	case IC_MILITAR: ic_espada(0.15f, 0.15f, 0.7f, 0.7f, acero); ic_espada(0.85f, 0.15f, 0.3f, 0.7f, acero); break;
	default: ip_circ(0.5f, 0.5f, 0.3f, crema); break;
	}
}

/* los iconos de los datos originales que existen: INTERFAC 50731 (recursos y estadísticas) y 50721 (órdenes) */
static Sprite *icono_drs(int id) {
	switch (id) {
	case IC_MADERA: return iface_frame(50731, 0, 16);
	case IC_PIEDRA: return iface_frame(50731, 1, 16);
	case IC_COMIDA: return iface_frame(50731, 2, 16);
	case IC_ORO: return iface_frame(50731, 3, 16);
	case IC_POB: return iface_frame(50731, 4, 16);
	case IC_ATAQUE: return iface_frame(50731, 7, 16);
	case IC_ARMADURA: return iface_frame(50731, 8, 16);
	case IC_TIEMPO: return iface_frame(50731, 9, 16);
	case IC_PERFORANTE: return iface_frame(50731, 10, 16);
	case IC_CONSTRUIR: return iface_frame(50721, 0, 30);
	case IC_DETENER: return iface_frame(50721, 3, 30);
	case IC_DESEMBARCAR: return iface_frame(50721, 5, 30);
	case IC_REUNION: return iface_frame(50721, 6, 30);
	case IC_CANCELAR: return iface_frame(50721, 10, 30);
	case IC_SIGUIENTE: return iface_frame(50721, 11, 30);
	case IC_ELIMINAR: return iface_frame(50721, 13, 30);
	case IC_AGRESIVA: return iface_frame(50721, 4, 30);
	case IC_DEFENSIVA: return iface_frame(50721, 12, 30);
	default: return NULL;
	}
}

/* dibuja un sprite de interfaz centrado y escalado para caber en un cuadrado de sz (lógico) */
static void sprite_en_caja(const Sprite *s, int x, int y, int w, int h, int player) {
	int sc = MIN(w * 256 / s->w, h * 256 / s->h);
	if (sc > 256 && sc < 384) sc = 256;   /* sin ampliar un poco (se vería borroso o con dientes) */
	int dw = s->w * sc / 256, dh = s->h * sc / 256;
	gfx_sprite(s, x + (w - dw) / 2 + s->hx * sc / 256, y + (h - dh) / 2 + s->hy * sc / 256, sc, player, 256, 0);
}

void icono(int id, int x, int y, int sz) {
	Sprite *s = icono_drs(id);
	if (s) { sprite_en_caja(s, x, y, sz, sz, 0); return; }
	int us = gfx_ui_suspend();
	ix0 = x * us; iy0 = y * us; isz = sz * us;
	icono_dibujado(id);
	gfx_ui_resume(us);
	if (id == IC_AYUDA) gfx_ts(x + sz / 2 - 4, y + sz / 2 - 8, "?", COL_CREAM, 1);
}

static const int ic_rec[RES_COUNT] = { IC_MADERA, IC_COMIDA, IC_ORO, IC_PIEDRA };
void icono_rec(int kind, int x, int y, int h) {
	Sprite *s = icono_drs(ic_rec[kind]);
	if (s) { sprite_en_caja(s, x, y, h * 3 / 2, h, 0); return; }
	icono(ic_rec[kind], x + h / 4, y, h);
}

int coste_ancho(const int *coste) {
	int w = 0;
	for (int k = 0; k < RES_COUNT; ++k) if (coste[k] > 0) { char b[16]; snprintf(b, sizeof b, "%d", coste[k]); w += 26 + gfx_t_w(b, 1) + 8; }
	return w;
}

/* coste con iconos en una línea; lo que el jugador no puede pagar sale en rojo */
int coste_linea(int x, int y, const int *coste, int faltan_marca) {
	int x0 = x;
	for (int k = 0; k < RES_COUNT; ++k) {
		if (coste[k] <= 0) continue;
		icono_rec(k, x, y, 16);
		char b[16];
		snprintf(b, sizeof b, "%d", coste[k]);
		int falta = faltan_marca && W.nents && W.pl[W.human].res[k] < coste[k];
		gfx_ts(x + 26, y, b, falta ? COL_MAL : COL_CREAM, 1);
		x += 26 + gfx_t_w(b, 1) + 8;
	}
	return x - x0;
}

/* iconos de unidades, edificios y mejoras en los datos originales */
Sprite *icono_unidad(EntType t) {
	static const struct { EntType t; int icon; } ul[] = {
		{ E_VILLAGER, 0 }, { E_CLUBMAN, 2 }, { E_AXEMAN, 3 }, { E_SHORTSWORD, 4 }, { E_BOWMAN, 6 }, { E_IMPBOW, 7 }, { E_CHARIOT, 10 },
		{ E_SCOUT, 31 }, { E_CAVALRY, 11 }, { E_HEAVYCAV, 12 }, { E_HOPLITE, 16 }, { E_PRIEST, 1 },
		{ E_BALLISTA, 53 }, { E_CATAPULT, 14 }, { E_HEAVYCAT, 15 },
		{ E_SLINGER, 8 }, { E_WARELE, 33 }, { E_ELEARCHER, 33 }, { E_FISHBOAT, 22 }, { E_TRANSPORT, 23 }, { E_WARSHIP, 24 },
		{ E_PHALANX, 54 }, { E_CENTURION, 50 }, { E_CATAPHRACT, 11 }, { E_HORSEARCHER, 48 }, { E_TRADESHIP, 25 }, { E_TRADECART, 10 }, { E_HORSE, 31 }, { E_ELEPHANT, 33 },
	};
	for (int i = 0; i < ARRAY_LEN(ul); ++i) if (ul[i].t == t) { Sprite *s = icon_unit(ul[i].icon); return s && s->w >= 16 ? s : NULL; }
	return NULL;
}

Sprite *icono_edificio(EntType t) {
	static const struct { EntType t; int icon; } bl[] = {
		{ E_TOWN_CENTER, 12 }, { E_HOUSE, 15 }, { E_BARRACKS, 3 }, { E_GRANARY, 11 }, { E_STORAGE, 28 }, { E_FARM, 10 }, { E_TOWER, 26 },
		{ E_ARCHERY, 1 }, { E_STABLE, 25 }, { E_TEMPLE, 31 }, { E_ACADEMY, 0 }, { E_GOVERNMENT, 19 }, { E_MARKET, 21 }, { E_SIEGE, 9 },
		{ E_WALL, 33 }, { E_DOCK, 6 }, { E_WONDER, 27 },
	};
	for (int i = 0; i < ARRAY_LEN(bl); ++i) if (bl[i].t == t) { Sprite *s = icon_bld(bl[i].icon); return s && s->w >= 16 ? s : NULL; }
	return NULL;
}

Sprite *icono_mejora(int tech) {
	/* cuadros de INTERFAC 50729 elegidos por el dibujo (las mejoras de AoEN no son las mismas que las del original) */
	static const int fr[T_COUNT] = {
		72, 73, 17, 90, 91, 37, 40, 56, 59, 53, 51, 39, 9, 65, 10, 66, 21, 22, 54, 75, 5, 14, 13, 88, 34, 87, 64,
	};
	if (tech < 0 || tech >= T_COUNT) return NULL;
	return iface_frame(50729, fr[tech], 30);
}

/* ---------- ayudas emergentes ---------- */
static Tip tip_cur;
static int tip_on;
void tip_poner(const Tip *t) { tip_cur = *t; tip_on = 1; }
void tip_cancelar(void) { tip_on = 0; }
const Tip *tip_actual(void) { return tip_on ? &tip_cur : NULL; }
void tip_simple(Rect ancla, const char *titulo, const char *desc) {
	Tip t;
	memset(&t, 0, sizeof t);
	snprintf(t.titulo, sizeof t.titulo, "%s", titulo);
	if (desc) snprintf(t.desc, sizeof t.desc, "%s", desc);
	t.ancla = ancla;
	tip_poner(&t);
}

void tip_dibujar(void) {
	if (!tip_on) return;
	tip_on = 0;
	const Tip *t = &tip_cur;
	int w = MAX(240, gfx_t_w(t->titulo, 1) + 20);
	int cw = t->con_coste ? coste_ancho(t->coste) + (t->tiempo > 0 ? 70 : 0) : 0;
	w = MAX(w, cw + 20);
	if (t->tecla[0]) w = MAX(w, gfx_t_w(t->tecla, 1) + 90);
	w = MIN(w, 380);
	int tw = w - 20;
	if (t->desc[0] && gfx_t_w(t->desc, 1) > tw) { w = MIN(380, MAX(w, 300)); tw = w - 20; }
	int h = 12 + 18;
	if (t->con_coste) h += 20;
	if (t->desc[0]) h += 16 * texto_lineas(t->desc, tw) + 4;
	if (t->extra[0]) h += 16 * texto_lineas(t->extra, tw) + 2;
	if (t->motivo[0]) h += 16 * texto_lineas(t->motivo, tw) + 4;
	if (t->tecla[0]) h += 18;
	int lw = fb.w, lh = fb.h;
	int x = t->ancla.x + t->ancla.w / 2 - w / 2, y = t->ancla.y - h - 6;
	if (y < 4) y = t->ancla.y + t->ancla.h + 6;
	x = CLAMP(x, 4, lw - w - 4); y = CLAMP(y, 4, lh - h - 4);
	gfx_fill_a(x + 3, y + 3, w, h, gfx_rgb(0, 0, 0), 110);
	gfx_fill_a(x, y, w, h, gfx_rgb(22, 15, 10), 242);
	bevel(x, y, w, h, COL_GOLD);
	int yy = y + 8;
	gfx_ts(x + 10, yy, t->titulo, COL_AVISO, 1);
	yy += 20;
	if (t->con_coste) {
		int cx = x + 10 + coste_linea(x + 10, yy, t->coste, 1);
		if (t->tiempo > 0) { char b[16]; snprintf(b, sizeof b, "%d s", (int)(t->tiempo + 0.5f)); icono(IC_TIEMPO, cx, yy, 16); gfx_ts(cx + 20, yy, b, COL_DIM, 1); }
		yy += 20;
	}
	if (t->desc[0]) { int n = texto_lineas(t->desc, tw); texto_ajustado(x + 10, yy, tw, t->desc, COL_CREAM, n); yy += 16 * n + 4; }
	if (t->extra[0]) { int n = texto_lineas(t->extra, tw); texto_ajustado(x + 10, yy, tw, t->extra, COL_DIM, n); yy += 16 * n + 2; }
	if (t->motivo[0]) { int n = texto_lineas(t->motivo, tw); texto_ajustado(x + 10, yy, tw, t->motivo, COL_MAL, n); yy += 16 * n + 4; }
	if (t->tecla[0]) {
		gfx_ts(x + 10, yy, "Tecla:", COL_DIM, 1);
		int kw = gfx_t_w(t->tecla, 1) + 8;
		gfx_fill(x + 64, yy - 1, kw, 18, gfx_rgb(60, 45, 30));
		bevel(x + 64, yy - 1, kw, 18, gfx_rgb(150, 120, 70));
		gfx_ts(x + 68, yy, t->tecla, COL_CREAM, 1);
	}
	static char ultimo[80];
	if (strcmp(ultimo, t->titulo)) {
		snprintf(ultimo, sizeof ultimo, "%s", t->titulo);
		AOEN_LOG("[AOEN] ayuda: %s | coste %d/%d/%d/%d | %s", t->titulo, t->coste[0], t->coste[1], t->coste[2], t->coste[3], t->motivo[0] ? t->motivo : "disponible");
	}
	solape_registrar(-1, 0, 0, 0, NULL);   /* la ayuda va encima de todo: no cuenta como solape */
}

/* ---------- solapes ---------- */
static struct { int x, y, w, h; const char *n; } sol[160];
static int nsol;
void solape_registrar(int x, int y, int w, int h, const char *nombre) {
	if (x < 0 || !nombre || nsol >= ARRAY_LEN(sol)) return;
	sol[nsol].x = x; sol[nsol].y = y; sol[nsol].w = w; sol[nsol].h = h; sol[nsol].n = nombre; ++nsol;
}
void solape_fin_cuadro(void) {
	static int activo = -1, cuadros;
	if (activo < 0) activo = getenv("AOEN_SOLAPES") && atoi(getenv("AOEN_SOLAPES")) > 0;
	if (activo && ++cuadros % 30 == 0 && nsol) {
		int n = 0, fuera = 0;
		char ej[200] = "";
		for (int a = 0; a < nsol; ++a) {
			if (sol[a].x < 0 || sol[a].y < 0 || sol[a].x + sol[a].w > fb.w || sol[a].y + sol[a].h > fb.h) { ++fuera; if (!ej[0]) snprintf(ej, sizeof ej, "%s fuera de la pantalla", sol[a].n); }
			for (int b = a + 1; b < nsol; ++b) {
				/* un bloque dentro de otro (botones dentro de su panel) no es un solape */
				int a_en_b = sol[a].x >= sol[b].x && sol[a].y >= sol[b].y && sol[a].x + sol[a].w <= sol[b].x + sol[b].w && sol[a].y + sol[a].h <= sol[b].y + sol[b].h;
				int b_en_a = sol[b].x >= sol[a].x && sol[b].y >= sol[a].y && sol[b].x + sol[b].w <= sol[a].x + sol[a].w && sol[b].y + sol[b].h <= sol[a].y + sol[a].h;
				if (a_en_b || b_en_a) continue;
				if (sol[a].x < sol[b].x + sol[b].w && sol[b].x < sol[a].x + sol[a].w && sol[a].y < sol[b].y + sol[b].h && sol[b].y < sol[a].y + sol[a].h) {
					++n;
					if (!ej[0]) snprintf(ej, sizeof ej, "%s con %s", sol[a].n, sol[b].n);
				}
			}
		}
		AOEN_LOG("[AOEN] solapes: %d de %d bloques, %d fuera (pantalla lógica %dx%d) %s", n, nsol, fuera, fb.w, fb.h, ej);
	}
	nsol = 0;
}

/* ---------- texturas de los paneles con los gráficos originales ----------
 * INTERFAC.DRS trae, para cada estilo de civilización, la barra superior (cuadro 0) y la inferior (cuadro 1) de la interfaz
 * original (50737..50740 a 800 de ancho; 50741..50744 a 1024) y una textura para los diálogos (50210..50216). La barra
 * inferior tiene un motivo que se repite (relieves): se busca su periodo comparando columnas y se repite ese trozo, así la
 * barra crece a cualquier ancho sin costuras. Sin datos se dibuja un degradado de madera. Todo se guarda ya dibujado. */

int estilo_jugador(void) {
	if (!W.nents) return 0;
	return CLAMP(civ_def[W.pl[W.human].civ].style, 0, 3);
}

typedef struct { int tipo, estilo, w, h, us; u32 *px; } Textura;
static Textura cache[6];
static int cache_sig;

static Sprite *textura_origen(int tipo, int estilo, int *x0, int *periodo) {
	static const int orden[4] = { 0, 1, 3, 2 };   /* estilo (egipcio, griego, asiático, babilonio) -> orden de los gráficos */
	int k = orden[estilo & 3];
	Sprite *s = NULL;
	if (tipo == 2) {
		s = iface_frame(50210 + 2 * k, 0, 40);
		if (!s) s = iface_frame(50210, 0, 40);
		*x0 = 0; *periodo = s ? s->w : 0;
		return s;
	}
	s = iface_frame(50741 + k, tipo == 0 ? 0 : 1, 400);
	if (!s) s = iface_frame(50737 + k, tipo == 0 ? 0 : 1, 400);
	if (!s) return NULL;
	if (tipo == 0) { *x0 = s->w / 2; *periodo = s->w / 2 - 4; return s; }   /* la superior: la mitad derecha, sin iconos */
	/* la inferior: entre el marco del retrato (izquierda) y el hueco del minimapa (derecha); periodo por autocorrelación */
	int a = s->w * 17 / 100, lim = s->w * 66 / 100, mejor = lim - a, mejor_d = 1 << 30;
	for (int p = 120; p <= lim - a; ++p) {
		int d = 0;
		for (int y = 4; y < s->h - 4; y += 3) {
			u32 c0 = fb.pal[s->px[y * s->w + a]], c1 = fb.pal[s->px[y * s->w + a + p]];
			d += abs((int)((c0 >> 16) & 255) - (int)((c1 >> 16) & 255)) + abs((int)((c0 >> 8) & 255) - (int)((c1 >> 8) & 255)) + abs((int)(c0 & 255) - (int)(c1 & 255));
		}
		if (d < mejor_d) { mejor_d = d; mejor = p; }
	}
	*x0 = a; *periodo = mejor;
	return s;
}

static u32 *textura(int tipo, int w, int h) {
	int estilo = estilo_jugador(), us = app.us;
	for (int i = 0; i < ARRAY_LEN(cache); ++i)
		if (cache[i].px && cache[i].tipo == tipo && cache[i].estilo == estilo && cache[i].w == w && cache[i].h == h && cache[i].us == us) return cache[i].px;
	int x0 = 0, per = 0;
	Sprite *s = textura_origen(tipo, estilo, &x0, &per);
	if (!s || per <= 0) return NULL;
	Textura *t = &cache[cache_sig];
	cache_sig = (cache_sig + 1) % ARRAY_LEN(cache);
	free(t->px);
	t->px = (u32 *)malloc((size_t)w * h * 4);
	if (!t->px) return NULL;
	t->tipo = tipo; t->estilo = estilo; t->w = w; t->h = h; t->us = us;
	int sh = s->h - (tipo == 1 ? 2 : 0);
	for (int y = 0; y < h; ++y) {
		/* vertical: la barra superior se estira a su alto; la inferior y los diálogos se repiten */
		int ly = y / us;
		int sy = tipo == 0 ? ly * sh / MAX(1, h / us) : tipo == 1 ? (ly < sh ? ly : sh - 1 - (ly - sh) % sh) : ly % s->h;
		sy = CLAMP(sy, 0, s->h - 1);
		for (int x = 0; x < w; ++x) {
			int sx = x0 + (x / us) % per;
			u8 v = s->px[sy * s->w + sx];
			t->px[(size_t)y * w + x] = v ? fb.pal[v] : 0xff201810u;
		}
	}
	return t->px;
}

/* copia una textura ya dibujada (en píxeles reales) */
static int pegar_textura(int tipo, int x, int y, int w, int h) {
	int us = app.us, rx = x * us, ry = y * us, rw = w * us, rh = h * us;
	u32 *t = textura(tipo, rw, rh);
	if (!t) return 0;
	for (int yy = 0; yy < rh; ++yy) {
		int dy = ry + yy;
		if (dy < 0 || dy >= gfx_real_h()) continue;
		int a = MAX(0, rx), b = MIN(gfx_real_w(), rx + rw);
		if (b > a) memcpy(fb.px + (size_t)dy * fb.pitch + a, t + (size_t)yy * rw + (a - rx), (size_t)(b - a) * 4);
	}
	return 1;
}

void fondo_barra(int x, int y, int w, int h, int superior) {
	if (!pegar_textura(superior ? 0 : 1, x, y, w, h)) {
		if (superior) gfx_fill_grad(x, y, w, h, gfx_rgb(48, 32, 22), gfx_rgb(20, 13, 9));
		else gfx_fill_grad(x, y, w, h, gfx_rgb(40, 27, 19), gfx_rgb(16, 10, 7));
	}
	if (superior) { gfx_fill(x, y + h - 1, w, 1, COL_GOLD); gfx_fill(x, y + h, w, 2, gfx_rgb(12, 8, 6)); }
	else { gfx_fill(x, y, w, 1, COL_GOLD); gfx_fill(x, y + 1, w, 1, gfx_rgb(130, 100, 56)); gfx_fill(x, y - 2, w, 2, gfx_rgb(12, 8, 6)); }
}

void fondo_dialogo(int x, int y, int w, int h) {
	gfx_fill_a(x + 4, y + 4, w, h, gfx_rgb(0, 0, 0), 120);
	if (pegar_textura(2, x, y, w, h)) {
		/* la textura queda como marco; el interior se oscurece para que el texto claro se lea */
		gfx_fill_a(x + 10, y + 10, w - 20, h - 20, gfx_rgb(26, 17, 11), 222);
		bevel(x + 9, y + 9, w - 18, h - 18, gfx_rgb(90, 66, 34));
	} else {
		gfx_fill_grad(x, y, w, h, gfx_rgb(62, 42, 28), gfx_rgb(34, 22, 15));
		bevel(x + 1, y + 1, w - 2, h - 2, gfx_rgb(96, 70, 34));
		gfx_fill(x + 3, y + 2, w - 6, 1, gfx_rgb(130, 100, 56));
		gfx_fill(x + 3, y + h - 3, w - 6, 1, gfx_rgb(14, 9, 6));
	}
	bevel(x, y, w, h, COL_GOLD);
}
