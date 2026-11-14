// SPDX-License-Identifier: GPL-2.0-or-later
/* rapido.h - copias y rellenos grandes de la interfaz gráfica (D042).
 *
 * Cada operación elige, en el momento, entre "rep movsd/stosd" y SSE2. El SSE2 solo
 * se usa dentro de un rapido_inicio()/rapido_fim() (que abren el
 * fpu_kernel_inicio/fim) y si la medida del arranque (rapido_iniciar) dijo que
 * es más rápido en esta máquina; fuera de eso, rep movsd/stosd. */
#ifndef SAGUI_RAPIDO_H
#define SAGUI_RAPIDO_H
#include "tipos.h"

void rapido_iniciar(void);   /* después de fpu_iniciar y video_iniciar: mide y elige */
void rapido_medir_tela(void);  /* de nuevo, solo la copia a la tarjeta (después de cambiar de modo) */
bool rapido_inicio(void);    /* true: abrió un fpu_kernel_inicio (cierre con rapido_fim) */
void rapido_fim(bool aberto);
void rapido_copiar(void *d, const void *o, size_t n);          /* RAM -> RAM */
void rapido_copiar_tela(void *d, const void *o, size_t n);     /* RAM -> tarjeta gráfica */
void rapido_preencher32(uint32_t *d, uint32_t cor, size_t n);  /* n píxeles */
/* n píxeles 0x00RRGGBB -> tarjeta de 16 bits (RGB565) o 15 (RGB555, r555), D078 */
void rapido_converter16(uint16_t *d, const uint32_t *o, size_t n, bool r555);
int  rapido_descrever(char *s, int max);                       /* "copia sse2, pantalla sse2-nt, ..." */

#endif
