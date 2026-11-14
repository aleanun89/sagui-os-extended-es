// SPDX-License-Identifier: GPL-2.0-or-later
/* spinlock.h - cerrojos de espera activa del kernel (D150).
 *
 * Un cerrojo de espera activa no duerme: quien lo quiere y no lo tiene
 * sigue probando hasta que el dueño lo suelta. En un solo procesador eso
 * significa gastar la CPU mientras se espera, así que aquí, cuando las
 * interrupciones están puestas, quien espera apaga el procesador con hlt
 * hasta la próxima interrupción (en un solo núcleo, el dueño está en otro
 * proceso, así que el hlt no lo retrasa).
 *
 * Con varios procesadores, cada núcleo gira en su propio cerrojo, y es lo
 * correcto: el dueño está en otro núcleo y va a soltarlo solo.
 *
 * El cerrojo no es reentrante: tomarlo dos veces desde el mismo sitio se
 * queda esperando para siempre. No se puede tomar en una interrupción si el
 * código interrumpido puede tener el mismo cerrojo (es el problema clásico
 * del cerrojo de espera activa en un manejador de IRQ).
 *
 * Todavía no lo usa nadie: es la base sobre la que se construirá el SMP
 * (D150). */
#ifndef SAGUI_SPINLOCK_H
#define SAGUI_SPINLOCK_H
#include "tipos.h"

typedef struct spinlock {
    volatile uint32_t estado;   /* 0 = libre; si no, la CPU dona (mas uno) */
    volatile uint32_t fundo;    /* quantas veces fue tomado (D157) */
    const char *nome;           /* sólo para el registro: de qué cerrojo se trata */
} spinlock_t;

#define SPINLOCK_INICIAL(nome) { 0, 0, (nome) }

/* Toma el cerrojo y espera (cediendo el turno con hlt) hasta conseguirlo. */
void spin_lock(spinlock_t *s);

/* Lo suelta. */
void spin_unlock(spinlock_t *s);

/* Lo suelta todo (la cuenta de fondo a cero). */
void spin_unlock_tudo(spinlock_t *s);

/* Lo toma sólo si está libre: devuelve true si lo consiguió, false si otro
 * lo tenía (en ese caso no espera). */
bool spin_trylock(spinlock_t *s);

#endif
