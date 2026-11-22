/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Arnés de h431 para el anfitrión: el mezclador de AoEN (mezcla.c) con un hilo de audio al ritmo del reloj y un hilo de «juego» lento.
 *   aoen_audio SALIDA.wav SEGUNDOS MS_CUADRO MS_PARADA
 * El «juego» deja un efecto (0,1 s, valores ±3000 alternos, siempre distinto de cero) cada 250 ms de reloj, con cuadros de MS_CUADRO
 * ms (ocupado, no dormido) y una parada de MS_PARADA ms a mitad (pausa o carga). La «música» es un seno de 440 Hz continuo
 * (valor n = 8000*sen(2*pi*440*n/44100) para el cuadro n de audio). Imprime una línea «evento CUADRO_DE_AUDIO_AL_DISPARAR» por cada
 * efecto y un resumen; el WAV es lo que salió del hilo de audio. */
#define _POSIX_C_SOURCE 200809L
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../userland/juegos/aoen/mezcla.h"

static volatile int fin;
static long long n_musica;
static void musica(int16_t *out, int frames) {
	for (int i = 0; i < frames; ++i, ++n_musica) {
		int v = (int)lrint(8000.0 * sin(2.0 * M_PI * 440.0 * (double)n_musica / MEZCLA_RATE));
		int l = out[i * 2] + v, r = out[i * 2 + 1] + v;
		out[i * 2] = (int16_t)l; out[i * 2 + 1] = (int16_t)r;
	}
}

static double ahora(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000.0 + t.tv_nsec / 1e6; }
static void dormir_ms(double ms) { struct timespec t = { (time_t)(ms / 1000), (long)((ms - (long)(ms / 1000) * 1000) * 1e6) }; nanosleep(&t, NULL); }

static void *hilo_audio(void *a) {
	(void)a;
	static int16_t buf[1024 * 2];
	double t0 = ahora();
	long long enviados = 0;
	while (!fin) {
		/* la tarjeta consume 44100 cuadros por segundo: se mezcla un bloque cuando toca, con 100 ms de adelanto */
		double tocado = (ahora() - t0) * MEZCLA_RATE / 1000.0;
		if (enviados - tocado < MEZCLA_RATE / 10) { mezcla_render(buf, 1024); enviados += 1024; }
		else dormir_ms(2);
	}
	return NULL;
}

int main(int argc, char **argv) {
	if (argc < 5) return 2;
	double seg = atof(argv[2]), cuadro = atof(argv[3]), parada = atof(argv[4]);
	static int16_t efecto[4410];
	for (int i = 0; i < 4410; ++i) efecto[i] = (i & 1) ? -3000 : 3000;
	mezcla_reiniciar();
	mezcla_musica(musica);
	mezcla_grabar((int)((seg + 2) * MEZCLA_RATE));
	pthread_t th;
	pthread_create(&th, NULL, hilo_audio, NULL);
	double t0 = ahora(), prox = 250, mitad = seg * 500, hecha = 0;
	while (ahora() - t0 < seg * 1000) {
		double t = ahora() - t0;
		if (t >= prox) { printf("evento %lld\n", mezcla_estadisticas().frames); mezcla_voz(efecto, 4410, 1.0f, 1.0f, 0); prox = t + 250; }
		if (!hecha && t >= mitad && parada > 0) { hecha = 1; dormir_ms(parada); }
		double c0 = ahora();
		while (ahora() - c0 < cuadro) { }   /* un cuadro lento que gasta CPU */
		dormir_ms(1);
	}
	fin = 1;
	pthread_join(th, NULL);
	MezclaEst e = mezcla_estadisticas();
	printf("cuadros %lld bloques %ld eventos %ld perdidos %ld\n", e.frames, e.llamadas, e.eventos, e.perdidos);
	return mezcla_grabar_guardar(argv[1]) ? 0 : 1;
}
