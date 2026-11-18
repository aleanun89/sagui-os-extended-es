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
    /* D350: la caché de disco, en la alta si la hay. */
    imprimir("Caché de disco:             %u MB (en la alta %u MB)\n", m.cache_kb / 1024, m.cache_alta_kb / 1024);
    /* D352: la swap comprimida en RAM. */
    if (m.troca_teto_kb) {
        unsigned c = m.troca_comprimida_kb ? m.troca_comprimida_kb : 1, razao = m.troca_kb * 10u / c;
        imprimir("Swap comprimida:            %u MB en la swap, comprimidos en %u MB (razón %u.%u:1)\n",
                 m.troca_kb / 1024, m.troca_comprimida_kb / 1024, razao / 10, razao % 10);
        imprimir("  almacén:                  %u MB de un tope de %u MB; salidas %u, entradas %u\n",
                 m.troca_armazem_kb / 1024, m.troca_teto_kb / 1024, m.troca_saidas, m.troca_entradas);
    } else {
        imprimir("Swap comprimida:            desactivada\n");
    }
    /* D378 (fase 3): el código de las bibliotecas, una vez para todos los programas. */
    imprimir("Bibliotecas compartidas:    %u KB en %u trozos (mapeados %u KB en total)\n", m.compartida_kb,
             m.compartida_objetos, m.compartida_mapeada_kb);
    imprimir("Paginacion:                 %s, NX %s\n", (m.modo & MEM_MODO_PAE) ? "PAE" : "sin PAE",
             (m.modo & MEM_MODO_NX) ? "activo" : "no");
    return 0;
}
