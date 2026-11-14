// SPDX-License-Identifier: GPL-2.0-or-later
/* teste_nucleo.c - pruebas unitarias del núcleo de lite-monkey (en el PC).
 *
 * BSON, comparación, regex, consultas, actualizaciones, índices, agregación,
 * recuperación tras un archivo cortado y compactación. Los documentos se
 * escriben en un JSON sencillo que se convierte a BSON aquí mismo. */
#define _DEFAULT_SOURCE
#include "../nucleo/lmk.h"
#include "../plataforma.h"
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

static int fallos, pruebas;

#define OK(c, ...) do { pruebas++; if (!(c)) { fallos++; printf("FALLO %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ------------------------------------------------------------ JSON -> BSON */

static const char *j;
static void blancos(void) { while (*j == ' ' || *j == '\n' || *j == '\t') j++; }
static void valor(lmk_buf *b, const char *nombre);

static void objeto(lmk_buf *b, bool array)
{
    uint32_t ini = bson_inicio(b);
    j++;
    blancos();
    int i = 0;
    while (*j && *j != (array ? ']' : '}')) {
        char nombre[128];
        if (array) snprintf(nombre, sizeof nombre, "%d", i++);
        else {
            blancos();
            j++;   /* " */
            int n = 0;
            while (*j != '"') nombre[n++] = *j++;
            nombre[n] = 0;
            j++;
            blancos();
            j++;   /* : */
        }
        blancos();
        valor(b, nombre);
        blancos();
        if (*j == ',') j++;
        blancos();
    }
    j++;
    bson_fin(b, ini);
}

static void valor(lmk_buf *b, const char *nombre)
{
    blancos();
    if (*j == '{') {
        /* {"$oid": "..."} */
        if (!strncmp(j, "{\"$oid\"", 7)) {
            j = strchr(j, ':') + 1;
            blancos();
            j++;
            uint8_t oid[12];
            for (int i = 0; i < 12; i++) { unsigned v; sscanf(j + 2 * i, "%2x", &v); oid[i] = (uint8_t)v; }
            j += 25;
            blancos();
            j++;
            bson_clave(b, BT_OID, nombre, (uint32_t)strlen(nombre));
            buf_poner(b, oid, 12);
            return;
        }
        bson_clave(b, BT_DOC, nombre, (uint32_t)strlen(nombre));
        objeto(b, false);
    } else if (*j == '[') {
        bson_clave(b, BT_ARRAY, nombre, (uint32_t)strlen(nombre));
        objeto(b, true);
    } else if (*j == '"') {
        j++;
        char s[512];
        int n = 0;
        while (*j != '"') s[n++] = *j++;
        j++;
        bson_poner_cadena(b, nombre, s, (uint32_t)n);
    } else if (!strncmp(j, "true", 4)) { j += 4; bson_poner_bool(b, nombre, true); }
    else if (!strncmp(j, "false", 5)) { j += 5; bson_poner_bool(b, nombre, false); }
    else if (!strncmp(j, "null", 4)) { j += 4; bson_poner_null(b, nombre); }
    else if (*j == '/') {   /* /patrón/opciones */
        j++;
        bson_clave(b, BT_REGEX, nombre, (uint32_t)strlen(nombre));
        while (*j != '/') buf_byte(b, (uint8_t)*j++);
        buf_byte(b, 0);
        j++;
        while (*j >= 'a' && *j <= 'z') buf_byte(b, (uint8_t)*j++);
        buf_byte(b, 0);
    } else {
        char *fin;
        const char *ini = j;
        long long v = strtoll(j, &fin, 10);
        if (*fin == '.' || *fin == 'e') { double d = strtod(ini, &fin); bson_poner_double(b, nombre, d); }
        else if (*fin == 'L') { bson_poner_int64(b, nombre, v); fin++; }
        else bson_poner_entero(b, nombre, v);
        j = fin;
    }
}

/* Un documento nuevo (se libera con free). */
static uint8_t *D(const char *json)
{
    lmk_buf b = { 0 };
    j = json;
    blancos();
    objeto(&b, false);
    lmk_error e;
    if (!bson_validar(b.p, b.n, &e)) { printf("JSON de prueba mal: %s (%s)\n", json, e.msg); exit(2); }
    return b.p;
}

