// SPDX-License-Identifier: GPL-2.0-or-later
/* uname - nombre y versión del sistema (D097). */
#include "sagui.h"

static const char *AYUDA =
    "uso: uname [-a] [-s] [-n] [-r] [-v] [-m] [-o]\n"
    "  -s sistema (por defecto)  -n nombre de la máquina  -r versión  -v compilación  -m arquitectura  -o sistema operativo  -a todo\n";

int principal(int argc, char **argv)
{
    bool s = false, n = false, r = false, v = false, m = false, op = false;
    opcoes_t o = { 0 };
    int c;
    while ((c = obter_opcao(argc, argv, "asnrvmop", &o)) >= 0) {
        switch (c) {
        case 'a': s = n = r = v = m = op = true; break;
        case 's': s = true; break;
        case 'n': n = true; break;
        case 'r': r = true; break;
        case 'v': v = true; break;
        case 'm': case 'p': m = true; break;
        case 'o': op = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("uname", c, &o);
        }
    }
    if (!(s || n || r || v || m || op)) s = true;
    info_versao_t iv;
    char ver[40] = "?";
    if (info_sistema(INFO_VERSAO, &iv, sizeof iv) == 0) copia_texto(ver, iv.versao, sizeof ver);
    const char *host = obter_env("HOSTNAME");
    if (!host || !*host) host = "sagui";
    char t[200];
    int k = 0;
    if (s) k += formatar_texto(t + k, sizeof t - (size_t)k, "%sSaguiOS", k ? " " : "");
    if (n) k += formatar_texto(t + k, sizeof t - (size_t)k, "%s%s", k ? " " : "", host);
    if (r) k += formatar_texto(t + k, sizeof t - (size_t)k, "%s%s", k ? " " : "", ver);
    if (v) k += formatar_texto(t + k, sizeof t - (size_t)k, "%s#imagen-%u", k ? " " : "", iv.imagem_bytes);
    if (m) k += formatar_texto(t + k, sizeof t - (size_t)k, "%si686", k ? " " : "");
    if (op) k += formatar_texto(t + k, sizeof t - (size_t)k, "%sSaguiOS", k ? " " : "");
    saida_linha(t, k);
    return 0;
}
