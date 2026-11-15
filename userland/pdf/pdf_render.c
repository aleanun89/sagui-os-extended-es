// SPDX-License-Identifier: GPL-2.0-or-later
/* pdf_render.c - renderizador del motor de PDF (mejora 059, D138).
 *
 * Este modulo dibuja una pagina del PDF en un bucle RGBA (alfa recto: el
 * fondo queda transparente y solo se ve lo que el PDF dibuja).
 *
 * Interpreta el flujo de contenido de la pagina (los operadores de PDF),
 * rasteriza los caminos con antialiasing horizontal y dibuja el texto con
 * los contornos de las tipografias.
 *
 * No hay libc: solo se usan las funciones de sagui.h (aloca, libera,
 * memcpy...) y las del motor (pdf_lex, pdf_geom, pdf_fonte). Los numeros
 * reales se calculan con la x87 (el modulo va con -mfpmath=387), asi que las
 * funciones matematicas que hacen falta (raiz, seno, coseno) estan escritas
 * aqui. */
#include "pdf.h"
#include "pdf_lex.h"
#include "pdf_geom.h"
#include "pdf_fonte.h"
#include "imagem.h"

/* ------------------------------------------------------------- ajudantes */

static bool iguais(const char *a, const char *b)
{
    if (!a || !b) return a == b;
    return compara_texto(a, b) == 0;
}

/* Copia n bytes de un texto a memoria nueva (con el cero al final). */
static char *copiar_txt(const char *s, int n)
{
    char *d = aloca((size_t)n + 1);
    if (!d) return NULL;
    if (n > 0) memcpy(d, s, (size_t)n);
    d[n] = 0;
    return d;
}

/* Copia un texto entero a memoria nueva. */
static char *copiar_str(const char *s)
{
    if (!s) return NULL;
    return copiar_txt(s, (int)tam_texto(s));
}

