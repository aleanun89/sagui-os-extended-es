// SPDX-License-Identifier: GPL-3.0-or-later
/* jb_escena.c - el dibujo del escenario (D377): el fondo, la capa del lápiz, los personajes con su giro y
 * tamaño, los bocadillos, los monitores de variables y la pregunta. Dibuja directamente a la medida de la
 * vista (el escenario lógico es de 480x360, centrado en (0, 0), con la y hacia arriba). */
#include "jb.h"

extern const unsigned char fonte_sagui[256][16];
int jb_zoom_escena;

static uint8_t *lapiz;          /* capa del lápiz: un índice de paleta por píxel lógico */
static bool lapiz_usado;

static void asegurar_lapiz(void)
{
    if (!lapiz) lapiz = aloca_zerado((size_t)ESC_L * ESC_A);
}
void jb_lapiz_borrar(void) { if (lapiz) memset(lapiz, 0, (size_t)ESC_L * ESC_A); lapiz_usado = false; }
void jb_lapiz_punto(int x, int y, int color, int g)
{
    asegurar_lapiz();
    if (!lapiz) return;
    int px = x + ESC_L / 2, py = ESC_A / 2 - y;
    int a = g / 2;
    for (int j = -a; j < g - a; j++)
        for (int i = -a; i < g - a; i++) {
            int xx = px + i, yy = py + j;
            if (xx >= 0 && yy >= 0 && xx < ESC_L && yy < ESC_A) lapiz[yy * ESC_L + xx] = (uint8_t)color;
        }
    lapiz_usado = true;
}
void jb_lapiz_linea(int x0, int y0, int x1, int y1, int color, int g)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1, sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx - dy;
    for (int n = 0; n < 2000; n++) {
        jb_lapiz_punto(x0, y0, color, g);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * e;
        if (e2 > -dy) { e -= dy; x0 += sx; }
        if (e2 < dx) { e += dx; y0 += sy; }
    }
}

int jb_color_en(int x, int y)
{
    int px = x + ESC_L / 2, py = ESC_A / 2 - y;
    if (px < 0 || py < 0 || px >= ESC_L || py >= ESC_A) return 0;
    if (lapiz && lapiz[py * ESC_L + px]) return lapiz[py * ESC_L + px];
    if (jb_p.fondo < 0 || jb_p.fondo >= jb_p.nfondos) return 0;
    return jb_p.fondos[jb_p.fondo]->px[(py / 10) * FONDO_L + px / 10];
}

/* ---- el giro de un personaje ---- */
static void giro_de(const pers_t *p, int *c, int *s, bool *voltear)
{
    *voltear = false;
    if (p->giro == 0) { *c = jb_coseno(p->dir - 90000); *s = jb_seno(p->dir - 90000); }
    else { *c = 10000; *s = 0; if (p->giro == 1 && jb_seno(p->dir) < 0) *voltear = true; }
}

/* ¿el punto lógico (X, Y) cae sobre un píxel opaco del personaje? */
static bool opaco_en(const pers_t *p, int X, int Y)
{
    if (p->traje < 0 || p->traje >= p->ntrajes) return false;
    int lado = TAM_BASE * p->tam / 100;
    if (lado < 1) return false;
    int c, s; bool fl;
    giro_de(p, &c, &s, &fl);
    int64_t dx = (int64_t)(X * 256 + 128) - (int64_t)p->x * 256 / U, dy = (int64_t)p->y * 256 / U - (int64_t)(Y * 256 + 128);
    int64_t u = (dx * c + dy * s) / 10000, v = (-dx * s + dy * c) / 10000;
    int64_t den = (int64_t)lado * 256, fu = u * 32 + 16 * den, fv = v * 32 + 16 * den;
    if (fu < 0 || fv < 0) return false;
    int cx = (int)(fu / den), cy = (int)(fv / den);
    if (fl) cx = 31 - cx;
    if (cx < 0 || cy < 0 || cx >= TRAJE_L || cy >= TRAJE_L) return false;
    return p->trajes[p->traje]->px[cy * TRAJE_L + cx] != 0;
}

int jb_pers_en(int x, int y)
{
    for (int i = jb_p.npers - 1; i >= 0; i--) {
        pers_t *p = jb_p.pers[i];
        if (p->visible && opaco_en(p, x, y)) return i;
    }
    return -1;
}

