// SPDX-License-Identifier: GPL-2.0-or-later
/* rede.c - cookies, caché HTTP, armado de pedidos, descompresión y CORS del Cipó (D487).
 *
 * Código propio. La conexión (TCP/TLS) la abre el navegador; aquí solo se decide qué
 * se envía y qué se hace con lo que vuelve. Los puntos con hilos (las imágenes se bajan
 * en paralelo, D373) están protegidos por un cerrojo: la jarra de cookies y la caché
 * son datos compartidos. */
#include "rede.h"
#include "compressao.h"

uint8_t *brotli_descomprimir(const uint8_t *in, unsigned n, unsigned *saida_n, unsigned max);

static mutex_t trava = MUTEX_INICIAL;

/* ------------------------------------------------------------ utilidades */

static char mn(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

bool rede_igual_ci(const char *a, const char *b)
{
    while (*a && *b) { if (mn(*a) != mn(*b)) return false; a++; b++; }
    return !*a && !*b;
}

int rede_comeca_ci(const char *s, const char *p)
{
    while (*p) { if (mn(*s) != mn(*p)) return 0; s++; p++; }
    return 1;
}

/* Primera aparición de `agulha` en `palha` (o NULL). */
static const char *rede_acha(const char *palha, const char *agulha)
{
    int n = (int)tam_texto(agulha);
    for (; *palha; palha++) if (comeca_com(palha, agulha)) return palha;
    return n ? NULL : palha;
}

static char *dup_n(const char *s, int n)
{
    char *o = aloca((size_t)n + 1);
    if (!o) return NULL;
    memcpy(o, s, (size_t)n);
    o[n] = 0;
    return o;
}
static char *dup_s(const char *s) { return dup_n(s, (int)tam_texto(s)); }

static void minusculas(char *s) { for (; *s; s++) *s = mn(*s); }

/* Días desde 1970-01-01 de una fecha civil (algoritmo de Howard Hinnant). */
static int64_t dias_civil(int y, int m, int d)
{
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    int yoe = y - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int64_t)era * 146097 + doe - 719468;
}

static bool acha_dp(const char *s, int n)
{
    for (int i = 0; i < n; i++) if (s[i] == ':') return true;
    return false;
}

/* "Sun, 06 Nov 1994 08:49:37 GMT", "Sunday, 06-Nov-94 08:49:37 GMT", "Sun Nov  6 08:49:37 1994". */
uint32_t rede_data_http(const char *s)
{
    static const char *meses[] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec" };
    int dia = 0, mes = 0, ano = 0, h = -1, mi = 0, se = 0;
    const char *p = s;
    while (*p) {
        while (*p && (*p == ' ' || *p == ',' || *p == '-' || *p == '\t')) p++;
        if (!*p) break;
        const char *ini = p;
        while (*p && *p != ' ' && *p != ',' && *p != '-' && *p != '\t') p++;
        int n = (int)(p - ini);
        if (acha_dp(ini, n)) {
            int v[3] = { 0, 0, 0 }, k = 0;
            for (const char *q = ini; q < p; q++) {
                if (*q == ':') { if (++k > 2) break; }
                else if (*q >= '0' && *q <= '9') v[k] = v[k] * 10 + (*q - '0');
            }
            h = v[0]; mi = v[1]; se = v[2];
        } else if (ini[0] >= '0' && ini[0] <= '9') {
            int v = 0;
            for (const char *q = ini; q < p && *q >= '0' && *q <= '9'; q++) v = v * 10 + (*q - '0');
            if (!dia && v >= 1 && v <= 31 && n <= 2) dia = v;
            else if (!ano) { ano = v; if (n <= 2) ano += ano < 70 ? 2000 : 1900; }
        } else if (n >= 3) {
            char t[4] = { mn(ini[0]), mn(ini[1]), mn(ini[2]), 0 };
            for (int i = 0; i < 12; i++) if (compara_texto(t, meses[i]) == 0 && !mes) mes = i + 1;
        }
    }
    if (!dia || !mes || !ano || h < 0 || ano < 1970) return 0;
    int64_t d = dias_civil(ano, mes, dia);
    int64_t t = d * 86400 + h * 3600 + mi * 60 + se;
    if (t <= 0) return 0;
    if (t > 0xFFFFFFF0LL) return 0xFFFFFFF0u;
    return (uint32_t)t;
}

bool rede_origem(const char *url, char *o, int max)
{
    url_t u;
    if (!url || !url_separar(url, &u)) return false;
    char host[130];
    copia_texto(host, u.host, sizeof host);
    minusculas(host);
    if (u.porta != (u.https ? 443 : 80)) formatar_texto(o, (size_t)max, "%s://%s:%d", u.https ? "https" : "http", host, u.porta);
    else formatar_texto(o, (size_t)max, "%s://%s", u.https ? "https" : "http", host);
    return true;
}

bool rede_mesma_origem(const char *a, const char *b)
{
    char x[200], y[200];
    return a && b && rede_origem(a, x, sizeof x) && rede_origem(b, y, sizeof y) && compara_texto(x, y) == 0;
}

static bool e_ip(const char *h)
{
    int pontos = 0;
    for (const char *p = h; *p; p++) {
        if (*p == '.') pontos++;
        else if (*p == ':') return true;
        else if (*p < '0' || *p > '9') return false;
    }
    return pontos == 3;
}

static bool sufixo_publico2(const char *l1, const char *l2)   /* "co"+"uk", "com"+"br"... */
{
    static const char *segundos[] = { "co", "com", "org", "net", "gov", "edu", "ac", "or", "ne", "go", "gob", "nom", "mil", "int" };
    (void)l2;
    for (unsigned i = 0; i < sizeof segundos / sizeof *segundos; i++) if (compara_texto(l1, segundos[i]) == 0) return true;
    return false;
}

/* El dominio registrable aproximado (eTLD+1) de un nombre de host. */
static void sitio_de(const char *host, char *o, int max)
{
    char h[130];
    copia_texto(h, host, sizeof h);
    minusculas(h);
    if (e_ip(h)) { copia_texto(o, h, (size_t)max); return; }
    int n = (int)tam_texto(h);
    /* etiquetas desde el final */
    int pos[8], np = 0;
    for (int i = n; i >= 0 && np < 8; i--) if (i == 0 || h[i - 1] == '.') pos[np++] = i;
    /* pos[0] = inicio de la última etiqueta, ... */
    int quer = 2;
    if (np >= 3) {
        const char *ult = h + pos[0], *pen = h + pos[1];
        int lu = n - pos[0], lp = pos[0] - 1 - pos[1];
        char p2[8] = "";
        if (lp > 0 && lp < 8) { memcpy(p2, pen, (size_t)lp); p2[lp] = 0; }
        if (lu == 2 && sufixo_publico2(p2, ult)) quer = 3;
    }
    if (np < quer) quer = np;
    copia_texto(o, h + pos[quer - 1], (size_t)max);
}

