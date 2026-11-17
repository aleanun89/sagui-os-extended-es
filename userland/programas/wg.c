// SPDX-License-Identifier: GPL-3.0-only
/* wg - la VPN WireGuard de SaguiOS (D363).
 *
 *   wg genkey                     una clave privada nueva (base64)
 *   wg pubkey                     lee una privada por la entrada y escribe su pública
 *   wg genpsk                     una clave precompartida (PresharedKey)
 *   wg show [INTERFAZ]            el estado del túnel: claves públicas, extremo, último
 *                                 saludo, bytes y lo descartado
 *   wg-quick up NOMBRE|RUTA       enciende el túnel de /home/.config/wireguard/NOMBRE.conf
 *   wg-quick down [NOMBRE]        lo apaga
 *   wg-quick check NOMBRE|RUTA    solo comprueba el archivo
 *   wg --ventana                  la ventana «VPN (WireGuard)» (la abre Configuración > Red)
 *   wg --demonio NOMBRE           (interno) el demonio; lee el .conf por la entrada (D372)
 *
 * El formato del archivo es el de wg-quick ([Interface] PrivateKey, Address, DNS,
 * MTU, ListenPort; [Peer] PublicKey, PresharedKey, Endpoint, AllowedIPs,
 * PersistentKeepalive). Solo IPv4: lo de IPv6 se ignora con un aviso.
 *
 * Cómo está hecho (D363): el kernel solo tiene la interfaz (wg0: dirección, rutas,
 * MTU y DNS, kernel/rede/tunel.c); el protocolo corre en este programa, que
 * «wg-quick up» deja en segundo plano como demonio («wg --demonio RUTA»). El
 * demonio abre la interfaz (SOCK_TUN), un socket UDP que sale siempre por la
 * interfaz física (OPC_FISICA) y hace de puente con userland/libtls/wireguard.c.
 * Al morir (o con «wg-quick down»), el kernel quita la interfaz y sus rutas.
 *
 * Encender (D372): solo root. wg-quick up y la ventana piden permiso cada vez (el
 * diálogo de administrador, o la contraseña en el terminal sin escritorio), leen el
 * archivo con los permisos del usuario y se lo pasan al demonio, que corre como root,
 * por una tubería. Apagar: también la sesión local sin elevar (devuelve el tráfico a
 * la red física). El archivo tiene la clave privada: debe ser solo del dueño (chmod
 * 600); si otros pueden leerlo, se avisa (como wg-quick). */
#include "sagui.h"
#include "cripto.h"
#include "wireguard.h"
#include "gui.h"
#include "contas.h"

#define DIR_WG   "/home/.config/wireguard"
#define CONF_MAX 16384

/* ------------------------------------------------------------ el archivo .conf */

typedef struct {
    uint8_t  publica[32], psk[32];
    bool     tem_psk;
    char     endpoint[128];        /* el nombre o la IP tal como venía */
    uint32_t ip;                   /* resuelto */
    uint16_t porta;
    wg_prefixo_t ips[WG_IPS_MAX];
    int      nips;
    uint16_t keepalive;
} conf_par_t;

typedef struct {
    char     nome[16];
    uint8_t  privada[32];
    bool     tem_privada;
    uint32_t ip, mascara, dns;
    uint16_t mtu, porta;
    int      npares;
    conf_par_t pares[WG_PARES_MAX];
} conf_wg_t;

static bool modo_gui;
static bool sem_avisos;   /* el demonio: los avisos del archivo ya los dio wg-quick */
static char msg_gui[200];

/* Los avisos van a la salida de errores; en la ventana, a su línea de estado. */
static void aviso(const char *fmt, ...)
{
    if (sem_avisos) return;
    va_list ap;
    va_start(ap, fmt);
    char t[240];
    formatar_lista(t, sizeof t, fmt, ap);
    va_end(ap);
    if (modo_gui) copia_texto(msg_gui, t, sizeof msg_gui);
    else imprimir_em(ERROS, "%s", t);
}

static char minus(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }
static bool igual_ci(const char *a, const char *b)
{
    while (*a && *b && minus(*a) == minus(*b)) { a++; b++; }
    return !*a && !*b;
}

static char *recortar(char *s)
{
    while (eh_espaco(*s)) s++;
    char *f = s + tam_texto(s);
    while (f > s && eh_espaco(f[-1])) *--f = 0;
    return s;
}

/* Siguiente elemento de una lista separada por comas (lo deja recortado); NULL al final. */
static char *proximo_item(char **p)
{
    if (!*p || !**p) return NULL;
    char *ini = *p, *v = acha_char(ini, ',');
    if (v) { *v = 0; *p = v + 1; } else *p = ini + tam_texto(ini);
    return recortar(ini);
}

static bool ler_numero(const char *s, uint32_t max, uint32_t *v)
{
    uint32_t n = 0;
    if (!*s) return false;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return false;
        n = n * 10 + (uint32_t)(*s - '0');
        if (n > max) return false;
    }
    *v = n;
    return true;
}

static bool eh_ipv6(const char *s) { return acha_char(s, ':') != NULL; }
static bool tudo_zero_conf(const uint8_t *k) { uint8_t d = 0; for (int i = 0; i < 32; i++) d |= k[i]; return d == 0; }

