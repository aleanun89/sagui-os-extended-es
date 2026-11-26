/* pthread.h - hilos POSIX mínimos sobre los hilos de la libsagui (D458, docs/ffmpeg.md).
 *
 * Capa fina sobre thread_create/thread_join y los mutex y el futex de sagui.h (docs/multihilo.md): lo justo
 * que pide un port como FFmpeg (hilos, mutex, variables de condición, once, claves por hilo). Sin
 * cancelación, sin prioridades ni afinidad (se aceptan y se ignoran los atributos que no se pueden cumplir).
 * pthread_t es el puntero del hilo creado con pthread_create; pthread_self() de un hilo que no creó esta
 * capa (el principal) devuelve un valor que solo sirve para pthread_equal. */
#ifndef SAGUI_PTHREAD_H
#define SAGUI_PTHREAD_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <sched.h>

typedef unsigned long pthread_t;

typedef struct {
    size_t tam_pila;       /* 0 = la pila del kernel (960 KB con guarda) */
    int    separado;
} pthread_attr_t;

typedef struct {
    volatile uint32_t estado;   /* igual que mutex_t de sagui.h */
    volatile uint32_t dono;
    int      tipo;
    int      cuenta;            /* tomas del dueño (solo recursivo) */
    unsigned propietario;       /* tid del dueño (solo recursivo) */
} pthread_mutex_t;
typedef struct { int tipo; } pthread_mutexattr_t;

typedef struct {
    volatile uint32_t seq;      /* se incrementa en cada signal/broadcast; el futex duerme sobre él */
} pthread_cond_t;
typedef struct { int reloj; } pthread_condattr_t;

/* El cerrojo de lectura y escritura es exclusivo (los lectores también se turnan): vale para el uso que hay. */
typedef struct { pthread_mutex_t m; } pthread_rwlock_t;
typedef struct { int vacio; } pthread_rwlockattr_t;

typedef volatile int pthread_once_t;
typedef unsigned pthread_key_t;

#define PTHREAD_MUTEX_NORMAL     0
#define PTHREAD_MUTEX_RECURSIVE  1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT    PTHREAD_MUTEX_NORMAL
#define PTHREAD_MUTEX_INITIALIZER   { 0, 0, 0, 0, 0 }
#define PTHREAD_COND_INITIALIZER    { 0 }
#define PTHREAD_RWLOCK_INITIALIZER  { PTHREAD_MUTEX_INITIALIZER }
#define PTHREAD_ONCE_INIT 0
#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1
#define PTHREAD_STACK_MIN 16384

int pthread_attr_init(pthread_attr_t *a);
int pthread_attr_destroy(pthread_attr_t *a);
int pthread_attr_setstacksize(pthread_attr_t *a, size_t n);
int pthread_attr_getstacksize(const pthread_attr_t *a, size_t *n);
int pthread_attr_setdetachstate(pthread_attr_t *a, int d);

int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg);
int pthread_join(pthread_t t, void **ret);
int pthread_detach(pthread_t t);
void pthread_exit(void *ret) __attribute__((noreturn));
pthread_t pthread_self(void);
int pthread_equal(pthread_t a, pthread_t b);
/* D477 (Valkey): sin cancelación ni señales a hilos. pthread_cancel y pthread_kill devuelven ENOSYS (el hilo sigue);
 * pthread_setcancelstate/type aceptan y no hacen nada. Valkey solo los usa al apagar o en el informe de caída. */
#define PTHREAD_CANCEL_ENABLE       0
#define PTHREAD_CANCEL_DISABLE      1
#define PTHREAD_CANCEL_DEFERRED     0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1
int pthread_cancel(pthread_t t);
int pthread_kill(pthread_t t, int sinal);
int pthread_setcancelstate(int estado, int *antiguo);
int pthread_setcanceltype(int tipo, int *antiguo);
int pthread_attr_setguardsize(pthread_attr_t *a, size_t n);
int pthread_setname_np(pthread_t t, const char *nombre);

int pthread_mutexattr_init(pthread_mutexattr_t *a);
int pthread_mutexattr_destroy(pthread_mutexattr_t *a);
int pthread_mutexattr_settype(pthread_mutexattr_t *a, int tipo);
int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a);
int pthread_mutex_destroy(pthread_mutex_t *m);
int pthread_mutex_lock(pthread_mutex_t *m);
int pthread_mutex_trylock(pthread_mutex_t *m);
int pthread_mutex_unlock(pthread_mutex_t *m);

int pthread_condattr_init(pthread_condattr_t *a);
int pthread_condattr_destroy(pthread_condattr_t *a);
int pthread_condattr_setclock(pthread_condattr_t *a, clockid_t reloj);
int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a);
int pthread_cond_destroy(pthread_cond_t *c);
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);
int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *limite);
int pthread_cond_signal(pthread_cond_t *c);
int pthread_cond_broadcast(pthread_cond_t *c);

int pthread_rwlock_init(pthread_rwlock_t *l, const pthread_rwlockattr_t *a);
int pthread_rwlock_destroy(pthread_rwlock_t *l);
int pthread_rwlock_rdlock(pthread_rwlock_t *l);
int pthread_rwlock_wrlock(pthread_rwlock_t *l);
int pthread_rwlock_unlock(pthread_rwlock_t *l);

int pthread_once(pthread_once_t *o, void (*f)(void));

int pthread_key_create(pthread_key_t *k, void (*destructor)(void *));
int pthread_key_delete(pthread_key_t k);
void *pthread_getspecific(pthread_key_t k);
int pthread_setspecific(pthread_key_t k, const void *v);

#endif
