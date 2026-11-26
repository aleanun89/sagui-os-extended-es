// SPDX-License-Identifier: GPL-3.0-only
/* dav - unidades de red WebDAV montadas en /red/<nombre> (D485).
 *
 *   dav agregar <nombre> <url> [usuario] [--auto] [--cacert F] [--inseguro] [--sin-clave]
 *   dav montar <nombre>        dav desmontar <nombre>      dav borrar <nombre>
 *   dav estado                 dav lista                   dav probar <nombre>
 *   dav iniciar                (las de «conectar al iniciar»; lo lanza el escritorio al entrar)
 *   dav llavero estado|abrir|contrasena
 *   dav servir <nombre>        (el servicio de una unidad; lo lanza «dav montar»)
 *
 * El servicio monta la unidad con SIS_PONTE y atiende lo que los programas hacen en ella: PROPFIND (Depth 0 y 1),
 * GET (con Range para leer a trozos los ficheros grandes), PUT al cerrar desde un fichero temporal, DELETE,
 * MKCOL y MOVE, con autenticación Basic y HTTPS con la verificación de siempre. Publica en el kernel si hay
 * conexión; sin ella el kernel contesta solo, al momento. La contraseña sale del llavero (chaveiro.h), nunca de
 * la línea de órdenes ni de un fichero en claro, y no se escribe en ningún registro. */
#include "sagui.h"
#include "tls.h"
#include "cripto.h"
#include "contas.h"
#include "chaveiro.h"
#include "unidades_red.h"

#define CONECTAR_MS     5000
#define LEER_MS         15000
#define PROBAR_CONECTADA_MS    15000
#define PROBAR_DESCONECTADA_MS 5000
#define GRANDE          (8u * 1024 * 1024)   /* de aquí para arriba, leer por Range sin descargarlo entero */
#define BLOCO_RANGO     (256u * 1024)
#define MAX_ABERTOS     32
#define MAX_CACHE       64
#define CACHE_MS        5000
#define MAX_PENDENTES   16
#define CORPO_MAX       (4u * 1024 * 1024)
#define CAMINHO_MAX     128

static const char *const AJUDA =
    "uso: dav agregar <nombre> <url> [usuario] [--auto] [--cacert F] [--inseguro] [--sin-clave]\n"
    "     dav montar|desmontar|borrar|probar <nombre>\n"
    "     dav estado | lista | iniciar\n"
    "     dav llavero estado|abrir|cerrar|contrasena\n"
    "Las unidades se montan en /red/<nombre>, solo las ve tu cuenta y nada se ejecuta desde ellas.\n"
    "Las contraseñas se guardan cifradas en tu llavero (~/.config/llavero).\n";

/* ================================================================ HTTP y WebDAV */

static unidade_red_t ur;
static url_t base;                 /* host, puerto y carpeta base (terminada en «/», ya codificada) */
static char autorizacao[400];      /* "Authorization: Basic ...\r\n" o "" */
static tls_config_t tlscfg;
static bool silencio;              /* «dav iniciar»: sin mensajes en la terminal */

