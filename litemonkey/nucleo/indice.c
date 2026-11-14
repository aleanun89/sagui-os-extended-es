// SPDX-License-Identifier: GPL-2.0-or-later
/* indice.c - índices secundarios: un treap (árbol binario de búsqueda con
 * prioridades al azar) ordenado por (clave, ranura). Inserción y borrado en
 * O(log n) esperado y recorrido de rangos en orden. Cada nodo guarda una
 * copia de la clave (el valor BSON del campo), así el índice no depende de
 * que el documento esté en memoria. */
#include "lmk.h"

struct lmk_inodo {
    lmk_inodo *izq, *der;
    uint32_t   prio;
    uint32_t   ranura;
    uint32_t   klen;
    uint8_t    tipo;
    uint8_t    clave[];
};

static uint32_t azar(lmk_indice *ix)
{
    uint32_t x = ix->semilla ? ix->semilla : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    ix->semilla = x;
    return x;
}

static int cmp(const lmk_inodo *n, uint8_t tipo, const uint8_t *v, uint32_t ranura)
{
    int r = bson_comparar(n->tipo, n->clave, tipo, v);
    if (r) return r;
    return n->ranura < ranura ? -1 : n->ranura > ranura;
}

static lmk_inodo *rot_der(lmk_inodo *n) { lmk_inodo *l = n->izq; n->izq = l->der; l->der = n; return l; }
static lmk_inodo *rot_izq(lmk_inodo *n) { lmk_inodo *r = n->der; n->der = r->izq; r->izq = n; return r; }

static lmk_inodo *insertar(lmk_inodo *t, lmk_inodo *n)
{
    if (!t) return n;
    if (cmp(t, n->tipo, n->clave, n->ranura) > 0) {
        t->izq = insertar(t->izq, n);
        if (t->izq->prio > t->prio) t = rot_der(t);
    } else {
        t->der = insertar(t->der, n);
        if (t->der->prio > t->prio) t = rot_izq(t);
    }
    return t;
}

bool indice_insertar(lmk_indice *ix, const bson_elem *clave, uint32_t ranura)
{
    lmk_inodo *n = lmk_aloca(sizeof *n + clave->vlen);
    if (!n) return false;
    n->izq = n->der = NULL;
    n->prio = azar(ix);
    n->ranura = ranura;
    n->klen = clave->vlen;
    n->tipo = clave->tipo;
    memcpy(n->clave, clave->v, clave->vlen);
    ix->raiz = insertar(ix->raiz, n);
    ix->n++;
    return true;
}

static lmk_inodo *unir(lmk_inodo *a, lmk_inodo *b)
{
    if (!a) return b;
    if (!b) return a;
    if (a->prio > b->prio) { a->der = unir(a->der, b); return a; }
    b->izq = unir(a, b->izq);
    return b;
}

static lmk_inodo *borrar(lmk_indice *ix, lmk_inodo *t, uint8_t tipo, const uint8_t *v, uint32_t ranura)
{
    if (!t) return NULL;
    int r = cmp(t, tipo, v, ranura);
    if (r > 0) t->izq = borrar(ix, t->izq, tipo, v, ranura);
    else if (r < 0) t->der = borrar(ix, t->der, tipo, v, ranura);
    else {
        lmk_inodo *u = unir(t->izq, t->der);
        lmk_libera(t);
        ix->n--;
        return u;
    }
    return t;
}

void indice_borrar(lmk_indice *ix, const bson_elem *clave, uint32_t ranura)
{
    ix->raiz = borrar(ix, ix->raiz, clave->tipo, clave->v, ranura);
}

typedef struct {
    const bson_elem *lo, *hi;
    bool lo_incl, hi_incl;
    indice_fn f;
    void *ctx;
    uint32_t vistas;
    bool parar;
} recorrido_t;

static void recorrer(lmk_inodo *t, recorrido_t *r)
{
    while (t && !r->parar) {
        int clo = r->lo ? bson_comparar(t->tipo, t->clave, r->lo->tipo, r->lo->v) : 1;
        int chi = r->hi ? bson_comparar(t->tipo, t->clave, r->hi->tipo, r->hi->v) : -1;
        bool sobre_lo = clo > 0 || (clo == 0 && r->lo_incl);
        bool bajo_hi = chi < 0 || (chi == 0 && r->hi_incl);
        if (clo >= 0) recorrer(t->izq, r);      /* puede haber más a la izquierda */
        if (r->parar) return;
        if (sobre_lo && bajo_hi) {
            r->vistas++;
            if (r->f(r->ctx, t->ranura)) { r->parar = true; return; }
        }
        if (chi > 0) return;                    /* todo lo de la derecha es mayor */
        t = t->der;
    }
}

uint32_t indice_recorrer(lmk_indice *ix, const bson_elem *lo, bool lo_incl, const bson_elem *hi, bool hi_incl,
                         indice_fn f, void *ctx)
{
    recorrido_t r = { lo, hi, lo_incl, hi_incl, f, ctx, 0, false };
    recorrer(ix->raiz, &r);
    return r.vistas;
}

static bool otra_ranura(void *ctx, uint32_t ranura) { return ranura != *(uint32_t *)ctx; }

bool indice_hay_otro(lmk_indice *ix, const bson_elem *clave, uint32_t ranura)
{
    /* Con tantas entradas iguales como ranuras distintas se para en la primera. */
    bool hay = false;
    recorrido_t r = { clave, clave, true, true, otra_ranura, &ranura, 0, false };
    recorrer(ix->raiz, &r);
    hay = r.parar;
    return hay;
}

static void liberar(lmk_inodo *t)
{
    while (t) {
        liberar(t->izq);
        lmk_inodo *d = t->der;
        lmk_libera(t);
        t = d;
    }
}

void indice_vaciar(lmk_indice *ix)
{
    liberar(ix->raiz);
    ix->raiz = NULL;
    ix->n = 0;
}

/* ------------------------------------------------------------ claves de un documento */

#define CLAVES_MAX 256
typedef struct {
    bson_elem v[CLAVES_MAX];
    uint32_t  n;
} claves_t;

static bool visita_clave(void *c, const bson_elem *e, bool de_array)
{
    claves_t *k = c;
    if (e->tipo == BT_ARRAY && !de_array) {
        if (!bson_vacio(e->v)) return false;     /* cuentan sus elementos */
    }
    for (uint32_t i = 0; i < k->n; i++)
        if (k->v[i].tipo == e->tipo && elem_comparar(&k->v[i], e) == 0) return false;
    if (k->n < CLAVES_MAX) k->v[k->n++] = *e;
    return false;
}

bool indice_claves(const lmk_indice *ix, const uint8_t *doc, clave_fn f, void *ctx)
{
    static claves_t k;   /* el servidor es de un solo hilo y esto no se anida */
    k.n = 0;
    bson_camino(doc, ix->campo, (uint32_t)strlen(ix->campo), visita_clave, &k);
    if (!k.n) {
        static const uint8_t nada[1] = { 0 };
        bson_elem nulo = { .tipo = BT_NULL, .v = nada, .vlen = 0 };
        return f(ctx, &nulo);
    }
    /* copia local: f puede volver a llamar a indice_claves de otro índice */
    uint32_t n = k.n;
    bson_elem local[16];
    bson_elem *v = n <= 16 ? local : lmk_aloca(n * sizeof *v);
    if (!v) return false;
    memcpy(v, k.v, n * sizeof *v);
    bool r = true;
    for (uint32_t i = 0; i < n && r; i++) r = f(ctx, &v[i]);
    if (v != local) lmk_libera(v);
    return r;
}
