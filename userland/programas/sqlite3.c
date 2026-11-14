// SPDX-License-Identifier: GPL-2.0-or-later
/* sqlite3 - intérprete de órdenes SQL sobre el SQLite de userland/terceros/sqlite (D116).
 *
 * Uso: sqlite3 [base.db] ["SQL"...]    (sin SQL: interactivo, o lee el guion de la entrada)
 * Órdenes: .tables .schema [tabla] .mode list|column|csv|line|tabs .headers on|off
 *          .separator SEP .nullvalue TXT .read archivo .open archivo .bench [N] .help .quit
 * Es un intérprete propio y pequeño (no el shell.c oficial): la salida de los modos list y csv
 * coincide con la del sqlite3 del anfitrión. */
#include "sagui.h"
#include "sqlite3.h"

enum { M_LISTA, M_COLUNA, M_CSV, M_LINHA, M_TABS };

static sqlite3 *bd;
static int   modo = M_LISTA, cabecalho, interativo, falhou, parar;
static char  sep[16] = "|", nulo[32] = "";
static char *sql_pendente;
static int   sql_tam, sql_cap;

/* ---- salida con búfer ---- */
static char sbuf[4096];
static int  sn;
static void sai_vaciar(void) { if (sn) { escrever_tudo(SAIDA, sbuf, (unsigned)sn); sn = 0; } }
static void sai(const char *s, int n)
{
    if (n < 0) n = (int)tam_texto(s);
    while (n > 0) {
        int c = n > (int)sizeof sbuf - sn ? (int)sizeof sbuf - sn : n;
        memcpy(sbuf + sn, s, (size_t)c);
        sn += c; s += c; n -= c;
        if (sn == (int)sizeof sbuf) sai_vaciar();
    }
}
static void sais(const char *s) { sai(s, -1); }

static void erro(const char *fmt, const char *a)
{
    sai_vaciar();
    imprimir_em(ERROS, fmt, a);
    falhou = 1;
}

/* ---- ejecutar SQL ---- */

/* Como el shell oficial: entre comillas si está vacío o tiene algún byte de control, espacio,
 * comilla (simple o doble), coma o byte >= 0x7F. */
static void csv_campo(const char *t, int n, bool tem_dados)
{
    bool aspas = n == 0;
    if (!tem_dados) return;
    for (int i = 0; i < n && !aspas; i++) {
        unsigned char c = (unsigned char)t[i];
        if (c < 0x20 || c == ' ' || c == '"' || c == '\'' || c == ',' || c >= 0x7F) aspas = true;
    }
    if (!aspas) { sai(t, n); return; }
    sai("\"", 1);
    for (int i = 0; i < n; i++) { if (t[i] == '"') sai("\"\"", 2); else sai(t + i, 1); }
    sai("\"", 1);
}

static void mostrar_linha_simples(sqlite3_stmt *st, int nc, bool primeira)
{
    if (modo == M_LINHA) {
        int larg = 0;
        for (int i = 0; i < nc; i++) { int t = (int)tam_texto(sqlite3_column_name(st, i)); if (t > larg) larg = t; }
        if (!primeira) sais("\n");
        for (int i = 0; i < nc; i++) {
            const char *nome = sqlite3_column_name(st, i);
            const char *v = (const char *)sqlite3_column_text(st, i);
            sais("    ");
            sais(nome);
            for (int k = (int)tam_texto(nome); k < larg; k++) sais(" ");
            sais(" = ");
            sais(v ? v : nulo);
            sais("\n");
        }
        return;
    }
    const char *s = modo == M_TABS ? "\t" : modo == M_CSV ? "," : sep;
    if (primeira && cabecalho)
        for (int i = 0; i < nc; i++) {
            const char *nome = sqlite3_column_name(st, i);
            if (modo == M_CSV) csv_campo(nome, (int)tam_texto(nome), true); else sais(nome);
            sais(i + 1 < nc ? s : modo == M_CSV ? "\r\n" : "\n");
        }
    for (int i = 0; i < nc; i++) {
        const char *v = (const char *)sqlite3_column_text(st, i);
        int n = sqlite3_column_bytes(st, i);
        if (modo == M_CSV) {
            if (v) csv_campo(v, n, true); else sais(nulo);
        } else sai(v ? v : nulo, v ? n : -1);
        sais(i + 1 < nc ? s : modo == M_CSV ? "\r\n" : "\n");
    }
}

