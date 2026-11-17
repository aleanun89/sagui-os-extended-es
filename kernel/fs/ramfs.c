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
    /* v8.5.4.2: el tope (la capa de la live en la RAM, /tmp). Se cuenta la memoria que tienen los
     * archivos escritos por este volumen (la capacidad de cada nodo); lo que traía el initramfs no
     * cuenta (no es memoria nueva). 0 = sin tope. */
    uint64_t limite, usado_bytes;
    bool    avisado;
} ramfs_vol_t;

static ramfs_vol_t vols[MAX_RAMFS];

/* v8.5.4.2: el tope por defecto, en % de la RAM (kconfig capa_ram_pct; 50 de serie, como el tmpfs de
 * Linux), y una reserva que nunca se toca: con poca memoria libre, ENOSPC antes de quedarse sin ella. */
static uint32_t pct_limite = 50;
void ramfs_fixar_pct(uint32_t pct) { if (pct >= 5 && pct <= 95) pct_limite = pct; }

static uint64_t limite_padrao(void)
{
    uint64_t total = ((uint64_t)memfisica_geridas() + memfisica_geridas_alta()) * 4096u;
    return total * pct_limite / 100;
}

static uint64_t reserva(void)
{
    uint64_t r = (uint64_t)memfisica_geridas() * 4096u / 8;   /* 1/8 de la memoria baja, y al menos 4 MB */
    return r < 4u * 1024 * 1024 ? 4u * 1024 * 1024 : r;
}

static uint32_t cap_futura(const no_t *x, uint32_t tam)
{
    if (x->dados && x->capacidade >= tam) return x->capacidade;
    uint32_t nova = x->capacidade ? x->capacidade : 256;
    while (nova < tam && nova < 0x80000000u) nova *= 2;
    return nova;
}

/* ¿Puede el archivo x crecer hasta "tam" bytes? Si no, ERRO_SEM_ESPACO y un aviso (una vez). */
static int cabe(ramfs_vol_t *v, const no_t *x, uint32_t tam)
{
    uint32_t antes = x->dados ? x->capacidade : 0, depois = cap_futura(x, tam);
    if (depois <= antes) return 0;
    uint64_t mais = depois - antes;
    bool sem_tope = v->limite && v->usado_bytes + mais > v->limite;
    bool sem_ram = (uint64_t)memfisica_livres() * 4096u < mais + reserva();
    if (!sem_tope && !sem_ram) return 0;
    if (!v->avisado) {
        v->avisado = true;
        klog("[RAMFS] %s sin sitio (%s): usado %u KB, tope %u KB\n", v->d.nome, sem_tope ? "tope" : "poca memoria libre",
             (uint32_t)(v->usado_bytes / 1024), (uint32_t)(v->limite / 1024));
        console_cor(COR_AMARELO, COR_PRETO);
        kprintf("[aviso] No queda sitio en la RAM para %s (%u MB usados%s): no se puede escribir más ahí. Borra algo\n"
                "o, en la live, usa una capa persistente (SAGUI-PERSIST) o instala SaguiOS.\n", v->d.nome,
                (uint32_t)(v->usado_bytes >> 20), sem_tope ? "" : ", queda poca memoria");
        console_cor(COR_CINZA, COR_PRETO);
    }
    return ERRO_SEM_ESPACO;
}

static void contar(ramfs_vol_t *v, uint32_t antes, const no_t *x)
{
    uint32_t depois = x->dados ? x->capacidade : 0;
    if (depois >= antes) v->usado_bytes += depois - antes;
    else v->usado_bytes = v->usado_bytes > antes - depois ? v->usado_bytes - (antes - depois) : 0;
    if (v->avisado && v->limite && v->usado_bytes + v->limite / 16 < v->limite) v->avisado = false;
}

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
    no_t *x = ((ramfs_arq_t *)arq)->no;
    if (n && pos + n > pos) {
        int c = cabe(V, x, pos + n);
        if (c < 0) { *tam = x->tamanho; return c; }
    }
    uint32_t antes = x->dados ? x->capacidade : 0;
    int r = vfs_escrever(x, pos, buf, n);
    contar(V, antes, x);
    *tam = x->tamanho;
    return r;
}

static int r_truncar(void *priv, void *arq, uint32_t tam)
{
    no_t *x = ((ramfs_arq_t *)arq)->no;
    if (tam > x->tamanho) {
        int c = cabe(V, x, tam);
        if (c < 0) return c;
    }
    uint32_t antes = x->dados ? x->capacidade : 0;
    int r = vfs_redimensionar(x, tam);
    contar(V, antes, x);
    return r;
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
    no_t *x = vfs_achar(c);
    uint32_t cap = x && x->tipo != TIPO_PASTA && x->dados ? x->capacidade : 0;
    int r = vfs_apagar(c);
    if (r == 0 && cap) V->usado_bytes = V->usado_bytes > cap ? V->usado_bytes - cap : 0;
    return r;
}

static int r_renomear(void *priv, const char *de, const char *para)
{
    if (!*de || !*para) return ERRO_ACESSO;
    char a[CAMINHO_MAX], b[CAMINHO_MAX];
    if (!caminho(V, de, a) || !caminho(V, para, b)) return ERRO_INVALIDO;
    /* v8.5.4.2 (revisión): renombrar encima de un archivo lo borra (vfs_renomear): su memoria sale de la
     * cuenta del tope; si no, cada guardado atómico (temporal + renombrar) la iba subiendo hasta un ENOSPC
     * falso. El destino puede ser «carpeta + el mismo nombre» (mover dentro). */
    no_t *x = vfs_achar(a), *d = vfs_achar(b);
    if (x && d && d->tipo == TIPO_PASTA) {
        char c[CAMINHO_MAX];
        size_t n = txt_tamanho(b), k = txt_tamanho(x->nome);
        if (n + 1 + k + 1 <= sizeof c) {
            memcpy(c, b, n);
            if (n > 1) c[n++] = '/';
            memcpy(c + n, x->nome, k + 1);
            d = vfs_achar(c);
        } else d = NULL;
    }
    uint32_t cap = x && d && d != x && d->tipo != TIPO_PASTA && d->dados ? d->capacidade : 0;
    int r = vfs_renomear(a, b);
    if (r == 0 && cap) V->usado_bytes = V->usado_bytes > cap ? V->usado_bytes - cap : 0;
    return r;
}

static void r_espaco(void *priv, uint32_t *total, uint32_t *livre)
{
    /* v8.5.4.2: con tope, el tope y lo que queda de él (sin pasar de la memoria libre menos la reserva). */
    uint64_t ram = (uint64_t)memfisica_livres() * 4096u, res = reserva();
    uint64_t pode = ram > res ? ram - res : 0;
    if (V->limite) {
        uint64_t resto = V->limite > V->usado_bytes ? V->limite - V->usado_bytes : 0;
        *total = (uint32_t)(V->limite / 1024);
        *livre = (uint32_t)((resto < pode ? resto : pode) / 1024);
    } else {
        *livre = (uint32_t)(pode / 1024);
        *total = *livre;
    }
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
    txt_copia_n(v->d.nome, "la capa", sizeof v->d.nome);   /* «la capa» de la live: sale en el aviso */
    v->limite = limite_padrao();
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
    if (!(opcoes & MONTAR_SO_LEITURA)) v->limite = limite_padrao();   /* v8.5.4.2: /tmp con tope */
    v->usado = true;
    int r = montagem_montar_dono(&v->d, ponto, "ramfs", opcoes | MONTAR_DONO, UID_ROOT | GID_ROOT << 16);
    if (r < 0) v->usado = false;
    return r;
}
