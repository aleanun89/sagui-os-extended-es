/* web_compat.c - o que o motor da web precisa do browser (melhoria 081).
 *
 * O motor (motor.c, js.c, ...) veio do repositorio robson-apk/sagui-os, ramo
 * "cipo-web" (ver userland/web/ORIGEM.md). Ele foi escrito contra um sagui.h
 * diferente do nosso. O que falta esta aqui:
 *
 *   - web_imagem_decodificar: decodifica PNG ou JPEG em pixels ARGB (o motor
 *     usa isso para desenhar imagens num <canvas> e no SVG). A nossa imagem.h
 *     tem imagem_decodificar, mas com outra assinatura e em 0x00RRGGBB, por
 *     isso aqui se faz a traducao.
 *   - http_preparar: guarda o metodo, os cabecalhos e o corpo do proximo
 *     pedido (o JavaScript faz fetch() e POST). O browser le com
 *     http_preparado().
 *   - cookie_guardar / cookie_texto: o pote de cookies (RFC 6265) do browser.
 *     O motor usa isto para o document.cookie.
 */
#include "js.h"

/* ---- imagens: PNG e JPEG em ARGB ---- */
bool web_imagem_decodificar(const uint8_t *d, unsigned n, uint32_t **px, int *l, int *a)
{
    imagem_t img;
    if (!px || !l || !a) return false;
    *px = NULL; *l = 0; *a = 0;
    if (imagem_decodificar(d, (int)n, &img) != 0 && imagem_ler_jpeg(d, (int)n, &img) != 0)
        return false;
    uint32_t *saida = (uint32_t *)aloca((size_t)img.larg * (size_t)img.alt * sizeof(uint32_t));
    if (!saida) { imagem_liberar(&img); return false; }
    /* A nossa imagem_t usa 0x00RRGGBB; o Canvas 2D usa ARGB sem pre-multiplicar.
     * A transparencia do PNG ja vem misturada com branco ao ler, por isso o
     * alfa e sempre opaco. */
    for (int i = 0; i < img.larg * img.alt; i++)
        saida[i] = 0xFF000000u | (img.px[i] & 0x00FFFFFFu);
    *px = saida;
    *l = img.larg;
    *a = img.alt;
    imagem_liberar(&img);
    return true;
}

/* ---- o proximo pedido (fetch e POST do JavaScript) ---- */
static char   prep_metodo[16];
static char   prep_cab[1024];
static uint8_t prep_corpo[4096];
static int    prep_ncorpo = -1;   /* -1 = ainda nao foi preparado nada */

void http_preparar(const char *metodo, const char *cabecalhos, const void *corpo, int n)
{
    prep_metodo[0] = 0;
    prep_cab[0] = 0;
    prep_ncorpo = -1;
    if (metodo) copia_texto(prep_metodo, metodo, sizeof prep_metodo);
    if (cabecalhos) copia_texto(prep_cab, cabecalhos, sizeof prep_cab);
    if (corpo && n > 0) {
        int t = n;
        if (t > (int)sizeof prep_corpo) t = (int)sizeof prep_corpo;
        memcpy(prep_corpo, corpo, (size_t)t);
        prep_ncorpo = t;
    }
}

/* Le o que ficou preparado; devolve false se nao ha nada. */
bool http_preparado(char *metodo, int mmax, char *cab, int cmax, const uint8_t **corpo, int *ncorpo)
{
    if (prep_metodo[0] == 0) return false;
    if (metodo && mmax > 0) copia_texto(metodo, prep_metodo, (size_t)mmax);
    if (cab && cmax > 0) copia_texto(cab, prep_cab, (size_t)cmax);
    if (corpo) *corpo = prep_corpo;
    if (ncorpo) *ncorpo = prep_ncorpo;
    return true;
}

void http_preparar_limpar(void)
{
    prep_metodo[0] = 0;
    prep_cab[0] = 0;
    prep_ncorpo = -1;
}

