// SPDX-License-Identifier: GPL-2.0-or-later
/* troca - prueba de la swap comprimida en RAM (D352, h354).
 *
 *   troca llenar MB VUELTAS [HILOS]
 *       reserva MB (en bloques de 16 MB, con aumentar_memoria), llena cada página con un
 *       contenido que depende de la página, la palabra y una versión, y da VUELTAS vueltas:
 *       en cada una comprueba TODAS las palabras y las reescribe con la versión siguiente.
 *       Con HILOS > 1, cada hilo tiene sus palabras (j % HILOS) en TODAS las páginas: los
 *       hilos tocan a la vez las mismas páginas mientras la swap las saca y las trae, y otro
 *       hilo más saca trozos al azar con MEM_SACAR. Las páginas son de cuatro clases: casi
 *       todas muy comprimibles, algunas a medias, algunas aleatorias (no se sacan) y algunas
 *       a cero.
 *   troca varios K MB VUELTAS
 *       K procesos "llenar" a la vez; acaba con «varios: K de K procesos ...: todos ok».
 *   troca lector ARCHIVO SEGUNDOS
 *       un hilo lee el archivo en trozos de 256 KB a un búfer y compara con la primera
 *       lectura, mientras otro saca ese búfer a la swap (MEM_SACAR) sin parar: cuando la
 *       lectura duerme en el disco, el kernel encuentra el búfer fuera y lo trae en su
 *       propio fallo de página (D352: lo que la D351 vigila para MEM_DEVOLVER).
 *   troca info
 *       la línea de la swap de info_memoria. */
#include "sagui.h"

#define BLOCO   (16u * 1024u * 1024u)
#define PALAVRAS 1024u
#define MAX_HILOS 8u

static uint32_t semente;
static uint32_t n_hilos = 1;
static uint32_t *area;
static uint32_t paginas;
static volatile uint32_t erros_total;
static volatile uint32_t troca_max_kb;
static volatile int parar_sacador;

