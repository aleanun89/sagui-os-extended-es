// SPDX-License-Identifier: GPL-2.0-or-later
/* sb16.h - la Sound Blaster 16 (ISA) y el ISA PnP que necesitan las Vibra/AWE PnP (D302). */
#ifndef SAGUI_SB16_H
#define SAGUI_SB16_H
#include "tipos.h"
#include "audio.h"

bool    sb16_procurar(placa_som_t *p);          /* audio_iniciar, solo si no hay tarjeta PCI con driver */
int32_t sb16_chamada(uint32_t op, uint32_t a, uint32_t b);   /* SB16_OP_* por SIS_AUDIO_HDA */

/* ISA PnP (isapnp.c): aísla las tarjetas, anota cada una y configura la primera Sound Blaster
 * de Creative (vendedor CTL) en un puerto, IRQ y DMA libres. -> true si dejó una activa.
 * Con configurar = false solo las lista (la SB16 ya contestó en su puerto: la configuró la BIOS) y
 * deja en r->id el identificador de la primera de Creative; no anota las de Creative en la lista ISA. */
typedef struct {
    uint16_t porta;
    uint8_t  irq, dma8, dma16;
    char     id[12];             /* "CTL0070" */
} isapnp_sb_t;
bool isapnp_configurar_sb(isapnp_sb_t *r, bool configurar);

#endif
