// SPDX-License-Identifier: GPL-2.0-or-later
/* ramfs.c - una rama del árbol en RAM montada como un volumen más (D379, fase R3).
 *
 * El árbol en RAM (vfs.c: el ramdisk o el initramfs con su capa de copia al escribir)
 * fue siempre la raíz y no era un sistema de archivos montable. Con la raíz en un
 * disco hace falta poder montar trozos de él en otra parte: /tmp en RAM (como el tmpfs
 * de Linux), /initrd (el initramfs, para el rescate) y, en la live, la capa escribible
 * (sobreposicao.c). Este driver hace de una carpeta del árbol en RAM (el «prefijo»)
 * un sistema_arquivos_t: cada operación traduce la ruta relativa a la ruta del árbol
 * y llama a las funciones de vfs.c, que solo tocan la RAM.
 *
 * Cada volumen lleva su propio disco_t de mentira (no está en la tabla de discos: no
 * sale en «discos» ni lo toca la caché), porque la tabla de montajes trabaja con discos. */
#include "ramfs.h"
#include "console.h"
#include "memoria.h"
#include "montagem.h"
#include "relogio.h"
#include "texto.h"
#include "vfs.h"

#define MAX_RAMFS 4

typedef struct {
    disco_t d;                       /* el primero: montagem.c ve el volumen por aquí */
    char    prefixo[CAMINHO_MAX];    /* carpeta del árbol en RAM ("" = la raíz del árbol) */
    bool    usado;
} ramfs_vol_t;

static ramfs_vol_t vols[MAX_RAMFS];

typedef struct { no_t *no; } ramfs_arq_t;

/* La ruta del árbol en RAM para "rel" dentro del volumen. */
static bool caminho(const ramfs_vol_t *v, const char *rel, char *saida)
{
    size_t np = txt_tamanho(v->prefixo), nr = txt_tamanho(rel);
    if (np + nr + 2 > CAMINHO_MAX) return false;
    memcpy(saida, v->prefixo, np);
    saida[np] = '/';
    memcpy(saida + np + 1, rel, nr + 1);
    if (!np && !nr) saida[1] = 0;     /* "/" */
    else if (!nr) saida[np] = 0;      /* la carpeta del prefijo */
    return true;
}

#define V ((ramfs_vol_t *)priv)
#define CAMINHO(rel, c) char c[CAMINHO_MAX]; if (!caminho(V, rel, c)) return ERRO_INVALIDO

static int r_montar(disco_t *d, uint32_t *opcoes, void **priv)
{
    (void)opcoes;
    ramfs_vol_t *v = (ramfs_vol_t *)d;
    no_t *x = vfs_achar(v->prefixo[0] ? v->prefixo : "/");
    if (!x || x->tipo != TIPO_PASTA) return ERRO_NAO_EXISTE;
    *priv = v;
    return 0;
}

static int r_desmontar(void *priv, bool gravar) { (void)gravar; V->usado = false; return 0; }

static int r_info(void *priv, const char *rel, info_arquivo_t *inf)
{
    CAMINHO(rel, c);
    no_t *x = vfs_achar(c);
    if (!x) return ERRO_NAO_EXISTE;
    inf->tipo = x->tipo;
    inf->tamanho = x->tipo == TIPO_PASTA ? 0 : x->tamanho;
    return 0;
}

static int r_listar(void *priv, const char *rel, int i, entrada_pasta_t *e)
{
    CAMINHO(rel, c);
    return vfs_listar(c, i, e);
}

static int r_abrir(void *priv, const char *rel, int modo, void *arq, uint32_t *tam)
{
    CAMINHO(rel, c);
    no_t *x = vfs_achar(c);
    if (!x) {
        if (!(modo & ABRIR_CRIAR)) return ERRO_NAO_EXISTE;
        int r = vfs_criar(c, TIPO_ARQUIVO, &x);
        if (r < 0) return r;
    }
    if (x->tipo == TIPO_PASTA) return ERRO_E_PASTA;
    if ((modo & ABRIR_TRUNCAR) && (modo & ABRIR_ESCREVER)) vfs_truncar(x);
    vfs_segurar(x);
    ((ramfs_arq_t *)arq)->no = x;
    *tam = x->tamanho;
    return 0;
}

static int r_ler(void *priv, void *arq, uint32_t pos, void *buf, uint32_t n)
{
    (void)priv;
    return (int)vfs_ler(((ramfs_arq_t *)arq)->no, pos, buf, n);
}

static int r_escrever(void *priv, void *arq, uint32_t pos, const void *buf, uint32_t n, uint32_t *tam)
{
    (void)priv;
    no_t *x = ((ramfs_arq_t *)arq)->no;
    int r = vfs_escrever(x, pos, buf, n);
    *tam = x->tamanho;
    return r;
}

static int r_truncar(void *priv, void *arq, uint32_t tam)
{
    (void)priv;
    return vfs_redimensionar(((ramfs_arq_t *)arq)->no, tam);
}

static void r_fechar(void *priv, void *arq)
{
    (void)priv;
    no_t *x = ((ramfs_arq_t *)arq)->no;
    if (x) vfs_soltar(x);
    ((ramfs_arq_t *)arq)->no = NULL;
}

static int r_criar_pasta(void *priv, const char *rel)
{
    CAMINHO(rel, c);
    return vfs_criar(c, TIPO_PASTA, NULL);
}

