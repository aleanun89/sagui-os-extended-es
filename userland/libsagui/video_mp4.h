// SPDX-License-Identifier: GPL-2.0-or-later
/* video_mp4.h - MP4 con H.264 y AAC-LC dentro de la libsagui (mejora 100, D276).
 *
 * Interno de la libsagui: video.c lo llama desde video_abrir() y compañía.
 * Junta el lector de MP4 (mp4.c), el decodificador H.264 (h264.c) y el AAC-LC
 * (aac.c), que vienen de robson-apk (D276), y los pone detrás de la misma API
 * que el AVI y el MPEG-1, para que el reproductor y el Cipó compartan el
 * decodificador (no hay dos copias). */
#ifndef SAGUI_VIDEO_MP4_H
#define SAGUI_VIDEO_MP4_H
#include "video.h"

typedef struct vmp4 vmp4_t;

/* ¿Los primeros bytes son los de un MP4 (caja ftyp/moov/mdat/free al principio)? */
bool vmp4_reconhece(const uint8_t *cab12);

/* Abre un MP4 de un archivo (se lee el moov; las muestras se leen al decodificar). */
vmp4_t *vmp4_abrir_arquivo(const char *caminho, video_info_t *info, int *erro);
/* Abre un MP4 que ya está en memoria; los datos tienen que seguir ahí hasta vmp4_fechar. */
vmp4_t *vmp4_abrir_memoria(const uint8_t *dados, uint32_t n, video_info_t *info, int *erro);

/* Siguiente cuadro en orden de exhibición. img == NULL: se decodifica y se descarta
 * (saltar un cuadro sin gastar la conversión de color). */
int  vmp4_quadro(vmp4_t *v, imagem_t *img);
int  vmp4_audio(vmp4_t *v, int16_t *pcm, int max_quadros);
int  vmp4_buscar(vmp4_t *v, int quadro);
int  vmp4_quadro_atual(const vmp4_t *v);
void vmp4_reducao(vmp4_t *v, int fator);   /* 1 (entero) o 2 (mitad): el cuadro sale reducido */
void vmp4_fechar(vmp4_t *v);

#endif
