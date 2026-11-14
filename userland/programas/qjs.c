// SPDX-License-Identifier: GPL-2.0-or-later
/* qjs.c - el intérprete de JavaScript en la terminal (D188).
 *
 *   qjs fichero.js [args]     ejecuta un script (módulo ES si es .mjs o usa import/export)
 *   qjs -e 'código' [args]    evalúa el código y muestra errores con su traza
 *   qjs                       abre el REPL (historial con las flechas, varias líneas)
 *
 * El motor es el QuickJS de la D149 (userland/web/quickjs/): se enlazan los
 * mismos objetos que usa el navegador Cipó, sin su puente de DOM. Aquí se
 * añade lo que una terminal necesita: console/print (qjs_prelude.js),
 * setTimeout/setInterval con su bucle de eventos, los módulos "std" y "os"
 * (al estilo del quickjs-libc, un subconjunto) y el cargador de módulos ES.
 *
 * Codificación: el terminal del SaguiOS es Latin-1; los scripts y los
 * ficheros están en UTF-8. La salida se convierte de UTF-8 a Latin-1 y lo que
 * se teclea en el REPL, al revés.
 */
#include "sagui.h"
#include "quickjs.h"
#include "qjs_prelude.h"

size_t strlen(const char *s);
int strcmp(const char *a, const char *b);

#define PILHA_QJS       (8u * 1024 * 1024)   /* la pila propia: la del sistema (64 KB) no basta */
#define PILHA_QJS_LIM   (7u * 1024 * 1024)   /* lo que el motor puede usar de ella */
#define MEM_QJS_LIM     (96u * 1024 * 1024)
#define TIMERS_MAX      256
#define LINHA_MAX       1024

static JSRuntime *rt;
static JSContext *ctx;
static int codigo_saida;          /* se pone a 1 con cualquier error no capturado */
static int g_argc;
static char **g_argv;

/* ------------------------------------------------------------ salida */

/* Escribe texto UTF-8 en el descriptor, convertido a Latin-1. */
static void saida_utf8(int fd, const char *s, size_t n)
{
    char tmp[512];
    while (n > 0) {
        size_t k = n < sizeof tmp ? n : sizeof tmp;
        /* no cortar una secuencia UTF-8 por la mitad */
        while (k < n && ((unsigned char)s[k] & 0xC0) == 0x80 && k > 1) k--;
        for (size_t i = 0; i < k; i++) tmp[i] = s[i];
        size_t m = utf8_para_latin1(tmp, k, NULL);
        escrever_tudo(fd, tmp, (unsigned)m);
        s += k; n -= k;
    }
}

static void saida_txt(int fd, const char *s) { saida_utf8(fd, s, strlen(s)); }

