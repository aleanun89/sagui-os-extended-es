// SPDX-License-Identifier: GPL-2.0-or-later
/* smp.h - o arranque dos processadores secundários (D157).
 *
 * O processador principal (o BSP) lê a MADT do ACPI, onde o firmware
 * enumera os processadores e os seus identificadores de APIC local, e
 * acende cada um deles com a sequência INIT-SIPI: manda o INIT (que
 * reinicia o processador e o deixa à espera da SIPI) e depois a SIPI (que
 * o põe a correr em modo real, na página do trampolín).
 *
 * O trampolín (kernel/arch/ap_trampolim.asm) é o código que o processador
 * secundário executa: passa a modo protegido, carrega a GDT e a IDT,
 * liga a paginação e entra no kernel. */
#ifndef SAGUI_SMP_H
#define SAGUI_SMP_H
#include "tipos.h"

/* Onde se copia o trampolín: uma página abaixo de 1 MB, livre depois do
 * carregador (0x7000). O vector da SIPI é o número da página. */
#define SMP_TRAMPOLIM 0x7000u
#define SMP_VETOR_SIPI 7u

/* Liga os processadores secundários (chamado pelo principal, depois da
 * paginação e do relógio). Devolve quantos ficaram a trabalhar. */
uint32_t smp_iniciar(void);

/* Despierta esa CPU con una IPI para que planifique (D164). */
void smp_acordar(uint32_t cpu);

/* Quantos processadores secundários arrancaram de facto. */
uint32_t smp_arrancadas(void);

/* D450: al apagar o reiniciar, desde la CPU 0 con el cerrojo general y las interrupciones
 * cerradas: manda la IPI VETOR_PARAR a los demás y espera como mucho "ms" a que se paren.
 * Devuelve cuántos se pararon. Los que no contestan (dentro del kernel, esperando el
 * cerrojo) se quedan así: no pueden hacer nada sin él. */
uint32_t smp_parar_secundarias(uint32_t ms);
SEM_RETORNO void smp_parar_esta_cpu(void);   /* la IPI, en el secundario (idt.c) */
/* D450: puesto por smp_parar_secundarias. Quien espera el cerrojo general (con las interrupciones
 * cerradas, así que no ve la IPI) lo mira y se para también (trava_geral.c). */
extern volatile bool smp_parando;

#endif
