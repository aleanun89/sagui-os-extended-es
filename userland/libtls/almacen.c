// SPDX-License-Identifier: GPL-2.0-or-later
/* almacen.c - o almacén de certificados raiz (D063).
 *
 * As raízes vêm de arquivos PEM (um ou vários certificados cada) em duas
 * pastas:
 *   /etc/ssl/certs       as que vêm com o SaguiOS, no ramdisk (perde-se ao
 *                        desligar o que for mudado aqui);
 *   /home/.certificados  as que o usuário adicionou, quando /home é um disco
 *                        persistente montado (D040). Um arquivo "quitados"
 *                        nela lista as huellas SHA-256 das raízes de fábrica
 *                        que o usuário tirou, para continuarem fora depois
 *                        de reiniciar.
 * Os certificados ficam na memória (o DER copiado para o heap). */
#include "tls.h"

bool almacen_persistente(void)
{
    info_montagem_t m[10];
    int n = info_sistema(INFO_MONTAGENS, m, 10);
    for (int i = 0; i < n && i < 10; i++)
        if (compara_texto(m[i].ponto, "/home") == 0 && !(m[i].opcoes & MONTAR_SO_LEITURA)) return true;
    return false;
}

/* Lê um arquivo inteiro para o heap (com um zero no fim). */
static char *ler_arquivo(const char *caminho, unsigned *n)
{
    info_arquivo_t inf;
    if (info_arquivo(caminho, &inf) < 0 || inf.tipo != TIPO_ARQUIVO || inf.tamanho > 1024 * 1024) return NULL;
    char *b = aloca(inf.tamanho + 1);
    if (!b) return NULL;
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) { libera(b); return NULL; }
    unsigned k = 0;
    int r;
    while (k < inf.tamanho && (r = ler(fd, b + k, inf.tamanho - k)) > 0) k += (unsigned)r;
    fechar(fd);
    b[k] = 0;
    *n = k;
    return b;
}

static bool quitado(const char *quitados, const cert_t *c)
{
    if (!quitados) return false;
    char h[100];
    x509_huella(c, h, sizeof h);
    unsigned nh = (unsigned)tam_texto(h);
    for (const char *p = quitados; *p; ) {
        if (compara_texto_n(p, h, nh) == 0) return true;
        while (*p && *p != '\n') p++;
        if (*p) p++;
    }
    return false;
}

static void carregar_pasta(almacen_t *a, const char *pasta, const char *quitados)
{
    entrada_pasta_t e;
    for (int i = 0; listar(pasta, i, &e) == 1 && a->n < RAIZES_MAX; i++) {
        unsigned ne = (unsigned)tam_texto(e.nome);
        if (e.tipo != TIPO_ARQUIVO || ne < 5) continue;
        if (compara_texto(e.nome + ne - 4, ".pem") != 0 && compara_texto(e.nome + ne - 4, ".crt") != 0) continue;
        char caminho[160];
        formatar_texto(caminho, sizeof caminho, "%s/%s", pasta, e.nome);
        unsigned n;
        char *texto = ler_arquivo(caminho, &n);
        if (!texto) continue;
        unsigned pos = 0;
        for (;;) {
            uint8_t *der = aloca(n);   /* o DER é sempre menor que o PEM */
            if (!der) break;
            int k = pem_proximo(texto, n, &pos, der, n);
            if (k <= 0) { libera(der); break; }
            cert_t *c = &a->c[a->n];
            bool repetido = false;
            for (int j = 0; j < a->n && !repetido; j++)
                repetido = a->c[j].nder == (unsigned)k && memcmp(a->c[j].der, der, (unsigned)k) == 0;
            if (repetido || x509_ler(c, der, (unsigned)k) != X509_OK || quitado(quitados, c)) { libera(der); continue; }
            copia_texto(a->arquivo[a->n], caminho, sizeof a->arquivo[0]);
            if (++a->n == RAIZES_MAX) break;
        }
        libera(texto);
    }
}

int almacen_carregar(almacen_t *a)
{
    a->n = 0;
    char *quitados = NULL;
    bool pers = almacen_persistente();
    if (pers) {
        unsigned n;
        quitados = ler_arquivo(RAIZES_PERSISTENTE "/quitados", &n);
    }
    carregar_pasta(a, RAIZES_PASTA, quitados);
    if (pers) carregar_pasta(a, RAIZES_PERSISTENTE, NULL);
    libera(quitados);
    return a->n;
}
