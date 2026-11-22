// SPDX-License-Identifier: GPL-3.0-only
/* gui_paleta.c - paleta de casillas (D454): elegir celdas de una imagen dividida en una cuadrícula, con selección
 * rectangular múltiple, para pintar mapas de casillas (tilemaps). La imagen se ve ampliada por «escala» (1 a 4); si
 * no hay imagen, la aplicación dibuja cada celda con desenhar_celda. Rueda: desplaza; Mayús + clic: amplía la selección;
 * con el foco, las flechas mueven la selección de una casilla. */
#include "gui.h"

static bool dentro(int px, int py, int x, int y, int l, int a)
{
    return px >= x && py >= y && px < x + l && py < y + a;
}

void edpaleta_iniciar(edpaleta_t *p, int x, int y, int l, int a, const imagem_t *img, int cel_l, int cel_a)
{
    memset(p, 0, sizeof *p);
    p->x = x; p->y = y; p->l = l; p->a = a;
    p->img = img;
    p->cel_l = cel_l > 0 ? cel_l : 16;
    p->cel_a = cel_a > 0 ? cel_a : 16;
    p->escala = 2;
    if (img) { p->cols = img->larg / p->cel_l; p->filas = img->alt / p->cel_a; }
}

void edpaleta_dimensoes(edpaleta_t *p, int cols, int filas) { p->cols = cols; p->filas = filas; }

int edpaleta_indice(const edpaleta_t *p, int col, int fila) { return fila * p->cols + col; }

static int lim(int v, int a, int b) { return v < a ? a : v > b ? b : v; }

void edpaleta_selecao(const edpaleta_t *p, int *c0, int *f0, int *c1, int *f1)
{
    if (c0) *c0 = p->c0 < p->c1 ? p->c0 : p->c1;
    if (c1) *c1 = p->c0 < p->c1 ? p->c1 : p->c0;
    if (f0) *f0 = p->f0 < p->f1 ? p->f0 : p->f1;
    if (f1) *f1 = p->f0 < p->f1 ? p->f1 : p->f0;
}

void edpaleta_definir_selecao(edpaleta_t *p, int c0, int f0, int c1, int f1)
{
    p->c0 = lim(c0, 0, p->cols > 0 ? p->cols - 1 : 0); p->c1 = lim(c1, 0, p->cols > 0 ? p->cols - 1 : 0);
    p->f0 = lim(f0, 0, p->filas > 0 ? p->filas - 1 : 0); p->f1 = lim(f1, 0, p->filas > 0 ? p->filas - 1 : 0);
}

int edpaleta_selecao_indices(const edpaleta_t *p, int *saida, int max)
{
    int c0, f0, c1, f1, n = 0;
    edpaleta_selecao(p, &c0, &f0, &c1, &f1);
    for (int f = f0; f <= f1; f++)
        for (int c = c0; c <= c1; c++)
            if (n < max) saida[n++] = edpaleta_indice(p, c, f);
    return n;
}

static int cel_px_l(const edpaleta_t *p) { return p->cel_l * p->escala; }
static int cel_px_a(const edpaleta_t *p) { return p->cel_a * p->escala; }

static void limitar_desloc(edpaleta_t *p)
{
    int maxx = p->cols * cel_px_l(p) - p->l, maxy = p->filas * cel_px_a(p) - p->a;
    p->cx = lim(p->cx, 0, maxx < 0 ? 0 : maxx);
    p->cy = lim(p->cy, 0, maxy < 0 ? 0 : maxy);
}

void edpaleta_desenhar(jan_t *j, edpaleta_t *p)
{
    limitar_desloc(p);
    g_retangulo(j, p->x, p->y, p->l, p->a, mod_tema.pista);
    int cl = cel_px_l(p), ca = cel_px_a(p);
    int c0, f0, c1, f1;
    edpaleta_selecao(p, &c0, &f0, &c1, &f1);
    for (int f = p->cy / ca; f < p->filas && f * ca - p->cy < p->a; f++)
        for (int c = p->cx / cl; c < p->cols && c * cl - p->cx < p->l; c++) {
            int x = p->x + c * cl - p->cx, y = p->y + f * ca - p->cy;
            /* parte visible de la celda */
            int vx0 = x < p->x ? p->x : x, vy0 = y < p->y ? p->y : y;
            int vx1 = x + cl > p->x + p->l ? p->x + p->l : x + cl, vy1 = y + ca > p->y + p->a ? p->y + p->a : y + ca;
            if (vx1 <= vx0 || vy1 <= vy0) continue;
            if (p->img) {
                for (int yy = vy0; yy < vy1; yy++) {
                    int sy = f * p->cel_a + (yy - y) / p->escala;
                    if (yy < 0 || yy >= j->alt || sy >= p->img->alt) continue;
                    for (int xx = vx0; xx < vx1; xx++) {
                        int sx = c * p->cel_l + (xx - x) / p->escala;
                        if (xx < 0 || xx >= j->larg || sx >= p->img->larg) continue;
                        j->px[yy * j->larg + xx] = p->img->px[sy * p->img->larg + sx] & 0xFFFFFF;
                    }
                }
            } else if (p->desenhar_celda) {
                p->desenhar_celda(p->ctx, j, f * p->cols + c, x, y, cl, ca);
            }
            bool sel = c >= c0 && c <= c1 && f >= f0 && f <= f1;
            if (sel) {   /* tinte de acento sobre lo elegido */
                for (int yy = vy0; yy < vy1; yy++)
                    for (int xx = vx0; xx < vx1; xx++)
                        if (xx >= 0 && xx < j->larg && yy >= 0 && yy < j->alt)
                            j->px[yy * j->larg + xx] = mod_mezclar(j->px[yy * j->larg + xx], mod_tema.acento, 70);
            }
            /* rejilla fina entre celdas */
            if (x + cl - 1 >= p->x && x + cl - 1 < p->x + p->l) g_retangulo(j, x + cl - 1, vy0, 1, vy1 - vy0, mod_mezclar(mod_tema.borde_forte, 0, 20));
            if (y + ca - 1 >= p->y && y + ca - 1 < p->y + p->a) g_retangulo(j, vx0, y + ca - 1, vx1 - vx0, 1, mod_mezclar(mod_tema.borde_forte, 0, 20));
        }
    /* marco de la selección */
    int sx = p->x + c0 * cl - p->cx, sy = p->y + f0 * ca - p->cy, sl = (c1 - c0 + 1) * cl, sa = (f1 - f0 + 1) * ca;
    if (p->cols > 0 && p->filas > 0) {
        for (int k = 0; k < 2; k++) {
            int x0 = sx + k, y0 = sy + k, w = sl - 2 * k, h = sa - 2 * k;
            int xs[4] = { x0, x0, x0, x0 + w - 1 }, ys[4] = { y0, y0 + h - 1, y0, y0 };
            int ls[4] = { w, w, 1, 1 }, as[4] = { 1, 1, h, h };
            for (int q = 0; q < 4; q++) {
                int rx = xs[q], ry = ys[q], rl = ls[q], ra = as[q];
                if (rx < p->x) { rl -= p->x - rx; rx = p->x; }
                if (ry < p->y) { ra -= p->y - ry; ry = p->y; }
                if (rx + rl > p->x + p->l) rl = p->x + p->l - rx;
                if (ry + ra > p->y + p->a) ra = p->y + p->a - ry;
                if (rl > 0 && ra > 0) g_retangulo(j, rx, ry, rl, ra, k ? 0xFFFFFF : mod_tema.acento);
            }
        }
    }
    g_moldura(j, p->x, p->y, p->l, p->a, p->foco ? mod_tema.foco : mod_tema.borde);
    jan_mostrar_area(j, p->x, p->y, p->l, p->a);
    GR_REG(j, "paleta", p, p->x, p->y, p->l, p->a, "", 0, 0);
}

