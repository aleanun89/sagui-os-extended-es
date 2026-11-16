/* SPDX-License-Identifier: GPL-3.0-only
 * cliente_rsync.h - rsync de SaguiOS como cliente por ssh (D314, v8.4.2). */
#ifndef CLIENTE_RSYNC_H
#define CLIENTE_RSYNC_H
#include "extern.h"

/* ¿"a" es una ruta remota "[usuario@]maquina:ruta" (y no una local con ':' tras una barra)? */
int rsync_es_remota(const char *a);

/* Copia por ssh. "ssh_orden" es lo que dio -e ("ssh -p 2222 -i clave"; NULL = "ssh"). Los
 * origenes y el destino son los de la linea de ordenes: o los origenes son remotos (bajar) o lo
 * es el destino (subir), no los dos. Devuelve el codigo de salida del programa. */
int rsync_cliente_ssh(const struct opts *opts, const char *ssh_orden, int argc, char **argv);

/* D336: "rsync://maquina[:puerto]/modulo/ruta" y "maquina::modulo/ruta" (el demonio, puerto 873). */
int rsync_es_demonio(const char *a);
/* Con un solo argumento (sin modulo) lista los modulos; si no, sube o baja como el cliente ssh. */
int rsync_cliente_demonio(const struct opts *opts, int argc, char **argv);
#endif
