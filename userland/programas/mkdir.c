// SPDX-License-Identifier: GPL-3.0-only
/* mkdir (crear-carpeta) - crea carpetas (D097). -p crea también las intermedias y no
 * se queja si ya existe; -v explica lo que crea. */
#include "sagui.h"

static const char *AYUDA =
    "uso: mkdir [-p] [-v] carpeta...\n"
    "  -p  crea las carpetas intermedias que falten y no falla si ya existe\n"
    "  -v  muestra cada carpeta creada\n";

static bool paso(const char *c, bool pasta)
{
    info_arquivo_t inf;
    return info_arquivo(c, &inf) == 0 && (!pasta || inf.tipo == TIPO_PASTA);
}

int principal(int argc, char **argv)
{
    bool pais = false, verboso = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "pv", &o)) >= 0) {
        switch (r) {
        case 'p': pais = true; break;
        case 'v': verboso = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("mkdir", r, &o);
        }
    }
    if (o.indice >= argc) { imprimir_em(ERROS, "mkdir: falta el nombre de la carpeta (mkdir --help)\n"); return 2; }
    int erros = 0;
    for (int i = o.indice; i < argc; i++) {
        char c[256];
        copia_texto(c, argv[i], sizeof c);
        size_t n = tam_texto(c);
        while (n > 1 && c[n - 1] == '/') c[--n] = 0;
        if (pais) {
            /* crea cada prefijo: a, a/b, a/b/c */
            for (size_t k = 1; k < n; k++) {
                if (c[k] != '/') continue;
                c[k] = 0;
                if (!paso(c, false)) {
                    int s = criar_pasta(c);
                    if (s < 0) { imprimir_em(ERROS, "mkdir: %s: %s\n", c, nome_erro(s)); erros++; c[k] = '/'; goto siguiente; }
                    if (verboso) imprimir("mkdir: creada la carpeta '%s'\n", c);
                }
                c[k] = '/';
            }
            if (paso(c, true)) continue;
        }
        int s = criar_pasta(c);
        if (s < 0) {
            imprimir_em(ERROS, "mkdir: %s: %s\n", c, s == ERRO_JA_EXISTE ? "ya existe" : nome_erro(s));
            erros++;
        } else if (verboso) imprimir("mkdir: creada la carpeta '%s'\n", c);
siguiente:;
    }
    return erros ? 1 : 0;
}
