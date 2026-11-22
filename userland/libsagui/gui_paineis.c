// SPDX-License-Identifier: GPL-3.0-only
/* gui_paineis.c - paneles divisibles (D454).
 *
 * Un árbol de nodos: las hojas llevan el identificador que pone la aplicación y las divisiones (PD_LADO: a | b,
 * PD_PILHA: a sobre b) llevan la posición del separador en milésimas. El componente solo calcula las áreas y trata los
 * separadores (arrastrar, plegar con la flecha del medio); el contenido de cada hoja lo pinta la aplicación en el
 * rectángulo que da paineis_rect. Tamaños mínimos: un separador no pasa de min_a ni deja a b por debajo de min_b. */
#include "gui.h"

static bool dentro(int px, int py, int x, int y, int l, int a)
{
    return px >= x && py >= y && px < x + l && py < y + a;
}

void paineis_iniciar(paineis_t *p, int x, int y, int l, int a)
{
    memset(p, 0, sizeof *p);
    p->x = x; p->y = y; p->l = l; p->a = a;
    p->raiz = -1; p->arrasto = -1; p->hover = -1;
}

int paineis_folha(paineis_t *p, int id)
{
    if (p->n >= PAINEIS_MAX) return -1;
    pd_no_t *n = &p->no[p->n];
    memset(n, 0, sizeof *n);
    n->tipo = PD_FOLHA; n->id = id; n->a = n->b = -1;
    p->raiz = p->n;
    return p->n++;
}

int paineis_dividir(paineis_t *p, int tipo, int a, int b, int pos, int min_a, int min_b, int plegavel)
{
    if (p->n >= PAINEIS_MAX || a < 0 || b < 0 || a >= p->n || b >= p->n) return -1;
    pd_no_t *n = &p->no[p->n];
    memset(n, 0, sizeof *n);
    n->tipo = tipo == PD_PILHA ? PD_PILHA : PD_LADO;
    n->a = a; n->b = b;
    n->pos = pos < 0 ? 0 : pos > 1000 ? 1000 : pos;
    n->min_a = min_a; n->min_b = min_b; n->plegavel = plegavel;
    p->raiz = p->n;
    return p->n++;
}

void paineis_raiz(paineis_t *p, int no) { if (no >= 0 && no < p->n) p->raiz = no; }

/* Tamaño en píxeles de a dentro de un espacio total (sin el separador). */
static int tam_a(const pd_no_t *n, int total)
{
    if (n->plegado && n->plegavel == 1) return 0;
    if (n->plegado && n->plegavel == 2) return total;
    int a = total * n->pos / 1000;
    int mn = n->min_a, mx = total - n->min_b;
    if (mx < mn) return total * n->min_a / (n->min_a + n->min_b ? n->min_a + n->min_b : 1);   /* no caben los dos mínimos: reparto proporcional */
    if (a < mn) a = mn;
    if (a > mx) a = mx;
    return a;
}

static void calcular(paineis_t *p, int i, int x, int y, int l, int h)
{
    pd_no_t *n = &p->no[i];
    n->x = x; n->y = y; n->l = l; n->h = h;
    n->sl = n->sh = 0;
    if (n->tipo == PD_FOLHA) return;
    if (n->tipo == PD_LADO) {
        int total = l - PAINEIS_SEP;
        if (total < 0) total = 0;
        int a = tam_a(n, total);
        n->sx = x + a; n->sy = y; n->sl = PAINEIS_SEP; n->sh = h;
        calcular(p, n->a, x, y, a, h);
        calcular(p, n->b, x + a + PAINEIS_SEP, y, total - a, h);
    } else {
        int total = h - PAINEIS_SEP;
        if (total < 0) total = 0;
        int a = tam_a(n, total);
        n->sx = x; n->sy = y + a; n->sl = l; n->sh = PAINEIS_SEP;
        calcular(p, n->a, x, y, l, a);
        calcular(p, n->b, x, y + a + PAINEIS_SEP, l, total - a);
    }
}

