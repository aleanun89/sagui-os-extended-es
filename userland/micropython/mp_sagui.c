/* mp_sagui.c - MicroPython en SaguiOS: REPL en el terminal y ejecución de
 * archivos .py.
 *
 *   python                        abre el REPL (">>> "; Ctrl+D en una línea vacía sale)
 *   python arquivo.py [args]      ejecuta el archivo (sys.argv = [arquivo, args...])
 *   python -c "código" [args]     ejecuta el texto
 *   python -X heap=16M ...        memoria máxima de los objetos Python (también PYTHONHEAP)
 *
 * Los textos que aparecen para el usuario están en español. Al contrario del
 * resto de SaguiOS, los *_sagui.c del lado de MicroPython se compilan con
 * las cadenas en UTF-8: todo lo que va al terminal pasa por el conversor
 * UTF-8 -> Latin-1 de sys.stdout (D068).
 *
 * El propio MicroPython viene generado en micropython_embed/ (ver D034); aquí
 * están el enlace con el terminal, la memoria, el import de archivos y el punto
 * de entrada. Los archivos, los módulos os, socket, ssl y hashlib están en los
 * otros *_sagui.c (D067-D069). */
#include <string.h>
#include "py/compile.h"
#include "py/gc.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/stackctrl.h"
#include "py/mperrno.h"
#include "py/lexer.h"
#include "py/builtin.h"
#include "py/objlist.h"
#include "py/objstr.h"
#include "shared/runtime/gchelper.h"
#include "shared/readline/readline.h"
#include "sagui_mp.h"

/* Memoria de los objetos Python (recolector de basura). Sin opción, empieza con
 * HEAP_INICIAL y crece sola hasta HEAP_PADRAO. Con -X heap=N (o
 * PYTHONHEAP=N) el heap tiene N desde el principio y no pasa de ahí: un tamaño
 * fijo, como pide quien elige el número. */
#define HEAP_INICIAL  (4 * 1024 * 1024)
#define HEAP_PADRAO   (256 * 1024 * 1024)
#define HEAP_MIN      (256 * 1024)
#define HEAP_MAX      (1024u * 1024 * 1024)  /* el kernel da hasta ~1,5 GB a un programa (D049) */
#define PILHA_LIMITE  (1024 * 1024)          /* la pila del programa tiene 8 MB que se asignan al tocarlos (D186) */

/* ------------------------------------------------------------ salida/entrada */

/* Todo lo que MicroPython escribe "en el terminal" (el eco del REPL, el print sin
 * file=, los tracebacks de mp_plat_print) pasa por el conversor de sys.stdout:
 * UTF-8 -> Latin-1 (D068). */
mp_uint_t mp_hal_stdout_tx_strn(const char *s, size_t n)
{
    sagui_saida_texto(1, s, n);
    return n;
}

void mp_hal_stdout_tx_strn_cooked(const char *s, size_t n) { sagui_saida_texto(1, s, n); }
void mp_hal_stdout_tx_str(const char *s) { sagui_saida_texto(1, s, strlen(s)); }
static void erro_txt(const char *s) { sagui_saida_texto(2, s, strlen(s)); }

int mp_hal_stdin_rx_chr(void)
{
    int c = sagui_entrada_byte();
    return c < 0 ? 4 : c;   /* fin de la entrada = Ctrl+D */
}

mp_uint_t mp_hal_ticks_ms(void) { return tempo_ms(); }
mp_uint_t mp_hal_ticks_us(void) { return tempo_ms() * 1000; }
mp_uint_t mp_hal_ticks_cpu(void) { return tempo_ms(); }
/* Las esperas sueltan el GIL (D170): mientras, corren los otros hilos. */
void mp_hal_delay_ms(mp_uint_t ms)
{
    if (!ms) return;
    /* Una señal despierta al kernel antes de tiempo: se llama al tratador de Python
     * (puede levantar una excepción, que sale de sleep) y se sigue esperando lo que
     * falte (PEP 475, D419). */
    unsigned fim = tempo_ms() + ms, resto = ms;
    for (;;) {
        sagui_rodar_tratadores();
        MP_THREAD_GIL_EXIT();
        dormir(resto);
        MP_THREAD_GIL_ENTER();
        sagui_rodar_tratadores();
        int d = (int)(fim - tempo_ms());
        if (d <= 0) break;
        resto = d;
    }
}
/* El reloj del kernel tiene 10 ms: menos de 1 ms no permite esperar. */
void mp_hal_delay_us(mp_uint_t us) { if (us >= 1000) mp_hal_delay_ms(us / 1000); }