/* El nombre de la interfaz: el del archivo sin «.conf» (como wg-quick), hasta 15 letras. */
static bool nome_de_caminho(const char *caminho, char nome[16])
{
    const char *b = acha_ultimo_char(caminho, '/');
    b = b ? b + 1 : caminho;
    size_t n = tam_texto(b);
    if (n > 5 && compara_texto(b + n - 5, ".conf") == 0) n -= 5;
    if (n == 0 || n > 15) return false;
    for (size_t i = 0; i < n; i++) {
        char c = b[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '=' ||
                  c == '+' || c == '.' || c == '-';
        if (!ok) return false;
        nome[i] = c;
    }
    nome[n] = 0;
    return true;
}

/* «casa» -> /home/.config/wireguard/casa.conf; con «/» o «.conf», la ruta tal cual. */
static void caminho_de(const char *arg, char *caminho, int max)
{
    size_t n = tam_texto(arg);
    if (acha_char(arg, '/') || (n > 5 && compara_texto(arg + n - 5, ".conf") == 0)) copia_texto(caminho, arg, (size_t)max);
    else formatar_texto(caminho, (size_t)max, "%s/%s.conf", DIR_WG, arg);
}

/* El texto del .conf tal como se leyó (D372): wg-quick lo lee con los permisos de quien lo pide y,
 * si hay que encender, se lo pasa entero al demonio (root) por una tubería. Así el demonio no abre
 * ninguna ruta: root nunca lee un archivo que haya elegido otro usuario (ni /etc/sombra ni nada
 * fuera de su carpeta), y lo que no sea un .conf válido no llega a usarse. */
static char conf_bruto[CONF_MAX + 2];
static int  conf_bruto_n;
static char conf_trabalho[CONF_MAX + 2];   /* la copia que se trocea al analizar */

/* Lee de "fd" a conf_bruto. Como mucho CONF_MAX bytes y solo texto (sin bytes 0). */
static bool conf_ler_fd(int fd, char *erro, int max)
{
    int n = 0, r = 0;
    while (n <= CONF_MAX && (r = ler(fd, conf_bruto + n, (unsigned)(CONF_MAX + 1 - n))) > 0) n += r;
    conf_bruto_n = 0;
    if (r < 0) { formatar_texto(erro, (size_t)max, _("no se pudo leer la configuración: %s"), nome_erro(r)); return false; }
    if (n > CONF_MAX) { formatar_texto(erro, (size_t)max, _("la configuración pasa de %d bytes"), CONF_MAX); return false; }
    for (int i = 0; i < n; i++)
        if (!conf_bruto[i]) { copia_texto(erro, _("la configuración no es un archivo de texto"), (size_t)max); return false; }
    conf_bruto[n] = 0;
    conf_bruto_n = n;
    return true;
}

static bool conf_analisar(const char *nome, conf_wg_t *c, char *erro, int max);

/* Lee y comprueba el archivo (con los permisos de quien llama). false con el motivo en "erro". El
 * texto queda en conf_bruto hasta conf_wg_apagar(). */
static bool conf_ler(const char *caminho, conf_wg_t *c, char *erro, int max)
{
    memset(c, 0, sizeof *c);
    char nome[16];
    if (!nome_de_caminho(caminho, nome)) {
        formatar_texto(erro, (size_t)max, _("el nombre de «%s» no vale para una interfaz (letras, cifras, «_-.+=», hasta 15)"), caminho);
        return false;
    }
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) { formatar_texto(erro, (size_t)max, _("no se puede abrir %s: %s"), caminho, nome_erro(fd)); return false; }
    char e[120];
    bool ok = conf_ler_fd(fd, e, sizeof e);
    fechar(fd);
    if (!ok) { formatar_texto(erro, (size_t)max, "%s: %s", caminho, e); return false; }
    info_permisos_t pm;
    if (permisos_arquivo(caminho, &pm) == 0 && pm.com_donos && (pm.modo & 077))
        aviso(_("Aviso: otros usuarios pueden leer %s (tiene la clave privada): chmod 600 %s\n"), caminho, caminho);
    return conf_analisar(nome, c, erro, max);
}

/* Analiza conf_bruto (sobre una copia, que se borra al terminar). */
static bool conf_analisar_texto(char *texto, conf_wg_t *c, char *erro, int max);
static bool conf_analisar(const char *nome, conf_wg_t *c, char *erro, int max)
{
    memset(c, 0, sizeof *c);
    copia_texto(c->nome, nome, sizeof c->nome);
    memcpy(conf_trabalho, conf_bruto, (size_t)conf_bruto_n + 1);
    bool ok = conf_analisar_texto(conf_trabalho, c, erro, max);
    cripto_apagar(conf_trabalho, sizeof conf_trabalho);   /* tenía la clave privada en claro */
    return ok;
}

