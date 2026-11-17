// SPDX-License-Identifier: GPL-3.0-only
/* wg-quick - enciende y apaga la VPN WireGuard (D363): «wg-quick up NOMBRE», «wg-quick
 * down». Todo lo hace /bin/wg (wg.c); este programa solo le pasa las órdenes. */
#include "sagui.h"

int principal(int argc, char **argv)
{
    char *args[8];
    int n = 0;
    args[n++] = "wg";
    args[n++] = "--quick";
    for (int i = 1; i < argc && n < 7; i++) args[n++] = argv[i];
    args[n] = NULL;
    int pid = executar("/bin/wg", args);
    if (pid < 0) { imprimir_em(ERROS, "wg-quick: /bin/wg: %s\n", nome_erro(pid)); return 1; }
    int codigo = 1;
    esperar(pid, &codigo);
    return codigo;
}
