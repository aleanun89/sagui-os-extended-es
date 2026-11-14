/* memoria.c - malloc/free/realloc/calloc de la libc común (D210).
 *
 * Hasta la D209 NetSurf traía aquí su propio reservador por clases de tamaño,
 * porque el de la libsagui buscaba "el primero que quepa" desde el principio.
 * Desde la D209 el de la libsagui ya es por clases (O(1)) y devuelve al
 * sistema lo que se libera, así que la libc solo lo envuelve: así un bloque
 * de malloc() se puede liberar con libera() y al revés, y todos los ports
 * comparten un solo montón.
 *
 * free() de un puntero que no salió de aquí (o liberado dos veces, si su
 * tramo ya no existe) se registra y para el programa: mejor parar aquí que
 * corromper las listas y caer lejos.
 */
#include "sagui.h"

#include "errno.h"
#include "stdlib.h"
#include "string.h"

void *malloc(size_t n)
{
    void *p = aloca(n);
    if (!p) errno = ENOMEM;
    return p;
}

void free(void *p)
{
    if (!p) return;
    if (!memoria_tamanho(p)) {
        registrar("libc: free() de un puntero desconocido %p\n", p);
        sair(134);
    }
    libera(p);
}

void *realloc(void *p, size_t n)
{
    if (!p) return malloc(n);
    if (n == 0) { free(p); return NULL; }
    if (!memoria_tamanho(p)) {
        registrar("libc: realloc() de un puntero desconocido %p\n", p);
        sair(134);
    }
    void *q = realoca(p, n);
    if (!q) errno = ENOMEM;
    return q;
}

void *calloc(size_t n, size_t tam)
{
    if (tam && n > 0xFFFFFFFFu / tam) { errno = ENOMEM; return NULL; }
    void *p = aloca_zerado(n * tam);
    if (!p) errno = ENOMEM;
    return p;
}

int posix_memalign(void **p, size_t alin, size_t n)
{
    if (!alin || (alin & (alin - 1)) || alin % sizeof(void *)) return EINVAL;
    void *q = aloca_alinhado(alin, n);
    if (!q) return ENOMEM;
    *p = q;
    return 0;
}

void *aligned_alloc(size_t alin, size_t n)
{
    void *q = aloca_alinhado(alin, n);
    if (!q) errno = ENOMEM;
    return q;
}

/* La antigua de malloc.h: lo mismo que aligned_alloc. */
void *memalign(size_t alin, size_t n) { return aligned_alloc(alin, n); }

size_t malloc_usable_size(void *p) { return memoria_tamanho(p); }
