// SPDX-License-Identifier: GPL-3.0-only
/* desmontar [disco] - graba todo lo que esté pendiente y libera o pendrive. */
#include "sagui.h"

int principal(int argc, char **argv)
{
    char nome[64] = "";
    if (argc > 1) copia_texto(nome, argv[1], sizeof nome);
    else {
        info_disco_t d;
        for (int i = 0; listar_discos(i, &d) == 1; i++)
            if (d.montado && !d.particao && (d.flags & DISCO_REMOVIVEL) && !(d.flags & DISCO_BOOT)) {
                copia_texto(nome, d.nome, sizeof nome);
                break;
            }
        if (!nome[0]) { imprimir_em(ERROS, "desmontar: no hay ningún pendrive montado\n"); return 1; }
    }
    int r = desmontar(nome);
    if (r < 0) { imprimir_em(ERROS, "desmontar: %s: %s\n", nome, nome_erro(r)); return 1; }
    imprimir("Ya puedes retirar el pendrive.\n");
    return 0;
}
