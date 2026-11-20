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
/* D415: diplomacia entre el humano y la máquina. ia_responde decide si el jugador de la máquina `p` acepta que `de` pase a
 * la relación `nivel` (dipl) y escribe en `msg` lo que contesta; ia_tributo anota un tributo recibido, ia_traicion que `de`
 * rompió un pacto con `p`; ia_pide decide si `p` envía a `de` un tributo de `cantidad` del recurso `kind`. */
int ia_responde(int p, int de, int nivel, char *msg, int n);
void ia_tributo(int p, int de, int kind, int cantidad);
void ia_traicion(int p, int de);
int ia_pide(int p, int de, int kind, int cantidad, char *msg, int n);
/* personalidad (0 belicosa, 1 equilibrada, 2 pacífica), según la semilla y el jugador */
int ia_personalidad(int p);
/* olvida la memoria de todos (partida nueva o cargada); también se detecta sola */
void ia_olvidar(void);

#endif
