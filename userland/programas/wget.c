// SPDX-License-Identifier: GPL-2.0-or-later
/* wget - descarga un archivo por HTTP o HTTPS y lo graba con el nombre de la URL (D062).
 *
 *   wget [-O archivo] [-q] [-k] [--cacert arq.pem] http[s]://host[:puerto]/camino
 *
 * Sigue redirecciones (hasta 10). Mientras descarga, muestra una barra con el
 * porcentaje (si el servidor dijo el tamaño) o los KB recibidos. Un error
 * HTTP (404...) no crea el archivo. */
#include "tls.h"

static char bloco[16384];
static http_resposta_t resp;

static void nome_do_arquivo(const url_t *u, char *s, int max)
{
    char c[512];
    copia_texto(c, u->caminho, sizeof c);
    char *q = acha_char(c, '?');
    if (q) *q = 0;
    char *b = acha_ultimo_char(c, '/');
    copia_texto(s, b && b[1] ? b + 1 : "index.html", (size_t)max);
}

/* "\r 45% [=========>          ]  123 KB" */
static void barra(unsigned feito, int total, bool fim)
{
    char s[80];
    if (total > 0) {
        unsigned pct = (unsigned)total >= 100 ? feito / ((unsigned)total / 100) : feito * 100 / (unsigned)total;
        if (pct > 100) pct = 100;
        char b[31];
        unsigned cheio = pct * 30 / 100;
        for (unsigned i = 0; i < 30; i++) b[i] = i < cheio ? '=' : i == cheio ? '>' : ' ';
        b[30] = 0;
        formatar_texto(s, sizeof s, "\r%3u%% [%s] %6u KB", pct, b, feito / 1024);
    } else formatar_texto(s, sizeof s, "\r%8u KB", feito / 1024);
    escrever_texto(ERROS, s);
    if (fim) escrever_texto(ERROS, "\n");
}

static int uso(void)
{
    imprimir_em(ERROS, "uso: wget [-O archivo] [-q] [-k] [--cacert archivo.pem] http[s]://host[:puerto]/ruta\n");
    return 2;
}