/* Modo columna: guarda las filas para saber el ancho de cada columna. */
static void mostrar_columnas(sqlite3_stmt *st, int nc)
{
    int cap = 64, nl = 0;
    char **cel = sqlite3_malloc(cap * nc * (int)sizeof(char *));
    int *larg = sqlite3_malloc(nc * (int)sizeof(int));
    if (!cel || !larg) { sqlite3_free(cel); sqlite3_free(larg); erro("%s\n", "sin memoria"); return; }
    for (int i = 0; i < nc; i++) larg[i] = cabecalho ? (int)tam_texto(sqlite3_column_name(st, i)) : 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (nl == cap) {
            cap *= 2;
            char **n2 = sqlite3_realloc(cel, cap * nc * (int)sizeof(char *));
            if (!n2) break;
            cel = n2;
        }
        for (int i = 0; i < nc; i++) {
            const char *v = (const char *)sqlite3_column_text(st, i);
            char *c = sqlite3_mprintf("%s", v ? v : nulo);
            cel[nl * nc + i] = c;
            int t = c ? (int)tam_texto(c) : 0;
            if (t > larg[i]) larg[i] = t;
        }
        nl++;
    }
    if (nl) {
        if (cabecalho) {
            for (int i = 0; i < nc; i++) {
                const char *nome = sqlite3_column_name(st, i);
                sais(nome);
                for (int k = (int)tam_texto(nome); k < larg[i]; k++) sais(" ");
                sais(i + 1 < nc ? "  " : "\n");
            }
            for (int i = 0; i < nc; i++) {
                for (int k = 0; k < larg[i]; k++) sais("-");
                sais(i + 1 < nc ? "  " : "\n");
            }
        }
        for (int r = 0; r < nl; r++)
            for (int i = 0; i < nc; i++) {
                const char *c = cel[r * nc + i];
                sais(c ? c : "");
                for (int k = c ? (int)tam_texto(c) : 0; k < larg[i]; k++) sais(" ");
                sais(i + 1 < nc ? "  " : "\n");
            }
    }
    for (int r = 0; r < nl; r++) for (int i = 0; i < nc; i++) sqlite3_free(cel[r * nc + i]);
    sqlite3_free(cel);
    sqlite3_free(larg);
}

static void executar_sql(const char *sql)
{
    while (sql && *sql) {
        sqlite3_stmt *st = NULL;
        const char *resto;
        int r = sqlite3_prepare_v2(bd, sql, -1, &st, &resto);
        if (r != SQLITE_OK) { erro("Error: %s\n", sqlite3_errmsg(bd)); return; }
        if (!st) { sql = resto; continue; }   /* solo espacios o comentarios */
        int nc = sqlite3_column_count(st);
        if (nc > 0 && modo == M_COLUNA) mostrar_columnas(st, nc);
        else {
            bool primeira = true;
            while ((r = sqlite3_step(st)) == SQLITE_ROW) { mostrar_linha_simples(st, nc, primeira); primeira = false; }
        }
        r = sqlite3_finalize(st);
        if (r != SQLITE_OK) { erro("Error: %s\n", sqlite3_errmsg(bd)); return; }
        sql = resto;
    }
}

/* ---- órdenes con punto ---- */

static int comparar(const char *a, const char *b) { return compara_texto(a, b); }

static void listar_tablas(const char *patron)
{
    sqlite3_stmt *st;
    char *q = sqlite3_mprintf("SELECT name FROM sqlite_schema WHERE type IN ('table','view') "
                              "AND name NOT LIKE 'sqlite_%%' AND name LIKE %Q ORDER BY 1", patron ? patron : "%");
    if (sqlite3_prepare_v2(bd, q, -1, &st, 0) != SQLITE_OK) { erro("Error: %s\n", sqlite3_errmsg(bd)); sqlite3_free(q); return; }
    sqlite3_free(q);
    int n = 0, cap = 32, maxl = 0;
    char **nombres = sqlite3_malloc(cap * (int)sizeof(char *));
    while (nombres && sqlite3_step(st) == SQLITE_ROW) {
        if (n == cap) { cap *= 2; char **n2 = sqlite3_realloc(nombres, cap * (int)sizeof(char *)); if (!n2) break; nombres = n2; }
        nombres[n] = sqlite3_mprintf("%s", sqlite3_column_text(st, 0));
        int t = (int)tam_texto(nombres[n]);
        if (t > maxl) maxl = t;
        n++;
    }
    sqlite3_finalize(st);
    /* Igual que el shell oficial: columnas de 80 caracteres, ordenadas de arriba a abajo. */
    int cols = 80 / (maxl + 2);
    if (cols < 1) cols = 1;
    int filas = (n + cols - 1) / cols;
    for (int i = 0; i < filas; i++) {
        const char *s = "";
        for (int j = i; j < n; j += filas) {
            sais(s);
            sais(nombres[j]);
            for (int k = (int)tam_texto(nombres[j]); k < maxl; k++) sais(" ");
            s = "  ";
        }
        sais("\n");
    }
    for (int i = 0; i < n; i++) sqlite3_free(nombres[i]);
    sqlite3_free(nombres);
}

