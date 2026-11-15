// SPDX-License-Identifier: GPL-2.0-or-later
/* edicao_rica.c - control de edición de texto con formato (mejora 116, D291).
 *
 * El documento es un texto Latin-1 (párrafos separados por '\n'; '\f' = salto de página), un
 * índice de formato por carácter (tabla de hasta 256 formatos distintos: fuente, tamaño,
 * N/C/S/T y color) y los atributos de cada párrafo. La maquetación se hace en TWIPS
 * (1/20 de punto) con los anchos de Helvetica/Times/Courier (pdf_metricas.h): es la misma que
 * usa la exportación a PDF, así que lo que se ve en pantalla se pagina igual en el PDF.
 * En pantalla se dibuja con la fuente de mapa de bits 8x16 escalada con promediado (la
 * libsagui no usa coma flotante ni enlaza la libfontes).
 * Deshacer/rehacer por instantáneas del documento (texto + formatos + párrafos). */
#include "gui.h"
#include "pdf_escribir.h"

extern const unsigned char fonte_sagui[256][16];

#define GAP_PX     10     /* separación entre hojas */
#define MARGEM_PX  8      /* margen del fondo gris alrededor de la hoja */
#define BARRA_L    16     /* ancho de las barras de desplazamiento */
#define UNDO_MAX   48
#define UNDO_BYTES (6 * 1024 * 1024)

typedef struct { char *t; uint8_t *at; int n; rte_par_t *par; int npar; int cur; } snap_t;

typedef struct {
    int ini, fim;       /* texto [ini, fim): sin el '\n' final del párrafo */
    int x0;             /* inicio del texto respecto al margen izquierdo (twips), con sangría y alineación */
    int y, alt, base;   /* respecto al borde superior del área de texto de la hoja */
    int pag, par;
    int extra;          /* twips que se suman a cada espacio (justificado) */
    bool prim, marca;   /* primera línea del párrafo; lleva viñeta */
    bool fin_par;       /* última línea del párrafo */
} linha_t;

struct rte {
    int x, y, l, a;
    char *t; uint8_t *at; int n, cap;
    rte_fmt_t fmts[256]; int nfmt;
    rte_par_t *par; int npar, cap_par;
    int pw, ph, m_esq, m_dir, m_sup, m_inf;
    int zoom;
    int cur, anc;
    bool foco, arrastando, modificado;
    rte_fmt_t fmt_ins;
    int topo, esq_px;
    int col_pref;                 /* x preferido (twips) al moverse en vertical; -1 = recalcular */
    linha_t *lin; int nlin, cap_lin; bool sujo; int npag;
    snap_t und[UNDO_MAX]; int nund;
    snap_t red[UNDO_MAX]; int nred;
    int ult_tipo, ult_pos;
    int drag_barra, drag_off;     /* 1 = barra vertical, 2 = horizontal */
    int vis_l, vis_a;             /* área de texto sin las barras */
    bool hbarra;
};

/* ------------------------------------------------------------ utilidades */

static const rte_par_t PAR_PADRAO = { RTE_ESQ, 0, 0, 0, 0, 100, 0, 120 };

static int tw2px(const rte_t *r, int v)
{
    long p = (long)v * r->zoom;
    return (int)((p + (p >= 0 ? 750 : -750)) / 1500);
}

static int em_px(const rte_t *r, int tam) { return (tam * r->zoom + 37) / 75; }

static int fmt_id(rte_t *r, const rte_fmt_t *f)
{
    for (int i = 0; i < r->nfmt; i++) {
        const rte_fmt_t *g = &r->fmts[i];
        if (g->fam == f->fam && g->est == f->est && g->tam == f->tam && g->cor == f->cor) return i;
    }
    if (r->nfmt < 256) { r->fmts[r->nfmt] = *f; return r->nfmt++; }
    /* tabla llena: el más parecido (misma fuente y tamaño) */
    for (int i = 0; i < r->nfmt; i++)
        if (r->fmts[i].fam == f->fam && r->fmts[i].tam == f->tam) return i;
    return 0;
}

static bool garantir(rte_t *r, int precisa)
{
    if (precisa <= r->cap) return true;
    int nc = r->cap ? r->cap : 4096;
    while (nc < precisa) nc *= 2;
    char *t = realoca(r->t, (size_t)nc);
    if (!t) return false;
    r->t = t;
    uint8_t *a = realoca(r->at, (size_t)nc);
    if (!a) return false;
    r->at = a;
    r->cap = nc;
    return true;
}

static bool garantir_par(rte_t *r, int precisa)
{
    if (precisa <= r->cap_par) return true;
    int nc = r->cap_par ? r->cap_par : 64;
    while (nc < precisa) nc *= 2;
    rte_par_t *p = realoca(r->par, (size_t)nc * sizeof *p);
    if (!p) return false;
    r->par = p;
    r->cap_par = nc;
    return true;
}

static int par_idx(const rte_t *r, int pos)
{
    int p = 0;
    for (int i = 0; i < pos && i < r->n; i++) if (r->t[i] == '\n') p++;
    return p;
}

/* ------------------------------------------------------------ instantáneas (deshacer) */

static void snap_liberar(snap_t *s) { libera(s->t); libera(s->at); libera(s->par); memset(s, 0, sizeof *s); }

static bool snap_tomar(const rte_t *r, snap_t *s)
{
    memset(s, 0, sizeof *s);
    s->t = aloca((size_t)r->n + 1);
    s->at = aloca((size_t)r->n + 1);
    s->par = aloca((size_t)r->npar * sizeof(rte_par_t) + 1);
    if (!s->t || !s->at || !s->par) { snap_liberar(s); return false; }
    memcpy(s->t, r->t, (size_t)r->n);
    memcpy(s->at, r->at, (size_t)r->n);
    memcpy(s->par, r->par, (size_t)r->npar * sizeof(rte_par_t));
    s->n = r->n; s->npar = r->npar; s->cur = r->cur;
    return true;
}

static void snap_poner(rte_t *r, const snap_t *s)
{
    garantir(r, s->n + 1);
    garantir_par(r, s->npar + 1);
    memcpy(r->t, s->t, (size_t)s->n);
    memcpy(r->at, s->at, (size_t)s->n);
    memcpy(r->par, s->par, (size_t)s->npar * sizeof(rte_par_t));
    r->n = s->n; r->npar = s->npar; r->cur = s->cur; r->anc = -1;
    r->sujo = true;
}

static long und_bytes(const rte_t *r)
{
    long b = 0;
    for (int i = 0; i < r->nund; i++) b += r->und[i].n * 2L;
    for (int i = 0; i < r->nred; i++) b += r->red[i].n * 2L;
    return b;
}

static void red_limpar(rte_t *r) { for (int i = 0; i < r->nred; i++) snap_liberar(&r->red[i]); r->nred = 0; }
static void und_limpar(rte_t *r) { for (int i = 0; i < r->nund; i++) snap_liberar(&r->und[i]); r->nund = 0; }

/* Antes de cambiar el documento. tipo: 1 = escribir (se agrupa con lo anterior), 0 = otra cosa. */
static void marcar_undo(rte_t *r, int tipo)
{
    r->modificado = true;
    if (tipo == 1 && r->ult_tipo == 1 && r->ult_pos == r->cur && r->nund > 0) { return; }
    r->ult_tipo = tipo;
    red_limpar(r);
    if (r->nund == UNDO_MAX || und_bytes(r) > UNDO_BYTES) {
        snap_liberar(&r->und[0]);
        memmove(&r->und[0], &r->und[1], (size_t)(r->nund - 1) * sizeof(snap_t));
        r->nund--;
    }
    if (snap_tomar(r, &r->und[r->nund])) r->nund++;
}

bool rte_pode_desfazer(const rte_t *r) { return r->nund > 0; }
bool rte_pode_refazer(const rte_t *r) { return r->nred > 0; }

static void fmt_sincronizar(rte_t *r)
{
    if (r->cur > 0 && r->t[r->cur - 1] != '\n') r->fmt_ins = r->fmts[r->at[r->cur - 1]];
    else if (r->cur < r->n) r->fmt_ins = r->fmts[r->at[r->cur]];
}

