// SPDX-License-Identifier: GPL-3.0-only
/* memoria.h - páginas físicas y el asignador del kernel. */
#ifndef SAGUI_MEMORIA_H
#define SAGUI_MEMORIA_H
#include "tipos.h"

#define PAGINA        4096u
#define MEMFIS_LIMITE 0x78000000u   /* = KERNEL_JANELAS; ver docs/decisoes.md, D008 y D049 */

void     memfisica_iniciar(void);
uint32_t memfisica_alocar(void);              /* 0 si se acabó */
uint32_t memfisica_alocar_seguidas(uint32_t n);
void     memfisica_liberar(uint32_t endereco);
void     memfisica_liberar_seguidas(uint32_t endereco, uint32_t n);
uint32_t memfisica_livres(void);
uint32_t memfisica_geridas(void);

typedef struct {
    uint64_t ram_total;        /* toda la RAM libre según E820 (incluida por encima de 4 GB) */
    uint32_t paginas_geridas;  /* páginas que el bitmap puede entregar */
    uint32_t paginas_livres;
    uint32_t heap_paginas;     /* páginas usadas por las clases pequeñas del kmalloc */
    uint32_t heap_blocos_usados;
    uint32_t heap_grandes;     /* bloques grandes en uso */
} estat_memoria_t;

void memoria_estatisticas(estat_memoria_t *e);

void *kmalloc(size_t n);
void *kcalloc(size_t n);
void  kfree(void *p);

#endif