static bool conf_analisar_texto(char *texto, conf_wg_t *c, char *erro, int max)
{
    enum { NADA, INTERFACE, PEER } secao = NADA;
    bool tem_ip = false;
    int linha_n = 0;
    char *p = texto;
    while (*p) {
        char *linha = p, *fim = acha_char(p, '\n');
        if (fim) { *fim = 0; p = fim + 1; } else p += tam_texto(p);
        linha_n++;
        char *h = acha_char(linha, '#');   /* como wg-quick: lo que sigue a «#» es comentario */
        if (h) *h = 0;
        char *l = recortar(linha);
        if (!*l) continue;
        if (l[0] == '[') {
            if (igual_ci(l, "[Interface]")) secao = INTERFACE;
            else if (igual_ci(l, "[Peer]")) {
                if (c->npares >= WG_PARES_MAX) { formatar_texto(erro, (size_t)max, _("línea %d: más de %d pares"), linha_n, WG_PARES_MAX); return false; }
                secao = PEER;
                c->npares++;
            } else { formatar_texto(erro, (size_t)max, _("línea %d: sección desconocida %s"), linha_n, l); return false; }
            continue;
        }
        char *ig = acha_char(l, '=');
        if (!ig) { formatar_texto(erro, (size_t)max, _("línea %d: falta «=»"), linha_n); return false; }
        *ig = 0;
        char *k = recortar(l), *v = recortar(ig + 1);
        conf_par_t *par = secao == PEER ? &c->pares[c->npares - 1] : NULL;
        if (secao == INTERFACE && igual_ci(k, "PrivateKey")) {
            if (!wg_base64_ler(v, c->privada)) { formatar_texto(erro, (size_t)max, _("línea %d: PrivateKey no es una clave válida"), linha_n); return false; }
            c->tem_privada = true;
        } else if (secao == INTERFACE && igual_ci(k, "Address")) {
            char *it;
            while ((it = proximo_item(&v))) {
                wg_prefixo_t pf;
                if (eh_ipv6(it)) { aviso(_("Aviso: %s: IPv6 no está soportado; se ignora %s\n"), c->nome, it); continue; }
                char *barra = acha_char(it, '/');
                uint32_t ip;
                if (barra) *barra = 0;
                if (!texto_para_ip(it, &ip)) { formatar_texto(erro, (size_t)max, _("línea %d: Address no válida"), linha_n); return false; }
                if (barra) *barra = '/';
                if (!wg_prefixo_ler(it, &pf)) { formatar_texto(erro, (size_t)max, _("línea %d: Address no válida"), linha_n); return false; }
                if (tem_ip) { aviso(_("Aviso: %s: solo se usa la primera dirección IPv4; se ignora %s\n"), c->nome, it); continue; }
                c->ip = ip;
                c->mascara = pf.bits ? 0xFFFFFFFFu << (32 - pf.bits) : 0;
                tem_ip = true;
            }
        } else if (secao == INTERFACE && igual_ci(k, "DNS")) {
            char *it;
            while ((it = proximo_item(&v))) {
                uint32_t ip;
                if (eh_ipv6(it)) continue;
                if (!texto_para_ip(it, &ip)) continue;   /* un dominio de búsqueda: no se usa */
                if (!c->dns) c->dns = ip;
            }
        } else if (secao == INTERFACE && igual_ci(k, "MTU")) {
            uint32_t m;
            if (!ler_numero(v, 1500, &m) || m < 576) { formatar_texto(erro, (size_t)max, _("línea %d: MTU entre 576 y 1500"), linha_n); return false; }
            c->mtu = (uint16_t)m;
        } else if (secao == INTERFACE && igual_ci(k, "ListenPort")) {
            uint32_t m;
            if (!ler_numero(v, 65535, &m)) { formatar_texto(erro, (size_t)max, _("línea %d: ListenPort no válido"), linha_n); return false; }
            c->porta = (uint16_t)m;
        } else if (par && igual_ci(k, "PublicKey")) {
            if (!wg_base64_ler(v, par->publica)) { formatar_texto(erro, (size_t)max, _("línea %d: PublicKey no es una clave válida"), linha_n); return false; }
        } else if (par && igual_ci(k, "PresharedKey")) {
            if (!wg_base64_ler(v, par->psk)) { formatar_texto(erro, (size_t)max, _("línea %d: PresharedKey no es una clave válida"), linha_n); return false; }
            par->tem_psk = true;
        } else if (par && igual_ci(k, "Endpoint")) {
            if (v[0] == '[') { formatar_texto(erro, (size_t)max, _("línea %d: Endpoint IPv6 no está soportado"), linha_n); return false; }
            char *dp = acha_ultimo_char(v, ':');
            uint32_t pt;
            if (!dp || dp == v || !ler_numero(dp + 1, 65535, &pt) || pt == 0) {
                formatar_texto(erro, (size_t)max, _("línea %d: Endpoint es «nombre:puerto»"), linha_n);
                return false;
            }
            *dp = 0;
            copia_texto(par->endpoint, v, sizeof par->endpoint);
            par->porta = (uint16_t)pt;
        } else if (par && igual_ci(k, "AllowedIPs")) {
            char *it;
            while ((it = proximo_item(&v))) {
                if (eh_ipv6(it)) { aviso(_("Aviso: %s: IPv6 no está soportado; se ignora %s\n"), c->nome, it); continue; }
                if (par->nips >= WG_IPS_MAX) { formatar_texto(erro, (size_t)max, _("línea %d: demasiadas AllowedIPs"), linha_n); return false; }
                if (!wg_prefixo_ler(it, &par->ips[par->nips])) { formatar_texto(erro, (size_t)max, _("línea %d: AllowedIPs no válida: %s"), linha_n, it); return false; }
                par->nips++;
            }
        } else if (par && igual_ci(k, "PersistentKeepalive")) {
            uint32_t m;
            if (igual_ci(v, "off")) m = 0;
            else if (!ler_numero(v, 65535, &m)) { formatar_texto(erro, (size_t)max, _("línea %d: PersistentKeepalive en segundos u «off»"), linha_n); return false; }
            par->keepalive = (uint16_t)m;
        } else if (secao == INTERFACE && (igual_ci(k, "PreUp") || igual_ci(k, "PostUp") || igual_ci(k, "PreDown") ||
                                          igual_ci(k, "PostDown") || igual_ci(k, "Table") || igual_ci(k, "SaveConfig") ||
                                          igual_ci(k, "FwMark"))) {
            aviso(_("Aviso: %s: %s no está soportado en SaguiOS; se ignora\n"), c->nome, k);
        } else if (secao == NADA) {
            formatar_texto(erro, (size_t)max, _("línea %d: falta [Interface] o [Peer] antes"), linha_n);
            return false;
        } else {
            formatar_texto(erro, (size_t)max, _("línea %d: clave desconocida %s"), linha_n, k);
            return false;
        }
    }
    if (!c->tem_privada) { formatar_texto(erro, (size_t)max, _("falta PrivateKey en [Interface]")); return false; }
    if (!tem_ip) { formatar_texto(erro, (size_t)max, _("falta una Address IPv4 en [Interface]")); return false; }
    if (!c->npares) { formatar_texto(erro, (size_t)max, _("no hay ningún [Peer]")); return false; }
    for (int i = 0; i < c->npares; i++) {
        if (tudo_zero_conf(c->pares[i].publica)) { formatar_texto(erro, (size_t)max, _("al par %d le falta PublicKey"), i + 1); return false; }
    }
    if (!c->mtu) c->mtu = WG_MTU_PADRAO;
    return true;
}

