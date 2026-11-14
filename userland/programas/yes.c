// SPDX-License-Identifier: GPL-2.0-or-later
/* yes - repite un texto (por defecto «y») hasta que se cierre la salida (D097). */
#include "sagui.h"

int principal(int argc, char **argv)
{
    if (argc > 1 && (compara_texto(argv[1], "--help") == 0 || compara_texto(argv[1], "--ayuda") == 0)) {
        escrever_texto(SAIDA, "uso: yes [texto...]\nEscribe el texto (por defecto «y») una y otra vez; para con Ctrl+C o cuando el pipe se cierra (yes | head -3).\n");
        return 0;
    }
    char l[512];
    int k = 0;
    if (argc < 2) k = formatar_texto(l, sizeof l, "y");
    for (int i = 1; i < argc; i++) k += formatar_texto(l + k, sizeof l - (size_t)k, "%s%s", i > 1 ? " " : "", argv[i]);
    if (k > 500) k = 500;
    l[k++] = '\n';
    while (escrever_tudo(SAIDA, l, (unsigned)k) >= 0) ;
    return 0;
}
