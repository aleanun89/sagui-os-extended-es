// SPDX-License-Identifier: GPL-2.0-or-later
/* pdf_indice.c - el índice (marcadores / outline) de un PDF (mejora 105).
 *
 * Recorre /Outlines del catálogo en profundidad (/First, /Next) y resuelve
 * el destino de cada marcador a un número de página: un array directo
 * [página /XYZ ...], una acción /A << /S /GoTo /D destino >> o un destino
 * con nombre (diccionario /Dests o árbol de nombres /Names /Dests). */
#include "pdf.h"
#include "pdf_lex.h"

#define MAX_MARCAS 2000
#define MAX_PROF   8
#define MAX_PAGS   20000

typedef struct {
    pdf_doc_t   *d;
    pdf_obj_t  **pags;
    int          n_pags;
    pdf_marca_t *marcas;
    int          n, cap;
} ind_t;

static pdf_obj_t *res(pdf_doc_t *d, pdf_obj_t *o)
{
    if (o && o->tipo == PDF_OBJ_REFERENCIA) return pdf_doc_objeto(d, o->num);
    return o;
}

static pdf_obj_t *dic(pdf_doc_t *d, pdf_obj_t *o, const char *k)
{
    if (!o || (o->tipo != PDF_OBJ_DICCIONARIO && o->tipo != PDF_OBJ_STREAM)) return NULL;
    return res(d, pdf_dict_buscar(o, k));
}

/* El índice de la página a la que apunta un destino (-1 si no se sabe). */
static int pagina_de_array(ind_t *t, pdf_obj_t *arr)
{
    if (!arr || arr->tipo != PDF_OBJ_ARREGLO || arr->n_itens < 1) return -1;
    pdf_obj_t *p = arr->itens[0];
    if (!p) return -1;
    if (p->tipo == PDF_OBJ_NUMERO) {          /* destino remoto: número de página */
        int n = (int)p->numero;
        return (n >= 0 && n < t->n_pags) ? n : -1;
    }
    pdf_obj_t *o = res(t->d, p);
    for (int i = 0; i < t->n_pags; i++)
        if (t->pags[i] == o) return i;
    return -1;
}

/* Un destino con nombre en un árbol de nombres. */
static pdf_obj_t *arbol_buscar(ind_t *t, pdf_obj_t *no, const char *nome, int tam, int prof)
{
    if (!no || prof > 12) return NULL;
    pdf_obj_t *nomes = dic(t->d, no, "Names");
    if (nomes && nomes->tipo == PDF_OBJ_ARREGLO) {
        for (int i = 0; i + 1 < nomes->n_itens; i += 2) {
            pdf_obj_t *k = res(t->d, nomes->itens[i]);
            if (k && k->tipo == PDF_OBJ_CADENA && k->tam_cadena == tam && memcmp(k->cadena, nome, (size_t)tam) == 0)
                return res(t->d, nomes->itens[i + 1]);
        }
    }
    pdf_obj_t *kids = dic(t->d, no, "Kids");
    if (kids && kids->tipo == PDF_OBJ_ARREGLO) {
        for (int i = 0; i < kids->n_itens; i++) {
            pdf_obj_t *r = arbol_buscar(t, res(t->d, kids->itens[i]), nome, tam, prof + 1);
            if (r) return r;
        }
    }
    return NULL;
}

static int pagina_de_destino(ind_t *t, pdf_obj_t *dest)
{
    dest = res(t->d, dest);
    if (!dest) return -1;
    if (dest->tipo == PDF_OBJ_ARREGLO) return pagina_de_array(t, dest);
    if (dest->tipo == PDF_OBJ_NOME || dest->tipo == PDF_OBJ_CADENA) {
        const char *nome = dest->tipo == PDF_OBJ_NOME ? dest->nome : dest->cadena;
        int tam = dest->tipo == PDF_OBJ_NOME ? (int)tam_texto(dest->nome) : dest->tam_cadena;
        pdf_obj_t *cat = pdf_doc_catalogo(t->d);
        pdf_obj_t *v = NULL;
        pdf_obj_t *dests = dic(t->d, cat, "Dests");
        if (dests && dest->tipo == PDF_OBJ_NOME) v = dic(t->d, dests, nome);
        if (!v) {
            pdf_obj_t *nomes = dic(t->d, cat, "Names");
            pdf_obj_t *raiz = dic(t->d, nomes, "Dests");
            if (raiz) v = arbol_buscar(t, raiz, nome, tam, 0);
        }
        if (!v) return -1;
        if (v->tipo == PDF_OBJ_DICCIONARIO) v = dic(t->d, v, "D");
        return pagina_de_array(t, v);
    }
    return -1;
}

static void anadir(ind_t *t, pdf_obj_t *item, int nivel)
{
    if (t->n >= MAX_MARCAS) return;
    if (t->n >= t->cap) {
        int nc = t->cap ? t->cap * 2 : 64;
        pdf_marca_t *m = realoca(t->marcas, (size_t)nc * sizeof(pdf_marca_t));
        if (!m) return;
        t->marcas = m;
        t->cap = nc;
    }
    pdf_marca_t *m = &t->marcas[t->n];
    memset(m, 0, sizeof *m);
    m->nivel = nivel;
    pdf_obj_t *ti = dic(t->d, item, "Title");
    const char *tx = ti ? pdf_doc_texto(t->d, ti) : "";
    int k = 0;
    while (tx && tx[k] && k < (int)sizeof m->titulo - 1) { m->titulo[k] = tx[k]; k++; }
    m->titulo[k] = 0;
    pdf_obj_t *dest = dic(t->d, item, "Dest");
    int pg = -1;
    if (dest) pg = pagina_de_destino(t, dest);
    if (pg < 0) {
        pdf_obj_t *a = dic(t->d, item, "A");
        pdf_obj_t *dd = dic(t->d, a, "D");
        if (dd) pg = pagina_de_destino(t, dd);
    }
    m->pagina = pg;
    t->n++;
}

static void recorrer(ind_t *t, pdf_obj_t *primero, int nivel)
{
    int guarda = 0;
    for (pdf_obj_t *it = primero; it && guarda < MAX_MARCAS; it = dic(t->d, it, "Next"), guarda++) {
        if (it->tipo != PDF_OBJ_DICCIONARIO) break;
        anadir(t, it, nivel);
        if (nivel + 1 < MAX_PROF) {
            pdf_obj_t *hijo = dic(t->d, it, "First");
            if (hijo) recorrer(t, hijo, nivel + 1);
        }
        if (t->n >= MAX_MARCAS) break;
    }
}

int pdf_indice(pdf_t *d, pdf_marca_t **marcas)
{
    if (!d || !marcas) return 0;
    *marcas = NULL;
    pdf_obj_t *cat = pdf_doc_catalogo(d);
    pdf_obj_t *out = dic(d, cat, "Outlines");
    pdf_obj_t *primero = dic(d, out, "First");
    if (!primero) return 0;
    ind_t t;
    memset(&t, 0, sizeof t);
    t.d = d;
    int np = pdf_doc_paginas(d);
    if (np > MAX_PAGS) np = MAX_PAGS;
    t.pags = aloca((size_t)(np > 0 ? np : 1) * sizeof(pdf_obj_t *));
    if (!t.pags) return 0;
    for (int i = 0; i < np; i++) t.pags[i] = pdf_doc_pagina(d, i);
    t.n_pags = np;
    recorrer(&t, primero, 0);
    libera(t.pags);
    *marcas = t.marcas;
    return t.n;
}
