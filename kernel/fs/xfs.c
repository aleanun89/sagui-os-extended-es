// SPDX-License-Identifier: GPL-3.0-only
/* xfs.c - el sistema de archivos XFS, en solo lectura (D446, fase 1 de docs/xfs.md).
 *
 * Implementación propia, escrita desde cero con la especificación pública «XFS Algorithms &
 * Data Structures» (la documentación del formato que publica kernel.org); no se copió código
 * de Linux ni de xfsprogs, que son GPL-2.0-only. Solo se usan las constantes y los formatos del
 * disco, que no son código.
 *
 * Todo en el disco va en big-endian, salvo las sumas CRC32C (v5), que se guardan como
 * ~crc32c en little-endian con el campo a cero durante el cálculo.
 *
 * Estructura de un volumen:
 *   - grupos de asignación (AG) de "agblocks" bloques; el primero empieza por el superbloque.
 *     Un número de bloque (xfs_fsblock_t, 64 bits) es grupo << agblklog | bloque, y un número
 *     de inodo es grupo << (agblklog + inopblog) | bloque << inopblog | índice;
 *   - inodos (xfs_dinode) con la bifurcación de datos en uno de tres formatos: local (los
 *     datos dentro del inodo), extents (lista de tramos de 16 bytes) o btree (árbol B+ de
 *     tramos, «bmbt», con la raíz dentro del inodo);
 *   - carpetas en cinco formas: shortform (dentro del inodo), block (un bloque con las
 *     entradas y la tabla de hash al final), leaf (bloques de datos y un bloque de hojas con
 *     los hash), node (árbol B+ de hash, «da-btree», encima de las hojas) y lo mismo con la
 *     lista de bloques de la carpeta en un bmbt. Los bloques de hojas y nodos están a partir
 *     de los 32 GB del espacio de la carpeta (XFS_DIR2_LEAF_OFFSET);
 *   - el diario, al que en esta fase solo se le mira si quedó limpio.
 *
 * Pensado para la fase 2 (escritura):
 *   - todo metadato pasa por una caché de búferes propia (buf_*) con referencias y marca de
 *     sucio, encima de disco_ler (y por tanto de cache_disco.c en los discos internos). En la
 *     fase 1 nada se ensucia; la escritura tendrá que fijar («pin») los búferes de una
 *     transacción hasta que el diario los tenga, y resolver los solapes entre búferes de
 *     tamaños distintos (aquí no importan porque nada cambia);
 *   - un solo recorrido de árbol B+ (arvore_descer) con una tabla de funciones por tipo de
 *     árbol (bmbt, da-btree); bnobt, cntbt, inobt y finobt serán otras tablas, y la inserción
 *     y la división de nodos se escribirán una vez para todos;
 *   - los números de bloque y de sector son siempre de 64 bits; solo se pasan a 32 al llamar
 *     a disco_ler, y ahí se comprueba que caben (un volumen de más de 2 TB da error de E/S en
 *     vez de leer otro sitio);
 *   - la suma CRC32C es la de crc32c.c, que también servirá para calcular las sumas al escribir.
 *
 * Límites de esta fase: solo lectura siempre (cualquier escritura da ERRO_SO_LEITURA, el EROFS
 * de SaguiOS); el diario no se reproduce (con un apagado incorrecto se monta igual, con aviso);
 * los atributos extendidos se ignoran; el VFS cuenta posiciones en 32 bits, así que un archivo
 * de 4 GB o más se lee hasta 4 GB - 1; los archivos del volumen de tiempo real no se leen. */
#include "xfs.h"
#include "montagem.h"
#include "console.h"
#include "crc32c.h"
#include "memoria.h"
#include "texto.h"

/* ------------------------------------------------------------ constantes del formato */

#define SB_MAGIA          0x58465342u   /* "XFSB" */
#define INODE_MAGIA       0x494Eu       /* "IN" */
#define BMAP_MAGIA        0x424D4150u   /* "BMAP": bloque del bmbt (v4) */
#define BMA3_MAGIA        0x424D4133u   /* "BMA3": bloque del bmbt (v5) */
#define DIR_BLOCO_V4      0x58443242u   /* "XD2B": carpeta de un solo bloque */
#define DIR_BLOCO_V5      0x58444233u   /* "XDB3" */
#define DIR_DADOS_V4      0x58443244u   /* "XD2D": bloque de datos de una carpeta leaf/node */
#define DIR_DADOS_V5      0x58444433u   /* "XDD3" */
#define FOLHA1_V4         0xD2F1u       /* bloque de hojas único (forma leaf) */
#define FOLHA1_V5         0x3DF1u
#define FOLHAN_V4         0xD2FFu       /* hoja de un da-btree (forma node) */
#define FOLHAN_V5         0x3DFFu
#define NO_DA_V4          0xFEBEu       /* nodo interior del da-btree */
#define NO_DA_V5          0x3EBEu
#define LIGACAO_MAGIA     0x58534C4Du   /* "XSLM": cabecera de los bloques de un enlace simbólico (v5) */
#define DIARIO_MAGIA      0xFEEDBABEu   /* cabecera de un registro del diario */

/* sb_versionnum: la versión en los 4 bits bajos y estas marcas. */
#define VER_ATTR        0x0010
#define VER_NLINK       0x0020
#define VER_QUOTA       0x0040
#define VER_ALIGN       0x0080
#define VER_DALIGN      0x0100
#define VER_SHARED      0x0200
#define VER_LOGV2       0x0400
#define VER_SECTOR      0x0800
#define VER_EXTFLG      0x1000   /* hay extents sin escribir */
#define VER_DIRV2       0x2000   /* carpetas versión 2 (las únicas que se admiten) */
#define VER_BORG        0x4000   /* nombres sin distinguir mayúsculas (ASCII): otro hash */
#define VER_MOREBITS    0x8000   /* sb_features2 vale */

/* sb_features2 (v4): las que no cambian cómo se lee. */
#define F2_LAZYSBCOUNT  0x0002
#define F2_ATTR2        0x0008
#define F2_PROJID32     0x0080
#define F2_CRC          0x0100
#define F2_FTYPE        0x0200
#define F2_CONHECIDAS   (F2_LAZYSBCOUNT | F2_ATTR2 | F2_PROJID32 | F2_FTYPE)

/* sb_features_incompat (v5). */
#define INC_FTYPE       0x0001
#define INC_SPINODES    0x0002
#define INC_META_UUID   0x0004
#define INC_BIGTIME     0x0008
#define INC_NEEDSREPAIR 0x0010
#define INC_NREXT64     0x0020
#define INC_EXCHRANGE   0x0040
#define INC_PARENT      0x0080
#define INC_METADIR     0x0100
#define INC_LEGIVEIS    (INC_FTYPE | INC_SPINODES | INC_META_UUID | INC_BIGTIME | INC_NREXT64)

/* di_format */
#define FMT_DEV         0
#define FMT_LOCAL       1
#define FMT_EXTENTS     2
#define FMT_BTREE       3

/* di_flags y di_flags2 */
#define DIFLAG_REALTIME 0x0001
#define DIFLAG2_BIGTIME 0x0008
#define DIFLAG2_NREXT64 0x0010

#define MODO_TIPO       0xF000
#define MODO_PASTA      0x4000
#define MODO_ARQUIVO    0x8000
#define MODO_LIGACAO    0xA000

#define DIR_FOLHA_DESL  (1ull << 35)   /* XFS_DIR2_LEAF_OFFSET: 32 GB del espacio de la carpeta */
#define NULO64          0xFFFFFFFFFFFFFFFFull
#define ALTURA_MAX      9              /* niveles de un árbol B+ como mucho (más = disco estropeado) */
#define LIGACAO_MAX     1024           /* destino de un enlace simbólico (XFS_SYMLINK_MAXLEN) */
#define SALTOS_MAX      8              /* enlaces simbólicos seguidos en una ruta */
#define CAM_MAX         (LIGACAO_MAX + 4 * CAMINHO_MAX + 8)
#define NBUF            48             /* búferes de metadatos por volumen */
#define JANELA          128            /* sectores de la ventana de lectura anticipada (64 KB) */
#define DIRETO_MIN      (32u * 1024u)  /* desde aquí, la lectura va directa al destino */
#define DIRETO_MAX      256            /* sectores por llamada a disco_ler (128 KB) */
#define DIARIO_BUSCA    16384          /* sectores que se miran hacia atrás buscando el último registro */

/* ------------------------------------------------------------ lectura de enteros del disco */

static inline uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static inline uint32_t be32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static inline uint64_t be64(const uint8_t *p) { return (uint64_t)be32(p) << 32 | be32(p + 4); }
static inline uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Un valor de 64 bits para klog (que no tiene %llu): las dos mitades en hexadecimal. */
#define X64(x) (uint32_t)((uint64_t)(x) >> 32), (uint32_t)(x)

/* ------------------------------------------------------------ el volumen */

/* Un búfer de metadatos: "tam" bytes desde el sector "daddr" del volumen. */
typedef struct {
    uint64_t daddr;
    uint32_t tam, cap;
    uint32_t refs;          /* quién lo está usando ahora: no se puede reciclar */
    uint32_t uso;           /* reloj del LRU */
    uint32_t verificado;    /* magia del tipo ya comprobada (y su suma): 0 = sin comprobar */
    bool     valido;
    bool     sujo;          /* fase 2: cambiado y sin grabar (en la fase 1, siempre falso) */
    uint8_t *dados;
} xbuf_t;

typedef struct {
    disco_t *d;
    bool     v5;
    uint32_t versao, features2, incompat, ro_compat;
    uint32_t bs, blocklog;              /* bloque del sistema de archivos */
    uint32_t sectsize;
    uint32_t isz, inodelog, inopblog;   /* inodos */
    uint32_t agblocks, agblklog, agcount;
    uint64_t dblocks, fdblocks, rblocks;
    uint64_t logstart;
    uint32_t logblocks;
    uint64_t raiz;                      /* número del inodo raíz */
    uint32_t dirbs, dirblklog;          /* bloque de carpeta: bs << dirblklog */
    bool     ftype;                     /* las entradas de carpeta llevan el tipo (un byte) */
    uint8_t  uuid[16];                  /* el que llevan los metadatos v5 (sb_meta_uuid si INC_META_UUID) */
    char     rotulo[16];
    uint32_t bmbt_cab;                  /* cabecera de un bloque del bmbt: 24 (v4) o 72 (v5) */
    uint32_t da_cab;                    /* cabecera de hojas y nodos del da-btree: 16 o 64 */
    uint32_t dados_cab;                 /* cabecera de un bloque de datos de carpeta: 16 o 64 */
    xbuf_t   buf[NBUF];
    uint32_t relogio;
    uint32_t erros_soma;                /* sumas CRC32C que fallaron desde el montaje */
    /* el listado sigue donde paró (listar(i + 1) después de listar(i)) */
    uint64_t lst_ino, lst_pos;
    int      lst_indice;
    /* ventana de lectura anticipada de los datos de archivo */
    uint8_t *ra;
    uint64_t ra_setor;
    uint32_t ra_n;
} xvol_t;

