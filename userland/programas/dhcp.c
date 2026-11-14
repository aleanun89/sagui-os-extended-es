// SPDX-License-Identifier: GPL-2.0-or-later
/* dhcp - pide una dirección IP por DHCP y muestra el resultado (D046).
 * Lo mismo que "ifconfig eth0 dhcp". */
#include "sagui.h"

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    imprimir("Pidiendo dirección por DHCP...\n");
    int r = config_rede(REDE_DHCP, NULL);
    if (r < 0) { imprimir_em(ERROS, "dhcp: %s\n", nome_erro(r)); return 1; }
    config_rede_t c;
    config_rede(REDE_CONSULTAR, &c);
    char a[16], b[16], d[16], m[16];
    imprimir("Dirección: %s, máscara %s, puerta de enlace %s, DNS %s\n", ip_para_texto(c.ip, a),
             ip_para_texto(c.mascara, m), c.roteador ? ip_para_texto(c.roteador, b) : "ninguna",
             c.dns ? ip_para_texto(c.dns, d) : "ninguno");
    return 0;
}
