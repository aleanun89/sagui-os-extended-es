// SPDX-License-Identifier: GPL-2.0-or-later
/* pdf_geom.c - geometría del motor de PDF (mejora 059, D138).
 *
 * Ver pdf_geom.h. Son tres cosas: los caminos (listas de segmentos), el
 * aplanado de las curvas y las matrices 2x3 de PDF. */
#include "pdf_geom.h"

/* ------------------------------------------------------------ caminos */

void pdf_caminho_limpar(pdf_caminho_t *c)
{
    c->n = 0;
    c->fechado = false;
}

void pdf_caminho_liberar(pdf_caminho_t *c)
{
    if (c->segs) libera(c->segs);
    c->segs = NULL;
    c->n = c->cap = 0;
    c->fechado = false;
}

bool pdf_caminho_add(pdf_caminho_t *c, const pdf_segmento_t *s)
{
    if (c->n >= c->cap) {
        int nova = c->cap ? c->cap * 2 : 16;
        pdf_segmento_t *p = realoca(c->segs, (size_t)nova * sizeof(pdf_segmento_t));
        if (!p) return false;
        c->segs = p;
        c->cap = nova;
    }
    c->segs[c->n++] = *s;
    return true;
}

bool pdf_caminho_linha(pdf_caminho_t *c, double x0, double y0, double x1, double y1)
{
    pdf_segmento_t s;
    s.tipo = PDF_TRA_LINHA;
    s.x0 = x0; s.y0 = y0; s.x1 = x1; s.y1 = y1;
    s.cx1 = s.cy1 = s.cx2 = s.cy2 = 0;
    return pdf_caminho_add(c, &s);
}

bool pdf_caminho_cubica(pdf_caminho_t *c, double x0, double y0,
                        double cx1, double cy1, double cx2, double cy2,
                        double x1, double y1)
{
    pdf_segmento_t s;
    s.tipo = PDF_TRA_CUBICA;
    s.x0 = x0; s.y0 = y0; s.x1 = x1; s.y1 = y1;
    s.cx1 = cx1; s.cy1 = cy1; s.cx2 = cx2; s.cy2 = cy2;
    return pdf_caminho_add(c, &s);
}

/* ------------------------------------------------------------ aplanado */

/* No hay libm: el seno, el cosseno y la raiz salen de aqui. La serie de
 * Taylor basta de sobra para los angulos del PDF (de -360 a 360 grados) y
 * para la raiz se usa el metodo de Newton, que con la x87 es rapidisimo. */
#define PDF_PI 3.14159265358979323846

static double pdf_raiz(double x)
{
    if (x <= 0) return 0;
    double r = x;
    if (r < 1) r = 1;
    for (int i = 0; i < 24; i++) {
        double novo = 0.5 * (r + x / r);
        if (novo == r) break;
        r = novo;
    }
    return r;
}

/* El seno, reduciendo antes al primer cuadrante. */
static double pdf_seno(double x)
{
    while (x < 0) x += 2 * PDF_PI;
    while (x >= 2 * PDF_PI) x -= 2 * PDF_PI;
    if (x > PDF_PI / 2) x = PDF_PI - x;
    double x2 = x * x;
    return x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72))));
}

/* El cosseno. */
static double pdf_cosseno(double x)
{
    while (x < 0) x += 2 * PDF_PI;
    while (x >= 2 * PDF_PI) x -= 2 * PDF_PI;
    if (x > PDF_PI / 2) x = PDF_PI - x;
    double x2 = x * x;
    return 1 - x2 / 2 * (1 - x2 / 12 * (1 - x2 / 30 * (1 - x2 / 56)));
}

/* Cuantos tramos rectos hacen falta para que la poligonal no se separe de
 * la cubica mas que la tolerancia (formula de Wang): con L el mayor de
 * |P0 - 2P1 + P2| y |P1 - 2P2 + P3| (las segundas diferencias), basta con
 * n = raiz(3 L / (4 tolerancia)). La cota antigua (|P0-3P1+3P2-P3|/18,
 * la tercera diferencia) vale casi 0 en un arco simetrico como el cuarto de
 * circulo de una "o", y la curva salia como una recta: letras en rombo. */