void paineis_calcular(paineis_t *p)
{
    if (p->raiz >= 0) calcular(p, p->raiz, p->x, p->y, p->l, p->a);
}

void paineis_mover(paineis_t *p, int x, int y, int l, int a)
{
    p->x = x; p->y = y; p->l = l; p->a = a;
    paineis_calcular(p);
}

bool paineis_rect(const paineis_t *p, int id, int *x, int *y, int *l, int *a)
{
    for (int i = 0; i < p->n; i++)
        if (p->no[i].tipo == PD_FOLHA && p->no[i].id == id) {
            const pd_no_t *n = &p->no[i];
            if (x) *x = n->x;
            if (y) *y = n->y;
            if (l) *l = n->l;
            if (a) *a = n->h;
            return n->l > 0 && n->h > 0;
        }
    return false;
}

/* El botón de plegar: una pastilla centrada en el separador. */
static void rect_boton(const pd_no_t *n, int *x, int *y, int *l, int *a)
{
    if (n->tipo == PD_LADO) { *l = PAINEIS_SEP; *a = 28; *x = n->sx; *y = n->sy + (n->sh - *a) / 2; }
    else { *l = 28; *a = PAINEIS_SEP; *x = n->sx + (n->sl - *l) / 2; *y = n->sy; }
}

void paineis_desenhar(jan_t *j, const paineis_t *p)
{
    for (int i = 0; i < p->n; i++) {
        const pd_no_t *n = &p->no[i];
        if (n->tipo == PD_FOLHA || n->sl <= 0 || n->sh <= 0) continue;
        bool vivo = i == p->arrasto || i == p->hover;
        g_retangulo(j, n->sx, n->sy, n->sl, n->sh, vivo ? mod_tema.acento_suave : mod_tema.barra);
        /* un filete fino en el medio y tres puntos de agarre */
        if (n->tipo == PD_LADO) {
            g_retangulo(j, n->sx + n->sl / 2, n->sy, 1, n->sh, i == p->arrasto ? mod_tema.acento : mod_tema.borde);
        } else {
            g_retangulo(j, n->sx, n->sy + n->sh / 2, n->sl, 1, i == p->arrasto ? mod_tema.acento : mod_tema.borde);
        }
        if (n->plegavel) {
            int bx, by, bl, ba;
            rect_boton(n, &bx, &by, &bl, &ba);
            g_retangulo_red(j, bx, by, bl, ba, 2, vivo ? mod_tema.acento : mod_tema.borde_forte);
            /* flecha hacia donde se va a plegar (o a desplegar si ya está plegado) */
            bool hacia_a = (n->plegavel == 1) != n->plegado;
            int cx = bx + bl / 2, cy = by + ba / 2;
            uint32_t c = mod_tema.sobre_acento;
            if (n->tipo == PD_LADO) {
                int d = hacia_a ? -1 : 1;
                g_retangulo(j, cx, cy - 1, 1, 3, c);
                g_retangulo(j, cx + d, cy, 1, 1, c);
            } else {
                int d = hacia_a ? -1 : 1;
                g_retangulo(j, cx - 1, cy, 3, 1, c);
                g_retangulo(j, cx, cy + d, 1, 1, c);
            }
        }
        jan_mostrar_area(j, n->sx, n->sy, n->sl, n->sh);
    }
}

/* ¿Qué división tiene su separador bajo el punto? */
static int separador_em(const paineis_t *p, int x, int y)
{
    for (int i = 0; i < p->n; i++) {
        const pd_no_t *n = &p->no[i];
        if (n->tipo != PD_FOLHA && dentro(x, y, n->sx, n->sy, n->sl, n->sh)) return i;
    }
    return -1;
}

void paineis_plegar(paineis_t *p, int no, bool plegar)
{
    if (no < 0 || no >= p->n || p->no[no].tipo == PD_FOLHA || !p->no[no].plegavel) return;
    p->no[no].plegado = plegar;
    paineis_calcular(p);
}

