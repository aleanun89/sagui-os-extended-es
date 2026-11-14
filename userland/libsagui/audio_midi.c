// SPDX-License-Identifier: GPL-2.0-or-later
/* audio_midi.c - lector de archivos MIDI estándar y sintetizador a PCM (D190).
 *
 * Estructura:
 *   1. Lectura del SMF: las pistas se recorren a la vez, siempre por el evento más cercano
 *      (no se ordenan ni se copian: el archivo entero está en memoria y cada pista es un cursor).
 *      Una pasada previa (prescan) calcula la duración, las etiquetas y la letra de los .kar.
 *   2. El tiempo: los tics se convierten a cuadros de audio con un decimal de 16 bits
 *      (espera), de modo que los eventos caen en su muestra y el tempo puede cambiar.
 *   3. Voces (32): cada nota es una voz con una tabla de ondas (suma de armónicos, una de cada
 *      familia de timbres y de cuatro tablas según la altura, para no repetir armónicos por
 *      encima de Nyquist), o FM de dos operadores (campanas, pianos eléctricos), más un filtro
 *      de paso bajo con su propia envolvente de brillo, y una envolvente ADSR que se actualiza
 *      cada 32 cuadros. La percusión (canal 10) es tono con barrido de altura más ruido.
 *   4. Mezcla estéreo con panorama de potencia constante, una reverberación de Schroeder
 *      (peines + paso-todo) y un limitador suave.
 *
 * Todo en enteros (el userland no usa SSE/FPU). Una sola instancia a la vez. */
#include "audio_interno.h"
#include "audio_midi.h"

#define TAXA      MIDI_TAXA
#define BLOCO     32           /* cuadros entre actualizaciones de las envolventes */
#define NVOZES    32
#define MASTER_Q  90           /* nivel maestro: nivel del patch (0-255) * 90 = Q15 */
#define NTIM      16
#define NBANDAS   4
#define TAB_N     1024
#define MAX_TRILHAS 64
#define MAX_ARQ   (8u << 20)
#define MAX_LETRAS 4096
#define LETRA_POOL 49152
#define NSNAP     128
#define SNAP_CADA 735          /* cuadros entre instantáneas (16 ms) */
#define CAUDA_MAX (TAXA * 3)   /* cola máxima tras el último evento */

/* ------------------------------------------------------------ tablas globales (se calculan una vez) */

static int16_t g_seno[TAB_N];
static uint32_t g_incn[140];       /* incremento de fase por nota (0 = 8,18 Hz) a 44100 Hz */
static uint32_t g_frac[256];       /* 2^(i/3072) en Q16 */
static bool g_iniciado;

/* a * (1 + mf / 2^32) sin desbordar 64 bits */
static uint64_t mulf32(uint64_t a, uint32_t mf)
{
    return a + (a >> 32) * mf + (((a & 0xFFFFFFFFull) * mf) >> 32);
}

static void iniciar_tabelas(void)
{
    if (g_iniciado) return;
    /* seno: un cuarto de onda por serie de Taylor en Q30 */
    int16_t q[257];
    for (int i = 0; i <= 256; i++) {
        int64_t x = ((int64_t)i * 1686629713) >> 8;     /* i * (pi/2) / 256, Q30 */
        int64_t x2 = (x * x) >> 30;
        int64_t t = x, s = x;
        static const int64_t rec[4] = { 178956971, 53687091, 25565282, 14913081 };   /* 2^30 / (6, 20, 42, 72) */
        for (int k = 0; k < 4; k++) {
            t = -((((t * x2) >> 30) * rec[k]) >> 30);
            s += t;
        }
        int64_t v = s >> 15;
        q[i] = v > 32767 ? 32767 : (int16_t)v;
    }
    for (int i = 0; i < TAB_N; i++) {
        int k = i & 511;
        int16_t v = k <= 256 ? q[k] : q[512 - k];
        g_seno[i] = i < 512 ? v : (int16_t)-v;
    }
    uint64_t f = 52183283659ull << 8;                   /* incremento de la nota 0 en Q24 */
    for (int i = 0; i < 140; i++) {
        g_incn[i] = (uint32_t)(f >> 24);
        f = mulf32(f, 0x0F38F92Eu);                     /* * 2^(1/12) */
    }
    uint64_t c = 1ull << 32;
    for (int i = 0; i < 256; i++) {
        g_frac[i] = (uint32_t)(c >> 16);
        c = mulf32(c, 0x000EC9EFu);                     /* * 2^(1/3072) */
    }
    g_iniciado = true;
}

/* seno con interpolación: fase de 32 bits -> int16 */
static inline int seno_fase(uint32_t ph)
{
    uint32_t i = ph >> 22, fr = (ph >> 6) & 0xFFFF;
    int a = g_seno[i], b = g_seno[(i + 1) & (TAB_N - 1)];
    return a + (((b - a) * (int)fr) >> 16);
}

/* ------------------------------------------------------------ timbres (amplitud de 24 armónicos) */

enum { T_SENO, T_TRI, T_QUAD, T_SERRA, T_PIANO, T_ORGAO, T_CLARI, T_OBOE, T_METAL, T_FLAUTA, T_CORDAS, T_GUITAR, T_DISTOR, T_BAJO, T_VOZ, T_PULSO };

static const uint8_t g_espectro[NTIM][24] = {
    /* seno */ { 255 },
    /* tri  */ { 255, 0, 28, 0, 10, 0, 5, 0, 3, 0, 2, 0, 1 },
    /* quad */ { 255, 0, 85, 0, 51, 0, 36, 0, 28, 0, 23, 0, 19, 0, 17, 0, 15, 0, 13, 0, 12, 0, 11 },
    /* serra*/ { 255, 128, 85, 64, 51, 43, 36, 32, 28, 26, 23, 21, 20, 18, 17, 16, 15, 14, 13, 13, 12, 11, 11, 10 },
    /* piano*/ { 255, 205, 120, 95, 60, 44, 30, 20, 14, 10, 7, 6, 4, 3, 2, 2 },
    /* orgao*/ { 255, 235, 150, 195, 20, 115, 0, 95, 0, 40, 0, 25 },
    /* clari*/ { 255, 10, 190, 8, 125, 6, 85, 5, 50, 3, 28, 2, 14 },
    /* oboe */ { 200, 255, 235, 160, 115, 95, 65, 42, 32, 26, 20, 14, 10, 8, 6 },
    /* metal*/ { 150, 255, 240, 210, 165, 135, 105, 85, 65, 50, 40, 30, 25, 20, 16, 12, 10, 8 },
    /* flaut*/ { 255, 55, 18, 8, 3, 2 },
    /* cord */ { 255, 175, 135, 105, 85, 68, 56, 46, 40, 34, 29, 25, 22, 19, 17, 15, 13, 12, 10, 9 },
    /* guit */ { 255, 195, 125, 75, 62, 38, 26, 20, 13, 10, 7, 5, 4 },
    /* disto*/ { 255, 235, 205, 185, 165, 145, 125, 112, 100, 90, 80, 70, 62, 54, 48, 42, 36, 32, 28, 24 },
    /* bajo */ { 255, 170, 62, 40, 25, 15, 10, 6, 4 },
    /* voz  */ { 255, 150, 62, 82, 125, 72, 34, 22, 40, 28, 14, 10, 6 },
    /* pulso*/ { 255, 235, 215, 195, 175, 155, 138, 124, 112, 100, 90, 80, 72, 64, 58, 52, 47, 42, 38, 34 },
};

static const uint8_t g_banda_h[NBANDAS] = { 24, 10, 5, 2 };   /* armónicos máximos por banda */

static int banda_da_nota(int n) { return n < 82 ? 0 : n < 96 ? 1 : n < 108 ? 2 : 3; }

static int16_t *gerar_tabela(int tim, int banda)
{
    int16_t *t = aloca(TAB_N * sizeof(int16_t));
    if (!t) return NULL;
    int32_t pico = 1;
    int32_t *tmp = aloca(TAB_N * sizeof(int32_t));
    if (!tmp) { libera(t); return NULL; }
    int maxh = g_banda_h[banda];
    for (int i = 0; i < TAB_N; i++) {
        int32_t s = 0;
        for (int h = 1; h <= maxh && h <= 24; h++) {
            int a = g_espectro[tim][h - 1];
            if (a) s += (a * g_seno[(h * i) & (TAB_N - 1)]) >> 8;
        }
        if (tim == T_DISTOR) {   /* saturación: recorta y redondea */
            if (s > 30000) s = 30000 + (s - 30000) / 8;
            if (s < -30000) s = -30000 + (s + 30000) / 8;
        }
        tmp[i] = s;
        int32_t a = s < 0 ? -s : s;
        if (a > pico) pico = a;
    }
    for (int i = 0; i < TAB_N; i++) t[i] = (int16_t)(tmp[i] * 22000 / pico);
    libera(tmp);
    return t;
}

/* ------------------------------------------------------------ instrumentos (General MIDI) */

