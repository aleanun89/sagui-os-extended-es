// SPDX-License-Identifier: GPL-2.0-or-later
/* hilos - prueba de los hilos de usuario (D167, H140).
 *
 * Sin argumentos hace todas las pruebas y acaba con "RESULTADO hilos: ...":
 *   1. crear y esperar N hilos, cada uno devuelve su valor;
 *   2. un contador compartido protegido con el mutex (resultado exacto);
 *   3. productor/consumidor con dos semáforos y un mutex;
 *   4. un hilo pide memoria (malloc, y el área de datos crece) mientras
 *      otros, en otras CPU, leen y comprueban lo que ya publicó;
 *   5. muchos hilos que ceden el turno;
 *   6. procesos que acaban con hilos vivos (salir del principal, salir de
 *      otro hilo, un fallo en un hilo, matar desde fuera), varias rondas,
 *      sin fugas de memoria ni de entradas de la tabla de procesos.
 *
 * Con "--filho <modo>" es uno de los procesos de la prueba 6. */
#include "sagui.h"
#include "verifica.h"

unsigned memoria_total(void);   /* libsagui/memoria.c: bytes pedidos al kernel (D118) */

#define N_HILOS 8

/* ------------------------------------------------------------ 1 */

static volatile uint32_t vistos[N_HILOS];

static void *quadrado(void *arg)
{
    uint32_t i = (uint32_t)arg;
    vistos[i] = 1;
    return (void *)(i * i + 1);
}

static void prova_criar_esperar(void)
{
    thread_t *t[N_HILOS];
    int criados = 0, certos = 0;
    for (int i = 0; i < N_HILOS; i++) {
        t[i] = thread_create(quadrado, (void *)(uint32_t)i, 0);
        if (t[i]) criados++;
    }
    for (int i = 0; i < N_HILOS; i++)
        if (t[i] && thread_join(t[i]) == i * i + 1 && vistos[i]) certos++;
    imprimir("HILOS criados: %d, certos: %d\n", criados, certos);
    verifica(criados == N_HILOS, "se crean ocho hilos");
    verifica(certos == N_HILOS, "cada hilo devuelve su valor a thread_join");
}

/* ------------------------------------------------------------ 2 */

#define VOLTAS_MUTEX 100000
static mutex_t m_contador;
static volatile uint32_t contador;
static volatile uint32_t dentro, invasoes;

static void *somar(void *arg)
{
    (void)arg;
    for (int i = 0; i < VOLTAS_MUTEX; i++) {
        mutex_tomar(&m_contador);
        if (atomico_troca32(&dentro, 1) != 0) invasoes++;
        contador = contador + 1;
        atomico_troca32(&dentro, 0);
        mutex_largar(&m_contador);
    }
    return 0;
}

static void prova_mutex(void)
{
    mutex_criar(&m_contador);
    thread_t *t[4];
    for (int i = 0; i < 4; i++) t[i] = thread_create(somar, 0, 0);
    for (int i = 0; i < 4; i++) if (t[i]) thread_join(t[i]);
    imprimir("HILOS contador: %u (esperado %u), invasoes: %u\n", contador, 4u * VOLTAS_MUTEX, invasoes);
    verifica(contador == 4u * VOLTAS_MUTEX, "el contador protegido por el mutex es exacto con cuatro hilos");
    verifica(invasoes == 0, "el mutex no deja entrar a dos hilos a la vez");
}

/* ------------------------------------------------------------ 3 */

#define BUF 8
#define ITENS 5000
static uint32_t buffer[BUF];
static uint32_t pos_por, pos_tirar;
static semaforo_t vazios, cheios;
static mutex_t m_buf;
static volatile uint32_t soma_consumida;

static void *produtor(void *arg)
{
    (void)arg;
    for (uint32_t v = 1; v <= ITENS; v++) {
        semaforo_tomar(&vazios);
        mutex_tomar(&m_buf);
        buffer[pos_por] = v;
        pos_por = (pos_por + 1) % BUF;
        mutex_largar(&m_buf);
        semaforo_sinalizar(&cheios);
    }
    return 0;
}

static void *consumidor(void *arg)
{
    (void)arg;
    uint32_t soma = 0;
    for (int k = 0; k < ITENS; k++) {
        semaforo_tomar(&cheios);
        mutex_tomar(&m_buf);
        soma += buffer[pos_tirar];
        pos_tirar = (pos_tirar + 1) % BUF;
        mutex_largar(&m_buf);
        semaforo_sinalizar(&vazios);
    }
    atomico_add32(&soma_consumida, soma);
    return 0;
}

