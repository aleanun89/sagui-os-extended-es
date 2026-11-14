// SPDX-License-Identifier: GPL-2.0-or-later
/* coleccion.c - el motor: bases, colecciones, almacenamiento de solo añadir,
 * índice hash del _id, índices secundarios y planificador de consultas.
 *
 * Archivo de una colección (base/coleccion.lmk): una cabecera "LMK1" de 8
 * bytes y registros
 *     tipo (1 byte) | longitud (u32 LE) | documento BSON | CRC32 (u32 LE)
 * El CRC cubre tipo, longitud y documento. Tipos: 1 insertar, 2 actualizar
 * (el documento entero nuevo), 3 borrar ({_id}), 4 índice (su
 * especificación) y 5 borrar índice ({name}). Al abrir se recorre el
 * archivo entero; el primer registro cortado o con CRC malo termina la
 * lectura (corte de luz a media escritura) y la colección se reescribe
 * limpia. La compactación escribe un archivo nuevo y lo renombra encima. */
#include "lmk.h"
#include "../plataforma.h"

#define REG_INSERTAR      1
#define REG_ACTUALIZAR    2
#define REG_BORRAR        3
#define REG_INDICE        4
#define REG_BORRAR_INDICE 5
#define CAB_TAM           8
#define REG_EXTRA         9          /* tipo + longitud + CRC */
#define PENDIENTE_MAX     (1024u * 1024u)
#define COMPACTAR_MIN     (256u * 1024u)
#define LIBRE             0
#define BORRADO           0xFFFFFFFFu

static const uint8_t CABECERA[CAB_TAM] = { 'L', 'M', 'K', '1', 1, 0, 0, 0 };

/* ------------------------------------------------------------ CRC32 (IEEE 802.3) */

static uint32_t tabla_crc[256];

uint32_t lmk_crc32(uint32_t crc, const uint8_t *p, uint32_t n)
{
    if (!tabla_crc[1]) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            tabla_crc[i] = c;
        }
    }
    crc = ~crc;
    for (uint32_t i = 0; i < n; i++) crc = tabla_crc[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

static void avisar(lmk_motor *m, const char *t)
{
    if (m->aviso) m->aviso(t);
}

bool nombre_valido(const char *s, bool coleccion)
{
    size_t n = strlen(s);
    if (!n || n > LMK_NOMBRE_MAX || s[0] == '.' || s[0] == '$') return false;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
                  (c == '.' && coleccion);
        if (!ok) return false;
    }
    if (coleccion && n > 4 && strcmp(s + n - 4, ".tmp") == 0) return false;
    return true;
}

static void unir_ruta(char *dst, size_t max, const char *a, const char *b, const char *ext)
{
    lmk_fmt(dst, max, "%s/%s%s", a, b, ext);
}

/* ------------------------------------------------------------ tabla hash del _id */

static bool id_de(const uint8_t *doc, bson_elem *id) { return bson_buscar(doc, "_id", id); }

static bool tabla_crecer(lmk_coleccion *c, uint32_t cap)
{
    uint32_t *t = lmk_aloca(cap * sizeof *t);
    if (!t) return false;
    memset(t, 0, cap * sizeof *t);
    for (uint32_t i = 0; i < c->captabla; i++) {
        uint32_t v = c->tabla[i];
        if (v == LIBRE || v == BORRADO) continue;
        uint32_t j = c->r[v - 1].hash & (cap - 1);
        while (t[j]) j = (j + 1) & (cap - 1);
        t[j] = v;
    }
    lmk_libera(c->tabla);
    c->tabla = t;
    c->captabla = cap;
    c->borrados_tabla = 0;
    return true;
}

static int64_t tabla_buscar(lmk_coleccion *c, const bson_elem *id, uint32_t h)
{
    if (!c->captabla) return -1;
    uint32_t j = h & (c->captabla - 1);
    for (;;) {
        uint32_t v = c->tabla[j];
        if (v == LIBRE) return -1;
        if (v != BORRADO && c->r[v - 1].hash == h) {
            const uint8_t *d = coleccion_doc(c, v - 1);
            bson_elem otro;
            if (d && id_de(d, &otro) && bson_rango(otro.tipo) == bson_rango(id->tipo) && elem_comparar(&otro, id) == 0)
                return v - 1;
        }
        j = (j + 1) & (c->captabla - 1);
    }
}

static bool tabla_poner(lmk_coleccion *c, uint32_t ranura)
{
    if ((c->ntabla + c->borrados_tabla + 1) * 4 >= c->captabla * 3) {
        uint32_t cap = c->captabla ? c->captabla : 64;
        while ((c->ntabla + 1) * 2 >= cap) cap *= 2;
        if (!tabla_crecer(c, cap)) return false;
    }
    uint32_t j = c->r[ranura].hash & (c->captabla - 1);
    while (c->tabla[j] != LIBRE && c->tabla[j] != BORRADO) j = (j + 1) & (c->captabla - 1);
    if (c->tabla[j] == BORRADO) c->borrados_tabla--;
    c->tabla[j] = ranura + 1;
    c->ntabla++;
    return true;
}

static void tabla_quitar(lmk_coleccion *c, uint32_t ranura)
{
    if (!c->captabla) return;
    uint32_t j = c->r[ranura].hash & (c->captabla - 1);
    while (c->tabla[j] != LIBRE) {
        if (c->tabla[j] == ranura + 1) {
            c->tabla[j] = BORRADO;
            c->ntabla--;
            c->borrados_tabla++;
            return;
        }
        j = (j + 1) & (c->captabla - 1);
    }
}

/* ------------------------------------------------------------ ranuras y memoria */

static int64_t ranura_nueva(lmk_coleccion *c)
{
    if (c->nlibres) return c->libres[--c->nlibres];
    if (c->nr == c->capr) {
        uint32_t cap = c->capr ? c->capr * 2 : 256;
        lmk_ranura *r = lmk_realoca(c->r, cap * sizeof *r);
        if (!r) return -1;
        c->r = r;
        c->capr = cap;
    }
    memset(&c->r[c->nr], 0, sizeof *c->r);
    return c->nr++;
}

static void ranura_soltar(lmk_coleccion *c, uint32_t s)
{
    if (c->nlibres == c->caplibres) {
        uint32_t cap = c->caplibres ? c->caplibres * 2 : 64;
        uint32_t *l = lmk_realoca(c->libres, cap * sizeof *l);
        if (!l) return;   /* la ranura se pierde: no pasa nada */
        c->libres = l;
        c->caplibres = cap;
    }
    c->libres[c->nlibres++] = s;
}

