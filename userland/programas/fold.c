// SPDX-License-Identifier: GPL-2.0-or-later
/* fold - parte las líneas largas (D232). */
#include "sagui.h"

static const char *AYUDA =
    "uso: fold [-s] [-w ANCHO] [archivo...]\n"
    "  Parte las líneas que pasan de ANCHO columnas (80 por defecto) en varias.\n"
    "  -s  parte por el último espacio en vez de a mitad de palabra\n";

int principal(int argc, char **argv)
{
    int ancho = 80;
    bool espacios = false;
    opcoes_t o = { 0 };
    int r, codigo = 0;
    while ((r = obter_opcao(argc, argv, "sbw:", &o)) >= 0) {
        switch (r) {
        case 's': espacios = true; break;
        case 'b': break;
        case 'w': if (!texto_para_numero(o.valor, &ancho) || ancho < 1) { imprimir_em(ERROS, "fold: -w: número no válido\n"); return 2; } break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("fold", r, &o);
        }
    }
    static char linea[8192];
    for (int i = o.indice; i < argc || i == o.indice; i++) {
        int fd = abrir_entrada("fold", i < argc ? argv[i] : "-");
        if (fd < 0) { codigo = 1; if (i >= argc) break; continue; }
        leitor_linhas_t l;
        leitor_iniciar(&l, fd, false);
        int n;
        while ((n = leitor_linha(&l, linea, sizeof linea, NULL)) >= 0) {
            int p = 0;
            if (n == 0 && saida_linha("", 0) < 0) return 0;
            while (p < n) {
                int k = n - p < ancho ? n - p : ancho;
                if (p + k < n && espacios) {
                    int q = k;
                    while (q > 0 && linea[p + q - 1] != ' ') q--;
                    if (q > 0) k = q;
                }
                if (saida_linha(linea + p, k) < 0) { fechar_entrada(fd); return 0; }
                p += k;
            }
        }
        fechar_entrada(fd);
        if (i >= argc) break;
    }
    return codigo;
}
