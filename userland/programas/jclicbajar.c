/* jclicbajar - "Actividades JClic (bajar)": el descargador del repositorio de Clic del XTEC (D185).
 *
 *   jclicbajar [--url BASE]                      ventana: lista, busca, baja y abre
 *   jclicbajar --lista [--buscar TEXTO] [--idioma es|ca|en|todos] [--url BASE]
 *   jclicbajar --bajar CAMINO [--url BASE]       baja un proyecto (CAMINO es el "path" del indice)
 *   jclicbajar --url-zip https://.../x.jclic.zip  baja un .jclic.zip suelto
 *
 * El repositorio (https://projectes.xtec.cat/clic/es/repo/) publica su indice en
 * https://clic.xtec.cat/projects/projects.json (una lista de unos 2.600 proyectos con titulo, autor,
 * fecha, idiomas, niveles y areas) y cada proyecto como BASE/<path>/jclic/<nombre>.jclic.zip, donde
 * <nombre> es el del fichero principal. Con --url BASE se cambia el servidor (espejo, pruebas).
 *
 * Como el modelo de chip8-roms (D123): NO se distribuye ningun proyecto, se bajan al usarlos a
 * /home/jclic (o /tmp/jclic si no hay disco de datos, con aviso), cada uno con un <proyecto>.txt con
 * el titulo, el autor, la fecha y la direccion: cada proyecto tiene su propia licencia (normalmente
 * Creative Commons) y hay que mirarla en el repositorio antes de redistribuirlo. El indice se
 * guarda en la misma carpeta (projects.json) para trabajar sin red. HTTPS con la libtls.
 *
 * Solo enteros; el JSON se lee con un analizador propio (el indice es una lista de objetos planos). */
#include "sagui.h"
#include "gui.h"
#include "tls.h"
#include "compressao.h"


/* Coloca un botón sin tocar su estado de pulsación (si se rehiciera entero, un clic que llega en dos
 * eventos —pulsar y soltar— perdería el "apretado" entre un dibujo y otro). */
static void bot_pos(botao_t *b, int x, int y, int l, int a, const char *t)
{
    b->x = x; b->y = y; b->l = l; b->a = a; b->texto = t;
}

#define BASE_PADRAO "https://clic.xtec.cat/projects"
#define MAX_PROJ 6000
#define MAX_BAIXA (48u * 1024 * 1024)

typedef struct {
    char *titulo, *autor, *data, *caminho, *principal;
    char  langs[24], niveis[40], areas[40];
} proj_t;

static proj_t *proj;
static int n_proj;
static char base_url[300] = BASE_PADRAO;
static char pasta[64];
static bool pasta_temporaria;
static char estado[300];

/* ------------------------------------------------------------ memoria de textos */

static char *pool;
static size_t pool_uso, pool_cap;
static void *pool_blocos[512];
static int n_blocos;

static char *guardar(const char *s, int n)
{
    if (!pool || pool_uso + (size_t)n + 1 > pool_cap) {
        pool_cap = 131072;
        pool = aloca(pool_cap);
        pool_uso = 0;
        if (!pool) return NULL;
        if (n_blocos < 512) pool_blocos[n_blocos++] = pool;
    }
    char *d = pool + pool_uso;
    memcpy(d, s, (size_t)n);
    d[n] = 0;
    pool_uso += (size_t)n + 1;
    return d;
}

static void indice_soltar(void)
{
    for (int i = 0; i < n_blocos; i++) libera(pool_blocos[i]);
    n_blocos = 0;
    pool = NULL;
    libera(proj);
    proj = NULL;
    n_proj = 0;
}

/* ------------------------------------------------------------ JSON */

typedef struct { const char *p, *fim; } js_t;

static void js_esp(js_t *j) { while (j->p < j->fim && (*j->p == ' ' || *j->p == '\n' || *j->p == '\r' || *j->p == '\t')) j->p++; }

static int js_latin1(unsigned cp)
{
    if (cp < 256) return (int)cp;
    switch (cp) {
    case 0x2018: case 0x2019: return '\'';
    case 0x201C: case 0x201D: return '"';
    case 0x2013: case 0x2014: return '-';
    case 0x2026: return '.';
    case 0x0107: case 0x010D: return 'c';
    case 0x0142: return 'l';
    case 0x0161: return 's';
    case 0x017E: return 'z';
    default: return '?';
    }
}

