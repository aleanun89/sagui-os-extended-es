// SPDX-License-Identifier: GPL-2.0-or-later
/* KHR/khrplatform.h - tipos básicos de las API de Khronos para SaguiOS (D425).
 *
 * Escrita para SaguiOS con los nombres y tamaños que fijan las especificaciones
 * de Khronos (no es la cabecera de Khronos): x86 de 32 bits, ILP32. Los enteros
 * de 64 bits solo se dan si el compilador los tiene (el cc de dentro no). */
#ifndef __khrplatform_h_
#define __khrplatform_h_

#define KHRONOS_APICALL
#define KHRONOS_APIENTRY
#define KHRONOS_APIATTRIBUTES

typedef int                khronos_int32_t;
typedef unsigned int       khronos_uint32_t;
#if defined(__GNUC__) || defined(__TINYC__)
typedef long long          khronos_int64_t;
typedef unsigned long long khronos_uint64_t;
#define KHRONOS_SUPPORT_INT64 1
#else
#define KHRONOS_SUPPORT_INT64 0
#endif
#define KHRONOS_SUPPORT_FLOAT 1
typedef signed char        khronos_int8_t;
typedef unsigned char      khronos_uint8_t;
typedef short              khronos_int16_t;
typedef unsigned short     khronos_uint16_t;
typedef long               khronos_intptr_t;
typedef unsigned long      khronos_uintptr_t;
typedef long               khronos_ssize_t;
typedef unsigned long      khronos_usize_t;
typedef float              khronos_float_t;
#if KHRONOS_SUPPORT_INT64
typedef khronos_uint64_t   khronos_utime_nanoseconds_t;
typedef khronos_int64_t    khronos_stime_nanoseconds_t;
#endif

typedef enum { KHRONOS_FALSE = 0, KHRONOS_TRUE = 1, KHRONOS_BOOLEAN_ENUM_FORCE_SIZE = 0x7FFFFFFF } khronos_boolean_enum_t;

#endif
