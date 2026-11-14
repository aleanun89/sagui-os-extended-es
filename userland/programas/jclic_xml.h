// SPDX-License-Identifier: GPL-2.0-or-later
/* jclic_xml.h - el analizador de XML del visor de JClic (D185).
 *
 * Un DOM mínimo, propio: elementos con atributos, texto propio y árbol de hijos; sin espacios de
 * nombres ni DTD (los .jclic no los usan). El documento se convierte de UTF-8 a ISO-8859-1 al
 * analizarlo (los textos de la interfaz y la fuente de 8x16 son Latin-1): lo que no cabe se
 * aproxima (comillas, rayas, puntos suspensivos) o sale como '?'. Entiende comentarios, CDATA,
 * <?...?>, <!DOCTYPE>, &amp; &lt; &gt; &quot; &apos; y &#N; / &#xN;.
 *
 * La memoria sale de un "arenero" (arena): se piden bloques grandes y se sueltan todos a la vez,
 * porque un proyecto de cientos de actividades son decenas de miles de nodos pequeños.
 * Cabecera "estática" incluida por jclic.c (no es una biblioteca compartida). */
#ifndef JCLIC_XML_H
#define JCLIC_XML_H
#include "sagui.h"

/* ------------------------------------------------------------ arenero */

typedef struct ar_bloco { struct ar_bloco *ant; size_t cap, uso; } ar_bloco_t;
typedef struct { ar_bloco_t *topo; } arena_t;

static void *ar_pedir(arena_t *a, size_t n)
{
    n = (n + 7) & ~(size_t)7;
    if (!a->topo || a->topo->uso + n > a->topo->cap) {
        size_t cap = n + sizeof(ar_bloco_t) > 65536 ? n + sizeof(ar_bloco_t) : 65536;
        ar_bloco_t *b = aloca_zerado(cap);
        if (!b) return NULL;
        b->ant = a->topo;
        b->cap = cap;
        b->uso = (sizeof(ar_bloco_t) + 7) & ~(size_t)7;
        a->topo = b;
    }
    void *p = (uint8_t *)a->topo + a->topo->uso;
    a->topo->uso += n;
    return p;
}

static void ar_soltar(arena_t *a)
{
    while (a->topo) { ar_bloco_t *p = a->topo->ant; libera(a->topo); a->topo = p; }
}

static char *ar_texto(arena_t *a, const char *s, int n)
{
    char *d = ar_pedir(a, (size_t)n + 1);
    if (!d) return NULL;
    memcpy(d, s, (size_t)n);
    d[n] = 0;
    return d;
}

/* ------------------------------------------------------------ nodos */

typedef struct xn {
    const char *nome;
    const char **at;        /* nombre, valor, nombre, valor... */
    int          nat;
    struct xn   *pai, *filho, *ult, *irmao;
    char        *txt;       /* texto propio, concatenado (sin recortar) */
} xn;

/* De un punto de código Unicode a Latin-1 (lo que no existe se aproxima). */
static int xml_latin1(unsigned cp)
{
    if (cp < 256) return (int)cp;
    switch (cp) {
    case 0x2018: case 0x2019: case 0x201A: return '\'';
    case 0x201C: case 0x201D: case 0x201E: return '"';
    case 0x2013: case 0x2014: case 0x2212: return '-';
    case 0x2026: return '.';
    case 0x20AC: return 'E';
    case 0x2022: return '*';
    case 0x0152: return 'O';
    case 0x0153: return 'o';
    case 0x0160: return 'S';
    case 0x0161: return 's';
    case 0x2192: return '>';
    case 0x2190: return '<';
    case 0x0141: return 'L';
    case 0x0142: return 'l';
    case 0x0107: case 0x010D: return 'c';
    case 0x0106: case 0x010C: return 'C';
    case 0x017E: return 'z';
    case 0x017D: return 'Z';
    default: return '?';
    }
}

