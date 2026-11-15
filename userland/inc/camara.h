// SPDX-License-Identifier: GPL-2.0-or-later
/* camara.h - cámaras de la libsagui (D277): lo que el kernel entrega por INFO_CAMARA y la
 * conversión de los cuadros a imagen.
 *
 * Interfaz para cualquier programa (documentada en docs/camara.md):
 *   info_camara_t i;
 *   camara_estado(CAM_FUENTE_REAL, &i);          // i.presente, i.nome, i.n_modos, i.modos[]
 *   camara_abrir(CAM_FUENTE_REAL, modo, &i);     // empieza a capturar (una cámara, un proceso)
 *   n = camara_cuadro(&i, buf, tam, seq_visto);  // > 0: cuadro nuevo (i.seq, i.larg, i.alt, i.formato)
 *   camara_cerrar();
 * Los cuadros YUY2 (Y0 U Y1 V) o MJPEG (el JPEG entero) se convierten con camara_a_imagem. */
#ifndef SAGUI_CAMARA_H
#define SAGUI_CAMARA_H
#include "sagui.h"
#include "imagem.h"

/* Lo que devuelven las funciones: >= 0 bien; < 0 un ERRO_* del sistema. */
int  camara_estado(unsigned fuente, info_camara_t *i);
int  camara_abrir(unsigned fuente, unsigned modo, info_camara_t *i);
int  camara_cuadro(info_camara_t *i, void *buf, unsigned tam, unsigned seq_visto);
int  camara_cerrar(void);
int  camara_ajuste(unsigned control, bool escribir, int valor, info_camara_t *i);   /* i.valor/minimo/maximo/defecto */

/* Convierte un cuadro en imagen RGB: 0 = bien, <0 error de imagem_*. */
int  camara_a_imagem(const void *dados, unsigned n, unsigned larg, unsigned alt, unsigned formato, imagem_t *img);
/* YUY2 a 0x00RRGGBB (BT.601, rango limitado), en enteros. dst debe tener larg*alt píxeles. */
void camara_yuy2_a_rgb(const uint8_t *yuy2, unsigned larg, unsigned alt, uint32_t *dst);
/* Guarda la foto: PNG siempre; JPEG solo si el cuadro ya era MJPEG (no hay codificador). 0 = bien. */
int  camara_guardar(const char *caminho, const void *dados, unsigned n, unsigned larg, unsigned alt, unsigned formato);
const char *camara_nombre_formato(unsigned formato);

#endif
