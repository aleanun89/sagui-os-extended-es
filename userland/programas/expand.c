// SPDX-License-Identifier: GPL-2.0-or-later
/* expand - cambia los tabuladores por espacios (D232). */
#include "sagui.h"

static const char *AYUDA =
    "uso: expand [-t N] [archivo...]\n"
    "  Sustituye cada tabulador por los espacios que lleven a la siguiente columna múltiplo de N (8 por defecto).\n";

int principal(int argc, char **argv)
{
    int tab = 8;
    opcoes_t o = { 0 };
    int r, codigo = 0;
    while ((r = obter_opcao(argc, argv, "it:", &o)) >= 0) {
        switch (r) {
        case 'i': break;
        case 't': if (!texto_para_numero(o.valor, &tab) || tab < 1 || tab > 64) { imprimir_em(ERROS, "expand: -t: de 1 a 64\n"); return 2; } break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("expand", r, &o);
        }
    }
    static char linea[4096], sal[8192];
    for (int i = o.indice; i < argc || i == o.indice; i++) {
        int fd = abrir_entrada("expand", i < argc ? argv[i] : "-");
        if (fd < 0) { codigo = 1; if (i >= argc) break; continue; }
        leitor_linhas_t l;
        leitor_iniciar(&l, fd, false);
        int n;
        while ((n = leitor_linha(&l, linea, sizeof linea, NULL)) >= 0) {
            int k = 0;
            for (int j = 0; j < n && k < (int)sizeof sal - 70; j++) {
                if (linea[j] == '\t') { int m = tab - k % tab; while (m--) sal[k++] = ' '; }
                else sal[k++] = linea[j];
            }
            if (saida_linha(sal, k) < 0) { fechar_entrada(fd); return 0; }
        }
        fechar_entrada(fd);
        if (i >= argc) break;
    }
    return codigo;
}