static void cache_soltar(lmk_coleccion *c, lmk_ranura *r)
{
    if (r->doc) {
        lmk_libera(r->doc);
        c->base->motor->mem_usada -= r->len;
        r->doc = NULL;
    }
}

static void cache_poner(lmk_coleccion *c, lmk_ranura *r, const uint8_t *doc, uint32_t len)
{
    lmk_motor *m = c->base->motor;
    r->doc = NULL;
    if (m->mem_usada + len > m->mem_limite) return;
    r->doc = lmk_aloca(len);
    if (!r->doc) return;
    memcpy(r->doc, doc, len);
    m->mem_usada += len;
}

const uint8_t *coleccion_doc(lmk_coleccion *c, uint32_t s)
{
    lmk_ranura *r = &c->r[s];
    if (!r->viva) return NULL;
    if (r->doc) return r->doc;
    if (r->off >= c->pendiente_off) return c->pendiente.p + (r->off - c->pendiente_off);
    c->temporal.n = 0;
    if (!buf_reservar(&c->temporal, r->len)) return NULL;
    if (plat_leer_en(c->fd, r->off, c->temporal.p, r->len) != (int)r->len) return NULL;
    return c->temporal.p;
}

/* ------------------------------------------------------------ registros */

static uint32_t escribir_registro(lmk_coleccion *c, uint8_t tipo, const uint8_t *doc, uint32_t len)
{
    lmk_buf *p = &c->pendiente;
    if (!buf_reservar(p, len + REG_EXTRA)) return 0;
    uint32_t ini = p->n;
    p->p[p->n++] = tipo;
    lmk_pon32(p->p + p->n, len);
    p->n += 4;
    memcpy(p->p + p->n, doc, len);
    p->n += len;
    lmk_pon32(p->p + p->n, lmk_crc32(0, p->p + ini, len + 5));
    p->n += 4;
    uint32_t off_doc = c->fin + 5;
    c->fin += len + REG_EXTRA;
    c->sin_sync++;
    if (p->n >= PENDIENTE_MAX) coleccion_volcar(c);
    return off_doc;
}

bool coleccion_volcar(lmk_coleccion *c)
{
    if (!c->pendiente.n || c->fd < 0) return true;
    /* Los documentos que se leían del búfer pendiente pasan a leerse del archivo. */
    bool ok = plat_escribir_en(c->fd, c->pendiente_off, c->pendiente.p, c->pendiente.n);
    if (!ok) {
        if (!c->error_disco) avisar(c->base->motor, "error al escribir en el disco: la colección queda en solo lectura");
        c->error_disco = true;
        return false;
    }
    c->pendiente_off += c->pendiente.n;
    c->pendiente.n = 0;
    if (c->pendiente.cap > 4 * PENDIENTE_MAX) buf_liberar(&c->pendiente);
    return true;
}

/* ------------------------------------------------------------ índices: especificación */

static void escribir_spec(lmk_buf *b, const lmk_indice *ix)
{
    uint32_t ini = bson_inicio(b);
    bson_poner_texto(b, "name", ix->nombre);
    uint32_t k = bson_inicio_sub(b, BT_DOC, "key");
    bson_poner_int32(b, ix->campo, ix->dir);
    bson_fin(b, k);
    if (ix->unico) bson_poner_bool(b, "unique", true);
    bson_fin(b, ini);
}

static lmk_indice *indice_desde_spec(const uint8_t *spec)
{
    bson_elem n, k, u, campo;
    if (!bson_buscar(spec, "name", &n) || n.tipo != BT_STRING || !bson_buscar(spec, "key", &k) || k.tipo != BT_DOC)
        return NULL;
    bson_it it;
    bson_it_iniciar(&it, k.v);
    if (!bson_it_sig(&it, &campo) || campo.nlen >= 128 || bson_cadena_len(&n) >= LMK_NOMBRE_MAX) return NULL;
    lmk_indice *ix = lmk_aloca(sizeof *ix);
    if (!ix) return NULL;
    memset(ix, 0, sizeof *ix);
    memcpy(ix->nombre, bson_cadena(&n), bson_cadena_len(&n));
    memcpy(ix->campo, campo.nombre, campo.nlen);
    ix->dir = bson_a_double(&campo) < 0 ? -1 : 1;
    ix->unico = bson_buscar(spec, "unique", &u) && bson_verdadero(&u);
    ix->semilla = lmk_crc32(0, (const uint8_t *)ix->nombre, (uint32_t)strlen(ix->nombre)) | 1;
    return ix;
}

int coleccion_indice_de(lmk_coleccion *c, const char *nombre)
{
    for (int i = 0; i < c->nix; i++)
        if (strcmp(c->ix[i]->nombre, nombre) == 0) return i;
    return -1;
}

static void quitar_indice(lmk_coleccion *c, int i)
{
    indice_vaciar(c->ix[i]);
    lmk_libera(c->ix[i]);
    c->ix[i] = c->ix[--c->nix];
}

/* ------------------------------------------------------------ claves en los índices */

typedef struct { lmk_indice *ix; uint32_t ranura; bool ok; } ctx_ix_t;

static bool cb_insertar(void *x, const bson_elem *k) { ctx_ix_t *c = x; if (!indice_insertar(c->ix, k, c->ranura)) c->ok = false; return true; }
static bool cb_borrar(void *x, const bson_elem *k) { ctx_ix_t *c = x; indice_borrar(c->ix, k, c->ranura); return true; }
static bool cb_unico(void *x, const bson_elem *k)
{
    ctx_ix_t *c = x;
    if (indice_hay_otro(c->ix, k, c->ranura)) { c->ok = false; return false; }
    return true;
}

static void claves_poner(lmk_coleccion *c, const uint8_t *doc, uint32_t s)
{
    for (int i = 0; i < c->nix; i++) {
        ctx_ix_t x = { c->ix[i], s, true };
        indice_claves(c->ix[i], doc, cb_insertar, &x);
    }
}

static void claves_quitar(lmk_coleccion *c, const uint8_t *doc, uint32_t s)
{
    for (int i = 0; i < c->nix; i++) {
        ctx_ix_t x = { c->ix[i], s, true };
        indice_claves(c->ix[i], doc, cb_borrar, &x);
    }
}

static bool error_duplicado(lmk_coleccion *c, const char *indice, lmk_error *err)
{
    char msg[200];
    lmk_fmt(msg, sizeof msg, "E11000 clave duplicada: colección %s.%s, índice %s", c->base->nombre, c->nombre, indice);
    return lmk_fallo(err, ERR_DUPLICADO, msg);
}

