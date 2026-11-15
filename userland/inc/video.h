// SPDX-License-Identifier: GPL-2.0-or-later
/* video.h - decodificador de vídeo de la libsagui (mejora 048, D129).
 *
 * Abre un archivo de vídeo y lo entrega cuadro a cuadro, ya en píxeles
 * (imagem_t, el mismo formato que el visor y el resto de la biblioteca).
 *
 * Hoy se lee el vídeo MJPEG dentro de un AVI: cada cuadro es un JPEG
 * completo, que decodifica el lector de la mejora 047 (imagem_jpeg.c). El
 * MPEG-1 en MPEG-PS (el formato que recomienda la ficha de la mejora) se
 * reconoce, pero su decodificador todavía no está escrito: se avisa con
 * VIDEO_ERRO_NAO_SUPORTADO en vez de dar una imagen mala (D129).
 *
 * El audio del AVI (PCM sin comprimir, en los trozos '01wb') sale con
 * video_audio(): el PCM del intervalo que corresponde al cuadro recién
 * decodificado, para que el reproductor lo mande a la tarjeta de sonido y
 * sea el reloj maestro (si no hay tarjeta, el vídeo va en silencio).
 *
 * Uso:
 *
 *   int erro;
 *   video_t *v = video_abrir("/usb/pelicula.avi", &erro);
 *   if (!v) { ... video_erro_texto(erro) ... }
 *   const video_info_t *i = video_info(v);
 *   imagem_t img = { 0 };
 *   while (video_quadro(v, &img) == 0) {
 *       ... pintar img ...
 *       int16_t pcm[4096 * 2];
 *       int n = video_audio(v, pcm, 4096);
 *       ... tocar pcm ...
 *   }
 *   video_fechar(v);
 *
 * video_quadro() suelta la imagen anterior que hubiera en img (la libera), de
 * modo que el bucle no pierde memoria; después de la llamada, img es el cuadro
 * nuevo. Sólo hay un vídeo abierto por programa a la vez (el estado es
 * estático, como en los decodificadores de audio). */
#ifndef SAGUI_VIDEO_H
#define SAGUI_VIDEO_H
#include "sagui.h"
#include "imagem.h"

/* Errores propios (los de archivo son los ERRO_* del sistema). */
#define VIDEO_ERRO_FORMATO       (-220)   /* no es un AVI con vídeo */
#define VIDEO_ERRO_NAO_SUPORTADO (-221)   /* es un vídeo, pero de un formato que aún no se decodifica */
#define VIDEO_ERRO_CORROMPIDO    (-222)   /* datos inválidos o archivo cortado */
#define VIDEO_ERRO_MEMORIA       (-223)   /* no hay memoria para el búfer del cuadro */
#define VIDEO_ERRO_FIM           (-224)   /* no queda ningún cuadro */

typedef struct {
    int      larg, alt;        /* tamaño del vídeo */
    int      fps;              /* cuadros por segundo */
    int      quadros;          /* número de cuadros (0 = desconocido) */
    uint32_t duracao_ms;       /* duración estimada */
    bool     tem_audio;        /* el archivo lleva pista de audio */
    uint32_t taxa_audio;       /* Hz del audio */
    int      canais_audio;     /* 1 o 2 */
    const char *nome_formato;  /* "MJPEG en AVI", "H.264/AAC en MP4"... */
} video_info_t;

typedef struct video video_t;

/* Abre el archivo y lee sus cabeceras. Devuelve NULL con *erro = un
 * VIDEO_ERRO_* (o un ERRO_* del sistema si el archivo no se pudo abrir). */
video_t *video_abrir(const char *caminho, int *erro);
const video_info_t *video_info(video_t *v);

/* Decodifica el cuadro siguiente en img (0 = ok, VIDEO_ERRO_FIM al
 * terminar, otro VIDEO_ERRO_* si el cuadro está dañado). */
int  video_quadro(video_t *v, imagem_t *img);

/*PCM del audio que va con el cuadro actual: los trozos de audio leídos
 * hasta ahora menos los que ya se entregaron. Devuelve los cuadros de
 * audio entregados (0 si ahora mismo no hay audio). */
int  video_audio(video_t *v, int16_t *pcm, int max_quadros);

/* Salta al cuadro indicado (0 = el primero). */
int  video_buscar(video_t *v, int quadro);
int  video_quadro_atual(const video_t *v);    /* el cuadro que sale ahora */
int  video_total_quadros(const video_t *v);

void video_fechar(video_t *v);

/* MP4 (H.264 Baseline/Main + AAC-LC, mejora 100, D276). video_abrir() lo abre por la
 * cabecera, no por la extensión; video_abrir_memoria() es para quien ya lo tiene en un
 * búfer (el Cipó): los datos tienen que seguir vivos hasta video_fechar(). Solo MP4.
 * Con un MP4, video_quadro(v, NULL) decodifica y descarta el cuadro (saltarlo sin
 * gastar la conversión de color) y video_reducao(v, 2) entrega los cuadros a la mitad
 * de tamaño (más rápido); el audio sale siempre estéreo de 16 bits a taxa_audio. */
video_t *video_abrir_memoria(const uint8_t *dados, uint32_t n, int *erro);
void video_reducao(video_t *v, int fator);
const char *video_erro_texto(int erro);
bool video_extensao(const char *nome);   /* .avi .mpg .mpeg (sin distinguir mayúsculas) */

#endif
