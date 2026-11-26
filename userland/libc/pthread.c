/* pthread.c - hilos POSIX mínimos sobre la libsagui (D458, docs/ffmpeg.md). Ver pthread.h. */
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "sagui.h"
#include "pthread.h"
#include "sched.h"

/* Todas las funciones son débiles: el port del miniJVM trae su propio pthread_sagui.c (con su cabecera) y gana en el enlace. */
#define FRACA __attribute__((weak))

FRACA int sched_yield(void) { thread_yield(); return 0; }

/* ---- atributos ---- */
FRACA int pthread_attr_init(pthread_attr_t *a) { a->tam_pila = 0; a->separado = 0; return 0; }
FRACA int pthread_attr_destroy(pthread_attr_t *a) { (void)a; return 0; }
FRACA int pthread_attr_setstacksize(pthread_attr_t *a, size_t n) { a->tam_pila = n; return 0; }
FRACA int pthread_attr_getstacksize(const pthread_attr_t *a, size_t *n) { *n = a->tam_pila ? a->tam_pila : 960u * 1024u; return 0; }
FRACA int pthread_attr_setdetachstate(pthread_attr_t *a, int d) { a->separado = d; return 0; }

/* ---- hilos ---- */
FRACA int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg)
{
    unsigned pila = (a && a->tam_pila > 960u * 1024u) ? (unsigned)a->tam_pila : 0;   /* hasta 960 KB, la del kernel */
    thread_t *h = thread_create(f, arg, pila);
    if (!h) return EAGAIN;
    *t = (pthread_t)h;
    return 0;
}
FRACA int pthread_join(pthread_t t, void **ret)
{
    if (t < 0x1000) return EINVAL;   /* el principal u otro hilo que no creó pthread_create */
    int r = thread_join((thread_t *)t);
    if (ret) *ret = (void *)(intptr_t)r;
    return 0;
}
FRACA int pthread_detach(pthread_t t) { (void)t; return 0; }   /* la memoria de la estructura queda hasta que acaba el proceso */
FRACA void pthread_exit(void *ret) { (void)ret; thread_sair(); for (;;) {} }
FRACA pthread_t pthread_self(void) { return (pthread_t)thread_id(); }
FRACA int pthread_equal(pthread_t a, pthread_t b) { return a == b; }
FRACA int pthread_setname_np(pthread_t t, const char *nombre) { (void)t; (void)nombre; return 0; }

/* ---- mutex (sagui.h: mutex_t) ---- */
FRACA int pthread_mutexattr_init(pthread_mutexattr_t *a) { a->tipo = PTHREAD_MUTEX_NORMAL; return 0; }
FRACA int pthread_mutexattr_destroy(pthread_mutexattr_t *a) { (void)a; return 0; }
FRACA int pthread_mutexattr_settype(pthread_mutexattr_t *a, int tipo) { a->tipo = tipo; return 0; }
FRACA int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a)
{
    m->estado = 0; m->dono = 0; m->cuenta = 0; m->propietario = 0;
    m->tipo = a ? a->tipo : PTHREAD_MUTEX_NORMAL;
    return 0;
}
FRACA int pthread_mutex_destroy(pthread_mutex_t *m) { (void)m; return 0; }
FRACA int pthread_mutex_lock(pthread_mutex_t *m)
{
    if (m->tipo == PTHREAD_MUTEX_RECURSIVE) {
        unsigned yo = (unsigned)thread_id();
        if (m->cuenta > 0 && m->propietario == yo) { m->cuenta++; return 0; }
        mutex_tomar((mutex_t *)m);
        m->propietario = yo; m->cuenta = 1;
        return 0;
    }
    mutex_tomar((mutex_t *)m);
    return 0;
}
FRACA int pthread_mutex_trylock(pthread_mutex_t *m)
{
    if (m->tipo == PTHREAD_MUTEX_RECURSIVE) {
        unsigned yo = (unsigned)thread_id();
        if (m->cuenta > 0 && m->propietario == yo) { m->cuenta++; return 0; }
        if (!mutex_tentar((mutex_t *)m)) return EBUSY;
        m->propietario = yo; m->cuenta = 1;
        return 0;
    }
    return mutex_tentar((mutex_t *)m) ? 0 : EBUSY;
}
FRACA int pthread_mutex_unlock(pthread_mutex_t *m)
{
    if (m->tipo == PTHREAD_MUTEX_RECURSIVE) {
        if (--m->cuenta > 0) return 0;
        m->propietario = 0;
    }
    mutex_largar((mutex_t *)m);
    return 0;
}

/* ---- variables de condición: un contador y el futex del kernel ---- */
FRACA int pthread_condattr_init(pthread_condattr_t *a) { a->reloj = CLOCK_REALTIME; return 0; }
FRACA int pthread_condattr_destroy(pthread_condattr_t *a) { (void)a; return 0; }
FRACA int pthread_condattr_setclock(pthread_condattr_t *a, clockid_t reloj) { a->reloj = reloj; return 0; }
FRACA int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a) { (void)a; c->seq = 0; return 0; }
FRACA int pthread_cond_destroy(pthread_cond_t *c) { (void)c; return 0; }

