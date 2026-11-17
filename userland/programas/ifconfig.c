// SPDX-License-Identifier: GPL-2.0-or-later
/* ifconfig (alias ip) - muestra y configura la placa de red (D046).
 *
 *   ifconfig                          interfaz, MAC, IP, máscara, router, DNS, contadores
 *   ifconfig -a                       y también las conexiones y la caché ARP
 *   ifconfig eth0 10.0.2.15/24 gw 10.0.2.2 [dns 10.0.2.3]    dirección estática
 *   ifconfig eth0 dhcp                pide dirección por DHCP (como el comando dhcp)
 *   ifconfig dns 1.1.1.1              cambia solo el servidor DNS
 *   ifconfig eth0 experimental        activa el driver experimental (Realtek r8169 o Atheros alc)
 *   ifconfig eth0 down | up           apaga/vuelve a encender la interfaz (D079) */
#include "sagui.h"

static const char *nome_metodo(unsigned m) { return m == REDE_POR_DHCP ? "DHCP" : m == REDE_ESTATICO ? "estática" : "ninguna"; }

static void tamanho(char *s, unsigned b)
{
    if (b < 10 * 1024) formatar_texto(s, 24, "%u B", b);
    else if (b < 10 * 1024 * 1024) formatar_texto(s, 24, "%u KB", b / 1024);
    else formatar_texto(s, 24, "%u MB", b / (1024 * 1024));
}

static int prefixo(uint32_t m) { int n = 0; while (m & 0x80000000u) { n++; m <<= 1; } return n; }

/* Tarjetas de red del PCI que no son la que tiene driver: con los números
 * se sabe qué driver falta (por ejemplo, en el G470). */
static void otras_tarjetas(const info_rede_t *r, bool hay)
{
    static info_pci_t v[64];
    int n = info_sistema(INFO_PCI, v, 64);
    for (int i = 0; i < n; i++) {
        if (v[i].classe != 0x02) continue;
        if (hay && v[i].fabricante == r->fabricante && v[i].modelo == r->modelo &&
            v[i].barramento == r->barramento && v[i].dispositivo == r->dispositivo) continue;
        const char *tipo = v[i].subclasse == 0x00 ? "Ethernet" : v[i].subclasse == 0x80 ? "red (¿inalámbrica?)" : "red";
        const char *fab = v[i].fabricante == 0x168C || v[i].fabricante == 0x1969 ? " (Atheros)" :
                          v[i].fabricante == 0x10EC ? " (Realtek)" : v[i].fabricante == 0x8086 ? " (Intel)" :
                          v[i].fabricante == 0x14E4 ? " (Broadcom)" : "";
        imprimir("Sin driver: PCI %02x:%02x.%x %s %04x:%04x%s\n", v[i].barramento, v[i].dispositivo, v[i].funcao,
                 tipo, v[i].fabricante, v[i].modelo, fab);
    }
}

/* D215: el Wi-Fi (pincho RTL8188EU): red, señal y tasa, por SIS_WIFI. */
static void linha_wifi(void)
{
    static info_wifi_t w;
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SIS_WIFI), "b"(WIFI_ESTADO), "c"(&w), "d"(sizeof w), "S"(0), "D"(0) : "memory");
    if (r < 0) return;
    if (w.estado == WIFI_CONECTADO)
        imprimir("      Wi-Fi: %s, canal %u, %d dBm (%u %%), %u Mb/s\n", w.ssid, w.canal, w.sinal, w.qualidade, w.taxa_mbps);
    else imprimir("      Wi-Fi: sin conectar (wifi buscar, wifi conectar SSID)%s%s\n", w.erro[0] ? "; " : "", w.erro);
}

/* La interfaz que no es la activa: la eth0 aparcada mientras la wlan0 está
 * conectada, o la wlan0 sin conectar (D215). */
