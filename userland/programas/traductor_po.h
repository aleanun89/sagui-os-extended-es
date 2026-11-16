// SPDX-License-Identifier: GPL-3.0-only
/* traductor_po.h - la lógica del Traductor (D335, mejora 133): leer y escribir catálogos .po/.pot,
 * validarlos, actualizarlos desde un .pot (como msgmerge), generar el .cat que carga el sistema (D313),
 * estadísticas y memoria de traducción.
 *
 * Todo en memoria en UTF-8, tal como está en el archivo (las cadenas ya sin las comillas ni los escapes).
 * La interfaz (traductor.c) enseña Latin-1 y convierte solo al editar. Se separa del programa para
 * probarlo en el PC sin QEMU (tests/traductor_unidad.c, h338) y para que el .cat que genera sea el mismo,
 * byte a byte, que el de tools/extraer_textos.py. */
#ifndef TRADUCTOR_PO_H
#define TRADUCTOR_PO_H
#include "sagui.h"
#pragma GCC diagnostic ignored "-Wunused-function"

#define PO_PLURAL_MAX 6

typedef struct {
    char *ctxt, *id, *id_pl;
    char *str[PO_PLURAL_MAX];
    int   n_str;               /* 1 sin plural; si hay id_pl, tantas como Plural-Forms */
    char *com;                 /* «# »: comentarios del traductor, una línea por '\n' */
    char *ext;                 /* «#. »: comentarios de extracción */
    char *refs;                /* «#: »: referencias, separadas por espacios */
    char *otras;               /* otras banderas de «#,» (distintas de fuzzy y c-format) */
    char *previo;              /* «#| »: texto anterior, tal cual */
    bool  fuzzy, obsoleta, cformat;
} po_ent_t;

typedef struct {
    po_ent_t *e;
    int       n, cap;
} po_t;

typedef struct { int nuevas, dudosas, obsoletas, iguales; } po_merge_t;
typedef struct { int total, traducidas, dudosas, sin, obsoletas; } po_stats_t;
typedef struct { int puntos; const char *orig, *trad; } po_sug_t;

/* validación: máscara de problemas */
#define PO_V_FORMATO  1    /* %s %d %1$s */
#define PO_V_ESCAPES  2    /* \n, \t, saltos al principio o al final */
#define PO_V_ATAJO    4    /* &x, _x y el atajo tras \t */
#define PO_V_ETIQUETA 8    /* <b>, <a href=...> */
#define PO_V_LONGITUD 16   /* no cabrá en la interfaz */
/* cabecera */
#define PO_H_FALTA    1
#define PO_H_CHARSET  2    /* no es UTF-8 */
#define PO_H_PLURALES 4    /* sin Plural-Forms válido pero hay plurales */
#define PO_H_IDIOMA   8    /* sin Language */
#define PO_H_MIME     16   /* sin Content-Type / MIME-Version */

/* ------------------------------------------------------------ memoria y texto */

static char *po_dup_n(const char *s, int n)
{
    char *d = aloca((size_t)n + 1);
    if (!d) return NULL;
    memcpy(d, s, (size_t)n);
    d[n] = 0;
    return d;
}
static char *po_dup(const char *s) { return s ? po_dup_n(s, (int)tam_texto(s)) : NULL; }
static void po_lib(char **p) { if (*p) { libera(*p); *p = NULL; } }
static void po_fijar(char **p, const char *s) { char *n = po_dup(s); po_lib(p); *p = n; }

typedef struct { char *p; int n, cap; } po_sb_t;
static bool po_sb_c(po_sb_t *b, char c)
{
    if (b->n + 2 > b->cap) {
        int nc = b->cap ? b->cap * 2 : 64;
        char *np = realoca(b->p, (size_t)nc);
        if (!np) return false;
        b->p = np; b->cap = nc;
    }
    b->p[b->n++] = c;
    b->p[b->n] = 0;
    return true;
}
static bool po_sb_s(po_sb_t *b, const char *s, int n)
{
    for (int i = 0; i < n; i++) if (!po_sb_c(b, s[i])) return false;
    if (!b->p) po_sb_c(b, 0), b->n = 0;
    return true;
}
static bool po_sb_z(po_sb_t *b, const char *s) { return po_sb_s(b, s, (int)tam_texto(s)); }

/* UTF-8 -> Latin-1 (para dibujar). false si algún carácter no cabe (sale '?'). */
static bool po_u8_l1(const char *s, char *dst, int max)
{
    bool ok = true;
    int k = 0;
    while (*s && k < max - 1) {
        uint8_t c = (uint8_t)*s;
        if (c < 0x80) { dst[k++] = (char)c; s++; }
        else if ((c == 0xC2 || c == 0xC3) && ((uint8_t)s[1] & 0xC0) == 0x80) {
            dst[k++] = (char)(((c & 3) << 6) | ((uint8_t)s[1] & 0x3F));
            s += 2;
        } else {
            dst[k++] = '?'; ok = false;
            s++;
            while (((uint8_t)*s & 0xC0) == 0x80) s++;
        }
    }
    dst[k] = 0;
    return ok;
}
/* Latin-1 -> UTF-8 (lo que se teclea en la interfaz). El resultado se libera con libera(). */
static char *po_l1_u8(const char *s, int n)
{
    po_sb_t b = { 0 };
    if (!po_sb_c(&b, 0)) return NULL;
    b.n = 0; b.p[0] = 0;
    for (int i = 0; i < n; i++) {
        uint8_t c = (uint8_t)s[i];
        if (c < 0x80) po_sb_c(&b, (char)c);
        else { po_sb_c(&b, (char)(0xC0 | (c >> 6))); po_sb_c(&b, (char)(0x80 | (c & 0x3F))); }
    }
    return b.p;
}
static int po_u8_len(const char *s)   /* caracteres, no bytes */
{
    int n = 0;
    for (; *s; s++) if (((uint8_t)*s & 0xC0) != 0x80) n++;
    return n;
}

static int po_min3(int a, int b, int c) { int m = a < b ? a : b; return m < c ? m : c; }

/* Distancia de edición con banda: devuelve banda+1 si la distancia es mayor. */
static int po_distancia(const char *a, int la, const char *b, int lb, int banda)
{
    if (la - lb > banda || lb - la > banda) return banda + 1;
    if (la == 0) return lb;
    if (lb == 0) return la;
    int *fila = aloca(sizeof(int) * 2 * (size_t)(lb + 1));
    if (!fila) return banda + 1;
    int *ant = fila, *act = fila + lb + 1;
    for (int j = 0; j <= lb; j++) ant[j] = j;
    int res = banda + 1;
    for (int i = 1; i <= la; i++) {
        act[0] = i;
        int minimo = act[0];
        for (int j = 1; j <= lb; j++) {
            int c = a[i - 1] == b[j - 1] ? 0 : 1;
            act[j] = po_min3(ant[j] + 1, act[j - 1] + 1, ant[j - 1] + c);
            if (act[j] < minimo) minimo = act[j];
        }
        if (minimo > banda) { libera(fila); return banda + 1; }
        int *t = ant; ant = act; act = t;
    }
    res = ant[lb];
    libera(fila);
    return res > banda ? banda + 1 : res;
}

