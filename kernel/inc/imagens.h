// SPDX-License-Identifier: GPL-3.0-only
/* imagens.h - páginas de solo lectura de las bibliotecas compartidas entre procesos
 * (D378, fase 3; docs/enlace-dinamico.md §4 y §15). */
#ifndef SAGUI_IMAGENS_H
#define SAGUI_IMAGENS_H
#include "tipos.h"
#include "paginacao.h"

struct processo;

/* MEM_COMPARTIR: mapea en [ini, ini+n) del área de datos del proceso "l" (páginas aún sin
 * asignar) las páginas [desloc, desloc+n) del fichero abierto "fd", de solo lectura (y
 * ejecutables si "exec"). Si otro proceso ya tiene esas mismas páginas (mismo fichero y
 * versión, y el mismo contenido byte a byte), se comparten; si no, se crea el objeto. Lo
 * que pase del final del fichero queda a cero. Devuelve 1 si reutilizó un objeto, 0 si lo
 * creó, o un error (ERRO_ACESSO: fichero que no es de confianza). Con error, el rango queda
 * sin asignar o, si el fallo llega después de leer el fichero, con páginas propias normales
 * (lectura y escritura, sin ejecución): quien llama lo devuelve siempre con MEM_DEVOLVER. Solo ficheros ELF ET_DYN y tramos dentro de sus PT_LOAD sin
 * escritura (si no, ERRO_INVALIDO); con un tope global de una cuarta parte de la RAM
 * (ERRO_LIMITE). Solo desde una llamada al sistema del propio proceso. */
int32_t  imagens_mapear(struct processo *l, uint32_t ini, uint32_t n, uint32_t fd, bool exec, uint32_t desloc);
/* MEM_DEVOLVER: quita las páginas compartidas de [ini, fim) de "e" (con la TLB de las demás
 * CPU sincronizada) y suelta los objetos que se queden sin usar. Devuelve cuántas quitó. */
uint32_t imagens_desmapear(espaco_t e, uint32_t ini, uint32_t fim);
/* espaco_destruir(): el espacio "e" ya no lo usa ninguna CPU; se sueltan sus objetos. */
void     imagens_espaco_destruido(espaco_t e);
/* Páginas compartidas de "e" que NO se le cuentan: de cada objeto paga su parte
 * (mapeadas / procesos que lo usan); un objeto que solo usa él se le cuenta entero. */
uint32_t imagens_paginas_alheias(espaco_t e);

typedef struct {
    uint32_t objetos;        /* objetos imagen vivos */
    uint32_t paginas;        /* páginas físicas que ocupan (una vez) */
    uint32_t mapeadas;       /* páginas mapeadas sumando todos los procesos */
    uint32_t reutilizadas;   /* veces que un proceso reutilizó un objeto (desde el arranque) */
    uint32_t criadas;        /* objetos creados desde el arranque */
} imagens_est_t;
void     imagens_estatisticas(imagens_est_t *est);

#endif
