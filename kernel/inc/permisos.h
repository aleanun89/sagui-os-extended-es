// SPDX-License-Identifier: GPL-2.0-or-later
/* permisos.h - quién puede hacer qué: usuarios y llamadas privilegiadas (D255, mejora 098, fase 1).
 *
 * Un solo módulo decide. El despachador (sistema.c) pregunta antes de cada
 * llamada; las llamadas con rutas preguntan por la ruta ya resuelta; el resto
 * del kernel solo lee la identidad del proceso (processo_t.uid/gid). */
#ifndef SAGUI_PERMISOS_H
#define SAGUI_PERMISOS_H
#include "tipos.h"
#include "processo.h"

/* Al arrancar: la identidad de los programas que lanza el kernel (escritorio,
 * Terminal, programas del menú) según las opciones de arranque. */
void permisos_iniciar(void);
uint32_t permisos_uid_inicial(void);
bool permisos_modo_rescate(void);
bool permisos_sesion_raiz(void);   /* imagen de prueba con la sesión como root */

/* ¿Es root el proceso (su líder)? */
static inline bool permisos_root(const processo_t *p) { return processo_lider((processo_t *)p)->uid == 0; }

/* La llamada "n" con sus argumentos: 0 si puede, ERRO_ACESSO si no (y lo anota). */
int permisos_chamada(uint32_t n, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e);

/* Rutas (ya resueltas y absolutas). Provisional hasta la fase 2 (permisos de
 * ficheros): unas pocas rutas fijas del sistema. */
#define PERM_LER      1
#define PERM_ESCREVER 2
int permisos_ruta(const char *caminho, int acao);

/* Para el registro: lo que se hizo como root (elevaciones) y las denegaciones. */
void permisos_anotar_negacao(const char *que, uint32_t n, uint32_t op);

/* La clase de una llamada (para la autoprueba y la documentación). */
#define PERM_SEM_CLASSE 0   /* no clasificada: solo root */
#define PERM_LIVRE      1
#define PERM_ROOT       2
#define PERM_OPERACAO   3   /* depende de la operación o de los argumentos */
int permisos_classe(uint32_t n);

#endif
