// SPDX-License-Identifier: GPL-3.0-or-later
/* escaner - escáner de red propio de SaguiOS (D398). Escrito desde cero, sin código de terceros.
 *
 *   escaner [opciones] objetivo...
 *
 * Objetivos: una IP, un nombre, un rango de la última cifra (192.168.1.1-50) o una red
 * CIDR (192.168.1.0/24; como mucho /22, unas mil direcciones).
 *
 *   -sn            solo descubre equipos (no escanea puertos)
 *   -Pn            no descubre: trata todos los objetivos como activos
 *   -p LISTA       puertos: 22,80,1-1024 (-p- = todos)
 *   --top-ports N  los N puertos más comunes de la tabla propia
 *   -F             escaneo rápido: los 25 más comunes
 *   -sV            intenta leer el rótulo del servicio (banner o HEAD de HTTP)
 *   -T1..-T4       plazo por puerto y paralelismo (T1 lento ... T4 rápido; de serie T3)
 *   -oN FICHERO    guarda además el informe en un fichero
 *   -v             más detalle (puertos que se van descubriendo, cerrados y filtrados)
 *   -h, --ayuda    ayuda
 *
 * Diseño (lo que ofrece la pila de SaguiOS): no hay connect no bloqueante ni select sobre
 * varios sockets, así que el paralelismo son hilos (thread_create) que cogen tareas
 * (equipo, puerto) de una cola común; cada connect lleva su plazo (tempo_limite). El kernel
 * tiene 32 conexiones TCP, de ahí el tope de hilos. Una conexión aceptada es «abierto»,
 * una recusada (RST) «cerrado», y un plazo agotado «filtrado». El descubrimiento prueba
 * conexiones TCP a 80/443/22/445 (un RST también delata un equipo vivo) y, si no hay
 * respuesta, un eco ICMP (el kernel admite un eco a la vez por proceso: va en serie).
 *
 * Usar solo en redes propias o con permiso expreso. */
#include "sagui.h"

#define MAX_EQ        1024
#define MAX_PUERTOS   65536
#define MAX_RES       4096
#define MAX_HILOS     24

typedef struct { uint16_t puerto; const char *nombre; } serv_t;

/* Tabla propia puerto -> servicio. */
static const serv_t servicios[] = {
    {7, "echo"}, {9, "discard"}, {13, "daytime"}, {19, "chargen"}, {20, "ftp-data"}, {21, "ftp"},
    {22, "ssh"}, {23, "telnet"}, {25, "smtp"}, {37, "time"}, {43, "whois"}, {53, "dns"},
    {67, "dhcp"}, {69, "tftp"}, {79, "finger"}, {80, "http"}, {88, "kerberos"}, {110, "pop3"},
    {111, "rpcbind"}, {113, "ident"}, {119, "nntp"}, {123, "ntp"}, {135, "msrpc"}, {137, "netbios-ns"},
    {139, "netbios-ssn"}, {143, "imap"}, {161, "snmp"}, {179, "bgp"}, {389, "ldap"}, {427, "svrloc"},
    {443, "https"}, {445, "microsoft-ds"}, {465, "smtps"}, {500, "isakmp"}, {514, "syslog"}, {515, "printer"},
    {543, "klogin"}, {548, "afp"}, {554, "rtsp"}, {587, "submission"}, {631, "ipp"}, {636, "ldaps"},
    {873, "rsync"}, {902, "vmware"}, {993, "imaps"}, {995, "pop3s"}, {1080, "socks"}, {1194, "openvpn"},
    {1433, "ms-sql"}, {1521, "oracle"}, {1723, "pptp"}, {1883, "mqtt"}, {1900, "upnp"}, {2049, "nfs"},
    {2082, "cpanel"}, {2181, "zookeeper"}, {2375, "docker"}, {3000, "http-alt"}, {3128, "squid"},
    {3260, "iscsi"}, {3306, "mysql"}, {3389, "rdp"}, {3690, "svn"}, {4444, "krb524"}, {5000, "upnp"},
    {5060, "sip"}, {5222, "xmpp"}, {5353, "mdns"}, {5432, "postgresql"}, {5555, "adb"}, {5631, "pcanywhere"},
    {5900, "vnc"}, {5901, "vnc-1"}, {5984, "couchdb"}, {6000, "x11"}, {6379, "redis"}, {6667, "irc"},
    {7000, "afs3"}, {8000, "http-alt"}, {8008, "http-alt"}, {8080, "http-proxy"}, {8081, "http-alt"},
    {8443, "https-alt"}, {8888, "http-alt"}, {9000, "cslistener"}, {9090, "websm"}, {9100, "jetdirect"},
    {9200, "elasticsearch"}, {10000, "webmin"}, {11211, "memcached"}, {27017, "mongodb"}, {51820, "wireguard"},
    {0, NULL}
};

