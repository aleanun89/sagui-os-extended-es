// SPDX-License-Identifier: GPL-3.0-or-later
/* audio_conversor.h - lo que el Conversor de audio necesita además de los decodificadores (D339).
 *
 * Todo con aritmética entera (la libsagui no usa la FPU) y por bloques, para convertir un archivo
 * largo sin cargarlo entero:
 *   - cv_rem_*: cambio de canales (mono <-> estéreo) y de frecuencia de muestreo. Al bajar la
 *     frecuencia promedia (integra) cada intervalo de entrada, que es un filtro paso bajo sencillo
 *     y evita casi todo el aliasing; al subirla interpola en línea recta;
 *   - cv_wav_*: WAV PCM de 8 o 16 bits;
 *   - cv_flac_*: codificador FLAC sin pérdida propio (predictores fijos de orden 0 a 4, partición
 *     Rice, decorrelación estéreo izquierda/lado/derecha/medio), con niveles de compresión 0 a 8;
 *   - el MP3 lo hace shine (userland/terceros/shine/, LGPL-2.0+), que se enlaza solo en el programa.
 * La salida va por una «tubería» de dos llamadas (escribir y parchear) para no depender de dónde
 * se guarda: un archivo del sistema, o la memoria en las pruebas del anfitrión (-DAUDIO_HOST). */
#ifndef AUDIO_CONVERSOR_H
#define AUDIO_CONVERSOR_H
#ifdef AUDIO_HOST
#include "audio_host.h"
#else
#include "sagui.h"
#endif

/* Destino de lo codificado. escribir añade al final; parchear reescribe n bytes desde «desde»
 * (las cabeceras que llevan el tamaño, que solo se conoce al final). 0 = bien, <0 = error. */
typedef struct {
    int (*escribir)(void *ctx, const void *dados, unsigned n);
    int (*parchear)(void *ctx, unsigned desde, const void *dados, unsigned n);
    void *ctx;
} cv_salida_t;

/* ---- canales y frecuencia ---- */
typedef struct {
    uint32_t taxa_in, taxa_out, can_in, can_out;
    uint32_t paso;                /* taxa_in / taxa_out en Q16 */
    bool bajando;                 /* true: se promedia; false: se interpola */
    bool primeiro;
    int32_t ant[2];               /* subida: la muestra anterior */
    uint32_t fase;                /* subida: posición entre ant y la siguiente (Q16) */
    int64_t acc[2];               /* bajada: suma ponderada de lo que llevamos del intervalo */
    uint32_t falta;               /* bajada: lo que falta del intervalo (Q16 de cuadro de entrada) */
} cv_rem_t;

void     cv_rem_iniciar(cv_rem_t *r, uint32_t taxa_in, uint32_t can_in, uint32_t taxa_out, uint32_t can_out);
/* Cuadros de salida como mucho para n de entrada. */
unsigned cv_rem_max(const cv_rem_t *r, unsigned n);
/* Convierte n cuadros (canales intercalados); devuelve los cuadros de salida escritos en «saida». */
unsigned cv_rem_procesar(cv_rem_t *r, const int16_t *entrada, unsigned n, int16_t *saida);

/* ---- WAV ---- */
typedef struct { cv_salida_t s; uint32_t taxa, canais, bits, bytes; } cv_wav_t;
int cv_wav_iniciar(cv_wav_t *w, cv_salida_t s, uint32_t taxa, uint32_t canais, uint32_t bits);   /* bits: 8 o 16 */
int cv_wav_escrever(cv_wav_t *w, const int16_t *pcm, unsigned quadros);
int cv_wav_fechar(cv_wav_t *w);

/* ---- FLAC ---- */
#define CV_FLAC_NIVEL_MAX 8
typedef struct cv_flac cv_flac_t;
cv_flac_t *cv_flac_iniciar(cv_salida_t s, uint32_t taxa, uint32_t canais, int nivel);   /* NULL = sin memoria o parámetros fuera de rango */
int cv_flac_escrever(cv_flac_t *f, const int16_t *pcm, unsigned quadros);
int cv_flac_fechar(cv_flac_t *f);   /* codifica lo que quede, parchea STREAMINFO y libera */

#endif
