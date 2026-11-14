// SPDX-License-Identifier: GPL-2.0-or-later
/* bson.c - documentos en codificación BSON (bsonspec.org): lectura,
 * validación, escritura, comparación y caminos con punto; más el búfer que
 * crece, la arena y el árbol que usan las actualizaciones. */
#include "lmk.h"

bool lmk_fallo(lmk_error *e, int codigo, const char *msg)
{
    if (e) {
        e->codigo = codigo;
        size_t n = strlen(msg);
        if (n >= sizeof e->msg) n = sizeof e->msg - 1;
        memcpy(e->msg, msg, n);
        e->msg[n] = 0;
    }
    return false;
}

/* ------------------------------------------------------------ búfer */

void buf_liberar(lmk_buf *b)
{
    lmk_libera(b->p);
    b->p = NULL;
    b->n = b->cap = 0;
    b->sin_memoria = false;
}

bool buf_reservar(lmk_buf *b, uint32_t extra)
{
    if (b->sin_memoria) return false;
    if (b->n + extra <= b->cap) return true;
    uint32_t cap = b->cap ? b->cap : 256;
    while (cap < b->n + extra) {
        if (cap > 0x7FFFFFFFu) { b->sin_memoria = true; return false; }
        cap *= 2;
    }
    uint8_t *p = lmk_realoca(b->p, cap);
    if (!p) { b->sin_memoria = true; return false; }
    b->p = p;
    b->cap = cap;
    return true;
}

void buf_poner(lmk_buf *b, const void *d, uint32_t n)
{
    if (!n || !buf_reservar(b, n)) return;
    memcpy(b->p + b->n, d, n);
    b->n += n;
}

void buf_byte(lmk_buf *b, uint8_t v)
{
    if (!buf_reservar(b, 1)) return;
    b->p[b->n++] = v;
}

void buf_u32(lmk_buf *b, uint32_t v)
{
    if (!buf_reservar(b, 4)) return;
    lmk_pon32(b->p + b->n, v);
    b->n += 4;
}

void buf_u64(lmk_buf *b, uint64_t v)
{
    if (!buf_reservar(b, 8)) return;
    lmk_pon64(b->p + b->n, v);
    b->n += 8;
}

/* ------------------------------------------------------------ arena */

struct lmk_bloque {
    lmk_bloque *ant;
    uint32_t    usado, cap;
    uint8_t     d[];
};

void *arena_aloca(lmk_arena *a, uint32_t n)
{
    n = (n + 7) & ~7u;
    lmk_bloque *b = a->b;
    if (!b || b->cap - b->usado < n) {
        uint32_t cap = n > 16384 ? n : 16384;
        lmk_bloque *nb = lmk_aloca(sizeof(lmk_bloque) + cap);
        if (!nb) return NULL;
        nb->ant = b;
        nb->usado = 0;
        nb->cap = cap;
        a->b = b = nb;
    }
    void *p = b->d + b->usado;
    b->usado += n;
    return p;
}

void arena_liberar(lmk_arena *a)
{
    while (a->b) {
        lmk_bloque *ant = a->b->ant;
        lmk_libera(a->b);
        a->b = ant;
    }
}

/* ------------------------------------------------------------ lectura */

static uint32_t cadena_c(const uint8_t *p, uint32_t max)   /* longitud con el 0; 0 = sin 0 */
{
    const uint8_t *z = memchr(p, 0, max);
    return z ? (uint32_t)(z - p) + 1 : 0;
}

uint32_t bson_tam_valor(uint8_t tipo, const uint8_t *v, uint32_t max)
{
    uint32_t n;
    switch (tipo) {
    case BT_DOUBLE: case BT_FECHA: case BT_INT64: case BT_TIMESTAMP: return max >= 8 ? 8 : 0;
    case BT_INT32: return max >= 4 ? 4 : 0;
    case BT_BOOL: return max >= 1 ? 1 : 0;
    case BT_OID: return max >= 12 ? 12 : 0;
    case BT_NULL: case BT_MINKEY: case BT_MAXKEY: return 0x80000000u;   /* cero bytes (ver abajo) */
    case BT_STRING:
        if (max < 5) return 0;
        n = lmk_le32(v);
        if (n < 1 || n > max - 4) return 0;
        return 4 + n;
    case BT_DOC: case BT_ARRAY:
        if (max < 5) return 0;
        n = lmk_le32(v);
        return n >= 5 && n <= max ? n : 0;
    case BT_BINARIO:
        if (max < 5) return 0;
        n = lmk_le32(v);
        return n <= max - 5 ? 5 + n : 0;
    case BT_REGEX: {
        uint32_t a = cadena_c(v, max);
        if (!a) return 0;
        uint32_t b = cadena_c(v + a, max - a);
        return b ? a + b : 0;
    }
    default: return 0;
    }
}

