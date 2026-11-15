/* SPDX-License-Identifier: GPL-3.0-only
 * grp.h - SaguiOS no tiene base de grupos: todo es el grupo 0 (D314). */
#ifndef RSYNC_GRP_H
#define RSYNC_GRP_H
#include <sys/types.h>
struct group { char *gr_name; gid_t gr_gid; };
struct group *getgrnam(const char *nome);
struct group *getgrgid(gid_t gid);
#endif
