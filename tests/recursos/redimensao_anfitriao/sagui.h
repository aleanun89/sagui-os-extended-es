// SPDX-License-Identifier: GPL-2.0-or-later
/* sagui.h de mentira para compilar userland/libsagui/redimensao.c en el anfitrión (h213, D213):
 * solo lo que usa ese archivo, con la libc del PC. */
#ifndef SAGUI_H
#define SAGUI_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    unsigned int setores, inicio, trim, arranque_real, reservado_pid, gpt, tipo_mbr, reservado[9];
} info_bloco_t;
typedef struct { int nada; } info_disco_t;

#define ERRO_INVALIDO      -1
#define ERRO_NAO_EXISTE    -2
#define ERRO_JA_EXISTE     -3
#define ERRO_SEM_MEMORIA   -4
#define ERRO_LIMITE        -9
#define ERRO_FORMATO      -13
#define ERRO_ACESSO       -14
#define ERRO_OCUPADO      -15
#define ERRO_NAO_SUPORTADO -17
#define DBLOCO_CONF_ARRANQUE 1
#define DBLOCO_CONF_GPT      2
#define DBLOCO_CONF_MONTADO  4

static inline void *aloca(size_t n) { return malloc(n); }
static inline void *aloca_zerado(size_t n) { return calloc(1, n); }
static inline void libera(void *p) { free(p); }
static inline int formatar_lista(char *s, size_t n, const char *f, va_list ap) { return vsnprintf(s, n, f, ap); }
#define formatar_texto snprintf
static inline void copia_texto(char *d, const char *s, size_t n) { snprintf(d, n, "%s", s); }
static inline int compara_texto(const char *a, const char *b) { return strcmp(a, b); }
static inline bool comeca_com(const char *s, const char *p) { return strncmp(s, p, strlen(p)) == 0; }   /* D282: formatacao.c */
static inline const char *nome_erro(int r) { (void)r; return "error de E/S"; }
void registrar(const char *fmt, ...);
int bytes_aleatorios(void *p, unsigned n);
uint32_t tempo_ms(void);
#define TRIM_SIM 1
static inline uint32_t hora_utc(void *p) { (void)p; return 1762905600u; }
static inline size_t tam_texto(const char *s) { return strlen(s); }
#endif
