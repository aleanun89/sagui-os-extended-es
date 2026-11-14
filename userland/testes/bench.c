// SPDX-License-Identifier: GPL-2.0-or-later
/* bench.c - medida de desempenho, com um processo ou com varios (D151).
 *
 * A carga e a mesma em todos: copiar e preencher blocos de memoria (o
 * memcpy e o memset rapidos da libsagui, que se medem no arranque). Com um
 * processo mede quanto tempo demora; com varios, cada um mede o seu e o
 * programa mede o tempo de parede de todos juntos.
 *
 * A diferenca entre "N processos em X ms" e "1 processo em Y ms" e o que se
 * ganha quando o SO usa mais de um nucleo. Hoje o SO usa so um (o SMP e o
 * que a D150 prepara), por isso o ganho ainda e 1: esta prova mede a base
 * para quando isso mudar.
 *
 * Uso: /testes/bench [N [c|s]]
 *   sem N: mede so com um processo;
 *   com N: mede com um e com N processos e diz o ganho;
 *   c: so calculo (D164); s: muitas chamadas ao sistema (D165);
 *   k: mata N processos enquanto correm noutras CPU, varias vezes (D165);
 *   h: o calculo repartido em N hilos do mesmo processo (D167).
 */
#include "sagui.h"

#define TAM  (8u * 1024 * 1024)   /* 8 MB por bloco */
#define VEZES 240                  /* vueltas: ~4 GB de copia y relleno; con KVM, 24 duraban
                                      * menos que un tic del reloj (10 ms) y la medida salía 0 (D164) */
#define MAX_FILHOS 8

/* Modo "c" (D164): solo cálculo, sin memoria. La copia mide sobre todo la
 * caché y la memoria, que los núcleos comparten: con varios procesos a la vez
 * no escala aunque el SMP funcione. El cálculo sí debe escalar. */
#define VOLTAS_CALCULO 50000000u
static int modo_calculo;

/* Modo "s" (D165): muchas llamadas al sistema baratas (pedir el PID) con un
 * poco de cálculo entre ellas. Con el modelo maestro/esclavo (D164) cada
 * llamada volvía a la CPU 0 y no escalaba; con el cerrojo general del kernel
 * cada CPU atiende las suyas. */
#define VOLTAS_CHAMADAS 8000000u
static int modo_chamadas;

static unsigned chamadas(void)
{
    unsigned t0 = tempo_ms();
    volatile unsigned x = 1;
    unsigned soma = 0;
    for (unsigned i = 0; i < VOLTAS_CHAMADAS; i++) {
        soma += (unsigned)meu_pid();
        for (int k = 0; k < 16; k++)
            x = x * 1664525u + 1013904223u;
    }
    if (soma == 0 || x == 0) registrar("bench: chamadas impossiveis\n");
    return tempo_ms() - t0;
}

static unsigned calculo(void)
{
    unsigned t0 = tempo_ms();
    volatile unsigned x = 1;
    for (unsigned i = 0; i < VOLTAS_CALCULO; i++)
        x = x * 1664525u + 1013904223u;
    if (x == 0) registrar("bench: calculo impossible\n");
    return tempo_ms() - t0;
}

/* A carga: copiar e preencher. Devolve os milissegundos que demorou. */
static unsigned carga(void)
{
    if (modo_calculo) return calculo();
    if (modo_chamadas) return chamadas();
    unsigned t0 = tempo_ms();
    char *a = aloca(TAM);
    char *b = aloca(TAM);
    if (!a || !b) {
        if (a) libera(a);
        if (b) libera(b);
        registrar("bench: sem memoria para a carga\n");
        return 0;
    }
    memset(a, 0x5A, TAM);
    for (int i = 0; i < VEZES; i++) {
        memcpy(b, a, TAM);
        memset(a, (char)(i & 0xFF), TAM);
    }
    unsigned t = tempo_ms() - t0;
    /* Uma leitura do resultado, para que o compilador nao tire a carga. */
    unsigned soma = 0;
    for (unsigned i = 0; i < TAM; i += 4096) soma += (unsigned char)b[i];
    libera(a);
    libera(b);
    if (soma == 0xFFFFFFFFu) registrar("bench: carga impossible\n");
    return t;
}

/* Modo "k" (D165): matar procesos mientras corren en otras CPU. Lanza N
 * hijos (la mitad con llamadas al sistema, la mitad con cálculo), los deja
 * correr un poco, los mata y los recoge; varias rondas. Con el cerrojo
 * general, matar un proceso que corre en otro núcleo tiene que sacarlo de
 * allí antes de liberar su memoria. */
#define RONDAS_MATAR 20
static int matar_em_outras_cpus(int n)
{
    int erros = 0;
    for (int r = 0; r < RONDAS_MATAR; r++) {
        int pids[MAX_FILHOS];
        for (int i = 0; i < n; i++) {
            char *args[5];
            args[0] = "/testes/bench";
            args[1] = "--filho";
            args[2] = "k";
            args[3] = (i & 1) ? "c" : "s";
            args[4] = 0;
            pids[i] = executar("/testes/bench", args);
        }
        dormir(20 + (unsigned)(r % 5) * 15);
        for (int i = 0; i < n; i++)
            if (pids[i] > 0 && matar(pids[i]) < 0) erros++;
        for (int i = 0; i < n; i++) {
            int codigo = 0;
            if (pids[i] > 0 && esperar(pids[i], &codigo) != pids[i]) erros++;
        }
    }
    registrar("bench: matar %d rondas de %d procesos: %s (%d errores)\n", RONDAS_MATAR, n,
              erros ? "FALLO" : "ok", erros);
    return erros ? 1 : 0;
}

