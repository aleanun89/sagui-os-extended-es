// SPDX-License-Identifier: GPL-3.0-only
/* xfs.h - lectura de XFS (D446, fase 1: solo lectura). Implementación propia a partir de la
 * especificación pública «XFS Algorithms & Data Structures»; no se copió código de Linux ni de
 * xfsprogs. Las operaciones quedan en la tabla fs_xfs (montagem.h). */
#ifndef SAGUI_XFS_H
#define SAGUI_XFS_H
#include "tipos.h"
#include "abi.h"
#include "disco.h"

/* Lo que dice xfs_reconhecer del primer sector de un volumen. */
#define XFS_NAO       0   /* no es XFS */
#define XFS_SIM       1   /* XFS que SaguiOS sabe leer */
#define XFS_RECUSADO  2   /* XFS con funciones desconocidas o que no se admiten: no se monta */

/* ¿El sector 0 es un superbloque XFS? Con XFS_RECUSADO, "motivo" dice por qué (en español);
 * "rotulo" recibe la etiqueta (12 caracteres como mucho, sin los espacios del final). */
int xfs_reconhecer(const uint8_t *s0, char *motivo, size_t max_motivo, char *rotulo, size_t max_rotulo);

/* Tipos de un tramo del archivo (extent o hueco). */
#define XFS_EXT_DADOS       1   /* bloques escritos */
#define XFS_EXT_BURACO      2   /* hueco: se lee como ceros */
#define XFS_EXT_NAO_ESCRITO 3   /* extent reservado sin escribir («unwritten»): ceros */

/* Un tramo del archivo: [ini, ini + n) en bloques del archivo; "bloco" es el número de bloque
 * del sistema (xfs_fsblock_t, de 64 bits: grupo y bloque dentro del grupo) si son datos. */
typedef struct {
    uint64_t ini, n, bloco;
    uint8_t  tipo;
} xfs_extent_t;

/* Un archivo abierto (fs->tam_arq bytes): el inodo y el último tramo usado. */
typedef struct {
    uint64_t     ino;
    uint64_t     tam;          /* di_size (el VFS ve como mucho 4 GB - 1) */
    bool         ligacao;      /* enlace simbólico que no lleva a nada: se lee su destino como texto */
    bool         ext_valido;
    xfs_extent_t ext;
} xfs_arq_t;

#endif