/* random: la semilla viene del generador del kernel (SIS_ALEATORIO, D061). */
uint32_t sagui_aleatorio32(void)
{
    uint32_t v = 0;
    bytes_aleatorios(&v, sizeof v);
    return v;
}

/* ------------------------------------------------------------ import */

/* El import busca "nome.py" y la carpeta "nome" (paquete) en cada elemento de
 * sys.path; la ruta llega en UTF-8 y el kernel quiere Latin-1. */
mp_import_stat_t mp_import_stat(const char *caminho)
{
    char c[256];
    sagui_str_para_latin1(mp_obj_new_str(caminho, strlen(caminho)), c, sizeof c);
    info_arquivo_t i;
    if (info_arquivo(c, &i) < 0) return MP_IMPORT_STAT_NO_EXIST;
    return i.tipo == TIPO_PASTA ? MP_IMPORT_STAT_DIR : MP_IMPORT_STAT_FILE;
}

/* Lee un .py entero. El Editor de SaguiOS graba en Latin-1; un archivo
 * descargado o escrito fuera suele estar en UTF-8. Si el contenido ya es UTF-8
 * válido se deja como está; si no, se trata como Latin-1 (D068). Devuelve NULL
 * con el error en *erro. */
static char *ler_fonte(const char *caminho_utf8, size_t *n, size_t *cap, int *erro)
{
    char c[256];
    sagui_str_para_latin1(mp_obj_new_str(caminho_utf8, strlen(caminho_utf8)), c, sizeof c);
    int fd = abrir(c, ABRIR_LER);
    if (fd < 0) { *erro = fd; return NULL; }
    size_t tam = 4096, k = 0;
    char *buf = aloca(tam);
    int r;
    while (buf && (r = ler(fd, buf + k, tam - k)) > 0) {
        k += r;
        if (k == tam) { tam *= 2; buf = realoca(buf, tam); }
    }
    fechar(fd);
    if (!buf) { *erro = ERRO_SEM_MEMORIA; return NULL; }
    char *texto = sagui_para_utf8(buf, k, n, cap);
    libera(buf);
    return texto;
}

mp_lexer_t *mp_lexer_new_from_file(qstr arquivo)
{
    size_t n, cap;
    int erro;
    char *texto = ler_fonte(qstr_str(arquivo), &n, &cap, &erro);
    if (!texto) sagui_levantar_obj(erro, MP_OBJ_NEW_QSTR(arquivo));
    return mp_lexer_new_from_str_len(arquivo, texto, n, cap);
}

/* --------------------------------------------- memoria (recolector de basura) */

/* El recolector busca punteros a objetos en los registros y en la pila,
 * desde el punto actual hasta la cima guardada en mp_stack_set_top. */
void gc_collect(void)
{
    gc_collect_start();
    gc_helper_collect_regs_and_stack();
    #if MICROPY_PY_THREAD
    mp_thread_gc_others();   /* las pilas y los registros de los otros hilos (D170) */
    #endif
    gc_collect_end();
}

/* Crecimiento del heap (MICROPY_GC_SPLIT_HEAP_AUTO): gc.c pregunta cuánto
 * todavía cabe y pide áreas nuevas; las que quedan vacías vuelven a libsagui. */
static size_t heap_total, heap_maximo;

size_t gc_get_max_new_split(void) { return heap_maximo > heap_total ? heap_maximo - heap_total : 0; }

void *sagui_aloca_heap(size_t n)
{
    if (n > gc_get_max_new_split()) return NULL;
    void *p = aloca(n);
    if (p) heap_total += n;
    return p;
}

void sagui_libera_heap(void *p)
{
    mp_state_mem_area_t *a = p;
    heap_total -= a->gc_pool_end - (byte *)p;   /* (hasta 15 bytes menos: el final está alineado) */
    libera(p);
}

