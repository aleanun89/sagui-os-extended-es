// SPDX-License-Identifier: GPL-2.0-or-later
/* umount <carpeta|dispositivo> - graba lo que falta y desmonta (D040). */
#include "sagui.h"

#define MAX 32
static char antes[MAX][12];

int principal(int argc, char **argv)
{
    if (argc != 2) { imprimir_em(ERROS, "uso: umount <carpeta|dispositivo>\n"); return 1; }
    /* Pendrives con algo montado antes: si quedan libres, avisa que se puede quitar. */
    info_disco_t d;
    int n = 0;
    for (int i = 0; listar_discos(i, &d) == 1 && n < MAX; i++)
        if (!d.particao && d.montado && (d.flags & DISCO_REMOVIVEL) && !(d.flags & DISCO_BOOT)) copia_texto(antes[n++], d.nome, 12);
    int r = desmontar(argv[1]);
    if (r == ERRO_NAO_EXISTE) { imprimir_em(ERROS, "umount: %s: no está montado\n", argv[1]); return 1; }
    if (r < 0) { imprimir_em(ERROS, "umount: %s: %s\n", argv[1], nome_erro(r)); return 1; }
    imprimir("Desmontado: %s\n", argv[1]);
    for (int k = 0; k < n; k++)
        for (int i = 0; listar_discos(i, &d) == 1; i++)
            if (compara_texto(d.nome, antes[k]) == 0 && !d.montado) imprimir("Ya puedes retirar el pendrive (%s).\n", d.nome);
    return 0;
}
