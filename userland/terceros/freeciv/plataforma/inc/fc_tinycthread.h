/* fc_tinycthread.h - hilos de C11 sin hilos, para Freeciv en SaguiOS (D374).
 * Freeciv solo usa hilos para el escaneo del metaservidor y las IA «threaded», que aquí no
 * existen: los cerrojos no hacen nada y thrd_create falla. */
#ifndef FC_TINYCTHREAD_SAGUI_H
#define FC_TINYCTHREAD_SAGUI_H
typedef int thrd_t;
typedef int mtx_t;
typedef int cnd_t;
typedef int (*thrd_start_t)(void *);
enum { thrd_success = 0, thrd_error = 2 };
enum { mtx_plain = 1, mtx_recursive = 2 };
static inline int thrd_create(thrd_t *t, thrd_start_t f, void *a) { (void)t; (void)f; (void)a; return thrd_error; }
static inline int thrd_join(thrd_t t, int *r) { (void)t; (void)r; return thrd_success; }
static inline int mtx_init(mtx_t *m, int t) { (void)t; *m = 0; return thrd_success; }
static inline void mtx_destroy(mtx_t *m) { (void)m; }
static inline int mtx_lock(mtx_t *m) { (void)m; return thrd_success; }
static inline int mtx_unlock(mtx_t *m) { (void)m; return thrd_success; }
static inline int cnd_init(cnd_t *c) { *c = 0; return thrd_success; }
static inline void cnd_destroy(cnd_t *c) { (void)c; }
static inline int cnd_wait(cnd_t *c, mtx_t *m) { (void)c; (void)m; return thrd_success; }
static inline int cnd_signal(cnd_t *c) { (void)c; return thrd_success; }
#endif
