// SPDX-License-Identifier: GPL-3.0-only
/* java_main.c - el programa «java» de SaguiOS (D376): arranca miniJVM.
 *
 *   java [-Xmx64M] [-cp ruta:ruta] [-jar app.jar] [-version] Clase [argumentos]
 *
 * Basado en el main.c de miniJVM (MIT, (c) 2023 Gust), reescrito para SaguiOS: la biblioteca de
 * clases está en /apps/java/lib/minijvm_rt.jar, el cp por defecto es «.» y no hay interfaz gráfica
 * de miniJVM. Los argumentos llegan en Latin-1 (el terminal) y se pasan a Java en UTF-8. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "sagui.h"

#include "utils/d_type.h"
#include "utils/bytebuf.h"
#include "utils/miniz_wrapper.h"
#include "jvm/jvm_util.h"
#include "jvm/jvm.h"
#include "jvm/garbage.h"

#define RT_JAR "/apps/java/lib/minijvm_rt.jar"

static s64 memoria(const char *a, size_t pre)
{
    char *fin;
    if (strlen(a) <= pre) return -1;
    unsigned long long v = strtoull(a + pre, &fin, 10);
    unsigned long long m = 1;
    if (fin == a + pre || v == 0) return -1;
    if (*fin) {
        if (fin[1]) return -1;
        switch (*fin) {
        case 'k': case 'K': m = 1024ULL; break;
        case 'm': case 'M': m = 1024ULL * 1024; break;
        case 'g': case 'G': m = 1024ULL * 1024 * 1024; break;
        default: return -1;
        }
    }
    return (s64)(v * m);
}

/* Latin-1 -> UTF-8 (nuevo, con malloc). */
static char *latin1_a_utf8(const char *s)
{
    char *r = malloc(strlen(s) * 2 + 1), *p = r;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x80) *p++ = (char)c;
        else { *p++ = (char)(0xC0 | (c >> 6)); *p++ = (char)(0x80 | (c & 0x3F)); }
    }
    *p = 0;
    return r;
}

/* Main-Class del manifiesto de un jar (NULL si no hay). */
static char *clase_principal(const char *jar)
{
    ByteBuf *b = bytebuf_create(0);
    char *res = NULL;
    if (zip_loadfile(jar, "META-INF/MANIFEST.MF", b) == 0 && b->buf) {
        char *t = malloc(b->wp + 1);
        memcpy(t, b->buf, b->wp);
        t[b->wp] = 0;
        char *l = strstr(t, "Main-Class:");
        if (l) {
            l += 11;
            while (*l == ' ') l++;
            char *f = l;
            while (*f && *f != '\r' && *f != '\n') f++;
            *f = 0;
            res = strdup(l);
        }
        free(t);
    }
    bytebuf_destroy(b);
    return res;
}


/* ---- compilar dentro de SaguiOS con Janino (D376) ---- */
#define JANINO_CP "/apps/java/lib/janino.jar:/apps/java/lib/commons-compiler.jar:/apps/java/lib/sagui-javac.jar"

void sagui_gui_registrar(MiniJVM *jvm);

static int termina_en(const char *s, const char *fin)
{
    size_t a = strlen(s), b = strlen(fin);
    return a >= b && !strcmp(s + a - b, fin);
}

/* Ejecuta /apps/bin/java con estos argumentos y devuelve su código de salida. */
static int lanzar_java(char **args)
{
    int pid = executar("/apps/bin/java", args), codigo = 1;
    if (pid < 0) { printf("java: no se pudo lanzar /apps/bin/java\n"); return 1; }
    esperar(pid, &codigo);
    return codigo;
}

/* javac [-d dir] [-cp ruta] Fuente.java...: SaguiJavac (userland/java/javac) con el compilador Janino sobre esta VM. */
static int modo_javac(int argc, char **argv)
{
    char *args[argc + 8];
    int n = 0, i;
    args[n++] = "java"; args[n++] = "-cp"; args[n++] = JANINO_CP;
    args[n++] = "SaguiJavac";
    for (i = 1; i < argc; i++) args[n++] = argv[i];
    args[n] = NULL;
    return lanzar_java(args);
}