static int r_remover_pasta(void *priv, const char *rel)
{
    if (!*rel) return ERRO_ACESSO;
    CAMINHO(rel, c);
    return vfs_remover_pasta(c);
}

static int r_apagar(void *priv, const char *rel)
{
    CAMINHO(rel, c);
    return vfs_apagar(c);
}

static int r_renomear(void *priv, const char *de, const char *para)
{
    if (!*de || !*para) return ERRO_ACESSO;
    char a[CAMINHO_MAX], b[CAMINHO_MAX];
    if (!caminho(V, de, a) || !caminho(V, para, b)) return ERRO_INVALIDO;
    return vfs_renomear(a, b);
}

static void r_espaco(void *priv, uint32_t *total, uint32_t *livre)
{
    (void)priv;
    /* Lo que queda en la RAM: el volumen crece hasta donde dé la memoria del kernel. */
    *livre = memfisica_livres() * 4;
    *total = *livre;
}

static int r_datas(void *priv, const char *rel, datas_arquivo_t *d)
{
    CAMINHO(rel, c);
    no_t *x = vfs_achar(c);
    if (!x) return ERRO_NAO_EXISTE;
    d->modificado = x->modificado;
    d->criado = 0;
    d->acessado = 0;
    return 0;
}

static int r_meta(void *priv, const char *rel, meta_t *m)
{
    CAMINHO(rel, c);
    no_t *x = vfs_achar(c);
    if (!x) return ERRO_NAO_EXISTE;
    m->uid = x->uid;
    m->gid = x->gid;
    m->modo = x->modo;
    m->tipo = x->tipo;
    return 0;
}

static int r_fixar_meta(void *priv, const char *rel, int32_t uid, int32_t gid, int32_t modo)
{
    CAMINHO(rel, c);
    no_t *x = vfs_achar(c);
    if (!x) return ERRO_NAO_EXISTE;
    if (uid >= 0) x->uid = (uint32_t)uid;
    if (gid >= 0) x->gid = (uint32_t)gid;
    if (modo >= 0) x->modo = (uint16_t)(modo & MODO_PERMISOS);
    return 0;
}

static int r_fixar_datas(void *priv, const char *rel, uint32_t mod, uint32_t acesso)
{
    (void)acesso;
    CAMINHO(rel, c);
    no_t *x = vfs_achar(c);
    if (!x) return ERRO_NAO_EXISTE;
    if (mod) x->modificado = mod;
    return 0;
}
#undef V
#undef CAMINHO

const sistema_arquivos_t fs_ramfs = {
    .nome = "ramfs", .tam_arq = sizeof(ramfs_arq_t),
    .montar = r_montar, .desmontar = r_desmontar, .info = r_info, .listar = r_listar,
    .abrir = r_abrir, .ler = r_ler, .escrever = r_escrever, .truncar = r_truncar, .fechar = r_fechar,
    .criar_pasta = r_criar_pasta, .remover_pasta = r_remover_pasta, .apagar = r_apagar, .renomear = r_renomear,
    .espaco = r_espaco, .datas = r_datas, .meta = r_meta, .fixar_meta = r_fixar_meta, .fixar_datas = r_fixar_datas,
};

/* D379 (L1): un volumen sin montar, para la capa de la live (sobreposicao.c lo llama directamente). */
void *ramfs_volume(const char *prefixo)
{
    ramfs_vol_t *v = NULL;
    for (int i = 0; i < MAX_RAMFS && !v; i++) if (!vols[i].usado) v = &vols[i];
    if (!v) return NULL;
    if (prefixo[0] && !vfs_achar(prefixo) && vfs_criar(prefixo, TIPO_PASTA, NULL) < 0) return NULL;
    memset(v, 0, sizeof *v);
    txt_copia_n(v->prefixo, prefixo, sizeof v->prefixo);
    v->usado = true;
    no_t *x = vfs_achar(prefixo);
    if (x) { x->uid = UID_ROOT; x->gid = GID_ROOT; x->modo = 0755; }
    return v;
}

void ramfs_volume_soltar(void *v) { if (v) ((ramfs_vol_t *)v)->usado = false; }

int ramfs_montar(const char *ponto, const char *prefixo, const char *nome, uint32_t opcoes)
{
    ramfs_vol_t *v = NULL;
    for (int i = 0; i < MAX_RAMFS && !v; i++) if (!vols[i].usado) v = &vols[i];
    if (!v) return ERRO_LIMITE;
    /* La carpeta del prefijo, si falta (p. ej. una capa nueva de la live). */
    if (prefixo[0] && !vfs_achar(prefixo)) {
        int r = vfs_criar(prefixo, TIPO_PASTA, NULL);
        if (r < 0) return r;
    }
    memset(v, 0, sizeof *v);
    txt_copia_n(v->prefixo, txt_compara(prefixo, "/") == 0 ? "" : prefixo, sizeof v->prefixo);
    txt_copia_n(v->d.nome, nome, sizeof v->d.nome);
    txt_copia_n(v->d.formato, "RAM", sizeof v->d.formato);
    txt_copia_n(v->d.tipo_fs, "ramfs", sizeof v->d.tipo_fs);
    v->d.presente = true;
    v->usado = true;
    int r = montagem_montar_dono(&v->d, ponto, "ramfs", opcoes | MONTAR_DONO, UID_ROOT | GID_ROOT << 16);
    if (r < 0) v->usado = false;
    return r;
}
