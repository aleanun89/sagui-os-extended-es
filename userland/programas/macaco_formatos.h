// SPDX-License-Identifier: GPL-2.0-or-later
/* macaco_formatos.h - Macaco lee y escribe XLSX (Office Open XML) y ODS (OpenDocument) (D332).
 *
 * Se incluye en macaco.c, después del libro y de los .mxs: usa sus tipos y funciones estáticas.
 * Lo que se guarda en ambos formatos: valores, fórmulas (con su resultado), anchos de columna, alturas y
 * filas/columnas ocultas, celdas combinadas (XLSX <mergeCells>, ODS table:number-columns-spanned /
 * number-rows-spanned), paneles inmovilizados (XLSX) y el formato de celda (negrita, cursiva, subrayado,
 * color, relleno, alineación, bordes y tamaño; en XLSX también el formato de número).
 * El ZIP y la compresión son los de la libsagui (compressao.h). El XML se escribe y se lee a mano:
 * un lector «de empuje» de etiquetas muy pequeño, suficiente para estos dos formatos. */
#ifndef MACACO_FORMATOS_H
#define MACACO_FORMATOS_H

#include "compressao.h"

/* ------------------------------------------------------------ escritura de XML */

/* Añade texto de Latin-1 escapado para XML y convertido a UTF-8. */
static void xml_esc(char **buf, int *tam, int *cap, const char *s)
{
    char tmp[1100];
    int n = 0;
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (n > (int)sizeof tmp - 8) { tmp[n] = 0; txt_add(buf, tam, cap, tmp); n = 0; }
        if (ch == '&') { memcpy(tmp + n, "&amp;", 5); n += 5; }
        else if (ch == '<') { memcpy(tmp + n, "&lt;", 4); n += 4; }
        else if (ch == '>') { memcpy(tmp + n, "&gt;", 4); n += 4; }
        else if (ch == '"') { memcpy(tmp + n, "&quot;", 6); n += 6; }
        else if (ch < 0x20 && ch != '\n' && ch != '\t') { /* los controles no caben en XML 1.0 */ }
        else if (ch >= 0x80) { tmp[n++] = (char)(0xC0 | (ch >> 6)); tmp[n++] = (char)(0x80 | (ch & 0x3F)); }
        else tmp[n++] = (char)ch;
    }
    tmp[n] = 0;
    txt_add(buf, tam, cap, tmp);
}

/* ------------------------------------------------------------ lectura de XML */

typedef struct { const char *p, *fim; } xr_t;
typedef struct {
    int         tipo;        /* 1 = etiqueta que abre, 2 = etiqueta que cierra, 3 = texto */
    char        nome[56];
    const char *attr;        /* los atributos sin interpretar */
    int         attr_n;
    bool        vazia;       /* <x/> */
    const char *txt;         /* texto (sin decodificar las entidades) */
    int         txt_n;
} xtok_t;

static bool xml_prox(xr_t *x, xtok_t *t)
{
    memset(t, 0, sizeof *t);
    while (x->p < x->fim) {
        if (*x->p != '<') {
            const char *ini = x->p;
            while (x->p < x->fim && *x->p != '<') x->p++;
            t->tipo = 3; t->txt = ini; t->txt_n = (int)(x->p - ini);
            return true;
        }
        if (x->p + 1 < x->fim && (x->p[1] == '?' || x->p[1] == '!')) {
            if (x->p + 8 < x->fim && memcmp(x->p, "<![CDATA[", 9) == 0) {
                const char *ini = x->p + 9, *q = ini;
                while (q + 2 < x->fim && !(q[0] == ']' && q[1] == ']' && q[2] == '>')) q++;
                x->p = (q + 2 < x->fim) ? q + 3 : x->fim;
                t->tipo = 3; t->txt = ini; t->txt_n = (int)(q - ini);
                return true;
            }
            if (x->p + 3 < x->fim && memcmp(x->p, "<!--", 4) == 0) {
                const char *q = x->p + 4;
                while (q + 2 < x->fim && !(q[0] == '-' && q[1] == '-' && q[2] == '>')) q++;
                x->p = (q + 2 < x->fim) ? q + 3 : x->fim;
                continue;
            }
            while (x->p < x->fim && *x->p != '>') x->p++;
            if (x->p < x->fim) x->p++;
            continue;
        }
        const char *q = x->p + 1;
        bool cierra = false;
        if (q < x->fim && *q == '/') { cierra = true; q++; }
        int n = 0;
        while (q < x->fim && *q != '>' && *q != ' ' && *q != '/' && *q != '\t' && *q != '\n' && *q != '\r') {
            if (n < (int)sizeof t->nome - 1) t->nome[n++] = *q;
            q++;
        }
        t->nome[n] = 0;
        const char *ini_attr = q;
        char comilla = 0;
        while (q < x->fim && (comilla || *q != '>')) {      /* hasta el '>' que no esté entre comillas */
            if (comilla) { if (*q == comilla) comilla = 0; }
            else if (*q == '"' || *q == '\'') comilla = *q;
            q++;
        }
        const char *fin_attr = q;
        if (fin_attr > ini_attr && fin_attr[-1] == '/') { t->vazia = true; fin_attr--; }
        t->attr = ini_attr; t->attr_n = (int)(fin_attr - ini_attr);
        t->tipo = cierra ? 2 : 1;
        x->p = (q < x->fim) ? q + 1 : x->fim;
        return true;
    }
    return false;
}

/* Decodifica las entidades de un tramo de texto (el XML ya se pasó a Latin-1). */
static int xml_decod(const char *s, int n, char *dst, int max)
{
    int k = 0;
    for (int i = 0; i < n && k < max - 1; i++) {
        if (s[i] != '&') { dst[k++] = s[i]; continue; }
        int j = i + 1;
        while (j < n && s[j] != ';' && j - i < 10) j++;
        if (j >= n || s[j] != ';') { dst[k++] = s[i]; continue; }
        int el = j - i - 1;
        const char *e = s + i + 1;
        char ch = '?';
        if (el == 3 && memcmp(e, "amp", 3) == 0) ch = '&';
        else if (el == 2 && memcmp(e, "lt", 2) == 0) ch = '<';
        else if (el == 2 && memcmp(e, "gt", 2) == 0) ch = '>';
        else if (el == 4 && memcmp(e, "quot", 4) == 0) ch = '"';
        else if (el == 4 && memcmp(e, "apos", 4) == 0) ch = '\'';
        else if (el > 1 && e[0] == '#') {
            int v = 0;
            if (e[1] == 'x' || e[1] == 'X') {
                for (int q = 2; q < el; q++) { char c = e[q]; v = v * 16 + ((c <= '9') ? c - '0' : ((c | 32) - 'a' + 10)); }
            } else for (int q = 1; q < el; q++) v = v * 10 + (e[q] - '0');
            ch = (v >= 32 && v < 256) ? (char)v : (v == 9 || v == 10 ? (char)v : '?');
        }
        dst[k++] = ch;
        i = j;
    }
    dst[k] = 0;
    return k;
}