/* Lee una cadena JSON ("..." ya posicionado en la comilla) a Latin-1 en el buffer; devuelve su largo. */
static int js_cadena(js_t *j, char *buf, int max)
{
    int o = 0;
    if (j->p >= j->fim || *j->p != '"') return -1;
    j->p++;
    while (j->p < j->fim && *j->p != '"') {
        unsigned char c = (unsigned char)*j->p++;
        int r = c;
        if (c == '\\' && j->p < j->fim) {
            char e = *j->p++;
            if (e == 'n') r = ' ';
            else if (e == 't') r = ' ';
            else if (e == 'u' && j->p + 4 <= j->fim) {
                unsigned v = 0;
                for (int k = 0; k < 4; k++) {
                    char h = j->p[k];
                    v = v * 16 + (unsigned)(h >= '0' && h <= '9' ? h - '0' : (h | 0x20) >= 'a' && (h | 0x20) <= 'f' ? (h | 0x20) - 'a' + 10 : 0);
                }
                j->p += 4;
                if (v >= 0xD800 && v < 0xE000 && j->p + 6 <= j->fim && j->p[0] == '\\') j->p += 6;   /* pareja subrogada: un solo '?' */
                r = js_latin1(v);
            } else r = (unsigned char)e;
        } else if (c >= 0xC2 && c < 0xE0 && j->p < j->fim) {   /* UTF-8 */
            r = js_latin1(((unsigned)(c & 0x1F) << 6) | ((unsigned char)*j->p & 0x3F));
            j->p++;
        } else if (c >= 0xE0 && j->p + 1 < j->fim) {
            r = js_latin1(((unsigned)(c & 0x0F) << 12) | (((unsigned char)j->p[0] & 0x3F) << 6) | ((unsigned char)j->p[1] & 0x3F));
            j->p += 2;
        }
        if (o < max - 1) buf[o++] = (char)r;
    }
    if (j->p < j->fim) j->p++;
    buf[o] = 0;
    return o;
}

static void js_saltar(js_t *j)
{
    js_esp(j);
    if (j->p >= j->fim) return;
    if (*j->p == '"') { char t[8]; js_t k = *j; while (k.p < k.fim) { js_cadena(&k, t, sizeof t); break; } *j = k; return; }
    if (*j->p == '[' || *j->p == '{') {
        int prof = 0;
        while (j->p < j->fim) {
            char c = *j->p;
            if (c == '"') { char t[4]; js_cadena(j, t, sizeof t); continue; }
            if (c == '[' || c == '{') prof++;
            if (c == ']' || c == '}') prof--;
            j->p++;
            if (prof == 0) break;
        }
        return;
    }
    while (j->p < j->fim && *j->p != ',' && *j->p != '}' && *j->p != ']') j->p++;
}

/* Un array de cadenas a "a,b,c". */
static void js_lista(js_t *j, char *dst, int max)
{
    dst[0] = 0;
    js_esp(j);
    if (j->p >= j->fim || *j->p != '[') { js_saltar(j); return; }
    j->p++;
    int o = 0;
    for (;;) {
        js_esp(j);
        if (j->p >= j->fim || *j->p == ']') { j->p++; break; }
        if (*j->p == ',') { j->p++; continue; }
        char t[40];
        if (js_cadena(j, t, sizeof t) < 0) { js_saltar(j); continue; }
        int n = (int)tam_texto(t);
        if (o + n + 2 < max) { if (o) dst[o++] = ','; memcpy(dst + o, t, (size_t)n); o += n; dst[o] = 0; }
    }
}

/* Analiza el indice: [ {...}, {...} ]. Devuelve el numero de proyectos. */
static int indice_analisar(const char *d, int n)
{
    indice_soltar();
    proj = aloca_zerado(sizeof(proj_t) * MAX_PROJ);
    if (!proj) return 0;
    js_t j = { d, d + n };
    js_esp(&j);
    if (j.p >= j.fim || *j.p != '[') return 0;
    j.p++;
    static char t[600];
    while (n_proj < MAX_PROJ) {
        js_esp(&j);
        if (j.p >= j.fim || *j.p == ']') break;
        if (*j.p == ',') { j.p++; continue; }
        if (*j.p != '{') { js_saltar(&j); continue; }
        j.p++;
        proj_t *p = &proj[n_proj];
        for (;;) {
            js_esp(&j);
            if (j.p >= j.fim) break;
            if (*j.p == '}') { j.p++; break; }
            if (*j.p == ',') { j.p++; continue; }
            char chave[32];
            if (js_cadena(&j, chave, sizeof chave) < 0) break;
            js_esp(&j);
            if (j.p < j.fim && *j.p == ':') j.p++;
            js_esp(&j);
            if (!compara_texto(chave, "title") || !compara_texto(chave, "author") || !compara_texto(chave, "date") ||
                !compara_texto(chave, "path") || !compara_texto(chave, "mainFile")) {
                int l = js_cadena(&j, t, sizeof t);
                char *s = guardar(t, l < 0 ? 0 : l);
                if (!compara_texto(chave, "title")) p->titulo = s;
                else if (!compara_texto(chave, "author")) p->autor = s;
                else if (!compara_texto(chave, "date")) p->data = s;
                else if (!compara_texto(chave, "path")) p->caminho = s;
                else p->principal = s;
            } else if (!compara_texto(chave, "langCodes")) js_lista(&j, p->langs, sizeof p->langs);
            else if (!compara_texto(chave, "levelCodes")) js_lista(&j, p->niveis, sizeof p->niveis);
            else if (!compara_texto(chave, "areaCodes")) js_lista(&j, p->areas, sizeof p->areas);
            else js_saltar(&j);
        }
        if (p->caminho && p->principal && p->titulo) n_proj++;
        else { memset(p, 0, sizeof *p); }
    }
    return n_proj;
}

