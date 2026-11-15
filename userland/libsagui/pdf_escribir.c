// SPDX-License-Identifier: GPL-2.0-or-later
/* pdf_escribir.c - escritor de PDF mínimo (mejora 116, D291). Ver pdf_escribir.h. */
#include "pdf_escribir.h"
#include "compressao.h"
#include "pdf_metricas.h"

#define MAX_PAGINAS 2000

typedef struct { char *d; int n, cap; } bufer_t;

struct pdfw {
    int      papel, larg, alt;
    char     titulo[128];
    bufer_t  pag[MAX_PAGINAS];
    int      npag;
    bool     fuente_usada[12];
    bool     fallo;
    /* Estado gráfico de la página en curso: evita repetir fuente y color. */
    int      f_actual, tam_actual;
    uint32_t cor_actual;
};

static void b_poner(bufer_t *b, const char *s, int n, bool *fallo)
{
    if (b->n + n + 1 > b->cap) {
        int nc = b->cap ? b->cap * 2 : 4096;
        while (nc < b->n + n + 1) nc *= 2;
        char *p = realoca(b->d, (size_t)nc);
        if (!p) { *fallo = true; return; }
        b->d = p; b->cap = nc;
    }
    memcpy(b->d + b->n, s, (size_t)n);
    b->n += n;
    b->d[b->n] = 0;
}

static void b_texto(bufer_t *b, const char *s, bool *fallo) { b_poner(b, s, (int)tam_texto(s), fallo); }

/* Número con 2 decimales a partir de centésimas. */
static int fmt_cent(char *d, long v)
{
    int n = 0;
    if (v < 0) { d[n++] = '-'; v = -v; }
    n += formatar_texto(d + n, 20, "%d.%02d", (int)(v / 100), (int)(v % 100));
    return n;
}

/* twips -> puntos con 2 decimales (twips / 20 = twips * 5 centésimas). */
static int fmt_tw(char *d, long tw) { return fmt_cent(d, tw * 5); }

static int fmt_cor(char *d, uint32_t c)
{
    int r = (int)((c >> 16) & 255) * 1000 / 255, g = (int)((c >> 8) & 255) * 1000 / 255, b = (int)(c & 255) * 1000 / 255;
    return formatar_texto(d, 40, "%d.%03d %d.%03d %d.%03d", r / 1000, r % 1000, g / 1000, g % 1000, b / 1000, b % 1000);
}

void pdfw_papel_tamanho(int papel, int *l, int *a)
{
    if (papel == PDFW_CARTA) { *l = 12240; *a = 15840; }
    else { *l = 11906; *a = 16838; }
}

/* Índice de fuente: familia*4 + estilo (sans: 0..3, serif: 4..7, mono: 8..11). */
static int idx_fonte(int fam, int est) { return (fam < 0 || fam > 2 ? 0 : fam) * 4 + (est & 3); }

static const char *const nomes_fonte[12] = {
    "Helvetica", "Helvetica-Bold", "Helvetica-Oblique", "Helvetica-BoldOblique",
    "Times-Roman", "Times-Bold", "Times-Italic", "Times-BoldItalic",
    "Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique",
};

int pdfw_largura_char(int fam, int est, unsigned char c)
{
    if (c < 32) return 0;
    int tab;
    if (fam == PDFW_MONO) tab = 6;
    else if (fam == PDFW_SERIF) tab = 2 + (est & 3);
    else tab = (est & PDFW_NEGRITA) ? 1 : 0;
    return pdf_larguras[tab][c - 32];
}

int pdfw_largura_texto(int fam, int est, int tam, const char *t, int n)
{
    long s = 0;
    for (int i = 0; i < n; i++) s += pdfw_largura_char(fam, est, (unsigned char)t[i]);
    return (int)((s * tam + 500) / 1000);
}

pdfw_t *pdfw_criar(int papel)
{
    pdfw_t *w = aloca_zerado(sizeof *w);
    if (!w) return NULL;
    w->papel = papel;
    pdfw_papel_tamanho(papel, &w->larg, &w->alt);
    return w;
}

void pdfw_tamanho(pdfw_t *w, int l, int a) { if (l > 1000 && a > 1000) { w->larg = l; w->alt = a; } }

void pdfw_liberar(pdfw_t *w)
{
    if (!w) return;
    for (int i = 0; i < w->npag; i++) libera(w->pag[i].d);
    libera(w);
}

void pdfw_titulo(pdfw_t *w, const char *t) { copia_texto(w->titulo, t ? t : "", sizeof w->titulo); }
int  pdfw_paginas(const pdfw_t *w) { return w->npag; }