/* Espera hasta ms milisegundos (0 = sin límite). Devuelve 0, o ETIMEDOUT si pasó el plazo. */
static int cond_esperar(pthread_cond_t *c, pthread_mutex_t *m, unsigned ms, int con_plazo)
{
    uint32_t visto = c->seq;
    int cuenta = m->cuenta;
    if (m->tipo == PTHREAD_MUTEX_RECURSIVE) { m->cuenta = 1; }
    pthread_mutex_unlock(m);
    unsigned t0 = tempo_ms();
    int r = futex_esperar(&c->seq, visto, con_plazo ? (ms ? ms : 1) : 0);
    pthread_mutex_lock(m);
    if (m->tipo == PTHREAD_MUTEX_RECURSIVE) m->cuenta = cuenta;
    if (r == 0 && con_plazo && c->seq == visto && tempo_ms() - t0 >= ms) return ETIMEDOUT;
    return 0;
}
FRACA int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) { return cond_esperar(c, m, 0, 0); }
FRACA int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *limite)
{
    struct timespec ahora;
    clock_gettime(CLOCK_REALTIME, &ahora);
    long long dif = ((long long)limite->tv_sec - ahora.tv_sec) * 1000 + (limite->tv_nsec - ahora.tv_nsec) / 1000000;
    if (dif <= 0) {   /* ya pasó: ceder y devolver */
        pthread_mutex_unlock(m); thread_yield(); pthread_mutex_lock(m);
        return ETIMEDOUT;
    }
    if (dif > 0x7fffffff) dif = 0x7fffffff;
    return cond_esperar(c, m, (unsigned)dif, 1);
}
FRACA int pthread_cond_signal(pthread_cond_t *c) { atomico_inc32(&c->seq); futex_acordar(&c->seq, 1); return 0; }
FRACA int pthread_cond_broadcast(pthread_cond_t *c) { atomico_inc32(&c->seq); futex_acordar(&c->seq, INT_MAX); return 0; }

/* ---- cerrojo de lectura y escritura: exclusivo ---- */
FRACA int pthread_rwlock_init(pthread_rwlock_t *l, const pthread_rwlockattr_t *a) { (void)a; return pthread_mutex_init(&l->m, NULL); }
FRACA int pthread_rwlock_destroy(pthread_rwlock_t *l) { (void)l; return 0; }
FRACA int pthread_rwlock_rdlock(pthread_rwlock_t *l) { return pthread_mutex_lock(&l->m); }
FRACA int pthread_rwlock_wrlock(pthread_rwlock_t *l) { return pthread_mutex_lock(&l->m); }
FRACA int pthread_rwlock_unlock(pthread_rwlock_t *l) { return pthread_mutex_unlock(&l->m); }

/* ---- once: 0 = sin hacer, 1 = haciéndose, 2 = hecho ---- */
FRACA int pthread_once(pthread_once_t *o, void (*f)(void))
{
    if (*o == 2) return 0;
    if (atomico_cas32((volatile uint32_t *)o, 0, 1)) {
        f();
        atomico_barreira();
        *o = 2;
        return 0;
    }
    while (*o != 2) thread_yield();
    return 0;
}

/* ---- claves por hilo: una tabla (clave, tid) -> valor, con un mutex; las entradas de hilos que ya acabaron
 * se quedan hasta el final del proceso (los hilos de un programa son pocos). ---- */
#define PT_CLAVES 64
#define PT_VALORES 512
static struct { int usada; void (*destructor)(void *); } pt_claves[PT_CLAVES];
static struct { int tid; unsigned clave; void *valor; } pt_valores[PT_VALORES];
static mutex_t pt_m = MUTEX_INICIAL;

FRACA int pthread_key_create(pthread_key_t *k, void (*destructor)(void *))
{
    int r = EAGAIN;
    mutex_tomar(&pt_m);
    for (unsigned i = 0; i < PT_CLAVES; i++)
        if (!pt_claves[i].usada) { pt_claves[i].usada = 1; pt_claves[i].destructor = destructor; *k = i; r = 0; break; }
    mutex_largar(&pt_m);
    return r;
}
FRACA int pthread_key_delete(pthread_key_t k)
{
    if (k >= PT_CLAVES) return EINVAL;
    mutex_tomar(&pt_m);
    pt_claves[k].usada = 0;
    for (int i = 0; i < PT_VALORES; i++) if (pt_valores[i].tid && pt_valores[i].clave == k) pt_valores[i].tid = 0;
    mutex_largar(&pt_m);
    return 0;
}
FRACA void *pthread_getspecific(pthread_key_t k)
{
    void *v = NULL;
    int yo = thread_id();
    mutex_tomar(&pt_m);
    for (int i = 0; i < PT_VALORES; i++)
        if (pt_valores[i].tid == yo && pt_valores[i].clave == k) { v = pt_valores[i].valor; break; }
    mutex_largar(&pt_m);
    return v;
}
FRACA int pthread_setspecific(pthread_key_t k, const void *v)
{
    if (k >= PT_CLAVES) return EINVAL;
    int yo = thread_id(), r = ENOMEM, libre = -1;
    mutex_tomar(&pt_m);
    for (int i = 0; i < PT_VALORES; i++) {
        if (pt_valores[i].tid == yo && pt_valores[i].clave == k) { pt_valores[i].valor = (void *)v; r = 0; libre = -2; break; }
        if (!pt_valores[i].tid && libre < 0) libre = i;
    }
    if (libre >= 0) { pt_valores[libre].tid = yo; pt_valores[libre].clave = k; pt_valores[libre].valor = (void *)v; r = 0; }
    mutex_largar(&pt_m);
    return r;
}

/* D477: cancelación y señales a hilos, sin soporte (ver pthread.h). */
FRACA int pthread_cancel(pthread_t t) { (void)t; return ENOSYS; }
FRACA int pthread_kill(pthread_t t, int sinal) { (void)t; (void)sinal; return ENOSYS; }
FRACA int pthread_setcancelstate(int estado, int *antiguo) { (void)estado; if (antiguo) *antiguo = 0; return 0; }
FRACA int pthread_setcanceltype(int tipo, int *antiguo) { (void)tipo; if (antiguo) *antiguo = 0; return 0; }
FRACA int pthread_attr_setguardsize(pthread_attr_t *a, size_t n) { (void)a; (void)n; return 0; }