static bool iguales(const uint8_t *a, const uint8_t *b)
{
    return bson_tam(a) == bson_tam(b) && memcmp(a, b, bson_tam(a)) == 0;
}

/* ------------------------------------------------------------ pruebas */

static void prueba_bson(void)
{
    uint8_t *d = D("{\"a\": 1, \"b\": \"hola\", \"c\": {\"d\": [1, 2.5, \"x\"]}, \"e\": null, \"f\": 5000000000}");
    bson_elem e;
    OK(bson_buscar(d, "b", &e) && e.tipo == BT_STRING && !strcmp(bson_cadena(&e), "hola"), "bson_buscar cadena");
    OK(bson_buscar(d, "f", &e) && e.tipo == BT_INT64 && bson_int64(e.v) == 5000000000LL, "int64");
    OK(bson_camino_simple(d, "c.d.1", 5, &e) && e.tipo == BT_DOUBLE && bson_a_double(&e) == 2.5, "camino c.d.1");
    OK(bson_contar(d) == 5, "contar");
    lmk_error err;
    uint8_t malo[16];
    memcpy(malo, d, 16);
    lmk_pon32(malo, 15);
    OK(!bson_validar(malo, 16, &err), "tamaño incoherente rechazado");
    /* 33 niveles de anidamiento */
    char prof[300] = "";
    for (int i = 0; i < 33; i++) strcat(prof, "{\"a\":");
    strcat(prof, "1");
    for (int i = 0; i < 33; i++) strcat(prof, "}");
    lmk_buf b = { 0 };
    j = prof;
    objeto(&b, false);
    OK(!bson_validar(b.p, b.n, &err) && strstr(err.msg, "32"), "más de 32 niveles rechazado (%s)", err.msg);
    buf_liberar(&b);
    /* cadena sin el 0 final */
    uint8_t *s = D("{\"s\": \"abc\"}");
    s[bson_tam(s) - 2] = 'x';
    OK(!bson_validar(s, bson_tam(s), &err), "cadena sin terminar rechazada");
    free(s);
    free(d);
}

static void prueba_comparar(void)
{
    uint8_t *d = D("{\"i\": 1, \"l\": 1L, \"d\": 1.0, \"d2\": 1.5, \"s\": \"a\", \"n\": null, \"t\": true, \"o\": {\"x\": 1}}");
    bson_elem i, l, dd, d2, s, n, t, o;
    bson_buscar(d, "i", &i); bson_buscar(d, "l", &l); bson_buscar(d, "d", &dd); bson_buscar(d, "d2", &d2);
    bson_buscar(d, "s", &s); bson_buscar(d, "n", &n); bson_buscar(d, "t", &t); bson_buscar(d, "o", &o);
    OK(elem_comparar(&i, &l) == 0 && elem_comparar(&i, &dd) == 0, "1 == 1L == 1.0");
    OK(elem_comparar(&i, &d2) < 0, "1 < 1.5");
    OK(elem_comparar(&n, &i) < 0 && elem_comparar(&i, &s) < 0 && elem_comparar(&s, &o) < 0 && elem_comparar(&o, &t) < 0,
       "orden de tipos null < número < cadena < documento < bool");
    OK(bson_hash(i.tipo, i.v, i.vlen) == bson_hash(dd.tipo, dd.v, dd.vlen), "hash 1 == hash 1.0");
    free(d);
}

static void prueba_regex(void)
{
    static const struct { const char *pat, *opc, *txt; bool si; } C[] = {
        { "^ab", "", "abc", true }, { "^ab", "", "cab", false }, { "c$", "", "abc", true }, { "a.c", "", "abc", true },
        { "a.*z", "", "a123z", true }, { "a+b", "", "aaab", true }, { "a+b", "", "b", false }, { "colou?r", "", "color", true },
        { "[0-9]+", "", "abc123", true }, { "^[^0-9]+$", "", "abc", true }, { "^[^0-9]+$", "", "ab1", false },
        { "\\d\\d", "", "a12", true }, { "\\w+@\\w+\\.com", "", "yo@sagui.com", true }, { "HOLA", "i", "hola", true },
        { "^.$", "", "\xc3\xb1", true }, { "^b", "m", "a\nb", true }, { "^b", "", "a\nb", false }, { "x\\.y", "", "xzy", false },
        { "^$", "", "", true }, { "^a.*b.*c$", "", "aXXbYYc", true },
    };
    for (unsigned k = 0; k < sizeof C / sizeof C[0]; k++) {
        lmk_regex r;
        lmk_error err;
        bool ok = regex_compilar(&r, C[k].pat, (uint32_t)strlen(C[k].pat), C[k].opc, &err);
        OK(ok && regex_buscar(&r, C[k].txt, (uint32_t)strlen(C[k].txt)) == C[k].si, "regex /%s/%s en '%s'", C[k].pat, C[k].opc, C[k].txt);
    }
    lmk_regex r;
    lmk_error err;
    OK(!regex_compilar(&r, "(a|b)", 5, "", &err), "grupos rechazados con error claro");
}

