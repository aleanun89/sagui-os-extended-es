// SPDX-License-Identifier: GPL-2.0-or-later
/* consulta.c - filtros ({"edad": {"$gt": 30}}), proyecciones y orden.
 *
 * El filtro se interpreta directamente sobre el BSON (sin compilarlo): se
 * valida una vez al principio de cada consulta y después la comparación ya
 * no comprueba nada. Las regex sí se compilan, una vez, en una caché
 * pequeña. Semántica de MongoDB: un camino puede dar varios valores (arrays)
 * y la condición se cumple si la cumple alguno; $ne y $nin, si no lo cumple
 * ninguno. */
#include "lmk.h"

enum { OP_EQ = 1, OP_NE, OP_GT, OP_GTE, OP_LT, OP_LTE, OP_IN, OP_NIN, OP_EXISTS, OP_NOT, OP_REGEX,
       OP_OPTIONS, OP_SIZE, OP_ALL, OP_ELEMMATCH };

static const struct { const char *n; int op; } OPERADORES[] = {
    { "$eq", OP_EQ }, { "$ne", OP_NE }, { "$gt", OP_GT }, { "$gte", OP_GTE }, { "$lt", OP_LT }, { "$lte", OP_LTE },
    { "$in", OP_IN }, { "$nin", OP_NIN }, { "$exists", OP_EXISTS }, { "$not", OP_NOT }, { "$regex", OP_REGEX },
    { "$options", OP_OPTIONS }, { "$size", OP_SIZE }, { "$all", OP_ALL }, { "$elemMatch", OP_ELEMMATCH },
};

static int operador(const char *n)
{
    for (unsigned i = 0; i < sizeof OPERADORES / sizeof OPERADORES[0]; i++)
        if (strcmp(n, OPERADORES[i].n) == 0) return OPERADORES[i].op;
    return 0;
}

/* Un documento de operadores: {"$gt": 1, "$lt": 5}. */
static bool es_doc_operadores(const bson_elem *e)
{
    if (e->tipo != BT_DOC || bson_vacio(e->v)) return false;
    return e->v[5] == '$';   /* el nombre del primer elemento empieza en el byte 5 */
}

/* ------------------------------------------------------------ caché de regex */

#define RXC 8
static struct {
    const char *pat;
    uint32_t    plen;
    const char *opc;
    lmk_regex   rx;
    bool        ok;
} cache_rx[RXC];
static int ncache_rx, prox_rx;

void consulta_olvidar_regex(void) { ncache_rx = 0; prox_rx = 0; }

static const lmk_regex *regex_de(const char *pat, uint32_t plen, const char *opc, lmk_error *err)
{
    if (!opc) opc = "";
    for (int i = 0; i < ncache_rx; i++)
        if (cache_rx[i].pat == pat && cache_rx[i].plen == plen && strcmp(cache_rx[i].opc, opc) == 0)
            return cache_rx[i].ok ? &cache_rx[i].rx : NULL;
    int i = ncache_rx < RXC ? ncache_rx++ : (prox_rx++ % RXC);
    cache_rx[i].pat = pat;
    cache_rx[i].plen = plen;
    cache_rx[i].opc = opc;
    cache_rx[i].ok = regex_compilar(&cache_rx[i].rx, pat, plen, opc, err);
    return cache_rx[i].ok ? &cache_rx[i].rx : NULL;
}

/* La regex de un elemento: BT_REGEX (patrón y opciones) o cadena + $options. */
static const lmk_regex *regex_elem(const bson_elem *e, const char *opciones, lmk_error *err)
{
    if (e->tipo == BT_REGEX) {
        const char *p = (const char *)e->v;
        return regex_de(p, (uint32_t)strlen(p), p + strlen(p) + 1, err);
    }
    if (e->tipo == BT_STRING) return regex_de(bson_cadena(e), bson_cadena_len(e), opciones, err);
    lmk_fallo(err, ERR_VALOR, "$regex tiene que ser una cadena");
    return NULL;
}

