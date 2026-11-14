// SPDX-License-Identifier: GPL-3.0-only
/* rmdir (quitar-carpeta) - borra carpetas vacías (D097). -p borra también las carpetas
 * padre que queden vacías; -v explica lo que borra. */
#include "sagui.h"

static const char *AYUDA =
    "uso: rmdir [-p] [-v] carpeta...\n"
    "  -p  borra también las carpetas padre si quedan vacías (rmdir -p a/b/c)\n"
    "  -v  muestra cada carpeta borrada\n";

int principal(int argc, char **argv)
{
    bool pais = false, verboso = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "pv", &o)) >= 0) {
        switch (r) {
        case 'p': pais = true; break;
        case 'v': verboso = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("rmdir", r, &o);
        }
    }
    if (o.indice >= argc) { imprimir_em(ERROS, "rmdir: falta el nombre de la carpeta (rmdir --help)\n"); return 2; }
    int erros = 0;
    for (int i = o.indice; i < argc; i++) {
        char c[256];
        copia_texto(c, argv[i], sizeof c);
        size_t n = tam_texto(c);
        while (n > 1 && c[n - 1] == '/') c[--n] = 0;
        for (;;) {
            int s = remover_pasta(c);
            if (s < 0) { imprimir_em(ERROS, "rmdir: %s: %s\n", c, nome_erro(s)); erros++; break; }
            if (verboso) imprimir("rmdir: borrada la carpeta '%s'\n", c);
            if (!pais) break;
            char *b = acha_ultimo_char(c, '/');
            if (!b || b == c) break;
            *b = 0;
        }
    }
    return erros ? 1 : 0;
}
