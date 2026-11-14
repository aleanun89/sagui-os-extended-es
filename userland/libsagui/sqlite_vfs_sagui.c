// SPDX-License-Identifier: GPL-2.0-or-later
/* sqlite_vfs_sagui.c - el VFS de SQLite sobre las llamadas de SaguiOS (D116).
 *
 * Cada archivo de SQLite es un descriptor del kernel. Lectura y escritura en una
 * posición = posicionar + leer/escribir; truncar y sincronizar son las llamadas
 * SIS_TRUNCAR y SIS_SINCRONIZAR (D116). Cerrojo entre procesos (D118): el kernel no
 * tiene creación exclusiva de archivos ni flock, pero crear una carpeta (criar_pasta) es
 * atómico y falla con ERRO_JA_EXISTE si ya existe. Así que el escritor (nivel RESERVED o
 * más) crea la carpeta "base.db-lock" y la borra al bajar a SHARED/NONE; un segundo escritor
 * recibe SQLITE_BUSY ("database is locked"). Los lectores (SHARED) no toman cerrojo: no se
 * bloquean entre sí, pero un lector que coincide con un commit puede ver páginas a medias.
 * Si un proceso muere con la base bloqueada, la carpeta queda huérfana (borrar a mano con
 * "rmdir base.db-lock"). Sin WAL ni mmap. Documentado en docs/sqlite.md.
 *
 * Se compila dentro de SQLITE_OBJ, no en la libsagui común (no tiene sentido enlazar
 * el SQLite en todos los programas). */
#include "sagui.h"
#include "sqlite3.h"

#define MAX_CAMINHO 512

typedef struct {
    sqlite3_file base;      /* tiene que ser el primero */
    int  fd;
    int  bloqueio;          /* último nivel pedido (solo para devolverlo bien) */
    char *apagar;           /* archivo temporal: nombre a borrar al cerrar (o NULL) */
    char *cerrolho;         /* carpeta-cerrojo "base.db-lock" (solo la base principal) o NULL */
    int  tem;               /* este archivo tiene creada la carpeta-cerrojo */
} arq_sagui_t;

/* ------------------------------------------------------------ métodos del archivo */

static int sa_fechar(sqlite3_file *f)
{
    arq_sagui_t *a = (arq_sagui_t *)f;
    if (a->fd >= 0) fechar(a->fd);
    a->fd = -1;
    if (a->tem) { remover_pasta(a->cerrolho); a->tem = 0; }
    if (a->cerrolho) { sqlite3_free(a->cerrolho); a->cerrolho = NULL; }
    if (a->apagar) { apagar(a->apagar); sqlite3_free(a->apagar); a->apagar = NULL; }
    return SQLITE_OK;
}

static int sa_ler(sqlite3_file *f, void *buf, int n, sqlite3_int64 desloc)
{
    arq_sagui_t *a = (arq_sagui_t *)f;
    if (posicionar(a->fd, (int)desloc, POS_INICIO) < 0) return SQLITE_IOERR_READ;
    int feito = 0;
    while (feito < n) {
        int r = ler(a->fd, (char *)buf + feito, (unsigned)(n - feito));
        if (r < 0) return SQLITE_IOERR_READ;
        if (r == 0) break;
        feito += r;
    }
    if (feito < n) {   /* lectura corta: SQLite espera el resto a cero */
        memset((char *)buf + feito, 0, (size_t)(n - feito));
        return SQLITE_IOERR_SHORT_READ;
    }
    return SQLITE_OK;
}

static int sa_escrever(sqlite3_file *f, const void *buf, int n, sqlite3_int64 desloc)
{
    arq_sagui_t *a = (arq_sagui_t *)f;
    if (posicionar(a->fd, (int)desloc, POS_INICIO) < 0) return SQLITE_IOERR_WRITE;
    int feito = 0;
    while (feito < n) {
        int r = escrever(a->fd, (const char *)buf + feito, (unsigned)(n - feito));
        if (r == ERRO_SEM_MEMORIA || r == ERRO_LIMITE) return SQLITE_FULL;
        if (r <= 0) return SQLITE_IOERR_WRITE;
        feito += r;
    }
    return SQLITE_OK;
}

static int sa_truncar(sqlite3_file *f, sqlite3_int64 tamanho)
{
    arq_sagui_t *a = (arq_sagui_t *)f;
    return truncar(a->fd, (unsigned)tamanho) < 0 ? SQLITE_IOERR_TRUNCATE : SQLITE_OK;
}

