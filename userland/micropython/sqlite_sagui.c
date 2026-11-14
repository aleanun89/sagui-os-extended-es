/* sqlite_sagui.c - el módulo sqlite3 de Python: un subconjunto de la DB-API 2.0 (D116).
 *
 * connect(ruta) -> Connection (cursor, execute, executemany, executescript, commit, rollback,
 * close, "with"), Cursor (execute con "?" o ":nombre", executemany, fetchone/fetchmany/fetchall,
 * description, rowcount, lastrowid, arraysize, iteración). Tipos: None, int, float, str, bytes.
 * Como en CPython (isolation_level ""), antes de INSERT/UPDATE/DELETE/REPLACE se abre una
 * transacción sola; commit() la cierra. Excepciones: Error, DatabaseError, OperationalError,
 * IntegrityError, ProgrammingError, InterfaceError. Sin adaptadores.
 * row_factory (D118): None (tuplas), sqlite3.Row (acceso por índice y por nombre) o cualquier
 * función (cursor, fila); se hereda de la conexión al crear el cursor y se puede cambiar en
 * los dos. connect(ruta, timeout=5.0): reintenta si otro proceso tiene la base bloqueada.
 * Los textos son UTF-8 en los dos lados: pasan tal cual.
 * Liberación automática (D118): conexiones y cursores tienen finalizador (__del__): el
 * recolector los cierra si no se cerró a mano. El SQLite usa malloc, no el montón de Python,
 * así que el recolector no ve su peso: connect() fuerza una recolección cuando hay muchas
 * conexiones abiertas a la vez (LIMITE_CON), para que soltar conexiones en un bucle no
 * crezca la memoria. El finalizador del cursor solo toca su sentencia (no la conexión, que
 * puede haberse recogido ya en la misma pasada): sqlite3_close_v2 deja la conexión
 * "zombi" hasta que se liberan sus sentencias. */
#include <string.h>
#include "py/runtime.h"
#include "py/gc.h"
#include "py/objstr.h"
#include "py/objint.h"
#include "py/objtuple.h"
#include "py/objexcept.h"
#include "sqlite3.h"

MP_DEFINE_EXCEPTION(Error, Exception)
MP_DEFINE_EXCEPTION(InterfaceError, Error)
MP_DEFINE_EXCEPTION(DatabaseError, Error)
MP_DEFINE_EXCEPTION(OperationalError, DatabaseError)
MP_DEFINE_EXCEPTION(IntegrityError, DatabaseError)
MP_DEFINE_EXCEPTION(ProgrammingError, DatabaseError)

typedef struct {
    mp_obj_base_t base;
    sqlite3 *db;
    mp_obj_t row_factory;
} conexao_t;

typedef struct {
    mp_obj_base_t base;
    conexao_t *con;
    sqlite3_stmt *st;     /* sentencia activa con una fila ya leída (o NULL) */
    mp_int_t rowcount;
    long long lastrowid;
    mp_obj_t descricao;
    mp_obj_t nomes;       /* tupla con los nombres de las columnas (para sqlite3.Row) */
    mp_obj_t row_factory;
    mp_int_t arraysize;
    bool fechado;
} cursor_t;

extern const mp_obj_type_t tipo_conexao, tipo_cursor, tipo_row;

#define LIMITE_CON 8                 /* a partir de aquí, connect() recoge basura antes de abrir */
static int con_abertas = 0;          /* conexiones abiertas y no cerradas */
/* Sentencias activas (con filas por leer): un fetchone() deja la sentencia viva hasta que el
 * cursor se recoge, y en CPython eso pasa al instante (cuenta de referencias). Aquí se recoge
 * basura al pasar el límite, que sube si de verdad hay tantos cursores en uso. */
static int st_ativas = 0, st_limite = 32;

/* ------------------------------------------------------------ errores */

static NORETURN void erro_sqlite(sqlite3 *db, int codigo)
{
    const mp_obj_type_t *t = &mp_type_OperationalError;
    int base = codigo & 0xFF;
    if (base == SQLITE_CONSTRAINT) t = &mp_type_IntegrityError;
    else if (base == SQLITE_MISUSE || base == SQLITE_RANGE) t = &mp_type_ProgrammingError;
    else if (base == SQLITE_MISMATCH) t = &mp_type_IntegrityError;
    mp_raise_msg_varg(t, MP_ERROR_TEXT("%s"), db ? sqlite3_errmsg(db) : sqlite3_errstr(codigo));
}

