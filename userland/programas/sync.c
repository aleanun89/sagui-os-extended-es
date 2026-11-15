// SPDX-License-Identifier: GPL-2.0-or-later
/* sync - graba la caché de bloques de todos los discos (D281).
 *   sync          vacía la escritura diferida de todos los discos internos
 *   sync -v       además anota en el log del kernel las cifras de la caché ([CACHE]) */
#include "sagui.h"

int principal(int argc, char **argv)
{
    int r = desmontar("*");
    if (r < 0) { imprimir_em(ERROS, "sync: %s\n", nome_erro(r)); return 1; }
    if (argc > 1 && compara_texto(argv[1], "-v") == 0) desmontar("?cache");
    return 0;
}