/* Puertos más comunes, de más a menos (orden propio). */
static const uint16_t comunes[] = {
    80, 443, 22, 21, 25, 53, 23, 110, 143, 445, 139, 3389, 8080, 3306, 993, 995, 135, 111, 587, 8443,
    5900, 1723, 8000, 8888, 5432, 631, 548, 21, 3000, 5000, 5060, 6379, 27017, 1433, 389, 636, 873, 2049,
    1900, 9100, 8081, 8008, 3128, 1080, 1194, 51820, 5353, 137, 161, 179, 465, 515, 554, 902, 1521, 1883,
    2375, 3260, 3690, 5222, 5555, 5984, 6000, 6667, 9000, 9090, 9200, 10000, 11211, 20, 37, 43, 69, 79,
    88, 113, 119, 123, 427, 500, 514, 543, 2082, 2181, 5631, 7000, 4444, 7, 9, 13, 19, 67, 5901, 8009,
    8082, 8888, 49152, 32768, 6001, 4000, 7070, 8181, 5001, 81
};
#define N_COMUNES ((int)(sizeof comunes / sizeof comunes[0]))

enum { E_CERRADO = 0, E_ABIERTO = 1, E_FILTRADO = 2 };

typedef struct {
    uint16_t equipo, puerto;
    uint8_t estado;
    char servicio[16];
    char version[72];
} res_t;

/* ---- configuración ---- */
static bool opt_sn, opt_pn, opt_sv, opt_v;
static int tempo_n = 3;
static unsigned plazo_ms = 1500;
static int hilos_n = 16;
static int fd_sal = -1;

static uint32_t ips[MAX_EQ];
static const char *nombres[MAX_EQ];
static int n_eq;
static bool vivo[MAX_EQ];
static unsigned lat_ms[MAX_EQ];
static uint16_t c_abre[MAX_EQ], c_cierra[MAX_EQ], c_filtra[MAX_EQ];

static uint16_t puertos[MAX_PUERTOS];
static int n_puertos;
static uint8_t visto[MAX_PUERTOS / 8];

static res_t res[MAX_RES];
static int n_res, perdidos_res;

static int vivos[MAX_EQ], n_vivos;
static unsigned prox_tarea, total_tareas;
static mutex_t m_cola = MUTEX_INICIAL, m_res = MUTEX_INICIAL, m_ping = MUTEX_INICIAL;
static volatile int err_fatal;