/* ------------------------------------------------------------ red */

static uint8_t *baixado;
static int baixado_n;
static void (*progresso_cb)(unsigned feito, int total);

/* GET de la URL entera a memoria (sigue hasta 6 redirecciones). Devuelve 0 o un codigo de error (texto en estado). */
static int obter(const char *url0)
{
    static char url[600], destino[600], nova[600];
    static http_resposta_t resp;
    static conexao_t con;
    static uint8_t bloco[16384];
    copia_texto(url, url0, sizeof url);
    tls_config_t cfg;
    memset(&cfg, 0, sizeof cfg);
    bool cfg_pronta = false;
    libera(baixado);
    baixado = NULL;
    baixado_n = 0;
    for (int redir = 0; redir < 7; redir++) {
        url_t u;
        if (!url_separar(url, &u)) { formatar_texto(estado, sizeof estado, "Direccion no valida: %s", url); return 3; }
        if (u.https && !cfg_pronta) {
            char erro[200];
            if (!tls_config_padrao(&cfg, NULL, erro, sizeof erro)) { formatar_texto(estado, sizeof estado, "TLS: %s", erro); return 5; }
            cfg_pronta = true;
        }
        int r = conexao_abrir(&con, &u, &cfg);
        if (r < 0) { formatar_texto(estado, sizeof estado, "No se pudo conectar con %s: %s", u.host, con.erro); conexao_fechar(&con); return 4; }
        char pedido[1024];
        int np = http_montar_pedido(&u, "GET", NULL, pedido, sizeof pedido);
        if (conexao_escrever(&con, pedido, (unsigned)np) < 0) { copia_texto(estado, "No se pudo enviar el pedido", sizeof estado); conexao_fechar(&con); return 4; }
        r = http_ler_cabecalho_de(&resp, conexao_ler, &con);
        if (r < 0) { copia_texto(estado, "Respuesta no valida del servidor", sizeof estado); conexao_fechar(&con); return 8; }
        int c = resp.codigo;
        if ((c == 301 || c == 302 || c == 303 || c == 307 || c == 308) && http_campo(resp.cabecalho, "Location", destino, sizeof destino)) {
            conexao_fechar(&con);
            if (!url_juntar(url, destino, nova, sizeof nova)) { copia_texto(estado, "Redireccion no valida", sizeof estado); return 8; }
            copia_texto(url, nova, sizeof url);
            continue;
        }
        if (c >= 400 || c < 200) {
            formatar_texto(estado, sizeof estado, "El servidor respondio %d para %s", c, u.caminho);
            conexao_fechar(&con);
            return 8;
        }
        int total = resp.pedacos ? -1 : resp.restante;
        if (total > (int)MAX_BAIXA) { copia_texto(estado, "El archivo es demasiado grande (mas de 48 MB)", sizeof estado); conexao_fechar(&con); return 9; }
        int cap = total > 0 ? total + 1 : 262144;
        baixado = aloca((size_t)cap);
        if (!baixado) { copia_texto(estado, "Sin memoria", sizeof estado); conexao_fechar(&con); return 9; }
        int n;
        unsigned t_ult = 0;
        while ((n = http_ler_corpo(&resp, bloco, sizeof bloco)) > 0) {
            if ((unsigned)baixado_n + (unsigned)n > MAX_BAIXA) { n = -1; copia_texto(estado, "El archivo es demasiado grande (mas de 48 MB)", sizeof estado); break; }
            if (baixado_n + n > cap) {
                while (baixado_n + n > cap) cap *= 2;
                uint8_t *nb = realoca(baixado, (size_t)cap);
                if (!nb) { n = -1; copia_texto(estado, "Sin memoria", sizeof estado); break; }
                baixado = nb;
            }
            memcpy(baixado + baixado_n, bloco, (size_t)n);
            baixado_n += n;
            unsigned agora = tempo_ms();
            if (progresso_cb && agora - t_ult > 150) { t_ult = agora; progresso_cb((unsigned)baixado_n, total); }
        }
        conexao_fechar(&con);
        if (n < 0) { if (!estado[0]) copia_texto(estado, "La transferencia se corto", sizeof estado); libera(baixado); baixado = NULL; baixado_n = 0; return 4; }
        return 0;
    }
    copia_texto(estado, "Demasiadas redirecciones", sizeof estado);
    return 8;
}

