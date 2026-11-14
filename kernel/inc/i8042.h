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

#endif
