// SPDX-License-Identifier: GPL-2.0-or-later
/* ifconfig (alias ip) - muestra y configura las interfaces de red (D046, D445).
 *
 *   ifconfig                          cada interfaz: MAC, IP, máscara, router, DNS, contadores
 *   ifconfig -a                       y también las conexiones y la caché ARP
 *   ifconfig eth1                     solo esa interfaz
 *   ifconfig route | ip route         la tabla de rutas (red local de cada interfaz y rutas por defecto)
 *   ifconfig eth0 10.0.2.15/24 gw 10.0.2.2 [dns 10.0.2.3]    dirección estática
 *   ifconfig eth0 dhcp                pide dirección por DHCP (como el comando dhcp)
 *   ifconfig [eth0] dns 1.1.1.1       cambia solo el servidor DNS
 *   ifconfig eth1 metric 50           prioridad de su ruta por defecto (menor = preferida; D445)
 *   ifconfig eth0 experimental        activa el driver experimental (Realtek r8169 o Atheros alc)
 *   ifconfig eth0 down | up           apaga/vuelve a encender la interfaz (D079)
 * Sin nombre de interfaz, las órdenes van a la principal (la de la ruta por defecto). */
#include "sagui.h"

#define MAXIF 8

static const char *nome_metodo(unsigned m) { return m == REDE_POR_DHCP ? "DHCP" : m == REDE_ESTATICO ? "estática" : "ninguna"; }

static void tamanho(char *s, unsigned b)
{
    if (b < 10 * 1024) formatar_texto(s, 24, "%u B", b);
    else if (b < 10 * 1024 * 1024) formatar_texto(s, 24, "%u KB", b / 1024);
    else formatar_texto(s, 24, "%u MB", b / (1024 * 1024));
}

static int prefixo(uint32_t m) { int n = 0; while (m & 0x80000000u) { n++; m <<= 1; } return n; }

/* D445: SIS_CONFIG_REDE con el nombre de la interfaz (NULL = la principal). */
static int config_em(int op, void *c, const char *nome)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SIS_CONFIG_REDE), "b"(op), "c"(c), "d"(nome), "S"(0), "D"(0) : "memory");
    return r;
}

static info_rede_t ifs[MAXIF];
static int nifs;
static void ler_interfaces(void)
{
    nifs = info_sistema(INFO_INTERFACES, ifs, MAXIF);
    if (nifs < 0) nifs = 0;
}
static bool eh_tunel(const info_rede_t *v) { return compara_texto(v->driver, "wireguard") == 0; }
static bool eh_fisica(const info_rede_t *v) { return !v->loopback && !eh_tunel(v); }
static bool eh_wlan(const info_rede_t *v) { return v->wifi || compara_texto(v->nome, "wlan0") == 0; }
/* encendida (con driver y, la wlan0, conectada) */
static bool ligada(const info_rede_t *v) { return eh_fisica(v) && v->pode_desativar; }
static int fisicas_ligadas(void)
{
    int n = 0;
    for (int i = 0; i < nifs; i++) if (ligada(&ifs[i])) n++;
    return n;
}

/* Tarjetas de red del PCI que no son ninguna de las interfaces: con los
 * números se sabe qué driver falta (por ejemplo, en el G470). */