static double limitar(double v, double lo, double hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static double fabs_d(double x)
{
    return __builtin_fabs(x);
}

static double piso_d(double x)
{
    return __builtin_floor(x);
}

#define PDF_PI 3.14159265358979323846

/* La raiz cuadrada: Newton-Raphson (sin libm, que no existe aqui). Se
 * empieza con una aproximacion del orden de magnitud para que converja
 * rapido. */
static double raiz_d(double x)
{
    if (!(x > 0.0)) return 0.0;
    double v = x;
    int e = 0;
    while (v >= 4.0) { v *= 0.25; e += 2; }
    while (v < 1.0) { v *= 4.0; e -= 2; }
    double g = 0.5 * (v + 1.0);
    for (int i = 0; i < 40; i++) {
        double n = 0.5 * (g + v / g);
        if (n == g) break;
        g = n;
    }
    double r = g;
    for (int i = 0; i < e / 2; i++) r *= 2.0;
    for (int i = 0; i < (-e) / 2; i++) r *= 0.5;
    return r;
}

/* El seno y el coseno: serie de Taylor con reduccion de rango (basta para
 * los arcos de las uniones redondeadas y de los topes). */
static double seno_d(double x)
{
    double k = piso_d(x / (2.0 * PDF_PI) + 0.5);
    x -= k * (2.0 * PDF_PI);
    double x2 = x * x;
    return x * (1.0 + x2 * (-1.0 / 6.0 + x2 * (1.0 / 120.0 +
                x2 * (-1.0 / 5040.0 + x2 * (1.0 / 362880.0)))));
}

static double cosseno_d(double x)
{
    return seno_d(x + PDF_PI / 2.0);
}

/* --------------------------------------------------------------- colores */

typedef struct {
    double r, g, b, a;
} cor_t;

/* Los espacios de color que se saben pintar. */
#define ESP_GRAY   0
#define ESP_RGB    1
#define ESP_CMYK   2
#define ESP_PADRAO 3

/* ------------------------------------------- valores de los operandos */

typedef enum {
    VAL_NULO,
    VAL_NUMERO,
    VAL_NOME,
    VAL_CADEIA,
    VAL_ARR,
    VAL_DICT,
    VAL_BOOL
} val_tipo_t;

typedef struct val_t {
    val_tipo_t tipo;
    double     numero;
    char      *texto;      /* el nombre o los bytes de la cadena */
    int        tam_texto;
    bool       estatico;    /* el texto no hay que liberarlo */
    struct val_t **itens;                     /* VAL_ARR */
    int         n_itens;
    struct { char *chave; struct val_t *valor; } *entradas;   /* VAL_DICT */
    int         n_entradas;
} val_t;

static val_t *val_novo(val_tipo_t t)
{
    val_t *v = aloca_zerado(sizeof *v);
    if (v) v->tipo = t;
    return v;
}

static void val_liberar(val_t *v)
{
    if (!v) return;
    if (v->texto && !v->estatico) libera(v->texto);
    for (int i = 0; i < v->n_itens; i++) val_liberar(v->itens[i]);
    if (v->itens) libera(v->itens);
    for (int i = 0; i < v->n_entradas; i++) {
        if (v->entradas[i].chave) libera(v->entradas[i].chave);
        val_liberar(v->entradas[i].valor);
    }
    if (v->entradas) libera(v->entradas);
    libera(v);
}

static bool val_add_item(val_t *arr, val_t *item)
{
    if (arr->n_itens >= arr->n_itens + 1 && !arr->itens) { /* nada */ }
    val_t **novo = realoca(arr->itens, (size_t)(arr->n_itens + 1) * sizeof(val_t *));
    if (!novo) return false;
    arr->itens = novo;
    arr->itens[arr->n_itens++] = item;
    return true;
}

static bool val_add_entrada(val_t *dic, const char *chave, val_t *valor)
{
    char *c = copiar_str(chave);
    if (!c) return false;
    void *novo = realoca(dic->entradas, (size_t)(dic->n_entradas + 1) * sizeof(*dic->entradas));
    if (!novo) { libera(c); return false; }
    dic->entradas = novo;
    dic->entradas[dic->n_entradas].chave = c;
    dic->entradas[dic->n_entradas].valor = valor;
    dic->n_entradas++;
    return true;
}

/* Los numeros de un arreglo de operandos. */
static double val_numero(const val_t *v)
{
    if (!v) return 0.0;
    if (v->tipo == VAL_NUMERO) return v->numero;
    if (v->tipo == VAL_BOOL) return v->numero;
    return 0.0;
}

/* -------------------------------------------- lexico del flujo de contenido */

typedef enum {
    TOK_FIM,
    TOK_NUMERO,
    TOK_NOME,
    TOK_CADEIA,
    TOK_PALAVRA,
    TOK_ARR_ABRE,
    TOK_ARR_FECHA,
    TOK_DIC_ABRE,
    TOK_DIC_FECHA
} tok_tipo_t;

typedef struct {
    const uint8_t *d;
    int n, p;
    char pal[64];      /* la palabra del operador */
} lex_t;

static bool lex_espaco(int c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == 0;
}

static bool lex_delim(int c)
{
    /* La barra tambien separa: "]TJ/F49 9 Tf" son tres cosas (pdfTeX lo
     * escribe asi, sin espacios). */
    return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']' ||
           c == '/' || c == '{' || c == '}';
}

static void lex_pular(lex_t *l)
{
    while (l->p < l->n) {
        uint8_t c = l->d[l->p];
        if (lex_espaco(c)) { l->p++; continue; }
        if (c == '%') {
            while (l->p < l->n && l->d[l->p] != '\n' && l->d[l->p] != '\r') l->p++;
            continue;
        }
        break;
    }
}

static int hex_valor(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Le un numero (con signo y exponente). */
static bool lex_numero(lex_t *l, double *valor)
{
    int ini = l->p;
    bool neg = false;
    if (l->p < l->n && (l->d[l->p] == '+' || l->d[l->p] == '-')) {
        neg = (l->d[l->p] == '-');
        l->p++;
    }
    double v = 0.0;
    bool algum = false;
    while (l->p < l->n && l->d[l->p] >= '0' && l->d[l->p] <= '9') {
        v = v * 10.0 + (double)(l->d[l->p] - '0');
        l->p++;
        algum = true;
    }
    if (l->p < l->n && l->d[l->p] == '.') {
        l->p++;
        double f = 0.1;
        while (l->p < l->n && l->d[l->p] >= '0' && l->d[l->p] <= '9') {
            v += (double)(l->d[l->p] - '0') * f;
            f *= 0.1;
            l->p++;
            algum = true;
        }
    }
    if (!algum) { l->p = ini; return false; }
    if (l->p < l->n && (l->d[l->p] == 'e' || l->d[l->p] == 'E')) {
        int save = l->p;
        l->p++;
        bool eneg = false;
        if (l->p < l->n && (l->d[l->p] == '+' || l->d[l->p] == '-')) {
            eneg = (l->d[l->p] == '-');
            l->p++;
        }
        int ex = 0;
        bool tem = false;
        while (l->p < l->n && l->d[l->p] >= '0' && l->d[l->p] <= '9') {
            if (ex < 10000) ex = ex * 10 + (l->d[l->p] - '0');
            l->p++;
            tem = true;
        }
        if (tem) {
            double m = 1.0;
            for (int i = 0; i < ex; i++) m *= 10.0;
            v = eneg ? v / m : v * m;
        } else {
            l->p = save;
        }
    }
    *valor = neg ? -v : v;
    return true;
}

/* Le un nombre (/algo). El texto va al valor. */
static val_t *lex_nome(lex_t *l)
{
    l->p++;    /* salta la barra */
    int cap = 32, n = 0;
    char *b = aloca((size_t)cap);
    if (!b) return NULL;
    while (l->p < l->n) {
        uint8_t c = l->d[l->p];
        if (lex_espaco(c) || lex_delim(c) || c == '%') break;
        l->p++;
        if (c == '#' && l->p + 1 < l->n) {
            int h1 = hex_valor(l->d[l->p]), h2 = hex_valor(l->d[l->p + 1]);
            if (h1 >= 0 && h2 >= 0) { c = (uint8_t)(h1 * 16 + h2); l->p += 2; }
        }
        if (n + 1 >= cap) {
            cap *= 2;
            char *nb = realoca(b, (size_t)cap);
            if (!nb) { libera(b); return NULL; }
            b = nb;
        }
        b[n++] = (char)c;
    }
    b[n] = 0;
    val_t *v = val_novo(VAL_NOME);
    if (!v) { libera(b); return NULL; }
    v->texto = b;
    v->tam_texto = n;
    return v;
}

/* Le una cadena entre parentesis. */
static val_t *lex_cadeia(lex_t *l)
{
    l->p++;    /* salta el parentesis */
    int cap = 64, n = 0;
    uint8_t *b = aloca((size_t)cap);
    if (!b) return NULL;
    int nivel = 1;
    while (l->p < l->n) {
        uint8_t c = l->d[l->p++];
        if (c == '\\') {
            if (l->p >= l->n) break;
            uint8_t e = l->d[l->p++];
            switch (e) {
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 't': c = '\t'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case '(': case ')': case '\\': c = e; break;
            case '\r':
                /* Barra y fin de linea: la cadena sigue en la linea de
                 * abajo, sin ningun byte (Acrobat corta asi las largas). */
                if (l->p < l->n && l->d[l->p] == '\n') l->p++;
                continue;
            case '\n': continue;
            default:
                if (e >= '0' && e <= '7') {
                    int oc = e - '0';
                    for (int k = 0; k < 2 && l->p < l->n; k++) {
                        uint8_t o = l->d[l->p];
                        if (o < '0' || o > '7') break;
                        oc = oc * 8 + (o - '0');
                        l->p++;
                    }
                    c = (uint8_t)oc;
                } else {
                    c = e;
                }
            }
        } else if (c == '\r') {
            /* Un fin de linea sin barra vale un \n (CR y CRLF tambien). */
            if (l->p < l->n && l->d[l->p] == '\n') l->p++;
            c = '\n';
        } else if (c == '(') {
            nivel++;
        } else if (c == ')') {
            nivel--;
            if (nivel == 0) break;
        }
        if (n + 1 >= cap) {
            cap *= 2;
            uint8_t *nb = realoca(b, (size_t)cap);
            if (!nb) { libera(b); return NULL; }
            b = nb;
        }
        b[n++] = c;
    }
    val_t *v = val_novo(VAL_CADEIA);
    if (!v) { libera(b); return NULL; }
    v->texto = (char *)b;
    v->tam_texto = n;
    return v;
}

/* Le una cadena hexadecimal (<0a1b>). */
static val_t *lex_hex(lex_t *l)
{
    l->p++;    /* salta el menor */
    int cap = 64, n = 0;
    uint8_t *b = aloca((size_t)cap);
    if (!b) return NULL;
    int acum = -1;
    while (l->p < l->n) {
        uint8_t c = l->d[l->p];
        if (c == '>') { l->p++; break; }
        l->p++;
        if (lex_espaco(c)) continue;
        int h = hex_valor(c);
        if (h < 0) continue;
        if (acum < 0) {
            acum = h;
        } else {
            if (n + 1 >= cap) {
                cap *= 2;
                uint8_t *nb = realoca(b, (size_t)cap);
                if (!nb) { libera(b); return NULL; }
                b = nb;
            }
            b[n++] = (uint8_t)(acum * 16 + h);
            acum = -1;
        }
    }
    if (acum >= 0) {
        if (n + 1 >= cap) {
            cap *= 2;
            uint8_t *nb = realoca(b, (size_t)cap);
            if (!nb) { libera(b); return NULL; }
            b = nb;
        }
        b[n++] = (uint8_t)(acum * 16);
    }
    val_t *v = val_novo(VAL_CADEIA);
    if (!v) { libera(b); return NULL; }
    v->texto = (char *)b;
    v->tam_texto = n;
    return v;
}

static tok_tipo_t lex_tok(lex_t *l, val_t **valor);

/* Lee un arreglo [...] o un diccionario <<...>> (llamado al ver elsymbolo). */
static val_t *lex_estrutura(lex_t *l, bool dicionario)
{
    val_t *v = val_novo(dicionario ? VAL_DICT : VAL_ARR);
    if (!v) return NULL;
    while (l->p < l->n) {
        lex_pular(l);
        if (l->p >= l->n) break;
        uint8_t c = l->d[l->p];
        if (dicionario) {
            if (c == '>') { l->p++; if (l->p < l->n && l->d[l->p] == '>') l->p++; break; }
            if (c != '/') {
                /* una clave que no es un nombre: se salta el valor */
                val_t *k = NULL;
                tok_tipo_t t = lex_tok(l, &k);
                if (t == TOK_FIM) { val_liberar(k); break; }
                if (t != TOK_NOME) { val_liberar(k); continue; }
                char chave[64];
                int nn = k->tam_texto;
                if (nn > 63) nn = 63;
                memcpy(chave, k->texto, (size_t)nn);
                chave[nn] = 0;
                val_liberar(k);
                val_t *val = NULL;
                t = lex_tok(l, &val);
                if (t == TOK_FIM) { val_liberar(val); break; }
                if (t == TOK_DIC_ABRE || t == TOK_ARR_ABRE) { /* ya viene construido */ }
                if (!val_add_entrada(v, chave, val)) { val_liberar(val); break; }
            } else {
                val_t *k = NULL;
                lex_tok(l, &k);
                if (!k) break;
                char chave[64];
                int nn = k->tam_texto;
                if (nn > 63) nn = 63;
                memcpy(chave, k->texto, (size_t)nn);
                chave[nn] = 0;
                val_liberar(k);
                val_t *val = NULL;
                tok_tipo_t t = lex_tok(l, &val);
                if (t == TOK_FIM) { val_liberar(val); break; }
                if (!val_add_entrada(v, chave, val)) { val_liberar(val); break; }
            }
        } else {
            if (c == ']') { l->p++; break; }
            val_t *item = NULL;
            tok_tipo_t t = lex_tok(l, &item);
            if (t == TOK_FIM) { val_liberar(item); break; }
            if (t == TOK_ARR_FECHA || t == TOK_DIC_FECHA) { val_liberar(item); continue; }
            if (!val_add_item(v, item)) { val_liberar(item); break; }
        }
    }
    return v;
}

/* El siguiente token del flujo de contenido. */
static tok_tipo_t lex_tok(lex_t *l, val_t **valor)
{
    lex_pular(l);
    *valor = NULL;
    if (l->p >= l->n) return TOK_FIM;
    uint8_t c = l->d[l->p];

    if (c == '[') { l->p++; *valor = lex_estrutura(l, false); return TOK_ARR_ABRE; }
    if (c == ']') { l->p++; return TOK_ARR_FECHA; }
    if (c == '<') {
        if (l->p + 1 < l->n && l->d[l->p + 1] == '<') {
            l->p += 2;
            *valor = lex_estrutura(l, true);
            return TOK_DIC_ABRE;
        }
        *valor = lex_hex(l);
        return TOK_CADEIA;
    }
    if (c == '>') { l->p++; if (l->p < l->n && l->d[l->p] == '>') l->p++; return TOK_DIC_FECHA; }
    if (c == '(') { *valor = lex_cadeia(l); return TOK_CADEIA; }
    if (c == '/') { *valor = lex_nome(l); return TOK_NOME; }
    if (c == ')' || c == '{' || c == '}') { l->p++; return TOK_PALAVRA; }

    if (c == '+' || c == '-' || c == '.' || (c >= '0' && c <= '9')) {
        double v = 0;
        if (lex_numero(l, &v)) {
            val_t *num = val_novo(VAL_NUMERO);
            if (num) { num->numero = v; *valor = num; }
            return TOK_NUMERO;
        }
    }

    /* Una palabra: un operador (o true/false/null). */
    int k = 0;
    while (l->p < l->n) {
        uint8_t ch = l->d[l->p];
        if (lex_espaco(ch) || lex_delim(ch) || ch == '%') break;
        l->p++;
        if (k < 63) l->pal[k++] = (char)ch;
    }
    l->pal[k] = 0;
    return TOK_PALAVRA;
}

/* La pila de operandos. */
typedef struct {
    val_t **v;
    int n, cap;
} pilha_t;

static void pilha_push(pilha_t *p, val_t *v)
{
    if (!v) return;
    if (p->n >= p->cap) {
        int nova = p->cap ? p->cap * 2 : 32;
        val_t **nv = realoca(p->v, (size_t)nova * sizeof(val_t *));
        if (!nv) { val_liberar(v); return; }
        p->v = nv;
        p->cap = nova;
    }
    p->v[p->n++] = v;
}

static void pilha_limpar(pilha_t *p)
{
    for (int i = 0; i < p->n; i++) val_liberar(p->v[i]);
    p->n = 0;
}

/* --------------------------------------------------------------- caminos */

/* Un camino con varios subtramos: cada "m" abre uno nuevo. Cada subtramo es
 * un pdf_caminho_t (con el motor) y se guarda su punto inicial. */
typedef struct {
    pdf_caminho_t *subs;
    double        *sx, *sy;
    int            n, cap;
} caminho_t;

static void caminho_init(caminho_t *c)
{
    c->subs = NULL;
    c->sx = c->sy = NULL;
    c->n = c->cap = 0;
}

static void caminho_limpar(caminho_t *c)
{
    for (int i = 0; i < c->n; i++) pdf_caminho_limpar(&c->subs[i]);
    c->n = 0;
}

static void caminho_liberar(caminho_t *c)
{
    for (int i = 0; i < c->n; i++) pdf_caminho_liberar(&c->subs[i]);
    if (c->subs) libera(c->subs);
    if (c->sx) libera(c->sx);
    if (c->sy) libera(c->sy);
    caminho_init(c);
}

/* Abre un subtramo nuevo. Devuelve su indice (o -1 si no hay memoria). */
static int caminho_novo(caminho_t *c)
{
    if (c->n >= c->cap) {
        int nova = c->cap ? c->cap * 2 : 8;
        pdf_caminho_t *ns = realoca(c->subs, (size_t)nova * sizeof(pdf_caminho_t));
        if (!ns) return -1;
        c->subs = ns;
        double *nx = realoca(c->sx, (size_t)nova * sizeof(double));
        if (!nx) return -1;
        c->sx = nx;
        double *ny = realoca(c->sy, (size_t)nova * sizeof(double));
        if (!ny) return -1;
        c->sy = ny;
        c->cap = nova;
    }
    int i = c->n++;
    memset(&c->subs[i], 0, sizeof(pdf_caminho_t));
    c->sx[i] = 0;
    c->sy[i] = 0;
    return i;
}

static pdf_caminho_t *caminho_sub(caminho_t *c, int i)
{
    if (i < 0 || i >= c->n) return NULL;
    return &c->subs[i];
}

/* --------------------------------------------------------------- recorte */

/* Un recorte: la lista de caminos (ya aplanados, en pixeles) que acotan el
 * dibujo, con la regla de cada uno. */
typedef struct {
    pdf_caminho_t plana;
    bool          par;     /* true = even-odd */
} recorte_t;

typedef struct {
    int       ref;
    int       n;
    recorte_t *v;
} clip_t;

static clip_t *clip_criar(void)
{
    clip_t *c = aloca_zerado(sizeof *c);
    if (c) c->ref = 1;
    return c;
}

static clip_t *clip_referenciar(clip_t *c)
{
    if (c) c->ref++;
    return c;
}

static void clip_liberar(clip_t *c)
{
    if (!c) return;
    c->ref--;
    if (c->ref > 0) return;
    for (int i = 0; i < c->n; i++) pdf_caminho_liberar(&c->v[i].plana);
    if (c->v) libera(c->v);
    libera(c);
}

static bool clip_adicionar(clip_t *c, const pdf_caminho_t *plana, bool par)
{
    void *novo = realoca(c->v, (size_t)(c->n + 1) * sizeof(recorte_t));
    if (!novo) return false;
    c->v = novo;
    memset(&c->v[c->n], 0, sizeof(recorte_t));
    /* se copia el camino aplanado */
    pdf_caminho_t *dst = &c->v[c->n].plana;
    pdf_caminho_limpar(dst);
    for (int i = 0; i < plana->n; i++)
        if (!pdf_caminho_add(dst, &plana->segs[i])) return false;
    c->v[c->n].par = par;
    c->n++;
    return true;
}

/* Una copia de un recorte (para las formas, que anaden su BBox). */
static clip_t *clip_copiar(clip_t *c)
{
    clip_t *n = clip_criar();
    if (!n) return NULL;
    if (!c) return n;
    for (int i = 0; i < c->n; i++)
        if (!clip_adicionar(n, &c->v[i].plana, c->v[i].par)) {
            clip_liberar(n);
            return NULL;
        }
    return n;
}

/* ------------------------------------------------------- estado grafico */

typedef struct {
    pdf_mat_t ctm;
    cor_t     preench, trazo;
    double    alfa_preench, alfa_trazo;
    double    largura;        /* grosor de linea */
    int       tampao;         /* 0 recto, 1 redondo, 2 cuadrado */
    int       junta;          /* 0 bisel, 1 redondo, 2 miter */
    double    limite_miter;
    double   *guiones;        /* el patron de guiones */
    int       n_guiones;
    double    fase_guiones;
    clip_t   *recorte;
    char      recurso_fonte[64];
    double    tam_fonte;
    double    esp_car, esp_pal, esc_hor, interlinha, subida;
    int       modo_render;    /* Tr */
    pdf_mat_t tm, tlm;
} estado_t;

static void estado_liberar(estado_t *e)
{
    if (e->guiones) libera(e->guiones);
    e->guiones = NULL;
    e->n_guiones = 0;
    if (e->recorte) clip_liberar(e->recorte);
    e->recorte = NULL;
}

/* Una copia del estado (para la pila de q/Q). */
static bool estado_copiar(estado_t *dst, const estado_t *src)
{
    *dst = *src;
    dst->guiones = NULL;
    dst->n_guiones = 0;
    if (src->n_guiones > 0) {
        double *g = aloca((size_t)src->n_guiones * sizeof(double));
        if (!g) return false;
        memcpy(g, src->guiones, (size_t)src->n_guiones * sizeof(double));
        dst->guiones = g;
        dst->n_guiones = src->n_guiones;
    }
    dst->recorte = clip_referenciar(src->recorte);
    return true;
}

/* ------------------------------------------------------- el contexto */

typedef struct {
    pdf_doc_t *doc;
    int        largo, alto;
    uint8_t   *rgba;
    pdf_obj_t *recursos;         /* los recursos de la pagina */
    estado_t   est;
    estado_t  *pilha;            /* la pila de q/Q */
    int        n_pilha, cap_pilha;
    /* buffers del rasterizador */
    double        *cob, *rec;    /* cobertura por pixel */
    pdf_caminho_t  flat;         /* camino aplanado del relleno */
    pdf_caminho_t  plano;        /* scratch */
    double *crux;
    int    *crud;
    int     cap_crux;
    bool    erro_fonte;
    int     espaco_preench, espaco_trazo;
    char    padrao_preench[64], padrao_trazo[64];
    /* o caminho que se esta construindo (en pixeles) */
    caminho_t cam;
    double    cx, cy;         /* o ponto atual */
    bool      tem_atual;
    bool      recorte_pendente;   /* um W/W* a espera de pintar */
    bool      recorte_par;
    /* Extracción de texto (mejora 105): con so_texto no se pinta nada y
     * mostrar_texto apunta cada carácter en 'cars'. */
    bool      so_texto;
    pdf_car_t *cars;
    int       n_cars, cap_cars;
    double    ult_fim_x, ult_y, ult_h;   /* donde acabó el carácter anterior */
} ctx_t;

/* ------------------------------------------------------------- intervalos */

/* Una lista de intervalos [x0, x1] en la linea de barrido (sin solaparse). */
typedef struct {
    double *x0, *x1;
    int     n, cap;
} spans_t;

static void spans_init(spans_t *s)
{
    s->x0 = s->x1 = NULL;
    s->n = s->cap = 0;
}

static void spans_limpar(spans_t *s)
{
    s->n = 0;
}

static void spans_liberar(spans_t *s)
{
    if (s->x0) libera(s->x0);
    if (s->x1) libera(s->x1);
    spans_init(s);
}

static bool spans_add(spans_t *s, double a, double b)
{
    if (b <= a) return true;
    if (s->n >= s->cap) {
        int nova = s->cap ? s->cap * 2 : 16;
        double *n0 = realoca(s->x0, (size_t)nova * sizeof(double));
        if (!n0) return false;
        s->x0 = n0;
        double *n1 = realoca(s->x1, (size_t)nova * sizeof(double));
        if (!n1) return false;
        s->x1 = n1;
        s->cap = nova;
    }
    s->x0[s->n] = a;
    s->x1[s->n] = b;
    s->n++;
    return true;
}

/* La interseccion de dos listas de intervalos (los dos ordenados y sin
 * solaparse). */
static void spans_intersectar(spans_t *a, const spans_t *b)
{
    if (a->n == 0 || b->n == 0) { a->n = 0; return; }
    double *rx0 = NULL, *rx1 = NULL;
    int n = 0, cap = 0;
    for (int i = 0; i < a->n; i++) {
        for (int j = 0; j < b->n; j++) {
            double ini = a->x0[i] > b->x0[j] ? a->x0[i] : b->x0[j];
            double fim = a->x1[i] < b->x1[j] ? a->x1[i] : b->x1[j];
            if (fim <= ini) continue;
            if (n >= cap) {
                int nova = cap ? cap * 2 : 16;
                double *t0 = realoca(rx0, (size_t)nova * sizeof(double));
                if (!t0) goto fim_mem;
                rx0 = t0;
                double *t1 = realoca(rx1, (size_t)nova * sizeof(double));
                if (!t1) goto fim_mem;
                rx1 = t1;
                cap = nova;
            }
            rx0[n] = ini;
            rx1[n] = fim;
            n++;
        }
    }
    if (a->x0) libera(a->x0);
    if (a->x1) libera(a->x1);
    a->x0 = rx0;
    a->x1 = rx1;
    a->n = n;
    a->cap = cap;
    return;
fim_mem:
    if (rx0) libera(rx0);
    if (rx1) libera(rx1);
}

/* ------------------------------------------------------ rasterizacion */

/* Los cruces de los lados del camino con la linea y = sy. */
static int cruzamentos(ctx_t *ctx, const pdf_caminho_t *cam, double sy)
{
    int n = 0;
    for (int i = 0; i < cam->n; i++) {
        const pdf_segmento_t *s = &cam->segs[i];
        double y0 = s->y0, y1 = s->y1;
        if (y0 == y1) continue;
        bool entra;
        if (y0 <= sy) entra = (y1 > sy);
        else entra = (y1 <= sy);
        if (!entra) continue;
        if (n >= ctx->cap_crux) {
            int nova = ctx->cap_crux ? ctx->cap_crux * 2 : 64;
            double *nx = realoca(ctx->crux, (size_t)nova * sizeof(double));
            if (!nx) return 0;
            ctx->crux = nx;
            int *nd = realoca(ctx->crud, (size_t)nova * sizeof(int));
            if (!nd) return 0;
            ctx->crud = nd;
            ctx->cap_crux = nova;
        }
        double t = (sy - y0) / (y1 - y0);
        ctx->crux[n] = s->x0 + t * (s->x1 - s->x0);
        ctx->crud[n] = (y1 > y0) ? 1 : -1;
        n++;
    }
    /* ordenar por x (insercion: son pocos) */
    for (int i = 1; i < n; i++) {
        double x = ctx->crux[i];
        int d = ctx->crud[i];
        int j = i - 1;
        while (j >= 0 && ctx->crux[j] > x) {
            ctx->crux[j + 1] = ctx->crux[j];
            ctx->crud[j + 1] = ctx->crud[j];
            j--;
        }
        ctx->crux[j + 1] = x;
        ctx->crud[j + 1] = d;
    }
    return n;
}

/* Los intervalos donde el camino esta dentro, en la linea y = sy. */
static void spans_do_caminho(ctx_t *ctx, const pdf_caminho_t *cam, bool par, double sy, spans_t *saida)
{
    spans_limpar(saida);
    int n = cruzamentos(ctx, cam, sy);
    if (n < 1) return;
    if (!par) {
        int w = 0;
        double inicio = 0;
        for (int i = 0; i < n; i++) {
            int antes = w;
            w += ctx->crud[i];
            if (antes == 0 && w != 0) inicio = ctx->crux[i];
            else if (antes != 0 && w == 0) spans_add(saida, inicio, ctx->crux[i]);
        }
    } else {
        bool dentro = false;
        double inicio = 0;
        for (int i = 0; i < n; i++) {
            if (!dentro) { dentro = true; inicio = ctx->crux[i]; }
            else { dentro = false; spans_add(saida, inicio, ctx->crux[i]); }
        }
    }
}

/* Os intervalos do recorte (a intersecao de todos os caminhos de recorte)
 * na linha y = sy. Devolve false se nao ha recorte. */
/* Los tramos del recorte en la linea sy. Devuelve -1 si no hay recorte,
 * 0 si el recorte no deja nada en esta linea (antes se confundia con "no
 * hay recorte" y las imagenes se salian de su marco) y 1 si hay tramos. */
static int spans_recorte(ctx_t *ctx, double sy, spans_t *saida, spans_t *tmp)
{
    spans_limpar(saida);
    if (!ctx->est.recorte || ctx->est.recorte->n == 0) return -1;
    bool tem = false;
    for (int i = 0; i < ctx->est.recorte->n; i++) {
        const recorte_t *r = &ctx->est.recorte->v[i];
        spans_do_caminho(ctx, &r->plana, r->par, sy, tmp);
        if (tmp->n == 0) { spans_limpar(saida); return 0; }
        if (!tem) {
            for (int k = 0; k < tmp->n; k++) spans_add(saida, tmp->x0[k], tmp->x1[k]);
            tem = true;
        } else {
            spans_intersectar(saida, tmp);
            if (saida->n == 0) return 0;
        }
    }
    return 1;
}

static double cobertura_recorte(const spans_t *sp, int x)
{
    double cov = 0;
    for (int i = 0; i < sp->n; i++) {
        double a = sp->x0[i], b = sp->x1[i];
        double lo = (a > (double)x) ? a : (double)x;
        double hi = (b < (double)(x + 1)) ? b : (double)(x + 1);
        if (hi > lo) cov += hi - lo;
    }
    return cov > 1.0 ? 1.0 : cov;
}

/* Mezcla un pixel (regla source-over, con alfa recto). */
static void compor(ctx_t *ctx, int x, int y, const cor_t *cor, double cob)
{
    double a = cor->a * cob;
    if (a <= 0.0) return;
    if (a > 1.0) a = 1.0;
    uint8_t *p = ctx->rgba + ((size_t)y * (size_t)ctx->largo + (size_t)x) * 4;
    double ad = (double)p[3] / 255.0;
    double ao = a + ad * (1.0 - a);
    if (ao <= 0.0) { p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 0; return; }
    double f = ad * (1.0 - a);
    p[0] = (uint8_t)(255.0 * (cor->r * a + ((double)p[0] / 255.0) * f) / ao + 0.5);
    p[1] = (uint8_t)(255.0 * (cor->g * a + ((double)p[1] / 255.0) * f) / ao + 0.5);
    p[2] = (uint8_t)(255.0 * (cor->b * a + ((double)p[2] / 255.0) * f) / ao + 0.5);
    p[3] = (uint8_t)(255.0 * ao + 0.5);
}

/* Rellena el camino (ya en pixeles) con la regla indicada. */
#define SUBFILAS 4   /* muestras verticales por pixel */

static void pintar_caminho(ctx_t *ctx, caminho_t *cam, bool par, cor_t cor, double alfa)
{
    if (cam->n == 0 || ctx->so_texto) return;
    cor.a = alfa;

    /* aplanar todos los subtramos en uno solo */
    pdf_caminho_limpar(&ctx->flat);
    double xmin = 0, xmax = 0, ymin = 0, ymax = 0;
    bool primeiro = true;
    for (int i = 0; i < cam->n; i++) {
        if (!pdf_caminho_achatado(&cam->subs[i], &ctx->plano, 0.2)) continue;
        for (int k = 0; k < ctx->plano.n; k++) {
            const pdf_segmento_t *s = &ctx->plano.segs[k];
            if (!pdf_caminho_add(&ctx->flat, s)) break;
            if (primeiro) {
                xmin = xmax = s->x0;
                ymin = ymax = s->y0;
                primeiro = false;
            }
            if (s->x0 < xmin) xmin = s->x0;
            if (s->x0 > xmax) xmax = s->x0;
            if (s->x1 < xmin) xmin = s->x1;
            if (s->x1 > xmax) xmax = s->x1;
            if (s->y0 < ymin) ymin = s->y0;
            if (s->y0 > ymax) ymax = s->y0;
            if (s->y1 < ymin) ymin = s->y1;
            if (s->y1 > ymax) ymax = s->y1;
        }
    }
    if (primeiro) return;

    int y0 = (int)piso_d(ymin);
    int y1 = (int)piso_d(ymax);
    if (y0 < 0) y0 = 0;
    if (y1 > ctx->alto - 1) y1 = ctx->alto - 1;
    int xmin_i = (int)piso_d(xmin), xmax_i = (int)piso_d(xmax);
    if (xmin_i < 0) xmin_i = 0;
    if (xmax_i > ctx->largo - 1) xmax_i = ctx->largo - 1;
    if (xmax_i < xmin_i) return;

    spans_t sp_pint, sp_rec, sp_tmp;
    spans_init(&sp_pint);
    spans_init(&sp_rec);
    spans_init(&sp_tmp);

    /* Cada fila de pixeles se muestrea en SUBFILAS lineas: asi una linea
     * mas fina que un pixel (los filetes de 0,75 pt de las tablas, el
     * recorte fino que usa cairo) sigue saliendo, y los bordes de arriba y
     * abajo de las letras quedan suavizados como los de los lados. */
    for (int y = y0; y <= y1; y++) {
        int xa = ctx->largo, xb = -1;
        for (int k = 0; k < SUBFILAS; k++) {
            double sy = (double)y + ((double)k + 0.5) / (double)SUBFILAS;
            spans_do_caminho(ctx, &ctx->flat, par, sy, &sp_pint);
            if (sp_pint.n == 0) continue;
            int r = spans_recorte(ctx, sy, &sp_rec, &sp_tmp);
            if (r == 0) continue;                 /* el recorte no deja nada */
            if (r > 0) {
                spans_intersectar(&sp_pint, &sp_rec);
                if (sp_pint.n == 0) continue;
            }
            for (int i = 0; i < sp_pint.n; i++) {
                double a = sp_pint.x0[i], b = sp_pint.x1[i];
                int ia = (int)piso_d(a), ib = (int)piso_d(b);
                if (ib < ia) ib = ia;
                if (ia < xmin_i) ia = xmin_i;
                if (ib > xmax_i) ib = xmax_i;
                if (ia < xa) xa = ia;
                if (ib > xb) xb = ib;
                for (int x = ia; x <= ib; x++) {
                    double lo = (a > (double)x) ? a : (double)x;
                    double hi = (b < (double)(x + 1)) ? b : (double)(x + 1);
                    if (hi > lo) ctx->cob[x] += (hi - lo) * (1.0 / (double)SUBFILAS);
                }
            }
        }
        if (xb < xa) continue;
        for (int x = xa; x <= xb; x++) {
            double c = ctx->cob[x];
            ctx->cob[x] = 0.0;
            if (c <= 0.0) continue;
            if (c > 1.0) c = 1.0;
            compor(ctx, x, y, &cor, c);
        }
    }

    spans_liberar(&sp_pint);
    spans_liberar(&sp_rec);
    spans_liberar(&sp_tmp);
}

/* ------------------------------------------------------------- contorno */

/* Una polilinea (los puntos de un subtramo, en pixeles). */
typedef struct {
    double *p;      /* x, y, x, y... */
    int     n, cap;
} linha_t;

static void linha_init(linha_t *l)
{
    l->p = NULL;
    l->n = l->cap = 0;
}

static void linha_limpar(linha_t *l)
{
    l->n = 0;
}

static void linha_liberar(linha_t *l)
{
    if (l->p) libera(l->p);
    linha_init(l);
}

static bool linha_add(linha_t *l, double x, double y)
{
    if (l->n >= l->cap) {
        int nova = l->cap ? l->cap * 2 : 32;
        double *np = realoca(l->p, (size_t)nova * 2 * sizeof(double));
        if (!np) return false;
        l->p = np;
        l->cap = nova;
    }
    l->p[l->n * 2] = x;
    l->p[l->n * 2 + 1] = y;
    l->n++;
    return true;
}

/* Un poligono cerrado con la orientacion corregida (todos hacia el mismo
 * lado: asi la regla non-zero los une). */
static bool poligono_add(caminho_t *destino, const double *pts, int n)
{
    if (n < 3) return true;
    double area = 0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        area += pts[i * 2] * pts[j * 2 + 1] - pts[j * 2] * pts[i * 2 + 1];
    }
    int idx = caminho_novo(destino);
    if (idx < 0) return false;
    pdf_caminho_t *sub = caminho_sub(destino, idx);
    if (!sub) return false;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        if (!pdf_caminho_linha(sub, pts[i * 2], pts[i * 2 + 1], pts[j * 2], pts[j * 2 + 1]))
            return false;
    }
    sub->fechado = true;
    destino->sx[idx] = pts[0];
    destino->sy[idx] = pts[1];
    return true;
}

/* La union de dos arcos (redondos) de una union de linea. */
static void junta_redonda(caminho_t *destino, double px, double py,
                          double vx, double vy, double c, double s, int passos)
{
    double pts[64];
    int n = 0;
    double rx = vx, ry = vy;
    pts[n * 2] = px; pts[n * 2 + 1] = py; n++;
    for (int k = 0; k <= passos && n < 60; k++) {
        pts[n * 2] = px + rx;
        pts[n * 2 + 1] = py + ry;
        n++;
        double t = rx * c - ry * s;
        ry = rx * s + ry * c;
        rx = t;
    }
    poligono_add(destino, pts, n);
}

/* El contorno de una polilinea: los cuadrilateros de cada lado, las uniones
 * y los topes. */
