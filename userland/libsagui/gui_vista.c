// SPDX-License-Identifier: GPL-3.0-only
/* gui_vista.c - lienzo 2D genérico con zoom y desplazamiento (D454), para editores.
 *
 * No guarda objetos: la aplicación dibuja su contenido en coordenadas del MUNDO con edvista_ret / edvista_linha /
 * edvista_texto dentro del callback desenhar, y la vista pone la rejilla, el zoom (rueda, centrado en el ratón), el
 * desplazamiento (botón central, o Espacio y arrastrar con el izquierdo), la selección por rectángulo y las asas de
 * un rectángulo seleccionado (mover, escalar con 8 asas, girar con el asa de arriba). Todo en enteros: el zoom va en
 * milésimas y los giros en grados con una tabla de senos. Límite: coordenadas del mundo de hasta ±60000 con zoom máximo.
 * Para el rectángulo girado las cuentas se hacen en medias unidades (el doble de cada coordenada) para no perder el centro. */
#include "gui.h"

static const short seno_tab[91] = {
    0, 17, 35, 52, 70, 87, 105, 122, 139, 156, 174, 191, 208, 225, 242, 259, 276, 292, 309, 326, 342, 358, 375, 391, 407, 423,
    438, 454, 469, 485, 500, 515, 530, 545, 559, 574, 588, 602, 616, 629, 643, 656, 669, 682, 695, 707, 719, 731, 743, 755, 766,
    777, 788, 799, 809, 819, 829, 839, 848, 857, 866, 875, 883, 891, 899, 906, 914, 921, 927, 934, 940, 946, 951, 956, 961, 966,
    970, 974, 978, 982, 985, 988, 990, 993, 995, 996, 998, 999, 999, 1000, 1000 };

int edvista_seno(int g)
{
    g %= 360; if (g < 0) g += 360;
    if (g <= 90) return seno_tab[g];
    if (g <= 180) return seno_tab[180 - g];
    if (g <= 270) return -seno_tab[g - 180];
    return -seno_tab[360 - g];
}
static int coseno(int g) { return edvista_seno(g + 90); }

static int fdiv(int a, int b)   /* división hacia abajo (b > 0) */
{
    int q = a / b;
    if ((a % b) < 0) q--;
    return q;
}

static bool dentro(int px, int py, int x, int y, int l, int a)
{
    return px >= x && py >= y && px < x + l && py < y + a;
}

/* ------------------------------------------------------------ conversiones */

void edvista_para_tela(const edvista_t *v, int wx, int wy, int *sx, int *sy)
{
    if (sx) *sx = v->x + v->ox + fdiv(wx * v->zoom, 1000);
    if (sy) *sy = v->y + v->oy + fdiv(wy * v->zoom, 1000);
}

void edvista_para_mundo(const edvista_t *v, int sx, int sy, int *wx, int *wy)
{
    if (wx) *wx = fdiv((sx - v->x - v->ox) * 1000, v->zoom);
    if (wy) *wy = fdiv((sy - v->y - v->oy) * 1000, v->zoom);
}

/* Mundo en medias unidades (2 * coordenada) -> pantalla. */
static int tela2x(const edvista_t *v, int w2) { return v->x + v->ox + fdiv(w2 * v->zoom, 2000); }
static int tela2y(const edvista_t *v, int w2) { return v->y + v->oy + fdiv(w2 * v->zoom, 2000); }

int edvista_iman(const edvista_t *v, int w)
{
    if (!v->iman || v->grade <= 0) return w;
    int g = v->grade;
    return fdiv(w + g / 2, g) * g;
}

void edvista_iniciar(edvista_t *v, int x, int y, int l, int a)
{
    memset(v, 0, sizeof *v);
    v->x = x; v->y = y; v->l = l; v->a = a;
    v->zoom = 1000;
    v->ox = l / 2; v->oy = a / 2;
    v->grade = 16;
}

void edvista_mover(edvista_t *v, int x, int y, int l, int a)
{
    v->x = x; v->y = y; v->l = l; v->a = a;
}