static inline uint32_t misturar(uint32_t p, uint32_t j, uint32_t v)
{
    uint32_t h = p * 0x9E3779B1u ^ j * 0x85EBCA77u ^ v * 0xC2B2AE3Du ^ semente;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

/* Lo que tiene que haber en la palabra j de la página p con la versión v. */
static inline uint32_t esperado(uint32_t p, uint32_t j, uint32_t v)
{
    uint32_t classe = (p * 2654435761u) >> 28;
    uint32_t base = misturar(p, 0xFFFF, 0);
    if (classe < 12) return base ^ ((j & 7) * 0x01010101u) ^ (v << 20);   /* muy comprimible */
    if (classe < 14) return j < 512 ? misturar(p, j, v) : base ^ v;       /* a medias */
    if (classe == 14) return misturar(p, j, v);                           /* no comprime */
    return 0;                                                             /* a cero */
}

static void info_troca(info_memoria_t *m)
{
    info_sistema(INFO_MEMORIA, m, sizeof *m);
    if (m->troca_kb > troca_max_kb) troca_max_kb = m->troca_kb;
}

typedef struct {
    uint32_t hilo;
    uint32_t voltas;
    uint32_t erros;
} trabalho_t;

/* Una vuelta del hilo t: comprueba sus palabras con la versión v y escribe v + 1. */
static uint32_t volta(uint32_t t, uint32_t v)
{
    uint32_t erros = 0;
    for (uint32_t p = 0; p < paginas; p++) {
        uint32_t *w = area + p * PALAVRAS;
        for (uint32_t j = t; j < PALAVRAS; j += n_hilos) {
            if (w[j] != esperado(p, j, v)) {
                if (erros < 4)
                    imprimir("troca: ERROR pagina %u palabra %u: %08x, esperaba %08x (version %u)\n", p, j, w[j],
                             esperado(p, j, v), v);
                erros++;
            }
            w[j] = esperado(p, j, v + 1);
        }
    }
    return erros;
}

static void *hilo_trabalho(void *arg)
{
    trabalho_t *tr = arg;
    for (uint32_t v = 1; v <= tr->voltas; v++) {
        tr->erros += volta(tr->hilo, v);
        if (tr->hilo == 0) {
            info_memoria_t m;
            info_troca(&m);
            imprimir("troca: vuelta %u: %u errores; swap %u MB (comprimidos %u MB), salidas %u, entradas %u\n", v,
                     tr->erros, m.troca_kb / 1024, m.troca_comprimida_kb / 1024, m.troca_saidas, m.troca_entradas);
        }
    }
    return NULL;
}

/* Saca trozos de 1 MB al azar mientras los demás trabajan. */
static void *hilo_sacador(void *arg)
{
    (void)arg;
    uint32_t x = semente | 1, vezes = 0;
    while (!parar_sacador) {
        x = x * 1103515245u + 12345u;
        uint32_t mb = paginas / 256;
        if (!mb) break;
        uint32_t ini = ((x >> 8) % mb) * 256;
        memoria_paginas(MEM_SACAR, area + ini * PALAVRAS, 256 * 4096);
        vezes++;
        dormir(20);
    }
    return (void *)vezes;
}

static int llenar(uint32_t mb, uint32_t voltas, uint32_t hilos)
{
    if (hilos < 1) hilos = 1;
    if (hilos > MAX_HILOS) hilos = MAX_HILOS;
    n_hilos = hilos;
    semente = (uint32_t)meu_pid() * 0x9E3779B9u;
    uint32_t feito = 0;
    uint8_t *ini = NULL;
    while (feito < mb) {
        uint32_t n = mb - feito >= 16 ? BLOCO : (mb - feito) * 1024u * 1024u;
        uint8_t *b = aumentar_memoria(n);
        if (!b) break;
        if (!ini) ini = b;
        feito += n / (1024u * 1024u);
    }
    int pid = meu_pid();
    if (!ini || feito < mb) {
        imprimir("troca %d: el kernel solo dio %u de %u MB\n", pid, feito, mb);
        return 2;
    }
    area = (uint32_t *)ini;
    paginas = feito * 256u;
    imprimir("troca %d: %u MB reservados en 0x%08x\n", pid, feito, (uint32_t)ini);
    /* Versión 1 en todas las palabras de todos los hilos. */
    for (uint32_t p = 0; p < paginas; p++) {
        uint32_t *w = area + p * PALAVRAS;
        for (uint32_t j = 0; j < PALAVRAS; j++) w[j] = esperado(p, j, 1);
    }
    info_memoria_t m;
    info_troca(&m);
    imprimir("troca %d: lleno; swap %u MB (comprimidos %u MB, almacen %u MB de %u MB)\n", pid, m.troca_kb / 1024,
             m.troca_comprimida_kb / 1024, m.troca_armazem_kb / 1024, m.troca_teto_kb / 1024);
    static trabalho_t tr[MAX_HILOS];
    thread_t *th[MAX_HILOS] = { 0 };
    thread_t *sacador = NULL;
    uint32_t t0 = tempo_ms();
    if (hilos > 1 && m.troca_teto_kb) sacador = thread_create(hilo_sacador, NULL, 0);
    for (uint32_t t = 0; t < hilos; t++) {
        tr[t].hilo = t;
        tr[t].voltas = voltas;
        tr[t].erros = 0;
        if (t) th[t] = thread_create(hilo_trabalho, &tr[t], 0);
    }
    hilo_trabalho(&tr[0]);
    uint32_t erros = tr[0].erros;
    for (uint32_t t = 1; t < hilos; t++) {
        if (th[t]) thread_join(th[t]);
        else erros++;
        erros += tr[t].erros;
    }
    parar_sacador = 1;
    int sacadas = sacador ? thread_join(sacador) : 0;
    info_troca(&m);
    imprimir("troca %d: llenar %u MB, %u vueltas, %u hilos: %s (%u errores) en %u ms; swap maxima %u MB, "
             "salidas %u, entradas %u, MEM_SACAR %d veces\n",
             pid, mb, voltas, hilos, erros ? "FALLO" : "ok", erros, tempo_ms() - t0, troca_max_kb / 1024,
             m.troca_saidas, m.troca_entradas, sacadas);
    return erros ? 3 : 0;
}

static int varios(uint32_t k, uint32_t mb, uint32_t voltas)
{
    if (k < 1 || k > 16) return 1;
    char n_mb[12], n_v[12];
    formatar_texto(n_mb, sizeof n_mb, "%u", mb);
    formatar_texto(n_v, sizeof n_v, "%u", voltas);
    char *argv[] = { "/testes/troca", "llenar", n_mb, n_v, NULL };
    int pids[16];
    for (uint32_t i = 0; i < k; i++) {
        pids[i] = executar("/testes/troca", argv);
        if (pids[i] < 0) { imprimir("varios: no se pudo lanzar el proceso %u (%d)\n", i, pids[i]); return 1; }
    }
    uint32_t ok = 0;
    for (uint32_t i = 0; i < k; i++) {
        int c = -1;
        esperar(pids[i], &c);
        if (c == 0) ok++;
        else imprimir("varios: el proceso %d termino con %d\n", pids[i], c);
    }
    info_memoria_t m;
    info_troca(&m);
    imprimir("varios: %u de %u procesos con %u MB cada uno: %s; swap ahora %u MB, salidas %u, entradas %u\n", ok, k,
             mb, ok == k ? "todos ok" : "FALLO", m.troca_kb / 1024, m.troca_saidas, m.troca_entradas);
    return ok == k ? 0 : 1;
}

/* ---- lector: el kernel escribe en un búfer que está en la swap ---- */

#define TROZO (256u * 1024u)
static uint8_t *buf_lector;
static volatile int fim_lector;

static void *hilo_sacar_buf(void *arg)
{
    (void)arg;
    uint32_t n = 0;
    while (!fim_lector) {
        memoria_paginas(MEM_SACAR, buf_lector, TROZO);
        n++;
        hilo_ceder();
    }
    return (void *)n;
}

static uint32_t fnv(const uint8_t *p, uint32_t n)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

static int lector(const char *caminho, uint32_t segundos)
{
    /* El búfer en el área de datos (MEM_SACAR solo vale ahí), alineado a página. */
    uint8_t *b = aumentar_memoria(TROZO + 4096);
    if (!b) return 2;
    buf_lector = (uint8_t *)(((uint32_t)b + 4095) & ~4095u);
    static uint32_t ref[512];
    uint32_t trozos = 0;
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) { imprimir("lector: no se abre %s\n", caminho); return 2; }
    for (; trozos < 512; trozos++) {
        int n = ler(fd, buf_lector, TROZO);
        if (n <= 0) break;
        ref[trozos] = fnv(buf_lector, (uint32_t)n) ^ (uint32_t)n;
    }
    fechar(fd);
    if (!trozos) { imprimir("lector: %s esta vacio\n", caminho); return 2; }
    imprimir("lector: %s, %u trozos de 256 KB\n", caminho, trozos);
    thread_t *s = thread_create(hilo_sacar_buf, NULL, 0);
    uint32_t t0 = tempo_ms(), lecturas = 0, erros = 0;
    while (tempo_ms() - t0 < segundos * 1000u) {
        fd = abrir(caminho, ABRIR_LER);
        if (fd < 0) { erros++; break; }
        for (uint32_t i = 0; i < trozos; i++) {
            int n = ler(fd, buf_lector, TROZO);
            if (n <= 0 || (fnv(buf_lector, (uint32_t)n) ^ (uint32_t)n) != ref[i]) {
                if (erros < 4) imprimir("lector: ERROR en el trozo %u (%d bytes)\n", i, n);
                erros++;
            }
            lecturas++;
        }
        fechar(fd);
    }
    fim_lector = 1;
    int sacadas = s ? thread_join(s) : 0;
    info_memoria_t m;
    info_troca(&m);
    imprimir("lector: %u lecturas, %d veces sacado el bufer, %u errores: %s; salidas %u, entradas %u\n", lecturas,
             sacadas, erros, erros || !s ? "FALLO" : "ok", m.troca_saidas, m.troca_entradas);
    return erros || !s ? 3 : 0;
}