/* Un inodo leído: los bytes siguen en su búfer (con una referencia, hasta inode_soltar). */
typedef struct {
    uint64_t       ino;
    xbuf_t        *b;
    const uint8_t *p;
    uint16_t       modo, flags;
    uint8_t        formato, versao;
    uint64_t       tam, nextents, flags2;
    const uint8_t *fork;               /* bifurcación de datos */
    uint32_t       fork_tam;
} inode_t;

#define EH_PASTA(i)   (((i)->modo & MODO_TIPO) == MODO_PASTA)
#define EH_ARQUIVO(i) (((i)->modo & MODO_TIPO) == MODO_ARQUIVO)
#define EH_LIGACAO(i) (((i)->modo & MODO_TIPO) == MODO_LIGACAO)

static int corrompido(xvol_t *v, const char *que, uint64_t onde)
{
    klog("[XFS] %s: %s estropeado (%x%08x): error de E/S\n", v->d->nome, que, X64(onde));
    return ERRO_ACESSO;
}

/* ------------------------------------------------------------ CRC32C (v5) */

/* La suma de un objeto de "tam" bytes con el campo de la suma en "desl": se calcula con ese
 * campo a cero y se guarda invertida, en little-endian. */
static uint32_t soma_calcular(const uint8_t *b, uint32_t tam, uint32_t desl)
{
    static const uint8_t zero[4];
    uint32_t c = crc32c(0xFFFFFFFFu, b, desl);
    c = crc32c(c, zero, 4);
    c = crc32c(c, b + desl + 4, tam - desl - 4);
    return ~c;
}

static bool soma_ok(xvol_t *v, const uint8_t *b, uint32_t tam, uint32_t desl, const char *que, uint64_t onde)
{
    if (le32(b + desl) == soma_calcular(b, tam, desl)) return true;
    v->erros_soma++;
    klog("[XFS] %s: suma CRC32C mal en %s (%x%08x): no se lee\n", v->d->nome, que, X64(onde));
    return false;
}

/* ------------------------------------------------------------ direcciones */

/* Un bloque válido está dentro de su grupo y del volumen (el último grupo puede ser más corto). */
static bool fsb_valido(xvol_t *v, uint64_t fsb)
{
    uint64_t ag = fsb >> v->agblklog, bloco = fsb & ((1ull << v->agblklog) - 1);
    return ag < v->agcount && bloco < v->agblocks && ag * v->agblocks + bloco < v->dblocks;
}

/* Un tramo [fsb, fsb + n) entero dentro de un grupo. */
static bool tramo_valido(xvol_t *v, uint64_t fsb, uint64_t n)
{
    if (!n || !fsb_valido(v, fsb)) return false;
    uint64_t ag = fsb >> v->agblklog, bloco = fsb & ((1ull << v->agblklog) - 1);
    return n <= v->agblocks - bloco && ag * v->agblocks + bloco + n <= v->dblocks;
}

/* Sector de 512 bytes (del volumen) donde empieza el bloque "fsb". */
static uint64_t fsb_setor(xvol_t *v, uint64_t fsb)
{
    uint64_t ag = fsb >> v->agblklog, bloco = fsb & ((1ull << v->agblklog) - 1);
    return (ag * v->agblocks + bloco) << (v->blocklog - 9);
}

/* disco_ler con un sector de 64 bits: lo que no cabe en 32 bits es un error, nunca otro sitio. */
static int ler_setores(xvol_t *v, uint64_t setor, uint32_t n, void *dst)
{
    if (setor > 0xFFFFFFFFull || n > 0xFFFFFFFFu - (uint32_t)setor) return ERRO_ACESSO;
    return disco_ler(v->d, (uint32_t)setor, n, dst) < 0 ? ERRO_ACESSO : 0;
}

/* ------------------------------------------------------------ búferes de metadatos */

static void buf_soltar(xbuf_t *b) { if (b && b->refs) b->refs--; }
static void buf_segurar(xbuf_t *b) { b->refs++; }

static xbuf_t *buf_vitima(xvol_t *v, uint32_t tam)
{
    xbuf_t *m = NULL;
    for (int i = 0; i < NBUF; i++) {
        xbuf_t *b = &v->buf[i];
        if (b->refs || b->sujo) continue;
        if (!b->valido) { m = b; break; }
        if (!m || b->uso < m->uso) m = b;
    }
    if (!m) return NULL;
    if (m->cap < tam) {
        if (m->dados) kfree(m->dados);
        m->dados = kmalloc(tam);
        m->cap = m->dados ? tam : 0;
        if (!m->dados) return NULL;
    }
    m->valido = false;
    m->verificado = 0;
    return m;
}

/* "nb" bloques de "tam_b" bytes, cada uno en el sector setores[i] (un bloque de carpeta puede
 * estar repartido en varios tramos del disco). El búfer se identifica por el primer sector y el
 * tamaño total. Devuelve 0 y el búfer con una referencia, o un error. */
static int buf_ler_partes(xvol_t *v, const uint64_t *setores, uint32_t nb, uint32_t tam_b, xbuf_t **saida)
{
    uint32_t tam = nb * tam_b;
    *saida = NULL;
    for (int i = 0; i < NBUF; i++) {
        xbuf_t *b = &v->buf[i];
        if (b->valido && b->daddr == setores[0] && b->tam == tam) {
            b->refs++;
            b->uso = ++v->relogio;
            *saida = b;
            return 0;
        }
    }
    xbuf_t *b = buf_vitima(v, tam);
    if (!b) return ERRO_SEM_MEMORIA;
    uint32_t spb = tam_b / 512;
    for (uint32_t i = 0; i < nb; ) {
        uint32_t k = 1;   /* bloques seguidos en el disco: una sola lectura */
        while (i + k < nb && setores[i + k] == setores[i] + (uint64_t)k * spb) k++;
        if (ler_setores(v, setores[i], k * spb, b->dados + i * tam_b) < 0) {
            klog("[XFS] %s: error de lectura en el sector %x%08x\n", v->d->nome, X64(setores[i]));
            return ERRO_ACESSO;
        }
        i += k;
    }
    b->daddr = setores[0];
    b->tam = tam;
    b->valido = true;
    b->refs = 1;
    b->uso = ++v->relogio;
    *saida = b;
    return 0;
}

static int buf_ler(xvol_t *v, uint64_t setor, uint32_t tam, xbuf_t **saida)
{
    return buf_ler_partes(v, &setor, 1, tam, saida);
}

static void buf_liberar_todos(xvol_t *v)
{
    for (int i = 0; i < NBUF; i++) {
        if (v->buf[i].dados) kfree(v->buf[i].dados);
        v->buf[i].dados = NULL;
        v->buf[i].valido = false;
        v->buf[i].cap = 0;
    }
}

/* ------------------------------------------------------------ superbloque */

static bool potencia2(uint32_t x) { return x && !(x & (x - 1)); }
static uint32_t log2_32(uint32_t x) { uint32_t l = 0; while (x > 1) { x >>= 1; l++; } return l; }

/* Comprueba el superbloque (los 512 primeros bytes bastan). XFS_SIM, XFS_RECUSADO (con el
 * motivo, corto: cabe en el «formato» de 40 bytes del disco) o XFS_NAO. Las funciones
 * incompatibles desconocidas (needsrepair, exchrange, parent, metadir y lo que venga) no se
 * montan: un disco así se leería mal. */
static int validar_sb(const uint8_t *s, char *motivo, size_t max)
{
    if (be32(s) != SB_MAGIA) return XFS_NAO;
    uint32_t bs = be32(s + 4), versao = be16(s + 100) & 0xF, vnum = be16(s + 100);
    uint32_t sect = be16(s + 102), isz = be16(s + 104), inopb = be16(s + 106);
    uint8_t blocklog = s[120], sectlog = s[121], inodelog = s[122], inopblog = s[123], agblklog = s[124];
    uint32_t agblocks = be32(s + 84), agcount = be32(s + 88);
    uint8_t dirblklog = s[192];
    if (versao != 4 && versao != 5) { snformatar(motivo, max, "XFS v%u: versión no soportada", versao); return XFS_RECUSADO; }
    if (!potencia2(bs) || bs < 512 || bs > 65536 || log2_32(bs) != blocklog ||
        !potencia2(sect) || sect < 512 || sect > 32768 || log2_32(sect) != sectlog || sect > bs ||
        !potencia2(isz) || isz < 256 || isz > 2048 || log2_32(isz) != inodelog || isz > bs ||
        inopb != bs / isz || inopblog != blocklog - inodelog ||
        !agblocks || !agcount || agblklog > 31 || (1ull << agblklog) < agblocks ||
        (agblklog && (1ull << (agblklog - 1)) >= agblocks) ||
        blocklog + dirblklog > 16 || !be64(s + 56) || !be64(s + 8)) {
        snformatar(motivo, max, "XFS con la geometría estropeada");
        return XFS_RECUSADO;
    }
    if (s[126]) { snformatar(motivo, max, "XFS a medio formatear"); return XFS_RECUSADO; }
    if (versao == 4) {
        if (!(vnum & VER_DIRV2)) { snformatar(motivo, max, "XFS con carpetas v1: no soportado"); return XFS_RECUSADO; }
        if (vnum & VER_SHARED) { snformatar(motivo, max, "XFS compartido: no soportado"); return XFS_RECUSADO; }
        if (vnum & VER_BORG) { snformatar(motivo, max, "XFS ascii-ci: no soportado"); return XFS_RECUSADO; }
        uint32_t f2 = (vnum & VER_MOREBITS) ? (be32(s + 200) | be32(s + 204)) : 0;
        if (f2 & ~(uint32_t)F2_CONHECIDAS) {
            snformatar(motivo, max, "XFS: funciones desconocidas 0x%x", f2 & ~(uint32_t)F2_CONHECIDAS);
            return XFS_RECUSADO;
        }
    } else {
        uint32_t inc = be32(s + 216);
        if (isz < 512) { snformatar(motivo, max, "XFS v5 con inodos de %u bytes", isz); return XFS_RECUSADO; }
        if (inc & ~(uint32_t)INC_LEGIVEIS) {
            uint32_t x = inc & ~(uint32_t)INC_LEGIVEIS;
            if (x == INC_NEEDSREPAIR) snformatar(motivo, max, "XFS: needsrepair (usa xfs_repair)");
            else snformatar(motivo, max, "XFS: funciones desconocidas 0x%x", x);
            return XFS_RECUSADO;
        }
    }
    return XFS_SIM;
}

