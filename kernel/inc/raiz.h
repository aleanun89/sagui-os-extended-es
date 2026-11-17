// SPDX-License-Identifier: GPL-2.0-or-later
/* raiz.h - la raíz del sistema en un disco (D379). Ver kernel/fs/raiz.c. */
#ifndef SAGUI_RAIZ_H
#define SAGUI_RAIZ_H
#include "tipos.h"

typedef enum {
    RAIZ_NA_RAM = 0,          /* lo de siempre: el ramdisk es la raíz (no hay /etc/raiz.conf) */
    RAIZ_DISCO,               /* la raíz es un volumen ext2 montado en "/" */
    RAIZ_DISCO_SO_LEITURA,    /* idem, en solo lectura (pedido o porque el driver lo bajó) */
    RAIZ_RESCATE,             /* el initramfs pedía una raíz y no se pudo montar */
    RAIZ_LIVE,                /* live: la raíz del medio de solo lectura con la capa en la RAM */
    RAIZ_LIVE_PERSISTENTE,    /* live con la capa en SAGUI-PERSIST */
    RAIZ_TORAM,               /* la raíz copiada a la RAM (ram0): el medio se puede quitar */
} raiz_estado_t;

/* Lee /etc/raiz.conf del initramfs y, si lo hay, monta la raíz. Devuelve si la montó.
 * Va después de detectar los discos y antes del fstab. */
bool raiz_montar(void);
raiz_estado_t raiz_estado(void);
/* Tras el fstab: /tmp en la RAM y el initramfs en /initrd (solo con la raíz en disco). */
void raiz_pos_fstab(void);

#endif