int principal(int argc, char **argv)
{
    if (argc > 3 && !compara_texto(argv[1], "llenar"))
        return llenar((uint32_t)texto_para_int(argv[2]), (uint32_t)texto_para_int(argv[3]),
                      argc > 4 ? (uint32_t)texto_para_int(argv[4]) : 1);
    if (argc > 4 && !compara_texto(argv[1], "varios"))
        return varios((uint32_t)texto_para_int(argv[2]), (uint32_t)texto_para_int(argv[3]),
                      (uint32_t)texto_para_int(argv[4]));
    if (argc > 3 && !compara_texto(argv[1], "lector"))
        return lector(argv[2], (uint32_t)texto_para_int(argv[3]));
    if (argc > 1 && !compara_texto(argv[1], "info")) {
        info_memoria_t m;
        info_troca(&m);
        /* Una línea corta por dato: la consola parte las largas. */
        imprimir("swap tope KB: %u\n", m.troca_teto_kb);
        imprimir("swap datos KB: %u\n", m.troca_kb);
        imprimir("swap comprimidos KB: %u\n", m.troca_comprimida_kb);
        imprimir("swap almacen KB: %u\n", m.troca_armazem_kb);
        imprimir("swap salidas: %u\n", m.troca_saidas);
        imprimir("swap entradas: %u\n", m.troca_entradas);
        return 0;
    }
    imprimir("uso: troca llenar MB VUELTAS [HILOS] | varios K MB VUELTAS | lector ARCHIVO SEGUNDOS | info\n");
    return 1;
}