bool rte_desfazer(rte_t *r)
{
    if (r->nund == 0) return false;
    if (r->nred < UNDO_MAX && snap_tomar(r, &r->red[r->nred])) r->nred++;
    snap_t *s = &r->und[--r->nund];
    snap_poner(r, s);
    snap_liberar(s);
    r->ult_tipo = 0;
    r->modificado = true;
    fmt_sincronizar(r);
    return true;
}

bool rte_refazer(rte_t *r)
{
    if (r->nred == 0) return false;
    if (r->nund < UNDO_MAX && snap_tomar(r, &r->und[r->nund])) r->nund++;
    snap_t *s = &r->red[--r->nred];
    snap_poner(r, s);
    snap_liberar(s);
    r->ult_tipo = 0;
    r->modificado = true;
    fmt_sincronizar(r);
    return true;
}

/* ------------------------------------------------------------ primitivas de edición */

static bool ins_raw(rte_t *r, int pos, const char *t, const uint8_t *at, int n, int fid)
{
    if (n <= 0) return true;
    int nl = 0;
    for (int i = 0; i < n; i++) if (t[i] == '\n') nl++;
    if (!garantir(r, r->n + n + 1) || !garantir_par(r, r->npar + nl + 1)) return false;
    int p = pos >= r->n ? r->npar - 1 : par_idx(r, pos);
    memmove(r->t + pos + n, r->t + pos, (size_t)(r->n - pos));
    memmove(r->at + pos + n, r->at + pos, (size_t)(r->n - pos));
    memcpy(r->t + pos, t, (size_t)n);
    if (at) memcpy(r->at + pos, at, (size_t)n); else memset(r->at + pos, fid, (size_t)n);
    r->n += n;
    if (nl) {
        memmove(r->par + p + 1 + nl, r->par + p + 1, (size_t)(r->npar - p - 1) * sizeof(rte_par_t));
        for (int k = 1; k <= nl; k++) r->par[p + k] = r->par[p];
        r->npar += nl;
    }
    r->sujo = true;
    return true;
}

static void del_raw(rte_t *r, int a, int b)
{
    if (b > r->n) b = r->n;
    if (a >= b) return;
    int nl = 0;
    for (int i = a; i < b; i++) if (r->t[i] == '\n') nl++;
    int p = par_idx(r, a);
    memmove(r->t + a, r->t + b, (size_t)(r->n - b));
    memmove(r->at + a, r->at + b, (size_t)(r->n - b));
    r->n -= b - a;
    if (nl) {
        memmove(r->par + p + 1, r->par + p + 1 + nl, (size_t)(r->npar - p - 1 - nl) * sizeof(rte_par_t));
        r->npar -= nl;
    }
    r->sujo = true;
}

static bool sel_rango(const rte_t *r, int *a, int *b)
{
    if (r->anc < 0 || r->anc == r->cur) return false;
    *a = r->anc < r->cur ? r->anc : r->cur;
    *b = r->anc < r->cur ? r->cur : r->anc;
    if (*b > r->n) *b = r->n;
    return *a < *b;
}

bool rte_selecao(const rte_t *r, int *a, int *b) { int x, y; if (!sel_rango(r, &x, &y)) return false; if (a) *a = x; if (b) *b = y; return true; }

static bool apagar_sel(rte_t *r)
{
    int a, b;
    if (!sel_rango(r, &a, &b)) { r->anc = -1; return false; }
    del_raw(r, a, b);
    r->cur = a; r->anc = -1;
    return true;
}

/* ------------------------------------------------------------ creación */

static void iniciar_doc(rte_t *r)
{
    r->n = 0; r->npar = 1; r->par[0] = PAR_PADRAO;
    r->cur = 0; r->anc = -1; r->topo = 0; r->esq_px = 0; r->sujo = true;
    r->col_pref = -1;
}

rte_t *rte_criar(int x, int y, int l, int a)
{
    rte_t *r = aloca_zerado(sizeof *r);
    if (!r) return NULL;
    r->x = x; r->y = y; r->l = l; r->a = a;
    r->zoom = 100;
    r->pw = 11906; r->ph = 16838; r->m_esq = r->m_dir = r->m_sup = r->m_inf = 1417;
    r->fmt_ins.fam = RTE_SERIF; r->fmt_ins.tam = 12; r->fmt_ins.cor = 0;
    fmt_id(r, &r->fmt_ins);
    if (!garantir(r, 4096) || !garantir_par(r, 64)) { rte_liberar(r); return NULL; }
    iniciar_doc(r);
    r->foco = true;
    return r;
}

void rte_liberar(rte_t *r)
{
    if (!r) return;
    und_limpar(r); red_limpar(r);
    libera(r->t); libera(r->at); libera(r->par); libera(r->lin);
    libera(r);
}

void rte_mover(rte_t *r, int x, int y, int l, int a) { r->x = x; r->y = y; r->l = l; r->a = a; }

void rte_limpar(rte_t *r)
{
    und_limpar(r); red_limpar(r);
    iniciar_doc(r);
    r->modificado = false; r->ult_tipo = 0;
}

void rte_papel(rte_t *r, int pw, int ph, int me, int md, int ms, int mi)
{
    r->pw = pw; r->ph = ph; r->m_esq = me; r->m_dir = md; r->m_sup = ms; r->m_inf = mi; r->sujo = true;
}
void rte_papel_obter(const rte_t *r, int *pw, int *ph, int *me, int *md, int *ms, int *mi)
{
    if (pw) *pw = r->pw;
    if (ph) *ph = r->ph;
    if (me) *me = r->m_esq;
    if (md) *md = r->m_dir;
    if (ms) *ms = r->m_sup;
    if (mi) *mi = r->m_inf;
}

void rte_zoom(rte_t *r, int z) { r->zoom = z < 25 ? 25 : z > 400 ? 400 : z; }
int  rte_zoom_obter(const rte_t *r) { return r->zoom; }
void rte_foco(rte_t *r, bool f) { r->foco = f; }
bool rte_modificado(const rte_t *r) { return r->modificado; }
void rte_marcar_limpo(rte_t *r) { r->modificado = false; }
int  rte_tamanho(const rte_t *r) { return r->n; }
const char *rte_texto(const rte_t *r) { return r->t; }
int  rte_cursor(const rte_t *r) { return r->cur; }
int  rte_n_par(const rte_t *r) { return r->npar; }

void rte_doc_anexar(rte_t *r, const char *t, int n, const rte_fmt_t *f)
{
    int id = fmt_id(r, f);
    ins_raw(r, r->n, t, NULL, n, id);
}

void rte_doc_par(rte_t *r, const rte_par_t *p) { r->par[r->npar - 1] = *p; r->sujo = true; }

void rte_doc_fim(rte_t *r)
{
    if (r->n > 0 && r->t[r->n - 1] == '\n') del_raw(r, r->n - 1, r->n);   /* el último \par no abre otro párrafo */
    r->cur = 0; r->anc = -1; r->sujo = true; r->modificado = false;
    und_limpar(r); red_limpar(r);
    fmt_sincronizar(r);
}

bool rte_par_dados(const rte_t *r, int i, int *ini, int *fim, rte_par_t *p)
{
    if (i < 0 || i >= r->npar) return false;
    int k = 0, pos = 0;
    while (k < i) { while (pos < r->n && r->t[pos] != '\n') pos++; pos++; k++; }
    int e = pos;
    while (e < r->n && r->t[e] != '\n') e++;
    if (ini) *ini = pos;
    if (fim) *fim = e;
    if (p) *p = r->par[i];
    return true;
}

void rte_fmt_em(const rte_t *r, int pos, rte_fmt_t *f)
{
    if (pos < 0) pos = 0;
    if (pos >= r->n) { *f = r->fmt_ins; return; }
    *f = r->fmts[r->at[pos]];
}

/* ------------------------------------------------------------ maquetación */

static int larg_char_tw(const rte_t *r, int i)
{
    unsigned char c = (unsigned char)r->t[i];
    if (c < 32) return 0;
    const rte_fmt_t *f = &r->fmts[r->at[i]];
    return (pdfw_largura_char(f->fam, f->est, c) * f->tam + 25) / 50;
}

