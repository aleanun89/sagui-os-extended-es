// SPDX-License-Identifier: GPL-2.0-or-later
/* sobreposicao.c - la raíz de la live: el sistema del medio, de solo lectura, con una
 * capa escribible encima (D379, fase L1; al estilo del overlayfs de Linux).
 *
 *   capa inferior: el ext2 SAGUI-RAIZ del pendrive o de la imagen, montado SOLO para leer
 *                  (el sistema de serie no se toca nunca: un corte no lo estropea);
 *   capa superior: donde va todo lo que se cambia. En la RAM (una carpeta del árbol en RAM,
 *                  ramfs.c: se pierde al apagar) o, en la live «con persistencia» (fase L2),
 *                  un ext2 SAGUI-PERSIST del mismo medio.
 *
 * Reglas (las de overlayfs, en pequeño):
 *   - Buscar: primero la superior; luego la inferior, salvo que una «marca de borrado» la tape.
 *   - Escribir algo que solo está en la inferior: se copia antes a la superior («copy-up»),
 *     con sus carpetas, dueño, permisos y fecha.
 *   - Borrar algo de la inferior: se deja en la superior la marca ".wh.<nombre>" junto a
 *     donde estaba. Una carpeta borrada y vuelta a crear lleva ".wh..opq" (opaca): lo de
 *     la inferior con ese nombre ya no se ve dentro.
 *   - Listar: lo de la superior (sin las marcas) y lo de la inferior que no esté repetido ni
 *     marcado.
 *   - Renombrar una carpeta que está en la inferior no se admite (como EXDEV en Linux): los
 *     programas copian y borran. Los archivos, sí (se copian arriba primero).
 * Los nombres que empiezan por ".wh." son de la capa: no se ven ni se pueden crear.
 *
 * La tabla de montajes llama a esto con el bloqueo de los sistemas de archivos tomado
 * (montagem.c, D095); las dos capas se llaman directamente, sin volver a pasar por él. */
#include "sobreposicao.h"
#include "console.h"
#include "memoria.h"
#include "montagem.h"
#include "permisos.h"
#include "ramfs.h"
#include "texto.h"

typedef struct {
    const sistema_arquivos_t *fs;
    void *priv;
} capa_t;

static struct {
    capa_t inf, sup;
    disco_t *disco_sup;           /* SAGUI-PERSIST (L2), o NULL con la capa en la RAM */
} vol;

/* Lo que se pide antes de montar (raiz.c). */
static disco_t *pedido_sup;

#define MARCA     ".wh."
#define MARCA_N   4
#define OPACA     ".wh..opq"

typedef struct {
    uint8_t capa;                 /* 0 = inferior, 1 = superior */
    uint8_t pad[7];
    uint8_t sub[];                /* el archivo abierto de esa capa */
} arq_t;

/* ------------------------------------------------------------ rutas */

static bool e_marca(const char *nome) { return txt_compara_n(nome, MARCA, MARCA_N) == 0; }

static const char *ultimo(const char *rel)
{
    const char *n = rel;
    for (const char *p = rel; *p; p++) if (*p == '/') n = p + 1;
    return n;
}

/* "a/b/c" -> pasta "a/b"; "c" -> "". */
static void pasta_de(const char *rel, char *pasta)
{
    const char *n = ultimo(rel);
    size_t k = n > rel ? (size_t)(n - rel - 1) : 0;
    memcpy(pasta, rel, k);
    pasta[k] = 0;
}

static bool juntar(char *saida, const char *pasta, const char *nome)
{
    size_t a = txt_tamanho(pasta), b = txt_tamanho(nome);
    if (a + b + 2 > CAMINHO_MAX) return false;
    if (a) { memcpy(saida, pasta, a); saida[a++] = '/'; }
    memcpy(saida + a, nome, b + 1);
    return true;
}

static bool nome_marca(char *saida, const char *pasta, const char *nome)
{
    char m[NOME_MAX + MARCA_N + 1];
    if (txt_tamanho(nome) + MARCA_N >= sizeof m) return false;
    memcpy(m, MARCA, MARCA_N);
    txt_copia_n(m + MARCA_N, nome, sizeof m - MARCA_N);
    return juntar(saida, pasta, m);
}