static void conf_wg_apagar(conf_wg_t *c)
{
    cripto_apagar(c, sizeof *c);
    cripto_apagar(conf_bruto, sizeof conf_bruto);
    conf_bruto_n = 0;
}

/* ------------------------------------------------------------ el demonio */

static int tun_fd = -1, udp_fd = -1;
static wg_t w;
static uint32_t base_utc, base_ms;

static void d_enviar_udp(void *ctx, uint32_t ip, uint16_t porta, const uint8_t *d, unsigned n)
{
    (void)ctx;
    enviar_para(udp_fd, d, n, ip, porta);
}

static void d_entregar(void *ctx, const uint8_t *pacote, unsigned n)
{
    (void)ctx;
    escrever(tun_fd, pacote, n);
}

/* TAI64N creciente: la hora UTC del arranque del demonio + el reloj en ms (que no retrocede). */
static void d_tai64n(void *ctx, uint8_t ts[12])
{
    (void)ctx;
    uint32_t ms = tempo_ms() - base_ms;
    uint64_t s = 0x400000000000000AULL + base_utc + ms / 1000;
    uint32_t ns = (ms % 1000) * 1000000u;
    for (int i = 0; i < 8; i++) ts[i] = (uint8_t)(s >> (56 - 8 * i));
    for (int i = 0; i < 4; i++) ts[8 + i] = (uint8_t)(ns >> (24 - 8 * i));
}

static uint32_t d_hora(void *ctx) { (void)ctx; return base_utc + (tempo_ms() - base_ms) / 1000; }

static void publicar(int porta)
{
    static info_wg_demonio_t e;
    memset(&e, 0, sizeof e);
    wg_base64_escrever(w.publica, e.publica);
    e.porta = (unsigned)porta;
    e.npares = (unsigned)w.npares;
    for (int i = 0; i < w.npares && i < WG_INFO_PARES; i++) {
        wg_par_t *p = &w.pares[i];
        info_wg_par_t *q = &e.pares[i];
        wg_base64_escrever(p->publica, q->publica);
        q->endpoint_ip = p->endpoint_ip;
        q->endpoint_porta = p->endpoint_porta;
        q->ultimo_hs = p->hs_completo_unix;
        q->keepalive_s = p->keepalive_s;
        uint8_t d = 0;
        for (int k = 0; k < 32; k++) d |= p->psk[k];
        q->tem_psk = d != 0;
        q->rx_bytes = p->rx_bytes;
        q->tx_bytes = p->tx_bytes;
        q->nips = (unsigned)(p->nips < 8 ? p->nips : 8);
        for (unsigned k = 0; k < q->nips; k++) { q->ips[k].ip = p->ips[k].ip; q->ips[k].bits = p->ips[k].bits; }
    }
    e.descartes_repeticao = w.descartes_repeticao;
    e.descartes_mac = w.descartes_mac;
    e.descartes_origem = w.descartes_origem;
    e.descartes_outros = w.descartes_outros;
    e.cookies_recebidas = w.cookies_recebidas;
    e.cookies_enviadas = w.cookies_enviadas;
    opcao_socket(tun_fd, OPC_TUN_ESTADO, (unsigned)(size_t)&e);
}

/* Código de salida del demonio (wg-quick up lo enseña si no arrancó). */
enum { D_OK = 0, D_CONF = 2, D_ACESSO = 3, D_OCUPADO = 4, D_REDE = 5, D_RESOLVER = 6 };

/* D372: el demonio corre como root (wg-quick pidió el permiso) y recibe la configuración por la
 * entrada (una tubería desde wg-quick), nunca una ruta: no abre ningún archivo. "nome" es el de la
 * interfaz. */