static int tramos_cubica(double x0, double y0, double cx1, double cy1,
                         double cx2, double cy2, double x1, double y1, double tolerancia)
{
    double ax = x0 - 2 * cx1 + cx2, ay = y0 - 2 * cy1 + cy2;
    double bx = cx1 - 2 * cx2 + x1, by = cy1 - 2 * cy2 + y1;
    double la = ax * ax + ay * ay, lb = bx * bx + by * by;
    double l = pdf_raiz(la > lb ? la : lb);
    if (tolerancia <= 0) tolerancia = 0.25;
    double q = 0.75 * l / tolerancia;
    if (q <= 1.0) return 1;
    int n = (int)pdf_raiz(q) + 1;
    if (n > 64) n = 64;
    return n;
}

bool pdf_caminho_achatado(const pdf_caminho_t *c, pdf_caminho_t *saida, double tolerancia)
{
    pdf_caminho_limpar(saida);
    if (tolerancia <= 0) tolerancia = 0.25;
    for (int i = 0; i < c->n; i++) {
        const pdf_segmento_t *s = &c->segs[i];
        if (s->tipo == PDF_TRA_LINHA) {
            if (!pdf_caminho_linha(saida, s->x0, s->y0, s->x1, s->y1)) return false;
            continue;
        }
        int n = tramos_cubica(s->x0, s->y0, s->cx1, s->cy1, s->cx2, s->cy2, s->x1, s->y1,
                              tolerancia);
        double px = s->x0, py = s->y0;
        for (int k = 1; k <= n; k++) {
            double t = (double)k / (double)n;
            double mt = 1.0 - t;
            double a = mt * mt * mt, b = 3 * mt * mt * t, c2 = 3 * mt * t * t, d = t * t * t;
            double x = a * s->x0 + b * s->cx1 + c2 * s->cx2 + d * s->x1;
            double y = a * s->y0 + b * s->cy1 + c2 * s->cy2 + d * s->y1;
            if (!pdf_caminho_linha(saida, px, py, x, y)) return false;
            px = x; py = y;
        }
    }
    saida->fechado = c->fechado;
    return true;
}

/* ------------------------------------------------------------ matrices */

pdf_mat_t pdf_mat_identidade(void)
{
    pdf_mat_t m;
    m.a = 1; m.b = 0; m.c = 0; m.d = 1; m.e = 0; m.f = 0;
    return m;
}

/* El producto es m * n: primero se aplica n y luego m (como en PDF, donde
 * CTM = CTM x M). */
pdf_mat_t pdf_mat_multiplica(const pdf_mat_t *m, const pdf_mat_t *n)
{
    pdf_mat_t r;
    r.a = m->a * n->a + m->b * n->c;
    r.b = m->a * n->b + m->b * n->d;
    r.c = m->c * n->a + m->d * n->c;
    r.d = m->c * n->b + m->d * n->d;
    r.e = m->e * n->a + m->f * n->c + n->e;
    r.f = m->e * n->b + m->f * n->d + n->f;
    return r;
}

pdf_mat_t pdf_mat_translada(double tx, double ty)
{
    pdf_mat_t m = pdf_mat_identidade();
    m.e = tx; m.f = ty;
    return m;
}

pdf_mat_t pdf_mat_escalada(double sx, double sy)
{
    pdf_mat_t m = pdf_mat_identidade();
    m.a = sx; m.d = sy;
    return m;
}

pdf_mat_t pdf_mat_rotacion(double graus)
{
    /* El PDF gira en sentido contrario al mathematico (la Y crece hacia
     * arriba, asi que un giro antihorario en la pantalla es horario en las
     * coordenadas del PDF). */
    double r = graus * 3.14159265358979323846 / 180.0;
    double s = pdf_seno(r), c = pdf_cosseno(r);
    pdf_mat_t m;
    m.a = c;  m.b = s;
    m.c = -s; m.d = c;
    m.e = 0;  m.f = 0;
    return m;
}

void pdf_mat_aplica(const pdf_mat_t *m, double x, double y, double *rx, double *ry)
{
    *rx = m->a * x + m->c * y + m->e;
    *ry = m->b * x + m->d * y + m->f;
}

double pdf_mat_escala_media(const pdf_mat_t *m)
{
    /* La escala "media" de una matriz: la media de la escala de los ejes.
     * Sirve para saber cuanto se agranda un trazo al transformar. */
    double sx = pdf_raiz(m->a * m->a + m->b * m->b);
    double sy = pdf_raiz(m->c * m->c + m->d * m->d);
    return (sx + sy) * 0.5;
}
