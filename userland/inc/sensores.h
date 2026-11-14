// SPDX-License-Identifier: GPL-2.0-or-later
/* sensores.h - el monitor de hardware visto desde los programas (D092, mejora 050).
 *
 * El kernel lee (INFO_SENSORES); aquí quedan los textos, iguales en el comando
 * "sensores", en "temp", en infopc (pestaña CPU e informe) y en el Administrador de
 * tareas. */
#ifndef SAGUI_SENSORES_H
#define SAGUI_SENSORES_H
#include "sagui.h"

bool sensores_ler(info_sensores_t *s);   /* false = kernel sin INFO_SENSORES */
/* Texto de varias líneas, "Etiqueta:   valor\n" (la etiqueta ocupa 19 columnas,
 * como en infopc). */
int  sensores_texto(const info_sensores_t *s, char *buf, int max);
/* "45 °C" o "no disponible"; la mayor temperatura (o -1 sin ninguna). */
int  sensores_temp_maior(const info_sensores_t *s);
const char *sensores_grau(char *buf, int max, int mili_c);   /* "45 °C" (entero) */
const char *sensores_mhz(char *buf, int max, unsigned khz);  /* "2394 MHz" */
/* Frecuencia que mejor representa la CPU ahora: la real (APERF/MPERF) o, sin
 * ella, la del multiplicador o P-state actual; 0 = ninguna. */
unsigned sensores_freq_atual(const info_sensores_t *s, const char **origem);

/* Driver de frecuencia (D106, mejora 061): estado (INFO_FREQUENCIA) y
 * control (SIS_FREQUENCIA, FREQ_OP_*). */
bool frequencia_ler(info_frequencia_t *f);   /* false = kernel sin el driver */
int  frequencia_controlar(int op, int arg);   /* 0 o ERRO_* */
const char *frequencia_politica(unsigned p);  /* "rendimiento", "ahorro", "a demanda" */
/* Líneas «Etiqueta:   valor» como sensores_texto (control, estados, límite y
 * «LIMITADA POR TEMPERATURA» si lo está). */
int  frequencia_texto(const info_frequencia_t *f, char *buf, int max);
/* D175: el estado de cada CPU (INFO_FREQ_NUCLEOS); devuelve cuántas (0 = sin
 * driver o kernel antiguo). */
int  frequencia_nucleos(info_freq_nucleo_t *v, int max);

#endif
