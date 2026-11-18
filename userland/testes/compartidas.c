// SPDX-License-Identifier: GPL-3.0-only
/* compartidas - prueba de las páginas compartidas de las bibliotecas (D378, fase 3; h390).
 *
 * Programa dinámico (libsagui.so.1, libsagui_medios.so.1 y libc.so.1) que corre como root y
 * usa /apps/testes/lib/libcomp_a.so y libcomp_b.so (iguales salvo una letra de un texto):
 *
 *   compartidas [N [VUELTAS]]
 *       1. MEM_COMPARTIR directo: un fichero de /tmp no se comparte (ERRO_ACESSO); nunca con
 *          escritura; no encima de páginas ya asignadas; MEM_DEVOLVER las quita;
 *       2. N hijos con libcomp.so: la memoria compartida no crece con cada hijo (sí lo mapeado),
 *          cada hijo gasta mucho menos que sin compartir; el contador de .data es de cada uno;
 *       3. libcomp.so sustituida en el disco por la versión B (mismo tamaño y misma fecha): un
 *          hijo nuevo ve la B y los viejos siguen con la A;
 *       4. escribir en el texto o en el código de libcomp.so (también tras pedir escritura con
 *          MEM_PROTEGER) mata al hijo;
 *       5. al terminar los hijos, la memoria compartida y la libre vuelven a lo de antes;
 *       6. VUELTAS vueltas de 4 hijos que abren y cierran libcomp.so: sin fugas;
 *       7. (revisión B1) un hilo pide escritura con MEM_PROTEGER sin parar mientras otro hace
 *          MEM_COMPARTIR: o la llamada falla o el tramo queda sin escritura, y otro proceso lo
 *          ve igual al fichero.
 *       Acaba con «RESULTADO compartidas: ...».
 *   compartidas presion N MB
 *       N hijos que comprueban sin parar sus páginas compartidas mientras el padre llena MB
 *       de memoria (la swap saca las privadas); las compartidas no se sacan ni cambian.
 *   compartidas hijo I | escribir | codigo | proteger | rapido    (los hijos) */
#include "sagui.h"
#include "contas.h"
#include "ligador.h"
#include "verifica.h"

#define LIB      "/apps/testes/lib/libcomp.so"
#define LIB_A    "/apps/testes/lib/libcomp_a.so"
#define LIB_B    "/apps/testes/lib/libcomp_b.so"
#define PASTA    "/tmp/comp"
#define PROG     "/apps/testes/compartidas"
#define MAX_HIJOS 32
#define SOMA_OK  (4096u * 0x5A5A5A5Au)

typedef const char *(*f_texto_t)(void);
typedef int (*f_int_t)(void);
typedef unsigned (*f_soma_t)(void);
typedef void *(*f_ptr_t)(void);

static int mem_compartir(void *ini, unsigned n, int fd, int prot, unsigned desloc)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r)
                     : "a"(SIS_MEMORIA), "b"(MEM_COMPARTIR), "c"(ini), "d"(n), "S"((unsigned)fd << 8 | (unsigned)prot),
                       "D"(desloc)
                     : "memory");
    return r;
}

static void info(info_memoria_t *m)
{
    memset(m, 0, sizeof *m);
    info_sistema(INFO_MEMORIA, m, sizeof *m);
}

static void mostrar(const char *et, const info_memoria_t *m)
{
    imprimir("COMPARTIDAS %s: libre %u KB, compartida %u KB en %u objetos, mapeada %u KB, reutilizadas %u\n", et,
             m->livre_kb, m->compartida_kb, m->compartida_objetos, m->compartida_mapeada_kb, m->compartida_reutilizadas);
}

