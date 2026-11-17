// SPDX-License-Identifier: GPL-2.0-or-later
/* nslookup (alias host) nombre [servidor[:puerto]] - pregunta al DNS (D046).
 *
 * Sin servidor, usa el del DHCP (y la caché del kernel); con servidor, pregunta
 * directamente a él, por UDP, en el puerto 53 o en el indicado. */
#include "sagui.h"

int principal(int argc, char **argv)
{
    if (argc < 2 || argc > 3 || argv[1][0] == '-') {
        imprimir_em(ERROS, "uso: nslookup <nombre> [servidor[:puerto]]\n");
        return 2;
    }
    uint32_t servidor = 0, ip = 0;
    int porta = 0;
    char t[16], s[24];
    if (argc == 3) {
        char *dois = acha_char(argv[2], ':');
        if (dois) { *dois = 0; porta = texto_para_int(dois + 1); }
        if (resolver(argv[2], &servidor) < 0) { imprimir_em(ERROS, "nslookup: servidor no válido: %s\n", argv[2]); return 2; }
    } else {
        config_rede_t c;
        if (config_rede(REDE_CONSULTAR, &c) == 0) servidor = c.dns;
        static info_wg_t wg;   /* D363: con la VPN encendida y su DNS, se pregunta a ese */
        if (info_sistema(INFO_WG, &wg, sizeof wg) > 0 && wg.config.dns) servidor = wg.config.dns;
    }
    if (porta && porta != 53) formatar_texto(s, sizeof s, "%s#%d", ip_para_texto(servidor, t), porta);
    else ip_para_texto(servidor, s);
    imprimir("Servidor:  %s\n", servidor ? s : "(ninguno)");
    if (texto_para_ip(argv[1], &ip)) {
        imprimir("Nombre:    %s\nDirección: %s\n", argv[1], argv[1]);
        return 0;
    }
    int r = argc == 3 ? resolver_em(argv[1], &ip, servidor, porta) : resolver(argv[1], &ip);
    if (r < 0) {
        imprimir("** No se pudo resolver %s: %s\n", argv[1], r == ERRO_NAO_EXISTE ? "el nombre no existe" : nome_erro(r));
        return 1;
    }
    /* D218: 0.0.0.0 es la respuesta del bloqueo de publicidad por DNS. */
    imprimir("Nombre:    %s\nDirección: %s%s\n", argv[1], ip_para_texto(ip, t),
             ip ? "" : " (bloqueado: publicidad o rastreo; bloqueo estado)");
    return 0;
}
