// SPDX-License-Identifier: GPL-3.0-or-later
/* arara_dibujo.h - cómo se dibuja un diagrama de Arara (D465).
 *
 * Una sola descripción de las figuras (dib_fig, dib_con) que se envía a un «sumidero» (sink_t) con polígonos, elipses,
 * líneas y texto en coordenadas del mundo. Hay dos sumideros: el rasterizador (la pantalla, el PNG y el PDF, que
 * incrusta la trama) y el de SVG, que escribe el texto vectorial. Solo enteros; el rasterizador no suaviza los bordes
 * (los trazos gruesos se hacen con polígonos y discos). */
#ifndef ARARA_DIBUJO_H
#define ARARA_DIBUJO_H

extern const unsigned char fonte_sagui[256][16];

typedef struct {
    void *ctx;
    void (*poli)(void *ctx, const int *xy, int n, uint32_t relleno, uint32_t linea, int grosor);      /* cerrado */
    void (*abierta)(void *ctx, const int *xy, int n, uint32_t linea, int grosor);                      /* poligonal */
    void (*elipse)(void *ctx, int cx, int cy, int rx, int ry, uint32_t relleno, uint32_t linea, int grosor);
    void (*texto)(void *ctx, int x, int y, int l, int a, const char *t, uint32_t color, bool izq);     /* en el rectángulo */
} sink_t;

static uint32_t cmezcla(uint32_t a, uint32_t b, int t)      /* a*(100-t)% + b*t%, conserva el alfa de a */
{
    uint32_t r = a & 0xFF000000u;
    for (int s = 0; s <= 16; s += 8) {
        int ca = (int)((a >> s) & 255), cb = (int)((b >> s) & 255);
        r |= (uint32_t)((ca * (100 - t) + cb * t) / 100) << s;
    }
    return r;
}
static uint32_t color_texto(uint32_t relleno)
{
    if ((relleno >> 24) < 128) return 0xFF000000;
    int lum = (int)(((relleno >> 16) & 255) * 299 + ((relleno >> 8) & 255) * 587 + (relleno & 255) * 114) / 1000;
    return lum < 120 ? 0xFFFFFFFF : 0xFF000000;
}

#define PMAX 160
static int arco(int *xy, int n, int cx, int cy, int rx, int ry, int g0, int g1, int pasos)
{
    for (int k = 0; k <= pasos; k++) {
        int g = g0 + (g1 - g0) * k / pasos;
        xy[2 * n] = cx + rx * edvista_seno(g + 90) / 1000;
        xy[2 * n + 1] = cy + ry * edvista_seno(g) / 1000;
        n++;
    }
    return n;
}
static void rect4(int *xy, int x, int y, int l, int a)
{
    xy[0] = x; xy[1] = y; xy[2] = x + l; xy[3] = y; xy[4] = x + l; xy[5] = y + a; xy[6] = x; xy[7] = y + a;
}
static void dib_rect(const sink_t *s, int x, int y, int l, int a, uint32_t rel, uint32_t lin, int g)
{
    int xy[8];
    rect4(xy, x, y, l, a);
    s->poli(s->ctx, xy, 4, rel, lin, g);
}