/* Decodifica entidades y UTF-8 de [s, s+n) a un texto Latin-1 nuevo en el arenero. */
static char *xml_decodificar(arena_t *ar, const char *s, int n)
{
    char *d = ar_pedir(ar, (size_t)n + 1);
    if (!d) return NULL;
    int o = 0;
    for (int i = 0; i < n;) {
        unsigned char c = (unsigned char)s[i];
        if (c == '&') {
            int j = i + 1;
            while (j < n && j - i < 10 && s[j] != ';') j++;
            if (j < n && s[j] == ';') {
                int ln = j - i - 1;
                const char *e = s + i + 1;
                int r = -1;
                if (ln == 3 && !memcmp(e, "amp", 3)) r = '&';
                else if (ln == 2 && !memcmp(e, "lt", 2)) r = '<';
                else if (ln == 2 && !memcmp(e, "gt", 2)) r = '>';
                else if (ln == 4 && !memcmp(e, "quot", 4)) r = '"';
                else if (ln == 4 && !memcmp(e, "apos", 4)) r = '\'';
                else if (ln == 4 && !memcmp(e, "nbsp", 4)) r = ' ';
                else if (ln >= 2 && e[0] == '#') {
                    unsigned v = 0;
                    if (e[1] == 'x' || e[1] == 'X') {
                        for (int k = 2; k < ln; k++) {
                            char h = e[k];
                            v = v * 16 + (unsigned)(h >= '0' && h <= '9' ? h - '0' : (h | 0x20) >= 'a' && (h | 0x20) <= 'f' ? (h | 0x20) - 'a' + 10 : 0);
                        }
                    } else for (int k = 1; k < ln; k++) v = v * 10 + (unsigned)(e[k] - '0');
                    r = xml_latin1(v);
                }
                if (r >= 0) { d[o++] = (char)r; i = j + 1; continue; }
            }
            d[o++] = '&';
            i++;
            continue;
        }
        if (c >= 0xC2 && c < 0xE0 && i + 1 < n && ((unsigned char)s[i + 1] & 0xC0) == 0x80) {
            d[o++] = (char)xml_latin1(((unsigned)(c & 0x1F) << 6) | ((unsigned char)s[i + 1] & 0x3F));
            i += 2;
        } else if (c >= 0xE0 && c < 0xF0 && i + 2 < n && ((unsigned char)s[i + 1] & 0xC0) == 0x80 &&
                   ((unsigned char)s[i + 2] & 0xC0) == 0x80) {
            d[o++] = (char)xml_latin1(((unsigned)(c & 0x0F) << 12) | (((unsigned char)s[i + 1] & 0x3F) << 6) |
                                      ((unsigned char)s[i + 2] & 0x3F));
            i += 3;
        } else if (c >= 0xF0 && c < 0xF8 && i + 3 < n) {
            d[o++] = '?';
            i += 4;
        } else { d[o++] = (char)c; i++; }   /* ASCII, o un byte Latin-1 suelto */
    }
    d[o] = 0;
    return d;
}

