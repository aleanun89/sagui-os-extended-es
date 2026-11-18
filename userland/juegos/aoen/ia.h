/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Inteligencia artificial de la máquina (ia.c). */
#ifndef AOEN_IA_H
#define AOEN_IA_H

/* dificultades: 0 fácil, 1 normal, 2 difícil, 3 muy difícil (la única con ventajas: ve un 20 % más lejos y recoge más (+0,22 por segundo y recolector, alrededor de un 40 %)) */
#define IA_DIFICULTADES 4

/* piensa por cada jugador de la máquina cuando le toca; game.c la llama una vez por tic */
void ia_actualizar(float dt);
/* qué está haciendo un jugador de la máquina (para la pantalla de diplomacia y la depuración) */
const char *ia_estado(int p);
/* olvida la memoria de todos (partida nueva o cargada); también se detecta sola */
void ia_olvidar(void);

#endif
