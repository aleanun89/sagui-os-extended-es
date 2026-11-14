// SPDX-License-Identifier: GPL-2.0-or-later
/* paste - junta las líneas de varios archivos lado a lado (D232). */
#include "sagui.h"

static const char *AYUDA =
    "uso: paste [-s] [-d LISTA] archivo...\n"
    "  Escribe la línea 1 de cada archivo separadas por tabulador, luego la 2, etc. Un \"-\" es la entrada estándar.\n"
    "  -d LISTA  separadores que se usan por turnos (por defecto, tabulador)\n"
    "  -s  pega cada archivo en una sola línea, una línea de salida por archivo\n";

#define MAXF 8

int principal(int argc, char **argv)
{
    const char *sep = "\t";
    bool serie = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "sd:", &o)) >= 0) {
        switch (r) {
        case 's': serie = true; break;
        case 'd': sep = o.valor; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("paste", r, &o);
        }
    }
    int nf = argc - o.indice;
    static char *solo[] = { "-" };
    char **arch = nf > 0 ? &argv[o.indice] : solo;
    if (nf < 1) nf = 1;
    if (nf > MAXF) { imprimir_em(ERROS, "paste: como mucho %d archivos\n", MAXF); return 2; }
    int ns = (int)tam_texto(sep);
    if (ns == 0) sep = "\t", ns = 1;
    static leitor_linhas_t ls[MAXF];
    int fds[MAXF];
    bool vivo[MAXF];
    for (int i = 0; i < nf; i++) {
        fds[i] = abrir_entrada("paste", arch[i]);
        if (fds[i] < 0) return 1;
        leitor_iniciar(&ls[i], fds[i], false);
        vivo[i] = true;
    }
    static char linea[4096], sal[MAXF * 4100];
    if (serie) {
        for (int i = 0; i < nf; i++) {
            int k = 0, n, j = 0;
            while ((n = leitor_linha(&ls[i], linea, sizeof linea, NULL)) >= 0) {
                if (j++) sal[k++] = sep[(j - 2) % ns];
                if (k + n < (int)sizeof sal - 2) { memcpy(sal + k, linea, (size_t)n); k += n; }
            }
            if (saida_linha(sal, k) < 0) return 0;
        }
    } else {
        for (;;) {
            int k = 0, vivos = 0;
            for (int i = 0; i < nf; i++) {
                if (i) sal[k++] = sep[(i - 1) % ns];
                int n = vivo[i] ? leitor_linha(&ls[i], linea, sizeof linea, NULL) : -1;
                if (n < 0) { vivo[i] = false; continue; }
                vivos++;
                memcpy(sal + k, linea, (size_t)n);
                k += n;
            }
            if (!vivos) break;
            if (saida_linha(sal, k) < 0) return 0;
        }
    }
    for (int i = 0; i < nf; i++) fechar_entrada(fds[i]);
    return 0;
}
