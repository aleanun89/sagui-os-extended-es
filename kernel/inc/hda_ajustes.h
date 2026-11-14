// SPDX-License-Identifier: GPL-2.0-or-later
/* hda_ajustes.h - ajustes («quirks») de códecs HDA por códec y subsistema (D107).
 *
 * El driver HDA (hda.c) recorre el códec solo y enciende todas las salidas
 * analógicas. Algunos portátiles necesitan además algo que el códec no
 * describe: un GPIO que enciende el amplificador de los altavoces, otra
 * configuración de pines... Este módulo elige el ajuste a partir de los
 * identificadores y no toca el hardware (se prueba en el anfitrión, h79). */
#ifndef SAGUI_HDA_AJUSTES_H
#define SAGUI_HDA_AJUSTES_H
#include "tipos.h"

#define HDA_AJUSTE_MAX_PINOS 4

typedef struct {
    const char *nome;            /* corto, para el log y infopc ("idt-hp-dv5"); NULL = ninguno */
    const char *modelo;          /* la fila de la tabla que coincidió ("HP dv6") o la familia */
    uint32_t    ssid;            /* subsistema que coincidió (0 = solo por el códec) */
    /* GPIO del grupo de audio: máscara (habilitados), dirección (1 = salida) y datos */
    uint8_t     gpio_mascara, gpio_direcao, gpio_dados;
    /* Configuraciones por defecto de pines que se sustituyen */
    uint8_t     n_pinos;
    uint8_t     pino_no[HDA_AJUSTE_MAX_PINOS];
    uint32_t    pino_config[HDA_AJUSTE_MAX_PINOS];
    /* Si A (0x0a) y F (0x0f) son salidas de línea/auriculares, los dos son auriculares */
    bool        hp_auriculares;
    uint8_t     hp_pino_a, hp_pino_f;
    /* Verbos propios del fabricante (0 = no se usan) */
    uint32_t    verbo_gpio_cmos;           /* se manda con carga 0 antes de los GPIO */
    uint32_t    verbo_mapa_energia;        /* se manda con carga 0: todos los puertos encendidos */
    uint32_t    no_volume, volume_valor;   /* control de volumen (verbo 0x70f) si el nodo lo es */
    const uint8_t *nos_desmutar;           /* amplificador de entrada 0 a 0 dB y sin silencio */
    uint8_t     n_desmutar;
} hda_ajuste_t;

/* Elige el ajuste. codec = fabricante << 16 | modelo (parámetro 0 del nodo 0);
 * ssid_pci = subsistema PCI del controlador (registro 0x2C); ssid_codec = el
 * del grupo de audio (verbo F20); n_gpio = GPIO del grupo de audio
 * (parámetro 0x11, bits 7:0). Como Linux, prueba primero el subsistema PCI
 * y después el del códec. Devuelve false (y a->nome = NULL) sin ajuste. */
bool hda_ajuste_escolher(uint32_t codec, uint32_t ssid_pci, uint32_t ssid_codec, uint32_t n_gpio, hda_ajuste_t *a);

/* ¿La configuración por defecto es una salida de línea o de auriculares conectada? */
bool hda_config_saida_fones(uint32_t config);
/* La misma configuración convertida en auriculares (dispositivo 2, asociación 1, secuencia 0xf) */
uint32_t hda_config_para_fones(uint32_t config);

#endif
