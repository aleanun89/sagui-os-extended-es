// SPDX-License-Identifier: GPL-2.0-or-later
/* fetch_http.c - el fetcher de http: y https: de NetSurf en el SaguiOS
 * (D184), sobre el cliente HTTP de la libsagui (http.c) y su TLS
 * (userland/libtls, el de curl, wget y Cipó).
 *
 * NetSurf trae un fetcher sobre libcurl (content/fetchers/curl.c) y el
 * SaguiOS no tiene libcurl. Este lo sustituye con la misma interfaz
 * (struct fetcher_operation_table) y los mismos mensajes que manda el de
 * curl: las cabeceras una a una (FETCH_HEADER), los datos (FETCH_DATA) y el
 * final (FETCH_FINISHED), o FETCH_REDIRECT / FETCH_NOTMODIFIED / FETCH_ERROR.
 * Las cookies van y vienen por urldb, como en el de curl.
 *
 * Limitación (carencia del sistema): no hay E/S sin bloqueo con varios
 * descriptores (select/poll) ni hilos que NetSurf sepa usar, así que cada
 * descarga se hace entera, bloqueando, dentro de poll(): mientras baja una
 * página la ventana no responde. Tampoco hay gzip en el transporte, ni
 * conexiones persistentes, ni autenticación, ni proxy, ni formularios
 * multipart.
 */
#include "sagui.h"
#include "tls.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <libwapcaplet/libwapcaplet.h>

#include "utils/nsurl.h"
#include "utils/ring.h"
#include "utils/useragent.h"
#include "utils/messages.h"
#include "content/fetch.h"
#include "content/fetchers.h"
#include "content/urldb.h"

typedef struct ns_http {
    struct ns_http *r_next, *r_prev;   /* para RING_* */
    struct fetch *pai;
    nsurl *url;
    bool only_2xx;
    char *post;                        /* cuerpo urlencoded (o NULL) */
    char *cabecalhos;                  /* los de NetSurf, cada uno con \r\n */
    bool comecou, abortado, travado;
} ns_http_t;

static ns_http_t *anel;
static tls_config_t cfg_tls;
static bool cfg_tls_pronta;
static http_resposta_t resposta;      /* una descarga cada vez */

static void enviar(ns_http_t *c, const fetch_msg *msg)
{
    c->travado = true;
    fetch_send_callback(msg, c->pai);
    c->travado = false;
}

static void enviar_erro(ns_http_t *c, const char *texto)
{
    fetch_msg msg;
    msg.type = FETCH_ERROR;
    msg.data.error = texto;
    enviar(c, &msg);
}

static bool http_initialise(lwc_string *esquema) { (void)esquema; return true; }
static void http_finalise(lwc_string *esquema) { (void)esquema; }
static bool http_acceptable(const nsurl *url) { (void)url; return true; }

static void *http_setup(struct fetch *pai, nsurl *url, bool only_2xx, bool downgrade_tls,
                        const char *post_urlenc, const struct fetch_multipart_data *post_multipart,
                        const char **headers)
{
    (void)downgrade_tls;
    (void)post_multipart;
    ns_http_t *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->pai = pai;
    c->url = nsurl_ref(url);
    c->only_2xx = only_2xx;
    if (post_urlenc) c->post = strdup(post_urlenc);
    size_t n = 1;
    for (int i = 0; headers && headers[i]; i++) n += strlen(headers[i]) + 2;
    c->cabecalhos = malloc(n);
    if (c->cabecalhos) {
        c->cabecalhos[0] = 0;
        for (int i = 0; headers && headers[i]; i++) {
            /* El transporte comprimido no lo hacemos: fuera Accept-Encoding. */
            if (strncasecmp(headers[i], "Accept-Encoding:", 16) == 0) continue;
            strcat(c->cabecalhos, headers[i]);
            strcat(c->cabecalhos, "\r\n");
        }
    }
    RING_INSERT(anel, c);
    return c;
}

static bool http_start(void *ctx) { ((ns_http_t *)ctx)->comecou = true; return true; }
static void http_abort(void *ctx) { ((ns_http_t *)ctx)->abortado = true; }

static void http_free(void *ctx)
{
    ns_http_t *c = ctx;
    nsurl_unref(c->url);
    free(c->post);
    free(c->cabecalhos);
    free(c);
}

/* Las cabeceras de la respuesta, línea a línea, a NetSurf (sin la línea de
 * estado). Set-Cookie va además a urldb. Devuelve false si abortaron. */
static bool mandar_cabecalhos(ns_http_t *c, const char *t)
{
    const char *linha = strchr(t, '\n');   /* la primera es "HTTP/1.1 200 OK" */
    while (linha && *++linha) {
        const char *fim = strchr(linha, '\n');
        size_t n = fim ? (size_t)(fim - linha) : strlen(linha);
        size_t k = n;
        if (k && linha[k - 1] == '\r') k--;
        if (k) {
            char tmp[1024];
            if (k >= sizeof tmp) k = sizeof tmp - 1;
            memcpy(tmp, linha, k);
            tmp[k] = 0;
            if (strncasecmp(tmp, "Set-Cookie:", 11) == 0) fetch_set_cookie(c->pai, tmp + 11);
            fetch_msg msg;
            msg.type = FETCH_HEADER;
            msg.data.header_or_data.buf = (const uint8_t *)tmp;
            msg.data.header_or_data.len = k;
            enviar(c, &msg);
            if (c->abortado) return false;
        }
        linha = fim;
    }
    return true;
}

