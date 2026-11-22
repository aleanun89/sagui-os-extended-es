/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382, D431. */
/* Mezclador de efectos independiente de los FPS (D431): ver mezcla.h. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mezcla.h"

typedef struct { const int16_t *pcm; int n; float vl, vr; int loop; } Orden;
typedef struct { const int16_t *pcm; int n, pos; float vl, vr; int loop; } Voz;

/* cola de un productor (hilo del juego) y un consumidor (hilo de audio): cada índice lo escribe uno solo */
static Orden cola[MEZCLA_COLA];
static volatile unsigned cola_esc, cola_lec;
static Voz voces[MEZCLA_VOCES];                 /* solo las toca el hilo de audio */
static void (*fn_musica)(int16_t *, int);
static volatile long long st_frames;
static volatile long st_llamadas, st_eventos, st_perdidos;
static int16_t *grab;
static long long grab_max, grab_n;

int mezcla_voz(const int16_t *pcm, int n, float vl, float vr, int loop) {
	if (!pcm || n <= 0) return 0;
	unsigned e = cola_esc;
	if (e - cola_lec >= MEZCLA_COLA) { ++st_perdidos; return 0; }
	Orden *o = &cola[e & (MEZCLA_COLA - 1)];
	o->pcm = pcm; o->n = n; o->vl = vl; o->vr = vr; o->loop = loop;
	MEZCLA_BARRERA();
	cola_esc = e + 1;
	return 1;
}

void mezcla_musica(void (*fn)(int16_t *, int)) { fn_musica = fn; }

static void tomar_ordenes(void) {
	unsigned l = cola_lec;
	MEZCLA_BARRERA();
	while (l != cola_esc) {
		const Orden *o = &cola[l & (MEZCLA_COLA - 1)];
		int v = -1;
		for (int i = 0; i < MEZCLA_VOCES; ++i) if (!voces[i].pcm) { v = i; break; }
		/* todas ocupadas: no se quita ninguna, se descarta el efecto nuevo */
		if (v >= 0) {
			voces[v].pcm = o->pcm; voces[v].n = o->n; voces[v].pos = 0;
			voces[v].vl = o->vl; voces[v].vr = o->vr; voces[v].loop = o->loop;
		} else ++st_perdidos;
		++st_eventos;
		++l;
	}
	MEZCLA_BARRERA();
	cola_lec = l;
}

void mezcla_render(int16_t *out, int frames) {
	memset(out, 0, (size_t)frames * 2 * sizeof(int16_t));
	tomar_ordenes();
	if (fn_musica) fn_musica(out, frames);

	for (int v = 0; v < MEZCLA_VOCES; ++v) {
		Voz *vo = &voces[v];
		if (!vo->pcm) continue;
		for (int i = 0; i < frames; ++i) {
			if (vo->pos >= vo->n) {
				if (vo->loop) vo->pos = 0;
				else { vo->pcm = NULL; break; }
			}
			int s = vo->pcm[vo->pos++];
			int l = out[i * 2] + (int)(s * vo->vl), r = out[i * 2 + 1] + (int)(s * vo->vr);
			out[i * 2] = (int16_t)(l < -32768 ? -32768 : l > 32767 ? 32767 : l);
			out[i * 2 + 1] = (int16_t)(r < -32768 ? -32768 : r > 32767 ? 32767 : r);
		}
	}

	if (grab && grab_n < grab_max) {
		long long n = frames;
		if (grab_n + n > grab_max) n = grab_max - grab_n;
		memcpy(grab + grab_n * 2, out, (size_t)n * 2 * sizeof(int16_t));
		grab_n += n;
	}
	st_frames += frames;
	++st_llamadas;
}

int mezcla_grabar(int max_frames) {
	free(grab);
	grab = (int16_t *)malloc((size_t)max_frames * 2 * sizeof(int16_t));
	grab_max = grab ? max_frames : 0;
	grab_n = 0;
	return grab != NULL;
}

static void le32(unsigned char *p, uint32_t v) { p[0] = v & 255; p[1] = (v >> 8) & 255; p[2] = (v >> 16) & 255; p[3] = (v >> 24) & 255; }

int mezcla_grabar_guardar(const char *ruta) {
	if (!grab) return 0;
	FILE *f = fopen(ruta, "wb");
	if (!f) return 0;
	uint32_t bytes = (uint32_t)(grab_n * 4);
	unsigned char h[44];
	memcpy(h, "RIFF", 4); le32(h + 4, 36 + bytes); memcpy(h + 8, "WAVEfmt ", 8); le32(h + 16, 16);
	h[20] = 1; h[21] = 0; h[22] = 2; h[23] = 0; le32(h + 24, MEZCLA_RATE); le32(h + 28, MEZCLA_RATE * 4);
	h[32] = 4; h[33] = 0; h[34] = 16; h[35] = 0; memcpy(h + 36, "data", 4); le32(h + 40, bytes);
	fwrite(h, 1, 44, f);
	fwrite(grab, 1, bytes, f);
	fclose(f);
	return 1;
}

MezclaEst mezcla_estadisticas(void) {
	MezclaEst e;
	e.frames = st_frames; e.llamadas = st_llamadas; e.eventos = st_eventos; e.perdidos = st_perdidos;
	return e;
}

void mezcla_reiniciar(void) {
	memset(voces, 0, sizeof voces);
	cola_lec = cola_esc = 0;
	st_frames = 0; st_llamadas = st_eventos = st_perdidos = 0;
	grab_n = 0;
}
