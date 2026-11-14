// SPDX-License-Identifier: GPL-3.0-only
/* teclado.h - teclado PS/2 con diseños ES (por defecto) y ABNT2.
 *
 * Cada tecla se convierte en un código: de 1 a 255 es un carácter Latin-1 (con Ctrl+letra
 * que da el código de control 1-26, como Ctrl+C = 3); de 0x100 en adelante son
 * teclas sin carácter (flechas, Home, F1...). */
#ifndef SAGUI_TECLADO_H
#define SAGUI_TECLADO_H
#include "tipos.h"
#include "abi.h"

enum {
    TECLA_CIMA = 0x100, TECLA_BAIXO, TECLA_ESQUERDA, TECLA_DIREITA,
    TECLA_HOME, TECLA_END, TECLA_PGUP, TECLA_PGDN, TECLA_INSERT, TECLA_DELETE,
    TECLA_F1, TECLA_F2, TECLA_F3, TECLA_F4, TECLA_F5, TECLA_F6,
    TECLA_F7, TECLA_F8, TECLA_F9, TECLA_F10, TECLA_F11, TECLA_F12,
    TECLA_ESC, TECLA_SUPER, TECLA_IMPRPANT,   /* TECLA_SUPER: tecla Windows, E0 5B o E0 5C (D065) */
};

#define MOD_SHIFT 1
#define MOD_CTRL  2
#define MOD_ALT   4
#define MOD_ALTGR 8
#define MOD_CAPS  16
#define MOD_SUPER 32   /* tecla Windows apretada (D119) */

void     teclado_iniciar(void);
int      teclado_ler(void);      /* espera una tecla */
int      teclado_tentar(void);   /* -1 si no hay tecla en el búfer */
int      teclado_tentar_mods(uint32_t *m);   /* igual, y los modificadores de cuando se pulsó (D119) */
uint32_t teclado_modificadores(void);
int      teclado_layout(int novo);   /* TECLADO_ES o TECLADO_ABNT2; -1 solo consulta. Devuelve el actual. */
const void *teclado_canal(void);
void     teclado_seguradas(uint8_t mapa[32]);   /* un bit por código de rastreo (128+ = extendida) */
void     teclado_byte(uint8_t b);   /* un byte del teclado (set 1): del i8042 o del teclado USB (D214) */
uint8_t  teclado_leds_usb(void);    /* luces para un teclado USB: bit 0 Bloq Num, bit 1 Bloq Mayús */
/* D251: teclas del escritorio remoto (injecao.c), por el mismo búfer. tecla 0 = solo
 * modificadores; m = MOD_SHIFT | MOD_CTRL | MOD_ALT pulsados ahora en el remoto. */
void     teclado_injetar(int tecla, uint32_t m, bool pulsada);
void     teclado_injetar_soltar(void);   /* suelta todo lo remoto (modificadores y sostenidas) */
void     injecao_vigiar(void);   /* injecao.c: si el dueño del permiso murió, suelta lo que tuviera pulsado */

#endif
