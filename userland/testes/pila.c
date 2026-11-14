// SPDX-License-Identifier: GPL-2.0-or-later
/* pila - prueba de las pilas de usuario bajo demanda con guarda (D186, H154).
 *
 * Sin argumentos hace todas las pruebas, cada una en un proceso hijo, y
 * acaba con "RESULTADO pila: ...":
 *   1. recursión que usa 1 MB y 6 MB de pila: funciona, y el kernel dice
 *      que el proceso ocupa esa pila (INFO_PILHAS);
 *   2. recursión infinita: el hijo muere (el kernel escribe «Desbordamiento
 *      de pila») y el sistema sigue;
 *   3. un hilo que usa 512 KB de pila: funciona;
 *   4. un hilo que se desborda: muere en su guarda sin tocar el heap (un
 *      centinela en el heap sigue intacto mientras baja);
 *   5. una llamada al sistema que escribe en una parte de la pila aún sin
 *      tocar (read a un buffer local grande);
 *   6. memoria libre antes, durante y después de 10 procesos pequeños: cada
 *      uno ocupa pocos KB de pila;
 *   7. varios hilos tocando a la vez páginas nuevas de su pila y la misma
 *      página nueva de la pila del principal (con --smp, en varias CPU).
 *
 * Con "--<modo>" es uno de los hijos. */
#include "sagui.h"
#include "verifica.h"

#define KB 1024u

static uint32_t esp_agora(void)
{
    uint32_t esp;
    __asm__ volatile("movl %%esp, %0" : "=r"(esp));
    return esp;
}

/* ------------------------------------------------------------ recursión */

static uint32_t esp_minimo;

/* Cada nivel ocupa algo más de 1 KB. "volatile" para que el compilador no
 * quite el buffer ni convierta la recursión en un bucle. */
static __attribute__((noinline)) uint32_t descer(uint32_t n)
{
    volatile uint8_t buf[1000];
    buf[0] = (uint8_t)n;
    buf[999] = (uint8_t)(n >> 8);
    uint32_t e = esp_agora();
    if (e < esp_minimo) esp_minimo = e;
    if (n == 0) return buf[0] + buf[999];
    return descer(n - 1) + buf[0] + buf[999];
}

static unsigned pilha_propia_kb(void)
{
    info_pilha_t v[64];
    int n = info_sistema(INFO_PILHAS, v, 64);
    for (int i = 0; i < n; i++)
        if (v[i].pid == meu_pid()) return v[i].pilha_kb;
    return 0;
}

/* Baja hasta usar "kb" KB de pila (medido por ESP). */
static int modo_recursao(unsigned kb)
{
    uint32_t topo = esp_agora();
    esp_minimo = topo;
    uint32_t n = kb;   /* un nivel ~ 1 KB: se baja de más y se mide */
    uint32_t r = descer(n);
    unsigned usado = (topo - esp_minimo) / KB;
    imprimir("PILA recursao %u KB: usado %u KB, pila del proceso %u KB (r=%u)\n", kb, usado, pilha_propia_kb(), r);
    return usado >= kb ? 0 : 1;
}

static volatile uint32_t nunca = 0xFFFFFFFFu;   /* el compilador no sabe que la recursión no acaba */

static __attribute__((noinline)) uint32_t sem_fim(uint32_t n)
{
    volatile uint8_t buf[512];
    buf[0] = (uint8_t)n;
    if (n == nunca) return 0;
    return sem_fim(n + 1) + buf[0];
}

/* ------------------------------------------------------------ hilos */

#define CENTINELA_N   (64u * KB)
static uint8_t *centinela;
static volatile uint32_t hilo_kb_maximo;

static bool centinela_ok(void)
{
    for (uint32_t i = 0; i < CENTINELA_N; i++)
        if (centinela[i] != (uint8_t)(0xA5 ^ i)) return false;
    return true;
}

static __attribute__((noinline)) uint32_t descer_hilo(uint32_t topo, uint32_t n, bool vigiar)
{
    volatile uint8_t buf[1000];
    buf[0] = (uint8_t)n;
    buf[999] = (uint8_t)n;
    uint32_t usado = (topo - esp_agora()) / KB;
    if (usado > hilo_kb_maximo) hilo_kb_maximo = usado;
    if (vigiar && (n % 128) == 0) {
        if (!centinela_ok()) {
            imprimir("PILA CENTINELA ROTO a %u KB\n", usado);
            sair(3);
        }
        imprimir("PILA hilo-desborda: %u KB, centinela intacto\n", usado);
    }
    if (n == 0) return buf[0];
    return descer_hilo(topo, n - 1, vigiar) + buf[0] + buf[999];
}