bool rede_mesmo_site(const char *a, const char *b)
{
    url_t x, y;
    if (!a || !b || !url_separar(a, &x) || !url_separar(b, &y)) return false;
    char sx[130], sy[130];
    sitio_de(x.host, sx, sizeof sx);
    sitio_de(y.host, sy, sizeof sy);
    return compara_texto(sx, sy) == 0;
}

int rede_cab_cada(const char *cab, const char *nome, void (*f)(const char *valor, void *ctx), void *ctx)
{
    int t = (int)tam_texto(nome), n = 0;
    for (const char *l = cab; l && *l;) {
        int i = 0;
        while (i < t && l[i] && mn(l[i]) == mn(nome[i])) i++;
        if (i == t && l[i] == ':') {
            const char *v = l + t + 1;
            while (*v == ' ' || *v == '\t') v++;
            int k = 0;
            while (v[k] && v[k] != '\r' && v[k] != '\n') k++;
            while (k > 0 && (v[k - 1] == ' ' || v[k - 1] == '\t')) k--;
            char *c = dup_n(v, k);
            if (c) { f(c, ctx); libera(c); n++; }
        }
        l = acha_char(l, '\n');
        if (l) l++;
    }
    return n;
}

bool rede_cab_valor(const char *cab, const char *nome, char *valor, int max)
{
    int t = (int)tam_texto(nome);
    for (const char *l = cab; l && *l;) {
        int i = 0;
        while (i < t && l[i] && mn(l[i]) == mn(nome[i])) i++;
        if (i == t && l[i] == ':') {
            const char *v = l + t + 1;
            while (*v == ' ' || *v == '\t') v++;
            int k = 0;
            while (*v && *v != '\r' && *v != '\n' && k < max - 1) valor[k++] = *v++;
            while (k > 0 && (valor[k - 1] == ' ' || valor[k - 1] == '\t')) k--;
            valor[k] = 0;
            return true;
        }
        l = acha_char(l, '\n');
        if (l) l++;
    }
    return false;
}

const char *rede_pasta(void)
{
    info_arquivo_t inf;
    if (info_arquivo("/home", &inf) == 0 && inf.tipo == TIPO_PASTA) return "/home";
    return "/tmp";
}

void rede_pasta_cipo(char *o, int max)
{
    char d[96];
    formatar_texto(d, sizeof d, "%s/.config", rede_pasta());
    criar_pasta(d);
    formatar_texto(d, sizeof d, "%s/.config/cipo", rede_pasta());
    criar_pasta(d);
    copia_texto(o, d, (size_t)max);
}

/* ------------------------------------------------------------ cookies */

#define COOKIE_MAX 300
#define COOKIE_POR_DOMINIO 50
#define COOKIE_TAM_MAX 4096      /* nombre + valor */
enum { SS_NADA = 0, SS_LAX, SS_STRICT, SS_NONE };

typedef struct {
    char *nome, *valor, *dominio, *caminho;
    uint32_t expira;     /* 0 = de sesión */
    uint32_t criado;
    bool seguro, so_http, so_host;
    uint8_t samesite;
} ck_t;

static ck_t *jarra[COOKIE_MAX];
static int njarra;
static bool jarra_suja;
static bool jarra_carregada;
static uint32_t ck_serie;     /* orden de creación dentro de un mismo segundo */

static uint32_t agora(void) { return hora_utc(NULL); }

static void ck_liberar(ck_t *c)
{
    if (!c) return;
    libera(c->nome); libera(c->valor); libera(c->dominio); libera(c->caminho);
    libera(c);
}

static void ck_quitar(int i)
{
    ck_liberar(jarra[i]);
    memmove(jarra + i, jarra + i + 1, sizeof jarra[0] * (size_t)(njarra - i - 1));
    njarra--;
    jarra_suja = true;
}

static void ck_purgar(void)
{
    uint32_t t = agora();
    for (int i = njarra - 1; i >= 0; i--) if (jarra[i]->expira && jarra[i]->expira <= t) ck_quitar(i);
}

/* ¿Es `dominio` un sufijo público (com, co.uk...)? Los cookies para esos dominios se rechazan. */
static bool dominio_publico(const char *d)
{
    if (!acha_char(d, '.')) return true;   /* "com", "localhost"... solo vale como host exacto */
    char sitio[130];
    sitio_de(d, sitio, sizeof sitio);
    /* si el dominio es más corto que su sitio registrable, es un sufijo público (co.uk) */
    return (int)tam_texto(d) < (int)tam_texto(sitio);
}

static bool sufijo_de(const char *host, const char *dom)
{
    int h = (int)tam_texto(host), d = (int)tam_texto(dom);
    if (d > h) return false;
    if (compara_texto(host + h - d, dom) != 0) return false;
    return d == h || host[h - d - 1] == '.';
}

static bool caminho_casa(const char *req, const char *ck)
{
    int c = (int)tam_texto(ck);
    if (!comeca_com(req, ck)) return false;
    if ((int)tam_texto(req) == c) return true;
    return ck[c - 1] == '/' || req[c] == '/';
}

static void caminho_padrao(const char *cam, char *o, int max)
{
    char t[REDE_URL_MAX];
    copia_texto(t, cam, sizeof t);
    char *q = acha_char(t, '?');
    if (q) *q = 0;
    char *b = acha_ultimo_char(t, '/');
    if (!b || b == t) { copia_texto(o, "/", (size_t)max); return; }
    *b = 0;
    copia_texto(o, t, (size_t)max);
}

