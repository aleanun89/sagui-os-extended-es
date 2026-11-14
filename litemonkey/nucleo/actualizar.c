// SPDX-License-Identifier: GPL-2.0-or-later
/* actualizar.c - operadores de actualización ($set, $unset, $inc, $push,
 * $pull, $rename y algunos más), reemplazo y upsert.
 *
 * El documento se pasa a un árbol (lmk_nodo) que solo se expande por donde
 * pasan los caminos: lo que no se toca se copia tal cual al volver a BSON. */
#include "lmk.h"

static const uint8_t DOC_VACIO[5] = { 5, 0, 0, 0, 0 };

enum { U_SET = 1, U_UNSET, U_INC, U_MUL, U_MIN, U_MAX, U_PUSH, U_ADDTOSET, U_PULL, U_POP, U_RENAME, U_SETONINSERT };
static const struct { const char *n; int op; } OPS[] = {
    { "$set", U_SET }, { "$unset", U_UNSET }, { "$inc", U_INC }, { "$mul", U_MUL }, { "$min", U_MIN },
    { "$max", U_MAX }, { "$push", U_PUSH }, { "$addToSet", U_ADDTOSET }, { "$pull", U_PULL }, { "$pop", U_POP },
    { "$rename", U_RENAME }, { "$setOnInsert", U_SETONINSERT },
};

static int op_de(const char *n)
{
    for (unsigned i = 0; i < sizeof OPS / sizeof OPS[0]; i++)
        if (strcmp(n, OPS[i].n) == 0) return OPS[i].op;
    return 0;
}

bool actualizacion_con_operadores(const uint8_t *u) { return !bson_vacio(u) && u[5] == '$'; }

static bool camino_valido(const char *c, uint32_t n)
{
    if (!n || c[0] == '$' || c[0] == '.' || c[n - 1] == '.') return false;
    for (uint32_t i = 1; i < n; i++)
        if (c[i] == '.' && c[i - 1] == '.') return false;
    return true;
}

bool actualizacion_validar(const uint8_t *u, lmk_error *err)
{
    bson_it it;
    bson_elem e;
    char msg[160];
    if (!actualizacion_con_operadores(u)) {   /* reemplazo: sin $ en ningún campo de primer nivel */
        bson_it_iniciar(&it, u);
        while (bson_it_sig(&it, &e))
            if (e.nombre[0] == '$') return lmk_fallo(err, ERR_VALOR, "un reemplazo no puede tener operadores ($...)");
        return true;
    }
    bson_it_iniciar(&it, u);
    while (bson_it_sig(&it, &e)) {
        int op = op_de(e.nombre);
        if (!op) {
            lmk_fmt(msg, sizeof msg, "operador de actualización desconocido: %s", e.nombre);
            return lmk_fallo(err, ERR_VALOR, msg);
        }
        if (e.tipo != BT_DOC) {
            lmk_fmt(msg, sizeof msg, "%s necesita un documento", e.nombre);
            return lmk_fallo(err, ERR_VALOR, msg);
        }
        bson_it ic;
        bson_elem c;
        bson_it_iniciar(&ic, e.v);
        while (bson_it_sig(&ic, &c)) {
            if (!camino_valido(c.nombre, c.nlen)) {
                lmk_fmt(msg, sizeof msg, "%s: nombre de campo no válido", e.nombre);
                return lmk_fallo(err, ERR_VALOR, msg);
            }
            if ((op == U_INC || op == U_MUL) && !bson_es_numero(c.tipo)) {
                lmk_fmt(msg, sizeof msg, "%s necesita números (campo %s)", e.nombre, c.nombre);
                return lmk_fallo(err, ERR_TIPO, msg);
            }
            if (op == U_RENAME && (c.tipo != BT_STRING || !camino_valido(bson_cadena(&c), bson_cadena_len(&c))))
                return lmk_fallo(err, ERR_VALOR, "$rename: el nombre nuevo tiene que ser una cadena válida");
            if (op == U_POP && !bson_es_numero(c.tipo)) return lmk_fallo(err, ERR_VALOR, "$pop vale 1 o -1");
            if ((op == U_PUSH || op == U_ADDTOSET) && c.tipo == BT_DOC && !bson_vacio(c.v) && c.v[5] == '$') {
                bson_it ie;
                bson_elem m;
                bson_it_iniciar(&ie, c.v);
                while (bson_it_sig(&ie, &m)) {
                    if (!nombre_es(&m, "$each")) return lmk_fallo(err, ERR_VALOR, "$push/$addToSet: solo se admite $each");
                    if (m.tipo != BT_ARRAY) return lmk_fallo(err, ERR_VALOR, "$each necesita un array");
                }
            }
            if (op == U_PULL && c.tipo == BT_DOC && !bson_vacio(c.v)) {
                /* {"$gte": 5} se valida como condición de un campo; {"a": 2}, como filtro */
                lmk_buf f = { 0 };
                uint32_t ini = bson_inicio(&f);
                bson_poner_elem(&f, "v", 1, &c);
                bson_fin(&f, ini);
                bool ok = !f.sin_memoria && consulta_validar(c.v[5] == '$' ? f.p : c.v, err);
                buf_liberar(&f);
                if (!ok) return false;
            }
        }
    }
    return true;
}

