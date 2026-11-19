/* pwd.h - usuarios (D210).
 *
 * Parte de la libc común (userland/libc). El SaguiOS no tiene usuarios:
 * getpwnam y getpwuid no encuentran ninguno. En posix.c. */
#ifndef NS_PWD_H
#define NS_PWD_H

#include <sys/types.h>
struct passwd { char *pw_name, *pw_passwd, *pw_gecos, *pw_dir, *pw_shell; uid_t pw_uid; gid_t pw_gid; };
struct passwd *getpwnam(const char *nome);
struct passwd *getpwuid(uid_t uid);
int getpwuid_r(uid_t uid, struct passwd *pw, char *buf, size_t n, struct passwd **res);   /* D400: nunca encuentra */
#endif