/* Avance de un carácter con el tabulador en una columna de 720 twips; xrel respecto al margen. */
static int avance(const rte_t *r, int i, int xrel)
{
    if (r->t[i] == '\t') return 720 - (xrel % 720);
    return larg_char_tw(r, i);
}

static bool linhas_garantir(rte_t *r)
{
    if (r->nlin < r->cap_lin) return true;
    int nc = r->cap_lin ? r->cap_lin * 2 : 256;
    linha_t *l = realoca(r->lin, (size_t)nc * sizeof *l);
    if (!l) return false;
    r->lin = l; r->cap_lin = nc;
    return true;
}

/* Altura y base de una línea (twips) para el rango [a, b) con el tamaño máximo. */
static void altura_linha(const rte_t *r, int a, int b, int vazio_fmt, int *alt, int *base)
{
    int mx = 0;
    for (int i = a; i < b; i++) {
        int tm = r->t[i] == '\f' ? 0 : r->fmts[r->at[i]].tam;
        if (tm > mx) mx = tm;
    }
    if (mx == 0) mx = r->fmts[vazio_fmt].tam;
    if (mx == 0) mx = 12;
    *alt = mx * 23;          /* 1,15 x el cuerpo, en twips */
    *base = mx * 18;         /* 0,9 x el cuerpo sobre la línea base */
}

static void maquetar(rte_t *r)
{
    if (!r->sujo) return;
    r->sujo = false;
    r->nlin = 0;
    int pag = 0, ycur = 0, area_a = r->ph - r->m_sup - r->m_inf;
    int area_l = r->pw - r->m_esq - r->m_dir;
    if (area_a < 400) area_a = 400;
    if (area_l < 400) area_l = 400;
    bool forcar = false;
    int pos = 0;
    for (int pi = 0; pi < r->npar; pi++) {
        const rte_par_t *pr = &r->par[pi];
        int pe = pos;
        while (pe < r->n && r->t[pe] != '\n') pe++;
        int marca = pr->marcador != 0;
        bool prim = true;
        int ls = pos;
        if (pr->antes && ycur > 0) ycur += pr->antes;
        for (;;) {
            int sang = pr->esq + (prim ? pr->prim : 0);
            if (marca && prim) sang = pr->esq;   /* con viñeta, el texto de la 1ª línea arranca en la sangría izquierda */
            if (sang < 0) sang = 0;
            int disp = area_l - sang - pr->dir;
            if (disp < 200) disp = 200;
            int w = 0, le = ls, ult_esp = -1, ult_w = 0;
            bool duro = false;
            int i = ls;
            for (; i < pe; i++) {
                if (r->t[i] == '\f') { le = i + 1; duro = true; break; }
                int cw = avance(r, i, sang + w);
                if (w + cw > disp && i > ls && r->t[i] != ' ') {
                    if (ult_esp > ls) { le = ult_esp; w = ult_w; }
                    else le = i;
                    break;
                }
                w += cw;
                if (r->t[i] == ' ') { ult_esp = i + 1; ult_w = w; }
                le = i + 1;
            }
            if (i >= pe && !duro) le = pe;
            bool fin = (le >= pe);
            /* ancho visible (sin los espacios finales) */
            int ve = le;
            while (ve > ls && r->t[ve - 1] == ' ' && !(fin && false)) ve--;
            int wv = 0;
            for (int k = ls; k < ve; k++) wv += avance(r, k, sang + wv);
            int alt, base;
            int vf = (pe < r->n) ? r->at[pe] : fmt_id(r, &r->fmt_ins);
            altura_linha(r, ls, le, vf, &alt, &base);
            if (pr->interlin > 100) { int ex = alt * (pr->interlin - 100) / 100; alt += ex; base += ex; }
            if (forcar || (ycur + alt > area_a && ycur > 0)) { pag++; ycur = 0; forcar = false; }
            if (!linhas_garantir(r)) return;
            linha_t *ln = &r->lin[r->nlin++];
            ln->ini = ls; ln->fim = le; ln->y = ycur; ln->alt = alt; ln->base = base;
            ln->pag = pag; ln->par = pi; ln->prim = prim; ln->marca = marca && prim; ln->fin_par = fin;
            ln->extra = 0;
            int x0 = sang;
            if (pr->alin == RTE_CENTRO) x0 += (disp - wv) / 2;
            else if (pr->alin == RTE_DIR) x0 += disp - wv;
            else if (pr->alin == RTE_JUST && !fin && !duro) {
                int esp = 0;
                for (int k = ls; k < ve; k++) if (r->t[k] == ' ') esp++;
                if (esp > 0 && disp > wv) ln->extra = (disp - wv) / esp;
            }
            ln->x0 = x0;
            ycur += alt;
            if (duro) forcar = true;
            if (fin) break;
            ls = le;
            prim = false;
            if (duro) { /* después del salto de página sigue el mismo párrafo */ }
        }
        if (pr->depois) ycur += pr->depois;
        pos = pe + 1;
    }
    r->npag = pag + 1;
}

static int linha_de(rte_t *r, int pos)
{
    maquetar(r);
    int lo = 0, hi = r->nlin - 1, res = 0;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (r->lin[m].ini <= pos) { res = m; lo = m + 1; } else hi = m - 1;
    }
    return res;
}

/* x (twips, respecto al margen) de la posición pos en la línea ln. */
static int x_de(const rte_t *r, const linha_t *ln, int pos)
{
    int x = ln->x0;
    for (int i = ln->ini; i < pos && i < ln->fim; i++) {
        x += avance(r, i, x);
        if (r->t[i] == ' ') x += ln->extra;
    }
    return x;
}

/* Posición de la línea más cercana al x dado (twips respecto al margen). */
static int pos_en_x(const rte_t *r, const linha_t *ln, int xt)
{
    int x = ln->x0, fim = ln->fim;
    if (ln->fin_par == false) { /* en una línea partida no se puede dejar el cursor tras el último espacio */ }
    for (int i = ln->ini; i < fim; i++) {
        if (r->t[i] == '\f') return i;
        int a = avance(r, i, x) + (r->t[i] == ' ' ? ln->extra : 0);
        if (xt < x + a / 2) return i;
        x += a;
    }
    return fim > ln->ini && r->t[fim - 1] == ' ' && !ln->fin_par ? fim - 1 : fim;
}

int rte_paginas(rte_t *r) { maquetar(r); return r->npag; }

int rte_pagina_atual(rte_t *r)
{
    maquetar(r);
    return r->lin[linha_de(r, r->cur)].pag + 1;
}

void rte_linha_coluna(rte_t *r, int *lin, int *col)
{
    maquetar(r);
    int li = linha_de(r, r->cur);
    /* línea del documento (cuenta cada línea maquetada de la página) y columna (1-based) */
    if (lin) *lin = li + 1;
    if (col) *col = r->cur - r->lin[li].ini + 1;
}

/* ------------------------------------------------------------ geometría en pantalla */

static int pag_alt_px(const rte_t *r) { return tw2px(r, r->ph); }
static int pag_larg_px(const rte_t *r) { return tw2px(r, r->pw); }

static int doc_alt_px(rte_t *r)
{
    maquetar(r);
    return r->npag * (pag_alt_px(r) + GAP_PX) + 2 * MARGEM_PX - GAP_PX;
}

static void calc_vista(rte_t *r)
{
    int pl = pag_larg_px(r) + 2 * MARGEM_PX;
    r->hbarra = pl > r->l - BARRA_L;
    r->vis_l = r->l - BARRA_L;
    r->vis_a = r->a - (r->hbarra ? BARRA_L : 0);
    int dh = doc_alt_px(r);
    int max = dh - r->vis_a; if (max < 0) max = 0;
    if (r->topo > max) r->topo = max;
    if (r->topo < 0) r->topo = 0;
    int hmax = pl - r->vis_l; if (hmax < 0) hmax = 0;
    if (r->esq_px > hmax) r->esq_px = hmax;
    if (r->esq_px < 0) r->esq_px = 0;
}

