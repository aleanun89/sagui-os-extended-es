// SPDX-License-Identifier: GPL-2.0-or-later
/* trava.c - exclusión mutua entre procesos y tareas del kernel (D095).
 *
 * Quien no consigue el bloqueo se duerme en su dirección y es despertado por
 * quien lo suelta; un plazo de un tic garantiza que, aunque el aviso se pierda
 * (dueño terminado por la fuerza), la espera vuelva a comprobar al dueño. El proceso 0
 * (ocioso) nunca queda fuera del planificador, así que sólo cede el turno.
 *
 * Dentro de una IRQ (por ejemplo, Ctrl+C terminando un proceso que tiene
 * archivos abiertos) no se espera: se sigue sin el bloqueo.
 *
 * Antes de que exista el planificador ("atual" nulo) nadie compite: el bloqueo
 * no hace nada. */
#include "trava.h"
#include "cpu.h"
#include "processo.h"
#include "relogio.h"

static bool dono_morto(const trava_t *t)
{
    processo_t *p = processo_por_pid(t->pid_dono);
    return !p || p != t->dono || p->estado == P_ZUMBI;
}

void trava_pegar(trava_t *t)
{
    if (!processo_atual()) return;
    for (;;) {
        if (t->profundidade == 0) break;
        /* Reentrada: el mismo proceso, no solo la misma entrada de la tabla.
         * Un dueño que murió con el bloqueo deja su entrada libre, y el
         * siguiente proceso que la ocupa tiene el mismo puntero pero otro pid:
         * para él el bloqueo es de un muerto (si no, sumaba profundidad a un
         * bloqueo ajeno y los demás lo robaban mientras lo usaba). */
        if (t->dono == processo_atual() && t->pid_dono == processo_atual()->pid) break;
        if (dono_morto(t)) {
            t->profundidade = 0;
            if (t->ao_roubar) t->ao_roubar();
            break;
        }
        /* Dentro de una IRQ ningún otro proceso corre: esperar sería trabar
         * la máquina. Se sigue sin el bloqueo, como antes de que existiera (D095). */
        if (contexto_irq()) return;
        /* Quien espera cede el turno con las interrupciones como estaban: el "int"
         * de ceder funciona incluso con IF apagado. */
        if (processo_atual()->pid != 0) {
            bloquear(t);
            processo_atual()->acordar_em = relogio_tiques() + 1;
            ceder();
            processo_atual()->acordar_em = 0;
        } else {
            /* D379: el ocioso (proceso 0) no puede dormir y suele llegar aquí con las
             * interrupciones apagadas (carga un programa: con la raíz en disco, del ext2).
             * Si el dueño del bloqueo duerme esperando la IRQ de su DMA, ceder sin
             * interrupciones vuelve siempre al ocioso: ni la IRQ ni el reloj llegan y la
             * máquina se queda girando (visto en h387: shell sin prompt, ~1 de cada 4
             * arranques). Se espera un tic con las interrupciones encendidas. */
            bool ints = int_salva_desliga();
            int_liga();
            cpu_espera();
            if (!ints) int_desliga();
        }
    }
    if (t->profundidade++ == 0) {
        t->dono = processo_atual();
        t->pid_dono = processo_atual()->pid;
    }
}

void trava_soltar(trava_t *t)
{
    if (!processo_atual() || t->dono != processo_atual() || t->pid_dono != processo_atual()->pid ||
        t->profundidade == 0) return;   /* nos lo quitaron: nada que soltar */
    if (--t->profundidade == 0) {
        t->dono = NULL;
        acordar(t);
    }
}

/* Toma el bloqueo sólo si está libre (o ya es nuestro). */
bool trava_tentar(trava_t *t)
{
    if (!processo_atual()) return true;
    if (t->profundidade != 0 && !(t->dono == processo_atual() && t->pid_dono == processo_atual()->pid) && !dono_morto(t))
        return false;
    trava_pegar(t);
    return true;
}
