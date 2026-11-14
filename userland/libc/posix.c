/* posix.c - las llamadas de POSIX que pide NetSurf, sobre la API del
 * SaguiOS (D184): descriptores, stat, carpetas, mmap emulado, entorno,
 * salida del proceso, getopt, señales y locale de mentira, uname y las
 * funciones de direcciones IP.
 *
 * Lo que el SaguiOS no tiene se marca "carencia" en el comentario de cada
 * función y está en docs/carencias-netsurf.md.
 */
#include "sagui.h"

#include "arpa/inet.h"
#include "assert.h"
#include "ctype.h"
#include "dirent.h"
#include "errno.h"
#include "fcntl.h"
#include "getopt.h"
#include "limits.h"
#include "locale.h"
#include "signal.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/mman.h"
#include "sys/select.h"
#include "sys/stat.h"
#include "sys/utsname.h"
#include "sys/wait.h"
#include "pwd.h"
#include "time.h"
#include "unistd.h"

int errno;

int __ns_errno_de(int e)
{
    switch (e) {
    case ERRO_INVALIDO:      return EINVAL;
    case ERRO_NAO_EXISTE:    return ENOENT;
    case ERRO_JA_EXISTE:     return EEXIST;
    case ERRO_SEM_MEMORIA:   return ENOMEM;
    case ERRO_ENDERECO:      return EFAULT;
    case ERRO_NAO_E_PASTA:   return ENOTDIR;
    case ERRO_E_PASTA:       return EISDIR;
    case ERRO_NAO_VAZIA:     return ENOTEMPTY;
    case ERRO_LIMITE:        return EMFILE;
    case ERRO_DESCRITOR:     return EBADF;
    case ERRO_PIPE_QUEBRADO: return EPIPE;
    case ERRO_ACESSO:        return EACCES;
    case ERRO_OCUPADO:       return EBUSY;
    case ERRO_SO_LEITURA:    return EROFS;
    case ERRO_TEMPO:         return ETIMEDOUT;
    case ERRO_RECUSADA:      return ECONNREFUSED;
    case ERRO_INALCANCAVEL:  return EHOSTUNREACH;
    case ERRO_CONEXAO_CAIU:  return ECONNRESET;
    default:                 return EIO;
    }
}

static int falha(int r)
{
    errno = __ns_errno_de(r);
    return -1;
}

/* ------------------------------------------------------------- salida */

#define MAX_ATEXIT 32
static void (*ao_sair[MAX_ATEXIT])(void);
static int n_ao_sair;

int atexit(void (*f)(void))
{
    if (n_ao_sair >= MAX_ATEXIT) return -1;
    ao_sair[n_ao_sair++] = f;
    return 0;
}

void exit(int codigo)
{
    while (n_ao_sair > 0) ao_sair[--n_ao_sair]();
    fflush(NULL);
    sair(codigo);
}

void _Exit(int codigo) { sair(codigo); }
void _exit(int codigo) { sair(codigo); }

void abort(void)
{
    registrar("netsurf: abort()\n");
    fflush(NULL);
    sair(134);
}

void __ns_assert_falhou(const char *expr, const char *arq, int linha)
{
    registrar("netsurf: assert falló: %s (%s:%d)\n", expr, arq, linha);
    fprintf(stderr, "assert falló: %s (%s:%d)\n", expr, arq, linha);
    abort();
}

/* ------------------------------------------------------------- descriptores */

int open(const char *caminho, int flags, ...)
{
    int acc = flags & O_ACCMODE;
    int m = 0;
    if (acc == O_RDONLY || acc == O_RDWR) m |= ABRIR_LER;
    if (acc == O_WRONLY || acc == O_RDWR) m |= ABRIR_ESCREVER;
    if (flags & O_CREAT) m |= ABRIR_CRIAR;
    if (flags & O_TRUNC) m |= ABRIR_TRUNCAR;
    if (flags & O_APPEND) m |= ABRIR_ACRESCENTAR;
    if ((flags & O_CREAT) && (flags & O_EXCL)) {
        info_arquivo_t inf;
        if (info_arquivo(caminho, &inf) == 0) { errno = EEXIST; return -1; }
    }
    int fd = abrir(caminho, m);
    return fd < 0 ? falha(fd) : fd;
}

int openat(int pasta, const char *caminho, int flags, ...)
{
    (void)pasta;
    return open(caminho, flags);
}

int creat(const char *caminho, mode_t modo)
{
    (void)modo;
    return open(caminho, O_WRONLY | O_CREAT | O_TRUNC);
}