/* ---- salida: pantalla y, si hay -oN, fichero ---- */
static void sal(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void sal(const char *fmt, ...)
{
    char b[400];
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int n = formatar_lista(b, sizeof b, fmt, ap);
    __builtin_va_end(ap);
    if (n < 0) return;
    if (n >= (int)sizeof b) n = (int)sizeof b - 1;
    escrever_tudo(SAIDA, b, (unsigned)n);
    if (fd_sal >= 0) escrever_tudo(fd_sal, b, (unsigned)n);
}

static const char *nome_servico(int p)
{
    for (const serv_t *s = servicios; s->nombre; s++) if (s->puerto == p) return s->nombre;
    return "desconocido";
}

static void anadir_puerto(int p)
{
    if (p < 1 || p > 65535 || (visto[p >> 3] >> (p & 7) & 1)) return;
    visto[p >> 3] |= (uint8_t)(1 << (p & 7));
    puertos[n_puertos++] = (uint16_t)p;
}

/* "22,80,1-1024" -> puertos[]; false si está mal escrito. */
static bool leer_puertos(const char *s)
{
    if (compara_texto(s, "-") == 0) s = "1-65535";
    while (*s) {
        int a = 0, b;
        if (*s < '0' || *s > '9') return false;
        while (*s >= '0' && *s <= '9') { a = a * 10 + (*s++ - '0'); if (a > 65535) return false; }
        b = a;
        if (*s == '-') {
            s++;
            if (*s < '0' || *s > '9') return false;
            b = 0;
            while (*s >= '0' && *s <= '9') { b = b * 10 + (*s++ - '0'); if (b > 65535) return false; }
            if (b < a) return false;
        }
        if (a < 1) return false;
        for (int p = a; p <= b; p++) anadir_puerto(p);
        if (*s == ',') s++;
        else if (*s) return false;
    }
    return n_puertos > 0;
}

static bool solo_digitos(const char *s)
{
    if (!*s) return false;
    for (; *s; s++) if (*s < '0' || *s > '9') return false;
    return true;
}

static bool anadir_eq(uint32_t ip, const char *nombre)
{
    if (n_eq >= MAX_EQ) return false;
    ips[n_eq] = ip;
    nombres[n_eq++] = nombre;
    return true;
}

/* Un objetivo -> equipos. Devuelve false (y dice por qué) si no sirve. */
static bool leer_objetivo(const char *t)
{
    char c[80];
    copia_texto(c, t, sizeof c);
    uint32_t ip;
    char *barra = acha_char(c, '/');
    if (barra) {
        *barra = 0;
        int bits;
        if (!texto_para_ip(c, &ip) || !solo_digitos(barra + 1) || (bits = texto_para_int(barra + 1)) > 32) {
            imprimir_em(ERROS, "escaner: objetivo no válido: %s\n", t);
            return false;
        }
        if (bits < 22) { imprimir_em(ERROS, "escaner: %s: red demasiado grande (el mínimo es /22)\n", t); return false; }
        uint32_t mascara = bits ? 0xFFFFFFFFu << (32 - bits) : 0;
        uint32_t red = ip & mascara, ultimo = red | ~mascara;
        if (bits <= 30) { red++; ultimo--; }   /* sin la dirección de red ni la de difusión */
        for (uint32_t a = red; a <= ultimo; a++) if (!anadir_eq(a, NULL)) break;
        return true;
    }
    if (texto_para_ip(c, &ip)) return anadir_eq(ip, NULL);
    char *g = c;
    for (char *p = c; *p; p++) if (*p == '-') g = p;
    if (g != c && solo_digitos(g + 1)) {   /* a.b.c.d-e */
        *g = 0;
        int fin = texto_para_int(g + 1);
        if (texto_para_ip(c, &ip) && fin >= (int)(ip & 255) && fin <= 255) {
            for (uint32_t a = ip; a <= (ip & ~255u) + (uint32_t)fin; a++) if (!anadir_eq(a, NULL)) break;
            return true;
        }
        imprimir_em(ERROS, "escaner: rango no válido: %s\n", t);
        return false;
    }
    int r = resolver(t, &ip);
    if (r < 0) { imprimir_em(ERROS, "escaner: %s: %s\n", t, nome_erro(r)); return false; }
    return anadir_eq(ip, t);
}

/* ---- sondeo de un puerto ---- */

/* Abre una conexión a ip:puerto con plazo. Devuelve el fd (>=0) o un ERRO_*. */
static int conectar_plazo(uint32_t ip, int puerto, unsigned plazo)
{
    for (int intento = 0; intento < 150; intento++) {
        int fd = criar_socket(SOCK_TCP);
        if (fd < 0) {
            if (fd == ERRO_LIMITE || fd == ERRO_SEM_MEMORIA) { dormir(40); continue; }
            return fd;
        }
        tempo_limite(fd, plazo);
        int r = conectar(fd, ip, puerto);
        if (r == 0) return fd;
        fechar(fd);
        if (r == ERRO_LIMITE || r == ERRO_SEM_MEMORIA) { dormir(40); continue; }
        return r;
    }
    return ERRO_LIMITE;
}

static void limpiar(char *s)
{
    for (; *s; s++) if ((unsigned char)*s < 32 || (unsigned char)*s > 126) *s = '.';
}

static bool puerto_http(int p)
{
    return p == 80 || p == 8000 || p == 8008 || p == 8080 || p == 8081 || p == 8888 || p == 3000 || p == 5000 ||
           p == 9000 || p == 9090 || p == 81 || p == 8181 || p == 7070 || p == 631 || p == 5984 || p == 9200;
}

static bool empieza(const char *s, const char *pre)
{
    return compara_texto_n(s, pre, tam_texto(pre)) == 0;
}

/* Lee el rótulo del servicio de un socket ya conectado (-sV). */
static void sondear_version(int fd, uint32_t ip, int puerto, char *serv, size_t smax, char *ver, size_t vmax)
{
    char b[512];
    unsigned espera = plazo_ms < 800 ? 600 : (plazo_ms > 2000 ? 1500 : plazo_ms);
    tempo_limite(fd, espera);
    int n = 0;
    if (!puerto_http(puerto)) n = ler(fd, b, sizeof b - 1);   /* muchos servicios hablan primero */
    if (n <= 0) {
        char t[16], p[96];
        int k = formatar_texto(p, sizeof p, "HEAD / HTTP/1.0\r\nHost: %s\r\n\r\n", ip_para_texto(ip, t));
        if (escrever_tudo(fd, p, (unsigned)k) < 0) return;
        n = ler(fd, b, sizeof b - 1);
    }
    if (n <= 0) return;
    b[n] = 0;
    if (empieza(b, "HTTP/")) {
        copia_texto(serv, puerto == 443 || puerto == 8443 ? "https" : "http", smax);
        char *ln = NULL;
        for (const char *s = b; *s; s++) {
            if ((s == b || s[-1] == '\n') && (compara_texto_n(s, "Server:", 7) == 0 || compara_texto_n(s, "server:", 7) == 0)) {
                ln = (char *)s + 7;
                break;
            }
        }
        if (ln) {
            while (*ln == ' ') ln++;
            copia_texto(ver, ln, vmax);
        } else {
            copia_texto(ver, b, vmax);
        }
    } else {
        if (empieza(b, "SSH-")) copia_texto(serv, "ssh", smax);
        else if (empieza(b, "220") && puerto != 21 && puerto != 25 && puerto != 587 && puerto != 465) copia_texto(serv, "ftp/smtp", smax);
        else if (empieza(b, "+OK")) copia_texto(serv, "pop3", smax);
        else if (empieza(b, "* OK")) copia_texto(serv, "imap", smax);
        else if (empieza(b, "RFB ")) copia_texto(serv, "vnc", smax);
        copia_texto(ver, b, vmax);
    }
    char *f = ver;
    for (; *f; f++) if (*f == '\r' || *f == '\n') { *f = 0; break; }   /* solo la primera línea */
    limpiar(ver);
}

static void guardar_res(int eq, int puerto, int estado, const char *serv, const char *ver)
{
    mutex_tomar(&m_res);
    if (estado == E_ABIERTO) c_abre[eq]++; else if (estado == E_CERRADO) c_cierra[eq]++; else c_filtra[eq]++;
    if (estado == E_ABIERTO) {
        if (n_res < MAX_RES) {
            res_t *r = &res[n_res++];
            r->equipo = (uint16_t)eq;
            r->puerto = (uint16_t)puerto;
            r->estado = (uint8_t)estado;
            copia_texto(r->servicio, serv, sizeof r->servicio);
            copia_texto(r->version, ver, sizeof r->version);
        } else perdidos_res++;
        if (opt_v) {
            char t[16];
            sal("Puerto abierto descubierto: %d/tcp en %s\n", puerto, ip_para_texto(ips[eq], t));
        }
    }
    mutex_largar(&m_res);
}

static void *trab_puertos(void *arg)
{
    (void)arg;
    for (;;) {
        mutex_tomar(&m_cola);
        unsigned k = prox_tarea++;
        mutex_largar(&m_cola);
        if (k >= total_tareas || err_fatal) break;
        int eq = vivos[k / (unsigned)n_puertos];
        int puerto = puertos[k % (unsigned)n_puertos];
        int fd = conectar_plazo(ips[eq], puerto, plazo_ms);
        if (fd >= 0) {
            char serv[16], ver[72];
            copia_texto(serv, nome_servico(puerto), sizeof serv);
            ver[0] = 0;
            if (opt_sv) sondear_version(fd, ips[eq], puerto, serv, sizeof serv, ver, sizeof ver);
            fechar(fd);
            guardar_res(eq, puerto, E_ABIERTO, serv, ver);
        } else if (fd == ERRO_RECUSADA || fd == ERRO_CONEXAO_CAIU) {
            guardar_res(eq, puerto, E_CERRADO, "", "");
        } else if (fd == ERRO_SEM_REDE) {
            err_fatal = fd;
        } else {
            guardar_res(eq, puerto, E_FILTRADO, "", "");
        }
    }
    return NULL;
}

/* ---- descubrimiento ---- */
static void *trab_descubrir(void *arg)
{
    (void)arg;
    static const uint16_t sonda[] = { 80, 443, 22, 445 };
    for (;;) {
        mutex_tomar(&m_cola);
        unsigned k = prox_tarea++;
        mutex_largar(&m_cola);
        if (k >= total_tareas || err_fatal) break;
        uint32_t ip = ips[k];
        bool arriba = false, sin_ruta = false, plazos = false;
        unsigned ini = tempo_ms(), lat = 0;
        for (int i = 0; i < 4 && !arriba && !sin_ruta; i++) {
            ini = tempo_ms();
            int fd = conectar_plazo(ip, sonda[i], plazo_ms);
            if (fd >= 0) { fechar(fd); arriba = true; }
            else if (fd == ERRO_RECUSADA || fd == ERRO_CONEXAO_CAIU) arriba = true;
            else if (fd == ERRO_INALCANCAVEL) sin_ruta = true;
            else if (fd == ERRO_SEM_REDE) { err_fatal = fd; break; }
            else plazos = true;
            lat = tempo_ms() - ini;
        }
        if (!arriba && !sin_ruta && plazos) {   /* ¿responde a ICMP aunque filtre TCP? */
            mutex_tomar(&m_ping);
            info_ping_t p = { 1, 0, 0, 0 };
            p.tamanho = 1;
            int r = ping(ip, &p, plazo_ms < 500 ? 500 : plazo_ms);
            mutex_largar(&m_ping);
            if (r == 0) { arriba = true; lat = (p.us + 500) / 1000; }
        }
        mutex_tomar(&m_res);
        vivo[k] = arriba;
        lat_ms[k] = lat;
        if (arriba && opt_v) { char t[16]; sal("Equipo activo descubierto: %s\n", ip_para_texto(ip, t)); }
        mutex_largar(&m_res);
    }
    return NULL;
}

static void lanzar(void *(*f)(void *), unsigned tareas)
{
    prox_tarea = 0;
    total_tareas = tareas;
    int n = hilos_n;
    if ((unsigned)n > tareas) n = (int)tareas;
    thread_t *h[MAX_HILOS];
    int k = 0;
    for (int i = 1; i < n; i++) {   /* el hilo principal también trabaja */
        h[k] = thread_create(f, NULL, 64 * 1024);
        if (h[k]) k++;
    }
    f(NULL);
    for (int i = 0; i < k; i++) thread_join(h[i]);
}

static void ordenar(void)
{
    for (int i = 1; i < n_res; i++) {   /* inserción: por equipo y puerto (ya es casi un orden) */
        res_t t = res[i];
        int j = i - 1;
        while (j >= 0 && (res[j].equipo > t.equipo || (res[j].equipo == t.equipo && res[j].puerto > t.puerto))) { res[j + 1] = res[j]; j--; }
        res[j + 1] = t;
    }
}

static void ayuda(int fd)
{
    imprimir_em(fd,
        "uso: escaner [opciones] objetivo...\n"
        "  objetivo: IP, nombre, rango 192.168.1.1-50 o red 192.168.1.0/24 (hasta /22)\n"
        "  -sn            solo descubrir equipos        -Pn   no descubrir (todos activos)\n"
        "  -p LISTA       puertos (22,80,1-1024; -p- todos)\n"
        "  --top-ports N  los N puertos más comunes     -F    rápido (25 más comunes)\n"
        "  -sV            leer el rótulo del servicio   -v    más detalle\n"
        "  -T1..-T4       lento ... rápido (de serie 3) -oN fichero  guardar el informe\n"
        "Escanea solo redes propias o con permiso.\n");
}

int principal(int argc, char **argv)
{
    int top = 0;
    const char *lista_p = NULL, *fich = NULL;
    int n_obj = 0;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (compara_texto(a, "-h") == 0 || compara_texto(a, "--ayuda") == 0 || compara_texto(a, "--help") == 0) { ayuda(SAIDA); return 0; }
        else if (compara_texto(a, "-sn") == 0) opt_sn = true;
        else if (compara_texto(a, "-Pn") == 0) opt_pn = true;
        else if (compara_texto(a, "-sV") == 0) opt_sv = true;
        else if (compara_texto(a, "-v") == 0) opt_v = true;
        else if (compara_texto(a, "-F") == 0) top = 25;
        else if (compara_texto(a, "-p") == 0 && i + 1 < argc) lista_p = argv[++i];
        else if (empieza(a, "-p") && a[2]) lista_p = a + 2;
        else if (compara_texto(a, "--top-ports") == 0 && i + 1 < argc) top = texto_para_int(argv[++i]);
        else if (compara_texto(a, "-oN") == 0 && i + 1 < argc) fich = argv[++i];
        else if (a[0] == '-' && a[1] == 'T' && a[2] >= '1' && a[2] <= '5' && !a[3]) tempo_n = a[2] - '0';
        else if (a[0] == '-' && a[1]) { imprimir_em(ERROS, "escaner: opción desconocida: %s\n", a); ayuda(ERROS); return 2; }
        else n_obj++;
    }
    if (n_obj == 0) { ayuda(ERROS); return 2; }
    switch (tempo_n) {
        case 1: plazo_ms = 5000; hilos_n = 4; break;
        case 2: plazo_ms = 3000; hilos_n = 8; break;
        case 3: plazo_ms = 1500; hilos_n = 16; break;
        default: plazo_ms = tempo_n == 4 ? 800 : 400; hilos_n = MAX_HILOS; break;
    }
    if (lista_p) {
        if (!leer_puertos(lista_p)) { imprimir_em(ERROS, "escaner: lista de puertos no válida: %s\n", lista_p); return 2; }
    } else {
        if (top <= 0) top = 100;
        if (top > N_COMUNES) top = N_COMUNES;
        for (int i = 0; i < top; i++) anadir_puerto(comunes[i]);
    }
    for (int i = 1; i < argc; i++) {   /* segunda pasada: los objetivos */
        const char *a = argv[i];
        if (a[0] == '-' && a[1]) {
            if (compara_texto(a, "-p") == 0 || compara_texto(a, "--top-ports") == 0 || compara_texto(a, "-oN") == 0) i++;
            continue;
        }
        if (!leer_objetivo(a)) return 2;
    }
    if (n_eq == 0) { imprimir_em(ERROS, "escaner: ningún objetivo\n"); return 2; }
    if (fich) {
        fd_sal = abrir(fich, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (fd_sal < 0) { imprimir_em(ERROS, "escaner: %s: %s\n", fich, nome_erro(fd_sal)); return 1; }
    }

    unsigned t0 = tempo_ms();
    sal("Escáner de red de SaguiOS (solo redes propias o con permiso)\n");
    if (opt_pn || (!opt_sn && n_eq == 1)) {
        for (int i = 0; i < n_eq; i++) vivo[i] = true;
    } else {
        lanzar(trab_descubrir, (unsigned)n_eq);
    }
    if (err_fatal) { imprimir_em(ERROS, "escaner: %s\n", nome_erro(err_fatal)); return 1; }
    for (int i = 0; i < n_eq; i++) if (vivo[i]) vivos[n_vivos++] = i;

    if (opt_sn) {
        for (int i = 0; i < n_vivos; i++) {
            int e = vivos[i];
            char t[16];
            if (nombres[e]) sal("Equipo %s (%s) activo, respuesta en %u ms\n", nombres[e], ip_para_texto(ips[e], t), lat_ms[e]);
            else sal("Equipo %s activo, respuesta en %u ms\n", ip_para_texto(ips[e], t), lat_ms[e]);
        }
        unsigned dt = tempo_ms() - t0;
        sal("Hecho: %d direcciones escaneadas, %d activas, en %u.%02u s\n", n_eq, n_vivos, dt / 1000, dt % 1000 / 10);
        if (fd_sal >= 0) fechar(fd_sal);
        return 0;
    }

    if (n_vivos)
        lanzar(trab_puertos, (unsigned)n_vivos * (unsigned)n_puertos);
    if (err_fatal) { imprimir_em(ERROS, "escaner: %s\n", nome_erro(err_fatal)); return 1; }
    ordenar();
    int r = 0;
    for (int i = 0; i < n_vivos; i++) {
        int e = vivos[i];
        char t[16];
        ip_para_texto(ips[e], t);
        if (nombres[e]) sal("\nInforme de escaneo de %s (%s)\n", nombres[e], t);
        else sal("\nInforme de escaneo de %s\n", t);
        if (!opt_pn && n_eq > 1) sal("Equipo activo (%u ms)\n", lat_ms[e]);
        if (c_abre[e]) {
            sal("%-10s %-9s %-14s %s\n", "PUERTO", "ESTADO", "SERVICIO", opt_sv ? "VERSIÓN" : "");
            for (int k = 0; k < n_res; k++) {
                if (res[k].equipo != e) continue;
                char p[12];
                formatar_texto(p, sizeof p, "%u/tcp", res[k].puerto);
                sal("%-10s %-9s %-14s %s\n", p, "abierto", res[k].servicio, res[k].version);
            }
        } else {
            sal("No hay puertos abiertos entre los %d escaneados\n", n_puertos);
        }
        sal("No se muestran: %u puertos cerrados, %u filtrados\n", c_cierra[e], c_filtra[e]);
    }
    if (perdidos_res) sal("Aviso: demasiados puertos abiertos, %d sin listar\n", perdidos_res);
    unsigned dt = tempo_ms() - t0;
    sal("\nHecho: %d direcciones (%d activas) escaneadas en %u.%02u s\n", n_eq, n_vivos, dt / 1000, dt % 1000 / 10);
    if (fd_sal >= 0) fechar(fd_sal);
    return r;
}
