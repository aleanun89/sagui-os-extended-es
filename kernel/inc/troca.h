// SPDX-License-Identifier: GPL-3.0-only
/* troca.h - swap comprimida en RAM (D352, mejora 125 fase 3b, mejora 069 fase 3).
 *
 * Cuando la memoria libre de los programas baja de un umbral, o una página de
 * programa no se puede asignar, se sacan páginas de programas (las propias de cada
 * proceso: datos, montón, pila, código; nunca las prestadas: píxeles de ventanas
 * y BAR) a un almacén en la RAM alta o superior (en la baja si no hay alta),
 * comprimidas con compressor.c. La entrada de la tabla de páginas queda «no
 * presente» con la dirección del trozo comprimido; el fallo de página (del programa
 * o del kernel en una llamada al sistema) la vuelve a traer sin dormir.
 *
 * Formato de una entrada de swap (solo la mitad baja; con PAE la alta queda a 0):
 *   bit 0       P = 0 (el hardware no mira nada más)
 *   bit 1       TROCA_MARCA: es una entrada de swap (las ausentes de siempre son 0)
 *   bit 2       la página era escribible
 *   bit 3       la página no era ejecutable (NX)
 *   bit 4       página a cero: no ocupa nada en el almacén
 *   bits 5-10   primer bloque de 64 bytes del trozo dentro de su página del almacén
 *   bits 11-31  índice de la página del almacén (hasta 2^21) */
#ifndef SAGUI_TROCA_H
#define SAGUI_TROCA_H
#include "tipos.h"
#include "paginacao.h"

#define TROCA_MARCA    0x002u
#define TROCA_ESCRITA  0x004u
#define TROCA_NX       0x008u
#define TROCA_ZERO     0x010u
#define TROCA_DESL_POS 5
#define TROCA_DESL_IDX 11

static inline bool troca_e_entrada(pte_t v)
{
    return (v & (PG_PRESENTE | TROCA_MARCA)) == TROCA_MARCA;
}

/* Resultado de troca_trazer(). */
#define TROCA_TRAZIDA     0   /* estaba fuera y ya está en su sitio */
#define TROCA_PRESENTE    1   /* ya estaba presente (otra CPU la trajo antes): repetir */
#define TROCA_NAO         2   /* no es una entrada de swap */
#define TROCA_SEM_MEMORIA 3   /* no hay página donde descomprimirla */
#define TROCA_CORROMPIDA  4   /* el trozo no se descomprime o su suma no coincide */

typedef struct {
    uint32_t teto_paginas;      /* tope del almacén (0 = sin swap) */
    uint32_t armazem_paginas;   /* páginas físicas del almacén */
    uint32_t armazem_altas;     /* de ellas, en la alta o la superior */
    uint32_t paginas;           /* páginas de programas en la swap */
    uint32_t zeros;             /* de ellas, a cero (no ocupan nada) */
    uint32_t blocos;            /* bloques de 64 bytes ocupados en el almacén */
    uint32_t bytes;             /* bytes que ocupan en el almacén (bloques * 64) */
    uint32_t saidas, entradas;  /* páginas sacadas y traídas desde el arranque */
    uint32_t rejeitadas;        /* intentos que no comprimieron lo bastante */
    uint32_t do_kernel;         /* traídas por un fallo del kernel (llamada al sistema) */
    uint32_t sem_memoria, corrompidas;
} troca_est_t;

void     troca_iniciar(void);              /* después de memfisica_iniciar() y memalta_iniciar() */
void     troca_tarefa_iniciar(void);       /* la tarea que mantiene memoria libre (con planificador) */
bool     troca_ativa(void);
/* Saca páginas de programas hasta liberar "quer" páginas (netas: lo que ocupa el almacén
 * se descuenta). No duerme; devuelve las liberadas. Con el cerrojo general. */
uint32_t troca_liberar(uint32_t quer);
/* Trae la página "virt" de "e" si está en la swap. do_kernel: la pide el kernel (un fallo
 * en modo kernel o espaco_traduzir), que puede usar la memoria baja de reserva. */
int      troca_trazer(espaco_t e, uint32_t virt, bool do_kernel);
/* Libera el trozo de una entrada de swap que desaparece (espaco_destruir, espaco_tirar). */
void     troca_soltar(pte_t v);
void     troca_estatisticas(troca_est_t *e);
/* Saca todo lo presente de [ini, fim) de "e" (MEM_SACAR y la autoprueba); devuelve cuántas
 * páginas del rango están en la swap al terminar. */
uint32_t troca_sacar_faixa(espaco_t e, uint32_t ini, uint32_t fim);

#endif
