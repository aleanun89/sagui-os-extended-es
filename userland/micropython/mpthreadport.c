/* mpthreadport.c - el módulo _thread del MicroPython sobre los hilos del
 * SaguiOS (D170).
 *
 * Lo que py/modthread.c pide al puerto (py/mpthread.h):
 *
 *   - mp_thread_create: un hilo de usuario de la D167 (hilo_criar) con una
 *     pila de SAGUI_HILO_PILHA alineada a su tamaño; la primera palabra de
 *     la pila guarda el estado del hilo (mp_thread_get_state, en
 *     mpthreadport.h, lo halla por ESP sin llamar al kernel).
 *   - mp_thread_mutex_*: un cerrojo por turnos sobre el futex (ver
 *     mpthreadport.h). Sirve para el GIL y para los Lock de Python.
 *   - mp_thread_gc_others: el recolector, que corre en un hilo, tiene que
 *     ver los objetos que los demás tienen en sus pilas y registros.
 *
 * El GIL (MICROPY_PY_THREAD_GIL): un solo hilo ejecuta Python a la vez. El
 * intérprete lo suelta cada MICROPY_PY_THREAD_GIL_VM_DIVISOR saltos hacia
 * atrás y en las esperas (time.sleep, Lock.acquire, la red, la entrada).
 *
 * Los registros de los otros hilos. Sin señales en el SaguiOS, no se puede
 * pedir a otro hilo que vuelque sus registros en el momento de recolectar
 * (lo que hace el puerto de Unix). Pero con el GIL, mientras un hilo
 * recolecta los demás están quietos en uno de dos sitios:
 *   a) esperando el GIL: al entrar en mp_thread_mutex_lock(GIL) se
 *      fotografían sus registros callee-saved (EBX, ESI, EDI, EBP) y ESP.
 *      Todo puntero vivo tras la llamada está ahí o en la pila por encima
 *      de ese ESP: la foto es exacta;
 *   b) en una espera sin el GIL (dormir, leer de la red): al soltarlo
 *      (mp_thread_mutex_unlock(GIL)) se fotografían igual. El código entre
 *      soltar y volver a coger el GIL no crea ni toca objetos de Python; los
 *      punteros que lleve son los de la foto.
 * Además se recorre la pila entera de cada hilo (no solo desde su ESP): lo
 * de más abajo solo puede hacer que se guarde algún objeto de más.
 *
 * La lista de hilos la protege el propio GIL: solo se toca con él cogido
 * (crear, empezar, acabar, recolectar). */
#include <stddef.h>
#include <string.h>
#include "py/runtime.h"
#include "py/gc.h"
#include "py/mpthread.h"
#include "py/mperrno.h"
#include "sagui_mp.h"

#if MICROPY_PY_THREAD

/* mpthreadport.h toma el estado del hilo principal por la dirección de
 * mp_state_ctx: tiene que ser su primer campo. */
_Static_assert(offsetof(mp_state_ctx_t, thread) == 0, "mp_state_ctx.thread tiene que ir primero");

/* Las dos palabras al pie de la pila de cada hilo. */
#define PILHA_ESTADO   0   /* mp_state_thread_t *  (mp_thread_set_state) */
#define PILHA_REGISTRO 1   /* hilo_py_t *          (el registro de abajo) */
#define PILHA_RESERVA  (12u * 1024u)  /* lo que no se deja usar a Python: el mismo margen que el principal (PILHA_LIMITE, mp_sagui.c) */

enum { HILO_CRIADO, HILO_CORRENDO, HILO_ACABADO };

typedef struct {
    uint32_t ebx, esi, edi, ebp, esp;
} foto_t;

typedef struct hilo_py {
    struct hilo_py *prox;
    int      tid;
    volatile int estado;
    void  *(*entrada)(void *);
    void    *arg;            /* los argumentos del hilo (en el heap de Python) */
    void    *bloco;          /* lo pedido a la libsagui, para liberarlo */
    uint32_t base;           /* la pila: [base, base + SAGUI_HILO_PILHA) */
    foto_t   ao_soltar;      /* al soltar el GIL (con el GIL cogido) */
    foto_t   ao_pedir;       /* al pedir el GIL (sin él) */
} hilo_py_t;

static hilo_py_t *hilos;            /* los hilos de Python vivos o por recoger */
static hilo_py_t principal;         /* las fotos del hilo principal */
static volatile bool encerrando;    /* sagui_hilos_encerrar */

/* ------------------------------------------------------------ atómicas */

