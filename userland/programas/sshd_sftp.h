// SPDX-License-Identifier: GPL-2.0-or-later
/* sshd_sftp.h - el subsistema "sftp" del servidor SSH (D250): SFTP versión 3
 * (draft-ietf-secsh-filexfer-02), lo que usan el `sftp` y el `scp` de OpenSSH.
 * Solo lo incluye sshd.c: usa su registro (anotar) y sus canales.
 *
 * Lo que hay: abrir, leer, escribir, cerrar, stat/lstat/fstat, listar carpetas,
 * borrar, mkdir, rmdir, renombrar y realpath. setstat/fsetstat solo aplican el
 * tamaño (truncar); permisos, dueño y fechas se aceptan y se ignoran (SaguiOS
 * no los tiene). Sin enlaces simbólicos ni extensiones.
 *
 * Las rutas llegan en UTF-8 y los nombres de SaguiOS son Latin-1: se
 * convierten en los dos sentidos. Las relativas salen de /home. */
#ifndef SSHD_SFTP_H
#define SSHD_SFTP_H

#define FXP_INIT      1
#define FXP_VERSION   2
#define FXP_OPEN      3
#define FXP_CLOSE     4
#define FXP_READ      5
#define FXP_WRITE     6
#define FXP_LSTAT     7
#define FXP_FSTAT     8
#define FXP_SETSTAT   9
#define FXP_FSETSTAT  10
#define FXP_OPENDIR   11
#define FXP_READDIR   12
#define FXP_REMOVE    13
#define FXP_MKDIR     14
#define FXP_RMDIR     15
#define FXP_REALPATH  16
#define FXP_STAT      17
#define FXP_RENAME    18
#define FXP_STATUS    101
#define FXP_HANDLE    102
#define FXP_DATA      103
#define FXP_NAME      104
#define FXP_ATTRS     105

#define FX_OK          0
#define FX_EOF         1
#define FX_NO_EXISTE   2
#define FX_PERMISO     3
#define FX_FALLO       4
#define FX_MAL_MENSAJE 5
#define FX_SIN_SOPORTE 8

#define FXF_READ   1
#define FXF_WRITE  2
#define FXF_APPEND 4
#define FXF_CREAT  8
#define FXF_TRUNC  16
#define FXF_EXCL   32

#define FATTR_SIZE  1
#define FATTR_UIDGID 2
#define FATTR_PERM  4
#define FATTR_TEMPO 8

#define SFTP_PUNHOS   16
#define SFTP_PKT_MAX  (128u * 1024u)   /* un paquete del cliente (las escrituras de OpenSSH son de 32 KB) */
#define SFTP_LER_MAX  32768u           /* lo máximo que se devuelve en una lectura */
#define SFTP_RESP_MAX (SFTP_LER_MAX + 1024u)
#define SFTP_BASE     "/home"
#define SFTP_MAGIA    0x53474600u      /* los identificadores de archivo abierto: "SGF" + índice */

typedef struct {
    int  tipo;            /* 0 libre, 1 archivo, 2 carpeta */
    int  fd;
    int  indice;          /* carpeta: la siguiente entrada */
    bool acrescentar;
    char caminho[256];
} sftp_punho_t;

typedef struct {
    sftp_punho_t punho[SFTP_PUNHOS];
    uint8_t *resp;        /* SFTP_RESP_MAX */
    bool iniciado;
} sftp_t;

static bool canal_fila_por(struct canal *c, const uint8_t *p, unsigned n);

static sftp_t *sftp_criar(void)
{
    sftp_t *s = aloca_zerado(sizeof *s);
    if (!s) return NULL;
    s->resp = aloca(SFTP_RESP_MAX);
    if (!s->resp) { libera(s); return NULL; }
    return s;
}

static void sftp_destruir(sftp_t *s)
{
    if (!s) return;
    for (int i = 0; i < SFTP_PUNHOS; i++)
        if (s->punho[i].tipo == 1) fechar(s->punho[i].fd);
    libera(s->resp);
    libera(s);
}

/* Manda una respuesta: tamaño + cuerpo, a la cola de salida del canal. */
static void sftp_responder(struct canal *c, ssh_escritor_t *e)
{
    uint8_t t[4] = { (uint8_t)(e->n >> 24), (uint8_t)(e->n >> 16), (uint8_t)(e->n >> 8), (uint8_t)e->n };
    canal_fila_por(c, t, 4);
    canal_fila_por(c, e->b, e->n);
}

