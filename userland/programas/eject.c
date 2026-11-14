// SPDX-License-Identifier: GPL-2.0-or-later
/* eject [-t] [lector] - desmonta el CD/DVD y abre la bandeja (D233).
 *   eject          el primer lector de CD/DVD (cd0)
 *   eject cd1      ese lector (también vale su imagen de arranque, cd1img, o la
 *                  carpeta donde está montado, /cd)
 *   eject -t       cierra la bandeja (muchos portátiles no pueden: se dice)
 * El kernel desmonta antes lo que haya del disco; si un programa lo está usando
 * (el instalador copiando del CD), no se expulsa. */
#include "sagui.h"

/* El lector de un nombre de "discos" o de una carpeta montada (/cd). */
static bool achar_leitor(const char *arg, char *nome, int max)
{
    info_disco_t d;
    for (int i = 0; listar_discos(i, &d) == 1; i++) {
        if (!(d.flags & DISCO_CD) || d.particao) continue;
        if (!arg || compara_texto(d.nome, arg) == 0 || (arg[0] == '/' && d.montado && compara_texto(d.ponto, arg) == 0)) {
            copia_texto(nome, d.nome, (size_t)max);
            return true;
        }
    }
    return false;
}

int principal(int argc, char **argv)
{
    bool fechar = false;
    const char *arg = NULL;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "-t") == 0) fechar = true;
        else if (argv[i][0] == '-' || arg) { imprimir_em(ERROS, "uso: eject [-t] [cd0|/cd]\n"); return 1; }
        else arg = argv[i];
    }
    char nome[16];
    if (!achar_leitor(arg, nome, sizeof nome)) {
        if (arg) imprimir_em(ERROS, "eject: %s: no es un lector de CD/DVD (mira \"discos\")\n", arg);
        else imprimir_em(ERROS, "eject: no hay ningún lector de CD/DVD\n");
        return 1;
    }
    int r = expulsar_disco(nome, fechar);
    if (r == ERRO_OCUPADO) {
        imprimir_em(ERROS, "eject: %s: está en uso (¿el instalador copiando del CD?); no se expulsa\n", nome);
        return 1;
    }
    if (r == ERRO_NAO_SUPORTADO && fechar) {
        imprimir_em(ERROS, "eject: %s: el lector no puede cerrar la bandeja solo; empújala\n", nome);
        return 1;
    }
    if (r < 0) { imprimir_em(ERROS, "eject: %s: %s\n", nome, nome_erro(r)); return 1; }
    imprimir(fechar ? "%s: bandeja cerrada.\n" : "%s: disco expulsado.\n", nome);
    return 0;
}
