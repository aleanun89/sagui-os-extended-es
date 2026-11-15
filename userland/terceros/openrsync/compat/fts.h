/* SPDX-License-Identifier: GPL-3.0-only
 * fts.h - recorrido de carpetas mínimo (solo lo que usa openrsync, FTS_PHYSICAL),
 * sobre opendir/lstat de la libc de SaguiOS (D314). Ver compat_sagui.c. */
#ifndef RSYNC_FTS_H
#define RSYNC_FTS_H
#include <sys/stat.h>
#define FTS_PHYSICAL 0x0010
enum { FTS_D = 1, FTS_DC, FTS_DEFAULT, FTS_DNR, FTS_DOT, FTS_DP, FTS_ERR, FTS_F, FTS_NS, FTS_NSOK, FTS_SL, FTS_SLNONE };
typedef struct _ftsent {
    unsigned short fts_info;
    char          *fts_path;
    unsigned short fts_pathlen;
    int            fts_errno;
    struct stat   *fts_statp;
} FTSENT;
typedef struct _fts FTS;
FTS    *fts_open(char *const *raizes, int opcoes, int (*cmp)(const FTSENT **, const FTSENT **));
FTSENT *fts_read(FTS *f);
int     fts_close(FTS *f);
#endif
