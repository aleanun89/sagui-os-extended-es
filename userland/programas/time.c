// SPDX-License-Identifier: GPL-2.0-or-later
/* time - ejecuta una orden y mide cuánto tarda (D232). */
#include "execucao.h"

static const char *AYUDA =
    "uso: time ORDEN [argumentos...]\n"
    "  Ejecuta la orden y, al terminar, escribe por el canal de errores el tiempo real que tardó\n"
    "  (real 0m1,234s). Devuelve el código de salida de la orden.\n"
    "  Solo vale para programas (los comandos internos del shell, como cd o echo, no).\n";

int principal(int argc, char **argv)
{
    if (argc < 2) { imprimir_em(ERROS, "uso: time ORDEN [argumentos...]  (time --help)\n"); return 2; }
    if (compara_texto(argv[1], "--help") == 0 || compara_texto(argv[1], "--ayuda") == 0) { escrever_texto(SAIDA, AYUDA); return 0; }
    unsigned t0 = tempo_ms();
    int pid = lanzar_orden("time", &argv[1]);
    if (pid < 0) return 127;
    int codigo = 0;
    esperar(pid, &codigo);
    unsigned ms = tempo_ms() - t0;
    imprimir_em(ERROS, "\nreal\t%um%u,%03us\n", ms / 60000, ms / 1000 % 60, ms % 1000);
    return codigo;
}
