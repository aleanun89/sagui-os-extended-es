// SPDX-License-Identifier: GPL-2.0-or-later
/* servidor.c - las órdenes del protocolo: cada petición es un documento BSON
 * al estilo de MongoDB ({"find": "productos", "$db": "tienda", ...}) y la
 * respuesta otro ({"ok": 1, ...} o {"ok": 0, "errmsg": ..., "code": N}).
 * Aquí están también los cursores y la autenticación (desafío y respuesta
 * con HMAC-SHA256). La red y las tramas están en principal.c. */
#include "lmk.h"
#include "../plataforma.h"
#include "sha256.h"

#define CURSORES_MAX     1024
#define CURSOR_CADUCA_MS (10u * 60u * 1000u)
#define LOTE_DEFECTO     101

typedef struct {
    int64_t        id;
    uint64_t       uso;
    char           base[LMK_NOMBRE_MAX + 1], col[LMK_NOMBRE_MAX + 1];
    /* de find: ranuras pendientes + proyección */
    lmk_coleccion *c;
    uint32_t       generacion;
    uint32_t      *ranuras;
    uint32_t      *gens;          /* lmk_ranura.gen al buscar: una ranura borrada y reutilizada
                                     entre lotes ya es otro documento y se salta */
    uint32_t       n, pos;
    lmk_buf        proy;
    /* de aggregate/listados: documentos ya hechos */
    bool           lista;
    lmk_buf        docs;
    uint32_t       off;
} cursor_t;

struct lmk_servidor {
    lmk_motor *m;
    char       usuario[64];
    uint8_t    clave[128];
    uint32_t   nclave;
    bool       con_auth;
    cursor_t  *cur[CURSORES_MAX];
    uint32_t   ncur;
    uint64_t   inicio;
    uint64_t   ordenes;
};

static const uint8_t VACIO[5] = { 5, 0, 0, 0, 0 };

static const char *ORDENES[] = {
    "hello", "isMaster", "ismaster", "ping", "insert", "find", "getMore", "killCursors", "update", "delete", "count",
    "distinct", "aggregate", "createIndexes", "createIndex", "dropIndexes", "dropIndex", "listIndexes",
    "listCollections", "listDatabases", "create", "drop", "dropDatabase", "collStats", "stats", "dbStats", "compact",
    "explain", "serverStatus", "authStart", "authFinish", "shutdown", NULL,
};

/* ------------------------------------------------------------ HMAC-SHA256 (RFC 2104) */

void lmk_hmac_sha256(const uint8_t *clave, uint32_t nclave, const uint8_t *msg, uint32_t nmsg, uint8_t out[32])
{
    uint8_t k[SHA256_BLOCO], ipad[SHA256_BLOCO], opad[SHA256_BLOCO], interno[SHA256_TAM];
    memset(k, 0, sizeof k);
    if (nclave > SHA256_BLOCO) sha256(clave, nclave, k);
    else memcpy(k, clave, nclave);
    for (int i = 0; i < SHA256_BLOCO; i++) { ipad[i] = k[i] ^ 0x36; opad[i] = k[i] ^ 0x5c; }
    sha256_t c;
    sha256_iniciar(&c);
    sha256_somar(&c, ipad, SHA256_BLOCO);
    sha256_somar(&c, msg, nmsg);
    sha256_fim(&c, interno);
    sha256_iniciar(&c);
    sha256_somar(&c, opad, SHA256_BLOCO);
    sha256_somar(&c, interno, SHA256_TAM);
    sha256_fim(&c, out);
}

/* ------------------------------------------------------------ servidor */

lmk_servidor *servidor_nuevo(lmk_motor *m, const char *usuario, const char *clave)
{
    lmk_servidor *s = lmk_aloca(sizeof *s);
    if (!s) return NULL;
    memset(s, 0, sizeof *s);
    s->m = m;
    s->inicio = plat_ms();
    if (usuario && clave && *usuario) {
        lmk_fmt(s->usuario, sizeof s->usuario, "%s", usuario);
        s->nclave = (uint32_t)LMK_MIN(strlen(clave), sizeof s->clave);
        memcpy(s->clave, clave, s->nclave);
        s->con_auth = true;
    }
    return s;
}

static void cursor_liberar(cursor_t *c)
{
    lmk_libera(c->ranuras);
    lmk_libera(c->gens);
    buf_liberar(&c->proy);
    buf_liberar(&c->docs);
    lmk_libera(c);
}

static void cursor_quitar(lmk_servidor *s, uint32_t i)
{
    cursor_liberar(s->cur[i]);
    s->cur[i] = s->cur[--s->ncur];
}

void servidor_liberar(lmk_servidor *s)
{
    while (s->ncur) cursor_quitar(s, s->ncur - 1);
    lmk_libera(s);
}

void servidor_tic(lmk_servidor *s)
{
    uint64_t ahora = plat_ms();
    for (uint32_t i = 0; i < s->ncur;) {
        if (ahora - s->cur[i]->uso > CURSOR_CADUCA_MS) cursor_quitar(s, i);
        else i++;
    }
    motor_sincronizar(s->m, true);
}

/* ------------------------------------------------------------ respuestas */

static void resp_error(lmk_buf *r, const lmk_error *e)
{
    r->n = 0;
    r->sin_memoria = false;
    uint32_t ini = bson_inicio(r);
    bson_poner_double(r, "ok", 0.0);
    bson_poner_texto(r, "errmsg", e->msg);
    bson_poner_int32(r, "code", e->codigo ? e->codigo : ERR_INTERNO);
    const char *nombre = e->codigo == ERR_DUPLICADO ? "DuplicateKey" : e->codigo == ERR_ORDEN ? "CommandNotFound" :
                         e->codigo == ERR_NO_AUTORIZADO ? "Unauthorized" : e->codigo == ERR_AUTENTICACION ? "AuthenticationFailed" :
                         e->codigo == ERR_CURSOR ? "CursorNotFound" : e->codigo == ERR_NS_NO_EXISTE ? "NamespaceNotFound" :
                         e->codigo == ERR_VALOR ? "BadValue" : e->codigo == ERR_FORMATO ? "InvalidBSON" : "Error";
    bson_poner_texto(r, "codeName", nombre);
    bson_fin(r, ini);
}

static bool error(lmk_buf *r, int codigo, const char *msg)
{
    lmk_error e;
    lmk_fallo(&e, codigo, msg);
    resp_error(r, &e);
    return true;
}

/* ------------------------------------------------------------ lectura de opciones */

typedef struct {
    const uint8_t *orden;
    bson_elem      nombre;    /* el primer elemento: {"find": "col"} */
    const char    *base;
    char           ns[2 * LMK_NOMBRE_MAX + 2];
} pet_t;

static bool opt_doc(const uint8_t *o, const char *n, const uint8_t **v, lmk_error *err)
{
    bson_elem e;
    *v = NULL;
    if (!bson_buscar(o, n, &e) || e.tipo == BT_NULL) return true;
    if (e.tipo != BT_DOC) {
        char msg[100];
        lmk_fmt(msg, sizeof msg, "'%s' tiene que ser un documento", n);
        return lmk_fallo(err, ERR_VALOR, msg);
    }
    *v = e.v;
    return true;
}

static int64_t opt_num(const uint8_t *o, const char *n, int64_t defecto)
{
    bson_elem e;
    if (!bson_buscar(o, n, &e) || !bson_es_numero(e.tipo)) return defecto;
    return bson_a_int64(&e);
}

static bool opt_bool(const uint8_t *o, const char *n, bool defecto)
{
    bson_elem e;
    if (!bson_buscar(o, n, &e)) return defecto;
    return bson_verdadero(&e);
}

