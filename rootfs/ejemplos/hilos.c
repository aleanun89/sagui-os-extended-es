// SPDX-License-Identifier: GPL-2.0-or-later
/* hilos.c - la misma cuenta en varios hilos (los hilos del sistema, D167).
 * Compila y ejecuta con:
 *     cc /ejemplos/hilos.c -o /tmp/hilos
 *     /tmp/hilos 4
 * Con varios procesadores (arranque con --smp) los hilos corren a la vez y
 * el tiempo con 4 hilos se parece al de 1; con uno solo, se suman.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sagui.h>          /* thread_create, thread_join, mutex_t... */

#define MAX_HILOS 8

static mutex_t cerrojo;
static long total;

static long cuenta(long hasta)
{
    long s = 0;
    for (long i = 0; i < hasta; i++) s += i % 7;
    return s;
}

static void *hilo(void *arg)
{
    long n = (long)arg;
    long r = cuenta(20000000);
    mutex_tomar(&cerrojo);          /* el total es de todos: con el cerrojo */
    total += r;
    printf("hilo %ld: %ld\n", n, r);
    mutex_largar(&cerrojo);
    return (void *)n;
}

int main(int argc, char **argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 4;
    if (n < 1) n = 1;
    if (n > MAX_HILOS) n = MAX_HILOS;
    thread_t *h[MAX_HILOS];
    mutex_criar(&cerrojo);
    int t0 = time_ms();
    for (int i = 0; i < n; i++) h[i] = thread_create(hilo, (void *)(long)i, 0);
    for (int i = 0; i < n; i++) thread_join(h[i]);
    printf("total %ld con %d hilos en %d ms\n", total, n, time_ms() - t0);
    return 0;
}
