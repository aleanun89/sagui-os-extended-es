// SPDX-License-Identifier: GPL-3.0-only
/* montar [disco] - monta el pendrive en /usb (sin nombre: el primero disponible
 * para montar). Forma antigua, mantenida; mount hace el resto (D040). */
#include "sagui.h"

int principal(int argc, char **argv)
{
    char nome[16] = "";
    if (argc > 1) copia_texto(nome, argv[1], sizeof nome);
    else {
        info_disco_t d;
        for (int i = 0; listar_discos(i, &d) == 1; i++)
            if (d.suportado && !d.particao && (d.flags & DISCO_REMOVIVEL) && !(d.flags & DISCO_BOOT)) {
                copia_texto(nome, d.nome, sizeof nome);
                break;
            }
        if (!nome[0]) { imprimir_em(ERROS, "montar: no hay ningún pendrive FAT32, exFAT, ext2 o XFS conectado\n"); return 1; }
    }
    int r = montar(nome);
    if (r == ERRO_FORMATO) { imprimir_em(ERROS, "montar: %s: formato no soportado (solo FAT32, exFAT, ext2/ext3/ext4 y XFS para leer)\n", nome); return 1; }
    if (r == ERRO_OCUPADO) { imprimir_em(ERROS, "montar: ya hay un pendrive montado en /usb\n"); return 1; }
    if (r < 0) { imprimir_em(ERROS, "montar: %s: %s\n", nome, nome_erro(r)); return 1; }
    /* D446: un XFS (o un disco que solo se puede leer) lo dice */
    info_montagem_t m[16];
    int n = info_sistema(INFO_MONTAGENS, m, 16);
    for (int i = 0; i < n; i++)
        if (compara_texto(m[i].ponto, "/usb") == 0 && (m[i].opcoes & MONTAR_SO_LEITURA)) {
            imprimir("Pendrive montado en /usb (%s, solo lectura)\n", m[i].tipo);
            return 0;
        }
    imprimir("Pendrive montado en /usb\n");
    return 0;
}
