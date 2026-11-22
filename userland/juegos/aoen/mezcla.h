/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382, D431. */
/* Mezclador de efectos independiente de los FPS (D431). No usa la SDL: lo llama el hilo de audio (la llamada de vuelta de la
 * SDL o el hilo propio de SaguiOS) y lo alimenta el hilo del juego por una cola de eventos de un productor y un consumidor,
 * sin cerrojos y sin bloquear nunca a quien mezcla. Así el audio avanza al ritmo de la tarjeta, no al de los cuadros. */
#ifndef AOEN_MEZCLA_H
#define AOEN_MEZCLA_H
#include <stdint.h>

#define MEZCLA_RATE 44100
#define MEZCLA_VOCES 24
#define MEZCLA_COLA 256          /* eventos de sonido pendientes (potencia de 2) */

/* hilo del juego: dispara un efecto (pcm mono a MEZCLA_RATE; el búfer debe seguir vivo). 0 si la cola está llena */
int mezcla_voz(const int16_t *pcm, int n, float vl, float vr, int loop);
/* música: función que SUMA su salida (estéreo intercalado) en el búfer; se llama desde el hilo de audio */
void mezcla_musica(void (*fn)(int16_t *out, int frames));
/* hilo de audio: llena «frames» cuadros estéreo (primero los pone a cero) */
void mezcla_render(int16_t *out, int frames);

/* grabación de la salida (pruebas): reserva sitio para «max_frames» cuadros y, al final, escribe un WAV */
int mezcla_grabar(int max_frames);
int mezcla_grabar_guardar(const char *ruta);

typedef struct {
	long long frames;            /* cuadros de audio mezclados */
	long llamadas;               /* veces que se ha llamado a mezcla_render */
	long eventos;                /* efectos recibidos por la cola */
	long perdidos;               /* efectos descartados (cola llena o sin voz libre) */
} MezclaEst;
MezclaEst mezcla_estadisticas(void);
void mezcla_reiniciar(void);     /* voces, cola y cuentas a cero (solo con el audio parado) */

/* barrera entre escribir el dato y publicar el índice (x86 ya ordena las escrituras; el resto del mundo, no) */
#if defined(__GNUC__) && !defined(__TINYC__)
#define MEZCLA_BARRERA() __sync_synchronize()
#else
#define MEZCLA_BARRERA() ((void)0)
#endif

#endif
