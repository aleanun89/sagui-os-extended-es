/* grp.h - grupos (D477). Como pwd.h: getgrnam no encuentra ninguno (posix.c). */
#ifndef SAGUI_GRP_H
#define SAGUI_GRP_H
#include <sys/types.h>
struct group {
    char  *gr_name;
    char  *gr_passwd;
    gid_t  gr_gid;
    char **gr_mem;
};
struct group *getgrnam(const char *nome);
struct group *getgrgid(gid_t gid);
#endif
