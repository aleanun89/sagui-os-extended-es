// SPDX-License-Identifier: GPL-2.0-or-later
/* md5sum - resumen MD5 de archivos, como en Linux (D232).
 *   md5sum archivo...     "hexa  nombre" por archivo       md5sum -c LISTA   comprueba una lista
 * Sirve para conferir descargas; no es una protección contra manipulaciones. */
#include "resumos_logica.h"

static const char *AYUDA =
    "uso: md5sum [-c] [archivo...]\n"
    "  Escribe el resumen MD5 de cada archivo (o de la entrada si no hay) con el formato de Linux.\n"
    "  -c  los archivos son listas de resúmenes (salida de md5sum): comprueba cada uno e indica OK o FALLO\n";

static uint8_t buf[16384];

static int resumir(int fd, char *hex)
{
    resumo_t c;
    resumo_iniciar(&c, false);
    int n;
    while ((n = ler(fd, buf, sizeof buf)) > 0) resumo_somar(&c, false, buf, (unsigned)n);
    if (n < 0) return n;
    resumo_fim(&c, false, hex);
    return 0;
}

static int comprobar(const char *lista)
{
    int fd = abrir_entrada("md5sum", lista);
    if (fd < 0) return 1;
    leitor_linhas_t l;
    leitor_iniciar(&l, fd, false);
    char linea[600];
    int mal = 0, noleidos = 0, total = 0, r;
    while ((r = leitor_linha(&l, linea, sizeof linea, NULL)) >= 0) {
        if (r < 34) continue;
        char *p = linea;
        while (*p && *p != ' ') p++;
        if (!*p) continue;
        *p++ = 0;
        if (*p == ' ' || *p == '*') p++;
        int f = abrir(p, ABRIR_LER);
        total++;
        char hex[48];
        if (f < 0 || resumir(f, hex) < 0) { imprimir("%s: no se pudo leer\n", p); noleidos++; if (f >= 0) fechar(f); continue; }
        fechar(f);
        bool ok = compara_texto(hex, linea) == 0;
        imprimir("%s: %s\n", p, ok ? "OK" : "FALLO");
        if (!ok) mal++;
    }
    fechar_entrada(fd);
    if (mal) imprimir_em(ERROS, "md5sum: AVISO: %d de %d sumas calculadas no coinciden\n", mal, total);
    if (noleidos) imprimir_em(ERROS, "md5sum: AVISO: %d archivos listados no se pudieron leer\n", noleidos);
    return (mal || noleidos) ? 1 : 0;
}

int principal(int argc, char **argv)
{
    bool verificar = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "cb", &o)) >= 0) {
        if (r == 'c') verificar = true;
        else if (r == 'b') continue;
        else if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        else return opcao_invalida("md5sum", r, &o);
    }
    int erros = 0;
    if (o.indice >= argc) {
        if (verificar) return comprobar("-");
        char hex[48];
        int e = resumir(ENTRADA, hex);
        if (e < 0) { imprimir_em(ERROS, "md5sum: %s\n", nome_erro(e)); return 1; }
        imprimir("%s  -\n", hex);
        return 0;
    }
    for (int i = o.indice; i < argc; i++) {
        if (verificar) { erros += comprobar(argv[i]); continue; }
        int fd = compara_texto(argv[i], "-") == 0 ? ENTRADA : abrir(argv[i], ABRIR_LER);
        char hex[48];
        int e = fd < 0 ? fd : resumir(fd, hex);
        if (fd > 2) fechar(fd);
        if (e < 0) { imprimir_em(ERROS, "md5sum: %s: %s\n", argv[i], nome_erro(e)); erros++; continue; }
        imprimir("%s  %s\n", hex, argv[i]);
    }
    return erros ? 1 : 0;
}