/* ¿Algún nombre de la ruta empieza por ".wh."? Esas rutas no existen para nadie. */
static bool tem_marca(const char *rel)
{
    const char *c = rel;
    while (*c) {
        if (e_marca(c)) return true;
        while (*c && *c != '/') c++;
        if (*c) c++;
    }
    return false;
}

/* ------------------------------------------------------------ las capas */

static bool sup_info(const char *rel, info_arquivo_t *i)
{
    info_arquivo_t x;
    return vol.sup.fs->info(vol.sup.priv, rel, i ? i : &x) == 0;
}

static bool inf_info(const char *rel, info_arquivo_t *i)
{
    info_arquivo_t x;
    return vol.inf.fs->info(vol.inf.priv, rel, i ? i : &x) == 0;
}

static bool marcado(const char *pasta, const char *nome)
{
    char m[CAMINHO_MAX];
    return nome_marca(m, pasta, nome) && sup_info(m, NULL);
}

static bool opaca(const char *pasta)
{
    char m[CAMINHO_MAX];
    return juntar(m, pasta, OPACA) && sup_info(m, NULL);
}

/* ¿Se ve la inferior en esta ruta? (ni una marca ni una carpeta opaca por el camino) */
static bool inf_visivel(const char *rel)
{
    char pasta[CAMINHO_MAX] = "";
    const char *c = rel;
    while (*c) {
        const char *fim = c;
        while (*fim && *fim != '/') fim++;
        char nome[NOME_MAX];
        size_t n = (size_t)(fim - c);
        if (n >= sizeof nome) return false;
        memcpy(nome, c, n);
        nome[n] = 0;
        if (opaca(pasta) || marcado(pasta, nome)) return false;
        char prox[CAMINHO_MAX];
        if (!juntar(prox, pasta, nome)) return false;
        txt_copia_n(pasta, prox, sizeof pasta);
        c = *fim ? fim + 1 : fim;
    }
    return true;
}

/* Lo que se ve: 1 = superior, 0 = inferior, -1 = no existe. */
static int onde(const char *rel, info_arquivo_t *i)
{
    if (tem_marca(rel)) return -1;
    if (sup_info(rel, i)) return 1;
    if (inf_visivel(rel) && inf_info(rel, i)) return 0;
    return -1;
}

static void copiar_meta(const char *rel)
{
    meta_t m;
    if (vol.inf.fs->meta && vol.inf.fs->meta(vol.inf.priv, rel, &m) == 0 && vol.sup.fs->fixar_meta)
        vol.sup.fs->fixar_meta(vol.sup.priv, rel, (int32_t)m.uid, (int32_t)m.gid, (int32_t)(m.modo & MODO_PERMISOS));
    datas_arquivo_t d;
    if (vol.inf.fs->datas && vol.inf.fs->datas(vol.inf.priv, rel, &d) == 0 && d.modificado && vol.sup.fs->fixar_datas)
        vol.sup.fs->fixar_datas(vol.sup.priv, rel, d.modificado, 0);
}

/* Las carpetas de "rel" (incluida) en la superior, con los permisos de la inferior. */
static int garantir_pasta(const char *rel)
{
    char parcial[CAMINHO_MAX];
    size_t n = txt_tamanho(rel);
    if (!n) return 0;
    for (size_t i = 0; i <= n; i++) {
        if (rel[i] != '/' && rel[i] != 0) continue;
        memcpy(parcial, rel, i);
        parcial[i] = 0;
        info_arquivo_t inf;
        if (sup_info(parcial, &inf)) {
            if (inf.tipo != TIPO_PASTA) return ERRO_NAO_E_PASTA;
            continue;
        }
        int r = vol.sup.fs->criar_pasta(vol.sup.priv, parcial);
        if (r < 0) return r;
        copiar_meta(parcial);
    }
    return 0;
}