int xfs_reconhecer(const uint8_t *s0, char *motivo, size_t max_motivo, char *rotulo, size_t max_rotulo)
{
    char m[48];
    int r = validar_sb(s0, m, sizeof m);
    if (r == XFS_NAO) return r;
    if (motivo && max_motivo) txt_copia_n(motivo, r == XFS_RECUSADO ? m : "", max_motivo);
    if (rotulo && max_rotulo) {
        size_t n = 0;
        while (n < 12 && n + 1 < max_rotulo && s0[108 + n]) { rotulo[n] = (char)s0[108 + n]; n++; }
        while (n && rotulo[n - 1] == ' ') n--;
        rotulo[n] = 0;
    }
    return r;
}

/* ------------------------------------------------------------ inodos */

static void inode_soltar(inode_t *i) { if (i->b) buf_soltar(i->b); i->b = NULL; }

static int inode_ler(xvol_t *v, uint64_t ino, inode_t *i)
{
    memset(i, 0, sizeof *i);
    uint64_t ag = ino >> (v->agblklog + v->inopblog);
    uint64_t bloco = (ino >> v->inopblog) & ((1ull << v->agblklog) - 1);
    uint32_t idx = (uint32_t)(ino & ((1u << v->inopblog) - 1));
    if (!ino || ag >= v->agcount || bloco >= v->agblocks) return corrompido(v, "número de inodo", ino);
    xbuf_t *b;
    int r = buf_ler(v, fsb_setor(v, ag << v->agblklog | bloco), v->bs, &b);
    if (r < 0) return r;
    const uint8_t *p = b->dados + idx * v->isz;
    uint8_t versao = p[4];
    if (be16(p) != INODE_MAGIA || (v->v5 ? versao != 3 : (versao != 1 && versao != 2))) {
        buf_soltar(b);
        return corrompido(v, "inodo", ino);
    }
    if (v->v5) {
        if (!soma_ok(v, p, v->isz, 100, "inodo", ino)) { buf_soltar(b); return ERRO_ACESSO; }
        if (be64(p + 152) != ino || memcmp(p + 160, v->uuid, 16) != 0) { buf_soltar(b); return corrompido(v, "inodo (número o uuid)", ino); }
    }
    i->ino = ino;
    i->b = b;
    i->p = p;
    i->versao = versao;
    i->modo = be16(p + 2);
    i->formato = p[5];
    i->tam = be64(p + 56);
    i->flags = be16(p + 90);
    i->flags2 = versao == 3 ? be64(p + 120) : 0;
    i->nextents = (i->flags2 & DIFLAG2_NREXT64) ? be64(p + 24) : be32(p + 76);
    uint32_t nucleo = versao == 3 ? 176 : 100, lit = v->isz - nucleo, forkoff = (uint32_t)p[82] << 3;
    if (forkoff >= lit) { inode_soltar(i); return corrompido(v, "inodo (forkoff)", ino); }
    i->fork = p + nucleo;
    i->fork_tam = forkoff ? forkoff : lit;
    /* el formato tiene que encajar con el tipo */
    bool ok;
    switch (i->modo & MODO_TIPO) {
    case MODO_PASTA:   ok = i->formato == FMT_LOCAL || i->formato == FMT_EXTENTS || i->formato == FMT_BTREE; break;
    case MODO_ARQUIVO: ok = i->formato == FMT_EXTENTS || i->formato == FMT_BTREE; break;
    case MODO_LIGACAO: ok = i->formato == FMT_LOCAL || i->formato == FMT_EXTENTS || i->formato == FMT_BTREE; break;
    default:           ok = true;   /* dispositivos, tuberías y sockets: no se leen */
    }
    if (!ok || (i->formato == FMT_LOCAL && i->tam > i->fork_tam) ||
        (i->formato == FMT_EXTENTS && i->nextents > i->fork_tam / 16)) {
        inode_soltar(i);
        return corrompido(v, "inodo (formato)", ino);
    }
    return 0;
}

/* Fechas (segundos UTC). Con bigtime son nanosegundos desde 1901-12-13 (2^31 s antes de 1970). */
static uint32_t data_inode(const inode_t *i, uint32_t desl)
{
    if (i->flags2 & DIFLAG2_BIGTIME) {
        uint64_t s = be64(i->p + desl) / 1000000000ull;
        if (s < 2147483648ull) return 0;
        s -= 2147483648ull;
        return s > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)s;
    }
    int32_t s = (int32_t)be32(i->p + desl);
    return s < 0 ? 0 : (uint32_t)s;
}

/* ------------------------------------------------------------ árboles B+ (recorrido común)
 *
 * Cada tipo de árbol se describe con una tabla. La raíz puede estar en un bloque (da-btree) o
 * dentro del inodo (bmbt): "raiz_inode" se pasa a las funciones de la tabla para que sepan
 * dónde están la cabecera, las claves y los punteros. Las claves de un nodo son la menor del
 * hijo (bmbt, y los árboles del espacio libre) o la mayor (da-btree, por hash). */
typedef struct arvore_tipo {
    const char *nome;
    bool        chave_maxima;
    uint32_t  (*nivel)(xvol_t *v, const uint8_t *b, bool raiz_inode);
    uint32_t  (*nrecs)(xvol_t *v, const uint8_t *b, bool raiz_inode);
    uint32_t  (*maxrecs)(xvol_t *v, uint32_t tam, bool raiz_inode);
    uint64_t  (*chave)(xvol_t *v, const uint8_t *b, uint32_t tam, bool raiz_inode, uint32_t i);
    uint64_t  (*ponteiro)(xvol_t *v, const uint8_t *b, uint32_t tam, bool raiz_inode, uint32_t i);
    /* Lee y comprueba el bloque "ptr", que tiene que ser del nivel "nivel". */
    int       (*ler_no)(xvol_t *v, void *ctx, uint64_t ptr, uint32_t nivel, xbuf_t **saida);
} arvore_tipo_t;

/* Baja desde la raíz hasta la hoja que contiene "chave". La raíz es "rb" (un búfer, o NULL si
 * está dentro del inodo: entonces "raiz"/"raiz_tam"). Devuelve 0 y la hoja con una referencia
 * propia (aunque sea "rb"); ERRO_NAO_EXISTE si la clave es mayor que todas (chave_maxima). */
static int arvore_descer(xvol_t *v, const arvore_tipo_t *t, void *ctx, xbuf_t *rb, const uint8_t *raiz,
                         uint32_t raiz_tam, uint64_t chave, xbuf_t **saida)
{
    const uint8_t *b = rb ? rb->dados : raiz;
    uint32_t tam = rb ? rb->tam : raiz_tam;
    bool ri = rb == NULL;
    xbuf_t *atual = NULL;
    uint32_t nivel = t->nivel(v, b, ri);
    *saida = NULL;
    if (nivel > ALTURA_MAX) return corrompido(v, t->nome, nivel);
    while (nivel > 0) {
        uint32_t n = t->nrecs(v, b, ri);
        if (!n || n > t->maxrecs(v, tam, ri)) { buf_soltar(atual); return corrompido(v, t->nome, n); }
        /* búsqueda binaria del hijo */
        uint32_t k;
        if (t->chave_maxima) {   /* el primero con clave >= chave */
            uint32_t lo = 0, hi = n;
            while (lo < hi) { uint32_t m = lo + (hi - lo) / 2; if (t->chave(v, b, tam, ri, m) >= chave) hi = m; else lo = m + 1; }
            if (lo == n) { buf_soltar(atual); return ERRO_NAO_EXISTE; }
            k = lo;
        } else {                 /* el último con clave <= chave (o el primero) */
            uint32_t lo = 0, hi = n;
            while (lo < hi) { uint32_t m = lo + (hi - lo) / 2; if (t->chave(v, b, tam, ri, m) <= chave) lo = m + 1; else hi = m; }
            k = lo ? lo - 1 : 0;
        }
        xbuf_t *f;
        int r = t->ler_no(v, ctx, t->ponteiro(v, b, tam, ri, k), nivel - 1, &f);
        buf_soltar(atual);
        if (r < 0) return r;
        atual = f;
        b = f->dados; tam = f->tam; ri = false;
        nivel--;
    }
    if (!atual) {
        if (!rb) return corrompido(v, t->nome, 0);   /* una raíz en el inodo no puede ser hoja */
        buf_segurar(rb);
        atual = rb;
    }
    *saida = atual;
    return 0;
}

/* ---- bmbt: el árbol de extents de un inodo (formato btree) */

static uint32_t bmbt_nivel(xvol_t *v, const uint8_t *b, bool ri) { (void)v; return be16(b + (ri ? 0 : 4)); }
static uint32_t bmbt_nrecs(xvol_t *v, const uint8_t *b, bool ri) { (void)v; return be16(b + (ri ? 2 : 6)); }
static uint32_t bmbt_cab(xvol_t *v, bool ri) { return ri ? 4 : v->bmbt_cab; }
static uint32_t bmbt_maxrecs(xvol_t *v, uint32_t tam, bool ri) { return (tam - bmbt_cab(v, ri)) / 16; }
static uint64_t bmbt_chave(xvol_t *v, const uint8_t *b, uint32_t tam, bool ri, uint32_t i)
{
    (void)tam;
    return be64(b + bmbt_cab(v, ri) + i * 8);
}
static uint64_t bmbt_ponteiro(xvol_t *v, const uint8_t *b, uint32_t tam, bool ri, uint32_t i)
{
    return be64(b + bmbt_cab(v, ri) + bmbt_maxrecs(v, tam, ri) * 8 + i * 8);
}
static int bmbt_ler_no(xvol_t *v, void *ctx, uint64_t fsb, uint32_t nivel, xbuf_t **saida)
{
    const inode_t *ino = ctx;
    *saida = NULL;
    if (!fsb_valido(v, fsb)) return corrompido(v, "puntero del bmbt", fsb);
    uint64_t setor = fsb_setor(v, fsb);
    xbuf_t *b;
    int r = buf_ler(v, setor, v->bs, &b);
    if (r < 0) return r;
    const uint8_t *p = b->dados;
    if (b->verificado != BMA3_MAGIA) {
        bool ok = be32(p) == (v->v5 ? BMA3_MAGIA : BMAP_MAGIA);
        if (ok && v->v5) {
            if (!soma_ok(v, p, v->bs, 64, "bloque del bmbt", fsb)) { buf_soltar(b); return ERRO_ACESSO; }
            ok = be64(p + 24) == setor && be64(p + 56) == ino->ino && memcmp(p + 40, v->uuid, 16) == 0;
        }
        if (!ok) { buf_soltar(b); return corrompido(v, "bloque del bmbt", fsb); }
        b->verificado = BMA3_MAGIA;
    }
    if (be16(p + 4) != nivel || be16(p + 6) > bmbt_maxrecs(v, v->bs, false)) { buf_soltar(b); return corrompido(v, "bloque del bmbt (nivel)", fsb); }
    *saida = b;
    return 0;
}
static const arvore_tipo_t arvore_bmbt = {
    "bmbt", false, bmbt_nivel, bmbt_nrecs, bmbt_maxrecs, bmbt_chave, bmbt_ponteiro, bmbt_ler_no,
};

