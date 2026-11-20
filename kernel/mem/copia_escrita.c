// SPDX-License-Identifier: GPL-3.0-only
/* copia_escrita.c - la cuenta de referencias de las páginas compartidas por el fork (D401).
 *
 * Una tabla hash de direccionamiento abierto (sondeo lineal) de pares (número de página física,
 * referencias). La capacidad es una potencia de dos y nunca pasa de la mitad llena; se borra
 * desplazando hacia atrás las entradas siguientes del mismo grupo, sin lápidas. La clave 0 es
 * «vacío»: la página física 0 nunca es de un programa.
 *
 * Por qué dispersa (D401): con PAE hay hasta 16 M páginas físicas; un contador por página serían
 * megas de memoria baja para nada, y solo una parte pequeña de las páginas se comparte (las de un
 * hijo recién nacido, hasta que escribe o hace exec). La tabla solo crece en cow_reservar(), que
 * se llama antes de cambiar nada en un fork. */
#include "copia_escrita.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "texto.h"

typedef struct {
    uint32_t pfn;    /* 0 = vacío */
    uint32_t refs;   /* >= 2 */
} cow_ent_t;

#define CAP_MIN 4096u

static cow_ent_t *tabela;
static uint32_t   cap;      /* potencia de dos (0 = sin tabla) */
static uint32_t   usadas;

static inline uint32_t hash(uint32_t pfn) { return (pfn * 2654435761u) & (cap - 1); }

/* Posición de "pfn" o, si no está, la del hueco donde iría. */
static uint32_t achar(uint32_t pfn)
{
    uint32_t i = hash(pfn);
    while (tabela[i].pfn && tabela[i].pfn != pfn) i = (i + 1) & (cap - 1);
    return i;
}

static bool crescer(uint32_t nova_cap)
{
    cow_ent_t *nova = kcalloc(nova_cap * sizeof(cow_ent_t));
    if (!nova) return false;
    cow_ent_t *velha = tabela;
    uint32_t velha_cap = cap;
    tabela = nova;
    cap = nova_cap;
    for (uint32_t i = 0; i < velha_cap; i++)
        if (velha[i].pfn) tabela[achar(velha[i].pfn)] = velha[i];
    if (velha) kfree(velha);
    return true;
}

bool cow_reservar(uint32_t mais)
{
    bool irq = int_salva_desliga();
    bool ok = true;
    uint64_t quer = (uint64_t)usadas + mais;
    if (quer * 2 > cap) {
        uint64_t c = cap ? cap : CAP_MIN;
        while (quer * 2 > c) c *= 2;
        ok = c <= 0x10000000ull && crescer((uint32_t)c);
        if (!ok) klog("[COW] sin memoria para la tabla de referencias (%u paginas mas)\n", mais);
    }
    int_restaura(irq);
    return ok;
}

void cow_somar(uint64_t fis)
{
    uint32_t pfn = (uint32_t)(fis >> 12);
    bool irq = int_salva_desliga();
    if (!cap || (usadas + 1) * 2 > cap) {
        /* No debería pasar (cow_reservar antes): se intenta crecer aquí. */
        if (!crescer(cap ? cap * 2 : CAP_MIN)) {
            klog("[COW] ERRO: tabla llena sin reserva (pagina %x)\n", pfn);
            int_restaura(irq);
            return;
        }
    }
    uint32_t i = achar(pfn);
    if (tabela[i].pfn) tabela[i].refs++;
    else { tabela[i].pfn = pfn; tabela[i].refs = 2; usadas++; }
    int_restaura(irq);
}

/* Borra la posición i desplazando hacia atrás las siguientes del grupo que puedan ocuparla. */
static void apagar(uint32_t i)
{
    uint32_t j = i;
    for (;;) {
        tabela[i].pfn = 0;
        tabela[i].refs = 0;
        for (;;) {
            j = (j + 1) & (cap - 1);
            if (!tabela[j].pfn) return;
            uint32_t k = hash(tabela[j].pfn);
            /* ¿k está fuera del tramo cíclico (i, j]? Entonces la entrada j puede ir a i. */
            if (i <= j ? (i < k && k <= j) : (i < k || k <= j)) continue;
            break;
        }
        tabela[i] = tabela[j];
        i = j;
    }
}

bool cow_soltar(uint64_t fis)
{
    if (!usadas) return false;   /* atajo: nada compartido */
    uint32_t pfn = (uint32_t)(fis >> 12);
    bool irq = int_salva_desliga();
    uint32_t i = achar(pfn);
    bool estava = tabela[i].pfn == pfn;
    if (estava) {
        if (--tabela[i].refs < 2) { apagar(i); usadas--; }
    }
    int_restaura(irq);
    return estava;
}

uint32_t cow_refs(uint64_t fis)
{
    if (!usadas) return 1;
    uint32_t pfn = (uint32_t)(fis >> 12);
    bool irq = int_salva_desliga();
    uint32_t i = achar(pfn);
    uint32_t r = tabela[i].pfn == pfn ? tabela[i].refs : 1;
    int_restaura(irq);
    return r;
}

uint32_t cow_paginas(void) { return usadas; }
uint32_t cow_capacidade(void) { return cap; }
