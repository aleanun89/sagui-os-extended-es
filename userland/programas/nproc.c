// SPDX-License-Identifier: GPL-2.0-or-later
/* nproc - cuántas CPU hay (D232). */
#include "sagui.h"

static const char *AYUDA =
    "uso: nproc [--all]\n"
    "  Escribe cuántos procesadores (núcleos e hilos) usa el sistema. Con --all, los que detectó el PC aunque\n"
    "  el SMP esté apagado (por defecto solo arranca la CPU 0, D164).\n";

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    static const opcao_longa_t largas[] = { { "all", 'a' }, { NULL, 0 } };
    o.longas = largas;
    int r;
    while ((r = obter_opcao(argc, argv, "a", &o)) >= 0) {
        if (r == 'a') continue;
        if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        return opcao_invalida("nproc", r, &o);
    }
    static info_nucleo_t v[64];
    int n = info_sistema(INFO_NUCLEOS, v, 64);
    if (n < 1) n = 1;
    imprimir("%d\n", n);
    return 0;
}
