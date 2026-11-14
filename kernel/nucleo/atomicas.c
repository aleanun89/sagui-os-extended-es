// SPDX-License-Identifier: GPL-2.0-or-later
/* atomicas.c - la parte de 64 bits de las primitivas atómicas (D150).
 *
 * Lo de 32 bits cabe entero en el cabecero (atomicas.h) porque son cuatro
 * instrucciones; lo de 64 bits necesita cmpxchg8b, que no está en todos los
 * procesadores, y por eso va aquí, donde se puede comprobar antes de usar.
 *
 * El cmpxchg8b es del Pentium en adelante: en un 486 (la CPU mínima de este
 * proyecto) no existe y la instrucción daría una excepción. Por eso
 * atomic_cas64() y atomic_add64() miran primero si la CPU lo tiene (bit 8
 * del EDX de CPUID.1) y, si no lo tiene, no tocan nada: devuelven 0 y el
 * llamante sigue como estaba.
 *
 * También hace falta que la dirección esté alineada a 8 bytes: el
 * cmpxchg8b con una dirección desalineada da una excepción de tipo General
 * Protection. Las variables de 64 bits del kernel se alinean solas (o con
 * ALINHADO(8)), pero el comprobante está por si acaso. */
#include "atomicas.h"

volatile uint32_t atomic_barreira;

/* CPUID: hoja 0 para saber hasta qué hoja llega la CPU, hoja 1 para las
 * Characteristics (EDX). Lo mismo que hacen el resto de ficheros del
 * kernel (wc.c, sensores.c, fpu.c). */
static inline void cpuid(uint32_t folha, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(folha), "c"(0));
}

/* 0 = ainda não olhado; 1 = a CPU tem cmpxchg8b; 2 = não tem. */
static int estado_cas64;

bool atomicas_tem_cas64(void)
{
    if (estado_cas64 == 0) {
        uint32_t a, b, c, d;
        cpuid(0, &a, &b, &c, &d);
        if (a >= 1) {
            cpuid(1, &a, &b, &c, &d);
            estado_cas64 = (d & (1u << 8)) ? 1 : 2;   /* EDX bit 8 = CX8 (cmpxchg8b) */
        } else {
            estado_cas64 = 2;
        }
    }
    return estado_cas64 == 1;
}

/* Un entero de 64 bits Partidos en dos mitades de 32: el cmpxchg8b trabaja
 * con EDX:EAX (el valor viejo) y ECX:EBX (el nuevo). En x86 el byte de
 * menos peso va primero. */
typedef union {
    uint64_t total;
    uint32_t metade[2];
} u64_t;

int atomic_cas64(volatile uint64_t *p, uint64_t esperado, uint64_t novo)
{
    if (!atomicas_tem_cas64()) return 0;
    if (((uintptr_t)p & 7u) != 0) return 0;   /* alineación de 8 bytes */
    u64_t velho, novo64;
    velho.total = esperado;
    novo64.total = novo;
    /* Tras la instrucción: si el cambio se hizo, EAX:EDX siguen con el
     * valor esperado; si no, EAX:EDX traen el valor que había en la
     * memoria. Por eso se compara con el esperado. */
    __asm__ volatile("lock; cmpxchg8b %0"
                     : "+m"(*p), "+a"(velho.metade[0]), "+d"(velho.metade[1])
                     : "b"(novo64.metade[0]), "c"(novo64.metade[1])
                     : "cc", "memory");
    return (velho.total == esperado) ? 1 : 0;
}

uint64_t atomic_add64(volatile uint64_t *p, uint64_t valor)
{
    if (!atomicas_tem_cas64()) return 0;
    if (((uintptr_t)p & 7u) != 0) return 0;
    uint64_t antigo = *p;
    for (;;) {
        u64_t velho, novo64;
        velho.total = antigo;
        novo64.total = antigo + valor;
        __asm__ volatile("lock; cmpxchg8b %0"
                         : "+m"(*p), "+a"(velho.metade[0]), "+d"(velho.metade[1])
                         : "b"(novo64.metade[0]), "c"(novo64.metade[1])
                         : "cc", "memory");
        if (velho.total == antigo) return antigo;   /* el cambio se hizo */
        antigo = velho.total;                        /* otro lo cambió: repetir */
    }
}