/* "16M", "512K", "4194304" -> bytes (0 = no válido). */
static size_t tamanho_heap(const char *s)
{
    size_t v = 0;
    if (*s < '0' || *s > '9') return 0;
    while (*s >= '0' && *s <= '9') {
        if (v > HEAP_MAX) return 0;
        v = v * 10 + (*s++ - '0');
    }
    if (*s == 'k' || *s == 'K') { v *= 1024; s++; }
    else if (*s == 'm' || *s == 'M') { if (v > HEAP_MAX >> 20) return 0; v *= 1024 * 1024; s++; }
    else if (*s == 'g' || *s == 'G') { if (v > 1) return 0; v *= 1024u * 1024 * 1024; s++; }
    if (*s == 'b' || *s == 'B') s++;
    if (*s || v < HEAP_MIN || v > HEAP_MAX) return 0;
    return v;
}

/* ---------------------------------------------------------------- ejecución */

/* Compila y ejecuta un texto. Devuelve 0, o 1 si queda una excepción (que ya
 * se imprimió en sys.stderr, como en CPython). SystemExit termina el
 * proceso con el código pedido. "livre" es la capacidad del buffer, si el
 * lexer debe liberarlo al final (0 = no es del recolector). */
static int executar_texto(const char *src, size_t n, size_t livre, qstr nome, mp_parse_input_kind_t tipo)
{
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_lexer_t *lex = mp_lexer_new_from_str_len(nome, src, n, livre);
        mp_parse_tree_t arvore = mp_parse(lex, tipo);
        if (tipo == MP_PARSE_FILE_INPUT) mp_store_global(MP_QSTR___file__, MP_OBJ_NEW_QSTR(nome));
        mp_obj_t modulo = mp_compile(&arvore, nome, tipo == MP_PARSE_SINGLE_INPUT);
        mp_call_function_0(modulo);
        nlr_pop();
        return 0;
    }
    mp_obj_t exc = (mp_obj_t)nlr.ret_val;
    if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(mp_obj_get_type(exc)), MP_OBJ_FROM_PTR(&mp_type_SystemExit))) {
        mp_obj_t val = mp_obj_exception_get_value(exc);
        int codigo = 0;
        if (val != mp_const_none && mp_obj_is_int(val)) codigo = mp_obj_get_int(val);
        else if (val != mp_const_none) {   /* sys.exit("mensaje"): escribe y sale con 1 */
            mp_obj_print_helper(sagui_print_erros(), val, PRINT_STR);
            erro_txt("\n");
            codigo = 1;
        }
        gc_sweep_all();   /* cierra archivos y sockets que quedaron abiertos (finalizadores) */
        sair(codigo);
    }
    mp_obj_print_exception(sagui_print_erros(), exc);
    return 1;
}

static int executar_arquivo(const char *caminho)
{
    size_t n = 0, cap = 0;
    int erro = 0;
    char *texto = NULL;
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        texto = ler_fonte(caminho, &n, &cap, &erro);
        nlr_pop();
    }
    if (!texto) {
        erro_txt("python: ");
        erro_txt(caminho);
        erro_txt(": ");
        if (erro) mp_obj_print_helper(sagui_print_erros(), sagui_str_de_latin1(nome_erro(erro)), PRINT_STR);
        else erro_txt("sin memoria");
        erro_txt("\n");
        return 2;
    }
    return executar_texto(texto, n, cap, qstr_from_str(caminho), MP_PARSE_FILE_INPUT);
}

/* Una línea de la entrada, ya en UTF-8 (sin el '\n'); false al final. */
static bool ler_linha_utf8(vstr_t *v)
{
    vstr_reset(v);
    for (;;) {
        int c = sagui_entrada_byte();
        if (c < 0) return v->len > 0;
        if (c == '\n') return true;
        vstr_add_byte(v, c);
    }
}

/* REPL: una línea por vez; si la línea abre un bloque (termina en ":", o tiene
 * paréntesis abiertos), sigue leyendo con "... " hasta una línea vacía. */