/* ------------------------------------------------------------ carpeta y descargas */

static void escolher_pasta(void)
{
    info_arquivo_t i;
    if (info_arquivo("/home", &i) == 0) { copia_texto(pasta, "/home/jclic", sizeof pasta); pasta_temporaria = false; }
    else { copia_texto(pasta, "/tmp/jclic", sizeof pasta); pasta_temporaria = true; }
    criar_pasta(pasta);
}

static void caminho_indice(char *s, int max) { caminho_juntar(s, (size_t)max, pasta, "projects.json"); }

/* El indice: de la red y, si no hay, de la copia guardada. rede=false usa solo la copia. */
static bool indice_carregar(bool rede)
{
    char cam[200];
    caminho_indice(cam, sizeof cam);
    estado[0] = 0;
    if (rede) {
        char url[400];
        formatar_texto(url, sizeof url, "%s/projects.json", base_url);
        int r = obter(url);
        if (r == 0 && baixado_n > 2) {
            salvar_arquivo(cam, baixado, baixado_n);
            int n = indice_analisar((const char *)baixado, baixado_n);
            libera(baixado); baixado = NULL;
            if (n > 0) { formatar_texto(estado, sizeof estado, "Indice al dia: %d proyectos.", n); return true; }
            copia_texto(estado, "El indice del servidor no se pudo leer", sizeof estado);
        }
        /* sin red: cae a la copia, conservando el aviso */
    }
    char aviso[300];
    copia_texto(aviso, estado, sizeof aviso);
    int n = 0;
    uint8_t *d = carregar_arquivo(cam, &n);
    if (d) {
        int k = indice_analisar((const char *)d, n);
        libera(d);
        if (k > 0) {
            formatar_texto(estado, sizeof estado, "%s%sUsando el indice guardado (%d proyectos).", aviso, aviso[0] ? " " : "", k);
            return true;
        }
    }
    formatar_texto(estado, sizeof estado, "%s%sNo hay indice guardado: hace falta red para la primera vez.", aviso, aviso[0] ? " " : "");
    return false;
}

static void nome_destino(const proj_t *p, char *s, int max)
{
    char t[160];
    copia_texto(t, p->caminho, sizeof t);
    for (char *q = t; *q; q++) if (*q == '/' || *q == ' ') *q = '-';
    formatar_texto(s, (size_t)max, "%s/%s.jclic.zip", pasta, t);
}

static bool ja_baixado(const proj_t *p)
{
    char c[260];
    info_arquivo_t i;
    nome_destino(p, c, sizeof c);
    return info_arquivo(c, &i) == 0;
}

static void escrever_txt(const proj_t *p, const char *zip, const char *url)
{
    char c[300], t[900];
    copia_texto(c, zip, sizeof c);
    char *pt = c + tam_texto(c) - 10;   /* ".jclic.zip" */
    if (pt > c) copia_texto(pt, ".txt", 8);
    formatar_texto(t, sizeof t, "%s\nAutor: %s\nFecha: %s\nOrigen: %s\nRepositorio: https://projectes.xtec.cat/clic/es/repo/\n"
                   "Licencia: la de cada proyecto (normalmente Creative Commons); mirala en el repositorio antes de redistribuirlo.\n",
                   p->titulo, p->autor ? p->autor : "?", p->data ? p->data : "?", url);
    salvar_arquivo(c, t, (int)tam_texto(t));
}

static bool zip_valido(const uint8_t *d, int n)
{
    zip_leitor_t z;
    if (n < 30 || d[0] != 'P' || d[1] != 'K') return false;
    if (zip_abrir(&z, d, n) < 0) return false;
    bool tem = false;
    for (int i = 0; i < z.cnt; i++) if (!z.e[i].pasta && z.e[i].nome) {
        int l = (int)tam_texto(z.e[i].nome);
        if (l > 6 && !compara_texto(z.e[i].nome + l - 6, ".jclic")) tem = true;
    }
    zip_leitor_liberar(&z);
    return tem;
}