static void celda_em(const edpaleta_t *p, int x, int y, int *c, int *f)
{
    *c = lim((x - p->x + p->cx) / cel_px_l(p), 0, p->cols > 0 ? p->cols - 1 : 0);
    *f = lim((y - p->y + p->cy) / cel_px_a(p), 0, p->filas > 0 ? p->filas - 1 : 0);
}

int edpaleta_tratar(jan_t *j, edpaleta_t *p, const evento_t *e)
{
    if (p->cols <= 0 || p->filas <= 0) return EDPALETA_NADA;
    if (e->tipo == EV_RODA || e->tipo == EV_RODA_H) {
        if (!dentro(e->x, e->y, p->x, p->y, p->l, p->a)) return EDPALETA_NADA;
        int ax = p->cx, ay = p->cy, d = (int)e->cliques * cel_px_a(p);
        if (e->tipo == EV_RODA_H || (e->modificadores & 1)) p->cx += d; else p->cy += d;
        limitar_desloc(p);
        if (ax != p->cx || ay != p->cy) edpaleta_desenhar(j, p);
        return EDPALETA_NADA;
    }
    if (e->tipo == EV_CLIQUE && (e->botoes & 1)) {
        p->foco = dentro(e->x, e->y, p->x, p->y, p->l, p->a);
        if (!p->foco) { edpaleta_desenhar(j, p); return EDPALETA_NADA; }
        int c, f;
        celda_em(p, e->x, e->y, &c, &f);
        if (e->modificadores & 1) { p->c1 = c; p->f1 = f; }
        else { p->c0 = p->c1 = c; p->f0 = p->f1 = f; }
        p->arrastando = true;
        edpaleta_desenhar(j, p);
        return EDPALETA_MUDOU;
    }
    if (e->tipo == EV_MOVER && p->arrastando && (e->botoes & 1)) {
        int c, f;
        celda_em(p, e->x, e->y, &c, &f);
        if (c == p->c1 && f == p->f1) return EDPALETA_NADA;
        p->c1 = c; p->f1 = f;
        edpaleta_desenhar(j, p);
        return EDPALETA_MUDOU;
    }
    if (e->tipo == EV_SOLTOU && p->arrastando) {
        p->arrastando = false;
        return EDPALETA_ESCOLHEU;
    }
    if (e->tipo == EV_TECLA && p->foco) {
        int dc = 0, df = 0;
        if (e->tecla == TECLA_ESQUERDA) dc = -1;
        else if (e->tecla == TECLA_DIREITA) dc = 1;
        else if (e->tecla == TECLA_CIMA) df = -1;
        else if (e->tecla == TECLA_BAIXO) df = 1;
        else return EDPALETA_NADA;
        int c = lim(p->c1 + dc, 0, p->cols - 1), f = lim(p->f1 + df, 0, p->filas - 1);
        if (e->modificadores & 1) { p->c1 = c; p->f1 = f; }
        else { p->c0 = p->c1 = c; p->f0 = p->f1 = f; }
        /* mantener la casilla a la vista */
        int cl = cel_px_l(p), ca = cel_px_a(p);
        if (c * cl < p->cx) p->cx = c * cl;
        if ((c + 1) * cl > p->cx + p->l) p->cx = (c + 1) * cl - p->l;
        if (f * ca < p->cy) p->cy = f * ca;
        if ((f + 1) * ca > p->cy + p->a) p->cy = (f + 1) * ca - p->a;
        limitar_desloc(p);
        edpaleta_desenhar(j, p);
        return EDPALETA_ESCOLHEU;
    }
    return EDPALETA_NADA;
}
