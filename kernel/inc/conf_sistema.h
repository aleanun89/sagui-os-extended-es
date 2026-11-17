// SPDX-License-Identifier: GPL-2.0-or-later
/* conf_sistema.h - la partición SAGUI-CONF: configuración persistente del sistema (D255). */
#ifndef SAGUI_CONF_SISTEMA_H
#define SAGUI_CONF_SISTEMA_H
#include "tipos.h"
#include "abi.h"

#define CONF_PONTO CONF_SISTEMA_PONTO   /* abi.h: /sistema/conf */

bool conf_sistema_montada(void);     /* SAGUI-CONF montada con escritura */
void conf_sistema_superpor(void);    /* copia su etc/ sobre el /etc de la RAM (al arrancar) */
/* D371: ¿la configuración de la máquina ya está en /etc (SAGUI-CONF montada y /etc/sistema.conf)? Si no, el
 * kernel la sigue leyendo de /home como antes (en vivo, o el primer arranque de una versión nueva). */
bool conf_sistema_separada(void);
/* D371: escribe /etc/"rel" en la RAM y en SAGUI-CONF (temporal + renombrar). */
int  conf_sistema_gravar(const char *rel, const void *dados, uint32_t n);
/* D371 (P14): la carpeta de root. /root (0700, de root) existe siempre en la RAM; con SAGUI-CONF, lo que se pide
 * en /root va de verdad a CONF_RAIZ (permisos_casa_traduzir), así que se guarda. Se llama al arrancar. */
#define CONF_RAIZ CONF_PONTO "/raiz"
void conf_sistema_raiz_preparar(void);
bool conf_sistema_raiz_persistente(void);
/* D371 (fase 4): /etc/migracion-home dice «hecha»: /home es de root (vfs_home_de_root). Al arrancar y tras la
 * migración (la hace «configuracion --aplicar»). */
void conf_sistema_home_reavaliar(void);
/* D371 (fase 6, P5): ¿se entra sin pedir la cuenta? En vivo (sin SAGUI-CONF), sí; con ella, salvo
 * «inicio_automatico=no» en /etc/sistema.conf (lo pone el instalador). Las imágenes de prueba (CAB_USU_OPC), sí. */
bool conf_sistema_inicio_automatico(void);
#endif
