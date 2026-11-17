// SPDX-License-Identifier: GPL-3.0-only
/* pthread_sagui.c - POSIX threads mínimos sobre los hilos de SaguiOS (D167, D376), lo justo para
 * tinycthread y miniJVM. Mutex y variables de condición con futex; el almacén por hilo (tss) es
 * una tabla (clave, tid) -> valor, porque la libsagui no tiene TLS. */
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <sagui.h>

#define MAX_HILOS 512
static struct { int tid; thread_t *t; } g_hilos[MAX_HILOS];
static mutex_t g_cerrojo = MUTEX_INICIAL;

struct arranque { void *(*f)(void *); void *arg; };

static void *entrada(void *p)
{
    struct arranque a = *(struct arranque *)p;
    free(p);
    return a.f(a.arg);
}

int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg)
{
    (void)a;
    struct arranque *ar = malloc(sizeof *ar);
    if (!ar) return EAGAIN;
    ar->f = f; ar->arg = arg;
    thread_t *h = thread_create(entrada, ar, 0);
    if (!h) { free(ar); return EAGAIN; }
    int tid = thread_tid(h);
    mutex_tomar(&g_cerrojo);
    int i;
    for (i = 0; i < MAX_HILOS && g_hilos[i].tid; i++) ;
    if (i < MAX_HILOS) { g_hilos[i].tid = tid; g_hilos[i].t = h; }
    mutex_largar(&g_cerrojo);
    *t = (pthread_t)tid;
    return 0;
}

int pthread_join(pthread_t t, void **res)
{
    thread_t *h = NULL;
    mutex_tomar(&g_cerrojo);
    for (int i = 0; i < MAX_HILOS; i++)
        if (g_hilos[i].tid == (int)t) { h = g_hilos[i].t; g_hilos[i].tid = 0; break; }
    mutex_largar(&g_cerrojo);
    if (!h) return ESRCH;
    int r = thread_join(h);
    if (res) *res = (void *)(intptr_t)r;
    return 0;
}

int pthread_detach(pthread_t t)
{
    /* Sin join no se libera el thread_t: unos 20 bytes por hilo, aceptable. */
    mutex_tomar(&g_cerrojo);
    for (int i = 0; i < MAX_HILOS; i++)
        if (g_hilos[i].tid == (int)t) { g_hilos[i].tid = 0; break; }
    mutex_largar(&g_cerrojo);
    return 0;
}

void pthread_exit(void *res) { hilo_sair((int)(intptr_t)res); }
pthread_t pthread_self(void) { return (pthread_t)thread_id(); }
int pthread_equal(pthread_t a, pthread_t b) { return a == b; }
int sched_yield(void) { thread_yield(); return 0; }

/* ---------------------------------------------------------------- mutex */
int pthread_mutexattr_init(pthread_mutexattr_t *a) { a->tipo = 0; return 0; }
int pthread_mutexattr_destroy(pthread_mutexattr_t *a) { (void)a; return 0; }
int pthread_mutexattr_settype(pthread_mutexattr_t *a, int tipo) { a->tipo = tipo; return 0; }

int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a)
{
    mutex_criar((mutex_t *)m->m);
    m->tipo = a ? a->tipo : 0;
    m->dono = 0; m->cuenta = 0;
    return 0;
}
int pthread_mutex_destroy(pthread_mutex_t *m) { (void)m; return 0; }

int pthread_mutex_lock(pthread_mutex_t *m)
{
    if (m->tipo == PTHREAD_MUTEX_RECURSIVE) {
        uint32_t yo = (uint32_t)thread_id();
        if (m->cuenta > 0 && m->dono == yo) { m->cuenta++; return 0; }
        mutex_tomar((mutex_t *)m->m);
        m->dono = yo; m->cuenta = 1;
        return 0;
    }
    mutex_tomar((mutex_t *)m->m);
    return 0;
}
int pthread_mutex_trylock(pthread_mutex_t *m)
{
    if (m->tipo == PTHREAD_MUTEX_RECURSIVE) {
        uint32_t yo = (uint32_t)thread_id();
        if (m->cuenta > 0 && m->dono == yo) { m->cuenta++; return 0; }
        if (!mutex_tentar((mutex_t *)m->m)) return EBUSY;
        m->dono = yo; m->cuenta = 1;
        return 0;
    }
    return mutex_tentar((mutex_t *)m->m) ? 0 : EBUSY;
}
int pthread_mutex_timedlock(pthread_mutex_t *m, const struct timespec *ts)
{
    (void)ts;   /* espera sin plazo: miniJVM no lo usa en el camino normal */
    return pthread_mutex_lock(m);
}
int pthread_mutex_unlock(pthread_mutex_t *m)
{
    if (m->tipo == PTHREAD_MUTEX_RECURSIVE) {
        if (--m->cuenta > 0) return 0;
        m->dono = 0;
    }
    mutex_largar((mutex_t *)m->m);
    return 0;
}