static void mostrar_tunel(const info_rede_t *v)
{
    /* D363: el túnel WireGuard (wg-quick up): dirección, DNS y el tráfico de dentro. */
    char a[16], b[16], t1[24], t2[24];
    imprimir("%s: %s (wg show)\n", v->nome, v->descricao);
    imprimir("      IPv4 %s/%d   saludo: %s", ip_para_texto(v->ip, a), prefixo(v->mascara), v->cabo ? "sí" : "aún no");
    if (v->dns) imprimir("   DNS %s", ip_para_texto(v->dns, b));
    imprimir("\n");
    tamanho(t1, v->rx_bytes);
    tamanho(t2, v->tx_bytes);
    imprimir("      RX: %u paquetes (%s)   TX: %u paquetes (%s), %u descartados\n", v->rx_pacotes, t1, v->tx_pacotes, t2,
             v->descartados);
}

static void mostrar_inativa(const info_rede_t *v)
{
    if (compara_texto(v->driver, "wireguard") == 0) { mostrar_tunel(v); return; }
    imprimir("%s: %s, driver %s\n", v->nome, v->descricao, v->driver[0] ? v->driver : "-");
    imprimir("      MAC %02x:%02x:%02x:%02x:%02x:%02x   %s\n", v->mac[0], v->mac[1], v->mac[2], v->mac[3], v->mac[4], v->mac[5],
             compara_texto(v->nome, "wlan0") == 0 ? "sin conectar" : "en espera (la wlan0 es la interfaz activa)");
    if (compara_texto(v->nome, "wlan0") == 0) linha_wifi();
}

static void outras_interfaces(const char *ativa)
{
    static info_rede_t ifs[5];
    int ni = info_sistema(INFO_INTERFACES, ifs, 5);
    for (int i = 0; i < ni; i++)
        if (!ifs[i].loopback && compara_texto(ifs[i].nome, ativa) != 0) mostrar_inativa(&ifs[i]);
}

