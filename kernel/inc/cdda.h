// SPDX-License-Identifier: GPL-2.0-or-later
/* cdda.h - las pistas de un CD de audio como archivos WAV, solo lectura (D233).
 *
 * El lector (atapi.c) lee la tabla de contenido y el CD-TEXT y registra un disco
 * virtual "cdNaudio" (discos.c) con la tabla en disco_t.toc y la lectura de sectores
 * de audio (READ CD, 2352 bytes) en disco_t.ler_audio. El sistema de archivos
 * "cdda" (kernel/fs/cdda.c) lo presenta como una carpeta con un WAV por pista y una
 * lista M3U. La lógica pura (nombres, cabecera WAV, desplazamiento -> sector,
 * CD-TEXT) no toca el hardware y se prueba en el PC (tests/cdda_unidad.c). */
#ifndef SAGUI_CDDA_H
#define SAGUI_CDDA_H
#include "tipos.h"

#define CDDA_MAX_PISTAS   99
#define CDDA_SETOR        2352u      /* bytes de audio por sector: 588 cuadros de 16 bits estéreo */
#define CDDA_CABECALHO    44u        /* la cabecera RIFF/WAVE canónica */
#define CDDA_TEXTO        64         /* un título o un intérprete de CD-TEXT */
#define CDDA_SETORES_SEG  75u        /* sectores por segundo */
#define CDDA_LEADOUT_SESSAO 11400u   /* lead-out + lead-in + pregap entre la sesión de audio y la de datos */

typedef struct {
    uint8_t  numero;                 /* número de la pista en el disco (1..99) */
    uint32_t lba;                    /* primer sector */
    uint32_t setores;                /* sectores de audio */
} cdda_pista_t;

typedef struct cd_toc {
    int          qtd;                /* pistas de audio */
    cdda_pista_t pista[CDDA_MAX_PISTAS];
    int          dados;              /* pistas de datos (un CD mixto o Enhanced CD) */
    char         album[CDDA_TEXTO], artista[CDDA_TEXTO];   /* CD-TEXT del disco ("" si no hay) */
    char         titulo[CDDA_MAX_PISTAS][CDDA_TEXTO];      /* CD-TEXT por pista (índice como pista[]) */
    char         interprete[CDDA_MAX_PISTAS][CDDA_TEXTO];
} cd_toc_t;

/* --- lógica pura (también en el PC) --- */

/* De la respuesta de READ TOC formato 0 (con LBA): las pistas de audio y su duración.
 * Entre la última pista de audio y una pista de datos de otra sesión (Enhanced CD) se
 * quitan los 11400 sectores del cambio de sesión. Devuelve las pistas de audio. */
int  cdda_toc_ler(const uint8_t *r, uint32_t n, cd_toc_t *t);
/* La respuesta de READ TOC formato 5 (CD-TEXT): títulos e intérpretes del bloque 0. */
void cdda_cdtext_ler(const uint8_t *r, uint32_t n, cd_toc_t *t);
/* Nombre del archivo de la pista i: "Pista01.wav" o, con CD-TEXT, "01 - Título.wav"
 * (sin / \ : * ? " < > | ni controles, como mucho max - 1). */
void cdda_nome(const cd_toc_t *t, int i, char *s, int max);
/* La cabecera WAV (44 bytes) de una pista de "setores" sectores. */
void cdda_cabecalho(uint8_t c[CDDA_CABECALHO], uint32_t setores);
/* Tamaño del archivo WAV de la pista. */
uint32_t cdda_tamanho(uint32_t setores);
/* Duración total en segundos. */
uint32_t cdda_segundos(const cd_toc_t *t);
/* El texto de Lista.m3u (#EXTM3U, #EXTINF con duración y título). Devuelve los bytes. */
uint32_t cdda_m3u(const cd_toc_t *t, char *s, uint32_t max);

#endif