static void traje_a_logico(const pers_t *p, int cx, int cy, int *X, int *Y)
{
    int lado = TAM_BASE * p->tam / 100;
    int c, s; bool fl;
    giro_de(p, &c, &s, &fl);
    if (fl) cx = 31 - cx;
    int64_t u = ((int64_t)(cx - 16) * 2 + 1) * lado * 128 / 32, v = ((int64_t)(cy - 16) * 2 + 1) * lado * 128 / 32;   /* en 1/256 px */
    int64_t xp = (u * c - v * s) / 10000, yp = (u * s + v * c) / 10000;
    *X = (int)((int64_t)p->x * 256 / U + xp) >> 8;
    *Y = (int)((int64_t)p->y * 256 / U - yp) >> 8;
}

bool jb_pers_toca_color(const pers_t *p, int color)
{
    if (p->traje < 0 || p->traje >= p->ntrajes) return false;
    const traje_t *t = p->trajes[p->traje];
    for (int cy = p->sy0; cy <= p->sy1; cy++)
        for (int cx = p->sx0; cx <= p->sx1; cx++) {
            if (!t->px[cy * TRAJE_L + cx]) continue;
            int X, Y;
            traje_a_logico(p, cx, cy, &X, &Y);
            if (jb_color_en(X, Y) == color) return true;
        }
    return false;
}

bool jb_pers_toca_pers(const pers_t *a, const pers_t *b)
{
    int ax0, ay0, ax1, ay1, bx0, by0, bx1, by1;
    jb_pers_caja(a, &ax0, &ay0, &ax1, &ay1);
    jb_pers_caja(b, &bx0, &by0, &bx1, &by1);
    int ra = TAM_BASE * a->tam / 100, rb = TAM_BASE * b->tam / 100;
    /* con giro, la caja sin girar no vale: se amplía al círculo que encierra el traje */
    int ma = a->giro == 0 ? ra / 4 : 0, mb = b->giro == 0 ? rb / 4 : 0;
    if (ax1 + ma < bx0 - mb || bx1 + mb < ax0 - ma || ay1 + ma < by0 - mb || by1 + mb < ay0 - ma) return false;
    if (a->traje < 0 || a->traje >= a->ntrajes) return false;
    const traje_t *t = a->trajes[a->traje];
    for (int cy = a->sy0; cy <= a->sy1; cy += 1)
        for (int cx = a->sx0; cx <= a->sx1; cx += 1) {
            if (!t->px[cy * TRAJE_L + cx]) continue;
            int X, Y;
            traje_a_logico(a, cx, cy, &X, &Y);
            if (opaco_en(b, X, Y)) return true;
        }
    return false;
}

/* ============================================================ dibujo */

static void v_texto(uint32_t *d, int pitch, int vl, int va, int x, int y, const char *t, uint32_t cor)
{
    for (; *t; t++, x += 8) {
        if (x + 8 <= 0 || x >= vl) continue;
        const unsigned char *g = fonte_sagui[(uint8_t)*t];
        for (int l = 0; l < 16; l++) {
            int yy = y + l;
            if (yy < 0 || yy >= va) continue;
            for (int c = 0; c < 8; c++) if ((g[l] & (0x80 >> c)) && x + c >= 0 && x + c < vl) d[yy * pitch + x + c] = cor;
        }
    }
}
static void v_rect(uint32_t *d, int pitch, int vl, int va, int x, int y, int w, int h, uint32_t cor)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > vl) w = vl - x;
    if (y + h > va) h = va - y;
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) d[(y + j) * pitch + x + i] = cor;
}