static int mostrar(bool tudo)
{
    static info_rede_t r;
    int achou = info_sistema(INFO_REDE, &r, sizeof r);
    if (achou < 0) { imprimir_em(ERROS, "ifconfig: %s\n", nome_erro(achou)); return 1; }
    if (!achou) {
        imprimir("No hay ninguna tarjeta de red con driver (SaguiOS conoce la Intel e1000 y la Realtek RTL8139).\n");
        otras_tarjetas(&r, false);
        outras_interfaces("");
        return 1;
    }
    char a[16], b[16], c[16];
    imprimir("%s: %s, driver %s%s\n", r.nome, r.descricao, r.driver[0] ? r.driver : "-",
             r.experimental ? " (EXPERIMENTAL)" : "");
    bool wlan = compara_texto(r.nome, "wlan0") == 0;
    if (wlan) imprimir("      USB (%04x:%04x), ", r.fabricante, r.modelo);
    else imprimir("      PCI %02x:%02x.%x (%04x:%04x), ", r.barramento, r.dispositivo, r.funcao, r.fabricante, r.modelo);
    if (!r.ativa) imprimir("driver desactivado: ifconfig %s experimental\n", r.nome);
    else if (r.irq == 0xFF) imprimir("sin IRQ (sondeo cada 10 ms)\n");
    else imprimir("IRQ %u\n", r.irq);
    imprimir("      MAC %02x:%02x:%02x:%02x:%02x:%02x   enlace: %s%s\n", r.mac[0], r.mac[1], r.mac[2], r.mac[3], r.mac[4],
             r.mac[5], !r.ativa ? "?" : r.cabo ? "conectado" : wlan ? "sin conectar" : "SIN CABLE",
             r.desativada ? "   DESACTIVADA (ifconfig eth0 up)" : "");
    if (wlan) linha_wifi();
    if (r.ip) {
        imprimir("      IPv4 %s/%d   máscara %s   (%s", ip_para_texto(r.ip, a), prefixo(r.mascara),
                 ip_para_texto(r.mascara, b), nome_metodo(r.metodo));
        if (r.metodo == REDE_POR_DHCP) imprimir(" de %s, quedan %u s", ip_para_texto(r.servidor_dhcp, c), r.concessao_s);
        imprimir(")\n");
        imprimir("      puerta de enlace %s   DNS %s\n", r.roteador ? ip_para_texto(r.roteador, a) : "ninguna",
                 r.dns ? ip_para_texto(r.dns, b) : "ninguno");
    } else if (r.ativa) {
        static const char *e[] = { "parado", "buscando servidor", "pidiendo la dirección", "listo", "renovando" };
        imprimir("      IPv4 sin dirección (DHCP: %s)\n", r.dhcp_estado < 5 ? e[r.dhcp_estado] : "?");
    }
    char t1[24], t2[24];
    tamanho(t1, r.rx_bytes);
    tamanho(t2, r.tx_bytes);
    imprimir("      RX: %u paquetes (%s), %u errores, %u descartados\n", r.rx_pacotes, t1, r.rx_erros, r.descartados);
    imprimir("      TX: %u paquetes (%s), %u errores   interrupciones: %u\n", r.tx_pacotes, t2, r.tx_erros, r.irqs);
    /* D091: diagnóstico del driver experimental (motivos de los descartes,
     * revisión del chip, anillo, contadores de la tarjeta). */
    for (const char *l = r.diagnostico; *l;) {
        char linha[200];
        int k = 0;
        while (*l && *l != '\n') { if (k < (int)sizeof linha - 1) linha[k++] = *l; l++; }
        linha[k] = 0;
        if (*l) l++;
        imprimir("      %s\n", linha);
    }
    otras_tarjetas(&r, true);
    outras_interfaces(r.nome);
    /* D079: el loopback, que solo tiene contadores. */
    static info_rede_t ifs[5];
    int ni = info_sistema(INFO_INTERFACES, ifs, 5);
    for (int i = 0; i < ni; i++)
        if (ifs[i].loopback) {
            tamanho(t1, ifs[i].rx_bytes);
            imprimir("%s: bucle local 127.0.0.1/8   %u paquetes (%s)\n", ifs[i].nome, ifs[i].rx_pacotes, t1);
        }
    if (!tudo) return 0;

    static info_conexao_t v[32];
    int n = info_sistema(INFO_CONEXOES, v, 32);
    imprimir("\nConexiones:\n");
    if (n <= 0) imprimir("  (ninguna)\n");
    else imprimir("  Proto  Local                  Remoto                 Estado        RX     TX  PID\n");
    static const char *est[] = { "CERRADA", "ESCUCHANDO", "SYN_ENVIADO", "SYN_RECIBIDO", "ESTABLECIDA", "CERRANDO", "-" };
    for (int i = 0; i < n; i++) {
        char l[24], rm[24];
        formatar_texto(l, sizeof l, "%s:%u", ip_para_texto(v[i].ip_local, a), v[i].porta_local);
        if (v[i].ip_remoto) formatar_texto(rm, sizeof rm, "%s:%u", ip_para_texto(v[i].ip_remoto, b), v[i].porta_remota);
        else copia_texto(rm, "*", sizeof rm);
        imprimir("  %-5s  %-21s  %-21s  %-12s %5u %5u  %d\n", v[i].tipo == SOCK_TCP ? "tcp" : "udp", l, rm,
                 v[i].estado < 7 ? est[v[i].estado] : "?", v[i].na_fila_rx, v[i].na_fila_tx, v[i].pid);
    }
    static info_arp_t arp[32];
    n = info_sistema(INFO_ARP, arp, 32);
    imprimir("\nCaché ARP:\n");
    if (n <= 0) imprimir("  (vacía)\n");
    for (int i = 0; i < n; i++) {
        if (arp[i].completa)
            imprimir("  %-15s  %02x:%02x:%02x:%02x:%02x:%02x  (hace %u s)\n", ip_para_texto(arp[i].ip, a), arp[i].mac[0],
                     arp[i].mac[1], arp[i].mac[2], arp[i].mac[3], arp[i].mac[4], arp[i].mac[5], arp[i].idade_s);
        else
            imprimir("  %-15s  (esperando respuesta)\n", ip_para_texto(arp[i].ip, a));
    }
    return 0;
}

