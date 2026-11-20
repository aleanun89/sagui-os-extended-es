// SPDX-License-Identifier: GPL-2.0-or-later
/* wifi - el Wi-Fi con el pincho Realtek RTL8188EU (D215, D220).
 *
 *   wifi [estado]               estado del adaptador y de la conexión
 *   wifi buscar                 redes cercanas: SSID, canal, señal y seguridad
 *   wifi conectar SSID [clave]  conecta (abierta, WEP, WPA, WPA2 o WPA3) y la recuerda
 *   wifi desconectar
 *   wifi recordadas             las redes recordadas
 *   wifi olvidar SSID
 *   wifi compatibles [VID:PID]  qué pinchos USB reconoce el sistema; con un VID:PID (hex), de qué familia es (D409)
 *   wifi --ventana              la ventana de redes (la que abre el icono Wi-Fi de la bandeja, D354)
 *
 * Conectar y desconectar no piden ser root: vale la sesión local (D354). Las redes recordadas son
 * de cada usuario (su /home/.config/wifi.conf).
 *
 * La contraseña nunca llega al kernel ni al disco: aquí se calcula lo que hace
 * falta y eso es lo que se manda y se guarda en /home/.config/wifi.conf, una
 * línea por red:
 *   rede <SSID en hex> <PMK en hex | aberta | -> [wep=<hex>] [pt=<hex>] [pwe=<BSSID>:<hex>]...
 *   - WPA, WPA2 (y WPA3 en transición): la PMK, PBKDF2-SHA1 con el SSID (D215);
 *   - WPA3 (SAE, D220): el PT del hash-to-element (de la contraseña y el SSID)
 *     y, para los puntos de acceso que hacen «cazar y picotear», el PWE de
 *     cada uno (de la contraseña y las dos MAC). SAE no admite una PMK fija:
 *     cada conexión saca una nueva del intercambio;
 *   - WEP: la clave misma (WEP no tiene nada derivado: la clave es lo que cifra).
 * El kernel lee ese archivo para conectarse solo a las redes recordadas. */
#include "sagui.h"
#include "wpa.h"
#include "sae.h"
#include "gui.h"

#define CONFIG "/home/.config/wifi.conf"
#define MAX_LINHAS 32
#define TAM_LINHA  1024

static bool simulado;   /* --simulado: redes de mentira para probar la ventana sin adaptador (D354) */
static int wifi_sim(int op, void *p, unsigned n);

static int wifi(int op, void *p, unsigned n)
{
    int r;
    if (simulado) return wifi_sim(op, p, n);
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SIS_WIFI), "b"(op), "c"(p), "d"(n), "S"(0), "D"(0) : "memory");
    return r;
}

/* Avisos y errores: en la terminal salen por la salida; en la ventana se guardan para mostrarlos. */
static bool modo_gui;
static char msg_gui[160];
static void avisa(bool erro, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (modo_gui) formatar_lista(msg_gui, sizeof msg_gui, fmt, ap);
    else {
        char t[200];
        formatar_lista(t, sizeof t, fmt, ap);
        if (erro) imprimir_em(ERROS, "%s", t); else imprimir("%s", t);
    }
    va_end(ap);
}

/* ¿«t» contiene «frag»? */
static int contem_texto(const char *t, const char *frag)
{
    for (; *t; t++) {
        const char *a = t, *b = frag;
        while (*a && *b && *a == *b) { a++; b++; }
        if (!*b) return 1;
    }
    return 0;
}

static const char *nome_estado(unsigned e)
{
    static const char *const n[] = { "sin adaptador", "iniciando", "listo, sin conectar", "conectando",
                                     "comprobando la clave", "conectado", "error", "sin firmware" };
    return e < sizeof n / sizeof n[0] ? n[e] : "?";
}

/* La seguridad exacta: WEP, WPA-TKIP, WPA2, WPA2/WPA3, WPA3... */
static const char *nome_seg(unsigned s, unsigned det)
{
    bool tkip = (det & WIFI_DET_TKIP) != 0;
    switch (s) {
    case WIFI_ABERTA: return "abierta";
    case WIFI_WEP: return "WEP (insegura)";
    case WIFI_WPA1: return tkip ? "WPA-TKIP" : "WPA-AES";
    case WIFI_WPA2: return tkip ? "WPA2 (TKIP)" : "WPA2";
    case WIFI_OUTRA: return "Enterprise/otra";
    case WIFI_WPA3: return "WPA3";
    case WIFI_WPA2_WPA3: return "WPA2/WPA3";
    case WIFI_WPA1_WPA2: return "WPA/WPA2";
    }
    return "?";
}