static void dibujar_pers(uint32_t *d, int pitch, int vl, int va, const pers_t *p)
{
    if (!p->visible || p->traje < 0 || p->traje >= p->ntrajes) return;
    int lado = TAM_BASE * p->tam / 100;
    if (lado < 1) return;
    int c, s; bool fl;
    giro_de(p, &c, &s, &fl);
    int rad = lado * 3 / 4 + 2;                     /* radio que encierra el traje girado */
    int cxv = (int)((int64_t)(p->x / U + ESC_L / 2) * vl / ESC_L), cyv = (int)((int64_t)(ESC_A / 2 - p->y / U) * va / ESC_A);
    int rv = rad * vl / ESC_L + 2;
    int x0 = cxv - rv, x1 = cxv + rv, y0 = cyv - rv, y1 = cyv + rv;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > vl) x1 = vl;
    if (y1 > va) y1 = va;
    const uint8_t *px = p->trajes[p->traje]->px;
    int64_t pxs = (int64_t)p->x * 256 / U, pys = (int64_t)p->y * 256 / U;
    int64_t den = (int64_t)lado * 256;
    /* El giro es una transformación afín: se calcula el valor inicial una vez y se avanza sumando
     * (punto fijo 16.16), sin divisiones por píxel (los Pentium II no tienen prisa pero tampoco paciencia). */
    int64_t X0 = ((int64_t)(2 * x0 + 1) * ESC_L * 128) / vl - (int64_t)ESC_L / 2 * 256;
    int64_t Y0 = ((int64_t)(2 * y0 + 1) * ESC_A * 128) / va - (int64_t)ESC_A / 2 * 256 + pys;
    int64_t dx0 = X0 - pxs;
    int32_t fu0 = (int32_t)(((dx0 * c + Y0 * s) / 10000 * 32 * 65536) / den + 16 * 65536);
    int32_t fv0 = (int32_t)(((-dx0 * s + Y0 * c) / 10000 * 32 * 65536) / den + 16 * 65536);
    int32_t fu_x = (int32_t)((int64_t)ESC_L * 256 * c * 32 * 65536 / ((int64_t)vl * 10000 * den));
    int32_t fv_x = (int32_t)(-(int64_t)ESC_L * 256 * s * 32 * 65536 / ((int64_t)vl * 10000 * den));
    int32_t fu_y = (int32_t)((int64_t)ESC_A * 256 * s * 32 * 65536 / ((int64_t)va * 10000 * den));
    int32_t fv_y = (int32_t)((int64_t)ESC_A * 256 * c * 32 * 65536 / ((int64_t)va * 10000 * den));
    for (int vy = y0; vy < y1; vy++) {
        int32_t fu = fu0 + (vy - y0) * fu_y, fv = fv0 + (vy - y0) * fv_y;
        uint32_t *o = d + vy * pitch;
        for (int vx = x0; vx < x1; vx++, fu += fu_x, fv += fv_x) {
            int cx = fu >> 16, cy = fv >> 16;
            if ((unsigned)cx >= TRAJE_L || (unsigned)cy >= TRAJE_L) continue;
            if (fl) cx = 31 - cx;
            uint8_t ci = px[cy * TRAJE_L + cx];
            if (ci) o[vx] = jb_paleta[ci];
        }
    }
}

static void bocadillo(uint32_t *d, int pitch, int vl, int va, const pers_t *p)
{
    if (!p->burbuja[0]) return;
    int lado = TAM_BASE * p->tam / 100;
    int cxv = (int)((int64_t)(p->x / U + ESC_L / 2) * vl / ESC_L), cyv = (int)((int64_t)(ESC_A / 2 - p->y / U) * va / ESC_A);
    int maxc = vl / 8 - 3;
    if (maxc > 24) maxc = 24;
    if (maxc < 6) maxc = 6;
    char lin[4][28];
    int nl = 0, n = 0, ancho = 0;
    const char *t = p->burbuja;
    memset(lin, 0, sizeof lin);
    for (; *t && nl < 4; t++) {
        lin[nl][n++] = *t;
        if (n >= maxc || (*t == ' ' && n >= maxc - 6)) { lin[nl][n] = 0; if (n > ancho) ancho = n; nl++; n = 0; }
    }
    if (n && nl < 4) { lin[nl][n] = 0; if (n > ancho) ancho = n; nl++; }
    int w = ancho * 8 + 10, h = nl * 16 + 8;
    int x = cxv - w / 2, y = cyv - lado * va / ESC_A / 2 - h - 6;
    if (x < 1) x = 1;
    if (x + w > vl - 1) x = vl - 1 - w;
    if (y < 1) y = cyv + lado * va / ESC_A / 2 + 6;
    v_rect(d, pitch, vl, va, x - 1, y - 1, w + 2, h + 2, 0x6A737C);
    v_rect(d, pitch, vl, va, x, y, w, h, 0xFFFFFF);
    for (int i = 0; i < nl; i++) v_texto(d, pitch, vl, va, x + 5, y + 4 + i * 16, lin[i], 0x1E2328);
    if (p->burb_pensar) { v_rect(d, pitch, vl, va, cxv - 2, y + h + 2, 4, 4, 0xFFFFFF); }
}

