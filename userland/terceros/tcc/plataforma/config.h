/* SPDX-License-Identifier: GPL-3.0-only */
/* config.h - configuración de TCC para SaguiOS (D384). Sustituye al config.h que genera el
 * «configure» de TCC: la incluye tcc.h. Vale para los dos compiladores que se construyen:
 *   - /apps/bin/tcc, que corre dentro de SaguiOS (TCC_SAGUI_NATIVO: -run, sin dlopen);
 *   - build/tcc-anfitrion/i386-tcc, el cruzado que corre en el PC (libtcc1.a y pruebas rápidas).
 * Los dos generan lo mismo: ELF32 de i386 estáticos a 0x80000000 (el área de los programas, D049). */
#ifndef TCC_SAGUI_CONFIG_H
#define TCC_SAGUI_CONFIG_H

#define TCC_VERSION "0.9.28rc"
#define TCC_TARGET_I386 1
#define CONFIG_SAGUIOS 1

/* Rutas dentro de SaguiOS: {B} es CONFIG_TCCDIR o lo que diga -B. */
#define CONFIG_TCCDIR "/apps/tcc"
#define CONFIG_TCC_SYSINCLUDEPATHS "{B}/include:{B}/include/sagui:{B}/include/kernel:{B}/include/SDL2"
#define CONFIG_TCC_LIBPATHS "{B}/lib"
#define CONFIG_TCC_CRTPREFIX "{B}/lib"
#define CONFIG_TCC_ELFINTERP "/lib/ld-sagui.so.1"

/* SaguiOS solo enlaza en estático con TCC (D384): sin dlopen dentro del compilador, sin hilos,
 * sin trazas de pila ni comprobación de límites (necesitan señales y bibliotecas que no hay). */
#define CONFIG_TCC_STATIC 1
#define CONFIG_TCC_SEMLOCK 0
#define CONFIG_TCC_BACKTRACE 1
#define CONFIG_TCC_BCHECK 1
/* tccdefs.h se lee en tiempo de ejecución de {B}/include (no hace falta el c2str del anfitrión). */
#define CONFIG_TCC_PREDEFS 0
/* -run: el código en páginas R-X y los datos en RW- (W^X, D349). */
#define CONFIG_RUNMEM_RO 1

/* tccrun.c usa environ; la libc común lo define (posix.c) sin declararlo en unistd.h */
extern char **environ;
/* tcc_libc_extra.c (la libm común no trae long double) */
long double ldexpl(long double x, int e);

#endif
