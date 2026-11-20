/* SPDX-License-Identifier: GPL-3.0-or-later */
/* musica.h - música MIDI de Tzar: lista de canciones, introducción, historias y resultado (D417, fase 7). Interno de som.c/musica.c. */
#ifndef TZAR_MUSICA_H
#define TZAR_MUSICA_H

#include "som.h"

TzMusica *tz_musica_criar(TzSom *dono, TzRecursos *r, const char *ini_ruta);
void tz_musica_liberar(TzMusica *m);
/* Una canción del paquete de sonidos («MUSIC/INTRO.MID») o de la misión («USER/x.mid»); repetir = en bucle. */
int tz_musica_tocar(TzMusica *m, const char *nome, int repetir);
int tz_musica_tocar_dados(TzMusica *m, const uint8_t *midi, uint32_t n, int repetir);
void tz_musica_menu(TzMusica *m);                 /* INTRO.MID en bucle */
void tz_musica_jogo(TzMusica *m);                 /* la lista de misión: STARTSONG..ENDSONG barajada, con SONGPAUSE entre canciones */
void tz_musica_resultado(TzMusica *m, int vitoria);   /* WIN1/WIN2 o OUTRO */
void tz_musica_parar(TzMusica *m);
/* Lógica de la lista: tras acabar una canción espera la pausa y empieza la siguiente. `ms` = reloj en milisegundos. */
void tz_musica_tic(TzMusica *m, uint32_t ms);
/* SUMA a `mix` (estéreo) hasta `quadros` cuadros con el volumen 0..256. Lo llama la tarjeta. */
void tz_musica_gerar(TzMusica *m, int32_t *mix, int quadros, int volume);
const char *tz_musica_atual(const TzMusica *m);
int tz_musica_a_tocar(const TzMusica *m);
int tz_musica_sintetizador(void);                 /* 1 si esta compilación trae el sintetizador MIDI */
/* internos de som.c: el cerrojo del dispositivo y la lectura de recursos */
void tz_som_trancar(TzSom *s);
void tz_som_destrancar(TzSom *s);
uint8_t *tz_som_ler(TzSom *s, const char *nome, uint32_t *n);   /* recurso, o USER\ de la misión */

#endif
