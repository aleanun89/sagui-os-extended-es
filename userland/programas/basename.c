// SPDX-License-Identifier: GPL-2.0-or-later
/* basename - quita la carpeta (y un sufijo) de una ruta (D097). */
#include "sagui.h"

static const char *AYUDA =
    "uso: basename RUTA [SUFIJO]     basename -a [-s SUFIJO] RUTA...\n"
    "  basename /usr/lib/x.txt  ->  x.txt;  basename x.txt .txt  ->  x\n";

static void una(const char *ruta, const char *suf)
{
    char b[256];
    copia_texto(b, ruta, sizeof b);
    size_t n = tam_texto(b);
    while (n > 1 && b[n - 1] == '/') b[--n] = 0;
    const char *base = n == 1 && b[0] == '/' ? b : nome_base(b);
    char r[256];
    copia_texto(r, base, sizeof r);
    if (suf && *suf) {
        size_t ls = tam_texto(suf), lr = tam_texto(r);
        if (lr > ls && compara_texto(r + lr - ls, suf) == 0) r[lr - ls] = 0;
    }
    saida_linha(r, -1);
}

int principal(int argc, char **argv)
{
    bool varios = false;
    const char *suf = NULL;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "as:", &o)) >= 0) {
        if (r == 'a') varios = true;
        else if (r == 's') { suf = o.valor; varios = true; }
        else if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        else return opcao_invalida("basename", r, &o);
    }
    int n = argc - o.indice;
    if (n < 1) { imprimir_em(ERROS, "basename: falta la ruta (basename --help)\n"); return 2; }
    if (!varios) {
        if (n > 2) { imprimir_em(ERROS, "basename: demasiados argumentos\n"); return 2; }
        una(argv[o.indice], n == 2 ? argv[o.indice + 1] : NULL);
    } else for (int i = o.indice; i < argc; i++) una(argv[i], suf);
    return 0;
}