/* ------------------------------------------------------------ caminos en el árbol */

static bool es_indice(const char *s, uint32_t n, uint32_t *v)
{
    if (!n || n > 7) return false;
    uint32_t x = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        x = x * 10 + (uint32_t)(s[i] - '0');
    }
    *v = x;
    return true;
}

static void nodo_vacio(lmk_nodo *n, uint8_t tipo)
{
    n->tipo = tipo;
    n->v = DOC_VACIO;
    n->vlen = 5;
    n->expandido = true;
    n->hijos = NULL;
    n->n = n->cap = 0;
}

static bool error_camino(lmk_error *err, const char *c, uint32_t clen)
{
    char msg[220], cam[121];
    uint32_t n = LMK_MIN(clen, 120u);
    memcpy(cam, c, n);
    cam[n] = 0;
    lmk_fmt(msg, sizeof msg, "no se puede crear el campo en '%s': hay un valor que no es documento en el camino", cam);
    return lmk_fallo(err, ERR_PATH, msg);
}

/* El hijo "nom" de un documento o array expandido; crear: lo crea (null). */
static lmk_nodo *hijo(lmk_arena *a, lmk_nodo *p, const char *nom, uint32_t nlen, bool crear, bool *nuevo)
{
    if (nuevo) *nuevo = false;
    if (p->tipo == BT_ARRAY) {
        uint32_t i;
        if (!es_indice(nom, nlen, &i)) return NULL;
        if (i < p->n) return &p->hijos[i];
        if (!crear || i > p->n + 100000) return NULL;
        while (p->n <= i) {
            char t[12];
            uint32_t l = bson_indice(t, p->n);
            if (!arbol_anadir(a, p, t, l)) return NULL;
        }
        if (nuevo) *nuevo = true;
        return &p->hijos[i];
    }
    int i = arbol_hijo(p, nom, nlen);
    if (i >= 0) return &p->hijos[i];
    if (!crear) return NULL;
    if (nuevo) *nuevo = true;
    return arbol_anadir(a, p, nom, nlen);
}