static inline uint32_t xadd(volatile uint32_t *p, uint32_t v)
{
    __asm__ volatile("lock; xaddl %0, %1" : "+r"(v), "+m"(*p) : : "cc", "memory");
    return v;
}

static inline int cas(volatile uint32_t *p, uint32_t esperado, uint32_t novo)
{
    uint32_t r = esperado;
    __asm__ volatile("lock; cmpxchgl %2, %0" : "+m"(*p), "+a"(r) : "r"(novo) : "cc", "memory");
    return r == esperado;
}

/* ------------------------------------------------------------ cada hilo */

static inline bool na_pilha_principal(void)
{
    uint32_t esp;
    __asm__("movl %%esp, %0" : "=r"(esp));
    return esp >= SAGUI_PILHA_PRINCIPAL_BASE;
}

static inline hilo_py_t *eu(void)
{
    if (na_pilha_principal()) return &principal;
    uint32_t esp;
    __asm__("movl %%esp, %0" : "=r"(esp));
    return ((hilo_py_t **)(esp & ~(SAGUI_HILO_PILHA - 1u)))[PILHA_REGISTRO];
}

void mp_thread_set_state(struct _mp_state_thread_t *estado)
{
    if (na_pilha_principal()) return;   /* el principal siempre usa mp_state_ctx.thread */
    uint32_t esp;
    __asm__("movl %%esp, %0" : "=r"(esp));
    ((struct _mp_state_thread_t **)(esp & ~(SAGUI_HILO_PILHA - 1u)))[PILHA_ESTADO] = estado;
}

/* Los registros que la función que llama espera conservar, y ESP. Sin
 * inline: así los registros son los de quien la llama (o están guardados en
 * su pila, por encima del ESP de aquí). */
static void __attribute__((noinline)) fotografar(foto_t *f)
{
    __asm__ volatile("movl %%ebx, 0(%0)\n\t"
                     "movl %%esi, 4(%0)\n\t"
                     "movl %%edi, 8(%0)\n\t"
                     "movl %%ebp, 12(%0)\n\t"
                     "movl %%esp, 16(%0)"
                     : : "r"(f) : "memory");
}

mp_uint_t mp_thread_get_id(void) { return (mp_uint_t)thread_id(); }

void mp_thread_start(void) { eu()->estado = HILO_CORRENDO; }
void mp_thread_finish(void) { eu()->estado = HILO_ACABADO; }

/* Donde empieza el hilo: la pila trae un retorno falso (0) y el registro. */
static void inicio_hilo(hilo_py_t *h)
{
    h->entrada(h->arg);
    hilo_sair(0);
}

/* Recoge los hilos que ya acabaron (con el GIL: ya lo soltaron y solo les
 * queda hilo_sair) y libera sus pilas. */
static void recolher_acabados(void)
{
    hilo_py_t **pp = &hilos;
    while (*pp) {
        hilo_py_t *h = *pp;
        if (h->estado == HILO_ACABADO) {
            int codigo;
            hilo_esperar(h->tid, &codigo);
            *pp = h->prox;
            libera(h->bloco);
            libera(h);
        } else {
            pp = &h->prox;
        }
    }
}

mp_uint_t mp_thread_create(void *(*entrada)(void *), void *arg, size_t *tam_pilha)
{
    recolher_acabados();
    hilo_py_t *h = aloca(sizeof *h);
    void *bloco = h ? aloca(2 * SAGUI_HILO_PILHA) : NULL;
    if (!bloco) {
        if (h) libera(h);
        mp_raise_OSError(MP_ENOMEM);
    }
    memset(h, 0, sizeof *h);
    h->entrada = entrada;
    h->arg = arg;
    h->bloco = bloco;
    h->estado = HILO_CRIADO;
    h->base = ((uint32_t)bloco + SAGUI_HILO_PILHA - 1u) & ~(SAGUI_HILO_PILHA - 1u);
    uint32_t *pe = (uint32_t *)h->base;
    pe[PILHA_ESTADO] = 0;                  /* lo pone mp_thread_init_state, lo primero del hilo */
    pe[PILHA_REGISTRO] = (uint32_t)h;
    /* Como en hilos.c: (ESP + 4) alineado a 16 al entrar en la función. */
    uint32_t *topo = (uint32_t *)((h->base + SAGUI_HILO_PILHA - 16u) & ~15u) - 1;
    topo[0] = 0;
    topo[1] = (uint32_t)h;
    /* Lo que Python puede usar, contado desde el estado del hilo (que está
     * casi arriba de la pila, en thread_entry). */
    *tam_pilha = SAGUI_HILO_PILHA - PILHA_RESERVA;
    h->prox = hilos;
    hilos = h;
    int tid = hilo_criar((void (*)(void))inicio_hilo, topo);
    if (tid < 0) {
        hilos = h->prox;
        libera(bloco);
        libera(h);
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("no se puede crear el hilo"));
    }
    h->tid = tid;
    return (mp_uint_t)tid;
}

