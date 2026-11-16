// SPDX-License-Identifier: GPL-3.0-only
/* compat_sagui.c - lo que openrsync pide a la libc y SaguiOS no tiene (D314).
 *
 * Es código propio de SaguiOS (no de openrsync): poll sobre pipes y ficheros,
 * arc4random, hsearch, un recorrido de carpetas tipo fts (solo FTS_PHYSICAL), las
 * funciones "...at" con la carpeta raíz del receptor y los no-op de lo que el
 * sistema de archivos no guarda (fechas, dueño, enlaces simbólicos).
 *
 * Fechas (D341): utimensat va al kernel (PARQ_DATAS) por la ruta resuelta y futimens es el
 * de la libc (PARQ_DATAS_FD). Con -t el receptor deja las fechas del emisor y la
 * comprobación rápida (tamaño y fecha) vuelve a valer. Un sistema sin fechas fijables
 * (ISO 9660, UDF) no es un error. Los modos ya estan conectados (D282).
 */
#include "sagui.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "poll.h"
#include "fts.h"
#include "search.h"
#include "grp.h"
#include "rsync_sagui.h"
#undef read    /* aquí se llaman las de verdad: rs_read/rs_write las envuelven */
#undef write

/* ------------------------------------------------------------- generales */

uint32_t arc4random(void)
{
    uint32_t v = 0;
    if (bytes_aleatorios(&v, sizeof v) != (int)sizeof v) v = tempo_ms() * 2654435761u;
    return v;
}

void *reallocarray(void *p, size_t n, size_t t)
{
    if (t && n > (size_t)-1 / t) { errno = ENOMEM; return NULL; }
    return realloc(p, n * t);
}

/* Como reallocarray, pero lo nuevo queda a cero. */
void *recallocarray(void *p, size_t viejo, size_t nuevo, size_t t)
{
    if (t && nuevo > (size_t)-1 / t) { errno = ENOMEM; return NULL; }
    void *q = realloc(p, nuevo * t);
    if (q && nuevo > viejo) memset((char *)q + viejo * t, 0, (nuevo - viejo) * t);
    return q;
}

const char *getprogname(void) { return "rsync"; }

/* No hay base de grupos: existen root (0) y sagui (1000, D255); cualquier otro gid recibe un nombre
 * inventado ("g2000") para poder mandarlo por el protocolo, que lleva nombres. */
static struct group grupo_zero = { "root", 0 };
static struct group grupo_sagui = { "sagui", 1000 };
static char nome_grupo[16];
static struct group grupo_otro = { nome_grupo, 0 };
struct group *getgrnam(const char *nome)
{
    if (strcmp(nome, "root") == 0) return &grupo_zero;
    if (strcmp(nome, "sagui") == 0) return &grupo_sagui;
    return NULL;
}
struct group *getgrgid(gid_t gid)
{
    if (gid == 0) return &grupo_zero;
    if (gid == 1000) return &grupo_sagui;
    snprintf(nome_grupo, sizeof nome_grupo, "g%u", (unsigned)gid);
    grupo_otro.gr_gid = gid;
    return &grupo_otro;
}

/* Dueño por descriptor: no se aplica aquí (rs_chownat lo hace por ruta, D336). futimens viene de
 * la libc (D341). */
int fchown(int fd, uid_t uid, gid_t gid) { (void)fd; (void)uid; (void)gid; return 0; }

/* ------------------------------------------------------------- poll */

/* ------------------------------------------------------- tuberías hacia ssh
 *
 * El cliente rsync lanza /bin/ssh con dos tuberías (ssh lee la entrada por la una y escribe
 * la salida en la otra). Las tuberías del kernel son de 512 bytes y escribir en una llena
 * duerme hasta que el otro lado lea: si rsync escribe un bloque grande mientras ssh escribe
 * otro hacia rsync, los dos duermen para siempre. Por eso:
 *   - lo que llega de ssh se vacía siempre a un búfer propio (crece cuanto haga falta) antes
 *     de escribir y mientras se espera;
 *   - hacia ssh solo se escribe lo que cabe: la tubería tiene 512 bytes y nosotros guardamos
 *     una copia del lado de lectura, así que cabe 512 - disponivel(copia). */
#define TUBERIA_TAM 512

