/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Órdenes de los jugadores en el multijugador (D461). En una partida en red la interfaz no ejecuta las órdenes: cada cmd_*
 * la apunta aquí (ord_cap), la red la reparte en el turno siguiente y todas las máquinas la aplican a la vez, en el mismo
 * orden (ord_aplicar_bloque). Fuera de la red (una partida sola) ord_cap no hace nada y todo va como siempre. */
#ifndef AOEN_ORDENES_H
#define AOEN_ORDENES_H
#include "game.h"

enum {
	OP_MOVE = 1, OP_TARGET, OP_BUILD, OP_TRAIN, OP_CANCELA, OP_TRADE, OP_STANCE, OP_GUARD, OP_RALLY, OP_RESEARCH, OP_AGE,
	OP_COMERCIO, OP_DIPL, OP_TRIBUTO, OP_PEDIR, OP_MOVEQ, OP_PATRULLA, OP_ELIMINAR,
	/* de control (las aplica la red): pausa (alterna), velocidad (a = 1..4), soltar al jugador a (pasa a la IA), duración del turno (a = tics) */
	OP_PAUSA = 40, OP_VELOCIDAD, OP_SOLTAR, OP_TURNO
};
#define ORD_REFS 24

typedef struct { u8 op, jug; u16 nref; int a, b, c; float x, y; Ref ref[ORD_REFS]; } OrdRed;

extern int ord_red;      /* 1: partida en red (las órdenes de la interfaz se apuntan en vez de ejecutarse) */
extern int ord_ctx;      /* > 0: se está simulando o aplicando un turno: los cmd_* se ejecutan en el acto */

/* apunta una orden de la interfaz; devuelve 1 si la capturó (red activa y fuera de la simulación), 0 si hay que ejecutarla */
int ord_cap(int op, int a, int b, int c, float x, float y, Ref r0, Ref r1);
int ord_cap_grupo(int op, const Ref *refs, int n, int a, int b, int c);
void ord_poner(const OrdRed *o);      /* una orden de control o una ya hecha */

/* cola local: vacía lo apuntado y lo escribe en buf (formato de red); devuelve los bytes (0 si nada) */
int ord_sacar(u8 *buf, int max);
int ord_pendientes(void);
/* decodifica y aplica las órdenes de un bloque que pertenece al jugador jug; devuelve cuántas */
int ord_aplicar_bloque(int jug, const u8 *buf, int n);
int ord_codificar(const OrdRed *o, u8 *buf, int max);
int ord_decodificar(const u8 *buf, int n, OrdRed *o);   /* bytes consumidos, o -1 si está mal formado */
void ord_reiniciar(void);

/* lo implementa red.c: las órdenes de control */
void red_control(int op, int jug, int a, int b);
#endif