static void sftp_estado(struct canal *c, sftp_t *s, uint32_t id, int codigo)
{
    static const char *const textos[] = { "bien", "fin del archivo", "no existe", "permiso denegado", "fallo",
                                          "mensaje mal formado", "", "", "no soportado" };
    ssh_escritor_t e;
    se_iniciar(&e, s->resp, SFTP_RESP_MAX);
    se_u8(&e, FXP_STATUS);
    se_u32(&e, id);
    se_u32(&e, (uint32_t)codigo);
    se_cstr(&e, codigo >= 0 && codigo <= 8 ? textos[codigo] : "");
    se_cstr(&e, "es");
    sftp_responder(c, &e);
}

static int sftp_codigo(int erro)
{
    if (erro >= 0) return FX_OK;
    if (erro == ERRO_NAO_EXISTE) return FX_NO_EXISTE;
    if (erro == ERRO_ACESSO) return FX_PERMISO;
    return FX_FALLO;
}

/* Lee una ruta del paquete (UTF-8) y la deja absoluta y en Latin-1. */
static bool sftp_ler_caminho(ssh_leitor_t *l, char *dst, unsigned max)
{
    unsigned n;
    const uint8_t *p = sl_texto(l, &n);
    char tmp[400];
    if (l->erro || n >= sizeof tmp) return false;
    for (unsigned i = 0; i < n; i++)
        if (!p[i]) return false;
    n = ssh_de_utf8(p, n, (uint8_t *)tmp);
    tmp[n] = 0;
    return ssh_caminho(SFTP_BASE, tmp, dst, max);
}

/* Los atributos del paquete: solo interesa el tamaño. */
static void sftp_ler_attrs(ssh_leitor_t *l, bool *tem_tam, uint32_t *tam)
{
    uint32_t f = sl_u32(l);
    *tem_tam = false;
    if (f & FATTR_SIZE) { *tam = sl_u64_baixo(l); *tem_tam = true; }
    if (f & FATTR_UIDGID) { sl_u32(l); sl_u32(l); }
    if (f & FATTR_PERM) sl_u32(l);
    if (f & FATTR_TEMPO) { sl_u32(l); sl_u32(l); }
    /* los extendidos (bit 31) no se esperan en la versión 3 de OpenSSH */
}

static uint32_t sftp_permissoes(unsigned tipo)
{
    if (tipo == TIPO_PASTA) return 0040755;
    if (tipo == TIPO_PROGRAMA) return 0100755;
    return 0100644;
}

static void sftp_por_attrs(ssh_escritor_t *e, const char *caminho, unsigned tipo, unsigned tamanho, uint32_t *mod)
{
    datas_arquivo_t d;
    memset(&d, 0, sizeof d);
    datas_arquivo(caminho, &d);
    se_u32(e, FATTR_SIZE | FATTR_UIDGID | FATTR_PERM | FATTR_TEMPO);
    se_u64(e, tamanho);
    se_u32(e, 0);
    se_u32(e, 0);
    se_u32(e, sftp_permissoes(tipo));
    se_u32(e, d.acessado ? d.acessado : d.modificado);
    se_u32(e, d.modificado);
    if (mod) *mod = d.modificado;
}

static void sftp_stat(struct canal *c, sftp_t *s, uint32_t id, const char *caminho)
{
    info_arquivo_t i;
    int r = info_arquivo(caminho, &i);
    if (r < 0) { sftp_estado(c, s, id, sftp_codigo(r)); return; }
    ssh_escritor_t e;
    se_iniciar(&e, s->resp, SFTP_RESP_MAX);
    se_u8(&e, FXP_ATTRS);
    se_u32(&e, id);
    sftp_por_attrs(&e, caminho, i.tipo, i.tamanho, NULL);
    sftp_responder(c, &e);
}

static sftp_punho_t *sftp_punho(sftp_t *s, ssh_leitor_t *l)
{
    unsigned n;
    const uint8_t *p = sl_texto(l, &n);
    if (l->erro || n != 4) return NULL;
    uint32_t v = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    if ((v & 0xFFFFFF00u) != SFTP_MAGIA || (v & 255) >= SFTP_PUNHOS) return NULL;
    sftp_punho_t *h = &s->punho[v & 255];
    return h->tipo ? h : NULL;
}