/* ------------------------------------------------------------ mapa de bloques (bmap) */

/* Un registro de extent: 1 bit «sin escribir», 54 de bloque del archivo, 52 de bloque del
 * sistema y 21 de longitud. */
static void ext_ler(const uint8_t *r, xfs_extent_t *e)
{
    uint64_t l0 = be64(r), l1 = be64(r + 8);
    e->tipo = (l0 >> 63) ? XFS_EXT_NAO_ESCRITO : XFS_EXT_DADOS;
    e->ini = (l0 & 0x7FFFFFFFFFFFFFFFull) >> 9;
    e->bloco = (l0 & 0x1FFull) << 43 | l1 >> 21;
    e->n = l1 & 0x1FFFFFull;
}

/* De una lista ordenada de "n" registros: el tramo que contiene "off" (o el hueco que lo
 * contiene, hasta "prox" si no hay más registros en la lista). */
static int ext_procurar(xvol_t *v, const inode_t *ino, const uint8_t *recs, uint32_t n, uint64_t off, uint64_t prox,
                        xfs_extent_t *e)
{
    uint32_t lo = 0, hi = n;
    while (lo < hi) {   /* el primero con ini > off */
        uint32_t m = lo + (hi - lo) / 2;
        xfs_extent_t x;
        ext_ler(recs + m * 16, &x);
        if (x.ini <= off) lo = m + 1; else hi = m;
    }
    if (lo) {
        ext_ler(recs + (lo - 1) * 16, e);
        if (off - e->ini < e->n) {
            if (!tramo_valido(v, e->bloco, e->n)) return corrompido(v, "extent", ino->ino);
            return 0;
        }
    }
    uint64_t fim = prox;
    if (lo < n) { xfs_extent_t x; ext_ler(recs + lo * 16, &x); fim = x.ini; }
    e->tipo = XFS_EXT_BURACO;
    e->ini = off;
    e->n = fim > off ? fim - off : 1;
    e->bloco = 0;
    return 0;
}

/* El tramo del archivo que contiene el bloque "off" (datos, sin escribir o hueco). */
static int bmap(xvol_t *v, inode_t *ino, uint64_t off, xfs_extent_t *e)
{
    if (ino->formato == FMT_EXTENTS) return ext_procurar(v, ino, ino->fork, (uint32_t)ino->nextents, off, NULO64, e);
    if (ino->formato != FMT_BTREE) return corrompido(v, "mapa de un inodo local", ino->ino);
    uint32_t nivel = be16(ino->fork), n = be16(ino->fork + 2);
    if (!nivel || !n || n > bmbt_maxrecs(v, ino->fork_tam, true)) return corrompido(v, "raíz del bmbt", ino->ino);
    xbuf_t *f;
    int r = arvore_descer(v, &arvore_bmbt, ino, NULL, ino->fork, ino->fork_tam, off, &f);
    if (r < 0) return r;
    uint32_t nr = be16(f->dados + 6);
    uint64_t dir = be64(f->dados + 16);   /* hermano derecho: de ahí sale dónde acaba un hueco final */
    uint64_t prox = NULO64;
    const uint8_t *recs = f->dados + v->bmbt_cab;
    xfs_extent_t ult = { 0, 0, 0, 0 };
    if (nr) ext_ler(recs + (nr - 1) * 16, &ult);
    if (nr && off >= ult.ini + ult.n && dir != NULO64) {
        xbuf_t *g;
        if (bmbt_ler_no(v, ino, dir, 0, &g) == 0) {
            if (be16(g->dados + 6)) { xfs_extent_t x; ext_ler(g->dados + v->bmbt_cab, &x); prox = x.ini; }
            buf_soltar(g);
        } else { buf_soltar(f); return ERRO_ACESSO; }
    }
    r = ext_procurar(v, ino, recs, nr, off, prox, e);
    buf_soltar(f);
    return r;
}

/* Lee los "nb" bloques del archivo desde "off" (un bloque de carpeta o del da-btree). 0 = leído;
 * 1 = alguno es un hueco (o no está escrito): no existe, y *prox dice dónde vuelve a haber datos. */
static int ler_logico(xvol_t *v, inode_t *ino, uint64_t off, uint32_t nb, xbuf_t **saida, uint64_t *prox)
{
    uint64_t setores[1u << 7];   /* dirblklog <= 16 - blocklog: como mucho 128 bloques */
    if (nb > sizeof setores / sizeof setores[0]) return corrompido(v, "bloque de carpeta", ino->ino);
    *saida = NULL;
    for (uint32_t i = 0; i < nb; ) {
        xfs_extent_t e;
        int r = bmap(v, ino, off + i, &e);
        if (r < 0) return r;
        if (e.tipo != XFS_EXT_DADOS) {
            if (prox) *prox = e.ini + e.n < e.ini ? NULO64 : e.ini + e.n;
            return 1;
        }
        for (uint64_t k = off + i - e.ini; k < e.n && i < nb; k++, i++) setores[i] = fsb_setor(v, e.bloco + k);
    }
    return buf_ler_partes(v, setores, nb, v->bs, saida);
}

/* ------------------------------------------------------------ carpetas */

/* El hash de los nombres de las carpetas (xfs_da_hashname): 7 bits de rotación por byte. */
static uint32_t rol32(uint32_t x, uint32_t n) { return x << n | x >> (32 - n); }
static uint32_t da_hash(const uint8_t *n, uint32_t len)
{
    uint32_t h = 0;
    for (; len >= 4; len -= 4, n += 4)
        h = (uint32_t)n[0] << 21 ^ (uint32_t)n[1] << 14 ^ (uint32_t)n[2] << 7 ^ n[3] ^ rol32(h, 7 * 4);
    switch (len) {
    case 3: return (uint32_t)n[0] << 14 ^ (uint32_t)n[1] << 7 ^ n[2] ^ rol32(h, 7 * 3);
    case 2: return (uint32_t)n[0] << 7 ^ n[1] ^ rol32(h, 7 * 2);
    case 1: return n[0] ^ rol32(h, 7);
    default: return h;
    }
}

typedef struct {
    uint64_t ino;
    uint8_t  nlen;
    uint8_t  nome[256];
} dentrada_t;

/* Lee el bloque de datos número "db" de una carpeta (forma block o bloque de datos) y lo
 * comprueba. *ini y *fim: dónde están las entradas. 1 = hueco (con *prox, en bloques). */
static int dir_bloco(xvol_t *v, inode_t *dir, uint64_t db, xbuf_t **saida, uint32_t *ini, uint32_t *fim, uint64_t *prox)
{
    xbuf_t *b;
    int r = ler_logico(v, dir, db << v->dirblklog, 1u << v->dirblklog, &b, prox);
    if (r) return r;
    const uint8_t *p = b->dados;
    uint32_t mg = be32(p);
    bool bloco = mg == (v->v5 ? DIR_BLOCO_V5 : DIR_BLOCO_V4);
    if (b->verificado != DIR_DADOS_V5) {
        bool ok = bloco || mg == (v->v5 ? DIR_DADOS_V5 : DIR_DADOS_V4);
        if (ok && v->v5) {
            if (!soma_ok(v, p, v->dirbs, 4, "bloque de carpeta", dir->ino)) { buf_soltar(b); return ERRO_ACESSO; }
            ok = be64(p + 8) == b->daddr && be64(p + 40) == dir->ino && memcmp(p + 24, v->uuid, 16) == 0;
        }
        if (!ok) { buf_soltar(b); return corrompido(v, "bloque de carpeta", dir->ino); }
        b->verificado = DIR_DADOS_V5;
    }
    *ini = v->dados_cab;
    *fim = v->dirbs;
    if (bloco) {   /* al final: las hojas (count * 8 bytes) y la cola (count, stale) */
        uint32_t count = be32(p + v->dirbs - 8);
        if (count > (v->dirbs - v->dados_cab - 8) / 8) { buf_soltar(b); return corrompido(v, "cola de la carpeta", dir->ino); }
        *fim = v->dirbs - 8 - count * 8;
    }
    *saida = b;
    return 0;
}

/* La entrada que empieza en "off" de un bloque de datos. Devuelve su tamaño (múltiplo de 8),
 * 0 si es espacio libre (con *livre = su tamaño) o un error. */
static int dir_entrada(xvol_t *v, const uint8_t *b, uint32_t off, uint32_t fim, dentrada_t *e, uint32_t *livre)
{
    const uint8_t *p = b + off;
    *livre = 0;
    if (off + 8 > fim) return ERRO_ACESSO;
    if (be16(p) == 0xFFFF) {
        uint32_t n = be16(p + 2);
        if (n < 8 || n % 8 || off + n > fim) return ERRO_ACESSO;
        *livre = n;
        return 0;
    }
    uint32_t nl = p[8];
    uint32_t t = (8 + 1 + nl + (v->ftype ? 1 : 0) + 2 + 7) & ~7u;
    if (!nl || off + t > fim || be16(p + t - 2) != off) return ERRO_ACESSO;
    e->ino = be64(p);
    e->nlen = (uint8_t)nl;
    memcpy(e->nome, p + 9, nl);
    return (int)t;
}