static int pagina_x(rte_t *r)
{
    int pl = pag_larg_px(r);
    int ox = r->x + (r->vis_l - pl) / 2;
    if (ox < r->x + MARGEM_PX) ox = r->x + MARGEM_PX;
    return ox - r->esq_px;
}

int rte_pagina_x(rte_t *r) { calc_vista(r); return pagina_x(r); }

static int pagina_y(const rte_t *r, int p) { return r->y + MARGEM_PX + p * (pag_alt_px(r) + GAP_PX) - r->topo; }

/* y (px, ventana) del borde superior de una línea y su altura */
static int linha_y_px(rte_t *r, const linha_t *ln) { return pagina_y(r, ln->pag) + tw2px(r, r->m_sup + ln->y); }

static void cursor_visivel(rte_t *r)
{
    calc_vista(r);
    int li = linha_de(r, r->cur);
    const linha_t *ln = &r->lin[li];
    int y = linha_y_px(r, ln) - r->y, h = tw2px(r, ln->alt) + 2;
    if (y < 0) r->topo += y - 4;
    else if (y + h > r->vis_a) r->topo += y + h - r->vis_a + 4;
    int x = pagina_x(r) - r->x + tw2px(r, r->m_esq + x_de(r, ln, r->cur));
    if (x < 8) r->esq_px += x - 16;
    else if (x > r->vis_l - 8) r->esq_px += x - r->vis_l + 16;
    calc_vista(r);
}

/* ------------------------------------------------------------ dibujo de glifos */

static int clip_x0, clip_y0, clip_x1, clip_y1;

static void mezclar(jan_t *j, int x, int y, uint32_t cor, int cnt, int total)
{
    if (x < clip_x0 || y < clip_y0 || x >= clip_x1 || y >= clip_y1 || x < 0 || y < 0 || x >= j->larg || y >= j->alt) return;
    uint32_t *p = &j->px[y * j->larg + x];
    if (cnt >= total) { *p = cor; return; }
    uint32_t d = *p;
    int dr = (int)((d >> 16) & 255), dg = (int)((d >> 8) & 255), db = (int)(d & 255);
    int cr = (int)((cor >> 16) & 255), cg = (int)((cor >> 8) & 255), cb = (int)(cor & 255);
    dr += (cr - dr) * cnt / total; dg += (cg - dg) * cnt / total; db += (cb - db) * cnt / total;
    *p = ((uint32_t)dr << 16) | ((uint32_t)dg << 8) | (uint32_t)db;
}

static void retang_clip(jan_t *j, int x, int y, int l, int a, uint32_t cor)
{
    if (l <= 0 || a <= 0) return;
    int x1 = x + l, y1 = y + a;
    if (x < clip_x0) x = clip_x0;
    if (y < clip_y0) y = clip_y0;
    if (x1 > clip_x1) x1 = clip_x1;
    if (y1 > clip_y1) y1 = clip_y1;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > j->larg) x1 = j->larg;
    if (y1 > j->alt) y1 = j->alt;
    for (int yy = y; yy < y1; yy++)
        for (int xx = x; xx < x1; xx++) j->px[yy * j->larg + xx] = cor;
}

/* Un glifo de la fuente 8x16 escalado a (adv x altura) con promediado de 2x2 muestras. */
static void glifo(jan_t *j, int x, int ybase, int adv, int em, unsigned char c, int est, uint32_t cor)
{
    if (c <= ' ' || c == 0xA0) return;
    int ch = em + em / 8; if (ch < 4) ch = 4;
    int cw = adv < 2 ? 2 : adv;
    int top = ybase - (ch * 13 + 8) / 16;
    unsigned char fila[16];
    const unsigned char *g = fonte_sagui[c];
    for (int k = 0; k < 16; k++) fila[k] = (est & RTE_NEGRITA) ? (unsigned char)(g[k] | (g[k] >> 1)) : g[k];
    for (int dy = 0; dy < ch; dy++) {
        int sy0 = (2 * dy) * 16 / (2 * ch), sy1 = (2 * dy + 1) * 16 / (2 * ch);
        int desl = (est & RTE_CURSIVA) ? (ybase - (top + dy)) / 5 : 0;
        for (int dx = 0; dx < cw; dx++) {
            int sx0 = (2 * dx) * 8 / (2 * cw), sx1 = (2 * dx + 1) * 8 / (2 * cw);
            int cnt = ((fila[sy0] >> (7 - sx0)) & 1) + ((fila[sy0] >> (7 - sx1)) & 1) +
                      ((fila[sy1] >> (7 - sx0)) & 1) + ((fila[sy1] >> (7 - sx1)) & 1);
            if (cnt) mezclar(j, x + dx + desl, top + dy, cor, cnt, 4);
        }
    }
}

void rte_amostra(jan_t *j, int x, int y, int l, int a, const rte_fmt_t *f, const char *texto)
{
    clip_x0 = x; clip_y0 = y; clip_x1 = x + l; clip_y1 = y + a;
    int em = f->tam * 4 / 3;   /* 96 ppp, zoom 100 % */
    int xx = x + 4, base = y + a / 2 + em / 3;
    for (const char *p = texto; *p; p++) {
        int adv = (pdfw_largura_char(f->fam, f->est, (unsigned char)*p) * em + 500) / 1000;
        glifo(j, xx, base, adv, em, (unsigned char)*p, f->est, f->cor);
        if (f->est & RTE_SUBLINHADO) retang_clip(j, xx, base + 1, adv, 1, f->cor);
        if (f->est & RTE_RISCADO) retang_clip(j, xx, base - em / 3, adv, 1, f->cor);
        xx += adv;
    }
}

/* ------------------------------------------------------------ dibujo */

static void barra_desenhar(jan_t *j, int x, int y, int l, int a, bool vertical, int total, int vis, int pos)
{
    g_retangulo(j, x, y, l, a, 0xE0E0E0);
    int b = BARRA_L;
    /* botones de flecha */
    int fim = vertical ? y + a - b : x + l - b;
    for (int k = 0; k < 2; k++) {
        int bx = vertical ? x : (k ? fim : x), by = vertical ? (k ? fim : y) : y;
        g_retangulo(j, bx, by, b, b, W98_FACE);
        g_bisel(j, bx, by, b, b, false);
        g_flecha(j, bx + 4, by + 6, vertical ? (k ? 1 : 0) : (k ? 3 : 2), W98_NEGRO);
    }
    int trilho = (vertical ? a : l) - 2 * b;
    if (trilho < 8) return;
    int pol = total <= vis ? trilho : trilho * vis / total;
    if (pol < 12) pol = 12;
    if (pol > trilho) pol = trilho;
    int max = total - vis;
    int off = max <= 0 ? 0 : (trilho - pol) * pos / max;
    int px = vertical ? x : x + b + off, py = vertical ? y + b + off : y;
    g_retangulo(j, px, py, vertical ? b : pol, vertical ? pol : b, W98_FACE);
    g_bisel(j, px, py, vertical ? b : pol, vertical ? pol : b, false);
}