static char *aparar(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    int n = (int)tam_texto(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = 0;
    return s;
}

void cookie_guardar(const char *url, const char *sc, bool de_js)
{
    if (!url || !sc || !*sc) return;
    url_t u;
    if (!url_separar(url, &u)) return;
    int n = (int)tam_texto(sc);
    if (n > COOKIE_TAM_MAX + 1024) return;
    char *w = dup_n(sc, n);
    if (!w) return;
    char host[130];
    copia_texto(host, u.host, sizeof host);
    minusculas(host);

    /* nombre=valor ; atributos */
    char *resto = acha_char(w, ';');
    if (resto) *resto++ = 0;
    char *eq = acha_char(w, '=');
    char *nome, *valor;
    if (!eq) { libera(w); return; }   /* sin '=' no es un cookie (RFC 6265bis lo admite sin nombre; no lo guardamos) */
    *eq = 0;
    nome = aparar(w);
    valor = aparar(eq + 1);
    if (!nome[0] || (int)tam_texto(nome) + (int)tam_texto(valor) > COOKIE_TAM_MAX) { libera(w); return; }
    for (char *p = nome; *p; p++) if ((uint8_t)*p < 32 || *p == 127) { libera(w); return; }
    for (char *p = valor; *p; p++) if ((uint8_t)*p < 32 && *p != '\t') { libera(w); return; }

    char *dominio = NULL, caminho[REDE_URL_MAX] = "";
    uint32_t expira = 0;
    bool tem_maxage = false, seguro = false, so_http = false;
    uint8_t ss = SS_NADA;
    uint32_t t = agora();
    for (char *a = resto; a && *a;) {
        char *prox = acha_char(a, ';');
        if (prox) *prox++ = 0;
        char *av = acha_char(a, '=');
        char *an = a, *vv = (char *)"";
        if (av) { *av = 0; vv = aparar(av + 1); }
        an = aparar(an);
        if (rede_igual_ci(an, "domain")) {
            while (*vv == '.') vv++;
            dominio = vv;
            minusculas(dominio);
        } else if (rede_igual_ci(an, "path")) {
            if (vv[0] == '/') copia_texto(caminho, vv, sizeof caminho);
        } else if (rede_igual_ci(an, "expires")) {
            if (!tem_maxage) {
                uint32_t e = rede_data_http(vv);
                expira = e ? e : 1;   /* fecha inválida o pasada: ya caducó */
            }
        } else if (rede_igual_ci(an, "max-age")) {
            bool neg = *vv == '-';
            if (neg) vv++;
            int v = 0, dig = 0;
            while (*vv >= '0' && *vv <= '9' && dig < 9) { v = v * 10 + (*vv++ - '0'); dig++; }
            if (dig) { tem_maxage = true; expira = (neg || v == 0) ? 1 : t + (uint32_t)v; }
        } else if (rede_igual_ci(an, "secure")) seguro = true;
        else if (rede_igual_ci(an, "httponly")) so_http = true;
        else if (rede_igual_ci(an, "samesite")) {
            ss = rede_igual_ci(vv, "strict") ? SS_STRICT : rede_igual_ci(vv, "lax") ? SS_LAX : rede_igual_ci(vv, "none") ? SS_NONE : SS_NADA;
        }
        a = prox;
    }
    if (expira > t + 400u * 86400u) expira = t + 400u * 86400u;   /* tope de 400 días */

    /* reglas de aceptación */
    bool ok = true;
    bool so_host = true;
    char dom_final[130];
    copia_texto(dom_final, host, sizeof dom_final);
    if (dominio && dominio[0]) {
        if (e_ip(host)) { if (compara_texto(dominio, host) != 0) ok = false; }
        else if (!sufijo_de(host, dominio) || dominio_publico(dominio)) ok = false;
        else { copia_texto(dom_final, dominio, sizeof dom_final); so_host = false; }
    }
    if (ok && seguro && !u.https) ok = false;                 /* un Secure no se fija desde http */
    if (ok && ss == SS_NONE && !seguro) ok = false;           /* SameSite=None exige Secure */
    if (ok && de_js && so_http) ok = false;                   /* JS no puede fijar HttpOnly */
    if (ok && rede_comeca_ci(nome, "__secure-") && !(seguro && u.https)) ok = false;
    if (ok && rede_comeca_ci(nome, "__host-") && !(seguro && u.https && so_host && (!caminho[0] || compara_texto(caminho, "/") == 0))) ok = false;
    if (ok && rede_comeca_ci(nome, "__host-") && !caminho[0]) copia_texto(caminho, "/", sizeof caminho);
    if (!ok) { libera(w); return; }
    if (!caminho[0]) caminho_padrao(u.caminho, caminho, sizeof caminho);

    mutex_tomar(&trava);
    /* ¿ya existe? (mismo nombre, dominio y ruta) */
    int ex = -1;
    for (int i = 0; i < njarra; i++)
        if (compara_texto(jarra[i]->nome, nome) == 0 && compara_texto(jarra[i]->dominio, dom_final) == 0 &&
            compara_texto(jarra[i]->caminho, caminho) == 0 && jarra[i]->so_host == so_host) { ex = i; break; }
    if (ex >= 0 && de_js && jarra[ex]->so_http) { mutex_largar(&trava); libera(w); return; }   /* JS no pisa un HttpOnly */
    if (expira && expira <= t) {                       /* borrar */
        if (ex >= 0) ck_quitar(ex);
        mutex_largar(&trava);
        libera(w);
        return;
    }
    ck_t *c = ex >= 0 ? jarra[ex] : NULL;
    uint32_t criado = c ? c->criado : t;
    if (!c) {
        c = aloca_zerado(sizeof *c);
        if (!c) { mutex_largar(&trava); libera(w); return; }
        /* límites: 50 por dominio y 300 en total; sale el más antiguo */
        int en_dom = 0, mas_viejo = -1, mas_viejo_dom = -1;
        for (int i = 0; i < njarra; i++) {
            if (mas_viejo < 0 || jarra[i]->criado < jarra[mas_viejo]->criado) mas_viejo = i;
            if (compara_texto(jarra[i]->dominio, dom_final) == 0) {
                en_dom++;
                if (mas_viejo_dom < 0 || jarra[i]->criado < jarra[mas_viejo_dom]->criado) mas_viejo_dom = i;
            }
        }
        if (en_dom >= COOKIE_POR_DOMINIO && mas_viejo_dom >= 0) ck_quitar(mas_viejo_dom);
        else if (njarra >= COOKIE_MAX && mas_viejo >= 0) ck_quitar(mas_viejo);
        jarra[njarra++] = c;
    } else {
        libera(c->nome); libera(c->valor); libera(c->dominio); libera(c->caminho);
    }
    c->nome = dup_s(nome); c->valor = dup_s(valor); c->dominio = dup_s(dom_final); c->caminho = dup_s(caminho);
    if (!c->nome || !c->valor || !c->dominio || !c->caminho) {
        for (int i = 0; i < njarra; i++) if (jarra[i] == c) { ck_quitar(i); break; }
        mutex_largar(&trava);
        libera(w);
        return;
    }
    c->expira = expira; c->criado = criado ? criado : t; c->seguro = seguro; c->so_http = so_http; c->so_host = so_host; c->samesite = ss;
    ck_serie++;
    jarra_suja = true;
    mutex_largar(&trava);
    libera(w);
}

static bool ck_mesmo_site_ok(const ck_t *c, const char *url, const char *iniciador, bool navegacao, const char *metodo)
{
    if (!iniciador || !iniciador[0]) return true;                 /* navegación escrita a mano */
    if (rede_mesmo_site(url, iniciador)) return true;
    uint8_t ss = c->samesite == SS_NADA ? SS_LAX : c->samesite;   /* Lax por omisión, como los navegadores actuales */
    if (ss == SS_NONE) return true;
    if (ss == SS_LAX) return navegacao && (!metodo || rede_igual_ci(metodo, "GET") || rede_igual_ci(metodo, "HEAD"));
    return false;
}

/* Rellena la cabecera ordenando por ruta (más larga primero) y antigüedad. */
static int montar_cookies(const char *url, const char *iniciador, bool navegacao, const char *metodo,
                          bool credenciales, bool de_js, char *o, int max)
{
    if (!o || max <= 0) return 0;
    o[0] = 0;
    url_t u;
    if (!url || !url_separar(url, &u)) return 0;
    char host[130];
    copia_texto(host, u.host, sizeof host);
    minusculas(host);
    char cam[REDE_URL_MAX];
    copia_texto(cam, u.caminho, sizeof cam);
    char *q = acha_char(cam, '?');
    if (q) *q = 0;
    if (!credenciales && iniciador && !rede_mesma_origem(url, iniciador)) return 0;

    mutex_tomar(&trava);
    ck_purgar();
    int idx[COOKIE_MAX], ni = 0;
    for (int i = 0; i < njarra; i++) {
        ck_t *c = jarra[i];
        if (c->so_host ? compara_texto(c->dominio, host) != 0 : !sufijo_de(host, c->dominio)) continue;
        if (!caminho_casa(cam, c->caminho)) continue;
        if (c->seguro && !u.https) continue;
        if (de_js && c->so_http) continue;
        if (!de_js && !ck_mesmo_site_ok(c, url, iniciador, navegacao, metodo)) continue;
        idx[ni++] = i;
    }
    for (int a = 1; a < ni; a++) {   /* inserción: ruta larga primero, luego la más antigua */
        int v = idx[a], b = a - 1;
        while (b >= 0) {
            ck_t *x = jarra[idx[b]], *y = jarra[v];
            int lx = (int)tam_texto(x->caminho), ly = (int)tam_texto(y->caminho);
            if (lx > ly || (lx == ly && x->criado <= y->criado)) break;
            idx[b + 1] = idx[b];
            b--;
        }
        idx[b + 1] = v;
    }
    int total = 0;
    for (int k = 0; k < ni; k++) {
        ck_t *c = jarra[idx[k]];
        int ln = (int)tam_texto(c->nome), lv = (int)tam_texto(c->valor);
        if (total + ln + lv + 4 >= max) break;
        if (total > 0) { o[total++] = ';'; o[total++] = ' '; }
        memcpy(o + total, c->nome, (size_t)ln); total += ln;
        o[total++] = '=';
        memcpy(o + total, c->valor, (size_t)lv); total += lv;
        o[total] = 0;
    }
    mutex_largar(&trava);
    return total;
}

int cookie_texto(const char *url, char *o, int max, bool de_js)
{
    return montar_cookies(url, NULL, false, "GET", true, de_js, o, max);
}

int rede_cookie_cabecalho(const char *url, const char *iniciador, bool navegacao, const char *metodo,
                          bool credenciales, char *o, int max)
{
    return montar_cookies(url, iniciador, navegacao, metodo, credenciales, false, o, max);
}

void cookie_limpar(void)
{
    mutex_tomar(&trava);
    while (njarra) ck_quitar(njarra - 1);
    jarra_suja = true;
    mutex_largar(&trava);
    rede_cookie_gravar(true);
}

int rede_cookie_contar(int *de_sesion)
{
    mutex_tomar(&trava);
    ck_purgar();
    int s = 0;
    for (int i = 0; i < njarra; i++) if (!jarra[i]->expira) s++;
    int n = njarra;
    mutex_largar(&trava);
    if (de_sesion) *de_sesion = s;
    return n;
}

int rede_cookie_lista(char *o, int max)
{
    int k = 0;
    o[0] = 0;
    mutex_tomar(&trava);
    for (int i = 0; i < njarra; i++) {
        int n = formatar_texto(o + k, (size_t)(max - k), "%s%s %s=%s\n", jarra[i]->so_host ? "" : ".", jarra[i]->dominio, jarra[i]->nome, "...");
        if (k + n >= max - 80) break;
        k += n;
    }
    mutex_largar(&trava);
    return k;
}

/* ---- persistencia: <home>/.config/cipo/cookies, una línea por cookie persistente ---- */

static void esc_campo(const char *s, char *o, int max)
{
    static const char hx[] = "0123456789ABCDEF";
    int k = 0;
    for (; *s && k < max - 4; s++) {
        uint8_t c = (uint8_t)*s;
        if (c < 32 || c == '%' || c == 127) { o[k++] = '%'; o[k++] = hx[c >> 4]; o[k++] = hx[c & 15]; }
        else o[k++] = (char)c;
    }
    o[k] = 0;
}

static void des_campo(char *s)
{
    char *d = s;
    for (; *s; s++) {
        if (*s == '%' && s[1] && s[2]) {
            int a = s[1] <= '9' ? s[1] - '0' : (mn(s[1]) - 'a' + 10), b = s[2] <= '9' ? s[2] - '0' : (mn(s[2]) - 'a' + 10);
            *d++ = (char)(a * 16 + b);
            s += 2;
        } else *d++ = *s;
    }
    *d = 0;
}

void rede_cookie_gravar(bool forcar)
{
    mutex_tomar(&trava);
    if (!jarra_suja && !forcar) { mutex_largar(&trava); return; }
    jarra_suja = false;
    char dir[96], caminho[128];
    rede_pasta_cipo(dir, sizeof dir);
    formatar_texto(caminho, sizeof caminho, "%s/cookies", dir);
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd >= 0) {
        uint32_t t = agora();
        for (int i = 0; i < njarra; i++) {
            ck_t *c = jarra[i];
            if (!c->expira || c->expira <= t) continue;     /* los de sesión no se guardan */
            char a[300], l[1400];
            esc_campo(c->nome, a, sizeof a);
            char *vv = aloca(COOKIE_TAM_MAX * 3 + 4);
            if (!vv) continue;
            esc_campo(c->valor, vv, COOKIE_TAM_MAX * 3 + 4);
            int n = formatar_texto(l, sizeof l, "%s\t%s\t%s\t", c->dominio, c->caminho, a);
            escrever_tudo(fd, l, (unsigned)n);
            escrever_tudo(fd, vv, (unsigned)tam_texto(vv));
            libera(vv);
            n = formatar_texto(l, sizeof l, "\t%u\t%u\t%d\t%d\t%d\t%d\n", c->expira, c->criado, c->seguro, c->so_http, c->so_host, c->samesite);
            escrever_tudo(fd, l, (unsigned)n);
        }
        fechar(fd);
    }
    mutex_largar(&trava);
}

