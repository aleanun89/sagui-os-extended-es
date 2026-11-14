// SPDX-License-Identifier: GPL-2.0-or-later
/* mount - monta sistemas de archivos (D040).
 *   mount                                   lista lo que está montado
 *   mount [-t fat32|ext2|ext3|ext4|iso9660|udf|cd|auto] [-o ro|rw] <dispositivo> <carpeta>
 * "-t cd" (D233): lo que tenga el CD/DVD (ISO 9660 o UDF), como auto.
 * El dispositivo es un nombre de "discos" (usb0, usb0p1, hda1), LABEL=rótulo
 * ou "arranque". Os discos internos montam só para leitura, salvo -o rw. */
#include "sagui.h"

static void listar_montagens(void)
{
    info_montagem_t m[10];
    int n = info_sistema(INFO_MONTAGENS, m, 10);
    if (n <= 0) { imprimir("No hay nada montado.\n"); return; }
    for (int i = 0; i < n; i++) {
        imprimir("%-7s en %s  tipo %s  (%s", m[i].dispositivo, m[i].ponto, m[i].tipo,
                 (m[i].opcoes & MONTAR_SO_LEITURA) ? "ro" : "rw");
        if (m[i].total_kb) imprimir(", %u MB libres de %u MB", m[i].livre_kb / 1024, m[i].total_kb / 1024);
        imprimir(")\n");
    }
}

static int uso(void)
{
    imprimir_em(ERROS, "uso: mount [-t fat32|ext2|ext3|ext4|iso9660|udf|cd|auto] [-o ro|rw] <dispositivo> <carpeta>\n");
    return 1;
}

int principal(int argc, char **argv)
{
    if (argc == 1) { listar_montagens(); return 0; }
    int opcoes = 0;
    const char *disp = NULL, *pasta = NULL;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "-t") == 0 && i + 1 < argc) {
            const char *t = argv[++i];
            if (compara_texto(t, "fat32") == 0 || compara_texto(t, "vfat") == 0) opcoes |= MONTAR_FAT32;
            else if (compara_texto(t, "ext2") == 0) opcoes |= MONTAR_EXT2;
            else if (compara_texto(t, "ext3") == 0) opcoes |= MONTAR_EXT3;
            else if (compara_texto(t, "ext4") == 0) opcoes |= MONTAR_EXT4;
            else if (compara_texto(t, "iso9660") == 0) opcoes |= MONTAR_ISO9660;   /* D207 */
            else if (compara_texto(t, "udf") == 0) opcoes |= MONTAR_UDF;           /* D233 */
            else if (compara_texto(t, "auto") != 0 && compara_texto(t, "cd") != 0) { imprimir_em(ERROS, "mount: tipo desconocido: %s\n", t); return 1; }
        } else if (compara_texto(argv[i], "-o") == 0 && i + 1 < argc) {
            char o[64];
            copia_texto(o, argv[++i], sizeof o);
            for (char *p = o; *p; ) {
                char *fim = p;
                while (*fim && *fim != ',') fim++;
                char c = *fim;
                *fim = 0;
                if (compara_texto(p, "ro") == 0) opcoes |= MONTAR_SO_LEITURA;
                else if (compara_texto(p, "rw") == 0) opcoes |= MONTAR_LEITURA_ESCRITA;
                else if (compara_texto(p, "defaults") != 0) { imprimir_em(ERROS, "mount: opción desconocida: %s\n", p); return 1; }
                p = c ? fim + 1 : fim;
            }
        } else if (argv[i][0] == '-') {
            return uso();
        } else if (!disp) disp = argv[i];
        else if (!pasta) pasta = argv[i];
        else return uso();
    }
    if (!disp || !pasta) return uso();
    int r = montar_em(disp, pasta, opcoes);
    if (r == ERRO_FORMATO) {
        /* D233: en un CD/DVD, lo que hay en el lector ("sin disco", "CD de audio: no se monta"...). */
        info_disco_t d;
        for (int i = 0; listar_discos(i, &d) == 1; i++)
            if (compara_texto(d.nome, disp) == 0 && (d.flags & DISCO_CD) && !d.suportado) {
                imprimir_em(ERROS, "mount: %s: %s\n", disp, d.formato);
                return 1;
            }
        imprimir_em(ERROS, "mount: %s: formato no soportado o tipo equivocado\n", disp);
        return 1;
    }
    if (r == ERRO_OCUPADO) { imprimir_em(ERROS, "mount: %s: ya está montado, o la carpeta %s está ocupada\n", disp, pasta); return 1; }
    if (r == ERRO_NAO_EXISTE) { imprimir_em(ERROS, "mount: %s: no existe ese dispositivo (mira \"discos\")\n", disp); return 1; }
    if (r < 0) { imprimir_em(ERROS, "mount: %s: %s\n", disp, nome_erro(r)); return 1; }
    /* Comprueba cómo quedó: el sistema puede haber rechazado la escritura. */
    info_montagem_t m[10];
    int n = info_sistema(INFO_MONTAGENS, m, 10);
    for (int i = 0; i < n; i++) {
        char abs[128];
        if (pasta[0] == '/') copia_texto(abs, pasta, sizeof abs);
        else { pasta_atual(abs, sizeof abs); if (tam_texto(abs) > 1) junta_texto(abs, "/", sizeof abs); junta_texto(abs, pasta, sizeof abs); }
        if (compara_texto(m[i].ponto, abs) != 0) continue;
        bool ro = (m[i].opcoes & MONTAR_SO_LEITURA) != 0;
        imprimir("%s (%s) montado en %s%s\n", m[i].dispositivo, m[i].tipo, m[i].ponto, ro ? ", solo lectura" : "");
        if (ro && (opcoes & MONTAR_LEITURA_ESCRITA))
            imprimir(compara_texto(m[i].tipo, "iso9660") == 0 || compara_texto(m[i].tipo, "udf") == 0 ? "Aviso: un CD/DVD es de solo lectura.\n"
                     : "Aviso: SaguiOS solo sabe leer este sistema de archivos (ext3/ext4, diario pendiente o características desconocidas).\n");
        return 0;
    }
    imprimir("%s montado en %s\n", disp, pasta);
    return 0;
}
