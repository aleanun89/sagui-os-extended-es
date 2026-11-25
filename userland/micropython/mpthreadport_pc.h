/* SPDX-License-Identifier: GPL-2.0-or-later */
/* mpthreadport_pc.h - los hilos del MicroPython de SaguiOS en el PC (D459, VisualPythonS).
 *
 * En SaguiOS el modulo _thread va sobre los hilos de usuario del kernel (mpthreadport.h); aqui, sobre pthreads.
 * El mutex es un cerrojo por turnos (justo, como el de SaguiOS): quien suelta el GIL y lo vuelve a pedir se pone
 * detras de los que esperan. El estado del hilo esta en una variable por hilo (__thread). */
#ifndef SAGUI_MPTHREADPORT_PC_H
#define SAGUI_MPTHREADPORT_PC_H

#include <pthread.h>
#include <stdint.h>

typedef struct {
    pthread_mutex_t m;
    pthread_cond_t c;
    unsigned long proximo;    /* el siguiente numero que se reparte */
    unsigned long servindo;   /* el numero que tiene el cerrojo ahora */
} mp_thread_mutex_t;

struct _mp_state_thread_t;
extern struct _mp_state_thread_t mp_state_ctx_thread_principal __asm__("mp_state_ctx");
extern __thread struct _mp_state_thread_t *sagui_estado_tls;

static inline __attribute__((always_inline)) struct _mp_state_thread_t *sagui_estado_hilo(void)
{
    return sagui_estado_tls ? sagui_estado_tls : &mp_state_ctx_thread_principal;
}
#define mp_thread_get_state sagui_estado_hilo

void mp_thread_gc_others(void);
void sagui_hilos_encerrar(void);   /* el principal acaba: los demas no vuelven a Python */

#endif
