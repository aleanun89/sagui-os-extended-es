// SPDX-License-Identifier: GPL-2.0-or-later
/* agregar.c - tubería de agregación básica: $match, $project, $addFields
 * ($set), $sort, $skip, $limit, $unwind, $count y $group (con $sum, $avg,
 * $min, $max, $count, $first, $last y $push).
 *
 * Cada etapa recibe una lista de documentos y produce otra. Un $match al
 * principio usa el planificador (y los índices). Las expresiones son
 * caminos ("$precio", "$direccion.ciudad"), literales, {"$literal": v},
 * $add, $subtract, $multiply, $divide, $concat y documentos de expresiones. */
#include "lmk.h"

typedef struct {
    lmk_buf   datos;
    uint32_t *off;
    uint32_t  n, cap;
} lista_t;

static void lista_liberar(lista_t *l)
{
    buf_liberar(&l->datos);
    lmk_libera(l->off);
    memset(l, 0, sizeof *l);
}

static bool lista_reservar(lista_t *l)
{
    if (l->n < l->cap) return true;
    uint32_t cap = l->cap ? l->cap * 2 : 64;
    uint32_t *o = lmk_realoca(l->off, cap * sizeof *o);
    if (!o) return false;
    l->off = o;
    l->cap = cap;
    return true;
}

/* El documento que acaba de escribirse al final de "datos" (desde ini) entra en la lista. */
static bool lista_cerrar(lista_t *l, uint32_t ini)
{
    if (l->datos.sin_memoria || !lista_reservar(l)) return false;
    l->off[l->n++] = ini;
    return true;
}

static bool lista_poner(lista_t *l, const uint8_t *doc)
{
    uint32_t ini = l->datos.n;
    buf_poner(&l->datos, doc, bson_tam(doc));
    return lista_cerrar(l, ini);
}

static inline const uint8_t *lista_doc(const lista_t *l, uint32_t i) { return l->datos.p + l->off[i]; }

/* ------------------------------------------------------------ expresiones */

static const uint8_t NADA[1] = { 0 };

