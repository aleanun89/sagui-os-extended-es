// SPDX-License-Identifier: GPL-2.0-or-later
/* free (libre) - memoria total, usada y libre (D097). */
#include "sagui.h"

static const char *AYUDA =
    "uso: free [-b | -k | -m | -h]\n"
    "  -b bytes   -k KB (por defecto)   -m MB   -h legible (1.5G)\n";

static void celda(char *s, size_t max, uint32_t kb, int modo)
{
    if (modo == 'h' && kb >= 4194304u)   /* D349: más de 4 GB no cabe en bytes de 32 bits */
        formatar_texto(s, max, "%u.%uG", kb / 1048576u, kb % 1048576u * 10u / 1048576u);
    else if (modo == 'h') formatar_tamanho(s, max, kb * 1024u);
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
    imprimir("%-14s %12s %12s %12s\n", "Mem:", g, u, l);
    /* D348: la baja la usa el kernel y los programas cuando la alta se acaba; la
     * alta (por encima de 1,875 GB) solo los programas. */
    unsigned bg = m.gerida_kb - m.alta_gerida_kb, bl = m.livre_kb - m.alta_livre_kb;
    celda(g, sizeof g, bg, modo); celda(u, sizeof u, bg - bl, modo); celda(l, sizeof l, bl, modo);
    imprimir("%-14s %12s %12s %12s\n", "  baja:", g, u, l);
    celda(g, sizeof g, m.alta_gerida_kb, modo); celda(u, sizeof u, m.alta_gerida_kb - m.alta_livre_kb, modo);
    celda(l, sizeof l, m.alta_livre_kb, modo);
    imprimir("%-14s %12s %12s %12s\n", "  alta:", g, u, l);
    /* D349: de la alta, lo que está por encima de 4 GB (solo con PAE). */
    if (m.superior_gerida_kb) {
        celda(g, sizeof g, m.superior_gerida_kb, modo);
        celda(u, sizeof u, m.superior_gerida_kb - m.superior_livre_kb, modo);
        celda(l, sizeof l, m.superior_livre_kb, modo);
        imprimir("%-14s %12s %12s %12s\n", "   >4 GB:", g, u, l);
    }
    imprimir("%-14s %12s\n", "RAM del PC:", t);
    imprimir("%-14s %s%s\n", "Paginacion:", (m.modo & MEM_MODO_PAE) ? "PAE (hasta 64 GB)" :
             (m.modo & MEM_MODO_CPU_PAE) ? "sin PAE (desactivado; hasta 4 GB)" : "sin PAE (la CPU no lo tiene; hasta 4 GB)",
             (m.modo & MEM_MODO_NX) ? ", NX activo" : (m.modo & MEM_MODO_CPU_NX) ? ", NX no activo" : ", sin NX");
    return 0;
}
