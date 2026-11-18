// SPDX-License-Identifier: GPL-3.0-only
/* dinamico - programa enlazado en dinámico (D378, fase 2; tests/h386_dinamico.py).
 *
 * Se enlaza con /lib/libsagui.so.1 y /lib/libc.so.1 y lleva PT_INTERP =
 * /lib/ld-sagui.so.1: el kernel carga el intérprete, que abre las bibliotecas,
 * reloca el programa (con R_386_COPY de environ) y salta a _inicio. Comprueba:
 *   - que principal recibe argc/argv y environ (copiado por R_386_COPY);
 *   - que funcionan la libsagui compartida (texto, malloc) y la libc (snprintf, strtol);
 *   - dlopen por el intérprete: libdldep.so llama a una función del programa
 *     (exportada con --export-dynamic-symbol);
 *   - W^X y RELRO: escribir en el código de libsagui.so o en la GOT del
 *     programa mata al proceso;
 *   - revisión de seguridad: un proceso de root (seguro) no carga una
 *     biblioteca de /tmp (carpeta que cualquiera escribe) ni la busca en
 *     SAGUI_LIB_CAMINHO; una cuenta normal sí; una ruta demasiado larga es un
 *     error (antes se cortaba y podía abrir otro fichero).
 *
 *   dinamico                todo; acaba con "RESULTADO dinamico: ..."
 *   dinamico escribir-codigo | escribir-got     (los hijos de la prueba W^X)
 *   dinamico lib-caminho    (el hijo con SAGUI_LIB_CAMINHO=/tmp: +1 si cargó la de /tmp por
 *                            nombre, +2 si por su ruta; root lo lanza también como sagui) */
#include "sagui.h"
#include "contas.h"
#include "ligador.h"
#include "verifica.h"

int snprintf(char *s, size_t n, const char *fmt, ...);
long strtol(const char *s, char **fim, int base);
extern char **environ;

static char registro[128];

/* La llama libdldep.so (constructor y destructor): el ld la exporta en .dynsym. */
void anfitriao_registrar(const char *texto)
{
    junta_texto(registro, texto, sizeof registro);
    junta_texto(registro, ";", sizeof registro);
}

/* Un puntero a una función de libsagui.so: en un programa no PIC, su valor lo
 * escribe el intérprete en la GOT del programa (R_386_GLOB_DAT), que después
 * queda en solo lectura (RELRO). */
static size_t (*const ptr_tam)(const char *) = tam_texto;

static int filho(const char *arg)
{
    char *argv[] = { "/apps/testes/dinamico", (char *)arg, NULL };
    int pid = executar("/apps/testes/dinamico", argv);
    if (pid < 0) return -2000;
    int codigo = -2001;
    if (esperar(pid, &codigo) < 0) return -2002;
    return codigo;
}