/* Parecido de 0 a 100 (100 = iguales); 0 si es menor que "minimo". */
static int po_similitud(const char *a, const char *b, int minimo)
{
    int la = (int)tam_texto(a), lb = (int)tam_texto(b);
    if (la == lb && memcmp(a, b, (size_t)la) == 0) return 100;
    int mx = la > lb ? la : lb;
    if (mx == 0 || mx > 600) return 0;
    int banda = mx * (100 - minimo) / 100;
    int d = po_distancia(a, la, b, lb, banda);
    if (d > banda) return 0;
    return 100 - d * 100 / mx;
}

/* ------------------------------------------------------------ entradas */

static void po_ent_liberar(po_ent_t *e)
{
    po_lib(&e->ctxt); po_lib(&e->id); po_lib(&e->id_pl);
    for (int i = 0; i < PO_PLURAL_MAX; i++) po_lib(&e->str[i]);
    po_lib(&e->com); po_lib(&e->ext); po_lib(&e->refs); po_lib(&e->otras); po_lib(&e->previo);
    memset(e, 0, sizeof *e);
}
static void po_iniciar(po_t *p) { memset(p, 0, sizeof *p); }
static void po_liberar(po_t *p)
{
    for (int i = 0; i < p->n; i++) po_ent_liberar(&p->e[i]);
    if (p->e) libera(p->e);
    memset(p, 0, sizeof *p);
}
static po_ent_t *po_anadir(po_t *p, const po_ent_t *e)
{
    if (p->n == p->cap) {
        int nc = p->cap ? p->cap * 2 : 32;
        po_ent_t *ne = realoca(p->e, sizeof(po_ent_t) * (size_t)nc);
        if (!ne) return NULL;
        p->e = ne; p->cap = nc;
    }
    p->e[p->n] = *e;
    return &p->e[p->n++];
}
static bool po_es_cabecera(const po_ent_t *e) { return e->id && !e->id[0] && !e->ctxt && !e->obsoleta; }
static const char *po_cab_texto(const po_t *p)
{
    for (int i = 0; i < p->n; i++) if (po_es_cabecera(&p->e[i])) return p->e[i].str[0] ? p->e[i].str[0] : "";
    return NULL;
}
static po_ent_t *po_cab_ent(po_t *p)
{
    for (int i = 0; i < p->n; i++) if (po_es_cabecera(&p->e[i])) return &p->e[i];
    return NULL;
}

/* Valor de un campo de la cabecera («Language»), "" si no está. false si no hay. */
static bool po_cab_campo(const po_t *p, const char *campo, char *out, int max)
{
    const char *h = po_cab_texto(p);
    out[0] = 0;
    if (!h) return false;
    int lc = (int)tam_texto(campo);
    while (*h) {
        const char *fin = h;
        while (*fin && *fin != '\n') fin++;
        if (compara_texto_n(h, campo, (size_t)lc) == 0 && h[lc] == ':') {
            const char *v = h + lc + 1;
            while (*v == ' ') v++;
            int n = (int)(fin - v);
            if (n > max - 1) n = max - 1;
            memcpy(out, v, (size_t)n);
            out[n] = 0;
            return true;
        }
        h = *fin ? fin + 1 : fin;
    }
    return false;
}

/* Fija (o añade) un campo de la cabecera. */
static bool po_cab_fijar(po_t *p, const char *campo, const char *valor)
{
    po_ent_t *c = po_cab_ent(p);
    if (!c) {
        po_ent_t n;
        memset(&n, 0, sizeof n);
        n.id = po_dup(""); n.str[0] = po_dup(""); n.n_str = 1;
        if (!n.id || !n.str[0]) return false;
        /* la cabecera va la primera */
        if (!po_anadir(p, &n)) return false;
        po_ent_t tmp = p->e[p->n - 1];
        memmove(&p->e[1], &p->e[0], sizeof(po_ent_t) * (size_t)(p->n - 1));
        p->e[0] = tmp;
        c = &p->e[0];
    }
    po_sb_t b = { 0 };
    int lc = (int)tam_texto(campo);
    bool hecho = false;
    const char *h = c->str[0] ? c->str[0] : "";
    po_sb_c(&b, 0); b.n = 0; b.p[0] = 0;
    while (*h) {
        const char *fin = h;
        while (*fin && *fin != '\n') fin++;
        if (compara_texto_n(h, campo, (size_t)lc) == 0 && h[lc] == ':') {
            po_sb_z(&b, campo); po_sb_z(&b, ": "); po_sb_z(&b, valor); po_sb_c(&b, '\n');
            hecho = true;
        } else { po_sb_s(&b, h, (int)(fin - h)); po_sb_c(&b, '\n'); }
        h = *fin ? fin + 1 : fin;
    }
    if (!hecho) { po_sb_z(&b, campo); po_sb_z(&b, ": "); po_sb_z(&b, valor); po_sb_c(&b, '\n'); }
    po_lib(&c->str[0]);
    c->str[0] = b.p;
    return true;
}

static int po_nplurales(const po_t *p)
{
    char v[96];
    if (!po_cab_campo(p, "Plural-Forms", v, sizeof v)) return 2;
    for (char *q = v; *q; q++)
        if (compara_texto_n(q, "nplurals=", 9) == 0) {
            int n = texto_para_int(q + 9);
            return n >= 1 && n <= PO_PLURAL_MAX ? n : 2;
        }
    return 2;
}

/* Una entrada con plural debe tener tantos msgstr como Plural-Forms. */
static void po_ajustar_plurales(const po_t *p, po_ent_t *e)
{
    int n = e->id_pl ? po_nplurales(p) : 1;
    for (int i = n; i < PO_PLURAL_MAX; i++) po_lib(&e->str[i]);
    for (int i = 0; i < n; i++) if (!e->str[i]) e->str[i] = po_dup("");
    e->n_str = n;
}

/* ------------------------------------------------------------ leer */

