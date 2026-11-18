// SPDX-License-Identifier: GPL-3.0-only
/* dlprueba - prototipo del enlace dinámico (D378, fase 1; tests/h385_dlopen.py).
 *
 * Abre con dlopen bibliotecas .so compiladas con gcc -fPIC -shared y
 * comprueba, dentro de SaguiOS:
 *   - libdldep.so arrastra a libdlbase.so (DT_NEEDED por DT_RUNPATH=$ORIGIN),
 *     una con DT_GNU_HASH y otra con DT_HASH;
 *   - relocaciones RELATIVE, GLOB_DAT y JMP_SLOT; constructores y
 *     destructores; un símbolo del programa ofrecido con dl_exportar;
 *   - dlsym, dlclose (cuenta de referencias), dlerror;
 *   - W^X: el código y la zona RELRO no se pueden escribir (un hijo que lo
 *     intenta muere con fallo de página);
 *   - una biblioteca sin -fPIC (relocaciones de texto) se rechaza;
 *   - coste del código PIC: SHA-256 estático frente al de libdlsha.so.
 *
 *   dlprueba            todo; acaba con "RESULTADO dlprueba: ..."
 *   dlprueba escribir-codigo | escribir-relro    (el hijo de la prueba W^X) */
#include "sagui.h"
#include "ligador.h"
#include "sha256.h"
#include "verifica.h"

#define LIB "/apps/testes/lib/"

static char registro[128];

/* La llaman los constructores y destructores de libdldep.so. */
static void anfitriao_registrar(const char *texto)
{
    junta_texto(registro, texto, sizeof registro);
    junta_texto(registro, ";", sizeof registro);
}

static const dl_simbolo_t exportados[] = {
    { "anfitriao_registrar", (void *)anfitriao_registrar },
    { NULL, NULL },
};

/* Hijo de la prueba W^X: escribe donde no debe. Si vuelve, la protección falló. */
static int escrever_proibido(const char *onde)
{
    void *dep = dlopen(LIB "libdldep.so", RTLD_NOW);
    void *base = dlopen("libdlbase.so", RTLD_NOW);
    if (!dep || !base) { imprimir("DLPRUEBA hijo: dlopen falló\n"); return 2; }
    void *(*f)(void) = (void *(*)(void))dlsym(base, compara_texto(onde, "escribir-codigo") == 0
                                                   ? "dl_endereco_codigo" : "dl_endereco_tabela");
    volatile uint8_t *p = f();
    imprimir("DLPRUEBA hijo: escribiendo en %p\n", (void *)p);
    p[0] = p[0];   /* fallo de página esperado */
    imprimir("DLPRUEBA hijo: la escritura NO falló\n");
    return 0;
}

static int filho(const char *arg)
{
    char *argv[] = { "/apps/testes/dlprueba", (char *)arg, NULL };
    int pid = executar("/apps/testes/dlprueba", argv);
    if (pid < 0) return -2000;
    int codigo = -2001;
    if (esperar(pid, &codigo) < 0) return -2002;
    return codigo;
}

typedef void (*sha_f)(const void *, unsigned, unsigned char *);

/* Millones de ciclos (RDTSC): el reloj del sistema va en pasos de 10 ms. */
static uint64_t ciclos(void)
{
    uint64_t t;
    __asm__ volatile("rdtsc" : "=A"(t));
    return t;
}

static unsigned medir(sha_f f, const uint8_t *buf, unsigned n, int vezes, unsigned char *saida)
{
    uint64_t t0 = ciclos();
    for (int i = 0; i < vezes; i++) f(buf, n, saida);
    return (unsigned)((ciclos() - t0) / 1000000u);
}

