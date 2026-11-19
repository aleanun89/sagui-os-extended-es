// SPDX-License-Identifier: GPL-2.0-or-later
/* mandos.h - mandos y joysticks en la libsagui (D307; docs/mandos.md).
 *
 * Interfaz para cualquier programa (las SDL 1.2 y 2, DOSBox, Configuración, «mandos»):
 *   info_mando_t l[MANDOS_MAX];
 *   unsigned gen;
 *   int n = mandos_listar(l, MANDOS_MAX, &gen);   // nombre, ejes, botones, hats, tipo y vía
 *   estado_mando_t e;
 *   mando_ler(0, &e);                              // e.eixos[] -32768..32767, e.botoes, e.hats[]
 * Se lee por sondeo (en cada cuadro); "gen" cambia al conectar o desconectar un mando. El
 * índice es la posición en la lista; e.id dice de qué mando es el estado (si no coincide con
 * l[i].id, la lista cambió). El joystick del puerto de juegos se mide al leerlo.
 * Las funciones usan un búfer estático: no se llaman desde dos hilos a la vez. */
#ifndef SAGUI_MANDOS_LIB_H
#define SAGUI_MANDOS_LIB_H
#include "sagui.h"

/* >= 0 bien; < 0 un ERRO_* del sistema. */
int  mandos_listar(info_mando_t *l, int max, unsigned *geracao);
int  mando_ler(int indice, estado_mando_t *e);
int  mando_ler_crudo(int indice, estado_mando_t *e, unsigned short crudo[4]);   /* + tiempos del puerto */
int  mandos_detectar(void);                          /* 1 = hay joystick en el puerto de juegos */
int  mandos_calibracao(unsigned short cal[4][3], int *manual);
int  mandos_calibrar(const unsigned short cal[4][3]); /* NULL = vuelve a la automática */
int  mandos_inyectar(int indice, const void *informe, unsigned n);   /* solo --mandos-prueba */
/* Diagnóstico (USB HID): el último informe crudo (n bytes) y el descriptor de informe
 * (devuelve su tamaño total; copia como mucho max bytes). */
int  mando_ler_informe(int indice, estado_mando_t *e, unsigned char informe[64], unsigned *n);
int  mando_descriptor(int indice, unsigned char *d, unsigned max);
const char *mando_nome_tipo(unsigned tipo);          /* "USB HID", "Xbox 360", "puerto de juegos", "prueba" */

/* La calibración en texto (sagui.conf, clave mando_puerto): "min,centro,max/..." por eje. */
int  mandos_cal_texto(const unsigned short cal[4][3], char *s, unsigned max);
int  mandos_cal_de_texto(const char *s, unsigned short cal[4][3]);   /* 0 bien, < 0 mal formada */
int  mandos_conf_ler(unsigned short cal[4][3]);           /* la de /home/.config/sagui.conf; 0 = hay */
int  mandos_conf_gravar(const unsigned short cal[4][3]);  /* NULL = la quita; 1 = /home no persiste */

#endif