/* Forma shortform (en el inodo): la entrada número "idx". 1 = hay, 0 = no hay más. */
static int sf_entrada(xvol_t *v, inode_t *dir, uint32_t idx, dentrada_t *e, uint64_t *pai)
{
    const uint8_t *f = dir->fork;
    uint32_t tam = (uint32_t)dir->tam;
    if (tam < 6) return corrompido(v, "carpeta shortform", dir->ino);
    uint32_t count = f[0], isz = f[1] ? 8 : 4, pos = 2 + isz;
    if (pos > tam) return corrompido(v, "carpeta shortform", dir->ino);
    if (pai) *pai = isz == 8 ? be64(f + 2) : be32(f + 2);
    for (uint32_t i = 0; i < count; i++) {
        if (pos + 3 > tam) return corrompido(v, "carpeta shortform", dir->ino);
        uint32_t nl = f[pos];
        uint32_t t = 3 + nl + (v->ftype ? 1 : 0) + isz;
        if (!nl || pos + t > tam) return corrompido(v, "carpeta shortform", dir->ino);
        if (i == idx) {
            const uint8_t *q = f + pos + 3 + nl + (v->ftype ? 1 : 0);
            e->ino = isz == 8 ? be64(q) : be32(q);
            e->nlen = (uint8_t)nl;
            memcpy(e->nome, f + pos + 3, nl);
            return 1;
        }
        pos += t;
    }
    return 0;
}

/* La siguiente entrada de la carpeta desde la posición *pos (índice en shortform; byte del
 * espacio de datos en las demás formas). Incluye "." y "..". 1 = hay, 0 = terminó, < 0 error. */
static int dir_proxima(xvol_t *v, inode_t *dir, uint64_t *pos, dentrada_t *e)
{
    if (dir->formato == FMT_LOCAL) {
        int r = sf_entrada(v, dir, (uint32_t)*pos, e, NULL);
        if (r == 1) (*pos)++;
        return r;
    }
    uint32_t dsh = v->blocklog + v->dirblklog;
    while (*pos < DIR_FOLHA_DESL) {
        uint64_t db = *pos >> dsh;
        uint32_t off = (uint32_t)(*pos & (v->dirbs - 1));
        xbuf_t *b;
        uint32_t ini, fim;
        uint64_t prox = NULO64;
        int r = dir_bloco(v, dir, db, &b, &ini, &fim, &prox);
        if (r < 0) return r;
        if (r == 1) {   /* hueco en el espacio de datos: al siguiente bloque con datos */
            if (prox == NULO64 || (prox << v->blocklog) >= DIR_FOLHA_DESL) return 0;
            uint64_t novo = ((prox << v->blocklog) + v->dirbs - 1) & ~(uint64_t)(v->dirbs - 1);
            *pos = novo > *pos ? novo : (db + 1) << dsh;
            continue;
        }
        if (off < ini) off = ini;
        while (off < fim) {
            uint32_t livre;
            r = dir_entrada(v, b->dados, off, fim, e, &livre);
            if (r < 0) { buf_soltar(b); return corrompido(v, "entrada de carpeta", dir->ino); }
            if (r == 0) { off += livre; continue; }
            *pos = (db << dsh) + off + (uint32_t)r;
            buf_soltar(b);
            return 1;
        }
        buf_soltar(b);
        *pos = (db + 1) << dsh;
    }
    return 0;
}

/* Comprueba si la entrada del espacio de datos "endereco" (en unidades de 8 bytes) se llama
 * "nome". 1 = sí (con *ino), 0 = no, < 0 error. */
static int dir_comparar(xvol_t *v, inode_t *dir, uint32_t endereco, const uint8_t *nome, uint32_t nl, uint64_t *ino)
{
    uint64_t byte = (uint64_t)endereco << 3;
    uint32_t dsh = v->blocklog + v->dirblklog;
    xbuf_t *b;
    uint32_t ini, fim;
    int r = dir_bloco(v, dir, byte >> dsh, &b, &ini, &fim, NULL);
    if (r) return r < 0 ? r : corrompido(v, "hoja de carpeta (bloque que no existe)", dir->ino);
    uint32_t off = (uint32_t)(byte & (v->dirbs - 1)), livre;
    dentrada_t e;
    r = off >= ini ? dir_entrada(v, b->dados, off, fim, &e, &livre) : ERRO_ACESSO;
    buf_soltar(b);
    if (r <= 0) return corrompido(v, "hoja de carpeta (dirección)", dir->ino);
    if (e.nlen != nl || memcmp(e.nome, nome, nl) != 0) return 0;
    *ino = e.ino;
    return 1;
}

/* Busca por hash en una lista ordenada de hojas (hashval, address). 1 = encontrado, 0 = no
 * está aquí; *seguir = las coincidencias llegan hasta el final y pueden seguir en la siguiente hoja. */
static int folhas_buscar(xvol_t *v, inode_t *dir, const uint8_t *ents, uint32_t n, uint32_t h,
                         const uint8_t *nome, uint32_t nl, uint64_t *ino, bool *seguir)
{
    uint32_t lo = 0, hi = n;
    while (lo < hi) { uint32_t m = lo + (hi - lo) / 2; if (be32(ents + m * 8) < h) lo = m + 1; else hi = m; }
    *seguir = lo == n;
    for (uint32_t i = lo; i < n; i++) {
        if (be32(ents + i * 8) != h) return 0;
        uint32_t a = be32(ents + i * 8 + 4);
        if (!a) continue;   /* entrada vieja (stale) */
        int r = dir_comparar(v, dir, a, nome, nl, ino);
        if (r) return r;
        if (i + 1 == n) *seguir = true;
    }
    return 0;
}

/* ---- da-btree: hojas y nodos de las carpetas leaf y node */

static uint32_t da_magia(xvol_t *v, const uint8_t *b) { (void)v; return be16(b + 8); }
static bool da_eh_no(xvol_t *v, const uint8_t *b) { return da_magia(v, b) == (v->v5 ? NO_DA_V5 : NO_DA_V4); }
static uint32_t da_nivel(xvol_t *v, const uint8_t *b, bool ri)
{
    (void)ri;
    return da_eh_no(v, b) ? be16(b + v->da_cab - (v->v5 ? 6 : 2)) : 0;
}
static uint32_t da_nrecs(xvol_t *v, const uint8_t *b, bool ri) { (void)ri; return be16(b + (v->v5 ? 56 : 12)); }
static uint32_t da_maxrecs(xvol_t *v, uint32_t tam, bool ri) { (void)ri; return (tam - v->da_cab) / 8; }
static uint64_t da_chave(xvol_t *v, const uint8_t *b, uint32_t tam, bool ri, uint32_t i)
{
    (void)tam; (void)ri;
    return be32(b + v->da_cab + i * 8);
}
static uint64_t da_ponteiro(xvol_t *v, const uint8_t *b, uint32_t tam, bool ri, uint32_t i)
{
    (void)tam; (void)ri;
    return be32(b + v->da_cab + i * 8 + 4);
}

/* Un bloque del da-btree (hoja o nodo) en el bloque "dablk" (en bloques del archivo). */
static int da_ler(xvol_t *v, inode_t *dir, uint64_t dablk, xbuf_t **saida)
{
    xbuf_t *b;
    int r = ler_logico(v, dir, dablk, 1u << v->dirblklog, &b, NULL);
    if (r) return r < 0 ? r : corrompido(v, "bloque del da-btree (hueco)", dir->ino);
    const uint8_t *p = b->dados;
    if (b->verificado != NO_DA_V5) {
        uint32_t mg = be16(p + 8);
        bool ok = v->v5 ? (mg == FOLHA1_V5 || mg == FOLHAN_V5 || mg == NO_DA_V5)
                        : (mg == FOLHA1_V4 || mg == FOLHAN_V4 || mg == NO_DA_V4);
        if (ok && v->v5) {
            if (!soma_ok(v, p, v->dirbs, 12, "bloque del da-btree", dir->ino)) { buf_soltar(b); return ERRO_ACESSO; }
            ok = be64(p + 16) == b->daddr && be64(p + 48) == dir->ino && memcmp(p + 32, v->uuid, 16) == 0;
        }
        if (!ok) { buf_soltar(b); return corrompido(v, "bloque del da-btree", dir->ino); }
        b->verificado = NO_DA_V5;
    }
    if (da_nrecs(v, p, false) > da_maxrecs(v, v->dirbs, false)) { buf_soltar(b); return corrompido(v, "bloque del da-btree (count)", dir->ino); }
    *saida = b;
    return 0;
}

static int da_ler_no(xvol_t *v, void *ctx, uint64_t ptr, uint32_t nivel, xbuf_t **saida)
{
    xbuf_t *b;
    int r = da_ler(v, ctx, ptr, &b);
    if (r < 0) return r;
    uint32_t mg = da_magia(v, b->dados);
    bool ok = nivel ? (da_eh_no(v, b->dados) && da_nivel(v, b->dados, false) == nivel)
                    : mg == (v->v5 ? FOLHAN_V5 : FOLHAN_V4);
    if (!ok) { buf_soltar(b); return corrompido(v, "da-btree (nivel)", ptr); }
    *saida = b;
    return 0;
}

static const arvore_tipo_t arvore_da = {
    "da-btree", true, da_nivel, da_nrecs, da_maxrecs, da_chave, da_ponteiro, da_ler_no,
};