typedef struct {
    uint8_t  tim;
    uint8_t  fm;                  /* 0 = tabla de ondas; si no, razón del modulador en cuartos */
    uint8_t  fmi;                 /* índice de modulación inicial, en 1/16 de radián */
    uint16_t fmd;                 /* ms de caída del índice */
    uint16_t atk, dec, rel;       /* ms: ataque, constante de caída hacia el sostenido, liberación */
    uint8_t  sus;                 /* nivel sostenido 0-255 */
    uint8_t  bri0, bri1;          /* brillo (apertura del filtro) al principio y al final, 0-255 */
    uint16_t brid;                /* ms de la caída del brillo */
    uint8_t  vib;                 /* vibrato propio (0-255) */
    uint8_t  det;                 /* desafinación de la segunda onda, en cents (0 = una sola) */
    uint8_t  niv;                 /* nivel 0-255 */
} patch_t;

/* Familias base: una por cada grupo de 8 programas de GM. */
static const patch_t g_familia[16] = {
    /* 0 piano     */ { T_PIANO, 0, 0, 0, 2, 2400, 260, 0, 255, 70, 700, 0, 0, 230 },
    /* 1 percusión cromática */ { T_SENO, 14, 20, 500, 1, 1500, 300, 0, 255, 255, 1, 0, 0, 220 },
    /* 2 órgano    */ { T_ORGAO, 0, 0, 0, 8, 100, 50, 255, 255, 255, 1, 0, 0, 170 },
    /* 3 guitarra  */ { T_GUITAR, 0, 0, 0, 1, 900, 200, 0, 255, 50, 350, 0, 0, 230 },
    /* 4 bajo      */ { T_BAJO, 0, 0, 0, 2, 1000, 120, 0, 255, 70, 250, 0, 0, 255 },
    /* 5 cuerdas   */ { T_CORDAS, 0, 0, 0, 70, 100, 160, 255, 230, 240, 200, 14, 7, 200 },
    /* 6 conjunto  */ { T_CORDAS, 0, 0, 0, 160, 100, 280, 255, 200, 255, 400, 8, 11, 190 },
    /* 7 metales   */ { T_METAL, 0, 0, 0, 35, 200, 90, 230, 90, 235, 90, 10, 5, 210 },
    /* 8 lengüetas */ { T_OBOE, 0, 0, 0, 40, 100, 90, 240, 150, 220, 120, 10, 0, 210 },
    /* 9 flautas   */ { T_FLAUTA, 0, 0, 0, 50, 100, 90, 240, 255, 255, 1, 18, 0, 215 },
    /* 10 solistas sintetizados */ { T_QUAD, 0, 0, 0, 6, 100, 60, 255, 255, 255, 1, 6, 8, 160 },
    /* 11 pads     */ { T_SERRA, 0, 0, 0, 260, 100, 420, 255, 120, 230, 900, 5, 12, 150 },
    /* 12 efectos  */ { T_TRI, 0, 0, 0, 120, 100, 300, 255, 255, 255, 1, 0, 6, 170 },
    /* 13 étnicos  */ { T_GUITAR, 0, 0, 0, 1, 700, 200, 0, 255, 60, 300, 0, 0, 220 },
    /* 14 percusivos */ { T_SENO, 14, 18, 300, 1, 600, 150, 0, 255, 255, 1, 0, 0, 220 },
    /* 15 efectos de sonido */ { T_TRI, 0, 0, 0, 300, 100, 500, 255, 180, 200, 600, 0, 8, 150 },
};