int principal(int argc, char **argv)
{
    bool silencio = false, inseguro = false;
    const char *arquivo = NULL, *endereco = NULL, *cacert = NULL;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if ((compara_texto(a, "-O") == 0 || compara_texto(a, "-o") == 0) && i + 1 < argc) arquivo = argv[++i];
        else if (compara_texto(a, "--cacert") == 0 && i + 1 < argc) cacert = argv[++i];
        else if (compara_texto(a, "-q") == 0) silencio = true;
        else if (compara_texto(a, "-k") == 0 || compara_texto(a, "--inseguro") == 0 ||
                 compara_texto(a, "--no-check-certificate") == 0) inseguro = true;
        else if (a[0] == '-') return uso();
        else endereco = a;
    }
    if (!endereco) return uso();
    static char url[600];
    copia_texto(url, endereco, sizeof url);
    url_t u;
    tls_config_t cfg;
    memset(&cfg, 0, sizeof cfg);
    bool cfg_pronta = false;
    static conexao_t con;
    for (int redirecoes = 0;; redirecoes++) {
        if (!url_separar(url, &u)) { imprimir_em(ERROS, "wget: dirección no válida: %s\n", url); return 3; }
        if (u.https && !cfg_pronta) {
            char erro[200];
            if (!tls_config_padrao(&cfg, cacert, erro, sizeof erro)) { imprimir_em(ERROS, "wget: %s\n", erro); return 77; }
            cfg.inseguro = inseguro;
            cfg_pronta = true;
        }
        if (!silencio) imprimir_em(ERROS, "wget: conectando a %s:%d...\n", u.host, u.porta);
        int r = conexao_abrir(&con, &u, &cfg);
        if (r < 0) {
            if (r == TLS_ERRO_CERT)
                imprimir_em(ERROS, "wget: %s: %s.\n  La conexión no es segura; usa -k (--inseguro) para seguir de todos modos.\n",
                            u.host, con.erro);
            else imprimir_em(ERROS, "wget: %s%s\n", con.tls ? "TLS: " : "", con.erro);
            bool de_tls = con.tls != NULL;
            conexao_fechar(&con);
            return r == TLS_ERRO_CERT ? 5 : de_tls ? 5 : 4;
        }
        if (con.tls && !silencio)
            imprimir_em(ERROS, "wget: %s, %s%s\n", tls_versao_texto(con.tls), tls_suite_texto(con.tls),
                        tls_validacao(con.tls) == X509_OK ? ", certificado válido" : ", certificado NO verificado (-k)");
        static char pedido[1024];
        int np = http_montar_pedido(&u, "GET", NULL, pedido, sizeof pedido);
        if (conexao_escrever(&con, pedido, (unsigned)np) < 0) {
            imprimir_em(ERROS, "wget: no se pudo enviar el pedido\n");
            conexao_fechar(&con);
            return 4;
        }
        r = http_ler_cabecalho_de(&resp, conexao_ler, &con);
        if (r < 0) {
            imprimir_em(ERROS, "wget: respuesta no válida del servidor (%s)\n",
                        r == ERRO_FORMATO ? "no es HTTP" : con.tls ? tls_erro(con.tls) : nome_erro(r));
            conexao_fechar(&con);
            return 8;
        }
        int c = resp.codigo;
        static char destino[600];
        if ((c == 301 || c == 302 || c == 303 || c == 307 || c == 308) &&
            http_campo(resp.cabecalho, "Location", destino, sizeof destino)) {
            conexao_fechar(&con);
            if (redirecoes >= 10) { imprimir_em(ERROS, "wget: demasiadas redirecciones\n"); return 8; }
            static char nova[600];
            if (!url_juntar(url, destino, nova, sizeof nova)) { imprimir_em(ERROS, "wget: redirección no válida\n"); return 8; }
            if (!silencio) imprimir_em(ERROS, "wget: redirigido (%d) a %s\n", c, nova);
            copia_texto(url, nova, sizeof url);
            continue;
        }
        if (c >= 400) {
            char *fim = acha_char(resp.cabecalho, '\r');
            if (fim) *fim = 0;
            imprimir_em(ERROS, "wget: el servidor respondió %s\n", acha_char(resp.cabecalho, ' ') + 1);
            conexao_fechar(&con);
            return 8;
        }
        static char nome[256];
        if (!arquivo) { nome_do_arquivo(&u, nome, sizeof nome); arquivo = nome; }
        int saida = abrir(arquivo, ABRIR_CRIAR | ABRIR_TRUNCAR | ABRIR_ESCREVER);
        if (saida < 0) { imprimir_em(ERROS, "wget: %s: %s\n", arquivo, nome_erro(saida)); conexao_fechar(&con); return 3; }
        int total = resp.pedacos ? -1 : resp.restante;
        unsigned feito = 0, inicio = tempo_ms(), ultimo = 0;
        int n;
        while ((n = http_ler_corpo(&resp, bloco, sizeof bloco)) > 0) {
            if (escrever_tudo(saida, bloco, (unsigned)n) < 0) {
                imprimir_em(ERROS, "\nwget: %s: no se pudo escribir (¿disco lleno?)\n", arquivo);
                n = ERRO_SEM_MEMORIA;
                break;
            }
            feito += (unsigned)n;
            unsigned agora = tempo_ms();
            if (!silencio && agora - ultimo >= 200) { barra(feito, total, false); ultimo = agora; }
        }
        if (n < 0 && con.tls) imprimir_em(ERROS, "\nwget: TLS: %s\n", tls_erro(con.tls));
        conexao_fechar(&con);
        fechar(saida);
        unsigned ms = tempo_ms() - inicio;
        if (!silencio) barra(feito, total < 0 ? -1 : (int)feito, true);
        if (n < 0) { imprimir_em(ERROS, "wget: la transferencia se cortó: %s (%u bytes en %s)\n", nome_erro(n), feito, arquivo); return 4; }
        if (!silencio) {
            char vel[24] = "";
            if (ms >= 10) formatar_texto(vel, sizeof vel, ", %u KB/s", (unsigned)((feito / ms) * 1000u / 1024u));
            imprimir_em(ERROS, "wget: %u bytes guardados en %s (HTTP %d, %u ms%s)\n", feito, arquivo, c, ms, vel);
        }
        return 0;
    }
}
