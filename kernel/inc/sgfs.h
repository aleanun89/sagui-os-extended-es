// SPDX-License-Identifier: GPL-3.0-only
/* sgfs.h - lector del sistema de archivos SGFS (formato en tools/mksgfs.py). */
#ifndef SAGUI_SGFS_H
#define SAGUI_SGFS_H
#include "tipos.h"

#define SGFS_ARQUIVO  1
#define SGFS_PASTA    2
#define SGFS_PROGRAMA 3
#define SGFS_MAX_CAMINHO 56

typedef struct EMPACOTADO {
    char     caminho[SGFS_MAX_CAMINHO];  /* sin "/" inicial; "" sería la raíz */
    uint8_t  tipo;
    uint8_t  reservado[3];
    uint32_t tamanho;
    uint32_t desloc;
    uint32_t reservado2;
} sgfs_entrada_t;

bool  sgfs_iniciar(const void *base, uint32_t bytes);
uint32_t sgfs_quantidade(void);
const sgfs_entrada_t *sgfs_entrada(uint32_t i);

/* Ruta absoluta ("/docs/a.txt") o ya sin la barra. La raíz ("/") devuelve
 * una entrada fija del tipo carpeta. NULL si no existe. */
const sgfs_entrada_t *sgfs_procurar(const char *caminho);

/* Recorre los hijos directos de una carpeta: empiece con *pos = 0 y llame hasta
 * devolver NULL. */
const sgfs_entrada_t *sgfs_proximo_filho(const char *pasta, uint32_t *pos);

uint32_t    sgfs_ler(const sgfs_entrada_t *e, uint32_t desloc, void *destino, uint32_t n);
const char *sgfs_nome(const sgfs_entrada_t *e);   /* último componente de la ruta */
const uint8_t *sgfs_dados(const sgfs_entrada_t *e);

#endif
