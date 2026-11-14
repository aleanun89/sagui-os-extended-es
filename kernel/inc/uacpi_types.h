// SPDX-License-Identifier: GPL-2.0-or-later
/* uacpi_types.h - los tipos de uACPI sobre los del kernel (D192).
 *
 * uACPI incluye este archivo en lugar de <stdint.h>, <stddef.h>, <stdbool.h>
 * y <stdarg.h> cuando se compila con UACPI_OVERRIDE_TYPES (el Makefile lo pone
 * para todo el kernel). Así uACPI y la capa propia que lo usa ven los mismos
 * tipos que el resto del kernel (tipos.h) y nunca se mezclan las cabeceras del
 * compilador con las nuestras (bool, NULL y va_list están definidos en los dos
 * sitios y chocarían). Es el mismo contenido que la cabecera por defecto de
 * uACPI (include/uacpi/platform/types.h), con nuestros nombres. */
#ifndef SAGUI_UACPI_TYPES_H
#define SAGUI_UACPI_TYPES_H
#include "tipos.h"
#include <uacpi/helpers.h>

typedef uint8_t  uacpi_u8;
typedef uint16_t uacpi_u16;
typedef uint32_t uacpi_u32;
typedef uint64_t uacpi_u64;

typedef int8_t  uacpi_i8;
typedef int16_t uacpi_i16;
typedef int32_t uacpi_i32;
typedef int64_t uacpi_i64;

#define UACPI_TRUE  true
#define UACPI_FALSE false
typedef bool uacpi_bool;

#define UACPI_NULL NULL

typedef uintptr_t uacpi_uintptr;
typedef uacpi_uintptr uacpi_virt_addr;
typedef size_t uacpi_size;

typedef va_list uacpi_va_list;
#define uacpi_va_start va_start
#define uacpi_va_end   va_end
#define uacpi_va_arg   va_arg

typedef char uacpi_char;

#define uacpi_offsetof(t, m) __builtin_offsetof(t, m)

/* El formato de 64 bits: uACPI tiene su propio snprintf (stdlib.c), que entiende "ll". */
#define UACPI_PRIu64 "llu"
#define UACPI_PRIx64 "llx"
#define UACPI_PRIX64 "llX"
#define UACPI_FMT64(val) ((unsigned long long)(val))

#endif
