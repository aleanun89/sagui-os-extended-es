// SPDX-License-Identifier: GPL-2.0-or-later
/* hilos.c - hilos de usuario (D167): la API de docs/multihilo.md.
 *
 *   thread_t *t = thread_create(funcion, arg, 0);   (0 = la pila del kernel)
 *   int r = thread_join(t);                         (lo que devolvió la función)
 *   thread_yield();  thread_sair();
 *
 * El kernel solo sabe de "hilos" (SIS_HILO_*): un punto de entrada y un tope
 * de pila. Lo demás está aquí: el hilo empieza en hilo_inicio(), que llama a
 * la función y sale con lo que devolvió, y thread_join() espera y recoge ese
 * valor.
 *
 * La pila (D186): si cabe en PILHA_KERNEL_UTIL (lo normal, y siempre con
 * pilha = 0), la pone el kernel: un hueco de 1 MB con 64 KB de guarda abajo,
 * cuyas páginas se asignan al tocarlas; un desbordamiento termina el proceso
 * con «Desbordamiento de pila» en vez de pisar el heap. Si se pide más, sale
 * del malloc de la libsagui como antes de la D186 (sin guarda), y
 * thread_join() la libera.
 *
 * Sin TLS: la libsagui no tiene errno ni otro estado por hilo. Lo único que
 * un hilo necesita saber de sí mismo es su número (thread_id(), una llamada
 * al sistema). Ver la D167. */
#include "sagui.h"

#define PILHA_KERNEL_UTIL (960u * 1024u)   /* HILO_VAGA - HILO_GUARDA (kernel/inc/paginacao.h) */
#define PILHA_MINIMA (8u * 1024u)

struct thread {
    int    tid;
    void *(*funcao)(void *);
    void  *arg;
    void  *pilha;
};

static inline int chamar(unsigned n, unsigned a, unsigned b, unsigned c)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return r;
}

/* ------------------------------------------------------------ llamadas */

int  hilo_criar(void (*entrada)(void), void *pilha_topo)
{
    return chamar(SIS_HILO_CRIAR, (unsigned)entrada, (unsigned)pilha_topo, 0);
}
int  hilo_criar_kernel(void (*entrada)(void), void *arg)
{
    return chamar(SIS_HILO_CRIAR, (unsigned)entrada, 0, (unsigned)arg);
}
void hilo_sair(int codigo)             { chamar(SIS_HILO_SAIR, (unsigned)codigo, 0, 0); for (;;) ; }
int  hilo_esperar(int tid, int *codigo) { return chamar(SIS_HILO_ESPERAR, (unsigned)tid, (unsigned)codigo, 0); }
void hilo_ceder(void)                  { chamar(SIS_CEDER, 0, 0, 0); }
int  thread_id(void)                   { return chamar(SIS_HILO_ID, 0, 0, 0); }

int futex_esperar(volatile uint32_t *palavra, uint32_t valor, unsigned ms)
{
    return chamar(SIS_FUTEX_ESPERAR, (unsigned)palavra, valor, ms);
}

int futex_acordar(volatile uint32_t *palavra, int quantos)
{
    return chamar(SIS_FUTEX_ACORDAR, (unsigned)palavra, (unsigned)quantos, 0);
}

/* ------------------------------------------------------------ API */

/* Donde empieza cada hilo: la pila trae, como si lo hubieran llamado, una
 * dirección de retorno (0: nunca vuelve) y el thread_t. */
static void hilo_inicio(thread_t *t)
{
    void *r = t->funcao(t->arg);
    hilo_sair((int)(uint32_t)r);
}

thread_t *thread_create(void *(*funcao)(void *), void *arg, unsigned pilha)
{
    if (!funcao) return NULL;
    thread_t *t = aloca(sizeof *t);
    if (!t) return NULL;
    t->funcao = funcao;
    t->arg = arg;
    t->pilha = NULL;
    if (pilha <= PILHA_KERNEL_UTIL) {
        t->tid = hilo_criar_kernel((void (*)(void))hilo_inicio, t);
        if (t->tid < 0) { libera(t); return NULL; }
        return t;
    }
    if (pilha < PILHA_MINIMA) pilha = PILHA_MINIMA;
    t->pilha = aloca(pilha);
    if (!t->pilha) { libera(t); return NULL; }
    /* El ABI i386 quiere (esp + 4) alineado a 16 al entrar en una función:
     * el argumento en una dirección múltiplo de 16 y el retorno justo debajo. */
    uint32_t base = ((uint32_t)t->pilha + pilha - 16u) & ~15u;
    uint32_t *topo = (uint32_t *)base - 1;
    topo[0] = 0;               /* retorno: hilo_inicio no vuelve */
    topo[1] = (uint32_t)t;     /* su argumento */
    t->tid = hilo_criar((void (*)(void))hilo_inicio, topo);
    if (t->tid < 0) {
        libera(t->pilha);
        libera(t);
        return NULL;
    }
    return t;
}

int thread_join(thread_t *t)
{
    if (!t) return ERRO_INVALIDO;
    int codigo = 0;
    int r = hilo_esperar(t->tid, &codigo);
    if (r < 0) return r;
    libera(t->pilha);
    libera(t);
    return codigo;
}

void thread_yield(void) { hilo_ceder(); }

/* En el hilo principal acaba el proceso (con todos sus hilos), como salir(0). */
void thread_sair(void) { hilo_sair(0); }

int thread_tid(const thread_t *t) { return t ? t->tid : ERRO_INVALIDO; }
