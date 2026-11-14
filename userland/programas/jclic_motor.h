// SPDX-License-Identifier: GPL-2.0-or-later
/* jclic_motor.h - las reglas de las actividades de JClic y su disposición (D185).
 *
 * Todo el estado de la actividad en curso vive aquí: los paneles (A = "primary", B = "secondary")
 * con sus cajas, los contadores, la selección y los temporizadores. Los gestos del ratón se reducen
 * a dos operaciones, "clique" (pulsar y soltar en la misma caja) y "gesto" (arrastrar de una caja
 * a otra); la ventana y la prueba automática llaman a las mismas funciones, así lo que resuelve
 * --autoprueba es exactamente lo que hace un jugador.
 *
 * Tipos de actividad que se juegan: pantalla de información, puzles doble / de intercambio / de
 * agujero, asociación simple y compleja, memoria, explorar, identificar, sopa de letras y
 * crucigrama. Las de texto (@text.*) y las desconocidas avisan y se saltan. */
#ifndef JCLIC_MOTOR_H
#define JCLIC_MOTOR_H
#include "jclic_dados.h"

enum { E_NORMAL, E_VAZIO, E_ACERTO, E_VIRADO, E_ERRO };

typedef struct {
    int       x, y, w, h;
    grupo_t  *g;
    int       cel;      /* celda del grupo que se enseña (-1 = ninguna) */
    int       orig;     /* identidad lógica de la pieza (pareja, posición original...) */
    uint8_t   est;
} cx_t;

typedef struct {
    bool  tem;
    int   n, col, lin;
    int   cw, ch;       /* tamaño natural de la celda */
    int   x, y, w, h;   /* rectángulo del panel en la ventana */
    cx_t *c;
} pn_t;

static ativ_t at;
static pn_t   pn[2];
static int    sel_p = -1, sel_i = -1;
static int    acertos, tentativas, alvo;
static bool   resolvida, mostrar_final;
static uint32_t t_ini, t_fim;
static int    memo_a = -1, memo_b = -1;       /* memoria: las dos cartas vueltas que esperan taparse */
static uint32_t memo_ate;
static int    err_p = -1, err_i = -1;         /* caja que parpadea en rojo */
static uint32_t err_ate;
static int    exp_a = -1, exp_b = -1;         /* explorar: caja de A elegida y su pareja en B */
static uint32_t rng_estado = 12345;
static const char *som_pedido;                /* el medio de sonido de la última celda pulsada (punto de enganche, ver tocar_midia en jclic.c) */
static const char *salto_pedido;              /* una celda pide saltar a otra actividad (RUN_CLIC_ACTIVITY) */
static int    jw = 800, jh = 560;             /* área de la ventana */
static int    area_x, area_y, area_w, area_h; /* donde va el tablero */

/* sopa de letras */
static char    sp_letras[TG_MAX][TG_MAX + 1];
static uint8_t sp_est[TG_MAX][TG_MAX];        /* 1 = parte de una palabra encontrada */
static bool    sp_ok[PAL_MAX];
static int     sp_x0 = -1, sp_y0, sp_x1, sp_y1;  /* selección en curso (celdas) */
static int     sp_ox, sp_oy, sp_cw, sp_ch;       /* origen y tamaño de celda del tablero */
/* crucigrama */
static char    cz_user[TG_MAX][TG_MAX + 1];
static int     cz_fx = -1, cz_fy, cz_dir;        /* foco y sentido (0 horizontal, 1 vertical) */
static int     cz_num[TG_MAX][TG_MAX];
/* agujero: el camino del embarajado, para que la autoprueba lo deshaga */
static int     bu_hist[4096], bu_n_hist, bu_buraco;

static uint32_t alea(void)
{
    rng_estado ^= rng_estado << 13;
    rng_estado ^= rng_estado >> 17;
    rng_estado ^= rng_estado << 5;
    return rng_estado;
}

static void efeito_ok(void) { static const nota_t n[] = { { 660, 70 }, { 880, 90 } }; efeito_tocar(n, 2); }
static void efeito_erro(void) { static const nota_t n[] = { { 220, 140 } }; efeito_tocar(n, 1); }
static void efeito_fim(void) { static const nota_t n[] = { { 523, 90 }, { 659, 90 }, { 784, 90 }, { 1047, 220 } }; efeito_tocar(n, 4); }

