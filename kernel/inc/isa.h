// SPDX-License-Identifier: GPL-2.0-or-later
/* isa.h - el bus ISA: el controlador de DMA 8237, la memoria baja para él y la lista de
 * dispositivos ISA detectados (D302). Lo usa la Sound Blaster 16 y lo podrán usar otros
 * drivers ISA de la época (mejora 126). */
#ifndef SAGUI_ISA_H
#define SAGUI_ISA_H
#include "tipos.h"

/* ------------------------------------------------------------ DMA 8237 */

/* Canales: 0-3 de 8 bits (el 2 es la disquetera), 5-7 de 16 bits (el 4 es la cascada).
 * Un canal de 8 bits transfiere como mucho 64 KB y uno de 16 bits 128 KB, sin cruzar un
 * límite de 64 KB (128 KB en los de 16 bits), y solo por debajo de 16 MB. */
#define ISA_DMA_LER_MEM    0x08   /* de la memoria al dispositivo (reproducir) */
#define ISA_DMA_ESCREVER_MEM 0x04 /* del dispositivo a la memoria (grabar) */
#define ISA_DMA_AUTO       0x10   /* auto-init: al acabar vuelve a empezar */

/* En el arranque, justo después de memfisica_iniciar: reserva ISA_DMA_RESERVA bytes por debajo de
 * 16 MB alineados a 128 KB (aún queda memoria baja libre). Sin memoria, los drivers ISA no tienen DMA. */
#define ISA_DMA_RESERVA (128u * 1024u)
void     isa_dma_reservar(void);

/* Un trozo de la reserva que no cruza el límite del canal (64 KB en los de 8 bits, 128 KB en los
 * de 16), visto por la CPU sin caché. -> dirección física (0 = no queda) y *cpu el alias. */
uint32_t isa_dma_bufer(uint32_t bytes, bool de16, void **cpu);

/* Programa el canal (enmascarado al acabar: se activa con isa_dma_ativar). false = canal o
 * búfer no válidos (ver arriba). */
bool     isa_dma_programar(uint8_t canal, uint32_t fis, uint32_t bytes, uint8_t modo);
void     isa_dma_ativar(uint8_t canal, bool sim);   /* quita (true) o pone la máscara */
uint32_t isa_dma_feito(uint8_t canal);    /* bytes ya transferidos desde el principio del búfer */
bool     isa_dma_terminou(uint8_t canal); /* llegó al final (TC) desde la última lectura del estado */
bool     isa_dma_canal_valido(uint8_t canal);

/* ------------------------------------------------------------ dispositivos */

#define ISA_DISPOSITIVOS 8
void isa_registrar(const char *nome, uint16_t porta, uint8_t irq, uint8_t dma8, uint8_t dma16, const char *detalhe);
int32_t isa_info(uint32_t ptr, uint32_t n);   /* INFO_ISA */

#endif
