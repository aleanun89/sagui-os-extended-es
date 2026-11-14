// SPDX-License-Identifier: GPL-2.0-or-later
/* cmp - compara dos archivos byte a byte (D232). */
#include "sagui.h"

static const char *AYUDA =
    "uso: cmp [-s] [-l] archivo1 archivo2\n"
    "  Dice en qué byte y línea difieren los dos archivos (sin salida si son iguales).\n"
    "  -s  silencioso, solo el código de salida      -l  lista cada byte distinto (posición y valores en octal)\n"
    "  Código de salida: 0 iguales, 1 distintos, 2 error.\n";

int principal(int argc, char **argv)
{
    bool callado = false, lista = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "sl", &o)) >= 0) {
        switch (r) {
        case 's': callado = true; break;
        case 'l': lista = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("cmp", r, &o);
        }
    }
    if (argc - o.indice != 2) { imprimir_em(ERROS, "uso: cmp [-sl] archivo1 archivo2  (cmp --help)\n"); return 2; }
    const char *n1 = argv[o.indice], *n2 = argv[o.indice + 1];
    int f1 = abrir_entrada("cmp", n1), f2 = f1 < 0 ? -1 : abrir_entrada("cmp", n2);
    if (f1 < 0 || f2 < 0) return 2;
    static unsigned char a[4096], b[4096];
    unsigned pos = 0, linea = 1;
    int dif = 0;
    for (;;) {
        int na = 0, nb = 0, k;
        while (na < (int)sizeof a && (k = ler(f1, a + na, (unsigned)(sizeof a - (unsigned)na))) > 0) na += k;
        while (nb < (int)sizeof b && (k = ler(f2, b + nb, (unsigned)(sizeof b - (unsigned)nb))) > 0) nb += k;
        int m = na < nb ? na : nb;
        for (int i = 0; i < m; i++) {
            pos++;
            if (a[i] != b[i]) {
                dif = 1;
                if (callado) return 1;
                if (!lista) { imprimir("%s %s son distintos: byte %u, línea %u\n", n1, n2, pos, linea); return 1; }
                imprimir("%u %c%c%c %c%c%c\n", pos, '0' + (a[i] >> 6), '0' + ((a[i] >> 3) & 7), '0' + (a[i] & 7),
                         '0' + (b[i] >> 6), '0' + ((b[i] >> 3) & 7), '0' + (b[i] & 7));
            }
            if (a[i] == '\n') linea++;
        }
        if (na != nb) {
            if (!callado) imprimir_em(ERROS, "cmp: fin de archivo en %s tras el byte %u\n", na < nb ? n1 : n2, pos);
            return 1;
        }
        if (na == 0) break;
    }
    fechar_entrada(f1);
    fechar_entrada(f2);
    return dif;
}