/* Copia un archivo de la inferior a la superior (copy-up). */
static int subir(const char *rel)
{
    char pasta[CAMINHO_MAX];
    pasta_de(rel, pasta);
    int r = garantir_pasta(pasta);
    if (r < 0) return r;
    uint8_t *a = kcalloc(vol.inf.fs->tam_arq + 8), *b = kcalloc(vol.sup.fs->tam_arq + 8), *buf = kmalloc(4096);
    if (!a || !b || !buf) { r = ERRO_SEM_MEMORIA; goto fim; }
    uint32_t tam = 0, tam2 = 0;
    r = vol.inf.fs->abrir(vol.inf.priv, rel, ABRIR_LER, a, &tam);
    if (r < 0) goto fim;
    r = vol.sup.fs->abrir(vol.sup.priv, rel, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR, b, &tam2);
    if (r < 0) { vol.inf.fs->fechar(vol.inf.priv, a); goto fim; }
    for (uint32_t pos = 0; pos < tam && r >= 0; ) {
        int n = vol.inf.fs->ler(vol.inf.priv, a, pos, buf, 4096);
        if (n <= 0) { r = n < 0 ? n : ERRO_FORMATO; break; }
        r = vol.sup.fs->escrever(vol.sup.priv, b, pos, buf, (uint32_t)n, &tam2);
        pos += (uint32_t)n;
    }
    vol.inf.fs->fechar(vol.inf.priv, a);
    vol.sup.fs->fechar(vol.sup.priv, b);
    if (r >= 0) { copiar_meta(rel); r = 0; }
    else vol.sup.fs->apagar(vol.sup.priv, rel);   /* nada a medias en la superior */
fim:
    if (a) kfree(a);
    if (b) kfree(b);
    if (buf) kfree(buf);
    if (r < 0) klog("[SOBRE] copy-up de %s fallo (%d)\n", rel, r);
    return r;
}

static void tirar_marca(const char *pasta, const char *nome)
{
    char m[CAMINHO_MAX];
    if (nome_marca(m, pasta, nome) && sup_info(m, NULL)) vol.sup.fs->apagar(vol.sup.priv, m);
}

static int criar_vazio(const char *rel)
{
    uint8_t *b = kcalloc(vol.sup.fs->tam_arq + 8);
    if (!b) return ERRO_SEM_MEMORIA;
    uint32_t t = 0;
    int r = vol.sup.fs->abrir(vol.sup.priv, rel, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR, b, &t);
    if (r >= 0) { vol.sup.fs->fechar(vol.sup.priv, b); r = 0; }
    kfree(b);
    return r;
}

static int por_marca(const char *pasta, const char *nome)
{
    char m[CAMINHO_MAX];
    if (!nome_marca(m, pasta, nome)) return ERRO_INVALIDO;
    int r = garantir_pasta(pasta);
    return r < 0 ? r : criar_vazio(m);
}

/* ------------------------------------------------------------ operaciones */

static int s_montar(disco_t *d, uint32_t *opcoes, void **priv)
{
    memset(&vol, 0, sizeof vol);
    uint32_t ro = MONTAR_SO_LEITURA;
    void *p = NULL;
    int r = fs_ext2.montar(d, &ro, &p);        /* la inferior, siempre de solo lectura */
    if (r < 0) return r;
    vol.inf.fs = &fs_ext2;
    vol.inf.priv = p;
    if (pedido_sup) {                          /* L2: la capa persistente */
        uint32_t rw = MONTAR_LEITURA_ESCRITA;
        void *q = NULL;
        pedido_sup->rw_autorizado = true;
        r = fs_ext2.montar(pedido_sup, &rw, &q);
        if (r >= 0 && !(rw & MONTAR_SO_LEITURA)) {
            vol.sup.fs = &fs_ext2;
            vol.sup.priv = q;
            vol.disco_sup = pedido_sup;
            klog("[SOBRE] capa persistente en %s\n", pedido_sup->nome);
        } else {
            if (r >= 0) fs_ext2.desmontar(q, false);
            pedido_sup->rw_autorizado = false;
            klog("[SOBRE] %s no se puede usar como capa (%d): la capa va en la RAM\n", pedido_sup->nome, r);
        }
    }
    if (!vol.sup.fs) {
        vol.sup.priv = ramfs_volume("/.capa");
        if (!vol.sup.priv) { fs_ext2.desmontar(p, false); return ERRO_SEM_MEMORIA; }
        vol.sup.fs = &fs_ramfs;
    }
    *opcoes &= ~(uint32_t)MONTAR_SO_LEITURA;   /* la vista es de lectura y escritura */
    *priv = &vol;
    return 0;
}