static conexao_t *con_viva(conexao_t *c)
{
    if (!c->db) mp_raise_msg(&mp_type_ProgrammingError, MP_ERROR_TEXT("Cannot operate on a closed database."));
    return c;
}

/* ------------------------------------------------------------ valores */

static void ligar_valor(cursor_t *cur, int i, mp_obj_t v)
{
    sqlite3_stmt *st = cur->st;
    int r;
    if (v == mp_const_none) r = sqlite3_bind_null(st, i);
    else if (v == mp_const_true || v == mp_const_false) r = sqlite3_bind_int(st, i, v == mp_const_true);
    else if (mp_obj_is_small_int(v)) r = sqlite3_bind_int64(st, i, MP_OBJ_SMALL_INT_VALUE(v));
    else if (mp_obj_is_int(v)) {
        uint8_t b[8];
        mp_obj_int_to_bytes_impl(v, false, 8, b);
        long long x = 0;
        for (int k = 7; k >= 0; k--) x = x << 8 | b[k];
        r = sqlite3_bind_int64(st, i, x);
    }
    else if (mp_obj_is_float(v)) r = sqlite3_bind_double(st, i, (double)mp_obj_get_float(v));
    else if (mp_obj_is_str(v)) {
        size_t n;
        const char *s = mp_obj_str_get_data(v, &n);
        r = sqlite3_bind_text(st, i, s, (int)n, SQLITE_TRANSIENT);
    } else {
        mp_buffer_info_t bi;
        if (!mp_get_buffer(v, &bi, MP_BUFFER_READ))
            mp_raise_msg_varg(&mp_type_ProgrammingError, MP_ERROR_TEXT("Error binding parameter %d: type '%s' is not supported"), i, mp_obj_get_type_str(v));
        r = sqlite3_bind_blob(st, i, bi.buf, (int)bi.len, SQLITE_TRANSIENT);
    }
    if (r != SQLITE_OK) erro_sqlite(cur->con->db, r);
}

static void ligar_parametros(cursor_t *cur, mp_obj_t params)
{
    int n = sqlite3_bind_parameter_count(cur->st);
    if (params == mp_const_none) params = mp_const_empty_tuple;
    if (mp_obj_is_type(params, &mp_type_dict)) {
        for (int i = 1; i <= n; i++) {
            const char *nome = sqlite3_bind_parameter_name(cur->st, i);
            if (!nome || !nome[1]) mp_raise_msg(&mp_type_ProgrammingError, MP_ERROR_TEXT("Binding is not named"));
            mp_obj_t k = mp_obj_new_str(nome + 1, strlen(nome + 1));
            mp_map_elem_t *e = mp_map_lookup(mp_obj_dict_get_map(params), k, MP_MAP_LOOKUP);
            if (!e) mp_raise_msg_varg(&mp_type_ProgrammingError, MP_ERROR_TEXT("You did not supply a value for binding parameter :%s"), nome + 1);
            ligar_valor(cur, i, e->value);
        }
        return;
    }
    size_t qtd;
    mp_obj_t *itens;
    mp_obj_get_array(params, &qtd, &itens);
    if ((int)qtd != n)
        mp_raise_msg_varg(&mp_type_ProgrammingError, MP_ERROR_TEXT("Incorrect number of bindings supplied. The current statement uses %d, and there are %d supplied."), n, (int)qtd);
    for (int i = 0; i < n; i++) ligar_valor(cur, i + 1, itens[i]);
}

static mp_obj_t valor_coluna(sqlite3_stmt *st, int i)
{
    switch (sqlite3_column_type(st, i)) {
    case SQLITE_INTEGER: return mp_obj_new_int_from_ll(sqlite3_column_int64(st, i));
    case SQLITE_FLOAT: return mp_obj_new_float((mp_float_t)sqlite3_column_double(st, i));
    case SQLITE_TEXT: return mp_obj_new_str((const char *)sqlite3_column_text(st, i), (size_t)sqlite3_column_bytes(st, i));
    case SQLITE_BLOB: return mp_obj_new_bytes(sqlite3_column_blob(st, i), (size_t)sqlite3_column_bytes(st, i));
    default: return mp_const_none;
    }
}