void rte_desenhar(jan_t *j, rte_t *r)
{
    maquetar(r);
    calc_vista(r);
    clip_x0 = r->x; clip_y0 = r->y; clip_x1 = r->x + r->vis_l; clip_y1 = r->y + r->vis_a;
    retang_clip(j, r->x, r->y, r->vis_l, r->vis_a, W98_SOMBRA);
    int ox = pagina_x(r), pl = pag_larg_px(r), ph = pag_alt_px(r);
    int s0 = 0, s1 = 0;
    bool tem_sel = sel_rango(r, &s0, &s1);
    /* hojas */
    for (int p = 0; p < r->npag; p++) {
        int py = pagina_y(r, p);
        if (py + ph < r->y || py > r->y + r->vis_a) continue;
        retang_clip(j, ox + 3, py + 3, pl, ph, W98_NEGRO);        /* sombra */
        retang_clip(j, ox, py, pl, ph, COR_BRANCO);
        retang_clip(j, ox - 1, py - 1, pl + 2, 1, W98_NEGRO);
        retang_clip(j, ox - 1, py + ph, pl + 2, 1, W98_NEGRO);
        retang_clip(j, ox - 1, py - 1, 1, ph + 2, W98_NEGRO);
        retang_clip(j, ox + pl, py - 1, 1, ph + 2, W98_NEGRO);
    }
    for (int li = 0; li < r->nlin; li++) {
        const linha_t *ln = &r->lin[li];
        int ly = linha_y_px(r, ln), lh = tw2px(r, ln->alt);
        if (ly + lh < r->y) continue;
        if (ly > r->y + r->vis_a) break;
        int base = ly + tw2px(r, ln->base);
        int xt = ln->x0, xp = ox + tw2px(r, r->m_esq + xt);
        /* viñeta */
        if (ln->marca) {
            const rte_par_t *pr = &r->par[ln->par];
            int bx = ox + tw2px(r, r->m_esq + (pr->esq + pr->prim < 0 ? 0 : pr->esq + pr->prim));
            int em = em_px(r, ln->fim > ln->ini ? r->fmts[r->at[ln->ini]].tam : r->fmt_ins.tam);
            int d = em / 4 > 2 ? em / 4 : 2;
            retang_clip(j, bx + 1, base - em / 3 - d / 2, d, d, COR_TEXTO);
        }
        /* fondo de selección */
        if (tem_sel && s0 <= ln->fim && s1 > ln->ini) {
            int a0 = s0 > ln->ini ? s0 : ln->ini, a1 = s1 < ln->fim ? s1 : ln->fim;
            int xa = ox + tw2px(r, r->m_esq + x_de(r, ln, a0));
            int xb = ox + tw2px(r, r->m_esq + x_de(r, ln, a1));
            if (s1 > ln->fim && ln->fin_par) xb += 6;
            if (a1 <= a0 && !(s1 > ln->fim && ln->fin_par)) xb = xa;
            retang_clip(j, xa, ly, xb - xa, lh, W98_AZUL);
        }
        /* texto */
        int xc = ln->x0;
        for (int i = ln->ini; i < ln->fim; i++) {
            int adv_t = avance(r, i, xc) + (r->t[i] == ' ' ? ln->extra : 0);
            int xa = ox + tw2px(r, r->m_esq + xc), xb = ox + tw2px(r, r->m_esq + xc + adv_t);
            (void)xp;
            const rte_fmt_t *f = &r->fmts[r->at[i]];
            unsigned char c = (unsigned char)r->t[i];
            bool sel = tem_sel && i >= s0 && i < s1;
            uint32_t cor = sel ? COR_BRANCO : f->cor;
            int em = em_px(r, f->tam);
            if (c != '\t' && c != '\f' && c >= 32) glifo(j, xa, base, xb - xa, em, c, f->est, cor);
            if ((f->est & RTE_SUBLINHADO) && c >= 32) retang_clip(j, xa, base + 1 + em / 16, xb - xa, em / 14 > 1 ? em / 14 : 1, cor);
            if ((f->est & RTE_RISCADO) && c >= 32) retang_clip(j, xa, base - em / 3, xb - xa, 1, cor);
            xc += adv_t;
        }
        /* cursor */
        if (r->foco && !tem_sel && r->cur >= ln->ini && r->cur <= ln->fim && linha_de(r, r->cur) == li) {
            int cx = ox + tw2px(r, r->m_esq + x_de(r, ln, r->cur));
            retang_clip(j, cx, ly, 1, lh > 2 ? lh : 2, W98_NEGRO);
        }
    }
    clip_x0 = r->x; clip_y0 = r->y; clip_x1 = r->x + r->l; clip_y1 = r->y + r->a;
    /* barras de desplazamiento */
    barra_desenhar(j, r->x + r->vis_l, r->y, BARRA_L, r->vis_a, true, doc_alt_px(r), r->vis_a, r->topo);
    if (r->hbarra) {
        barra_desenhar(j, r->x, r->y + r->vis_a, r->vis_l, BARRA_L, false,
                       pag_larg_px(r) + 2 * MARGEM_PX, r->vis_l, r->esq_px);
        g_retangulo(j, r->x + r->vis_l, r->y + r->vis_a, BARRA_L, BARRA_L, W98_FACE);
    }
    jan_mostrar_area(j, r->x, r->y, r->l, r->a);
}

/* ------------------------------------------------------------ ratón */

static int pos_en_pixel(rte_t *r, int mx, int my)
{
    maquetar(r);
    int ox = pagina_x(r), ph = pag_alt_px(r);
    int rel = my - r->y - MARGEM_PX + r->topo;
    int p = rel < 0 ? 0 : rel / (ph + GAP_PX);
    if (p >= r->npag) p = r->npag - 1;
    int ty = (my - pagina_y(r, p)) * 1500 / (r->zoom ? r->zoom : 100) - r->m_sup;
    int xt = (mx - ox) * 1500 / (r->zoom ? r->zoom : 100) - r->m_esq;
    int mejor = -1;
    for (int i = 0; i < r->nlin; i++) {
        const linha_t *ln = &r->lin[i];
        if (ln->pag != p) continue;
        if (mejor < 0) mejor = i;
        if (ty >= ln->y) mejor = i;
        if (ty < ln->y + ln->alt) break;
    }
    if (mejor < 0) { /* página sin líneas: la última línea anterior */
        for (int i = 0; i < r->nlin; i++) if (r->lin[i].pag <= p) mejor = i;
        if (mejor < 0) mejor = 0;
    }
    return pos_en_x(r, &r->lin[mejor], xt);
}

static void palavra_em(const rte_t *r, int pos, int *a, int *b)
{
    int s = pos, e = pos;
    bool (*letra)(unsigned char) = NULL;
    (void)letra;
    while (s > 0 && r->t[s - 1] != ' ' && r->t[s - 1] != '\n' && r->t[s - 1] != '\t' && r->t[s - 1] != '\f') s--;
    while (e < r->n && r->t[e] != ' ' && r->t[e] != '\n' && r->t[e] != '\t' && r->t[e] != '\f') e++;
    *a = s; *b = e;
}

static bool dentro_r(int px, int py, int x, int y, int l, int a) { return px >= x && py >= y && px < x + l && py < y + a; }

/* Barra de desplazamiento: clic en flecha, carril o pulgar. */
static bool clic_barra(jan_t *j, rte_t *r, const evento_t *e)
{
    int vx = r->x + r->vis_l;
    if (dentro_r(e->x, e->y, vx, r->y, BARRA_L, r->vis_a)) {
        int trilho = r->vis_a - 2 * BARRA_L, total = doc_alt_px(r), vis = r->vis_a;
        int pol = total <= vis ? trilho : trilho * vis / total; if (pol < 12) pol = 12; if (pol > trilho) pol = trilho;
        int max = total - vis; if (max < 1) max = 1;
        int off = (trilho - pol) * r->topo / max;
        int ry = e->y - r->y;
        if (ry < BARRA_L) r->topo -= 48;
        else if (ry >= r->vis_a - BARRA_L) r->topo += 48;
        else if (ry < BARRA_L + off) r->topo -= vis - 24;
        else if (ry >= BARRA_L + off + pol) r->topo += vis - 24;
        else { r->drag_barra = 1; r->drag_off = ry - BARRA_L - off; }
        calc_vista(r);
        rte_desenhar(j, r);
        return true;
    }
    if (r->hbarra && dentro_r(e->x, e->y, r->x, r->y + r->vis_a, r->vis_l, BARRA_L)) {
        int trilho = r->vis_l - 2 * BARRA_L, total = pag_larg_px(r) + 2 * MARGEM_PX, vis = r->vis_l;
        int pol = total <= vis ? trilho : trilho * vis / total; if (pol < 12) pol = 12; if (pol > trilho) pol = trilho;
        int max = total - vis; if (max < 1) max = 1;
        int off = (trilho - pol) * r->esq_px / max;
        int rx = e->x - r->x;
        if (rx < BARRA_L) r->esq_px -= 32;
        else if (rx >= r->vis_l - BARRA_L) r->esq_px += 32;
        else if (rx < BARRA_L + off) r->esq_px -= vis - 24;
        else if (rx >= BARRA_L + off + pol) r->esq_px += vis - 24;
        else { r->drag_barra = 2; r->drag_off = rx - BARRA_L - off; }
        calc_vista(r);
        rte_desenhar(j, r);
        return true;
    }
    return false;
}

