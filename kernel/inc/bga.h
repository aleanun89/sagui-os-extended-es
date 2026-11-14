// SPDX-License-Identifier: GPL-2.0-or-later
/* bga.h - tarjeta de vídeo de Bochs/QEMU/VirtualBox ("dispi"): cambio de modo sin la BIOS (D059). */
#ifndef SAGUI_BGA_H
#define SAGUI_BGA_H
#include "tipos.h"

bool     bga_detectar(uint32_t memoria_vbe);   /* memoria_vbe: bytes que anunció el VBE (reserva) */
bool     bga_presente(void);
uint32_t bga_memoria(void);                    /* bytes de memoria de vídeo */
uint32_t bga_max_largura(void);
uint32_t bga_max_altura(void);
/* Activa anchura x altura con bpp bits (32, 16 o 15, D078); devuelve el pitch o 0
 * (y vuelve al modo anterior). */
uint32_t bga_mudar(uint32_t l, uint32_t a, uint32_t bpp, uint32_t l_antes, uint32_t a_antes, uint32_t bpp_antes);

#endif
