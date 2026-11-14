// SPDX-License-Identifier: GPL-2.0-or-later
/* atomicas.h - primitivas atómicas del kernel (D150, base del SMP).
 *
 * El kernel se compila con -mgeneral-regs-only: sin SSE2 y sin los atómicos
 * que mete el compilador por su cuenta (los __sync y los __atomic generan
 * llamadas a la biblioteca de apoyo del GCC, que aquí no existe). Por eso
 * estas primitivas van en asm en línea, con el prefijo lock.
 *
 * Por qué lock: en un x86 de 32 bits, una instrucción con lock es una
 * barrera de memoria completa (la CPU no mueve nada de la caché mientras la
 * instrucción está en curso) y hace la operación de forma atómica. El
 * cmpxchg existe desde el 486, que es la CPU mínima real de este proyecto;
 * el cmpxchg8b (el de 64 bits) es del Pentium en adelante, así que las
 * operaciones de 64 bits comprueban antes si la CPU lo tiene (atomicas_tem_cas64).
 *
 * En x86 la memoria es "strongly ordered": no hace falta mfence para dar
 * semántica de barrera, el propio lock ya la da. Por eso atomic_barrier() es
 * un lock or sobre una variable propia y no un mfence (que además pediría
 * SSE2, que el kernel no usa).
 *
 * Estas primitivas son el cimiento del SMP: cuando se arranquen los
 * procesadores secundarios, todo lo que hoy se actualiza sin más (el
 * planificador, las colas del disco, la consola) tendrá que pasar por aquí.
 * Hoy el sistema tiene un solo procesador, así que no cambian nada. */
#ifndef SAGUI_ATOMICAS_H
#define SAGUI_ATOMICAS_H
#include "tipos.h"

/* Variable usada por la barrera: sólo sirve para que el lock tenga una
 * dirección de memoria sobre la que trabajar (definida en atomicas.c). */
extern volatile uint32_t atomic_barreira;

/* Barrera de memoria completa: no se puede reordenar ni una lectura ni una
 * escritura alrededor de la llamada. En x86 es un lock sobre una variable
 * inofensiva; la barrera para el compilador la pone la cláusula "memory". */
static inline void atomic_barrier(void)
{
    __asm__ volatile("lock; orl $0, %0" : "+m"(atomic_barreira) : : "cc", "memory");
}

/* Comparación y cambio: si *p vale "esperado", pasa a valer "novo" y
 * devuelve 1; si no, deja *p como estaba y devuelve 0. */
static inline int atomic_cas32(volatile uint32_t *p, uint32_t esperado, uint32_t novo)
{
    uint32_t reg = esperado;
    /* Tras el cmpxchg, EAX vale lo que había en *p. Si sigue siendo el
     * esperado, el cambio se hizo; si no, *p tenía otro valor. */
    __asm__ volatile("lock; cmpxchgl %2, %0"
                     : "+m"(*p), "+a"(reg)
                     : "r"(novo)
                     : "cc", "memory");
    return (reg == esperado) ? 1 : 0;
}

/* Suma atómica: devuelve el valor que tenía *p antes de la suma. */
static inline uint32_t atomic_add32(volatile uint32_t *p, uint32_t valor)
{
    uint32_t r = valor;
    /* xadd intercambia: deja en el registro el valor anterior de la memoria. */
    __asm__ volatile("lock; xaddl %0, %1" : "+r"(r), "+m"(*p) : : "cc", "memory");
    return r;
}

/* Resta atómica: devuelve el valor que tenía *p antes de la resta.
 * Se hace con un xadd del valor negado, porque la resta de x86 no deja el
 * valor anterior en ningún registro (el "sub" deja en EAX el valor viejo
 * menos el sustraendo, que no sirve aquí). */
static inline uint32_t atomic_sub32(volatile uint32_t *p, uint32_t valor)
{
    uint32_t r = (uint32_t)(0u - valor);
    __asm__ volatile("lock; xaddl %0, %1" : "+r"(r), "+m"(*p) : : "cc", "memory");
    return r;
}

/* Incremento y decremento atómicos: devuelven el valor anterior. */
static inline uint32_t atomic_inc32(volatile uint32_t *p) { return atomic_add32(p, 1); }
static inline uint32_t atomic_dec32(volatile uint32_t *p) { return atomic_sub32(p, 1); }

/* Intercambio atómico: pasa *p a "novo" y devuelve el valor anterior. */
static inline uint32_t atomic_troca32(volatile uint32_t *p, uint32_t novo)
{
    uint32_t r = novo;
    __asm__ volatile("lock; xchgl %0, %1" : "+r"(r), "+m"(*p) : : "cc", "memory");
    return r;
}

/* Las operaciones de 64 bits necesitan cmpxchg8b (Pentium en adelante) y que
 * la dirección esté alineada a 8 bytes. atomicas.c es quien lo comprueba:
 * si la CPU no lo tiene, atomic_cas64() devuelve 0 y deja el valor como
 * estaba, y atomic_add64() devuelve 0 sin tocar nada. */
int     atomic_cas64(volatile uint64_t *p, uint64_t esperado, uint64_t novo);
uint64_t atomic_add64(volatile uint64_t *p, uint64_t valor);
bool    atomicas_tem_cas64(void);   /* ¿tiene la CPU el cmpxchg8b? */

#endif