void rede_cookie_carregar(void)
{
    if (jarra_carregada) return;
    jarra_carregada = true;
    char dir[96], caminho[128];
    rede_pasta_cipo(dir, sizeof dir);
    formatar_texto(caminho, sizeof caminho, "%s/cookies", dir);
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return;
    char *l = aloca(COOKIE_TAM_MAX * 3 + 1024);
    if (!l) { fechar(fd); return; }
    uint32_t t = agora();
    mutex_tomar(&trava);
    while (njarra < COOKIE_MAX) {
        int n = ler_linha(fd, l, COOKIE_TAM_MAX * 3 + 1024);
        if (n < 0) break;
        char *f[10];
        int nf = 0;
        f[nf++] = l;
        for (char *p = l; *p && nf < 10; p++) if (*p == '\t') { *p = 0; f[nf++] = p + 1; }
        if (nf != 10) continue;
        uint32_t ex = 0, cr = 0;
        for (char *p = f[4]; *p >= '0' && *p <= '9'; p++) ex = ex * 10 + (uint32_t)(*p - '0');
        for (char *p = f[5]; *p >= '0' && *p <= '9'; p++) cr = cr * 10 + (uint32_t)(*p - '0');
        if (!ex || ex <= t) continue;
        des_campo(f[2]); des_campo(f[3]);
        ck_t *c = aloca_zerado(sizeof *c);
        if (!c) break;
        c->dominio = dup_s(f[0]); c->caminho = dup_s(f[1]); c->nome = dup_s(f[2]); c->valor = dup_s(f[3]);
        c->expira = ex; c->criado = cr;
        c->seguro = f[6][0] == '1'; c->so_http = f[7][0] == '1'; c->so_host = f[8][0] == '1';
        c->samesite = (uint8_t)(f[9][0] - '0');
        if (!c->dominio || !c->caminho || !c->nome || !c->valor) { ck_liberar(c); break; }
        jarra[njarra++] = c;
    }
    mutex_largar(&trava);
    libera(l);
    fechar(fd);
}

