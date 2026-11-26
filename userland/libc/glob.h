/* SPDX-License-Identifier: GPL-3.0-only */
/* glob.h - lo mínimo de glob() (GNU make y Valkey); en glob.c de la libc común. */
#ifndef SAGUI_GLOB_H
#define SAGUI_GLOB_H
#include <stddef.h>
#include <sys/stat.h>
#include <dirent.h>

#define GLOB_ERR       (1 << 0)
#define GLOB_MARK      (1 << 1)
#define GLOB_NOCHECK   (1 << 4)
#define GLOB_NOSORT    (1 << 5)
#define GLOB_NOESCAPE  (1 << 6)
#define GLOB_ALTDIRFUNC (1 << 9)
#define GLOB_NOSPACE   1
#define GLOB_ABORTED   2
#define GLOB_NOMATCH   3

typedef struct {
    size_t gl_pathc;
    char **gl_pathv;
    size_t gl_offs;
    int gl_flags;
    void (*gl_closedir)(void *);
    struct dirent *(*gl_readdir)(void *);
    void *(*gl_opendir)(const char *);
    int (*gl_lstat)(const char *, struct stat *);
    int (*gl_stat)(const char *, struct stat *);
} glob_t;

int glob(const char *patron, int flags, int (*errfn)(const char *, int), glob_t *g);
void globfree(glob_t *g);
#endif
