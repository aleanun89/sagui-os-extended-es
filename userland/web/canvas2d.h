/* canvas2d.h - telas do Canvas 2D (melhoria 081).
 *
 * Vem do mesmo sitio que o motor (ver userland/web/ORIGEM.md). Uma tela e um
 * bloco de pixels ARGB sem pre-multiplicar; o browser cria uma tela por
 * <canvas> e o motor pinta nela.
 */
#ifndef SAGUI_WEB_CANVAS2D_H
#define SAGUI_WEB_CANVAS2D_H

#include "web.h"

const uint32_t *tela2d_pixels(int id, int *l, int *a);
uint32_t *tela2d_px_escrita(int id, int *l, int *a);   /* para pintar nela (iframes) */
unsigned tela2d_versao(void);            /* muda a cada desenho */
void tela2d_liberar_todas(void);

#endif