/* ---------------------------------------------------------------- condición */
int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a) { (void)a; c->seq = 0; return 0; }
int pthread_cond_destroy(pthread_cond_t *c) { (void)c; return 0; }
int pthread_cond_signal(pthread_cond_t *c) { atomico_inc32(&c->seq); futex_acordar(&c->seq, 1); return 0; }
int pthread_cond_broadcast(pthread_cond_t *c) { atomico_inc32(&c->seq); futex_acordar(&c->seq, 0x7fffffff); return 0; }

static int espera(pthread_cond_t *c, pthread_mutex_t *m, unsigned ms)
{
    uint32_t s = c->seq;
    int cuenta = m->cuenta;
    pthread_mutex_unlock(m);   /* recursivo: se suelta una vez, como POSIX exige un solo nivel */
    int r = futex_esperar(&c->seq, s, ms);
    pthread_mutex_lock(m);
    (void)cuenta;
    return r;
}
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) { espera(c, m, 0); return 0; }
int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *ts)
{
    struct timespec ahora;
    clock_gettime(CLOCK_REALTIME, &ahora);
    long long ms = ((long long)ts->tv_sec - ahora.tv_sec) * 1000 + (ts->tv_nsec - ahora.tv_nsec) / 1000000;
    if (ms <= 0) return ETIMEDOUT;
    if (ms > 0x7ffffff) ms = 0x7ffffff;
    uint32_t s = c->seq;
    espera(c, m, (unsigned)ms);
    return c->seq == s ? ETIMEDOUT : 0;
}

/* ---------------------------------------------------------------- almacén por hilo */
#define MAX_CLAVES 32
#define MAX_VALORES 1024
static int g_claves_usadas[MAX_CLAVES];
static struct { unsigned clave; int tid; void *v; } g_vals[MAX_VALORES];

int pthread_key_create(pthread_key_t *k, void (*dtor)(void *))
{
    (void)dtor;
    mutex_tomar(&g_cerrojo);
    int r = EAGAIN;
    for (int i = 0; i < MAX_CLAVES; i++)
        if (!g_claves_usadas[i]) { g_claves_usadas[i] = 1; *k = (unsigned)i; r = 0; break; }
    mutex_largar(&g_cerrojo);
    return r;
}
int pthread_key_delete(pthread_key_t k)
{
    mutex_tomar(&g_cerrojo);
    if (k < MAX_CLAVES) g_claves_usadas[k] = 0;
    for (int i = 0; i < MAX_VALORES; i++) if (g_vals[i].tid && g_vals[i].clave == k) g_vals[i].tid = 0;
    mutex_largar(&g_cerrojo);
    return 0;
}
void *pthread_getspecific(pthread_key_t k)
{
    int tid = thread_id();
    void *r = NULL;
    mutex_tomar(&g_cerrojo);
    for (int i = 0; i < MAX_VALORES; i++)
        if (g_vals[i].tid == tid && g_vals[i].clave == k) { r = g_vals[i].v; break; }
    mutex_largar(&g_cerrojo);
    return r;
}
int pthread_setspecific(pthread_key_t k, const void *v)
{
    int tid = thread_id(), libre = -1, r = 0;
    mutex_tomar(&g_cerrojo);
    for (int i = 0; i < MAX_VALORES; i++) {
        if (g_vals[i].tid == tid && g_vals[i].clave == k) { g_vals[i].v = (void *)v; libre = -2; break; }
        if (!g_vals[i].tid && libre < 0) libre = i;
    }
    if (libre >= 0) { g_vals[libre].tid = tid; g_vals[libre].clave = k; g_vals[libre].v = (void *)v; }
    else if (libre == -1) r = ENOMEM;
    mutex_largar(&g_cerrojo);
    return r;
}

int pthread_once(pthread_once_t *o, void (*f)(void))
{
    if (atomico_cas32((volatile uint32_t *)o, 0, 1)) { f(); *o = 2; }
    else while (*o != 2) thread_yield();
    return 0;
}
