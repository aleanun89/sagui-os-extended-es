// SPDX-License-Identifier: GPL-2.0-or-later
/* certificados - gestiona el almacén de raíces del TLS (D063).
 *
 *   certificados [lista]                 lista sujeto, emisor, caducidad y huella SHA-256
 *   certificados ver ARCHIVO|N           detalles de un archivo PEM/DER o de la raíz N
 *   certificados añadir ARCHIVO.pem      confía en los certificados del archivo (también "anadir")
 *   certificados quitar N                deja de confiar en la raíz N
 *   certificados probar HOST[:PUERTO]    conecta, muestra la cadena y el resultado de la validación
 *
 * Con un disco persistente montado en /home (D040) los cambios van a
 * /home/.certificados y sobreviven al reinicio; sin él quedan solo en el ramdisk
 * (/etc/ssl/certs) y el comando avisa. */
#include "tls.h"

static almacen_t raizes;

static int uso(void)
{
    imprimir("uso: certificados [lista]\n"
             "     certificados ver ARCHIVO|N\n"
             "     certificados añadir ARCHIVO.pem   (o \"anadir\")\n"
             "     certificados quitar N\n"
             "     certificados probar HOST[:PUERTO]\n"
             "Gestiona las raíces de confianza que usan curl, wget y Cipó para HTTPS.\n");
    return 2;
}

static void aviso_persistencia(void)
{
    if (almacen_persistente()) imprimir("Los cambios se guardan en %s (disco persistente).\n", RAIZES_PERSISTENTE);
    else imprimir("Aviso: no hay un disco persistente montado en /home: el cambio solo dura hasta apagar.\n");
}

static int listar_raizes(void)
{
    if (!raizes.n) { imprimir("No hay certificados raíz en %s.\n", RAIZES_PASTA); return 1; }
    imprimir("%d certificados raíz de confianza:\n", raizes.n);
    for (int i = 0; i < raizes.n; i++) {
        const cert_t *c = &raizes.c[i];
        char suj[80], emi[80], cad[24], h[100];
        x509_nome_curto(c->sujeito, suj, sizeof suj);
        x509_nome_curto(c->emissor, emi, sizeof emi);
        x509_data_texto(c->depois, cad, sizeof cad);
        cad[10] = 0;   /* solo la fecha */
        x509_huella(c, h, sizeof h);
        bool caducado = data_agora() > c->depois;
        imprimir("%3d  %-40s caduca %s%s\n", i + 1, suj, cad, caducado ? " (CADUCADO)" : "");
        if (compara_texto(suj, emi) != 0) imprimir("     emisor: %s\n", emi);
        imprimir("     SHA-256 %s\n", h);
    }
    if (almacen_persistente()) imprimir("Raíces del sistema en %s; las añadidas, en %s.\n", RAIZES_PASTA, RAIZES_PERSISTENTE);
    else imprimir("Raíces en %s (sin disco persistente en /home: lo añadido se pierde al apagar).\n", RAIZES_PASTA);
    return 0;
}