/* ¿El documento choca con algún índice único? (s = su ranura, o UINT32_MAX) */
static bool unicos_ok(lmk_coleccion *c, const uint8_t *doc, uint32_t s, lmk_error *err)
{
    for (int i = 0; i < c->nix; i++) {
        if (!c->ix[i]->unico) continue;
        ctx_ix_t x = { c->ix[i], s, true };
        indice_claves(c->ix[i], doc, cb_unico, &x);
        if (!x.ok) return error_duplicado(c, c->ix[i]->nombre, err);
    }
    return true;
}

/* ------------------------------------------------------------ apertura */

typedef struct {
    int      fd;
    uint8_t *b;
    uint32_t cap, ini, fin;      /* bytes válidos: b[ini..fin) */
    uint32_t off;                /* desplazamiento en el archivo de b[0] */
    uint32_t tam;
} lector_t;

/* Asegura n bytes desde la posición "pos" del archivo; devuelve el puntero. */
static const uint8_t *lector_pedir(lector_t *l, uint32_t pos, uint32_t n)
{
    if (pos >= l->off + l->ini && pos + n <= l->off + l->fin) return l->b + (pos - l->off);
    if (pos + n > l->tam) return NULL;
    uint32_t quiero = n > 1024 * 1024 ? n : 1024 * 1024;
    if (quiero > l->tam - pos) quiero = l->tam - pos;
    if (quiero > l->cap) {
        uint8_t *nb = lmk_realoca(l->b, quiero);
        if (!nb) return NULL;
        l->b = nb;
        l->cap = quiero;
    }
    int r = plat_leer_en(l->fd, pos, l->b, quiero);
    if (r < (int)n) return NULL;
    l->off = pos;
    l->ini = 0;
    l->fin = (uint32_t)r;
    return l->b;
}

static bool reescribir(lmk_coleccion *c, lmk_error *err);

static bool aplicar_registro(lmk_coleccion *c, uint8_t tipo, const uint8_t *doc, uint32_t len, uint32_t off_doc)
{
    lmk_motor *m = c->base->motor;
    bson_elem id;
    if (tipo == REG_INDICE) {
        lmk_indice *ix = indice_desde_spec(doc);
        if (!ix) return false;
        int i = coleccion_indice_de(c, ix->nombre);
        if (i >= 0) quitar_indice(c, i);
        if (c->nix >= LMK_INDICES_MAX) { lmk_libera(ix); return false; }
        c->ix[c->nix++] = ix;
        c->vivos += len + REG_EXTRA;
        return true;
    }
    if (tipo == REG_BORRAR_INDICE) {
        bson_elem n;
        if (bson_buscar(doc, "name", &n) && n.tipo == BT_STRING) {
            int i = coleccion_indice_de(c, bson_cadena(&n));
            if (i >= 0) quitar_indice(c, i);
        }
        return true;
    }
    if (!id_de(doc, &id)) return false;
    uint32_t h = bson_hash(id.tipo, id.v, id.vlen);
    int64_t s = tabla_buscar(c, &id, h);
    if (tipo == REG_BORRAR) {
        if (s >= 0) {
            lmk_ranura *r = &c->r[s];
            cache_soltar(c, r);
            c->vivos -= r->rlen;
            tabla_quitar(c, (uint32_t)s);
            r->viva = 0;
            ranura_soltar(c, (uint32_t)s);
            c->vivas--;
        }
        return true;
    }
    if (tipo != REG_INSERTAR && tipo != REG_ACTUALIZAR) return false;
    lmk_ranura *r;
    if (s >= 0) {
        r = &c->r[s];
        cache_soltar(c, r);
        c->vivos -= r->rlen;
    } else {
        s = ranura_nueva(c);
        if (s < 0) return false;
        r = &c->r[s];
        r->viva = 1;
        r->hash = h;
        r->gen = ++c->gen_sig;
        if (!tabla_poner(c, (uint32_t)s)) return false;
        c->vivas++;
    }
    r->len = len;
    r->off = off_doc;
    r->rlen = len + REG_EXTRA;
    c->vivos += r->rlen;
    cache_poner(c, r, doc, len);
    (void)m;
    return true;
}

static bool cargar(lmk_coleccion *c, lmk_error *err)
{
    lmk_motor *m = c->base->motor;
    c->cargada = true;
    c->fd = plat_abrir(c->ruta, PLAT_LEER | PLAT_ESCRIBIR | PLAT_CREAR);
    if (c->fd < 0) return lmk_fallo(err, ERR_DISCO, "no se pudo abrir el archivo de la colección");
    uint32_t tam = 0;
    plat_tamano(c->fd, &tam);
    if (tam < CAB_TAM) {
        if (!plat_escribir_en(c->fd, 0, CABECERA, CAB_TAM)) return lmk_fallo(err, ERR_DISCO, "no se pudo escribir en el disco");
        c->fin = c->pendiente_off = CAB_TAM;
        c->vivos = CAB_TAM;
        return true;
    }
    uint8_t cab[CAB_TAM];
    if (plat_leer_en(c->fd, 0, cab, CAB_TAM) != CAB_TAM || memcmp(cab, CABECERA, 4) != 0) {
        plat_cerrar(c->fd);
        c->fd = -1;
        return lmk_fallo(err, ERR_DISCO, "el archivo de la colección no es de lite-monkey");
    }
    lector_t l = { .fd = c->fd, .tam = tam };
    uint32_t pos = CAB_TAM, registros = 0;
    bool corrupto = false;
    c->vivos = CAB_TAM;
    c->pendiente_off = c->fin = tam;   /* todo lo leído está en el archivo */
    while (pos < tam) {
        const uint8_t *h = lector_pedir(&l, pos, 5);
        if (!h) { corrupto = true; break; }
        uint8_t tipo = h[0];
        uint32_t len = lmk_le32(h + 1);
        if (tipo < REG_INSERTAR || tipo > REG_BORRAR_INDICE || len < 5 || len > LMK_DOC_MAX + 64) { corrupto = true; break; }
        const uint8_t *r = lector_pedir(&l, pos, len + REG_EXTRA);
        if (!r) { corrupto = true; break; }
        if (lmk_crc32(0, r, len + 5) != lmk_le32(r + 5 + len)) { corrupto = true; break; }
        if (!bson_validar(r + 5, len, NULL)) { corrupto = true; break; }
        if (!aplicar_registro(c, tipo, r + 5, len, pos + 5)) { corrupto = true; break; }
        pos += len + REG_EXTRA;
        registros++;
    }
    lmk_libera(l.b);
    c->fin = c->pendiente_off = pos;
    /* Los índices se construyen al final, de una vez. */
    for (int i = 0; i < c->nix; i++)
        for (uint32_t s = 0; s < c->nr; s++) {
            if (!c->r[s].viva) continue;
            const uint8_t *d = coleccion_doc(c, s);
            if (!d) continue;
            ctx_ix_t x = { c->ix[i], s, true };
            indice_claves(c->ix[i], d, cb_insertar, &x);
        }
    if (corrupto) {
        char msg[300];
        lmk_fmt(msg, sizeof msg, "aviso: %s.%s: se descartan %u bytes del final del archivo (registro cortado o CRC malo); "
                "se conservan %u registros", c->base->nombre, c->nombre, tam - pos, registros);
        avisar(m, msg);
        if (!reescribir(c, err)) return false;
    }
    return true;
}