/* Dibuja una figura. */
static void dib_fig(const sink_t *s, const fig_t *f)
{
    int xy[2 * PMAX], n = 0;
    int x = f->x, y = f->y, l = f->l, a = f->a, g = f->grosor;
    uint32_t rel = f->relleno, lin = f->linea;
    int tx = x + 4, ty = y + 2, tl = l - 8, ta = a - 4;      /* área del texto */
    uint32_t tc = color_texto(rel);
    bool izq = false;
    switch (f->tipo) {
    case T_RECT: case T_PROCESO:
        dib_rect(s, x, y, l, a, rel, lin, g);
        break;
    case T_TEXTO:
        if ((rel >> 24) || (lin >> 24)) dib_rect(s, x, y, l, a, rel, lin, g);
        tc = (rel >> 24) < 128 ? 0xFF000000 : tc;
        break;
    case T_REDOND: {
        int r = (l < a ? l : a) / 4;
        if (r > 20) r = 20;
        n = arco(xy, n, x + r, y + r, r, r, 180, 270, 6);
        n = arco(xy, n, x + l - r, y + r, r, r, 270, 360, 6);
        n = arco(xy, n, x + l - r, y + a - r, r, r, 0, 90, 6);
        n = arco(xy, n, x + r, y + a - r, r, r, 90, 180, 6);
        s->poli(s->ctx, xy, n, rel, lin, g);
        break;
    }
    case T_ELIPSE:
        s->elipse(s->ctx, x + l / 2, y + a / 2, l / 2, a / 2, rel, lin, g);
        tx = x + l / 8; ty = y + a / 8; tl = l * 3 / 4; ta = a * 3 / 4;
        break;
    case T_ROMBO: case T_DECISION:
        xy[0] = x + l / 2; xy[1] = y; xy[2] = x + l; xy[3] = y + a / 2; xy[4] = x + l / 2; xy[5] = y + a; xy[6] = x; xy[7] = y + a / 2;
        s->poli(s->ctx, xy, 4, rel, lin, g);
        tx = x + l / 4; ty = y + a / 4; tl = l / 2; ta = a / 2;
        break;
    case T_TRIANGULO:
        xy[0] = x + l / 2; xy[1] = y; xy[2] = x + l; xy[3] = y + a; xy[4] = x; xy[5] = y + a;
        s->poli(s->ctx, xy, 3, rel, lin, g);
        tx = x + l / 4; ty = y + a / 2; tl = l / 2; ta = a / 2 - 2;
        break;
    case T_INICIO: {
        int r = a / 2;
        n = arco(xy, n, x + r, y + r, r, r, 90, 270, 10);
        n = arco(xy, n, x + l - r, y + r, r, r, 270, 450, 10);
        s->poli(s->ctx, xy, n, rel, lin, g);
        tx = x + r / 2; tl = l - r;
        break;
    }
    case T_ES: {
        int k = l / 5;
        xy[0] = x + k; xy[1] = y; xy[2] = x + l; xy[3] = y; xy[4] = x + l - k; xy[5] = y + a; xy[6] = x; xy[7] = y + a;
        s->poli(s->ctx, xy, 4, rel, lin, g);
        tx = x + k; tl = l - 2 * k;
        break;
    }
    case T_DOCUMENTO: {
        int am = a / 10;
        xy[0] = x; xy[1] = y; xy[2] = x + l; xy[3] = y; n = 2;
        for (int i = 0; i <= 16; i++) {
            xy[2 * n] = x + l - l * i / 16;
            xy[2 * n + 1] = y + a - am - am * edvista_seno(360 * i / 16) / 1000;
            n++;
        }
        s->poli(s->ctx, xy, n, rel, lin, g);
        ta = a * 8 / 10 - 4;
        break;
    }
    case T_BD: {
        int ry = a / 6;
        if (ry < 4) ry = 4;
        xy[0] = x; xy[1] = y + ry; n = 1;
        xy[2] = x; xy[3] = y + a - ry; n = 2;
        n = arco(xy, n, x + l / 2, y + a - ry, l / 2, ry, 180, 0, 12);
        xy[2 * n] = x + l; xy[2 * n + 1] = y + ry; n++;
        s->poli(s->ctx, xy, n, rel, lin, g);
        s->elipse(s->ctx, x + l / 2, y + ry, l / 2, ry, cmezcla(rel, 0xFFFFFFFF, 45), lin, g);
        ty = y + ry * 2; ta = a - ry * 3 - 2;
        break;
    }
    case T_PERSONA: {
        n = 0;
        int r = 6;
        n = arco(xy, n, x + r, y + r, r, r, 180, 270, 4);
        n = arco(xy, n, x + l - r, y + r, r, r, 270, 360, 4);
        n = arco(xy, n, x + l - r, y + a - r, r, r, 0, 90, 4);
        n = arco(xy, n, x + r, y + a - r, r, r, 90, 180, 4);
        s->poli(s->ctx, xy, n, rel, lin, g);
        uint32_t ic = cmezcla(lin, 0xFFFFFFFF, 25);
        s->elipse(s->ctx, x + a * 38 / 100, y + a * 34 / 100, a * 15 / 100, a * 15 / 100, ic, lin, 1);
        s->elipse(s->ctx, x + a * 38 / 100, y + a * 77 / 100, a * 24 / 100, a * 15 / 100, ic, lin, 1);
        tx = x + a * 3 / 4; tl = l - a * 3 / 4 - 4; ty = y + 4; ta = a - 8;
        break;
    }
    case T_NUBE: {
        int ia = a - 18;
        n = 0;
        for (int k = 0; k < 84; k++) {                        /* elipse con 7 lóbulos */
            int gr = k * 360 / 84;
            int sb = edvista_seno(gr * 7 / 2);
            if (sb < 0) sb = -sb;
            int rr = 840 + 160 * sb / 1000;
            xy[2 * n] = x + l / 2 + (l / 2) * rr / 1000 * edvista_seno(gr + 90) / 1000;
            xy[2 * n + 1] = y + ia / 2 + (ia / 2) * rr / 1000 * edvista_seno(gr) / 1000;
            n++;
        }
        s->poli(s->ctx, xy, n, rel, lin, g);
        tx = x; ty = y + a - 18; tl = l; ta = 18; tc = 0xFF000000;
        break;
    }
    case T_PC: {
        int ia = a - 18, mh = ia * 70 / 100;
        dib_rect(s, x + l * 8 / 100, y, l * 84 / 100, mh, rel, lin, g);
        dib_rect(s, x + l * 14 / 100, y + mh * 12 / 100, l * 72 / 100, mh * 76 / 100, 0xFFD6EAF8, cmezcla(lin, 0xFFFFFFFF, 30), 1);
        dib_rect(s, x + l * 44 / 100, y + mh, l * 12 / 100, ia * 10 / 100, rel, lin, 1);
        dib_rect(s, x + l * 28 / 100, y + mh + ia * 10 / 100, l * 44 / 100, ia * 6 / 100, rel, lin, 1);
        dib_rect(s, x + l * 12 / 100, y + ia * 88 / 100, l * 76 / 100, ia * 12 / 100, cmezcla(rel, 0xFFFFFFFF, 30), lin, 1);
        tx = x; ty = y + a - 18; tl = l; ta = 18; tc = 0xFF000000;
        break;
    }
    case T_SERVIDOR: {
        int ia = a - 18, h3 = ia / 3;
        for (int k = 0; k < 3; k++) {
            int yy = y + k * h3;
            dib_rect(s, x + l / 6, yy, l * 2 / 3, h3 - 3, rel, lin, 1);
            s->elipse(s->ctx, x + l / 6 + 8, yy + (h3 - 3) / 2, 2, 2, k == 1 ? 0xFFE0A030 : 0xFF40B040, 0, 1);
            xy[0] = x + l / 6 + 18; xy[1] = yy + (h3 - 3) / 2; xy[2] = x + l * 5 / 6 - 8; xy[3] = yy + (h3 - 3) / 2;
            s->abierta(s->ctx, xy, 2, lin, 1);
        }
        tx = x; ty = y + a - 18; tl = l; ta = 18; tc = 0xFF000000;
        break;
    }
    case T_ROUTER: {
        int ia = a - 18, by = y + ia * 45 / 100, bh = ia * 50 / 100;
        xy[0] = x + l * 25 / 100; xy[1] = by; xy[2] = x + l * 15 / 100; xy[3] = y + 2; s->abierta(s->ctx, xy, 2, lin, 2);
        xy[0] = x + l * 75 / 100; xy[2] = x + l * 85 / 100; s->abierta(s->ctx, xy, 2, lin, 2);
        n = 0;
        n = arco(xy, n, x + 8, by + 8, 8, 8, 180, 270, 4);
        n = arco(xy, n, x + l - 8, by + 8, 8, 8, 270, 360, 4);
        n = arco(xy, n, x + l - 8, by + bh - 4, 4, 4, 0, 90, 3);
        n = arco(xy, n, x + 8, by + bh - 4, 4, 4, 90, 180, 3);
        s->poli(s->ctx, xy, n, rel, lin, g);
        for (int k = 0; k < 3; k++) s->elipse(s->ctx, x + l / 4 + k * 12, by + bh / 2, 2, 2, k == 0 ? 0xFF40B040 : 0xFF9ABF9A, 0, 1);
        xy[0] = x + l * 60 / 100; xy[1] = by + bh / 2; xy[2] = x + l * 85 / 100; xy[3] = by + bh / 2; s->abierta(s->ctx, xy, 2, lin, 1);
        tx = x; ty = y + a - 18; tl = l; ta = 18; tc = 0xFF000000;
        break;
    }
    case T_IMPRESORA: {
        int ia = a - 18;
        dib_rect(s, x + l * 25 / 100, y, l * 50 / 100, ia * 36 / 100, 0xFFFFFFFF, lin, 1);
        dib_rect(s, x + l * 5 / 100, y + ia * 30 / 100, l * 90 / 100, ia * 42 / 100, rel, lin, g);
        dib_rect(s, x + l * 22 / 100, y + ia * 58 / 100, l * 56 / 100, ia * 42 / 100, 0xFFFFFFFF, lin, 1);
        xy[0] = x + l * 30 / 100; xy[1] = y + ia * 72 / 100; xy[2] = x + l * 70 / 100; xy[3] = y + ia * 72 / 100; s->abierta(s->ctx, xy, 2, lin, 1);
        xy[1] = xy[3] = y + ia * 86 / 100; s->abierta(s->ctx, xy, 2, lin, 1);
        s->elipse(s->ctx, x + l * 80 / 100, y + ia * 42 / 100, 2, 2, 0xFF40B040, 0, 1);
        tx = x; ty = y + a - 18; tl = l; ta = 18; tc = 0xFF000000;
        break;
    }
    }
    if (f->texto[0]) s->texto(s->ctx, tx, ty, tl, ta, f->texto, tc, izq);
}

