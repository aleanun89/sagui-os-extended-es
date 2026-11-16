// SPDX-License-Identifier: GPL-3.0-only
/* principal_rsync.c - punto de entrada de /apps/bin/rsync (D314).
 *
 * Tres modos:
 *   - rsync --server [--sender] ...   lo lanza el sshd de SaguiOS cuando un rsync
 *                                     del PC se conecta por SSH (openrsync: protocolo 27).
 *   - rsync [opciones] ORIGEN DESTINO copia local (local_rsync.c), con delta de ficheros
 *                                     completos: solo copia lo que cambió.
 *   - [usuario@]host:ruta             cliente por ssh (cliente_rsync.c, v8.4.2): sube o baja con
 *                                     el /bin/ssh de SaguiOS (D317); -e "ssh -p 2222 -i clave".
 *   - rsync://host/modulo/ruta        cliente del demonio rsync (D336, puerto 873): listar modulos, bajar y subir.
 *
 * Reemplaza al main.c de openrsync, que usa fork, exec y socketpair (SaguiOS no tiene).
 */
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"
#include "cliente_rsync.h"
#include "local_rsync.h"

static int uso(void)
{
    fprintf(stderr,
        "uso: rsync [-avrnlptgoc] [--delete] [--dry-run] [--progress] [--stats] ORIGEN... DESTINO\n"
        "  Copia local (como rsync -a /home/ /pendrive/copia/). Con una barra final en ORIGEN\n"
        "  se copia su contenido; sin ella, la carpeta misma.\n"
        "  Por ssh, desde SaguiOS: rsync -av carpeta/ usuario@maquina:ruta/ y al reves\n"
        "  (-e \"ssh -p 2222 -i clave\" para opciones; hace falta rsync en la otra maquina).\n"
        "  Demonio rsync: rsync rsync://maquina/ (lista los modulos), rsync -av rsync://maquina/modulo/ruta/ destino/\n"
        "  y al reves (puerto 873, sin clave ni cifrado: red local).\n"
        "  Desde otro ordenador: rsync -av -e ssh carpeta/ root@IP:/destino/ (el sshd de SaguiOS\n"
        "  ejecuta 'rsync --server'; la clave publica va en /home/.ssh/authorized_keys).\n");
    return 1;
}

int principal(int argc, char **argv)
{
    struct opts opts;
    struct local_opts lo;
    int c, nao_suportada = 0;
    const char *ssh_orden = NULL;
    struct option lopts[] = {
        { "delete",          no_argument,       &opts.del, 1 },
        { "delete-before",   no_argument,       &opts.del, 1 },
        { "delete-during",   no_argument,       &opts.del, 1 },
        { "delete-after",    no_argument,       &opts.del, 1 },
        { "delete-delay",    no_argument,       &opts.del, 1 },
        { "sender",          no_argument,       &opts.sender, 1 },
        { "server",          no_argument,       &opts.server, 1 },
        { "dry-run",         no_argument,       NULL, 'n' },
        { "archive",         no_argument,       NULL, 'a' },
        { "verbose",         no_argument,       NULL, 'v' },
        { "recursive",       no_argument,       NULL, 'r' },
        { "progress",        no_argument,       NULL, 1001 },
        { "stats",           no_argument,       NULL, 1002 },
        { "numeric-ids",     no_argument,       NULL, 1003 },
        { "ignore-errors",   no_argument,       NULL, 1003 },
        { "rsync-path",      required_argument, NULL, 1004 },
        { "checksum",        no_argument,       NULL, 'c' },
        { "quiet",           no_argument,       NULL, 'q' },
        { NULL, 0, NULL, 0 }
    };

    memset(&opts, 0, sizeof opts);
    memset(&lo, 0, sizeof lo);

    while ((c = getopt_long(argc, argv, "e:aglnoprtvcqDzxhLsu", lopts, NULL)) != -1) {
        switch (c) {
        case 'e': ssh_orden = optarg; break;   /* cliente: la orden de ssh; el servidor recibe -e.iLsfxC y no lo usa */
        case 'a': opts.recursive = opts.preserve_links = opts.preserve_perms =
                  opts.preserve_times = opts.preserve_gids = opts.preserve_uids = 1; break;
        case 'g': opts.preserve_gids = 1; break;
        case 'l': opts.preserve_links = 1; break;
        case 'n': opts.dry_run = 1; break;
        case 'o': opts.preserve_uids = 1; break;
        case 'p': opts.preserve_perms = 1; break;
        case 'r': opts.recursive = 1; break;
        case 't': opts.preserve_times = 1; break;
        case 'v': opts.verbose++; break;
        case 'c': lo.sumas = 1; break;
        case 'q': lo.callar = 1; break;
        case 'D': case 'x': case 'L': case 's': case 'u': case 'h': break;   /* sin efecto en SaguiOS */
        case 'z': nao_suportada = 1; break;    /* compresión: openrsync no la implementa */
        case 1001: lo.progreso = 1; break;
        case 1002: lo.estadisticas = 1; break;
        case 1003: break;
        case 1004: opts.rsync_path = optarg; break;
        case 0: break;                         /* las opciones largas con bandera */
        default: return uso();
        }
    }
    argc -= optind;
    argv += optind;
    rs_modos_exactos(opts.preserve_perms);   /* D282: con -p (o -a) los modos nuevos son los del origen */

    if (opts.server) {
        if (nao_suportada) {
            fprintf(stderr, "rsync: la compresion (-z) no esta soportada en este rsync\n");
            return 1;
        }
        if (argc < 2) return uso();
        return rsync_server(&opts, (size_t)argc, argv) ? 0 : 1;
    }

    if (argc == 1 && rsync_es_demonio(argv[0])) return rsync_cliente_demonio(&opts, argc, argv);   /* lista los modulos */
    if (argc < 2) return uso();
    for (int i = 0; i < argc; i++) {
        if (rsync_es_demonio(argv[i])) return rsync_cliente_demonio(&opts, argc, argv);   /* D336 */
        if (rsync_es_remota(argv[i]))
            return rsync_cliente_ssh(&opts, ssh_orden, argc, argv);
    }
    lo.permisos = opts.preserve_perms;
    lo.recursivo = opts.recursive;
    lo.borrar = opts.del;
    lo.simular = opts.dry_run;
    lo.verbose = opts.verbose;
    return rsync_local(&lo, argc - 1, argv, argv[argc - 1]);
}
