// SPDX-License-Identifier: GPL-2.0-or-later
/* plataforma_posix.c - lite-monkey en Linux/WSL: libc, sockets BSD, select
 * y fsync. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "plataforma.h"
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

void *lmk_aloca(size_t n) { return malloc(n ? n : 1); }
void *lmk_realoca(void *p, size_t n) { return realloc(p, n ? n : 1); }
void  lmk_libera(void *p) { free(p); }

uint64_t plat_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u;
}

uint64_t plat_epoca_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return (uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u;
}

void plat_azar(void *buf, uint32_t n)
{
    int fd = open("/dev/urandom", O_RDONLY);
    uint32_t hecho = 0;
    if (fd >= 0) {
        while (hecho < n) {
            ssize_t r = read(fd, (char *)buf + hecho, n - hecho);
            if (r <= 0) break;
            hecho += (uint32_t)r;
        }
        close(fd);
    }
    for (; hecho < n; hecho++) ((uint8_t *)buf)[hecho] = (uint8_t)(rand() ^ (int)plat_ms());
}

void plat_dormir(uint32_t ms)
{
    struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&t, NULL);
}

void plat_escribir_log(const char *texto)
{
    fprintf(stderr, "%s\n", texto);
    fflush(stderr);
}

/* ------------------------------------------------------------ archivos */

int plat_abrir(const char *ruta, int modo)
{
    int f = (modo & PLAT_ESCRIBIR) ? ((modo & PLAT_LEER) ? O_RDWR : O_WRONLY) : O_RDONLY;
    if (modo & PLAT_CREAR) f |= O_CREAT;
    if (modo & PLAT_TRUNCAR) f |= O_TRUNC;
    return open(ruta, f | O_CLOEXEC, 0644);
}

int plat_leer_en(int fd, uint32_t off, void *buf, uint32_t n)
{
    uint32_t hecho = 0;
    while (hecho < n) {
        ssize_t r = pread(fd, (char *)buf + hecho, n - hecho, (off_t)off + hecho);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) break;
        hecho += (uint32_t)r;
    }
    return (int)hecho;
}

bool plat_escribir_en(int fd, uint32_t off, const void *buf, uint32_t n)
{
    uint32_t hecho = 0;
    while (hecho < n) {
        ssize_t r = pwrite(fd, (const char *)buf + hecho, n - hecho, (off_t)off + hecho);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return false;
        hecho += (uint32_t)r;
    }
    return true;
}

bool plat_sincronizar(int fd) { return fdatasync(fd) == 0; }
void plat_cerrar(int fd) { close(fd); }

bool plat_tamano(int fd, uint32_t *n)
{
    struct stat st;
    if (fstat(fd, &st) != 0) return false;
    *n = st.st_size > 0xFFFFFFFFll ? 0xFFFFFFFFu : (uint32_t)st.st_size;
    return true;
}

bool plat_renombrar(const char *de, const char *a)
{
    if (rename(de, a) != 0) return false;
    /* que el renombrado también llegue al disco: sincronizar la carpeta */
    char carpeta[512];
    snprintf(carpeta, sizeof carpeta, "%s", a);
    char *barra = strrchr(carpeta, '/');
    if (barra) {
        *barra = 0;
        int fd = open(carpeta[0] ? carpeta : "/", O_RDONLY | O_DIRECTORY);
        if (fd >= 0) { fsync(fd); close(fd); }
    }
    return true;
}

bool plat_borrar(const char *ruta) { return unlink(ruta) == 0; }
bool plat_crear_carpeta(const char *ruta) { return mkdir(ruta, 0755) == 0 || errno == EEXIST; }
bool plat_borrar_carpeta(const char *ruta) { return rmdir(ruta) == 0; }

bool plat_existe_carpeta(const char *ruta)
{
    struct stat st;
    return stat(ruta, &st) == 0 && S_ISDIR(st.st_mode);
}

bool plat_listar(const char *ruta, plat_listar_fn f, void *ctx)
{
    DIR *d = opendir(ruta);
    if (!d) return false;
    struct dirent *e;
    char completo[1024];
    while ((e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        snprintf(completo, sizeof completo, "%s/%s", ruta, e->d_name);
        f(ctx, e->d_name, plat_existe_carpeta(completo));
    }
    closedir(d);
    return true;
}

/* ------------------------------------------------------------ red */

static volatile sig_atomic_t parar;
static void al_parar(int s) { (void)s; parar = 1; }

bool plat_parar(void) { return parar != 0; }

int plat_escuchar(const char *direccion, int puerto)
{
    int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (s < 0) return -1;
    int uno = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &uno, sizeof uno);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)puerto);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (direccion && inet_pton(AF_INET, direccion, &a.sin_addr) != 1) { close(s); return -1; }
    if (bind(s, (struct sockaddr *)&a, sizeof a) != 0 || listen(s, 64) != 0) { close(s); return -1; }
    fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = al_parar;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
    return s;
}

int plat_aceptar(int fd, char *quien, uint32_t max)
{
    struct sockaddr_in a;
    socklen_t l = sizeof a;
    int c = accept(fd, (struct sockaddr *)&a, &l);
    if (c < 0) return -1;
    fcntl(c, F_SETFL, fcntl(c, F_GETFL) | O_NONBLOCK);
    fcntl(c, F_SETFD, FD_CLOEXEC);
    int uno = 1;
    setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &uno, sizeof uno);
    char ip[INET_ADDRSTRLEN] = "?";
    inet_ntop(AF_INET, &a.sin_addr, ip, sizeof ip);
    snprintf(quien, max, "%s:%u", ip, (unsigned)ntohs(a.sin_port));
    return c;
}

int plat_recibir(int fd, void *buf, uint32_t n)
{
    ssize_t r = recv(fd, buf, n, 0);
    if (r > 0) return (int)r;
    if (r == 0) return 0;
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return -1;
    return -2;
}

int plat_enviar(int fd, const void *buf, uint32_t n)
{
    ssize_t r = send(fd, buf, n, MSG_NOSIGNAL);
    if (r >= 0) return (int)r;
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return 0;
    return -2;
}

void plat_cerrar_socket(int fd) { close(fd); }

int plat_esperar(const int *fds, const uint8_t *quiere_escribir, uint8_t *listo, int n, int plazo_ms)
{
    fd_set lr, le;
    FD_ZERO(&lr);
    FD_ZERO(&le);
    int max = -1;
    for (int i = 0; i < n; i++) {
        listo[i] = 0;
        if (fds[i] < 0 || fds[i] >= FD_SETSIZE) continue;
        FD_SET(fds[i], &lr);
        if (quiere_escribir[i]) FD_SET(fds[i], &le);
        if (fds[i] > max) max = fds[i];
    }
    struct timeval t = { plazo_ms / 1000, (plazo_ms % 1000) * 1000 };
    int r = select(max + 1, &lr, &le, NULL, &t);
    if (r <= 0) return 0;
    for (int i = 0; i < n; i++) {
        if (fds[i] < 0 || fds[i] >= FD_SETSIZE) continue;
        if (FD_ISSET(fds[i], &lr)) listo[i] |= 1;
        if (FD_ISSET(fds[i], &le)) listo[i] |= 2;
    }
    return r;
}

const char *plat_carpeta_datos(void)
{
    static char ruta[512];
    const char *h = getenv("HOME");
    snprintf(ruta, sizeof ruta, "%s/.litemonkey", h ? h : ".");
    return ruta;
}

uint32_t plat_memoria_mb(void) { return 256; }

#ifndef LMK_SIN_MAIN
int main(int argc, char **argv) { return lmk_principal(argc, argv); }
#endif
