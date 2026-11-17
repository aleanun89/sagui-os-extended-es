/* pthread.h - lo mínimo de POSIX threads para tinycthread (miniJVM) sobre los hilos y el futex de
 * SaguiOS (D167, D376). Implementación en pthread_sagui.c. Cabecera propia, no de terceros. */
#ifndef SAGUI_PTHREAD_H
#define SAGUI_PTHREAD_H
#include <stdint.h>
#include <time.h>

typedef unsigned long pthread_t;
typedef struct {
    volatile uint32_t m[2];   /* mutex_t de la libsagui (estado, dueño) */
    int tipo;                 /* 0 normal, 1 recursivo */
    volatile uint32_t dono;   /* tid del dueño (recursivo) */
    int cuenta;
} pthread_mutex_t;
typedef struct { int tipo; } pthread_mutexattr_t;
typedef struct { volatile uint32_t seq; } pthread_cond_t;
typedef int pthread_condattr_t;
typedef unsigned pthread_key_t;
typedef volatile int pthread_once_t;
typedef int pthread_attr_t;

#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_INITIALIZER { { 0, 0 }, 0, 0, 0 }
#define PTHREAD_ONCE_INIT 0
#define PTHREAD_DESTRUCTOR_ITERATIONS 4

int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg);
int pthread_join(pthread_t t, void **res);
int pthread_detach(pthread_t t);
void pthread_exit(void *res) __attribute__((noreturn));
pthread_t pthread_self(void);
int pthread_equal(pthread_t a, pthread_t b);
int pthread_mutexattr_init(pthread_mutexattr_t *a);
int pthread_mutexattr_destroy(pthread_mutexattr_t *a);
int pthread_mutexattr_settype(pthread_mutexattr_t *a, int tipo);
int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a);
int pthread_mutex_destroy(pthread_mutex_t *m);
int pthread_mutex_lock(pthread_mutex_t *m);
int pthread_mutex_trylock(pthread_mutex_t *m);
int pthread_mutex_timedlock(pthread_mutex_t *m, const struct timespec *ts);
int pthread_mutex_unlock(pthread_mutex_t *m);
int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a);
int pthread_cond_destroy(pthread_cond_t *c);
int pthread_cond_signal(pthread_cond_t *c);
int pthread_cond_broadcast(pthread_cond_t *c);
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);
int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *ts);
int pthread_key_create(pthread_key_t *k, void (*dtor)(void *));
int pthread_key_delete(pthread_key_t k);
void *pthread_getspecific(pthread_key_t k);
int pthread_setspecific(pthread_key_t k, const void *v);
int pthread_once(pthread_once_t *o, void (*f)(void));
int sched_yield(void);
#endif