/* java Fuente.java [argumentos]: compila a /tmp/java-run-PID y ejecuta la clase con el nombre del fichero. */
static int modo_fuente(int argc, char **argv, int pos, const char *classpath)
{
    char dir[64], cp[1024], clase[128], *p;
    char *c1[8], *c2[argc + 8];
    int n = 0, i, r;
    snprintf(dir, sizeof dir, "/tmp/java-run-%d", meu_pid());
    mkdir(dir, 0755);
    c1[n++] = "javac"; c1[n++] = "-d"; c1[n++] = dir; c1[n++] = argv[pos]; c1[n] = NULL;
    r = modo_javac(n, c1);
    if (r) return r;
    p = strrchr(argv[pos], '/');
    snprintf(clase, sizeof clase, "%s", p ? p + 1 : argv[pos]);
    clase[strlen(clase) - 5] = 0;
    snprintf(cp, sizeof cp, "%s:%s", dir, classpath);
    n = 0;
    c2[n++] = "java"; c2[n++] = "-cp"; c2[n++] = cp; c2[n++] = clase;
    for (i = pos + 1; i < argc; i++) c2[n++] = argv[i];
    c2[n] = NULL;
    return lanzar_java(c2);
}

static void ayuda(void)
{
    printf("Uso: java [-Xmx64M] [-cp ruta:ruta] (-jar app.jar | Clase) [argumentos]\n"
           "     java -version\n");
}

int main(int argc, char **argv)
{
    char *bootclasspath = RT_JAR;
    char *classpath = ".";
    char *main_name = NULL;
    char *jar = NULL;
    s64 maxheap = 128LL * 1024 * 1024;   /* -Xmx cambia el límite del montón */
    ArrayList *java_para = arraylist_create(0);
    int i;

    {   /* «javac» es una copia de este programa: se reconoce por su nombre */
        const char *b = strrchr(argv[0], '/');
        b = b ? b + 1 : argv[0];
        if (!strcmp(b, "javac")) return modo_javac(argc, argv);
    }
    jvm_init_mem_alloc();

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-bootclasspath") && i + 1 < argc) bootclasspath = argv[++i];
        else if ((!strcmp(argv[i], "-cp") || !strcmp(argv[i], "-classpath")) && i + 1 < argc) classpath = argv[++i];
        else if (!strcmp(argv[i], "-jar") && i + 1 < argc) jar = argv[++i];
        else if (!strcmp(argv[i], "-version") || !strcmp(argv[i], "--version")) { main_name = "org.mini.vm.PrintVersion"; classpath = NULL; }
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-help") || !strcmp(argv[i], "--help")) { ayuda(); return 0; }
        else if (!strncmp(argv[i], "-Xmx", 4)) {
            s64 p = memoria(argv[i], 4);
            if (p > 0) maxheap = p; else printf("java: opción de memoria no válida: %s\n", argv[i]);
        } else if (argv[i][0] == '-') {
            /* -D..., -X... y demás: se ignoran */
        } else if (!main_name && termina_en(argv[i], ".java")) {
            return modo_fuente(argc, argv, i, classpath);
        } else if (!main_name) {
            main_name = argv[i++];
            for (; i < argc; i++) arraylist_push_back(java_para, latin1_a_utf8(argv[i]));
            break;
        }
    }
    if (jar) {
        main_name = clase_principal(jar);
        if (!main_name) { printf("java: %s no tiene Main-Class en el manifiesto\n", jar); return 1; }
        for (c8 *p = main_name; *p; p++) if (*p == '/') *p = '.';
        classpath = jar;
    }
    if (!main_name) { ayuda(); return 1; }
    if (classpath) {   /* la biblioteca gráfica sagui.gui va siempre detrás del classpath del usuario */
        static char cpfull[2048];
        snprintf(cpfull, sizeof cpfull, "%s:/apps/java/lib/sagui.jar", classpath);
        classpath = cpfull;
    }

    int ret = 1;
    MiniJVM *jvm = jvm_create();
    if (jvm) {
        jvm->startup_dir = utf8_create_part_c("/apps/java/bin/", 0, 15);
        jvm->jdwp_enable = 0;
        jvm->gc_backend = NULL;
        jvm->max_heap_size = maxheap;
        jvm->max_vm_memory = 0;
        ret = jvm_init(jvm, bootclasspath, classpath);
        if (ret) printf("java: no se pudo iniciar la máquina virtual (¿falta %s?)\n", bootclasspath);
        else {
            sagui_gui_registrar(jvm);
            ret = call_main(jvm, main_name, java_para);
        }
        if (jvm->collector->exit_flag) ret = jvm->collector->exit_code;
        jvm_destroy(jvm);
    }
    fflush(stdout);
    fflush(stderr);
    jvm_destroy_mem_alloc();
    return ret;
}

int principal(int argc, char **argv)
{
    return main(argc, argv);
}
