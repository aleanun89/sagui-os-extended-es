// SPDX-License-Identifier: GPL-2.0-or-later
/* ramfs.h - una carpeta del árbol en RAM montada como volumen (D379). Ver kernel/fs/ramfs.c. */
#ifndef SAGUI_RAMFS_H
#define SAGUI_RAMFS_H
#include "tipos.h"

/* Monta en "ponto" la carpeta "prefixo" del árbol en RAM (se crea si falta; "" o "/" = la raíz
 * del árbol, el initramfs). "nome" sale en «mount» (p. ej. "tmpfs", "initrd"). opcoes: MONTAR_*. */
int ramfs_montar(const char *ponto, const char *prefixo, const char *nome, uint32_t opcoes);

/* D379 (L1): un volumen de ramfs sin montarlo (la capa de la live) y soltarlo. */
void *ramfs_volume(const char *prefixo);
void  ramfs_volume_soltar(void *v);
/* v8.5.4.2: el tope de los volúmenes que se creen después, en % de la RAM (kconfig capa_ram_pct). */
void  ramfs_fixar_pct(uint32_t pct);

#endif
