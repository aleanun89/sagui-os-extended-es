// SPDX-License-Identifier: GPL-2.0-or-later
/* acel_intel.h - el ring de órdenes de las GPU de Intel, compartido por el
 * blitter 2D (acel_intel.c, D195) y el 3D de Gen3 (gpu3d_intel.c, D285).
 *
 * En Gen3 hay un solo ring (RCS) para las órdenes 2D y 3D: lo pone en marcha
 * acel_intel.c y el 3D escribe en él por estas funciones. Quien escribe cierra
 * las interrupciones mientras dura una orden (el escritorio también escribe). */
#ifndef SAGUI_ACEL_INTEL_H
#define SAGUI_ACEL_INTEL_H
#include "tipos.h"

typedef struct {
    int      geracao;
    uint32_t fb_fisico;      /* la pantalla: dirección física (apertura) ... */
    uint32_t fb_gfx;         /* ... y dirección gráfica (GTT) */
    uint32_t arena_off;      /* zona libre de la memoria del VBE: desplazamiento desde la pantalla */
    uint32_t arena_bytes;
    uint32_t ctl, head, tail;
} intel_anel_t;

bool     intel_anel_info(intel_anel_t *a);          /* false: el ring no está en marcha */
bool     intel_anel_reservar(uint32_t palavras);    /* seguidas, sin partirse en el final del ring */
void     intel_anel_emitir(uint32_t v);
#define  INTEL_ANEL_PAL_ENVIO 12                    /* palabras que añade intel_anel_enviar (reservarlas) */
void     intel_anel_enviar(bool carga_3d);          /* MI_FLUSH y aviso a la GPU */
bool     intel_anel_ocioso(void);                   /* HEAD alcanzó a TAIL */
bool     intel_anel_esperar(uint32_t ms);           /* false: tiempo agotado */
uint32_t intel_anel_reg(uint32_t r);                /* lee un registro MMIO (diagnóstico) */

/* Lo llama acel_intel.c cuando para el ring: el 3D se apaga (kernel/gui/gpu3d.c). */
void     gpu3d_anel_parado(void);

#endif