/* Dibuja un conector con sus puntas de flecha. */
static void dib_con(const sink_t *s, const pag_t *p, const con_t *c)
{
    int pts[RUTA_MAX][2], xy[2 * RUTA_MAX];
    int n = con_ruta(p, c, pts);
    for (int i = 0; i < n; i++) { xy[2 * i] = pts[i][0]; xy[2 * i + 1] = pts[i][1]; }
    s->abierta(s->ctx, xy, n, c->linea, c->grosor);
    int fl[6];
    if (c->flecha_fin) { flecha_puntos(pts[n - 2][0], pts[n - 2][1], pts[n - 1][0], pts[n - 1][1], c->grosor, fl); s->poli(s->ctx, fl, 3, c->linea, c->linea, 1); }
    if (c->flecha_ini) { flecha_puntos(pts[1][0], pts[1][1], pts[0][0], pts[0][1], c->grosor, fl); s->poli(s->ctx, fl, 3, c->linea, c->linea, 1); }
}

static void dib_pagina(const sink_t *s, const pag_t *p)
{
    for (int i = 0; i < p->nfig; i++) dib_fig(s, &p->fig[i]);
    for (int i = 0; i < p->ncon; i++) dib_con(s, p, &p->con[i]);
}

/* Rectángulo que ocupa el contenido de la página; false si está vacía. */
static bool pag_limites(const pag_t *p, int *x0, int *y0, int *x1, int *y1)
{
    bool hay = false;
    int ax = 0, ay = 0, bx = 0, by = 0;
    for (int i = 0; i < p->nfig; i++) {
        const fig_t *f = &p->fig[i];
        if (!hay) { ax = f->x; ay = f->y; bx = f->x + f->l; by = f->y + f->a; hay = true; continue; }
        if (f->x < ax) ax = f->x;
        if (f->y < ay) ay = f->y;
        if (f->x + f->l > bx) bx = f->x + f->l;
        if (f->y + f->a > by) by = f->y + f->a;
    }
    for (int i = 0; i < p->ncon; i++) {
        int pts[RUTA_MAX][2];
        int n = con_ruta(p, &p->con[i], pts);
        for (int k = 0; k < n; k++) {
            if (!hay) { ax = bx = pts[k][0]; ay = by = pts[k][1]; hay = true; continue; }
            if (pts[k][0] < ax) ax = pts[k][0];
            if (pts[k][1] < ay) ay = pts[k][1];
            if (pts[k][0] > bx) bx = pts[k][0];
            if (pts[k][1] > by) by = pts[k][1];
        }
    }
    *x0 = ax; *y0 = ay; *x1 = bx; *y1 = by;
    return hay;
}