int fcntl(int fd, int op, ...)
{
    /* Sin banderas de descriptor en el SaguiOS: se dice que todo va bien. */
    (void)fd; (void)op;
    return 0;
}

ssize_t read(int fd, void *buf, size_t n)
{
    /* stdin desde la consola: lo escrito en stdout sale antes de esperar */
    if (fd == 0) fflush(stdout);
    int r = ler(fd, buf, (unsigned)n);
    if (r == ERRO_PIPE_QUEBRADO) return 0;   /* tubería sin quien escriba: fin (POSIX) */
    return r < 0 ? falha(r) : r;
}

ssize_t write(int fd, const void *buf, size_t n)
{
    int r = escrever(fd, buf, (unsigned)n);
    return r < 0 ? falha(r) : r;
}

off_t lseek(int fd, off_t desloc, int origem)
{
    int r = posicionar(fd, (int)desloc, origem);   /* SEEK_* = POS_* */
    return r < 0 ? falha(r) : r;
}

/* pread/pwrite: el SaguiOS no las tiene; se emulan con posicionar, que
 * mueve la posición del descriptor (y se devuelve a donde estaba). */
ssize_t pread(int fd, void *buf, size_t n, off_t desloc)
{
    int antes = posicionar(fd, 0, POS_ATUAL);
    if (antes < 0) return falha(antes);
    int r = posicionar(fd, (int)desloc, POS_INICIO);
    if (r < 0) return falha(r);
    r = ler(fd, buf, (unsigned)n);
    posicionar(fd, antes, POS_INICIO);
    return r < 0 ? falha(r) : r;
}

ssize_t pwrite(int fd, const void *buf, size_t n, off_t desloc)
{
    int antes = posicionar(fd, 0, POS_ATUAL);
    if (antes < 0) return falha(antes);
    int r = posicionar(fd, (int)desloc, POS_INICIO);
    if (r < 0) return falha(r);
    r = escrever(fd, buf, (unsigned)n);
    posicionar(fd, antes, POS_INICIO);
    return r < 0 ? falha(r) : r;
}

int close(int fd)
{
    int r = fechar(fd);
    return r < 0 ? falha(r) : 0;
}

int dup(int fd)
{
    /* SIS_DUPLICAR copia a un número dado: se busca uno libre a partir de 3. */
    for (int novo = 3; novo < 64; novo++) {
        if (tipo_descritor(novo) < 0) {
            int r = duplicar(fd, novo);
            return r < 0 ? falha(r) : novo;
        }
    }
    errno = EMFILE;
    return -1;
}

int dup2(int fd, int novo)
{
    int r = duplicar(fd, novo);
    return r < 0 ? falha(r) : novo;
}

int isatty(int fd) { return tipo_descritor(fd) == TIPO_TERMINAL; }

int ftruncate(int fd, off_t n)
{
    int r = truncar(fd, (unsigned)n);
    return r < 0 ? falha(r) : 0;
}

int fsync(int fd)
{
    int r = sincronizar(fd);
    return r < 0 ? falha(r) : 0;
}

/* ------------------------------------------------------------- stat */

static int preencher_stat(const char *caminho, struct stat *st)
{
    info_arquivo_t inf;
    int r = info_arquivo(caminho, &inf);
    if (r < 0) return falha(r);
    memset(st, 0, sizeof *st);
    if (inf.tipo == TIPO_PASTA) st->st_mode = S_IFDIR | 0755;
    else if (inf.tipo == TIPO_PROGRAMA) st->st_mode = S_IFREG | 0755;
    else if (inf.tipo == TIPO_PIPE) st->st_mode = S_IFIFO | 0644;
    else if (inf.tipo == TIPO_TERMINAL) st->st_mode = S_IFCHR | 0644;
    else st->st_mode = S_IFREG | 0644;
    st->st_size = (off_t)inf.tamanho;
    st->st_nlink = 1;
    st->st_blksize = 4096;
    st->st_blocks = (blkcnt_t)((inf.tamanho + 511) / 512);
    /* El SaguiOS no da número de nodo: un resumen de la ruta lo sustituye
     * (NetSurf solo lo usa para comparar). */
    unsigned h = 5381;
    for (const char *p = caminho; *p; p++) h = h * 33 + (unsigned char)*p;
    st->st_ino = h;
    datas_arquivo_t d;
    if (datas_arquivo(caminho, &d) == 0) {
        st->st_mtime = (time_t)d.modificado;
        st->st_ctime = (time_t)(d.criado ? d.criado : d.modificado);
        st->st_atime = (time_t)(d.acessado ? d.acessado : d.modificado);
    }
    return 0;
}