static void prova_semaforo(void)
{
    semaforo_criar(&vazios, BUF);
    semaforo_criar(&cheios, 0);
    mutex_criar(&m_buf);
    thread_t *t[4];
    t[0] = thread_create(consumidor, 0, 0);
    t[1] = thread_create(consumidor, 0, 0);
    t[2] = thread_create(produtor, 0, 0);
    t[3] = thread_create(produtor, 0, 0);
    for (int i = 0; i < 4; i++) if (t[i]) thread_join(t[i]);
    uint32_t esperado = 2u * (ITENS * (ITENS + 1u) / 2u);
    imprimir("HILOS semaforo: soma %u (esperado %u), cheios %u, vazios %u\n", soma_consumida, esperado,
             semaforo_valor(&cheios), semaforo_valor(&vazios));
    verifica(soma_consumida == esperado, "productores y consumidores con semaforos: la suma es exacta");
    verifica(semaforo_valor(&cheios) == 0 && semaforo_valor(&vazios) == BUF, "los semaforos acaban como empezaron");
}

/* ------------------------------------------------------------ 4 */

#define BLOCOS 100
static volatile uint32_t *publicados[BLOCOS];
static volatile uint32_t tamanhos[BLOCOS];
static volatile uint32_t publicados_n, escritor_fim, erros_leitura, leituras;

static void *escritor(void *arg)
{
    (void)arg;
    for (uint32_t i = 0; i < BLOCOS; i++) {
        uint32_t palavras = (16u * 1024u + (i % 16u) * 4096u) / 4u;
        uint32_t *p = aloca(palavras * 4u);
        if (!p) break;
        for (uint32_t k = 0; k < palavras; k++) p[k] = i * 0x01010101u ^ k;
        tamanhos[i] = palavras;
        atomico_troca32((volatile uint32_t *)&publicados[i], (uint32_t)p);
        atomico_troca32(&publicados_n, i + 1);
    }
    atomico_troca32(&escritor_fim, 1);
    return 0;
}

static void *leitor(void *arg)
{
    (void)arg;
    uint32_t ronda = 0;
    for (;;) {
        bool fim = escritor_fim != 0;
        uint32_t n = publicados_n;
        for (uint32_t i = 0; i < n; i++) {
            volatile uint32_t *p = publicados[i];
            uint32_t palavras = tamanhos[i];
            for (uint32_t k = 0; k < palavras; k += 61)
                if (p[k] != (i * 0x01010101u ^ k)) { atomico_inc32(&erros_leitura); break; }
            atomico_inc32(&leituras);
        }
        /* Y también malloc/free pequeños a la vez que el escritor crece. */
        void *x = aloca(24 + (ronda++ % 200));
        if (x) libera(x);
        if (fim) break;
    }
    return 0;
}

static void prova_memoria(void)
{
    unsigned antes = memoria_total();
    thread_t *t[4];
    t[0] = thread_create(escritor, 0, 0);
    for (int i = 1; i < 4; i++) t[i] = thread_create(leitor, 0, 0);
    for (int i = 0; i < 4; i++) if (t[i]) thread_join(t[i]);
    imprimir("HILOS memoria: %u blocos, %u leituras, %u erros, area %u -> %u KB\n", publicados_n, leituras,
             erros_leitura, antes / 1024, memoria_total() / 1024);
    verifica(publicados_n == BLOCOS, "un hilo pide memoria cien veces mientras otros leen");
    verifica(erros_leitura == 0, "los otros hilos leen bien la memoria nueva");
    verifica(memoria_total() > antes, "el area de datos del proceso crecio desde un hilo");
    for (int i = 0; i < BLOCOS; i++) if (publicados[i]) libera((void *)publicados[i]);
}

/* ------------------------------------------------------------ 5 */

static volatile uint32_t cedidos;

static void *ceder_muito(void *arg)
{
    (void)arg;
    for (int i = 0; i < 50; i++) { thread_yield(); atomico_inc32(&cedidos); }
    return (void *)(uint32_t)thread_id();
}

static void prova_muitos(void)
{
    enum { N = 24 };
    thread_t *t[N];
    int criados = 0, tids_ok = 0;
    for (int i = 0; i < N; i++) {
        t[i] = thread_create(ceder_muito, 0, 16 * 1024);
        if (t[i]) criados++;
    }
    for (int i = 0; i < N; i++) {
        if (!t[i]) continue;
        int tid = thread_tid(t[i]);
        if (thread_join(t[i]) == tid && tid != meu_pid()) tids_ok++;
    }
    imprimir("HILOS muitos: %d criados, %u cedidos, tids %d\n", criados, cedidos, tids_ok);
    verifica(criados == N && cedidos == (uint32_t)N * 50u, "veinticuatro hilos que ceden el turno acaban todos");
    verifica(tids_ok == criados, "cada hilo tiene su tid, distinto del pid del proceso");
}

/* ------------------------------------------------------------ 6 */

static semaforo_t nunca;   /* nadie lo sinaliza: quien lo toma duerme para siempre */

static void *girar_sempre(void *arg)
{
    (void)arg;
    volatile uint32_t x = 1;
    for (;;) x = x * 1664525u + 1013904223u;
    return 0;
}

static void *dormir_sempre(void *arg) { (void)arg; for (;;) dormir(5); return 0; }
static void *bloquear_sempre(void *arg) { (void)arg; semaforo_tomar(&nunca); return 0; }

static void *sair_daqui(void *arg)
{
    (void)arg;
    dormir(30);
    sair(9);   /* un hilo que no es el principal termina el proceso */
    return 0;
}

