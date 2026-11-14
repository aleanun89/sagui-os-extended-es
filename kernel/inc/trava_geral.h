// SPDX-License-Identifier: GPL-2.0-or-later
/* trava_geral.h - el cerrojo general del kernel (D165).
 *
 * Un solo dueño del kernel a la vez: la CPU que lo tiene. Se toma al entrar
 * en el kernel (llamada al sistema, excepción, IRQ, IPI) desde modo usuario o
 * desde la espera ociosa, y se suelta al volver a modo usuario o al "hlt" de
 * cpu_espera(). Es de la CPU, no del proceso: un proceso que se bloquea
 * dentro del kernel deja el cerrojo a la CPU, y la CPU que lo retoma ya lo
 * tiene (lo tomó al entrar). Ver la D165 en docs/decisoes.md. */
#ifndef SAGUI_TRAVA_GERAL_H
#define SAGUI_TRAVA_GERAL_H
#include "tipos.h"
#include "cpu.h"

/* Toma el cerrojo esperando con las interrupciones como estén (en la entrada
 * del kernel, apagadas). Si ya es de esta CPU, no hace nada: una IRQ que
 * llega en medio de código del kernel de esta CPU entra sin esperar. */
void trava_geral_pegar(void);

/* Lo toma solo si está libre y nadie espera (o ya es de esta CPU). */
bool trava_geral_tentar(void);

/* D169: por turnos (justo) o sin cola (bajo un hipervisor, salvo
 * forcar_turnos). Se llama con las CPU secundarias ya arrancadas. */
void trava_geral_escolher(bool forcar_turnos);
bool trava_geral_por_turnos(void);

/* Lo suelta si es de esta CPU. */
void trava_geral_soltar(void);

/* ¿Es de esta CPU? */
bool trava_geral_minha(void);

/* Cuántas veces una CPU tuvo que esperar al cerrojo (para la D165). */
uint32_t trava_geral_esperas(void);

/* Lo que esperó esa CPU: cuántas veces y cuántos ciclos del TSC (D169). */
void trava_geral_medida(uint32_t cpu, uint32_t *esperas, uint64_t *ciclos);
#ifdef TRAVA_MEDIR
void trava_geral_relatorio(uint32_t agora);   /* el tic de la CPU 0, cada segundo */
#endif

/* El código de entrada lo llama al salir del kernel, ya sobre la pila del
 * marco que se reanuda: suelta el cerrojo si ese marco no lo tenía (modo
 * usuario o la espera de cpu_espera) y manda las IPI pendientes. */
void kernel_saida(quadro_t *q);

/* vetores.asm: "sti; hlt; cli" sin el cerrojo. Las dos direcciones son las
 * que puede tener el EIP de un marco interrumpido ahí. */
void espera_sem_trava(void);
extern char ponto_hlt[];
extern char ponto_depois_hlt[];

#endif