static void prueba_consultas(void)
{
    uint8_t *doc = D("{\"_id\": 1, \"nombre\": \"Ana\", \"edad\": 30, \"tags\": [\"a\", \"b\"], "
                     "\"dir\": {\"ciudad\": \"Lima\", \"cp\": 15001}, \"notas\": [{\"m\": \"x\", \"v\": 7}, {\"m\": \"y\", \"v\": 9}], \"nulo\": null}");
    static const struct { const char *f; bool si; } C[] = {
        { "{}", true }, { "{\"nombre\": \"Ana\"}", true }, { "{\"nombre\": \"Eva\"}", false },
        { "{\"edad\": {\"$gt\": 29}}", true }, { "{\"edad\": {\"$gt\": 30}}", false }, { "{\"edad\": {\"$gte\": 30, \"$lt\": 31}}", true },
        { "{\"edad\": {\"$lte\": 29.5}}", false }, { "{\"edad\": {\"$ne\": 30}}", false }, { "{\"edad\": {\"$in\": [1, 30]}}", true },
        { "{\"edad\": {\"$nin\": [1, 30]}}", false }, { "{\"edad\": {\"$gt\": \"a\"}}", false },
        { "{\"tags\": \"b\"}", true }, { "{\"tags\": [\"a\", \"b\"]}", true }, { "{\"tags\": {\"$in\": [\"z\", \"a\"]}}", true },
        { "{\"tags\": {\"$ne\": \"a\"}}", false }, { "{\"tags\": {\"$size\": 2}}", true }, { "{\"tags\": {\"$all\": [\"a\", \"b\"]}}", true },
        { "{\"dir.ciudad\": \"Lima\"}", true }, { "{\"dir.cp\": {\"$gte\": 15000}}", true }, { "{\"dir.pais\": {\"$exists\": false}}", true },
        { "{\"dir.ciudad\": {\"$exists\": true}}", true }, { "{\"notas.v\": 9}", true }, { "{\"notas.v\": {\"$gt\": 9}}", false },
        { "{\"notas.1.m\": \"y\"}", true }, { "{\"notas\": {\"$elemMatch\": {\"m\": \"x\", \"v\": {\"$gt\": 8}}}}", false },
        { "{\"notas\": {\"$elemMatch\": {\"m\": \"y\", \"v\": {\"$gt\": 8}}}}", true },
        { "{\"$or\": [{\"edad\": 1}, {\"nombre\": \"Ana\"}]}", true }, { "{\"$and\": [{\"edad\": 30}, {\"nombre\": \"Eva\"}]}", false },
        { "{\"$nor\": [{\"edad\": 1}]}", true }, { "{\"nombre\": {\"$regex\": \"^a\", \"$options\": \"i\"}}", true },
        { "{\"nombre\": /n/}", true }, { "{\"nombre\": {\"$not\": /^A/}}", false }, { "{\"edad\": {\"$not\": {\"$gt\": 40}}}", true },
        { "{\"nulo\": null}", true }, { "{\"falta\": null}", true }, { "{\"edad\": null}", false }, { "{\"falta\": {\"$ne\": null}}", false },
        { "{\"_id\": 1.0}", true }, { "{\"edad\": {\"$eq\": 30L}}", true },
    };
    for (unsigned k = 0; k < sizeof C / sizeof C[0]; k++) {
        uint8_t *f = D(C[k].f);
        lmk_error err;
        bool v = consulta_validar(f, &err);
        OK(v && consulta_coincide(doc, f) == C[k].si, "filtro %s (válido=%d)", C[k].f, v);
        free(f);
    }
    static const char *MALOS[] = { "{\"edad\": {\"$foo\": 1}}", "{\"$xor\": []}", "{\"a\": {\"$in\": 3}}", "{\"a\": {\"$regex\": \"(x\"}}",
                                   "{\"$or\": []}", "{\"a\": {\"$gt\": 1, \"b\": 2}}" };
    for (unsigned k = 0; k < sizeof MALOS / sizeof MALOS[0]; k++) {
        uint8_t *f = D(MALOS[k]);
        lmk_error err;
        OK(!consulta_validar(f, &err), "filtro inválido rechazado: %s", MALOS[k]);
        free(f);
    }
    /* proyección */
    uint8_t *p = D("{\"nombre\": 1, \"dir.ciudad\": 1, \"_id\": 0}");
    lmk_buf out = { 0 };
    proyectar(doc, p, &out);
    uint8_t *esp = D("{\"nombre\": \"Ana\", \"dir\": {\"ciudad\": \"Lima\"}}");
    OK(iguales(out.p, esp), "proyección de inclusión con camino");
    free(p); free(esp);
    out.n = 0;
    p = D("{\"notas\": 0, \"tags\": 0, \"dir\": 0}");
    proyectar(doc, p, &out);
    esp = D("{\"_id\": 1, \"nombre\": \"Ana\", \"edad\": 30, \"nulo\": null}");
    OK(iguales(out.p, esp), "proyección de exclusión");
    free(p); free(esp);
    buf_liberar(&out);
    free(doc);
}

