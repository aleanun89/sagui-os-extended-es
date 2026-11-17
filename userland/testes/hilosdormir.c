// SPDX-License-Identifier: GPL-3.0-only
/* hilosdormir - prueba de htop (D366, H368): un proceso con dos hilos que duermen hasta que lo matan. */
#include "sagui.h"

static void *dormilon(void *arg)
{
    (void)arg;
    for (;;) dormir(500);
    return NULL;
}

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    thread_t *a = thread_create(dormilon, NULL, 0);
    thread_t *b = thread_create(dormilon, NULL, 0);
    registrar("hilosdormir: comecando (pid %d, hilos %d y %d)", meu_pid(), a ? thread_tid(a) : -1, b ? thread_tid(b) : -1);
    for (;;) dormir(500);
    return 0;
}