static void patch_de(int prog, patch_t *p)
{
    *p = g_familia[prog >> 3];
    switch (prog) {
    /* piano */
    case 1: p->bri0 = 255; p->bri1 = 110; break;
    case 2: p->tim = T_CORDAS; p->det = 5; p->niv = 220; break;
    case 3: p->det = 14; p->bri1 = 90; break;
    case 4: p->tim = T_SENO; p->fm = 4; p->fmi = 14; p->fmd = 700; p->bri0 = p->bri1 = 255; p->sus = 0; p->dec = 3200; break;
    case 5: p->tim = T_SENO; p->fm = 4; p->fmi = 24; p->fmd = 400; p->bri0 = p->bri1 = 255; p->det = 4; p->dec = 2800; break;
    case 6: p->tim = T_SERRA; p->dec = 500; p->sus = 0; p->rel = 60; p->bri0 = 255; p->bri1 = 170; break;
    case 7: p->tim = T_PULSO; p->dec = 600; p->rel = 60; p->bri0 = 255; p->bri1 = 130; break;
    /* percusión cromática */
    case 8: p->fm = 16; p->fmi = 14; p->fmd = 300; p->dec = 900; break;
    case 9: p->fm = 20; p->fmi = 16; p->fmd = 450; p->dec = 1200; break;
    case 10: p->fm = 12; p->fmi = 8; p->fmd = 500; p->dec = 1100; break;
    case 11: p->fm = 16; p->fmi = 8; p->fmd = 800; p->dec = 3000; p->vib = 10; break;
    case 12: p->fm = 16; p->fmi = 16; p->fmd = 60; p->dec = 500; break;
    case 13: p->fm = 12; p->fmi = 18; p->fmd = 40; p->dec = 280; break;
    case 14: p->fm = 14; p->fmi = 30; p->fmd = 1500; p->dec = 4500; p->rel = 600; break;
    case 15: p->fm = 8; p->fmi = 10; p->fmd = 200; p->dec = 600; break;
    /* órgano */
    case 16: p->tim = T_ORGAO; break;
    case 17: p->tim = T_ORGAO; p->vib = 15; p->det = 4; break;
    case 18: p->tim = T_PULSO; p->niv = 140; break;
    case 19: p->tim = T_ORGAO; p->atk = 25; p->rel = 160; p->det = 3; break;
    case 20: p->tim = T_CLARI; p->atk = 20; break;
    case 21: case 23: p->tim = T_OBOE; p->det = 9; p->atk = 30; p->vib = 10; break;
    case 22: p->tim = T_CLARI; p->atk = 25; p->vib = 20; break;
    /* guitarras */
    case 24: p->tim = T_GUITAR; p->bri1 = 40; break;
    case 25: p->tim = T_SERRA; p->bri0 = 255; p->bri1 = 70; p->det = 3; break;
    case 26: p->tim = T_PIANO; p->bri1 = 30; p->dec = 1200; break;
    case 27: p->tim = T_GUITAR; p->bri1 = 100; p->dec = 1600; break;
    case 28: p->dec = 140; p->rel = 50; p->bri1 = 25; break;
    case 29: p->tim = T_DISTOR; p->sus = 110; p->dec = 3500; p->bri0 = 200; p->bri1 = 200; p->niv = 190; break;
    case 30: p->tim = T_DISTOR; p->sus = 150; p->dec = 6000; p->bri0 = 255; p->bri1 = 230; p->niv = 180; break;
    case 31: p->tim = T_SENO; p->dec = 700; p->bri0 = p->bri1 = 255; break;
    /* bajos */
    case 32: p->tim = T_BAJO; p->dec = 1100; break;
    case 33: p->tim = T_BAJO; p->dec = 1800; p->bri1 = 90; break;
    case 34: p->tim = T_SERRA; p->bri0 = 255; p->bri1 = 30; p->dec = 900; break;
    case 35: p->tim = T_TRI; p->dec = 1800; p->vib = 30; p->bri0 = 150; p->bri1 = 60; break;
    case 36: case 37: p->tim = T_SERRA; p->bri0 = 255; p->bri1 = 40; p->dec = 500; p->brid = 120; break;
    case 38: p->tim = T_SERRA; p->bri0 = 255; p->bri1 = 40; p->brid = 180; p->sus = 220; p->dec = 6000; break;
    case 39: p->tim = T_QUAD; p->bri0 = 200; p->bri1 = 50; p->brid = 300; p->sus = 220; p->dec = 6000; break;
    /* cuerdas */
    case 40: case 41: break;
    case 42: case 43: p->atk = 90; p->bri1 = 190; break;
    case 44: p->vib = 25; p->tim = T_SERRA; break;
    case 45: p->atk = 2; p->dec = 160; p->sus = 0; p->rel = 80; p->bri0 = 255; p->bri1 = 60; p->vib = 0; p->det = 0; break;
    case 46: p->tim = T_PIANO; p->atk = 1; p->dec = 1500; p->sus = 0; p->rel = 400; p->bri1 = 120; break;
    case 47: p->tim = T_SENO; p->atk = 2; p->dec = 900; p->sus = 0; p->rel = 150; p->bri0 = p->bri1 = 255; p->fm = 4; p->fmi = 24; p->fmd = 60; break;
    /* conjuntos */
    case 48: case 49: break;
    case 50: case 51: p->tim = T_SERRA; p->bri1 = 150; p->det = 14; break;
    case 52: case 53: p->tim = T_VOZ; p->atk = 120; p->vib = 20; p->bri1 = 200; break;
    case 54: p->tim = T_VOZ; p->atk = 20; p->bri0 = 255; p->bri1 = 255; break;
    case 55: p->tim = T_METAL; p->atk = 3; p->dec = 400; p->sus = 0; p->rel = 120; p->bri0 = 255; p->bri1 = 150; break;
    /* metales */
    case 56: break;
    case 57: p->bri0 = 80; p->bri1 = 190; p->tim = T_METAL; p->atk = 50; break;
    case 58: p->bri0 = 60; p->bri1 = 140; p->atk = 60; p->tim = T_BAJO; p->niv = 255; break;
    case 59: p->bri0 = 150; p->bri1 = 150; p->tim = T_OBOE; break;
    case 60: p->tim = T_CORDAS; p->atk = 80; p->bri0 = 60; p->bri1 = 160; break;
    case 61: p->atk = 25; p->det = 8; break;
    case 62: case 63: p->tim = T_SERRA; p->atk = 30; p->det = 10; p->bri0 = 110; p->bri1 = 230; break;
    /* lengüetas */
    case 64: case 65: case 66: case 67: p->tim = T_CLARI; p->atk = 25; p->bri0 = 190; p->bri1 = 190; p->vib = 28; break;
    case 68: case 69: p->tim = T_OBOE; break;
    case 70: p->tim = T_CLARI; p->atk = 35; p->bri0 = p->bri1 = 150; break;
    case 71: p->tim = T_CLARI; p->atk = 30; p->vib = 6; break;
    /* flautas */
    case 72: p->bri0 = p->bri1 = 255; p->tim = T_TRI; break;
    case 73: case 74: break;
    case 75: case 76: p->tim = T_SENO; p->atk = 70; break;
    case 77: p->tim = T_FLAUTA; p->atk = 90; p->vib = 24; break;
    case 78: case 79: p->tim = T_SENO; p->atk = 30; p->vib = 8; break;
    /* sintetizadores solistas */
    case 80: p->tim = T_QUAD; break;
    case 81: p->tim = T_SERRA; p->det = 10; break;
    case 82: p->tim = T_FLAUTA; p->atk = 40; p->vib = 20; break;
    case 83: p->tim = T_SERRA; p->atk = 12; p->bri0 = 255; p->bri1 = 200; break;
    case 84: p->tim = T_DISTOR; p->niv = 130; break;
    case 85: p->tim = T_VOZ; p->vib = 20; break;
    case 86: p->tim = T_PULSO; p->det = 12; break;
    case 87: p->tim = T_SERRA; p->bri1 = 200; break;
    /* pads */
    case 88: p->tim = T_TRI; p->bri1 = 255; p->bri0 = 255; break;
    case 89: p->tim = T_SERRA; p->bri0 = 100; p->bri1 = 110; break;
    case 90: p->tim = T_PULSO; p->atk = 30; p->bri0 = 255; p->bri1 = 170; p->det = 12; break;
    case 91: p->tim = T_VOZ; p->atk = 200; p->bri1 = 190; break;
    case 92: p->tim = T_CORDAS; p->atk = 400; break;
    case 93: p->tim = T_SENO; p->fm = 14; p->fmi = 20; p->fmd = 2000; p->atk = 120; p->dec = 3000; break;
    case 94: p->tim = T_VOZ; p->atk = 300; p->bri1 = 255; p->det = 18; break;
    case 95: p->tim = T_SERRA; p->atk = 500; p->bri0 = 40; p->bri1 = 255; p->brid = 800; break;
    /* efectos */
    case 96: p->tim = T_SENO; p->fm = 6; p->fmi = 10; p->fmd = 3000; p->atk = 50; break;
    case 98: p->tim = T_SENO; p->fm = 14; p->fmi = 10; p->fmd = 1500; p->dec = 3000; p->atk = 5; p->sus = 0; break;
    case 102: p->tim = T_SENO; p->fm = 6; p->fmi = 12; p->fmd = 1800; p->dec = 3000; p->sus = 120; break;
    /* étnicos */
    case 104: p->tim = T_PULSO; p->dec = 1400; p->bri0 = 255; p->bri1 = 190; p->sus = 0; break;
    case 105: p->tim = T_SERRA; p->dec = 380; p->sus = 0; p->bri1 = 60; break;
    case 106: p->tim = T_SERRA; p->dec = 500; p->sus = 0; p->bri1 = 50; break;
    case 107: p->tim = T_PIANO; p->dec = 800; p->sus = 0; p->bri1 = 90; break;
    case 108: p->tim = T_SENO; p->fm = 16; p->fmi = 10; p->fmd = 40; p->dec = 600; p->sus = 0; p->bri0 = p->bri1 = 255; break;
    case 109: p->tim = T_OBOE; p->atk = 70; p->det = 8; p->bri0 = p->bri1 = 255; p->sus = 255; break;
    case 110: p->tim = T_CORDAS; p->atk = 30; break;
    case 111: p->tim = T_OBOE; p->det = 6; p->atk = 40; p->vib = 30; break;
    /* percusivos */
    case 112: p->fm = 14; p->fmi = 12; p->fmd = 700; p->dec = 1800; break;
    case 113: p->fm = 10; p->fmi = 20; p->fmd = 120; p->dec = 400; break;
    case 114: p->fm = 8; p->fmi = 18; p->fmd = 300; p->dec = 1000; break;
    case 115: p->fm = 12; p->fmi = 14; p->fmd = 25; p->dec = 90; break;
    case 116: p->fm = 4; p->fmi = 26; p->fmd = 60; p->dec = 400; break;
    case 117: p->fm = 4; p->fmi = 22; p->fmd = 120; p->dec = 600; break;
    case 118: p->fm = 4; p->fmi = 28; p->fmd = 40; p->dec = 220; break;
    case 119: p->tim = T_SERRA; p->atk = 1200; p->bri0 = 80; p->bri1 = 255; p->brid = 1500; p->sus = 120; break;
    default: break;
    }
}

/* ------------------------------------------------------------ percusión (notas 35 a 81 de GM) */

typedef struct { uint16_t f0, f1, tms; uint8_t tl; uint16_t nms; uint8_t nl, modo; uint16_t k; } perc_t;

/* modo de ruido: 0 blanco, 1 paso alto con corte k, 2 paso bajo con corte k (k = 2*pi*fc/fs en Q15) */
static const perc_t g_perc[47] = {
    /* 35 */ { 160, 48, 230, 255, 25, 40, 2, 8000 },
    /* 36 */ { 180, 50, 200, 255, 20, 55, 2, 9000 },
    /* 37 */ { 900, 700, 25, 190, 20, 130, 1, 12000 },
    /* 38 */ { 230, 170, 110, 170, 170, 210, 1, 6500 },
    /* 39 */ { 0, 0, 0, 0, 140, 255, 1, 5500 },
    /* 40 */ { 250, 180, 100, 170, 190, 220, 1, 5500 },
    /* 41 */ { 100, 70, 320, 255, 25, 30, 2, 6000 },
    /* 42 */ { 0, 0, 0, 0, 45, 190, 1, 26000 },
    /* 43 */ { 120, 85, 300, 255, 25, 30, 2, 6000 },
    /* 44 */ { 0, 0, 0, 0, 75, 180, 1, 26000 },
    /* 45 */ { 140, 100, 280, 255, 25, 30, 2, 6000 },
    /* 46 */ { 0, 0, 0, 0, 420, 190, 1, 25000 },
    /* 47 */ { 165, 115, 260, 255, 25, 30, 2, 6000 },
    /* 48 */ { 200, 140, 250, 255, 25, 30, 2, 6000 },
    /* 49 */ { 380, 380, 300, 35, 1600, 230, 1, 22000 },
    /* 50 */ { 240, 170, 240, 255, 25, 30, 2, 6000 },
    /* 51 */ { 4500, 4500, 700, 45, 800, 140, 1, 24000 },
    /* 52 */ { 330, 330, 500, 40, 1100, 200, 1, 17000 },
    /* 53 */ { 3300, 3300, 600, 120, 200, 45, 1, 24000 },
    /* 54 */ { 6000, 6000, 40, 60, 180, 150, 1, 26000 },
    /* 55 */ { 520, 520, 200, 30, 700, 220, 1, 24000 },
    /* 56 */ { 560, 560, 240, 190, 10, 40, 1, 20000 },
    /* 57 */ { 420, 420, 300, 35, 1800, 230, 1, 22000 },
    /* 58 */ { 0, 0, 0, 0, 320, 120, 1, 12000 },
    /* 59 */ { 4200, 4200, 700, 45, 900, 140, 1, 24000 },
    /* 60 */ { 420, 320, 90, 255, 12, 60, 1, 10000 },
    /* 61 */ { 310, 240, 110, 255, 12, 60, 1, 10000 },
    /* 62 */ { 330, 300, 60, 255, 8, 40, 1, 10000 },
    /* 63 */ { 330, 290, 150, 255, 8, 40, 1, 10000 },
    /* 64 */ { 240, 200, 180, 255, 8, 40, 1, 10000 },
    /* 65 */ { 450, 400, 150, 220, 40, 60, 1, 22000 },
    /* 66 */ { 350, 300, 170, 220, 40, 60, 1, 22000 },
    /* 67 */ { 900, 900, 200, 160, 10, 50, 1, 20000 },
    /* 68 */ { 680, 680, 220, 160, 10, 50, 1, 20000 },
    /* 69 */ { 0, 0, 0, 0, 55, 160, 1, 23000 },
    /* 70 */ { 0, 0, 0, 0, 40, 160, 1, 25000 },
    /* 71 */ { 2600, 2600, 150, 130, 20, 20, 0, 0 },
    /* 72 */ { 2400, 2400, 700, 130, 20, 20, 0, 0 },
    /* 73 */ { 0, 0, 0, 0, 90, 150, 1, 15000 },
    /* 74 */ { 0, 0, 0, 0, 400, 150, 1, 15000 },
    /* 75 */ { 2400, 2400, 35, 230, 5, 20, 1, 20000 },
    /* 76 */ { 1100, 1100, 55, 230, 5, 20, 1, 20000 },
    /* 77 */ { 800, 800, 70, 230, 5, 20, 1, 20000 },
    /* 78 */ { 400, 300, 120, 200, 40, 30, 1, 12000 },
    /* 79 */ { 300, 500, 200, 200, 40, 30, 1, 12000 },
    /* 80 */ { 6000, 6000, 80, 120, 5, 20, 1, 24000 },
    /* 81 */ { 6000, 6000, 900, 120, 5, 20, 1, 24000 },
};

