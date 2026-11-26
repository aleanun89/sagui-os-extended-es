// SPDX-License-Identifier: GPL-2.0-or-later
/* sagui.h para la prueba del puente del QuickJS en el PC (D488).
 *
 * El puente (userland/web/quickjs/qjs_bridge.c) solo usa estas pocas funciones
 * del SO; aquí se hacen con la libc del anfitrión para poder probar el DOM en
 * JavaScript (qjs_dom.js) sin arrancar QEMU. Lo compila tests/h488_*.py. */
#ifndef SAGUI_ANFITRIAO_H
#define SAGUI_ANFITRIAO_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

static inline void *aloca(size_t n) { return malloc(n); }
static inline void *aloca_zerado(size_t n) { return calloc(1, n); }
static inline void *realoca(void *p, size_t n) { return realloc(p, n); }
static inline void libera(void *p) { free(p); }
static inline size_t tam_texto(const char *s) { return strlen(s); }
static inline void copia_texto(char *d, const char *s, size_t n) { if (!n) return; strncpy(d, s, n - 1); d[n - 1] = 0; }
static inline unsigned tempo_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}
static inline void registrar(const char *f, ...)
{
    va_list a;
    va_start(a, f);
    vprintf(f, a);
    va_end(a);
}
#endif
