/* alloca.h - memoria en la pila de la función (D210).
 *
 * Parte de la libc común (userland/libc). La da el propio GCC; stdlib.h la
 * incluye con _DEFAULT_SOURCE, como la glibc (QuickJS la usa así).
 */
#ifndef NS_ALLOCA_H
#define NS_ALLOCA_H

#include <stddef.h>
#ifndef alloca
#define alloca(n) __builtin_alloca(n)
#endif

#endif
