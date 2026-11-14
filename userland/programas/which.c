// SPDX-License-Identifier: GPL-2.0-or-later
/* which (donde) - dice qué programa se ejecutaría para un nombre (D097). Busca en PATH (por defecto /bin:/apps/bin, D194). */
#include "sagui.h"

static const char *AYUDA =
    "uso: which [-a] orden...\n"
    "  -a  muestra todas las coincidencias en PATH, no solo la primera\n"
    "Devuelve 1 si alguna orden no se encuentra. Los comandos internos del shell (cd, echo...) no están en PATH:\n"
    "para esos usa «type».\n";

int principal(int argc, char **argv)
{
    bool todas = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "a", &o)) >= 0) {
        if (r == 'a') todas = true;
        else if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        else return opcao_invalida("which", r, &o);
    }
    if (o.indice >= argc) { imprimir_em(ERROS, "which: falta el nombre de una orden (which --help)\n"); return 2; }
    const char *path = obter_env("PATH");
    if (!path || !*path) path = SAGUI_PATH_PADRAO;
    int faltan = 0;
    for (int i = o.indice; i < argc; i++) {
        bool halla = false;
        if (acha_char(argv[i], '/')) {
            info_arquivo_t inf;
            if (info_arquivo(argv[i], &inf) == 0 && inf.tipo != TIPO_PASTA) { saida_linha(argv[i], -1); halla = true; }
        } else {
            const char *p = path;
            for (;;) {
                const char *fin = p;
                while (*fin && *fin != ':') fin++;
                char d[160], c[256];
                size_t n = (size_t)(fin - p);
                if (n >= sizeof d) n = sizeof d - 1;
                if (n == 0) copia_texto(d, ".", sizeof d);
                else { memcpy(d, p, n); d[n] = 0; }
                caminho_juntar(c, sizeof c, d, argv[i]);
                info_arquivo_t inf;
                if (info_arquivo(c, &inf) == 0 && inf.tipo != TIPO_PASTA) {
                    saida_linha(c, -1);
                    halla = true;
                    if (!todas) break;
                }
                if (!*fin) break;
                p = fin + 1;
            }
        }
        if (!halla) { imprimir_em(ERROS, "which: %s: no encontrado en PATH\n", argv[i]); faltan++; }
    }
    return faltan ? 1 : 0;
}
