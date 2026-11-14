/* malloc.h - lo que no es estándar de la memoria (D210).
 *
 * Parte de la libc común (userland/libc). malloc y compañía están en
 * stdlib.h; aquí, malloc_usable_size (lo que se puede usar de un bloque sin
 * moverlo; lo pide QuickJS) y memalign. Las funciones, en memoria.c.
 */
#ifndef NS_MALLOC_H
#define NS_MALLOC_H

#include <stdlib.h>

void *memalign(size_t alin, size_t n);

#endif
