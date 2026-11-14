// SPDX-License-Identifier: GPL-2.0-or-later
/* rev - escribe cada línea al revés (D232). */
#include "sagui.h"

static const char *AYUDA =
    "uso: rev [archivo...]\n"
    "  Escribe cada línea invertida, de la última letra a la primera (echo hola | rev -> aloh).\n"
    "  Invierte bytes: con caracteres UTF-8 de varios bytes el resultado no es legible.\n";

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    int r, codigo = 0;
    while ((r = obter_opcao(argc, argv, "", &o)) >= 0) {
        if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        return opcao_invalida("rev", r, &o);
    }
    static char linea[8192];
    for (int i = o.indice; i < argc || i == o.indice; i++) {
        int fd = abrir_entrada("rev", i < argc ? argv[i] : "-");
        if (fd < 0) { codigo = 1; if (i >= argc) break; continue; }
        leitor_linhas_t l;
        leitor_iniciar(&l, fd, false);
        int n;
        while ((n = leitor_linha(&l, linea, sizeof linea, NULL)) >= 0) {
            for (int a = 0, b = n - 1; a < b; a++, b--) { char t = linea[a]; linea[a] = linea[b]; linea[b] = t; }
            if (saida_linha(linea, n) < 0) { fechar_entrada(fd); return 0; }
        }
        fechar_entrada(fd);
        if (i >= argc) break;
    }
    return codigo;
}
