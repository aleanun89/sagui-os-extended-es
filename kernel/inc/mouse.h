// SPDX-License-Identifier: GPL-3.0-only
/* mouse.h - mouse/touchpad PS/2 (H19). */
#ifndef SAGUI_MOUSE_H
#define SAGUI_MOUSE_H
#include "tipos.h"

#define BOTAO_ESQUERDO 1
#define BOTAO_DIREITO  2
#define BOTAO_MEIO     4

typedef struct {
    int16_t  x, y;          /* posición en la pantalla tras el movimiento */
    int16_t  dx, dy;
    uint8_t  botoes;        /* estado actual de los botones */
    uint8_t  mudaram;       /* botones que cambiaron en este paquete */
    int8_t   dz;            /* rueda: muescas, + hacia abajo (D048) */
    int8_t   dw;            /* rueda horizontal: muescas, + hacia la derecha (D407, panel táctil) */
} evento_mouse_t;

bool mouse_iniciar(void);
bool mouse_tentar(evento_mouse_t *e);   /* siguiente evento, si lo hay */
void mouse_posicao(int *x, int *y);
const void *mouse_canal(void);
void mouse_formato(int id);           /* lo llama el i8042: id 3 = IntelliMouse (paquete de 4 bytes) */
bool mouse_tem_roda(void);
void mouse_tela_mudou(int largura, int altura);   /* cambio de modo de vídeo (D059) */
/* Ratón y tableta USB (D214), con las interrupciones desactivadas: movimiento
 * relativo (y crece hacia abajo, rueda + hacia abajo) o posición absoluta. */
void mouse_usb(int dx, int dy, int dz, uint8_t botoes);
void mouse_usb_absoluto(uint32_t x, uint32_t xmax, uint32_t y, uint32_t ymax, int dz, uint8_t botoes);
void mouse_usb_presente(bool com_roda);
/* D242: captura del ratón. Con ella, el cursor queda quieto en (x, y) y no se dibuja, y los
 * eventos llevan desplazamientos crudos en dx, dy. */
void mouse_captura(bool lig, int x, int y);
bool mouse_capturado(void);
/* D251: entrada del escritorio remoto. Posición absoluta en la pantalla (se recorta) y
 * botones BOTAO_*; mouse_injetar_aqui deja el cursor donde está (rueda, soltar botones). */
void mouse_injetar(int x, int y, uint8_t botoes);
void mouse_injetar_aqui(uint8_t botoes, int dz);
uint8_t mouse_botoes(void);
/* D407: lo que sale de los gestos del panel táctil (painel.c), con las interrupciones desactivadas:
 * desplazamiento (y hacia abajo), rueda vertical y horizontal y el estado de los botones. */
void mouse_painel(int dx, int dy, int rv, int rh, uint8_t botoes);

#endif
