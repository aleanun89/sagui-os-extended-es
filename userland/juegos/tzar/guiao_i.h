/* SPDX-License-Identifier: GPL-3.0-or-later */
/* guiao_i.h - lo que comparten el lector de guiones, el evaluador de expresiones y el intérprete (D417, fase 7).
 * Traducido de ReTzar (GPL-3.0-or-later): script/expr.rs. Interno de guiao*.c. */
#ifndef TZAR_GUIAO_I_H
#define TZAR_GUIAO_I_H

#include "guiao.h"

/* funciones de la tabla nativa, en su orden (expr.rs FUNCTIONS) */
enum {
	TZ_FN_GOLD, TZ_FN_STONE, TZ_FN_FOOD, TZ_FN_WOOD, TZ_FN_RND, TZ_FN_STR, TZ_FN_PODE_RESGATAR, TZ_FN_SET_AI, TZ_FN_SELECT,
	TZ_FN_UNSELECT, TZ_FN_CLEAR_SEL, TZ_FN_GOD, TZ_FN_SET_TIME, TZ_FN_GET_TIME, TZ_FN_MIN, TZ_FN_MAX, TZ_FN_GET_DIP,
	TZ_FN_IS_ALLY, TZ_FN_IS_NEUTRAL, TZ_FN_IS_ENEMY, TZ_FN_SHARED_VISION, TZ_FN_SHARED_UNITS, TZ_FN_SHARED_RES,
	TZ_FN_SET_POP, TZ_FN_BUILD, TZ_FN_SET_FOOD, TZ_FN_SET_WOOD, TZ_FN_SET_STONE, TZ_FN_SET_GOLD, TZ_FN_NO_ATTACK,
	TZ_FN_COUNT, TZ_FN_IN_PROGRESS, TZ_FN_N
};
enum {   /* propiedades de un objeto («$health»...) */
	TZ_PR_HEALTH, TZ_PR_MAXHEALTH, TZ_PR_ATTACK, TZ_PR_DEFENCE, TZ_PR_LEVEL, TZ_PR_MANNA, TZ_PR_MAXMANNA, TZ_PR_KINGDOM,
	TZ_PR_RACE, TZ_PR_X, TZ_PR_Y, TZ_PR_CARGO, TZ_PR_ISSELECTED, TZ_PR_INHOLDER, TZ_PR_PROGRAM, TZ_PR_FLAGS, TZ_PR_UNDERFOG,
	TZ_PR_N
};
enum { TZ_GL_GOLD, TZ_GL_STONE, TZ_GL_FOOD, TZ_GL_WOOD, TZ_GL_DIFFICULTY, TZ_GL_CAN_DELETE, TZ_GL_CAN_DIPLOMACY };

typedef struct {
	int32_t (*chamar)(void *ctx, int fn, const int32_t a[8], int obj);
	void (*filme)(void *ctx, const char *nome);
	int32_t (*propriedade)(void *ctx, int obj, int prop);
	void (*definir_propriedade)(void *ctx, int obj, int prop, int32_t v);
	int32_t (*global)(void *ctx, int g);
	void (*definir_global)(void *ctx, int g, int32_t v);
	int (*evento)(void *ctx, const char *nome, int *valor);   /* 1 si hay un suceso con ese nombre */
	TzVariaveis *(*vars)(void *ctx);
} TzExprOps;

/* Evalúa «texto» (sentencias separadas por «;»): el valor de la última. obj = índice del objeto de contexto o -1. */
int32_t tz_expr_eval(const TzExprOps *ops, void *ctx, const char *texto, int obj);
/* Los saltos de línea de una acción de expresión pasan a «;», los de una condición a espacios. */
void tz_expr_sentencias(const char *src, char *dst, size_t n);
void tz_expr_condicao(const char *src, char *dst, size_t n);
int32_t tz_vars_get(TzVariaveis *v, const char *nome);
void tz_vars_set(TzVariaveis *v, const char *nome, int32_t valor);
/* cuántas expresiones tuvieron paréntesis sin cerrar o un operador desconocido (para las pruebas) */
extern unsigned tz_expr_avisos;

#endif