static int cli_de = -1, cli_a = -1, cli_a_ler = -1;   /* de ssh, a ssh, y el lado de lectura de esa */
static unsigned char *cb;                              /* lo ya sacado de la tubería, sin consumir */
static size_t cb_ini, cb_n, cb_cap;
static int cli_fim;                                    /* ssh cerró su salida */

void rs_cliente_tuberias(int de_ssh, int a_ssh, int a_ssh_leitura)
{
    cli_de = de_ssh;
    cli_a = a_ssh;
    cli_a_ler = a_ssh_leitura;
}

static void drenar(void)
{
    if (cli_de < 0 || cli_fim) return;
    for (;;) {
        int d = disponivel(cli_de);
        if (d == ERRO_PIPE_QUEBRADO) { cli_fim = 1; return; }
        if (d <= 0) return;
        if (cb_ini && cb_ini + cb_n + (size_t)d > cb_cap) {   /* compactar antes de crecer */
            memmove(cb, cb + cb_ini, cb_n);
            cb_ini = 0;
        }
        if (cb_ini + cb_n + (size_t)d > cb_cap) {
            size_t nc = cb_cap ? cb_cap : 4096;
            while (nc < cb_ini + cb_n + (size_t)d) nc *= 2;
            unsigned char *q = realloc(cb, nc);
            if (!q) { cli_fim = 1; return; }
            cb = q;
            cb_cap = nc;
        }
        int n = ler(cli_de, cb + cb_ini + cb_n, (unsigned)d);
        if (n <= 0) { cli_fim = 1; return; }
        cb_n += (size_t)n;
    }
}

/* Bytes que caben ahora en la tubería hacia ssh; -1 si ssh ya no está. */
static int cabe_a_ssh(void)
{
    int d = disponivel(cli_a_ler);
    if (d < 0) return -1;
    return TUBERIA_TAM - d;
}

ssize_t rs_read(int fd, void *buf, size_t n)
{
    if (fd != cli_de || cli_de < 0) return read(fd, buf, n);
    unsigned espera = 0;
    for (;;) {
        drenar();
        if (cb_n) {
            size_t k = n < cb_n ? n : cb_n;
            memcpy(buf, cb + cb_ini, k);
            cb_ini += k;
            cb_n -= k;
            if (!cb_n) cb_ini = 0;
            return (ssize_t)k;
        }
        if (cli_fim) return 0;
        dormir(espera);
        if (espera < 4) espera++;
    }
}

ssize_t rs_write(int fd, const void *buf, size_t n)
{
    if (fd != cli_a || cli_a < 0) return write(fd, buf, n);
    const unsigned char *p = buf;
    size_t resto = n;
    unsigned espera = 0;
    while (resto) {
        drenar();
        int c = cabe_a_ssh();
        if (c < 0 || (c == 0 && cli_fim && !cb_n)) { errno = EPIPE; return -1; }   /* ssh terminó */
        if (c > 0) {
            unsigned k = resto < (size_t)c ? (unsigned)resto : (unsigned)c;
            int r = escrever(fd, p, k);
            if (r < 0) { errno = EPIPE; return -1; }
            p += r;
            resto -= (size_t)r;
            espera = 0;
        } else {
            dormir(espera);
            if (espera < 4) espera++;
        }
    }
    return (ssize_t)n;
}

/* Un pipe se puede leer si tiene bytes o si ya no hay quien escriba (la
 * lectura devuelve 0). Un fichero siempre. Escribir se da siempre por posible:
 * la escritura de un pipe lleno espera sola a que el otro lado lea (D010), y
 * el otro lado (sshd, o la propia sesión) lee sin esperar a que escriba */
static int cli_socket = -1;
void rs_cliente_socket(int fd) { cli_socket = fd; }

static int listo_leer(int fd)
{
    if (fd < 0) return 0;
    if (fd == cli_socket) return disponivel(fd) != 0;   /* con datos, o cerrado (la lectura dara 0) */
    if (fd == cli_de && cli_de >= 0) { drenar(); return cb_n > 0 || cli_fim; }
    if (tipo_descritor(fd) != TIPO_PIPE) return 1;
    return disponivel(fd) != 0;
}

static int listo_escrever(int fd)
{
    if (fd != cli_a || cli_a < 0) return 1;
    drenar();
    return cabe_a_ssh() != 0;   /* con ssh terminado (-1) también: el write fallará y se verá */
}

