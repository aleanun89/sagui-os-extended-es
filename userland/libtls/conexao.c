// SPDX-License-Identifier: GPL-2.0-or-later
/* conexao.c - uma conexão HTTP ou HTTPS, com a mesma interface (D062).
 *
 * O curl, o wget e o Cipó leem a resposta com http_ler_cabecalho_de e
 * http_ler_corpo passando conexao_ler: para eles tanto faz se há TLS. */
#include "tls.h"

int conexao_abrir(conexao_t *c, const url_t *u, const tls_config_t *cfg)
{
    memset(c, 0, sizeof *c);
    c->fd = conectar_tcp(u->host, u->porta, 20000);
    if (c->fd < 0) {
        int r = c->fd;
        formatar_texto(c->erro, sizeof c->erro, "%s:%d: %s", u->host, u->porta, nome_erro(r));
        return r;
    }
    if (!u->https) return 0;
    tls_config_t k = *cfg;
    k.host = u->host;
    c->tls = tls_novo(&k, c->fd);
    if (!c->tls) {
        copia_texto(c->erro, "sin memoria para TLS", sizeof c->erro);
        return TLS_ERRO_MEMORIA;
    }
    int r = tls_conectar(c->tls);
    if (r < 0) copia_texto(c->erro, tls_erro(c->tls), sizeof c->erro);
    return r;
}

int conexao_ler(void *ctx, void *buf, unsigned n)
{
    conexao_t *c = ctx;
    return c->tls ? tls_ler(c->tls, buf, n) : ler(c->fd, buf, n);
}

int conexao_escrever(conexao_t *c, const void *buf, unsigned n)
{
    if (c->tls) return tls_escrever(c->tls, buf, n);
    return escrever_tudo(c->fd, buf, n);
}

void conexao_fechar(conexao_t *c)
{
    if (c->tls) tls_fechar(c->tls);
    c->tls = NULL;
    if (c->fd >= 0) fechar(c->fd);
    c->fd = -1;
}

bool tls_config_padrao(tls_config_t *cfg, const char *cacert, char *erro, unsigned max)
{
    static almacen_t raizes;
    static bool carregado;
    memset(cfg, 0, sizeof *cfg);
    if (!carregado) {
        almacen_carregar(&raizes);
        carregado = true;
        if (cacert) {   /* --cacert: como no curl, SÓ os certificados deste PEM (pode ter vários) */
            raizes.n = 0;
            int fd = abrir(cacert, ABRIR_LER);
            if (fd < 0) { formatar_texto(erro, max, "%s: %s", cacert, nome_erro(fd)); return false; }
            static char texto[65536];
            int n = 0, k;
            while (n < (int)sizeof texto - 1 && (k = ler(fd, texto + n, sizeof texto - 1 - (unsigned)n)) > 0) n += k;
            fechar(fd);
            unsigned pos = 0;
            int achados = 0;
            while (raizes.n < RAIZES_MAX) {
                uint8_t *der = aloca((unsigned)n + 1);
                if (!der) break;
                int t = pem_proximo(texto, (unsigned)n, &pos, der, (unsigned)n);
                if (t <= 0 || x509_ler(&raizes.c[raizes.n], der, (unsigned)t) != X509_OK) { libera(der); if (t <= 0) break; continue; }
                copia_texto(raizes.arquivo[raizes.n], cacert, sizeof raizes.arquivo[0]);
                raizes.n++;
                achados++;
            }
            if (!achados) { formatar_texto(erro, max, "%s: no contiene ningún certificado PEM", cacert); return false; }
        }
    }
    cfg->raizes = &raizes;
    cfg->alpn = "http/1.1";
    return true;
}