/* ------------------------------------------------------------ validación */

static bool validar_filtro(const uint8_t *f, int prof, lmk_error *err);

static bool validar_ops(const bson_elem *cond, int prof, lmk_error *err)
{
    bson_it it;
    bson_elem e, opc;
    bson_it_iniciar(&it, cond->v);
    const char *opciones = NULL;
    if (bson_buscar(cond->v, "$options", &opc)) {
        if (opc.tipo != BT_STRING) return lmk_fallo(err, ERR_VALOR, "$options tiene que ser una cadena");
        opciones = bson_cadena(&opc);
    }
    while (bson_it_sig(&it, &e)) {
        int op = operador(e.nombre);
        char msg[120];
        if (!op) {
            if (e.nombre[0] != '$') return lmk_fallo(err, ERR_VALOR, "no se pueden mezclar operadores y campos en una condición");
            lmk_fmt(msg, sizeof msg, "operador desconocido: %s", e.nombre);
            return lmk_fallo(err, ERR_VALOR, msg);
        }
        switch (op) {
        case OP_IN: case OP_NIN: case OP_ALL:
            if (e.tipo != BT_ARRAY) {
                lmk_fmt(msg, sizeof msg, "%s necesita un array", e.nombre);
                return lmk_fallo(err, ERR_VALOR, msg);
            }
            {
                bson_it ia;
                bson_elem x;
                bson_it_iniciar(&ia, e.v);
                while (bson_it_sig(&ia, &x)) {
                    if (x.tipo == BT_REGEX && !regex_elem(&x, NULL, err)) return false;
                    if (es_doc_operadores(&x) && op != OP_ALL) return lmk_fallo(err, ERR_VALOR, "no se puede usar un operador dentro de $in");
                }
            }
            break;
        case OP_REGEX:
            if (!regex_elem(&e, opciones, err)) return false;
            break;
        case OP_OPTIONS:
            if (!bson_buscar(cond->v, "$regex", &opc)) return lmk_fallo(err, ERR_VALOR, "$options sin $regex");
            break;
        case OP_NOT:
            if (e.tipo == BT_REGEX) { if (!regex_elem(&e, NULL, err)) return false; break; }
            if (!es_doc_operadores(&e)) return lmk_fallo(err, ERR_VALOR, "$not necesita una regex o un documento de operadores");
            if (!validar_ops(&e, prof + 1, err)) return false;
            break;
        case OP_SIZE:
            if (!bson_es_numero(e.tipo)) return lmk_fallo(err, ERR_VALOR, "$size necesita un número");
            break;
        case OP_ELEMMATCH:
            if (e.tipo != BT_DOC) return lmk_fallo(err, ERR_VALOR, "$elemMatch necesita un documento");
            if (es_doc_operadores(&e) && !(e.v[5] == '$' && (strcmp((const char *)e.v + 5, "$and") == 0 ||
                                                             strcmp((const char *)e.v + 5, "$or") == 0))) {
                if (!validar_ops(&e, prof + 1, err)) return false;
            } else if (!validar_filtro(e.v, prof + 1, err)) {
                return false;
            }
            break;
        default:
            if (e.tipo == BT_REGEX && op != OP_EQ && op != OP_NE && !regex_elem(&e, NULL, err)) return false;
            break;
        }
    }
    return true;
}