static void prueba_actualizar(void)
{
    static const struct { const char *doc, *upd, *esp; } C[] = {
        { "{\"_id\": 1, \"a\": 1}", "{\"$set\": {\"a\": 2, \"b.c\": \"x\"}}", "{\"_id\": 1, \"a\": 2, \"b\": {\"c\": \"x\"}}" },
        { "{\"_id\": 1, \"a\": 1, \"b\": 2}", "{\"$unset\": {\"a\": \"\"}}", "{\"_id\": 1, \"b\": 2}" },
        { "{\"_id\": 1, \"n\": 1}", "{\"$inc\": {\"n\": 2, \"m\": 5}}", "{\"_id\": 1, \"n\": 3, \"m\": 5}" },
        { "{\"_id\": 1, \"n\": 2147483647}", "{\"$inc\": {\"n\": 1}}", "{\"_id\": 1, \"n\": 2147483648L}" },
        { "{\"_id\": 1, \"n\": 1}", "{\"$inc\": {\"n\": 0.5}}", "{\"_id\": 1, \"n\": 1.5}" },
        { "{\"_id\": 1, \"l\": [1]}", "{\"$push\": {\"l\": 2}}", "{\"_id\": 1, \"l\": [1, 2]}" },
        { "{\"_id\": 1}", "{\"$push\": {\"l\": {\"$each\": [1, 2]}}}", "{\"_id\": 1, \"l\": [1, 2]}" },
        { "{\"_id\": 1, \"l\": [1, 2, 3, 2]}", "{\"$pull\": {\"l\": 2}}", "{\"_id\": 1, \"l\": [1, 3]}" },
        { "{\"_id\": 1, \"l\": [1, 5, 9]}", "{\"$pull\": {\"l\": {\"$gte\": 5}}}", "{\"_id\": 1, \"l\": [1]}" },
        { "{\"_id\": 1, \"l\": [{\"a\": 1}, {\"a\": 2}]}", "{\"$pull\": {\"l\": {\"a\": 2}}}", "{\"_id\": 1, \"l\": [{\"a\": 1}]}" },
        { "{\"_id\": 1, \"a\": 1, \"b\": 2}", "{\"$rename\": {\"a\": \"z.y\"}}", "{\"_id\": 1, \"b\": 2, \"z\": {\"y\": 1}}" },
        { "{\"_id\": 1, \"l\": [1]}", "{\"$addToSet\": {\"l\": {\"$each\": [1, 2]}}}", "{\"_id\": 1, \"l\": [1, 2]}" },
        { "{\"_id\": 1, \"l\": [1, 2, 3]}", "{\"$pop\": {\"l\": -1}}", "{\"_id\": 1, \"l\": [2, 3]}" },
        { "{\"_id\": 1, \"a\": [{\"b\": 1}, {\"b\": 2}]}", "{\"$set\": {\"a.1.b\": 9}}", "{\"_id\": 1, \"a\": [{\"b\": 1}, {\"b\": 9}]}" },
        { "{\"_id\": 1, \"n\": 5}", "{\"$min\": {\"n\": 3}, \"$max\": {\"m\": 1}}", "{\"_id\": 1, \"n\": 3, \"m\": 1}" },
        { "{\"_id\": 1, \"n\": 5}", "{\"$mul\": {\"n\": 3}}", "{\"_id\": 1, \"n\": 15}" },
    };
    for (unsigned k = 0; k < sizeof C / sizeof C[0]; k++) {
        uint8_t *d = D(C[k].doc), *u = D(C[k].upd), *e = D(C[k].esp);
        lmk_buf out = { 0 };
        lmk_error err = { 0 };
        bool cambio = false;
        bool ok = actualizacion_validar(u, &err) && actualizar_doc(d, u, &out, &cambio, &err);
        OK(ok && iguales(out.p, e), "actualización %s sobre %s (%s)", C[k].upd, C[k].doc, ok ? "resultado distinto" : err.msg);
        buf_liberar(&out);
        free(d); free(u); free(e);
    }
    uint8_t *d = D("{\"_id\": 1, \"a\": 5}"), *u = D("{\"$set\": {\"_id\": 2}}");
    lmk_buf out = { 0 };
    lmk_error err = { 0 };
    bool cambio;
    OK(!actualizar_doc(d, u, &out, &cambio, &err) && err.codigo == ERR_ID_INMUTABLE, "el _id no se puede cambiar");
    free(u);
    u = D("{\"$inc\": {\"a.b\": 1}}");
    out.n = 0;
    OK(!actualizar_doc(d, u, &out, &cambio, &err), "$inc dentro de un número: error");
    free(u);
    u = D("{\"$set\": {\"a\": 5}}");
    out.n = 0;
    OK(actualizar_doc(d, u, &out, &cambio, &err) && !cambio, "$set al mismo valor: sin cambio");
    free(u);
    u = D("{\"$unknown\": {\"a\": 1}}");
    OK(!actualizacion_validar(u, &err), "operador desconocido rechazado");
    free(u);
    /* upsert */
    uint8_t *f = D("{\"nombre\": \"x\", \"edad\": {\"$gt\": 3}, \"dir.cp\": 1}");
    u = D("{\"$set\": {\"activo\": true}, \"$setOnInsert\": {\"nuevo\": 1}}");
    out.n = 0;
    uint8_t *e = D("{\"nombre\": \"x\", \"dir\": {\"cp\": 1}, \"activo\": true, \"nuevo\": 1}");
    OK(upsert_doc(f, u, &out, &err) && iguales(out.p, e), "upsert construye el documento con las igualdades del filtro");
    free(f); free(u); free(e); free(d);
    buf_liberar(&out);
}