static void descarregar(ns_http_t *c)
{
    static char texto_erro[256];
    static conexao_t con;
    url_t u;
    /* Sin el fragmento (#...): no va al servidor. */
    char limpa[1100];
    strlcpy(limpa, nsurl_access(c->url), sizeof limpa);
    char *frag = strchr(limpa, '#');
    if (frag) *frag = 0;
    if (!url_separar(limpa, &u)) { enviar_erro(c, "Dirección no válida"); return; }
    if (u.https && !cfg_tls_pronta) {
        char erro[200];
        if (!tls_config_padrao(&cfg_tls, NULL, erro, sizeof erro)) {
            snprintf(texto_erro, sizeof texto_erro, "TLS: %s", erro);
            enviar_erro(c, texto_erro);
            return;
        }
        cfg_tls_pronta = true;
    }
    registrar("[NETSURF] http: %s\n", limpa);
    int r = conexao_abrir(&con, &u, &cfg_tls);
    if (r < 0) {
        snprintf(texto_erro, sizeof texto_erro, "%s%s", con.tls ? "TLS: " : "", con.erro);
        conexao_fechar(&con);
        registrar("[NETSURF] http: error %s\n", texto_erro);
        enviar_erro(c, texto_erro);
        return;
    }
    /* Las cabeceras de más: las de NetSurf, el agente, las cookies y el POST. */
    char *cookie = urldb_get_cookie(c->url, true);
    size_t n_extra = (c->cabecalhos ? strlen(c->cabecalhos) : 0) + (cookie ? strlen(cookie) : 0) + 256;
    char *extra = malloc(n_extra);
    char *pedido = malloc(n_extra + 1200);
    if (!extra || !pedido) {
        free(extra); free(pedido); free(cookie);
        conexao_fechar(&con);
        enviar_erro(c, "Sin memoria");
        return;
    }
    snprintf(extra, n_extra, "User-Agent: %s\r\n%s%s%s%s", user_agent_string(),
             c->cabecalhos ? c->cabecalhos : "",
             cookie ? "Cookie: " : "", cookie ? cookie : "", cookie ? "\r\n" : "");
    if (c->post) {
        size_t l = strlen(extra);
        snprintf(extra + l, n_extra - l, "Content-Type: application/x-www-form-urlencoded\r\n"
                 "Content-Length: %u\r\n", (unsigned)strlen(c->post));
    }
    free(cookie);
    int np = http_montar_pedido(&u, c->post ? "POST" : "GET", extra, pedido, (int)(n_extra + 1200));
    free(extra);
    bool ok = np > 0 && conexao_escrever(&con, pedido, (unsigned)np) >= 0;
    free(pedido);
    if (ok && c->post) ok = conexao_escrever(&con, c->post, (unsigned)strlen(c->post)) >= 0;
    if (!ok) { conexao_fechar(&con); enviar_erro(c, "No se pudo enviar el pedido"); return; }

    r = http_ler_cabecalho_de(&resposta, conexao_ler, &con);
    if (r < 0) {
        conexao_fechar(&con);
        enviar_erro(c, "Respuesta no válida del servidor");
        return;
    }
    registrar("[NETSURF] http: respuesta %d\n", resposta.codigo);
    fetch_set_http_code(c->pai, resposta.codigo);
    if (!mandar_cabecalhos(c, resposta.cabecalho)) { conexao_fechar(&con); return; }

    fetch_msg msg;
    int codigo = resposta.codigo;
    static char destino[1024];
    if (codigo == 304 && !c->post) {
        msg.type = FETCH_NOTMODIFIED;
        enviar(c, &msg);
        conexao_fechar(&con);
        return;
    }
    if (codigo >= 300 && codigo < 400 && http_campo(resposta.cabecalho, "Location", destino, sizeof destino)) {
        msg.type = FETCH_REDIRECT;
        msg.data.redirect = destino;
        enviar(c, &msg);
        conexao_fechar(&con);
        return;
    }
    if (c->only_2xx && (codigo < 200 || codigo > 299)) {
        conexao_fechar(&con);
        enviar_erro(c, messages_get("Not2xx"));
        return;
    }
    static uint8_t buf[8192];
    int lidos;
    while (!c->abortado && (lidos = http_ler_corpo(&resposta, buf, sizeof buf)) > 0) {
        msg.type = FETCH_DATA;
        msg.data.header_or_data.buf = buf;
        msg.data.header_or_data.len = (size_t)lidos;
        enviar(c, &msg);
    }
    conexao_fechar(&con);
    if (c->abortado) return;
    msg.type = FETCH_FINISHED;
    enviar(c, &msg);
}

static void http_poll(lwc_string *esquema)
{
    (void)esquema;
    ns_http_t *c, *guardados = NULL;
    /* Una descarga por llamada: entre una y otra NetSurf redibuja. */
    bool feita = false;
    while (anel) {
        c = anel;
        RING_REMOVE(anel, c);
        if (c->travado || !c->comecou || (feita && !c->abortado)) {
            RING_INSERT(guardados, c);
            continue;
        }
        if (!c->abortado) {
            descarregar(c);
            feita = true;
        }
        fetch_remove_from_queues(c->pai);
        fetch_free(c->pai);
    }
    anel = guardados;
}

void ns_sagui_registrar_http(void)
{
    static const struct fetcher_operation_table ops = {
        .initialise = http_initialise,
        .acceptable = http_acceptable,
        .setup = http_setup,
        .start = http_start,
        .abort = http_abort,
        .free = http_free,
        .poll = http_poll,
        .finalise = http_finalise,
    };
    lwc_string *http, *https;
    if (lwc_intern_string("http", 4, &http) == lwc_error_ok) fetcher_add(http, &ops);
    if (lwc_intern_string("https", 5, &https) == lwc_error_ok) fetcher_add(https, &ops);
}