int stat(const char *caminho, struct stat *st) { return preencher_stat(caminho, st); }
int lstat(const char *caminho, struct stat *st) { return preencher_stat(caminho, st); }

int fstat(int fd, struct stat *st)
{
    /* Carencia: no hay stat por descriptor. Solo se sabe el tamaño (con
     * posicionar al final) y el tipo. */
    int tipo = tipo_descritor(fd);
    if (tipo < 0) return falha(tipo);
    memset(st, 0, sizeof *st);
    if (tipo == TIPO_ARQUIVO) {
        int antes = posicionar(fd, 0, POS_ATUAL);
        int fim = posicionar(fd, 0, POS_FIM);
        if (antes >= 0) posicionar(fd, antes, POS_INICIO);
        st->st_mode = S_IFREG | 0644;
        st->st_size = fim < 0 ? 0 : fim;
    } else if (tipo == TIPO_PIPE) st->st_mode = S_IFIFO | 0644;
    else st->st_mode = S_IFCHR | 0644;
    st->st_nlink = 1;
    st->st_blksize = 4096;
    return 0;
}

int access(const char *caminho, int modo)
{
    (void)modo;   /* sin permisos por usuario: si existe, se puede */
    info_arquivo_t inf;
    int r = info_arquivo(caminho, &inf);
    return r < 0 ? falha(r) : 0;
}

int mkdir(const char *caminho, mode_t modo)
{
    (void)modo;
    int r = criar_pasta(caminho);
    return r < 0 ? falha(r) : 0;
}

int rmdir(const char *caminho)
{
    int r = remover_pasta(caminho);
    return r < 0 ? falha(r) : 0;
}

int unlink(const char *caminho)
{
    int r = apagar(caminho);
    return r < 0 ? falha(r) : 0;
}

mode_t umask(mode_t m) { (void)m; return 022; }
int chmod(const char *caminho, mode_t modo) { (void)modo; return access(caminho, F_OK); }

int symlink(const char *de, const char *para) { (void)de; (void)para; errno = ENOSYS; return -1; }
int link(const char *de, const char *para) { (void)de; (void)para; errno = ENOSYS; return -1; }
ssize_t readlink(const char *c, char *b, size_t n) { (void)c; (void)b; (void)n; errno = EINVAL; return -1; }

/* ------------------------------------------------------------- carpetas */

struct ns_pasta {
    char caminho[PATH_MAX];
    int indice;
    int fd;              /* el número que da dirfd() */
    struct dirent atual;
};

/* dirfd(): el SaguiOS no abre carpetas como descriptores. Se dan números
 * desde 1000 que solo entienden fstatat() y unlinkat() de esta libc. */
#define DIRFD_BASE 1000
#define MAX_PASTAS 16
static DIR *pastas_abertas[MAX_PASTAS];

DIR *opendir(const char *caminho)
{
    info_arquivo_t inf;
    int r = info_arquivo(caminho, &inf);
    if (r < 0) { falha(r); return NULL; }
    if (inf.tipo != TIPO_PASTA) { errno = ENOTDIR; return NULL; }
    DIR *d = malloc(sizeof *d);
    if (!d) { errno = ENOMEM; return NULL; }
    memset(d, 0, sizeof *d);
    strlcpy(d->caminho, caminho, sizeof d->caminho);
    d->fd = -1;
    for (int i = 0; i < MAX_PASTAS; i++)
        if (!pastas_abertas[i]) { pastas_abertas[i] = d; d->fd = DIRFD_BASE + i; break; }
    return d;
}

struct dirent *readdir(DIR *d)
{
    if (!d) { errno = EBADF; return NULL; }
    entrada_pasta_t e;
    if (listar(d->caminho, d->indice, &e) != 1) return NULL;
    d->indice++;
    memset(&d->atual, 0, sizeof d->atual);
    strlcpy(d->atual.d_name, e.nome, sizeof d->atual.d_name);
    d->atual.d_type = e.tipo == TIPO_PASTA ? DT_DIR : DT_REG;
    d->atual.d_ino = (ino_t)d->indice;
    return &d->atual;
}

void rewinddir(DIR *d) { if (d) d->indice = 0; }

int closedir(DIR *d)
{
    if (!d) { errno = EBADF; return -1; }
    if (d->fd >= DIRFD_BASE) pastas_abertas[d->fd - DIRFD_BASE] = NULL;
    free(d);
    return 0;
}

int dirfd(DIR *d)
{
    if (!d || d->fd < 0) { errno = ENOTSUP; return -1; }
    return d->fd;
}