/* Lee un archivo PEM (o DER) y devuelve los certificados en "der" (heap). */
static int ler_certs(const char *caminho, cert_t *certs, int max, uint8_t **der_saida)
{
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) { imprimir_em(ERROS, "certificados: %s: %s\n", caminho, nome_erro(fd)); return -1; }
    static char texto[131072];
    int n = 0, k;
    while (n < (int)sizeof texto - 1 && (k = ler(fd, texto + n, sizeof texto - 1 - (unsigned)n)) > 0) n += k;
    fechar(fd);
    texto[n] = 0;
    uint8_t *der = aloca((unsigned)n + 1);
    if (!der) return -1;
    *der_saida = der;
    int qtd = 0;
    unsigned pos = 0, usado = 0;
    if (n > 0 && (uint8_t)texto[0] == 0x30) {   /* DER crudo: un solo certificado */
        memcpy(der, texto, (unsigned)n);
        if (x509_ler(&certs[0], der, (unsigned)n) == X509_OK) return 1;
        imprimir_em(ERROS, "certificados: %s: no es un certificado válido\n", caminho);
        return -1;
    }
    while (qtd < max) {
        int t = pem_proximo(texto, (unsigned)n, &pos, der + usado, (unsigned)n - usado);
        if (t == 0) break;
        if (t < 0) { imprimir_em(ERROS, "certificados: %s: bloque PEM roto\n", caminho); return -1; }
        int r = x509_ler(&certs[qtd], der + usado, (unsigned)t);
        if (r != X509_OK) imprimir_em(ERROS, "certificados: %s: certificado %d: %s\n", caminho, qtd + 1, x509_erro_texto(r));
        else qtd++;
        usado += (unsigned)t;
    }
    if (!qtd) imprimir_em(ERROS, "certificados: %s: no contiene certificados PEM\n", caminho);
    return qtd;
}

static bool numero(const char *s, int *n)
{
    if (!*s) return false;
    int v = 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return false;
        v = v * 10 + (*s - '0');
    }
    *n = v;
    return true;
}

static int ver(const char *alvo)
{
    int n;
    if (numero(alvo, &n)) {
        if (n < 1 || n > raizes.n) { imprimir_em(ERROS, "certificados: no hay raíz número %s (mira \"certificados lista\")\n", alvo); return 1; }
        imprimir("Raíz %d (%s):\n", n, raizes.arquivo[n - 1]);
        x509_mostrar(SAIDA, &raizes.c[n - 1], "  ");
        return 0;
    }
    static cert_t certs[16];
    uint8_t *der;
    int q = ler_certs(alvo, certs, 16, &der);
    if (q <= 0) return 1;
    for (int i = 0; i < q; i++) {
        if (q > 1) imprimir("Certificado %d de %d:\n", i + 1, q);
        x509_mostrar(SAIDA, &certs[i], "  ");
    }
    return 0;
}

/* ------------------------------------------------------------ grabar PEM */

static void gravar_pem(int fd, const uint8_t *d, unsigned n)
{
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    escrever_texto(fd, "-----BEGIN CERTIFICATE-----\n");
    char linha[66];
    unsigned k = 0;
    for (unsigned i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)d[i] << 16 | (i + 1 < n ? (uint32_t)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        linha[k++] = b64[v >> 18 & 63];
        linha[k++] = b64[v >> 12 & 63];
        linha[k++] = i + 1 < n ? b64[v >> 6 & 63] : '=';
        linha[k++] = i + 2 < n ? b64[v & 63] : '=';
        if (k == 64) { linha[k++] = '\n'; escrever_tudo(fd, linha, k); k = 0; }
    }
    if (k) { linha[k++] = '\n'; escrever_tudo(fd, linha, k); }
    escrever_texto(fd, "-----END CERTIFICATE-----\n");
}

/* Nombre de archivo a partir del CN: letras, dígitos y '-' (corto: el SGFS limita el camino). */
static void nome_arquivo(const cert_t *c, char *s, unsigned max)
{
    char cn[128], h[100];
    x509_nome_curto(c->sujeito, cn, sizeof cn);
    x509_huella(c, h, sizeof h);
    unsigned k = 0;
    for (const char *p = cn; *p && k < 24; p++) {
        char ch = *p;
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) s[k++] = ch;
        else if (k && s[k - 1] != '-') s[k++] = '-';
    }
    while (k && s[k - 1] == '-') k--;
    s[k] = 0;
    char extra[16];   /* 4 bytes de la huella evitan colisiones entre CN iguales */
    formatar_texto(extra, sizeof extra, "-%c%c%c%c.pem", h[0] | 32, h[1] | 32, h[3] | 32, h[4] | 32);
    if (!k) copia_texto(s, "cert", max);
    junta_texto(s, extra, max);
}

