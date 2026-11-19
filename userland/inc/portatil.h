// SPDX-License-Identifier: GPL-2.0-or-later
/* portatil.h - SIS_PORTATIL desde los programas (D403): brillo, acciones del
 * sistema, estado de los dispositivos de los portátiles y tablas ACPI.
 * Funciones en línea (sin tocar la libsagui): las usan `brillo` y `portatil`. */
#ifndef SAGUI_U_PORTATIL_H
#define SAGUI_U_PORTATIL_H
#include "abi.h"

static inline int portatil_sis(unsigned op, unsigned a, unsigned b, unsigned c)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SIS_PORTATIL), "b"(op), "c"(a), "d"(b), "S"(c) : "memory");
    return r;
}

static inline int portatil_estado(info_portatil_t *p) { return portatil_sis(PORTATIL_ESTADO, (unsigned)p, sizeof *p, 0); }
static inline int portatil_brilho(int modo, int valor) { return portatil_sis(PORTATIL_BRILHO, (unsigned)modo, (unsigned)valor, 0); }
static inline int portatil_acao(int acao) { return portatil_sis(PORTATIL_ACAO, (unsigned)acao, 0, 0); }
static inline int portatil_notificar(int alvo, int valor) { return portatil_sis(PORTATIL_NOTIFICAR, (unsigned)alvo, (unsigned)valor, 0); }
static inline int portatil_tabela(int i, void *buf, unsigned n) { return portatil_sis(PORTATIL_TABELA, (unsigned)i, (unsigned)buf, n); }

#endif