static void sftp_novo_punho(struct canal *c, sftp_t *s, uint32_t id, int tipo, int fd, const char *caminho, bool acrescentar)
{
    for (int i = 0; i < SFTP_PUNHOS; i++) {
        sftp_punho_t *h = &s->punho[i];
        if (h->tipo) continue;
        h->tipo = tipo;
        h->fd = fd;
        h->indice = 0;
        h->acrescentar = acrescentar;
        copia_texto(h->caminho, caminho, sizeof h->caminho);
        ssh_escritor_t e;
        se_iniciar(&e, s->resp, SFTP_RESP_MAX);
        se_u8(&e, FXP_HANDLE);
        se_u32(&e, id);
        se_u32(&e, 4);
        se_u32(&e, SFTP_MAGIA | (uint32_t)i);
        sftp_responder(c, &e);
        return;
    }
    if (tipo == 1) fechar(fd);
    sftp_estado(c, s, id, FX_FALLO);   /* demasiados archivos abiertos */
}

/* Una entrada de FXP_NAME: nombre, línea "larga" (lo que muestra ls -l) y atributos. */
static void sftp_por_nome(ssh_escritor_t *e, const char *pasta, const char *nome, unsigned tipo, unsigned tamanho)
{
    char caminho[400], longo[256], data[32];
    uint8_t utf[2 * NOME_MAX + 2];
    caminho_juntar(caminho, sizeof caminho, pasta, nome);
    unsigned n = ssh_para_utf8((const uint8_t *)nome, (unsigned)tam_texto(nome), utf);
    utf[n] = 0;
    se_texto(e, utf, n);
    /* los atributos van después de la línea larga, que necesita la fecha: se calculan aparte */
    uint8_t at[40];
    ssh_escritor_t ea;
    uint32_t mod = 0;
    se_iniciar(&ea, at, sizeof at);
    sftp_por_attrs(&ea, caminho, tipo, tamanho, &mod);
    fuso_data_t d;
    hora_local(mod, &d);
    if (mod) formatar_data(data, sizeof data, "%b %e %Y", &d);
    else copia_texto(data, "-", sizeof data);
    formatar_texto(longo, sizeof longo, "%s 1 root root %10u %s %s",
                   tipo == TIPO_PASTA ? "drwxr-xr-x" : tipo == TIPO_PROGRAMA ? "-rwxr-xr-x" : "-rw-r--r--",
                   tamanho, data, (const char *)utf);
    se_cstr(e, longo);
    se_bytes(e, at, ea.n);
}

