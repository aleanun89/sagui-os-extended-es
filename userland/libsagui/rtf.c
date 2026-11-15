// SPDX-License-Identifier: GPL-2.0-or-later
/* rtf.c - lector y escritor de RTF (mejora 116, D291). Ver rtf.h. */
#include "rtf.h"

#define PILHA_MAX 64
#define FONTES_MAX 64
#define CORES_MAX 64

typedef struct { rte_fmt_t f; bool ignora; int uc; } estado_t;

typedef struct {
    rte_t *r;
    estado_t pilha[PILHA_MAX]; int prof;
    uint8_t fonte_fam[FONTES_MAX];
    uint32_t cores[CORES_MAX]; int ncores;
    int deff;
    rte_par_t par; int sl, slmult;
    char buf[256]; int nbuf; rte_fmt_t buf_f;
    int skip;
    int fonttbl_prof, colortbl_prof;     /* profundidad en la que empezó la tabla (0 = no está en ella) */
    int fonte_idx, fonte_hint; char fonte_nome[48]; int fonte_nn;
    int cr, cg, cb; bool cor_dada;
    int paperw, paperh, margl, margr, margt, margb;
} lector_t;

static const rte_par_t PAR_BASE = { RTE_ESQ, 0, 0, 0, 0, 100, 0, 0 };

static void descarga(lector_t *L)
{
    if (L->nbuf) { rte_doc_anexar(L->r, L->buf, L->nbuf, &L->buf_f); L->nbuf = 0; }
}

static void poner_char(lector_t *L, unsigned char c)
{
    estado_t *s = &L->pilha[L->prof];
    if (s->ignora || L->fonttbl_prof || L->colortbl_prof) return;
    if (L->skip > 0) { L->skip--; return; }
    if (L->nbuf && (L->nbuf >= (int)sizeof L->buf || L->buf_f.fam != s->f.fam || L->buf_f.est != s->f.est ||
                    L->buf_f.tam != s->f.tam || L->buf_f.cor != s->f.cor)) descarga(L);
    if (L->nbuf == 0) L->buf_f = s->f;
    L->buf[L->nbuf++] = (char)c;
}

static void fin_parrafo(lector_t *L)
{
    descarga(L);
    rte_par_t p = L->par;
    if (L->sl > 0 && L->slmult) { int v = L->sl * 100 / 240; p.interlin = (uint16_t)(v < 100 ? 100 : v > 300 ? 300 : v); }
    if (p.marcador && p.esq == 0 && p.prim == 0) { p.esq = 360; p.prim = -360; }
    rte_doc_par(L->r, &p);
    estado_t *s = &L->pilha[L->prof];
    rte_fmt_t f = s->f;
    rte_doc_anexar(L->r, "\n", 1, &f);
}

static unsigned char minus(unsigned char c) { return c >= 'A' && c <= 'Z' ? (unsigned char)(c + 32) : c; }

static bool contiene(const char *t, const char *p)
{
    int n = (int)tam_texto(p);
    for (int i = 0; t[i]; i++) {
        int k = 0;
        while (k < n && t[i + k] && minus((unsigned char)t[i + k]) == (unsigned char)p[k]) k++;
        if (k == n) return true;
    }
    return false;
}

static int clasificar_fonte(int hint, const char *nome)
{
    if (contiene(nome, "courier") || contiene(nome, "mono") || contiene(nome, "consolas") || contiene(nome, "console") || contiene(nome, "typewriter")) return RTE_MONO;
    if (contiene(nome, "arial") || contiene(nome, "helvet") || contiene(nome, "sans") || contiene(nome, "calibri") ||
        contiene(nome, "verdana") || contiene(nome, "tahoma") || contiene(nome, "segoe")) return RTE_SANS;
    if (contiene(nome, "times") || contiene(nome, "serif") || contiene(nome, "georgia") || contiene(nome, "garamond") ||
        contiene(nome, "cambria") || contiene(nome, "palatino") || contiene(nome, "book") || contiene(nome, "roman")) return RTE_SERIF;
    if (hint == 2) return RTE_SERIF;       /* \froman */
    if (hint == 3) return RTE_MONO;        /* \fmodern */
    return RTE_SANS;
}