static int demonio(const char *nome, bool tempos_prova)
{
    static conf_wg_t c;
    char erro[200], n16[16];
    sem_avisos = true;
    if (!nome_de_caminho(nome, n16)) { imprimir_em(ERROS, _("wg: nombre de interfaz no válido\n")); return D_CONF; }
    if (!conf_ler_fd(ENTRADA, erro, sizeof erro) || !conf_analisar(n16, &c, erro, sizeof erro)) {
        imprimir_em(ERROS, "wg: %s\n", erro);
        conf_wg_apagar(&c);
        return D_CONF;
    }
    cripto_apagar(conf_bruto, sizeof conf_bruto);   /* lo que hace falta ya está en "c" */
    /* Los extremos se resuelven antes de crear las rutas: con AllowedIPs = 0.0.0.0/0 la
     * pregunta al DNS ya iría por un túnel que aún no funciona. */
    for (int i = 0; i < c.npares; i++) {
        if (!c.pares[i].endpoint[0]) continue;
        int r = resolver(c.pares[i].endpoint, &c.pares[i].ip);
        if (r < 0 || !c.pares[i].ip) {
            imprimir_em(ERROS, _("wg: no se pudo resolver %s: %s\n"), c.pares[i].endpoint, nome_erro(r));
            conf_wg_apagar(&c);
            return D_RESOLVER;
        }
    }
    tun_fd = criar_socket(SOCK_TUN);
    if (tun_fd < 0) {
        conf_wg_apagar(&c);
        if (tun_fd == ERRO_ACESSO) { imprimir_em(ERROS, _("wg: encender una VPN es solo para el administrador (wg-quick up pide el permiso)\n")); return D_ACESSO; }
        if (tun_fd == ERRO_OCUPADO) { imprimir_em(ERROS, _("wg: ya hay un túnel encendido (wg-quick down primero)\n")); return D_OCUPADO; }
        imprimir_em(ERROS, _("wg: no se pudo crear la interfaz: %s\n"), nome_erro(tun_fd));
        return D_REDE;
    }
    static config_tunel_t ct;
    memset(&ct, 0, sizeof ct);
    copia_texto(ct.nome, c.nome, sizeof ct.nome);
    ct.ip = c.ip; ct.mascara = c.mascara; ct.mtu = c.mtu; ct.dns = c.dns;
    for (int i = 0; i < c.npares; i++)
        for (int k = 0; k < c.pares[i].nips && ct.nrotas < TUNEL_ROTAS_MAX; k++) {
            ct.rotas[ct.nrotas].ip = c.pares[i].ips[k].ip;
            ct.rotas[ct.nrotas].bits = c.pares[i].ips[k].bits;
            ct.nrotas++;
        }
    udp_fd = criar_socket(SOCK_UDP);
    int r = udp_fd < 0 ? udp_fd : opcao_socket(udp_fd, OPC_FISICA, 1);
    if (r >= 0 && c.porta) r = escutar(udp_fd, c.porta, 0);
    if (r < 0) {
        imprimir_em(ERROS, _("wg: el socket UDP (puerto %u): %s\n"), (unsigned)c.porta, nome_erro(r));
        conf_wg_apagar(&c);
        return D_REDE;
    }
    base_utc = hora_utc(NULL);
    base_ms = tempo_ms();
    wg_iniciar(&w, c.privada);
    w.enviar_udp = d_enviar_udp; w.entregar = d_entregar; w.tai64n = d_tai64n; w.hora_unix = d_hora;
    w.mtu = c.mtu;
    if (tempos_prova) {   /* solo para tests/h365_wireguard.py: renovar en segundos, no en minutos */
        w.t.rekey_apos_ms = 4000; w.t.rejeitar_apos_ms = 12000; w.t.keepalive_ms = 2000;
        w.t.rekey_timeout_ms = 1500; w.t.tentativa_ms = 20000;
    }
    for (int i = 0; i < c.npares; i++) {
        conf_par_t *q = &c.pares[i];
        wg_par_t *p = wg_par_novo(&w, q->publica, q->tem_psk ? q->psk : NULL);
        if (!p) {
            imprimir_em(ERROS, _("wg: el par %d no vale (clave repetida, la propia o inválida)\n"), i + 1);
            conf_wg_apagar(&c);
            wg_apagar(&w);
            return D_CONF;
        }
        p->endpoint_ip = q->ip;
        p->endpoint_porta = q->porta;
        p->keepalive_s = q->keepalive;
        p->nips = q->nips;
        memcpy(p->ips, q->ips, sizeof q->ips);
    }
    r = opcao_socket(tun_fd, OPC_TUN_CONFIG, (unsigned)(size_t)&ct);
    if (r < 0) {
        imprimir_em(ERROS, _("wg: la interfaz no aceptó la dirección o las rutas: %s\n"), nome_erro(r));
        conf_wg_apagar(&c);
        wg_apagar(&w);
        return D_CONF;
    }
    int porta_escuta = c.porta;
    conf_wg_apagar(&c);   /* las claves ya están en "w"; la copia del archivo se borra */
    publicar(porta_escuta);
    registrar("[WG] %s encendida (%d pares)\n", ct.nome, w.npares);
    /* Ya en marcha: la terminal se suelta (wg-quick up vuelve y el terminal puede cerrarse). */
    fechar(ENTRADA); fechar(SAIDA); fechar(ERROS);
    for (int i = 0; i < w.npares; i++)
        if (w.pares[i].keepalive_s && w.pares[i].endpoint_ip) wg_iniciar_saudacao(&w, &w.pares[i], tempo_ms());

    static uint8_t buf[2048];
    unsigned visto = (unsigned)esperar_rede(0, 0);
    uint32_t ultima_pub = tempo_ms();
    for (;;) {
        visto = (unsigned)esperar_rede(visto, 0);
        int k;
        for (k = 0; k < 64; k++) {   /* de la interfaz al túnel */
            int n = disponivel(tun_fd);
            if (n < 0) goto fim;      /* REDE_TUNEL_DESLIGAR: wg-quick down */
            if (n == 0) break;
            n = ler(tun_fd, buf, sizeof buf);
            if (n > 0) wg_enviar_pacote(&w, buf, (unsigned)n, tempo_ms());
        }
        for (k = 0; k < 64; k++) {   /* del túnel a la interfaz */
            if (disponivel(udp_fd) <= 0) break;
            endereco_rede_t de;
            int n = receber_de(udp_fd, buf, sizeof buf, &de);
            if (n > 0) wg_udp_chegou(&w, de.ip, (uint16_t)de.porta, buf, (unsigned)n, tempo_ms());
        }
        uint32_t prox = wg_tique(&w, tempo_ms());
        if (tempo_ms() - ultima_pub >= 1000) { publicar(porta_escuta); ultima_pub = tempo_ms(); }
        if (prox > 1000) prox = 1000;
        visto = (unsigned)esperar_rede(visto, prox);
    }
fim:
    registrar("[WG] %s apagada\n", ct.nome);
    wg_apagar(&w);
    cripto_apagar(buf, sizeof buf);
    fechar(udp_fd);
    fechar(tun_fd);
    return D_OK;
}