/* Atiende un paquete completo del cliente (sin los 4 bytes del tamaño). */
static void sftp_pedido(struct canal *c, sftp_t *s, const uint8_t *p, unsigned n)
{
    ssh_leitor_t l;
    ssh_escritor_t e;
    char caminho[256], outro[256];
    sl_iniciar(&l, p, n);
    unsigned tipo = sl_u8(&l);
    if (tipo == FXP_INIT) {
        sl_u32(&l);   /* la versión del cliente: se contesta con la 3, sin extensiones */
        se_iniciar(&e, s->resp, SFTP_RESP_MAX);
        se_u8(&e, FXP_VERSION);
        se_u32(&e, 3);
        sftp_responder(c, &e);
        s->iniciado = true;
        return;
    }
    uint32_t id = sl_u32(&l);
    if (l.erro || !s->iniciado) { sftp_estado(c, s, id, FX_MAL_MENSAJE); return; }
    switch (tipo) {
    case FXP_OPEN: {
        bool tem_tam;
        uint32_t tam = 0;
        bool ok = sftp_ler_caminho(&l, caminho, sizeof caminho);
        uint32_t f = sl_u32(&l);
        sftp_ler_attrs(&l, &tem_tam, &tam);
        if (!ok || l.erro) { sftp_estado(c, s, id, FX_MAL_MENSAJE); return; }
        int modo = 0;
        if (f & FXF_READ) modo |= ABRIR_LER;
        if (f & FXF_WRITE) modo |= ABRIR_ESCREVER;
        if (f & FXF_CREAT) modo |= ABRIR_CRIAR;
        if (f & FXF_TRUNC) modo |= ABRIR_TRUNCAR;
        info_arquivo_t i;
        int existe = info_arquivo(caminho, &i);
        if (existe == 0 && i.tipo == TIPO_PASTA) { sftp_estado(c, s, id, FX_FALLO); return; }
        if ((f & FXF_EXCL) && existe == 0) { sftp_estado(c, s, id, FX_FALLO); return; }
        int fd = abrir(caminho, modo ? modo : ABRIR_LER);
        anotar("sftp: abrir %s (%s%s)%s%s", caminho, (f & FXF_WRITE) ? "escritura" : "lectura",
               (f & FXF_TRUNC) ? ", vaciar" : "", fd < 0 ? ": " : "", fd < 0 ? nome_erro(fd) : "");
        if (fd < 0) { sftp_estado(c, s, id, sftp_codigo(fd)); return; }
        sftp_novo_punho(c, s, id, 1, fd, caminho, (f & FXF_APPEND) != 0);
        return;
    }
    case FXP_CLOSE: {
        sftp_punho_t *h = sftp_punho(s, &l);
        if (!h) { sftp_estado(c, s, id, FX_FALLO); return; }
        int r = 0;
        if (h->tipo == 1) r = fechar(h->fd);
        h->tipo = 0;
        sftp_estado(c, s, id, sftp_codigo(r));
        return;
    }
    case FXP_READ: {
        sftp_punho_t *h = sftp_punho(s, &l);
        uint32_t pos = sl_u64_baixo(&l), q = sl_u32(&l);
        if (!h || h->tipo != 1 || l.erro) { sftp_estado(c, s, id, FX_FALLO); return; }
        if (q > SFTP_LER_MAX) q = SFTP_LER_MAX;
        se_iniciar(&e, s->resp, SFTP_RESP_MAX);
        se_u8(&e, FXP_DATA);
        se_u32(&e, id);
        int r = posicionar(h->fd, (int)pos, POS_INICIO);
        if (r >= 0) r = ler(h->fd, s->resp + e.n + 4, q);
        if (r < 0) { sftp_estado(c, s, id, sftp_codigo(r)); return; }
        if (r == 0) { sftp_estado(c, s, id, FX_EOF); return; }
        se_u32(&e, (uint32_t)r);
        e.n += (unsigned)r;
        sftp_responder(c, &e);
        return;
    }
    case FXP_WRITE: {
        sftp_punho_t *h = sftp_punho(s, &l);
        uint32_t pos = sl_u64_baixo(&l);
        unsigned q;
        const uint8_t *d = sl_texto(&l, &q);
        if (!h || h->tipo != 1 || l.erro) { sftp_estado(c, s, id, FX_FALLO); return; }
        int r = h->acrescentar ? posicionar(h->fd, 0, POS_FIM) : posicionar(h->fd, (int)pos, POS_INICIO);
        if (r >= 0) r = escrever_tudo(h->fd, d, q);
        if (r >= 0 && (unsigned)r != q) r = ERRO_INVALIDO;   /* disco lleno */
        sftp_estado(c, s, id, sftp_codigo(r));
        return;
    }
    case FXP_LSTAT:
    case FXP_STAT:
        if (!sftp_ler_caminho(&l, caminho, sizeof caminho)) { sftp_estado(c, s, id, FX_MAL_MENSAJE); return; }
        sftp_stat(c, s, id, caminho);
        return;
    case FXP_FSTAT: {
        sftp_punho_t *h = sftp_punho(s, &l);
        if (!h) { sftp_estado(c, s, id, FX_FALLO); return; }
        sftp_stat(c, s, id, h->caminho);
        return;
    }
    case FXP_SETSTAT:
    case FXP_FSETSTAT: {
        bool tem_tam;
        uint32_t tam = 0;
        sftp_punho_t *h = NULL;
        int r = 0;
        if (tipo == FXP_FSETSTAT) h = sftp_punho(s, &l);
        else if (!sftp_ler_caminho(&l, caminho, sizeof caminho)) { sftp_estado(c, s, id, FX_MAL_MENSAJE); return; }
        sftp_ler_attrs(&l, &tem_tam, &tam);
        if (l.erro || (tipo == FXP_FSETSTAT && !h)) { sftp_estado(c, s, id, FX_FALLO); return; }
        if (tem_tam) {
            if (h && h->tipo == 1) r = truncar(h->fd, tam);
            else if (!h) {
                int fd = abrir(caminho, ABRIR_ESCREVER);
                r = fd;
                if (fd >= 0) { r = truncar(fd, tam); fechar(fd); }
            }
        }
        sftp_estado(c, s, id, sftp_codigo(r));   /* permisos, dueño y fechas: aceptados e ignorados */
        return;
    }
    case FXP_OPENDIR: {
        info_arquivo_t i;
        if (!sftp_ler_caminho(&l, caminho, sizeof caminho)) { sftp_estado(c, s, id, FX_MAL_MENSAJE); return; }
        int r = info_arquivo(caminho, &i);
        if (r < 0) { sftp_estado(c, s, id, sftp_codigo(r)); return; }
        if (i.tipo != TIPO_PASTA) { sftp_estado(c, s, id, FX_FALLO); return; }
        sftp_novo_punho(c, s, id, 2, -1, caminho, false);
        return;
    }
    case FXP_READDIR: {
        sftp_punho_t *h = sftp_punho(s, &l);
        if (!h || h->tipo != 2) { sftp_estado(c, s, id, FX_FALLO); return; }
        se_iniciar(&e, s->resp, SFTP_RESP_MAX);
        se_u8(&e, FXP_NAME);
        se_u32(&e, id);
        unsigned onde = e.n, quantos = 0;
        se_u32(&e, 0);
        while (quantos < 40) {
            entrada_pasta_t en;
            if (listar(h->caminho, h->indice, &en) <= 0) break;
            h->indice++;
            sftp_por_nome(&e, h->caminho, en.nome, en.tipo, en.tamanho);
            quantos++;
        }
        if (!quantos || e.erro) { sftp_estado(c, s, id, quantos ? FX_FALLO : FX_EOF); return; }
        s->resp[onde + 3] = (uint8_t)quantos;
        sftp_responder(c, &e);
        return;
    }
    case FXP_REMOVE:
    case FXP_RMDIR:
    case FXP_MKDIR: {
        if (!sftp_ler_caminho(&l, caminho, sizeof caminho)) { sftp_estado(c, s, id, FX_MAL_MENSAJE); return; }
        int r = tipo == FXP_REMOVE ? apagar(caminho) : tipo == FXP_RMDIR ? remover_pasta(caminho) : criar_pasta(caminho);
        anotar("sftp: %s %s%s%s", tipo == FXP_REMOVE ? "borrar" : tipo == FXP_RMDIR ? "rmdir" : "mkdir", caminho,
               r < 0 ? ": " : "", r < 0 ? nome_erro(r) : "");
        sftp_estado(c, s, id, sftp_codigo(r));
        return;
    }
    case FXP_RENAME: {
        if (!sftp_ler_caminho(&l, caminho, sizeof caminho) || !sftp_ler_caminho(&l, outro, sizeof outro)) {
            sftp_estado(c, s, id, FX_MAL_MENSAJE);
            return;
        }
        info_arquivo_t i;
        int r = info_arquivo(outro, &i) == 0 ? ERRO_JA_EXISTE : renomear(caminho, outro);
        anotar("sftp: renombrar %s -> %s%s%s", caminho, outro, r < 0 ? ": " : "", r < 0 ? nome_erro(r) : "");
        sftp_estado(c, s, id, sftp_codigo(r));
        return;
    }
    case FXP_REALPATH: {
        if (!sftp_ler_caminho(&l, caminho, sizeof caminho)) { sftp_estado(c, s, id, FX_MAL_MENSAJE); return; }
        uint8_t utf[2 * sizeof caminho];
        unsigned k = ssh_para_utf8((const uint8_t *)caminho, (unsigned)tam_texto(caminho), utf);
        se_iniciar(&e, s->resp, SFTP_RESP_MAX);
        se_u8(&e, FXP_NAME);
        se_u32(&e, id);
        se_u32(&e, 1);
        se_texto(&e, utf, k);
        se_texto(&e, utf, k);
        se_u32(&e, 0);   /* sin atributos */
        sftp_responder(c, &e);
        return;
    }
    default:
        sftp_estado(c, s, id, FX_SIN_SOPORTE);   /* enlaces simbólicos, extensiones */
        return;
    }
}

#endif