/* El padre del último componente del camino (expandido), o NULL. */
static lmk_nodo *padre(lmk_arena *a, lmk_nodo *raiz, const char *c, uint32_t clen, const char **ult, uint32_t *ulen,
                       bool crear, lmk_error *err)
{
    lmk_nodo *n = raiz;
    const char *p = c;
    uint32_t resto = clen;
    for (;;) {
        const char *punto = memchr(p, '.', resto);
        if (!punto) { *ult = p; *ulen = resto; return n; }
        uint32_t k = (uint32_t)(punto - p);
        bool nuevo;
        lmk_nodo *h = hijo(a, n, p, k, crear, &nuevo);
        if (!h) {
            if (crear && n->tipo == BT_ARRAY) error_camino(err, c, clen);
            return NULL;
        }
        if (nuevo || (crear && h->tipo == BT_NULL)) nodo_vacio(h, BT_DOC);
        if (h->tipo != BT_DOC && h->tipo != BT_ARRAY) {
            if (crear) error_camino(err, c, clen);
            return NULL;
        }
        if (!arbol_expandir(a, h)) { lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria"); return NULL; }
        n = h;
        p = punto + 1;
        resto -= k + 1;
    }
}

/* El valor de un nodo como elemento BSON (serializa si se modificó). */
static bool elem_de(lmk_arena *a, lmk_nodo *n, bson_elem *e)
{
    e->tipo = n->tipo;
    e->nombre = n->nombre;
    e->nlen = n->nlen;
    if (!n->expandido) { e->v = n->v; e->vlen = n->vlen; return true; }
    lmk_buf b = { 0 };
    arbol_escribir_valor(n, &b);
    if (b.sin_memoria) { buf_liberar(&b); return false; }
    uint8_t *p = arena_aloca(a, b.n);
    if (!p) { buf_liberar(&b); return false; }
    memcpy(p, b.p, b.n);
    e->v = p;
    e->vlen = b.n;
    buf_liberar(&b);
    return true;
}

static void poner_numero(lmk_arena *a, lmk_nodo *n, uint8_t tipo, int64_t i, double d)
{
    uint8_t *p = arena_aloca(a, 8);
    if (!p) return;
    if (tipo == BT_INT32 && (i < INT32_MIN || i > INT32_MAX)) tipo = BT_INT64;
    if (tipo == BT_INT32) { lmk_pon32(p, (uint32_t)i); n->vlen = 4; }
    else if (tipo == BT_INT64) { lmk_pon64(p, (uint64_t)i); n->vlen = 8; }
    else { uint64_t u; memcpy(&u, &d, 8); lmk_pon64(p, u); n->vlen = 8; }
    n->tipo = tipo;
    n->v = p;
    n->expandido = false;
    n->hijos = NULL;
    n->n = n->cap = 0;
}

static bool aritmetica(lmk_arena *a, lmk_nodo *n, const bson_elem *arg, bool mul, lmk_error *err)
{
    bson_elem actual = { .tipo = n->tipo, .v = n->v, .vlen = n->vlen };
    if (!bson_es_numero(n->tipo)) {
        char msg[160];
        lmk_fmt(msg, sizeof msg, "%s: el campo '%s' no es un número", mul ? "$mul" : "$inc", n->nombre);
        return lmk_fallo(err, ERR_TIPO, msg);
    }
    if (n->tipo == BT_DOUBLE || arg->tipo == BT_DOUBLE) {
        double x = bson_a_double(&actual), y = bson_a_double(arg);
        poner_numero(a, n, BT_DOUBLE, 0, mul ? x * y : x + y);
        return true;
    }
    int64_t x = bson_a_int64(&actual), y = bson_a_int64(arg);
    uint8_t tipo = (n->tipo == BT_INT64 || arg->tipo == BT_INT64) ? BT_INT64 : BT_INT32;
    poner_numero(a, n, tipo, mul ? x * y : x + y, 0);
    return true;
}

static bool esta_en(lmk_arena *a, lmk_nodo *arr, const bson_elem *v)
{
    for (uint32_t i = 0; i < arr->n; i++) {
        bson_elem e;
        if (elem_de(a, &arr->hijos[i], &e) && bson_rango(e.tipo) == bson_rango(v->tipo) && elem_comparar(&e, v) == 0)
            return true;
    }
    return false;
}

static bool anadir_array(lmk_arena *a, lmk_nodo *arr, const bson_elem *v, bool sin_repetir)
{
    if (sin_repetir && esta_en(a, arr, v)) return true;
    char t[12];
    uint32_t l = bson_indice(t, arr->n);
    lmk_nodo *h = arbol_anadir(a, arr, t, l);
    if (!h) return false;
    arbol_poner(h, v);
    return true;
}

static bool pull_casa(lmk_arena *a, lmk_nodo *n, const bson_elem *cond)
{
    bson_elem e;
    if (!elem_de(a, n, &e)) return false;
    if (cond->tipo == BT_DOC && !bson_vacio(cond->v)) {
        if (cond->v[5] == '$') return consulta_valor_cumple(&e, cond->v);
        if (e.tipo == BT_DOC) return consulta_coincide(e.v, cond->v);
        return false;
    }
    if (cond->tipo == BT_REGEX) {
        if (e.tipo != BT_STRING) return false;
        static lmk_regex rx;
        const char *pat = (const char *)cond->v;
        if (!regex_compilar(&rx, pat, (uint32_t)strlen(pat), pat + strlen(pat) + 1, NULL)) return false;
        return regex_buscar(&rx, bson_cadena(&e), bson_cadena_len(&e));
    }
    return bson_rango(e.tipo) == bson_rango(cond->tipo) && elem_comparar(&e, cond) == 0;
}

static bool aplicar_op(lmk_arena *a, lmk_nodo *raiz, int op, const bson_elem *c, bool insercion, lmk_error *err)
{
    const char *ult;
    uint32_t ulen;
    bool crear = op != U_UNSET && op != U_PULL && op != U_POP && op != U_RENAME;
    if (op == U_SETONINSERT) {
        if (!insercion) return true;
        op = U_SET;
    }
    lmk_nodo *p = padre(a, raiz, c->nombre, c->nlen, &ult, &ulen, crear, err);
    if (!p) return !crear;   /* sin crear y el camino no existe: no hay nada que hacer */
    bool nuevo;
    lmk_nodo *n = hijo(a, p, ult, ulen, crear, &nuevo);
    if (!n) {
        if (crear) return p->tipo == BT_ARRAY ? error_camino(err, c->nombre, c->nlen) : lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
        return true;
    }
    switch (op) {
    case U_SET:
        arbol_poner(n, c);
        return true;
    case U_UNSET:
        if (p->tipo == BT_ARRAY) { n->tipo = BT_NULL; n->v = DOC_VACIO; n->vlen = 0; n->expandido = false; }
        else arbol_quitar(p, (uint32_t)(n - p->hijos));
        return true;
    case U_INC: case U_MUL:
        if (nuevo) {
            if (op == U_INC) arbol_poner(n, c);
            else poner_numero(a, n, c->tipo == BT_DOUBLE ? BT_DOUBLE : c->tipo, 0, 0.0);
            return true;
        }
        return aritmetica(a, n, c, op == U_MUL, err);
    case U_MIN: case U_MAX: {
        if (nuevo) { arbol_poner(n, c); return true; }
        bson_elem e;
        if (!elem_de(a, n, &e)) return lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
        int r = elem_comparar(c, &e);
        if ((op == U_MIN && r < 0) || (op == U_MAX && r > 0)) arbol_poner(n, c);
        return true;
    }
    case U_PUSH: case U_ADDTOSET: {
        if (nuevo) nodo_vacio(n, BT_ARRAY);
        if (n->tipo != BT_ARRAY) {
            char msg[160];
            lmk_fmt(msg, sizeof msg, "%s: el campo '%s' no es un array", op == U_PUSH ? "$push" : "$addToSet", n->nombre);
            return lmk_fallo(err, ERR_TIPO, msg);
        }
        if (!arbol_expandir(a, n)) return lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
        bson_elem cada;
        if (c->tipo == BT_DOC && bson_buscar(c->v, "$each", &cada)) {
            bson_it it;
            bson_elem v;
            bson_it_iniciar(&it, cada.v);
            while (bson_it_sig(&it, &v))
                if (!anadir_array(a, n, &v, op == U_ADDTOSET)) return lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
            return true;
        }
        if (!anadir_array(a, n, c, op == U_ADDTOSET)) return lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
        return true;
    }
    case U_PULL: {
        if (n->tipo != BT_ARRAY) return n->tipo == BT_NULL ? true : lmk_fallo(err, ERR_TIPO, "$pull: el campo no es un array");
        if (!arbol_expandir(a, n)) return lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
        uint32_t j = 0;
        for (uint32_t i = 0; i < n->n; i++)
            if (!pull_casa(a, &n->hijos[i], c)) n->hijos[j++] = n->hijos[i];
        n->n = j;
        return true;
    }
    case U_POP: {
        if (n->tipo != BT_ARRAY) return lmk_fallo(err, ERR_TIPO, "$pop: el campo no es un array");
        if (!arbol_expandir(a, n)) return lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
        if (n->n) arbol_quitar(n, bson_a_double(c) < 0 ? 0 : n->n - 1);
        return true;
    }
    case U_RENAME: {
        if (p->tipo == BT_ARRAY) return lmk_fallo(err, ERR_VALOR, "$rename no vale dentro de un array");
        lmk_nodo copia = *n;
        arbol_quitar(p, (uint32_t)(n - p->hijos));
        const char *dst = bson_cadena(c);
        uint32_t dlen = bson_cadena_len(c);
        lmk_nodo *p2 = padre(a, raiz, dst, dlen, &ult, &ulen, true, err);
        if (!p2) return false;
        if (p2->tipo == BT_ARRAY) return lmk_fallo(err, ERR_VALOR, "$rename no vale dentro de un array");
        lmk_nodo *d = hijo(a, p2, ult, ulen, true, NULL);
        if (!d) return lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
        const char *nom = d->nombre;
        uint32_t nl = d->nlen;
        *d = copia;
        d->nombre = nom;
        d->nlen = nl;
        return true;
    }
    }
    return true;
}

static bool aplicar(lmk_arena *a, lmk_nodo *raiz, const uint8_t *u, bool insercion, lmk_error *err)
{
    bson_it it;
    bson_elem e;
    bson_it_iniciar(&it, u);
    while (bson_it_sig(&it, &e)) {
        int op = op_de(e.nombre);
        bson_it ic;
        bson_elem c;
        bson_it_iniciar(&ic, e.v);
        while (bson_it_sig(&ic, &c))
            if (!aplicar_op(a, raiz, op, &c, insercion, err)) return false;
    }
    return true;
}

static bool mismo_id(const uint8_t *a, const uint8_t *b)
{
    bson_elem x, y;
    bool hx = bson_buscar(a, "_id", &x), hy = bson_buscar(b, "_id", &y);
    if (hx != hy) return false;
    return !hx || (x.tipo == y.tipo && elem_comparar(&x, &y) == 0);
}

static bool terminar(lmk_nodo *raiz, lmk_buf *out, lmk_error *err)
{
    uint32_t ini = out->n;
    arbol_escribir(raiz, out);
    if (out->sin_memoria) return lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
    if (out->n - ini > LMK_DOC_MAX) return lmk_fallo(err, ERR_ES, "el documento pasaría de 1 MB");
    return true;
}

bool actualizar_doc(const uint8_t *doc, const uint8_t *u, lmk_buf *out, bool *cambio, lmk_error *err)
{
    lmk_arena a = { 0 };
    lmk_nodo *raiz = arbol_desde(&a, doc);
    bool ok = raiz && aplicar(&a, raiz, u, false, err);
    if (!raiz) lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
    uint32_t ini = out->n;
    if (ok) ok = terminar(raiz, out, err);
    arena_liberar(&a);
    if (!ok) return false;
    if (!mismo_id(doc, out->p + ini))
        return lmk_fallo(err, ERR_ID_INMUTABLE, "no se puede cambiar el _id de un documento");
    *cambio = out->n - ini != bson_tam(doc) || memcmp(out->p + ini, doc, out->n - ini) != 0;
    return true;
}

bool reemplazar_doc(const uint8_t *doc, const uint8_t *nuevo, lmk_buf *out, lmk_error *err)
{
    bson_elem id, nid;
    bool tiene = bson_buscar(doc, "_id", &id);
    if (bson_buscar(nuevo, "_id", &nid) && (!tiene || elem_comparar(&id, &nid) != 0 || id.tipo != nid.tipo))
        return lmk_fallo(err, ERR_ID_INMUTABLE, "el reemplazo no puede cambiar el _id");
    uint32_t ini = bson_inicio(out);
    if (tiene) bson_poner_elem(out, "_id", 3, &id);
    bson_it it;
    bson_elem e;
    bson_it_iniciar(&it, nuevo);
    while (bson_it_sig(&it, &e))
        if (!nombre_es(&e, "_id")) bson_poner_elem(out, e.nombre, e.nlen, &e);
    bson_fin(out, ini);
    if (out->sin_memoria) return lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
    if (out->n - ini > LMK_DOC_MAX) return lmk_fallo(err, ERR_ES, "el documento pasaría de 1 MB");
    return true;
}

/* Las igualdades del filtro forman el documento nuevo de un upsert. */
static bool igualdades(lmk_arena *a, lmk_nodo *raiz, const uint8_t *filtro, lmk_error *err)
{
    bson_it it;
    bson_elem e;
    bson_it_iniciar(&it, filtro);
    while (bson_it_sig(&it, &e)) {
        if (e.nombre[0] == '$') {
            if (nombre_es(&e, "$and")) {
                bson_it ia;
                bson_elem x;
                bson_it_iniciar(&ia, e.v);
                while (bson_it_sig(&ia, &x))
                    if (!igualdades(a, raiz, x.v, err)) return false;
            }
            continue;
        }
        bson_elem v = e;
        if (e.tipo == BT_DOC && !bson_vacio(e.v) && e.v[5] == '$') {
            if (!bson_buscar(e.v, "$eq", &v)) continue;
        } else if (e.tipo == BT_REGEX) {
            continue;
        }
        if (!aplicar_op(a, raiz, U_SET, &(bson_elem){ .tipo = v.tipo, .nombre = e.nombre, .nlen = e.nlen, .v = v.v, .vlen = v.vlen },
                        true, err))
            return false;
    }
    return true;
}

bool upsert_doc(const uint8_t *filtro, const uint8_t *u, lmk_buf *out, lmk_error *err)
{
    if (!actualizacion_con_operadores(u)) {
        bson_elem id;
        if (bson_buscar(u, "_id", &id) || !bson_buscar(filtro, "_id", &id) ||
            (id.tipo == BT_DOC && !bson_vacio(id.v) && id.v[5] == '$')) {
            buf_poner(out, u, bson_tam(u));
            return !out->sin_memoria || lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
        }
        uint32_t ini = bson_inicio(out);
        bson_poner_elem(out, "_id", 3, &id);
        bson_it it;
        bson_elem e;
        bson_it_iniciar(&it, u);
        while (bson_it_sig(&it, &e)) bson_poner_elem(out, e.nombre, e.nlen, &e);
        bson_fin(out, ini);
        return !out->sin_memoria || lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
    }
    lmk_arena a = { 0 };
    lmk_nodo *raiz = arbol_desde(&a, DOC_VACIO);
    bool ok = raiz && igualdades(&a, raiz, filtro, err) && aplicar(&a, raiz, u, true, err);
    if (!raiz) lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
    if (ok) ok = terminar(raiz, out, err);
    arena_liberar(&a);
    return ok;
}