static mp_obj_t linha_atual(cursor_t *cur)
{
    int n = sqlite3_column_count(cur->st);
    mp_obj_t t = mp_obj_new_tuple((size_t)n, NULL);
    mp_obj_tuple_t *tp = MP_OBJ_TO_PTR(t);
    for (int i = 0; i < n; i++) tp->items[i] = valor_coluna(cur->st, i);
    if (cur->row_factory != mp_const_none) {
        mp_obj_t args[2] = { MP_OBJ_FROM_PTR(cur), t };
        return mp_call_function_n_kw(cur->row_factory, 2, 0, args);
    }
    return t;
}

/* ------------------------------------------------------------ sqlite3.Row */

typedef struct {
    mp_obj_base_t base;
    mp_obj_t nomes;     /* tupla de nombres */
    mp_obj_t valores;   /* tupla de valores */
} row_t;

/* Row(cursor, tupla): también es lo que llama row_factory = sqlite3.Row. */
static mp_obj_t row_make_new(const mp_obj_type_t *tipo, size_t n, size_t nk, const mp_obj_t *a)
{
    mp_arg_check_num(n, nk, 2, 2, false);
    if (!mp_obj_is_type(a[0], &tipo_cursor) || !mp_obj_is_type(a[1], &mp_type_tuple))
        mp_raise_TypeError(MP_ERROR_TEXT("Row(cursor, tuple) expected"));
    cursor_t *c = MP_OBJ_TO_PTR(a[0]);
    row_t *r = mp_obj_malloc(row_t, tipo);
    r->nomes = c->nomes;
    r->valores = a[1];
    return MP_OBJ_FROM_PTR(r);
}

