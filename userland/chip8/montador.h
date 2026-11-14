// SPDX-License-Identifier: GPL-2.0-or-later
/* montador.h - o montador do chip8asm (montador.c, D071). */
#ifndef C8_MONTADOR_H
#define C8_MONTADOR_H
#include "c8base.h"

/* Monta o arquivo; devolve 0 e a ROM (a partir de 0x200, aloca()) ou o
 * número de erros (já mostrados como "arquivo:linha: mensagem"). */
int c8_montar(const char *fonte, uint8_t **rom, int *tam);

#endif