static void fin_fonte(lector_t *L)
{
    if (L->fonte_idx >= 0 && L->fonte_idx < FONTES_MAX) {
        L->fonte_nome[L->fonte_nn] = 0;
        L->fonte_fam[L->fonte_idx] = (uint8_t)clasificar_fonte(L->fonte_hint, L->fonte_nome);
    }
    L->fonte_nn = 0; L->fonte_hint = 0;
}

static void padrao_fmt(lector_t *L, rte_fmt_t *f)
{
    f->fam = L->deff >= 0 && L->deff < FONTES_MAX ? L->fonte_fam[L->deff] : RTE_SANS;
    f->est = 0; f->tam = 12; f->cor = 0;
}

static void emitir_u(lector_t *L, int v)
{
    if (v < 0) v += 65536;
    if (v >= 32 && v < 256) { poner_char(L, (unsigned char)v); return; }
    if (v == 0x2013 || v == 0x2014 || v == 0x2212) poner_char(L, '-');
    else if (v == 0x2018 || v == 0x2019) poner_char(L, '\'');
    else if (v == 0x201C || v == 0x201D) poner_char(L, '"');
    else if (v == 0x2022 || v == 0xF0B7) poner_char(L, 0xB7);
    else if (v == 0x2026) { poner_char(L, '.'); poner_char(L, '.'); poner_char(L, '.'); }
    else if (v == 0x20AC) poner_char(L, 'E');
    else if (v == 0x00A0) poner_char(L, 0xA0);
    else poner_char(L, '?');
}

static bool destino_ignorado(const char *w)
{
    static const char *const l[] = { "stylesheet", "info", "pict", "header", "footer", "headerl", "headerr", "headerf",
        "footerl", "footerr", "footerf", "footnote", "listtable", "listoverridetable", "generator", "revtbl", "themedata",
        "colorschememapping", "latentstyles", "datastore", "xmlnstbl", "rsidtbl", "private", "fldinst", "object",
        "bkmkstart", "bkmkend", "shpinst", "nonshppict", "filetbl", "pgptbl", "operator", "title", "author", NULL };
    for (int i = 0; l[i]; i++) if (compara_texto(w, l[i]) == 0) return true;
    return false;
}