static void arrastar_barra(jan_t *j, rte_t *r, const evento_t *e)
{
    if (r->drag_barra == 1) {
        int trilho = r->vis_a - 2 * BARRA_L, total = doc_alt_px(r), vis = r->vis_a;
        int pol = total <= vis ? trilho : trilho * vis / total; if (pol < 12) pol = 12; if (pol > trilho) pol = trilho;
        int livre = trilho - pol; if (livre < 1) livre = 1;
        int off = e->y - r->y - BARRA_L - r->drag_off;
        r->topo = off * (total - vis) / livre;
    } else {
        int trilho = r->vis_l - 2 * BARRA_L, total = pag_larg_px(r) + 2 * MARGEM_PX, vis = r->vis_l;
        int pol = total <= vis ? trilho : trilho * vis / total; if (pol < 12) pol = 12; if (pol > trilho) pol = trilho;
        int livre = trilho - pol; if (livre < 1) livre = 1;
        int off = e->x - r->x - BARRA_L - r->drag_off;
        r->esq_px = off * (total - vis) / livre;
    }
    calc_vista(r);
    rte_desenhar(j, r);
}

/* ------------------------------------------------------------ formato */

void rte_fmt_atual(const rte_t *r, rte_fmt_t *f)
{
    int a, b;
    if (sel_rango(r, &a, &b)) { *f = r->fmts[r->at[a]]; return; }
    *f = r->fmt_ins;
}

static void fmt_mod(rte_t *r, unsigned campos, const rte_fmt_t *f, unsigned est_set, unsigned est_clr)
{
    int a, b;
    if (sel_rango(r, &a, &b)) {
        marcar_undo(r, 0);
        uint8_t cache_de[256]; int cache_para[256];
        memset(cache_de, 0, sizeof cache_de);
        for (int i = 0; i < 256; i++) cache_para[i] = -1;
        for (int i = a; i < b; i++) {
            int id = r->at[i];
            if (cache_para[id] < 0) {
                rte_fmt_t g = r->fmts[id];
                if (campos & RTE_C_FAM) g.fam = f->fam;
                if (campos & RTE_C_TAM) g.tam = f->tam;
                if (campos & RTE_C_COR) g.cor = f->cor;
                g.est = (uint8_t)((g.est | est_set) & ~est_clr);
                cache_para[id] = fmt_id(r, &g);
            }
            r->at[i] = (uint8_t)cache_para[id];
        }
        r->sujo = true;
    } else {
        rte_fmt_t g = r->fmt_ins;
        if (campos & RTE_C_FAM) g.fam = f->fam;
        if (campos & RTE_C_TAM) g.tam = f->tam;
        if (campos & RTE_C_COR) g.cor = f->cor;
        g.est = (uint8_t)((g.est | est_set) & ~est_clr);
        r->fmt_ins = g;
        /* en un párrafo vacío, la marca de párrafo toma el formato (altura de la línea) */
        if (r->cur >= r->n || r->t[r->cur] == '\n') {
            if (r->cur < r->n && (r->cur == 0 || r->t[r->cur - 1] == '\n')) {
                marcar_undo(r, 0);
                r->at[r->cur] = (uint8_t)fmt_id(r, &g);
                r->sujo = true;
            }
        }
        r->ult_tipo = 0;
    }
}

void rte_fmt_aplicar(rte_t *r, unsigned campos, const rte_fmt_t *f) { fmt_mod(r, campos, f, 0, 0); }

void rte_estilo(rte_t *r, unsigned bits, bool ligar)
{
    rte_fmt_t f = r->fmt_ins;
    fmt_mod(r, 0, &f, ligar ? bits : 0, ligar ? 0 : bits);
}

void rte_par_atual(const rte_t *r, rte_par_t *p) { *p = r->par[par_idx(r, r->cur)]; }

void rte_par_aplicar(rte_t *r, unsigned campos, const rte_par_t *p)
{
    int a = r->cur, b = r->cur, s0, s1;
    if (sel_rango(r, &s0, &s1)) { a = s0; b = s1 > s0 ? s1 - 1 : s1; }
    int pa = par_idx(r, a), pb = par_idx(r, b);
    marcar_undo(r, 0);
    for (int i = pa; i <= pb && i < r->npar; i++) {
        rte_par_t *q = &r->par[i];
        if (campos & RTE_P_ALIN) q->alin = p->alin;
        if (campos & RTE_P_SANG) { q->esq = p->esq; q->dir = p->dir; q->prim = p->prim; }
        if (campos & RTE_P_INTER) q->interlin = p->interlin;
        if (campos & RTE_P_MARC) {
            q->marcador = p->marcador;
            if (p->marcador && q->esq == 0 && q->prim == 0) { q->esq = 360; q->prim = -360; }
            if (!p->marcador && q->esq == 360 && q->prim == -360) { q->esq = 0; q->prim = 0; }
        }
        if (campos & RTE_P_ESPACO) { q->antes = p->antes; q->depois = p->depois; }
    }
    r->sujo = true;
    r->ult_tipo = 0;
}

/* ------------------------------------------------------------ edición */

void rte_inserir(rte_t *r, const char *t, int n)
{
    marcar_undo(r, (n == 1 && t[0] != '\n') ? 1 : 0);
    apagar_sel(r);
    int id = fmt_id(r, &r->fmt_ins);
    if (ins_raw(r, r->cur, t, NULL, n, id)) r->cur += n;
    r->ult_pos = r->cur;
    r->col_pref = -1;
}

void rte_salto_pagina(rte_t *r) { rte_inserir(r, "\f", 1); }

void rte_selecionar(rte_t *r, int a, int b)
{
    if (a < 0) a = 0;
    if (b > r->n) b = r->n;
    if (a > b) a = b;
    r->anc = a == b ? -1 : a;
    r->cur = b;
    fmt_sincronizar(r);
}

void rte_selecionar_tudo(rte_t *r) { r->anc = 0; r->cur = r->n; r->col_pref = -1; }

void rte_ir_pagina(rte_t *r, int pagina)
{
    maquetar(r);
    if (pagina < 1) pagina = 1;
    for (int i = 0; i < r->nlin; i++)
        if (r->lin[i].pag + 1 >= pagina) { r->cur = r->lin[i].ini; r->anc = -1; fmt_sincronizar(r); cursor_visivel(r); return; }
    r->cur = r->n; r->anc = -1;
    cursor_visivel(r);
}

/* Portapapeles con formato propio: el texto va al del sistema; si al pegar sigue igual, se
 * pegan también los formatos y los párrafos. */
static struct { char *t; rte_fmt_t *f; rte_par_t *par; int n, npar; } clip_rico;

static void clip_liberar(void) { libera(clip_rico.t); libera(clip_rico.f); libera(clip_rico.par); memset(&clip_rico, 0, sizeof clip_rico); }

bool rte_copiar(rte_t *r)
{
    int a, b;
    if (!sel_rango(r, &a, &b)) return false;
    clip_liberar();
    int n = b - a, nl = 0;
    for (int i = a; i < b; i++) if (r->t[i] == '\n') nl++;
    clip_rico.t = aloca((size_t)n + 1);
    clip_rico.f = aloca((size_t)n * sizeof(rte_fmt_t) + 1);
    clip_rico.par = aloca((size_t)(nl + 1) * sizeof(rte_par_t));
    if (clip_rico.t && clip_rico.f && clip_rico.par) {
        memcpy(clip_rico.t, r->t + a, (size_t)n);
        clip_rico.t[n] = 0;
        for (int i = 0; i < n; i++) clip_rico.f[i] = r->fmts[r->at[a + i]];
        int p0 = par_idx(r, a);
        for (int k = 0; k <= nl; k++) clip_rico.par[k] = r->par[p0 + k];
        clip_rico.n = n; clip_rico.npar = nl + 1;
    } else clip_liberar();
    copiar_texto(r->t + a, (unsigned)n);
    return true;
}

bool rte_cortar(jan_t *j, rte_t *r)
{
    (void)j;
    if (!rte_copiar(r)) return false;
    marcar_undo(r, 0);
    apagar_sel(r);
    fmt_sincronizar(r);
    return true;
}