static int maiuscula(int c)
{
    if ((c >= 'a' && c <= 'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7)) return c - 0x20;
    return c;
}

static void permutar(int *v, int n)
{
    for (int i = n - 1; i > 0; i--) {
        int j = (int)(alea() % (uint32_t)(i + 1));
        int t = v[i]; v[i] = v[j]; v[j] = t;
    }
}

/* Un embarajado que no es la identidad (si se puede). */
static void embaralhar(int *v, int n)
{
    for (int i = 0; i < n; i++) v[i] = i;
    if (n < 2) return;
    for (int tent = 0; tent < 8; tent++) {
        permutar(v, n);
        bool ident = true;
        for (int i = 0; i < n; i++) if (v[i] != i) { ident = false; break; }
        if (!ident) return;
    }
}

static pn_t *painel_novo(int k, grupo_t *g, int n)
{
    pn_t *p = &pn[k];
    memset(p, 0, sizeof *p);
    p->tem = true;
    p->n = n;
    p->col = g->col;
    p->lin = g->lin;
    p->cw = g->cw;
    p->ch = g->ch;
    p->c = ar_pedir(&ar_at, (size_t)(n > 0 ? n : 1) * sizeof(cx_t));
    for (int i = 0; i < n; i++) { p->c[i].g = g; p->c[i].cel = i; p->c[i].orig = i; }
    return p;
}


static void checar_fim(void)
{
    if (resolvida) return;
    if (alvo > 0 && acertos >= alvo) {
        resolvida = true;
        mostrar_final = true;
        t_fim = tempo_ms();
        efeito_fim();
        registrar("jclic: resuelta aciertos=%d intentos=%d", acertos, tentativas);
    }
}

/* ------------------------------------------------------------ disposición */

static void motor_layout(void)
{
    if (at.tipo == T_SOPA || at.tipo == T_CRUZ) return;   /* lo dispone el dibujo */
    pn_t *a = &pn[0], *b = &pn[1];
    if (!a->tem) return;
    int gap = 14;
    bool dois = b->tem;
    bool vert = dois && acha_char(at.layout, 'U') != NULL;
    bool b_primeiro = false;
    if (dois) {
        const char *pa = acha_char(at.layout, 'A'), *pb = acha_char(at.layout, 'B');
        b_primeiro = pa && pb && pb < pa;
    }
    int wa = a->col * a->cw, ha = a->lin * a->ch;
    int wb = dois ? b->col * b->cw : 0, hb = dois ? b->lin * b->ch : 0;
    int totw, toth;
    if (!dois) { totw = wa; toth = ha; }
    else if (vert) { totw = wa > wb ? wa : wb; toth = ha + hb; }
    else { totw = wa + wb; toth = ha > hb ? ha : hb; }
    /* el hueco entre paneles se resta del área antes de escalar */
    int dw = area_w - 16 - (dois && !vert ? gap : 0), dh = area_h - 16 - (dois && vert ? gap : 0);
    if (dw < 40) dw = 40;
    if (dh < 40) dh = 40;
    int s1 = (int)((long)dw * 1024 / totw), s2 = (int)((long)dh * 1024 / toth);
    int s = s1 < s2 ? s1 : s2;
    if (s > 3072) s = 3072;
    if (s < 64) s = 64;
    int pwa = (int)((long)wa * s / 1024), pha = (int)((long)ha * s / 1024);
    int pwb = (int)((long)wb * s / 1024), phb = (int)((long)hb * s / 1024);
    int tw = !dois ? pwa : vert ? (pwa > pwb ? pwa : pwb) : pwa + gap + pwb;
    int th = !dois ? pha : vert ? pha + gap + phb : (pha > phb ? pha : phb);
    int ox = area_x + (area_w - tw) / 2, oy = area_y + (area_h - th) / 2;
    pn_t *p1 = b_primeiro ? b : a, *p2 = b_primeiro ? a : b;
    int w1 = b_primeiro ? pwb : pwa, h1 = b_primeiro ? phb : pha;
    int w2 = b_primeiro ? pwa : pwb, h2 = b_primeiro ? pha : phb;
    p1->x = ox; p1->y = oy; p1->w = w1; p1->h = h1;
    if (dois) {
        if (vert) { p1->x = ox + (tw - w1) / 2; p2->x = ox + (tw - w2) / 2; p2->y = oy + h1 + gap; }
        else { p1->y = oy + (th - h1) / 2; p2->x = ox + w1 + gap; p2->y = oy + (th - h2) / 2; }
        p2->w = w2; p2->h = h2;
    }
    for (int k = 0; k < 2; k++) {
        pn_t *p = &pn[k];
        if (!p->tem) continue;
        for (int i = 0; i < p->n; i++) {
            int r = i / p->col, c = i % p->col;
            p->c[i].x = p->x + c * p->w / p->col;
            p->c[i].y = p->y + r * p->h / p->lin;
            p->c[i].w = p->x + (c + 1) * p->w / p->col - p->c[i].x;
            p->c[i].h = p->y + (r + 1) * p->h / p->lin - p->c[i].y;
        }
    }
    /* la sopa y el crucigrama no pasan por aquí; la memoria usa solo A */
}

/* ------------------------------------------------------------ inicio de cada tipo */

static void iniciar_sopa(void)
{
    memset(sp_est, 0, sizeof sp_est);
    memset(sp_ok, 0, sizeof sp_ok);
    int na = (int)tam_texto(at.alfabeto);
    if (na < 2) { copia_texto(at.alfabeto, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", sizeof at.alfabeto); na = 26; }
    for (int y = 0; y < at.tg_lin; y++) {
        for (int x = 0; x < at.tg_col; x++) {
            int c = (unsigned char)at.tg[y][x];
            if (c == '*' || c == ' ' || c == '_') c = (unsigned char)at.alfabeto[alea() % (uint32_t)na];
            sp_letras[y][x] = (char)maiuscula(c);
        }
        sp_letras[y][at.tg_col] = 0;
    }
    /* las palabras, en mayúsculas y sin espacios */
    for (int i = 0; i < at.n_pal; i++) {
        char *d = at.pal[i], *o = at.pal[i];
        for (; *d; d++) if (*d != ' ') *o++ = (char)maiuscula((unsigned char)*d);
        *o = 0;
    }
    sp_x0 = -1;
    alvo = at.n_pal;
}

static void iniciar_cruzado(void)
{
    memset(cz_user, 0, sizeof cz_user);
    memset(cz_num, 0, sizeof cz_num);
    int tot = 0, nm = 0;
    for (int y = 0; y < at.tg_lin; y++)
        for (int x = 0; x < at.tg_col; x++) {
            cz_user[y][x] = ' ';
            bool l = at.tg[y][x] != '*' && at.tg[y][x] != ' ';
            if (!l) continue;
            tot++;
            bool ini_h = (x == 0 || at.tg[y][x - 1] == '*') && x + 1 < at.tg_col && at.tg[y][x + 1] != '*';
            bool ini_v = (y == 0 || at.tg[y - 1][x] == '*') && y + 1 < at.tg_lin && at.tg[y + 1][x] != '*';
            if (ini_h || ini_v) cz_num[y][x] = ++nm;
        }
    for (int y = 0; y < at.tg_lin; y++) cz_user[y][at.tg_col] = 0;
    cz_fx = -1;
    cz_dir = 0;
    alvo = tot;
}

static int mem_n_pares;

static void motor_iniciar(void)
{
    memset(pn, 0, sizeof pn);
    sel_p = sel_i = -1;
    acertos = tentativas = alvo = 0;
    resolvida = mostrar_final = false;
    memo_a = memo_b = -1;
    err_p = err_i = -1;
    exp_a = exp_b = -1;
    bu_n_hist = 0;
    t_ini = tempo_ms();
    t_fim = 0;
    grupo_t *ga = &at.g[G_PRIM], *gb = &at.g[G_SEC];
    switch (at.tipo) {
    case T_INFO: {
        if (!ga->tem) break;
        painel_novo(0, ga, ga->n);
        break;
    }
    case T_DOBLE: {
        if (!ga->tem) break;
        int n = ga->n, *v = ar_pedir(&ar_at, (size_t)n * sizeof(int));
        embaralhar(v, n);
        pn_t *a = painel_novo(0, ga, n), *b = painel_novo(1, ga, n);
        for (int i = 0; i < n; i++) { a->c[i].cel = a->c[i].orig = v[i]; b->c[i].cel = -1; b->c[i].orig = -1; b->c[i].est = E_VAZIO; }
        alvo = n;
        break;
    }
    case T_INTERC: {
        if (!ga->tem) break;
        int n = ga->n, *v = ar_pedir(&ar_at, (size_t)n * sizeof(int));
        embaralhar(v, n);
        pn_t *a = painel_novo(0, ga, n);
        for (int i = 0; i < n; i++) { a->c[i].cel = a->c[i].orig = v[i]; if (v[i] == i) acertos++; }
        alvo = n;
        break;
    }
    case T_BURACO: {
        if (!ga->tem) break;
        int n = ga->n;
        pn_t *a = painel_novo(0, ga, n);
        int h = n - 1;   /* el hueco es la última pieza */
        a->c[h].est = E_VAZIO;
        /* se embaraja con movimientos legales, para que siempre tenga solución */
        int anterior = -1;
        for (int k = 0; k < n * 24 && bu_n_hist < 4000; k++) {
            int r = h / a->col, c = h % a->col, viz[4], nv = 0;
            if (r > 0) viz[nv++] = h - a->col;
            if (r < a->lin - 1) viz[nv++] = h + a->col;
            if (c > 0) viz[nv++] = h - 1;
            if (c < a->col - 1) viz[nv++] = h + 1;
            int q = viz[alea() % (uint32_t)nv];
            if (q == anterior && nv > 1) continue;
            cx_t t = a->c[q]; a->c[q] = a->c[h]; a->c[h] = t;
            /* se intercambian solo cel/orig/est, no la geometría (aún sin calcular) */
            bu_hist[bu_n_hist++] = h;
            anterior = h;
            h = q;
        }
        bu_buraco = h;
        for (int i = 0; i < n; i++) if (a->c[i].est != E_VAZIO && a->c[i].orig == i) acertos++;   /* ya en su sitio */
        alvo = n;
        break;
    }
    case T_ASOC_S:
    case T_ASOC_C:
    case T_EXPLORA: {
        if (!ga->tem) break;
        int na = ga->n, nb = gb->tem ? gb->n : 0;
        int *va = ar_pedir(&ar_at, (size_t)na * sizeof(int)), *vb = ar_pedir(&ar_at, (size_t)(nb > 0 ? nb : 1) * sizeof(int));
        for (int i = 0; i < na; i++) va[i] = i;
        for (int i = 0; i < nb; i++) vb[i] = i;
        if (at.emb_prim) embaralhar(va, na);
        if (at.emb_sec) embaralhar(vb, nb);
        pn_t *a = painel_novo(0, ga, na);
        for (int i = 0; i < na; i++) { a->c[i].cel = a->c[i].orig = va[i]; }
        if (nb) {
            pn_t *b = painel_novo(1, gb, nb);
            for (int i = 0; i < nb; i++) { b->c[i].cel = b->c[i].orig = vb[i]; }
        }
        if (at.tipo == T_ASOC_S) alvo = na < nb ? na : nb;
        else if (at.tipo == T_ASOC_C) {
            for (int i = 0; i < na; i++) if (ga->c[i].id >= 0) alvo++;
            if (alvo == 0) alvo = na;
        }
        break;
    }
    case T_MEMORIA: {
        if (!ga->tem) break;
        int na = ga->n, nb = gb->tem ? gb->n : 0;
        /* una baraja: las celdas de A y sus parejas (las de B o, sin B, la segunda mitad de A) */
        int pares = nb ? (na < nb ? na : nb) : na / 2;
        if (pares < 1) break;
        int m = pares * 2;
        int col = 2;
        while (col * col < m * 3 / 2) col++;
        grupo_t *gg = ga;
        static grupo_t barato;
        barato = *gg;
        barato.col = col;
        barato.lin = (m + col - 1) / col;
        pn_t *a = painel_novo(0, &barato, m);
        a->col = col;
        a->lin = barato.lin;
        int *v = ar_pedir(&ar_at, (size_t)m * sizeof(int));
        embaralhar(v, m);
        for (int i = 0; i < m; i++) {
            int k = v[i];
            int par = k % pares;
            bool segunda = k >= pares;
            a->c[i].orig = par;
            a->c[i].est = E_NORMAL;
            if (nb) { a->c[i].g = segunda ? gb : ga; a->c[i].cel = par; }
            else { a->c[i].g = ga; a->c[i].cel = segunda ? par + pares : par; }
        }
        a->cw = ga->cw; a->ch = ga->ch;
        mem_n_pares = pares;
        alvo = pares;
        break;
    }
    case T_IDENT: {
        if (!ga->tem) break;
        int n = ga->n, *v = ar_pedir(&ar_at, (size_t)n * sizeof(int));
        for (int i = 0; i < n; i++) v[i] = i;
        if (at.emb_prim) embaralhar(v, n);
        pn_t *a = painel_novo(0, ga, n);
        for (int i = 0; i < n; i++) a->c[i].cel = a->c[i].orig = v[i];
        for (int i = 0; i < n; i++) if (ga->c[i].id > 0) alvo++;   /* sin ninguna correcta es un menú de saltos: no se resuelve */
        break;
    }
    case T_SOPA: iniciar_sopa(); break;
    case T_CRUZ: iniciar_cruzado(); break;
    default: break;
    }
    if (at.tipo == T_BURACO) { /* nada: el hueco ya está */ }
    motor_layout();
    /* la geometría se calculó después del embarajado del agujero: nada más que hacer */
}

/* ------------------------------------------------------------ gestos */

static int zona_em(int p, int x, int y)
{
    if (p < 0 || !pn[p].tem) return -1;
    for (int i = 0; i < pn[p].n; i++) {
        cx_t *b = &pn[p].c[i];
        if (x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h) return i;
    }
    return -1;
}

static void marcar_erro(int p, int i)
{
    err_p = p; err_i = i;
    err_ate = tempo_ms() + 450;
    efeito_erro();
}

static void contar_lugares(void)
{
    acertos = 0;
    for (int i = 0; i < pn[0].n; i++) if (pn[0].c[i].est != E_VAZIO && pn[0].c[i].orig == i) acertos++;
}

static bool vizinhos(pn_t *p, int a, int b)
{
    int ra = a / p->col, ca = a % p->col, rb = b / p->col, cb = b % p->col;
    return (ra == rb && (ca - cb == 1 || cb - ca == 1)) || (ca == cb && (ra - rb == 1 || rb - ra == 1));
}

/* Una asociación entre la caja ia de A y la ib de B. Devuelve true si es correcta. */
static bool asoc_confere(int ia, int ib)
{
    cx_t *a = &pn[0].c[ia], *b = &pn[1].c[ib];
    if (at.tipo == T_ASOC_C) {
        int id = a->g->c[a->cel].id;
        if (id < 0) id = a->cel;
        return id == b->orig;
    }
    return a->orig == b->orig;
}

/* Arrastrar de la caja (p1,i1) a la (p2,i2). */
static void gesto(int p1, int i1, int p2, int i2)
{
    if (resolvida || p1 < 0 || p2 < 0 || i1 < 0 || i2 < 0) return;
    if (!pn[p1].tem || !pn[p2].tem) return;
    cx_t *o = &pn[p1].c[i1], *d = &pn[p2].c[i2];
    sel_p = sel_i = -1;
    switch (at.tipo) {
    case T_DOBLE:
        if (p1 != 0 || p2 != 1 || o->est == E_VAZIO || d->est != E_VAZIO) return;
        tentativas++;
        if (o->orig == i2) {
            d->cel = d->orig = o->orig;
            d->est = E_ACERTO;
            o->est = E_VAZIO;
            acertos++;
            efeito_ok();
            checar_fim();
        } else marcar_erro(0, i1);
        break;
    case T_INTERC:
        if (p1 != 0 || p2 != 0 || i1 == i2) return;
        {
            int c = o->cel, g = o->orig;
            o->cel = d->cel; o->orig = d->orig;
            d->cel = c; d->orig = g;
            tentativas++;
            int antes = acertos;
            contar_lugares();
            if (acertos > antes) efeito_ok(); else efeito_erro();
            checar_fim();
        }
        break;
    case T_BURACO:
        if (p1 != 0 || p2 != 0) return;
        if (d->est == E_VAZIO && o->est != E_VAZIO && vizinhos(&pn[0], i1, i2)) {
            int c = d->cel, g = d->orig;   /* el hueco conserva su identidad (la última pieza) */
            d->cel = o->cel; d->orig = o->orig; d->est = E_NORMAL;
            o->cel = c; o->orig = g; o->est = E_VAZIO;
            bu_buraco = i1;
            tentativas++;
            contar_lugares();
            if (acertos >= pn[0].n - 1 && pn[0].c[pn[0].n - 1].est == E_VAZIO) { acertos = pn[0].n; checar_fim(); }
        }
        break;
    case T_ASOC_S:
    case T_ASOC_C:
        if (p1 == p2) return;
        {
            int ia = p1 == 0 ? i1 : i2, ib = p1 == 0 ? i2 : i1;
            cx_t *a = &pn[0].c[ia], *b = &pn[1].c[ib];
            if (a->est == E_VAZIO || b->est == E_VAZIO) return;
            tentativas++;
            if (asoc_confere(ia, ib)) {
                acertos++;
                a->est = E_VAZIO;
                if (at.tipo == T_ASOC_S) b->est = E_VAZIO;
                else b->est = E_NORMAL;
                efeito_ok();
                checar_fim();
            } else marcar_erro(p1, i1);
        }
        break;
    default: break;
    }
}

/* Un clic (pulsar y soltar en la misma caja). */
static void clique(int p, int i)
{
    if (resolvida && at.tipo != T_INFO && at.tipo != T_EXPLORA) return;
    if (p < 0 || i < 0 || !pn[p].tem) return;
    cx_t *b = &pn[p].c[i];
    if (b->cel >= 0 && b->g && b->g->c[b->cel].som) som_pedido = b->g->c[b->cel].som;
    if (b->cel >= 0 && b->g && b->g->c[b->cel].salto && (at.tipo == T_IDENT || at.tipo == T_INFO || at.tipo == T_EXPLORA)) {
        salto_pedido = b->g->c[b->cel].salto;   /* una pantalla de menú: la celda lleva a otra actividad */
        return;
    }
    switch (at.tipo) {
    case T_DOBLE:
        if (p == 0 && b->est != E_VAZIO) { if (sel_p == 0 && sel_i == i) sel_p = sel_i = -1; else { sel_p = 0; sel_i = i; } }
        else if (p == 1 && sel_p == 0) gesto(0, sel_i, 1, i);
        break;
    case T_INTERC:
        if (sel_p == 0 && sel_i >= 0) { if (sel_i == i) sel_p = sel_i = -1; else gesto(0, sel_i, 0, i); }
        else { sel_p = 0; sel_i = i; }
        break;
    case T_BURACO:
        if (b->est != E_VAZIO) {   /* se mueve al hueco si está al lado */
            if (vizinhos(&pn[0], i, bu_buraco)) gesto(0, i, 0, bu_buraco);
            else efeito_erro();
        }
        break;
    case T_ASOC_S:
    case T_ASOC_C:
        if (b->est == E_VAZIO) break;
        if (sel_p < 0) { sel_p = p; sel_i = i; }
        else if (sel_p == p) { if (sel_i == i) sel_p = sel_i = -1; else sel_i = i; }
        else gesto(sel_p, sel_i, p, i);
        break;
    case T_MEMORIA: {
        if (b->est != E_NORMAL) break;
        if (memo_a >= 0 && memo_b >= 0) break;   /* esperando a que se tapen */
        b->est = E_VIRADO;
        if (memo_a < 0) { memo_a = i; break; }
        memo_b = i;
        tentativas++;
        cx_t *a = &pn[0].c[memo_a];
        if (a->orig == b->orig) {
            a->est = b->est = E_ACERTO;
            memo_a = memo_b = -1;
            acertos++;
            efeito_ok();
            checar_fim();
        } else { memo_ate = tempo_ms() + 900; efeito_erro(); }
        break;
    }
    case T_EXPLORA:
        if (p == 0) {
            exp_a = i;
            exp_b = -1;
            if (pn[1].tem) {
                int id = b->g->c[b->cel].id;
                if (id < 0) id = b->cel;
                for (int k = 0; k < pn[1].n; k++) if (pn[1].c[k].orig == id) exp_b = k;
            }
        }
        break;
    case T_IDENT: {
        if (b->est == E_ACERTO || alvo == 0) break;
        tentativas++;
        if (b->g->c[b->cel].id > 0) { b->est = E_ACERTO; acertos++; efeito_ok(); checar_fim(); }
        else marcar_erro(0, i);
        break;
    }
    default: break;
    }
}

/* El tiempo que pasa: tapar las cartas de la memoria, apagar el parpadeo. Devuelve true si hay que redibujar. */
static bool motor_tempo(void)
{
    bool r = false;
    uint32_t t = tempo_ms();
    if (memo_a >= 0 && memo_b >= 0 && (int)(t - memo_ate) >= 0) {
        pn[0].c[memo_a].est = E_NORMAL;
        pn[0].c[memo_b].est = E_NORMAL;
        memo_a = memo_b = -1;
        r = true;
    }
    if (err_p >= 0 && (int)(t - err_ate) >= 0) { err_p = err_i = -1; r = true; }
    return r;
}

/* ------------------------------------------------------------ sopa de letras */

static bool sp_linea(int x0, int y0, int x1, int y1, int *dx, int *dy, int *n)
{
    int ax = x1 - x0, ay = y1 - y0;
    int aax = ax < 0 ? -ax : ax, aay = ay < 0 ? -ay : ay;
    if (aax != 0 && aay != 0 && aax != aay) return false;
    *dx = ax > 0 ? 1 : ax < 0 ? -1 : 0;
    *dy = ay > 0 ? 1 : ay < 0 ? -1 : 0;
    *n = (aax > aay ? aax : aay) + 1;
    return true;
}

/* Selección de x0,y0 a x1,y1: marca la palabra si es una de la lista. */
static bool sp_soltar(int x0, int y0, int x1, int y1)
{
    int dx, dy, n;
    if (!sp_linea(x0, y0, x1, y1, &dx, &dy, &n) || n < 2) return false;
    char s[TG_MAX + 1];
    for (int k = 0; k < n && k < TG_MAX; k++) s[k] = sp_letras[y0 + dy * k][x0 + dx * k];
    s[n < TG_MAX ? n : TG_MAX] = 0;
    char r[TG_MAX + 1];
    for (int k = 0; k < n && k < TG_MAX; k++) r[k] = s[n - 1 - k];
    r[n < TG_MAX ? n : TG_MAX] = 0;
    tentativas++;
    for (int i = 0; i < at.n_pal; i++) {
        if (sp_ok[i]) continue;
        if (!compara_texto(s, at.pal[i]) || !compara_texto(r, at.pal[i])) {
            sp_ok[i] = true;
            for (int k = 0; k < n; k++) sp_est[y0 + dy * k][x0 + dx * k] = 1;
            acertos++;
            efeito_ok();
            checar_fim();
            return true;
        }
    }
    efeito_erro();
    return false;
}

/* ------------------------------------------------------------ crucigrama */

static bool cz_letra(int x, int y)
{
    return x >= 0 && y >= 0 && x < at.tg_col && y < at.tg_lin && at.tg[y][x] != '*' && at.tg[y][x] != ' ';
}

static void cz_contar(void)
{
    int ok = 0;
    for (int y = 0; y < at.tg_lin; y++)
        for (int x = 0; x < at.tg_col; x++)
            if (cz_letra(x, y) && maiuscula((unsigned char)cz_user[y][x]) == maiuscula((unsigned char)at.tg[y][x])) ok++;
    acertos = ok;
}

static void cz_foco(int x, int y)
{
    if (!cz_letra(x, y)) return;
    if (x == cz_fx && y == cz_fy) cz_dir ^= 1;   /* otro toque cambia el sentido */
    else {
        cz_fx = x; cz_fy = y;
        bool h = cz_letra(x - 1, y) || cz_letra(x + 1, y), v = cz_letra(x, y - 1) || cz_letra(x, y + 1);
        if (!h && v) cz_dir = 1; else if (h && !v) cz_dir = 0;
    }
}

static void cz_escrever(int c)
{
    if (cz_fx < 0 || resolvida) return;
    int dx = cz_dir ? 0 : 1, dy = cz_dir ? 1 : 0;
    if (c == 8) {   /* retroceso */
        if (cz_user[cz_fy][cz_fx] == ' ' && cz_letra(cz_fx - dx, cz_fy - dy)) { cz_fx -= dx; cz_fy -= dy; }
        cz_user[cz_fy][cz_fx] = ' ';
        cz_contar();
        return;
    }
    if (c < 32 || c == 127) return;
    cz_user[cz_fy][cz_fx] = (char)maiuscula(c);
    tentativas++;
    if (cz_letra(cz_fx + dx, cz_fy + dy)) { cz_fx += dx; cz_fy += dy; }
    cz_contar();
    checar_fim();
}

/* ------------------------------------------------------------ resolución automática (--autoprueba) */

static int auto_buscar_b(int orig)
{
    for (int i = 0; i < pn[1].n; i++) if (pn[1].c[i].orig == orig) return i;
    return -1;
}

static void auto_resolver(void)
{
    switch (at.tipo) {
    case T_DOBLE:
        for (int i = 0; i < pn[0].n; i++)
            for (int k = 0; k < pn[0].n; k++) if (pn[0].c[k].est != E_VAZIO && pn[0].c[k].orig == i) { gesto(0, k, 1, i); break; }
        break;
    case T_INTERC:
        for (int i = 0; i < pn[0].n; i++)
            for (int k = i + 1; k < pn[0].n; k++) if (pn[0].c[k].orig == i && pn[0].c[i].orig != i) { gesto(0, k, 0, i); break; }
        break;
    case T_BURACO:
        for (int k = bu_n_hist - 1; k >= 0; k--) clique(0, bu_hist[k]);
        break;
    case T_ASOC_S:
        for (int i = 0; i < pn[0].n; i++) { int j = auto_buscar_b(pn[0].c[i].orig); if (j >= 0) gesto(0, i, 1, j); }
        break;
    case T_ASOC_C:
        for (int i = 0; i < pn[0].n; i++) {
            cx_t *a = &pn[0].c[i];
            int id = a->g->c[a->cel].id;
            if (id < 0 && alvo == pn[0].n) id = a->cel;
            if (id < 0) continue;
            int j = auto_buscar_b(id);
            if (j >= 0) gesto(0, i, 1, j);
        }
        break;
    case T_MEMORIA:
        for (int i = 0; i < pn[0].n; i++) {
            if (pn[0].c[i].est != E_NORMAL) continue;
            for (int k = i + 1; k < pn[0].n; k++)
                if (pn[0].c[k].est == E_NORMAL && pn[0].c[k].orig == pn[0].c[i].orig) { clique(0, i); clique(0, k); break; }
        }
        break;
    case T_IDENT:
        for (int i = 0; i < pn[0].n; i++) { cx_t *b = &pn[0].c[i]; if (b->g->c[b->cel].id > 0) clique(0, i); }
        break;
    case T_EXPLORA:
        for (int i = 0; i < pn[0].n; i++) clique(0, i);
        break;
    case T_SOPA:
        for (int w = 0; w < at.n_pal; w++) {
            int n = (int)tam_texto(at.pal[w]);
            bool ok = false;
            for (int y = 0; y < at.tg_lin && !ok; y++)
                for (int x = 0; x < at.tg_col && !ok; x++)
                    for (int dy = -1; dy <= 1 && !ok; dy++)
                        for (int dx = -1; dx <= 1 && !ok; dx++) {
                            if (!dx && !dy) continue;
                            int xe = x + dx * (n - 1), ye = y + dy * (n - 1);
                            if (xe < 0 || ye < 0 || xe >= at.tg_col || ye >= at.tg_lin) continue;
                            int k = 0;
                            while (k < n && sp_letras[y + dy * k][x + dx * k] == at.pal[w][k]) k++;
                            if (k == n) { sp_soltar(x, y, xe, ye); ok = true; }
                        }
        }
        break;
    case T_CRUZ:
        for (int y = 0; y < at.tg_lin; y++)
            for (int x = 0; x < at.tg_col; x++)
                if (cz_letra(x, y)) { cz_fx = x; cz_fy = y; cz_escrever(at.tg[y][x]); }
        break;
    default: break;
    }
}

static bool tipo_se_resolve(int t)
{
    if (t == T_IDENT && alvo == 0) return false;
    return t != T_INFO && t != T_EXPLORA && t != T_TEXTO && t != T_OUTRO && t != T_NADA;
}
#endif