/* ------------------------------------------------------------ rasterizador */

typedef struct {
    uint32_t *px;
    int stride;
    int cx0, cy0, cx1, cy1;      /* recorte (en píxeles del destino, cx1/cy1 exclusivos) */
    int ox, oy, zoom;            /* píxel donde cae el origen del mundo y zoom en milésimas */
    int ts;                      /* escala del texto */
} rd_t;

#define DEV_MAX 20000
static int rd_x(const rd_t *r, int wx) { return limitar(r->ox + fdivi(wx * r->zoom, 1000), -DEV_MAX, DEV_MAX); }
static int rd_y(const rd_t *r, int wy) { return limitar(r->oy + fdivi(wy * r->zoom, 1000), -DEV_MAX, DEV_MAX); }

static void rd_pix(const rd_t *r, int x, int y, uint32_t c)
{
    if (x < r->cx0 || y < r->cy0 || x >= r->cx1 || y >= r->cy1) return;
    unsigned al = c >> 24;
    uint32_t *d = &r->px[y * r->stride + x];
    if (al >= 255) { *d = c & 0xFFFFFF; return; }
    if (al == 0) return;
    uint32_t o = *d;
    int rr = (int)((((c >> 16) & 255) * al + ((o >> 16) & 255) * (255 - al)) / 255);
    int gg = (int)((((c >> 8) & 255) * al + ((o >> 8) & 255) * (255 - al)) / 255);
    int bb = (int)(((c & 255) * al + (o & 255) * (255 - al)) / 255);
    *d = (uint32_t)((rr << 16) | (gg << 8) | bb);
}
static void rd_span(const rd_t *r, int y, int xa, int xb, uint32_t c)
{
    if (y < r->cy0 || y >= r->cy1) return;
    if (xa < r->cx0) xa = r->cx0;
    if (xb > r->cx1) xb = r->cx1;
    if ((c >> 24) == 255) { uint32_t *d = &r->px[y * r->stride]; for (int x = xa; x < xb; x++) d[x] = c & 0xFFFFFF; }
    else for (int x = xa; x < xb; x++) rd_pix(r, x, y, c);
}