int poll(struct pollfd *fds, nfds_t n, int prazo_ms)
{
    unsigned t0 = tempo_ms();
    unsigned espera = 0;
    for (;;) {
        int listos = 0;
        for (nfds_t i = 0; i < n; i++) {
            fds[i].revents = 0;
            if (fds[i].fd < 0) continue;
            if ((fds[i].events & POLLIN) && listo_leer(fds[i].fd)) fds[i].revents |= POLLIN;
            if ((fds[i].events & POLLOUT) && listo_escrever(fds[i].fd)) fds[i].revents |= POLLOUT;
            if (fds[i].revents) listos++;
        }
        if (listos) return listos;
        if (prazo_ms >= 0 && tempo_ms() - t0 >= (unsigned)prazo_ms) return 0;
        /* espera creciente: reactiva con datos, sin comerse la CPU en reposo */
        dormir(espera);
        if (espera < 4) espera++;
    }
}

/* ------------------------------------------------------------- hsearch */

static ENTRY *tabla;
static size_t tabla_n, tabla_cap;

int hcreate(size_t n)
{
    (void)n;
    tabla = NULL; tabla_n = tabla_cap = 0;
    return 1;
}

void hdestroy(void) { free(tabla); tabla = NULL; tabla_n = tabla_cap = 0; }

/* Orden por clave y búsqueda binaria: sin dependencias y de sobra rápida. */
static size_t buscar_pos(const char *k, int *igual)
{
    size_t a = 0, b = tabla_n;
    while (a < b) {
        size_t m = (a + b) / 2;
        int c = strcmp(tabla[m].key, k);
        if (c == 0) { *igual = 1; return m; }
        if (c < 0) a = m + 1; else b = m;
    }
    *igual = 0;
    return a;
}

ENTRY *hsearch(ENTRY e, ACTION a)
{
    int igual;
    size_t p = buscar_pos(e.key, &igual);
    if (igual) return &tabla[p];
    if (a == FIND) return NULL;
    if (tabla_n == tabla_cap) {
        size_t nc = tabla_cap ? tabla_cap * 2 : 256;
        ENTRY *nt = realloc(tabla, nc * sizeof *nt);
        if (!nt) { errno = ENOMEM; return NULL; }
        tabla = nt; tabla_cap = nc;
    }
    memmove(&tabla[p + 1], &tabla[p], (tabla_n - p) * sizeof *tabla);
    tabla[p] = e;
    tabla_n++;
    return &tabla[p];
}

/* ------------------------------------------------------------- fts */

struct pendiente { char *ruta; struct pendiente *sig; };
struct _fts {
    char **raizes;
    int    iraiz;
    struct pendiente *pila;   /* rutas por visitar (la cima va primero) */
    FTSENT ent;
    struct stat st;
    char  *ruta_actual;
};

FTS *fts_open(char *const *raizes, int opcoes, int (*cmp)(const FTSENT **, const FTSENT **))
{
    (void)opcoes; (void)cmp;
    FTS *f = calloc(1, sizeof *f);
    if (!f) return NULL;
    int n = 0;
    while (raizes[n]) n++;
    f->raizes = calloc((size_t)n + 1, sizeof *f->raizes);
    if (!f->raizes) { free(f); return NULL; }
    for (int i = 0; i < n; i++) f->raizes[i] = strdup(raizes[i]);
    return f;
}

/* D282: los permisos y el dueño de verdad sobre un stat de la libc (que da 0755/0644 segun el tipo). */
static void sobrepor_permisos(const char *ruta, struct stat *st)
{
    info_permisos_t p;
    if (permisos_arquivo(ruta, &p) == 0) {
        st->st_mode = (st->st_mode & S_IFMT) | (p.modo & 07777);
        st->st_uid = (uid_t)p.uid;
        st->st_gid = (gid_t)p.gid;
    }
}

static void empilhar(FTS *f, char *ruta)
{
    struct pendiente *p = malloc(sizeof *p);
    if (!p) { free(ruta); return; }
    p->ruta = ruta;
    p->sig = f->pila;
    f->pila = p;
}

/* Lee la carpeta entera y apila sus entradas (en orden inverso, para que salgan
 * en el directo). Se cierra enseguida: libc solo da 16 carpetas abiertas. */
