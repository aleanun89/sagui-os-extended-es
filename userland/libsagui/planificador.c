// SPDX-License-Identifier: GPL-2.0-or-later
/* planificador.c - los planificadores de procesos (D174): elegir el activo,
 * la máscara de CPU y el nice de un proceso, y lo que el kernel sabe de él.
 * Todo va por una sola llamada, SIS_PLANIFICADOR, con la operación delante. */
#include "sagui.h"

static inline int chamar4(unsigned op, unsigned a, unsigned b, unsigned c)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SIS_PLANIFICADOR), "b"(op), "c"(a), "d"(b), "S"(c) : "memory");
    return r;
}

int planificador_consultar(char *nome, unsigned n) { return chamar4(PLAN_CONSULTAR, (unsigned)nome, n, 0); }
int planificador_escolher(int numero)              { return chamar4(PLAN_ESCOLHER, (unsigned)numero, 0, 0); }
int planificador_nome(int numero, char *nome, unsigned n)
{
    return chamar4(PLAN_NOME, (unsigned)numero, (unsigned)nome, n);
}

int planificador_numero(const char *nome)
{
    char t[24];
    for (int i = 1; i <= PLANIFICADOR_MAX; i++)
        if (planificador_nome(i, t, sizeof t) == 0 && compara_texto(t, nome) == 0) return i;
    return -1;
}

int afinidade_cpu(int pid, unsigned mascara)    { return chamar4(PLAN_AFINIDADE, (unsigned)pid, mascara, 0); }
int afinidade_consultar(int pid)                { return chamar4(PLAN_AFINIDADE, (unsigned)pid, 0xFFFFFFFFu, 0); }
int prioridade_processo(int pid, int nice)      { return chamar4(PLAN_PRIORIDADE, (unsigned)pid, (unsigned)nice, 0); }
int planificacion_info(int pid, info_planificacion_t *v)
{
    return chamar4(PLAN_INFO, (unsigned)pid, (unsigned)v, 0);
}
