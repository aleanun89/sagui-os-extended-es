// SPDX-License-Identifier: GPL-2.0-or-later
/* wc.h - write-combining en el framebuffer (D057). */
#ifndef SAGUI_WC_H
#define SAGUI_WC_H
#include "tipos.h"

void        wc_iniciar(void);                         /* después de la paginación: comprueba la CPU y programa la PAT */
uint64_t    wc_iniciar_secundaria(void);              /* D198: cada secundario repite la PAT/MTRR del principal; devuelve su PAT */
uint32_t    wc_mapear(uint32_t fis, uint32_t bytes);  /* ventana del kernel para el framebuffer, en WC si se puede */
bool        wc_ativo(void);
const char *wc_via(void);                             /* "PAT", "MTRR" o "" */
uint32_t    wc_mbs_medido(void);                      /* D245: MB/s medidos al escribir en el framebuffer (0 = sin medir) */
const char *wc_motivo(void);                          /* por qué no está activo (vacío si lo está) */

#endif
