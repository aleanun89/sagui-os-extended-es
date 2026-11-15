/* SPDX-License-Identifier: GPL-3.0-only
 * local_rsync.h - copia local de rsync (D314). */
#ifndef LOCAL_RSYNC_H
#define LOCAL_RSYNC_H
struct local_opts {
    int recursivo, borrar, simular, verbose, sumas, callar, progreso, estadisticas,
        permisos;   /* -p o -a: el destino toma los modos del origen (D282) */
};
int rsync_local(const struct local_opts *o, int norigens, char **origens, const char *destino);
#endif