static void *falhar_daqui(void *arg)
{
    (void)arg;
    dormir(30);
    volatile uint32_t *nulo = (volatile uint32_t *)(uint32_t)dormir(1);   /* 0, sin que el compilador lo sepa */
    nulo[1] = 1;   /* fallo de página: el kernel termina el proceso */
    return 0;
}

static void lancar_vivos(void)
{
    semaforo_criar(&nunca, 0);
    thread_create(girar_sempre, 0, 0);
    thread_create(girar_sempre, 0, 0);
    thread_create(dormir_sempre, 0, 0);
    thread_create(bloquear_sempre, 0, 0);
}

static int filho(const char *modo)
{
    lancar_vivos();
    if (modo[0] == 's' && modo[1] == 'a' && modo[4] == 0) {   /* "sair": el principal sale */
        dormir(30);
        return 7;
    }
    if (modo[0] == 's') {                                      /* "sair-hilo" */
        thread_t *t = thread_create(sair_daqui, 0, 0);
        thread_t *b = thread_create(bloquear_sempre, 0, 0);
        thread_join(b);   /* nunca vuelve: otro hilo termina el proceso */
        (void)t;
        return 1;
    }
    if (modo[0] == 'f') {                                      /* "falha" */
        thread_create(falhar_daqui, 0, 0);
        for (;;) dormir(100);
    }
    for (;;) dormir(100);                                      /* "matar": lo matan desde fuera */
}

static int processos_vivos(void)
{
    info_processo_t v[64];
    int n = info_sistema(INFO_PROCESSOS, v, 64);
    return n;
}

static unsigned livre_kb(void)
{
    info_memoria_t m;
    if (info_sistema(INFO_MEMORIA, &m, sizeof m) < 0) return 0;
    return m.livre_kb;
}

static int rodar_filho(const char *modo, bool matar_logo, int *codigo)
{
    char *args[4];
    args[0] = "/testes/hilos";
    args[1] = "--filho";
    args[2] = (char *)modo;
    args[3] = 0;
    int pid = executar("/testes/hilos", args);
    if (pid < 0) return pid;
    if (matar_logo) {
        /* Cuántos hilos ve el kernel en el proceso (INFO_PROCESSOS, D167):
         * se espera a que estén los cinco (o un segundo como mucho). */
        unsigned hilos = 0;
        for (int espera = 0; espera < 100 && hilos < 5; espera++) {
            dormir(10);
            info_processo_t v[64];
            int n = info_sistema(INFO_PROCESSOS, v, 64);
            for (int i = 0; i < n; i++)
                if (v[i].pid == pid) hilos = v[i].n_hilos;
        }
        imprimir("HILOS filho %d: %u hilos\n", pid, hilos);
        dormir(20);
        matar(pid);
    }
    return esperar(pid, codigo) == pid ? 0 : -1;
}

static void prova_saidas(void)
{
    int procs0 = processos_vivos();
    unsigned livre0 = livre_kb();
    int c_sair = 0, c_hilo = 0, c_falha = 0, c_matar = 0, erros = 0;
    for (int r = 0; r < 5; r++) {
        int c;
        if (rodar_filho("sair", false, &c) == 0 && c == 7) c_sair++; else erros++;
        if (rodar_filho("sair-hilo", false, &c) == 0 && c == 9) c_hilo++; else erros++;
        if (rodar_filho("falha", false, &c) == 0 && c == SAIDA_ENCERRADO_PELO_KERNEL) c_falha++; else erros++;
        if (rodar_filho("matar", true, &c) == 0 && c == SAIDA_ENCERRADO_PELO_KERNEL) c_matar++; else erros++;
    }
    dormir(50);   /* que el kernel recoja lo que quede */
    int procs1 = processos_vivos();
    unsigned livre1 = livre_kb();
    imprimir("HILOS saidas: sair %d, sair-hilo %d, falha %d, matar %d, erros %d\n", c_sair, c_hilo, c_falha,
             c_matar, erros);
    imprimir("HILOS fugas: processos %d -> %d, livre %u -> %u KB\n", procs0, procs1, livre0, livre1);
    verifica(c_sair == 5, "salir del hilo principal con hilos vivos termina el proceso con su codigo");
    verifica(c_hilo == 5, "salir desde otro hilo termina el proceso entero");
    verifica(c_falha == 5, "un fallo en un hilo termina el proceso entero");
    verifica(c_matar == 5, "matar un proceso con hilos corriendo en otras CPU");
    verifica(procs1 == procs0, "no quedan hilos ni procesos en la tabla");
    verifica(livre1 + 64 >= livre0, "no se pierde memoria (tolerancia 64 KB)");
}

int principal(int argc, char **argv)
{
    if (argc > 2 && argv[1][0] == '-' && argv[1][1] == '-')
        return filho(argv[2]);
    imprimir("HILOS pid %d, tid %d\n", meu_pid(), thread_id());
    prova_criar_esperar();
    prova_mutex();
    prova_semaforo();
    prova_memoria();
    prova_muitos();
    prova_saidas();
    return resultado("hilos");
}