static int pedir_dhcp(void)
{
    imprimir("Pidiendo dirección por DHCP...\n");
    int r = config_rede(REDE_DHCP, NULL);
    if (r < 0) { imprimir_em(ERROS, "dhcp: %s\n", nome_erro(r)); return 1; }
    config_rede_t c;
    config_rede(REDE_CONSULTAR, &c);
    char a[16], b[16], d[16];
    imprimir("Dirección: %s/%d, puerta de enlace %s, DNS %s\n", ip_para_texto(c.ip, a), prefixo(c.mascara),
             c.roteador ? ip_para_texto(c.roteador, b) : "ninguna", c.dns ? ip_para_texto(c.dns, d) : "ninguno");
    return 0;
}

static int uso(void)
{
    imprimir_em(ERROS, "uso: ifconfig [-a]\n"
                       "     ifconfig eth0 <ip>[/bits] [netmask <máscara>] [gw <puerta>] [dns <servidor>]\n"
                       "     ifconfig eth0 dhcp | ifconfig eth0 down|up | ifconfig dns <servidor> | ifconfig eth0 experimental\n"
                       "     ifconfig eth0 r8169 [rxcfg=HEX] [rms=N] [multi=0|1] [buf=N] [reiniciar-rx]\n");
    return 1;
}