/* ------------------------------------------------------------ wg show */

static int info_wg(info_wg_t *v) { return info_sistema(INFO_WG, v, sizeof *v); }

static void texto_bytes(char *s, int max, unsigned long long b)
{
    if (b < 1024) formatar_texto(s, (size_t)max, "%u B", (unsigned)b);
    else if (b < 1024ULL * 1024) formatar_texto(s, (size_t)max, "%u.%02u KiB", (unsigned)(b / 1024), (unsigned)(b % 1024 * 100 / 1024));
    else if (b < 1024ULL * 1024 * 1024) formatar_texto(s, (size_t)max, "%u.%02u MiB", (unsigned)(b >> 20), (unsigned)((b & 0xFFFFF) * 100 >> 20));
    else formatar_texto(s, (size_t)max, "%u.%02u GiB", (unsigned)(b >> 30), (unsigned)(((b >> 20) & 1023) * 100 / 1024));
}

static void texto_desde(char *s, int max, unsigned quando)
{
    if (!quando) { copia_texto(s, _("nunca"), (size_t)max); return; }
    uint32_t agora = hora_utc(NULL);
    uint32_t d = agora > quando ? agora - quando : 0;
    if (d < 60) formatar_texto(s, (size_t)max, _("hace %u s"), d);
    else if (d < 3600) formatar_texto(s, (size_t)max, _("hace %u min %u s"), d / 60, d % 60);
    else formatar_texto(s, (size_t)max, _("hace %u h %u min"), d / 3600, d / 60 % 60);
}

static int cmd_show(const char *qual)
{
    static info_wg_t v;
    if (info_wg(&v) <= 0) { imprimir(_("No hay ningún túnel WireGuard encendido.\n")); return 0; }
    if (qual && compara_texto(qual, v.config.nome) != 0 && compara_texto(qual, "all") != 0 &&
        compara_texto(qual, "interfaces") != 0) {
        imprimir_em(ERROS, _("wg: no existe la interfaz %s\n"), qual);
        return 1;
    }
    if (qual && compara_texto(qual, "interfaces") == 0) { imprimir("%s\n", v.config.nome); return 0; }
    char a[16], b[24], t1[24], t2[24];
    imprimir(_("interfaz: %s%s\n"), v.config.nome, v.desligando ? _(" (apagándose)") : "");
    imprimir(_("  clave pública: %s\n"), v.wg.publica);
    imprimir(_("  clave privada: (oculta)\n"));
    if (v.wg.porta) imprimir(_("  puerto de escucha: %u\n"), v.wg.porta);
    imprimir(_("  dirección: %s/%u, MTU %u"), ip_para_texto(v.config.ip, a), 32 - __builtin_ctz(v.config.mascara ? v.config.mascara : 1) - (v.config.mascara ? 0 : 1), v.config.mtu);
    if (v.config.dns) imprimir(_(", DNS %s"), ip_para_texto(v.config.dns, b));
    imprimir(_(" (demonio: proceso %u)\n"), v.pid);
    for (unsigned i = 0; i < v.wg.npares; i++) {
        info_wg_par_t *p = &v.wg.pares[i];
        imprimir(_("\npar: %s\n"), p->publica);
        if (p->tem_psk) imprimir(_("  clave precompartida: (oculta)\n"));
        if (p->endpoint_ip) imprimir(_("  extremo: %s:%u\n"), ip_para_texto(p->endpoint_ip, a), p->endpoint_porta);
        imprimir(_("  IP permitidas: "));
        for (unsigned k = 0; k < p->nips; k++) imprimir("%s%s/%u", k ? ", " : "", ip_para_texto(p->ips[k].ip, a), p->ips[k].bits);
        imprimir(p->nips ? "\n" : _("(ninguna)\n"));
        texto_desde(b, sizeof b, p->ultimo_hs);
        imprimir(_("  último saludo: %s\n"), b);
        texto_bytes(t1, sizeof t1, p->rx_bytes);
        texto_bytes(t2, sizeof t2, p->tx_bytes);
        imprimir(_("  transferencia: %s recibidos, %s enviados\n"), t1, t2);
        if (p->keepalive_s) imprimir(_("  keepalive persistente: cada %u s\n"), p->keepalive_s);
    }
    imprimir(_("\ndescartados: %u repetidos, %u con MAC o etiqueta mala, %u con origen no permitido, %u otros; cookies recibidas %u\n"),
             v.wg.descartes_repeticao, v.wg.descartes_mac, v.wg.descartes_origem, v.wg.descartes_outros, v.wg.cookies_recebidas);
    return 0;
}

/* ------------------------------------------------------------ claves */

static int cmd_genkey(void)
{
    uint8_t k[32];
    char s[45];
    wg_gerar_privada(k);
    wg_base64_escrever(k, s);
    imprimir("%s\n", s);
    cripto_apagar(k, sizeof k);
    cripto_apagar(s, sizeof s);
    return 0;
}

static int cmd_genpsk(void)
{
    uint8_t k[32];
    char s[45];
    cripto_aleatorio(k, 32);
    wg_base64_escrever(k, s);
    imprimir("%s\n", s);
    cripto_apagar(k, sizeof k);
    cripto_apagar(s, sizeof s);
    return 0;
}

