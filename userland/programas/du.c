// SPDX-License-Identifier: GPL-2.0-or-later
/* du - espacio que ocupan archivos y carpetas (D097). Tamaños reales (bytes), en bloques de 1 KB. */
#include "sagui.h"

static const char *AYUDA =
    "uso: du [-a] [-h] [-s] [-c] [-b] [-d N] [ruta...]\n"
    "  -a  muestra también cada archivo     -h  tamaños legibles (1.5K, 3M)\n"
    "  -s  solo el total de cada ruta       -c  añade una línea con el total general\n"
    "  -b  tamaños en bytes                 -d N  profundidad máxima de lo que se muestra\n";

static bool todos, humano, resumen, total, en_bytes;
static int max_prof = 1000, erros;

static void escribir(uint32_t bytes, const char *ruta)
{
    char t[16], l[300];
    if (humano) formatar_tamanho(t, sizeof t, bytes);
    else if (en_bytes) formatar_texto(t, sizeof t, "%u", bytes);
    else formatar_texto(t, sizeof t, "%u", (bytes + 1023) / 1024);
    formatar_texto(l, sizeof l, "%s\t%s", t, ruta);
    if (saida_linha(l, -1) < 0) sair(0);
}

static uint32_t medir(const char *ruta, unsigned tipo, unsigned tam, int prof)
{
    if (tipo != TIPO_PASTA) {
        if (todos && !resumen && prof <= max_prof) escribir(tam, ruta);
        return tam;
    }
    uint32_t suma = 0;
    entrada_pasta_t *v;
    int n = listar_tudo(ruta, &v);
    if (n < 0) { imprimir_em(ERROS, "du: %s: %s\n", ruta, nome_erro(n)); erros++; return 0; }
    for (int i = 0; i < n && prof < 40; i++) {
        char h[256];
        caminho_juntar(h, sizeof h, ruta, v[i].nome);
        suma += medir(h, v[i].tipo, v[i].tamanho, prof + 1);
    }
    libera(v);
    if (!resumen && prof <= max_prof) escribir(suma, ruta);
    return suma;
}

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    static const opcao_longa_t largas[] = { { "human-readable", 'h' }, { "summarize", 's' }, { "max-depth", 'd' }, { NULL, 0 } };
    o.longas = largas;
    int r, v;
    while ((r = obter_opcao(argc, argv, "ahscbkd:", &o)) >= 0) {
        switch (r) {
        case 'a': todos = true; break;
        case 'h': humano = true; break;
        case 's': resumen = true; break;
        case 'c': total = true; break;
        case 'b': en_bytes = true; break;
        case 'k': break;
        case 'd':
            if (!texto_para_numero(o.valor, &v)) { imprimir_em(ERROS, "du: %s: profundidad no válida\n", o.valor); return 2; }
            max_prof = v;
            break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("du", r, &o);
        }
    }
    uint32_t suma = 0;
    int n = argc - o.indice;
    for (int i = 0; i < (n ? n : 1); i++) {
        const char *ruta = n ? argv[o.indice + i] : ".";
        info_arquivo_t inf;
        int s = info_arquivo(ruta, &inf);
        if (s < 0) { imprimir_em(ERROS, "du: %s: %s\n", ruta, nome_erro(s)); erros++; continue; }
        char c[256];
        copia_texto(c, ruta, sizeof c);
        size_t k = tam_texto(c);
        while (k > 1 && c[k - 1] == '/') c[--k] = 0;
        uint32_t t = medir(c, inf.tipo, inf.tamanho, 0);
        if (resumen || inf.tipo != TIPO_PASTA) { if (!(todos && !resumen && inf.tipo != TIPO_PASTA)) escribir(t, c); }
        suma += t;
    }
    if (total) escribir(suma, "total");
    return erros ? 1 : 0;
}
