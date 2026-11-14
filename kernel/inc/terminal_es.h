// SPDX-License-Identifier: GPL-3.0-only
/* terminal_es.h - el terminal como archivo (entrada 0 y salida 1/2 de los procesos). */
#ifndef SAGUI_TERMINAL_ES_H
#define SAGUI_TERMINAL_ES_H
#include "tipos.h"

int32_t terminal_ler(char *destino, uint32_t n);          /* puede devolver SIS_REPETIR */
int32_t terminal_escrever(const char *origem, uint32_t n);
void    terminal_modo(int modo);
void    terminal_limpar(void);

#endif