/* Busca "nome" (bytes del disco) en la carpeta. 0 y *ino, ERRO_NAO_EXISTE o un error. */
static int dir_buscar_bytes(xvol_t *v, inode_t *dir, const uint8_t *nome, uint32_t nl, uint64_t *ino)
{
    if (dir->formato == FMT_LOCAL) {
        uint64_t pai = 0;
        dentrada_t e;
        int r = sf_entrada(v, dir, 0xFFFFFFFFu, &e, &pai);
        if (r < 0) return r;
        if (nl == 1 && nome[0] == '.') { *ino = dir->ino; return 0; }
        if (nl == 2 && nome[0] == '.' && nome[1] == '.') { *ino = pai; return 0; }
        for (uint32_t i = 0; (r = sf_entrada(v, dir, i, &e, NULL)) == 1; i++)
            if (e.nlen == nl && memcmp(e.nome, nome, nl) == 0) { *ino = e.ino; return 0; }
        return r < 0 ? r : ERRO_NAO_EXISTE;
    }
    uint32_t h = da_hash(nome, nl);
    bool seguir;
    if (dir->tam == v->dirbs) {   /* forma block: las hojas al final del bloque 0 */
        xbuf_t *b;
        uint32_t ini, fim;
        int r = dir_bloco(v, dir, 0, &b, &ini, &fim, NULL);
        if (r) return r < 0 ? r : corrompido(v, "carpeta block (sin bloque)", dir->ino);
        uint32_t count = be32(b->dados + v->dirbs - 8);
        r = folhas_buscar(v, dir, b->dados + fim, count, h, nome, nl, ino, &seguir);
        buf_soltar(b);
        return r < 0 ? r : r ? 0 : ERRO_NAO_EXISTE;
    }
    /* formas leaf y node: la raíz del da-btree está en el primer bloque de hojas */
    xbuf_t *raiz, *f;
    int r = da_ler(v, dir, DIR_FOLHA_DESL >> v->blocklog, &raiz);
    if (r < 0) return r;
    if (da_eh_no(v, raiz->dados)) {
        r = arvore_descer(v, &arvore_da, dir, raiz, NULL, 0, h, &f);
        buf_soltar(raiz);
        if (r < 0) return r;
    } else f = raiz;
    for (int voltas = 0; voltas < 1 << 16; voltas++) {   /* una cadena de hojas en bucle no cuelga */
        uint32_t mg = da_magia(v, f->dados);
        if (mg != (v->v5 ? FOLHA1_V5 : FOLHA1_V4) && mg != (v->v5 ? FOLHAN_V5 : FOLHAN_V4)) {
            buf_soltar(f);
            return corrompido(v, "hoja de carpeta", dir->ino);
        }
        r = folhas_buscar(v, dir, f->dados + v->da_cab, da_nrecs(v, f->dados, false), h, nome, nl, ino, &seguir);
        uint32_t forw = be32(f->dados);
        buf_soltar(f);
        if (r) return r < 0 ? r : 0;
        if (!seguir || !forw) return ERRO_NAO_EXISTE;
        r = da_ler(v, dir, forw, &f);   /* el mismo hash sigue en la hoja siguiente */
        if (r < 0) return r;
    }
    return corrompido(v, "cadena de hojas", dir->ino);
}

/* ------------------------------------------------------------ nombres */

/* Latin-1 (SaguiOS) -> UTF-8 (el disco). -1 si no cabe. */
static int nome_para_disco(const char *s, size_t n, uint8_t *out, int max)
{
    int k = 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t c = (uint8_t)s[i];
        if (c < 0x80) { if (k + 1 > max) return -1; out[k++] = c; }
        else { if (k + 2 > max) return -1; out[k++] = (uint8_t)(0xC0 | (c >> 6)); out[k++] = (uint8_t)(0x80 | (c & 0x3F)); }
    }
    return k;
}

/* UTF-8 de 2 bytes que cabe en Latin-1 pasa a 1 byte; el resto queda como está. */
static void nome_do_disco(const uint8_t *s, uint32_t n, char *out, int max)
{
    int k = 0;
    for (uint32_t i = 0; i < n && k < max - 1; i++) {
        if ((s[i] == 0xC2 || s[i] == 0xC3) && i + 1 < n && (s[i + 1] & 0xC0) == 0x80) {
            out[k++] = (char)(((s[i] & 0x03) << 6) | (s[i + 1] & 0x3F));
            i++;
        } else out[k++] = s[i] ? (char)s[i] : '_';
    }
    out[k] = 0;
}

/* Busca un nombre en Latin-1: primero como UTF-8 y, si no es ASCII, con los bytes tal cual. */
static int dir_buscar(xvol_t *v, inode_t *dir, const char *nome, size_t n, uint64_t *ino)
{
    uint8_t u[256];
    int nu = nome_para_disco(nome, n, u, 255);
    int r = nu > 0 ? dir_buscar_bytes(v, dir, u, (uint32_t)nu, ino) : ERRO_NAO_EXISTE;
    if (r == ERRO_NAO_EXISTE && (size_t)nu != n && n <= 255) r = dir_buscar_bytes(v, dir, (const uint8_t *)nome, (uint32_t)n, ino);
    return r;
}

/* ------------------------------------------------------------ enlaces simbólicos */

/* El destino del enlace en "dst" (sin el cero). Devuelve su longitud o un error. */
static int ler_ligacao(xvol_t *v, inode_t *i, uint8_t *dst, uint32_t max)
{
    uint64_t tam = i->tam;
    if (!tam || tam > LIGACAO_MAX || tam > max) return corrompido(v, "enlace simbólico", i->ino);
    if (i->formato == FMT_LOCAL) { memcpy(dst, i->fork, (size_t)tam); return (int)tam; }
    /* en bloques: v5 lleva una cabecera al principio de cada tramo, con su suma */
    uint32_t feito = 0;
    uint64_t off = 0;
    while (feito < tam) {
        xfs_extent_t e;
        int r = bmap(v, i, off, &e);
        if (r < 0) return r;
        if (e.tipo != XFS_EXT_DADOS || e.n > 4) return corrompido(v, "enlace simbólico (bloques)", i->ino);
        uint64_t setor = fsb_setor(v, e.bloco);
        xbuf_t *b;
        r = buf_ler(v, setor, (uint32_t)e.n << v->blocklog, &b);
        if (r < 0) return r;
        const uint8_t *p = b->dados;
        uint32_t cab = v->v5 ? 56 : 0, cabe = b->tam - cab, parte = (uint32_t)tam - feito < cabe ? (uint32_t)tam - feito : cabe;
        if (v->v5) {
            if (!soma_ok(v, p, b->tam, 12, "enlace simbólico", i->ino)) { buf_soltar(b); return ERRO_ACESSO; }
            if (be32(p) != LIGACAO_MAGIA || be32(p + 4) != feito || be32(p + 8) != parte || be64(p + 32) != i->ino ||
                be64(p + 40) != setor || memcmp(p + 16, v->uuid, 16) != 0) {
                buf_soltar(b);
                return corrompido(v, "enlace simbólico (cabecera)", i->ino);
            }
        }
        memcpy(dst + feito, p + cab, parte);
        buf_soltar(b);
        feito += parte;
        off = e.ini + e.n;
    }
    return (int)tam;
}

/* ------------------------------------------------------------ rutas */

/* Resuelve una ruta del volumen ("" = la raíz). Los enlaces simbólicos se siguen si su destino
 * es relativo (dentro del volumen; ".." no pasa de la raíz); con "seguir", también el último. Un
 * último componente que es un enlace sin destino válido (absoluto, roto o en bucle) se devuelve
 * tal cual: al abrirlo se lee su destino como texto. */
static int achar_n(xvol_t *v, const char *cam, inode_t *saida, bool seguir, int prof)
{
    inode_t cur;
    int r = inode_ler(v, v->raiz, &cur);
    if (r < 0) return r;
    const char *p = cam;
    for (;;) {
        while (*p == '/') p++;
        if (!*p) { *saida = cur; return 0; }
        const char *q = p;
        while (*q && *q != '/') q++;
        if (!EH_PASTA(&cur)) { inode_soltar(&cur); return ERRO_NAO_E_PASTA; }
        uint64_t ino;
        r = dir_buscar(v, &cur, p, (size_t)(q - p), &ino);
        inode_soltar(&cur);
        if (r < 0) return r;
        inode_t nx;
        r = inode_ler(v, ino, &nx);
        if (r < 0) return r;
        const char *resto = q;
        while (*resto == '/') resto++;
        bool ultimo = !*resto;
        if (EH_LIGACAO(&nx) && (!ultimo || seguir)) {
            char *novo = prof < SALTOS_MAX ? kmalloc(CAM_MAX) : NULL;
            size_t pre = (size_t)(p - cam);
            int n = -1;
            if (novo && pre < CAMINHO_MAX * 2) {
                memcpy(novo, cam, pre);
                n = ler_ligacao(v, &nx, (uint8_t *)novo + pre, LIGACAO_MAX);
            }
            if (n <= 0 || novo[pre] == '/' || txt_tamanho(resto) >= CAMINHO_MAX * 2) {
                if (novo) kfree(novo);
                if (n < 0 && n != ERRO_ACESSO) n = ERRO_NAO_EXISTE;
                if (ultimo && n != ERRO_ACESSO) { *saida = nx; return 0; }
                inode_soltar(&nx);
                return n == ERRO_ACESSO ? n : ERRO_NAO_EXISTE;
            }
            size_t k = pre + (size_t)n;
            for (size_t j = pre; j < k; j++) if (!novo[j]) novo[j] = '_';   /* un cero dentro del destino */
            if (!ultimo) { novo[k++] = '/'; size_t t = txt_tamanho(resto); memcpy(novo + k, resto, t); k += t; }
            novo[k] = 0;
            r = achar_n(v, novo, saida, seguir, prof + 1);
            kfree(novo);
            if (r == ERRO_NAO_EXISTE && ultimo) { *saida = nx; return 0; }
            inode_soltar(&nx);
            return r;
        }
        cur = nx;
        p = q;
    }
}

static int achar(xvol_t *v, const char *rel, inode_t *saida)
{
    return achar_n(v, rel, saida, true, 0);
}

static unsigned tipo_de(const inode_t *i)
{
    if (EH_PASTA(i)) return TIPO_PASTA;
    return (EH_ARQUIVO(i) && (i->modo & 0111)) ? TIPO_PROGRAMA : TIPO_ARQUIVO;
}

static uint32_t tam_vfs(uint64_t t) { return t > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)t; }

/* ------------------------------------------------------------ diario */

/* El ciclo de un sector del diario: el de la cabecera, o la primera palabra de un bloque de datos. */
static int diario_ciclo(xvol_t *v, uint64_t base, uint32_t k, uint8_t *s, uint32_t *ciclo)
{
    if (ler_setores(v, base + k, 1, s) < 0) return ERRO_ACESSO;
    *ciclo = be32(s) == DIARIO_MAGIA ? be32(s + 4) : be32(s);
    return 0;
}

/* 1 = limpio (el último registro es el de desmontar y la cabeza está justo detrás), 0 = no está
 * limpio, 2 = diario externo (no se puede mirar), < 0 = error de lectura. Solo se leen cabeceras:
 * el diario no se reproduce en esta fase. */
