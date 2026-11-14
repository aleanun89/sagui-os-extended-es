// SPDX-License-Identifier: GPL-2.0-or-later
/* tree (arbol) - muestra carpetas y archivos como un árbol (D097). Usa solo ASCII (|-- y `--). */
#include "sagui.h"

static const char *AYUDA =
    "uso: tree [-a] [-d] [-L N] [carpeta...]\n"
    "  -a  incluye nombres que empiezan por punto     -d  solo carpetas\n"
    "  -L N  profundidad máxima\n";

static bool ocultos, solo_pastas;
static int max_prof = 1000, carpetas, archivos;

static void recorrer(const char *pasta, const char *prefijo, int prof)
{
    entrada_pasta_t *v;
    int n = listar_tudo(pasta, &v);
    if (n < 0) return;
    int k = 0;
    entrada_pasta_t *vis = v;
    for (int i = 0; i < n; i++) {
        if (!ocultos && v[i].nome[0] == '.') continue;
        if (solo_pastas && v[i].tipo != TIPO_PASTA) continue;
        vis[k++] = v[i];
    }
    for (int i = 0; i < k; i++) {
        bool ultimo = i == k - 1;
        char l[300];
        formatar_texto(l, sizeof l, "%s%s%s%s", prefijo, ultimo ? "`-- " : "|-- ", vis[i].nome, vis[i].tipo == TIPO_PASTA ? "/" : "");
        if (saida_linha(l, -1) < 0) sair(0);
        if (vis[i].tipo == TIPO_PASTA) {
            carpetas++;
            if (prof < max_prof && prof < 30) {
                char h[256], p2[200];
                caminho_juntar(h, sizeof h, pasta, vis[i].nome);
                formatar_texto(p2, sizeof p2, "%s%s", prefijo, ultimo ? "    " : "|   ");
                recorrer(h, p2, prof + 1);
            }
        } else archivos++;
    }
    libera(v);
}

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    int r, v;
    while ((r = obter_opcao(argc, argv, "adL:", &o)) >= 0) {
        switch (r) {
        case 'a': ocultos = true; break;
        case 'd': solo_pastas = true; break;
        case 'L':
            if (!texto_para_numero(o.valor, &v) || v < 1) { imprimir_em(ERROS, "tree: %s: profundidad no válida\n", o.valor); return 2; }
            max_prof = v;
            break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("tree", r, &o);
        }
    }
    int n = argc - o.indice, erros = 0;
    for (int i = 0; i < (n ? n : 1); i++) {
        const char *ruta = n ? argv[o.indice + i] : ".";
        info_arquivo_t inf;
        int s = info_arquivo(ruta, &inf);
        if (s < 0) { imprimir_em(ERROS, "tree: %s: %s\n", ruta, nome_erro(s)); erros++; continue; }
        saida_linha(ruta, -1);
        if (inf.tipo == TIPO_PASTA) recorrer(ruta, "", 1);
    }
    imprimir("\n%d carpetas, %d archivos\n", carpetas, archivos);
    return erros ? 1 : 0;
}