static int cmd_pubkey(void)
{
    char l[128];
    uint8_t k[32], p[32];
    if (ler_linha(ENTRADA, l, sizeof l) < 0) l[0] = 0;
    char *t = recortar(l);
    if (!wg_base64_ler(t, k)) { imprimir_em(ERROS, _("wg: la entrada no es una clave en base64\n")); cripto_apagar(l, sizeof l); return 1; }
    wg_publica_de(k, p);
    char s[45];
    wg_base64_escrever(p, s);
    imprimir("%s\n", s);
    cripto_apagar(l, sizeof l);
    cripto_apagar(k, sizeof k);
    return 0;
}

/* ------------------------------------------------------------ wg-quick */

static void resumo_conf(const conf_wg_t *c)
{
    char a[16], b[16];
    imprimir(_("[#] %s: dirección %s/%u, MTU %u"), c->nome, ip_para_texto(c->ip, a),
             c->mascara ? 32 - __builtin_ctz(c->mascara) : 0, c->mtu);
    if (c->dns) imprimir(_(", DNS %s"), ip_para_texto(c->dns, b));
    imprimir("\n");
    for (int i = 0; i < c->npares; i++) {
        const conf_par_t *p = &c->pares[i];
        imprimir(_("[#] par %d: "), i + 1);
        if (p->endpoint[0]) imprimir(_("extremo %s:%u, "), p->endpoint, p->porta);
        imprimir(_("rutas"));
        for (int k = 0; k < p->nips; k++) imprimir(" %s/%u", ip_para_texto(p->ips[k].ip, a), p->ips[k].bits);
        imprimir("\n");
    }
}

/* D372: encender la VPN es de root. Quien no lo es pide permiso cada vez: con escritorio, el diálogo
 * de administrador (D269) con PED_SIN_RECORDAR (sin la autorización recordada de 5 min: otro programa
 * podría lanzar /bin/wg con su propio .conf en ese rato); en la consola o por SSH, la contraseña en el
 * terminal, como admin. 0 = ya es root o se concedió; "elevado" dice si hay que soltarlo después. */
static bool elevado_texto;
static unsigned uid_antes, gid_antes;

static int pedir_permiso(const char *nome, bool *elevado)
{
    *elevado = false;
    if (sou_root()) return 0;
    char motivo[PED_MOTIVO_MAX];
    formatar_texto(motivo, sizeof motivo, _("encender la VPN «%s» (todo el tráfico de red irá por ella)"), nome);
    int est = usuario_op(USU_ESTADO, 0, 0);
    bool servico = est >= 0 && (est & USU_EST_SERVICIO);
    int r = servico ? ERRO_NAO_SUPORTADO : elevar_grafico_ex(motivo, PED_SIN_RECORDAR);
    if (r == 0) { *elevado = true; return 0; }
    if (r == ERRO_NAO_SUPORTADO && !modo_gui) {   /* sin escritorio (consola, SSH): en el terminal */
        uid_antes = meu_uid();
        gid_antes = meu_gid();
        imprimir_em(ERROS, _("wg-quick: encender una VPN necesita permiso de administrador.\n"));
        r = elevar_interativo(UID_ROOT, "wg-quick");
        if (r == 0) { elevado_texto = true; *elevado = true; return 0; }
    }
    return r < 0 ? r : ERRO_ACESSO;
}

static void soltar_permiso(void)
{
    if (elevado_texto) {
        usuario_op(USU_FIJAR, (int)uid_antes, (int)gid_antes);
        elevado_texto = false;
    } else elevacao_soltar();
}

/* Lanza el demonio y espera a que la interfaz aparezca. 0 = encendida. El archivo lo lee este
 * proceso con los permisos de quien lo pide (antes de elevar) y se pasa al demonio por la entrada. */
static int quick_up(const char *arg, bool tempos_prova, bool calado)
{
    char caminho[160], erro[200];
    caminho_de(arg, caminho, sizeof caminho);
    static conf_wg_t c;
    if (!conf_ler(caminho, &c, erro, sizeof erro)) { aviso("wg-quick: %s\n", erro); conf_wg_apagar(&c); return 1; }
    static info_wg_t v;
    if (info_wg(&v) > 0) {
        aviso(_("wg-quick: ya está encendida %s (wg-quick down %s)\n"), v.config.nome, v.config.nome);
        conf_wg_apagar(&c);
        return 1;
    }
    if (!calado) resumo_conf(&c);
    char nome[16];
    copia_texto(nome, c.nome, sizeof nome);
    cripto_apagar(&c, sizeof c);   /* conf_bruto sigue: es lo que se manda al demonio */
    bool elevado;
    int r = pedir_permiso(nome, &elevado);
    if (r < 0) {
        conf_wg_apagar(&c);
        registrar("wg: encender %s: permiso denegado (%d)", nome, r);
        if (r == ERRO_ESPERA) aviso(_("wg-quick: demasiados intentos fallidos; espera un minuto\n"));
        else aviso(_("wg-quick: no se encendió: hace falta permiso de administrador\n"));
        return 1;
    }
    int tubo[2] = { -1, -1 };
    int pid = criar_pipe(tubo);
    if (pid >= 0) {
        char *args[] = { "wg", "--demonio", nome, tempos_prova ? "--tiempos-prueba" : NULL, NULL };
        int g0 = duplicar(ENTRADA, 28);
        duplicar(tubo[0], ENTRADA);
        pid = executar("/bin/wg", args);   /* hereda root: el demonio sigue siéndolo */
        if (g0 >= 0) { duplicar(g0, ENTRADA); fechar(g0); } else fechar(ENTRADA);
        fechar(tubo[0]);
        if (pid >= 0) definir_grupo(pid, 0);   /* su propio grupo: un Ctrl+C en el terminal no lo mata */
    }
    if (elevado) soltar_permiso();   /* este proceso vuelve a la cuenta normal en cuanto lanza el demonio */
    if (pid < 0) {
        if (tubo[1] >= 0) fechar(tubo[1]);
        conf_wg_apagar(&c);
        aviso(_("wg-quick: no se pudo lanzar el demonio: %s\n"), nome_erro(pid));
        return 1;
    }
    escrever_tudo(tubo[1], conf_bruto, (unsigned)conf_bruto_n);
    fechar(tubo[1]);
    conf_wg_apagar(&c);
    for (int t = 0; t < 100; t++) {   /* hasta 10 s (resolver el extremo puede tardar) */
        int cod;
        if (recolher(&cod) == pid) {
            const char *m = cod == D_ACESSO ? _("permiso denegado") : cod == D_OCUPADO ? _("ya hay un túnel") :
                            cod == D_RESOLVER ? _("no se pudo resolver el extremo") : _("error");
            aviso(_("wg-quick: el túnel no arrancó (%s)\n"), m);
            return 1;
        }
        if (info_wg(&v) > 0 && v.pid == (unsigned)pid && v.config.ip) {
            if (!calado) imprimir(_("[#] %s encendida (demonio: proceso %d). «wg show» da el estado.\n"), v.config.nome, pid);
            return 0;
        }
        dormir(100);
    }
    aviso(_("wg-quick: el demonio no respondió\n"));
    /* El demonio es root: matarlo no está al alcance de la cuenta normal; pedirle que salga sí. */
    if (info_wg(&v) > 0 && v.pid == (unsigned)pid) config_rede(REDE_TUNEL_DESLIGAR, NULL);
    matar(pid);
    return 1;
}