/* La ruta de "nome" relativa a la carpeta "pasta" (un dirfd o AT_FDCWD). */
static int ruta_em(int pasta, const char *nome, char *saida, size_t max)
{
    if (nome[0] == '/' || pasta == AT_FDCWD) { strlcpy(saida, nome, max); return 0; }
    int i = pasta - DIRFD_BASE;
    if (i < 0 || i >= MAX_PASTAS || !pastas_abertas[i]) { errno = EBADF; return -1; }
    snprintf(saida, max, "%s/%s", pastas_abertas[i]->caminho, nome);
    return 0;
}

int fstatat(int pasta, const char *nome, struct stat *st, int flags)
{
    (void)flags;
    char c[PATH_MAX];
    if (ruta_em(pasta, nome, c, sizeof c) < 0) return -1;
    return stat(c, st);
}

int unlinkat(int pasta, const char *nome, int flags)
{
    char c[PATH_MAX];
    if (ruta_em(pasta, nome, c, sizeof c) < 0) return -1;
    return (flags & AT_REMOVEDIR) ? rmdir(c) : unlink(c);
}

int alphasort(const struct dirent **a, const struct dirent **b)
{
    return strcmp((*a)->d_name, (*b)->d_name);
}

int scandir(const char *caminho, struct dirent ***lista,
            int (*filtro)(const struct dirent *),
            int (*cmp)(const struct dirent **, const struct dirent **))
{
    DIR *d = opendir(caminho);
    if (!d) return -1;
    struct dirent **v = NULL;
    int n = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (filtro && !filtro(e)) continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 16;
            struct dirent **nv = realloc(v, (size_t)cap * sizeof *v);
            if (!nv) goto sem_memoria;
            v = nv;
        }
        v[n] = malloc(sizeof **v);
        if (!v[n]) goto sem_memoria;
        *v[n++] = *e;
    }
    closedir(d);
    if (cmp && n > 1)
        qsort(v, (size_t)n, sizeof *v, (int (*)(const void *, const void *))cmp);
    *lista = v;
    return n;
sem_memoria:
    while (n > 0) free(v[--n]);
    free(v);
    closedir(d);
    errno = ENOMEM;
    return -1;
}

char *getcwd(char *buf, size_t n)
{
    char tmp[PATH_MAX];
    int r = pasta_atual(tmp, sizeof tmp);
    if (r < 0) { falha(r); return NULL; }
    if (!buf) {
        buf = strdup(tmp);
        if (!buf) errno = ENOMEM;
        return buf;
    }
    if (strlen(tmp) + 1 > n) { errno = ERANGE; return NULL; }
    strcpy(buf, tmp);
    return buf;
}

int chdir(const char *caminho)
{
    int r = mudar_pasta(caminho);
    return r < 0 ? falha(r) : 0;
}

/* realpath: absoluta y sin "." ni ".." (el SaguiOS no tiene enlaces
 * simbólicos, así que no hay nada más que resolver); tiene que existir. */
char *realpath(const char *caminho, char *resolvido)
{
    char tmp[PATH_MAX], saida[PATH_MAX];
    if (!caminho) { errno = EINVAL; return NULL; }
    if (caminho[0] == '/') strlcpy(tmp, caminho, sizeof tmp);
    else {
        if (!getcwd(tmp, sizeof tmp)) return NULL;
        strlcat(tmp, "/", sizeof tmp);
        strlcat(tmp, caminho, sizeof tmp);
    }
    size_t n = 0;
    saida[0] = 0;
    char *resto = tmp, *parte;
    while ((parte = strsep(&resto, "/"))) {
        if (!*parte || strcmp(parte, ".") == 0) continue;
        if (strcmp(parte, "..") == 0) {
            while (n > 0 && saida[n - 1] != '/') n--;
            if (n > 0) n--;
            saida[n] = 0;
            continue;
        }
        size_t l = strlen(parte);
        if (n + l + 2 > sizeof saida) { errno = ENAMETOOLONG; return NULL; }
        saida[n++] = '/';
        memcpy(saida + n, parte, l);
        n += l;
        saida[n] = 0;
    }
    if (n == 0) { saida[0] = '/'; saida[1] = 0; }
    struct stat st;
    if (stat(saida, &st) < 0) return NULL;
    if (!resolvido) {
        resolvido = strdup(saida);
        if (!resolvido) errno = ENOMEM;
        return resolvido;
    }
    strcpy(resolvido, saida);
    return resolvido;
}

