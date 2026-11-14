// SPDX-License-Identifier: GPL-2.0-or-later
/* audio_formato.h - decodificadores de audio de la libsagui (mejora 053, D098).
 *
 * Abre un archivo de sonido, descubre el formato por el CONTENIDO (no por la
 * extensión) y entrega PCM de 16 bits intercalado, por bloques, en el momento de
 * reproducirlo. Formatos: WAV PCM (8, 16, 24 y 32 bits), WAV IMA ADPCM (0x11), MP3
 * (MPEG-1 y MPEG-2 Layer III, con etiquetas ID3 y cabecera Xing/Info) y FLAC
 * (de 4 a 24 bits), y MIDI estándar (.mid/.kar) sintetizado a PCM estéreo de 44100 Hz (audio_midi.h). El mismo código compila en el anfitrión (-DAUDIO_HOST, ver
 * tools/audio_host.h) para las pruebas rápidas.
 *
 * Solo un archivo abierto por programa a la vez (el estado es estático: el MP3
 * y el FLAC tienen búferes grandes y los programas de sonido reproducen una sola cosa).
 *
 *   int erro;
 *   audio_arquivo_t *a = audio_arquivo_abrir("/usb/musica.flac", &erro);
 *   const audio_info_t *i = audio_arquivo_info(a);      // tasa, canales, duración, etiquetas
 *   int16_t pcm[2048 * 2];
 *   int n;
 *   while ((n = audio_arquivo_ler(a, pcm, 2048)) > 0) ...   // n cuadros (canales intercalados)
 *   audio_arquivo_fechar(a);
 *
 * audio_arquivo_ler devuelve 0 al final del archivo y un AUDIO_ERRO_* (negativo)
 * si el archivo está dañado o cortado; lo que ya se entregó vale. */
#ifndef AUDIO_FORMATO_H
#define AUDIO_FORMATO_H
#ifdef AUDIO_HOST
#include "audio_host.h"
#else
#include "sagui.h"
#endif

#define AUDIO_FMT_NENHUM 0
#define AUDIO_FMT_WAV    1   /* PCM */
#define AUDIO_FMT_ADPCM  2   /* WAV con IMA ADPCM */
#define AUDIO_FMT_MP3    3
#define AUDIO_FMT_FLAC   4
#define AUDIO_FMT_MIDI   5   /* MIDI estándar, sintetizado (audio_midi.h, D190) */

#define AUDIO_ERRO_FORMATO       (-110)   /* no es un archivo de sonido que conozca */
#define AUDIO_ERRO_NAO_SUPORTADO (-111)   /* lo es, pero una variante que no decodifico */
#define AUDIO_ERRO_CORROMPIDO    (-112)   /* datos inválidos en medio del archivo */
#define AUDIO_ERRO_TRUNCADO      (-113)   /* el archivo termina antes del final */
#define AUDIO_ERRO_OCUPADO       (-114)   /* ya hay un archivo abierto */

typedef struct {
    int      formato;            /* AUDIO_FMT_* */
    const char *nome_formato;    /* "MP3 (MPEG-1 Layer III)", "FLAC", ... */
    uint32_t taxa;               /* Hz */
    uint32_t canais;             /* 1 o 2 (lo que sale de audio_arquivo_ler) */
    uint32_t bits_origem;        /* profundidad del archivo (16, 24, 4 del ADPCM...); 0 = con pérdidas */
    uint32_t quadros;            /* muestras por canal; 0 = desconocido */
    uint32_t duracao_ms;         /* 0 = desconocida */
    uint32_t kbps;               /* tasa de bits media (kbit/s); 0 = desconocida */
    bool     vbr;                /* MP3 de tasa variable */
    bool     sem_perdas;
    uint32_t quadros_ruins;      /* cuadros MP3 dañados que se han saltado hasta ahora */
    char     titulo[96];         /* etiquetas (ISO-8859-1); vacías si no hay */
    char     artista[96];
    char     album[96];
} audio_info_t;

typedef struct audio_arquivo audio_arquivo_t;

/* Formato por el contenido de los primeros bytes (n >= 12 es suficiente; MP3 con ID3 necesita más). */
int audio_detectar(const uint8_t *buf, unsigned n);

audio_arquivo_t *audio_arquivo_abrir(const char *caminho, int *erro);
const audio_info_t *audio_arquivo_info(const audio_arquivo_t *a);
int  audio_arquivo_ler(audio_arquivo_t *a, int16_t *pcm, unsigned quadros);
void audio_arquivo_fechar(audio_arquivo_t *a);
const char *audio_erro_texto(int erro);

/* Salto (D115): coloca la lectura en el instante ms. WAV PCM y ADPCM, exacto; FLAC y MP3,
 * en el cuadro más cercano (por la proporción de bytes: aproximado en los MP3 de tasa
 * variable). Devuelve 0 o un AUDIO_ERRO_*; audio_arquivo_posicao da los cuadros que se
 * han entregado desde el principio (tras un salto, el instante pedido). */
int audio_arquivo_buscar(audio_arquivo_t *a, uint32_t ms);
uint32_t audio_arquivo_posicao(const audio_arquivo_t *a);

#endif