/* ------------------------------------------------------------ estructuras del sintetizador */

typedef struct { const uint8_t *p, *fim; uint32_t prox; uint8_t status; bool ativa; } trilha_t;
typedef struct { trilha_t t[MAX_TRILHAS]; int n; } cursor_t;
typedef struct {
    uint32_t tick;
    uint8_t st, d1, d2, meta;
    const uint8_t *dado;
    uint32_t n;
} ev_t;

enum { F_ATAQUE, F_DECAI, F_SUSTENTO, F_LIBERA };

typedef struct {
    bool     ativa, perc, sustentada;
    uint8_t  canal, nota, vel, fase, tipo;   /* tipo: 0 tabla, 1 FM, 2 percusión */
    uint8_t  vib, det;
    uint32_t ordem, inicio;
    const int16_t *tab;
    uint32_t ph, ph2, phm, inc;              /* fases; incremento base (sin bend) */
    uint32_t velq;                           /* velocidad al cuadrado, Q15 */
    uint32_t niv;                            /* nivel del patch * volumen maestro, Q15 */
    int32_t  nivel;                          /* envolvente, Q24 */
    int32_t  sus;                            /* sostenido, Q24 */
    int32_t  k_ata, c_dec, c_lib;
    int32_t  bri, bri1, c_bri;               /* brillo Q8 -> Q16 (0..255<<8) */
    int32_t  lp;                             /* estado del filtro de paso bajo */
    uint32_t fmr;                            /* razón del modulador (cuartos) */
    int32_t  fmi, c_fmi;                     /* índice FM, Q8 radianes */
    /* percusión */
    uint32_t inc1;                           /* incremento de destino del barrido */
    int32_t  c_sweep;
    int32_t  nivel2, c_n2;                   /* ruido (Q24) */
    uint8_t  modo;
    uint16_t kruido;
    uint32_t ruido;
    int32_t  lp2;
    int32_t  gl, gr;                         /* ganancias actuales (Q16) */
} voz_t;

typedef struct {
    uint8_t  prog, vol, expr, pan, mod, rango;
    bool     pedal;
    int16_t  bend;                           /* -8192..8191 */
    uint16_t rpn;
} canal_t;

#define REV_COMBS 4
static const uint16_t g_comb_len[2][REV_COMBS] = { { 1116, 1277, 1422, 1557 }, { 1188, 1356, 1491, 1617 } };
static const uint16_t g_ap_len[2] = { 556, 441 };

typedef struct {
    int16_t  comb[2][REV_COMBS][1700];
    int32_t  comb_lp[2][REV_COMBS];
    uint16_t comb_i[2][REV_COMBS];
    int16_t  ap[2][600];
    uint16_t ap_i[2];
} reverb_t;

typedef struct {
    uint32_t ativas[MIDI_CANAIS][4];
    uint8_t  nivel[MIDI_CANAIS], programa[MIDI_CANAIS], volume[MIDI_CANAIS], pan[MIDI_CANAIS];
    uint8_t  pedal, vozes;
    uint32_t quadro;
} snap_t;

struct midi {
    uint8_t *arq;
    uint32_t arq_tam;
    midi_info_t info;
    cursor_t cur;
    /* tiempo */
    uint32_t ppqn, tempo;
    bool     smpte;
    int64_t  espera;                         /* cuadros (Q16) hasta el próximo evento */
    uint32_t spt16;                          /* cuadros por tic, Q16 */
    uint32_t tick_ant;
    ev_t     prox;
    bool     tem;
    uint32_t pos;                            /* cuadros entregados */
    uint32_t dur_q;                          /* duración en cuadros */
    uint32_t cauda;                          /* cuadros dados tras el último evento */
    bool     mudo;                           /* buscando: no se crean voces */
    /* voces y canales */
    voz_t    v[NVOZES];
    canal_t  c[MIDI_CANAIS];
    uint32_t ordem;
    uint32_t lfo;
    int16_t *tab[NTIM][NBANDAS];
    uint8_t  nv[MIDI_CANAIS];                /* niveles con caída */
    uint32_t ruido;
    int32_t  agc;                            /* ganancia automática, Q15 */
    reverb_t *rev;
    /* instantáneas */
    snap_t   snap[NSNAP];
    uint32_t snap_n, snap_ult;
    /* letra */
    midi_letra_t *letra;
    unsigned nletra;
    char     *pool;
    uint32_t pool_n;
};

/* ------------------------------------------------------------ utilidades */

static uint64_t div64(uint64_t n, uint32_t d)
{
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= 1ull << i; }
    }
    return q;
}

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static uint32_t be16(const uint8_t *p) { return p[0] << 8 | p[1]; }

static bool vlq(trilha_t *t, uint32_t *v)
{
    uint32_t r = 0;
    for (int i = 0; i < 4; i++) {
        if (t->p >= t->fim) return false;
        uint8_t b = *t->p++;
        r = r << 7 | (b & 0x7F);
        if (!(b & 0x80)) { *v = r; return true; }
    }
    return false;
}

/* ------------------------------------------------------------ lectura del SMF */

static void cur_proximo_delta(trilha_t *t, uint32_t base)
{
    uint32_t d;
    if (!vlq(t, &d)) { t->ativa = false; return; }
    t->prox = base + d;
}

static bool cur_proximo(cursor_t *c, ev_t *e)
{
    for (;;) {
        int b = -1;
        for (int i = 0; i < c->n; i++)
            if (c->t[i].ativa && (b < 0 || c->t[i].prox < c->t[b].prox)) b = i;
        if (b < 0) return false;
        trilha_t *t = &c->t[b];
        e->tick = t->prox;
        e->dado = NULL;
        e->n = 0;
        e->meta = 0;
        if (t->p >= t->fim) { t->ativa = false; continue; }
        uint8_t x = *t->p;
        if (x == 0xFF) {
            t->p++;
            if (t->p >= t->fim) { t->ativa = false; continue; }
            e->meta = *t->p++;
            uint32_t len;
            if (!vlq(t, &len) || len > (uint32_t)(t->fim - t->p)) { t->ativa = false; continue; }
            e->st = 0xFF;
            e->dado = t->p;
            e->n = len;
            t->p += len;
            if (e->meta == 0x2F) t->ativa = false;
            else cur_proximo_delta(t, e->tick);
            return true;
        }
        if (x == 0xF0 || x == 0xF7) {
            t->p++;
            uint32_t len;
            if (!vlq(t, &len) || len > (uint32_t)(t->fim - t->p)) { t->ativa = false; continue; }
            e->st = 0xF0;
            e->dado = t->p;
            e->n = len;
            t->p += len;
            cur_proximo_delta(t, e->tick);
            return true;
        }
        if (x & 0x80) {
            t->p++;
            t->status = x;
        } else if (!t->status) { t->ativa = false; continue; }
        if (t->status >= 0xF0) { t->ativa = false; continue; }
        unsigned nd = ((t->status & 0xF0) == 0xC0 || (t->status & 0xF0) == 0xD0) ? 1 : 2;
        if ((unsigned)(t->fim - t->p) < nd) { t->ativa = false; continue; }
        e->st = t->status;
        e->d1 = t->p[0] & 0x7F;
        e->d2 = nd == 2 ? (t->p[1] & 0x7F) : 0;
        t->p += nd;
        cur_proximo_delta(t, e->tick);
        return true;
    }
}