static void *hilo_usa(void *arg)
{
    uint32_t kb = (uint32_t)arg;
    hilo_kb_maximo = 0;
    uint32_t r = descer_hilo(esp_agora(), kb, false);
    return (void *)(r + 1);
}

static int modo_hilo(unsigned kb)
{
    thread_t *t = thread_create(hilo_usa, (void *)kb, 0);
    if (!t) { imprimir("PILA hilo: no se pudo crear\n"); return 2; }
    int r = thread_join(t);
    imprimir("PILA hilo %u KB: usado %u KB (r=%d)\n", kb, hilo_kb_maximo, r);
    return hilo_kb_maximo >= kb && r > 0 ? 0 : 1;
}

static void *hilo_desborda(void *arg)
{
    (void)arg;
    descer_hilo(esp_agora(), 0xFFFFFFFFu, true);   /* nunca llega al fondo */
    return 0;
}

static int modo_hilo_desborda(void)
{
    centinela = aloca(CENTINELA_N);
    if (!centinela) return 2;
    for (uint32_t i = 0; i < CENTINELA_N; i++) centinela[i] = (uint8_t)(0xA5 ^ i);
    thread_t *t = thread_create(hilo_desborda, 0, 0);
    if (!t) return 2;
    thread_join(t);   /* no vuelve: el kernel termina el proceso */
    imprimir("PILA hilo-desborda: el hilo volvio\n");
    return 1;
}

/* ------------------------------------------------------------ llamada al sistema */

static __attribute__((noinline)) int ler_no_fundo(void)
{
    uint8_t buf[300 * KB];   /* sin tocar: el kernel la hace crecer al copiar */
    int fd = abrir("/testes/pila", ABRIR_LER);
    if (fd < 0) return -1;
    int n = ler(fd, buf, 64 * KB);
    fechar(fd);
    imprimir("PILA ler: %d bytes en un buffer de la pila sin tocar (%02x %c%c%c)\n", n, buf[0], buf[1], buf[2], buf[3]);
    return n > 0 && buf[0] == 0x7F && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F' ? 0 : 1;
}

/* ------------------------------------------------------------ varios hilos a la vez */

#define N_SIMUL   4
#define PAGINAS   48
static volatile uint32_t chegaram, rodada;
static volatile uint8_t *pagina_comum;   /* en la pila del principal, sin tocar */
static volatile uint32_t erros_simul;

static void barreira(uint32_t vez)
{
    __sync_fetch_and_add(&chegaram, 1);
    while (chegaram < vez * N_SIMUL) ;
}

static void *hilo_simul(void *arg)
{
    uint32_t id = (uint32_t)arg;
    volatile uint8_t propia[PAGINAS * 4096];   /* su pila: páginas nuevas */
    for (uint32_t p = 0; p < PAGINAS; p++) {
        barreira(p + 1);
        /* Los N_SIMUL a la vez: una página nueva de la pila del principal
         * (la misma para todos) y una de la suya. */
        pagina_comum[p * 4096 + id * 8] = (uint8_t)(p + id + 1);
        propia[(PAGINAS - 1 - p) * 4096] = (uint8_t)(p ^ id);
    }
    for (uint32_t p = 0; p < PAGINAS; p++)
        if (propia[(PAGINAS - 1 - p) * 4096] != (uint8_t)(p ^ id)) __sync_fetch_and_add(&erros_simul, 1);
    return (void *)1;
}

static __attribute__((noinline)) int modo_simultaneo(void)
{
    uint8_t comum[PAGINAS * 4096 + 4096];   /* sin tocar hasta que llegan los hilos */
    pagina_comum = (volatile uint8_t *)(((uint32_t)comum + 4095) & ~4095u);
    chegaram = 0;
    erros_simul = 0;
    thread_t *t[N_SIMUL];
    for (uint32_t i = 0; i < N_SIMUL; i++) t[i] = thread_create(hilo_simul, (void *)i, 0);
    int ok = 0;
    for (uint32_t i = 0; i < N_SIMUL; i++) ok += t[i] && thread_join(t[i]) == 1;
    for (uint32_t p = 0; p < PAGINAS; p++)
        for (uint32_t id = 0; id < N_SIMUL; id++)
            if (pagina_comum[p * 4096 + id * 8] != (uint8_t)(p + id + 1)) erros_simul++;
    imprimir("PILA simultaneo: %d hilos, %u paginas, %u errores\n", ok, PAGINAS, erros_simul);
    return ok == N_SIMUL && erros_simul == 0 ? 0 : 1;
}