static void construir_contorno(caminho_t *destino, const linha_t *lin, double w,
                               int junta, double limite_miter, int tampao)
{
    double hw = w * 0.5;
    if (hw <= 0.0) return;
    int np = lin->n;
    if (np < 2) {
        /* un punto suelto: solo el tope redondo */
        if (np == 1 && tampao == 1) {
            double pts[32];
            int n = 0;
            for (int k = 0; k < 16; k++) {
                double a = 2.0 * PDF_PI * (double)k / 16.0;
                pts[n * 2] = lin->p[0] + hw * cosseno_d(a);
                pts[n * 2 + 1] = lin->p[1] + hw * seno_d(a);
                n++;
            }
            poligono_add(destino, pts, n);
        }
        return;
    }

    bool fechado = false;
    if (np > 2 && lin->p[0] == lin->p[(np - 1) * 2] && lin->p[1] == lin->p[(np - 1) * 2 + 1]) {
        fechado = true;
        np--;   /* el punto final repite el inicial */
    }

    int ns = np - 1;   /* numero de lados */

    /* los cuadrilateros de cada lado */
    for (int i = 0; i < ns; i++) {
        double x0 = lin->p[i * 2], y0 = lin->p[i * 2 + 1];
        double x1 = lin->p[(i + 1) * 2], y1 = lin->p[(i + 1) * 2 + 1];
        double dx = x1 - x0, dy = y1 - y0;
        double len = raiz_d(dx * dx + dy * dy);
        if (len <= 0.0) continue;
        double ux = dx / len, uy = dy / len;
        double nx = -uy * hw, ny = ux * hw;
        double ex = 0, ey = 0;
        if (tampao == 2) { ex = ux * hw; ey = uy * hw; }
        double pts[8];
        pts[0] = x0 + nx - ex; pts[1] = y0 + ny - ey;
        pts[2] = x1 + nx + ex; pts[3] = y1 + ny + ey;
        pts[4] = x1 - nx + ex; pts[5] = y1 - ny + ey;
        pts[6] = x0 - nx - ex; pts[7] = y0 - ny - ey;
        poligono_add(destino, pts, 4);
    }

    /* las uniones entre lados */
    int n_juntas = fechado ? ns : ns - 1;
    for (int i = 1; i <= n_juntas; i++) {
        int ia = (i - 1) % ns;
        int ib = i % ns;
        double x0 = lin->p[ia * 2], y0 = lin->p[ia * 2 + 1];
        double x1 = lin->p[ib * 2], y1 = lin->p[ib * 2 + 1];
        double px = lin->p[i * 2], py = lin->p[i * 2 + 1];
        double d1x = x1 - x0, d1y = y1 - y0;
        double d2x = px - x1, d2y = py - y1;
        double l1 = raiz_d(d1x * d1x + d1y * d1y);
        double l2 = raiz_d(d2x * d2x + d2y * d2y);
        if (l1 <= 0.0 || l2 <= 0.0) continue;
        d1x /= l1; d1y /= l1;
        d2x /= l2; d2y /= l2;
        double cr = d1x * d2y - d1y * d2x;
        double n1x = -d1y, n1y = d1x;
        double n2x = -d2y, n2y = d2x;
        double s = (cr > 0.0) ? -1.0 : 1.0;    /* el lado de fuera */
        double ax = px + n1x * hw * s, ay = py + n1y * hw * s;
        double bx = px + n2x * hw * s, by = py + n2y * hw * s;

        if (junta == 1) {
            /* redonda: el arco de fuera */
            double c = d1x * d2x + d1y * d2y;
            double sn = cr;
            junta_redonda(destino, px, py, ax - px, ay - py, c, sn, 8);
            continue;
        }

        if (junta == 2) {
            /* miter: el punto donde se cortan las dos rectas paralelas */
            if (fabs_d(cr) > 1e-9) {
                double t = ((bx - ax) * d2y - (by - ay) * d2x) / cr;
                double mx = ax + t * d1x, my = ay + t * d1y;
                double dist = raiz_d((mx - px) * (mx - px) + (my - py) * (my - py));
                if (dist <= limite_miter * hw) {
                    double pts[8];
                    pts[0] = px; pts[1] = py;
                    pts[2] = ax; pts[3] = ay;
                    pts[4] = mx; pts[5] = my;
                    pts[6] = bx; pts[7] = by;
                    poligono_add(destino, pts, 4);
                    continue;
                }
            }
        }
        /* bisel */
        double pts[6];
        pts[0] = px; pts[1] = py;
        pts[2] = ax; pts[3] = ay;
        pts[4] = bx; pts[5] = by;
        poligono_add(destino, pts, 3);
    }

    /* los topes de los extremos */
    if (!fechado && tampao != 0) {
        for (int k = 0; k < 2; k++) {
            int i = (k == 0) ? 0 : ns;
            double px = lin->p[i * 2], py = lin->p[i * 2 + 1];
            if (tampao == 1) {
                double pts[32];
                int n = 0;
                for (int j = 0; j < 16; j++) {
                    double a = 2.0 * PDF_PI * (double)j / 16.0;
                    pts[n * 2] = px + hw * cosseno_d(a);
                    pts[n * 2 + 1] = py + hw * seno_d(a);
                    n++;
                }
                poligono_add(destino, pts, n);
            }
        }
    }
}

/* Los guiones: el patron se repite a lo largo del camino. */
typedef struct {
    double *v;
    int     n;
    double  fase;
} guioes_t;

/* Parte una polilinea en trozos (los que estan "encendidos"). */
static void aplicar_guioes(const guioes_t *g, const linha_t *lin, linha_t **saida, int *n_saida)
{
    *saida = NULL;
    *n_saida = 0;
    int np = lin->n;
    if (!g || g->n <= 0) {
        linha_t *l = aloca_zerado(sizeof *l);
        if (!l) return;
        linha_init(l);
        for (int i = 0; i < np; i++) linha_add(l, lin->p[i * 2], lin->p[i * 2 + 1]);
        *saida = l;
        *n_saida = 1;
        return;
    }

    /* el patron (si tiene un numero impar de elementos, se repite dos veces) */
    int npp = g->n;
    int total = (npp % 2 == 0) ? npp : npp * 2;
    double *pat = aloca((size_t)total * sizeof(double));
    if (!pat) return;
    for (int i = 0; i < total; i++) pat[i] = g->v[i % npp];

    int idx = 0;
    double restante = pat[0];
    double fase = g->fase;
    while (fase > restante) {
        fase -= restante;
        idx = (idx + 1) % total;
        restante = pat[idx];
    }
    restante -= fase;
    bool ligado = (idx % 2 == 0);

    linha_t *atual = NULL;
    int cap = 0, n = 0;
    for (int i = 0; i + 1 < np; i++) {
        double x0 = lin->p[i * 2], y0 = lin->p[i * 2 + 1];
        double x1 = lin->p[(i + 1) * 2], y1 = lin->p[(i + 1) * 2 + 1];
        double dx = x1 - x0, dy = y1 - y0;
        double len = raiz_d(dx * dx + dy * dy);
        if (len <= 0.0) continue;
        double t = 0.0;
        while (t < len - 1e-9) {
            double passo = restante;
            if (passo > len - t) passo = len - t;
            if (passo <= 0.0) break;
            double ax = x0 + dx * (t / len), ay = y0 + dy * (t / len);
            double bx = x0 + dx * ((t + passo) / len), by = y0 + dy * ((t + passo) / len);
            if (ligado) {
                if (!atual) {
                    atual = aloca_zerado(sizeof *atual);
                    if (!atual) break;
                    linha_init(atual);
                    linha_add(atual, ax, ay);
                    if (n >= cap) {
                        int nova = cap ? cap * 2 : 8;
                        linha_t *nv = realoca(*saida, (size_t)nova * sizeof(linha_t));
                        if (!nv) break;
                        *saida = nv;
                        cap = nova;
                    }
                    memset(&(*saida)[n], 0, sizeof(linha_t));
                    linha_init(&(*saida)[n]);
                    (*saida)[n] = *atual;
                    atual = &(*saida)[n];
                    n++;
                }
                linha_add(atual, bx, by);
            } else if (atual) {
                atual = NULL;
            }
            t += passo;
            restante -= passo;
            if (restante <= 1e-9) {
                idx = (idx + 1) % total;
                restante = pat[idx];
                ligado = !ligado;
                if (!ligado) atual = NULL;
            }
        }
    }
    libera(pat);
    *n_saida = n;
}

/* --------------------------------------------------------------- fontes */

typedef struct {
    int codigo;
    int largura;    /* en 1/1000 de em */
} largura_t;

/* Una fuente del PDF, con lo que hace falta para dibujar su texto. */
typedef struct {
    char        recurso[64];      /* el nombre en /Resources /Font */
    char        nome[64];         /* el /BaseFont normalizado */
    char        base[64];         /* el nombre base (sin el estilo) */
    pdf_fonte_t *fonte;
    bool        tipo0;            /* compuesta (CID) */
    bool        valido;
    largura_t  *larguras;         /* los anchos que se saben */
    int         n_larguras;
    double      largura_padrao;    /* dw (tipo0) o el ancho que falta */
    int        *tu_cod, *tu_uni;  /* el /ToUnicode (tipo0) */
    int         n_tu;
    bool        cid_tt;           /* CIDFontType2: el CID pasa por el CIDToGIDMap */
    uint16_t   *cid_gid;          /* el /CIDToGIDMap en stream (NULL = Identity) */
    int         n_cid_gid;
    bool        substituta;       /* se dibuja con una basica en lugar de la suya */
    bool        larg_desordem;    /* los anchos no van en orden de codigo */
} fonte_pdf_t;

