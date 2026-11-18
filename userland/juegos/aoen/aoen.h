/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* AoEN - declaraciones compartidas. C99, solo SDL2. */
#ifndef AOEN_H
#define AOEN_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;

#define MAX_PLAYERS 8

/* Registro de sucesos (D382): en SaguiOS va al registro del sistema (serie), donde lo leen las pruebas; en el PC, a stderr. */
#ifdef SAGUI_AOEN
void aoen_registro(const char *texto);
#else
#define aoen_registro(t) fprintf(stderr, "%s\n", (t))
#endif
#define AOEN_LOG(...) do { char _lb[400]; snprintf(_lb, sizeof _lb, __VA_ARGS__); aoen_registro(_lb); } while (0)
#define ARRAY_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))

/* ---------- data.c: datos originales del juego (archivos DRS, sprites SLP, paletas) ---------- */

typedef struct {
	int w, h, hx, hy;
	u8 *px;   /* índices de paleta w*h, 0 = transparente */
	u8 *pc;   /* indicadores w*h: 1 = píxel del color del jugador (px guarda el índice base) */
} Sprite;

typedef struct {
	u32 id, offset, size;
} DrsItem;

typedef struct {
	FILE *f;
	DrsItem *items;
	int count;
	long len;          /* tamaño del fichero (se valida todo lo que dice la cabecera contra él) */
} Drs;

typedef struct {
	u8 *data;        /* slp sin procesar */
	int size;
	int nframes;
	Sprite **frames; /* decodificado de forma diferida */
} Slp;

int drs_open(Drs *d, const char *path);
void drs_close(Drs *d);
u8 *drs_read(Drs *d, u32 id, int *size); /* reservado con malloc, lo libera quien llama */
Slp *slp_load(Drs *d, u32 id);           /* NULL si falta */
Sprite *slp_frame(Slp *s, int index);
int pal_load(Drs *d, u32 id, u32 *rgb256);   /* paleta JASC -> 0x00RRGGBB */

/* ---------- rapido.c: rutas rápidas de píxeles (SSE2 por CPUID en SaguiOS; siempre con alternativa en C) ---------- */
int px_sse2(void);
void px_fill(u32 *d, u32 col, int n);
void px_blend(u32 *d, const u32 *o, int n, int a);     /* d = (o*a + d*(256-a)) >> 8 por canal, a 0..256 */
void px_blend_c(u32 *d, const u32 *o, int n, int a);
void px_dim(u32 *d, int n, int shift, u32 mask);       /* d = (d >> shift) & mask, opaco */

/* ---------- gfx.c: renderizador por software ---------- */

typedef struct {
	u32 *px;
	int w, h, pitch; /* paso de línea en píxeles */
	u32 pal[256];    /* paleta en el formato de destino */
	int cx0, cy0, cx1, cy1; /* rectángulo de recorte (x1,y1 excluido), en píxeles reales */
} Fb;

extern Fb fb;

u32 gfx_rgb(int r, int g, int b);
void gfx_clip(int x0, int y0, int x1, int y1);
void gfx_clip_reset(void);
int gfx_real_w(void);
int gfx_real_h(void);
void gfx_set_size(int w, int h);                 /* tamaño real del búfer */
extern int gfx_us, gfx_smooth;
void gfx_ui_begin(int us);                       /* a partir de aquí las coordenadas son lógicas y se multiplican por us */
void gfx_ui_end(void);
int gfx_ui_suspend(void);
void gfx_ui_resume(int s);
void gfx_pal_changed(void);                      /* avisa de que cambió fb.pal (se guardan paletas con sombra) */
void gfx_fill_grad(int x, int y, int w, int h, u32 top, u32 bottom);
void gfx_ellipse(int cx, int cy, int rx, int ry, u32 col, int alpha);
void gfx_shadow_ellipse(int cx, int cy, int rx, int ry);
void gfx_dim(int x, int y, int w, int h);
void gfx_fill(int x, int y, int w, int h, u32 col);
void gfx_fill_a(int x, int y, int w, int h, u32 col, int alpha); /* alfa 0..255 */
void gfx_rect(int x, int y, int w, int h, u32 col);
void gfx_line(int x0, int y0, int x1, int y1, u32 col);
/* dibuja el sprite con el punto de anclaje en x,y. Escala en unidades de 1/256, color de jugador 0..7, sombra 0..256 (256 = normal) */
void gfx_sprite(const Sprite *s, int x, int y, int scale, int player, int shade, int flip);
void gfx_diamond(int cx, int cy, int hw, int hh, u32 col);
void gfx_text(int x, int y, const char *s, u32 col, int size);
int gfx_text_w(const char *s, int size);
void gfx_text_c(int cx, int y, const char *s, u32 col, int size);
/* texto de la interfaz (fuente de 8x16 de SaguiOS, con minúsculas y acentos); tam 1 o 2; gfx_ts lleva sombra */
#define GFX_T_ALTO 16
int gfx_t_w(const char *s, int tam);
void gfx_t(int x, int y, const char *s, u32 col, int tam);
void gfx_ts(int x, int y, const char *s, u32 col, int tam);


/* ---------- audio.c: efectos de sonido mezclados por software y reproducidos con el audio de SDL ---------- */

typedef enum {
	SND_CLICK, SND_ERROR, SND_PLACE, SND_BUILD_DONE, SND_TRAIN_VILL, SND_TRAIN_MIL, SND_HIT_MELEE, SND_SHOOT,
	SND_WOOD, SND_MINE, SND_FORAGE, SND_DIE_UNIT, SND_DIE_BLD, SND_SEL_VILL, SND_SEL_MIL, SND_SEL_BLD, SND_SEL_TC,
	SND_ALERT, SND_AGE, SND_VICTORY, SND_DEFEAT, SND_CONVERT, SND_HEAL, SND_COUNT
} SndEvent;

int audio_init(const char *data_dir);   /* devuelve 0 si no hay dispositivo de audio */
void audio_shutdown(void);
void snd_event(SndEvent ev, float wx, float wy); /* posicional (casillas), atenuado según la distancia a la cámara */
void snd_ui(SndEvent ev);                        /* no posicional */
void audio_set_volumes(float sfx, float ambience);
void audio_ambience_tick(float dt);

/* ---------- music.c: secuenciador MIDI y pequeño sintetizador por software ---------- */
int music_setup(const char *data_dir);      /* busca los ficheros MIDI; devuelve cuántas pistas hay */
void music_play_menu(void);
void music_play_game(void);
void music_play_result(int victory);
void music_stop(void);
void music_update(void);                    /* llamar una vez por cuadro: arranca la siguiente pista cuando termina una */
void music_set_volume(float v);
float music_volume(void);
void music_render(int16_t *out, int frames); /* añade la música a un búfer estéreo intercalado (hilo de audio) */
int music_count(void);
const char *music_current(void);
void music_test(void);
float audio_sfx_volume(void);
float audio_ambience_volume(void);

#endif
