// SPDX-License-Identifier: GPL-3.0-only
/* fat32.h - lectura y escritura de FAT32 (H32, H33). Ver D026 y D040.
 * Las operaciones quedan en la tabla fs_fat32 (montagem.h). */
#ifndef SAGUI_FAT32_H
#define SAGUI_FAT32_H
#include "tipos.h"
#include "abi.h"
#include "disco.h"

/* Un archivo abierto en FAT32: dónde está su entrada en la carpeta, el primer
 * clúster y el tamaño. */
typedef struct {
    uint32_t setor_entrada;    /* sector (del volumen) de la entrada corta en la carpeta */
    uint32_t desloc_entrada;   /* desplazamiento dentro del sector */
    uint32_t primeiro;         /* primer clúster (0 = archivo vacío) */
    uint32_t tamanho;
    bool     mudou;
    uint32_t cache_n, cache_cl;   /* último clúster encontrado: el n-ésimo es cache_cl */
} fat_arq_t;

bool fat_rotulo_raiz(disco_t *d, char *rotulo);   /* etiqueta de la entrada especial de la raíz */

#endif