/* El tamaño real (0 para null) a partir de lo que devuelve bson_tam_valor. */
static inline uint32_t tam_real(uint32_t t) { return t == 0x80000000u ? 0 : t; }

static bool validar_doc(const uint8_t *p, uint32_t n, int prof, bool array, lmk_error *err)
{
    if (prof > LMK_PROF_MAX) return lmk_fallo(err, ERR_FORMATO, "documento anidado más de 32 niveles");
    if (n < 5 || lmk_le32(p) != n || p[n - 1] != 0) return lmk_fallo(err, ERR_FORMATO, "BSON mal formado (tamaño)");
    uint32_t i = 4, cuenta = 0;
    while (i < n - 1) {
        uint8_t tipo = p[i++];
        uint32_t nl = cadena_c(p + i, n - 1 - i);
        if (!nl) return lmk_fallo(err, ERR_FORMATO, "BSON mal formado (nombre)");
        if (array) {   /* los nombres de un array son 0, 1, 2... */
            char esperado[12];
            uint32_t el = bson_indice(esperado, cuenta);
            if (el != nl - 1 || memcmp(esperado, p + i, el) != 0)
                return lmk_fallo(err, ERR_FORMATO, "BSON mal formado (índice de array)");
        }
        i += nl;
        uint32_t t = bson_tam_valor(tipo, p + i, n - 1 - i);
        if (!t) {
            if (tipo == 0x13) return lmk_fallo(err, ERR_FORMATO, "Decimal128 no está soportado");
            return lmk_fallo(err, ERR_FORMATO, "BSON mal formado (valor o tipo desconocido)");
        }
        t = tam_real(t);
        if (tipo == BT_STRING && p[i + t - 1] != 0) return lmk_fallo(err, ERR_FORMATO, "BSON mal formado (cadena)");
        if ((tipo == BT_DOC || tipo == BT_ARRAY) && !validar_doc(p + i, t, prof + 1, tipo == BT_ARRAY, err))
            return false;
        if (tipo == BT_BOOL && p[i] > 1) return lmk_fallo(err, ERR_FORMATO, "BSON mal formado (bool)");
        i += t;
        cuenta++;
    }
    if (i != n - 1) return lmk_fallo(err, ERR_FORMATO, "BSON mal formado (fin)");
    return true;
}

bool bson_validar(const uint8_t *p, uint32_t n, lmk_error *err)
{
    return validar_doc(p, n, 1, false, err);
}

void bson_it_iniciar(bson_it *it, const uint8_t *doc)
{
    it->p = doc + 4;
    it->fin = doc + bson_tam(doc) - 1;
}

bool bson_it_sig(bson_it *it, bson_elem *e)
{
    if (it->p >= it->fin) return false;
    e->tipo = *it->p++;
    e->nombre = (const char *)it->p;
    e->nlen = (uint32_t)strlen(e->nombre);
    it->p += e->nlen + 1;
    e->v = it->p;
    e->vlen = tam_real(bson_tam_valor(e->tipo, it->p, (uint32_t)(it->fin - it->p)));
    it->p += e->vlen;
    return true;
}

bool bson_buscar(const uint8_t *doc, const char *nombre, bson_elem *e)
{
    bson_it it;
    bson_it_iniciar(&it, doc);
    while (bson_it_sig(&it, e))
        if (strcmp(e->nombre, nombre) == 0) return true;
    return false;
}

bool bson_vacio(const uint8_t *doc) { return bson_tam(doc) <= 5; }

uint32_t bson_contar(const uint8_t *doc)
{
    bson_it it;
    bson_elem e;
    uint32_t n = 0;
    bson_it_iniciar(&it, doc);
    while (bson_it_sig(&it, &e)) n++;
    return n;
}

bool nombre_es(const bson_elem *e, const char *s) { return strcmp(e->nombre, s) == 0; }

bool elem_es_texto(const bson_elem *e, const char *s)
{
    return e->tipo == BT_STRING && strcmp(bson_cadena(e), s) == 0;
}

