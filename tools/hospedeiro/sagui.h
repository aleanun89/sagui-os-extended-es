// SPDX-License-Identifier: GPL-2.0-or-later
/* sagui.h de mentira: permite compilar cc.c y pascal.c como programas del
 * ordenador de desarrollo (gcc -m32), para probar el generador sin QEMU.
 * Solo trae lo que usan los compiladores. Se usa con -I tools/hospedeiro
 * y -I kernel/inc. Los programas generados deben usar la opción -linux. */
#ifndef SAGUI_H
#define SAGUI_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "abi.h"
#define ENTRADA 0
#define SAIDA   1
#define ERROS   2
#ifndef ABRIR_LER
#define ABRIR_LER 1
#endif
static inline int abrir(const char *c, int modo)
{
    int f = O_RDONLY;
    if (modo & ABRIR_ESCREVER) f = O_WRONLY | O_CREAT | O_TRUNC;
    return open(c, f, 0755);
}
static inline int ler(int fd, void *b, unsigned n) { return (int)read(fd, b, n); }
static inline int escrever(int fd, const void *b, unsigned n) { return (int)write(fd, b, n); }
static inline int fechar(int fd) { return close(fd); }
static inline void sair(int c) { exit(c); }
static inline size_t tam_texto(const char *s) { return strlen(s); }
static inline int compara_texto(const char *a, const char *b) { return strcmp(a, b); }
static inline char *copia_texto(char *d, const char *s, size_t m) { strncpy(d, s, m - 1); d[m - 1] = 0; return d; }
static inline void *aloca_zerado(size_t n) { return calloc(1, n); }
static inline void libera(void *p) { free(p); }
#define formatar_texto snprintf
static inline int imprimir_em(int fd, const char *f, ...)
{
    va_list a; va_start(a, f);
    int r = vdprintf(fd, f, a);
    va_end(a); return r;
}
static inline int info_sistema(int t, void *b, unsigned n) { (void)t; (void)b; (void)n; return -1; }
static inline char *acha_char(const char *s, int c) { return strchr(s, c); }
static inline char *acha_ultimo_char(const char *s, int c) { return strrchr(s, c); }
static inline bool cpu_tem_sse2(void) { return true; }
static inline int formatar_lista(char *d, size_t m, const char *f, va_list a) { return vsnprintf(d, m, f, a); }
static inline int apagar(const char *c) { return unlink(c); }
static inline int escrever_tudo(int fd, const void *b, unsigned n)
{
    const char *p = b; unsigned t = 0;
    while (t < n) { int r = (int)write(fd, p + t, n - t); if (r <= 0) return -1; t += r; }
    return (int)n;
}
static inline const char *nome_erro(int e) { (void)e; return "error"; }
static inline void registrar(const char *f, ...) { (void)f; }
extern const unsigned char fonte_sagui[256][16];
int principal(int argc, char **argv);
#endif
