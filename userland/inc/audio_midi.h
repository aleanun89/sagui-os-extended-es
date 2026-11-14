// SPDX-License-Identifier: GPL-2.0-or-later
/* audio_midi.h - sintetizador de archivos MIDI estándar (.mid, .midi, .kar) de la libsagui (D190).
 *
 * No hay MIDI por hardware: el archivo se lee, se ordenan sus pistas por tiempo y se sintetiza
 * PCM de 16 bits estéreo a 44100 Hz, en el momento. Sin números de coma flotante (el userland
 * se compila sin SSE) y sin ficheros de datos: los timbres salen de tablas de ondas generadas
 * por suma de armónicos, FM de dos operadores y ruido (percusión). Soporta formatos 0 y 1,
 * 16 canales, cambio de programa (los 128 de General MIDI), percusión en el canal 10, velocidad,
 * volumen (CC7), expresión (CC11), panorama (CC10), rueda de tono con rango (RPN 0), modulación
 * (vibrato, CC1), pedal de sustain (CC64), todo-fuera (CC120/123), reinicio de controladores,
 * cambios de tempo, división SMPTE y la letra de los .kar (metaeventos 0x05 y 0x01).
 *
 * El mismo código compila en el anfitrión (-DAUDIO_HOST) para pruebas y para generar WAV.
 *
 * Además de usarse por la interfaz común (audio_formato.h: AUDIO_FMT_MIDI), se puede usar
 * directamente para ver el estado de los canales mientras suena:
 *
 *   midi_t *m = midi_abrir("/ejemplos/musica/alegria.kar", &erro);
 *   int16_t pcm[1024 * 2];
 *   while (midi_ler(m, pcm, 1024) > 0) { ...entregar...; midi_estado_em(m, midi_posicao(m), &e); }
 *   midi_fechar(m);
 *
 * Solo una instancia abierta a la vez por programa (los búferes son grandes). */
#ifndef AUDIO_MIDI_H
#define AUDIO_MIDI_H
#include "audio_formato.h"

#define MIDI_TAXA   44100
#define MIDI_CANAIS 16

typedef struct midi midi_t;

typedef struct {
    int      formato;            /* 0 o 1 */
    int      trilhas;
    uint32_t divisao;            /* tics por negra (o, en SMPTE, tics por segundo / 2) */
    uint32_t duracao_ms;
    uint32_t notas;              /* notas en total */
    uint32_t tempo_ini;          /* tempo inicial en microsegundos por negra */
    uint32_t letras;             /* sílabas de letra (karaoke) */
    bool     karaoke;            /* el archivo trae «@KMIDI KARAOKE FILE» */
    char     titulo[96];         /* nombre de secuencia / @T */
    char     copyright[96];      /* metaevento 2 */
    char     autor[96];          /* segunda línea @T de un .kar, si la hay */
} midi_info_t;

typedef struct {
    uint32_t ativas[MIDI_CANAIS][4];   /* bitmap de las notas que suenan (0-127) por canal */
    uint8_t  nivel[MIDI_CANAIS];       /* 0-255: actividad del canal (con caída suave) */
    uint8_t  programa[MIDI_CANAIS];
    uint8_t  volume[MIDI_CANAIS];      /* volumen * expresión, 0-127 */
    uint8_t  pan[MIDI_CANAIS];         /* 0 izquierda, 64 centro, 127 derecha */
    uint8_t  pedal;                    /* bit por canal con el sustain pisado (solo 8 primeros) */
    uint8_t  vozes;                    /* voces sonando */
    uint32_t quadro;                   /* instante (cuadros) de la instantánea */
} midi_estado_t;

typedef struct {
    uint32_t ms;                       /* instante en que se canta */
    const char *texto;                 /* sílaba (ISO-8859-1); un '/' al principio abre línea y un '\\', párrafo (convención .kar) */
} midi_letra_t;

midi_t *midi_abrir(const char *caminho, int *erro);
midi_t *midi_abrir_memoria(const uint8_t *dados, uint32_t tam, int *erro);   /* copia los datos */
void    midi_fechar(midi_t *m);
const midi_info_t *midi_info(const midi_t *m);

/* Sintetiza hasta `quadros` cuadros estéreo. Devuelve los cuadros dados; 0 al terminar (con la cola de
 * la reverberación ya sonada). */
int      midi_ler(midi_t *m, int16_t *pcm, unsigned quadros);
int      midi_buscar(midi_t *m, uint32_t ms);       /* sin sonar: aplica programa, volumen... hasta ahí */
uint32_t midi_posicao(const midi_t *m);              /* cuadros entregados */

/* Estado de los canales en el instante `quadro` (el más cercano de los últimos ~2 s ya sintetizados). */
void midi_estado_em(const midi_t *m, uint32_t quadro, midi_estado_t *e);

unsigned midi_letra_qtd(const midi_t *m);
const midi_letra_t *midi_letra(const midi_t *m, unsigned i);
int      midi_letra_indice(const midi_t *m, uint32_t ms);   /* última sílaba con ms <= dado, o -1 */

#endif