/* __salida(fd, texto): lo que usa el console del prelude. */
static JSValue js_salida(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv)
{
    int32_t fd = 1;
    size_t n;
    JS_ToInt32(c, &fd, argv[0]);
    const char *s = JS_ToCStringLen(c, &n, argv[1]);
    if (!s) return JS_EXCEPTION;
    saida_utf8(fd, s, n);
    JS_FreeCString(c, s);
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------ errores */

static void relatar_valor(JSContext *c, JSValueConst e, const char *prefijo)
{
    const char *s = JS_ToCString(c, e);
    saida_txt(2, prefijo);
    saida_txt(2, s ? s : "(error)");
    saida_txt(2, "\n");
    if (s) JS_FreeCString(c, s);
    if (JS_IsError(c, e)) {
        JSValue st = JS_GetPropertyStr(c, e, "stack");
        if (!JS_IsUndefined(st)) {
            const char *t = JS_ToCString(c, st);
            if (t) { saida_txt(2, t); JS_FreeCString(c, t); }
        }
        JS_FreeValue(c, st);
    }
}

static void relatar_excecao(JSContext *c)
{
    JSValue e = JS_GetException(c);
    relatar_valor(c, e, "Uncaught ");
    JS_FreeValue(c, e);
    codigo_saida = 1;
}

static void rastreador_rejeicao(JSContext *c, JSValueConst promessa, JSValueConst razao,
                                int tratada, void *opaco)
{
    if (tratada) return;
    relatar_valor(c, razao, "Uncaught (in promise) ");
    codigo_saida = 1;
}

/* ------------------------------------------------------------ ficheros */

/* Lee un fichero entero; NULL si no existe. El resultado se libera con libera(). */
static char *ler_ficheiro(const char *caminho, size_t *tam)
{
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return NULL;
    size_t cap = 4096, n = 0;
    info_arquivo_t inf;
    if (info_arquivo(caminho, &inf) == 0 && inf.tamanho + 1 > cap) cap = inf.tamanho + 1;
    char *buf = aloca(cap);
    while (buf) {
        if (n + 1 >= cap) {
            char *nb = aloca(cap * 2);
            if (!nb) { libera(buf); buf = NULL; break; }
            for (size_t i = 0; i < n; i++) nb[i] = buf[i];
            libera(buf);
            buf = nb; cap *= 2;
        }
        int r = ler(fd, buf + n, (unsigned)(cap - 1 - n));
        if (r <= 0) break;
        n += (size_t)r;
    }
    fechar(fd);
    if (!buf) return NULL;
    buf[n] = 0;
    *tam = n;
    return buf;
}

static int escrever_ficheiro(const char *caminho, const char *s, size_t n, bool acrescentar)
{
    int modo = ABRIR_ESCREVER | ABRIR_CRIAR | (acrescentar ? ABRIR_ACRESCENTAR : ABRIR_TRUNCAR);
    int fd = abrir(caminho, modo);
    if (fd < 0) return -1;
    int r = escrever_tudo(fd, s, (unsigned)n);
    fechar(fd);
    return r < 0 ? -1 : 0;
}

/* ------------------------------------------------------------ std y os */

static JSValue js_ler_ficheiro(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv)
{
    const char *p = JS_ToCString(c, argv[0]);
    if (!p) return JS_EXCEPTION;
    size_t n;
    char *buf = ler_ficheiro(p, &n);
    JS_FreeCString(c, p);
    if (!buf) return JS_NULL;
    JSValue r = JS_NewStringLen(c, buf, n);
    libera(buf);
    return r;
}

/* magic: 0 = escribir, 1 = añadir */
static JSValue js_escrever_ficheiro(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv, int magic)
{
    const char *p = JS_ToCString(c, argv[0]);
    if (!p) return JS_EXCEPTION;
    size_t n;
    const char *s = JS_ToCStringLen(c, &n, argv[1]);
    if (!s) { JS_FreeCString(c, p); return JS_EXCEPTION; }
    int r = escrever_ficheiro(p, s, n, magic == 1);
    JS_FreeCString(c, s);
    if (r < 0) {
        JSValue e = JS_ThrowTypeError(c, "no se puede escribir '%s'", p);
        JS_FreeCString(c, p);
        return e;
    }
    JS_FreeCString(c, p);
    return JS_UNDEFINED;
}

static JSValue js_getenv(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv)
{
    const char *n = JS_ToCString(c, argv[0]);
    if (!n) return JS_EXCEPTION;
    const char *v = obter_env(n);
    JS_FreeCString(c, n);
    return v ? JS_NewString(c, v) : JS_UNDEFINED;
}

static JSValue js_sair(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv)
{
    int32_t k = 0;
    if (argc > 0) JS_ToInt32(c, &k, argv[0]);
    sair(k);
}

static JSValue js_dormir(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv)
{
    int32_t ms = 0;
    JS_ToInt32(c, &ms, argv[0]);
    if (ms > 0) dormir((unsigned)ms);
    return JS_UNDEFINED;
}

static JSValue js_agora(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv)
{
    return JS_NewInt64(c, (int64_t)tempo_ms());
}

/* std.readLine(): una línea de la entrada estándar (sin \n), o null al final. */
static JSValue js_ler_linha(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv)
{
    char buf[LINHA_MAX];
    int n = ler_linha(ENTRADA, buf, sizeof buf);
    if (n < 0) return JS_NULL;
    return JS_NewStringLen(c, buf, (size_t)n);
}

static JSValue js_puts(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv)
{
    size_t n;
    const char *s = JS_ToCStringLen(c, &n, argv[0]);
    if (!s) return JS_EXCEPTION;
    saida_utf8(1, s, n);
    JS_FreeCString(c, s);
    return JS_UNDEFINED;
}

/* std.evalScript(texto[, nombre]) ejecuta código global; std.loadScript(ruta) lo lee de un fichero. */
static JSValue js_eval_script(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv)
{
    size_t n;
    const char *s = JS_ToCStringLen(c, &n, argv[0]);
    if (!s) return JS_EXCEPTION;
    const char *nome = argc > 1 ? JS_ToCString(c, argv[1]) : NULL;
    JSValue r = JS_Eval(c, s, n, nome ? nome : "<evalScript>", JS_EVAL_TYPE_GLOBAL);
    if (nome) JS_FreeCString(c, nome);
    JS_FreeCString(c, s);
    return r;
}

static JSValue js_carregar_script(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv)
{
    const char *p = JS_ToCString(c, argv[0]);
    if (!p) return JS_EXCEPTION;
    size_t n;
    char *buf = ler_ficheiro(p, &n);
    if (!buf) {
        JSValue e = JS_ThrowReferenceError(c, "no se puede leer '%s'", p);
        JS_FreeCString(c, p);
        return e;
    }
    JSValue r = JS_Eval(c, buf, n, p, JS_EVAL_TYPE_GLOBAL);
    libera(buf);
    JS_FreeCString(c, p);
    return r;
}

static JSValue js_ler_pasta(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv)
{
    const char *p = JS_ToCString(c, argv[0]);
    if (!p) return JS_EXCEPTION;
    JSValue arr = JS_NewArray(c);
    entrada_pasta_t e;
    uint32_t k = 0;
    for (int i = 0; listar(p, i, &e) == 1; i++)
        JS_SetPropertyUint32(c, arr, k++, JS_NewString(c, e.nome));
    JS_FreeCString(c, p);
    return arr;
}

/* os.stat(ruta) -> { size, isDir, isFile } o null */
static JSValue js_stat(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv)
{
    const char *p = JS_ToCString(c, argv[0]);
    if (!p) return JS_EXCEPTION;
    info_arquivo_t inf;
    int r = info_arquivo(p, &inf);
    JS_FreeCString(c, p);
    if (r != 0) return JS_NULL;
    JSValue o = JS_NewObject(c);
    JS_SetPropertyStr(c, o, "size", JS_NewInt32(c, (int32_t)inf.tamanho));
    JS_SetPropertyStr(c, o, "isDir", JS_NewBool(c, inf.tipo == TIPO_PASTA));
    JS_SetPropertyStr(c, o, "isFile", JS_NewBool(c, inf.tipo == TIPO_ARQUIVO));
    return o;
}

/* magic: 0 = mkdir, 1 = remove (fichero), 2 = rmdir, 3 = chdir */
static JSValue js_ruta_op(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv, int magic)
{
    const char *p = JS_ToCString(c, argv[0]);
    if (!p) return JS_EXCEPTION;
    int r = magic == 0 ? criar_pasta(p) : magic == 1 ? apagar(p) : magic == 2 ? remover_pasta(p) : mudar_pasta(p);
    JS_FreeCString(c, p);
    return JS_NewInt32(c, r < 0 ? -1 : 0);
}

static JSValue js_getcwd(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv)
{
    char buf[256];
    if (pasta_atual(buf, sizeof buf) < 0) return JS_NULL;
    return JS_NewString(c, buf);
}

static JSValue js_renomear(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv)
{
    const char *a = JS_ToCString(c, argv[0]);
    const char *b = a ? JS_ToCString(c, argv[1]) : NULL;
    int r = -1;
    if (a && b) r = renomear(a, b);
    if (a) JS_FreeCString(c, a);
    if (b) JS_FreeCString(c, b);
    return JS_NewInt32(c, r < 0 ? -1 : 0);
}

static const JSCFunctionListEntry std_funcs[] = {
    JS_CFUNC_DEF("loadFile", 1, js_ler_ficheiro),
    JS_CFUNC_MAGIC_DEF("writeFile", 2, js_escrever_ficheiro, 0),
    JS_CFUNC_MAGIC_DEF("appendFile", 2, js_escrever_ficheiro, 1),
    JS_CFUNC_DEF("getenv", 1, js_getenv),
    JS_CFUNC_DEF("exit", 1, js_sair),
    JS_CFUNC_DEF("puts", 1, js_puts),
    JS_CFUNC_DEF("readLine", 0, js_ler_linha),
    JS_CFUNC_DEF("evalScript", 2, js_eval_script),
    JS_CFUNC_DEF("loadScript", 1, js_carregar_script),
};

static const JSCFunctionListEntry os_funcs[] = {
    JS_CFUNC_DEF("sleep", 1, js_dormir),
    JS_CFUNC_DEF("now", 0, js_agora),
    JS_CFUNC_DEF("readFile", 1, js_ler_ficheiro),
    JS_CFUNC_MAGIC_DEF("writeFile", 2, js_escrever_ficheiro, 0),
    JS_CFUNC_DEF("readdir", 1, js_ler_pasta),
    JS_CFUNC_DEF("stat", 1, js_stat),
    JS_CFUNC_MAGIC_DEF("mkdir", 1, js_ruta_op, 0),
    JS_CFUNC_MAGIC_DEF("remove", 1, js_ruta_op, 1),
    JS_CFUNC_MAGIC_DEF("rmdir", 1, js_ruta_op, 2),
    JS_CFUNC_MAGIC_DEF("chdir", 1, js_ruta_op, 3),
    JS_CFUNC_DEF("getcwd", 0, js_getcwd),
    JS_CFUNC_DEF("rename", 2, js_renomear),
    JS_PROP_STRING_DEF("platform", "sagui", JS_PROP_CONFIGURABLE),
};

#define N_STD ((int)(sizeof std_funcs / sizeof std_funcs[0]))
#define N_OS  ((int)(sizeof os_funcs / sizeof os_funcs[0]))

static int iniciar_std(JSContext *c, JSModuleDef *m) { return JS_SetModuleExportList(c, m, std_funcs, N_STD); }
static int iniciar_os(JSContext *c, JSModuleDef *m)  { return JS_SetModuleExportList(c, m, os_funcs, N_OS); }

/* ------------------------------------------------------------ temporizadores */

static struct { JSValue fn; unsigned vence; int id; int periodo; bool ativo; } tim[TIMERS_MAX];
static int prox_tim = 1;

/* magic: 0 = setTimeout, 1 = setInterval */
static JSValue js_set_timer(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv, int magic)
{
    if (argc < 1 || !JS_IsFunction(c, argv[0])) return JS_ThrowTypeError(c, "se esperaba una función");
    int32_t ms = 0;
    if (argc > 1) JS_ToInt32(c, &ms, argv[1]);
    if (ms < 0) ms = 0;
    for (int i = 0; i < TIMERS_MAX; i++) {
        if (tim[i].ativo) continue;
        tim[i].fn = JS_DupValue(c, argv[0]);
        tim[i].vence = tempo_ms() + (unsigned)ms;
        tim[i].periodo = magic ? (ms > 0 ? ms : 1) : -1;
        tim[i].id = prox_tim++;
        tim[i].ativo = true;
        return JS_NewInt32(c, tim[i].id);
    }
    return JS_ThrowInternalError(c, "demasiados temporizadores");
}

static JSValue js_clear_timer(JSContext *c, JSValueConst esto, int argc, JSValueConst *argv)
{
    int32_t id = 0;
    if (argc > 0) JS_ToInt32(c, &id, argv[0]);
    for (int i = 0; i < TIMERS_MAX; i++)
        if (tim[i].ativo && tim[i].id == id) {
            JS_FreeValue(c, tim[i].fn);
            tim[i].ativo = false;
        }
    return JS_UNDEFINED;
}

/* Vacía los trabajos pendientes (promesas); devuelve false si uno lanzó. */
static void correr_trabalhos(void)
{
    for (;;) {
        JSContext *c1;
        int r = JS_ExecutePendingJob(rt, &c1);
        if (r == 0) break;
        if (r < 0) relatar_excecao(c1);
    }
}

/* El bucle de eventos: trabajos y temporizadores hasta que no quede nada.
 * Con no_esperar, solo corre lo que ya venció. */
static void bucle_eventos(bool no_esperar)
{
    for (;;) {
        correr_trabalhos();
        int melhor = -1;
        for (int i = 0; i < TIMERS_MAX; i++)
            if (tim[i].ativo && (melhor < 0 || (int)(tim[i].vence - tim[melhor].vence) < 0)) melhor = i;
        if (melhor < 0) return;
        int espera = (int)(tim[melhor].vence - tempo_ms());
        if (espera > 0) {
            if (no_esperar) return;
            dormir((unsigned)espera);
            continue;
        }
        JSValue fn = JS_DupValue(ctx, tim[melhor].fn);
        if (tim[melhor].periodo < 0) { JS_FreeValue(ctx, tim[melhor].fn); tim[melhor].ativo = false; }
        else tim[melhor].vence += (unsigned)tim[melhor].periodo;
        JSValue r = JS_Call(ctx, fn, JS_UNDEFINED, 0, NULL);
        JS_FreeValue(ctx, fn);
        if (JS_IsException(r)) relatar_excecao(ctx);
        JS_FreeValue(ctx, r);
    }
}

/* ------------------------------------------------------------ módulos ES */

static JSModuleDef *cargador_modulo(JSContext *c, const char *nome, void *opaco)
{
    size_t n;
    char *buf = ler_ficheiro(nome, &n);
    if (!buf) {
        JS_ThrowReferenceError(c, "no se encuentra el módulo '%s'", nome);
        return NULL;
    }
    JSValue f = JS_Eval(c, buf, n, nome, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    libera(buf);
    if (JS_IsException(f)) return NULL;
    JSModuleDef *m = JS_VALUE_GET_PTR(f);
    JS_FreeValue(c, f);
    return m;
}

static bool termina_en(const char *s, const char *suf)
{
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && strcmp(s + a - b, suf) == 0;
}

/* ------------------------------------------------------------ ejecución */

/* Ejecuta el código; módulo = true para un módulo ES. Devuelve 0 o -1 si hubo error. */
static int ejecutar_codigo(const char *cod, size_t n, const char *nome, bool modulo)
{
    JSValue v = JS_Eval(ctx, cod, n, nome, modulo ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL);
    int rc = 0;
    if (JS_IsException(v)) {
        relatar_excecao(ctx);
        rc = -1;
    }
    bucle_eventos(false);
    if (rc == 0 && JS_PromiseState(ctx, v) == JS_PROMISE_REJECTED) {
        JSValue r = JS_PromiseResult(ctx, v);
        relatar_valor(ctx, r, "Uncaught ");
        JS_FreeValue(ctx, r);
        codigo_saida = 1;
        rc = -1;
    }
    JS_FreeValue(ctx, v);
    return rc;
}

static void mostrar_valor(JSValueConst v)
{
    if (JS_IsUndefined(v)) return;
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue f = JS_GetPropertyStr(ctx, g, "__inspect");
    JSValue s = JS_Call(ctx, f, JS_UNDEFINED, 1, &v);
    if (JS_IsException(s)) { relatar_excecao(ctx); }
    else {
        size_t n;
        const char *t = JS_ToCStringLen(ctx, &n, s);
        if (t) { saida_utf8(1, t, n); saida_txt(1, "\n"); JS_FreeCString(ctx, t); }
    }
    JS_FreeValue(ctx, s);
    JS_FreeValue(ctx, f);
    JS_FreeValue(ctx, g);
}

/* ------------------------------------------------------------ REPL */

#define HIST_MAX 100
static char *hist[HIST_MAX];
static int n_hist;

static void hist_poner(const char *l)
{
    if (!*l) return;
    if (n_hist && compara_texto(hist[n_hist - 1], l) == 0) return;
    if (n_hist == HIST_MAX) {
        libera(hist[0]);
        for (int i = 1; i < HIST_MAX; i++) hist[i - 1] = hist[i];
        n_hist--;
    }
    char *d = aloca(tam_texto(l) + 1);
    if (!d) return;
    copia_texto(d, l, tam_texto(l) + 1);
    hist[n_hist++] = d;
}

static void cur_izq(int n) { while (n-- > 0) escrever_texto(SAIDA, "\x1b[D"); }
static void cur_der(int n) { while (n-- > 0) escrever_texto(SAIDA, "\x1b[C"); }

static void redibujar(const char *l, int n, int de, int cur)
{
    escrever(SAIDA, l + de, (unsigned)(n - de));
    escrever_texto(SAIDA, "\x1b[K");
    cur_izq(n - cur);
}

static int leer_tecla(void)
{
    unsigned char c;
    if (ler(ENTRADA, &c, 1) <= 0) return -1;
    if (c != 27) return c;
    unsigned char d;
    if (ler(ENTRADA, &d, 1) <= 0) return -1;
    return 0x100 | d;
}

/* Lee una línea (Latin-1) con edición. -1 = fin de la entrada, -2 = Ctrl+C. */
static int leer_linea(const char *prompt, char *l)
{
    escrever_texto(SAIDA, prompt);
    if (tipo_descritor(ENTRADA) != TIPO_TERMINAL) {
        int n = ler_linha(ENTRADA, l, LINHA_MAX);
        return n;
    }
    int n = 0, cur = 0, pos = n_hist;
    l[0] = 0;
    modo_terminal(TERM_BRUTO);
    for (;;) {
        int t = leer_tecla();
        if (t < 0) { modo_terminal(TERM_LINHA); return -1; }
        if (t == '\n' || t == '\r') break;
        if (t == 4 && n == 0) { modo_terminal(TERM_LINHA); escrever_texto(SAIDA, "\n"); return -1; }
        if (t == 3) {
            cur_der(n - cur);
            escrever_texto(SAIDA, "^C\n");
            modo_terminal(TERM_LINHA);
            return -2;
        }
        if (t == '\b' || t == 0x7F) {
            if (cur > 0) {
                memmove(l + cur - 1, l + cur, (size_t)(n - cur + 1));
                n--; cur--;
                cur_izq(1);
                redibujar(l, n, cur, cur);
            }
        } else if (t == (0x100 | TECLA_B_DELETE)) {
            if (cur < n) {
                memmove(l + cur, l + cur + 1, (size_t)(n - cur));
                n--;
                redibujar(l, n, cur, cur);
            }
        } else if (t == (0x100 | TECLA_B_ESQUERDA)) {
            if (cur > 0) { cur--; cur_izq(1); }
        } else if (t == (0x100 | TECLA_B_DIREITA)) {
            if (cur < n) { cur++; cur_der(1); }
        } else if (t == (0x100 | TECLA_B_HOME) || t == 1) {
            cur_izq(cur); cur = 0;
        } else if (t == (0x100 | TECLA_B_END) || t == 5) {
            cur_der(n - cur); cur = n;
        } else if (t == 21) {   /* Ctrl+U */
            if (cur > 0) {
                memmove(l, l + cur, (size_t)(n - cur + 1));
                n -= cur;
                cur_izq(cur); cur = 0;
                redibujar(l, n, 0, 0);
            }
        } else if (t == 11) {   /* Ctrl+K */
            if (cur < n) { l[cur] = 0; n = cur; escrever_texto(SAIDA, "\x1b[K"); }
        } else if (t == (0x100 | TECLA_B_CIMA) || t == (0x100 | TECLA_B_BAIXO)) {
            const char *nova = NULL;
            if (t == (0x100 | TECLA_B_CIMA)) { if (pos > 0) nova = hist[--pos]; }
            else if (pos < n_hist - 1) nova = hist[++pos];
            else if (pos < n_hist) { pos = n_hist; nova = ""; }
            if (nova) {
                cur_izq(cur);
                copia_texto(l, nova, LINHA_MAX);
                n = cur = (int)tam_texto(l);
                redibujar(l, n, 0, cur);
            }
        } else if (t >= 0x20 && t < 0x100 && t != 0x7F && n < LINHA_MAX - 1) {
            memmove(l + cur + 1, l + cur, (size_t)(n - cur + 1));
            l[cur] = (char)t;
            n++;
            if (cur == n - 1) escrever(SAIDA, &l[cur], 1);
            else redibujar(l, n, cur, cur + 1);
            cur++;
        }
    }
    escrever_texto(SAIDA, "\n");
    modo_terminal(TERM_LINHA);
    l[n] = 0;
    return n;
}

/* ¿Falta algo para cerrar la entrada (paréntesis, comillas, comentario)? */
static bool incompleto(const char *s)
{
    int prof = 0;
    char com = 0;   /* ' " ` abiertas */
    bool bloque = false;
    for (; *s; s++) {
        char ch = *s;
        if (bloque) { if (ch == '*' && s[1] == '/') { bloque = false; s++; } continue; }
        if (com) {
            if (ch == '\\' && s[1]) s++;
            else if (ch == com) com = 0;
            else if (ch == '\n' && com != '`') com = 0;   /* una cadena simple no cruza líneas */
            continue;
        }
        if (ch == '/' && s[1] == '/') { while (s[1] && s[1] != '\n') s++; continue; }
        if (ch == '/' && s[1] == '*') { bloque = true; s++; continue; }
        if (ch == '\'' || ch == '"' || ch == '`') { com = ch; continue; }
        if (ch == '(' || ch == '[' || ch == '{') prof++;
        else if (ch == ')' || ch == ']' || ch == '}') prof--;
    }
    return prof > 0 || com == '`' || bloque;
}

/* Latin-1 -> UTF-8 (lo que se tecleó, para el motor). */
static char *latin1_a_utf8(const char *s)
{
    size_t n = strlen(s);
    char *d = aloca(n * 2 + 1);
    if (!d) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 0x80) d[k++] = (char)ch;
        else { d[k++] = (char)(0xC0 | (ch >> 6)); d[k++] = (char)(0x80 | (ch & 0x3F)); }
    }
    d[k] = 0;
    return d;
}