bool pdfw_pagina(pdfw_t *w)
{
    if (w->npag >= MAX_PAGINAS) return false;
    w->npag++;
    w->f_actual = -1; w->tam_actual = -1; w->cor_actual = 0xFFFFFFFFu;
    return true;
}

static void cor_rellenar(pdfw_t *w, bufer_t *b, uint32_t cor, bool trazo)
{
    char t[64];
    int n = fmt_cor(t, cor);
    b_poner(b, t, n, &w->fallo);
    b_texto(b, trazo ? " RG\n" : " rg\n", &w->fallo);
}

void pdfw_texto(pdfw_t *w, int x, int y, int fam, int est, int tam, uint32_t cor,
                const char *t, int n, int extra)
{
    if (w->npag == 0 || n <= 0) return;
    bufer_t *b = &w->pag[w->npag - 1];
    int f = idx_fonte(fam, est);
    w->fuente_usada[f] = true;
    char num[48];
    b_texto(b, "BT\n", &w->fallo);
    if (cor != w->cor_actual) { cor_rellenar(w, b, cor, false); w->cor_actual = cor; }
    int k = formatar_texto(num, sizeof num, "/F%d ", f + 1);
    b_poner(b, num, k, &w->fallo);
    k = fmt_tw(num, tam); b_poner(b, num, k, &w->fallo);
    b_texto(b, " Tf\n", &w->fallo);
    k = fmt_tw(num, extra); b_poner(b, num, k, &w->fallo);
    b_texto(b, " Tw\n1 0 0 1 ", &w->fallo);
    k = fmt_tw(num, x); b_poner(b, num, k, &w->fallo);
    b_poner(b, " ", 1, &w->fallo);
    k = fmt_tw(num, w->alt - y); b_poner(b, num, k, &w->fallo);
    b_texto(b, " Tm\n(", &w->fallo);
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)t[i];
        if (c == '(' || c == ')' || c == '\\') { char e[2] = { '\\', (char)c }; b_poner(b, e, 2, &w->fallo); }
        else if (c < 32 || c >= 127) {
            if (c < 32) continue;
            char e[4] = { '\\', (char)('0' + (c >> 6)), (char)('0' + ((c >> 3) & 7)), (char)('0' + (c & 7)) };
            b_poner(b, e, 4, &w->fallo);
        } else b_poner(b, (const char *)&c, 1, &w->fallo);
    }
    b_texto(b, ") Tj\nET\n", &w->fallo);
}

void pdfw_retangulo(pdfw_t *w, int x, int y, int l, int a, uint32_t cor)
{
    if (w->npag == 0) return;
    bufer_t *b = &w->pag[w->npag - 1];
    char num[48];
    if (cor != w->cor_actual) { cor_rellenar(w, b, cor, false); w->cor_actual = cor; }
    int k = fmt_tw(num, x); b_poner(b, num, k, &w->fallo); b_poner(b, " ", 1, &w->fallo);
    k = fmt_tw(num, w->alt - y - a); b_poner(b, num, k, &w->fallo); b_poner(b, " ", 1, &w->fallo);
    k = fmt_tw(num, l); b_poner(b, num, k, &w->fallo); b_poner(b, " ", 1, &w->fallo);
    k = fmt_tw(num, a); b_poner(b, num, k, &w->fallo);
    b_texto(b, " re f\n", &w->fallo);
}

void pdfw_linha(pdfw_t *w, int x0, int y0, int x1, int y1, int esp, uint32_t cor)
{
    if (w->npag == 0) return;
    bufer_t *b = &w->pag[w->npag - 1];
    char num[48];
    cor_rellenar(w, b, cor, true);
    int k = fmt_tw(num, esp); b_poner(b, num, k, &w->fallo); b_texto(b, " w\n", &w->fallo);
    int v[4] = { x0, w->alt - y0, x1, w->alt - y1 };
    static const char *const sep[4] = { " ", " m ", " ", " l\nS\n" };
    for (int i = 0; i < 4; i++) {
        k = fmt_tw(num, v[i]); b_poner(b, num, k, &w->fallo);
        b_texto(b, sep[i], &w->fallo);
    }
}

/* Escribe un objeto "N 0 obj ... endobj" y apunta su posición. */
static void obj_ini(bufer_t *o, int *pos, int num, bool *fallo)
{
    char t[40];
    pos[num] = o->n;
    int k = formatar_texto(t, sizeof t, "%d 0 obj\n", num);
    b_poner(o, t, k, fallo);
}

