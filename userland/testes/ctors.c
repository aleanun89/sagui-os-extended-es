// SPDX-License-Identifier: GPL-2.0-or-later
/* ctors.c - prueba de la mejora 094, puntos 9 y 10 (h160).
 *
 *   - Constructores globales: dos funciones con __attribute__((constructor))
 *     (una con prioridad) deben ejecutarse antes de principal, una sola vez
 *     cada una y en orden de prioridad.
 *   - División de 64 bits: el gcc llama a __udivdi3, __umoddi3, __divdi3 y
 *     __moddi3, que ahora da la libsagui (débiles) para todos los programas.
 * Escribe «RESULTADO ctors: SUCESSO (0 falhas)» o con el número de fallos. */
#include "sagui.h"

static volatile int orden[4], n_llamadas;

__attribute__((constructor(200))) static void primero(void) { orden[n_llamadas++] = 1; }
__attribute__((constructor)) static void despues(void) { orden[n_llamadas++] = 2; }

static int fallos;
static void comprobar(int ok, const char *que)
{
    imprimir("CTORS %s: %s\n", ok ? "ok" : "[FALHA]", que);
    if (!ok) fallos++;
}

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    comprobar(n_llamadas == 2, "los dos constructores se llamaron una vez antes de principal");
    comprobar(orden[0] == 1 && orden[1] == 2, "en orden de prioridad (200 antes que el sin prioridad)");

    /* volatile: que el gcc no pueda calcularlo al compilar */
    volatile unsigned long long a = 0x0123456789ABCDEFULL, b = 1000003ULL, c = 0x100000001ULL;
    volatile long long n = -9000000000000000000LL, m = 7;
    comprobar(a / b == 81985283260ULL && a % b == 637115ULL, "uint64 / y % con divisor de 32 bits");
    comprobar(a / c == 19088743ULL && a % c == 2290649224ULL, "uint64 / y % con divisor de 64 bits");
    comprobar(n / m == -1285714285714285714LL && n % m == -2LL, "int64 / y % con signo");

    imprimir("RESULTADO ctors: %s (%d falhas)\n", fallos ? "FALHA" : "SUCESSO", fallos);
    return fallos ? 1 : 0;
}
