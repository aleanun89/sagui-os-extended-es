// SPDX-License-Identifier: GPL-2.0-or-later
/* prazo.h - esperas con límite de tiempo real (D199, D207).
 *
 * Las esperas de los drivers de disco no pueden contar vueltas: en el QEMU una
 * vuelta no dura nada y en un PC real la misma cuenta se vuelve de decenas de
 * segundos. Se usa el TSC, medido al arrancar (hw_tsc_khz); sin medida, el
 * puerto 0x80 (~1 us por escritura en un PC real). Vale con las interrupciones
 * apagadas, porque no depende del reloj del sistema. Es la misma lógica que
 * tenía ahci.c (D199), puesta aquí para el ATAPI (D207). */
#ifndef SAGUI_PRAZO_H
#define SAGUI_PRAZO_H
#include "tipos.h"
#include "portas.h"
#include "info_hardware.h"

typedef struct { uint64_t fim; uint32_t voltas; } prazo_t;

static inline uint64_t prazo_tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline prazo_t prazo_ms(uint32_t ms)
{
    prazo_t p = { 0, 0 };
    uint32_t khz = hw_tsc_khz();
    if (khz) p.fim = prazo_tsc() + (uint64_t)khz * ms;
    else p.voltas = ms * 1000u;
    return p;
}

/* true cuando el plazo ha vencido; si no, hace una pausa corta (puerto 0x80). */
static inline bool prazo_vencido(prazo_t *p)
{
    porta_pausa();
    if (p->fim) return prazo_tsc() >= p->fim;
    if (!p->voltas) return true;
    p->voltas--;
    return false;
}

#endif