/* ------------------------------------------------------------ escritura */

uint32_t bson_inicio(lmk_buf *b)
{
    uint32_t ini = b->n;
    buf_u32(b, 0);
    return ini;
}

void bson_fin(lmk_buf *b, uint32_t ini)
{
    buf_byte(b, 0);
    if (!b->sin_memoria) lmk_pon32(b->p + ini, b->n - ini);
}

void bson_clave(lmk_buf *b, uint8_t tipo, const char *nombre, uint32_t nlen)
{
    if (!buf_reservar(b, nlen + 2)) return;
    b->p[b->n++] = tipo;
    memcpy(b->p + b->n, nombre, nlen);
    b->n += nlen;
    b->p[b->n++] = 0;
}

static void clave_c(lmk_buf *b, uint8_t tipo, const char *nombre) { bson_clave(b, tipo, nombre, (uint32_t)strlen(nombre)); }

uint32_t bson_inicio_sub(lmk_buf *b, uint8_t tipo, const char *nombre)
{
    clave_c(b, tipo, nombre);
    return bson_inicio(b);
}

void bson_poner_double(lmk_buf *b, const char *nombre, double d)
{
    uint64_t u;
    memcpy(&u, &d, 8);
    clave_c(b, BT_DOUBLE, nombre);
    buf_u64(b, u);
}

void bson_poner_int32(lmk_buf *b, const char *nombre, int32_t v) { clave_c(b, BT_INT32, nombre); buf_u32(b, (uint32_t)v); }
void bson_poner_int64(lmk_buf *b, const char *nombre, int64_t v) { clave_c(b, BT_INT64, nombre); buf_u64(b, (uint64_t)v); }

void bson_poner_entero(lmk_buf *b, const char *nombre, int64_t v)
{
    if (v >= -2147483647 - 1 && v <= 2147483647) bson_poner_int32(b, nombre, (int32_t)v);
    else bson_poner_int64(b, nombre, v);
}

void bson_poner_bool(lmk_buf *b, const char *nombre, bool v) { clave_c(b, BT_BOOL, nombre); buf_byte(b, v ? 1 : 0); }
void bson_poner_null(lmk_buf *b, const char *nombre) { clave_c(b, BT_NULL, nombre); }

void bson_poner_cadena(lmk_buf *b, const char *nombre, const char *s, uint32_t len)
{
    clave_c(b, BT_STRING, nombre);
    buf_u32(b, len + 1);
    buf_poner(b, s, len);
    buf_byte(b, 0);
}

void bson_poner_texto(lmk_buf *b, const char *nombre, const char *s) { bson_poner_cadena(b, nombre, s, (uint32_t)strlen(s)); }

void bson_poner_elem(lmk_buf *b, const char *nombre, uint32_t nlen, const bson_elem *e)
{
    bson_clave(b, e->tipo, nombre, nlen);
    buf_poner(b, e->v, e->vlen);
}

uint32_t bson_indice(char *dst, uint32_t i)
{
    char t[12];
    uint32_t n = 0;
    do { t[n++] = (char)('0' + i % 10); i /= 10; } while (i);
    for (uint32_t k = 0; k < n; k++) dst[k] = t[n - 1 - k];
    dst[n] = 0;
    return n;
}

/* ------------------------------------------------------------ números */

bool bson_es_numero(uint8_t t) { return t == BT_DOUBLE || t == BT_INT32 || t == BT_INT64; }

double bson_a_double(const bson_elem *e)
{
    switch (e->tipo) {
    case BT_DOUBLE: return lmk_ledouble(e->v);
    case BT_INT32: return (double)bson_int32(e->v);
    case BT_INT64: return (double)bson_int64(e->v);
    case BT_BOOL: return e->v[0] ? 1.0 : 0.0;
    default: return 0.0;
    }
}

int64_t bson_a_int64(const bson_elem *e)
{
    switch (e->tipo) {
    case BT_DOUBLE: {
        double d = lmk_ledouble(e->v);
        if (d != d) return 0;
        if (d >= 9.2e18) return INT64_MAX;
        if (d <= -9.2e18) return INT64_MIN;
        return (int64_t)d;
    }
    case BT_INT32: return bson_int32(e->v);
    case BT_INT64: return bson_int64(e->v);
    case BT_BOOL: return e->v[0];
    default: return 0;
    }
}