static int s_desmontar(void *priv, bool gravar)
{
    (void)priv;
    int r = 0;
    if (vol.disco_sup) {
        r = vol.sup.fs->desmontar(vol.sup.priv, gravar);
        disco_sincronizar(vol.disco_sup->pai ? vol.disco_sup->pai : vol.disco_sup);
        vol.disco_sup->rw_autorizado = false;
        vol.disco_sup->montagem = NULL;
    } else if (vol.sup.fs) {
        ramfs_volume_soltar(vol.sup.priv);
    }
    vol.inf.fs->desmontar(vol.inf.priv, false);
    memset(&vol, 0, sizeof vol);
    return r;
}

static int s_info(void *priv, const char *rel, info_arquivo_t *i)
{
    (void)priv;
    return onde(rel, i) < 0 ? ERRO_NAO_EXISTE : 0;
}

static int s_listar(void *priv, const char *rel, int indice, entrada_pasta_t *e)
{
    (void)priv;
    info_arquivo_t i;
    int o = onde(rel, &i);
    if (o < 0) return ERRO_NAO_EXISTE;
    if (i.tipo != TIPO_PASTA) return ERRO_NAO_E_PASTA;
    info_arquivo_t is;
    bool tem_sup = sup_info(rel, &is) && is.tipo == TIPO_PASTA;
    info_arquivo_t ii;
    bool usa_inf = inf_visivel(rel) && !(tem_sup && opaca(rel)) && inf_info(rel, &ii) && ii.tipo == TIPO_PASTA;
    if (!tem_sup) return vol.inf.fs->listar(vol.inf.priv, rel, indice, e);   /* lo normal: nada cambiado aquí */
    int k = 0;
    entrada_pasta_t x;
    for (int j = 0; vol.sup.fs->listar(vol.sup.priv, rel, j, &x) == 1; j++) {
        if (e_marca(x.nome)) continue;
        if (k++ == indice) { *e = x; return 1; }
    }
    if (!usa_inf) return 0;
    for (int j = 0; vol.inf.fs->listar(vol.inf.priv, rel, j, &x) == 1; j++) {
        char c[CAMINHO_MAX];
        if (!juntar(c, rel, x.nome)) continue;
        if (sup_info(c, NULL) || marcado(rel, x.nome)) continue;
        if (k++ == indice) { *e = x; return 1; }
    }
    return 0;
}

static int s_abrir(void *priv, const char *rel, int modo, void *arq, uint32_t *tam)
{
    (void)priv;
    arq_t *a = arq;
    bool escrita = (modo & (ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR | ABRIR_ACRESCENTAR)) != 0;
    if (tem_marca(rel)) return escrita ? ERRO_ACESSO : ERRO_NAO_EXISTE;
    info_arquivo_t i;
    int o = onde(rel, &i);
    if (o == 0 && !escrita) {
        a->capa = 0;
        return vol.inf.fs->abrir(vol.inf.priv, rel, modo, a->sub, tam);
    }
    if (o == 0) {
        if (i.tipo == TIPO_PASTA) return ERRO_E_PASTA;
        int r = subir(rel);
        if (r < 0) return r;
    } else if (o < 0) {
        if (!(modo & ABRIR_CRIAR)) return ERRO_NAO_EXISTE;
        char pasta[CAMINHO_MAX];
        pasta_de(rel, pasta);
        info_arquivo_t ip;
        if (*pasta && (onde(pasta, &ip) < 0 || ip.tipo != TIPO_PASTA)) return ERRO_NAO_EXISTE;
        int r = garantir_pasta(pasta);
        if (r < 0) return r;
        tirar_marca(pasta, ultimo(rel));
    }
    a->capa = 1;
    return vol.sup.fs->abrir(vol.sup.priv, rel, modo, a->sub, tam);
}

#define CAPA(a) (((arq_t *)(a))->capa ? &vol.sup : &vol.inf)

static int s_ler(void *priv, void *arq, uint32_t pos, void *buf, uint32_t n)
{
    (void)priv;
    capa_t *c = CAPA(arq);
    return c->fs->ler(c->priv, ((arq_t *)arq)->sub, pos, buf, n);
}

static int s_escrever(void *priv, void *arq, uint32_t pos, const void *buf, uint32_t n, uint32_t *tam)
{
    (void)priv;
    if (!((arq_t *)arq)->capa) return ERRO_SO_LEITURA;
    return vol.sup.fs->escrever(vol.sup.priv, ((arq_t *)arq)->sub, pos, buf, n, tam);
}