static void esquema(const char *patron)
{
    sqlite3_stmt *st;
    char *q = sqlite3_mprintf("SELECT sql FROM sqlite_schema WHERE sql NOT NULL AND (tbl_name LIKE %Q OR name LIKE %Q) "
                              "ORDER BY CASE type WHEN 'table' THEN 0 ELSE 1 END, rowid", patron ? patron : "%", patron ? patron : "%");
    if (sqlite3_prepare_v2(bd, q, -1, &st, 0) != SQLITE_OK) { erro("Error: %s\n", sqlite3_errmsg(bd)); sqlite3_free(q); return; }
    sqlite3_free(q);
    while (sqlite3_step(st) == SQLITE_ROW) { sais((const char *)sqlite3_column_text(st, 0)); sais(";\n"); }
    sqlite3_finalize(st);
}

static void abrir_base(const char *nombre)
{
    if (bd) sqlite3_close(bd);
    bd = NULL;
    if (sqlite3_open(nombre, &bd) != SQLITE_OK) { erro("Error: %s\n", sqlite3_errmsg(bd)); sqlite3_close(bd); bd = NULL; sqlite3_open(":memory:", &bd); }
}

static void leer_archivo(const char *nombre);

static void ayuda(void)
{
    sais(".bench [N]        medir el rendimiento (inserciones, consultas, recorrido)\n"
         ".headers on|off   mostrar los nombres de columna\n"
         ".mode MODO        list, column, csv, line o tabs\n"
         ".nullvalue TEXTO  texto para NULL\n"
         ".open ARCHIVO     abrir otra base\n"
         ".quit             salir\n"
         ".read ARCHIVO     ejecutar un guion SQL\n"
         ".schema [TABLA]   mostrar las sentencias CREATE\n"
         ".separator SEP    separador del modo list\n"
         ".tables [PATRON]  listar las tablas\n");
}

/* Parte la línea en palabras (con comillas simples o dobles). */
static int palabras(char *l, char **p, int max)
{
    int n = 0;
    while (*l && n < max) {
        while (*l == ' ' || *l == '\t') l++;
        if (!*l) break;
        if (*l == '"' || *l == '\'') {
            char q = *l++;
            p[n++] = l;
            while (*l && *l != q) l++;
        } else {
            p[n++] = l;
            while (*l && *l != ' ' && *l != '\t') l++;
        }
        if (*l) *l++ = 0;
    }
    return n;
}


/* ---- .bench [N]: medición de rendimiento con la API de C (mejora 064, D118) ---- */

static void bench_cifra(const char *que, unsigned ops, unsigned ms)
{
    if (ms == 0) ms = 1;
    imprimir("bench: %s: %u en %u ms = %u por segundo\n", que, ops, ms, (unsigned)((unsigned long long)ops * 1000 / ms));
}

static int bench_exec(const char *sql)
{
    return sqlite3_exec(bd, sql, 0, 0, 0);
}

/* Inserciones con y sin transacción, consultas por clave primaria y por índice, y un
 * recorrido completo, sobre la tabla bench_t (se borra al terminar). */