/* Copia un fichero pequeño (para poner una biblioteca en /tmp). */
static bool copiar(const char *de, const char *para)
{
    static uint8_t buf[64 * 1024];
    int fd = abrir(de, ABRIR_LER);
    if (fd < 0) return false;
    int n = 0, r;
    while (n < (int)sizeof buf && (r = ler(fd, buf + n, sizeof buf - (unsigned)n)) > 0) n += r;
    fechar(fd);
    if (n <= 0 || n == (int)sizeof buf) return false;
    fd = abrir(para, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return false;
    bool ok = escrever_tudo(fd, buf, (unsigned)n) == n;
    fechar(fd);
    return ok;
}

#define LIB_TMP "/tmp/libdltmp.so"
#define AVISO_SEGURO ": un proceso de root o del sistema solo carga bibliotecas de root"

int principal(int argc, char **argv)
{
    if (argc > 1 && compara_texto(argv[1], "lib-caminho") == 0) {
        void *t = dlopen("libdltmp.so", RTLD_NOW);
        if (!t) imprimir("DINAMICO hijo lib-caminho (uid %u): %s\n", meu_uid(), dlerror());
        int r = t ? 1 : 0;
        if (t) dlclose(t);
        t = dlopen(LIB_TMP, RTLD_NOW);
        if (!t) imprimir("DINAMICO hijo lib-caminho (uid %u): %s\n", meu_uid(), dlerror());
        else { r += 2; dlclose(t); }
        return r;
    }
    if (argc > 1) {
        volatile uint8_t *p;
        if (compara_texto(argv[1], "escribir-codigo") == 0) {
            /* La función de verdad, dentro de libsagui.so (no la entrada de la PLT del programa). */
            p = (volatile uint8_t *)dlsym(dlopen("libsagui.so.1", RTLD_NOW), "imprimir");
        } else {
            /* La entrada de la PLT del programa para imprimir es «jmp *[GOT+n]» (FF 25 dirección):
             * esa dirección es la de su casilla en la GOT, que la RELRO deja en solo lectura. */
            const uint8_t *plt = (const uint8_t *)(void *)imprimir;
            if (plt[0] != 0xFF || plt[1] != 0x25) { imprimir("DINAMICO hijo: PLT inesperada\n"); return 2; }
            p = (volatile uint8_t *)*(uint32_t *)(plt + 2);
        }
        imprimir("DINAMICO hijo: escribiendo en %p\n", (void *)p);
        p[0] = p[0];
        imprimir("DINAMICO hijo: la escritura NO falló\n");
        return 0;
    }
    imprimir("DINAMICO programa con bibliotecas compartidas (D378)\n");
    verifica(argc == 1 && argv && compara_texto(argv[0], "/apps/testes/dinamico") == 0, "principal recibe argc y argv");
    verifica(environ != NULL, "environ copiado por R_386_COPY y puesto por _inicio");
    dl_info_t inf;
    void *ls = dlopen("libsagui.so.1", RTLD_NOW);
    verifica(ls && dl_info(ls, &inf) == 0 && inf.base > (uint32_t)(void *)registro, "libsagui.so.1 cargada (después de la imagen del programa)");
    if (ls) { imprimir("DINAMICO libsagui.so.1 en %x (%u bytes, %u relocaciones)\n", inf.base, inf.tamanho, inf.relocacoes); dlclose(ls); }
    imprimir("DINAMICO tam_texto en %p, principal en %p\n", (void *)tam_texto, (void *)principal);
    verifica(ptr_tam("sagui") == 5, "puntero a función de libsagui.so por la GOT");
    char *m = aloca(100000);
    verifica(m != NULL, "aloca (el malloc de libsagui.so)");
    if (m) { memset(m, 7, 100000); verifica(m[99999] == 7, "memoria del malloc usable"); libera(m); }
    char buf[64];
    snprintf(buf, sizeof buf, "%d-%s-%x", 42, "ok", 255u);
    verifica(compara_texto(buf, "42-ok-ff") == 0, "snprintf de libc.so");
    verifica(strtol("-1234", NULL, 10) == -1234, "strtol de libc.so");

    /* dlopen por el intérprete (la base por ruta: su soname cubre el DT_NEEDED de dep). */
    void *base = dlopen("/apps/testes/lib/libdlbase.so", RTLD_NOW);
    void *dep = dlopen("/apps/testes/lib/libdldep.so", RTLD_NOW);
    if (!dep) imprimir("DINAMICO dlerror: %s\n", dlerror());
    verifica(base && dep, "dlopen desde un programa dinámico");
    if (dep) {
        int (*soma3)(int, int, int) = (int (*)(int, int, int))dlsym(dep, "dl_soma3");
        verifica(soma3 && soma3(40, 1, 1) == 42, "dlsym y llamada a libdldep.so");
        verifica(compara_texto(registro, "dep:iniciar;") == 0, "libdldep.so llamó a una función del programa");
        dlclose(dep);
        verifica(compara_texto(registro, "dep:iniciar;dep:terminar;") == 0, "dlclose llama al destructor");
    }
    if (base) dlclose(base);
    verifica(dlsym(RTLD_DEFAULT, "imprimir") == (void *)imprimir, "dlsym(RTLD_DEFAULT) encuentra libsagui.so");

    /* Revisión de seguridad (D378): bibliotecas que una cuenta normal puede cambiar. */
    bool root = sou_root();
    imprimir("DINAMICO proceso %s\n", root ? "de root (seguro)" : "de una cuenta normal");
    verifica(copiar("/apps/testes/lib/libdlbase.so", LIB_TMP), "copia de libdlbase.so en /tmp");
    void *t = dlopen(LIB_TMP, RTLD_NOW);
    const char *e = t ? "" : dlerror();
    if (!t) imprimir("DINAMICO dlopen %s: %s\n", LIB_TMP, e ? e : "?");
    if (root) verifica(!t && e && comeca_com(e, LIB_TMP AVISO_SEGURO), "root no carga una biblioteca de /tmp");
    else verifica(t != NULL, "una cuenta normal carga su biblioteca de /tmp");
    if (t) dlclose(t);
    char *ev[] = { "SAGUI_LIB_CAMINHO=/tmp", NULL };
    char *av[] = { "/apps/testes/dinamico", "lib-caminho", NULL };
    int pid = executar_env("/apps/testes/dinamico", av, ev), cod = -1;
    if (pid >= 0) esperar(pid, &cod);
    if (root) verifica(cod == 0, "root no busca en SAGUI_LIB_CAMINHO");
    else verifica(cod == 3, "una cuenta normal busca en SAGUI_LIB_CAMINHO");
    if (root && usuario_op(USU_COMO, UID_SAGUI, GID_SAGUI) == 0) {   /* el mismo hijo, como sagui */
        pid = executar_env("/apps/testes/dinamico", av, ev);
        cod = -1;
        if (pid >= 0) esperar(pid, &cod);
        verifica(cod == 3, "sagui (no seguro) carga de /tmp y busca en SAGUI_LIB_CAMINHO");
    }
    apagar(LIB_TMP);
    char largo[200];
    copia_texto(largo, "/apps/testes/lib/", sizeof largo);
    for (int i = (int)tam_texto(largo); i < (int)sizeof largo - 1; i++) largo[i] = 'a';
    largo[sizeof largo - 1] = 0;
    t = dlopen(largo, RTLD_NOW);
    e = t ? "" : dlerror();
    verifica(!t && e && comeca_com(e, "/apps/testes/lib/aaa"), "dlopen con una ruta demasiado larga: error, sin cortarla");

    int r1 = filho("escribir-codigo");
    int r2 = filho("escribir-got");
    imprimir("DINAMICO hijos W^X: %d %d\n", r1, r2);
    verifica(r1 == SAIDA_ENCERRADO_PELO_KERNEL, "escribir en el código de libsagui.so da fallo de página");
    verifica(r2 == SAIDA_ENCERRADO_PELO_KERNEL, "escribir en la GOT del programa (RELRO) da fallo de página");
    return resultado("dinamico");
}
