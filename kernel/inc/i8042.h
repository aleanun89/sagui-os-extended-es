// SPDX-License-Identifier: GPL-3.0-only
/* i8042.h - controlador del teclado y del touchpad. */
#ifndef SAGUI_I8042_H
#define SAGUI_I8042_H
#include "tipos.h"

typedef struct {
    int config_lida;       /* -1 = sin respuesta */
    int resposta_padrao;   /* del mouse al 0xF6 (0xFA = ok) */
    int resposta_reset;    /* al 0xFF, si hizo falta */
    int resposta_ligar;    /* al 0xF4 */
    int config_final;
    int id_mouse;          /* al 0xF2 después de 200,100,80: 3 = IntelliMouse con rueda (D048) */
} i8042_diag_t;

bool i8042_iniciar(bool com_mouse);   /* devuelve si el mouse respondió */
void i8042_esvaziar(void);
void i8042_tique(void);               /* llamado por el reloj */
const i8042_diag_t *i8042_diagnostico(void);
/* D407: órdenes al ratón (prefijo 0xD4; devuelve la respuesta, 0xFA = ACK) y lectura de un byte suyo
 * (-1 si no llega en ~0,5 s). Solo con las interrupciones desactivadas (painel.c, al arrancar). */
int  i8042_rato_cmd(uint8_t c);
int  i8042_rato_ler(void);
bool i8042_rato_reiniciar(bool painel);   /* reinicia el ratón; painel = probar el modo absoluto */

#endif