static void base64(const unsigned char *d, int n, char *s)
{
    static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int k = 0;
    for (int i = 0; i < n; i += 3) {
        unsigned v = (unsigned)d[i] << 16 | (i + 1 < n ? (unsigned)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        s[k++] = t[v >> 18 & 63];
        s[k++] = t[v >> 12 & 63];
        s[k++] = i + 1 < n ? t[v >> 6 & 63] : '=';
        s[k++] = i + 2 < n ? t[v & 63] : '=';
    }
    s[k] = 0;
}

/* Prepara base, autorizacao y tlscfg para la unidad. false = URL inválida. */
static bool preparar(const unidade_red_t *u, const char *senha, char *erro, int max)
{
    ur = *u;
    if (!url_separar(u->url, &base)) { formatar_texto(erro, (size_t)max, "dirección no válida: %s", u->url); return false; }
    int n = (int)tam_texto(base.caminho);
    if (n == 0 || base.caminho[n - 1] != '/') junta_texto(base.caminho, "/", sizeof base.caminho);
    autorizacao[0] = 0;
    if (u->usuario[0]) {
        char par[UR_USUARIO_MAX + CHAVEIRO_VALOR_MAX + 2], b64[sizeof autorizacao - 32];
        formatar_texto(par, sizeof par, "%s:%s", u->usuario, senha ? senha : "");
        base64((const unsigned char *)par, (int)tam_texto(par), b64);
        formatar_texto(autorizacao, sizeof autorizacao, "Authorization: Basic %s\r\n", b64);
        cripto_apagar(par, sizeof par);
        cripto_apagar(b64, sizeof b64);
    }
    if (!tls_config_padrao(&tlscfg, u->cacert[0] ? u->cacert : NULL, erro, (unsigned)max)) return false;
    tlscfg.inseguro = u->inseguro;
    return true;
}

static int hexv(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

/* ¿Vale la ruta relativa que manda el kernel? Sin «.» ni «..» ni bytes de control. */
static bool rel_valida(const char *rel)
{
    const char *seg = rel;
    for (const char *p = rel; ; p++) {
        if (*p == '/' || !*p) {
            int n = (int)(p - seg);
            if ((n == 1 && seg[0] == '.') || (n == 2 && seg[0] == '.' && seg[1] == '.')) return false;
            if (n == 0 && *p == '/') return false;
            if (!*p) return true;
            seg = p + 1;
        } else if ((unsigned char)*p < 0x20 || *p == 0x7F) return false;
    }
}

/* La ruta (Latin-1) codificada para la URL: los bytes altos pasan a UTF-8 y van en %XX. */
static bool codificar(const char *rel, char *s, int max)
{
    static const char hx[] = "0123456789ABCDEF";
    int k = 0;
    for (const unsigned char *p = (const unsigned char *)rel; *p; p++) {
        unsigned char c = *p;
        unsigned char b[2];
        int nb = 1;
        b[0] = c;
        if (c >= 0x80) { b[0] = (unsigned char)(0xC0 | c >> 6); b[1] = (unsigned char)(0x80 | (c & 0x3F)); nb = 2; }
        for (int i = 0; i < nb; i++) {
            unsigned char x = b[i];
            bool livre = (x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z') || (x >= '0' && x <= '9') ||
                         x == '-' || x == '.' || x == '_' || x == '~' || x == '/';
            if (k + 4 >= max) return false;
            if (livre) s[k++] = (char)x;
            else { s[k++] = '%'; s[k++] = hx[x >> 4]; s[k++] = hx[x & 15]; }
        }
    }
    s[k] = 0;
    return true;
}

/* %XX y UTF-8 -> Latin-1 (lo que no cabe, «?»), y las entidades de XML. En el sitio. */
static void decodificar(char *s)
{
    char *o = s;
    for (char *p = s; *p; ) {
        if (*p == '%' && hexv(p[1]) >= 0 && hexv(p[2]) >= 0) { *o++ = (char)(hexv(p[1]) << 4 | hexv(p[2])); p += 3; }
        else if (*p == '&') {
            if (comeca_com(p, "&amp;")) { *o++ = '&'; p += 5; }
            else if (comeca_com(p, "&lt;")) { *o++ = '<'; p += 4; }
            else if (comeca_com(p, "&gt;")) { *o++ = '>'; p += 4; }
            else if (comeca_com(p, "&quot;")) { *o++ = '"'; p += 6; }
            else if (comeca_com(p, "&apos;")) { *o++ = '\''; p += 6; }
            else *o++ = *p++;
        } else *o++ = *p++;
    }
    *o = 0;
    /* UTF-8 -> Latin-1 */
    o = s;
    for (unsigned char *p = (unsigned char *)s; *p; ) {
        unsigned c = *p;
        if (c < 0x80) { *o++ = (char)c; p++; }
        else if ((c & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
            unsigned v = (c & 0x1F) << 6 | (p[1] & 0x3F);
            *o++ = v < 0x100 ? (char)v : '?';
            p += 2;
        } else {
            int n = (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
            *o++ = '?';
            while (n-- && *p) p++;
        }
    }
    *o = 0;
}

typedef struct {
    int  codigo;
    char cab[1024];
} resp_t;

static int estado_rede = PONTE_EST_CONECTANDO;
static int canal = -1;
static void marcar_estado(int e);

/* Abre la conexión (TCP con plazo corto y, si es https, TLS). */
static int abrir_conexao(conexao_t *c)
{
    memset(c, 0, sizeof *c);
    c->fd = conectar_tcp(base.host, base.porta, CONECTAR_MS);
    if (c->fd < 0) { int r = c->fd; c->fd = -1; return r; }
    opcao_socket(c->fd, OPC_TEMPO_LIMITE, LEER_MS);
    if (!base.https) return 0;
    tls_config_t k = tlscfg;
    k.host = base.host;
    c->tls = tls_novo(&k, c->fd);
    if (!c->tls) { conexao_fechar(c); return ERRO_SEM_MEMORIA; }
    int r = tls_conectar(c->tls);
    if (r < 0) {
        registrar("dav %s: TLS: %s", ur.nome, tls_erro(c->tls));
        conexao_fechar(c);
        return r == TLS_ERRO_CERT ? ERRO_ACESSO : ERRO_SEM_REDE;
    }
    return 0;
}

static bool erro_de_rede(int r) { return r == ERRO_SEM_REDE || r == ERRO_TEMPO || r == ERRO_RECUSADA || r == ERRO_INALCANCAVEL || r == ERRO_CONEXAO_CAIU || r == TLS_ERRO_REDE; }

/* Un pedido HTTP: método, ruta relativa (codificada aquí), campos de más, cuerpo (de memoria o de un fichero) y
 * dónde va la respuesta (un fichero, un búfer nuevo o nada). Devuelve 0 (con r->codigo) o un error de red. */
static int trocar(const char *metodo, const char *rel, bool barra, const char *extra,
                  const void *corpo, unsigned ncorpo, int fd_corpo, unsigned tam_fd,
                  int fd_saida, char **saida, unsigned *nsaida, resp_t *r)
{
    url_t u = base;
    char cod[3 * CAMINHO_MAX + 8];
    if (!codificar(rel, cod, sizeof cod)) return ERRO_INVALIDO;
    junta_texto(u.caminho, cod, sizeof u.caminho);
    if (barra && u.caminho[tam_texto(u.caminho) - 1] != '/') junta_texto(u.caminho, "/", sizeof u.caminho);
    char campos[900];
    bool com_corpo = corpo || fd_corpo >= 0 || !compara_texto(metodo, "PUT") || !compara_texto(metodo, "PROPFIND") ||
                     !compara_texto(metodo, "MKCOL");
    formatar_texto(campos, sizeof campos, "%s%s", autorizacao, extra ? extra : "");
    if (com_corpo) {
        char cl[40];
        formatar_texto(cl, sizeof cl, "Content-Length: %u\r\n", fd_corpo >= 0 ? tam_fd : ncorpo);
        junta_texto(campos, cl, sizeof campos);
    }
    char pedido[2048];
    int np = http_montar_pedido(&u, metodo, campos, pedido, sizeof pedido);
    cripto_apagar(campos, sizeof campos);
    conexao_t c;
    int e = abrir_conexao(&c);
    if (e < 0) { cripto_apagar(pedido, sizeof pedido); return e; }
    e = conexao_escrever(&c, pedido, (unsigned)np);
    cripto_apagar(pedido, sizeof pedido);
    if (e >= 0 && corpo && ncorpo) e = conexao_escrever(&c, corpo, ncorpo);
    if (e >= 0 && fd_corpo >= 0) {
        static char tr[16384];
        posicionar(fd_corpo, 0, POS_INICIO);
        unsigned feito = 0;
        while (feito < tam_fd) {
            int k = ler(fd_corpo, tr, tam_fd - feito > sizeof tr ? sizeof tr : tam_fd - feito);
            if (k <= 0) { e = ERRO_INVALIDO; break; }
            if ((e = conexao_escrever(&c, tr, (unsigned)k)) < 0) break;
            feito += (unsigned)k;
        }
    }
    if (e < 0) { conexao_fechar(&c); return e == ERRO_INVALIDO ? e : ERRO_CONEXAO_CAIU; }
    static http_resposta_t hr;
    e = http_ler_cabecalho_de(&hr, conexao_ler, &c);
    if (e < 0) { conexao_fechar(&c); return ERRO_CONEXAO_CAIU; }
    r->codigo = hr.codigo;
    copia_texto(r->cab, hr.cabecalho, sizeof r->cab);
    bool head = !compara_texto(metodo, "HEAD");
    char *buf = NULL;
    unsigned nb = 0, cap = 0;
    static char tmp[16384];
    while (!head) {
        int k = http_ler_corpo(&hr, tmp, sizeof tmp);
        if (k < 0) { e = ERRO_CONEXAO_CAIU; break; }
        if (k == 0) break;
        if (fd_saida >= 0 && hr.codigo / 100 == 2) {
            if (escrever_tudo(fd_saida, tmp, (unsigned)k) < 0) { e = ERRO_SEM_ESPACO; break; }
        } else if (saida) {
            if (nb + (unsigned)k + 1 > cap) {
                unsigned nc = cap ? cap * 2 : 32768;
                while (nc < nb + (unsigned)k + 1) nc *= 2;
                if (nc > CORPO_MAX) { e = ERRO_LIMITE; break; }
                char *x = realoca(buf, nc);
                if (!x) { e = ERRO_SEM_MEMORIA; break; }
                buf = x;
                cap = nc;
            }
            memcpy(buf + nb, tmp, (unsigned)k);
            nb += (unsigned)k;
        }
    }
    conexao_fechar(&c);
    if (e < 0) { libera(buf); return e; }
    if (saida) {
        if (!buf) buf = aloca(1);
        if (buf) buf[nb] = 0;
        *saida = buf;
        if (nsaida) *nsaida = nb;
    }
    return 0;
}

/* El código HTTP -> error de SaguiOS. */
static int erro_http(int c)
{
    if (c / 100 == 2) return 0;
    switch (c) {
    case 401: case 403: return ERRO_ACESSO;
    case 404: case 410: case 409: return ERRO_NAO_EXISTE;
    case 405: case 412: return ERRO_JA_EXISTE;
    case 423: return ERRO_OCUPADO;
    case 507: case 413: return ERRO_SEM_ESPACO;
    default:  return ERRO_INVALIDO;
    }
}

/* ---------------------------------------------------------------- PROPFIND y su XML */

typedef struct {
    char     nome[NOME_MAX];
    uint8_t  tipo;
    uint32_t tamanho, modificado;
} item_t;

/* La siguiente etiqueta (de apertura o de cierre) cuyo nombre local (sin el prefijo «D:») es "nome", entre p y fim. */
static const char *achar_tag(const char *p, const char *fim, const char *nome, bool cierre)
{
    int n = (int)tam_texto(nome);
    for (; p < fim; p++) {
        if (*p != '<') continue;
        const char *q = p + 1;
        bool fecha = *q == '/';
        if (fecha) q++;
        if (fecha != cierre) continue;
        const char *ini = q;
        while (q < fim && *q != '>' && *q != ' ' && *q != '/' && *q != '\t' && *q != '\n' && *q != '\r') q++;
        const char *loc = ini;
        for (const char *z = ini; z < q; z++) if (*z == ':') loc = z + 1;
        if (q - loc == n && compara_texto_n(loc, nome, (size_t)n) == 0) return p;
    }
    return NULL;
}

/* El texto dentro de <nome>...</nome> en [p, fim), copiado a s. */
static bool texto_tag(const char *p, const char *fim, const char *nome, char *s, int max)
{
    const char *a = achar_tag(p, fim, nome, false);
    if (!a) return false;
    const char *b = acha_char(a, '>');
    if (!b || b >= fim) return false;
    if (b[-1] == '/') { s[0] = 0; return true; }   /* <x/> */
    b++;
    const char *z = achar_tag(b, fim, nome, true);
    if (!z) return false;
    int k = 0;
    while (b < z && (*b == ' ' || *b == '\n' || *b == '\r' || *b == '\t')) b++;
    for (; b < z && k < max - 1; b++) s[k++] = *b;
    while (k && (s[k - 1] == ' ' || s[k - 1] == '\n' || s[k - 1] == '\r' || s[k - 1] == '\t')) k--;
    s[k] = 0;
    return true;
}

/* «Wed, 26 Nov 2026 10:00:00 GMT» -> segundos desde 1970 (UTC). 0 si no se entiende. */
static uint32_t data_http(const char *s)
{
    static const char *const meses = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *p = acha_char(s, ',');
    p = p ? p + 1 : s;
    while (*p == ' ') p++;
    int d = 0, a = 0, h = 0, mi = 0, se = 0, m = -1;
    while (*p >= '0' && *p <= '9') d = d * 10 + (*p++ - '0');
    while (*p == ' ') p++;
    for (int i = 0; i < 12; i++) if (compara_texto_n(p, meses + 3 * i, 3) == 0) m = i;
    if (m < 0) return 0;
    p += 3;
    while (*p == ' ') p++;
    while (*p >= '0' && *p <= '9') a = a * 10 + (*p++ - '0');
    while (*p == ' ') p++;
    while (*p >= '0' && *p <= '9') h = h * 10 + (*p++ - '0');
    if (*p == ':') p++;
    while (*p >= '0' && *p <= '9') mi = mi * 10 + (*p++ - '0');
    if (*p == ':') p++;
    while (*p >= '0' && *p <= '9') se = se * 10 + (*p++ - '0');
    if (a < 1970 || d < 1 || d > 31) return 0;
    /* días desde 1970 (algoritmo de los días civiles) */
    int y = a, mm = m + 1;
    y -= mm <= 2;
    int era = y / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (unsigned)((153 * (mm + (mm > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int dias = era * 146097 + (int)doe - 719468;
    return (uint32_t)dias * 86400u + (uint32_t)(h * 3600 + mi * 60 + se);
}

/* La ruta de un href, sin el esquema ni el servidor, decodificada y sin «/» al final. */
static void href_caminho(char *h)
{
    decodificar(h);
    char *p = h;
    if (comeca_com(p, "http://") || comeca_com(p, "https://")) {
        p = acha_char(p + 8, '/');
        if (!p) { h[0] = '/'; h[1] = 0; return; }
    }
    memmove(h, p, tam_texto(p) + 1);
    int n = (int)tam_texto(h);
    while (n > 1 && h[n - 1] == '/') h[--n] = 0;
}

static int achar_resposta(const char *xml, const char *fim, item_t *eu, bool *tem_eu, item_t **filhos, int *nf,
                          const char *alvo)
{
    int cap = 0;
    *nf = 0;
    *filhos = NULL;
    *tem_eu = false;
    for (const char *p = xml; (p = achar_tag(p, fim, "response", false)); ) {
        const char *z = achar_tag(p, fim, "response", true);
        if (!z) break;
        char href[3 * CAMINHO_MAX + 64], v[64];
        if (!texto_tag(p, z, "href", href, sizeof href)) { p = z; continue; }
        href_caminho(href);
        item_t it;
        memset(&it, 0, sizeof it);
        /* Solo las propiedades con 200: un propstat con 404 dice lo que falta. */
        it.tipo = achar_tag(p, z, "collection", false) ? TIPO_PASTA : TIPO_ARQUIVO;
        if (texto_tag(p, z, "getcontentlength", v, sizeof v)) it.tamanho = (uint32_t)texto_para_int(v);
        if (texto_tag(p, z, "getlastmodified", v, sizeof v)) it.modificado = data_http(v);
        const char *barra = acha_ultimo_char(href, '/');
        copia_texto(it.nome, barra ? barra + 1 : href, sizeof it.nome);
        if (compara_texto(href, alvo) == 0) { *eu = it; *tem_eu = true; }
        else if (it.nome[0]) {
            if (*nf >= cap) {
                cap = cap ? cap * 2 : 32;
                item_t *x = realoca(*filhos, (size_t)cap * sizeof(item_t));
                if (!x) break;
                *filhos = x;
            }
            (*filhos)[(*nf)++] = it;
        }
        p = z;
    }
    return 0;
}

static const char PROPFIND_CORPO[] =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<D:propfind xmlns:D=\"DAV:\"><D:prop>"
    "<D:resourcetype/><D:getcontentlength/><D:getlastmodified/></D:prop></D:propfind>\n";

/* El alvo (la ruta del servidor, decodificada y sin «/» final) de una ruta relativa. */
static void alvo_de(const char *rel, char *s, int max)
{
    copia_texto(s, base.caminho, (size_t)max);
    decodificar(s);
    junta_texto(s, rel, (size_t)max);
    int n = (int)tam_texto(s);
    while (n > 1 && s[n - 1] == '/') s[--n] = 0;
}

/* PROPFIND de "rel". depth 0: solo *eu; depth 1: también los hijos (libera *filhos el que llama). */
static int propfind(const char *rel, int depth, item_t *eu, item_t **filhos, int *nf)
{
    char extra[96];
    formatar_texto(extra, sizeof extra, "Depth: %d\r\nContent-Type: application/xml; charset=utf-8\r\n", depth);
    resp_t r;
    char *xml = NULL;
    unsigned n = 0;
    bool barra = depth == 1 || !rel[0];
    int e = trocar("PROPFIND", rel, barra, extra, PROPFIND_CORPO, sizeof PROPFIND_CORPO - 1, -1, 0, -1, &xml, &n, &r);
    if (e < 0) return e;
    if (r.codigo == 301 || r.codigo == 302 || r.codigo == 307 || r.codigo == 308) {   /* una carpeta pedida sin «/» */
        libera(xml);
        xml = NULL;
        e = trocar("PROPFIND", rel, true, extra, PROPFIND_CORPO, sizeof PROPFIND_CORPO - 1, -1, 0, -1, &xml, &n, &r);
        if (e < 0) return e;
    }
    if (r.codigo != 207) { libera(xml); return r.codigo / 100 == 2 ? ERRO_FORMATO : erro_http(r.codigo); }
    char alvo[3 * CAMINHO_MAX + 64];
    alvo_de(rel, alvo, sizeof alvo);
    item_t *fs = NULL;
    int k = 0;
    bool tem;
    achar_resposta(xml, xml + n, eu, &tem, &fs, &k, alvo);
    libera(xml);
    if (!tem && depth == 0 && k == 1) { *eu = fs[0]; tem = true; k = 0; }   /* un servidor que devuelve otro href */
    if (!tem) { libera(fs); return rel[0] ? ERRO_NAO_EXISTE : 0; }
    if (!rel[0]) eu->tipo = TIPO_PASTA;
    if (filhos) { *filhos = fs; *nf = k; } else libera(fs);
    return 0;
}

/* ---------------------------------------------------------------- cachés del servicio */

typedef struct { bool usado; char rel[CAMINHO_MAX]; item_t it; int erro; unsigned quando; } cache_t;
static cache_t cache[MAX_CACHE];
static unsigned cache_prox;

static struct { char rel[CAMINHO_MAX]; item_t *itens; int n; unsigned quando; bool valida; } lista;

static void cache_esquecer(void)
{
    for (int i = 0; i < MAX_CACHE; i++) cache[i].usado = false;
    lista.valida = false;
}

static int info_de(const char *rel, item_t *it)
{
    unsigned agora = tempo_ms();
    for (int i = 0; i < MAX_CACHE; i++)
        if (cache[i].usado && agora - cache[i].quando < CACHE_MS && compara_texto(cache[i].rel, rel) == 0) {
            *it = cache[i].it;
            return cache[i].erro;
        }
    int e = propfind(rel, 0, it, NULL, NULL);
    if (e < 0 && erro_de_rede(e)) return e;
    cache_t *c = &cache[cache_prox++ % MAX_CACHE];
    c->usado = true;
    copia_texto(c->rel, rel, sizeof c->rel);
    c->it = *it;
    c->erro = e;
    c->quando = agora;
    return e;
}

/* ---------------------------------------------------------------- ficheros abiertos */

typedef struct {
    bool     usado;
    uint32_t id;
    char     rel[CAMINHO_MAX];
    char     tmp[64];
    int      fd;                  /* el temporal (-1 en lectura por Range) */
    uint32_t tamanho;
    bool     sujo;                /* hay que subirlo al cerrar */
    bool     rango;               /* lectura a trozos del servidor */
} aberto_t;
static aberto_t abertos[MAX_ABERTOS];
static uint32_t abertos_geracao = 1;
static char *bloco_rango;         /* el último trozo leído por Range */
static int bloco_dono = -1;
static uint32_t bloco_ini, bloco_n;

typedef struct { bool usado; char rel[CAMINHO_MAX]; char tmp[64]; uint32_t tamanho; } pendente_t;
static pendente_t pendentes[MAX_PENDENTES];
static unsigned tmp_seq;

static aberto_t *aberto_de(uint32_t id)
{
    uint32_t i = id & 0xFF;
    if (i >= MAX_ABERTOS || !abertos[i].usado || abertos[i].id != id) return NULL;
    return &abertos[i];
}

static void novo_tmp(char *s, int max) { formatar_texto(s, (size_t)max, "/tmp/.dav-%d-%u", meu_pid(), ++tmp_seq); }

static void avisar(const char *fmt, const char *a)
{
    char t[200];
    formatar_texto(t, sizeof t, fmt, ur.nome, a ? a : "");
    aviso_tela(t, NULL);
    registrar("dav: %s", t);
}

/* PUT del temporal. 0, un error de red (se puede reintentar) u otro (no se reintenta). */
static int subir(const char *rel, int fd, uint32_t tam)
{
    resp_t r;
    int e = trocar("PUT", rel, false, "Content-Type: application/octet-stream\r\n", NULL, 0, fd, tam, -1, NULL, NULL, &r);
    if (e < 0) return e;
    cache_esquecer();
    return erro_http(r.codigo);
}

static void guardar_pendente(const char *rel, const char *tmp, uint32_t tam)
{
    for (int i = 0; i < MAX_PENDENTES; i++)
        if (!pendentes[i].usado) {
            pendentes[i].usado = true;
            copia_texto(pendentes[i].rel, rel, sizeof pendentes[i].rel);
            copia_texto(pendentes[i].tmp, tmp, sizeof pendentes[i].tmp);
            pendentes[i].tamanho = tam;
            return;
        }
    avisar("/red/%s: no queda sitio para guardar %s sin subir", rel);
}

static void reintentar_pendentes(void)
{
    for (int i = 0; i < MAX_PENDENTES; i++) {
        pendente_t *p = &pendentes[i];
        if (!p->usado) continue;
        int fd = abrir(p->tmp, ABRIR_LER);
        if (fd < 0) { p->usado = false; continue; }
        int e = subir(p->rel, fd, p->tamanho);
        fechar(fd);
        if (e < 0 && erro_de_rede(e)) { marcar_estado(PONTE_EST_DESCONECTADA); return; }
        if (e == 0) avisar("/red/%s: %s ya está subido", p->rel);
        else avisar("/red/%s: el servidor no acepta %s (sigue en /tmp)", p->rel);
        if (e == 0) apagar(p->tmp);
        p->usado = false;
    }
}

/* Al salir con algo sin subir: a ~/.cache/dav-pendientes/<unidad>/, para no perderlo. */
static void salvar_pendentes(void)
{
    char d[CAMINHO_MAX];
    bool algum = false;
    for (int i = 0; i < MAX_PENDENTES; i++) {
        pendente_t *p = &pendentes[i];
        if (!p->usado) continue;
        if (!algum) {
            criar_pasta("/home/.cache");
            criar_pasta("/home/.cache/dav-pendientes");
            formatar_texto(d, sizeof d, "/home/.cache/dav-pendientes/%s", ur.nome);
            criar_pasta(d);
            algum = true;
        }
        char dest[CAMINHO_MAX + 8], plano[CAMINHO_MAX];
        copia_texto(plano, p->rel, sizeof plano);
        for (char *c = plano; *c; c++) if (*c == '/') *c = '_';
        formatar_texto(dest, sizeof dest, "%s/%s", d, plano);
        int fo = abrir(p->tmp, ABRIR_LER), fd = abrir(dest, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (fo >= 0 && fd >= 0) {
            static char b[8192];
            int k;
            while ((k = ler(fo, b, sizeof b)) > 0) escrever_tudo(fd, b, (unsigned)k);
        }
        if (fo >= 0) fechar(fo);
        if (fd >= 0) { fechar(fd); mudar_permisos(dest, 0600); }
        apagar(p->tmp);
        avisar("/red/%s: %s no se pudo subir; está en ~/.cache/dav-pendientes", p->rel);
        p->usado = false;
    }
}

/* ---------------------------------------------------------------- estado de la conexión */

static unsigned ultima_prova;

static void marcar_estado(int e)
{
    if (e == estado_rede) return;
    int antes = estado_rede;
    estado_rede = e;
    if (canal >= 0) ponte_op(PONTE_ESTADO, (unsigned)canal, (unsigned)e, 0, 0);
    registrar("dav %s: %s", ur.nome, unidade_red_estado_texto(e));
    if (e == PONTE_EST_DESCONECTADA) { cache_esquecer(); avisar("Unidad de red %s: sin conexión%s", NULL); }
    else if (e == PONTE_EST_CONECTADA && antes == PONTE_EST_DESCONECTADA) avisar("Unidad de red %s: conexión recuperada%s", NULL);
}

/* PROPFIND de la raíz. 0 = conectada; si no, el error. */
static int provar(void)
{
    item_t it;
    ultima_prova = tempo_ms();
    int e = propfind("", 0, &it, NULL, NULL);
    if (e == 0) { marcar_estado(PONTE_EST_CONECTADA); reintentar_pendentes(); }
    else if (erro_de_rede(e)) marcar_estado(PONTE_EST_DESCONECTADA);
    return e;
}

/* Tras un error de red en una operación: desconectada (el kernel contesta solo hasta que vuelva). */
static int rede(int e)
{
    if (erro_de_rede(e)) { marcar_estado(PONTE_EST_DESCONECTADA); return ERRO_SEM_REDE; }
    return e;
}

/* ---------------------------------------------------------------- las operaciones */

static uint8_t pedido_buf[sizeof(ponte_pedido_t) + PONTE_DADOS];
static uint8_t resposta_buf[sizeof(ponte_resposta_t) + PONTE_DADOS];

static void responder(ponte_resposta_t *r, const void *dados, unsigned n)
{
    memcpy(resposta_buf, r, sizeof *r);
    if (n) memcpy(resposta_buf + sizeof *r, dados, n);
    ponte_op(PONTE_RESPONDER, (unsigned)canal, (unsigned)resposta_buf, (unsigned)(sizeof *r + n), 0);
}

static void fechar_aberto(aberto_t *a, bool subir_ja)
{
    if (a->sujo && subir_ja) {
        int e = subir(a->rel, a->fd, a->tamanho);
        if (e < 0 && erro_de_rede(e)) {
            marcar_estado(PONTE_EST_DESCONECTADA);
            fechar(a->fd);
            guardar_pendente(a->rel, a->tmp, a->tamanho);
            avisar("/red/%s: sin conexión; %s se subirá al volver", a->rel);
            a->usado = false;
            return;
        }
        if (e < 0) {
            fechar(a->fd);
            guardar_pendente(a->rel, a->tmp, a->tamanho);   /* el servidor no lo quiere: no se pierde (salvar_pendentes) */
            avisar("/red/%s: el servidor no acepta %s", a->rel);
            a->usado = false;
            return;
        }
    }
    if (a->fd >= 0) fechar(a->fd);
    if (a->tmp[0]) apagar(a->tmp);
    if (bloco_dono == (int)(a - abertos)) bloco_dono = -1;
    a->usado = false;
}

static int op_abrir(const ponte_pedido_t *p, ponte_resposta_t *r)
{
    int livre = -1;
    for (int i = 0; i < MAX_ABERTOS && livre < 0; i++) if (!abertos[i].usado) livre = i;
    if (livre < 0) return ERRO_LIMITE;
    if (!p->caminho[0]) return ERRO_E_PASTA;
    item_t it;
    int e = info_de(p->caminho, &it);
    if (e < 0 && e != ERRO_NAO_EXISTE) return rede(e);
    bool existe = e == 0;
    if (existe && it.tipo == TIPO_PASTA) return ERRO_E_PASTA;
    if (!existe && !(p->modo & ABRIR_CRIAR)) return ERRO_NAO_EXISTE;
    aberto_t *a = &abertos[livre];
    memset(a, 0, sizeof *a);
    a->fd = -1;
    copia_texto(a->rel, p->caminho, sizeof a->rel);
    bool escrita = (p->modo & ABRIR_ESCREVER) != 0;
    if (existe && !escrita && it.tamanho >= GRANDE) {   /* grande y solo para leer: a trozos */
        a->rango = true;
        a->tamanho = it.tamanho;
    } else {
        novo_tmp(a->tmp, sizeof a->tmp);
        apagar(a->tmp);
        a->fd = abrir(a->tmp, ABRIR_LER | ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (a->fd < 0) { a->tmp[0] = 0; return a->fd; }
        mudar_permisos(a->tmp, 0600);
        if (existe && !(p->modo & ABRIR_TRUNCAR)) {
            resp_t rr;
            e = trocar("GET", a->rel, false, NULL, NULL, 0, -1, 0, a->fd, NULL, NULL, &rr);
            if (e == 0 && rr.codigo / 100 != 2) e = erro_http(rr.codigo);
            if (e < 0) { fechar(a->fd); apagar(a->tmp); return rede(e); }
            info_arquivo_t inf;
            a->tamanho = info_arquivo(a->tmp, &inf) == 0 ? inf.tamanho : 0;
        }
        if (!existe || (p->modo & ABRIR_TRUNCAR)) a->sujo = escrita;   /* nuevo o vaciado: se sube aunque no se escriba */
        if (!existe) cache_esquecer();
    }
    a->usado = true;
    a->id = (abertos_geracao++ << 8) | (uint32_t)livre;
    r->arq = a->id;
    r->tamanho = a->tamanho;
    return 0;
}

static int op_ler(const ponte_pedido_t *p, ponte_resposta_t *r, uint8_t *dados)
{
    aberto_t *a = aberto_de(p->arq);
    if (!a) return ERRO_DESCRITOR;
    uint32_t n = p->n > PONTE_DADOS ? PONTE_DADOS : p->n;
    if (p->pos >= a->tamanho) { r->n = 0; return 0; }
    if (n > a->tamanho - p->pos) n = a->tamanho - p->pos;
    if (!a->rango) {
        posicionar(a->fd, (int)p->pos, POS_INICIO);
        uint32_t t = 0;
        while (t < n) {
            int k = ler(a->fd, dados + t, n - t);
            if (k <= 0) break;
            t += (uint32_t)k;
        }
        r->n = t;
        return 0;
    }
    int idx = (int)(a - abertos);
    if (bloco_dono != idx || p->pos < bloco_ini || p->pos + n > bloco_ini + bloco_n) {
        if (!bloco_rango && !(bloco_rango = aloca(BLOCO_RANGO))) return ERRO_SEM_MEMORIA;
        uint32_t ini = p->pos, fim = ini + BLOCO_RANGO > a->tamanho ? a->tamanho : ini + BLOCO_RANGO;
        char extra[64];
        formatar_texto(extra, sizeof extra, "Range: bytes=%u-%u\r\n", ini, fim - 1);
        resp_t rr;
        char *corpo = NULL;
        unsigned nc = 0;
        int e = trocar("GET", a->rel, false, extra, NULL, 0, -1, 0, -1, &corpo, &nc, &rr);
        if (e < 0) return rede(e);
        if (rr.codigo != 206 && rr.codigo != 200) { libera(corpo); return erro_http(rr.codigo); }
        const char *orig = corpo;
        if (rr.codigo == 200) {   /* el servidor no sabe de Range: vino entero (hasta CORPO_MAX) */
            if (nc <= ini) { libera(corpo); r->n = 0; return 0; }
            orig += ini;
            nc -= ini;
        }
        if (nc > fim - ini) nc = fim - ini;
        memcpy(bloco_rango, orig, nc);
        libera(corpo);
        bloco_dono = idx;
        bloco_ini = ini;
        bloco_n = nc;
        if (p->pos + n > bloco_ini + bloco_n) n = bloco_ini + bloco_n > p->pos ? bloco_ini + bloco_n - p->pos : 0;
    }
    memcpy(dados, bloco_rango + (p->pos - bloco_ini), n);
    r->n = n;
    return 0;
}

static int op_escrever(const ponte_pedido_t *p, ponte_resposta_t *r, const uint8_t *dados)
{
    aberto_t *a = aberto_de(p->arq);
    if (!a || a->rango) return ERRO_DESCRITOR;
    if (posicionar(a->fd, (int)p->pos, POS_INICIO) < 0) return ERRO_INVALIDO;
    int e = escrever_tudo(a->fd, dados, p->n);
    if (e < 0) return e;
    if (p->pos + p->n > a->tamanho) a->tamanho = p->pos + p->n;
    a->sujo = true;
    r->tamanho = a->tamanho;
    return 0;
}

static int op_truncar(const ponte_pedido_t *p)
{
    aberto_t *a = aberto_de(p->arq);
    if (!a || a->rango) return ERRO_DESCRITOR;
    int e = truncar(a->fd, p->pos);
    if (e < 0) return e;
    a->tamanho = p->pos;
    a->sujo = true;
    return 0;
}

/* Lo abierto para escribir manda sobre el servidor (aún no se subió). */
static bool info_local(const char *rel, item_t *it)
{
    for (int i = 0; i < MAX_ABERTOS; i++)
        if (abertos[i].usado && abertos[i].sujo && compara_texto(abertos[i].rel, rel) == 0) {
            memset(it, 0, sizeof *it);
            it->tipo = TIPO_ARQUIVO;
            it->tamanho = abertos[i].tamanho;
            return true;
        }
    return false;
}

static int op_info(const ponte_pedido_t *p, ponte_resposta_t *r)
{
    item_t it;
    if (!p->caminho[0]) { r->tipo = TIPO_PASTA; return 0; }
    int e = info_local(p->caminho, &it) ? 0 : info_de(p->caminho, &it);
    if (e < 0) return rede(e);
    r->tipo = it.tipo;
    r->tamanho = it.tamanho;
    r->modificado = it.modificado;
    return 0;
}

static int op_listar(const ponte_pedido_t *p, ponte_resposta_t *r, uint8_t *dados)
{
    unsigned agora = tempo_ms();
    if (p->pos == 0 || !lista.valida || compara_texto(lista.rel, p->caminho) != 0 || agora - lista.quando > 10000) {
        item_t eu;
        item_t *fs = NULL;
        int n = 0;
        int e = propfind(p->caminho, 1, &eu, &fs, &n);
        if (e < 0) return rede(e);
        if (p->caminho[0] && eu.tipo != TIPO_PASTA) { libera(fs); return ERRO_NAO_E_PASTA; }
        libera(lista.itens);
        lista.itens = fs;
        lista.n = n;
        lista.quando = agora;
        lista.valida = true;
        copia_texto(lista.rel, p->caminho, sizeof lista.rel);
        /* los metadatos de los hijos ya están: a la caché */
        for (int i = 0; i < n; i++) {
            cache_t *c = &cache[cache_prox++ % MAX_CACHE];
            c->usado = true;
            formatar_texto(c->rel, sizeof c->rel, "%s%s%s", p->caminho, p->caminho[0] ? "/" : "", fs[i].nome);
            c->it = fs[i];
            c->erro = 0;
            c->quando = agora;
        }
    }
    uint32_t max = p->n, k = 0;
    if (max * sizeof(entrada_pasta_t) > PONTE_DADOS) max = PONTE_DADOS / sizeof(entrada_pasta_t);
    entrada_pasta_t *v = (entrada_pasta_t *)dados;
    for (uint32_t i = p->pos; i < (uint32_t)lista.n && k < max; i++, k++) {
        memset(&v[k], 0, sizeof v[k]);
        copia_texto(v[k].nome, lista.itens[i].nome, sizeof v[k].nome);
        v[k].tipo = lista.itens[i].tipo;
        v[k].tamanho = lista.itens[i].tamanho;
    }
    r->n = k;
    return 0;
}

static int op_simples(const char *metodo, const char *rel, bool barra, const char *extra)
{
    resp_t r;
    int e = trocar(metodo, rel, barra, extra, NULL, 0, -1, 0, -1, NULL, NULL, &r);
    cache_esquecer();
    if (e < 0) return rede(e);
    return erro_http(r.codigo);
}

static int op_remover_pasta(const ponte_pedido_t *p)
{
    if (!p->caminho[0]) return ERRO_OCUPADO;
    item_t eu, *fs = NULL;
    int n = 0;
    int e = propfind(p->caminho, 1, &eu, &fs, &n);   /* DELETE de una carpeta lo borra todo: solo si está vacía */
    libera(fs);
    if (e < 0) return rede(e);
    if (eu.tipo != TIPO_PASTA) return ERRO_NAO_E_PASTA;
    if (n) return ERRO_NAO_VAZIA;
    return op_simples("DELETE", p->caminho, true, NULL);
}

static int op_apagar(const ponte_pedido_t *p)
{
    item_t it;
    int e = info_de(p->caminho, &it);
    if (e < 0) return rede(e);
    if (it.tipo == TIPO_PASTA) return ERRO_E_PASTA;
    return op_simples("DELETE", p->caminho, false, NULL);
}

static int op_renomear(const ponte_pedido_t *p)
{
    if (!rel_valida(p->destino) || !p->destino[0]) return ERRO_INVALIDO;
    item_t it;
    int e = info_de(p->caminho, &it);
    if (e < 0) return rede(e);
    char cod[3 * CAMINHO_MAX + 8], extra[3 * CAMINHO_MAX + 400];
    if (!codificar(p->destino, cod, sizeof cod)) return ERRO_INVALIDO;
    char porta[8] = "";
    if (base.porta != (base.https ? 443 : 80)) formatar_texto(porta, sizeof porta, ":%d", base.porta);
    formatar_texto(extra, sizeof extra, "Destination: %s://%s%s%s%s%s\r\nOverwrite: T\r\n", base.https ? "https" : "http",
                   base.host, porta, base.caminho, cod, it.tipo == TIPO_PASTA ? "/" : "");
    return op_simples("MOVE", p->caminho, it.tipo == TIPO_PASTA, extra);
}

static void atender(const ponte_pedido_t *p, const uint8_t *dados_in)
{
    static uint8_t dados[PONTE_DADOS];
    ponte_resposta_t r;
    memset(&r, 0, sizeof r);
    r.id = p->id;
    unsigned n = 0;
    int e;
    if (!rel_valida(p->caminho)) e = ERRO_INVALIDO;
    else switch (p->op) {
    case PO_INFO:          e = op_info(p, &r); break;
    case PO_LISTAR:        e = op_listar(p, &r, dados); n = r.n * sizeof(entrada_pasta_t); break;
    case PO_ABRIR:         e = op_abrir(p, &r); break;
    case PO_LER:           e = op_ler(p, &r, dados); n = r.n; break;
    case PO_ESCREVER:      e = op_escrever(p, &r, dados_in); break;
    case PO_TRUNCAR:       e = op_truncar(p); break;
    case PO_SINCRONIZAR: {
        aberto_t *a = aberto_de(p->arq);
        e = !a ? ERRO_DESCRITOR : !a->sujo ? 0 : rede(subir(a->rel, a->fd, a->tamanho));
        if (a && e == 0) a->sujo = false;
        break;
    }
    case PO_FECHAR: {
        aberto_t *a = aberto_de(p->arq);
        if (a) fechar_aberto(a, true);
        return;   /* sin respuesta */
    }
    case PO_CRIAR_PASTA:   e = p->caminho[0] ? op_simples("MKCOL", p->caminho, true, NULL) : ERRO_JA_EXISTE; break;
    case PO_REMOVER_PASTA: e = op_remover_pasta(p); break;
    case PO_APAGAR:        e = p->caminho[0] ? op_apagar(p) : ERRO_E_PASTA; break;
    case PO_RENOMEAR:      e = op_renomear(p); break;
    default:               e = ERRO_NAO_SUPORTADO;
    }
    r.resultado = e < 0 ? e : 0;
    if (e < 0) n = 0;
    if (p->id) responder(&r, dados, n);
}

/* ---------------------------------------------------------------- la contraseña y el llavero */

static void servico_de(const char *nome, char *s, int max) { formatar_texto(s, (size_t)max, "webdav:%s", nome); }

/* La contraseña de la unidad desde el llavero (en *senha, que el que llama borra). false = llavero cerrado. */
static bool senha_de(const char *nome, char *senha, int max)
{
    senha[0] = 0;
    chaveiro_t *c = aloca(sizeof *c);
    if (!c) return false;
    int r = chaveiro_abrir(c);
    if (r == CHAV_ABERTO) {
        char s[96];
        servico_de(nome, s, sizeof s);
        const char *v = chaveiro_ler(c, s);
        if (v) copia_texto(senha, v, (size_t)max);
    }
    chaveiro_fechar(c);
    libera(c);
    return r == CHAV_ABERTO;
}

/* ---------------------------------------------------------------- dav servir */

static int servir(const char *nome)
{
    unidade_red_t v[UNIDADES_RED_MAX];
    int n = unidades_red_ler(v, UNIDADES_RED_MAX);
    int i = unidade_red_achar(v, n, nome);
    if (i < 0) { registrar("dav servir %s: no está en %s", nome, UNIDADES_RED_ARQ); return 1; }
    char senha[CHAVEIRO_VALOR_MAX], erro[200];
    if (!senha_de(nome, senha, sizeof senha)) { registrar("dav servir %s: el llavero está cerrado", nome); return 1; }
    bool ok = preparar(&v[i], senha, erro, sizeof erro);
    cripto_apagar(senha, sizeof senha);
    if (!ok) { registrar("dav servir %s: %s", nome, erro); return 1; }
    definir_grupo(meu_pid(), meu_pid());   /* que no lo cierre la terminal desde la que se montó */
    ponte_montar_t pm;
    memset(&pm, 0, sizeof pm);
    copia_texto(pm.nome, nome, sizeof pm.nome);
    unidade_red_rotulo(v[i].url, pm.rotulo, sizeof pm.rotulo);
    canal = ponte_op(PONTE_MONTAR, (unsigned)&pm, 0, 0, 0);
    if (canal < 0) { registrar("dav servir %s: no se puede montar: %s", nome, nome_erro(canal)); return 1; }
    estado_rede = PONTE_EST_CONECTANDO;
    provar();
    if (estado_rede == PONTE_EST_CONECTANDO) marcar_estado(PONTE_EST_CONECTADA);   /* respondió (aunque fuera con un error HTTP) */
    registrar("dav servir %s: montada en /red/%s (pid %d)", nome, nome, meu_pid());
    for (;;) {
        int k = ponte_op(PONTE_RECEBER, (unsigned)canal, (unsigned)pedido_buf, sizeof pedido_buf, 1000);
        if (k == ERRO_NAO_EXISTE || k == ERRO_ACESSO) break;   /* desmontada */
        if (k > 0) {
            const ponte_pedido_t *p = (const ponte_pedido_t *)pedido_buf;
            atender(p, pedido_buf + sizeof *p);
            continue;
        }
        unsigned agora = tempo_ms();
        unsigned cada = estado_rede == PONTE_EST_DESCONECTADA ? PROBAR_DESCONECTADA_MS : PROBAR_CONECTADA_MS;
        if (agora - ultima_prova >= cada) provar();
    }
    for (int j = 0; j < MAX_ABERTOS; j++) if (abertos[j].usado) fechar_aberto(&abertos[j], estado_rede != PONTE_EST_DESCONECTADA);
    salvar_pendentes();
    cripto_apagar(autorizacao, sizeof autorizacao);
    registrar("dav servir %s: termina", nome);
    return 0;
}

/* ================================================================ las órdenes */

static void dizer(const char *fmt, const char *a, const char *b)
{
    if (!silencio) imprimir(fmt, a ? a : "", b ? b : "");
}

static int montar_unidade(const char *nome)
{
    unidade_red_t v[UNIDADES_RED_MAX];
    int n = unidades_red_ler(v, UNIDADES_RED_MAX);
    int i = unidade_red_achar(v, n, nome);
    if (i < 0) { imprimir_em(ERROS, "dav: no hay ninguna unidad «%s» (dav agregar)\n", nome); return 1; }
    if (unidade_red_estado(nome, NULL) >= 0) { dizer("/red/%s ya está montada\n", nome, NULL); return 0; }
    char senha[CHAVEIRO_VALOR_MAX], erro[200];
    if (!senha_de(nome, senha, sizeof senha)) {
        imprimir_em(ERROS, "dav: tu llavero está cerrado: ábrelo con «dav llavero abrir»\n");
        return 1;
    }
    bool ok = preparar(&v[i], senha, erro, sizeof erro);
    cripto_apagar(senha, sizeof senha);
    if (!ok) { imprimir_em(ERROS, "dav: %s\n", erro); return 1; }
    item_t it;
    int e = propfind("", 0, &it, NULL, NULL);
    if (e == ERRO_ACESSO) { imprimir_em(ERROS, "dav: %s: usuario o contraseña incorrectos, o certificado no válido\n", nome); return 1; }
    if (e < 0 && !erro_de_rede(e)) { imprimir_em(ERROS, "dav: %s: el servidor no responde como WebDAV (%s)\n", nome, nome_erro(e)); return 1; }
    char *argv[] = { "dav", "servir", (char *)nome, NULL };
    int pid = executar("/bin/dav", argv);
    if (pid < 0) { imprimir_em(ERROS, "dav: no se puede lanzar el servicio: %s\n", nome_erro(pid)); return 1; }
    for (int k = 0; k < 100; k++) {
        ponte_info_t inf;
        int est = unidade_red_estado(nome, &inf);
        if (est >= 0 && est != PONTE_EST_CONECTANDO) {
            dizer("/red/%s montada: %s\n", nome, unidade_red_estado_texto(est));
            return 0;
        }
        dormir(100);
    }
    imprimir_em(ERROS, "dav: %s: el servicio no ha montado la unidad\n", nome);
    return 1;
}

static int desmontar_unidade(const char *nome)
{
    ponte_info_t inf;
    if (unidade_red_estado(nome, &inf) < 0) { imprimir_em(ERROS, "dav: /red/%s no está montada\n", nome); return 1; }
    int r = ponte_op(PONTE_DESMONTAR, inf.canal, 0, 0, 0);
    if (r < 0) { imprimir_em(ERROS, "dav: %s: %s\n", nome, nome_erro(r)); return 1; }
    imprimir("/red/%s desmontada\n", nome);
    return 0;
}

static int estado(void)
{
    ponte_info_t v[PONTE_MAX];
    int n = ponte_op(PONTE_LISTAR, (unsigned)v, PONTE_MAX, 0, 0);
    if (n <= 0) { imprimir("No hay unidades de red montadas.\n"); return 0; }
    for (int i = 0; i < n; i++)
        imprimir("/red/%-12s %-13s %s  (pid %d, %u pedidos, %u errores, %u sin respuesta)\n", v[i].nome,
                 unidade_red_estado_texto((int)v[i].estado), v[i].rotulo, v[i].pid, v[i].pedidos, v[i].erros, v[i].tempo_esgotado);
    return 0;
}

static int lista_cmd(void)
{
    unidade_red_t v[UNIDADES_RED_MAX];
    int n = unidades_red_ler(v, UNIDADES_RED_MAX);
    if (!n) { imprimir("No tienes unidades de red (dav agregar <nombre> <url> [usuario]).\n"); return 0; }
    for (int i = 0; i < n; i++) {
        char rot[PONTE_ROTULO_MAX];
        unidade_red_rotulo(v[i].url, rot, sizeof rot);
        imprimir("%-12s %-13s %s%s%s%s\n", v[i].nome, unidade_red_estado_texto(unidade_red_estado(v[i].nome, NULL)), rot,
                 v[i].usuario[0] ? " como " : "", v[i].usuario, v[i].automatica ? " (al iniciar)" : "");
    }
    return 0;
}

static int abrir_llavero(chaveiro_t *c)
{
    int r = chaveiro_abrir(c);
    if (r == CHAV_ABERTO) return 0;
    if (r == CHAV_DANADO) { imprimir_em(ERROS, "dav: el llavero (%s) está dañado\n", CHAVEIRO_ARQ); return -1; }
    char s[PED_SENHA_MAX];
    if (ler_senha("Contraseña del llavero (la de tu cuenta, o la anterior si la cambiaste): ", s, sizeof s) < 0) s[0] = 0;
    r = chaveiro_abrir_com(c, s);
    cripto_apagar(s, sizeof s);
    if (r != CHAV_ABERTO) { imprimir_em(ERROS, "dav: esa contraseña no abre el llavero\n"); return -1; }
    if (c->modo != CHAV_MODO_PROPIA && chaveiro_tem_chave_sessao()) chaveiro_guardar(c);
    return 0;
}

static void aviso_modo(const chaveiro_t *c)
{
    if (c->modo == CHAV_MODO_VAZIA && !silencio)
        imprimir("Aviso: tu cuenta no tiene contraseña y el llavero tampoco: está cifrado, pero quien copie el fichero lo abre.\n"
                 "       Ponle una con «dav llavero contrasena» (o en Configuración > Unidades de red).\n");
}

static int agregar(int argc, char **argv)
{
    if (argc < 4) { imprimir_em(ERROS, "%s", AJUDA); return 2; }
    unidade_red_t u;
    memset(&u, 0, sizeof u);
    bool sem_senha = false;
    int pos = 0;
    for (int i = 2; i < argc; i++) {
        if (compara_texto(argv[i], "--auto") == 0) u.automatica = true;
        else if (compara_texto(argv[i], "--inseguro") == 0) u.inseguro = true;
        else if (compara_texto(argv[i], "--sin-clave") == 0) sem_senha = true;
        else if (compara_texto(argv[i], "--cacert") == 0 && i + 1 < argc) copia_texto(u.cacert, argv[++i], sizeof u.cacert);
        else if (pos == 0) { copia_texto(u.nome, argv[i], sizeof u.nome); pos++; }
        else if (pos == 1) { copia_texto(u.url, argv[i], sizeof u.url); pos++; }
        else if (pos == 2) { copia_texto(u.usuario, argv[i], sizeof u.usuario); pos++; }
    }
    if (!unidade_red_nome_valido(u.nome)) { imprimir_em(ERROS, "dav: nombre no válido (letras, números, - y _, hasta 24)\n"); return 2; }
    url_t t;
    if (!url_separar(u.url, &t) || (!comeca_com(u.url, "http://") && !comeca_com(u.url, "https://"))) {
        imprimir_em(ERROS, "dav: la dirección tiene que empezar por http:// o https://\n");
        return 2;
    }
    chaveiro_t *c = aloca(sizeof *c);
    if (!c) return 1;
    if (abrir_llavero(c) < 0) { libera(c); return 1; }
    if (u.usuario[0] && !sem_senha) {
        char s[CHAVEIRO_VALOR_MAX], sv[96];
        char perg[120];
        formatar_texto(perg, sizeof perg, "Contraseña de %s en %s: ", u.usuario, u.nome);
        if (ler_senha(perg, s, sizeof s) < 0) s[0] = 0;
        servico_de(u.nome, sv, sizeof sv);
        int r = chaveiro_por(c, sv, s);
        cripto_apagar(s, sizeof s);
        if (r < 0) { imprimir_em(ERROS, "dav: no se puede guardar en el llavero: %s\n", nome_erro(r)); chaveiro_fechar(c); libera(c); return 1; }
    }
    aviso_modo(c);
    chaveiro_fechar(c);
    libera(c);
    unidade_red_t v[UNIDADES_RED_MAX];
    int n = unidades_red_ler(v, UNIDADES_RED_MAX);
    int i = unidade_red_achar(v, n, u.nome);
    if (i < 0) {
        if (n >= UNIDADES_RED_MAX) { imprimir_em(ERROS, "dav: ya tienes %d unidades\n", UNIDADES_RED_MAX); return 1; }
        i = n++;
    }
    v[i] = u;
    int r = unidades_red_gravar(v, n);
    if (r < 0) { imprimir_em(ERROS, "dav: no se puede guardar %s: %s\n", UNIDADES_RED_ARQ, nome_erro(r)); return 1; }
    imprimir("Unidad %s guardada; móntala con «dav montar %s».\n", u.nome, u.nome);
    return 0;
}

static int borrar(const char *nome)
{
    if (unidade_red_estado(nome, NULL) >= 0) desmontar_unidade(nome);
    unidade_red_t v[UNIDADES_RED_MAX];
    int n = unidades_red_ler(v, UNIDADES_RED_MAX);
    int i = unidade_red_achar(v, n, nome);
    if (i < 0) { imprimir_em(ERROS, "dav: no hay ninguna unidad «%s»\n", nome); return 1; }
    for (int k = i + 1; k < n; k++) v[k - 1] = v[k];
    unidades_red_gravar(v, n - 1);
    chaveiro_t *c = aloca(sizeof *c);
    if (c && chaveiro_abrir(c) == CHAV_ABERTO) {
        char sv[96];
        servico_de(nome, sv, sizeof sv);
        chaveiro_tirar(c, sv);
    }
    if (c) { chaveiro_fechar(c); libera(c); }
    imprimir("Unidad %s borrada (y su contraseña del llavero).\n", nome);
    return 0;
}

static int probar(const char *nome)
{
    unidade_red_t v[UNIDADES_RED_MAX];
    int n = unidades_red_ler(v, UNIDADES_RED_MAX);
    int i = unidade_red_achar(v, n, nome);
    if (i < 0) { imprimir_em(ERROS, "dav: no hay ninguna unidad «%s»\n", nome); return 1; }
    char senha[CHAVEIRO_VALOR_MAX], erro[200];
    if (!senha_de(nome, senha, sizeof senha)) { imprimir_em(ERROS, "dav: tu llavero está cerrado\n"); return 1; }
    bool ok = preparar(&v[i], senha, erro, sizeof erro);
    cripto_apagar(senha, sizeof senha);
    if (!ok) { imprimir_em(ERROS, "dav: %s\n", erro); return 1; }
    item_t it, *fs = NULL;
    int k = 0;
    int e = propfind("", 1, &it, &fs, &k);
    libera(fs);
    if (e == 0) { imprimir("%s: conexión correcta (%d elementos en la raíz)\n", nome, k); return 0; }
    imprimir_em(ERROS, "%s: %s\n", nome, e == ERRO_ACESSO ? "usuario o contraseña incorrectos, o certificado no válido" :
                                     erro_de_rede(e) ? "sin conexión con el servidor" : nome_erro(e));
    return 1;
}

static int iniciar(void)
{
    silencio = true;
    unidade_red_t v[UNIDADES_RED_MAX];
    int n = unidades_red_ler(v, UNIDADES_RED_MAX);
    for (int i = 0; i < n; i++) if (v[i].automatica) montar_unidade(v[i].nome);
    return 0;
}

static int llavero(int argc, char **argv)
{
    const char *sub = argc > 2 ? argv[2] : "estado";
    chaveiro_t *c = aloca(sizeof *c);
    if (!c) return 1;
    int r = 0;
    if (compara_texto(sub, "estado") == 0) {
        int a = chaveiro_abrir(c);
        if (a == CHAV_ABERTO) {
            imprimir("Llavero %s: %s; %d %s.\n", c->existia ? "abierto" : "nuevo (aún vacío)", chaveiro_modo_texto(c->modo), c->n,
                     c->n == 1 ? "secreto" : "secretos");
            aviso_modo(c);
        } else imprimir("Llavero cerrado: ábrelo con «dav llavero abrir».\n");
    } else if (compara_texto(sub, "abrir") == 0) {
        r = abrir_llavero(c) < 0;
        if (!r) imprimir("Llavero abierto (%s).\n", chaveiro_modo_texto(c->modo));
    } else if (compara_texto(sub, "contrasena") == 0 || compara_texto(sub, "contraseña") == 0) {
        if (abrir_llavero(c) < 0) r = 1;
        else {
            char a[PED_SENHA_MAX], b[PED_SENHA_MAX];
            if (ler_senha("Contraseña nueva del llavero (vacía: la de la cuenta): ", a, sizeof a) < 0) a[0] = 0;
            if (a[0] && ler_senha("Repítela: ", b, sizeof b) < 0) b[0] = 0;
            if (a[0] && compara_texto(a, b) != 0) { imprimir_em(ERROS, "dav: no coinciden\n"); r = 1; }
            else if ((r = chaveiro_mudar_senha(c, a)) < 0) { imprimir_em(ERROS, "dav: %s\n", nome_erro(r)); r = 1; }
            else imprimir("Llavero %s.\n", chaveiro_modo_texto(c->modo));
            cripto_apagar(a, sizeof a);
            cripto_apagar(b, sizeof b);
        }
    } else if (compara_texto(sub, "cerrar") == 0) {   /* la clave sale de la memoria del kernel hasta que se vuelva a abrir */
        chaveiro_op(CHAVEIRO_ESQUECER, meu_uid(), NULL);
        imprimir("Llavero cerrado.\n");
    } else { imprimir_em(ERROS, "%s", AJUDA); r = 2; }
    chaveiro_fechar(c);
    libera(c);
    return r;
}

int principal(int argc, char **argv)
{
    if (argc < 2 || compara_texto(argv[1], "--ayuda") == 0 || compara_texto(argv[1], "-h") == 0) { imprimir("%s", AJUDA); return argc < 2; }
    const char *o = argv[1];
    if (compara_texto(o, "servir") == 0 && argc > 2) return servir(argv[2]);
    if (compara_texto(o, "agregar") == 0) return agregar(argc, argv);
    if (compara_texto(o, "montar") == 0 && argc > 2) return montar_unidade(argv[2]);
    if (compara_texto(o, "desmontar") == 0 && argc > 2) return desmontar_unidade(argv[2]);
    if (compara_texto(o, "borrar") == 0 && argc > 2) return borrar(argv[2]);
    if (compara_texto(o, "probar") == 0 && argc > 2) return probar(argv[2]);
    if (compara_texto(o, "estado") == 0) return estado();
    if (compara_texto(o, "lista") == 0) return lista_cmd();
    if (compara_texto(o, "iniciar") == 0) return iniciar();
    if (compara_texto(o, "llavero") == 0) return llavero(argc, argv);
    imprimir_em(ERROS, "%s", AJUDA);
    return 2;
}
