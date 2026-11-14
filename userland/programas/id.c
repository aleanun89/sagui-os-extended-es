// SPDX-License-Identifier: GPL-2.0-or-later
/* id - identidad del proceso (D232; de verdad desde D255): el uid y el gid que
 * le da el kernel, con los nombres de /etc/usuarios. */
#include "sagui.h"
#include "contas.h"

static const char *AYUDA =
    "uso: id [-u] [-g] [-n]\n"
    "  Sin opciones: uid=1000(sagui) gid=1000(sagui) grupos=1000(sagui).\n"
    "  -u solo el uid   -g solo el gid   -n con -u o -g, el nombre en vez del número\n";

int principal(int argc, char **argv)
{
    bool u = false, g = false, nombre = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "ugnG", &o)) >= 0) {
        switch (r) {
        case 'u': u = true; break;
        case 'g': case 'G': g = true; break;
        case 'n': nombre = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("id", r, &o);
        }
    }
    unsigned uid = meu_uid(), gid = meu_gid();
    char nu[CONTA_NOME_MAX], ng[CONTA_NOME_MAX];
    conta_nome(uid, nu, sizeof nu);
    conta_nome(gid, ng, sizeof ng);   /* cada cuenta tiene su grupo con el mismo número */
    if (u) { if (nombre) saida_linha(nu, -1); else imprimir("%u\n", uid); return 0; }
    if (g) { if (nombre) saida_linha(ng, -1); else imprimir("%u\n", gid); return 0; }
    imprimir("uid=%u(%s) gid=%u(%s) grupos=%u(%s)\n", uid, nu, gid, ng, gid, ng);
    return 0;
}