static int expandir(FTS *f, const char *ruta)
{
    DIR *d = opendir(ruta);
    if (!d) return -1;
    char **nomes = NULL;
    size_t n = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 32;
            char **nv = realloc(nomes, cap * sizeof *nv);
            if (!nv) break;
            nomes = nv;
        }
        nomes[n++] = strdup(e->d_name);
    }
    closedir(d);
    size_t lr = strlen(ruta);
    int barra = lr > 0 && ruta[lr - 1] == '/';
    for (size_t i = n; i-- > 0;) {
        size_t l = lr + strlen(nomes[i]) + 2;
        char *c = malloc(l);
        if (c) snprintf(c, l, "%s%s%s", ruta, barra ? "" : "/", nomes[i]);
        free(nomes[i]);
        if (c) empilhar(f, c);
    }
    free(nomes);
    return 0;
}

FTSENT *fts_read(FTS *f)
{
    char *ruta;
    if (f->pila) {
        struct pendiente *p = f->pila;
        f->pila = p->sig;
        ruta = p->ruta;
        free(p);
    } else if (f->raizes[f->iraiz]) {
        ruta = strdup(f->raizes[f->iraiz++]);
    } else return NULL;
    free(f->ruta_actual);
    f->ruta_actual = ruta;
    memset(&f->ent, 0, sizeof f->ent);
    f->ent.fts_path = ruta;
    f->ent.fts_pathlen = (unsigned short)strlen(ruta);
    f->ent.fts_statp = &f->st;
    if (lstat(ruta, &f->st) < 0) {
        f->ent.fts_info = FTS_NS;
        f->ent.fts_errno = errno;
        return &f->ent;
    }
    sobrepor_permisos(ruta, &f->st);
    if (S_ISDIR(f->st.st_mode)) {
        f->ent.fts_info = FTS_D;
        if (expandir(f, ruta) < 0) { f->ent.fts_info = FTS_DNR; f->ent.fts_errno = errno; }
    } else if (S_ISREG(f->st.st_mode)) f->ent.fts_info = FTS_F;
    else if (S_ISLNK(f->st.st_mode)) f->ent.fts_info = FTS_SL;
    else f->ent.fts_info = FTS_DEFAULT;
    return &f->ent;
}

int fts_close(FTS *f)
{
    if (!f) return 0;
    while (f->pila) { struct pendiente *p = f->pila; f->pila = p->sig; free(p->ruta); free(p); }
    for (int i = 0; f->raizes[i]; i++) free(f->raizes[i]);
    free(f->raizes);
    free(f->ruta_actual);
    free(f);
    return 0;
}

/* ------------------------------------------------------------- funciones "at" */

static char raiz_ruta[PATH_MAX];
static int  raiz_hay;

int rs_abrir_raiz(const char *caminho)
{
    if (strlen(caminho) >= sizeof raiz_ruta) { errno = ENAMETOOLONG; return -1; }
    strcpy(raiz_ruta, caminho);
    raiz_hay = 1;
    return RS_RAIZFD;
}

/* La ruta de "nome" respecto de la carpeta "pasta" (RS_RAIZFD o el directorio actual). */
static int ruta_en(int pasta, const char *nome, char *saida, size_t max)
{
    int n;
    if (nome[0] == '/' || pasta != RS_RAIZFD || !raiz_hay) n = snprintf(saida, max, "%s", nome);
    else if (!strcmp(nome, ".")) n = snprintf(saida, max, "%s", raiz_ruta);
    else n = snprintf(saida, max, "%s/%s", raiz_ruta, nome);
    if (n < 0 || (size_t)n >= max) { errno = ENAMETOOLONG; return -1; }
    return 0;
}

/* Modo de los ficheros nuevos (D282). openrsync pasa el modo al crear (open con O_CREAT) y la libc
 * lo ignora: aqui se aplica despues con chmod. Con -p el modo es el exacto del origen; sin -p,
 * como open(2), sin escritura para grupo ni otros (umask 022). */
int rs_copiar_modo(const char *de, const char *para)
{
    info_permisos_t p;
    if (permisos_arquivo(de, &p) < 0) return -1;
    return mudar_permisos(para, p.modo & 01777);
}

static int modos_exactos;
void rs_modos_exactos(int si) { modos_exactos = si; }

