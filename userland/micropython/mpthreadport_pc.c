/* SPDX-License-Identifier: GPL-2.0-or-later */
/* mpthreadport_pc.c - el modulo _thread de MicroPython sobre pthreads, para el PC (D459, VisualPythonS).
 *
 * Es el equivalente de mpthreadport.c (hilos de usuario de SaguiOS, D170). Mismo diseno:
 *   - un GIL: un solo hilo ejecuta Python a la vez; se suelta cada MICROPY_PY_THREAD_GIL_VM_DIVISOR saltos
 *     hacia atras y en las esperas (time.sleep, Lock, red, entrada);
 *   - el recolector corre en un hilo y tiene que ver los objetos de los demas. Sin senales, con el GIL los demas
 *     estan quietos en uno de dos sitios: esperando el GIL (al entrar en mp_thread_mutex_lock se fotografian sus
 *     registros y su pila) o en una espera sin GIL (al soltarlo se fotografian igual; ese codigo no crea ni toca
 *     objetos de Python). La foto son los registros (getcontext) mas la pila desde esa marca hasta su cima. */
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <ucontext.h>
#include "py/runtime.h"
#include "py/gc.h"
#include "py/mpthread.h"
#include "py/mperrno.h"
#include "sagui_mp.h"

#if MICROPY_PY_THREAD

__thread struct _mp_state_thread_t *sagui_estado_tls;

enum { HILO_CRIADO, HILO_CORRENDO, HILO_ACABADO };

typedef struct hilo_py {
    struct hilo_py *prox;
    pthread_t id;
    volatile int estado;
    void *(*entrada)(void *);
    void *arg;                     /* argumentos del hilo (en el heap de Python) */
    ucontext_t foto[2];            /* [0] al soltar el GIL, [1] al pedirlo */
    void *sp[2];                   /* donde empieza lo vivo de la pila en cada foto (0 = sin foto) */
    struct _mp_state_thread_t *ts; /* estado del hilo (en su pila) */
} hilo_py_t;

static hilo_py_t *hilos;           /* los hilos de Python vivos o por recoger (los protege el GIL) */
static hilo_py_t principal;
static __thread hilo_py_t *yo;     /* NULL en el principal */
static volatile int encerrando;

static inline hilo_py_t *eu(void) { return yo ? yo : &principal; }

void mp_thread_set_state(struct _mp_state_thread_t *estado)
{
    sagui_estado_tls = estado;
    if (yo) yo->ts = estado;
}

mp_uint_t mp_thread_get_id(void) { return (mp_uint_t)pthread_self(); }
void mp_thread_start(void) { eu()->estado = HILO_CORRENDO; }
void mp_thread_finish(void) { eu()->estado = HILO_ACABADO; }

static void __attribute__((noinline)) fotografar(hilo_py_t *h, int k, void *sp)
{
    getcontext(&h->foto[k]);
    h->sp[k] = sp;
}

static void recolher_acabados(void)
{
    hilo_py_t **pp = &hilos;
    while (*pp) {
        hilo_py_t *h = *pp;
        if (h->estado == HILO_ACABADO) {
            pthread_join(h->id, NULL);
            *pp = h->prox;
            free(h);
        } else {
            pp = &h->prox;
        }
    }
}

typedef struct { hilo_py_t *h; } arranque_t;

static void *inicio_hilo(void *p)
{
    hilo_py_t *h = p;
    yo = h;
    h->entrada(h->arg);
    h->estado = HILO_ACABADO;
    return NULL;
}