void jb_escena_dibujar(uint32_t *d, int pitch, int vl, int va)
{
    /* fondo */
    const fondo_t *f = jb_p.fondo >= 0 && jb_p.fondo < jb_p.nfondos ? jb_p.fondos[jb_p.fondo] : NULL;
    int tabx[2048];
    if (vl > 2048) vl = 2048;
    for (int x = 0; x < vl; x++) tabx[x] = x * FONDO_L / vl;
    for (int y = 0; y < va; y++) {
        const uint8_t *fila = f ? f->px + (y * FONDO_A / va) * FONDO_L : NULL;
        uint32_t *o = d + y * pitch;
        if (!fila) { for (int x = 0; x < vl; x++) o[x] = 0xFFFFFF; continue; }
        for (int x = 0; x < vl; x++) o[x] = jb_paleta[fila[tabx[x]] ? fila[tabx[x]] : 6];
    }
    /* lápiz */
    if (lapiz && lapiz_usado) {
        for (int y = 0; y < va; y++) {
            const uint8_t *fila = lapiz + (y * ESC_A / va) * ESC_L;
            uint32_t *o = d + y * pitch;
            for (int x = 0; x < vl; x++) { uint8_t c = fila[x * ESC_L / vl]; if (c) o[x] = jb_paleta[c]; }
        }
    }
    for (int i = 0; i < jb_p.npers; i++) dibujar_pers(d, pitch, vl, va, jb_p.pers[i]);
    for (int i = 0; i < jb_p.npers; i++) bocadillo(d, pitch, vl, va, jb_p.pers[i]);
    /* monitores de variables */
    int my = 3;
    for (int i = 0; i < jb_p.nvars; i++) {
        const var_t *v = &jb_p.vars[i];
        if (!v->mostrar) continue;
        char val[48], txt[80];
        if (v->es_t) copia_texto(val, v->t, sizeof val); else jb_num_texto(v->n, val, sizeof val);
        formatar_texto(txt, sizeof txt, "%s: %s", v->nombre, val);
        int w = (int)tam_texto(txt) * 8 + 8;
        if (w > vl - 6) w = vl - 6;
        v_rect(d, pitch, vl, va, 3, my, w, 20, 0x6A737C);
        v_rect(d, pitch, vl, va, 4, my + 1, w - 2, 18, 0xFF8C1A);
        v_texto(d, pitch, vl, va, 7, my + 2, txt, 0xFFFFFF);
        my += 22;
    }
    /* pregunta */
    if (jb_hay_pregunta) {
        int h = 40;
        v_rect(d, pitch, vl, va, 2, va - h - 2, vl - 4, h, 0x6A737C);
        v_rect(d, pitch, vl, va, 3, va - h - 1, vl - 6, h - 2, 0xFFFFFF);
        v_texto(d, pitch, vl, va, 8, va - h + 1, jb_pregunta, 0x1E2328);
        v_rect(d, pitch, vl, va, 8, va - 20, vl - 16, 17, 0xE2E6EA);
        char r[70];
        formatar_texto(r, sizeof r, "%s_", jb_respuesta);
        v_texto(d, pitch, vl, va, 10, va - 20, r, 0x1E2328);
    }
}

/* una miniatura de un traje a lado x lado píxeles en (px, py) */
void jb_escena_traje(uint32_t *d, int pitch, int cl, int ca, const traje_t *t, int px, int py, int lado)
{
    for (int y = 0; y < lado; y++) {
        int yy = py + y;
        if (yy < 0 || yy >= ca) continue;
        for (int x = 0; x < lado; x++) {
            int xx = px + x;
            if (xx < 0 || xx >= cl) continue;
            uint8_t c = t->px[(y * TRAJE_L / lado) * TRAJE_L + x * TRAJE_L / lado];
            if (c) d[yy * pitch + xx] = jb_paleta[c];
        }
    }
}