static void bench(unsigned n)
{
    if (n < 10) n = 10;
    unsigned t0, i, hallados = 0;
    sqlite3_stmt *st;
    bench_exec("DROP TABLE IF EXISTS bench_t");
    if (bench_exec("CREATE TABLE bench_t(id INTEGER PRIMARY KEY, k INTEGER, v TEXT)") != SQLITE_OK ||
        bench_exec("CREATE INDEX bench_k ON bench_t(k)") != SQLITE_OK) { erro("Error: %s\n", sqlite3_errmsg(bd)); return; }
    unsigned n_sin = n / 10 < 10 ? 10 : n / 10;   /* cada confirmación escribe y sincroniza: son pocas */
    sqlite3_prepare_v2(bd, "INSERT INTO bench_t(k, v) VALUES (?, 'valor de prueba')", -1, &st, 0);
    t0 = tempo_ms();
    for (i = 0; i < n_sin; i++) { sqlite3_bind_int(st, 1, (int)(i * 7)); sqlite3_step(st); sqlite3_reset(st); }
    bench_cifra("insertar sin transaccion (una confirmacion por fila)", n_sin, tempo_ms() - t0);
    t0 = tempo_ms();
    bench_exec("BEGIN");
    for (i = 0; i < n; i++) { sqlite3_bind_int(st, 1, (int)(i * 7)); sqlite3_step(st); sqlite3_reset(st); }
    bench_exec("COMMIT");
    bench_cifra("insertar en una transaccion", n, tempo_ms() - t0);
    sqlite3_finalize(st);
    sqlite3_prepare_v2(bd, "SELECT v FROM bench_t WHERE id = ?", -1, &st, 0);
    t0 = tempo_ms();
    for (i = 0; i < n; i++) {
        sqlite3_bind_int(st, 1, (int)(i % n_sin) + 1);
        if (sqlite3_step(st) == SQLITE_ROW) hallados++;
        sqlite3_reset(st);
    }
    bench_cifra("consulta por clave primaria", n, tempo_ms() - t0);
    sqlite3_finalize(st);
    sqlite3_prepare_v2(bd, "SELECT id FROM bench_t WHERE k = ?", -1, &st, 0);
    t0 = tempo_ms();
    for (i = 0; i < n; i++) {
        sqlite3_bind_int(st, 1, (int)((i % n_sin) * 7));
        if (sqlite3_step(st) == SQLITE_ROW) hallados++;
        sqlite3_reset(st);
    }
    bench_cifra("consulta por indice", n, tempo_ms() - t0);
    sqlite3_finalize(st);
    sqlite3_prepare_v2(bd, "SELECT id, k, v FROM bench_t", -1, &st, 0);
    t0 = tempo_ms();
    unsigned filas = 0;
    long long suma = 0;
    while (sqlite3_step(st) == SQLITE_ROW) { suma += sqlite3_column_int(st, 1); (void)sqlite3_column_text(st, 2); filas++; }
    sqlite3_finalize(st);
    bench_cifra("recorrido completo (filas leidas)", filas, tempo_ms() - t0);
    bench_exec("DROP TABLE bench_t");
    imprimir("bench: comprobacion: %u filas, %u aciertos, suma %d\n", filas, hallados, (int)suma);
}

static void orden_punto(const char *linha)
{
    char copia[512];
    copia_texto(copia, linha, sizeof copia);
    char *p[8];
    int n = palabras(copia, p, 8);
    if (n == 0) return;
    const char *c = p[0];
    if (!comparar(c, ".quit") || !comparar(c, ".exit")) parar = 1;
    else if (!comparar(c, ".tables")) listar_tablas(n > 1 ? p[1] : NULL);
    else if (!comparar(c, ".schema")) esquema(n > 1 ? p[1] : NULL);
    else if (!comparar(c, ".headers") || !comparar(c, ".header")) cabecalho = n > 1 && (!comparar(p[1], "on") || !comparar(p[1], "1"));
    else if (!comparar(c, ".mode")) {
        if (n < 2) { erro("%s\n", "uso: .mode list|column|csv|line|tabs"); return; }
        if (!comparar(p[1], "list")) { modo = M_LISTA; copia_texto(sep, "|", sizeof sep); }
        else if (!comparar(p[1], "column")) modo = M_COLUNA;
        else if (!comparar(p[1], "csv")) modo = M_CSV;
        else if (!comparar(p[1], "line")) modo = M_LINHA;
        else if (!comparar(p[1], "tabs")) modo = M_TABS;
        else erro("Error: modo desconocido: %s\n", p[1]);
    }
    else if (!comparar(c, ".separator")) { if (n > 1) copia_texto(sep, p[1], sizeof sep); }
    else if (!comparar(c, ".nullvalue")) { copia_texto(nulo, n > 1 ? p[1] : "", sizeof nulo); }
    else if (!comparar(c, ".read")) { if (n > 1) leer_archivo(p[1]); else erro("%s\n", "uso: .read ARCHIVO"); }
    else if (!comparar(c, ".open")) { if (n > 1) abrir_base(p[1]); else erro("%s\n", "uso: .open ARCHIVO"); }
    else if (!comparar(c, ".bench")) bench(n > 1 ? (unsigned)texto_para_int(p[1]) : 2000);
    else if (!comparar(c, ".help")) ayuda();
    else erro("Error: orden desconocida o no implementada: \"%s\". Escribe .help\n", c);
}

/* ---- entrada línea a línea ---- */