static int cur_iniciar(midi_t *m, cursor_t *c)
{
    memset(c, 0, sizeof *c);
    const uint8_t *d = m->arq, *fim = m->arq + m->arq_tam;
    uint32_t hl = be32(d + 4);
    const uint8_t *p = d + 8 + hl;
    while (p + 8 <= fim && c->n < MAX_TRILHAS) {
        uint32_t tl = be32(p + 4);
        const uint8_t *ini = p + 8;
        if (tl > (uint32_t)(fim - ini)) tl = (uint32_t)(fim - ini);
        if (memcmp(p, "MTrk", 4) == 0) {
            trilha_t *t = &c->t[c->n++];
            t->p = ini;
            t->fim = ini + tl;
            t->ativa = true;
            cur_proximo_delta(t, 0);
        }
        p = ini + tl;
    }
    return c->n;
}

/* ------------------------------------------------------------ tiempo */

static void definir_tempo(midi_t *m, uint32_t us)
{
    if (!us) us = 500000;
    m->tempo = us;
    uint64_t n = (uint64_t)us * TAXA * 1024;
    m->spt16 = (uint32_t)div64(n, m->ppqn * 15625u);
    if (!m->spt16) m->spt16 = 1;
}

static void carregar_proximo(midi_t *m)
{
    if (cur_proximo(&m->cur, &m->prox)) {
        m->tem = true;
        m->espera += (int64_t)(m->prox.tick - m->tick_ant) * m->spt16;
        m->tick_ant = m->prox.tick;
    } else m->tem = false;
}

/* ------------------------------------------------------------ prescan: duración, etiquetas, letra */

enum { LK_LYRIC = 5, LK_TEXT = 1 };

static void anotar_letra(midi_t *m, uint32_t ms, const uint8_t *d, uint32_t n, int tipo)
{
    if (m->nletra >= MAX_LETRAS || !n || m->pool_n + n + 2 > LETRA_POOL) return;
    char *s = m->pool + m->pool_n;
    uint32_t k = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint8_t c = d[i];
        if (c == 0) break;
        s[k++] = (char)c;
    }
    s[k] = 0;
    /* tipo en el campo de ms: se separa al final del prescan */
    m->pool_n += k + 1;
    m->letra[m->nletra].ms = ms | (tipo == LK_LYRIC ? 0x80000000u : 0);
    m->letra[m->nletra].texto = s;
    m->nletra++;
}

static void mid_texto(char *dst, unsigned max, const uint8_t *d, uint32_t n)
{
    unsigned o = 0;
    for (uint32_t i = 0; i < n && o + 1 < max; i++)
        if (d[i] >= 32 || d[i] == '\t') dst[o++] = (char)d[i];
    while (o && dst[o - 1] == ' ') o--;
    dst[o] = 0;
}

static void prescan(midi_t *m)
{
    cursor_t c;
    cur_iniciar(m, &c);
    uint32_t tempo = 500000, tick_ant = 0;
    uint64_t us = 0;
    ev_t e;
    uint32_t ms_fim = 0;
    bool tem_ts = false;
    int ntitulos = 0;
    m->info.tempo_ini = 0;
    while (cur_proximo(&c, &e)) {
        us += (uint64_t)(e.tick - tick_ant) * tempo;
        tick_ant = e.tick;
        uint32_t ms = (uint32_t)div64(us, m->ppqn * 1000u);
        if (ms > ms_fim) ms_fim = ms;
        if (e.st == 0xFF) {
            if (e.meta == 0x51 && e.n >= 3 && !m->smpte) {
                tempo = (uint32_t)e.dado[0] << 16 | e.dado[1] << 8 | e.dado[2];
                if (!tempo) tempo = 500000;
                if (!m->info.tempo_ini) m->info.tempo_ini = tempo;
            } else if (e.meta == 0x03 && !tem_ts) {
                mid_texto(m->info.titulo, sizeof m->info.titulo, e.dado, e.n);
                if (m->info.titulo[0]) tem_ts = true;
            } else if (e.meta == 0x02 && !m->info.copyright[0]) {
                mid_texto(m->info.copyright, sizeof m->info.copyright, e.dado, e.n);
            } else if (e.meta == 0x05) {
                anotar_letra(m, ms, e.dado, e.n, LK_LYRIC);
            } else if (e.meta == 0x01) {
                if (e.n >= 2 && e.dado[0] == '@') {
                    if (e.dado[1] == 'K') m->info.karaoke = true;
                    else if (e.dado[1] == 'T') {
                        if (ntitulos == 0) mid_texto(m->info.titulo, sizeof m->info.titulo, e.dado + 2, e.n - 2);
                        else if (ntitulos == 1) mid_texto(m->info.autor, sizeof m->info.autor, e.dado + 2, e.n - 2);
                        ntitulos++;
                    }
                } else anotar_letra(m, ms, e.dado, e.n, LK_TEXT);
            }
        } else if ((e.st & 0xF0) == 0x90 && e.d2) m->info.notas++;
    }
    if (!m->info.tempo_ini) m->info.tempo_ini = 500000;
    m->info.duracao_ms = ms_fim;
    m->dur_q = ms_fim / 1000 * TAXA + ms_fim % 1000 * TAXA / 1000;
    /* la letra: los metaeventos 5 si los hay; si no, los de texto (solo en un .kar) */
    bool hay5 = false;
    for (unsigned i = 0; i < m->nletra; i++) if (m->letra[i].ms & 0x80000000u) { hay5 = true; break; }
    unsigned o = 0;
    for (unsigned i = 0; i < m->nletra; i++) {
        bool es5 = (m->letra[i].ms & 0x80000000u) != 0;
        if (hay5 ? es5 : m->info.karaoke) {
            m->letra[o] = m->letra[i];
            m->letra[o].ms &= 0x7FFFFFFFu;
            o++;
        }
    }
    m->nletra = o;
    m->info.letras = o;
}

/* ------------------------------------------------------------ voces */

static void canal_reiniciar(canal_t *c, bool tudo)
{
    c->vol = 100; c->expr = 127; c->pan = 64; c->mod = 0; c->bend = 0; c->rango = 2; c->pedal = false; c->rpn = 0x3FFF;
    if (tudo) c->prog = 0;
}

static void reiniciar_tudo(midi_t *m)
{
    memset(m->v, 0, sizeof m->v);
    for (int i = 0; i < MIDI_CANAIS; i++) canal_reiniciar(&m->c[i], true);
    memset(m->nv, 0, sizeof m->nv);
    m->agc = 32768;
    if (m->rev) memset(m->rev, 0, sizeof *m->rev);
}

static int32_t coef_ms(uint32_t ms)
{
    uint32_t tb = (ms * 1378u + 500) / 1000;   /* bloques de 32 cuadros */
    if (tb < 1) tb = 1;
    return (1 << 20) - (1 << 20) / (int32_t)tb;
}

static int escolher_voz(midi_t *m)
{
    int livre = -1, lib = -1, velha = -1;
    uint32_t menor = 0xFFFFFFFFu, ord = 0xFFFFFFFFu;
    for (int i = 0; i < NVOZES; i++) {
        voz_t *v = &m->v[i];
        if (!v->ativa) { livre = i; break; }
        if (v->fase == F_LIBERA && (uint32_t)v->nivel < menor) { menor = (uint32_t)v->nivel; lib = i; }
        if (v->ordem < ord) { ord = v->ordem; velha = i; }
    }
    if (livre >= 0) return livre;
    return lib >= 0 ? lib : velha;
}

static void nota_on(midi_t *m, int canal, int nota, int vel)
{
    if (m->mudo) return;
    canal_t *c = &m->c[canal];
    int i = escolher_voz(m);
    voz_t *v = &m->v[i];
    memset(v, 0, sizeof *v);
    v->ativa = true;
    v->canal = (uint8_t)canal;
    v->nota = (uint8_t)nota;
    v->vel = (uint8_t)vel;
    v->ordem = ++m->ordem;
    v->inicio = m->pos;
    v->velq = (uint32_t)vel * vel * 32768u / 16129u;
    v->ruido = 0x9E3779B9u ^ (m->ordem * 2654435761u);
    if (canal == 9) {
        if (nota < 35 || nota > 81) { v->ativa = false; return; }
        const perc_t *p = &g_perc[nota - 35];
        v->perc = true;
        v->tipo = 2;
        v->tab = g_seno;
        v->inc = (uint32_t)p->f0 * 97392u;
        v->inc1 = (uint32_t)p->f1 * 97392u;
        v->c_sweep = coef_ms(p->tms / 4 + 5);
        v->nivel = p->tl ? (int32_t)p->tl << 16 : 0;       /* Q24 = 255 << 16 */
        v->c_dec = coef_ms(p->tms / 3 + 1);
        v->nivel2 = (int32_t)p->nl << 16;
        v->c_n2 = coef_ms(p->nms / 3 + 1);
        v->modo = p->modo;
        v->kruido = p->k;
        v->niv = 255 * MASTER_Q;
        v->fase = F_DECAI;
        return;
    }
    patch_t pa;
    patch_de(c->prog & 127, &pa);
    int banda = banda_da_nota(nota);
    int tim = pa.fm ? T_SENO : pa.tim;
    if (!m->tab[tim][banda]) m->tab[tim][banda] = gerar_tabela(tim, banda);
    v->tab = m->tab[tim][banda];
    if (!v->tab) { v->ativa = false; return; }
    v->inc = g_incn[nota];
    v->ph = m->ordem * 0x9E3779B9u;                       /* fase inicial distinta: sin choque entre notas iguales */
    v->ph2 = v->ph + 0x40000000u;
    v->tipo = pa.fm ? 1 : 0;
    v->det = pa.det;
    v->vib = pa.vib;
    v->niv = (uint32_t)pa.niv * MASTER_Q;
    v->sus = (int32_t)pa.sus << 16;
    uint32_t atk_b = (pa.atk * 1378u + 500) / 1000;
    if (atk_b < 1) atk_b = 1;
    v->k_ata = (int32_t)((1u << 24) / atk_b);
    v->fase = F_ATAQUE;
    v->c_dec = coef_ms(pa.dec);
    v->c_lib = coef_ms(pa.rel < 15 ? 15 : pa.rel);
    int b0 = pa.bri0 * (128 + vel) / 255;
    if (b0 > 255) b0 = 255;
    v->bri = b0 << 8;
    v->bri1 = pa.bri1 << 8;
    v->c_bri = coef_ms(pa.brid);
    if (pa.fm) {
        v->fmr = pa.fm;
        v->fmi = pa.fmi * 16 * (96 + vel) / 223;
        v->c_fmi = coef_ms(pa.fmd);
    }
}