/* ------------------------------------------------------------ motor */

static char carpeta[256];

static uint8_t *doc_n(int i)
{
    char t[300];
    snprintf(t, sizeof t, "{\"_id\": %d, \"n\": %d, \"par\": %s, \"nombre\": \"doc%05d\", \"tags\": [\"t%d\", \"todos\"], \"sub\": {\"x\": %d}}",
             i, i, i % 2 ? "false" : "true", i, i % 10, i * 2);
    return D(t);
}

static uint32_t contar(lmk_coleccion *c, const char *filtro, const char **plan)
{
    uint8_t *f = D(filtro);
    uint32_t n = 0;
    lmk_plan p;
    lmk_error err;
    uint32_t *v = coleccion_buscar(c, f, 0, &n, &p, &err);
    if (plan) *plan = p.plan;
    free(v);
    free(f);
    return n;
}

static void prueba_motor(void)
{
    snprintf(carpeta, sizeof carpeta, "%s/lmk-prueba-%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf %s", carpeta);
    if (system(cmd) != 0) printf("aviso: %s falló\n", cmd);
    lmk_motor m;
    lmk_error err;
    OK(motor_abrir(&m, carpeta, 1u << 30, false, &err), "motor_abrir (%s)", err.msg);
    lmk_base *b = motor_base(&m, "prueba", true, &err);
    lmk_coleccion *c = b ? base_coleccion(b, "docs", true, &err) : NULL;
    OK(c != NULL, "colección creada");
    if (!c) return;
    const int N = 5000;
    for (int i = 0; i < N; i++) {
        uint8_t *d = doc_n(i);
        if (!coleccion_insertar(c, d, NULL, NULL, &err)) { OK(0, "insertar %d: %s", i, err.msg); free(d); break; }
        free(d);
    }
    OK(c->vivas == (uint32_t)N, "%d documentos", N);
    uint8_t *d = doc_n(7);
    OK(!coleccion_insertar(c, d, NULL, NULL, &err) && err.codigo == ERR_DUPLICADO, "_id repetido: E11000");
    free(d);
    /* sin _id: se genera un ObjectId */
    d = D("{\"sin\": 1}");
    uint8_t tid;
    lmk_buf id = { 0 };
    OK(coleccion_insertar(c, d, &tid, &id, &err) && tid == BT_OID && id.n == 12, "ObjectId generado");
    free(d);
    buf_liberar(&id);
    const char *plan;
    OK(contar(c, "{\"_id\": 4321}", &plan) == 1 && !strcmp(plan, "IDHACK"), "_id por la tabla hash");
    OK(contar(c, "{\"n\": {\"$gte\": 100, \"$lt\": 200}}", &plan) == 100 && !strcmp(plan, "COLLSCAN"), "rango sin índice");
    bool ya;
    OK(coleccion_crear_indice(c, "n_1", "n", 1, false, &ya, &err), "índice n");
    OK(coleccion_crear_indice(c, "tags_1", "tags", 1, false, &ya, &err), "índice multiclave tags");
    OK(contar(c, "{\"n\": {\"$gte\": 100, \"$lt\": 200}}", &plan) == 100 && !strcmp(plan, "IXSCAN"), "rango por índice");
    OK(contar(c, "{\"n\": 77}", &plan) == 1 && !strcmp(plan, "IXSCAN"), "igualdad por índice");
    OK(contar(c, "{\"n\": {\"$in\": [1, 2, 3, 99999]}}", &plan) == 3 && !strcmp(plan, "IXSCAN"), "$in por índice");
    OK(contar(c, "{\"tags\": \"t3\"}", &plan) == N / 10 && !strcmp(plan, "IXSCAN"), "multiclave");
    OK(contar(c, "{\"tags\": \"todos\", \"n\": {\"$lt\": 10}}", NULL) == 10, "dos condiciones con índice");
    OK(contar(c, "{\"n\": {\"$gt\": 4990}, \"par\": true}", NULL) == 4, "índice + filtro");
    OK(!coleccion_crear_indice(c, "par_u", "par", 1, true, &ya, &err) && err.codigo == ERR_DUPLICADO, "índice único con duplicados: error");
    OK(coleccion_crear_indice(c, "nombre_u", "nombre", 1, true, &ya, &err), "índice único");
    d = D("{\"nombre\": \"doc00010\"}");
    OK(!coleccion_insertar(c, d, NULL, NULL, &err) && err.codigo == ERR_DUPLICADO, "índice único impide repetir");
    free(d);
    /* actualizar y borrar */
    uint32_t n;
    uint8_t *f = D("{\"n\": {\"$lt\": 1000}}");
    uint32_t *v = coleccion_buscar(c, f, 0, &n, NULL, &err);
    for (uint32_t i = 0; i < n; i++) {
        lmk_buf nuevo = { 0 };
        bool cambio;
        uint8_t *u = D("{\"$inc\": {\"n\": 100000}, \"$push\": {\"tags\": \"viejo\"}}");
        if (actualizar_doc(coleccion_doc(c, v[i]), u, &nuevo, &cambio, &err)) coleccion_cambiar(c, v[i], nuevo.p, &err);
        buf_liberar(&nuevo);
        free(u);
    }
    free(v);
    free(f);
    OK(contar(c, "{\"n\": {\"$gte\": 100000}}", &plan) == 1000 && !strcmp(plan, "IXSCAN"), "actualización reindexada");
    OK(contar(c, "{\"tags\": \"viejo\"}", NULL) == 1000, "$push reindexado (multiclave)");
    f = D("{\"n\": {\"$gte\": 2000, \"$lt\": 100000}}");
    v = coleccion_buscar(c, f, 0, &n, NULL, &err);
    for (uint32_t i = 0; i < n; i++) coleccion_quitar(c, v[i], &err);
    free(v);
    free(f);
    OK(c->vivas == 2001, "borrados (quedan %u)", c->vivas);
    uint32_t vivas = c->vivas, antes = c->fin;
    motor_cerrar(&m);   /* más de la mitad del archivo es basura: se compacta solo */
    /* reabrir: todo igual, índices reconstruidos */
    OK(motor_abrir(&m, carpeta, 1u << 30, false, &err), "reabrir");
    b = motor_base(&m, "prueba", false, &err);
    c = b ? base_coleccion(b, "docs", false, &err) : NULL;
    OK(c && c->vivas == vivas && c->nix == 3, "tras reabrir: %u documentos y 3 índices", c ? c->vivas : 0);
    if (!c) return;
    OK(contar(c, "{\"tags\": \"viejo\"}", &plan) == 1000 && !strcmp(plan, "IXSCAN"), "índice tras reabrir");
    OK(c->fin < antes / 2 && c->fin == c->vivos, "compactación automática al cerrar: %u -> %u bytes", antes, c->fin);
    antes = c->fin;
    OK(coleccion_compactar(c, &err) && c->fin == antes, "compactar a mano: %u -> %u bytes", antes, c->fin);
    OK(contar(c, "{\"n\": {\"$gte\": 1000}}", NULL) == vivas - 1, "consultas tras compactar");
    char ruta[400];
    snprintf(ruta, sizeof ruta, "%s", c->ruta);
    motor_cerrar(&m);
    /* corte de luz: un registro a medias al final */
    FILE *fp = fopen(ruta, "ab");
    uint8_t medio[] = { 1, 200, 0, 0, 0, 0x30, 0, 0, 0, 0x10, 'a', 0 };
    fwrite(medio, 1, sizeof medio, fp);
    fclose(fp);
    long tam_cortado;
    fp = fopen(ruta, "rb"); fseek(fp, 0, SEEK_END); tam_cortado = ftell(fp); fclose(fp);
    OK(motor_abrir(&m, carpeta, 1u << 30, false, &err), "abrir tras el corte");
    b = motor_base(&m, "prueba", false, &err);
    c = b ? base_coleccion(b, "docs", false, &err) : NULL;
    OK(c && c->vivas == vivas && c->fin == (uint32_t)tam_cortado - sizeof medio, "registro cortado descartado y archivo limpio");
    /* CRC malo en el último registro */
    d = D("{\"_id\": \"ultimo\", \"nombre\": \"zz\"}");
    OK(coleccion_insertar(c, d, NULL, NULL, &err), "insertar el último (%s)", err.msg);
    free(d);
    motor_cerrar(&m);
    fp = fopen(ruta, "r+b"); fseek(fp, -6, SEEK_END); fputc('Z', fp); fclose(fp);
    OK(motor_abrir(&m, carpeta, 1u << 30, false, &err), "abrir con CRC malo");
    b = motor_base(&m, "prueba", false, &err);
    c = b ? base_coleccion(b, "docs", false, &err) : NULL;
    OK(c && c->vivas == vivas && contar(c, "{\"_id\": \"ultimo\"}", NULL) == 0, "registro con CRC malo descartado");
    /* caché limitada: los documentos se leen del disco */
    motor_cerrar(&m);
    OK(motor_abrir(&m, carpeta, 64 * 1024, false, &err), "abrir con 64 KB de caché");
    b = motor_base(&m, "prueba", false, &err);
    c = b ? base_coleccion(b, "docs", false, &err) : NULL;
    OK(c && m.mem_usada <= 64 * 1024 && contar(c, "{\"sub.x\": {\"$gte\": 3000}}", NULL) == 500, "consultas leyendo del disco");
    /* agregación */
    lmk_buf out = { 0 };
    uint32_t nd;
    uint8_t *tub = D("{\"0\": {\"$match\": {\"n\": {\"$lt\": 2000}}}, \"1\": {\"$group\": {\"_id\": \"$par\", \"total\": {\"$sum\": 1}, "
                     "\"suma\": {\"$sum\": \"$n\"}, \"max\": {\"$max\": \"$n\"}}}, \"2\": {\"$sort\": {\"_id\": 1}}}");
    OK(agregar(c, tub, &out, &nd, &err) && nd == 2, "agregación: 2 grupos (%s)", err.msg);
    if (nd == 2) {
        uint8_t *e = D("{\"_id\": false, \"total\": 500, \"suma\": 750000, \"max\": 1999}");
        OK(iguales(out.p, e), "agregación: grupo false");
        free(e);
    }
    free(tub);
    buf_liberar(&out);
    motor_cerrar(&m);
    snprintf(cmd, sizeof cmd, "rm -rf %s", carpeta);
    if (system(cmd) != 0) printf("aviso: %s falló\n", cmd);
}