bool rte_colar(jan_t *j, rte_t *r)
{
    (void)j;
    int n = colar_texto(NULL, 0);
    if (n <= 0) return false;
    char *b = aloca((size_t)n + 1);
    if (!b) return false;
    n = colar_texto(b, (unsigned)n + 1);
    int k = 0;
    for (int i = 0; i < n; i++) if (b[i] != '\r') b[k++] = b[i];
    b[k] = 0;
    marcar_undo(r, 0);
    apagar_sel(r);
    bool rico = clip_rico.t && clip_rico.n == k && memcmp(clip_rico.t, b, (size_t)k) == 0;
    int pos = r->cur;
    int p0 = par_idx(r, pos);
    if (rico) {
        uint8_t *ids = aloca((size_t)k + 1);
        if (ids) {
            for (int i = 0; i < k; i++) ids[i] = (uint8_t)fmt_id(r, &clip_rico.f[i]);
            ins_raw(r, pos, b, ids, k, 0);
            libera(ids);
            for (int q = 1; q < clip_rico.npar; q++) r->par[p0 + q] = clip_rico.par[q];
        }
    } else {
        ins_raw(r, pos, b, NULL, k, fmt_id(r, &r->fmt_ins));
    }
    r->cur = pos + k;
    r->ult_tipo = 0;
    libera(b);
    return true;
}

/* ------------------------------------------------------------ buscar y contar */

static unsigned char minus(unsigned char c)
{
    if (c >= 'A' && c <= 'Z') return (unsigned char)(c + 32);
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return (unsigned char)(c + 32);
    return c;
}

static bool coincide(const rte_t *r, int pos, const char *t, int n, bool maius)
{
    if (pos + n > r->n) return false;
    for (int i = 0; i < n; i++) {
        unsigned char a = (unsigned char)r->t[pos + i], b = (unsigned char)t[i];
        if (!maius) { a = minus(a); b = minus(b); }
        if (a != b) return false;
    }
    return true;
}

bool rte_buscar(rte_t *r, const char *t, bool maius, bool adiante)
{
    int n = (int)tam_texto(t);
    if (n == 0) return false;
    int a, b;
    int desde = r->cur;
    if (sel_rango(r, &a, &b)) desde = adiante ? b : a;
    if (adiante) {
        for (int k = 0; k < r->n; k++) {
            int p = (desde + k) % (r->n ? r->n : 1);
            if (coincide(r, p, t, n, maius)) { r->anc = p; r->cur = p + n; goto achou; }
        }
    } else {
        for (int k = 1; k <= r->n; k++) {
            int p = ((desde - k) % r->n + r->n) % r->n;
            if (coincide(r, p, t, n, maius)) { r->anc = p; r->cur = p + n; goto achou; }
        }
    }
    return false;
achou:
    fmt_sincronizar(r);
    cursor_visivel(r);
    return true;
}

bool rte_substituir(rte_t *r, const char *por)
{
    int a, b;
    if (!sel_rango(r, &a, &b)) return false;
    rte_fmt_t f = r->fmts[r->at[a]];
    marcar_undo(r, 0);
    del_raw(r, a, b);
    r->anc = -1;
    int id = fmt_id(r, &f), m = (int)tam_texto(por);
    ins_raw(r, a, por, NULL, m, id);
    r->cur = a + m;
    return true;
}

int rte_substituir_tudo(rte_t *r, const char *t, const char *por, bool maius)
{
    int n = (int)tam_texto(t), m = (int)tam_texto(por), cont = 0;
    if (n == 0) return 0;
    marcar_undo(r, 0);
    int p = 0;
    while (p + n <= r->n) {
        if (coincide(r, p, t, n, maius)) {
            rte_fmt_t f = r->fmts[r->at[p]];
            del_raw(r, p, p + n);
            ins_raw(r, p, por, NULL, m, fmt_id(r, &f));
            p += m ? m : 0;
            cont++;
        } else p++;
    }
    r->cur = r->cur > r->n ? r->n : r->cur;
    r->anc = -1;
    return cont;
}

void rte_contar(const rte_t *r, int *palavras, int *caracteres, int *paragrafos)
{
    int w = 0, c = 0, p = 0;
    bool em = false;
    for (int i = 0; i < r->n; i++) {
        unsigned char ch = (unsigned char)r->t[i];
        bool esp = ch == ' ' || ch == '\n' || ch == '\t' || ch == '\f';
        if (!esp) { c++; if (!em) { w++; em = true; } } else em = false;
    }
    for (int i = 0; i < r->npar; i++) {
        int a, b;
        rte_par_dados(r, i, &a, &b, NULL);
        if (b > a) p++;
    }
    if (palavras) *palavras = w;
    if (caracteres) *caracteres = c;
    if (paragrafos) *paragrafos = p;
}

/* ------------------------------------------------------------ eventos */

static void mover_cursor(rte_t *r, int pos, bool shift)
{
    if (pos < 0) pos = 0;
    if (pos > r->n) pos = r->n;
    if (shift) { if (r->anc < 0) r->anc = r->cur; }
    else r->anc = -1;
    r->cur = pos;
    if (r->anc == r->cur) r->anc = -1;
}

static int palavra_ant(const rte_t *r, int p)
{
    while (p > 0 && (r->t[p - 1] == ' ' || r->t[p - 1] == '\n')) p--;
    while (p > 0 && r->t[p - 1] != ' ' && r->t[p - 1] != '\n') p--;
    return p;
}

static int palavra_sig(const rte_t *r, int p)
{
    while (p < r->n && r->t[p] != ' ' && r->t[p] != '\n') p++;
    while (p < r->n && (r->t[p] == ' ' || r->t[p] == '\n')) p++;
    return p;
}

