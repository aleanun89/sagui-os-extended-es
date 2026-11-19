/* SPDX-License-Identifier: MIT */
/* atom_compat.h - lo que el intérprete ATOM de Linux (atom.c) toma del kernel de Linux, para el
 * kernel de SaguiOS y para las pruebas en el anfitrión (D399). Propio de SaguiOS; MIT como la
 * carpeta, para que la copia de atom.c no cambie de licencia al compilarse.
 *
 * En el anfitrión (pruebas: tests/gpu_energia_unidad.c) se define ATOM_ANFITRIAO y se usa la libc.
 * En el kernel: kmalloc/kfree, memset y klog (las líneas del intérprete salen como [ATOM]). */
#ifndef ATOM_COMPAT_H
#define ATOM_COMPAT_H

#ifdef ATOM_ANFITRIAO
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define atom_alocar(n)      calloc(1, (n))
#define atom_soltar(p)      free(p)
#define atom_registo(...)   do { if (getenv("ATOM_VERBOSO")) { printf("[ATOM] " __VA_ARGS__); } } while (0)
#else
#include "tipos.h"
#include "memoria.h"
#include "texto.h"
#include "console.h"
static inline void *atom_alocar(size_t n) { void *p = kmalloc(n); if (p) memset(p, 0, n); return p; }
#define atom_soltar(p)      do { if (p) kfree(p); } while (0)
#define atom_registo(...)   klog("[ATOM] " __VA_ARGS__)
static inline int strncmp(const char *a, const char *b, size_t n) { return txt_compara_n(a, b, n); }
static inline size_t strlen(const char *s) { return txt_tamanho(s); }
#endif

typedef uint16_t u16;
typedef uint32_t u32;
typedef uint8_t  u8;

#define GFP_KERNEL           0
#define EINVAL               22
#define ENOMEM               12
#define ARRAY_SIZE(a)        (sizeof(a) / sizeof((a)[0]))
#define kzalloc(n, f)        atom_alocar(n)
#define kcalloc(n, t, f)     atom_alocar((size_t)(n) * (size_t)(t))
#define kfree(p)             atom_soltar(p)
#define printk(...)          atom_registo(__VA_ARGS__)
#define pr_info(...)         atom_registo(__VA_ARGS__)
#define DRM_ERROR(...)       atom_registo(__VA_ARGS__)
#define DRM_DEBUG(...)       do { } while (0)
#define cpu_to_le32(x)       (x)
#define le32_to_cpu(x)       (x)

/* x86: los accesos sin alinear valen */
static inline uint32_t get_unaligned_le32(const void *p) { const uint8_t *b = (const uint8_t *)p; return b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24; }

#endif
