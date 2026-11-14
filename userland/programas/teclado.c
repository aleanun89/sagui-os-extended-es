// SPDX-License-Identifier: GPL-2.0-or-later
/* teclado - muestra o cambia el layout do teclado (ES ou ABNT2). */
#include "sagui.h"

static const char *nome_layout(int l) { return l == TECLADO_ABNT2 ? "abnt2" : "es"; }

int principal(int argc, char **argv)
{
    if (argc == 1) {
        imprimir("Distribución del teclado: %s\n", nome_layout(teclado_layout(-1)));
        return 0;
    }
    int novo = -1;
    if (argc == 2 && compara_texto(argv[1], "es") == 0) novo = TECLADO_ES;
    else if (argc == 2 && compara_texto(argv[1], "abnt2") == 0) novo = TECLADO_ABNT2;
    if (novo < 0) {
        imprimir_em(ERROS, "uso: teclado [es | abnt2]\n"
                           "  es     español (ñ, ç, ¿, ¡; acentos con teclas muertas)\n"
                           "  abnt2  portugués de Brasil (ç; ~ y ^ como teclas muertas)\n");
        return 1;
    }
    int r = teclado_layout(novo);
    if (r < 0) { imprimir_em(ERROS, "teclado: %s\n", nome_erro(r)); return 1; }
    imprimir("Distribución del teclado: %s\n", nome_layout(r));
    return 0;
}