void rede_cookie_recarregar(void)
{
    mutex_tomar(&trava);
    while (njarra) ck_quitar(njarra - 1);
    jarra_suja = false;
    jarra_carregada = false;
    mutex_largar(&trava);
    rede_cookie_carregar();
}

int rede_cookie_em_disco(void)
{
    char dir[96], caminho[128];
    rede_pasta_cipo(dir, sizeof dir);
    formatar_texto(caminho, sizeof caminho, "%s/cookies", dir);
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return 0;
    char *l = aloca(COOKIE_TAM_MAX * 3 + 1024);
    int n = 0;
    if (l) { while (ler_linha(fd, l, COOKIE_TAM_MAX * 3 + 1024) >= 0) n++; libera(l); }
    fechar(fd);
    return n;
}

/* ------------------------------------------------------------ caché HTTP */

#define CACHE_MAX 48
#define CACHE_ITEM_MAX (1024 * 1024)
#define CACHE_TOTAL_MAX (6 * 1024 * 1024)
typedef struct {
    char *url;
    uint8_t *corpo; int tam;
    char *cab;
    int codigo;
    uint32_t guardado, vida;      /* segundos */
    char etag[100], lm[48];
    uint32_t uso;
} cc_t;
static cc_t *cache[CACHE_MAX];
static int ncache, cache_bytes;

static void cc_liberar(cc_t *c)
{
    cache_bytes -= c->tam;
    libera(c->url); libera(c->corpo); libera(c->cab); libera(c);
}

static void cc_quitar(int i)
{
    cc_liberar(cache[i]);
    memmove(cache + i, cache + i + 1, sizeof cache[0] * (size_t)(ncache - i - 1));
    ncache--;
}

static int cc_achar(const char *url)
{
    for (int i = 0; i < ncache; i++) if (compara_texto(cache[i]->url, url) == 0) return i;
    return -1;
}

/* La vida útil según las cabeceras: -1 = no se guarda, 0 = hay que revalidar siempre. */
static int vida_util(const char *cab, bool tem_validador, uint32_t t)
{
    char cc[300];
    int vida = -1;
    bool sem_cache = false;
    if (rede_cab_valor(cab, "Cache-Control", cc, sizeof cc)) {
        minusculas(cc);
        if (rede_acha(cc, "no-store")) return -1;
        if (rede_acha(cc, "no-cache")) sem_cache = true;
        const char *m = rede_acha(cc, "max-age=");
        if (m) { int v = 0, d = 0; m += 8; while (*m >= '0' && *m <= '9' && d < 9) { v = v * 10 + (*m++ - '0'); d++; } vida = v; }
    }
    if (sem_cache) return tem_validador ? 0 : -1;
    char pragma[40];
    if (rede_cab_valor(cab, "Pragma", pragma, sizeof pragma) && rede_acha(pragma, "no-cache") && vida < 0) return tem_validador ? 0 : -1;
    char v[64];
    if (vida < 0 && rede_cab_valor(cab, "Expires", v, sizeof v)) {
        uint32_t e = rede_data_http(v), d = 0;
        char dv[64];
        if (rede_cab_valor(cab, "Date", dv, sizeof dv)) d = rede_data_http(dv);
        if (!d) d = t;
        vida = e > d ? (int)(e - d) : 0;
    }
    if (vida < 0 && tem_validador) {   /* heurística: el 10% del tiempo desde la última modificación, hasta una hora */
        char lm[64];
        if (rede_cab_valor(cab, "Last-Modified", lm, sizeof lm)) {
            uint32_t m = rede_data_http(lm);
            if (m && t > m) { uint32_t h = (t - m) / 10; vida = h > 3600 ? 3600 : (int)h; }
        }
        if (vida < 0) vida = 0;
    }
    return vida;
}