mp_uint_t mp_thread_create(void *(*entrada)(void *), void *arg, size_t *tam_pilha)
{
    recolher_acabados();
    hilo_py_t *h = calloc(1, sizeof *h);
    if (!h) mp_raise_OSError(MP_ENOMEM);
    h->entrada = entrada;
    h->arg = arg;
    h->estado = HILO_CRIADO;
    size_t tam = *tam_pilha ? *tam_pilha : 256 * 1024;
    if (tam < 128 * 1024) tam = 128 * 1024;
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, tam);
    h->prox = hilos;
    hilos = h;
    int r = pthread_create(&h->id, &at, inicio_hilo, h);
    pthread_attr_destroy(&at);
    if (r) {
        hilos = h->prox;
        free(h);
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("no se puede crear el hilo"));
    }
    *tam_pilha = tam - 8 * 1024;
    return (mp_uint_t)h->id;
}

/* ------------------------------------------------------------ mutex */

void mp_thread_mutex_init(mp_thread_mutex_t *m)
{
    pthread_mutex_init(&m->m, NULL);
    pthread_cond_init(&m->c, NULL);
    m->proximo = m->servindo = 0;
}

static inline int e_o_gil(mp_thread_mutex_t *m) { return m == &MP_STATE_VM(gil_mutex); }

int mp_thread_mutex_lock(mp_thread_mutex_t *m, int esperar)
{
    int gil = hilos && e_o_gil(m);
    if (gil) fotografar(eu(), 1, __builtin_frame_address(0));
    pthread_mutex_lock(&m->m);
    if (!esperar) {
        int ok = m->proximo == m->servindo;
        if (ok) m->proximo++;
        pthread_mutex_unlock(&m->m);
        return ok;
    }
    unsigned long vez = m->proximo++;
    while (m->servindo != vez) pthread_cond_wait(&m->c, &m->m);
    pthread_mutex_unlock(&m->m);
    if (gil && encerrando && yo) {   /* el programa acaba: un hilo que no es el principal ya no vuelve a Python */
        mp_thread_mutex_unlock(m);
        pthread_exit(NULL);
    }
    return 1;
}

void mp_thread_mutex_unlock(mp_thread_mutex_t *m)
{
    if (hilos && e_o_gil(m)) fotografar(eu(), 0, __builtin_frame_address(0));
    pthread_mutex_lock(&m->m);
    m->servindo++;
    pthread_cond_broadcast(&m->c);
    pthread_mutex_unlock(&m->m);
}

void sagui_hilos_encerrar(void) { encerrando = 1; }

/* ------------------------------------------------------------ recolector */

static void percorrer(void *de, void *ate)
{
    uintptr_t a = ((uintptr_t)de + sizeof(void *) - 1) & ~(uintptr_t)(sizeof(void *) - 1);
    if ((uintptr_t)ate > a) gc_collect_root((void **)a, ((uintptr_t)ate - a) / sizeof(void *));
}

/* Lo llama gc_collect (mp_sagui.c) tras recorrer la pila del hilo actual. */
void mp_thread_gc_others(void)
{
    hilo_py_t *atual = eu();
    if (atual != &principal) {
        for (int k = 0; k < 2; k++)
            if (principal.sp[k]) gc_collect_root((void **)&principal.foto[k], sizeof principal.foto[k] / sizeof(void *));
        void *de = principal.sp[0];
        if (principal.sp[1] && (!de || principal.sp[1] < de)) de = principal.sp[1];
        if (de) percorrer(de, mp_state_ctx_thread_principal.stack_top);
    }
    for (hilo_py_t *h = hilos; h; h = h->prox) {
        if (h->estado == HILO_ACABADO) continue;
        gc_collect_root(&h->arg, 1);
        if (h == atual || !h->ts) continue;
        void *de = NULL;
        for (int k = 0; k < 2; k++) {
            if (!h->sp[k]) continue;
            gc_collect_root((void **)&h->foto[k], sizeof h->foto[k] / sizeof(void *));
            if (!de || h->sp[k] < de) de = h->sp[k];
        }
        if (de) percorrer(de, h->ts->stack_top);
    }
}

/* D419: en el PC no hay fork (bifurcar() da error), así que no queda nada que limpiar */
void sagui_hilos_pos_fork(void) {}

#endif