static void palabra(lector_t *L, const char *w, bool tiene, int v)
{
    estado_t *s = &L->pilha[L->prof];
    rte_fmt_t *f = &s->f;
    /* tablas */
    if (L->fonttbl_prof) {
        if (compara_texto(w, "f") == 0 && tiene) L->fonte_idx = v;
        else if (compara_texto(w, "froman") == 0) L->fonte_hint = 2;
        else if (compara_texto(w, "fmodern") == 0) L->fonte_hint = 3;
        else if (compara_texto(w, "fswiss") == 0) L->fonte_hint = 1;
        return;
    }
    if (L->colortbl_prof) {
        if (compara_texto(w, "red") == 0) { L->cr = v; L->cor_dada = true; }
        else if (compara_texto(w, "green") == 0) { L->cg = v; L->cor_dada = true; }
        else if (compara_texto(w, "blue") == 0) { L->cb = v; L->cor_dada = true; }
        return;
    }
    if (compara_texto(w, "fonttbl") == 0) { L->fonttbl_prof = L->prof; L->fonte_idx = -1; L->fonte_nn = 0; L->fonte_hint = 0; return; }
    if (compara_texto(w, "colortbl") == 0) { L->colortbl_prof = L->prof; L->ncores = 0; L->cr = L->cg = L->cb = 0; L->cor_dada = false; return; }
    if (compara_texto(w, "pnlvlblt") == 0) { L->par.marcador = 1; return; }
    if (compara_texto(w, "pntext") == 0 || compara_texto(w, "listtext") == 0) { L->par.marcador = 1; s->ignora = true; return; }
    if (destino_ignorado(w)) { s->ignora = true; return; }
    if (s->ignora) return;
    /* caracteres especiales */
    if (compara_texto(w, "par") == 0 || compara_texto(w, "line") == 0 || compara_texto(w, "sect") == 0) { fin_parrafo(L); return; }
    if (compara_texto(w, "tab") == 0) { poner_char(L, '\t'); return; }
    if (compara_texto(w, "page") == 0) { poner_char(L, '\f'); return; }
    if (compara_texto(w, "bullet") == 0) { poner_char(L, 0xB7); return; }
    if (compara_texto(w, "endash") == 0 || compara_texto(w, "emdash") == 0) { poner_char(L, '-'); return; }
    if (compara_texto(w, "lquote") == 0 || compara_texto(w, "rquote") == 0) { poner_char(L, '\''); return; }
    if (compara_texto(w, "ldblquote") == 0 || compara_texto(w, "rdblquote") == 0) { poner_char(L, '"'); return; }
    if (compara_texto(w, "emspace") == 0 || compara_texto(w, "enspace") == 0) { poner_char(L, ' '); return; }
    if (compara_texto(w, "u") == 0 && tiene) { emitir_u(L, v); L->skip = s->uc; return; }
    if (compara_texto(w, "uc") == 0 && tiene) { s->uc = v; return; }
    /* formato de carácter */
    if (L->nbuf) descarga(L);
    if (compara_texto(w, "plain") == 0) { padrao_fmt(L, f); return; }
    if (compara_texto(w, "b") == 0) { f->est = (uint8_t)(!tiene || v ? f->est | RTE_NEGRITA : f->est & ~RTE_NEGRITA); return; }
    if (compara_texto(w, "i") == 0) { f->est = (uint8_t)(!tiene || v ? f->est | RTE_CURSIVA : f->est & ~RTE_CURSIVA); return; }
    if (compara_texto(w, "ulnone") == 0) { f->est = (uint8_t)(f->est & ~RTE_SUBLINHADO); return; }
    if (compara_texto_n(w, "ul", 2) == 0 && (w[2] == 0 || compara_texto(w, "uld") == 0 || compara_texto(w, "uldb") == 0 ||
                                              compara_texto(w, "ulw") == 0 || compara_texto(w, "ulth") == 0)) {
        f->est = (uint8_t)(!tiene || v ? f->est | RTE_SUBLINHADO : f->est & ~RTE_SUBLINHADO); return;
    }
    if (compara_texto(w, "strike") == 0 || compara_texto(w, "striked") == 0) {
        f->est = (uint8_t)(!tiene || v ? f->est | RTE_RISCADO : f->est & ~RTE_RISCADO); return;
    }
    if (compara_texto(w, "fs") == 0 && tiene) { int t = (v + 1) / 2; f->tam = (uint8_t)(t < 1 ? 1 : t > 127 ? 127 : t); return; }
    if (compara_texto(w, "f") == 0 && tiene) { f->fam = v >= 0 && v < FONTES_MAX ? L->fonte_fam[v] : RTE_SANS; return; }
    if (compara_texto(w, "cf") == 0 && tiene) { f->cor = v > 0 && v < L->ncores ? L->cores[v] : 0; return; }
    if (compara_texto(w, "deff") == 0 && tiene) { L->deff = v; return; }
    /* párrafo */
    if (compara_texto(w, "pard") == 0) { L->par = PAR_BASE; L->sl = 0; L->slmult = 0; return; }
    if (compara_texto(w, "ql") == 0) { L->par.alin = RTE_ESQ; return; }
    if (compara_texto(w, "qc") == 0) { L->par.alin = RTE_CENTRO; return; }
    if (compara_texto(w, "qr") == 0) { L->par.alin = RTE_DIR; return; }
    if (compara_texto(w, "qj") == 0) { L->par.alin = RTE_JUST; return; }
    if ((compara_texto(w, "li") == 0) && tiene) { L->par.esq = (int16_t)v; return; }
    if (compara_texto(w, "ri") == 0 && tiene) { L->par.dir = (int16_t)v; return; }
    if (compara_texto(w, "fi") == 0 && tiene) { L->par.prim = (int16_t)v; return; }
    if (compara_texto(w, "sl") == 0 && tiene) { L->sl = v; return; }
    if (compara_texto(w, "slmult") == 0 && tiene) { L->slmult = v; return; }
    if (compara_texto(w, "sb") == 0 && tiene) { L->par.antes = (int16_t)v; return; }
    if (compara_texto(w, "sa") == 0 && tiene) { L->par.depois = (int16_t)v; return; }
    /* hoja */
    if (compara_texto(w, "paperw") == 0 && tiene) L->paperw = v;
    else if (compara_texto(w, "paperh") == 0 && tiene) L->paperh = v;
    else if (compara_texto(w, "margl") == 0 && tiene) L->margl = v;
    else if (compara_texto(w, "margr") == 0 && tiene) L->margr = v;
    else if (compara_texto(w, "margt") == 0 && tiene) L->margt = v;
    else if (compara_texto(w, "margb") == 0 && tiene) L->margb = v;
}

