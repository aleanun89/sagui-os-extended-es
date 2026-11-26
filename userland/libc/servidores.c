/* servidores.c - lo que piden los servidores portados (Valkey, D477) y la libc no tenía: readv y writev,
 * flock, termios, ioctl, syslog y getgrnam. Casi todo son «aceptar y no hacer nada» honestos:
 * SaguiOS no tiene bloqueos de ficheros entre procesos, ni syslogd, ni modos de terminal, ni grupos. */
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <termios.h>
#include <syslog.h>
#include <grp.h>
#include <unistd.h>

/* Se escribe trozo a trozo; si uno se queda corto o falla tras haber escrito algo, se devuelve lo escrito. */
__attribute__((weak)) ssize_t writev(int fd, const struct iovec *v, int n)
{
    ssize_t total = 0;
    if (n < 0 || n > IOV_MAX) { errno = EINVAL; return -1; }
    for (int i = 0; i < n; i++) {
        if (v[i].iov_len == 0) continue;
        ssize_t r = write(fd, v[i].iov_base, v[i].iov_len);
        if (r < 0) return total ? total : -1;
        total += r;
        if ((size_t)r < v[i].iov_len) break;
    }
    return total;
}

__attribute__((weak)) ssize_t readv(int fd, const struct iovec *v, int n)
{
    ssize_t total = 0;
    if (n < 0 || n > IOV_MAX) { errno = EINVAL; return -1; }
    for (int i = 0; i < n; i++) {
        if (v[i].iov_len == 0) continue;
        ssize_t r = read(fd, v[i].iov_base, v[i].iov_len);
        if (r < 0) return total ? total : -1;
        total += r;
        if ((size_t)r < v[i].iov_len) break;   /* con lo que hubiera (socket, tubería) no se espera más */
    }
    return total;
}

__attribute__((weak)) int flock(int fd, int op) { (void)fd; (void)op; return 0; }

__attribute__((weak)) int tcgetattr(int fd, struct termios *t) { (void)fd; (void)t; errno = ENOTTY; return -1; }
__attribute__((weak)) int tcsetattr(int fd, int cuando, const struct termios *t) { (void)fd; (void)cuando; (void)t; return 0; }

/* Sin ioctl en el sistema: FIONBIO y FIONREAD los hacen fcntl y disponivel; el resto, ENOTTY. */
__attribute__((weak)) int ioctl(int fd, unsigned long pedido, ...)
{
    (void)fd; (void)pedido;
    errno = ENOTTY;
    return -1;
}

__attribute__((weak)) void openlog(const char *ident, int opciones, int facilidad) { (void)ident; (void)opciones; (void)facilidad; }
__attribute__((weak)) void syslog(int prioridad, const char *fmt, ...) { (void)prioridad; (void)fmt; }
__attribute__((weak)) void closelog(void) {}

__attribute__((weak)) struct group *getgrnam(const char *nome) { (void)nome; return NULL; }
__attribute__((weak)) struct group *getgrgid(gid_t gid) { (void)gid; return NULL; }

#include <sys/resource.h>
/* Límite de descriptores: el del kernel (processo.h: MAX_ARQUIVOS). Lo demás, sin límite. */
#define NOFILE_KERNEL 128
__attribute__((weak)) int getrlimit(int recurso, struct rlimit *l)
{
    if (!l) { errno = EFAULT; return -1; }
    l->rlim_cur = l->rlim_max = recurso == RLIMIT_NOFILE ? NOFILE_KERNEL : RLIM_INFINITY;
    return 0;
}

__attribute__((weak)) int setrlimit(int recurso, const struct rlimit *l)
{
    if (!l) { errno = EFAULT; return -1; }
    if (recurso == RLIMIT_NOFILE && l->rlim_cur > NOFILE_KERNEL) { errno = EPERM; return -1; }
    return 0;
}

#include <dlfcn.h>
__attribute__((weak)) int dladdr(const void *dir, Dl_info *info) { (void)dir; (void)info; return 0; }

#include <sys/time.h>
__attribute__((weak)) int setitimer(int cual, const struct itimerval *nuevo, struct itimerval *antiguo)
{
    if (cual != ITIMER_REAL || !nuevo) { errno = EINVAL; return -1; }
    unsigned s = (unsigned)nuevo->it_value.tv_sec + (nuevo->it_value.tv_usec > 0 ? 1 : 0);
    unsigned previo = alarm(s);
    if (antiguo) { antiguo->it_interval.tv_sec = antiguo->it_interval.tv_usec = 0; antiguo->it_value.tv_sec = previo; antiguo->it_value.tv_usec = 0; }
    return 0;
}

#include <string.h>
/* strerror_r de POSIX (devuelve 0 o un error): copia el texto de strerror en el búfer del llamador. */
__attribute__((weak)) int strerror_r(int erro, char *buf, size_t n)
{
    if (!buf || n == 0) return EINVAL;
    const char *t = strerror(erro);
    size_t l = strlen(t);
    if (l >= n) { memcpy(buf, t, n - 1); buf[n - 1] = 0; return ERANGE; }
    memcpy(buf, t, l + 1);
    return 0;
}

#include <math.h>
#include <time.h>
#include <stdlib.h>
/* long double sobre double: lo que piden Valkey (INCRBYFLOAT, ZINCRBY) y otros; el printf y el scanf de la libc ya
 * convierten el long double a double. Pierde las cifras de más de la x87, que no son el uso real. */
__attribute__((weak)) long double ceill(long double x) { return ceil((double)x); }
__attribute__((weak)) long double floorl(long double x) { return floor((double)x); }
__attribute__((weak)) long double roundl(long double x) { return round((double)x); }
__attribute__((weak)) long double truncl(long double x) { return trunc((double)x); }
__attribute__((weak)) long double fabsl(long double x) { return x < 0 ? -x : x; }
__attribute__((weak)) long long llroundl(long double x) { return llround((double)x); }
__attribute__((weak)) long double strtold(const char *s, char **fin) { return strtod(s, fin); }

__attribute__((weak)) char *ctime_r(const time_t *t, char *buf)
{
    char *r = ctime(t);
    if (!r) return NULL;
    memcpy(buf, r, 26);
    return buf;
}

#include <sys/stat.h>
#include <fcntl.h>
__attribute__((weak)) int truncate(const char *caminho, off_t n)
{
    int fd = open(caminho, O_WRONLY);
    if (fd < 0) return -1;
    int r = ftruncate(fd, n);
    close(fd);
    return r;
}
/* Un solo usuario con permisos por cuenta (D255): chown acepta y no cambia nada. */
__attribute__((weak)) int chown(const char *caminho, uid_t u, gid_t g) { (void)caminho; (void)u; (void)g; return 0; }
__attribute__((weak)) int fchown(int fd, uid_t u, gid_t g) { (void)fd; (void)u; (void)g; return 0; }
__attribute__((weak)) int lchown(const char *caminho, uid_t u, gid_t g) { (void)caminho; (void)u; (void)g; return 0; }

/* Sin carga dinámica en los programas estáticos (la hace libdl en los dinámicos): débiles, para que gane la real. */
__attribute__((weak)) void *dlopen(const char *nome, int modo) { (void)nome; (void)modo; return NULL; }
__attribute__((weak)) void *dlsym(void *b, const char *s) { (void)b; (void)s; return NULL; }
__attribute__((weak)) int dlclose(void *b) { (void)b; return 0; }
__attribute__((weak)) char *dlerror(void) { return (char *)"no hay carga dinamica de modulos en este programa"; }
