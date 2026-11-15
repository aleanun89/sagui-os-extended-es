// SPDX-License-Identifier: GPL-2.0-or-later
/* hostname - nombre de la máquina (D097). La variable HOSTNAME; si no, /etc/hostname (lo escribe el
 * instalador en SAGUI-CONF, D318, y se copia a /etc al arrancar); si no, «sagui». */
#include "sagui.h"

int principal(int argc, char **argv)
{
    if (argc > 1 && (compara_texto(argv[1], "--help") == 0 || compara_texto(argv[1], "--ayuda") == 0 || compara_texto(argv[1], "-h") == 0)) {
        escrever_texto(SAIDA, "uso: hostname\nMuestra el nombre de la máquina: la variable HOSTNAME si existe; si no, /etc/hostname; si no, «sagui».\n");
        return 0;
    }
    if (argc > 1) { imprimir_em(ERROS, "hostname: SaguiOS no permite cambiar el nombre (define HOSTNAME en /etc/sagshrc)\n"); return 1; }
    const char *h = obter_env("HOSTNAME");
    char b[64] = "";
    if (!h || !*h) {   /* D318 */
        int fd = abrir("/etc/hostname", ABRIR_LER);
        if (fd >= 0) {
            int n = ler(fd, b, sizeof b - 1);
            fechar(fd);
            b[n > 0 ? n : 0] = 0;
            for (int i = 0; b[i]; i++) if (b[i] == '\n' || b[i] == '\r' || b[i] == ' ') { b[i] = 0; break; }
            h = b;
        }
    }
    saida_linha(h && *h ? h : "sagui", -1);
    return 0;
}