static int diario_limpo(xvol_t *v)
{
    if (!v->logstart) return 2;
    if (!fsb_valido(v, v->logstart)) return 0;
    uint64_t base = fsb_setor(v, v->logstart);
    uint64_t L64 = (uint64_t)v->logblocks << (v->blocklog - 9);
    if (!L64 || L64 > 0xFFFFFFFFull || base + L64 > v->d->setores) return 0;
    uint32_t L = (uint32_t)L64;
    uint8_t *s = kmalloc(512);
    if (!s) return ERRO_SEM_MEMORIA;
    int res = 0;
    uint32_t c0, cl, c;
    if (diario_ciclo(v, base, 0, s, &c0) < 0 || diario_ciclo(v, base, L - 1, s, &cl) < 0) { res = ERRO_ACESSO; goto fim; }
    if (c0 == 0) { res = 1; goto fim; }   /* diario a cero: no hay nada pendiente */
    uint32_t cabeca = 0;
    if (c0 != cl) {   /* la cabeza es el primer sector que ya no es del ciclo actual */
        uint32_t lo = 0, hi = L - 1;
        while (hi - lo > 1) {
            uint32_t m = lo + (hi - lo) / 2;
            if (diario_ciclo(v, base, m, s, &c) < 0) { res = ERRO_ACESSO; goto fim; }
            if (c == c0) lo = m; else hi = m;
        }
        cabeca = hi;
    }
    /* el último registro escrito: hacia atrás desde la cabeza */
    uint32_t reg = 0xFFFFFFFFu;
    for (uint32_t k = 1; k <= L && k <= DIARIO_BUSCA; k++) {
        uint32_t b = (cabeca + L - k) % L;
        if (ler_setores(v, base + b, 1, s) < 0) { res = ERRO_ACESSO; goto fim; }
        if (be32(s) == DIARIO_MAGIA && be32(s + 4) == c0) { reg = b; break; }
    }
    if (reg == 0xFFFFFFFFu) goto fim;
    uint32_t versao = be32(s + 8), h_len = be32(s + 12), nops = be32(s + 40), h_size = be32(s + 320);
    uint32_t hblks = (versao & 2) && h_size > 32768 ? (h_size + 32767) / 32768 : 1;
    uint32_t fim_reg = (uint32_t)(((uint64_t)reg + hblks + (h_len + 511) / 512) % L);
    if (fim_reg != cabeca || nops != 1) goto fim;
    if (ler_setores(v, base + (reg + hblks) % L, 1, s) < 0) { res = ERRO_ACESSO; goto fim; }
    res = (s[9] & 0x20) ? 1 : 0;   /* XLOG_UNMOUNT_TRANS en la cabecera de la operación */
fim:
    kfree(s);
    return res;
}

/* ------------------------------------------------------------ montar */

static void liberar(xvol_t *v)
{
    buf_liberar_todos(v);
    if (v->ra) kfree(v->ra);
    kfree(v);
}

static int xf_montar(disco_t *d, uint32_t *opcoes, void **priv)
{
    xvol_t *v = kcalloc(sizeof(xvol_t));
    uint8_t *s = kmalloc(512);
    if (!v || !s) { if (v) kfree(v); if (s) kfree(s); return ERRO_SEM_MEMORIA; }
    v->d = d;
    char motivo[48];
    int r = disco_ler(d, 0, 1, s) < 0 ? ERRO_ACESSO : 0;
    int ok = r < 0 ? XFS_NAO : validar_sb(s, motivo, sizeof motivo);
    kfree(s);
    if (ok != XFS_SIM) {
        if (ok == XFS_RECUSADO) {
            klog("[XFS] %s: %s; no se monta\n", d->nome, motivo);
            disco_avisar(AVISO_INFO, "%s: %s: no se monta, para no leerlo mal.", d->nome, motivo);
        }
        liberar(v);
        return r < 0 ? r : ERRO_FORMATO;
    }
    /* el superbloque entero (un sector del sistema de archivos), con su suma en v5 */
    xbuf_t *sb;
    uint32_t sect = 0;
    {
        uint8_t t[512];
        if (disco_ler(d, 0, 1, t) < 0) { liberar(v); return ERRO_ACESSO; }
        sect = be16(t + 102);
        v->v5 = (be16(t + 100) & 0xF) == 5;
    }
    r = buf_ler(v, 0, sect, &sb);
    if (r < 0) { liberar(v); return r; }
    const uint8_t *p = sb->dados;
    if (v->v5 && !soma_ok(v, p, sect, 224, "superbloque", 0)) {
        buf_soltar(sb);
        disco_avisar(AVISO_INFO, "%s: el superbloque XFS tiene la suma mal: no se monta.", d->nome);
        liberar(v);
        return ERRO_ACESSO;
    }
    v->sectsize = sect;
    v->bs = be32(p + 4);
    v->blocklog = p[120];
    v->dblocks = be64(p + 8);
    v->rblocks = be64(p + 16);
    v->logstart = be64(p + 48);
    v->raiz = be64(p + 56);
    v->agblocks = be32(p + 84);
    v->agcount = be32(p + 88);
    v->logblocks = be32(p + 96);
    v->versao = be16(p + 100);
    v->isz = be16(p + 104);
    v->inodelog = p[122];
    v->inopblog = p[123];
    v->agblklog = p[124];
    v->fdblocks = be64(p + 144);
    v->dirblklog = p[192];
    v->dirbs = v->bs << v->dirblklog;
    v->features2 = (v->versao & VER_MOREBITS) ? (be32(p + 200) | be32(p + 204)) : 0;
    if (v->v5) {
        v->ro_compat = be32(p + 212);
        v->incompat = be32(p + 216);
        v->ftype = (v->incompat & INC_FTYPE) != 0;
        memcpy(v->uuid, (v->incompat & INC_META_UUID) ? p + 248 : p + 32, 16);
    } else {
        v->ftype = (v->features2 & F2_FTYPE) != 0;
        memcpy(v->uuid, p + 32, 16);
    }
    xfs_reconhecer(p, NULL, 0, v->rotulo, sizeof v->rotulo);
    buf_soltar(sb);
    v->bmbt_cab = v->v5 ? 72 : 24;
    v->da_cab = v->v5 ? 64 : 16;
    v->dados_cab = v->v5 ? 64 : 16;
    /* los grupos tienen que cubrir justo el volumen (así ningún bloque válido cae fuera de él), y el
     * volumen no puede ser mayor que la partición (una imagen cortada leería basura) */
    uint64_t setores = v->dblocks << (v->blocklog - 9);
    if ((uint64_t)(v->agcount - 1) * v->agblocks >= v->dblocks || (uint64_t)v->agcount * v->agblocks < v->dblocks) {
        klog("[XFS] %s: %u grupos de %u bloques no cuadran con %x%08x bloques\n", d->nome, v->agcount, v->agblocks, X64(v->dblocks));
        disco_avisar(AVISO_INFO, "%s: el XFS tiene la geometría estropeada: no se monta.", d->nome);
        liberar(v);
        return ERRO_FORMATO;
    }
    if (setores > d->setores) {
        klog("[XFS] %s: el sistema de archivos (%x%08x sectores) no cabe en la particion (%u)\n", d->nome, X64(setores), d->setores);
        disco_avisar(AVISO_INFO, "%s: el XFS es mayor que la partición (¿imagen cortada?): no se monta.", d->nome);
        liberar(v);
        return ERRO_FORMATO;
    }
    inode_t raiz;
    r = inode_ler(v, v->raiz, &raiz);
    if (r == 0 && !EH_PASTA(&raiz)) { inode_soltar(&raiz); r = corrompido(v, "inodo raíz", v->raiz); }
    if (r < 0) { liberar(v); return r; }
    inode_soltar(&raiz);
    int dl = diario_limpo(v);
    if (dl == 0) {
        klog("[XFS] %s: el diario no esta vacio (apagado incorrecto); se monta igual en solo lectura\n", d->nome);
        disco_avisar(AVISO_INFO, "XFS: el diario no está vacío (apagado incorrecto): lo último escrito puede no verse; "
                     "repáralo en Linux con xfs_repair o móntalo allí una vez.");
    } else if (dl == 2) {
        klog("[XFS] %s: diario externo: no se puede comprobar si quedo limpio\n", d->nome);
    } else if (dl < 0) {
        klog("[XFS] %s: no se pudo leer el diario (%d); se monta igual\n", d->nome, dl);
    }
    if (*opcoes & MONTAR_LEITURA_ESCRITA) {
        klog("[XFS] %s: escritura pedida, pero XFS es de solo lectura en SaguiOS\n", d->nome);
        disco_avisar(AVISO_INFO, "%s: SaguiOS solo sabe leer XFS; montado en solo lectura.", d->nome);
    }
    *opcoes |= MONTAR_SO_LEITURA;
    *opcoes &= ~(uint32_t)MONTAR_LEITURA_ESCRITA;
    klog("[XFS] montado %s: v%u, bloque %u, inodo %u, %u grupos de %u bloques, carpeta %u, ftype %u, "
         "incompat %x, ro_compat %x, diario %s, etiqueta '%s', solo lectura\n",
         d->nome, v->versao & 0xF, v->bs, v->isz, v->agcount, v->agblocks, v->dirbs, v->ftype ? 1 : 0,
         v->incompat, v->ro_compat, dl == 1 ? "limpio" : dl == 2 ? "externo" : dl == 0 ? "sucio" : "?", v->rotulo);
    if (v->rblocks) klog("[XFS] %s: tiene volumen de tiempo real: sus archivos no se leen\n", d->nome);
    *priv = v;
    return 0;
}

static int xf_desmontar(void *priv, bool gravar)
{
    (void)gravar;   /* nada que grabar: solo lectura */
    xvol_t *v = priv;
    klog("[XFS] desmontado %s (%u sumas mal)\n", v->d->nome, v->erros_soma);
    liberar(v);
    return 0;
}

/* ------------------------------------------------------------ operaciones */

static int xf_info(void *priv, const char *rel, info_arquivo_t *inf)
{
    xvol_t *v = priv;
    inode_t i;
    int r = achar(v, rel, &i);
    if (r < 0) return r;
    inf->tipo = tipo_de(&i);
    inf->tamanho = EH_PASTA(&i) ? 0 : tam_vfs(i.tam);
    inode_soltar(&i);
    return 0;
}

static int xf_datas(void *priv, const char *rel, datas_arquivo_t *d)
{
    xvol_t *v = priv;
    inode_t i;
    int r = achar(v, rel, &i);
    if (r < 0) return r;
    d->modificado = data_inode(&i, 40);
    d->acessado = data_inode(&i, 32);
    d->criado = i.versao == 3 ? data_inode(&i, 144) : data_inode(&i, 48);
    inode_soltar(&i);
    return 0;
}