static void entrada_linea(const char *l)
{
    if (sql_tam == 0) {
        const char *q = l;
        while (*q == ' ' || *q == '\t' || *q == '\r') q++;
        if (!*q) return;
        if (*q == '.') { orden_punto(q); return; }
    }
    int t = (int)tam_texto(l);
    if (sql_tam + t + 2 > sql_cap) {
        int nc = (sql_tam + t + 2) * 2;
        char *n2 = sqlite3_realloc(sql_pendente, nc);
        if (!n2) { erro("%s\n", "sin memoria"); return; }
        sql_pendente = n2; sql_cap = nc;
    }
    memcpy(sql_pendente + sql_tam, l, (size_t)t);
    sql_tam += t;
    sql_pendente[sql_tam++] = '\n';
    sql_pendente[sql_tam] = 0;
    if (sqlite3_complete(sql_pendente)) {
        executar_sql(sql_pendente);
        sql_tam = 0;
    }
}

static void leer_archivo(const char *nombre)
{
    int fd = abrir(nombre, ABRIR_LER);
    if (fd < 0) { erro("Error: no se puede abrir \"%s\"\n", nombre); return; }
    info_arquivo_t inf;
    if (info_arquivo(nombre, &inf) < 0) { fechar(fd); return; }
    char *buf = sqlite3_malloc((int)inf.tamanho + 1);
    if (!buf) { fechar(fd); erro("%s\n", "sin memoria"); return; }
    int feito = 0;
    while (feito < (int)inf.tamanho) {
        int r = ler(fd, buf + feito, inf.tamanho - (unsigned)feito);
        if (r <= 0) break;
        feito += r;
    }
    fechar(fd);
    buf[feito] = 0;
    char *ini = buf;
    while (*ini && !parar) {
        char *fin = ini;
        while (*fin && *fin != '\n') fin++;
        char c = *fin;
        *fin = 0;
        if (fin > ini && fin[-1] == '\r') fin[-1] = 0;
        entrada_linea(ini);
        if (!c) break;
        ini = fin + 1;
    }
    sqlite3_free(buf);
}

int principal(int argc, char **argv)
{
    const char *base = ":memory:";
    int primero_sql = argc;
    int i = 1;
    for (; i < argc; i++) {
        if (!comparar(argv[i], "-header")) cabecalho = 1;
        else if (!comparar(argv[i], "-csv")) modo = M_CSV;
        else if (!comparar(argv[i], "-column")) modo = M_COLUNA;
        else if (!comparar(argv[i], "-line")) modo = M_LINHA;
        else if (!comparar(argv[i], "-list")) modo = M_LISTA;
        else if (!comparar(argv[i], "-noheader")) cabecalho = 0;
        else if (!comparar(argv[i], "-separator") && i + 1 < argc) copia_texto(sep, argv[++i], sizeof sep);
        else if (!comparar(argv[i], "-help") || !comparar(argv[i], "--help")) {
            imprimir("uso: sqlite3 [-header] [-csv|-column|-line|-list] [-separator SEP] [base.db] [SQL...]\n"); return 0;
        } else break;
    }
    if (i < argc) { base = argv[i++]; primero_sql = i; }
    abrir_base(base);
    if (!bd) return 1;
    if (primero_sql < argc) {   /* SQL en la línea de órdenes */
        for (i = primero_sql; i < argc && !parar; i++) {
            if (argv[i][0] == '.') orden_punto(argv[i]); else executar_sql(argv[i]);
            if (falhou) break;
        }
        sai_vaciar();
        sqlite3_close(bd);
        return falhou ? 1 : 0;
    }
    interativo = tipo_descritor(ENTRADA) == TIPO_TERMINAL;
    if (interativo) {
        imprimir("SQLite version %s (SaguiOS)\nEscribe \".help\" para ver las ordenes; \".quit\" para salir.\n", sqlite3_libversion());
        if (!comparar(base, ":memory:")) imprimir("Conectado a una base temporal en memoria. Usa \".open ARCHIVO\" para una persistente.\n");
    }
    static char linea[4096];
    while (!parar) {
        if (interativo) { sai(sql_tam ? "   ...> " : "sqlite> ", 8); sai_vaciar(); }
        int n = ler_linha(ENTRADA, linea, sizeof linea);
        if (n < 0) break;
        if (n > 0 && linea[n - 1] == '\r') linea[n - 1] = 0;
        entrada_linea(linea);
        sai_vaciar();
    }
    if (sql_tam) executar_sql(sql_pendente);
    sai_vaciar();
    sqlite3_close(bd);
    return falhou && !interativo ? 1 : 0;
}
