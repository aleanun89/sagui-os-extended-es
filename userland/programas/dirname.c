// SPDX-License-Identifier: GPL-2.0-or-later
/* dirname - quita el último componente de una ruta (D097). */
#include "sagui.h"

static const char *AYUDA =
    "uso: dirname RUTA...\n"
    "  dirname /a/b/c.txt  ->  /a/b;  dirname c.txt  ->  .\n";

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "", &o)) >= 0) {
        if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        return opcao_invalida("dirname", r, &o);
    }
    if (o.indice >= argc) { imprimir_em(ERROS, "dirname: falta la ruta (dirname --help)\n"); return 2; }
    for (int i = o.indice; i < argc; i++) {
        char b[256];
        copia_texto(b, argv[i], sizeof b);
        size_t n = tam_texto(b);
        while (n > 1 && b[n - 1] == '/') b[--n] = 0;
        char *s = acha_ultimo_char(b, '/');
        if (!s) copia_texto(b, ".", sizeof b);
        else {
            while (s > b && s[-1] == '/') s--;
            if (s == b) b[1] = 0; else *s = 0;
        }
        saida_linha(b, -1);
    }
    return 0;
}