static bool validar_filtro(const uint8_t *f, int prof, lmk_error *err)
{
    if (prof > LMK_PROF_MAX) return lmk_fallo(err, ERR_VALOR, "filtro demasiado anidado");
    bson_it it;
    bson_elem e;
    bson_it_iniciar(&it, f);
    while (bson_it_sig(&it, &e)) {
        if (e.nombre[0] == '$') {
            if (nombre_es(&e, "$and") || nombre_es(&e, "$or") || nombre_es(&e, "$nor")) {
                if (e.tipo != BT_ARRAY || bson_vacio(e.v)) return lmk_fallo(err, ERR_VALOR, "$and/$or/$nor necesitan un array no vacío");
                bson_it ia;
                bson_elem x;
                bson_it_iniciar(&ia, e.v);
                while (bson_it_sig(&ia, &x)) {
                    if (x.tipo != BT_DOC) return lmk_fallo(err, ERR_VALOR, "$and/$or/$nor: cada elemento tiene que ser un documento");
                    if (!validar_filtro(x.v, prof + 1, err)) return false;
                }
            } else if (!nombre_es(&e, "$comment")) {
                char msg[120];
                lmk_fmt(msg, sizeof msg, "operador de nivel superior desconocido: %s", e.nombre);
                return lmk_fallo(err, ERR_VALOR, msg);
            }
            continue;
        }
        if (es_doc_operadores(&e)) {
            if (!validar_ops(&e, prof, err)) return false;
        } else if (e.tipo == BT_REGEX) {
            if (!regex_elem(&e, NULL, err)) return false;
        }
    }
    return true;
}

bool consulta_validar(const uint8_t *filtro, lmk_error *err) { return validar_filtro(filtro, 0, err); }

/* ------------------------------------------------------------ evaluación */

typedef struct {
    int              op;
    const bson_elem *ref;
    const lmk_regex *rx;
    bool             visto;     /* hubo algún valor (el camino existe) */
    bool             visto_no_null;
} ctx_op_t;

static bool igual(const bson_elem *v, const bson_elem *ref)
{
    if (ref->tipo == BT_REGEX) {
        if (v->tipo == BT_REGEX) return elem_comparar(v, ref) == 0;
        if (v->tipo != BT_STRING) return false;
        const lmk_regex *rx = regex_elem(ref, NULL, NULL);
        return rx && regex_buscar(rx, bson_cadena(v), bson_cadena_len(v));
    }
    return bson_rango(v->tipo) == bson_rango(ref->tipo) && elem_comparar(v, ref) == 0;
}

static bool visita_op(void *c, const bson_elem *v, bool de_array)
{
    ctx_op_t *x = c;
    (void)de_array;
    x->visto = true;
    if (v->tipo != BT_NULL) x->visto_no_null = true;
    switch (x->op) {
    case OP_EQ: return igual(v, x->ref);
    case OP_GT: case OP_GTE: case OP_LT: case OP_LTE: {
        if (bson_rango(v->tipo) != bson_rango(x->ref->tipo)) return false;
        int r = elem_comparar(v, x->ref);
        if (x->op == OP_GT) return r > 0;
        if (x->op == OP_GTE) return r >= 0;
        if (x->op == OP_LT) return r < 0;
        return r <= 0;
    }
    case OP_IN: {
        bson_it it;
        bson_elem r;
        bson_it_iniciar(&it, x->ref->v);
        while (bson_it_sig(&it, &r))
            if (igual(v, &r)) return true;
        return false;
    }
    case OP_REGEX:
        if (v->tipo == BT_STRING) return regex_buscar(x->rx, bson_cadena(v), bson_cadena_len(v));
        if (v->tipo == BT_REGEX && x->ref->tipo == BT_REGEX) return elem_comparar(v, x->ref) == 0;
        return false;
    case OP_SIZE:
        if (v->tipo != BT_ARRAY || de_array) return false;
        return (double)bson_contar(v->v) == bson_a_double(x->ref);
    case OP_ELEMMATCH: {
        if (v->tipo != BT_ARRAY || de_array) return false;
        bool ops = es_doc_operadores(x->ref) && strcmp((const char *)x->ref->v + 5, "$and") != 0 &&
                   strcmp((const char *)x->ref->v + 5, "$or") != 0;
        bson_it it;
        bson_elem el;
        bson_it_iniciar(&it, v->v);
        while (bson_it_sig(&it, &el)) {
            if (ops) { if (consulta_valor_cumple(&el, x->ref->v)) return true; }
            else if (el.tipo == BT_DOC && consulta_coincide(el.v, x->ref->v)) return true;
        }
        return false;
    }
    default: return false;
    }
}

