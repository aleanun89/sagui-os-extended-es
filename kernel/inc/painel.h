// SPDX-License-Identifier: GPL-3.0-only
/* painel.h - paneles táctiles PS/2: Synaptics, ALPS y Elantech en modo absoluto, con gestos (D407).
 * Lo usan i8042.c (reconocerlo al arrancar), mouse.c (los bytes del ratón) y las llamadas al sistema
 * (INFO_PAINEL). Si no hay panel reconocido, o algo falla, el ratón sigue como PS/2 de siempre. */
#ifndef SAGUI_PAINEL_H
#define SAGUI_PAINEL_H
#include "tipos.h"

/* Con las interrupciones desactivadas y el ratón callado (tras 0xF6): reconoce el panel y, si se puede
 * (permitir y la persona no pidió el modo PS/2), lo pasa al modo absoluto. true = paquetes de 6 bytes.
 * Si no, deja el ratón con sus valores por defecto, como estaba. */
bool painel_detectar(bool permitir);
void painel_desligar(void);        /* volver a tratar los bytes como ratón PS/2 (el dispositivo ya está en relativo) */
bool painel_ativo(void);           /* los bytes del ratón son del panel en modo absoluto */
const char *painel_nome(void);
void painel_byte(uint8_t b);       /* un byte del ratón (lo llama mouse_byte) */
void painel_tique(void);           /* en cada tic del reloj */
int32_t painel_info(uint32_t ptr, uint32_t n);     /* INFO_PAINEL */
int32_t painel_gravar(uint32_t ptr, uint32_t n);   /* INFO_PAINEL_GRAVAR */

#endif