static int po_hex(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

/* Desescapa una cadena entre comillas que empieza en s (tras espacios) y añade a b. false si no es válida. */
static bool po_literal(po_sb_t *b, const char *s, const char *fin)
{
    while (s < fin && (*s == ' ' || *s == '\t')) s++;
    while (fin > s && (fin[-1] == ' ' || fin[-1] == '\t' || fin[-1] == '\r')) fin--;
    if (fin - s < 2 || *s != '"' || fin[-1] != '"') return false;
    s++; fin--;
    while (s < fin) {
        char c = *s++;
        if (c != '\\') { po_sb_c(b, c); continue; }
        if (s >= fin) return false;
        c = *s++;
        switch (c) {
        case 'n': po_sb_c(b, '\n'); break;
        case 't': po_sb_c(b, '\t'); break;
        case 'r': po_sb_c(b, '\r'); break;
        case 'a': po_sb_c(b, '\a'); break;
        case 'b': po_sb_c(b, '\b'); break;
        case 'f': po_sb_c(b, '\f'); break;
        case 'v': po_sb_c(b, '\v'); break;
        case '\\': po_sb_c(b, '\\'); break;
        case '"': po_sb_c(b, '"'); break;
        case 'x': {
            int v = 0, k = 0;
            while (s < fin && po_hex(*s) >= 0 && k < 2) { v = v * 16 + po_hex(*s); s++; k++; }
            po_sb_c(b, (char)v);
            break;
        }
        default:
            if (c >= '0' && c <= '7') {
                int v = c - '0', k = 1;
                while (s < fin && *s >= '0' && *s <= '7' && k < 3) { v = v * 8 + (*s - '0'); s++; k++; }
                po_sb_c(b, (char)v);
            } else { po_sb_c(b, c); }
        }
    }
    return true;
}

static void po_anexar_linea(char **dst, const char *s, int n)
{
    po_sb_t b = { 0 };
    if (*dst) { po_sb_z(&b, *dst); po_sb_c(&b, '\n'); }
    po_sb_s(&b, s, n);
    if (!b.p) { po_sb_c(&b, 0); b.n = 0; }
    po_lib(dst);
    *dst = b.p;
}

typedef struct {
    po_ent_t e;
    po_sb_t  id, ctxt, idpl, str[PO_PLURAL_MAX];
    bool     tiene_id, tiene_ctxt, tiene_idpl, tiene_str, hay_algo;
    int      n_str;
    po_sb_t *objetivo;
} po_pend_t;

static void po_pend_liberar(po_pend_t *q)
{
    po_ent_liberar(&q->e);
    if (q->id.p) libera(q->id.p);
    if (q->ctxt.p) libera(q->ctxt.p);
    if (q->idpl.p) libera(q->idpl.p);
    for (int i = 0; i < PO_PLURAL_MAX; i++) if (q->str[i].p) libera(q->str[i].p);
    memset(q, 0, sizeof *q);
}

static char *po_sb_tomar(po_sb_t *b)
{
    if (!b->p) return po_dup("");
    char *r = b->p;
    b->p = NULL; b->n = b->cap = 0;
    return r;
}

/* Cierra la entrada pendiente y la añade al catálogo. */
static bool po_pend_cerrar(po_t *p, po_pend_t *q)
{
    bool ok = true;
    if (q->tiene_id) {
        po_ent_t e = q->e;
        memset(&q->e, 0, sizeof q->e);
        e.id = po_sb_tomar(&q->id);
        if (q->tiene_ctxt) e.ctxt = po_sb_tomar(&q->ctxt);
        if (q->tiene_idpl) e.id_pl = po_sb_tomar(&q->idpl);
        int n = q->n_str ? q->n_str : 1;
        for (int i = 0; i < n && i < PO_PLURAL_MAX; i++) e.str[i] = po_sb_tomar(&q->str[i]);
        e.n_str = n > PO_PLURAL_MAX ? PO_PLURAL_MAX : n;
        if (!po_anadir(p, &e)) { po_ent_liberar(&e); ok = false; }
    }
    po_pend_liberar(q);
    return ok;
}

/* Lee un .po/.pot entero (UTF-8). 0 = bien; >0 = número de línea con el error (el texto en err). */
static int po_leer(po_t *p, const char *txt, int n, char *err, int max_err)
{
    po_pend_t q;
    memset(&q, 0, sizeof q);
    int linea = 0, pos = 0;
    while (pos < n) {
        int ini = pos;
        while (pos < n && txt[pos] != '\n') pos++;
        const char *l = txt + ini;
        int len = pos - ini;
        if (pos < n) pos++;
        linea++;
        if (len && l[len - 1] == '\r') len--;
        if (linea == 1 && len >= 3 && (uint8_t)l[0] == 0xEF && (uint8_t)l[1] == 0xBB && (uint8_t)l[2] == 0xBF) { l += 3; len -= 3; }
        const char *fin = l + len;
        bool obs = false;
        if (len >= 2 && l[0] == '#' && l[1] == '~') {
            if (len >= 3 && l[2] == '|') continue;
            obs = true;
            l += 2; len -= 2;
            if (len && *l == ' ') { l++; len--; }
        }
        while (len && (*l == ' ' || *l == '\t') && !obs) { l++; len--; }
        if (len == 0) {
            if (q.tiene_str) po_pend_cerrar(p, &q);
            continue;
        }
        if (l[0] == '#' && !obs) {
            if (q.tiene_str) po_pend_cerrar(p, &q);
            q.hay_algo = true;
            char c = len > 1 ? l[1] : ' ';
            const char *t = l + 2;
            int tl = len > 2 ? len - 2 : 0;
            if (len >= 2 && l[1] == ' ') { /* comentario del traductor */ }
            if (c == '.') po_anexar_linea(&q.e.ext, t + (tl && *t == ' ' ? 1 : 0), tl - (tl && *t == ' ' ? 1 : 0));
            else if (c == ':') {
                if (tl && *t == ' ') { t++; tl--; }
                po_sb_t b = { 0 };
                if (q.e.refs) { po_sb_z(&b, q.e.refs); po_sb_c(&b, ' '); }
                po_sb_s(&b, t, tl);
                po_lib(&q.e.refs);
                q.e.refs = b.p;
            } else if (c == ',') {
                int i = 0;
                while (i < tl) {
                    while (i < tl && (t[i] == ' ' || t[i] == ',')) i++;
                    int j = i;
                    while (j < tl && t[j] != ' ' && t[j] != ',') j++;
                    if (j > i) {
                        if (j - i == 5 && compara_texto_n(t + i, "fuzzy", 5) == 0) q.e.fuzzy = true;
                        else if (j - i == 8 && compara_texto_n(t + i, "c-format", 8) == 0) q.e.cformat = true;
                        else {
                            po_sb_t b = { 0 };
                            if (q.e.otras) { po_sb_z(&b, q.e.otras); po_sb_c(&b, ' '); }
                            po_sb_s(&b, t + i, j - i);
                            po_lib(&q.e.otras);
                            q.e.otras = b.p;
                        }
                    }
                    i = j;
                }
            } else if (c == '|') po_anexar_linea(&q.e.previo, t + (tl && *t == ' ' ? 1 : 0), tl - (tl && *t == ' ' ? 1 : 0));
            else po_anexar_linea(&q.e.com, l + (len > 1 && l[1] == ' ' ? 2 : 1), len - (len > 1 && l[1] == ' ' ? 2 : 1));
            continue;
        }
        if (l[0] == '"') {
            if (!q.objetivo || !po_literal(q.objetivo, l, fin)) {
                if (err) formatar_texto(err, (size_t)max_err, "linea %d: cadena inesperada", linea);
                po_pend_liberar(&q); return linea;
            }
            continue;
        }
        /* palabra clave */
        int k = 0;
        while (k < len && l[k] != ' ' && l[k] != '"' && l[k] != '[') k++;
        const char *resto = l + k;
        int idx = -1;
        if (k < len && l[k] == '[') {
            idx = 0;
            resto++;
            while (resto < fin && *resto >= '0' && *resto <= '9') idx = idx * 10 + (*resto++ - '0');
            if (resto >= fin || *resto != ']') { if (err) formatar_texto(err, (size_t)max_err, "linea %d: msgstr[] mal formado", linea); po_pend_liberar(&q); return linea; }
            resto++;
        }
        bool es_ctxt = k == 7 && compara_texto_n(l, "msgctxt", 7) == 0;
        bool es_id = k == 5 && compara_texto_n(l, "msgid", 5) == 0;
        bool es_idpl = k == 12 && compara_texto_n(l, "msgid_plural", 12) == 0;
        bool es_str = k == 6 && compara_texto_n(l, "msgstr", 6) == 0;
        if (!es_ctxt && !es_id && !es_idpl && !es_str) {
            if (err) formatar_texto(err, (size_t)max_err, "linea %d: palabra desconocida", linea);
            po_pend_liberar(&q); return linea;
        }
        if ((es_ctxt || es_id) && q.tiene_str) po_pend_cerrar(p, &q);
        if (obs) q.e.obsoleta = true;
        if (es_ctxt) { q.tiene_ctxt = true; q.objetivo = &q.ctxt; }
        else if (es_id) { q.tiene_id = true; q.objetivo = &q.id; }
        else if (es_idpl) { q.tiene_idpl = true; q.objetivo = &q.idpl; }
        else {
            if (idx < 0) idx = 0;
            if (idx >= PO_PLURAL_MAX) { if (err) formatar_texto(err, (size_t)max_err, "linea %d: demasiados plurales", linea); po_pend_liberar(&q); return linea; }
            q.tiene_str = true;
            if (idx + 1 > q.n_str) q.n_str = idx + 1;
            q.objetivo = &q.str[idx];
        }
        if (!q.objetivo->p) { po_sb_c(q.objetivo, 0); q.objetivo->n = 0; q.objetivo->p[0] = 0; }
        if (!po_literal(q.objetivo, resto, fin)) {
            if (err) formatar_texto(err, (size_t)max_err, "linea %d: cadena sin comillas", linea);
            po_pend_liberar(&q); return linea;
        }
    }
    if (q.tiene_str) po_pend_cerrar(p, &q);
    else if (q.tiene_id) {
        if (err) formatar_texto(err, (size_t)max_err, "linea %d: msgid sin msgstr", linea);
        po_pend_liberar(&q);
        return linea;
    } else po_pend_liberar(&q);
    /* los plurales con menos msgstr de los que pide la cabecera se completan */
    for (int i = 0; i < p->n; i++) if (p->e[i].id_pl && !p->e[i].obsoleta) po_ajustar_plurales(p, &p->e[i]);
    return 0;
}

/* ------------------------------------------------------------ escribir */

static void po_esc_linea(po_sb_t *b, const char *pre, const char *clave, const char *s)
{
    bool varias = false;
    for (const char *q = s; *q; q++) if (*q == '\n' && q[1]) varias = true;
    po_sb_z(b, pre); po_sb_z(b, clave); po_sb_c(b, ' ');
    if (varias) { po_sb_z(b, "\"\"\n"); po_sb_z(b, pre); }
    po_sb_c(b, '"');
    for (const char *q = s; *q; q++) {
        char c = *q;
        if (c == '\n') {
            po_sb_z(b, "\\n\"");
            if (q[1]) { po_sb_c(b, '\n'); po_sb_z(b, pre); po_sb_c(b, '"'); }
            else { po_sb_c(b, '\n'); return; }
            continue;
        }
        if (c == '"') po_sb_z(b, "\\\"");
        else if (c == '\\') po_sb_z(b, "\\\\");
        else if (c == '\t') po_sb_z(b, "\\t");
        else if (c == '\r') po_sb_z(b, "\\r");
        else po_sb_c(b, c);
    }
    po_sb_z(b, "\"\n");
}

static void po_lineas(po_sb_t *b, const char *pre, const char *t)
{
    if (!t) return;
    const char *q = t;
    for (;;) {
        const char *f = q;
        while (*f && *f != '\n') f++;
        po_sb_z(b, pre);
        po_sb_s(b, q, (int)(f - q));
        po_sb_c(b, '\n');
        if (!*f) break;
        q = f + 1;
    }
}

/* El .po entero como texto UTF-8 (se libera con libera). */
static char *po_escribir(const po_t *p, int *tam)
{
    po_sb_t b = { 0 };
    po_sb_c(&b, 0); b.n = 0; b.p[0] = 0;
    bool primera = true;
    for (int pasada = 0; pasada < 2; pasada++) {
        for (int i = 0; i < p->n; i++) {
            const po_ent_t *e = &p->e[i];
            if ((pasada == 0) == e->obsoleta) continue;
            if (!primera) po_sb_c(&b, '\n');
            primera = false;
            const char *pre = e->obsoleta ? "#~ " : "";
            if (!e->obsoleta) {
                if (e->com) { for (const char *q = e->com;;) { const char *f = q; while (*f && *f != '\n') f++; po_sb_z(&b, *q == '\0' || f == q ? "#" : "# "); po_sb_s(&b, q, (int)(f - q)); po_sb_c(&b, '\n'); if (!*f) break; q = f + 1; } }
                po_lineas(&b, "#. ", e->ext);
                po_lineas(&b, "#: ", e->refs && !e->refs[0] ? NULL : e->refs);
                if (e->fuzzy || e->cformat || (e->otras && e->otras[0])) {
                    po_sb_z(&b, "#,");
                    bool pr = true;
                    if (e->fuzzy) { po_sb_z(&b, " fuzzy"); pr = false; }
                    if (e->cformat) { po_sb_z(&b, pr ? " c-format" : ", c-format"); pr = false; }
                    if (e->otras && e->otras[0]) { po_sb_z(&b, pr ? " " : ", "); po_sb_z(&b, e->otras); }
                    po_sb_c(&b, '\n');
                }
                po_lineas(&b, "#| ", e->previo);
            }
            if (e->ctxt) po_esc_linea(&b, pre, "msgctxt", e->ctxt);
            po_esc_linea(&b, pre, "msgid", e->id ? e->id : "");
            if (e->id_pl) {
                po_esc_linea(&b, pre, "msgid_plural", e->id_pl);
                int n = e->n_str < 1 ? 1 : e->n_str;
                for (int k = 0; k < n; k++) {
                    char clave[16];
                    formatar_texto(clave, sizeof clave, "msgstr[%d]", k);
                    po_esc_linea(&b, pre, clave, e->str[k] ? e->str[k] : "");
                }
            } else po_esc_linea(&b, pre, "msgstr", e->str[0] ? e->str[0] : "");
        }
    }
    if (tam) *tam = b.n;
    return b.p;
}

/* ------------------------------------------------------------ estado y estadísticas */

enum { PO_SIN = 0, PO_DUDOSA = 1, PO_TRAD = 2 };

static bool po_completa(const po_ent_t *e)
{
    int n = e->n_str < 1 ? 1 : e->n_str;
    for (int i = 0; i < n; i++) if (!e->str[i] || !e->str[i][0]) return false;
    return true;
}
static int po_estado(const po_ent_t *e)
{
    if (!po_completa(e)) return PO_SIN;
    return e->fuzzy ? PO_DUDOSA : PO_TRAD;
}
/* ¿Es una cadena traducible de la lista? (ni la cabecera ni las obsoletas) */
static bool po_en_lista(const po_ent_t *e) { return !e->obsoleta && !po_es_cabecera(e); }

static void po_estadisticas(const po_t *p, po_stats_t *s)
{
    memset(s, 0, sizeof *s);
    for (int i = 0; i < p->n; i++) {
        const po_ent_t *e = &p->e[i];
        if (e->obsoleta) { s->obsoletas++; continue; }
        if (po_es_cabecera(e)) continue;
        s->total++;
        int st = po_estado(e);
        if (st == PO_TRAD) s->traducidas++;
        else if (st == PO_DUDOSA) s->dudosas++;
        else s->sin++;
    }
}

/* ------------------------------------------------------------ validación */

#define PO_ESPEC_MAX 24
typedef struct { int pos; char conv; } po_espec_t;

/* Las conversiones de printf de un texto: «%s», «%1$d»... Devuelve cuántas. */
static int po_especificaciones(const char *s, po_espec_t *v, int max)
{
    int n = 0;
    for (; *s; s++) {
        if (*s != '%') continue;
        s++;
        if (*s == '%') continue;
        int pos = 0;
        const char *q = s;
        int num = 0;
        while (*q >= '0' && *q <= '9') num = num * 10 + (*q++ - '0');
        if (q != s && *q == '$') { pos = num; s = q + 1; }
        while (*s == '-' || *s == '+' || *s == ' ' || *s == '#' || *s == '0' || *s == '\'') s++;
        if (*s == '*') s++; else while (*s >= '0' && *s <= '9') s++;
        if (*s == '.') { s++; if (*s == '*') s++; else while (*s >= '0' && *s <= '9') s++; }
        while (*s == 'h' || *s == 'l' || *s == 'L' || *s == 'q' || *s == 'j' || *s == 'z' || *s == 't') s++;
        if (!*s) break;
        if (n < max) { v[n].pos = pos; v[n].conv = *s; n++; }
    }
    return n;
}

static bool po_espec_iguales(const po_espec_t *a, int na, const po_espec_t *b, int nb)
{
    if (na != nb) return false;
    bool pa = false, pb = false;
    for (int i = 0; i < na; i++) { if (a[i].pos) pa = true; if (b[i].pos) pb = true; }
    if (!pa && !pb) {
        for (int i = 0; i < na; i++) if (a[i].conv != b[i].conv) return false;
        return true;
    }
    if (pa != pb) return false;
    /* posicionales: mismo conjunto de (posición, conversión) en cualquier orden */
    for (int i = 0; i < na; i++) {
        bool hay = false;
        for (int k = 0; k < nb; k++) if (a[i].pos == b[k].pos && a[i].conv == b[k].conv) hay = true;
        if (!hay) return false;
    }
    return true;
}

/* Resumen legible: «%s %d». */
static void po_espec_resumen(const char *s, char *out, int max)
{
    po_espec_t v[PO_ESPEC_MAX];
    int n = po_especificaciones(s, v, PO_ESPEC_MAX), k = 0;
    out[0] = 0;
    for (int i = 0; i < n && k < max - 8; i++) {
        if (i) out[k++] = ' ';
        k += formatar_texto(out + k, (size_t)(max - k), v[i].pos ? "%%%d$%c" : "%%%c", v[i].pos ? v[i].pos : v[i].conv, v[i].conv);
    }
    if (!n) copia_texto(out, "-", (size_t)max);
}

static int po_contar(const char *s, char c) { int n = 0; for (; *s; s++) if (*s == c) n++; return n; }

/* Marcas de atajo: «&x» y «_x» (no «&&», no «_» entre letras). */
static int po_atajos(const char *s)
{
    int n = 0;
    for (const char *q = s; *q; q++) {
        bool alnum_sig = (q[1] >= '0' && q[1] <= '9') || (q[1] >= 'A' && q[1] <= 'Z') || (q[1] >= 'a' && q[1] <= 'z') || (uint8_t)q[1] >= 0x80;
        if (*q == '&') { if (q[1] == '&') { q++; continue; } if (alnum_sig) n++; }
        else if (*q == '_' && alnum_sig) {
            char a = q == s ? ' ' : q[-1];
            bool alnum_ant = (a >= '0' && a <= '9') || (a >= 'A' && a <= 'Z') || (a >= 'a' && a <= 'z');
            if (!alnum_ant) n++;
        }
    }
    return n;
}

/* Etiquetas <x> y </x>: la secuencia debe ser la misma. */
static bool po_etiquetas_iguales(const char *a, const char *b)
{
    for (;;) {
        const char *ta = NULL, *tb = NULL;
        int la = 0, lb = 0;
        for (; *a; a++)
            if (*a == '<' && ((a[1] >= 'a' && a[1] <= 'z') || (a[1] >= 'A' && a[1] <= 'Z') || a[1] == '/')) {
                const char *f = a;
                while (*f && *f != '>' && *f != '\n') f++;
                if (*f == '>') { ta = a; la = (int)(f - a) + 1; a = f; break; }
            }
        for (; *b; b++)
            if (*b == '<' && ((b[1] >= 'a' && b[1] <= 'z') || (b[1] >= 'A' && b[1] <= 'Z') || b[1] == '/')) {
                const char *f = b;
                while (*f && *f != '>' && *f != '\n') f++;
                if (*f == '>') { tb = b; lb = (int)(f - b) + 1; b = f; break; }
            }
        if (!ta && !tb) return true;
        if (!ta || !tb || la != lb || memcmp(ta, tb, (size_t)la) != 0) return false;
        a++; b++;
    }
}

/* ¿Qué le pasa a str[k] respecto del original? 0 si está bien o está vacía. Si "det" no es NULL,
 * recibe en det[0] el resumen de %s del original y en det[1] el de la traducción (para el mensaje). */
static int po_validar(const po_t *p, const po_ent_t *e, int k, char (*det)[48])
{
    const char *t = k < PO_PLURAL_MAX ? e->str[k] : NULL;
    if (!t || !t[0]) return 0;
    const char *o = e->id;
    if (e->id_pl && k >= 1) o = e->id_pl;
    int r = 0;
    /* formato */
    po_espec_t so[PO_ESPEC_MAX], st[PO_ESPEC_MAX];
    int no = po_especificaciones(o, so, PO_ESPEC_MAX), nt = po_especificaciones(t, st, PO_ESPEC_MAX);
    bool igual = po_espec_iguales(so, no, st, nt);
    if (!igual && e->id_pl && k == 0) {   /* la forma singular puede decir «un archivo» sin el %d */
        po_espec_t sp[PO_ESPEC_MAX];
        int np = po_especificaciones(e->id_pl, sp, PO_ESPEC_MAX);
        igual = nt == 0 || po_espec_iguales(sp, np, st, nt);
    }
    if (!igual) r |= PO_V_FORMATO;
    if (det) { po_espec_resumen(o, det[0], 48); po_espec_resumen(t, det[1], 48); }
    /* \n y \t */
    int fo = o[0] == '\n', ft = t[0] == '\n';
    int lo = (int)tam_texto(o), lt = (int)tam_texto(t);
    if (fo != ft || (lo && lt && (o[lo - 1] == '\n') != (t[lt - 1] == '\n'))) r |= PO_V_ESCAPES;
    if (po_contar(o, '\n') != po_contar(t, '\n') && !(e->id_pl && k == 0)) r |= PO_V_ESCAPES;
    if (po_contar(o, '\t') != po_contar(t, '\t')) r |= PO_V_ESCAPES;
    /* atajos: &x/_x y lo que sigue al tabulador (Ctrl+S) */
    if (po_atajos(o) != po_atajos(t)) r |= PO_V_ATAJO;
    const char *to = acha_char(o, '\t'), *tt = acha_char(t, '\t');
    if ((to != NULL) != (tt != NULL) || (to && compara_texto(to, tt) != 0)) r |= PO_V_ATAJO;
    /* etiquetas */
    if (!po_etiquetas_iguales(o, t)) r |= PO_V_ETIQUETA;
    /* longitud: más de la mitad más larga que el original y algo de margen */
    int co = po_u8_len(o), ct = po_u8_len(t);
    if (ct > co * 3 / 2 + 6) r |= PO_V_LONGITUD;
    (void)p;
    return r;
}

/* Todos los problemas de una entrada (de todas sus formas). */
static int po_validar_ent(const po_t *p, const po_ent_t *e)
{
    int r = 0, n = e->n_str < 1 ? 1 : e->n_str;
    for (int k = 0; k < n; k++) r |= po_validar(p, e, k, NULL);
    return r;
}

/* Problemas de la cabecera. */
static int po_validar_cabecera(const po_t *p)
{
    char v[128];
    int r = 0;
    if (!po_cab_texto(p)) return PO_H_FALTA | PO_H_CHARSET | PO_H_IDIOMA | PO_H_MIME;
    if (!po_cab_campo(p, "Content-Type", v, sizeof v)) r |= PO_H_MIME | PO_H_CHARSET;
    else {
        bool utf = false;
        for (char *q = v; *q; q++)
            if ((q[0] == 'U' || q[0] == 'u') && (q[1] == 'T' || q[1] == 't') && (q[2] == 'F' || q[2] == 'f') && q[3] == '-' && q[4] == '8') utf = true;
        if (!utf) r |= PO_H_CHARSET;
    }
    if (!po_cab_campo(p, "MIME-Version", v, sizeof v)) r |= PO_H_MIME;
    if (!po_cab_campo(p, "Language", v, sizeof v) || !v[0]) r |= PO_H_IDIOMA;
    bool plural = false;
    for (int i = 0; i < p->n; i++) if (p->e[i].id_pl) plural = true;
    if (plural) {
        if (!po_cab_campo(p, "Plural-Forms", v, sizeof v) || !acha_char(v, '=') || compara_texto_n(v, "nplurals=", 9) != 0) r |= PO_H_PLURALES;
    }
    return r;
}

/* ------------------------------------------------------------ buscar y filtrar */

static bool po_contiene_ci(const char *h, const char *a)
{
    if (!h || !a) return false;
    if (!a[0]) return true;
    int la = (int)tam_texto(a);
    for (; *h; h++) {
        int i = 0;
        for (; i < la; i++) {
            char x = h[i], y = a[i];
            if (!x) return false;
            if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
            if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
            if (x != y) break;
        }
        if (i == la) return true;
    }
    return false;
}

/* ¿La entrada contiene el texto (UTF-8) en el original, la traducción, el contexto o los comentarios? */
static bool po_coincide(const po_ent_t *e, const char *texto)
{
    if (po_contiene_ci(e->id, texto) || po_contiene_ci(e->id_pl, texto) || po_contiene_ci(e->ctxt, texto) ||
        po_contiene_ci(e->com, texto) || po_contiene_ci(e->ext, texto)) return true;
    for (int i = 0; i < e->n_str; i++) if (po_contiene_ci(e->str[i], texto)) return true;
    return false;
}

static int po_buscar(const po_t *p, const char *ctxt, const char *id)
{
    for (int i = 0; i < p->n; i++) {
        const po_ent_t *e = &p->e[i];
        if (!e->id || compara_texto(e->id, id) != 0) continue;
        if ((e->ctxt != NULL) != (ctxt != NULL)) continue;
        if (ctxt && compara_texto(e->ctxt, ctxt) != 0) continue;
        return i;
    }
    return -1;
}

/* ------------------------------------------------------------ actualizar desde el .pot (msgmerge) */

static bool po_tiene_traduccion(const po_ent_t *e)
{
    for (int i = 0; i < e->n_str; i++) if (e->str[i] && e->str[i][0]) return true;
    return false;
}

static po_ent_t po_copiar_ent(const po_ent_t *s)
{
    po_ent_t d;
    memset(&d, 0, sizeof d);
    d.ctxt = po_dup(s->ctxt); d.id = po_dup(s->id); d.id_pl = po_dup(s->id_pl);
    for (int i = 0; i < PO_PLURAL_MAX; i++) d.str[i] = po_dup(s->str[i]);
    d.n_str = s->n_str;
    d.com = po_dup(s->com); d.ext = po_dup(s->ext); d.refs = po_dup(s->refs);
    d.otras = po_dup(s->otras); d.previo = po_dup(s->previo);
    d.fuzzy = s->fuzzy; d.obsoleta = s->obsoleta; d.cformat = s->cformat;
    return d;
}

/* Actualiza "po" con las cadenas del .pot: las nuevas entran vacías, las que cambian un poco entran
 * como dudosas con la traducción vieja, las que desaparecen pasan a obsoletas (si tenían traducción). */
static bool po_actualizar(po_t *po, const po_t *pot, po_merge_t *res)
{
    po_merge_t r = { 0, 0, 0, 0 };
    int nviejo = po->n;
    bool *usada = aloca_zerado((size_t)(nviejo ? nviejo : 1));
    if (!usada) return false;
    po_t nuevo;
    po_iniciar(&nuevo);
    /* la cabecera: la del .po (o la del .pot si no tiene) */
    int ihv = -1;
    for (int i = 0; i < nviejo; i++) if (po_es_cabecera(&po->e[i])) { ihv = i; break; }
    if (ihv >= 0) { po_anadir(&nuevo, &po->e[ihv]); usada[ihv] = true; memset(&po->e[ihv], 0, sizeof(po_ent_t)); }
    else {
        for (int i = 0; i < pot->n; i++) if (po_es_cabecera(&pot->e[i])) { po_ent_t c = po_copiar_ent(&pot->e[i]); po_anadir(&nuevo, &c); break; }
    }
    char pc[96];
    if (po_cab_campo(pot, "POT-Creation-Date", pc, sizeof pc) && pc[0]) po_cab_fijar(&nuevo, "POT-Creation-Date", pc);
    /* pasada 1: coincidencias exactas */
    int *par = aloca(sizeof(int) * (size_t)(pot->n ? pot->n : 1));
    if (!par) { libera(usada); po_liberar(&nuevo); return false; }
    for (int j = 0; j < pot->n; j++) {
        par[j] = -1;
        const po_ent_t *q = &pot->e[j];
        if (q->obsoleta || po_es_cabecera(q)) continue;
        for (int i = 0; i < nviejo; i++) {
            if (usada[i] || !po->e[i].id) continue;
            if (compara_texto(po->e[i].id, q->id) != 0) continue;
            if ((po->e[i].ctxt != NULL) != (q->ctxt != NULL)) continue;
            if (q->ctxt && compara_texto(po->e[i].ctxt, q->ctxt) != 0) continue;
            par[j] = i; usada[i] = true;
            break;
        }
    }
    /* pasada 2: en el orden del .pot, lo exacto, lo parecido y lo nuevo */
    for (int j = 0; j < pot->n; j++) {
        const po_ent_t *q = &pot->e[j];
        if (q->obsoleta || po_es_cabecera(q)) continue;
        po_ent_t e;
        memset(&e, 0, sizeof e);
        if (par[j] >= 0) {
            e = po->e[par[j]];
            memset(&po->e[par[j]], 0, sizeof(po_ent_t));
            e.obsoleta = false;
            if (!e.fuzzy) po_lib(&e.previo);
            po_fijar(&e.id_pl, q->id_pl);
            if (!q->id_pl) e.id_pl = NULL;
            po_fijar(&e.refs, q->refs); po_fijar(&e.ext, q->ext);
            if (!q->refs) po_lib(&e.refs);
            if (!q->ext) po_lib(&e.ext);
            e.cformat = q->cformat;
            po_lib(&e.otras); e.otras = po_dup(q->otras);
            po_ajustar_plurales(&nuevo, &e);
            r.iguales++;
        } else {
            int mejor = -1, mp = 0;
            for (int i = 0; i < nviejo; i++) {
                if (usada[i] || !po->e[i].id || !po_tiene_traduccion(&po->e[i])) continue;
                if ((po->e[i].ctxt != NULL) != (q->ctxt != NULL)) continue;
                int s = po_similitud(po->e[i].id, q->id, 60);
                if (s > mp) { mp = s; mejor = i; }
            }
            e = po_copiar_ent(q);
            for (int i = 0; i < PO_PLURAL_MAX; i++) po_lib(&e.str[i]);
            e.fuzzy = false; e.obsoleta = false;
            if (mejor >= 0) {
                po_ent_t *v = &po->e[mejor];
                usada[mejor] = true;
                for (int i = 0; i < PO_PLURAL_MAX; i++) { e.str[i] = v->str[i]; v->str[i] = NULL; }
                e.n_str = v->n_str;
                po_lib(&e.previo);
                po_sb_t b = { 0 };
                po_sb_c(&b, 0); b.n = 0; b.p[0] = 0;
                if (v->ctxt) { po_sb_z(&b, "msgctxt \""); po_sb_z(&b, v->ctxt); po_sb_z(&b, "\"\n"); }
                po_sb_z(&b, "msgid \""); po_sb_z(&b, v->id); po_sb_c(&b, '"');
                e.previo = b.p;
                e.fuzzy = true;
                po_ajustar_plurales(&nuevo, &e);
                for (int i = 0; i < PO_PLURAL_MAX; i++) po_lib(&po->e[mejor].str[i]);
                r.dudosas++;
            } else {
                po_ajustar_plurales(&nuevo, &e);
                r.nuevas++;
            }
            po_lib(&e.com);
        }
        po_anadir(&nuevo, &e);
    }
    /* lo que sobra: obsoleto si tenía traducción */
    for (int i = 0; i < nviejo; i++) {
        if (usada[i] || !po->e[i].id) continue;
        if (po_tiene_traduccion(&po->e[i]) && po->e[i].id[0]) {
            po_ent_t e = po->e[i];
            memset(&po->e[i], 0, sizeof(po_ent_t));
            e.obsoleta = true;
            po_lib(&e.refs); po_lib(&e.ext); po_lib(&e.previo);
            po_anadir(&nuevo, &e);
            r.obsoletas++;
        }
    }
    libera(usada); libera(par);
    po_liberar(po);
    *po = nuevo;
    if (res) *res = r;
    return true;
}

/* Un catálogo nuevo desde un .pot, para un idioma: todo sin traducir. */
static bool po_desde_pot(po_t *dst, const po_t *pot, const char *idioma, const char *plural)
{
    po_iniciar(dst);
    po_t vacio;
    po_iniciar(&vacio);
    po_merge_t r;
    if (!po_actualizar(&vacio, pot, &r)) return false;
    *dst = vacio;
    po_cab_fijar(dst, "Language", idioma);
    po_cab_fijar(dst, "MIME-Version", "1.0");
    po_cab_fijar(dst, "Content-Type", "text/plain; charset=UTF-8");
    po_cab_fijar(dst, "Content-Transfer-Encoding", "8bit");
    po_cab_fijar(dst, "Plural-Forms", plural);
    for (int i = 0; i < dst->n; i++) if (dst->e[i].id_pl) po_ajustar_plurales(dst, &dst->e[i]);
    return true;
}

/* La fórmula de plurales habitual de un idioma. */
static const char *po_plurales_de(const char *idioma)
{
    if (!compara_texto_n(idioma, "pt_BR", 5) || !compara_texto_n(idioma, "fr", 2)) return "nplurals=2; plural=(n > 1);";
    if (!compara_texto_n(idioma, "ja", 2) || !compara_texto_n(idioma, "zh", 2) || !compara_texto_n(idioma, "ko", 2)) return "nplurals=1; plural=0;";
    if (!compara_texto_n(idioma, "ru", 2) || !compara_texto_n(idioma, "pl", 2))
        return "nplurals=3; plural=(n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);";
    return "nplurals=2; plural=(n != 1);";
}

/* ------------------------------------------------------------ catálogo del sistema (.cat) */

#define PO_CAT_MAGIA 0x54434753u
static uint32_t po_fnv(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
    return h;
}
static void po_le32(uint8_t *d, uint32_t v) { d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); d[2] = (uint8_t)(v >> 16); d[3] = (uint8_t)(v >> 24); }
static uint32_t po_rd32(const uint8_t *d) { return d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24); }