bool bson_verdadero(const bson_elem *e)
{
    switch (e->tipo) {
    case BT_BOOL: return e->v[0] != 0;
    case BT_DOUBLE: case BT_INT32: case BT_INT64: return bson_a_double(e) != 0.0;
    case BT_NULL: return false;
    default: return true;
    }
}

/* ------------------------------------------------------------ comparación */

int bson_rango(uint8_t t)
{
    switch (t) {
    case BT_MINKEY: return 1;
    case BT_NULL: return 2;
    case BT_DOUBLE: case BT_INT32: case BT_INT64: return 3;
    case BT_STRING: return 4;
    case BT_DOC: return 5;
    case BT_ARRAY: return 6;
    case BT_BINARIO: return 7;
    case BT_OID: return 8;
    case BT_BOOL: return 9;
    case BT_FECHA: return 10;
    case BT_TIMESTAMP: return 11;
    case BT_REGEX: return 12;
    case BT_MAXKEY: return 13;
    default: return 14;
    }
}

static int cmp_num(uint8_t ta, const uint8_t *va, uint8_t tb, const uint8_t *vb)
{
    if (ta != BT_DOUBLE && tb != BT_DOUBLE) {
        int64_t a = ta == BT_INT32 ? bson_int32(va) : bson_int64(va);
        int64_t b = tb == BT_INT32 ? bson_int32(vb) : bson_int64(vb);
        return a < b ? -1 : a > b;
    }
    bson_elem ea = { .tipo = ta, .v = va }, eb = { .tipo = tb, .v = vb };
    double a = bson_a_double(&ea), b = bson_a_double(&eb);
    if (a != a) return b != b ? 0 : -1;    /* NaN va antes que todos los números */
    if (b != b) return 1;
    return a < b ? -1 : a > b;
}

static int cmp_bytes(const uint8_t *a, uint32_t na, const uint8_t *b, uint32_t nb)
{
    int r = memcmp(a, b, na < nb ? na : nb);
    if (r) return r < 0 ? -1 : 1;
    return na < nb ? -1 : na > nb;
}

static int cmp_doc(const uint8_t *a, const uint8_t *b, bool array)
{
    bson_it ia, ib;
    bson_elem ea, eb;
    bson_it_iniciar(&ia, a);
    bson_it_iniciar(&ib, b);
    for (;;) {
        bool ha = bson_it_sig(&ia, &ea), hb = bson_it_sig(&ib, &eb);
        if (!ha || !hb) return ha ? 1 : hb ? -1 : 0;
        if (!array) {
            int ra = bson_rango(ea.tipo), rb = bson_rango(eb.tipo);
            if (ra != rb) return ra < rb ? -1 : 1;
            int r = cmp_bytes((const uint8_t *)ea.nombre, ea.nlen, (const uint8_t *)eb.nombre, eb.nlen);
            if (r) return r;
        }
        int r = elem_comparar(&ea, &eb);
        if (r) return r;
    }
}

int bson_comparar(uint8_t ta, const uint8_t *va, uint8_t tb, const uint8_t *vb)
{
    int ra = bson_rango(ta), rb = bson_rango(tb);
    if (ra != rb) return ra < rb ? -1 : 1;
    switch (ra) {
    case 3: return cmp_num(ta, va, tb, vb);
    case 4: return cmp_bytes(va + 4, lmk_le32(va) - 1, vb + 4, lmk_le32(vb) - 1);
    case 5: return cmp_doc(va, vb, false);
    case 6: return cmp_doc(va, vb, true);
    case 7: {
        uint32_t na = lmk_le32(va), nb = lmk_le32(vb);
        if (na != nb) return na < nb ? -1 : 1;
        if (va[4] != vb[4]) return va[4] < vb[4] ? -1 : 1;
        return cmp_bytes(va + 5, na, vb + 5, nb);
    }
    case 8: return cmp_bytes(va, 12, vb, 12);
    case 9: return va[0] < vb[0] ? -1 : va[0] > vb[0];
    case 10: case 11: {
        int64_t a = bson_int64(va), b = bson_int64(vb);
        if (ra == 11) { uint64_t ua = (uint64_t)a, ub = (uint64_t)b; return ua < ub ? -1 : ua > ub; }
        return a < b ? -1 : a > b;
    }
    case 12: {
        int r = strcmp((const char *)va, (const char *)vb);
        if (r) return r < 0 ? -1 : 1;
        const char *fa = (const char *)va + strlen((const char *)va) + 1;
        const char *fb = (const char *)vb + strlen((const char *)vb) + 1;
        r = strcmp(fa, fb);
        return r < 0 ? -1 : r > 0;
    }
    default: return 0;
    }
}

