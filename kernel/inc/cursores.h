/* SPDX-License-Identifier: GPL-3.0-only */
/* cursores.h - los temas y tamaños del cursor del ratón (D482).
 *
 * Una sola forma (la flecha de 12x19 de siempre, con la punta en la esquina) y tres temas y tres
 * tamaños. El kernel dibuja el cursor con cursor_pixel() y Configuración dibuja con lo mismo su vista
 * previa, así que lo que se ve en la página es lo que sale en pantalla. Los píxeles son 0xAARRGGBB:
 * alfa 0 = transparente, 255 = opaco y lo de en medio se mezcla con lo que hay debajo. */
#ifndef CURSORES_H
#define CURSORES_H

#include <stdint.h>

#define CURSOR_TEMAS    3   /* 0 clásico (negro y blanco), 1 oscuro, 2 moderno (azul con alfa) */
#define CURSOR_TAMANOS  3   /* 0 normal (x1), 1 grande (x2), 2 muy grande (x3) */
#define CURSOR_BASE_L   12
#define CURSOR_BASE_A   19

/* SIS_CURSOR: operaciones. LER devuelve tema | tamaño << 8 | ancho << 16 | alto << 24 (en píxeles). */
#define CURSOR_OP_LER    0
#define CURSOR_OP_FIJAR  1   /* (tema, tamaño) */

static const char *const cursor_forma_base[CURSOR_BASE_A] = {
    "#           ", "##          ", "#.#         ", "#..#        ", "#...#       ",
    "#....#      ", "#.....#     ", "#......#    ", "#.......#   ", "#........#  ",
    "#.........# ", "#......#####", "#...#..#    ", "#..# #..#   ", "#.#  #..#   ",
    "##    #..#  ", "#     #..#  ", "       ##   ", "            ",
};

static inline int cursor_escala(int tamano) { return tamano < 0 ? 1 : tamano >= CURSOR_TAMANOS ? CURSOR_TAMANOS : tamano + 1; }
static inline int cursor_ancho(int tamano) { return CURSOR_BASE_L * cursor_escala(tamano); }
static inline int cursor_alto(int tamano)  { return CURSOR_BASE_A * cursor_escala(tamano); }

/* El píxel (i, j) del cursor ya a su tamaño (0 <= i < ancho, 0 <= j < alto). */
static inline uint32_t cursor_pixel(int tema, int tamano, int i, int j)
{
    int e = cursor_escala(tamano);
    char p = cursor_forma_base[j / e][i / e];
    if (p == '#') return tema == 1 ? 0xFFFFFFFFu : tema == 2 ? 0xFFFFFFFFu : 0xFF000000u;
    if (p == '.') return tema == 1 ? 0xFF1B1F24u : tema == 2 ? 0xE02A6FD8u : 0xFFFFFFFFu;
    return 0;
}

#endif