typedef struct { uint32_t h; char *k, *t; } po_par_t;

static int po_par_cmp(const po_par_t *a, const po_par_t *b)
{
    if (a->h != b->h) return a->h < b->h ? -1 : 1;
    return compara_texto(a->k, b->k);
}

/* Genera el .cat (Latin-1, como el que hace tools/extraer_textos.py): solo lo traducido y no dudoso.
 * "omitidas" cuenta lo que no cabe en Latin-1. Se libera con libera(). */
static uint8_t *po_a_cat(const po_t *p, int *tam, int *entradas, int *omitidas)
{
    int cap = 0;
    for (int i = 0; i < p->n; i++) cap += 2;
    po_par_t *v = aloca(sizeof(po_par_t) * (size_t)(cap + 1));
    if (!v) return NULL;
    int n = 0, om = 0;
    for (int i = 0; i < p->n; i++) {
        const po_ent_t *e = &p->e[i];
        if (!po_en_lista(e) || e->fuzzy || !po_completa(e)) continue;
        for (int f = 0; f < (e->id_pl ? 2 : 1); f++) {
            const char *clave = f == 0 ? e->id : e->id_pl;
            const char *trad = f == 0 ? e->str[0] : e->str[e->n_str - 1];
            char *kl = aloca(tam_texto(clave) + 1), *tl = aloca(tam_texto(trad) + 1);
            if (!kl || !tl) { if (kl) libera(kl); if (tl) libera(tl); continue; }
            if (!po_u8_l1(clave, kl, (int)tam_texto(clave) + 1) || !po_u8_l1(trad, tl, (int)tam_texto(trad) + 1)) { om++; libera(kl); libera(tl); continue; }
            bool dup = false;
            for (int k = 0; k < n; k++) if (compara_texto(v[k].k, kl) == 0) { dup = true; break; }
            if (dup) { libera(kl); libera(tl); continue; }
            v[n].h = po_fnv(kl); v[n].k = kl; v[n].t = tl; n++;
        }
    }
    /* ordenar por (hash, clave): shell sort */
    for (int gap = n / 2; gap > 0; gap /= 2)
        for (int i = gap; i < n; i++) {
            po_par_t t = v[i];
            int j = i;
            while (j >= gap && po_par_cmp(&v[j - gap], &t) > 0) { v[j] = v[j - gap]; j -= gap; }
            v[j] = t;
        }
    int total = 16 + 12 * n;
    for (int i = 0; i < n; i++) total += (int)tam_texto(v[i].k) + 1 + (int)tam_texto(v[i].t) + 1;
    uint8_t *d = aloca_zerado((size_t)total);
    if (d) {
        po_le32(d, PO_CAT_MAGIA); po_le32(d + 4, 1); po_le32(d + 8, (uint32_t)n); po_le32(d + 12, 0);
        int off = 16 + 12 * n;
        for (int i = 0; i < n; i++) {
            int lk = (int)tam_texto(v[i].k) + 1, lt = (int)tam_texto(v[i].t) + 1;
            po_le32(d + 16 + 12 * i, v[i].h);
            po_le32(d + 16 + 12 * i + 4, (uint32_t)off);
            memcpy(d + off, v[i].k, (size_t)lk); off += lk;
            po_le32(d + 16 + 12 * i + 8, (uint32_t)off);
            memcpy(d + off, v[i].t, (size_t)lt); off += lt;
        }
    }
    for (int i = 0; i < n; i++) { libera(v[i].k); libera(v[i].t); }
    libera(v);
    if (tam) *tam = total;
    if (entradas) *entradas = n;
    if (omitidas) *omitidas = om;
    return d;
}

