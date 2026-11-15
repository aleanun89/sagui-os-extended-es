/* SPDX-License-Identifier: GPL-3.0-only
 * search.h - hsearch mínimo (tabla con claves de texto) para openrsync (D314). */
#ifndef RSYNC_SEARCH_H
#define RSYNC_SEARCH_H
#include <stddef.h>
typedef struct { char *key; void *data; } ENTRY;
typedef enum { FIND, ENTER } ACTION;
int    hcreate(size_t n);
void   hdestroy(void);
ENTRY *hsearch(ENTRY e, ACTION a);
#endif