/* Modo "h" (D167): el mismo cálculo, pero en N hilos de UN proceso. Cada
 * hilo hace la vuelta entera (como cada hijo en el modo c), así que con N
 * CPU el tiempo de pared debería ser el de uno solo. */
#define VEZES_HILO 8   /* con KVM una vuelta de cálculo son ~40 ms: cuatro tics, poca precisión */

static unsigned calculo_longo(void)
{
    unsigned t = 0;
    for (int i = 0; i < VEZES_HILO; i++) t += calculo();
    return t;
}

static void *hilo_calculo(void *arg)
{
    (void)arg;
    return (void *)calculo_longo();
}

static int calculo_em_hilos(int n)
{
    calculo();   /* calentamiento */
    unsigned t1 = calculo_longo();
    registrar("bench: 1 hilo: %u ms (calculo)\n", t1);
    thread_t *h[MAX_FILHOS];
    unsigned t0 = tempo_ms();
    for (int i = 0; i < n; i++) h[i] = thread_create(hilo_calculo, 0, 0);
    unsigned maximo = 0;
    int falhas = 0;
    for (int i = 0; i < n; i++) {
        if (!h[i]) { falhas++; continue; }
        int t = thread_join(h[i]);
        if (t < 0) falhas++;
        else if ((unsigned)t > maximo) maximo = (unsigned)t;
    }
    unsigned t2 = tempo_ms() - t0;
    registrar("bench: %d hilos: %u ms (el mas lento %u ms, %d fallos)\n", n, t2, maximo, falhas);
    if (t2) {
        unsigned ganho = (unsigned)((unsigned long)t1 * (unsigned long)n * 100u / (unsigned long)t2);
        registrar("bench: ganho %u.%02ux (%u ms por hilo)\n", ganho / 100, ganho % 100, t2 / (unsigned)n);
    } else {
        registrar("bench: ganho: a medicao foi demasiado curta\n");
    }
    return falhas ? 1 : 0;
}

int principal(int argc, char **argv)
{
    /* Modo filho: mede a carga e diz quanto tempo demorou. */
    if (argc > 3 && argv[3][0] == 'c') modo_calculo = 1;
    if (argc > 2 && argv[1][0] != '-' && argv[2][0] == 'c') modo_calculo = 1;   /* bench N c */
    if (argc > 3 && argv[3][0] == 's') modo_chamadas = 1;
    if (argc > 2 && argv[1][0] != '-' && argv[2][0] == 's') modo_chamadas = 1;   /* bench N s (D165) */
    /* "--filho" (antes se miraba argv[1][1] == 'f', que en "--filho" es '-':
     * los hijos hacían el trabajo del padre entero, calentamiento incluido). */
    if (argc > 2 && argv[1][0] == '-' && argv[1][1] == '-' && argv[1][2] == 'f') {
        if (argv[2][0] == 'k')   /* el modo k: corre hasta que lo maten (D165) */
            for (;;) carga();
        unsigned t = carga();
        registrar("bench: filho %s: %u ms\n", argv[2], t);
        return 0;
    }

    if (argc > 2 && argv[1][0] != '-' && argv[2][0] == 'h') {   /* bench N h (D167) */
        int n = 0;
        texto_para_numero(argv[1], &n);
        if (n < 1) n = 1;
        if (n > MAX_FILHOS) n = MAX_FILHOS;
        return calculo_em_hilos(n);
    }

    if (argc > 2 && argv[1][0] != '-' && argv[2][0] == 'k') {   /* bench N k (D165) */
        int n = 0;
        texto_para_numero(argv[1], &n);
        if (n < 1) n = 1;
        if (n > MAX_FILHOS) n = MAX_FILHOS;
        return matar_em_outras_cpus(n);
    }

    /* Um aquecimento: a primeira vez as paginas ainda nao existem e a falta
     * delas pesa mais que a carga. A media comeca depois. */
    carga();
    unsigned t1 = carga();
    registrar("bench: 1 processo: %u ms%s\n", t1,
              modo_calculo ? " (calculo)" : modo_chamadas ? " (chamadas)" : "");

    int n = 0;
    if (argc > 1) texto_para_numero(argv[1], &n);
    if (n > MAX_FILHOS) n = MAX_FILHOS;
    if (n > 1) {
        unsigned t0 = tempo_ms();
        int pids[MAX_FILHOS];
        for (int i = 0; i < n; i++) {
            char num[8];
            formatar_texto(num, sizeof num, "%d", i);
            char *args[5];
            args[0] = "/testes/bench";
            args[1] = "--filho";
            args[2] = num;
            args[3] = modo_calculo ? "c" : modo_chamadas ? "s" : 0;
            args[4] = 0;
            pids[i] = executar("/testes/bench", args);
        }
        for (int i = 0; i < n; i++) {
            int codigo = 0;
            if (pids[i] > 0) esperar(pids[i], &codigo);
        }
        unsigned t2 = tempo_ms() - t0;
        registrar("bench: %d processos: %u ms\n", n, t2);
        /* O ganho, em centesimos: 250 = 2,5 vezes mais rapido. */
        if (t2) {
            unsigned ganho = (unsigned)((unsigned long)t1 * (unsigned long)n * 100u / (unsigned long)t2);
            registrar("bench: ganho %u.%02ux (%u ms por processo)\n",
                      ganho / 100, ganho % 100, t2 / (unsigned)n);
        } else {
            registrar("bench: ganho: a medicao foi demasiado curta\n");
        }
    }
    return 0;
}
