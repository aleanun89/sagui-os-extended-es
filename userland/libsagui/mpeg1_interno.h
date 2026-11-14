// SPDX-License-Identifier: GPL-2.0-or-later
/* mpeg1_interno.h - el demultiplexador MPEG-PS y el decodificador de vídeo
 * MPEG-1 (mejora 048, fase 2, D137).
 *
 * Un fichero .mpg es un "program stream" MPEG (ISO/IEC 11172-1): una serie de
 * paquetes que transportan el flujo elemental de vídeo (MPEG-1, ISO/IEC
 * 11172-2) y, si hay, el de audio. Este módulo hace las dos cosas:
 *
 *  - el demultiplexador: lee los paquetes del programa y separa el flujo de
 *    vídeo (y el de audio, para saber qué sonido tiene);
 *  - el decodificador: lee los cuadros del flujo de vídeo, con los cuadros B
 *    reordenados como pide la norma, y devuelve cada cuadro en píxeles RGB.
 *
 * Esto es interno de la libsagui: el programa de vídeo (video.c) es el que
 * habla con el usuario. */
#ifndef SAGUI_MPEG1_INTERNO_H
#define SAGUI_MPEG1_INTERNO_H
#include "video.h"

/* Lo que se sabe del vídeo después de abrir el archivo. */
typedef struct {
    int      larg, alt;        /* el tamaño del vídeo */
    int      fps;              /* los cuadros por segundo (redondeado) */
    int      quadros;          /* cuántos cuadros tiene el vídeo */
    bool     tem_audio;        /* el programa trae audio */
    uint32_t taxa_audio;       /* Hz del audio */
    int      canais_audio;     /* 1 o 2 */
} mpeg1_info_t;

typedef struct mpeg1 mpeg1_t;

/* Abre el archivo y lee las cabeceras. Devuelve NULL con *erro = un
 * VIDEO_ERRO_* (o un ERRO_* del sistema, si el archivo no se abrió). */
mpeg1_t *mpeg1_abrir(const char *caminho, mpeg1_info_t *info, int *erro);

/* Decodifica el cuadro siguiente (0 = ok, VIDEO_ERRO_FIM al final, otro
 * VIDEO_ERRO_* si el cuadro está estropeado). El cuadro va a img, que
 * puede soltarlo el propio módulo (como en el camino del AVI). */
int  mpeg1_quadro(mpeg1_t *m, imagem_t *img);

void mpeg1_fechar(mpeg1_t *m);

#endif
