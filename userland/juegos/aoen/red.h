/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* AoEN multijugador en red local (D461): sala, canal fiable sobre UDP y lockstep. Ver docs/aoen.md.
 *
 * Todas las máquinas simulan el mismo mundo: solo viajan las órdenes de los jugadores (ordenes.c). El anfitrión recoge las
 * órdenes de cada turno, las junta en un paquete ordenado por jugador y lo reparte; nadie ejecuta un turno hasta tenerlo.
 * Los jugadores de la máquina corren en todas las máquinas con la misma simulación (no viajan sus órdenes). */
#ifndef AOEN_RED_H
#define AOEN_RED_H
#include "game.h"

#define RED_PUERTO 47811           /* el del anfitrión y el de la búsqueda por difusión */
#define RED_PROTO 1
#define RED_RETRASO 2              /* turnos entre dar una orden y ejecutarla */

typedef struct { unsigned ip; unsigned short puerto; } RDir;   /* ip y puerto en orden de máquina */

enum { RED_NADA, RED_UNIENDO, RED_SALA_ANF, RED_SALA_CLI, RED_ARRANCANDO, RED_JUEGO, RED_ERROR };
enum { PL_LIBRE, PL_HUMANO, PL_IA, PL_CERRADA };

typedef struct { int tipo; char nombre[16]; int civ, color, equipo, dif, listo; } Plaza;
typedef struct { RDir dir; char nombre[16]; int jugadores, humanos, mapa, tam; unsigned visto; } RPartida;

/* ---- estado visible (la sala la edita el anfitrión; todos la ven) ---- */
extern Plaza red_plaza[MAX_PLAYERS];
extern Setup red_cfg;              /* mapa y reglas de la partida (civilización, color y equipo salen de las plazas) */
extern int red_mi_plaza;           /* en la sala: mi plaza; en la partida: mi jugador (= W.human) */
extern int red_ticks_turno;        /* tics de juego por turno (3 a 6) */
extern int red_suma_cada;          /* cada cuántos turnos se comprueba la suma de sincronía */
extern void (*red_cb_inicio)(void);   /* la interfaz: el mundo ya está hecho, entrar en la partida */

int red_estado(void);
int red_abierta(void);                         /* hay socket abierto: hay que llamar a red_tick */
const char *red_error(void);
int red_anfitrion(void);

/* ---- sala ---- */
int red_iniciar(void);                         /* abre el socket; 1 si pudo */
void red_cerrar(void);                         /* sale de la sala o de la partida y cierra todo */
int red_crear_sala(const char *mi_nombre);     /* el anfitrión: plaza 0 */
int red_unirse(RDir dir, const char *mi_nombre);
int red_dir_texto(const char *txt, RDir *d);   /* «192.168.1.5» o «192.168.1.5:47811» */
void red_buscar(void);                         /* difusión por la red local (y las IP de AOEN_RED_BUSCAR) */
int red_partidas(RPartida *out, int max);      /* las que han contestado hace poco */
void red_tick(void);                           /* hay que llamarla en cada cuadro (sala y partida) */
void red_sala_cambiada(void);                  /* el anfitrión cambió red_cfg o red_plaza: avisar a todos */
void red_elegir(int civ, int color, int equipo, int listo);   /* un cliente cambia lo suyo */
void red_chat(const char *texto);
int red_chat_n(void);                          /* mensajes recibidos en total */
const char *red_chat_texto(int k, int *de);    /* el k-ésimo contando desde 0 (solo los últimos 32) */
int red_humanos_listos(void);
int red_empezar(void);                         /* el anfitrión; 0 con el motivo en red_error() si no se puede */
int red_expulsar(int plaza);
int red_ping_ms(int plaza);                    /* ida y vuelta medida con ese jugador (anfitrión), o con el anfitrión (cliente) */

/* ---- partida ---- */
int red_avanzar(float dt);                     /* hace avanzar el juego lo que toque; devuelve los tics simulados */
int red_turno(void);
int red_pausado(void);
int red_velocidad(void);
void red_pedir_pausa(void);
void red_pedir_velocidad(int v);
unsigned red_esperando(void);                  /* jugadores (bits) a los que se espera ahora, 0 si nada */
int red_desincronizado(void);                  /* turno en que se detectó una desincronización, o -1 */
unsigned red_ultima_suma(void);
int red_jugador_humano(int p);                 /* el jugador p lo lleva una persona conectada */
const char *red_nombre(int p);
void red_salir_partida(void);                  /* abandona: la civilización pasa a la máquina */
void red_azar_semilla(unsigned s);              /* pruebas: órdenes al azar con las unidades propias, por los cmd_* normales */
void red_ordenes_azar(int porcentaje);
extern int red_turno_max;                      /* pruebas: no pasar de este turno (0: sin límite) */
extern int red_rapido;                         /* pruebas: sin esperar el reloj entre turnos (AOEN_RED_RAPIDO=1) */
#endif