int principal(int argc, char **argv)
{
    if (comeca_com(argv[0], "dhcp") || (argc >= 2 && compara_texto(argv[argc - 1], "dhcp") == 0)) return pedir_dhcp();
    if (argc == 1) return mostrar(false);
    if (argc == 2 && compara_texto(argv[1], "wlan0") == 0) {
        /* D215: la wlan0 es la activa (conectada) o una de las otras */
        static info_rede_t r;
        if (info_sistema(INFO_REDE, &r, sizeof r) > 0 && compara_texto(r.nome, "wlan0") == 0) return mostrar(false);
        static info_rede_t ifs[5];
        int ni = info_sistema(INFO_INTERFACES, ifs, 5);
        for (int k = 0; k < ni; k++)
            if (compara_texto(ifs[k].nome, "wlan0") == 0) { mostrar_inativa(&ifs[k]); return 0; }
        imprimir_em(ERROS, "ifconfig: no hay wlan0 (el pincho Wi-Fi Realtek RTL8188EU no está conectado)\n");
        return 1;
    }
    if (argc == 2 && (compara_texto(argv[1], "-a") == 0 || compara_texto(argv[1], "eth0") == 0 ||
                      compara_texto(argv[1], "addr") == 0 || compara_texto(argv[1], "a") == 0))
        return mostrar(compara_texto(argv[1], "-a") == 0 || argv[1][0] == 'a');
    int i = 1;
    if (compara_texto(argv[i], "eth0") == 0 || compara_texto(argv[i], "wlan0") == 0) i++;
    if (i < argc && compara_texto(argv[i], "experimental") == 0) {
        imprimir("Activando el driver experimental (no probado en hardware real)...\n");
        int r = config_rede(REDE_EXPERIMENTAL, NULL);
        if (r < 0) { imprimir_em(ERROS, "ifconfig: %s\n", nome_erro(r)); return 1; }
        imprimir("Driver activado. Usa \"dhcp\" o una dirección estática.\n");
        return 0;
    }
    if (i < argc && compara_texto(argv[i], "r8169") == 0) {
        /* D124: ajustes en caliente del driver r8169 para probar variantes:
         * rxcfg=HEX rms=N multi=0|1 buf=N reiniciar-rx */
        ajuste_driver_t a = { 0, 0, 0, 0, 0 };
        for (i++; i < argc; i++) {
            const char *x = argv[i];
            if (comeca_com(x, "rxcfg=")) {
                unsigned v = 0;
                for (const char *c = x + 6; *c; c++) {
                    int h = (*c >= '0' && *c <= '9') ? *c - '0' : (*c >= 'a' && *c <= 'f') ? *c - 'a' + 10 :
                            (*c >= 'A' && *c <= 'F') ? *c - 'A' + 10 : -1;
                    if (*c == 'x' || *c == 'X') { v = 0; continue; }
                    if (h < 0) return uso();
                    v = v * 16 + (unsigned)h;
                }
                a.rxcfg = v; a.mascara |= AJUSTE_RXCFG;
            } else if (comeca_com(x, "rms=")) { a.rms = (unsigned)texto_para_int(x + 4); a.mascara |= AJUSTE_RMS; }
            else if (comeca_com(x, "multi=")) { a.multi = x[6] == '1'; a.mascara |= AJUSTE_MULTI; }
            else if (comeca_com(x, "buf=")) { a.buf = (unsigned)texto_para_int(x + 4); a.mascara |= AJUSTE_BUF; }
            else if (compara_texto(x, "reiniciar-rx") == 0) a.mascara |= AJUSTE_REINICIAR;
            else return uso();
        }
        int r = config_rede(REDE_AJUSTAR_DRIVER, (config_rede_t *)&a);   /* la syscall recibe el puntero tal cual */
        if (r < 0) { imprimir_em(ERROS, "ifconfig: %s\n", nome_erro(r)); return 1; }
        imprimir("Ajuste aplicado%s. Usa \"dhcp\" y luego \"ip\".\n",
                 (a.mascara & (AJUSTE_REINICIAR | AJUSTE_BUF)) ? " y tarjeta reprogramada" : "");
        return 0;
    }
    if (i < argc && (compara_texto(argv[i], "down") == 0 || compara_texto(argv[i], "up") == 0)) {
        int r = config_rede(argv[i][0] == 'd' ? REDE_DESATIVAR : REDE_ATIVAR, NULL);
        if (r < 0) { imprimir_em(ERROS, "ifconfig: %s\n", nome_erro(r)); return 1; }
        return mostrar(false);
    }
    if (i < argc && compara_texto(argv[i], "dns") == 0) {
        config_rede_t c = { 0, 0, 0, 0, 0 };
        if (i + 1 >= argc || !texto_para_ip(argv[i + 1], &c.dns)) return uso();
        int r = config_rede(REDE_DNS, &c);
        if (r < 0) { imprimir_em(ERROS, "ifconfig: %s\n", nome_erro(r)); return 1; }
        return 0;
    }
    if (i >= argc) return uso();
    config_rede_t c = { 0, 0xFFFFFF00u, 0, 0, 0 };
    char ip[32];
    copia_texto(ip, argv[i++], sizeof ip);
    char *barra = acha_char(ip, '/');
    if (barra) {
        *barra = 0;
        int bits = texto_para_int(barra + 1);
        if (bits < 1 || bits > 30) return uso();
        c.mascara = 0xFFFFFFFFu << (32 - bits);
    }
    if (!texto_para_ip(ip, &c.ip)) return uso();
    for (; i + 1 < argc; i += 2) {
        uint32_t v;
        if (!texto_para_ip(argv[i + 1], &v)) return uso();
        if (compara_texto(argv[i], "gw") == 0 || compara_texto(argv[i], "via") == 0) c.roteador = v;
        else if (compara_texto(argv[i], "netmask") == 0) c.mascara = v;
        else if (compara_texto(argv[i], "dns") == 0) c.dns = v;
        else return uso();
    }
    if (i < argc) return uso();
    int r = config_rede(REDE_FIXAR, &c);
    if (r < 0) { imprimir_em(ERROS, "ifconfig: %s\n", nome_erro(r)); return 1; }
    return mostrar(false);
}