static void otras_tarjetas(void)
{
    static info_pci_t v[64];
    int n = info_sistema(INFO_PCI, v, 64);
    for (int i = 0; i < n; i++) {
        if (v[i].classe != 0x02) continue;
        bool conocida = false;
        for (int k = 0; k < nifs; k++)
            if (eh_fisica(&ifs[k]) && !ifs[k].usb && v[i].fabricante == ifs[k].fabricante && v[i].modelo == ifs[k].modelo &&
                v[i].barramento == ifs[k].barramento && v[i].dispositivo == ifs[k].dispositivo) conocida = true;
        if (conocida) continue;
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

/* La wlan0 sin conectar (D215): la tarjeta, sin dirección. */
static void mostrar_inativa(const info_rede_t *v)
{
    if (eh_tunel(v)) { mostrar_tunel(v); return; }
    imprimir("%s: %s, driver %s\n", v->nome, v->descricao, v->driver[0] ? v->driver : "-");
    imprimir("      MAC %02x:%02x:%02x:%02x:%02x:%02x   sin conectar\n", v->mac[0], v->mac[1], v->mac[2], v->mac[3], v->mac[4],
             v->mac[5]);
    if (eh_wlan(v)) linha_wifi();
}

/* Una interfaz física encendida o detectada sin driver. */
static void mostrar_uma(const info_rede_t *r, bool varias)
{
    char a[16], b[16], c[16];
    imprimir("%s: %s, driver %s%s\n", r->nome, r->descricao, r->driver[0] ? r->driver : "-",
             r->experimental ? " (EXPERIMENTAL)" : "");
    bool wlan = eh_wlan(r);
    if (r->usb) imprimir("      USB (%04x:%04x), ", r->fabricante, r->modelo);
    else imprimir("      PCI %02x:%02x.%x (%04x:%04x), ", r->barramento, r->dispositivo, r->funcao, r->fabricante, r->modelo);
    if (!r->ativa) imprimir("driver desactivado: ifconfig %s experimental\n", r->nome);
    else if (r->irq == 0xFF) imprimir("sin IRQ (sondeo cada 10 ms)\n");
    else imprimir("IRQ %u\n", r->irq);
    imprimir("      MAC %02x:%02x:%02x:%02x:%02x:%02x   enlace: %s%s%s%s\n", r->mac[0], r->mac[1], r->mac[2], r->mac[3],
             r->mac[4], r->mac[5], !r->ativa ? "?" : r->cabo ? "conectado" : wlan ? "sin conectar" : "SIN CABLE",
             r->desativada ? "   DESACTIVADA (ifconfig " : "", r->desativada ? r->nome : "", r->desativada ? " up)" : "");
    if (wlan) linha_wifi();
    if (r->ip) {
        imprimir("      IPv4 %s/%d   máscara %s   (%s", ip_para_texto(r->ip, a), prefixo(r->mascara),
                 ip_para_texto(r->mascara, b), nome_metodo(r->metodo));
        if (r->metodo == REDE_POR_DHCP) imprimir(" de %s, quedan %u s", ip_para_texto(r->servidor_dhcp, c), r->concessao_s);
        imprimir(")\n");
        imprimir("      puerta de enlace %s   DNS %s", r->roteador ? ip_para_texto(r->roteador, a) : "ninguna",
                 r->dns ? ip_para_texto(r->dns, b) : "ninguno");
        if (varias) imprimir("   métrica %u%s", r->metrica, r->rota_padrao ? " (ruta por defecto)" : "");
        imprimir("\n");
    } else if (r->ativa) {
        static const char *e[] = { "parado", "buscando servidor", "pidiendo la dirección", "listo", "renovando" };
        imprimir("      IPv4 sin dirección (DHCP: %s)\n", r->dhcp_estado < 5 ? e[r->dhcp_estado] : "?");
    }
    char t1[24], t2[24];
    tamanho(t1, r->rx_bytes);
    tamanho(t2, r->tx_bytes);
    imprimir("      RX: %u paquetes (%s), %u errores, %u descartados\n", r->rx_pacotes, t1, r->rx_erros, r->descartados);
    imprimir("      TX: %u paquetes (%s), %u errores   interrupciones: %u\n", r->tx_pacotes, t2, r->tx_erros, r->irqs);
    /* D091: diagnóstico del driver experimental (motivos de los descartes,
     * revisión del chip, anillo, contadores de la tarjeta). */
    for (const char *l = r->diagnostico; *l;) {
        char linha[200];
        int k = 0;
        while (*l && *l != '\n') { if (k < (int)sizeof linha - 1) linha[k++] = *l; l++; }
        linha[k] = 0;
        if (*l) l++;
        imprimir("      %s\n", linha);
    }
}

static void mostrar_qualquer(const info_rede_t *v, bool varias)
{
    if (eh_tunel(v)) mostrar_tunel(v);
    else if (eh_wlan(v) && !v->pode_desativar) mostrar_inativa(v);
    else mostrar_uma(v, varias);
}

/* D445: la tabla de rutas, como la usa el kernel: la red local de cada interfaz con dirección y
 * una ruta por defecto por cada una con puerta de enlace; se usa la de métrica menor con enlace. */
static int mostrar_rotas(void)
{
    ler_interfaces();
    char a[16], b[16];
    imprimir("Destino            Puerta de enlace  Interfaz  Métrica\n");
    int n = 0;
    for (int i = 0; i < nifs; i++) {
        const info_rede_t *v = &ifs[i];
        if (v->loopback || !v->ip || (eh_fisica(v) && !v->pode_desativar)) continue;
        char d[24];
        formatar_texto(d, sizeof d, "%s/%d", ip_para_texto(v->ip & v->mascara, a), prefixo(v->mascara));
        imprimir("%-18s %-17s %-9s %s\n", d, "-", v->nome, eh_tunel(v) ? "-" : "0");
        n++;
    }
    for (int i = 0; i < nifs; i++) {
        const info_rede_t *v = &ifs[i];
        if (!ligada(v) || !v->ip || !v->roteador) continue;
        imprimir("%-18s %-17s %-9s %u%s%s\n", "por defecto", ip_para_texto(v->roteador, b), v->nome, v->metrica,
                 v->rota_padrao ? "   <- en uso" : "", v->desativada ? "   (desactivada)" : !v->cabo ? "   (sin enlace)" : "");
        n++;
    }
    imprimir("%-18s %-17s %-9s %s\n", "127.0.0.0/8", "-", "lo", "0");
    if (!n) imprimir("(ninguna interfaz con dirección)\n");
    /* el DNS del sistema: el de la interfaz principal (la de la ruta por defecto) */
    static info_rede_t p;
    config_rede_t c = { 0, 0, 0, 0, 0 };
    if (info_sistema(INFO_REDE, &p, sizeof p) > 0 && config_em(REDE_CONSULTAR, &c, NULL) == 0 && c.dns)
        imprimir("DNS del sistema: %s (%s)\n", ip_para_texto(c.dns, a), p.nome);
    return 0;
}

static int mostrar(bool tudo, const char *so)
{
    ler_interfaces();
    static info_rede_t r;
    int achou = info_sistema(INFO_REDE, &r, sizeof r);
    if (achou < 0) { imprimir_em(ERROS, "ifconfig: %s\n", nome_erro(achou)); return 1; }
    if (so) {   /* una sola interfaz */
        for (int i = 0; i < nifs; i++)
            if (compara_texto(ifs[i].nome, so) == 0) { mostrar_qualquer(&ifs[i], fisicas_ligadas() > 1); return 0; }
        imprimir_em(ERROS, "ifconfig: no hay %s\n", so);
        return 1;
    }
    bool alguma = false;
    for (int i = 0; i < nifs; i++) if (eh_fisica(&ifs[i]) && !(eh_wlan(&ifs[i]) && !ifs[i].pode_desativar)) alguma = true;
    if (!achou || !alguma) {
        imprimir("No hay ninguna tarjeta de red con driver (SaguiOS conoce la Intel e1000 y la Realtek RTL8139).\n");
        otras_tarjetas();
        for (int i = 0; i < nifs; i++) if (!ifs[i].loopback) mostrar_qualquer(&ifs[i], false);
        return 1;
    }
    bool varias = fisicas_ligadas() > 1;
    /* las encendidas (o detectadas) primero, en su orden; luego la wlan0 sin conectar y el túnel */
    for (int i = 0; i < nifs; i++)
        if (eh_fisica(&ifs[i]) && !(eh_wlan(&ifs[i]) && !ifs[i].pode_desativar)) mostrar_uma(&ifs[i], varias);
    otras_tarjetas();
    for (int i = 0; i < nifs; i++)
        if (!ifs[i].loopback && !(eh_fisica(&ifs[i]) && !(eh_wlan(&ifs[i]) && !ifs[i].pode_desativar))) mostrar_qualquer(&ifs[i], varias);
    /* D079: el loopback, que solo tiene contadores. */
    char t1[24], a[16], b[16];
    for (int i = 0; i < nifs; i++)
        if (ifs[i].loopback) {
            tamanho(t1, ifs[i].rx_bytes);
            imprimir("%s: bucle local 127.0.0.1/8   %u paquetes (%s)\n", ifs[i].nome, ifs[i].rx_pacotes, t1);
        }
    if (varias) {   /* D445: por dónde sale lo que no es de ninguna red local */
        bool hay = false;
        for (int i = 0; i < nifs; i++)
            if (ifs[i].rota_padrao) {
                imprimir("Ruta por defecto: %s por %s (métrica %u)\n", ifs[i].nome, ip_para_texto(ifs[i].roteador, a), ifs[i].metrica);
                hay = true;
            }
        if (!hay) imprimir("Ruta por defecto: ninguna\n");
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
        char dev[16] = "";
        if (varias && arp[i].interface < nifs) formatar_texto(dev, sizeof dev, "  %s", ifs[arp[i].interface].nome);
        if (arp[i].completa)
            imprimir("  %-15s  %02x:%02x:%02x:%02x:%02x:%02x  (hace %u s)%s\n", ip_para_texto(arp[i].ip, a), arp[i].mac[0],
                     arp[i].mac[1], arp[i].mac[2], arp[i].mac[3], arp[i].mac[4], arp[i].mac[5], arp[i].idade_s, dev);
        else
            imprimir("  %-15s  (esperando respuesta)%s\n", ip_para_texto(arp[i].ip, a), dev);
    }
    return 0;
}

static int pedir_dhcp(const char *nome)
{
    imprimir("Pidiendo dirección por DHCP%s%s...\n", nome ? " en " : "", nome ? nome : "");
    int r = config_em(REDE_DHCP, NULL, nome);
    if (r < 0) { imprimir_em(ERROS, "dhcp: %s\n", nome_erro(r)); return 1; }
    config_rede_t c;
    config_em(REDE_CONSULTAR, &c, nome);
    char a[16], b[16], d[16];
    imprimir("Dirección: %s/%d, puerta de enlace %s, DNS %s\n", ip_para_texto(c.ip, a), prefixo(c.mascara),
             c.roteador ? ip_para_texto(c.roteador, b) : "ninguna", c.dns ? ip_para_texto(c.dns, d) : "ninguno");
    return 0;
}

static int uso(void)
{
    imprimir_em(ERROS, "uso: ifconfig [-a] | ifconfig IFAZ | ifconfig route\n"
                       "     ifconfig [IFAZ] <ip>[/bits] [netmask <máscara>] [gw <puerta>] [dns <servidor>]\n"
                       "     ifconfig [IFAZ] dhcp | down | up | dns <servidor> | metric <0-9999> | experimental\n"
                       "     ifconfig eth0 r8169 [rxcfg=HEX] [rms=N] [multi=0|1] [buf=N] [reiniciar-rx]\n"
                       "     IFAZ: eth0, eth1, wlan0... (sin ella, la de la ruta por defecto)\n");
    return 1;
}

/* ¿parece un nombre de interfaz? (eth0, eth1, wlan0...) */
static bool nome_interface(const char *s)
{
    return (comeca_com(s, "eth") && s[3] >= '0' && s[3] <= '9') || (comeca_com(s, "wlan") && s[4] >= '0' && s[4] <= '9');
}

int principal(int argc, char **argv)
{
    const char *ifaz = argc >= 2 && nome_interface(argv[1]) ? argv[1] : NULL;
    if (comeca_com(argv[0], "dhcp") || (argc >= 2 && compara_texto(argv[argc - 1], "dhcp") == 0)) return pedir_dhcp(ifaz);
    if (argc == 1) return mostrar(false, NULL);
    if (argc == 2 && (compara_texto(argv[1], "route") == 0 || compara_texto(argv[1], "rutas") == 0 || compara_texto(argv[1], "r") == 0))
        return mostrar_rotas();
    if (argc == 2 && compara_texto(argv[1], "wlan0") == 0) {
        /* D215: la wlan0, conectada o no */
        ler_interfaces();
        for (int k = 0; k < nifs; k++)
            if (compara_texto(ifs[k].nome, "wlan0") == 0) { mostrar_qualquer(&ifs[k], fisicas_ligadas() > 1); return 0; }
        imprimir_em(ERROS, "ifconfig: no hay wlan0 (el pincho Wi-Fi Realtek RTL8188EU no está conectado)\n");
        return 1;
    }
    if (argc == 2 && compara_texto(argv[1], "eth0") == 0) {
        /* con una sola interfaz, «eth0» es esa (como siempre); con varias, solo la eth0 */
        ler_interfaces();
        bool hay = false;
        for (int k = 0; k < nifs; k++) if (compara_texto(ifs[k].nome, "eth0") == 0) hay = true;
        return mostrar(false, hay && fisicas_ligadas() > 1 ? "eth0" : NULL);
    }
    if (argc == 2 && ifaz) return mostrar(false, ifaz);
    if (argc == 2 && (compara_texto(argv[1], "-a") == 0 || compara_texto(argv[1], "addr") == 0 || compara_texto(argv[1], "a") == 0))
        return mostrar(true, NULL);
    int i = 1;
    if (ifaz) i++;
    if (i < argc && compara_texto(argv[i], "route") == 0) return mostrar_rotas();
    if (i < argc && compara_texto(argv[i], "experimental") == 0) {
        imprimir("Activando el driver experimental (no probado en hardware real)...\n");
        int r = config_em(REDE_EXPERIMENTAL, NULL, ifaz);
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
        int r = config_em(REDE_AJUSTAR_DRIVER, &a, ifaz);   /* la syscall recibe el puntero tal cual */
        if (r < 0) { imprimir_em(ERROS, "ifconfig: %s\n", nome_erro(r)); return 1; }
        imprimir("Ajuste aplicado%s. Usa \"dhcp\" y luego \"ip\".\n",
                 (a.mascara & (AJUSTE_REINICIAR | AJUSTE_BUF)) ? " y tarjeta reprogramada" : "");
        return 0;
    }
    if (i < argc && (compara_texto(argv[i], "down") == 0 || compara_texto(argv[i], "up") == 0)) {
        int r = config_em(argv[i][0] == 'd' ? REDE_DESATIVAR : REDE_ATIVAR, NULL, ifaz);
        if (r < 0) { imprimir_em(ERROS, "ifconfig: %s\n", nome_erro(r)); return 1; }
        return mostrar(false, NULL);
    }
    if (i < argc && (compara_texto(argv[i], "metric") == 0 || compara_texto(argv[i], "metrica") == 0)) {
        if (i + 1 >= argc) return uso();
        int m = texto_para_int(argv[i + 1]);
        if (m < 0 || m > 9999) return uso();
        int r = config_em(REDE_METRICA, (void *)(unsigned)m, ifaz);   /* la métrica va en el lugar del puntero */
        if (r < 0) { imprimir_em(ERROS, "ifconfig: %s\n", nome_erro(r)); return 1; }
        return mostrar_rotas();
    }
    if (i < argc && compara_texto(argv[i], "dns") == 0) {
        config_rede_t c = { 0, 0, 0, 0, 0 };
        if (i + 1 >= argc || !texto_para_ip(argv[i + 1], &c.dns)) return uso();
        int r = config_em(REDE_DNS, &c, ifaz);
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
    int r = config_em(REDE_FIXAR, &c, ifaz);
    if (r < 0) { imprimir_em(ERROS, "ifconfig: %s\n", nome_erro(r)); return 1; }
    return mostrar(false, NULL);
}
