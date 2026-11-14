// SPDX-License-Identifier: GPL-3.0-only
/* ola - imprime os argumentos recebidos (teste do carregador, H13). */
#include "sagui.h"

int principal(int argc, char **argv)
{
    imprimir("¡Hola! Soy el programa %s, proceso %d, ejecutándome en el anillo 3.\n", argv[0], meu_pid());
    imprimir("He recibido %d argumento(s):\n", argc);
    for (int i = 0; i < argc; i++)
        imprimir("  argv[%d] = \"%s\"\n", i, argv[i]);
    return 0;
}
