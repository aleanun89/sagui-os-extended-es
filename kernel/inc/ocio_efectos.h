// SPDX-License-Identifier: GPL-2.0-or-later
/* ocio_efectos.h - los efectos del salvapantallas (D422).
 *
 * Dibujados por código, sin imágenes y sin biblioteca: solo enteros y un búfer de píxeles 0xRRGGBB. Los usa el
 * kernel (kernel/gui/ocio.c, a pantalla completa) y Configuración (la vista previa), que incluye el mismo .c.
 * Cada cuadro se dibuja entero a partir del estado y del tiempo, así que da igual cuántas veces se pida
 * (un equipo lento solo pierde cuadros, no el hilo de la animación). */
#ifndef SAGUI_OCIO_EFECTOS_H
#define SAGUI_OCIO_EFECTOS_H

#define OCIO_EF_NEGRO     0   /* pantalla en negro */
#define OCIO_EF_ESTRELLAS 1   /* viaje entre estrellas */
#define OCIO_EF_LOGOTIPO  2   /* el rótulo SAGUIOS rebotando */
#define OCIO_EF_TUBERIAS  3   /* tuberías que crecen por la pantalla */
#define OCIO_EF_RELOJ     4   /* la hora, grande, que cambia de sitio cada minuto */
#define OCIO_EF_N         5

#define OCIO_ESTRELLAS_MAX 260
#define OCIO_TUBOS_MAX     2400
#define OCIO_TUBERIAS_N    3

typedef struct { short x, y; unsigned char cor, cx; } ocio_tubo_t;   /* una casilla de tubería y de cuál viene */

typedef struct {
    int efecto;
    unsigned semilla;                 /* generador (xorshift) */
    unsigned ms_ultimo;               /* el tiempo del último cuadro */
    unsigned ms_inicio;
    /* estrellas */
    int ex[OCIO_ESTRELLAS_MAX], ey[OCIO_ESTRELLAS_MAX], ez[OCIO_ESTRELLAS_MAX];
    /* tuberías */
    ocio_tubo_t tubos[OCIO_TUBOS_MAX];
    int n_tubos;
    int tx[OCIO_TUBERIAS_N], ty[OCIO_TUBERIAS_N], td[OCIO_TUBERIAS_N], tc[OCIO_TUBERIAS_N];
    unsigned ms_tubo;
} ocio_ef_t;

/* Empieza un efecto (OCIO_EF_*) con una semilla y el tamaño del lienzo. */
void ocio_ef_iniciar(ocio_ef_t *e, int efecto, unsigned semilla, int w, int h, unsigned ms);
/* Dibuja un cuadro en pix (stride en píxeles). limpiar: pinta antes el fondo negro (el kernel ya lo hizo con
 * su rutina rápida y pasa 0). hora/min/seg para el reloj. */
void ocio_ef_cuadro(ocio_ef_t *e, unsigned *pix, int w, int h, int stride, unsigned ms, int limpiar,
                    int hora, int min, int seg);

#endif