/* ¿Algún valor del camino cumple op con ref? (y si hubo valores) */
static bool algun_valor(const uint8_t *doc, const char *c, uint32_t clen, int op, const bson_elem *ref,
                        const lmk_regex *rx, ctx_op_t *x)
{
    x->op = op;
    x->ref = ref;
    x->rx = rx;
    x->visto = x->visto_no_null = false;
    return bson_camino(doc, c, clen, visita_op, x);
}

static bool visita_existe(void *c, const bson_elem *v, bool de_array) { (void)c; (void)v; (void)de_array; return true; }

static bool cumple_igual(const uint8_t *doc, const char *c, uint32_t clen, const bson_elem *ref)
{
    ctx_op_t x;
    if (algun_valor(doc, c, clen, OP_EQ, ref, NULL, &x)) return true;
    return ref->tipo == BT_NULL && !x.visto_no_null;   /* {a: null}: null o que falte */
}

static bool cumple_ops(const uint8_t *doc, const char *c, uint32_t clen, const bson_elem *cond);

static bool cumple_op(const uint8_t *doc, const char *c, uint32_t clen, const bson_elem *e, const uint8_t *cond)
{
    ctx_op_t x;
    int op = operador(e->nombre);
    switch (op) {
    case OP_EQ: return cumple_igual(doc, c, clen, e);
    case OP_NE: return !cumple_igual(doc, c, clen, e);
    case OP_GT: case OP_LT:
        if (e->tipo == BT_NULL) return false;
        return algun_valor(doc, c, clen, op, e, NULL, &x);
    case OP_GTE: case OP_LTE:
        if (e->tipo == BT_NULL) return cumple_igual(doc, c, clen, e);
        return algun_valor(doc, c, clen, op, e, NULL, &x);
    case OP_IN: case OP_NIN: {
        bool r = algun_valor(doc, c, clen, OP_IN, e, NULL, &x);
        if (!r && !x.visto_no_null) {   /* null en la lista: casa con lo que falta */
            bson_it it;
            bson_elem v;
            bson_it_iniciar(&it, e->v);
            while (bson_it_sig(&it, &v))
                if (v.tipo == BT_NULL) { r = true; break; }
        }
        return op == OP_IN ? r : !r;
    }
    case OP_ALL: {
        bson_it it;
        bson_elem v;
        bson_it_iniciar(&it, e->v);
        bool alguno = false;
        while (bson_it_sig(&it, &v)) {
            alguno = true;
            bool ok = es_doc_operadores(&v) && nombre_es(&(bson_elem){ .nombre = (const char *)v.v + 5 }, "$elemMatch")
                          ? cumple_ops(doc, c, clen, &v)
                          : cumple_igual(doc, c, clen, &v);
            if (!ok) return false;
        }
        return alguno;
    }
    case OP_EXISTS: {
        bool hay = bson_camino(doc, c, clen, visita_existe, NULL);
        return hay == bson_verdadero(e);
    }
    case OP_NOT:
        if (e->tipo == BT_REGEX) {
            const lmk_regex *rx = regex_elem(e, NULL, NULL);
            return !(rx && algun_valor(doc, c, clen, OP_REGEX, e, rx, &x));
        }
        return !cumple_ops(doc, c, clen, e);
    case OP_REGEX: {
        bson_elem o;
        const char *opc = bson_buscar(cond, "$options", &o) && o.tipo == BT_STRING ? bson_cadena(&o) : NULL;
        const lmk_regex *rx = regex_elem(e, opc, NULL);
        return rx && algun_valor(doc, c, clen, OP_REGEX, e, rx, &x);
    }
    case OP_OPTIONS: return true;
    case OP_SIZE: case OP_ELEMMATCH: return algun_valor(doc, c, clen, op, e, NULL, &x);
    default: return false;
    }
}

static bool cumple_ops(const uint8_t *doc, const char *c, uint32_t clen, const bson_elem *cond)
{
    bson_it it;
    bson_elem e;
    bson_it_iniciar(&it, cond->v);
    while (bson_it_sig(&it, &e))
        if (!cumple_op(doc, c, clen, &e, cond->v)) return false;
    return true;
}