static int repl(void)
{
    saida_txt(1, "QuickJS " CONFIG_VERSION " en SaguiOS. \".help\" da ayuda; \".exit\" o Ctrl+D salen.\n");
    static char linea[LINHA_MAX];
    char *acum = NULL;
    size_t acum_n = 0;
    for (;;) {
        int n = leer_linea(acum ? "... " : "> ", linea);
        if (n == -1) break;
        if (n == -2) { libera(acum); acum = NULL; acum_n = 0; continue; }
        if (!acum) {
            if (compara_texto(linea, ".exit") == 0) break;
            if (compara_texto(linea, ".help") == 0) {
                saida_txt(1, ".exit  sale (o Ctrl+D)\n.help  esta ayuda\nUp/Down: historial. Se muestra el valor de cada expresión.\nstd y os: std.loadFile, std.writeFile, std.getenv, os.readdir, os.sleep...\n");
                continue;
            }
        }
        hist_poner(linea);
        char *u = latin1_a_utf8(linea);
        if (!u) continue;
        size_t ul = strlen(u);
        char *nuevo = aloca(acum_n + ul + 2);
        if (!nuevo) { libera(u); continue; }
        for (size_t i = 0; i < acum_n; i++) nuevo[i] = acum[i];
        for (size_t i = 0; i < ul; i++) nuevo[acum_n + i] = u[i];
        nuevo[acum_n + ul] = '\n';
        nuevo[acum_n + ul + 1] = 0;
        libera(u);
        libera(acum);
        acum = nuevo;
        acum_n += ul + 1;
        if (incompleto(acum)) continue;
        JSValue v = JS_Eval(ctx, acum, acum_n, "<stdin>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(v)) { relatar_excecao(ctx); codigo_saida = 0; }
        else { mostrar_valor(v); JS_FreeValue(ctx, v); }
        bucle_eventos(false);
        libera(acum);
        acum = NULL;
        acum_n = 0;
    }
    libera(acum);
    return 0;
}

/* ------------------------------------------------------------ principal */

static void ayuda(void)
{
    saida_txt(1, "uso: qjs [-m] [-e CODIGO] [FICHERO.js] [argumentos]\n"
                 "  FICHERO.js   ejecuta el script (módulo ES con -m, con extensión .mjs o si usa import/export)\n"
                 "  -e CODIGO    evalúa el código\n"
                 "  sin argumentos abre el REPL (.exit o Ctrl+D para salir)\n"
                 "Globales: console, print, setTimeout, setInterval, scriptArgs; módulos std y os.\n");
}

static int qjs_main(int argc, char **argv)
{
    rt = JS_NewRuntime();
    if (!rt) { saida_txt(2, "qjs: no hay memoria para el motor\n"); return 1; }
    JS_SetMemoryLimit(rt, MEM_QJS_LIM);
    JS_SetMaxStackSize(rt, PILHA_QJS_LIM);
    ctx = JS_NewContext(rt);
    if (!ctx) { saida_txt(2, "qjs: no hay memoria para el contexto\n"); return 1; }
    JS_SetModuleLoaderFunc(rt, NULL, cargador_modulo, NULL);
    JS_SetHostPromiseRejectionTracker(rt, rastreador_rejeicao, NULL);

    /* argumentos */
    const char *expr = NULL, *ficheiro = NULL;
    bool forzar_modulo = false;
    int i = 1;
    for (; i < argc; i++) {
        if (compara_texto(argv[i], "-h") == 0 || compara_texto(argv[i], "--help") == 0) { ayuda(); return 0; }
        else if (compara_texto(argv[i], "-m") == 0) forzar_modulo = true;
        else if (compara_texto(argv[i], "-e") == 0) {
            if (i + 1 >= argc) { saida_txt(2, "qjs: -e necesita un código\n"); return 2; }
            expr = argv[++i];
        } else if (compara_texto(argv[i], "--") == 0) { i++; break; }
        else if (argv[i][0] == '-' && argv[i][1]) { saida_txt(2, "qjs: opción desconocida; qjs -h da la ayuda\n"); return 2; }
        else break;
    }
    int primero_arg = i;
    if (!expr && i < argc) { ficheiro = argv[i]; primero_arg = i; }
    else if (expr) primero_arg = i;

    JSValue g = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, g, "__salida", JS_NewCFunction(ctx, js_salida, "__salida", 2));
    JS_SetPropertyStr(ctx, g, "setTimeout", JS_NewCFunctionMagic(ctx, js_set_timer, "setTimeout", 2, JS_CFUNC_generic_magic, 0));
    JS_SetPropertyStr(ctx, g, "setInterval", JS_NewCFunctionMagic(ctx, js_set_timer, "setInterval", 2, JS_CFUNC_generic_magic, 1));
    JS_SetPropertyStr(ctx, g, "clearTimeout", JS_NewCFunction(ctx, js_clear_timer, "clearTimeout", 1));
    JS_SetPropertyStr(ctx, g, "clearInterval", JS_NewCFunction(ctx, js_clear_timer, "clearInterval", 1));
    JSValue ostd = JS_NewObject(ctx), oos = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ostd, std_funcs, N_STD);
    JS_SetPropertyFunctionList(ctx, oos, os_funcs, N_OS);
    JS_SetPropertyStr(ctx, g, "std", ostd);
    JS_SetPropertyStr(ctx, g, "os", oos);
    JSValue sa = JS_NewArray(ctx);
    uint32_t k = 0;
    if (ficheiro) JS_SetPropertyUint32(ctx, sa, k++, JS_NewString(ctx, ficheiro));
    for (int j = ficheiro ? primero_arg + 1 : primero_arg; j < argc; j++)
        JS_SetPropertyUint32(ctx, sa, k++, JS_NewString(ctx, argv[j]));
    JS_SetPropertyStr(ctx, g, "scriptArgs", sa);
    JS_FreeValue(ctx, g);
    JSModuleDef *m = JS_NewCModule(ctx, "std", iniciar_std);
    if (m) JS_AddModuleExportList(ctx, m, std_funcs, N_STD);
    m = JS_NewCModule(ctx, "os", iniciar_os);
    if (m) JS_AddModuleExportList(ctx, m, os_funcs, N_OS);

    if (ejecutar_codigo(qjs_prelude, strlen(qjs_prelude), "<prelude>", false) != 0) return 1;

    int rc = 0;
    if (expr) {
        rc = ejecutar_codigo(expr, strlen(expr), "<cmdline>", forzar_modulo) ? 1 : 0;
    } else if (ficheiro) {
        size_t n;
        char *buf = ler_ficheiro(ficheiro, &n);
        if (!buf) {
            saida_txt(2, "qjs: no se puede leer ");
            saida_txt(2, ficheiro);
            saida_txt(2, "\n");
            return 1;
        }
        bool modulo = forzar_modulo || termina_en(ficheiro, ".mjs") || JS_DetectModule(buf, n);
        rc = ejecutar_codigo(buf, n, ficheiro, modulo) ? 1 : 0;
    } else {
        return repl();
    }
    return rc || codigo_saida ? 1 : 0;
}

/* El programa corre sobre una pila propia (la del sistema tiene 64 KB y el
 * motor recursa mucho): se cambia esp en ensamblador, como en OpenTyrian (D183). */
static int resultado_qjs;
static void correr_qjs(void) { resultado_qjs = qjs_main(g_argc, g_argv); }

int principal(int argc, char **argv)
{
    g_argc = argc;
    g_argv = argv;
    uint8_t *pilha = aloca(PILHA_QJS);
    if (!pilha) { saida_txt(2, "qjs: no hay memoria para la pila\n"); return 1; }
    uint32_t topo = ((uint32_t)(pilha + PILHA_QJS)) & ~15u;
    __asm__ volatile(
        "mov %%esp, %%esi\n\t"
        "mov %0, %%esp\n\t"
        "call *%1\n\t"
        "mov %%esi, %%esp\n\t"
        : : "r"(topo), "r"(correr_qjs) : "esi", "eax", "ecx", "edx", "memory");
    return resultado_qjs;
}