static bool sin_mem(lmk_error *err) { return lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria"); }

/* Copia un valor ya escrito en b (desde ini) a la arena. */
static bool a_arena(lmk_arena *a, const lmk_buf *b, uint32_t ini, bson_elem *out)
{
    uint32_t n = b->n - ini;
    uint8_t *p = arena_aloca(a, n ? n : 1);
    if (!p) return false;
    memcpy(p, b->p + ini, n);
    out->v = p;
    out->vlen = n;
    return true;
}

static bool numero_a_elem(lmk_arena *a, double d, int64_t i, bool entero, bson_elem *out)
{
    uint8_t *p = arena_aloca(a, 8);
    if (!p) return false;
    if (entero && i >= INT32_MIN && i <= INT32_MAX) { lmk_pon32(p, (uint32_t)i); out->tipo = BT_INT32; out->vlen = 4; }
    else if (entero) { lmk_pon64(p, (uint64_t)i); out->tipo = BT_INT64; out->vlen = 8; }
    else { uint64_t u; memcpy(&u, &d, 8); lmk_pon64(p, u); out->tipo = BT_DOUBLE; out->vlen = 8; }
    out->v = p;
    return true;
}

/* Evalúa expr sobre doc. false = el valor no existe (camino ausente). */
static bool evaluar(lmk_arena *a, const uint8_t *doc, const bson_elem *expr, bson_elem *out, lmk_error *err, int prof);

static bool evaluar_op(lmk_arena *a, const uint8_t *doc, const bson_elem *op, bson_elem *out, lmk_error *err, int prof)
{
    if (nombre_es(op, "$literal")) { *out = *op; return true; }
    bool suma = nombre_es(op, "$add"), resta = nombre_es(op, "$subtract"), mul = nombre_es(op, "$multiply"),
         div = nombre_es(op, "$divide"), concat = nombre_es(op, "$concat");
    if (!suma && !resta && !mul && !div && !concat) {
        char msg[120];
        lmk_fmt(msg, sizeof msg, "expresión desconocida: %s", op->nombre);
        lmk_fallo(err, ERR_VALOR, msg);
        return false;
    }
    if (op->tipo != BT_ARRAY) { lmk_fallo(err, ERR_VALOR, "los operadores de expresión necesitan un array de argumentos"); return false; }
    bson_it it;
    bson_elem arg, v;
    bson_it_iniciar(&it, op->v);
    if (concat) {
        lmk_buf b = { 0 };
        buf_u32(&b, 0);
        while (bson_it_sig(&it, &arg)) {
            if (!evaluar(a, doc, &arg, &v, err, prof + 1) || v.tipo != BT_STRING) {   /* null o ausente: null */
                buf_liberar(&b);
                out->tipo = BT_NULL; out->v = NADA; out->vlen = 0;
                return err->codigo == 0;
            }
            buf_poner(&b, bson_cadena(&v), bson_cadena_len(&v));
        }
        buf_byte(&b, 0);
        if (b.sin_memoria) { buf_liberar(&b); sin_mem(err); return false; }
        lmk_pon32(b.p, b.n - 4);
        out->tipo = BT_STRING;
        bool ok = a_arena(a, &b, 0, out);
        buf_liberar(&b);
        if (!ok) sin_mem(err);
        return ok;
    }
    double d = mul ? 1.0 : 0.0;
    int64_t i = mul ? 1 : 0;
    bool entero = !div, primero = true;
    uint32_t nargs = 0;
    while (bson_it_sig(&it, &arg)) {
        nargs++;
        if (!evaluar(a, doc, &arg, &v, err, prof + 1) || !bson_es_numero(v.tipo)) {
            if (err->codigo) return false;
            out->tipo = BT_NULL; out->v = NADA; out->vlen = 0;
            return true;
        }
        double x = bson_a_double(&v);
        int64_t xi = bson_a_int64(&v);
        if (v.tipo == BT_DOUBLE) entero = false;
        if (primero) { d = x; i = xi; primero = false; continue; }
        if (suma) { d += x; i += xi; }
        else if (resta) { d -= x; i -= xi; }
        else if (mul) { d *= x; i *= xi; }
        else {
            if (x == 0.0) { lmk_fallo(err, ERR_VALOR, "$divide: división por cero"); return false; }
            d /= x;
        }
    }
    if ((resta || div) && nargs != 2) { lmk_fallo(err, ERR_VALOR, "$subtract y $divide necesitan 2 argumentos"); return false; }
    if (!numero_a_elem(a, d, i, entero, out)) { sin_mem(err); return false; }
    return true;
}

static bool evaluar(lmk_arena *a, const uint8_t *doc, const bson_elem *expr, bson_elem *out, lmk_error *err, int prof)
{
    if (prof > LMK_PROF_MAX) { lmk_fallo(err, ERR_VALOR, "expresión demasiado anidada"); return false; }
    out->nombre = "";
    out->nlen = 0;
    if (expr->tipo == BT_STRING && bson_cadena_len(expr) > 1 && bson_cadena(expr)[0] == '$') {
        return bson_camino_simple(doc, bson_cadena(expr) + 1, bson_cadena_len(expr) - 1, out);
    }
    if (expr->tipo == BT_DOC && !bson_vacio(expr->v) && expr->v[5] == '$') {
        bson_it it;
        bson_elem op;
        bson_it_iniciar(&it, expr->v);
        bson_it_sig(&it, &op);
        return evaluar_op(a, doc, &op, out, err, prof);
    }
    if (expr->tipo == BT_DOC) {   /* documento de expresiones: {"a": "$x", "b": 1} */
        lmk_buf b = { 0 };
        uint32_t ini = bson_inicio(&b);
        bson_it it;
        bson_elem e, v;
        bson_it_iniciar(&it, expr->v);
        while (bson_it_sig(&it, &e)) {
            if (evaluar(a, doc, &e, &v, err, prof + 1)) bson_poner_elem(&b, e.nombre, e.nlen, &v);
            else if (err->codigo) { buf_liberar(&b); return false; }
        }
        bson_fin(&b, ini);
        out->tipo = BT_DOC;
        bool ok = !b.sin_memoria && a_arena(a, &b, 0, out);
        buf_liberar(&b);
        if (!ok) sin_mem(err);
        return ok;
    }
    *out = *expr;
    return true;
}

/* ------------------------------------------------------------ etapas */

static bool etapa_match(lista_t *in, lista_t *out, const uint8_t *filtro, lmk_error *err)
{
    if (!consulta_validar(filtro, err)) return false;
    for (uint32_t i = 0; i < in->n; i++)
        if (consulta_coincide(lista_doc(in, i), filtro) && !lista_poner(out, lista_doc(in, i))) return sin_mem(err);
    return true;
}

/* $project y $addFields. */
static bool etapa_project(lista_t *in, lista_t *out, const uint8_t *spec, bool anadir, lmk_error *err)
{
    /* Separa la proyección simple (0/1) de los campos calculados. */
    lmk_buf simple = { 0 };
    uint32_t ini = bson_inicio(&simple);
    bson_it it;
    bson_elem e;
    bool calculados = false, incluye = false, id_fuera = false;
    bson_it_iniciar(&it, spec);
    while (bson_it_sig(&it, &e)) {
        if (!anadir && (bson_es_numero(e.tipo) || e.tipo == BT_BOOL)) {
            bson_poner_elem(&simple, e.nombre, e.nlen, &e);
            if (nombre_es(&e, "_id")) id_fuera = !bson_verdadero(&e);
            else if (bson_verdadero(&e)) incluye = true;
        } else {
            if (e.nombre[0] == '$' || !e.nlen) { buf_liberar(&simple); return lmk_fallo(err, ERR_VALOR, "nombre de campo no válido en $project"); }
            calculados = true;
        }
    }
    bson_fin(&simple, ini);
    if (simple.sin_memoria) { buf_liberar(&simple); return sin_mem(err); }
    if (!anadir && !proyeccion_validar(simple.p, err)) { buf_liberar(&simple); return false; }
    bool solo_id = !anadir && calculados && !incluye;   /* solo calculados: modo inclusión, con el _id */
    lmk_buf tmp = { 0 };
    bool ok = true;
    for (uint32_t i = 0; i < in->n && ok; i++) {
        const uint8_t *doc = lista_doc(in, i);
        tmp.n = 0;
        if (anadir) buf_poner(&tmp, doc, bson_tam(doc));
        else if (solo_id) {
            uint32_t k = bson_inicio(&tmp);
            bson_elem id;
            if (!id_fuera && bson_buscar(doc, "_id", &id)) bson_poner_elem(&tmp, "_id", 3, &id);
            bson_fin(&tmp, k);
        } else proyectar(doc, simple.p, &tmp);
        if (tmp.sin_memoria) { ok = sin_mem(err); break; }
        uint32_t ini_doc = out->datos.n;
        uint32_t k = bson_inicio(&out->datos);
        lmk_arena a = { 0 };
        /* los campos de la base que no se recalculan */
        bson_it ib;
        bson_elem b, v;
        bson_it_iniciar(&ib, tmp.p);
        while (bson_it_sig(&ib, &b)) {
            bool recalculado = false;
            if (calculados) {
                bson_elem s;
                recalculado = bson_buscar(spec, b.nombre, &s) && (anadir || !(bson_es_numero(s.tipo) || s.tipo == BT_BOOL));
            }
            if (!recalculado) bson_poner_elem(&out->datos, b.nombre, b.nlen, &b);
        }
        if (calculados) {
            bson_it_iniciar(&it, spec);
            while (bson_it_sig(&it, &e)) {
                if (!anadir && (bson_es_numero(e.tipo) || e.tipo == BT_BOOL)) continue;
                lmk_error e2 = { 0 };
                if (evaluar(&a, doc, &e, &v, &e2, 0)) bson_poner_elem(&out->datos, e.nombre, e.nlen, &v);
                else if (e2.codigo) { *err = e2; ok = false; break; }
            }
        }
        arena_liberar(&a);
        bson_fin(&out->datos, k);
        if (ok && !lista_cerrar(out, ini_doc)) ok = sin_mem(err);
    }
    buf_liberar(&tmp);
    buf_liberar(&simple);
    return ok;
}

typedef struct { lista_t *l; lmk_buf *claves; uint32_t *koff; const uint8_t *orden; } ctx_sort_t;

static int cmp_sort(void *x, uint32_t a, uint32_t b)
{
    ctx_sort_t *c = x;
    return orden_comparar(c->claves->p + c->koff[a], c->claves->p + c->koff[b], c->orden);
}

static bool etapa_sort(lista_t *in, lista_t *out, const uint8_t *orden, lmk_error *err)
{
    if (!orden_validar(orden, err)) return false;
    if (bson_vacio(orden)) return lmk_fallo(err, ERR_VALOR, "$sort necesita al menos un campo");
    lmk_buf claves = { 0 };
    uint32_t *koff = lmk_aloca((in->n + 1) * sizeof *koff), *idx = lmk_aloca((in->n + 1) * sizeof *idx);
    bool ok = koff && idx;
    for (uint32_t i = 0; ok && i < in->n; i++) {
        koff[i] = claves.n;
        idx[i] = i;
        orden_clave(lista_doc(in, i), orden, &claves);
    }
    if (claves.sin_memoria) ok = false;
    if (ok) {
        ctx_sort_t c = { in, &claves, koff, orden };
        ordenar_u32(idx, in->n, cmp_sort, &c);
        for (uint32_t i = 0; i < in->n && ok; i++) ok = lista_poner(out, lista_doc(in, idx[i]));
    }
    buf_liberar(&claves);
    lmk_libera(koff);
    lmk_libera(idx);
    return ok || sin_mem(err);
}

static bool etapa_unwind(lista_t *in, lista_t *out, const bson_elem *spec, lmk_error *err)
{
    const char *camino = NULL;
    uint32_t clen = 0;
    bool preservar = false;
    if (spec->tipo == BT_STRING) { camino = bson_cadena(spec); clen = bson_cadena_len(spec); }
    else if (spec->tipo == BT_DOC) {
        bson_elem p, pr;
        if (bson_buscar(spec->v, "path", &p) && p.tipo == BT_STRING) { camino = bson_cadena(&p); clen = bson_cadena_len(&p); }
        preservar = bson_buscar(spec->v, "preserveNullAndEmptyArrays", &pr) && bson_verdadero(&pr);
    }
    if (!camino || clen < 2 || camino[0] != '$') return lmk_fallo(err, ERR_VALOR, "$unwind necesita un camino \"$campo\"");
    camino++;
    clen--;
    if (memchr(camino, '.', clen)) return lmk_fallo(err, ERR_VALOR, "$unwind: en lite-monkey solo campos de primer nivel");
    for (uint32_t i = 0; i < in->n; i++) {
        const uint8_t *doc = lista_doc(in, i);
        bson_elem v;
        bool hay = bson_camino_simple(doc, camino, clen, &v);
        if (!hay || v.tipo != BT_ARRAY || bson_vacio(v.v)) {
            if (hay && v.tipo != BT_ARRAY && v.tipo != BT_NULL) { if (!lista_poner(out, doc)) return sin_mem(err); }
            else if (preservar && !lista_poner(out, doc)) return sin_mem(err);
            continue;
        }
        bson_it ia;
        bson_elem x;
        bson_it_iniciar(&ia, v.v);
        while (bson_it_sig(&ia, &x)) {
            /* doc es parte de in->datos, que no cambia mientras se escribe en out */
            uint32_t ini = out->datos.n;
            uint32_t k = bson_inicio(&out->datos);
            bson_it it;
            bson_elem e;
            bson_it_iniciar(&it, doc);
            while (bson_it_sig(&it, &e)) {
                if (e.nlen == clen && memcmp(e.nombre, camino, clen) == 0) bson_poner_elem(&out->datos, e.nombre, e.nlen, &x);
                else bson_poner_elem(&out->datos, e.nombre, e.nlen, &e);
            }
            bson_fin(&out->datos, k);
            if (!lista_cerrar(out, ini)) return sin_mem(err);
        }
    }
    return true;
}

/* ------------------------------------------------------------ $group */

enum { A_SUM = 1, A_AVG, A_MIN, A_MAX, A_COUNT, A_FIRST, A_LAST, A_PUSH };

typedef struct {
    int       op;
    double    suma;
    int64_t   isuma;
    bool      entero;
    uint32_t  cuenta;
    bool      hay;
    bson_elem val;
    lmk_buf   lista;      /* $push: elementos ya serializados */
    uint32_t  nlista;
} acum_t;

typedef struct {
    bson_elem clave;
    uint32_t  hash;
    acum_t   *acc;
} grupo_t;

static int acumulador(const char *n)
{
    static const char *nombres[] = { "$sum", "$avg", "$min", "$max", "$count", "$first", "$last", "$push" };
    for (int i = 0; i < 8; i++)
        if (strcmp(n, nombres[i]) == 0) return i + 1;
    return 0;
}

static bool copiar_valor(lmk_arena *a, const bson_elem *v, bson_elem *dst)
{
    uint8_t *p = arena_aloca(a, v->vlen ? v->vlen : 1);
    if (!p) return false;
    memcpy(p, v->v, v->vlen);
    *dst = *v;
    dst->v = p;
    dst->nombre = "";
    dst->nlen = 0;
    return true;
}

static bool etapa_group(lista_t *in, lista_t *out, const uint8_t *spec, lmk_error *err)
{
    bson_elem idexpr;
    if (!bson_buscar(spec, "_id", &idexpr)) return lmk_fallo(err, ERR_VALOR, "$group necesita _id");
    /* los acumuladores */
    uint32_t nacc = 0;
    bson_elem campos[32], exprs[32];
    int ops[32];
    bson_it it;
    bson_elem e;
    bson_it_iniciar(&it, spec);
    while (bson_it_sig(&it, &e)) {
        if (nombre_es(&e, "_id")) continue;
        if (nacc >= 32) return lmk_fallo(err, ERR_VALOR, "$group: demasiados campos");
        bson_elem op;
        bson_it io;
        if (e.tipo != BT_DOC || bson_contar(e.v) != 1) return lmk_fallo(err, ERR_VALOR, "$group: cada campo es {\"$acumulador\": expresión}");
        bson_it_iniciar(&io, e.v);
        bson_it_sig(&io, &op);
        int a = acumulador(op.nombre);
        if (!a) {
            char msg[120];
            lmk_fmt(msg, sizeof msg, "$group: acumulador desconocido %s", op.nombre);
            return lmk_fallo(err, ERR_VALOR, msg);
        }
        campos[nacc] = e;
        exprs[nacc] = op;
        ops[nacc] = a;
        nacc++;
    }
    lmk_arena a = { 0 };
    grupo_t *g = NULL;
    uint32_t ng = 0, capg = 0, captab = 0;
    uint32_t *tabla = NULL;   /* índice del grupo + 1 */
    bool ok = true;
    for (uint32_t i = 0; i < in->n && ok; i++) {
        const uint8_t *doc = lista_doc(in, i);
        bson_elem k;
        lmk_error e2 = { 0 };
        if (!evaluar(&a, doc, &idexpr, &k, &e2, 0)) {
            if (e2.codigo) { *err = e2; ok = false; break; }
            k.tipo = BT_NULL; k.v = NADA; k.vlen = 0;
        }
        uint32_t h = bson_hash(k.tipo, k.v, k.vlen);
        /* buscar el grupo */
        int64_t gi = -1;
        if (captab) {
            for (uint32_t j = h & (captab - 1); tabla[j]; j = (j + 1) & (captab - 1)) {
                grupo_t *x = &g[tabla[j] - 1];
                if (x->hash == h && bson_rango(x->clave.tipo) == bson_rango(k.tipo) && elem_comparar(&x->clave, &k) == 0) {
                    gi = tabla[j] - 1;
                    break;
                }
            }
        }
        if (gi < 0) {
            if (ng == capg) {
                uint32_t cap = capg ? capg * 2 : 16;
                grupo_t *ngp = lmk_realoca(g, cap * sizeof *ngp);
                if (!ngp) { ok = sin_mem(err); break; }
                g = ngp;
                capg = cap;
            }
            if ((ng + 1) * 2 > captab) {
                uint32_t cap = captab ? captab * 2 : 64;
                uint32_t *t = lmk_aloca(cap * sizeof *t);
                if (!t) { ok = sin_mem(err); break; }
                memset(t, 0, cap * sizeof *t);
                for (uint32_t x = 0; x < ng; x++) {
                    uint32_t j = g[x].hash & (cap - 1);
                    while (t[j]) j = (j + 1) & (cap - 1);
                    t[j] = x + 1;
                }
                lmk_libera(tabla);
                tabla = t;
                captab = cap;
            }
            grupo_t *x = &g[ng];
            memset(x, 0, sizeof *x);
            x->hash = h;
            x->acc = nacc ? arena_aloca(&a, nacc * (uint32_t)sizeof(acum_t)) : NULL;
            if ((nacc && !x->acc) || !copiar_valor(&a, &k, &x->clave)) { ok = sin_mem(err); break; }
            if (nacc) memset(x->acc, 0, nacc * sizeof(acum_t));
            for (uint32_t c = 0; c < nacc; c++) { x->acc[c].op = ops[c]; x->acc[c].entero = true; }
            uint32_t j = h & (captab - 1);
            while (tabla[j]) j = (j + 1) & (captab - 1);
            tabla[j] = ng + 1;
            gi = ng++;
        }
        /* acumular */
        grupo_t *x = &g[gi];
        for (uint32_t c = 0; c < nacc && ok; c++) {
            acum_t *ac = &x->acc[c];
            bson_elem v;
            bool hay = ac->op == A_COUNT ? false : evaluar(&a, doc, &exprs[c], &v, &e2, 0);
            if (e2.codigo) { *err = e2; ok = false; break; }
            switch (ac->op) {
            case A_COUNT: ac->cuenta++; break;
            case A_SUM: case A_AVG:
                if (hay && bson_es_numero(v.tipo)) {
                    ac->suma += bson_a_double(&v);
                    ac->isuma += bson_a_int64(&v);
                    if (v.tipo == BT_DOUBLE) ac->entero = false;
                    ac->cuenta++;
                }
                break;
            case A_MIN: case A_MAX:
                if (hay && v.tipo != BT_NULL) {
                    int r = ac->hay ? elem_comparar(&v, &ac->val) : 0;
                    if (!ac->hay || (ac->op == A_MIN ? r < 0 : r > 0)) {
                        if (!copiar_valor(&a, &v, &ac->val)) ok = sin_mem(err);
                        ac->hay = true;
                    }
                }
                break;
            case A_FIRST: case A_LAST:
                if (ac->op == A_FIRST && ac->hay) break;
                if (!hay) { v.tipo = BT_NULL; v.v = NADA; v.vlen = 0; }
                if (!copiar_valor(&a, &v, &ac->val)) ok = sin_mem(err);
                ac->hay = true;
                break;
            case A_PUSH:
                if (hay) {
                    char t[12];
                    uint32_t l = bson_indice(t, ac->nlista++);
                    bson_poner_elem(&ac->lista, t, l, &v);
                    if (ac->lista.sin_memoria) ok = sin_mem(err);
                }
                break;
            }
        }
    }
    /* resultados */
    for (uint32_t i = 0; i < ng && ok; i++) {
        grupo_t *x = &g[i];
        uint32_t ini = out->datos.n;
        uint32_t k = bson_inicio(&out->datos);
        bson_poner_elem(&out->datos, "_id", 3, &x->clave);
        for (uint32_t c = 0; c < nacc; c++) {
            acum_t *ac = &x->acc[c];
            const char *n = campos[c].nombre;
            switch (ac->op) {
            case A_COUNT: bson_poner_int32(&out->datos, n, (int32_t)ac->cuenta); break;
            case A_SUM:
                if (ac->entero) bson_poner_entero(&out->datos, n, ac->isuma);
                else bson_poner_double(&out->datos, n, ac->suma);
                break;
            case A_AVG:
                if (ac->cuenta) bson_poner_double(&out->datos, n, ac->suma / ac->cuenta);
                else bson_poner_null(&out->datos, n);
                break;
            case A_MIN: case A_MAX: case A_FIRST: case A_LAST:
                if (ac->hay) bson_poner_elem(&out->datos, n, campos[c].nlen, &ac->val);
                else bson_poner_null(&out->datos, n);
                break;
            case A_PUSH: {
                uint32_t s = bson_inicio_sub(&out->datos, BT_ARRAY, n);
                buf_poner(&out->datos, ac->lista.p, ac->lista.n);
                bson_fin(&out->datos, s);
                break;
            }
            }
        }
        bson_fin(&out->datos, k);
        if (!lista_cerrar(out, ini)) ok = sin_mem(err);
    }
    for (uint32_t i = 0; i < ng; i++)
        for (uint32_t c = 0; c < nacc; c++) buf_liberar(&g[i].acc[c].lista);
    lmk_libera(g);
    lmk_libera(tabla);
    arena_liberar(&a);
    return ok;
}

/* ------------------------------------------------------------ la tubería */

bool agregar(lmk_coleccion *c, const uint8_t *tuberia, lmk_buf *out, uint32_t *ndocs, lmk_error *err)
{
    lista_t a = { 0 }, b = { 0 };
    bson_it it;
    bson_elem etapa;
    bool ok = true, primera = true;
    *ndocs = 0;
    bson_it_iniciar(&it, tuberia);
    /* entrada: con un $match al principio, por el planificador */
    bson_elem prim;
    bson_it ip = it;
    const uint8_t *filtro = NULL;
    if (bson_it_sig(&ip, &prim) && prim.tipo == BT_DOC && bson_contar(prim.v) == 1 &&
        bson_buscar(prim.v, "$match", &etapa) && etapa.tipo == BT_DOC) {
        filtro = etapa.v;
        it = ip;
    }
    if (c) {
        uint32_t n;
        uint32_t *v = coleccion_buscar(c, filtro, 0, &n, NULL, err);
        if (!v) return false;
        for (uint32_t i = 0; i < n && ok; i++) {
            const uint8_t *d = coleccion_doc(c, v[i]);
            if (d && !lista_poner(&a, d)) ok = sin_mem(err);
        }
        lmk_libera(v);
    }
    while (ok && bson_it_sig(&it, &etapa)) {
        primera = false;
        bson_elem op;
        bson_it io;
        if (etapa.tipo != BT_DOC || bson_contar(etapa.v) != 1) { ok = lmk_fallo(err, ERR_VALOR, "cada etapa es un documento con un solo campo"); break; }
        bson_it_iniciar(&io, etapa.v);
        bson_it_sig(&io, &op);
        b.n = 0;
        b.datos.n = 0;
        if (nombre_es(&op, "$match")) {
            ok = op.tipo == BT_DOC ? etapa_match(&a, &b, op.v, err) : lmk_fallo(err, ERR_VALOR, "$match necesita un documento");
        } else if (nombre_es(&op, "$project") || nombre_es(&op, "$addFields") || nombre_es(&op, "$set")) {
            ok = op.tipo == BT_DOC ? etapa_project(&a, &b, op.v, op.nombre[1] != 'p', err)
                                   : lmk_fallo(err, ERR_VALOR, "$project necesita un documento");
        } else if (nombre_es(&op, "$sort")) {
            ok = op.tipo == BT_DOC ? etapa_sort(&a, &b, op.v, err) : lmk_fallo(err, ERR_VALOR, "$sort necesita un documento");
        } else if (nombre_es(&op, "$skip") || nombre_es(&op, "$limit")) {
            if (!bson_es_numero(op.tipo) || bson_a_int64(&op) < 0) { ok = lmk_fallo(err, ERR_VALOR, "$skip/$limit necesitan un número >= 0"); break; }
            int64_t k = bson_a_int64(&op);
            bool skip = op.nombre[2] == 'k';
            for (uint32_t i = 0; i < a.n && ok; i++) {
                bool poner = skip ? (int64_t)i >= k : (int64_t)i < k;
                if (poner && !lista_poner(&b, lista_doc(&a, i))) ok = sin_mem(err);
            }
        } else if (nombre_es(&op, "$unwind")) {
            ok = etapa_unwind(&a, &b, &op, err);
        } else if (nombre_es(&op, "$count")) {
            if (op.tipo != BT_STRING || !bson_cadena_len(&op) || bson_cadena(&op)[0] == '$') {
                ok = lmk_fallo(err, ERR_VALOR, "$count necesita el nombre del campo");
                break;
            }
            if (a.n) {
                uint32_t ini = b.datos.n;
                uint32_t k = bson_inicio(&b.datos);
                bson_poner_int32(&b.datos, bson_cadena(&op), (int32_t)a.n);
                bson_fin(&b.datos, k);
                if (!lista_cerrar(&b, ini)) ok = sin_mem(err);
            }
        } else if (nombre_es(&op, "$group")) {
            ok = op.tipo == BT_DOC ? etapa_group(&a, &b, op.v, err) : lmk_fallo(err, ERR_VALOR, "$group necesita un documento");
        } else {
            char msg[120];
            lmk_fmt(msg, sizeof msg, "etapa de agregación desconocida: %s", op.nombre);
            ok = lmk_fallo(err, ERR_VALOR, msg);
        }
        lista_t t = a; a = b; b = t;
    }
    (void)primera;
    if (ok) {
        for (uint32_t i = 0; i < a.n; i++) buf_poner(out, lista_doc(&a, i), bson_tam(lista_doc(&a, i)));
        if (out->sin_memoria) ok = sin_mem(err);
        *ndocs = a.n;
    }
    lista_liberar(&a);
    lista_liberar(&b);
    return ok;
}