bool pdfw_finalizar(pdfw_t *w, uint8_t **dados, int *tam)
{
    if (w->npag == 0 || w->fallo) return false;
    int nobj = 3 + 12 + 2 * w->npag;   /* 1 catálogo, 2 páginas, 3 info, 4..15 fuentes, luego contenido+página */
    int *pos = aloca_zerado(sizeof(int) * (size_t)(nobj + 1));
    if (!pos) return false;
    bufer_t o = { 0, 0, 0 };
    bool *f = &w->fallo;
    char t[256];
    b_texto(&o, "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n", f);
    obj_ini(&o, pos, 1, f);
    b_texto(&o, "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n", f);
    obj_ini(&o, pos, 2, f);
    b_texto(&o, "<< /Type /Pages /Kids [", f);
    for (int i = 0; i < w->npag; i++) {
        int k = formatar_texto(t, sizeof t, "%d 0 R ", 17 + 2 * i);
        b_poner(&o, t, k, f);
    }
    int k = formatar_texto(t, sizeof t, "] /Count %d >>\nendobj\n", w->npag);
    b_poner(&o, t, k, f);
    obj_ini(&o, pos, 3, f);
    b_texto(&o, "<< /Producer (SaguiOS Pluma) /Title (", f);
    for (const char *p = w->titulo; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '(' || c == ')' || c == '\\') b_poner(&o, "\\", 1, f);
        if (c >= 32 && c < 127) b_poner(&o, (const char *)&c, 1, f);
        else if (c >= 127) { char e[4] = { '\\', (char)('0' + (c >> 6)), (char)('0' + ((c >> 3) & 7)), (char)('0' + (c & 7)) }; b_poner(&o, e, 4, f); }
    }
    b_texto(&o, ") >>\nendobj\n", f);
    for (int i = 0; i < 12; i++) {
        obj_ini(&o, pos, 4 + i, f);
        k = formatar_texto(t, sizeof t, "<< /Type /Font /Subtype /Type1 /BaseFont /%s /Encoding /WinAnsiEncoding >>\nendobj\n", nomes_fonte[i]);
        b_poner(&o, t, k, f);
    }
    for (int p = 0; p < w->npag && !*f; p++) {
        bufer_t *c = &w->pag[p];
        int lim = deflar_limite(c->n) + 16;
        uint8_t *z = aloca((size_t)lim);
        if (!z) { *f = true; break; }
        int nz = 2;
        z[0] = 0x78; z[1] = 0x9C;
        int r = deflar((const uint8_t *)c->d, c->n, z + 2, lim - 6, 6);
        if (r < 0) { libera(z); *f = true; break; }
        nz += r;
        uint32_t ad = adler32_somar(1, c->d, (size_t)c->n);
        z[nz++] = (uint8_t)(ad >> 24); z[nz++] = (uint8_t)(ad >> 16); z[nz++] = (uint8_t)(ad >> 8); z[nz++] = (uint8_t)ad;
        obj_ini(&o, pos, 16 + 2 * p, f);
        k = formatar_texto(t, sizeof t, "<< /Length %d /Filter /FlateDecode >>\nstream\n", nz);
        b_poner(&o, t, k, f);
        b_poner(&o, (const char *)z, nz, f);
        b_texto(&o, "\nendstream\nendobj\n", f);
        libera(z);
        obj_ini(&o, pos, 17 + 2 * p, f);
        b_texto(&o, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ", f);
        char a[24];
        k = fmt_tw(a, w->larg); b_poner(&o, a, k, f); b_poner(&o, " ", 1, f);
        k = fmt_tw(a, w->alt); b_poner(&o, a, k, f);
        b_texto(&o, "] /Resources << /Font << ", f);
        for (int i = 0; i < 12; i++) {
            k = formatar_texto(t, sizeof t, "/F%d %d 0 R ", i + 1, 4 + i);
            b_poner(&o, t, k, f);
        }
        k = formatar_texto(t, sizeof t, ">> >> /Contents %d 0 R >>\nendobj\n", 16 + 2 * p);
        b_poner(&o, t, k, f);
    }
    int xref = o.n;
    k = formatar_texto(t, sizeof t, "xref\n0 %d\n0000000000 65535 f \n", nobj + 1);
    b_poner(&o, t, k, f);
    for (int i = 1; i <= nobj; i++) {
        k = formatar_texto(t, sizeof t, "%010d 00000 n \n", pos[i]);
        b_poner(&o, t, k, f);
    }
    k = formatar_texto(t, sizeof t, "trailer\n<< /Size %d /Root 1 0 R /Info 3 0 R >>\nstartxref\n%d\n%%%%EOF\n", nobj + 1, xref);
    b_poner(&o, t, k, f);
    libera(pos);
    if (*f) { libera(o.d); return false; }
    *dados = (uint8_t *)o.d;
    *tam = o.n;
    return true;
}
