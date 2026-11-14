// SPDX-License-Identifier: GPL-3.0-only
/* sobre - versión y créditos de SaguiOS-Extended (D101). */
#include "sagui.h"
#include "versao.h"
#include "compilacao.h"   /* fecha, hora y commit de la compilación (D126) */

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    static info_processo_t v[64];
    int procs = info_sistema(INFO_PROCESSOS, v, 64);
    info_memoria_t m;
    info_sistema(INFO_MEMORIA, &m, sizeof m);
    imprimir(SAGUI_NOME_VERSAO ": un sistema operativo escrito desde cero.\n");
    imprimir("Kernel de 32 bits con paginación, procesos en el anillo 3, multitarea\n");
    imprimir("apropiativa, llamadas al sistema, archivos en la RAM y pipes.\n");
    imprimir("Basado en SaguiOS, proyecto original de Eli Rigobeli\n");
    imprimir("(github.com/erigobeli/sagui-os), escrito por Claude (Anthropic).\n");
    imprimir("Red (driver e1000, TCP/IP) y navegador Cipó: M4R4G0N.\n");
    imprimir(SAGUI_NOME ": mantenido por Alejandro Anuncibay,\n");
    imprimir("con mejoras escritas con Claude (Anthropic)\n");
    imprimir("y con un agente de IA (MiniMax Code).\n");
    imprimir("Compilado el %s a las %s (commit %s, variante %s).\n",
            SAGUI_BUILD_DATA, SAGUI_BUILD_HORA, SAGUI_BUILD_COMMIT, SAGUI_VARIANTE);
    imprimir("Código de terceros: MicroPython (MIT), libm (musl/FreeBSD),\n");
    imprimir("partes de drivers de Linux (GPLv2); ver docs/terceros.md.\n");
    imprimir("Ahora: %d procesos, %u MB de memoria libres.\n", procs, m.livre_kb / 1024);
    imprimir("Lee /sobre.txt y /historia/saguios.txt. La mascota está en /arte/sagui.txt.\n");
    return 0;
}