int mkstemp(char *modelo)
{
    size_t l = strlen(modelo);
    if (l < 6 || strcmp(modelo + l - 6, "XXXXXX") != 0) { errno = EINVAL; return -1; }
    for (int t = 0; t < 100; t++) {
        unsigned v = tempo_ms() * 2654435761u + (unsigned)t * 40503u;
        for (int i = 0; i < 6; i++) { modelo[l - 6 + i] = "abcdefghijklmnopqrstuvwxyz0123456789"[v % 36]; v /= 36; }
        int fd = open(modelo, O_RDWR | O_CREAT | O_EXCL);
        if (fd >= 0 || errno != EEXIST) return fd;
    }
    return -1;
}

char *mkdtemp(char *modelo)
{
    size_t l = strlen(modelo);
    if (l < 6 || strcmp(modelo + l - 6, "XXXXXX") != 0) { errno = EINVAL; return NULL; }
    for (int t = 0; t < 100; t++) {
        unsigned v = tempo_ms() * 2654435761u + (unsigned)t * 40503u;
        for (int i = 0; i < 6; i++) { modelo[l - 6 + i] = "abcdefghijklmnopqrstuvwxyz0123456789"[v % 36]; v /= 36; }
        if (mkdir(modelo, 0700) == 0) return modelo;
        if (errno != EEXIST) return NULL;
    }
    return NULL;
}

/* ------------------------------------------------------------- mmap */

/* D209: los anónimos son páginas propias (mapear_memoria) que munmap
 * devuelve al sistema, también por trozos. Los de fichero son una copia
 * privada leída al mapear (mapear_arquivo): no hay páginas compartidas con
 * la caché del disco, así que MAP_SHARED con escritura no se puede. */
void *mmap(void *dir, size_t n, int prot, int flags, int fd, off_t desloc)
{
    (void)dir;
    if (n == 0 || (desloc & 4095)) { errno = EINVAL; return MAP_FAILED; }
    if ((prot & PROT_WRITE) && (flags & MAP_SHARED) && !(flags & MAP_ANONYMOUS)) { errno = ENOTSUP; return MAP_FAILED; }
    void *p;
    if (flags & MAP_ANONYMOUS) p = mapear_memoria(n);
    else {
        int antes = posicionar(fd, 0, POS_ATUAL);
        if (antes < 0) { errno = EBADF; return MAP_FAILED; }
        p = mapear_arquivo(fd, (unsigned)desloc, n);
        posicionar(fd, antes, POS_INICIO);
    }
    if (!p) { errno = ENOMEM; return MAP_FAILED; }
    return p;
}

int munmap(void *dir, size_t n)
{
    if (desmapear_memoria(dir, n) < 0) { errno = EINVAL; return -1; }
    return 0;
}

/* ------------------------------------------------------------- entorno */

char *getenv(const char *nome) { return (char *)obter_env(nome); }

/* setenv/unsetenv: environ es de la libsagui (D037); aquí se cambia la
 * copia del proceso (los hijos que se lancen la heredan). */
int setenv(const char *nome, const char *valor, int substituir)
{
    if (!nome || !*nome || strchr(nome, '=')) { errno = EINVAL; return -1; }
    size_t ln = strlen(nome);
    int n = 0;
    if (environ) {
        for (; environ[n]; n++) {
            if (strncmp(environ[n], nome, ln) == 0 && environ[n][ln] == '=') {
                if (!substituir) return 0;
                break;
            }
        }
    }
    char *novo = malloc(ln + strlen(valor) + 2);
    if (!novo) { errno = ENOMEM; return -1; }
    strcpy(novo, nome);
    novo[ln] = '=';
    strcpy(novo + ln + 1, valor);
    if (environ && environ[n]) { environ[n] = novo; return 0; }
    char **v = malloc((size_t)(n + 2) * sizeof *v);
    if (!v) { free(novo); errno = ENOMEM; return -1; }
    for (int i = 0; i < n; i++) v[i] = environ[i];
    v[n] = novo;
    v[n + 1] = NULL;
    environ = v;
    return 0;
}

int unsetenv(const char *nome)
{
    size_t ln = strlen(nome);
    if (!environ) return 0;
    for (int i = 0; environ[i]; i++) {
        if (strncmp(environ[i], nome, ln) == 0 && environ[i][ln] == '=') {
            for (int j = i; environ[j]; j++) environ[j] = environ[j + 1];
            i--;
        }
    }
    return 0;
}

/* ------------------------------------------------------------- procesos */

/* El SaguiOS no tiene fork ni exec: un programa se lanza con executar() de la
 * libsagui (el hijo hereda los descriptores y el entorno). */
