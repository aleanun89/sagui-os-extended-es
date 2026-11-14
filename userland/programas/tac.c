// SPDX-License-Identifier: GPL-2.0-or-later
/* tac - escribe las líneas en orden inverso: la última primero (D232). */
#include "sagui.h"

static const char *AYUDA =
    "uso: tac [archivo...]\n"
    "  Escribe las líneas del archivo (o de la entrada) de la última a la primera.\n";

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    int r, codigo = 0;
    while ((r = obter_opcao(argc, argv, "", &o)) >= 0) {
        if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        return opcao_invalida("tac", r, &o);
    }
    for (int i = o.indice; i < argc || i == o.indice; i++) {
        int fd = abrir_entrada("tac", i < argc ? argv[i] : "-");
        if (fd < 0) { codigo = 1; if (i >= argc) break; continue; }
        unsigned tam;
        char *t = ler_todo_fd(fd, &tam);
        fechar_entrada(fd);
        if (!t) { imprimir_em(ERROS, "tac: sin memoria\n"); return 1; }
        int fin = (int)tam;
        if (fin > 0 && t[fin - 1] == '\n') fin--;   /* el salto final no abre una línea vacía */
        while (fin > 0) {
            int ini = fin;
            while (ini > 0 && t[ini - 1] != '\n') ini--;
            if (saida_linha(t + ini, fin - ini) < 0) return 0;
            fin = ini > 0 ? ini - 1 : 0;
        }
        libera(t);
        if (i >= argc) break;
    }
    return codigo;
}