/* ------------------------------------------------------------ bases y motor */

static lmk_coleccion *registrar_coleccion(lmk_base *b, const char *nombre)
{
    if (b->ncol == b->capcol) {
        uint32_t cap = b->capcol ? b->capcol * 2 : 8;
        lmk_coleccion **v = lmk_realoca(b->col, cap * sizeof *v);
        if (!v) return NULL;
        b->col = v;
        b->capcol = cap;
    }
    lmk_coleccion *c = lmk_aloca(sizeof *c);
    if (!c) return NULL;
    memset(c, 0, sizeof *c);
    lmk_fmt(c->nombre, sizeof c->nombre, "%s", nombre);
    c->base = b;
    c->fd = -1;
    c->generacion = ++b->motor->generacion;
    unir_ruta(c->ruta, sizeof c->ruta, b->ruta, nombre, ".lmk");
    b->col[b->ncol++] = c;
    return c;
}

static void listar_col(void *ctx, const char *nombre, bool carpeta)
{
    lmk_base *b = ctx;
    size_t n = strlen(nombre);
    if (carpeta) return;
    if (n > 8 && strcmp(nombre + n - 8, ".lmk.tmp") == 0) {   /* una compactación que no terminó */
        char ruta[400];
        unir_ruta(ruta, sizeof ruta, b->ruta, nombre, "");
        plat_borrar(ruta);
        return;
    }
    if (n <= 4 || strcmp(nombre + n - 4, ".lmk") != 0 || n - 4 > LMK_NOMBRE_MAX) return;
    char col[LMK_NOMBRE_MAX + 1];
    memcpy(col, nombre, n - 4);
    col[n - 4] = 0;
    if (nombre_valido(col, true)) registrar_coleccion(b, col);
}

static lmk_base *registrar_base(lmk_motor *m, const char *nombre)
{
    if (m->nbases == m->capbases) {
        uint32_t cap = m->capbases ? m->capbases * 2 : 8;
        lmk_base **v = lmk_realoca(m->bases, cap * sizeof *v);
        if (!v) return NULL;
        m->bases = v;
        m->capbases = cap;
    }
    lmk_base *b = lmk_aloca(sizeof *b);
    if (!b) return NULL;
    memset(b, 0, sizeof *b);
    lmk_fmt(b->nombre, sizeof b->nombre, "%s", nombre);
    b->motor = m;
    unir_ruta(b->ruta, sizeof b->ruta, m->datos, nombre, "");
    m->bases[m->nbases++] = b;
    plat_listar(b->ruta, listar_col, b);
    return b;
}

static void listar_bases(void *ctx, const char *nombre, bool carpeta)
{
    if (carpeta && nombre_valido(nombre, false)) registrar_base(ctx, nombre);
}

bool motor_abrir(lmk_motor *m, const char *datos, uint64_t mem_limite, bool sync, lmk_error *err)
{
    memset(m, 0, sizeof *m);
    lmk_fmt(m->datos, sizeof m->datos, "%s", datos);
    size_t n = strlen(m->datos);
    while (n > 1 && m->datos[n - 1] == '/') m->datos[--n] = 0;
    m->mem_limite = mem_limite;
    m->sync = sync;
    m->sync_cada = 1000;
    m->aviso = plat_escribir_log;
    plat_azar(m->azar_oid, sizeof m->azar_oid);
    plat_azar(&m->contador_oid, sizeof m->contador_oid);
    if (!plat_crear_carpeta(m->datos)) return lmk_fallo(err, ERR_DISCO, "no se pudo crear la carpeta de datos");
    if (!plat_listar(m->datos, listar_bases, m)) return lmk_fallo(err, ERR_DISCO, "no se pudo leer la carpeta de datos");
    return true;
}

static void liberar_coleccion(lmk_coleccion *c)
{
    for (uint32_t s = 0; s < c->nr; s++) cache_soltar(c, &c->r[s]);
    while (c->nix) quitar_indice(c, c->nix - 1);
    lmk_libera(c->r);
    lmk_libera(c->libres);
    lmk_libera(c->tabla);
    buf_liberar(&c->pendiente);
    buf_liberar(&c->temporal);
    if (c->fd >= 0) plat_cerrar(c->fd);
    lmk_libera(c);
}

void motor_cerrar(lmk_motor *m)
{
    motor_sincronizar(m, true);
    for (uint32_t i = 0; i < m->nbases; i++) {
        lmk_base *b = m->bases[i];
        for (uint32_t k = 0; k < b->ncol; k++) liberar_coleccion(b->col[k]);
        lmk_libera(b->col);
        lmk_libera(b);
    }
    lmk_libera(m->bases);
    m->bases = NULL;
    m->nbases = 0;
}

void motor_sincronizar(lmk_motor *m, bool forzar)
{
    for (uint32_t i = 0; i < m->nbases; i++) {
        lmk_base *b = m->bases[i];
        for (uint32_t k = 0; k < b->ncol; k++) {
            lmk_coleccion *c = b->col[k];
            if (!c->cargada || c->fd < 0) continue;
            coleccion_volcar(c);
            /* Más de la mitad del archivo es basura: compactar. */
            if (c->fin > COMPACTAR_MIN && c->fin - c->vivos > c->fin / 2 && !c->error_disco) {
                lmk_error e;
                if (!coleccion_compactar(c, &e)) avisar(m, e.msg);
            }
            if (c->sin_sync && (forzar || m->sync || c->sin_sync >= m->sync_cada)) {
                plat_sincronizar(c->fd);
                c->sin_sync = 0;
            }
        }
    }
}