bool consulta_coincide(const uint8_t *doc, const uint8_t *filtro)
{
    bson_it it;
    bson_elem e;
    bson_it_iniciar(&it, filtro);
    while (bson_it_sig(&it, &e)) {
        if (e.nombre[0] == '$') {
            bson_it ia;
            bson_elem x;
            if (nombre_es(&e, "$and")) {
                bson_it_iniciar(&ia, e.v);
                while (bson_it_sig(&ia, &x))
                    if (!consulta_coincide(doc, x.v)) return false;
            } else if (nombre_es(&e, "$or") || nombre_es(&e, "$nor")) {
                bool alguno = false;
                bson_it_iniciar(&ia, e.v);
                while (bson_it_sig(&ia, &x))
                    if (consulta_coincide(doc, x.v)) { alguno = true; break; }
                if (alguno != (e.nombre[2] == 'r')) return false;   /* $or: 'r'; $nor: 'o' */
            }
            continue;
        }
        bool ok;
        if (es_doc_operadores(&e)) ok = cumple_ops(doc, e.nombre, e.nlen, &e);
        else if (e.tipo == BT_REGEX) {
            ctx_op_t x;
            const lmk_regex *rx = regex_elem(&e, NULL, NULL);
            ok = rx && algun_valor(doc, e.nombre, e.nlen, OP_REGEX, &e, rx, &x);
        } else ok = cumple_igual(doc, e.nombre, e.nlen, &e);
        if (!ok) return false;
    }
    return true;
}

bool consulta_valor_cumple(const bson_elem *valor, const uint8_t *ops)
{
    /* Se envuelve el valor en {"v": valor} para reutilizar los caminos. */
    uint8_t pequeno[256];
    lmk_buf b = { 0 };
    uint8_t *doc;
    uint32_t total = 4 + 1 + 2 + valor->vlen + 1;
    if (total <= sizeof pequeno) {
        doc = pequeno;
        lmk_pon32(doc, total);
        doc[4] = valor->tipo;
        doc[5] = 'v';
        doc[6] = 0;
        memcpy(doc + 7, valor->v, valor->vlen);
        doc[total - 1] = 0;
    } else {
        uint32_t ini = bson_inicio(&b);
        bson_poner_elem(&b, "v", 1, valor);
        bson_fin(&b, ini);
        if (b.sin_memoria) { buf_liberar(&b); return false; }
        doc = b.p;
    }
    bson_elem cond = { .tipo = BT_DOC, .v = ops, .vlen = bson_tam(ops) };
    bool r = cumple_ops(doc, "v", 1, &cond);
    buf_liberar(&b);
    return r;
}

/* ------------------------------------------------------------ proyección */

/* 1 = incluir campos, 0 = excluir, -1 = solo {_id: 0} o vacía */
static int modo_proy(const uint8_t *proy, bool *id)
{
    bson_it it;
    bson_elem e;
    int modo = -1;
    *id = true;
    bson_it_iniciar(&it, proy);
    while (bson_it_sig(&it, &e)) {
        bool v = bson_verdadero(&e);
        if (nombre_es(&e, "_id")) { *id = v; continue; }
        modo = v ? 1 : 0;
    }
    return modo;
}

bool proyeccion_validar(const uint8_t *proy, lmk_error *err)
{
    bson_it it;
    bson_elem e;
    int modo = -1;
    bson_it_iniciar(&it, proy);
    while (bson_it_sig(&it, &e)) {
        if (!bson_es_numero(e.tipo) && e.tipo != BT_BOOL)
            return lmk_fallo(err, ERR_VALOR, "proyección: cada campo vale 1/true (incluir) o 0/false (excluir)");
        if (e.nombre[0] == '$' || !e.nlen) return lmk_fallo(err, ERR_VALOR, "proyección: nombre de campo no válido");
        if (nombre_es(&e, "_id")) continue;
        int m = bson_verdadero(&e) ? 1 : 0;
        if (modo >= 0 && m != modo) return lmk_fallo(err, ERR_VALOR, "proyección: no se pueden mezclar inclusión y exclusión (salvo _id)");
        modo = m;
    }
    return true;
}

