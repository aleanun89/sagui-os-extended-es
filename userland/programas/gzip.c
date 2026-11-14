// SPDX-License-Identifier: GPL-2.0-or-later
/* gzip, gunzip y zcat (mejora 056, D103): un solo fuente; gunzip.c y zcat.c lo
 * incluyen con MODO_GZ distinto.
 *
 *   gzip [-k] [-c] [-f] [-1..-9] archivo...     comprime a archivo.gz (borra el original salvo -k)
 *   gunzip [-k] [-c] [-f] archivo.gz...         descomprime (= gzip -d)
 *   zcat archivo.gz...                          descomprime a la salida estándar (= gzip -dc)
 *   gzip -t archivo.gz                          comprueba la integridad
 * Sin archivos (o con -) trabaja de la entrada a la salida estándar. */
#include "compressao.h"

#ifndef MODO_GZ
#define MODO_GZ 0   /* 0 = gzip, 1 = gunzip, 2 = zcat */
#endif

static const char *const prog = MODO_GZ == 0 ? "gzip" : MODO_GZ == 1 ? "gunzip" : "zcat";
static bool k_manter, c_saida, forcar, descomprimir = MODO_GZ != 0, testar, silencio;
static int nivel = 6;

static void uso(void)
{
    imprimir_em(ERROS, "uso: %s [-cdfkt] [-1..-9] [archivo...]\n"
                       "  -c salida estándar   -d descomprimir   -f sobrescribir   -k conservar el original\n"
                       "  -t comprobar   -1..-9 nivel (1 rápido, 9 más pequeño)\n", prog);
}

static bool existe(const char *c) { info_arquivo_t i; return info_arquivo(c, &i) == 0; }

static int trabalhar(const char *caminho)
{
    bool entrada = !caminho;
    int n;
    if (!entrada) {
        info_arquivo_t inf;
        if (info_arquivo(caminho, &inf) < 0) { imprimir_em(ERROS, "%s: %s: no existe\n", prog, caminho); return 1; }
        if (inf.tipo == TIPO_PASTA) { imprimir_em(ERROS, "%s: %s: es una carpeta, se omite\n", prog, caminho); return 1; }
    }
    uint8_t *d = entrada ? carregar_descritor(ENTRADA, &n) : carregar_arquivo(caminho, &n);
    if (!d) { imprimir_em(ERROS, "%s: %s: no se puede leer\n", prog, entrada ? "entrada estándar" : caminho); return 1; }
    char destino[300];
    uint8_t *saida;
    int tam;
    bool ao_terminal = entrada || c_saida;
    if (descomprimir || testar) {
        char nome_orig[NOME_MAX];
        int r = gzip_descomprimir(d, n, &saida, &tam, nome_orig, sizeof nome_orig);
        libera(d);
        if (r < 0) { imprimir_em(ERROS, "%s: %s: %s\n", prog, entrada ? "-" : caminho, comp_nome_erro(r)); return 1; }
        if (testar) { libera(saida); if (!silencio && !entrada) imprimir("%s: OK\n", caminho); return 0; }
        if (!ao_terminal) {
            size_t l = tam_texto(caminho);
            copia_texto(destino, caminho, sizeof destino);
            if (l > 3 && compara_texto(caminho + l - 3, ".gz") == 0) destino[l - 3] = 0;
            else if (l > 4 && compara_texto(caminho + l - 4, ".tgz") == 0) { destino[l - 4] = 0; junta_texto(destino, ".tar", sizeof destino); }
            else if (l > 2 && compara_texto(caminho + l - 2, ".z") == 0) destino[l - 2] = 0;
            else { imprimir_em(ERROS, "%s: %s: sufijo desconocido, se omite\n", prog, caminho); libera(saida); return 1; }
        }
    } else {
        if (!entrada && !c_saida) {
            size_t l = tam_texto(caminho);
            if (l > 3 && compara_texto(caminho + l - 3, ".gz") == 0) {
                imprimir_em(ERROS, "%s: %s ya tiene el sufijo .gz, se omite\n", prog, caminho);
                libera(d);
                return 1;
            }
            formatar_texto(destino, sizeof destino, "%s.gz", caminho);
        }
        const char *base = entrada ? NULL : acha_ultimo_char(caminho, '/');
        if (!entrada) base = base ? base + 1 : caminho;
        int cap = deflar_limite(n) + 64 + (base ? (int)tam_texto(base) : 0);
        saida = aloca((size_t)cap);
        if (!saida) { libera(d); imprimir_em(ERROS, "%s: sin memoria\n", prog); return 1; }
        tam = gzip_comprimir(d, n, base, entrada ? hora_utc(NULL) : comp_mtime(caminho), nivel, saida, cap);
        libera(d);
        if (tam < 0) { imprimir_em(ERROS, "%s: %s\n", prog, comp_nome_erro(tam)); libera(saida); return 1; }
    }
    int r = 0;
    if (ao_terminal) {
        r = escrever_tudo(SAIDA, saida, (unsigned)tam);
    } else {
        if (existe(destino) && !forcar) { imprimir_em(ERROS, "%s: %s ya existe (usa -f para sobrescribir)\n", prog, destino); libera(saida); return 1; }
        r = salvar_arquivo(destino, saida, tam);
        if (r >= 0 && !k_manter) apagar(caminho);
    }
    libera(saida);
    if (r < 0) { imprimir_em(ERROS, "%s: %s\n", prog, nome_erro(r)); return 1; }
    return 0;
}

int principal(int argc, char **argv)
{
    int erros = 0, arquivos = 0;
    if (MODO_GZ == 2) c_saida = true;
    for (int i = 1; i < argc; i++) {
        char *a = argv[i];
        if (compara_texto(a, "--help") == 0) { uso(); return 0; }
        if (a[0] != '-' || !a[1]) continue;
        for (char *c = a + 1; *c; c++) {
            if (*c == 'k') k_manter = true;
            else if (*c == 'c') c_saida = true;
            else if (*c == 'd') descomprimir = true;
            else if (*c == 'f') forcar = true;
            else if (*c == 't') testar = true;
            else if (*c == 'q') silencio = true;
            else if (*c >= '1' && *c <= '9') nivel = *c - '0';
            else { imprimir_em(ERROS, "%s: opción desconocida -%c\n", prog, *c); uso(); return 2; }
        }
    }
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1]) continue;
        arquivos++;
        erros += trabalhar(compara_texto(argv[i], "-") == 0 ? NULL : argv[i]);
    }
    if (!arquivos) erros += trabalhar(NULL);
    return erros ? 1 : 0;
}