int paineis_tratar(jan_t *j, paineis_t *p, const evento_t *e)
{
    if (p->raiz < 0) return PAINEIS_NADA;
    if (e->tipo == EV_MOVER) {
        if (p->arrasto >= 0 && (e->botoes & 1)) {
            pd_no_t *n = &p->no[p->arrasto];
            int total = (n->tipo == PD_LADO ? n->l : n->h) - PAINEIS_SEP;
            if (total <= 0) return PAINEIS_NADA;
            int ini = n->tipo == PD_LADO ? n->x : n->y;
            int m = (n->tipo == PD_LADO ? e->x : e->y) - ini - PAINEIS_SEP / 2;
            int nuevo = m * 1000 / total;
            if (nuevo < 0) nuevo = 0;
            if (nuevo > 1000) nuevo = 1000;
            n->plegado = false;
            /* los mínimos se aplican en tam_a; guardar la posición pedida */
            if (nuevo == n->pos) return PAINEIS_NADA;
            n->pos = nuevo;
            paineis_calcular(p);
            return PAINEIS_ARRASTA;
        }
        int h = separador_em(p, e->x, e->y);
        if (h != p->hover) {
            p->hover = h;
            paineis_desenhar(j, p);
        }
        return PAINEIS_NADA;
    }
    if (e->tipo == EV_CLIQUE && (e->botoes & 1)) {
        int i = separador_em(p, e->x, e->y);
        if (i < 0) return PAINEIS_NADA;
        pd_no_t *n = &p->no[i];
        if (n->plegavel) {
            int bx, by, bl, ba;
            rect_boton(n, &bx, &by, &bl, &ba);
            if (dentro(e->x, e->y, bx, by, bl, ba)) {
                n->plegado = !n->plegado;
                paineis_calcular(p);
                return PAINEIS_PLEGOU;
            }
        }
        if (e->cliques >= 2 && n->plegavel) {   /* doble clic en el separador: también pliega */
            n->plegado = !n->plegado;
            paineis_calcular(p);
            return PAINEIS_PLEGOU;
        }
        p->arrasto = i;
        paineis_desenhar(j, p);
        return PAINEIS_NADA;
    }
    if (e->tipo == EV_SOLTOU && p->arrasto >= 0) {
        p->arrasto = -1;
        return PAINEIS_SOLTOU;
    }
    return PAINEIS_NADA;
}

/* ------------------------------------------------------------ guardar y restaurar la disposición */

int paineis_guardar(const paineis_t *p, char *buf, int max)
{
    int k = 0;
    if (max < 1) return 0;
    buf[0] = 0;
    for (int i = 0; i < p->n; i++) {
        if (p->no[i].tipo == PD_FOLHA) continue;
        char t[24];
        int m = formatar_texto(t, sizeof t, "%d,%d;", p->no[i].pos, p->no[i].plegado ? 1 : 0);
        if (k + m >= max) break;
        memcpy(buf + k, t, (size_t)m);
        k += m;
        buf[k] = 0;
    }
    return k;
}

bool paineis_restaurar(paineis_t *p, const char *txt)
{
    int pos[PAINEIS_MAX], pleg[PAINEIS_MAX], n = 0;
    while (*txt && n < PAINEIS_MAX) {
        int v = 0, s = 0;
        if (*txt < '0' || *txt > '9') return false;
        while (*txt >= '0' && *txt <= '9') v = v * 10 + (*txt++ - '0');
        if (*txt != ',') return false;
        txt++;
        while (*txt >= '0' && *txt <= '9') s = s * 10 + (*txt++ - '0');
        if (*txt != ';') return false;
        txt++;
        pos[n] = v > 1000 ? 1000 : v; pleg[n] = s; n++;
    }
    int divs = 0;
    for (int i = 0; i < p->n; i++) if (p->no[i].tipo != PD_FOLHA) divs++;
    if (n != divs) return false;
    int k = 0;
    for (int i = 0; i < p->n; i++) {
        if (p->no[i].tipo == PD_FOLHA) continue;
        p->no[i].pos = pos[k];
        p->no[i].plegado = pleg[k] && p->no[i].plegavel;
        k++;
    }
    paineis_calcular(p);
    return true;
}