pid_t fork(void) { errno = ENOSYS; return -1; }
int execv(const char *c, char *const argv[]) { (void)c; (void)argv; errno = ENOSYS; return -1; }
int execvp(const char *c, char *const argv[]) { (void)c; (void)argv; errno = ENOSYS; return -1; }

int pipe(int fds[2])
{
    int r = criar_pipe(fds);
    return r < 0 ? falha(r) : 0;
}

/* El estado como en Linux (código << 8). Un proceso terminado por el kernel o
 * con kill da un código negativo: se informa como la señal 9 (SIGKILL). */
pid_t waitpid(pid_t pid, int *estado, int opcoes)
{
    int codigo = 0;
    if (opcoes & WNOHANG) {
        int r = recolher(&codigo);
        if (r <= 0) return r == 0 ? 0 : falha(r);
        if (estado) *estado = codigo < 0 ? 9 : (codigo & 0xff) << 8;
        return r;
    }
    if (pid <= 0) { errno = ECHILD; return -1; }   /* carencia: no hay «cualquier hijo» */
    int r = esperar(pid, &codigo);
    if (r < 0) return falha(r);
    if (estado) *estado = codigo < 0 ? 9 : (codigo & 0xff) << 8;
    return pid;
}

pid_t wait(int *estado) { (void)estado; errno = ECHILD; return -1; }

/* system: el intérprete de órdenes con -c, y se espera a que acabe. */
int system(const char *cmd)
{
    if (!cmd) return 1;
    fflush(NULL);
    char *argv[] = { "sh", "-c", (char *)cmd, NULL };
    int pid = executar("/bin/sh", argv);
    if (pid < 0) return falha(pid);
    int st;
    if (waitpid(pid, &st, 0) < 0) return -1;
    return st;
}

/* kill: el SaguiOS solo sabe terminar un proceso; la señal 0 pregunta si
 * existe (y se dice que sí). */
int kill(pid_t pid, int sinal)
{
    if (sinal == 0) return 0;
    int r = matar(pid);
    return r < 0 ? falha(r) : 0;
}

struct passwd *getpwnam(const char *nome) { (void)nome; return NULL; }
struct passwd *getpwuid(uid_t uid) { (void)uid; return NULL; }