static int hex(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

int rtf_ler(rte_t *r, const char *d, int n)
{
    if (n < 6 || memcmp(d, "{\\rtf", 5) != 0) return -1;
    lector_t *L = aloca_zerado(sizeof *L);
    if (!L) return -1;
    rte_limpar(r);
    L->r = r; L->par = PAR_BASE; L->deff = 0;
    for (int i = 0; i < FONTES_MAX; i++) L->fonte_fam[i] = RTE_SANS;
    padrao_fmt(L, &L->pilha[0].f);
    L->pilha[0].uc = 1;
    bool dest_ign = false;
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)d[i];
        if (c == '{') {
            if (L->prof + 1 < PILHA_MAX) { L->pilha[L->prof + 1] = L->pilha[L->prof]; L->prof++; }
            continue;
        }
        if (c == '}') {
            descarga(L);
            if (L->fonttbl_prof && L->prof == L->fonttbl_prof) L->fonttbl_prof = 0;
            if (L->colortbl_prof && L->prof == L->colortbl_prof) L->colortbl_prof = 0;
            if (L->prof > 0) L->prof--;
            continue;
        }
        if (c == '\r' || c == '\n') continue;
        if (c != '\\') {
            if (L->fonttbl_prof) {
                if (c == ';') fin_fonte(L);
                else if (L->fonte_nn < (int)sizeof L->fonte_nome - 1) L->fonte_nome[L->fonte_nn++] = (char)c;
            } else if (L->colortbl_prof) {
                if (c == ';') {
                    if (L->ncores < CORES_MAX)
                        L->cores[L->ncores++] = L->cor_dada ? ((uint32_t)(L->cr & 255) << 16) | ((uint32_t)(L->cg & 255) << 8) | (uint32_t)(L->cb & 255) : 0;
                    L->cr = L->cg = L->cb = 0; L->cor_dada = false;
                }
            } else poner_char(L, c);
            continue;
        }
        /* secuencia de control */
        if (++i >= n) break;
        c = (unsigned char)d[i];
        if (c == '\\' || c == '{' || c == '}') { poner_char(L, c); continue; }
        if (c == '\'') {
            if (i + 2 < n && hex(d[i + 1]) >= 0 && hex(d[i + 2]) >= 0) { poner_char(L, (unsigned char)(hex(d[i + 1]) * 16 + hex(d[i + 2]))); i += 2; }
            continue;
        }
        if (c == '*') { L->pilha[L->prof].ignora = true; dest_ign = true; continue; }
        if (c == '~') { poner_char(L, 0xA0); continue; }
        if (c == '_' ) { poner_char(L, '-'); continue; }
        if (c == '\r' || c == '\n') { fin_parrafo(L); continue; }
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) continue;
        char w[32]; int nw = 0;
        while (i < n && ((d[i] >= 'a' && d[i] <= 'z') || (d[i] >= 'A' && d[i] <= 'Z'))) { if (nw < 31) w[nw++] = d[i]; i++; }
        w[nw] = 0;
        bool tiene = false; int v = 0, sg = 1;
        if (i < n && d[i] == '-') { sg = -1; i++; }
        while (i < n && d[i] >= '0' && d[i] <= '9') { tiene = true; v = v * 10 + (d[i] - '0'); i++; }
        v *= sg;
        if (!(i < n && d[i] == ' ')) i--;   /* un espacio tras la palabra es su delimitador */
        palabra(L, w, tiene, v);
    }
    (void)dest_ign;
    descarga(L);
    rte_par_t p = L->par;
    if (L->sl > 0 && L->slmult) { int v = L->sl * 100 / 240; p.interlin = (uint16_t)(v < 100 ? 100 : v > 300 ? 300 : v); }
    if (p.marcador && p.esq == 0 && p.prim == 0) { p.esq = 360; p.prim = -360; }
    rte_doc_par(r, &p);
    rte_doc_fim(r);
    int pw, ph, ml, mr, mt, mb;
    rte_papel_obter(r, &pw, &ph, &ml, &mr, &mt, &mb);
    if (L->paperw > 2000 && L->paperh > 2000) { pw = L->paperw; ph = L->paperh; }
    if (L->margl > 0 || L->margr > 0) { ml = L->margl; mr = L->margr; }
    if (L->margt > 0 || L->margb > 0) { mt = L->margt; mb = L->margb; }
    rte_papel(r, pw, ph, ml, mr, mt, mb);
    libera(L);
    return 0;
}