static int sa_sincronizar(sqlite3_file *f, int flags)
{
    (void)flags;
    arq_sagui_t *a = (arq_sagui_t *)f;
    return sincronizar(a->fd) < 0 ? SQLITE_IOERR_FSYNC : SQLITE_OK;
}

static int sa_tamanho(sqlite3_file *f, sqlite3_int64 *tam)
{
    arq_sagui_t *a = (arq_sagui_t *)f;
    int r = posicionar(a->fd, 0, POS_FIM);   /* devuelve la nueva posición = el tamaño */
    if (r < 0) return SQLITE_IOERR_FSTAT;
    *tam = (unsigned)r;
    return SQLITE_OK;
}

/* Cerrojos: ver la cabecera. Solo el escritor (RESERVED o más) toma la carpeta-cerrojo. */
static int sa_bloquear(sqlite3_file *f, int nivel)
{
    arq_sagui_t *a = (arq_sagui_t *)f;
    if (nivel >= SQLITE_LOCK_RESERVED && a->cerrolho && !a->tem) {
        int r = criar_pasta(a->cerrolho);
        if (r == ERRO_JA_EXISTE) return SQLITE_BUSY;
        if (r < 0) return SQLITE_IOERR_LOCK;
        a->tem = 1;
    }
    a->bloqueio = nivel;
    return SQLITE_OK;
}

static int sa_desbloquear(sqlite3_file *f, int nivel)
{
    arq_sagui_t *a = (arq_sagui_t *)f;
    if (nivel < SQLITE_LOCK_RESERVED && a->tem) { remover_pasta(a->cerrolho); a->tem = 0; }
    a->bloqueio = nivel;
    return SQLITE_OK;
}

/* Hay un escritor (de otro archivo, u otro proceso) con RESERVED o más. */
static int sa_reservado(sqlite3_file *f, int *saida)
{
    arq_sagui_t *a = (arq_sagui_t *)f;
    info_arquivo_t inf;
    *saida = a->tem ? 1 : (a->cerrolho && info_arquivo(a->cerrolho, &inf) == 0);
    return SQLITE_OK;
}
static int sa_controle(sqlite3_file *f, int op, void *arg) { (void)f; (void)op; (void)arg; return SQLITE_NOTFOUND; }
static int sa_setor(sqlite3_file *f) { (void)f; return 512; }
static int sa_caracteristicas(sqlite3_file *f) { (void)f; return 0; }

static const sqlite3_io_methods metodos = {
    1, sa_fechar, sa_ler, sa_escrever, sa_truncar, sa_sincronizar, sa_tamanho,
    sa_bloquear, sa_desbloquear, sa_reservado, sa_controle, sa_setor, sa_caracteristicas,
    0, 0, 0, 0,
};

/* ------------------------------------------------------------ métodos del VFS */

static int sv_abrir(sqlite3_vfs *vfs, sqlite3_filename nome, sqlite3_file *f, int flags, int *flags_saida)
{
    (void)vfs;
    arq_sagui_t *a = (arq_sagui_t *)f;
    a->base.pMethods = NULL;
    a->fd = -1;
    a->bloqueio = 0;
    a->apagar = NULL;
    a->cerrolho = NULL;
    a->tem = 0;
    char temporal[64];
    const char *caminho = nome;
    if (!caminho) {   /* archivo temporal anónimo: nombre al azar en /tmp, se borra al cerrar */
        unsigned char r[6];
        bytes_aleatorios(r, sizeof r);
        formatar_texto(temporal, sizeof temporal, "/tmp/sqlite_%02x%02x%02x%02x%02x%02x", r[0], r[1], r[2], r[3], r[4], r[5]);
        caminho = temporal;
        flags |= SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_DELETEONCLOSE;
    }
    int modo = 0;
    if (flags & SQLITE_OPEN_READWRITE) modo |= ABRIR_LER | ABRIR_ESCREVER;
    else modo |= ABRIR_LER;
    info_arquivo_t inf;
    bool existe = info_arquivo(caminho, &inf) == 0;
    if ((flags & SQLITE_OPEN_EXCLUSIVE) && (flags & SQLITE_OPEN_CREATE) && existe) return SQLITE_CANTOPEN;
    if (!existe && !(flags & SQLITE_OPEN_CREATE)) return SQLITE_CANTOPEN;
    if (existe && inf.tipo == TIPO_PASTA) return SQLITE_CANTOPEN;
    if (!existe) modo |= ABRIR_CRIAR | ABRIR_ESCREVER | ABRIR_LER;
    int fd = abrir(caminho, modo);
    if (fd < 0 && (flags & SQLITE_OPEN_READWRITE) && existe) {   /* sin permiso de escribir: solo lectura */
        fd = abrir(caminho, ABRIR_LER);
        if (fd >= 0) flags = (flags & ~SQLITE_OPEN_READWRITE) | SQLITE_OPEN_READONLY;
    }
    if (fd < 0) return SQLITE_CANTOPEN;
    a->fd = fd;
    if ((flags & SQLITE_OPEN_MAIN_DB) && (flags & SQLITE_OPEN_READWRITE)) {
        size_t n = tam_texto(caminho);
        a->cerrolho = sqlite3_malloc((int)(n + 6));
        if (a->cerrolho) { memcpy(a->cerrolho, caminho, n); memcpy(a->cerrolho + n, "-lock", 6); }
    }
    if (flags & SQLITE_OPEN_DELETEONCLOSE) {
        size_t n = tam_texto(caminho) + 1;
        a->apagar = sqlite3_malloc((int)n);
        if (a->apagar) memcpy(a->apagar, caminho, n);
    }
    if (flags_saida) *flags_saida = flags;
    a->base.pMethods = &metodos;
    return SQLITE_OK;
}