/* Baja el proyecto p a la carpeta. Devuelve 0 o un error (texto en estado). */
static int baixar_projeto(const proj_t *p)
{
    char url[600], zip[260];
    const char *b = acha_ultimo_char(p->principal, '/');
    char nome[120];
    copia_texto(nome, b ? b + 1 : p->principal, sizeof nome);
    int ln = (int)tam_texto(nome);
    if (ln > 6 && !compara_texto(nome + ln - 6, ".jclic")) nome[ln - 6] = 0;
    formatar_texto(url, sizeof url, "%s/%s/jclic/%s.jclic.zip", base_url, p->caminho, nome);
    estado[0] = 0;
    int r = obter(url);
    if (r) return r;
    if (!zip_valido(baixado, baixado_n)) {
        copia_texto(estado, "Lo bajado no es un .jclic.zip valido", sizeof estado);
        libera(baixado); baixado = NULL;
        return 10;
    }
    nome_destino(p, zip, sizeof zip);
    r = salvar_arquivo(zip, baixado, baixado_n);
    int n = baixado_n;
    libera(baixado); baixado = NULL;
    if (r < 0) { formatar_texto(estado, sizeof estado, "No se pudo guardar %s: %s", zip, nome_erro(r)); return 11; }
    escrever_txt(p, zip, url);
    char tam[16];
    formatar_tamanho(tam, sizeof tam, (uint32_t)n);
    formatar_texto(estado, sizeof estado, "Bajado: %s (%s)%s", zip, tam, pasta_temporaria ? " - en /tmp: se pierde al apagar" : "");
    registrar("jclicbajar: bajado %s (%d bytes)", zip, n);
    return 0;
}

/* ------------------------------------------------------------ filtro */

static int *filtro;
static int n_filtro;
static char (*itens)[64];
static caixa_t busca;
static int idioma;   /* 0 es, 1 ca, 2 en, 3 todos */
static const char *const idiomas[4] = { "Espanol", "Catalan", "Ingles", "Todos" };
static const char *const cod_idioma[4] = { "es", "ca", "en", "" };

static bool contem_ci(const char *s, const char *p)
{
    if (!p[0]) return true;
    int n = (int)tam_texto(p);
    for (; *s; s++) {
        int k = 0;
        while (k < n && s[k] && ((s[k] | 0x20) == (p[k] | 0x20))) k++;
        if (k == n) return true;
    }
    return false;
}

static bool tem_codigo(const char *lista, const char *cod)
{
    if (!cod[0]) return true;
    int n = (int)tam_texto(cod);
    for (const char *s = lista; *s;) {
        const char *v = acha_char(s, ',');
        int l = v ? (int)(v - s) : (int)tam_texto(s);
        if (l == n && !compara_texto_n(s, cod, (size_t)n)) return true;
        if (!v) break;
        s = v + 1;
    }
    return false;
}

static int filtrar(const char *texto, int idi, int *saida, int max)
{
    int n = 0;
    for (int i = 0; i < n_proj && n < max; i++) {
        const proj_t *p = &proj[i];
        if (!tem_codigo(p->langs, cod_idioma[idi])) continue;
        if (texto[0] && !contem_ci(p->titulo, texto) && !contem_ci(p->autor ? p->autor : "", texto) && !contem_ci(p->caminho, texto)) continue;
        saida[n++] = i;
    }
    return n;
}

/* ------------------------------------------------------------ linea de ordenes */

static int uso(void)
{
    imprimir("uso: jclicbajar [--url BASE]\n"
             "     jclicbajar --lista [--buscar TEXTO] [--idioma es|ca|en|todos]\n"
             "     jclicbajar --bajar CAMINO\n"
             "     jclicbajar --url-zip https://sitio/proyecto.jclic.zip\n");
    return 2;
}

