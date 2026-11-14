// SPDX-License-Identifier: GPL-2.0-or-later
/* audio_interno.h - estado compartido de los decodificadores (solo lo usan audio_*.c). */
#ifndef AUDIO_INTERNO_H
#define AUDIO_INTERNO_H
#include "audio_formato.h"

/* Lector con búfer sobre el descriptor de archivo. */
#define FL_TAM 16384
typedef struct {
    int      fd;
    uint8_t  buf[FL_TAM];
    uint32_t ini, fim;       /* bytes válidos en buf[ini..fim) */
    uint32_t pos;            /* posición en el archivo del byte buf[ini] */
    uint32_t tam;            /* tamaño del archivo */
    bool     fim_arq;
} fluxo_t;

void     fl_iniciar(fluxo_t *f, int fd);
uint32_t fl_garantir(fluxo_t *f, uint32_t n);        /* deja n bytes seguidos en buf+ini (salvo al final); devuelve cuántos hay */
uint32_t fl_ler(fluxo_t *f, void *dst, uint32_t n);  /* copia hasta n bytes; devuelve cuántos */
bool     fl_pular(fluxo_t *f, uint32_t n);
bool     fl_posicionar(fluxo_t *f, uint32_t pos);
int      fl_byte(fluxo_t *f);                         /* -1 al final */
static inline const uint8_t *fl_ptr(const fluxo_t *f) { return f->buf + f->ini; }
static inline void fl_consumir(fluxo_t *f, uint32_t n) { f->ini += n; f->pos += n; }

/* ---- IMA ADPCM ---- */
#define ADPCM_BLOCO_MAX 4096
typedef struct {
    uint32_t bloco, amostras_bloco, canais;
    uint32_t restam;                        /* bytes de datos que faltan */
    uint32_t total;                         /* bytes de datos en total */
    uint8_t  dados[ADPCM_BLOCO_MAX];
    int16_t  pcm[2 * (ADPCM_BLOCO_MAX * 2)];   /* un bloque decodificado, intercalado */
    uint32_t pcm_n, pcm_i;                  /* cuadros en el bloque y ya entregados */
} adpcm_t;

/* ---- FLAC ---- */
#define FLAC_BLOCO_MAX 16384
#define FLAC_CAN_MAX   2
typedef struct {
    uint32_t bits, bloco_max;
    int32_t  can[FLAC_CAN_MAX][FLAC_BLOCO_MAX];
    uint32_t restam;                         /* cuadros que faltan según el STREAMINFO (0 = no se sabe) */
    uint32_t amostra;                        /* muestra en que empieza el último cuadro decodificado */
    uint32_t n, i;                           /* cuadros en el bloque decodificado y ya entregados */
    uint32_t canais_arq;                     /* canales del archivo (puede ser 1 o 2; más, no soportado) */
    /* lector de bits */
    uint32_t acc, nacc;
    uint16_t crc16;
    uint8_t  crc8;
    bool     erro_fim;
} flac_t;

/* ---- MP3 ---- */
typedef struct mp3_estado mp3_t;

struct audio_arquivo {
    audio_info_t info;
    fluxo_t fl;
    int      fd;
    bool     em_uso;
    int      fmt;
    uint32_t entregues;         /* cuadros ya entregados */
    uint32_t inicio_dados;      /* donde empiezan los datos (después de las etiquetas / cabecera) */
    int      pend;              /* error que quedó para la siguiente llamada (ya se entregó lo que había antes de él) */
    /* WAV PCM */
    uint32_t pcm_bq, pcm_bits, pcm_restam;
    adpcm_t *adpcm;
    flac_t  *flac;
    mp3_t   *mp3;
    struct midi *midi;
};

int adpcm_buscar(audio_arquivo_t *a, uint32_t quadro);
int flac_buscar(audio_arquivo_t *a, uint32_t quadro);
int mp3_buscar(audio_arquivo_t *a, uint32_t quadro);
/* n * q / total sin divisiones de 64 bits (aproximado: sirve para colocar el cursor en un archivo). */
uint32_t audio_proporcao(uint32_t n, uint32_t q, uint32_t total);
int adpcm_abrir(audio_arquivo_t *a, uint32_t canais, uint32_t taxa, uint32_t bloco, uint32_t amostras, uint32_t dados);
int adpcm_ler(audio_arquivo_t *a, int16_t *pcm, unsigned n);
int flac_abrir(audio_arquivo_t *a);
int flac_ler(audio_arquivo_t *a, int16_t *pcm, unsigned n);
int mp3_abrir(audio_arquivo_t *a);
int mp3_ler(audio_arquivo_t *a, int16_t *pcm, unsigned n);

/* Etiquetas: se copian a un campo de audio_info_t convirtiendo a ISO-8859-1
 * (lo que no cabe se convierte en '?'). utf16: be = true para UTF-16BE. */
void audio_copiar_etiqueta(char *dst, unsigned max, const char *src, unsigned n);
void audio_etiqueta_utf8(char *dst, unsigned max, const uint8_t *src, unsigned n);
void audio_etiqueta_utf16(char *dst, unsigned max, const uint8_t *src, unsigned n, bool be);

#endif