int rte_tratar(jan_t *j, rte_t *r, const evento_t *e)
{
    int res = 0;
    calc_vista(r);
    if (e->tipo == EV_RODA) {
        if (!dentro_r(e->x, e->y, r->x, r->y, r->l, r->a)) return 0;
        if (e->modificadores & 2) {
            rte_zoom(r, r->zoom - (int)e->cliques * 10);
            res |= RTE_R_CURSOR;
        } else r->topo += (int)e->cliques * 48;
        calc_vista(r);
        rte_desenhar(j, r);
        return res;
    }
    if (e->tipo == EV_CLIQUE && (e->botoes & 1)) {
        if (!dentro_r(e->x, e->y, r->x, r->y, r->l, r->a)) { if (r->foco) { r->foco = false; rte_desenhar(j, r); } return 0; }
        r->foco = true;
        if (clic_barra(j, r, e)) return 0;
        int p = pos_en_pixel(r, e->x, e->y);
        if (e->cliques >= 2) { int a, b; palavra_em(r, p, &a, &b); r->anc = a; r->cur = b; if (a == b) r->anc = -1; }
        else if (e->modificadores & 1) { if (r->anc < 0) r->anc = r->cur; r->cur = p; }
        else { r->anc = -1; r->cur = p; }
        if (r->anc == r->cur) r->anc = -1;
        r->arrastando = true;
        r->col_pref = -1;
        fmt_sincronizar(r);
        rte_desenhar(j, r);
        return RTE_R_CURSOR;
    }
    if (e->tipo == EV_MOVER && (e->botoes & 1)) {
        if (r->drag_barra) { arrastar_barra(j, r, e); return 0; }
        if (r->arrastando) {
            if (r->anc < 0) r->anc = r->cur;
            int p = pos_en_pixel(r, e->x, e->y);
            if (p != r->cur) {
                r->cur = p;
                if (e->y < r->y) r->topo -= 16;
                else if (e->y > r->y + r->vis_a) r->topo += 16;
                calc_vista(r);
                rte_desenhar(j, r);
                return RTE_R_CURSOR;
            }
        }
        return 0;
    }
    if (e->tipo == EV_SOLTOU) {
        r->drag_barra = 0;
        if (r->arrastando) {
            r->arrastando = false;
            if (r->anc == r->cur) r->anc = -1;
            fmt_sincronizar(r);
            return RTE_R_CURSOR;
        }
        return 0;
    }
    if (e->tipo != EV_TECLA || !r->foco) return 0;
    unsigned t = e->tecla;
    bool shift = (e->modificadores & 1) != 0, ctrl = (e->modificadores & 2) != 0;
    int antes_n = r->n;
    if (ctrl && t >= 1 && t <= 26) {
        switch (t) {
        case 1: rte_selecionar_tudo(r); res |= RTE_R_CURSOR; break;
        case 3: rte_copiar(r); return 0;
        case 24: if (rte_cortar(j, r)) res |= RTE_R_MUDOU | RTE_R_CURSOR; break;
        case 22: if (rte_colar(j, r)) res |= RTE_R_MUDOU | RTE_R_CURSOR; break;
        case 26: if (rte_desfazer(r)) res |= RTE_R_MUDOU | RTE_R_CURSOR; break;
        case 25: if (rte_refazer(r)) res |= RTE_R_MUDOU | RTE_R_CURSOR; break;
        case 2: rte_estilo(r, RTE_NEGRITA, !(r->fmt_ins.est & RTE_NEGRITA) ); res |= RTE_R_MUDOU | RTE_R_CURSOR; break;
        case 9: rte_estilo(r, RTE_CURSIVA, !(r->fmt_ins.est & RTE_CURSIVA)); res |= RTE_R_MUDOU | RTE_R_CURSOR; break;
        case 21: rte_estilo(r, RTE_SUBLINHADO, !(r->fmt_ins.est & RTE_SUBLINHADO)); res |= RTE_R_MUDOU | RTE_R_CURSOR; break;
        case 10: rte_salto_pagina(r); res |= RTE_R_MUDOU | RTE_R_CURSOR; break;   /* Ctrl+Intro */
        default: return 0;
        }
        goto fim;
    }
    {
        bool mover = t == TECLA_ESQUERDA || t == TECLA_DIREITA || t == TECLA_CIMA || t == TECLA_BAIXO ||
                     t == TECLA_PGUP || t == TECLA_PGDN || t == TECLA_HOME || t == TECLA_END;
        if (mover) {
            maquetar(r);
            int li = linha_de(r, r->cur);
            const linha_t *ln = &r->lin[li];
            int np = r->cur;
            bool vert = false;
            if (t == TECLA_ESQUERDA) np = ctrl ? palavra_ant(r, r->cur) : (r->anc >= 0 && !shift ? (r->anc < r->cur ? r->anc : r->cur) : r->cur - 1);
            else if (t == TECLA_DIREITA) np = ctrl ? palavra_sig(r, r->cur) : (r->anc >= 0 && !shift ? (r->anc > r->cur ? r->anc : r->cur) : r->cur + 1);
            else if (t == TECLA_HOME) np = ctrl ? 0 : ln->ini;
            else if (t == TECLA_END) np = ctrl ? r->n : (ln->fim > ln->ini && !ln->fin_par && r->t[ln->fim - 1] == ' ' ? ln->fim - 1 : ln->fim);
            else {
                vert = true;
                if (r->col_pref < 0) r->col_pref = x_de(r, ln, r->cur);
                int passo = (t == TECLA_PGUP || t == TECLA_PGDN) ? (r->vis_a * 1500 / r->zoom) : 0;
                int alvo = li;
                if (t == TECLA_CIMA) alvo = li - 1;
                else if (t == TECLA_BAIXO) alvo = li + 1;
                else {
                    int yabs = ln->pag * (r->ph + 1000) + ln->y;
                    int dest = yabs + (t == TECLA_PGDN ? passo : -passo);
                    alvo = li;
                    while (alvo > 0 && alvo < r->nlin - 1) {
                        int ya = r->lin[alvo].pag * (r->ph + 1000) + r->lin[alvo].y;
                        if (t == TECLA_PGDN ? ya >= dest : ya <= dest) break;
                        alvo += t == TECLA_PGDN ? 1 : -1;
                    }
                }
                if (alvo < 0) { alvo = 0; np = 0; }
                else if (alvo >= r->nlin) { alvo = r->nlin - 1; np = r->n; }
                else np = pos_en_x(r, &r->lin[alvo], r->col_pref);
            }
            if (!vert) r->col_pref = -1;
            mover_cursor(r, np, shift);
            fmt_sincronizar(r);
            r->ult_tipo = 0;
            res |= RTE_R_CURSOR;
            goto fim;
        }
    }
    if (t == '\b') {
        marcar_undo(r, 0);
        if (!apagar_sel(r)) { if (r->cur > 0) { del_raw(r, r->cur - 1, r->cur); r->cur--; } else { r->nund--; snap_liberar(&r->und[r->nund]); } }
        r->col_pref = -1;
        fmt_sincronizar(r);
        res |= RTE_R_MUDOU | RTE_R_CURSOR;
    } else if (t == TECLA_DELETE) {
        marcar_undo(r, 0);
        if (!apagar_sel(r)) del_raw(r, r->cur, r->cur + 1);
        r->col_pref = -1;
        res |= RTE_R_MUDOU | RTE_R_CURSOR;
    } else if (t == '\n' || t == '\r') {
        rte_inserir(r, "\n", 1);
        res |= RTE_R_MUDOU | RTE_R_CURSOR;
    } else if (t == '\t' || (t >= 0x20 && t < 0x100 && t != 0x7F)) {
        char c = (char)t;
        rte_inserir(r, &c, 1);
        res |= RTE_R_MUDOU | RTE_R_CURSOR;
    } else return 0;
fim:
    (void)antes_n;
    cursor_visivel(r);
    rte_desenhar(j, r);
    return res;
}

/* ------------------------------------------------------------ exportación a PDF */

int rte_exportar_pdf(rte_t *r, const char *titulo, uint8_t **dados, int *n)
{
    maquetar(r);
    pdfw_t *w = pdfw_criar(PDFW_A4);
    if (!w) return -1;
    pdfw_tamanho(w, r->pw, r->ph);
    pdfw_titulo(w, titulo);
    int pag_atual = -1;
    for (int li = 0; li < r->nlin; li++) {
        const linha_t *ln = &r->lin[li];
        while (pag_atual < ln->pag) { if (!pdfw_pagina(w)) { pdfw_liberar(w); return -1; } pag_atual++; }
        int ybase = r->m_sup + ln->y + ln->base;
        if (ln->marca) {
            const rte_par_t *pr = &r->par[ln->par];
            int bx = r->m_esq + (pr->esq + pr->prim < 0 ? 0 : pr->esq + pr->prim);
            rte_fmt_t f = ln->fim > ln->ini ? r->fmts[r->at[ln->ini]] : r->fmt_ins;
            pdfw_texto(w, bx, ybase, f.fam, f.est & 3, f.tam * 20, f.cor, "\xB7", 1, 0);
        }
        /* tramos de igual formato, partidos en los tabuladores */
        int x = ln->x0;
        int i = ln->ini;
        while (i < ln->fim) {
            if (r->t[i] == '\t' || r->t[i] == '\f') { x += avance(r, i, x); i++; continue; }
            int k = i, id = r->at[i];
            int xi = x;
            while (k < ln->fim && r->at[k] == id && r->t[k] != '\t' && r->t[k] != '\f') { x += avance(r, k, x) + (r->t[k] == ' ' ? ln->extra : 0); k++; }
            const rte_fmt_t *f = &r->fmts[id];
            /* los espacios finales de una línea no se dibujan: no importan al PDF */
            pdfw_texto(w, r->m_esq + xi, ybase, f->fam, f->est & 3, f->tam * 20, f->cor, r->t + i, k - i, ln->extra);
            int tam = f->tam * 20;
            int larg = x - xi;
            if (f->est & RTE_SUBLINHADO) pdfw_retangulo(w, r->m_esq + xi, ybase + tam / 10, larg, tam / 18 + 4, f->cor);
            if (f->est & RTE_RISCADO) pdfw_retangulo(w, r->m_esq + xi, ybase - tam * 3 / 10, larg, tam / 18 + 4, f->cor);
            i = k;
        }
    }
    while (pag_atual < r->npag - 1) { pdfw_pagina(w); pag_atual++; }
    bool ok = pdfw_finalizar(w, dados, n);
    pdfw_liberar(w);
    return ok ? 0 : -1;
}
