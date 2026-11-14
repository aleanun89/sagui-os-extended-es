// SPDX-License-Identifier: GPL-2.0-or-later
/* hostname - nombre de la máquina (D097). SaguiOS no guarda uno: usa HOSTNAME o «sagui». */
#include "sagui.h"

int principal(int argc, char **argv)
{
    if (argc > 1 && (compara_texto(argv[1], "--help") == 0 || compara_texto(argv[1], "--ayuda") == 0 || compara_texto(argv[1], "-h") == 0)) {
        escrever_texto(SAIDA, "uso: hostname\nMuestra el nombre de la máquina: la variable HOSTNAME si existe y, si no, «sagui».\n");
        return 0;
    }
    if (argc > 1) { imprimir_em(ERROS, "hostname: SaguiOS no permite cambiar el nombre (define HOSTNAME en /etc/sagshrc)\n"); return 1; }
    const char *h = obter_env("HOSTNAME");
    saida_linha(h && *h ? h : "sagui", -1);
    return 0;
}