static int modo_texto(bool lista, const char *buscar, const char *idi, const char *bajar, const char *url_zip, bool sem_rede)
{
    escolher_pasta();
    if (url_zip) {
        int r = obter(url_zip);
        if (r) { imprimir("jclicbajar: %s\n", estado); return 1; }
        if (!zip_valido(baixado, baixado_n)) { imprimir("jclicbajar: no es un .jclic.zip valido\n"); return 1; }
        const char *b = acha_ultimo_char(url_zip, '/');
        char d[260], nome[120];
        copia_texto(nome, b ? b + 1 : "proyecto.jclic.zip", sizeof nome);
        char *q = acha_char(nome, '?');
        if (q) *q = 0;
        formatar_texto(d, sizeof d, "%s/%s", pasta, nome);
        r = salvar_arquivo(d, baixado, baixado_n);
        imprimir("jclicbajar: %s (%d bytes) %s\n", d, baixado_n, r < 0 ? "ERROR al guardar" : "guardado");
        libera(baixado);
        return r < 0;
    }
    bool ok = indice_carregar(!sem_rede);
    imprimir("jclicbajar: %s\n", estado);
    if (!ok) return 1;
    if (bajar) {
        for (int i = 0; i < n_proj; i++)
            if (!compara_texto(proj[i].caminho, bajar)) {
                int r = baixar_projeto(&proj[i]);
                imprimir("jclicbajar: %s\n", estado);
                return r != 0;
            }
        imprimir("jclicbajar: no hay ningun proyecto con el camino %s\n", bajar);
        return 1;
    }
    (void)lista;
    int ii = 0;
    for (int k = 0; k < 4; k++) if (!compara_texto(idi, cod_idioma[k][0] ? cod_idioma[k] : "todos")) ii = k;
    int *f = aloca((size_t)MAX_PROJ * sizeof(int));
    if (!f) return 1;
    int n = filtrar(buscar, ii, f, MAX_PROJ);
    imprimir("jclicbajar: %d proyectos\n", n);
    for (int i = 0; i < n && i < 40; i++)
        imprimir("  %s | %s | %s%s\n", proj[f[i]].caminho, proj[f[i]].titulo, proj[f[i]].langs, ja_baixado(&proj[f[i]]) ? " | bajado" : "");
    libera(f);
    return 0;
}

/* ------------------------------------------------------------ ventana */

static jan_t *jan;
static lista_t lst;
static botao_t bt_idioma, bt_baixar, bt_abrir, bt_indice, bt_url, bt_sair;
static bool com_rede_falhou;

static void refiltrar(void)
{
    int sel_proj = lst.sel >= 0 && lst.sel < n_filtro ? filtro[lst.sel] : -1;
    n_filtro = filtrar(busca.texto, idioma, filtro, MAX_PROJ);
    for (int i = 0; i < n_filtro; i++) {
        const proj_t *p = &proj[filtro[i]];
        formatar_texto(itens[i], 64, "%s%s", ja_baixado(p) ? "+ " : "  ", p->titulo);
        itens[i][63] = 0;
    }
    lst.qtd = n_filtro;
    lst.sel = 0;
    lst.topo = 0;
    if (sel_proj >= 0) for (int i = 0; i < n_filtro; i++) if (filtro[i] == sel_proj) { lst.sel = i; break; }
}

static void barra_estado(void)
{
    int w = jan->larg;
    g_retangulo(jan, 0, jan->alt - 26, w, 26, COR_BOTAO);
    char t[300];
    copia_texto(t, estado, sizeof t);
    if ((int)tam_texto(t) * 8 > w - 16) t[(w - 16) / 8] = 0;
    g_texto(jan, 8, jan->alt - 22, t, COR_TEXTO);
}

static void texto_quebrado(int x, int y, int w, const char *t, uint32_t cor, int linhas_max)
{
    int maxc = w / 8, n = (int)tam_texto(t), i = 0, l = 0;
    while (i < n && l < linhas_max) {
        int len = n - i > maxc ? maxc : n - i;
        if (n - i > maxc) { int k = len; while (k > 0 && t[i + k] != ' ') k--; if (k > 0) len = k; }
        g_texto_n(jan, x, y + l * 16, t + i, len, cor);
        i += len;
        while (i < n && t[i] == ' ') i++;
        l++;
    }
}