/* Un .cat leído como catálogo de entradas (clave -> traducción), para la memoria de traducción. */
static bool po_de_cat(po_t *p, const uint8_t *d, int tam)
{
    if (tam < 16 || po_rd32(d) != PO_CAT_MAGIA) return false;
    uint32_t n = po_rd32(d + 8);
    if ((uint64_t)16 + 12ull * n > (uint64_t)tam) return false;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t ok = po_rd32(d + 16 + 12 * i + 4), ot = po_rd32(d + 16 + 12 * i + 8);
        if (ok >= (uint32_t)tam || ot >= (uint32_t)tam) continue;
        const char *k = (const char *)d + ok, *t = (const char *)d + ot;
        if (!t[0]) continue;
        /* Latin-1 -> UTF-8 */
        po_ent_t e;
        memset(&e, 0, sizeof e);
        e.id = po_l1_u8(k, (int)tam_texto(k));
        e.str[0] = po_l1_u8(t, (int)tam_texto(t));
        e.n_str = 1;
        if (!e.id || !e.str[0] || !po_anadir(p, &e)) { po_ent_liberar(&e); return false; }
    }
    return true;
}

/* ------------------------------------------------------------ memoria de traducción */

/* Las "max" mejores traducciones (iguales o parecidas, de 60 % arriba) de "id" en las fuentes. */
static int po_memoria(const po_t *const *fuentes, int nf, const char *id, po_sug_t *res, int max)
{
    int n = 0;
    for (int f = 0; f < nf; f++) {
        const po_t *p = fuentes[f];
        for (int i = 0; i < p->n; i++) {
            const po_ent_t *e = &p->e[i];
            if (!po_en_lista(e) || e->fuzzy || !e->str[0] || !e->str[0][0] || e->id_pl) continue;
            int s = po_similitud(e->id, id, 60);
            if (s < 60) continue;
            bool dup = false;
            for (int k = 0; k < n; k++) if (compara_texto(res[k].trad, e->str[0]) == 0) { if (res[k].puntos < s) { res[k].puntos = s; res[k].orig = e->id; } dup = true; }
            if (dup) continue;
            int pos = n;
            if (n < max) n++;
            else if (res[n - 1].puntos >= s) continue;
            else pos = n - 1;
            while (pos > 0 && res[pos - 1].puntos < s) { res[pos] = res[pos - 1]; pos--; }
            res[pos].puntos = s; res[pos].orig = e->id; res[pos].trad = e->str[0];
        }
    }
    return n;
}

#endif