static void hex(char *o, const uint8_t *p, int n)
{
    static const char d[] = "0123456789abcdef";
    for (int i = 0; i < n; i++) { o[2 * i] = d[p[i] >> 4]; o[2 * i + 1] = d[p[i] & 15]; }
    o[2 * n] = 0;
}

static int hexval(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

/* n bytes de hexadecimal; false si no lo son. */
static bool de_hex(const char *s, uint8_t *o, int n)
{
    for (int k = 0; k < n; k++) {
        int a = hexval(s[2 * k]), b = a >= 0 ? hexval(s[2 * k + 1]) : -1;
        if (a < 0 || b < 0) return false;
        o[k] = (uint8_t)(a << 4 | b);
    }
    return true;
}

/* ------------------------------------------------------------ wifi.conf */

static char linhas[MAX_LINHAS][TAM_LINHA];
static int nlinhas;

static void ler_config(void)
{
    nlinhas = 0;
    int fd = abrir(CONFIG, ABRIR_LER);
    if (fd < 0) return;
    static char l[TAM_LINHA + 64];
    while (nlinhas < MAX_LINHAS && ler_linha(fd, l, sizeof l) >= 0)
        if (comeca_com(l, "rede ")) copia_texto(linhas[nlinhas++], l, sizeof linhas[0]);
    wpa_apagar(l, sizeof l);
    fechar(fd);
}

static int gravar_config(void)
{
    criar_pasta("/home/.config");
    int fd = abrir(CONFIG, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    escrever_texto(fd, "# Redes Wi-Fi recordadas (programa wifi, D215 y D220): SSID, PMK, clave WEP y PT/PWE de SAE en hexadecimal.\n");
    for (int i = 0; i < nlinhas; i++) { escrever_texto(fd, linhas[i]); escrever_texto(fd, "\n"); }
    fechar(fd);
    return 0;
}

static int achar(const char *ssid)
{
    char h[70];
    hex(h, (const uint8_t *)ssid, (int)tam_texto(ssid));
    for (int i = 0; i < nlinhas; i++) {
        size_t n = tam_texto(h);
        if (!compara_texto_n(linhas[i] + 5, h, n) && linhas[i][5 + n] == ' ') return i;
    }
    return -1;
}

/* La línea de una red -> pedido (lo mismo que lee el kernel). */
static bool ler_recordada(const char *ssid, pedido_wifi_t *p, bool *aberta)
{
    int i = achar(ssid);
    if (i < 0) return false;
    const char *l = linhas[i] + 5;
    while (*l && *l != ' ') l++;                       /* el SSID */
    if (*l) l++;
    *aberta = comeca_com(l, "aberta");
    if (de_hex(l, p->pmk, 32) && (l[64] == ' ' || !l[64])) p->tem_pmk = 1;
    for (const char *t = acha_char(l, ' '); t; t = acha_char(t + 1, ' ')) {
        const char *v = t + 1;
        if (comeca_com(v, "wep=")) {
            int n = 0;
            while (n < 13 && hexval(v[4 + 2 * n]) >= 0 && hexval(v[5 + 2 * n]) >= 0) n++;
            if ((n == 5 || n == 13) && de_hex(v + 4, p->wep, n)) p->nwep = (unsigned char)n;
        } else if (comeca_com(v, "pt=")) {
            if (de_hex(v + 3, p->pt, 64)) p->tem_pt = 1;
        } else if (comeca_com(v, "pwe=") && p->npwe < WIFI_MAX_PWE && tam_texto(v) >= 17 + 128 && v[16] == ':') {
            pwe_wifi_t *w = &p->pwe[p->npwe];
            if (de_hex(v + 4, w->bssid, 6) && de_hex(v + 17, w->pwe, 64)) p->npwe++;
        }
    }
    return true;
}

static void recordar(const char *ssid, const pedido_wifi_t *p)
{
    ler_config();
    int i = achar(ssid);
    if (i < 0) {
        if (nlinhas == MAX_LINHAS) { for (int k = 1; k < nlinhas; k++) copia_texto(linhas[k - 1], linhas[k], sizeof linhas[0]); nlinhas--; }
        i = nlinhas++;
    }
    static char h[TAM_LINHA], t[160];
    hex(t, (const uint8_t *)ssid, (int)tam_texto(ssid));
    formatar_texto(h, sizeof h, "rede %s ", t);
    bool aberta = !p->tem_pmk && !p->nwep && !p->tem_pt && !p->npwe;
    if (p->tem_pmk) { hex(t, p->pmk, 32); copia_texto(h + tam_texto(h), t, sizeof h - tam_texto(h)); }
    else copia_texto(h + tam_texto(h), aberta ? "aberta" : "-", sizeof h - tam_texto(h));
    if (p->nwep) {
        hex(t, p->wep, p->nwep);
        formatar_texto(h + tam_texto(h), sizeof h - tam_texto(h), " wep=%s", t);
    }
    if (p->tem_pt) {
        hex(t, p->pt, 64);
        formatar_texto(h + tam_texto(h), sizeof h - tam_texto(h), " pt=%s", t);
    }
    for (int k = 0; k < p->npwe && k < WIFI_MAX_PWE; k++) {
        char b[16];
        hex(b, p->pwe[k].bssid, 6);
        hex(t, p->pwe[k].pwe, 64);
        formatar_texto(h + tam_texto(h), sizeof h - tam_texto(h), " pwe=%s:%s", b, t);
    }
    copia_texto(linhas[i], h, sizeof linhas[0]);
    if (gravar_config() < 0) imprimir_em(ERROS, "wifi: no se pudo guardar %s\n", CONFIG);
    wpa_apagar(h, sizeof h);
    wpa_apagar(t, sizeof t);
    wpa_apagar(linhas, sizeof linhas);
}

/* ------------------------------------------------------------ órdenes */

static int estado(void)
{
    info_wifi_t v;
    int r = wifi(WIFI_ESTADO, &v, sizeof v);
    if (r < 0) {
        imprimir("Wi-Fi: no hay ningún adaptador (el pincho Realtek RTL8188EU 0bda:8179)\n");
        return 1;
    }
    imprimir("Adaptador: %s\n", v.chip[0] ? v.chip : "Realtek RTL8188EU");
    if (contem_texto(v.chip, "experimental"))   /* D409: RTL8188CU/8192CU y RT2870/RT3070/RT5370 */
        avisa(false, _("AVISO: este pincho es experimental: el driver se escribió sin tener el dispositivo y no se ha probado en hardware (D409).\n"));
    imprimir("MAC:       %02x:%02x:%02x:%02x:%02x:%02x\n", v.mac[0], v.mac[1], v.mac[2], v.mac[3], v.mac[4], v.mac[5]);
    if (v.firmware[0]) imprimir("Firmware:  %s\n", v.firmware);
    imprimir("Estado:    %s\n", nome_estado(v.estado));
    if (v.estado >= WIFI_CONECTANDO && v.estado <= WIFI_CONECTADO) {
        imprimir("Red:       %s (canal %u, %s%s%s)\n", v.ssid, v.canal, nome_seg(v.seguranca, v.detalhe),
                 v.detalhe & WIFI_DET_PMF ? ", 802.11w" : "", v.detalhe & WIFI_DET_H2E ? ", H2E" : "");
        imprimir("Señal:     %d dBm (%u %%)\n", v.sinal, v.qualidade);
        if (v.seguranca == WIFI_WEP)
            imprimir("AVISO:     WEP no protege nada: cualquiera cerca puede leer y falsificar el tráfico.\n");
        else if (v.detalhe & WIFI_DET_TKIP)
            imprimir("AVISO:     TKIP está obsoleto; mejor WPA2-AES o WPA3 en el router.\n");
    }
    if (v.estado == WIFI_CONECTADO) {
        /* D238: la tasa con decimales y, con 802.11n, el MCS */
        if (v.mcs != 255)
            imprimir("Tasa:      MCS %u, %u,%u Mb/s (802.11n, 20 MHz%s)\n", v.mcs, v.taxa_dec / 10, v.taxa_dec % 10,
                     v.sgi ? ", GI corto" : "");
        else if (v.taxa_dec % 10)
            imprimir("Tasa:      %u,%u Mb/s (%s)\n", v.taxa_dec / 10, v.taxa_dec % 10, v.ht == 2 ? "b/g; la red es 802.11n" : "b/g");
        else imprimir("Tasa:      %u Mb/s (%s)\n", v.taxa_dec / 10, v.ht == 2 ? "b/g; la red es 802.11n" : "b/g");
        if (v.taxa_rx_dec) {
            if (v.mcs_rx != 255) imprimir("Recibe a:  MCS %u (%u,%u Mb/s)\n", v.mcs_rx, v.taxa_rx_dec / 10, v.taxa_rx_dec % 10);
            else imprimir("Recibe a:  %u,%u Mb/s\n", v.taxa_rx_dec / 10, v.taxa_rx_dec % 10);
        }
        imprimir("Conectado: hace %u s\n", v.conectado_s);
        config_rede_t c;
        char ip[16];
        if (config_rede(REDE_CONSULTAR, &c) == 0 && c.ip) imprimir("IP:        %s (wlan0)\n", ip_para_texto(c.ip, ip));
    }
    imprimir("Tramas:    %u recibidas, %u enviadas, %u balizas de la red, %u respuestas a nuestros sondeos\n",
             v.rx_quadros, v.tx_quadros, v.balizas, v.respostas_sondeo);
    /* D238: para medir el rendimiento (también en el registro serie, [WIFI] cada 10 s con tráfico) */
    imprimir("Datos:     %u KB recibidos, %u KB enviados\n", v.rx_bytes / 1024, v.tx_bytes / 1024);
    imprimir("USB:       %u lecturas (cola de %u, %u en vuelo, hasta %u tramas por lectura), "
             "%u envíos (cola de %u, %u en vuelo)\n",
             v.rx_usb, v.rx_cola, v.rx_en_vuelo, v.rx_agregadas_max, v.tx_usb, v.tx_cola, v.tx_en_vuelo);
    if (v.tx_cola_llena || v.tx_perdidas)
        imprimir("Envío:     cola llena %u veces, %u tramas perdidas\n", v.tx_cola_llena, v.tx_perdidas);
    if (v.addba_rechazados) imprimir("Block Ack: %u pedidos rechazados (sin A-MPDU)\n", v.addba_rechazados);
    if (v.erros_usb) imprimir("Errores USB: %u\n", v.erros_usb);
    if (v.erro[0]) imprimir("Último aviso: %s\n", v.erro);
    return v.estado == WIFI_FALHOU || v.estado == WIFI_SEM_FIRMWARE ? 1 : 0;
}

static bool esperar_pronto(unsigned ms)
{
    info_wifi_t v;
    for (unsigned t = 0; t < ms; t += 200) {
        if (wifi(WIFI_ESTADO, &v, sizeof v) < 0) return false;
        if (v.estado != WIFI_INICIANDO) return v.estado != WIFI_FALHOU && v.estado != WIFI_SEM_FIRMWARE;
        dormir(200);
    }
    return false;
}

static rede_wifi_t redes[32];

/* Pide una búsqueda y deja el resultado en «redes». */
static int buscar_redes(void)
{
    int r = wifi(WIFI_BUSCAR, 0, 0);
    if (r < 0) return r;
    info_wifi_t v;
    dormir(300);
    for (int t = 0; t < 100; t++) {
        if (wifi(WIFI_ESTADO, &v, sizeof v) < 0 || !v.buscando) break;
        dormir(200);
    }
    return wifi(WIFI_REDES, redes, 32);
}

static int buscar(void)
{
    if (!esperar_pronto(20000)) { estado(); return 1; }
    imprimir("Buscando redes (13 canales)...\n");
    int n = buscar_redes();
    if (n < 0) { imprimir_em(ERROS, "wifi: %s\n", nome_erro(n)); return 1; }
    if (n == 0) { imprimir("No se ha encontrado ninguna red.\n"); return 0; }
    /* de la más fuerte a la más débil */
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && redes[j].sinal > redes[j - 1].sinal; j--) {
            rede_wifi_t x = redes[j]; redes[j] = redes[j - 1]; redes[j - 1] = x;
        }
    imprimir("%-32s %5s %8s %5s  %s\n", "SSID", "Canal", "Señal", "%", "Seguridad");
    for (int i = 0; i < n; i++)
        imprimir("%-32s %5u %4d dBm %4u%%  %s%s\n", redes[i].oculta ? "(oculta)" : redes[i].ssid, redes[i].canal,
                 redes[i].sinal, redes[i].qualidade, nome_seg(redes[i].seguranca, redes[i].detalhe),
                 redes[i].suportada ? "" : " (no admitida)");
    imprimir("%d redes\n", n);
    return 0;
}

/* La clave WEP: 5 o 13 caracteres (ASCII) o 10 o 26 hexadecimales. */
static int clave_wep(const char *s, uint8_t *o)
{
    size_t n = tam_texto(s);
    if (n == 10 || n == 26) return de_hex(s, o, (int)n / 2) ? (int)n / 2 : -1;
    if (n == 5 || n == 13) { memcpy(o, s, n); return (int)n; }
    return -1;
}

static bool tem_sae(unsigned s) { return s == WIFI_WPA3 || s == WIFI_WPA2_WPA3; }
static bool tem_psk(unsigned s) { return s == WIFI_WPA1 || s == WIFI_WPA2 || s == WIFI_WPA2_WPA3 || s == WIFI_WPA1_WPA2; }

/* Lo que se manda al kernel a partir de la contraseña (que no sale de aquí). */
static int preparar(pedido_wifi_t *p, const char *ssid, const char *senha, int k, int n)
{
    unsigned s = redes[k].seguranca;
    uint32_t nssid = (uint32_t)tam_texto(ssid), nsenha = (uint32_t)tam_texto(senha);
    if (s == WIFI_WEP) {
        int nw = clave_wep(senha, p->wep);
        if (nw < 0) {
            avisa(true, _("wifi: la clave WEP tiene 5 o 13 caracteres, o 10 o 26 cifras hexadecimales\n"));
            return 2;
        }
        p->nwep = (unsigned char)nw;
        avisa(false, _("AVISO: %s usa WEP, que no protege nada: cualquiera cerca puede leer y falsificar el tráfico. Cámbialo a WPA2-AES o WPA3.\n"), ssid);
        return 0;
    }
    if (tem_psk(s)) {
        if (wpa_pmk(senha, (const uint8_t *)ssid, nssid, p->pmk)) p->tem_pmk = 1;
        else if (!tem_sae(s)) {
            avisa(true, _("wifi: la clave de WPA/WPA2 tiene de 8 a 63 caracteres ASCII\n"));
            return 2;
        }
        if (redes[k].detalhe & WIFI_DET_TKIP)
            avisa(false, _("AVISO: %s usa TKIP, que está obsoleto. Mejor WPA2-AES o WPA3.\n"), ssid);
    }
    if (tem_sae(s)) {
        if (nsenha < 1 || nsenha > 128) { avisa(true, _("wifi: la contraseña de WPA3 tiene de 1 a 128 caracteres\n")); return 2; }
        info_wifi_t v;
        if (wifi(WIFI_ESTADO, &v, sizeof v) < 0) return 1;
        if (!modo_gui) imprimir("Preparando WPA3 (SAE con P-256)...\n");
        if (sae_pt((const uint8_t *)ssid, nssid, (const uint8_t *)senha, nsenha, (const uint8_t *)0, 0, p->pt)) p->tem_pt = 1;
        /* «cazar y picotear»: un PWE por cada punto de acceso de la red que no sea solo H2E */
        for (int i = 0; i < n && p->npwe < WIFI_MAX_PWE; i++) {
            if (compara_texto(redes[i].ssid, ssid) || !tem_sae(redes[i].seguranca) || (redes[i].detalhe & WIFI_DET_SO_H2E)) continue;
            pwe_wifi_t *w = &p->pwe[p->npwe];
            memcpy(w->bssid, redes[i].bssid, 6);
            if (sae_pwe_hp((const uint8_t *)senha, nsenha, v.mac, redes[i].bssid, w->pwe)) p->npwe++;
        }
    }
    return 0;
}

static int conectar_rede(const char *ssid, const char *senha)
{
    if (tam_texto(ssid) < 1 || tam_texto(ssid) > 32) { imprimir_em(ERROS, "wifi: el SSID tiene de 1 a 32 caracteres\n"); return 2; }
    if (!esperar_pronto(20000)) { estado(); return 1; }
    static pedido_wifi_t p;
    memset(&p, 0, sizeof p);
    copia_texto(p.ssid, ssid, sizeof p.ssid);
    bool aberta = false;
    ler_config();
    if (senha) {
        /* El tipo de la red (y sus puntos de acceso): de la última búsqueda o de una nueva. */
        int n = wifi(WIFI_REDES, redes, 32), k = -1;
        for (int vez = 0; vez < 2 && k < 0; vez++) {
            for (int i = 0; i < n; i++)
                if (!compara_texto(redes[i].ssid, ssid) && (k < 0 || redes[i].sinal > redes[k].sinal)) k = i;
            if (k < 0 && vez == 0) { imprimir("Buscando %s...\n", ssid); n = buscar_redes(); }
        }
        if (k < 0) {
            /* Red oculta o lejos: se supone WPA/WPA2 (como antes de la D220). */
            if (!wpa_pmk(senha, (const uint8_t *)ssid, (uint32_t)tam_texto(ssid), p.pmk)) {
                imprimir_em(ERROS, "wifi: no se ve la red; si es WPA2, la clave tiene de 8 a 63 caracteres ASCII\n");
                wpa_apagar(linhas, sizeof linhas);
                return 2;
            }
            p.tem_pmk = 1;
        } else {
            int r = preparar(&p, ssid, senha, k, n);
            if (r) { wpa_apagar(&p, sizeof p); wpa_apagar(linhas, sizeof linhas); return r; }
        }
    } else ler_recordada(ssid, &p, &aberta);
    wpa_apagar(linhas, sizeof linhas);
    int r = wifi(WIFI_CONECTAR, &p, sizeof p);
    if (r < 0) { wpa_apagar(&p, sizeof p); imprimir_em(ERROS, "wifi: %s\n", nome_erro(r)); return 1; }
    imprimir("Conectando a %s...\n", ssid);
    info_wifi_t v;
    bool ok = false;
    dormir(500);
    for (int t = 0; t < 150; t++) {
        if (wifi(WIFI_ESTADO, &v, sizeof v) < 0) break;
        if (v.estado == WIFI_CONECTADO) { ok = true; break; }
        if (v.estado == WIFI_PRONTO && v.erro[0]) break;
        dormir(200);
    }
    if (!ok) {
        wpa_apagar(&p, sizeof p);
        imprimir_em(ERROS, "wifi: no se pudo conectar: %s\n", v.erro[0] ? v.erro : "tiempo agotado");
        return 1;
    }
    recordar(ssid, &p);
    wpa_apagar(&p, sizeof p);
    imprimir("Conectado a %s (canal %u, %d dBm, %s). Pidiendo IP por DHCP...\n", v.ssid, v.canal, v.sinal,
             nome_seg(v.seguranca, v.detalhe));
    if (v.seguranca == WIFI_WEP) imprimir("AVISO: conexión WEP: trátala como una red abierta.\n");
    config_rede_t c;
    for (int t = 0; t < 50; t++) {
        if (config_rede(REDE_CONSULTAR, &c) == 0 && c.ip) {
            char a[16], b[16];
            imprimir("wlan0: %s, puerta de enlace %s\n", ip_para_texto(c.ip, a), c.roteador ? ip_para_texto(c.roteador, b) : "ninguna");
            return 0;
        }
        dormir(200);
    }
    imprimir("wlan0: el DHCP aún no ha respondido (ifconfig wlan0 para verlo)\n");
    return 0;
}

static int recordadas(void)
{
    ler_config();
    if (!nlinhas) { imprimir("No hay redes recordadas.\n"); return 0; }
    for (int i = 0; i < nlinhas; i++) {
        char ssid[40];
        int k = 0;
        for (const char *p = linhas[i] + 5; p[0] != ' ' && p[0] && p[1] && k < 32; p += 2) {
            int a = hexval(p[0]), b = hexval(p[1]);
            if (a < 0 || b < 0) break;
            ssid[k++] = (char)(a << 4 | b);
        }
        ssid[k] = 0;
        static pedido_wifi_t p;
        bool aberta = false;
        memset(&p, 0, sizeof p);
        ler_recordada(ssid, &p, &aberta);
        bool sae = p.tem_pt || p.npwe;
        const char *tipo = aberta ? "abierta" : p.nwep ? "WEP" : sae && p.tem_pmk ? "WPA2/WPA3" : sae ? "WPA3" : "WPA/WPA2";
        imprimir("%s (%s)\n", ssid, tipo);
        wpa_apagar(&p, sizeof p);
    }
    wpa_apagar(linhas, sizeof linhas);
    return 0;
}

static int olvidar(const char *ssid)
{
    ler_config();
    int i = achar(ssid);
    if (i < 0) { avisa(true, "wifi: %s no está entre las redes recordadas\n", ssid); return 1; }
    for (int k = i + 1; k < nlinhas; k++) copia_texto(linhas[k - 1], linhas[k], sizeof linhas[0]);
    nlinhas--;
    int r = gravar_config();
    wpa_apagar(linhas, sizeof linhas);
    if (r < 0) { avisa(true, "wifi: no se pudo guardar %s\n", CONFIG); return 1; }
    avisa(false, "Olvidada: %s\n", ssid);
    return 0;
}

#include "wifi_ventana.h"
#include "../../kernel/rede/wifi_usb_logica.h"   /* D409: qué pinchos reconoce el kernel (por VID:PID) */

/* D409: «wifi compatibles [VID:PID]». */
static int hex4(const char *t, unsigned *v, const char **fin)
{
    unsigned r = 0;
    int n = 0;
    for (; n < 4; n++, t++) {
        unsigned d;
        if (*t >= '0' && *t <= '9') d = (unsigned)(*t - '0');
        else if (*t >= 'a' && *t <= 'f') d = (unsigned)(*t - 'a' + 10);
        else if (*t >= 'A' && *t <= 'F') d = (unsigned)(*t - 'A' + 10);
        else break;
        r = r << 4 | d;
    }
    if (!n) return 0;
    *v = r;
    *fin = t;
    return 1;
}

static int compatibles(int argc, char **argv)
{
    if (argc >= 3) {
        unsigned vid, pid;
        const char *f;
        if (!hex4(argv[2], &vid, &f) || *f != ':' || !hex4(f + 1, &pid, &f) || *f) {
            imprimir_em(ERROS, "uso: wifi compatibles [VID:PID]   (por ejemplo 148f:3070)\n");
            return 2;
        }
        int fam = wusb_familia((uint16_t)vid, (uint16_t)pid);
        if (fam == WUSB_NINGUNO) { imprimir("%04x:%04x: no es un pincho Wi-Fi USB reconocido\n", vid, pid); return 1; }
        imprimir("%04x:%04x: %s%s\n", vid, pid, wusb_nome_familia(fam),
                 fam == WUSB_AR9271 ? " (reconocido, todavía sin driver)" :
                 wusb_experimental(fam) ? " (experimental, sin probar en hardware)" : " (probado)");
        return 0;
    }
    imprimir("Pinchos Wi-Fi USB reconocidos (por VID:PID):\n");
    imprimir("  %-30s %3d  probado en hardware\n", wusb_nome_familia(WUSB_RTL8188EU), 1);
    imprimir("  %-30s %3d  experimental, sin probar en hardware\n", wusb_nome_familia(WUSB_RTL8192CU), WUSB_N(wusb_ids_rtl8192cu));
    imprimir("  %-30s %3d  experimental, sin probar en hardware\n", wusb_nome_familia(WUSB_RT2870), WUSB_N(wusb_ids_rt2870));
    imprimir("  %-30s %3d  reconocido, todavía sin driver\n", wusb_nome_familia(WUSB_AR9271), WUSB_N(wusb_ids_ar9271));
    imprimir("Un solo adaptador Wi-Fi a la vez. Comprueba el tuyo con: wifi compatibles VID:PID (el VID:PID lo ves con el programa usb).\n");
    return 0;
}

int principal(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) if (!compara_texto(argv[i], "--simulado")) simulado = true;
    if (argc > 1 && (!compara_texto(argv[1], "--ventana") || !compara_texto(argv[1], "-v"))) return ventana();
    if (argc < 2 || !compara_texto(argv[1], "estado")) return estado();
    if (!compara_texto(argv[1], "buscar")) return buscar();
    if (!compara_texto(argv[1], "conectar") && argc >= 3) return conectar_rede(argv[2], argc >= 4 ? argv[3] : NULL);
    if (!compara_texto(argv[1], "desconectar")) {
        int r = wifi(WIFI_DESCONECTAR, 0, 0);
        if (r < 0) { imprimir_em(ERROS, "wifi: %s\n", nome_erro(r)); return 1; }
        imprimir("Desconectado.\n");
        return 0;
    }
    if (!compara_texto(argv[1], "compatibles")) return compatibles(argc, argv);
    if (!compara_texto(argv[1], "recordadas")) return recordadas();
    if (!compara_texto(argv[1], "olvidar") && argc >= 3) return olvidar(argv[2]);
    imprimir_em(ERROS, "uso: wifi [estado | buscar | conectar SSID [clave] | desconectar | recordadas | olvidar SSID | compatibles [VID:PID] | --ventana]\n");
    return 2;
}
