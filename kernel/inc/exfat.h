// SPDX-License-Identifier: GPL-2.0-or-later
/* exfat.h - lectura y escritura de exFAT (D325). Implementación propia a partir de la
 * especificación pública de Microsoft (2019); no se copió código de ningún otro sistema.
 * Las operaciones quedan en la tabla fs_exfat (montagem.h). */
#ifndef SAGUI_EXFAT_H
#define SAGUI_EXFAT_H
#include "tipos.h"
#include "abi.h"
#include "disco.h"

/* Cómo se llega a los datos de un archivo o carpeta: primer clúster, cuántos clústeres
 * tiene asignados, y si están seguidos sin cadena en la FAT (NoFatChain). "tam" es la
 * DataLength (satura en 4 GB: el VFS cuenta posiciones en 32 bits). */
typedef struct {
    uint32_t primeiro;       /* 0 = vacío */
    uint32_t ncl;
    uint32_t tam;
    uint32_t ultimo;         /* el último clúster, si se sabe (0 = no se sabe) */
    bool     sem_cadeia;
} exfat_fluxo_t;

/* Un archivo abierto: dónde está su grupo de entradas (carpeta, número de entrada y cuántas),
 * y sus datos. */
typedef struct {
    exfat_fluxo_t fl;
    exfat_fluxo_t pai;       /* la carpeta que lo contiene, como estaba al abrir */
    uint32_t idx, qtd;       /* entrada File dentro de la carpeta y entradas del grupo */
    uint32_t vdl;            /* ValidDataLength: lo que hay detrás se lee como ceros */
    uint32_t cache_n, cache_cl;   /* el clúster número cache_n del archivo es cache_cl */
} exfat_arq_t;

bool exfat_reconhecer(const uint8_t *s0);              /* ¿el sector 0 es un arranque exFAT? */
bool exfat_rotulo_raiz(disco_t *d, char *rotulo);      /* etiqueta (entrada 0x83 de la raíz), en Latin-1 */

#endif
