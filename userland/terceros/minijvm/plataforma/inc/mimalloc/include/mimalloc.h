/* mimalloc.h - sustituto mínimo de mimalloc para miniJVM en SaguiOS (D376): todo va al malloc
 * de la libc común. La contabilidad de memoria de la VM (pri_alloc_*) usa mi_usable_size. */
#ifndef SAGUI_MIMALLOC_H
#define SAGUI_MIMALLOC_H
#include <stdlib.h>
#include <malloc.h>
#include <stdbool.h>
#define mi_malloc(n) malloc(n)
#define mi_free(p) free(p)
#define mi_realloc(p, n) realloc((p), (n))
#define mi_usable_size(p) malloc_usable_size(p)
#define mi_collect(f) ((void)0)
#define mi_version() 0
#define mi_process_init() ((void)0)
#define mi_process_done() ((void)0)
#define mi_process_info(a, b, c, d, e, f, g, h) do { *(a) = *(b) = *(c) = *(d) = *(e) = *(f) = *(g) = *(h) = 0; } while (0)
#endif
