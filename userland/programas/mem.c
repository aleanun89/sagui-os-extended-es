// SPDX-License-Identifier: GPL-3.0-only
/* mem - memória total, usada e livre. */
#include "sagui.h"

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    info_memoria_t m;
    if (info_sistema(INFO_MEMORIA, &m, sizeof m) < 0) return 1;
    imprimir("Memoria RAM total (E820):   %u MB\n", m.ram_total_kb / 1024);
    imprimir("Gestionada por el kernel:   %u MB\n", m.gerida_kb / 1024);
    imprimir("Usada:                      %u KB\n", m.usada_kb);
    imprimir("Libre:                      %u MB (%u KB)\n", m.livre_kb / 1024, m.livre_kb);
    /* D348: la memoria alta (por encima de 1,875 GB), solo para programas. */
    imprimir("Baja (kernel y programas):  %u MB, libre %u MB\n", (m.gerida_kb - m.alta_gerida_kb) / 1024,
             (m.livre_kb - m.alta_livre_kb) / 1024);
    imprimir("Alta (solo programas):      %u MB, libre %u MB\n", m.alta_gerida_kb / 1024, m.alta_livre_kb / 1024);
    /* D349: con PAE, la parte de la alta por encima de 4 GB; y el modo de paginación. */
    if (m.superior_gerida_kb)
        imprimir("  por encima de 4 GB:       %u MB, libre %u MB\n", m.superior_gerida_kb / 1024, m.superior_livre_kb / 1024);
    imprimir("Paginacion:                 %s, NX %s\n", (m.modo & MEM_MODO_PAE) ? "PAE" : "sin PAE",
             (m.modo & MEM_MODO_NX) ? "activo" : "no");
    return 0;
}
