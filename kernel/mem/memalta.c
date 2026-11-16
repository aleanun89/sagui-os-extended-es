// SPDX-License-Identifier: GPL-3.0-only
/* memalta.c - ventanas temporales a páginas físicas (D348). Ver memalta.h.
 *
 * Al arrancar se reservan CPU_MAX * MEMALTA_NIVEIS páginas de la franja de
 * ventanas del kernel (janela_reservar, D192) y se guarda el puntero a la
 * entrada de la tabla de páginas de cada una. La CPU n solo usa las suyas:
 * las ventanas de n*MEMALTA_NIVEIS en adelante, como una pila.
 *
 * TLB: las entradas no son globales, y la CPU que pone o quita una ventana
 * hace invlpg en las dos ocasiones. Otra CPU no necesita enterarse (no hace
 * falta TLB shootdown): nunca usa esas direcciones, y aunque su TLB guardara
 * una traducción especulativa de ellas, no la usaría. Con las interrupciones
 * apagadas mientras la ventana está puesta, ni una IRQ en esta CPU ni el
 * planificador pueden pisarla. */
#include "memalta.h"
#include "memoria.h"
#include "paginacao.h"
#include "cpu.h"
#include "cpu_dados.h"
#include "console.h"
#include "texto.h"

#define TOTAL_JANELAS (CPU_MAX * MEMALTA_NIVEIS)

static uint32_t base_virt;                 /* 0: sin iniciar */
static void    *entrada[TOTAL_JANELAS];      /* 4 u 8 bytes según el modo (D349) */
static uint32_t usos;

typedef struct {
    uint32_t nivel;                        /* ventanas puestas en esta CPU */
    bool     irq[MEMALTA_NIVEIS];          /* si las interrupciones estaban encendidas al poner cada una */
} ALINHADO(CPU_LINHA_CACHE) estado_cpu_t;

static estado_cpu_t por_cpu[CPU_MAX];

SEM_RETORNO static void falha(const char *o_que, uint64_t valor)
{
    kprintf("memalta: %s (%x%08x)\n", o_que, (uint32_t)(valor >> 32), (uint32_t)valor);
    klog("[SAGUI] ERRO memalta: %s (%x%08x)\n", o_que, (uint32_t)(valor >> 32), (uint32_t)valor);
    parar_maquina();
}

void memalta_iniciar(void)
{
    uint32_t v = janela_reservar(TOTAL_JANELAS * PAGINA);
    if (!v) falha("sin sitio en la franja de ventanas", TOTAL_JANELAS);
    for (uint32_t k = 0; k < TOTAL_JANELAS; k++) {
        entrada[k] = janela_entrada(v + k * PAGINA);
        if (!entrada[k]) falha("ventana sin tabla de páginas", v + k * PAGINA);
        pte_escrever(entrada[k], 0, 0);
    }
    base_virt = v;
    klog("[SAGUI] memalta: %u janelas temporais em %x (%u por CPU)\n", TOTAL_JANELAS, v, MEMALTA_NIVEIS);
}

void *fisica_mapear(uint64_t fis)
{
    if (fis < paginacao_fim_mapa())
        return (void *)(uint32_t)fis;   /* baja: el mapeo 1:1 */
    if (!base_virt) falha("pagina alta antes de memalta_iniciar", fis);
    bool irq = int_salva_desliga();
    uint32_t cpu = cpu_atual();
    if (cpu >= CPU_MAX) falha("CPU fuera de la tabla", cpu);
    estado_cpu_t *c = &por_cpu[cpu];
    if (c->nivel >= MEMALTA_NIVEIS) falha("demasiadas ventanas anidadas", fis);
    uint32_t k = cpu * MEMALTA_NIVEIS + c->nivel;
    c->irq[c->nivel++] = irq;
    uint32_t virt = base_virt + k * PAGINA;
    pte_escrever(entrada[k], fis & ~(uint64_t)(PAGINA - 1), PG_PRESENTE | PG_ESCRITA | PG_NX);
    pagina_invalidar(virt);
    usos++;
    return (void *)(virt | ((uint32_t)fis & (PAGINA - 1)));
}

void fisica_soltar(void *p)
{
    uint32_t virt = (uint32_t)p & ~(PAGINA - 1);
    if (!base_virt || virt < base_virt || virt >= base_virt + TOTAL_JANELAS * PAGINA)
        return;   /* era baja: nada que quitar */
    uint32_t cpu = cpu_atual();
    estado_cpu_t *c = &por_cpu[cpu];
    uint32_t k = (virt - base_virt) / PAGINA;
    /* Se sueltan en orden inverso, y cada CPU solo las suyas. */
    if (c->nivel == 0 || k != cpu * MEMALTA_NIVEIS + c->nivel - 1)
        falha("ventana soltada fuera de orden o de otra CPU", virt);
    c->nivel--;
    pte_escrever(entrada[k], 0, 0);
    pagina_invalidar(virt);
    int_restaura(c->irq[c->nivel]);
}

void fisica_zerar(uint64_t fis)
{
    void *p = fisica_mapear(fis & ~(uint64_t)(PAGINA - 1));
    memset(p, 0, PAGINA);
    fisica_soltar(p);
}

void fisica_escrever(uint64_t fis, const void *origem, uint32_t n)
{
    if (!n) return;
    if ((fis & (PAGINA - 1)) + n > PAGINA) falha("escritura que cruza la pagina", fis);
    void *p = fisica_mapear(fis);
    memcpy(p, origem, n);
    fisica_soltar(p);
}

void fisica_ler(uint64_t fis, void *destino, uint32_t n)
{
    if (!n) return;
    if ((fis & (PAGINA - 1)) + n > PAGINA) falha("lectura que cruza la pagina", fis);
    void *p = fisica_mapear(fis);
    memcpy(destino, p, n);
    fisica_soltar(p);
}

void fisica_copiar_pagina(uint64_t destino, uint64_t origem)
{
    void *o = fisica_mapear(origem & ~(uint64_t)(PAGINA - 1));
    void *d = fisica_mapear(destino & ~(uint64_t)(PAGINA - 1));
    memcpy(d, o, PAGINA);
    fisica_soltar(d);
    fisica_soltar(o);
}

uint32_t memalta_usos(void) { return usos; }