int rs_openat(int pasta, const char *caminho, int flags, ...)
{
    char c[PATH_MAX];
    if (ruta_en(pasta, caminho, c, sizeof c) < 0) return -1;
    int fd = open(c, flags);
    if (fd >= 0 && (flags & O_CREAT)) {
        va_list ap;
        va_start(ap, flags);
        mode_t modo = (mode_t)va_arg(ap, int);
        va_end(ap);
        if (flags & O_EXCL) {   /* lo acabamos de crear: el modo es nuestro (si no, es de otro y no se toca) */
            unsigned m = (unsigned)modo & 01777;
            if (!modos_exactos) m &= ~022u;
            mudar_permisos(c, m);
        }
    }
    return fd;
}

int rs_fstatat(int pasta, const char *caminho, struct stat *st, int flags)
{
    char c[PATH_MAX];
    if (ruta_en(pasta, caminho, c, sizeof c) < 0) return -1;
    int r = (flags & AT_SYMLINK_NOFOLLOW) ? lstat(c, st) : stat(c, st);
    if (r < 0) return r;
    sobrepor_permisos(c, st);
    return 0;
}

int rs_unlinkat(int pasta, const char *caminho, int flags)
{
    char c[PATH_MAX];
    if (ruta_en(pasta, caminho, c, sizeof c) < 0) return -1;
    return (flags & AT_REMOVEDIR) ? rmdir(c) : unlink(c);
}

int rs_renameat(int p1, const char *de, int p2, const char *para)
{
    char a[PATH_MAX], b[PATH_MAX];
    if (ruta_en(p1, de, a, sizeof a) < 0 || ruta_en(p2, para, b, sizeof b) < 0) return -1;
    /* rename de SaguiOS puede no sustituir un destino existente: se quita antes */
    struct stat st;
    if (stat(b, &st) == 0 && S_ISREG(st.st_mode)) unlink(b);
    return rename(a, b);
}

int rs_mkdirat(int pasta, const char *caminho, mode_t modo)
{
    char c[PATH_MAX];
    if (ruta_en(pasta, caminho, c, sizeof c) < 0) return -1;
    return mkdir(c, modo);
}

/* Sin enlaces simbólicos en el sistema de archivos de SaguiOS. */
int rs_symlinkat(const char *alvo, int pasta, const char *caminho)
{ (void)alvo; (void)pasta; (void)caminho; errno = ENOSYS; return -1; }
ssize_t rs_readlinkat(int pasta, const char *caminho, char *buf, size_t n)
{ (void)pasta; (void)caminho; (void)buf; (void)n; errno = EINVAL; return -1; }

int rs_fchmodat(int pasta, const char *caminho, mode_t modo, int flags)
{
    (void)flags;
    char c[PATH_MAX];
    if (ruta_en(pasta, caminho, c, sizeof c) < 0) return -1;
    /* D282: chmod de verdad (la libc no lo hace). Un FAT32 o un montaje sin permisos propios
     * lo rechaza: como rsync hacia un FAT, se sigue sin error (el modo es el del montaje). */
    mudar_permisos(c, (unsigned)(modo & 01777));
    return 0;
}

int rs_chownat(int pasta, const char *caminho, int uid, int gid)
{
    char c[PATH_MAX];
    if (ruta_en(pasta, caminho, c, sizeof c) < 0) return -1;
    /* Sin permiso (no es root) o en un FAT32/ISO sin dueños propios: se sigue, como rsync sin ser root. */
    mudar_dono(c, uid, gid);
    return 0;
}

int rs_utimensat(int pasta, const char *caminho, const struct timespec ts[2], int flags)
{
    (void)flags;
    char c[PATH_MAX];
    if (ruta_en(pasta, caminho, c, sizeof c) < 0) return -1;
    unsigned v[2];
    for (int i = 0; i < 2; i++) {
        if (!ts || ts[i].tv_nsec == UTIME_NOW) v[i] = PARQ_DATA_AGORA;
        else if (ts[i].tv_nsec == UTIME_OMIT) v[i] = PARQ_DATA_MANTER;
        else v[i] = ts[i].tv_sec > 0 ? (unsigned)ts[i].tv_sec : 1u;   /* antes de 1970: el primer segundo */
    }
    int r = fixar_datas(c, v[1], v[0]);   /* D341: [0] acceso, [1] modificación */
    if (r == ERRO_NAO_SUPORTADO) return 0;   /* ISO 9660, UDF: sin fechas que fijar */
    if (r < 0) { errno = r == ERRO_ACESSO ? EPERM : EIO; return -1; }
    return 0;
}