static int adicionar(const char *arquivo)
{
    static cert_t certs[16];
    uint8_t *der;
    int q = ler_certs(arquivo, certs, 16, &der);
    if (q <= 0) return 1;
    bool pers = almacen_persistente();
    const char *pasta = pers ? RAIZES_PERSISTENTE : RAIZES_PASTA;
    if (pers) criar_pasta(RAIZES_PERSISTENTE);
    int novos = 0;
    for (int i = 0; i < q; i++) {
        const cert_t *c = &certs[i];
        char nome[128], h[100];
        x509_nome_curto(c->sujeito, nome, sizeof nome);
        bool ja = false;
        for (int j = 0; j < raizes.n && !ja; j++)
            ja = raizes.c[j].nder == c->nder && memcmp(raizes.c[j].der, c->der, c->nder) == 0;
        if (ja) { imprimir("\"%s\" ya está en el almacén.\n", nome); continue; }
        if (raizes.n + novos >= RAIZES_MAX) { imprimir_em(ERROS, "certificados: el almacén está lleno (%d)\n", RAIZES_MAX); break; }
        if (!c->tem_basico || !c->eh_ca)
            imprimir("Aviso: \"%s\" no es una autoridad de certificación; solo valdrá para ese mismo servidor.\n", nome);
        if (data_agora() > c->depois) imprimir("Aviso: \"%s\" está caducado.\n", nome);
        char arq[64], caminho[160];
        nome_arquivo(c, arq, sizeof arq);
        formatar_texto(caminho, sizeof caminho, "%s/%s", pasta, arq);
        int fd = abrir(caminho, ABRIR_CRIAR | ABRIR_TRUNCAR | ABRIR_ESCREVER);
        if (fd < 0) { imprimir_em(ERROS, "certificados: %s: %s\n", caminho, nome_erro(fd)); return 1; }
        gravar_pem(fd, c->der, c->nder);
        fechar(fd);
        x509_huella(c, h, sizeof h);
        imprimir("Añadido \"%s\" como %s\n  SHA-256 %s\n", nome, caminho, h);
        novos++;
    }
    if (novos) aviso_persistencia();
    return 0;
}

/* Reescribe un archivo de varios certificados sin el de índice "fuera". */
static bool reescrever_sem(const char *caminho, const cert_t *fora)
{
    static cert_t certs[16];
    uint8_t *der;
    int q = ler_certs(caminho, certs, 16, &der);
    if (q <= 0) return false;
    int fd = abrir(caminho, ABRIR_CRIAR | ABRIR_TRUNCAR | ABRIR_ESCREVER);
    if (fd < 0) return false;
    int ficam = 0;
    for (int i = 0; i < q; i++)
        if (!(certs[i].nder == fora->nder && memcmp(certs[i].der, fora->der, fora->nder) == 0)) { gravar_pem(fd, certs[i].der, certs[i].nder); ficam++; }
    fechar(fd);
    if (!ficam) apagar(caminho);
    libera(der);
    return true;
}

static int quitar(const char *alvo)
{
    int n;
    if (!numero(alvo, &n) || n < 1 || n > raizes.n) {
        imprimir_em(ERROS, "certificados: indica el número de la raíz (mira \"certificados lista\")\n");
        return 1;
    }
    const cert_t *c = &raizes.c[n - 1];
    const char *arq = raizes.arquivo[n - 1];
    char nome[128], h[100];
    x509_nome_curto(c->sujeito, nome, sizeof nome);
    x509_huella(c, h, sizeof h);
    if (!reescrever_sem(arq, c)) { imprimir_em(ERROS, "certificados: no se pudo cambiar %s\n", arq); return 1; }
    /* una raíz de fábrica en un sistema persistente: anota la huella para
     * seguir afuera después de reiniciar (el ramdisk vuelve como era) */
    if (almacen_persistente() && comeca_com(arq, RAIZES_PASTA "/")) {
        criar_pasta(RAIZES_PERSISTENTE);
        int fd = abrir(RAIZES_PERSISTENTE "/quitados", ABRIR_CRIAR | ABRIR_ACRESCENTAR | ABRIR_ESCREVER);
        if (fd >= 0) { escrever_texto(fd, h); escrever_texto(fd, "\n"); fechar(fd); }
    }
    imprimir("Quitado \"%s\" (%s)\n", nome, arq);
    aviso_persistencia();
    return 0;
}

