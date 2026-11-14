// SPDX-License-Identifier: GPL-2.0-or-later
/* plataforma.c - arquivos e saída de texto do emulador e do montador de
 * CHIP-8, no SaguiOS (libsagui) ou no host (libc), ver c8base.h. */
#include "c8base.h"

#ifdef SAGUI_HOST
#include <stdarg.h>

uint8_t *c8_ler_arquivo(const char *caminho, int *tam)
{
    FILE *f = fopen(caminho, "rb");
    if (!f) return NULL;
    size_t cap = 4096, n = 0;
    uint8_t *b = malloc(cap + 1);
    for (;;) {
        if (n == cap) { cap *= 2; b = realloc(b, cap + 1); }
        size_t r = fread(b + n, 1, cap - n, f);
        if (r == 0) break;
        n += r;
    }
    fclose(f);
    b[n] = 0;   /* o montador trata o texto como uma cadeia terminada em zero */
    *tam = (int)n;
    return b;
}

bool c8_gravar_arquivo(const char *caminho, const void *dados, int n)
{
    FILE *f = fopen(caminho, "wb");
    if (!f) return false;
    bool ok = fwrite(dados, 1, (size_t)n, f) == (size_t)n;
    return fclose(f) == 0 && ok;
}

void c8_saida(const char *t) { fputs(t, stdout); }
void c8_erro(const char *t) { fputs(t, stderr); }

void c8_saidaf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
}

void c8_errof(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

#else

uint8_t *c8_ler_arquivo(const char *caminho, int *tam)
{
    info_arquivo_t inf;
    if (info_arquivo(caminho, &inf) < 0 || inf.tipo == TIPO_PASTA) return NULL;
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return NULL;
    uint8_t *b = aloca(inf.tamanho + 1);
    if (!b) { fechar(fd); return NULL; }
    int n = 0;
    while (n < (int)inf.tamanho) {
        int r = ler(fd, b + n, inf.tamanho - (unsigned)n);
        if (r <= 0) break;
        n += r;
    }
    fechar(fd);
    b[n] = 0;
    *tam = n;
    return b;
}

bool c8_gravar_arquivo(const char *caminho, const void *dados, int n)
{
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return false;
    bool ok = escrever_tudo(fd, dados, (unsigned)n) == n;
    return fechar(fd) >= 0 && ok;
}

void c8_saida(const char *t) { escrever_texto(SAIDA, t); }
void c8_erro(const char *t) { escrever_texto(ERROS, t); }

void c8_saidaf(const char *fmt, ...)
{
    char b[512];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(b, sizeof b, fmt, ap);
    va_end(ap);
    escrever_texto(SAIDA, b);
}

void c8_errof(const char *fmt, ...)
{
    char b[512];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(b, sizeof b, fmt, ap);
    va_end(ap);
    escrever_texto(ERROS, b);
}

#endif
