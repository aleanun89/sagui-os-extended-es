// SPDX-License-Identifier: GPL-2.0-or-later
/* conf_reglas.h - dependencias e incompatibilidades entre opciones de configuración (D367).
 *
 * Una sola tabla (conf_reglas.c) que usan «kconfig», la orden de texto «configuracion» y la ventana de Configuración,
 * para que nunca se guarde una combinación que no tiene sentido. Es C puro (sin sagui.h): se prueba en el PC con
 * tests/conf_reglas_unidad.c.
 *
 * Las opciones se llaman por su nombre. El valor es un entero:
 *   - sí/no: 1 / 0 (las del kernel; kconfig);
 *   - de varios valores (tamaños, gobernador): 0 = automático, otro = fijado;
 *   - triestado de la aceleración (a_2d, a_3d, a_sdl, a_disco): 0 automático, 1 activado, 2 desactivado.
 * «Activa» es: sí/no -> 1; varios valores -> distinto de 0 (la opción «hace algo»); triestado -> distinto de 2.
 *
 * Hay dos clases de regla «OPCIÓN depende de PADRE»:
 *   REQUIERE    la combinación «OPCIÓN activa con PADRE inactivo» es inválida: se rechaza, y al apagar el PADRE la OPCIÓN
 *               se apaga sola (con aviso).
 *   SIN_EFECTO  con el PADRE inactivo la OPCIÓN no hace nada: se enseña deshabilitada con el motivo y no se puede fijar un
 *               valor; no se toca lo que ya hubiera (se quita con «defecto»). */
#ifndef SAGUI_CONF_REGLAS_H
#define SAGUI_CONF_REGLAS_H
#include <stddef.h>

#ifndef N_
#define N_(s) (s)
#endif

enum { CR_REQUIERE = 0, CR_SIN_EFECTO = 1 };

typedef struct {
    const char *opcion;      /* la que depende */
    const char *padre;       /* de la que depende */
    unsigned char clase;     /* CR_* */
    unsigned char solo_anillo;   /* 1 = solo cuando 2D y 3D comparten el anillo de la GPU (Intel): se pasa `comparte` */
    signed char padre_igual;     /* >= 0: el padre solo cuenta como activo si vale exactamente esto (p. ej. menú == Moderno); -1 = la regla de siempre */
    const char *motivo;      /* N_(): «Requiere PAE» */
} conf_regla_t;

extern const conf_regla_t conf_reglas[];
extern const int conf_reglas_n;

/* El valor de una opción por su nombre en el estado que se está mirando; < 0 si no existe en este contexto
 * (entonces las reglas que la nombran no cuentan). */
typedef int (*conf_leer_fn)(const char *nome, void *ctx);

typedef struct { const char *nome; int valor; const char *motivo; } conf_ajuste_t;

int  conf_activo(const char *nome, int valor);
int  conf_valor_inactivo(const char *nome);   /* el valor que la deja inactiva: 0, o 2 en el triestado */
/* ¿Se puede fijar un valor «activo» en `opcion`? NULL = sí; si no, el motivo (sin traducir: N_()). */
const char *conf_bloqueo(const char *opcion, conf_leer_fn leer, void *ctx, int comparte);
/* Al poner `opcion` = `valor`, las opciones REQUIERE que quedarían inválidas y su valor corregido (recursivo).
 * Devuelve cuántas (como mucho `max`). */
int  conf_ajustes(const char *opcion, int valor, conf_leer_fn leer, void *ctx, int comparte, conf_ajuste_t *out, int max);
/* ¿Es válido el estado entero? 0 = sí; si no, el motivo de la primera regla REQUIERE rota (y `*opcion` la que lo rompe). */
const char *conf_invalido(conf_leer_fn leer, void *ctx, int comparte, const char **opcion);

#endif