static bool vary_ok(const char *cab)
{
    char v[200];
    if (!rede_cab_valor(cab, "Vary", v, sizeof v)) return true;
    minusculas(v);
    for (char *p = v; *p; ) {
        while (*p == ' ' || *p == ',') p++;
        char *ini = p;
        while (*p && *p != ',' && *p != ' ') p++;
        char c = *p; *p = 0;
        bool ok = !*ini || compara_texto(ini, "accept-encoding") == 0;
        *p = c;
        if (!ok) return false;
    }
    return true;
}

void rede_cache_guardar(const char *url, int codigo, const char *cab, const uint8_t *corpo, int n)
{
    if (codigo != 200 || n < 0 || n > CACHE_ITEM_MAX || !cab || !vary_ok(cab)) return;
    char etag[100] = "", lm[48] = "";
    rede_cab_valor(cab, "ETag", etag, sizeof etag);
    rede_cab_valor(cab, "Last-Modified", lm, sizeof lm);
    uint32_t t = agora();
    int vida = vida_util(cab, etag[0] || lm[0], t);
    if (vida < 0) return;
    if (!etag[0] && !lm[0] && vida == 0) return;
    cc_t *c = aloca_zerado(sizeof *c);
    if (!c) return;
    c->url = dup_s(url);
    c->corpo = aloca((size_t)n + 1);
    c->cab = dup_s(cab);
    if (!c->url || !c->corpo || !c->cab) { libera(c->url); libera(c->corpo); libera(c->cab); libera(c); return; }
    memcpy(c->corpo, corpo, (size_t)n);
    c->tam = n; c->codigo = codigo; c->guardado = t; c->vida = (uint32_t)vida;
    copia_texto(c->etag, etag, sizeof c->etag); copia_texto(c->lm, lm, sizeof c->lm);
    c->uso = tempo_ms();
    mutex_tomar(&trava);
    int i = cc_achar(url);
    if (i >= 0) cc_quitar(i);
    cache_bytes += c->tam;
    while ((ncache >= CACHE_MAX || cache_bytes > CACHE_TOTAL_MAX) && ncache > 0) {
        int v = 0;
        for (int k = 1; k < ncache; k++) if ((int)(cache[k]->uso - cache[v]->uso) < 0) v = k;
        cc_quitar(v);
    }
    cache[ncache++] = c;
    mutex_largar(&trava);
}

int rede_cache_consultar(const char *url, char *etag, int emax, char *lm, int lmax)
{
    mutex_tomar(&trava);
    int i = cc_achar(url), r = CACHE_NADA;
    if (i >= 0) {
        cc_t *c = cache[i];
        if (etag) copia_texto(etag, c->etag, (size_t)emax);
        if (lm) copia_texto(lm, c->lm, (size_t)lmax);
        r = (agora() - c->guardado) < c->vida ? CACHE_FRESCA : (c->etag[0] || c->lm[0] ? CACHE_VELHA : CACHE_NADA);
        c->uso = tempo_ms();
    }
    mutex_largar(&trava);
    return r;
}

uint8_t *rede_cache_ler(const char *url, int *n, char **cab)
{
    uint8_t *o = NULL;
    mutex_tomar(&trava);
    int i = cc_achar(url);
    if (i >= 0) {
        cc_t *c = cache[i];
        o = aloca((size_t)c->tam + 1);
        char *h = dup_s(c->cab);
        if (o && h) { memcpy(o, c->corpo, (size_t)c->tam); o[c->tam] = 0; *n = c->tam; if (cab) *cab = h; else libera(h); }
        else { libera(o); libera(h); o = NULL; }
    }
    mutex_largar(&trava);
    return o;
}

void rede_cache_renovar(const char *url, const char *cab)
{
    mutex_tomar(&trava);
    int i = cc_achar(url);
    if (i >= 0 && cab) {
        cc_t *c = cache[i];
        uint32_t t = agora();
        char etag[100];
        int vida = vida_util(cab, c->etag[0] || c->lm[0], t);
        if (vida >= 0) c->vida = (uint32_t)vida;
        c->guardado = t;
        if (rede_cab_valor(cab, "ETag", etag, sizeof etag)) copia_texto(c->etag, etag, sizeof c->etag);
    }
    mutex_largar(&trava);
}

void rede_cache_limpar(void)
{
    mutex_tomar(&trava);
    while (ncache) cc_quitar(ncache - 1);
    mutex_largar(&trava);
}

int rede_cache_contar(int *bytes)
{
    if (bytes) *bytes = cache_bytes;
    return ncache;
}

/* ------------------------------------------------------------ pedidos */

static bool cab_prohibida(const char *nome)
{
    static const char *p[] = { "host", "content-length", "connection", "cookie", "cookie2", "transfer-encoding", "te", "upgrade",
                               "origin", "referer", "accept-encoding", "keep-alive", "expect", "date", "via", "trailer" };
    for (unsigned i = 0; i < sizeof p / sizeof *p; i++) if (rede_igual_ci(nome, p[i])) return true;
    return rede_comeca_ci(nome, "sec-") || rede_comeca_ci(nome, "proxy-");
}

static bool tem_cab(const char *cabs, const char *nome)
{
    int t = (int)tam_texto(nome);
    for (const char *l = cabs; l && *l;) {
        int i = 0;
        while (i < t && l[i] && mn(l[i]) == mn(nome[i])) i++;
        if (i == t && l[i] == ':') return true;
        l = acha_char(l, '\n');
        if (l) l++;
    }
    return false;
}

#define ADD(...) do { int _n = formatar_texto(buf + k, (size_t)(max - k), __VA_ARGS__); if (_n < 0 || k + _n >= max - 8) return -1; k += _n; } while (0)

