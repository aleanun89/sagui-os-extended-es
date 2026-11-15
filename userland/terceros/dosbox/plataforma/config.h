/* config.h - la configuración de DOSBox 0.74-3 para el SaguiOS (D294).
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Hace el papel del config.h que genera ./configure: i686 de 32 bits sin SDL_net, sin OpenGL, sin
 * depurador, sin captura de pantalla (libpng), sin módem ni IPX (sin red), sin serie real. Con
 * los dos núcleos de CPU rápidos del PC de 32 bits: el recompilador dinámico de x86
 * (C_DYNAMIC_X86, «core=dynamic») y la FPU en ensamblador x87 (C_FPU_X86). C_DYNREC (el de otras
 * arquitecturas) y C_DEBUG quedan fuera. Los nombres son los de config.h.in.
 */
#ifndef SAGUI_DOSBOX_CONFIG_H
#define SAGUI_DOSBOX_CONFIG_H

#define C_ATTRIBUTE_ALWAYS_INLINE 1
#define C_ATTRIBUTE_FASTCALL 1
#define C_HAS_ATTRIBUTE 1
#define C_HAS_BUILTIN_EXPECT 1
#define C_TARGETCPU X86
#define C_UNALIGNED_MEMORY 1
#define C_FPU 1
#define C_FPU_X86 1
#define C_DYNAMIC_X86 1
#define C_CORE_INLINE 1

#define HAVE_INTTYPES_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define STDC_HEADERS 1
#define TM_IN_SYS_TIME 1
#define DIRENT_HAS_D_TYPE 1

#define PACKAGE "dosbox"
#define PACKAGE_NAME "dosbox"
#define PACKAGE_STRING "dosbox 0.74-3"
#define PACKAGE_TARNAME "dosbox"
#define PACKAGE_VERSION "0.74-3"
#define VERSION "0.74-3"

#define SIZEOF_INT_P 4
#define SIZEOF_UNSIGNED_CHAR 1
#define SIZEOF_UNSIGNED_SHORT 2
#define SIZEOF_UNSIGNED_INT 4
#define SIZEOF_UNSIGNED_LONG 4
#define SIZEOF_UNSIGNED_LONG_LONG 8

/* Lo que el config.h.in de DOSBox trae al final (INLINE, GCC_ATTRIBUTE, los tipos Bit8u... Bitu),
 * tal cual. */
#if C_ATTRIBUTE_ALWAYS_INLINE
#define INLINE inline __attribute__((always_inline))
#else
#define INLINE inline
#endif

#if C_ATTRIBUTE_FASTCALL
#define DB_FASTCALL __attribute__((fastcall))
#else
#define DB_FASTCALL
#endif

#if C_HAS_ATTRIBUTE
#define GCC_ATTRIBUTE(x) __attribute__ ((x))
#else
#define GCC_ATTRIBUTE(x) /* attribute not supported */
#endif

#if C_HAS_BUILTIN_EXPECT
#define GCC_UNLIKELY(x) __builtin_expect((x),0)
#define GCC_LIKELY(x) __builtin_expect((x),1)
#else
#define GCC_UNLIKELY(x) (x)
#define GCC_LIKELY(x) (x)
#endif


typedef         double     Real64;

#if SIZEOF_UNSIGNED_CHAR != 1
#  error "sizeof (unsigned char) != 1"
#else
  typedef unsigned char Bit8u;
  typedef   signed char Bit8s;
#endif

#if SIZEOF_UNSIGNED_SHORT != 2
#  error "sizeof (unsigned short) != 2"
#else
  typedef unsigned short Bit16u;
  typedef   signed short Bit16s;
#endif

#if SIZEOF_UNSIGNED_INT == 4
  typedef unsigned int Bit32u;
  typedef   signed int Bit32s;
#elif SIZEOF_UNSIGNED_LONG == 4
  typedef unsigned long Bit32u;
  typedef   signed long Bit32s;
#else
#  error "can't find sizeof(type) of 4 bytes!"
#endif

#if SIZEOF_UNSIGNED_LONG == 8
  typedef unsigned long Bit64u;
  typedef   signed long Bit64s;
#elif SIZEOF_UNSIGNED_LONG_LONG == 8
  typedef unsigned long long Bit64u;
  typedef   signed long long Bit64s;
#else
#  error "can't find data type of 8 bytes"
#endif

#if SIZEOF_INT_P == 4
  typedef Bit32u Bitu;
  typedef Bit32s Bits;
#else
  typedef Bit64u Bitu;
  typedef Bit64s Bits;
#endif

#endif
