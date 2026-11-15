// SPDX-License-Identifier: GPL-2.0-or-later
/* hid_informe.h - analizador de descriptores de informe HID y decodificación de mandos (D307).
 *
 * Solo aritmética y bits, sin tocar el hardware: el kernel (kernel/usb/hid.c y
 * kernel/dispositivos/mandos.c) le pasa el descriptor de informe que da el dispositivo
 * y los informes que llegan por el endpoint de interrupción; las pruebas compilan el mismo
 * archivo en el anfitrión (tests/h307_mandos.py, con -DSAGUI_HOST) con descriptores reales.
 *
 * Referencias: «Device Class Definition for HID 1.11» (sección 6.2.2, items) y «HID Usage
 * Tables 1.12» (página 0x01 Generic Desktop, 0x02 Simulation, 0x09 Button). El mando de
 * Xbox 360 con cable no es HID (clase 0xFF/0x5D/0x01): su informe es fijo, de 20 bytes. */
#ifndef SAGUI_HID_INFORME_H
#define SAGUI_HID_INFORME_H
#ifdef SAGUI_HOST
#include <stdint.h>
#include <stdbool.h>
#else
#include "tipos.h"
#endif

/* Usos (página << 16 | uso). */
#define HID_USO_PONTEIRO    0x00010001u
#define HID_USO_RATO        0x00010002u
#define HID_USO_JOYSTICK    0x00010004u
#define HID_USO_GAMEPAD     0x00010005u
#define HID_USO_TECLADO     0x00010006u
#define HID_USO_MULTIEIXO   0x00010008u   /* Multi-axis Controller */
#define HID_USO_X           0x00010030u
#define HID_USO_Y           0x00010031u
#define HID_USO_Z           0x00010032u
#define HID_USO_RX          0x00010033u
#define HID_USO_RY          0x00010034u
#define HID_USO_RZ          0x00010035u
#define HID_USO_SLIDER      0x00010036u
#define HID_USO_DIAL        0x00010037u
#define HID_USO_RODA        0x00010038u
#define HID_USO_HAT         0x00010039u
#define HID_PAGINA_BOTAO    0x0009u

/* Un campo de entrada (Input) del descriptor, ya separado: cada uso de un item
 * «variable» es un campo; un item «array» queda como un solo campo con su rango. */
#define HID_CAMPOS_MAX 96
typedef struct {
    uint32_t uso;          /* página << 16 | uso; en un array, el primero del rango */
    uint32_t uso_max;      /* en un array, el último del rango */
    uint32_t aplicacao;    /* uso de la colección de aplicación donde está */
    int32_t  lmin, lmax;   /* mínimo y máximo lógicos */
    uint16_t bit;          /* desplazamiento en el informe, sin contar el byte del identificador */
    uint8_t  tam;          /* bits de cada valor */
    uint8_t  id;           /* identificador del informe (0 = el dispositivo no usa identificadores) */
    uint8_t  cont;         /* en un array, cuántos valores */
    uint8_t  matriz;       /* 1 = array (cada valor es un uso pulsado) */
    uint8_t  relativo;
    uint8_t  sinal;        /* el valor lleva signo (mínimo lógico negativo) */
} hid_campo_t;

typedef struct {
    int         n;
    bool        usa_ids;   /* el dispositivo antepone el identificador a cada informe */
    hid_campo_t c[HID_CAMPOS_MAX];
} hid_desc_t;

/* Analiza un descriptor de informe. Devuelve los campos de entrada que encontró (>= 0) o
 * -1 si el descriptor está mal formado (item cortado, colecciones descuadradas). */
int hid_analisar(const uint8_t *d, int n, hid_desc_t *out);

/* Lee un valor de tam bits desde el bit dado; con signo si sinal (complemento a dos). */
int32_t hid_ler_bits(const uint8_t *r, int n, uint32_t bit, uint8_t tam, bool sinal);

/* ---- mandos ---- */
#define HID_MANDO_EIXOS   8
#define HID_MANDO_HATS    4
#define HID_MANDO_BOTOES  32

/* Direcciones del hat (como las de SDL): se combinan en las diagonales. */
#define HAT_CENTRO   0
#define HAT_CIMA     1
#define HAT_DIREITA  2
#define HAT_BAIXO    4
#define HAT_ESQUERDA 8

typedef struct {
    uint32_t uso;
    int32_t  lmin, lmax;
    uint16_t bit;
    uint8_t  tam, sinal, id;
} hid_eixo_t;

/* Lo que hace falta para leer un mando, sacado del descriptor. */
typedef struct {
    uint32_t   aplicacao;                 /* HID_USO_JOYSTICK, HID_USO_GAMEPAD o HID_USO_MULTIEIXO */
    bool       usa_ids;
    uint8_t    n_eixos, n_hats, n_botoes;
    hid_eixo_t eixo[HID_MANDO_EIXOS];     /* ordenados por uso: X, Y, Z, Rx, Ry, Rz, deslizadores... */
    hid_eixo_t hat[HID_MANDO_HATS];
    uint16_t   botao_bit[HID_MANDO_BOTOES];
    uint8_t    botao_id[HID_MANDO_BOTOES];
    /* Botones en array (raros): un campo; cada valor es el número de botón pulsado. */
    hid_eixo_t botao_matriz;
    uint8_t    botao_matriz_cont;
} hid_mando_t;

/* El estado de un mando, normalizado como en SDL: ejes de -32768 a 32767 (centro 0;
 * X a la derecha y Y hacia abajo positivos), botones bit a bit, hats con HAT_*. */
typedef struct {
    int16_t  eixo[HID_MANDO_EIXOS];
    uint32_t botoes;
    uint8_t  hat[HID_MANDO_HATS];
} hid_estado_t;

/* Busca en los campos una colección de aplicación de joystick, mando o controlador
 * multieje y saca sus ejes, hats y botones. false = no es un mando. */
bool hid_mando_extrair(const hid_desc_t *d, hid_mando_t *m);

/* Aplica un informe recibido (con el byte del identificador delante si usa_ids). Solo
 * cambia lo que viene en ese informe. false = el informe no es de este mando. */
bool hid_mando_decodificar(const hid_mando_t *m, const uint8_t *r, int n, hid_estado_t *e);

/* Valor lógico -> -32768..32767. */
int16_t hid_normalizar_eixo(int32_t v, int32_t lmin, int32_t lmax);
/* Valor de un hat -> HAT_* (8 direcciones si el rango es de 8 valores; 4 si es de 4). */
uint8_t hid_normalizar_hat(int32_t v, int32_t lmin, int32_t lmax);

/* ---- mando de Xbox 360 con cable (clase 0xFF/0x5D/0x01) ----
 * Informe de entrada de 20 bytes: 0x00 0x14, botones (16 bits), gatillos (2 bytes),
 * sticks (4 x int16, Y hacia arriba positivo). Se presenta como 6 ejes (stick izquierdo
 * X/Y, gatillo izquierdo, stick derecho X/Y, gatillo derecho; el orden de SDL en Linux),
 * 11 botones (A B X Y LB RB Back Start Guía L3 R3) y un hat (la cruceta). */
#define XBOX360_EIXOS  6
#define XBOX360_BOTOES 11
bool xbox360_decodificar(const uint8_t *r, int n, hid_estado_t *e);

#endif
