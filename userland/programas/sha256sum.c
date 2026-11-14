// SPDX-License-Identifier: GPL-2.0-or-later
/* sha256sum - resumen SHA-256 de archivos (D060), como en Linux.
 *
 *   sha256sum archivo...     un "hexa  nombre" por archivo
 *   ... | sha256sum          da entrada
 * Serve para conferir descargas (wget) e certificados. */
#include "cripto.h"

static uint8_t buf[16384];

static int resumir(int fd, const char *nome)
{
    sha256_t c;
    sha256_iniciar(&c);
    int n;
    while ((n = ler(fd, buf, sizeof buf)) > 0) sha256_somar(&c, buf, (unsigned)n);
    if (n < 0) { imprimir_em(ERROS, "sha256sum: %s: %s\n", nome, nome_erro(n)); return 1; }
    uint8_t h[32];
    sha256_fim(&c, h);
    char s[65];
    for (int i = 0; i < 32; i++) formatar_texto(s + 2 * i, 3, "%02x", h[i]);
    imprimir("%s  %s\n", s, nome);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc < 2) return resumir(ENTRADA, "-");
    int erros = 0;
    for (int i = 1; i < argc; i++) {
        int fd = abrir(argv[i], ABRIR_LER);
        if (fd < 0) { imprimir_em(ERROS, "sha256sum: %s: %s\n", argv[i], nome_erro(fd)); erros++; continue; }
        erros += resumir(fd, argv[i]);
        fechar(fd);
    }
    return erros ? 1 : 0;
}