/* ---- cookies (RFC 6265, o essencial) ---- */
#define COOKIE_MAX 64
typedef struct {
    char host[128];
    char nome[64];
    char valor[192];
} cookie_t;

static cookie_t cookies[COOKIE_MAX];
static int n_cookies;

/* O host do endereco, para saber a que potes pertence o cookie. */
static bool host_do_url(const char *url, char *saida, int max)
{
    url_t u;
    if (!url_separar(url, &u)) return false;
    copia_texto(saida, u.host, (size_t)max);
    return true;
}

void cookie_guardar(const char *url, const char *sc, bool de_js)
{
    (void)de_js;   /* por agora nao distinguimos os cookies do JavaScript */
    if (!url || !sc || !*sc) return;
    char host[128];
    if (!host_do_url(url, host, sizeof host)) return;
    /* "nome=valor; Path=/; HttpOnly" -> "nome=valor" */
    int i = 0;
    while (sc[i] && sc[i] != '=' && sc[i] != ';') i++;
    if (!sc[i] || sc[i] != '=') return;
    char nome[64];
    int j = 0;
    for (int k = 0; k < i && j < (int)sizeof nome - 1; k++) nome[j++] = sc[k];
    nome[j] = 0;
    i++;
    char valor[192];
    j = 0;
    while (sc[i] && sc[i] != ';' && j < (int)sizeof valor - 1) valor[j++] = sc[i++];
    valor[j] = 0;
    if (!nome[0]) return;
    /* Se ja existe um cookie com esse nome nesse host, substitui. */
    for (int k = 0; k < n_cookies; k++) {
        if (compara_texto(cookies[k].host, host) == 0 && compara_texto(cookies[k].nome, nome) == 0) {
            copia_texto(cookies[k].valor, valor, sizeof cookies[k].valor);
            return;
        }
    }
    if (n_cookies >= COOKIE_MAX) return;
    copia_texto(cookies[n_cookies].host, host, sizeof cookies[0].host);
    copia_texto(cookies[n_cookies].nome, nome, sizeof cookies[0].nome);
    copia_texto(cookies[n_cookies].valor, valor, sizeof cookies[0].valor);
    n_cookies++;
}

int cookie_texto(const char *url, char *o, int max, bool de_js)
{
    (void)de_js;
    if (!o || max <= 0) return 0;
    o[0] = 0;
    if (!url) return 0;
    char host[128];
    if (!host_do_url(url, host, sizeof host)) return 0;
    int total = 0;
    for (int k = 0; k < n_cookies; k++) {
        if (compara_texto(cookies[k].host, host) != 0) continue;
        int n = (int)tam_texto(cookies[k].nome) + (int)tam_texto(cookies[k].valor) + 3;
        if (total + n >= max) break;
        if (total > 0) { o[total++] = ';'; o[total++] = ' '; }
        total += (int)tam_texto(cookies[k].nome);
        copia_texto(o + total, cookies[k].nome, (size_t)(max - total));
        total += (int)tam_texto(cookies[k].nome);
        o[total++] = '=';
        copia_texto(o + total, cookies[k].valor, (size_t)(max - total));
        total += (int)tam_texto(cookies[k].valor);
        o[total] = 0;
    }
    return total;
}

void cookie_limpar(void)
{
    n_cookies = 0;
}

/* ---- descompressao para as fontes da web ----
 * A WOFF usa zlib (o fonte.c tira a cabecalha de 2 bytes antes de chamar) e a
 * WOFF2 usa Brotli (o brotli.c, tambem do mesmo repositorio). O nosso inflate
 * (imagem.c) e um deflate cru, que e o que o zlib traz dentro. */
bool inflar_zlib(const uint8_t *p, unsigned n, uint8_t *s, unsigned max)
{
    return inflar(p, (int)n, s, (int)max) > 0;
}