static int probar(const char *alvo)
{
    char host[200];
    copia_texto(host, alvo, sizeof host);
    if (comeca_com(host, "https://")) memmove(host, host + 8, tam_texto(host + 8) + 1);
    char *barra = acha_char(host, '/');
    if (barra) *barra = 0;
    int porta = 443;
    char *dp = acha_char(host, ':');
    if (dp) { *dp = 0; porta = texto_para_int(dp + 1); }
    if (!host[0] || porta <= 0 || porta > 65535) return uso();
    url_t u;
    memset(&u, 0, sizeof u);
    copia_texto(u.host, host, sizeof u.host);
    u.porta = porta;
    u.https = true;
    tls_config_t cfg;
    char erro[200];
    if (!tls_config_padrao(&cfg, NULL, erro, sizeof erro)) { imprimir_em(ERROS, "certificados: %s\n", erro); return 1; }
    cfg.inseguro = true;   /* conecta aunque el certificado sea malo: el objetivo es mostrar por qué */
    imprimir("Conectando a %s:%d...\n", host, porta);
    conexao_t con;
    int r = conexao_abrir(&con, &u, &cfg);
    if (r < 0 && !con.tls) { imprimir("Error: %s\n", con.erro); return 1; }
    tls_t *t = con.tls;
    if (r == 0)
        imprimir("Conexión %s, %s, %s%s%s (%u ms)\n", tls_versao_texto(t), tls_suite_texto(t), tls_grupo_texto(t),
                 tls_alpn(t)[0] ? ", ALPN " : "", tls_alpn(t), tls_ms_handshake(t));
    else imprimir("La negociación TLS falló: %s\n", con.erro);
    const cert_t *c;
    int n = tls_cadeia(t, &c);
    for (int i = 0; i < n; i++) {
        imprimir("Certificado %d%s:\n", i, i == 0 ? " (servidor)" : "");
        x509_mostrar(SAIDA, &c[i], "  ");
    }
    int v = tls_validacao(t), raiz = tls_raiz(t);
    bool ok = r == 0 && v == X509_OK;
    if (ok && raiz >= 0) {
        char nome[128];
        x509_nome_curto(cfg.raizes->c[raiz].sujeito, nome, sizeof nome);
        imprimir("Resultado: certificado válido para %s (raíz %d: %s)\n", host, raiz + 1, nome);
    } else if (n > 0) {
        imprimir("Resultado: NO VÁLIDO: %s\n", x509_erro_texto(v));
        if (v == X509_EMISSOR) imprimir("  Si confías en el emisor, añade su certificado con \"certificados añadir archivo.pem\".\n");
    }
    conexao_fechar(&con);
    return ok ? 0 : 1;
}

int principal(int argc, char **argv)
{
    almacen_carregar(&raizes);
    const char *cmd = argc > 1 ? argv[1] : "lista";
    if (compara_texto(cmd, "lista") == 0 || compara_texto(cmd, "-l") == 0) return listar_raizes();
    if (compara_texto(cmd, "ver") == 0 && argc > 2) return ver(argv[2]);
    if ((compara_texto(cmd, "añadir") == 0 || compara_texto(cmd, "anadir") == 0 || compara_texto(cmd, "agregar") == 0) && argc > 2)
        return adicionar(argv[2]);
    if (compara_texto(cmd, "quitar") == 0 && argc > 2) return quitar(argv[2]);
    if (compara_texto(cmd, "probar") == 0 && argc > 2) return probar(argv[2]);
    return uso();
}
