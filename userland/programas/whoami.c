// SPDX-License-Identifier: GPL-2.0-or-later
/* whoami - nombre del usuario (D097; de verdad desde D255): la cuenta del uid del proceso. */
#include "sagui.h"
#include "contas.h"

int principal(int argc, char **argv)
{
    if (argc > 1 && (compara_texto(argv[1], "--help") == 0 || compara_texto(argv[1], "--ayuda") == 0 || compara_texto(argv[1], "-h") == 0)) {
        escrever_texto(SAIDA, "uso: whoami\nMuestra el nombre de la cuenta con la que corre (la del uid del proceso).\n");
        return 0;
    }
    char n[CONTA_NOME_MAX];
    saida_linha(conta_nome(meu_uid(), n, sizeof n), -1);
    return 0;
}