static void repl(void)
{
    mp_hal_stdout_tx_str("MicroPython " MICROPY_VERSION_STRING " en SaguiOS\n"
                         "Ctrl+D en una línea vacía o sys.exit() para salir.\n");
    vstr_t linha, texto;
    vstr_init(&linha, 128);
    vstr_init(&texto, 256);
    for (;;) {
        mp_hal_stdout_tx_str(">>> ");
        if (!ler_linha_utf8(&linha)) break;
        if (linha.len == 0) continue;
        vstr_reset(&texto);
        vstr_add_strn(&texto, linha.buf, linha.len);
        while (mp_repl_continue_with_input(vstr_null_terminated_str(&texto))) {
            mp_hal_stdout_tx_str("... ");
            if (!ler_linha_utf8(&linha)) break;
            vstr_add_byte(&texto, '\n');
            vstr_add_strn(&texto, linha.buf, linha.len);
            if (linha.len == 0) break;
        }
        executar_texto(texto.buf, texto.len, 0, MP_QSTR__lt_stdin_gt_, MP_PARSE_SINGLE_INPUT);
    }
    mp_hal_stdout_tx_str("\n");
}

static void ajuda(void)
{
    mp_hal_stdout_tx_str(
        "Uso: python [opciones] [archivo.py | -c código] [argumentos...]\n"
        "  (sin nada)       abre el intérprete interactivo; Ctrl+D en una línea vacía sale\n"
        "  archivo.py       ejecuta el archivo; su carpeta entra en sys.path\n"
        "  -c código        ejecuta el texto\n"
        "  -X heap=TAMAÑO   memoria fija para los objetos de Python (p. ej. 16M); sin la\n"
        "                   opción empieza con 4M y crece cuando hace falta, hasta 256M\n"
        "  -V               muestra la versión\n"
        "Variables: PYTHONHEAP=TAMAÑO (como -X heap), PYTHONPATH=carpeta:carpeta.\n"
        "Módulos: os, sys, io, json, re, random, time, struct, collections, hashlib,\n"
        "binascii, socket, ssl, select, signal, math, _thread, sagui (ventanas y dibujo)...\n"
        "y en /lib/python: threading, subprocess, selectors, pwd, grp, resource, tkinter,\n"
        "requests, urllib.parse.\n"
        "Ayuda: /ayuda/python.md (sockets, subprocess, señales...), /ayuda/python-tk.md;\n"
        "los hilos (con GIL): /ejemplos/hilos.py.\n"
        "Ejemplos en /ejemplos.\n");
}

/* (los nombres de qstr solo pueden tener letras: los textos con '/' y '-' van así) */
static MP_DEFINE_STR_OBJ(str_biblioteca, "/lib/python");
static MP_DEFINE_STR_OBJ(str_menos_c, "-c");

/* sys.path = [carpeta del script o '', PYTHONPATH..., '/lib/python'] */
static void preparar_sys_path(const char *script)
{
    mp_obj_list_t *p = MP_OBJ_TO_PTR(mp_sys_path);
    mp_obj_list_init(p, 0);
    if (script) {
        const char *barra = strrchr(script, '/');
        if (!barra) mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR_));
        else if (barra == script) mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_));
        else mp_obj_list_append(mp_sys_path, mp_obj_new_str(script, barra - script));
    } else {
        mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR_));
    }
    const char *pp = obter_env("PYTHONPATH");
    while (pp && *pp) {
        const char *fim = strchr(pp, ':');
        size_t n = fim ? (size_t)(fim - pp) : strlen(pp);
        if (n) {
            vstr_t v;
            vstr_init(&v, n + 4);
            sagui_latin1_para_vstr(&v, pp, n);
            mp_obj_list_append(mp_sys_path, mp_obj_new_str_from_vstr(&v));
        }
        pp = fim ? fim + 1 : NULL;
    }
    mp_obj_list_append(mp_sys_path, MP_OBJ_FROM_PTR(&str_biblioteca));
}

/* Los argumentos llegan de la shell en Latin-1 (lo que se escribió en el terminal). */
static mp_obj_t arg_str(const char *s)
{
    size_t n, cap;
    char *u = sagui_para_utf8(s, strlen(s), &n, &cap);
    return mp_obj_new_str(u, n);
}