static void nota_off(midi_t *m, int canal, int nota)
{
    if (canal == 9) return;
    canal_t *c = &m->c[canal];
    int melhor = -1;
    for (int i = 0; i < NVOZES; i++) {
        voz_t *v = &m->v[i];
        if (!v->ativa || v->canal != canal || v->nota != nota || v->fase == F_LIBERA || v->sustentada) continue;
        if (melhor < 0 || v->ordem < m->v[melhor].ordem) melhor = i;
    }
    if (melhor < 0) return;
    voz_t *v = &m->v[melhor];
    if (c->pedal) v->sustentada = true;
    else v->fase = F_LIBERA;
}

static void soltar_pedal(midi_t *m, int canal)
{
    for (int i = 0; i < NVOZES; i++) {
        voz_t *v = &m->v[i];
        if (v->ativa && v->canal == canal && v->sustentada) { v->sustentada = false; v->fase = F_LIBERA; }
    }
}

static void todas_off(midi_t *m, int canal, bool ja)
{
    for (int i = 0; i < NVOZES; i++) {
        voz_t *v = &m->v[i];
        if (!v->ativa || v->canal != canal) continue;
        if (ja) v->ativa = false;
        else if (!v->perc) { v->sustentada = false; if (m->c[canal].pedal) v->sustentada = true; else v->fase = F_LIBERA; }
    }
}

static void controle(midi_t *m, int canal, int cc, int val)
{
    canal_t *c = &m->c[canal];
    switch (cc) {
    case 1: c->mod = (uint8_t)val; break;
    case 6: if (c->rpn == 0) c->rango = (uint8_t)(val > 24 ? 24 : val); break;
    case 7: c->vol = (uint8_t)val; break;
    case 10: c->pan = (uint8_t)val; break;
    case 11: c->expr = (uint8_t)val; break;
    case 64:
        if (val >= 64) c->pedal = true;
        else if (c->pedal) { c->pedal = false; soltar_pedal(m, canal); }
        break;
    case 100: c->rpn = (uint16_t)((c->rpn & 0x3F80) | val); break;
    case 101: c->rpn = (uint16_t)((c->rpn & 0x7F) | (val << 7)); break;
    case 120: todas_off(m, canal, true); break;
    case 121: { uint8_t p = c->prog; canal_reiniciar(c, false); c->prog = p; } break;
    case 123: todas_off(m, canal, false); break;
    default: break;
    }
}

static void aplicar(midi_t *m, const ev_t *e)
{
    if (e->st == 0xFF) {
        if (e->meta == 0x51 && e->n >= 3 && !m->smpte)
            definir_tempo(m, (uint32_t)e->dado[0] << 16 | e->dado[1] << 8 | e->dado[2]);
        return;
    }
    if (e->st == 0xF0) {   /* GM reset: F0 7E 7F 09 01 F7 */
        if (e->n >= 4 && e->dado[0] == 0x7E && e->dado[2] == 9 && e->dado[3] == 1) {
            for (int i = 0; i < MIDI_CANAIS; i++) { todas_off(m, i, true); canal_reiniciar(&m->c[i], true); }
        }
        return;
    }
    int ch = e->st & 15;
    switch (e->st & 0xF0) {
    case 0x80: nota_off(m, ch, e->d1); break;
    case 0x90: if (e->d2) nota_on(m, ch, e->d1, e->d2); else nota_off(m, ch, e->d1); break;
    case 0xB0: controle(m, ch, e->d1, e->d2); break;
    case 0xC0: m->c[ch].prog = e->d1; break;
    case 0xE0: m->c[ch].bend = (int16_t)((e->d1 | e->d2 << 7) - 8192); break;
    default: break;
    }
}

static void bombear_eventos(midi_t *m)
{
    while (m->tem && m->espera <= 0) {
        aplicar(m, &m->prox);
        carregar_proximo(m);
    }
}

/* ------------------------------------------------------------ síntesis */

static inline uint32_t inc_com_altura(uint32_t inc, int nota, int cents256)
{
    /* cents256: desplazamiento en 1/256 de semitono (con signo) */
    if (!cents256) return inc;
    int ent = cents256 >> 8, fr = cents256 & 255;
    int n = nota + ent;
    if (n < 0) n = 0;
    if (n > 139) n = 139;
    uint32_t b = g_incn[n];
    return (uint32_t)(((uint64_t)b * g_frac[fr]) >> 16);
}

static void passo_envolvente(voz_t *v)
{
    switch (v->fase) {
    case F_ATAQUE:
        v->nivel += v->k_ata;
        if (v->nivel >= (1 << 24)) { v->nivel = 1 << 24; v->fase = F_DECAI; }
        break;
    case F_DECAI: {
        int32_t d = v->nivel - v->sus;
        v->nivel = v->sus + (int32_t)(((int64_t)d * v->c_dec) >> 20);
        if (v->perc) {
            v->nivel2 = (int32_t)(((int64_t)v->nivel2 * v->c_n2) >> 20);
            if (v->nivel < (1 << 13) && v->nivel2 < (1 << 13)) v->ativa = false;
        } else if (v->sus == 0 && v->nivel < (1 << 13)) v->ativa = false;
        else if (v->sus && d < (1 << 12) && d > -(1 << 12)) { v->nivel = v->sus; v->fase = F_SUSTENTO; }
        break;
    }
    case F_SUSTENTO: break;
    case F_LIBERA:
        v->nivel = (int32_t)(((int64_t)v->nivel * v->c_lib) >> 20);
        if (v->nivel < (1 << 13)) v->ativa = false;
        break;
    }
    if (!v->perc) {
        int32_t d = v->bri - v->bri1;
        v->bri = v->bri1 + (int32_t)(((int64_t)d * v->c_bri) >> 20);
        if (v->tipo == 1) v->fmi = (int32_t)(((int64_t)v->fmi * v->c_fmi) >> 20);
    } else {
        v->inc = v->inc1 + (uint32_t)(((int64_t)((int32_t)(v->inc - v->inc1)) * v->c_sweep) >> 20);
    }
}

static void reverb_processar(reverb_t *r, int32_t *l, int32_t *rr)
{
    int32_t in = (*l + *rr) >> 4;
    int32_t saida[2];
    for (int lado = 0; lado < 2; lado++) {
        int32_t soma = 0;
        for (int k = 0; k < REV_COMBS; k++) {
            uint16_t i = r->comb_i[lado][k];
            int32_t o = r->comb[lado][k][i];
            r->comb_lp[lado][k] += ((o - r->comb_lp[lado][k]) * 16384) >> 15;   /* amortiguación */
            int32_t n = in + ((r->comb_lp[lado][k] * 27300) >> 15);
            if (n > 32767) n = 32767;
            if (n < -32768) n = -32768;
            r->comb[lado][k][i] = (int16_t)n;
            if (++i >= g_comb_len[lado][k]) i = 0;
            r->comb_i[lado][k] = i;
            soma += o;
        }
        /* paso-todo */
        uint16_t i = r->ap_i[lado];
        int32_t b = r->ap[lado][i];
        int32_t y = b - soma;
        int32_t nv = soma + (b >> 1);
        if (nv > 32767) nv = 32767;
        if (nv < -32768) nv = -32768;
        r->ap[lado][i] = (int16_t)nv;
        if (++i >= g_ap_len[lado]) i = 0;
        r->ap_i[lado] = i;
        saida[lado] = y;
    }
    /* mezcla húmeda: ~ 1/4 del peine sumado */
    *l += -saida[0] >> 2;
    *rr += -saida[1] >> 2;
}