/* Cursores: un documento borrado entre lotes cuya ranura se reutiliza para otro
 * que no cumple el filtro no debe salir en el getMore. */
static void prueba_cursor(void)
{
    char dir[300], cmd[400];
    snprintf(dir, sizeof dir, "%s/lmk-cursor-%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    snprintf(cmd, sizeof cmd, "rm -rf %s", dir);
    if (system(cmd) != 0) printf("aviso: %s falló\n", cmd);
    lmk_motor m;
    lmk_error err;
    if (!motor_abrir(&m, dir, 1u << 20, false, &err)) { OK(0, "motor_abrir"); return; }
    lmk_servidor *s = servidor_nuevo(&m, NULL, NULL);
    lmk_sesion ses = { 0 };
    lmk_buf r = { 0 };
    uint8_t *o = D("{\"insert\": \"c\", \"$db\": \"t\", \"documents\": [{\"_id\": 0, \"x\": 0}, {\"_id\": 1, \"x\": 1}, "
                   "{\"_id\": 2, \"x\": 2}, {\"_id\": 3, \"x\": 3}, {\"_id\": 4, \"x\": 4}, {\"_id\": 5, \"x\": 5}]}");
    servidor_orden(s, &ses, o, &r);
    free(o);
    o = D("{\"find\": \"c\", \"$db\": \"t\", \"filter\": {\"x\": {\"$lt\": 10}}, \"batchSize\": 2}");
    servidor_orden(s, &ses, o, &r);
    free(o);
    bson_elem cur, id;
    int64_t cid = 0;
    if (bson_buscar(r.p, "cursor", &cur) && bson_buscar(cur.v, "id", &id)) cid = bson_int64(id.v);
    OK(cid != 0, "find con batchSize 2 deja un cursor abierto");
    /* borrar _id 4 y 5 e insertar otros dos que no cumplen el filtro (reutilizan sus ranuras) */
    o = D("{\"delete\": \"c\", \"$db\": \"t\", \"deletes\": [{\"q\": {\"_id\": {\"$gte\": 4}}, \"limit\": 0}]}");
    servidor_orden(s, &ses, o, &r);
    free(o);
    o = D("{\"insert\": \"c\", \"$db\": \"t\", \"documents\": [{\"_id\": 98, \"x\": 98}, {\"_id\": 99, \"x\": 99}]}");
    servidor_orden(s, &ses, o, &r);
    free(o);
    lmk_buf g = { 0 };
    uint32_t ini = bson_inicio(&g);
    bson_poner_int64(&g, "getMore", cid);
    bson_poner_texto(&g, "collection", "c");
    bson_poner_texto(&g, "$db", "t");
    bson_fin(&g, ini);
    servidor_orden(s, &ses, g.p, &r);
    buf_liberar(&g);
    bson_elem lote, d;
    int vistos = 0;
    bool intruso = false;
    if (bson_buscar(r.p, "cursor", &cur) && bson_buscar(cur.v, "nextBatch", &lote)) {
        bson_it it;
        bson_it_iniciar(&it, lote.v);
        while (bson_it_sig(&it, &d)) {
            bson_elem x;
            vistos++;
            if (bson_buscar(d.v, "_id", &x) && bson_a_int64(&x) >= 4) intruso = true;
        }
    }
    OK(vistos == 2 && !intruso, "getMore: ni los borrados ni los que ocupan su ranura (%d documentos)", vistos);
    buf_liberar(&r);
    servidor_liberar(s);
    motor_cerrar(&m);
    if (system(cmd) != 0) printf("aviso: %s falló\n", cmd);
}

int main(void)
{
    prueba_bson();
    prueba_comparar();
    prueba_regex();
    prueba_consultas();
    prueba_actualizar();
    prueba_motor();
    prueba_cursor();
    printf("teste_nucleo: %d pruebas, %d fallos\n", pruebas, fallos);
    return fallos ? 1 : 0;
}