static int s_truncar(void *priv, void *arq, uint32_t tam)
{
    (void)priv;
    if (!((arq_t *)arq)->capa) return ERRO_SO_LEITURA;
    return vol.sup.fs->truncar(vol.sup.priv, ((arq_t *)arq)->sub, tam);
}

static void s_fechar(void *priv, void *arq)
{
    (void)priv;
    capa_t *c = CAPA(arq);
    c->fs->fechar(c->priv, ((arq_t *)arq)->sub);
}

static int s_sincronizar(void *priv, void *arq)
{
    (void)priv;
    capa_t *c = CAPA(arq);
    return c->fs->sincronizar ? c->fs->sincronizar(c->priv, ((arq_t *)arq)->sub) : 0;
}

static int s_criar_pasta(void *priv, const char *rel)
{
    (void)priv;
    if (!*rel || tem_marca(rel)) return ERRO_INVALIDO;
    if (onde(rel, NULL) >= 0) return ERRO_JA_EXISTE;
    char pasta[CAMINHO_MAX];
    pasta_de(rel, pasta);
    info_arquivo_t ip;
    if (*pasta && (onde(pasta, &ip) < 0 || ip.tipo != TIPO_PASTA)) return ERRO_NAO_EXISTE;
    int r = garantir_pasta(pasta);
    if (r < 0) return r;
    bool tinha = marcado(pasta, ultimo(rel));
    r = vol.sup.fs->criar_pasta(vol.sup.priv, rel);
    if (r < 0) return r;
    tirar_marca(pasta, ultimo(rel));
    if (tinha && inf_info(rel, NULL)) {   /* una carpeta de la inferior, borrada y vuelta a crear: opaca */
        char m[CAMINHO_MAX];
        if (juntar(m, rel, OPACA)) criar_vazio(m);
    }
    return 0;
}

/* ¿Está vacía la carpeta, tal como se ve? */
static bool vazia(const char *rel)
{
    entrada_pasta_t e;
    for (int i = 0; s_listar(NULL, rel, i, &e) == 1; i++)
        if (txt_compara(e.nome, ".") && txt_compara(e.nome, "..") && txt_compara(e.nome, "lost+found")) return false;
    return true;
}

static int s_remover_pasta(void *priv, const char *rel)
{
    (void)priv;
    if (!*rel) return ERRO_ACESSO;
    info_arquivo_t i;
    int o = onde(rel, &i);
    if (o < 0) return ERRO_NAO_EXISTE;
    if (i.tipo != TIPO_PASTA) return ERRO_NAO_E_PASTA;
    if (!vazia(rel)) return ERRO_NAO_VAZIA;
    bool na_inf = inf_visivel(rel) && inf_info(rel, NULL);
    if (o == 1) {   /* fuera las marcas de dentro y la carpeta de arriba */
        entrada_pasta_t e;
        char c[CAMINHO_MAX];
        while (vol.sup.fs->listar(vol.sup.priv, rel, 0, &e) == 1 && e_marca(e.nome) && juntar(c, rel, e.nome))
            if (vol.sup.fs->apagar(vol.sup.priv, c) < 0) break;
        int r = vol.sup.fs->remover_pasta(vol.sup.priv, rel);
        if (r < 0) return r;
    }
    if (na_inf) {
        char pasta[CAMINHO_MAX];
        pasta_de(rel, pasta);
        return por_marca(pasta, ultimo(rel));
    }
    return 0;
}

static int s_apagar(void *priv, const char *rel)
{
    (void)priv;
    info_arquivo_t i;
    int o = onde(rel, &i);
    if (o < 0) return ERRO_NAO_EXISTE;
    if (i.tipo == TIPO_PASTA) return ERRO_E_PASTA;
    bool na_inf = inf_visivel(rel) && inf_info(rel, NULL);
    if (o == 1) {
        int r = vol.sup.fs->apagar(vol.sup.priv, rel);
        if (r < 0) return r;
    }
    if (na_inf) {
        char pasta[CAMINHO_MAX];
        pasta_de(rel, pasta);
        return por_marca(pasta, ultimo(rel));
    }
    return 0;
}

