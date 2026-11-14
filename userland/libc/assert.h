/* assert.h - assert (con NDEBUG no hace nada) (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_ASSERT_H
#define NS_ASSERT_H

#ifdef NDEBUG
#define assert(e) ((void)0)
#else
void __ns_assert_falhou(const char *expr, const char *arq, int linha) __attribute__((noreturn));
#define assert(e) ((e) ? (void)0 : __ns_assert_falhou(#e, __FILE__, __LINE__))
#endif
/* En C11 static_assert es una macro de <assert.h>; en C++ es una palabra
 * clave y la macro la estropearía (D230). */
#if !defined(__cplusplus) && !defined(static_assert)
#define static_assert _Static_assert
#endif
#endif