void edvista_zoom(edvista_t *v, int zoom, int cx, int cy)
{
    if (zoom < EDVISTA_ZOOM_MIN) zoom = EDVISTA_ZOOM_MIN;
    if (zoom > EDVISTA_ZOOM_MAX) zoom = EDVISTA_ZOOM_MAX;
    if (zoom == v->zoom) return;
    /* el punto del mundo bajo (cx, cy) se queda quieto; se lleva con 4 bits más de precisión */
    int px = cx - v->x - v->ox, py = cy - v->y - v->oy;
    int w16x = fdiv(px * 16000, v->zoom), w16y = fdiv(py * 16000, v->zoom);
    v->zoom = zoom;
    v->ox = cx - v->x - fdiv(w16x * zoom, 16000);
    v->oy = cy - v->y - fdiv(w16y * zoom, 16000);
}

void edvista_centrar(edvista_t *v, int wx, int wy)
{
    v->ox = v->l / 2 - fdiv(wx * v->zoom, 1000);
    v->oy = v->a / 2 - fdiv(wy * v->zoom, 1000);
}

void edvista_selecionar(edvista_t *v, int x, int y, int l, int a, int rot)
{
    v->tem_sel = l > 0 && a > 0;
    v->sx = x; v->sy = y; v->sl = l; v->sa = a;
    rot %= 360; if (rot < 0) rot += 360;
    v->srot = rot;
}

/* ------------------------------------------------------------ dibujo recortado */

static void ret_clip(jan_t *j, const edvista_t *v, int x, int y, int l, int a, uint32_t cor)
{
    int x1 = x + l, y1 = y + a;
    if (x < v->x) x = v->x;
    if (y < v->y) y = v->y;
    if (x1 > v->x + v->l) x1 = v->x + v->l;
    if (y1 > v->y + v->a) y1 = v->y + v->a;
    if (x1 > x && y1 > y) g_retangulo(j, x, y, x1 - x, y1 - y, cor);
}

static void pixel_clip(jan_t *j, const edvista_t *v, int x, int y, uint32_t cor)
{
    if (dentro(x, y, v->x, v->y, v->l, v->a) && x < j->larg && y < j->alt) j->px[y * j->larg + x] = cor;
}