/* La colección del primer campo de la orden; crear: la crea si falta. NULL y
 * err->codigo == 0: no existe (y no hacía falta crearla). */
static lmk_coleccion *coleccion_de(lmk_servidor *s, pet_t *p, bool crear, lmk_error *err)
{
    err->codigo = 0;
    if (p->nombre.tipo != BT_STRING) { lmk_fallo(err, ERR_VALOR, "falta el nombre de la colección"); return NULL; }
    const char *col = bson_cadena(&p->nombre);
    if (!nombre_valido(p->base, false)) { lmk_fallo(err, ERR_VALOR, "nombre de base no válido"); return NULL; }
    if (!nombre_valido(col, true)) { lmk_fallo(err, ERR_VALOR, "nombre de colección no válido"); return NULL; }
    lmk_base *b = motor_base(s->m, p->base, crear, err);
    if (!b) { if (!crear && err->codigo == ERR_NS_NO_EXISTE) err->codigo = 0; return NULL; }
    lmk_coleccion *c = base_coleccion(b, col, crear, err);
    if (!c && !crear && err->codigo == ERR_NS_NO_EXISTE) err->codigo = 0;
    return c;
}

/* ------------------------------------------------------------ cursores */

static cursor_t *cursor_nuevo(lmk_servidor *s, const pet_t *p)
{
    if (s->ncur >= CURSORES_MAX) {   /* el más viejo se va */
        uint32_t viejo = 0;
        for (uint32_t i = 1; i < s->ncur; i++)
            if (s->cur[i]->uso < s->cur[viejo]->uso) viejo = i;
        cursor_quitar(s, viejo);
    }
    cursor_t *c = lmk_aloca(sizeof *c);
    if (!c) return NULL;
    memset(c, 0, sizeof *c);
    do {
        plat_azar(&c->id, sizeof c->id);
        c->id &= INT64_MAX;
    } while (c->id == 0);
    c->uso = plat_ms();
    lmk_fmt(c->base, sizeof c->base, "%s", p->base);
    if (p->nombre.tipo == BT_STRING) lmk_fmt(c->col, sizeof c->col, "%s", bson_cadena(&p->nombre));
    s->cur[s->ncur++] = c;
    return c;
}

static int cursor_indice(lmk_servidor *s, int64_t id)
{
    for (uint32_t i = 0; i < s->ncur; i++)
        if (s->cur[i]->id == id) return (int)i;
    return -1;
}

/* Escribe un lote ("firstBatch" o "nextBatch") y el id (0 = acabó). */
static void escribir_lote(lmk_servidor *s, cursor_t *cur, lmk_buf *r, const char *campo, int64_t lote, const char *ns)
{
    uint32_t ci = bson_inicio_sub(r, BT_DOC, "cursor");
    uint32_t ai = bson_inicio_sub(r, BT_ARRAY, campo);
    uint32_t base_n = r->n, k = 0;
    char idx[12];
    bool acabo = true;
    if (cur && cur->lista) {
        while (cur->off < cur->docs.n) {
            if ((lote > 0 && k >= (uint64_t)lote) || r->n - base_n > LMK_LOTE_BYTES) { acabo = false; break; }
            const uint8_t *d = cur->docs.p + cur->off;
            uint32_t l = bson_indice(idx, k++);
            bson_clave(r, BT_DOC, idx, l);
            buf_poner(r, d, bson_tam(d));
            cur->off += bson_tam(d);
        }
    } else if (cur) {
        /* la colección pudo borrarse entre lotes */
        bool valida = cur->c && cur->c->generacion == cur->generacion;
        if (valida) {
            lmk_error e;
            lmk_base *b = motor_base(s->m, cur->base, false, &e);
            lmk_coleccion *c = b ? base_coleccion(b, cur->col, false, &e) : NULL;
            valida = c == cur->c;
        }
        while (valida && cur->pos < cur->n) {
            if ((lote > 0 && k >= (uint64_t)lote) || r->n - base_n > LMK_LOTE_BYTES) { acabo = false; break; }
            uint32_t k2 = cur->pos++, ran = cur->ranuras[k2];
            const uint8_t *d = ran < cur->c->nr && cur->c->r[ran].gen == cur->gens[k2] ? coleccion_doc(cur->c, ran) : NULL;
            if (!d) continue;   /* borrado (o borrado y la ranura reutilizada) entre lotes */
            uint32_t l = bson_indice(idx, k++);
            bson_clave(r, BT_DOC, idx, l);
            proyectar(d, cur->proy.n ? cur->proy.p : NULL, r);
        }
    }
    bson_fin(r, ai);
    if (cur && !acabo) cur->uso = plat_ms();
    int64_t id = cur && !acabo ? cur->id : 0;
    bson_poner_int64(r, "id", id);
    bson_poner_texto(r, "ns", ns);
    bson_fin(r, ci);
    if (cur && acabo) {
        int i = cursor_indice(s, cur->id);
        if (i >= 0) cursor_quitar(s, (uint32_t)i);
    }
}

