/* mpthreadport.h - los hilos del MicroPython en el SaguiOS (D170).
 *
 * El módulo _thread (py/modthread.c) pide al puerto: crear hilos, un mutex
 * y el estado de cada hilo (mp_thread_get_state). Aquí van sobre los hilos
 * de usuario de la D167 (hilo_criar, futex); el código está en
 * mpthreadport.c.
 *
 * El estado del hilo se lee en casi cada paso del intérprete (MP_STATE_THREAD:
 * la cadena de excepciones, la comprobación de la pila, los globales...).
 * La libsagui no tiene variables por hilo (TLS) y preguntar el tid al kernel
 * en cada paso sería una llamada al sistema por paso. En su lugar, el estado
 * se halla por la pila, sin llamar al kernel:
 *
 *   - el hilo principal tiene la pila del programa, en lo más alto de la
 *     memoria del usuario (de USUARIO_PILHA a USUARIO_FIM, 8 MB que se
 *     asignan al tocarlos, D186, kernel/inc/paginacao.h): si ESP cae ahí, el
 *     estado es el de siempre (mp_state_ctx.thread);
 *   - cada hilo de Python tiene una pila de SAGUI_HILO_PILHA bytes alineada
 *     a su propio tamaño: la primera palabra de esa área (ESP con los bits
 *     bajos a cero) guarda el puntero a su estado.
 *
 * Son una comparación y una lectura de memoria, en línea. */
#ifndef SAGUI_MPTHREADPORT_H
#define SAGUI_MPTHREADPORT_H

#include <stdint.h>

/* El mutex del puerto: un cerrojo por turnos (ticket lock) sobre el futex
 * del kernel. Cada uno coge un número (proximo) y entra cuando le toca
 * (servindo). Es justo: quien suelta el GIL y lo vuelve a pedir enseguida
 * se pone a la cola detrás de los que esperan; con el mutex_t de la
 * libsagui (que no lo es) un hilo que calcula no soltaría nunca el GIL a
 * otro en la práctica. */
typedef struct {
    volatile uint32_t proximo;     /* el siguiente número que se reparte */
    volatile uint32_t servindo;    /* el número que tiene el cerrojo ahora */
    volatile uint32_t esperando;   /* cuántos duermen en el futex de servindo */
} mp_thread_mutex_t;

/* Pila de cada hilo de Python: potencia de dos (por la máscara de abajo).
 * Sale del heap (mpthreadport.c), no de los huecos con guarda del kernel
 * (D186): el recolector recorre la pila entera de los demás hilos, y en un
 * hueco de 1 MB eso asignaría todas sus páginas. */
#define SAGUI_HILO_PILHA          (64u * 1024u)
#define SAGUI_PILHA_PRINCIPAL_BASE 0xFF400000u   /* USUARIO_PILHA (D186) */

/* El estado del hilo principal es el primer campo de mp_state_ctx (lo
 * comprueba mpthreadport.c). Se nombra el símbolo directamente para que su
 * dirección sea una constante: con un puntero en memoria, cada
 * MP_STATE_THREAD costaría una lectura más (un 15 % en las llamadas). */
struct _mp_state_thread_t;
extern struct _mp_state_thread_t sagui_estado_principal __asm__("mp_state_ctx");

static inline __attribute__((always_inline)) struct _mp_state_thread_t *sagui_estado_hilo(void)
{
    uint32_t esp;
    /* Sin volatile: dentro de una función la pila es siempre la misma, y el
     * compilador puede reutilizar el valor. */
    __asm__("movl %%esp, %0" : "=r"(esp));
    if (esp >= SAGUI_PILHA_PRINCIPAL_BASE) return &sagui_estado_principal;
    return *(struct _mp_state_thread_t **)(esp & ~(SAGUI_HILO_PILHA - 1u));
}

/* py/mpthread.h declara mp_thread_get_state() como función normal; con este
 * nombre su declaración pasa a ser la de la función en línea de arriba. */
#define mp_thread_get_state sagui_estado_hilo

void mp_thread_gc_others(void);
void sagui_hilos_encerrar(void);   /* el principal acaba: los demás no vuelven a Python */

#endif