static void pintar(void)
{
    int w = jan->larg, h = jan->alt;
    g_retangulo(jan, 0, 0, w, h, COR_FUNDO);
    g_texto(jan, 12, 12, "Buscar:", COR_TEXTO);
    busca.x = 76; busca.y = 8; busca.l = 240; busca.foco = true;
    caixa_desenhar(jan, &busca);
    bot_pos(&bt_idioma, 324, 8, 100, 24, (char *)idiomas[idioma]);
    bot_pos(&bt_indice, 432, 8, 150, 24, "Actualizar indice");
    bot_pos(&bt_url, 590, 8, 90, 24, "Por URL...");
    botao_desenhar(jan, &bt_idioma);
    botao_desenhar(jan, &bt_indice);
    botao_desenhar(jan, &bt_url);
    int lw = w * 56 / 100;
    lst.x = 8; lst.y = 44; lst.l = lw; lst.a = h - 44 - 34;
    lst.itens = itens;
    lista_desenhar(jan, &lst);
    int dx = lst.x + lw + 12, dw = w - dx - 8;
    char s[400];
    if (lst.sel >= 0 && lst.sel < n_filtro) {
        const proj_t *p = &proj[filtro[lst.sel]];
        texto_quebrado(dx, 48, dw, p->titulo, COR_TEXTO, 4);
        formatar_texto(s, sizeof s, "Autor: %s", p->autor ? p->autor : "?");
        texto_quebrado(dx, 120, dw, s, COR_TEXTO_FRACO, 4);
        formatar_texto(s, sizeof s, "Fecha: %s   Idiomas: %s", p->data ? p->data : "?", p->langs);
        texto_quebrado(dx, 192, dw, s, COR_TEXTO_FRACO, 2);
        formatar_texto(s, sizeof s, "Niveles: %s   Areas: %s", p->niveis, p->areas);
        texto_quebrado(dx, 228, dw, s, COR_TEXTO_FRACO, 2);
        formatar_texto(s, sizeof s, "Camino: %s", p->caminho);
        texto_quebrado(dx, 264, dw, s, COR_TEXTO_FRACO, 2);
        if (ja_baixado(p)) g_texto(jan, dx, 300, "Ya esta bajado (+).", 0x108030);
        texto_quebrado(dx, 330, dw, "Cada proyecto tiene su propia licencia (normalmente Creative Commons): mirala en el repositorio.", COR_TEXTO_FRACO, 5);
    } else if (n_proj == 0) {
        texto_quebrado(dx, 48, dw, "No hay indice. Pulsa \"Actualizar indice\" con la red conectada.", COR_TEXTO, 4);
    }
    bot_pos(&bt_baixar, dx, h - 34 - 34, 110, 28, "Bajar");
    bot_pos(&bt_abrir, dx + 118, h - 34 - 34, 110, 28, "Jugar");
    bot_pos(&bt_sair, w - 98, h - 34 - 34, 90, 28, "Salir");
    botao_desenhar(jan, &bt_baixar);
    botao_desenhar(jan, &bt_abrir);
    botao_desenhar(jan, &bt_sair);
    barra_estado();
    jan_mostrar(jan);
}

static void progresso_janela(unsigned feito, int total)
{
    char t[16], u[16];
    formatar_tamanho(t, sizeof t, feito);
    if (total > 0) { formatar_tamanho(u, sizeof u, (uint32_t)total); formatar_texto(estado, sizeof estado, "Bajando... %s de %s", t, u); }
    else formatar_texto(estado, sizeof estado, "Bajando... %s", t);
    barra_estado();
    jan_mostrar_area(jan, 0, jan->alt - 26, jan->larg, 26);
}

static void jogar(const proj_t *p)
{
    char c[260];
    nome_destino(p, c, sizeof c);
    char *av[] = { "jclic", c, NULL };
    int pid = executar("/apps/bin/jclic", av);
    if (pid < 0) formatar_texto(estado, sizeof estado, "No se pudo abrir el visor: %s", nome_erro(pid));
}

static void baixar_url_manual(void)
{
    char url[300] = "";
    if (!pedir_texto("Bajar un proyecto por URL", "Direccion de un .jclic.zip:", url, sizeof url) || !url[0]) return;
    copia_texto(estado, "Bajando...", sizeof estado);
    pintar();
    int r = obter(url);
    if (r) return;
    if (!zip_valido(baixado, baixado_n)) { copia_texto(estado, "Lo bajado no es un .jclic.zip valido", sizeof estado); libera(baixado); baixado = NULL; return; }
    const char *b = acha_ultimo_char(url, '/');
    char d[260], nome[120];
    copia_texto(nome, b ? b + 1 : "proyecto.jclic.zip", sizeof nome);
    char *q = acha_char(nome, '?');
    if (q) *q = 0;
    if (!eh_ascii(nome) || !nome[0]) copia_texto(nome, "proyecto.jclic.zip", sizeof nome);
    if ((int)tam_texto(nome) < 10 || compara_texto(nome + tam_texto(nome) - 10, ".jclic.zip")) copia_texto(nome + (tam_texto(nome) > 100 ? 100 : tam_texto(nome)), ".jclic.zip", 12);
    formatar_texto(d, sizeof d, "%s/%s", pasta, nome);
    int w = salvar_arquivo(d, baixado, baixado_n);
    formatar_texto(estado, sizeof estado, w < 0 ? "No se pudo guardar %s" : "Bajado: %s", d);
    libera(baixado); baixado = NULL;
    refiltrar();
}