static void linha_tela(jan_t *j, const edvista_t *v, int x0, int y0, int x1, int y1, uint32_t cor)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (int n = 0; n < 5000; n++) {
        pixel_clip(j, v, x0, y0, cor);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void edvista_ret(jan_t *j, const edvista_t *v, int wx, int wy, int wl, int wa, uint32_t cor)
{
    int x0, y0, x1, y1;
    edvista_para_tela(v, wx, wy, &x0, &y0);
    edvista_para_tela(v, wx + wl, wy + wa, &x1, &y1);
    if (x1 <= x0) x1 = x0 + 1;
    if (y1 <= y0) y1 = y0 + 1;
    ret_clip(j, v, x0, y0, x1 - x0, y1 - y0, cor);
}

void edvista_moldura(jan_t *j, const edvista_t *v, int wx, int wy, int wl, int wa, uint32_t cor)
{
    int x0, y0, x1, y1;
    edvista_para_tela(v, wx, wy, &x0, &y0);
    edvista_para_tela(v, wx + wl, wy + wa, &x1, &y1);
    if (x1 <= x0) x1 = x0 + 1;
    if (y1 <= y0) y1 = y0 + 1;
    ret_clip(j, v, x0, y0, x1 - x0, 1, cor);
    ret_clip(j, v, x0, y1 - 1, x1 - x0, 1, cor);
    ret_clip(j, v, x0, y0, 1, y1 - y0, cor);
    ret_clip(j, v, x1 - 1, y0, 1, y1 - y0, cor);
}

void edvista_linha(jan_t *j, const edvista_t *v, int x0, int y0, int x1, int y1, uint32_t cor)
{
    int a, b, c, d;
    edvista_para_tela(v, x0, y0, &a, &b);
    edvista_para_tela(v, x1, y1, &c, &d);
    linha_tela(j, v, a, b, c, d, cor);
}

void edvista_texto(jan_t *j, const edvista_t *v, int wx, int wy, const char *t, uint32_t cor)
{
    int sx, sy;
    edvista_para_tela(v, wx, wy, &sx, &sy);
    for (; *t; t++, sx += LETRA_L) {
        if (sx < v->x || sx + LETRA_L > v->x + v->l || sy < v->y || sy + LETRA_A > v->y + v->a) continue;
        g_texto_n(j, sx, sy, t, 1, cor);
    }
}

/* ------------------------------------------------------------ rectángulo seleccionado con asas */

/* Posición en medias unidades del punto local (u, w) (también en medias unidades) del rectángulo de partida. */
static void local_a_mundo2(int cx2, int cy2, int rot, int u, int w, int *x2, int *y2)
{
    int s = edvista_seno(rot), c = coseno(rot);
    *x2 = cx2 + (u * c - w * s) / 1000;
    *y2 = cy2 + (u * s + w * c) / 1000;
}

static void mundo2_a_local(int cx2, int cy2, int rot, int x2, int y2, int *u, int *w)
{
    int s = edvista_seno(rot), c = coseno(rot), dx = x2 - cx2, dy = y2 - cy2;
    *u = (dx * c + dy * s) / 1000;
    *w = (-dx * s + dy * c) / 1000;
}

/* Las 8 asas: orden NO, N, NE, E, SE, S, SO, O (hx, hy en -1..1) y el asa de giro (índice 8). */
static const signed char asa_h[8][2] = { {-1,-1}, {0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0} };

static void asa_pos(const edvista_t *v, int k, int *px, int *py)
{
    int cx2 = 2 * v->sx + v->sl, cy2 = 2 * v->sy + v->sa;
    int x2, y2;
    if (k < 8) local_a_mundo2(cx2, cy2, v->srot, asa_h[k][0] * v->sl, asa_h[k][1] * v->sa, &x2, &y2);
    else local_a_mundo2(cx2, cy2, v->srot, 0, -v->sa, &x2, &y2);
    *px = tela2x(v, x2);
    *py = tela2y(v, y2);
    if (k == 8) {   /* el asa de giro está 22 px por encima del borde, en la dirección «arriba» del rectángulo */
        *px += 22 * edvista_seno(v->srot) / 1000;
        *py -= 22 * coseno(v->srot) / 1000;
    }
}

static int asa_em(const edvista_t *v, int mx, int my)
{
    if (!v->tem_sel) return -1;
    for (int k = 8; k >= 0; k--) {
        int px, py;
        asa_pos(v, k, &px, &py);
        int r = k == 8 ? 6 : 5;
        if (mx >= px - r && mx <= px + r && my >= py - r && my <= py + r) return k;
    }
    return -1;
}

static bool dentro_sel(const edvista_t *v, int mx, int my)
{
    if (!v->tem_sel) return false;
    int wx, wy, u, w;
    edvista_para_mundo(v, mx, my, &wx, &wy);
    mundo2_a_local(2 * v->sx + v->sl, 2 * v->sy + v->sa, v->srot, 2 * wx, 2 * wy, &u, &w);
    return u >= -v->sl && u <= v->sl && w >= -v->sa && w <= v->sa;
}

static void desenhar_sel(jan_t *j, const edvista_t *v)
{
    if (!v->tem_sel) return;
    int px[8], py[8];
    for (int k = 0; k < 8; k++) asa_pos(v, k, &px[k], &py[k]);
    uint32_t cor = mod_tema.acento;
    for (int k = 0; k < 8; k += 2) {   /* esquinas: 0, 2, 4, 6 */
        int n = (k + 2) % 8;
        linha_tela(j, v, px[k], py[k], px[n], py[n], cor);
    }
    int gx, gy;
    asa_pos(v, 8, &gx, &gy);
    linha_tela(j, v, px[1], py[1], gx, gy, cor);
    for (int k = 0; k < 8; k++) {
        ret_clip(j, v, px[k] - 3, py[k] - 3, 7, 7, 0xFFFFFF);
        ret_clip(j, v, px[k] - 3, py[k] - 3, 7, 1, cor); ret_clip(j, v, px[k] - 3, py[k] + 3, 7, 1, cor);
        ret_clip(j, v, px[k] - 3, py[k] - 3, 1, 7, cor); ret_clip(j, v, px[k] + 3, py[k] - 3, 1, 7, cor);
    }
    for (int dy = -4; dy <= 4; dy++)   /* asa de giro: círculo */
        for (int dx = -4; dx <= 4; dx++) {
            int d = dx * dx + dy * dy;
            if (d <= 20) pixel_clip(j, v, gx + dx, gy + dy, d >= 12 ? cor : 0xFFFFFF);
        }
}

/* ------------------------------------------------------------ dibujo */

static void rejilla(jan_t *j, const edvista_t *v)
{
    if (v->grade <= 0) return;
    int paso = v->grade;
    while (paso * v->zoom / 1000 < 8 && paso < 1000000) paso *= 2;
    int w0, w1, h0, h1;
    edvista_para_mundo(v, v->x, v->y, &w0, &h0);
    edvista_para_mundo(v, v->x + v->l, v->y + v->a, &w1, &h1);
    uint32_t cl = mod_mezclar(mod_tema.pista, mod_tema.superficie, 120);
    uint32_t cg = mod_mezclar(mod_tema.pista, mod_tema.borde_forte, 80);
    for (int w = fdiv(w0, paso) * paso; w <= w1; w += paso) {
        int sx;
        edvista_para_tela(v, w, 0, &sx, NULL);
        ret_clip(j, v, sx, v->y, 1, v->a, (fdiv(w, paso) % 4 == 0) ? cg : cl);
    }
    for (int w = fdiv(h0, paso) * paso; w <= h1; w += paso) {
        int sy;
        edvista_para_tela(v, 0, w, NULL, &sy);
        ret_clip(j, v, v->x, sy, v->l, 1, (fdiv(w, paso) % 4 == 0) ? cg : cl);
    }
}

void edvista_desenhar(jan_t *j, edvista_t *v)
{
    ret_clip(j, v, v->x, v->y, v->l, v->a, mod_mezclar(mod_tema.pista, mod_tema.superficie, 180));
    rejilla(j, v);
    int ex, ey;
    edvista_para_tela(v, 0, 0, &ex, &ey);   /* ejes del mundo */
    ret_clip(j, v, ex, v->y, 1, v->a, mod_tema.foco);
    ret_clip(j, v, v->x, ey, v->l, 1, mod_tema.aviso);
    if (v->desenhar) v->desenhar(v->ctx, j, v);
    desenhar_sel(j, v);
    if (v->modo == 2) {   /* rectángulo de selección */
        int x0 = v->rx0 < v->rx1 ? v->rx0 : v->rx1, x1 = v->rx0 < v->rx1 ? v->rx1 : v->rx0;
        int y0 = v->ry0 < v->ry1 ? v->ry0 : v->ry1, y1 = v->ry0 < v->ry1 ? v->ry1 : v->ry0;
        int a, b, c, d;
        edvista_para_tela(v, x0, y0, &a, &b);
        edvista_para_tela(v, x1, y1, &c, &d);
        for (int x = a; x <= c; x += 4) { pixel_clip(j, v, x, b, mod_tema.acento); pixel_clip(j, v, x + 1, b, mod_tema.acento); pixel_clip(j, v, x, d, mod_tema.acento); pixel_clip(j, v, x + 1, d, mod_tema.acento); }
        for (int y = b; y <= d; y += 4) { pixel_clip(j, v, a, y, mod_tema.acento); pixel_clip(j, v, a, y + 1, mod_tema.acento); pixel_clip(j, v, c, y, mod_tema.acento); pixel_clip(j, v, c, y + 1, mod_tema.acento); }
    }
    char t[16];
    formatar_texto(t, sizeof t, "%d%%", (v->zoom + 5) / 10);
    int tl = (int)tam_texto(t) * LETRA_L + 8;
    g_caixa_red(j, v->x + v->l - tl - 6, v->y + 6, tl, 20, 4, mod_tema.superficie, mod_tema.borde);
    g_texto(j, v->x + v->l - tl - 2, v->y + 8, t, mod_tema.texto_fraco);
    g_moldura(j, v->x, v->y, v->l, v->a, v->hover || v->modo ? mod_tema.foco : mod_tema.borde);
    jan_mostrar_area(j, v->x, v->y, v->l, v->a);
    GR_REG(j, "vista", v, v->x, v->y, v->l, v->a, "", 0, 0);
}

/* ------------------------------------------------------------ gestos */

static int angulo_de(int dx, int dy)   /* grados (0 = arriba, sentido horario) del vector (dx, dy) de pantalla */
{
    int mejor = 0, mx = -2000000000;
    for (int g = 0; g < 360; g++) {
        int d = dx * edvista_seno(g) - dy * coseno(g);
        if (d > mx) { mx = d; mejor = g; }
    }
    return mejor;
}

static void aviso(edvista_t *v, int ev, int a, int b, int c, int d, bool fim)
{
    if (v->aviso) v->aviso(v->ctx, ev, a, b, c, d, fim);
}

static void escalar(edvista_t *v, int mx, int my, bool shift)
{
    (void)shift;
    int k = v->asa;
    int hx = asa_h[k][0], hy = asa_h[k][1];
    int cx2 = 2 * v->s0x + v->s0l, cy2 = 2 * v->s0y + v->s0a;
    int wx, wy, u, w;
    edvista_para_mundo(v, mx, my, &wx, &wy);
    mundo2_a_local(cx2, cy2, v->s0rot, 2 * wx, 2 * wy, &u, &w);
    if (v->iman && v->srot == 0) {   /* sin giro, el borde que se mueve salta a la rejilla */
        if (hx) u = 2 * edvista_iman(v, (cx2 + u) / 2) - cx2;
        if (hy) w = 2 * edvista_iman(v, (cy2 + w) / 2) - cy2;
    }
    int nl = v->s0l, na = v->s0a, uc = 0, wc = 0;
    if (hx) {
        int uf = -hx * v->s0l;
        nl = (u > uf ? u - uf : uf - u) / 2;
        uc = (u + uf) / 2;
    }
    if (hy) {
        int wf = -hy * v->s0a;
        na = (w > wf ? w - wf : wf - w) / 2;
        wc = (w + wf) / 2;
    }
    if (nl < 1) nl = 1;
    if (na < 1) na = 1;
    int ncx2, ncy2;
    local_a_mundo2(cx2, cy2, v->s0rot, uc, wc, &ncx2, &ncy2);
    v->sl = nl; v->sa = na;
    v->sx = fdiv(ncx2 - nl, 2);
    v->sy = fdiv(ncy2 - na, 2);
}

int edvista_tratar(jan_t *j, edvista_t *v, const evento_t *e)
{
    int r = EDVISTA_NADA;
    if (e->tipo == EV_RODA) {
        if (!dentro(e->x, e->y, v->x, v->y, v->l, v->a)) return r;
        int n = (int)e->cliques, z = v->zoom;
        for (; n < 0; n++) { int nz = z * 5 / 4; z = nz == z ? z + 1 : nz; }
        for (; n > 0; n--) { int nz = z * 4 / 5; z = nz == z ? z - 1 : nz; }
        int antes = v->zoom;
        edvista_zoom(v, z, e->x, e->y);
        if (v->zoom != antes) {
            aviso(v, EDVISTA_E_ZOOM, v->zoom, 0, 0, 0, true);
            edvista_desenhar(j, v);
            r = EDVISTA_MUDOU;
        }
        return r;
    }
    if (e->tipo == EV_TECLA) {
        if (e->tecla == ' ' && v->hover) v->mao = true;
        return r;
    }
    if (e->tipo == EV_CLIQUE && dentro(e->x, e->y, v->x, v->y, v->l, v->a)) {
        v->sobre_x = e->x; v->sobre_y = e->y;
        v->ax = e->x; v->ay = e->y;
        if ((e->botoes & 4) || ((e->botoes & 1) && v->mao)) {
            v->modo = 1; v->ax0 = v->ox; v->ay0 = v->oy;
            return r;
        }
        if (!(e->botoes & 1)) return r;
        v->s0x = v->sx; v->s0y = v->sy; v->s0l = v->sl; v->s0a = v->sa; v->s0rot = v->srot;
        int k = asa_em(v, e->x, e->y);
        if (k == 8) {
            v->modo = 5; v->asa = 8;
            int gx, gy;
            edvista_para_tela(v, v->sx, v->sy, &gx, &gy);
            int cx = tela2x(v, 2 * v->sx + v->sl), cy = tela2y(v, 2 * v->sy + v->sa);
            v->ax0 = angulo_de(e->x - cx, e->y - cy);
            return r;
        }
        if (k >= 0) { v->modo = 4; v->asa = k; return r; }
        if (dentro_sel(v, e->x, e->y)) { v->modo = 3; return r; }
        int wx, wy;
        edvista_para_mundo(v, e->x, e->y, &wx, &wy);
        int id = v->acertar ? v->acertar(v->ctx, wx, wy) : -1;
        aviso(v, EDVISTA_E_CLIQUE, id, wx, wy, e->modificadores, false);
        if (id < 0) { v->modo = 2; v->rx0 = v->rx1 = wx; v->ry0 = v->ry1 = wy; }
        else if (v->tem_sel && dentro_sel(v, e->x, e->y)) { v->s0x = v->sx; v->s0y = v->sy; v->s0l = v->sl; v->s0a = v->sa; v->s0rot = v->srot; v->modo = 3; }
        edvista_desenhar(j, v);
        return EDVISTA_MUDOU;
    }
    if (e->tipo == EV_MOVER) {
        bool dentro_v = dentro(e->x, e->y, v->x, v->y, v->l, v->a);
        if (v->modo == 0) {
            if (dentro_v != v->hover) { v->hover = dentro_v; edvista_desenhar(j, v); }
            if (dentro_v) edvista_para_mundo(v, e->x, e->y, &v->sobre_x, &v->sobre_y);
            return r;
        }
        if (v->modo != 1 && !(e->botoes & 1)) return r;
        int wx, wy;
        edvista_para_mundo(v, e->x, e->y, &wx, &wy);
        v->sobre_x = wx; v->sobre_y = wy;
        switch (v->modo) {
        case 1:
            v->ox = v->ax0 + (e->x - v->ax);
            v->oy = v->ay0 + (e->y - v->ay);
            aviso(v, EDVISTA_E_DESLOCAR, v->ox, v->oy, 0, 0, false);
            break;
        case 2:
            v->rx1 = wx; v->ry1 = wy;
            break;
        case 3: {
            int dx = fdiv((e->x - v->ax) * 1000, v->zoom), dy = fdiv((e->y - v->ay) * 1000, v->zoom);
            v->sx = edvista_iman(v, v->s0x + dx);
            v->sy = edvista_iman(v, v->s0y + dy);
            aviso(v, EDVISTA_E_MOVER, v->sx, v->sy, v->sl, v->sa, false);
            break;
        }
        case 4:
            escalar(v, e->x, e->y, (e->modificadores & 1) != 0);
            aviso(v, EDVISTA_E_ESCALAR, v->sx, v->sy, v->sl, v->sa, false);
            break;
        case 5: {
            int cx = tela2x(v, 2 * v->s0x + v->s0l), cy = tela2y(v, 2 * v->s0y + v->s0a);
            int g = angulo_de(e->x - cx, e->y - cy) - v->ax0 + v->s0rot;
            if (e->modificadores & 1) g = (g + 7) / 15 * 15;
            g %= 360; if (g < 0) g += 360;
            v->srot = g;
            aviso(v, EDVISTA_E_GIRAR, g, v->sx, v->sy, 0, false);
            break;
        }
        }
        edvista_desenhar(j, v);
        return EDVISTA_MUDOU;
    }
    if (e->tipo == EV_SOLTOU && v->modo) {
        int m = v->modo;
        v->modo = 0;
        v->mao = false;
        if (m == 2) {
            int x0 = v->rx0 < v->rx1 ? v->rx0 : v->rx1, x1 = v->rx0 < v->rx1 ? v->rx1 : v->rx0;
            int y0 = v->ry0 < v->ry1 ? v->ry0 : v->ry1, y1 = v->ry0 < v->ry1 ? v->ry1 : v->ry0;
            aviso(v, EDVISTA_E_RET, x0, y0, x1, y1, true);
        } else if (m == 3) aviso(v, EDVISTA_E_MOVER, v->sx, v->sy, v->sl, v->sa, true);
        else if (m == 4) aviso(v, EDVISTA_E_ESCALAR, v->sx, v->sy, v->sl, v->sa, true);
        else if (m == 5) aviso(v, EDVISTA_E_GIRAR, v->srot, v->sx, v->sy, 0, true);
        else if (m == 1) aviso(v, EDVISTA_E_DESLOCAR, v->ox, v->oy, 0, 0, true);
        edvista_desenhar(j, v);
        return EDVISTA_MUDOU;
    }
    return r;
}