static bool xml_nomec(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == ':' || c == '.'; }
static bool xml_esp(int c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

#define XML_PROF_MAX 64

/* Analiza el documento; devuelve el elemento raíz o NULL (y *erro con el motivo). */
static xn *xml_analisar(arena_t *ar, const char *d, int n, const char **erro)
{
    xn *raiz = NULL, *atual = NULL;
    int i = 0, prof = 0;
    /* BOM */
    if (n >= 3 && (unsigned char)d[0] == 0xEF && (unsigned char)d[1] == 0xBB && (unsigned char)d[2] == 0xBF) i = 3;
    while (i < n) {
        if (d[i] != '<') {   /* texto */
            int j = i;
            while (j < n && d[j] != '<') j++;
            if (atual) {
                char *t = xml_decodificar(ar, d + i, j - i);
                if (!t) { *erro = "sin memoria"; return NULL; }
                if (!atual->txt) atual->txt = t;
                else {   /* concatena */
                    int a1 = (int)tam_texto(atual->txt), a2 = (int)tam_texto(t);
                    char *u = ar_pedir(ar, (size_t)(a1 + a2 + 1));
                    if (!u) { *erro = "sin memoria"; return NULL; }
                    memcpy(u, atual->txt, (size_t)a1);
                    memcpy(u + a1, t, (size_t)a2 + 1);
                    atual->txt = u;
                }
            }
            i = j;
            continue;
        }
        if (i + 3 < n && !memcmp(d + i, "<!--", 4)) {
            int j = i + 4;
            while (j + 2 < n && memcmp(d + j, "-->", 3)) j++;
            i = j + 3;
            continue;
        }
        if (i + 8 < n && !memcmp(d + i, "<![CDATA[", 9)) {
            int j = i + 9;
            while (j + 2 < n && memcmp(d + j, "]]>", 3)) j++;
            if (atual) {
                char *t = ar_texto(ar, d + i + 9, j - i - 9);
                if (!t) { *erro = "sin memoria"; return NULL; }
                if (!atual->txt) atual->txt = t;
            }
            i = j + 3;
            continue;
        }
        if (i + 1 < n && (d[i + 1] == '?' || d[i + 1] == '!')) {
            int j = i + 2;
            int nivel = 0;
            while (j < n && !(d[j] == '>' && nivel == 0)) { if (d[j] == '[') nivel++; if (d[j] == ']') nivel--; j++; }
            i = j + 1;
            continue;
        }
        if (i + 1 < n && d[i + 1] == '/') {   /* cierre */
            int j = i + 2;
            while (j < n && d[j] != '>') j++;
            if (!atual) { *erro = "etiqueta de cierre sobrante"; return NULL; }
            atual = atual->pai;
            prof--;
            i = j + 1;
            continue;
        }
        /* apertura */
        int j = i + 1;
        int ini = j;
        while (j < n && xml_nomec((unsigned char)d[j])) j++;
        if (j == ini) { *erro = "etiqueta mal formada"; return NULL; }
        xn *no = ar_pedir(ar, sizeof(xn));
        if (!no) { *erro = "sin memoria"; return NULL; }
        no->nome = ar_texto(ar, d + ini, j - ini);
        const char *tmp[64];
        int nt = 0;
        bool vazio = false;
        for (;;) {
            while (j < n && xml_esp((unsigned char)d[j])) j++;
            if (j >= n) { *erro = "documento cortado"; return NULL; }
            if (d[j] == '/') { vazio = true; j++; continue; }
            if (d[j] == '>') { j++; break; }
            int a0 = j;
            while (j < n && xml_nomec((unsigned char)d[j])) j++;
            if (j == a0) { *erro = "atributo mal formado"; return NULL; }
            const char *nm = ar_texto(ar, d + a0, j - a0);
            while (j < n && xml_esp((unsigned char)d[j])) j++;
            const char *vl = "";
            if (j < n && d[j] == '=') {
                j++;
                while (j < n && xml_esp((unsigned char)d[j])) j++;
                if (j < n && (d[j] == '"' || d[j] == '\'')) {
                    char q = d[j++];
                    int v0 = j;
                    while (j < n && d[j] != q) j++;
                    vl = xml_decodificar(ar, d + v0, j - v0);
                    j++;
                } else {
                    int v0 = j;
                    while (j < n && !xml_esp((unsigned char)d[j]) && d[j] != '>' && d[j] != '/') j++;
                    vl = xml_decodificar(ar, d + v0, j - v0);
                }
            }
            if (nt < 62) { tmp[nt++] = nm; tmp[nt++] = vl; }
        }
        if (nt) {
            no->at = ar_pedir(ar, (size_t)nt * sizeof(char *));
            if (!no->at) { *erro = "sin memoria"; return NULL; }
            for (int k = 0; k < nt; k++) no->at[k] = tmp[k];
            no->nat = nt / 2;
        }
        no->pai = atual;
        if (atual) {
            if (atual->ult) atual->ult->irmao = no; else atual->filho = no;
            atual->ult = no;
        } else if (!raiz) raiz = no;
        else { *erro = "mas de un elemento raiz"; return NULL; }
        if (!vazio) {
            if (++prof > XML_PROF_MAX * 4) { *erro = "demasiado anidado"; return NULL; }
            atual = no;
        }
        i = j;
    }
    if (!raiz) *erro = "documento vacio";
    return raiz;
}

/* ------------------------------------------------------------ consultas */

static const char *x_at(const xn *no, const char *nome)
{
    if (!no) return NULL;
    for (int k = 0; k < no->nat; k++) if (!compara_texto(no->at[2 * k], nome)) return no->at[2 * k + 1];
    return NULL;
}
static const char *x_atd(const xn *no, const char *nome, const char *def) { const char *v = x_at(no, nome); return v ? v : def; }
static xn *x_irm(const xn *c, const char *nome)   /* el siguiente hermano (desde c, exclusive) con ese nombre */
{
    for (xn *k = c ? c->irmao : NULL; k; k = k->irmao) if (!nome || !compara_texto(k->nome, nome)) return k;
    return NULL;
}
static xn *x_filho(const xn *no, const char *nome)
{
    if (!no) return NULL;
    for (xn *k = no->filho; k; k = k->irmao) if (!nome || !compara_texto(k->nome, nome)) return k;
    return NULL;
}
static int x_int(const xn *no, const char *nome, int def)
{
    const char *v = x_at(no, nome);
    if (!v || !*v) return def;
    bool neg = *v == '-';
    int r = 0;
    for (const char *p = v + (neg || *v == '+'); *p >= '0' && *p <= '9'; p++) r = r * 10 + (*p - '0');
    return neg ? -r : r;
}
static bool x_bool(const xn *no, const char *nome, bool def)
{
    const char *v = x_at(no, nome);
    if (!v) return def;
    return !compara_texto(v, "true") || !compara_texto(v, "1");
}
/* Colores de JClic: "0xRRGGBB", "#RRGGBB" o nombres comunes. */
static uint32_t x_cor(const char *v, uint32_t def)
{
    if (!v || !*v) return def;
    const char *p = v;
    if (p[0] == '#') p++;
    else if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    else {
        static const struct { const char *n; uint32_t c; } nomes[] = {
            { "black", 0x000000 }, { "white", 0xFFFFFF }, { "red", 0xFF0000 }, { "green", 0x00FF00 }, { "blue", 0x0000FF },
            { "yellow", 0xFFFF00 }, { "cyan", 0x00FFFF }, { "magenta", 0xFF00FF }, { "gray", 0x808080 }, { "orange", 0xFFC800 },
            { "pink", 0xFFAFAF }, { "lightGray", 0xC0C0C0 }, { "darkGray", 0x404040 },
        };
        for (unsigned k = 0; k < sizeof nomes / sizeof nomes[0]; k++) if (!compara_texto(v, nomes[k].n)) return nomes[k].c;
        return def;
    }
    uint32_t r = 0;
    int nd = 0;
    for (; *p; p++, nd++) {
        int h = *p >= '0' && *p <= '9' ? *p - '0' : (*p | 0x20) >= 'a' && (*p | 0x20) <= 'f' ? (*p | 0x20) - 'a' + 10 : -1;
        if (h < 0) break;
        r = (r << 4) | (uint32_t)h;
    }
    return nd ? (r & 0xFFFFFF) : def;
}
/* El texto de un nodo con párrafos <p>: los párrafos van separados por '\n'; sin <p>, el texto propio
 * recortado. Devuelve un texto nuevo en el arenero (nunca NULL salvo sin memoria). */
static char *x_paragrafos(arena_t *ar, const xn *no)
{
    int tot = 1;
    bool tem_p = false;
    for (xn *k = no ? no->filho : NULL; k; k = k->irmao)
        if (!compara_texto(k->nome, "p")) { tem_p = true; tot += (k->txt ? (int)tam_texto(k->txt) : 0) + 1; }
    if (!tem_p) {
        const char *t = no && no->txt ? no->txt : "";
        while (xml_esp((unsigned char)*t)) t++;
        int ln = (int)tam_texto(t);
        while (ln > 0 && xml_esp((unsigned char)t[ln - 1])) ln--;
        return ar_texto(ar, t, ln);
    }
    char *r = ar_pedir(ar, (size_t)tot + 1);
    if (!r) return NULL;
    int o = 0;
    bool primeiro = true;
    for (xn *k = no->filho; k; k = k->irmao) {
        if (compara_texto(k->nome, "p")) continue;
        if (!primeiro) r[o++] = '\n';
        primeiro = false;
        if (k->txt) { int l = (int)tam_texto(k->txt); memcpy(r + o, k->txt, (size_t)l); o += l; }
    }
    r[o] = 0;
    return r;
}
#endif