static int s_renomear(void *priv, const char *de, const char *para)
{
    (void)priv;
    if (!*de || !*para || tem_marca(para)) return ERRO_INVALIDO;
    info_arquivo_t i;
    int o = onde(de, &i);
    if (o < 0) return ERRO_NAO_EXISTE;
    bool na_inf = inf_visivel(de) && inf_info(de, NULL);
    if (i.tipo == TIPO_PASTA && na_inf) return ERRO_NAO_SUPORTADO;   /* como EXDEV */
    if (o == 0) {
        int r = subir(de);
        if (r < 0) return r;
    }
    char pp[CAMINHO_MAX];
    pasta_de(para, pp);
    info_arquivo_t ip;
    if (*pp && (onde(pp, &ip) < 0 || ip.tipo != TIPO_PASTA)) return ERRO_NAO_EXISTE;
    int r = garantir_pasta(pp);
    if (r < 0) return r;
    tirar_marca(pp, ultimo(para));
    r = vol.sup.fs->renomear(vol.sup.priv, de, para);
    if (r < 0) return r;
    if (na_inf) {
        char pd[CAMINHO_MAX];
        pasta_de(de, pd);
        return por_marca(pd, ultimo(de));
    }
    return 0;
}

static void s_espaco(void *priv, uint32_t *total, uint32_t *livre)
{
    (void)priv;
    if (vol.sup.fs->espaco) vol.sup.fs->espaco(vol.sup.priv, total, livre);
    else *total = *livre = 0;
}

static int s_datas(void *priv, const char *rel, datas_arquivo_t *d)
{
    (void)priv;
    int o = onde(rel, NULL);
    if (o < 0) return ERRO_NAO_EXISTE;
    capa_t *c = o ? &vol.sup : &vol.inf;
    return c->fs->datas ? c->fs->datas(c->priv, rel, d) : ERRO_NAO_SUPORTADO;
}

static int s_meta(void *priv, const char *rel, meta_t *m)
{
    (void)priv;
    int o = onde(rel, NULL);
    if (o < 0) return ERRO_NAO_EXISTE;
    capa_t *c = o ? &vol.sup : &vol.inf;
    return c->fs->meta ? c->fs->meta(c->priv, rel, m) : ERRO_NAO_SUPORTADO;
}

/* Cambiar dueño, permisos o fechas de algo de la inferior: primero se sube. */
static int subir_qualquer(const char *rel)
{
    info_arquivo_t i;
    int o = onde(rel, &i);
    if (o < 0) return ERRO_NAO_EXISTE;
    if (o == 1) return 0;
    return i.tipo == TIPO_PASTA ? garantir_pasta(rel) : subir(rel);
}

static int s_fixar_meta(void *priv, const char *rel, int32_t uid, int32_t gid, int32_t modo)
{
    (void)priv;
    int r = subir_qualquer(rel);
    return r < 0 ? r : vol.sup.fs->fixar_meta(vol.sup.priv, rel, uid, gid, modo);
}

static int s_fixar_datas(void *priv, const char *rel, uint32_t mod, uint32_t acesso)
{
    (void)priv;
    int r = subir_qualquer(rel);
    return r < 0 ? r : vol.sup.fs->fixar_datas(vol.sup.priv, rel, mod, acesso);
}

sistema_arquivos_t fs_sobreposicao = {
    .nome = "sobreposicao",
    .montar = s_montar, .desmontar = s_desmontar, .info = s_info, .listar = s_listar,
    .abrir = s_abrir, .ler = s_ler, .escrever = s_escrever, .truncar = s_truncar, .fechar = s_fechar,
    .sincronizar = s_sincronizar, .criar_pasta = s_criar_pasta, .remover_pasta = s_remover_pasta,
    .apagar = s_apagar, .renomear = s_renomear, .espaco = s_espaco, .datas = s_datas, .meta = s_meta,
    .fixar_meta = s_fixar_meta, .fixar_datas = s_fixar_datas,
};

void sobreposicao_preparar(disco_t *persistente)
{
    pedido_sup = persistente;
    uint32_t a = fs_ext2.tam_arq, b = fs_ramfs.tam_arq;
    fs_sobreposicao.tam_arq = (uint32_t)sizeof(arq_t) + (a > b ? a : b);
}

bool sobreposicao_persistente(void) { return vol.disco_sup != NULL; }