/* Relleno por barrido, par-impar. xy en píxeles del destino. */
static void rd_rellenar(const rd_t *r, const int *xy, int n, uint32_t c)
{
    if (n < 3 || (c >> 24) == 0) return;
    int ymin = xy[1], ymax = xy[1];
    for (int i = 1; i < n; i++) { if (xy[2 * i + 1] < ymin) ymin = xy[2 * i + 1]; if (xy[2 * i + 1] > ymax) ymax = xy[2 * i + 1]; }
    if (ymin < r->cy0) ymin = r->cy0;
    if (ymax >= r->cy1) ymax = r->cy1 - 1;
    for (int y = ymin; y <= ymax; y++) {
        int xs[64], k = 0;
        for (int i = 0, j = n - 1; i < n; j = i++) {
            int y0 = xy[2 * j + 1], y1 = xy[2 * i + 1];
            if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
                int x0 = xy[2 * j], x1 = xy[2 * i];
                if (k < 64) xs[k++] = x0 + (y - y0) * (x1 - x0) / (y1 - y0);
            }
        }
        for (int a = 1; a < k; a++) { int v = xs[a], b = a - 1; while (b >= 0 && xs[b] > v) { xs[b + 1] = xs[b]; b--; } xs[b + 1] = v; }
        for (int a = 0; a + 1 < k; a += 2) rd_span(r, y, xs[a], xs[a + 1], c);
    }
}

static void rd_disco(const rd_t *r, int cx, int cy, int rx, int ry, uint32_t c)
{
    if ((c >> 24) == 0 || rx < 0 || ry < 0) return;
    if (ry == 0) { rd_span(r, cy, cx - rx, cx + rx + 1, c); return; }
    for (int dy = -ry; dy <= ry; dy++) {
        int h = rx * (int)raiz_u((unsigned)(ry * ry - dy * dy)) / ry;
        rd_span(r, cy + dy, cx - h, cx + h + 1, c);
    }
}