/* ¿"camino" (prefijo + nombre) está en la proyección? 2 = exacto, 1 = es prefijo de alguno. */
static int en_proy(const uint8_t *proy, const char *pre, uint32_t plen, const char *nom, uint32_t nlen)
{
    bson_it it;
    bson_elem e;
    int r = 0;
    uint32_t total = plen + (plen ? 1 : 0) + nlen;
    bson_it_iniciar(&it, proy);
    while (bson_it_sig(&it, &e)) {
        if (e.nlen < total) continue;
        if (plen && (memcmp(e.nombre, pre, plen) != 0 || e.nombre[plen] != '.')) continue;
        if (memcmp(e.nombre + plen + (plen ? 1 : 0), nom, nlen) != 0) continue;
        if (e.nlen == total) return 2;
        if (e.nombre[total] == '.') r = 1;
    }
    return r;
}

static void proy_rec(const uint8_t *doc, bool array, const uint8_t *proy, bool incluir, bool id,
                     char *pre, uint32_t plen, lmk_buf *out, int prof)
{
    uint32_t ini = bson_inicio(out);
    bson_it it;
    bson_elem e;
    bson_it_iniciar(&it, doc);
    uint32_t idx = 0;
    char nidx[12];
    while (bson_it_sig(&it, &e)) {
        const char *nombre = e.nombre;
        uint32_t nlen = e.nlen;
        if (array) {   /* los arrays heredan el camino del padre (proyección de "a.b" en [{b}, {b}]) */
            if (e.tipo != BT_DOC && e.tipo != BT_ARRAY) { if (!incluir) { nlen = bson_indice(nidx, idx++); bson_poner_elem(out, nidx, nlen, &e); } continue; }
            nlen = bson_indice(nidx, idx++);
            bson_clave(out, e.tipo, nidx, nlen);
            proy_rec(e.v, e.tipo == BT_ARRAY, proy, incluir, id, pre, plen, out, prof + 1);
            continue;
        }
        if (!plen && nombre_es(&e, "_id")) {
            if (id) bson_poner_elem(out, nombre, nlen, &e);
            continue;
        }
        int r = en_proy(proy, pre, plen, nombre, nlen);
        bool poner;
        if (r == 2) poner = incluir;
        else if (r == 1 && (e.tipo == BT_DOC || e.tipo == BT_ARRAY) && prof < LMK_PROF_MAX && plen + nlen + 2 < 512) {
            bson_clave(out, e.tipo, nombre, nlen);
            uint32_t viejo = plen;
            if (plen) pre[plen++] = '.';
            memcpy(pre + plen, nombre, nlen);
            plen += nlen;
            proy_rec(e.v, e.tipo == BT_ARRAY, proy, incluir, id, pre, plen, out, prof + 1);
            plen = viejo;
            continue;
        } else poner = !incluir;
        if (poner) bson_poner_elem(out, nombre, nlen, &e);
    }
    bson_fin(out, ini);
}

void proyectar(const uint8_t *doc, const uint8_t *proy, lmk_buf *out)
{
    bool id;
    int modo = proy ? modo_proy(proy, &id) : -1;
    if (!proy || (modo == -1 && id)) { buf_poner(out, doc, bson_tam(doc)); return; }
    char pre[512];
    proy_rec(doc, false, proy, modo == 1, id, pre, 0, out, 0);
}

/* ------------------------------------------------------------ orden */

bool orden_validar(const uint8_t *orden, lmk_error *err)
{
    bson_it it;
    bson_elem e;
    bson_it_iniciar(&it, orden);
    while (bson_it_sig(&it, &e)) {
        if (!bson_es_numero(e.tipo)) return lmk_fallo(err, ERR_VALOR, "sort: cada campo vale 1 o -1");
        double d = bson_a_double(&e);
        if (d != 1.0 && d != -1.0) return lmk_fallo(err, ERR_VALOR, "sort: cada campo vale 1 o -1");
        if (!e.nlen || e.nombre[0] == '$') return lmk_fallo(err, ERR_VALOR, "sort: nombre de campo no válido");
    }
    return true;
}