static void renderizar_voz(midi_t *m, voz_t *v, int32_t *mixl, int32_t *mixr, int n)
{
    canal_t *c = &m->c[v->canal];
    /* altura */
    int sv = (c->bend * c->rango) >> 5;                    /* 1/256 de semitono */
    int vib = (int)v->vib * 4 + c->mod * 2;
    if (vib && !v->perc) {
        int lfo = seno_fase(m->lfo);                       /* ±32767 */
        sv += (lfo * vib) >> 15;                           /* hasta ±(vib/256) semitonos */
    }
    /* ganancia */
    uint32_t ch = ((uint32_t)c->vol * c->expr) >> 7;
    uint32_t chq = ch * ch * 32768u / 16129u;
    int64_t g = (int64_t)(v->nivel >> 8) * v->velq >> 15;
    g = g * chq >> 15;
    g = g * v->niv >> 15;
    uint32_t pidx = (uint32_t)c->pan * 256 / 128;          /* 0-255 -> cuarto de onda 0-256 */
    if (pidx > 255) pidx = 255;
    int pl = g_seno[(pidx + 256) & (TAB_N - 1)];           /* cos */
    int pr = g_seno[pidx & (TAB_N - 1)];                   /* sen */
    pl = pl < 0 ? 0 : pl;
    int32_t gl = (int32_t)(g * pl >> 15), gr = (int32_t)(g * pr >> 15);
    int32_t dgl = (gl - v->gl) / n, dgr = (gr - v->gr) / n;
    int32_t cgl = v->gl, cgr = v->gr;
    if (!v->gl && !v->gr) { cgl = gl; cgr = gr; dgl = dgr = 0; }
    if (v->perc) {
        int32_t g2 = (int32_t)(((int64_t)(v->nivel2 >> 8) * v->velq >> 15) * chq >> 15);
        g2 = (int32_t)((int64_t)g2 * v->niv >> 15);
        uint32_t inc = v->inc;
        int32_t tn_gain_l = gl, tn_gain_r = gr;
        int32_t n_l = (int32_t)((int64_t)g2 * pl >> 15), n_r = (int32_t)((int64_t)g2 * pr >> 15);
        for (int i = 0; i < n; i++) {
            v->ph += inc;
            int t = seno_fase(v->ph);
            uint32_t r = v->ruido;
            r ^= r << 13; r ^= r >> 17; r ^= r << 5;
            v->ruido = r;
            int32_t x = (int16_t)(r >> 16);
            if (v->modo == 1) { v->lp2 += ((x - v->lp2) * (int32_t)v->kruido) >> 15; x -= v->lp2; }
            else if (v->modo == 2) { v->lp2 += ((x - v->lp2) * (int32_t)v->kruido) >> 15; x = v->lp2; }
            int32_t sl = (int32_t)(((int64_t)t * tn_gain_l) >> 16) + (int32_t)(((int64_t)x * n_l) >> 16);
            int32_t sr = (int32_t)(((int64_t)t * tn_gain_r) >> 16) + (int32_t)(((int64_t)x * n_r) >> 16);
            mixl[i] += sl;
            mixr[i] += sr;
        }
        v->gl = gl; v->gr = gr;
        return;
    }
    uint32_t inc = inc_com_altura(v->inc, v->nota, sv);
    uint32_t inc2 = v->det ? inc + (uint32_t)(((uint64_t)inc * v->det * 38) >> 16) : 0;
    /* filtro: k = f(inc, brillo) */
    uint32_t b8 = (uint32_t)v->bri >> 8;
    uint32_t rq4 = 16 + ((b8 * b8) >> 7);
    uint32_t x = (inc >> 12) * rq4;
    uint32_t kk = (uint32_t)(((uint64_t)x * 805) >> 16);
    if (kk > 32767) kk = 32767;
    int32_t k = (int32_t)kk;
    const int16_t *tab = v->tab;
    uint32_t ph = v->ph, ph2 = v->ph2;
    int32_t lp = v->lp;
    if (v->tipo == 1) {
        uint32_t incm = (uint32_t)(((uint64_t)inc * v->fmr) >> 2);
        uint32_t phm = v->phm;
        int32_t idx = v->fmi;
        for (int i = 0; i < n; i++) {
            ph += inc;
            phm += incm;
            int md = seno_fase(phm);
            uint32_t off = (uint32_t)(md * idx) * 81u;
            int s = seno_fase(ph + off);
            if (inc2) { ph2 += inc2; s = (s + seno_fase(ph2 + off)) >> 1; }
            lp += ((s - lp) * k) >> 15;
            cgl += dgl; cgr += dgr;
            mixl[i] += (int32_t)(((int64_t)lp * cgl) >> 16);
            mixr[i] += (int32_t)(((int64_t)lp * cgr) >> 16);
        }
        v->phm = phm;
    } else {
        for (int i = 0; i < n; i++) {
            ph += inc;
            uint32_t ti = ph >> 22, fr = (ph >> 6) & 0xFFFF;
            int a = tab[ti], b = tab[(ti + 1) & (TAB_N - 1)];
            int s = a + (((b - a) * (int)fr) >> 16);
            if (inc2) {
                ph2 += inc2;
                ti = ph2 >> 22; fr = (ph2 >> 6) & 0xFFFF;
                a = tab[ti]; b = tab[(ti + 1) & (TAB_N - 1)];
                s = (s + a + (((b - a) * (int)fr) >> 16)) >> 1;
            }
            lp += ((s - lp) * k) >> 15;
            cgl += dgl; cgr += dgr;
            mixl[i] += (int32_t)(((int64_t)lp * cgl) >> 16);
            mixr[i] += (int32_t)(((int64_t)lp * cgr) >> 16);
        }
    }
    v->ph = ph; v->ph2 = ph2; v->lp = lp;
    v->gl = gl; v->gr = gr;
}

static inline int16_t limitar(int32_t x)
{
    int32_t a = x < 0 ? -x : x;
    if (a > 20000) {
        int32_t d = a - 20000;
        a = 20000 + d * 12767 / (d + 12767);
        if (a > 32767) a = 32767;
    }
    return (int16_t)(x < 0 ? -a : a);
}

static void tirar_instantanea(midi_t *m)
{
    snap_t *s = &m->snap[m->snap_n % NSNAP];
    memset(s, 0, sizeof *s);
    s->quadro = m->pos;
    uint32_t soma[MIDI_CANAIS] = { 0 };
    int nv = 0;
    for (int i = 0; i < NVOZES; i++) {
        voz_t *v = &m->v[i];
        if (!v->ativa) continue;
        nv++;
        bool soa = v->perc ? (m->pos - v->inicio < TAXA / 8) : (v->fase != F_LIBERA || v->sustentada);
        if (soa) s->ativas[v->canal][v->nota >> 5] |= 1u << (v->nota & 31);
        uint32_t a = (uint32_t)(v->nivel >> 16) * v->vel / 127;
        if (v->perc) a = (a > 255 ? 255 : a) * 3 / 4;
        soma[v->canal] += a;
    }
    for (int c = 0; c < MIDI_CANAIS; c++) {
        uint32_t x = soma[c] > 255 ? 255 : soma[c];
        uint32_t ant = m->nv[c];
        m->nv[c] = (uint8_t)(x > ant ? x : ant - (ant >> 3) - (ant ? 1 : 0));
        s->nivel[c] = m->nv[c];
        s->programa[c] = m->c[c].prog;
        s->volume[c] = (uint8_t)(((uint32_t)m->c[c].vol * m->c[c].expr) >> 7);
        s->pan[c] = m->c[c].pan;
        if (c < 8 && m->c[c].pedal) s->pedal |= 1u << c;
    }
    s->vozes = (uint8_t)nv;
    m->snap_n++;
    m->snap_ult = m->pos;
}

static void renderizar(midi_t *m, int16_t *pcm, int n)
{
    int32_t ml[BLOCO] = { 0 }, mr[BLOCO] = { 0 };
    if ((m->pos & (BLOCO - 1)) == 0) {
        for (int i = 0; i < NVOZES; i++) if (m->v[i].ativa) passo_envolvente(&m->v[i]);
        m->lfo += 0x00F0F0F0u / 1;      /* ~5,5 Hz por bloque de 32 cuadros */
    }
    for (int i = 0; i < NVOZES; i++) if (m->v[i].ativa) renderizar_voz(m, &m->v[i], ml, mr, n);
    int32_t pico = 0;
    for (int i = 0; i < n; i++) {
        reverb_processar(m->rev, &ml[i], &mr[i]);
        int32_t a = ml[i] < 0 ? -ml[i] : ml[i], b = mr[i] < 0 ? -mr[i] : mr[i];
        if (a > pico) pico = a;
        if (b > pico) pico = b;
    }
    /* control automático de ganancia: baja deprisa si la mezcla pasa de 20000 y sube despacio */
    int32_t g0 = m->agc, g1 = g0;
    if (pico > 20000) { int32_t alvo = (int32_t)(655360000u / (uint32_t)pico); if (alvo < g1) g1 = alvo; }
    else g1 += (32768 - g1) >> 10;
    m->agc = g1;
    int32_t dg = (g1 - g0) / n, g = g0;
    for (int i = 0; i < n; i++) {
        g += dg;
        pcm[2 * i] = limitar((int32_t)(((int64_t)ml[i] * g) >> 15));
        pcm[2 * i + 1] = limitar((int32_t)(((int64_t)mr[i] * g) >> 15));
    }
}