int rede_montar_pedido(const rede_pedido_t *q, const char *metodo, const url_t *u, const char *url,
                       const char *tipo_corpo, int ncorpo, const char *etag, const char *lm, char *buf, int max)
{
    int k = 0;
    char porta[8] = "";
    if (u->porta != (u->https ? 443 : 80)) formatar_texto(porta, sizeof porta, ":%d", u->porta);
    ADD("%s %s HTTP/1.1\r\nHost: %s%s\r\nUser-Agent: " REDE_UA "\r\n", metodo, u->caminho, u->host, porta);
    const char *cabs = q->cabs ? q->cabs : "";
    if (!tem_cab(cabs, "Accept")) ADD("Accept: %s\r\n", (q->flags & RP_NAVEGACAO) ? "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8" : "*/*");
    if (!tem_cab(cabs, "Accept-Language")) ADD("Accept-Language: es-ES,es;q=0.9,en;q=0.5\r\n");
    if (!(q->flags & RP_SEM_CODIF)) ADD("Accept-Encoding: gzip, deflate, br\r\n");
    bool cred = (q->flags & RP_CREDENCIAIS) || !q->iniciador || rede_mesma_origem(url, q->iniciador);
    char *ck = (q->flags & RP_SEM_COOKIES) ? NULL : aloca(8192);
    if (ck) {
        int n = rede_cookie_cabecalho(url, q->iniciador, (q->flags & RP_NAVEGACAO) != 0, metodo, cred, ck, 8192);
        if (n > 0) ADD("Cookie: %s\r\n", ck);
        libera(ck);
    }
    if (q->iniciador && q->iniciador[0]) {
        char og[200], ref[REDE_URL_MAX];
        bool mesma = rede_mesma_origem(url, q->iniciador);
        if ((!mesma || rede_igual_ci(metodo, "POST") || (q->flags & RP_CORS)) && rede_origem(q->iniciador, og, sizeof og))
            ADD("Origin: %s\r\n", og);
        if (!(q->flags & RP_SEM_REFERER) && rede_origem(q->iniciador, og, sizeof og)) {
            bool baixa = comeca_com(q->iniciador, "https://") && !u->https;   /* sin degradar de https a http */
            if (!baixa) {
                if (mesma) { copia_texto(ref, q->iniciador, sizeof ref); char *f = acha_char(ref, '#'); if (f) *f = 0; }
                else formatar_texto(ref, sizeof ref, "%s/", og);
                ADD("Referer: %s\r\n", ref);
            }
        }
    }
    if (etag && etag[0]) ADD("If-None-Match: %s\r\n", etag);
    if (lm && lm[0] && !(etag && etag[0])) ADD("If-Modified-Since: %s\r\n", lm);
    if (ncorpo > 0 || rede_igual_ci(metodo, "POST") || rede_igual_ci(metodo, "PUT") || rede_igual_ci(metodo, "PATCH")) {
        if (tipo_corpo && tipo_corpo[0] && !tem_cab(cabs, "Content-Type")) ADD("Content-Type: %s\r\n", tipo_corpo);
        ADD("Content-Length: %d\r\n", ncorpo);
    }
    /* las cabeceras del script, menos las prohibidas */
    for (const char *l = cabs; *l;) {
        const char *fin = acha_char(l, '\n');
        int ln = fin ? (int)(fin - l) : (int)tam_texto(l);
        const char *dp = l;
        while (dp < l + ln && *dp != ':') dp++;
        if (dp < l + ln && ln < 1500) {
            char nome[64];
            int nn = (int)(dp - l);
            if (nn > 0 && nn < (int)sizeof nome) {
                memcpy(nome, l, (size_t)nn); nome[nn] = 0;
                if (!cab_prohibida(nome) && !(rede_igual_ci(nome, "user-agent") && 0)) {
                    char t[1600];
                    int tl = ln;
                    if (tl > 0 && l[tl - 1] == '\r') tl--;
                    memcpy(t, l, (size_t)tl); t[tl] = 0;
                    ADD("%s\r\n", t);
                }
            }
        }
        if (!fin) break;
        l = fin + 1;
    }
    ADD("Connection: close\r\n\r\n");
    return k;
}

bool rede_redirecao(int codigo, char *metodo, int mmax)
{
    if (codigo == 303) {
        if (!rede_igual_ci(metodo, "HEAD")) copia_texto(metodo, "GET", (size_t)mmax);
        return false;
    }
    if ((codigo == 301 || codigo == 302) && rede_igual_ci(metodo, "POST")) {
        copia_texto(metodo, "GET", (size_t)mmax);
        return false;
    }
    return true;   /* 307 y 308 (y 301/302 con otros métodos) conservan método y cuerpo */
}

static void uno_set_cookie(const char *v, void *ctx) { cookie_guardar((const char *)ctx, v, false); }

void rede_cookies_da_resposta(const char *url, const char *cab)
{
    rede_cab_cada(cab, "Set-Cookie", uno_set_cookie, (void *)url);
}

/* ------------------------------------------------------------ descompresión */

static uint8_t *inflar_cresciendo(const uint8_t *ent, int n, int maximo, int dica, int *saida_n)
{
    int cap = dica > 0 ? dica : n * 4 + 4096;
    if (cap < 4096) cap = 4096;
    if (cap > maximo) cap = maximo;
    for (;;) {
        uint8_t *s = aloca((size_t)cap + 1);
        if (!s) return NULL;
        int prod = 0;
        int r = inflar_parcial(ent, n, s, cap, &prod);
        if (r >= 0) { *saida_n = r; s[r] = 0; return s; }
        if (r == COMP_ERRO_CHEIO && cap < maximo) {
            libera(s);
            cap = cap > maximo / 2 ? maximo : cap * 2;
            continue;
        }
        if (r == COMP_ERRO_CHEIO) { *saida_n = prod; s[prod] = 0; return s; }   /* tope: se recorta */
        if (prod > 0) { *saida_n = prod; s[prod] = 0; return s; }              /* flujo truncado: lo que haya */
        libera(s);
        return NULL;
    }
}

static uint8_t *des_gzip(const uint8_t *d, int n, int maximo, int *saida_n)
{
    if (n < 18 || d[0] != 0x1F || d[1] != 0x8B || d[2] != 8) return NULL;
    int flg = d[3], p = 10;
    if (flg & 4) { if (p + 2 > n) return NULL; p += 2 + (d[p] | d[p + 1] << 8); }
    if (flg & 8) { while (p < n && d[p]) p++; p++; }
    if (flg & 16) { while (p < n && d[p]) p++; p++; }
    if (flg & 2) p += 2;
    if (p >= n) return NULL;
    uint32_t isize = (uint32_t)d[n - 4] | (uint32_t)d[n - 3] << 8 | (uint32_t)d[n - 2] << 16 | (uint32_t)d[n - 1] << 24;
    int dica = isize > 0 && isize <= (uint32_t)maximo ? (int)isize + 16 : 0;
    return inflar_cresciendo(d + p, n - p - 8 > 0 ? n - p - 8 : n - p, maximo, dica, saida_n);
}

