// SPDX-License-Identifier: GPL-2.0-or-later
/* c8base.h - o pouco que o emulador e o montador de CHIP-8 pedem ao sistema.
 *
 * Os mesmos .c compilam para o SaguiOS (com a libsagui) e para o host (gcc
 * com a libc, -DSAGUI_HOST). Assim o build monta as ROMs dos jogos com o
 * mesmo montador que roda dentro do SaguiOS, e as provas comparam as duas
 * saídas byte a byte (D071). Só se usa aqui o que existe nos dois lados com
 * o mesmo comportamento: formatar_texto com %d %u %x %s %c e largura. */
#ifndef C8BASE_H
#define C8BASE_H

#ifdef SAGUI_HOST
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#define formatar_texto snprintf
#define tam_texto strlen
#define compara_texto strcmp
#define compara_texto_n strncmp
#define aloca malloc
#define realoca realloc
#define libera free
#define acha_ultimo_char strrchr
static inline void *aloca_zerado(size_t n) { return calloc(1, n); }
static inline char *copia_texto(char *d, const char *s, size_t max)
{
    if (max) { snprintf(d, max, "%s", s); }
    return d;
}
#else
#include "sagui.h"
#endif

/* Entrada e saída (plataforma.c): o resto do código não sabe onde roda. */
uint8_t *c8_ler_arquivo(const char *caminho, int *tam);    /* NULL = não deu; libera() depois */
bool     c8_gravar_arquivo(const char *caminho, const void *dados, int n);
void     c8_saida(const char *texto);                      /* saída padrão */
void     c8_erro(const char *texto);                       /* saída de erros */
void     c8_saidaf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void     c8_errof(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