static int xf_meta(void *priv, const char *rel, meta_t *m)
{
    xvol_t *v = priv;
    inode_t i;
    int r = achar(v, rel, &i);
    if (r < 0) return r;
    m->uid = be32(i.p + 8);
    m->gid = be32(i.p + 12);
    m->modo = (uint16_t)(i.modo & 01777);
    m->tipo = EH_PASTA(&i) ? TIPO_PASTA : TIPO_ARQUIVO;
    inode_soltar(&i);
    return 0;
}

static int xf_listar(void *priv, const char *rel, int indice, entrada_pasta_t *saida)
{
    xvol_t *v = priv;
    inode_t dir;
    int r = achar(v, rel, &dir);
    if (r < 0) return r;
    if (!EH_PASTA(&dir)) { inode_soltar(&dir); return ERRO_NAO_E_PASTA; }
    uint64_t pos = 0;
    int k = 0;
    if (indice > 0 && v->lst_ino == dir.ino && v->lst_indice + 1 == indice) { pos = v->lst_pos; k = indice; }
    dentrada_t e;
    while ((r = dir_proxima(v, &dir, &pos, &e)) == 1) {
        if ((e.nlen == 1 && e.nome[0] == '.') || (e.nlen == 2 && e.nome[0] == '.' && e.nome[1] == '.')) continue;
        if (k++ != indice) continue;
        nome_do_disco(e.nome, e.nlen, saida->nome, NOME_MAX);
        inode_t f;
        int rf = inode_ler(v, e.ino, &f);
        if (rf == 0 && EH_LIGACAO(&f)) {
            /* un enlace se ve como lo que hay al otro lado (si lo hay) */
            char *c = kmalloc(CAMINHO_MAX + 2 + 256);
            if (c) {
                size_t n = txt_tamanho(rel);
                if (n < CAMINHO_MAX) {
                    memcpy(c, rel, n);
                    if (n) c[n++] = '/';
                    memcpy(c + n, e.nome, e.nlen);
                    c[n + e.nlen] = 0;
                    inode_t alvo;
                    if (achar(v, c, &alvo) == 0) { inode_soltar(&f); f = alvo; }
                }
                kfree(c);
            }
        }
        if (rf == 0) {
            saida->tipo = tipo_de(&f);
            saida->tamanho = EH_PASTA(&f) ? 0 : tam_vfs(f.tam);
            inode_soltar(&f);
        } else {
            saida->tipo = TIPO_ARQUIVO;
            saida->tamanho = 0;
        }
        v->lst_ino = dir.ino; v->lst_indice = indice; v->lst_pos = pos;
        inode_soltar(&dir);
        return 1;
    }
    v->lst_ino = 0;
    inode_soltar(&dir);
    return r < 0 ? r : 0;
}

static int xf_abrir(void *priv, const char *rel, int modo, void *arq, uint32_t *tamanho)
{
    xvol_t *v = priv;
    xfs_arq_t *f = arq;
    memset(f, 0, sizeof *f);
    *tamanho = 0;
    if (modo & (ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR | ABRIR_ACRESCENTAR)) return ERRO_SO_LEITURA;
    inode_t i;
    int r = achar(v, rel, &i);
    if (r < 0) return r;
    if (EH_PASTA(&i)) r = ERRO_E_PASTA;
    else if (!EH_ARQUIVO(&i) && !EH_LIGACAO(&i)) r = ERRO_NAO_SUPORTADO;   /* dispositivo, tubería, socket */
    else if (i.flags & DIFLAG_REALTIME) r = ERRO_NAO_SUPORTADO;            /* datos en el volumen de tiempo real */
    if (r == 0) {
        f->ino = i.ino;
        f->tam = i.tam;
        f->ligacao = EH_LIGACAO(&i);
        *tamanho = tam_vfs(i.tam);
    }
    inode_soltar(&i);
    return r;
}

/* Copia bytes de los datos de un tramo: los grandes directos al destino; los pequeños por la
 * ventana de lectura anticipada (sin pasar del final del tramo). */
static int ler_tramo(xvol_t *v, uint64_t byte, uint64_t resto_tramo, uint8_t *dst, uint32_t n)
{
    uint64_t setor = byte >> 9;
    uint32_t desl = (uint32_t)(byte & 511);
    if (!desl && n >= DIRETO_MIN) {
        uint32_t k = n / 512 > DIRETO_MAX ? DIRETO_MAX : n / 512;
        if (ler_setores(v, setor, k, dst) < 0) return ERRO_ACESSO;
        if (v->ra_n && setor < v->ra_setor + v->ra_n && setor + k > v->ra_setor) v->ra_n = 0;
        return (int)(k * 512);
    }
    if (!v->ra_n || setor < v->ra_setor || setor >= v->ra_setor + v->ra_n) {
        if (!v->ra && !(v->ra = kmalloc(JANELA * 512))) return ERRO_SEM_MEMORIA;
        uint64_t k = (desl + resto_tramo + 511) / 512;
        if (k > JANELA) k = JANELA;
        if (setor + k > v->d->setores) k = v->d->setores > setor ? v->d->setores - setor : 0;
        v->ra_n = 0;
        if (!k || ler_setores(v, setor, (uint32_t)k, v->ra) < 0) return ERRO_ACESSO;
        v->ra_setor = setor;
        v->ra_n = (uint32_t)k;
    }
    uint32_t dentro = (uint32_t)(setor - v->ra_setor) * 512 + desl, disp = v->ra_n * 512 - dentro;
    uint32_t q = n < disp ? n : disp;
    memcpy(dst, v->ra + dentro, q);
    return (int)q;
}

static int xf_ler(void *priv, void *arq, uint32_t pos, void *buf, uint32_t n)
{
    xvol_t *v = priv;
    xfs_arq_t *f = arq;
    uint64_t tam = f->tam > 0xFFFFFFFFull ? 0xFFFFFFFFull : f->tam;
    if (pos >= tam) return 0;
    if (n > tam - pos) n = (uint32_t)(tam - pos);
    uint8_t *dst = buf;
    inode_t i;
    int r = inode_ler(v, f->ino, &i);
    if (r < 0) return r;
    if (f->ligacao) {   /* el destino de un enlace que no lleva a nada, como texto */
        uint8_t *t = kmalloc(LIGACAO_MAX);
        int k = t ? ler_ligacao(v, &i, t, LIGACAO_MAX) : ERRO_SEM_MEMORIA;
        inode_soltar(&i);
        if (k < 0) { if (t) kfree(t); return k; }
        if (pos >= (uint32_t)k) { kfree(t); return 0; }
        if (n > (uint32_t)k - pos) n = (uint32_t)k - pos;
        memcpy(dst, t + pos, n);
        kfree(t);
        return (int)n;
    }
    uint32_t feito = 0;
    while (feito < n) {
        uint64_t p = (uint64_t)pos + feito, fb = p >> v->blocklog;
        if (!f->ext_valido || fb < f->ext.ini || fb - f->ext.ini >= f->ext.n) {
            r = bmap(v, &i, fb, &f->ext);
            if (r < 0) { f->ext_valido = false; break; }
            f->ext_valido = true;
        }
        /* bytes desde p hasta el final del tramo (un hueco final puede llegar al infinito) */
        uint64_t blocos = f->ext.ini + f->ext.n - fb;
        uint64_t resto = blocos > (NULO64 >> (v->blocklog + 1)) ? NULO64 : (blocos << v->blocklog) - (p & (v->bs - 1));
        uint32_t q = resto < n - feito ? (uint32_t)resto : n - feito;
        if (f->ext.tipo != XFS_EXT_DADOS) {   /* hueco o sin escribir: ceros */
            memset(dst + feito, 0, q);
            feito += q;
            continue;
        }
        uint64_t byte = (fsb_setor(v, f->ext.bloco + (fb - f->ext.ini)) << 9) + (p & (v->bs - 1));
        int k = ler_tramo(v, byte, resto, dst + feito, q);
        if (k < 0) {
            klog("[XFS] %s: error de lectura en el inodo %x%08x\n", v->d->nome, X64(f->ino));
            r = k;
            break;
        }
        feito += (uint32_t)k;
    }
    inode_soltar(&i);
    if (feito < n && r < 0) return feito ? (int)feito : r;
    return (int)feito;
}

static int xf_escrever(void *priv, void *arq, uint32_t pos, const void *buf, uint32_t n, uint32_t *tamanho)
{ (void)priv; (void)arq; (void)pos; (void)buf; (void)n; (void)tamanho; return ERRO_SO_LEITURA; }
static int xf_truncar(void *priv, void *arq, uint32_t tamanho) { (void)priv; (void)arq; (void)tamanho; return ERRO_SO_LEITURA; }
static void xf_fechar(void *priv, void *arq) { (void)priv; (void)arq; }
static int xf_so_leitura(void *priv, const char *rel) { (void)priv; (void)rel; return ERRO_SO_LEITURA; }
static int xf_renomear(void *priv, const char *de, const char *para) { (void)priv; (void)de; (void)para; return ERRO_SO_LEITURA; }
static int xf_fixar_meta(void *priv, const char *rel, int32_t uid, int32_t gid, int32_t modo)
{ (void)priv; (void)rel; (void)uid; (void)gid; (void)modo; return ERRO_SO_LEITURA; }
static int xf_fixar_datas(void *priv, const char *rel, uint32_t mod, uint32_t acesso)
{ (void)priv; (void)rel; (void)mod; (void)acesso; return ERRO_SO_LEITURA; }

static void xf_espaco(void *priv, uint32_t *total_kb, uint32_t *livre_kb)
{
    xvol_t *v = priv;
    uint64_t blocos = v->dblocks - (v->logstart && v->logblocks < v->dblocks ? v->logblocks : 0);
    uint64_t t = (blocos << v->blocklog) >> 10, l = (v->fdblocks << v->blocklog) >> 10;
    if (l > t) l = t;
    *total_kb = t > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)t;
    *livre_kb = l > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)l;
}

const sistema_arquivos_t fs_xfs = {
    .nome = "xfs", .tam_arq = sizeof(xfs_arq_t),
    .montar = xf_montar, .desmontar = xf_desmontar, .info = xf_info, .datas = xf_datas, .listar = xf_listar,
    .abrir = xf_abrir, .ler = xf_ler, .escrever = xf_escrever, .truncar = xf_truncar, .fechar = xf_fechar,
    .criar_pasta = xf_so_leitura, .remover_pasta = xf_so_leitura, .apagar = xf_so_leitura,
    .renomear = xf_renomear, .espaco = xf_espaco,
    .meta = xf_meta, .fixar_meta = xf_fixar_meta, .fixar_datas = xf_fixar_datas,
};