void motor_oid(lmk_motor *m, uint8_t oid[12])
{
    uint32_t t = (uint32_t)(plat_epoca_ms() / 1000u);
    oid[0] = (uint8_t)(t >> 24); oid[1] = (uint8_t)(t >> 16); oid[2] = (uint8_t)(t >> 8); oid[3] = (uint8_t)t;
    memcpy(oid + 4, m->azar_oid, 5);
    uint32_t k = ++m->contador_oid;
    oid[9] = (uint8_t)(k >> 16); oid[10] = (uint8_t)(k >> 8); oid[11] = (uint8_t)k;
}

lmk_base *motor_base(lmk_motor *m, const char *nombre, bool crear, lmk_error *err)
{
    for (uint32_t i = 0; i < m->nbases; i++)
        if (strcmp(m->bases[i]->nombre, nombre) == 0) return m->bases[i];
    if (!nombre_valido(nombre, false)) {
        lmk_fallo(err, ERR_VALOR, "nombre de base no válido (letras, números, _ y -; hasta 64)");
        return NULL;
    }
    if (!crear) { lmk_fallo(err, ERR_NS_NO_EXISTE, "la base no existe"); return NULL; }
    lmk_base *b = registrar_base(m, nombre);
    if (!b) { lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria"); return NULL; }
    if (!plat_crear_carpeta(b->ruta)) {
        lmk_fallo(err, ERR_DISCO, "no se pudo crear la carpeta de la base");
        m->nbases--;
        lmk_libera(b);
        return NULL;
    }
    return b;
}

lmk_coleccion *base_coleccion(lmk_base *b, const char *nombre, bool crear, lmk_error *err)
{
    lmk_coleccion *c = NULL;
    for (uint32_t i = 0; i < b->ncol; i++)
        if (strcmp(b->col[i]->nombre, nombre) == 0) { c = b->col[i]; break; }
    if (!c) {
        if (!nombre_valido(nombre, true)) {
            lmk_fallo(err, ERR_VALOR, "nombre de colección no válido (letras, números, _, - y .; hasta 64)");
            return NULL;
        }
        if (!crear) { lmk_fallo(err, ERR_NS_NO_EXISTE, "la colección no existe"); return NULL; }
        if (!plat_crear_carpeta(b->ruta)) { lmk_fallo(err, ERR_DISCO, "no se pudo crear la carpeta de la base"); return NULL; }
        c = registrar_coleccion(b, nombre);
        if (!c) { lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria"); return NULL; }
    }
    if (!c->cargada && !cargar(c, err)) return NULL;
    return c;
}

bool base_borrar_coleccion(lmk_base *b, lmk_coleccion *c, lmk_error *err)
{
    for (uint32_t i = 0; i < b->ncol; i++) {
        if (b->col[i] != c) continue;
        if (c->fd >= 0) { plat_cerrar(c->fd); c->fd = -1; }
        if (!plat_borrar(c->ruta)) {
            int fd = plat_abrir(c->ruta, PLAT_LEER);
            if (fd >= 0) { plat_cerrar(fd); return lmk_fallo(err, ERR_DISCO, "no se pudo borrar el archivo de la colección"); }
        }
        b->col[i] = b->col[--b->ncol];
        liberar_coleccion(c);
        return true;
    }
    return lmk_fallo(err, ERR_NS_NO_EXISTE, "la colección no existe");
}

bool motor_borrar_base(lmk_motor *m, lmk_base *b, lmk_error *err)
{
    while (b->ncol)
        if (!base_borrar_coleccion(b, b->col[b->ncol - 1], err)) return false;
    plat_borrar_carpeta(b->ruta);
    for (uint32_t i = 0; i < m->nbases; i++)
        if (m->bases[i] == b) { m->bases[i] = m->bases[--m->nbases]; break; }
    lmk_libera(b->col);
    lmk_libera(b);
    return true;
}

uint64_t base_tamano(lmk_base *b)
{
    uint64_t t = 0;
    for (uint32_t i = 0; i < b->ncol; i++) {
        lmk_coleccion *c = b->col[i];
        if (c->cargada) t += c->fin;
        else {
            int fd = plat_abrir(c->ruta, PLAT_LEER);
            uint32_t n = 0;
            if (fd >= 0) { plat_tamano(fd, &n); plat_cerrar(fd); }
            t += n;
        }
    }
    return t;
}

/* ------------------------------------------------------------ escrituras */

bool coleccion_insertar(lmk_coleccion *c, const uint8_t *doc, uint8_t *id_tipo, lmk_buf *id_out, lmk_error *err)
{
    lmk_motor *m = c->base->motor;
    if (c->error_disco) return lmk_fallo(err, ERR_DISCO, "la colección está en solo lectura por un error de disco");
    if (bson_tam(doc) > LMK_DOC_MAX) return lmk_fallo(err, ERR_ES, "el documento pasa de 1 MB");
    bson_it it;
    bson_elem e, id;
    bool tiene_id = false;
    bson_it_iniciar(&it, doc);
    while (bson_it_sig(&it, &e)) {
        if (e.nombre[0] == '$') return lmk_fallo(err, ERR_VALOR, "un campo no puede empezar por $");
        if (nombre_es(&e, "_id")) { tiene_id = true; id = e; }
    }
    lmk_buf con_id = { 0 };
    if (!tiene_id) {   /* se genera un ObjectId y va el primero */
        uint8_t oid[12];
        motor_oid(m, oid);
        uint32_t ini = bson_inicio(&con_id);
        bson_clave(&con_id, BT_OID, "_id", 3);
        buf_poner(&con_id, oid, 12);
        buf_poner(&con_id, doc + 4, bson_tam(doc) - 4);
        bson_fin(&con_id, ini);
        con_id.n--;   /* bson_fin añadió otro 0: el del documento original ya cierra */
        if (con_id.sin_memoria) return lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
        lmk_pon32(con_id.p, con_id.n);
        doc = con_id.p;
        id_de(doc, &id);
    } else if (id.tipo == BT_ARRAY || id.tipo == BT_REGEX) {
        return lmk_fallo(err, ERR_VALOR, "el _id no puede ser un array ni una regex");
    }
    uint32_t len = bson_tam(doc);
    bool ok = false;
    uint32_t h = bson_hash(id.tipo, id.v, id.vlen);
    if (tabla_buscar(c, &id, h) >= 0) {
        error_duplicado(c, "_id_", err);
        goto fin;
    }
    if (!unicos_ok(c, doc, UINT32_MAX, err)) goto fin;
    if (id_out) { id_out->n = 0; buf_poner(id_out, id.v, id.vlen); if (id_tipo) *id_tipo = id.tipo; }
    int64_t s = ranura_nueva(c);
    if (s < 0) { lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria"); goto fin; }
    lmk_ranura *r = &c->r[s];
    uint32_t off = escribir_registro(c, REG_INSERTAR, doc, len);
    if (!off) { c->nr--; lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria"); goto fin; }
    r->viva = 1;
    r->hash = h;
    r->gen = ++c->gen_sig;
    r->len = len;
    r->off = off;
    r->rlen = len + REG_EXTRA;
    cache_poner(c, r, doc, len);
    tabla_poner(c, (uint32_t)s);
    claves_poner(c, doc, (uint32_t)s);
    c->vivas++;
    c->vivos += r->rlen;
    m->insertados++;
    ok = true;
fin:
    buf_liberar(&con_id);
    return ok;
}

bool coleccion_cambiar(lmk_coleccion *c, uint32_t s, const uint8_t *nuevo, lmk_error *err)
{
    if (c->error_disco) return lmk_fallo(err, ERR_DISCO, "la colección está en solo lectura por un error de disco");
    uint32_t len = bson_tam(nuevo);
    if (len > LMK_DOC_MAX) return lmk_fallo(err, ERR_ES, "el documento pasa de 1 MB");
    if (!unicos_ok(c, nuevo, s, err)) return false;
    const uint8_t *viejo = coleccion_doc(c, s);
    if (!viejo) return lmk_fallo(err, ERR_DISCO, "no se pudo leer el documento");
    claves_quitar(c, viejo, s);
    uint32_t off = escribir_registro(c, REG_ACTUALIZAR, nuevo, len);
    lmk_ranura *r = &c->r[s];
    if (!off) {
        if ((viejo = coleccion_doc(c, s)) != NULL) claves_poner(c, viejo, s);
        return lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
    }
    cache_soltar(c, r);
    c->vivos -= r->rlen;
    r->len = len;
    r->off = off;
    r->rlen = len + REG_EXTRA;
    c->vivos += r->rlen;
    cache_poner(c, r, nuevo, len);
    claves_poner(c, nuevo, s);
    c->base->motor->actualizados++;
    return true;
}

bool coleccion_quitar(lmk_coleccion *c, uint32_t s, lmk_error *err)
{
    if (c->error_disco) return lmk_fallo(err, ERR_DISCO, "la colección está en solo lectura por un error de disco");
    const uint8_t *viejo = coleccion_doc(c, s);
    bson_elem id;
    if (!viejo || !id_de(viejo, &id)) return lmk_fallo(err, ERR_DISCO, "no se pudo leer el documento");
    lmk_buf b = { 0 };
    uint32_t ini = bson_inicio(&b);
    bson_poner_elem(&b, "_id", 3, &id);
    bson_fin(&b, ini);
    if (b.sin_memoria) return lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
    claves_quitar(c, viejo, s);
    tabla_quitar(c, s);
    escribir_registro(c, REG_BORRAR, b.p, b.n);
    buf_liberar(&b);
    lmk_ranura *r = &c->r[s];
    cache_soltar(c, r);
    c->vivos -= r->rlen;
    r->viva = 0;
    ranura_soltar(c, s);
    c->vivas--;
    c->base->motor->borrados++;
    return true;
}

/* ------------------------------------------------------------ índices */

bool coleccion_crear_indice(lmk_coleccion *c, const char *nombre, const char *campo, int dir, bool unico,
                            bool *ya_estaba, lmk_error *err)
{
    *ya_estaba = false;
    if (strcmp(campo, "_id") == 0) { *ya_estaba = true; return true; }
    for (int i = 0; i < c->nix; i++) {
        if (strcmp(c->ix[i]->campo, campo) == 0 && c->ix[i]->dir == dir && c->ix[i]->unico == unico) {
            *ya_estaba = true;
            return true;
        }
        if (strcmp(c->ix[i]->nombre, nombre) == 0)
            return lmk_fallo(err, ERR_INDICE_EXISTE, "ya hay otro índice con ese nombre");
    }
    if (c->nix >= LMK_INDICES_MAX) return lmk_fallo(err, ERR_VALOR, "demasiados índices (máximo 16 por colección)");
    if (strlen(nombre) >= LMK_NOMBRE_MAX || strlen(campo) >= 128 || !strlen(campo) || campo[0] == '$')
        return lmk_fallo(err, ERR_VALOR, "nombre de índice o campo no válido");
    lmk_buf spec = { 0 };
    lmk_indice tmp;
    memset(&tmp, 0, sizeof tmp);
    lmk_fmt(tmp.nombre, sizeof tmp.nombre, "%s", nombre);
    lmk_fmt(tmp.campo, sizeof tmp.campo, "%s", campo);
    tmp.dir = dir;
    tmp.unico = unico;
    escribir_spec(&spec, &tmp);
    lmk_indice *ix = spec.sin_memoria ? NULL : indice_desde_spec(spec.p);
    if (!ix) { buf_liberar(&spec); return lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria"); }
    for (uint32_t s = 0; s < c->nr; s++) {
        if (!c->r[s].viva) continue;
        const uint8_t *d = coleccion_doc(c, s);
        if (!d) continue;
        ctx_ix_t x = { ix, s, true };
        if (unico) {
            indice_claves(ix, d, cb_unico, &x);
            if (!x.ok) {
                indice_vaciar(ix);
                lmk_libera(ix);
                buf_liberar(&spec);
                return error_duplicado(c, nombre, err);
            }
        }
        indice_claves(ix, d, cb_insertar, &x);
    }
    c->ix[c->nix++] = ix;
    escribir_registro(c, REG_INDICE, spec.p, spec.n);
    c->vivos += spec.n + REG_EXTRA;
    buf_liberar(&spec);
    return true;
}

bool coleccion_borrar_indice(lmk_coleccion *c, const char *nombre, lmk_error *err)
{
    int i = coleccion_indice_de(c, nombre);
    if (i < 0) return lmk_fallo(err, ERR_INDICE_NO_EXISTE, "el índice no existe");
    lmk_buf b = { 0 };
    uint32_t ini = bson_inicio(&b);
    bson_poner_texto(&b, "name", nombre);
    bson_fin(&b, ini);
    escribir_registro(c, REG_BORRAR_INDICE, b.p, b.n);
    buf_liberar(&b);
    quitar_indice(c, i);
    return true;
}

/* ------------------------------------------------------------ compactación */

/* Escribe todo lo vivo en "ruta.tmp", lo sincroniza y lo renombra encima. */
static bool reescribir(lmk_coleccion *c, lmk_error *err)
{
    char tmp[400];
    lmk_fmt(tmp, sizeof tmp, "%s.tmp", c->ruta);
    coleccion_volcar(c);
    int fd = plat_abrir(tmp, PLAT_LEER | PLAT_ESCRIBIR | PLAT_CREAR | PLAT_TRUNCAR);
    if (fd < 0) return lmk_fallo(err, ERR_DISCO, "compactar: no se pudo crear el archivo temporal");
    uint32_t *offs = c->nr ? lmk_aloca(c->nr * sizeof *offs) : NULL;
    if (c->nr && !offs) { plat_cerrar(fd); plat_borrar(tmp); return lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria"); }
    lmk_buf b = { 0 }, spec = { 0 };
    uint32_t pos = 0;   /* bytes ya escritos en el archivo nuevo */
    bool ok = true;
    buf_poner(&b, CABECERA, CAB_TAM);
    uint32_t vivos = CAB_TAM;
    for (int i = 0; i < c->nix; i++) {
        spec.n = 0;
        escribir_spec(&spec, c->ix[i]);
        uint32_t ini = b.n;
        buf_byte(&b, REG_INDICE);
        buf_u32(&b, spec.n);
        buf_poner(&b, spec.p, spec.n);
        if (b.sin_memoria) break;
        buf_u32(&b, lmk_crc32(0, b.p + ini, spec.n + 5));
        vivos += spec.n + REG_EXTRA;
    }
    for (uint32_t s = 0; s < c->nr && ok && !b.sin_memoria; s++) {
        if (!c->r[s].viva) continue;
        const uint8_t *d = coleccion_doc(c, s);
        if (!d) { ok = false; break; }
        uint32_t len = c->r[s].len, ini = b.n;
        buf_byte(&b, REG_INSERTAR);
        buf_u32(&b, len);
        buf_poner(&b, d, len);
        if (b.sin_memoria) break;
        buf_u32(&b, lmk_crc32(0, b.p + ini, len + 5));
        offs[s] = pos + ini + 5;
        vivos += len + REG_EXTRA;
        if (b.n >= PENDIENTE_MAX) {
            ok = plat_escribir_en(fd, pos, b.p, b.n);
            pos += b.n;
            b.n = 0;
        }
    }
    if (b.sin_memoria) ok = false;
    if (ok && b.n) { ok = plat_escribir_en(fd, pos, b.p, b.n); pos += b.n; }
    if (ok) ok = plat_sincronizar(fd);
    plat_cerrar(fd);
    buf_liberar(&b);
    buf_liberar(&spec);
    if (!ok) {
        plat_borrar(tmp);
        lmk_libera(offs);
        return lmk_fallo(err, ERR_DISCO, "compactar: no se pudo escribir el archivo temporal");
    }
    if (c->fd >= 0) plat_cerrar(c->fd);
    c->fd = -1;
    if (!plat_renombrar(tmp, c->ruta)) {
        c->fd = plat_abrir(c->ruta, PLAT_LEER | PLAT_ESCRIBIR);
        plat_borrar(tmp);
        lmk_libera(offs);
        return lmk_fallo(err, ERR_DISCO, "compactar: no se pudo renombrar el archivo");
    }
    c->fd = plat_abrir(c->ruta, PLAT_LEER | PLAT_ESCRIBIR);
    for (uint32_t s = 0; s < c->nr; s++)
        if (c->r[s].viva) { c->r[s].off = offs[s]; c->r[s].rlen = c->r[s].len + REG_EXTRA; }
    lmk_libera(offs);
    c->fin = c->pendiente_off = pos;
    c->vivos = vivos;
    c->sin_sync = 0;
    if (c->fd < 0) { c->error_disco = true; return lmk_fallo(err, ERR_DISCO, "compactar: no se pudo reabrir el archivo"); }
    return true;
}

bool coleccion_compactar(lmk_coleccion *c, lmk_error *err)
{
    if (c->error_disco) return lmk_fallo(err, ERR_DISCO, "la colección está en solo lectura por un error de disco");
    return reescribir(c, err);
}

/* ------------------------------------------------------------ consultas */

typedef struct {
    lmk_coleccion *c;
    const uint8_t *filtro;
    uint32_t      *v;
    uint32_t       n, cap, max;
    uint8_t       *visto;        /* una marca por ranura (índices multiclave, $in) */
    lmk_plan      *plan;
    bool           sin_memoria;
} busqueda_t;

static bool probar(void *ctx, uint32_t s)
{
    busqueda_t *b = ctx;
    if (s >= b->c->nr || !b->c->r[s].viva) return false;
    if (b->visto) {
        if (b->visto[s >> 3] & (1u << (s & 7))) return false;
        b->visto[s >> 3] |= (uint8_t)(1u << (s & 7));
    }
    const uint8_t *d = coleccion_doc(b->c, s);
    if (!d) return false;
    b->plan->examinados++;
    if (!consulta_coincide(d, b->filtro)) return false;
    if (b->n == b->cap) {
        uint32_t cap = b->cap ? b->cap * 2 : 64;
        uint32_t *v = lmk_realoca(b->v, cap * sizeof *v);
        if (!v) { b->sin_memoria = true; return true; }
        b->v = v;
        b->cap = cap;
    }
    b->v[b->n++] = s;
    return b->max && b->n >= b->max;
}

/* Un valor apto para buscarlo en un índice: ni regex, ni array, ni documento de operadores. */
static bool valor_simple(const bson_elem *e)
{
    if (e->tipo == BT_REGEX || e->tipo == BT_ARRAY) return false;
    if (e->tipo == BT_DOC && !bson_vacio(e->v) && e->v[5] == '$') return false;
    return true;
}

static const uint8_t MAS_INF[8] = { 0, 0, 0, 0, 0, 0, 0xF0, 0x7F }, MENOS_INF[8] = { 0, 0, 0, 0, 0, 0, 0xF0, 0xFF };
static const uint8_t FECHA_MIN[8] = { 0, 0, 0, 0, 0, 0, 0, 0x80 }, FECHA_MAX[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F };

/* Límites de un rango: sin uno de los lados, el extremo de la misma clase de tipo. */
static void extremo(const bson_elem *ref, bool alto, bson_elem *out)
{
    out->vlen = 8;
    if (bson_es_numero(ref->tipo)) { out->tipo = BT_DOUBLE; out->v = alto ? MAS_INF : MENOS_INF; return; }
    if (ref->tipo == BT_FECHA) { out->tipo = BT_FECHA; out->v = alto ? FECHA_MAX : FECHA_MIN; return; }
    out->tipo = 0;
}

/* Intenta resolver el filtro con un índice; true si lo hizo. */
static bool por_indice(busqueda_t *b)
{
    lmk_coleccion *c = b->c;
    bson_it it;
    bson_elem e;
    /* 1. _id por la tabla hash */
    if (bson_buscar(b->filtro, "_id", &e)) {
        bson_elem v = e, lista;
        bool ok = valor_simple(&e);
        if (!ok && e.tipo == BT_DOC && bson_contar(e.v) == 1 && bson_buscar(e.v, "$eq", &v) && valor_simple(&v)) ok = true;
        if (ok) {
            b->plan->plan = "IDHACK";
            b->plan->indice = "_id_";
            int64_t s = tabla_buscar(c, &v, bson_hash(v.tipo, v.v, v.vlen));
            b->plan->claves = 1;
            if (s >= 0) probar(b, (uint32_t)s);
            return true;
        }
        if (e.tipo == BT_DOC && bson_contar(e.v) == 1 && bson_buscar(e.v, "$in", &lista)) {
            b->plan->plan = "IDHACK";
            b->plan->indice = "_id_";
            bson_it il;
            bson_elem x;
            bson_it_iniciar(&il, lista.v);
            while (bson_it_sig(&il, &x)) {
                if (!valor_simple(&x)) continue;
                b->plan->claves++;
                int64_t s = tabla_buscar(c, &x, bson_hash(x.tipo, x.v, x.vlen));
                if (s >= 0 && probar(b, (uint32_t)s)) break;
            }
            return true;
        }
    }
    /* 2. índices secundarios: primero igualdad, luego rango */
    for (int pasada = 0; pasada < 2; pasada++) {
        bson_it_iniciar(&it, b->filtro);
        while (bson_it_sig(&it, &e)) {
            if (e.nombre[0] == '$') continue;
            lmk_indice *ix = NULL;
            for (int i = 0; i < c->nix; i++)
                if (strcmp(c->ix[i]->campo, e.nombre) == 0) { ix = c->ix[i]; break; }
            if (!ix) continue;
            bson_elem lo, hi, x;
            bool hay_lo = false, hay_hi = false, lo_incl = true, hi_incl = true, igualdad = false;
            bson_elem lista = { 0 };
            if (valor_simple(&e)) { lo = hi = e; hay_lo = hay_hi = igualdad = true; }
            else if (e.tipo == BT_DOC && !bson_vacio(e.v) && e.v[5] == '$') {
                bson_it io;
                bson_it_iniciar(&io, e.v);
                while (bson_it_sig(&io, &x)) {
                    if (!valor_simple(&x) && !nombre_es(&x, "$in")) continue;
                    if (nombre_es(&x, "$eq")) { lo = hi = x; hay_lo = hay_hi = igualdad = true; }
                    else if (nombre_es(&x, "$in") && x.tipo == BT_ARRAY) { lista = x; igualdad = true; }
                    else if (nombre_es(&x, "$gt") || nombre_es(&x, "$gte")) { lo = x; hay_lo = true; lo_incl = x.nombre[3] == 'e'; }
                    else if (nombre_es(&x, "$lt") || nombre_es(&x, "$lte")) { hi = x; hay_hi = true; hi_incl = x.nombre[3] == 'e'; }
                }
            }
            if (pasada == 0 && !igualdad) continue;
            if (pasada == 1 && !hay_lo && !hay_hi) continue;
            b->plan->plan = "IXSCAN";
            b->plan->indice = ix->nombre;
            if (!b->visto) {
                b->visto = lmk_aloca((c->nr + 7) / 8 + 1);
                if (b->visto) memset(b->visto, 0, (c->nr + 7) / 8 + 1);
            }
            if (lista.tipo == BT_ARRAY && !hay_lo) {
                bson_it il;
                bson_elem v;
                bson_it_iniciar(&il, lista.v);
                bool simple = true;
                while (bson_it_sig(&il, &v)) if (!valor_simple(&v)) simple = false;
                if (!simple) { b->plan->plan = NULL; continue; }
                bson_it_iniciar(&il, lista.v);
                while (bson_it_sig(&il, &v))
                    b->plan->claves += indice_recorrer(ix, &v, true, &v, true, probar, b);
                return true;
            }
            if (hay_lo && hay_hi && bson_rango(lo.tipo) != bson_rango(hi.tipo)) { b->plan->plan = NULL; continue; }
            bson_elem ext;
            if (!hay_lo) { extremo(&hi, false, &ext); if (ext.tipo) { lo = ext; hay_lo = true; lo_incl = true; } }
            if (!hay_hi) { extremo(&lo, true, &ext); if (ext.tipo) { hi = ext; hay_hi = true; hi_incl = true; } }
            if (lo.tipo == BT_NULL && hay_lo && !igualdad) { b->plan->plan = NULL; continue; }
            b->plan->claves += indice_recorrer(ix, hay_lo ? &lo : NULL, lo_incl, hay_hi ? &hi : NULL, hi_incl, probar, b);
            return true;
        }
    }
    return false;
}

uint32_t *coleccion_buscar(lmk_coleccion *c, const uint8_t *filtro, uint32_t max, uint32_t *n,
                           lmk_plan *plan, lmk_error *err)
{
    static const uint8_t vacio[5] = { 5, 0, 0, 0, 0 };
    lmk_plan p0;
    if (!plan) plan = &p0;
    memset(plan, 0, sizeof *plan);
    *n = 0;
    if (!filtro) filtro = vacio;
    if (!consulta_validar(filtro, err)) return NULL;
    busqueda_t b = { .c = c, .filtro = filtro, .max = max, .plan = plan };
    c->base->motor->consultas++;
    if (!por_indice(&b)) {
        plan->plan = "COLLSCAN";
        plan->indice = NULL;
        for (uint32_t s = 0; s < c->nr; s++)
            if (c->r[s].viva && probar(&b, s)) break;
    }
    lmk_libera(b.visto);
    if (b.sin_memoria) {
        lmk_libera(b.v);
        lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria para el resultado");
        return NULL;
    }
    *n = b.n;
    if (!b.v) b.v = lmk_aloca(sizeof(uint32_t));   /* nunca NULL si fue bien */
    if (!b.v) lmk_fallo(err, ERR_SIN_MEMORIA, "sin memoria");
    return b.v;
}