static char minuscula(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

static mp_obj_t row_subscr(mp_obj_t self, mp_obj_t idx, mp_obj_t val)
{
    row_t *r = MP_OBJ_TO_PTR(self);
    if (val != MP_OBJ_SENTINEL) return MP_OBJ_NULL;   /* solo lectura */
    if (mp_obj_is_str(idx)) {
        size_t ln;
        const char *k = mp_obj_str_get_data(idx, &ln);
        size_t nn;
        mp_obj_t *nom;
        mp_obj_tuple_get(r->nomes, &nn, &nom);
        for (size_t i = 0; i < nn; i++) {
            size_t lc;
            const char *c = mp_obj_str_get_data(nom[i], &lc);
            if (lc != ln) continue;
            size_t j = 0;
            while (j < ln && minuscula(c[j]) == minuscula(k[j])) j++;   /* como CPython: sin mayúsculas ASCII */
            if (j == ln) return mp_obj_subscr(r->valores, MP_OBJ_NEW_SMALL_INT(i), MP_OBJ_SENTINEL);
        }
        mp_raise_msg(&mp_type_IndexError, MP_ERROR_TEXT("No item with that key"));
    }
    return mp_obj_subscr(r->valores, idx, MP_OBJ_SENTINEL);
}

static mp_obj_t row_unary(mp_unary_op_t op, mp_obj_t self)
{
    row_t *r = MP_OBJ_TO_PTR(self);
    switch (op) {
    case MP_UNARY_OP_LEN: return mp_obj_len(r->valores);
    case MP_UNARY_OP_HASH: return mp_unary_op(MP_UNARY_OP_HASH, r->valores);
    default: return MP_OBJ_NULL;
    }
}

static mp_obj_t row_binop(mp_binary_op_t op, mp_obj_t a, mp_obj_t b)
{
    if (op != MP_BINARY_OP_EQUAL && op != MP_BINARY_OP_NOT_EQUAL) return MP_OBJ_NULL;
    if (!mp_obj_is_type(b, &tipo_row)) return mp_obj_new_bool(op == MP_BINARY_OP_NOT_EQUAL);
    row_t *x = MP_OBJ_TO_PTR(a), *y = MP_OBJ_TO_PTR(b);
    bool ig = mp_obj_equal(x->nomes, y->nomes) && mp_obj_equal(x->valores, y->valores);
    return mp_obj_new_bool(op == MP_BINARY_OP_EQUAL ? ig : !ig);
}

static mp_obj_t row_getiter(mp_obj_t self, mp_obj_iter_buf_t *buf)
{
    return mp_getiter(((row_t *)MP_OBJ_TO_PTR(self))->valores, buf);
}

static mp_obj_t row_keys(mp_obj_t self)
{
    size_t nn;
    mp_obj_t *nom;
    mp_obj_tuple_get(((row_t *)MP_OBJ_TO_PTR(self))->nomes, &nn, &nom);
    return mp_obj_new_list(nn, nom);
}
static MP_DEFINE_CONST_FUN_OBJ_1(row_keys_obj, row_keys);

static const mp_rom_map_elem_t row_metodos_tab[] = {
    { MP_ROM_QSTR(MP_QSTR_keys), MP_ROM_PTR(&row_keys_obj) },
};
static MP_DEFINE_CONST_DICT(row_metodos, row_metodos_tab);

MP_DEFINE_CONST_OBJ_TYPE(
    tipo_row,
    MP_QSTR_Row,
    MP_TYPE_FLAG_NONE,
    make_new, row_make_new,
    subscr, row_subscr,
    unary_op, row_unary,
    binary_op, row_binop,
    iter, row_getiter,
    locals_dict, &row_metodos
    );

/* ------------------------------------------------------------ cursor */

static void cursor_soltar(cursor_t *c)
{
    if (c->st) { sqlite3_finalize(c->st); c->st = NULL; st_ativas--; }
}

static bool e_dml(const char *s)
{
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    static const char *const p[] = { "insert", "update", "delete", "replace" };
    for (int k = 0; k < 4; k++) {
        size_t n = strlen(p[k]);
        size_t i = 0;
        while (i < n && (s[i] | 0x20) == p[k][i]) i++;
        if (i == n && !((s[n] | 0x20) >= 'a' && (s[n] | 0x20) <= 'z')) return true;
    }
    return false;
}

static void exec_simples(sqlite3 *db, const char *sql)
{
    int r = sqlite3_exec(db, sql, 0, 0, 0);
    if (r != SQLITE_OK) erro_sqlite(db, r);
}

/* Prepara y ejecuta la sentencia hasta la primera fila (o el final). Con "params" = NULL, sin
 * enlazar (executemany llama aparte). Devuelve true si hay una fila pendiente. */
static void cursor_preparar(cursor_t *cur, mp_obj_t sql_obj)
{
    cursor_soltar(cur);
    if (st_ativas >= st_limite) {
        gc_collect();
        if (st_ativas >= st_limite / 2) st_limite *= 2;
    }
    size_t n;
    const char *sql = mp_obj_str_get_data(sql_obj, &n);
    sqlite3 *db = con_viva(cur->con)->db;
    const char *resto;
    int r = sqlite3_prepare_v2(db, sql, (int)n, &cur->st, &resto);
    if (r != SQLITE_OK) { cur->st = NULL; erro_sqlite(db, r); }
    if (cur->st) st_ativas++;
    if (!cur->st) { /* vacía o solo comentarios */
        cur->descricao = mp_const_none; cur->rowcount = -1; return;
    }
    while (resto < sql + n && (*resto == ' ' || *resto == '\t' || *resto == '\n' || *resto == '\r' || *resto == ';')) resto++;
    if (resto < sql + n && *resto) {
        cursor_soltar(cur);
        mp_raise_msg(&mp_type_ProgrammingError, MP_ERROR_TEXT("You can only execute one statement at a time."));
    }
}

static void cursor_descricao(cursor_t *cur)
{
    int nc = sqlite3_column_count(cur->st);
    if (nc == 0) { cur->descricao = mp_const_none; cur->nomes = mp_const_empty_tuple; return; }
    mp_obj_t t = mp_obj_new_tuple((size_t)nc, NULL);
    mp_obj_t nomes = mp_obj_new_tuple((size_t)nc, NULL);
    cur->nomes = nomes;
    for (int i = 0; i < nc; i++) {
        const char *nome = sqlite3_column_name(cur->st, i);
        mp_obj_t sn = mp_obj_new_str(nome, strlen(nome));
        ((mp_obj_tuple_t *)MP_OBJ_TO_PTR(nomes))->items[i] = sn;
        mp_obj_t campos[7] = { sn, mp_const_none, mp_const_none, mp_const_none,
                               mp_const_none, mp_const_none, mp_const_none };
        ((mp_obj_tuple_t *)MP_OBJ_TO_PTR(t))->items[i] = mp_obj_new_tuple(7, campos);
    }
    cur->descricao = t;
}

/* Avanza un paso; deja cur->st vivo solo si hay una fila para leer. */
static void cursor_paso(cursor_t *cur)
{
    int r = sqlite3_step(cur->st);
    if (r == SQLITE_LOCKED) {   /* quizá lo bloquea un cursor olvidado, aún sin recoger: recoger y reintentar */
        gc_collect();
        r = sqlite3_step(cur->st);
    }
    if (r == SQLITE_ROW) return;
    sqlite3 *db = cur->con->db;
    if (r != SQLITE_DONE) {
        int e = sqlite3_extended_errcode(db);
        sqlite3_reset(cur->st);
        cursor_soltar(cur);
        erro_sqlite(db, e);
    }
    cursor_soltar(cur);
}

static mp_obj_t cursor_ejecutar_uno(cursor_t *cur, mp_obj_t sql_obj, mp_obj_t params, bool acumular)
{
    size_t n;
    const char *sql = mp_obj_str_get_data(sql_obj, &n);
    bool dml = e_dml(sql);
    cursor_preparar(cur, sql_obj);
    sqlite3 *db = cur->con->db;
    if (!cur->st) return MP_OBJ_FROM_PTR(cur);
    if (dml && sqlite3_get_autocommit(db)) exec_simples(db, "BEGIN");
    ligar_parametros(cur, params);
    cursor_descricao(cur);
    long long antes = sqlite3_total_changes(db);
    cursor_paso(cur);
    if (dml) {
        long long dep = sqlite3_changes(db);
        (void)antes;
        cur->rowcount = (acumular && cur->rowcount > 0 ? cur->rowcount : 0) + (mp_int_t)dep;
        cur->lastrowid = sqlite3_last_insert_rowid(db);
    } else if (!acumular) cur->rowcount = -1;
    return MP_OBJ_FROM_PTR(cur);
}

static cursor_t *cursor_novo(conexao_t *con)
{
    cursor_t *c = mp_obj_malloc_with_finaliser(cursor_t, &tipo_cursor);
    c->con = con;
    c->st = NULL;
    c->rowcount = -1;
    c->lastrowid = 0;
    c->descricao = mp_const_none;
    c->nomes = mp_const_empty_tuple;
    c->row_factory = con->row_factory;
    c->arraysize = 1;
    c->fechado = false;
    return c;
}

static cursor_t *cursor_viva(mp_obj_t o)
{
    cursor_t *c = MP_OBJ_TO_PTR(o);
    if (c->fechado) mp_raise_msg(&mp_type_ProgrammingError, MP_ERROR_TEXT("Cannot operate on a closed cursor."));
    con_viva(c->con);
    return c;
}

static mp_obj_t cursor_execute(size_t n, const mp_obj_t *a)
{
    cursor_t *c = cursor_viva(a[0]);
    return cursor_ejecutar_uno(c, a[1], n > 2 ? a[2] : mp_const_none, false);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(cursor_execute_obj, 2, 3, cursor_execute);

static mp_obj_t cursor_executemany(mp_obj_t self, mp_obj_t sql, mp_obj_t seq)
{
    cursor_t *c = cursor_viva(self);
    c->rowcount = 0;
    mp_obj_t it = mp_getiter(seq, NULL), p;
    while ((p = mp_iternext(it)) != MP_OBJ_STOP_ITERATION) cursor_ejecutar_uno(c, sql, p, true);
    return self;
}
static MP_DEFINE_CONST_FUN_OBJ_3(cursor_executemany_obj, cursor_executemany);

static mp_obj_t cursor_fetchone(mp_obj_t self)
{
    cursor_t *c = cursor_viva(self);
    if (!c->st) return mp_const_none;
    mp_obj_t l = linha_atual(c);
    cursor_paso(c);
    return l;
}
static MP_DEFINE_CONST_FUN_OBJ_1(cursor_fetchone_obj, cursor_fetchone);

static mp_obj_t cursor_fetchmany(size_t n, const mp_obj_t *a)
{
    cursor_t *c = cursor_viva(a[0]);
    mp_int_t max = n > 1 ? mp_obj_get_int(a[1]) : c->arraysize;
    mp_obj_t lista = mp_obj_new_list(0, NULL);
    while (max-- > 0 && c->st) {
        mp_obj_t l = linha_atual(c);
        cursor_paso(c);
        mp_obj_list_append(lista, l);
    }
    return lista;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(cursor_fetchmany_obj, 1, 2, cursor_fetchmany);

static mp_obj_t cursor_fetchall(mp_obj_t self)
{
    cursor_t *c = cursor_viva(self);
    mp_obj_t lista = mp_obj_new_list(0, NULL);
    while (c->st) {
        mp_obj_t l = linha_atual(c);
        cursor_paso(c);
        mp_obj_list_append(lista, l);
    }
    return lista;
}
static MP_DEFINE_CONST_FUN_OBJ_1(cursor_fetchall_obj, cursor_fetchall);

static mp_obj_t cursor_close(mp_obj_t self)
{
    cursor_t *c = MP_OBJ_TO_PTR(self);
    cursor_soltar(c);
    c->fechado = true;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(cursor_close_obj, cursor_close);

static mp_obj_t cursor_iternext(mp_obj_t self)
{
    cursor_t *c = cursor_viva(self);
    if (!c->st) return MP_OBJ_STOP_ITERATION;
    mp_obj_t l = linha_atual(c);
    cursor_paso(c);
    return l;
}

static void cursor_atributo(mp_obj_t self, qstr nome, mp_obj_t *dest)
{
    cursor_t *c = MP_OBJ_TO_PTR(self);
    if (dest[0] == MP_OBJ_NULL) {
        if (nome == MP_QSTR_description) dest[0] = c->descricao;
        else if (nome == MP_QSTR_rowcount) dest[0] = mp_obj_new_int(c->rowcount);
        else if (nome == MP_QSTR_lastrowid) dest[0] = mp_obj_new_int_from_ll(c->lastrowid);
        else if (nome == MP_QSTR_arraysize) dest[0] = mp_obj_new_int(c->arraysize);
        else if (nome == MP_QSTR_connection) dest[0] = MP_OBJ_FROM_PTR(c->con);
        else if (nome == MP_QSTR_row_factory) dest[0] = c->row_factory;
        else dest[1] = MP_OBJ_SENTINEL;
    } else if (dest[0] == MP_OBJ_SENTINEL && nome == MP_QSTR_arraysize) {
        c->arraysize = mp_obj_get_int(dest[1]);
        dest[0] = MP_OBJ_NULL;
    } else if (dest[0] == MP_OBJ_SENTINEL && nome == MP_QSTR_row_factory) {
        c->row_factory = dest[1];
        dest[0] = MP_OBJ_NULL;
    }
}

static const mp_rom_map_elem_t cursor_metodos_tab[] = {
    { MP_ROM_QSTR(MP_QSTR_execute), MP_ROM_PTR(&cursor_execute_obj) },
    { MP_ROM_QSTR(MP_QSTR_executemany), MP_ROM_PTR(&cursor_executemany_obj) },
    { MP_ROM_QSTR(MP_QSTR_fetchone), MP_ROM_PTR(&cursor_fetchone_obj) },
    { MP_ROM_QSTR(MP_QSTR_fetchmany), MP_ROM_PTR(&cursor_fetchmany_obj) },
    { MP_ROM_QSTR(MP_QSTR_fetchall), MP_ROM_PTR(&cursor_fetchall_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&cursor_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&cursor_close_obj) },   /* finalizador */
};
static MP_DEFINE_CONST_DICT(cursor_metodos, cursor_metodos_tab);

MP_DEFINE_CONST_OBJ_TYPE(
    tipo_cursor,
    MP_QSTR_Cursor,
    MP_TYPE_FLAG_ITER_IS_ITERNEXT,
    attr, cursor_atributo,
    iter, cursor_iternext,
    locals_dict, &cursor_metodos
    );

/* ------------------------------------------------------------ conexión */

static mp_obj_t con_cursor(mp_obj_t self)
{
    return MP_OBJ_FROM_PTR(cursor_novo(con_viva(MP_OBJ_TO_PTR(self))));
}
static MP_DEFINE_CONST_FUN_OBJ_1(con_cursor_obj, con_cursor);

static mp_obj_t con_execute(size_t n, const mp_obj_t *a)
{
    cursor_t *c = cursor_novo(con_viva(MP_OBJ_TO_PTR(a[0])));
    return cursor_ejecutar_uno(c, a[1], n > 2 ? a[2] : mp_const_none, false);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(con_execute_obj, 2, 3, con_execute);

static mp_obj_t con_executemany(mp_obj_t self, mp_obj_t sql, mp_obj_t seq)
{
    cursor_t *c = cursor_novo(con_viva(MP_OBJ_TO_PTR(self)));
    return cursor_executemany(MP_OBJ_FROM_PTR(c), sql, seq);
}
static MP_DEFINE_CONST_FUN_OBJ_3(con_executemany_obj, con_executemany);

static mp_obj_t con_executescript(mp_obj_t self, mp_obj_t sql)
{
    conexao_t *c = con_viva(MP_OBJ_TO_PTR(self));
    if (!sqlite3_get_autocommit(c->db)) exec_simples(c->db, "COMMIT");
    exec_simples(c->db, mp_obj_str_get_str(sql));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(con_executescript_obj, con_executescript);

static mp_obj_t con_commit(mp_obj_t self)
{
    conexao_t *c = con_viva(MP_OBJ_TO_PTR(self));
    if (!sqlite3_get_autocommit(c->db)) exec_simples(c->db, "COMMIT");
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(con_commit_obj, con_commit);

static mp_obj_t con_rollback(mp_obj_t self)
{
    conexao_t *c = con_viva(MP_OBJ_TO_PTR(self));
    if (!sqlite3_get_autocommit(c->db)) exec_simples(c->db, "ROLLBACK");
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(con_rollback_obj, con_rollback);

static mp_obj_t con_close(mp_obj_t self)
{
    conexao_t *c = MP_OBJ_TO_PTR(self);
    if (c->db) {
        sqlite3_close_v2(c->db);   /* lo pendiente sin confirmar se descarta, como en CPython */
        c->db = NULL;
        con_abertas--;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(con_close_obj, con_close);

static mp_obj_t con_enter(mp_obj_t self) { return self; }
static MP_DEFINE_CONST_FUN_OBJ_1(con_enter_obj, con_enter);

static mp_obj_t con_exit(size_t n, const mp_obj_t *a)
{
    if (a[1] == mp_const_none) con_commit(a[0]); else con_rollback(a[0]);
    (void)n;
    return mp_const_false;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(con_exit_obj, 4, 4, con_exit);

static void con_atributo(mp_obj_t self, qstr nome, mp_obj_t *dest)
{
    conexao_t *c = MP_OBJ_TO_PTR(self);
    if (dest[0] == MP_OBJ_NULL) {
        if (nome == MP_QSTR_total_changes) dest[0] = mp_obj_new_int(c->db ? sqlite3_total_changes(c->db) : 0);
        else if (nome == MP_QSTR_in_transaction) dest[0] = mp_obj_new_bool(c->db && !sqlite3_get_autocommit(c->db));
        else if (nome == MP_QSTR_row_factory) dest[0] = c->row_factory;
        else dest[1] = MP_OBJ_SENTINEL;
    } else if (dest[0] == MP_OBJ_SENTINEL && nome == MP_QSTR_row_factory) {
        c->row_factory = dest[1];
        dest[0] = MP_OBJ_NULL;
    }
}

static const mp_rom_map_elem_t con_metodos_tab[] = {
    { MP_ROM_QSTR(MP_QSTR_cursor), MP_ROM_PTR(&con_cursor_obj) },
    { MP_ROM_QSTR(MP_QSTR_execute), MP_ROM_PTR(&con_execute_obj) },
    { MP_ROM_QSTR(MP_QSTR_executemany), MP_ROM_PTR(&con_executemany_obj) },
    { MP_ROM_QSTR(MP_QSTR_executescript), MP_ROM_PTR(&con_executescript_obj) },
    { MP_ROM_QSTR(MP_QSTR_commit), MP_ROM_PTR(&con_commit_obj) },
    { MP_ROM_QSTR(MP_QSTR_rollback), MP_ROM_PTR(&con_rollback_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&con_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&con_close_obj) },   /* finalizador */
    { MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&con_enter_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&con_exit_obj) },
};
static MP_DEFINE_CONST_DICT(con_metodos, con_metodos_tab);

MP_DEFINE_CONST_OBJ_TYPE(
    tipo_conexao,
    MP_QSTR_Connection,
    MP_TYPE_FLAG_NONE,
    attr, con_atributo,
    locals_dict, &con_metodos
    );

/* ------------------------------------------------------------ módulo */

/* Auxiliar para las pruebas de fugas: bytes que el malloc del programa ha pedido al kernel. */
extern unsigned memoria_total(void);
static mp_obj_t sq_memoria(void) { return mp_obj_new_int_from_uint(memoria_total()); }
static MP_DEFINE_CONST_FUN_OBJ_0(sq_memoria_obj, sq_memoria);

static mp_obj_t sq_connect(size_t n, const mp_obj_t *args, mp_map_t *kw)
{
    /* timeout (segundos, 5.0 por defecto): tercer argumento posicional no; solo por nombre o 2.º.
     * isolation_level, check_same_thread...: se ignoran. */
    if (n < 1) mp_raise_TypeError(MP_ERROR_TEXT("connect() needs a database name"));
    const char *ruta = mp_obj_str_get_str(args[0]);
    mp_float_t espera = 5;
    if (n > 1) espera = mp_obj_get_float(args[1]);
    if (kw) {
        mp_map_elem_t *e = mp_map_lookup(kw, MP_OBJ_NEW_QSTR(MP_QSTR_timeout), MP_MAP_LOOKUP);
        if (e) espera = mp_obj_get_float(e->value);
    }
    if (con_abertas >= LIMITE_CON) gc_collect();   /* suelta las conexiones olvidadas (ver arriba) */
    sqlite3 *db = NULL;
    int r = sqlite3_open(ruta, &db);
    if (r != SQLITE_OK) {
        const char *m = db ? sqlite3_errmsg(db) : "unable to open database file";
        char buf[160];
        size_t ml = strlen(m);
        if (ml > sizeof buf - 1) ml = sizeof buf - 1;
        memcpy(buf, m, ml);
        buf[ml] = 0;
        sqlite3_close(db);
        mp_raise_msg_varg(&mp_type_OperationalError, MP_ERROR_TEXT("%s"), buf);
    }
    sqlite3_busy_timeout(db, espera <= 0 ? 0 : (int)(espera * 1000));
    conexao_t *c = mp_obj_malloc_with_finaliser(conexao_t, &tipo_conexao);
    c->db = db;
    c->row_factory = mp_const_none;
    con_abertas++;
    return MP_OBJ_FROM_PTR(c);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(sq_connect_obj, 1, sq_connect);

static const mp_rom_map_elem_t sq_globais_tab[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_sqlite3) },
    { MP_ROM_QSTR(MP_QSTR_connect), MP_ROM_PTR(&sq_connect_obj) },
    { MP_ROM_QSTR(MP_QSTR_Connection), MP_ROM_PTR(&tipo_conexao) },
    { MP_ROM_QSTR(MP_QSTR_Cursor), MP_ROM_PTR(&tipo_cursor) },
    { MP_ROM_QSTR(MP_QSTR__memoria_total), MP_ROM_PTR(&sq_memoria_obj) },
    { MP_ROM_QSTR(MP_QSTR_Row), MP_ROM_PTR(&tipo_row) },
    { MP_ROM_QSTR(MP_QSTR_Error), MP_ROM_PTR(&mp_type_Error) },
    { MP_ROM_QSTR(MP_QSTR_InterfaceError), MP_ROM_PTR(&mp_type_InterfaceError) },
    { MP_ROM_QSTR(MP_QSTR_DatabaseError), MP_ROM_PTR(&mp_type_DatabaseError) },
    { MP_ROM_QSTR(MP_QSTR_OperationalError), MP_ROM_PTR(&mp_type_OperationalError) },
    { MP_ROM_QSTR(MP_QSTR_IntegrityError), MP_ROM_PTR(&mp_type_IntegrityError) },
    { MP_ROM_QSTR(MP_QSTR_ProgrammingError), MP_ROM_PTR(&mp_type_ProgrammingError) },
    { MP_ROM_QSTR(MP_QSTR_apilevel), MP_ROM_QSTR(MP_QSTR_2_dot_0) },
    { MP_ROM_QSTR(MP_QSTR_paramstyle), MP_ROM_QSTR(MP_QSTR_qmark) },
    { MP_ROM_QSTR(MP_QSTR_threadsafety), MP_ROM_INT(0) },
};
static MP_DEFINE_CONST_DICT(sq_globais, sq_globais_tab);

const mp_obj_module_t modulo_sqlite3 = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&sq_globais,
};
MP_REGISTER_MODULE(MP_QSTR_sqlite3, modulo_sqlite3);
