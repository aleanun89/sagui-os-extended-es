// SPDX-License-Identifier: GPL-2.0-or-later
/* opcoes_kernel.h - las opciones del kernel que se guardan desde el sistema (D353), vistas desde los
 * programas: la orden «kconfig» y la vista «Kernel» de Configuración usan lo mismo.
 *
 * El kernel dice qué opciones hay, su tipo, su rango y sus valores (INFO_KCONF); aquí quedan los
 * nombres para personas, los textos de los valores y cómo se escriben en la orden. */
#ifndef SAGUI_OPCOES_KERNEL_H
#define SAGUI_OPCOES_KERNEL_H
#include "sagui.h"

#define KCONF_MAX 24

/* La lista: devuelve cuántas (o < 0) y, en "estado", KCONF_EST_*. */
int  kconf_ler(info_kconf_t *v, int max, unsigned *estado);
/* Guardar (solo administrador): < 0 = error (ERRO_ACESSO sin permisos, ERRO_SO_LEITURA desde CD...). */
int  kconf_fijar(int hueco, unsigned valor);
int  kconf_quitar(int hueco);           /* vuelve el de la imagen */
int  kconf_quitar_todo(void);           /* kconfig --por-defecto */

const char *kconf_rotulo(const char *nome);    /* «Paginación PAE (más de 4 GB)»; traducido */
const char *kconf_ayuda(const char *nome);     /* una línea de qué hace; traducida */
/* El texto de un valor: «sí», «no», «automático», «64 MB», «ahorro»... */
void kconf_texto_valor(const info_kconf_t *o, unsigned v, char *buf, size_t n);
/* Lee un valor escrito en la orden. 0 = bien (en *v), 1 = «defecto» o «imagen» (quitar lo guardado),
 * < 0 = no vale (fuera de rango o mal escrito). */
int  kconf_interpretar(const info_kconf_t *o, const char *txt, unsigned *v);
/* El siguiente valor del ciclo de la ventana (sí/no, o una escala de valores habituales). */
unsigned kconf_siguiente(const info_kconf_t *o, unsigned v);
/* ¿Hace falta reiniciar para que valga lo que se arrancará la próxima vez? */
bool kconf_pide_reinicio(const info_kconf_t *o);


/* D367: dependencias entre opciones (conf_reglas.h). Un cambio pedido y lo que de verdad hay que guardar. */
typedef struct { int hueco; unsigned valor; int quitar; const char *aviso; } kconf_cambio_t;
/* ¿Está deshabilitada la opción `nome` con el estado que se arrancará la próxima vez? NULL = libre; si no, el motivo (traducido). */
const char *kconf_bloqueo(const info_kconf_t *ops, int n, const char *nome);
/* Resuelve los cambios pedidos (n) contra las reglas: los aplica sobre el estado del próximo arranque, añade los ajustes
 * en cascada (p. ej. nx=no si se apaga pae; `aviso` = el motivo, traducido) y comprueba el resultado. Devuelve cuántos
 * cambios hay que guardar (los pedidos primero) o < 0 con `*error` (traducido, listo para mostrar) y nada que guardar. */
int kconf_resolver(const info_kconf_t *ops, int n_ops, const kconf_cambio_t *pedidos, int n, kconf_cambio_t *salida, int max, char *error, size_t ne);

#endif