static uint32_t fnv(uint32_t h, const uint8_t *p, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

uint32_t bson_hash(uint8_t tipo, const uint8_t *v, uint32_t vlen)
{
    uint32_t h = 2166136261u;
    uint8_t r = (uint8_t)bson_rango(tipo);
    h = fnv(h, &r, 1);
    if (r == 3) {   /* 1, 1L y 1.0 son el mismo _id: se normaliza el número */
        bson_elem e = { .tipo = tipo, .v = v };
        double d = bson_a_double(&e);
        uint8_t t[8];
        if (tipo != BT_DOUBLE || (d >= -9.2e18 && d <= 9.2e18 && (double)(int64_t)d == d)) {
            lmk_pon64(t, (uint64_t)(tipo == BT_DOUBLE ? (int64_t)d : bson_a_int64(&e)));
        } else {
            memcpy(t, v, 8);
        }
        return fnv(h, t, 8);
    }
    return fnv(h, v, vlen);
}

/* ------------------------------------------------------------ caminos */

static bool es_numero_txt(const char *s, uint32_t n, uint32_t *v)
{
    if (!n || n > 9) return false;
    uint32_t x = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        x = x * 10 + (uint32_t)(s[i] - '0');
    }
    if (n > 1 && s[0] == '0') return false;
    *v = x;
    return true;
}

static bool camino_en(const uint8_t *doc, bool es_array, const char *c, uint32_t clen,
                      bson_visita_fn f, void *ctx, int prof);

/* Visita el valor encontrado: si es el final del camino, el valor y (si es
 * array) cada elemento; si no, sigue bajando. */
static bool seguir(const bson_elem *e, const char *resto, uint32_t rlen, bson_visita_fn f, void *ctx, int prof)
{
    if (!rlen) {
        if (f(ctx, e, false)) return true;
        if (e->tipo == BT_ARRAY) {
            bson_it it;
            bson_elem x;
            bson_it_iniciar(&it, e->v);
            while (bson_it_sig(&it, &x))
                if (f(ctx, &x, true)) return true;
        }
        return false;
    }
    if (e->tipo == BT_DOC || e->tipo == BT_ARRAY)
        return camino_en(e->v, e->tipo == BT_ARRAY, resto, rlen, f, ctx, prof + 1);
    return false;
}

static bool camino_en(const uint8_t *doc, bool es_array, const char *c, uint32_t clen,
                      bson_visita_fn f, void *ctx, int prof)
{
    if (prof > LMK_PROF_MAX + 2) return false;
    const char *punto = memchr(c, '.', clen);
    uint32_t k = punto ? (uint32_t)(punto - c) : clen;
    const char *resto = punto ? punto + 1 : c + clen;
    uint32_t rlen = punto ? clen - k - 1 : 0;
    bson_it it;
    bson_elem e;
    bson_it_iniciar(&it, doc);
    if (!es_array) {
        while (bson_it_sig(&it, &e))
            if (e.nlen == k && memcmp(e.nombre, c, k) == 0) return seguir(&e, resto, rlen, f, ctx, prof);
        return false;
    }
    /* Dentro de un array: "a.1" es el elemento 1; "a.b" busca b en cada
     * documento del array. */
    uint32_t idx;
    bool numerico = es_numero_txt(c, k, &idx);
    uint32_t i = 0;
    while (bson_it_sig(&it, &e)) {
        if (numerico && i == idx && seguir(&e, resto, rlen, f, ctx, prof)) return true;
        if (e.tipo == BT_DOC && camino_en(e.v, false, c, clen, f, ctx, prof + 1)) return true;
        i++;
    }
    return false;
}

bool bson_camino(const uint8_t *doc, const char *camino, uint32_t clen, bson_visita_fn f, void *ctx)
{
    return camino_en(doc, false, camino, clen, f, ctx, 0);
}