typedef struct {
    bson_elem mejor;
    bool      hay;
    int       dir;
} ctx_orden_t;

static bool visita_orden(void *c, const bson_elem *v, bool de_array)
{
    ctx_orden_t *x = c;
    if (v->tipo == BT_ARRAY && !de_array && !bson_vacio(v->v)) return false;   /* cuentan sus elementos */
    if (!x->hay || elem_comparar(v, &x->mejor) * x->dir < 0) { x->mejor = *v; x->hay = true; }
    return false;
}

void orden_clave(const uint8_t *doc, const uint8_t *orden, lmk_buf *out)
{
    uint32_t ini = bson_inicio(out);
    bson_it it;
    bson_elem e;
    bson_it_iniciar(&it, orden);
    while (bson_it_sig(&it, &e)) {
        ctx_orden_t x = { .hay = false, .dir = bson_a_double(&e) < 0 ? -1 : 1 };
        bson_camino(doc, e.nombre, e.nlen, visita_orden, &x);
        if (x.hay) bson_poner_elem(out, "k", 1, &x.mejor);
        else bson_poner_null(out, "k");
    }
    bson_fin(out, ini);
}

int orden_comparar(const uint8_t *ka, const uint8_t *kb, const uint8_t *orden)
{
    bson_it ia, ib, io;
    bson_elem a, b, o;
    bson_it_iniciar(&ia, ka);
    bson_it_iniciar(&ib, kb);
    bson_it_iniciar(&io, orden);
    while (bson_it_sig(&io, &o) && bson_it_sig(&ia, &a) && bson_it_sig(&ib, &b)) {
        int r = elem_comparar(&a, &b);
        if (r) return bson_a_double(&o) < 0 ? -r : r;
    }
    return 0;
}

/* Mezcla (merge sort), estable. */
void ordenar_u32(uint32_t *v, uint32_t n, int (*cmp)(void *ctx, uint32_t a, uint32_t b), void *ctx)
{
    if (n < 2) return;
    uint32_t *t = lmk_aloca(n * sizeof *t);
    if (!t) {   /* sin memoria: inserción (lenta pero correcta) */
        for (uint32_t i = 1; i < n; i++) {
            uint32_t x = v[i], j = i;
            while (j > 0 && cmp(ctx, v[j - 1], x) > 0) { v[j] = v[j - 1]; j--; }
            v[j] = x;
        }
        return;
    }
    /* tramos cortos por inserción */
    const uint32_t CORTO = 16;
    for (uint32_t ini = 0; ini < n; ini += CORTO) {
        uint32_t fin = LMK_MIN(ini + CORTO, n);
        for (uint32_t i = ini + 1; i < fin; i++) {
            uint32_t x = v[i], j = i;
            while (j > ini && cmp(ctx, v[j - 1], x) > 0) { v[j] = v[j - 1]; j--; }
            v[j] = x;
        }
    }
    uint32_t *a = v, *b = t;
    for (uint32_t ancho = CORTO; ancho < n; ancho *= 2) {
        for (uint32_t ini = 0; ini < n; ini += 2 * ancho) {
            uint32_t m = LMK_MIN(ini + ancho, n), fin = LMK_MIN(ini + 2 * ancho, n);
            uint32_t i = ini, j = m, k = ini;
            while (i < m && j < fin) b[k++] = cmp(ctx, a[i], a[j]) <= 0 ? a[i++] : a[j++];
            while (i < m) b[k++] = a[i++];
            while (j < fin) b[k++] = a[j++];
        }
        uint32_t *x = a; a = b; b = x;
    }
    if (a != v) memcpy(v, a, n * sizeof *v);
    lmk_libera(t);
}
