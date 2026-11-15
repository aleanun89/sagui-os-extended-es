// SPDX-License-Identifier: GPL-2.0-or-later
/* idioma.h - el idioma de la interfaz del kernel (D313, mejora 118).
 *
 * La barra, el menú Inicio, los avisos y el diálogo de permisos escriben sus textos en español y
 * los pasan por _(): si el catálogo del idioma elegido (/sistema/idiomas/<idioma>/kernel.cat,
 * del ramdisk) tiene la traducción, sale esa; si no, sale el español. El dmesg y los registros
 * (klog) NO se traducen. El idioma lo fija el programa «configuracion» con SIS_IDIOMA (al
 * arrancar con --aplicar y cuando se elige otro en Configuración > Idioma). */
#ifndef SAGUI_IDIOMA_H
#define SAGUI_IDIOMA_H

#include "tipos.h"

const char *kernel_traducir(const char *clave);
const char *idioma_actual(void);
/* true si la fecha corta se escribe mes/día/año (inglés de EE. UU.) en vez de día/mes/año. */
bool idioma_fecha_mdy(void);

/* _("texto") traduce en el momento; N_("texto") solo marca la cadena (tablas estáticas) y se
 * traduce al usarla con _(). tools/extraer_textos.py busca las dos marcas. */
#define _(s)  kernel_traducir(s)
#define N_(s) (s)

#endif