static unsigned vozes_ativas(const midi_t *m)
{
    unsigned n = 0;
    for (int i = 0; i < NVOZES; i++) n += m->v[i].ativa;
    return n;
}

int midi_ler(midi_t *m, int16_t *pcm, unsigned quadros)
{
    if (!m) return 0;
    unsigned feito = 0;
    while (feito < quadros) {
        bombear_eventos(m);
        if (!m->tem && (vozes_ativas(m) == 0 && m->cauda >= TAXA / 2)) break;
        if (!m->tem && m->cauda >= CAUDA_MAX) break;
        unsigned chunk = quadros - feito;
        unsigned ate_bloco = BLOCO - (m->pos & (BLOCO - 1));
        if (chunk > ate_bloco) chunk = ate_bloco;
        if (m->tem) {
            int64_t e = m->espera;
            unsigned ate_ev = (unsigned)((e + 65535) >> 16);
            if (ate_ev < chunk) chunk = ate_ev ? ate_ev : 1;
            m->espera -= (int64_t)chunk << 16;
        } else m->cauda += chunk;
        renderizar(m, pcm + 2 * feito, (int)chunk);
        m->pos += chunk;
        feito += chunk;
        if (m->pos - m->snap_ult >= SNAP_CADA) tirar_instantanea(m);
    }
    if (m->pos - m->snap_ult >= SNAP_CADA / 2) tirar_instantanea(m);
    return (int)feito;
}

uint32_t midi_posicao(const midi_t *m) { return m ? m->pos : 0; }

int midi_buscar(midi_t *m, uint32_t ms)
{
    if (!m) return AUDIO_ERRO_FORMATO;
    uint32_t alvo = ms / 1000 * TAXA + ms % 1000 * TAXA / 1000;
    reiniciar_tudo(m);
    cur_iniciar(m, &m->cur);
    m->tick_ant = 0;
    m->espera = 0;
    definir_tempo(m, m->smpte ? 500000 : 500000);
    m->cauda = 0;
    m->snap_n = 0;
    m->snap_ult = 0;
    carregar_proximo(m);
    m->mudo = true;
    int64_t resta = (int64_t)alvo << 16;
    while (m->tem) {
        int64_t passo = m->espera > 0 ? m->espera : 0;
        if (passo > resta) { m->espera -= resta; resta = 0; break; }
        resta -= passo;
        m->espera -= passo;
        aplicar(m, &m->prox);
        carregar_proximo(m);
    }
    m->mudo = false;
    m->pos = alvo;
    m->snap_ult = alvo;
    if (!m->tem) m->cauda = alvo > m->dur_q ? alvo - m->dur_q : 0;
    return 0;
}

void midi_estado_em(const midi_t *m, uint32_t quadro, midi_estado_t *e)
{
    memset(e, 0, sizeof *e);
    if (!m || !m->snap_n) { for (int i = 0; i < MIDI_CANAIS; i++) { e->volume[i] = 100; e->pan[i] = 64; } return; }
    uint32_t n = m->snap_n;
    uint32_t lim = n > NSNAP ? NSNAP : n;
    const snap_t *melhor = &m->snap[(n - 1) % NSNAP];
    bool achou = false;
    for (uint32_t k = 0; k < lim; k++) {
        const snap_t *s = &m->snap[(n - 1 - k) % NSNAP];
        if (s->quadro <= quadro) { melhor = s; achou = true; break; }
        melhor = s;   /* si todas son posteriores, la más antigua */
    }
    (void)achou;
    memcpy(e->ativas, melhor->ativas, sizeof e->ativas);
    memcpy(e->nivel, melhor->nivel, sizeof e->nivel);
    memcpy(e->programa, melhor->programa, sizeof e->programa);
    memcpy(e->volume, melhor->volume, sizeof e->volume);
    memcpy(e->pan, melhor->pan, sizeof e->pan);
    e->pedal = melhor->pedal;
    e->vozes = melhor->vozes;
    e->quadro = melhor->quadro;
}

/* ------------------------------------------------------------ letra */

unsigned midi_letra_qtd(const midi_t *m) { return m ? m->nletra : 0; }
const midi_letra_t *midi_letra(const midi_t *m, unsigned i) { return m && i < m->nletra ? &m->letra[i] : NULL; }

int midi_letra_indice(const midi_t *m, uint32_t ms)
{
    int r = -1;
    for (unsigned i = 0; m && i < m->nletra; i++) {
        if (m->letra[i].ms <= ms) r = (int)i;
        else break;
    }
    return r;
}

/* ------------------------------------------------------------ abrir y cerrar */

const midi_info_t *midi_info(const midi_t *m) { return &m->info; }

void midi_fechar(midi_t *m)
{
    if (!m) return;
    for (int i = 0; i < NTIM; i++) for (int b = 0; b < NBANDAS; b++) if (m->tab[i][b]) libera(m->tab[i][b]);
    libera(m->rev);
    libera(m->letra);
    libera(m->pool);
    libera(m->arq);
    libera(m);
}

midi_t *midi_abrir_memoria(const uint8_t *dados, uint32_t tam, int *erro)
{
    int dummy;
    if (!erro) erro = &dummy;
    if (tam < 14 || memcmp(dados, "MThd", 4) != 0) { *erro = AUDIO_ERRO_FORMATO; return NULL; }
    uint32_t hl = be32(dados + 4);
    if (hl < 6 || hl + 8 > tam) { *erro = AUDIO_ERRO_CORROMPIDO; return NULL; }
    int fmt = (int)be16(dados + 8);
    uint32_t div = be16(dados + 12);
    if (fmt > 1) { *erro = AUDIO_ERRO_NAO_SUPORTADO; return NULL; }
    iniciar_tabelas();
    midi_t *m = aloca_zerado(sizeof *m);
    if (!m) { *erro = ERRO_SEM_MEMORIA; return NULL; }
    m->arq = aloca(tam);
    m->rev = aloca_zerado(sizeof(reverb_t));
    m->letra = aloca(MAX_LETRAS * sizeof(midi_letra_t));
    m->pool = aloca(LETRA_POOL);
    if (!m->arq || !m->rev || !m->letra || !m->pool) { *erro = ERRO_SEM_MEMORIA; midi_fechar(m); return NULL; }
    memcpy(m->arq, dados, tam);
    m->arq_tam = tam;
    m->info.formato = fmt;
    if (div & 0x8000) {
        int fps = (int)(0x100 - (div >> 8)), sub = (int)(div & 0xFF);
        if (fps == 29) fps = 30;
        uint32_t tics = (uint32_t)(fps * sub);
        m->smpte = true;
        m->ppqn = tics / 2 ? tics / 2 : 1;
    } else m->ppqn = div ? div : 96;
    m->info.divisao = m->ppqn;
    definir_tempo(m, 500000);
    prescan(m);
    /* una pieza de más de 2 horas es un archivo estropeado (tempo o tics absurdos): no se sintetiza */
    if (m->info.duracao_ms > 2u * 3600u * 1000u) { *erro = AUDIO_ERRO_CORROMPIDO; midi_fechar(m); return NULL; }
    int nt = cur_iniciar(m, &m->cur);
    if (nt <= 0) { *erro = AUDIO_ERRO_CORROMPIDO; midi_fechar(m); return NULL; }
    m->info.trilhas = nt;
    reiniciar_tudo(m);
    m->tick_ant = 0;
    m->espera = 0;
    definir_tempo(m, 500000);
    carregar_proximo(m);
    *erro = 0;
    return m;
}

midi_t *midi_abrir(const char *caminho, int *erro)
{
    int dummy;
    if (!erro) erro = &dummy;
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) { *erro = fd; return NULL; }
    int t = posicionar(fd, 0, POS_FIM);
    posicionar(fd, 0, POS_INICIO);
    if (t < 14) { fechar(fd); *erro = t < 0 ? AUDIO_ERRO_TRUNCADO : AUDIO_ERRO_FORMATO; return NULL; }
    if ((uint32_t)t > MAX_ARQ) { fechar(fd); *erro = AUDIO_ERRO_NAO_SUPORTADO; return NULL; }
    uint8_t *buf = aloca((size_t)t);
    if (!buf) { fechar(fd); *erro = ERRO_SEM_MEMORIA; return NULL; }
    uint32_t got = 0;
    while (got < (uint32_t)t) {
        uint32_t q = (uint32_t)t - got;
        if (q > 4096) q = 4096;   /* trozos pequeños: el pendrive lee con las interrupciones desactivadas (D089) */
        int r = ler(fd, buf + got, q);
        if (r <= 0) break;
        got += (uint32_t)r;
    }
    fechar(fd);
    if (got < (uint32_t)t) { libera(buf); *erro = AUDIO_ERRO_TRUNCADO; return NULL; }
    midi_t *m = midi_abrir_memoria(buf, got, erro);
    libera(buf);
    return m;
}
