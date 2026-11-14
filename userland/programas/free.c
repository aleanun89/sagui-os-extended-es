// SPDX-License-Identifier: GPL-2.0-or-later
/* free (libre) - memoria total, usada y libre (D097). */
#include "sagui.h"

static const char *AYUDA =
    "uso: free [-b | -k | -m | -h]\n"
    "  -b bytes   -k KB (por defecto)   -m MB   -h legible (1.5G)\n";

static void celda(char *s, size_t max, uint32_t kb, int modo)
{
    if (modo == 'h') formatar_tamanho(s, max, kb >= 4194304u ? 0xFFFFFFFFu : kb * 1024u);
    else if (modo == 'm') formatar_texto(s, max, "%u", kb / 1024);
    else if (modo == 'b') {
        if (kb >= 4194304u) formatar_texto(s, max, "%u000+", kb / 1024 * 1024 / 1000);   /* no cabe en 32 bits */
        else formatar_texto(s, max, "%u", kb * 1024);
    } else formatar_texto(s, max, "%u", kb);
}

int principal(int argc, char **argv)
{
    int modo = 'k';
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "bkmh", &o)) >= 0) {
        if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        if (r == '?' || r == ':') return opcao_invalida("free", r, &o);
        modo = r;
    }
    info_memoria_t m;
    if (info_sistema(INFO_MEMORIA, &m, sizeof m) < 0) { imprimir_em(ERROS, "free: no se puede leer la memoria\n"); return 1; }
    char t[16], u[16], l[16], g[16];
    celda(t, sizeof t, m.ram_total_kb, modo);
    celda(g, sizeof g, m.gerida_kb, modo);
    celda(u, sizeof u, m.usada_kb, modo);
    celda(l, sizeof l, m.livre_kb, modo);
    imprimir("%-14s %12s %12s %12s\n", "", "total", "usado", "libre");
    imprimir("%-14s %12s %12s %12s\n", "Mem (kernel):", g, u, l);
    imprimir("%-14s %12s\n", "RAM del PC:", t);
    return 0;
}
