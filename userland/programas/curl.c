// SPDX-License-Identifier: GPL-2.0-or-later
/* curl - descarga por HTTP/1.1 o HTTPS (D046, D062).
 *
 *   curl [-o archivo] [-O] [-I] [-v] [-L] [-s] [-f] [-k] [--cacert arq.pem] http[s]://host[:puerto]/camino
 *
 * Sin -o, el cuerpo va a la pantalla; si es texto, el UTF-8 de afuera se vuelve el
 * Latin-1 de SaguiOS. En archivo, va byte a byte como vino. Con -v, también
 * muestra el pedido, el encabezado de la respuesta, la conexión TLS (versión, suite,
 * certificado) y, al final, el tamaño y el CRC-32 del cuerpo (el mismo del zlib),
 * para verificar una descarga. -k (--inseguro) acepta cualquier certificado;
 * --cacert usa solo las raíces del archivo dado. El wget es un programa aparte. */
#include "tls.h"

static char corpo[8192];
static http_resposta_t resp;

static uint32_t crc_tabela[256];
static uint32_t crc32(uint32_t crc, const uint8_t *p, unsigned n)
{
    if (!crc_tabela[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            crc_tabela[i] = c;
        }
    crc = ~crc;
    while (n--) crc = crc_tabela[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

/* Escribe el encabezado línea a línea, con un prefijo ("< ") y sin los \r. */
static void mostrar_linhas(int fd, const char *prefixo, const char *t)
{
    while (*t) {
        const char *fim = acha_char(t, '\n');
        int n = fim ? (int)(fim - t) : (int)tam_texto(t);
        int k = n;
        if (k && t[k - 1] == '\r') k--;
        if (k || fim) {
            escrever_texto(fd, prefixo);
            escrever_tudo(fd, t, (unsigned)k);
            escrever_texto(fd, "\n");
        }
        t += n + (fim ? 1 : 0);
    }
}

static void nome_do_arquivo(const url_t *u, char *s, int max)
{
    char c[512];
    copia_texto(c, u->caminho, sizeof c);
    char *q = acha_char(c, '?');
    if (q) *q = 0;
    char *b = acha_ultimo_char(c, '/');
    copia_texto(s, b && b[1] ? b + 1 : "index.html", (size_t)max);
}

/* -v: la conexión TLS y el certificado del servidor */
static void mostrar_tls(tls_t *t, const tls_config_t *cfg)
{
    if (tls_suite_texto(t)[0] != '?')
        imprimir_em(ERROS, "* Conexión TLS: %s, %s, %s (%u ms)\n", tls_versao_texto(t), tls_suite_texto(t),
                    tls_grupo_texto(t), tls_ms_handshake(t));
    if (tls_alpn(t)[0]) imprimir_em(ERROS, "* ALPN: %s\n", tls_alpn(t));
    const cert_t *c;
    int n = tls_cadeia(t, &c);
    if (n > 0) {
        imprimir_em(ERROS, "* Certificado del servidor:\n");
        x509_mostrar(ERROS, &c[0], "*   ");
        if (n > 1) imprimir_em(ERROS, "* La cadena trae %d certificado%s más\n", n - 1, n > 2 ? "s" : "");
    }
    int v = tls_validacao(t), r = tls_raiz(t);
    if (v == X509_OK && r >= 0) {
        char nome[128];
        x509_nome_curto(cfg->raizes->c[r].sujeito, nome, sizeof nome);
        imprimir_em(ERROS, "* Verificación: certificado válido (raíz: %s)\n", nome);
    } else if (v != X509_OK) {
        imprimir_em(ERROS, "* Verificación: NO VÁLIDO (%s)%s\n", x509_erro_texto(v), cfg->inseguro ? ", aceptado por -k" : "");
    }
}

static int uso(void)
{
    imprimir_em(ERROS, "uso: curl [-o archivo] [-O] [-I] [-v] [-L] [-s] [-f] [-k] [--cacert archivo.pem] http[s]://host[:puerto]/ruta\n");
    return 2;
}

int principal(int argc, char **argv)
{
    const char *prog = "curl";
    bool so_cabecalho = false, verboso = false, seguir = false, silencio = false, falhar = false, nome_remoto = false;
    bool inseguro = false;
    const char *arquivo = NULL, *endereco = NULL, *cacert = NULL;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (compara_texto(a, "-o") == 0 && i + 1 < argc) arquivo = argv[++i];
        else if (compara_texto(a, "--cacert") == 0 && i + 1 < argc) cacert = argv[++i];
        else if (compara_texto(a, "-O") == 0) nome_remoto = true;
        else if (compara_texto(a, "-I") == 0 || compara_texto(a, "--head") == 0) so_cabecalho = true;
        else if (compara_texto(a, "-v") == 0) verboso = true;
        else if (compara_texto(a, "-L") == 0) seguir = true;
        else if (compara_texto(a, "-s") == 0) silencio = true;
        else if (compara_texto(a, "-f") == 0) falhar = true;
        else if (compara_texto(a, "-k") == 0 || compara_texto(a, "--inseguro") == 0 || compara_texto(a, "--insecure") == 0)
            inseguro = true;
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
    for (int redirecoes = 0;; redirecoes++) {
        if (!url_separar(url, &u)) { imprimir_em(ERROS, "%s: dirección no válida: %s\n", prog, url); return 3; }
        if (u.https && !cfg_pronta) {
            char erro[200];
            if (!tls_config_padrao(&cfg, cacert, erro, sizeof erro)) { imprimir_em(ERROS, "%s: %s\n", prog, erro); return 77; }
            cfg.inseguro = inseguro;
            cfg_pronta = true;
        }
        unsigned inicio = tempo_ms();
        if (verboso) imprimir_em(ERROS, "* Conectando a %s puerto %d...\n", u.host, u.porta);
        static conexao_t con;
        int r = conexao_abrir(&con, &u, &cfg);
        if (r < 0) {
            if (r == TLS_ERRO_CERT) {
                if (verboso) mostrar_tls(con.tls, &cfg);
                imprimir_em(ERROS, "%s: %s: %s.\n  La conexión no es segura; usa -k (--inseguro) para seguir de todos modos.\n",
                            prog, u.host, con.erro);
                conexao_fechar(&con);
                return 60;
            }
            bool de_tls = con.tls != NULL;
            imprimir_em(ERROS, "%s: %s%s\n", prog, de_tls ? "TLS: " : "", con.erro);
            conexao_fechar(&con);
            return de_tls ? 35 : 7;
        }
        if (verboso) {
            imprimir_em(ERROS, "* Conectado\n");
            if (con.tls) mostrar_tls(con.tls, &cfg);
        }
        static char pedido[1024];
        int np = http_montar_pedido(&u, so_cabecalho ? "HEAD" : "GET", NULL, pedido, sizeof pedido);
        if (verboso) mostrar_linhas(ERROS, "> ", pedido);
        if (conexao_escrever(&con, pedido, (unsigned)np) < 0) {
            imprimir_em(ERROS, "%s: no se pudo enviar el pedido\n", prog);
            conexao_fechar(&con);
            return 55;
        }
        r = http_ler_cabecalho_de(&resp, conexao_ler, &con);
        if (r < 0) {
            imprimir_em(ERROS, "%s: respuesta no válida del servidor (%s)\n", prog,
                        r == ERRO_FORMATO ? "no es HTTP" : con.tls ? tls_erro(con.tls) : nome_erro(r));
            conexao_fechar(&con);
            return 8;
        }
        if (so_cabecalho) resp.acabou = true;
        if (verboso) { mostrar_linhas(ERROS, "< ", resp.cabecalho); imprimir_em(ERROS, "<\n"); }
        static char destino[600];
        int c = resp.codigo;
        if (seguir && (c == 301 || c == 302 || c == 303 || c == 307 || c == 308) &&
            http_campo(resp.cabecalho, "Location", destino, sizeof destino)) {
            conexao_fechar(&con);
            if (redirecoes >= 10) { imprimir_em(ERROS, "%s: demasiadas redirecciones\n", prog); return 47; }
            static char nova[600];
            if (!url_juntar(url, destino, nova, sizeof nova)) { imprimir_em(ERROS, "%s: redirección no válida\n", prog); return 3; }
            if (!silencio) imprimir_em(ERROS, "%s: redirigido (%d) a %s\n", prog, c, nova);
            copia_texto(url, nova, sizeof url);
            continue;
        }
        if (so_cabecalho) {
            mostrar_linhas(SAIDA, "", resp.cabecalho);
            conexao_fechar(&con);
            return c >= 400 && falhar ? 22 : 0;
        }
        if (falhar && c >= 400) {
            char *fim = acha_char(resp.cabecalho, '\r');
            if (fim) *fim = 0;
            imprimir_em(ERROS, "%s: el servidor respondió %s\n", prog, acha_char(resp.cabecalho, ' ') + 1);
            conexao_fechar(&con);
            return 22;
        }

        /* Destino del cuerpo */
        static char nome[256];
        if (nome_remoto && !arquivo) { nome_do_arquivo(&u, nome, sizeof nome); arquivo = nome; }
        int saida = SAIDA;
        if (arquivo) {
            saida = abrir(arquivo, ABRIR_CRIAR | ABRIR_TRUNCAR | ABRIR_ESCREVER);
            if (saida < 0) { imprimir_em(ERROS, "%s: %s: %s\n", prog, arquivo, nome_erro(saida)); conexao_fechar(&con); return 23; }
        }
        char tipo[80] = "";
        http_campo(resp.cabecalho, "Content-Type", tipo, sizeof tipo);
        bool texto = !tipo[0] || comeca_com(tipo, "text/") || comeca_com(tipo, "application/json") ||
                     comeca_com(tipo, "application/xml") || comeca_com(tipo, "application/javascript");
        bool converter = saida == SAIDA && texto;
        unsigned total = 0;
        uint32_t crc = 0;
        size_t sobra = 0;   /* bytes de un carácter UTF-8 cortado entre dos lecturas */
        int n;
        while ((n = http_ler_corpo(&resp, corpo + sobra, (int)(sizeof corpo - sobra))) > 0) {
            crc = crc32(crc, (const uint8_t *)corpo + sobra, (unsigned)n);
            total += (unsigned)n;
            size_t k = (size_t)n + sobra, s = 0;
            if (converter) k = utf8_para_latin1(corpo, k, &s);
            if (escrever_tudo(saida, corpo, (unsigned)k) < 0) { n = ERRO_PIPE_QUEBRADO; break; }
            if (s) memmove(corpo, corpo + (size_t)n + sobra - s, s);
            sobra = s;
        }
        if (n < 0 && n != ERRO_PIPE_QUEBRADO && con.tls) imprimir_em(ERROS, "%s: TLS: %s\n", prog, tls_erro(con.tls));
        conexao_fechar(&con);
        if (saida != SAIDA) fechar(saida);
        if (n < 0) imprimir_em(ERROS, "%s: la transferencia se cortó: %s\n", prog, nome_erro(n));
        unsigned ms = tempo_ms() - inicio;
        if (verboso) imprimir_em(ERROS, "* %u bytes en %u ms, CRC32 %08x\n", total, ms, crc);
        if (arquivo && !silencio) {
            char vel[24] = "";
            if (ms >= 10) formatar_texto(vel, sizeof vel, ", %u KB/s", (unsigned)((total / ms) * 1000u / 1024u));
            imprimir_em(ERROS, "%s: %u bytes guardados en %s (HTTP %d, %u ms%s)\n", prog, total, arquivo, c, ms, vel);
        }
        if (c >= 400 && !silencio) {
            char *fim = acha_char(resp.cabecalho, '\r');
            if (fim) *fim = 0;
            imprimir_em(ERROS, "%s: el servidor respondió %s\n", prog, acha_char(resp.cabecalho, ' ') + 1);
        }
        if (n < 0) return 18;
        return c >= 400 ? 22 : 0;
    }
}