static int sv_apagar(sqlite3_vfs *vfs, const char *nome, int sync_dir)
{
    (void)vfs; (void)sync_dir;
    int r = apagar(nome);
    if (r == ERRO_NAO_EXISTE) return SQLITE_IOERR_DELETE_NOENT;
    return r < 0 ? SQLITE_IOERR_DELETE : SQLITE_OK;
}

static int sv_acessar(sqlite3_vfs *vfs, const char *nome, int flags, int *saida)
{
    (void)vfs; (void)flags;
    info_arquivo_t inf;
    *saida = info_arquivo(nome, &inf) == 0;
    return SQLITE_OK;
}

static int sv_caminho_completo(sqlite3_vfs *vfs, const char *nome, int n, char *saida)
{
    (void)vfs;
    saida[0] = 0;
    if (nome[0] == '/') {
        if ((int)tam_texto(nome) >= n) return SQLITE_CANTOPEN;
        copia_texto(saida, nome, (size_t)n);
        return SQLITE_OK;
    }
    char atual[MAX_CAMINHO];
    if (pasta_atual(atual, sizeof atual) < 0) return SQLITE_CANTOPEN;
    size_t t = tam_texto(atual);
    if (t > 0 && atual[t - 1] == '/') atual[--t] = 0;
    if ((int)(t + 1 + tam_texto(nome)) >= n) return SQLITE_CANTOPEN;
    formatar_texto(saida, (size_t)n, "%s/%s", atual, nome);
    return SQLITE_OK;
}

static int sv_aleatorio(sqlite3_vfs *vfs, int n, char *buf)
{
    (void)vfs;
    int feito = 0;
    while (feito < n) {
        int pedaco = n - feito > 4096 ? 4096 : n - feito;
        if (bytes_aleatorios(buf + feito, (unsigned)pedaco) <= 0) break;
        feito += pedaco;
    }
    return feito;
}

static int sv_dormir(sqlite3_vfs *vfs, int microsegundos)
{
    (void)vfs;
    dormir((unsigned)(microsegundos + 999) / 1000);
    return microsegundos;
}

/* Hora en milisegundos desde el día juliano 0: Unix * 1000 + 2440587.5 días. */
static int sv_hora64(sqlite3_vfs *vfs, sqlite3_int64 *saida)
{
    (void)vfs;
    hora_t h;
    unsigned s = hora_utc(&h);
    *saida = 210866760000000LL + (sqlite3_int64)s * 1000 + (sqlite3_int64)h.ms;
    return SQLITE_OK;
}

static int sv_hora(sqlite3_vfs *vfs, double *saida)
{
    sqlite3_int64 ms;
    sv_hora64(vfs, &ms);
    *saida = (double)ms / 86400000.0;
    return SQLITE_OK;
}

static int sv_ultimo_erro(sqlite3_vfs *vfs, int n, char *buf) { (void)vfs; (void)n; (void)buf; return 0; }

static sqlite3_vfs vfs_sagui = {
    2, sizeof(arq_sagui_t), MAX_CAMINHO, NULL, "sagui", NULL,
    sv_abrir, sv_apagar, sv_acessar, sv_caminho_completo,
    0, 0, 0, 0,   /* dlopen, dlerror, dlsym, dlclose (sin extensiones) */
    sv_aleatorio, sv_dormir, sv_hora, sv_ultimo_erro,
    sv_hora64, 0, 0, 0,
};

int sqlite3_os_init(void) { return sqlite3_vfs_register(&vfs_sagui, 1); }
int sqlite3_os_end(void) { return SQLITE_OK; }