/* ------------------------------------------------------------ el principal */

static int filho(const char *modo, int *codigo)
{
    char *args[3] = { "/testes/pila", (char *)modo, 0 };
    int pid = executar("/testes/pila", args);
    if (pid < 0) return pid;
    return esperar(pid, codigo) == pid ? 0 : -1;
}

static unsigned livre_kb(void)
{
    info_memoria_t m;
    if (info_sistema(INFO_MEMORIA, &m, sizeof m) < 0) return 0;
    return m.livre_kb;
}

static void prova_memoria(void)
{
    unsigned livre0 = livre_kb();
    int pids[10];
    char *args[3] = { "/testes/pila", "--dormir", 0 };
    for (int i = 0; i < 10; i++) pids[i] = executar("/testes/pila", args);
    dormir(500);
    unsigned livre1 = livre_kb();
    info_pilha_t v[64];
    int n = info_sistema(INFO_PILHAS, v, 64);
    unsigned soma = 0, maior = 0, vistos = 0;
    for (int i = 0; i < n; i++)
        for (int k = 0; k < 10; k++)
            if (v[i].pid == pids[k]) {
                soma += v[i].pilha_kb;
                if (v[i].pilha_kb > maior) maior = v[i].pilha_kb;
                vistos++;
            }
    for (int i = 0; i < 10; i++) { int c; esperar(pids[i], &c); }
    dormir(100);
    unsigned livre2 = livre_kb();
    unsigned por = livre0 > livre1 ? (livre0 - livre1) / 10 : 0;
    imprimir("PILA memoria: livre %u -> %u -> %u KB; %u KB por proceso, pila %u KB de media (max %u) en %u\n",
             livre0, livre1, livre2, por, vistos ? soma / vistos : 0, maior, vistos);
    verifica(vistos == 10, "el kernel informa de la pila de los 10 procesos (INFO_PILHAS)");
    verifica(maior <= 16, "un proceso pequeño ocupa como mucho 16 KB de pila");
    verifica(livre2 + 64 >= livre0, "los 10 procesos devuelven su memoria (tolerancia 64 KB)");
}

int principal(int argc, char **argv)
{
    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '-') {
        const char *m = argv[1] + 2;
        if (compara_texto(m, "rec1") == 0) return modo_recursao(1024);
        if (compara_texto(m, "rec6") == 0) return modo_recursao(6 * 1024);
        if (compara_texto(m, "infinita") == 0) { sem_fim(0); return 1; }
        if (compara_texto(m, "hilo512") == 0) return modo_hilo(512);
        if (compara_texto(m, "hilo-desborda") == 0) return modo_hilo_desborda();
        if (compara_texto(m, "ler") == 0) return ler_no_fundo();
        if (compara_texto(m, "simultaneo") == 0) return modo_simultaneo();
        if (compara_texto(m, "dormir") == 0) { dormir(2000); return 0; }
        return 2;
    }
    imprimir("PILA pid %d, esp %x, pila %u KB\n", meu_pid(), esp_agora(), pilha_propia_kb());
    int c = -1;
    verifica(filho("--rec1", &c) == 0 && c == 0, "recursion de 1 MB de pila");
    verifica(filho("--rec6", &c) == 0 && c == 0, "recursion de 6 MB de pila");
    verifica(filho("--infinita", &c) == 0 && c == SAIDA_ENCERRADO_PELO_KERNEL,
             "la recursion infinita termina el proceso (desbordamiento de pila)");
    verifica(filho("--hilo512", &c) == 0 && c == 0, "un hilo usa 512 KB de pila");
    verifica(filho("--hilo-desborda", &c) == 0 && c == SAIDA_ENCERRADO_PELO_KERNEL,
             "un hilo que se desborda termina el proceso sin romper el heap");
    verifica(filho("--ler", &c) == 0 && c == 0, "el kernel escribe en una parte de la pila sin tocar");
    prova_memoria();
    verifica(filho("--simultaneo", &c) == 0 && c == 0, "varios hilos hacen crecer pilas a la vez");
    return resultado("pila");
}