/* El valor de un atributo (con sus entidades decodificadas); false si no está. */
static bool xml_attr(const xtok_t *t, const char *nome, char *dst, int max)
{
    int ln = (int)tam_texto(nome);
    const char *p = t->attr, *fim = t->attr + t->attr_n;
    while (p < fim) {
        while (p < fim && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        const char *ini = p;
        while (p < fim && *p != '=' && *p != ' ') p++;
        int nn = (int)(p - ini);
        while (p < fim && (*p == ' ' || *p == '=')) p++;
        if (p >= fim || (*p != '"' && *p != '\'')) return false;
        char cm = *p++;
        const char *v = p;
        while (p < fim && *p != cm) p++;
        if (nn == ln && memcmp(ini, nome, (size_t)ln) == 0) { xml_decod(v, (int)(p - v), dst, max); return true; }
        if (p < fim) p++;
    }
    return false;
}

static int xml_attr_int(const xtok_t *t, const char *nome, int omision)
{
    char v[32];
    if (!xml_attr(t, nome, v, sizeof v)) return omision;
    return texto_para_int(v);
}

/* Quita el prefijo de espacio de nombres («x:row» -> «row») para comparar. */
static const char *xml_local(const char *nome)
{
    const char *d = acha_char(nome, ':');
    return d ? d + 1 : nome;
}

static bool xml_nome_es(const xtok_t *t, const char *nome) { return compara_texto(xml_local(t->nome), nome) == 0; }

/* Un número decimal con hasta 4 cifras, multiplicado por 10000 (y la posición donde acaba). */
static int xml_decimal_10k(const char *s, const char **fim)
{
    int v = 0;
    bool neg = false;
    if (*s == '-') { neg = true; s++; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    if (v > 200000) v = 200000;
    v *= 10000;
    if (*s == '.') {
        s++;
        int m = 1000;
        while (*s >= '0' && *s <= '9') { v += (*s - '0') * m; m /= 10; s++; }
    }
    if (fim) *fim = s;
    return neg ? -v : v;
}

/* Una medida de ODS («2.26cm», «0.75in», «17pt», «10mm») en píxeles a 96 ppp. */
static int xml_medida_px(const char *s)
{
    const char *u;
    int v = xml_decimal_10k(s, &u);
    if (v > 20000000) v = 20000000;
    int px;
    if (compara_texto(u, "cm") == 0) px = (v * 96 + 12700) / 25400;
    else if (compara_texto(u, "mm") == 0) px = (v * 96 + 127000) / 254000;
    else if (compara_texto(u, "pt") == 0) px = (v * 4 + 15000) / 30000;
    else px = (v * 96 + 5000) / 10000;         /* pulgadas (y lo que no se conoce) */
    return px;
}

/* Una medida de ODS en puntos (para los tamaños de letra). */
static int xml_medida_pt(const char *s)
{
    const char *u;
    int v = xml_decimal_10k(s, &u);
    if (compara_texto(u, "pt") == 0) return (v + 5000) / 10000;
    return (xml_medida_px(s) * 72 + 48) / 96;
}

/* ------------------------------------------------------------ el formato de celda <-> XLSX/ODS */

static const char *const FONTES_XML[3] = { "Arial", "Times New Roman", "Courier New" };

static void rgb_hex(char *dst, uint32_t rgb)
{
    static const char dig[] = "0123456789ABCDEF";
    for (int i = 0; i < 6; i++) dst[i] = dig[(rgb >> (20 - 4 * i)) & 15];
    dst[6] = 0;
}

static uint32_t hex_rgb(const char *s)
{
    if (*s == '#') s++;
    uint32_t v = 0;
    int n = 0;
    while (n < 8 && ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F'))) {
        v = v * 16u + (uint32_t)((*s <= '9') ? *s - '0' : ((*s | 32) - 'a' + 10));
        s++; n++;
    }
    return v & 0xFFFFFF;
}

/* El código de formato de número de XLSX para un formato de Macaco; false = General. */
static bool nf_codigo(const fmt_t *f, char *dst, int max)
{
    char dec[8];
    int d = f->dec > 4 ? 4 : f->dec;
    int k = 0;
    if (d > 0) { dec[k++] = '.'; for (int i = 0; i < d; i++) dec[k++] = '0'; }
    dec[k] = 0;
    switch (f->nf) {
    case NF_NUMERO: formatar_texto(dst, max, "0%s", dec); return true;
    case NF_MILHAR: formatar_texto(dst, max, "#,##0%s", dec); return true;
    case NF_PERCENT: formatar_texto(dst, max, "0%s%%", dec); return true;
    case NF_MOEDA: {
        const char *m = f->moeda == 1 ? "EUR " : (f->moeda == 2 ? "£" : "$");
        formatar_texto(dst, max, "\"%s\"#,##0%s", m, dec);
        return true;
    }
    default: return false;
    }
}

/* Lo contrario: de un código de formato a nf/dec/moeda. */
static void nf_de_codigo(const char *c, fmt_t *f)
{
    f->nf = NF_GERAL; f->dec = 0; f->moeda = 0;
    if (!c[0] || compara_texto(c, "General") == 0) return;
    bool pct = acha_char(c, '%') != NULL;
    bool mil = acha_texto(c, "#,##0") != NULL;
    bool mon = acha_char(c, '$') != NULL || acha_texto(c, "EUR") != NULL || acha_char(c, '\xA3') != NULL;
    const char *pt = acha_char(c, '.');
    int d = 0;
    if (pt) { pt++; while (*pt == '0') { d++; pt++; } }
    if (d > 4) d = 4;
    f->dec = (uint8_t)d;
    if (pct) f->nf = NF_PERCENT;
    else if (mon) { f->nf = NF_MOEDA; f->moeda = acha_texto(c, "EUR") ? 1 : (acha_char(c, '\xA3') ? 2 : 0); }
    else if (mil) f->nf = NF_MILHAR;
    else if (c[0] == '0') f->nf = NF_NUMERO;
}

/* ------------------------------------------------------------ las fórmulas */

/* Fórmula de Macaco (sin «=») a la de XLSX: el separador de argumentos es la coma. */
static void formula_para_xlsx(const char *f, char **buf, int *tam, int *cap)
{
    char tmp[MAC_TEXTO_CEL + 8];
    int k = 0;
    bool aspas = false;
    for (; *f && k < (int)sizeof tmp - 2; f++) {
        char ch = *f;
        if (ch == '"') aspas = !aspas;
        if (!aspas && ch == ';') ch = ',';
        tmp[k++] = ch;
    }
    tmp[k] = 0;
    xml_esc(buf, tam, cap, tmp);
}

/* Fórmula de Macaco (sin «=») a OpenFormula: referencias entre corchetes y «;» como separador. */
static void formula_para_ods(const char *f, char **buf, int *tam, int *cap)
{
    reftok_t tk[48];
    int n = refs_da_formula(f, tk, 48);
    char tmp[2 * MAC_TEXTO_CEL];
    int k = 0, p = 0;
    for (int i = 0; i <= n; i++) {
        int ate = (i < n) ? tk[i].ini : (int)tam_texto(f);
        bool aspas = false;
        for (int q = 0; q < p; q++) if (f[q] == '"') aspas = !aspas;
        while (p < ate && k < (int)sizeof tmp - 2) {
            char ch = f[p++];
            if (ch == '"') aspas = !aspas;
            if (!aspas && ch == ',') ch = ';';
            tmp[k++] = ch;
        }
        if (i == n) break;
        char a[24], b[24];
        reftok_celula(a, sizeof a, tk[i].c1, tk[i].ac1, tk[i].l1, tk[i].al1);
        char r[96];
        if (tk[i].intervalo) {
            reftok_celula(b, sizeof b, tk[i].c2, tk[i].ac2, tk[i].l2, tk[i].al2);
            formatar_texto(r, sizeof r, "[%s%s.%s:.%s]", tk[i].folha[0] ? "$" : "", tk[i].folha, a, b);
        } else formatar_texto(r, sizeof r, "[%s%s.%s]", tk[i].folha[0] ? "$" : "", tk[i].folha, a);
        for (int q = 0; r[q] && k < (int)sizeof tmp - 2; q++) tmp[k++] = r[q];
        p = tk[i].fim;
    }
    tmp[k] = 0;
    xml_esc(buf, tam, cap, tmp);
}

/* De OpenFormula a Macaco: «of:=SUM([.A1:.B2];[$Hoja2.C3])» -> «SUM(A1:B2;Hoja2!C3)». */
static void formula_de_ods(const char *src, char *dst, int max)
{
    if (comeca_com(src, "of:")) src += 3;
    else if (comeca_com(src, "oooc:")) src += 5;
    int k = 0;
    if (src[0] == '=') src++;
    dst[k++] = '=';
    bool aspas = false;
    for (int i = 0; src[i] && k < max - 2; i++) {
        char ch = src[i];
        if (ch == '"') aspas = !aspas;
        if (!aspas && ch == '[') {
            int j = i + 1;
            if (src[j] == '$') j++;
            const char *ini = src + j;
            const char *pt = ini;
            while (*pt && *pt != '.' && *pt != ']') pt++;
            if (*pt == '.' && pt > ini) {                 /* «Hoja.» delante */
                for (const char *q = ini; q < pt && k < max - 2; q++) dst[k++] = *q;
                dst[k++] = '!';
            }
            if (*pt == '.') pt++;
            for (; *pt && *pt != ']' && k < max - 2; pt++) {
                if (*pt == '.' && pt[-1] == ':') continue;   /* el «.» del segundo extremo */
                dst[k++] = *pt;
            }
            i = (int)(pt - src);                           /* en el «]» */
            continue;
        }
        dst[k++] = ch;
    }
    dst[k] = 0;
}

/* ------------------------------------------------------------ XLSX: escribir */

static void xlsx_estilos(char **b, int *t, int *c)
{
    int nf = livro.n_fmts < 1 ? 1 : livro.n_fmts;
    txt_add(b, t, c, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">");
    /* formatos de número propios: 164 + índice */
    int n_nf = 0;
    for (int i = 0; i < nf; i++) { char cod[48]; if (nf_codigo(&livro.fmts[i], cod, sizeof cod)) n_nf++; }
    if (n_nf) {
        txt_addf(b, t, c, "<numFmts count=\"%d\">", n_nf);
        for (int i = 0; i < nf; i++) {
            char cod[48];
            if (!nf_codigo(&livro.fmts[i], cod, sizeof cod)) continue;
            txt_addf(b, t, c, "<numFmt numFmtId=\"%d\" formatCode=\"", 164 + i);
            xml_esc(b, t, c, cod);
            txt_add(b, t, c, "\"/>");
        }
        txt_add(b, t, c, "</numFmts>");
    }
    txt_addf(b, t, c, "<fonts count=\"%d\">", nf);
    for (int i = 0; i < nf; i++) {
        const fmt_t *f = &livro.fmts[i];
        char rgb[8];
        rgb_hex(rgb, f->cor);
        txt_add(b, t, c, "<font>");
        if (f->est & EST_NEGRITA) txt_add(b, t, c, "<b/>");
        if (f->est & EST_CURSIVA) txt_add(b, t, c, "<i/>");
        if (f->est & EST_SUBLIN) txt_add(b, t, c, "<u/>");
        txt_addf(b, t, c, "<sz val=\"%d\"/><color rgb=\"FF%s\"/><name val=\"%s\"/></font>",
                 f->tam ? f->tam : 10, rgb, FONTES_XML[f->fam < 3 ? f->fam : 0]);
    }
    txt_add(b, t, c, "</fonts>");
    txt_addf(b, t, c, "<fills count=\"%d\"><fill><patternFill patternType=\"none\"/></fill><fill><patternFill patternType=\"gray125\"/></fill>", 2 + nf);
    for (int i = 0; i < nf; i++) {
        const fmt_t *f = &livro.fmts[i];
        char rgb[8];
        rgb_hex(rgb, f->rel);
        if (f->rel & REL_ATIVO) txt_addf(b, t, c, "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FF%s\"/><bgColor indexed=\"64\"/></patternFill></fill>", rgb);
        else txt_add(b, t, c, "<fill><patternFill patternType=\"none\"/></fill>");
    }
    txt_add(b, t, c, "</fills>");
    txt_addf(b, t, c, "<borders count=\"%d\">", nf);
    for (int i = 0; i < nf; i++) {
        int bo = livro.fmts[i].bor;
        txt_addf(b, t, c, "<border>%s%s%s%s<diagonal/></border>",
                 (bo & BOR_ESQ) ? "<left style=\"thin\"><color auto=\"1\"/></left>" : "<left/>",
                 (bo & BOR_DIR) ? "<right style=\"thin\"><color auto=\"1\"/></right>" : "<right/>",
                 (bo & BOR_CIMA) ? "<top style=\"thin\"><color auto=\"1\"/></top>" : "<top/>",
                 (bo & BOR_BAIXO) ? "<bottom style=\"thin\"><color auto=\"1\"/></bottom>" : "<bottom/>");
    }
    txt_add(b, t, c, "</borders><cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>");
    txt_addf(b, t, c, "<cellXfs count=\"%d\">", nf);
    for (int i = 0; i < nf; i++) {
        const fmt_t *f = &livro.fmts[i];
        char cod[48];
        int numid = nf_codigo(f, cod, sizeof cod) ? 164 + i : 0;
        txt_addf(b, t, c, "<xf numFmtId=\"%d\" fontId=\"%d\" fillId=\"%d\" borderId=\"%d\" xfId=\"0\" applyNumberFormat=\"1\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\"",
                 numid, i, (f->rel & REL_ATIVO) ? 2 + i : 0, i);
        if (f->al || f->aj) {           /* ajustar texto (D345): wrapText */
            txt_add(b, t, c, " applyAlignment=\"1\"><alignment");
            if (f->al) txt_addf(b, t, c, " horizontal=\"%s\"", f->al == AL_ESQ ? "left" : (f->al == AL_CENTRO ? "center" : "right"));
            if (f->aj) txt_add(b, t, c, " wrapText=\"1\"");
            txt_add(b, t, c, "/></xf>");
        }
        else txt_add(b, t, c, "/>");
    }
    txt_add(b, t, c, "</cellXfs><cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles></styleSheet>");
}

static void xlsx_hoja(folha_t *fo, char **b, int *t, int *c)
{
    txt_add(b, t, c, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">");
    int ul, uc;
    folha_extremos(fo, &ul, &uc, false);
    if (ul < 0) { ul = 0; uc = 0; }
    char r2[16];
    nome_ref(r2, sizeof r2, ul, uc);
    txt_addf(b, t, c, "<dimension ref=\"A1:%s\"/>", r2);
    if (fo->cong_l || fo->cong_c) {
        char tl[16];
        nome_ref(tl, sizeof tl, fo->cong_l, fo->cong_c);
        txt_add(b, t, c, "<sheetViews><sheetView workbookViewId=\"0\"><pane");
        if (fo->cong_c) txt_addf(b, t, c, " xSplit=\"%d\"", fo->cong_c);
        if (fo->cong_l) txt_addf(b, t, c, " ySplit=\"%d\"", fo->cong_l);
        txt_addf(b, t, c, " topLeftCell=\"%s\" activePane=\"%s\" state=\"frozen\"/></sheetView></sheetViews>", tl,
                 fo->cong_l && fo->cong_c ? "bottomRight" : (fo->cong_l ? "bottomLeft" : "topRight"));
    }
    txt_add(b, t, c, "<sheetFormatPr defaultRowHeight=\"15\"/>");
    int n_col = 0;
    for (int k = 0; k < MAC_MAX_COLUNAS; k++) if (fo->larg_col[k] != MAC_LARGCOL || fo->col_oculta[k]) n_col++;
    if (n_col) {
        txt_add(b, t, c, "<cols>");
        for (int k = 0; k < MAC_MAX_COLUNAS; k++) {
            if (fo->larg_col[k] == MAC_LARGCOL && !fo->col_oculta[k]) continue;
            int w100 = (fo->larg_col[k] - 5) * 100 / 7;
            if (w100 < 10) w100 = 10;
            txt_addf(b, t, c, "<col min=\"%d\" max=\"%d\" width=\"%d.%02d\" customWidth=\"1\"%s/>", k + 1, k + 1,
                     w100 / 100, w100 % 100, fo->col_oculta[k] ? " hidden=\"1\"" : "");
        }
        txt_add(b, t, c, "</cols>");
    }
    txt_add(b, t, c, "<sheetData>");
    int ip = 0;                         /* siguiente fila con altura propia */
    int l = 0, cc = -1, fila_abierta = -1;
    celula_t *ce;
#define XLSX_FILA_ATTR(lp_) do { \
        if ((lp_)->alt) txt_addf(b, t, c, " ht=\"%d.%02d\" customHeight=\"1\"", (lp_)->alt * 75 / 100, (lp_)->alt * 75 % 100); \
        if ((lp_)->oculta) txt_add(b, t, c, " hidden=\"1\""); } while (0)
    for (;;) {
        bool hay = it_proximo(fo, &l, &cc, &ce);
        int fila = hay ? l : MAC_MAX_LINHAS;
        /* las filas con altura propia que quedan antes de esta */
        while (ip < fo->n_lp && fo->lp[ip].l < fila) {
            linha_prop_t *lp = &fo->lp[ip++];
            if (lp->l == fila_abierta) continue;
            if (!lp->alt && !lp->oculta) continue;
            if (fila_abierta >= 0) txt_add(b, t, c, "</row>");
            fila_abierta = -1;
            txt_addf(b, t, c, "<row r=\"%d\"", lp->l + 1);
            XLSX_FILA_ATTR(lp);
            txt_add(b, t, c, "/>");
        }
        if (!hay) break;
        if (fila != fila_abierta) {
            if (fila_abierta >= 0) txt_add(b, t, c, "</row>");
            txt_addf(b, t, c, "<row r=\"%d\"", fila + 1);
            linha_prop_t *lp = lp_buscar(fo, fila);
            if (lp) XLSX_FILA_ATTR(lp);
            txt_add(b, t, c, ">");
            fila_abierta = fila;
            while (ip < fo->n_lp && fo->lp[ip].l <= fila) ip++;
        }
        char ref[16];
        nome_ref(ref, sizeof ref, l, cc);
        txt_addf(b, t, c, "<c r=\"%s\"", ref);
        if (ce->fmt) txt_addf(b, t, c, " s=\"%d\"", ce->fmt);
        if (!ce->texto) { txt_add(b, t, c, "/>"); continue; }
        if (ce->flags & CEL_FORMULA) {
            bool texto = (ce->flags & CEL_TEXTO) != 0;
            txt_add(b, t, c, texto ? " t=\"str\"><f>" : "><f>");
            formula_para_xlsx(inicio_formula(ce->texto), b, t, c);
            txt_add(b, t, c, "</f><v>");
            if (texto) xml_esc(b, t, c, tc_buscar(ce));
            else { char v[32]; formatar_numero(v, sizeof v, ce->valor); txt_add(b, t, c, v); }
            txt_add(b, t, c, "</v></c>");
        } else if ((ce->flags & CEL_NUMERO) && !(ce->flags & CEL_TEXTO)) {
            char v[32];
            formatar_numero(v, sizeof v, ce->valor);
            txt_addf(b, t, c, "><v>%s</v></c>", v);
        } else {
            txt_add(b, t, c, " t=\"inlineStr\"><is><t xml:space=\"preserve\">");
            xml_esc(b, t, c, ce->texto);
            txt_add(b, t, c, "</t></is></c>");
        }
    }
#undef XLSX_FILA_ATTR
    if (fila_abierta >= 0) txt_add(b, t, c, "</row>");
    txt_add(b, t, c, "</sheetData>");
    if (fo->n_fu) {
        txt_addf(b, t, c, "<mergeCells count=\"%d\">", fo->n_fu);
        for (int i = 0; i < fo->n_fu; i++) {
            char a[16], z[16];
            nome_ref(a, sizeof a, fo->fu[i].l1, fo->fu[i].c1);
            nome_ref(z, sizeof z, fo->fu[i].l2, fo->fu[i].c2);
            txt_addf(b, t, c, "<mergeCell ref=\"%s:%s\"/>", a, z);
        }
        txt_add(b, t, c, "</mergeCells>");
    }
    txt_add(b, t, c, "</worksheet>");
}

static bool zip_texto(zip_escritor_t *z, const char *nome, const char *txt, int n, int nivel)
{
    return zip_adicionar(z, nome, (const uint8_t *)txt, (uint32_t)n, hora_utc(NULL), nivel, false, 0644) == 0;
}

static bool exportar_xlsx(const char *destino)
{
    zip_escritor_t z;
    memset(&z, 0, sizeof z);
    bool ok = true;
    char *b = NULL; int t = 0, c = 0;

    txt_add(&b, &t, &c, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
            "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
            "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
            "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
            "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>");
    for (int f = 0; f < livro.n; f++)
        txt_addf(&b, &t, &c, "<Override PartName=\"/xl/worksheets/sheet%d.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>", f + 1);
    txt_add(&b, &t, &c, "</Types>");
    ok = ok && zip_texto(&z, "[Content_Types].xml", b, t, 6);

    t = 0; if (b) b[0] = 0;
    txt_add(&b, &t, &c, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
            "</Relationships>");
    ok = ok && zip_texto(&z, "_rels/.rels", b, t, 6);

    t = 0; if (b) b[0] = 0;
    txt_add(&b, &t, &c, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
            "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets>");
    for (int f = 0; f < livro.n; f++) {
        txt_add(&b, &t, &c, "<sheet name=\"");
        xml_esc(&b, &t, &c, livro.folha[f].nome);
        txt_addf(&b, &t, &c, "\" sheetId=\"%d\" r:id=\"rId%d\"/>", f + 1, f + 1);
    }
    txt_add(&b, &t, &c, "</sheets></workbook>");
    ok = ok && zip_texto(&z, "xl/workbook.xml", b, t, 6);

    t = 0; if (b) b[0] = 0;
    txt_add(&b, &t, &c, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">");
    for (int f = 0; f < livro.n; f++)
        txt_addf(&b, &t, &c, "<Relationship Id=\"rId%d\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet%d.xml\"/>", f + 1, f + 1);
    txt_addf(&b, &t, &c, "<Relationship Id=\"rId%d\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/></Relationships>", livro.n + 1);
    ok = ok && zip_texto(&z, "xl/_rels/workbook.xml.rels", b, t, 6);

    t = 0; if (b) b[0] = 0;
    xlsx_estilos(&b, &t, &c);
    ok = ok && zip_texto(&z, "xl/styles.xml", b, t, 6);

    for (int f = 0; f < livro.n && ok; f++) {
        t = 0; if (b) b[0] = 0;
        xlsx_hoja(&livro.folha[f], &b, &t, &c);
        char nome[40];
        formatar_texto(nome, sizeof nome, "xl/worksheets/sheet%d.xml", f + 1);
        ok = zip_texto(&z, nome, b, t, 6);
    }
    if (b) libera(b);
    if (ok) ok = zip_concluir(&z) == 0 && escrever_buffer(destino, z.b.d, z.b.n);
    zip_escritor_liberar(&z);
    return ok;
}

/* ------------------------------------------------------------ ZIP: lectura */

typedef struct { zip_leitor_t z; char *fich; } zpac_t;

static bool zpac_abrir(zpac_t *p, const char *ruta)
{
    int tam = 0;
    memset(p, 0, sizeof *p);
    p->fich = ler_ficheiro(ruta, &tam);
    if (!p->fich) return false;
    if (zip_abrir(&p->z, (const uint8_t *)p->fich, tam) != 0) { libera(p->fich); p->fich = NULL; return false; }
    return true;
}

static void zpac_cerrar(zpac_t *p)
{
    zip_leitor_liberar(&p->z);
    if (p->fich) libera(p->fich);
    p->fich = NULL;
}

/* El contenido de una parte, ya en Latin-1 y terminado en 0; NULL si no está. */
static char *zpac_parte(zpac_t *p, const char *nome, int *n)
{
    for (int i = 0; i < p->z.cnt; i++) {
        if (compara_texto(p->z.e[i].nome, nome) != 0) continue;
        uint8_t *o = NULL;
        int r = zip_extrair(&p->z, i, &o);
        if (r < 0) return NULL;
        o[r] = 0;
        r = (int)utf8_para_latin1((char *)o, (size_t)r, NULL);
        o[r] = 0;
        if (n) *n = r;
        return (char *)o;
    }
    return NULL;
}

/* Prepara el libro para cargar uno nuevo (como en el .mxs y el CSV). */
static void libro_novo_para_importar(void)
{
    for (int f = 0; f < livro.n; f++) folha_liberar(&livro.folha[f]);
    memset(&livro, 0, sizeof livro);
    livro.n_fmts = 1;
    graficos_limpar();
    desfazer_limpar();
    copia_limpar();
}

/* ------------------------------------------------------------ XLSX: leer */

#define XL_MAX_XF 512
static uint8_t xl_xf_fmt[XL_MAX_XF];     /* de cada «xf» del libro al formato de Macaco */

static void xlsx_estilos_ler(char *x, int n)
{
    memset(xl_xf_fmt, 0, sizeof xl_xf_fmt);
    typedef struct { int id; char cod[40]; } nfc_t;
    nfc_t *nfs = aloca_zerado(64 * sizeof(nfc_t));
    int n_nfs = 0;
    fmt_t *fontes = aloca_zerado(64 * sizeof(fmt_t));
    uint32_t *rellenos = aloca_zerado(128 * sizeof(uint32_t));
    uint8_t *bordas = aloca_zerado(128);
    int n_fontes = 0, n_rellenos = 0, n_bordas = 0;
    if (!nfs || !fontes || !rellenos || !bordas) goto fim;
    xr_t xr = { x, x + n };
    xtok_t t;
    int sec = 0;            /* 1 numFmts, 2 fonts, 3 fills, 4 borders, 5 cellXfs */
    int cur_font = -1, cur_fill = -1, cur_bor = -1, n_xf = 0;
    int xf_atual = -1;
    while (xml_prox(&xr, &t)) {
        if (t.tipo == 1) {
            if (xml_nome_es(&t, "numFmts")) sec = 1;
            else if (xml_nome_es(&t, "fonts")) sec = 2;
            else if (xml_nome_es(&t, "fills")) sec = 3;
            else if (xml_nome_es(&t, "borders")) sec = 4;
            else if (xml_nome_es(&t, "cellXfs")) sec = 5;
            else if (xml_nome_es(&t, "cellStyleXfs") || xml_nome_es(&t, "cellStyles")) sec = 0;
            else if (sec == 1 && xml_nome_es(&t, "numFmt") && n_nfs < 64) {
                nfs[n_nfs].id = xml_attr_int(&t, "numFmtId", 0);
                if (!xml_attr(&t, "formatCode", nfs[n_nfs].cod, sizeof nfs[n_nfs].cod)) nfs[n_nfs].cod[0] = 0;
                n_nfs++;
            } else if (sec == 2 && xml_nome_es(&t, "font") && n_fontes < 64) { cur_font = n_fontes++; }
            else if (sec == 2 && cur_font >= 0) {
                fmt_t *f = &fontes[cur_font];
                char v[48];
                if (xml_nome_es(&t, "b")) f->est |= EST_NEGRITA;
                else if (xml_nome_es(&t, "i")) f->est |= EST_CURSIVA;
                else if (xml_nome_es(&t, "u")) f->est |= EST_SUBLIN;
                else if (xml_nome_es(&t, "sz")) { int s = xml_attr_int(&t, "val", 10); f->tam = (uint8_t)(s == 10 ? 0 : (s > 72 ? 72 : s)); }
                else if (xml_nome_es(&t, "color") && xml_attr(&t, "rgb", v, sizeof v)) { uint32_t c = hex_rgb(v); f->cor = (c == 0) ? 0 : c; }
                else if (xml_nome_es(&t, "name") && xml_attr(&t, "val", v, sizeof v)) {
                    if (acha_texto(v, "Times") || acha_texto(v, "Georgia") || acha_texto(v, "Cambria")) f->fam = 1;
                    else if (acha_texto(v, "Courier") || acha_texto(v, "Consolas") || acha_texto(v, "Mono")) f->fam = 2;
                }
            } else if (sec == 3 && xml_nome_es(&t, "fill") && n_rellenos < 128) { cur_fill = n_rellenos++; }
            else if (sec == 3 && cur_fill >= 0 && xml_nome_es(&t, "fgColor")) {
                char v[24];
                if (xml_attr(&t, "rgb", v, sizeof v)) rellenos[cur_fill] = REL_ATIVO | hex_rgb(v);
            } else if (sec == 3 && cur_fill >= 0 && xml_nome_es(&t, "patternFill")) {
                char v[24];
                if (xml_attr(&t, "patternType", v, sizeof v) && compara_texto(v, "solid") != 0) rellenos[cur_fill] = 0;
                else if (xml_attr(&t, "patternType", v, sizeof v)) rellenos[cur_fill] = REL_ATIVO | 0xC0C0C0;   /* se corrige con fgColor */
            } else if (sec == 4 && xml_nome_es(&t, "border") && n_bordas < 128) { cur_bor = n_bordas++; }
            else if (sec == 4 && cur_bor >= 0 && !t.vazia) {
                char v[24];
                int bit = 0;
                if (xml_nome_es(&t, "left")) bit = BOR_ESQ; else if (xml_nome_es(&t, "right")) bit = BOR_DIR;
                else if (xml_nome_es(&t, "top")) bit = BOR_CIMA; else if (xml_nome_es(&t, "bottom")) bit = BOR_BAIXO;
                if (bit && xml_attr(&t, "style", v, sizeof v) && v[0]) bordas[cur_bor] |= (uint8_t)bit;
            } else if (sec == 5 && xml_nome_es(&t, "xf") && n_xf < XL_MAX_XF) {
                fmt_t f;
                memset(&f, 0, sizeof f);
                int fi = xml_attr_int(&t, "fontId", 0), fl = xml_attr_int(&t, "fillId", 0), bo = xml_attr_int(&t, "borderId", 0);
                int nid = xml_attr_int(&t, "numFmtId", 0);
                if (fi >= 0 && fi < n_fontes) f = fontes[fi];
                if (fl >= 0 && fl < n_rellenos) f.rel = rellenos[fl];
                if (bo >= 0 && bo < n_bordas) f.bor = bordas[bo];
                const char *cod = "";
                for (int k = 0; k < n_nfs; k++) if (nfs[k].id == nid) cod = nfs[k].cod;
                if (nid == 1) cod = "0"; else if (nid == 2) cod = "0.00"; else if (nid == 3) cod = "#,##0";
                else if (nid == 4) cod = "#,##0.00"; else if (nid == 9) cod = "0%"; else if (nid == 10) cod = "0.00%";
                nf_de_codigo(cod, &f);
                xf_atual = n_xf;
                int idx = fmt_internar_livro(&f);
                xl_xf_fmt[n_xf++] = (uint8_t)(idx < 0 ? 0 : idx);
                if (t.vazia) xf_atual = -1;
            } else if (sec == 5 && xf_atual >= 0 && xml_nome_es(&t, "alignment")) {
                char v[24], w[8];
                bool tiene_h = xml_attr(&t, "horizontal", v, sizeof v);
                bool ajusta = xml_attr(&t, "wrapText", w, sizeof w) && w[0] == '1';
                if (tiene_h || ajusta) {
                    fmt_t f = livro.fmts[xl_xf_fmt[xf_atual]];
                    if (tiene_h) f.al = compara_texto(v, "left") == 0 ? AL_ESQ : (compara_texto(v, "center") == 0 ? AL_CENTRO : (compara_texto(v, "right") == 0 ? AL_DIR : 0));
                    if (ajusta) f.aj = 1;
                    int idx = fmt_internar_livro(&f);
                    if (idx >= 0) xl_xf_fmt[xf_atual] = (uint8_t)idx;
                }
            }
        } else if (t.tipo == 2) {
            if (xml_nome_es(&t, "font")) cur_font = -1;
            else if (xml_nome_es(&t, "fill")) cur_fill = -1;
            else if (xml_nome_es(&t, "border")) cur_bor = -1;
            else if (xml_nome_es(&t, "xf")) xf_atual = -1;
        }
    }
fim:
    if (nfs) libera(nfs);
    if (fontes) libera(fontes);
    if (rellenos) libera(rellenos);
    if (bordas) libera(bordas);
}

/* Las cadenas compartidas: un vector de textos (cada <si> con todos sus <t>). */
static char **xlsx_cadenas_ler(char *x, int n, int *cuenta)
{
    int cap = 64, k = 0;
    char **v = aloca_zerado((size_t)cap * sizeof(char *));
    *cuenta = 0;
    if (!v) return NULL;
    xr_t xr = { x, x + n };
    xtok_t t;
    char acum[MAC_TEXTO_CEL];
    int na = 0;
    bool en_si = false, en_t = false, en_ph = false;
    while (xml_prox(&xr, &t)) {
        if (t.tipo == 1) {
            if (xml_nome_es(&t, "si")) { en_si = true; na = 0; acum[0] = 0; }
            else if (xml_nome_es(&t, "rPh")) en_ph = true;
            else if (en_si && xml_nome_es(&t, "t") && !t.vazia) en_t = true;
        } else if (t.tipo == 2) {
            if (xml_nome_es(&t, "t")) en_t = false;
            else if (xml_nome_es(&t, "rPh")) en_ph = false;
            else if (xml_nome_es(&t, "si")) {
                en_si = false;
                if (k >= cap) {
                    cap *= 2;
                    char **nv = realoca(v, (size_t)cap * sizeof(char *));
                    if (!nv) break;
                    v = nv;
                }
                v[k++] = duplicar_texto(acum);
            }
        } else if (t.tipo == 3 && en_t && !en_ph) {
            int d = xml_decod(t.txt, t.txt_n, acum + na, (int)sizeof acum - na);
            na += d;
        }
    }
    *cuenta = k;
    return v;
}

static void xlsx_hoja_ler(folha_t *fo, char *x, int n, char **cadenas, int n_cadenas)
{
    xr_t xr = { x, x + n };
    xtok_t t;
    int cur_l = -1, cur_c = -1, cur_s = 0;
    char cur_t[16];
    cur_t[0] = 0;
    char valor[MAC_TEXTO_CEL], formula[MAC_TEXTO_CEL], textoin[MAC_TEXTO_CEL];
    valor[0] = formula[0] = textoin[0] = 0;
    int fila_l = 0;
    int campo = 0;     /* 1 = <v>, 2 = <f>, 3 = <t> de inlineStr */
    bool en_celda = false;
    while (xml_prox(&xr, &t)) {
        if (t.tipo == 1) {
            if (xml_nome_es(&t, "row")) {
                fila_l = xml_attr_int(&t, "r", fila_l + 1) - 1;
                char v[24];
                int ht100 = 0;
                if (xml_attr(&t, "ht", v, sizeof v)) { int32_t cent; if (parse_numero(v, &cent)) ht100 = cent; }
                bool custom = xml_attr_int(&t, "customHeight", 0) != 0;
                bool oculta = xml_attr_int(&t, "hidden", 0) != 0;
                if (fila_l >= 0 && fila_l < MAC_MAX_LINHAS && ((ht100 && custom) || oculta)) {
                    linha_prop_t *lp = lp_obter(fo, fila_l);
                    if (lp) {
                        if (custom && ht100) { int px = ht100 * 4 / 300; lp->alt = (uint16_t)(px < 4 ? 4 : (px > 400 ? 400 : px)); }
                        lp->oculta = oculta;
                    }
                }
            } else if (xml_nome_es(&t, "col")) {
                int mn = xml_attr_int(&t, "min", 1) - 1, mx = xml_attr_int(&t, "max", mn + 1) - 1;
                char v[24];
                int px = 0;
                if (xml_attr(&t, "width", v, sizeof v)) { int32_t cent; if (parse_numero(v, &cent)) px = (cent * 7 + 50) / 100 + 5; }
                bool oculta = xml_attr_int(&t, "hidden", 0) != 0;
                for (int c = mn; c <= mx && c < MAC_MAX_COLUNAS; c++) {
                    if (c < 0) continue;
                    if (px) fo->larg_col[c] = px < 16 ? 16 : (px > 400 ? 400 : px);
                    fo->col_oculta[c] = oculta;
                }
            } else if (xml_nome_es(&t, "pane")) {
                char v[24];
                if (xml_attr(&t, "state", v, sizeof v) && acha_texto(v, "frozen")) {
                    fo->cong_c = xml_attr_int(&t, "xSplit", 0);
                    fo->cong_l = xml_attr_int(&t, "ySplit", 0);
                }
            } else if (xml_nome_es(&t, "mergeCell")) {
                char v[40];
                if (xml_attr(&t, "ref", v, sizeof v)) {
                    const char *q = v;
                    int c1, l1, c2, l2;
                    if (ler_coluna(&q, &c1) && ler_numero_linha(&q, &l1) && *q == ':') {
                        q++;
                        if (ler_coluna(&q, &c2) && ler_numero_linha(&q, &l2)) fusao_juntar(fo, l1, c1, l2, c2);
                    }
                }
            } else if (xml_nome_es(&t, "c")) {
                char r[24];
                cur_l = -1;
                if (xml_attr(&t, "r", r, sizeof r)) {
                    const char *q = r;
                    int col, lin;
                    if (ler_coluna(&q, &col) && ler_numero_linha(&q, &lin)) { cur_l = lin; cur_c = col; }
                }
                cur_s = xml_attr_int(&t, "s", 0);
                if (!xml_attr(&t, "t", cur_t, sizeof cur_t)) cur_t[0] = 0;
                valor[0] = formula[0] = textoin[0] = 0;
                en_celda = !t.vazia;
                campo = 0;
                if (t.vazia && cur_l >= 0 && cur_s > 0 && cur_s < XL_MAX_XF && xl_xf_fmt[cur_s]) {
                    celula_t *ce = celula_obter(fo, cur_l, cur_c);
                    if (ce) ce->fmt = xl_xf_fmt[cur_s];
                }
            } else if (en_celda && xml_nome_es(&t, "v")) campo = 1;
            else if (en_celda && xml_nome_es(&t, "f")) campo = 2;
            else if (en_celda && xml_nome_es(&t, "t")) campo = 3;
        } else if (t.tipo == 3) {
            if (!en_celda || !campo) continue;
            char *d = campo == 1 ? valor : (campo == 2 ? formula : textoin);
            int ln = (int)tam_texto(d);
            xml_decod(t.txt, t.txt_n, d + ln, MAC_TEXTO_CEL - ln);
        } else if (t.tipo == 2) {
            if (xml_nome_es(&t, "v") || xml_nome_es(&t, "f") || xml_nome_es(&t, "t")) campo = 0;
            else if (xml_nome_es(&t, "c") && en_celda) {
                en_celda = false;
                if (cur_l < 0) continue;
                char conteudo[MAC_TEXTO_CEL];
                conteudo[0] = 0;
                bool texto = false;
                if (formula[0]) { conteudo[0] = '='; copia_texto(conteudo + 1, formula, sizeof conteudo - 1); }
                else if (compara_texto(cur_t, "s") == 0) {
                    int i = texto_para_int(valor);
                    if (i >= 0 && i < n_cadenas && cadenas[i]) { copia_texto(conteudo, cadenas[i], sizeof conteudo); texto = true; }
                } else if (compara_texto(cur_t, "inlineStr") == 0) { copia_texto(conteudo, textoin, sizeof conteudo); texto = true; }
                else if (compara_texto(cur_t, "str") == 0 || compara_texto(cur_t, "e") == 0) { copia_texto(conteudo, valor, sizeof conteudo); texto = true; }
                else if (compara_texto(cur_t, "b") == 0) copia_texto(conteudo, valor[0] == '1' ? "1" : "0", sizeof conteudo);
                else copia_texto(conteudo, valor, sizeof conteudo);
                (void)texto;
                if (conteudo[0] && celula_definir(fo, cur_l, cur_c, conteudo)) {
                    celula_t *ce = celula_de(fo, cur_l, cur_c);
                    if (ce && cur_s > 0 && cur_s < XL_MAX_XF) ce->fmt = xl_xf_fmt[cur_s];
                } else if (!conteudo[0] && cur_s > 0 && cur_s < XL_MAX_XF && xl_xf_fmt[cur_s]) {
                    celula_t *ce = celula_obter(fo, cur_l, cur_c);
                    if (ce) ce->fmt = xl_xf_fmt[cur_s];
                }
            }
        }
    }
}

static bool importar_xlsx(const char *ruta)
{
    zpac_t p;
    if (!zpac_abrir(&p, ruta)) return false;
    int nw = 0, nr = 0, ns = 0, nst = 0;
    char *wb = zpac_parte(&p, "xl/workbook.xml", &nw);
    char *rels = zpac_parte(&p, "xl/_rels/workbook.xml.rels", &nr);
    char *ss = zpac_parte(&p, "xl/sharedStrings.xml", &ns);
    char *st = zpac_parte(&p, "xl/styles.xml", &nst);
    if (!wb) { if (rels) libera(rels); if (ss) libera(ss); if (st) libera(st); zpac_cerrar(&p); return false; }
    libro_novo_para_importar();
    if (st) xlsx_estilos_ler(st, nst);
    int n_cad = 0;
    char **cad = ss ? xlsx_cadenas_ler(ss, ns, &n_cad) : NULL;

    typedef struct { char id[24]; char dest[80]; } rel_t;
    rel_t *rl = aloca_zerado(32 * sizeof(rel_t));
    int n_rl = 0;
    if (rels && rl) {
        xr_t xr = { rels, rels + nr };
        xtok_t t;
        while (xml_prox(&xr, &t))
            if (t.tipo == 1 && xml_nome_es(&t, "Relationship") && n_rl < 32) {
                xml_attr(&t, "Id", rl[n_rl].id, sizeof rl[n_rl].id);
                xml_attr(&t, "Target", rl[n_rl].dest, sizeof rl[n_rl].dest);
                n_rl++;
            }
    }
    xr_t xr = { wb, wb + nw };
    xtok_t t;
    int n_hojas = 0;
    while (xml_prox(&xr, &t)) {
        if (t.tipo != 1 || !xml_nome_es(&t, "sheet") || livro.n >= MAC_MAX_FOLHAS) continue;
        char nome[MAC_NOME_FOLHA * 2], rid[24], ruta_h[120];
        if (!xml_attr(&t, "name", nome, sizeof nome)) copia_texto(nome, "Hoja", sizeof nome);
        if (!xml_attr(&t, "r:id", rid, sizeof rid)) rid[0] = 0;
        ruta_h[0] = 0;
        for (int i = 0; i < n_rl; i++)
            if (compara_texto(rl[i].id, rid) == 0) {
                const char *d = rl[i].dest;
                if (d[0] == '/') d++;
                else if (!comeca_com(d, "xl/")) { copia_texto(ruta_h, "xl/", sizeof ruta_h); }
                copia_texto(ruta_h + tam_texto(ruta_h), d, sizeof ruta_h - tam_texto(ruta_h));
            }
        if (!ruta_h[0]) formatar_texto(ruta_h, sizeof ruta_h, "xl/worksheets/sheet%d.xml", n_hojas + 1);
        n_hojas++;
        folha_nova(nome);
        int nh = 0;
        char *hx = zpac_parte(&p, ruta_h, &nh);
        if (hx) { xlsx_hoja_ler(&livro.folha[livro.n - 1], hx, nh, cad, n_cad); libera(hx); }
    }
    if (cad) { for (int i = 0; i < n_cad; i++) if (cad[i]) libera(cad[i]); libera(cad); }
    if (rl) libera(rl);
    libera(wb);
    if (rels) libera(rels);
    if (ss) libera(ss);
    if (st) libera(st);
    zpac_cerrar(&p);
    if (livro.n == 0) folha_nova("Hoja1");
    livro.ativa = 0;
    recalcular();
    registrar("macaco: leído XLSX con %d hojas", livro.n);
    return true;
}

/* ------------------------------------------------------------ ODS: escribir */

static void ods_tam(char *dst, int max, int px)
{
    int v = px * 10000 / 96;
    formatar_texto(dst, max, "%d.%04din", v / 10000, v % 10000);
}

static void ods_estilo_celda(char **b, int *t, int *c, int i, const fmt_t *f)
{
    char rgb[8];
    txt_addf(b, t, c, "<style:style style:name=\"ce%d\" style:family=\"table-cell\" style:parent-style-name=\"Default\">", i);
    txt_add(b, t, c, "<style:table-cell-properties");
    if (f->rel & REL_ATIVO) { rgb_hex(rgb, f->rel); txt_addf(b, t, c, " fo:background-color=\"#%s\"", rgb); }
    if (f->bor & BOR_CIMA) txt_add(b, t, c, " fo:border-top=\"0.74pt solid #000000\"");
    if (f->bor & BOR_BAIXO) txt_add(b, t, c, " fo:border-bottom=\"0.74pt solid #000000\"");
    if (f->bor & BOR_ESQ) txt_add(b, t, c, " fo:border-left=\"0.74pt solid #000000\"");
    if (f->bor & BOR_DIR) txt_add(b, t, c, " fo:border-right=\"0.74pt solid #000000\"");
    txt_add(b, t, c, "/>");
    if (f->al) txt_addf(b, t, c, "<style:paragraph-properties fo:text-align=\"%s\"/>",
                        f->al == AL_ESQ ? "start" : (f->al == AL_CENTRO ? "center" : "end"));
    rgb_hex(rgb, f->cor);
    txt_addf(b, t, c, "<style:text-properties fo:color=\"#%s\" fo:font-size=\"%dpt\"", rgb, f->tam ? f->tam : 10);
    if (f->est & EST_NEGRITA) txt_add(b, t, c, " fo:font-weight=\"bold\"");
    if (f->est & EST_CURSIVA) txt_add(b, t, c, " fo:font-style=\"italic\"");
    if (f->est & EST_SUBLIN) txt_add(b, t, c, " style:text-underline-style=\"solid\" style:text-underline-width=\"auto\" style:text-underline-color=\"font-color\"");
    txt_addf(b, t, c, " style:font-name=\"%s\"/></style:style>", FONTES_XML[f->fam < 3 ? f->fam : 0]);
}

static void ods_celda(const celula_t *ce, folha_t *fo, int l, int c, char **b, int *t, int *cap)
{
    (void)fo; (void)l; (void)c;
    if (ce->fmt) txt_addf(b, t, cap, " table:style-name=\"ce%d\"", ce->fmt);
    if (!ce->texto) { txt_add(b, t, cap, "/>"); return; }
    char vis[MAC_TEXTO_CEL];
    celula_exibida(vis, sizeof vis, (celula_t *)ce);
    bool formula = (ce->flags & CEL_FORMULA) != 0;
    bool texto = (ce->flags & CEL_TEXTO) != 0;
    if (formula) {
        txt_add(b, t, cap, " table:formula=\"of:=");
        formula_para_ods(inicio_formula(ce->texto), b, t, cap);
        txt_add(b, t, cap, "\"");
    }
    if (texto) txt_add(b, t, cap, " office:value-type=\"string\"");
    else { char v[32]; formatar_numero(v, sizeof v, ce->valor); txt_addf(b, t, cap, " office:value-type=\"float\" office:value=\"%s\"", v); }
    txt_add(b, t, cap, "><text:p>");
    xml_esc(b, t, cap, texto && !formula ? ce->texto : vis);
    txt_add(b, t, cap, "</text:p></table:table-cell>");
}

static bool ods_fila_en_fusion(folha_t *fo, int l)
{
    for (int i = 0; i < fo->n_fu; i++) if (l >= fo->fu[i].l1 && l <= fo->fu[i].l2) return true;
    return false;
}

static void ods_hoja(folha_t *fo, char **b, int *t, int *c)
{
    txt_add(b, t, c, "<table:table table:name=\"");
    xml_esc(b, t, c, fo->nome);
    txt_add(b, t, c, "\">");
    int ul, uc;
    folha_extremos(fo, &ul, &uc, false);
    for (int i = 0; i < fo->n_fu; i++) {
        if (fo->fu[i].l2 > ul) ul = fo->fu[i].l2;
        if (fo->fu[i].c2 > uc) uc = fo->fu[i].c2;
    }
    for (int i = 0; i < fo->n_lp; i++) if ((fo->lp[i].alt || fo->lp[i].oculta) && fo->lp[i].l > ul) ul = fo->lp[i].l;
    for (int k = MAC_MAX_COLUNAS - 1; k > uc; k--) if (fo->larg_col[k] != MAC_LARGCOL || fo->col_oculta[k]) { uc = k; break; }
    if (ul < 0) ul = 0;
    if (uc < 0) uc = 0;
    /* columnas: tandas con el mismo ancho */
    for (int k = 0; k <= uc;) {
        int j = k;
        while (j + 1 <= uc && fo->larg_col[j + 1] == fo->larg_col[k] && fo->col_oculta[j + 1] == fo->col_oculta[k]) j++;
        txt_addf(b, t, c, "<table:table-column table:style-name=\"co%d\"%s", fo->larg_col[k], fo->col_oculta[k] ? " table:visibility=\"collapse\"" : "");
        if (j > k) txt_addf(b, t, c, " table:number-columns-repeated=\"%d\"", j - k + 1);
        txt_add(b, t, c, "/>");
        k = j + 1;
    }
    int pl = 0, pc = -1;
    celula_t *nx = NULL;
    bool hay = it_proximo(fo, &pl, &pc, &nx);
    for (int l = 0; l <= ul;) {
        linha_prop_t *lp = lp_buscar(fo, l);
        bool especial = hay && pl == l;
        if (!especial && !lp && !ods_fila_en_fusion(fo, l)) {
            /* tanda de filas vacías */
            int fin = hay ? pl : ul + 1;
            for (int i = 0; i < fo->n_lp; i++) if (fo->lp[i].l > l && fo->lp[i].l < fin) fin = fo->lp[i].l;
            for (int i = 0; i < fo->n_fu; i++) if (fo->fu[i].l1 > l && fo->fu[i].l1 < fin) fin = fo->fu[i].l1;
            if (fin > ul + 1) fin = ul + 1;
            if (fin <= l) fin = l + 1;
            txt_add(b, t, c, "<table:table-row");
            if (fin - l > 1) txt_addf(b, t, c, " table:number-rows-repeated=\"%d\"", fin - l);
            txt_addf(b, t, c, "><table:table-cell table:number-columns-repeated=\"%d\"/></table:table-row>", uc + 1);
            l = fin;
            continue;
        }
        txt_add(b, t, c, "<table:table-row");
        if (lp && lp->alt) txt_addf(b, t, c, " table:style-name=\"ro%d\"", lp->alt);
        if (lp && lp->oculta) txt_add(b, t, c, " table:visibility=\"collapse\"");
        txt_add(b, t, c, ">");
        int vacias = 0;
        for (int k = 0; k <= uc; k++) {
            fusao_t *u = fusao_em(fo, l, k);
            bool cubierta = u && !(u->l1 == l && u->c1 == k);
            celula_t *ce = celula_de(fo, l, k);
            bool vacia = !cubierta && (!ce || !celula_cheia(ce)) && !(u && u->l1 == l && u->c1 == k);
            if (vacia) { vacias++; continue; }
            if (vacias) {
                txt_add(b, t, c, "<table:table-cell");
                if (vacias > 1) txt_addf(b, t, c, " table:number-columns-repeated=\"%d\"", vacias);
                txt_add(b, t, c, "/>");
                vacias = 0;
            }
            if (cubierta) { txt_add(b, t, c, "<table:covered-table-cell/>"); continue; }
            txt_add(b, t, c, "<table:table-cell");
            if (u) txt_addf(b, t, c, " table:number-columns-spanned=\"%d\" table:number-rows-spanned=\"%d\"",
                            u->c2 - u->c1 + 1, u->l2 - u->l1 + 1);
            ods_celda(ce ? ce : &celula_vazia_estatica, fo, l, k, b, t, c);
        }
        if (vacias) {
            txt_add(b, t, c, "<table:table-cell");
            if (vacias > 1) txt_addf(b, t, c, " table:number-columns-repeated=\"%d\"", vacias);
            txt_add(b, t, c, "/>");
        }
        txt_add(b, t, c, "</table:table-row>");
        while (hay && pl <= l) hay = it_proximo(fo, &pl, &pc, &nx);
        l++;
    }
    txt_add(b, t, c, "</table:table>");
}

static bool exportar_ods(const char *destino)
{
    zip_escritor_t z;
    memset(&z, 0, sizeof z);
    bool ok = true;
    char *b = NULL; int t = 0, c = 0;
    /* «mimetype» va primero y sin comprimir, como manda el estándar */
    ok = zip_texto(&z, "mimetype", "application/vnd.oasis.opendocument.spreadsheet", 46, 0);

    t = 0;
    txt_add(&b, &t, &c, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<manifest:manifest xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\" manifest:version=\"1.2\">"
            "<manifest:file-entry manifest:full-path=\"/\" manifest:version=\"1.2\" manifest:media-type=\"application/vnd.oasis.opendocument.spreadsheet\"/>"
            "<manifest:file-entry manifest:full-path=\"content.xml\" manifest:media-type=\"text/xml\"/>"
            "</manifest:manifest>");
    ok = ok && zip_texto(&z, "META-INF/manifest.xml", b, t, 6);

    t = 0; if (b) b[0] = 0;
    txt_add(&b, &t, &c, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<office:document-content xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
            "xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\" "
            "xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\" "
            "xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\" "
            "xmlns:fo=\"urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0\" "
            "xmlns:svg=\"urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0\" "
            "xmlns:of=\"urn:oasis:names:tc:opendocument:xmlns:of:1.2\" office:version=\"1.2\">"
            "<office:automatic-styles>");
    /* anchos y alturas distintos que se usan */
    static uint8_t ancho_usado[401], alto_usado[401];
    memset(ancho_usado, 0, sizeof ancho_usado);
    memset(alto_usado, 0, sizeof alto_usado);
    for (int f = 0; f < livro.n; f++) {
        folha_t *fo = &livro.folha[f];
        for (int k = 0; k < MAC_MAX_COLUNAS; k++) { int w = fo->larg_col[k]; if (w >= 0 && w <= 400) ancho_usado[w] = 1; }
        for (int i = 0; i < fo->n_lp; i++) if (fo->lp[i].alt <= 400) alto_usado[fo->lp[i].alt] = 1;
    }
    for (int w = 0; w <= 400; w++) {
        char m[24];
        if (ancho_usado[w]) {
            ods_tam(m, sizeof m, w);
            txt_addf(&b, &t, &c, "<style:style style:name=\"co%d\" style:family=\"table-column\"><style:table-column-properties style:column-width=\"%s\"/></style:style>", w, m);
        }
        if (alto_usado[w] && w > 0) {
            ods_tam(m, sizeof m, w);
            txt_addf(&b, &t, &c, "<style:style style:name=\"ro%d\" style:family=\"table-row\"><style:table-row-properties style:row-height=\"%s\" style:use-optimal-row-height=\"false\"/></style:style>", w, m);
        }
    }
    for (int i = 1; i < livro.n_fmts; i++) ods_estilo_celda(&b, &t, &c, i, &livro.fmts[i]);
    txt_add(&b, &t, &c, "</office:automatic-styles><office:body><office:spreadsheet>");
    for (int f = 0; f < livro.n; f++) ods_hoja(&livro.folha[f], &b, &t, &c);
    txt_add(&b, &t, &c, "</office:spreadsheet></office:body></office:document-content>");
    ok = ok && zip_texto(&z, "content.xml", b, t, 6);
    if (b) libera(b);
    if (ok) ok = zip_concluir(&z) == 0 && escrever_buffer(destino, z.b.d, z.b.n);
    zip_escritor_liberar(&z);
    return ok;
}

/* ------------------------------------------------------------ ODS: leer */

typedef struct { char nome[24]; int tipo; int px; fmt_t f; } ods_est_t;   /* tipo: 1 columna, 2 fila, 3 celda */

static void ods_est_atributos(const xtok_t *t, ods_est_t *e)
{
    char v[96];
    if (xml_nome_es(t, "table-column-properties") && xml_attr(t, "style:column-width", v, sizeof v)) e->px = xml_medida_px(v);
    else if (xml_nome_es(t, "table-row-properties") && xml_attr(t, "style:row-height", v, sizeof v)) e->px = xml_medida_px(v);
    else if (xml_nome_es(t, "table-cell-properties")) {
        if (xml_attr(t, "fo:background-color", v, sizeof v) && v[0] == '#') e->f.rel = REL_ATIVO | hex_rgb(v);
        if (xml_attr(t, "fo:border", v, sizeof v) && acha_texto(v, "solid")) e->f.bor = BOR_CIMA | BOR_BAIXO | BOR_ESQ | BOR_DIR;
        if (xml_attr(t, "fo:border-top", v, sizeof v) && acha_texto(v, "solid")) e->f.bor |= BOR_CIMA;
        if (xml_attr(t, "fo:border-bottom", v, sizeof v) && acha_texto(v, "solid")) e->f.bor |= BOR_BAIXO;
        if (xml_attr(t, "fo:border-left", v, sizeof v) && acha_texto(v, "solid")) e->f.bor |= BOR_ESQ;
        if (xml_attr(t, "fo:border-right", v, sizeof v) && acha_texto(v, "solid")) e->f.bor |= BOR_DIR;
    } else if (xml_nome_es(t, "paragraph-properties")) {
        if (xml_attr(t, "fo:text-align", v, sizeof v)) {
            if (compara_texto(v, "start") == 0 || compara_texto(v, "left") == 0) e->f.al = AL_ESQ;
            else if (compara_texto(v, "center") == 0) e->f.al = AL_CENTRO;
            else if (compara_texto(v, "end") == 0 || compara_texto(v, "right") == 0) e->f.al = AL_DIR;
        }
    } else if (xml_nome_es(t, "text-properties")) {
        if (xml_attr(t, "fo:font-weight", v, sizeof v) && compara_texto(v, "bold") == 0) e->f.est |= EST_NEGRITA;
        if (xml_attr(t, "fo:font-style", v, sizeof v) && compara_texto(v, "italic") == 0) e->f.est |= EST_CURSIVA;
        if (xml_attr(t, "style:text-underline-style", v, sizeof v) && compara_texto(v, "none") != 0) e->f.est |= EST_SUBLIN;
        if (xml_attr(t, "fo:color", v, sizeof v) && v[0] == '#') e->f.cor = hex_rgb(v);
        if (xml_attr(t, "fo:font-size", v, sizeof v)) { int s = xml_medida_pt(v); e->f.tam = (uint8_t)(s == 10 ? 0 : (s < 1 ? 0 : (s > 72 ? 72 : s))); }
        if (xml_attr(t, "style:font-name", v, sizeof v)) {
            if (acha_texto(v, "Times") || acha_texto(v, "Georgia")) e->f.fam = 1;
            else if (acha_texto(v, "Courier") || acha_texto(v, "Mono")) e->f.fam = 2;
        }
    }
}

static bool importar_ods(const char *ruta)
{
    zpac_t p;
    if (!zpac_abrir(&p, ruta)) return false;
    int n = 0;
    char *x = zpac_parte(&p, "content.xml", &n);
    zpac_cerrar(&p);
    if (!x) return false;
    libro_novo_para_importar();

    enum { MAX_EST = 192 };
    ods_est_t *est = aloca_zerado(MAX_EST * sizeof(ods_est_t));
    int n_est = 0;
    if (!est) { libera(x); return false; }
    xr_t xr = { x, x + n };
    xtok_t t;
    ods_est_t *cur = NULL;
    folha_t *fo = NULL;
    int fila = 0, col = 0;
    int col_idx = 0;
    int row_rep = 1;
    bool fila_oculta = false;
    int fila_alt = 0;
    int cel_rep = 1, cel_cs = 1, cel_rs = 1, cel_fmt = 0;
    char cel_tipo[16], cel_valor[48], cel_formula[MAC_TEXTO_CEL], cel_texto[MAC_TEXTO_CEL];
    bool en_celda = false, en_p = false;
    int parrafos = 0;
    while (xml_prox(&xr, &t)) {
        if (t.tipo == 1) {
            if (xml_nome_es(&t, "style") && n_est < MAX_EST) {
                char fam[24];
                cur = &est[n_est++];
                memset(cur, 0, sizeof *cur);
                xml_attr(&t, "style:name", cur->nome, sizeof cur->nome);
                if (xml_attr(&t, "style:family", fam, sizeof fam))
                    cur->tipo = compara_texto(fam, "table-column") == 0 ? 1 : (compara_texto(fam, "table-row") == 0 ? 2 : (compara_texto(fam, "table-cell") == 0 ? 3 : 0));
                if (t.vazia) cur = NULL;
            } else if (cur) ods_est_atributos(&t, cur);
            else if (xml_nome_es(&t, "table") && livro.n < MAC_MAX_FOLHAS) {
                char nome[MAC_NOME_FOLHA * 2];
                if (!xml_attr(&t, "table:name", nome, sizeof nome)) copia_texto(nome, "Hoja", sizeof nome);
                folha_nova(nome);
                fo = &livro.folha[livro.n - 1];
                fila = 0; col_idx = 0;
            } else if (fo && xml_nome_es(&t, "table-column")) {
                char v[48];
                int rep = xml_attr_int(&t, "table:number-columns-repeated", 1);
                int px = 0;
                bool oculta = xml_attr(&t, "table:visibility", v, sizeof v) && compara_texto(v, "visible") != 0;
                if (xml_attr(&t, "table:style-name", v, sizeof v))
                    for (int i = 0; i < n_est; i++) if (est[i].tipo == 1 && compara_texto(est[i].nome, v) == 0) px = est[i].px;
                for (int k = 0; k < rep && col_idx < MAC_MAX_COLUNAS; k++, col_idx++) {
                    if (px) fo->larg_col[col_idx] = px < 16 ? 16 : (px > 400 ? 400 : px);
                    fo->col_oculta[col_idx] = oculta;
                }
            } else if (fo && xml_nome_es(&t, "table-row")) {
                char v[48];
                row_rep = xml_attr_int(&t, "table:number-rows-repeated", 1);
                fila_oculta = xml_attr(&t, "table:visibility", v, sizeof v) && compara_texto(v, "visible") != 0;
                fila_alt = 0;
                if (xml_attr(&t, "table:style-name", v, sizeof v))
                    for (int i = 0; i < n_est; i++) if (est[i].tipo == 2 && compara_texto(est[i].nome, v) == 0) fila_alt = est[i].px;
                col = 0;
                if ((fila_alt > 0 && fila_alt != MAC_ALTLINHA) || fila_oculta) {
                    for (int k = 0; k < row_rep && k < 1000 && fila + k < MAC_MAX_LINHAS; k++) {
                        linha_prop_t *lp = lp_obter(fo, fila + k);
                        if (lp) { lp->alt = (uint16_t)(fila_alt > 400 ? 400 : (fila_alt == MAC_ALTLINHA ? 0 : fila_alt)); lp->oculta = fila_oculta; }
                    }
                }
                if (t.vazia) { fila += row_rep; }
            } else if (fo && xml_nome_es(&t, "covered-table-cell")) {
                col += xml_attr_int(&t, "table:number-columns-repeated", 1);
            } else if (fo && xml_nome_es(&t, "table-cell")) {
                char v[48];
                cel_rep = xml_attr_int(&t, "table:number-columns-repeated", 1);
                cel_cs = xml_attr_int(&t, "table:number-columns-spanned", 1);
                cel_rs = xml_attr_int(&t, "table:number-rows-spanned", 1);
                cel_tipo[0] = cel_valor[0] = cel_formula[0] = cel_texto[0] = 0;
                xml_attr(&t, "office:value-type", cel_tipo, sizeof cel_tipo);
                xml_attr(&t, "office:value", cel_valor, sizeof cel_valor);
                xml_attr(&t, "table:formula", cel_formula, sizeof cel_formula);
                cel_fmt = 0;
                if (xml_attr(&t, "table:style-name", v, sizeof v))
                    for (int i = 0; i < n_est; i++)
                        if (est[i].tipo == 3 && compara_texto(est[i].nome, v) == 0) {
                            int idx = fmt_internar_livro(&est[i].f);
                            cel_fmt = idx < 0 ? 0 : idx;
                        }
                parrafos = 0;
                if (t.vazia) { en_celda = true; goto cerrar_celda; }
                en_celda = true;
            } else if (en_celda && xml_nome_es(&t, "p")) {
                en_p = true;
                if (parrafos++ > 0) { int ln = (int)tam_texto(cel_texto); if (ln < MAC_TEXTO_CEL - 2) { cel_texto[ln] = ' '; cel_texto[ln + 1] = 0; } }
            }
        } else if (t.tipo == 3) {
            if (en_celda && en_p) {
                int ln = (int)tam_texto(cel_texto);
                xml_decod(t.txt, t.txt_n, cel_texto + ln, MAC_TEXTO_CEL - ln);
            }
        } else if (t.tipo == 2) {
            if (xml_nome_es(&t, "style")) cur = NULL;
            else if (xml_nome_es(&t, "p")) en_p = false;
            else if (xml_nome_es(&t, "table-row")) { fila += row_rep; }
            else if (fo && xml_nome_es(&t, "table-cell") && en_celda) {
cerrar_celda:
                en_celda = false;
                {
                    char conteudo[MAC_TEXTO_CEL];
                    conteudo[0] = 0;
                    if (cel_formula[0]) formula_de_ods(cel_formula, conteudo, sizeof conteudo);
                    else if (compara_texto(cel_tipo, "float") == 0 || compara_texto(cel_tipo, "percentage") == 0 || compara_texto(cel_tipo, "currency") == 0)
                        copia_texto(conteudo, cel_valor, sizeof conteudo);
                    else if (cel_tipo[0]) copia_texto(conteudo, cel_texto, sizeof conteudo);
                    int rep = cel_rep;
                    bool con_algo = conteudo[0] || cel_fmt || cel_cs > 1 || cel_rs > 1;
                    if (con_algo && fila < MAC_MAX_LINHAS) {
                        if (rep > 256) rep = 256;
                        for (int k = 0; k < rep && col + k < MAC_MAX_COLUNAS; k++) {
                            for (int rr = 0; rr < row_rep && rr < 64 && fila + rr < MAC_MAX_LINHAS; rr++) {
                                if (conteudo[0]) celula_definir(fo, fila + rr, col + k, conteudo);
                                if (cel_fmt) { celula_t *ce = celula_obter(fo, fila + rr, col + k); if (ce) ce->fmt = (uint8_t)cel_fmt; }
                            }
                        }
                        if (cel_cs > 1 || cel_rs > 1) fusao_juntar(fo, fila, col, fila + cel_rs - 1, col + cel_cs - 1);
                    }
                    col += cel_rep;
                }
            }
        }
    }
    libera(est);
    libera(x);
    if (livro.n == 0) folha_nova("Hoja1");
    livro.ativa = 0;
    recalcular();
    registrar("macaco: leído ODS con %d hojas", livro.n);
    return true;
}

#endif /* MACACO_FORMATOS_H */
