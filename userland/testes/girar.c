// SPDX-License-Identifier: GPL-2.0-or-later
/* girar [segundos] - teste do Administrador de tareas (D043): ocupa a CPU
 * sem dormir, para o uso de CPU subir. Por padrão gira até ser encerrado. */
#include "sagui.h"

int principal(int argc, char **argv)
{
    unsigned seg = argc > 1 ? (unsigned)texto_para_int(argv[1]) : 0;
    unsigned ini = tempo_ms();
    registrar("girar: comecando (pid %d)", meu_pid());
    volatile unsigned voltas = 0;
    while (!seg || tempo_ms() - ini < seg * 1000) {
        for (int k = 0; k < 100000; k++) voltas++;
    }
    registrar("girar: fim");
    return 0;
}