static void rd_linea1(const rd_t *r, int x0, int y0, int x1, int y1, uint32_t c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (int n = 0; n < 60000; n++) {
        rd_pix(r, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* Trazo grueso: a lo largo del eje mayor, un tramo de píxeles por columna (o fila) con el grosor medido en perpendicular. */
static void rd_trazo(const rd_t *r, int x0, int y0, int x1, int y1, int w, uint32_t c)
{
    if (w <= 1) { rd_linea1(r, x0, y0, x1, y1, c); return; }
    int dx = x1 - x0, dy = y1 - y0;
    int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    unsigned len = raiz_u((unsigned)(dx * dx) + (unsigned)(dy * dy));
    if (len == 0) { rd_disco(r, x0, y0, w / 2, w / 2, c); return; }
    if (adx >= ady) {
        if (x1 < x0) { int t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; dy = -dy; dx = -dx; }
        int grosor = (int)(w * len / (unsigned)adx);
        int xa = x0 < r->cx0 ? r->cx0 : x0, xb = x1 >= r->cx1 ? r->cx1 - 1 : x1;
        for (int x = xa; x <= xb; x++) {
            int yc = y0 + (dx ? (dy * (x - x0) + (dy >= 0 ? dx / 2 : -dx / 2)) / dx : 0);
            int ya = yc - (grosor - 1) / 2, yb = ya + grosor - 1;
            if (ya < r->cy0) ya = r->cy0;
            if (yb >= r->cy1) yb = r->cy1 - 1;
            for (int y = ya; y <= yb; y++) rd_pix(r, x, y, c);
        }
    } else {
        if (y1 < y0) { int t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; dx = -dx; dy = -dy; }
        int grosor = (int)(w * len / (unsigned)ady);
        int ya = y0 < r->cy0 ? r->cy0 : y0, yb = y1 >= r->cy1 ? r->cy1 - 1 : y1;
        for (int y = ya; y <= yb; y++) {
            int xc = x0 + (dy ? (dx * (y - y0) + (dx >= 0 ? dy / 2 : -dy / 2)) / dy : 0);
            int xa = xc - (grosor - 1) / 2, xb = xa + grosor - 1;
            if (xa < r->cx0) xa = r->cx0;
            if (xb >= r->cx1) xb = r->cx1 - 1;
            for (int x = xa; x <= xb; x++) rd_pix(r, x, y, c);
        }
    }
    if (w >= 3) { rd_disco(r, x0, y0, w / 2, w / 2, c); rd_disco(r, x1, y1, w / 2, w / 2, c); }
}

static int rd_grosor(const rd_t *r, int g)
{
    int w = g * r->zoom / 1000;
    return w < 1 ? 1 : w;
}

static void rd_s_poli(void *ctx, const int *xy, int n, uint32_t rel, uint32_t lin, int g)
{
    const rd_t *r = ctx;
    int d[2 * PMAX];
    if (n > PMAX) n = PMAX;
    for (int i = 0; i < n; i++) { d[2 * i] = rd_x(r, xy[2 * i]); d[2 * i + 1] = rd_y(r, xy[2 * i + 1]); }
    rd_rellenar(r, d, n, rel);
    if ((lin >> 24) == 0) return;
    int w = rd_grosor(r, g);
    for (int i = 0; i < n; i++) { int k = (i + 1) % n; rd_trazo(r, d[2 * i], d[2 * i + 1], d[2 * k], d[2 * k + 1], w, lin); }
}
static void rd_s_abierta(void *ctx, const int *xy, int n, uint32_t lin, int g)
{
    const rd_t *r = ctx;
    int w = rd_grosor(r, g);
    for (int i = 0; i + 1 < n; i++) rd_trazo(r, rd_x(r, xy[2 * i]), rd_y(r, xy[2 * i + 1]), rd_x(r, xy[2 * i + 2]), rd_y(r, xy[2 * i + 3]), w, lin);
}
static void rd_s_elipse(void *ctx, int cx, int cy, int rx, int ry, uint32_t rel, uint32_t lin, int g)
{
    const rd_t *r = ctx;
    int dcx = rd_x(r, cx), dcy = rd_y(r, cy), drx = rx * r->zoom / 1000, dry = ry * r->zoom / 1000;
    if (drx > DEV_MAX) drx = DEV_MAX;
    if (dry > DEV_MAX) dry = DEV_MAX;
    rd_disco(r, dcx, dcy, drx, dry, rel);
    if ((lin >> 24) == 0) return;
    int w = rd_grosor(r, g), px = 0, py = 0;
    int pasos = 8 + (drx + dry) / 6;
    if (pasos > 96) pasos = 96;
    for (int k = 0; k <= pasos; k++) {
        int gr = 360 * k / pasos;
        int xx = dcx + drx * edvista_seno(gr + 90) / 1000, yy = dcy + dry * edvista_seno(gr) / 1000;
        if (k) rd_trazo(r, px, py, xx, yy, w, lin);
        px = xx; py = yy;
    }
}
static void rd_s_texto(void *ctx, int x, int y, int l, int a, const char *t, uint32_t col, bool izq)
{
    const rd_t *r = ctx;
    int ts = r->ts;
    int dx = rd_x(r, x), dy = rd_y(r, y), dl = l * r->zoom / 1000, da = a * r->zoom / 1000;
    int cpl = dl / (8 * ts);
    if (cpl < 1) return;
    char lin[LIN_MAX][LIN_LONG];
    int nl = texto_lineas(t, cpl, lin);
    int alto = nl * 16 * ts;
    int y0 = dy + (da - alto) / 2;
    if (y0 < dy - 4 * ts && nl > 1) y0 = dy;
    for (int k = 0; k < nl; k++) {
        int ln = (int)tam_texto(lin[k]);
        int x0 = izq ? dx : dx + (dl - ln * 8 * ts) / 2;
        for (int i = 0; i < ln; i++) {
            const unsigned char *gl = fonte_sagui[(unsigned char)lin[k][i]];
            int gx = x0 + i * 8 * ts, gy = y0 + k * 16 * ts;
            if (gx > r->cx1 || gx + 8 * ts < r->cx0 || gy > r->cy1 || gy + 16 * ts < r->cy0) continue;
            for (int row = 0; row < 16; row++) {
                if (!gl[row]) continue;
                for (int cc = 0; cc < 8; cc++)
                    if (gl[row] & (0x80 >> cc)) {
                        if (ts == 1) rd_pix(r, gx + cc, gy + row, col);
                        else for (int sy = 0; sy < ts; sy++) rd_span(r, gy + row * ts + sy, gx + cc * ts, gx + cc * ts + ts, col);
                    }
            }
        }
    }
}

static sink_t rd_sink(rd_t *r)
{
    sink_t s = { r, rd_s_poli, rd_s_abierta, rd_s_elipse, rd_s_texto };
    return s;
}

/* Pinta la página en una trama nueva con margen m (px del mundo) y escala 1 o 2. Devuelve false si no cabe. */
#define EXPORT_PX_MAX (6 * 1024 * 1024)
static bool pag_a_imagen(const pag_t *p, int escala, int max_px, imagem_t *img)
{
    int x0, y0, x1, y1;
    if (!pag_limites(p, &x0, &y0, &x1, &y1)) { x0 = y0 = 0; x1 = 200; y1 = 120; }
    int m = 16;
    int l = x1 - x0 + 2 * m, a = y1 - y0 + 2 * m;
    if (l < 64) l = 64;
    if (a < 64) a = 64;
    if (escala == 2 && l * 2 * a * 2 > max_px) escala = 1;
    if (l * a * escala * escala > EXPORT_PX_MAX * 2 || l * escala > 8000 || a * escala > 8000) return false;
    if (!imagem_criar(img, l * escala, a * escala, 0xFFFFFF)) return false;
    rd_t r = { img->px, img->larg, 0, 0, img->larg, img->alt, (m - x0) * escala, (m - y0) * escala, escala * 1000, escala };
    sink_t s = rd_sink(&r);
    dib_pagina(&s, p);
    return true;
}

/* ------------------------------------------------------------ SVG */

typedef struct { buf_t *b; } svg_t;

static void svg_color(buf_t *b, const char *atrib, uint32_t c)
{
    if ((c >> 24) == 0) { buf_f(b, " %s=\"none\"", atrib); return; }
    buf_f(b, " %s=\"#%06x\"", atrib, c & 0xFFFFFF);
    if ((c >> 24) < 255) buf_f(b, " %s-opacity=\"%d.%02d\"", atrib, (int)(c >> 24) * 100 / 255 / 100, (int)(c >> 24) * 100 / 255 % 100);
}
static void svg_trazo(buf_t *b, uint32_t lin, int g)
{
    svg_color(b, "stroke", lin);
    if ((lin >> 24)) buf_f(b, " stroke-width=\"%d\" stroke-linejoin=\"round\" stroke-linecap=\"round\"", g);
}
static void sv_poli(void *ctx, const int *xy, int n, uint32_t rel, uint32_t lin, int g)
{
    buf_t *b = ((svg_t *)ctx)->b;
    buf_s(b, "    <polygon points=\"");
    for (int i = 0; i < n; i++) buf_f(b, "%s%d,%d", i ? " " : "", xy[2 * i], xy[2 * i + 1]);
    buf_s(b, "\"");
    svg_color(b, "fill", rel); svg_trazo(b, lin, g);
    buf_s(b, "/>\n");
}
static void sv_abierta(void *ctx, const int *xy, int n, uint32_t lin, int g)
{
    buf_t *b = ((svg_t *)ctx)->b;
    buf_s(b, "    <polyline points=\"");
    for (int i = 0; i < n; i++) buf_f(b, "%s%d,%d", i ? " " : "", xy[2 * i], xy[2 * i + 1]);
    buf_s(b, "\" fill=\"none\"");
    svg_trazo(b, lin, g);
    buf_s(b, "/>\n");
}
static void sv_elipse(void *ctx, int cx, int cy, int rx, int ry, uint32_t rel, uint32_t lin, int g)
{
    buf_t *b = ((svg_t *)ctx)->b;
    buf_f(b, "    <ellipse cx=\"%d\" cy=\"%d\" rx=\"%d\" ry=\"%d\"", cx, cy, rx, ry);
    svg_color(b, "fill", rel); svg_trazo(b, lin, g);
    buf_s(b, "/>\n");
}
static void sv_texto(void *ctx, int x, int y, int l, int a, const char *t, uint32_t col, bool izq)
{
    buf_t *b = ((svg_t *)ctx)->b;
    char lin[LIN_MAX][LIN_LONG];
    int nl = texto_lineas(t, l / 8, lin);
    int y0 = y + (a - nl * 16) / 2 + 12;
    for (int k = 0; k < nl; k++) {
        if (!lin[k][0]) continue;
        buf_f(b, "    <text x=\"%d\" y=\"%d\" text-anchor=\"%s\" font-family=\"DejaVu Sans, Arial, sans-serif\" font-size=\"13\" fill=\"#%06x\">",
              izq ? x : x + l / 2, y0 + k * 16, izq ? "start" : "middle", col & 0xFFFFFF);
        buf_cadena(b, lin[k], false);
        buf_s(b, "</text>\n");
    }
}

/* El SVG de una página (con un margen de 16). */
static void pag_a_svg(const pag_t *p, buf_t *b)
{
    int x0, y0, x1, y1;
    if (!pag_limites(p, &x0, &y0, &x1, &y1)) { x0 = y0 = 0; x1 = 200; y1 = 120; }
    int m = 16, l = x1 - x0 + 2 * m, a = y1 - y0 + 2 * m;
    buf_s(b, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    buf_f(b, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\">\n", l, a, l, a);
    buf_s(b, "  <title>"); buf_cadena(b, p->nombre, false); buf_s(b, "</title>\n");
    buf_s(b, "  <rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n");
    buf_f(b, "  <g transform=\"translate(%d,%d)\">\n", m - x0, m - y0);
    svg_t sv = { b };
    sink_t s = { &sv, sv_poli, sv_abierta, sv_elipse, sv_texto };
    dib_pagina(&s, p);
    buf_s(b, "  </g>\n</svg>\n");
}

#endif
