// SPDX-License-Identifier: GPL-2.0-or-later
/* acoes.h - acciones comunes del sistema: volumen, silencio, brillo, radio (D403).
 *
 * El único punto de entrada para todo lo que es «una tecla de función»: las
 * teclas multimedia del teclado (teclado.c, E0 20/2E/30; hid.c), los avisos del
 * firmware de los portátiles (ACPI Video, Fujitsu, Toshiba; y HP WMI, D405)
 * y la orden PORTATIL_ACAO. Quien llama solo dice qué acción
 * (ACAO_* de abi.h) y de dónde viene (ORIGEM_ACAO_*); esto la hace y deja el
 * aviso en pantalla (OSD) para el escritorio.
 *
 * acao_sistema() vale desde una IRQ: solo encola. La acción se hace después,
 * fuera de la IRQ, en acoes_atender(): la llama el gestor de ventanas en cada
 * vuelta y, sin escritorio, la tarea "acpi-aml". El brillo lo hace la tarea
 * "acpi-aml" (el intérprete de AML): esto solo se lo pide. */
#ifndef SAGUI_ACOES_H
#define SAGUI_ACOES_H
#include "tipos.h"
#include "abi.h"

/* Avisos en pantalla (tipo, valor), que el escritorio convierte en texto. */
enum {
    OSD_NADA = 0,
    OSD_VOLUME,        /* valor = % */
    OSD_SILENCIO,      /* valor = 1 calla, 0 vuelve a sonar */
    OSD_SEM_SOM,       /* no hay tarjeta de sonido */
    OSD_BRILHO,        /* valor = % */
    OSD_SEM_BRILHO,    /* este equipo no deja cambiar el brillo */
    OSD_RADIO,         /* valor = 1 apagada (modo avión), 0 encendida */
    OSD_TELA,          /* la tecla de cambio de pantalla */
};

void acao_sistema(int acao, int origem);       /* encola (vale desde una IRQ) */
void acoes_atender(void);                      /* hace lo encolado (fuera de una IRQ) */
void acoes_osd(int tipo, int valor);           /* publica un aviso (el último manda) */
bool acoes_osd_proximo(int *tipo, int *valor); /* el escritorio lo recoge */
bool acoes_radio_apagada(void);
uint32_t acoes_feitas(void);
uint32_t acoes_ultima(void);                   /* ACAO_* << 8 | ORIGEM_ACAO_* */

/* Avisos de texto del kernel (D405: las radios y el panel de los HP): la misma cola para
 * todos. Se pueden dejar desde la tarea de ACPI o una IRQ; el escritorio los recoge con
 * acoes_aviso_proximo() y, sin escritorio, acoes_atender() los escribe en la consola. Si
 * la cola está llena, se pierde el más viejo. */
#define ACOES_AVISO_BYTES 160
void acoes_aviso(const char *texto);
bool acoes_aviso_proximo(char *dst, uint32_t max);

/* El escritorio: muestra el aviso (avisos.c). Un aviso de estos sustituye al
 * anterior del mismo estilo en vez de hacer cola (pulsar diez veces el volumen
 * no deja diez avisos). */
void gui_aviso_osd(int tipo, int valor);

#endif