static int quick_down(const char *arg, bool calado)
{
    static info_wg_t v;
    if (info_wg(&v) <= 0) { aviso(_("wg-quick: no hay ningún túnel encendido\n")); return 1; }
    if (arg) {
        char nome[16];
        char caminho[160];
        caminho_de(arg, caminho, sizeof caminho);
        if (!nome_de_caminho(caminho, nome) || compara_texto(nome, v.config.nome) != 0) {
            aviso(_("wg-quick: %s no está encendida (la encendida es %s)\n"), arg, v.config.nome);
            return 1;
        }
    }
    int r = config_rede(REDE_TUNEL_DESLIGAR, NULL);
    if (r < 0) { aviso(_("wg-quick: no se pudo apagar: %s\n"), nome_erro(r)); return 1; }
    for (int t = 0; t < 30; t++) {   /* el demonio sale en cuanto lo ve */
        if (info_wg(&v) <= 0) {
            if (!calado) imprimir(_("[#] túnel apagado\n"));
            return 0;
        }
        dormir(100);
    }
    unsigned pid = v.pid;
    if (pid && matar((int)pid) == 0) {   /* no salió: se termina (el kernel quita la interfaz igual) */
        if (!calado) imprimir(_("[#] túnel apagado (el demonio no respondía)\n"));
        return 0;
    }
    aviso(_("wg-quick: el demonio no se apagó\n"));
    return 1;
}

static int cmd_quick(int argc, char **argv)
{
    if (argc < 1) goto uso;
    bool prova = argc >= 3 && compara_texto(argv[2], "--tiempos-prueba") == 0;
    if (compara_texto(argv[0], "up") == 0 && argc >= 2) return quick_up(argv[1], prova, false);
    if (compara_texto(argv[0], "down") == 0) return quick_down(argc >= 2 ? argv[1] : NULL, false);
    if ((compara_texto(argv[0], "check") == 0 || compara_texto(argv[0], "strip") == 0) && argc >= 2) {
        char caminho[160], erro[200];
        static conf_wg_t c;
        caminho_de(argv[1], caminho, sizeof caminho);
        if (!conf_ler(caminho, &c, erro, sizeof erro)) { imprimir_em(ERROS, "wg-quick: %s\n", erro); return 1; }
        resumo_conf(&c);
        conf_wg_apagar(&c);
        imprimir(_("[#] %s es correcto\n"), caminho);
        return 0;
    }
uso:
    imprimir_em(ERROS, _("uso: wg-quick up|down|check NOMBRE (el archivo es %s/NOMBRE.conf) o RUTA.conf\n"), DIR_WG);
    return 2;
}

#include "wg_ventana.h"

int principal(int argc, char **argv)
{
    if (argc >= 2 && compara_texto(argv[1], "--demonio") == 0 && argc >= 3)
        return demonio(argv[2], argc >= 4 && compara_texto(argv[3], "--tiempos-prueba") == 0);
    if (argc >= 2 && compara_texto(argv[1], "--quick") == 0) return cmd_quick(argc - 2, argv + 2);
    if (argc >= 2 && compara_texto(argv[1], "--ventana") == 0) return ventana_wg(argc, argv);
    if (argc >= 2 && compara_texto(argv[1], "genkey") == 0) return cmd_genkey();
    if (argc >= 2 && compara_texto(argv[1], "genpsk") == 0) return cmd_genpsk();
    if (argc >= 2 && compara_texto(argv[1], "pubkey") == 0) return cmd_pubkey();
    if (argc == 1 || compara_texto(argv[1], "show") == 0) return cmd_show(argc >= 3 ? argv[2] : NULL);
    imprimir_em(ERROS, _("uso: wg genkey | pubkey | genpsk | show [INTERFAZ]\n"
                         "     wg-quick up|down NOMBRE    (encender y apagar la VPN)\n"));
    return 2;
}
