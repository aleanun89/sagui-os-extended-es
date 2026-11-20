/* unistd.h - llamadas POSIX básicas sobre la API del SaguiOS (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_UNISTD_H
#define NS_UNISTD_H

#include <stddef.h>
#include <sys/types.h>
/* clock_gettime(CLOCK_MONOTONIC) existe (libc/tempo.c): libnsutils lo usa
 * si ve estas dos. */
#define _POSIX_TIMERS 200809L
#define _POSIX_MONOTONIC_CLOCK 200809L
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
ssize_t read(int fd, void *buf, size_t n);
ssize_t write(int fd, const void *buf, size_t n);
ssize_t pread(int fd, void *buf, size_t n, off_t desloc);
ssize_t pwrite(int fd, const void *buf, size_t n, off_t desloc);
off_t   lseek(int fd, off_t desloc, int origem);
int     close(int fd);
int     unlink(const char *caminho);
int     rmdir(const char *caminho);
int     unlinkat(int pasta, const char *caminho, int flags);
int     access(const char *caminho, int modo);
char   *getcwd(char *buf, size_t n);
int     chdir(const char *caminho);
int     isatty(int fd);
int     dup(int fd);
int     dup2(int fd, int novo);
int     ftruncate(int fd, off_t n);
int     fsync(int fd);
unsigned sleep(unsigned s);
int     usleep(unsigned us);
pid_t   getpid(void);
pid_t   getppid(void);
/* D401: fork con copia al escribir, exec que conserva el pid y los descriptores (menos los
 * FD_CLOEXEC), alarm y pause (sinais.c). */
pid_t   fork(void);
pid_t   vfork(void);
int     execve(const char *caminho, char *const argv[], char *const envp[]);
int     execv(const char *caminho, char *const argv[]);
int     execvp(const char *arq, char *const argv[]);
int     execvpe(const char *arq, char *const argv[], char *const envp[]);
int     execl(const char *caminho, const char *arg, ...);
int     execlp(const char *arq, const char *arg, ...);
int     execle(const char *caminho, const char *arg, ...);
unsigned alarm(unsigned segundos);
int     pause(void);
pid_t   setsid(void);
pid_t   getpgrp(void);
int     setpgid(pid_t pid, pid_t grupo);
pid_t   getpgid(pid_t pid);
int     pipe(int fds[2]);
int     pipe2(int fds[2], int flags);
void    _exit(int codigo) __attribute__((noreturn));
int     getpagesize(void);
uid_t   getuid(void);
uid_t   geteuid(void);
gid_t   getgid(void);
int     gethostname(char *nome, size_t n);
long    sysconf(int nome);
int     symlink(const char *de, const char *para);
ssize_t readlink(const char *caminho, char *buf, size_t n);
int     link(const char *de, const char *para);
#define _SC_PAGESIZE 30
#define _SC_NPROCESSORS_ONLN 84
#define _SC_CLK_TCK 2
#define _SC_GETPW_R_SIZE_MAX 70   /* D400: libgit2 */
pid_t   getsid(pid_t pid);
#define _SC_PAGE_SIZE _SC_PAGESIZE
#include <getopt.h>
#endif