static int janela(void)
{
    filtro = aloca((size_t)MAX_PROJ * sizeof(int));
    itens = aloca((size_t)MAX_PROJ * 64);
    if (!filtro || !itens) return 1;
    jan = jan_abrir("Actividades JClic (bajar)", 780, 520);
    if (!jan) { registrar("jclicbajar: no se ha abierto la ventana"); return 1; }
    jan_estilo(jan, JAN_REDIMENSIONAVEL, 640, 420);
    memset(&busca, 0, sizeof busca);
    memset(&lst, 0, sizeof lst);
    escolher_pasta();
    copia_texto(estado, "Cargando el indice del repositorio...", sizeof estado);
    pintar();
    if (!indice_carregar(true)) com_rede_falhou = true;
    if (pasta_temporaria) {
        int n = (int)tam_texto(estado);
        formatar_texto(estado + n, sizeof estado - (size_t)n, " (sin /home persistente: se usa /tmp/jclic)");
    }
    refiltrar();
    pintar();
    registrar("jclicbajar: pronto indice=%d", n_proj);
    evento_t ev;
    bool sair = false;
    while (!sair) {
        if (esperar_evento(jan->id, &ev, -1) != 1) continue;
        do {
            if (ev.tipo == EV_FECHAR) { sair = true; break; }
            if (ev.tipo == EV_REDIMENSIONAR) { if (jan_redimensionar(jan, &ev)) pintar(); continue; }
            char antes[128];
            copia_texto(antes, busca.texto, sizeof antes);
            if (ev.tipo == EV_TECLA && ev.tecla == TECLA_ESC) { sair = true; break; }
            caixa_tratar(jan, &busca, &ev);
            if (compara_texto(antes, busca.texto)) refiltrar();
            int r = lista_tratar(jan, &lst, &ev);
            if (r == LISTA_ATIVOU && lst.sel >= 0 && lst.sel < n_filtro) {
                const proj_t *p = &proj[filtro[lst.sel]];
                if (!ja_baixado(p)) { copia_texto(estado, "Bajando...", sizeof estado); pintar(); progresso_cb = progresso_janela; baixar_projeto(p); progresso_cb = NULL; refiltrar(); }
                if (ja_baixado(p)) jogar(p);
            }
            if (botao_tratar(jan, &bt_idioma, &ev)) { idioma = (idioma + 1) % 4; refiltrar(); }
            if (botao_tratar(jan, &bt_indice, &ev)) {
                copia_texto(estado, "Actualizando el indice...", sizeof estado);
                pintar();
                progresso_cb = progresso_janela;
                indice_carregar(true);
                progresso_cb = NULL;
                refiltrar();
            }
            if (botao_tratar(jan, &bt_url, &ev)) baixar_url_manual();
            if (botao_tratar(jan, &bt_baixar, &ev) && lst.sel >= 0 && lst.sel < n_filtro) {
                copia_texto(estado, "Bajando...", sizeof estado);
                pintar();
                progresso_cb = progresso_janela;
                baixar_projeto(&proj[filtro[lst.sel]]);
                progresso_cb = NULL;
                refiltrar();
            }
            if (botao_tratar(jan, &bt_abrir, &ev) && lst.sel >= 0 && lst.sel < n_filtro) {
                const proj_t *p = &proj[filtro[lst.sel]];
                if (!ja_baixado(p)) { copia_texto(estado, "Bajando...", sizeof estado); pintar(); progresso_cb = progresso_janela; baixar_projeto(p); progresso_cb = NULL; refiltrar(); }
                if (ja_baixado(p)) jogar(p);
            }
            if (botao_tratar(jan, &bt_sair, &ev)) { sair = true; break; }
        } while (proximo_evento(jan->id, &ev, false) == 1);
        if (!sair) pintar();
    }
    jan_fechar(jan);
    return 0;
}

int principal(int argc, char **argv)
{
    bool lista = false, sem_rede = false;
    const char *buscar = "", *idi = "es", *bajar = NULL, *url_zip = NULL;
    for (int k = 1; k < argc; k++) {
        if (!compara_texto(argv[k], "--url") && k + 1 < argc) copia_texto(base_url, argv[++k], sizeof base_url);
        else if (!compara_texto(argv[k], "--lista")) lista = true;
        else if (!compara_texto(argv[k], "--buscar") && k + 1 < argc) buscar = argv[++k];
        else if (!compara_texto(argv[k], "--idioma") && k + 1 < argc) idi = argv[++k];
        else if (!compara_texto(argv[k], "--bajar") && k + 1 < argc) bajar = argv[++k];
        else if (!compara_texto(argv[k], "--url-zip") && k + 1 < argc) url_zip = argv[++k];
        else if (!compara_texto(argv[k], "--sin-red")) sem_rede = true;
        else return uso();
    }
    int n = (int)tam_texto(base_url);
    while (n > 0 && base_url[n - 1] == '/') base_url[--n] = 0;
    if (lista || bajar || url_zip || sem_rede) return modo_texto(lista, buscar, idi, bajar, url_zip, sem_rede);
    return janela();
}