bool bson_camino_simple(const uint8_t *doc, const char *c, uint32_t clen, bson_elem *e)
{
    for (int prof = 0; prof <= LMK_PROF_MAX; prof++) {
        const char *punto = memchr(c, '.', clen);
        uint32_t k = punto ? (uint32_t)(punto - c) : clen;
        bson_it it;
        bson_it_iniciar(&it, doc);
        bool achou = false;
        while (bson_it_sig(&it, e))
            if (e->nlen == k && memcmp(e->nombre, c, k) == 0) { achou = true; break; }
        if (!achou) return false;
        if (!punto) return true;
        if (e->tipo != BT_DOC && e->tipo != BT_ARRAY) return false;
        doc = e->v;
        clen -= k + 1;
        c = punto + 1;
    }
    return false;
}

/* ------------------------------------------------------------ árbol */

lmk_nodo *arbol_desde(lmk_arena *a, const uint8_t *doc)
{
    lmk_nodo *n = arena_aloca(a, sizeof *n);
    if (!n) return NULL;
    memset(n, 0, sizeof *n);
    n->tipo = BT_DOC;
    n->v = doc;
    n->vlen = bson_tam(doc);
    return arbol_expandir(a, n) ? n : NULL;
}

static bool crecer_hijos(lmk_arena *a, lmk_nodo *n, uint32_t quiero)
{
    if (quiero <= n->cap) return true;
    uint32_t cap = n->cap ? n->cap * 2 : 8;
    while (cap < quiero) cap *= 2;
    lmk_nodo *h = arena_aloca(a, cap * (uint32_t)sizeof *h);
    if (!h) return false;
    if (n->n) memcpy(h, n->hijos, n->n * sizeof *h);
    n->hijos = h;
    n->cap = cap;
    return true;
}

bool arbol_expandir(lmk_arena *a, lmk_nodo *n)
{
    if (n->expandido) return true;
    if (n->tipo != BT_DOC && n->tipo != BT_ARRAY) return false;
    n->n = 0;
    if (!crecer_hijos(a, n, bson_contar(n->v) + 4)) return false;
    bson_it it;
    bson_elem e;
    bson_it_iniciar(&it, n->v);
    while (bson_it_sig(&it, &e)) {
        lmk_nodo *h = &n->hijos[n->n++];
        memset(h, 0, sizeof *h);
        h->nombre = e.nombre;
        h->nlen = e.nlen;
        arbol_poner(h, &e);
    }
    n->expandido = true;
    return true;
}

int arbol_hijo(lmk_nodo *n, const char *nombre, uint32_t nlen)
{
    for (uint32_t i = 0; i < n->n; i++)
        if (n->hijos[i].nlen == nlen && memcmp(n->hijos[i].nombre, nombre, nlen) == 0) return (int)i;
    return -1;
}

lmk_nodo *arbol_anadir(lmk_arena *a, lmk_nodo *n, const char *nombre, uint32_t nlen)
{
    if (!crecer_hijos(a, n, n->n + 1)) return NULL;
    char *copia = arena_aloca(a, nlen + 1);
    if (!copia) return NULL;
    memcpy(copia, nombre, nlen);
    copia[nlen] = 0;
    lmk_nodo *h = &n->hijos[n->n++];
    memset(h, 0, sizeof *h);
    h->nombre = copia;
    h->nlen = nlen;
    h->tipo = BT_NULL;
    return h;
}

void arbol_quitar(lmk_nodo *n, uint32_t i)
{
    if (i >= n->n) return;
    memmove(&n->hijos[i], &n->hijos[i + 1], (n->n - i - 1) * sizeof *n->hijos);
    n->n--;
}

void arbol_poner(lmk_nodo *dst, const bson_elem *e)
{
    dst->tipo = e->tipo;
    dst->v = e->v;
    dst->vlen = e->vlen;
    dst->expandido = false;
    dst->hijos = NULL;
    dst->n = dst->cap = 0;
}

void arbol_escribir_valor(lmk_nodo *n, lmk_buf *b)
{
    if (!n->expandido) { buf_poner(b, n->v, n->vlen); return; }
    uint32_t ini = bson_inicio(b);
    char idx[12];
    for (uint32_t i = 0; i < n->n; i++) {
        lmk_nodo *h = &n->hijos[i];
        if (n->tipo == BT_ARRAY) {
            uint32_t l = bson_indice(idx, i);
            bson_clave(b, h->tipo, idx, l);
        } else {
            bson_clave(b, h->tipo, h->nombre, h->nlen);
        }
        arbol_escribir_valor(h, b);
    }
    bson_fin(b, ini);
}

void arbol_escribir(lmk_nodo *raiz, lmk_buf *b) { arbol_escribir_valor(raiz, b); }