/* ------------------------------------------------------------ escritor */

typedef struct { char *d; int n, cap; bool fallo; } sal_t;

static void sp(sal_t *s, const char *t, int n)
{
    if (s->fallo) return;
    if (s->n + n + 1 > s->cap) {
        int nc = s->cap ? s->cap * 2 : 8192;
        while (nc < s->n + n + 1) nc *= 2;
        char *p = realoca(s->d, (size_t)nc);
        if (!p) { s->fallo = true; return; }
        s->d = p; s->cap = nc;
    }
    memcpy(s->d + s->n, t, (size_t)n);
    s->n += n;
    s->d[s->n] = 0;
}

static void st(sal_t *s, const char *t) { sp(s, t, (int)tam_texto(t)); }

static void sf(sal_t *s, const char *fmt, int a, int b, int c)
{
    char t[160];
    int n = formatar_texto(t, sizeof t, fmt, a, b, c);
    sp(s, t, n);
}

int rtf_escrever(rte_t *r, char **saida, int *n)
{
    sal_t s = { 0, 0, 0, false };
    uint32_t cores[CORES_MAX]; int nc = 0;
    int np = rte_n_par(r);
    const char *txt = rte_texto(r);
    for (int p = 0; p < np; p++) {
        int a, b;
        rte_par_dados(r, p, &a, &b, NULL);
        for (int i = a; i < b; i++) {
            rte_fmt_t f; rte_fmt_em(r, i, &f);
            if (f.cor == 0) continue;
            bool ja = false;
            for (int k = 0; k < nc; k++) if (cores[k] == f.cor) ja = true;
            if (!ja && nc < CORES_MAX - 1) cores[nc++] = f.cor;
        }
    }
    int pw, ph, ml, mr, mt, mb;
    rte_papel_obter(r, &pw, &ph, &ml, &mr, &mt, &mb);
    st(&s, "{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1034\n{\\fonttbl{\\f0\\fswiss\\fcharset0 Arial;}{\\f1\\froman\\fcharset0 Times New Roman;}{\\f2\\fmodern\\fcharset0 Courier New;}}\n{\\colortbl ;");
    for (int k = 0; k < nc; k++) sf(&s, "\\red%d\\green%d\\blue%d;", (int)((cores[k] >> 16) & 255), (int)((cores[k] >> 8) & 255), (int)(cores[k] & 255));
    st(&s, "}\n");
    sf(&s, "\\paperw%d\\paperh%d\\margl%d", pw, ph, ml);
    sf(&s, "\\margr%d\\margt%d\\margb%d", mr, mt, mb);
    st(&s, "\\widowctrl\\f0\\fs24\n");
    for (int p = 0; p < np; p++) {
        int a, b;
        rte_par_t pr;
        rte_par_dados(r, p, &a, &b, &pr);
        st(&s, "\\pard");
        st(&s, pr.alin == RTE_CENTRO ? "\\qc" : pr.alin == RTE_DIR ? "\\qr" : pr.alin == RTE_JUST ? "\\qj" : "\\ql");
        if (pr.esq) sf(&s, "\\li%d", pr.esq, 0, 0);
        if (pr.dir) sf(&s, "\\ri%d", pr.dir, 0, 0);
        if (pr.prim) sf(&s, "\\fi%d", pr.prim, 0, 0);
        if (pr.interlin > 100) sf(&s, "\\sl%d\\slmult1", pr.interlin * 240 / 100, 0, 0);
        if (pr.antes) sf(&s, "\\sb%d", pr.antes, 0, 0);
        if (pr.depois) sf(&s, "\\sa%d", pr.depois, 0, 0);
        if (pr.marcador) st(&s, "{\\pntext\\f0\\'B7\\tab}{\\*\\pn\\pnlvlblt\\pnf0\\pnindent360{\\pntxtb\\'B7}}");
        int i = a;
        while (i < b) {
            rte_fmt_t f; rte_fmt_em(r, i, &f);
            int k = i + 1;
            while (k < b) {
                rte_fmt_t g; rte_fmt_em(r, k, &g);
                if (g.fam != f.fam || g.est != f.est || g.tam != f.tam || g.cor != f.cor) break;
                k++;
            }
            st(&s, "{");
            sf(&s, "\\f%d\\fs%d", f.fam, f.tam * 2, 0);
            if (f.est & RTE_NEGRITA) st(&s, "\\b");
            if (f.est & RTE_CURSIVA) st(&s, "\\i");
            if (f.est & RTE_SUBLINHADO) st(&s, "\\ul");
            if (f.est & RTE_RISCADO) st(&s, "\\strike");
            if (f.cor) { int ci = 0; for (int q = 0; q < nc; q++) if (cores[q] == f.cor) ci = q + 1; if (ci) sf(&s, "\\cf%d", ci, 0, 0); }
            st(&s, " ");
            for (int q = i; q < k; q++) {
                unsigned char c = (unsigned char)txt[q];
                if (c == '\\' || c == '{' || c == '}') { char e[2] = { '\\', (char)c }; sp(&s, e, 2); }
                else if (c == '\t') st(&s, "\\tab ");
                else if (c == '\f') st(&s, "\\page ");
                else if (c >= 128) {
                    char e[5] = { '\\', '\'', "0123456789abcdef"[c >> 4], "0123456789abcdef"[c & 15], 0 };
                    sp(&s, e, 4);
                }
                else sp(&s, (const char *)&c, 1);
            }
            st(&s, "}");
            i = k;
        }
        st(&s, "\\par\n");
    }
    st(&s, "}\n");
    if (s.fallo) { libera(s.d); return -1; }
    *saida = s.d;
    *n = s.n;
    return 0;
}
