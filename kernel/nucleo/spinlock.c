// SPDX-License-Identifier: GPL-2.0-or-later
/* spinlock.c - cerrojos de espera activa del kernel (D150).
 *
 * El cerrojo es un número: 0 = libre, 1 = tomado. Se toma con un
 * cmpxchg (comparar y cambiar) atómico, de las primitivas de atomicas.h.
 *
 * La espera no se queda para siempre bloqueada: cada cierto número de
 * intentos, si las interrupciones están puestas, el núcleo se apaga con hlt
 * y espera a la próxima interrupción (el reloj del PIT). Así, en un solo
 * procesador, quien espera no monopoliza la máquina: el dueño (otro
 * proceso) tiene su turno.
 *
 * Con las interrupciones apagadas no se puede usar hlt (se quedaría
 * colgado sin reloj que lo despertara), así que en ese caso se sigue
 * probando a ritmo de CPU. Por eso los cerrojos de espera activa no se
 * pueden tomar con las interrupciones apagadas si el dueño puede estar en
 * el mismo núcleo. */
#include "spinlock.h"
#include "atomicas.h"
#include "cpu.h"
#include "cpu_dados.h"

/* Cuántos intentos se hacen antes de ceder el turno con hlt. */
#define SPIN_ANTES_DE_DORMIR 1024

/* ¿Están puestas las interrupciones? Se lee el EFLAGS (bit 9, IF). Con las
 * apagadas, hlt se quedaría colgado: no hay interrupción que lo despierte. */
static bool interrupcoes_ativas(void)
{
    uint32_t f;
    __asm__ volatile("pushf; pop %0" : "=r"(f));
    return (f & 0x200u) != 0;
}

/* El cerrojo sabe quien lo tiene: el estado guarda el numero de la CPU
 * dona (mas uno, para que cero signifique "libre"). Asi una interrupcion
 * que pilla al propio codigo con el cerrojo tomado no se queda esperando
 * por el mismo: es el caso del reloj a mitad de una troca de processo (D157). */
void spin_lock(spinlock_t *s)
{
    uint32_t cpu = cpu_atual() + 1;
    unsigned intentos = 0;
    for (;;) {
        /* El cmpxchg lleva lock: es atómico y hace de barrera de memoria,
         * así que quien toma el cerrojo ve todo lo que escribió quien lo
         * soltó. No hace falta otra barrera. */
        if (atomic_cas32(&s->estado, 0, cpu)) { s->fundo = 1; return; }
        /* Ya es nuestro? Entonces estamos dentro de una interrupcion nuestra:
         * el codigo interrumpido es el dono, y no se espera por el. */
        if (s->estado == cpu) { s->fundo++; return; }
        /* Barrera sólo para el compilador: el cmpxchg con lock ya es una
         * barrera de memoria completa, y no hace falta tocar ninguna otra
         * línea de caché (con varios núcleos, eso sólo daría trabajo). */
        __asm__ volatile("" ::: "memory");
        if (++intentos >= SPIN_ANTES_DE_DORMIR) {
            intentos = 0;
            if (interrupcoes_ativas()) cpu_espera();   /* hlt: hasta la IRQ */
        }
    }
}

bool spin_trylock(spinlock_t *s)
{
    return atomic_cas32(&s->estado, 0, cpu_atual() + 1) != 0;
}

void spin_unlock(spinlock_t *s)
{
    /* Si todavia estamos dentro (interrupcion dentro del cerrojo), solo se
     * baja un escalon: el cerrojo sigue tomado. */
    if (s->fundo > 1) { s->fundo--; return; }
    s->fundo = 0;
    /* El intercambio con lock es una barrera completa: todo lo que se
     * escribió bajo el cerrojo queda visible antes de soltarlo, y el
     * cerrojo queda libre en la misma operación. */
    atomic_troca32(&s->estado, 0);
}

void spin_unlock_tudo(spinlock_t *s)
{
    s->fundo = 0;
    atomic_troca32(&s->estado, 0);
}