pid_t getpid(void) { return meu_pid(); }
pid_t getppid(void) { return 1; }
int getpagesize(void) { return 4096; }
uid_t getuid(void) { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getgid(void) { return 0; }

int gethostname(char *nome, size_t n)
{
    strlcpy(nome, "saguios", n);
    return 0;
}

long sysconf(int nome)
{
    if (nome == _SC_PAGESIZE) return 4096;
    if (nome == _SC_NPROCESSORS_ONLN) return 1;
    if (nome == _SC_CLK_TCK) return 100;
    errno = EINVAL;
    return -1;
}

int uname(struct utsname *u)
{
    memset(u, 0, sizeof *u);
    strcpy(u->sysname, "SaguiOS");
    strcpy(u->nodename, "saguios");
    strcpy(u->release, "7");
    strcpy(u->version, "SaguiOS");
    strcpy(u->machine, "i686");
    return 0;
}

unsigned sleep(unsigned s) { dormir(s * 1000); return 0; }
int usleep(unsigned us) { dormir((us + 999) / 1000); return 0; }

/* Carencia: no hay select(). Solo se cumple el plazo. */
int select(int n, fd_set *l, fd_set *e, fd_set *x, struct timeval *prazo)
{
    (void)n;
    if (l) FD_ZERO(l);
    if (e) FD_ZERO(e);
    if (x) FD_ZERO(x);
    if (prazo) dormir((unsigned)(prazo->tv_sec * 1000 + prazo->tv_usec / 1000));
    return 0;
}

/* ------------------------------------------------------------- señales */

/* Carencia: el SaguiOS no tiene señales. Se recuerda el tratador; raise()
 * lo llama (es lo único que puede hacer una señal aquí). */
static sighandler_t tratadores[32];

sighandler_t signal(int sinal, sighandler_t tratador)
{
    if (sinal <= 0 || sinal >= 32) { errno = EINVAL; return SIG_ERR; }
    sighandler_t antes = tratadores[sinal];
    tratadores[sinal] = tratador;
    return antes;
}

/* sigaction: como signal() (sa_handler; con SA_SIGINFO no se guarda nada). */
int sigaction(int sinal, const struct sigaction *a, struct sigaction *antiga)
{
    if (sinal <= 0 || sinal >= 32) { errno = EINVAL; return -1; }
    if (antiga) {
        memset(antiga, 0, sizeof *antiga);
        antiga->sa_handler = tratadores[sinal];
    }
    if (a && !(a->sa_flags & SA_SIGINFO)) tratadores[sinal] = a->sa_handler;
    return 0;
}

int sigemptyset(sigset_t *s) { *s = 0; return 0; }
int sigfillset(sigset_t *s) { *s = ~0ul; return 0; }
int sigaddset(sigset_t *s, int sinal) { *s |= 1ul << (sinal & 31); return 0; }
int sigdelset(sigset_t *s, int sinal) { *s &= ~(1ul << (sinal & 31)); return 0; }
int sigismember(const sigset_t *s, int sinal) { return (int)((*s >> (sinal & 31)) & 1); }

static sigset_t mascara;
int sigprocmask(int como, const sigset_t *s, sigset_t *antigo)
{
    if (antigo) *antigo = mascara;
    if (s) {
        if (como == SIG_BLOCK) mascara |= *s;
        else if (como == SIG_UNBLOCK) mascara &= ~*s;
        else mascara = *s;
    }
    return 0;
}

int raise(int sinal)
{
    if (sinal <= 0 || sinal >= 32) { errno = EINVAL; return -1; }
    sighandler_t t = tratadores[sinal];
    if (t == SIG_IGN) return 0;
    if (t && t != SIG_DFL) { t(sinal); return 0; }
    exit(128 + sinal);
}

/* ------------------------------------------------------------- locale */

/* Carencia: no hay locales. Todo es "C" (punto decimal, sin separador de
 * miles), que es lo que NetSurf necesita para leer el CSS. */
char *setlocale(int categoria, const char *locale)
{
    (void)categoria;
    if (!locale || !*locale || strcmp(locale, "C") == 0 || strcmp(locale, "POSIX") == 0)
        return "C";
    return NULL;
}

struct lconv *localeconv(void)
{
    static struct lconv c = { ".", "", "" };
    return &c;
}

/* ------------------------------------------------------------- getopt */

char *optarg;
int optind = 1, opterr = 1, optopt;
static int dentro;   /* posición dentro de un grupo de opciones cortas ("-ab") */

int getopt(int argc, char *const argv[], const char *opcoes)
{
    optarg = NULL;
    if (!dentro) {
        if (optind >= argc || !argv[optind] || argv[optind][0] != '-' || !argv[optind][1])
            return -1;
        if (strcmp(argv[optind], "--") == 0) { optind++; return -1; }
        dentro = 1;
    }
    int c = (unsigned char)argv[optind][dentro++];
    const char *o = strchr(opcoes, c);
    bool ultimo = !argv[optind][dentro];
    if (!o || c == ':') {
        optopt = c;
        if (opterr && opcoes[0] != ':') fprintf(stderr, "%s: opción desconocida -%c\n", argv[0], c);
        if (ultimo) { optind++; dentro = 0; }
        return '?';
    }
    if (o[1] == ':') {
        if (!ultimo) optarg = (char *)&argv[optind][dentro];
        else if (optind + 1 < argc) optarg = argv[++optind];
        else {
            optopt = c;
            optind++;
            dentro = 0;
            return opcoes[0] == ':' ? ':' : '?';
        }
        optind++;
        dentro = 0;
        return c;
    }
    if (ultimo) { optind++; dentro = 0; }
    return c;
}

int getopt_long(int argc, char *const argv[], const char *opcoes,
                const struct option *longas, int *indice)
{
    if (!dentro && optind < argc && argv[optind] && strncmp(argv[optind], "--", 2) == 0 && argv[optind][2]) {
        const char *nome = argv[optind] + 2;
        const char *igual = strchr(nome, '=');
        size_t ln = igual ? (size_t)(igual - nome) : strlen(nome);
        for (int i = 0; longas && longas[i].name; i++) {
            if (strncmp(longas[i].name, nome, ln) != 0 || longas[i].name[ln]) continue;
            optind++;
            optarg = NULL;
            if (longas[i].has_arg != no_argument) {
                if (igual) optarg = (char *)igual + 1;
                else if (longas[i].has_arg == required_argument && optind < argc) optarg = argv[optind++];
                else if (longas[i].has_arg == required_argument) return '?';
            }
            if (indice) *indice = i;
            if (longas[i].flag) { *longas[i].flag = longas[i].val; return 0; }
            return longas[i].val;
        }
        optind++;
        return '?';
    }
    return getopt(argc, argv, opcoes);
}

/* ------------------------------------------------------------- IP */

uint32_t htonl(uint32_t v) { return __builtin_bswap32(v); }
uint32_t ntohl(uint32_t v) { return __builtin_bswap32(v); }
uint16_t htons(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
uint16_t ntohs(uint16_t v) { return htons(v); }

/* inet_aton como la de BSD: "a", "a.b", "a.b.c" y "a.b.c.d", en decimal,
 * octal (0...) o hexadecimal (0x...). */
int inet_aton(const char *s, struct in_addr *a)
{
    unsigned long partes[4];
    int n = 0;
    const char *p = s;
    for (;;) {
        if (!isdigit((unsigned char)*p)) return 0;
        char *fim;
        unsigned long v = strtoul(p, &fim, 0);
        if (n >= 4) return 0;
        partes[n++] = v;
        p = fim;
        if (*p == '.') { p++; continue; }
        if (*p && !isspace((unsigned char)*p)) return 0;
        break;
    }
    uint32_t ip;
    switch (n) {
    case 1: ip = (uint32_t)partes[0]; break;
    case 2: if (partes[0] > 255 || partes[1] > 0xFFFFFF) return 0;
            ip = (uint32_t)(partes[0] << 24 | partes[1]); break;
    case 3: if (partes[0] > 255 || partes[1] > 255 || partes[2] > 0xFFFF) return 0;
            ip = (uint32_t)(partes[0] << 24 | partes[1] << 16 | partes[2]); break;
    default: for (int i = 0; i < 4; i++) if (partes[i] > 255) return 0;
            ip = (uint32_t)(partes[0] << 24 | partes[1] << 16 | partes[2] << 8 | partes[3]); break;
    }
    if (a) a->s_addr = htonl(ip);
    return 1;
}

in_addr_t inet_addr(const char *s)
{
    struct in_addr a;
    return inet_aton(s, &a) ? a.s_addr : INADDR_NONE;
}

static int ipv6_texto(const char *s, unsigned char *d)
{
    uint16_t g[8];
    int n = 0, duplo = -1;
    const char *p = s;
    if (p[0] == ':' && p[1] == ':') { duplo = 0; p += 2; if (!*p) goto fim; }
    for (;;) {
        if (n >= 8) return 0;
        char *fim;
        if (!isxdigit((unsigned char)*p)) return 0;
        unsigned long v = strtoul(p, &fim, 16);
        if (v > 0xFFFF || fim - p > 4) return 0;
        if (*fim == '.') {
            /* IPv4 al final: ::ffff:1.2.3.4 */
            struct in_addr a;
            if (n > 6 || !inet_aton(p, &a)) return 0;
            uint32_t ip = ntohl(a.s_addr);
            g[n++] = (uint16_t)(ip >> 16);
            g[n++] = (uint16_t)ip;
            break;
        }
        g[n++] = (uint16_t)v;
        p = fim;
        if (!*p) break;
        if (*p != ':') return 0;
        p++;
        if (*p == ':') {
            if (duplo >= 0) return 0;
            duplo = n;
            p++;
            if (!*p) break;
        }
    }
fim:
    if (duplo < 0 && n != 8) return 0;
    {
        uint16_t r[8] = { 0 };
        if (duplo < 0) for (int i = 0; i < 8; i++) r[i] = g[i];
        else {
            for (int i = 0; i < duplo; i++) r[i] = g[i];
            for (int i = duplo; i < n; i++) r[8 - (n - i)] = g[i];
        }
        for (int i = 0; i < 8; i++) { d[2 * i] = (unsigned char)(r[i] >> 8); d[2 * i + 1] = (unsigned char)r[i]; }
    }
    return 1;
}

int inet_pton(int familia, const char *s, void *dst)
{
    if (familia == AF_INET) {
        /* Aquí solo la forma completa a.b.c.d en decimal. */
        int puntos = 0;
        for (const char *p = s; *p; p++) {
            if (*p == '.') puntos++;
            else if (!isdigit((unsigned char)*p)) return 0;
        }
        if (puntos != 3) return 0;
        return inet_aton(s, dst);
    }
    if (familia == AF_INET6) return ipv6_texto(s, dst);
    errno = EAFNOSUPPORT;
    return -1;
}

const char *inet_ntop(int familia, const void *src, char *dst, socklen_t n)
{
    if (familia == AF_INET) {
        const unsigned char *b = src;
        snprintf(dst, n, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
        return dst;
    }
    if (familia == AF_INET6) {
        const unsigned char *b = src;
        size_t k = 0;
        for (int i = 0; i < 8; i++)
            k += (size_t)snprintf(dst + k, n > k ? n - k : 0, i ? ":%x" : "%x", b[2 * i] << 8 | b[2 * i + 1]);
        return dst;
    }
    errno = EINVAL;
    return NULL;
}
