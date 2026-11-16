// SPDX-License-Identifier: GPL-3.0-only
/* memoria.h - páginas físicas y el asignador del kernel. */
#ifndef SAGUI_MEMORIA_H
#define SAGUI_MEMORIA_H
#include "tipos.h"

#define PAGINA        4096u
#define MEMFIS_LIMITE 0x78000000u   /* = KERNEL_JANELAS; ver docs/decisoes.md, D008 y D049 */

/* D348/D349: tres zonas de páginas físicas.
 *   baja     [0, MEMFIS_LIMITE): el kernel la ve 1:1; estructuras del kernel, tablas
 *            de páginas, kmalloc, DMA, caché de disco sin alta y píxeles de las ventanas.
 *   alta     [MEMFIS_LIMITE, 4 GB): páginas de los programas y datos de la caché de
 *            disco (D350); el kernel la toca por ventanas temporales (memalta.h), nunca
 *            por su dirección.
 *   superior [4 GB, 64 GB) (D349, solo con PAE): como la alta.
 * memfisica_alocar*() dan SIEMPRE memoria baja (la dirección sirve de puntero);
 * memfisica_alocar_usuario() da superior o alta si queda y, si no, baja.
 * Las cuentas "alta" incluyen la superior (todo lo que solo es de los programas). */
void     memfisica_iniciar(void);
uint32_t memfisica_alocar(void);              /* baja; 0 si se acabó */
uint32_t memfisica_alocar_seguidas(uint32_t n);
uint32_t memfisica_alocar_baixa(uint32_t n, uint32_t alinhamento, uint32_t limite);   /* D302: DMA ISA */
uint64_t memfisica_alocar_usuario(void);      /* D348/D349: alta o superior si hay; NO usar como puntero */
void     memfisica_liberar(uint64_t endereco);   /* de cualquiera de las zonas */
void     memfisica_liberar_seguidas(uint64_t endereco, uint32_t n);
uint32_t memfisica_livres(void);              /* zona baja (lo que puede usar el kernel) */
uint32_t memfisica_geridas(void);             /* zona baja */
uint32_t memfisica_livres_alta(void);         /* D348: zonas alta y superior */
uint32_t memfisica_geridas_alta(void);
uint32_t memfisica_livres_superior(void);     /* D349: solo la superior (por encima de 4 GB) */
uint32_t memfisica_geridas_superior(void);
bool     memfisica_sobre_ram(uint64_t ini, uint64_t fim);   /* D348: ¿toca RAM libre del E820? */
/* D350: páginas de la alta/superior para la caché de disco (NO usar como puntero: ventanas de
 * memalta.h). 0 si no hay alta o si quedan "reserva" páginas libres o menos. */
uint64_t memfisica_alocar_alta(uint32_t reserva);
/* D350: memfisica_alocar_usuario() sin páginas pide a "f" que devuelva unas "paginas" (la caché
 * de disco, sus bloques limpios) antes de pasar a la baja o de fallar. "f" devuelve cuántas
 * liberó; no puede dormir ni esperar ningún cerrojo (si su cerrojo está tomado, devuelve 0). */
void     memfisica_registar_reclamo(uint32_t (*f)(uint32_t paginas));
/* D352: la swap comprimida. Con ella, memfisica_alocar_usuario() no baja de "reserva_baixa"
 * páginas bajas libres (las del kernel) sin antes sacar páginas a la swap con "f" (que no
 * duerme ni pide páginas de programa; devuelve las liberadas). Solo si la swap no libera
 * nada se usa la baja entera, como sin swap. */
void     memfisica_registar_troca(uint32_t (*f)(uint32_t paginas), uint32_t reserva_baixa);
/* D352: páginas que los programas pueden tomar sin tocar la reserva baja del kernel. */
uint32_t memfisica_livres_programas(void);
/* D352: pide al reclamador (la caché de disco) hasta "paginas"; devuelve las devueltas. */
uint32_t memfisica_reclamar(uint32_t paginas);

typedef struct {
    uint64_t ram_total;        /* toda la RAM libre según E820 (incluida por encima de 4 GB) */
    uint32_t paginas_geridas;  /* páginas que los bitmaps pueden entregar (baja + alta, D348) */
    uint32_t paginas_livres;   /* baja + alta */
    uint32_t altas_geridas;    /* D348: de ellas, en la zona alta */
    uint32_t altas_livres;
    uint32_t superiores_geridas;   /* D349: de las altas, por encima de 4 GB */
    uint32_t superiores_livres;
    uint32_t heap_paginas;     /* páginas usadas por las clases pequeñas del kmalloc */
    uint32_t heap_blocos_usados;
    uint32_t heap_grandes;     /* bloques grandes en uso */
} estat_memoria_t;

void memoria_estatisticas(estat_memoria_t *e);

void *kmalloc(size_t n);
void *kcalloc(size_t n);
void  kfree(void *p);

#endif