int mp_principal(int argc, char **argv)
{
    int topo_da_pilha;
    size_t heap_tam = HEAP_PADRAO;
    bool fixo = false;
    const char *env = obter_env("PYTHONHEAP");
    if (env) {
        size_t t = tamanho_heap(env);
        if (t) { heap_tam = t; fixo = true; }
        else erro_txt("python: PYTHONHEAP no válido (p. ej. 16M); se ignora\n");
    }
/* opciones antes del archivo */
    int i = 1;
    while (i < argc && argv[i][0] == '-' && argv[i][1] && strcmp(argv[i], "-c") != 0) {
        const char *a = argv[i];
        if (!strcmp(a, "-h") || !strcmp(a, "--help") || !strcmp(a, "--ayuda")) { ajuda(); return 0; }
        if (!strcmp(a, "-V") || !strcmp(a, "--version")) {
            mp_hal_stdout_tx_str("MicroPython " MICROPY_VERSION_STRING " (SaguiOS)\n");
            return 0;
        }
        const char *x = NULL;
        if (!strcmp(a, "-X") && i + 1 < argc) x = argv[++i];
        else if (!strncmp(a, "-X", 2) && a[2]) x = a + 2;
        if (x) {
            if (!strncmp(x, "heap=", 5) && tamanho_heap(x + 5)) { heap_tam = tamanho_heap(x + 5); fixo = true; }
            else {
                erro_txt("python: opción -X no válida (use -X heap=16M, entre 256K y 1024M)\n");
                return 2;
            }
            i++;
            continue;
        }
        erro_txt("python: opción desconocida: ");
        erro_txt(a);
        erro_txt(" (python -h muestra la ayuda)\n");
        return 2;
    }

    heap_maximo = heap_tam;
    size_t inicial = fixo || heap_tam < HEAP_INICIAL ? heap_tam : HEAP_INICIAL;
    void *heap = aloca(inicial);
    if (!heap) {
        erro_txt("python: sin memoria para el heap (pruebe con un -X heap= menor)\n");
        return 2;
    }
    heap_total = inicial;
    sagui_iniciar_padrao();
    mp_stack_set_top(&topo_da_pilha);
    mp_stack_set_limit(PILHA_LIMITE);
    gc_init(heap, (char *)heap + inicial);
    mp_init();

    int r = 0;
    if (i < argc && strcmp(argv[i], "-c") == 0) {
        if (i + 1 >= argc) { erro_txt("python: -c necesita un argumento\n"); return 2; }
        preparar_sys_path(NULL);
        mp_obj_list_append(mp_sys_argv, MP_OBJ_FROM_PTR(&str_menos_c));
        for (int k = i + 2; k < argc; k++) mp_obj_list_append(mp_sys_argv, arg_str(argv[k]));
        size_t n, cap;
        char *texto = sagui_para_utf8(argv[i + 1], strlen(argv[i + 1]), &n, &cap);
        r = executar_texto(texto, n, cap, MP_QSTR__lt_string_gt_, MP_PARSE_FILE_INPUT);
    } else if (i < argc) {
        mp_obj_t caminho = arg_str(argv[i]);
        const char *c = mp_obj_str_get_str(caminho);
        preparar_sys_path(c);
        for (int k = i; k < argc; k++) mp_obj_list_append(mp_sys_argv, k == i ? caminho : arg_str(argv[k]));
        r = executar_arquivo(c);
    } else {
        preparar_sys_path(NULL);
        mp_obj_list_append(mp_sys_argv, MP_OBJ_NEW_QSTR(MP_QSTR_));
        repl();
    }
    #if MICROPY_PY_THREAD
    sagui_hilos_encerrar();   /* los otros hilos no vuelven a Python (D170) */
    #endif
    gc_sweep_all();   /* finalizadores: cierra archivos y sockets olvidados */
    mp_deinit();
    return r;
}

/* Excepción fuera de cualquier manejador: no debería ocurrir. */
void nlr_jump_fail(void *val)
{
    (void)val;
    erro_txt("python: error fatal (nlr_jump_fail)\n");
    sair(134);
}

/* input(): una línea de la entrada (con la edición del modo de línea del terminal),
 * convertida a UTF-8. */
int sagui_readline(vstr_t *linha, const char *prompt)
{
    mp_hal_stdout_tx_str(prompt);
    for (;;) {
        int c = sagui_entrada_byte();
        if (c < 0) return CHAR_CTRL_D;
        if (c == '\n') return 0;
        vstr_add_byte(linha, c);
    }
}