int principal(int argc, char **argv)
{
    dl_exportar(exportados);
    if (argc > 1) return escrever_proibido(argv[1]);

    imprimir("DLPRUEBA prototipo del enlace dinámico (D378)\n");

    /* 1. Dependencias, relocaciones y constructores. */
    void *dep = dlopen(LIB "libdldep.so", RTLD_NOW);
    if (!dep) imprimir("DLPRUEBA dlerror: %s\n", dlerror());
    verifica(dep != NULL, "dlopen libdldep.so (con su dependencia libdlbase.so)");
    if (!dep) return resultado("dlprueba");
    verifica(compara_texto(registro, "dep:iniciar;") == 0, "el constructor de libdldep.so llamó al programa");

    int (*soma3)(int, int, int) = (int (*)(int, int, int))dlsym(dep, "dl_soma3");
    int (*ler_contador)(void) = (int (*)(void))dlsym(dep, "dl_ler_contador");
    int *(*end_contador)(void) = (int *(*)(void))dlsym(dep, "dl_endereco_contador");
    verifica(soma3 && ler_contador && end_contador, "dlsym en libdldep.so (DT_HASH)");
    if (!soma3 || !ler_contador || !end_contador) return resultado("dlprueba");
    verifica(soma3(1, 2, 39) == 42, "dl_soma3 llama a dl_soma de libdlbase.so por la PLT (JMP_SLOT)");
    verifica(ler_contador() == 2, "dl_contador de libdlbase.so leído por la GOT (GLOB_DAT)");

    /* dlsym sobre libdldep.so encuentra también lo de su dependencia. */
    int *contador = (int *)dlsym(dep, "dl_contador");
    verifica(contador && contador == end_contador() && *contador == 2, "dlsym busca en las dependencias y la dirección coincide");

    void *base = dlopen("libdlbase.so", RTLD_NOW);
    dl_info_t ib, id;
    verifica(base && dl_info(base, &ib) == 0 && ib.referencias == 2, "dlopen por soname reutiliza la copia abierta (2 referencias)");
    if (!base) return resultado("dlprueba");
    dl_info(dep, &id);
    imprimir("DLPRUEBA libdlbase.so en %x (%u bytes, %u relocaciones, soname «%s»)\n", ib.base, ib.tamanho, ib.relocacoes, ib.soname);
    imprimir("DLPRUEBA libdldep.so en %x (%u bytes, %u relocaciones)\n", id.base, id.tamanho, id.relocacoes);

    int *iniciado = (int *)dlsym(base, "dl_iniciado");
    verifica(iniciado && *iniciado == 1234, "el constructor de libdlbase.so (.init_array) se ejecutó");
    int (*aplicar)(int, int) = (int (*)(int, int))dlsym(base, "dl_aplicar");
    verifica(aplicar && aplicar(0, 21) == 42 && aplicar(1, 14) == 42, "tabla de punteros a función (RELATIVE en RELRO)");
    const char **saudacao = (const char **)dlsym(base, "dl_saudacao");
    verifica(saudacao && compara_texto(*saudacao, "hola desde libdlbase") == 0, "puntero a texto en .data (RELATIVE)");
    verifica(dlsym(RTLD_DEFAULT, "dl_soma") != NULL, "dlsym(RTLD_DEFAULT) busca en todas las abiertas");
    verifica(dlsym(base, "no_existe") == NULL && dlerror() != NULL && dlerror() == NULL,
             "un símbolo que no existe da NULL, y dlerror lo explica una vez");

    /* 2. Errores: biblioteca que no está y biblioteca sin -fPIC. */
    verifica(dlopen("libnoexiste.so", RTLD_NOW) == NULL && dlerror() != NULL, "dlopen de una biblioteca que no existe falla");
    void *mal = dlopen(LIB "libdlmal.so", RTLD_NOW);
    char *e = mal ? NULL : dlerror();
    if (e) imprimir("DLPRUEBA rechazada: %s\n", e);
    verifica(!mal && e && acha_char(e, 'P') && dl_info(mal, &ib) < 0, "una biblioteca con relocaciones de texto (sin -fPIC) se rechaza");

    /* 3. W^X: escribir en el código o en la RELRO mata al hijo. */
    int r1 = filho("escribir-codigo");
    int r2 = filho("escribir-relro");
    imprimir("DLPRUEBA hijos W^X: %d %d\n", r1, r2);
    verifica(r1 == SAIDA_ENCERRADO_PELO_KERNEL, "escribir en el código de la biblioteca da fallo de página");
    verifica(r2 == SAIDA_ENCERRADO_PELO_KERNEL, "escribir en la zona RELRO da fallo de página");

    /* 4. Cerrar: el destructor corre al llegar a cero referencias. */
    verifica(dlclose(base) == 0 && compara_texto(registro, "dep:iniciar;") == 0, "dlclose con referencias pendientes no descarga");
    verifica(dlclose(dep) == 0 && compara_texto(registro, "dep:iniciar;dep:terminar;") == 0, "dlclose final llama al destructor");
    verifica(dlclose(dep) < 0, "dlclose de una biblioteca ya cerrada falla");
    void *otra = dlopen(LIB "libdldep.so", RTLD_NOW);
    verifica(otra && ((int (*)(void))dlsym(otra, "dl_ler_contador"))() == 0, "reabrir la carga de cero (datos nuevos)");
    if (otra) dlclose(otra);

    /* 5. Coste del PIC: el mismo SHA-256 estático, en .so y en .so con -Bsymbolic. */
    void *sha = dlopen(LIB "libdlsha.so", RTLD_NOW);
    void *shab = dlopen(LIB "libdlsha_simb.so", RTLD_NOW);
    sha_f pic = sha ? (sha_f)dlsym(sha, "sha256") : NULL;
    sha_f picb = shab ? (sha_f)dlsym(shab, "sha256") : NULL;
    verifica(pic && picb, "dlopen libdlsha.so y libdlsha_simb.so");
    if (pic && picb) {
        unsigned n = 2u * 1024 * 1024;
        uint8_t *buf = aloca(n);
        for (unsigned i = 0; i < n; i++) buf[i] = (uint8_t)(i * 7 + (i >> 9));
        unsigned char a[32], b[32], c[32];
        int vezes = 4;
        unsigned te = 0, tp = 0, tb = 0;
        for (int k = 0; k < 3; k++) {   /* alternadas, para repartir el ruido */
            te += medir(sha256, buf, n, vezes, a);
            tp += medir(pic, buf, n, vezes, b);
            tb += medir(picb, buf, n, vezes, c);
        }
        verifica(memcmp(a, b, 32) == 0 && memcmp(a, c, 32) == 0, "el SHA-256 de la .so da lo mismo que el estático");
        imprimir("DLPRUEBA PIC sha256 %u MB: estatico %u Mciclos, so %u Mciclos, so-Bsymbolic %u Mciclos\n",
                 (unsigned)(3 * vezes * (n >> 20)), te, tp, tb);
        libera(buf);
    }
    if (sha) dlclose(sha);
    if (shab) dlclose(shab);
    return resultado("dlprueba");
}
