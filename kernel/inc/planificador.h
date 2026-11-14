// SPDX-License-Identifier: GPL-2.0-or-later
/* planificador.h - los planificadores de procesos entre las CPU (D174).
 *
 * El mecanismo (cambiar de cuadro, de CR3, de FPU; na_cpu; los zombis) está
 * en processos.c (escalonar). Aquí solo está la POLÍTICA: qué proceso toma
 * cada CPU, cuánto le dura la rebanada, a qué CPU se despierta, el reparto
 * periódico y las prioridades. Se puede cambiar en caliente: todo corre
 * con el cerrojo general (D165) salvo lo indicado. */
#ifndef SAGUI_PLANIFICADOR_H
#define SAGUI_PLANIFICADOR_H
#include "processo.h"

void        planificador_iniciar(uint32_t pedido);      /* el de las opciones de arranque (0 = por defecto) */
uint32_t    planificador_ativo(void);                   /* PLANIFICADOR_* */
const char *planificador_nome(uint32_t n);              /* NULL si no existe */
uint32_t    planificador_trocar_para(uint32_t n);       /* en caliente: devuelve el anterior (0 si "n" no existe) */

/* Lo que llama escalonar() (con el cerrojo). */
processo_t *planificador_escolher(uint32_t cpu, processo_t *atual);   /* NULL: la CPU se queda ociosa */
uint32_t    planificador_fatia(const processo_t *p);    /* tiques de la rebanada */
void        planificador_tomou(processo_t *p, uint32_t cpu);   /* esta CPU lo tomó: migraciones, latencia */
bool        planificador_pode_cpu(const processo_t *p, uint32_t cpu);   /* máscara y tareas del kernel */

/* Ciclo de vida de un proceso. */
void        planificador_herdar(processo_t *p, int pai);          /* processo_novo: máscara y nice del padre */
void        planificador_pronto_novo(processo_t *p);              /* listo por primera vez: dónde colocarlo */
void        planificador_despertou(processo_t *p);          /* listo de nuevo: ¿quitar la CPU a otro? */
int         planificador_ociosa_para(const processo_t *p, uint32_t eu, uint32_t marcadas);   /* CPU ociosa a despertar, -1 */

/* El tic de la CPU 0 (con el cerrojo): el uso reciente y el reparto. */
void        planificador_tique0(uint32_t agora);

/* "Esta CPU tiene que planificar ya" (una prioridad mayor despertó, o el
 * proceso que corre aquí ya no puede usar esta CPU). Se pide con el cerrojo;
 * el tic de un secundario lo lee sin él (volatile). */
void        planificador_pedir_troca(uint32_t cpu);
bool        planificador_troca_pendente(uint32_t cpu);

#endif