static uint8_t *des_deflate(const uint8_t *d, int n, int maximo, int *saida_n)
{
    /* zlib (cabecera de 2 bytes) o deflate crudo: algunos servidores mandan el segundo aunque digan "deflate" */
    if (n >= 2 && (d[0] & 0x0F) == 8 && ((d[0] << 8) | d[1]) % 31 == 0) {
        uint8_t *r = inflar_cresciendo(d + 2, n - 2, maximo, 0, saida_n);
        if (r) return r;
    }
    return inflar_cresciendo(d, n, maximo, 0, saida_n);
}

uint8_t *rede_decodificar(const char *cab, uint8_t *corpo, int *n, int maximo, const char **erro)
{
    char ce[80];
    if (!corpo || *n <= 0 || !rede_cab_valor(cab, "Content-Encoding", ce, sizeof ce)) return corpo;
    minusculas(ce);
    /* varias codificaciones ("gzip, br"): se deshacen de derecha a izquierda */
    char *cods[4];
    int nc = 0;
    for (char *p = ce; *p && nc < 4;) {
        while (*p == ' ' || *p == ',') p++;
        if (!*p) break;
        cods[nc++] = p;
        while (*p && *p != ',' && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    for (int i = nc - 1; i >= 0; i--) {
        uint8_t *nuevo = NULL;
        int nn = 0;
        const char *c = cods[i];
        if (compara_texto(c, "identity") == 0) continue;
        if (compara_texto(c, "gzip") == 0 || compara_texto(c, "x-gzip") == 0) nuevo = des_gzip(corpo, *n, maximo, &nn);
        else if (compara_texto(c, "deflate") == 0) nuevo = des_deflate(corpo, *n, maximo, &nn);
        else if (compara_texto(c, "br") == 0) { unsigned bn = 0; nuevo = brotli_descomprimir(corpo, (unsigned)*n, &bn, (unsigned)maximo); nn = (int)bn; }
        else { if (erro) *erro = "Codificación no admitida"; return corpo; }
        if (!nuevo) {
            if (erro) *erro = "No se pudo descomprimir la respuesta";
            libera(corpo);
            *n = 0;
            return NULL;
        }
        libera(corpo);
        corpo = nuevo;
        *n = nn;
    }
    return corpo;
}

/* ------------------------------------------------------------ CORS */

bool rede_cors_permite(const char *cab, const char *origem_pagina, bool credenciales)
{
    char og[200], ac[300], cr[40];
    if (!rede_origem(origem_pagina, og, sizeof og)) return false;
    if (!rede_cab_valor(cab, "Access-Control-Allow-Origin", ac, sizeof ac)) return false;
    if (compara_texto(ac, "*") == 0) return !credenciales;
    if (compara_texto(ac, og) != 0) return false;
    if (credenciales) return rede_cab_valor(cab, "Access-Control-Allow-Credentials", cr, sizeof cr) && rede_igual_ci(cr, "true");
    return true;
}

bool rede_cors_precisa_preparacao(const char *metodo, const char *cabs)
{
    if (!(rede_igual_ci(metodo, "GET") || rede_igual_ci(metodo, "HEAD") || rede_igual_ci(metodo, "POST"))) return true;
    for (const char *l = cabs ? cabs : ""; *l;) {
        const char *fin = acha_char(l, '\n');
        const char *dp = l;
        while (*dp && *dp != ':' && *dp != '\n') dp++;
        if (*dp == ':') {
            char nome[64];
            int nn = (int)(dp - l);
            if (nn > 0 && nn < (int)sizeof nome) {
                memcpy(nome, l, (size_t)nn); nome[nn] = 0;
                bool seguro = rede_igual_ci(nome, "accept") || rede_igual_ci(nome, "accept-language") || rede_igual_ci(nome, "content-language");
                if (rede_igual_ci(nome, "content-type")) {
                    const char *v = dp + 1;
                    while (*v == ' ') v++;
                    seguro = rede_comeca_ci(v, "application/x-www-form-urlencoded") || rede_comeca_ci(v, "multipart/form-data") || rede_comeca_ci(v, "text/plain");
                }
                if (!seguro && !cab_prohibida(nome)) return true;
            }
        }
        if (!fin) break;
        l = fin + 1;
    }
    return false;
}

void rede_cab_para_js(const char *cab, bool entre, char *o, int max)
{
    char expor[300] = "";
    if (entre) rede_cab_valor(cab, "Access-Control-Expose-Headers", expor, sizeof expor);
    minusculas(expor);
    int k = 0;
    o[0] = 0;
    bool primeira = true;
    for (const char *l = cab; l && *l;) {
        const char *fin = acha_char(l, '\n');
        int ln = fin ? (int)(fin - l) : (int)tam_texto(l);
        if (primeira) { primeira = false; }    /* línea de estado: se conserva, el JS la salta */
        const char *dp = l;
        while (dp < l + ln && *dp != ':') dp++;
        bool ok = true;
        if (dp < l + ln) {
            char nome[64];
            int nn = (int)(dp - l);
            if (nn >= (int)sizeof nome) nn = (int)sizeof nome - 1;
            memcpy(nome, l, (size_t)nn); nome[nn] = 0;
            minusculas(nome);
            if (compara_texto(nome, "set-cookie") == 0 || compara_texto(nome, "set-cookie2") == 0) ok = false;
            if (ok && entre) {
                static const char *seguras[] = { "cache-control", "content-language", "content-length", "content-type", "expires", "last-modified", "pragma" };
                bool s = false;
                for (unsigned i = 0; i < sizeof seguras / sizeof *seguras; i++) if (compara_texto(nome, seguras[i]) == 0) s = true;
                if (!s && !(expor[0] && (compara_texto(expor, "*") == 0 || rede_acha(expor, nome)))) ok = false;
            }
        }
        if (ok && k + ln + 2 < max) { memcpy(o + k, l, (size_t)ln); k += ln; o[k++] = '\n'; o[k] = 0; }
        if (!fin) break;
        l = fin + 1;
    }
}

/* ------------------------------------------------------------ el ejecutor de pedidos */

static rede_executor_t ejecutor;
void rede_definir_executor(rede_executor_t f) { ejecutor = f; }
int rede_executar(const rede_pedido_t *q, rede_resposta_t *r)
{
    memset(r, 0, sizeof *r);
    if (!ejecutor) return -1;
    return ejecutor(q, r);
}
void rede_resposta_liberar(rede_resposta_t *r)
{
    libera(r->cab); libera(r->corpo);
    r->cab = NULL; r->corpo = NULL; r->tam = 0;
}