/* Un cursor de lista (agregación, listados): docs ya escritos uno tras otro. */
static bool responder_lista(lmk_servidor *s, pet_t *p, lmk_buf *docs, int64_t lote, lmk_buf *r)
{
    cursor_t *cur = cursor_nuevo(s, p);
    if (!cur) { buf_liberar(docs); return error(r, ERR_SIN_MEMORIA, "sin memoria"); }
    cur->lista = true;
    cur->docs = *docs;
    memset(docs, 0, sizeof *docs);
    uint32_t ini = bson_inicio(r);
    escribir_lote(s, cur, r, "firstBatch", lote, p->ns);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

/* ------------------------------------------------------------ find / count / distinct */

typedef struct { lmk_coleccion *c; lmk_buf *claves; uint32_t *koff; const uint8_t *orden; } ctx_orden_t;

static int cmp_ranuras(void *x, uint32_t a, uint32_t b)
{
    ctx_orden_t *c = x;
    return orden_comparar(c->claves->p + c->koff[a], c->claves->p + c->koff[b], c->orden);
}

/* Ordena las ranuras v[0..n) según "orden". */
static bool ordenar_ranuras(lmk_coleccion *c, uint32_t *v, uint32_t n, const uint8_t *orden)
{
    lmk_buf claves = { 0 };
    uint32_t *koff = lmk_aloca((n + 1) * sizeof *koff), *idx = lmk_aloca((n + 1) * sizeof *idx);
    bool ok = koff && idx;
    for (uint32_t i = 0; ok && i < n; i++) {
        koff[i] = claves.n;
        idx[i] = i;
        const uint8_t *d = coleccion_doc(c, v[i]);
        if (d) orden_clave(d, orden, &claves);
        else { uint32_t k = bson_inicio(&claves); bson_fin(&claves, k); }
    }
    if (claves.sin_memoria) ok = false;
    if (ok) {
        ctx_orden_t x = { c, &claves, koff, orden };
        ordenar_u32(idx, n, cmp_ranuras, &x);
        for (uint32_t i = 0; i < n; i++) koff[i] = v[idx[i]];   /* koff se reutiliza como copia */
        memcpy(v, koff, n * sizeof *v);
    }
    buf_liberar(&claves);
    lmk_libera(koff);
    lmk_libera(idx);
    return ok;
}

/* Ejecuta la búsqueda de find/count con skip y limit. */
static uint32_t *buscar(lmk_coleccion *c, const uint8_t *filtro, const uint8_t *orden, int64_t skip, int64_t limite,
                        uint32_t *n, lmk_plan *plan, lmk_error *err)
{
    if (skip < 0) { lmk_fallo(err, ERR_VALOR, "skip no puede ser negativo"); return NULL; }
    if (orden && !orden_validar(orden, err)) return NULL;
    bool ordenar = orden && !bson_vacio(orden);
    uint32_t max = 0;
    if (!ordenar && limite > 0 && skip + limite < 0x7FFFFFFF) max = (uint32_t)(skip + limite);
    uint32_t *v = coleccion_buscar(c, filtro, max, n, plan, err);
    if (!v) return NULL;
    if (ordenar && !ordenar_ranuras(c, v, *n, orden)) {
        lmk_libera(v);
        lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria para ordenar");
        return NULL;
    }
    uint32_t desde = (uint64_t)skip >= *n ? *n : (uint32_t)skip;
    uint32_t total = *n - desde;
    if (limite > 0 && (uint64_t)limite < total) total = (uint32_t)limite;
    if (desde) memmove(v, v + desde, total * sizeof *v);
    *n = total;
    return v;
}

static bool orden_find(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    const uint8_t *filtro, *proy, *orden;
    if (!opt_doc(p->orden, "filter", &filtro, &err) || !opt_doc(p->orden, "projection", &proy, &err) ||
        !opt_doc(p->orden, "sort", &orden, &err)) { resp_error(r, &err); return true; }
    if (proy && !proyeccion_validar(proy, &err)) { resp_error(r, &err); return true; }
    int64_t skip = opt_num(p->orden, "skip", 0), limite = opt_num(p->orden, "limit", 0);
    int64_t lote = opt_num(p->orden, "batchSize", LOTE_DEFECTO);
    bool unico = opt_bool(p->orden, "singleBatch", false);
    if (limite < 0) { limite = -limite; unico = true; }
    if (lote < 0) return error(r, ERR_VALOR, "batchSize no puede ser negativo");
    if (lote == 0) lote = LOTE_DEFECTO;
    lmk_coleccion *c = coleccion_de(s, p, false, &err);
    if (!c && err.codigo) { resp_error(r, &err); return true; }
    if (!c) {   /* colección que no existe: resultado vacío */
        if (filtro && !consulta_validar(filtro, &err)) { resp_error(r, &err); return true; }
        uint32_t ini = bson_inicio(r);
        escribir_lote(s, NULL, r, "firstBatch", 0, p->ns);
        bson_poner_double(r, "ok", 1.0);
        bson_fin(r, ini);
        return true;
    }
    uint32_t n;
    uint32_t *v = buscar(c, filtro, orden, skip, limite, &n, NULL, &err);
    if (!v) { resp_error(r, &err); return true; }
    cursor_t *cur = cursor_nuevo(s, p);
    if (!cur) { lmk_libera(v); return error(r, ERR_SIN_MEMORIA, "sin memoria"); }
    cur->c = c;
    cur->generacion = c->generacion;
    cur->ranuras = v;
    cur->n = n;
    cur->gens = lmk_aloca((n ? n : 1) * sizeof *cur->gens);
    if (!cur->gens) {
        int i = cursor_indice(s, cur->id);
        if (i >= 0) cursor_quitar(s, (uint32_t)i);
        return error(r, ERR_SIN_MEMORIA, "sin memoria");
    }
    for (uint32_t i = 0; i < n; i++) cur->gens[i] = c->r[v[i]].gen;
    if (proy) buf_poner(&cur->proy, proy, bson_tam(proy));
    uint32_t ini = bson_inicio(r);
    escribir_lote(s, cur, r, "firstBatch", unico ? 0 : lote, p->ns);
    if (unico) {   /* singleBatch: no queda cursor abierto */
        int i = cursor_indice(s, cur->id);
        if (i >= 0) cursor_quitar(s, (uint32_t)i);
    }
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

static bool orden_getmore(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    if (!bson_es_numero(p->nombre.tipo)) return error(r, ERR_VALOR, "getMore necesita el id del cursor");
    int64_t id = bson_a_int64(&p->nombre);
    int i = cursor_indice(s, id);
    if (i < 0) return error(r, ERR_CURSOR, "el cursor no existe (o caducó)");
    cursor_t *cur = s->cur[i];
    int64_t lote = opt_num(p->orden, "batchSize", 0);
    char ns[2 * LMK_NOMBRE_MAX + 2];
    lmk_fmt(ns, sizeof ns, "%s.%s", cur->base, cur->col);
    uint32_t ini = bson_inicio(r);
    escribir_lote(s, cur, r, "nextBatch", lote, ns);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

static bool orden_killcursors(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    bson_elem lista, x;
    uint32_t ini = bson_inicio(r);
    uint32_t ai = bson_inicio_sub(r, BT_ARRAY, "cursorsKilled");
    uint32_t k = 0;
    char idx[12];
    if (bson_buscar(p->orden, "cursors", &lista) && lista.tipo == BT_ARRAY) {
        bson_it it;
        bson_it_iniciar(&it, lista.v);
        while (bson_it_sig(&it, &x)) {
            int i = bson_es_numero(x.tipo) ? cursor_indice(s, bson_a_int64(&x)) : -1;
            if (i < 0) continue;
            cursor_quitar(s, (uint32_t)i);
            bson_poner_elem(r, idx, bson_indice(idx, k++), &x);
        }
    }
    bson_fin(r, ai);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

static bool orden_count(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    const uint8_t *filtro;
    if (!opt_doc(p->orden, "query", &filtro, &err)) { resp_error(r, &err); return true; }
    if (!filtro && !opt_doc(p->orden, "filter", &filtro, &err)) { resp_error(r, &err); return true; }
    lmk_coleccion *c = coleccion_de(s, p, false, &err);
    if (!c && err.codigo) { resp_error(r, &err); return true; }
    uint32_t n = 0;
    if (c) {
        int64_t skip = opt_num(p->orden, "skip", 0), limite = opt_num(p->orden, "limit", 0);
        if (limite < 0) limite = -limite;
        if (!filtro || bson_vacio(filtro)) {   /* atajo: sin filtro, el total */
            n = c->vivas;
            n = skip >= n ? 0 : n - (uint32_t)skip;
            if (limite > 0 && (uint64_t)limite < n) n = (uint32_t)limite;
        } else {
            uint32_t *v = buscar(c, filtro, NULL, skip, limite, &n, NULL, &err);
            if (!v) { resp_error(r, &err); return true; }
            lmk_libera(v);
        }
    }
    uint32_t ini = bson_inicio(r);
    bson_poner_int32(r, "n", (int32_t)n);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

typedef struct {
    lmk_buf   vals;     /* elementos uno tras otro: tipo, longitud (u32), valor */
    uint32_t *off;
    uint32_t  n, cap;
    uint32_t *tabla;
    uint32_t  captab;
    bool      sin_memoria;
} distintos_t;

static void val_de(const distintos_t *d, uint32_t i, bson_elem *e)
{
    const uint8_t *p = d->vals.p + d->off[i];
    e->tipo = p[0];
    e->vlen = lmk_le32(p + 1);
    e->v = p + 5;
}

static bool visita_distinto(void *x, const bson_elem *v, bool de_array)
{
    distintos_t *d = x;
    if (v->tipo == BT_ARRAY && !de_array && !bson_vacio(v->v)) return false;
    uint32_t h = bson_hash(v->tipo, v->v, v->vlen);
    if ((d->n + 1) * 2 > d->captab) {
        uint32_t cap = d->captab ? d->captab * 2 : 64;
        uint32_t *t = lmk_aloca(cap * sizeof *t);
        if (!t) { d->sin_memoria = true; return true; }
        memset(t, 0, cap * sizeof *t);
        for (uint32_t i = 0; i < d->n; i++) {
            bson_elem e;
            val_de(d, i, &e);
            uint32_t j = bson_hash(e.tipo, e.v, e.vlen) & (cap - 1);
            while (t[j]) j = (j + 1) & (cap - 1);
            t[j] = i + 1;
        }
        lmk_libera(d->tabla);
        d->tabla = t;
        d->captab = cap;
    }
    uint32_t j = h & (d->captab - 1);
    for (; d->tabla[j]; j = (j + 1) & (d->captab - 1)) {
        bson_elem e;
        val_de(d, d->tabla[j] - 1, &e);
        if (bson_rango(e.tipo) == bson_rango(v->tipo) && elem_comparar(&e, v) == 0) return false;
    }
    if (d->n == d->cap) {
        uint32_t cap = d->cap ? d->cap * 2 : 64;
        uint32_t *o = lmk_realoca(d->off, cap * sizeof *o);
        if (!o) { d->sin_memoria = true; return true; }
        d->off = o;
        d->cap = cap;
    }
    d->off[d->n] = d->vals.n;
    buf_byte(&d->vals, v->tipo);
    buf_u32(&d->vals, v->vlen);
    buf_poner(&d->vals, v->v, v->vlen);
    if (d->vals.sin_memoria) { d->sin_memoria = true; return true; }
    d->tabla[j] = ++d->n;
    return false;
}

static int cmp_distintos(void *x, uint32_t a, uint32_t b)
{
    distintos_t *d = x;
    bson_elem ea, eb;
    val_de(d, a, &ea);
    val_de(d, b, &eb);
    return elem_comparar(&ea, &eb);
}

static bool orden_distinct(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    const uint8_t *filtro;
    bson_elem clave;
    if (!bson_buscar(p->orden, "key", &clave) || clave.tipo != BT_STRING || !bson_cadena_len(&clave))
        return error(r, ERR_VALOR, "distinct necesita 'key'");
    if (!opt_doc(p->orden, "query", &filtro, &err)) { resp_error(r, &err); return true; }
    lmk_coleccion *c = coleccion_de(s, p, false, &err);
    if (!c && err.codigo) { resp_error(r, &err); return true; }
    distintos_t d = { 0 };
    if (c) {
        uint32_t n;
        uint32_t *v = coleccion_buscar(c, filtro, 0, &n, NULL, &err);
        if (!v) { resp_error(r, &err); return true; }
        for (uint32_t i = 0; i < n && !d.sin_memoria; i++) {
            const uint8_t *doc = coleccion_doc(c, v[i]);
            if (doc) bson_camino(doc, bson_cadena(&clave), bson_cadena_len(&clave), visita_distinto, &d);
        }
        lmk_libera(v);
    }
    uint32_t *idx = d.n ? lmk_aloca(d.n * sizeof *idx) : NULL;
    if (d.sin_memoria || (d.n && !idx)) {
        buf_liberar(&d.vals); lmk_libera(d.off); lmk_libera(d.tabla); lmk_libera(idx);
        return error(r, ERR_SIN_MEMORIA, "sin memoria");
    }
    for (uint32_t i = 0; i < d.n; i++) idx[i] = i;
    ordenar_u32(idx, d.n, cmp_distintos, &d);
    uint32_t ini = bson_inicio(r);
    uint32_t ai = bson_inicio_sub(r, BT_ARRAY, "values");
    char t[12];
    for (uint32_t i = 0; i < d.n; i++) {
        bson_elem e;
        val_de(&d, idx[i], &e);
        bson_poner_elem(r, t, bson_indice(t, i), &e);
    }
    bson_fin(r, ai);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    buf_liberar(&d.vals);
    lmk_libera(d.off);
    lmk_libera(d.tabla);
    lmk_libera(idx);
    return true;
}

/* ------------------------------------------------------------ escrituras */

static void poner_error_escritura(lmk_buf *errores, uint32_t *nerr, uint32_t indice, const lmk_error *e)
{
    char t[12];
    uint32_t ini = bson_inicio_sub(errores, BT_DOC, (bson_indice(t, (*nerr)++), t));
    bson_poner_int32(errores, "index", (int32_t)indice);
    bson_poner_int32(errores, "code", e->codigo);
    bson_poner_texto(errores, "errmsg", e->msg);
    bson_fin(errores, ini);
}

static void cerrar_escritura(lmk_buf *r, uint32_t ini, lmk_buf *errores, uint32_t nerr)
{
    if (nerr) {
        bson_clave(r, BT_ARRAY, "writeErrors", 11);
        buf_u32(r, errores->n + 5);
        buf_poner(r, errores->p, errores->n);
        buf_byte(r, 0);
    }
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
}

static bool orden_insert(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    bson_elem docs;
    if (!bson_buscar(p->orden, "documents", &docs) || docs.tipo != BT_ARRAY)
        return error(r, ERR_VALOR, "insert necesita 'documents' (un array)");
    bool ordenado = opt_bool(p->orden, "ordered", true);
    lmk_coleccion *c = coleccion_de(s, p, true, &err);
    if (!c) { resp_error(r, &err); return true; }
    lmk_buf errores = { 0 };
    uint32_t nerr = 0, n = 0, i = 0;
    bson_it it;
    bson_elem d;
    bson_it_iniciar(&it, docs.v);
    while (bson_it_sig(&it, &d)) {
        if (d.tipo != BT_DOC) lmk_fallo(&err, ERR_VALOR, "cada documento tiene que ser un documento");
        else if (coleccion_insertar(c, d.v, NULL, NULL, &err)) { n++; i++; continue; }
        poner_error_escritura(&errores, &nerr, i++, &err);
        if (ordenado) break;
    }
    uint32_t ini = bson_inicio(r);
    bson_poner_int32(r, "n", (int32_t)n);
    cerrar_escritura(r, ini, &errores, nerr);
    buf_liberar(&errores);
    return true;
}

static bool una_actualizacion(lmk_coleccion **cp, lmk_servidor *s, pet_t *p, const uint8_t *st, uint32_t *n,
                              uint32_t *nmod, lmk_buf *upserts, uint32_t *nups, uint32_t indice, lmk_error *err)
{
    const uint8_t *q, *u;
    bson_elem ue;
    if (!opt_doc(st, "q", &q, err)) return false;
    if (!bson_buscar(st, "u", &ue) || ue.tipo != BT_DOC) return lmk_fallo(err, ERR_VALOR, "cada actualización necesita 'u' (un documento)");
    u = ue.v;
    if (!q) q = VACIO;
    bool multi = opt_bool(st, "multi", false), upsert = opt_bool(st, "upsert", false);
    if (!actualizacion_validar(u, err)) return false;
    bool ops = actualizacion_con_operadores(u);
    if (multi && !ops) return lmk_fallo(err, ERR_VALOR, "multi: true solo con operadores ($set...), no con un reemplazo");
    lmk_coleccion *c = *cp;
    if (!c) {
        c = coleccion_de(s, p, upsert, err);
        if (!c && err->codigo) return false;
        *cp = c;
    }
    uint32_t cuantos = 0;
    uint32_t *v = c ? coleccion_buscar(c, q, multi ? 0 : 1, &cuantos, NULL, err) : NULL;
    if (c && !v) return false;
    if (!c && !consulta_validar(q, err)) return false;
    lmk_buf nuevo = { 0 };
    bool ok = true;
    for (uint32_t i = 0; i < cuantos && ok; i++) {
        const uint8_t *d = coleccion_doc(c, v[i]);
        if (!d) continue;
        nuevo.n = 0;
        bool cambio = true;
        ok = ops ? actualizar_doc(d, u, &nuevo, &cambio, err) : reemplazar_doc(d, u, &nuevo, err);
        if (ok && !ops) cambio = bson_tam(d) != nuevo.n || memcmp(d, nuevo.p, nuevo.n) != 0;
        if (ok && cambio) ok = coleccion_cambiar(c, v[i], nuevo.p, err);
        if (ok) { (*n)++; if (cambio) (*nmod)++; }
    }
    lmk_libera(v);
    if (ok && cuantos == 0 && upsert) {
        nuevo.n = 0;
        ok = upsert_doc(q, u, &nuevo, err);
        uint8_t idt;
        lmk_buf id = { 0 };
        if (ok) ok = bson_validar(nuevo.p, nuevo.n, err) && coleccion_insertar(c, nuevo.p, &idt, &id, err);
        if (ok) {
            char t[12];
            uint32_t k = bson_inicio_sub(upserts, BT_DOC, (bson_indice(t, (*nups)++), t));
            bson_poner_int32(upserts, "index", (int32_t)indice);
            bson_clave(upserts, idt, "_id", 3);
            buf_poner(upserts, id.p, id.n);
            bson_fin(upserts, k);
            (*n)++;
        }
        buf_liberar(&id);
    }
    buf_liberar(&nuevo);
    return ok;
}

static bool orden_update(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    bson_elem lista;
    if (!bson_buscar(p->orden, "updates", &lista) || lista.tipo != BT_ARRAY)
        return error(r, ERR_VALOR, "update necesita 'updates' (un array)");
    bool ordenado = opt_bool(p->orden, "ordered", true);
    lmk_coleccion *c = NULL;
    lmk_buf errores = { 0 }, ups = { 0 };
    uint32_t nerr = 0, n = 0, nmod = 0, nups = 0, i = 0;
    bson_it it;
    bson_elem st;
    bson_it_iniciar(&it, lista.v);
    while (bson_it_sig(&it, &st)) {
        err.codigo = 0;
        bool ok = st.tipo == BT_DOC ? una_actualizacion(&c, s, p, st.v, &n, &nmod, &ups, &nups, i, &err)
                                    : lmk_fallo(&err, ERR_VALOR, "cada actualización tiene que ser un documento");
        if (!ok) {
            poner_error_escritura(&errores, &nerr, i, &err);
            if (ordenado) break;
        }
        i++;
    }
    uint32_t ini = bson_inicio(r);
    bson_poner_int32(r, "n", (int32_t)n);
    bson_poner_int32(r, "nModified", (int32_t)nmod);
    if (nups) {
        bson_clave(r, BT_ARRAY, "upserted", 8);
        buf_u32(r, ups.n + 5);
        buf_poner(r, ups.p, ups.n);
        buf_byte(r, 0);
    }
    cerrar_escritura(r, ini, &errores, nerr);
    buf_liberar(&errores);
    buf_liberar(&ups);
    return true;
}

static bool orden_delete(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    bson_elem lista;
    if (!bson_buscar(p->orden, "deletes", &lista) || lista.tipo != BT_ARRAY)
        return error(r, ERR_VALOR, "delete necesita 'deletes' (un array)");
    bool ordenado = opt_bool(p->orden, "ordered", true);
    lmk_coleccion *c = coleccion_de(s, p, false, &err);
    if (!c && err.codigo) { resp_error(r, &err); return true; }
    lmk_buf errores = { 0 };
    uint32_t nerr = 0, n = 0, i = 0;
    bson_it it;
    bson_elem st;
    bson_it_iniciar(&it, lista.v);
    while (bson_it_sig(&it, &st)) {
        const uint8_t *q = NULL;
        bool ok = st.tipo == BT_DOC && opt_doc(st.v, "q", &q, &err);
        if (st.tipo != BT_DOC) lmk_fallo(&err, ERR_VALOR, "cada borrado tiene que ser un documento");
        if (ok && !q) q = VACIO;
        if (ok && c) {
            int64_t limite = opt_num(st.v, "limit", 0);
            uint32_t cuantos;
            uint32_t *v = coleccion_buscar(c, q, limite == 1 ? 1 : 0, &cuantos, NULL, &err);
            ok = v != NULL;
            for (uint32_t k = 0; ok && k < cuantos; k++) {
                ok = coleccion_quitar(c, v[k], &err);
                if (ok) n++;
            }
            lmk_libera(v);
        } else if (ok) ok = consulta_validar(q, &err);
        if (!ok) {
            poner_error_escritura(&errores, &nerr, i, &err);
            if (ordenado) break;
        }
        i++;
    }
    uint32_t ini = bson_inicio(r);
    bson_poner_int32(r, "n", (int32_t)n);
    cerrar_escritura(r, ini, &errores, nerr);
    buf_liberar(&errores);
    return true;
}

/* ------------------------------------------------------------ agregación y explain */

static bool orden_aggregate(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    bson_elem tub, cur;
    if (!bson_buscar(p->orden, "pipeline", &tub) || tub.tipo != BT_ARRAY)
        return error(r, ERR_VALOR, "aggregate necesita 'pipeline' (un array)");
    int64_t lote = LOTE_DEFECTO;
    if (bson_buscar(p->orden, "cursor", &cur) && cur.tipo == BT_DOC) lote = opt_num(cur.v, "batchSize", LOTE_DEFECTO);
    if (lote <= 0) lote = LOTE_DEFECTO;
    lmk_coleccion *c = coleccion_de(s, p, false, &err);
    if (!c && err.codigo) { resp_error(r, &err); return true; }
    lmk_buf docs = { 0 };
    uint32_t n;
    if (!agregar(c, tub.v, &docs, &n, &err)) { buf_liberar(&docs); resp_error(r, &err); return true; }
    return responder_lista(s, p, &docs, lote, r);
}

static bool orden_explain(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    if (p->nombre.tipo != BT_DOC) return error(r, ERR_VALOR, "explain necesita la orden find a explicar");
    const uint8_t *o = p->nombre.v;
    bson_elem nombre;
    if (!bson_buscar(o, "find", &nombre) && !bson_buscar(o, "count", &nombre))
        return error(r, ERR_VALOR, "explain solo entiende find y count");
    pet_t q = *p;
    q.orden = o;
    q.nombre = nombre;
    lmk_fmt(q.ns, sizeof q.ns, "%s.%s", p->base, nombre.tipo == BT_STRING ? bson_cadena(&nombre) : "?");
    const uint8_t *filtro, *orden;
    if ((!opt_doc(o, "filter", &filtro, &err) || !opt_doc(o, "sort", &orden, &err))) { resp_error(r, &err); return true; }
    if (!filtro && !opt_doc(o, "query", &filtro, &err)) { resp_error(r, &err); return true; }
    lmk_coleccion *c = coleccion_de(s, &q, false, &err);
    if (!c && err.codigo) { resp_error(r, &err); return true; }
    lmk_plan plan = { "EOF", NULL, 0, 0 };
    uint32_t n = 0;
    uint64_t t0 = plat_ms();
    if (c) {
        uint32_t *v = buscar(c, filtro, orden, opt_num(o, "skip", 0), opt_num(o, "limit", 0), &n, &plan, &err);
        if (!v) { resp_error(r, &err); return true; }
        lmk_libera(v);
    }
    uint32_t ms = (uint32_t)(plat_ms() - t0);
    uint32_t ini = bson_inicio(r);
    uint32_t qp = bson_inicio_sub(r, BT_DOC, "queryPlanner");
    bson_poner_texto(r, "namespace", q.ns);
    uint32_t wp = bson_inicio_sub(r, BT_DOC, "winningPlan");
    bson_poner_texto(r, "stage", plan.plan);
    if (plan.indice) bson_poner_texto(r, "indexName", plan.indice);
    if (orden && !bson_vacio(orden)) bson_poner_bool(r, "sortInMemory", true);
    bson_fin(r, wp);
    bson_fin(r, qp);
    uint32_t es = bson_inicio_sub(r, BT_DOC, "executionStats");
    bson_poner_int32(r, "nReturned", (int32_t)n);
    bson_poner_int32(r, "totalDocsExamined", (int32_t)plan.examinados);
    bson_poner_int32(r, "totalKeysExamined", (int32_t)plan.claves);
    bson_poner_int32(r, "executionTimeMillis", (int32_t)ms);
    bson_fin(r, es);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

/* ------------------------------------------------------------ índices */

static bool orden_create_indexes(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    bson_elem lista, un;
    lmk_buf solo = { 0 };
    if (!bson_buscar(p->orden, "indexes", &lista) || lista.tipo != BT_ARRAY) {
        /* createIndex: {"createIndex": col, "key": {...}, "name", "unique"} */
        bson_elem k;
        if (!bson_buscar(p->orden, "key", &k) || k.tipo != BT_DOC)
            return error(r, ERR_VALOR, "createIndexes necesita 'indexes' (un array)");
        uint32_t a = bson_inicio(&solo);
        uint32_t d = bson_inicio_sub(&solo, BT_DOC, "0");
        bson_it it;
        bson_elem e;
        bson_it_iniciar(&it, p->orden);
        while (bson_it_sig(&it, &e))
            if (nombre_es(&e, "key") || nombre_es(&e, "name") || nombre_es(&e, "unique")) bson_poner_elem(&solo, e.nombre, e.nlen, &e);
        bson_fin(&solo, d);
        bson_fin(&solo, a);
        lista.tipo = BT_ARRAY;
        lista.v = solo.p;
    }
    bool existia = coleccion_de(s, p, false, &err) != NULL;
    lmk_coleccion *c = coleccion_de(s, p, true, &err);
    if (!c) { buf_liberar(&solo); resp_error(r, &err); return true; }
    int antes = c->nix + 1;
    bson_it it;
    bson_it_iniciar(&it, lista.v);
    bool ok = true;
    while (ok && bson_it_sig(&it, &un)) {
        bson_elem key, campo, nombre;
        if (un.tipo != BT_DOC || !bson_buscar(un.v, "key", &key) || key.tipo != BT_DOC) { ok = lmk_fallo(&err, ERR_VALOR, "cada índice necesita 'key'"); break; }
        if (bson_contar(key.v) != 1) { ok = lmk_fallo(&err, ERR_VALOR, "lite-monkey solo tiene índices de un campo"); break; }
        bson_it ik;
        bson_it_iniciar(&ik, key.v);
        bson_it_sig(&ik, &campo);
        if (!bson_es_numero(campo.tipo) || (bson_a_double(&campo) != 1.0 && bson_a_double(&campo) != -1.0)) {
            ok = lmk_fallo(&err, ERR_VALOR, "el índice vale 1 (ascendente) o -1 (descendente)");
            break;
        }
        int dir = bson_a_double(&campo) < 0 ? -1 : 1;
        char nom[LMK_NOMBRE_MAX];
        if (bson_buscar(un.v, "name", &nombre) && nombre.tipo == BT_STRING) lmk_fmt(nom, sizeof nom, "%s", bson_cadena(&nombre));
        else lmk_fmt(nom, sizeof nom, "%s_%d", campo.nombre, dir);
        bool ya;
        ok = coleccion_crear_indice(c, nom, campo.nombre, dir, opt_bool(un.v, "unique", false), &ya, &err);
    }
    buf_liberar(&solo);
    if (!ok) { resp_error(r, &err); return true; }
    uint32_t ini = bson_inicio(r);
    bson_poner_bool(r, "createdCollectionAutomatically", !existia);
    bson_poner_int32(r, "numIndexesBefore", antes);
    bson_poner_int32(r, "numIndexesAfter", c->nix + 1);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

static bool orden_drop_indexes(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    bson_elem ix;
    lmk_coleccion *c = coleccion_de(s, p, false, &err);
    if (!c) { if (!err.codigo) lmk_fallo(&err, ERR_NS_NO_EXISTE, "la colección no existe"); resp_error(r, &err); return true; }
    if (!bson_buscar(p->orden, "index", &ix)) return error(r, ERR_VALOR, "dropIndexes necesita 'index'");
    int antes = c->nix + 1;
    if (elem_es_texto(&ix, "*")) {
        while (c->nix) if (!coleccion_borrar_indice(c, c->ix[c->nix - 1]->nombre, &err)) break;
    } else if (ix.tipo == BT_STRING) {
        if (elem_es_texto(&ix, "_id_")) return error(r, ERR_VALOR, "el índice _id_ no se puede borrar");
        if (!coleccion_borrar_indice(c, bson_cadena(&ix), &err)) { resp_error(r, &err); return true; }
    } else if (ix.tipo == BT_DOC) {
        bson_it it;
        bson_elem campo;
        bson_it_iniciar(&it, ix.v);
        bool hecho = false;
        if (bson_it_sig(&it, &campo))
            for (int i = 0; i < c->nix; i++)
                if (strcmp(c->ix[i]->campo, campo.nombre) == 0) { hecho = coleccion_borrar_indice(c, c->ix[i]->nombre, &err); break; }
        if (!hecho) return error(r, ERR_INDICE_NO_EXISTE, "el índice no existe");
    } else return error(r, ERR_VALOR, "'index' es un nombre, \"*\" o la clave");
    uint32_t ini = bson_inicio(r);
    bson_poner_int32(r, "nIndexesWas", antes);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

static void spec_indice(lmk_buf *b, const char *nombre, const char *campo, int dir, bool unico)
{
    uint32_t ini = bson_inicio(b);
    bson_poner_int32(b, "v", 2);
    uint32_t k = bson_inicio_sub(b, BT_DOC, "key");
    bson_poner_int32(b, campo, dir);
    bson_fin(b, k);
    bson_poner_texto(b, "name", nombre);
    if (unico) bson_poner_bool(b, "unique", true);
    bson_fin(b, ini);
}

static bool orden_list_indexes(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    lmk_coleccion *c = coleccion_de(s, p, false, &err);
    if (!c) { if (!err.codigo) lmk_fallo(&err, ERR_NS_NO_EXISTE, "la colección no existe"); resp_error(r, &err); return true; }
    lmk_buf docs = { 0 };
    spec_indice(&docs, "_id_", "_id", 1, false);
    for (int i = 0; i < c->nix; i++) spec_indice(&docs, c->ix[i]->nombre, c->ix[i]->campo, c->ix[i]->dir, c->ix[i]->unico);
    return responder_lista(s, p, &docs, 0, r);
}

/* ------------------------------------------------------------ administración */

static bool orden_list_collections(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    const uint8_t *filtro = NULL;
    opt_doc(p->orden, "filter", &filtro, &err);
    bool solo_nombres = opt_bool(p->orden, "nameOnly", false);
    lmk_buf docs = { 0 }, tmp = { 0 };
    lmk_base *b = nombre_valido(p->base, false) ? motor_base(s->m, p->base, false, &err) : NULL;
    for (uint32_t i = 0; b && i < b->ncol; i++) {
        tmp.n = 0;
        uint32_t ini = bson_inicio(&tmp);
        bson_poner_texto(&tmp, "name", b->col[i]->nombre);
        bson_poner_texto(&tmp, "type", "collection");
        bson_fin(&tmp, ini);
        if (filtro && consulta_validar(filtro, &err) && !consulta_coincide(tmp.p, filtro)) continue;
        buf_poner(&docs, tmp.p, tmp.n);
    }
    (void)solo_nombres;
    buf_liberar(&tmp);
    char ns[2 * LMK_NOMBRE_MAX + 2];
    lmk_fmt(ns, sizeof ns, "%s.$cmd.listCollections", p->base);
    lmk_fmt(p->ns, sizeof p->ns, "%s", ns);
    return responder_lista(s, p, &docs, 0, r);
}

static bool orden_list_databases(lmk_servidor *s, lmk_buf *r)
{
    uint32_t ini = bson_inicio(r);
    uint32_t ai = bson_inicio_sub(r, BT_ARRAY, "databases");
    uint64_t total = 0;
    char t[12];
    for (uint32_t i = 0; i < s->m->nbases; i++) {
        lmk_base *b = s->m->bases[i];
        uint64_t tam = base_tamano(b);
        total += tam;
        uint32_t d = bson_inicio_sub(r, BT_DOC, (bson_indice(t, i), t));
        bson_poner_texto(r, "name", b->nombre);
        bson_poner_int64(r, "sizeOnDisk", (int64_t)tam);
        bson_poner_bool(r, "empty", b->ncol == 0);
        bson_fin(r, d);
    }
    bson_fin(r, ai);
    bson_poner_int64(r, "totalSize", (int64_t)total);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

static void cerrar_cursores_de(lmk_servidor *s, lmk_coleccion *c)
{
    for (uint32_t i = 0; i < s->ncur;) {
        if (!s->cur[i]->lista && s->cur[i]->c == c) cursor_quitar(s, i);
        else i++;
    }
}

static bool orden_drop(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    lmk_coleccion *c = coleccion_de(s, p, false, &err);
    if (!c) { if (!err.codigo) lmk_fallo(&err, ERR_NS_NO_EXISTE, "ns not found: la colección no existe"); resp_error(r, &err); return true; }
    cerrar_cursores_de(s, c);
    int nix = c->nix + 1;
    if (!base_borrar_coleccion(c->base, c, &err)) { resp_error(r, &err); return true; }
    uint32_t ini = bson_inicio(r);
    bson_poner_texto(r, "ns", p->ns);
    bson_poner_int32(r, "nIndexesWas", nix);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

static bool orden_drop_database(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    lmk_base *b = nombre_valido(p->base, false) ? motor_base(s->m, p->base, false, &err) : NULL;
    if (b) {
        for (uint32_t i = 0; i < b->ncol; i++) cerrar_cursores_de(s, b->col[i]);
        if (!motor_borrar_base(s->m, b, &err)) { resp_error(r, &err); return true; }
    }
    uint32_t ini = bson_inicio(r);
    bson_poner_texto(r, "dropped", p->base);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

static bool orden_create(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    if (coleccion_de(s, p, false, &err)) return error(r, 48, "la colección ya existe");
    if (err.codigo || !coleccion_de(s, p, true, &err)) { resp_error(r, &err); return true; }
    uint32_t ini = bson_inicio(r);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

static bool orden_stats(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    lmk_coleccion *c = coleccion_de(s, p, false, &err);
    if (!c) { if (!err.codigo) lmk_fallo(&err, ERR_NS_NO_EXISTE, "la colección no existe"); resp_error(r, &err); return true; }
    uint64_t datos = 0, memoria = 0, claves = 0;
    for (uint32_t i = 0; i < c->nr; i++)
        if (c->r[i].viva) { datos += c->r[i].len; if (c->r[i].doc) memoria += c->r[i].len; }
    uint32_t ini = bson_inicio(r);
    bson_poner_texto(r, "ns", p->ns);
    bson_poner_int32(r, "count", (int32_t)c->vivas);
    bson_poner_int64(r, "size", (int64_t)datos);
    bson_poner_int32(r, "avgObjSize", c->vivas ? (int32_t)(uint32_t)(datos / c->vivas) : 0);
    bson_poner_int64(r, "storageSize", (int64_t)c->fin);
    bson_poner_int64(r, "garbageSize", (int64_t)(c->fin - c->vivos));
    bson_poner_int64(r, "cachedSize", (int64_t)memoria);
    bson_poner_int32(r, "nindexes", c->nix + 1);
    uint32_t ix = bson_inicio_sub(r, BT_DOC, "indexKeys");
    bson_poner_int32(r, "_id_", (int32_t)c->vivas);
    for (int i = 0; i < c->nix; i++) { bson_poner_int32(r, c->ix[i]->nombre, (int32_t)c->ix[i]->n); claves += c->ix[i]->n; }
    bson_fin(r, ix);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    (void)claves;
    return true;
}

static bool orden_db_stats(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    lmk_base *b = nombre_valido(p->base, false) ? motor_base(s->m, p->base, false, &err) : NULL;
    uint32_t objetos = 0, indices = 0;
    for (uint32_t i = 0; b && i < b->ncol; i++) {
        lmk_coleccion *c = base_coleccion(b, b->col[i]->nombre, false, &err);
        if (c) { objetos += c->vivas; indices += (uint32_t)c->nix + 1; }
    }
    uint32_t ini = bson_inicio(r);
    bson_poner_texto(r, "db", p->base);
    bson_poner_int32(r, "collections", b ? (int32_t)b->ncol : 0);
    bson_poner_int32(r, "objects", (int32_t)objetos);
    bson_poner_int32(r, "indexes", (int32_t)indices);
    bson_poner_int64(r, "storageSize", b ? (int64_t)base_tamano(b) : 0);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

static bool orden_compact(lmk_servidor *s, pet_t *p, lmk_buf *r)
{
    lmk_error err = { 0 };
    lmk_coleccion *c = coleccion_de(s, p, false, &err);
    if (!c) { if (!err.codigo) lmk_fallo(&err, ERR_NS_NO_EXISTE, "la colección no existe"); resp_error(r, &err); return true; }
    uint32_t antes = c->fin;
    if (!coleccion_compactar(c, &err)) { resp_error(r, &err); return true; }
    uint32_t ini = bson_inicio(r);
    bson_poner_int64(r, "bytesFreed", (int64_t)antes - (int64_t)c->fin);
    bson_poner_int64(r, "storageSize", (int64_t)c->fin);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

static bool orden_hello(lmk_servidor *s, lmk_buf *r)
{
    uint32_t ini = bson_inicio(r);
    bson_poner_bool(r, "isWritablePrimary", true);
    bson_poner_bool(r, "ismaster", true);
    bson_poner_texto(r, "version", LMK_VERSION);
    bson_poner_int32(r, "protocolo", 1);
    bson_poner_int32(r, "maxBsonObjectSize", (int32_t)LMK_DOC_MAX);
    bson_poner_int32(r, "maxMessageSizeBytes", (int32_t)LMK_TRAMA_MAX);
    bson_poner_int64(r, "localTime", (int64_t)plat_epoca_ms());
    bson_poner_bool(r, "auth", s->con_auth);
    uint32_t ai = bson_inicio_sub(r, BT_ARRAY, "commands");
    char t[12];
    for (uint32_t i = 0; ORDENES[i]; i++) bson_poner_texto(r, (bson_indice(t, i), t), ORDENES[i]);
    bson_fin(r, ai);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

static bool orden_server_status(lmk_servidor *s, lmk_buf *r)
{
    lmk_motor *m = s->m;
    uint32_t ini = bson_inicio(r);
    bson_poner_texto(r, "version", LMK_VERSION);
    bson_poner_int64(r, "uptimeMillis", (int64_t)(plat_ms() - s->inicio));
    uint32_t me = bson_inicio_sub(r, BT_DOC, "mem");
    bson_poner_int64(r, "cacheBytes", (int64_t)m->mem_usada);
    bson_poner_int64(r, "cacheLimit", (int64_t)m->mem_limite);
    bson_fin(r, me);
    uint32_t op = bson_inicio_sub(r, BT_DOC, "opcounters");
    bson_poner_int64(r, "insert", (int64_t)m->insertados);
    bson_poner_int64(r, "update", (int64_t)m->actualizados);
    bson_poner_int64(r, "delete", (int64_t)m->borrados);
    bson_poner_int64(r, "query", (int64_t)m->consultas);
    bson_poner_int64(r, "command", (int64_t)s->ordenes);
    bson_fin(r, op);
    bson_poner_int32(r, "openCursors", (int32_t)s->ncur);
    bson_poner_bool(r, "sync", m->sync);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

/* ------------------------------------------------------------ autenticación */

static bool orden_auth_start(lmk_servidor *s, lmk_sesion *ses, pet_t *p, lmk_buf *r)
{
    if (p->nombre.tipo != BT_STRING) return error(r, ERR_VALOR, "authStart necesita el usuario");
    lmk_fmt(ses->usuario, sizeof ses->usuario, "%s", bson_cadena(&p->nombre));
    plat_azar(ses->nonce, sizeof ses->nonce);
    ses->pendiente_auth = true;
    uint32_t ini = bson_inicio(r);
    bson_clave(r, BT_BINARIO, "nonce", 5);
    buf_u32(r, sizeof ses->nonce);
    buf_byte(r, 0);
    buf_poner(r, ses->nonce, sizeof ses->nonce);
    bson_poner_texto(r, "mecanismo", "HMAC-SHA256");
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    (void)s;
    return true;
}

static bool orden_auth_finish(lmk_servidor *s, lmk_sesion *ses, pet_t *p, lmk_buf *r)
{
    bson_elem prueba;
    if (!ses->pendiente_auth) return error(r, ERR_AUTENTICACION, "falta authStart");
    ses->pendiente_auth = false;
    if (!bson_buscar(p->orden, "proof", &prueba) || prueba.tipo != BT_BINARIO || lmk_le32(prueba.v) != 32)
        return error(r, ERR_AUTENTICACION, "falta la prueba (32 bytes)");
    /* prueba = HMAC-SHA256(clave, nonce || usuario) */
    uint8_t msg[24 + 64], esperada[32];
    uint32_t nu = (uint32_t)strlen(ses->usuario);
    memcpy(msg, ses->nonce, 24);
    memcpy(msg + 24, ses->usuario, nu);
    lmk_hmac_sha256(s->clave, s->nclave, msg, 24 + nu, esperada);
    uint8_t dif = 0;
    for (int i = 0; i < 32; i++) dif |= (uint8_t)(esperada[i] ^ prueba.v[5 + i]);
    if (dif || strcmp(ses->usuario, s->usuario) != 0) return error(r, ERR_AUTENTICACION, "usuario o clave incorrectos");
    ses->autenticado = true;
    /* el servidor también demuestra que conoce la clave: HMAC(clave, "servidor" || nonce) */
    uint8_t m2[8 + 24], suya[32];
    memcpy(m2, "servidor", 8);
    memcpy(m2 + 8, ses->nonce, 24);
    lmk_hmac_sha256(s->clave, s->nclave, m2, sizeof m2, suya);
    uint32_t ini = bson_inicio(r);
    bson_clave(r, BT_BINARIO, "serverProof", 11);
    buf_u32(r, 32);
    buf_byte(r, 0);
    buf_poner(r, suya, 32);
    bson_poner_double(r, "ok", 1.0);
    bson_fin(r, ini);
    return true;
}

/* ------------------------------------------------------------ despacho */

bool servidor_orden(lmk_servidor *s, lmk_sesion *ses, const uint8_t *orden, lmk_buf *r)
{
    r->n = 0;
    r->sin_memoria = false;
    s->ordenes++;
    consulta_olvidar_regex();
    pet_t p;
    memset(&p, 0, sizeof p);
    p.orden = orden;
    bson_it it;
    bson_it_iniciar(&it, orden);
    if (!bson_it_sig(&it, &p.nombre)) return error(r, ERR_ORDEN, "orden vacía");
    const char *o = p.nombre.nombre;
    bson_elem db;
    p.base = bson_buscar(orden, "$db", &db) && db.tipo == BT_STRING ? bson_cadena(&db) : "test";
    lmk_fmt(p.ns, sizeof p.ns, "%s.%s", p.base, p.nombre.tipo == BT_STRING ? bson_cadena(&p.nombre) : "$cmd");
    bool libre = strcmp(o, "hello") == 0 || strcmp(o, "isMaster") == 0 || strcmp(o, "ismaster") == 0 ||
                 strcmp(o, "ping") == 0 || strcmp(o, "authStart") == 0 || strcmp(o, "authFinish") == 0;
    if (s->con_auth && !ses->autenticado && !libre)
        return error(r, ERR_NO_AUTORIZADO, "hace falta autenticarse (usuario y clave)");
    bool seguir = true;
    if (!strcmp(o, "hello") || !strcmp(o, "isMaster") || !strcmp(o, "ismaster")) orden_hello(s, r);
    else if (!strcmp(o, "ping")) { uint32_t i = bson_inicio(r); bson_poner_double(r, "ok", 1.0); bson_fin(r, i); }
    else if (!strcmp(o, "insert")) orden_insert(s, &p, r);
    else if (!strcmp(o, "find")) orden_find(s, &p, r);
    else if (!strcmp(o, "getMore")) orden_getmore(s, &p, r);
    else if (!strcmp(o, "killCursors")) orden_killcursors(s, &p, r);
    else if (!strcmp(o, "update")) orden_update(s, &p, r);
    else if (!strcmp(o, "delete")) orden_delete(s, &p, r);
    else if (!strcmp(o, "count")) orden_count(s, &p, r);
    else if (!strcmp(o, "distinct")) orden_distinct(s, &p, r);
    else if (!strcmp(o, "aggregate")) orden_aggregate(s, &p, r);
    else if (!strcmp(o, "explain")) orden_explain(s, &p, r);
    else if (!strcmp(o, "createIndexes") || !strcmp(o, "createIndex")) orden_create_indexes(s, &p, r);
    else if (!strcmp(o, "dropIndexes") || !strcmp(o, "dropIndex")) orden_drop_indexes(s, &p, r);
    else if (!strcmp(o, "listIndexes")) orden_list_indexes(s, &p, r);
    else if (!strcmp(o, "listCollections")) orden_list_collections(s, &p, r);
    else if (!strcmp(o, "listDatabases")) orden_list_databases(s, r);
    else if (!strcmp(o, "create")) orden_create(s, &p, r);
    else if (!strcmp(o, "drop")) orden_drop(s, &p, r);
    else if (!strcmp(o, "dropDatabase")) orden_drop_database(s, &p, r);
    else if (!strcmp(o, "collStats") || !strcmp(o, "stats")) orden_stats(s, &p, r);
    else if (!strcmp(o, "dbStats")) orden_db_stats(s, &p, r);
    else if (!strcmp(o, "compact")) orden_compact(s, &p, r);
    else if (!strcmp(o, "serverStatus")) orden_server_status(s, r);
    else if (!strcmp(o, "authStart")) orden_auth_start(s, ses, &p, r);
    else if (!strcmp(o, "authFinish")) orden_auth_finish(s, ses, &p, r);
    else if (!strcmp(o, "shutdown")) {
        uint32_t i = bson_inicio(r);
        bson_poner_double(r, "ok", 1.0);
        bson_fin(r, i);
        seguir = false;
    } else {
        char msg[120];
        lmk_fmt(msg, sizeof msg, "orden desconocida: %s", o);
        error(r, ERR_ORDEN, msg);
    }
    if (r->sin_memoria) error(r, ERR_SIN_MEMORIA, "sin memoria para la respuesta");
    motor_sincronizar(s->m, false);
    return seguir;
}