/* ------------------------------------------------------------ mutex */

void mp_thread_mutex_init(mp_thread_mutex_t *m)
{
    m->proximo = 0;
    m->servindo = 0;
    m->esperando = 0;
}

static inline bool e_o_gil(mp_thread_mutex_t *m)
{
    return m == &MP_STATE_VM(gil_mutex);
}

static void soltar(mp_thread_mutex_t *m)
{
    xadd(&m->servindo, 1);   /* barrera completa: lo escrito antes queda visible */
    /* Se despierta a todos: cada uno mira si es su turno (son pocos hilos). */
    if (m->esperando) futex_acordar(&m->servindo, 0x7FFFFFFF);
}

int mp_thread_mutex_lock(mp_thread_mutex_t *m, int esperar)
{
    bool gil = hilos && e_o_gil(m);
    /* Sin otros hilos nadie recolecta por nosotros: no hace falta la foto. */
    if (gil) fotografar(&eu()->ao_pedir);
    if (!esperar) {
        uint32_t s = m->servindo;
        return cas(&m->proximo, s, s + 1);
    }
    uint32_t vez = xadd(&m->proximo, 1);
    /* Unas vueltas por si quien lo tiene está en otra CPU y lo suelta ya;
     * después, a dormir hasta que cambie "servindo". */
    int i = 0;
    while (m->servindo != vez && i++ < 64)
        __asm__ volatile("pause" ::: "memory");
    if (m->servindo != vez) {
        xadd(&m->esperando, 1);
        for (;;) {
            uint32_t s = m->servindo;
            if (s == vez) break;
            futex_esperar(&m->servindo, s, 0);
        }
        xadd(&m->esperando, (uint32_t)-1);
    }
    /* El programa está acabando (sagui_hilos_encerrar): un hilo que no es el
     * principal ya no vuelve a Python; pasa el turno y se acaba. */
    if (gil && encerrando && !na_pilha_principal()) {
        soltar(m);
        hilo_sair(0);
    }
    return 1;
}

void mp_thread_mutex_unlock(mp_thread_mutex_t *m)
{
    if (hilos && e_o_gil(m)) fotografar(&eu()->ao_soltar);
    soltar(m);
}

/* El hilo principal acaba el programa: desde aquí, con el GIL en la mano, se
 * cierran los ficheros (gc_sweep_all) y mp_deinit suelta el GIL. Ningún otro
 * hilo debe volver a Python en ese rato: el heap ya no vale. Al salir el
 * proceso, el kernel acaba con todos. */
void sagui_hilos_encerrar(void)
{
    encerrando = true;
}

/* ------------------------------------------------------------ recolector */

static void percorrer_foto(foto_t *f)
{
    gc_collect_root((void **)f, 4);
}

static void percorrer(uint32_t de, uint32_t ate)
{
    de = (de + 3u) & ~3u;
    if (ate > de) gc_collect_root((void **)de, (ate - de) / 4);
}

/* Lo llama gc_collect (mp_sagui.c) tras recorrer la pila del hilo actual. */
void mp_thread_gc_others(void)
{
    hilo_py_t *atual = eu();
    if (atual != &principal) {
        /* El principal: desde su ESP más bajo de las fotos hasta su cima. */
        percorrer_foto(&principal.ao_soltar);
        percorrer_foto(&principal.ao_pedir);
        uint32_t de = principal.ao_soltar.esp;
        if (principal.ao_pedir.esp && principal.ao_pedir.esp < de) de = principal.ao_pedir.esp;
        if (de < SAGUI_PILHA_PRINCIPAL_BASE) de = SAGUI_PILHA_PRINCIPAL_BASE;
        percorrer(de, (uint32_t)mp_state_ctx.thread.stack_top);
    }
    for (hilo_py_t *h = hilos; h; h = h->prox) {
        if (h->estado == HILO_ACABADO) continue;
        gc_collect_root(&h->arg, 1);
        if (h == atual) continue;
        percorrer_foto(&h->ao_soltar);
        percorrer_foto(&h->ao_pedir);
        percorrer(h->base + 2 * sizeof(uint32_t), h->base + SAGUI_HILO_PILHA);
    }
}

#endif