static bool copiar(const char *de, const char *para)
{
    static uint8_t buf[512 * 1024];
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

static bool gravar(const char *caminho, const char *texto)
{
    char tmp[64];
    formatar_texto(tmp, sizeof tmp, "%s.tmp", caminho);
    int fd = abrir(tmp, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return false;
    bool ok = escrever_tudo(fd, texto, (unsigned)tam_texto(texto)) == (int)tam_texto(texto);
    fechar(fd);
    apagar(caminho);
    return ok && renomear(tmp, caminho) == 0;
}

static int ler_texto(const char *caminho, char *buf, int max)
{
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return -1;
    int n = ler(fd, buf, (unsigned)max - 1);
    fechar(fd);
    if (n < 0) n = 0;
    buf[n] = 0;
    return n;
}

static void *abrir_comp(f_texto_t *t, f_int_t *inc, f_soma_t *soma)
{
    void *b = dlopen(LIB, RTLD_NOW);
    if (!b) { imprimir("COMPARTIDAS dlopen: %s\n", dlerror()); return NULL; }
    *t = (f_texto_t)dlsym(b, "comp_texto");
    *inc = (f_int_t)dlsym(b, "comp_inc");
    *soma = (f_soma_t)dlsym(b, "comp_soma");
    if (!*t || !*inc || !*soma) return NULL;
    return b;
}

static int lancar_e_esperar(const char *modo);

/* ------------------------------------------------- la carrera (revisión B1) */

#define CARRERA_DESLOC 0x2000u              /* el segmento de solo lectura de libcomp (comp_grande) */
#define CARRERA_PAGINAS 64u
static volatile int carrera_parar;
static volatile unsigned carrera_ini, carrera_dadas;

/* ¿Puede el kernel escribir en la página (usuario_ok con escritura)? Se lee un byte del propio
 * fichero en su sitio: si la página fuera escribible, el contenido no cambia igualmente. */
static bool escribible(int fd, char *pag, unsigned desloc)
{
    if (posicionar(fd, (int)desloc, POS_INICIO) < 0) return false;
    return ler(fd, pag, 1) == 1;
}

/* El atacante: pide escritura sin parar sobre el tramo que el otro hilo está compartiendo.
 * Solo cuenta las veces que la página quedó de verdad escribible (MEM_PROTEGER devuelve 0
 * también sobre páginas ya compartidas, que no cambia). Usa su propio descriptor. */
static void atacante(void)
{
    int fd = abrir(LIB_A, ABRIR_LER);
    while (!carrera_parar)
        if (memoria_proteger((void *)carrera_ini, CARRERA_PAGINAS * 4096u, MEM_PROT_ESCRITA) == 0 && fd >= 0 &&
            escribible(fd, (char *)carrera_ini, CARRERA_DESLOC))
            carrera_dadas++;
    if (fd >= 0) fechar(fd);
    hilo_sair(0);
}

/* El nieto: mapea el mismo tramo de libcomp_a.so (tiene que reutilizar el objeto que su padre
 * aún tiene) y comprueba que es igual al fichero. 0 = bien. */
static int verificar_a(void)
{
    int fd = abrir(LIB_A, ABRIR_LER);
    char *r = (char *)memoria_paginas(MEM_RESERVAR, 0, CARRERA_PAGINAS * 4096u);
    if (fd < 0 || (unsigned)r < 0x80000000u) return 2;
    int c = mem_compartir(r, CARRERA_PAGINAS * 4096u, fd, 0, CARRERA_DESLOC);
    static char buf[4096];
    for (unsigned i = 0; c >= 0 && i < CARRERA_PAGINAS; i++) {
        posicionar(fd, (int)(CARRERA_DESLOC + i * 4096u), POS_INICIO);
        int n = ler(fd, buf, 4096);
        if (n <= 0 || memcmp(buf, r + i * 4096u, (unsigned)n) != 0) return 3;
    }
    imprimir("COMPARTIDAS nieto: MEM_COMPARTIR = %d, igual al fichero\n", c);
    return c == 1 ? 0 : 4;
}

static int carrera(void)
{
    int fd = abrir(LIB_A, ABRIR_LER);
    unsigned n = CARRERA_PAGINAS * 4096u;
    char *r = (char *)memoria_paginas(MEM_RESERVAR, 0, n);
    if (fd < 0 || (unsigned)r < 0x80000000u) return 100;
    carrera_ini = (unsigned)r;
    int tid = hilo_criar_kernel(atacante, NULL);
    if (tid < 0) return 101;
    unsigned bien = 0, rechazos = 0, alias = 0, otros = 0;
    for (int k = 0; k < 150; k++) {
        int c = mem_compartir(r, n, fd, 0, CARRERA_DESLOC);
        if (c >= 0) {
            bien++;
            for (unsigned i = 0; i < CARRERA_PAGINAS; i++)
                if (escribible(fd, r + i * 4096u, CARRERA_DESLOC + i * 4096u)) { alias++; break; }
        } else if (c == ERRO_INVALIDO) rechazos++;
        else otros++;
        memoria_paginas(MEM_DEVOLVER, r, n);
    }
    carrera_parar = 1;
    int cod;
    hilo_esperar(tid, &cod);
    imprimir("COMPARTIDAS carrera: %u compartidas, %u rechazadas (escritura dada a mitad), %u otros errores, "
             "%u alias escribibles; MEM_PROTEGER dio escritura %u veces\n", bien, rechazos, otros, alias, carrera_dadas);
    /* Lo que queda compartido lo ve igual otro proceso. */
    int c = mem_compartir(r, n, fd, 0, CARRERA_DESLOC);
    int nieto = -1;
    if (c >= 0) nieto = lancar_e_esperar("verificar-a");
    imprimir("COMPARTIDAS carrera: el nieto devolvió %d\n", nieto);
    if (alias) return 1;
    if (c < 0 || nieto != 0) return 2;
    return 0;
}

/* ------------------------------------------------------------------ los hijos */

static int hijo(int argc, char **argv)
{
    const char *modo = argv[1];
    if (compara_texto(modo, "carrera") == 0) return carrera();
    if (compara_texto(modo, "verificar-a") == 0) return verificar_a();
    f_texto_t t;
    f_int_t inc;
    f_soma_t soma;
    void *b = abrir_comp(&t, &inc, &soma);
    if (!b) return 90;
    if (compara_texto(modo, "rapido") == 0) {
        bool ok = soma() == SOMA_OK && inc() == 8;
        dlclose(b);
        b = abrir_comp(&t, &inc, &soma);
        ok = ok && b && soma() == SOMA_OK;
        return ok ? 0 : 91;
    }
    if (compara_texto(modo, "escribir") == 0 || compara_texto(modo, "codigo") == 0 ||
        compara_texto(modo, "proteger") == 0) {
        volatile char *p = (volatile char *)t();
        if (compara_texto(modo, "codigo") == 0) p = (volatile char *)((f_ptr_t)dlsym(b, "comp_endereco_codigo"))();
        if (compara_texto(modo, "proteger") == 0) {
            void *pag = (void *)((unsigned)p & ~4095u);
            int r = memoria_proteger(pag, 4096, MEM_PROT_ESCRITA);
            imprimir("COMPARTIDAS hijo proteger: MEM_PROTEGER = %d\n", r);
        }
        imprimir("COMPARTIDAS hijo %s: escribiendo en %p\n", modo, (void *)p);
        p[0] = 'Z';
        imprimir("COMPARTIDAS hijo %s: la escritura NO falló (%c)\n", modo, p[0]);
        return 0;
    }
    /* hijo I: el contador (privado) sube I+1 veces desde 7; después escribe su resultado y
     * se queda comprobando sus páginas hasta que lo maten. */
    int i = (int)texto_para_int(argc > 2 ? argv[2] : "0");
    int c = 0;
    for (int k = 0; k <= i; k++) c = inc();
    char res[96], cam[48];
    formatar_texto(res, sizeof res, "%s %d %u", t(), c, soma());
    formatar_texto(cam, sizeof cam, PASTA "/r%d", i);
    gravar(cam, res);
    formatar_texto(cam, sizeof cam, PASTA "/v%d", i);
    char ultimo[96] = "";
    for (;;) {
        formatar_texto(res, sizeof res, "%s %u", t(), soma());
        if (soma() != SOMA_OK) formatar_texto(res, sizeof res, "ERRO %s %u", t(), soma());
        if (compara_texto(res, ultimo) != 0) { gravar(cam, res); copia_texto(ultimo, res, sizeof ultimo); }
        dormir(100);
    }
}

static int lancar(const char *modo, int i)
{
    char num[12];
    formatar_texto(num, sizeof num, "%d", i);
    char *argv[] = { PROG, (char *)modo, num, NULL };
    return executar(PROG, argv);
}

static int lancar_e_esperar(const char *modo)
{
    int pid = lancar(modo, 0), cod = -2000;
    if (pid >= 0) esperar(pid, &cod);
    return cod;
}

/* Espera a que exista "caminho" (el hijo ya escribió su resultado). */
static bool esperar_arquivo(const char *caminho, char *buf, int max, unsigned ms)
{
    unsigned t0 = tempo_ms();
    while (tempo_ms() - t0 < ms) {
        if (ler_texto(caminho, buf, max) > 0) return true;
        dormir(50);
    }
    return false;
}

static void terminar(int *pids, int n)
{
    for (int i = 0; i < n; i++)
        if (pids[i] > 0) {
            int cod;
            matar(pids[i]);
            esperar(pids[i], &cod);
            pids[i] = 0;
        }
}

static unsigned memoria_do_processo(int pid)
{
    static info_processo_t v[64];
    int n = info_sistema(INFO_PROCESSOS, v, 64);
    for (int i = 0; i < n; i++)
        if (v[i].pid == pid) return v[i].memoria_kb;
    return 0;
}

/* ------------------------------------------------------------------ las partes */

static void parte_directa(void)
{
    imprimir("COMPARTIDAS 1. MEM_COMPARTIR directo\n");
    int fd = abrir(LIB, ABRIR_LER);
    char *r = (char *)memoria_paginas(MEM_RESERVAR, 0, 3 * 4096);
    verifica(fd >= 0 && (unsigned)r >= 0x80000000u, "abrir libcomp.so y reservar 3 páginas");
    if (fd < 0 || (unsigned)r < 0x80000000u) return;
    int a = mem_compartir(r, 4096, fd, 0, 0);
    verifica(a >= 0 && r[0] == 0x7F && r[1] == 'E', "MEM_COMPARTIR de la primera página (la cabecera ELF)");
    verifica(mem_compartir(r, 4096, fd, 0, 0) == ERRO_INVALIDO, "no encima de una página ya mapeada");
    verifica(mem_compartir(r + 4096, 4096, fd, MEM_PROT_ESCRITA, 0) == ERRO_INVALIDO, "nunca con escritura");
    verifica(mem_compartir(r + 4096, 4096, fd, 0, 1u << 30) == ERRO_INVALIDO, "más allá del final del fichero: error");
    verifica(mem_compartir(r + 4096, 4096, fd, 0, 0x43000) == ERRO_INVALIDO,
             "revisión I1: fuera de los PT_LOAD sin escritura (la página de .data): error");
    int fx = abrir(PROG, ABRIR_LER);
    verifica(fx >= 0 && mem_compartir(r + 4096, 4096, fx, 0, 0) == ERRO_INVALIDO,
             "revisión I1: un fichero que no es una biblioteca (ET_EXEC): error");
    if (fx >= 0) fechar(fx);
    verifica(memoria_paginas(MEM_ASIGNAR, r + 2 * 4096, 4096) == 0 && mem_compartir(r + 2 * 4096, 4096, fd, 0, 0) == ERRO_INVALIDO,
             "no encima de una página propia");
    verifica(memoria_paginas(MEM_SACAR, r, 4096) == 0 && r[0] == 0x7F, "MEM_SACAR no saca la compartida");
    verifica(memoria_paginas(MEM_DEVOLVER, r, 4096) == 1, "MEM_DEVOLVER la quita");
    verifica(mem_compartir(r, 4096, fd, MEM_PROT_EXEC, 0) >= 0 && r[1] == 'E', "y el hueco vuelve a servir");
    memoria_paginas(MEM_DEVOLVER, r, 3 * 4096);
    fechar(fd);
    verifica(copiar(LIB_A, PASTA "/x.so"), "copia de libcomp.so en /tmp");
    fd = abrir(PASTA "/x.so", ABRIR_LER);
    verifica(fd >= 0 && mem_compartir(r, 4096, fd, 0, 0) == ERRO_ACESSO, "un fichero de /tmp no se comparte (ERRO_ACESSO)");
    if (fd >= 0) fechar(fd);
    apagar(PASTA "/x.so");
}

static int pids[MAX_HIJOS + 1];

static void parte_hijos(int n, info_memoria_t *m0)
{
    imprimir("COMPARTIDAS 2. %d hijos con libcomp.so\n", n);
    info_memoria_t a, b;
    info(&a);
    mostrar("antes", &a);
    unsigned kb_proc = 0;
    for (int i = 0; i < n; i++) {
        pids[i] = lancar("hijo", i);
        char buf[96], cam[48];
        formatar_texto(cam, sizeof cam, PASTA "/r%d", i);
        bool ok = pids[i] >= 0 && esperar_arquivo(cam, buf, sizeof buf, 60000);
        char esp[96];
        formatar_texto(esp, sizeof esp, "COMP-VERSAO-A %d %u", 7 + i + 1, SOMA_OK);
        if (!ok || compara_texto(buf, esp) != 0) {
            imprimir("COMPARTIDAS hijo %d: «%s» (esperado «%s»)\n", i, ok ? buf : "sin resultado", esp);
            verifica(false, "un hijo con libcomp.so");
        }
        if (i == 0) {
            info_memoria_t x;
            info(&x);
            mostrar("con 1 hijo", &x);
        }
    }
    dormir(300);
    info(&b);
    mostrar("con todos", &b);
    kb_proc = memoria_do_processo(pids[0]);
    unsigned por_hijo = (a.livre_kb - b.livre_kb) / (unsigned)n;
    unsigned mapeado = (b.compartida_mapeada_kb - a.compartida_mapeada_kb) / (unsigned)n;
    imprimir("COMPARTIDAS por hijo: %u KB gastados de verdad, %u KB compartidos (sin compartir serian %u KB); "
             "el Administrador de tareas le cuenta %u KB\n", por_hijo, mapeado, por_hijo + mapeado, kb_proc);
    verifica(mapeado >= 900, "cada hijo mapea más de 900 KB compartidos (libsagui, medios, libc, libcomp)");
    verifica(b.compartida_kb <= a.compartida_kb + 300, "lo compartido no crece con los hijos (solo libcomp.so una vez)");
    verifica(b.compartida_reutilizadas >= a.compartida_reutilizadas + (unsigned)(n - 1) * 4, "los hijos reutilizan las páginas");
    verifica(por_hijo < mapeado, "cada hijo gasta menos de lo que comparte");
    /* Revisión I1: a cada uno se le cuenta su parte de lo compartido (lo mapeado entre los que lo usan). */
    verifica(kb_proc > 0 && kb_proc < por_hijo + mapeado / 3 + 200,
             "el Administrador de tareas cuenta a cada hijo solo su parte de las compartidas");
    (void)m0;
}

static void parte_sustituir(int n)
{
    imprimir("COMPARTIDAS 3. libcomp.so sustituida en el disco\n");
    datas_arquivo_t d;
    info_memoria_t a, b;
    info(&a);
    bool ok = datas_arquivo(LIB, &d) == 0 && copiar(LIB_B, LIB) && fixar_datas(LIB, d.modificado, d.acessado) == 0;
    datas_arquivo_t d2;
    verifica(ok && datas_arquivo(LIB, &d2) == 0 && d2.modificado == d.modificado, "versión B con el mismo tamaño y la misma fecha");
    pids[n] = lancar("hijo", n);
    char buf[96], cam[48], esp[96];
    formatar_texto(cam, sizeof cam, PASTA "/r%d", n);
    formatar_texto(esp, sizeof esp, "COMP-VERSAO-B %d %u", 7 + n + 1, SOMA_OK);
    ok = esperar_arquivo(cam, buf, sizeof buf, 60000);
    if (!ok || compara_texto(buf, esp) != 0) imprimir("COMPARTIDAS hijo nuevo: «%s»\n", ok ? buf : "sin resultado");
    verifica(ok && compara_texto(buf, esp) == 0, "un hijo nuevo ve la versión B (no las páginas viejas)");
    dormir(400);
    bool viejos = true;
    for (int i = 0; i < n; i++) {
        formatar_texto(cam, sizeof cam, PASTA "/v%d", i);
        formatar_texto(esp, sizeof esp, "COMP-VERSAO-A %u", SOMA_OK);
        if (ler_texto(cam, buf, sizeof buf) <= 0 || compara_texto(buf, esp) != 0) {
            imprimir("COMPARTIDAS hijo viejo %d: «%s»\n", i, buf);
            viejos = false;
        }
    }
    verifica(viejos, "los hijos viejos siguen con la versión A");
    info(&b);
    mostrar("con la B", &b);
    verifica(b.compartida_kb > a.compartida_kb, "la versión B ocupa sus propias páginas");
}

static void parte_escribir(void)
{
    imprimir("COMPARTIDAS 4. W^X en las páginas compartidas\n");
    int r1 = lancar_e_esperar("escribir"), r2 = lancar_e_esperar("codigo"), r3 = lancar_e_esperar("proteger");
    imprimir("COMPARTIDAS hijos que escriben: %d %d %d\n", r1, r2, r3);
    verifica(r1 == SAIDA_ENCERRADO_PELO_KERNEL, "escribir en el texto de solo lectura de libcomp.so: fallo de página");
    verifica(r2 == SAIDA_ENCERRADO_PELO_KERNEL, "escribir en el código de libcomp.so: fallo de página");
    verifica(r3 == SAIDA_ENCERRADO_PELO_KERNEL, "tampoco después de pedir escritura con MEM_PROTEGER");
}

static bool igual_a(const info_memoria_t *x, const info_memoria_t *y, unsigned holgura_kb)
{
    unsigned dl = x->livre_kb > y->livre_kb ? x->livre_kb - y->livre_kb : y->livre_kb - x->livre_kb;
    return x->compartida_kb == y->compartida_kb && x->compartida_objetos == y->compartida_objetos &&
           x->compartida_mapeada_kb == y->compartida_mapeada_kb && dl <= holgura_kb;
}

static void parte_vueltas(int vueltas, const info_memoria_t *m0)
{
    imprimir("COMPARTIDAS 6. %d vueltas de 4 hijos que abren y cierran libcomp.so\n", vueltas);
    int erros = 0;
    for (int v = 0; v < vueltas; v++) {
        int p[4];
        for (int i = 0; i < 4; i++) p[i] = lancar("rapido", i);
        for (int i = 0; i < 4; i++) {
            int cod = -1;
            if (p[i] < 0 || esperar(p[i], &cod) < 0 || cod != 0) erros++;
        }
    }
    info_memoria_t m;
    info(&m);
    mostrar("tras las vueltas", &m);
    verifica(erros == 0, "todos los hijos de las vueltas bien");
    verifica(igual_a(&m, m0, 1024), "sin fugas: compartida, mapeada y libre como al principio");
}

static int presion(int n, unsigned mb)
{
    imprimir("COMPARTIDAS presion: %d hijos y %u MB\n", n, mb);
    criar_pasta(PASTA);
    verifica(copiar(LIB_A, LIB), "libcomp.so = versión A");
    info_memoria_t m0;
    info(&m0);
    for (int i = 0; i < n; i++) pids[i] = lancar("hijo", i);
    char buf[96], cam[48];
    for (int i = 0; i < n; i++) {
        formatar_texto(cam, sizeof cam, PASTA "/r%d", i);
        verifica(esperar_arquivo(cam, buf, sizeof buf, 60000), "hijo listo");
    }
    uint32_t paginas = mb * 256u, erros = 0, troca_max = 0;
    uint8_t *area = aumentar_memoria(paginas * 4096u);
    verifica(area != NULL, "memoria para la presión");
    for (int volta = 0; area && volta < 2; volta++) {
        for (uint32_t p = 0; p < paginas; p++) {
            uint32_t *w = (uint32_t *)(area + p * 4096u);
            if (volta && w[0] != p * 2654435761u) erros++;
            w[0] = p * 2654435761u;
            w[1] = p;
        }
        info_memoria_t m;
        info(&m);
        if (m.troca_kb > troca_max) troca_max = m.troca_kb;
        mostrar(volta ? "presion 2" : "presion 1", &m);
    }
    imprimir("COMPARTIDAS swap maxima %u KB\n", troca_max);
    bool bien = true;
    for (int i = 0; i < n; i++) {
        formatar_texto(cam, sizeof cam, PASTA "/v%d", i);
        char esp[96];
        formatar_texto(esp, sizeof esp, "COMP-VERSAO-A %u", SOMA_OK);
        if (ler_texto(cam, buf, sizeof buf) <= 0 || compara_texto(buf, esp) != 0) {
            imprimir("COMPARTIDAS hijo %d: «%s»\n", i, buf);
            bien = false;
        }
    }
    verifica(erros == 0, "la memoria del padre bien tras la swap");
    verifica(troca_max > 0, "la swap se usó");
    verifica(bien, "los hijos leen siempre bien sus páginas compartidas");
    terminar(pids, n);
    return resultado("compartidas");
}

int principal(int argc, char **argv)
{
    if (argc > 1 && compara_texto(argv[1], "presion") != 0) return hijo(argc, argv);
    if (argc > 1) return presion(argc > 2 ? (int)texto_para_int(argv[2]) : 4, argc > 3 ? (unsigned)texto_para_int(argv[3]) : 256);
    int vueltas = 20;
    imprimir("COMPARTIDAS páginas compartidas de las bibliotecas (D378, fase 3), uid %u\n", meu_uid());
    criar_pasta(PASTA);
    verifica(copiar(LIB_A, LIB), "libcomp.so = versión A");
    info_memoria_t m0;
    info(&m0);
    mostrar("al empezar", &m0);
    verifica(m0.compartida_objetos >= 3 && m0.compartida_mapeada_kb >= 900,
             "el propio programa ya comparte libsagui, medios y libc");
    parte_directa();
    info_memoria_t m1;
    info(&m1);
    verifica(igual_a(&m1, &m0, 256), "tras MEM_COMPARTIR directo, todo como al principio");
    parte_hijos(8, &m0);
    parte_sustituir(8);
    terminar(pids, 9);
    dormir(200);
    info_memoria_t m2;
    info(&m2);
    mostrar("hijos terminados", &m2);
    verifica(igual_a(&m2, &m0, 1024), "5. al terminar los hijos vuelve todo (compartida, mapeada y libre)");
    parte_escribir();
    imprimir("COMPARTIDAS 7. carrera: MEM_PROTEGER con escritura mientras otro hilo comparte\n");
    int rc = lancar_e_esperar("carrera");
    verifica(rc == 0, "nunca un alias escribible de un objeto compartido, y otro proceso lo ve igual al fichero");
    parte_vueltas(vueltas, &m0);
    return resultado("compartidas");
}
