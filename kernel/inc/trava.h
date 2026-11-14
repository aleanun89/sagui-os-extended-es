// SPDX-License-Identifier: GPL-2.0-or-later
/* trava.h - cerrojo de exclusión mutua del kernel (D095).
 *
 * Hasta D095 el kernel nunca cedía la CPU a mitad de una llamada al sistema:
 * todo corría con las interrupciones desactivadas y, por tanto, sin concurrencia.
 * Ahora una E/S USB larga duerme (cede el turno) a mitad de la operación, y otro
 * proceso puede entrar en el mismo código (el FAT32, el driver EHCI) mientras la
 * primera todavía no ha terminado. El cerrojo serializa esas entradas.
 *
 * Es reentrante (el mismo proceso puede volver a tomarla) y a prueba de dueño
 * muerto: si un proceso es terminado (Ctrl+C, "matar") durmiendo con el cerrojo,
 * quien espera lo toma y llama a la función de recuperación del cerrojo. */
#ifndef SAGUI_TRAVA_H
#define SAGUI_TRAVA_H
#include "tipos.h"

struct processo;

typedef struct trava {
    struct processo *dono;
    int      pid_dono;
    int      profundidade;
    void   (*ao_roubar)(void);   /* limpia lo que dejó a medias el dueño muerto (puede ser NULL) */
} trava_t;

void trava_pegar(trava_t *t);
void trava_soltar(trava_t *t);
bool trava_tentar(trava_t *t);   /* sin esperar: false si otro es el dueño */

#endif