static void copiar_n(char *dst, const char *src, int max)
{
    int i = 0;
    if (src) for (; i < max - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

static void fonte_pdf_liberar(fonte_pdf_t *f)
{
    if (f->fonte) pdf_fonte_liberar(f->fonte);
    f->fonte = NULL;
    if (f->larguras) libera(f->larguras);
    f->larguras = NULL;
    f->n_larguras = 0;
    if (f->tu_cod) libera(f->tu_cod);
    if (f->tu_uni) libera(f->tu_uni);
    f->tu_cod = NULL;
    f->tu_uni = NULL;
    f->n_tu = 0;
    if (f->cid_gid) libera(f->cid_gid);
    f->cid_gid = NULL;
    f->n_cid_gid = 0;
}

/* Contiene 'sub' (sin distinguir mayusculas)? */
static bool nome_tem(const char *s, const char *sub)
{
    for (; *s; s++) {
        int i = 0;
        while (sub[i]) {
            char a = s[i], b = sub[i];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
            i++;
        }
        if (!sub[i]) return true;
    }
    return false;
}

/* La basica que mas se parece a una fuente que no esta incrustada (o que
 * no se pudo leer): por el nombre y por los /Flags del FontDescriptor. */
static const char *fonte_parecida(const char *nome, int flags)
{
    static const char *const tabela[3][4] = {
        { "Helvetica", "Helvetica-Bold", "Helvetica-Oblique", "Helvetica-BoldOblique" },
        { "Times-Roman", "Times-Bold", "Times-Italic", "Times-BoldItalic" },
        { "Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique" },
    };
    int fam = 0;
    if ((flags & 1) || nome_tem(nome, "courier") || nome_tem(nome, "mono") ||
        nome_tem(nome, "consol") || nome_tem(nome, "typewriter") || nome_tem(nome, "cmtt"))
        fam = 2;
    else if (((flags & 2) && !nome_tem(nome, "sans")) || nome_tem(nome, "times") ||
             nome_tem(nome, "roman") || nome_tem(nome, "georgia") || nome_tem(nome, "cambria") ||
             nome_tem(nome, "garamond") || nome_tem(nome, "palatino") || nome_tem(nome, "book") ||
             nome_tem(nome, "minion") || nome_tem(nome, "nimbusrom") || nome_tem(nome, "cmr") ||
             (nome_tem(nome, "serif") && !nome_tem(nome, "sans")))
        fam = 1;
    bool negrita = (flags & 0x40000) || nome_tem(nome, "bold") || nome_tem(nome, "black") ||
                   nome_tem(nome, "heavy") || nome_tem(nome, "medi") || nome_tem(nome, "cmbx");
    bool italica = (flags & 0x40) || nome_tem(nome, "italic") || nome_tem(nome, "oblique") ||
                   nome_tem(nome, "cmti");
    return tabela[fam][(negrita ? 1 : 0) + (italica ? 2 : 0)];
}

/* La cache de fuentes: las del documento que se esta dibujando. */
typedef struct {
    pdf_doc_t  *doc;
    pdf_obj_t  *catalogo;
    pdf_obj_t  *obj;        /* el diccionario de la fuente */
    char        recurso[64];
    fonte_pdf_t f;
} cache_fonte_t;

#define CACHE_FONTES 8
static cache_fonte_t cache_fontes[CACHE_FONTES];
static int n_cache_fontes;

static void cache_fontes_limpar(void)
{
    for (int i = 0; i < n_cache_fontes; i++) fonte_pdf_liberar(&cache_fontes[i].f);
    n_cache_fontes = 0;
}

/* Si el documento cambio, se sueltan las fuentes del anterior. */
static void cache_fontes_outro_doc(pdf_doc_t *d)
{
    bool mudou = false;
    for (int i = 0; i < n_cache_fontes; i++)
        if (cache_fontes[i].doc != d || cache_fontes[i].catalogo != pdf_doc_catalogo(d))
            mudou = true;
    if (mudou) cache_fontes_limpar();
}

/* Un objeto del documento (resolviendo las referencias). */
static pdf_obj_t *resolver_ref(pdf_doc_t *d, pdf_obj_t *o)
{
    if (!o) return NULL;
    if (o->tipo == PDF_OBJ_REFERENCIA) return pdf_doc_objeto(d, o->num);
    return o;
}

/* Una entrada de un diccionario (resuelta). */
static pdf_obj_t *dic(pdf_doc_t *d, pdf_obj_t *obj, const char *chave)
{
    if (!obj) return NULL;
    if (obj->tipo != PDF_OBJ_DICCIONARIO && obj->tipo != PDF_OBJ_STREAM) return NULL;
    return resolver_ref(d, pdf_dict_buscar(obj, chave));
}

/* O numero de un objeto del PDF. */
static double obj_numero(const pdf_obj_t *o)
{
    if (!o) return 0.0;
    if (o->tipo == PDF_OBJ_NUMERO) return o->numero;
    if (o->tipo == PDF_OBJ_BOOL) return o->numero;
    return 0.0;
}

/* Quita el prefijo do subconjunto ("ABCDEF+") do nome da fonte. */
static void normalizar_nome(const char *orig, char *dst, int max)
{
    const char *p = orig ? orig : "";
    if (orig) {
        bool subset = true;
        for (int i = 0; i < 6 && orig[i]; i++) {
            char ch = orig[i];
            if (ch < 'A' || ch > 'Z') { subset = false; break; }
        }
        if (subset && orig[6] == '+') p = orig + 7;
    }
    copiar_n(dst, p, max);
}

/* Crecer al doble sin guardar la capacidad (ver pdf_lex.c). */
static bool hay_que_crecer(int n)
{
    return n == 0 || (n >= 4 && (n & (n - 1)) == 0);
}

static size_t capacidade(int n)
{
    return n == 0 ? 4 : (size_t)n * 2;
}

static void largura_add(fonte_pdf_t *f, int cod, int larg)
{
    if (cod < 0) return;
    if (hay_que_crecer(f->n_larguras)) {
        void *novo = realoca(f->larguras, capacidade(f->n_larguras) * sizeof(largura_t));
        if (!novo) return;
        f->larguras = novo;
    }
    if (f->n_larguras > 0 && cod <= f->larguras[f->n_larguras - 1].codigo) f->larg_desordem = true;
    f->larguras[f->n_larguras].codigo = cod;
    f->larguras[f->n_larguras].largura = larg;
    f->n_larguras++;
}

/* O ancho known de um codigo (-1 se nao se sabe). */
static int fonte_largura(fonte_pdf_t *f, int cod)
{
    if (!f->larg_desordem) {
        /* Lo normal: los codigos van en orden (busqueda binaria; una
         * fuente CID puede tener miles de anchos). */
        int a = 0, b = f->n_larguras - 1;
        while (a <= b) {
            int m = (a + b) / 2;
            if (f->larguras[m].codigo == cod) return f->larguras[m].largura;
            if (f->larguras[m].codigo < cod) a = m + 1;
            else b = m - 1;
        }
        return -1;
    }
    /* Desordenados: vale el primero que se anadio (como antes). */
    for (int i = 0; i < f->n_larguras; i++)
        if (f->larguras[i].codigo == cod) return f->larguras[i].largura;
    return -1;
}

/* Las codificaciones de base de las fuentes simples (ISO 32000-1, anexo D),
 * del codigo 32 al 255: el nombre del glifo de cada codigo ("" = ninguno;
 * el 127 no tiene). */
static const char *const enc_winansi[224] = {
    "space", "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand",
    "quotesingle", "parenleft", "parenright", "asterisk", "plus", "comma", "hyphen",
    "period", "slash", "zero", "one", "two", "three", "four", "five", "six", "seven",
    "eight", "nine", "colon", "semicolon", "less", "equal", "greater", "question", "at",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q",
    "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "bracketleft", "backslash",
    "bracketright", "asciicircum", "underscore", "grave", "a", "b", "c", "d", "e", "f",
    "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w",
    "x", "y", "z", "braceleft", "bar", "braceright", "asciitilde", "", "Euro", "",
    "quotesinglbase", "florin", "quotedblbase", "ellipsis", "dagger", "daggerdbl",
    "circumflex", "perthousand", "Scaron", "guilsinglleft", "OE", "", "Zcaron", "", "",
    "quoteleft", "quoteright", "quotedblleft", "quotedblright", "bullet", "endash",
    "emdash", "tilde", "trademark", "scaron", "guilsinglright", "oe", "", "zcaron",
    "Ydieresis", "space", "exclamdown", "cent", "sterling", "currency", "yen",
    "brokenbar", "section", "dieresis", "copyright", "ordfeminine", "guillemotleft",
    "logicalnot", "hyphen", "registered", "macron", "degree", "plusminus",
    "twosuperior", "threesuperior", "acute", "mu", "paragraph", "periodcentered",
    "cedilla", "onesuperior", "ordmasculine", "guillemotright", "onequarter", "onehalf",
    "threequarters", "questiondown", "Agrave", "Aacute", "Acircumflex", "Atilde",
    "Adieresis", "Aring", "AE", "Ccedilla", "Egrave", "Eacute", "Ecircumflex",
    "Edieresis", "Igrave", "Iacute", "Icircumflex", "Idieresis", "Eth", "Ntilde",
    "Ograve", "Oacute", "Ocircumflex", "Otilde", "Odieresis", "multiply", "Oslash",
    "Ugrave", "Uacute", "Ucircumflex", "Udieresis", "Yacute", "Thorn", "germandbls",
    "agrave", "aacute", "acircumflex", "atilde", "adieresis", "aring", "ae", "ccedilla",
    "egrave", "eacute", "ecircumflex", "edieresis", "igrave", "iacute", "icircumflex",
    "idieresis", "eth", "ntilde", "ograve", "oacute", "ocircumflex", "otilde",
    "odieresis", "divide", "oslash", "ugrave", "uacute", "ucircumflex", "udieresis",
    "yacute", "thorn", "ydieresis",
};
static const char *const enc_macroman[224] = {
    "space", "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand",
    "quotesingle", "parenleft", "parenright", "asterisk", "plus", "comma", "hyphen",
    "period", "slash", "zero", "one", "two", "three", "four", "five", "six", "seven",
    "eight", "nine", "colon", "semicolon", "less", "equal", "greater", "question", "at",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q",
    "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "bracketleft", "backslash",
    "bracketright", "asciicircum", "underscore", "grave", "a", "b", "c", "d", "e", "f",
    "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w",
    "x", "y", "z", "braceleft", "bar", "braceright", "asciitilde", "", "Adieresis", "Aring",
    "Ccedilla", "Eacute", "Ntilde", "Odieresis", "Udieresis", "aacute", "agrave",
    "acircumflex", "adieresis", "atilde", "aring", "ccedilla", "eacute", "egrave",
    "ecircumflex", "edieresis", "iacute", "igrave", "icircumflex", "idieresis",
    "ntilde", "oacute", "ograve", "ocircumflex", "odieresis", "otilde", "uacute",
    "ugrave", "ucircumflex", "udieresis", "dagger", "degree", "cent", "sterling",
    "section", "bullet", "paragraph", "germandbls", "registered", "copyright",
    "trademark", "acute", "dieresis", "notequal", "AE", "Oslash", "infinity",
    "plusminus", "lessequal", "greaterequal", "yen", "mu", "partialdiff", "summation",
    "product", "pi", "integral", "ordfeminine", "ordmasculine", "Omega", "ae", "oslash",
    "questiondown", "exclamdown", "logicalnot", "radical", "florin", "approxequal",
    "Delta", "guillemotleft", "guillemotright", "ellipsis", "space", "Agrave", "Atilde",
    "Otilde", "OE", "oe", "endash", "emdash", "quotedblleft", "quotedblright",
    "quoteleft", "quoteright", "divide", "lozenge", "ydieresis", "Ydieresis",
    "fraction", "currency", "guilsinglleft", "guilsinglright", "fi", "fl", "daggerdbl",
    "periodcentered", "quotesinglbase", "quotedblbase", "perthousand", "Acircumflex",
    "Ecircumflex", "Aacute", "Edieresis", "Egrave", "Iacute", "Icircumflex",
    "Idieresis", "Igrave", "Oacute", "Ocircumflex", "apple", "Ograve", "Uacute",
    "Ucircumflex", "Ugrave", "dotlessi", "circumflex", "tilde", "macron", "breve",
    "dotaccent", "ring", "cedilla", "hungarumlaut", "ogonek", "caron",
};

/* Pone la codificacion de base (/WinAnsiEncoding, /MacRomanEncoding) como
 * nombres de glifo: asi el 0x92 de WinAnsi es "quoteright" y no el 0x92 de
 * la codificacion propia de la fuente (StandardEncoding, que no lo tiene). */
static void aplicar_base(fonte_pdf_t *f, const char *nome)
{
    const char *const *tab = NULL;
    if (iguais(nome, "WinAnsiEncoding")) tab = enc_winansi;
    else if (iguais(nome, "MacRomanEncoding")) tab = enc_macroman;
    if (!tab || !f->fonte || f->substituta) return;
    for (int c = 32; c < 256; c++)
        if (tab[c - 32][0]) pdf_fonte_differences(f->fonte, c, tab[c - 32]);
}

/* A tabela /Differences: asigna um nome de glifo a um codigo. */
static void aplicar_diferencas(pdf_doc_t *d, fonte_pdf_t *f, pdf_obj_t *enc)
{
    pdf_obj_t *dif = dic(d, enc, "Differences");
    if (!dif || dif->tipo != PDF_OBJ_ARREGLO) return;
    int cod = 0;
    for (int i = 0; i < dif->n_itens; i++) {
        pdf_obj_t *it = resolver_ref(d, dif->itens[i]);
        if (!it) continue;
        if (it->tipo == PDF_OBJ_NUMERO) {
            cod = (int)it->numero;
        } else if (it->tipo == PDF_OBJ_NOME && f->fonte) {
            pdf_fonte_differences(f->fonte, cod, it->nome);
            cod++;
        }
    }
}

/* Os anchos de uma fonte simples (/FirstChar, /Widths). */
static void ler_larguras_simples(pdf_doc_t *d, fonte_pdf_t *f, pdf_obj_t *dict)
{
    pdf_obj_t *fc = dic(d, dict, "FirstChar");
    pdf_obj_t *ws = dic(d, dict, "Widths");
    if (!fc || fc->tipo != PDF_OBJ_NUMERO) return;
    if (!ws || ws->tipo != PDF_OBJ_ARREGLO) return;
    int first = (int)fc->numero;
    for (int i = 0; i < ws->n_itens; i++) {
        pdf_obj_t *w = resolver_ref(d, ws->itens[i]);
        if (w && w->tipo == PDF_OBJ_NUMERO) largura_add(f, first + i, (int)w->numero);
    }
}

/* Os anchos de uma fonte CID (/W e /DW do descendente). */
static void ler_larguras_cid(pdf_doc_t *d, fonte_pdf_t *f, pdf_obj_t *dict)
{
    pdf_obj_t *desc0 = dic(d, dict, "DescendantFonts");
    if (!desc0 || desc0->tipo != PDF_OBJ_ARREGLO || desc0->n_itens < 1) return;
    pdf_obj_t *cidf = resolver_ref(d, desc0->itens[0]);
    pdf_obj_t *dw = dic(d, cidf, "DW");
    f->largura_padrao = 1000.0;
    if (dw && dw->tipo == PDF_OBJ_NUMERO) f->largura_padrao = dw->numero;
    pdf_obj_t *w = dic(d, cidf, "W");
    if (!w || w->tipo != PDF_OBJ_ARREGLO) return;
    int i = 0;
    while (i < w->n_itens) {
        pdf_obj_t *c1 = resolver_ref(d, w->itens[i]);
        if (!c1 || c1->tipo != PDF_OBJ_NUMERO) { i++; continue; }
        int cod = (int)c1->numero;
        if (i + 1 < w->n_itens) {
            pdf_obj_t *s2 = resolver_ref(d, w->itens[i + 1]);
            if (s2 && s2->tipo == PDF_OBJ_ARREGLO) {
                for (int k = 0; k < s2->n_itens; k++) {
                    pdf_obj_t *wv = resolver_ref(d, s2->itens[k]);
                    if (wv && wv->tipo == PDF_OBJ_NUMERO) largura_add(f, cod + k, (int)wv->numero);
                }
                i += 2;
                continue;
            }
            if (s2 && s2->tipo == PDF_OBJ_NUMERO && i + 2 < w->n_itens) {
                pdf_obj_t *wv = resolver_ref(d, w->itens[i + 2]);
                int c2 = (int)s2->numero;
                if (wv && wv->tipo == PDF_OBJ_NUMERO) {
                    int lim = c2 - cod;
                    if (lim > 4096) lim = 4096;
                    for (int k = 0; k <= lim; k++) largura_add(f, cod + k, (int)wv->numero);
                }
                i += 3;
                continue;
            }
        }
        i++;
    }
}

/* O /ToUnicode: o mapa de codigo para caracter (so para o tipo0). */
static int hex2(const uint8_t *d, int n, int *p)
{
    int v = 0, ndig = 0;
    while (*p < n && ndig < 4) {
        int c = d[*p];
        int h;
        if (c >= '0' && c <= '9') h = c - '0';
        else if (c >= 'a' && c <= 'f') h = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') h = c - 'A' + 10;
        else break;
        v = v * 16 + h;
        (*p)++;
        ndig++;
    }
    return v;
}

/* Le um token hexadecimal (<...>) e devolve o valor. */
static bool ler_hex_token(const uint8_t *d, int n, int *p, int *valor)
{
    while (*p < n && (d[*p] == ' ' || d[*p] == '\r' || d[*p] == '\n' || d[*p] == '\t')) (*p)++;
    if (*p >= n || d[*p] != '<') return false;
    (*p)++;
    *valor = hex2(d, n, p);
    while (*p < n && d[*p] != '>') (*p)++;
    if (*p < n) (*p)++;
    return true;
}

static void tu_add(fonte_pdf_t *f, int cod, int uni)
{
    if (hay_que_crecer(f->n_tu)) {
        void *c = realoca(f->tu_cod, capacidade(f->n_tu) * sizeof(int));
        if (!c) return;
        f->tu_cod = c;
        void *u = realoca(f->tu_uni, capacidade(f->n_tu) * sizeof(int));
        if (!u) return;
        f->tu_uni = u;
    }
    f->tu_cod[f->n_tu] = cod;
    f->tu_uni[f->n_tu] = uni;
    f->n_tu++;
}

static int fonte_tounicode(fonte_pdf_t *f, int cod)
{
    for (int i = 0; i < f->n_tu; i++)
        if (f->tu_cod[i] == cod) return f->tu_uni[i];
    return -1;
}

static void ler_tounicode(fonte_pdf_t *f, pdf_obj_t *cmap)
{
    if (!cmap || cmap->tipo != PDF_OBJ_STREAM || !cmap->datos) return;
    const uint8_t *d = cmap->datos;
    int n = cmap->tam_datos;
    int p = 0;
    while (p < n) {
        /* procura "beginbfchar" ou "beginbfrange" */
        bool achou = false;
        bool rango = false;
        for (int i = p; i + 11 <= n; i++) {
            if (d[i] == 'b' && (memcmp(d + i, "beginbfchar", 11) == 0 ||
                                (i + 12 <= n && memcmp(d + i, "beginbfrange", 12) == 0))) {
                achou = true;
                rango = (d[i + 5] == 'r');
                p = i + (rango ? 12 : 11);
                break;
            }
        }
        if (!achou) break;
        if (!rango) {
            /* pares <cod> <unicode> */
            while (p < n) {
                int c1, c2;
                if (!ler_hex_token(d, n, &p, &c1)) break;
                if (!ler_hex_token(d, n, &p, &c2)) break;
                tu_add(f, c1, c2);
            }
        } else {
            while (p < n) {
                int c1, c2, c3;
                if (!ler_hex_token(d, n, &p, &c1)) break;
                if (!ler_hex_token(d, n, &p, &c2)) break;
                if (p < n && d[p] == '[') {
                    p++;
                    int cod = c1;
                    while (p < n && d[p] != ']') {
                        if (!ler_hex_token(d, n, &p, &c3)) break;
                        tu_add(f, cod, c3);
                        cod++;
                    }
                    if (p < n) p++;
                } else {
                    if (!ler_hex_token(d, n, &p, &c3)) break;
                    for (int k = 0; c1 + k <= c2 && k < 4096; k++)
                        tu_add(f, c1 + k, c3 + k);
                }
            }
        }
    }
}

/* Carrega uma fonte (incrustada ou basica). */
static bool fonte_carregar(ctx_t *ctx, const char *recurso, pdf_obj_t *dict, fonte_pdf_t *f)
{
    memset(f, 0, sizeof *f);
    copiar_n(f->recurso, recurso, 64);

    pdf_obj_t *sub = dic(ctx->doc, dict, "Subtype");
    f->tipo0 = (sub && sub->tipo == PDF_OBJ_NOME && iguais(sub->nome, "Type0"));

    pdf_obj_t *bf = dic(ctx->doc, dict, "BaseFont");
    if (bf && bf->tipo == PDF_OBJ_NOME) normalizar_nome(bf->nome, f->nome, 64);
    copiar_n(f->base, f->nome, 64);
    for (int i = 0; i < 63; i++) {
        if (f->base[i] == '-') { f->base[i] = 0; break; }
        if (!f->base[i]) break;
    }

    /* o programa da fonte, se ha */
    pdf_obj_t *desc = NULL;
    pdf_obj_t *cidf = NULL;
    if (f->tipo0) {
        pdf_obj_t *ds = dic(ctx->doc, dict, "DescendantFonts");
        if (ds && ds->tipo == PDF_OBJ_ARREGLO && ds->n_itens > 0) {
            cidf = resolver_ref(ctx->doc, ds->itens[0]);
            desc = dic(ctx->doc, cidf, "FontDescriptor");
        }
    } else {
        desc = dic(ctx->doc, dict, "FontDescriptor");
    }

    int tipo_prog = 0;
    pdf_obj_t *prog = NULL;
    if (desc) {
        pdf_obj_t *ff3 = dic(ctx->doc, desc, "FontFile3");
        pdf_obj_t *ff2 = dic(ctx->doc, desc, "FontFile2");
        pdf_obj_t *ff1 = dic(ctx->doc, desc, "FontFile");
        if (ff3 && ff3->tipo == PDF_OBJ_STREAM) { prog = ff3; tipo_prog = PDF_FONTE_CFF; }
        else if (ff2 && ff2->tipo == PDF_OBJ_STREAM) { prog = ff2; tipo_prog = PDF_FONTE_TRUETYPE; }
        else if (ff1 && ff1->tipo == PDF_OBJ_STREAM) { prog = ff1; tipo_prog = PDF_FONTE_TYPE1; }
    }
    if (prog && prog->datos && prog->tam_datos > 0)
        f->fonte = pdf_fonte_carregar(tipo_prog, prog->datos, prog->tam_datos);
    if (!f->fonte) {
        /* Sin programa (o ilegible): una de las basicas, la que mas se
         * parezca; "Arial,Bold" y "TimesNewRomanPSMT" tambien valen. */
        f->substituta = true;
        f->fonte = pdf_fonte_basica(f->nome);
        if (!f->fonte && !iguais(f->base, f->nome)) f->fonte = pdf_fonte_basica(f->base);
        if (!f->fonte) {
            pdf_obj_t *fl = desc ? dic(ctx->doc, desc, "Flags") : NULL;
            int flags = (fl && fl->tipo == PDF_OBJ_NUMERO) ? (int)fl->numero : 0;
            f->fonte = pdf_fonte_basica(fonte_parecida(f->nome, flags));
        }
    }
    if (!f->fonte) return false;
    f->valido = true;

    /* CIDFontType2: el CID se pasa a glifo con el /CIDToGIDMap. */
    if (f->tipo0 && cidf) {
        pdf_obj_t *st = dic(ctx->doc, cidf, "Subtype");
        f->cid_tt = (st && st->tipo == PDF_OBJ_NOME && iguais(st->nome, "CIDFontType2"));
        pdf_obj_t *mapa = dic(ctx->doc, cidf, "CIDToGIDMap");
        if (f->cid_tt && mapa && mapa->tipo == PDF_OBJ_STREAM && mapa->datos && mapa->tam_datos >= 2) {
            int n = mapa->tam_datos / 2;
            f->cid_gid = aloca((size_t)n * sizeof(uint16_t));
            if (f->cid_gid) {
                for (int i = 0; i < n; i++)
                    f->cid_gid[i] = (uint16_t)((mapa->datos[2 * i] << 8) | mapa->datos[2 * i + 1]);
                f->n_cid_gid = n;
            }
        }
    }

    /* a codificacao */
    pdf_obj_t *enc = dic(ctx->doc, dict, "Encoding");
    if (enc && !f->tipo0) {
        if (enc->tipo == PDF_OBJ_NOME) aplicar_base(f, enc->nome);
        if (enc->tipo == PDF_OBJ_DICCIONARIO) {
            pdf_obj_t *be = dic(ctx->doc, enc, "BaseEncoding");
            if (be && be->tipo == PDF_OBJ_NOME) aplicar_base(f, be->nome);
            aplicar_diferencas(ctx->doc, f, enc);
        }
    }

    /* os anchos */
    if (f->tipo0) {
        ler_larguras_cid(ctx->doc, f, dict);
        pdf_obj_t *tu = dic(ctx->doc, dict, "ToUnicode");
        if (tu && tu->tipo == PDF_OBJ_STREAM) ler_tounicode(f, tu);
    } else {
        ler_larguras_simples(ctx->doc, f, dict);
        if (desc) {
            pdf_obj_t *mw = dic(ctx->doc, desc, "MissingWidth");
            if (mw && mw->tipo == PDF_OBJ_NUMERO) f->largura_padrao = mw->numero;
        }
    }
    return true;
}

/* A fonte de um recurso (com a cache). */
static fonte_pdf_t *fonte_obter(ctx_t *ctx, const char *recurso, pdf_obj_t *dict)
{
    for (int i = 0; i < n_cache_fontes; i++) {
        if (cache_fontes[i].doc == ctx->doc &&
            iguais(cache_fontes[i].recurso, recurso) &&
            cache_fontes[i].obj == dict)
            return &cache_fontes[i].f;
    }
    if (n_cache_fontes >= CACHE_FONTES) {
        /* recicla a mais antiga */
        fonte_pdf_liberar(&cache_fontes[0].f);
        for (int i = 0; i + 1 < CACHE_FONTES; i++) cache_fontes[i] = cache_fontes[i + 1];
        n_cache_fontes--;
    }
    cache_fonte_t *e = &cache_fontes[n_cache_fontes];
    memset(e, 0, sizeof *e);
    e->doc = ctx->doc;
    e->catalogo = pdf_doc_catalogo(ctx->doc);
    copiar_n(e->recurso, recurso, 64);
    e->obj = dict;
    if (!fonte_carregar(ctx, recurso, dict, &e->f)) {
        fonte_pdf_liberar(&e->f);
        return NULL;
    }
    n_cache_fontes++;
    return &e->f;
}

/* ---------------------------------------------------------- recursos */

/* Um recurso de um dicionario /Resources (pode ser uma referencia). */
static pdf_obj_t *recurso(pdf_doc_t *d, pdf_obj_t *recursos, const char *cat, const char *nome)
{
    if (!recursos || !nome) return NULL;
    pdf_obj_t *c = dic(d, recursos, cat);
    if (!c || c->tipo != PDF_OBJ_DICCIONARIO) return NULL;
    return resolver_ref(d, pdf_dict_buscar(c, nome));
}

/* ------------------------------------------------------------- matrizes */

static bool mat_inversa(const pdf_mat_t *m, pdf_mat_t *r)
{
    double det = m->a * m->d - m->b * m->c;
    if (fabs_d(det) < 1e-12) return false;
    r->a = m->d / det;
    r->b = -m->b / det;
    r->c = -m->c / det;
    r->d = m->a / det;
    r->e = (m->c * m->f - m->d * m->e) / det;
    r->f = (m->b * m->e - m->a * m->f) / det;
    return true;
}

/* --------------------------------------------------------------- cores */

static cor_t cor_componentes(int espaco, const double *c, int n)
{
    cor_t r;
    r.a = 1.0;
    r.r = r.g = r.b = 0.0;
    if (espaco == ESP_RGB) {
        r.r = (n > 0) ? c[0] : 0.0;
        r.g = (n > 1) ? c[1] : 0.0;
        r.b = (n > 2) ? c[2] : 0.0;
    } else if (espaco == ESP_CMYK) {
        double ci = (n > 0) ? c[0] : 0.0;
        double mi = (n > 1) ? c[1] : 0.0;
        double ya = (n > 2) ? c[2] : 0.0;
        double k = (n > 3) ? c[3] : 0.0;
        r.r = (1.0 - ci) * (1.0 - k);
        r.g = (1.0 - mi) * (1.0 - k);
        r.b = (1.0 - ya) * (1.0 - k);
    } else {
        double g = (n > 0) ? c[0] : 0.0;
        r.r = r.g = r.b = g;
    }
    r.r = limitar(r.r, 0.0, 1.0);
    r.g = limitar(r.g, 0.0, 1.0);
    r.b = limitar(r.b, 0.0, 1.0);
    return r;
}

/* Quantos componentes tem um espaco de cor (1 = cinza, 3 = rgb, 4 = cmyk). */
static int ncomp_espaco(pdf_doc_t *d, pdf_obj_t *cs)
{
    if (!cs) return 1;
    if (cs->tipo == PDF_OBJ_NOME) {
        if (iguais(cs->nome, "DeviceRGB") || iguais(cs->nome, "CalRGB") || iguais(cs->nome, "Lab"))
            return 3;
        if (iguais(cs->nome, "DeviceCMYK")) return 4;
        return 1;
    }
    if (cs->tipo == PDF_OBJ_ARREGLO && cs->n_itens > 0) {
        pdf_obj_t *n = resolver_ref(d, cs->itens[0]);
        if (n && n->tipo == PDF_OBJ_NOME) {
            if (iguais(n->nome, "Indexed")) {
                if (cs->n_itens > 1) return ncomp_espaco(d, resolver_ref(d, cs->itens[1]));
                return 1;
            }
            if (iguais(n->nome, "ICCBased")) {
                pdf_obj_t *st = resolver_ref(d, cs->itens[1]);
                pdf_obj_t *nn = dic(d, st, "N");
                if (nn && nn->tipo == PDF_OBJ_NUMERO) return (int)nn->numero;
                return 3;
            }
            if (iguais(n->nome, "DeviceN")) {
                /* [/DeviceN [nombres] alternativo funcion]: un componente
                 * por nombre. */
                pdf_obj_t *nomes = cs->n_itens > 1 ? resolver_ref(d, cs->itens[1]) : NULL;
                if (nomes && nomes->tipo == PDF_OBJ_ARREGLO && nomes->n_itens > 0) return nomes->n_itens;
                return 1;
            }
            if (iguais(n->nome, "Separation")) return 1;
            if (iguais(n->nome, "CalRGB") || iguais(n->nome, "Lab")) return 3;
            if (iguais(n->nome, "DeviceCMYK")) return 4;
            if (iguais(n->nome, "DeviceRGB")) return 3;
            return 1;
        }
    }
    return 1;
}

/* ------------------------------------------------------------ caminhos */

static void cam_transformar(ctx_t *ctx, double ux, double uy, double *dx, double *dy)
{
    pdf_mat_aplica(&ctx->est.ctm, ux, uy, dx, dy);
}

/* m: abre um subtramo novo. */
static void cam_m(ctx_t *ctx, double x, double y)
{
    double dx, dy;
    cam_transformar(ctx, x, y, &dx, &dy);
    int idx = caminho_novo(&ctx->cam);
    if (idx < 0) return;
    ctx->cam.sx[idx] = dx;
    ctx->cam.sy[idx] = dy;
    ctx->cx = dx;
    ctx->cy = dy;
    ctx->tem_atual = true;
}

/* l: uma linha ate (x, y). */
static void cam_l(ctx_t *ctx, double x, double y)
{
    if (!ctx->tem_atual) { cam_m(ctx, x, y); return; }
    double dx, dy;
    cam_transformar(ctx, x, y, &dx, &dy);
    if (ctx->cam.n > 0) {
        pdf_caminho_t *sub = caminho_sub(&ctx->cam, ctx->cam.n - 1);
        if (sub) pdf_caminho_linha(sub, ctx->cx, ctx->cy, dx, dy);
    }
    ctx->cx = dx;
    ctx->cy = dy;
}

/* c: uma curva cubica. */
static void cam_c(ctx_t *ctx, double x1, double y1, double x2, double y2, double x3, double y3)
{
    if (!ctx->tem_atual) { cam_m(ctx, x1, y1); return; }
    double cx1, cy1, cx2, cy2, dx, dy;
    cam_transformar(ctx, x1, y1, &cx1, &cy1);
    cam_transformar(ctx, x2, y2, &cx2, &cy2);
    cam_transformar(ctx, x3, y3, &dx, &dy);
    if (ctx->cam.n > 0) {
        pdf_caminho_t *sub = caminho_sub(&ctx->cam, ctx->cam.n - 1);
        if (sub) pdf_caminho_cubica(sub, ctx->cx, ctx->cy, cx1, cy1, cx2, cy2, dx, dy);
    }
    ctx->cx = dx;
    ctx->cy = dy;
}

/* h: fecha o subtramo. */
static void cam_h(ctx_t *ctx)
{
    if (!ctx->tem_atual || ctx->cam.n == 0) return;
    int idx = ctx->cam.n - 1;
    double dx = ctx->cam.sx[idx], dy = ctx->cam.sy[idx];
    if (dx != ctx->cx || dy != ctx->cy) {
        pdf_caminho_t *sub = caminho_sub(&ctx->cam, idx);
        if (sub) pdf_caminho_linha(sub, ctx->cx, ctx->cy, dx, dy);
    }
    ctx->cam.subs[idx].fechado = true;
    ctx->cx = dx;
    ctx->cy = dy;
}

/* re: um retangulo. */
static void cam_re(ctx_t *ctx, double x, double y, double w, double h)
{
    double x0, y0, x1, y1, x2, y2, x3, y3;
    cam_transformar(ctx, x, y, &x0, &y0);
    cam_transformar(ctx, x + w, y, &x1, &y1);
    cam_transformar(ctx, x + w, y + h, &x2, &y2);
    cam_transformar(ctx, x, y + h, &x3, &y3);
    int idx = caminho_novo(&ctx->cam);
    if (idx < 0) return;
    pdf_caminho_t *sub = caminho_sub(&ctx->cam, idx);
    if (!sub) return;
    pdf_caminho_linha(sub, x0, y0, x1, y1);
    pdf_caminho_linha(sub, x1, y1, x2, y2);
    pdf_caminho_linha(sub, x2, y2, x3, y3);
    pdf_caminho_linha(sub, x3, y3, x0, y0);
    sub->fechado = true;
    ctx->cam.sx[idx] = x0;
    ctx->cam.sy[idx] = y0;
    ctx->cx = x0;
    ctx->cy = y0;
    ctx->tem_atual = true;
}

/* Aplana um caminho (todos os subtramos) em um so. */
static void aplanar_caminho(ctx_t *ctx, caminho_t *cam, pdf_caminho_t *destino)
{
    pdf_caminho_limpar(destino);
    for (int i = 0; i < cam->n; i++) {
        if (!pdf_caminho_achatado(&cam->subs[i], &ctx->plano, 0.2)) continue;
        for (int k = 0; k < ctx->plano.n; k++)
            if (!pdf_caminho_add(destino, &ctx->plano.segs[k])) return;
    }
}

/* ---------------------------------------------------------- pintura */

/* Preenche um caminho. */
static void pintar_preench(ctx_t *ctx, caminho_t *cam, bool par)
{
    pintar_caminho(ctx, cam, par, ctx->est.preench, ctx->est.alfa_preench);
}

/* Contorna um caminho. */
static void pintar_contorno(ctx_t *ctx, caminho_t *cam)
{
    double esc = pdf_mat_escala_media(&ctx->est.ctm);
    double w = ctx->est.largura * esc;
    /* Ancho 0 = la linea mas fina que se pueda: un pixel. Las de menos de
     * un pixel tambien salen con uno (para que no se pierdan los filetes),
     * pero mas claras, como las ve el ojo (y los demas lectores). */
    double atenua = 1.0;
    if (w < 1.0) {
        atenua = (w > 0.0) ? w : 1.0;
        if (atenua < 0.35) atenua = 0.35;
        w = 1.0;
    }
    guioes_t g;
    g.v = ctx->est.guiones;
    g.n = ctx->est.n_guiones;
    g.fase = ctx->est.fase_guiones;

    caminho_t contorno;
    caminho_init(&contorno);
    linha_t lin;
    linha_init(&lin);
    for (int i = 0; i < cam->n; i++) {
        linha_limpar(&lin);
        if (!pdf_caminho_achatado(&cam->subs[i], &ctx->plano, 0.2)) continue;
        if (ctx->plano.n > 0) {
            linha_add(&lin, ctx->plano.segs[0].x0, ctx->plano.segs[0].y0);
            for (int k = 0; k < ctx->plano.n; k++)
                linha_add(&lin, ctx->plano.segs[k].x1, ctx->plano.segs[k].y1);
        } else if (cam->subs[i].n == 0) {
            /* um ponto solto: so o topo redondo */
            linha_add(&lin, ctx->cam.sx[i], ctx->cam.sy[i]);
        }
        if (lin.n < 1) continue;
        linha_t *pedacos = NULL;
        int n_ped = 0;
        aplicar_guioes(&g, &lin, &pedacos, &n_ped);
        for (int k = 0; k < n_ped; k++)
            construir_contorno(&contorno, &pedacos[k], w, ctx->est.junta,
                               ctx->est.limite_miter, ctx->est.tampao);
        for (int k = 0; k < n_ped; k++) linha_liberar(&pedacos[k]);
        if (pedacos) libera(pedacos);
    }
    linha_liberar(&lin);
    pintar_caminho(ctx, &contorno, false, ctx->est.trazo, ctx->est.alfa_trazo * atenua);
    caminho_liberar(&contorno);
}

/* Aplica o recorte pendente (W/W*). */
static void aplicar_recorte(ctx_t *ctx, bool par)
{
    if (!ctx->est.recorte) ctx->est.recorte = clip_criar();
    if (!ctx->est.recorte) return;
    aplanar_caminho(ctx, &ctx->cam, &ctx->flat);
    clip_adicionar(ctx->est.recorte, &ctx->flat, par);
}

/* Um operador de pintura: 0 = nada, 1 = preencher, 2 = contornar, 3 = os dois. */
static void pintar_op(ctx_t *ctx, int op, bool par)
{
    if (ctx->recorte_pendente) {
        aplicar_recorte(ctx, ctx->recorte_par);
        ctx->recorte_pendente = false;
    }
    if (op & 1) pintar_preench(ctx, &ctx->cam, par);
    if (op & 2) pintar_contorno(ctx, &ctx->cam);
    caminho_limpar(&ctx->cam);
    ctx->tem_atual = false;
}

/* -------------------------------------------------------------- texto */

/* A matriz do texto: a escala da fonte, a subida e a CTM. */
static pdf_mat_t matriz_texto(ctx_t *ctx)
{
    double ts = ctx->est.tam_fonte;
    double th = ctx->est.esc_hor / 100.0;
    pdf_mat_t s = pdf_mat_identidade();
    s.a = ts * th / 1000.0;
    s.d = ts / 1000.0;
    s.f = ctx->est.subida;
    /* A ordem e a da norma: primeiro a escala do glifo, depois a matriz de
     * texto e depois a CTM. */
    pdf_mat_t mt = pdf_mat_multiplica(&s, &ctx->est.tm);
    return pdf_mat_multiplica(&mt, &ctx->est.ctm);
}

/* Desenha o contorno de um glifo (o caminho vai em pixels). */
static void desenhar_glifo(ctx_t *ctx, const pdf_glifo_t *g, const pdf_mat_t *mat, int modo)
{
    if (g->contorno.n == 0) return;
    caminho_t cam;
    caminho_init(&cam);
    int idx = caminho_novo(&cam);
    if (idx < 0) return;
    pdf_caminho_t *sub = caminho_sub(&cam, idx);
    if (!sub) { caminho_liberar(&cam); return; }
    for (int i = 0; i < g->contorno.n; i++) {
        const pdf_segmento_t *s = &g->contorno.segs[i];
        double x0, y0, x1, y1;
        pdf_mat_aplica(mat, s->x0, s->y0, &x0, &y0);
        pdf_mat_aplica(mat, s->x1, s->y1, &x1, &y1);
        if (s->tipo == PDF_TRA_CUBICA) {
            double cx1, cy1, cx2, cy2;
            pdf_mat_aplica(mat, s->cx1, s->cy1, &cx1, &cy1);
            pdf_mat_aplica(mat, s->cx2, s->cy2, &cx2, &cy2);
            pdf_caminho_cubica(sub, x0, y0, cx1, cy1, cx2, cy2, x1, y1);
        } else {
            pdf_caminho_linha(sub, x0, y0, x1, y1);
        }
    }
    if (modo == 1 || modo == 2) {
        /* o contorno: a largura e a da fonte, escalada pela matriz do texto */
        double esc = pdf_mat_escala_media(mat);
        double w = ctx->est.largura * esc;
        if (w > 0.0) {
            guioes_t g2;
            g2.v = NULL;
            g2.n = 0;
            g2.fase = 0;
            linha_t lin;
            linha_init(&lin);
            if (!pdf_caminho_achatado(sub, &ctx->plano, 0.15)) {
                linha_liberar(&lin);
                caminho_liberar(&cam);
                return;
            }
            if (ctx->plano.n > 0) {
                linha_add(&lin, ctx->plano.segs[0].x0, ctx->plano.segs[0].y0);
                for (int k = 0; k < ctx->plano.n; k++)
                    linha_add(&lin, ctx->plano.segs[k].x1, ctx->plano.segs[k].y1);
            }
            linha_t *pedacos = NULL;
            int n_ped = 0;
            aplicar_guioes(&g2, &lin, &pedacos, &n_ped);
            caminho_t contorno;
            caminho_init(&contorno);
            for (int k = 0; k < n_ped; k++)
                construir_contorno(&contorno, &pedacos[k], w, ctx->est.junta,
                                   ctx->est.limite_miter, ctx->est.tampao);
            for (int k = 0; k < n_ped; k++) linha_liberar(&pedacos[k]);
            if (pedacos) libera(pedacos);
            linha_liberar(&lin);
            pintar_caminho(ctx, &contorno, false, ctx->est.trazo, ctx->est.alfa_trazo);
            caminho_liberar(&contorno);
        }
    }
    if (modo == 0 || modo == 2 || modo >= 4)
        pintar_caminho(ctx, &cam, false, ctx->est.preench, ctx->est.alfa_preench);
    caminho_liberar(&cam);
}

/* Baixa uma linha (T*). */
static void texto_avancar_linha(ctx_t *ctx)
{
    pdf_mat_t t = pdf_mat_translada(0.0, -ctx->est.interlinha);
    ctx->est.tlm = pdf_mat_multiplica(&t, &ctx->est.tlm);
    ctx->est.tm = ctx->est.tlm;
}

/* Apunta un carácter para la búsqueda de texto: su Unicode y la caja en
 * décimas de punto (y hacia abajo). Mete un espacio si hay salto de línea o
 * un hueco grande (los PDF casi nunca llevan espacios entre palabras). */
static void car_push(ctx_t *ctx, int uni, int x, int y, int l, int a)
{
    if (ctx->n_cars >= 400000) return;
    if (ctx->n_cars >= ctx->cap_cars) {
        int nc = ctx->cap_cars ? ctx->cap_cars * 2 : 512;
        pdf_car_t *n = realoca(ctx->cars, (size_t)nc * sizeof(pdf_car_t));
        if (!n) return;
        ctx->cars = n;
        ctx->cap_cars = nc;
    }
    pdf_car_t *c = &ctx->cars[ctx->n_cars++];
    c->uni = uni; c->x = x; c->y = y; c->l = l; c->a = a;
}

static void anotar_car(ctx_t *ctx, fonte_pdf_t *f, int cod, int w0, const pdf_mat_t *mat)
{
    int uni = fonte_tounicode(f, cod);
    if (uni < 0) uni = f->tipo0 ? 0 : cod;
    if (uni < 32 && uni != 0) uni = 32;
    if (uni == 0) return;
    double x0, y0, x1, y1, x2, y2;
    pdf_mat_aplica(mat, 0.0, 0.0, &x0, &y0);
    pdf_mat_aplica(mat, (double)w0, 0.0, &x1, &y1);
    pdf_mat_aplica(mat, 0.0, 1000.0, &x2, &y2);
    double alto = fabs_d(y2 - y0) + fabs_d(x2 - x0);     /* el cuerpo de la fuente */
    double ancho = fabs_d(x1 - x0) + fabs_d(y1 - y0);
    double xi = x1 < x0 ? x1 : x0;
    if (alto < 1.0) alto = 1.0;
    /* Un hueco o un salto de línea entre este y el anterior: un espacio. */
    if (ctx->n_cars > 0 && uni != 32 && ctx->cars[ctx->n_cars - 1].uni != 32) {
        double dy = fabs_d(y0 - ctx->ult_y);
        double hueco = xi - ctx->ult_fim_x;
        if (dy > ctx->ult_h * 0.5 || hueco > ctx->ult_h * 0.18 || hueco < -ctx->ult_h * 2.0)
            car_push(ctx, 32, (int)(ctx->ult_fim_x * 10), (int)((ctx->ult_y - ctx->ult_h * 0.8) * 10), 0,
                     (int)(ctx->ult_h * 10));
    }
    car_push(ctx, uni, (int)(xi * 10), (int)((y0 - alto * 0.8) * 10), (int)(ancho * 10), (int)(alto * 10));
    ctx->ult_fim_x = xi + ancho;
    ctx->ult_y = y0;
    ctx->ult_h = alto;
}

/* Desenha uma sequencia de bytes. */
static void mostrar_texto(ctx_t *ctx, const uint8_t *bytes, int n)
{
    if (n <= 0) return;
    fonte_pdf_t *f = NULL;
    if (ctx->est.recurso_fonte[0]) {
        pdf_obj_t *fd = recurso(ctx->doc, ctx->recursos, "Font", ctx->est.recurso_fonte);
        if (fd) f = fonte_obter(ctx, ctx->est.recurso_fonte, fd);
    }
    if (!f || !f->fonte) {
        ctx->erro_fonte = true;
        return;
    }
    int i = 0;
    while (i < n) {
        /* A matriz muda em cada glifo: o avanco vai acumulando na matriz de
         * texto. */
        pdf_mat_t mat = matriz_texto(ctx);
        int cod, usados;
        if (f->tipo0) {
            if (i + 1 >= n) break;
            cod = ((int)bytes[i] << 8) | (int)bytes[i + 1];
            usados = 2;
        } else {
            cod = (int)bytes[i];
            usados = 1;
        }
        i += usados;

        pdf_glifo_t glifo;
        memset(&glifo, 0, sizeof glifo);
        bool ok;
        if (ctx->so_texto) {
            ok = false;   /* solo se quiere el texto: no hace falta el contorno */
        } else if (f->tipo0 && !f->substituta) {
            int g = cod;
            if (f->cid_tt && f->cid_gid) g = (cod < f->n_cid_gid) ? f->cid_gid[cod] : 0;
            ok = pdf_fonte_glifo_cid(f->fonte, g, &glifo);
        } else if (f->tipo0) {
            /* Una basica en lugar de una fuente CID: por el Unicode. */
            int uni = fonte_tounicode(f, cod);
            ok = (uni > 0 && uni < 256) ? pdf_fonte_glifo(f->fonte, uni, &glifo) : false;
        } else {
            ok = pdf_fonte_glifo(f->fonte, cod, &glifo);
        }

        int w0 = fonte_largura(f, cod);
        if (w0 < 0 && f->fonte) {
            w0 = pdf_fonte_largura(f->fonte, cod);
            if (w0 < 0) w0 = 0;
        }
        if (w0 < 0) w0 = (int)f->largura_padrao;

        if (ok && ctx->est.modo_render != 3 && !ctx->so_texto)
            desenhar_glifo(ctx, &glifo, &mat, ctx->est.modo_render);
        if (ok) pdf_glifo_liberar(&glifo);
        if (ctx->so_texto) anotar_car(ctx, f, cod, w0, &mat);

        /* o avanco */
        double extra = ctx->est.esp_car;
        if (!f->tipo0 && cod == 32) extra += ctx->est.esp_pal;
        double tx = ((w0 / 1000.0) * ctx->est.tam_fonte + extra) * (ctx->est.esc_hor / 100.0);
        pdf_mat_t t = pdf_mat_translada(tx, 0.0);
        ctx->est.tm = pdf_mat_multiplica(&t, &ctx->est.tm);
    }
}

/* ------------------------------------------------------------ imagens */

/* A descricao dos samples de uma imagem. */
typedef struct {
    int   larg, alt;
    int   bpc;             /* bits por componente */
    int   ncomp;           /* componentes por pixel */
    bool  indexada;
    uint8_t *paleta;       /* 3 bytes por indice */
    int   n_cores;
    bool  mascara;         /* /ImageMask */
    bool  inverter;        /* /Decode [1 0] */
    const uint8_t *dados;
    int   tam;
    int   bytes_linha;
} img_am_t;

typedef struct img_des_t {
    img_am_t am;
    bool      tem_jpeg;
    imagem_t  jpg;
    struct img_des_t *smask;   /* a mascara de alfa (/SMask) */
} img_des_t;

static void liberar_img(img_des_t *im)
{
    if (!im) return;
    if (im->am.paleta) libera(im->am.paleta);
    im->am.paleta = NULL;
    if (im->tem_jpeg) imagem_liberar(&im->jpg);
    if (im->smask) {
        liberar_img(im->smask);
        libera(im->smask);
        im->smask = NULL;
    }
}

/* O valor de um componente (ja no maximo do bpc). */
static int img_valor(const img_am_t *im, int x, int y, int comp)
{
    int bpc = im->bpc;
    if (bpc <= 0) return 0;
    int off = (y * im->bytes_linha * 8) + ((x * im->ncomp + comp) * bpc);
    if (off < 0) return 0;
    int byte = off / 8;
    if (byte >= im->tam) return 0;
    if (bpc == 8) return im->dados[byte];
    if (bpc == 16) {
        if (byte + 1 >= im->tam) return 0;
        return ((int)im->dados[byte] << 8) | (int)im->dados[byte + 1];
    }
    int shift = 8 - bpc - (off % 8);
    int mask = (1 << bpc) - 1;
    return (im->dados[byte] >> shift) & mask;
}

/* A paleta de um espaco Indexed. */
static bool paleta_indexed(pdf_doc_t *d, pdf_obj_t *cs, uint8_t **paleta, int *n_cores)
{
    if (!cs || cs->tipo != PDF_OBJ_ARREGLO || cs->n_itens < 4) return false;
    pdf_obj_t *n = resolver_ref(d, cs->itens[0]);
    if (!n || n->tipo != PDF_OBJ_NOME || !iguais(n->nome, "Indexed")) return false;
    int nc = ncomp_espaco(d, resolver_ref(d, cs->itens[1]));
    pdf_obj_t *hv = resolver_ref(d, cs->itens[2]);
    int hival = (hv && hv->tipo == PDF_OBJ_NUMERO) ? (int)hv->numero : 0;
    if (hival < 0 || hival > 4095) return false;
    pdf_obj_t *look = resolver_ref(d, cs->itens[3]);
    const uint8_t *dados = NULL;
    int tam = 0;
    if (look && look->tipo == PDF_OBJ_STREAM) { dados = look->datos; tam = look->tam_datos; }
    else if (look && look->tipo == PDF_OBJ_CADENA) { dados = (const uint8_t *)look->cadena; tam = look->tam_cadena; }
    if (!dados || tam <= 0) return false;
    int ncores = hival + 1;
    uint8_t *p = aloca((size_t)ncores * 3);
    if (!p) return false;
    for (int i = 0; i < ncores; i++) {
        double comp[4];
        for (int c = 0; c < 4; c++) comp[c] = 0.0;
        for (int c = 0; c < nc; c++) {
            int b = i * nc + c;
            if (b >= tam) break;
            comp[c] = (double)dados[b] / 255.0;
        }
        int esp = (nc == 3) ? ESP_RGB : ((nc == 4) ? ESP_CMYK : ESP_GRAY);
        cor_t cor = cor_componentes(esp, comp, nc);
        p[i * 3 + 0] = (uint8_t)(cor.r * 255.0 + 0.5);
        p[i * 3 + 1] = (uint8_t)(cor.g * 255.0 + 0.5);
        p[i * 3 + 2] = (uint8_t)(cor.b * 255.0 + 0.5);
    }
    *paleta = p;
    *n_cores = ncores;
    return true;
}

/* Prepara uma imagem (com os dados ja descomprimidos, ou um JPEG). */
static bool preparar_imagem(ctx_t *ctx, pdf_obj_t *dict, const uint8_t *dados, int tam, img_des_t *des)
{
    memset(des, 0, sizeof *des);
    pdf_obj_t *w = dic(ctx->doc, dict, "Width");
    pdf_obj_t *h = dic(ctx->doc, dict, "Height");
    if (!w || w->tipo != PDF_OBJ_NUMERO || !h || h->tipo != PDF_OBJ_NUMERO) return false;
    int larg = (int)w->numero, alt = (int)h->numero;
    if (larg <= 0 || alt <= 0 || larg > 8192 || alt > 8192) return false;

    const char *filtro = NULL;
    pdf_obj_t *f = dic(ctx->doc, dict, "Filter");
    if (f && f->tipo == PDF_OBJ_NOME) filtro = f->nome;
    else if (f && f->tipo == PDF_OBJ_ARREGLO && f->n_itens > 0) {
        /* Con varios filtros manda el ultimo (p. ej. [/FlateDecode /DCTDecode]):
         * los anteriores ya los quito pdf_lex.c. */
        pdf_obj_t *f0 = resolver_ref(ctx->doc, f->itens[f->n_itens - 1]);
        if (f0 && f0->tipo == PDF_OBJ_NOME) filtro = f0->nome;
    }
    /* JPEG 2000 y JBIG2 no se saben leer: mejor no dibujar nada que dibujar
     * los bytes comprimidos como si fueran pixeles (eso es basura). El
     * CCITT ya llega descomprimido (pdf_lex.c). */
    if (filtro && (iguais(filtro, "JPXDecode") || iguais(filtro, "JBIG2Decode"))) return false;

    pdf_obj_t *msk = dic(ctx->doc, dict, "ImageMask");
    bool mascara = (msk && msk->tipo == PDF_OBJ_BOOL && msk->booleano);

    img_am_t *am = &des->am;
    am->larg = larg;
    am->alt = alt;
    am->mascara = mascara;

    if (filtro && (iguais(filtro, "DCTDecode") || iguais(filtro, "DCT"))) {
        if (!dados || tam <= 0) return false;
        if (imagem_ler_jpeg(dados, tam, &des->jpg) != 0) return false;
        des->tem_jpeg = true;
        return true;
    }

    am->dados = dados;
    am->tam = tam;
    am->bpc = 8;
    pdf_obj_t *bpc = dic(ctx->doc, dict, "BitsPerComponent");
    if (bpc && bpc->tipo == PDF_OBJ_NUMERO) am->bpc = (int)bpc->numero;
    if (mascara) {
        am->bpc = 1;
        am->ncomp = 1;
    } else {
        pdf_obj_t *cs = dic(ctx->doc, dict, "ColorSpace");
        am->ncomp = ncomp_espaco(ctx->doc, cs);
        if (cs && cs->tipo == PDF_OBJ_ARREGLO) {
            uint8_t *pal = NULL;
            int nc = 0;
            if (paleta_indexed(ctx->doc, cs, &pal, &nc)) {
                am->paleta = pal;
                am->n_cores = nc;
                am->indexada = true;
                am->ncomp = 1;
            }
        }
    }
    if (am->bpc != 1 && am->bpc != 2 && am->bpc != 4 && am->bpc != 8 && am->bpc != 16) return false;
    am->bytes_linha = (larg * am->ncomp * am->bpc + 7) / 8;

    pdf_obj_t *dec = dic(ctx->doc, dict, "Decode");
    if (dec && dec->tipo == PDF_OBJ_ARREGLO && dec->n_itens >= 2) {
        pdf_obj_t *a = resolver_ref(ctx->doc, dec->itens[0]);
        pdf_obj_t *b = resolver_ref(ctx->doc, dec->itens[1]);
        if (a && b && a->tipo == PDF_OBJ_NUMERO && b->tipo == PDF_OBJ_NUMERO && a->numero > b->numero)
            am->inverter = true;
    }

    pdf_obj_t *sm = dic(ctx->doc, dict, "SMask");
    if (sm && sm->tipo == PDF_OBJ_STREAM) {
        img_des_t *s = aloca_zerado(sizeof *s);
        if (s) {
            if (preparar_imagem(ctx, sm, sm->datos, sm->tam_datos, s)) des->smask = s;
            else { liberar_img(s); libera(s); }
        }
    }
    return true;
}

/* O pixel (ix, iy) de uma imagem: a cor e o alfa. */
static bool img_amostrar(void *aux, int ix, int iy, cor_t *cor, double *alfa)
{
    img_des_t *im = aux;
    cor->r = cor->g = cor->b = 0.0;
    cor->a = 1.0;
    *alfa = 1.0;
    if (im->tem_jpeg) {
        if (ix < 0 || iy < 0 || ix >= im->jpg.larg || iy >= im->jpg.alt) return false;
        uint32_t px = im->jpg.px[(size_t)iy * (size_t)im->jpg.larg + (size_t)ix];
        cor->r = (double)((px >> 16) & 0xFF) / 255.0;
        cor->g = (double)((px >> 8) & 0xFF) / 255.0;
        cor->b = (double)(px & 0xFF) / 255.0;
        return true;
    }
    img_am_t *a = &im->am;
    if (a->mascara) {
        /* Cada fila empieza en un byte nuevo. */
        int byte = iy * a->bytes_linha + ix / 8;
        int bit = ix;
        if (byte >= a->tam) return false;
        int v = (a->dados[byte] >> (7 - (bit % 8))) & 1;
        if (a->inverter) v = !v;
        *alfa = v ? 0.0 : 1.0;
        return true;
    }
    if (a->indexada) {
        int idx = img_valor(a, ix, iy, 0);
        if (idx < 0 || idx >= a->n_cores) return false;
        cor->r = (double)a->paleta[idx * 3] / 255.0;
        cor->g = (double)a->paleta[idx * 3 + 1] / 255.0;
        cor->b = (double)a->paleta[idx * 3 + 2] / 255.0;
        return true;
    }
    double comp[4];
    int maximo = (1 << a->bpc) - 1;
    for (int c = 0; c < 4; c++) comp[c] = 0.0;
    for (int c = 0; c < a->ncomp && c < 4; c++) {
        int v = img_valor(a, ix, iy, c);
        if (a->inverter) v = maximo - v;
        comp[c] = (double)v / (double)maximo;
    }
    int esp = (a->ncomp == 3) ? ESP_RGB : ((a->ncomp == 4) ? ESP_CMYK : ESP_GRAY);
    *cor = cor_componentes(esp, comp, a->ncomp);
    cor->a = 1.0;
    return true;
}

/* Una muestra de la imagen en (u, v) del cuadrado unidad: el color con su
 * alfa (la /SMask, la mascara y la transparencia del estado ya aplicadas). */
static bool img_ponto(ctx_t *ctx, img_des_t *im, int iw, int ih, double u, double v,
                      cor_t cor_base, bool mascara, cor_t *saida)
{
    if (u < 0.0 || u >= 1.0 || v < 0.0 || v >= 1.0) return false;
    int ix = (int)(u * (double)iw);
    int iy = (int)((1.0 - v) * (double)ih);
    if (ix < 0) ix = 0;
    if (ix >= iw) ix = iw - 1;
    if (iy < 0) iy = 0;
    if (iy >= ih) iy = ih - 1;
    cor_t cor;
    double alfa = 1.0;
    if (!img_amostrar(im, ix, iy, &cor, &alfa)) return false;
    if (mascara) cor = cor_base;
    cor.a *= alfa * ctx->est.alfa_preench;
    if (im->smask) {
        /* La /SMask es una imagen en gris (el gris ES el alfa) y puede
         * tener otro tamano que la imagen. */
        img_des_t *sm = im->smask;
        int sw = sm->tem_jpeg ? sm->jpg.larg : sm->am.larg;
        int sh = sm->tem_jpeg ? sm->jpg.alt : sm->am.alt;
        int sx = (int)(u * (double)sw), sy = (int)((1.0 - v) * (double)sh);
        if (sx >= sw) sx = sw - 1;
        if (sy >= sh) sy = sh - 1;
        if (sx < 0) sx = 0;
        if (sy < 0) sy = 0;
        cor_t cm;
        double am = 1.0;
        if (img_amostrar(sm, sx, sy, &cm, &am)) cor.a *= cm.r * am;
    }
    *saida = cor;
    return true;
}

/* Desenha uma imagem com a CTM (o quadrado [0,1]x[0,1]). */
static void desenhar_imagem(ctx_t *ctx, img_des_t *im, cor_t cor_base, bool mascara)
{
    if (ctx->so_texto) return;
    int iw = im->tem_jpeg ? im->jpg.larg : im->am.larg;
    int ih = im->tem_jpeg ? im->jpg.alt : im->am.alt;
    if (iw <= 0 || ih <= 0) return;
    pdf_mat_t inv;
    if (!mat_inversa(&ctx->est.ctm, &inv)) return;

    double ux[4] = {0.0, 1.0, 1.0, 0.0};
    double uy[4] = {0.0, 0.0, 1.0, 1.0};
    double xs[4], ys[4];
    for (int k = 0; k < 4; k++) pdf_mat_aplica(&ctx->est.ctm, ux[k], uy[k], &xs[k], &ys[k]);
    double xmin = xs[0], xmax = xs[0], ymin = ys[0], ymax = ys[0];
    for (int k = 1; k < 4; k++) {
        if (xs[k] < xmin) xmin = xs[k];
        if (xs[k] > xmax) xmax = xs[k];
        if (ys[k] < ymin) ymin = ys[k];
        if (ys[k] > ymax) ymax = ys[k];
    }
    int bx0 = (int)piso_d(xmin);
    int bx1 = (int)piso_d(xmax);
    int by0 = (int)piso_d(ymin);
    int by1 = (int)piso_d(ymax);
    if (bx0 < 0) bx0 = 0;
    if (by0 < 0) by0 = 0;
    if (bx1 > ctx->largo - 1) bx1 = ctx->largo - 1;
    if (by1 > ctx->alto - 1) by1 = ctx->alto - 1;
    if (bx1 < bx0 || by1 < by0) return;

    /* Al reducir (un escaneado de 300 ppp en una pantalla), un solo punto
     * por pixel se salta los trazos finos y el texto sale roto: se toman
     * n x n puntos por pixel (hasta 4 x 4) y se hace la media. */
    double pu = (fabs_d(inv.a) + fabs_d(inv.c)) * (double)iw;
    double pv = (fabs_d(inv.b) + fabs_d(inv.d)) * (double)ih;
    double pmax = pu > pv ? pu : pv;
    int ns = (int)pmax;
    if (ns < 1) ns = 1;
    if (ns > 4) ns = 4;

    for (int y = by0; y <= by1; y++) {
        spans_t sp_rec, sp_tmp;
        spans_init(&sp_rec);
        spans_init(&sp_tmp);
        int r_rec = spans_recorte(ctx, (double)y + 0.5, &sp_rec, &sp_tmp);
        if (r_rec == 0) {
            spans_liberar(&sp_rec);
            spans_liberar(&sp_tmp);
            continue;
        }
        bool tem_rec = (r_rec > 0);
        for (int x = bx0; x <= bx1; x++) {
            cor_t cor;
            if (ns == 1) {
                double u, v;
                pdf_mat_aplica(&inv, (double)x + 0.5, (double)y + 0.5, &u, &v);
                if (!img_ponto(ctx, im, iw, ih, u, v, cor_base, mascara, &cor)) continue;
            } else {
                /* La media con el alfa premultiplicado. */
                double sr = 0, sg = 0, sb = 0, sa = 0;
                for (int j = 0; j < ns; j++) {
                    for (int i = 0; i < ns; i++) {
                        double u, v;
                        cor_t c;
                        pdf_mat_aplica(&inv, (double)x + ((double)i + 0.5) / ns,
                                       (double)y + ((double)j + 0.5) / ns, &u, &v);
                        if (!img_ponto(ctx, im, iw, ih, u, v, cor_base, mascara, &c)) continue;
                        sr += c.r * c.a; sg += c.g * c.a; sb += c.b * c.a; sa += c.a;
                    }
                }
                if (sa <= 0.0) continue;
                cor.r = sr / sa; cor.g = sg / sa; cor.b = sb / sa;
                cor.a = sa / (double)(ns * ns);
            }
            double cob = 1.0;
            if (tem_rec) {
                cob = cobertura_recorte(&sp_rec, x);
                if (cob <= 0.0) continue;
            }
            compor(ctx, x, y, &cor, cob);
        }
        spans_liberar(&sp_rec);
        spans_liberar(&sp_tmp);
    }
}

/* Um XObject de imagem. */
static void desenhar_xobject_imagem(ctx_t *ctx, pdf_obj_t *img)
{
    img_des_t *des = aloca_zerado(sizeof *des);
    if (!des) return;
    if (preparar_imagem(ctx, img, img->datos, img->tam_datos, des))
        desenhar_imagem(ctx, des, ctx->est.preench, des->am.mascara);
    liberar_img(des);
    libera(des);
}

/* ---------------------------------------------------------- sombreados */

/* A cor media de um sombreado (os degradados sao aproximados). */
static bool cor_sombreado(ctx_t *ctx, pdf_obj_t *som, cor_t *cor)
{
    pdf_obj_t *cs = dic(ctx->doc, som, "ColorSpace");
    int nc = ncomp_espaco(ctx->doc, cs);
    if (nc > 4) nc = 4;
    double comp[4];
    for (int i = 0; i < 4; i++) comp[i] = 0.0;

    pdf_obj_t *fn = dic(ctx->doc, som, "Function");
    if (!fn) {
        pdf_obj_t *fns = dic(ctx->doc, som, "Functions");
        if (fns && fns->tipo == PDF_OBJ_ARREGLO && fns->n_itens > 0)
            fn = resolver_ref(ctx->doc, fns->itens[fns->n_itens / 2]);
    }
    if (!fn) return false;
    if (fn->tipo == PDF_OBJ_ARREGLO) {
        if (fn->n_itens > 0) fn = resolver_ref(ctx->doc, fn->itens[fn->n_itens / 2]);
        else return false;
    }
    if (fn->tipo != PDF_OBJ_STREAM) return false;
    pdf_obj_t *ft = dic(ctx->doc, fn, "FunctionType");
    int tipo = (ft && ft->tipo == PDF_OBJ_NUMERO) ? (int)ft->numero : 0;

    if (tipo == 2) {
        pdf_obj_t *c0 = dic(ctx->doc, fn, "C0");
        pdf_obj_t *c1 = dic(ctx->doc, fn, "C1");
        for (int i = 0; i < nc; i++) {
            double a = 0.0, b = 0.0;
            if (c0 && c0->tipo == PDF_OBJ_ARREGLO && i < c0->n_itens && c0->itens[i])
                a = c0->itens[i]->numero;
            if (c1 && c1->tipo == PDF_OBJ_ARREGLO && i < c1->n_itens && c1->itens[i])
                b = c1->itens[i]->numero;
            comp[i] = (a + b) * 0.5;
        }
    } else if (tipo == 0) {
        pdf_obj_t *sz = dic(ctx->doc, fn, "Size");
        int n = (sz && sz->tipo == PDF_OBJ_NUMERO) ? (int)sz->numero : 0;
        pdf_obj_t *sam = dic(ctx->doc, fn, "BitsPerSample");
        int bps = (sam && sam->tipo == PDF_OBJ_NUMERO) ? (int)sam->numero : 8;
        if (n <= 0) return false;
        int meio = n / 2;
        const uint8_t *d = fn->datos;
        int tam = fn->tam_datos;
        for (int i = 0; i < nc; i++) {
            int off = ((meio * nc + i) * bps) / 8;
            double v = 0.0;
            if (d && off >= 0 && off < tam) v = (double)d[off] / 255.0;
            comp[i] = v;
        }
    } else {
        return false;
    }
    int esp = (nc == 3) ? ESP_RGB : ((nc == 4) ? ESP_CMYK : ESP_GRAY);
    *cor = cor_componentes(esp, comp, nc);
    return true;
}

/* O operador sh: pinta a area do recorte com a cor do sombreado. */
static void desenhar_sombreado(ctx_t *ctx, pdf_obj_t *pat)
{
    cor_t cor;
    cor.r = cor.g = cor.b = 0.5;
    cor.a = 1.0;
    if (!pat || pat->tipo != PDF_OBJ_STREAM) return;
    if (!cor_sombreado(ctx, pat, &cor)) return;
    caminho_t r;
    caminho_init(&r);
    int idx = caminho_novo(&r);
    pdf_caminho_t *sub = caminho_sub(&r, idx);
    if (sub) {
        pdf_caminho_linha(sub, 0.0, 0.0, (double)ctx->largo, 0.0);
        pdf_caminho_linha(sub, (double)ctx->largo, 0.0, (double)ctx->largo, (double)ctx->alto);
        pdf_caminho_linha(sub, (double)ctx->largo, (double)ctx->alto, 0.0, (double)ctx->alto);
        pdf_caminho_linha(sub, 0.0, (double)ctx->alto, 0.0, 0.0);
        sub->fechado = true;
        pintar_caminho(ctx, &r, false, cor, ctx->est.alfa_preench);
    }
    caminho_liberar(&r);
}

/* ------------------------------------------------------------ formas */

static bool executar_conteudo(ctx_t *ctx, const uint8_t *dados, int tam,
                              pdf_obj_t *recursos, int prof);

/* Executa um XObject de forma. */
static void executar_form(ctx_t *ctx, pdf_obj_t *form, pdf_obj_t *recursos_pagina)
{
    if (!form || form->tipo != PDF_OBJ_STREAM) return;
    if (!form->datos || form->tam_datos <= 0) return;

    estado_t novo;
    memset(&novo, 0, sizeof novo);
    if (!estado_copiar(&novo, &ctx->est)) return;

    /* a matriz da forma */
    pdf_obj_t *mx = dic(ctx->doc, form, "Matrix");
    if (mx && mx->tipo == PDF_OBJ_ARREGLO && mx->n_itens >= 6) {
        pdf_mat_t m;
        double v[6];
        for (int i = 0; i < 6; i++) {
            pdf_obj_t *o = resolver_ref(ctx->doc, mx->itens[i]);
            v[i] = (o && o->tipo == PDF_OBJ_NUMERO) ? o->numero : (i == 0 || i == 3 ? 1.0 : 0.0);
        }
        m.a = v[0]; m.b = v[1]; m.c = v[2]; m.d = v[3]; m.e = v[4]; m.f = v[5];
        novo.ctm = pdf_mat_multiplica(&m, &novo.ctm);
    }

    /* o BBox: um recorte mais */
    pdf_obj_t *bb = dic(ctx->doc, form, "BBox");
    if (bb && bb->tipo == PDF_OBJ_ARREGLO && bb->n_itens >= 4) {
        double v[4];
        for (int i = 0; i < 4; i++) {
            pdf_obj_t *o = resolver_ref(ctx->doc, bb->itens[i]);
            v[i] = (o && o->tipo == PDF_OBJ_NUMERO) ? o->numero : 0.0;
        }
        double x0 = v[0], y0 = v[1], x1 = v[2], y1 = v[3];
        if (x1 < x0) { double t = x0; x0 = x1; x1 = t; }
        if (y1 < y0) { double t = y0; y0 = y1; y1 = t; }
        caminho_t cam;
        caminho_init(&cam);
        int idx = caminho_novo(&cam);
        pdf_caminho_t *sub = caminho_sub(&cam, idx);
        if (sub) {
            double xs[4], ys[4];
            double ux[4] = {x0, x1, x1, x0};
            double uy[4] = {y0, y0, y1, y1};
            for (int k = 0; k < 4; k++) pdf_mat_aplica(&novo.ctm, ux[k], uy[k], &xs[k], &ys[k]);
            pdf_caminho_linha(sub, xs[0], ys[0], xs[1], ys[1]);
            pdf_caminho_linha(sub, xs[1], ys[1], xs[2], ys[2]);
            pdf_caminho_linha(sub, xs[2], ys[2], xs[3], ys[3]);
            pdf_caminho_linha(sub, xs[3], ys[3], xs[0], ys[0]);
            sub->fechado = true;
            /* o recorte da forma: o recorte atual mais o BBox */
            clip_t *copia = clip_copiar(novo.recorte);
            if (copia) {
                clip_liberar(novo.recorte);
                novo.recorte = copia;
                aplanar_caminho(ctx, &cam, &ctx->flat);
                clip_adicionar(novo.recorte, &ctx->flat, false);
            }
        }
        caminho_liberar(&cam);
    }

    /* os recursos da forma (ou os da pagina) */
    pdf_obj_t *rec = dic(ctx->doc, form, "Resources");
    if (!rec) rec = recursos_pagina;

    estado_t *ant = &ctx->est;
    ctx->est = novo;
    executar_conteudo(ctx, form->datos, form->tam_datos, rec, 1);
    estado_liberar(&ctx->est);
    ctx->est = *ant;
}

/* ------------------------------------------------------- interpretador */

/* Um operando numerico (0 = o ultimo). */
/* Um numero da pilha: k = 0 e o ultimo, k = -1 o anterior, etc. */
static double op_num(pilha_t *p, int k)
{
    int i = p->n - 1 + k;
    if (i < 0 || i >= p->n) return 0.0;
    return val_numero(p->v[i]);
}

/* O espaco de cor de um nome. */
static int espaco_por_nome(ctx_t *ctx, pdf_obj_t *recursos, const char *nome, bool *e_padrao)
{
    *e_padrao = false;
    if (!nome) return ESP_GRAY;
    if (iguais(nome, "Pattern")) { *e_padrao = true; return ESP_PADRAO; }
    if (iguais(nome, "DeviceGray") || iguais(nome, "G")) return ESP_GRAY;
    if (iguais(nome, "DeviceRGB") || iguais(nome, "RGB")) return ESP_RGB;
    if (iguais(nome, "DeviceCMYK") || iguais(nome, "CMYK")) return ESP_CMYK;
    pdf_obj_t *cs = recurso(ctx->doc, recursos, "ColorSpace", nome);
    if (!cs) return ESP_GRAY;
    if (cs->tipo == PDF_OBJ_NOME) {
        bool pd = false;
        return espaco_por_nome(ctx, NULL, cs->nome, &pd);
    }
    if (cs->tipo == PDF_OBJ_ARREGLO && cs->n_itens > 0) {
        pdf_obj_t *n = resolver_ref(ctx->doc, cs->itens[0]);
        if (n && n->tipo == PDF_OBJ_NOME) {
            if (iguais(n->nome, "Pattern")) { *e_padrao = true; return ESP_PADRAO; }
            if (iguais(n->nome, "Separation")) return ESP_GRAY;
            if (iguais(n->nome, "DeviceN")) {
                int nc = ncomp_espaco(ctx->doc, cs);
                return (nc == 3) ? ESP_RGB : ESP_GRAY;
            }
            if (iguais(n->nome, "ICCBased")) {
                int nc = ncomp_espaco(ctx->doc, cs);
                if (nc == 3) return ESP_RGB;
                if (nc == 4) return ESP_CMYK;
                return ESP_GRAY;
            }
            if (iguais(n->nome, "Lab") || iguais(n->nome, "CalRGB")) return ESP_RGB;
            if (iguais(n->nome, "DeviceCMYK")) return ESP_CMYK;
            if (iguais(n->nome, "DeviceRGB")) return ESP_RGB;
        }
    }
    return ESP_GRAY;
}

/* A cor de um padrao (so os sombreados se podem aproximar). */
static bool cor_do_padrao(ctx_t *ctx, pdf_obj_t *recursos, const char *nome, cor_t *cor)
{
    pdf_obj_t *pat = recurso(ctx->doc, recursos, "Pattern", nome);
    if (!pat) return false;
    if (pat->tipo == PDF_OBJ_STREAM) {
        if (dic(ctx->doc, pat, "ShadingType")) return cor_sombreado(ctx, pat, cor);
        return false;
    }
    if (pat->tipo == PDF_OBJ_DICCIONARIO) {
        pdf_obj_t *sh = dic(ctx->doc, pat, "Shading");
        if (sh && sh->tipo == PDF_OBJ_STREAM) return cor_sombreado(ctx, sh, cor);
    }
    return false;
}

/* Aplica un /ExtGState. */
static void aplicar_gs(ctx_t *ctx, pdf_obj_t *gs)
{
    pdf_obj_t *o;
    o = dic(ctx->doc, gs, "ca");
    if (o && o->tipo == PDF_OBJ_NUMERO) ctx->est.alfa_preench = limitar(o->numero, 0.0, 1.0);
    o = dic(ctx->doc, gs, "CA");
    if (o && o->tipo == PDF_OBJ_NUMERO) ctx->est.alfa_trazo = limitar(o->numero, 0.0, 1.0);
    o = dic(ctx->doc, gs, "LW");
    if (o && o->tipo == PDF_OBJ_NUMERO) ctx->est.largura = o->numero;
    o = dic(ctx->doc, gs, "LC");
    if (o && o->tipo == PDF_OBJ_NUMERO) ctx->est.tampao = (int)o->numero;
    o = dic(ctx->doc, gs, "LJ");
    if (o && o->tipo == PDF_OBJ_NUMERO) ctx->est.junta = (int)o->numero;
    o = dic(ctx->doc, gs, "ML");
    if (o && o->tipo == PDF_OBJ_NUMERO) ctx->est.limite_miter = o->numero;
    o = dic(ctx->doc, gs, "D");
    if (o && o->tipo == PDF_OBJ_ARREGLO && o->n_itens >= 2) {
        pdf_obj_t *arr = resolver_ref(ctx->doc, o->itens[0]);
        if (ctx->est.guiones) { libera(ctx->est.guiones); ctx->est.guiones = NULL; ctx->est.n_guiones = 0; }
        if (arr && arr->tipo == PDF_OBJ_ARREGLO && arr->n_itens > 0) {
            double *g = aloca((size_t)arr->n_itens * sizeof(double));
            if (g) {
                for (int i = 0; i < arr->n_itens; i++) g[i] = obj_numero(arr->itens[i]);
                ctx->est.guiones = g;
                ctx->est.n_guiones = arr->n_itens;
            }
        }
        pdf_obj_t *fase = resolver_ref(ctx->doc, o->itens[1]);
        if (fase && fase->tipo == PDF_OBJ_NUMERO) ctx->est.fase_guiones = fase->numero;
    }
}

/* Um objeto del PDF creado aqui (para las imagenes en linea). */
static pdf_obj_t *obj_local(pdf_obj_tipo_t t)
{
    pdf_obj_t *o = aloca_zerado(sizeof *o);
    if (o) o->tipo = t;
    return o;
}

static void obj_local_liberar(pdf_obj_t *o)
{
    if (!o) return;
    if (o->nome) libera(o->nome);
    if (o->cadena) libera(o->cadena);
    for (int i = 0; i < o->n_itens; i++) obj_local_liberar(o->itens[i]);
    if (o->itens) libera(o->itens);
    for (int i = 0; i < o->n_entradas; i++) {
        if (o->entradas[i].chave) libera(o->entradas[i].chave);
        obj_local_liberar(o->entradas[i].valor);
    }
    if (o->entradas) libera(o->entradas);
    libera(o);
}

/* Convierte un valor del lexico en un objeto del PDF. */
static pdf_obj_t *val_para_obj(const val_t *v)
{
    if (!v) return obj_local(PDF_OBJ_NULO);
    switch (v->tipo) {
    case VAL_NUMERO: {
        pdf_obj_t *o = obj_local(PDF_OBJ_NUMERO);
        if (o) o->numero = v->numero;
        return o;
    }
    case VAL_BOOL: {
        pdf_obj_t *o = obj_local(PDF_OBJ_BOOL);
        if (o) { o->booleano = (v->numero != 0.0); o->numero = v->numero; }
        return o;
    }
    case VAL_NOME: {
        pdf_obj_t *o = obj_local(PDF_OBJ_NOME);
        if (o) o->nome = copiar_txt(v->texto, v->tam_texto);
        return o;
    }
    case VAL_CADEIA: {
        pdf_obj_t *o = obj_local(PDF_OBJ_CADENA);
        if (o) { o->cadena = copiar_txt(v->texto, v->tam_texto); o->tam_cadena = v->tam_texto; }
        return o;
    }
    case VAL_ARR: {
        pdf_obj_t *o = obj_local(PDF_OBJ_ARREGLO);
        if (o) {
            o->itens = aloca((size_t)(v->n_itens > 0 ? v->n_itens : 1) * sizeof(pdf_obj_t *));
            if (o->itens) {
                for (int i = 0; i < v->n_itens; i++) o->itens[i] = val_para_obj(v->itens[i]);
                o->n_itens = v->n_itens;
            }
        }
        return o;
    }
    case VAL_DICT: {
        pdf_obj_t *o = obj_local(PDF_OBJ_DICCIONARIO);
        if (o) {
            o->entradas = aloca((size_t)(v->n_entradas > 0 ? v->n_entradas : 1) * sizeof(pdf_entrada_t));
            if (o->entradas) {
                for (int i = 0; i < v->n_entradas; i++) {
                    o->entradas[i].chave = copiar_str(v->entradas[i].chave);
                    o->entradas[i].valor = val_para_obj(v->entradas[i].valor);
                }
                o->n_entradas = v->n_entradas;
            }
        }
        return o;
    }
    default:
        return obj_local(PDF_OBJ_NULO);
    }
}

/* O inflate da libsagui e de "deflate" cru: o FlateDecode do PDF trae o
 * cabecalho do zlib (2 bytes), que se tira antes de descomprimir. */
static bool inflar_zlib(const uint8_t *ent, int n, uint8_t **saida, int *tam)
{
    return pdf_inflar(ent, n, saida, tam);
}

/* Uma imagem en linea (BI ... ID ... EI). */
static void executar_imagem_inline(ctx_t *ctx, lex_t *l)
{
    pdf_obj_t *dict = obj_local(PDF_OBJ_DICCIONARIO);
    if (!dict) return;
    while (l->p < l->n) {
        lex_pular(l);
        if (l->p >= l->n) break;
        if (l->d[l->p] == '/') {
            val_t *k = NULL;
            lex_tok(l, &k);
            if (!k) break;
            char chave[64];
            int nn = k->tam_texto;
            if (nn > 63) nn = 63;
            memcpy(chave, k->texto, (size_t)nn);
            chave[nn] = 0;
            val_liberar(k);
            val_t *v = NULL;
            lex_tok(l, &v);
            if (!v) break;
            pdf_obj_t *vo = val_para_obj(v);
            val_liberar(v);
            if (!vo) break;
            char *c = copiar_str(chave);
            void *novo = realoca(dict->entradas, (size_t)(dict->n_entradas + 1) * sizeof(pdf_entrada_t));
            if (!novo) { libera(c); obj_local_liberar(vo); break; }
            dict->entradas = novo;
            dict->entradas[dict->n_entradas].chave = c;
            dict->entradas[dict->n_entradas].valor = vo;
            dict->n_entradas++;
        } else {
            val_t *k = NULL;
            tok_tipo_t t = lex_tok(l, &k);
            val_liberar(k);
            if (t == TOK_FIM) break;
            if (iguais(l->pal, "ID")) break;
        }
    }
    /* un espacio despues de ID */
    if (l->p < l->n && lex_espaco(l->d[l->p])) l->p++;
    /* os bytes ate EI */
    int ini = l->p;
    int fim = ini;
    while (fim < l->n) {
        if (l->d[fim] == 'E' && fim + 1 < l->n && l->d[fim + 1] == 'I' &&
            (fim + 2 >= l->n || lex_espaco(l->d[fim + 2]) || lex_delim(l->d[fim + 2])) &&
            (fim == 0 || lex_espaco(l->d[fim - 1]))) {
            break;
        }
        fim++;
    }
    int tam = fim - ini;
    l->p = (fim < l->n) ? fim + 2 : fim;

    /* o filtro (so o Flate, que se pode descomprimir aqui) */
    const uint8_t *dados = l->d + ini;
    int tamd = tam;
    uint8_t *infl = NULL;
    pdf_obj_t *f = pdf_dict_buscar(dict, "Filter");
    const char *filtro = NULL;
    if (f && f->tipo == PDF_OBJ_NOME) filtro = f->nome;
    else if (f && f->tipo == PDF_OBJ_ARREGLO && f->n_itens > 0) {
        pdf_obj_t *f0 = resolver_ref(ctx->doc, f->itens[0]);
        if (f0 && f0->tipo == PDF_OBJ_NOME) filtro = f0->nome;
    }
    if (filtro && (iguais(filtro, "FlateDecode") || iguais(filtro, "Fl"))) {
        if (inflar_zlib(dados, tamd, &infl, &tamd)) dados = infl;
    }

    img_des_t *des = aloca_zerado(sizeof *des);
    if (des) {
        if (preparar_imagem(ctx, dict, dados, tamd, des))
            desenhar_imagem(ctx, des, ctx->est.preench, des->am.mascara);
        liberar_img(des);
        libera(des);
    }
    if (infl) libera(infl);
    obj_local_liberar(dict);
}

/* Um operador do fluxo de conteudo. */
static void executar_operador(ctx_t *ctx, const char *op, pilha_t *p, lex_t *l, pdf_obj_t *recursos)
{
    estado_t *e = &ctx->est;

    /* ---- estado grafico ---- */
    if (iguais(op, "q")) {
        if (ctx->n_pilha >= ctx->cap_pilha) {
            int nova = ctx->cap_pilha ? ctx->cap_pilha * 2 : 16;
            estado_t *nv = realoca(ctx->pilha, (size_t)nova * sizeof(estado_t));
            if (!nv) return;
            ctx->pilha = nv;
            ctx->cap_pilha = nova;
        }
        if (!estado_copiar(&ctx->pilha[ctx->n_pilha], e)) return;
        ctx->n_pilha++;
        return;
    }
    if (iguais(op, "Q")) {
        if (ctx->n_pilha <= 0) return;
        ctx->n_pilha--;
        estado_t *ant = &ctx->pilha[ctx->n_pilha];
        estado_liberar(e);
        *e = *ant;
        ant->guiones = NULL;
        ant->n_guiones = 0;
        ant->recorte = NULL;
        memset(ant, 0, sizeof *ant);
        return;
    }
    if (iguais(op, "cm")) {
        if (p->n < 6) return;
        pdf_mat_t m;
        m.a = op_num(p, -5); m.b = op_num(p, -4);
        m.c = op_num(p, -3); m.d = op_num(p, -2);
        m.e = op_num(p, -1); m.f = op_num(p, 0);
        e->ctm = pdf_mat_multiplica(&m, &e->ctm);
        return;
    }
    if (iguais(op, "w")) { e->largura = op_num(p, 0); return; }
    if (iguais(op, "J")) { e->tampao = (int)op_num(p, 0); return; }
    if (iguais(op, "j")) { e->junta = (int)op_num(p, 0); return; }
    if (iguais(op, "M")) { e->limite_miter = op_num(p, 0); return; }
    if (iguais(op, "i") || iguais(op, "ri")) return;
    if (iguais(op, "d")) {
        if (p->n < 2) return;
        val_t *arr = p->v[p->n - 2];
        if (e->guiones) { libera(e->guiones); e->guiones = NULL; e->n_guiones = 0; }
        if (arr && arr->tipo == VAL_ARR && arr->n_itens > 0) {
            double *g = aloca((size_t)arr->n_itens * sizeof(double));
            if (g) {
                for (int i = 0; i < arr->n_itens; i++) g[i] = val_numero(arr->itens[i]);
                e->guiones = g;
                e->n_guiones = arr->n_itens;
            }
        }
        e->fase_guiones = op_num(p, 0);
        return;
    }
    if (iguais(op, "gs")) {
        if (p->n < 1) return;
        val_t *nm = p->v[p->n - 1];
        if (!nm || nm->tipo != VAL_NOME) return;
        pdf_obj_t *gs = recurso(ctx->doc, recursos, "ExtGState", nm->texto);
        if (gs) aplicar_gs(ctx, gs);
        return;
    }

    /* ---- cor ---- */
    if (iguais(op, "cs") || iguais(op, "CS")) {
        if (p->n < 1) return;
        val_t *nm = p->v[p->n - 1];
        if (!nm || nm->tipo != VAL_NOME) return;
        bool e_padrao = false;
        int esp = espaco_por_nome(ctx, recursos, nm->texto, &e_padrao);
        if (op[0] == 'c') {
            ctx->espaco_preench = esp;
            copiar_n(ctx->padrao_preench, nm->texto, 64);
        } else {
            ctx->espaco_trazo = esp;
            copiar_n(ctx->padrao_trazo, nm->texto, 64);
        }
        return;
    }
    if (iguais(op, "sc") || iguais(op, "scn") || iguais(op, "SC") || iguais(op, "SCN")) {
        bool trazo = (op[0] == 'S');
        int esp = trazo ? ctx->espaco_trazo : ctx->espaco_preench;
        double comp[8];
        int nc = 0;
        const char *nome_padrao = NULL;
        for (int i = 0; i < p->n; i++) {
            val_t *v = p->v[i];
            if (!v) continue;
            if (v->tipo == VAL_NUMERO || v->tipo == VAL_BOOL) {
                if (nc < 8) comp[nc++] = val_numero(v);
            } else if (v->tipo == VAL_NOME) {
                nome_padrao = v->texto;
            }
        }
        cor_t cor;
        if (esp == ESP_PADRAO && nome_padrao) {
            if (!cor_do_padrao(ctx, recursos, nome_padrao, &cor)) return;
        } else {
            cor = cor_componentes(esp, comp, nc);
        }
        if (trazo) e->trazo = cor;
        else e->preench = cor;
        return;
    }
    if (iguais(op, "g") || iguais(op, "rg") || iguais(op, "k")) {
        double comp[4];
        int nc = (op[0] == 'g') ? 1 : ((op[0] == 'r') ? 3 : 4);
        for (int i = 0; i < nc; i++) comp[i] = op_num(p, i - nc + 1);
        cor_t cor = cor_componentes((op[0] == 'g') ? ESP_GRAY : ((op[0] == 'r') ? ESP_RGB : ESP_CMYK),
                                   comp, nc);
        e->preench = cor;
        return;
    }
    if (iguais(op, "G") || iguais(op, "RG") || iguais(op, "K")) {
        double comp[4];
        int nc = (op[0] == 'G') ? 1 : ((op[0] == 'R') ? 3 : 4);
        for (int i = 0; i < nc; i++) comp[i] = op_num(p, i - nc + 1);
        cor_t cor = cor_componentes((op[0] == 'G') ? ESP_GRAY : ((op[0] == 'R') ? ESP_RGB : ESP_CMYK),
                                   comp, nc);
        e->trazo = cor;
        return;
    }

    /* ---- caminhos ---- */
    if (iguais(op, "m")) { cam_m(ctx, op_num(p, -1), op_num(p, 0)); return; }
    if (iguais(op, "l")) { cam_l(ctx, op_num(p, -1), op_num(p, 0)); return; }
    if (iguais(op, "c")) {
        cam_c(ctx, op_num(p, -5), op_num(p, -4), op_num(p, -3), op_num(p, -2), op_num(p, -1), op_num(p, 0));
        return;
    }
    if (iguais(op, "v")) {
        cam_c(ctx, ctx->cx, ctx->cy, op_num(p, -3), op_num(p, -2), op_num(p, -1), op_num(p, 0));
        return;
    }
    if (iguais(op, "y")) {
        cam_c(ctx, op_num(p, -5), op_num(p, -4), op_num(p, -3), op_num(p, -2), op_num(p, -1), op_num(p, 0));
        return;
    }
    if (iguais(op, "h")) { cam_h(ctx); return; }
    if (iguais(op, "re")) {
        double x = op_num(p, -3), y = op_num(p, -2);
        double w = op_num(p, -1), h = op_num(p, 0);
        cam_re(ctx, x, y, w, h);
        return;
    }

    /* ---- pintura ---- */
    if (iguais(op, "S")) { pintar_op(ctx, 2, false); return; }
    if (iguais(op, "s")) { cam_h(ctx); pintar_op(ctx, 2, false); return; }
    if (iguais(op, "f") || iguais(op, "F")) { pintar_op(ctx, 1, false); return; }
    if (iguais(op, "f*")) { pintar_op(ctx, 1, true); return; }
    if (iguais(op, "B")) { pintar_op(ctx, 3, false); return; }
    if (iguais(op, "B*")) { pintar_op(ctx, 3, true); return; }
    if (iguais(op, "b")) { cam_h(ctx); pintar_op(ctx, 3, false); return; }
    if (iguais(op, "b*")) { cam_h(ctx); pintar_op(ctx, 3, true); return; }
    if (iguais(op, "n")) { pintar_op(ctx, 0, false); return; }
    if (iguais(op, "W")) { ctx->recorte_pendente = true; ctx->recorte_par = false; return; }
    if (iguais(op, "W*")) { ctx->recorte_pendente = true; ctx->recorte_par = true; return; }

    /* ---- texto ---- */
    if (iguais(op, "BT")) {
        e->tm = pdf_mat_identidade();
        e->tlm = e->tm;
        return;
    }
    if (iguais(op, "ET")) return;
    if (iguais(op, "Tf")) {
        if (p->n < 2) return;
        val_t *nm = p->v[p->n - 2];
        if (nm && nm->tipo == VAL_NOME) copiar_n(e->recurso_fonte, nm->texto, 64);
        e->tam_fonte = op_num(p, 0);
        return;
    }
    if (iguais(op, "Td") || iguais(op, "TD")) {
        if (p->n < 2) return;
        double tx = op_num(p, -1), ty = op_num(p, 0);
        if (op[0] == 'T' && op[1] == 'D') e->interlinha = -ty;
        pdf_mat_t t = pdf_mat_translada(tx, ty);
        e->tlm = pdf_mat_multiplica(&t, &e->tlm);
        e->tm = e->tlm;
        return;
    }
    if (iguais(op, "Tm")) {
        if (p->n < 6) return;
        pdf_mat_t m;
        m.a = op_num(p, -5); m.b = op_num(p, -4);
        m.c = op_num(p, -3); m.d = op_num(p, -2);
        m.e = op_num(p, -1); m.f = op_num(p, 0);
        e->tm = m;
        e->tlm = m;
        return;
    }
    if (iguais(op, "T*")) { texto_avancar_linha(ctx); return; }
    if (iguais(op, "Tc")) { e->esp_car = op_num(p, 0); return; }
    if (iguais(op, "Tw")) { e->esp_pal = op_num(p, 0); return; }
    if (iguais(op, "Tz")) { e->esc_hor = op_num(p, 0); return; }
    if (iguais(op, "TL")) { e->interlinha = op_num(p, 0); return; }
    if (iguais(op, "Ts")) { e->subida = op_num(p, 0); return; }
    if (iguais(op, "Tr")) { e->modo_render = (int)op_num(p, 0); return; }
    if (iguais(op, "Tj")) {
        if (p->n < 1) return;
        val_t *s = p->v[p->n - 1];
        if (s && s->tipo == VAL_CADEIA)
            mostrar_texto(ctx, (const uint8_t *)s->texto, s->tam_texto);
        return;
    }
    if (iguais(op, "TJ")) {
        if (p->n < 1) return;
        val_t *arr = p->v[p->n - 1];
        if (!arr || arr->tipo != VAL_ARR) return;
        for (int i = 0; i < arr->n_itens; i++) {
            val_t *it = arr->itens[i];
            if (!it) continue;
            if (it->tipo == VAL_CADEIA) {
                mostrar_texto(ctx, (const uint8_t *)it->texto, it->tam_texto);
            } else if (it->tipo == VAL_NUMERO) {
                double tx = -(it->numero / 1000.0) * e->tam_fonte * (e->esc_hor / 100.0);
                pdf_mat_t t = pdf_mat_translada(tx, 0.0);
                e->tm = pdf_mat_multiplica(&t, &e->tm);
            }
        }
        return;
    }
    if (iguais(op, "'")) {
        if (p->n < 1) return;
        val_t *s = p->v[p->n - 1];
        texto_avancar_linha(ctx);
        if (s && s->tipo == VAL_CADEIA)
            mostrar_texto(ctx, (const uint8_t *)s->texto, s->tam_texto);
        return;
    }
    if (iguais(op, "\"")) {
        if (p->n < 3) return;
        e->esp_pal = op_num(p, -1);
        e->esp_car = op_num(p, 0);
        val_t *s = p->v[p->n - 1];
        texto_avancar_linha(ctx);
        if (s && s->tipo == VAL_CADEIA)
            mostrar_texto(ctx, (const uint8_t *)s->texto, s->tam_texto);
        return;
    }

    /* ---- XObjects, imagenes e sombreados ---- */
    if (iguais(op, "Do")) {
        if (p->n < 1) return;
        val_t *nm = p->v[p->n - 1];
        if (!nm || nm->tipo != VAL_NOME) return;
        pdf_obj_t *xo = recurso(ctx->doc, recursos, "XObject", nm->texto);
        if (!xo) return;
        pdf_obj_t *st = dic(ctx->doc, xo, "Subtype");
        if (!st || st->tipo != PDF_OBJ_NOME) return;
        if (iguais(st->nome, "Image")) desenhar_xobject_imagem(ctx, xo);
        else if (iguais(st->nome, "Form")) executar_form(ctx, xo, recursos);
        return;
    }
    if (iguais(op, "sh")) {
        if (p->n < 1) return;
        val_t *nm = p->v[p->n - 1];
        if (!nm || nm->tipo != VAL_NOME) return;
        pdf_obj_t *pat = recurso(ctx->doc, recursos, "Pattern", nm->texto);
        desenhar_sombreado(ctx, pat);
        return;
    }
    if (iguais(op, "BI")) { executar_imagem_inline(ctx, l); return; }

    /* ---- o resto nao se dibuja ---- */
    if (iguais(op, "d0") || iguais(op, "d1")) return;
    if (iguais(op, "BMC") || iguais(op, "BDC") || iguais(op, "EMC")) return;
    if (iguais(op, "MP") || iguais(op, "DP")) return;
}

/* Executa um fluxo de conteudo. */
static bool executar_conteudo(ctx_t *ctx, const uint8_t *dados, int tam,
                              pdf_obj_t *recursos, int prof)
{
    if (prof > 8) return false;
    if (!dados || tam <= 0) return true;
    lex_t l;
    l.d = dados;
    l.n = tam;
    l.p = 0;
    l.pal[0] = 0;
    pilha_t pilha;
    pilha.v = NULL;
    pilha.n = 0;
    pilha.cap = 0;
    pdf_obj_t *rec_ant = ctx->recursos;
    ctx->recursos = recursos;
    while (true) {
        val_t *v = NULL;
        tok_tipo_t t = lex_tok(&l, &v);
        if (t == TOK_FIM) { val_liberar(v); break; }
        if (t == TOK_NUMERO || t == TOK_NOME || t == TOK_CADEIA ||
            t == TOK_ARR_ABRE || t == TOK_DIC_ABRE) {
            pilha_push(&pilha, v);
            continue;
        }
        if (t == TOK_ARR_FECHA || t == TOK_DIC_FECHA) {
            val_liberar(v);
            continue;
        }
        /* uma palavra: um operador (ou true/false/null) */
        if (iguais(l.pal, "true") || iguais(l.pal, "false")) {
            val_t *b = val_novo(VAL_BOOL);
            if (b) {
                b->numero = iguais(l.pal, "true") ? 1.0 : 0.0;
                pilha_push(&pilha, b);
            }
            continue;
        }
        if (iguais(l.pal, "null")) {
            pilha_push(&pilha, val_novo(VAL_NULO));
            continue;
        }
        executar_operador(ctx, l.pal, &pilha, &l, ctx->recursos);
        pilha_limpar(&pilha);
    }
    pilha_limpar(&pilha);
    if (pilha.v) libera(pilha.v);
    ctx->recursos = rec_ant;
    return true;
}

/* ------------------------------------------------------- a pagina */

/* Procura a pagina na arvore, para herdar o MediaBox e o Rotate. */
static bool herdar_pagina(pdf_doc_t *d, pdf_obj_t *no, pdf_obj_t *alvo,
                           double *box, bool *tem_box, int *rot, int prof)
{
    if (!no || prof > 32) return false;
    no = resolver_ref(d, no);
    if (!no) return false;
    if (no == alvo) return true;
    pdf_obj_t *mb = dic(d, no, "MediaBox");
    if (mb && mb->tipo == PDF_OBJ_ARREGLO && mb->n_itens >= 4) {
        for (int i = 0; i < 4; i++) {
            pdf_obj_t *o = resolver_ref(d, mb->itens[i]);
            if (o && o->tipo == PDF_OBJ_NUMERO) box[i] = o->numero;
        }
        *tem_box = true;
    }
    pdf_obj_t *ro = dic(d, no, "Rotate");
    if (ro && ro->tipo == PDF_OBJ_NUMERO) *rot = ((int)ro->numero % 360 + 360) % 360;
    pdf_obj_t *kids = dic(d, no, "Kids");
    if (kids && kids->tipo == PDF_OBJ_ARREGLO) {
        for (int i = 0; i < kids->n_itens; i++)
            if (herdar_pagina(d, kids->itens[i], alvo, box, tem_box, rot, prof + 1))
                return true;
    }
    return false;
}

/* A transformacao base: do espaco do PDF para o lienzo. */
static pdf_mat_t matriz_base(pdf_doc_t *d, pdf_obj_t *pag, int pagina,
                              int largo, int alto, int *rot_saida)
{
    double box[4];
    bool tem_box = false;
    int rot = 0;
    pdf_obj_t *cat = pdf_doc_catalogo(d);
    pdf_obj_t *arvore = dic(d, cat, "Pages");
    if (arvore) herdar_pagina(d, arvore, pag, box, &tem_box, &rot, 0);
    if (!tem_box) {
        pdf_obj_t *mb = dic(d, pag, "MediaBox");
        if (mb && mb->tipo == PDF_OBJ_ARREGLO && mb->n_itens >= 4) {
            for (int i = 0; i < 4; i++) {
                pdf_obj_t *o = resolver_ref(d, mb->itens[i]);
                box[i] = (o && o->tipo == PDF_OBJ_NUMERO) ? o->numero : 0.0;
            }
            tem_box = true;
        }
    }
    pdf_obj_t *ro = dic(d, pag, "Rotate");
    if (ro && ro->tipo == PDF_OBJ_NUMERO) rot = ((int)ro->numero % 360 + 360) % 360;
    if (!tem_box) {
        int lw = pdf_doc_pagina_largura(d, pagina);
        int al = pdf_doc_pagina_altura(d, pagina);
        box[0] = 0; box[1] = 0; box[2] = lw; box[3] = al;
    }
    double x0 = box[0], y0 = box[1], x1 = box[2], y1 = box[3];
    if (x1 < x0) { double t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { double t = y0; y0 = y1; y1 = t; }
    double pw = x1 - x0, ph = y1 - y0;
    if (pw <= 0.0) pw = 1.0;
    if (ph <= 0.0) ph = 1.0;

    bool girada = (rot == 90 || rot == 270);
    double uw = girada ? (double)alto : (double)largo;   /* o lienzo virtual */
    double uh = girada ? (double)largo : (double)alto;
    double sxu = uw / pw, syu = uh / ph;

    /* do espaco do PDF para o virtual */
    pdf_mat_t s = pdf_mat_identidade();
    s.a = sxu;
    s.d = -syu;
    s.e = -x0 * sxu;
    s.f = y1 * syu;

    /* a rotacao */
    pdf_mat_t r = pdf_mat_identidade();
    if (rot == 90) { r.a = 0; r.b = 1; r.c = -1; r.d = 0; r.e = uh; r.f = 0; }
    else if (rot == 180) { r.a = -1; r.b = 0; r.c = 0; r.d = -1; r.e = uw; r.f = uh; }
    else if (rot == 270) { r.a = 0; r.b = -1; r.c = 1; r.d = 0; r.e = 0; r.f = uw; }
    if (rot_saida) *rot_saida = rot;
    /* Primeiro a transformacao do espaco do PDF, depois a rotacao. */
    return pdf_mat_multiplica(&s, &r);
}

/* -------------------------------------------------------------- render */

static int render_interno(pdf_doc_t *d, int pagina, int largo, int alto, uint8_t *rgba,
                          pdf_car_t **cars, int *n_cars)
{
    if (!d || (!rgba && !cars) || largo <= 0 || alto <= 0) return PDF_ERRO_PAGINA;
    if (pagina < 0 || pagina >= pdf_doc_paginas(d)) return PDF_ERRO_PAGINA;
    pdf_obj_t *pag = pdf_doc_pagina(d, pagina);
    if (!pag) return PDF_ERRO_PAGINA;

    /* se o documento mudou, as fontes da cache ya no valen */
    cache_fontes_outro_doc(d);

    if (rgba) memset(rgba, 0, (size_t)largo * (size_t)alto * 4);

    ctx_t ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.so_texto = (cars != NULL);
    ctx.doc = d;
    ctx.largo = largo;
    ctx.alto = alto;
    ctx.rgba = rgba;
    /* A cero: pintar_caminho acumula la cobertura con += (con basura, manchas). */
    ctx.cob = aloca_zerado((size_t)largo * sizeof(double));
    ctx.rec = aloca_zerado((size_t)largo * sizeof(double));
    if (!ctx.cob || !ctx.rec) {
        if (ctx.cob) libera(ctx.cob);
        if (ctx.rec) libera(ctx.rec);
        return PDF_ERRO_MEMORIA;
    }
    caminho_init(&ctx.cam);

    /* o estado inicial */
    ctx.est.ctm = matriz_base(d, pag, pagina, largo, alto, NULL);
    /* A matriz de texto e a de linha comecam na identidade (a "Td" move-se
     * a partir de ai). */
    ctx.est.tm = pdf_mat_identidade();
    ctx.est.tlm = pdf_mat_identidade();
    ctx.est.largura = 1.0;
    ctx.est.limite_miter = 10.0;
    ctx.est.alfa_preench = 1.0;
    ctx.est.alfa_trazo = 1.0;
    ctx.est.esc_hor = 100.0;
    ctx.est.preench.r = 0; ctx.est.preench.g = 0; ctx.est.preench.b = 0; ctx.est.preench.a = 1;
    ctx.est.trazo = ctx.est.preench;

    /* os recursos da pagina */
    ctx.recursos = dic(d, pag, "Resources");

    /* o conteudo */
    pdf_obj_t *cont = dic(d, pag, "Contents");
    uint8_t *concat = NULL;
    const uint8_t *dados = NULL;
    int tam = 0;
    if (cont && cont->tipo == PDF_OBJ_STREAM) {
        dados = cont->datos;
        tam = cont->tam_datos;
    } else if (cont && cont->tipo == PDF_OBJ_ARREGLO) {
        int total = 0;
        for (int i = 0; i < cont->n_itens; i++) {
            pdf_obj_t *s = resolver_ref(d, cont->itens[i]);
            if (s && s->tipo == PDF_OBJ_STREAM) total += s->tam_datos + 1;
        }
        if (total > 0) {
            concat = aloca((size_t)total);
            if (concat) {
                int k = 0;
                for (int i = 0; i < cont->n_itens; i++) {
                    pdf_obj_t *s = resolver_ref(d, cont->itens[i]);
                    if (!s || s->tipo != PDF_OBJ_STREAM || !s->datos) continue;
                    memcpy(concat + k, s->datos, (size_t)s->tam_datos);
                    k += s->tam_datos;
                    concat[k++] = '\n';
                }
                dados = concat;
                tam = k;
            }
        }
    }
    if (dados) executar_conteudo(&ctx, dados, tam, ctx.recursos, 0);

    /* limpa */
    estado_liberar(&ctx.est);
    for (int i = 0; i < ctx.n_pilha; i++) estado_liberar(&ctx.pilha[i]);
    if (ctx.pilha) libera(ctx.pilha);
    caminho_liberar(&ctx.cam);
    pdf_caminho_liberar(&ctx.flat);
    pdf_caminho_liberar(&ctx.plano);
    if (ctx.cob) libera(ctx.cob);
    if (ctx.rec) libera(ctx.rec);
    if (ctx.crux) libera(ctx.crux);
    if (ctx.crud) libera(ctx.crud);
    if (concat) libera(concat);
    if (cars) {
        *cars = ctx.cars;
        *n_cars = ctx.n_cars;
    }
    return 0;
}

int pdf_render(pdf_doc_t *d, int pagina, int largo, int alto, uint8_t *rgba)
{
    return render_interno(d, pagina, largo, alto, rgba, NULL, NULL);
}

/* El texto de una página con la posición de cada carácter (mejora 105). Va a
 * escala 1 punto = 1 píxel, así que las cajas salen en décimas de punto. */
int pdf_texto_pagina(pdf_doc_t *d, int pagina, pdf_car_t **cars, int *n_cars)
{
    if (!d || !cars || !n_cars) return PDF_ERRO_PAGINA;
    *cars = NULL;
    *n_cars = 0;
    int pl = pdf_doc_pagina_largura(d, pagina);
    int pa = pdf_doc_pagina_altura(d, pagina);
    if (pl <= 0 || pa <= 0) return PDF_ERRO_PAGINA;
    return render_interno(d, pagina, pl, pa, NULL, cars, n_cars);
}
